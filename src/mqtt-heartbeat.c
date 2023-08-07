/*******************************************/ /**
 * @file mqtt-heartbeat.c
 * @author marsman7 (you@domain.com)
 * @brief MQTT-Heartbeat is a Linux daemon that 
 *        periodically sends a status message via MQTT.
 * @version see 'version' file
 * @date 2022-02-02
 * 
 * @headerfile mqtt-heartbeat.h
 * 
 * @copyright Copyright (c) 2022
 ***********************************************/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/reboot.h>
#include <linux/limits.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <getopt.h>		// only for getopt_long() not for getopt()
#include <libconfig.h>
#include <mosquitto.h> // for MQTT funtionallity

#include <sys/sysinfo.h>

#include "mqtt-heartbeat.h"

//-----------------------------------------------
#define ERROR_EXIT(msg) do	{perror(msg); _exit(EXIT_FAILURE); } while(0)

#define LOG(level, msg, args...) if (level <= log_level) { fprintf(stderr, msg, level, ##args); }

#define MQTT_MAX_MESSAGE_LENGTH 1024

#ifndef VERSION_STR
	#define VERSION_STR "0.0.0"
#endif

//-----------------------------------------------
typedef void (*sighandler_t)(int);
char *config_file_name = NULL;
const char *shutdown_cmd = NULL;
bool pause_flag = false; 	/*!< set it TRUE the process go to paused, send SIGCONT to continue the process */
int keepalive = 30;
struct mosquitto *mosq = NULL; //! mosquitto client instance
bool connected = false;
int status = STAT_ON;
char *pub_parsed = NULL;

//-----------------------------------------------
void terminate_second_instance();
char *parse_string(char *, const char *);
char *alloc_string(char *, const char *);
int get_config_int(const config_t *, const char *, int *, int );
int get_config_string(const config_t *, const char *, char **, const char *, bool);
int read_config();
void init_mosquitto();
void connect_broker();
void on_connect_callback(struct mosquitto *, void *, int);
void on_subscribe_callback(struct mosquitto *, void *, int, int, const int *);
void on_message_callback(struct mosquitto *, void *, const struct mosquitto_message *);
void on_publish_callback(struct mosquitto *, void *, int);
void discard_free_config();
void init_signal_handler();
void signal_handler(int);

/*******************************************/ /**
 * @brief If this not the first instance, it is terminated
 ***********************************************/
void terminate_second_instance()
{
    int socket_fd = -1;
    struct sockaddr_un un_sockaddr = {0};
    size_t sockaddr_len;

    if ((socket_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
    {
        LOG(3, "<%d>Could not create socket.\n");
        _exit(EXIT_FAILURE);
    }

    un_sockaddr.sun_family = AF_UNIX;
    strncpy(un_sockaddr.sun_path, lock_socket_name, sizeof(un_sockaddr.sun_path)-1);
    // use the real sockaddr_length of socket name
    sockaddr_len = sizeof(un_sockaddr.sun_family) + strlen(lock_socket_name) + 1;

    if ( bind(socket_fd, (struct sockaddr *)&un_sockaddr, sockaddr_len) )
    {
        if (errno == EADDRINUSE) 
		{
			LOG(4, "<%d>An instance is already running, this will be terminated.\n");
		} 
		else 
		{
			char *errmsg = strerror( errno );
			LOG(3, "<%d>Error on binding socket : %d; %s; %s\n", errno, errmsg, lock_socket_name);
		}
		close(socket_fd);
        _exit(EXIT_FAILURE);
    } else {
        //LOG(6, "<%d>First instance\n");
    }
	close(socket_fd);
}

/*******************************************/ /**
 * @brief Terminate the process an clean the memory.
 *        This function is passed to atexit and is 
 *        only called after exit() and not after _exit().
 ***********************************************/
void clean_exit()
{
	// Publish a last message, it is not the MQTT "last will"
	status = STAT_OFF;
	if (mosq)
	{
		if (connected)
		{
			pub_parsed = parse_string(pub_parsed, stat_pub_message);
			mosquitto_publish(mosq, NULL, stat_pub_topic, strlen(pub_parsed), pub_parsed, qos, false);

			// Wait of empty send queue
			int i = 100;
			while (mosquitto_want_write(mosq) && (i > 0))
			{
					usleep(100000); // sleep 100ms
					i--;
			}
		}

		// Terminate the connection to MQTT broker
		//mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
	}

	// Remove link to the socket for run instance only once
	unlink(lock_socket_name);

	// Free allocated memory
	discard_free_config();

	LOG(4, "<%d>Cleanly teminated\n");

	if (shutdown_cmd)
	{
		char *command = malloc(128);
		sync();
		snprintf(command, 128, "shutdown %s %d", shutdown_cmd, shutdown_delay);
		if ( system(command) )
		{
			LOG(3, "<%d>System command faild!\n");
		}
		free(command);
	}

	_exit(EXIT_SUCCESS);
}

/*******************************************/ /**
 * @brief Pars a string and exchang the tags.
 * A Tag must be surrounded with %. For example foo/%bar%/for/%every%
 * The returnet pointer must be freeing ( free(char*) ).
 * 
 * @param dst_string - Pointer to destination string to reuse the memory 
 *                     or NULL to use new allocated memory.
 * @param src_string - Pointer to given incoming string
 * @return char* - Pointer to parsed string
 ***********************************************/
char *parse_string(char *dst_string, const char *src_string)
{
	if (! src_string)
	{
		return NULL;
	}

	char *ptemp_src = NULL;
	if (! dst_string) {
		dst_string = malloc(1); 	// Pointer to destination parsed string
	}
	*dst_string = '\0';
	unsigned int dst_length = 0;

	// Check if a tag is included in the topic and get the begin of the tag
	ptemp_src = strchr(src_string, '%');
	if (!ptemp_src)
	{
		// topic does not contain tags
		dst_length = strnlen(src_string, MQTT_MAX_MESSAGE_LENGTH - 1);
		dst_string = realloc(dst_string, dst_length + 1);
		strncat(dst_string, src_string, dst_length);
		return dst_string;
	}
	
	unsigned int sub_string_length = 0;
	bool var_found = false;		// indicates a valid tag 
	char tag_value[64] = "\0";
	char *ptag_value = NULL;

	do
	{
		sub_string_length = ptemp_src - src_string;
		// Check and allocate memory
		size_t requested_size = dst_length + sub_string_length + 1;
		if (requested_size > malloc_usable_size(dst_string)) {
			if (!(dst_string = realloc(dst_string, requested_size)))
			{
				ERROR_EXIT(err_out_of_memory);
			}
		}

		// copy string up to tag '%'
		strncat(dst_string, src_string, sub_string_length);
		dst_length = strlen(dst_string);
		src_string = ptemp_src + 1; // point to character after tag '%'

		// Search close tag to get the attribut name length
		ptemp_src = strchr(src_string, '%');
		if (!ptemp_src)
		{
			// topic does not contain tag
			return (char *)dst_string;
		}
		sub_string_length = ptemp_src - src_string;

		var_found = false;
		if (strncasecmp("hostname", src_string, sub_string_length) == 0)
		{
			gethostname(tag_value, sizeof(tag_value) - 1);
			sub_string_length = strnlen(tag_value, sizeof(tag_value));
			ptag_value = tag_value;
			var_found = true;
		} 
		else if (strncasecmp("user", src_string, sub_string_length) == 0) 
		{
			ptag_value = secure_getenv("USER");
			sub_string_length = strnlen(ptag_value, 63);
			var_found = true;
		}
		else if (strncasecmp("version", src_string, sub_string_length) == 0) 
		{
			strncpy(tag_value, VERSION_STR, strnlen(VERSION_STR, sizeof(tag_value)-1)+1);
			sub_string_length = strnlen(tag_value, sizeof(tag_value));
			ptag_value = tag_value;
			var_found = true;
		}
		else if (strncasecmp("status", src_string, sub_string_length) == 0) 
		{
			if (status)
			{
				ptag_value = (char *)status_on_string;
			}
			else
			{
				ptag_value = (char *)status_off_string;
			}
			sub_string_length = strnlen(ptag_value, 63);
			var_found = true;
		}
		else if (strncasecmp("loadavg_1", src_string, sub_string_length) == 0)
		{
			//double loadavgs[3];
			//getloadavg(loadavgs, 3);
			//sprintf(tag_value, "%.0f", loadavgs[0]*1000);		
			struct sysinfo info;
			sysinfo(&info);
			sprintf(tag_value, "%lu", info.loads[0]);		
			sub_string_length = strnlen(tag_value, sizeof(tag_value));
			ptag_value = tag_value;
			var_found = true;
		}
		else if (strncasecmp("uptime", src_string, sub_string_length) == 0)
		{
			struct sysinfo info;
			sysinfo(&info);
			sprintf(tag_value, "%ld", info.uptime);		
			sub_string_length = strnlen(tag_value, sizeof(tag_value));
			ptag_value = tag_value;
			var_found = true;
		}
		else if (strncasecmp("ramfree", src_string, sub_string_length) == 0)
		{
			// free RAM in percent
			struct sysinfo info;
			sysinfo(&info);
			sprintf(tag_value, "%ld", info.freeram * 100 / info.totalram);		
			sub_string_length = strnlen(tag_value, sizeof(tag_value));
			ptag_value = tag_value;
			var_found = true;
		}
		else if (strncasecmp("diskfree_mb", src_string, sub_string_length) == 0)
		{
			struct statvfs fsinfo;

  			if ( ! statvfs("/", &fsinfo) )
			{
				sprintf(tag_value, "%ld", (fsinfo.f_bsize * fsinfo.f_bfree) >> 20);
			}
			else
			{
				LOG(3, "<%d>Error : Get file system info!\n");
				sprintf(tag_value, "0");
			}
			sub_string_length = strnlen(tag_value, sizeof(tag_value));
			ptag_value = tag_value;
			var_found = true;
		}
		else if (strncasecmp(service_prefix, src_string, strlen(service_prefix)) == 0)
		{
			const char *ptemp = src_string + strlen(service_prefix);
			int service_name_len = sub_string_length - strlen(service_prefix);
			if ( (! strlen(ptemp)) && service_name_len )
			{
				sprintf(tag_value, "0");
			}
			else
			{
				char system_cmd[128] = "systemctl is-active ";
				strncat(system_cmd, ptemp, service_name_len);
				//LOG(6, "<%d>%s\n", system_cmd);

				// Will write the output from system command in the pipe
				FILE *system_command_pipe;
				system_command_pipe = popen(system_cmd, "r");
				if ( ! fgets(tag_value, sizeof(tag_value), system_command_pipe) )
				{
					LOG(6, "<%d>Error : On read from pipe!\n");
					tag_value[0] = '\0';
				}
				else
				{
					char *ptemp = strpbrk(tag_value, "\n\r");
					// Cut the line break on the end of the string
					*ptemp = '\0';
				}
				pclose(system_command_pipe);
			}
			sub_string_length = strnlen(tag_value, sizeof(tag_value));
			ptag_value = tag_value;
			var_found = true;
		}

		if (var_found)
		{
			// Check and allocate memory
			size_t requested_size = dst_length + sub_string_length + 1;
			if (requested_size > malloc_usable_size(dst_string)) {
				if (!(dst_string = realloc(dst_string, requested_size)))
				{
					ERROR_EXIT(err_out_of_memory);
				}
			}
			strncat(dst_string, ptag_value, sub_string_length);
			dst_length = strlen(dst_string);
		}

		src_string = ptemp_src + 1; // point to character after tag
		ptemp_src = strchr(src_string, '%');
	} while (ptemp_src);

	// Check if there is anything left of the string
	sub_string_length = strnlen(src_string, MQTT_MAX_MESSAGE_LENGTH);
	if (sub_string_length)
	{
		// Check and allocate memory
		size_t requested_size = dst_length + sub_string_length + 1;
		if (requested_size > malloc_usable_size(dst_string)) {
			if (!(dst_string = realloc(dst_string, requested_size)))
			{
				ERROR_EXIT(err_out_of_memory);
			}
		}
		// Copy the rest of the string
		strncat(dst_string, src_string, sub_string_length);
		dst_length = strlen(dst_string);
	}

	return dst_string;
}

/*******************************************/ /**
 * @brief Allocate or reallocate memory to store a string. The 
 * returnet pointer must be freeing ( free(char*) ).
 * 
 * @param dst_string - Pointer to destination string to reuse the memory 
 *                     or NULL to use new allocated memory.
 * @param src_string - Pointer to given incoming string
 * @return char* - Pointer to destination string
 ***********************************************/
char *alloc_string(char *dst_string, const char *src_string)
{
	dst_string = realloc(dst_string, strnlen(src_string, MQTT_MAX_MESSAGE_LENGTH-1)+1);
	*dst_string = '\0';
	strncpy(dst_string, src_string, strnlen(src_string, MQTT_MAX_MESSAGE_LENGTH-1)+1);

	return dst_string;
}

/*******************************************/ /**
 * @brief Get the config int object
 * 
 * @param config - A valid mosquitto instance.
 * @param name - Name of option in config file.
 * @param dst_int - Pointer to store the value.
 * @param default_int - Preseted value if option not found in the file.
 * @return int - On option succes return 'CONFIG_TRUE'. If the setting was 
 *             not found or if the type of the value did not match, 
 *             return CONFIG_FALSE. 
 ***********************************************/
int get_config_int(const config_t *config, const char *name, int *dst_int, int default_int)
{
	*dst_int = default_int;
	return config_lookup_int(config, name, dst_int);
}

/*******************************************/ /**
 * @brief Get the config string object and allocate memory
 * 
 * @param config - A valid mosquitto instance.
 * @param name - Name of option in config file.
 * @param dst_string - Pointer to pointer to store the string. For allocate
 *             new memory the pointer will NULL.
 * @param src_string - Preseted value if option not found in the file.
 * @param to_pars - Flag for will parsing the string or not.
 * @return int - On option succes return 'CONFIG_TRUE'. If the setting was 
 *             not found or if the type of the value did not match, 
 *             return CONFIG_FALSE. 
 ***********************************************/
int get_config_string(const config_t *config, const char *name, char **dst_string, const char *src_string, bool to_pars)
{
	int result = config_lookup_string(config, name, &src_string);

	if (to_pars)
	{
		*dst_string = parse_string(*dst_string, src_string);
		return result;
	}

	// if found the option "name" in config use it otherwise use the given string
	*dst_string = realloc(*dst_string, strnlen(src_string, MQTT_MAX_MESSAGE_LENGTH-1)+1);
	**dst_string = '\0';
	strncpy(*dst_string, src_string, strnlen(src_string, MQTT_MAX_MESSAGE_LENGTH-1)+1);

	return result;
}

/*******************************************/ /**
 * @brief Reads the configuration file or creates it if it is not available
 * 
 * @return int - Result, ZERO at successfully, otherwise -1
 ***********************************************/
int read_config()
{
	LOG(6, "<%d>libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	LOG(6, "<%d>Config file to use : %s\n", config_file_name);

	// check exist the .conf file
	// Atributes F_OK, R_OK, W_OK, XOK or "OR" linked R_OK|X_OK
	if (access(config_file_name, F_OK))
	{
		FILE *newfile;
		if ((newfile = fopen(config_file_name, "w+")))
		{
			fprintf(newfile, preset_config_file );
			fclose(newfile);
			LOG(5, "<%d>Create a new config file\n");
		}
		else
		{
			LOG(3, "<%d>Can't create a new config file\n");
			return EXIT_FAILURE;
		}
	}

	config_t cfg;
	config_init(&cfg);

	// Read the file. If there is an error, report it and return.
	if (!config_read_file(&cfg, config_file_name))
	{
		LOG(4, "<%d>ERROR : Config file read : (%d) %s @ %s:%d\n",
				config_error_type(&cfg), config_error_text(&cfg),
				config_error_file(&cfg), config_error_line(&cfg));
		config_destroy(&cfg);
		return EXIT_FAILURE;
	}

	// Get stored settings.
	get_config_int(&cfg, "log_level", &log_level, preset_log_level);
	get_config_string(&cfg, "broker", &mqtt_broker, preset_mqtt_broker, false);
	get_config_int(&cfg, "port", &port, preset_port);
	get_config_string(&cfg, "broker_user", &broker_user, preset_broker_user, false);
	get_config_string(&cfg, "broker_password", &broker_password, preset_broker_password, false);
	get_config_int(&cfg, "shutdown_delay", &shutdown_delay, preset_shutdown_delay);
	get_config_int(&cfg, "stat_interval", &stat_interval, preset_stat_interval);
	get_config_string(&cfg, "stat_pub_topic", &stat_pub_topic, preset_stat_pub_topic, true);
	get_config_string(&cfg, "stat_pub_message", &stat_pub_message, preset_stat_pub_message, false);
	get_config_int(&cfg, "tele_interval", &tele_interval, preset_tele_interval);
	get_config_string(&cfg, "tele_pub_topic", &tele_pub_topic, preset_tele_pub_topic, true);
	get_config_string(&cfg, "tele_pub_message", &tele_pub_message, preset_tele_pub_message, false);

	get_config_string(&cfg, "pub_terminate_message", &pub_terminate_message, preset_pub_terminate_message, true);
	get_config_string(&cfg, "last_will_topic", &last_will_topic, preset_last_will_topic, true);
	get_config_string(&cfg, "last_will_message", &last_will_message, preset_last_will_message, false);

	get_config_string(&cfg, "sub_topic", &sub_topic, preset_sub_topic, true);
	get_config_int(&cfg, "QoS", &qos, preset_qos);

	// mosquitto_pub_topic_check
	// mosquitto_sub_topic_check

	config_destroy(&cfg);

	return EXIT_SUCCESS;
}

/*******************************************/ /**
 * @brief Discard all settings and give free allocated memory
 ***********************************************/
void discard_free_config()
{
	free(stat_pub_topic); stat_pub_topic = NULL;
	free(tele_pub_topic); tele_pub_topic = NULL;
	free(pub_parsed); pub_parsed = NULL;
	free(sub_topic); sub_topic = NULL;
	free(last_will_topic); last_will_topic = NULL;
	free(last_will_message); last_will_message = NULL;
	free(pub_terminate_message); pub_terminate_message = NULL;
	free(mqtt_broker); mqtt_broker = NULL;
}

/********************************************//**
 * @brief Init the mosquitto libraryconnection to MQTT broker
 ***********************************************/
void init_mosquitto()
{
	int major, minor, revision;

	// Print version of libmosquitto
	mosquitto_lib_version(&major, &minor, &revision);
	LOG(6, "<%d>Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	// Init Mosquitto Client, required for use libmosquitto
	mosquitto_lib_init();

	// Create a new client instance.
	mosq = mosquitto_new(NULL, true, NULL);
	if (!mosq)
	{
		LOG(3, "<%d>ERROR: Can't create mosquitto client!\n");
		mosquitto_lib_cleanup();
		exit(EXIT_FAILURE);
	}

	mosquitto_connect_callback_set(mosq, on_connect_callback);
	mosquitto_message_callback_set(mosq, on_message_callback);
	mosquitto_subscribe_callback_set(mosq, on_subscribe_callback);
	mosquitto_publish_callback_set(mosq, on_publish_callback);

	// mosquitto_disconnect_callback_set(mosq, void (*on_disconnect)(struct mosquitto *, void *, int));
	// mosquitto_unsubscribe_callback_set(mosq, void (*on_unsubscribe)(struct mosquitto *, void *, int));
}

/********************************************//**
 * @brief Connect the MQTT broker
 ***********************************************/
void connect_broker()
{
	int err = 0;
	// Set the last will must be called before calling mosquitto_connect.
	if ( (err = mosquitto_will_set(mosq, last_will_topic, 
			strlen(last_will_message), last_will_message, 
			QOS_MOST_ONCE_DELIVERY, false) ))
	{
		LOG(3, "<%d>ERROR: Last will cant't set : %d %s!\n", err, mosquitto_connack_string(err));
	}

	if (broker_user && broker_password)
	{
		if ((strnlen(broker_user, 128) > 0) && (strnlen(broker_password, 128) > 0))
		{
			// This is must be called before calling mosquitto_connect.
			mosquitto_username_pw_set(mosq, broker_user, broker_password);
		}
	}

	if ( (err = mosquitto_connect(mosq, mqtt_broker, port, keepalive)) )
	{
		LOG(3, "<%d>ERROR: Unable to connect MQTT-broker %s:%d\n", mqtt_broker, port);
		exit(EXIT_FAILURE);
	}

	if ( (err = mosquitto_loop_start(mosq)) )
	{
		LOG(3, "<%d>ERROR: Unable to start work loop\n");
		exit(EXIT_FAILURE);
	}
}

/*******************************************/ /**
 * @brief Trigert by the client receives a CONNACK message from the broker.
 * 
 * @param mosq - Pointer to a valid mosquitto instance.
 * @param userdata - Defined in mosquitto_new, a pointer to an object that will be 
 *            an argument on any callbacks.
 * @param result - Connect return code, the values are defined by the MQTT protocol
 *            http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html#_Table_3.1_-
 ***********************************************/
void on_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	if (!result)
	{
		connected = true;

		LOG(5, "<%d>Connecting to MQTT-broker '%s:%d' success\n", mqtt_broker, port);
		if (strlen(sub_topic) > 0)
		{
			// Subscribe to broker information topics on successful connect.
			if (mosquitto_sub_topic_check(sub_topic) == MOSQ_ERR_SUCCESS) 
			{
				LOG(5, "<%d>Subscribe : %s\n", sub_topic);
				mosquitto_subscribe(mosq, NULL, sub_topic, qos);
			}
			else
			{
				LOG(4, "<%d>Invalid subscribe topic : %s\n", sub_topic);
			}
		}
	}
	else
	{
		connected = false;

		LOG(3, "<%d>ERROR : Connect to MQTT-broker failed : %d %s!\n",
				result, mosquitto_connack_string(result));
		if (result == MOSQ_ERR_CONN_REFUSED) // not authorised
		{
			exit(EXIT_FAILURE);
		}
	}
}

/*******************************************/ /**
 * @brief Trigert by when the broker responds to a subscription request
 * 
 * @param mosq - Pointer to a valid mosquitto instance.
 * @param userdata - Defined in mosquitto_new, a pointer to an object that will be 
 *            an argument on any callbacks. 
 * @param mid - The message id of the subscribe message.
 * @param qos_count - Value 0, 1 or 2 indicating the Quality of Service to be
 *            used for the will.
 * @param granted_qos - An array indicating the granted QoS for each of the subscriptions.
 ***********************************************/
void on_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	LOG(6, "<%d>Subscribed : (mid: %d): %d\n", mid, granted_qos[0]);
}

/*******************************************/ /**
 * @brief Trigert by a incomming message
 * 
 * @param mosq - Pointer to a valid mosquitto instance.
 * @param userdata - Defined in mosquitto_new, a pointer to an object that will be 
 *            an argument on any callbacks. 
 * @param message - Struct of received message
 ***********************************************/
void on_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	if (message->payloadlen)
	{
		LOG(5, "<%d>Message incomming : %s %s\n", message->topic, (char *)message->payload);
		
		char **topic_part;
		int topic_part_count;
		
		// Splitt topic substring array
		mosquitto_sub_topic_tokenise(message->topic, &topic_part, &topic_part_count);

		// toggle, off, reboot
		if ( ! (strcasecmp("cmnd", topic_part[0]))) {
			if ( ! (strcasecmp("POWER1", topic_part[2]))) {
				if (getuid() == 0)
				{
					if ( ! strcasecmp("off", message->payload)) 
					{
						shutdown_cmd = shutdown_poweroff;
						kill(0, SIGTERM);
					}
					else if ( ! strcasecmp("toggle", message->payload)) 
					{
						shutdown_cmd = shutdown_poweroff;
						kill(0, SIGTERM);
					}
					else if ( ! strcasecmp("reboot", message->payload)) 
					{
						shutdown_cmd = shutdown_reboot;
						kill(0, SIGTERM);
					}
				}
				else
				{
					LOG(4, "<%d>Command permission denied\n");
				}
			}
		}
		
		mosquitto_sub_topic_tokens_free(&topic_part, topic_part_count);
	}
	else
	{
		LOG(6, "<%d>Message incomming whithout payload : %s (null)\n", message->topic);
	}
	fflush(stdout);
}

/*******************************************/ /**
 * @brief Trigert by a message has been successfully sent (published).
 * 
 * @param mosq - Pointer to a valid mosquitto instance.
 * @param userdata - Defined in mosquitto_new, a pointer to an object that will be 
 *            an argument on any callbacks. 
 * @param mid - The message id of the sent message.
 ***********************************************/
void on_publish_callback(struct mosquitto *mosq, void *userdata, int mid)
{
	LOG(6, "<%d>Successfully published : (mid: %d)\n", mid);
}

/*******************************************/ /**
 * @brief Processing of the received signals
 * 
 * void 
 *signal_handler(int sig)
 * 
 * @param iSignal : catched signal number
 ***********************************************/
void signal_handler(int iSignal)
{
	LOG(5, "<%d>signal : %s\n", strsignal(iSignal));

	switch (iSignal)
	{
	case SIGTERM:
		// triggert by systemctl stop process
		LOG(5, "<%d>Terminate signal triggered\n");
		exit(EXIT_SUCCESS);
		break;
	case SIGINT:
		// triggert by pressing Ctrl-C in terminal
		//fflush(stdin);
		LOG(5, "<%d>Ctrl-C signal triggered\n");
		exit(EXIT_SUCCESS);
		break;
	case SIGHUP:
		// trigger defined in *.service file ExecReload=
		LOG(5, "<%d>Reload config\n");

		int err = 0;
		err |= mosquitto_will_clear(mosq);
		err |= mosquitto_unsubscribe(mosq,	NULL, sub_topic);
		err |= mosquitto_disconnect(mosq);
		err |= mosquitto_loop_stop(mosq, false);
		if ( err )
		{
			LOG(4, "<%d>Error on discard broker connection\n");
		}

		// Free allocated memory
		discard_free_config();

		read_config();
		connect_broker();
		break;
	case SIGTSTP:
		// triggert by pressing Ctrl-C in terminal
		LOG(5, "<%d>Ctrl-Z signal triggered -> process pause\n");
		pause_flag = true;
		break;
	case SIGCONT:
		// trigger by run 'kill -SIGCONT <PID>'
		LOG(5, "<%d>Continue paused process\n");
		pause_flag = false;
		break;
	}
}

/*******************************************/ /**
 * @brief Initialize signals to be catched
 * 
 ***********************************************/
void init_signal_handler()
{
	struct sigaction new_action;

	new_action.sa_handler = signal_handler;
	sigfillset(&new_action.sa_mask);
	new_action.sa_flags = 0;

	// Register signal handler
	if (sigaction(SIGTERM, &new_action, NULL))
	{
		ERROR_EXIT(err_register_sigaction);
	}

	if (sigaction(SIGHUP, &new_action, NULL) != 0)
	{
		ERROR_EXIT(err_register_sigaction);
	}

	if (sigaction(SIGINT, &new_action, NULL) != 0)
	{
		ERROR_EXIT(err_register_sigaction);
	}

	if (sigaction(SIGTSTP, &new_action, NULL) != 0)
	{
		ERROR_EXIT(err_register_sigaction);
	}

	if (sigaction(SIGCONT, &new_action, NULL) != 0)
	{
		ERROR_EXIT(err_register_sigaction);
	}
}

/*******************************************/ /**
 * @brief Main function
 * 
 * @param argc - Count of command line parameter
 * @param argv - An Array of commend line parameter
 * @return int - Exit Code, Zero at success otherwise -1
 ***********************************************/
int main(int argc, char *argv[])
{
	LOG(4, "<%d>MQTT-Heartbeat %s started  [PID %d] [PPID %d]\n", VERSION_STR, getpid(), getppid());
				
	if (getppid() == 1)
	{
		// run as daemon
	}
	else
	{
		LOG(6, "<%d>Starting not as daemon by user ID %d privileges.\n", getuid());
	}

	// Check command line arguments
	if (argc > 1) {
		int opt = 0;

		// Parse command line arguments
		// letter followed by a colon requires an option
		while ( (opt = getopt_long(argc, argv, "uc:", NULL, NULL)) > 0 ) {
			switch(opt) {
				case 'u':
					LOG(5, "<%d>Socket unlinking ...\n");
					unlink(lock_socket_name);
					break;
				case 'c':
					LOG(5, "<%d>config file : %s\n", optarg);
					config_file_name = optarg;
					if (strlen(config_file_name) >= PATH_MAX)
					{
						LOG(3, "<%d>Config file path to long!\n");
						_exit(EXIT_FAILURE);
					}
					break;
				case '?':
					LOG(4, "<%d>Invalid command line option '%c'\n", optopt);
					break;
				case -1:
					//LOG(6, "<%d>Command line argument parsing finished\n");
					break;
			}
		}
	}	

	// Allow only one instance and finish each one more
	terminate_second_instance();

	// Only called after exit() and not after _exit()
	atexit(clean_exit);

	// if not config file set by commandline, get directory and config file name 
	if ( ! config_file_name) {
		// LOG(6, "<%d>  CWD   : %s\n", getcwd(NULL, 0));
		// LOG(6, "<%d>  Arg 0 : %s\n", argv[0]);
		// LOG(6, "<%d>  Base  : %s\n", basename(argv[0]));

		if (getppid() == 1) 
		{
			// run as daemon, as config file use system config directory and 
			// programm name with config file extension
			char *base_file_name = basename(argv[0]);
			int config_path_length = strlen(system_config_dir) + strlen(base_file_name) + strlen(config_file_ext);
			if (config_path_length >= PATH_MAX)
			{
				LOG(3, "<%d>Config file path to long!\n");
				exit(EXIT_FAILURE);
			}
			config_file_name = malloc(config_path_length + 1);
			*config_file_name = '\0';
			strncat(config_file_name, system_config_dir, strnlen(system_config_dir, config_path_length));
			strncat(config_file_name, base_file_name, strnlen(base_file_name, 
						config_path_length-strlen(config_file_name)));
			strncat(config_file_name, config_file_ext, strnlen(config_file_ext, 
						config_path_length-strlen(config_file_name)));
		}
		else
		{
			// get the name of programm and expand it with config file extansion
			int config_path_length = strlen(argv[0]) + strlen(config_file_ext);
			if (config_path_length >= PATH_MAX)
			{
				LOG(3, "<%d>Config file path to long!\n");
				exit(EXIT_FAILURE);
			}
			config_file_name = malloc(config_path_length + 1);
			*config_file_name = '\0';
			strncat(config_file_name, argv[0], strnlen(argv[0], config_path_length));
			strncat(config_file_name, config_file_ext, strnlen(config_file_ext, 
						config_path_length - strlen(config_file_name)));
		}
	}

	// read parameter from config file
	int err = read_config();
	if (err)
	{
		LOG(4, "<%d>WARNING: Config file I/O error, use default settings!\n");
	}
	LOG(6, "<%d>Config file processed\n");

	// Initialize connetction to MQTT broker
	init_mosquitto();
	connect_broker();

	// Initialize signals to be catched
	init_signal_handler();

	int stat_couter = stat_interval;
	int tele_couter = tele_interval;

	// Main Loop
	while (1)
	{
		if (shutdown_cmd)
		{
			continue;
		}
		
		if (pause_flag)
			pause();

		if (connected && (! stat_couter--) && (stat_interval > 0)) {
			pub_parsed = parse_string(pub_parsed, stat_pub_message);
			LOG(6, "<%d>Sending status ... \n");
			//LOG(6, "<%d>Sending heartbeat ... %s : %s\n", pub_topic, pub_parsed);
			mosquitto_publish(mosq, NULL, stat_pub_topic, strlen(pub_parsed), pub_parsed, qos, false);
			stat_couter = stat_interval;
		}

		if (connected && (! tele_couter--) && (tele_interval > 0)) {
			pub_parsed = parse_string(pub_parsed, tele_pub_message);
			LOG(6, "<%d>Sending telemetry ... \n");
			//LOG(6, "<%d>Sending heartbeat ... %s : %s\n", pub_topic, pub_parsed);
			mosquitto_publish(mosq, NULL, tele_pub_topic, strlen(pub_parsed), pub_parsed, qos, false);
			tele_couter = tele_interval;
		}

		sleep(1);
	}

	// This code is never executed but when it is, the process 
	// is cleanly terminated with the function specified in atexit().
	LOG(6, "<%d>Finished ...\n");
	return EXIT_SUCCESS;
}
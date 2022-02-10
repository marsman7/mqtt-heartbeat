/*******************************************/ /**
 * @file mqtt-heartbeat.c
 * @author your name (you@domain.com)
 * @brief MQTT-Heartbeat is a Linux daemon that 
 *        periodically sends a status message via MQTT.
 * @version 0.1
 * @date 2022-02-02
 * 
 * @copyright Copyright (c) 2022
 * 
 * @link  https://mosquitto.org/api/files/mosquitto-h.html
 * 
 ***********************************************/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/reboot.h>
//#include <linux/reboot.h>
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
#define errExit(msg) do	{perror(msg); _exit(EXIT_FAILURE); } while (0)

#define MQTT_MAX_MESSAGE_LENGTH 1024

//-----------------------------------------------
typedef void (*sighandler_t)(int);
char *config_file_name = NULL;
bool pause_flag = false; 	/*!< set it TRUE the process go to paused, send SIGCONT to continue the process */
bool reboot_flag = false;
bool shutdown_flag = false;
int keepalive = 30;
char *mqtt_broker = NULL;
struct mosquitto *mosq = NULL; //! mosquitto client instance
int status = STAT_ON;
char *pub_parsed = NULL;

//-----------------------------------------------
char *parse_string(const char *, char *);


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
		pub_parsed = parse_string(stat_pub_message, pub_parsed);
		mosquitto_publish(mosq, NULL, stat_pub_topic, strlen(pub_parsed), pub_parsed, qos, false);

		fprintf(stderr, "wait");
		int i = 200;
		while (mosquitto_want_write(mosq) && (i > 0))
		{
			fprintf(stderr, ".");
			usleep(100000);	// sleep 100ms
			i--;
		}
		fprintf(stderr, "end\n");

		// Terminate the conection to MQTT broker
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
	}

	// Remove link to the socket for run instance only once
	unlink(lock_socket_name);

	// Free allocated memory
	free(stat_pub_topic);
	free(tele_pub_topic);
	free(pub_parsed);
	free(sub_topic);
	free(last_will_topic);
	free(last_will_message);
	free(pub_terminate_message);
	free(mqtt_broker);

	fprintf(stderr, "<4>Cleanly teminated\n");

	int return_value = 0;
	if (reboot_flag)
	{
		sync();
		char shutdown_cmd[32] = "\n";
		sprintf(shutdown_cmd, "shutdown --reboot %d", shutdown_delay);
		return_value = system(shutdown_cmd);
	}
	else if (shutdown_flag)
	{
		sync();
		char shutdown_cmd[32] = "\n";
		sprintf(shutdown_cmd, "shutdown --poweroff %d", shutdown_delay);
		return_value = system(shutdown_cmd);
	}
	if (return_value)
	{
		fprintf(stderr, "<3>System command faild!\n");
	}

	_exit(EXIT_SUCCESS);
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
	// fprintf(stderr, "<5>signal : %s\n", strsignal(iSignal));

	switch (iSignal)
	{
	case SIGTERM:
		// triggert by systemctl stop process
		fprintf(stderr, "<5>Terminate signal triggered\n");
		exit(EXIT_SUCCESS);
		break;
	case SIGINT:
		// triggert by pressing Ctrl-C in terminal
		//fflush(stdin);
		fprintf(stderr, "<5>Ctrl-C signal triggered\n");
		exit(EXIT_SUCCESS);
		break;
	case SIGHUP:
		// trigger defined in *.service file ExecReload=
		fprintf(stderr, "<5>Hangup signal triggered\n");
		break;
	case SIGTSTP:
		// triggert by pressing Ctrl-C in terminal
		fprintf(stderr, "<5>Ctrl-Z signal triggered -> process pause\n");
		pause_flag = true;
		break;
	case SIGCONT:
		// trigger by run 'kill -SIGCONT <PID>'
		fprintf(stderr, "<5>Continue paused process\n");
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
		errExit(err_register_sigaction);
	}

	if (sigaction(SIGHUP, &new_action, NULL) != 0)
	{
		errExit(err_register_sigaction);
	}

	if (sigaction(SIGINT, &new_action, NULL) != 0)
	{
		errExit(err_register_sigaction);
	}

	if (sigaction(SIGTSTP, &new_action, NULL) != 0)
	{
		errExit(err_register_sigaction);
	}

	if (sigaction(SIGCONT, &new_action, NULL) != 0)
	{
		errExit(err_register_sigaction);
	}
}

/*******************************************/ /**
 * @brief Pars a string and exchang the tags.
 * A Tag must be surrounded with %. For example foo/%bar%/for/%every%
 * The returnet pointer must be freeing ( free(char*) ).
 * 
 * @param src_string - Given incoming string
 * @return char* - Pointer to parsed string
 ***********************************************/
char *parse_string(const char *src_string, char *dst_string)
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
				errExit(err_out_of_memory);
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
			double loadavgs[3];
			getloadavg(loadavgs, 3);
			sprintf(tag_value, "%d", (int)loadavgs[0]*1000);		
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
					errExit(err_out_of_memory);
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
				errExit(err_out_of_memory);
			}
		}
		// Copy the rest of the string
		strncat(dst_string, src_string, sub_string_length);
		dst_length = strlen(dst_string);
	}

	return dst_string;
}

/*******************************************/ /**
 * @brief Reads the configuration file or creates it if it is not available
 * 
 * @return int - Result, ZERO at successfully, otherwise -1
 ***********************************************/
int read_config()
{
	fprintf(stderr, "<6>libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	fprintf(stderr, "<6>Config file to use : %s\n", config_file_name);

	// check exist the .conf file
	// Atributes F_OK, R_OK, W_OK, XOK or "OR" linked R_OK|X_OK
	if (access(config_file_name, F_OK))
	{
		FILE *newfile;
		if ((newfile = fopen(config_file_name, "w+")))
		{
			fprintf(newfile, preset_config_file );
			fclose(newfile);
			fprintf(stderr, "<5>Create a new config file\n");
		}
		else
		{
			fprintf(stderr, "<3>Can't create a new config file\n");
			return EXIT_FAILURE;
		}
	}

	config_t cfg;
	config_init(&cfg);

	// Read the file. If there is an error, report it and return.
	if (!config_read_file(&cfg, config_file_name))
	{
		fprintf(stderr, "<4>ERROR : Config file read : (%d) %s @ %s:%d\n",
				config_error_type(&cfg), config_error_text(&cfg),
				config_error_file(&cfg), config_error_line(&cfg));
		config_destroy(&cfg);
		return EXIT_FAILURE;
	}

	// Get stored settings.
	config_lookup_string(&cfg, "broker", &preset_mqtt_broker);
	config_lookup_int(&cfg, "port", &port);
	config_lookup_int(&cfg, "shutdown_delay", &shutdown_delay);
	config_lookup_int(&cfg, "stat_interval", &stat_interval);
	config_lookup_string(&cfg, "stat_pub_topic", &preset_stat_pub_topic);
	config_lookup_string(&cfg, "stat_pub_message", &preset_stat_pub_message);
	config_lookup_int(&cfg, "tele_interval", &tele_interval);
	config_lookup_string(&cfg, "tele_pub_topic", &preset_tele_pub_topic);
	config_lookup_string(&cfg, "tele_pub_message", &preset_tele_pub_message);
	config_lookup_string(&cfg, "sub_topic", &preset_sub_topic);
	config_lookup_int(&cfg, "QoS", &qos);

	stat_pub_topic = parse_string(preset_stat_pub_topic, NULL);
	tele_pub_topic = parse_string(preset_tele_pub_topic, NULL);
	sub_topic = parse_string(preset_sub_topic, NULL);
	last_will_topic = parse_string(preset_last_will_topic, NULL);
	last_will_message = parse_string(preset_last_will_message, NULL);
	pub_terminate_message = parse_string(preset_pub_terminate_message, NULL);

	if ( ! stat_pub_message )
	{
		stat_pub_message = realloc(stat_pub_message, strnlen(preset_stat_pub_message, MQTT_MAX_MESSAGE_LENGTH));
		*stat_pub_message = '\0';
	}
	strncpy(stat_pub_message, preset_stat_pub_message, strnlen(preset_stat_pub_message, MQTT_MAX_MESSAGE_LENGTH-1)+1);

	if ( ! tele_pub_message )
	{
		tele_pub_message = realloc(tele_pub_message, strnlen(preset_tele_pub_message, MQTT_MAX_MESSAGE_LENGTH));
		*tele_pub_message = '\0';
	}
	strncpy(tele_pub_message, preset_tele_pub_message, strnlen(preset_tele_pub_message, MQTT_MAX_MESSAGE_LENGTH-1)+1);

	if ( ! mqtt_broker )
	{
		mqtt_broker = realloc(mqtt_broker, strnlen(preset_mqtt_broker, 128));
		*mqtt_broker = '\0';
	}
	strncpy(mqtt_broker, preset_mqtt_broker, strnlen(preset_mqtt_broker, 128-1)+1);


	// mosquitto_pub_topic_check
	// mosquitto_sub_topic_check

	config_destroy(&cfg);

	return EXIT_SUCCESS;
}

/*******************************************/ /**
 * @brief Trigert by the client receives a CONNACK message from the broker.
 * 
 * @param mosq - Pointer to a valid mosquitto instance.
 * @param userdata - Defined in mosquitto_new, a pointer to an object that will be 
 *            an argument on any callbacks.
 * @param result - Connect return code, the values are defined by the MQTT protocol
 *            http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html#_Table_3.1_-
 * 
 * @todo #1
 ***********************************************/
void on_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	if (!result)
	{
		fprintf(stderr, "<5>Connect to MQTT-broker success\n");
		if (strlen(sub_topic) > 0)
		{
			// Subscribe to broker information topics on successful connect.
			if (mosquitto_sub_topic_check(sub_topic) == MOSQ_ERR_SUCCESS) 
			{
				fprintf(stderr, "<5>Subscribe : %s\n", sub_topic);
				mosquitto_subscribe(mosq, NULL, sub_topic, qos);
			}
			else
			{
				fprintf(stderr, "<4>Invalid subscribe topic : %s\n", sub_topic);
			}
		}
	}
	else
	{
		fprintf(stderr, "<3>ERROR : Connect to MQTT-broker failed : %s!\n",
				mosquitto_connack_string(result));
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
	int i;

	fprintf(stderr, "<6>Subscribed : (mid: %d): %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++)
	{
		fprintf(stderr, ", %d", granted_qos[i]);
	}
	fprintf(stderr, "\n");
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
		fprintf(stderr, "<6>Message incomming : %s %s\n", message->topic, (char *)message->payload);
		
		char **topic_part;
		int topic_part_count;
		mosquitto_sub_topic_tokenise(message->topic, &topic_part, &topic_part_count);
		/*
		for (int i=0; i<topic_part_count; i++) {
			fprintf(stderr, "<6>Topic part %d : %s\n", i, topic_part[i]);
		}
		*/

		// toggle, off, reboot
		if ( ! (strcasecmp("cmnd", topic_part[0]))) {
			if ( ! (strcasecmp("POWER1", topic_part[2]))) {
				if ( ! strcasecmp("off", message->payload)) {
					// fprintf(stderr, "<4>Shutdown ...\n");

					if (getuid() == 0)
					{
						reboot_flag = true;
						exit(EXIT_SUCCESS);
					}
					else
					{
						fprintf(stderr, "<4>Shutdown permission denied\n");
					}
				}
				else if ( ! strcasecmp("reboot", message->payload)) {
					// fprintf(stderr, "<4>Reboot ...\n");

					if (getuid() == 0)
					{
						reboot_flag = true;
						exit(EXIT_SUCCESS);
					}
					else
					{
						fprintf(stderr, "<4>Reboot permission denied\n");
					}
				}
			}
		}
		
		mosquitto_sub_topic_tokens_free(&topic_part, topic_part_count);
	}
	else
	{
		fprintf(stderr, "<6>Message incomming : %s (null)\n", message->topic);
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
	fprintf(stderr, "<6>Successfully published : (mid: %d)\n", mid);
}

/********************************************//**
 * @brief Init the connection to MQTT broker
 ***********************************************/
void init_mosquitto_connection()
{
	int major, minor, revision;

	// Print version of libmosquitto
	mosquitto_lib_version(&major, &minor, &revision);
	fprintf(stderr, "<6>Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	// Init Mosquitto Client, required for use libmosquitto
	mosquitto_lib_init();

	// Create a new client instance.
	mosq = mosquitto_new(NULL, true, NULL);
	if (!mosq)
	{
		fprintf(stderr, "<3>ERROR: Can't create mosquitto client!\n");
		mosquitto_lib_cleanup();
		exit(EXIT_FAILURE);
	}

	mosquitto_connect_callback_set(mosq, on_connect_callback);
	mosquitto_message_callback_set(mosq, on_message_callback);
	mosquitto_subscribe_callback_set(mosq, on_subscribe_callback);
	mosquitto_publish_callback_set(mosq, on_publish_callback);

	// Set the last will must be called before calling mosquitto_connect.
	// int mosquitto_will_set(mosq, topic, payloadlen, payload, qos, retain);
	mosquitto_will_set(mosq, last_will_topic, 
			strlen(last_will_message), last_will_message, 
			QOS_MOST_ONCE_DELIVERY, false);

	if (mosquitto_connect(mosq, mqtt_broker, port, keepalive))
	{
		fprintf(stderr, "<3>ERROR: Unable to connect MQTT-broker %s:%d\n", mqtt_broker, port);
		exit(EXIT_FAILURE);
	}

	if (mosquitto_loop_start(mosq))
	{
		fprintf(stderr, "<3>ERROR: Unable to connect MQTT-broker %s:%d\n", mqtt_broker, port);
		exit(EXIT_FAILURE);
	}
}

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
        fprintf(stderr, "<3>Could not create socket.\n");
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
			fprintf(stderr, "<4>An instance is already running, this will be terminated.\n");
		} 
		else 
		{
			char *errmsg = strerror( errno );
			fprintf(stderr, "<3>Error on binding socket : %d; %s; %s\n", errno, errmsg, lock_socket_name);
		}
		close(socket_fd);
        _exit(EXIT_FAILURE);
    } else {
        //fprintf(stderr, "<6>First instance\n");
    }
	close(socket_fd);
}

/*******************************************/ /**
 * @brief Main function
 * 
 * @param argc - Count of command line parameter
 * @param argv - An Array of commend line parameter
 * @return int - Exit Code, Zero at success otherwise -1
 * 
 * @todo #2 make QOS on publish editable
 ***********************************************/
int main(int argc, char *argv[])
{
	fprintf(stderr, "<4>Process started [PID - %d] [PPID - %d] ...\n", getpid(), getppid());
	if (getppid() == 1)
	{
		// run as daemon
	}
	else
	{
		fprintf(stderr, "<6>Starting not as daemon by user ID %d privileges.\n", getuid());
	}

	// Check command line arguments
	if (argc > 1) {
		int opt = 0;

		// Parse command line arguments
		// letter followed by a colon requires an option
		while ( (opt = getopt_long(argc, argv, "uc:", NULL, NULL)) > 0 ) {
			switch(opt) {
				case 'u':
					fprintf(stderr, "<5>Socket unlinking ...\n");
					unlink(lock_socket_name);
					break;
				case 'c':
					fprintf(stderr, "<5>config file : %s\n", optarg);
					config_file_name = optarg;
					if (strlen(config_file_name) >= PATH_MAX)
					{
						fprintf(stderr, "<3>Config file path to long!\n");
						_exit(EXIT_FAILURE);
					}
					break;
				case '?':
					fprintf(stderr, "<4>Invalid command line option '%c'\n", optopt);
					break;
				case -1:
					//fprintf(stderr, "<6>Command line argument parsing finished\n");
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
		// fprintf(stderr, "<6>  CWD   : %s\n", getcwd(NULL, 0));
		// fprintf(stderr, "<6>  Arg 0 : %s\n", argv[0]);
		// fprintf(stderr, "<6>  Base  : %s\n", basename(argv[0]));

		if (getppid() == 1) 
		{
			// run as daemon, as config file use system config directory and 
			// programm name with config file extension
			char *base_file_name = basename(argv[0]);
			int config_path_length = strlen(system_config_dir) + strlen(base_file_name) + strlen(config_file_ext);
			if (config_path_length >= PATH_MAX)
			{
				fprintf(stderr, "<3>Config file path to long!\n");
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
				fprintf(stderr, "<3>Config file path to long!\n");
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
		fprintf(stderr, "<4>WARNING: Config file I/O error, use default settings!\n");
	}
	fprintf(stderr, "<6>Config file processed ...\n");

	// Initialize connetction to MQTT broker
	init_mosquitto_connection();
	fprintf(stderr, "<5>MQTT-broker connected : %s:%d\n", mqtt_broker, port);

	// Initialize signals to be catched
	init_signal_handler();

	int stat_couter = stat_interval;
	int tele_couter = tele_interval;

	// Main Loop
	while (1)
	{
		if (reboot_flag || shutdown_flag)
		{
			continue;
		}
		
		if (pause_flag)
			pause();

		if ( (! stat_couter--) && (stat_interval > 0)) {
			pub_parsed = parse_string(stat_pub_message, pub_parsed);
			fprintf(stderr, "<6>Sending status ... \n");
			//fprintf(stderr, "<6>Sending heartbeat ... %s : %s\n", pub_topic, pub_parsed);
			mosquitto_publish(mosq, NULL, stat_pub_topic, strlen(pub_parsed), pub_parsed, qos, false);
			stat_couter = stat_interval;
		}

		if ( (! tele_couter--) && (tele_interval > 0)) {
			pub_parsed = parse_string(tele_pub_message, pub_parsed);
			fprintf(stderr, "<6>Sending telemetry ... \n");
			//fprintf(stderr, "<6>Sending heartbeat ... %s : %s\n", pub_topic, pub_parsed);
			mosquitto_publish(mosq, NULL, tele_pub_topic, strlen(pub_parsed), pub_parsed, qos, false);
			tele_couter = tele_interval;
		}

		sleep(1);
	}

	// This code is never executed but when it is, the process 
	// is cleanly terminated with the function specified in atexit().
	fprintf(stderr, "<6>Finished ...\n");
	return EXIT_SUCCESS;
}
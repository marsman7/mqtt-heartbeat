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
#include <syslog.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <libconfig.h>
#include <mosquitto.h> // for MQTT funtionallity

/*******************************************/ /**
 * @brief List of Quality of Service levels
 * 
 ***********************************************/
enum qos_t
{
	QOS_MOST_ONCE_DELIVERY = 0,
	QOS_LEAST_ONCE_DELIVERY = 1,
	QOS_EXACTLY_ONCE_DELIVERY = 2,
};

const char *config_file_name = "mqtt-heartbeat.conf";
const char *err_register_sigaction = "ERROR : Registering the signal handler fail!\n";
const char *err_out_of_memory = "ERROR : Out of memory!\n";

int keepalive = 30;
struct mosquitto *mosq = NULL; //! mosquitto client instance

const char *mqtt_broker = "localhost";
int port = 1883;
int interval = 5;
char *pub_topic = NULL;
const char *preset_pub_topic = "foo/\%hostname\%/STATE";
char *pub_message = NULL;
const char *preset_pub_message = "{\"POWER\":\"ON\"}";
char *sub_topic = NULL;
const char *preset_sub_topic = "cmnd/\%hostname\%/POWER";
int qos = QOS_MOST_ONCE_DELIVERY;

bool pause_flag = false; /*!< if it set to TRUE the process go to paused,
							 	 send SIGCONT to continue the process */
//int exit_code = EXIT_SUCCESS;

typedef void (*sighandler_t)(int);

#define errExit(msg)         \
	do                       \
	{                        \
		perror(msg);         \
		_exit(EXIT_FAILURE); \
	} while (0)

/*******************************************/ /**
 * @brief Terminate the process
 * 
 ***********************************************/
void lastCall()
{
	fprintf(stderr, "<6>free : %p, %lu \n", pub_topic, malloc_usable_size(pub_topic));
	free(pub_topic);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	fprintf(stderr, "<4>teminated ...\n");
	exit(EXIT_SUCCESS);
}

/*******************************************/ /**
 * @brief Processing of the received signals
 * 
 * void my_signal_handler(int sig)
 * 
 * @param iSignal : catched signal number
 ***********************************************/
void my_signal_handler(int iSignal)
{
	fprintf(stderr, "<5>signal : %s\n", strsignal(iSignal));

	switch (iSignal)
	{
	case SIGTERM:
		// triggert by systemctl stop process
		fprintf(stderr, "<5>Terminate signal triggered ...\n");
		exit(EXIT_SUCCESS);
		break;
	case SIGINT:
		// triggert by pressing Ctrl-C in terminal
		fprintf(stderr, "<5>Ctrl-C signal triggered ...\n");
		exit(EXIT_SUCCESS);
		break;
	case SIGHUP:
		// trigger defined in *.service file ExecReload=
		fprintf(stderr, "<5>Hangup signal triggered ...\n");
		break;
	case SIGTSTP:
		// triggert by pressing Ctrl-C in terminal
		fprintf(stderr, "<5>Ctrl-Z signal triggered -> process pause ...\n");
		pause_flag = true;
		break;
	case SIGCONT:
		// trigger by run 'kill -SIGCONT <PID>'
		fprintf(stderr, "<5>Continue paused process ...\n");
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

	new_action.sa_handler = my_signal_handler;
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
char *parse_string(const char *src_string)
{
	// unsigned int size = 0;
	char *ptemp_src = NULL;

	// Check if a tag is included in the topic and get the begin of the tag
	ptemp_src = strchr(src_string, '%');
	if (!ptemp_src)
	{
		// topic does not contain tags
		return (char *)src_string;
	}

	unsigned int dst_length = 0;
	unsigned int sub_string_length = 0;
	char *dst_string = NULL; //! Pointer to destination parsed string
	bool var_found = false;	//! indicates a valid tag 
	char tag_value[64] = "\0";
	char *ptag_value = NULL;

	do
	{
		sub_string_length = ptemp_src - src_string;
		// Check and allocate memory
		if ((dst_length + sub_string_length + 1) > malloc_usable_size(dst_string)) {
			if (!(dst_string = realloc(dst_string, dst_length + sub_string_length + 1)))
			{
				errExit(err_out_of_memory);
			}
		}

		strncat(dst_string + dst_length, src_string, sub_string_length);
		dst_length += sub_string_length;
		src_string = ptemp_src + 1; // point to character after tag

		// Search close tag
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
			sub_string_length = strnlen(tag_value, 63);
			var_found = true;
			ptag_value = tag_value;
		} 
		else if (strncasecmp("user", src_string, sub_string_length) == 0) 
		{
			ptag_value = secure_getenv("USER");
			sub_string_length = strnlen(ptag_value, 63);
			var_found = true;
		}

		if (var_found)
		{
			// Check and allocate memory
			if ((dst_length + sub_string_length + 1) > malloc_usable_size(dst_string)) {
				if (!(dst_string = realloc(dst_string, dst_length + sub_string_length + 1)))
				{
					errExit(err_out_of_memory);
				}
			}
			strncat(dst_string + dst_length, ptag_value, sub_string_length);
			dst_length += sub_string_length;
		}

		src_string = ptemp_src + 1; // point to character after tag
		ptemp_src = strchr(src_string, '%');
	} while (ptemp_src);

	sub_string_length = strnlen(src_string, 1024);
	if (sub_string_length)
	{
		if ((dst_length + sub_string_length + 1) > malloc_usable_size(dst_string)) {
			if (!(dst_string = realloc(dst_string, dst_length + sub_string_length + 1)))
			{
				errExit(err_out_of_memory);
			}
		}
		strncat(dst_string + dst_length, src_string, sub_string_length);
		dst_length += sub_string_length;
	}

	return dst_string;
}

/*******************************************/ /**
 * @brief Reads the configuration file or creates it if it is not available
 * 
 * @return int - Result, ZERO at successfully, otherwise -1
 ***********************************************/
int configReader()
{
	fprintf(stderr, "<6>libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	fprintf(stderr, "<6>config file to use : %s\n", config_file_name);

	// check exist the .conf file
	// Atributes F_OK, R_OK, W_OK, XOK or "OR" linked R_OK|X_OK
	if (access(config_file_name, F_OK))
	{
		FILE *newfile;
		if ((newfile = fopen(config_file_name, "w+")))
		{
			fprintf(newfile,
					"# Name or IP of the MQTT-broker\n"
					"# default : localhost\n"
					"#broker = \"localhost\"\n"
					"\n"
					"# Broker port\n"
					"# default : 1883\n"
					"#port = 1883\n"
					"\n"
					"# The topic of published messages\n"
					"# default : \"tele/%%hostname%%/STATE\"\n"
					"#pub_topic = \"footele/%%hostname%%/STATE\"\n"
					"\n"
					"# Interval of sending status message in seconds\n"
					"# default : 5\n"
					"#interval = 5\n"
					"\n"
					"# The topic of subscribe messages\n"
					"# default : none\n"
					"#preset_sub_topic = \"cmd/%%hostname%%/STATE\"\n"
					"\n"
					"# Quality of Service Indicator Value 0, 1 or 2 to be used for the will\n"
					"# QoS 0: At most once delivery\n"
					"# QoS 1: At least once delivery\n"
					"# QoS 2: Exactly once delivery\n"
					"# default : 0\n"
					"#QoS = 0\n"
					"\n");
			fclose(newfile);
			fprintf(stderr, "<5>create a new config file\n");
		}
		else
		{
			fprintf(stderr, "<3>can't create a new config file\n");
			return EXIT_FAILURE;
		}
	}

	config_t cfg;
	config_init(&cfg);

	// Read the file. If there is an error, report it and return.
	if (!config_read_file(&cfg, config_file_name))
	{
		fprintf(stderr, "<4>Config file error : (%d) %s @ %s:%d\n",
				config_error_type(&cfg), config_error_text(&cfg),
				config_error_file(&cfg), config_error_line(&cfg));
		config_destroy(&cfg);
		return EXIT_FAILURE;
	}

	// Get stored settings.
	config_lookup_string(&cfg, "broker", &mqtt_broker);
	config_lookup_int(&cfg, "port", &port);
	config_lookup_string(&cfg, "pub_topic", &preset_pub_topic);
	config_lookup_int(&cfg, "interval", &interval);
	config_lookup_string(&cfg, "preset_sub_topic", &preset_sub_topic);
	config_lookup_int(&cfg, "QoS", &qos);

	pub_topic = parse_string(preset_pub_topic);

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
		fprintf(stderr, "<5>connect to mqtt-broker success\n");
		if (strlen(preset_sub_topic) > 0)
		{
			// Subscribe to broker information topics on successful connect.
			// mosquitto_subscribe(struct mosquitto *mosq, int *mid, const char *subscribe, int qos);
			fprintf(stderr, "<6>subscribe : %s\n", preset_sub_topic);
			mosquitto_subscribe(mosq, NULL, "mars/#", qos);
		}
	}
	else
	{
		fprintf(stderr, "<3>ERROR : connect to mqtt-broker failed : %s!\n",
				mosquitto_connack_string(result));
	}
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
		fprintf(stderr, "<6>mqtt message incomming : %s %s\n", message->topic, (char *)message->payload);
	}
	else
	{
		fprintf(stderr, "<6>mqtt message incomming : %s (null)\n", message->topic);
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
	fprintf(stderr, "<6>published : (mid: %d)\n", mid);
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

	fprintf(stderr, "<6>subscribed : (mid: %d): %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++)
	{
		fprintf(stderr, ", %d", granted_qos[i]);
	}
	fprintf(stderr, "\n");
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
	int major, minor, revision;

	fprintf(stderr, "<5>process started [pid - %d] [ppid - %d] ...\n", getpid(), getppid());
	if (getppid() != 1)
	{
		fprintf(stderr, "Starting in local only mode. Connections will only be "
						"possible from clients running on this machine.\n");
	}

	//char path[128] = "\0";
	//getcwd(path, 128);
	//fprintf(stderr, "<5>%s : %s\n", argv[0], path);

	// print the name of this machine
	char localhostname[256] = "\0";
	gethostname(localhostname, sizeof(localhostname) - 1);
	fprintf(stderr, "<6>local machine : %s\n", localhostname);

	// only called after exit() and not after _exit()
	atexit(lastCall);

	// Initialize signals to be catched
	init_signal_handler();

	// read parameter from config file
	int err = configReader();
	if (err)
	{
		fprintf(stderr, "<4>WARNING: config file I/O error, use default settings!\n");
	}
	fprintf(stderr, "<6>configuration processed ...\n");

	// Print version of libmosquitto
	mosquitto_lib_version(&major, &minor, &revision);
	fprintf(stderr, "<6>Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	// Init Mosquitto Client, required for use libmosquitto
	mosquitto_lib_init();

	// Create a new client instance.
	mosq = mosquitto_new(NULL, true, NULL);
	if (!mosq)
	{
		fprintf(stderr, "<3>ERROR: cant't create mosquitto client, out of memory!\n");
		mosquitto_lib_cleanup();
		return EXIT_FAILURE;
	}

	mosquitto_connect_callback_set(mosq, on_connect_callback);
	mosquitto_message_callback_set(mosq, on_message_callback);
	mosquitto_subscribe_callback_set(mosq, on_subscribe_callback);
	mosquitto_publish_callback_set(mosq, on_publish_callback);

	if (mosquitto_connect(mosq, mqtt_broker, port, keepalive))
	{
		fprintf(stderr, "<3>ERROR: unable to connect mqtt-broker %s:%d\n", mqtt_broker, port);
		exit(EXIT_FAILURE);
	}

	if (mosquitto_loop_start(mosq))
	{
		fprintf(stderr, "<3>ERROR: unable to connect mqtt-broker %s:%d\n", mqtt_broker, port);
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "<5>connected mqtt-broker %s:%d\n", mqtt_broker, port);

	// Main Loop
	while (1)
	{
		if (pause_flag)
			pause();

		fprintf(stderr, "<6>sending heartbeat ... %s\n", pub_topic);
		// int mosquitto_publish(struct mosquitto , mid, topic, payloadlen, payload, qos, retain)
		mosquitto_publish(mosq, NULL, pub_topic, strlen(preset_pub_message), preset_pub_message /* message */, qos, false);

		sleep(interval);
	}

	// Finish Mosquitto Client
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	fprintf(stderr, "<6>finished ...\n");
	return EXIT_SUCCESS;
}
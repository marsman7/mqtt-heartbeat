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
#include <libconfig.h>
#include <mosquitto.h> // for MQTT funtionallity

/*******************************************/ /**
 * @brief List of Quality of Service levels
 * 
 ***********************************************/
enum qos_t {
	QOS_MOST_ONCE_DELIVERY = 0,
	QOS_LEAST_ONCE_DELIVERY = 1,
	QOS_EXACTLY_ONCE_DELIVERY = 2,
};

#define CONFIG_FILE_NAME "mqtt-heartbeat.conf"

const char *mqtt_broker = "localhost";
int port = 1883;
int interval = 5;
const char *pub_topic = "tele/%hostname%/STATE";
const char *message = "Online"; // "{\"POWER\":\"ON\"}";
const char *sub_topic = "";
int qos = QOS_MOST_ONCE_DELIVERY;

bool run_main_loop = true;
int exit_code = EXIT_SUCCESS;

typedef void (*sighandler_t)(int);

#define errExit(msg)        \
	do {                    \
		perror(msg);        \
		exit(EXIT_FAILURE); \
	} while (0)

/*******************************************/ /**
 * @brief Reads the configuration file or creates it if it is not available
 * 
 * @return int - Result, ZERO at successfully, otherwise -1
 ***********************************************/
int configReader()
{
	fprintf(stderr, "<6>libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	fprintf(stderr, "<6>config file to use : %s\n", CONFIG_FILE_NAME);

	// check exist the .conf file
	// Atributes F_OK, R_OK, W_OK, XOK or "OR" linked R_OK|X_OK
	if (access(CONFIG_FILE_NAME, F_OK))
	{
		FILE *newfile;
		if ( (newfile = fopen(CONFIG_FILE_NAME, "w+")) )
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
					"#sub_topic = \"cmd/%%hostname%%/STATE\"\n"
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
	if (!config_read_file(&cfg, CONFIG_FILE_NAME))
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
	config_lookup_string(&cfg, "pub_topic", &pub_topic);
	config_lookup_int(&cfg, "interval", &interval);
	config_lookup_string(&cfg, "sub_topic", &sub_topic);
	config_lookup_int(&cfg, "QoS", &qos);

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
		if (strlen(sub_topic) > 0 ) {
			// Subscribe to broker information topics on successful connect.
			// mosquitto_subscribe(struct mosquitto *mosq, int *mid, const char *subscribe, int qos);
			fprintf(stderr, "<6>subscribe : %s\n", sub_topic);
			mosquitto_subscribe(mosq, NULL, "mars/#", qos);
		}
	}
	else
	{
		fprintf(stderr, "<3>ERROR : connect to mqtt-broker failed : %s!\n", \
			mosquitto_connack_string(result) );
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
	int keepalive = 30;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	int major, minor, revision;

	fprintf(stderr, "<5>process started [pid - %d] [ppid - %d] ...\n", getpid(), getppid());

	// print the name of this machine
	char localhostname[256] = "\0";
	gethostname(localhostname, sizeof(localhostname) - 1);
	fprintf(stderr, "<6>local machine : %s\n", localhostname);

	// Print version of libmosquitto
	mosquitto_lib_version(&major, &minor, &revision);
	fprintf(stderr, "<6>Mosquitto Version : %d.%d.%d\n", major, minor, revision);


	// read parameter from config file
	int err = configReader();
	if (err)
	{
		fprintf(stderr, "<4>WARNING: config file I/O error, use default settings!\n");
	}
	fprintf(stderr, "<6>configuration processed ...\n");

	// Init Mosquitto Client, required for use libmosquitto
	mosquitto_lib_init();

	// Create a new client instance.
	mosq = mosquitto_new(NULL, clean_session, NULL);
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
		exit_code = EXIT_FAILURE;
		run_main_loop = false;
	}

	if (mosquitto_loop_start(mosq)) 
	{
		fprintf(stderr, "<3>ERROR: unable to connect mqtt-broker %s:%d\n", mqtt_broker, port);
		exit_code = EXIT_FAILURE;
		run_main_loop = false;
	}

	if (exit_code == EXIT_SUCCESS)
	{
		fprintf(stderr, "<5>connected mqtt-broker %s:%d\n", mqtt_broker, port);
	}

	// Main Loop
	while (run_main_loop)
	{
		fprintf(stderr, "<6>sending heartbeat ... %s\n", pub_topic);

		// int mosquitto_publish(struct mosquitto , mid, topic, payloadlen, payload, qos, retain)
		mosquitto_publish(mosq, NULL, pub_topic, strlen(localhostname), localhostname /* message */, qos, false);
		sleep(interval);
	}

	// Finish Mosquitto Client
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	fprintf(stderr, "<6>finished ...\n");
	return exit_code;
}
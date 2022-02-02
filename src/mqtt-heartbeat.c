/***************************************************************************** /
 * mqtt-heartbeat
 *
 * https://mosquitto.org/api/files/mosquitto-h.html
 *
/*****************************************************************************/
#define _GNU_SOURCE

/*******************************************/ /**
 * Header files
 ***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <libconfig.h>
#include <mosquitto.h> // for MQTT funtionallity

/*******************************************/ /**
 * Variables
 ***********************************************/
#define CONFIG_FILE_NAME "mqtt-heartbeat.conf"


const char *server = "localhost";
int port = 1883;
int interval = 5;

const char *topic = "tele/mars/STATE";
const char *message = "Online"; // "{\"POWER\":\"ON\"}";

typedef void (*sighandler_t)(int);

#define errExit(msg)        \
	do                      \
	{                       \
		perror(msg);        \
		exit(EXIT_FAILURE); \
	} while (0)

/*******************************************/ /**
 * Read configuration
 ***********************************************/
int configReader()
{
	fprintf(stderr, "<6>libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	fprintf(stderr, "<6>config file to use : %s\n", CONFIG_FILE_NAME);

	// check exist the .conf file
	// Atributes F_OK, R_OK, W_OK, XOK or "OR" linked R_OK|X_OK
	if (access(CONFIG_FILE_NAME, F_OK))
	{
		FILE *file;
		if (file = fopen(CONFIG_FILE_NAME, "w+"))
		{
			fprintf(file,
					"# Name or IP of the Server\n"
					"# default : localhost\n"
					"#server = \"localhost\"\n"
					"\n"
					"# Server Port\n"
					"# default : 12345\n"
					"#port = 9876\n"
					"\n"
					"# A Value\n"
					"# default : 5\n"
					"#value = 3\n");
			fclose(file);
			fprintf(stderr, "<5>create a new config file\n");
		}
		else
		{
			fprintf(stderr, "<3>can't create a new config file\n");
		}
	}

	config_t cfg;
	config_init(&cfg);

	// Read the file. If there is an error, report it and exit.
	if (!config_read_file(&cfg, CONFIG_FILE_NAME))
	{
		fprintf(stderr, "<4>Config file error : (%d) %s @ %s:%d\n",
				config_error_type(&cfg), config_error_text(&cfg),
				config_error_file(&cfg), config_error_line(&cfg));
		config_destroy(&cfg);
		return EXIT_FAILURE;
	}

	// Get stored settings.
	config_lookup_string(&cfg, "hostname", &server);
	config_lookup_int(&cfg, "port", &port);

	config_destroy(&cfg);

	return EXIT_SUCCESS;
}

/*******************************************/ /**
 * Callback called on incomming message
 ***********************************************/
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
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
 * Callback called on change connection status
 ***********************************************/
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	int i;
	if (!result)
	{
		// Subscribe to broker information topics on successful connect.
		// mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
		mosquitto_subscribe(mosq, NULL, "mars/#", 0);
	}
	else
	{
		//fprintf(stderr, "Connect failed\n");
		fprintf(stderr, "<3>ERROR : connect to mqtt server failed!\n");
	}
}

/*******************************************/ /**
 * Callback called on incomming message
 ***********************************************/
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	fprintf(stderr, "<6>mqtt subscribed : (mid: %d): %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++)
	{
		fprintf(stderr, ", %d", granted_qos[i]);
	}
	fprintf(stderr, "\n");
}

/*******************************************/ /**
 * Callback called after outgoing message
 ***********************************************/
void my_publish_callback(struct mosquitto *mosq, void *userdata, int mid)
{
	//printf("Published (mid: %d)\n", mid);
	fprintf(stderr, "<6>mqtt published : (mid: %d)\n", mid);
}

/*******************************************/ /**
 * Main
 * *********************************************/
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

	// read parameter from config file
	int err = configReader();
	if (err)
	{
		fprintf(stderr, "<4>WARNING: config file I/O error, use default settings!\n");
	}
	fprintf(stderr, "<6>configuration processed ...\n");

	// Init Mosquitto Client
	mosquitto_lib_init();
	mosquitto_lib_version(&major, &minor, &revision);
	//printf("Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	mosq = mosquitto_new(NULL, clean_session, NULL);
	if (!mosq)
	{
		fprintf(stderr, "<3>ERROR: cant't create mosquitto client, out of memory!\n");
		mosquitto_lib_cleanup();
		return EXIT_FAILURE;
	}

	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);

	if (mosquitto_connect(mosq, server, port, keepalive))
	{
		fprintf(stderr, "<3>ERROR: unable to connect mqtt server!\n");
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return EXIT_FAILURE;
	}

	mosquitto_loop_start(mosq);

	// Main Loop
	while (1)
	{
		fprintf(stderr, "<6>sending heartbeat ...");

		mosquitto_publish(mosq, NULL, topic, 8, localhostname /* message */, 0, false);
		sleep(5);
	}

	// Finish Mosquitto Client
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	fprintf(stderr, "<6>finished ...\n");
	return EXIT_SUCCESS;
}
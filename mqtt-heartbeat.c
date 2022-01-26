/***************************************************************************** /
 * mqtt-heartbeat
 *
 * https://mosquitto.org/api/files/mosquitto-h.html
 *
/*****************************************************************************/

/***********************************************
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
#include <mosquitto.h>	// for MQTT funtionallity

/***********************************************
 * Variables
 ***********************************************/
#define CONFIG_FILE_NAME  "mqtt-heartbeat.conf"

const char *host = "localhost";
int port = 1883;
int interval = 5;

const char *topic = "tele/mars/STATE";
const char *message = "Online"; // "{\"POWER\":\"ON\"}";

typedef void (*sighandler_t)(int);

#define errExit(msg) \
		do { \
			perror(msg); \
			exit(EXIT_FAILURE); \
		} while (0)


/***********************************************
 * Read configuration
 ***********************************************/
int configReader() {
	config_t cfg;

	printf("Config file to use : %s\n", CONFIG_FILE_NAME);
	//printf("libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	config_init(&cfg);
	
	// Read the file. If there is an error, report it and exit.
  	if (! config_read_file(&cfg, CONFIG_FILE_NAME) ) {
		fprintf(stderr, "Config file error : (%d) %s @ %s:%d\n", \
				config_error_type(&cfg), config_error_text(&cfg), \
				config_error_file(&cfg), config_error_line(&cfg));
		config_destroy(&cfg);
		return EXIT_FAILURE;	
	}

	// Get stored string.
	if ( config_lookup_string(&cfg, "hostname", &host) ) {
		printf("host : %s\n", host);
	} else {
		printf("use default hostname : %s\n", host);
	}

	if (config_lookup_int(&cfg, "port", &port)) {
		printf("port : %d\n", port);
	} else {
		printf("use default port : %d\n", port);
	}
	
	return EXIT_SUCCESS;
}

/***********************************************
 * Callback called on incomming message
 ***********************************************/
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	if(message->payloadlen){
		printf("%s %s\n", message->topic, (char*)message->payload);
	}else{
		printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}

/***********************************************
 * Callback called
 ***********************************************/
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	int i;
	if(!result){
		/* Subscribe to broker information topics on successful connect. */
		// mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
		mosquitto_subscribe(mosq, NULL, "mars/#", 2);
	}else{
		//fprintf(stderr, "Connect failed\n");
		syslog(LOG_ERR, "ERROR Connect to mqtt server failed!\n");
	}
}

/***********************************************
 * Callback called on incomming message
 ***********************************************/
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

/***********************************************
 * Callback called after outgoing message
 ***********************************************/
void my_publish_callback(struct mosquitto *mosq, void *userdata, int mid)
{
	//printf("Published (mid: %d)\n", mid);
	syslog(LOG_NOTICE, "Message published (mid: %d)\n", mid);
}

/***********************************************
 * 
 ***********************************************/
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	// Pring all log messages regardless of level.
	//printf("Log: %d : %s\n", level, str);
	syslog(LOG_NOTICE, "Log: %d : %s\n", level, str);
}

/***********************************************
 * Main
 * *********************************************/
int main(int argc, char *argv[])
{
	bool run_as_daemon = false;
	int keepalive = 30;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	int major, minor, revision;

	syslog(LOG_INFO, "mqtt-heartbeat gestartet ...\n");

	/*
	printf("Argument count: %d\n", argc);
	for (int i = 0; i < argc; i++) {
		printf("Argument (%d): %s\n", i, argv[i]);
	};
	*/
	int opt = 0;
	char *in_fname = NULL;
	char *out_fname = NULL;

	while ( (opt = getopt(argc, argv, "i:o:d:") ) != -1) {
		switch(opt) {
			case 'i':
				in_fname = optarg;
				printf("Input option value=%s\n", in_fname);
				break;
			case 'o':
				out_fname = optarg;
				printf("Output option value=%s\n", out_fname);
				break;
			case '?':
				/* Case when user enters the command as
				* $ ./cmd_exe -i
				*/
				if (optopt == 'i') {
					printf("Missing mandatory input option\n");
					/* Case when user enters the command as
					* # ./cmd_exe -o
					*/
				} else if (optopt == 'o') {
					printf("Missing mandatory output option\n");
				} else {
					printf("Invalid option received\n");
				}
			break;
		}
	}	

	// print the name of this machine
	char localhostname[256] = "\0";
	gethostname(localhostname, sizeof(localhostname)-1);
	printf("Local machine : %s\n", localhostname);

	// read parameter from config file
	int err = configReader();
	if (err) {
		syslog(LOG_WARNING, "WARNING: Config file I/O, use default settings!\n");
	}
	//printf("Configuration processed ...\n");

	printf("[pid - %d] running...\n", getpid());
	//openlog("mqtt-heartbeat-daemon", LOG_PID | LOG_CONS| LOG_NDELAY, LOG_LOCAL0);

	if (run_as_daemon) {
		// int daemon(int nochdir, int noclose);
		if (daemon(0, 0) == -1) {
			errExit("Daemon");
		}
		//printf("Daemon gestartet ...\n");
		syslog(LOG_NOTICE, "Daemon gestartet ...\n");
	}

	// Init Mosquitto Client
	mosquitto_lib_init();
	mosquitto_lib_version(&major, &minor, &revision);
	//printf("Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	mosq = mosquitto_new(NULL, clean_session, NULL);
	if(!mosq){
		syslog(LOG_ERR, "ERROR: create mosquitto session, out of memory!\n");
		mosquitto_lib_cleanup();
		closelog();
		return EXIT_FAILURE;
	}

	// mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);

	if(mosquitto_connect(mosq, host, port, keepalive)){
		//fprintf(stderr, "Unable to connect.\n");
		syslog(LOG_ERR, "ERROR: Unable to connect mqtt server!\n");
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		closelog();
		return EXIT_FAILURE;
	}

	mosquitto_loop_start(mosq);

	// Main Loop
	while(1)
	{
      	/* Enlosschleifen: Hier sollte nun der Code für den
       	 * Daemon stehen, was immer er auch tun soll.
      	 * Bspw. E-Mails abholen, versenden (was kein 
      	 * Verweis sein soll, um mit dem Spammen anzufangen); 
      	 * Bei Fehlermeldungen beispielsweise:
      	 * if(dies_ist_passiert)
      	 *    syslog(LOG_WARNING, "dies_ist_Passiert");
      	 * else if(das_ist_Passiert)
      	 *    syslog(LOG_INFO, "das_ist_Passiert"); 
       	 */

		if (run_as_daemon) {
			syslog(LOG_NOTICE, "Daemon is running ...");
		} else {
			syslog(LOG_NOTICE, "is running ...");
		}

		mosquitto_publish(mosq, NULL, topic, 8, localhostname /* message */, 0, false);
		sleep(5);
      	//break;
	}

	// Finish Mosquitto Client
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	syslog(LOG_NOTICE, "finished ...\n");
	closelog();
	return EXIT_SUCCESS;
}
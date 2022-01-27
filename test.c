/*
* mqtt-heartbeat
*
* https://mosquitto.org/api/files/mosquitto-h.html
*/

#include <stdio.h>
#include <unistd.h>		// for sleep()
#include <libconfig.h>
#include <mosquitto.h>	// for MQTT funtionallity

// char *host = "homeserver";
const char *host = "localhost";
int port = 1883;
int interval = 5;

const char *topic = "mars/test";

/**
 * @brief 
 * 
 * @return int 
 */
int configHandler() {
	config_t cfg;

	// Init .conf file
	printf("libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	config_init(&cfg);
	
	/* Read the file. If there is an error, report it and exit. */
  	if(! config_read_file(&cfg, "mqtt-heartbeat.conf")) {
		fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return 1;	// (EXIT_FAILURE);	
	}

	/* Get stored string. */
	if(config_lookup_string(&cfg, "hostname", &host)){
		printf("host : %s\n", host);
	} else {
		printf("use default hostname : %s\n", host);
	}

	if (config_lookup_int(&cfg, "port", &port)) {
		printf("port : %d\n", port);
	} else {
		printf("use default port : %d\n", port);
	}
	
	return 0;
}

/**
 * @brief 
 * 
 * @param mosq 
 * @param userdata 
 * @param message 
 */
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
	if(message->payloadlen) {
		printf("%s %s\n", message->topic, (char*)message->payload);
	} else {
		printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}

/**
 * @brief 
 * 
 * @param mosq 
 * @param userdata 
 * @param result 
 */
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	int i;
	if(!result) {
		/* Subscribe to broker information topics on successful connect. */
		mosquitto_subscribe(mosq, NULL, "mars/#", 2);
	} else {
		fprintf(stderr, "Connect failed\n");
	}
}

/**
 * @brief 
 * 
 * @param mosq 
 * @param userdata 
 * @param mid 
 * @param qos_count 
 * @param granted_qos 
 */
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++) {
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

/**
 * @brief 
 * 
 * @param mosq 
 * @param userdata 
 * @param mid 
 */
void my_publish_callback(struct mosquitto *mosq, void *userdata, int mid)
{
	printf("Published (mid: %d)\n", mid);
}

/**
 * @brief 
 * 
 * @param mosq 
 * @param userdata 
 * @param level 
 * @param str 
 */
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	/* Pring all log messages regardless of level. */
	printf("Log: %d : %s\n", level, str);
}

/**
 * @brief 
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char *argv[])
{
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	int major, minor, revision;

	int err = configHandler();
	if (err) {
		return err;
	}

	mosquitto_lib_init();
	mosquitto_lib_version(&major, &minor, &revision);
	printf("Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	mosq = mosquitto_new(NULL, clean_session, NULL);
	if(!mosq) {
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}
	// mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);

	if(mosquitto_connect(mosq, host, port, keepalive)) {
		fprintf(stderr, "Unable to connect.\n");
		return 1;
	}

	// mosquitto_loop_forever(mosq, -1, 1);
	mosquitto_loop_start(mosq);
	while(1) {
		mosquitto_publish(mosq, NULL, "vscode/test", 8, "marsman", 1, false);
		sleep(interval);
	}

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	return 0;
}
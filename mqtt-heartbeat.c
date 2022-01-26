/***************************************************************************** /
 * mqtt-heartbeat
 *
 * https://mosquitto.org/api/files/mosquitto-h.html
 *
 *
 * ToDo :
 * - Run as Linux service 
 *     https://github.com/pasce/daemon-skeleton-linux-c
 *     https://openbook.rheinwerk-verlag.de/linux_unix_programmierung/Kap07-011.htm#RxxKap07011040002021F048100
 *     https://www.freedesktop.org/software/systemd/man/daemon.html#New-Style%20Daemons
 * - Config file using
 * - Publish uptime
 *    https://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime
 * - MQTT LWT (Last Will Testament)
 *     int mosquitto_will_set(struct mosquitto *mosq, const char *topic, int payloadlen, const void *payload, int qos, bool retain);
 *     Topic : tele/xxx/LWT Online
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
#include <stdbool.h>
#include <libconfig.h>
#include <mosquitto.h>	// for MQTT funtionallity

/***********************************************
 * Variables
 ***********************************************/
#define CONFIG_FILE_NAME  "x_mqtt-heartbeat.conf"

const char *host = "localhost";
int port = 1883;
int interval = 5;

const char *topic = "tele/mars/STATE";
const char *message = "Online"; // "{\"POWER\":\"ON\"}";

typedef void (*sighandler_t)(int);

/***********************************************
 * Read configuration
 ***********************************************/
int configReader() {
	config_t cfg;

	printf("libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	config_init(&cfg);
	
	// Read the file. If there is an error, report it and exit.
  	if (! config_read_file(&cfg, CONFIG_FILE_NAME ) ) {
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
 * OS signal handler
 ***********************************************/
static sighandler_t handle_signal (int sig_nr, sighandler_t signalhandler) {
	struct sigaction neu_sig, alt_sig;

	neu_sig.sa_handler = signalhandler;
	sigemptyset (&neu_sig.sa_mask);
	neu_sig.sa_flags = SA_RESTART;
	if (sigaction (sig_nr, &neu_sig, &alt_sig) < 0) {
		return SIG_ERR;
	}
	return alt_sig.sa_handler;
}

/***********************************************
 * 
 ***********************************************/
static void start_daemon (const char *log_name, int facility) {
	int i;
	pid_t pid;

	// Elternprozess beenden, somit haben wir einen Waisen
	// dessen sich jetzt vorerst init annimmt
	if ( (pid = fork() ) != 0) {
		exit(EXIT_FAILURE);
	}
	
	// Kindprozess wird zum Sessionführer. Misslingt
	// dies, kann der Fehler daran liegen, dass der
	// Prozess bereits Sessionführer ist
	if ( setsid() < 0 ) {
		printf("%s kann nicht Sessionführer werden!\n",	log_name);
		exit(EXIT_FAILURE);
	}
	// Signal SIGHUP ignorieren
	handle_signal (SIGHUP, SIG_IGN);

	// Oder einfach: signal(SIGHUP, SIG_IGN) ...
	// Das Kind terminieren
	if ((pid = fork ()) != 0)
		exit(EXIT_FAILURE);

	// Gründe für das Arbeitsverzeichnis:
	// + core-Datei wird im aktuellen Arbeitsverzeichnis hinterlegt.
	// + Damit bei Beendigung mit umount das Dateisystem 
	//   sicher abgehängt werden kann
	chdir("/");

	// Damit wir nicht die Bitmaske vom Elternprozess
	// erben bzw. diese bleibt, stellen wir diese auf 0
	umask(0);

	// Wir schließen alle geöffneten Filedeskriptoren ...
	for (i = sysconf (_SC_OPEN_MAX); i > 0; i--)
		close(i);

	// Da unser Dämonprozess selbst kein Terminal für
	// die Ausgabe hat....
	openlog(log_name, LOG_PID | LOG_CONS| LOG_NDELAY, facility);
}

/***********************************************
 * 
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
 * 
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
	int keepalive = 30;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	int major, minor, revision;

	syslog(LOG_INFO, "mqtt-heartbeat gestartet ...\n");

	// print the name of this machine
	char localhostname[256] = "\0";
	gethostname(localhostname, sizeof(localhostname)-1);
	printf("Local machine : %s\n", localhostname);

	// read parameter from config file
	int err = configReader();
	if (err) {
		syslog(LOG_WARNING, "WARNING Config file I/O, use default settings!\n");
	}

	start_daemon ("mqtt-heartbeat-daemon", LOG_LOCAL0);
	syslog(LOG_NOTICE, "Daemon gestartet ...\n");

	// Init Mosquitto Client
	mosquitto_lib_init();
	mosquitto_lib_version(&major, &minor, &revision);
	//printf("Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	mosq = mosquitto_new(NULL, clean_session, NULL);
	if(!mosq){
		//fprintf(stderr, "Error: Out of memory.\n");
		syslog(LOG_ERR, "ERROR create mosquitto session!\n");
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
		syslog(LOG_ERR, "ERROR Unable to connect mqtt server!\n");
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
      	//syslog(LOG_NOTICE, "Daemon läuft bereits %d Sekunden\n", time );
      	//break;

		mosquitto_publish(mosq, NULL, topic, 8, localhostname /* message */, 0, false);

		sleep(5);
	}

	// Finish Mosquitto Client
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	syslog(LOG_NOTICE, "Daemon closed ...\n");
	closelog();
	return EXIT_SUCCESS;
}
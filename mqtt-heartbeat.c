/*
* mqtt-heartbeat
*
* https://mosquitto.org/api/files/mosquitto-h.html
*
* ToDo :
*	- Run as Linux service 
*		(https://github.com/pasce/daemon-skeleton-linux-c)
*		(https://openbook.rheinwerk-verlag.de/linux_unix_programmierung/Kap07-011.htm#RxxKap07011040002021F048100)
*	- Publish uptime (https://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <mosquitto.h>	// for MQTT funtionallity

typedef void (*sighandler_t)(int);

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

static void start_daemon (const char *log_name, int facility) {
	int i;
	pid_t pid;

	/* Elternprozess beenden, somit haben wir einen Waisen */
	/* dessen sich jetzt vorerst init annimmt              */
	if ((pid = fork ()) != 0) {
		exit (EXIT_FAILURE);
	}
	
	/* Kindprozess wird zum Sessionführer. Misslingt */
	/* dies, kann der Fehler daran liegen, dass der     */
	/* Prozess bereits Sessionführer ist               */
	if (setsid() < 0) {
		printf("%s kann nicht Sessionführer werden!\n",	log_name);
		exit (EXIT_FAILURE);
	}
	/* Signal SIGHUP ignorieren */
	handle_signal (SIGHUP, SIG_IGN);

	/* Oder einfach: signal(SIGHUP, SIG_IGN) ...
	 * Das Kind terminieren */
	if ((pid = fork ()) != 0)
		exit (EXIT_FAILURE);

	/* Gründe für das Arbeitsverzeichnis:
	 * + core-Datei wird im aktuellen Arbeitsverzeichnis 
	 *   hinterlegt.
	 * + Damit bei Beendigung mit umount das Dateisystem 
	 *   sicher abgehängt werden kann 	 */
	chdir ("/");

	/* Damit wir nicht die Bitmaske vom Elternprozess
	 * erben bzw. diese bleibt, stellen wir diese auf 0  */
	umask (0);

	/* Wir schließen alle geöffneten Filedeskriptoren ... */
	for (i = sysconf (_SC_OPEN_MAX); i > 0; i--)
		close (i);

	/* Da unser Dämonprozess selbst kein Terminal für
	 * die Ausgabe hat....                            */
	openlog ( log_name, LOG_PID | LOG_CONS| LOG_NDELAY, facility );
}

void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	if(message->payloadlen){
		printf("%s %s\n", message->topic, (char*)message->payload);
	}else{
		printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	int i;
	if(!result){
		/* Subscribe to broker information topics on successful connect. */
		// mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
		mosquitto_subscribe(mosq, NULL, "mars/#", 2);
	}else{
		fprintf(stderr, "Connect failed\n");
	}
}

void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

void my_publish_callback(struct mosquitto *mosq, void *userdata, int mid)
{
	printf("Published (mid: %d)\n", mid);
}

void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	/* Pring all log messages regardless of level. */
	printf("Log: %d : %s\n", level, str);
}

int main(int argc, char *argv[])
{
	pid_t pid;
	
	int i;
	char *host = "homeserver";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	int major, minor, revision;

	start_daemon ("mqtt-heartbeat-daemon", LOG_LOCAL0);
	syslog( LOG_NOTICE, "Daemon gestartet ...\n");
/*
    // Init Service
	pid = fork();
    if (pid < 0) {
	    // An error occurred
		syslog(LOG_NOTICE, "process not created!\n");
        exit(EXIT_FAILURE);
	}

    if (pid > 0) {
	    // Success: Let the parent terminate
        exit(EXIT_SUCCESS);
	}
*/

	// Init Mosquitto Client
	mosquitto_lib_init();
	mosquitto_lib_version(&major, &minor, &revision);
	printf("Mosquitto Version : %d.%d.%d\n", major, minor, revision);

	mosq = mosquitto_new(NULL, clean_session, NULL);
	if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}

	// mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);

	if(mosquitto_connect(mosq, host, port, keepalive)){
		fprintf(stderr, "Unable to connect.\n");
		return 1;
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

		mosquitto_publish(mosq, NULL, "vscode/test", 8, "marsman", 0, false);

		sleep(5);
	}

	// Finish Mosquitto Client
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	syslog(LOG_NOTICE, "Daemon closed\n");
	closelog();
	return EXIT_SUCCESS;
}
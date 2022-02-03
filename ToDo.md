* Run as Linux service 
* ~~Create, read and process config file~~
* Publish uptime
    - https://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime
    - sysinfo() uptime
* Send message on terminate ( ? atexit() )
* MQTT LWT (Last Will Testament)
    - int mosquitto_will_set(struct mosquitto *mosq, const char *topic, int payloadlen, const void *payload, int qos, bool retain);
    - Topic : tele/xxx/LWT Online
* Signalhandler
* Service nur einmal starten
* ~~Unterstütung von makefile ("make" , "make install" , "make uninstell")~~
* ~~Doxygen unterstützen https://www.selflinux.org/selflinux/html/doxygen01.html~~
* Linux Man Pages
* Variables in Topic
* Topic subscribe to shut down the machine
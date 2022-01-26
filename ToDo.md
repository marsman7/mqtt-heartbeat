* Run as Linux service 
* Config file using
* Publish uptime
* MQTT LWT (Last Will Testament)
    - int mosquitto_will_set(struct mosquitto *mosq, const char *topic, int payloadlen, const void *payload, int qos, bool retain);
    - Topic : tele/xxx/LWT Online
* Service nur einmal starten
* Unterstütung von makefile ("make" , "make install")
* ~~Directory of config file~~
* ~~Run as Linux service per Simple-service~~
* ~~Create, read and process config file~~
* ~~Send message on terminate ( ? atexit() )~~
* ~~MQTT LWT (Last Will Testament)~~
* ~~Signalhandler~~
* ~~Service nur einmal starten~~
* ~~Unterstütung von makefile ("make" , "make install" , "make uninstell")~~
* ~~Variables in Topic~~
* ~~Doxygen unterstützen~~
* ~~SIGHUP - Config neu laden~~
* Linux Man Pages
* ~~Log-level adjustable~~
* Username and password for mqtt connection
    - `mosquitto_username_pw_set(struct mosquitto *mosq, const char *username, const char *password);`
* Publish status / telemetry
    - ~~stat/# and tele/#~~
    - ~~On Status~~
    - ~~hostname~~
    - sysinfo() uptime : https://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime
    - ~~CPU load~~
    - RAM : status
    - HDD : status
    - status of a service : `system("systemctl is-active <service>")`
* ~~Topic subscribe to shut down the machine~~
* ~~make install not overwrite the config file~~
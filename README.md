# MQTT-Heartbeat

MQTT-Heartbeat is a Linux service that periodically sends a status message via MQTT. 

## Dependencys

The following Librarys must be installed for use:

* libconfig9
* libmosquitto1

The following Librarys must be installed for development:

* libconfig-dev
* libmosquitto-dev


## Compilation and installation

>make
>make install

## Run as a normal process in a terminal

>./mqtt-heartbeat

## Run as a service daemon



>`sudo systemctl start mqtt-heartbeat`

>`sudo systemctl enable mqtt-heartbeat`  


## Diagnostic

Prints the syslog output from task "mqtt-heartbeat"

>`journalctl -f -t mqtt-heartbeat`

## Weblinks

#### Publish uptime

* https://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime

sysinfo() uptime

## About

Marsmans webpage :joy: : [martinsuniverse.de](https://martinsuniverse.de)

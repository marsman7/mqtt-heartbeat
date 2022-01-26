# MQTT-Heartbeat

MQTT-Heartbeat is a Linux service that periodically sends a status message via MQTT. 

Änderung BE
MQTT-Heartbeat is a Linux service that periodically sends a status message via MQTT.

## Dependencys

The following Librarys must be installed :<br>
* libconfig9
* libconfig-dev
* libmosquitto1
* libmosquitto-dev


## Installation

Copy the control script  

>`sudo mv mqtt-heartbeat.sh /etc/init.d/`

and make it executable

>`sudo chmod u+x /etc/init.d/mqtt-heartbeat.sh`

Copy the compiled binary file

>`sudo cp bin/mqtt-heartbeat /usr/sbin/`


## Start the service

>`sudo systemctl enable mqtt-heartbeat`
>`sudo systemctl start mqtt-heartbeat`


## Diagnostic

Prints the syslog output from task "mqtt-heartbeat"

>`journalctl -f -t mqtt-heartbeat`

Filter syslog my message pattern "error"

>`journalctl -f -g error --case-sensitive=false`


## About

Marsmans webpage :joy: : [martinsuniverse.de](https://martinsuniverse.de)

Markdown guide : https://www.markdownguide.org/cheat-sheet/

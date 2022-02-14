# MQTT-Heartbeat

[Deutsche Version dieses Dokuments](README_DE.md)

**@todo #4 must be revised**


MQTT-Heartbeat is a Linux daemon that periodically sends status 
and telemetry messages via MQTT. 

## Dependencys

The following Librarys must be installed for use:

* libconfig9
* libmosquitto1

The following Librarys must be installed for development:

* libconfig-dev
* libmosquitto-dev


## Compilation and installation

>`make`

>`sudo make install`

## Run as a normal process in a terminal

>`./mqtt-heartbeat`

### Command line arguments

| Option | Discription |
|-----|-----|
| `-c<file>` | Use the given configuration file |
| `-u` | Unlink the lock socket that prevent multible instances |

## Run as a daemon

Run the daemon once:
>`sudo systemctl start mqtt-heartbeat`

Configure the daemon to start it as system startup:
>`sudo systemctl enable mqtt-heartbeat`  

## Diagnostic

Prints the syslog output from task "mqtt-heartbeat"

>`journalctl -f -t mqtt-heartbeat`

## MQTT Nachrichten

Subscrib - eingehende MQTT Kommandos
>`cmnd/foobar/POWER1 <command>`

Mögliche Kommandos sind :
* `OFF` - Fährt den Rechner runter
* `REBOOT` - Starten den Rechner neu

Publish - ausgehende MQTT Nachrichten 
>`stat/foobar/RESULT {"POWER1":"OFF"}`
>`stat/foobar/POWER1 OFF`

>`tele/foobar/STATE {"POWER1":"OFF","UptimeSec":459441,"Heap":28,"Sleep":50,"LoadAvg":19,"MqttCount":20}`

## Konfiguration


## Weblinks

Mosquitto API : (https://mosquitto.org/api/files/mosquitto-h.html)

## About

Marsmans webpage :joy: : [martinsuniverse.de](https://martinsuniverse.de)

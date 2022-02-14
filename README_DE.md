# MQTT-Heartbeat (Deutsch)

[English version of this document](README.md)

MQTT-Heartbeat ist ein Linux-Dämon der periodisch Status und Telementry Nachrichten per MQTT sendet. 

Intension für diese Programm war mein Smart-Home. Über die Server, in meinem Fall ein Raspi mit Node-Red, werden andere Raspis und Linux-PCs gestart aber der Server hat keine Rückmeldung bekommen ob der Rechner gestartet ist oder nicht. Das erledigt nun dieser kleine Dämon. Ich habe ihn sowohl auf einem Raspi als auch auf einem Linux-PC getestet.


## Dependencys

Die folgenden Bibliotheken müssen installiert sein damit der Dämon läuft :

* libconfig9
* libmosquitto1

Die folgenden Bibliotheken müssen installiert sein um das Programm kompilieren zu können :

* libconfig-dev
* libmosquitto-dev

Zum Kompilieren sollte vorher das System Aktuell sein und einige Programme wie folgt installiert werden :

>`sudo apt update && sudo apt upgrade`

>`sudo apt install build-essential libconfig9 libconfig-dev libmosquitto1 libmosquitto-dev`


## Kompilieren und installieren

>`make`

>`sudo make install`


## Start als normaler Prozess in einem Terminal

Das Programm muss hierfür nicht mit `make install` installiert werden.

>`./mqtt-heartbeat`

So gestartet wird standardmäßig die Konfiguration im selben Verzeichnis verwendet. Alle Info-, Debug-, Warnungs- und Fehlerausgaben erfolgen im Terminal.  Wurde das Programm unsauber beendet kann es vorkommen, dass noch ein offener Socket vorhanden ist. Dann kann das Programm nicht gestartet werden. In diesem Fall kann der Socket mit dem Parameter "-u" geschlossen werden.


### Komandozeilen Parameter

| Option | Beschreibung |
|-----|-----|
| `-c<file>` | Die angegebene Konfigurationsdatei verwenden |
| `-u` | Unlink des Socket the lock socket that prevent multible instances |


## Starten als Dämon

Starten des Dämons :

>`sudo systemctl start mqtt-heartbeat`

Nach einem Systemneustart wird der Dämon so aber nicht gestartet. Dies kann mit dem folgenden Befehl erledigt werden :

>`sudo systemctl enable mqtt-heartbeat`  

Wurde die Konfiguration geändert muss der Dämon sie neu laden :

>`sudo systemctl reload mqtt-heartbeat`

Weitere Befehle zum steuren des Dämons sind :

>`sudo systemctl disable mqtt-heartbeat`
>`sudo systemctl stop mqtt-heartbeat`
>`sudo systemctl restart mqtt-heartbeat`


## Diagnostic

Der aktuelle Status wird mit :

>`sudo systemctl status mqtt-heartbeat`

angezeigt. Alle Info-, Debug-, Warnungs- und Fehlerausgaben werden in das Journal von **systemd** geschrieben. Das Journal kann mit folgendem Befehl angeschaut werden :

>`journalctl -f -t mqtt-heartbeat`

## MQTT Nachrichten

Der Rechner kann per eingehende (Subscrib) MQTT Kommandos gesteuert werden. Der MQTT-Topic kann in der Konfigurationsdatei angegeben werden aber es wird nur "cmnd" an erster Stelle und "POWER1" an dritter Stelle erwartet damit das Kommando ausgeführt wird.
>`cmnd/foobar/POWER1 <command>`

Mögliche Kommandos sind :
* `OFF` - Fährt den Rechner runter
* `TOGGLE` - Entspricht in der Praxit `OFF`
* `REBOOT` - Starten den Rechner neu

Das Programm bietet zwei Typen von Nachrichten die in unterschiedlichen Zeitabständen gesendet (Publish) werden können. Die MQTT-Topics und die Inhalte (Payloads) der Nachrichten können in der Konfigurationsdatei eingestellt werden wie zum Biespiel :

>`stat/foobar/POWER1 OFF`
>`tele/foobar/STATE {"POWER1":"OFF"}`

In der Konfigurationsdatei wird das Topic und der Nachrichtentext getrennt eingestellt. Es können auch einige variablen Werte angegeben werden, zum Beispiel :

>`{"POWER1": "%status%", "HOSTNAME": "%hostname%", "USER": "%user%"}`

Die Variablennamen müssen in '%'-Zeichen eingefasst sein. 

Folgende Variablen sind möglich :
Name | Beschreibung
----- | -----
%hostname% | The name of this machine
%user% | The user that run this daemon
%status% | Status of this machine ('On' or 'Off')
%loadavg_1% | Average CPU load of last 1 minute
%uptime% | Time since the start of the machine in seconds
%ramfree% | Free RAM space in percent
%diskfree_mb% | Free disk space in mega byte
%service_<serice_name>% | Status of a spezified service ('active' or 'inactive')

Weitere Formatierungs Vorgaben folgen unter 'Konfiguration'

## Konfiguration

In der Konfigurationsdatei werden alle Zeilen, die mit einem '#'-Zeichen anfangen als Kommentar gewerten und haben damit keine Funktion. 

Eine Optionszeile beginnt mit dem Namen der Option gefolgt von einem '='-Zeichen und dem Wert der Option. Für einen Zahlenwert sieht ein Beispiel so aus :

>`foobar = 123`

oder für eine Text-Option muss der Wert in Anführungszeichen stehen :

>`foo = "bar"`

Sollen in einem Text Anführungszeichen enthalten sein muss dem Anführungszeichen ein Backslash '\' voran gestellt werden.

>`foobar = "foo \"bar\" foo"`

Diese Beispiel würde also folgendes senden :

>`foo "bar" foo`


## Weblinks

Mosquitto API : (https://mosquitto.org/api/files/mosquitto-h.html)

## About

Marsmans webpage :joy: : [martinsuniverse.de](https://martinsuniverse.de)

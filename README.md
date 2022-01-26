# MQTT-Heartbeat

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

Nur ein Merker ...

>`$ sudo make install`<br>
>`...`<br>
>`/bin/mkdir -p '/lib/systemd/system'`<br>
>`/usr/bin/install -c -m 644 foo.service "/lib/systemd/system"`<br>
>`...`

## Start the service

>`sudo systemctl enable mqtt-heartbeat`  
>`sudo systemctl start mqtt-heartbeat`


## Diagnostic

Prints the syslog output from task "mqtt-heartbeat"

>`journalctl -f -t mqtt-heartbeat`

Filter syslog my message pattern "error"

>`journalctl -f -g error --case-sensitive=false`

## Weblinks

Linux service Infos

* https://www.delftstack.com/de/howto/c/ac-daemon-in-c/
* https://lloydrochester.com/post/c/unix-daemon-example/
* https://manpages.debian.org/unstable/manpages-de-dev/daemon.3.de.html
* https://www.man7.org/linux/man-pages/man3/daemon.3.html
* https://manpages.debian.org/testing/manpages-de/daemon.7.de.html
* https://manpages.debian.org/testing/manpages-de/systemd.service.5.de.html
* https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux
    * https://www.apt-browse.org/browse/debian/wheezy/main/amd64/initscripts/2.88dsf-41+deb7u1/file/etc/init.d/skeleton
    * https://github.com/pasce/daemon-skeleton-linux-c


Publish uptime

* https://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime

Parse Command Line Arguments

* https://manpages.debian.org/unstable/manpages-de-dev/getopt.3.de.html


## About

Marsmans webpage :joy: : [martinsuniverse.de](https://martinsuniverse.de)

Markdown guide : https://www.markdownguide.org/cheat-sheet/

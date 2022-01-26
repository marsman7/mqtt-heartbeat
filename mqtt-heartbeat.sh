#!/bin/sh
# Systemservice control script
# Name : mqtt-heartbeat.sh

DAEMON="/usr/sbin/mqtt-heartbeat"
test -f $DAEMON || exit 0
case "$1" in
   start)
        echo -n "Starte mqtt-heartbeat"
        $DAEMON
        echo "....[DONE]"
        ;;
    stop)
        echo -n "Stoppe mqtt-heartbeat"
        killall mqtt-heartbeat
        echo "....[DONE]"
        ;;
    restart)
        $0 stop
        $0 start
        echo "....[DONE]"
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
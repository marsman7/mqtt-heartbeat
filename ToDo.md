* Run as Linux service 
* Directory of config file
* ~~Create, read and process config file~~
* Publish status / telemetry
    - sysinfo() uptime : https://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime
    - getloadavg()
    - memory statistic
* ~~Send message on terminate ( ? atexit() )~~
* MQTT LWT (Last Will Testament)
    - int mosquitto_will_set(struct mosquitto *mosq, const char *topic, int payloadlen, const void *payload, int qos, bool retain);
    - Topic : tele/xxx/LWT Online
* ~~Signalhandler~~
* Service nur einmal starten
    - Check and create PID-file
    - Lock-file in /run/lock : https://cpp.hotexamples.com/de/examples/-/-/lockf/cpp-lockf-function-examples.html
* ~~Unterstütung von makefile ("make" , "make install" , "make uninstell")~~
* ~~Doxygen unterstützen https://www.selflinux.org/selflinux/html/doxygen01.html~~
* Linux Man Pages
* Log-level adjustable
* ~~Variables in Topic~~
* Topic subscribe to shut down the machine
    - https://man7.org/linux/man-pages/man2/reboot.2.html
    - ```c
      #include <unistd.h>  
      #include <linux/reboot.h>
      #include <sys/reboot.h>
       
      int main() {
        sync();
        reboot(LINUX_REBOOT_CMD_POWER_OFF);
      }
      ```
    -
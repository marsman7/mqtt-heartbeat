#include <stddef.h>

/*******************************************/ /**
 * @brief Quality of Service levels list
 ***********************************************/
enum qos_t
{
	QOS_MOST_ONCE_DELIVERY = 0,
	QOS_LEAST_ONCE_DELIVERY = 1,
	QOS_EXACTLY_ONCE_DELIVERY = 2
};

enum stat_t
{
    STAT_OFF = 0,
    STAT_ON = 1
};

#define MAJOR       1
#define MINOR       0
#define REVISION    1
#define BUILD       0

const char *lock_socket_name = "/tmp/mqtt-heartbeat";

const char *system_config_dir = "/etc/";
const char *config_file_ext = ".conf";
const char *err_register_sigaction = "ERROR : Registering the signal handler fail!\n";
const char *err_out_of_memory = "ERROR : Out of memory!\n";
const char *status_off_string = "OFF";
const char *status_on_string = "ON";
const char *shutdown_poweroff = "--poweroff";
const char *shutdown_reboot = "--reboot";

/*******************************************/ /**
 * @brief Presets if options not found in config file
 ***********************************************/
int log_level = 6;
int preset_log_level = 6;  // Logging before read config use ever this level
int port = 0;
int preset_port = 1883;
int qos = 0;
int preset_qos = QOS_MOST_ONCE_DELIVERY;
int shutdown_delay = 0;
int preset_shutdown_delay = 0;

char *mqtt_broker = NULL;
const char *preset_mqtt_broker = "localhost";
char *broker_user = NULL;
const char *preset_broker_user = "\0";
char *broker_password = NULL;
const char *preset_broker_password = "\0";


int stat_interval = 0;
int preset_stat_interval = 5;
char *stat_pub_topic = NULL;
const char *preset_stat_pub_topic = "stat/\%hostname\%/POWER1";
char *stat_pub_message = NULL;
const char *preset_stat_pub_message = "\%status\%";

char *pub_terminate_message = NULL;
const char *preset_pub_terminate_message ="\%status\%";

int tele_interval = 0;
int preset_tele_interval = 60;
char *tele_pub_topic = NULL;
const char *preset_tele_pub_topic = "tele/\%hostname\%/STATE";
char *tele_pub_message = NULL;
const char *preset_tele_pub_message = "{\"POWER1\":\"\%status\%\"}";

char *sub_topic = NULL;
const char *preset_sub_topic = "\0";    // "cmnd/\%hostname\%/POWER1";

char *last_will_topic = NULL;
const char *preset_last_will_topic = "tele/\%hostname\%/LWT";
char *last_will_message = NULL;
const char *preset_last_will_message = "Offline";

/*******************************************/ /**
 * @brief The configuration file string used by 
 *        creating config file
 ***********************************************/
const char preset_config_file[] = 
    "# See Github for the example configuration\n";

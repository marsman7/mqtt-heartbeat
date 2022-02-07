#include <stddef.h>

/*******************************************/ /**
 * @brief Quality of Service levels list
 ***********************************************/
enum qos_t
{
	QOS_MOST_ONCE_DELIVERY = 0,
	QOS_LEAST_ONCE_DELIVERY = 1,
	QOS_EXACTLY_ONCE_DELIVERY = 2,
};


//const char *config_file_name = "mqtt-heartbeat.conf";
const char *config_file_ext = ".conf";
const char *err_register_sigaction = "ERROR : Registering the signal handler fail!\n";
const char *err_out_of_memory = "ERROR : Out of memory!\n";

/*******************************************/ /**
 * @brief Presets if options not found in config file
 ***********************************************/
const char *mqtt_broker = "localhost";
int port = 1883;
int interval = 5;
int qos = QOS_MOST_ONCE_DELIVERY;
char *pub_topic = NULL;
//const char *preset_pub_topic = "tele/\%hostname\%/STATE";
char *pub_message = NULL;
const char *preset_pub_message = "{\"POWER\":\"ON\"}";
char *pub_terminate_message = NULL;
const char *preset_pub_terminate_message = "{\"POWER\":\"OFF\"}";
char *sub_topic = NULL;
const char *preset_sub_topic = "cmnd/\%hostname\%/POWER";
char *last_will_topic = NULL;
const char *preset_last_will_topic = "tele/\%hostname\%/LWT";
char *last_will_message = NULL;
const char *preset_last_will_message = "Offline";

/*******************************************/ /**
 * @brief The configuration file string used by 
 *        creating of the config file
 ***********************************************/
const char preset_config_file[] = 
    "# Name or IP of the MQTT-broker\n"
    "# default : localhost\n"
    "#broker = \"localhost\"\n"
    "\n"
    "# Broker port\n"
    "# default : 1883\n"
    "#port = 1883\n"
    "\n"
    "# The topic of published messages\n"
    "# default : \"tele/%%hostname%%/STATE\"\n"
    "#pub_topic = \"footele/%%hostname%%/STATE\"\n"
    "\n"
    "# Interval of sending status message in seconds\n"
    "# default : 5\n"
    "#interval = 5\n"
    "\n"
    "# The topic of subscribe messages\n"
    "# default : none\n"
    "#preset_sub_topic = \"cmd/%%hostname%%/STATE\"\n"
    "\n"
    "# Quality of Service Indicator Value 0, 1 or 2 to be used for the will\n"
    "# QoS 0: At most once delivery\n"
    "# QoS 1: At least once delivery\n"
    "# QoS 2: Exactly once delivery\n"
    "# default : 0\n"
    "#QoS = 0\n"
    "\n";

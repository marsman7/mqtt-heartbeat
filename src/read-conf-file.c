/********************************************//**
 * @file read-conf-file.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-02-01
 * 
 * @copyright Copyright (c) 2022
 * 
 * @todo
 * 
 * https://www.delftstack.com/de/howto/c/c-check-if-file-exists/
 * https://www.cplusplus.com/reference/cstring/strtok/
 * https://gist.github.com/ajithbh/8154257
 * 
 ***********************************************/

/*******************************************//**
 * Header files
 ***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <libconfig.h>

#include <sys/sysinfo.h>
int sysinfo(struct sysinfo *info);

/*******************************************//**
 * Variables
 ***********************************************/
#define CONFIG_FILE_NAME  __BASE_FILE__".conf"
#define CONFIG_FILE_EXT   ".conf"

const char *server = "localhost";
int port = 1883;

/********************************************//**
 * @brief 
 * 
 * @param myStr 
 * @param extSep 
 * @param pathSep 
 * @return char* 
 ***********************************************/
char *remove_ext (char* myStr, char extSep, char pathSep) {
    char *retStr, *lastExt, *lastPath;

    // Error checks and allocate string.
    if (myStr == NULL) return NULL;
    if ((retStr = malloc (strlen (myStr) + 1)) == NULL) return NULL;

    // Make a copy and find the relevant characters.
    strcpy (retStr, myStr);
    lastExt = strrchr (retStr, extSep);
    lastPath = (pathSep == 0) ? NULL : strrchr (retStr, pathSep);

    // If it has an extension separator.
    if (lastExt != NULL) {
        // and it's to the right of the path separator.
        if (lastPath != NULL) {
            if (lastPath < lastExt) {
                // then remove it.
                *lastExt = '\0';
            }
        } else {
            // Has extension separator with no path separator.
            *lastExt = '\0';
        }
    }
    // Return the modified string.
    return retStr;
}


/********************************************//**
 * @brief Create a Config object
 * 
 ***********************************************/
void createConfig() {
   /* try to open file to read */
   FILE *file;
   if (file = fopen("a.txt", "r")) {
      fclose(file);
      printf("file exists");
   } else {
      printf("file doesn't exist");
   }
}

/********************************************//**
 * @brief 
 * 
 * @return int 
 ***********************************************/
int readConfig() {
	printf("libconfig Version : %d.%d.%d\n", LIBCONFIG_VER_MAJOR, LIBCONFIG_VER_MINOR, LIBCONFIG_VER_REVISION);
	printf("Config file to use : %s\n", CONFIG_FILE_NAME);
	
    struct sysinfo info;
  
    if (sysinfo(&info) < 0)
        return 0;
    
    info.freeram;
  
    printf ("----------------------------------------------\n");
    char str[] ="/This/is/a/sample.file.name";
//    char str[] ="This/is/a/sample.file.name/";
    char *pch;
    printf ("Splitting string \"%s\" into tokens:\n",str);
    pch = strtok (str,"/.");
    while (pch != NULL)
    {
        printf ("%s\n",pch);
        pch = strtok (NULL, "/.");
    }
    printf ("----------------------------------------------\n");

    printf ("[%s]\n", remove_ext(str, '.', '/'));

    printf ("----------------------------------------------\n");

    // Atributes F_OK, R_OK, W_OK, XOK or "OR" linked R_OK|X_OK
    if( access( "test.conf", F_OK ) != -1) {
        printf("file exists\n");
    } else {
        printf("file not found\n");
    }

	config_t cfg;
    config_init(&cfg);
	
	// Read the file. If there is an error, report it and exit.
  	if (! config_read_file(&cfg, CONFIG_FILE_NAME) ) {
		fprintf(stderr, "Config file error : (%d) %s @ %s:%d\n", \
				config_error_type(&cfg), config_error_text(&cfg), \
				config_error_file(&cfg), config_error_line(&cfg));
		config_destroy(&cfg);
		return EXIT_FAILURE;	
	}

	// Get stored settings.
	config_lookup_string(&cfg, "hostname", &server);
	config_lookup_int(&cfg, "port", &port);
	
	return EXIT_SUCCESS;
}

/*******************************************//**
 * Main
 * *********************************************/
int main(int argc, char *argv[])
{
	printf("%s start [pid - %d] [ppid - %d] ...\n", __BASE_FILE__, getpid(), getppid());

    //printf("%s\n", strrchr (__BASE_FILE__, '/'));
    // char * strrchr (       char * str, int character );

	// print the name of this machine
	char localhostname[256] = "\0";
	gethostname(localhostname, sizeof(localhostname)-1);
	printf("Local machine : %s\n", localhostname);

	// read parameter from config file
	int err = readConfig();
	if (err) {
		printf("WARNING: Config file I/O, use default settings!\n");
	}

	printf("Configuration processed ...\n");
    printf("Servername : %s\n", server);
    printf("Serverport : %d\n", port);
    printf("\n");

	return EXIT_SUCCESS;
}
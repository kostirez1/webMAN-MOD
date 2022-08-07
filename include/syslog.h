#include <stdlib.h>

#define SYSLOG_MAX_LENGTH 1024 // maximum message length according to RFC3164

int gSyslog_socket = 0;

static int syslog_send(int facility, int severity, char *process_id, char *message){
    int priority = facility * 8 + severity;
    char hostname = "PS3";

    char *buffer = (char *) malloc(SYSLOG_MAX_LENGTH + 1);
    
    snprintf(buffer, SYSLOG_MAX_LENGTH + 1, "<%i> %s %s %s", priority, hostname, process_id, message);
    send(gSyslog_socket, buffer, strlen(buffer), 0);
    free(buffer);
}
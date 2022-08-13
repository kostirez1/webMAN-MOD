#include <stdlib.h>
#include <sys/syscall.h>

static void play_rco_sound(const char *sound);

#define SYSLOG_MAX_LENGTH 1024 // maximum message length according to RFC3164

int gSyslog_socket = 0;

static bool syslog_current_datetime(char *output, int len){
    CellRtcDateTime clk;
    cellRtcGetCurrentClockLocalTime(&clk);

    char *month;
    switch(clk.month){
        case 1:
            month = "Jan"; break;
        case 2:
            month = "Feb"; break;
        case 3:
            month = "Mar"; break;
        case 4:
            month = "Apr"; break;
        case 5:
            month = "May"; break;
        case 6:
            month = "Jun"; break;
        case 7:
            month = "Jul"; break;
        case 8:
            month = "Aug"; break;
        case 9:
            month = "Sep"; break;
        case 10:
            month = "Oct"; break;
        case 11:
            month = "Nov"; break;
        case 12:
            month = "Dec"; break;
    }

    snprintf(output, len, "%s % 2d %02d:%02d:%02d", month, clk.day, clk.hour, clk.minute, clk.second);
    return true;
}

static int connectudp_to_server(const char *server_ip, u16 port);
static int ssend(int socket, const char *str);
static void sclose(int *socket_e);

static int syslog_send(int facility, int severity, char *process_id, char *message){
    int syslog_socket = connectudp_to_server("192.168.69.1", 514);

    int priority = facility * 8 + severity;
    char *hostname = "PS3";
    char datetime[32];
    syslog_current_datetime(datetime, 32);

    char *buffer = (char *) malloc(SYSLOG_MAX_LENGTH + 1);
    
    snprintf(buffer, SYSLOG_MAX_LENGTH + 1, "<%i> %s %s %s %s", priority, datetime, hostname, process_id, message);
    if(ssend(syslog_socket, buffer) < 0){
        play_rco_sound("snd_trophy");
        if(sys_net_errno == SYS_NET_ENOBUFS){
            //system_call_3(392, 0x1004, 0x7,  0x36);
        }
    }
    free(buffer);
    sclose(&syslog_socket);
}
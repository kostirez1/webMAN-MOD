#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/syscall.h>
#include <cell/rtc.h>

#include "printf.h"
#include "network_util.h"
#include "syslog.h"
#include "vsh/vsh_exports.h"

int g_Syslog_socket = -1;

bool syslog_current_datetime(char *output, int len){
    CellRtcDateTime clk;
    cellRtcGetCurrentClockLocalTime(&clk);

    const char * month;
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
        default:
            month = "err"; break;
    }

    snprintf(output, len, "%s % 2d %02d:%02d:%02d", month, clk.day, clk.hour, clk.minute, clk.second);
    return true;
}

int syslog_send(int facility, int severity, const char *process_id, const char *message){
    if(g_Syslog_socket == -1){
        g_Syslog_socket = connect_to_server(UDP, "192.168.10.1", 514);
    }

    int priority = facility * 8 + severity;
    const char *hostname = "PS3"; //TODO: replace with hostname
    char datetime[32];
    syslog_current_datetime(datetime, 32);

    char *buffer = (char *) malloc(SYSLOG_MAX_LENGTH + 1);
    
    snprintf(buffer, SYSLOG_MAX_LENGTH + 1, "<%i> %s %s %s %s", priority, datetime, hostname, process_id, message);
    if(ssend(g_Syslog_socket, buffer) < 0){
        vshtask_notify("ssend error");
        return -1;
        //play_rco_sound("snd_trophy");
        /*if(sys_net_errno == SYS_NET_ENOBUFS){
            //system_call_3(392, 0x1004, 0x7,  0x36);
        }*/
    }

    //system_call_2(398, buffer, strlen(buffer)); // Works with cobra debug

    free(buffer);
    return 0; // TODO: return some status number
    //sclose(&syslog_socket);
}
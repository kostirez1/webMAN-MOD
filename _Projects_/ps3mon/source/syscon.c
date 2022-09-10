#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <cell/cell_fs.h>
#include <cell/rtc.h>

//#include "stdc.h"
#include "printf.h"
#include "vsh/vsh_exports.h"
#include "syscon.h"

inline int sys_sm_request_error_log(uint8_t offset, uint8_t* unknown0, uint32_t* unknown1, uint32_t* unknown2){
	system_call_4(390, (uint64_t)offset, (uint64_t)unknown0, (uint64_t)unknown1, (uint64_t)unknown2);
	return_to_user_prog(int);
}

const char * getSysconErrorDesc(uint32_t error_code){
	// Get the last 4 digits
	switch(error_code & 0xFFFF){
		case 0x1001:
			return "Power CELL";
		case 0x1002:
			return "Power RSX";
		case 0x1004:
			return "Power AC/DC";
		case 0x1103:
			return "Thermal Alert SYSTEM";
		case 0x1200:
			return "Thermal CELL";
		case 0x1201:
			return "Thermal RSX";
		case 0x1203:
			return "Thermal CELL VR";
		case 0x1204:
			return "Thermal South Bridge";
		case 0x1205:
			return "Thermal EE/GS";
		case 0x1301:
			return "CELL PLL";
		case 0x14FF:
			return "Check stop";
		case 0x1601:
			return "BE Livelock Detection";
		case 0x1701:
			return "CELL attention";
		case 0x1802:
			return "RSX init";
		case 0x1900:
			return "RTC Voltage";
		case 0x1901:
			return "RTC Oscilator";
		case 0x1902:
			return "RTC Access";
		case 0x2022:
			return "DVE Error (IC2406, CXM4024R MultiAV controller for analog out)";
		case 0xFFFF:
			return "Blank";
		default:
			return "Unknown";
	}
}

int format_date(char *buf, time_t print_time){
	CellRtcDateTime cDate;

	if(print_time != NULL){
		cellRtcSetTime_t(&cDate, print_time);
	} else {
		cellRtcGetCurrentClockLocalTime(&cDate);
	}

	int written = sprintf(buf, "%04i-%02i-%02i %02i:%02i:%02i", cDate.year, cDate.month, cDate.day, cDate.hour, cDate.minute, cDate.second);
	
	return (written > 0) ? written : 0;
}

void dump_syscon_errors(void){
	char buf[SYSCON_ERROR_BUFF_LEN];
	char datebuf[32];
	bool need_to_writeback = false;
	bool log_print_header = true;
	bool initial_dump = false;
	bool beeped = false;

	syscon_error error_cache[32];
	memset(&error_cache, 0, sizeof(error_cache));

	int fd_errors = -1;
	int fd_log = -1;
	CellFsStat stat_log;
	if(cellFsStat(SYSCON_ERROR_LOG, &stat_log) == CELL_FS_SUCCEEDED){
		if(stat_log.st_size >= SYSCON_ERROR_LOG_MAXSIZE){
			cellFsUnlink(SYSCON_ERROR_LOG);
		} else {
			log_print_header = false;
		}
	}

	if(cellFsOpen(SYSCON_ERROR_LOG, CELL_FS_O_WRONLY|CELL_FS_O_CREAT|CELL_FS_O_APPEND, &fd_log, NULL, NULL) == CELL_FS_SUCCEEDED){
		if(log_print_header){
			initial_dump = true;
			format_date(datebuf, NULL);
			snprintf(buf, SYSCON_ERROR_BUFF_LEN, "[%s] Syscon error log, for more information, refer to https://www.psdevwiki.com/ps3/Syscon_Error_Codes\r\n", datebuf);
			cellFsWrite(fd_log, buf, strlen(buf), NULL);

			snprintf(buf, SYSCON_ERROR_BUFF_LEN, "[%s] --- Making initial dump ---\r\n", datebuf);
			cellFsWrite(fd_log, buf, strlen(buf), NULL);
		 }
	}

	if(cellFsOpen(SYSCON_ERROR_CACHE, CELL_FS_O_RDONLY, &fd_errors, NULL, NULL) == CELL_FS_SUCCEEDED){
		uint64_t nread = 0;

		if(cellFsRead(fd_errors, &error_cache, sizeof(error_cache), &nread) == CELL_FS_SUCCEEDED){
			if(nread != sizeof(error_cache)){
				memset(&error_cache, 0, sizeof(error_cache));
			}
		}

		cellFsClose(fd_errors);
	} else {
		need_to_writeback = true;
	}

	for(int i = 0; i < 32; i++){
		uint8_t status;
		time_t print_time;

		uint32_t orig_code = error_cache[i].error_code;
		uint32_t orig_time = error_cache[i].error_time;

		uint32_t ret = sys_sm_request_error_log(i, &status, &error_cache[i].error_code, &error_cache[i].error_time);
		if(!ret && !status){
			// 1970-01-01 -> 2000-01-01, +30 years
			print_time = (time_t) ((uint32_t) error_cache[i].error_time + 946684800);

			if(orig_code != error_cache[i].error_code || orig_time != error_cache[i].error_time){
				format_date(datebuf, NULL);

				const char *reason = getSysconErrorDesc(error_cache[i].error_code);

				char errdatebuf[32];
				format_date(errdatebuf, print_time);
				snprintf(buf, SYSCON_ERROR_BUFF_LEN, "[%s] New error found %02d: %08X (%s) at %s\r\n", datebuf, i + 1, error_cache[i].error_code, reason, errdatebuf);
				cellFsWrite(fd_log, buf, strlen(buf), NULL);

				if(!initial_dump){
					snprintf(buf, SYSCON_ERROR_BUFF_LEN, "Syscon error detected:\r\n%08X (%s)\r\n%s", error_cache[i].error_code, reason, errdatebuf);

					vshtask_notify(buf);
					if(!beeped){
						BEEP3; // Beep 3 times to alert the user
						beeped = true;
					}
				}

				need_to_writeback = true;
			}
		} else {
			break;
		}
	}

	if(initial_dump){
		format_date(datebuf, NULL);
		snprintf(buf, SYSCON_ERROR_BUFF_LEN, "[%s] --- Initial dump complete ---\r\n", datebuf);
		cellFsWrite(fd_log, buf, strlen(buf), NULL);
	}

	if(need_to_writeback && cellFsOpen(SYSCON_ERROR_CACHE, CELL_FS_O_WRONLY|CELL_FS_O_CREAT|CELL_FS_O_TRUNC, &fd_errors, NULL, NULL) == CELL_FS_SUCCEEDED){
		uint64_t nwrite = 0;

		if(cellFsWrite(fd_errors, &error_cache, sizeof(error_cache), &nwrite) == CELL_FS_SUCCEEDED){
			if(nwrite == sizeof(error_cache)){
				// Write ok
			} else {
				// Wrong length written
			}
		} else {
			// Other error
		}

		cellFsClose(fd_errors);
	}

	cellFsClose(fd_log);
}
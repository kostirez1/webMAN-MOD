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

struct __attribute__((__packed__)) system_info {
	uint8_t firmware_version_high;
	uint16_t firmware_version_low;
	uint8_t reserved;
	uint8_t unk1[4];
	char platform_id[8];
	uint32_t firmware_build;
	uint8_t unk2[4];
};

static inline int sys_sm_get_system_info(struct system_info* unknown0)
{
	system_call_1(387, (uint64_t)unknown0);
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

static const char * getBoardDesc(const char *platform_id)
{
	// IDs according to https://www.psdevwiki.com/ps3/Platform_ID
	// Fat
	if(!strncmp(platform_id, "Cok14",  PLATFORM_ID_MAX_LEN)) return "COK-001 CECHA/CECHBxx";
	if(!strncmp(platform_id, "CokB10", PLATFORM_ID_MAX_LEN)) return "COK-002 CECHC/CECHExx";
	if(!strncmp(platform_id, "CokC12", PLATFORM_ID_MAX_LEN)) return "SEM-001 CECHGxx";
	if(!strncmp(platform_id, "CokD10", PLATFORM_ID_MAX_LEN)) return "DIA-001 CECHHxx";
	if(!strncmp(platform_id, "CokE10", PLATFORM_ID_MAX_LEN)) return "DIA-002 CECHKxx";
	if(!strncmp(platform_id, "Deb01",  PLATFORM_ID_MAX_LEN)) return "DEB-001 DECR-1400";
	if(!strncmp(platform_id, "CokF10", PLATFORM_ID_MAX_LEN)) return "VER-001 CECHL/CECHM/CECHP/CECHQxx";
	// Slim
	if(!strncmp(platform_id, "CokG11", PLATFORM_ID_MAX_LEN)) return "DYN-001 CECH-20xxA/B";
	if(!strncmp(platform_id, "CokH11", PLATFORM_ID_MAX_LEN)) return "SUR-001 CECH-21xxA/B";
	if(!strncmp(platform_id, "CokJ13", PLATFORM_ID_MAX_LEN)) return "JTP-001 CECH-25xxA/B";
	if(!strncmp(platform_id, "CokJ20", PLATFORM_ID_MAX_LEN)) return "JSD-001 CECH-25xxA/B";
	if(!strncmp(platform_id, "CokK10", PLATFORM_ID_MAX_LEN)) return "KTE-001 CECH-30xxA/B";
	// Super Slim
	if(!strncmp(platform_id, "CokM10", PLATFORM_ID_MAX_LEN)) return "MPX-001 NOR CECH-40xxB/C";
	if(!strncmp(platform_id, "CokM20", PLATFORM_ID_MAX_LEN)) return "MSX-001 NOR CECH-40xxB/C";
	if(!strncmp(platform_id, "CokM30", PLATFORM_ID_MAX_LEN)) return "MPX-001 eMMC CECH-40xxA";
	if(!strncmp(platform_id, "CokN10", PLATFORM_ID_MAX_LEN)) return "NPX-001 NOR CECH-40xxB/C";
	if(!strncmp(platform_id, "CokN30", PLATFORM_ID_MAX_LEN)) return "NPX-001 eMMC CECH-42xxA?"; // Wiki is unsure here
	if(!strncmp(platform_id, "CokP10", PLATFORM_ID_MAX_LEN)) return "PQX-001 NOR CECH-42xxB/C";
	if(!strncmp(platform_id, "CokP20", PLATFORM_ID_MAX_LEN)) return "PPX-001 NOR CECH-42xxB/C";
	if(!strncmp(platform_id, "CokP30", PLATFORM_ID_MAX_LEN)) return "PQX-001 eMMC CECH-42xxA";
	if(!strncmp(platform_id, "CokP40", PLATFORM_ID_MAX_LEN)) return "PPX-001 eMMC CECH-42xxA";
	if(!strncmp(platform_id, "CokR10", PLATFORM_ID_MAX_LEN)) return "RTX-001 NOR CECH-43xxB/C";
	if(!strncmp(platform_id, "CokR20", PLATFORM_ID_MAX_LEN)) return "REX-001 NOR CECH-43xxB/C";
	if(!strncmp(platform_id, "CokR30", PLATFORM_ID_MAX_LEN)) return "RTX-001 eMMC CECH-43xxA";
	if(!strncmp(platform_id, "CokR40", PLATFORM_ID_MAX_LEN)) return "REX-001 eMMC CECH-43xxA";
	// Anything else
	return "Unknown board, unknown model";
}

static int format_date(char *buf, time_t time){
	CellRtcDateTime cDate;
	bool rtc_not_set = false;
	int days = 0;

	if(time != NULL){
		cellRtcSetTime_t(&cDate, time);

		// When RTC is reset, it starts counting from around 2005/12/31 00:00:00 (0x0B488680)
		// Lets assume that all records from 2005 and 2006 are not actually that old
		if(cDate.year == 2005 || cDate.year == 2006)
		{
			// Subtract the J2000 => 2005/12/31 offset and also the 1970=>J2000 offset
			int64_t seconds_from_reset = time - 0x0B488680 - 946684800;

			days = seconds_from_reset / (86400);
			int remainder = seconds_from_reset % 86400;
			cDate.hour = remainder / 3600;
			remainder %= 3600;
			cDate.minute = remainder / 60;
			remainder %= 60;
			cDate.second = remainder;

			rtc_not_set = true;
		}
	} else {
		cellRtcGetCurrentClockLocalTime(&cDate);
	}

	int written;
	if(!rtc_not_set)
	{
		written = snprintf(buf, SYSCON_DATE_BUFF_LEN, "%04i-%02i-%02i %02i:%02i:%02i", cDate.year, cDate.month, cDate.day, cDate.hour, cDate.minute, cDate.second);
	}
	else
	{
		written = snprintf(buf, SYSCON_DATE_BUFF_LEN, "RTC: % 3id %02i:%02i:%02i", days, cDate.hour, cDate.minute, cDate.second);
	}
	
	return (written > 0) ? written : 0;
}

static int log_print(int fd_log, const char *str)
{
	char date[SYSCON_DATE_BUFF_LEN];
	char buf[SYSCON_DATE_BUFF_LEN];

	format_date(date, NULL);
	snprintf(buf, SYSCON_DATE_BUFF_LEN, "[%s] ", date);
	if(cellFsWrite(fd_log, buf, strlen(buf), NULL) != CELL_FS_SUCCEEDED) return false;

	if(cellFsWrite(fd_log, str, strlen(str), NULL) != CELL_FS_SUCCEEDED) return false;

	return true;
}

static bool load_disk_cache(syscon_error *target, unsigned int size)
{
	int fd_cache = -1;
	bool ret = true;

	if(cellFsOpen(SYSCON_ERROR_CACHE, CELL_FS_O_RDONLY, &fd_cache, NULL, NULL) == CELL_FS_SUCCEEDED){
		uint64_t nread = 0;

		if(cellFsRead(fd_cache, target, size, &nread) == CELL_FS_SUCCEEDED){
			if(nread != size){
				memset(target, 0, size);
				ret = false;
			}
		}

		cellFsClose(fd_cache);
	} else {
		ret = false;
	}

	return ret;
}

static bool save_disk_cache(syscon_error *cache, unsigned int size)
{
	int fd_cache = -1;
	bool ret = true;

	if(cellFsOpen(SYSCON_ERROR_CACHE, CELL_FS_O_WRONLY|CELL_FS_O_CREAT|CELL_FS_O_TRUNC, &fd_cache, NULL, NULL) == CELL_FS_SUCCEEDED){
		uint64_t nwrite = 0;

		if(cellFsWrite(fd_cache, cache, size, &nwrite) == CELL_FS_SUCCEEDED){
			if(nwrite == size){
				// Write ok
			} else {
				ret = false;
			}
		} else {
			ret = false;
		}

		cellFsClose(fd_cache);
	}

	return ret;
}

static bool load_syscon_errors(syscon_error *target)
{
	for(int i = 0; i < 32; i++){
		uint8_t status;

		sys_sm_request_error_log(i, &status, &target[i].error_code, &target[i].error_time);
	}

	return true;
}

static bool check_logfile(void)
{
	CellFsStat stat_log;
	if(cellFsStat(SYSCON_ERROR_LOG, &stat_log) == CELL_FS_SUCCEEDED){
		if(stat_log.st_size >= SYSCON_ERROR_LOG_MAXSIZE){
			cellFsUnlink(SYSCON_ERROR_LOG);
		} else {
			return true;
		}
	}
	
	return false;
}

static bool prepare_log_error(char *buf, syscon_error *cache, int index)
{
	// Original format is J2000, 1970-01-01 -> 2000-01-01, +30 years
	time_t print_time = (time_t) ((uint32_t) cache[index].error_time + 946684800);

	const char *reason = getSysconErrorDesc(cache[index].error_code);
	if(index == 31){
		// The last record is always a loop mark
		reason = "Loop mark";
	}

	char errdatebuf[SYSCON_DATE_BUFF_LEN];
	format_date(errdatebuf, print_time);
	snprintf(buf, SYSCON_ERROR_BUFF_LEN, "New error found: %08X (%s) at %s\r\n", cache[index].error_code, reason, errdatebuf);
	return true;
}

static bool print_log_header(int fd_log, syscon_error *cache)
{
	char buf[SYSCON_ERROR_BUFF_LEN];

	log_print(fd_log, "Syscon error log, for more information, refer to https://www.psdevwiki.com/ps3/Syscon_Error_Codes\r\n");

	struct system_info hwinfo = {0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, 0, {0, 0, 0, 0}};
	int ret = sys_sm_get_system_info(&hwinfo);
	if(!ret) {
		snprintf(buf, SYSCON_ERROR_BUFF_LEN, "Platform ID: %s (%s)\r\n", hwinfo.platform_id, getBoardDesc(hwinfo.platform_id));
		log_print(fd_log, buf);
	}

	log_print(fd_log, "--- Making initial dump ---\r\n");

	// Print errors backwards
	for(int i = 31; i >= 0; i--){
		prepare_log_error(buf, cache, i);
		log_print(fd_log, buf);
	}
	
	log_print(fd_log, "--- Initial dump complete ---\r\n");

	return true;
}

static bool print_new_errors(int fd_log, syscon_error *disk_cache, syscon_error *syscon_cache)
{
	bool beeped = false;
	for(int x = 0; x < 32; x++)
	{
		// look for the first disk cache entry in syscon cache
		// or for the last entry - loop mark
		if(memcmp(&disk_cache[0], &syscon_cache[x], sizeof(syscon_error)) == 0 || x == 31) 
		{
			if(x > 0)
			{
				char buf[SYSCON_ERROR_BUFF_LEN];

				for(int y = x-1; y >= 0; y--)
				{
					prepare_log_error(buf, syscon_cache, y);
					log_print(fd_log, buf);

					vshtask_notify(buf);
					if(!beeped)
					{
						BEEP3;
						beeped = true;
					}
				}

				// Errors found
				return true;
			}

			// No errors found
			return false;
		}
	}

	return false;
}

void dump_syscon_errors(void){
	bool need_to_writeback = false;
	bool print_header = true;

	syscon_error disk_cache[32]; // Syscon error dump loaded from HDD
	syscon_error syscon_cache[32]; // Syscon error dump from Syscon
	memset(&disk_cache, 0, sizeof(disk_cache));
	memset(&syscon_cache, 0, sizeof(syscon_cache));

	int fd_log = -1;
	
	// Check if logfile exists, delete if it's too large
	if(check_logfile()){
		print_header = false;
	}

	// Load all errors from Syscon
	load_syscon_errors(syscon_cache);

	// Load errors from HDD cache
	if(!load_disk_cache(disk_cache, sizeof(disk_cache)))
	{
		need_to_writeback = true;
		memcpy(disk_cache, syscon_cache, sizeof(disk_cache));
	}

	// Open logfile
	if(cellFsOpen(SYSCON_ERROR_LOG, CELL_FS_O_WRONLY|CELL_FS_O_CREAT|CELL_FS_O_APPEND, &fd_log, NULL, NULL) != CELL_FS_SUCCEEDED)
	{
		vshtask_notify("PS3Mon: Cannot create syscon logfile!");
		return;
	}

	// Print header and initial dump if necessary
	if(print_header) print_log_header(fd_log, syscon_cache);

	if(print_new_errors(fd_log, disk_cache, syscon_cache))
	{
		memcpy(disk_cache, syscon_cache, sizeof(disk_cache));
		need_to_writeback = true;
	}

	if(need_to_writeback) save_disk_cache(disk_cache, sizeof(disk_cache));

	cellFsClose(fd_log);
}
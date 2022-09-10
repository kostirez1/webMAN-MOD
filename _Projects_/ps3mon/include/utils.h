#include <stdbool.h>

#include "c_types.h"
#include "vsh/vshmain.h"

#define SC_GET_FAN_POLICY		(409)
#define SC_GET_TEMPERATURE		(383)
#define SC_FS_DISK_FREE 				(840)
#define SC_GET_FREE_MEM 	(352)

#define IS_ON_XMB		(GetCurrentRunningMode() == 0)

#define     KB		     1024UL
#define   _2KB_		     2048UL
#define   _4KB_		     4096UL
#define   _6KB_		     6144UL
#define   _8KB_		     8192UL
#define  _12KB_		    12288UL
#define  _16KB_		    16384UL
#define  _32KB_		    32768UL
#define  _48KB_		    49152UL
#define  _62KB_		    63488UL
#define  _64KB_		    65536UL
#define _128KB_		   131072UL
#define _192KB_		   196608UL
#define _256KB_		   262144UL
#define _384KB_		   393216UL
#define _512KB_		   524288UL
#define _640KB_		   655360UL
#define _768KB_		   786432UL
#define  _1MB_		0x0100000UL
#define  _2MB_		0x0200000UL
#define  _3MB_		0x0300000UL
#define _32MB_		0x2000000UL

typedef struct {
	u32 total;
	u32 avail;
} _meminfo;

void get_temperature(uint32_t _dev, uint8_t *temp);

void sys_sm_get_fan_policy(uint8_t id, uint8_t  *st, uint8_t  *mode, uint8_t  *speed, uint8_t  *unknown);

bool islike(const char *param, const char *text);

u64 get_free_space(const char *dev_name);

void get_meminfo(_meminfo *meminfo);

int get_game_info(char *game_titleID, char *game_title);
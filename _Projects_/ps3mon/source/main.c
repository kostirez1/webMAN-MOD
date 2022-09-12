#include <stdbool.h>

#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <sys/syscall.h>
#include <sys/sys_time.h>
#include <sys/timer.h>
#include <sys/time_util.h>
#include <cell/cell_fs.h>
#include <cell/pad.h>
#include <cell/rtc.h>

#include "vsh/vsh_exports.h"
#include "ini.h"
#include "syscon.h"
#include "syslog.h"
#include "telegraf.h"
#include "globals.h"


SYS_MODULE_INFO(PS3Mon, 0, 1, 0);
SYS_MODULE_START(ps3mon_start);
SYS_MODULE_STOP(ps3mon_stop);

//SYS_LIB_DECLARE(PS3MonLib, SYS_LIB_AUTO_EXPORT|SYS_LIB_WEAK_IMPORT);
//SYS_LIB_EXPORT(getSysconErrorDesc, PS3MonLib);

static sys_ppu_thread_t thread_id_loader = SYS_PPU_THREAD_ID_INVALID;
static sys_ppu_thread_t thread_id_main = SYS_PPU_THREAD_ID_INVALID;
static sys_ppu_thread_t thread_id_telegraf = SYS_PPU_THREAD_ID_INVALID;

bool g_poison_pill = false;
CellRtcTick g_startTick;

int32_t ps3mon_start(uint64_t arg);
int32_t ps3mon_stop(void);

static inline void _sys_ppu_thread_exit(uint64_t val)
{
	system_call_1(41, val);
}

static inline sys_prx_id_t prx_get_module_id_by_address(void *addr)
{
	system_call_1(461, (uint64_t)(uint32_t)addr);
	return (int)p1;
}

static void * getNIDfunc(const char * vsh_module, uint32_t fnid)
{
	uint32_t table = (*(uint32_t*)0x1008C) + 0x984; // vsh table address

	while(((uint32_t)*(uint32_t*)table) != 0)
	{
		uint32_t* export_stru_ptr = (uint32_t*)*(uint32_t*)table;

		const char* lib_name_ptr =  (const char*)*(uint32_t*)((char*)export_stru_ptr + 0x10);

		if(strncmp(vsh_module, lib_name_ptr, strlen(lib_name_ptr))==0)
		{
			uint32_t lib_fnid_ptr = *(uint32_t*)((char*)export_stru_ptr + 0x14);
			uint32_t lib_func_ptr = *(uint32_t*)((char*)export_stru_ptr + 0x18);
			uint16_t count = *(uint16_t*)((char*)export_stru_ptr + 6); // number of exports
			for(int i = 0; i < count; i++)
			{
				if(fnid == *(uint32_t*)((char*)lib_fnid_ptr + i*4))
				{
					return (void**)*((uint32_t*)(lib_func_ptr) + i);
				}
			}
		}
		table += 4;
	}
	return 0;
}

static uint8_t wait_for_xmb(void)
{
	uint8_t t = 0;
	while(true && (View_Find("explore_plugin") == 0)) {if(++t > 45) break; sys_timer_sleep(1);}
	return (t > 45); // true = timeout
}

/***********************************************************************
* plugin main thread
***********************************************************************/
static void PS3MonMain(uint64_t arg)
{
	vshtask_notify("PS3MON Loaded");
	dump_syscon_errors();
	
	// TODO: better logging, only in DEBUG version
	//syslog_send(21, 6, "PS3Mon", "Loaded");

	//////////////////////////////////////////////////////////////////////
	// example: class usage
	//char nickname[0x80];
	//int32_t nickname_len;
	//
	//xsetting_0AF1F161()->GetSystemNickname(nickname, &nickname_len);
	//////////////////////////////////////////////////////////////////////
	//char nickname[0x80];
	//int32_t nickname_len;
	//xsetting_0AF1F161()->GetSystemNickname(nickname, &nickname_len);

	//nickname[nickname_len] = 0;
	//vshtask_notify(nickname); works

	sys_ppu_thread_exit(0);
}

static void ps3mon_loader_thread(uint64_t arg)
{
	// Wait for XMB to load
	wait_for_xmb();

	sys_ppu_thread_create(&thread_id_main, PS3MonMain, 0, 3000, 1500, SYS_PPU_THREAD_CREATE_JOINABLE, "PS3MonMain");
	sys_ppu_thread_create(&thread_id_telegraf, telegraf_thread, 0, 3000, 1500, SYS_PPU_THREAD_CREATE_JOINABLE, "TelegrafMonitoring");

	sys_ppu_thread_exit(0);
}

int ps3mon_start(uint64_t arg)
{
	cellRtcGetCurrentTick(&g_startTick);

	sys_ppu_thread_create(&thread_id_loader, ps3mon_loader_thread, 0, 3000, 1500, SYS_PPU_THREAD_CREATE_JOINABLE, "PS3MonLoader");

	_sys_ppu_thread_exit(0);
	return SYS_PRX_RESIDENT;
}

static void ps3mon_stop_thread(uint64_t arg)
{
	g_poison_pill = true;

	if (thread_id_loader != SYS_PPU_THREAD_ID_INVALID){
		uint64_t exit_code;
		sys_ppu_thread_join(thread_id_loader, &exit_code);
	}

	if (thread_id_main != SYS_PPU_THREAD_ID_INVALID){
		uint64_t exit_code;
		sys_ppu_thread_join(thread_id_main, &exit_code);
	}

	if (thread_id_telegraf != SYS_PPU_THREAD_ID_INVALID){
		uint64_t exit_code;
		sys_ppu_thread_join(thread_id_telegraf, &exit_code);
	}

	sys_ppu_thread_exit(0);
}

static void finalize_module(void)
{
	uint64_t meminfo[5];

	sys_prx_id_t prx = prx_get_module_id_by_address(finalize_module);

	meminfo[0] = 0x28;
	meminfo[1] = 2;
	meminfo[3] = 0;

	system_call_3(482, prx, 0, (uint64_t)(uint32_t)meminfo);
}

int ps3mon_stop(void)
{
	sys_ppu_thread_t t;
	uint64_t exit_code;

	sys_ppu_thread_create(&t, ps3mon_stop_thread, 0, 0, 1500, SYS_PPU_THREAD_CREATE_JOINABLE, "PS3MonStop");
	sys_ppu_thread_join(t, &exit_code);

	finalize_module();
	_sys_ppu_thread_exit(0);
	return SYS_PRX_STOP_OK;
}

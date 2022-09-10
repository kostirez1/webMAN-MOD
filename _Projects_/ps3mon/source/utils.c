#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/syscall.h>
#include "vsh/paf.h"
#include "vsh/game_plugin.h"
#include "vsh/vshmain.h"
#include "utils.h"
#include "peek_poke.h"

uint64_t get_fan_policy_offset = 0x8000000000009E38ULL; // sys 409 get_fan_policy  4.55/4.60/4.65/4.70/4.75/4.76/4.78/4.80-4.89

void get_temperature(uint32_t _dev, uint8_t *temp){
	uint32_t _temp;
	system_call_2(SC_GET_TEMPERATURE, (uint64_t)(uint32_t) _dev, (uint64_t)(uint32_t)&_temp); *temp = _temp >> 24; // return °C
}

void sys_sm_get_fan_policy(uint8_t id, uint8_t  *st, uint8_t  *mode, uint8_t  *speed, uint8_t  *unknown){

	uint64_t restore_get_fan_policy = peekq(get_fan_policy_offset); // sys 409 get_fan_policy
	uint64_t enable_get_fan_policy = 0x3860000100000000ULL | (restore_get_fan_policy & 0xffffffffULL);

	lv2_poke_fan(get_fan_policy_offset, enable_get_fan_policy);
	system_call_5(SC_GET_FAN_POLICY, (uint64_t) id, (uint64_t)(uint32_t) st, (uint64_t)(uint32_t) mode, (uint64_t)(uint32_t) speed, (uint64_t)(uint32_t) unknown);
	lv2_poke_fan(get_fan_policy_offset, restore_get_fan_policy);
}

bool islike(const char *param, const char *text)
{
	if(!param || !text) return false;
	while(*text && (*text == *param)) text++, param++;
	return !*text;
}

u64 get_free_space(const char *dev_name)
{
	u64 freeSize = 0, devSize = 0;
	if(!islike(dev_name, "/dev_")) return 0;

	{system_call_3(SC_FS_DISK_FREE, (u64)(u32)(dev_name), (u64)(u32)&devSize, (u64)(u32)&freeSize);}
	if((freeSize>>30) > _2MB_) freeSize = 0;
	return freeSize;
}

void get_meminfo(_meminfo *meminfo)
{
	system_call_1(SC_GET_FREE_MEM, (u64)(u32) meminfo);
}

int get_game_info(char *game_titleID, char *game_title)
{
	game_titleID[0] = game_title[0] = 0;

	if(IS_ON_XMB) return 0; // prevents game_plugin detection during PKG installation

	int is_ingame = View_Find("game_plugin");

	if(is_ingame)
	{
		char _game_info[0x120];
		game_interface = (game_plugin_interface *)plugin_GetInterface(is_ingame, 1);
		game_interface->gameInfo(_game_info);

		snprintf(game_titleID, 10, "%s", _game_info+0x04);
		snprintf(game_title, 64, "%s", _game_info+0x14);
	}

	return is_ingame;
}
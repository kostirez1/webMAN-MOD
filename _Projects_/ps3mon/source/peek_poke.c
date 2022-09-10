#include <sys/syscall.h>

#include "peek_poke.h"
#include "c_types.h"

/////////////////// LV2 PEEK //////////////////////
u64 lv2_peek_cfw(u64 addr){ //sc8 + LV2_OFFSET_ON_LV1
	system_call_1(SC_PEEK_LV1, addr + LV2_OFFSET_ON_LV1); //old: {system_call_1(SC_PEEK_LV2, addr);}
	return (u64) p1;
}

u64 lv2_peek_hen(u64 addr){ //sc6
	system_call_1(SC_PEEK_LV2, addr);
	return (u64) p1;
}

u64 peekq(u64 addr){
    return lv2_peek_hen(addr);
}

/////////////////// LV2 POKE //////////////////////
void lv2_poke_hen(u64 addr, u64 value) //sc7
{
	system_call_2(SC_POKE_LV2, addr, value);
}

void lv2_poke_cfw(u64 addr, u64 value) //sc8 + LV2_OFFSET_ON_LV1
{
	system_call_2(SC_POKE_LV1, addr + LV2_OFFSET_ON_LV1, value);
}

void lv2_poke_fan(u64 addr, u64 value){
    return lv2_poke_hen(addr, value);
}
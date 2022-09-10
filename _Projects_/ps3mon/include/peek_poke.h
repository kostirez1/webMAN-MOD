#include "c_types.h"

#define SC_PEEK_LV2						(6)
#define SC_POKE_LV2						(7)
#define SC_PEEK_LV1 					(8)
#define SC_POKE_LV1 					(9)

#define LV2_OFFSET_ON_LV1 0x01000000ULL // 0x01000000ULL = CEX, 0x08000000ULL = DEX?

u64 lv2_peek_cfw(u64 addr);

u64 lv2_peek_hen(u64 addr);

u64 peekq(u64 addr);

void lv2_poke_hen(u64 addr, u64 value);

void lv2_poke_cfw(u64 addr, u64 value);

void lv2_poke_fan(u64 addr, u64 value);
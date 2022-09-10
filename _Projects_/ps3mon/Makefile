CELL_MK_DIR = $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

PEXPORTPICKUP = ppu-lv2-prx-exportpickup

CRT_HEAD += $(shell ppu-lv2-gcc -print-file-name'='ecrti.o)
CRT_HEAD += $(shell ppu-lv2-gcc -print-file-name'='crtbegin.o)
CRT_TAIL += $(shell ppu-lv2-gcc -print-file-name'='crtend.o)
CRT_HEAD += $(shell ppu-lv2-gcc -print-file-name'='ecrtn.o)

ROOT_PATH = ../..

PPU_SRCS = $(wildcard source/*.c)
PPU_INCDIRS	+= -I./include
PPU_PRX_TARGET = ps3mon.prx
PPU_PRX_LDFLAGS += -L./library -Wl,--strip-unused-data
PPU_PRX_STRIP_FLAGS = -s

PPU_PRX_LDLIBS 	+= -lfs_stub -lio_stub -lrtc_stub \
                   -lvsh_export_stub \
                   -lpaf_export_stub \
                   -lvshmain_export_stub \
                   -lvshtask_export_stub \
                   -lallocator_export_stub \
                   -lstdc_export_stub \
                   -lxsetting_export_stub \
                   -lnet_stub \
                   -lc

PPU_CFLAGS += -Os -ffunction-sections -fdata-sections -fno-builtin-printf -nodefaultlibs -std=gnu99 -Wno-shadow -Wno-unused-parameter

all:
	$(MAKE) $(PPU_OBJS_DEPENDS)
	$(PPU_PRX_STRIP) --strip-debug --strip-section-header $(PPU_PRX_TARGET)
	cp -r $(ROOT_PATH)/data data
	$(ROOT_PATH)/scetool -0 SELF -1 TRUE -s FALSE -2 0A -3 1070000052000001 -4 01000002 -5 APP -6 0003004000000000 -A 0001000000000000 -e $(PPU_PRX_TARGET) $(PPU_SPRX_TARGET)
	rm -r objs $(PPU_PRX_TARGET) *.sym

include $(CELL_MK_DIR)/sdk.target.mk

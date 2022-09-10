#include <stdio.h>

#include <netex/ifctl.h>
#include <sys/ppu_thread.h>
#include <sys/event.h>
#include <sys/timer.h>
#include <cell/rtc.h>
#include <netex/errno.h>
#include <netex/net.h>

#include "telegraf.h"
#include "utils.h"
#include "network_util.h"
#include "ini.h"
#include "vsh/vsh_exports.h"
#include "globals.h"

// float getCurrentFps();
// void setReporter(float *dst);

void telegraf_thread(uint64_t arg){
	// TODO: output format selection
	// TODO: broadcast selection, broadcast port

	sys_timer_sleep(TELEGRAF_INIT_SLEEP); // Sleep for OS init | wait_for_xmb()?

	// TODO: Read settings phase
	
	ini_t *config = ini_load(TELEGRAF_CONFIG);
	if(config == NULL){
		// play_rco_sound("snd_trophy");
		vshtask_notify("Telegraf config does not exist!");
		sys_ppu_thread_exit(1);
	}
	
	const char *s_server = ini_get(config, "telegraf", "server");
	const char *s_port = ini_get(config, "telegraf", "port");
	//int n_port = 0; 
	//ini_sget(config, "telegraf", "port", "%u", &n_port); // fails on T(hread) L(ocal) S(torage) error?? sscanf() related
	if(s_server == NULL || s_port == NULL){
		//play_rco_sound("snd_trophy");
		vshtask_notify("Invalid IP or port!");
		sys_ppu_thread_exit(1);
	}
	uint16_t n_port = atoi(s_port); // strol might be better for error detection
	
	// TODO: Init phase

	static int conn_socket = -1;
	static int broadcast_socket = -1;
	static char msg[256];
	uint8_t d_retries = 0;
	
	sprintf(msg, "Telegraf endpoint set to %s:%d", s_server, n_port);
	vshtask_notify(msg);
	
	sys_timer_t precise_timer;
	sys_timer_create(&precise_timer);
	
	sys_event_queue_t precise_queue;
	sys_event_queue_attribute_t precise_queue_attr;
	sys_event_queue_attribute_initialize(precise_queue_attr);
	uint64_t queue_size = 16;
	sys_event_queue_create(&precise_queue, &precise_queue_attr, SYS_EVENT_QUEUE_LOCAL, queue_size);
	
	sys_timer_connect_event_queue(precise_timer, precise_queue, TELEGRAF_TELEMETRY_EVENT, 100, 200);
	
	sys_timer_sleep(TELEGRAF_NETWORK_SLEEP);
	
	telegraf_conn_retry:
	conn_socket = connect_to_server(UDP, s_server, n_port);
	if(conn_socket < 0){
		d_retries++;
		sys_timer_sleep(2);
		if(d_retries < 10) goto telegraf_conn_retry;
		if(d_retries >= 10) {
			// play_rco_sound("snd_trophy");
			vshtask_notify("Connection failed!");
			sys_ppu_thread_exit(1);
		}
	}
	broadcast_socket = connect_to_broadcast(36000);
	
	//play_rco_sound("snd_trophy");
	vshtask_notify("Telegraf connected...");
	float fps = 0;

	/*if(sys_prx_get_module_id_by_name("VshFpsCounter", 0, NULL) > CELL_OK){
		vshtask_notify("FPS hooked");
		setReporter(&fps);

		while(true){
			//float fps = getCurrentFps();
			char buf[16];
			sprintf(buf, "%.2f", fps);
			vshtask_notify(buf);
			syslog_send(21, 6, "PS3Mon", "FPS received");
			sys_timer_sleep(10);
		}
	} else {
		vshtask_notify("FPS not hooked");
	}*/
	
	char system_name[0x80];
	memset(system_name, 0, 0x80);
	int system_name_len = 0;		
	xsetting_0AF1F161()->GetSystemNickname(system_name, &system_name_len);
	
	int errors = 0;
	int last_error = 0;
	
	sys_timer_start_periodic(precise_timer, 10000000); // 10 seconds

	// TODO: Main loop phase
	
	while(!g_poison_pill){
		sys_event_t event;
		
		if(sys_event_queue_receive(precise_queue, &event, 15000000) == CELL_OK){ // 15 seconds timeout
			// Event received
			
			s32 arg_1, total_time_in_sec, power_on_ctr, power_off_ctr;
			
			CellRtcTick pTick; uint32_t ss = 0;

			cellRtcGetCurrentTick(&pTick);

			//poll_start_play_time();

			ss = (uint32_t)((pTick.tick - g_startTick.tick) / 1000000); if(ss > 864000) ss = 0;
			
			system_call_4(0x187, (u32)&arg_1, (u32)&total_time_in_sec, (u32)&power_on_ctr, (u32)&power_off_ctr);
			ss += (uint32_t)total_time_in_sec;

			memset(msg, 0, 256);
			
			uint8_t t1 = 0, t2 = 0;
			get_temperature(0, &t1); // CPU // 3E030000 -> 3E.03°C -> 62.(03/256)°C
			get_temperature(1, &t2); // RSX

			uint8_t st, mode, fan_speed, unknown;
			sys_sm_get_fan_policy(0, &st, &mode, &fan_speed, &unknown);
			
			uint64_t hdd_free = get_free_space("/dev_hdd0");

			// TCP/IP stack info
			unsigned int tcpip_min = 0;
			sys_net_if_ctl(0, SYS_NET_CC_GET_MEMORY_FREE_MINIMUM, &tcpip_min, sizeof(tcpip_min));
			unsigned int tcpip_free = 0;
			sys_net_if_ctl(0, SYS_NET_CC_GET_MEMORY_FREE_CURRENT, &tcpip_free, sizeof(tcpip_free));

			_meminfo meminfo;
			get_meminfo(&meminfo);

			char game_TitleID[16];
			char game_Title[64];
			get_game_info(game_TitleID, game_Title);
			
			sprintf(msg, "ps3mon,hostname=%s,game=%s cpu=%ii,gpu=%ii,fan=%ii,ramfree=%ii,hddfree=%llui,tcpipfree=%ii,tcpipmin=%ii,celltime=%ii,fps=%.2f", system_name, (game_TitleID[0] != 0) ? game_TitleID : "XMB000000", t1, t2, fan_speed * 100 / 255, (int) meminfo.avail, hdd_free, tcpip_free, tcpip_min, ss, fps);
			int reply = ssend(conn_socket, msg);
			if(reply < 0){
				errors++;
				last_error = sys_net_errno;
			}
			ssend(broadcast_socket, msg);
		}
	}
	
	//play_rco_sound("snd_trophy");
	vshtask_notify("Telegraf unloaded!");
	
	// TODO: Cleanup phase

	// Resource cleanup
	ini_free(config);
	sclose(&conn_socket);
	sys_timer_stop(precise_timer);
	sys_timer_disconnect_event_queue(precise_timer);
	sys_timer_destroy(precise_timer);
	sys_event_queue_destroy(precise_queue, SYS_EVENT_QUEUE_DESTROY_FORCE);

	sys_timer_sleep(5);
	sys_ppu_thread_exit(0);
}
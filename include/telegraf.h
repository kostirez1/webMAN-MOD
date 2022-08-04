// https://github.com/rxi/ini ported to Cell
#include "tinyini/ini.c"

#define TELEGRAF_CRASH_DETECT "/dev_hdd0/tmp/telegraf_running"
#define TELEGRAF_CONFIG "/dev_hdd0/tmp/telegraf.ini"
#define TELEGRAF_INIT_SLEEP 10
#define TELEGRAF_NETWORK_SLEEP 45

#define TELEGRAF_TELEMETRY_EVENT 0x12345678ULL

sys_ppu_thread_t thread_id_telegraf = SYS_PPU_THREAD_ID_INVALID;

static void telegraf_thread(u64 arg)
{
	sys_ppu_thread_sleep(TELEGRAF_INIT_SLEEP); // Sleep for OS init | wait_for_xmb()?
	
	if(file_exists(TELEGRAF_CRASH_DETECT)){
		play_rco_sound("snd_trophy");
		vshtask_notify("Telegraf cannot be loaded. Crash file exists!");
		sys_ppu_thread_exit(1);
	}
	
	save_file(TELEGRAF_CRASH_DETECT, "", 0);
	
	ini_t *config = ini_load(TELEGRAF_CONFIG);
	if(config == NULL){
		play_rco_sound("snd_trophy");
		vshtask_notify("Config file does not exist!");
		cellFsUnlink(TELEGRAF_CRASH_DETECT);
		sys_ppu_thread_exit(1);
	}
	
	const char *s_server = ini_get(config, "telegraf", "server");
	const char *s_port = ini_get(config, "telegraf", "port");
	//int n_port = 0; 
	//ini_sget(config, "telegraf", "port", "%u", &n_port); // fails on TLS error??!! sscanf() related
	if(s_server == NULL || s_port == NULL){
		play_rco_sound("snd_trophy");
		vshtask_notify("Invalid IP or port!");
		cellFsUnlink(TELEGRAF_CRASH_DETECT);
		sys_ppu_thread_exit(1);
	}
	u16 n_port = atoi(s_port); // strol might be better for error detection
	
	static int conn_socket = -1;
	static char msg[256];
	u8 d_retries = 0;
	
	sprintf(msg, "Telegraf endpoint set to %s:%d", s_server, n_port);
	vshtask_notify(msg);
	
	sys_timer_t precise_timer;
	sys_timer_create(&precise_timer);
	
	sys_event_queue_t precise_queue;
	sys_event_queue_attribute_t precise_queue_attr = {SYS_SYNC_PRIORITY, SYS_PPU_QUEUE};
	uint64_t queue_size = 16;
	sys_event_queue_create(&precise_queue, &precise_queue_attr, SYS_EVENT_QUEUE_LOCAL, queue_size);
	
	sys_timer_connect_event_queue(precise_timer, precise_queue, TELEGRAF_TELEMETRY_EVENT, 100, 200);
	
	sys_ppu_thread_sleep(TELEGRAF_NETWORK_SLEEP);
	
	telegraf_conn_retry:
	conn_socket = connectudp_to_server(s_server, n_port);
	if(conn_socket <  0){
		d_retries++;
		sys_ppu_thread_sleep(2);
		if(d_retries < 10) goto telegraf_conn_retry;
		if(d_retries >= 10) {
			play_rco_sound("snd_trophy");
			vshtask_notify("Connection failed!");
			cellFsUnlink(TELEGRAF_CRASH_DETECT);
			sys_ppu_thread_exit(1);
		}
	}
	
	play_rco_sound("snd_trophy");
	vshtask_notify("Telegraf connected...");
	
	char system_name[0x80];
	memset(system_name, 0, 0x80);
	int system_name_len = 0;		
	xsetting_0AF1F161()->GetSystemNickname(system_name, &system_name_len);
	
	int errors = 0;
	int last_error = 0;
	
	sys_timer_start_periodic(precise_timer, 10000000); // 10 seconds
	
	while(working){
		sys_event_t event;
		
		if(sys_event_queue_receive(precise_queue, &event, 15000000) == CELL_OK){ // 15 seconds timeout
			// Event received
			
			// TODO: Monitor Cell total time
			//       Monitor internal HDD free space
			//       Monitor free RAM
		
			//s32 arg_1, total_time_in_sec, power_on_ctr, power_off_ctr;
			//u32 dd, hh, mm, ss;
			
			//sys_sm_request_be_count(&arg_1, &total_time_in_sec, &power_on_ctr, &power_off_ctr); // Cell time
			
			//CellRtcTick pTick;
			//cellRtcGetCurrentTick(&pTick); // RTC time
			
			//ss = (u32)((pTick.tick - (bb ? gTick.tick : rTick.tick)) / 1000000); if(ss > 864000) ss = 0;
			//ss += (u32)total_time_in_sec;
			
			memset(msg, 0, 256);
			
			u8 t1 = 0, t2 = 0;
			get_temperature(0, &t1); // CPU // 3E030000 -> 3E.03°C -> 62.(03/256)°C
			get_temperature(1, &t2); // RSX

			u8 st, mode, unknown;
			sys_sm_get_fan_policy(0, &st, &mode, &fan_speed, &unknown);
			
			get_game_info();
			
			sprintf(msg, "ps3mon,hostname=%s,game=%s cpu=%ii,rsx=%ii,fan=%ii", system_name, (_game_TitleID[0] != 0) ? _game_TitleID : "XMB", t1, t2, fan_speed * 100 / 255);
			int reply = ssend(conn_socket, msg);
			if(reply < 0){
				errors++;
				last_error = sys_net_errno;
			}
		}
	}
	
	play_rco_sound("snd_trophy");
	vshtask_notify("Telegraf unloaded!");
	
	// Resource cleanup
	ini_free(config);
	sclose(&conn_socket);
	sys_timer_stop(precise_timer);
	sys_timer_disconnect_event_queue(precise_timer);
	sys_timer_destroy(precise_timer);
	sys_event_queue_destroy(precise_queue, SYS_EVENT_QUEUE_DESTROY_FORCE);
	cellFsUnlink(TELEGRAF_CRASH_DETECT);

	sys_ppu_thread_sleep(5);
	sys_ppu_thread_exit(0);
}
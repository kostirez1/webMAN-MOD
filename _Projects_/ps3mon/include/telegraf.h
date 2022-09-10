#define TELEGRAF_CONFIG "/dev_hdd0/tmp/ps3mon_telegraf.ini"
#define TELEGRAF_INIT_SLEEP 10
#define TELEGRAF_NETWORK_SLEEP 45
#define TELEGRAF_CRASH_DETECT_SLEEP 90

#define TELEGRAF_TELEMETRY_EVENT 0x12345678ULL

void telegraf_thread(uint64_t arg);
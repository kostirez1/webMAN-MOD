#define _1MB_ 0x0100000UL
#define SC_RING_BUZZER  				(392)
#define BEEP3 { system_call_3(SC_RING_BUZZER, 0x1004, 0xa, 0x1b6); }

#define SYSCON_ERROR_CACHE "/dev_hdd0/tmp/ps3mon_syscon_errors.bin"
#define SYSCON_ERROR_LOG "/dev_hdd0/tmp/ps3mon_syscon_error_log.txt"
#define SYSCON_ERROR_LOG_MAXSIZE _1MB_
#define SYSCON_ERROR_BUFF_LEN 256
#define SYSCON_DATE_BUFF_LEN 32

#define PLATFORM_ID_MAX_LEN 6

typedef struct syscon_error {
	uint32_t error_code;
	uint32_t error_time;
} syscon_error;

inline int sys_sm_request_error_log(uint8_t offset, uint8_t* unknown0, uint32_t* unknown1, uint32_t* unknown2);

const char * getSysconErrorDesc(uint32_t error_code);

void dump_syscon_errors(void);

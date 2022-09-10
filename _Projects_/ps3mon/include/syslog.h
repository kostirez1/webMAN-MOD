#define SYSLOG_MAX_LENGTH 1024 // maximum message length according to RFC3164

bool syslog_current_datetime(char *output, int len);
int syslog_send(int facility, int severity, const char *process_id, const char *message);
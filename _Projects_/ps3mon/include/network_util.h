#include <sys/types.h>
#include <sys/socket.h>

typedef enum {
    TCP = SOCK_STREAM,
    UDP = SOCK_DGRAM
} socket_type;

int ssend(int socket, const char *str);

int connect_to_server_ex(socket_type s_type, const char *server_ip, uint16_t port, uint8_t rcv_timeout);

int connect_to_server(socket_type s_type, const char *server_ip, uint16_t port);

int connect_to_broadcast(uint16_t port);

int slisten(socket_type s_type, int port, int backlog);

void sclose(int *socket_e);
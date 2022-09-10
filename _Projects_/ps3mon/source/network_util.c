#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netex/errno.h>
#include <netex/net.h>
#include <arpa/inet.h>

#include "network_util.h"
#include "vsh/vsh_exports.h"

int ssend(int socket, const char *str){
	return send(socket, str, strlen(str), 0);
}

int connect_to_server_ex(socket_type s_type, const char *server_ip, uint16_t port, uint8_t rcv_timeout){
	int s = socket(AF_INET, s_type, 0);
	if(s < 0) return -1;

	struct sockaddr_in sin;
	socklen_t sin_len = sizeof(sin);
	memset(&sin, 0, sin_len);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	unsigned int ip_address;

	if((ip_address = inet_addr(server_ip)) != (unsigned int)-1)
	{
		sin.sin_addr.s_addr = ip_address;
	}
	else
	{
		struct hostent *hp;

		if((hp = gethostbyname(server_ip)) == NULL)
		{
			return -1;
		}

		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}

	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = rcv_timeout ? rcv_timeout : 30;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	if(rcv_timeout)
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	if(connect(s, (struct sockaddr *)&sin, sin_len) < 0)
	{
		return -1;
	}

	tv.tv_sec = 60;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	//setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	return s;
}

int connect_to_server(socket_type s_type, const char *server_ip, uint16_t port){
	return connect_to_server_ex(s_type, server_ip, port, false);
}

int connect_to_broadcast(uint16_t port){
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if(s < 0) return -1;

	int broadcast = 1;
	int ret = setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
	if(ret) goto failed;

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	sin.sin_port = htons(port);

	ret = connect(s, (const struct sockaddr *) &sin, sizeof(sin));
	if(ret) goto failed;

	return s;

	failed:
	if (s >= 0) {
		close(s);
		s = -1;
	}

	return s;
}

int slisten(socket_type s_type, int port, int backlog){
	int s = socket(AF_INET, s_type, 0);
	if(s < 0)
	{
		if(sys_net_errno == SYS_NET_EMFILE){
			// "No free space in the socket table!!!"
		}
		// "Listen socket create failed!!! Port", port
		return -1;
	}

	//int optval = 1;
	//setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	//setsockopt(s, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	//setsockopt(s, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));

	struct sockaddr_in sa;
	socklen_t sin_len = sizeof(sa);
	memset(&sa, 0, sin_len);

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(s, (struct sockaddr *)&sa, sin_len) < 0){
		if(sys_net_errno == SYS_NET_EADDRINUSE){
			// "listen bind addr in use!!! Port" port
		}
		// "listen Bind failed!!!"
		shutdown(s, SHUT_RDWR);
		socketclose(s); 
		return -1;
	}
	if(listen(s, backlog) < 0){
		if(sys_net_errno == SYS_NET_EBADF){
			// "listen bad socket number!!!"
		}
		// "listen failed!!!"
		shutdown(s, SHUT_RDWR);
		socketclose(s); 
		return -1;
	}

	return s;
}

void sclose(int *socket_e){
	if(*socket_e != -1){
		shutdown(*socket_e, SHUT_RDWR);
		socketclose(*socket_e);
		*socket_e = -1;
	}
}
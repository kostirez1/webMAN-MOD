#include <sys/tty.h>

static void play_rco_sound(const char *sound);

#define getPort(p1x, p2x) ((p1x * 256) + p2x)

static int ssend(int socket, const char *str)
{
	return send(socket, str, strlen(str), 0);
}

static void logmesocket(char *msg){
	unsigned int facak = 0;
	char buffer[64];
	snprintf(buffer, 64, "SOCKET - %s\n", msg);

	sys_tty_write(SYS_TTYP_USER5, buffer, strlen(buffer), &facak);
}

static void logmesocket2(char *msg, int i){
	char buffik[64];
	snprintf(buffik, 64, "%s - %i", msg, i);
	logmesocket(buffik);
}

static int connect_to_server_ex(const char *server_ip, u16 port, u8 rcv_timeout)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if(s < 0)
	{
		return FAILED;
	}

	struct sockaddr_in sin;
	socklen_t sin_len = sizeof(sin);
	_memset(&sin, sin_len);

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
			return FAILED;
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
		return FAILED;
	}

	tv.tv_sec = 60;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	//setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	return s;
}

static int connectudp_to_server_ex(const char *server_ip, u16 port, u8 rcv_timeout) // TODO: Refactor ugly hack
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if(s < 0)
	{
		return FAILED;
	}

	struct sockaddr_in sin;
	socklen_t sin_len = sizeof(sin);
	_memset(&sin, sin_len);

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
			return FAILED;
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
		return FAILED;
	}

	tv.tv_sec = 60;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	//setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	return s;
}

static int connect_to_server(const char *server_ip, u16 port)
{
	return connect_to_server_ex(server_ip, port, false);
}

static int connectudp_to_server(const char *server_ip, u16 port) // TODO: Refactor ugly hack
{
	return connectudp_to_server_ex(server_ip, port, false);
}

static int slisten(int port, int backlog)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if(s < 0)
	{
		if(sys_net_errno == SYS_NET_EMFILE){
			//logmesocket("No free space in socket table!!!");
		}
		//logmesocket2("Listen socket create failed!!! Port", port);
		return FAILED;
	}

	//int optval = 1;
	//setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	//setsockopt(s, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	//setsockopt(s, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));

	struct sockaddr_in sa;
	socklen_t sin_len = sizeof(sa);
	_memset(&sa, sin_len);

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(s, (struct sockaddr *)&sa, sin_len) < 0){
		if(sys_net_errno == SYS_NET_EADDRINUSE){
			//logmesocket2("listen bind addr in use!!! Port", port);
		}
		//logmesocket("listen Bind failed!!!");
		shutdown(s, SHUT_RDWR); // PATCH for BUG 1
		socketclose(s); 
		return FAILED;
	}
	if(listen(s, backlog) < 0){
		if(sys_net_errno == SYS_NET_EBADF){
			//logmesocket(" listen bad socket number!!!");
		}
		//logmesocket("listen failed!!!");
		shutdown(s, SHUT_RDWR); // PATCH for BUG 1
		socketclose(s); 
		return FAILED;
	}

	return s;
}

static void sclose(int *socket_e)
{
	if(*socket_e != NONE)
	{
		shutdown(*socket_e, SHUT_RDWR);
		socketclose(*socket_e);
		*socket_e = NONE;
	}
}

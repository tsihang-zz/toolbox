#ifndef SOCKET_H
#define SOCKET_H

#define HAVE_IPV6

/* Sockunion address string length.  Same as INET6_ADDRSTRLEN. */
#define SU_ADDRSTRLEN 46

union oryx_sockunion   {

  struct sockaddr sa;

  struct sockaddr_in sin;
  
#ifdef HAVE_IPV6
  struct sockaddr_in6 sin6;
#endif /* HAVE_IPV6 */

};

#define sockunion_family(X)  ((X)->sa.sa_family)
#define sockunion2ip(X)      ((X)->sin.sin_addr.s_addr)

extern int oryx_sock_bind (
		int sock,
		union oryx_sockunion *su,
		unsigned short port, 
		union oryx_sockunion *su_addr);
extern int oryx_sock_connect (
				int fd,
				const union oryx_sockunion *peersu,
				unsigned short port);

struct oryx_server_t {
	const char		*name;
	uint16_t		listen_port;
	int			family;
	int			(*handler)(const char *in_msg, uint32_t in_msg_lenght, void *opqua);
	uint32_t		ul_flags;

#define	ORYX_SERVER_INIT_VAL	{\
		.name			=	NULL,\
		.listen_port		=	NULL,\
		.family			=	NULL,\
		.handler		=	NULL,\
		.ul_flags		=	NULL,\
	}

};

struct oryx_client_t {
	const char		*name;
	uint16_t		connect_port;
	int			family;
	int			(*handler)(const char *in_msg, uint32_t in_msg_lenght, void *opqua);
	uint32_t		ul_flags;

#define	ORYX_CLIENT_INIT_VAL	{\
		.name			=	NULL,\
		.listen_port		=	NULL,\
		.family			=	NULL,\
		.handler		=	NULL,\
		.ul_flags		=	NULL,\
	}

};

#endif

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
#define	BUF_LENGTH	1024

struct oryx_sock_handle_t {
	int				sock,
					rx_buf_length,
					tx_buf_length;
	char			rx_buf[BUF_LENGTH],
					tx_buf[BUF_LENGTH];
};

struct oryx_server_t {
	const char		*name;
	char			rx_buf[BUF_LENGTH];
	int				rx_buf_length;
	char			tx_buf[BUF_LENGTH];
	int				tx_buf_length;

	uint16_t		listen_port;
	int				listen_sock;
	int				sock;
	int			family;
	int			(*handler)(void *instance, void *opaque);
	uint32_t		ul_flags;

	fd_set			rfds;		/* Read FD set */
	fd_set			wfds;		/* Write FD set */
	
#define	ORYX_SERVER_INIT_VAL	{\
		.name			=	NULL,\
		.rx_buf 		=	{0},\
		.rx_buf_length	=	BUF_LENGTH,\
		.tx_buf 		=	{0},\
		.tx_buf_length	=	BUF_LENGTH,\
		.sock			=	-1,\
		.listen_sock	=	NULL,\
		.listen_port	=	NULL,\
		.family			=	NULL,\
		.handler		=	NULL,\
		.ul_flags		=	NULL,\
	}

};

#define TO_SERVER_CONNECTED	(1 << 0)
#define TO_SERVER_RESET		(1 << 1)
struct oryx_client_t {

	const char		*name;
	int				sock;
	
	char			rx_buf[BUF_LENGTH];
	int				rx_buf_length;
	char			tx_buf[BUF_LENGTH];
	int				tx_buf_length;

	uint16_t		connect_port;
	const char		*connect_addr;
	int			family;
	int			(*handler)(void *instance, void *opaque);
	uint32_t		ul_flags;

#define	ORYX_CLIENT_INIT_VAL	{\
		.name			=	NULL,\
		.sock			=	-1,\
		.rx_buf			=	{0},\
		.rx_buf_length	=	BUF_LENGTH,\
		.tx_buf 		=	{0},\
		.tx_buf_length	=	BUF_LENGTH,\
		.connect_port	=	NULL,\
		.connect_addr	=	NULL,\
		.family			=	NULL,\
		.handler		=	NULL,\
		.ul_flags		=	NULL,\
	}

};

#define CLIENT_RESET(c)\
	((c)->ul_flags |= TO_SERVER_RESET)

#endif

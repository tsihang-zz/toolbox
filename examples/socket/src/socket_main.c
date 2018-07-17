#include "oryx.h"

static int server_handler (void *instance,
		void		__oryx_unused_param__ *opaque)
{
	struct oryx_server_t *s = (struct oryx_server_t *)instance;
	int nb_rx_length, nb_tx_length;

	while (1) {
		/* Wait for data from client */
		nb_rx_length = read(s->sock, s->rx_buf, 64);
		if (nb_rx_length == -1) {
			perror("read()");
			close(s->sock);
			return -1;
		}
	 
		/* Send response to client */
		nb_tx_length = write(s->sock, s->rx_buf, nb_rx_length);
		if (nb_tx_length == -1) {
			perror("write()");
			close(s->sock);
			return -1;
		}
	}
}

static int client_handler (void *instance,
		void		__oryx_unused_param__ *opaque)
{
	struct oryx_client_t *c = (struct oryx_client_t *)instance;
	int nb_rx_length, nb_tx_length;

	c->tx_buf_length	=	strlen("hello world");
	memcpy(c->tx_buf, "hello world", c->tx_buf_length);
	
	while (1) {
		if (c->ul_flags & TO_SERVER_RESET) {
			close(c->sock);
			c->ul_flags &= ~TO_SERVER_CONNECTED;
			break;
		}
		
		/* Send response to client */
		nb_tx_length = write(c->sock, c->tx_buf, c->tx_buf_length);
		if (nb_tx_length == -1) {
			CLIENT_RESET(c);
			continue;
		}
		/* Wait for data from client */
		nb_rx_length = read(c->sock, c->rx_buf, BUF_LENGTH);
		if (nb_rx_length == -1) {
			CLIENT_RESET(c);
			continue;
		}
		printf("rx_from_server -> %s\n", c->rx_buf);
		sleep(1);
	}

	return 0;
}

struct oryx_client_t client = {
	.name			=	"IPv6 Client",
	.family 		=	AF_INET6,
	.connect_port	=	7002,
	.connect_addr	=	"::1",
	.handler		=	client_handler,
};

struct oryx_server_t server = {
	.name			=	"IPv6 Server",
	.family 		=	AF_INET6,
	.listen_port	=	7002,
	.handler		=	server_handler,
};

static __oryx_always_inline__
void * server_thread (void *r)
{
	struct oryx_server_t *s = (struct oryx_server_t *)r;
	struct sockaddr_in6 sa, ca;
	socklen_t ca_length;
	char str_addr[INET6_ADDRSTRLEN];
	int ret, flag;
 
	/* Create socket for listening (client requests) */
	s->listen_sock = socket(s->family, SOCK_STREAM, IPPROTO_TCP);
	if(s->listen_sock == -1) {
		perror("socket()");
		goto finish;
	}
 
	/* Set socket to reuse address */
	flag = 1;
	ret = setsockopt(s->listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	if(ret == -1) {
		perror("setsockopt()");
		goto finish;
	}

	sa.sin6_family	= s->family;
	sa.sin6_addr	= in6addr_any;
	sa.sin6_port	= htons(s->listen_port);
 
	/* Bind address and socket together */
	ret = bind(s->listen_sock, (struct sockaddr*)&sa, sizeof(sa));
	if(ret == -1) {
		perror("bind()");
		close(s->listen_sock);
		goto finish;
	}
 
	/* Create listening queue (client requests) */
	ret = listen(s->listen_sock, 10);
	if (ret == -1) {
		perror("listen()");
		close(s->listen_sock);
		goto finish;
	}
 
	ca_length = sizeof(ca);
 
	while(1) {
		/* Do TCP handshake with client */
		s->sock = accept(s->listen_sock,
				(struct sockaddr*)&ca,
				&ca_length);
		if (s->sock == -1) {
			perror("accept()");
			continue;
		}
 
		inet_ntop(s->family, &ca.sin6_addr,
				str_addr, sizeof(str_addr));
		printf("New connection from: %s:%d ...\n",
				str_addr,
				ntohs(ca.sin6_port));
 
		if(s->handler)
			s->handler(s, NULL);
		
	}

finish:

	oryx_task_deregistry_id(pthread_self());
 
	return NULL;

}


static __oryx_always_inline__
void * client_thread (void *r)
{
	int ret;
	struct sockaddr_in6 sa;
	struct oryx_client_t *c = (struct oryx_client_t *)r;
 
	/* Create socket for communication with server */
	if ((c->sock = socket (
						c->family,
						SOCK_STREAM,
						IPPROTO_TCP)) == -1) {
		perror("socket()");
		goto finish;
	}
 
	/* Connect to server running on localhost */
	sa.sin6_family	=	c->family;
	sa.sin6_port	=	htons(c->connect_port);
	inet_pton(c->family, c->connect_addr, &sa.sin6_addr);

	do {
		ret = connect(c->sock, (struct sockaddr*)&sa, sizeof(sa));
		/* Try to do TCP handshake with server */
		if (ret == -1) {
			sleep(1);
		} else {
			c->ul_flags |= TO_SERVER_CONNECTED;
		}
	} while(ret == -1);

	FOREVER {
		if (c->ul_flags & TO_SERVER_CONNECTED) {
			if (c->handler)
				c->handler(c, NULL);
		} else {
			do {
				ret = connect(c->sock, (struct sockaddr*)&sa, sizeof(sa));
				/* Try to do TCP handshake with server */
				if (ret == -1) {
					sleep(1);
				} else {
					c->ul_flags |= TO_SERVER_CONNECTED;
				}
			} while(ret == -1);
		}
	}

	/* DO TCP teardown */
	close(c->sock);

finish:

	oryx_task_deregistry_id(pthread_self());
 
	return NULL;
}



static struct oryx_task_t client_task =
{
	.module		= THIS,
	.sc_alias	= "Client handler Task",
	.fn_handler	= client_thread,
	.ul_lcore	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 1,
	.argv		= &client,
	.ul_flags	= 0,	/** Can not be recyclable. */
};


static struct oryx_task_t server_task =
{
	.module		= THIS,
	.sc_alias	= "Server handler Task",
	.fn_handler	= server_thread,
	.ul_lcore	= INVALID_CORE,
	.ul_prio	= KERNEL_SCHED,
	.argc		= 1,
	.argv		= &server,
	.ul_flags	= 0,	/** Can not be recyclable. */
};

int main (
	int		__oryx_unused_param__	argc,
	char	__oryx_unused_param__	** argv
)
{
	oryx_initialize();


	oryx_task_registry(&server_task);
	
	oryx_task_registry(&client_task);

	oryx_task_launch();

	FOREVER;

	return 0;
}


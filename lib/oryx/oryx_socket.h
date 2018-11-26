#ifndef ORYX_SOCKET_H
#define ORYX_SOCKET_H

#include "socket.h"

ORYX_DECLARE (
	int oryx_sock_bind (
		IN int sock,
		IN union oryx_sockunion *su,
		IN unsigned short port, 
		IN union oryx_sockunion *su_addr
	)
);

ORYX_DECLARE (
	int oryx_sock_connect (
		IN int fd,
		IN const union oryx_sockunion *peersu,
		IN unsigned short port
	)
);

#endif
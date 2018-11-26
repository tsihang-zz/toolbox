#include "oryx.h"

/* return oryx_sockunion structure : this function should be revised. */
static const char *
sockunion_log (const union oryx_sockunion *su, char *buf, size_t len)
{
  switch (su->sa.sa_family) 
    {
    case AF_INET:
      return inet_ntop(AF_INET, &su->sin.sin_addr, buf, len);
#ifdef HAVE_IPV6
    case AF_INET6:
      return inet_ntop(AF_INET6, &(su->sin6.sin6_addr), buf, len);
      break;
#endif /* HAVE_IPV6 */
    default:
      snprintf (buf, len, "af_unknown %d ", su->sa.sa_family);
      return buf;
    }
}

/* Return sizeof union oryx_sockunion.  */
static int
sockunion_sizeof (const union oryx_sockunion *su)
{
  int ret;

  ret = 0;
  switch (su->sa.sa_family)
    {
    case AF_INET:
      ret = sizeof (struct sockaddr_in);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      ret = sizeof (struct sockaddr_in6);
      break;
#endif /* AF_INET6 */
    }
  return ret;
}

/* Bind socket to specified address. */
int oryx_sock_bind (
		int sock,
		union oryx_sockunion *su,
		unsigned short port, 
		union oryx_sockunion *su_addr)
{
  int size = 0;
  int err;

  if (su->sa.sa_family == AF_INET)
    {
      size = sizeof (struct sockaddr_in);
      su->sin.sin_port = htons (port);
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
      su->sin.sin_len = size;
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
      if (su_addr == NULL)
	sockunion2ip (su) = htonl (INADDR_ANY);
    }
#ifdef HAVE_IPV6
  else if (su->sa.sa_family == AF_INET6)
    {
      size = sizeof (struct sockaddr_in6);
      su->sin6.sin6_port = htons (port);
#ifdef SIN6_LEN
      su->sin6.sin6_len = size;
#endif /* SIN6_LEN */
      if (su_addr == NULL)
	{
#ifdef LINUX_IPV6
	  memset (&su->sin6.sin6_addr, 0, sizeof (struct in6_addr));
#else
	  su->sin6.sin6_addr = in6addr_any;
#endif /* LINUX_IPV6 */
	}
    }
#endif /* HAVE_IPV6 */
  

  err = bind (sock, (struct sockaddr *)su, size);
  if (err)
    oryx_logw(-1, "can't bind socket : %s", oryx_safe_strerror (errno));

  return err;
}

int oryx_sock_connect (int fd, const union oryx_sockunion *peersu, unsigned short port)
{
	int err;
	int val;
	union oryx_sockunion su;

	memcpy (&su, peersu, sizeof (union oryx_sockunion));

	switch (su.sa.sa_family)
	{
		case AF_INET:
		  su.sin.sin_port = port;
		  break;
#ifdef HAVE_IPV6
		case AF_INET6:
		  su.sin6.sin6_port  = port;
		  break;
#endif /* HAVE_IPV6 */
	}	   

	/* Make socket non-block. */
	val = fcntl (fd, F_GETFL, 0);
	fcntl (fd, F_SETFL, val|O_NONBLOCK);

	/* Call connect function. */
	err = connect (fd, (struct sockaddr *) &su, sockunion_sizeof (&su));

	/* Immediate success */
	if (err == 0)
	{
	  fcntl (fd, F_SETFL, val);
	  return 0;
	}

	/* If connect is in progress then return 1 else it's real error. */
	if (err < 0)
	{
	  if (errno != EINPROGRESS)
	{
	  char str[SU_ADDRSTRLEN];
	  oryx_logi("can't connect to %s fd %d : %s",
			 sockunion_log (&su, str, sizeof str),
			 fd, safe_strerror (errno));
	  return -1;
	}
	}

	fcntl (fd, F_SETFL, val);

	return -1;
}

/* Make socket from sockunion union. */
int
oryx_sockunion_stream_socket (union oryx_sockunion *su)
{
  int sock;

  if (su->sa.sa_family == 0)
    su->sa.sa_family = AF_INET6;

  sock = socket (su->sa.sa_family, SOCK_STREAM, 0);

  if (sock < 0)
    oryx_loge(-1, "can't make socket sockunion_stream_socket");

  return sock;
}

int
oryx_sockopt_reuseaddr (int sock)
{
  int err;
  int on = 1;

  err = setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, 
		    (void *) &on, sizeof (on));
  if (err < 0)
    {
      oryx_loge(-1, "can't set sockopt SO_REUSEADDR to socket %d", sock);
      return -1;
    }
  return 0;
}


/* Bind socket to specified address. */
int
oryx_sockunion_bind (int sock, union oryx_sockunion *su, unsigned short port, 
		union sockunion *su_addr)
{
  int size = 0;
  int err;

  if (su->sa.sa_family == AF_INET)
    {
      size = sizeof (struct sockaddr_in);
      su->sin.sin_port = htons (port);
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
      su->sin.sin_len = size;
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
      if (su_addr == NULL)
	sockunion2ip (su) = htonl (INADDR_ANY);
    }
#ifdef HAVE_IPV6
  else if (su->sa.sa_family == AF_INET6)
    {
      size = sizeof (struct sockaddr_in6);
      su->sin6.sin6_port = htons (port);
#ifdef SIN6_LEN
      su->sin6.sin6_len = size;
#endif /* SIN6_LEN */
      if (su_addr == NULL)
	{
#ifdef LINUX_IPV6
	  memset (&su->sin6.sin6_addr, 0, sizeof (struct in6_addr));
#else
	  su->sin6.sin6_addr = in6addr_any;
#endif /* LINUX_IPV6 */
	}
    }
#endif /* HAVE_IPV6 */
  

  err = bind (sock, (struct sockaddr *)su, size);
  if (err < 0)
    oryx_loge(-1, "can't bind socket : %s", oryx_safe_strerror (errno));

  return err;
}


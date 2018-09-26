/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ocd_tcpip.cpp,v 1.4 2005/10/20 18:39:37 jnekl Exp $
 *
 * This is the tcpip ocd connection.
 */

#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<errno.h>
#include	<assert.h>
#include	"xmalloc.h"

#ifndef	_WIN32
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<arpa/inet.h>
#include	<netdb.h>
#else	/* _WIN32 */
#include	<winsock2.h>
#endif

#include	"md5.h"
#include	"sockstream.h"
#include	"ocd_tcpip.h"

#include	"err_msg.h"

/**************************************************************/

#define	DEFAULT_PORT	6910

/**************************************************************
 * Constructor for ocd_tcpip class.
 */

ocd_tcpip::ocd_tcpip(void)
{
#ifdef	_WIN32
	int err;
	WSADATA wsa;
#endif

	open = 0;
	up = 0;
	s = NULL;

	version_major = version_minor = 0;

	buff = (char *)xmalloc(BUFSIZ);

#ifdef	_WIN32
	err = WSAStartup(MAKEWORD(2,1), &wsa);
	if(err) {
		strncpy(err_msg, 
		    "Could not initialize windows socket environment\n"
		    "WSAStartup failed\n", err_len-1);
		throw err_msg;
	}
#endif

	return;
}

/**************************************************************
 * Destructor for ocd_tcpip class.
 */

ocd_tcpip::~ocd_tcpip(void)
{
#ifdef	_WIN32
	int err;
#endif
	if(s) {
		ss_printf(s, "CLOSE\r\n");
		ss_flush(s);
		ss_close(s);
		delete s;
		s = NULL;
	}

	if(buff) {
		free(buff);
		buff = NULL;
	}

#ifdef	_WIN32
	err = WSACleanup();
	if(err) {
		strncpy(err_msg, "Close windows socket environment failed\n"
		    "WSACleanup failed\n", err_len-1);
		throw err_msg;
	}
#endif

	return;
}

/**************************************************************/

void ocd_tcpip::connect_server(char *host)
{
	int err;
	int tcp_port, fd;
	char *port, *tail;
	struct hostent *h;
	struct sockaddr_in sock;

	if(host && *host != '\0') {
		port = strchr(host, ':');
		if(port) {
			*port = '\0';
			port++;
		}
	} else {
		port = NULL;
	}

	sock.sin_family = AF_INET;
		
	if(host && *host != '\0') {
		h = gethostbyname(host);
		if(!h) {
			snprintf(err_msg, err_len-1,
			    "Connection to server failed\n"
			    "gethostbyname:%s\n", strerror(errno)); 
			throw err_msg;
		}
		sock.sin_addr.s_addr = *((uint32_t *)(h->h_addr));
	} else {
		sock.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}

	if(port && *port != '\0') {
		tcp_port = strtol(port, &tail, 0);
		if(tail == NULL || tail == port || *tail != '\0') {
			snprintf(err_msg, err_len-1,
			    "Connection to server failed\n"
			    "invalid port \'%s\'\n", port);
			throw err_msg;
		}
		if(tcp_port < 0 || tcp_port > 65536) {
			snprintf(err_msg, err_len-1,
			    "Connection to server failed\n"
			    "port %d out of range\n", tcp_port);
			throw err_msg;
		}
	} else {
		tcp_port = DEFAULT_PORT;
	}

	sock.sin_port = htons(tcp_port);

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if(fd < 0) {
		snprintf(err_msg, err_len-1, 
		    "Connection to server failed\n"
		    "socket:%s\n", strerror(errno));
		throw err_msg;
	}

	err = ::connect(fd, (struct sockaddr*)&sock, sizeof(sock));
	if(err) {
		close(fd);
		snprintf(err_msg, err_len-1,
		    "Connection to server failed\n"
		    "connect:%s\n", strerror(errno));
		throw err_msg;
	}

	err = ss_open(fd, s);
	if(err) {
		close(fd);
		snprintf(err_msg, err_len-1,
		    "Connection to server failed\n"
		    "ss_open:%s\n", strerror(errno));
		throw err_msg;
	}

	return;
}

/**************************************************************
 * This will verify we are connected to a "Z8ENCOREOCD" server.
 */

void ocd_tcpip::validate_server(void)
{
	char *ptr, *next, *tail;

	ptr = ss_gets(buff, BUFSIZ, s);
	if(!ptr) {
		snprintf(err_msg, err_len-1,
		    "Check server version failed\n"
		    "recv:%s\n", strerror(errno));
		throw err_msg;
	}
	ptr = strchr(buff, '#');
	if(ptr) {
		*ptr = '\0';
	}
	ptr = strtok(buff, " \t\r\n");
	if(!ptr) {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}
	if(strcasecmp(ptr, "+OK")) {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}
	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}
	if(strcasecmp(ptr, "Z8ENCOREOCD")) {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}
	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}

	next = strchr(ptr, '.');
	if(!next) {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}
	*next = '\0';
	next++;

	version_major = strtol(ptr, &tail, 10);
	if(tail == NULL || tail == ptr || *tail != '\0') {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}
	version_minor = strtol(next, &tail, 10);
	if(tail == NULL || tail == ptr || *tail != '\0') {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}

	if(version_major > 1) {
		snprintf(err_msg, err_len-1, "Check server version failed\n"
		    "version %d.%02d unknown\n",
		    version_major, version_minor);
		throw err_msg;
	}
		
	ptr = strtok(NULL, " \t\r\n");
	if(ptr) {
		strncpy(err_msg, "Check server version failed\n"
		    "invalid server version string\n", err_len-1);
		throw err_msg;
	}

	return;
}

/**************************************************************
 * This will check if the server requires authentication. If
 * authentication is required, it will authenticate using
 * md5 hashes.
 */

void ocd_tcpip::auth_server(char *user)
{
	int err, i;
	char *ptr, *pass;
	uint8_t challenge[16], password[16];
	MD5_CTX context;

	err = ss_printf(s, "STATUS\r\n");
	if(err < 0) {
		snprintf(err_msg, err_len-1, 
		    "Failed authenticating to server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}
	err = ss_flush(s);
	if(err < 0) {
		snprintf(err_msg, err_len-1,
		    "Failed authenticating to server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	/* eat CRLF until we get a response */
	do {
		ptr = ss_gets(buff, BUFSIZ, s);
		if(!ptr) {
			snprintf(err_msg, err_len-1,
			    "Failed authenticating to server\n"
			    "recv:%s\n", strerror(errno));
			throw err_msg;
		}
		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}
		ptr = strtok(buff, " \t\r\n");
		if(!ptr) {
			continue;
		}

	} while(!ptr);
	
	if(strcasecmp(ptr, "+OK")) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	if(strcasecmp(ptr, "AUTH")) {
		return;
	}

	ptr = strtok(NULL, " \t\r\n");
	if(ptr) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	if(user) {
		pass = strchr(user, ':');
		if(pass) {
			*pass = '\0';
			pass++;
		}
	} else {
		pass = NULL;
	}

	if(!user || *user == '\0' || !pass || *pass == '\0') {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "need username/password\n", err_len-1);
		throw err_msg;
	}
	if(!strncasecmp(pass, "md5=", 4)) {
		if(MD5StrToDigest(pass+4, password)) {
			strncpy(err_msg, "Failed authenticating to server\n"
			    "invalid md5 digest\n", err_len-1);
			throw err_msg;
		}
	} else {
		MD5Init(&context);
		MD5Update(&context, (unsigned char *)pass, strlen(pass));
		MD5Final(password, &context);
	}

	err = ss_printf(s, "USER %s AUTH MD5\r\n", user);
	if(err < 0) {
		snprintf(err_msg, err_len-1,
		    "Failed authenticating to server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}
	err = ss_flush(s);
	if(err < 0) {
		snprintf(err_msg, err_len-1,
		    "Failed authenticating to server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	/* eat CRLF until we get a response */
	do {
		ptr = ss_gets(buff, BUFSIZ, s);
		if(!ptr) {
			snprintf(err_msg, err_len-1,
			    "Failed authenticating to server\n"
			    "recv:%s\n", strerror(errno));
			throw err_msg;
		}
		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}
		ptr = strtok(buff, " \t\r\n");
		if(!ptr) {
			continue;
		}
	} while(!ptr);
	
	if(strcasecmp(ptr, "+OK")) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	if(strcasecmp(ptr, "CHALLENGE")) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	if(MD5StrToDigest(ptr, challenge)) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	ptr = strtok(NULL, " \t\r\n");
	if(ptr) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	MD5Init(&context);
	MD5Update(&context, challenge, 16);
	MD5Update(&context, password, 16);
	MD5Final(challenge, &context);

	for(i=0; i<16; i++) {
		err = ss_printf(s, "%02X", challenge[i]);
		if(err < 0) {
			snprintf(err_msg, err_len-1,
			    "Failed authenticating to server\n"
			    "send:%s\n", strerror(errno));
			throw err_msg;
		}
	}
	err = ss_printf(s, "\r\n");
	if(err < 0) {
		snprintf(err_msg, err_len-1,
		    "Failed authenticating to server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}
	err = ss_flush(s);
	if(err < 0) {
		snprintf(err_msg, err_len-1,
		    "Failed authenticating to server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	/* eat CRLF until we get a response */
	do {
		ptr = ss_gets(buff, BUFSIZ, s);
		if(!ptr) {
			snprintf(err_msg, err_len-1,
			    "Failed authenticating to server\n"
			    "recv:%s\n", strerror(errno));
			throw err_msg;
		}
		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}
		ptr = strtok(buff, " \t\r\n");
		if(!ptr) {
			continue;
		}

	} while(!ptr);
	
	if(strcasecmp(ptr, "+OK")) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "bad username/password\n", err_len-1);
		throw err_msg;
	}

	ptr = strtok(NULL, " \t\r\n");
	if(ptr) {
		strncpy(err_msg, "Failed authenticating to server\n"
		    "protocol error\n", err_len-1);
		throw err_msg;
	}

	return;
}

/**************************************************************/

void ocd_tcpip::connect(const char *device)
{
	char *ptr, *userpasswd, *host;

	if(s) {
		strncpy(err_msg, "Failed connecting to server\n"
		    "already connected\n", err_len-1);
		throw err_msg;
	}

	ptr = NULL;
	if(device && *device != '\0') {
		ptr = xstrdup(device);
		host = strchr(ptr, '@');
		if(host) {
			userpasswd = ptr;
			*host = '\0';
			host++;
		} else {
			host = ptr;
			userpasswd = NULL;
		}
	} else {
		userpasswd = NULL;
		host = NULL;
	}

	s = (SOCK *)xmalloc(sizeof(SOCK));

	try {
		connect_server(host);
		try {
			validate_server();
			auth_server(userpasswd);
		} catch(char *err) {
			ss_close(s);
			throw err;
		}

	} catch(char *err) {
		free(s);
		s = NULL;
		if(ptr) {
			free(ptr);
		}
		throw err;
	}

	if(ptr) {
		free(ptr);
	}

	open = 1;

	return;
}

/**************************************************************/

void ocd_tcpip::reset(void)
{
	int err;
	char *ptr;

	if(!s) {
		strncpy(err_msg, "Could not reset on-chip debugger link\n"
		    "socket not open\n", err_len-1);
		throw err_msg;
	}
	if(!open) {
		strncpy(err_msg, "Could not reset on-chip debugger link\n"
		    "communication with server is down\n", err_len-1);
		throw err_msg;
	}

	up = 0;

	err = ss_printf(s, "RESET\r\n");
	if(err < 0) {
		open = 0;
		snprintf(err_msg, err_len-1,
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}
	err = ss_flush(s);
	if(err) {
		open = 0;
		snprintf(err_msg, err_len-1,
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	/* eat blank lines */
	do {
		ptr = ss_gets(buff, BUFSIZ, s);
		if(!ptr) {
			open = 0;
			snprintf(err_msg, err_len-1,
			    "Failed communicating with server\n"
			    "recv:%s\n", strerror(errno));
			throw err_msg;
		}
		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}
		ptr = strtok(buff, " \t\r\n");
		if(!ptr) {
			continue;
		}

	} while(!ptr);

	if(!strcasecmp(ptr, "+OK")) {
		ptr = strtok(NULL, " \t\r\n");
		if(ptr) {
			open = 0;
			strncpy(err_msg,
			    "Failed communicating with server\n"
			    "protocol error: garbage after response\n", 
			    err_len-1);
			throw err_msg;
		}
		up = 1;
		return;
	} else if(!strcasecmp(ptr, "-ERR")) {
		ptr = strtok(NULL, " \t\r\n");
		if(ptr) {
			open = 0;
			strncpy(err_msg,
			    "Failed communicating with server\n"
			    "protocol error: garbage after response\n", 
			    err_len-1);
			throw err_msg;
		}
		strncpy(err_msg, "Failed resetting on-chip debugger link\n"
		    "remote link failure\n", err_len-1);
		throw err_msg;
	} else {
		open = 0;
		strncpy(err_msg,
		    "Failed communicating with server\n"
		    "protocol error: invalid response\n", err_len-1);
		throw err_msg;
	}
}

/**************************************************************/

bool ocd_tcpip::link_up(void)
{
	int err;
	char *ptr;

	if(!s) {
		strncpy(err_msg, "Could not determine link status\n"
		    "socket is not open\n", err_len-1);
		throw err_msg;
	}
	if(!open) {
		strncpy(err_msg, "Could not determine link status\n"
		    "communication with server is down\n", err_len-1);
		throw err_msg;
	}

	err = ss_printf(s, "STATUS\r\n");
	if(err < 0) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	err = ss_flush(s);
	if(err) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	/* eat blank lines */
	do {
		ptr = ss_gets(buff, BUFSIZ, s);
		if(!ptr) {
			open = 0;
			snprintf(err_msg, err_len-1,
			    "Failed communicating with server\n"
			    "recv:%s\n", strerror(errno));
			throw err_msg;
		}
		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}
		ptr = strtok(buff, " \t\r\n");
		if(!ptr) {
			continue;
		}

	} while(!ptr);
	
	if(!strcasecmp(ptr, "+OK")) {
		ptr = strtok(NULL, " \t\r\n");
		if(!ptr) {
			open = 0; 
			strncpy(err_msg, "Failed communicating with server\n"
			    "protocol error: no status\n", err_len-1);
			throw err_msg;
		}
		if(strcasecmp(ptr, "UP") == 0) {
			up = 1;
			return 1;
		} else if(strcasecmp(ptr, "DOWN") == 0) {
			up = 0;
			return 0;
		} else {
			open = 0; 
			strncpy(err_msg, "Failed communicating with server\n"
			    "protocol error: invalid status\n", err_len-1);
			throw err_msg;
		}
	} else if(!strcasecmp(ptr, "-ERR")) {
		open = 0; 
		strncpy(err_msg, "Failed communicating with server\n"
		    "protocol error: failed retrieving status\n", err_len-1);
		throw err_msg;
	} else {
		open = 0; 
		strncpy(err_msg, "Failed communicating with server\n"
		    "protocol error: invalid response\n", err_len-1);
		throw err_msg;
	}
}

/**************************************************************/

bool ocd_tcpip::link_open(void)
{
	return open;
}

/**************************************************************/

void ocd_tcpip::read(uint8_t *data, size_t size)
{
	int err;
	char *ptr, *tail;

	assert(data != NULL);
	assert(size != 0);

	if(!s) {
		strncpy(err_msg, "Could not read from on-chip debugger\n"
		    "socket is not open\n", err_len-1);
		throw err_msg;
	}
	if(!open) {
		strncpy(err_msg, "Could not read from on-chip debugger\n"
		    "communication with server is down\n", err_len-1);
		throw err_msg;
	}
	if(!up) {
		strncpy(err_msg, "Could not read from on-chip debugger\n"
		    "link needs reset first\n", err_len-1);
		throw err_msg;
	}

	err = ss_printf(s, "READ 0x%04X\r\n", size);
	if(err < 0) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}
	err = ss_flush(s);
	if(err) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	ptr = ss_gets(buff, BUFSIZ, s);
	if(!ptr) {
		open = 0;
		snprintf(err_msg, err_len-1,
		    "Failed communicating with server\n"
		    "recv:%s\n", strerror(errno));
		throw err_msg;
	}

	ptr = strchr(buff, '#');
	if(ptr) {
		*ptr = '\0';
	}

	ptr = strtok(buff, " \t\r\n");
	if(!strcasecmp(ptr, "-ERR")) {
		up = 0;
		strncpy(err_msg, "Failed reading from on-chip debugger\n"
		    "remote link failure\n", err_len-1);
		throw err_msg;
	} else if(strcasecmp(ptr, "+OK")) {
		open = 0;
		strncpy(err_msg, "Failed communicating with server\n"
		    "protocol error: invalid response\n", err_len-1);
		throw err_msg;
	}

	do {
		if(ptr) {
			ptr = strtok(NULL, " \t\r\n");
		} 
		if(!ptr) {
			ptr = ss_gets(buff, BUFSIZ, s);
			if(!ptr) {
				open = 0;
				snprintf(err_msg, err_len-1,
				    "Failed communicating with server\n"
				    "recv:%s\n", strerror(errno));
				throw err_msg;
			}

			ptr = strchr(buff, '#');
			if(ptr) {
				*ptr = '\0';
			}
			ptr = strtok(buff, " \t\r\n");
		} 

		if(ptr) {
			*data++ = strtoul(ptr, &tail, 0);
			size--;
			if(!tail || tail == ptr || *tail != '\0') {
				open = 0;
				strncpy(err_msg, 
				    "Failed communicating with server\n"
				    "protocol error: invalid data\n", 
				    err_len-1);
				throw err_msg;
			}
		}
	} while(size > 0 && ptr);

	if(size > 0) {
		open = 0;
		strncpy(err_msg, "Failed communicating with server\n"
		    "protocol error: returned size incorrect\n", err_len-1);
		throw err_msg;
	}

	return;
}

/**************************************************************/

void ocd_tcpip::write(const uint8_t *data, size_t size)
{
	int err;
	char *ptr;
	size_t i;

	assert(data != NULL);
	assert(size != 0);

	if(!s) {
		strncpy(err_msg, "Could not write to on-chip debugger\n"
		    "socket is not open\n", err_len-1);
		throw err_msg;
	}
	if(!open) {
		strncpy(err_msg, "Could not write to on-chip debugger\n"
		    "communication with server is down\n", err_len-1);
		throw err_msg;
	}
	if(!up) {
		strncpy(err_msg, "Could not write to on-chip debugger\n"
		    "link needs reset first\n", err_len-1);
		throw err_msg;
	}

	err = ss_printf(s, "WRITE ");
	if(err < 0) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}
	err = ss_flush(s);
	if(err) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	err = 0;

	for(i=0; i<size; i++) {
		if(i%8 == 0) {
			err = ss_printf(s, "\r\n");
			if(err < 0) {
				break;
			}
		}
		err = ss_printf(s, "0x%02x ", data[i]);
		if(err < 0) {
			break;
		}
	}
	if(err < 0) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	err = ss_printf(s, "\r\n\r\n");
	if(err < 0) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}
	err = ss_flush(s);
	if(err) {
		open = 0;
		snprintf(err_msg, err_len-1, 
		    "Failed communicating with server\n"
		    "send:%s\n", strerror(errno));
		throw err_msg;
	}

	/* eat blank lines */
	do {
		ptr = ss_gets(buff, BUFSIZ, s);
		if(!ptr) {
			open = 0;
			snprintf(err_msg, err_len-1,
			    "Failed communicating with server\n"
			    "recv:%s\n", strerror(errno));
			throw err_msg;
		}

		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}

		ptr = strtok(buff, " \t\r\n");
	} while(!ptr);

	if(!strcasecmp(ptr, "+OK")) {
		ptr = strtok(NULL, " \t\r\n");
		if(ptr) {
			open = 0;
			strncpy(err_msg,
			    "Failed communicating with server\n"
			    "protocol error: garbage after response\n", 
			    err_len-1);
			throw err_msg;
		}
		return;
	} else if(!strcasecmp(ptr, "-ERR")) {
		up = 0;
		ptr = strtok(NULL, " \t\r\n");
		if(ptr) {
			open = 0;
			strncpy(err_msg,
			    "Failed communicating with server\n"
			    "protocol error: garbage after response\n", 
			    err_len-1);
			throw err_msg;
		}
		strncpy(err_msg, "Failed writing to on-chip debugger\n"
		    "remote link failure\n", err_len-1);
		throw err_msg;
	} else {
		open = 0;
		strncpy(err_msg, "Failed communicating with server\n"
		    "protocol error: invalid response\n", err_len-1);
		throw err_msg;
	} 

}

/**************************************************************/

int ocd_tcpip::link_speed(void)
{
	/* TODO: add "LINK SPEED" to tcp/ip protocol */
	return 0;
}

void ocd_tcpip::set_timeout(int)
{
	/* TODO: add "SET TIMEOUT" to tcp/ip protocol */
	return;
}

void ocd_tcpip::set_baudrate(int)
{
	/* TODO: add "SET BAUDRATE" to tcp/ip protocol */
	return;
}

bool ocd_tcpip::available(void)
{
	/* TODO: add "AVAILABLE" command to tcp/ip protocol */
	return 0;
}

bool ocd_tcpip::error(void)
{
	/* TODO: add "ERROR" command to tcp/ip protocol */
	return 0;
}

/**************************************************************/



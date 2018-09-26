/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: server.cpp,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * This is the ocd tcp/ip server. It allows clients to connect
 * remotely and communicate directly with the ez8 ocd link layer.
 */

#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<errno.h>
#include	<time.h>
#include	"xmalloc.h"

#ifndef	_WIN32
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<arpa/inet.h>
#include	<netdb.h>
#else	/* _WIN32 */
#include	<winsock2.h>
typedef int socklen_t;
#endif	/* _WIN32 */

#include	"md5.h"
#include	"sockstream.h"
#include	"server.h"

/**************************************************************/

#define	DEFAULT_PORT	6910

#define	VERSION_MAJOR	1
#define	VERSION_MINOR	0

#define	AUTH_MAGIC	0x69

static uint8_t *data = NULL;
static int data_size = 0;
static char *buff = NULL;

enum auth_type_t { auth_none, auth_plaintext, auth_md5 };

/**************************************************************
 * This function will bind the server to a socket.
 *
 * *connection specifies an alternate interface and/or port
 * to use. If *connection is NULL (typical), the server will 
 * bind to all interfaces on the default port.
 *
 * *connection should be in the format
 *	[host/addr][:port]
 * host/addr can be a symbolic name or an ip address. If omitted,
 * all interfaces on the host are bound to. If :port is omitted,
 * the DEFAULT_PORT is used.
 *
 * This function returns a valid server descriptor (>= 0) upon
 * success, -1 on error.
 */

static int bind_server(char *connection)
{
	int err;
	int fdes, tcp_port;
	char *host, *port, *tail;
	struct hostent *h;
	struct sockaddr_in sock;
#ifdef	DEBUG_REUSE
	char optval[4];
#endif

	/* separate host from port */
	if(connection) {
		host = connection;
		port = strchr(host, ':');
		if(port) {
			*port = '\0';
			port++;
		} 
	} else {
		host = NULL;
		port = NULL;
	}

	sock.sin_family = AF_INET;

	/* If host specified, bind to specific interface. 
	 * *host may be a symbolic name or an ip address, 
	 * gethostbyname() will determine what it is.
	 */
	if(host && *host != '\0') {
		h = gethostbyname(host);
		if(!h) {
			perror("gethostbyname");
			return -1;
		}
		sock.sin_addr.s_addr = *((uint32_t *)(h->h_addr));
	} else {
		sock.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	/* If port specified, bind to alternate port */
	if(port && *port != '\0') {
		tcp_port = strtol(port, &tail, 0);
		if(tail == NULL || tail == port || *tail != '\0') {
			fprintf(stderr, "Invalid port \'%s\'\n", port);
			return -1;
		}
		if(tcp_port < 0 || tcp_port > 65536) {
			fprintf(stderr, "Port %d out-of-range\n", tcp_port);
			return -1;
		}
	} else {
		tcp_port = DEFAULT_PORT;
	}
	sock.sin_port = htons(tcp_port);

	/* create socket to use */	
	fdes = socket(PF_INET, SOCK_STREAM, 0);
	if(fdes < 0) {
		perror("socket");
		return -1;
	}

#ifdef	DEBUG_REUSE
	/* set SO_REUSEADDR option on socket so we don't have
	 *    to wait for TCP TIMEWAIT to expire before we
	 *    can re-bind to same port.
	 */
	err = setsockopt(fdes, SOL_SOCKET, SO_REUSEADDR, optval, 
	    sizeof(optval));
	if(err) {
		perror("setsockopt");
		close(fdes);
		return -1;
	}
#endif

	/* bind to host/port */
	err = bind(fdes, (struct sockaddr *)&sock, sizeof(sock));
	if(err) {
		perror("bind");
		close(fdes);
		return -1;
	}

	/* Start listening for connections. Accept at most one
	 * connection. Subsequent connections will fail with
	 * CONN_REFUSED until current client exits.
	 */
	err = listen(fdes, 1);
	if(err) {
		perror("listen");
		close(fdes);
		return -1;
	}
	
	return fdes;
}

/**************************************************************
 * This will accept a client connection and return a 
 * descriptor to use for the connection.
 * 
 * This function will return a valid descriptor (>= 0) upon
 * success, or -1 upon error.
 */

static int get_connection(int fdes)
{
	int err;
	int client;
	socklen_t len;
	struct sockaddr_in sock;
	struct hostent *h;

	/* print message about interface/socket we are listening on */
	len = sizeof(sock);
	err = getsockname(fdes, (struct sockaddr *)&sock, &len);
	if(err) {
		perror("getsockname");
		return -1;
	}

	if(sock.sin_addr.s_addr == INADDR_ANY ) {
		printf("\nListening on port %d\n", ntohs(sock.sin_port));
	} else {
		printf("\nListening on %s:%d\n", inet_ntoa(sock.sin_addr), 
		    ntohs(sock.sin_port));
	}

	/* accept client connection */
	len = sizeof(sock);
	client = accept(fdes, (struct sockaddr *)&sock, &len);
	if(client < 0) {
		perror("accept");
		return -1;
	}

	/* print message about connected client */
	h = gethostbyaddr((char *)&sock.sin_addr.s_addr, 
	    sizeof(sock.sin_addr.s_addr), AF_INET);
	if(h) {
		printf("Accepted connection from %s:%d\n", h->h_name, 
		    ntohs(sock.sin_port)); 
	} else {
		printf("Accepted connection from %s:%d\n", 
		    inet_ntoa(sock.sin_addr), ntohs(sock.sin_port));
	}

	return client;
}

/**************************************************************
 * This function will flush the input from a socket connection
 * until a blank line is received.
 *
 * This function is only used to recover from client protocol
 * errors. It is only called during a WRITE request for
 * an un-authenticated client, or when invalid data is received
 * for a write request.
 * 
 * This function returns 1 upon success, or -1 if an error
 * occurred when reading/writing the socket.
 */

static int client_flush(SOCK *s)
{
	char *ptr;

	/* eat CRLF to terminate request */
	do {
		ptr = ss_gets(buff, BUFSIZ, s);
		if(!ptr) {
			return -1;
		}
		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}
		ptr = strtok(buff, " \t\r\n");
	} while(ptr);

	return 1;
}
	
/**************************************************************
 * This handles an authentication request from the client
 * (initiated by a USER command).
 *
 * Client should send
 * 	USER <username> AUTH <PLAINTEXT|MD5>
 * If AUTH method is PLAINTEXT, server should respond with 
 * 	+OK
 * If AUTH method is MD5, then the server should respond with
 *	CHALLENGE <128-bit_ascii_hex_challenge>
 * Client should then respond with either the plaintext
 * password, or the MD5 hash of the concatination of the
 * received challenge plus the md5 hash of the users password.
 *
 * The server will then respond with +OK if authentication
 * sucessful, or -ERR if authentication failed.
 *
 * This function will return AUTH_MAGIC if authentication
 * was successful, 0 if authentication was unsuccessful, 1 if 
 * a protocol error occurred, or -1 if a failure occurred while 
 * reading/writing the socket.  
 *
 * NOTE: this function assumes the calling routine will flush
 * the output buffer before reading the next request.
 *
 * *userpasswd should be a comma separated list of 
 * username:password pairs. A password with an empty username
 * signifies a password to be used for any user without a 
 * specified password (for use as a default password).
 * If the password field begins with md5=, then the following
 * text is interpreted as an ascii hex encoded 128-bit md5 hash 
 * of the password.
 */

static int client_auth(SOCK *s, char *userpasswd)
{
	int err;
	int i;
	char *ptr, *user, *pass;
	uint8_t challenge[16], password[16];
	enum auth_type_t auth_type;
	MD5_CTX context;

	auth_type = auth_none;

	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		err = ss_printf(s, "-ERR #missing username \r\n");
		if(err < 0) {
			return -1;
		}
		return 1;
	}

	pass = NULL;
	user = userpasswd;

	/* find user */
	while(user) {
		pass = strchr(user, ':');
		if(!pass) {
			user = NULL;
			break;
		}
		if((int)strlen(ptr) == pass-user) {
			if(strncasecmp(ptr, user, pass-user) == 0) {
				pass++;
				break;
			}
		}
		user = strchr(pass, ',');
		if(user) {
			user++;
		} else {
			pass = NULL;
		}
	}

	/* if user doesn't exist, find empty user if it exists */
	if(!user) {
		user = userpasswd;
		while(user) {
			pass = strchr(user, ':');
			if(!pass) {
				user = NULL;
				break;
			}
			if(user == pass) {
				pass++;
				break;
			}
			user = strchr(pass, ',');
			if(user) {
				user++;
			} else {
				pass = NULL;
			}
		}
	}

	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		err = ss_printf(s, "-ERR #missing auth type\r\n");
		if(err < 0) {
			return -1;
		}
		return 1;
	}
	if(strcasecmp(ptr, "auth") != 0) {
		err = ss_printf(s, "-ERR #invalid auth type\r\n");
		if(err < 0) {
			return -1;
		}
		return 1;
	}
	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		err = ss_printf(s, "-ERR #missing auth type\r\n");
		if(err < 0) {
			return -1;
		}
		return 1;
	}

	if(strcasecmp(ptr, "plaintext") == 0) {
		auth_type = auth_plaintext;
	} else if(strcasecmp(ptr, "md5") == 0) {
		auth_type = auth_md5;
	} else {
		/* unrecognized authentication method */
		err = ss_printf(s, "-ERR #unknown authentication method\r\n");
		if(err) {
			return -1;
		}
		return 1;
	}

	/* return appropriate response for auth method requested */
	switch(auth_type) {
	case auth_plaintext:
		err = ss_printf(s, "+OK #send password\r\n");
		if(err < 0) {
			return -1;
		}
		break;
	case auth_md5:
		err = ss_printf(s, "+OK CHALLENGE ");
		if(err < 0) {
			return -1;
		}
		/* Generate and return a pseudo-random 128-bit challenge.
		 * This doesn't have to be truely random, it only needs
		 * to be random enough so subsequent client connections 
		 * don't get the same challenge.
		 */
		srand(time(0));
		for(i=0; i<16; i++) {
			challenge[i] = rand();
			err = ss_printf(s, "%02x", challenge[i]);
			if(err < 0) {
				return -1;
			}
		}
		err = ss_printf(s, "\r\n");
		if(err < 0) {
			return -1;
		}
		break;
	default:
		abort();
		break;
	}

	err = ss_flush(s);
	if(err < 0) {
		return -1;
	}

	ptr = ss_gets(buff, BUFSIZ, s);
	if(!ptr) {
		return -1;
	}
	ptr = strchr(buff, '#');
	if(ptr) {
		*ptr = '\0';
	}
	ptr = strtok(buff, " \t\r\n");
	if(!ptr) {
		err = ss_printf(s, "-ERR #need response\r\n");
		if(err < 0) {
			return -1;
		}
		return 1;
	}

	switch(auth_type) {
	case auth_plaintext:
		if(!pass) {
			break;
		}
		if(strncasecmp(pass, "md5=", 4)) {
			/* if received plaintext password, and we have 
			 * plaintext password, just compare passwords. 
			 */
			if(strcmp(ptr, pass)) {
				break;
			}
			err = ss_printf(s, "+OK\r\n");
			if(err < 0) {
				return -1;
			}
			return AUTH_MAGIC;
		} else {
			/* if received plaintext password, but we have 
			 * md5 hash of password, compute md5 hash of 
			 * received password and compare hashes.
			 */
			if(MD5StrToDigest(pass+4, password)) {
				fprintf(stderr, "Invalid md5 password %s\n",
				    pass+4);
				break;
			}
			MD5Init(&context);
			MD5Update(&context, (unsigned char *)ptr, strlen(ptr));
			MD5Final(challenge, &context);
			if(memcmp((void *)challenge, (void *)password, 16)) {
				break;
			}
			err = ss_printf(s, "+OK\r\n");
			if(err < 0) {
				return -1;
			}
			return AUTH_MAGIC;
		}
		break;
	case auth_md5:
		if(!pass) {
			break;
		}

		if(strncasecmp(pass, "md5=", 4)) {
			/* if received md5 hash, but have plaintext password,
			 * compute md5 hash of password so we can hash it
			 * again later with the challenge.
			 */
			MD5Init(&context);
			MD5Update(&context, (unsigned char *)pass, 
			    strcspn(pass, ","));
			MD5Final(password, &context);
		} else {
			/* if we have md5 hash of password, just read
			 * it in so we can hash it again with the challenge.
			 */
			if(MD5StrToDigest(pass+4, password)) {
				break;
			}
		}

		/* compute expected md5 hash of challenge concatinated 
		 * with md5 hash of password
		 */	
		MD5Init(&context);
		MD5Update(&context, challenge, 16);
		MD5Update(&context, password, 16);
		MD5Final(password, &context);

		/* read received md5 hash of challege concatinated
		 * with md5 hash of password 
		 */
		if(MD5StrToDigest(ptr, challenge)) {
			break;
		}

		/* check if md5 hashes match */
		if(memcmp(challenge, password, 16)) {
			break;
		}
		err = ss_printf(s, "+OK\r\n");
		if(err < 0) {
			return -1;
		}
		return AUTH_MAGIC;
	default:
		break;
	}

	err = ss_printf(s, "-ERR #authentication failed\r\n");
	if(err < 0) {
		return -1;
	}

	return 0;	
}

/**************************************************************
 * This function handles an ocd link layer reset request 
 * from the client.
 * 
 * This is called when a RESET request is received.
 *
 * This function return 0 upon success, -1 if an error occurred
 * while reading/writing the socket.
 *
 * NOTE: this function assumes the calling routine will flush
 * the output buffer before reading the next request.
 */

static int client_reset(SOCK *s, ocd *dbg)
{
	int err;
	char *msg;

	msg = NULL;

	try {
		dbg->reset();
	} catch(char *txt) {
		msg = txt;
	}

	if(!msg) {
		err = ss_printf(s, "+OK\r\n");
	} else {
		err = ss_printf(s, "-ERR #link reset failed\r\n");
	}

	if(err < 0) {
		return -1;
	}

	return 0;
}

/**************************************************************
 * This function handles a status request from a client.
 *
 * This is called with a STAT request is received from
 * an authenticated client. 
 *
 * If there is a pending link-layer error, the server will
 * respond with
 *	+OK DOWN
 * If the link is ready for communication, the server will
 * respond with
 * 	+OK UP
 *
 * This function return 0 upon success, -1 if an error occurred
 * while reading/writing the socket.
 *
 * NOTE: this function assumes the calling routine will flush
 * the output buffer before reading the next request.
 */

static int client_status(SOCK *s, ocd *dbg)
{
	int err;

	if(!dbg->link_up()) {
		/* link is down, needs reset before read/write 
		 * commands can be issued
		 */
		err = ss_printf(s, "+OK DOWN\r\n");
	} else {
		/* link is up */
		err = ss_printf(s, "+OK UP\r\n");
	}

	if(err < 0) {
		return -1;
	}

	return 0;
}

/**************************************************************
 * This function handles a read request for an authenticated
 * client.
 *
 * This function is called when the READ request is received.
 * The client should follow the READ request with the number
 * of bytes to read.
 *
 * If the read from the ocd link layer was successful, the
 * server will respond with +OK, followed by the data requested.
 * The data returned is ascii encoded and delimited by the
 * space ' ' character and/or carriage return '\r' and 
 * newline '\n' characters. Each byte is formatted so
 * it can be parsed with the standard C strtol() function.
 *
 * This specific implementation formats each data byte
 * as two hex characters prefixed with '0x'. This implementation
 * will insert CRLF sequences "\r\n" every eight bytes to
 * make the raw output more readable on a 80 character terminal.
 *
 * This function return 0 upon sucess, 1 if a protocol error 
 * occurred, or -1 if an error occurred while reading/writing 
 * the socket.
 *
 * NOTE: this function assumes the calling routine will flush
 * the output buffer before reading the next request.
 */

static int client_read(SOCK *s, ocd *dbg)
{
	int err;
	int i;
	char *ptr, *tail;

	/* get read size */
	ptr = strtok(NULL, " \t\r\n");
	if(!ptr) {
		err = ss_printf(s, "-ERR #size needed\r\n");
		if(err < 0) {
			return -1;
		}
		return 1;
	}
	data_size = strtoul(ptr, &tail, 0);
	if(!tail || *tail || tail == ptr) {
		err = ss_printf(s, "-ERR #invalid number \'%s\'\r\n", ptr);
		if(err < 0) {
			return -1;
		}
		return 1;
	}

	/* if link down, we cannot read yet, link needs reset */
	if(!dbg->link_up()) {
		err = ss_printf(s, "-ERR #link down\r\n");
		if(err < 0) {
			return -1;
		}
		return 1;
	}

	/* read data from ocd link layer */
	try {
		dbg->read(data, data_size);
	} catch(char *msg) {
		err = ss_printf(s, "-ERR #read failed\r\n");
		if(err < 0) {
			return -1;
		}
		return 0;
	}

	/* return data to client */
	err = ss_printf(s, "+OK ");
	if(err < 0) {
		return -1;
	}
	for(i=0; i<data_size; i++) {
		if(i%8 == 0) {
			err = ss_printf(s, "\r\n");
			if(err < 0) {
				return -1;
			}
		}
		err = ss_printf(s, "0x%02x ", data[i]);
		if(err < 0) {
			return -1;
		}
	}
	err = ss_printf(s, "\r\n\r\n");
	if(err < 0) {
		return -1;
	}

	return 0;
}

/**************************************************************
 * This function handles a write request for an authenticated
 * client.
 * 
 * This function is called when a WRITE request is received.
 * The client should follow the WRITE with the data to 
 * transmit on the ocd link layer. The client should terminate
 * the write request with a blank line (CRLFCRLF).
 *
 * The data to transmit should be ascii encoded with each
 * byte delimited by spaces ' ', tabs '\t', or carriage
 * return '\r' newline '\n' characters.
 *
 * This function returns 0 upon success, 1 if a protocol
 * error was detected, or -1 if an error occurred while 
 * reading/writing the socket.
 *
 * NOTE: this function assumes the calling routine will flush
 * the output buffer before reading the next request.
 */

static int client_write(SOCK *s, ocd *dbg)
{
	int err;
	char *ptr, *tail;

	data_size = 0;
	err = 0;

	do {
		/* get next byte */
		ptr = strtok(NULL, " \t\r\n");

		if(!ptr) {
			ptr = ss_gets(buff, BUFSIZ, s);
			if(!ptr) {
				return -1;
			}
			ptr = strtok(buff, " \t\r\n");
			if(!ptr) {
				/* if blank line, we have all data.
				 * write the data to the link layer.
				 */
				break;
			}
		}

		/* convert data and place in data buff to write
		 * later once we have all data 
		 */
		if(data_size < 0x10000) {
			data[data_size] = strtol(ptr, &tail, 0);
			data_size++;
			if(!tail || *tail || tail == ptr) {
				err = client_flush(s);
				if(err < 0) {
					return -1;
				}
				ss_printf(s, "-ERR #invalid data\r\n");
				return 1;
			}
		} else {
			err = client_flush(s);
			if(err < 0) {
				return -1;
			}
			err = ss_printf(s, "-ERR size out-of-range\r\n");
			if(err < 0) {
				return -1;
			}
			return 1;
		}

	} while(!err && ptr);

	if(!dbg->link_up()) {
		err = ss_printf(s, "-ERR #link down\r\n");
		if(err) {
			return -1;
		}
		return 1;
	}
	try {
		dbg->write(data, data_size);
	} catch(char *msg) {
		err = ss_printf(s, "-ERR\r\n");
		if(err < 0) {
			return -1;
		}
	}

	err = ss_printf(s, "+OK\r\n");
	if(err < 0) {
		return -1;
	}

	return 0;
}

/**************************************************************
 * This is the main service routine for client connections.
 */

static int service_client(ocd *dbg, int client, char *userpasswd)
{
	int err;
	char *ptr, *user;
	SOCK sock;
	int auth;

	err = ss_open(client, &sock);
	if(err) {
		close(client);
		return -1;
	}

	user = NULL;

	/* If no userpasswd requested, assume client authenticated */
	if(!userpasswd || *userpasswd == '\0') {
		auth = 1;
	} else {
		auth = 0;
	}

	err = ss_printf(&sock, "+OK Z8ENCOREOCD %d.%02d #build %s %s\r\n",
	    VERSION_MAJOR, VERSION_MINOR, __DATE__, __TIME__);

	while(err >= 0) {

		/* flush any output/response */
		err = ss_flush(&sock);
		if(err < 0) {
			break;
		}

		/* get request */
		ptr = ss_gets(buff, BUFSIZ, &sock);
		if(!ptr) {
			err = -1;
			break;
		}

		/* filter comments */
		ptr = strchr(buff, '#');
		if(ptr) {
			*ptr = '\0';
		}
		ptr = strtok(buff, " \t\r\n");
		if(!ptr) {	
			/* skip blank lines */
			continue;
		}

		/* determine request */
		if(strcasecmp(ptr, "user") == 0) {
			err = client_auth(&sock, userpasswd);
			if(err == AUTH_MAGIC) {
				auth = 1;
			}
			if(err < 0) {
				break;
			}
		} else if(strcasecmp(ptr, "status") == 0) {
			if(!auth) {
				err = ss_printf(&sock, "+OK AUTH\r\n");
				if(err < 0) {
					break;
				}
				continue;
			}
			err = client_status(&sock, dbg);
			if(err < 0) {
				break;
			} 
		} else if((strcasecmp(ptr, "close") == 0) ||
		          (strcasecmp(ptr, "exit") == 0) ||
		          (strcasecmp(ptr, "quit") == 0)) {
			err = ss_printf(&sock, "+OK #exiting\r\n");
			break;
		} else if(strcasecmp(ptr, "reset") == 0) {
			if(!auth) {
				err = ss_printf(&sock, "-ERR "
				    "#auth required\r\n");
				if(err < 0) {
					break;
				}
				continue;
			} 
			err = client_reset(&sock, dbg);
			if(err < 0) {
				break;
			}
		} else if(strcasecmp(ptr, "read") == 0) {
			if(!auth) {
				ss_printf(&sock, "-ERR #auth required\r\n");
				continue;
			} 
			err = client_read(&sock, dbg);
			if(err < 0) {
				break;
			}
		} else if(strcasecmp(ptr, "write") == 0) {
			if(!auth) {
				err = client_flush(&sock);
				if(err < 0) {
					break;
				}
				err = ss_printf(&sock, "-ERR "
				    "#auth required\r\n");
				if(err < 0) {
					break;
				}
				continue;
			}
			err = client_write(&sock, dbg);
			if(err < 0) {
				break;
			}
		} else {
			err = ss_printf(&sock, "-ERR #invalid command\r\n");
		}
	} 

	if(err >= 0) {
		ss_flush(&sock);
	}

	err = ss_close(&sock);
	if(err) {
		perror("ss_close");
	}

	return 0;
}

/**************************************************************
 * This will fire up and start the ocd server. 
 * 
 * *dbg should already be setup and connected to.
 *
 * *connection should be in the form
 *	[user:pass[,user:pass...]@][host][:port]
 * host should only be specified if you want to bind to a 
 *     specific interface. 
 * If :port is not specified, DEFAULT_PORT is used.
 * If user:pass@ is not specified, authentication will not
 *     be required.
 */

int run_server(ocd *dbg, char *connection)
{
	int fd, client;
	char *host, *userpasswd;
#ifdef	_WIN32
	int err;
	WSADATA wsa;

	err = WSAStartup(MAKEWORD(2,1), &wsa);
	if(err) {
		fprintf(stderr, "WSAStartup failed\n");
		return -1;
	}
#endif	/* _WIN32 */

	if(!data) {
		data = (uint8_t *)xmalloc(0x10000);
	}
	if(!buff) {
		buff = (char *)xmalloc(BUFSIZ+1);
		buff[BUFSIZ] = '\0';
	}

	userpasswd = NULL;
	host = NULL;

	if(connection) {
		host = strchr(connection, '@');
		if(host) {
			*host = '\0';
			host++;
			userpasswd = connection;
		} else {
			host = connection;
		}
	}

	fd = bind_server(host);
	if(fd < 0) {
		return -1;
	}

	while((client = get_connection(fd)) >= 0) {
		service_client(dbg, client, userpasswd);
	}

	close(fd);	

#ifdef	_WIN32
	err = WSACleanup();
	if(err) {
		return -1;
	}
#endif
	free(data);
	data = NULL;
	free(buff);
	buff = NULL;

	return 0;
}

/**************************************************************/


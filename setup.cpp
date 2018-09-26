/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: setup.cpp,v 1.6 2005/10/20 18:39:37 jnekl Exp $
 *
 * This file initializes the debugger enviroment by reading
 * settings from a config file and by parsing command-line
 * options.
 */

#include	<string.h>
#include	<stdio.h>
#include	<assert.h>
#include	<sys/stat.h>
#include	<readline/readline.h>
#include	<readline/history.h>
#include	"xmalloc.h"

#include	"ez8dbg.h"
#include	"ocd_serial.h"
#include	"ocd_parport.h"
#include	"ocd_tcpip.h"
#include	"server.h"
#include	"cfg.h"
#include	"md5.h"

/**************************************************************/

#ifndef	CFGFILE
#define	CFGFILE	"ez8mon.cfg"
#endif

#ifndef	_WIN32
#define	DIRSEP	'/'
#else
#define	DIRSEP	'\\'
#endif

/**************************************************************/

char *serialports[] = 
#if (!defined _WIN32) && (defined __sun__)
/* solaris */
{ "/dev/ttya", "/dev/ttyb", NULL };
#elif (!defined _WIN32)
/* linux */
{ "/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3", NULL };
#else
/* windows */
{ "com1", "com2", "com3", "com4", NULL };
#endif

const char *progname;
int verbose;

/**************************************************************/

#define	str(x) _str(x)
#define _str(x) #x


#ifndef	DEFAULT_CONNECTION
#define	DEFAULT_CONNECTION	serial
#endif

#ifndef	DEFAULT_DEVICE
#define	DEFAULT_DEVICE		auto
#endif

#ifndef	DEFAULT_BAUDRATE
#define	DEFAULT_BAUDRATE	57600
#endif

#ifndef	ALT_BAUDRATE
#define ALT_BAUDRATE		4800
#endif

#ifndef	DEFAULT_MTU
#define	DEFAULT_MTU		4096
#endif

#ifndef	DEFAULT_SYSCLK
#define	DEFAULT_SYSCLK		20MHz
#endif

/**************************************************************
 * Our debugger object.
 */

static ez8dbg _ez8;
ez8dbg *ez8 = &_ez8;

/**************************************************************/

rl_command_func_t *tab_function = rl_insert;

static char *connection = NULL;
static char *device = NULL;
static char *baudrate = NULL;
static char *sysclk = NULL;
static char *mtu = NULL;
static char *server = NULL;
static FILE *log_proto = NULL;

static int invoke_server = 0;
static int disable_cache = 0;

int repeat = 0x40;
int show_times = 0;
int esc_key = 0;
int testmenu = 0;

char *tcl_script = NULL;

/* option values to load upon reset */
struct option_t {
	uint8_t addr;
	uint8_t value;
	struct option_t *next;
} *options = NULL;

/**************************************************************
 * The ESC key is bound to this function.
 *
 * This function will terminate any readline call. It is
 * used to abort a command.
 */

int esc_function(int count, int key)
{
	esc_key = 1;
	rl_done = 1;

	return 0;
}

/**************************************************************
 * This initializes the readline library.
 */

int readline_init(void)
{
	int err;

	err = rl_bind_key(ESC, esc_function);
	if(err) {
		return -1;
	}

	err = rl_bind_key('\t', *tab_function);
	if(err) {
		return -1;
	}

	return 0;
}

/**************************************************************
 * This will search for a config file.
 *
 * It will first look in the current directory. If it doesn't
 * find one there, it will then look in the directory pointed
 * to by the HOME environment variable.
 */

cfgfile *find_cfgfile(void)
{
	char *filename, *home, *ptr;
	struct stat mystat;
	cfgfile *cfg;

	filename = CFGFILE;

	if(stat(filename, &mystat) == 0) {
		cfg = new cfgfile();
		cfg->open(filename);
		return cfg;
	}

	home = getenv("HOME");
	if(!home) {
		return NULL;
	}

	filename = (char *)xmalloc(strlen(home) + 1 + sizeof(CFGFILE));

	strcpy(filename, home);
	ptr = strchr(filename, '\0');
	*ptr++ = DIRSEP;
	*ptr = '\0';
	strcat(filename, CFGFILE);

	if(stat(filename, &mystat) == 0) {
		cfg = new cfgfile();
		cfg->open(filename);
		free(filename);
		return cfg;
	}

	free(filename);

	return NULL;
}

/**************************************************************
 * This function will get parameters from the config file.
 */

int load_config(void)
{
	char *ptr;
	cfgfile *cfg;

	cfg = find_cfgfile();
	if(!cfg) {
		return 1;
	}

	ptr = cfg->get("connection");
	if(ptr) {
		connection = xstrdup(ptr);
	}

	ptr = cfg->get("device");
	if(ptr) {
		device = xstrdup(ptr);
	}

	ptr = cfg->get("baudrate");
	if(ptr) {
		baudrate = xstrdup(ptr);
	}
			
	ptr = cfg->get("mtu");
	if(ptr) {
		mtu = xstrdup(ptr);
	}

	ptr = cfg->get("clock");
	if(ptr) {
		sysclk = xstrdup(ptr);
	}
			
	ptr = cfg->get("server");
	if(ptr) {
		server = xstrdup(ptr);
		invoke_server = 1;
	}

	ptr = cfg->get("cache");
	if(ptr) {
		if(!strcasecmp(ptr, "disabled")) {
			disable_cache = 1;
		}
	}

	ptr = cfg->get("testmenu");
	if(ptr) {
		if(!strcasecmp(ptr, "enabled")) {
			testmenu = 1;
		}
	}

	ptr = cfg->get("repeat");
	if(ptr) {
		char *tail;

		repeat = strtol(ptr, &tail, 0);
		if(!tail || *tail || tail == ptr) {
			fprintf(stderr, "Invalid repeat = %s\n", ptr);
			return -1;
		}
	}
	
	delete cfg;

	return 0;
}

/**************************************************************
 * display_md5hash()
 *
 * This routine computes and displays the md5 hash of a text
 * string. This is useful for generating password hashes.
 */

void display_md5hash(char *s)
{
	int i;
	uint8_t hash[16];
	MD5_CTX context;

	assert(s != NULL);
	
	MD5Init(&context);
	MD5Update(&context, (unsigned char *)s, strlen(s));
	MD5Final(hash, &context);

	printf("text = %s\n", s);
	printf("md5hash = ");

	for(i=0; i<16; i++) {
		printf("%02x", hash[i]);
	}

	printf("\n");

	return;
}

/**************************************************************
 * usage()
 *
 * This function displays command-line usage info.
 */

void usage(void)
{
extern char *build;
printf("%s - build %s\n", progname, build);
printf("Usage: %s [OPTIONS]\n", progname);
printf("Z8 Encore! command line debugger.\n\n");
printf("  -h                         show this help\n");
printf("  -p DEVICE                  connect to specified serial port device\n");
printf("  -b BAUDRATE                connect using specified baudrate\n");
printf("  -l                         list valid baudrates\n");
printf("  -t MTU                     set maximum packet size\n");
printf("                               (used to prevent receive overrun errors)\n");
printf("  -c FREQUENCY               use specified clock frequency for flash\n");
printf("                               program/erase oprations\n");
printf("  -s [:PORT]                 run as tcp/ip server\n");
printf("  -n [SERVER][:PORT]         connect to tcp/ip server\n");
printf("  -m TEXT                    calculate and display md5hash of text\n");
printf("  -d                         dump raw ocd communication\n");
printf("  -D                         disable memory cache\n");
printf("  -T                         display diagnostic run times\n");
printf("  -S SCRIPT                  run tcl script\n");
printf("\n");
	
return;
}

/**************************************************************
 * parse_options()
 *
 * This routine handles command line options.
 */

void parse_options(int argc, char **argv)
{
	int c;
	const struct baudvalue *b;
	char *s;

	progname = *argv;
	s = strrchr(progname, '/');
	if(s) {
		progname = s+1;
	}
	s = strrchr(progname, '\\');
	if(s) {
		progname = s+1;
	}

	while((c = getopt(argc, argv, "hldDTp:b:t:c:snm:vS:")) != EOF) {
		switch(c) {
		case '?':
			printf("Try '%s -h' for more information.\n", 
			    progname);
			exit(EXIT_FAILURE);
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		case 'l':
			b = baudrates;
			while(b->value) {
				printf("%d\n", b->value);
				b++;
			}
			exit(EXIT_SUCCESS);
			break;
		case 'p':
			connection = "serial";
			if(device) {
				free(device);
			}
			device = xstrdup(optarg);
			break;
		case 'b':
			if(baudrate) {
				free(baudrate);
			}
			baudrate = xstrdup(optarg);
			break;
		case 't':
			if(mtu) {
				free(mtu);
			}
			mtu = xstrdup(optarg);
			break;
		case 'c':
			if(sysclk) {
				free(sysclk);
			}
			sysclk = xstrdup(optarg);
			break;
		case 's':
			invoke_server = 1;
			break;
		case 'n':
			connection = "tcpip";
			if(device) {
				free(device);
				device = NULL;
			}
			break;
		case 'd':
			log_proto = stdout;
			break;
		case 'D':
			disable_cache = 1;
			break;
		case 'T':
			show_times = 1;
			break;
		case 'm':
			display_md5hash(optarg);
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			verbose++;
			break;
		case 'S':
			tcl_script = optarg;
			break;
		default:
			abort();
		}
		
	}

	if(invoke_server) {
		if(optind + 1 == argc) {
			if(server) {
				free(server);
			}
			server = xstrdup(argv[optind]);
		}
		optind++;
	} 

	if(connection && !strcasecmp(connection, "tcpip")) {
		if(optind + 1 == argc) {
			if(device) {
				free(device);
			}
			device = xstrdup(argv[optind]);
		}
		optind++;
	}

	if(!tcl_script && optind < argc) {
		printf("%s: too many arguments.\n", progname);
		printf("Try '%s -h' for more information.\n", progname);
		exit(EXIT_FAILURE);
	}
		
	return;
}

/**************************************************************
 * connect()
 *
 * This routine will connect to the on-chip debugger using
 * the configured interface.
 */

int connect(void)
{
	int i, value, clk, baud;
	char *tail;
	double clock;

	if(log_proto) {
		ez8->log_proto = log_proto;
	}

	if(mtu) {
		value = strtol(mtu, &tail, 0);
		if(!tail || *tail || tail == mtu) {
			fprintf(stderr, "Invalid mtu \"%s\"\n", mtu);
			return -1;
		}
		ez8->mtu = value;
	}

	if(sysclk) {
		clock = strtod(sysclk, &tail);
		if(tail == NULL || tail == sysclk) {
			fprintf(stderr, "Invalid clock \"%s\"\n", sysclk);
			return -1;
		}
		while(*tail && strchr(" \t", *tail)) {
			tail++;
		}
		if(*tail == 'k' || *tail == 'K') {
			clock *= 1000;
			tail++;
		} else if(*tail == 'M') {
			clock *= 1000000;
			tail++;
		}
		if(*tail != '\0' && strcasecmp(tail, "Hz")) {
			fprintf(stderr, "Invalid clock suffix \"%s\"\n", 
			    tail);
			return -1;
		}
	} else {
		fprintf(stderr, "Unknown system clock.\n");
		return -1;
	}

	clk = (int)clock;
	if(clk < 32000 || clk > 20000000) {
		fprintf(stderr, "Clock %d out of range\n", clk);
		return -1;
	}
	ez8->set_sysclk(clk);
	if(disable_cache) {
		ez8->memcache_enabled = 0;
	}

	if(connection == NULL) {
		fprintf(stderr, "Unknown connection type.\n");
		return -1;

	} else if(!strcasecmp(connection, "serial")) {
		if(!device) {
			fprintf(stderr, "Unknown communication device.\n");
			return -1;
		}
		if(!baudrate) {
			baud = DEFAULT_BAUDRATE;
		} else {
			baud = strtol(baudrate, &tail, 0);
			if(!tail || *tail || tail == baudrate) {
				fprintf(stderr, "Invalid baudrate \"%s\"\n", 
				    baudrate);
				return -1;
			}
		}

		if(!strcasecmp(device, "auto")) {
			uint16_t revid;
			bool found = 0;

			printf("Auto-searching for device ...\n");
			for(i=0; serialports[i]; i++) {
				device = serialports[i];
				printf("Trying %s ... ", device);
				fflush(stdout);
				try {
					ez8->connect_serial(device, baud);
				} catch(char *err) {
					printf("fail\n");
					continue;
				}

				try {
					ez8->reset_link();
				} catch(char *err) {
					printf("timeout\n");
					ez8->disconnect();
					continue;
				} 
				printf("ok\n");
				found = 1;
				break;
			}

			if(!found) {
				printf(
"Could not find dongle during autosearch.\n");
				return -1;
			}

			try {
				revid = ez8->rd_revid();
			} catch(char *err) {
				baud = ALT_BAUDRATE;
				ez8->set_baudrate(baud);
				ez8->reset_link();
				try {
					revid = ez8->rd_revid();
				} catch(char *err) {
					printf(
"Found dongle, but did not receive response from device.\n");
					return -1;
				}
			}
			if(ez8->cached_reload() && 
			   ez8->cached_sysclk() > 115200*8) {
				baud = 115200;
				ez8->set_baudrate(baud);
				ez8->reset_link();
			}
		} else {
			try {
				ez8->connect_serial(device, baud);
			} catch(char *err) {
				fprintf(stderr, "%s", err);
				return -1;
			}

			try {
				ez8->reset_link();
			} catch(char *err) {
				printf("Failed connecting to %s\n", device);
				fprintf(stderr, "%s", err);
				ez8->disconnect();
				return -1;
			}
		}

		printf("Connected to %s @ %d\n", device, baud);

	} else if(!strcasecmp(connection, "parport")) {
		if(!device) {
			fprintf(stderr, "Unknown device");
			return -1;
		}
		try {
			ez8->connect_parport(device);
		} catch(char *err) {
			printf("Parallel port connection failed\n");
			fprintf(stderr, "%s", err);
			return -1;
		}
		try {
			ez8->reset_link();
		} catch(char *err) {
			printf("IEEE1284 negotiation failed\n");
			fprintf(stderr, "%s", err);
			ez8->disconnect();
			return -1;
		}
	} else if(!strcasecmp(connection, "tcpip")) {
		try {
			ez8->connect_tcpip(device);
		} catch(char *err) {
			printf("Connection failed\n");
			fprintf(stderr, "%s", err);
			return -1;
		}

		try {
			ez8->reset_link();
		} catch(char *err) {
			printf("Remote reset failed\n");
			fprintf(stderr, "%s", err);
			ez8->disconnect();
			return -1;
		}

	} else {
		printf("Invalid connection type \"%s\"\n", 
		    connection);
		return -1;
	}


	return 0;
}

/**************************************************************
 * init()
 *
 * This routine sets up and connects to the on-chip debugger.
 */

int init(int argc, char **argv)
{
	int err;

	tab_function = rl_insert;
	rl_startup_hook = readline_init;

	connection = xstrdup(str(DEFAULT_CONNECTION));
	device = xstrdup(str(DEFAULT_DEVICE));
	baudrate = xstrdup(str(DEFAULT_BAUDRATE));
	sysclk = xstrdup(str(DEFAULT_SYSCLK));
	mtu = xstrdup(str(DEFAULT_MTU));

	err = load_config();
	if(err < 0) {
		return -1;
	}

	parse_options(argc, argv);

	err = connect();
	if(err) {
		return -1;
	}

	if(invoke_server) {
		err = run_server(ez8->iflink(), server);
		ez8->disconnect();
		if(err) {
			exit(EXIT_FAILURE);
		} else {
			exit(EXIT_SUCCESS);
		}
	} 

	#ifdef	TEST
	try {
		extern void check_config(void);
		check_config();
	} catch(char *err) {
		fprintf(stderr, "%s", err);
		return -1;
	}
	#endif

	try {
		ez8->stop();
	} catch(char *err) {
		fprintf(stderr, "%s", err);
		return -1;
	}

	return 0;
}

/**************************************************************
 * finish()
 *
 * This does the opposite of init(), it terminates a 
 * connection. It will remove any breakpoints that are
 * set and put the part into "run" mode when finished.
 */

int finish(void)
{
	try {
		int addr;

		while(ez8->get_num_breakpoints() > 0) {
			addr = ez8->get_breakpoint(0);
			ez8->remove_breakpoint(addr);
		}
		ez8->run();
		ez8->disconnect();

	} catch(char *err) {
		fprintf(stderr, "%s", err);
		return -1;
	}

	return 0;
}

/**************************************************************/



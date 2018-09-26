/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: flashutil.cpp,v 1.4 2008/10/02 18:49:07 jnekl Exp $
 *
 * Z8 Encore Flash programming utility.
 */

#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<string.h>
#include	<assert.h>
#include	<readline/readline.h>
#include	"xmalloc.h"

#include	"ez8dbg.h"
#include	"crc.h"
#include	"hexfile.h"
#include	"version.h"

/**************************************************************/

#ifndef	DEFAULT_SERIALPORT
#define	DEFAULT_SERIALPORT "auto"
#endif

#ifndef	DEFAULT_BAUDRATE
#ifndef	_WIN32
#define	DEFAULT_BAUDRATE 115200
#else
#define	DEFAULT_BAUDRATE 57600
#endif
#endif

#ifndef	DEFAULT_XTAL
#define	DEFAULT_XTAL 18432000
#endif

#ifndef	DEFAULT_MTU
#define	DEFAULT_MTU 4096
#endif

#define	MEMSIZE	0x10000

/**************************************************************/

static const char *banner = "Z8 Encore! Flash Utility";
static const char *progname;

static char *serialport = DEFAULT_SERIALPORT;
static int baudrate = DEFAULT_BAUDRATE;
static int mtu = DEFAULT_MTU;
static int xtal = DEFAULT_XTAL;
static int multipass = 0;
static int info = 0;
static int erase = 0;
static int zero_fill = 0;
static int crc_size = -1;
static int verbose = 0;
static char *savefilename = NULL;
static char *programfilename = NULL;

static uint8_t *buff, *blank;
static uint16_t buff_crc, blank_crc;
static int mem_size = 0;
static int max_mem = 0;

static uint16_t serial_address;
static uint32_t serial_number;
static int serial_size;

/**************************************************************/

char *serialport_selection[] = 
#if (!defined _WIN32) && (defined __sun__)
{ "/dev/ttya", "/dev/ttyb", NULL };
#elif (!defined _WIN32)
{ "/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3", NULL };
#else
{ "com1", "com2", "com3", "com4", NULL };
#endif

int baudrate_selection[] = { 115200, 57600, 38400, 0 };
int xtal_selection[] = { 20000000, 18432000, 1000000, 0 };

ez8dbg _dbg;
ez8dbg *dbg = &_dbg;

/**************************************************************/

void help(void)
{
printf("%s - build %s\n", progname, build);
printf("Usage: %s [OPTION]... [FILE]\n", progname);
printf( "Utility to program Z8 Encore! flash devices.\n\n");
printf("  -h               show this help\n");
printf("  -i               display information about device\n");
printf("    -r SIZE        calculate CRC on SIZE bytes of memory\n");
printf("  -m               multipass mode\n");
printf("  -n ADDR=NUMBER   serialize part at ADDR, starting with NUMBER\n");
printf("  -e               erase device\n");
printf("  -p SERIALPORT    specify serialport to use (default: %s)\n",
    DEFAULT_SERIALPORT);
printf("  -b BAUDRATE      use baudrate (default: %d)\n", 
    DEFAULT_BAUDRATE);
printf("  -t MTU           maximum transmission unit (default %d)\n", 
    DEFAULT_MTU);
printf("  -c FREQUENCY     clock frequency in hertz (default: %d)\n", 
    DEFAULT_XTAL);
printf("  -s FILENAME      save memory to file\n");
printf("  -z               fill memory with 00 instead of FF\n");
printf("\n");

return;
}

/**************************************************************/

int setup(int argc, char **argv)
{
	int c;
	char *last, *ptr, *s;
	double clock;

	progname = *argv;
	s = strrchr(progname, '/');
	if(s) {
		progname = s+1;
	}
	s = strrchr(progname, '\\');
	if(s) {
		progname = s+1;
	}
	
	while((c = getopt(argc, argv, "hiemn:p:b:c:s:t:zr:v")) != EOF) {
		switch(c) {
		case '?':
			printf("Try '%s -h' for more information.\n", argv[0]);
			exit(EXIT_FAILURE);
			break;
		case 'h':
			help();
			exit(EXIT_SUCCESS);
			break;
		case 'i':
			info = 1;
			break;
		case 'e':
			erase = 1;
			break;
		case 'm':
			multipass = 1;
			break;
		case 'n':
			serial_address = strtol(optarg, &last, 16);
			if(!last || last == optarg || *last != '=') {
				fprintf(stderr, "Invalid serial string '%s'\n",
				    optarg);
				exit(EXIT_FAILURE);
			}
			ptr = last+1;
			serial_number = strtol(ptr, &last, 16);
			if(!last || last == ptr || *last != '\0') {
				fprintf(stderr, "Invalid serial string '%s'\n",
				    optarg);
				exit(EXIT_FAILURE);
			}
			serial_size = (strlen(ptr) + 1) / 2;
			if(serial_size < 1 || serial_size > 4) {
				fprintf(stderr, 
				    "Serial number size out-of-range\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 's':
			savefilename = optarg;
			break;
		case 'p':
			serialport = optarg;
			break;
		case 'b':
			baudrate = strtol(optarg, &last, 0);
			if(!last || *last || last == optarg) {
				fprintf(stderr, 
				    "Invalid baudrate \'%s\'\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'c':
			clock = strtod(optarg, &last);
			if(!last || last == optarg) {
				fprintf(stderr, 
				    "Invalid clock frequency \'%s\'\n", 
				    optarg);
				exit(EXIT_FAILURE);
			}
			if(*last == 'k' || *last == 'K') {
				clock *= 1000;
				last++;
			} else if(*last == 'M') {
				clock *= 1000000;
				last++;
			}

			if(*last && strcasecmp(last, "Hz")) {
				fprintf(stderr, "Invalid clock suffix '%s'\n",
				    last);
				exit(EXIT_FAILURE);
			}
			if(clock < 20000 || clock > 65000000) {
				fprintf(stderr, 
				    "Clock frequency out of range\n");
				exit(EXIT_FAILURE);
			}
			xtal = (int)clock;
			break;
		case 't':
			mtu = strtol(optarg, &last, 0);
			if(last == NULL || last == optarg || *last != '\0') {
				fprintf(stderr, 
				    "Invalid MTU \'%s\'\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'z':
			zero_fill = 1;
			break;
		case 'r':
			crc_size = strtol(optarg, &last, 0);
			if(!last || last == optarg) {
				fprintf(stderr, 
				    "Invalid size \'%s\'\n", optarg);
				exit(EXIT_FAILURE);
			}
			if(*last == 'k' || *last == 'K') {
				crc_size *= 1024;
			} else if(*last) {
				fprintf(stderr, 
				    "Invalid size \'%s\'\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			verbose++;
			break;
		default:
			abort();
		}
	}

	if(optind == argc && !info && !savefilename && !erase) {
		fprintf(stderr, "%s: too few arguments\n", argv[0]);
		fprintf(stderr, "Try '%s -h' for more information.\n", 
		    argv[0]);
		exit(EXIT_FAILURE);
	} else if(optind + 1 < argc) {
		fprintf(stderr, "%s: too many arguments.\n", argv[0]);
		fprintf(stderr, "Try '%s -h' for more information.\n", 
		    argv[0]);
		exit(EXIT_FAILURE);
	} else {
		programfilename = argv[optind];
	}

	if(multipass && savefilename) {
		printf("Cannot save file(s) in multipass mode\n");
		return -1;
	}

	if(multipass && !programfilename) {
		printf("Need input file for multipass mode\n");
		return -1;
	}

	dbg->mtu = mtu;

	dbg->set_sysclk(xtal);

	buff = (uint8_t *)xmalloc(MEMSIZE);
	blank = (uint8_t *)xmalloc(MEMSIZE);

	memset(blank, 0xff, MEMSIZE);

	return 0;
}

/**************************************************************/

int connect(void)
{
	int i;
	char *port;

	if(strcasecmp(serialport, "auto") == 0) {

		printf("Autoconnecting to device ... ");
		fflush(stdout);

		for(i=0; serialport_selection[i]; i++) {
			port = serialport_selection[i];
			try {
				dbg->connect_serial(port, baudrate);
			} catch(char *err) {
				continue;
			}

			try {
				dbg->reset_link();
			} catch(char *err) {
				dbg->disconnect();
				continue;
			}

			printf("found on %s\n", port);
			return 0;
		}

		printf("fail\n");
		printf("Could not connect to device.\n");

		return -1;

	} else {
		try {
			dbg->connect_serial(serialport, baudrate);
		} catch(char *err) {
			printf("Could not connect to device\n");
			fprintf(stderr, "%s", err);
			return -1;
		}
		try {
			dbg->reset_link();
		} catch(char *err) {
			printf("Could not communicate with device\n");
			fprintf(stderr, "%s", err);
			dbg->disconnect();
			return -1;
		}
	}

	return 0;
}

/**************************************************************/

int display_info(void)
{
	uint16_t crc;

	printf("Reading device info ... ");
	fflush(stdout);
	try {
		if(crc_size >= 0) {
			dbg->rd_mem(0, buff, crc_size);
			crc = crc_ccitt(0, buff, crc_size);
		} else {
			crc = dbg->rd_crc();
		}
	} catch(char *err) {
		printf("fail\n");
		fprintf(stderr, "%s", err);
		return -1;
	}
	printf("ok, crc: %04x\n", crc);
	try {
		if(dbg->state(dbg->state_protected)) {
			printf("Read Protect ... enabled\n");
		} else {
			printf("Read Protect ... disabled\n");
		}
	} catch(char *err) {
		fprintf(stderr, "%s", err);
		return -1;
	}

	return 0;
}

/**************************************************************/

void serialize(void)
{
	int i;
	uint16_t address;
	uint32_t number;

	if(!serial_size) {
		return;
	}

	address = serial_address + serial_size - 1;
	number = serial_number;

	for(i=serial_size; i>0; i--) {
		buff[address] = number & 0xff;
		address--;
		number >>= 8;
	}

	buff_crc = crc_ccitt(0x0000, buff, mem_size);
	printf("Serial number: %0*x, crc: %04x\n", serial_size * 2, 
	    serial_number, buff_crc);

	serial_number++;

	return;
}

/**************************************************************/

int save_file(const char *filename)
{
	int err;
	uint16_t crc;

	assert(filename != NULL);

	printf("Saving memory to file: %s\n", filename);
	try {
		if(dbg->state(dbg->state_protected)) {
			fprintf(stderr, 
			    "ERROR: memory read protect is enabled\n");
			return -1;
		}
	} catch(char *err) {
		fprintf(stderr, "%s", err);
		return -1;
	}

	printf("Reading memory ... ");
	fflush(stdout);

	try {
		dbg->rd_mem(0x0000, buff, mem_size);
	} catch(char *err) {
		printf("fail\n");
		fprintf(stderr, "%s", err);
		return -1;
	}

	try {
		crc = dbg->rd_crc();
	} catch(char *err) {
		printf("fail\n");
		fprintf(stderr, "%s", err);
		return -1;
	}

	buff_crc = crc_ccitt(0x0000, buff, mem_size);
	if(buff_crc != crc) {
		printf("fail, crc calculated = %04x, device = %04x\n",
		    buff_crc, crc);
		fprintf(stderr, "ERROR: CRC check failed.\n");
		//return -1;
	} else {
		printf("ok, crc: %04x\n", crc);
	}

	printf("Saving file ... ");
	fflush(stdout);

	err = wr_hexfile(buff, mem_size, 0x0000, filename);
	if(err) {
		printf("fail\n");
		return -1;
	}
	printf("ok\n");

	return 0;
}

/**************************************************************/

int load_file(const char *filename)
{
	int err;
	uint8_t c;

	assert(filename != NULL);

	printf("Reading file: %s ... ", filename);
	fflush(stdout);

	memset(buff, zero_fill ? 0x00 : 0xff, MEMSIZE);

	err = rd_hexfile(buff, MEMSIZE, filename);
	if(err) {
		printf("fail\n");
		return -1;
	}
	printf("ok\n");

	/* find max used memory */
	c = zero_fill ? 0x00 : 0xff;
	for(max_mem=MEMSIZE-1; max_mem>0; max_mem--) {
		if(buff[max_mem] != c) {
			break;
		}
	}

	return 0;
}

/**************************************************************/

int erase_device(void)
{
	uint16_t crc;

	printf("Erasing device ... ");
	fflush(stdout);

	try {
#ifdef	TEST
		if(erase) {
			extern void prepare_for_erase(void);
			prepare_for_erase();
			dbg->mass_erase(1);
			dbg->reset_chip();
			mem_size = dbg->memory_size();
			blank_crc = crc_ccitt(0x0000, blank, mem_size);
		} else
#endif
			dbg->flash_mass_erase();
	} catch(char *err) {
		printf("fail\n");
		fprintf(stderr, "%s", err);
		return -1;
	}

	/* if memory read protect enabled, 
	 * reset after erased to clear it */
	if(dbg->state(dbg->state_protected)) {
		try {
			dbg->reset_chip();
		} catch(char *err) {
			printf("fail\n");
			fprintf(stderr, "%s", err);
			return -1;
		}
	}

	printf("ok\n");

	printf("Blank check ... ");
	fflush(stdout);

	try {
		crc = dbg->rd_crc();
	} catch(char *err) {
		printf("fail\n");
		fprintf(stderr, "%s", err);
		return -1;
	}

	if(crc != blank_crc) {
		printf("fail, crc: %04x\n", crc);
		return -1;
	} else {
		printf("ok, crc: %04x\n", crc);
	}

	return 0;
}

/**************************************************************/

int program_device()
{
	uint16_t crc;

	printf("Programming device ... ");
	fflush(stdout);
	try {
		dbg->wr_mem(0x0000, buff, 0x10000);
	} catch(char *err) {
		printf("fail\n");
		fprintf(stderr, "%s", err);
		return -1;
	}
 
	printf("ok\n");

	printf("Verifying ... ");
	fflush(stdout);
	try {
		crc = dbg->rd_crc();
	} catch(char *err) {
		printf("fail\n");
		fprintf(stderr, "%s", err);
		return -1;
	}

	if(crc != buff_crc) {
		printf("fail, crc: %04x\n", crc);
		return -1;
	} else {
		printf("ok, crc: %04x\n", crc);
	}

	return 0;
}

/**************************************************************/

int singlepassmode(void)
{
	int err;

	err = connect();
	if(err) {
		return -1;
	}

	try {
		dbg->stop();
	} catch(char *err) {
		fprintf(stderr, "%s", err);
		return -1;
	}

	try {
		dbg->reset_chip();
	} catch(char *err) {
		fprintf(stderr, "%s", err);
		return -1;
	}

	mem_size = dbg->memory_size();
	printf("Memory size: %dk\n", mem_size / 1024);

	if(info) {
		err = display_info();
		if(err) {
			return -1;
		}
	}

	if(savefilename) {
		err = save_file(savefilename);
		if(err) {
			return -1;
		}
	}

	if(programfilename) {
		err = load_file(programfilename);
		if(err) {
			return -1;
		}
	}

	if(mem_size <= max_mem) {
		fprintf(stderr, "ERROR: data to large for device\n");
		return -1;
	}
	if(mem_size < serial_address + serial_size) {
		fprintf(stderr, 
		    "ERROR: serial address out-of-range for device\n");
		return -1;
	}


	if(erase || programfilename) {
		blank_crc = crc_ccitt(0x0000, blank, mem_size);

		err = erase_device();
		if(err) {
			return -1;
		}
	}

	if(programfilename) {
		serialize();
		buff_crc = crc_ccitt(0x0000, buff, mem_size);
		err = program_device();
		if(err) {
			return -1;
		}
	}

	return err;
}

/**************************************************************/

int multipassmode(void)
{
	int err;
	int connected;
	char *input;
	int size;

	printf("Multipass mode\n");
	connected = 0;

	err = load_file(programfilename);
	if(err) {
		return -1;
	}

	while(multipass) {
		printf("\n");
		rl_num_chars_to_read = 1;
		input = readline(
		    "Press any key to continue ('Q' to quit) ... ");
		rl_num_chars_to_read = 0;
		if(!input || toupper(*input) == 'Q') {
			multipass = 0;
			break;
		}
		free(input);

		try {
			if(!connected) {
				if(connect()) {
					connected = 0;
					continue;
				} else {
					connected = 1;
				}
			} else {
				dbg->reset_link();
			}

			dbg->stop();
			dbg->reset_chip();

			size = dbg->memory_size();
			printf("Memory size: %dk\n", size / 1024);

			if(size <= max_mem) {
				fprintf(stderr, 
				    "ERROR: data out-of-range\n");
				continue;
			}
			if(size < serial_address + serial_size) {
				fprintf(stderr, 
				    "ERROR: serial address out-of-range\n");
				continue;
			}

			if(size != mem_size) {
				blank_crc = crc_ccitt(0x0000, blank, size);
				buff_crc = crc_ccitt(0x0000, buff, size);
				mem_size = size;
			}

			err = erase_device();
			if(err) {
				continue;
			}

			serialize();

			err = program_device();
			if(err) {
				continue;
			}

		} catch(char *err) {
			fprintf(stderr, "%s", err);
			continue;		
		}
	} 

	return 0;
}

/**************************************************************/

int main(int argc, char **argv)
{
	int err;

	err = setup(argc, argv);
	if(err) {
		return EXIT_FAILURE;
	}

	printf("%s - build %s\n", banner, build);

	if(multipass) {
		err = multipassmode();
	} else {
		err = singlepassmode();
	}

	if(err) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/**************************************************************/



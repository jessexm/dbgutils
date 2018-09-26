/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: serialport.cpp,v 1.7 2009/01/22 15:03:16 jnekl Exp $
 *
 * This is a universial serial port api. It will work on
 * both unix and windows systems.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<unistd.h>
#include	<string.h>
#include	"limits.h"
#include	<sys/types.h>
#include	<assert.h>

#include	<fcntl.h>
#include	<errno.h>

#ifndef	_WIN32
#include	<termios.h>
#else	/* _WIN32 */
#include	"winunistd.h"
#endif	/* _WIN32 */

#include	"serialport.h"
#include	"err_msg.h"

/**************************************************************/

extern "C" const struct baudvalue baudrates[] = {
#ifndef	_WIN32

#ifdef	B50
	{ 50, B50 },
#endif	/* B50 */
#ifdef	B75
	{ 75, B75 },
#endif	/* B75 */
#ifdef	B110
	{ 110, B110 },
#endif	/* B110 */
#ifdef	B134
	{ 134, B134 },
#endif	/* B134 */
#ifdef	B150
	{ 150, B150 },
#endif	/* B150 */
#ifdef	B200
	{ 200, B200 },
#endif	/* B200 */
#ifdef	B300
	{ 300, B300 },
#endif	/* B300 */
#ifdef	B600
	{ 600, B600 },
#endif	/* B600 */
#ifdef	B1200
	{ 1200, B1200 },
#endif	/* B1200 */
#ifdef	B1800
	{ 1800, B1800 },
#endif	/* B1800 */
#ifdef	B2400
	{ 2400, B2400 },
#endif	/* B2400 */
#ifdef	B4800
	{ 4800, B4800 },
#endif	/* B4800 */
#ifdef	B9600
	{ 9600, B9600 },
#endif	/* B9600 */
#ifdef	B19200
	{ 19200, B19200 },
#endif	/* B19200 */
#ifdef	B38400
	{ 38400, B38400 },
#endif	/* B38400 */
#ifdef	B57600
	{ 57600, B57600 },
#endif	/* B57600 */
#ifdef	B76800
	{ 76800, B76800 },
#endif	/* B76800 */
#ifdef	B115200
	{ 115200, B115200 },
#endif	/* B115200 */
#ifdef	B153600
	{ 153600, B153600 },
#endif	/* B153600 */
#ifdef	B230400
	{ 230400, B230400 },
#endif	/* B230400 */
#ifdef	B460800
	{ 460800, B460800 },
#endif	/* B460800 */

#else	/* _WIN32 */
	{ 110, CBR_110 },
	{ 300, CBR_300 },
	{ 600, CBR_600 },
	{ 1200, CBR_1200 },
	{ 2400, CBR_2400 },
	{ 4800, CBR_4800 },
	{ 9600, CBR_9600 },
	{ 14400, CBR_14400 },
	{ 19200, CBR_19200 },
	{ 38400, CBR_38400 },
	{ 56000, CBR_56000 },
	{ 57600, CBR_57600 },
	{ 115200, CBR_115200 },
	{ 256000, CBR_256000 },
#endif	/* _WIN32 */
	{ 0, 0 }
};

/**************************************************************
 */

#ifdef	_WIN32
char *winerr(char *s, size_t n)
{
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
	          | FORMAT_MESSAGE_IGNORE_INSERTS | 60,
	          NULL, GetLastError(), 0, 
	          (LPSTR)s, n < 65535 ? n : 65535, NULL);

	return s;
}
#endif

/**************************************************************
 * This will translate a baudrate into the given OS's #define
 * for that baudrate.
 */

static int baud2def(int baudrate)
{
	const struct baudvalue *b;

	b = baudrates;
	while(b->value) {
		if(baudrate == b->value) {
			return b->define;
		}
		b++;
	}

	return -1;
}

/**************************************************************
 * This will translate an OS's given #define for a baudrate
 * into it's baud value.
 */

static int def2baud(int define)
{
	const struct baudvalue *b;

	b = baudrates;

	while(b->value) {
		if(define == b->define) {
			return b->value;
		}
		b++;
	}

	return -1;
}

/**************************************************************
 * Constructor for serial port.
 */

serialport::serialport()
#ifndef	_WIN32
{
	fdes = -1;
	baudrate = 0;
	size = eight;
	parity = none;
	stopbits = one;
	flowcontrol = 0;
	timeout = 0;

	memset(rxbuff, 0, sizeof(rxbuff));
	len = 0;

	return;
}
#else	/* _WIN32 */
{
	fdes = INVALID_HANDLE_VALUE;
	baudrate = 0;
	size = eight;
	parity = none;
	stopbits = one;
	flowcontrol = 0;

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * Destructor for serial port. If serial port open, close it.
 */

serialport::~serialport()
#ifndef	_WIN32
{
	if(fdes >= 0) {
		close();
	}

	return;
}
#else	/* _WIN32 */
{
	if(fdes != INVALID_HANDLE_VALUE) {
		close();
	}

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * Set unix advisory file lock on serial port.
 */

#ifndef	_WIN32
void serialport::setflock(void)
{
	int err;
	struct flock filelock;

	memset(&filelock, 0, sizeof(filelock));

	filelock.l_type = F_WRLCK;
	filelock.l_whence = SEEK_SET;
	filelock.l_start = 0;
	filelock.l_len = 0;

	assert(fdes >= 0);

	err = fcntl(fdes, F_SETLK, &filelock);
	if(err < 0) {
		::close(fdes);
		fdes = -1;
		snprintf(err_msg, err_len-1,
		    "Could not obtain resource lock for serial port\n"
		    "fcntl:%s\n", strerror(errno));
		throw err_msg;
	}

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * This call will open a serial port. It does not configure it,
 * configuration is left for the configure() user function call.
 */

void serialport::open(const char *device)
#ifndef	_WIN32
{
	assert(device != NULL);

	if(fdes >= 0) {
		strncpy(err_msg, "Could not open serial port\n"
		    "serial port is already open\n", err_len-1);
		throw err_msg;
	}

	fdes = ::open(device, O_RDWR);
	if(fdes < 0) {
		snprintf(err_msg, err_len-1,
		    "Could not open serial port\n"
		    "open:%s\n", strerror(errno));
		throw err_msg;
	}

	setflock();

	if(!isatty(fdes)) {
		close();
		strncpy(err_msg, "Could not open serial port\n"
		    "device is not a tty\n", err_len-1);
		throw err_msg;
	}

	return;
}
#else	/* _WIN32 */
{
	assert(device != NULL);
	
	if(fdes != INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Could not open serial port\n"
		    "serial port already opened\n", err_len-1);
		throw err_msg;
	}

	fdes = CreateFile(device, GENERIC_READ | GENERIC_WRITE, 0, 
	                  NULL, OPEN_EXISTING, 0, NULL);

	if(fdes == INVALID_HANDLE_VALUE) {
		char *s;
		size_t n;

		strncpy(err_msg, "Could not open serial port\n"
		    "CreateFile:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * This will close the serial port.
 */

void serialport::close(void)
#ifndef	_WIN32
{
	int err;

	if(fdes < 0) {
		strncpy(err_msg, "Could not close serial port\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	/* call tcflush to prevent close() from hanging */
	tcflush(fdes, TCIOFLUSH);

	err = ::close(fdes);
	if(err) {
		snprintf(err_msg, err_len-1, "Could not close serial port\n"
		    "close:%s\n", strerror(errno));
		throw err_msg;
	}

	fdes = -1;	

	return;
}
#else	/* _WIN32 */
{
	int err;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Could not close serial port\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	err = ! CloseHandle(fdes);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Could not close serial port\n"
		    "CloseHandle:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	fdes = INVALID_HANDLE_VALUE;

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * This will configure the currently open serial port with 
 * the data members in the current object.
 */

void serialport::configure(void)
#ifndef	_WIN32
{
	int err;
	int bauddefine;
	struct termios cfg;

	if(fdes < 0) {
		strncpy(err_msg, "Configure serial port failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	assert(isatty(fdes));

	bauddefine = baud2def(baudrate);
	if(bauddefine <= 0) {
		strncpy(err_msg, "Configure serial port failed\n"
			    "invalid baudrate\n", err_len-1);
		throw err_msg;
	}

	err = tcgetattr(fdes, &cfg);
	if(err) {
		snprintf(err_msg, err_len-1, "Configure serial port failed\n"
		    "tcgetattr:%s\n", strerror(errno));
		throw err_msg;
	}

	err = cfsetospeed(&cfg, bauddefine);
	if(err) {
		snprintf(err_msg, err_len-1, "Configure serial port failed\n"
		    "cfsetospeed:%s\n", strerror(errno));
		throw err_msg;
	}

	err = cfsetispeed(&cfg, 0);
	if(err) {
		snprintf(err_msg, err_len-1, "Configure serial port failed\n"
		    "cfsetispeed:%s\n", strerror(errno));
		throw err_msg;
	}

	cfg.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | ISTRIP | INLCR
	                  | IGNCR | ICRNL | IUCLC | IMAXBEL);
	cfg.c_iflag |= (INPCK | PARMRK);
	cfg.c_oflag &= ~(OPOST);
	cfg.c_cflag |= (CREAD);
	cfg.c_lflag &= ~(ISIG | ICANON | ECHO | NOFLSH | TOSTOP | PENDIN 
	                  | IEXTEN);

	switch(size) {
	case eight:
		cfg.c_cflag &= ~(CSIZE);
		cfg.c_cflag |= (CS8);
		break;
	case seven:
		cfg.c_cflag &= ~(CSIZE);
		cfg.c_cflag |= (CS7);
		break;
	case six:
		cfg.c_cflag &= ~(CSIZE);
		cfg.c_cflag |= (CS6);
		break;
	case five:
		cfg.c_cflag &= ~(CSIZE);
		cfg.c_cflag |= (CS5);
		break;
	default:
		abort();
	}

	switch(parity){
	#ifdef CMSPAR
	case odd:
		cfg.c_cflag |= (PARENB | PARODD);
		cfg.c_cflag &= ~(CMSPAR);
		break;
	case even:
		cfg.c_cflag |= (PARENB);
		cfg.c_cflag &= ~(PARODD | CMSPAR);
		break;
	case mark:
		cfg.c_cflag |= (PARENB | PARODD | CMSPAR);
		break;
	case space:
		cfg.c_cflag |= (PARENB | CMSPAR);
		cfg.c_cflag &= ~(PARODD);
	case none:
		cfg.c_cflag &= ~(PARENB | PARODD | CMSPAR);
		break;
	#else	/* CMSPAR */
	case odd:
		cfg.c_cflag |= (PARENB | PARODD);
		break;
	case even:
		cfg.c_cflag |= (PARENB);
		cfg.c_cflag &= ~(PARODD);
		break;
	case mark:
		snprintf(err_msg, err_len-1, "Configure serial port failed\n"
		    "mark parity not supported on this system\n");
		throw err_msg;
	case space:
		snprintf(err_msg, err_len-1, "Configure serial port failed\n"
		    "space parity not supported on this system\n");
		throw err_msg;
	case none:
		cfg.c_cflag &= ~(PARENB | PARODD);
		break;
	#endif	/* CMSPAR */
	default:
		abort();
	}


	switch(stopbits) {
	case one:
		cfg.c_lflag &= ~(CSTOPB);
		break;
	case two:
		cfg.c_lflag |= (CSTOPB);
		break;
	default:
		abort();
	}

	cfg.c_iflag &= ~(IXOFF | IXON | IXANY);
	cfg.c_cflag |= CLOCAL;
	cfg.c_cflag &= ~(HUPCL);

	if(flowcontrol & SERIAL_XONXOFF_OUTPUT) {
		cfg.c_iflag |= IXON;
	}
	if(flowcontrol & SERIAL_XONXOFF_INPUT) {
		cfg.c_iflag |= IXOFF;
	}

	if(flowcontrol & SERIAL_RTS_INPUT) {
		#if defined CRTSXOFF 
		cfg.c_cflag |= CRTSXOFF;
		#elif defined CRTS_IFLOW
		cfg.c_cflag |= CRTS_IFLOW;
		#endif
	}

	if(flowcontrol & SERIAL_CTS_OUTPUT) {
		#if defined CRTSCTS
		cfg.c_cflag |= CRTSCTS;
		#elif defined CCTS_OFLOW 
		cfg.c_cflag |= CCTS_OFLOW;
		#endif
	}

	/* Round timeout up to nearest 100ms */
	cfg.c_cc[VTIME] = timeout / 100 + (timeout % 100 ? 1 : 0);
	cfg.c_cc[VMIN] = 0;

	err = tcsetattr(fdes, TCSADRAIN, &cfg);
	if(err) {
		snprintf(err_msg, err_len-1, "Configure serial port failed\n"
		    "tcsetattr:%s\n", strerror(errno));
		throw err_msg;
	}

	return;
}
#else	/* _WIN32 */
{
	int err;
	DCB cfg;
	COMMTIMEOUTS cto;
	int bauddefine;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Configure serial port failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}
		
	err = !GetCommState(fdes, &cfg);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Configure serial port failed\n"
		    "GetCommState:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	bauddefine = baud2def(baudrate);
	if(bauddefine < 0) {
		strncpy(err_msg, "Configure serial port failed\n"
		    "invalid baudrate\n", err_len-1);
		throw err_msg;
	}

	cfg.BaudRate = bauddefine;
	cfg.fBinary = TRUE;
	cfg.fErrorChar = FALSE;
	cfg.fNull = FALSE;
	cfg.fAbortOnError = FALSE;
	cfg.wReserved = 0;

	
	switch(size) {
	case eight:
		cfg.ByteSize = 8;
		break;
	case seven:
		cfg.ByteSize = 7;
		break;
	case six:
		cfg.ByteSize = 6;
		break;
	case five:
		cfg.ByteSize = 5;
		break;
	default:
		abort();
	}


	switch(parity) {
	case odd:
		cfg.fParity = TRUE;
		cfg.Parity = ODDPARITY;
		break;
	case even:
		cfg.fParity = TRUE;
		cfg.Parity = EVENPARITY;
		break;
	case mark:
		cfg.fParity = TRUE;
		cfg.Parity = MARKPARITY;
		break;
	case space:
		cfg.fParity = TRUE;
		cfg.Parity = SPACEPARITY;
		break;
	case none:
		cfg.fParity = FALSE;
		cfg.Parity = NOPARITY;
		break;
	default:
		abort();
	}


	switch(stopbits) {
	case one:
		cfg.StopBits = ONESTOPBIT;
		break;
	case two:
		cfg.StopBits = TWOSTOPBITS;
		break;
	default:
		abort();
	}

	cfg.fOutxCtsFlow = FALSE;
	cfg.fOutxDsrFlow = FALSE;
	cfg.fDtrControl = DTR_CONTROL_DISABLE;
	cfg.fDsrSensitivity = FALSE;
	cfg.fTXContinueOnXoff = FALSE;
	cfg.fOutX = FALSE;
	cfg.fInX = FALSE;
	cfg.fRtsControl = RTS_CONTROL_DISABLE;

	if(flowcontrol & SERIAL_XONXOFF_INPUT) {
		cfg.fInX = TRUE;
	}
	if(flowcontrol & SERIAL_XONXOFF_OUTPUT) {
		cfg.fOutX = TRUE;
	}

	if(flowcontrol & SERIAL_CTS_OUTPUT) {
		cfg.fOutxCtsFlow = TRUE;
	}

	if(flowcontrol & SERIAL_RTS_INPUT) {
		cfg.fRtsControl = RTS_CONTROL_HANDSHAKE;
	}

	err = !SetCommState(fdes, &cfg);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Configure serial port failed\n"
		    "SetCommState:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	err = !GetCommTimeouts(fdes, &cto);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Configure serial port failed\n"
		    "GetCommTimeouts:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	cto.ReadIntervalTimeout = timeout ? timeout : MAXDWORD;
	cto.ReadTotalTimeoutMultiplier = timeout ? 2*1000*10/baudrate + 1 : 0;
	cto.ReadTotalTimeoutConstant = timeout;
	cto.WriteTotalTimeoutMultiplier = 0;
	cto.WriteTotalTimeoutConstant = 0;

	err = !SetCommTimeouts(fdes, &cto);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Configure serial port failed\n"
		    "SetCommTimeouts:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}	

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * Read current serial port configuration.
 */

void serialport::loadconfig(void)
#ifndef	_WIN32
{
	int err;
	int baud;
	struct termios cfg;

	if(fdes < 0) {
		strncpy(err_msg, "Get serial port configuration failed\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}

	assert(isatty(fdes));

	err = tcgetattr(fdes, &cfg);
	if(err < 0) {
		snprintf(err_msg, err_len-1, 
		    "Get serial port configuration failed\n"
		    "tcgetattr:%s\n", strerror(errno));
		throw err_msg;
	}

	baud = cfgetospeed(&cfg);
	if(baud < 0) {
		snprintf(err_msg, err_len-1,
		    "Get serial port configuration failed\n"
		    "cfgetospeed:%s\n", strerror(errno));
		throw err_msg;
	}

	baudrate = def2baud(baud);
	if(baudrate < 0) {
		strncpy(err_msg, "Get serial port configuration failed\n"
		    "unknown baudrate\n", err_len-1);
		throw err_msg;
	}


	switch(cfg.c_cflag & CSIZE) {
	case CS8:
		size = eight;
		break;
	case CS7:
		size = seven;
		break;
	case CS6:
		size = six;
		break;
	case CS5:
		size = five;
		break;
	default:
		strncpy(err_msg, "Get serial port configuration failed\n"
		    "unknown character size\n", err_len-1);
		throw err_msg;
	}

	if(cfg.c_cflag & PARENB) {
		if(cfg.c_cflag & PARODD) {
			#ifdef CMSPAR
			if(cfg.c_cflag & CMSPAR) {
				parity = mark;
			} else {
				parity = odd;
			}
			#else	/* CMSPAR */
			parity = odd;
			#endif	/* CMSPAR */
		} else {
			#ifdef	CMSPAR
			if(cfg.c_cflag & CMSPAR) {
				parity = space;
			} else {
				parity = even;
			}
			#else	/* CMSPAR */
			parity = even;
			#endif	/* CMSPAR */
		}
	} else {
		parity = none;
	}

	if(cfg.c_lflag & CSTOPB) {
		stopbits = two;
	} else {
		stopbits = one;
	}

	flowcontrol = 0;

	if(cfg.c_iflag & IXOFF) {
		flowcontrol |= SERIAL_XONXOFF_INPUT;
	}
	if(cfg.c_iflag & IXON) {
		flowcontrol |= SERIAL_XONXOFF_OUTPUT;
	}
	#if defined CRTSXOFF
	if(cfg.c_cflag) {
		flowcontrol |= SERIAL_RTS_INPUT;
	}
	#elif defined CCTS_OFLOW 
	if(cfg.c_cflag & CTS_OFLOW) {
		flowcontrol |= SERIAL_RTS_INPUT;
	}
	#endif
	
	#if defined CRTSCTS
	if(cfg.c_cflag & CRTSCTS) {
		flowcontrol |= SERIAL_CTS_OUTPUT;
	}
	#elif defined CRTS_IFLOW
	if(cfg.c_cflag & CRTS_IFLOW) {
		flowcontrol |= SERIAL_CTS_OUTPUT;
	}
	#endif

	timeout = cfg.c_cc[VTIME] * 100;

	return;
}
#else	/* _WIN32 */
{
	int err;
	DCB cfg;
	COMMTIMEOUTS cto;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Get serial port configuration failed\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}

	err = !GetCommState(fdes, &cfg);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Get serial port configuration failed\n"
		    "GetCommState:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	baudrate = def2baud(cfg.BaudRate);
	if(baudrate < 0) {
		strncpy(err_msg, "Get serial port configuration failed\n"
		    "invalid baudrate\n", err_len-1);
		throw err_msg;
	}

	switch(cfg.ByteSize) {
	case 8:
		size = eight;
		break;
	case 7:
		size = seven;
		break;
	case 6:
		size = six;
		break;
	case 5:
		size = five;
		break;
	default:
		strncpy(err_msg, "Get serial port configuration failed\n"
		    "invalid character size\n", err_len-1);
		throw err_msg;
	}

	if(cfg.fParity == TRUE) {
		switch(cfg.Parity) {
		case ODDPARITY:
			parity = odd;
			break;
		case EVENPARITY:
			parity = even;
			break;
		case MARKPARITY:
			parity = mark;
			break;
		case SPACEPARITY:
			parity = space;
			break;
		case NOPARITY:
			parity = none;
			break;
		default:
			strncpy(err_msg, 
			    "Get serial port configuration failed\n"
			    "invalid parity\n", err_len-1);
			throw err_msg;
		}
	} else {	
		parity = none;
	}

	switch(cfg.StopBits) {
	case ONESTOPBIT:
		stopbits = one;
		break;
	case TWOSTOPBITS:
		stopbits = two;
		break;
	default:
		strncpy(err_msg, "Get serial port configuration failed\n"
		    "invalid stop bits\n", err_len-1);
		throw err_msg;
	}

	flowcontrol = 0;

	if(cfg.fOutX == TRUE) {
		flowcontrol |= SERIAL_XONXOFF_OUTPUT;
	} 
	if(cfg.fInX == TRUE) {
		flowcontrol |= SERIAL_XONXOFF_INPUT;
	}
	if(cfg.fRtsControl == RTS_CONTROL_HANDSHAKE) {
		flowcontrol |= SERIAL_RTS_INPUT;
	}
	if(cfg.fOutxCtsFlow == TRUE) {
		flowcontrol |= SERIAL_CTS_OUTPUT;
	}

	err = !GetCommTimeouts(fdes, &cto);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Get serial port configuration failed\n"
		    "GetCommTimeouts:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	timeout = cto.ReadIntervalTimeout == MAXDWORD ? 
	    0 : cto.ReadIntervalTimeout;

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * This will set the error flag based on windows 
 * ClearCommError.
 */

#ifdef	_WIN32
void serialport::seterror(char *s, size_t n)
{
	int err;
	DWORD err_status;
	COMSTAT cs;

	err = !ClearCommError(fdes, &err_status, &cs);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Clear communication error failed\n"
		    "ClearCommError:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	if(err_status & CE_BREAK) {
		strncpy(s, "break detected\n", n);
	}	
	if(err_status & CE_FRAME) {
		strncpy(s, "framing error detected\n", n);
	}
	if(err_status & CE_RXPARITY) {
		strncpy(s, "parity error detected\n", n);
	}
	if(err_status & CE_IOE) {
		strncpy(s, "hardware I/O error\n", n);
	}
	if(err_status & CE_MODE) {
		strncpy(s, "invalid mode\n", n);
	}
	if(err_status & CE_OVERRUN) {
		strncpy(s, "character buffer overrun\n", n);
	}
	if(err_status & CE_RXOVER) {
		strncpy(s, "receive overrun\n", n);
	}
	if(err_status & CE_TXFULL) {
		strncpy(s, "transmit buffer full\n", n);
	}

	return;
}
#endif	/* _WIN32 */

/**************************************************************
 * This function will return true if there is data available
 * to be read from the serial port.
 */

bool serialport::available(void)
#ifndef	_WIN32
{
	int ready;
	fd_set rd_fdes;
	struct timeval t;

	if(fdes < 0) {
		strncpy(err_msg, "Read serial port failed\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}

	FD_ZERO(&rd_fdes);
	FD_SET(fdes, &rd_fdes);

	t.tv_sec = 0;
	t.tv_usec = 0;

	do {
		ready = select(FD_SETSIZE, &rd_fdes, NULL, NULL, &t);
	} while(ready < 0 && errno == EINTR);

	if(ready < 0) {
		snprintf(err_msg, err_len-1,
		    "Read serial port failed\n"
		    "select:%s\n", strerror(errno));
		throw err_msg;
	}

	if(!ready) {
		return 0;
	}

	return 1;
}
#else	/* _WIN32 */
{
	DWORD err;
	COMSTAT stat;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Serial port read failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	ClearCommError(fdes, &err, &stat);

	if(!stat.cbInQue) {
		return 0;
	}

	return 1;
}
#endif

/**************************************************************
 * This will read data from the serial port. It returns the
 * number of bytes actually read.
 *
 * The number of bytes read may be less than the number
 * of bytes requested. This happens when the timeout occurs.
 */

int serialport::read(void *buff, size_t size)
#ifndef	_WIN32

/* Quick rundown of the unix tty driver api 
 *
 * Errors received are stuffed within the data stream. Errors
 * are escaped with the sequence [0xff,0x00,0x??]. If the third byte
 * is 0x00, a serial break was detected (line held in space state
 * for more than nine bit times (for eight bit characters)). If the 
 * third character is not 0x00, there was either a framing error or 
 * parity error, but we cannot distinguish which.
 *
 * Since the character 0xff is used to escape errors, if this character
 * occurs in the actual data stream, it is duplicated [0xff,0xff].
 */
{
	size_t bytes_read = 0;
	unsigned char *dst = (unsigned char *)buff;
	enum { s_normal, s_escape, s_error } state = s_normal;

	if(fdes < 0) {
		strncpy(err_msg, "Read serial port failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}	

	while(size > 0) {
		unsigned char *src;
		int count;

		if(!len) {
			/* read data from serial port */
			do {
				count = ::read(fdes, dst, size);
			} while(count < 0 && errno == EINTR);
		} else {
			/* if data in read-ahead buffer */
			count = 1;
			*dst = *rxbuff;
			rxbuff[0] = rxbuff[1];
			rxbuff[1] = rxbuff[2];
			rxbuff[2] = rxbuff[3];
			len--;
		}

		if(!count) {
			switch(state) {
			case s_normal:
				/* timeout reading data */
				return bytes_read;
			case s_escape:
			case s_error:
				fprintf(stderr, "Bug in tty driver\n"
				    "read timeout during escape\n");
			default:
				abort();
			}
		}

		/* error reading data */
		if(count < 0) {
			switch(state) {
			case s_normal:
				snprintf(err_msg, err_len-1,
				    "Read serial port failed\n"
				    "read:%s\n", strerror(errno));
				throw err_msg;
			case s_escape:
			case s_error:
				fprintf(stderr, "Bug in tty driver\n"
				    "read error during escape\n");
			default:
				abort();
			}
		}

		assert(count <= (int)size);

		for(src=dst; count>0; count--, src++) {
			switch(state) {
			case s_normal:
				if(*src == 0xff) {
					/* If an escape character
					 * goto escape state */
					state = s_escape;
				} else {
					/* If a normal character, 
					 * copy and continue */
					*dst++ = *src;
					bytes_read++;
					size--;
				}
				break;
			case s_escape:
				if(*src == 0xff) {
					/* If an escaped character 0xff, 
					 * add to data stream */
					*dst++ = *src;
					bytes_read++;
					size--;
					state = s_normal;
				} else if(!*src) {
					/* if parity, framing, or break,
					 * goto error state */
					state = s_error;
				} else {
					fprintf(stderr, "Bug in tty driver\n"
					    "invalid escaped character\n");
					abort();
				}
				break;
			case s_error:
				if(*src) {
					strncpy(err_msg,
					    "Serial port read failed\n"
					    "framing error detected\n", 
					    err_len-1);

				} else {
					strncpy(err_msg, 
					    "Serial port read failed\n"
					    "break detected\n", 
					    err_len-1);
				}
				throw err_msg;
			default:
				abort();
			}
		}
	}

	return bytes_read;
}
#else	/* _WIN32 */
{
	int err;
	DWORD bytes_read;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Serial port read failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	assert(buff != NULL);

	err = !ReadFile(fdes, buff, size, &bytes_read, NULL);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Serial port read failed\n"
		    "ReadFile:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		seterror(s, n);
		throw err_msg;
	}

	return bytes_read;
}
#endif	/* _WIN32 */

/**************************************************************
 * This will write data to the serial port. 
 */

void serialport::write(const void *buff, size_t size)
#ifndef	_WIN32
{
	ssize_t bytes_written;

	if(fdes < 0) {
		strncpy(err_msg, "Serial port write failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	assert(buff != NULL);

	do {
		do {
			bytes_written = ::write(fdes, buff, size);
		} while(bytes_written < 0 && errno == EINTR);

		if(bytes_written > 0) {
			assert((size_t)bytes_written <= size);
			size -= bytes_written;
			buff = (unsigned char *)buff + bytes_written;
		}

	} while(bytes_written >= 0 && size > 0);

	if(bytes_written < 0) {
		snprintf(err_msg, err_len-1, "Serial port write failed\n"
		    "write:%s\n", strerror(errno));
		throw err_msg;
	}

	return;
}
#else	/* _WIN32 */
{
	int err;
	DWORD bytes_written;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Serial port write failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	assert(buff != NULL);

	bytes_written = 0;

	err = !WriteFile(fdes, buff, size, &bytes_written, NULL);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Serial port write failed\n"
		    "WriteFile:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	if(bytes_written != size) {
		fprintf(stderr, "WriteFile: bytes_written != size\n");
		abort();
	}

	return;
}
#endif	/* _WIN32 */

/**************************************************************/

void serialport::sendbreak(void)
#ifndef	_WIN32
{
	int err;

	if(fdes < 0) {
		strncpy(err_msg, "Serial port send break failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	err = tcsendbreak(fdes, 0);
	if(err) {
		snprintf(err_msg, err_len-1, 
		    "Serial port send break failed\n"
		    "tcsendbreak:%s\n", strerror(errno));
		throw err_msg;
	}

	err = tcdrain(fdes);
	if(err) {
		snprintf(err_msg, err_len-1,
		    "Serial port send break failed\n"
		    "tcdrain:%s\n", strerror(errno));
		throw err_msg;
	}

	usleep(250000);

	return;
}
#else	/* _WIN32 */
{
	int err;
	DWORD status;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Serial port send break failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	ClearCommError(fdes, &status, NULL);
	err = !SetCommBreak(fdes);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Serial port send break failed\n"
		    "SetCommBreak:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	usleep(250*1000);

	ClearCommError(fdes, &status, NULL);
	err = !ClearCommBreak(fdes);

	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Serial port send break failed\n"
		    "ClearCommBreak:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	return;
}
#endif	/* _WIN32 */

/**************************************************************/

void serialport::flush(void)
#ifndef	_WIN32
{
	int err;

	if(fdes < 0) {
		strncpy(err_msg, "Serial port flush failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	len = 0;
	err = tcflush(fdes, TCIOFLUSH);
	if(err) {
		snprintf(err_msg, err_len-1, "Serial port flush failed\n"
		    "tcflush:%s\n", strerror(errno));
		throw err_msg;
	}

	return;
}
#else	/* _WIN32 */
{
	int err;
	DWORD status;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Serial port flush failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	ClearCommError(fdes, &status, NULL);
	err = !PurgeComm(fdes, PURGE_TXABORT | PURGE_RXABORT 
	          | PURGE_TXCLEAR | PURGE_RXCLEAR);
	if(err) {
		char *s;
		size_t n;

		strncpy(err_msg, "Serial port flush failed\n"
		    "PurgeComm:", err_len-1);
		s = strchr(err_msg, '\0');
		n = err_len - (err_msg - s) - 1;
		winerr(s, n);
		strncat(err_msg, "\n", err_len-1);
		throw err_msg;
	}

	return;
}
#endif	/* _WIN32 */

/**************************************************************/

#ifndef	_WIN32
void serialport::read_byte(unsigned char *ptr)
{
	int count;

	/* read data from serial port */
	do {
		count = ::read(fdes, ptr, 1);
	} while(count < 0 && errno == EINTR);

	if(count < 0) {
		snprintf(err_msg, err_len-1, "Read serial port failed\n"
		    "read:%s\n", strerror(errno));
		throw err_msg;
	}
	if(!count) {
		snprintf(err_msg, err_len-1, "Read serial port failed\n"
		    "read: timeout\n");
		throw err_msg;
	}
	len += count;
	return;
}
#endif

bool serialport::error(void)
#ifndef	_WIN32
{
	if(!available()) {
		return 0;
	}
	read_byte(&rxbuff[0]);
	if(rxbuff[0] != 0xff) {
		return 0;
	}
	read_byte(&rxbuff[1]);
	if(rxbuff[1] != 0x00) {
		return 0;
	}
	read_byte(&rxbuff[2]);
	return 1;
}
#else	/* _WIN32 */
{
	DWORD err;
	COMSTAT stat;

	if(fdes == INVALID_HANDLE_VALUE) {
		strncpy(err_msg, "Serial port read failed\n"
		    "serial port not opened\n", err_len-1);
		throw err_msg;
	}

	ClearCommError(fdes, &err, &stat);

	if(err & CE_BREAK || stat.cbInQue || stat.cbOutQue) {
		return 1;
	}

	return 0;
}
#endif	/* _WIN32 */

/**************************************************************/



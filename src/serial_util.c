/*
 * serial_util.c
 *
 *  Created on: Jun 9, 2025
 *      Author: g0kla
 */
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "serial_util.h"
#include "debug.h"

/* Forward Declarations */
int open_serial(char *devicename);
void close_serial(int fd);

int serial_send_cmd(char *serialdev, char * data, int len, unsigned char *response, int rlen) {
	int fd = open_serial(serialdev);
	if (!fd) {
		error_print("Error while initializing %s.\n", serialdev);
		return -1;
	}
	tcflush(fd,TCIOFLUSH );

	int p = write(fd, data, len);
	tcdrain(fd);
	if (p != len) {
		debug_print("Error writing data.  Sent %d but %d written\n",len, p);
		close_serial(fd);
		return -1;
	}
	//		else {
	//			debug_print("Wrote %d bytes\n",p);
	//		}
	usleep(200*1000);   //// Hmm, this delay seems to be critical, suggesting radio is slow or we do not have flow control setup correctly
	//		debug_print("Response:");
	int n = read(fd, response, rlen);
	if (response[0] == '?') {
		debug_print("Mic Command failed\n");
		close_serial(fd);
		return -1;
	}
	if (n < 0) {
		debug_print ("error %d reading data\n", errno);
		close_serial(fd);
		return 1;
	}
	//		int b = 0;
	//		if (n < rlen) {
	//			// try to read some more
	//			b = read(fd, response+n, rlen-n);
	//		}
	response[n] = 0; // terminate the string
	usleep(50*1000);
	/*
			int i;
			for (i=0; i< n; i++) {
	//			debug_print("%c",response[i]);
									debug_print(" %0x",response[i] & 0xff);
			}
			debug_print("\n");
	 */

	//		set_rts(g_serial_fd, 0);
	close_serial(fd);
	usleep(50*1000);
	return n;
}

int open_serial(char *devicename) {
	int fd;
	struct termios options;

	/* NOCTTY means we are not a terminal and dont want control codes.  NDELAY means we dont care about the state of the DCD line */
	if ((fd = open(devicename, O_RDWR | O_NOCTTY | O_NDELAY)) == -1) {
		error_print("mic_open_serial(): open()");
		return 0;
	}

	if (tcgetattr(fd, &options) == -1) {
		error_print("mic_open_serial(): tcgetattr()");
		return 0;
	}

	/*
	 * Set the baud rates to 9600, which is the default for the serial connection
	 */
	if (cfsetispeed(&options, B38400) == -1) {
		error_print("mic_open_serial(): cfsetispeed()");
		return 0;
	}
	if (cfsetospeed(&options, B38400) == -1) {
		error_print("mic_open_serial(): cfsetospeed()");
		return 0;
	}

	/*
	 * Enable the receiver and set local mode... The c_cflag member contains two options that should always be enabled,
	 * CLOCAL and CREAD. These will ensure that your program does not become the 'owner' of the port subject to sporatic
	 * job control and hangup signals, and also that the serial interface driver will read incoming data bytes.
	 */
	options.c_cflag |= (CLOCAL | CREAD);

	/* 8N1 */
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;

	//    options.c_cflag |= CRTSCTS; /* Enable hardware flow control */
	options.c_cflag &= ~CRTSCTS; /* disable hardware flow contol */
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG | IEXTEN); /* RAW Input */
	options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
			|INLCR|IGNCR|ICRNL|IXON);
	options.c_oflag &= ~OPOST; /* Raw output */


	/*
	 * Set the new options for the port...
	 */
	if (tcsetattr(fd, TCSANOW, &options) == -1) { //TCSANOW constant specifies that all changes should occur immediatel
		error_print("mic_open_serial: tcsetattr()\n");
		close_serial(fd);
	}
	return fd;
}

void close_serial(int fd) {
	if (close(fd) < 0)
		error_print("closeserial()");
}

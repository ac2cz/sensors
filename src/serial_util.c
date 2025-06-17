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


int serial_send_cmd(char *serialdev, speed_t speed, char * data, int len, unsigned char *response, int rlen) {
	int fd = open_serial(serialdev, speed);
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

	// TODO - this delay not needed if it is not the radio?
	usleep(200*1000);   //// Hmm, this delay seems to be critical, suggesting radio is slow or we do not have flow control setup correctly
	//		debug_print("Response:");
	int n = read(fd, response, rlen);
	if (n < 0) {
		debug_print ("error %d reading data\n", errno);
		close_serial(fd);
		return 1;
	}

	response[n] = 0; // terminate the string
	usleep(50*1000);

	close_serial(fd);
	usleep(50*1000);
	return n;
}


/**
 * Read a complete line from serial port
 * @param fd File descriptor of the serial port
 * @param buffer Buffer to store the line
 * @param buffer_size Size of the buffer
 * @param line_terminator Character that terminates a line (usually '\n' or '\r')
 * @return Number of characters read (excluding terminator), -1 on error, -2 on timeout
 */
int read_serial_line(char *serialdev, speed_t speed, char *buffer, size_t buffer_size, char line_terminator) {
    if (!buffer || buffer_size == 0) {
        errno = EINVAL;
        return -1;
    }

    int fd = open_serial(serialdev, speed);
    if (!fd) {
    	error_print("Error while initializing %s.\n", serialdev);
    	return -1;
    }
    tcflush(fd,TCIOFLUSH );

    size_t pos = 0;
    char ch;
    ssize_t bytes_read;

    // Clear the buffer
    memset(buffer, 0, buffer_size);

    while (pos < buffer_size - 1) { // Leave space for null terminator
        bytes_read = read(fd, &ch, 1);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                // Timeout occurred
//            	close_serial(fd);
//                return -2;
            	usleep(10);
            	continue;
            } else {
                // Read error
                perror("read");
                close_serial(fd);
                return -1;
            }
        } else if (bytes_read == 0) {
            // Timeout (no data available) - or end of file?
        	close_serial(fd);
            return -3;
        }

        // Check for line terminator
        if (ch == line_terminator) {
            buffer[pos] = '\0'; // Null-terminate the string
            close_serial(fd);
            return (int)pos; // Return number of characters read
        }

        // Add character to buffer
        buffer[pos++] = ch;
    }

    // Buffer full without finding terminator
    buffer[buffer_size - 1] = '\0';
    errno = ENOBUFS;
    close_serial(fd);
    return -4;
}



//int serial_read_data(char *serialdev, speed_t speed, unsigned char *response, int rlen) {
//	int fd = open_serial(serialdev, speed);
//	if (!fd) {
//		error_print("Error while initializing %s.\n", serialdev);
//		return -1;
//	}
//	tcflush(fd,TCIOFLUSH );
//
//	int n = read(fd, response, rlen);
//	if (n < 0) {
//		debug_print ("Error: %s, reading data\n", strerror(errno));
//		close_serial(fd);
//		return 1;
//	}
//	response[n] = 0; // terminate the string
//	usleep(50*1000);
//	close_serial(fd);
//	usleep(50*1000);
//	return n;
//}


int open_serial(char *devicename, speed_t speed) {
	int fd;
	struct termios options;

	/* NOCTTY means we are not a terminal and dont want control codes.  NDELAY means we dont care about the state of the DCD line */
	if ((fd = open(devicename, O_RDWR | O_NOCTTY | O_NDELAY)) == -1) {
		error_print("open() %s\n",devicename);
		return 0;
	}

	if (tcgetattr(fd, &options) == -1) {
		error_print("open_serial(): tcgetattr() %s\n",devicename);
		return 0;
	}

	/*
	 * Set the baud rates to 9600, which is the default for the serial connection
	 */
	if (cfsetispeed(&options, speed) == -1) {
		error_print("open_serial(): cfsetispeed() %s\n",devicename);
		return 0;
	}
	if (cfsetospeed(&options, speed) == -1) {
		error_print("open_serial(): cfsetospeed() %s\n",devicename);
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
		error_print("open_serial: tcsetattr() %s\n",devicename);
		close_serial(fd);
	}
	return fd;
}

void close_serial(int fd) {
	if (close(fd) < 0)
		error_print("closeserial()\n");
}

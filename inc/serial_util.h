/*
 * serial_util.h
 *
 *  Created on: Jun 9, 2025
 *      Author: g0kla
 */

#ifndef SERIAL_UTIL_H_
#define SERIAL_UTIL_H_

int serial_send_cmd(char *serialdev, speed_t speed, char * data, int len, unsigned char *response, int rlen);
int serial_read_data(char *serialdev, speed_t speed, unsigned char *response, int rlen);
int open_serial(char *devicename, speed_t speed);
void close_serial(int fd);

#endif /* SERIAL_UTIL_H_ */

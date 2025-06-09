/*
 * serial_util.h
 *
 *  Created on: Jun 9, 2025
 *      Author: g0kla
 */

#ifndef SERIAL_UTIL_H_
#define SERIAL_UTIL_H_

int serial_send_cmd(char *serialdev, char * data, int len, unsigned char *response, int rlen);

#endif /* SERIAL_UTIL_H_ */

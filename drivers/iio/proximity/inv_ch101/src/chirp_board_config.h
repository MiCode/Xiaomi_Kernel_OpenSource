/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chirp_board_config.h
 *
 * This file defines required symbols used to build an application with the
 * Chirp SonicLib API and driver.  These symbols are used for static array
 * allocations and counters in SonicLib (and often applications), and are based
 * on the number of specific resources on the target board.
 *
 * Two symbols must be defined:
 *  CHIRP_MAX_NUM_SENSORS -	the number of possible sensor devices
 *				(i.e. the number of sensor ports)
 *  CHIRP_NUM_I2C_BUSES -	the number of I2C buses on the board that are
 *				used for those sensor ports
 *
 * This file must be in the C pre-processor include path when the application
 * is built with SonicLib and this board support package.
 */

#ifndef CHIRP_BOARD_CONFIG_H_
#define CHIRP_BOARD_CONFIG_H_

/* Settings for the Chirp RB5 board */
/* maximum possible number of sensor devices */
#define CHIRP_MAX_NUM_SENSORS		6
/* number of I2C buses used by sensors */
#define CHIRP_NUM_I2C_BUSES		2

#define CH101_MAX_PORTS			CHIRP_MAX_NUM_SENSORS
#define CH101_I2C_BUSES			CHIRP_NUM_I2C_BUSES
#define CH101_PORTS_PER_BUSES		3
#define CH101_MAX_I2C_QUEUE_LENGTH	12

#endif /* CHIRP_BOARD_CONFIG_H_ */

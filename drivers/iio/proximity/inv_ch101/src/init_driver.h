/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef INITDRIVER_H_
#define INITDRIVER_H_

#include "soniclib.h"
#include "../ch101_client.h"

#define READ_IQ_DATA_BLOCKING
#define IQ_DATA_MAX_NUM_SAMPLES  CH101_MAX_NUM_SAMPLES	// use CH201 I/Q size

/* Bit flags used in main loop to check for completion of sensor I/O.  */
#define DATA_READY_FLAG			(1 << 0)
#define IQ_READY_FLAG			(1 << 1)

#define MAX_RX_SAMPLES			450
#define MAX_NB_SAMPLES			450

/* Define configuration settings for the Chirp sensors.
 * The following symbols define configuration values that are used to
 * initialize the ch_config_t structure passed during the ch_set_config() call.
 */

/* maximum range, in mm */
#define	CHIRP_SENSOR_MAX_RANGE_MM	1000
/* static target rejection sample range, in samples (0=disabled) */
#define	CHIRP_SENSOR_STATIC_RANGE	0
/* internal sample interval -NOT USED IF TRIGGERED */
#define CHIRP_SENSOR_SAMPLE_INTERVAL	0

struct chirp_data_t {
	// from ch_get_range()
	u32 range;
	// from ch_get_amplitude()
	u16 amplitude;
	// from ch_get_num_samples()
	u16 num_samples;
	// from ch_get_iq_data()
	struct ch_iq_sample_t iq_data[IQ_DATA_MAX_NUM_SAMPLES];
};

void set_chirp_data(struct ch101_client *data);
struct ch101_client *get_chirp_data(void);
void set_chirp_buffer(struct ch101_buffer *buffer);
int  find_sensors(void);
void init_driver(void);
void config_driver(void);
void start_driver(int period_ms, int time_ms);
void stop_driver(void);
void ext_ChirpINT0_handler(int index);
void single_shot_driver(void);
void test_detect(void);
void test_write_read(void);



#endif /* INITDRIVER_H_ */

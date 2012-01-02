/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __USF_H__
#define __USF_H__

#include <linux/types.h>
#include <linux/ioctl.h>

#define USF_IOCTL_MAGIC 'U'

#define US_SET_TX_INFO   _IOW(USF_IOCTL_MAGIC, 0, \
				struct us_tx_info_type)
#define US_START_TX      _IO(USF_IOCTL_MAGIC, 1)
#define US_GET_TX_UPDATE _IOWR(USF_IOCTL_MAGIC, 2, \
				struct us_tx_update_info_type)
#define US_SET_RX_INFO   _IOW(USF_IOCTL_MAGIC, 3, \
				struct us_rx_info_type)
#define US_SET_RX_UPDATE _IOWR(USF_IOCTL_MAGIC, 4, \
				struct us_rx_update_info_type)
#define US_START_RX      _IO(USF_IOCTL_MAGIC, 5)

#define US_STOP_TX      _IO(USF_IOCTL_MAGIC, 6)
#define US_STOP_RX      _IO(USF_IOCTL_MAGIC, 7)

#define US_SET_DETECTION _IOWR(USF_IOCTL_MAGIC, 8, \
				struct us_detect_info_type)

/* Special timeout values */
#define USF_NO_WAIT_TIMEOUT	0x00000000
/* Infinitive */
#define USF_INFINITIVE_TIMEOUT	0xffffffff
/* Default value, used by the driver */
#define USF_DEFAULT_TIMEOUT	0xfffffffe

/* US detection place (HW|FW) */
enum us_detect_place_enum {
/* US is detected in HW */
	US_DETECT_HW,
/* US is detected in FW */
	US_DETECT_FW
};

/* US detection mode */
enum us_detect_mode_enum {
/* US detection is disabled */
	US_DETECT_DISABLED_MODE,
/* US detection is enabled in continue mode */
	US_DETECT_CONTINUE_MODE,
/* US detection is enabled in one shot mode */
	US_DETECT_SHOT_MODE
};

/* Encoder (TX), decoder (RX) supported US data formats */
#define USF_POINT_EPOS_FORMAT	0
#define USF_RAW_FORMAT		1

/* Types of events, produced by the calculators */
#define USF_NO_EVENT 0
#define USF_TSC_EVENT 1
#define USF_MOUSE_EVENT 2
#define USF_KEYBOARD_EVENT 4
#define USF_ALL_EVENTS (USF_TSC_EVENT | USF_MOUSE_EVENT | USF_KEYBOARD_EVENT)

/* Max size of the client name */
#define USF_MAX_CLIENT_NAME_SIZE	20
/* Info structure common for TX and RX */
struct us_xx_info_type {
/* Input:  general info */
/* Name of the client - event calculator */
	const char *client_name;
/* Selected device identification, accepted in the kernel's CAD */
	uint32_t dev_id;
/* 0 - point_epos type; (e.g. 1 - gr_mmrd) */
	uint32_t stream_format;
/* Required sample rate in Hz */
	uint32_t sample_rate;
/* Size of a buffer (bytes) for US data transfer between the module and USF */
	uint32_t buf_size;
/* Number of the buffers for the US data transfer */
	uint16_t buf_num;
/* Number of the microphones (TX) or speakers(RX) */
	uint16_t port_cnt;
/* Microphones(TX) or speakers(RX) indexes in their enumeration */
	uint8_t  port_id[4];
/* Bits per sample 16 or 32 */
	uint16_t bits_per_sample;
/* Input:  Transparent info for encoder in the LPASS */
/* Parameters data size in bytes */
	uint16_t params_data_size;
/* Pointer to the parameters */
	uint8_t *params_data;
};

struct us_input_info_type {
	/* Touch screen dimensions: min & max;for input module */
	int tsc_x_dim[2];
	int tsc_y_dim[2];
    /* Touch screen fuzz; for input module */
	int tsc_x_fuzz;
	int tsc_y_fuzz;
	/* Touch screen pressure limits: min & max; for input module */
	int tsc_pressure[2];
	/* Bitmap of types of events (USF_X_EVENT), produced by calculator */
	uint16_t event_types;
};

struct us_tx_info_type {
	/* Common info */
	struct us_xx_info_type us_xx_info;
	/* Info specific for TX*/
	struct us_input_info_type input_info;
};

struct us_rx_info_type {
	/* Common info */
	struct us_xx_info_type us_xx_info;
	/* Info specific for RX*/
};


#define	USF_PIX_COORDINATE  0 /* unit is pixel */
#define	USF_CMM_COORDINATE  1 /* unit is 0.01 mm */
struct point_event_type {
/* Pen coordinates (x, y) in units, defined by <coordinates_type>  */
	int coordinates[2];
/* {x;y}  in degrees [-90; 90] */
	uint32_t inclinations[2];
/* [0-1023] (10bits); 0 - pen up */
	uint32_t pressure;
/* 0 - mapped in the display pixel. 1 - raw in 0.01 mm (only for log); */
	uint8_t coordinates_type;
};

/* Mouse buttons, supported by USF */
#define USF_BUTTON_LEFT_MASK   1
#define USF_BUTTON_MIDDLE_MASK 2
#define USF_BUTTON_RIGHT_MASK  4
struct mouse_event_type {
/* The mouse relative movement (dX, dY, dZ) */
	int rels[3];
/* Bitmap of mouse buttons states: 1 - down, 0 - up; */
	uint16_t buttons_states;
};

struct key_event_type {
/*  Calculated MS key- see input.h. */
	uint32_t key;
/* Keyboard's key state: 1 - down, 0 - up; */
	uint8_t key_state;
};

struct usf_event_type {
/* Event sequence number */
	uint32_t seq_num;
/* Event generation system time */
	uint32_t timestamp;
/* Destination input event type (e.g. touch screen, mouse, key) */
	uint16_t event_type;
	union {
		struct point_event_type point_event;
		struct mouse_event_type mouse_event;
		struct key_event_type   key_event;
	} event_data;
};

struct us_tx_update_info_type {
/* Input  general: */
/* Number of calculated events */
	uint16_t event_counter;
/* Calculated events or NULL */
	struct usf_event_type *event;
/* Pointer (read index) to the end of available region */
/* in the shared US data memory */
	uint32_t free_region;
/* Time (sec) to wait for data or special values: */
/* USF_NO_WAIT_TIMEOUT, USF_INFINITIVE_TIMEOUT, USF_DEFAULT_TIMEOUT */
	uint32_t timeout;

/* Input  transparent data: */
/* Parameters size */
	uint16_t params_data_size;
/* Pointer to the parameters */
	uint8_t *params_data;
/* Output parameters: */
/* Pointer (write index) to the end of ready US data region */
/* in the shared memory */
	uint32_t ready_region;
};

struct us_rx_update_info_type {
/* Input  general: */
/* Pointer (write index) to the end of ready US data region */
/* in the shared memory */
	uint32_t ready_region;
/* Input  transparent data: */
/* Parameters size */
	uint16_t params_data_size;
/* pPointer to the parameters */
	uint8_t *params_data;
/* Output parameters: */
/* Pointer (read index) to the end of available region */
/* in the shared US data memory */
	uint32_t free_region;
};

struct us_detect_info_type {
/* US detection place (HW|FW) */
/* NA in the Active and OFF states */
	enum us_detect_place_enum us_detector;
/* US detection mode */
	enum us_detect_mode_enum  us_detect_mode;
/* US data dropped during this time (msec) */
	uint32_t skip_time;
/* Transparent data size */
	uint16_t params_data_size;
/* Pointer to the transparent data */
	uint8_t *params_data;
/* Time (sec) to wait for US presence event */
	uint32_t detect_timeout;
/* Out parameter: US presence */
	bool is_us;
};

#endif /* __USF_H__ */

/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef BGRSB_H
#define BGRSB_H

struct event {
	uint8_t sub_id;
	int16_t evnt_data;
	uint32_t evnt_tm;
};


struct bg_glink_chnl {
	char *chnl_name;
	char *chnl_edge;
	char *chnl_trnsprt;
};

/**
 * bgrsb_send_input() - send the recived input to input framework
 * @evnt: pointer to the event structure
 */
int bgrsb_send_input(struct event *evnt);

#endif /* BGCOM_H */

/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _A2_SERVICE_H_
#define _A2_SERVICE_H_

int a2_mux_initialize(void);

int a2_mux_close(void);

int a2_mux_open_port(int wwan_logical_channel_id, void *rx_cb,
		void *tx_complete_cb);

#endif /* _A2_SERVICE_H_ */


/* Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
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

#ifndef __U_RMNET_H
#define __U_RMNET_H

#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

struct rmnet_ctrl_pkt {
	void	*buf;
	int	len;
	struct list_head	list;
};

enum qti_port_type {
	QTI_PORT_RMNET,
	QTI_PORT_DPL,
	QTI_NUM_PORTS
};


struct grmnet {
	/* to usb host, aka laptop, windows pc etc. Will
	 * be filled by usb driver of rmnet functionality
	 */
	int (*send_cpkt_response)(void *g, void *buf, size_t len);

	/* to modem, and to be filled by driver implementing
	 * control function
	 */
	int (*send_encap_cmd)(enum qti_port_type qport, void *buf, size_t len);
	void (*notify_modem)(void *g, enum qti_port_type qport, int cbits);

	void (*disconnect)(struct grmnet *g);
	void (*connect)(struct grmnet *g);
};

enum ctrl_client {
	FRMNET_CTRL_CLIENT,
	GPS_CTRL_CLIENT,

	NR_CTRL_CLIENTS
};

int gqti_ctrl_connect(void *gr, enum qti_port_type qport, unsigned intf);
void gqti_ctrl_disconnect(void *gr, enum qti_port_type qport);
int gqti_ctrl_init(void);
void gqti_ctrl_cleanup(void);
#endif /* __U_RMNET_H*/

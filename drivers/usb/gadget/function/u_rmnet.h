/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
	void			*buf;
	int			len;
	struct list_head	list;
};

struct grmnet {
	struct usb_function		func;

	struct usb_gadget		*gadget;
	struct usb_ep			*in;
	struct usb_ep			*out;

	/* to usb host, aka laptop, windows pc etc. Will
	 * be filled by usb driver of rmnet functionality
	 */
	int (*send_cpkt_response)(void *g, void *buf, size_t len);

	/* to modem, and to be filled by driver implementing
	 * control function
	 */
	int (*send_encap_cmd)(u8 port_num, void *buf, size_t len);

	void (*notify_modem)(void *g, u8 port_num, int cbits);

	void (*disconnect)(struct grmnet *g);
	void (*connect)(struct grmnet *g);
};

#define NR_QTI_PORTS	(NR_RMNET_PORTS + NR_DPL_PORTS)
#define NR_RMNET_PORTS	4
#define NR_DPL_PORTS	1

enum ctrl_client {
	FRMNET_CTRL_CLIENT,
	GPS_CTRL_CLIENT,

	NR_CTRL_CLIENTS
};

int gbam_setup(unsigned int no_bam_port, unsigned int no_bam2bam_port);
void gbam_cleanup(void);
int gbam_connect(struct grmnet *gr, u8 port_num,
	enum transport_type trans, u8 src_connection_idx,
	u8 dst_connection_idx);
void gbam_disconnect(struct grmnet *gr, u8 port_num,
	enum transport_type trans);
void gbam_suspend(struct grmnet *gr, u8 port_num, enum transport_type trans);
void gbam_resume(struct grmnet *gr, u8 port_num, enum transport_type trans);
int gsmd_ctrl_connect(struct grmnet *gr, int port_num);
void gsmd_ctrl_disconnect(struct grmnet *gr, u8 port_num);
int gsmd_ctrl_setup(enum ctrl_client client_num, unsigned int count,
					u8 *first_port_idx);
#endif /* __U_RMNET_H*/

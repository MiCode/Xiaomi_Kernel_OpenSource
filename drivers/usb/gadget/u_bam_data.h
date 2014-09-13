/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef __U_BAM_DATA_H
#define __U_BAM_DATA_H

#include "usb_gadget_xport.h"

enum function_type {
	USB_FUNC_ECM,
	USB_FUNC_MBIM,
	USB_FUNC_RNDIS,
};

struct data_port {
	struct usb_composite_dev	*cdev;
	struct usb_function		*func;
	struct usb_ep			*in;
	int				rx_buffer_size;
	struct usb_ep			*out;
	int                             ipa_consumer_ep;
	int                             ipa_producer_ep;
};

int bam2bam_data_port_select(int portno);

void bam_data_disconnect(struct data_port *gr, u8 port_num);

int bam_data_connect(struct data_port *gr, u8 port_num,
	enum transport_type trans, u8 src_connection_idx,
	u8 dst_connection_idx, enum function_type func);

int bam_data_setup(unsigned int no_bam2bam_port);

void bam_data_flush_workqueue(void);

void bam_data_suspend(u8 port_num);

void bam_data_resume(u8 port_num);

void u_bam_data_set_dl_max_xfer_size(u32 dl_max_transfer_size);

void u_bam_data_set_ul_max_pkt_num(u8 ul_max_packets_number);

void u_bam_data_set_ul_max_xfer_size(u32 ul_max_xfer_size);

void u_bam_data_start_rndis_ipa(void);

void u_bam_data_stop_rndis_ipa(void);

void bam_data_start_rx_tx(u8 port_num);

#endif /* __U_BAM_DATA_H */

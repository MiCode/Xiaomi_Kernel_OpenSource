/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
	USB_NUM_FUNCS,
};

#define PORTS_PER_FUNC 1
#define BAM2BAM_DATA_N_PORTS (USB_NUM_FUNCS * PORTS_PER_FUNC)

struct data_port {
	struct usb_composite_dev		*cdev;
	struct usb_function			*func;
	struct usb_ep				*in;
	int					rx_buffer_size;
	struct usb_ep				*out;
	int					ipa_consumer_ep;
	int					ipa_producer_ep;
	const struct usb_endpoint_descriptor	*in_ep_desc_backup;
	const struct usb_endpoint_descriptor	*out_ep_desc_backup;
};

void bam_data_disconnect(struct data_port *gr, enum function_type func,
		u8 dev_port_num);

int bam_data_connect(struct data_port *gr, enum transport_type trans,
		u8 dev_port_num, enum function_type func);

int bam_data_setup(enum function_type func, unsigned int no_bam2bam_port);

void bam_data_flush_workqueue(void);

void bam_data_suspend(struct data_port *port_usb, u8 dev_port_num,
		enum function_type func, bool remote_wakeup_enabled);

void bam_data_resume(struct data_port *port_usb, u8 dev_port_num,
		enum function_type func, bool remote_wakeup_enabled);

void u_bam_data_set_dl_max_xfer_size(u32 dl_max_transfer_size);

void u_bam_data_set_ul_max_pkt_num(u8 ul_max_packets_number);

void u_bam_data_set_ul_max_xfer_size(u32 ul_max_xfer_size);

void u_bam_data_start_rndis_ipa(void);

void u_bam_data_stop_rndis_ipa(void);

void bam_data_start_rx_tx(u8 port_num);

int u_bam_data_func_to_port(enum function_type func, u8 func_port);

#endif /* __U_BAM_DATA_H */

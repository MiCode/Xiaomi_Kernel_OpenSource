/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __U_DATA_IPA_H
#define __U_DATA_IPA_H

#include "usb_gadget_xport.h"

struct gadget_ipa_port {
	struct usb_composite_dev	*cdev;
	struct usb_function		*func;
	struct usb_ep			*in;
	struct usb_ep			*out;
	int				ipa_consumer_ep;
	int				ipa_producer_ep;
};

void ipa_data_port_select(int portno, enum gadget_type gtype);
void ipa_data_disconnect(struct gadget_ipa_port *gp, u8 port_num);
int ipa_data_connect(struct gadget_ipa_port *gp, u8 port_num,
			u8 src_connection_idx, u8 dst_connection_idx);
int ipa_data_setup(unsigned int no_ipa_port);
void ipa_data_resume(struct gadget_ipa_port *gp, u8 port_num);
void ipa_data_suspend(struct gadget_ipa_port *gp, u8 port_num);

#endif

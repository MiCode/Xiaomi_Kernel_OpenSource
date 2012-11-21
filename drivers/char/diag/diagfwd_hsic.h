/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef DIAGFWD_HSIC_H
#define DIAGFWD_HSIC_H

#include <mach/diag_bridge.h>

#define N_MDM_WRITE	8
#define N_MDM_READ	1
#define NUM_HSIC_BUF_TBL_ENTRIES N_MDM_WRITE

int diagfwd_write_complete_hsic(struct diag_request *);
int diagfwd_cancel_hsic(void);
void diag_read_usb_hsic_work_fn(struct work_struct *work);
void diag_usb_read_complete_hsic_fn(struct work_struct *w);
extern struct diag_bridge_ops hsic_diag_bridge_ops;
extern struct platform_driver msm_hsic_ch_driver;
void diag_hsic_close(void);

#endif

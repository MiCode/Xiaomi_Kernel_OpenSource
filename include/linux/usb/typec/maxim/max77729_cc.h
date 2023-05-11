/*
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_MFD_MAX77729_CC_H
#define __LINUX_MFD_MAX77729_CC_H
#define MAX77729_CC_NAME	"max77729_CC"


struct max77729_cc_data {

	/* interrupt pin */
	int irq_vconncop;
	int irq_vsafe0v;
	int irq_detabrt;
	int irq_vconnsc;
	int irq_ccpinstat;
	int irq_ccistat;
	int irq_ccvcnstat;
	int irq_ccstat;

	u8 usbc_status1;
	u8 usbc_status2;
	u8 bc_status;
	u8 cc_status0;
	u8 cc_status1;
	u8 pd_status0;
	u8 pd_status1;

	u8 opcode_res;

	/* VCONN Over Current Detection */
	u8 vconnocp;
	/* VCONN Over Short Circuit Detection */
	u8 vconnsc;
	/* Status of VBUS Detection */
	u8 vsafe0v;
	/* Charger Detection Abort Status */
	u8 detabrt;
	/* Output of active CC pin */
	u8 ccpinstat;
	/* CC Pin Detected Allowed VBUS Current in UFP mode */
	u8 ccistat;
	/* Status of Vconn Output */
	u8 ccvcnstat;
	/* CC Pin State Machine Detection */
	u8 ccstat;

	enum max77729_vcon_role	current_vcon;
	enum max77729_vcon_role	previous_vcon;
	enum max77729_power_role current_pr;
	enum max77729_power_role previous_pr;
};
#endif

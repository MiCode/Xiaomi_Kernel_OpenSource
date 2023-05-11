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

#ifndef __LINUX_MFD_MAX77729_PD_H
#define __LINUX_MFD_MAX77729_PD_H
#include "max77729.h"

#define MAX77729_PD_NAME	"MAX77729_PD"

enum {
	CC_SNK = 0,
	CC_SRC,
	CC_NO_CONN,
};

typedef enum {
	PDO_TYPE_FIXED = 0,
	PDO_TYPE_BATTERY,
	PDO_TYPE_VARIABLE,
	PDO_TYPE_APDO
} pdo_supply_type_t;

typedef enum
{
	RP_CURRENT_LEVEL_NONE = 0,
	RP_CURRENT_LEVEL_DEFAULT,
	RP_CURRENT_LEVEL2,
	RP_CURRENT_LEVEL3,
	RP_CURRENT_ABNORMAL,
} RP_CURRENT_LEVE;


typedef enum {
	PDIC_NOTIFY_EVENT_DETACH = 0,
	PDIC_NOTIFY_EVENT_PDIC_ATTACH,
	PDIC_NOTIFY_EVENT_PD_SINK,
	PDIC_NOTIFY_EVENT_PD_SOURCE,
	PDIC_NOTIFY_EVENT_PD_SINK_CAP,
	PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC,
} pdic_notifier_event_t;


typedef struct _power_list {
	int accept;
	int max_voltage;
	int min_voltage;
	int max_current;
	int apdo;
	int comm_capable;
	int suspend;
 } POWER_LIST;

typedef struct sec_pd_sink_status
{
	POWER_LIST power_list[9];
 	int has_apdo; // pd source has apdo or not
	int available_pdo_num; // the number of available PDO
	int selected_pdo_num; // selected number of PDO to change
	int current_pdo_num; // current number of PDO
	unsigned short vid;
	unsigned short pid;
	unsigned int xid;

	int pps_voltage;
	int pps_current;

	unsigned int rp_currentlvl; // rp current level by ccic

	void (*fp_sec_pd_select_pdo)(int num);
	int (*fp_sec_pd_select_pps)(int num, int ppsVol, int ppsCur);
	void (*fp_sec_pd_ext_cb)(unsigned short v_id, unsigned short p_id);
} SEC_PD_SINK_STATUS;

struct pdic_notifier_struct {
	pdic_notifier_event_t event;
	SEC_PD_SINK_STATUS sink_status;
	void *pusbpd;
};


typedef union sec_pdo_object {
	uint32_t		data;
	struct {
		uint8_t		bdata[4];
	} BYTES;
	struct {
		uint32_t	reserved:30,
				type:2;
	} BITS_supply;
	struct {
		uint32_t	max_current:10,        /* 10mA units */
				voltage:10,            /* 50mV units */
				peak_current:2,
				reserved:2,
				unchuncked_extended_messages_supported:1,
				data_role_data:1,
				usb_communications_capable:1,
				unconstrained_power:1,
				usb_suspend_supported:1,
				dual_role_power:1,
				supply:2;			/* Fixed supply : 00b */
	} BITS_pdo_fixed;
	struct {
		uint32_t	max_current:10,		/* 10mA units */
				min_voltage:10,		/* 50mV units */
				max_voltage:10,		/* 50mV units */
				supply:2;		/* Variable Supply (non-Battery) : 10b */
	} BITS_pdo_variable;
	struct {
		uint32_t	max_allowable_power:10,		/* 250mW units */
				min_voltage:10,		/* 50mV units  */
				max_voltage:10,		/* 50mV units  */
				supply:2;		/* Battery : 01b */
	} BITS_pdo_battery;
	struct {
		uint32_t	max_current:7, 	/* 50mA units */
				reserved1:1,
				min_voltage:8, 	/* 100mV units	*/
				reserved2:1,
				max_voltage:8, 	/* 100mV units	*/
				reserved3:2,
				pps_power_limited:1,
				pps_supply:2,
				supply:2;		/* APDO : 11b */
	} BITS_pdo_programmable;
} U_SEC_PDO_OBJECT;

struct max77729_pd_data {
	/* interrupt pin */
	int irq_pdmsg;
	int irq_psrdy;
	int irq_datarole;
	int irq_ssacc;
	int irq_fct_id;

	u8 usbc_status1;
	u8 usbc_status2;
	u8 bc_status;
	u8 cc_status0;
	u8 cc_status1;
	u8 pd_status0;
	u8 pd_status1;
	unsigned int rp_currentlvl;

	u8 opcode_res;

	/* PD Message */
	u8 pdsmg;

	/* Data Role */
	enum max77729_data_role current_dr;
	enum max77729_data_role previous_dr;
	/* SSacc */
	u8 ssacc;
	/* FCT cable */
	u8 fct_id;
	enum max77729_ccpd_device device;

	struct pdic_notifier_struct pd_noti;
	bool pdo_list;
	bool psrdy_received;
	bool cc_sbu_short;
	bool bPPS_on;
	bool sent_chg_info;

	struct workqueue_struct *wqueue;
	struct delayed_work retry_work;

	int cc_status;
};

#endif

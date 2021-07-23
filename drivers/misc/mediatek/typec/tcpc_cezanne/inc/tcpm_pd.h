/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TCPM_PD_H_
#define TCPM_PD_H_

#include "tcpci_config.h"

/* --- PD data message helpers --- */

#define PD_DATA_OBJ_SIZE		(7)
#define PDO_MAX_NR				(PD_DATA_OBJ_SIZE)
#define VDO_MAX_NR				(PD_DATA_OBJ_SIZE-1)
#define VDO_MAX_SVID_NR			(VDO_MAX_NR*2)

#define VDO_DISCOVER_ID_IDH			0
#define VDO_DISCOVER_ID_CSTAT		1
#define VDO_DISCOVER_ID_PRODUCT		2
#define VDO_DISCOVER_ID_CABLE		3
#define VDO_DISCOVER_ID_AMA			3

/******************* PD30 *******************/

#ifdef CONFIG_USB_PD_REV30_CHUNKING_BY_PE
#define MAX_EXTENDED_MSG_LEN	260
#else
#define MAX_EXTENDED_MSG_LEN	26
#endif	/* CONFIG_USB_PD_REV30_CHUNKING */

/* PD30 Data Message Data Object */

#define PD_BSDO_SIZE	1
#define PD_CCDO_SIZE	1
#define PD_ADO_SIZE		1

/*
 * Battery Status Data Object (BSDO)
 * ----------
 * <31:16>  :: Battery Present Capacity (1/10 WH, 0xFFFF = SOC unknown)
 * <15:8>     :: Battery Info
 * <7:0>    :: Reserved and Shall be set to zero
 */

#define BSDO(cap, info)	(((cap) << 16) | ((info) << 8))

#define BSDO_BAT_INFO(x)		((x>>8) & 0xff)

#define BSDO_BAT_INFO_INVALID_REF	(1<<0)
#define BSDO_BAT_INFO_PRESENT		(1<<1)
#define BSDO_BAT_INFO_CHARGING	((0<<2) | BSDO_BAT_INFO_PRESENT)
#define BSDO_BAT_INFO_DISCHARGING	((1<<2) | BSDO_BAT_INFO_PRESENT)
#define BSDO_BAT_INFO_IDLE			((2<<2) | BSDO_BAT_INFO_PRESENT)

#define BSDO_BAT_CAP_UNKNOWN		(0xffff)

/*
 * Country Code Data Object (CCDO)
 * ----------
 * <31:24>  :: First character of the Alpha-2 Country Code
 * <23:16>  :: Second character of the Alpha-2 Country Code
 * <15:0>    :: Reserved and Shall be set to zero
 */

#define CCDO(code)	((code) << 16)

#define CCDO_COUNTRY_CODE(x)	((x >> 16) & 0xffff)
#define CCDO_COUNTRY_CODE1(x)	((x >> 24) & 0xff)
#define CCDO_COUNTRY_CODE2(x)	((x >> 16) & 0xff)

/*
 * Alert Data Object (ADO)
 * ----------
 * <31:24>  :: Type Of Alert
 * <23:20>  :: Fixed Batteries (bit field)
 * <19:16>  :: Hot Swappable Batteries (bit field)
 */

#define ADO_ALERT_BAT_CHANGED	(1<<1)
#define ADO_ALERT_OCP	(1<<2)
#define ADO_ALERT_OTP	(1<<3)
#define ADO_ALERT_OPER_CHANGED	(1<<4)
#define ADO_ALERT_SRC_IN_CHANGED	(1<<5)
#define ADO_ALERT_OVP	(1<<6)

#define ADO_ALERT_TYPE(raw)	(raw >> 24)
#define ADO_FIXED_BAT(raw)	((raw >> 20) & 0x0f)
#define ADO_HOT_SWAP_BAT(raw)	((raw >> 16) & 0x0f)

#define ADO_ALERT_TYPE_SET(type)	(type << 24)
#define ADO_FIXED_BAT_SET(i)	((i) << 20)
#define ADO_HOT_SWAP_BAT_SET(i)	((i) << 16)

#define ADO(type, fixed, swap)	\
	(((type) << 24) | ((fixed) << 20) | ((swap) << 16))

#define ADO_GET_STATUS_ONCE_MASK    ADO(\
		ADO_ALERT_BAT_CHANGED|ADO_ALERT_SRC_IN_CHANGED,\
		0xff, 0xff)

/* PD30 Extend Message Data Object */

enum pd_present_temperature_flag {
	PD_PTF_NO_SUPPORT = 0,
	PD_PTF_NORMAL,
	PD_PTF_WARNING,
	PD_PTF_OVER_TEMP,
};

enum pd_battery_reference {
	PD_BAT_REF_FIXED0	= 0,
	PD_BAT_REF_FIXED1,
	PD_BAT_REF_FIXED2,
	PD_BAT_REF_FIXED3,

	PD_BAT_REF_SWAP0	= 4,
	PD_BAT_REF_SWAP1,
	PD_BAT_REF_SWAP2,
	PD_BAT_REF_SWAP3,

	PD_BAT_REF_MAX,

	/* 8 ~ 255 are reserved and shall not be used */
};


/* SCEDB, Source_Capabilities_Extended */

#define PD_SCEDB_SIZE	24

#define PD_SCEDB_VR(load_step, ioc)	\
	((load_step) | (ioc << 2))

enum {
	PD_SCEDB_VR_LOAD_STEP_150 = 0x00,
	PD_SCEDB_VR_LOAD_STEP_500 = 0x01,
};

enum {
	PD_SCEDB_VR_IOC_25 = 0x00,
	PD_SCEDB_VR_IOC_90 = 0x01,
};

#define PD_SCEDB_COMPLIANCE_LPS	(1<<0)
#define PD_SCEDB_COMPLIANCE_PS1	(1<<1)
#define PD_SCEDB_COMPLIANCE_PS2	(1<<2)

#define PD_SCEDB_TC_LOW_TC		(1<<0)
#define PD_SCEDB_TC_GROUND		(1<<1)
#define PD_SCEDB_TC_GROUND_INTEND	(1<<2)

#define PD_SCEDB_TT_IEC_60950		0
#define PD_SCEDB_TT_IEC_62368_TS1	1
#define PD_SCEDB_TT_IEC_62368_TS2	2

#define PD_SCEDB_INPUT_EXT		(1<<0)
#define PD_SCEDB_INPUT_EXT_UNCONSTRAINED	(1<<1)
#define PD_SCEDB_INPUT_INT		(1<<2)

#define PD_SCEDB_BATTERIES(swap_nr, fixed_nr)	\
	(swap_nr << 4 | fixed_nr)

#define PD_SCEDB_FIX_BAT_NR(raw)	(raw & 0xf)
#define PD_SCEDB_SWAP_BAT_NR(raw)	((raw >> 4) & 0xf)

struct pd_source_cap_ext {
	uint16_t	vid;
	uint16_t	pid;
	uint32_t	xid;
	uint8_t	fw_ver;
	uint8_t	hw_ver;
	uint8_t	voltage_regulation;
	uint8_t	hold_time_ms;
	uint8_t	compliance;	/* bit field */
	uint8_t	touch_current;	/* bit field */
	uint16_t	peak_current[3];
	uint8_t	touch_temp;
	uint8_t	source_inputs;	/* bit field */
	uint8_t	batteries;
	uint8_t	source_pdp;
};

/* GBSDB, Get_Battery_Status */

#define PD_GBSDB_SIZE	1

struct pd_get_battery_status {
	uint8_t	bat_status_ref;	/* pd_battery_reference */
};

/* GBCDB, Get_Battery_Cap */

#define PD_GBCDB_SIZE	1

struct pd_get_battery_capabilities {
	uint8_t	bat_cap_ref;	/* pd_battery_reference */
};

/* BCDB, Battery_Capabilities */

#define PD_BCDB_SIZE		9

#define PD_BCDB_BAT_CAP_NOT_PRESENT	0x0000
#define PD_BCDB_BAT_CAP_UNKNOWN		0Xffff
#define PD_BCDB_BAT_CAP_RAW(cap_wh)	(cap_wh*10)
#define PD_BCDB_BAT_CAP_VAL(raw)	(raw/10)

#define PD_BCDB_BAT_TYPE_INVALID		(1<<0)

struct pd_battery_capabilities {
	uint16_t	vid;
	uint16_t	pid;
	uint16_t	bat_design_cap;
	uint16_t	bat_last_full_cap;
	uint8_t	bat_type;
};

/* GMIDB, Get_Manufacturer_Info */

#define PD_GMIDB_SIZE	2

#define PD_GMIDB_TARGET_PORT		0
#define PD_GMIDB_TARGET_BATTRY		1

struct pd_get_manufacturer_info {
	uint8_t	info_target;
	uint8_t	info_ref;
};

/* MIDB, Manufacturer_Info */

#define PD_MIDB_MIN_SIZE	4
#define PD_MIDB_MAX_SIZE	26
#define PD_MIDB_DYNAMIC_SIZE	(PD_MIDB_MAX_SIZE-PD_MIDB_MIN_SIZE)

struct pd_manufacturer_info {
	uint16_t	vid;
	uint16_t	pid;
	uint8_t	mfrs_string[PD_MIDB_DYNAMIC_SIZE];
};

/* CCDB, Country_Codes */

#define PD_CCDB_MIN_SIZE	4
#define PD_CCDB_MAX_SIZE	MAX_EXTENDED_MSG_LEN
#define PD_CCDB_DYNAMIC_SIZE	(PD_CCDB_MAX_SIZE-PD_CCDB_MIN_SIZE)

struct pd_country_codes {
	uint16_t	length;
	uint16_t	country_code[1+PD_CCDB_DYNAMIC_SIZE/2];
};

/* CIDB, country_info */

#define PD_CIDB_MIN_SIZE	4
#define PD_CIDB_MAX_SIZE	MAX_EXTENDED_MSG_LEN

#define PD_CIDB_DYNAMIC_SIZE	(PD_CIDB_MAX_SIZE-PD_CIDB_MIN_SIZE)

struct pd_country_info {
	uint16_t	country_code;
	uint16_t	reserved;
	uint8_t country_special_data[PD_CIDB_DYNAMIC_SIZE];
};

/* SDB, Status */

#define PD_SDB_SIZE	5

#define PD_STATUS_INPUT_EXT_POWER	(1<<1)
#define PD_STATUS_INPUT_EXT_POWER_FROM_AC	(1<<2)
#define PD_STATUS_INPUT_INT_POWER_BAT		(1<<3)
#define PD_STATUS_INPUT_INT_POWER_NOT_BAT	(1<<4)

#define PD_STASUS_EVENT_OCP	(1<<1)
#define PD_STATUS_EVENT_OTP	(1<<2)
#define PD_STATUS_EVENT_OVP	(1<<3)
#define PD_STATUS_EVENT_CF_MODE	(1<<4)

#define PD_STASUS_EVENT_READ_CLEAR (\
	PD_STASUS_EVENT_OCP|PD_STATUS_EVENT_OTP|PD_STATUS_EVENT_OVP)

#define PD_STASUS_EVENT_MASK (\
	PD_STASUS_EVENT_OCP|PD_STATUS_EVENT_OTP|PD_STATUS_EVENT_OVP|\
	PD_STATUS_EVENT_CF_MODE)

#define PD_STATUS_TEMP_PTF(raw)	((raw & 0x06) >> 1)
#define PD_STATUS_TEMP_SET_PTF(val)		((val & 0x03) << 1)

struct pd_status {
	uint8_t internal_temp;	/* 0 means no support */
	uint8_t present_input;	/* bit filed */
	uint8_t present_battey_input; /* bit filed */
	uint8_t event_flags;	/* bit filed */
	uint8_t temp_status;	/* bit filed */
};

/* PPSSDB, PPSStatus */

#define PD_PPSSDB_SIZE	4	/* PPS_Status */

#define PD_PPS_GET_OUTPUT_MV(raw)		(raw*20)
#define PD_PPS_GET_OUTPUT_MA(raw)		(raw*50)

#define PD_PPS_SET_OUTPUT_MV(mv)		(((mv) / 20) & 0xFFFF)
#define PD_PPS_SET_OUTPUT_MA(ma)		(((ma) / 50) & 0xFF)

#define PD_PPS_FLAGS_CFF	(1 << 3)
#define PD_PPS_FLGAS_PTF(raw)	((raw & 0x06) >> 1)

#define PD_PPS_FLAGS_SET_PTF(flags)	((flags & 0x03) << 1)

struct pd_pps_status_raw {
	uint16_t output_vol_raw;	/* 0xffff means no support */
	uint8_t output_curr_raw;	/* 0xff means no support */
	uint8_t real_time_flags;
};

struct pd_pps_status {
	int output_mv;
	int output_ma;
	uint8_t real_time_flags;
};
#endif /* TCPM_PD_H_ */

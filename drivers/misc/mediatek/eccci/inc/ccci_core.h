/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __CCCI_CORE_H__
#define __CCCI_CORE_H__

#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <mt-plat/mtk_ccci_common.h>
#include "ccci_config.h"
#include "ccci_debug.h"
#include "ccci_bm.h"


#define CCCI_MAGIC_NUM 0xFFFFFFFF
/*
 * this is a trick for port->minor, which is configured in-sequence
 * by different type (char, net, ipc),
 * but when we use it in code, we need it's unique among
 * all ports for addressing.
 */
#define CCCI_IPC_MINOR_BASE 100
#define CCCI_SMEM_MINOR_BASE 150
#define CCCI_NET_MINOR_BASE 200


/* ============================================================= */
/* common structures */
/* ============================================================= */
enum DIRECTION {
	IN = 0,
	OUT,
};

struct ccci_md_attribute {
	struct attribute attr;
	struct ccci_modem *modem;
	 ssize_t (*show)(struct ccci_modem *md, char *buf);
	 ssize_t (*store)(struct ccci_modem *md, const char *buf, size_t count);
};

#define CCCI_MD_ATTR(_modem, _name, _mode, _show, _store)	\
static struct ccci_md_attribute ccci_md_attr_##_name = {	\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.modem = _modem,					\
	.show = _show,						\
	.store = _store,					\
}

/*
 * do not modify this c2k structure, because we assume its total size is 32bit,
 * and used as ccci_header's 'reserved' member
 */
struct c2k_ctrl_port_msg {
	unsigned char id_hi;
	unsigned char id_low;
	unsigned char chan_num;
	unsigned char option;
} __packed;

struct ccci_ccb_config {
	unsigned int user_id;
	unsigned char core_id;
	unsigned int dl_page_size;
	unsigned int ul_page_size;
	unsigned int dl_buff_size;
	unsigned int ul_buff_size;
};
struct ccci_ccb_debug {
	unsigned int buffer_id;
	unsigned int page_id;
	unsigned int value;
};

struct ccb_ctrl_info {
	unsigned int  user_id;
	unsigned int ctrl_offset;
	unsigned int ctrl_addr;	/*phy addr*/
	unsigned int ctrl_length;
};

extern unsigned int ccb_configs_len;
extern struct ccci_ccb_config ccb_configs[];
extern void mtk_ccci_ccb_info_peek(void);


/* ======================================================================= */
/* IOCTL definations */
/* ======================================================================= */
#define CCCI_IOC_MAGIC 'C'
/* mdlogger, META, muxreport */
#define CCCI_IOC_MD_RESET			_IO(CCCI_IOC_MAGIC, 0)
/* audio */
#define CCCI_IOC_GET_MD_STATE		_IOR(CCCI_IOC_MAGIC, 1, unsigned int)
/* audio */
#define CCCI_IOC_PCM_BASE_ADDR		_IOR(CCCI_IOC_MAGIC, 2, unsigned int)
/* audio */
#define CCCI_IOC_PCM_LEN			\
	_IOR(CCCI_IOC_MAGIC, 3, unsigned int)
/* muxreport, mdlogger */
#define CCCI_IOC_FORCE_MD_ASSERT		_IO(CCCI_IOC_MAGIC, 4)
/* mdlogger */
#define CCCI_IOC_ALLOC_MD_LOG_MEM		_IO(CCCI_IOC_MAGIC, 5)
/* md_init */
#define CCCI_IOC_DO_MD_RST			_IO(CCCI_IOC_MAGIC, 6)
/* md_init */
#define CCCI_IOC_SEND_RUNTIME_DATA		_IO(CCCI_IOC_MAGIC, 7)
/* md_init */
#define CCCI_IOC_GET_MD_INFO		_IOR(CCCI_IOC_MAGIC, 8, unsigned int)
/* mdlogger */
#define CCCI_IOC_GET_MD_EX_TYPE		_IOR(CCCI_IOC_MAGIC, 9, unsigned int)
/* muxreport */
#define CCCI_IOC_SEND_STOP_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 10)
/* muxreport */
#define CCCI_IOC_SEND_START_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 11)
/* md_init */
#define CCCI_IOC_DO_STOP_MD			_IO(CCCI_IOC_MAGIC, 12)
/* md_init */
#define CCCI_IOC_DO_START_MD			_IO(CCCI_IOC_MAGIC, 13)
/* RILD, factory */
#define CCCI_IOC_ENTER_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 14)
/* RILD, factory */
#define CCCI_IOC_LEAVE_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 15)
/* md_init, abandoned */
#define CCCI_IOC_POWER_ON_MD			_IO(CCCI_IOC_MAGIC, 16)
/* md_init, abandoned */
#define CCCI_IOC_POWER_OFF_MD			_IO(CCCI_IOC_MAGIC, 17)
/* md_init, abandoned */
#define CCCI_IOC_POWER_ON_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 18)
/* md_init, abandoned */
#define CCCI_IOC_POWER_OFF_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 19)
/* RILD, factory */
#define CCCI_IOC_SIM_SWITCH		_IOW(CCCI_IOC_MAGIC, 20, unsigned int)
/* md_init */
#define CCCI_IOC_SEND_BATTERY_INFO		_IO(CCCI_IOC_MAGIC, 21)
/* RILD */
#define CCCI_IOC_SIM_SWITCH_TYPE		\
	_IOR(CCCI_IOC_MAGIC, 22, unsigned int)
/* RILD */
#define CCCI_IOC_STORE_SIM_MODE			\
	_IOW(CCCI_IOC_MAGIC, 23, unsigned int)
/* RILD */
#define CCCI_IOC_GET_SIM_MODE			\
	_IOR(CCCI_IOC_MAGIC, 24, unsigned int)
/* META, md_init, muxreport */
#define CCCI_IOC_RELOAD_MD_TYPE			_IO(CCCI_IOC_MAGIC, 25)
/* terservice */
#define CCCI_IOC_GET_SIM_TYPE			\
	_IOR(CCCI_IOC_MAGIC, 26, unsigned int)
/* terservice */
#define CCCI_IOC_ENABLE_GET_SIM_TYPE	\
	_IOW(CCCI_IOC_MAGIC, 27, unsigned int)
/* icusbd */
#define CCCI_IOC_SEND_ICUSB_NOTIFY		\
	_IOW(CCCI_IOC_MAGIC, 28, unsigned int)
/* md_init */
#define CCCI_IOC_SET_MD_IMG_EXIST		\
	_IOW(CCCI_IOC_MAGIC, 29, unsigned int)
/* META */
#define CCCI_IOC_GET_MD_IMG_EXIST		\
	_IOR(CCCI_IOC_MAGIC, 30, unsigned int)
/* RILD */
#define CCCI_IOC_GET_MD_TYPE			\
	_IOR(CCCI_IOC_MAGIC, 31, unsigned int)
/* RILD */
#define CCCI_IOC_STORE_MD_TYPE			\
	_IOW(CCCI_IOC_MAGIC, 32, unsigned int)
/* META */
#define CCCI_IOC_GET_MD_TYPE_SAVING		\
	_IOR(CCCI_IOC_MAGIC, 33, unsigned int)
/* mdlogger */
#define CCCI_IOC_GET_EXT_MD_POST_FIX	\
	_IOR(CCCI_IOC_MAGIC, 34, unsigned int)
/* RILD */
#define CCCI_IOC_FORCE_FD			\
	_IOW(CCCI_IOC_MAGIC, 35, unsigned int)
/* md_init */
#define CCCI_IOC_AP_ENG_BUILD			\
	_IOW(CCCI_IOC_MAGIC, 36, unsigned int)
/* md_init */
#define CCCI_IOC_GET_MD_MEM_SIZE		\
	_IOR(CCCI_IOC_MAGIC, 37, unsigned int)
/* RILD */
#define CCCI_IOC_UPDATE_SIM_SLOT_CFG	\
	_IOW(CCCI_IOC_MAGIC, 38, unsigned int)
/* md_init */
#define CCCI_IOC_GET_CFG_SETTING		\
	_IOW(CCCI_IOC_MAGIC, 39, unsigned int)
/* md_init */
#define CCCI_IOC_SET_MD_SBP_CFG			\
	_IOW(CCCI_IOC_MAGIC, 40, unsigned int)
/* md_init */
#define CCCI_IOC_GET_MD_SBP_CFG			\
	_IOW(CCCI_IOC_MAGIC, 41, unsigned int)
/* mdlogger, META */
#define CCCI_IOC_GET_MD_PROTOCOL_TYPE		\
	_IOR(CCCI_IOC_MAGIC, 42, char[16])
/* md_init: phase from P */
#define CCCI_IOC_SEND_SIGNAL_TO_USER		\
	_IOW(CCCI_IOC_MAGIC, 43, unsigned int)
/* md_init */
#define CCCI_IOC_RESET_MD1_MD3_PCCIF		\
	_IO(CCCI_IOC_MAGIC, 45)
#define CCCI_IOC_SIM_LOCK_RANDOM_PATTERN \
	_IOW(CCCI_IOC_MAGIC, 46, unsigned int)
/* md_init */
#define CCCI_IOC_SET_BOOT_DATA			\
	_IOW(CCCI_IOC_MAGIC, 47, unsigned int[16])

/* for user space share memory user */
#define CCCI_IOC_SMEM_BASE			\
	_IOR(CCCI_IOC_MAGIC, 48, unsigned int)
#define CCCI_IOC_SMEM_LEN			\
	_IOR(CCCI_IOC_MAGIC, 49, unsigned int)
#define CCCI_IOC_SMEM_TX_NOTIFY			\
	_IOW(CCCI_IOC_MAGIC, 50, unsigned int)
#define CCCI_IOC_SMEM_RX_POLL			\
	_IOR(CCCI_IOC_MAGIC, 51, unsigned int)
#define CCCI_IOC_SMEM_SET_STATE			\
	_IOW(CCCI_IOC_MAGIC, 52, unsigned int)
#define CCCI_IOC_SMEM_GET_STATE			\
	_IOR(CCCI_IOC_MAGIC, 53, unsigned int)

/*md_init*/
#define CCCI_IOC_SET_CCIF_CG			\
	_IOW(CCCI_IOC_MAGIC, 54, unsigned int)
/* RILD */
#define CCCI_IOC_SET_EFUN			\
	_IOW(CCCI_IOC_MAGIC, 55, unsigned int)
/*mdlogger*/
#define CCCI_IOC_MDLOG_DUMP_DONE		\
	_IO(CCCI_IOC_MAGIC, 56)
/* mdlogger */
#define CCCI_IOC_GET_OTHER_MD_STATE		\
	_IOR(CCCI_IOC_MAGIC, 57, unsigned int)
/* META */
#define CCCI_IOC_SET_MD_BOOT_MODE		\
	_IOW(CCCI_IOC_MAGIC, 58, unsigned int)
/* md_init */
#define CCCI_IOC_GET_MD_BOOT_MODE		\
	_IOR(CCCI_IOC_MAGIC, 59, unsigned int)
/* RILD */
#define CCCI_IOC_GET_AT_CH_NUM			\
	_IOR(CCCI_IOC_MAGIC, 60, unsigned int)

/* for user space CCB lib user */
#define CCCI_IOC_CCB_CTRL_BASE			\
	_IOR(CCCI_IOC_MAGIC, 61, unsigned int)
#define CCCI_IOC_CCB_CTRL_LEN			\
	_IOR(CCCI_IOC_MAGIC, 62, unsigned int)
#define CCCI_IOC_GET_CCB_CONFIG_LENGTH	\
	_IOR(CCCI_IOC_MAGIC, 63, unsigned int)
#define CCCI_IOC_GET_CCB_CONFIG			\
	_IOWR(CCCI_IOC_MAGIC, 64, struct ccci_ccb_config)
#define CCCI_IOC_CCB_CTRL_OFFSET		\
	_IOR(CCCI_IOC_MAGIC, 65, unsigned int)
#define CCCI_IOC_GET_CCB_DEBUG_VAL		\
	_IOWR(CCCI_IOC_MAGIC, 67, struct ccci_ccb_debug)

#define CCCI_IOC_CCB_CTRL_INFO			\
	_IOWR(CCCI_IOC_MAGIC, 71, struct ccb_ctrl_info)

#define CCCI_IOC_SET_HEADER			\
	_IO(CCCI_IOC_MAGIC,  112) /* emcs_va */
#define CCCI_IOC_CLR_HEADER			\
	_IO(CCCI_IOC_MAGIC,  113) /* emcs_va */

/* mdlogger */
#define CCCI_IOC_DL_TRAFFIC_CONTROL		\
	_IOW(CCCI_IOC_MAGIC, 119, unsigned int)
/* RILD  factory */
#define CCCI_IOC_ENTER_DEEP_FLIGHT_ENHANCED     \
	_IO(CCCI_IOC_MAGIC,  123)
/* RILD  factory */
#define CCCI_IOC_LEAVE_DEEP_FLIGHT_ENHANCED     \
	_IO(CCCI_IOC_MAGIC,  124)


#define CCCI_IPC_MAGIC 'P' /* only for IPC user */
#define CCCI_IPC_RESET_RECV			_IO(CCCI_IPC_MAGIC, 0)
#define CCCI_IPC_RESET_SEND			_IO(CCCI_IPC_MAGIC, 1)
#define CCCI_IPC_WAIT_MD_READY			_IO(CCCI_IPC_MAGIC, 2)
#define CCCI_IPC_KERN_WRITE_TEST		_IO(CCCI_IPC_MAGIC, 3)
#define CCCI_IPC_UPDATE_TIME			_IO(CCCI_IPC_MAGIC, 4)
#define CCCI_IPC_WAIT_TIME_UPDATE		_IO(CCCI_IPC_MAGIC, 5)
#define CCCI_IPC_UPDATE_TIMEZONE		_IO(CCCI_IPC_MAGIC, 6)

/* ======================================================================= */
/* CCCI Channel ID and Message ID definations */
/* ======================================================================= */
#define C2K_MD_LOG_TX_Q		3
#define C2K_MD_LOG_RX_Q		3
#define C2K_PCM_TX_Q		1
#define C2K_PCM_RX_Q		1

enum CCCI_CH {
	CCCI_CONTROL_RX = 0,
	CCCI_CONTROL_TX = 1,
	CCCI_SYSTEM_RX = 2,
	CCCI_SYSTEM_TX = 3,
	CCCI_PCM_RX = 4,
	CCCI_PCM_TX = 5,
	CCCI_UART1_RX = 6, /* META */
	CCCI_UART1_RX_ACK = 7,
	CCCI_UART1_TX = 8,
	CCCI_UART1_TX_ACK = 9,
	CCCI_UART2_RX = 10, /* MUX */
	CCCI_UART2_RX_ACK = 11,
	CCCI_UART2_TX = 12,
	CCCI_UART2_TX_ACK = 13,
	CCCI_FS_RX = 14,
	CCCI_FS_TX = 15,
	CCCI_PMIC_RX = 16,
	CCCI_PMIC_TX = 17,
	CCCI_UEM_RX = 18,
	CCCI_UEM_TX = 19,
	CCCI_CCMNI1_RX = 20,
	CCCI_CCMNI1_RX_ACK = 21,
	CCCI_CCMNI1_TX = 22,
	CCCI_CCMNI1_TX_ACK = 23,
	CCCI_CCMNI2_RX = 24,
	CCCI_CCMNI2_RX_ACK = 25,
	CCCI_CCMNI2_TX = 26,
	CCCI_CCMNI2_TX_ACK = 27,
	CCCI_CCMNI3_RX = 28,
	CCCI_CCMNI3_RX_ACK = 29,
	CCCI_CCMNI3_TX = 30,
	CCCI_CCMNI3_TX_ACK = 31,
	CCCI_RPC_RX = 32,
	CCCI_RPC_TX = 33,
	CCCI_IPC_RX = 34,
	CCCI_IPC_RX_ACK = 35,
	CCCI_IPC_TX = 36,
	CCCI_IPC_TX_ACK = 37,
	CCCI_IPC_UART_RX = 38,
	CCCI_IPC_UART_RX_ACK = 39,
	CCCI_IPC_UART_TX = 40,
	CCCI_IPC_UART_TX_ACK = 41,
	CCCI_MD_LOG_RX = 42,
	CCCI_MD_LOG_TX = 43,
	/* ch44~49 reserved for ARM7 */
	CCCI_IT_RX = 50,
	CCCI_IT_TX = 51,
	CCCI_IMSV_UL = 52,
	CCCI_IMSV_DL = 53,
	CCCI_IMSC_UL = 54,
	CCCI_IMSC_DL = 55,
	CCCI_IMSA_UL = 56,
	CCCI_IMSA_DL = 57,
	CCCI_IMSDC_UL = 58,
	CCCI_IMSDC_DL = 59,
	CCCI_ICUSB_RX = 60,
	CCCI_ICUSB_TX = 61,
	CCCI_LB_IT_RX = 62,
	CCCI_LB_IT_TX = 63,
	CCCI_CCMNI1_DL_ACK = 64,
	CCCI_CCMNI2_DL_ACK = 65,
	CCCI_CCMNI3_DL_ACK = 66,
	CCCI_STATUS_RX = 67,
	CCCI_STATUS_TX = 68,
	CCCI_CCMNI4_RX                  = 69,
	CCCI_CCMNI4_RX_ACK              = 70,
	CCCI_CCMNI4_TX                  = 71,
	CCCI_CCMNI4_TX_ACK              = 72,
	CCCI_CCMNI4_DLACK_RX            = 73,
	CCCI_CCMNI5_RX                  = 74,
	CCCI_CCMNI5_RX_ACK              = 75,
	CCCI_CCMNI5_TX                  = 76,
	CCCI_CCMNI5_TX_ACK              = 77,
	CCCI_CCMNI5_DLACK_RX            = 78,
	CCCI_CCMNI6_RX                  = 79,
	CCCI_CCMNI6_RX_ACK              = 80,
	CCCI_CCMNI6_TX                  = 81,
	CCCI_CCMNI6_TX_ACK              = 82,
	CCCI_CCMNI6_DLACK_RX            = 83,
	CCCI_CCMNI7_RX                  = 84,
	CCCI_CCMNI7_RX_ACK              = 85,
	CCCI_CCMNI7_TX                  = 86,
	CCCI_CCMNI7_TX_ACK              = 87,
	CCCI_CCMNI7_DLACK_RX            = 88,
	CCCI_CCMNI8_RX                  = 89,
	CCCI_CCMNI8_RX_ACK              = 90,
	CCCI_CCMNI8_TX                  = 91,
	CCCI_CCMNI8_TX_ACK              = 92,
	CCCI_CCMNI8_DLACK_RX            = 93,
	CCCI_MDL_MONITOR_DL             = 94,
	CCCI_MDL_MONITOR_UL             = 95,
	CCCI_CCMNILAN_RX                = 96,
	CCCI_CCMNILAN_RX_ACK            = 97,
	CCCI_CCMNILAN_TX                = 98,
	CCCI_CCMNILAN_TX_ACK            = 99,
	CCCI_CCMNILAN_DLACK_RX          = 100,
	CCCI_IMSEM_UL                   = 101,
	CCCI_IMSEM_DL                   = 102,
	CCCI_CCMNI10_RX                 = 103,
	CCCI_CCMNI10_RX_ACK             = 104,
	CCCI_CCMNI10_TX                 = 105,
	CCCI_CCMNI10_TX_ACK             = 106,
	CCCI_CCMNI10_DLACK_RX           = 107,
	CCCI_CCMNI11_RX                 = 108,
	CCCI_CCMNI11_RX_ACK             = 109,
	CCCI_CCMNI11_TX                 = 110,
	CCCI_CCMNI11_TX_ACK             = 111,
	CCCI_CCMNI11_DLACK_RX           = 112,
	CCCI_CCMNI12_RX                 = 113,
	CCCI_CCMNI12_RX_ACK             = 114,
	CCCI_CCMNI12_TX                 = 115,
	CCCI_CCMNI12_TX_ACK             = 116,
	CCCI_CCMNI12_DLACK_RX           = 117,
	CCCI_CCMNI13_RX                 = 118,
	CCCI_CCMNI13_RX_ACK             = 119,
	CCCI_CCMNI13_TX                 = 120,
	CCCI_CCMNI13_TX_ACK             = 121,
	CCCI_CCMNI13_DLACK_RX           = 122,
	CCCI_CCMNI14_RX                 = 123,
	CCCI_CCMNI14_RX_ACK             = 124,
	CCCI_CCMNI14_TX                 = 125,
	CCCI_CCMNI14_TX_ACK             = 126,
	CCCI_CCMNI14_DLACK_RX           = 127,
	CCCI_CCMNI15_RX                 = 128,
	CCCI_CCMNI15_RX_ACK             = 129,
	CCCI_CCMNI15_TX                 = 130,
	CCCI_CCMNI15_TX_ACK             = 131,
	CCCI_CCMNI15_DLACK_RX           = 132,
	CCCI_CCMNI16_RX                 = 133,
	CCCI_CCMNI16_RX_ACK             = 134,
	CCCI_CCMNI16_TX                 = 135,
	CCCI_CCMNI16_TX_ACK             = 136,
	CCCI_CCMNI16_DLACK_RX           = 137,
	CCCI_CCMNI17_RX                 = 138,
	CCCI_CCMNI17_RX_ACK             = 139,
	CCCI_CCMNI17_TX                 = 140,
	CCCI_CCMNI17_TX_ACK             = 141,
	CCCI_CCMNI17_DLACK_RX           = 142,
	CCCI_CCMNI18_RX                 = 143,
	CCCI_CCMNI18_RX_ACK             = 144,
	CCCI_CCMNI18_TX                 = 145,
	CCCI_CCMNI18_TX_ACK             = 146,
	CCCI_CCMNI18_DLACK_RX           = 147,
	CCCI_CCMNI19_RX                 = 148,
	CCCI_CCMNI19_RX_ACK             = 149,
	CCCI_CCMNI19_TX                 = 150,
	CCCI_CCMNI19_TX_ACK             = 151,
	CCCI_CCMNI19_DLACK_RX           = 152,
	CCCI_CCMNI20_RX                 = 153,
	CCCI_CCMNI20_RX_ACK             = 154,
	CCCI_CCMNI20_TX                 = 155,
	CCCI_CCMNI20_TX_ACK             = 156,
	CCCI_CCMNI20_DLACK_RX           = 157,
	CCCI_CCMNI21_RX                 = 158,
	CCCI_CCMNI21_RX_ACK             = 159,
	CCCI_CCMNI21_TX                 = 160,
	CCCI_CCMNI21_TX_ACK             = 161,
	CCCI_CCMNI21_DLACK_RX           = 162,
	CCCI_ATCP_RX			= 163,
	CCCI_ATCP_TX			= 164,
	CCCI_C2K_PPP_RX			= 165,
	CCCI_C2K_PPP_TX			= 166,
	CCCI_C2K_AGPS_RX		= 167,
	CCCI_C2K_AGPS_TX		= 168,
	CCCI_IMSM_RX			= 169,
	CCCI_IMSM_TX			= 170,
	CCCI_WOA_RX			= 171,
	CCCI_WOA_TX			= 172,
	CCCI_XCAP_RX			= 173,
	CCCI_XCAP_TX			= 174,
	CCCI_BIP_RX			= 175,
	CCCI_BIP_TX			= 176,
	CCCI_UDC_RX			= 177,
	CCCI_UDC_TX			= 178,

	CCCI_MIPI_CHANNEL_RX	= 179,
	CCCI_MIPI_CHANNEL_TX	= 180,

	CCCI_TCHE_RX			= 181,
	CCCI_TCHE_TX			= 182,
	CCCI_DISP_RX			= 183,
	CCCI_DISP_TX			= 184,
	CCCI_WIFI_RX			= 187,
	CCCI_WIFI_TX			= 188,
	CCCI_VTS_RX			= 189,
	CCCI_VTS_TX			= 190,

	CCCI_IKERAW_RX			= 191,
	CCCI_IKERAW_TX			= 192,

	CCCI_MD_DIRC_RX			= 200,
	CCCI_MD_DIRC_TX			= 201,
	CCCI_TIME_RX			= 202,
	CCCI_TIME_TX			= 203,
	CCCI_GARB_RX			= 204,
	CCCI_GARB_TX			= 205,

	CCCI_C2K_PPP_DATA, /* data ch for c2k */

	CCCI_C2K_AT,	/*rild AT ch for c2k*/
	CCCI_C2K_AT2,	/*rild AT2 ch for c2k*/
	CCCI_C2K_AT3,	/*rild AT3 ch for c2k*/
	CCCI_C2K_AT4,	/*rild AT4 ch for c2k*/
	CCCI_C2K_AT5,	/*rild AT5 ch for c2k*/
	CCCI_C2K_AT6,	/*rild AT6 ch for c2k*/
	CCCI_C2K_AT7,	/*rild AT7 ch for c2k*/
	CCCI_C2K_AT8,	/*rild AT8 ch for c2k*/
	CCCI_C2K_LB_DL,	/*downlink loopback*/

	/* virtual channels */
	CCCI_DUMMY_CH,
	CCCI_SMEM_CH,
	CCCI_CCB_CTRL,
	CCCI_MAX_CH_NUM, /* RX channel ID should NOT be >= this!! */
	CCCI_OVER_MAX_CH, /* for C2K channel mapping */
	CCCI_MONITOR_CH_ID = 0xf0000000, /* for mdinit */
	CCCI_FORCE_ASSERT_CH = 20090215,
	CCCI_INVALID_CH_ID = 0xffffffff,
};

enum c2k_channel {
	CTRL_CH_C2K = 0,
	CTRL_CH_C2K_EXCP = 1,
	AUDIO_CH_C2K = 1,
	DATA_PPP_CH_C2K = 2,
	MDLOG_CTRL_CH_C2K = 3,
	FS_CH_C2K = 4,
	AT_CH_C2K = 5,
	AGPS_CH_C2K = 6,
	AT2_CH_C2K = 7,
	AT3_CH_C2K = 8,
	MDLOG_CH_C2K = 9,
	AT4_CH_C2K = 10,
	STATUS_CH_C2K = 11,
	NET1_CH_C2K = 12,
	NET2_CH_C2K = 13,	/*need sync with c2k */
	NET3_CH_C2K = 14,	/*need sync with c2k */
	NET4_CH_C2K = 15,
	NET5_CH_C2K = 16,
	NET6_CH_C2K = 17,	/*need sync with c2k */
	NET7_CH_C2K = 18,
	NET8_CH_C2K = 19,
	AT5_CH_C2K = 20,
	AT6_CH_C2K = 21,
	AT7_CH_C2K = 22,
	AT8_CH_C2K = 23,
	NET9_CH_C2K = 24,	/*unused port*/
	NET10_CH_C2K = 25,	/*need sync with c2k */
	NET11_CH_C2K = 26,	/*need sync with c2k */
	NET12_CH_C2K = 27,
	NET13_CH_C2K = 28,
	NET14_CH_C2K = 29,	/*need sync with c2k */
	NET15_CH_C2K = 30,
	NET16_CH_C2K = 31,
	NET17_CH_C2K = 32,
	NET18_CH_C2K = 33,	/*need sync with c2k */
	NET19_CH_C2K = 34,	/*need sync with c2k */
	NET20_CH_C2K = 35,
	NET21_CH_C2K = 36,

	C2K_MAX_CH_NUM,
	C2K_OVER_MAX_CH,

	LOOPBACK_C2K = 255,
	MD2AP_LOOPBACK_C2K = 256,
};

enum md_bc_event {
	MD_STA_EV_INVALID = 0,
	MD_STA_EV_RESET_REQUEST,
	MD_STA_EV_F_ASSERT_REQUEST,
	MD_STA_EV_STOP_REQUEST,
	MD_STA_EV_START_REQUEST,
	MD_STA_EV_ENTER_FLIGHT_REQUEST,
	MD_STA_EV_LEAVE_FLIGHT_REQUEST,
	MD_STA_EV_ENTER_FLIGHT_E_REQUEST,
	MD_STA_EV_LEAVE_FLIGHT_E_REQUEST,
	MD_STA_EV_HS1,
	MD_STA_EV_READY,
	MD_STA_EV_EXCEPTION,
	MD_STA_EV_STOP,
};

/* ========================================================================= */
/* common API */
/* ========================================================================= */
void ccci_sysfs_add_md(int md_id, void *kobj);
int ccci_register_dev_node(const char *name, int major_id, int minor);

#ifdef CONFIG_MEDIATEK_MT6577_AUXADC
int ccci_get_adc_num(void);
int ccci_get_adc_val(void);
#endif

int hif_empty_query(int qno);

#ifdef FEATURE_SCP_CCCI_SUPPORT
extern void fsm_scp_init0(void);
#endif
#endif	/* __CCCI_CORE_H__ */

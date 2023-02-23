/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __MTK_CHARGER_TYPE_H__
#define __MTK_CHARGER_TYPE_H__

#ifdef CONFIG_FACTORY_BUILD
#define SDP_ICL		600
#define SDP_FCC		500
#else
#define SDP_ICL		500
#define SDP_FCC		500
#endif
#define SDP_VINMIN	4200

#define CDP_ICL		1500
#define CDP_FCC		1500
#define CDP_VINMIN	4200

#define DCP_ICL		1500
#define DCP_FCC		1500
#define DCP_VINMIN	4200

#define C2C_ICL		1500
#define C2C_FCC		1500
#define C2C_VINMIN	4200

#define OCP_ICL		1200
#define OCP_FCC		1200
#define OCP_VINMIN	4200

#define OTHER_ICL	500
#define OTHER_FCC	500
#define OTHER_VINMIN	4200

#define FLOAT_ICL	1000
#define FLOAT_FCC	1000
#define FLOAT_VINMIN	4200

#define QC2_ICL		1200
#define QC2_FCC		2500
#define QC2_VINMIN	8000

#define QC3_18_ICL	1800
#define QC3_18_FCC	3500
#define QC3_18_VINMIN	8000

#define QC3_27_ICL	2600
#define QC3_27_FCC	5400
#define QC3_27_VINMIN	8000

#define QC35_18_ICL	2200
#define QC35_18_FCC	4400
#define QC35_18_VINMIN	8000

#define QC35_27_ICL	2600
#define QC35_27_FCC	5400
#define QC35_27_VINMIN	8000

#define QC2_RUBYPLUS_ICL	1200
#define QC2_RUBYPLUS_FCC	1200
#define QC2_RUBYPLUS_VINMIN	8000

#define QC3_18_RUBYPLUS_ICL	1800
#define QC3_18_RUBYPLUS_FCC	1800
#define QC3_18_RUBYPLUS_VINMIN	8500

#define QC3_27_RUBYPLUS_ICL	2500
#define QC3_27_RUBYPLUS_FCC	2500
#define QC3_27_RUBYPLUS_VINMIN	8500

#define QC35_18_RUBYPLUS_ICL	2200
#define QC35_18_RUBYPLUS_FCC	2200
#define QC35_18_RUBYPLUS_VINMIN	9000

#define QC35_27_RUBYPLUS_ICL	2200
#define QC35_27_RUBYPLUS_FCC	2200
#define QC35_27_RUBYPLUS_VINMIN	9000

#define QC2_RUBYPLUS_DFAULT_ICL 2200
#define QC2_RUBYPLUS_DFAULT_FCC 2200

#define QC3_18_RUBYPLUS_DFAULT_ICL	2600
#define QC3_18_RUBYPLUS_DFAULT_FCC	2600

#define QC3_27_RUBYPLUS_DFAULT_ICL	2600
#define QC3_27_RUBYPLUS_DFAULT_FCC	2600

#define QC35_18_RUBYPLUS_DFAULT_ICL	2400
#define QC35_18_RUBYPLUS_DFAULT_FCC	2400

#define QC35_27_RUBYPLUS_DFAULT_ICL	2400
#define QC35_27_RUBYPLUS_DFAULT_FCC	2400

#define PD3_RUBYPLUS_DFAULT_ICL		1500

#define PD3_ICL		2500
#define PD3_VINMIN	8000

#define DEFAULT_ICL	550
#define DEFAULT_FCC	500
#define DEFAULT_VINMIN	4700
#define DEFAULT_LOW_VBUS	8000
#define DEFAULT_FV	8900
#define DEFAULT_ITERM	200

#define BBC_BUCK_HIGH_VBUS	9600
#define BBC_BUCK_LOW_VBUS	9300
#define QC3_PULSE_STEP		200
#define QC35_PULSE_STEP		20

#define CV_VBAT_MP2762_WA	8800
#define CV_IBAT_MP2762_WA	1100
#define CV_VBUS_MP2762_WA	9200
#define CV_MP2762_WA_COUNT	4

enum charger_type {
	CHARGER_UNKNOWN = 0,
	STANDARD_HOST,		/* USB : 450mA */
	CHARGING_HOST,
	NONSTANDARD_CHARGER,	/* AC : 450mA~1A */
	STANDARD_CHARGER,	/* AC : ~1A */
	APPLE_2_1A_CHARGER, /* 2.1A apple charger */
	APPLE_1_0A_CHARGER, /* 1A apple charger */
	APPLE_0_5A_CHARGER, /* 0.5A apple charger */
	WIRELESS_CHARGER,
	HVDCP_CHARGER,	/* QC2 */
};

enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,
	QUICK_CHARGE_SUPER,
	QUICK_CHARGE_MAX,
};

enum xmusb350_chg_type {
	XMUSB350_TYPE_OCP = 0x1,
	XMUSB350_TYPE_FLOAT = 0x2,
	XMUSB350_TYPE_SDP = 0x3,
	XMUSB350_TYPE_CDP = 0x4,
	XMUSB350_TYPE_DCP = 0x5,
	XMUSB350_TYPE_HVDCP_2 = 0x6,
	XMUSB350_TYPE_HVDCP_3 = 0x7,
	XMUSB350_TYPE_HVDCP_35_18 = 0x8,
	XMUSB350_TYPE_HVDCP_35_27 = 0x9,
	XMUSB350_TYPE_HVDCP_3_18 = 0xA,
	XMUSB350_TYPE_HVDCP_3_27 = 0xB,
	XMUSB350_TYPE_PD = 0xC,
	XMUSB350_TYPE_PD_DR = 0xD,
	XMUSB350_TYPE_HVDCP = 0x10,
	XMUSB350_TYPE_UNKNOW = 0x11,
};

enum xmusb350_pulse_type {
	QC3_DM_PULSE,
	QC3_DP_PULSE,
	QC35_DM_PULSE,
	QC35_DP_PULSE,
};

enum xmusb350_qc_mode {
	QC_MODE_QC2_5 = 1,
	QC_MODE_QC2_9,
	QC_MODE_QC2_12,
	QC_MODE_QC3_5,
	QC_MODE_QC35_5,
};

enum usbsw_state {
	USBSW_XMUSB350,
	USBSW_SOC,
};

enum hvdcp3_type {
	HVDCP3_NONE,
	HVDCP3_18,
	HVDCP3_27,
	HVDCP35_18,
	HVDCP35_27,
};

#if defined(CONFIG_USB_MTK_HDRC) || defined(CONFIG_USB_MU3D_DRV) \
	|| defined(CONFIG_EXTCON_MTK_USB)
extern void mt_usb_connect_v1(void);
extern void mt_usb_disconnect_v1(void);
#else
#define mt_usb_connect() do { } while (0)
#define mt_usb_disconnect() do { } while (0)
#endif

extern enum charger_type mt_get_charger_type(void);
extern void mtk_charger_int_handler(void);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT)
extern int register_charger_det_callback(int (*func)(int));
#endif /*CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT*/

#if defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6893) \
	||  defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6785)
extern bool is_usb_rdy(struct device *dev);
#else
extern bool is_usb_rdy(void);
#endif
extern bool mt_usb_is_device(void);
extern int is_otg_en(void);

#ifndef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
extern void mtk_pmic_enable_chr_type_det(bool en);
#endif

#endif /* __MTK_CHARGER_TYPE_H__ */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
//#include <linux/proc_fs.h>
//#include <linux/uaccess.h>

#ifndef __PMSR_H__
#define __PMSR_H__

#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define MONTYPE_RISING			(0)
#define MONTYPE_FALLING			(1)
#define MONTYPE_HIGH_LEVEL		(2)
#define MONITPE_LOW_LEVEL		(3)

#define SPEED_MODE_NORMAL		(0)
#define SPEED_MODE_SPEED		(1)

#define DEFAULT_MONTYPE			MONTYPE_HIGH_LEVEL
#define DEFAULT_SPEED_MODE		SPEED_MODE_SPEED

#define PMSR_PERIOD_MS			(1000)
#define WINDOW_LEN_SPEED		(PMSR_PERIOD_MS * 0x65B8)
#define WINDOW_LEN_NORMAL		(PMSR_PERIOD_MS * 0xD)
#define GET_EVENT_RATIO_SPEED(x)	((x)/(WINDOW_LEN_SPEED/1000))
#define GET_EVENT_RATIO_NORMAL(x)	((x)/(WINDOW_LEN_NORMAL/1000))

#define PMSR_TOOL_ACT_CHSET		(1u << 0)
#define PMSR_TOOL_ACT_ENABLE		(1u << 1)
#define PMSR_TOOL_ACT_DISABLE		(1u << 2)
#define PMSR_TOOL_ACT_WINDOW		(1u << 3)
#define PMSR_TOOL_ACT_RESUME		(1u << 4)
#define PMSR_TOOL_ACT_PAUSE		(1u << 5)
#define PMSR_TOOL_ACT_CH_OUTBUFFER	(1u << 7)
#define PMSR_TOOL_ACT_TEST_FAKE		(1u << 8)

#define SET_CH_MAX 4

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
struct pmsr_tool_scmi_data {
	unsigned int uid	: 24;
	unsigned int magic	: 8;
	unsigned int action;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
};
#elif IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
struct pmsr_tool_ipi_data {
	unsigned int uid;
	unsigned int action;
	unsigned int idx;
	unsigned int sig;
	unsigned int id;
	unsigned int window_len;
	unsigned int speed_mode;
	unsigned int montype;
};
#endif

struct pmsr_channel {
	unsigned int signal;	/* 2 bits, signal A~D */
	unsigned int id;	/* 5 bits, id 0~31    */
	unsigned int montype;	/* 2 bits, monitor type */
};

struct pmsr_cfg {
	struct pmsr_channel ch[SET_CH_MAX];	/* channel 0~3 config */
	unsigned int pmsr_speed_mode;		/* 0: normal, 1: high speed */
	unsigned int pmsr_window_len;
	bool enable;
	unsigned int pause;
	unsigned int output;
	unsigned int fake;
	/* pmsr_window_len
	 *	0: will be automatically updated for current speed mode
	 * for normal speed mode, set to WINDOW_LEN_NORMAL.
	 * for high speed mode, set to WINDOW_LEN_SPEED.
	 */
};

/* definitions for return result */
struct pmsr_result {
	struct pmsr_cfg cfg;
	unsigned char value[SET_CH_MAX];
};

#endif /* __PMSR_H__ */


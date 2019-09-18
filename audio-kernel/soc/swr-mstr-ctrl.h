/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

#ifndef _SWR_WCD_CTRL_H
#define _SWR_WCD_CTRL_H
#include <linux/module.h>
#include <soc/swr-wcd.h>
#include <linux/pm_qos.h>
#include <soc/qcom/pm.h>

#define SWR_ROW_48		0
#define SWR_ROW_50		1
#define SWR_ROW_64		2
#define SWR_MAX_COL		7 /* Cols = 16 */
#define SWR_MIN_COL		0 /* Cols = 2 */

#define SWR_WCD_NAME	"swr-wcd"

#define SWR_MSTR_PORT_LEN	8 /* Number of master ports */

#define SWRM_VERSION_1_0 0x01010000
#define SWRM_VERSION_1_2 0x01030000
#define SWRM_VERSION_1_3 0x01040000
#define SWRM_VERSION_1_5 0x01050000

#define SWR_MAX_CH_PER_PORT 8

#define SWR_MAX_SLAVE_DEVICES 11

enum {
	SWR_MSTR_PAUSE,
	SWR_MSTR_RESUME,
	SWR_MSTR_UP,
	SWR_MSTR_DOWN,
	SWR_MSTR_SSR,
};

enum swrm_pm_state {
	SWRM_PM_SLEEPABLE,
	SWRM_PM_AWAKE,
	SWRM_PM_ASLEEP,
};

enum {
	SWR_IRQ_FREE,
	SWR_IRQ_REGISTER,
};

enum {
	SWR_DAC_PORT,
	SWR_COMP_PORT,
	SWR_BOOST_PORT,
	SWR_VISENSE_PORT,
};

struct usecase {
	u8 num_port;
	u8 num_ch;
	u32 chrate;
};

struct port_params {
	u8 si;
	u8 off1;
	u8 off2;
	u8 hstart;/* head start */
	u8 hstop; /* head stop */
	u8 wd_len;/* word length */
	u8 bp_mode; /* block pack mode */
	u8 bgp_ctrl;/* block group control */
	u8 lane_ctrl;/* lane to be used */
};

struct swrm_mports {
	struct list_head port_req_list;
	bool port_en;
	u8 ch_en;
	u8 req_ch;
	u8 ch_rate;
	u8 offset1;
	u8 offset2;
	u8 sinterval;
	u8 hstart;
	u8 hstop;
	u8 blk_grp_count;
	u8 blk_pack_mode;
	u8 word_length;
	u8 lane_ctrl;
};

struct swrm_port_type {
	u8 port_type;
	u8 ch_mask;
};

struct swr_ctrl_platform_data {
	void *handle; /* holds priv data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*reg_irq)(void *handle, irqreturn_t(*irq_handler)(int irq,
			void *data), void *swr_handle, int type);
};

struct swr_mstr_ctrl {
	struct swr_master master;
	struct device *dev;
	struct resource *supplies;
	struct clk *mclk;
	int clk_ref_count;
	struct completion clk_off_complete;
	struct completion reset;
	struct completion broadcast;
	struct mutex clklock;
	struct mutex iolock;
	struct mutex devlock;
	struct mutex mlock;
	struct mutex reslock;
	struct mutex pm_lock;
	u32 swrm_base_reg;
	char __iomem *swrm_dig_base;
	u8 rcmd_id;
	u8 wcmd_id;
	u32 master_id;
	void *handle; /* SWR Master handle from client for read and writes */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*reg_irq)(void *handle, irqreturn_t(*irq_handler)(int irq,
			void *data), void *swr_handle, int type);
	int irq;
	int wake_irq;
	int version;
	int mclk_freq;
	u32 num_dev;
	int slave_status;
	struct swrm_mports mport_cfg[SWR_MAX_MSTR_PORT_NUM];
	struct list_head port_req_list;
	unsigned long port_req_pending;
	int state;
	struct platform_device *pdev;
	int num_rx_chs;
	u8 num_cfg_devs;
	struct mutex force_down_lock;
	int force_down_state;
	struct notifier_block event_notifier;
	struct work_struct dc_presence_work;
	u8 num_ports;
	struct swrm_port_type
			port_mapping[SWR_MSTR_PORT_LEN][SWR_MAX_CH_PER_PORT];
	int swr_irq;
	u32 clk_stop_mode0_supp;
	struct work_struct wakeup_work;
	u32 ipc_wakeup;
	bool dev_up;
	bool ipc_wakeup_triggered;
	struct pm_qos_request pm_qos_req;
	enum swrm_pm_state pm_state;
	wait_queue_head_t pm_wq;
	int wlock_holders;
	u32 intr_mask;
	u32 swr_irq_wakeup_capable;
};

#endif /* _SWR_WCD_CTRL_H */

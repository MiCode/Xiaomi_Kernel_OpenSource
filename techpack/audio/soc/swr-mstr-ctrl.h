/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SWR_WCD_CTRL_H
#define _SWR_WCD_CTRL_H
#include <linux/module.h>
#include <soc/swr-wcd.h>
#include <linux/pm_qos.h>
#include <soc/qcom/pm.h>
#include <soc/swr-common.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define SWR_MSTR_MAX_REG_ADDR	0x1740
#define SWR_MSTR_START_REG_ADDR	0x00
#define SWR_MSTR_MAX_BUF_LEN     32
#define BYTES_PER_LINE          12
#define SWR_MSTR_RD_BUF_LEN      8
#define SWR_MSTR_WR_BUF_LEN      32
#endif

#define SWR_ROW_48		0
#define SWR_ROW_50		1
#define SWR_ROW_64		3
#define SWR_COL_04		1 /* Cols = 4 */
#define SWR_MAX_COL		7 /* Cols = 16 */
#define SWR_MIN_COL		0 /* Cols = 2 */

#define SWR_WCD_NAME	"swr-wcd"

#define SWR_MSTR_PORT_LEN	8 /* Number of master ports */

#define SWRM_VERSION_1_0 0x01010000
#define SWRM_VERSION_1_2 0x01030000
#define SWRM_VERSION_1_3 0x01040000
#define SWRM_VERSION_1_5 0x01050000
#define SWRM_VERSION_1_5_1 0x01050001
#define SWRM_VERSION_1_6   0x01060000

#define SWR_MAX_CH_PER_PORT 8

#define SWRM_NUM_AUTO_ENUM_SLAVES    6

enum {
	SWR_MSTR_PAUSE,
	SWR_MSTR_RESUME,
	SWR_MSTR_UP,
	SWR_MSTR_DOWN,
	SWR_MSTR_SSR,
	SWR_MSTR_SSR_RESET,
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

enum {
	SWR_PDM = 0,
	SWR_PCM,
};

struct usecase {
	u8 num_port;
	u8 num_ch;
	u32 chrate;
};

struct swrm_mports {
	struct list_head port_req_list;
	bool port_en;
	u8 ch_en;
	u8 req_ch;
	u8 offset1;
	u8 offset2;
	u16 sinterval;
	u8 hstart;
	u8 hstop;
	u8 blk_grp_count;
	u8 blk_pack_mode;
	u8 word_length;
	u8 lane_ctrl;
	u8 dir;
	u8 stream_type;
	u32 ch_rate;
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
	int (*core_vote)(void *handle, bool enable);
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
	struct mutex irq_lock;
	u32 swrm_base_reg;
	char __iomem *swrm_dig_base;
	char __iomem *swrm_hctl_reg;
	u8 rcmd_id;
	u8 wcmd_id;
	u32 master_id;
	u32 dynamic_port_map_supported;
	void *handle; /* SWR Master handle from client for read and writes */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*core_vote)(void *handle, bool enable);
	int (*reg_irq)(void *handle, irqreturn_t(*irq_handler)(int irq,
			void *data), void *swr_handle, int type);
	int irq;
	int wake_irq;
	int version;
	int mclk_freq;
	int bus_clk;
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
	bool req_clk_switch;
	struct pm_qos_request pm_qos_req;
	enum swrm_pm_state pm_state;
	wait_queue_head_t pm_wq;
	int wlock_holders;
	u32 intr_mask;
	struct port_params **port_param;
	struct clk *lpass_core_hw_vote;
	struct clk *lpass_core_audio;
	u8 num_usecase;
	u32 swr_irq_wakeup_capable;
	int hw_core_clk_en;
	int aud_core_clk_en;
	int clk_src;
	u32 disable_div2_clk_switch;
	u32 rd_fifo_depth;
	u32 wr_fifo_depth;
	bool enable_slave_irq;
	u64 logical_dev[SWRM_NUM_AUTO_ENUM_SLAVES + 1];
	u64 phy_dev[SWRM_NUM_AUTO_ENUM_SLAVES + 1];
	bool use_custom_phy_addr;
	u32 is_always_on;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_swrm_dent;
	struct dentry *debugfs_peek;
	struct dentry *debugfs_poke;
	struct dentry *debugfs_reg_dump;
	unsigned int read_data;
#endif
};

#endif /* _SWR_WCD_CTRL_H */

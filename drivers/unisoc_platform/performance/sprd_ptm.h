/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRD_PTM_H__
#define __SPRD_PTM_H__

#define BM_CHN_MAX			11
#define BM_CHN_PARA			6
#define PTM_NAME			"sprd_ptm"

/* reg offset */
#define PTM_EN				0
#define INT_STU				0x4
#define FRE_CHG				0x8
#define MOD_SEL				0xc
#define FRE_INT				0x10
#define INT_CLR				0x14
#define CNT_CLR				0x18
#define CNT_WIN				0x1c
#define GRP_SEL				0x20

/* trace registers */
#define PTM_FUNNEL_FUNCTL		0x0
#define PTM_TPIU_CUR_PORTSZ		0x4
#define PTM_TMC_CTL			0x20
#define PTM_TMC_MODE			0x28
#define PTM_TMC_BUFWM			0x34
#define PTM_TMC_FFCR			0x304
#define PTM_CORESIGHT_LAR		0xfb0
#define PTM_TMC_PSCR			0x308
#define PTM_TMC_AXICTL			0x110
#define PTM_TMC_DBALO			0x118
#define PTM_TMC_DBAHI			0x11c
#define PTM_TMC_RSZ			0x4

/* PTM_EN */
#ifndef CONFIG_SPRD_PTM_R6P2
#define PTM_TRACE_LTCY_IDLE_EN		BIT(16)
#define PTM_TRACE_BW_IDLE_EN		BIT(15)
#endif
#define PTM_BW_LTCY_ALL_TRACE_EN	BIT(5)
#define PTM_TRACE_AREN			BIT(4)
#define PTM_TRACE_AWEN			BIT(3)
#define PTM_CMD_TRANS_EN		BIT(2)
#define PTM_BW_LTCY_CNT_EN		BIT(1)
#define PTM_ENABLE			BIT(0)
#define PTM_LTCY_TRACE_EN_OFFSET	7
#define PTM_LTCY_TRACE_EN_MSK		GENMASK(8, 0)
#define PTM_TRACE_QOS_BWITCY_EN		BIT(29)

/* INT_STU */
#define PTM_F_UP_INT_RAW		BIT(4)
#define PTM_F_UP_INT_MASK		BIT(3)
#define PTM_BW_LTCY_INT_RAW		BIT(2)
#define PTM_BW_LTCY_INT_MASK		BIT(1)
#define PTM_INT				BIT(0)

/* PTM_TMC_FFCR - 0x304 */
#define PTM_TMC_FFCR_EN_FMT		BIT(0)
#define PTM_TMC_FFCR_EN_TI		BIT(1)
#define PTM_TMC_FFCR_FLUSHMAN		BIT(6)
#define PTM_TMC_FFCR_STOP_ON_FLUSH	BIT(12)

/* TPIU_PORTSZ */
#define PTM_TPIU_PORT_SZ		BIT(31)

#define PTM_LTCY_MOD_OFFSET		7
#define PTM_TRACE_MOD_OFFSET		9
#ifdef CONFIG_SPRD_PTM_R6P2
#define PTM_USRID_MSK_OFFSET		16
#define PTM_USRID_MSK			GENMASK(9, 0)
#else
#define PTM_USRID_MSK_OFFSET		8
#define PTM_USRID_MSK			GENMASK(8, 0)
#endif
#define PTM_MASTERID_MSK		GENMASK(15, 0)
#define PTM_REG_MAX			GENMASK(31, 0)

#define PTM_FUNNEL_HOLDTIME_MASK	GENMASK(11, 8)
#define PTM_FUNNEL_HOLDTIME		GENMASK(10, 8)
#define PTM_CORESIGHT_UNLOCK		0xc5acce55

struct bm_per_info {
	u32 count;
	u32 t_start;
	u32 t_stop;
	u32 tmp1;
	u32 tmp2;
	u32 perf_data[BM_CHN_MAX][BM_CHN_PARA];
#ifdef CONFIG_SPRD_PTM_DIFF_R6P1
	u32 dpu_dcam_ovf[2][10];
#endif
};

#define BM_DATA_COUNT			5
#define BM_PER_CNT_RECORD_SIZE		800
#define BM_PER_CNT_BUF_SIZE		(sizeof(struct bm_per_info) \
					* BM_PER_CNT_RECORD_SIZE)
#define BM_LOG_FILE_PATH		"/mnt/obb/axi_per_log"
#define BM_LOG_FILE_SECONDS		(60  * 30)
#define BM_LOG_FILE_MAX_RECORDS		(BM_LOG_FILE_SECONDS * 100)
#define BM_TRACE_DEF_WINLEN		26000

enum ptm_trace_mode {
	CYCLE_CNT_MOD,
	FREQ_MOD,
	SINGLE_TRACE_MOD,
	AUTO_TRACE_MOD,
};

enum ptm_trace_out_mode {
	TPIU_MODE,
	ETR_MODE,
};

enum sprd_ptm_mode {
	INIT_MODE,
	LEGACY_MODE,
	TRACE_MODE,
};

enum ptm_lty_mode {
	ADD_ONE_IN_OS,
	ADD_OS,
	ADD_ONE,
	ADD_OS_IN_OS,
};

union sprd_ptm_mod_info {
	struct {
		struct task_struct	*bm_thl;
		u32			timer_interval;
		struct hrtimer		timer;
		bool			bm_perf_st;
		u32			bm_buf_write_cnt;
		void			*per_buf;
	} legacy;
	struct {
		void __iomem		*funnel_base;
		void __iomem		*tmc_base;
		void __iomem		*tpiu_base;
		void __iomem		*replicator_base;
		void __iomem		*etr_base;
		u32			winlen;
		u32			funnel_port;
		u32			out_mode;
		bool			cmd_eb;
		bool			trace_st;
	} trace;
};

struct sprd_ptm_chn_info {
	u32 usr_id;
	u32 usr_id_mask;
	u32 mster_id;
	u32 mster_id_mask;
	u32 grp_sel;
	u32 chnsel;
	u32 lty_mode;
};

struct sprd_ptm_dev {
	struct miscdevice		misc;
	spinlock_t			slock;
	struct completion		comp;
	void __iomem			*base;
	struct clk			*clk_cs;
	struct clk			*clk_cs_src;
	u32				grp_sel;
	int				irq;
	int				pub_chn;
	enum sprd_ptm_mode		mode;
	union sprd_ptm_mod_info		mode_info;
	struct sprd_ptm_chn_info	chn_info;
	const struct ptm_pvt_para	*pvt_data;
	const char			**sprd_ptm_list;
	u32				ptm_addr;
	u32				ptm_size;
};

struct ptm_pvt_para {
	u32				wbm_base;
	u32				rbm_base;
	u32				rly_base;
	u32				rtran_base;
	u32				wly_base;
	u32				wtran_base;
	u32				msterid_base;
	u32				trace_usr_base;
	u32				usrid_base;
	u32				grp_sel;
#ifdef CONFIG_SPRD_PTM_DIFF_R6P1
	u32				dpu_dcam_ovf_base;
#endif
};

static struct ptm_pvt_para ptm_v1_data[] = {
	{0x80, 0x9c, 0xc0, 0xdc, 0x100, 0x11c, 0x140, 0x15c, 0x160, 0xfac688},
};

#ifndef CONFIG_SPRD_PTM_DIFF_R6P1
static struct ptm_pvt_para ptm_v2_data[] = {
	{0x80, 0xa0, 0xc0, 0xe0, 0x100, 0x120, 0x140, 0x240, 0x160, 0x7fac688},
};

static struct ptm_pvt_para ptm_v3_data[] = {
	{0x180, 0x1a0, 0x1c0, 0x1e0, 0x200, 0x220, 0x140, 0x240, 0x160, 0x76543210},
};
#else
static struct ptm_pvt_para ptm_v2_data[] = {
	{0x80, 0xa0, 0xc0, 0xe0, 0x100, 0x120, 0x140, 0x240, 0x160, 0x7fac688,
	 0x410},
};

static struct ptm_pvt_para ptm_v3_data[] = {
	{0x180, 0x1a0, 0x1c0, 0x1e0, 0x200, 0x220, 0x140, 0x240, 0x160, 0x76543210,
	 0x410},
};
#endif

#endif

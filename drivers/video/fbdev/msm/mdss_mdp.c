/*
 * MDSS MDP Interface (used by framebuffer core)
 *
 * Copyright (c) 2007-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2007 Google Incorporated
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk/msm-clk.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>

#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/rpm-smd.h>
#include "soc/qcom/secure_buffer.h"
#include <asm/cacheflush.h>

#include "mdss.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_debug.h"
#include "mdss_smmu.h"

#include "mdss_mdp_trace.h"

#define AXI_HALT_TIMEOUT_US	0x4000
#define AUTOSUSPEND_TIMEOUT_MS	200
#define DEFAULT_MDP_PIPE_WIDTH	2048
#define RES_1080p		(1088*1920)
#define RES_UHD			(3840*2160)

#define MDP_DEVICE_ID		0x1A

struct mdss_data_type *mdss_res;
static u32 mem_protect_sd_ctrl_id;

static int mdss_fb_mem_get_iommu_domain(void)
{
	return mdss_smmu_get_domain_id(MDSS_IOMMU_DOMAIN_UNSECURE);
}

struct msm_mdp_interface mdp5 = {
	.init_fnc = mdss_mdp_overlay_init,
	.fb_mem_get_iommu_domain = mdss_fb_mem_get_iommu_domain,
	.fb_stride = mdss_mdp_fb_stride,
	.check_dsi_status = mdss_check_dsi_ctrl_status,
	.get_format_params = mdss_mdp_get_format_params,
};

#define IB_QUOTA 2000000000
#define AB_QUOTA 2000000000

#define MAX_AXI_PORT_COUNT 3

#define MEM_PROTECT_SD_CTRL 0xF
#define MEM_PROTECT_SD_CTRL_FLAT 0x14
#define MEM_PROTECT_SD_CTRL_SWITCH 0x18

static DEFINE_SPINLOCK(mdp_lock);
static DEFINE_SPINLOCK(mdss_mdp_intr_lock);
static DEFINE_MUTEX(mdp_clk_lock);
static DEFINE_MUTEX(mdp_iommu_ref_cnt_lock);
static DEFINE_MUTEX(mdp_fs_idle_pc_lock);
static DEFINE_MUTEX(mdp_sec_ref_cnt_lock);

static struct mdss_panel_intf pan_types[] = {
	{"dsi", MDSS_PANEL_INTF_DSI},
	{"edp", MDSS_PANEL_INTF_EDP},
	{"hdmi", MDSS_PANEL_INTF_HDMI},
};
static char mdss_mdp_panel[MDSS_MAX_PANEL_LEN];

struct mdss_hw mdss_mdp_hw = {
	.hw_ndx = MDSS_HW_MDP,
	.ptr = NULL,
	.irq_handler = mdss_mdp_isr,
};

/* define for h/w block with external driver */
struct mdss_hw mdss_misc_hw = {
	.hw_ndx = MDSS_HW_MISC,
	.ptr = NULL,
	.irq_handler = NULL,
};

#ifdef CONFIG_QCOM_BUS_SCALING
#define MDP_REG_BUS_VECTOR_ENTRY(ab_val, ib_val)	\
	{						\
		.src = MSM_BUS_MASTER_AMPSS_M0,		\
		.dst = MSM_BUS_SLAVE_DISPLAY_CFG,	\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

#define BUS_VOTE_19_MHZ 153600000
#define BUS_VOTE_40_MHZ 320000000
#define BUS_VOTE_80_MHZ 640000000

static struct msm_bus_vectors mdp_reg_bus_vectors[] = {
	MDP_REG_BUS_VECTOR_ENTRY(0, 0),
	MDP_REG_BUS_VECTOR_ENTRY(0, BUS_VOTE_19_MHZ),
	MDP_REG_BUS_VECTOR_ENTRY(0, BUS_VOTE_40_MHZ),
	MDP_REG_BUS_VECTOR_ENTRY(0, BUS_VOTE_80_MHZ),
};
static struct msm_bus_paths mdp_reg_bus_usecases[ARRAY_SIZE(
		mdp_reg_bus_vectors)];
static struct msm_bus_scale_pdata mdp_reg_bus_scale_table = {
	.usecase = mdp_reg_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_reg_bus_usecases),
	.name = "mdss_reg",
	.active_only = true,
};
static int mdss_mdp_parse_dt_bus_scale(struct platform_device *pdev);
#endif

u32 invalid_mdp107_wb_output_fmts[] = {
	MDP_XRGB_8888,
	MDP_RGBX_8888,
	MDP_BGRX_8888,
};

/*
 * struct intr_call - array of intr handlers
 * @func: intr handler
 * @arg: requested argument to the handler
 */
struct intr_callback {
	void (*func)(void *);
	void *arg;
};

/*
 * struct mdss_mdp_intr_reg - array of MDP intr register sets
 * @clr_off: offset to CLEAR reg
 * @en_off: offset to ENABLE reg
 * @status_off: offset to STATUS reg
 */
struct mdss_mdp_intr_reg {
	u32 clr_off;
	u32 en_off;
	u32 status_off;
};

/*
 * struct mdss_mdp_irq - maps each irq with i/f
 * @intr_type: type of interface
 * @intf_num: i/f the irq is associated with
 * @irq_mask: corresponding bit in the reg set
 * @reg_idx: which reg set to program
 */
struct mdss_mdp_irq {
	u32 intr_type;
	u32 intf_num;
	u32 irq_mask;
	u32 reg_idx;
};

static struct mdss_mdp_intr_reg mdp_intr_reg[] = {
	{ MDSS_MDP_REG_INTR_CLEAR, MDSS_MDP_REG_INTR_EN,
		MDSS_MDP_REG_INTR_STATUS },
	{ MDSS_MDP_REG_INTR2_CLEAR, MDSS_MDP_REG_INTR2_EN,
		MDSS_MDP_REG_INTR2_STATUS }
};

static struct mdss_mdp_irq mdp_irq_map[] =  {
	{ MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN, 1, MDSS_MDP_INTR_INTF_0_UNDERRUN, 0},
	{ MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN, 2, MDSS_MDP_INTR_INTF_1_UNDERRUN, 0},
	{ MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN, 3, MDSS_MDP_INTR_INTF_2_UNDERRUN, 0},
	{ MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN, 4, MDSS_MDP_INTR_INTF_3_UNDERRUN, 0},
	{ MDSS_MDP_IRQ_TYPE_INTF_VSYNC, 1, MDSS_MDP_INTR_INTF_0_VSYNC, 0},
	{ MDSS_MDP_IRQ_TYPE_INTF_VSYNC, 2, MDSS_MDP_INTR_INTF_1_VSYNC, 0},
	{ MDSS_MDP_IRQ_TYPE_INTF_VSYNC, 3, MDSS_MDP_INTR_INTF_2_VSYNC, 0},
	{ MDSS_MDP_IRQ_TYPE_INTF_VSYNC, 4, MDSS_MDP_INTR_INTF_3_VSYNC, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_COMP, 0, MDSS_MDP_INTR_PING_PONG_0_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_COMP, 1, MDSS_MDP_INTR_PING_PONG_1_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_COMP, 2, MDSS_MDP_INTR_PING_PONG_2_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_COMP, 3, MDSS_MDP_INTR_PING_PONG_3_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR, 0, MDSS_MDP_INTR_PING_PONG_0_RD_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR, 1, MDSS_MDP_INTR_PING_PONG_1_RD_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR, 2, MDSS_MDP_INTR_PING_PONG_2_RD_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR, 3, MDSS_MDP_INTR_PING_PONG_3_RD_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR, 0, MDSS_MDP_INTR_PING_PONG_0_WR_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR, 1, MDSS_MDP_INTR_PING_PONG_1_WR_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR, 2, MDSS_MDP_INTR_PING_PONG_2_WR_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR, 3, MDSS_MDP_INTR_PING_PONG_3_WR_PTR, 0},
	{ MDSS_MDP_IRQ_TYPE_WB_ROT_COMP, 0, MDSS_MDP_INTR_WB_0_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_WB_ROT_COMP, 1, MDSS_MDP_INTR_WB_1_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_WB_WFD_COMP, 0, MDSS_MDP_INTR_WB_2_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF, 0, MDSS_MDP_INTR_PING_PONG_0_AUTOREFRESH_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF, 1, MDSS_MDP_INTR_PING_PONG_1_AUTOREFRESH_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF, 2, MDSS_MDP_INTR_PING_PONG_2_AUTOREFRESH_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF, 3, MDSS_MDP_INTR_PING_PONG_3_AUTOREFRESH_DONE, 0},
	{ MDSS_MDP_IRQ_TYPE_CWB_OVERFLOW, 2, MDSS_MDP_INTR2_PING_PONG_2_CWB_OVERFLOW, 1},
	{ MDSS_MDP_IRQ_TYPE_CWB_OVERFLOW, 3, MDSS_MDP_INTR2_PING_PONG_3_CWB_OVERFLOW, 1}
};

static struct intr_callback *mdp_intr_cb;

static void mdss_mdp_footswitch_ctrl(struct mdss_data_type *mdata, int on);
static int mdss_mdp_parse_dt(struct platform_device *pdev);
static int mdss_mdp_parse_dt_pipe(struct platform_device *pdev);
static int mdss_mdp_parse_dt_mixer(struct platform_device *pdev);
static int mdss_mdp_parse_dt_wb(struct platform_device *pdev);
static int mdss_mdp_parse_dt_ctl(struct platform_device *pdev);
static int mdss_mdp_parse_dt_video_intf(struct platform_device *pdev);
static int mdss_mdp_parse_dt_handler(struct platform_device *pdev,
				char *prop_name, u32 *offsets, int len);
static int mdss_mdp_parse_dt_prop_len(struct platform_device *pdev,
				char *prop_name);
static int mdss_mdp_parse_dt_smp(struct platform_device *pdev);
static int mdss_mdp_parse_dt_prefill(struct platform_device *pdev);
static int mdss_mdp_parse_dt_misc(struct platform_device *pdev);
static int mdss_mdp_parse_dt_ad_cfg(struct platform_device *pdev);
static int mdss_mdp_parse_dt_ppb_off(struct platform_device *pdev);
static int mdss_mdp_parse_dt_cdm(struct platform_device *pdev);
static int mdss_mdp_parse_dt_dsc(struct platform_device *pdev);

static inline u32 is_mdp_irq_enabled(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int i;

	for (i = 0; i < ARRAY_SIZE(mdp_intr_reg); i++)
		if (mdata->mdp_irq_mask[i] != 0)
			return 1;

	if (mdata->mdp_hist_irq_mask)
		return 1;

	if (mdata->mdp_intf_irq_mask)
		return 1;

	return 0;
}

u32 mdss_mdp_fb_stride(u32 fb_index, u32 xres, int bpp)
{
	/* The adreno GPU hardware requires that the pitch be aligned to
	   32 pixels for color buffers, so for the cases where the GPU
	   is writing directly to fb0, the framebuffer pitch
	   also needs to be 32 pixel aligned */

	if (fb_index == 0)
		return ALIGN(xres, 32) * bpp;
	else
		return xres * bpp;
}

static void mdss_irq_mask(struct irq_data *data)
{
	struct mdss_data_type *mdata = irq_data_get_irq_chip_data(data);
	unsigned long irq_flags;

	if (!mdata)
		return;

	pr_debug("irq_domain_mask %lu\n", data->hwirq);

	if (data->hwirq < 32) {
		spin_lock_irqsave(&mdp_lock, irq_flags);
		mdata->mdss_util->disable_irq(&mdss_misc_hw);
		spin_unlock_irqrestore(&mdp_lock, irq_flags);
	} else if (data->hwirq < 64) {
		/* MDP_INTR is mapped as logical interrupt 32-63. */
		u32 irq = BIT(data->hwirq - 32);

		spin_lock_irqsave(&mdp_lock, irq_flags);
		if (!(mdata->mdp_irq_mask[0] & irq)) {
			pr_debug("%pS: MDP IRQ-%x is NOT set, mask=%x\n",
					__builtin_return_address(0),
					irq, mdata->mdp_irq_mask[0]);
		} else {
			pr_debug("%pS: MDP IRQ mask old=%x new=%x\n",
					__builtin_return_address(0),
					mdata->mdp_irq_mask[0], irq);
			mdata->mdp_irq_mask[0] &= ~irq;
			writel_relaxed(mdata->mdp_irq_mask[0], mdata->mdp_base +
				MDSS_MDP_REG_INTR_EN);
			if (!is_mdp_irq_enabled())
				mdata->mdss_util->disable_irq(&mdss_mdp_hw);
		}
		spin_unlock_irqrestore(&mdp_lock, irq_flags);
	}
}

static void mdss_irq_unmask(struct irq_data *data)
{
	struct mdss_data_type *mdata = irq_data_get_irq_chip_data(data);
	unsigned long irq_flags;

	if (!mdata)
		return;

	pr_debug("irq_domain_unmask %lu\n", data->hwirq);

	if (data->hwirq < 32) {
		spin_lock_irqsave(&mdp_lock, irq_flags);
		mdata->mdss_util->enable_irq(&mdss_misc_hw);
		spin_unlock_irqrestore(&mdp_lock, irq_flags);
	} else if (data->hwirq < 64) {
		/* MDP_INTR is mapped as logical interrupt 32-63. */
		u32 irq = BIT(data->hwirq - 32);

		spin_lock_irqsave(&mdp_lock, irq_flags);
		if (mdata->mdp_irq_mask[0] & irq) {
			pr_debug("%pS: MDP IRQ-0x%x is already set, mask=%x\n",
					__builtin_return_address(0),
					irq, mdata->mdp_irq_mask[0]);
		} else {
			pr_debug("%pS: MDP IRQ mask old=%x new=%x\n",
					__builtin_return_address(0),
					mdata->mdp_irq_mask[0], irq);
			mdata->mdp_irq_mask[0] |= irq;
			writel_relaxed(mdata->mdp_irq_mask[0], mdata->mdp_base +
				MDSS_MDP_REG_INTR_EN);
			mdata->mdss_util->enable_irq(&mdss_mdp_hw);
		}
		spin_unlock_irqrestore(&mdp_lock, irq_flags);
	}
}

static struct irq_chip mdss_irq_chip = {
	.name		= "mdss",
	.irq_mask	= mdss_irq_mask,
	.irq_unmask	= mdss_irq_unmask,
	/* avoid lazy disable by defining irq_disable explicitly */
	.irq_disable	= mdss_irq_mask,
};

static int mdss_irq_domain_map(struct irq_domain *d,
		unsigned int virq, irq_hw_number_t hw)
{
	struct mdss_data_type *mdata = d->host_data;
	/* check here if virq is a valid interrupt line */
	irq_set_chip_and_handler(virq, &mdss_irq_chip, handle_level_irq);
	irq_set_chip_data(virq, mdata);
	return 0;
}

static struct irq_domain_ops mdss_irq_domain_ops = {
	.map = mdss_irq_domain_map,
	.xlate = irq_domain_xlate_onecell,
};

static irqreturn_t mdss_irq_handler(int irq, void *ptr)
{
	struct mdss_data_type *mdata = ptr;
	u32 intr;

	if (!mdata)
		return IRQ_NONE;
	else if (!mdss_get_irq_enable_state(&mdss_mdp_hw))
		return IRQ_HANDLED;

	intr = MDSS_REG_READ(mdata, MDSS_REG_HW_INTR_STATUS);

	mdss_mdp_hw.irq_info->irq_buzy = true;

	if (intr & MDSS_INTR_MDP) {
		u32 mdp_irq_export;

		spin_lock(&mdp_lock);
		mdata->mdss_util->irq_dispatch(MDSS_HW_MDP, irq, ptr);
		spin_unlock(&mdp_lock);
		intr &= ~MDSS_INTR_MDP;

		/* export MDP_INTR as logical interrupts 32-63 */
		mdp_irq_export = mdata->mdp_irq_raw[0] &
					mdata->mdp_irq_export[0];
		while (mdp_irq_export) {
			irq_hw_number_t hwirq = fls(mdp_irq_export) - 1;

			generic_handle_irq(irq_find_mapping(
					mdata->irq_domain, hwirq + 32));
			mdp_irq_export &= ~(1 << hwirq);
		}
	}

	if (intr & MDSS_INTR_DSI0) {
		mdata->mdss_util->irq_dispatch(MDSS_HW_DSI0, irq, ptr);
		intr &= ~MDSS_INTR_DSI0;
	}

	if (intr & MDSS_INTR_DSI1) {
		mdata->mdss_util->irq_dispatch(MDSS_HW_DSI1, irq, ptr);
		intr &= ~MDSS_INTR_DSI1;
	}

	if (intr & MDSS_INTR_EDP) {
		mdata->mdss_util->irq_dispatch(MDSS_HW_EDP, irq, ptr);
		intr &= ~MDSS_INTR_EDP;
	}

	if (intr & MDSS_INTR_HDMI) {
		mdata->mdss_util->irq_dispatch(MDSS_HW_HDMI, irq, ptr);
		intr &= ~MDSS_INTR_HDMI;
	}

	/* route misc. interrupts to external drivers */
	while (intr) {
		irq_hw_number_t hwirq = fls(intr) - 1;

		generic_handle_irq(irq_find_mapping(
				mdata->irq_domain, hwirq));
		intr &= ~(1 << hwirq);
	}

	mdss_mdp_hw.irq_info->irq_buzy = false;

	return IRQ_HANDLED;
}

#ifdef CONFIG_QCOM_BUS_SCALING
static int mdss_mdp_bus_scale_register(struct mdss_data_type *mdata)
{
	struct msm_bus_scale_pdata *reg_bus_pdata;
	int i, rc;

	if (!mdata->bus_hdl) {
		rc = mdss_mdp_parse_dt_bus_scale(mdata->pdev);
		if (rc) {
			pr_err("Error in device tree : bus scale\n");
			return rc;
		}

		mdata->bus_hdl =
			msm_bus_scale_register_client(mdata->bus_scale_table);
		if (!mdata->bus_hdl) {
			pr_err("bus_client register failed\n");
			return -EINVAL;
		}

		pr_debug("register bus_hdl=%x\n", mdata->bus_hdl);
	}

	if (!mdata->reg_bus_scale_table) {
		reg_bus_pdata = &mdp_reg_bus_scale_table;
		for (i = 0; i < reg_bus_pdata->num_usecases; i++) {
			mdp_reg_bus_usecases[i].num_paths = 1;
			mdp_reg_bus_usecases[i].vectors =
				&mdp_reg_bus_vectors[i];
		}
		mdata->reg_bus_scale_table = reg_bus_pdata;
	}

	if (!mdata->reg_bus_hdl) {
		mdata->reg_bus_hdl =
			msm_bus_scale_register_client(
			      mdata->reg_bus_scale_table);
		if (!mdata->reg_bus_hdl)
			/* Continue without reg_bus scaling */
			pr_warn("reg_bus_client register failed\n");
		else
			pr_debug("register reg_bus_hdl=%x\n",
					mdata->reg_bus_hdl);
	}

	if (mdata->hw_rt_bus_scale_table && !mdata->hw_rt_bus_hdl) {
		mdata->hw_rt_bus_hdl =
			msm_bus_scale_register_client(
			      mdata->hw_rt_bus_scale_table);
		if (!mdata->hw_rt_bus_hdl)
			/* Continue without reg_bus scaling */
			pr_warn("hw_rt_bus client register failed\n");
		else
			pr_debug("register hw_rt_bus=%x\n",
					mdata->hw_rt_bus_hdl);
	}

	/*
	 * Following call will not result in actual vote rather update the
	 * current index and ab/ib value. When continuous splash is enabled,
	 * actual vote will happen when splash handoff is done.
	 */
	return mdss_bus_scale_set_quota(MDSS_MDP_RT, AB_QUOTA, IB_QUOTA);
}

static void mdss_mdp_bus_scale_unregister(struct mdss_data_type *mdata)
{
	pr_debug("unregister bus_hdl=%x\n", mdata->bus_hdl);

	if (mdata->bus_hdl)
		msm_bus_scale_unregister_client(mdata->bus_hdl);

	pr_debug("unregister reg_bus_hdl=%x\n", mdata->reg_bus_hdl);

	if (mdata->reg_bus_hdl) {
		msm_bus_scale_unregister_client(mdata->reg_bus_hdl);
		mdata->reg_bus_hdl = 0;
	}

	if (mdata->hw_rt_bus_hdl) {
		msm_bus_scale_unregister_client(mdata->hw_rt_bus_hdl);
		mdata->hw_rt_bus_hdl = 0;
	}
}

/*
 * Caller needs to hold mdata->bus_lock lock before calling this function.
 */
static int mdss_mdp_bus_scale_set_quota(u64 ab_quota_rt, u64 ab_quota_nrt,
		u64 ib_quota_rt, u64 ib_quota_nrt)
{
	int new_uc_idx;
	u64 ab_quota[MAX_AXI_PORT_COUNT] = {0, 0};
	u64 ib_quota[MAX_AXI_PORT_COUNT] = {0, 0};
	int rc;

	if (mdss_res->bus_hdl < 1) {
		pr_err("invalid bus handle %d\n", mdss_res->bus_hdl);
		return -EINVAL;
	}

	if (!ab_quota_rt && !ab_quota_nrt && !ib_quota_rt && !ib_quota_nrt)  {
		new_uc_idx = 0;
	} else {
		int i;
		struct msm_bus_vectors *vect = NULL;
		struct msm_bus_scale_pdata *bw_table =
			mdss_res->bus_scale_table;
		u32 nrt_axi_port_cnt = mdss_res->nrt_axi_port_cnt;
		u32 total_axi_port_cnt = mdss_res->axi_port_cnt;
		u32 rt_axi_port_cnt = total_axi_port_cnt - nrt_axi_port_cnt;
		int match_cnt = 0;

		if (!bw_table || !total_axi_port_cnt ||
		    total_axi_port_cnt > MAX_AXI_PORT_COUNT) {
			pr_err("invalid input\n");
			return -EINVAL;
		}

		if (mdss_res->bus_channels) {
			ib_quota_rt = div_u64(ib_quota_rt,
						mdss_res->bus_channels);
			ib_quota_nrt = div_u64(ib_quota_nrt,
						mdss_res->bus_channels);
		}

		if (mdss_res->has_fixed_qos_arbiter_enabled ||
			nrt_axi_port_cnt) {

			ab_quota_rt = div_u64(ab_quota_rt, rt_axi_port_cnt);
			ab_quota_nrt = div_u64(ab_quota_nrt, nrt_axi_port_cnt);

			for (i = 0; i < total_axi_port_cnt; i++) {
				if (i < rt_axi_port_cnt) {
					ab_quota[i] = ab_quota_rt;
					ib_quota[i] = ib_quota_rt;
				} else {
					ab_quota[i] = ab_quota_nrt;
					ib_quota[i] = ib_quota_nrt;
				}
			}
		} else {
			ab_quota[0] = div_u64(ab_quota_rt + ab_quota_nrt,
					total_axi_port_cnt);
			ib_quota[0] = ib_quota_rt + ib_quota_nrt;

			for (i = 1; i < total_axi_port_cnt; i++) {
				ab_quota[i] = ab_quota[0];
				ib_quota[i] = ib_quota[0];
			}
		}

		for (i = 0; i < total_axi_port_cnt; i++) {
			vect = &bw_table->usecase
				[mdss_res->curr_bw_uc_idx].vectors[i];
			/* avoid performing updates for small changes */
			if ((ab_quota[i] == vect->ab) &&
				(ib_quota[i] == vect->ib))
				match_cnt++;
		}

		if (match_cnt == total_axi_port_cnt) {
			pr_debug("skip BW vote\n");
			return 0;
		}

		new_uc_idx = (mdss_res->curr_bw_uc_idx %
			(bw_table->num_usecases - 1)) + 1;

		for (i = 0; i < total_axi_port_cnt; i++) {
			vect = &bw_table->usecase[new_uc_idx].vectors[i];
			vect->ab = ab_quota[i];
			vect->ib = ib_quota[i];

			pr_debug("uc_idx=%d %s path idx=%d ab=%llu ib=%llu\n",
				new_uc_idx, (i < rt_axi_port_cnt) ? "rt" : "nrt"
				, i, vect->ab, vect->ib);
		}
	}
	mdss_res->curr_bw_uc_idx = new_uc_idx;
	mdss_res->ao_bw_uc_idx = new_uc_idx;

	if ((mdss_res->bus_ref_cnt == 0) && mdss_res->curr_bw_uc_idx) {
		rc = 0;
	} else { /* vote BW if bus_bw_cnt > 0 or uc_idx is zero */
		ATRACE_BEGIN("msm_bus_scale_req");
		rc = msm_bus_scale_client_update_request(mdss_res->bus_hdl,
			new_uc_idx);
		ATRACE_END("msm_bus_scale_req");
	}
	return rc;
}

struct reg_bus_client *mdss_reg_bus_vote_client_create(char *client_name)
{
	struct reg_bus_client *client;
	static u32 id;

	if (client_name == NULL) {
		pr_err("client name is null\n");
		return ERR_PTR(-EINVAL);
	}

	client = kzalloc(sizeof(struct reg_bus_client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&mdss_res->reg_bus_lock);
	strlcpy(client->name, client_name, MAX_CLIENT_NAME_LEN);
	client->usecase_ndx = VOTE_INDEX_DISABLE;
	client->id = id;
	pr_debug("bus vote client %s created:%pK id :%d\n", client_name,
		client, id);
	id++;
	list_add(&client->list, &mdss_res->reg_bus_clist);
	mutex_unlock(&mdss_res->reg_bus_lock);

	return client;
}

void mdss_reg_bus_vote_client_destroy(struct reg_bus_client *client)
{
	if (!client) {
		pr_err("reg bus vote: invalid client handle\n");
	} else {
		pr_debug("bus vote client %s destroyed:%pK id:%u\n",
			client->name, client, client->id);
		mutex_lock(&mdss_res->reg_bus_lock);
		list_del_init(&client->list);
		mutex_unlock(&mdss_res->reg_bus_lock);
		kfree(client);
	}
}

int mdss_update_reg_bus_vote(struct reg_bus_client *bus_client, u32 usecase_ndx)
{
	int ret = 0;
	bool changed = false;
	u32 max_usecase_ndx = VOTE_INDEX_DISABLE;
	struct reg_bus_client *client, *temp_client;

	if (!mdss_res || !mdss_res->reg_bus_hdl || !bus_client)
		return 0;

	mutex_lock(&mdss_res->reg_bus_lock);
	bus_client->usecase_ndx = usecase_ndx;
	list_for_each_entry_safe(client, temp_client, &mdss_res->reg_bus_clist,
		list) {

		if (client->usecase_ndx < VOTE_INDEX_MAX &&
		    client->usecase_ndx > max_usecase_ndx)
			max_usecase_ndx = client->usecase_ndx;
	}

	if (mdss_res->reg_bus_usecase_ndx != max_usecase_ndx) {
		changed = true;
		mdss_res->reg_bus_usecase_ndx = max_usecase_ndx;
	}

	pr_debug("%pS: changed=%d current idx=%d request client %s id:%u idx:%d\n",
		__builtin_return_address(0), changed, max_usecase_ndx,
		bus_client->name, bus_client->id, usecase_ndx);
	MDSS_XLOG(changed, max_usecase_ndx, bus_client->id, usecase_ndx);
	if (changed)
		ret = msm_bus_scale_client_update_request(mdss_res->reg_bus_hdl,
			max_usecase_ndx);

	mutex_unlock(&mdss_res->reg_bus_lock);
	return ret;
}

int mdss_bus_scale_set_quota(int client, u64 ab_quota, u64 ib_quota)
{
	int rc = 0;
	int i;
	u64 total_ab_rt = 0, total_ib_rt = 0;
	u64 total_ab_nrt = 0, total_ib_nrt = 0;

	mutex_lock(&mdss_res->bus_lock);

	mdss_res->ab[client] = ab_quota;
	mdss_res->ib[client] = ib_quota;
	trace_mdp_perf_update_bus(client, ab_quota, ib_quota);

	for (i = 0; i < MDSS_MAX_BUS_CLIENTS; i++) {
		if (i == MDSS_MDP_NRT) {
			total_ab_nrt = mdss_res->ab[i];
			total_ib_nrt = mdss_res->ib[i];
		} else {
			total_ab_rt += mdss_res->ab[i];
			total_ib_rt = max(total_ib_rt, mdss_res->ib[i]);
		}
	}

	rc = mdss_mdp_bus_scale_set_quota(total_ab_rt, total_ab_nrt,
			total_ib_rt, total_ib_nrt);

	mutex_unlock(&mdss_res->bus_lock);

	return rc;
}
#else
static int mdss_mdp_bus_scale_register(struct mdss_data_type *mdata)
{
	return 0;
}

static void mdss_mdp_bus_scale_unregister(struct mdss_data_type *mdata)
{
}

int mdss_bus_scale_set_quota(int client, u64 ab_quota, u64 ib_quota)
{
	pr_debug("No bus scaling! client=%d ab=%llu ib=%llu\n",
			client, ab_quota, ib_quota);

	return 0;
}

struct reg_bus_client *mdss_reg_bus_vote_client_create(char *client_name)
{
	return NULL;
}

void mdss_reg_bus_vote_client_destroy(struct reg_bus_client *client)
{
}

int mdss_update_reg_bus_vote(struct reg_bus_client *bus_client, u32 usecase_ndx)
{
	pr_debug("%pS: No reg scaling! usecase=%u\n",
			__builtin_return_address(0), usecase_ndx);

	return 0;
}
#endif

void mdss_mdp_vbif_reg_lock(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	mutex_lock(&mdata->reg_lock);
}

void mdss_mdp_vbif_reg_unlock(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	mutex_unlock(&mdata->reg_lock);
}

bool mdss_mdp_handoff_pending(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	return mdata->handoff_pending;
}

static int mdss_mdp_intr2index(u32 intr_type, u32 intf_num)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mdp_irq_map); i++) {
		if (intr_type == mdp_irq_map[i].intr_type &&
			intf_num == mdp_irq_map[i].intf_num)
			return i;
	}
	return -EINVAL;
}

u32 mdss_mdp_get_irq_mask(u32 intr_type, u32 intf_num)
{
	int idx = mdss_mdp_intr2index(intr_type, intf_num);

	return (idx < 0) ? 0 : mdp_irq_map[idx].irq_mask;
}

void mdss_mdp_enable_hw_irq(struct mdss_data_type *mdata)
{
	mdata->mdss_util->enable_irq(&mdss_mdp_hw);
}

void mdss_mdp_disable_hw_irq(struct mdss_data_type *mdata)
{
	if (!is_mdp_irq_enabled())
		mdata->mdss_util->disable_irq(&mdss_mdp_hw);
}

/* function assumes that mdp is clocked to access hw registers */
void mdss_mdp_irq_clear(struct mdss_data_type *mdata,
		u32 intr_type, u32 intf_num)
{
	unsigned long irq_flags;
	int irq_idx;
	struct mdss_mdp_intr_reg reg;
	struct mdss_mdp_irq irq;

	irq_idx = mdss_mdp_intr2index(intr_type, intf_num);
	if (irq_idx < 0) {
		pr_err("invalid irq request\n");
		return;
	}

	irq = mdp_irq_map[irq_idx];
	reg = mdp_intr_reg[irq.reg_idx];

	pr_debug("clearing mdp irq mask=%x\n", irq.irq_mask);
	spin_lock_irqsave(&mdp_lock, irq_flags);
	writel_relaxed(irq.irq_mask, mdata->mdp_base + reg.clr_off);
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

int mdss_mdp_irq_enable(u32 intr_type, u32 intf_num)
{
	int irq_idx = 0;
	unsigned long irq_flags;
	int ret = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_intr_reg reg;
	struct mdss_mdp_irq irq;

	irq_idx = mdss_mdp_intr2index(intr_type, intf_num);
	if (irq_idx < 0) {
		pr_err("invalid irq request\n");
		return -EINVAL;
	}

	irq = mdp_irq_map[irq_idx];
	reg = mdp_intr_reg[irq.reg_idx];

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (mdata->mdp_irq_mask[irq.reg_idx] & irq.irq_mask) {
		pr_warn("MDSS MDP IRQ-0x%x is already set, mask=%x\n",
				irq.irq_mask, mdata->mdp_irq_mask[irq.reg_idx]);
		ret = -EBUSY;
	} else {
		pr_debug("MDP IRQ mask old=%x new=%x\n",
				mdata->mdp_irq_mask[irq.reg_idx], irq.irq_mask);
		mdata->mdp_irq_mask[irq.reg_idx] |= irq.irq_mask;
		writel_relaxed(irq.irq_mask, mdata->mdp_base + reg.clr_off);
		writel_relaxed(mdata->mdp_irq_mask[irq.reg_idx],
				mdata->mdp_base + reg.en_off);
		mdata->mdss_util->enable_irq(&mdss_mdp_hw);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);

	return ret;
}
int mdss_mdp_hist_irq_enable(u32 irq)
{
	int ret = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (mdata->mdp_hist_irq_mask & irq) {
		pr_warn("MDSS MDP Hist IRQ-0x%x is already set, mask=%x\n",
				irq, mdata->mdp_hist_irq_mask);
		ret = -EBUSY;
	} else {
		pr_debug("mask old=%x new=%x\n",
				mdata->mdp_hist_irq_mask, irq);
		mdata->mdp_hist_irq_mask |= irq;
		writel_relaxed(irq, mdata->mdp_base +
			MDSS_MDP_REG_HIST_INTR_CLEAR);
		writel_relaxed(mdata->mdp_hist_irq_mask, mdata->mdp_base +
			MDSS_MDP_REG_HIST_INTR_EN);
		mdata->mdss_util->enable_irq(&mdss_mdp_hw);
	}

	return ret;
}

void mdss_mdp_irq_disable(u32 intr_type, u32 intf_num)
{
	int irq_idx;
	unsigned long irq_flags;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_intr_reg reg;
	struct mdss_mdp_irq irq;

	irq_idx = mdss_mdp_intr2index(intr_type, intf_num);
	if (irq_idx < 0) {
		pr_err("invalid irq request\n");
		return;
	}

	irq =  mdp_irq_map[irq_idx];
	reg = mdp_intr_reg[irq.reg_idx];

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (!(mdata->mdp_irq_mask[irq.reg_idx] & irq.irq_mask)) {
		pr_warn("MDSS MDP IRQ-%x is NOT set, mask=%x\n",
				irq.irq_mask, mdata->mdp_irq_mask[irq.reg_idx]);
	} else {
		mdata->mdp_irq_mask[irq.reg_idx] &= ~irq.irq_mask;
		writel_relaxed(mdata->mdp_irq_mask[irq.reg_idx],
				mdata->mdp_base + reg.en_off);
		if (!is_mdp_irq_enabled())
			mdata->mdss_util->disable_irq(&mdss_mdp_hw);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

/* This function is used to check and clear the status of MDP interrupts */
void mdss_mdp_intr_check_and_clear(u32 intr_type, u32 intf_num)
{
	u32 status;
	int irq_idx;
	unsigned long irq_flags;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_intr_reg reg;
	struct mdss_mdp_irq irq;

	irq_idx = mdss_mdp_intr2index(intr_type, intf_num);
	if (irq_idx < 0) {
		pr_err("invalid irq request\n");
		return;
	}

	irq =  mdp_irq_map[irq_idx];
	reg = mdp_intr_reg[irq.reg_idx];

	spin_lock_irqsave(&mdp_lock, irq_flags);
	status = irq.irq_mask & readl_relaxed(mdata->mdp_base +
			reg.status_off);
	if (status) {
		pr_debug("clearing irq: intr_type:%d, intf_num:%d\n",
				intr_type, intf_num);
		writel_relaxed(irq.irq_mask, mdata->mdp_base + reg.clr_off);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

void mdss_mdp_hist_irq_disable(u32 irq)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!(mdata->mdp_hist_irq_mask & irq)) {
		pr_warn("MDSS MDP IRQ-%x is NOT set, mask=%x\n",
				irq, mdata->mdp_hist_irq_mask);
	} else {
		mdata->mdp_hist_irq_mask &= ~irq;
		writel_relaxed(mdata->mdp_hist_irq_mask, mdata->mdp_base +
			MDSS_MDP_REG_HIST_INTR_EN);
		if (!is_mdp_irq_enabled())
			mdata->mdss_util->disable_irq(&mdss_mdp_hw);
	}
}

/**
 * mdss_mdp_irq_disable_nosync() - disable mdp irq
 * @intr_type:	mdp interface type
 * @intf_num:	mdp interface num
 *
 * This fucntion is called from interrupt context
 * mdp_lock is already held at up stream (mdss_irq_handler)
 * therefore spin_lock(&mdp_lock) is not allowed here
 *
*/
void mdss_mdp_irq_disable_nosync(u32 intr_type, u32 intf_num)
{
	int irq_idx;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_intr_reg reg;
	struct mdss_mdp_irq irq;

	irq_idx = mdss_mdp_intr2index(intr_type, intf_num);
	if (irq_idx < 0) {
		pr_err("invalid irq request\n");
		return;
	}

	irq = mdp_irq_map[irq_idx];
	reg = mdp_intr_reg[irq.reg_idx];

	if (!(mdata->mdp_irq_mask[irq.reg_idx] & irq.irq_mask)) {
		pr_warn("MDSS MDP IRQ-%x is NOT set, mask=%x\n",
				irq.irq_mask, mdata->mdp_irq_mask[irq.reg_idx]);
	} else {
		mdata->mdp_irq_mask[irq.reg_idx] &= ~irq.irq_mask;
		writel_relaxed(mdata->mdp_irq_mask[irq.reg_idx],
				mdata->mdp_base + reg.en_off);
		if (!is_mdp_irq_enabled())
			mdata->mdss_util->disable_irq_nosync(&mdss_mdp_hw);
	}
}

int mdss_mdp_set_intr_callback(u32 intr_type, u32 intf_num,
		       void (*fnc_ptr)(void *), void *arg)
{
	unsigned long flags;
	int index;

	index = mdss_mdp_intr2index(intr_type, intf_num);
	if (index < 0) {
		pr_warn("invalid intr type=%u intf_numf_num=%u\n",
				intr_type, intf_num);
		return -EINVAL;
	}

	spin_lock_irqsave(&mdss_mdp_intr_lock, flags);
	WARN(mdp_intr_cb[index].func && fnc_ptr,
			"replacing current intr callback for ndx=%d\n", index);
	mdp_intr_cb[index].func = fnc_ptr;
	mdp_intr_cb[index].arg = arg;
	spin_unlock_irqrestore(&mdss_mdp_intr_lock, flags);

	return 0;
}

int mdss_mdp_set_intr_callback_nosync(u32 intr_type, u32 intf_num,
		       void	  (*fnc_ptr)(void *), void *arg)
{
	int index;

	index = mdss_mdp_intr2index(intr_type, intf_num);
	if (index < 0) {
		pr_warn("invalid intr Typee=%u intf_num=%u\n",
				intr_type, intf_num);
		return -EINVAL;
	}

	WARN(mdp_intr_cb[index].func && fnc_ptr,
			"replacing current intr callbackack for ndx=%d\n",
			index);
	mdp_intr_cb[index].func = fnc_ptr;
	mdp_intr_cb[index].arg = arg;

	return 0;
}

static inline void mdss_mdp_intr_done(int index)
{
	void (*fnc)(void *);
	void *arg;

	spin_lock(&mdss_mdp_intr_lock);
	fnc = mdp_intr_cb[index].func;
	arg = mdp_intr_cb[index].arg;
	spin_unlock(&mdss_mdp_intr_lock);
	if (fnc)
		fnc(arg);
}

irqreturn_t mdss_mdp_isr(int irq, void *ptr)
{
	struct mdss_data_type *mdata = ptr;
	u32 isr, mask, hist_isr, hist_mask;
	int i, j;

	/* Bypass if clock is not enabled and no export irq is requested. */
	if (!mdata->clk_ena &&
		!(mdata->mdp_irq_mask[0] & mdata->mdp_irq_export[0]))
		return IRQ_HANDLED;

	for (i = 0; i < ARRAY_SIZE(mdp_intr_reg); i++) {
		struct mdss_mdp_intr_reg reg = mdp_intr_reg[i];

		isr = readl_relaxed(mdata->mdp_base + reg.status_off);
		if (isr == 0)
			continue;

		mask = readl_relaxed(mdata->mdp_base + reg.en_off);

		/* Process only non-export irq */
		mdata->mdp_irq_raw[i] = isr;
		isr = isr & ~mdata->mdp_irq_export[i];

		writel_relaxed(isr, mdata->mdp_base + reg.clr_off);

		pr_debug("%s: reg:%d isr=%x mask=%x\n",
				__func__, i+1, isr, mask);

		isr &= mask;
		if (isr == 0)
			continue;

		for (j = 0; j < ARRAY_SIZE(mdp_irq_map); j++)
			if (mdp_irq_map[j].reg_idx == i &&
					(isr & mdp_irq_map[j].irq_mask))
				mdss_mdp_intr_done(j);
		if (!i) {
			if (isr & MDSS_MDP_INTR_PING_PONG_0_DONE)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_DSI0, false);

			if (isr & MDSS_MDP_INTR_PING_PONG_1_DONE)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_DSI1, false);

			if (isr & MDSS_MDP_INTR_INTF_0_VSYNC)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_EDP, true);

			if (isr & MDSS_MDP_INTR_INTF_1_VSYNC)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_DSI0, true);

			if (isr & MDSS_MDP_INTR_INTF_2_VSYNC)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_DSI1, true);

			if (isr & MDSS_MDP_INTR_INTF_3_VSYNC)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_HDMI, true);

			if (isr & MDSS_MDP_INTR_WB_0_DONE)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_MDP, true);

			if (isr & MDSS_MDP_INTR_WB_1_DONE)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_MDP, true);

			if (isr &  MDSS_MDP_INTR_WB_2_DONE)
				mdss_misr_crc_collect(mdata, DISPLAY_MISR_MDP, true);
		}
	}

	hist_isr = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_REG_HIST_INTR_STATUS);
	if (hist_isr != 0) {
		hist_mask = readl_relaxed(mdata->mdp_base +
				MDSS_MDP_REG_HIST_INTR_EN);
		writel_relaxed(hist_isr, mdata->mdp_base +
				MDSS_MDP_REG_HIST_INTR_CLEAR);
		hist_isr &= hist_mask;
		if (hist_isr != 0)
			mdss_mdp_hist_intr_done(hist_isr);
	}

	mdss_mdp_video_isr(mdata->video_intf, mdata->nintf);
	return IRQ_HANDLED;
}

static int mdss_mdp_clk_update(u32 clk_idx, u32 enable)
{
	int ret = -ENODEV;
	struct clk *clk = mdss_mdp_get_clk(clk_idx);
	struct mdss_data_type *mdata = mdss_res;

	if (clk) {
		pr_debug("clk=%d en=%d\n", clk_idx, enable);
		if (enable) {
			if (clk_idx == MDSS_CLK_MDP_VSYNC)
				clk_set_rate(clk, 19200000);
			if (mdss_has_quirk(mdata, MDSS_QUIRK_MDP_CLK_SET_RATE)
					&& (clk_idx == MDSS_CLK_MDP_CORE)) {

				if (WARN_ON(!mdata->mdp_clk_rate)) {
					/*
					 * rate should have been set in probe
					 * or during clk scaling; but if this
					 * is not the case, set max clk rate.
					 */
					pr_warn("set max mdp clk rate:%u\n",
						mdata->max_mdp_clk_rate);
					mdss_mdp_set_clk_rate(
						mdata->max_mdp_clk_rate, true);
				} else {
					clk_set_rate(clk, mdata->mdp_clk_rate);
				}
			}

			ret = clk_prepare_enable(clk);
		} else {
			clk_disable_unprepare(clk);
			ret = 0;
		}
	}
	return ret;
}

int mdss_mdp_vsync_clk_enable(int enable, bool locked)
{
	int ret = 0;
	pr_debug("clk enable=%d\n", enable);

	if (!locked)
		mutex_lock(&mdp_clk_lock);

	if (mdss_res->vsync_ena != enable) {
		mdss_res->vsync_ena = enable;
		ret = mdss_mdp_clk_update(MDSS_CLK_MDP_VSYNC, enable);
	}

	if (!locked)
		mutex_unlock(&mdp_clk_lock);
	return ret;
}

void mdss_mdp_set_clk_rate(unsigned long rate, bool locked)
{
	struct mdss_data_type *mdata = mdss_res;
	unsigned long clk_rate;
	struct clk *clk = mdss_mdp_get_clk(MDSS_CLK_MDP_CORE);
	unsigned long min_clk_rate;

	min_clk_rate = max(rate, mdata->perf_tune.min_mdp_clk);

	if (clk) {

		if (!locked)
			mutex_lock(&mdp_clk_lock);
		if (min_clk_rate < mdata->max_mdp_clk_rate)
			clk_rate = clk_round_rate(clk, min_clk_rate);
		else
			clk_rate = mdata->max_mdp_clk_rate;
		if (IS_ERR_VALUE(clk_rate)) {
			pr_err("unable to round rate err=%ld\n", clk_rate);
		} else if (clk_rate != clk_get_rate(clk)) {

			mdata->mdp_clk_rate = clk_rate;
			if (IS_ERR_VALUE(clk_set_rate(clk, clk_rate)))
				pr_err("clk_set_rate failed\n");
			else
				pr_debug("mdp clk rate=%lu\n", clk_rate);
		}
		if (!locked)
			mutex_unlock(&mdp_clk_lock);
	} else {
		pr_err("mdp src clk not setup properly\n");
	}
}

unsigned long mdss_mdp_get_clk_rate(u32 clk_idx, bool locked)
{
	unsigned long clk_rate = 0;
	struct clk *clk = mdss_mdp_get_clk(clk_idx);

	if (clk) {
		if (!locked)
			mutex_lock(&mdp_clk_lock);

		clk_rate = clk_get_rate(clk);

		if (!locked)
			mutex_unlock(&mdp_clk_lock);
	}

	return clk_rate;
}

/**
 * mdss_bus_rt_bw_vote() -- place bus bandwidth request
 * @enable: value of enable or disable
 *
 * hw_rt table has two entries, 0 and Min Vote (1Mhz)
 * while attaching SMMU and for few TZ operations which
 * happen at very early stage, we will request Min Vote
 * thru this handle.
 *
 */
static int mdss_bus_rt_bw_vote(bool enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int rc = 0;
	bool changed = false;

	if (!mdata->hw_rt_bus_hdl || mdata->handoff_pending)
		return 0;

	if (enable) {
		if (mdata->hw_rt_bus_ref_cnt == 0)
			changed = true;
		mdata->hw_rt_bus_ref_cnt++;
	} else {
		if (mdata->hw_rt_bus_ref_cnt != 0) {
			mdata->hw_rt_bus_ref_cnt--;
			if (mdata->hw_rt_bus_ref_cnt == 0)
				changed = true;
		} else {
			pr_warn("%s: bus bw votes are not balanced\n",
				__func__);
		}
	}

	pr_debug("%pS: task:%s bw_cnt=%d changed=%d enable=%d\n",
		__builtin_return_address(0), current->group_leader->comm,
		mdata->hw_rt_bus_ref_cnt, changed, enable);

	if (changed) {
		rc = msm_bus_scale_client_update_request(mdata->hw_rt_bus_hdl,
							 enable ? 1 : 0);
		if (rc)
			pr_err("%s: Bus bandwidth vote failed\n", __func__);
	}

	return rc;
}

/**
 * __mdss_mdp_reg_access_clk_enable - Enable minimum MDSS clocks required
 * for register access
 */
static inline void __mdss_mdp_reg_access_clk_enable(
		struct mdss_data_type *mdata, bool enable)
{
	if (enable) {
		mdss_update_reg_bus_vote(mdata->reg_bus_clt,
				VOTE_INDEX_LOW);
		mdss_bus_rt_bw_vote(true);
		mdss_mdp_clk_update(MDSS_CLK_MNOC_AHB, 1);
		mdss_mdp_clk_update(MDSS_CLK_AHB, 1);
		mdss_mdp_clk_update(MDSS_CLK_AXI, 1);
		mdss_mdp_clk_update(MDSS_CLK_MDP_CORE, 1);
	} else {
		mdss_mdp_clk_update(MDSS_CLK_MDP_CORE, 0);
		mdss_mdp_clk_update(MDSS_CLK_AXI, 0);
		mdss_mdp_clk_update(MDSS_CLK_AHB, 0);
		mdss_mdp_clk_update(MDSS_CLK_MNOC_AHB, 0);
		mdss_bus_rt_bw_vote(false);
		mdss_update_reg_bus_vote(mdata->reg_bus_clt,
				VOTE_INDEX_DISABLE);
	}
}

/*
 * __mdss_mdp_clk_control - Overall MDSS clock control for power on/off
 */
static void __mdss_mdp_clk_control(struct mdss_data_type *mdata, bool enable)
{
	int rc = 0;
	unsigned long flags;

	if (enable) {
		pm_runtime_get_sync(&mdata->pdev->dev);

		mdss_update_reg_bus_vote(mdata->reg_bus_clt,
			VOTE_INDEX_LOW);

		rc = mdss_iommu_ctrl(1);
		if (IS_ERR_VALUE(rc))
			pr_err("IOMMU attach failed\n");

		/* Active+Sleep */
		msm_bus_scale_client_update_context(mdata->bus_hdl,
			false, mdata->curr_bw_uc_idx);

		spin_lock_irqsave(&mdp_lock, flags);
		mdata->clk_ena = enable;
		spin_unlock_irqrestore(&mdp_lock, flags);

		mdss_mdp_clk_update(MDSS_CLK_MNOC_AHB, 1);
		mdss_mdp_clk_update(MDSS_CLK_AHB, 1);
		mdss_mdp_clk_update(MDSS_CLK_AXI, 1);
		mdss_mdp_clk_update(MDSS_CLK_MDP_CORE, 1);
		mdss_mdp_clk_update(MDSS_CLK_MDP_LUT, 1);
		if (mdata->vsync_ena)
			mdss_mdp_clk_update(MDSS_CLK_MDP_VSYNC, 1);
	} else {
		spin_lock_irqsave(&mdp_lock, flags);
		mdata->clk_ena = enable;
		spin_unlock_irqrestore(&mdp_lock, flags);

		if (mdata->vsync_ena)
			mdss_mdp_clk_update(MDSS_CLK_MDP_VSYNC, 0);

		mdss_mdp_clk_update(MDSS_CLK_MDP_LUT, 0);
		mdss_mdp_clk_update(MDSS_CLK_MDP_CORE, 0);
		mdss_mdp_clk_update(MDSS_CLK_AXI, 0);
		mdss_mdp_clk_update(MDSS_CLK_AHB, 0);
		mdss_mdp_clk_update(MDSS_CLK_MNOC_AHB, 0);

		/* release iommu control */
		mdss_iommu_ctrl(0);

		/* Active-Only */
		msm_bus_scale_client_update_context(mdata->bus_hdl,
			true, mdata->ao_bw_uc_idx);

		mdss_update_reg_bus_vote(mdata->reg_bus_clt,
			VOTE_INDEX_DISABLE);

		pm_runtime_mark_last_busy(&mdata->pdev->dev);
		pm_runtime_put_autosuspend(&mdata->pdev->dev);
	}
}

int __mdss_mdp_vbif_halt(struct mdss_data_type *mdata, bool is_nrt)
{
	int rc = 0;
	void __iomem *base;
	u32 halt_ack_mask = BIT(0), status;

	/* if not real time vbif */
	if (is_nrt)
		base = mdata->vbif_nrt_io.base;
	else
		base = mdata->vbif_io.base;

	if (!base) {
		/* some targets might not have a nrt port */
		goto vbif_done;
	}

	/* force vbif clock on */
	MDSS_VBIF_WRITE(mdata, MMSS_VBIF_CLKON, 1, is_nrt);

	/* request halt */
	MDSS_VBIF_WRITE(mdata, MMSS_VBIF_AXI_HALT_CTRL0, 1, is_nrt);

	rc = readl_poll_timeout(base +
			MMSS_VBIF_AXI_HALT_CTRL1, status, (status &
				halt_ack_mask),
			1000, AXI_HALT_TIMEOUT_US);
	if (rc == -ETIMEDOUT) {
		pr_err("VBIF axi is not halting. TIMEDOUT.\n");
		goto vbif_done;
	}

	pr_debug("VBIF axi is halted\n");

vbif_done:
	return rc;
}

/**
 * mdss_mdp_vbif_axi_halt() - Halt MDSS AXI ports
 * @mdata: pointer to the global mdss data structure.
 *
 * This function can be called during deep suspend, display off or for
 * debugging purposes. On success it should be assumed that AXI ports connected
 * to RT VBIF are in idle state and would not fetch any more data.
 */
static void mdss_mdp_vbif_axi_halt(struct mdss_data_type *mdata)
{
	__mdss_mdp_reg_access_clk_enable(mdata, true);

	/* real time ports */
	__mdss_mdp_vbif_halt(mdata, false);
	/* non-real time ports */
	__mdss_mdp_vbif_halt(mdata, true);

	__mdss_mdp_reg_access_clk_enable(mdata, false);
}

int mdss_iommu_ctrl(int enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int rc = 0;

	mutex_lock(&mdp_iommu_ref_cnt_lock);
	pr_debug("%pS: enable:%d ref_cnt:%d attach:%d hoff:%d\n",
		__builtin_return_address(0), enable, mdata->iommu_ref_cnt,
		mdata->iommu_attached, mdata->handoff_pending);

	if (enable) {
		/*
		 * delay iommu attach until continous splash screen has
		 * finished handoff, as it may still be working with phys addr
		 */
		if (!mdata->iommu_attached && !mdata->handoff_pending) {
			mdss_bus_rt_bw_vote(true);
			rc = mdss_smmu_attach(mdata);
		}
		mdata->iommu_ref_cnt++;
	} else {
		if (mdata->iommu_ref_cnt) {
			mdata->iommu_ref_cnt--;
			if (mdata->iommu_ref_cnt == 0) {
				rc = mdss_smmu_detach(mdata);
				mdss_bus_rt_bw_vote(false);
			}
		} else {
			pr_err("unbalanced iommu ref\n");
		}
	}
	mutex_unlock(&mdp_iommu_ref_cnt_lock);

	if (IS_ERR_VALUE(rc))
		return rc;
	else
		return mdata->iommu_ref_cnt;
}

#define MEM_RETAIN_ON 1
#define MEM_RETAIN_OFF 0
#define PERIPH_RETAIN_ON 1
#define PERIPH_RETAIN_OFF 0

static void mdss_mdp_memory_retention_ctrl(bool mem_ctrl, bool periph_ctrl)
{
	struct clk *mdss_mdp_clk = NULL;
	struct clk *mdp_vote_clk = mdss_mdp_get_clk(MDSS_CLK_MDP_CORE);
	struct clk *mdss_mdp_lut_clk = NULL;
	struct clk *mdp_lut_vote_clk = mdss_mdp_get_clk(MDSS_CLK_MDP_LUT);
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (mdp_vote_clk) {
		if (test_bit(MDSS_CAPS_MDP_VOTE_CLK_NOT_SUPPORTED,
				mdata->mdss_caps_map)) {
			mdss_mdp_clk = mdp_vote_clk;
			mdss_mdp_lut_clk = mdp_lut_vote_clk;
		} else {
			mdss_mdp_clk = clk_get_parent(mdp_vote_clk);
			mdss_mdp_lut_clk = clk_get_parent(mdp_lut_vote_clk);
		}
	}

	__mdss_mdp_reg_access_clk_enable(mdata, true);
	if (mdss_mdp_clk) {
		if (mem_ctrl)
			clk_set_flags(mdss_mdp_clk, CLKFLAG_RETAIN_MEM);
		else
			clk_set_flags(mdss_mdp_clk, CLKFLAG_NORETAIN_MEM);

		if (periph_ctrl) {
			clk_set_flags(mdss_mdp_clk, CLKFLAG_RETAIN_PERIPH);
			clk_set_flags(mdss_mdp_clk, CLKFLAG_PERIPH_OFF_CLEAR);
		} else {
			clk_set_flags(mdss_mdp_clk, CLKFLAG_PERIPH_OFF_SET);
			clk_set_flags(mdss_mdp_clk, CLKFLAG_NORETAIN_PERIPH);
		}
	}

	if (mdss_mdp_lut_clk) {
		if (mem_ctrl)
			clk_set_flags(mdss_mdp_lut_clk, CLKFLAG_RETAIN_MEM);
		else
			clk_set_flags(mdss_mdp_lut_clk, CLKFLAG_NORETAIN_MEM);

		if (periph_ctrl) {
			clk_set_flags(mdss_mdp_lut_clk, CLKFLAG_RETAIN_PERIPH);
			clk_set_flags(mdss_mdp_lut_clk,
				CLKFLAG_PERIPH_OFF_CLEAR);
		} else {
			clk_set_flags(mdss_mdp_lut_clk, CLKFLAG_PERIPH_OFF_SET);
			clk_set_flags(mdss_mdp_lut_clk,
				CLKFLAG_NORETAIN_PERIPH);
		}
	}
	__mdss_mdp_reg_access_clk_enable(mdata, false);
}

/**
 * mdss_mdp_idle_pc_restore() - Restore MDSS settings when exiting idle pc
 *
 * MDSS GDSC can be voted off during idle-screen usecase for MIPI DSI command
 * mode displays, referred to as MDSS idle power collapse. Upon subsequent
 * frame update, MDSS GDSC needs to turned back on and hw state needs to be
 * restored.
 */
static int mdss_mdp_idle_pc_restore(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int rc = 0;

	mutex_lock(&mdp_fs_idle_pc_lock);
	if (!mdata->idle_pc) {
		pr_debug("no idle pc, no need to restore\n");
		goto end;
	}

	pr_debug("called from %pS\n", __builtin_return_address(0));
	rc = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(rc)) {
		pr_err("mdss iommu attach failed rc=%d\n", rc);
		goto end;
	}
	mdss_hw_init(mdata);
	mdss_iommu_ctrl(0);

	mdss_mdp_ctl_restore(true);
	mdata->idle_pc = false;

end:
	if (mdata->mem_retain) {
		/**
		 * sleep 10 microseconds to make sure AD auto-reinitialization
		 * is done
		 */
		udelay(10);
		mdss_mdp_memory_retention_ctrl(MEM_RETAIN_ON,
			PERIPH_RETAIN_ON);
		mdata->mem_retain = false;
	}

	mutex_unlock(&mdp_fs_idle_pc_lock);
	return rc;
}

/**
 * mdss_mdp_retention_init() - initialize retention setting
 * @mdata: pointer to the global mdss data structure.
 */
static int mdss_mdp_retention_init(struct mdss_data_type *mdata)
{
	struct clk *mdss_axi_clk = mdss_mdp_get_clk(MDSS_CLK_AXI);
	int rc;

	if (!mdss_axi_clk) {
		pr_err("failed to get AXI clock\n");
		return -EINVAL;
	}

	rc = clk_set_flags(mdss_axi_clk, CLKFLAG_NORETAIN_MEM);
	if (rc) {
		pr_err("failed to set AXI no memory retention %d\n", rc);
		return rc;
	}

	rc = clk_set_flags(mdss_axi_clk, CLKFLAG_NORETAIN_PERIPH);
	if (rc) {
		pr_err("failed to set AXI no periphery retention %d\n", rc);
		return rc;
	}

	return rc;
}

/**
 * mdss_bus_bandwidth_ctrl() -- place bus bandwidth request
 * @enable:	value of enable or disable
 *
 * Function place bus bandwidth request to allocate saved bandwidth
 * if enabled or free bus bandwidth allocation if disabled.
 * Bus bandwidth is required by mdp.For dsi, it only requires to send
 * dcs coammnd. It returns error if bandwidth request fails.
 */
void mdss_bus_bandwidth_ctrl(int enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int changed = 0;

	mutex_lock(&mdata->bus_lock);
	if (enable) {
		if (mdata->bus_ref_cnt == 0)
			changed++;
		mdata->bus_ref_cnt++;
	} else {
		if (mdata->bus_ref_cnt) {
			mdata->bus_ref_cnt--;
			if (mdata->bus_ref_cnt == 0)
				changed++;
		} else {
			pr_err("Can not be turned off\n");
		}
	}

	pr_debug("%pS: task:%s bw_cnt=%d changed=%d enable=%d\n",
		__builtin_return_address(0), current->group_leader->comm,
		mdata->bus_ref_cnt, changed, enable);

	if (changed) {
		MDSS_XLOG(mdata->bus_ref_cnt, enable);

		if (!enable) {
			if (!mdata->handoff_pending) {
				msm_bus_scale_client_update_request(
						mdata->bus_hdl, 0);
				mdata->ao_bw_uc_idx = 0;
			}
			pm_runtime_mark_last_busy(&mdata->pdev->dev);
			pm_runtime_put_autosuspend(&mdata->pdev->dev);
		} else {
			pm_runtime_get_sync(&mdata->pdev->dev);
			msm_bus_scale_client_update_request(
				mdata->bus_hdl, mdata->curr_bw_uc_idx);
		}
	}

	mutex_unlock(&mdata->bus_lock);
}
EXPORT_SYMBOL(mdss_bus_bandwidth_ctrl);

void mdss_mdp_clk_ctrl(int enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	static int mdp_clk_cnt;
	int changed = 0;

	mutex_lock(&mdp_clk_lock);
	if (enable) {
		if (mdp_clk_cnt == 0)
			changed++;
		mdp_clk_cnt++;
	} else {
		if (mdp_clk_cnt) {
			mdp_clk_cnt--;
			if (mdp_clk_cnt == 0)
				changed++;
		} else {
			pr_err("Can not be turned off\n");
		}
	}

	if (changed)
		MDSS_XLOG(mdp_clk_cnt, enable, current->pid);

	pr_debug("%pS: task:%s clk_cnt=%d changed=%d enable=%d\n",
		__builtin_return_address(0), current->group_leader->comm,
		mdata->bus_ref_cnt, changed, enable);

	if (changed)
		__mdss_mdp_clk_control(mdata, enable);

	if (enable && changed)
		mdss_mdp_idle_pc_restore();

	mutex_unlock(&mdp_clk_lock);
}

static inline int mdss_mdp_irq_clk_register(struct mdss_data_type *mdata,
					    char *clk_name, int clk_idx)
{
	struct clk *tmp;
	if (clk_idx >= MDSS_MAX_CLK) {
		pr_err("invalid clk index %d\n", clk_idx);
		return -EINVAL;
	}

	tmp = devm_clk_get(&mdata->pdev->dev, clk_name);
	if (IS_ERR(tmp)) {
		pr_err("unable to get clk: %s\n", clk_name);
		return PTR_ERR(tmp);
	}

	mdata->mdp_clk[clk_idx] = tmp;
	return 0;
}

#define SEC_DEVICE_MDSS		1

static void __mdss_restore_sec_cfg(struct mdss_data_type *mdata)
{
	int ret, scm_ret = 0;

	if (test_bit(MDSS_CAPS_SCM_RESTORE_NOT_REQUIRED, mdata->mdss_caps_map))
		return;

	pr_debug("restoring mdss secure config\n");

	__mdss_mdp_reg_access_clk_enable(mdata, true);

	ret = scm_restore_sec_cfg(SEC_DEVICE_MDSS, 0, &scm_ret);
	if (ret || scm_ret)
		pr_warn("scm_restore_sec_cfg failed %d %d\n",
				ret, scm_ret);

	__mdss_mdp_reg_access_clk_enable(mdata, false);
}

static int mdss_mdp_gdsc_notifier_call(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct mdss_data_type *mdata;

	mdata = container_of(self, struct mdss_data_type, gdsc_cb);

	if (event & REGULATOR_EVENT_ENABLE) {
		__mdss_restore_sec_cfg(mdata);
	} else if (event & REGULATOR_EVENT_PRE_DISABLE) {
		int active_cnt = atomic_read(&mdata->active_intf_cnt);

		pr_debug("mdss gdsc is getting disabled, active_cnt=%d\n",
			active_cnt);
		/*
		 * halt the vbif transactions only if we have any active
		 * overlay session
		 */
		if (active_cnt)
			mdss_mdp_vbif_axi_halt(mdata);
	}

	return NOTIFY_OK;
}

static int mdss_mdp_irq_clk_setup(struct mdss_data_type *mdata)
{
	int ret;

	ret = of_property_read_u32(mdata->pdev->dev.of_node,
			"qcom,max-clk-rate", &mdata->max_mdp_clk_rate);
	if (ret) {
		pr_err("failed to get max mdp clock rate\n");
		return ret;
	}

	pr_debug("max mdp clk rate=%d\n", mdata->max_mdp_clk_rate);

	ret = devm_request_irq(&mdata->pdev->dev, mdss_mdp_hw.irq_info->irq,
				mdss_irq_handler, 0x0, "MDSS", mdata);
	if (ret) {
		pr_err("mdp request_irq() failed!\n");
		return ret;
	}
	disable_irq(mdss_mdp_hw.irq_info->irq);

	mdata->fs = devm_regulator_get(&mdata->pdev->dev, "vdd");
	if (IS_ERR_OR_NULL(mdata->fs)) {
		mdata->fs = NULL;
		pr_err("unable to get gdsc regulator\n");
		return -EINVAL;
	}

	mdata->core_gdsc = devm_regulator_get_optional(&mdata->pdev->dev,
		"gdsc-core");
	if (IS_ERR_OR_NULL(mdata->core_gdsc)) {
		mdata->core_gdsc = NULL;
		pr_err("unable to get core gdsc regulator\n");
	} else {
		pr_debug("core gdsc regulator found\n");
	}

	mdata->fs_ena = false;

	mdata->gdsc_cb.notifier_call = mdss_mdp_gdsc_notifier_call;
	mdata->gdsc_cb.priority = 5;
	if (regulator_register_notifier(mdata->fs, &(mdata->gdsc_cb)))
		pr_warn("GDSC notification registration failed!\n");
	else
		mdata->regulator_notif_register = true;

	mdata->vdd_cx = devm_regulator_get_optional(&mdata->pdev->dev,
				"vdd-cx");
	if (IS_ERR_OR_NULL(mdata->vdd_cx)) {
		pr_debug("unable to get CX reg. rc=%d\n",
					PTR_RET(mdata->vdd_cx));
		mdata->vdd_cx = NULL;
	} else {
		/* Parse CX voltage settings */
		ret = of_property_read_u32(mdata->pdev->dev.of_node,
			"vdd-cx-min-uV", &mdata->vdd_cx_min_uv);
		if (ret) {
			pr_err("min uV for vdd-cx not specified. rc=%d\n", ret);
			return ret;
		}

		ret = of_property_read_u32(mdata->pdev->dev.of_node,
			"vdd-cx-max-uV", &mdata->vdd_cx_max_uv);
		if (ret) {
			pr_err("max uV for vdd-cx not specified. rc=%d\n", ret);
			return ret;
		}

		pr_debug("vdd_cx [min_uV, max_uV] = [%d %d]\n",
			mdata->vdd_cx_min_uv, mdata->vdd_cx_max_uv);
	}

	mdata->reg_bus_clt = mdss_reg_bus_vote_client_create("mdp\0");
	if (IS_ERR(mdata->reg_bus_clt)) {
		pr_err("bus client register failed\n");
		return PTR_ERR(mdata->reg_bus_clt);
	}

	if (mdss_mdp_irq_clk_register(mdata, "bus_clk", MDSS_CLK_AXI) ||
	    mdss_mdp_irq_clk_register(mdata, "iface_clk", MDSS_CLK_AHB) ||
	    mdss_mdp_irq_clk_register(mdata, "core_clk",
				      MDSS_CLK_MDP_CORE))
		return -EINVAL;

	/* lut_clk is not present on all MDSS revisions */
	mdss_mdp_irq_clk_register(mdata, "lut_clk", MDSS_CLK_MDP_LUT);

	/* vsync_clk is optional for non-smart panels */
	mdss_mdp_irq_clk_register(mdata, "vsync_clk", MDSS_CLK_MDP_VSYNC);

	/* this clk is not present on all MDSS revisions */
	mdss_mdp_irq_clk_register(mdata, "mnoc_clk", MDSS_CLK_MNOC_AHB);

	/* Setting the default clock rate to the max supported.*/
	mdss_mdp_set_clk_rate(mdata->max_mdp_clk_rate, false);
	pr_debug("mdp clk rate=%ld\n",
		mdss_mdp_get_clk_rate(MDSS_CLK_MDP_CORE, false));

	return 0;
}

static void mdss_debug_enable_clock(int on)
{
	if (on)
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	else
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

static int mdss_mdp_debug_init(struct platform_device *pdev,
	struct mdss_data_type *mdata)
{
	int rc;
	struct mdss_debug_base *dbg_blk;

	mdata->debug_inf.debug_enable_clock = mdss_debug_enable_clock;

	rc = mdss_debugfs_init(mdata);
	if (rc)
		return rc;

	rc = mdss_mdp_debugfs_init(mdata);
	if (rc) {
		mdss_debugfs_remove(mdata);
		return rc;
	}

	mdss_debug_register_io("mdp", &mdata->mdss_io, &dbg_blk);
	mdss_debug_register_dump_range(pdev, dbg_blk, "qcom,regs-dump-mdp",
		"qcom,regs-dump-names-mdp", "qcom,regs-dump-xin-id-mdp");

	if (mdata->vbif_io.base)
		mdss_debug_register_io("vbif", &mdata->vbif_io, NULL);
	if (mdata->vbif_nrt_io.base)
		mdss_debug_register_io("vbif_nrt", &mdata->vbif_nrt_io, NULL);

	return 0;
}

static u32 mdss_get_props(void)
{
	u32 props = 0;
	void __iomem *props_base = ioremap(0xFC4B8114, 4);
	if (props_base) {
		props = readl_relaxed(props_base);
		iounmap(props_base);
	}
	return props;
}

static void mdss_rpm_set_msg_ram(bool enable)
{
	u32 read_reg = 0;
	void __iomem *rpm_msg_ram = ioremap(0x7781FC, 4);

	if (rpm_msg_ram) {
		writel_relaxed(enable, rpm_msg_ram);
		read_reg = readl_relaxed(rpm_msg_ram);
		pr_debug("%s enable=%d read_val=%x\n", __func__, enable,
				read_reg);
		iounmap(rpm_msg_ram);
	}
}

void mdss_mdp_init_default_prefill_factors(struct mdss_data_type *mdata)
{
	mdata->prefill_data.prefill_factors.fmt_mt_nv12_factor = 8;
	mdata->prefill_data.prefill_factors.fmt_mt_factor = 4;
	mdata->prefill_data.prefill_factors.fmt_linear_factor = 1;
	mdata->prefill_data.prefill_factors.scale_factor = 1;
	mdata->prefill_data.prefill_factors.xtra_ff_factor = 2;

	if (test_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map)) {
		mdata->prefill_data.ts_threshold = 25;
		mdata->prefill_data.ts_end = 8;
		mdata->prefill_data.ts_rate.numer = 1;
		mdata->prefill_data.ts_rate.denom = 4;
		mdata->prefill_data.ts_overhead = 2;
	}
}

static void mdss_mdp_hw_rev_caps_init(struct mdss_data_type *mdata)
{

	mdata->per_pipe_ib_factor.numer = 0;
	mdata->per_pipe_ib_factor.denom = 0;
	mdata->apply_post_scale_bytes = true;
	mdata->hflip_buffer_reused = true;
	/* prevent disable of prefill calculations */
	mdata->min_prefill_lines = 0xffff;
	/* clock gating feature is enabled by default */
	mdata->enable_gate = true;
	mdata->pixel_ram_size = 0;
	mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL_FLAT;

	mdss_mdp_hw_rev_debug_caps_init(mdata);

	switch (mdata->mdp_rev) {
	case MDSS_MDP_HW_REV_107:
		mdss_set_quirk(mdata, MDSS_QUIRK_ROTCDP);
	case MDSS_MDP_HW_REV_107_1:
		mdss_mdp_format_flag_removal(invalid_mdp107_wb_output_fmts,
			ARRAY_SIZE(invalid_mdp107_wb_output_fmts),
			VALID_MDP_WB_INTF_FORMAT);
	case MDSS_MDP_HW_REV_107_2:
		mdata->max_target_zorder = 7; /* excluding base layer */
		mdata->max_cursor_size = 128;
		mdata->per_pipe_ib_factor.numer = 8;
		mdata->per_pipe_ib_factor.denom = 5;
		mdata->apply_post_scale_bytes = false;
		mdata->hflip_buffer_reused = false;
		mdata->min_prefill_lines = 21;
		mdata->has_ubwc = true;
		mdata->pixel_ram_size = 50 * 1024;
		set_bit(MDSS_QOS_PER_PIPE_IB, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_OVERHEAD_FACTOR, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_CDP, mdata->mdss_qos_map); /* cdp supported */
		mdata->enable_cdp = true; /* enable cdp */
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_PER_PIPE_LUT, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_CAPS_YUV_CONFIG, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_SCM_RESTORE_NOT_REQUIRED,
			mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_3D_MUX_UNDERRUN_RECOVERY_SUPPORTED,
			mdata->mdss_caps_map);
		mdss_mdp_init_default_prefill_factors(mdata);
		mdss_set_quirk(mdata, MDSS_QUIRK_DSC_RIGHT_ONLY_PU);
		mdss_set_quirk(mdata, MDSS_QUIRK_DSC_2SLICE_PU_THRPUT);
		break;
	case MDSS_MDP_HW_REV_105:
	case MDSS_MDP_HW_REV_109:
		mdss_set_quirk(mdata, MDSS_QUIRK_BWCPANIC);
		mdata->max_target_zorder = 7; /* excluding base layer */
		mdata->max_cursor_size = 128;
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		set_bit(MDSS_CAPS_3D_MUX_UNDERRUN_RECOVERY_SUPPORTED,
			mdata->mdss_caps_map);
		break;
	case MDSS_MDP_HW_REV_110:
		mdss_set_quirk(mdata, MDSS_QUIRK_BWCPANIC);
		mdata->max_target_zorder = 4; /* excluding base layer */
		mdata->max_cursor_size = 128;
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		mdata->min_prefill_lines = 12;
		mdata->props = mdss_get_props();
		break;
	case MDSS_MDP_HW_REV_112:
		mdata->max_target_zorder = 4; /* excluding base layer */
		mdata->max_cursor_size = 64;
		mdata->min_prefill_lines = 12;
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		break;
	case MDSS_MDP_HW_REV_114:
		/* disable ECG for 28nm PHY platform */
		mdata->enable_gate = false;
	case MDSS_MDP_HW_REV_116:
		mdata->max_target_zorder = 4; /* excluding base layer */
		mdata->max_cursor_size = 128;
		mdata->min_prefill_lines = 14;
		mdata->has_ubwc = true;
		mdata->pixel_ram_size = 40 * 1024;
		mdata->apply_post_scale_bytes = false;
		mdata->hflip_buffer_reused = false;
		mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL;
		set_bit(MDSS_QOS_OVERHEAD_FACTOR, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_PER_PIPE_LUT, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_CAPS_YUV_CONFIG, mdata->mdss_caps_map);
		mdss_mdp_init_default_prefill_factors(mdata);
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		mdss_set_quirk(mdata, MDSS_QUIRK_DMA_BI_DIR);
		mdss_set_quirk(mdata, MDSS_QUIRK_NEED_SECURE_MAP);
		break;
	case MDSS_MDP_HW_REV_115:
		mdata->max_target_zorder = 4; /* excluding base layer */
		mdata->max_cursor_size = 128;
		mdata->min_prefill_lines = 14;
		mdata->has_ubwc = false;
		mdata->pixel_ram_size = 16 * 1024;
		mdata->apply_post_scale_bytes = false;
		mdata->hflip_buffer_reused = false;
		/* disable ECG for 28nm PHY platform */
		mdata->enable_gate = false;
		mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL;
		set_bit(MDSS_QOS_PER_PIPE_LUT, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_CAPS_YUV_CONFIG, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_MIXER_1_FOR_WB, mdata->mdss_caps_map);
		mdss_mdp_init_default_prefill_factors(mdata);
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		mdss_set_quirk(mdata, MDSS_QUIRK_DMA_BI_DIR);
		mdss_set_quirk(mdata, MDSS_QUIRK_NEED_SECURE_MAP);
		break;
	case MDSS_MDP_HW_REV_300:
	case MDSS_MDP_HW_REV_301:
		mdata->max_target_zorder = 7; /* excluding base layer */
		mdata->max_cursor_size = 512;
		mdata->per_pipe_ib_factor.numer = 8;
		mdata->per_pipe_ib_factor.denom = 5;
		mdata->apply_post_scale_bytes = false;
		mdata->hflip_buffer_reused = false;
		mdata->min_prefill_lines = 25;
		mdata->has_ubwc = true;
		mdata->pixel_ram_size = 50 * 1024;
		mdata->rects_per_sspp[MDSS_MDP_PIPE_TYPE_DMA] = 2;

		mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL_SWITCH;
		set_bit(MDSS_QOS_PER_PIPE_IB, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_REMAPPER, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_WB_QOS, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_OVERHEAD_FACTOR, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_CDP, mdata->mdss_qos_map); /* cdp supported */
		mdata->enable_cdp = false; /* disable cdp */
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_PER_PIPE_LUT, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_IB_NOCR, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_WB2_WRITE_GATHER_EN, mdata->mdss_qos_map);
		set_bit(MDSS_CAPS_YUV_CONFIG, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_SCM_RESTORE_NOT_REQUIRED,
			mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_3D_MUX_UNDERRUN_RECOVERY_SUPPORTED,
			mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_QSEED3, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_DEST_SCALER, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_CWB_SUPPORTED, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_MDP_VOTE_CLK_NOT_SUPPORTED,
			mdata->mdss_caps_map);
		mdss_mdp_init_default_prefill_factors(mdata);
		mdss_set_quirk(mdata, MDSS_QUIRK_DSC_RIGHT_ONLY_PU);
		mdss_set_quirk(mdata, MDSS_QUIRK_DSC_2SLICE_PU_THRPUT);
		mdss_set_quirk(mdata, MDSS_QUIRK_MMSS_GDSC_COLLAPSE);
		mdss_set_quirk(mdata, MDSS_QUIRK_MDP_CLK_SET_RATE);
		mdata->has_wb_ubwc = true;
		set_bit(MDSS_CAPS_10_BIT_SUPPORTED, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_AVR_SUPPORTED, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_SEC_DETACH_SMMU, mdata->mdss_caps_map);
		mdss_set_quirk(mdata, MDSS_QUIRK_HDR_SUPPORT_ENABLED);
		break;
	case MDSS_MDP_HW_REV_320:
	case MDSS_MDP_HW_REV_330:
		mdata->max_target_zorder = 7; /* excluding base layer */
		mdata->max_cursor_size = 512;
		mdata->per_pipe_ib_factor.numer = 8;
		mdata->per_pipe_ib_factor.denom = 5;
		mdata->apply_post_scale_bytes = false;
		mdata->hflip_buffer_reused = false;
		mdata->min_prefill_lines = 25;
		mdata->has_ubwc = true;
		mdata->pixel_ram_size =
			(mdata->mdp_rev == MDSS_MDP_HW_REV_320) ? 50 : 40;
		mdata->pixel_ram_size *= 1024;
		mdata->rects_per_sspp[MDSS_MDP_PIPE_TYPE_DMA] = 2;

		mem_protect_sd_ctrl_id = MEM_PROTECT_SD_CTRL_SWITCH;
		set_bit(MDSS_QOS_PER_PIPE_IB, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_REMAPPER, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_OVERHEAD_FACTOR, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_CDP, mdata->mdss_qos_map); /* cdp supported */
		mdata->enable_cdp = false; /* disable cdp */
		set_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_PER_PIPE_LUT, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_IB_NOCR, mdata->mdss_qos_map);
		set_bit(MDSS_QOS_WB2_WRITE_GATHER_EN, mdata->mdss_qos_map);
		set_bit(MDSS_CAPS_YUV_CONFIG, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_SCM_RESTORE_NOT_REQUIRED,
			mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_3D_MUX_UNDERRUN_RECOVERY_SUPPORTED,
			mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_QSEED3, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_DEST_SCALER, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_MDP_VOTE_CLK_NOT_SUPPORTED,
			mdata->mdss_caps_map);
		mdss_mdp_init_default_prefill_factors(mdata);
		mdss_set_quirk(mdata, MDSS_QUIRK_DSC_RIGHT_ONLY_PU);
		mdss_set_quirk(mdata, MDSS_QUIRK_DSC_2SLICE_PU_THRPUT);
		mdss_set_quirk(mdata, MDSS_QUIRK_MMSS_GDSC_COLLAPSE);
		mdss_set_quirk(mdata, MDSS_QUIRK_MDP_CLK_SET_RATE);
		mdss_set_quirk(mdata, MDSS_QUIRK_DMA_BI_DIR);
		mdata->has_wb_ubwc = true;
		set_bit(MDSS_CAPS_10_BIT_SUPPORTED, mdata->mdss_caps_map);
		set_bit(MDSS_CAPS_SEC_DETACH_SMMU, mdata->mdss_caps_map);
		break;
	default:
		mdata->max_target_zorder = 4; /* excluding base layer */
		mdata->max_cursor_size = 64;
	}

	if (mdata->mdp_rev < MDSS_MDP_HW_REV_103)
		mdss_set_quirk(mdata, MDSS_QUIRK_DOWNSCALE_HANG);

	if (mdata->mdp_rev < MDSS_MDP_HW_REV_102 ||
			mdata->mdp_rev == MDSS_MDP_HW_REV_200)
		mdss_set_quirk(mdata, MDSS_QUIRK_FMT_PACK_PATTERN);
}

static void mdss_hw_rev_init(struct mdss_data_type *mdata)
{
	if (mdata->mdp_rev)
		return;

	mdata->mdp_rev = MDSS_REG_READ(mdata, MDSS_REG_HW_VERSION);
	mdss_mdp_hw_rev_caps_init(mdata);
}

/**
 * mdss_hw_init() - Initialize MDSS target specific register settings
 * @mdata: MDP private data
 *
 * Initialize basic MDSS hardware settings based on the board specific
 * parameters. This function does not explicitly turn on the MDP clocks
 * and so it must be called with the MDP clocks already enabled.
 */
void mdss_hw_init(struct mdss_data_type *mdata)
{
	struct mdss_mdp_pipe *vig;

	mdss_hw_rev_init(mdata);

	/* Disable hw underrun recovery only for older mdp reversions. */
	if (mdata->mdp_rev < MDSS_MDP_HW_REV_105)
		writel_relaxed(0x0, mdata->mdp_base +
			MDSS_MDP_REG_VIDEO_INTF_UNDERFLOW_CTL);

	if (mdata->hw_settings) {
		struct mdss_hw_settings *hws = mdata->hw_settings;

		while (hws->reg) {
			writel_relaxed(hws->val, hws->reg);
			hws++;
		}
	}

	vig = mdata->vig_pipes;

	mdata->nmax_concurrent_ad_hw =
		(mdata->mdp_rev < MDSS_MDP_HW_REV_103) ? 1 : 2;

	pr_debug("MDP hw init done\n");
}

static u32 mdss_mdp_res_init(struct mdss_data_type *mdata)
{
	u32 rc = 0;

	if (mdata->res_init) {
		pr_err("mdss resources already initialized\n");
		return -EPERM;
	}

	mdata->res_init = true;
	mdata->clk_ena = false;
	mdss_mdp_hw.irq_info->irq_mask = MDSS_MDP_DEFAULT_INTR_MASK;
	mdss_mdp_hw.irq_info->irq_ena = false;

	rc = mdss_mdp_irq_clk_setup(mdata);
	if (rc)
		return rc;

	mdata->hist_intr.req = 0;
	mdata->hist_intr.curr = 0;
	mdata->hist_intr.state = 0;
	spin_lock_init(&mdata->hist_intr.lock);

	mdata->iclient = msm_ion_client_create(mdata->pdev->name);
	if (IS_ERR_OR_NULL(mdata->iclient)) {
		pr_err("msm_ion_client_create() return error (%pK)\n",
				mdata->iclient);
		mdata->iclient = NULL;
	}

	return rc;
}

static u32 mdss_mdp_scaler_init(struct mdss_data_type *mdata,
				struct device *dev)
{
	int ret = -EINVAL;
	struct device_node *node;
	u32 prop_val;
	int len = 0;

	if (!dev)
		return -EPERM;

	node = of_get_child_by_name(dev->of_node, "qcom,mdss-scaler-offsets");
	if (!node)
		return 0;

	if (mdata->scaler_off)
		return -EFAULT;

	mdata->scaler_off = devm_kzalloc(&mdata->pdev->dev,
			sizeof(*mdata->scaler_off), GFP_KERNEL);
	if (!mdata->scaler_off)
		return -ENOMEM;

	ret = of_property_read_u32(node,
			"qcom,mdss-vig-scaler-off",
			&prop_val);
	if (ret) {
		pr_err("read property %s failed ret %d\n",
				"qcom,mdss-vig-scaler-off", ret);
		return -EINVAL;
	}
	mdata->scaler_off->vig_scaler_off = prop_val;
	ret = of_property_read_u32(node,
			"qcom,mdss-vig-scaler-lut-off",
			&prop_val);
	if (ret) {
		pr_err("read property %s failed ret %d\n",
				"qcom,mdss-vig-scaler-lut-off", ret);
		return -EINVAL;
	}
	mdata->scaler_off->vig_scaler_lut_off = prop_val;
	mdata->scaler_off->has_dest_scaler =
		of_property_read_bool(node, "qcom,mdss-has-dest-scaler");
	if (mdata->scaler_off->has_dest_scaler) {
		ret = of_property_read_u32(node,
				"qcom,mdss-dest-block-off",
				&prop_val);
		if (ret) {
			pr_err("read property %s failed ret %d\n",
					"qcom,mdss-dest-block-off", ret);
			return -EINVAL;
		}
		mdata->scaler_off->dest_base = mdata->mdss_io.base +
			prop_val;

		if (!of_find_property(node, "qcom,mdss-dest-scaler-off", &len)
				|| (len < 1)) {
			pr_err("find property %s failed ret %d\n",
					"qcom,mdss-dest-scaler-off", ret);
			return -EINVAL;
		}
		mdata->scaler_off->ndest_scalers = len/sizeof(u32);

		mdata->scaler_off->dest_scaler_off =
			devm_kzalloc(dev, sizeof(u32) *
					mdata->scaler_off->ndest_scalers,
					GFP_KERNEL);
		if  (!mdata->scaler_off->dest_scaler_off) {
			return -ENOMEM;
		}
		ret = of_property_read_u32_array(node,
				"qcom,mdss-dest-scaler-off",
				mdata->scaler_off->dest_scaler_off,
				mdata->scaler_off->ndest_scalers);
		if (ret)
			return ret;

		mdata->scaler_off->dest_scaler_lut_off =
			devm_kzalloc(dev, sizeof(u32) *
					mdata->scaler_off->ndest_scalers,
					GFP_KERNEL);
		if  (!mdata->scaler_off->dest_scaler_lut_off) {
			return -ENOMEM;
		}
		ret = of_property_read_u32_array(node,
				"qcom,mdss-dest-scaler-lut-off",
				mdata->scaler_off->dest_scaler_lut_off,
				mdata->scaler_off->ndest_scalers);
		if (ret)
			return ret;

		ret = of_property_read_u32(dev->of_node,
				"qcom,max-dest-scaler-input-width",
				&mdata->max_dest_scaler_input_width);
		if (ret) {
			pr_debug("read property %s failed ret %d\n",
					"qcom,max-dest-scaler-input-width",
					ret);
		}

		ret = of_property_read_u32(dev->of_node,
				"qcom,max-dest-scaler-output-width",
				&mdata->max_dest_scaler_output_width);
		if (ret) {
			pr_debug("read property %s failed ret %d\n",
					"qcom,max-dest-scaler-output-width",
					ret);
		}

		ret = mdss_mdp_ds_addr_setup(mdata);
	}

	return ret;
}

/**
 * mdss_mdp_footswitch_ctrl_splash() - clocks handoff for cont. splash screen
 * @on: 1 to start handoff, 0 to complete the handoff after first frame update
 *
 * MDSS Clocks and GDSC are already on during continous splash screen, but
 * increasing ref count will keep clocks from being turned off until handoff
 * has properly happend after frame update.
 */
void mdss_mdp_footswitch_ctrl_splash(int on)
{
	int ret;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata != NULL) {
		if (on) {
			mdata->handoff_pending = true;
			pr_debug("Enable MDP FS for splash.\n");
			if (mdata->core_gdsc) {
				ret = regulator_enable(mdata->core_gdsc);
				if (ret)
					pr_err("core_gdsc failed to enable\n");
			}

			ret = regulator_enable(mdata->fs);
			if (ret)
				pr_err("Footswitch failed to enable\n");

			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			mdss_bus_bandwidth_ctrl(true);
		} else {
			pr_debug("Disable MDP FS for splash.\n");
			mdss_bus_bandwidth_ctrl(false);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
			regulator_disable(mdata->fs);
			if (mdata->core_gdsc)
				regulator_disable(mdata->core_gdsc);
			mdata->handoff_pending = false;
		}
	} else {
		pr_warn("mdss mdata not initialized\n");
	}
}

static int mdss_mdp_get_pan_intf(const char *pan_intf)
{
	int i, rc = MDSS_PANEL_INTF_INVALID;

	if (!pan_intf)
		return rc;

	for (i = 0; i < ARRAY_SIZE(pan_types); i++) {
		if (!strcmp(pan_intf, pan_types[i].name)) {
			rc = pan_types[i].type;
			break;
		}
	}
	return rc;
}

static int mdss_mdp_get_pan_cfg(struct mdss_panel_cfg *pan_cfg)
{
	char *t = NULL;
	char pan_intf_str[MDSS_MAX_PANEL_LEN];
	int rc, i, panel_len;
	char pan_name[MDSS_MAX_PANEL_LEN] = {'\0'};

	if (!pan_cfg)
		return -EINVAL;

	if (mdss_mdp_panel[0] == '0') {
		pr_debug("panel name is not set\n");
		pan_cfg->lk_cfg = false;
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	} else if (mdss_mdp_panel[0] == '1') {
		pan_cfg->lk_cfg = true;
	} else {
		/* read from dt */
		pan_cfg->lk_cfg = true;
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	}

	/* skip lk cfg and delimiter; ex: "1:" */
	strlcpy(pan_name, &mdss_mdp_panel[2], MDSS_MAX_PANEL_LEN);
	t = strnstr(pan_name, ":", MDSS_MAX_PANEL_LEN);
	if (!t) {
		pr_err("pan_name=[%s] invalid\n", pan_name);
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	}

	for (i = 0; ((pan_name + i) < t) && (i < 4); i++)
		pan_intf_str[i] = *(pan_name + i);
	pan_intf_str[i] = 0;
	pr_debug("%d panel intf %s\n", __LINE__, pan_intf_str);
	/* point to the start of panel name */
	t = t + 1;
	strlcpy(&pan_cfg->arg_cfg[0], t, sizeof(pan_cfg->arg_cfg));
	pr_debug("%d: t=[%s] panel name=[%s]\n", __LINE__,
		t, pan_cfg->arg_cfg);

	panel_len = strlen(pan_cfg->arg_cfg);
	if (!panel_len) {
		pr_err("Panel name is invalid\n");
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	}

	rc = mdss_mdp_get_pan_intf(pan_intf_str);
	pan_cfg->pan_intf = (rc < 0) ?  MDSS_PANEL_INTF_INVALID : rc;
	return 0;
}

static int mdss_mdp_parse_dt_pan_intf(struct platform_device *pdev)
{
	int rc;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	const char *prim_intf = NULL;

	rc = of_property_read_string(pdev->dev.of_node,
				"qcom,mdss-pref-prim-intf", &prim_intf);
	if (rc)
		return -ENODEV;

	rc = mdss_mdp_get_pan_intf(prim_intf);
	if (rc < 0) {
		mdata->pan_cfg.pan_intf = MDSS_PANEL_INTF_INVALID;
	} else {
		mdata->pan_cfg.pan_intf = rc;
		rc = 0;
	}
	return rc;
}

static int mdss_mdp_get_cmdline_config(struct platform_device *pdev)
{
	int rc, len = 0;
	int *intf_type;
	char *panel_name;
	struct mdss_panel_cfg *pan_cfg;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->pan_cfg.arg_cfg[MDSS_MAX_PANEL_LEN] = 0;
	pan_cfg = &mdata->pan_cfg;
	panel_name = &pan_cfg->arg_cfg[0];
	intf_type = &pan_cfg->pan_intf;

	/* reads from dt by default */
	pan_cfg->lk_cfg = true;

	len = strlen(mdss_mdp_panel);

	if (len > 0) {
		rc = mdss_mdp_get_pan_cfg(pan_cfg);
		if (!rc) {
			pan_cfg->init_done = true;
			return rc;
		}
	}

	rc = mdss_mdp_parse_dt_pan_intf(pdev);
	/* if pref pan intf is not present */
	if (rc)
		pr_warn("unable to parse device tree for pan intf\n");

	pan_cfg->init_done = true;

	return 0;
}

static void __update_sspp_info(struct mdss_mdp_pipe *pipe,
	int pipe_cnt, char *type, char *buf, int *cnt)
{
	int i;
	int j;
	size_t len = PAGE_SIZE;
	int num_bytes = BITS_TO_BYTES(MDP_IMGTYPE_LIMIT1);

	if (!pipe)
		return;
#define SPRINT(fmt, ...) \
		(*cnt += scnprintf(buf + *cnt, len - *cnt, fmt, ##__VA_ARGS__))

	for (i = 0; i < pipe_cnt && pipe; i++) {
		SPRINT("pipe_num:%d pipe_type:%s pipe_ndx:%d rects:%d pipe_is_handoff:%d display_id:%d ",
			pipe->num, type, pipe->ndx, pipe->multirect.max_rects,
			pipe->is_handed_off, mdss_mdp_get_display_id(pipe));
		SPRINT("fmts_supported:");
		for (j = 0; j < num_bytes; j++)
			SPRINT("%d,", pipe->supported_formats[j]);
		SPRINT("\n");
		pipe += pipe->multirect.max_rects;
	}
#undef SPRINT
}

static void mdss_mdp_update_sspp_info(struct mdss_data_type *mdata,
	char *buf, int *cnt)
{
	__update_sspp_info(mdata->vig_pipes, mdata->nvig_pipes,
		"vig", buf, cnt);
	__update_sspp_info(mdata->rgb_pipes, mdata->nrgb_pipes,
		"rgb", buf, cnt);
	__update_sspp_info(mdata->dma_pipes, mdata->ndma_pipes,
		"dma", buf, cnt);
	__update_sspp_info(mdata->cursor_pipes, mdata->ncursor_pipes,
		"cursor", buf, cnt);
}

static void mdss_mdp_update_wb_info(struct mdss_data_type *mdata,
	char *buf, int *cnt)
{
#define SPRINT(fmt, ...) \
		(*cnt += scnprintf(buf + *cnt, len - *cnt, fmt, ##__VA_ARGS__))
	size_t len = PAGE_SIZE;
	int i;
	int num_bytes = BITS_TO_BYTES(MDP_IMGTYPE_LIMIT1);

	SPRINT("rot_input_fmts=");
	for (i = 0; i < num_bytes && mdata->wb; i++)
		SPRINT("%d ", mdata->wb->supported_input_formats[i]);
	SPRINT("\nrot_output_fmts=");
	for (i = 0; i < num_bytes && mdata->wb; i++)
		SPRINT("%d ", mdata->wb->supported_input_formats[i]);
	SPRINT("\nwb_output_fmts=");
	for (i = 0; i < num_bytes && mdata->wb; i++)
		SPRINT("%d ", mdata->wb->supported_output_formats[i]);
	SPRINT("\n");
#undef SPRINT
}

ssize_t mdss_mdp_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	int cnt = 0;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("mdp_version=5\n");
	SPRINT("hw_rev=%d\n", mdata->mdp_rev);
	SPRINT("pipe_count:%d\n", mdata->nvig_pipes + mdata->nrgb_pipes +
		mdata->ndma_pipes + mdata->ncursor_pipes);
	mdss_mdp_update_sspp_info(mdata, buf, &cnt);
	mdss_mdp_update_wb_info(mdata, buf, &cnt);
	/* TODO : need to remove num pipes info */
	SPRINT("rgb_pipes=%d\n", mdata->nrgb_pipes);
	SPRINT("vig_pipes=%d\n", mdata->nvig_pipes);
	SPRINT("dma_pipes=%d\n", mdata->ndma_pipes);
	SPRINT("blending_stages=%d\n", mdata->max_target_zorder);
	SPRINT("cursor_pipes=%d\n", mdata->ncursor_pipes);
	SPRINT("max_cursor_size=%d\n", mdata->max_cursor_size);
	SPRINT("smp_count=%d\n", mdata->smp_mb_cnt);
	SPRINT("smp_size=%d\n", mdata->smp_mb_size);
	SPRINT("smp_mb_per_pipe=%d\n", mdata->smp_mb_per_pipe);
	SPRINT("max_downscale_ratio=%d\n", MAX_DOWNSCALE_RATIO);
	SPRINT("max_upscale_ratio=%d\n", MAX_UPSCALE_RATIO);

	if (mdata->nwb)
		SPRINT("wb_intf_index=%d\n", mdata->nwb - 1);

	if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map)) {
		SPRINT("fmt_mt_nv12_factor=%d\n",
			mdata->prefill_data.prefill_factors.fmt_mt_nv12_factor);
		SPRINT("fmt_mt_factor=%d\n",
			mdata->prefill_data.prefill_factors.fmt_mt_factor);
		SPRINT("fmt_linear_factor=%d\n",
			mdata->prefill_data.prefill_factors.fmt_linear_factor);
		SPRINT("scale_factor=%d\n",
			mdata->prefill_data.prefill_factors.scale_factor);
		SPRINT("xtra_ff_factor=%d\n",
			mdata->prefill_data.prefill_factors.xtra_ff_factor);
	}

	if (test_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map)) {
		SPRINT("amortizable_threshold=%d\n",
			mdata->prefill_data.ts_threshold);
		SPRINT("system_overhead_lines=%d\n",
			mdata->prefill_data.ts_overhead);
	}

	if (mdata->props)
		SPRINT("props=%d\n", mdata->props);
	if (mdata->max_bw_low)
		SPRINT("max_bandwidth_low=%u\n", mdata->max_bw_low);
	if (mdata->max_bw_high)
		SPRINT("max_bandwidth_high=%u\n", mdata->max_bw_high);
	if (mdata->max_pipe_width)
		SPRINT("max_pipe_width=%d\n", mdata->max_pipe_width);
	if (mdata->max_mixer_width)
		SPRINT("max_mixer_width=%d\n", mdata->max_mixer_width);
	if (mdata->max_bw_per_pipe)
		SPRINT("max_pipe_bw=%u\n", mdata->max_bw_per_pipe);
	if (mdata->max_mdp_clk_rate)
		SPRINT("max_mdp_clk=%u\n", mdata->max_mdp_clk_rate);
	if (mdata->clk_factor.numer)
		SPRINT("clk_fudge_factor=%u,%u\n", mdata->clk_factor.numer,
			mdata->clk_factor.denom);
	if (test_bit(MDSS_CAPS_DEST_SCALER, mdata->mdss_caps_map)) {
		SPRINT("max_dest_scaler_input_width=%u\n",
				mdata->max_dest_scaler_input_width);
		SPRINT("max_dest_scaler_output_width=%u\n",
				mdata->max_dest_scaler_output_width);
		SPRINT("dest_scaler_count=%u\n",
				mdata->scaler_off->ndest_scalers);
		SPRINT("max_dest_scale_up=%u\n", MAX_UPSCALE_RATIO);
	}

	SPRINT("features=");
	if (mdata->has_bwc)
		SPRINT(" bwc");
	if (mdata->has_ubwc)
		SPRINT(" ubwc");
	if (mdata->has_wb_ubwc)
		SPRINT(" wb_ubwc");
	if (mdata->has_decimation)
		SPRINT(" decimation");
	if (mdata->highest_bank_bit && !mdss_mdp_is_ubwc_supported(mdata))
		SPRINT(" tile_format");
	if (mdata->has_non_scalar_rgb)
		SPRINT(" non_scalar_rgb");
	if (mdata->has_src_split)
		SPRINT(" src_split");
	if (mdata->has_rot_dwnscale)
		SPRINT(" rotator_downscale");
	if (mdata->max_bw_settings_cnt)
		SPRINT(" dynamic_bw_limit");
	if (test_bit(MDSS_CAPS_QSEED3, mdata->mdss_caps_map))
		SPRINT(" qseed3");
	if (test_bit(MDSS_CAPS_DEST_SCALER, mdata->mdss_caps_map))
		SPRINT(" dest_scaler");
	if (mdata->has_separate_rotator)
		SPRINT(" separate_rotator");
	if (test_bit(MDSS_CAPS_CWB_SUPPORTED, mdata->mdss_caps_map))
		SPRINT(" concurrent_writeback");
	if (test_bit(MDSS_CAPS_AVR_SUPPORTED,  mdata->mdss_caps_map))
		SPRINT(" avr");
	if (mdss_has_quirk(mdata, MDSS_QUIRK_HDR_SUPPORT_ENABLED))
		SPRINT(" hdr");
	SPRINT("\n");
#undef SPRINT

	return cnt;
}

static ssize_t mdss_mdp_read_max_limit_bw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	u32 cnt = 0;
	int i;

	char bw_names[4][8] = {"default", "camera", "hflip", "vflip"};
	char pipe_bw_names[4][16] = {"default_pipe", "camera_pipe",
				"hflip_pipe", "vflip_pipe"};
	struct mdss_max_bw_settings *bw_settings;
	struct mdss_max_bw_settings *pipe_bw_settings;

	bw_settings = mdata->max_bw_settings;
	pipe_bw_settings = mdata->max_per_pipe_bw_settings;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("bw_mode_bitmap=%d\n", mdata->bw_mode_bitmap);
	SPRINT("bw_limit_pending=%d\n", mdata->bw_limit_pending);

	for (i = 0; i < mdata->max_bw_settings_cnt; i++) {
		SPRINT("%s=%d\n", bw_names[i], bw_settings->mdss_max_bw_val);
		bw_settings++;
	}

	for (i = 0; i < mdata->mdss_per_pipe_bw_cnt; i++) {
		SPRINT("%s=%d\n", pipe_bw_names[i],
					pipe_bw_settings->mdss_max_bw_val);
		pipe_bw_settings++;
	}

	return cnt;
}

static ssize_t mdss_mdp_store_max_limit_bw(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	u32 data = 0;

	if (kstrtouint(buf, 0, &data)) {
		pr_info("Not able scan to bw_mode_bitmap\n");
	} else {
		mdata->bw_mode_bitmap = data;
		mdata->bw_limit_pending = true;
		pr_debug("limit use case, bw_mode_bitmap = %d\n", data);
	}

	return len;
}

static DEVICE_ATTR(caps, S_IRUGO, mdss_mdp_show_capabilities, NULL);
static DEVICE_ATTR(bw_mode_bitmap, S_IRUGO | S_IWUSR | S_IWGRP,
		mdss_mdp_read_max_limit_bw, mdss_mdp_store_max_limit_bw);

static struct attribute *mdp_fs_attrs[] = {
	&dev_attr_caps.attr,
	&dev_attr_bw_mode_bitmap.attr,
	NULL
};

static struct attribute_group mdp_fs_attr_group = {
	.attrs = mdp_fs_attrs
};

static int mdss_mdp_register_sysfs(struct mdss_data_type *mdata)
{
	struct device *dev = &mdata->pdev->dev;
	int rc;

	rc = sysfs_create_group(&dev->kobj, &mdp_fs_attr_group);

	return rc;
}

int mdss_panel_get_intf_status(u32 disp_num, u32 intf_type)
{
	int rc, intf_status = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!mdss_res || !mdss_res->pan_cfg.init_done)
		return -EPROBE_DEFER;

	if (mdss_res->handoff_pending) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		intf_status = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_REG_DISP_INTF_SEL);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		if (intf_type == MDSS_PANEL_INTF_DSI) {
			if (disp_num == DISPLAY_1)
				rc = (intf_status & MDSS_MDP_INTF_DSI0_SEL);
			else if (disp_num == DISPLAY_2)
				rc = (intf_status & MDSS_MDP_INTF_DSI1_SEL);
			else
				rc = 0;
		} else if (intf_type == MDSS_PANEL_INTF_EDP) {
			intf_status &= MDSS_MDP_INTF_EDP_SEL;
			rc = (intf_status == MDSS_MDP_INTF_EDP_SEL);
		} else if (intf_type == MDSS_PANEL_INTF_HDMI) {
			intf_status &= MDSS_MDP_INTF_HDMI_SEL;
			rc = (intf_status == MDSS_MDP_INTF_HDMI_SEL);
		} else {
			rc = 0;
		}
	} else {
		rc = 0;
	}

	return rc;
}

static int mdss_mdp_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc;
	struct mdss_data_type *mdata;
	uint32_t intf_sel = 0;
	uint32_t split_display = 0;
	int num_of_display_on = 0;
	int i = 0;

	if (!pdev->dev.of_node) {
		pr_err("MDP driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	if (mdss_res) {
		pr_err("MDP already initialized\n");
		return -EINVAL;
	}

	mdata = devm_kzalloc(&pdev->dev, sizeof(*mdata), GFP_KERNEL);
	if (mdata == NULL)
		return -ENOMEM;

	pdev->id = 0;
	mdata->pdev = pdev;
	platform_set_drvdata(pdev, mdata);
	mdss_res = mdata;
	mutex_init(&mdata->reg_lock);
	mutex_init(&mdata->reg_bus_lock);
	mutex_init(&mdata->bus_lock);
	INIT_LIST_HEAD(&mdata->reg_bus_clist);
	atomic_set(&mdata->sd_client_count, 0);
	atomic_set(&mdata->active_intf_cnt, 0);
	init_waitqueue_head(&mdata->secure_waitq);

	mdss_res->mdss_util = mdss_get_util_intf();
	if (mdss_res->mdss_util == NULL) {
		pr_err("Failed to get mdss utility functions\n");
		return -ENODEV;
	}

	mdss_res->mdss_util->get_iommu_domain = mdss_smmu_get_domain_id;
	mdss_res->mdss_util->iommu_attached = is_mdss_iommu_attached;
	mdss_res->mdss_util->iommu_ctrl = mdss_iommu_ctrl;
	mdss_res->mdss_util->bus_scale_set_quota = mdss_bus_scale_set_quota;
	mdss_res->mdss_util->bus_bandwidth_ctrl = mdss_bus_bandwidth_ctrl;
	mdss_res->mdss_util->panel_intf_type = mdss_panel_intf_type;
	mdss_res->mdss_util->panel_intf_status = mdss_panel_get_intf_status;
	mdss_res->mdss_util->vbif_reg_lock = mdss_mdp_vbif_reg_lock;
	mdss_res->mdss_util->vbif_reg_unlock = mdss_mdp_vbif_reg_unlock;
	mdss_res->mdss_util->mdp_handoff_pending = mdss_mdp_handoff_pending;

	rc = msm_dss_ioremap_byname(pdev, &mdata->mdss_io, "mdp_phys");
	if (rc) {
		pr_err("unable to map MDP base\n");
		goto probe_done;
	}
	pr_debug("MDSS HW Base addr=0x%x len=0x%x\n",
		(int) (unsigned long) mdata->mdss_io.base,
		mdata->mdss_io.len);

	rc = msm_dss_ioremap_byname(pdev, &mdata->vbif_io, "vbif_phys");
	if (rc) {
		pr_err("unable to map MDSS VBIF base\n");
		goto probe_done;
	}
	pr_debug("MDSS VBIF HW Base addr=0x%x len=0x%x\n",
		(int) (unsigned long) mdata->vbif_io.base,
		mdata->vbif_io.len);

	rc = msm_dss_ioremap_byname(pdev, &mdata->vbif_nrt_io, "vbif_nrt_phys");
	if (rc)
		pr_debug("unable to map MDSS VBIF non-realtime base\n");
	else
		pr_debug("MDSS VBIF NRT HW Base addr=%pK len=0x%x\n",
			mdata->vbif_nrt_io.base, mdata->vbif_nrt_io.len);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("unable to get MDSS irq\n");
		rc = -ENOMEM;
		goto probe_done;
	}

	mdss_mdp_hw.irq_info = kzalloc(sizeof(struct irq_info), GFP_KERNEL);
	if (!mdss_mdp_hw.irq_info) {
		pr_err("no mem to save irq info: kzalloc fail\n");
		return -ENOMEM;
	}
	mdss_mdp_hw.irq_info->irq = res->start;
	mdss_mdp_hw.ptr = mdata;

	/* export misc. interrupts to external driver */
	mdata->irq_domain = irq_domain_add_linear(pdev->dev.of_node, 64,
			&mdss_irq_domain_ops, mdata);
	if (!mdata->irq_domain) {
		pr_err("unable to add linear domain\n");
		rc = -ENOMEM;
		goto probe_done;
	}

	mdss_misc_hw.irq_info = mdss_intr_line();
	rc = mdss_res->mdss_util->register_irq(&mdss_misc_hw);
	if (rc)
		pr_err("mdss_register_irq failed.\n");

	rc = mdss_mdp_res_init(mdata);
	if (rc) {
		pr_err("unable to initialize mdss mdp resources\n");
		goto probe_done;
	}

	rc = mdss_mdp_retention_init(mdata);
	if (rc) {
		pr_err("unable to initialize mdss mdp retention\n");
		goto probe_done;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev, AUTOSUSPEND_TIMEOUT_MS);
	if (mdata->idle_pc_enabled)
		pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		mdss_mdp_footswitch_ctrl(mdata, true);

	rc = mdss_mdp_bus_scale_register(mdata);
	if (rc) {
		pr_err("unable to register bus scaling\n");
		goto probe_done;
	}

	/*
	 * enable clocks and read mdp_rev as soon as possible once
	 * kernel is up.
	 */
	mdss_mdp_footswitch_ctrl_splash(true);
	mdss_hw_rev_init(mdata);

	/*populate hw iomem base info from device tree*/
	rc = mdss_mdp_parse_dt(pdev);
	if (rc) {
		pr_err("unable to parse device tree\n");
		goto probe_done;
	}

	rc = mdss_mdp_get_cmdline_config(pdev);
	if (rc) {
		pr_err("Error in panel override:rc=[%d]\n", rc);
		goto probe_done;
	}

	rc = mdss_mdp_debug_init(pdev, mdata);
	if (rc) {
		pr_err("unable to initialize mdp debugging\n");
		goto probe_done;
	}
	rc = mdss_mdp_scaler_init(mdata, &pdev->dev);
	if (rc)
		goto probe_done;

	rc = mdss_mdp_register_sysfs(mdata);
	if (rc)
		pr_err("unable to register mdp sysfs nodes\n");

	rc = mdss_fb_register_mdp_instance(&mdp5);
	if (rc)
		pr_err("unable to register mdp instance\n");

	rc = mdss_res->mdss_util->register_irq(&mdss_mdp_hw);
	if (rc)
		pr_err("mdss_register_irq failed.\n");

	rc = mdss_smmu_init(mdata, &pdev->dev);
	if (rc)
		pr_err("mdss smmu init failed\n");

	mdss_mdp_set_supported_formats(mdata);

	mdss_res->mdss_util->mdp_probe_done = true;

	mdss_hw_init(mdata);

	rc = mdss_mdp_pp_init(&pdev->dev);
	if (rc)
		pr_err("unable to initialize mdss pp resources\n");

	/* Restoring Secure configuration during boot-up */
	if (mdss_mdp_req_init_restore_cfg(mdata))
		__mdss_restore_sec_cfg(mdata);

	if (mdss_has_quirk(mdata, MDSS_QUIRK_BWCPANIC)) {
		mdata->default_panic_lut0 = readl_relaxed(mdata->mdp_base +
			MMSS_MDP_PANIC_LUT0);
		mdata->default_panic_lut1 = readl_relaxed(mdata->mdp_base +
			MMSS_MDP_PANIC_LUT1);
		mdata->default_robust_lut = readl_relaxed(mdata->mdp_base +
			MMSS_MDP_ROBUST_LUT);
	}

	/*
	 * Read the DISP_INTF_SEL register to check if display was enabled in
	 * bootloader or not. If yes, let handoff handle removing the extra
	 * clk/regulator votes else turn off clk/regulators because purpose
	 * here is to get mdp_rev.
	 */
	intf_sel = readl_relaxed(mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);
	split_display = readl_relaxed(mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_EN);
	if (intf_sel != 0) {
		for (i = 0; i < 4; i++)
			num_of_display_on += ((intf_sel >> i*8) & 0x000000FF);

		/*
		 * For split display enabled - DSI0, DSI1 interfaces are
		 * considered as single display. So decrement
		 * 'num_of_display_on' by 1
		 */
		if (split_display)
			num_of_display_on--;
	}
	if (!num_of_display_on) {
		mdss_mdp_footswitch_ctrl_splash(false);
	} else {
		mdata->handoff_pending = true;
		/*
		 * If multiple displays are enabled in LK, ctrl_splash off will
		 * be called multiple times during splash_cleanup. Need to
		 * enable it symmetrically
		*/
		for (i = 1; i < num_of_display_on; i++)
			mdss_mdp_footswitch_ctrl_splash(true);
	}

	mdp_intr_cb  = kcalloc(ARRAY_SIZE(mdp_irq_map),
			sizeof(struct intr_callback), GFP_KERNEL);
	if (mdp_intr_cb == NULL)
		return -ENOMEM;

	mdss_res->mdp_irq_mask = kcalloc(ARRAY_SIZE(mdp_intr_reg),
			sizeof(u32), GFP_KERNEL);
	if (mdss_res->mdp_irq_mask == NULL)
		return -ENOMEM;

	mdss_res->mdp_irq_raw = kcalloc(ARRAY_SIZE(mdp_intr_reg),
			sizeof(u32), GFP_KERNEL);
	if (mdss_res->mdp_irq_raw == NULL) {
		kfree(mdss_res->mdp_irq_mask);
		mdss_res->mdp_irq_mask = NULL;
		return -ENOMEM;
	}

	mdss_res->mdp_irq_export = kcalloc(ARRAY_SIZE(mdp_intr_reg),
			sizeof(u32), GFP_KERNEL);
	if (mdss_res->mdp_irq_export == NULL) {
		kfree(mdss_res->mdp_irq_mask);
		kfree(mdss_res->mdp_irq_raw);
		mdss_res->mdp_irq_mask = NULL;
		mdss_res->mdp_irq_raw = NULL;
		return -ENOMEM;
	}

	/*
	 * If rotator is indicated as separate prior to 2.0, it means
	 * rotator block in WB0 & WB1 are serviced by external driver.
	 * In that case, specify WB0 & WB1 irq as export in mdp_irq_export;
	 * otherwise, set mdp_irq_export to zero will disable mdp irq
	 * export.
	 */
	if (mdss_res->has_separate_rotator &&
			(mdata->mdp_rev < MDSS_MDP_HW_REV_200))
		mdss_res->mdp_irq_export[0] = MDSS_MDP_INTR_WB_0_DONE |
						MDSS_MDP_INTR_WB_1_DONE;

	pr_info("mdss version = 0x%x, bootloader display is %s, num %d, intf_sel=0x%08x\n",
		mdata->mdp_rev, num_of_display_on ? "on" : "off",
		num_of_display_on, intf_sel);

probe_done:
	if (IS_ERR_VALUE(rc)) {
		if (!num_of_display_on)
			mdss_mdp_footswitch_ctrl_splash(false);

		if (mdata->regulator_notif_register)
			regulator_unregister_notifier(mdata->fs,
						&(mdata->gdsc_cb));
		mdss_mdp_hw.ptr = NULL;
		mdss_mdp_pp_term(&pdev->dev);
		mutex_destroy(&mdata->reg_lock);
		mdss_res = NULL;
	}

	return rc;
}

static void mdss_mdp_parse_dt_regs_array(const u32 *arr, struct dss_io_data *io,
	struct mdss_hw_settings *hws, int count)
{
	u32 len, reg;
	int i;

	if (!arr)
		return;

	for (i = 0, len = count * 2; i < len; i += 2) {
		reg = be32_to_cpu(arr[i]);
		if (reg >= io->len)
			continue;

		hws->reg = io->base + reg;
		hws->val = be32_to_cpu(arr[i + 1]);
		pr_debug("reg: 0x%04x=0x%08x\n", reg, hws->val);
		hws++;
	}
}

int mdss_mdp_parse_dt_hw_settings(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	struct mdss_hw_settings *hws;
	const u32 *vbif_arr, *mdp_arr, *vbif_nrt_arr;
	int vbif_len, mdp_len, vbif_nrt_len;

	vbif_arr = of_get_property(pdev->dev.of_node, "qcom,vbif-settings",
			&vbif_len);
	if (!vbif_arr || (vbif_len & 1)) {
		pr_debug("MDSS VBIF settings not found\n");
		vbif_len = 0;
	}
	vbif_len /= 2 * sizeof(u32);

	vbif_nrt_arr = of_get_property(pdev->dev.of_node,
				"qcom,vbif-nrt-settings", &vbif_nrt_len);
	if (!vbif_nrt_arr || (vbif_nrt_len & 1)) {
		pr_debug("MDSS VBIF non-realtime settings not found\n");
		vbif_nrt_len = 0;
	}
	vbif_nrt_len /= 2 * sizeof(u32);

	mdp_arr = of_get_property(pdev->dev.of_node, "qcom,mdp-settings",
			&mdp_len);
	if (!mdp_arr || (mdp_len & 1)) {
		pr_debug("MDSS MDP settings not found\n");
		mdp_len = 0;
	}
	mdp_len /= 2 * sizeof(u32);

	if (!(mdp_len + vbif_len + vbif_nrt_len))
		return 0;

	hws = devm_kzalloc(&pdev->dev, sizeof(*hws) * (vbif_len + mdp_len +
			vbif_nrt_len + 1), GFP_KERNEL);
	if (!hws)
		return -ENOMEM;

	mdss_mdp_parse_dt_regs_array(vbif_arr, &mdata->vbif_io,
			hws, vbif_len);
	mdss_mdp_parse_dt_regs_array(vbif_nrt_arr, &mdata->vbif_nrt_io,
			hws, vbif_nrt_len);
	mdss_mdp_parse_dt_regs_array(mdp_arr, &mdata->mdss_io,
		hws + vbif_len, mdp_len);

	mdata->hw_settings = hws;

	return 0;
}

static int mdss_mdp_parse_dt(struct platform_device *pdev)
{
	int rc, data;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	rc = mdss_mdp_parse_dt_hw_settings(pdev);
	if (rc) {
		pr_err("Error in device tree : hw settings\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_pipe(pdev);
	if (rc) {
		pr_err("Error in device tree : pipes\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_mixer(pdev);
	if (rc) {
		pr_err("Error in device tree : mixers\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_misc(pdev);
	if (rc) {
		pr_err("Error in device tree : misc\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_wb(pdev);
	if (rc) {
		pr_err("Error in device tree : wb\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_ctl(pdev);
	if (rc) {
		pr_err("Error in device tree : ctl\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_video_intf(pdev);
	if (rc) {
		pr_err("Error in device tree : ctl\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_smp(pdev);
	if (rc) {
		pr_err("Error in device tree : smp\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_prefill(pdev);
	if (rc) {
		pr_err("Error in device tree : prefill\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_ad_cfg(pdev);
	if (rc) {
		pr_err("Error in device tree : ad\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_cdm(pdev);
	if (rc)
		pr_debug("CDM offset not found in device tree\n");

	rc = mdss_mdp_parse_dt_dsc(pdev);
	if (rc)
		pr_debug("DSC offset not found in device tree\n");

	/* Parse the mdp specific register base offset*/
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-mdp-reg-offset", &data);
	if (rc) {
		pr_err("Error in device tree : mdp reg base\n");
		return rc;
	}
	mdata->mdp_base = mdata->mdss_io.base + data;
	return 0;
}

static void mdss_mdp_parse_dt_pipe_sw_reset(struct platform_device *pdev,
	u32 reg_off, char *prop_name, struct mdss_mdp_pipe *pipe_list,
	u32 npipes)
{
	int len;
	const u32 *arr;

	arr = of_get_property(pdev->dev.of_node, prop_name, &len);
	if (arr) {
		int i;

		len /= sizeof(u32);
		if (len != npipes) {
			pr_err("%s: invalid sw_reset entries req:%d found:%d\n",
				prop_name, len, npipes);
			return;
		}

		for (i = 0; i < len; i++) {
			pipe_list[i].sw_reset.reg_off = reg_off;
			pipe_list[i].sw_reset.bit_off = be32_to_cpu(arr[i]);

			pr_debug("%s[%d]: sw_reset: reg_off:0x%x bit_off:%d\n",
				prop_name, i, reg_off, be32_to_cpu(arr[i]));
		}
	}
}

static int  mdss_mdp_parse_dt_pipe_clk_ctrl(struct platform_device *pdev,
	char *prop_name, struct mdss_mdp_pipe *pipe_list, u32 npipes)
{
	int rc = 0, len;
	const u32 *arr;

	arr = of_get_property(pdev->dev.of_node, prop_name, &len);
	if (arr) {
		int i, j;
		len /= sizeof(u32);
		for (i = 0, j = 0; i < len; j++) {
			struct mdss_mdp_pipe *pipe = NULL;

			if (j >= npipes) {
				pr_err("invalid clk ctrl enries for prop: %s\n",
					prop_name);
				return -EINVAL;
			}

			pipe = &pipe_list[j];

			pipe->clk_ctrl.reg_off = be32_to_cpu(arr[i++]);
			pipe->clk_ctrl.bit_off = be32_to_cpu(arr[i++]);

			/* status register is next in line to ctrl register */
			pipe->clk_status.reg_off = pipe->clk_ctrl.reg_off + 4;
			pipe->clk_status.bit_off = be32_to_cpu(arr[i++]);

			pr_debug("%s[%d]: ctrl: reg_off: 0x%x bit_off: %d\n",
				prop_name, j, pipe->clk_ctrl.reg_off,
				pipe->clk_ctrl.bit_off);
			pr_debug("%s[%d]: status: reg_off: 0x%x bit_off: %d\n",
				prop_name, j, pipe->clk_status.reg_off,
				pipe->clk_status.bit_off);
		}
		if (j != npipes) {
			pr_err("%s: %d entries found. required %d\n",
				prop_name, j, npipes);
			for (i = 0; i < npipes; i++) {
				memset(&pipe_list[i].clk_ctrl, 0,
					sizeof(pipe_list[i].clk_ctrl));
				memset(&pipe_list[i].clk_status, 0,
					sizeof(pipe_list[i].clk_status));
			}
			rc = -EINVAL;
		}
	} else {
		pr_err("error mandatory property '%s' not found\n", prop_name);
		rc = -EINVAL;
	}

	return rc;
}

static void mdss_mdp_parse_dt_pipe_panic_ctrl(struct platform_device *pdev,
	char *prop_name, struct mdss_mdp_pipe *pipe_list, u32 npipes)
{
	int i, j;
	int len;
	const u32 *arr;
	struct mdss_mdp_pipe *pipe = NULL;

	arr = of_get_property(pdev->dev.of_node, prop_name, &len);
	if (arr) {
		len /= sizeof(u32);
		for (i = 0, j = 0; i < len; j++) {
			if (j >= npipes) {
				pr_err("invalid panic ctrl enries for prop: %s\n",
					prop_name);
				return;
			}

			pipe = &pipe_list[j];
			pipe->panic_ctrl_ndx = be32_to_cpu(arr[i++]);
		}
		if (j != npipes)
			pr_err("%s: %d entries found. required %d\n",
				prop_name, j, npipes);
	} else {
		pr_debug("panic ctrl enabled but property '%s' not found\n",
								prop_name);
	}
}

static int mdss_mdp_parse_dt_pipe_helper(struct platform_device *pdev,
		u32 ptype, char *ptypestr,
		struct mdss_mdp_pipe **out_plist,
		size_t len,
		u8 priority_base)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 offsets[MDSS_MDP_MAX_SSPP];
	u32 ftch_id[MDSS_MDP_MAX_SSPP];
	u32 xin_id[MDSS_MDP_MAX_SSPP];
	u32 pnums[MDSS_MDP_MAX_SSPP];
	struct mdss_mdp_pipe *pipe_list;
	char prop_name[64];
	int i, cnt, rc;
	u32 rects_per_sspp;

	if (!out_plist)
		return -EINVAL;

	for (i = 0, cnt = 0; i < MDSS_MDP_MAX_SSPP && cnt < len; i++) {
		if (ptype == get_pipe_type_from_num(i)) {
			pnums[cnt] = i;
			cnt++;
		}
	}

	if (cnt < len)
		pr_warn("Invalid %s pipe count: %zu, max supported: %d\n",
				ptypestr, len, cnt);
	if (cnt == 0) {
		*out_plist = NULL;

		return 0;
	}

	/* by default works in single rect mode unless otherwise noted */
	rects_per_sspp = mdata->rects_per_sspp[ptype] ? : 1;

	pipe_list = devm_kzalloc(&pdev->dev,
			(sizeof(struct mdss_mdp_pipe) * cnt * rects_per_sspp),
			GFP_KERNEL);
	if (!pipe_list)
		return -ENOMEM;

	if (mdata->has_pixel_ram || (ptype == MDSS_MDP_PIPE_TYPE_CURSOR)) {
		for (i = 0; i < cnt; i++)
			ftch_id[i] = -1;
	} else {
		snprintf(prop_name, sizeof(prop_name),
				"qcom,mdss-pipe-%s-fetch-id", ptypestr);
		rc = mdss_mdp_parse_dt_handler(pdev, prop_name, ftch_id,
				cnt);
		if (rc)
			goto parse_fail;
	}

	snprintf(prop_name, sizeof(prop_name),
			"qcom,mdss-pipe-%s-xin-id", ptypestr);
	rc = mdss_mdp_parse_dt_handler(pdev, prop_name, xin_id, cnt);
	if (rc)
		goto parse_fail;

	snprintf(prop_name, sizeof(prop_name),
			"qcom,mdss-pipe-%s-off", ptypestr);
	rc = mdss_mdp_parse_dt_handler(pdev, prop_name, offsets, cnt);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_pipe_addr_setup(mdata, pipe_list, offsets, ftch_id,
			xin_id, ptype, pnums, cnt, rects_per_sspp,
			priority_base);
	if (rc)
		goto parse_fail;

	snprintf(prop_name, sizeof(prop_name),
			"qcom,mdss-pipe-%s-clk-ctrl-offsets", ptypestr);
	rc = mdss_mdp_parse_dt_pipe_clk_ctrl(pdev, prop_name,
			pipe_list, cnt);
	if (rc)
		goto parse_fail;

	*out_plist = pipe_list;

	return cnt;
parse_fail:
	devm_kfree(&pdev->dev, pipe_list);

	return rc;
}

static int mdss_mdp_parse_dt_pipe(struct platform_device *pdev)
{
	int rc = 0;
	u32 nfids = 0, len, nxids = 0, npipes = 0;
	u32 sw_reset_offset = 0;
	u32 data[4];

	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->has_pixel_ram = !mdss_mdp_parse_dt_prop_len(pdev,
						"qcom,mdss-smp-data");

	mdata->nvig_pipes = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-vig-off");
	mdata->nrgb_pipes = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-rgb-off");
	mdata->ndma_pipes = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-dma-off");
	mdata->ncursor_pipes = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-cursor-off");

	npipes = mdata->nvig_pipes + mdata->nrgb_pipes + mdata->ndma_pipes;

	if (!mdata->has_pixel_ram) {
		nfids  += mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-vig-fetch-id");
		nfids  += mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-rgb-fetch-id");
		nfids  += mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-dma-fetch-id");
		if (npipes != nfids) {
			pr_err("device tree err: unequal number of pipes and smp ids");
			return -EINVAL;
		}
	}

	if (mdata->nvig_pipes)
		nxids += mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-vig-xin-id");
	if (mdata->nrgb_pipes)
		nxids += mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-rgb-xin-id");
	if (mdata->ndma_pipes)
		nxids += mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-dma-xin-id");
	if (npipes != nxids) {
		pr_err("device tree err: unequal number of pipes and xin ids\n");
		return -EINVAL;
	}

	rc = mdss_mdp_parse_dt_pipe_helper(pdev, MDSS_MDP_PIPE_TYPE_VIG, "vig",
			&mdata->vig_pipes, mdata->nvig_pipes, 0);
	if (IS_ERR_VALUE(rc))
		goto parse_fail;
	mdata->nvig_pipes = rc;

	rc = mdss_mdp_parse_dt_pipe_helper(pdev, MDSS_MDP_PIPE_TYPE_RGB, "rgb",
			&mdata->rgb_pipes, mdata->nrgb_pipes,
			mdata->nvig_pipes);
	if (IS_ERR_VALUE(rc))
		goto parse_fail;
	mdata->nrgb_pipes = rc;

	rc = mdss_mdp_parse_dt_pipe_helper(pdev, MDSS_MDP_PIPE_TYPE_DMA, "dma",
			&mdata->dma_pipes, mdata->ndma_pipes,
			mdata->nvig_pipes + mdata->nrgb_pipes);
	if (IS_ERR_VALUE(rc))
		goto parse_fail;
	mdata->ndma_pipes = rc;

	rc = mdss_mdp_parse_dt_pipe_helper(pdev, MDSS_MDP_PIPE_TYPE_CURSOR,
			"cursor", &mdata->cursor_pipes, mdata->ncursor_pipes,
			0);
	if (IS_ERR_VALUE(rc))
		goto parse_fail;
	mdata->ncursor_pipes = rc;

	rc = 0;

	mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-sw-reset-off",
		&sw_reset_offset, 1);
	if (sw_reset_offset) {
		if (mdata->vig_pipes)
			mdss_mdp_parse_dt_pipe_sw_reset(pdev, sw_reset_offset,
				"qcom,mdss-pipe-vig-sw-reset-map",
				mdata->vig_pipes, mdata->nvig_pipes);
		if (mdata->rgb_pipes)
			mdss_mdp_parse_dt_pipe_sw_reset(pdev, sw_reset_offset,
				"qcom,mdss-pipe-rgb-sw-reset-map",
				mdata->rgb_pipes, mdata->nrgb_pipes);
		if (mdata->dma_pipes)
			mdss_mdp_parse_dt_pipe_sw_reset(pdev, sw_reset_offset,
				"qcom,mdss-pipe-dma-sw-reset-map",
				mdata->dma_pipes, mdata->ndma_pipes);
	}

	mdata->has_panic_ctrl = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-panic-ctrl");
	if (mdata->has_panic_ctrl) {
		if (mdata->vig_pipes)
			mdss_mdp_parse_dt_pipe_panic_ctrl(pdev,
				"qcom,mdss-pipe-vig-panic-ctrl-offsets",
				mdata->vig_pipes, mdata->nvig_pipes);
		if (mdata->rgb_pipes)
			mdss_mdp_parse_dt_pipe_panic_ctrl(pdev,
				"qcom,mdss-pipe-rgb-panic-ctrl-offsets",
				mdata->rgb_pipes, mdata->nrgb_pipes);
		if (mdata->dma_pipes)
			mdss_mdp_parse_dt_pipe_panic_ctrl(pdev,
				"qcom,mdss-pipe-dma-panic-ctrl-offsets",
				mdata->dma_pipes, mdata->ndma_pipes);
	}

	len = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-per-pipe-panic-luts");
	if (len != 4) {
		pr_debug("Unable to read per-pipe-panic-luts\n");
	} else {
		rc = mdss_mdp_parse_dt_handler(pdev,
			"qcom,mdss-per-pipe-panic-luts", data, len);
		mdata->default_panic_lut_per_pipe_linear = data[0];
		mdata->default_panic_lut_per_pipe_tile = data[1];
		mdata->default_robust_lut_per_pipe_linear = data[2];
		mdata->default_robust_lut_per_pipe_tile = data[3];
		pr_debug("per pipe panic lut [0]:0x%x [1]:0x%x [2]:0x%x [3]:0x%x\n",
			data[0], data[1], data[2], data[3]);
	}

parse_fail:
	return rc;
}

static int mdss_mdp_parse_dt_mixer(struct platform_device *pdev)
{

	u32 nmixers, npingpong;
	int rc = 0;
	u32 *mixer_offsets = NULL, *dspp_offsets = NULL,
	    *pingpong_offsets = NULL;
	u32 is_virtual_mixer_req = false;

	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->nmixers_intf = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-mixer-intf-off");
	mdata->nmixers_wb = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-mixer-wb-off");
	mdata->ndspp = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-dspp-off");
	npingpong = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pingpong-off");
	nmixers = mdata->nmixers_intf + mdata->nmixers_wb;

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,max-mixer-width", &mdata->max_mixer_width);
	if (rc) {
		pr_err("device tree err: failed to get max mixer width\n");
		return -EINVAL;
	}

	if (mdata->nmixers_intf < mdata->ndspp) {
		pr_err("device tree err: no of dspp are greater than intf mixers\n");
		return -EINVAL;
	}

	if (mdata->nmixers_intf != npingpong) {
		pr_err("device tree err: unequal no of pingpong and intf mixers\n");
		return -EINVAL;
	}

	mixer_offsets = kzalloc(sizeof(u32) * nmixers, GFP_KERNEL);
	if (!mixer_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		return -ENOMEM;
	}

	dspp_offsets = kzalloc(sizeof(u32) * mdata->ndspp, GFP_KERNEL);
	if (!dspp_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		rc = -ENOMEM;
		goto dspp_alloc_fail;
	}
	pingpong_offsets = kzalloc(sizeof(u32) * npingpong, GFP_KERNEL);
	if (!pingpong_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		rc = -ENOMEM;
		goto pingpong_alloc_fail;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-mixer-intf-off",
		mixer_offsets, mdata->nmixers_intf);
	if (rc)
		goto parse_done;

	mdata->has_separate_rotator = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-separate-rotator");
	if (mdata->nmixers_wb) {
		rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-mixer-wb-off",
				mixer_offsets + mdata->nmixers_intf,
				mdata->nmixers_wb);
		if (rc)
			goto parse_done;
	} else if (!mdata->has_separate_rotator) {
		/*
		 * If writeback mixers are not available, put the number of
		 * writeback mixers equal to number of DMA pipes so that
		 * later same number of virtual writeback mixers can be
		 * allocated.
		 */
		mdata->nmixers_wb = mdata->ndma_pipes;
		is_virtual_mixer_req = true;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-dspp-off",
		dspp_offsets, mdata->ndspp);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pingpong-off",
		pingpong_offsets, npingpong);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_mixer_addr_setup(mdata, mixer_offsets,
			dspp_offsets, pingpong_offsets,
			MDSS_MDP_MIXER_TYPE_INTF, mdata->nmixers_intf);
	if (rc)
		goto parse_done;

	if (mdata->nmixers_wb) {
		if (is_virtual_mixer_req) {
			/*
			 * Replicate last interface mixers based on number of
			 * dma pipes available as virtual writeback mixers.
			 */
			rc = mdss_mdp_mixer_addr_setup(mdata, mixer_offsets +
				mdata->nmixers_intf - mdata->ndma_pipes,
				NULL, NULL, MDSS_MDP_MIXER_TYPE_WRITEBACK,
				mdata->nmixers_wb);
			if (rc)
				goto parse_done;
		} else {
			rc = mdss_mdp_mixer_addr_setup(mdata, mixer_offsets +
				mdata->nmixers_intf, NULL, NULL,
				MDSS_MDP_MIXER_TYPE_WRITEBACK,
				mdata->nmixers_wb);
			if (rc)
				goto parse_done;
		}
	}

parse_done:
	kfree(pingpong_offsets);
pingpong_alloc_fail:
	kfree(dspp_offsets);
dspp_alloc_fail:
	kfree(mixer_offsets);

	return rc;
}

static int mdss_mdp_cdm_addr_setup(struct mdss_data_type *mdata,
				   u32 *cdm_offsets, u32 len)
{
	struct mdss_mdp_cdm *head;
	u32 i = 0;

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_cdm) *
				len, GFP_KERNEL);
	if (!head) {
		pr_err("%s: no memory for CDM info\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].num = i;
		head[i].base = (mdata->mdss_io.base) + cdm_offsets[i];
		atomic_set(&head[i].kref.refcount, 0);
		mutex_init(&head[i].lock);
		init_completion(&head[i].free_comp);
		pr_debug("%s: cdm off (%d) = %pK\n", __func__, i, head[i].base);
	}

	mdata->cdm_off = head;
	mutex_init(&mdata->cdm_lock);
	return 0;
}

static int mdss_mdp_parse_dt_cdm(struct platform_device *pdev)
{
	int rc = 0;
	u32 *cdm_offsets = NULL;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->ncdm = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-cdm-off");

	if (!mdata->ncdm) {
		rc = 0;
		pr_debug("%s: No CDM offsets present in DT\n", __func__);
		goto end;
	}
	pr_debug("%s: cdm len == %d\n", __func__, mdata->ncdm);
	cdm_offsets = kzalloc(sizeof(u32) * mdata->ncdm, GFP_KERNEL);
	if (!cdm_offsets) {
		pr_err("no more memory for cdm offsets\n");
		rc = -ENOMEM;
		mdata->ncdm = 0;
		goto end;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-cdm-off", cdm_offsets,
				       mdata->ncdm);
	if (rc) {
		pr_err("device tree err: failed to get cdm offsets\n");
		goto fail;
	}

	rc = mdss_mdp_cdm_addr_setup(mdata, cdm_offsets, mdata->ncdm);
	if (rc) {
		pr_err("%s: CDM address setup failed\n", __func__);
		goto fail;
	}

fail:
	kfree(cdm_offsets);
	if (rc)
		mdata->ncdm = 0;
end:
	return rc;
}

static int mdss_mdp_dsc_addr_setup(struct mdss_data_type *mdata,
				   u32 *dsc_offsets, u32 len)
{
	struct mdss_mdp_dsc *head;
	u32 i = 0;

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_dsc) *
				len, GFP_KERNEL);
	if (!head) {
		pr_err("no memory for DSC info\n");
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].num = i;
		head[i].base = (mdata->mdss_io.base) + dsc_offsets[i];
		pr_debug("dsc off (%d) = %pK\n", i, head[i].base);
	}

	mdata->dsc_off = head;
	return 0;
}

static int mdss_mdp_parse_dt_dsc(struct platform_device *pdev)
{
	int rc = 0;
	u32 *dsc_offsets = NULL;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->ndsc = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-dsc-off");
	if (!mdata->ndsc) {
		rc = 0;
		pr_debug("No DSC offsets present in DT\n");
		goto end;
	}
	pr_debug("dsc len == %d\n", mdata->ndsc);

	dsc_offsets = kzalloc(sizeof(u32) * mdata->ndsc, GFP_KERNEL);
	if (!dsc_offsets) {
		pr_err("no more memory for dsc offsets\n");
		rc = -ENOMEM;
		mdata->ndsc = 0;
		goto end;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-dsc-off", dsc_offsets,
				       mdata->ndsc);
	if (rc) {
		pr_err("device tree err: failed to get cdm offsets\n");
		goto fail;
	}

	rc = mdss_mdp_dsc_addr_setup(mdata, dsc_offsets, mdata->ndsc);
	if (rc) {
		pr_err("%s: DSC address setup failed\n", __func__);
		goto fail;
	}

fail:
	kfree(dsc_offsets);
	if (rc)
		mdata->ndsc = 0;
end:
	return rc;
}

static int mdss_mdp_parse_dt_wb(struct platform_device *pdev)
{
	int rc = 0;
	u32 *wb_offsets = NULL;
	u32 num_wb_mixer, nwb_offsets, num_intf_wb = 0;
	const char *wfd_data;
	struct mdss_data_type *mdata;

	mdata = platform_get_drvdata(pdev);

	num_wb_mixer = mdata->nmixers_wb;

	wfd_data = of_get_property(pdev->dev.of_node,
					"qcom,mdss-wfd-mode", NULL);
	if (wfd_data && strcmp(wfd_data, "shared") != 0)
		num_intf_wb = 1;

	nwb_offsets =  mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-wb-off");

	wb_offsets = kzalloc(sizeof(u32) * nwb_offsets, GFP_KERNEL);
	if (!wb_offsets) {
		pr_err("no more mem for writeback offsets\n");
		return -ENOMEM;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-wb-off",
		wb_offsets, nwb_offsets);
	if (rc)
		goto wb_parse_done;

	rc = mdss_mdp_wb_addr_setup(mdata, num_wb_mixer, num_intf_wb);
	if (rc)
		goto wb_parse_done;

	mdata->nwb_offsets = nwb_offsets;
	mdata->wb_offsets = wb_offsets;

	return 0;

wb_parse_done:
	kfree(wb_offsets);
	return rc;
}

static int mdss_mdp_parse_dt_ctl(struct platform_device *pdev)
{
	int rc = 0;
	u32 *ctl_offsets = NULL;

	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->nctl = mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-ctl-off");

	if (mdata->nctl < mdata->nwb) {
		pr_err("device tree err: number of ctl greater than wb\n");
		rc = -EINVAL;
		goto parse_done;
	}

	ctl_offsets = kzalloc(sizeof(u32) * mdata->nctl, GFP_KERNEL);
	if (!ctl_offsets) {
		pr_err("no more mem for ctl offsets\n");
		return -ENOMEM;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-ctl-off",
		ctl_offsets, mdata->nctl);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_ctl_addr_setup(mdata, ctl_offsets, mdata->nctl);
	if (rc)
		goto parse_done;

parse_done:
	kfree(ctl_offsets);

	return rc;
}

static int mdss_mdp_parse_dt_video_intf(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 count;
	u32 *offsets;
	int rc;


	count = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-intf-off");
	if (count == 0)
		return -EINVAL;

	offsets = kzalloc(sizeof(u32) * count, GFP_KERNEL);
	if (!offsets) {
		pr_err("no mem assigned for video intf\n");
		return -ENOMEM;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-intf-off",
			offsets, count);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_video_addr_setup(mdata, offsets, count);
	if (rc)
		pr_err("unable to setup video interfaces\n");

parse_fail:
	kfree(offsets);

	return rc;
}

static int mdss_mdp_update_smp_map(struct platform_device *pdev,
		const u32 *data, int len, int pipe_cnt,
		struct mdss_mdp_pipe *pipes)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	int i, j, k;
	u32 cnt, mmb;

	len /= sizeof(u32);
	for (i = 0, k = 0; i < len; k++) {
		struct mdss_mdp_pipe *pipe = NULL;

		if (k >= pipe_cnt) {
			pr_err("invalid fixed mmbs\n");
			return -EINVAL;
		}

		pipe = &pipes[k];

		cnt = be32_to_cpu(data[i++]);
		if (cnt == 0)
			continue;

		for (j = 0; j < cnt; j++) {
			mmb = be32_to_cpu(data[i++]);
			if (mmb > mdata->smp_mb_cnt) {
				pr_err("overflow mmb:%d pipe:%d: max:%d\n",
						mmb, k, mdata->smp_mb_cnt);
				return -EINVAL;
			}
			set_bit(mmb, pipe->smp_map[0].fixed);
		}
		if (bitmap_intersects(pipe->smp_map[0].fixed,
					mdata->mmb_alloc_map,
					mdata->smp_mb_cnt)) {
			pr_err("overlapping fixed mmb map\n");
			return -EINVAL;
		}
		bitmap_or(mdata->mmb_alloc_map, pipe->smp_map[0].fixed,
				mdata->mmb_alloc_map, mdata->smp_mb_cnt);
	}
	return 0;
}

static int mdss_mdp_parse_dt_smp(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 num;
	u32 data[2];
	int rc, len;
	const u32 *arr;

	num = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-smp-data");
	/*
	 * This property is optional for targets with fix pixel ram. Rest
	 * must provide no. of smp and size of each block.
	 */
	if (!num)
		return 0;
	else if (num != 2)
		return -EINVAL;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-smp-data", data, num);
	if (rc)
		return rc;

	rc = mdss_mdp_smp_setup(mdata, data[0], data[1]);

	if (rc) {
		pr_err("unable to setup smp data\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-smp-mb-per-pipe", data);
	mdata->smp_mb_per_pipe = (!rc ? data[0] : 0);

	rc = 0;
	arr = of_get_property(pdev->dev.of_node,
			"qcom,mdss-pipe-rgb-fixed-mmb", &len);
	if (arr) {
		rc = mdss_mdp_update_smp_map(pdev, arr, len,
				mdata->nrgb_pipes, mdata->rgb_pipes);

		if (rc)
			pr_warn("unable to update smp map for RGB pipes\n");
	}

	arr = of_get_property(pdev->dev.of_node,
			"qcom,mdss-pipe-vig-fixed-mmb", &len);
	if (arr) {
		rc = mdss_mdp_update_smp_map(pdev, arr, len,
				mdata->nvig_pipes, mdata->vig_pipes);

		if (rc)
			pr_warn("unable to update smp map for VIG pipes\n");
	}
	return rc;
}

static void mdss_mdp_parse_dt_fudge_factors(struct platform_device *pdev,
	char *prop_name, struct mult_factor *ff)
{
	int rc;
	u32 data[2] = {1, 1};

	rc = mdss_mdp_parse_dt_handler(pdev, prop_name, data, 2);
	if (rc) {
		pr_debug("err reading %s\n", prop_name);
	} else {
		ff->numer = data[0];
		ff->denom = data[1];
	}
}

static int mdss_mdp_parse_dt_prefill(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	int rc;

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-prefill-outstanding-buffer-bytes",
		&prefill->ot_bytes);
	if (rc) {
		pr_err("prefill outstanding buffer bytes not specified\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-prefill-y-buffer-bytes", &prefill->y_buf_bytes);
	if (rc) {
		pr_err("prefill y buffer bytes not specified\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-prefill-scaler-buffer-lines-bilinear",
		&prefill->y_scaler_lines_bilinear);
	if (rc) {
		pr_err("prefill scaler lines for bilinear not specified\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-prefill-scaler-buffer-lines-caf",
		&prefill->y_scaler_lines_caf);
	if (rc) {
		pr_debug("prefill scaler lines for caf not specified\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-prefill-post-scaler-buffer-pixels",
		&prefill->post_scaler_pixels);
	if (rc) {
		pr_err("prefill post scaler buffer pixels not specified\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-prefill-pingpong-buffer-pixels",
		&prefill->pp_pixels);
	if (rc) {
		pr_err("prefill pingpong buffer lines not specified\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-prefill-fbc-lines", &prefill->fbc_lines);
	if (rc)
		pr_debug("prefill FBC lines not specified\n");

	return 0;
}

static void mdss_mdp_parse_vbif_qos(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 npriority_lvl_nrt;
	int rc;

	mdata->npriority_lvl = mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-vbif-qos-rt-setting");
	if (mdata->npriority_lvl == MDSS_VBIF_QOS_REMAP_ENTRIES) {
		mdata->vbif_rt_qos = kzalloc(sizeof(u32) *
				mdata->npriority_lvl, GFP_KERNEL);
		if (!mdata->vbif_rt_qos) {
			pr_err("no memory for real time qos_priority\n");
			return;
		}

		rc = mdss_mdp_parse_dt_handler(pdev,
			"qcom,mdss-vbif-qos-rt-setting",
				mdata->vbif_rt_qos, mdata->npriority_lvl);
		if (rc) {
			pr_debug("rt setting not found\n");
			return;
		}
	} else {
		mdata->npriority_lvl = 0;
		pr_debug("Invalid or no vbif qos rt setting\n");
		return;
	}

	npriority_lvl_nrt = mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-vbif-qos-nrt-setting");

	if (!npriority_lvl_nrt) {
		pr_debug("no vbif nrt priorities found rt:%d\n",
			mdata->npriority_lvl);
		return;
	} else if (npriority_lvl_nrt != mdata->npriority_lvl) {
		/* driver expects same number for both nrt and rt */
		pr_err("invalid nrt settings nrt(%d) != rt(%d)\n",
			npriority_lvl_nrt, mdata->npriority_lvl);
		return;
	}

	if (mdata->npriority_lvl == MDSS_VBIF_QOS_REMAP_ENTRIES) {
		mdata->vbif_nrt_qos = kzalloc(sizeof(u32) *
				mdata->npriority_lvl, GFP_KERNEL);
		if (!mdata->vbif_nrt_qos) {
			pr_err("no memory for non real time qos_priority\n");
			return;
		}

		rc = mdss_mdp_parse_dt_handler(pdev,
			"qcom,mdss-vbif-qos-nrt-setting", mdata->vbif_nrt_qos,
				mdata->npriority_lvl);
		if (rc) {
			pr_debug("nrt setting not found\n");
			return;
		}
	} else {
		mdata->npriority_lvl = 0;
		pr_debug("Invalid or no vbif qos nrt setting\n");
	}
}

static void mdss_mdp_parse_max_bw_array(const u32 *arr,
		struct mdss_max_bw_settings *max_bw_settings, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		max_bw_settings->mdss_max_bw_mode = be32_to_cpu(arr[i*2]);
		max_bw_settings->mdss_max_bw_val = be32_to_cpu(arr[(i*2)+1]);
		max_bw_settings++;
	}
}

static void mdss_mdp_parse_max_bandwidth(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	struct mdss_max_bw_settings *max_bw_settings;
	int max_bw_settings_cnt = 0;
	const u32 *max_bw;

	max_bw = of_get_property(pdev->dev.of_node, "qcom,max-bw-settings",
			&max_bw_settings_cnt);

	if (!max_bw || !max_bw_settings_cnt) {
		pr_debug("MDSS max bandwidth settings not found\n");
		return;
	}

	max_bw_settings_cnt /= 2 * sizeof(u32);

	max_bw_settings = devm_kzalloc(&pdev->dev, sizeof(*max_bw_settings)
			* max_bw_settings_cnt, GFP_KERNEL);
	if (!max_bw_settings) {
		pr_err("Memory allocation failed for max_bw_settings\n");
		return;
	}

	mdss_mdp_parse_max_bw_array(max_bw, max_bw_settings,
			max_bw_settings_cnt);

	mdata->max_bw_settings = max_bw_settings;
	mdata->max_bw_settings_cnt = max_bw_settings_cnt;
}

static void mdss_mdp_parse_per_pipe_bandwidth(struct platform_device *pdev)
{

	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	struct mdss_max_bw_settings *max_bw_per_pipe_settings;
	int max_bw_settings_cnt = 0;
	const u32 *max_bw_settings;
	u32 max_bw, min_bw, threshold, i = 0;

	max_bw_settings = of_get_property(pdev->dev.of_node,
			"qcom,max-bandwidth-per-pipe-kbps",
			&max_bw_settings_cnt);

	if (!max_bw_settings || !max_bw_settings_cnt) {
		pr_debug("MDSS per pipe max bandwidth settings not found\n");
		return;
	}

	/* Support targets where a common per pipe max bw is provided */
	if ((max_bw_settings_cnt / sizeof(u32)) == 1) {
		mdata->max_bw_per_pipe = be32_to_cpu(max_bw_settings[0]);
		mdata->max_per_pipe_bw_settings = NULL;
		pr_debug("Common per pipe max bandwidth provided\n");
		return;
	}

	max_bw_settings_cnt /= 2 * sizeof(u32);

	max_bw_per_pipe_settings = devm_kzalloc(&pdev->dev,
		    sizeof(struct mdss_max_bw_settings) * max_bw_settings_cnt,
		    GFP_KERNEL);
	if (!max_bw_per_pipe_settings) {
		pr_err("Memory allocation failed for max_bw_settings\n");
		return;
	}

	mdss_mdp_parse_max_bw_array(max_bw_settings, max_bw_per_pipe_settings,
					max_bw_settings_cnt);
	mdata->max_per_pipe_bw_settings = max_bw_per_pipe_settings;
	mdata->mdss_per_pipe_bw_cnt = max_bw_settings_cnt;

	/* Calculate min and max allowed per pipe BW */
	min_bw = mdata->max_bw_high;
	max_bw = 0;

	while (i < max_bw_settings_cnt) {
		threshold = mdata->max_per_pipe_bw_settings[i].mdss_max_bw_val;
		if (threshold > max_bw)
			max_bw = threshold;
		if (threshold < min_bw)
			min_bw = threshold;
		++i;
	}
	mdata->max_bw_per_pipe = max_bw;
	mdata->min_bw_per_pipe = min_bw;
}

static int mdss_mdp_parse_dt_misc(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 data, slave_pingpong_off;
	const char *wfd_data;
	int rc;
	struct property *prop = NULL;

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,mdss-rot-block-size",
		&data);
	mdata->rot_block_size = (!rc ? data : 128);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-default-ot-rd-limit", &data);
	mdata->default_ot_rd_limit = (!rc ? data : 0);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-default-ot-wr-limit", &data);
	mdata->default_ot_wr_limit = (!rc ? data : 0);

	mdata->has_non_scalar_rgb = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-non-scalar-rgb");
	mdata->has_bwc = of_property_read_bool(pdev->dev.of_node,
					       "qcom,mdss-has-bwc");
	mdata->has_decimation = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-decimation");
	mdata->has_no_lut_read = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-no-lut-read");
	mdata->needs_hist_vote = !(of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-no-hist-vote"));
	wfd_data = of_get_property(pdev->dev.of_node,
					"qcom,mdss-wfd-mode", NULL);
	if (wfd_data) {
		pr_debug("wfd mode: %s\n", wfd_data);
		if (!strcmp(wfd_data, "intf")) {
			mdata->wfd_mode = MDSS_MDP_WFD_INTERFACE;
		} else if (!strcmp(wfd_data, "shared")) {
			mdata->wfd_mode = MDSS_MDP_WFD_SHARED;
		} else if (!strcmp(wfd_data, "dedicated")) {
			mdata->wfd_mode = MDSS_MDP_WFD_DEDICATED;
		} else {
			pr_debug("wfd default mode: Shared\n");
			mdata->wfd_mode = MDSS_MDP_WFD_SHARED;
		}
	} else {
		pr_warn("wfd mode not configured. Set to default: Shared\n");
		mdata->wfd_mode = MDSS_MDP_WFD_SHARED;
	}

	mdata->has_src_split = of_property_read_bool(pdev->dev.of_node,
		 "qcom,mdss-has-source-split");
	mdata->has_fixed_qos_arbiter_enabled =
			of_property_read_bool(pdev->dev.of_node,
		 "qcom,mdss-has-fixed-qos-arbiter-enabled");
	mdata->idle_pc_enabled = of_property_read_bool(pdev->dev.of_node,
		 "qcom,mdss-idle-power-collapse-enabled");

	prop = of_find_property(pdev->dev.of_node, "batfet-supply", NULL);
	mdata->batfet_required = prop ? true : false;
	mdata->en_svs_high = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-en-svs-high");
	if (!mdata->en_svs_high)
		pr_debug("%s: svs_high is not enabled\n", __func__);
	rc = of_property_read_u32(pdev->dev.of_node,
		 "qcom,mdss-highest-bank-bit", &(mdata->highest_bank_bit));
	if (rc)
		pr_debug("Could not read optional property: highest bank bit\n");

	mdata->has_pingpong_split = of_property_read_bool(pdev->dev.of_node,
		 "qcom,mdss-has-pingpong-split");

	if (mdata->has_pingpong_split) {
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,mdss-slave-pingpong-off",
				&slave_pingpong_off);
		if (rc) {
			pr_err("Error in device tree: slave pingpong offset\n");
			return rc;
		}
		mdata->slave_pingpong_base = mdata->mdss_io.base +
			slave_pingpong_off;
		rc = mdss_mdp_parse_dt_ppb_off(pdev);
		if (rc) {
			pr_err("Error in device tree: ppb offset not configured\n");
			return rc;
		}
	}

	/*
	 * 2x factor on AB because bus driver will divide by 2
	 * due to 2x ports to BIMC
	 */
	mdata->ab_factor.numer = 2;
	mdata->ab_factor.denom = 1;
	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-ab-factor",
		&mdata->ab_factor);

	/*
	 * 1.2 factor on ib as default value. This value is
	 * experimentally determined and should be tuned in device
	 * tree.
	 */
	mdata->ib_factor.numer = 6;
	mdata->ib_factor.denom = 5;
	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-ib-factor",
		&mdata->ib_factor);

	/*
	 * Set overlap ib value equal to ib by default. This value can
	 * be tuned in device tree to be different from ib.
	 * This factor apply when the max bandwidth per pipe
	 * is the overlap BW.
	 */
	mdata->ib_factor_overlap.numer = mdata->ib_factor.numer;
	mdata->ib_factor_overlap.denom = mdata->ib_factor.denom;
	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-ib-factor-overlap",
		&mdata->ib_factor_overlap);

	mdata->clk_factor.numer = 1;
	mdata->clk_factor.denom = 1;
	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-clk-factor",
		&mdata->clk_factor);

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,max-bandwidth-low-kbps", &mdata->max_bw_low);
	if (rc)
		pr_debug("max bandwidth (low) property not specified\n");

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,max-bandwidth-high-kbps", &mdata->max_bw_high);
	if (rc)
		pr_debug("max bandwidth (high) property not specified\n");

	mdss_mdp_parse_per_pipe_bandwidth(pdev);

	mdss_mdp_parse_max_bandwidth(pdev);

	mdata->nclk_lvl = mdss_mdp_parse_dt_prop_len(pdev,
					"qcom,mdss-clk-levels");

	if (mdata->nclk_lvl) {
		mdata->clock_levels = kzalloc(sizeof(u32) * mdata->nclk_lvl,
							GFP_KERNEL);
		if (!mdata->clock_levels) {
			pr_err("no mem assigned for mdata clock_levels\n");
			return -ENOMEM;
		}

		rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-clk-levels",
			mdata->clock_levels, mdata->nclk_lvl);
		if (rc)
			pr_debug("clock levels not found\n");
	}

	mdss_mdp_parse_vbif_qos(pdev);
	mdata->traffic_shaper_en = of_property_read_bool(pdev->dev.of_node,
		 "qcom,mdss-traffic-shaper-enabled");
	mdata->has_rot_dwnscale = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-rotator-downscale");

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-dram-channels", &mdata->bus_channels);
	if (rc)
		pr_debug("number of channels property not specified\n");

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,max-pipe-width", &mdata->max_pipe_width);
	if (rc) {
		pr_debug("max pipe width not specified. Using default value\n");
		mdata->max_pipe_width = DEFAULT_MDP_PIPE_WIDTH;
	}
	return 0;
}

static int mdss_mdp_parse_dt_ad_cfg(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 *ad_offsets = NULL;
	int rc;

	mdata->nad_cfgs = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-ad-off");

	if (mdata->nad_cfgs == 0) {
		mdata->ad_cfgs = NULL;
		return 0;
	}

	if (mdata->nad_cfgs > mdata->nmixers_intf)
		return -EINVAL;


	mdata->has_wb_ad = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-wb-ad");

	ad_offsets = kzalloc(sizeof(u32) * mdata->nad_cfgs, GFP_KERNEL);
	if (!ad_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		return -ENOMEM;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-ad-off", ad_offsets,
					mdata->nad_cfgs);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_ad_addr_setup(mdata, ad_offsets);
	if (rc)
		pr_err("unable to setup assertive display\n");

parse_done:
	kfree(ad_offsets);
	return rc;
}

static int mdss_mdp_parse_dt_ppb_off(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 len, index;
	const u32 *arr;
	arr = of_get_property(pdev->dev.of_node, "qcom,mdss-ppb-ctl-off", &len);
	if (arr) {
		mdata->nppb_ctl = len / sizeof(u32);
		mdata->ppb_ctl = devm_kzalloc(&mdata->pdev->dev,
				sizeof(u32) * mdata->nppb_ctl, GFP_KERNEL);

		if (mdata->ppb_ctl == NULL)
			return -ENOMEM;

		for (index = 0; index <  mdata->nppb_ctl; index++)
			mdata->ppb_ctl[index] = be32_to_cpu(arr[index]);
	}

	arr = of_get_property(pdev->dev.of_node, "qcom,mdss-ppb-cfg-off", &len);
	if (arr) {
		mdata->nppb_cfg = len / sizeof(u32);
		mdata->ppb_cfg = devm_kzalloc(&mdata->pdev->dev,
				sizeof(u32) * mdata->nppb_cfg, GFP_KERNEL);

		if (mdata->ppb_cfg == NULL)
			return -ENOMEM;

		for (index = 0; index <  mdata->nppb_cfg; index++)
			mdata->ppb_cfg[index] = be32_to_cpu(arr[index]);
	}
	return 0;
}

#ifdef CONFIG_QCOM_BUS_SCALING
static int mdss_mdp_parse_dt_bus_scale(struct platform_device *pdev)
{
	int rc, paths;
	struct device_node *node;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-bus,num-paths", &paths);
	if (rc) {
		pr_err("Error. qcom,msm-bus,num-paths prop not found.rc=%d\n",
			rc);
		return rc;
	}
	mdss_res->axi_port_cnt = paths;

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,mdss-num-nrt-paths", &mdata->nrt_axi_port_cnt);
	if (rc && mdata->has_fixed_qos_arbiter_enabled) {
		pr_err("Error. qcom,mdss-num-nrt-paths prop not found.rc=%d\n",
			rc);
		return rc;
	} else {
		rc = 0;
	}

	mdata->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (IS_ERR_OR_NULL(mdata->bus_scale_table)) {
		rc = PTR_ERR(mdata->bus_scale_table);
		if (!rc)
			rc = -EINVAL;
		pr_err("msm_bus_cl_get_pdata failed. rc=%d\n", rc);
		mdata->bus_scale_table = NULL;
		return rc;
	}

	/*
	 * if mdss-reg-bus is not found then default table is picked
	 * hence below code wont return error.
	 */
	node = of_get_child_by_name(pdev->dev.of_node, "qcom,mdss-reg-bus");
	if (node) {
		mdata->reg_bus_scale_table =
			msm_bus_pdata_from_node(pdev, node);
		if (IS_ERR_OR_NULL(mdata->reg_bus_scale_table)) {
			rc = PTR_ERR(mdata->reg_bus_scale_table);
			if (!rc)
				pr_err("bus_pdata reg_bus failed rc=%d\n", rc);
			rc = 0;
			mdata->reg_bus_scale_table = NULL;
		}
	} else {
		rc = 0;
		mdata->reg_bus_scale_table = NULL;
		pr_debug("mdss-reg-bus not found\n");
	}

	node = of_get_child_by_name(pdev->dev.of_node, "qcom,mdss-hw-rt-bus");
	if (node) {
		mdata->hw_rt_bus_scale_table =
			msm_bus_pdata_from_node(pdev, node);
		if (IS_ERR_OR_NULL(mdata->hw_rt_bus_scale_table)) {
			rc = PTR_ERR(mdata->hw_rt_bus_scale_table);
			if (!rc)
				pr_err("hw_rt_bus_scale failed rc=%d\n", rc);
			rc = 0;
			mdata->hw_rt_bus_scale_table = NULL;
		}
	} else {
		rc = 0;
		mdata->hw_rt_bus_scale_table = NULL;
		pr_debug("mdss-hw-rt-bus not found\n");
	}

	return rc;
}
#endif

static int mdss_mdp_parse_dt_handler(struct platform_device *pdev,
		char *prop_name, u32 *offsets, int len)
{
	int rc;
	rc = of_property_read_u32_array(pdev->dev.of_node, prop_name,
					offsets, len);
	if (rc) {
		pr_err("Error from prop %s : u32 array read\n", prop_name);
		return -EINVAL;
	}

	return 0;
}

static int mdss_mdp_parse_dt_prop_len(struct platform_device *pdev,
				      char *prop_name)
{
	int len = 0;

	of_find_property(pdev->dev.of_node, prop_name, &len);

	if (len < 1) {
		pr_debug("prop %s : doesn't exist in device tree\n",
			prop_name);
		return 0;
	}

	len = len/sizeof(u32);

	return len;
}

struct mdss_data_type *mdss_mdp_get_mdata(void)
{
	return mdss_res;
}

void mdss_mdp_batfet_ctrl(struct mdss_data_type *mdata, int enable)
{
	int ret;

	if (!mdata->batfet_required)
		return;

	if (!mdata->batfet) {
		if (enable) {
			mdata->batfet = devm_regulator_get(&mdata->pdev->dev,
				"batfet");
			if (IS_ERR_OR_NULL(mdata->batfet)) {
				pr_debug("unable to get batfet reg. rc=%d\n",
					PTR_RET(mdata->batfet));
				mdata->batfet = NULL;
				return;
			}
		} else {
			pr_debug("Batfet regulator disable w/o enable\n");
			return;
		}
	}

	if (enable) {
		ret = regulator_enable(mdata->batfet);
		if (ret)
			pr_err("regulator_enable failed\n");
	} else {
		regulator_disable(mdata->batfet);
	}
}

/**
 * mdss_is_ready() - checks if mdss is probed and ready
 *
 * Checks if mdss resources have been initialized
 *
 * returns true if mdss is ready, else returns false
 */
bool mdss_is_ready(void)
{
	return mdss_mdp_get_mdata() ? true : false;
}
EXPORT_SYMBOL(mdss_mdp_get_mdata);

/**
 * mdss_panel_intf_type() - checks if a given intf type is primary
 * @intf_val: panel interface type of the individual controller
 *
 * Individual controller queries with MDP to check if it is
 * configured as the primary interface.
 *
 * returns a pointer to the configured structure mdss_panel_cfg
 * to the controller that's configured as the primary panel interface.
 * returns NULL on error or if @intf_val is not the configured
 * controller.
 */
struct mdss_panel_cfg *mdss_panel_intf_type(int intf_val)
{
	if (!mdss_res || !mdss_res->pan_cfg.init_done)
		return ERR_PTR(-EPROBE_DEFER);

	if (mdss_res->pan_cfg.pan_intf == intf_val)
		return &mdss_res->pan_cfg;
	else
		return NULL;
}
EXPORT_SYMBOL(mdss_panel_intf_type);

struct irq_info *mdss_intr_line()
{
	return mdss_mdp_hw.irq_info;
}
EXPORT_SYMBOL(mdss_intr_line);

int mdss_mdp_wait_for_xin_halt(u32 xin_id, bool is_vbif_nrt)
{
	void __iomem *vbif_base;
	u32 status;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 idle_mask = BIT(xin_id);
	int rc;

	vbif_base = is_vbif_nrt ? mdata->vbif_nrt_io.base :
				mdata->vbif_io.base;

	rc = readl_poll_timeout(vbif_base + MMSS_VBIF_XIN_HALT_CTRL1,
		status, (status & idle_mask),
		1000, XIN_HALT_TIMEOUT_US);
	if (rc == -ETIMEDOUT) {
		pr_err("VBIF client %d not halting. TIMEDOUT.\n",
			xin_id);
		MDSS_XLOG_TOUT_HANDLER("mdp", "vbif", "vbif_nrt",
			"dbg_bus", "vbif_dbg_bus", "panic");
	} else {
		pr_debug("VBIF client %d is halted\n", xin_id);
	}

	return rc;
}

/**
 * force_on_xin_clk() - enable/disable the force-on for the pipe clock
 * @bit_off: offset of the bit to enable/disable the force-on.
 * @reg_off: register offset for the clock control.
 * @enable: boolean to indicate if the force-on of the clock needs to be
 * enabled or disabled.
 *
 * This function returns:
 * true - if the clock is forced-on by this function
 * false - if the clock was already forced on
 * It is the caller responsibility to check if this function is forcing
 * the clock on; if so, it will need to remove the force of the clock,
 * otherwise it should avoid to remove the force-on.
 * Clocks must be on when calling this function.
 */
bool force_on_xin_clk(u32 bit_off, u32 clk_ctl_reg_off, bool enable)
{
	u32 val;
	u32 force_on_mask;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool clk_forced_on = false;

	force_on_mask = BIT(bit_off);
	val = readl_relaxed(mdata->mdp_base + clk_ctl_reg_off);

	clk_forced_on = !(force_on_mask & val);

	if (true == enable)
		val |= force_on_mask;
	else
		val &= ~force_on_mask;

	writel_relaxed(val, mdata->mdp_base + clk_ctl_reg_off);

	return clk_forced_on;
}

static void apply_dynamic_ot_limit(u32 *ot_lim,
	struct mdss_mdp_set_ot_params *params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 res;

	if (false == test_bit(MDSS_QOS_OTLIM, mdata->mdss_qos_map))
		return;

	/* Dynamic OT setting done only for rotator and WFD */
	if (!((params->is_rot && params->is_yuv) || params->is_wfd))
		return;

	res = params->width * params->height;

	pr_debug("w:%d h:%d rot:%d yuv:%d wb:%d res:%d fps:%d\n",
		params->width, params->height, params->is_rot,
		params->is_yuv, params->is_wfd, res, params->frame_rate);

	switch (mdata->mdp_rev) {
	case MDSS_MDP_HW_REV_114:
	case MDSS_MDP_HW_REV_115:
	case MDSS_MDP_HW_REV_116:
		if ((res <= RES_1080p) && (params->frame_rate <= 30))
			*ot_lim = 2;
		else if (params->is_rot && params->is_yuv)
			*ot_lim = 4;
		else
			*ot_lim = 6;
		break;
	case MDSS_MDP_HW_REV_320:
	case MDSS_MDP_HW_REV_330:
		if ((res <= RES_1080p) && (params->frame_rate <= 30))
			*ot_lim = 2;
		else if ((res <= RES_1080p) && (params->frame_rate <= 60))
			*ot_lim = 6;
		else if ((res <= RES_UHD) && (params->frame_rate <= 30))
			*ot_lim = 16;
		break;
	default:
		if (res <= RES_1080p) {
			*ot_lim = 2;
		} else if (res <= RES_UHD) {
			if (params->is_rot && params->is_yuv)
				*ot_lim = 8;
			else
				*ot_lim = 16;
		}
		break;
	}
}

static u32 get_ot_limit(u32 reg_off, u32 bit_off,
	struct mdss_mdp_set_ot_params *params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 ot_lim = 0;
	u32 is_vbif_nrt, val;

	if (mdata->default_ot_wr_limit &&
		(params->reg_off_vbif_lim_conf == MMSS_VBIF_WR_LIM_CONF))
		ot_lim = mdata->default_ot_wr_limit;
	else if (mdata->default_ot_rd_limit &&
		(params->reg_off_vbif_lim_conf == MMSS_VBIF_RD_LIM_CONF))
		ot_lim = mdata->default_ot_rd_limit;

	/*
	 * If default ot is not set from dt,
	 * then do not configure it.
	 */
	if (ot_lim == 0)
		goto exit;

	/* Modify the limits if the target and the use case requires it */
	apply_dynamic_ot_limit(&ot_lim, params);

	is_vbif_nrt = params->is_vbif_nrt;
	val = MDSS_VBIF_READ(mdata, reg_off, is_vbif_nrt);
	val &= (0xFF << bit_off);
	val = val >> bit_off;

	if (val == ot_lim)
		ot_lim = 0;

exit:
	pr_debug("ot_lim=%d\n", ot_lim);
	return ot_lim;
}

void mdss_mdp_set_ot_limit(struct mdss_mdp_set_ot_params *params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 ot_lim;
	u32 reg_off_vbif_lim_conf = (params->xin_id / 4) * 4 +
		params->reg_off_vbif_lim_conf;
	u32 bit_off_vbif_lim_conf = (params->xin_id % 4) * 8;
	bool is_vbif_nrt = params->is_vbif_nrt;
	u32 reg_val;
	bool forced_on;

	ot_lim = get_ot_limit(
		reg_off_vbif_lim_conf,
		bit_off_vbif_lim_conf,
		params) & 0xFF;

	if (ot_lim == 0)
		goto exit;

	trace_mdp_perf_set_ot(params->num, params->xin_id, ot_lim,
		is_vbif_nrt);

	mutex_lock(&mdata->reg_lock);

	forced_on = force_on_xin_clk(params->bit_off_mdp_clk_ctrl,
		params->reg_off_mdp_clk_ctrl, true);

	reg_val = MDSS_VBIF_READ(mdata, reg_off_vbif_lim_conf,
		is_vbif_nrt);
	reg_val &= ~(0xFF << bit_off_vbif_lim_conf);
	reg_val |= (ot_lim) << bit_off_vbif_lim_conf;
	MDSS_VBIF_WRITE(mdata, reg_off_vbif_lim_conf, reg_val,
		is_vbif_nrt);

	reg_val = MDSS_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		is_vbif_nrt);
	MDSS_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		reg_val | BIT(params->xin_id), is_vbif_nrt);

	mutex_unlock(&mdata->reg_lock);
	mdss_mdp_wait_for_xin_halt(params->xin_id, is_vbif_nrt);
	mutex_lock(&mdata->reg_lock);

	reg_val = MDSS_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		is_vbif_nrt);
	MDSS_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
		reg_val & ~BIT(params->xin_id), is_vbif_nrt);

	if (forced_on)
		force_on_xin_clk(params->bit_off_mdp_clk_ctrl,
			params->reg_off_mdp_clk_ctrl, false);

	mutex_unlock(&mdata->reg_lock);

exit:
	return;
}

#define RPM_MISC_REQ_TYPE 0x6373696d
#define RPM_MISC_REQ_SVS_PLUS_KEY 0x2B737673

static void mdss_mdp_config_cx_voltage(struct mdss_data_type *mdata, int enable)
{
	int ret = 0;
	static struct msm_rpm_kvp rpm_kvp;
	static uint8_t svs_en;

	if (!mdata->en_svs_high)
		return;

	if (!rpm_kvp.key) {
		rpm_kvp.key = RPM_MISC_REQ_SVS_PLUS_KEY;
		rpm_kvp.length = sizeof(unsigned);
		pr_debug("%s: Initialized rpm_kvp structure\n", __func__);
	}

	if (enable) {
		svs_en = 1;
		rpm_kvp.data = &svs_en;
		pr_debug("%s: voting for svs high\n", __func__);
		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
					RPM_MISC_REQ_TYPE, 0,
					&rpm_kvp, 1);
		if (ret)
			pr_err("vote for active_set svs high failed: %d\n",
					ret);
		ret = msm_rpm_send_message(MSM_RPM_CTX_SLEEP_SET,
					RPM_MISC_REQ_TYPE, 0,
					&rpm_kvp, 1);
		if (ret)
			pr_err("vote for sleep_set svs high failed: %d\n",
					ret);
	} else {
		svs_en = 0;
		rpm_kvp.data = &svs_en;
		pr_debug("%s: Removing vote for svs high\n", __func__);
		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
					RPM_MISC_REQ_TYPE, 0,
					&rpm_kvp, 1);
		if (ret)
			pr_err("Remove vote:active_set svs high failed: %d\n",
					ret);
		ret = msm_rpm_send_message(MSM_RPM_CTX_SLEEP_SET,
					RPM_MISC_REQ_TYPE, 0,
					&rpm_kvp, 1);
		if (ret)
			pr_err("Remove vote:sleep_set svs high failed: %d\n",
					ret);
	}
}

static int mdss_mdp_cx_ctrl(struct mdss_data_type *mdata, int enable)
{
	int rc = 0;

	if (!mdata->vdd_cx)
		return rc;

	if (enable) {
		rc = regulator_set_voltage(
				mdata->vdd_cx,
				mdata->vdd_cx_min_uv,
				mdata->vdd_cx_max_uv);
		if (rc < 0)
			goto vreg_set_voltage_fail;

		pr_debug("Enabling CX power rail\n");
		rc = regulator_enable(mdata->vdd_cx);
		if (rc) {
			pr_err("Failed to enable regulator.\n");
			return rc;
		}
	} else {
		pr_debug("Disabling CX power rail\n");
		rc = regulator_disable(mdata->vdd_cx);
		if (rc) {
			pr_err("Failed to disable regulator.\n");
			return rc;
		}
		rc = regulator_set_voltage(
				mdata->vdd_cx,
				0,
				mdata->vdd_cx_max_uv);
		if (rc < 0)
			goto vreg_set_voltage_fail;
	}

	return rc;

vreg_set_voltage_fail:
	pr_err("Set vltg fail\n");
	return rc;
}

/**
 * mdss_mdp_footswitch_ctrl() - Disable/enable MDSS GDSC and CX/Batfet rails
 * @mdata: MDP private data
 * @on: 1 to turn on footswitch, 0 to turn off footswitch
 *
 * When no active references to the MDP device node and it's child nodes are
 * held, MDSS GDSC can be turned off. However, any any panels are still
 * active (but likely in an idle state), the vote for the CX and the batfet
 * rails should not be released.
 */
static void mdss_mdp_footswitch_ctrl(struct mdss_data_type *mdata, int on)
{
	int ret;
	int active_cnt = 0;
	bool footswitch_suspend = false;

	if (!mdata->fs)
		return;

	MDSS_XLOG(on, mdata->fs_ena, mdata->idle_pc, mdata->en_svs_high,
		atomic_read(&mdata->active_intf_cnt));

	if (on) {
		if (!mdata->fs_ena) {
			pr_debug("Enable MDP FS\n");
			if (mdata->core_gdsc) {
				ret = regulator_enable(mdata->core_gdsc);
				if (ret)
					pr_err("core_gdsc failed to enable\n");
			}

			/*
			 * Advise RPM to not turn MMSS GDSC off, this will
			 * ensure that GDSC off is maintained during Active
			 * display and during Idle display
			 */
			if (mdss_has_quirk(mdata,
					MDSS_QUIRK_MMSS_GDSC_COLLAPSE))
				mdss_rpm_set_msg_ram(true);

			ret = regulator_enable(mdata->fs);
			if (ret)
				pr_warn("Footswitch failed to enable\n");
			if (!mdata->idle_pc) {
				mdss_mdp_cx_ctrl(mdata, true);
				mdss_mdp_batfet_ctrl(mdata, true);
			}
		}
		if (mdata->en_svs_high)
			mdss_mdp_config_cx_voltage(mdata, true);
		mdata->fs_ena = true;
	} else {
		if (mdata->fs_ena) {
			pr_debug("Disable MDP FS\n");
			active_cnt = atomic_read(&mdata->active_intf_cnt);
			if (active_cnt != 0) {
				/*
				 * Turning off GDSC while overlays are still
				 * active.
				 */

				mdss_mdp_memory_retention_ctrl(MEM_RETAIN_ON,
					PERIPH_RETAIN_OFF);
				mdata->idle_pc = true;
				pr_debug("idle pc. active overlays=%d\n",
					active_cnt);
			} else {

				footswitch_suspend = true;
				mdss_mdp_cx_ctrl(mdata, false);
				mdss_mdp_batfet_ctrl(mdata, false);
				mdss_mdp_memory_retention_ctrl(
					MEM_RETAIN_OFF,
					PERIPH_RETAIN_OFF);
			}
			mdata->mem_retain = true;
			if (mdata->en_svs_high)
				mdss_mdp_config_cx_voltage(mdata, false);
			regulator_disable(mdata->fs);
			if (mdata->core_gdsc)
				regulator_disable(mdata->core_gdsc);

			if (footswitch_suspend) {
				/*
				 * Advise RPM to turn MMSS GDSC off during
				 * suspend case, do this after the MDSS GDSC
				 * regulator OFF, so we can ensure that MMSS
				 * GDSC will go OFF after the MDSS GDSC
				 * regulator
				 */
				if (mdss_has_quirk(mdata,
						MDSS_QUIRK_MMSS_GDSC_COLLAPSE))
					mdss_rpm_set_msg_ram(false);
			}
		}
		mdata->fs_ena = false;
	}
}

int mdss_mdp_secure_session_ctrl(unsigned int enable, u64 flags)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct sd_ctrl_req {
		unsigned int enable;
	} __attribute__ ((__packed__)) request;
	unsigned int resp = -1;
	int ret = 0;
	uint32_t sid_info;
	struct scm_desc desc;
	bool changed = false;

	mutex_lock(&mdp_sec_ref_cnt_lock);

	if (enable) {
		if (mdata->sec_session_cnt == 0)
			changed = true;
		mdata->sec_session_cnt++;
	} else {
		if (mdata->sec_session_cnt != 0) {
			mdata->sec_session_cnt--;
			if (mdata->sec_session_cnt == 0)
				changed = true;
		} else {
			pr_warn("%s: ref_count is not balanced\n",
				__func__);
		}
	}

	if (!changed)
		goto end;

	if (test_bit(MDSS_CAPS_SEC_DETACH_SMMU, mdata->mdss_caps_map)) {
		/*
		 * Prepare syscall to hypervisor to switch the secure_vmid
		 * between secure and non-secure contexts
		 */
		/* MDP secure SID */
		sid_info = 0x1;
		desc.arginfo = SCM_ARGS(4, SCM_VAL, SCM_RW, SCM_VAL, SCM_VAL);
		desc.args[0] = MDP_DEVICE_ID;
		desc.args[1] = SCM_BUFFER_PHYS(&sid_info);
		desc.args[2] = sizeof(uint32_t);


		pr_debug("Enable/Disable: %d, Flags %llx\n", enable, flags);
		if (enable) {
			if (flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION) {
				desc.args[3] = VMID_CP_SEC_DISPLAY;
				mdata->sec_disp_en = 1;
			} else if (flags & MDP_SECURE_CAMERA_OVERLAY_SESSION) {
				desc.args[3] = VMID_CP_CAMERA_PREVIEW;
				mdata->sec_cam_en = 1;
			} else {
				ret = 0;
				goto end;
			}

			/* detach smmu contexts */
			ret = mdss_smmu_detach(mdata);
			if (ret) {
				pr_err("Error while detaching smmu contexts ret = %d\n",
					ret);
				ret = -EINVAL;
				goto end;
			}

			/* let the driver think smmu is still attached */
			mdata->iommu_attached = true;

			dmac_flush_range(&sid_info, &sid_info + 1);
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				mem_protect_sd_ctrl_id), &desc);
			if (ret) {
				pr_err("Error scm_call MEM_PROTECT_SD_CTRL(%u) ret=%dm resp=%x\n",
						enable, ret, resp);
				ret = -EINVAL;
				goto end;
			}
			resp = desc.ret[0];

			pr_debug("scm_call MEM_PROTECT_SD_CTRL(%u): ret=%d, resp=%x\n",
					enable, ret, resp);
		} else {
			desc.args[3] = VMID_CP_PIXEL;
			if (flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION)
				mdata->sec_disp_en = 0;
			else if (flags & MDP_SECURE_CAMERA_OVERLAY_SESSION)
				mdata->sec_cam_en = 0;

			dmac_flush_range(&sid_info, &sid_info + 1);
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				mem_protect_sd_ctrl_id), &desc);
			if (ret)
				MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl",
						"dsi0_phy", "dsi1_ctrl",
						"dsi1_phy", "vbif", "vbif_nrt",
						"dbg_bus", "vbif_dbg_bus",
						"panic");
			resp = desc.ret[0];

			pr_debug("scm_call MEM_PROTECT_SD_CTRL(%u): ret=%d, resp=%x\n",
					enable, ret, resp);

			/* re-attach smmu contexts */
			mdata->iommu_attached = false;
			ret = mdss_smmu_attach(mdata);
			if (ret) {
				pr_err("Error while attaching smmu contexts ret = %d\n",
					ret);
				ret = -EINVAL;
				goto end;
			}
		}
		MDSS_XLOG(enable);
	} else {
		desc.args[0] = request.enable = enable;
		desc.arginfo = SCM_ARGS(1);

		if (!is_scm_armv8()) {
			ret = scm_call(SCM_SVC_MP, MEM_PROTECT_SD_CTRL,
					&request,
					sizeof(request),
					&resp,
					sizeof(resp));
		} else {
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
						mem_protect_sd_ctrl_id), &desc);
			resp = desc.ret[0];
		}

		pr_debug("scm_call MEM_PROTECT_SD_CTRL(%u): ret=%d, resp=%x\n",
				enable, ret, resp);
	}

end:
	mutex_unlock(&mdp_sec_ref_cnt_lock);
	return ret;

}

static inline int mdss_mdp_suspend_sub(struct mdss_data_type *mdata)
{
	mdata->suspend_fs_ena = mdata->fs_ena;
	mdss_mdp_footswitch_ctrl(mdata, false);

	pr_debug("suspend done fs=%d\n", mdata->suspend_fs_ena);

	return 0;
}

static inline int mdss_mdp_resume_sub(struct mdss_data_type *mdata)
{
	if (mdata->suspend_fs_ena)
		mdss_mdp_footswitch_ctrl(mdata, true);

	pr_debug("resume done fs=%d\n", mdata->suspend_fs_ena);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mdss_mdp_pm_suspend(struct device *dev)
{
	struct mdss_data_type *mdata;

	mdata = dev_get_drvdata(dev);
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "display pm suspend\n");

	return mdss_mdp_suspend_sub(mdata);
}

static int mdss_mdp_pm_resume(struct device *dev)
{
	struct mdss_data_type *mdata;

	mdata = dev_get_drvdata(dev);
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "display pm resume\n");

	/*
	 * It is possible that the runtime status of the mdp device may
	 * have been active when the system was suspended. Reset the runtime
	 * status to suspended state after a complete system resume.
	 */
	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);

	return mdss_mdp_resume_sub(mdata);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP)
static int mdss_mdp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	if (!mdata)
		return -ENODEV;

	dev_dbg(&pdev->dev, "display suspend\n");

	return mdss_mdp_suspend_sub(mdata);
}

static int mdss_mdp_resume(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	if (!mdata)
		return -ENODEV;

	dev_dbg(&pdev->dev, "display resume\n");

	return mdss_mdp_resume_sub(mdata);
}
#else
#define mdss_mdp_suspend NULL
#define mdss_mdp_resume NULL
#endif

#ifdef CONFIG_PM
static int mdss_mdp_runtime_resume(struct device *dev)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	bool device_on = true;
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "pm_runtime: resuming. active overlay cnt=%d\n",
		atomic_read(&mdata->active_intf_cnt));

	/* do not resume panels when coming out of idle power collapse */
	if (!mdata->idle_pc)
		device_for_each_child(dev, &device_on, mdss_fb_suspres_panel);
	mdss_mdp_footswitch_ctrl(mdata, true);

	return 0;
}

static int mdss_mdp_runtime_idle(struct device *dev)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "pm_runtime: idling...\n");

	return 0;
}

static int mdss_mdp_runtime_suspend(struct device *dev)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	bool device_on = false;
	if (!mdata)
		return -ENODEV;
	dev_dbg(dev, "pm_runtime: suspending. active overlay cnt=%d\n",
		atomic_read(&mdata->active_intf_cnt));

	if (mdata->clk_ena) {
		pr_err("MDP suspend failed\n");
		return -EBUSY;
	}

	mdss_mdp_footswitch_ctrl(mdata, false);
	/* do not suspend panels when going in to idle power collapse */
	if (!mdata->idle_pc)
		device_for_each_child(dev, &device_on, mdss_fb_suspres_panel);

	return 0;
}
#endif

static const struct dev_pm_ops mdss_mdp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mdss_mdp_pm_suspend, mdss_mdp_pm_resume)
#ifdef CONFIG_PM
	SET_RUNTIME_PM_OPS(mdss_mdp_runtime_suspend,
			mdss_mdp_runtime_resume,
			mdss_mdp_runtime_idle)
#endif
};

static int mdss_mdp_remove(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	if (!mdata)
		return -ENODEV;
	pm_runtime_disable(&pdev->dev);
	mdss_mdp_pp_term(&pdev->dev);
	mdss_mdp_bus_scale_unregister(mdata);
	mdss_debugfs_remove(mdata);
	if (mdata->regulator_notif_register)
		regulator_unregister_notifier(mdata->fs, &(mdata->gdsc_cb));
	return 0;
}

static const struct of_device_id mdss_mdp_dt_match[] = {
	{ .compatible = "qcom,mdss_mdp",},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_mdp_dt_match);

static struct platform_driver mdss_mdp_driver = {
	.probe = mdss_mdp_probe,
	.remove = mdss_mdp_remove,
	.suspend = mdss_mdp_suspend,
	.resume = mdss_mdp_resume,
	.shutdown = NULL,
	.driver = {
		/*
		 * Driver name must match the device name added in
		 * platform.c.
		 */
		.name = "mdp",
		.of_match_table = mdss_mdp_dt_match,
		.pm = &mdss_mdp_pm_ops,
	},
};

static int mdss_mdp_register_driver(void)
{
	return platform_driver_register(&mdss_mdp_driver);
}

static int __init mdss_mdp_driver_init(void)
{
	int ret;

	ret = mdss_mdp_register_driver();
	if (ret) {
		pr_err("mdp_register_driver() failed!\n");
		return ret;
	}

	return 0;

}

module_param_string(panel, mdss_mdp_panel, MDSS_MAX_PANEL_LEN, 0);
MODULE_PARM_DESC(panel,
		"panel=<lk_cfg>:<pan_intf>:<pan_intf_cfg>:<panel_topology_cfg> "
		"where <lk_cfg> is "1"-lk/gcdb config or "0" non-lk/non-gcdb "
		"config; <pan_intf> is dsi:<ctrl_id> or hdmi or edp "
		"<pan_intf_cfg> is panel interface specific string "
		"Ex: This string is panel's device node name from DT "
		"for DSI interface "
		"hdmi/edp interface does not use this string "
		"<panel_topology_cfg> is an optional string. Currently it is "
		"only valid for DSI panels. In dual-DSI case, it needs to be"
		"used on both panels or none. When used, format is config%d "
		"where %d is one of the configuration found in device node of "
		"panel selected by <pan_intf_cfg>");

module_init(mdss_mdp_driver_init);

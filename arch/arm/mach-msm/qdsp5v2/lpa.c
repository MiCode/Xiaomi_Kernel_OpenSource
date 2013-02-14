/* Copyright (c) 2009-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <mach/qdsp5v2/lpa.h>
#include <mach/qdsp5v2/lpa_hw.h>
#include <mach/qdsp5v2/msm_lpa.h>
#include <mach/debug_mm.h>

#define LPA_REG_WRITEL(drv, val, reg)  writel(val, drv->baseaddr + reg)
#define LPA_REG_READL(drv, reg)  readl(drv->baseaddr + reg)

/* bit 2:0 is reserved because watermarks have to be 64-bit aligned */
#define LLB_WATERMARK_VAL_MASK 0x00000003

#define LPA_STATUS_SBUF_EN 0x01

struct lpa_drv {
	void __iomem *baseaddr;
	u32 obuf_hlb_size;
	u32 dsp_proc_id;
	u32 app_proc_id;
	struct lpa_mem_config nosb_config;
	struct lpa_mem_config sb_config;
	u32 status;
	u32 watermark_bytes;
	u32 watermark_aheadtime;
	u32 sample_boundary;
};

struct lpa_state {
	struct lpa_drv lpa_drv; /* One instance for now */
	u32 assigned;
	struct mutex lpa_lock;
};

struct lpa_state the_lpa_state;

static void lpa_enable_codec(struct lpa_drv *lpa, bool enable)
{
	u32 val;

	val = LPA_REG_READL(lpa, LPA_OBUF_CODEC);
	val = enable ? (val | LPA_OBUF_CODEC_CODEC_INTF_EN_BMSK) :
		(val & ~LPA_OBUF_CODEC_CODEC_INTF_EN_BMSK);
	val |= LPA_OBUF_CODEC_LOAD_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_CODEC);
	mb();
}

static void lpa_reset(struct lpa_drv *lpa)
{
	u32 status;
	struct clk *adsp_clk;
	/* Need to make sure not disable clock while other device is enabled */
	adsp_clk = clk_get(NULL, "adsp_clk");
	if (!adsp_clk) {
		MM_ERR("failed to get adsp clk\n");
		goto error;
	}
	clk_prepare_enable(adsp_clk);
	lpa_enable_codec(lpa, 0);
	LPA_REG_WRITEL(lpa, (LPA_OBUF_RESETS_MISR_RESET |
		LPA_OBUF_RESETS_OVERALL_RESET), LPA_OBUF_RESETS);
	do {
		status = LPA_REG_READL(lpa, LPA_OBUF_STATUS);
	} while (!(status & LPA_OBUF_STATUS_RESET_DONE));

	LPA_REG_WRITEL(lpa, LPA_OBUF_ACK_RESET_DONE_BMSK, LPA_OBUF_ACK);
	mb();
	clk_disable_unprepare(adsp_clk);
	clk_put(adsp_clk);
error:
	return;
}

static void lpa_config_hlb_addr(struct lpa_drv *lpa)
{
	u32 val, min_addr = 0, max_addr = min_addr + lpa->obuf_hlb_size;

	val = (min_addr & LPA_OBUF_HLB_MIN_ADDR_SEG_BMSK) |
	LPA_OBUF_HLB_MIN_ADDR_LOAD_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_HLB_MIN_ADDR);
	val = max_addr & LPA_OBUF_HLB_MAX_ADDR_SEG_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_HLB_MAX_ADDR);
}

static void lpa_powerup_mem_bank(struct lpa_drv *lpa,
	struct lpa_mem_bank_select *bank)
{
	u32 status, val;

	status = LPA_REG_READL(lpa, LPA_OBUF_MEMORY_CONTROL);
	val = ((*((u32 *) bank)) << LPA_OBUF_MEM_CTL_PWRUP_SHFT) &
	LPA_OBUF_MEM_CTL_PWRUP_BMSK;
	val |= status;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_MEMORY_CONTROL);
}

static void lpa_enable_interrupt(struct lpa_drv *lpa, u32 proc_id)
{
	u32 val;

	proc_id &= LPA_OBUF_INTR_EN_BMSK;
	val = 0x1 << proc_id;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_INTR_ENABLE);
}

static void lpa_config_llb_addr(struct lpa_drv *lpa, u32 min_addr, u32 max_addr)
{
	u32 val;

	val = (min_addr & LPA_OBUF_LLB_MIN_ADDR_SEG_BMSK) |
	LPA_OBUF_LLB_MIN_ADDR_LOAD_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_LLB_MIN_ADDR);
	val = max_addr & LPA_OBUF_LLB_MAX_ADDR_SEG_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_LLB_MAX_ADDR);
}

static void lpa_config_sb_addr(struct lpa_drv *lpa, u32 min_addr, u32 max_addr)
{
	u32 val;

	val = (min_addr & LPA_OBUF_SB_MIN_ADDR_SEG_BMSK) |
	LPA_OBUF_SB_MIN_ADDR_LOAD_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_SB_MIN_ADDR);
	val = max_addr & LPA_OBUF_SB_MAX_ADDR_SEG_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_SB_MAX_ADDR);
}

static void lpa_switch_sb(struct lpa_drv *lpa)
{
	if (lpa->status & LPA_STATUS_SBUF_EN) {
		lpa_config_llb_addr(lpa, lpa->sb_config.llb_min_addr,
		lpa->sb_config.llb_max_addr);
		lpa_config_sb_addr(lpa, lpa->sb_config.sb_min_addr,
		lpa->sb_config.sb_max_addr);
	} else {
		lpa_config_llb_addr(lpa, lpa->nosb_config.llb_min_addr,
		lpa->nosb_config.llb_max_addr);
		lpa_config_sb_addr(lpa, lpa->nosb_config.sb_min_addr,
		lpa->nosb_config.sb_max_addr);
	}
}

static u8 lpa_req_wmark_id(struct lpa_drv *lpa)
{
  return (u8) (LPA_REG_READL(lpa, LPA_OBUF_WMARK_ASSIGN) &
	LPA_OBUF_WMARK_ASSIGN_BMSK);
}

static void lpa_enable_llb_wmark(struct lpa_drv *lpa, u32 wmark_ctrl,
	u32 wmark_id, u32 cpu_id)
{
	u32 val;

	wmark_id = (wmark_id > 3) ? 0 : wmark_id;
	val = LPA_REG_READL(lpa, LPA_OBUF_WMARK_n_LLB_ADDR(wmark_id));
	val &= ~LPA_OBUF_LLB_WMARK_CTRL_BMSK;
	val &= ~LPA_OBUF_LLB_WMARK_MAP_BMSK;
	val |= (wmark_ctrl << LPA_OBUF_LLB_WMARK_CTRL_SHFT) &
	LPA_OBUF_LLB_WMARK_CTRL_BMSK;
	val |= (cpu_id << LPA_OBUF_LLB_WMARK_MAP_SHFT) &
	LPA_OBUF_LLB_WMARK_MAP_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_WMARK_n_LLB_ADDR(wmark_id));
}

static void lpa_enable_sb_wmark(struct lpa_drv *lpa, u32 wmark_ctrl,
	u32 cpu_id)
{
	u32 val;

	val = LPA_REG_READL(lpa, LPA_OBUF_WMARK_SB);
	val &= ~LPA_OBUF_SB_WMARK_CTRL_BMSK;
	val &= ~LPA_OBUF_SB_WMARK_MAP_BMSK;
	val |= (wmark_ctrl << LPA_OBUF_SB_WMARK_CTRL_SHFT) &
	LPA_OBUF_SB_WMARK_CTRL_BMSK;
	val |= (cpu_id << LPA_OBUF_SB_WMARK_MAP_SHFT) &
	LPA_OBUF_SB_WMARK_MAP_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_WMARK_SB);
}
static void lpa_enable_hlb_wmark(struct lpa_drv *lpa, u32 wmark_ctrl,
	u32 cpu_id)
{
	u32 val;

	val = LPA_REG_READL(lpa, LPA_OBUF_WMARK_HLB);
	val &= ~LPA_OBUF_HLB_WMARK_CTRL_BMSK;
	val &= ~LPA_OBUF_HLB_WMARK_MAP_BMSK;
	val |= (wmark_ctrl << LPA_OBUF_HLB_WMARK_CTRL_SHFT) &
	LPA_OBUF_HLB_WMARK_CTRL_BMSK;
	val |= (cpu_id << LPA_OBUF_HLB_WMARK_MAP_SHFT) &
	LPA_OBUF_HLB_WMARK_MAP_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_WMARK_HLB);
}

static void lpa_enable_utc(struct lpa_drv *lpa, bool enable, u32 cpu_id)
{
	u32 val;

	val = (cpu_id << LPA_OBUF_UTC_CONFIG_MAP_SHFT) &
	LPA_OBUF_UTC_CONFIG_MAP_BMSK;
	enable = (enable ? 1 : 0);
	val = (enable << LPA_OBUF_UTC_CONFIG_EN_SHFT) &
	LPA_OBUF_UTC_CONFIG_EN_BMSK;
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_UTC_CONFIG);
}

static void lpa_enable_mixing(struct lpa_drv *lpa, bool enable)
{
	u32 val;

	val = LPA_REG_READL(lpa, LPA_OBUF_CONTROL);
	val = (enable ? val | LPA_OBUF_CONTROL_LLB_EN_BMSK :
		val & ~LPA_OBUF_CONTROL_LLB_EN_BMSK);
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_CONTROL);
}

static void lpa_enable_mixer_saturation(struct lpa_drv *lpa, u32 buf_id,
	bool enable)
{
	u32 val;

	val = LPA_REG_READL(lpa, LPA_OBUF_CONTROL);

	switch (buf_id) {
	case LPA_BUF_ID_LLB:
		val = enable ? (val | LPA_OBUF_CONTROL_LLB_SAT_EN_BMSK) :
		(val & ~LPA_OBUF_CONTROL_LLB_SAT_EN_BMSK);
		break;

	case LPA_BUF_ID_SB:
		val = enable ? (val | LPA_OBUF_CONTROL_SB_SAT_EN_BMSK) :
		(val & ~LPA_OBUF_CONTROL_SB_SAT_EN_BMSK);
		break;
	}

	LPA_REG_WRITEL(lpa, val, LPA_OBUF_CONTROL);
}

static void lpa_enable_obuf(struct lpa_drv *lpa, u32 buf_id, bool enable)
{
	u32 val;

	val = LPA_REG_READL(lpa, LPA_OBUF_CONTROL);

	switch (buf_id) {
	case LPA_BUF_ID_HLB:
		val = enable ? (val | LPA_OBUF_CONTROL_HLB_EN_BMSK) :
			(val & ~LPA_OBUF_CONTROL_HLB_EN_BMSK);
		break;

	case LPA_BUF_ID_LLB:
		val = enable ? (val | LPA_OBUF_CONTROL_LLB_EN_BMSK) :
		(val & ~LPA_OBUF_CONTROL_LLB_EN_BMSK);
		break;

	case LPA_BUF_ID_SB:
		val = enable ? (val | LPA_OBUF_CONTROL_SB_EN_BMSK) :
			(val & ~LPA_OBUF_CONTROL_SB_EN_BMSK);
		break;
	}
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_CONTROL);
}

struct lpa_drv *lpa_get(void)
{
	struct lpa_mem_bank_select mem_bank;
	struct lpa_drv *ret_lpa = &the_lpa_state.lpa_drv;

	mutex_lock(&the_lpa_state.lpa_lock);
	if (the_lpa_state.assigned) {
		MM_ERR("LPA HW accupied\n");
		ret_lpa = ERR_PTR(-EBUSY);
		goto error;
	}
	/* perform initialization */
	lpa_reset(ret_lpa);
	/* Config adec param */
	/* Initialize LLB/SB min/max address */
	lpa_switch_sb(ret_lpa);
	/* Config HLB minx/max address */
	lpa_config_hlb_addr(ret_lpa);

	/* Power up all memory bank for now */
	mem_bank.b0 = 1;
	mem_bank.b1 = 1;
	mem_bank.b2 = 1;
	mem_bank.b3 = 1;
	mem_bank.b4 = 1;
	mem_bank.b5 = 1;
	mem_bank.b6 = 1;
	mem_bank.b7 = 1;
	mem_bank.b8 = 1;
	mem_bank.b9 = 1;
	mem_bank.b10 = 1;
	mem_bank.llb = 1;
	lpa_powerup_mem_bank(ret_lpa, &mem_bank);

	while
	(lpa_req_wmark_id(ret_lpa) != LPA_OBUF_WMARK_ASSIGN_DONE);

	lpa_enable_llb_wmark(ret_lpa, LPA_WMARK_CTL_DISABLED, 0,
	ret_lpa->dsp_proc_id);
	lpa_enable_llb_wmark(ret_lpa, LPA_WMARK_CTL_DISABLED, 1,
	ret_lpa->dsp_proc_id);
	lpa_enable_llb_wmark(ret_lpa, LPA_WMARK_CTL_DISABLED, 2,
	ret_lpa->app_proc_id);
	lpa_enable_llb_wmark(ret_lpa, LPA_WMARK_CTL_DISABLED, 3,
	ret_lpa->app_proc_id);
	lpa_enable_hlb_wmark(ret_lpa, LPA_WMARK_CTL_DISABLED,
	ret_lpa->dsp_proc_id);
	lpa_enable_sb_wmark(ret_lpa, LPA_WMARK_CTL_DISABLED,
	ret_lpa->dsp_proc_id);
	lpa_enable_utc(ret_lpa, 0, LPA_OBUF_UTC_CONFIG_NO_INTR);

	lpa_enable_mixing(ret_lpa, 1);
	lpa_enable_mixer_saturation(ret_lpa, LPA_BUF_ID_LLB, 1);

	lpa_enable_obuf(ret_lpa, LPA_BUF_ID_HLB, 0);
	lpa_enable_obuf(ret_lpa, LPA_BUF_ID_LLB, 1);
	if (ret_lpa->status & LPA_STATUS_SBUF_EN) {
		lpa_enable_mixer_saturation(ret_lpa, LPA_BUF_ID_SB, 1);
		lpa_enable_obuf(ret_lpa, LPA_BUF_ID_SB, 1);
	}

	lpa_enable_interrupt(ret_lpa, ret_lpa->dsp_proc_id);
	mb();
	the_lpa_state.assigned++;
error:
	mutex_unlock(&the_lpa_state.lpa_lock);
	return ret_lpa;
}
EXPORT_SYMBOL(lpa_get);

void lpa_put(struct lpa_drv *lpa)
{

	mutex_lock(&the_lpa_state.lpa_lock);
	if (!lpa || &the_lpa_state.lpa_drv != lpa) {
		MM_ERR("invalid arg\n");
		goto error;
	}
	/* Deinitialize */
	the_lpa_state.assigned--;
error:
	mutex_unlock(&the_lpa_state.lpa_lock);
}
EXPORT_SYMBOL(lpa_put);

int lpa_cmd_codec_config(struct lpa_drv *lpa,
	struct lpa_codec_config *config_ptr)
{
	u32 sample_rate;
	u32 num_channels;
	u32 width;
	u32 val = 0;

	if (!lpa || !config_ptr) {
		MM_ERR("invalid parameters\n");
		return -EINVAL;
	}

	switch (config_ptr->num_channels) {
	case 8:
		num_channels = LPA_NUM_CHAN_7P1;
		break;
	case 6:
		num_channels = LPA_NUM_CHAN_5P1;
		break;
	case 4:
		num_channels = LPA_NUM_CHAN_4_CHANNEL;
		break;
	case 2:
		num_channels = LPA_NUM_CHAN_STEREO;
		break;
	case 1:
		num_channels = LPA_NUM_CHAN_MONO;
		break;
	default:
		MM_ERR("unsupported number of channel\n");
		goto error;
	}
	val |= (num_channels << LPA_OBUF_CODEC_NUM_CHAN_SHFT) &
	LPA_OBUF_CODEC_NUM_CHAN_BMSK;

	switch (config_ptr->sample_rate) {
	case 96000:
		sample_rate = LPA_SAMPLE_RATE_96KHZ;
		break;
	case 64000:
		sample_rate = LPA_SAMPLE_RATE_64KHZ;
		break;
	case 48000:
		sample_rate = LPA_SAMPLE_RATE_48KHZ;
		break;
	case 44100:
		sample_rate = LPA_SAMPLE_RATE_44P1KHZ;
		break;
	case 32000:
		sample_rate = LPA_SAMPLE_RATE_32KHZ;
		break;
	case 22050:
		sample_rate = LPA_SAMPLE_RATE_22P05KHZ;
		break;
	case 16000:
		sample_rate = LPA_SAMPLE_RATE_16KHZ;
		break;
	case 11025:
		sample_rate = LPA_SAMPLE_RATE_11P025KHZ;
		break;
	case 8000:
		sample_rate = LPA_SAMPLE_RATE_8KHZ;
		break;
	default:
		MM_ERR("unsupported sample rate \n");
		goto error;
	}
	val |= (sample_rate << LPA_OBUF_CODEC_SAMP_SHFT) &
		LPA_OBUF_CODEC_SAMP_BMSK;
	switch (config_ptr->sample_width) {
	case 32:
		width = LPA_BITS_PER_CHAN_32BITS;
		break;
	case 24:
		width = LPA_BITS_PER_CHAN_24BITS;
		break;
	case 16:
		width = LPA_BITS_PER_CHAN_16BITS;
		break;
	default:
		MM_ERR("unsupported sample width \n");
		goto error;
	}
	val |= (width << LPA_OBUF_CODEC_BITS_PER_CHAN_SHFT) &
		LPA_OBUF_CODEC_BITS_PER_CHAN_BMSK;

	val |= LPA_OBUF_CODEC_LOAD_BMSK;
	val |= (config_ptr->output_interface << LPA_OBUF_CODEC_INTF_SHFT) &
	LPA_OBUF_CODEC_INTF_BMSK;

	LPA_REG_WRITEL(lpa, val, LPA_OBUF_CODEC);
	mb();

	return 0;
error:
	return -EINVAL;
}
EXPORT_SYMBOL(lpa_cmd_codec_config);

static int lpa_check_llb_clear(struct lpa_drv *lpa)
{
	u32 val;
	val = LPA_REG_READL(lpa, LPA_OBUF_STATUS);

	return !(val & LPA_OBUF_STATUS_LLB_CLR_BMSK);
}

static void lpa_clear_llb(struct lpa_drv *lpa)
{
	u32 val;

	val = LPA_REG_READL(lpa, LPA_OBUF_CONTROL);
	LPA_REG_WRITEL(lpa, (val | LPA_OBUF_CONTROL_LLB_CLR_CMD_BMSK),
	LPA_OBUF_CONTROL);
	lpa_enable_obuf(lpa, LPA_BUF_ID_LLB, 0);

	while (!lpa_check_llb_clear(lpa))
		udelay(100);
	LPA_REG_WRITEL(lpa, val, LPA_OBUF_CONTROL);
}

int lpa_cmd_enable_codec(struct lpa_drv *lpa, bool enable)
{
	u32 val;
	struct lpa_mem_bank_select mem_bank;

	MM_DBG(" %s\n", (enable ? "enable" : "disable"));

	if (!lpa)
		return -EINVAL;

	val = LPA_REG_READL(lpa, LPA_OBUF_CODEC);

	if (enable) {
		if (val & LPA_OBUF_CODEC_CODEC_INTF_EN_BMSK)
			return -EBUSY;
		/* Power up all memory bank for now */
		mem_bank.b0 = 1;
		mem_bank.b1 = 1;
		mem_bank.b2 = 1;
		mem_bank.b3 = 1;
		mem_bank.b4 = 1;
		mem_bank.b5 = 1;
		mem_bank.b6 = 1;
		mem_bank.b7 = 1;
		mem_bank.b8 = 1;
		mem_bank.b9 = 1;
		mem_bank.b10 = 1;
		mem_bank.llb = 1;
		lpa_powerup_mem_bank(lpa, &mem_bank);

		/*clear LLB*/
		lpa_clear_llb(lpa);

		lpa_enable_codec(lpa, 1);
		MM_DBG("LPA codec is enabled\n");
	} else {
		if (val & LPA_OBUF_CODEC_CODEC_INTF_EN_BMSK) {
			lpa_enable_codec(lpa, 0);
			MM_DBG("LPA codec is disabled\n");
		} else
			MM_ERR("LPA codec is already disable\n");
	}
	mb();
	return 0;
}
EXPORT_SYMBOL(lpa_cmd_enable_codec);

static int lpa_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *mem_src;
	struct msm_lpa_platform_data *pdata;

	MM_INFO("lpa probe\n");

	if (!pdev || !pdev->dev.platform_data) {
		MM_ERR("no plaform data\n");
		rc = -ENODEV;
		goto error;
	}

	mem_src = platform_get_resource_byname(pdev, IORESOURCE_MEM, "lpa");
	if (!mem_src) {
		MM_ERR("LPA base address undefined\n");
		rc = -ENODEV;
		goto error;
	}

	pdata = pdev->dev.platform_data;
	the_lpa_state.lpa_drv.baseaddr = ioremap(mem_src->start,
	(mem_src->end - mem_src->start) + 1);
	if (!the_lpa_state.lpa_drv.baseaddr) {
		rc = -ENOMEM;
		goto error;
	}

	the_lpa_state.lpa_drv.obuf_hlb_size = pdata->obuf_hlb_size;
	the_lpa_state.lpa_drv.dsp_proc_id = pdata->dsp_proc_id;
	the_lpa_state.lpa_drv.app_proc_id = pdata->app_proc_id;
	the_lpa_state.lpa_drv.nosb_config = pdata->nosb_config;
	the_lpa_state.lpa_drv.sb_config = pdata->sb_config;
	/* default to enable summing buffer */
	the_lpa_state.lpa_drv.status = LPA_STATUS_SBUF_EN;

error:
	return rc;

}

static int lpa_remove(struct platform_device *pdev)
{
	iounmap(the_lpa_state.lpa_drv.baseaddr);
	return 0;
}

static struct platform_driver lpa_driver = {
	.probe = lpa_probe,
	.remove = lpa_remove,
	.driver = {
		.name = "lpa",
		.owner = THIS_MODULE,
	},
};

static int __init lpa_init(void)
{
	the_lpa_state.assigned = 0;
	mutex_init(&the_lpa_state.lpa_lock);
	return platform_driver_register(&lpa_driver);
}

static void __exit lpa_exit(void)
{
	platform_driver_unregister(&lpa_driver);
}

module_init(lpa_init);
module_exit(lpa_exit);

MODULE_DESCRIPTION("MSM LPA driver");
MODULE_LICENSE("GPL v2");

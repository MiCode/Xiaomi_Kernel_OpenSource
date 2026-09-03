// SPDX-License-Identifier: GPL-2.0
/*copyright (C) 2018 Spreadtrum Communications Inc.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include "sprd_ptm.h"

#define CREATE_TRACE_POINTS
#include "sprd_ptm_trace.h"

static const struct of_device_id sprd_ptm_of_match[] = {
	{ .compatible = "sprd,sharkle-ptm", .data = &ptm_v1_data},
	{ .compatible = "sprd,sharkl3-ptm", .data = &ptm_v2_data},
	{ .compatible = "sprd,sharkl5-ptm", .data = &ptm_v2_data},
	{ .compatible = "sprd,roc1-ptm", .data = &ptm_v2_data},
	{ .compatible = "sprd,orca-ptm", .data = &ptm_v2_data},
	{ .compatible = "sprd,sharkl5pro-ptm", .data = &ptm_v2_data},
	{ .compatible = "sprd,qogirl6-ptm", .data = &ptm_v2_data},
	{ .compatible = "sprd,qogirn6pro-ptm", .data = &ptm_v2_data},
	{ .compatible = "sprd,qogirn6l-ptm", .data = &ptm_v3_data},
	{ },
};
static struct attribute_group ptm_legacy_group;
static struct attribute_group ptm_trace_group;

static inline u32 ptm_get_wbm_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->wbm_base;
}

static inline u32 ptm_get_rbm_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->rbm_base;
}

static inline u32 ptm_get_wly_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->wly_base;
}

static inline u32 ptm_get_rly_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->rly_base;
}

static inline u32 ptm_get_wtran_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->wtran_base;
}

static inline u32 ptm_get_rtran_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->rtran_base;
}

#ifdef CONFIG_SPRD_PTM_DIFF_R6P1
static inline u32 ptm_get_dpu_dcam_ovf_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->dpu_dcam_ovf_base;
}
#endif

static inline u32 ptm_get_msterid_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->msterid_base;
}

static inline u32 ptm_get_usrid_base(struct sprd_ptm_dev *sdev)
{
	return sdev->pvt_data->usrid_base;
}

static inline void ptm_cs_lock(void __iomem *addr)
{
	/* Wait for things to settle */
	mb();
	writel_relaxed(0x0, addr + PTM_CORESIGHT_LAR);
}

static inline void ptm_cs_unlock(void __iomem *addr)
{
	writel_relaxed(PTM_CORESIGHT_UNLOCK, addr + PTM_CORESIGHT_LAR);
	/* Make sure everyone has seen this */
	mb();
}

static void sprd_ptm_set_enable(struct sprd_ptm_dev *sdev, bool enable)
{
	u32 tmp = readl_relaxed(sdev->base + PTM_EN);

	if (enable) {
		tmp |= PTM_BW_LTCY_CNT_EN;
		writel_relaxed(tmp, sdev->base + PTM_EN);
	} else {
		tmp &= ~PTM_BW_LTCY_CNT_EN;
		writel_relaxed(tmp, sdev->base + PTM_EN);
	}
}

static void sprd_ptm_set_lty_enable(struct sprd_ptm_dev *sdev)
{
	u32 tmp = readl_relaxed(sdev->base + PTM_EN);

	writel_relaxed(tmp | PTM_ENABLE | PTM_BW_LTCY_CNT_EN,
		       sdev->base + PTM_EN);
}

static void sprd_ptm_set_lty_trace_enable(struct sprd_ptm_dev *sdev)
{
	writel_relaxed(PTM_ENABLE |
		       PTM_BW_LTCY_CNT_EN |
		       PTM_BW_LTCY_ALL_TRACE_EN |
		       PTM_TRACE_QOS_BWITCY_EN |
		       PTM_LTCY_TRACE_EN_MSK << PTM_LTCY_TRACE_EN_OFFSET,
		       sdev->base + PTM_EN);
}

static void sprd_ptm_set_cmd_enable(struct sprd_ptm_dev *sdev)
{
	writel_relaxed(PTM_ENABLE |
		       PTM_CMD_TRANS_EN |
		       PTM_TRACE_AREN,
		       sdev->base + PTM_EN);
}

static void sprd_ptm_set_lty_mode(struct sprd_ptm_dev *sdev)
{
	u32 val;

	val = readl_relaxed(sdev->base + MOD_SEL);
	val |= sdev->chn_info.lty_mode << PTM_LTCY_MOD_OFFSET;
	writel_relaxed(val, sdev->base + MOD_SEL);
}

static void sprd_ptm_set_trace_mode(struct sprd_ptm_dev *sdev,
				    enum ptm_trace_mode mode)
{
	u32 val;

	val = readl(sdev->base + MOD_SEL);
	val |= mode << PTM_TRACE_MOD_OFFSET;
	writel_relaxed(val, sdev->base + MOD_SEL);
}

static void sprd_ptm_grp_sel(struct sprd_ptm_dev *sdev)
{
	/* if no one config grp_sel, use default val */
	if (!sdev->chn_info.grp_sel) {
		writel_relaxed(sdev->grp_sel, sdev->base + GRP_SEL);
		sdev->chn_info.grp_sel = sdev->grp_sel;
	} else {
		writel_relaxed(sdev->chn_info.grp_sel, sdev->base + GRP_SEL);
	}
}

static void sprd_ptm_set_msterid(struct sprd_ptm_dev *sdev)
{
	u32 mster_base = ptm_get_msterid_base(sdev);
	u32 msterid, msterid_mask, master_val, chn_sel;

	/* if there is no mask val, use 0xffff0000 */
	if (sdev->chn_info.mster_id_mask)
		msterid_mask = sdev->chn_info.mster_id_mask;
	else
		msterid_mask = PTM_MASTERID_MSK;
	msterid = sdev->chn_info.mster_id;
	chn_sel = sdev->chn_info.chnsel;
	master_val = ((msterid_mask & PTM_MASTERID_MSK) << 16) | (msterid & PTM_MASTERID_MSK);

	writel_relaxed(master_val, sdev->base + mster_base + 4 * chn_sel);
}

static void sprd_ptm_set_usrid(struct sprd_ptm_dev *sdev)
{
	u32 usrid_base = ptm_get_usrid_base(sdev);
	u32 usrid, usrid_mask, usr_bit_val, chn_sel;

	/* if there is no mask val, use 0xff00 */
	if (sdev->chn_info.usr_id_mask)
		usrid_mask = sdev->chn_info.usr_id_mask;
	else
		usrid_mask = PTM_USRID_MSK;
	usrid = sdev->chn_info.usr_id;
	chn_sel = sdev->chn_info.chnsel;
	usr_bit_val = ((usrid_mask & PTM_USRID_MSK) << PTM_USRID_MSK_OFFSET) |
		      (usrid & PTM_USRID_MSK);

	writel_relaxed(usr_bit_val, sdev->base + usrid_base + 4 * chn_sel);
}

static void sprd_ptm_set_winlen(struct sprd_ptm_dev *sdev, u32 len)
{
	writel_relaxed(len, sdev->base + CNT_WIN);
}

static void sprd_ptm_tpiu_enable(struct sprd_ptm_dev *sdev)
{
	void __iomem *tpiu_base = sdev->mode_info.trace.tpiu_base;

	ptm_cs_unlock(tpiu_base);
	writel_relaxed(PTM_TPIU_PORT_SZ, tpiu_base + PTM_TPIU_CUR_PORTSZ);
	ptm_cs_lock(tpiu_base);
}

static void sprd_ptm_tpiu_disable(struct sprd_ptm_dev *sdev)
{
	void __iomem *tpiu_base = sdev->mode_info.trace.tpiu_base;

	ptm_cs_unlock(tpiu_base);
	writel_relaxed(0, tpiu_base + PTM_TPIU_CUR_PORTSZ);
	ptm_cs_lock(tpiu_base);
}

static void sprd_ptm_etf_enable(struct sprd_ptm_dev *sdev)
{
	void __iomem *tmc_base = sdev->mode_info.trace.tmc_base;

	ptm_cs_unlock(tmc_base);
	/* reference coresight tmc etf enable */
	writel_relaxed(2, tmc_base + PTM_TMC_MODE);
	writel_relaxed(PTM_TMC_FFCR_EN_FMT | PTM_TMC_FFCR_EN_TI,
		       tmc_base + PTM_TMC_FFCR);
	writel_relaxed(0, tmc_base + PTM_TMC_BUFWM);
	writel_relaxed(1, tmc_base + PTM_TMC_CTL);
	ptm_cs_lock(tmc_base);
}

static void sprd_ptm_etf_disable(struct sprd_ptm_dev *sdev)
{
	void __iomem *tmc_base = sdev->mode_info.trace.tmc_base;
	u32 ffcr;

	ptm_cs_unlock(tmc_base);
	/* reference coresight etf disable */
	ffcr = readl_relaxed(tmc_base + PTM_TMC_FFCR);
	ffcr |= PTM_TMC_FFCR_STOP_ON_FLUSH;
	writel_relaxed(ffcr, tmc_base + PTM_TMC_FFCR);
	ffcr |= PTM_TMC_FFCR_FLUSHMAN;
	writel_relaxed(ffcr, tmc_base + PTM_TMC_FFCR);
	/* Ensure flush completes */
	udelay(10);
	writel_relaxed(0x0, tmc_base + PTM_TMC_CTL);
	ptm_cs_lock(tmc_base);
}

static void sprd_ptm_funnel_enable(struct sprd_ptm_dev *sdev)
{
	void __iomem *funnel_base = sdev->mode_info.trace.funnel_base;
	int port = sdev->mode_info.trace.funnel_port;
	u32 functl;

	ptm_cs_unlock(funnel_base);
	functl = (1UL << port) | PTM_FUNNEL_HOLDTIME;
	writel_relaxed(functl, funnel_base + PTM_FUNNEL_FUNCTL);
	ptm_cs_lock(funnel_base);
}

static void sprd_ptm_funnel_disable(struct sprd_ptm_dev *sdev)
{
	void __iomem *funnel_base = sdev->mode_info.trace.funnel_base;

	ptm_cs_unlock(funnel_base);
	writel_relaxed(0, funnel_base + PTM_FUNNEL_FUNCTL);
	ptm_cs_lock(funnel_base);
}

static int of_get_ptm_memory_info(struct sprd_ptm_dev *mem, struct device_node *np)
{
	struct device_node *node;
	struct resource r;
	int ret;

	node = of_find_node_by_name(NULL, "reserved-memory");
	if (!node) {
		pr_err("of_find_node_by_name:get reserved-memory fail\n");
		return -EINVAL;
		}
	node = of_find_node_by_name(NULL, "ptm-mem");
	if (!node) {
		pr_err("of_find_node_by_name:get ptm reserverd info fail\n");
		return -EINVAL;
		}
	ret = of_address_to_resource(node, 0, &r);
	of_node_put(node);
	if (ret) {
		pr_err("invalid ptm reserved memory node!\n");
		return -EINVAL;
		}

	mem->ptm_addr = r.start;
	mem->ptm_size = resource_size(&r);
		pr_info("ptm memory addr:0x%x size:0x%x\n",
			mem->ptm_addr, mem->ptm_size);
	return 0;
}

static void sprd_ptm_etr_enable(struct sprd_ptm_dev *sdev)
{
	void __iomem *etr_base = sdev->mode_info.trace.etr_base;

	ptm_cs_unlock(etr_base);
	writel_relaxed(PTM_CORESIGHT_UNLOCK, etr_base + PTM_CORESIGHT_LAR);
	writel_relaxed(0x1, etr_base + PTM_TMC_AXICTL);
	writel_relaxed(0x1000, etr_base + PTM_TMC_FFCR);
	writel_relaxed(0x7, etr_base + PTM_TMC_PSCR);
	writel_relaxed(sdev->ptm_size, etr_base + PTM_TMC_RSZ);
	writel_relaxed(sdev->ptm_addr, etr_base + PTM_TMC_DBALO);
	writel_relaxed(0x0, etr_base + PTM_TMC_DBAHI);
	writel_relaxed(0x1, etr_base + PTM_TMC_CTL);
	ptm_cs_lock(etr_base);
}

static void sprd_ptm_etr_disable(struct sprd_ptm_dev *sdev)
{
	void __iomem *etr_base = sdev->mode_info.trace.etr_base;

	ptm_cs_unlock(etr_base);
	writel_relaxed(0x0, etr_base + PTM_TMC_CTL);
	ptm_cs_lock(etr_base);
}

static void sprd_ptm_replicator_enable(struct sprd_ptm_dev *sdev)
{
	void __iomem *replicator_base = sdev->mode_info.trace.replicator_base;

	ptm_cs_unlock(replicator_base);
	writel_relaxed(0xFF, replicator_base);
	writel_relaxed(0x0, replicator_base + 0x4);
	ptm_cs_lock(replicator_base);
}

static void sprd_ptm_replicator_disable(struct sprd_ptm_dev *sdev)
{
	void __iomem *replicator_base = sdev->mode_info.trace.replicator_base;

	ptm_cs_unlock(replicator_base);
	writel_relaxed(0x0, replicator_base);
	writel_relaxed(0x0, replicator_base + 0x4);
	ptm_cs_lock(replicator_base);
}

static int sprd_ptm_trace_enable(struct sprd_ptm_dev *sdev)
{
	u32 winlen = sdev->mode_info.trace.winlen;
	u32 out_mod = sdev->mode_info.trace.out_mode;
	bool cmd_eb = sdev->mode_info.trace.cmd_eb;
	int ret;

	clk_set_parent(sdev->clk_cs, sdev->clk_cs_src);
	ret = clk_prepare_enable(sdev->clk_cs);
	if (ret)
		return ret;

	if (out_mod == TPIU_MODE) {
		sprd_ptm_tpiu_enable(sdev);
	} else {
		sprd_ptm_etr_enable(sdev);
		sprd_ptm_replicator_enable(sdev);
}
	sprd_ptm_etf_enable(sdev);
	sprd_ptm_funnel_enable(sdev);

	if (cmd_eb) {
		sprd_ptm_set_cmd_enable(sdev);
	} else {
		sprd_ptm_set_winlen(sdev, winlen);
		sprd_ptm_set_lty_mode(sdev);
		sprd_ptm_set_trace_mode(sdev, AUTO_TRACE_MOD);
		sprd_ptm_set_lty_trace_enable(sdev);
	}

	sdev->mode_info.trace.trace_st = true;
	return 0;
}

static int sprd_ptm_trace_disable(struct sprd_ptm_dev *sdev)
{
	u32 out_mod = sdev->mode_info.trace.out_mode;
	sprd_ptm_funnel_disable(sdev);
	sprd_ptm_etf_disable(sdev);
	if (out_mod == TPIU_MODE) {
		sprd_ptm_tpiu_disable(sdev);
	} else {
		sprd_ptm_etr_disable(sdev);
		sprd_ptm_replicator_disable(sdev);
	}
	clk_disable_unprepare(sdev->clk_cs);

	sdev->mode_info.trace.trace_st = false;
	return 0;
}

static void sprd_ptm_trace_event(struct sprd_ptm_dev *sdev,
				 struct bm_per_info *bm_info)
{
	const char *chn_name;
	u32 rd_cnt, wr_cnt, rd_bw, wr_bw, rd_lty, wr_lty;
	int chn;

	trace_ptm_ddr_ts(bm_info->t_start);
	for (chn = 0; chn < sdev->pub_chn; chn++) {
		chn_name = sdev->sprd_ptm_list[chn];
		rd_cnt = bm_info->perf_data[chn][0];
		rd_bw = bm_info->perf_data[chn][1];
		rd_lty = bm_info->perf_data[chn][2];
		wr_cnt = bm_info->perf_data[chn][3];
		wr_bw = bm_info->perf_data[chn][4];
		wr_lty = bm_info->perf_data[chn][5];
		trace_ptm_ddr_info(chn_name, rd_cnt, rd_bw, rd_lty,
				   wr_cnt, wr_bw, wr_lty);
	}
}

static enum hrtimer_restart
sprd_ptm_legacy_time_handler(struct hrtimer *timer)
{
	struct sprd_ptm_dev *sdev = container_of(timer, struct sprd_ptm_dev,
		mode_info.legacy.timer);
	struct bm_per_info *bm_info;
	u32 wbm_base = ptm_get_wbm_base(sdev);
	u32 rbm_base = ptm_get_rbm_base(sdev);
	u32 wly_base = ptm_get_wly_base(sdev);
	u32 rly_base = ptm_get_rly_base(sdev);
	u32 wtran_base = ptm_get_wtran_base(sdev);
	u32 rtran_base = ptm_get_rtran_base(sdev);
#ifdef CONFIG_SPRD_PTM_DIFF_R6P1
	u32 dpu_dcam_ovf_base = ptm_get_dpu_dcam_ovf_base(sdev);
	int i, j;
#endif
	u64 ts_val;
	static u32 num;
	u32 wr_cnt;
	int chn = 0;

	spin_lock(&sdev->slock);
	bm_info = (struct bm_per_info *)sdev->mode_info.legacy.per_buf;
	if (bm_info == NULL) {
		pr_err("PTM handler ERR, BM dev err, can get perf buf!\n");
		spin_unlock(&sdev->slock);
		return HRTIMER_NORESTART;
	}
	wr_cnt = sdev->mode_info.legacy.bm_buf_write_cnt;
	/* count stop time stamp */
	ts_val = ktime_get_boottime_ns();
	bm_info[wr_cnt].t_stop = (u32)ts_val;
	bm_info[wr_cnt].count = num++;
	/* it should clear ptm eb before read ptm data */
#ifndef CONFIG_SPRD_PTM_R6P2
	sprd_ptm_set_enable(sdev, false);
#else
	writel_relaxed(3, sdev->base + FRE_CHG);
#endif
	for (chn = 0; chn < sdev->pub_chn; chn++) {
		bm_info[wr_cnt].perf_data[chn][0] =
			readl_relaxed(sdev->base + rtran_base + 4 * chn);
		bm_info[wr_cnt].perf_data[chn][1] =
			readl_relaxed(sdev->base + rbm_base + 4 * chn);
		bm_info[wr_cnt].perf_data[chn][2] =
			readl_relaxed(sdev->base + rly_base + 4 * chn);
		bm_info[wr_cnt].perf_data[chn][3] =
			readl_relaxed(sdev->base + wtran_base + 4 * chn);
		bm_info[wr_cnt].perf_data[chn][4] =
			readl_relaxed(sdev->base + wbm_base + 4 * chn);
		bm_info[wr_cnt].perf_data[chn][5] =
			readl_relaxed(sdev->base + wly_base + 4 * chn);
	}
#ifdef CONFIG_SPRD_PTM_DIFF_R6P1
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 10; j++) {
			bm_info[wr_cnt].dpu_dcam_ovf[i][j] =
				(readl_relaxed(sdev->base + dpu_dcam_ovf_base + 4 * j) >>
				 (16 * (1 - i))) & 0xffff;
		}
	}
#endif
	sprd_ptm_set_enable(sdev, true);
#ifndef CONFIG_SPRD_PTM_R6P2
	/* clear ptm count*/
	writel_relaxed(1, sdev->base + CNT_CLR);
	writel_relaxed(0, sdev->base + CNT_CLR);
#endif
	if (trace_ptm_ddr_info_enabled())
		sprd_ptm_trace_event(sdev, &bm_info[wr_cnt]);
	if (++sdev->mode_info.legacy.bm_buf_write_cnt ==
		BM_PER_CNT_RECORD_SIZE)
		sdev->mode_info.legacy.bm_buf_write_cnt = 0;
	/* wake up the thread to output log per 4 second */
	if (!sdev->mode_info.legacy.bm_buf_write_cnt  ||
		sdev->mode_info.legacy.bm_buf_write_cnt ==
		(BM_PER_CNT_RECORD_SIZE >> 1))
		complete(&sdev->comp);
	/* count start time stamp */
	bm_info[sdev->mode_info.legacy.bm_buf_write_cnt].t_start = (u32)ts_val;

	hrtimer_forward_now(timer, ktime_set(0,
		sdev->mode_info.legacy.timer_interval * NSEC_PER_MSEC));
	spin_unlock(&sdev->slock);

	return HRTIMER_RESTART;
}

static int sprd_ptm_legacy_thread(void *data)
{
	struct sprd_ptm_dev *sdev = (struct sprd_ptm_dev *)data;
	struct file *bm_perf_file = NULL;
	u32 bm_read_cnt = 0;
	int rval;

	while (!kthread_should_stop()) {
		wait_for_completion(&sdev->comp);
		if (!bm_perf_file) {
			bm_perf_file = filp_open(BM_LOG_FILE_PATH,
				O_RDWR | O_CREAT | O_TRUNC, 0644);
			if (IS_ERR(bm_perf_file)) {
				dev_err(sdev->misc.parent, "file_open(%s) for create failed\n",
					BM_LOG_FILE_PATH);
				return PTR_ERR(bm_perf_file);
			}
		}

		if (sdev->mode_info.legacy.bm_buf_write_cnt <
			BM_PER_CNT_RECORD_SIZE >> 1)
			bm_read_cnt = BM_PER_CNT_RECORD_SIZE >> 1;
		else
			bm_read_cnt = 0;

		rval = kernel_write(bm_perf_file,
			sdev->mode_info.legacy.per_buf +
			bm_read_cnt * sizeof(struct bm_per_info),
			sizeof(struct bm_per_info)
			* (BM_PER_CNT_RECORD_SIZE >> 1),
			&bm_perf_file->f_pos);

		/*raw back file write*/
		if (bm_perf_file->f_pos >= (sizeof(struct bm_per_info)
			* BM_LOG_FILE_MAX_RECORDS)) {
			bm_perf_file->f_pos = 0x0;
		}
	}
	filp_close(bm_perf_file, NULL);
	bm_perf_file = NULL;
	return 0;
}

static int sprd_ptm_legacy_start(struct sprd_ptm_dev *sdev, u32 cir)
{
	trace_ptm_ddr_chn(sdev->pub_chn);
	reinit_completion(&sdev->comp);
	sdev->mode_info.legacy.per_buf = devm_kzalloc(sdev->misc.parent,
		BM_PER_CNT_BUF_SIZE,
		GFP_KERNEL);
	if (!sdev->mode_info.legacy.per_buf)
		return -ENOMEM;
	hrtimer_init(&sdev->mode_info.legacy.timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	sdev->mode_info.legacy.timer.function =
		sprd_ptm_legacy_time_handler;
	sdev->mode_info.legacy.timer_interval = cir;
	sdev->mode_info.legacy.bm_perf_st = true;
	sdev->mode_info.legacy.bm_thl =
		kthread_create(sprd_ptm_legacy_thread, sdev, "ptm_thread");
	if (IS_ERR(sdev->mode_info.legacy.bm_thl)) {
		dev_err(sdev->misc.parent, "Failed to create kthread: bm perf\n");
		devm_kfree(sdev->misc.parent, sdev->mode_info.legacy.per_buf);
		return PTR_ERR(sdev->mode_info.legacy.bm_thl);
	}
	sdev->mode_info.legacy.bm_buf_write_cnt = 0;
	wake_up_process(sdev->mode_info.legacy.bm_thl);
	sprd_ptm_set_winlen(sdev, PTM_REG_MAX);
	sprd_ptm_set_lty_mode(sdev);
	sprd_ptm_set_lty_enable(sdev);
	hrtimer_start(&sdev->mode_info.legacy.timer,
		ktime_set(0, cir * NSEC_PER_MSEC),
		HRTIMER_MODE_REL);

	return 0;
}

static void sprd_ptm_legacy_stop(struct sprd_ptm_dev *sdev)
{
	sdev->mode_info.legacy.bm_perf_st = false;
	if (sdev->mode_info.legacy.bm_thl)
		kthread_stop(sdev->mode_info.legacy.bm_thl);
	if (sdev->mode_info.legacy.per_buf)
		devm_kfree(sdev->misc.parent,
			sdev->mode_info.legacy.per_buf);
	sdev->mode_info.legacy.per_buf = NULL;
	sdev->mode_info.legacy.bm_thl = NULL;
	hrtimer_cancel(&sdev->mode_info.legacy.timer);
	writel_relaxed(0, sdev->base + CNT_WIN);
	writel_relaxed(0, sdev->base + PTM_EN);
}

static void sprd_ptm_init(struct device *dev)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

#ifdef CONFIG_SPRD_PTM_R6P2
	writel_relaxed(0, sdev->base + PTM_EN);
	writel_relaxed(2, sdev->base + FRE_CHG);
#else
	writel_relaxed(0 | PTM_TRACE_BW_IDLE_EN | PTM_TRACE_LTCY_IDLE_EN,
		       sdev->base + PTM_EN);
	writel_relaxed(0, sdev->base + FRE_CHG);
#endif
	writel_relaxed(0, sdev->base + INT_STU);
	writel_relaxed(1, sdev->base + MOD_SEL);
	writel_relaxed(0, sdev->base + FRE_INT);
	writel_relaxed(1, sdev->base + INT_CLR);
	writel_relaxed(0, sdev->base + INT_CLR);
	writel_relaxed(1, sdev->base + CNT_CLR);
	writel_relaxed(0, sdev->base + CNT_CLR);
	writel_relaxed(0, sdev->base + CNT_WIN);
}

static void sprd_ptm_deinit(struct device *dev)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	writel_relaxed(0, sdev->base + PTM_EN);
	writel_relaxed(0, sdev->base + INT_STU);
	writel_relaxed(0, sdev->base + FRE_CHG);
	writel_relaxed(0, sdev->base + MOD_SEL);
	writel_relaxed(0, sdev->base + FRE_INT);
	writel_relaxed(1, sdev->base + INT_CLR);
	writel_relaxed(0, sdev->base + INT_CLR);
	writel_relaxed(1, sdev->base + CNT_CLR);
	writel_relaxed(0, sdev->base + CNT_CLR);
	writel_relaxed(0, sdev->base + CNT_WIN);
	writel_relaxed(0, sdev->base + GRP_SEL);
}

static void sprd_ptm_legacy_init(struct device *dev)
{
	sysfs_merge_group(dev->kobj.parent, &ptm_legacy_group);
}

static void sprd_ptm_legacy_deinit(struct device *dev)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	sysfs_unmerge_group(dev->kobj.parent, &ptm_legacy_group);
	if (sdev->mode_info.legacy.bm_perf_st)
		sprd_ptm_legacy_stop(sdev);
}

static void sprd_ptm_trace_init(struct device *dev)
{
	sysfs_merge_group(dev->kobj.parent, &ptm_trace_group);
}

static void sprd_ptm_trace_deinit(struct device *dev)
{
	sysfs_unmerge_group(dev->kobj.parent, &ptm_trace_group);
}

/**
 * mode - select which monitor mode you want,
 * it includes 3 modes: initial/legacy/trace;
 * initial mode uses to init ptm;
 * legacy mode uses to monitor normal bandwidth info;
 * trace mode uses to trace detail bm info and output by cs.
 */
static ssize_t mode_show(struct device *dev,
			 struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	strcat(buf, "Mode selected: initial / legacy / trace\n");
	strcat(buf, "now mode is: ");
	switch (sdev->mode) {
	case INIT_MODE:
		strcat(buf, "initial mode.\n");
		break;
	case LEGACY_MODE:
		strcat(buf, "legacy mode.\n");
		break;
	case TRACE_MODE:
		strcat(buf, "trace mode.\n");
		break;
	default:
		strcat(buf, "bad mode.\n");
		break;
	}
	return strlen(buf);
}

static ssize_t mode_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	if (!strncmp(buf, "initial", 7)) {
		sprd_ptm_init(dev);
		sdev->mode = INIT_MODE;
	} else if (!strncmp(buf, "legacy", 6)) {
		sprd_ptm_legacy_init(dev);
		sdev->mode = LEGACY_MODE;
	} else if (!strncmp(buf, "trace", 5)) {
		sprd_ptm_trace_init(dev);
		sdev->mode = TRACE_MODE;
	} else {
		pr_info("Mode selected: initial legacy trace\n");
		return -EINVAL;
	}

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(mode);

static ssize_t lty_mode_show(struct device *dev,
			     struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 lty_mode = sdev->chn_info.lty_mode;

	return sprintf(buf, "ptm latency mode 0x%x\n", lty_mode);
}

static ssize_t lty_mode_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 lty_mode;
	int ret;

	ret = kstrtouint(buf, 0, &lty_mode);
	if (ret)
		return ret;

	sdev->chn_info.lty_mode = lty_mode;
	sprd_ptm_set_lty_mode(sdev);

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(lty_mode);

/**
 * grpsel - configure ptm group register to select monitor channels.
 * Notice! SHARKLE/SHARKL3 could not config this register in kernel.
 */
static ssize_t grpsel_show(struct device *dev,
			   struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 grp_sel = sdev->chn_info.grp_sel;

	return sprintf(buf, "ptm grp sel 0x%x\n", grp_sel);
}

static ssize_t grpsel_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 grpsel;
	int ret;

	ret = kstrtouint(buf, 0, &grpsel);
	if (ret)
		return ret;

	sdev->chn_info.grp_sel = grpsel;
	sprd_ptm_grp_sel(sdev);

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(grpsel);

static ssize_t chn_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	int chn, cnt = 0;

	for (chn = 0; chn < sdev->pub_chn; chn++)
		cnt += sprintf(buf + cnt,  "%d:	%s\n",
			chn, sdev->sprd_ptm_list[chn]);

	return cnt;
}
static DEVICE_ATTR_RO(chn);

/**
 * chn_sel - select one channel to configure it by software.
 */
static ssize_t chn_sel_show(struct device *dev,
			    struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	int chn_sel = sdev->chn_info.chnsel;

	return sprintf(buf, "ptm sel chn %d - %s\n",
			 chn_sel, sdev->sprd_ptm_list[chn_sel]);
}

static ssize_t chn_sel_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 chnsel;
	int ret;

	ret = kstrtouint(buf, 0, &chnsel);
	if (ret)
		return ret;

	sdev->chn_info.chnsel = chnsel;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(chn_sel);

/**
 * usrid - configure channel usrid to match user modules.
 * Notice! SHARKLE usrid only use to match bandwidth function.
 */
static ssize_t usrid_show(struct device *dev,
			  struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	return sprintf(buf, "ptm usid 0x%x\n", sdev->chn_info.usr_id);
}

static ssize_t usrid_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 usrid;
	int ret;

	ret = kstrtouint(buf, 0, &usrid);
	if (ret)
		return ret;

	sdev->chn_info.usr_id = usrid;
	sprd_ptm_set_usrid(sdev);

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(usrid);

static ssize_t usrid_mask_show(struct device *dev,
			       struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	return sprintf(buf, "ptm usid mask 0x%x\n", sdev->chn_info.usr_id_mask);
}

static ssize_t usrid_mask_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 mskid;
	int ret;

	ret = kstrtouint(buf, 0, &mskid);
	if (ret)
		return ret;

	sdev->chn_info.usr_id_mask = mskid;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(usrid_mask);

/**
 * msterid - configure channel msterid to match user modules.
 * Notice! SHARKLE msterid only use to match latency function.
 */
static ssize_t msterid_show(struct device *dev,
			    struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	return sprintf(buf, "ptm msterid 0x%x\n", sdev->chn_info.mster_id);
}

static ssize_t msterid_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 msterid;
	int ret;

	ret = kstrtouint(buf, 0, &msterid);
	if (ret)
		return ret;

	sdev->chn_info.mster_id = msterid;
	sprd_ptm_set_msterid(sdev);

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(msterid);

static ssize_t msterid_mask_show(struct device *dev,
				 struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	return sprintf(buf, "ptm msterid mask 0x%x\n",
			sdev->chn_info.mster_id_mask);
}

static ssize_t msterid_mask_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 mskid;
	int ret;

	ret = kstrtouint(buf, 0, &mskid);
	if (ret)
		return ret;

	sdev->chn_info.mster_id_mask = mskid;

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(msterid_mask);

static struct attribute *ptm_origin_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_lty_mode.attr,
	&dev_attr_grpsel.attr,
	&dev_attr_chn.attr,
	&dev_attr_chn_sel.attr,
	&dev_attr_usrid.attr,
	&dev_attr_usrid_mask.attr,
	&dev_attr_msterid.attr,
	&dev_attr_msterid_mask.attr,
	NULL,
};
static struct attribute_group ptm_origin_group = {
	.attrs = ptm_origin_attrs,
};

static ssize_t bandwidth_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 cir;
	int ret;

	ret = kstrtouint(buf, 0, &cir);
	if (ret)
		return ret;
	if (cir > 0 && !sdev->mode_info.legacy.bm_perf_st) {
		ret = sprd_ptm_legacy_start(sdev, cir);
		if (ret)
			return ret;
	} else if (sdev->mode_info.legacy.bm_perf_st && !cir) {
		sprd_ptm_legacy_stop(sdev);
	}
	return strnlen(buf, count);
}

static ssize_t bandwidth_show(struct device *dev,
			      struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);

	if (sdev->mode_info.legacy.bm_perf_st) {
		return sprintf(buf, "bandwidth enable: cnt window is %d ms!\n",
			sdev->mode_info.legacy.timer_interval);
	}
	return sprintf(buf, "ptm legacy bandwidth disable!\n");
}
static DEVICE_ATTR_RW(bandwidth);

static ssize_t data_show(struct device *dev,
			 struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	struct bm_per_info *bm_info;
	int cnt, n;

	if (!sdev->mode_info.legacy.bm_perf_st) {
		pr_err("ptm legacy bandwidth is not enable!!!\n");
		return -EFAULT;
	}
	bm_info = (struct bm_per_info *)sdev->mode_info.legacy.per_buf;
	for (n = 1; n <= BM_DATA_COUNT; n++) {
		cnt = sdev->mode_info.legacy.bm_buf_write_cnt - n;
		cnt = (cnt + BM_PER_CNT_RECORD_SIZE) % BM_PER_CNT_RECORD_SIZE;
		memcpy(buf + (n - 1) * sizeof(bm_info[cnt]),
			(void *)&bm_info[cnt], sizeof(bm_info[cnt]));
	}

	return BM_DATA_COUNT * sizeof(bm_info[cnt]);
}
static DEVICE_ATTR_RO(data);

static struct attribute *ptm_legacy_attrs[] = {
	&dev_attr_bandwidth.attr,
	&dev_attr_data.attr,
	NULL,
};

static struct attribute_group ptm_legacy_group = {
	.name = PTM_NAME,
	.attrs = ptm_legacy_attrs,
};

static ssize_t trace_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 enable;
	int ret;

	ret = kstrtouint(buf, 0, &enable);
	if (ret)
		return ret;
	if (enable && !sdev->mode_info.trace.trace_st) {
		ret = sprd_ptm_trace_enable(sdev);
		if (ret)
			return ret;
	} else if (sdev->mode_info.trace.trace_st && !enable) {
		sprd_ptm_trace_disable(sdev);
	}
	return strnlen(buf, count);
}

static ssize_t trace_show(struct device *dev,
			  struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	bool trace_en = sdev->mode_info.trace.trace_st;

	return sprintf(buf, "trace mode is %s\n",
		       trace_en ? "enable" : "disable");
}
static DEVICE_ATTR_RW(trace);

static ssize_t winlen_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 winlen;
	int ret;

	ret = kstrtouint(buf, 0, &winlen);
	if (ret)
		return ret;
	if (winlen)
		sdev->mode_info.trace.winlen = winlen;
	else
		sdev->mode_info.trace.winlen = BM_TRACE_DEF_WINLEN;
	return strnlen(buf, count);
}

static ssize_t winlen_show(struct device *dev,
			   struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 winlen = sdev->mode_info.trace.winlen;

	return sprintf(buf, "winlen is %d.\n",
		       winlen ? winlen : BM_TRACE_DEF_WINLEN);
}
static DEVICE_ATTR_RW(winlen);

static ssize_t cmd_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 cmd;
	int ret;

	ret = kstrtouint(buf, 0, &cmd);
	if (ret)
		return ret;
	if (cmd) {
		sdev->mode_info.trace.cmd_eb = true;
		ret = sprd_ptm_trace_enable(sdev);
		if (ret)
			return ret;
	} else {
		sprd_ptm_trace_disable(sdev);
		sdev->mode_info.trace.cmd_eb = false;
	}
	return strnlen(buf, count);
}

static ssize_t cmd_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	bool cmd_eb = sdev->mode_info.trace.cmd_eb;

	return sprintf(buf, "cmd trace is %s\n",
		       cmd_eb ? "enable" : "disable");
}
static DEVICE_ATTR_RW(cmd);

static ssize_t trace_out_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 out_mode;
	int ret;

	ret = kstrtouint(buf, 0, &out_mode);
	if (ret)
		return ret;
	sdev->mode_info.trace.out_mode = out_mode;
	return strnlen(buf, count);
}

static ssize_t trace_out_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	enum ptm_trace_out_mode out_mode = sdev->mode_info.trace.out_mode;

	return sprintf(buf, "trace out mode is %s\n",
		       out_mode ? "ETR OUT" : "TPIU OUT");
}

static DEVICE_ATTR_RW(trace_out);

static struct attribute *ptm_trace_attrs[] = {
	&dev_attr_trace.attr,
	&dev_attr_winlen.attr,
	&dev_attr_cmd.attr,
	&dev_attr_trace_out.attr,
	NULL,
};

static struct attribute_group ptm_trace_group = {
	.name = PTM_NAME,
	.attrs = ptm_trace_attrs,
};

static int sprd_ptm_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	struct sprd_ptm_dev *sdev;
	const struct ptm_pvt_para *ptm_ver_info;
	void __iomem *funnel_base, *tmc_base, *tpiu_base;
	struct regmap *regmap;
	int i, ret;
	u32 args[2];

	struct device_node *np = pdev->dev.of_node;
	ptm_ver_info = of_device_get_match_data(&pdev->dev);
	if (!ptm_ver_info)
		return -EINVAL;

	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	/* get number of ddr channel */
	if (of_property_read_u32_index(pdev->dev.of_node,
				       "sprd,ddr-chn",
				       0, &sdev->pub_chn)) {
		dev_err(&pdev->dev, "Error: Can't get chns number!\n");
		return -EINVAL;
	}

	/* inaccuracy DDR AXI channel number */
	if (!sdev->pub_chn)
		return -EINVAL;

	sdev->sprd_ptm_list = devm_kzalloc(&pdev->dev,
				sdev->pub_chn * sizeof(char *),
				GFP_KERNEL);
	if (!sdev->sprd_ptm_list)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sdev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sdev->base)) {
		dev_err(&pdev->dev, "Error: ptm get base address failed!\n");
		return PTR_ERR(sdev->base);
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	funnel_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(funnel_base)) {
		dev_err(&pdev->dev, "Error: ptm get funnel base addr failed\n");
		return PTR_ERR(funnel_base);
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	tmc_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(tmc_base)) {
		dev_err(&pdev->dev, "Error: ptm get tmc base addr failed\n");
		return PTR_ERR(tmc_base);
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	tpiu_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(tpiu_base)) {
		dev_err(&pdev->dev, "Error: ptm get tpiu base addr failed\n");
		return PTR_ERR(tpiu_base);
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	sdev->mode_info.trace.replicator_base = devm_ioremap(&pdev->dev,
							     res->start,
							     resource_size(res));
	if (IS_ERR_OR_NULL(sdev->mode_info.trace.replicator_base)) {
		dev_err(&pdev->dev, "Error: ptm get replicator base addr failed\n");
		return PTR_ERR(sdev->mode_info.trace.replicator_base);
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	sdev->mode_info.trace.etr_base = devm_ioremap(&pdev->dev,
						      res->start,
						      resource_size(res));
	if (IS_ERR_OR_NULL(sdev->mode_info.trace.etr_base)) {
		dev_err(&pdev->dev, "Error: ptm get soc etr base addr failed\n");
		return PTR_ERR(sdev->mode_info.trace.etr_base);
	}
	/* get name of each ddr channel */
	for (i = 0; i < sdev->pub_chn; i++) {
		if (of_property_read_string_index(pdev->dev.of_node,
			"sprd,chn-name", i, sdev->sprd_ptm_list + i)) {
			dev_err(&pdev->dev, "Error: Read Chn name\n");
			return -EINVAL;
		}
	}
	/* get funnel port */
	if (of_property_read_u32_index(pdev->dev.of_node,
				       "sprd,funnel-port",
				       0, &sdev->mode_info.trace.funnel_port)) {
		dev_err(&pdev->dev, "Error: Can't get port number!\n");
		return -EINVAL;
	}
	/* get coresight clock source */
	sdev->clk_cs = devm_clk_get(&pdev->dev, "clk_cs");
	if (IS_ERR(sdev->clk_cs)) {
		dev_err(&pdev->dev, "Error: Can not get the cs clock!\n");
		return PTR_ERR(sdev->clk_cs);
	}
	sdev->clk_cs_src = devm_clk_get(&pdev->dev, "cs_src");
	if (IS_ERR(sdev->clk_cs_src)) {
		dev_err(&pdev->dev, "Error: Can not get the cs src clock!\n");
		return PTR_ERR(sdev->clk_cs_src);
	}
	/* get pub clk */
	regmap = syscon_regmap_lookup_by_phandle_args(pdev->dev.of_node, "enable-syscon", 2, args);
	if (IS_ERR(regmap))
		dev_warn(&pdev->dev, "failed to get enable-syscon\n");
	else {
		regmap_update_bits(regmap, args[0], args[1], args[1]);
	}

	sdev->pvt_data = ptm_ver_info;
	sdev->grp_sel = ptm_ver_info->grp_sel;
	sdev->mode = INIT_MODE;
	sdev->mode_info.trace.funnel_base = funnel_base;
	sdev->mode_info.trace.tmc_base = tmc_base;
	sdev->mode_info.trace.tpiu_base = tpiu_base;
	sdev->mode_info.trace.winlen = BM_TRACE_DEF_WINLEN;
	sdev->mode_info.trace.out_mode = TPIU_MODE;
	sdev->chn_info.lty_mode = ADD_OS_IN_OS;
	sdev->misc.name = PTM_NAME;
	sdev->misc.parent = &pdev->dev;
	sdev->misc.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&sdev->misc);
	if (ret) {
		dev_err(&pdev->dev, "Error: Unable to register misc dev\n");
		return ret;
	}
	ret = sysfs_create_group(&sdev->misc.this_device->kobj,
		&ptm_origin_group);
	if (ret) {
		dev_err(&pdev->dev, "Error: Unable to export ptm sysfs\n");
		misc_deregister(&sdev->misc);
		return ret;
	}
	/* save the sdev as private data */
	of_get_ptm_memory_info(sdev, np);
	spin_lock_init(&sdev->slock);
	init_completion(&sdev->comp);
	platform_set_drvdata(pdev, sdev);
	sprd_ptm_init(&pdev->dev);

	return 0;
}

static int sprd_ptm_remove(struct platform_device *pdev)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(&pdev->dev);

	sprd_ptm_deinit(&pdev->dev);
	sprd_ptm_trace_deinit(&pdev->dev);
	sprd_ptm_legacy_deinit(&pdev->dev);
	sysfs_remove_group(pdev->dev.kobj.parent, &ptm_origin_group);
	misc_deregister(&sdev->misc);
	return 0;
}

static int sprd_ptm_suspend(struct device *dev)
{
	return 0;
}

static int sprd_ptm_resume(struct device *dev)
{
	struct sprd_ptm_dev *sdev = dev_get_drvdata(dev);
	u32 len = sdev->mode_info.legacy.timer_interval;
	bool trace_en = sdev->mode_info.trace.trace_st;

	if (sdev->mode == LEGACY_MODE && len) {
		sprd_ptm_set_winlen(sdev, PTM_REG_MAX);
		sprd_ptm_set_lty_mode(sdev);
		sprd_ptm_set_lty_enable(sdev);
	} else if (sdev->mode == TRACE_MODE && trace_en) {
		return sprd_ptm_trace_enable(sdev);
	}

	return 0;
}

static const struct dev_pm_ops sprd_ptm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_ptm_suspend, sprd_ptm_resume)
};

static struct platform_driver sprd_ptm_driver = {
	.probe    = sprd_ptm_probe,
	.remove   = sprd_ptm_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_ptm",
		.of_match_table = sprd_ptm_of_match,
		.pm = &sprd_ptm_pm_ops,
	},
};

module_platform_driver(sprd_ptm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobs Wang<bobs.wang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc platform ptm driver");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

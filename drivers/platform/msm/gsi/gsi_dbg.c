/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#ifdef CONFIG_DEBUG_FS

#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#include <linux/uaccess.h>
#include <linux/msm_gsi.h>
#include "gsi_reg.h"
#include "gsi.h"

#define TERR(fmt, args...) \
		pr_err("%s:%d " fmt, __func__, __LINE__, ## args)
#define TDBG(fmt, args...) \
		pr_debug("%s:%d " fmt, __func__, __LINE__, ## args)

static struct dentry *dent;
static struct dentry *dfile_gsi_ev_dump;
static struct dentry *dfile_gsi_ch_dump;
static struct dentry *dfile_gsi_ee_dump;
static struct dentry *dfile_gsi_map;
static struct dentry *dfile_gsi_stats;
static char dbg_buff[4096];

static ssize_t gsi_dump_evt(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	u32 arg1;
	u32 arg2;
	unsigned long missing;
	char *sptr, *token;
	uint32_t val;
	struct gsi_evt_ctx *ctx;
	uint16_t i;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';

	sptr = dbg_buff;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &arg1))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &arg2))
		return -EINVAL;

	TDBG("arg1=%u arg2=%u\n", arg1, arg2);

	if (arg1 >= GSI_MAX_EVT_RING) {
		TERR("invalid evt ring id %u\n", arg1);
		return -EFAULT;
	}

	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_0_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX0  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_1_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX1  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_2_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX2  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_3_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX3  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_4_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX4  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_5_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX5  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_6_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX6  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_7_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX7  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_8_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX8  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_9_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX9  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_10_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX10 0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_11_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX11 0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_12_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX12 0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_13_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d CTX13 0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_SCRATCH_0_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d SCR0  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_SCRATCH_1_OFFS(arg1, gsi_ctx->per.ee));
	TERR("EV%2d SCR1  0x%x\n", arg1, val);

	if (arg2) {
		ctx = &gsi_ctx->evtr[arg1];

		if (ctx->props.ring_base_vaddr) {
			for (i = 0; i < ctx->props.ring_len / 16; i++)
				TERR("EV%2d (0x%08llx) %08x %08x %08x %08x\n",
				arg1, ctx->props.ring_base_addr + i * 16,
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 0),
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 4),
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 8),
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 12));
		} else {
			TERR("No VA supplied for event ring id %u\n", arg1);
		}
	}

	return count;
}

static ssize_t gsi_dump_ch(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	u32 arg1;
	u32 arg2;
	unsigned long missing;
	char *sptr, *token;
	uint32_t val;
	struct gsi_chan_ctx *ctx;
	uint16_t i;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';

	sptr = dbg_buff;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &arg1))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &arg2))
		return -EINVAL;

	TDBG("arg1=%u arg2=%u\n", arg1, arg2);

	if (arg1 >= GSI_MAX_CHAN) {
		TERR("invalid chan id %u\n", arg1);
		return -EFAULT;
	}

	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX0  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_1_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX1  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_2_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX2  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_3_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX3  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX4  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_5_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX5  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_6_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX6  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_7_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d CTX7  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_RE_FETCH_READ_PTR_OFFS(arg1,
			gsi_ctx->per.ee));
	TERR("CH%2d REFRP 0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_RE_FETCH_WRITE_PTR_OFFS(arg1,
			gsi_ctx->per.ee));
	TERR("CH%2d REFWP 0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_QOS_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d QOS   0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d SCR0  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d SCR1  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d SCR2  0x%x\n", arg1, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(arg1, gsi_ctx->per.ee));
	TERR("CH%2d SCR3  0x%x\n", arg1, val);

	if (arg2) {
		ctx = &gsi_ctx->chan[arg1];

		if (ctx->props.ring_base_vaddr) {
			for (i = 0; i < ctx->props.ring_len / 16; i++)
				TERR("CH%2d (0x%08llx) %08x %08x %08x %08x\n",
				arg1, ctx->props.ring_base_addr + i * 16,
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 0),
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 4),
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 8),
				*(u32 *)((u8 *)ctx->props.ring_base_vaddr +
					i * 16 + 12));
		} else {
			TERR("No VA supplied for chan id %u\n", arg1);
		}
	}

	return count;
}

static ssize_t gsi_dump_ee(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	uint32_t val;

	val = gsi_readl(gsi_ctx->base +
		GSI_GSI_MANAGER_EE_QOS_n_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d QOS 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_STATUS_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d STATUS 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_HW_PARAM_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d HW_PARAM 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_SW_VERSION_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d SW_VERSION 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_MCS_CODE_VER_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d MCS_CODE_VER 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_TYPE_IRQ_MSK_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d TYPE_IRQ_MSK 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_MSK_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d CH_IRQ_MSK 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_MSK_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d EV_IRQ_MSK 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_IEOB_IRQ_MSK_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d IEOB_IRQ_MSK 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_GLOB_IRQ_EN_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d GLOB_IRQ_EN 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_GSI_IRQ_EN_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d GSI_IRQ_EN 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_INTSET_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d INTSET 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_MSI_BASE_LSB_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d MSI_BASE_LSB 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_MSI_BASE_MSB_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d MSI_BASE_MSB 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_INT_VEC_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d INT_VEC 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d SCR0 0x%x\n", gsi_ctx->per.ee, val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SCRATCH_1_OFFS(gsi_ctx->per.ee));
	TERR("EE%2d SCR1 0x%x\n", gsi_ctx->per.ee, val);

	return count;
}

static ssize_t gsi_dump_map(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct gsi_chan_ctx *ctx;
	uint32_t val1;
	uint32_t val2;
	int i;

	TERR("EVT bitmap 0x%lx\n", gsi_ctx->evt_bmap);
	for (i = 0; i < GSI_MAX_CHAN; i++) {
		ctx = &gsi_ctx->chan[i];

		if (ctx->allocated) {
			TERR("VIRT CH%2d -> VIRT EV%2d\n", ctx->props.ch_id,
				ctx->evtr ? ctx->evtr->id : GSI_NO_EVT_ERINDEX);
			val1 = gsi_readl(gsi_ctx->base +
				GSI_GSI_DEBUG_EE_n_CH_k_VP_TABLE_OFFS(i,
					gsi_ctx->per.ee));
			TERR("VIRT CH%2d -> PHYS CH%2d\n", ctx->props.ch_id,
				val1 &
				GSI_GSI_DEBUG_EE_n_CH_k_VP_TABLE_PHY_CH_BMSK);
			if (ctx->evtr) {
				val2 = gsi_readl(gsi_ctx->base +
				GSI_GSI_DEBUG_EE_n_EV_k_VP_TABLE_OFFS(
					ctx->evtr->id, gsi_ctx->per.ee));
				TERR("VRT EV%2d -> PHYS EV%2d\n", ctx->evtr->id,
				val2 &
				GSI_GSI_DEBUG_EE_n_CH_k_VP_TABLE_PHY_CH_BMSK);
			}
			TERR("\n");
		}
	}

	return count;
}

static ssize_t gsi_dump_stats(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct gsi_chan_ctx *ctx;
	int i;

	for (i = 0; i < GSI_MAX_CHAN; i++) {
		ctx = &gsi_ctx->chan[i];

		if (ctx->allocated) {
			TERR("CH%2d:\n", ctx->props.ch_id);
			TERR("queued=%lu compl=%lu\n",
				ctx->stats.queued,
				ctx->stats.completed);
			TERR("cb->poll=%lu poll->cb=%lu\n",
				ctx->stats.callback_to_poll,
				ctx->stats.poll_to_callback);
			TERR("invalid_tre_error=%lu\n",
				ctx->stats.invalid_tre_error);
			TERR("poll_ok=%lu poll_empty=%lu\n",
				ctx->stats.poll_ok, ctx->stats.poll_empty);
			if (ctx->evtr)
				TERR("compl_evt=%lu\n",
					ctx->evtr->stats.completed);
			TERR("\n");
		}
	}

	return count;
}

const struct file_operations gsi_ev_dump_ops = {
	.write = gsi_dump_evt,
};

const struct file_operations gsi_ch_dump_ops = {
	.write = gsi_dump_ch,
};

const struct file_operations gsi_ee_dump_ops = {
	.write = gsi_dump_ee,
};

const struct file_operations gsi_map_ops = {
	.write = gsi_dump_map,
};

const struct file_operations gsi_stats_ops = {
	.write = gsi_dump_stats,
};

void gsi_debugfs_init(void)
{
	const mode_t read_only_mode = S_IRUSR | S_IRGRP | S_IROTH;
	const mode_t write_only_mode = S_IWUSR | S_IWGRP;

	dent = debugfs_create_dir("gsi", 0);
	if (IS_ERR(dent)) {
		TERR("fail to create dir\n");
		return;
	}

	dfile_gsi_ev_dump = debugfs_create_file("ev_dump", write_only_mode,
			dent, 0, &gsi_ev_dump_ops);
	if (!dfile_gsi_ev_dump || IS_ERR(dfile_gsi_ev_dump)) {
		TERR("fail to create ev_dump file\n");
		goto fail;
	}

	dfile_gsi_ch_dump = debugfs_create_file("ch_dump", write_only_mode,
			dent, 0, &gsi_ch_dump_ops);
	if (!dfile_gsi_ch_dump || IS_ERR(dfile_gsi_ch_dump)) {
		TERR("fail to create ch_dump file\n");
		goto fail;
	}

	dfile_gsi_ee_dump = debugfs_create_file("ee_dump", read_only_mode, dent,
			0, &gsi_ee_dump_ops);
	if (!dfile_gsi_ee_dump || IS_ERR(dfile_gsi_ee_dump)) {
		TERR("fail to create ee_dump file\n");
		goto fail;
	}

	dfile_gsi_map = debugfs_create_file("map", read_only_mode, dent,
			0, &gsi_map_ops);
	if (!dfile_gsi_map || IS_ERR(dfile_gsi_map)) {
		TERR("fail to create map file\n");
		goto fail;
	}

	dfile_gsi_stats = debugfs_create_file("stats", read_only_mode, dent,
			0, &gsi_stats_ops);
	if (!dfile_gsi_stats || IS_ERR(dfile_gsi_stats)) {
		TERR("fail to create stats file\n");
		goto fail;
	}

	return;
fail:
	debugfs_remove_recursive(dent);
}
#else
void gsi_debugfs_init(void)
{
}
#endif

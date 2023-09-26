// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/module.h>

#include "cmdq-util.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

struct bw_monitor {
	struct device *dev;
	struct cmdq_client *clt;
	struct cmdq_pkt *pkt;
	struct {
		phys_addr_t base;
	} smi[2];
	u8 smi_cnt;
	u8 timer_gpr;
	u32 *result;
	dma_addr_t result_pa;
	bool enable;
};
static struct bw_monitor bw_mon;

static u32 bw_interval = 16000;
module_param(bw_interval, int, 0644);

static u32 bw_log;
module_param(bw_log, int, 0644);

enum mon_enable_command {
	MON_DISABLE_STOP = 0,
	MON_ENABLE_START = 1,
};

/* following def and func are platform dependent */
#define SMI_MON_AXI_ENA_MON0		0x1a0
#define SMI_MON_AXI_CLR_MON0		0x1a4
#define SMI_MON_AXI_TYPE_MON0		0x1ac
#define SMI_MON_AXI_CON_MON0		0x1b0

/* following are byte count for #1 #2 #3 #0 */
#define SMI_MON_AXI_ACT_CNT_MON0	0x1c0
#define SMI_MON_AXI_REQ_CNT_MON0	0x1c4
#define SMI_MON_AXI_BEA_CNT_MON0	0x1cc
#define SMI_MON_AXI_BYT_CNT_MON0	0x1d0

#define SMI_MON_EN			0x3
#define SMI_MON_CLR			0x1
#define SMI_MON_AXI_TYPE		0xa
#define SMI_MON_AXI_CON_0		0x55440
#define SMI_MON_AXI_CON_1		0x55440

#if IS_ENABLED(CMDQ_BWMON_SUPPORT)
static int cmdq_bwmon_enable_monitor(struct cmdq_pkt *pkt, u8 id)
{
	phys_addr_t base = bw_mon.smi[id].base;

#if IS_ENABLED(CONFIG_MACH_MT6885) || IS_ENABLED(CONFIG_MACH_MT6893)
	if (id == 0) {
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_CLR_MON0,
			SMI_MON_CLR, U32_MAX);
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_TYPE_MON0,
			SMI_MON_AXI_TYPE, U32_MAX);
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_CON_MON0,
			SMI_MON_AXI_CON_0, U32_MAX);
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_ENA_MON0,
			SMI_MON_EN, U32_MAX);
		return 0;
	} else if (id == 1) {
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_CLR_MON0,
			SMI_MON_CLR, U32_MAX);
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_TYPE_MON0,
			SMI_MON_AXI_TYPE, U32_MAX);
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_CON_MON0,
			SMI_MON_AXI_CON_1, U32_MAX);
		cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_ENA_MON0,
			SMI_MON_EN, U32_MAX);
		return 0;
	}
#else
	cmdq_msg("[bwmon] not support for base:%lx", (unsigned long)base);
#endif
	return -EINVAL;
}

static void cmdq_bwmon_read(struct cmdq_pkt *pkt, u8 id, dma_addr_t pa)
{
	phys_addr_t base = bw_mon.smi[id].base;
	const u8 reg_bw = CMDQ_THR_SPR_IDX3;

	cmdq_pkt_write_value_addr(pkt, base + SMI_MON_AXI_ENA_MON0,
		0, U32_MAX);
	cmdq_pkt_mem_move(pkt, NULL, base + SMI_MON_AXI_BYT_CNT_MON0,
		pa, reg_bw);
	cmdq_pkt_mem_move(pkt, NULL, base + SMI_MON_AXI_ACT_CNT_MON0,
		pa + 4, reg_bw);
	cmdq_pkt_mem_move(pkt, NULL, base + SMI_MON_AXI_REQ_CNT_MON0,
		pa + 8, reg_bw);
	cmdq_pkt_mem_move(pkt, NULL, base + SMI_MON_AXI_BEA_CNT_MON0,
		pa + 12, reg_bw);
}

static void cmdq_bwmon_clk_enable(void)
{
#if IS_ENABLED(CONFIG_MACH_MT6885) || IS_ENABLED(CONFIG_MACH_MT6893)
	smi_bus_prepare_enable(SMI_LARB4, "BWMon");
	smi_bus_prepare_enable(SMI_LARB5, "BWMon");
	smi_bus_prepare_enable(SMI_LARB7, "BWMon");
	smi_bus_prepare_enable(SMI_LARB8, "BWMon");
#else
	cmdq_err("[bwmon] not support");
#endif
}

static void cmdq_bwmon_clk_disable(void)
{
#if IS_ENABLED(CONFIG_MACH_MT6885) || IS_ENABLED(CONFIG_MACH_MT6893)
	smi_bus_disable_unprepare(SMI_LARB4, "BWMon");
	smi_bus_disable_unprepare(SMI_LARB5, "BWMon");
	smi_bus_disable_unprepare(SMI_LARB7, "BWMon");
	smi_bus_disable_unprepare(SMI_LARB8, "BWMon");
#else
	cmdq_err("[bwmon] not support");
#endif
}

static void cmdq_bwmon_print(u64 total, int err)
{
#if IS_ENABLED(CONFIG_MACH_MT6885) || IS_ENABLED(CONFIG_MACH_MT6893)
	u32 *buf = bw_mon.result;

	if (bw_log)
		cmdq_msg(
			"[bwmon] larb5 r:%u w:%u larb7 r:%u w:%u larb4 r:%u w:%u larb8 r:%u w:%u total bw:%llu status:%d",
			*buf, *(buf + 1), *(buf + 2), *(buf + 3),
			*(buf + 4), *(buf + 5), *(buf + 6), *(buf + 7),
			total, err);
	cmdq_trace_c("0|SMI_Codec_Bandwidth|%llu\n", total);
#else
	cmdq_msg("[bwmon] status:%d bwlog:%d", err, bw_log);
#endif
}

/* following funcs are common code */
static struct cmdq_pkt *cmdq_bwmon_build_pkt(void)
{
	const u8 reg_bw = CMDQ_THR_SPR_IDX3;
	struct cmdq_pkt *pkt = bw_mon.pkt;
	u8 i;
	u32 loop_anchor;
	int ret;

	/* reuse exist one */
	if (pkt)
		return pkt;

	if (!bw_mon.result) {
		bw_mon.result = (u32 *)cmdq_mbox_buf_alloc(bw_mon.dev,
			&bw_mon.result_pa);
		if (!bw_mon.result)
			return ERR_PTR(-ENOMEM);
	}

	pkt = cmdq_pkt_create(bw_mon.clt);
	if (!pkt || IS_ERR(pkt)) {
		cmdq_err("[bwmon] pkt create fail");
		return pkt;
	}

	bw_mon.pkt = pkt;

	/* write smi regs to enable bw monitor */
	cmdq_pkt_assign_command(pkt, reg_bw, 0);
	for (i = 0; i < bw_mon.smi_cnt; i++) {
		ret = cmdq_bwmon_enable_monitor(pkt, i);
		if (ret < 0)
			return ERR_PTR(ret);
	}

	loop_anchor = pkt->cmd_buf_size;
	cmdq_pkt_sleep(pkt, CMDQ_US_TO_TICK(bw_interval), bw_mon.timer_gpr);

	/* read smi register back to result buffer and clear */
	for (i = 0; i < bw_mon.smi_cnt; i++)
		cmdq_bwmon_read(pkt, i, bw_mon.result_pa + i * 4 * 4);

	cmdq_pkt_finalize_loop(pkt);

	return pkt;
}

static void cmdq_bwmon_cb(struct cmdq_cb_data data)
{
	u8 idx_smi, idx_larb;
	u64 total = 0;
	u32 *buf = bw_mon.result;
	static u64 last_bw;

	for (idx_smi = 0; idx_smi < bw_mon.smi_cnt; idx_smi++)
		for (idx_larb = 0; idx_larb < 4; idx_larb++) {
			total += *buf;
			buf++;
		}

	if (last_bw != total) {
		cmdq_bwmon_print(total, data.err);
		last_bw = total;
	}
}

static int cmdq_bwmon_start(void)
{
	struct cmdq_pkt *pkt;
	u8 idx_smi, idx_larb;
	u32 *buf;

	if (!bw_mon.clt) {
		cmdq_err("[bwmon] no client to start");
		return -EINVAL;
	}

	pkt = cmdq_bwmon_build_pkt();
	if (!pkt || IS_ERR(pkt))
		return PTR_ERR(pkt);

	buf = bw_mon.result;
	for (idx_smi = 0; idx_smi < bw_mon.smi_cnt; idx_smi++)
		for (idx_larb = 0; idx_larb < 4; idx_larb++) {
			*buf = 0;
			buf++;
		}

	cmdq_bwmon_clk_enable();
	cmdq_pkt_flush_async(pkt, cmdq_bwmon_cb, 0);
	bw_mon.enable = true;

	return 0;
}

static int cmdq_bwmon_stop(void)
{
	bw_mon.enable = false;

	if (!bw_mon.enable) {
		cmdq_msg("[bwmon] monitor not enable");
		return 0;
	}

	if (!bw_mon.pkt || !bw_mon.clt) {
		cmdq_err("[bwmon] monitor enable but empty pkt/client %p/%p",
			bw_mon.pkt, bw_mon.clt);
		return 0;
	}

	/* stop loop in thread */
	cmdq_mbox_stop(bw_mon.clt);

	/* still wait to make sure it is done */
	cmdq_pkt_wait_complete(bw_mon.pkt);

	cmdq_bwmon_clk_disable();

	return 0;
}
#endif

static int cmdq_bwmon_set(const char *val, const struct kernel_param *kp)
{
#if IS_ENABLED(CMDQ_BWMON_SUPPORT)
	int result, enable;

	cmdq_msg("%s [bwmon] in", __func__);
	result = kstrtoint(val, 0, &enable);
	if (result) {
		cmdq_err("[bwmon] monitor enable failed:%d", result);
		return result;
	}

	if (enable == MON_ENABLE_START) {
		result = cmdq_bwmon_start();
		if (result)
			bw_mon.enable = false;
		return result;
	}

	return cmdq_bwmon_stop();
#else
	cmdq_msg("%s [bwmon] not support", __func__);
	return 0;
#endif
}

static struct kernel_param_ops bw_monitor_ops = {.set = cmdq_bwmon_set};
module_param_cb(bw_monitor, &bw_monitor_ops, NULL, 0644);

static int cmdq_bwmon_parse_larb(struct platform_device *pdev, u8 idx)
{
	struct device_node *node;
	struct platform_device *node_pdev;
	struct resource res;
	int ret;

	node = of_parse_phandle(pdev->dev.of_node, "smi_mon", idx);
	if (!node) {
		cmdq_msg("[bwmon] of_parse_phandle smi_mon stop at idx:%hhu",
			idx);
		return -EINVAL;
	}

	node_pdev = of_find_device_by_node(node);
	of_node_put(node);
	if (!node_pdev) {
		cmdq_err("[bwmon] of_find_device_by_node to smi_mon failed");
		return -EINVAL;
	}

	ret = of_address_to_resource(node_pdev->dev.of_node, 0, &res);
	if (ret) {
		cmdq_err(
			"[bwmon] of_address_to_resource to mmsys_config failed ret:%d",
			ret);
		return ret;
	}
	bw_mon.smi[idx].base = res.start;

	cmdq_msg("[bwmon] smi%hhu:%#lx",
		idx, (unsigned long)bw_mon.smi[idx].base);

	return 0;
}

static int cmdq_bwmon_probe(struct platform_device *pdev)
{
	u8 i;

	cmdq_msg("%s", __func__);

	bw_mon.dev = &pdev->dev;
	bw_mon.clt = cmdq_mbox_create(&pdev->dev, 0);
	if (!bw_mon.clt || IS_ERR(bw_mon.clt)) {
		cmdq_msg("[bwmon] create bw mon client fail:%ld",
			PTR_ERR(bw_mon.clt));
		bw_mon.clt = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(bw_mon.smi); i++)
		if (cmdq_bwmon_parse_larb(pdev, i))
			break;
	bw_mon.smi_cnt = i;

	of_property_read_u8(pdev->dev.of_node, "bw_mon_gpr",
		&bw_mon.timer_gpr);

	cmdq_msg("%s end", __func__);

	return 0;
}

static int cmdq_bwmon_remove(struct platform_device *pdev)
{
	cmdq_mbox_destroy(bw_mon.clt);
	cmdq_mbox_buf_free(bw_mon.dev, bw_mon.result, bw_mon.result_pa);
	return 0;
}

static const struct of_device_id cmdq_bwmon_of_ids[] = {
	{
		.compatible = "mediatek,cmdq-bw-mon",
	},
	{}
};
MODULE_DEVICE_TABLE(of, cmdq_bwmon_of_ids);

static struct platform_driver cmdq_bwmon_drv = {
	.probe = cmdq_bwmon_probe,
	.remove = cmdq_bwmon_remove,
	.driver = {
		.name = "cmdq-bw-mon",
		.of_match_table = cmdq_bwmon_of_ids,
	},
};
module_platform_driver(cmdq_bwmon_drv);

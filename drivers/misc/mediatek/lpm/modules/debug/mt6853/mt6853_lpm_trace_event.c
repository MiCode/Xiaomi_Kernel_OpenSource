// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <lpm_trace_event/lpm_trace_event.h>
#include <mtk_spm_sysfs.h>
#include <mt6853_spm_reg.h>


#define plat_mmio_read(offset)	__raw_readl(spm_base + offset)

void __iomem *spm_base;
struct timer_list spm_resource_req_timer;
u32 spm_resource_req_timer_is_enabled;
u32 spm_resource_req_timer_ms;

static void spm_resource_req_timer_fn(struct timer_list *data)
{
	u32 req_sta_0, req_sta_1, req_sta_4;
	u32 src_req;
	u32 md = 0, conn = 0, scp = 0, adsp = 0;
	u32 ufs = 0, msdc = 0, disp = 0, apu = 0;
	u32 spm = 0;

	if (!spm_base)
		return;

	req_sta_0 = plat_mmio_read(SRC_REQ_STA_0);
	if (req_sta_0 & 0xFFF)
		md = 1;
	if (req_sta_0 & (0x3F << 12))
		conn = 1;
	if (req_sta_0 & (0xF << 26))
		disp = 1;

	req_sta_1 = plat_mmio_read(SRC_REQ_STA_1);
	if (req_sta_1 & 0x1F)
		scp = 1;
	if (req_sta_1 & (0x1F << 5))
		adsp = 1;
	if (req_sta_1 & (0x1F << 10))
		ufs = 1;
	if (req_sta_1 & (0x3FF << 21))
		msdc = 1;

	req_sta_4 = plat_mmio_read(SRC_REQ_STA_4);
	if (req_sta_4 & 0x1F)
		apu = 1;

	src_req = plat_mmio_read(SPM_SRC_REQ);
	if (src_req & 0x19B)
		spm = 1;

	trace_SPM__resource_req_0(
		md, conn,
		scp, adsp,
		ufs, msdc,
		disp, apu,
		spm);

	spm_resource_req_timer.expires = jiffies +
		msecs_to_jiffies(spm_resource_req_timer_ms);
	add_timer(&spm_resource_req_timer);
}

static void spm_resource_req_timer_en(u32 enable, u32 timer_ms)
{
	if (enable) {
		if (spm_resource_req_timer_is_enabled)
			return;

		timer_setup(&spm_resource_req_timer,
			spm_resource_req_timer_fn, 0);

		spm_resource_req_timer_ms = timer_ms;
		spm_resource_req_timer.expires = jiffies +
			msecs_to_jiffies(spm_resource_req_timer_ms);
		add_timer(&spm_resource_req_timer);

		spm_resource_req_timer_is_enabled = true;
	} else if (spm_resource_req_timer_is_enabled) {
		del_timer(&spm_resource_req_timer);
		spm_resource_req_timer_is_enabled = false;
	}
}

ssize_t get_spm_resource_req_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz
				, "spm resource request timer is enabled: %d\n",
				spm_resource_req_timer_is_enabled);
	return (bLen > sz) ? sz : bLen;
}

ssize_t set_spm_resource_req_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	u32 is_enable;
	u32 timer_ms;

	if (!ToUserBuf)
		return -EINVAL;

	if (sscanf(ToUserBuf, "%d %d", &is_enable, &timer_ms) == 2) {
		spm_resource_req_timer_en(is_enable, timer_ms);
		return sz;
	}

	if (kstrtouint(ToUserBuf, 10, &is_enable) == 0) {
		if (is_enable == 0) {
			spm_resource_req_timer_en(is_enable, 0);
			return sz;
		}
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op spm_resource_req_timer_enable_fops = {
	.fs_read = get_spm_resource_req_timer_enable,
	.fs_write = set_spm_resource_req_timer_enable,
};

int __init mt6853_lpm_trace_init(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node) {
		spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	mtk_spm_sysfs_root_entry_create();
	mtk_spm_sysfs_entry_node_add("spm_dump_res_req_enable", 0444
			, &spm_resource_req_timer_enable_fops, NULL);

	return 0;
}

void __exit mt6853_lpm_trace_deinit(void)
{
}


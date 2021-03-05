// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <mtk_lpm_trace_event/mtk_lpm_trace_event.h>
#include <mtk_spm_sysfs.h>
#include <mt6877_spm_reg.h>


#define plat_mmio_read(offset)	__raw_readl(spm_base + offset)

void __iomem *spm_base;
struct timer_list spm_resource_req_timer;
u32 spm_resource_req_timer_is_enabled;
u32 spm_resource_req_timer_ms;

static void spm_resource_req_timer_fn(unsigned long data)
{
	u32 req_sta_0, req_sta_2, req_sta_3, req_sta_4, req_sta_5;
	u32 md = 0, conn = 0, scp = 0, adsp = 0;
	u32 ufs = 0, msdc = 0, disp = 0, apu = 0;
	u32 spm = 0;

	req_sta_0 = plat_mmio_read(SPM_REQ_STA_0);
	req_sta_2 = plat_mmio_read(SPM_REQ_STA_2);
	req_sta_3 = plat_mmio_read(SPM_REQ_STA_3);
	req_sta_4 = plat_mmio_read(SPM_REQ_STA_4);
	req_sta_5 = plat_mmio_read(SPM_REQ_STA_5);

	if (req_sta_0 & (0x1F << 10))
		adsp = 1;
	if (req_sta_0 & (0x1F << 5))
		apu = 1;
	if (req_sta_2 & (0x3F << 5))
		conn = 1;
	if (req_sta_2 & (0xF << 11))
		disp = 1;
	if ((req_sta_3 & (0x1 << 31)) || (req_sta_4 & 0x1F))
		md = 1;
	if (req_sta_4 & (0x7FFF << 8))
		msdc = 1;
	if ((req_sta_4 & (0xF << 28)) || req_sta_5 & 0x1)
		scp = 1;
	if (req_sta_5 & (0x3 << 18))
		ufs = 1;
	if (req_sta_5 & (0x1F << 1))
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
		/* if spm resource request timer doesn't init */
		if (spm_resource_req_timer.function == NULL) {
			init_timer(&spm_resource_req_timer);
			spm_resource_req_timer.function =
				spm_resource_req_timer_fn;
			spm_resource_req_timer.data = 0;
			spm_resource_req_timer_is_enabled = false;
		}

		if (spm_resource_req_timer_is_enabled)
			return;

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

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op spm_resource_req_timer_enable_fops = {
	.fs_read = get_spm_resource_req_timer_enable,
	.fs_write = set_spm_resource_req_timer_enable,
};

bool spm_is_md1_sleep(void)
{
	return !((plat_mmio_read(SPM_REQ_STA_4) & 0x1F) ||
		 (plat_mmio_read(SPM_REQ_STA_3) & (0x1 << 31)));
}
EXPORT_SYMBOL(spm_is_md1_sleep);

int __init mt6877_lpm_trace_init(void)
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
late_initcall_sync(mt6877_lpm_trace_init);


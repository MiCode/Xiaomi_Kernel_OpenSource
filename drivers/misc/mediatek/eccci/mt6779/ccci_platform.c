// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_config.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"

#ifdef FEATURE_USING_4G_MEMORY_API
#include <mt-plat/mtk_lpae.h>
#endif

#define TAG "plat"

int Is_MD_EMI_voilation(void)
{
	return 1;
}

void __iomem *infra_ao_base;

/*
 * when MD attached its codeviser for debuging, this bit will be set.
 * so CCCI should disable some checkings and
 * operations as MD may not respond to us.
 */
unsigned int ccci_get_md_debug_mode(struct ccci_modem *md)
{
	return 0;
}
EXPORT_SYMBOL(ccci_get_md_debug_mode);

void ccci_get_platform_version(char *ver)
{
#ifdef ENABLE_CHIP_VER_CHECK
	sprintf(ver, "MT%04x_S%02x",
		get_chip_hw_ver_code(), (get_chip_hw_subcode() & 0xFF));
#else
	sprintf(ver, "MT6735_S00");
#endif
}

#ifdef FEATURE_LOW_BATTERY_SUPPORT
static int ccci_md_low_power_notify(
		struct ccci_modem *md, LOW_POEWR_NOTIFY_TYPE type, int level)
{
	unsigned int reserve = 0xFFFFFFFF;
	int ret = 0;

	CCCI_NORMAL_LOG(md->index, TAG,
		"low power notification type=%d, level=%d\n", type, level);
	/*
	 * byte3 byte2 byte1 byte0
	 *    0   4G   3G   2G
	 */
	switch (type) {
	case LOW_BATTERY:
		if (level == LOW_BATTERY_LEVEL_0)
			reserve = 0;	/* 0 */
		else if (level == LOW_BATTERY_LEVEL_1
						|| level == LOW_BATTERY_LEVEL_2)
			reserve = (1 << 6);	/* 64 */
		ret = port_proxy_send_msg_to_md(md->port_proxy,
			CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if (ret)
			CCCI_ERROR_LOG(md->index, TAG,
			 "send low battery notification fail, ret=%d\n", ret);
		break;
	case BATTERY_PERCENT:
		if (level == BATTERY_PERCENT_LEVEL_0)
			reserve = 0;	/* 0 */
		else if (level == BATTERY_PERCENT_LEVEL_1)
			reserve = (1 << 6);	/* 64 */
		ret = port_proxy_send_msg_to_md(md->port_proxy,
			CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if (ret)
			CCCI_ERROR_LOG(md->index, TAG,
			"send battery percent info fail, ret=%d\n", ret);
		break;
	default:
		break;
	};

	return ret;
}

static void ccci_md_low_battery_cb(LOW_BATTERY_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, LOW_BATTERY, level);
	}
}

static void ccci_md_battery_percent_cb(BATTERY_PERCENT_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, BATTERY_PERCENT, level);
	}
}
#endif

#define PCCIF_BUSY (0x4)
#define PCCIF_TCHNUM (0xC)
#define PCCIF_ACK (0x14)
#define PCCIF_CHDATA (0x100)
#define PCCIF_SRAM_SIZE (512)

void ccci_reset_ccif_hw(unsigned char md_id,
			int ccif_id, void __iomem *baseA, void __iomem *baseB)
{
	int i;
	struct ccci_smem_region *region;

	{
		int reset_bit = -1;

		switch (ccif_id) {
		case AP_MD1_CCIF:
			reset_bit = 8;
			break;
		}

		if (reset_bit == -1)
			return;

		/*
		 *this reset bit will clear
		 *CCIF's busy/wch/irq, but not SRAM
		 */
		/*set reset bit*/
		ccci_write32(infra_ao_base, 0x150, 1 << reset_bit);
		/*clear reset bit*/
		ccci_write32(infra_ao_base, 0x154, 1 << reset_bit);
	}

	/* clear SRAM */
	for (i = 0; i < PCCIF_SRAM_SIZE/sizeof(unsigned int); i++) {
		ccif_write32(baseA, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
		ccif_write32(baseB, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
	}

	/* extend from 36bytes to 72bytes in CCIF SRAM */
	/* 0~60bytes for bootup trace,
	 *last 12bytes for magic pattern,smem address and size
	 */
	region = ccci_md_get_smem_by_user_id(md_id,
		SMEM_USER_RAW_MDSS_DBG);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 3 * sizeof(u32),
		0x7274626E);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 2 * sizeof(u32),
		region->base_md_view_phy);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - sizeof(u32),
		region->size);
}

int ccci_platform_init(struct ccci_modem *md)
{
	struct device_node *node;
	/* Get infra cfg ao base */
	node = of_find_compatible_node(NULL, NULL,
					"mediatek,mt6779-infracfg_ao");
	infra_ao_base = of_iomap(node, 0);
	CCCI_INIT_LOG(-1, TAG, "infra_ao_base:0x%p\n", (void *)infra_ao_base);
#ifdef FEATURE_LOW_BATTERY_SUPPORT
	register_low_battery_notify(
		&ccci_md_low_battery_cb, LOW_BATTERY_PRIO_MD);
	register_battery_percent_notify(
		&ccci_md_battery_percent_cb, BATTERY_PERCENT_PRIO_MD);
#endif
	return 0;
}


/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mtk_spm_sleep.h>
#include <mach/mtk_pbm.h>

#include "ccci_config.h"
#include "ccci_modem.h"
#include "ccci_platform.h"
#include "ccif_c2k_platform.h"
#include "hif/ccci_hif_ccif.h"
#include "modem_sys.h"
#include "modem_reg_base.h"

#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_boot.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#define TAG "cif"

#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
static struct clk *clk_scp_sys_md2_main;
static struct clk *clk_scp_sys_md3_main;
#else
#include <mach/mt_clkmgr.h>
#endif

#define PCCIF_BUSY (0x4)
#define PCCIF_TCHNUM (0xC)
#define PCCIF_ACK (0x14)
#define PCCIF_CHDATA (0x100)
#define PCCIF_SRAM_SIZE (512)

static unsigned long apmixed_base;
static unsigned long apmcucfg_base;
static unsigned long apinfra_base;

struct c2k_pll_t c2k_pll_reg;

int md_ccif_get_modem_hw_info(struct platform_device *dev_ptr, struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;

	memset(dev_cfg, 0, sizeof(struct ccci_dev_cfg));
	memset(hw_info, 0, sizeof(struct md_hw_info));

#ifdef CONFIG_OF
	if (dev_ptr->dev.of_node == NULL) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG, "modem OF node NULL\n");
		return -1;
	}

	of_property_read_u32(dev_ptr->dev.of_node, "cell-index", &dev_cfg->index);
	CCCI_NORMAL_LOG(dev_cfg->index, TAG, "modem hw info get idx:%d\n", dev_cfg->index);
	if (!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG, "modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}
#else
	struct ccci_dev_cfg *dev_cfg_ptr = (struct ccci_dev_cfg *)dev->dev.platform_data;

	dev_cfg->index = dev_cfg_ptr->index;

	CCCI_NORMAL_LOG(dev_cfg->index, TAG, "modem hw info get idx:%d\n", dev_cfg->index);
	if (!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG, "modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}
#endif

	switch (dev_cfg->index) {
	case 2:		/*MD_SYS3 */
#ifdef CONFIG_OF
		of_property_read_u32(dev_ptr->dev.of_node, "ccif,major", &dev_cfg->major);
		of_property_read_u32(dev_ptr->dev.of_node, "ccif,minor_base", &dev_cfg->minor_base);
		of_property_read_u32(dev_ptr->dev.of_node, "ccif,capability", &dev_cfg->capability);

		hw_info->ap_ccif_base = (unsigned long)of_iomap(dev_ptr->dev.of_node, 0);
		/*hw_info->md_ccif_base = hw_info->ap_ccif_base+0x1000; */
		node = of_find_compatible_node(NULL, NULL, "mediatek,md_ccif1");
		hw_info->md_ccif_base = (unsigned long)of_iomap(node, 0);

		hw_info->ap_ccif_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
		hw_info->md_wdt_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 1);

		/*Device tree using none flag to register irq, sensitivity has set at "irq_of_parse_and_map" */
		hw_info->ap_ccif_irq_flags = IRQF_TRIGGER_NONE;
		hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;

		hw_info->md1_pccif_base = (unsigned long)of_iomap(dev_ptr->dev.of_node, 1);
		hw_info->md3_pccif_base = (unsigned long)of_iomap(dev_ptr->dev.of_node, 2);

		node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
		hw_info->infra_ao_base = of_iomap(node, 0);

		node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
		hw_info->sleep_base = of_iomap(node, 0);

		node = of_find_compatible_node(NULL, NULL, "mediatek,mt6799-toprgu");
		hw_info->toprgu_base = of_iomap(node, 0);

		node = of_find_compatible_node(NULL, NULL, "mediatek,mt6799-apmixedsys");
		apmixed_base = (unsigned long)of_iomap(node, 0);
		node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
		apmcucfg_base = (unsigned long)of_iomap(node, 0);
		node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg");
		apinfra_base = (unsigned long)of_iomap(node, 0);

		CCCI_NORMAL_LOG(dev_cfg->index, TAG,
				"infra_ao_base=0x%p, sleep_base=0x%p, toprgu_base=0x%p\n",
				hw_info->infra_ao_base, hw_info->sleep_base, hw_info->toprgu_base);

#endif

		hw_info->sram_size = CCIF_SRAM_SIZE;
		hw_info->md_rgu_base = MD3_RGU_BASE;

#if !defined(CONFIG_MTK_CLKMGR)
		clk_scp_sys_md3_main = devm_clk_get(&dev_ptr->dev, "scp-sys-c2k-main");
		if (IS_ERR(clk_scp_sys_md3_main)) {
			CCCI_ERROR_LOG(dev_cfg->index, TAG,
				     "modem %d get scp-sys-c2k-main failed\n", dev_cfg->index + 1);
			return -1;
		}
#endif

		/*no boot slave for md3 */

		break;
	default:
		return -1;
	}

	if (hw_info->ap_ccif_base == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG, "ap_ccif_base:0x%p\n", (void *)hw_info->ap_ccif_base);
		return -1;
	}
	if (hw_info->ap_ccif_irq_id == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG, "ccif_irq_id:%d\n", hw_info->ap_ccif_irq_id);
		return -1;
	}
	if (hw_info->md_wdt_irq_id == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG, "md_wdt_irq_id:%d\n", hw_info->md_wdt_irq_id);
		return -1;
	}

	CCCI_INIT_LOG(dev_cfg->index, TAG,
		"ap_ccif node info: major:%d, minor:%d, capability=%d, ap_ccif_base=0x%p, ccif_irq=%d, md_wdt_irq=%d\n",
		dev_cfg->major, dev_cfg->minor_base, dev_cfg->capability,
		(void *)hw_info->ap_ccif_base, hw_info->ap_ccif_irq_id, hw_info->md_wdt_irq_id);
	return 0;
}

int md_ccif_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_sys3_info *md_info = (struct md_sys3_info *)md->private_data;

	switch (md->index) {
	case MD_SYS3:
		c2k_pll_reg.c2k_pll_con3 = ioremap_nocache(C2KSYS_BASE + C2K_C2K_PLL_CON3, 0x4);
		c2k_pll_reg.c2k_pll_con2 = ioremap_nocache(C2KSYS_BASE + C2K_C2K_PLL_CON2, 0x4);
		c2k_pll_reg.c2k_plltd_con0 = ioremap_nocache(C2KSYS_BASE + C2K_C2K_PLLTD_CON0, 0x4);
		c2k_pll_reg.c2k_cppll_con0 = ioremap_nocache(C2KSYS_BASE + C2K_C2K_CPPLL_CON0, 0x4);
		c2k_pll_reg.c2k_dsppll_con0 = ioremap_nocache(C2KSYS_BASE + C2K_C2K_DSPPLL_CON0, 0x4);
		c2k_pll_reg.c2k_c2kpll1_con0 = ioremap_nocache(C2KSYS_BASE + C2K_C2K_C2KPLL1_CON0, 0x4);
		c2k_pll_reg.c2k_cg_amba_clksel = ioremap_nocache(C2KSYS_BASE + C2K_CG_ARM_AMBA_CLKSEL, 0x4);
		c2k_pll_reg.c2k_clk_ctrl4 = ioremap_nocache(C2KSYS_BASE + C2K_CLK_CTRL4, 0x4);
		c2k_pll_reg.c2k_clk_ctrl9 = ioremap_nocache(C2KSYS_BASE + C2K_CLK_CTRL9, 0x4);
		/*CCIRQ reg */
		md_info->ccirq_base[0] = ioremap_nocache(MD1_C2K_CCIRQ_BASE, 0x100);
		md_info->ccirq_base[1] = ioremap_nocache(C2K_MD1_CCIRQ_BASE, 0x100);
		md_info->c2k_cgbr1_addr = ioremap_nocache(C2KSYS_BASE + C2K_CGBR1, 0x4);
		md_info->md_rgu_base = ioremap_nocache(md->hw_info->md_rgu_base, 0x40);

		break;
	}
	return 0;
}

static int config_c2k_pll(void)
{
	ccif_write16(c2k_pll_reg.c2k_pll_con3, 0, 0x8805);
	ccif_write16(c2k_pll_reg.c2k_pll_con3, 0, 0x0005);
	ccif_write16(c2k_pll_reg.c2k_pll_con3, 0, 0x0001);
	ccif_write16(c2k_pll_reg.c2k_pll_con2, 0, 0x0);
	ccif_write16(c2k_pll_reg.c2k_plltd_con0, 0, 0x0010);

	ccif_write16(c2k_pll_reg.c2k_cppll_con0, 0, ccif_read16(c2k_pll_reg.c2k_cppll_con0, 0) | (0x1 << 15));

	udelay(30);

	ccif_write16(c2k_pll_reg.c2k_cg_amba_clksel, 0, 0xC124);
	ccif_write16(c2k_pll_reg.c2k_clk_ctrl4, 0, 0x8E43);
	ccif_write16(c2k_pll_reg.c2k_clk_ctrl9, 0, 0xA207);


	return 0;
}

static int reset_ccirq_hardware(struct ccci_modem *md)
{
	int i = 0;
	struct md_sys3_info *md_info = (struct md_sys3_info *)md->private_data;

	CCCI_NORMAL_LOG(MD_SYS3, TAG, "reset_ccirq_hardware start\n");
	for (i = 0; i < 2; i++) {
		/* config MD1_C2K/C2K_MD1 CC_IRQ_CLEAR_IRQ and CC_IRQ_CLEAR_AUTH_EXEC register to reset CC IRQ */
		ccif_write32(md_info->ccirq_base[i], 0x4, 0xA0000FFF);
		ccif_write32(md_info->ccirq_base[i], 0xC, 0xA0000FFF);

		/* clear CC IRQ dummy register */
		ccif_write32(md_info->ccirq_base[i], 0x40, 0x0);
		ccif_write32(md_info->ccirq_base[i], 0x44, 0x0);
		ccif_write32(md_info->ccirq_base[i], 0x48, 0x0);
		ccif_write32(md_info->ccirq_base[i], 0x4C, 0x0);
	}

	CCCI_NORMAL_LOG(md->index, TAG, "reset_ccirq_hardware end\n");
	return 0;
}

/*need modify according to dummy ap*/
int md_ccif_let_md_go(struct ccci_modem *md)
{
	struct md_sys3_info *md_info = (struct md_sys3_info *)md->private_data;
	struct md_hw_info *hw_info = md->hw_info;
	unsigned int reg_value;

	if (MD_IN_DEBUG(md)) {
		CCCI_NORMAL_LOG(md->index, TAG, "DBG_FLAG_JTAG is set\n");
		return -1;
	}
	CCCI_BOOTUP_LOG(md->index, TAG, "md_ccif_let_md_go\n");
	switch (md->index) {
	case MD_SYS3:
		/*check if meta mode */
		if (is_meta_mode() || get_boot_mode() == FACTORY_BOOT) {
			ccif_write32(hw_info->infra_ao_base,
				C2K_CONFIG, (ccif_read32(hw_info->infra_ao_base, C2K_CONFIG) | ETS_SEL_BIT));
		}

		/*dump power status for debugging*/
		CCCI_BOOTUP_LOG(md->index, TAG, "[C2K] AP_PWR_STATUS = 0x%x\n",
			     ccif_read32(hw_info->sleep_base, AP_PWR_STATUS));
		CCCI_BOOTUP_LOG(md->index, TAG, "[C2K] AP_PWR_STATUS_2ND = 0x%x\n",
			     ccif_read32(hw_info->sleep_base, AP_PWR_STATUS_2ND));
		CCCI_BOOTUP_LOG(md->index, TAG, "SLEEP_CLK_CON = 0x%x\n",
			     ccif_read32(hw_info->sleep_base, SLEEP_CLK_CON));
		CCCI_BOOTUP_LOG(md->index, TAG, "AP_POWERON_CONFIG_EN = 0x%x\n",
			     ccif_read32(hw_info->sleep_base, AP_POWERON_CONFIG_EN));

		/* step 1: config C2K boot mode */
		/* step 1.1: let CBP boot from EMI: [10:8] = 3'b101 */
		reg_value = ccif_read32(hw_info->infra_ao_base, C2K_CONFIG);
		reg_value &= (~(7<<8));
		reg_value |= (5<<8);
		ccif_write32(hw_info->infra_ao_base, C2K_CONFIG, reg_value);
		/* step 1.2: make CS_DEBUGOUT readable: [12:11] = 2'b00 */
		ccif_write32(hw_info->infra_ao_base, C2K_CONFIG,
				ccif_read32(hw_info->infra_ao_base, C2K_CONFIG) & (~(0x3 << 11)));
		/* step 1.3: C2K state matchine not wait md1src_ack: [0] = 1'b0 */
		ccif_write32(hw_info->infra_ao_base, C2K_CONFIG,
				ccif_read32(hw_info->infra_ao_base, C2K_CONFIG) & (~(0x1 << 0)));
		/* step 1.4: C2K state matchine md1src_req: [3] = 1'b1 */
		ccif_write32(hw_info->infra_ao_base, C2K_CONFIG,
				ccif_read32(hw_info->infra_ao_base, C2K_CONFIG) | (0x1 << 3));

		/* step 2: config srcclkena selection mask: |= 0x44 */
		ccif_write32(hw_info->infra_ao_base, INFRA_MISC2,
				ccif_read32(hw_info->infra_ao_base, INFRA_MISC2) | INFRA_MISC2_C2K_EN);
		CCCI_BOOTUP_LOG(md->index, TAG, "INFRA_MISC2 = 0x%x\n",
				ccif_read32(hw_info->infra_ao_base, INFRA_MISC2));


		/* step 3: config ClkSQ resigeter */
		ccif_write32(apmixed_base, AP_PLL_CON0, ccif_read32(apmixed_base, AP_PLL_CON0) | (0x1 << 1));
		CCCI_BOOTUP_LOG(md->index, TAG, "AP_PLL_CON0 = 0x%x\n", ccif_read32(apmixed_base, AP_PLL_CON0));

		/* step 4: hold C2K ARM core */
		ccif_write32(hw_info->infra_ao_base, C2K_CONFIG,
				 ccif_read32(hw_info->infra_ao_base, C2K_CONFIG) | (0x1 << 1));
		CCCI_BOOTUP_LOG(md->index, TAG, "C2K_CONFIG = 0x%x\n",
				ccif_read32(hw_info->infra_ao_base, C2K_CONFIG));

		/* step 5: wake up C2K */
		/* step 5.1: switch MDPLL1(208M) Control to hw mode: [28] = 1'b0 */
		ccif_write32(apmixed_base, MDPLL_CON3, ccif_read32(apmixed_base, MDPLL_CON3) & 0xEFFFFFFF);
		CCCI_BOOTUP_LOG(md->index, TAG, "MDPLL1_CON3 = 0x%x\n", ccif_read32(apmixed_base, MDPLL_CON3));
		/* step 5.2: release c2ksys_rstb */

		CCCI_BOOTUP_LOG(md->index, TAG,
				"[C2K] TOP_RGU_WDT_SWSYSRST = 0x%x\n",
				ccif_read32(hw_info->toprgu_base, TOP_RGU_WDT_SWSYSRST));
		/* step 5.4: wakeup C2KSYS: [1] = 1'b1 */
		ccif_write32(hw_info->infra_ao_base,
			     C2K_SPM_CTRL,
			     ccif_read32(hw_info->infra_ao_base, C2K_SPM_CTRL) | (0x1 << 1));
		/* step 5.5: polling C2K_STATUS[1] is high - C2KSYS has enter idle state */
		CCCI_BOOTUP_LOG(md->index, TAG,
				"[C2K] C2K_STATUS before = 0x%x\n",
				ccif_read32(hw_info->infra_ao_base, C2K_STATUS));
		while (!((ccif_read32(hw_info->infra_ao_base, C2K_STATUS) >> 1) & 0x1))
			;
		CCCI_BOOTUP_LOG(md->index, TAG,
				"[C2K] C2K_STATUS after = 0x%x\n",
				ccif_read32(hw_info->infra_ao_base, C2K_STATUS));
		/* step 5.6 */
		ccif_write32(hw_info->infra_ao_base,
				C2K_SPM_CTRL,
				ccif_read32(hw_info->infra_ao_base, C2K_SPM_CTRL) & (~(0x1 << 1)));
		CCCI_BOOTUP_LOG(md->index, TAG,
				"[C2K] C2K_SPM_CTRL = 0x%x, C2K_STATUS = 0x%x\n",
				ccif_read32(hw_info->infra_ao_base, C2K_SPM_CTRL),
				ccif_read32(hw_info->infra_ao_base, C2K_STATUS));
		ccif_write32(hw_info->infra_ao_base,
				INFRA_TOPAXI_PROTECTEN_1_CLR,
				ccif_read32(hw_info->infra_ao_base,
					 INFRA_TOPAXI_PROTECTEN_1_SET) & 0x03C00000);
		/* step 5.7: waiting for C2KSYS bus ready for operation */
		while (ccif_read32(md_info->c2k_cgbr1_addr, 0) != 0xFE8)
			;
		CCCI_BOOTUP_LOG(md->index, TAG, "[C2K] C2K_CGBR1 = 0x%x\n",
				ccif_read32(md_info->c2k_cgbr1_addr, 0));

		/* step 6: initialize C2K PLL */
		config_c2k_pll();

		/* step 7: release C2K ARM core */
		ccif_write32(hw_info->infra_ao_base, C2K_CONFIG,
				ccif_read32(hw_info->infra_ao_base, C2K_CONFIG) & (~(0x1 << 1)));

		break;
	}
	return 0;
}

int md_ccif_power_on(struct ccci_modem *md)
{
	int ret = 0;
	struct md_sys3_info *md_info = (struct md_sys3_info *)md->private_data;

	switch (md->index) {
	case MD_SYS3:
#if defined(CONFIG_MTK_CLKMGR)
		CCCI_NORMAL_LOG(md->index, TAG, "Call start md_power_on()\n");
		ret = md_power_on(SYS_MD2);
		CCCI_NORMAL_LOG(md->index, TAG, "Call end md_power_on() ret=%d\n", ret);
#else
		CCCI_NORMAL_LOG(md->index, TAG, "Call start clk_prepare_enable()\n");
		ret = clk_prepare_enable(clk_scp_sys_md3_main);
		CCCI_NORMAL_LOG(md->index, TAG, "Call end clk_prepare_enable() ret=%d\n", ret);
#endif
		/*kicker_pbm_by_md(KR_MD3, true);*/
		CCCI_NORMAL_LOG(md->index, TAG, "Call end kicker_pbm_by_md(3,true)\n");
		break;
	}
	CCCI_NORMAL_LOG(md->index, TAG, "md_ccif_power_on:ret=%d\n", ret);
	if (ret == 0 && md->index != MD_SYS3) {
		/*disable MD WDT */
		ccif_write32(md_info->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	}
	return ret;
}

int md_ccif_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;

	switch (md->index) {
	case MD_SYS2:
#if defined(CONFIG_MTK_CLKMGR)
		ret = md_power_off(SYS_MD2, timeout);
#else
		clk_disable_unprepare(clk_scp_sys_md2_main);
#endif
		break;
	case MD_SYS3:
#if defined(CONFIG_MTK_CLKMGR)
		ret = md_power_off(SYS_MD3, timeout);
#else
		clk_disable_unprepare(clk_scp_sys_md3_main);
#endif
		/*kicker_pbm_by_md(KR_MD3, false);*/
		CCCI_NORMAL_LOG(md->index, TAG, "Call end kicker_pbm_by_md(3,false)\n");
		break;
	}
	CCCI_NORMAL_LOG(md->index, TAG, "md_ccif_power_off:ret=%d\n", ret);
	return ret;
}

void reset_md1_md3_pccif(struct ccci_modem *md)
{
	ccci_set_clk_cg(md, 1);
	reset_ccirq_hardware(md);
	ccci_set_clk_cg(md, 0);
}

void dump_c2k_register(struct ccci_modem *md, unsigned int dump_boot_reg)
{
	CCCI_NORMAL_LOG(md->index, TAG, "INFRA_C2K_BOOT_STATUS = 0x%x\n",
			ccif_read32(apinfra_base, INFRA_C2K_BOOT_STATUS));
	CCCI_NORMAL_LOG(md->index, TAG, "INFRA_C2K_BOOT_STATUS2 = 0x%x\n",
			ccif_read32(apinfra_base, INFRA_C2K_BOOT_STATUS2));

	CCCI_NORMAL_LOG(md->index, TAG, "C2K_CONFIG = 0x%x\n",
			 ccif_read32(md->hw_info->infra_ao_base, C2K_CONFIG));
	CCCI_NORMAL_LOG(md->index, TAG, "[C2K] C2K_STATUS = 0x%x\n",
				 ccif_read32(md->hw_info->infra_ao_base, C2K_STATUS));

	if (dump_boot_reg == 0)
		return;

}

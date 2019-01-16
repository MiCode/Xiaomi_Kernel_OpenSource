#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_gpio.h>
#include <mach/mt_clkbuf_ctl.h>
#include <mach/mt_clkmgr.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>

#include "ccci_core.h"
#include "ccci_platform.h"
#include "modem_cldma.h"
#include "cldma_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

extern unsigned long infra_ao_base;
extern void ccci_mem_dump(int md_id, void *start_addr, int len);

#define TAG "mcd"

int md_cd_get_modem_hw_info(struct platform_device *dev_ptr, struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
    struct device_node *node=NULL;
    memset(dev_cfg, 0, sizeof(struct ccci_dev_cfg));
    memset(hw_info, 0, sizeof(struct md_hw_info));
    if(dev_ptr->dev.of_node == NULL) {
        CCCI_ERR_MSG(dev_cfg->index, TAG, "modem OF node NULL\n");
        return -1;
    }

    of_property_read_u32(dev_ptr->dev.of_node, "cell-index", &dev_cfg->index);
    CCCI_INF_MSG(dev_cfg->index, TAG, "modem hw info get idx:%d\n", dev_cfg->index);
    if(!get_modem_is_enabled(dev_cfg->index)) {
        CCCI_ERR_MSG(dev_cfg->index, TAG, "modem %d not enable, exit\n", dev_cfg->index + 1);
        return -1;
    }

    switch(dev_cfg->index) {
    case 0: //MD_SYS1
        of_property_read_u32(dev_ptr->dev.of_node, "cldma,major", &dev_cfg->major);
        of_property_read_u32(dev_ptr->dev.of_node, "cldma,minor_base", &dev_cfg->minor_base);
        of_property_read_u32(dev_ptr->dev.of_node, "cldma,capability", &dev_cfg->capability);
        hw_info->cldma_ap_pdn_base = of_iomap(dev_ptr->dev.of_node, 0);
        hw_info->cldma_ap_ao_base = hw_info->cldma_ap_pdn_base;
        hw_info->cldma_md_pdn_base = of_iomap(dev_ptr->dev.of_node, 1);
        hw_info->cldma_md_ao_base = hw_info->cldma_md_pdn_base;
        hw_info->ap_ccif_base = of_iomap(dev_ptr->dev.of_node, 2);
        node = of_find_compatible_node(NULL, NULL, "mediatek,MD_CCIF0");
        hw_info->md_ccif_base = of_iomap(node, 0);

        hw_info->cldma_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
        hw_info->ap_ccif_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 1);
        hw_info->md_wdt_irq_id = irq_of_parse_and_map(dev_ptr->dev.of_node, 2);

        // Device tree using none flag to register irq, sensitivity has set at "irq_of_parse_and_map"
        hw_info->cldma_irq_flags = IRQF_TRIGGER_NONE;
        hw_info->ap_ccif_irq_flags = IRQF_TRIGGER_NONE;
        hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;
        hw_info->ap2md_bus_timeout_irq_flags = IRQF_TRIGGER_NONE;

        hw_info->sram_size = CCIF_SRAM_SIZE;
        hw_info->md_rgu_base = MD_RGU_BASE;
        hw_info->md_boot_slave_Vector = MD_BOOT_VECTOR;
        hw_info->md_boot_slave_Key = MD_BOOT_VECTOR_KEY;
        hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;
        break;
    default:
        return -1;
    }

    CCCI_INF_MSG(dev_cfg->index, TAG, "modem cldma of node get dev_major:%d\n", dev_cfg->major);
    CCCI_INF_MSG(dev_cfg->index, TAG, "modem cldma of node get minor_base:%d\n", dev_cfg->minor_base);
    CCCI_INF_MSG(dev_cfg->index, TAG, "modem cldma of node get capability:%d\n", dev_cfg->capability);

    CCCI_INF_MSG(dev_cfg->index, TAG, "ap_cldma_base:0x%p\n", (void*)hw_info->cldma_ap_pdn_base);
    CCCI_INF_MSG(dev_cfg->index, TAG, "md_cldma_base:0x%p\n",(void*) hw_info->cldma_md_pdn_base);
    CCCI_INF_MSG(dev_cfg->index, TAG, "ap_ccif_base:0x%p\n",(void*) hw_info->ap_ccif_base);
    CCCI_INF_MSG(dev_cfg->index, TAG, "cldma_irq_id:%d\n", hw_info->cldma_irq_id);
    CCCI_INF_MSG(dev_cfg->index, TAG, "ccif_irq_id:%d\n", hw_info->ap_ccif_irq_id);
    CCCI_INF_MSG(dev_cfg->index, TAG, "md_wdt_irq_id:%d\n", hw_info->md_wdt_irq_id);

    return 0;
}

int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	md_ctrl->cldma_ap_pdn_base = (void __iomem *)(md_ctrl->hw_info->cldma_ap_pdn_base);
	md_ctrl->cldma_md_pdn_base = (void __iomem *)(md_ctrl->hw_info->cldma_md_pdn_base);
	md_ctrl->cldma_ap_ao_base = (void __iomem *)(md_ctrl->hw_info->cldma_ap_ao_base);
	md_ctrl->cldma_md_ao_base = (void __iomem *)(md_ctrl->hw_info->cldma_md_ao_base);
	md_ctrl->md_boot_slave_Vector = ioremap_nocache(md_ctrl->hw_info->md_boot_slave_Vector, 0x4);
	md_ctrl->md_boot_slave_Key = ioremap_nocache(md_ctrl->hw_info->md_boot_slave_Key, 0x4);
	md_ctrl->md_boot_slave_En = ioremap_nocache(md_ctrl->hw_info->md_boot_slave_En, 0x4);
	md_ctrl->md_rgu_base = ioremap_nocache(md_ctrl->hw_info->md_rgu_base, 0x40);
	md_ctrl->md_global_con0 = ioremap_nocache(MD_GLOBAL_CON0, 0x4);

	md_ctrl->md_bus_status = ioremap_nocache(MD_BUS_STATUS_BASE, MD_BUS_STATUS_LENGTH);
	md_ctrl->md_pc_monitor = ioremap_nocache(MD_PC_MONITOR_BASE, MD_PC_MONITOR_LENGTH);
	md_ctrl->md_topsm_status = ioremap_nocache(MD_TOPSM_STATUS_BASE, MD_TOPSM_STATUS_LENGTH);
	md_ctrl->md_ost_status = ioremap_nocache(MD_OST_STATUS_BASE, MD_OST_STATUS_LENGTH);
#ifdef MD_PEER_WAKEUP
	md_ctrl->md_peer_wakeup = ioremap_nocache(MD_PEER_WAKEUP, 0x4);
#endif
	return 0;
}

void md_cd_dump_debug_register(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
        md_cd_lock_cldma_clock_src(1);
	CCCI_INF_MSG(md->index, TAG, "Dump MD Bus status %x\n", MD_BUS_STATUS_BASE);
	ccci_mem_dump(md->index,md_ctrl->md_bus_status, MD_BUS_STATUS_LENGTH);
	CCCI_INF_MSG(md->index, TAG, "Dump MD PC monitor %x\n", MD_PC_MONITOR_BASE);
	ccci_write32(md_ctrl->md_pc_monitor, 0, 0x80000000); // stop MD PCMon
	ccci_mem_dump(md->index, md_ctrl->md_pc_monitor, MD_PC_MONITOR_LENGTH);
	ccci_write32(md_ctrl->md_pc_monitor, 0, 0x1); // restart MD PCMon
	CCCI_INF_MSG(md->index, TAG, "Dump MD TOPSM status %x\n", MD_TOPSM_STATUS_BASE);
	ccci_mem_dump(md->index, md_ctrl->md_topsm_status, MD_TOPSM_STATUS_LENGTH);
	CCCI_INF_MSG(md->index, TAG, "Dump MD OST status %x\n", MD_OST_STATUS_BASE);
	ccci_mem_dump(md->index, md_ctrl->md_ost_status, MD_OST_STATUS_LENGTH);
        md_cd_lock_cldma_clock_src(0);
}

void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

int md_cd_power_on(struct ccci_modem *md)
{
    int ret = 0;
    struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
#ifdef FEATURE_RF_CLK_BUF
    //config RFICx as BSI
    mutex_lock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->down(&clk_buf_ctrl_lock_2);
    CCCI_INF_MSG(md->index, TAG, "clock buffer, BSI mode\n"); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_01); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_01);
#endif
	// power on MD_INFRA and MODEM_TOP
    switch(md->index)
    {
        case MD_SYS1:
       	CCCI_INF_MSG(md->index, TAG, "Call start md_power_on()\n"); 
        ret = md_power_on(SYS_MD1);
        CCCI_INF_MSG(md->index, TAG, "Call end md_power_on() ret=%d\n",ret); 
        break;
    }
#ifdef FEATURE_RF_CLK_BUF 
	mutex_unlock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->delete
#endif
	if(ret)
		return ret;
	// disable MD WDT
	cldma_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	return 0;
}

int md_cd_bootup_cleanup(struct ccci_modem *md, int success)
{
	return 0;
}

int md_cd_let_md_go(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	if(MD_IN_DEBUG(md))
		return -1;
	CCCI_INF_MSG(md->index, TAG, "set MD boot slave\n"); 
	// set the start address to let modem to run
	cldma_write32(md_ctrl->md_boot_slave_Key, 0, 0x3567C766); // make boot vector programmable
	cldma_write32(md_ctrl->md_boot_slave_Vector, 0, 0x00000001); // after remap, MD ROM address is 0 from MD's view, MT6595 uses Thumb code
	cldma_write32(md_ctrl->md_boot_slave_En, 0, 0xA3B66175); // make boot vector take effect
	return 0;
}

int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
    int ret = 0;
#ifdef FEATURE_RF_CLK_BUF  
    mutex_lock(&clk_buf_ctrl_lock); 
#endif
    // power off MD_INFRA and MODEM_TOP
    switch(md->index)
    {
        case MD_SYS1:
        ret = md_power_off(SYS_MD1, timeout);
        break;
    }
#ifdef FEATURE_RF_CLK_BUF
    // config RFICx as GPIO
    CCCI_INF_MSG(md->index, TAG, "clock buffer, GPIO mode\n"); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_GPIO); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_GPIO);
	mutex_unlock(&clk_buf_ctrl_lock);
#endif

    return ret;
}

void md_cd_lock_cldma_clock_src(int locked)
{
	spm_ap_mdsrc_req(locked);
}

void cldma_dump_register(struct ccci_modem *md)
{
    struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
   
    md_cd_lock_cldma_clock_src(1);
    printk("[CCCI%d-DUMP]dump AP CLDMA md_global_con0=0x%x\n",md->index+1, cldma_read32(md_ctrl->md_global_con0, 0) );
    printk("[CCCI%d-DUMP]dump AP CLDMA Tx register, active=%x\n",md->index+1, md_ctrl->txq_active);
    ccci_mem_dump(md->index, md_ctrl->cldma_ap_pdn_base+CLDMA_AP_UL_START_ADDR_0, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE-CLDMA_AP_UL_START_ADDR_0+4);
    printk("[CCCI%d-DUMP]dump AP CLDMA Rx register, active=%x\n",md->index+1, md_ctrl->rxq_active);
    ccci_mem_dump(md->index, md_ctrl->cldma_ap_pdn_base+CLDMA_AP_SO_ERROR, CLDMA_AP_DEBUG_ID_EN-CLDMA_AP_SO_ERROR+4);
    printk("[CCCI%d-DUMP]dump AP CLDMA MISC register\n",md->index+1);
    ccci_mem_dump(md->index, md_ctrl->cldma_ap_pdn_base+CLDMA_AP_L2TISAR0, CLDMA_AP_CHNL_IDLE-CLDMA_AP_L2TISAR0+4);
    printk("[CCCI%d-DUMP]dump MD CLDMA Tx register\n",md->index+1);
    ccci_mem_dump(md->index, md_ctrl->cldma_md_pdn_base+CLDMA_AP_UL_START_ADDR_0, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE-CLDMA_AP_UL_START_ADDR_0+4);
    printk("[CCCI%d-DUMP]dump MD CLDMA Rx register\n",md->index+1);
    ccci_mem_dump(md->index, md_ctrl->cldma_md_pdn_base+CLDMA_AP_SO_ERROR, CLDMA_AP_DEBUG_ID_EN-CLDMA_AP_SO_ERROR+4);
    printk("[CCCI%d-DUMP]dump MD CLDMA MISC register\n",md->index+1);
    ccci_mem_dump(md->index, md_ctrl->cldma_md_pdn_base+CLDMA_AP_L2TISAR0, CLDMA_AP_CHNL_IDLE-CLDMA_AP_L2TISAR0+4);
    md_cd_lock_cldma_clock_src(0);
}

int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

void ccci_modem_shutdown(struct platform_device *dev)
{
}

int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_INF_MSG(md->index, TAG, "AP_BUSY(%p)=%x\n", md_ctrl->ap_ccif_base+APCCIF_BUSY, cldma_read32(md_ctrl->ap_ccif_base, APCCIF_BUSY));
	CCCI_INF_MSG(md->index, TAG, "MD_BUSY(%p)=%x\n", md_ctrl->md_ccif_base+APCCIF_BUSY, cldma_read32(md_ctrl->md_ccif_base, APCCIF_BUSY));

	return 0;
}

int ccci_modem_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	cldma_write32(md_ctrl->ap_ccif_base, APCCIF_CON, 0x01); // arbitration
	return 0;
}

int ccci_modem_pm_suspend(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

int ccci_modem_pm_resume(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_resume(pdev);
}

int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;
    struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	// IPO-H
    // restore IRQ
//#ifdef FEATURE_PM_IPO_H
#if 0
    irq_set_irq_type(md_ctrl->cldma_irq_id, IRQF_TRIGGER_HIGH); 
    irq_set_irq_type(md_ctrl->md_wdt_irq_id, IRQF_TRIGGER_FALLING); 
#endif
	// set flag for next md_start
	md->config.setting |= MD_SETTING_RELOAD;
	md->config.setting |= MD_SETTING_FIRST_BOOT;
    return 0;
}


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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/types.h>

#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif

#include "mach/mt_reg_base.h"
#include "mach/mt_typedefs.h"
#include "mach/sync_write.h"
#include "mach/irqs.h"
#include "devapc.h"
#include "mt_device_apc.h"

static struct cdev *g_devapc_ctrl;
static unsigned int devapc_irq;
static void __iomem *devapc_ao_base;
static void __iomem *devapc_pd_base;
static void __iomem *devapc_clk_base;

#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static struct DEVICE_INFO devapc_devices[] = {
	{"INFRA_AO_TOP_LEVEL_CLOCK_GENERATOR", FALSE},
	{"INFRA_AO_INFRASYS_CONFIG_REGS", FALSE},
	{"INFRA_AO_FHCTL", FALSE},
	{"INFRA_AO_PERISYS_CONFIG_REGS", FALSE},
	{"INFRA_AO_DRAMC_CONTROLLER", FALSE},
	{"INFRA_AO_GPIO_CONTROLLER", FALSE},
	{"INFRA_AO_TOP_LEVEL_SLP_MANAGER", FALSE},
	{"INFRA_AO_TOP_LEVEL_RESET_GENERATOR", FALSE},
	{"INFRA_AO_GPT", FALSE},
	{"INFRA_AO_RSVD", FALSE},
	/*10 */
	{"INFRA_AO_SEJ", FALSE},
	{"INFRA_AO_APMCU_EINT_CONTROLLER", FALSE},
	{"INFRA_AO_SMI_CONTROL_REG1", FALSE},
	{"INFRA_AO_PMIC_WRAP_CONTROL_REG", FALSE},
	{"INFRA_AO_DEVICE_APC_AO", FALSE},
	{"INFRA_AO_DDRPHY_CONTROL_REG", FALSE},
	{"INFRA_AO_KPAD_CONTROL_REG", FALSE},
	{"INFRA_AO_DRAM_B_CONTROLLER", FALSE},
	{"INFRA_AO_DDRPHY_B_CONTROL_REG", FALSE},
	{"INFRA_AO_CEC_CONTROL_REG", FALSE},
	/*20 */
	{"INFRA_AO_SPM_MD32", FALSE},
	{"INFRASYS_MCUSYS_CONFIG_REG", FALSE},
	{"INFRASYS_CONTROL_REG", FALSE},
	{"INFRASYS_BOOTROM/SRAM", FALSE},
	{"INFRASYS_EMI_BUS_INTERFACE", FALSE},
	{"INFRASYS_SYSTEM_CIRQ", FALSE},
	{"INFRASYS_MM_IOMMU_CONFIGURATION", FALSE},
	{"INFRASYS_EFUSEC", FALSE},
	{"INFRASYS_DEVICE_APC_MONITOR", FALSE},
	{"INFRASYS_MCU_BIU_CONFIGURATION", FALSE},
	/*30 */
	{"INFRASYS_AP_MIXED_CONTROL_REG", FALSE},
	{"INFRASYS_CA7_AP_CCIF", FALSE},
	{"INFRASYS_CA7_MD_CCIF", FALSE},
	{"INFRASYS_GPIO1_CONTROLLER", FALSE},
	{"INFRASYS_MBIST_CONTROL_REG", FALSE},
	{"INFRASYS_DRAMC_NAO_REGION_REG", FALSE},
	{"INFRASYS_TRNG", FALSE},
	{"INFRASYS_GCPU", FALSE},
	{"INFRASYS_GCPU_NS", FALSE},
	{"INFRASYS_GCE", FALSE},
	/*40 */
	{"INFRASYS_DRAM_B_NAO_CONTROLLER", FALSE},
	{"INFRASYS_PERISYS_IOMMU", FALSE},
	{"INFRA_AO_ANA_MIPI_DSI0", FALSE},
	{"INFRA_AO_ANA_MIPI_DSI1", FALSE},
	{"INFRA_AO_ANA_MIPI_CSI0", FALSE},
	{"INFRA_AO_ANA_MIPI_CSI1", FALSE},
	{"INFRA_AO_ANA_MIPI_CSI2", FALSE},
	{"DEGBUGSYS", FALSE},
	{"DMA", FALSE},
	{"AUXADC", FALSE},
	/*50 */
	{"UART0", FALSE},
	{"UART1", FALSE},
	{"UART2", FALSE},
	{"UART3", FALSE},
	{"PWM", FALSE},
	{"I2C0", FALSE},
	{"I2C1", FALSE},
	{"I2C2", FALSE},
	{"SPI0", FALSE},
	{"PTP", FALSE},
	/*60 */
	{"Irda", FALSE},
	{"NFI", FALSE},
	{"NFI_ECC", FALSE},
	{"NLI_ARBITER", FALSE},
	{"I2C3", FALSE},
	{"I2C4", FALSE},
	{"I2C5", FALSE},
	{"I2C6", FALSE},
	{"HSIC", FALSE},
	{"HSIC SIF", FALSE},
	/*70 */
	{"AUDIO", FALSE},
	{"MSDC0", FALSE},
	{"MSDC1", FALSE},
	{"MSDC2", FALSE},
	{"MSDC3", FALSE},
	{"SSUSB CSR", FALSE},
	{"SSUSB SIF", FALSE},
	{"SSUSB MI", FALSE},
	{"RSVD", FALSE},
	{"G3D_CONFIG", FALSE},
	/*80 */
	{"MMSYS_CONFIG", FALSE},
	{"MDP_RDMA0", FALSE},
	{"MDP_RDMA1", FALSE},
	{"MDP_RSZ0", FALSE},
	{"MDP_RSZ1", FALSE},
	{"MDP_RSZ2", FALSE},
	{"MDP_WDMA", FALSE},
	{"MDP_WROT1", FALSE},
	{"MDP_WROT2", FALSE},
	{"MDP_TDSHP0", FALSE},
	/*90 */
	{"MDP_TDSHP1", FALSE},
	{"MDP_CROP", FALSE},
	{"DISP_OVL0", FALSE},
	{"DISP_OVL1", FALSE},
	{"DISP_RDMA0", FALSE},
	{"DISP_RDMA1", FALSE},
	{"DISP_RDMA2", FALSE},
	{"DISP_WDMA0", FALSE},
	{"DISP_WDMA1", FALSE},
	{"DISP_COLOR0", FALSE},
	/*100 */
	{"DISP_COLOR1", FALSE},
	{"DISP_AAL", FALSE},
	{"DISP_GAMMA", FALSE},
	{"DISP_MERGE", FALSE},
	{"DISP_SPLIT", FALSE},
	{"DSI_UFO", FALSE},
	{"DSI_DISPATCHER", FALSE},
	{"DSI0", FALSE},
	{"DSI1", FALSE},
	{"DPI", FALSE},
	/*110 */
	{"DISP_PWM0", FALSE},
	{"DISP_PWM1", FALSE},
	{"MM_MUTEX", FALSE},
	{"SMI_LARB0", FALSE},
	{"SMI_COMMON", FALSE},
	{"DISP_OD", FALSE},
	{"DPI1", FALSE},
	{"HDMI", FALSE},
	{"LVDS", FALSE},
	/*{"SMI_LARB4",                                   FALSE}, KSY: 8173 add later */
	{"IMGSYS_CONFIG", FALSE},
	/*120 */
	{"IMGSYS_SMI_LARB2", FALSE},
	{"IMGSYS_CAM0", FALSE},
	{"IMGSYS_CAM1", FALSE},
	{"IMGSYS_CAM2", FALSE},
	{"IMGSYS_CAM3", FALSE},
	{"IMGSYS_SENINF", FALSE},
	{"IMGSYS_CAMSV", FALSE},
	{"IMGSYS_FD", FALSE},
	{"IMGSYS_MIPI_RX", FALSE},
	{"CAM0", FALSE},
	/*130 */
	{"CAM2", FALSE},
	{"CAM3", FALSE},
	{"VDECSYS_GLOBAL_CONFIGURATION", FALSE},
	{"VDECSYS_SMI_LARB1", FALSE},
	{"VDEC_FULL_TOP", FALSE},
	{"VENC_LT_GLOBAL_CON", FALSE},
	{"SMI_LARB5", FALSE},
	{"VENC_LT", FALSE},
	{"VENC_GLOBAL_CON", FALSE},
	{"SMI_LARB3", FALSE},
	/*140 */
	{"VENC", FALSE},
	{"JPGENC", FALSE},
	{"JPGDEC", FALSE},
};
#endif

/*****************************************************************************
*FUNCTION DEFINITION
*****************************************************************************/
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static int clear_vio_status(unsigned int module);
#endif
static int devapc_ioremap(void);
/**************************************************************************
*EXTERN FUNCTION
**************************************************************************/
int mt_devapc_check_emi_violation(void)
{
	if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_4)) & ABORT_EMI) == 0) {
		return -1;

	pr_debug("EMI violation! It should be cleared by EMI MPU driver later!\n");
	return 0;
}

int mt_devapc_emi_initial(void)
{
	devapc_ioremap();
	mt_reg_sync_writel(readl(IOMEM(DEVAPC0_APC_CON)) & (0xFFFFFFFF ^ (1 << 2)),
			   DEVAPC0_APC_CON);
	mt_reg_sync_writel(readl(IOMEM(DEVAPC0_PD_APC_CON)) & (0xFFFFFFFF ^ (1 << 2)),
			   DEVAPC0_PD_APC_CON);
	mt_reg_sync_writel(ABORT_EMI, DEVAPC0_D0_VIO_STA_4);
	mt_reg_sync_writel(readl(IOMEM(DEVAPC0_D0_VIO_MASK_4)) & (0xFFFFFFFF ^ (ABORT_EMI)),
			   DEVAPC0_D0_VIO_MASK_4);
	pr_debug("EMI_DAPC Init done\n");
	return 0;
}

int mt_devapc_clear_emi_violation(void)
{
	if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_4)) & ABORT_EMI) != 0)
		mt_reg_sync_writel(ABORT_EMI, DEVAPC0_D0_VIO_STA_4);

	return 0;
}


 /*
  * mt_devapc_set_permission: set module permission on device apc.
  * @module: the moudle to specify permission
  * @domain_num: domain index number
  * @permission_control: specified permission
  * no return value.
  */
int mt_devapc_set_permission(unsigned int module, E_MASK_DOM domain_num, APC_ATTR permission)
{
	unsigned int *base;
	unsigned int clr_bit = 0x3 << ((module % 16) * 2);
	unsigned int set_bit = permission << ((module % 16) * 2);

	if (module >= DEVAPC_DEVICE_NUMBER) {
		pr_warn("[DEVAPC] ERROR, device number %d exceeds the max number!\n", module);
		return -1;
	}

	if (DEVAPC_DOMAIN_AP == domain_num) {
		base = DEVAPC0_D0_APC_0 + (module / 16) * 4;
	} else if (DEVAPC_DOMAIN_MD == domain_num) {
		base = DEVAPC0_D1_APC_0 + (module / 16) * 4;
	} else if (DEVAPC_DOMAIN_CONN == domain_num) {
		base = DEVAPC0_D2_APC_0 + (module / 16) * 4;
	} else if (DEVAPC_DOMAIN_MM == domain_num) {
		base = DEVAPC0_D3_APC_0 + (module / 16) * 4;
	} else {
		pr_warn("[DEVAPC] ERROR, domain number %d exceeds the max number!\n", domain_num);
		return -2;
	}

	mt_reg_sync_writel(readl(base) & ~clr_bit, base);
	mt_reg_sync_writel(readl(base) | set_bit, base);
	return 0;
}

/**************************************************************************
*STATIC FUNCTION
**************************************************************************/

static int devapc_ioremap(void)
{
	struct device_node *node = NULL;
	unsigned int devapc_ao_base_pa, devapc_pd_base_pa, devapc_clk_base_pa;

	/*IO remap */
	node = of_find_compatible_node(NULL, NULL, "mediatek,DEVAPC_AO");
	if (node) {
		devapc_ao_base = of_iomap(node, 0);
		of_property_read_u32_index(node, "reg", 0, &devapc_ao_base_pa);
		pr_warn("[DEVAPC] AO_ADDRESS: PA -> 0x%x, VA -> %p\n", devapc_ao_base_pa,
			devapc_ao_base);
	} else {
		pr_warn("[DEVAPC] can't find DAPC_AO compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,DEVAPC");
	if (node) {
		devapc_pd_base = of_iomap(node, 0);
		of_property_read_u32_index(node, "reg", 0, &devapc_pd_base_pa);
		devapc_irq = irq_of_parse_and_map(node, 0);
		pr_warn("[DEVAPC] PD_ADDRESS: PA -> 0x%x, VA -> %p, IRD: %d\n", devapc_pd_base_pa,
			devapc_pd_base, devapc_irq);
	} else {
		pr_warn("[DEVAPC] can't find DAPC_PD compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");
	if (node) {
		devapc_clk_base = of_iomap(node, 0);
		of_property_read_u32_index(node, "reg", 0, &devapc_clk_base_pa);
		pr_warn("[DEVAPC] CLK_ADDRESS: PA -> 0x%x, VA -> %p\n", devapc_clk_base_pa,
			devapc_clk_base);
	} else {
		pr_warn("[DEVAPC] can't find DAPC_CLK compatible node\n");
		return -1;
	}

	return 0;
}

#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#ifdef CONFIG_MTK_HIBERNATION
static int devapc_pm_restore_noirq(struct device *device)
{
	if (devapc_irq != 0) {
		mt_irq_set_sens(devapc_irq, MT_LEVEL_SENSITIVE);
		mt_irq_set_polarity(devapc_irq, MT_POLARITY_LOW);
	}

	return 0;
}
#endif
#endif

/*
 * set_module_apc: set module permission on device apc.
 * @module: the moudle to specify permission
 * @devapc_num: device apc index number (device apc 0 or 1)
 * @domain_num: domain index number (AP or MD domain)
 * @permission_control: specified permission
 * no return value.
 */
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static void set_module_apc(unsigned int module, E_MASK_DOM domain_num, APC_ATTR permission_control)
{

	unsigned int *base = 0;
	unsigned int clr_bit = 0x3 << ((module % 16) * 2);
	unsigned int set_bit = permission_control << ((module % 16) * 2);

	if (module >= DEVAPC_DEVICE_NUMBER) {
		pr_warn("set_module_apc : device number %d exceeds the max number!\n", module);
		return;
	}

	if (DEVAPC_DOMAIN_AP == domain_num) {
		base = (unsigned int *)((uintptr_t) DEVAPC0_D0_APC_0 + (module / 16) * 4);
	} else if (DEVAPC_DOMAIN_MD == domain_num) {
		base = (unsigned int *)((uintptr_t) DEVAPC0_D1_APC_0 + (module / 16) * 4);
	} else if (DEVAPC_DOMAIN_CONN == domain_num) {
		base = (unsigned int *)((uintptr_t) DEVAPC0_D2_APC_0 + (module / 16) * 4);
	} else if (DEVAPC_DOMAIN_MM == domain_num) {
		base = (unsigned int *)((uintptr_t) DEVAPC0_D3_APC_0 + (module / 16) * 4);
	} else {
		pr_warn("set_module_apc : domain number %d exceeds the max number!\n", domain_num);
		return;
	}

	mt_reg_sync_writel(readl(base) & ~clr_bit, base);
	mt_reg_sync_writel(readl(base) | set_bit, base);
}

/*
 * unmask_module_irq: unmask device apc irq for specified module.
 * @module: the moudle to unmask
 * @devapc_num: device apc index number (device apc 0 or 1)
 * @domain_num: domain index number (AP or MD domain)
 * no return value.
 */
static int unmask_module_irq(unsigned int module)
{

	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	switch (apc_index) {
	case 0:
		*DEVAPC0_D0_VIO_MASK_0 &= ~(0x1 << apc_bit_index);
		break;
	case 1:
		*DEVAPC0_D0_VIO_MASK_1 &= ~(0x1 << apc_bit_index);
		break;
	case 2:
		*DEVAPC0_D0_VIO_MASK_2 &= ~(0x1 << apc_bit_index);
		break;
	case 3:
		*DEVAPC0_D0_VIO_MASK_3 &= ~(0x1 << apc_bit_index);
		break;
	case 4:
		*DEVAPC0_D0_VIO_MASK_4 &= ~(0x1 << apc_bit_index);
		break;
	default:
		pr_warn
		    ("UnMask_Module_IRQ_ERR :check if domain master setting is correct or not !\n");
		break;
	}
	return 0;
}

static void init_devpac(void)
{
	/* clear violation */
	int i;

	for (i = 0; i < (sizeof(devapc_devices) / sizeof(devapc_devices[0])); i++)
		clear_vio_status(i);

	mt_reg_sync_writel(VIO_DBG_CLR, DEVAPC0_VIO_DBG0);
	mt_reg_sync_writel(readl(DEVAPC0_APC_CON) & (0xFFFFFFFF ^ (1 << 2)), DEVAPC0_APC_CON);
	mt_reg_sync_writel(readl(DEVAPC0_PD_APC_CON) & (0xFFFFFFFF ^ (1 << 2)), DEVAPC0_PD_APC_CON);
}


/*
 * start_devapc: start device apc for MD
 */
static int start_devapc(void)
{
	int i = 0;

	init_devpac();
	for (i = 0; i < (sizeof(devapc_devices) / sizeof(devapc_devices[0])); i++) {
		if (TRUE == devapc_devices[i].forbidden) {
			clear_vio_status(i);
			unmask_module_irq(i);
			set_module_apc(i, E_DOMAIN_0, E_L3);
		}
	}
	return 0;
}

/*
 * clear_vio_status: clear violation status for each module.
 * @module: the moudle to clear violation status
 * @devapc_num: device apc index number (device apc 0 or 1)
 * @domain_num: domain index number (AP or MD domain)
 * no return value.
 */
static int clear_vio_status(unsigned int module)
{

	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	switch (apc_index) {
	case 0:
		*DEVAPC0_D0_VIO_STA_0 = (0x1 << apc_bit_index);
		break;
	case 1:
		*DEVAPC0_D0_VIO_STA_1 = (0x1 << apc_bit_index);
		break;
	case 2:
		*DEVAPC0_D0_VIO_STA_2 = (0x1 << apc_bit_index);
		break;
	case 3:
		*DEVAPC0_D0_VIO_STA_3 = (0x1 << apc_bit_index);
		break;
	case 4:
		*DEVAPC0_D0_VIO_STA_4 = (0x1 << apc_bit_index);
		break;
	default:
		break;
	}

	return 0;
}

static irqreturn_t devapc_violation_irq(int irq, void *dev_id)
{
	unsigned int dbg0 = 0, dbg1 = 0;
	unsigned int master_id;
	unsigned int domain_id;
	unsigned int r_w_violation;
	int i;

	dbg0 = readl(DEVAPC0_VIO_DBG0);
	dbg1 = readl(DEVAPC0_VIO_DBG1);
	master_id = dbg0 & VIO_DBG_MSTID;
	domain_id = dbg0 & VIO_DBG_DMNID;
	r_w_violation = dbg0 & VIO_DBG_RW;

	if (1 == r_w_violation) {
		pr_warn("Process:%s PID:%i Vio Addr:0x%x , Master ID:0x%x , Dom ID:0x%x, W\n",
			current->comm, current->pid, dbg1, master_id, domain_id);
	} else {
		pr_warn("Process:%s PID:%i Vio Addr:0x%x , Master ID:0x%x , Dom ID:0x%x, r\n",
			current->comm, current->pid, dbg1, master_id, domain_id);
	}

	pr_warn("VIO_STA -> 0:0x%x, 1:0x%x, 2:0x%x, 3:0x%x, 4:0x%x\n",
		readl(DEVAPC0_D0_VIO_STA_0), readl(DEVAPC0_D0_VIO_STA_1),
		readl(DEVAPC0_D0_VIO_STA_2), readl(DEVAPC0_D0_VIO_STA_3),
		readl(DEVAPC0_D0_VIO_STA_4));

	for (i = 0; i < (sizeof(devapc_devices) / sizeof(devapc_devices[0])); i++)
		clear_vio_status(i);

	mt_reg_sync_writel(VIO_DBG_CLR, DEVAPC0_VIO_DBG0);
	dbg0 = readl(DEVAPC0_VIO_DBG0);
	dbg1 = readl(DEVAPC0_VIO_DBG1);

	if ((dbg0 != 0) || (dbg1 != 0)) {
		pr_warn("[DEVAPC] Multi-violation!\n");
		pr_warn("[DEVAPC] DBG0 = %x, DBG1 = %x\n", dbg0, dbg1);
	}

	return IRQ_HANDLED;
}
#endif

static int devapc_probe(struct platform_device *dev)
{
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	int ret;
#endif

	/*IO remap */
	devapc_ioremap();

	/*OPEN DAPC CLOCK */
	mt_reg_sync_writel(DAPC_CLK_BIT, INFRA_PDN1);

	pr_warn("[DEVAPC] module probe.\n");

	if ((readl(INFRA_PDN_STA) & DAPC_CLK_BIT) == 0x0)
		pr_warn("[DEVAPC] The clock setting is OK!\n");
	else
		pr_warn("[DEVAPC] The clock setting is NOT OK...\n");

	/*
	 * Interrupts of vilation (including SPC in SMI, or EMI MPU) are triggered by the device APC.
	 * need to share the interrupt with the SPC driver.
	 */
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	pr_warn("[DEVAPC] Disable TEE...Request IRQ in REE.\n");

	ret = request_irq(devapc_irq, (irq_handler_t) devapc_violation_irq,
			  IRQF_TRIGGER_LOW | IRQF_SHARED, "devapc", &g_devapc_ctrl);
	if (ret) {
		pr_warn("[DEVAPC] Failed to request irq! (%d)\n", ret);
		return ret;
	}
#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_DEVAPC, devapc_pm_restore_noirq, NULL);
#endif

	start_devapc();

#else
	pr_warn("[DEVAPC] Enable TEE...Request IRQ in TEE.\n");
#endif
	return 0;
}


static int devapc_remove(struct platform_device *dev)
{
	return 0;
}

static int devapc_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int devapc_resume(struct platform_device *dev)
{
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	pr_debug("[DEVAPC] Disable TEE...module resume.\n");
	start_devapc();
#else
	pr_debug("[DEVAPC] Enable TEE...module resume.\n");
#endif
	return 0;
}

struct platform_device devapc_device = {
	.name = "devapc",
	.id = -1,
};

static struct platform_driver devapc_driver = {
	.probe = devapc_probe,
	.remove = devapc_remove,
	.suspend = devapc_suspend,
	.resume = devapc_resume,
	.driver = {
		   .name = "devapc",
		   .owner = THIS_MODULE,
		   },
};

/*
 * devapc_init: module init function.
 */
static int __init devapc_init(void)
{
	int ret;

	pr_warn("[DEVAPC] module init.\n");

	ret = platform_device_register(&devapc_device);
	if (ret) {
		pr_warn("[DEVAPC] Unable to do device register(%d)\n", ret);
		return ret;
	}

	ret = platform_driver_register(&devapc_driver);
	if (ret) {
		pr_warn("[DEVAPC] Unable to register driver (%d)\n", ret);
		platform_device_unregister(&devapc_device);
		return ret;
	}

	g_devapc_ctrl = cdev_alloc();
	if (!g_devapc_ctrl) {
		pr_warn("[DEVAPC] Failed to add devapc device! (%d)\n", ret);
		platform_driver_unregister(&devapc_driver);
		platform_device_unregister(&devapc_device);
		return ret;
	}
	g_devapc_ctrl->owner = THIS_MODULE;

	return 0;
}

/*
 * devapc_exit: module exit function.
 */
static void __exit devapc_exit(void)
{
	pr_debug("[DEVAPC] DEVAPC module exit\n");
#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_DEVAPC);
#endif
}
late_initcall(devapc_init);
module_exit(devapc_exit);
MODULE_LICENSE("GPL");

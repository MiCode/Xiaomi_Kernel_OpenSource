/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include "mt_spm_idle.h"
#include <mach/irqs.h>
#include <mt-plat/upmu_common.h>
#include "mt_spm_vcorefs.h"
#include "mt_spm_internal.h"
#include <mt_dramc.h> /* for ucDram_Register_Read () */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/irqchip/mt-eic.h>
/* #include <mach/eint.h> */
/* #include <mach/mt_boot.h> */
#ifdef CONFIG_MTK_WD_KICKER
#include <mach/wd_api.h>
#endif

#define ENABLE_DYNA_LOAD_PCM
#ifdef ENABLE_DYNA_LOAD_PCM	/* for dyna_load_pcm */
/* for request_firmware */
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/dcache.h>
#include <asm/cacheflush.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include "mt_spm_misc.h"
#ifndef dmac_map_area
#define dmac_map_area __dma_map_area
#endif

static struct dentry *spm_dir;
static struct dentry *spm_file;
struct platform_device *pspmdev;
static int dyna_load_pcm_done __nosavedata;
/* FIXME: add vendor/mediatek/proprietary/hardware/spm/mtxxxx */
static char *dyna_load_pcm_path[] = {
	[DYNA_LOAD_PCM_SUSPEND] = "pcm_suspend.bin",
	[DYNA_LOAD_PCM_SODI] = "pcm_sodi_ddrdfs.bin",
	[DYNA_LOAD_PCM_DEEPIDLE] = "pcm_deepidle.bin",
	[DYNA_LOAD_PCM_VCOREFS] = "pcm_vcorefs.bin",
	[DYNA_LOAD_PCM_MAX] = "pcm_path_max",
};

MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SUSPEND]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SODI]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_DEEPIDLE]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_VCOREFS]);

struct dyna_load_pcm_t dyna_load_pcm[DYNA_LOAD_PCM_MAX];

/* add char device for spm */
#include <linux/cdev.h>
#define SPM_DETECT_MAJOR 159	/* FIXME */
#define SPM_DETECT_DEV_NUM 1
#define SPM_DETECT_DRVIER_NAME "spm"
#define SPM_DETECT_DEVICE_NAME "spm"

struct class *pspmDetectClass = NULL;
struct device *pspmDetectDev = NULL;
static int gSPMDetectMajor = SPM_DETECT_MAJOR;
static struct cdev gSPMDetectCdev;

#endif /* ENABLE_DYNA_LOAD_PCM */

void __iomem *spm_base;
void __iomem *spm_cksys_base;
void __iomem *spm_mcucfg;
u32 spm_irq_0 = 224;

#if 0
u32 spm_vcorefs_start_irq = 152;
u32 spm_vcorefs_end_irq = 153;
#endif

/**************************************
 * Config and Parameter
 **************************************/

/**************************************
 * Define and Declare
 **************************************/
struct spm_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

static twam_handler_t spm_twam_handler;

#if 0
static vcorefs_handler_t vcorefs_handler;
static vcorefs_start_handler_t vcorefs_start_handler;
#endif

void __attribute__((weak)) spm_sodi3_init(void)
{

}

void __attribute__((weak)) spm_sodi_init(void)
{

}

void __attribute__((weak)) spm_deepidle_init(void)
{

}

void __attribute__((weak)) spm_vcorefs_init(void)
{

}

void __attribute__((weak)) mt_power_gs_dump_suspend(void)
{

}

int __attribute__((weak)) spm_fs_init(void)
{
	return 0;
}

/**************************************
 * Init and IRQ Function
 **************************************/
static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr;
	unsigned long flags;
	struct twam_sig twamsig;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct spm_data spm_d;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	spin_lock_irqsave(&__spm_lock, flags);
	/* get ISR status */
	isr = spm_read(SPM_IRQ_STA);
	if (isr & ISRS_TWAM) {
		twamsig.sig0 = spm_read(SPM_TWAM_LAST_STA0);
		twamsig.sig1 = spm_read(SPM_TWAM_LAST_STA1);
		twamsig.sig2 = spm_read(SPM_TWAM_LAST_STA2);
		twamsig.sig3 = spm_read(SPM_TWAM_LAST_STA3);
		udelay(40); /* delay 1T @ 32K */
	}

	/* clean ISR status */
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	spm_d.u.args.arg0 = isr;
	spm_to_sspm_command(SPM_IRQ0_HANDLER, &spm_d);
#else
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_ALL_EXC_TWAM);
	spm_write(SPM_IRQ_STA, isr);
	spm_write(SPM_SWINT_CLR, PCM_SW_INT0);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	spin_unlock_irqrestore(&__spm_lock, flags);

	if ((isr & ISRS_TWAM) && spm_twam_handler)
		spm_twam_handler(&twamsig);

	if (isr & (ISRS_SW_INT0 | ISRS_PCM_RETURN))
		spm_err("IRQ0 HANDLER SHOULD NOT BE EXECUTED (0x%x)\n", isr);

	return IRQ_HANDLED;
}

static int spm_irq_register(void)
{
	int i, err, r = 0;
	struct spm_irq_desc irqdesc[] = {
		{.irq = 0, .handler = spm_irq0_handler,}
	};
	irqdesc[0].irq = SPM_IRQ0_ID;
	for (i = 0; i < ARRAY_SIZE(irqdesc); i++) {
		if (cpu_present(i)) {
			err = request_irq(irqdesc[i].irq, irqdesc[i].handler,
					IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND | IRQF_PERCPU, "SPM", NULL);
			if (err) {
				spm_err("FAILED TO REQUEST IRQ%d (%d)\n", i, err);
				r = -EPERM;
			}
		}
	}
	return r;
}

#if 0
void spm_vcorefs_register_handler(vcorefs_handler_t handler, vcorefs_start_handler_t start_handler)
{
	vcorefs_handler = handler;
	vcorefs_start_handler = start_handler;
}
EXPORT_SYMBOL(spm_vcorefs_register_handler);

static irqreturn_t spm_vcorefs_start_handler(int irq, void *dev_id)
{
	if (vcorefs_start_handler)
		vcorefs_start_handler();

	mt_eint_virq_soft_clr(irq);
	return IRQ_HANDLED;
}

static irqreturn_t spm_vcorefs_end_handler(int irq, void *dev_id)
{
	u32 opp = 0;

	if (vcorefs_handler) {
		opp = spm_read(SPM_SW_RSV_5) & SPM_SW_RSV_5_LSB;
		vcorefs_handler(opp);
	}

	mt_eint_virq_soft_clr(irq);
	return IRQ_HANDLED;
}
#endif
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_reservedmem_define.h>
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

static void spm_register_init(void)
{
	unsigned long flags;
	struct device_node *node;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct spm_data spm_d;
	int ret;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node)
		spm_err("find SLEEP node failed\n");
	spm_base = of_iomap(node, 0);
	if (!spm_base)
		spm_err("base spm_base failed\n");

	spm_irq_0 = irq_of_parse_and_map(node, 0);
	if (!spm_irq_0)
		spm_err("get spm_irq_0 failed\n");

	/* cksys_base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,topckgen");
	if (!node)
		spm_err("[CLK_CKSYS] find node failed\n");
	spm_cksys_base = of_iomap(node, 0);
	if (!spm_cksys_base)
		spm_err("[CLK_CKSYS] base failed\n");

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
	if (!node)
		spm_err("[MCUCFG] find node failed\n");
	spm_mcucfg = of_iomap(node, 0);
	if (!spm_mcucfg)
		spm_err("[MCUCFG] base failed\n");

#if 0
	node = of_find_compatible_node(NULL, NULL, "mediatek,spm_vcorefs_start_eint");
	if (!node) {
		spm_err("find spm_vcorefs_start_eint failed\n");
	} else {
		int ret;
		u32 ints[2];

		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		mt_gpio_set_debounce(ints[0], ints[1]);
		spm_vcorefs_start_irq = irq_of_parse_and_map(node, 0);
		ret =
		    request_irq(spm_vcorefs_start_irq, spm_vcorefs_start_handler,
				IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, "spm_vcorefs_start_eint",
				NULL);
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,spm_vcorefs_end_eint");
	if (!node) {
		spm_err("find spm_vcorefs_end_eint failed\n");
	} else {
		int ret;
		u32 ints[2];

		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		mt_gpio_set_debounce(ints[0], ints[1]);
		spm_vcorefs_end_irq = irq_of_parse_and_map(node, 0);
		ret =
		    request_irq(spm_vcorefs_end_irq, spm_vcorefs_end_handler,
				IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, "spm_vcorefs_end_eint", NULL);
	}

	spm_err("spm_vcorefs_start_irq = %d, spm_vcorefs_end_irq = %d\n", spm_vcorefs_start_irq,
		spm_vcorefs_end_irq);
#endif
	spm_err("spm_base = %p, spm_irq_0 = %d\n", spm_base, spm_irq_0);
	spm_err("cksys_base = %p, spm_mcucfg = %p\n", spm_cksys_base, spm_mcucfg);

	spin_lock_irqsave(&__spm_lock, flags);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	spm_d.u.args.arg0 = sspm_reserve_mem_get_phys(SPM_MEM_ID);
	spm_d.u.args.arg1 = sspm_reserve_mem_get_size(SPM_MEM_ID);
	ret = spm_to_sspm_command(SPM_REGISTER_INIT, &spm_d);
	/* FIXME: wait spm return value ready */
#if 0
	if (ret < 0)
		BUG();
#endif
#else
	/* enable register control */
	spm_write(POWERON_CONFIG_EN, SPM_REGWR_CFG_KEY | BCLK_CG_EN_LSB);

	/* init power control register */
	/* dram will set this register */
	/* spm_write(SPM_POWER_ON_VAL0, 0); */
	spm_write(SPM_POWER_ON_VAL1, POWER_ON_VAL1_DEF);
	spm_write(PCM_PWR_IO_EN, 0);

	/* reset PCM */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | PCM_SW_RESET_LSB);
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
	BUG_ON((spm_read(PCM_FSM_STA) & 0x7fffff) != PCM_FSM_STA_DEF);	/* PCM reset failed */

	/* init PCM control register */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | RG_EN_IM_SLEEP_DVS_LSB);
	spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | REG_EVENT_LOCK_EN_LSB |
		  REG_SPM_SRAM_ISOINT_B_LSB | RG_PCM_WDT_EN_LSB | RG_AHBMIF_APBEN_LSB);
	spm_write(PCM_IM_PTR, 0);
	spm_write(PCM_IM_LEN, 0);

	/* clean wakeup event raw status */
	spm_write(SPM_WAKEUP_EVENT_MASK, SPM_WAKEUP_EVENT_MASK_DEF);

	/* clean ISR status */
	spm_write(SPM_IRQ_MASK, ISRM_ALL);
	spm_write(SPM_IRQ_STA, ISRC_ALL);
	spm_write(SPM_SWINT_CLR, PCM_SW_INT_ALL);

	/* init r7 with POWER_ON_VAL1 */
	spm_write(PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(PCM_PWR_IO_EN, 0);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	spin_unlock_irqrestore(&__spm_lock, flags);
}

int spm_module_init(void)
{
	int r = 0;

	spm_register_init();
	if (spm_irq_register() != 0)
		r = -EPERM;
#if defined(CONFIG_PM)
	if (spm_fs_init() != 0)
		r = -EPERM;
#endif

	spm_sodi3_init();
	spm_sodi_init();
	spm_deepidle_init();

#ifndef CONFIG_MTK_FPGA
	if (spm_golden_setting_cmp(1) != 0)
		aee_kernel_warning("SPM Warring", "dram golden setting mismach");
#endif /* CONFIG_MTK_FPGA */

	return r;
}

/* arch_initcall(spm_module_init); */

#ifdef ENABLE_DYNA_LOAD_PCM	/* for dyna_load_pcm */
static char *local_buf;
static dma_addr_t local_buf_dma;
static const struct firmware *spm_fw[DYNA_LOAD_PCM_MAX];

int spm_fw_count = DYNA_LOAD_PCM_MAX;
void *get_spm_firmware_version(uint32_t index)
{
	void *ptr = NULL;
	int loop = 30;

	while (dyna_load_pcm_done == 0 && loop > 0) {
		loop--;
		msleep(100);
	}

	if (index == 0) {
		ptr = (void *)&spm_fw_count;
		spm_crit("SPM firmware version count = %d\n", spm_fw_count);
	} else if (index <= DYNA_LOAD_PCM_MAX) {
		ptr = dyna_load_pcm[index - 1].version;
		spm_crit("SPM firmware version(0x%x) = %s\n", index - 1, (char *)ptr);
	}

	return ptr;
}
EXPORT_SYMBOL(get_spm_firmware_version);

/*Reserved memory by device tree!*/
int reserve_memory_spm_fn(struct reserved_mem *rmem)
{
	pr_info(" name: %s, base: 0x%llx, size: 0x%llx\n", rmem->name,
			   (unsigned long long)rmem->base, (unsigned long long)rmem->size);
	BUG_ON(rmem->size < PCM_FIRMWARE_SIZE * DYNA_LOAD_PCM_MAX);

	local_buf_dma = rmem->base;
	return 0;
}
RESERVEDMEM_OF_DECLARE(reserve_memory_test, "mediatek,spm-reserve-memory", reserve_memory_spm_fn);

int spm_load_pcm_firmware(struct platform_device *pdev)
{
	const struct firmware *fw;
	int err = 0;
	int i;
	int offset = 0;
	/* FIXME: need addr_2nd? */
	int addr_2nd = 0;
	int spm_fw_count = 0;

	if (!pdev)
		return err;

	if (dyna_load_pcm_done)
		return err;

	if (NULL == local_buf) {
		local_buf = (char *)ioremap_nocache(local_buf_dma, PCM_FIRMWARE_SIZE * DYNA_LOAD_PCM_MAX);
		if (!local_buf) {
			pr_debug("Failed to dma_alloc_coherent(), %d.\n", err);
			return -ENOMEM;
		}
	}

	for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
		u16 firmware_size = 0;
		int copy_size = 0;
		struct pcm_desc *pdesc = &(dyna_load_pcm[i].desc);
		int j = 0;

		spm_fw[i] = NULL;
		do {
			j++;
			pr_debug("try to request_firmware() %s - %d\n", dyna_load_pcm_path[i], j);
			err = request_firmware(&fw, dyna_load_pcm_path[i], &pdev->dev);
			if (err)
				pr_err("Failed to load %s, err = %d.\n", dyna_load_pcm_path[i], err);
		} while (err == -EAGAIN && j < 5);
		if (err) {
			pr_err("Failed to load %s, err = %d.\n", dyna_load_pcm_path[i], err);
			continue;
		}
		spm_fw[i] = fw;

		/* Do whatever it takes to load firmware into device. */
		/* start of binary size */
		offset = 0;
		copy_size = 2;
		memcpy(&firmware_size, fw->data, copy_size);

		/* start of binary */
		offset += copy_size;
		copy_size = firmware_size * 4;
		dyna_load_pcm[i].buf = local_buf + i * PCM_FIRMWARE_SIZE;
		dyna_load_pcm[i].buf_dma = local_buf_dma + i * PCM_FIRMWARE_SIZE;
		memcpy_toio(dyna_load_pcm[i].buf, fw->data + offset, copy_size);
		/* dmac_map_area((void *)dyna_load_pcm[i].buf, PCM_FIRMWARE_SIZE, DMA_TO_DEVICE); */

		/* start of pcm_desc without pointer */
		offset += copy_size;
		copy_size = sizeof(struct pcm_desc) - offsetof(struct pcm_desc, size);
		memcpy((void *)&(dyna_load_pcm[i].desc.size), fw->data + offset, copy_size);
		/* get minimum addr_2nd */
		if (pdesc->addr_2nd) {
			if (addr_2nd)
				addr_2nd = min_t(int, (int)pdesc->addr_2nd, (int)addr_2nd);
			else
				addr_2nd = pdesc->addr_2nd;
		}

		/* start of pcm_desc version */
		offset += copy_size;
		copy_size = fw->size - offset;
		snprintf(dyna_load_pcm[i].version, PCM_FIRMWARE_VERSION_SIZE - 1,
				"%s", fw->data + offset);
		pdesc->version = dyna_load_pcm[i].version;
		pdesc->base = (u32 *) dyna_load_pcm[i].buf;
		pdesc->base_dma = dyna_load_pcm[i].buf_dma;

		dyna_load_pcm[i].ready = 1;
		spm_fw_count++;
	}

	/* FIXME: need addr_2nd? */
#if 0
	/* check addr_2nd */
	if (spm_fw_count == DYNA_LOAD_PCM_MAX) {
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
			struct pcm_desc *pdesc = &(dyna_load_pcm[i].desc);

			if (!pdesc->version)
				continue;

			if (pdesc->addr_2nd == 0) {
				if (addr_2nd == (pdesc->size - 3))
					*(u16 *) &pdesc->size = addr_2nd;
				else
					BUG();
			}
		}
	}

	if (spm_fw_count == DYNA_LOAD_PCM_MAX) {
		spm_vcorefs_init();
		dyna_load_pcm_done = 1;
	}
#else
	if (spm_fw_count == DYNA_LOAD_PCM_MAX)
		dyna_load_pcm_done = 1;
#endif

	return err;
}

int spm_load_pcm_firmware_nodev(void)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	spm_vcorefs_init();
#else
	spm_load_pcm_firmware(pspmdev);
#endif
	return 0;
}

int spm_load_firmware_status(void)
{
	return dyna_load_pcm_done;
}

static int spm_dbg_show_firmware(struct seq_file *s, void *unused)
{
	int i;
	struct pcm_desc *pdesc = NULL;

	for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
		pdesc = &(dyna_load_pcm[i].desc);
		seq_printf(s, "#@# %s\n", dyna_load_pcm_path[i]);

		if (pdesc->version) {
			seq_printf(s, "#@#  version = %s\n", pdesc->version);
			seq_printf(s, "#@#  base = 0x%p\n", pdesc->base);
			seq_printf(s, "#@#  size = %u\n", pdesc->size);
			seq_printf(s, "#@#  sess = %u\n", pdesc->sess);
			seq_printf(s, "#@#  replace = %u\n", pdesc->replace);
			seq_printf(s, "#@#  addr_2nd = %u\n", pdesc->addr_2nd);
			seq_printf(s, "#@#  vec0 = 0x%x\n", pdesc->vec0);
			seq_printf(s, "#@#  vec1 = 0x%x\n", pdesc->vec1);
			seq_printf(s, "#@#  vec2 = 0x%x\n", pdesc->vec2);
			seq_printf(s, "#@#  vec3 = 0x%x\n", pdesc->vec3);
			seq_printf(s, "#@#  vec4 = 0x%x\n", pdesc->vec4);
			seq_printf(s, "#@#  vec5 = 0x%x\n", pdesc->vec5);
			seq_printf(s, "#@#  vec6 = 0x%x\n", pdesc->vec6);
			seq_printf(s, "#@#  vec7 = 0x%x\n", pdesc->vec7);
			seq_printf(s, "#@#  vec8 = 0x%x\n", pdesc->vec8);
			seq_printf(s, "#@#  vec9 = 0x%x\n", pdesc->vec9);
			seq_printf(s, "#@#  vec10 = 0x%x\n", pdesc->vec10);
			seq_printf(s, "#@#  vec11 = 0x%x\n", pdesc->vec11);
			seq_printf(s, "#@#  vec12 = 0x%x\n", pdesc->vec12);
			seq_printf(s, "#@#  vec13 = 0x%x\n", pdesc->vec13);
			seq_printf(s, "#@#  vec14 = 0x%x\n", pdesc->vec14);
			seq_printf(s, "#@#  vec15 = 0x%x\n", pdesc->vec15);
		}
	}
	seq_puts(s, "\n\n");

	return 0;
}

static int spm_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, spm_dbg_show_firmware, &inode->i_private);
}

static const struct file_operations spm_debug_fops = {
	.open = spm_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int SPM_detect_open(struct inode *inode, struct file *file)
{
	pr_debug("open major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);
	spm_load_pcm_firmware_nodev();

	return 0;
}

static int SPM_detect_close(struct inode *inode, struct file *file)
{
	pr_debug("close major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	return 0;
}

static ssize_t SPM_detect_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	pr_debug(" ++\n");
	pr_debug(" --\n");

	return 0;
}

ssize_t SPM_detect_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	pr_debug(" ++\n");
	pr_debug(" --\n");

	return 0;
}

const struct file_operations gSPMDetectFops = {
	.open = SPM_detect_open,
	.release = SPM_detect_close,
	.read = SPM_detect_read,
	.write = SPM_detect_write,
};

#ifndef CONFIG_MTK_FPGA
static int spm_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	int i = 0;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
			if (spm_fw[i])
				release_firmware(spm_fw[i]);
		}
		dyna_load_pcm_done = 0;
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++)
			dyna_load_pcm[i].ready = 0;
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		dyna_load_pcm_done = 0;
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++)
			dyna_load_pcm[i].ready = 0;
		if (local_buf) {
			iounmap(local_buf);
			local_buf = NULL;
		}
		spm_load_pcm_firmware_nodev();

		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block spm_pm_notifier_func = {
	.notifier_call = spm_pm_event,
	.priority = 0,
};
#endif /* CONFIG_MTK_FPGA */

static ssize_t show_debug_log(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *p = buf;

	p += sprintf(p, "for test\n");

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t store_debug_log(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	return size;
}

static DEVICE_ATTR(debug_log, 0664, show_debug_log, store_debug_log);	/*664*/

static int spm_probe(struct platform_device *pdev)
{
	int ret;

	ret = device_create_file(&(pdev->dev), &dev_attr_debug_log);

	return 0;
}

static int spm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id spm_of_ids[] = {
	{.compatible = "mediatek,SLEEP",},
	{}
};

static struct platform_driver spm_dev_drv = {
	.probe = spm_probe,
	.remove = spm_remove,
	.driver = {
		   .name = "spm",
		   .owner = THIS_MODULE,
		   .of_match_table = spm_of_ids,
		   },
};

int spm_module_late_init(void)
{
	int i = 0;
	dev_t devID = MKDEV(gSPMDetectMajor, 0);
	int cdevErr = -1;
	int ret = -1;

	ret = platform_driver_register(&spm_dev_drv);
	if (ret) {
		pr_debug("fail to register platform driver\n");
		return ret;
	}

	pspmdev = platform_device_register_simple("spm", -1, NULL, 0);
	if (IS_ERR(pspmdev)) {
		pr_debug("Failed to register platform device.\n");
		return -EINVAL;
	}

	ret = register_chrdev_region(devID, SPM_DETECT_DEV_NUM, SPM_DETECT_DRVIER_NAME);
	if (ret) {
		pr_debug("fail to register chrdev\n");
		return ret;
	}

	cdev_init(&gSPMDetectCdev, &gSPMDetectFops);
	gSPMDetectCdev.owner = THIS_MODULE;

	cdevErr = cdev_add(&gSPMDetectCdev, devID, SPM_DETECT_DEV_NUM);
	if (cdevErr) {
		pr_debug("cdev_add() fails (%d)\n", cdevErr);
		goto err1;
	}

	pspmDetectClass = class_create(THIS_MODULE, SPM_DETECT_DEVICE_NAME);
	if (IS_ERR(pspmDetectClass)) {
		pr_debug("class create fail, error code(%ld)\n", PTR_ERR(pspmDetectClass));
		goto err1;
	}

	pspmDetectDev = device_create(pspmDetectClass, NULL, devID, NULL, SPM_DETECT_DEVICE_NAME);
	if (IS_ERR(pspmDetectDev)) {
		pr_debug("device create fail, error code(%ld)\n", PTR_ERR(pspmDetectDev));
		goto err2;
	}

	pr_debug("driver(major %d) installed success\n", gSPMDetectMajor);

	spm_dir = debugfs_create_dir("spm", NULL);
	if (spm_dir == NULL) {
		pr_debug("Failed to create spm dir in debugfs.\n");
		return -EINVAL;
	}

	spm_file = debugfs_create_file("firmware", S_IRUGO, spm_dir, NULL, &spm_debug_fops);

	for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++)
		dyna_load_pcm[i].ready = 0;

#ifndef CONFIG_MTK_FPGA
	ret = register_pm_notifier(&spm_pm_notifier_func);
	if (ret) {
		pr_debug("Failed to register PM notifier.\n");
		goto err2;
	}
#endif /* CONFIG_MTK_FPGA */

	return 0;

err2:

	if (pspmDetectClass) {
		class_destroy(pspmDetectClass);
		pspmDetectClass = NULL;
	}

err1:

	if (cdevErr == 0)
		cdev_del(&gSPMDetectCdev);

	if (ret == 0) {
		unregister_chrdev_region(devID, SPM_DETECT_DEV_NUM);
		gSPMDetectMajor = -1;
	}

	pr_debug("fail\n");

	return -1;

}
late_initcall(spm_module_late_init);
#endif /* ENABLE_DYNA_LOAD_PCM */

/**************************************
 * TWAM Control API
 **************************************/
static unsigned int idle_sel;
void spm_twam_set_idle_select(unsigned int sel)
{
	idle_sel = sel & 0x3;
}
EXPORT_SYMBOL(spm_twam_set_idle_select)

static unsigned int window_len;
void spm_twam_set_window_length(unsigned int len)
{
	window_len = len;
}
EXPORT_SYMBOL(spm_twam_set_window_length)

static struct twam_sig mon_type;
void spm_twam_set_mon_type(struct twam_sig *mon)
{
	if (mon) {
		mon_type.sig0 = mon->sig0 & 0x3;
		mon_type.sig1 = mon->sig1 & 0x3;
		mon_type.sig2 = mon->sig2 & 0x3;
		mon_type.sig3 = mon->sig3 & 0x3;
	}
}
EXPORT_SYMBOL(spm_twam_set_mon_type)

void spm_twam_register_handler(twam_handler_t handler)
{
	spm_twam_handler = handler;
}
EXPORT_SYMBOL(spm_twam_register_handler);

void spm_twam_enable_monitor(const struct twam_sig *twamsig, bool speed_mode)
{
	u32 sig0 = 0, sig1 = 0, sig2 = 0, sig3 = 0;
	u32 mon0 = 0, mon1 = 0, mon2 = 0, mon3 = 0;
	unsigned int sel;
	unsigned int length;
	unsigned long flags;

	if (twamsig) {
		sig0 = twamsig->sig0 & 0x1f;
		sig1 = twamsig->sig1 & 0x1f;
		sig2 = twamsig->sig2 & 0x1f;
		sig3 = twamsig->sig3 & 0x1f;
	}

	/* Idle selection */
	sel = idle_sel;
	/* Window length */
	length = window_len;
	/* Monitor type */
	mon0 = mon_type.sig0 & 0x3;
	mon1 = mon_type.sig1 & 0x3;
	mon2 = mon_type.sig2 & 0x3;
	mon3 = mon_type.sig3 & 0x3;

	spin_lock_irqsave(&__spm_lock, flags);
	/* FIXME: move to power mcu */
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) & ~ISRM_TWAM);
	/* Signal Select */
	spm_write(SPM_TWAM_IDLE_SEL, sel);
	/* Monitor Control */
	spm_write(SPM_TWAM_CON,
		  (sig3 << 27) |
		  (sig2 << 22) |
		  (sig1 << 17) |
		  (sig0 << 12) |
		  (mon3 << 10) |
		  (mon2 << 8) |
		  (mon1 << 6) |
		  (mon0 << 4) | (speed_mode ? REG_SPEED_MODE_EN_LSB : 0) | REG_TWAM_ENABLE_LSB);
	/* Window Length */
	/* 0x13DDF0 for 50ms, 0x65B8 for 1ms, 0x1458 for 200us, 0xA2C for 100us */
	/* in speed mode (26 MHz) */
	spm_write(SPM_TWAM_WINDOW_LEN, length);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("enable TWAM for signal %u, %u, %u, %u (%u)\n",
		  sig0, sig1, sig2, sig3, speed_mode);
}
EXPORT_SYMBOL(spm_twam_enable_monitor);

void spm_twam_disable_monitor(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	/* FIXME: move to power mcu */
	spm_write(SPM_TWAM_CON, spm_read(SPM_TWAM_CON) & ~REG_TWAM_ENABLE_LSB);
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_TWAM);
	spm_write(SPM_IRQ_STA, ISRC_TWAM);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("disable TWAM\n");
}
EXPORT_SYMBOL(spm_twam_disable_monitor);

/**************************************
 * SPM Golden Seting API(MEMPLL Control, DRAMC)
 **************************************/
struct ddrphy_golden_cfg {
	u32 addr;
	u32 value;
	u32 value1;
};

/* FIXME: golden setting */
#define DDRPHY_SETTING_UNUSED 0xffffffff
static struct ddrphy_golden_cfg ddrphy_setting[] = {
	{0x5c0, 0x21271b1b, DDRPHY_SETTING_UNUSED},
	{0x5c4, 0x5096001e, DDRPHY_SETTING_UNUSED},
	{0x5c8, 0x9010f010, DDRPHY_SETTING_UNUSED},
	{0x5cc, 0x50101010, DDRPHY_SETTING_UNUSED},
	{0x640, 0x000220b1, 0x00022091},
	{0x650, 0x00000018, DDRPHY_SETTING_UNUSED},
	{0x698, 0x00011e00, 0x00018030},
};

int spm_golden_setting_cmp(bool en)
{

	int i, ddrphy_num, r = 0;

	if (!en)
		return r;

	/*Compare Dramc Goldeing Setting */
	ddrphy_num = sizeof(ddrphy_setting) / sizeof(ddrphy_setting[0]);
	for (i = 0; i < ddrphy_num; i++) {
		if ((ucDram_Register_Read(ddrphy_setting[i].addr) != ddrphy_setting[i].value)
		    && ((ddrphy_setting[i].value1 == DDRPHY_SETTING_UNUSED)
			|| (ucDram_Register_Read(ddrphy_setting[i].addr) !=
			    ddrphy_setting[i].value1))) {
			spm_err("dramc setting mismatch addr: 0x%x, val: 0x%x\n",
				ddrphy_setting[i].addr,
				ucDram_Register_Read(ddrphy_setting[i].addr));
			r = -EPERM;
		}
	}

	return r;

}

/* FIXME: fix spm_pmic_power_mode() */
void spm_pmic_power_mode(int mode, int force, int lock)
{
	static int prev_mode = -1;

	if (mode < PMIC_PWR_NORMAL || mode >= PMIC_PWR_NUM) {
		pr_debug("wrong spm pmic power mode");
		return;
	}

	if (force == 0 && mode == prev_mode)
		return;

	switch (mode) {
	case PMIC_PWR_NORMAL:
		/* nothing */
		break;
	case PMIC_PWR_DEEPIDLE:
		break;
	case PMIC_PWR_SODI3:
		break;
	case PMIC_PWR_SODI:
		/* nothing */
		break;
	case PMIC_PWR_SUSPEND:
		mt_power_gs_dump_suspend();
		break;
	default:
		pr_debug("spm pmic power mode (%d) is not configured\n", mode);
	}

	prev_mode = mode;
}

u32 spm_get_register(void __force __iomem *offset)
{
	if (offset == SPM_PASR_DPD_0)
		return spm_read(offset);
	else
		return 0xdeadbeef;
}

void spm_set_register(void __force __iomem *offset, u32 value)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct spm_data spm_d;

	spm_d.u.args.arg0 = value;

	if (offset == SPM_PASR_DPD_0)
		spm_to_sspm_command(SPM_DPD_WRITE, &spm_d);
#else
	if (offset == SPM_PASR_DPD_0)
		spm_write(offset, value);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
}

void *mt_spm_base_get(void)
{
	return spm_base;
}
EXPORT_SYMBOL(mt_spm_base_get);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT

#include "sspm_ipi.h"

int spm_to_sspm_command(u32 cmd, struct spm_data *spm_d)
{
#define SPM_D_LEN		(11) /* # of cmd + arg0 + arg1 + ... */
#define SPM_VCOREFS_D_LEN	(4) /* # of cmd + arg0 + arg1 + ... */
	int ack_data;
	unsigned int ret = 0;
	/* struct spm_data _spm_d; */

	pr_debug("#@# %s(%d) cmd %x\n", __func__, __LINE__, cmd);
	switch (cmd) {
	case SPM_SUSPEND:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_RESUME:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_PWR_CTRL_SUSPEND:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_NOLOCK, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_DDR_SYNC:
		break;
	case SPM_REGISTER_INIT:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_DPD_WRITE:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_NOLOCK, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_IRQ0_HANDLER:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_AP_MDSRC_REQ:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_NOLOCK, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	/* TO: spm_dpidle_task */
	case SPM_DPIDLE_ENTER:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_DEEP_IDLE, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_DPIDLE_LEAVE:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_DEEP_IDLE, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_PWR_CTRL_DPIDLE:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_DEEP_IDLE, IPI_OPT_NOLOCK, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_ENTER_SODI:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SODI, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_LEAVE_SODI:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SODI, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_PWR_CTRL_SODI:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_SODI, IPI_OPT_NOLOCK, spm_d, SPM_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_VCORE_DVFS_ENTER:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_VCORE_DVFS, IPI_OPT_LOCK_POLLING, spm_d, SPM_VCOREFS_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_PWR_CTRL_MSDC:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_VCORE_DVFS, IPI_OPT_LOCK_POLLING, spm_d, SPM_VCOREFS_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_VCORE_PWARP_CMD:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_VCORE_DVFS, IPI_OPT_LOCK_POLLING, spm_d, SPM_VCOREFS_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_VCORE_DVFS_FWINIT:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_VCORE_DVFS, IPI_OPT_LOCK_POLLING, spm_d, SPM_VCOREFS_D_LEN, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) sspm_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;
	/***********************/
	default:
		pr_err("#@# %s(%d) cmd(%d) wrong!!!\n", __func__, __LINE__, cmd);
		break;
	}

	return ret;
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

MODULE_DESCRIPTION("SPM Driver v0.1");

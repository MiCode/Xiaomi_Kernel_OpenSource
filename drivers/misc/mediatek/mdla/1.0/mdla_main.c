/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include "mdla_debug.h"
#include <linux/semaphore.h>
#include <linux/completion.h>
#include "mdla.h"
#include "gsm.h"
#include "mdla_pmu.h"
#include "mdla_hw_reg.h"
#include "mdla_ioctl.h"
#include "mdla_ion.h"
#include "mdla_trace.h"
#include "mdla_dvfs.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/mman.h>
#include <linux/dmapool.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#define DRIVER_NAME "mtk_mdla"
#define DEVICE_NAME "mdlactl"
#define CLASS_NAME  "mdla"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chen, Wen");
MODULE_DESCRIPTION("MDLA driver");
MODULE_VERSION("0.1");

/* internal function prototypes */
static int mdla_open(struct inode *, struct file *);
static int mdla_release(struct inode *, struct file *);
static long mdla_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static long mdla_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg);
//static int mdla_wait_command_sync(struct ioctl_wait_cmd *wt);
static int mdla_mmap(struct file *filp, struct vm_area_struct *vma);
static int mdla_process_command(struct command_entry *ce);
//static void mdla_timeup(unsigned long data);
static void mdla_power_timeup(unsigned long data);
static void mdla_start_power_off(struct work_struct *work);

/* TODO: move these global control vaiables into device specific data.
 * to support multiple instance (multiple MDLA).
 */
#define MDLA_TIMEOUT_DEFAULT 6000 /* ms */
#define MDLA_POWEROFF_TIME_DEFAULT 2000 /* ms */
#define MDLA_POLLING_LATENCY (5) /* ms */
u32 mdla_timeout = MDLA_TIMEOUT_DEFAULT;
u32 mdla_e1_detect_timeout = MDLA_POLLING_LATENCY;
u32 mdla_e1_detect_count;
u32 mdla_poweroff_time = MDLA_POWEROFF_TIME_DEFAULT;
u32 async_cmd_id;

static void *infracfg_ao_top;
void *apu_conn_top;
void *apu_mdla_cmde_mreg_top;
void *apu_mdla_config_top;
void *apu_mdla_biu_top;
void *apu_mdla_gsm_top;
void *apu_mdla_gsm_base;

int mdla_irq;

//static DEFINE_TIMER(mdla_timer, mdla_timeup, 0, 0);
static DEFINE_TIMER(mdla_power_timer, mdla_power_timeup, 0, 0);

static int majorNumber;
static int numberOpens;
static struct class *mdlactlClass;
static struct device *mdlactlDevice;
static struct platform_device *mdlactlPlatformDevice;
static u32 cmd_id;
static u32 last_reset_id;
static u32 max_cmd_id;    /* latest cmd id from MREG_TOP_G_FIN0 */
static u32 sw_max_cmd_id; /* post-processed sw cmd id after fifo_out */
struct work_struct mdla_queue;
struct work_struct mdla_power_off_work;

struct completion command_done;

#define UINT32_MAX (0xFFFFFFFF)

static DEFINE_SPINLOCK(hw_lock);
DEFINE_MUTEX(power_lock);
DEFINE_MUTEX(cmd_lock);
DEFINE_MUTEX(cmd_list_lock);
static DECLARE_WAIT_QUEUE_HEAD(mdla_cmd_queue);
static LIST_HEAD(cmd_list);
static LIST_HEAD(cmd_fin_list);

u32 mdla_max_cmd_id(void)
{
	return max_cmd_id;
}

struct mtk_mdla_local {
	int irq;
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;
};

/* command entry functions */
static inline u32 ce_last_id(struct command_entry *ce)
{
	return (ce->id + ce->count - 1);
}


static const struct file_operations fops = {
	.open = mdla_open,
	.unlocked_ioctl = mdla_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdla_compat_ioctl,
#endif
	.mmap = mdla_mmap,
	.release = mdla_release,
};

static u32 reg_read(void *base, u32 offset)
{
	return ioread32(base + offset);
}

static void reg_write(void *base, u32 offset, u32 value)
{
	iowrite32(value, base + offset);
}

static void reg_set(void *base, u32 offset, u32 value)
{
	reg_write(base, offset, reg_read(base, offset) | value);
}

static void reg_clr(void *base, u32 offset, u32 value)
{
	reg_write(base, offset, reg_read(base, offset) & (~value));
}

unsigned int mdla_cfg_read(u32 offset)
{
	return ioread32(apu_mdla_config_top + offset);
}

static void mdla_cfg_write(u32 value, u32 offset)
{
	iowrite32(value, apu_mdla_config_top + offset);
}

#define mdla_cfg_set(mask, offset) \
	mdla_cfg_write(mdla_cfg_read(offset) | (mask), (offset))

unsigned int mdla_reg_read(u32 offset)
{
	return ioread32(apu_mdla_cmde_mreg_top + offset);
}

void mdla_reg_write(u32 value, u32 offset)
{
	iowrite32(value, apu_mdla_cmde_mreg_top + offset);
}


/* power on/off warpper function, protected by cmd_list_lock */
enum MDLA_POWER_STAT {
	PWR_OFF = 0,
	PWR_ON = 1,
};


static int mdla_power_status;

int get_power_on_status(void)
{
	return mdla_power_status == PWR_ON ? 1:0;
}

static int mdla_power_on(struct command_entry *ce)
{
	int ret = 0;

	mutex_lock(&power_lock);
	if (timer_pending(&mdla_power_timer))
		del_timer(&mdla_power_timer);

	if (mdla_power_status == PWR_ON) {
		mdla_cmd_debug("%s: already on.\n", __func__);
		goto power_on_done;
	}

	mdla_cmd_debug("%s:mdla_dvfs_cmd_start <ENTER>\n", __func__);
	ret = mdla_dvfs_cmd_start(ce);
	mdla_cmd_debug("%s:mdla_dvfs_cmd_start <EXIT>: %d\n",
		__func__, ret);
	if (ret)
		goto power_on_done;

	mdla_cmd_debug("%s: powered on\n", __func__);
	mdla_qos_counter_start(0);
	mdla_power_status = PWR_ON;

power_on_done:
	/* EARA QoS*/
	mdla_cmd_qos_start(0);
	mdla_profile_start();
	mutex_unlock(&power_lock);
	return ret;
}

static int mdla_power_off(const char *reason)
{
	int ret = 0;


	mutex_lock(&power_lock);

	if (mdla_power_status == PWR_OFF)
		goto power_off_done;

	pmu_reg_save();
	mdla_qos_counter_end(0);

	mdla_cmd_debug("%s: mdla_dvfs_cmd_end_shutdown <ENTER>: %s\n",
		__func__, reason);
	ret = mdla_dvfs_cmd_end_shutdown();
	mdla_cmd_debug("%s: mdla_dvfs_cmd_end_shutdown <EXIT>: %s, %d\n",
		__func__, reason, ret);

	if (ret)
		goto power_off_done;

	mdla_cmd_debug("%s: powered off\n", __func__);
	mdla_power_status = PWR_OFF;

power_off_done:
	mutex_unlock(&power_lock);

	return ret;
}

static const char *reason_str[REASON_MAX+1] = {
	"others",
	"driver_init",
	"command_timeout",
	"power_on",
	"-"
};

static const char *mdla_get_reason_str(int res)
{
	if ((res < 0) || (res > REASON_MAX))
		res = REASON_MAX;

	return reason_str[res];
}

static void mdla_reset(int res)
{
	const char *str = mdla_get_reason_str(res);

	pr_info("%s: MDLA RESET: %s(%d)\n", __func__,
		str, res);

	// Enable Bus prot, start to turn off, set bus protect - step 1:0
	reg_write(infracfg_ao_top,
		INFRA_TOPAXI_PROTECTEN_MCU_SET,
		VPU_CORE2_PROT_STEP1_0_MASK);

	while ((reg_read(infracfg_ao_top, INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
		VPU_CORE2_PROT_STEP1_0_ACK_MASK) !=
		VPU_CORE2_PROT_STEP1_0_ACK_MASK) {
	}

	// Reset
	reg_set(apu_conn_top, APU_CONN_SW_RST, APU_CORE2_RSTB);
	reg_clr(apu_conn_top, APU_CONN_SW_RST, APU_CORE2_RSTB);

	// Release Bus Prot
	reg_write(infracfg_ao_top, INFRA_TOPAXI_PROTECTEN_MCU_CLR,
		VPU_CORE2_PROT_STEP1_0_MASK);

	mdla_cfg_write(0xffffffff, MDLA_CG_CLR);
	mdla_reg_write(MDLA_IRQ_MASK & ~(MDLA_IRQ_SWCMD_DONE),
		MREG_TOP_G_INTP2);

	/* for DCM and CG */
	mdla_reg_write(cfg_eng0, MREG_TOP_ENG0);
	mdla_reg_write(cfg_eng1, MREG_TOP_ENG1);
	mdla_reg_write(cfg_eng2, MREG_TOP_ENG2);
	mdla_reg_write(cfg_eng11, MREG_TOP_ENG11);

#ifdef CONFIG_MTK_MDLA_ION
	mdla_cfg_set(MDLA_AXI_CTRL_MASK, MDLA_AXI_CTRL);
	mdla_cfg_set(MDLA_AXI_CTRL_MASK, MDLA_AXI1_CTRL);
#endif

	mdla_profile_reset(str);
}

void mdla_reset_lock(int res)
{
	unsigned long flags;

	spin_lock_irqsave(&hw_lock, flags);
	mdla_reset(res);

	if (res == REASON_TIMEOUT)
		last_reset_id = cmd_id;

	spin_unlock_irqrestore(&hw_lock, flags);
}

static void mdla_power_timeup(unsigned long data)
{
	schedule_work(&mdla_power_off_work);
}

static int mdla_mmap(struct file *filp, struct vm_area_struct *vma)
{

	unsigned long offset = vma->vm_pgoff;
	unsigned long size = vma->vm_end - vma->vm_start;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, offset, size,
			vma->vm_page_prot)) {
		pr_info("%s: remap_pfn_range error: %p\n",
			__func__, vma);
		return -EAGAIN;
	}
	return 0;
}


static void mdla_start_queue(struct work_struct *work)
{
	mutex_lock(&cmd_list_lock);
	mutex_unlock(&cmd_list_lock);

}
static void mdla_start_power_off(struct work_struct *work)
{
	mutex_lock(&cmd_lock);
	mdla_power_off("timer shutdown");
	mutex_unlock(&cmd_lock);
}

/* if there's no more reqeusts
 * 1. delete command timeout timer
 * 2. setup delay power off timer
 * this function is protected by cmd_list_lock
 */
static void mdla_command_done(void)
{
	mutex_lock(&power_lock);
	mdla_profile_stop(1);
	if (mdla_poweroff_time) {
		mdla_drv_debug("%s: start power_timer\n", __func__);
		mod_timer(&mdla_power_timer,
			jiffies + msecs_to_jiffies(mdla_poweroff_time));
	}
	mutex_unlock(&power_lock);
}

static irqreturn_t mdla_interrupt(int irq, void *dev_id)
{
	u32 status_int = mdla_reg_read(MREG_TOP_G_INTP0);
	unsigned long flags;
	u32 id;

	spin_lock_irqsave(&hw_lock, flags);
	id = mdla_reg_read(MREG_TOP_G_FIN0);
	pmu_reg_save();

	if (id > max_cmd_id) /* avoid max_cmd_id lost after timeout reset */
		max_cmd_id = id;

	if (status_int & MDLA_IRQ_PMU_INTE)
		mdla_reg_write(MDLA_IRQ_PMU_INTE, MREG_TOP_G_INTP0);

	spin_unlock_irqrestore(&hw_lock, flags);

	mdla_cmd_debug("%s: max_cmd_id: %d, id: %d\n",
		__func__, max_cmd_id, id);

	complete(&command_done);
	return IRQ_HANDLED;
}

static int mdla_resume(struct platform_device *pdev)
{
	mutex_unlock(&cmd_lock);
	mdla_cmd_debug("%s: resume\n", __func__);
	return 0;
}
static int mdla_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	mutex_lock(&cmd_lock);
	mdla_power_off("mdla suspend\n");
	return 0;
}
static int mdla_probe(struct platform_device *pdev)
{
	struct resource *r_irq; /* Interrupt resources */
	struct resource *apu_mdla_command; /* IO mem resources */
	struct resource *apu_mdla_config; /* IO mem resources */
	struct resource *apu_mdla_biu; /* IO mem resources */
	struct resource *apu_mdla_gsm; /* IO mem resources */
	struct resource *apu_conn; /* IO mem resources */
	struct resource *infracfg_ao; /* IO mem resources */
	int rc = 0;
	struct device *dev = &pdev->dev;

	mdlactlPlatformDevice = pdev;

	dev_info(dev, "Device Tree Probing\n");

	/* Get iospace for MDLA Config */
	apu_mdla_config = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!apu_mdla_config) {
		mdla_drv_debug("invalid address\n");
		return -ENODEV;
	}

	/* Get iospace for MDLA Command */
	apu_mdla_command = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!apu_mdla_command) {
		dev_err(dev, "invalid address\n");
		return -ENODEV;
	}
	/* Get iospace for MDAL PMU */
	apu_mdla_biu = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!apu_mdla_biu) {
		dev_err(dev, "apu_mdla_biu address\n");
		return -ENODEV;
	}

	/* Get iospace GSM */
	apu_mdla_gsm = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!apu_mdla_gsm) {
		dev_err(dev, "apu_mdla_biu address\n");
		return -ENODEV;
	}

	/* Get iospace APU CONN */
	apu_conn = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	if (!apu_conn) {
		mdla_drv_debug("apu_conn address\n");
		return -ENODEV;
	}

	/* Get INFRA CFG */
	infracfg_ao = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	if (!infracfg_ao) {
		mdla_drv_debug("infracfg_ao address\n");
		return -ENODEV;
	}

	apu_mdla_config_top = ioremap_nocache(apu_mdla_config->start,
			apu_mdla_config->end - apu_mdla_config->start + 1);
	if (!apu_mdla_config_top) {
		dev_err(dev, "mtk_mdla: Could not allocate iomem\n");
		rc = -EIO;
		return rc;
	}

	apu_mdla_cmde_mreg_top = ioremap_nocache(apu_mdla_command->start,
			apu_mdla_command->end - apu_mdla_command->start + 1);
	if (!apu_mdla_cmde_mreg_top) {
		dev_err(dev, "mtk_mdla: Could not allocate iomem\n");
		rc = -EIO;
		return rc;
	}

	apu_mdla_biu_top = ioremap_nocache(apu_mdla_biu->start,
			apu_mdla_biu->end - apu_mdla_biu->start + 1);
	if (!apu_mdla_biu_top) {
		dev_err(dev, "mtk_mdla: Could not allocate iomem\n");
		rc = -EIO;
		return rc;
	}

	apu_mdla_gsm_top = ioremap_nocache(apu_mdla_gsm->start,
			apu_mdla_gsm->end - apu_mdla_gsm->start + 1);
	if (!apu_mdla_gsm_top) {
		dev_err(dev, "mtk_mdla: Could not allocate iomem\n");
		rc = -EIO;
		return rc;
	}
	apu_mdla_gsm_base = (void *) apu_mdla_gsm->start;
	pr_info("%s: apu_mdla_gsm_top: %p, apu_mdla_gsm_base: %p\n",
		__func__, apu_mdla_gsm_top, apu_mdla_gsm_base);

	apu_conn_top = ioremap_nocache(apu_conn->start,
			apu_conn->end - apu_conn->start + 1);
	if (!apu_conn_top) {
		mdla_drv_debug("mtk_mdla: Could not allocate apu_conn_top\n");
		rc = -EIO;
		return rc;
	}

	infracfg_ao_top = ioremap_nocache(infracfg_ao->start,
			infracfg_ao->end - infracfg_ao->start + 1);
	if (!infracfg_ao_top) {
		mdla_drv_debug("mtk_mdla: Could not allocate infracfg_ao_top\n");
		rc = -EIO;
		return rc;
	}

	/* Get IRQ for the device */
	r_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!r_irq) {
		dev_info(dev, "no IRQ found\n");
		return rc;
	}
	dev_info(dev, "platform_get_resource irq: %d\n", (int)r_irq->start);

	mdla_irq = r_irq->start;
	rc = request_irq(mdla_irq, mdla_interrupt, IRQF_TRIGGER_LOW,
	DRIVER_NAME, dev);
	dev_info(dev, "request_irq\n");
	if (rc) {
		rc = request_irq(mdla_irq, mdla_interrupt, IRQF_TRIGGER_HIGH,
				DRIVER_NAME, dev);
		if (rc) {
			dev_err(dev, "mtk_mdla: Could not allocate interrupt %d.\n",
					mdla_irq);
			return rc;
		}
	}

	mdla_init_hw(0, pdev);

	dev_info(dev, "apu_mdla_config_top at 0x%08lx mapped to 0x%08lx\n",
			(unsigned long __force)apu_mdla_config->start,
			(unsigned long __force)apu_mdla_config->end);
	dev_info(dev, "apu_mdla_command at 0x%08lx mapped to 0x%08lx, irq=%d\n",
			(unsigned long __force)apu_mdla_command->start,
			(unsigned long __force)apu_mdla_command->end,
			(int)r_irq->start);
	dev_info(dev, "apu_mdla_biu_top at 0x%08lx mapped to 0x%08lx\n",
			(unsigned long __force)apu_mdla_biu->start,
			(unsigned long __force)apu_mdla_biu->end);
	dev_info(dev, "apu_conn_top at 0x%08lx mapped to 0x%08lx\n",
			(unsigned long __force)apu_conn->start,
			(unsigned long __force)apu_conn->end);
	dev_info(dev, "infracfg_ao_top at 0x%08lx mapped to 0x%08lx\n",
			(unsigned long __force)infracfg_ao->start,
			(unsigned long __force)infracfg_ao->end);

	mdla_ion_init();

#ifdef CONFIG_FPGA_EARLY_PORTING
	mdla_reset_lock(REASON_DRVINIT);
#endif

	return 0;

}

static int mdla_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	mdla_uninit_hw();
	free_irq(mdla_irq, dev);
	iounmap(infracfg_ao_top);
	iounmap(apu_conn_top);
	iounmap(apu_mdla_config_top);
	iounmap(apu_mdla_cmde_mreg_top);
	iounmap(apu_mdla_biu_top);
	iounmap(apu_mdla_gsm_top);
	mdla_ion_exit();
	platform_set_drvdata(pdev, NULL);

	return 0;
}
static const struct of_device_id mdla_of_match[] = {
	{ .compatible = "mediatek,mdla", },
	{ .compatible = "mtk,mdla", },
	{ /* end of list */},
};

MODULE_DEVICE_TABLE(of, mdla_of_match);
static struct platform_driver mdla_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mdla_of_match,
	},
	.probe = mdla_probe,
	.suspend = mdla_suspend,
	.resume = mdla_resume,
	.remove = mdla_remove,
};

static int mdlactl_init(void)
{
	int ret;

	ret = platform_driver_register(&mdla_driver);
	if (ret != 0)
		return ret;

	numberOpens = 0;
	cmd_id = 1;
	max_cmd_id = 0;
	last_reset_id = 0;
	sw_max_cmd_id = 0;
	mdla_power_status = 0;
	mdla_e1_detect_count = 0;
	async_cmd_id = 0;

	// Try to dynamically allocate a major number for the device
	//  more difficult but worth it
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	/* TODO: replace with alloc_chrdev_region() and
	 * unregister_chrdev_region()
	 * see examples at drivers/misc/mediatek/vpu/1.0/ vpu_reg_chardev,
	 * vpu_unreg_chardev
	 */

	if (majorNumber < 0) {
		pr_warn("MDLA failed to register a major number\n");
		return majorNumber;
	}
	mdla_drv_debug("MDLA: registered correctly with major number %d\n",
			majorNumber);

	// Register the device class
	mdlactlClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mdlactlClass)) {  // Check for error and clean up if there is
		unregister_chrdev(majorNumber, DEVICE_NAME);
		pr_warn("Failed to register device class\n");
		return PTR_ERR(mdlactlClass);
	}
	// Register the device driver
	mdlactlDevice = device_create(mdlactlClass, NULL, MKDEV(majorNumber, 0),
	NULL, DEVICE_NAME);
	if (IS_ERR(mdlactlDevice)) {  // Clean up if there is an error
		unregister_chrdev(majorNumber, DEVICE_NAME);
		pr_warn("Failed to create the device\n");
		return PTR_ERR(mdlactlDevice);
	}

	// Init DMA from of
	of_dma_configure(mdlactlDevice, NULL);

	// Set DMA mask
	if (dma_get_mask(mdlactlDevice) != DMA_BIT_MASK(32)) {
		ret = dma_set_mask_and_coherent(mdlactlDevice,
					DMA_BIT_MASK(32));
		if (ret)
			pr_warn("MDLA: set DMA mask failed: %d\n", ret);
	}

	INIT_WORK(&mdla_queue, mdla_start_queue);
	INIT_WORK(&mdla_power_off_work, mdla_start_power_off);
	init_completion(&command_done);

	mdla_debugfs_init();
	mdla_profile_init();
	pmu_init();
	mdla_qos_counter_init();

	return 0;
}

static int mdla_open(struct inode *inodep, struct file *filep)
{
	numberOpens++;
	mdla_drv_debug("MDLA: Device has been opened %d time(s)\n",
		numberOpens);

	return 0;
}

static int mdla_release(struct inode *inodep, struct file *filep)
{
	mdla_drv_debug("MDLA: Device successfully closed\n");

	return 0;
}


static int mdla_process_command(struct command_entry *ce)
{
	dma_addr_t addr;
	u32 count;
	int ret = 0;
	unsigned long flags;

	mdla_timeout_debug("mdla_run\n");

	if (ce == NULL) {
		mdla_cmd_debug("%s: invalid command entry: ce=%p\n",
			__func__, ce);
		return 0;
	}

	addr = ce->mva;
	count = ce->count;

	mdla_cmd_debug("%s: count: %d, addr: %lx\n",
		__func__, ce->count,
		(unsigned long)addr);

	if (ret)
		return ret;

	/* Issue command */
	spin_lock_irqsave(&hw_lock, flags);
	ce->state = CE_RUN;
	mdla_reg_write(addr, MREG_TOP_G_CDMA1);
	mdla_reg_write(count, MREG_TOP_G_CDMA2);
	mdla_reg_write(count, MREG_TOP_G_CDMA3);
	spin_unlock_irqrestore(&hw_lock, flags);

//	list_add_tail(&ce->list, &cmd_fin_list);

	return ret;
}

static void
mdla_run_command_prepare(struct ioctl_run_cmd *cd, struct command_entry *ce)
{
	if (!ce)
		return;

	ce->mva = cd->buf.mva + cd->offset;
	mdla_cmd_debug("%s: mva=%08x, offset=%08x, count: %u, priority: %u, boost: %u\n",
			__func__,
			cd->buf.mva,
			cd->offset,
			cd->count,
			cd->priority,
			cd->boost_value);

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_CMD_SUCCESS;
	ce->count = cd->count;
	ce->khandle = cd->buf.ion_khandle;
	ce->type = cd->buf.type;
	ce->priority = cd->priority;
	ce->boost_value = cd->boost_value;
	ce->receive_t = sched_clock();
	ce->kva = NULL;
}


static void mdla_performance_index(struct ioctl_wait_cmd *wt,
		struct command_entry *ce)
{
	wt->queue_time = ce->poweron_t - ce->receive_t;
	wt->busy_time = ce->wait_t - ce->poweron_t;
	wt->bandwidth = ce->bandwidth;
}

static int hw_e1_timeout_detect(void)
{
	u32 ste_debug_if_1;

	ste_debug_if_1 = mdla_reg_read(0x0EA8);
	if (((ste_debug_if_1&0x1C0) != 0x0 &&
			(ste_debug_if_1&0x3) == 0x3)) {
		mdla_timeout_debug("%s: match E1 timeout issue\n",
				__func__);
		mdla_e1_detect_count++;
		return -1;
	}
	return 0;
}

static int mdla_run_command_sync(struct ioctl_run_cmd *cd,
	struct ioctl_wait_cmd *wt)
{
	int ret = 0;
	struct command_entry ce;
	u32 id;
	u64 deadline;

	ce.queue_t = sched_clock();

	/* The critical region of command enqueue */
	mutex_lock(&cmd_lock);

	mdla_run_command_prepare(cd, &ce);

	/* Trace start */
	mdla_trace_begin(&ce);

process_command:
	/* Compute deadline */
	deadline = get_jiffies_64() + msecs_to_jiffies(mdla_timeout);

	max_cmd_id = 0;
	id = ce.count;

	mdla_timeout_debug("%s: max_cmd_id: %d id: %d\n",
			__func__, max_cmd_id, id);

	mdla_power_on(&ce);
	ce.poweron_t = sched_clock();
	ce.req_start_t = sched_clock();

	/* Fill HW reg */
	mdla_process_command(&ce);

	/* Wait for timeout */
	while (max_cmd_id < id && time_before64(get_jiffies_64(), deadline)) {
		unsigned long wait_event_timeouts;

		if (cfg_timer_en)
			wait_event_timeouts =
				usecs_to_jiffies(cfg_period);
		else
			wait_event_timeouts =
				msecs_to_jiffies(mdla_e1_detect_timeout);

		wait_for_completion_interruptible_timeout(&command_done,
				wait_event_timeouts);

		/* E1 HW timeout check here */
		if (hw_e1_timeout_detect() != 0) {
			mdla_reset_lock(REASON_TIMEOUT);
			goto process_command;
		}
	}
	ce.req_end_t = sched_clock();
	mdla_trace_iter();

	wt->id = id;

	if (max_cmd_id >= id)
		wt->result = 0;
	else { // Command timeout
		mdla_timeout_debug("%s: command: %u, max_cmd_id: %u deadline:%llu, jiffies: %lu\n",
				__func__, id, max_cmd_id, deadline, jiffies);
		mdla_dump_reg();
		mdla_dump_ce(&ce);
		mdla_reset_lock(REASON_TIMEOUT);
		wt->result = 1;
	}

	/* Start power off timer */
	mdla_command_done();
	ce.wait_t = sched_clock();

	/* Trace stop */
	mdla_trace_end(&ce);

	mutex_unlock(&cmd_lock);

	/* Calculate all performance index */
	mdla_performance_index(wt, &ce);


	return ret;
}

void mdla_wait_command(struct ioctl_wait_cmd *wt)
{
	struct list_head *ele, *next;
	struct wait_entry *we;

	wt->result = -1;
	mdla_cmd_debug("%s: id: %u\n", __func__, wt->id);
	mutex_lock(&cmd_list_lock);
	list_for_each_safe(ele, next, &cmd_list) {
		mdla_cmd_debug("%s: loop id: %u\n", __func__, wt->id);
		we = list_entry(ele, struct wait_entry, list);
		if (wt->id == we->async_id) {
			mdla_cmd_debug("%s: found id: %u\n", __func__, wt->id);
			memcpy(wt, &we->wt, sizeof(struct ioctl_wait_cmd));
			list_del(&we->list);
			kfree(we);
			break;
		}
	}
	mutex_unlock(&cmd_list_lock);
}

static int mdla_run_command_async(struct ioctl_run_cmd *cd)
{
	struct wait_entry *we = kmalloc(sizeof(struct wait_entry),
			GFP_KERNEL);

	if (we == NULL)
		return -1;

	mdla_run_command_sync(cd, &we->wt);
	if (we->wt.result != 0) {
		kfree(we);
		return -1;
	}
	mutex_lock(&cmd_list_lock);
	we->async_id = async_cmd_id++;
	list_add_tail(&we->list, &cmd_list);
	mutex_unlock(&cmd_list_lock);
	mdla_cmd_debug("%s: %d\n", __func__, we->async_id);
	return we->async_id;
}

#define MAX_ALLOC_SIZE (128 * 1024 * 1024)
static int mdla_dram_alloc(struct ioctl_malloc *malloc_data)
{
	dma_addr_t phyaddr = 0;

	if (malloc_data->size > MAX_ALLOC_SIZE)
		malloc_data->size = MAX_ALLOC_SIZE;

	malloc_data->kva = dma_alloc_coherent(mdlactlDevice, malloc_data->size,
			&phyaddr, GFP_KERNEL);
	malloc_data->pa = (void *)dma_to_phys(mdlactlDevice, phyaddr);
	malloc_data->mva = (__u32)((long) malloc_data->pa);

	mdla_mem_debug("%s: kva:%p, mva:%x\n",
		__func__, malloc_data->kva, malloc_data->mva);

	return (malloc_data->kva) ? 0 : -ENOMEM;
}

static void mdla_dram_free(struct ioctl_malloc *malloc_data)
{
	mdla_mem_debug("%s: kva:%p, mva:%x\n",
		__func__, malloc_data->kva, malloc_data->mva);
	dma_free_coherent(mdlactlDevice, malloc_data->size,
			(void *) malloc_data->kva, malloc_data->mva);
}

static long mdla_ioctl_config(unsigned long arg)
{
	long retval = 0;
	struct ioctl_config cfg;

	if (copy_from_user(&cfg, (void *) arg, sizeof(cfg)))
		return -EFAULT;

	switch (cfg.op) {
	case MDLA_CFG_NONE:
		break;
	case MDLA_CFG_TIMEOUT_GET:
		cfg.arg[0] = mdla_timeout;
		cfg.arg_count = 1;
		break;
	case MDLA_CFG_TIMEOUT_SET:
		if (cfg.arg_count == 1)
			mdla_timeout = cfg.arg[0];
		break;
	case MDLA_CFG_FIFO_SZ_GET:
		cfg.arg[0] = 1;
		cfg.arg_count = 1;
		break;
	case MDLA_CFG_FIFO_SZ_SET:
		return -EINVAL;
	case MDLA_CFG_GSM_INFO:
		cfg.arg[0] = GSM_SIZE;
		cfg.arg[1] = GSM_MVA_BASE;
		cfg.arg[2] = (unsigned long) apu_mdla_gsm_base;
		cfg.arg[3] = (unsigned long) apu_mdla_gsm_top;
		cfg.arg_count = 4;
		break;
	default:
		return -EINVAL;
	}

	if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
		return -EFAULT;

	return retval;
}

static long mdla_ioctl(struct file *filp, unsigned int command,
		unsigned long arg)
{
	long retval = 0;
	struct ioctl_malloc malloc_data;
	struct ioctl_perf perf_data;

	switch (command) {
	case IOCTL_MALLOC:
		if (copy_from_user(&malloc_data, (void *) arg,
			sizeof(malloc_data))) {
			return -EFAULT;
		}
		if (malloc_data.type == MEM_DRAM)
			retval = mdla_dram_alloc(&malloc_data);
		else if (malloc_data.type == MEM_GSM)
			retval = mdla_gsm_alloc(&malloc_data);
		else
			return -EINVAL;

		if (copy_to_user((void *) arg, &malloc_data,
			sizeof(malloc_data)))
			return -EFAULT;
		mdla_mem_debug("%s: IOCTL_MALLOC: size:0x%x mva:0x%x kva:%p pa:%p type:%d\n",
			__func__,
			malloc_data.size,
			malloc_data.mva,
			malloc_data.kva,
			malloc_data.pa,
			malloc_data.type);
		if (malloc_data.kva == NULL)
			return -ENOMEM;
		break;
	case IOCTL_FREE:
		if (copy_from_user(&malloc_data, (void *) arg,
				sizeof(malloc_data))) {
			return -EFAULT;
		}
		if (malloc_data.type == MEM_DRAM)
			mdla_dram_free(&malloc_data);
		else if (malloc_data.type == MEM_GSM)
			mdla_gsm_free(&malloc_data);
		else
			return -EINVAL;
		mdla_mem_debug("%s: IOCTL_MALLOC: size:0x%x mva:0x%x kva:%p pa:%p type:%d\n",
			__func__,
			malloc_data.size,
			malloc_data.mva,
			malloc_data.kva,
			malloc_data.pa,
			malloc_data.type);
		break;
	case IOCTL_RUN_CMD_SYNC:
	{
		struct ioctl_run_cmd_sync cmd_data;

		if (copy_from_user(&cmd_data, (void *) arg,
				sizeof(cmd_data))) {
			return -EFAULT;
		}
		mdla_cmd_debug("%s: RUN_CMD_SYNC: kva=%p, mva=0x%08x, phys_to_virt=%p\n",
			__func__,
			(void *)cmd_data.req.buf.kva,
			cmd_data.req.buf.mva,
			phys_to_virt(cmd_data.req.buf.mva));
		retval = mdla_run_command_sync(&cmd_data.req, &cmd_data.res);
		if (copy_to_user((void *) arg, &cmd_data, sizeof(cmd_data)))
			return -EFAULT;
		break;
	}
	case IOCTL_RUN_CMD_ASYNC:
	{
		struct ioctl_run_cmd cmd_data;

		if (copy_from_user(&cmd_data, (void *) arg,
				sizeof(cmd_data))) {
			return -EFAULT;
		}
		cmd_data.id = mdla_run_command_async(&cmd_data);
		if (copy_to_user((void *) arg, &cmd_data,
				sizeof(cmd_data)))
			return -EFAULT;
		break;
	}
	case IOCTL_WAIT_CMD:
	{
		struct ioctl_wait_cmd wait_data;

		if (copy_from_user(&wait_data, (void *) arg,
			sizeof(struct ioctl_wait_cmd))) {
			return -EFAULT;
		}
		mdla_wait_command(&wait_data);
		if (copy_to_user((void *) arg, &wait_data, sizeof(wait_data)))
			return -EFAULT;
		break;

	}
	case IOCTL_PERF_SET_EVENT:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		perf_data.handle = pmu_counter_alloc(
			perf_data.interface, perf_data.event);
		if (copy_to_user((void *) arg, &perf_data, sizeof(perf_data)))
			return -EFAULT;
		break;
	case IOCTL_PERF_GET_EVENT:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		perf_data.event = pmu_counter_event_get(perf_data.handle);
		if (copy_to_user((void *) arg, &perf_data, sizeof(perf_data)))
			return -EFAULT;
		break;
	case IOCTL_PERF_GET_CNT:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		perf_data.counter = pmu_counter_get(perf_data.handle);

		if (copy_to_user((void *) arg, &perf_data, sizeof(perf_data)))
			return -EFAULT;
		break;
	case IOCTL_PERF_UNSET_EVENT:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		pmu_counter_free(perf_data.handle);

		break;
	case IOCTL_PERF_GET_START:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		perf_data.start = pmu_get_perf_start();
		if (copy_to_user((void *) arg, &perf_data, sizeof(perf_data)))
			return -EFAULT;
		break;
	case IOCTL_PERF_GET_END:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		perf_data.end = pmu_get_perf_end();
		if (copy_to_user((void *) arg, &perf_data, sizeof(perf_data)))
			return -EFAULT;
		break;
	case IOCTL_PERF_GET_CYCLE:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		perf_data.start = pmu_get_perf_cycle();
		if (copy_to_user((void *) arg, &perf_data, sizeof(perf_data)))
			return -EFAULT;
		break;
	case IOCTL_PERF_RESET_CNT:
		mutex_lock(&cmd_lock);
		mutex_lock(&power_lock);

		pmu_reset_saved_counter();

		mutex_unlock(&power_lock);
		mutex_unlock(&cmd_lock);
		break;
	case IOCTL_PERF_RESET_CYCLE:
		mutex_lock(&cmd_lock);
		mutex_lock(&power_lock);

		pmu_reset_saved_cycle();

		mutex_unlock(&power_lock);
		mutex_unlock(&cmd_lock);
		break;
	case IOCTL_PERF_SET_MODE:
		if (copy_from_user(&perf_data, (void *) arg,
				sizeof(perf_data))) {
			return -EFAULT;
		}
		mutex_lock(&cmd_lock);
		mutex_lock(&power_lock);

		pmu_clr_mode_save(perf_data.mode);

		mutex_unlock(&power_lock);
		mutex_unlock(&cmd_lock);
		break;
	case IOCTL_ION_KMAP:
		return mdla_ion_kmap(arg);
	case IOCTL_ION_KUNMAP:
		return mdla_ion_kunmap(arg);
	case IOCTL_CONFIG:
		return mdla_ioctl_config(arg);
	default:
		if (command >= MDLA_DVFS_IOCTL_START &&
			command <= MDLA_DVFS_IOCTL_END)
			return mdla_dvfs_ioctl(filp, command, arg);
		else
			return -EINVAL;
	}
	return retval;
}

#ifdef CONFIG_COMPAT
static long mdla_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return mdla_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static void mdlactl_exit(void)
{
	mdla_qos_counter_destroy();
	mdla_debugfs_exit();
	platform_driver_unregister(&mdla_driver);
	device_destroy(mdlactlClass, MKDEV(majorNumber, 0));
	class_destroy(mdlactlClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	mdla_drv_debug("MDLA: Goodbye from the LKM!\n");
}

late_initcall(mdlactl_init)
module_exit(mdlactl_exit);


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

#include <linux/semaphore.h>
#include <linux/completion.h>
#include "mdla.h"
#include "gsm.h"
#include "mdla_ioctl.h"
#include "mdla_ion.h"
#include "mdla_trace.h"
#include "mdla_debug.h"
#include "mdla_plat_api.h"
#include "mdla_util.h"
#include "mdla_power_ctrl.h"
#include "mdla_cmd_proc.h"
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
#include "../../include/apusys_device.h"
#endif

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

#ifdef CONFIG_OF
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#endif

#define DEVICE_NAME "mdlactl"
#define CLASS_NAME  "mdla"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SS5");
MODULE_DESCRIPTION("MDLA driver");
MODULE_VERSION("2.0");

/* internal function prototypes */
static int mdla_open(struct inode *, struct file *);
static int mdla_release(struct inode *, struct file *);
#ifdef __APUSYS_MDLA_UT__
static long mdla_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static long mdla_compat_ioctl(struct file *file,
unsigned int cmd, unsigned long arg);
static int mdla_mmap(struct file *filp, struct vm_area_struct *vma);
#endif
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
int apusys_mdla_handler(APUSYS_DEVICE_CMD_E type,
		void *hnd, struct apusys_device *dev);
#endif
static irqreturn_t mdla_irq0_handler(int irq, void *dev_id);
static irqreturn_t mdla_irq1_handler(int irq, void *dev_id);


/* TODO: move these global control vaiables into device specific data.
 * to support multiple instance (multiple MDLA).
 */
#define MDLA_TIMEOUT_DEFAULT 6000 /* ms */
#define MDLA_POWEROFF_TIME_DEFAULT 2000 /* ms */
#define MDLA_POLLING_LATENCY (5) /* ms */


u32 mdla_timeout = MDLA_TIMEOUT_DEFAULT;
u32 mdla_e1_detect_timeout = MDLA_POLLING_LATENCY;
u32 mdla_poweroff_time = MDLA_POWEROFF_TIME_DEFAULT;

/*MDLA Multi-Core or per command info*/
struct mdla_dev mdla_devices[] = {
	{
		.mdlaid = 0,
		.mdla_e1_detect_count = 0,
		.async_cmd_id = 0,
		.cmd_lock = __MUTEX_INITIALIZER(mdla_devices[0].cmd_lock),
		.cmd_list_lock =
			__MUTEX_INITIALIZER(mdla_devices[0].cmd_list_lock),
		.power_pdn_work = mdla0_start_power_off,
		.power_lock =  __MUTEX_INITIALIZER(mdla_devices[0].power_lock),

	},
	{
		.mdlaid = 1,
		.mdla_e1_detect_count = 0,
		.async_cmd_id = 0,
		.cmd_lock = __MUTEX_INITIALIZER(mdla_devices[1].cmd_lock),
		.cmd_list_lock =
			__MUTEX_INITIALIZER(mdla_devices[1].cmd_list_lock),
		.power_pdn_work = mdla1_start_power_off,
		.power_lock =  __MUTEX_INITIALIZER(mdla_devices[1].power_lock),
	},
};
struct mdla_irq_desc mdla_irqdesc[] = {
	{.irq = 0, .handler = mdla_irq0_handler,},
	{.irq = 0, .handler = mdla_irq1_handler,}
};

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
struct apusys_device apusys_mdla_dev[] = {
	{
		.dev_type = APUSYS_DEVICE_MDLA,
		.preempt_type = APUSYS_PREEMPT_NONE,
		.preempt_level = 0,
		.private = 0,
		.send_cmd =  0,

	},
	{
		.dev_type = APUSYS_DEVICE_MDLA,
		.preempt_type = APUSYS_PREEMPT_NONE,
		.preempt_level = 0,
		.private = 0,
		.send_cmd =  0,
	},
};
#endif

u32 mdla_max_num_core = MTK_MDLA_MAX_NUM;//TODO: core num get from DTS

struct mdla_reg_ctl mdla_reg_control[] = {
	{
		.apu_mdla_cmde_mreg_top = 0,
		.apu_mdla_biu_top = 0,
		.apu_mdla_config_top = 0,
	},
	{
		.apu_mdla_cmde_mreg_top = 0,
		.apu_mdla_biu_top = 0,
		.apu_mdla_config_top = 0,
	}
};
//void *infracfg_ao_top;//bus protect, only for mt6779
void *apu_conn_top;
void *apu_mdla_gsm_top;
void *apu_mdla_gsm_base;

static int majorNumber;
static int numberOpens;
static struct class *mdlactlClass;
static struct device *mdlactlDevice;
static u32 cmd_id;


static LIST_HEAD(cmd_list);

static const struct file_operations fops = {
	.open = mdla_open,
#ifdef __APUSYS_MDLA_UT__
	.unlocked_ioctl = mdla_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdla_compat_ioctl,
#endif
	.mmap = mdla_mmap,
#endif
	.release = mdla_release,
};


void mdla_reset_lock(int core, int res)
{
	unsigned long flags;

	spin_lock_irqsave(&mdla_devices[core].hw_lock, flags);
	mdla_reset(core, res);

	if (res == REASON_TIMEOUT)
		mdla_devices[core].last_reset_id = cmd_id;

	spin_unlock_irqrestore(&mdla_devices[core].hw_lock, flags);
}

#ifdef __APUSYS_MDLA_UT__
static int mdla_mmap(struct file *filp, struct vm_area_struct *vma)
{

	unsigned long offset = vma->vm_pgoff;
	unsigned long size = vma->vm_end - vma->vm_start;
	/*MDLA early verification*/
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, offset, size,
			vma->vm_page_prot)) {
		pr_info("%s: remap_pfn_range error: %p\n",
			__func__, vma);
		return -EAGAIN;
	}
	return 0;
}
#endif

static irqreturn_t mdla_irq0_handler(int irq, void *dev_id)
{
#ifndef __APUSYS_PREEMPTION__
	return mdla_interrupt(0);
#else
	return mdla_scheduler(0);
#endif
}

static irqreturn_t mdla_irq1_handler(int irq, void *dev_id)
{
#ifndef __APUSYS_PREEMPTION__
	return mdla_interrupt(1);
#else
	return mdla_scheduler(1);
#endif
}

#ifdef __APUSYS_PREEMPTION__
static int mdla_scheduler_init(struct device *dev)
{
	struct mdla_scheduler *scheduler;
	size_t i;

	for (i = 0; i < mdla_max_num_core; i++) {
		mdla_devices[i].scheduler = devm_kzalloc(dev,
							 sizeof(*scheduler),
							 GFP_KERNEL);
		scheduler = mdla_devices[i].scheduler;
		if (!scheduler)
			return -ENOMEM;

		spin_lock_init(&scheduler->lock);
		INIT_LIST_HEAD(&scheduler->completed_ce_queue);
		INIT_LIST_HEAD(&scheduler->ce_queue);

		scheduler->processing_ce = NULL;
		scheduler->enqueue_ce = mdla_enqueue_ce;
		scheduler->dequeue_ce = mdla_dequeue_ce;
		scheduler->process_ce = mdla_process_ce;
		scheduler->issue_ce = mdla_issue_ce;
		scheduler->complete_ce = mdla_complete_ce;
		scheduler->preempt_ce = mdla_preempt_ce;
		scheduler->all_ce_done = mdla_command_done;
	}
}
#endif

static int mdla_sw_multi_devices_init(void)
{
	int i;

	for (i = 0; i < mdla_max_num_core; i++) {
		mdla_devices[i].mdla_e1_detect_count = 0;
		mdla_devices[i].async_cmd_id = 0;
		mdla_devices[i].max_cmd_id = 0;
		mdla_devices[i].last_reset_id = 0;
		init_completion(&mdla_devices[i].command_done);
		spin_lock_init(&mdla_devices[i].hw_lock);
		mdla_devices[i].power_timer.data = i;
		mdla_devices[i].power_timer.function =
				mdla_power_timeup;
		init_timer(&mdla_devices[i].power_timer);
		INIT_WORK(&mdla_devices[i].power_off_work,
				mdla_devices[i].power_pdn_work);
		mdla_devices[i].mdla_power_status = 0;
	}
	return 0;
}
static int mdla_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	struct apusys_device *apusys_mdla_ptr = NULL;
	struct mdla_dev *apusys_mdla_dev_ptr = NULL;
#endif
#if defined(CONFIG_FPGA_EARLY_PORTING)
	int i;
#endif

	if (mdla_dts_map(pdev)) {
		dev_info(dev, "%s: failed due to DTS failed\n", __func__);
		return -EINVAL;
	}

	if (mdla_register_power(pdev)) {
		dev_info(dev, "register mdla power fail\n");
		return -EINVAL;
	}

#ifdef CONFIG_MTK_MDLA_ION
	mdla_ion_init();
#endif

#if defined(CONFIG_FPGA_EARLY_PORTING)
	for (i = 0; i < mdla_max_num_core; i++)
		mdla_reset_lock(i, REASON_DRVINIT);//TODO consider multi core
#endif

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	apusys_mdla_ptr = apusys_mdla_dev;
	apusys_mdla_dev_ptr = mdla_devices;

	for (i = 0; i < mdla_max_num_core; i++) {
		apusys_mdla_dev[i].private = apusys_mdla_dev_ptr;
		apusys_mdla_dev[i].send_cmd = apusys_mdla_handler;
		if (apusys_register_device(apusys_mdla_ptr)) {
			dev_info(dev, "register apusys mdla %d info\n", i);
			return -EINVAL;
		}
		apusys_mdla_ptr++;
		apusys_mdla_dev_ptr++;
	}
#endif

#ifdef __APUSYS_PREEMPTION__
	if (mdla_scheduler_init(&pdev->dev) < 0)
		return -ENOMEM;
#endif

	return 0;

}

static int mdla_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	int i;

	mdla_drv_debug("%s start -\n", __func__);

	for (i = 0; i < mdla_max_num_core; i++)
		mdla_start_power_off(i);

	if (mdla_unregister_power(pdev)) {
		dev_info(dev, "unregister mdla power fail\n");
		return -EINVAL;
	}

	mdla_drv_debug("%s unregister power -\n", __func__);

	//iounmap(infracfg_ao_top);
	iounmap(apu_conn_top);
	iounmap(apu_mdla_gsm_top);

	for (i = 0; i < mdla_max_num_core; i++) {
		free_irq(mdla_irqdesc[i].irq, dev);
		iounmap(mdla_reg_control[i].apu_mdla_config_top);
		iounmap(mdla_reg_control[i].apu_mdla_cmde_mreg_top);
		iounmap(mdla_reg_control[i].apu_mdla_biu_top);
	}

#ifdef CONFIG_MTK_MDLA_ION
	mdla_ion_exit();
#endif
	platform_set_drvdata(pdev, NULL);

	mdla_drv_debug("%s done -\n", __func__);

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
#ifndef __APUSYS_MIDDLEWARE__
	.suspend = mdla_suspend,
	.resume = mdla_resume,
#endif
	.remove = mdla_remove,
};

static int mdlactl_init(void)
{
	int ret;

	mdla_sw_multi_devices_init();

	ret = platform_driver_register(&mdla_driver);
	if (ret != 0)
		return ret;

	numberOpens = 0;
	cmd_id = 1;

	// Try to dynamically allocate a major number for the device
	//  more difficult but worth it
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	/* TODO: replace with alloc_chrdev_region() and
	 * unregister_chrdev_region()
	 * see examples at drivers/misc/mediatek/vpu/1.0/ vpu_reg_chardev,
	 * vpu_unreg_chardev
	 */

	if (majorNumber < 0) {
		mdla_drv_debug("MDLA failed to register a major number\n");
		return majorNumber;
	}
	mdla_drv_debug("MDLA: registered correctly with major number %d\n",
			majorNumber);

	// Register the device class
	mdlactlClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mdlactlClass)) {  // Check for error and clean up if there is
		unregister_chrdev(majorNumber, DEVICE_NAME);
		mdla_drv_debug("Failed to register device class\n");
		return PTR_ERR(mdlactlClass);
	}
	// Register the device driver
	mdlactlDevice = device_create(mdlactlClass, NULL, MKDEV(majorNumber, 0),
	NULL, DEVICE_NAME);
	if (IS_ERR(mdlactlDevice)) {  // Clean up if there is an error
		unregister_chrdev(majorNumber, DEVICE_NAME);
		mdla_drv_debug("Failed to create the device\n");
		return PTR_ERR(mdlactlDevice);
	}

	// Init DMA from of
	of_dma_configure(mdlactlDevice, NULL);

	// Set DMA mask
	if (dma_get_mask(mdlactlDevice) != DMA_BIT_MASK(32)) {
		ret = dma_set_mask_and_coherent(mdlactlDevice,
					DMA_BIT_MASK(32));
		if (ret)
			mdla_drv_debug("MDLA: set DMA mask failed: %d\n", ret);
	}

#if 0
	/* 32-bit will get dummy dma ops, -1 denote no matters */
	arch_setup_dma_ops(mdlactlDevice, -1, -1, NULL, 0);
#endif

	mdla_debugfs_init();
	mdla_profile_init();
	pmu_init();
	mdla_drv_debug("%s done!\n", __func__);

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
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
int apusys_mdla_handler(APUSYS_DEVICE_CMD_E type,
	void *hnd, struct apusys_device *dev)//struct apusys_run_cmd *acd)
{
	long retval = 0;
	struct apusys_cmd_hnd *cmd_hnd = hnd;
	struct mdla_dev *mdla_info = (struct mdla_dev *)dev->private;

	if (dev->dev_type != APUSYS_DEVICE_MDLA)
		return -EINVAL;

	switch (type) {
	case APUSYS_CMD_POWERON:
		mdla_pwr_on(mdla_info->mdlaid);
		break;
	case APUSYS_CMD_POWERDOWN:
		mdla_start_power_off(mdla_info->mdlaid);
		break;
	case APUSYS_CMD_RESUME:
		mutex_unlock(&mdla_info->cmd_lock);
		break;
	case APUSYS_CMD_SUSPEND:
		mutex_lock(&mdla_info->cmd_lock);
		mdla_pwr_off(mdla_info->mdlaid);
		break;
	case APUSYS_CMD_EXECUTE:
	{
		struct mdla_run_cmd_sync *cmd_data =
			(struct mdla_run_cmd_sync *)cmd_hnd->kva;

		mdla_run_command_sync(
			&cmd_data->req,
			&cmd_data->res,
			mdla_info);
		break;
	}
	case APUSYS_CMD_PREEMPT:
		//FIXME: Must consider multi core command
		return -EINVAL;
	default:
		return -EINVAL;
	}
	return retval;

}
#endif

#ifdef __APUSYS_MDLA_UT__
#define MAX_ALLOC_SIZE (128 * 1024 * 1024)
static int mdla_dram_alloc(struct ioctl_malloc *malloc_data)
{
	dma_addr_t dma_addr = 0;

	if (malloc_data->size > MAX_ALLOC_SIZE)
		malloc_data->size = MAX_ALLOC_SIZE;

	malloc_data->kva = dma_alloc_attrs(mdlactlDevice, malloc_data->size,
			&dma_addr, GFP_KERNEL, 0);
	malloc_data->pa = (void *)dma_to_phys(mdlactlDevice, dma_addr);
	malloc_data->mva = (__u32)((long) malloc_data->pa);

	mdla_mem_debug("%s: kva:%p, mva:%x\n",
		__func__, malloc_data->kva, malloc_data->mva);

	return (malloc_data->kva) ? 0 : -ENOMEM;
}

static void mdla_dram_free(struct ioctl_malloc *malloc_data)
{
	mdla_mem_debug("%s: kva:%p, mva:%x\n",
		__func__, malloc_data->kva, malloc_data->mva);
	dma_free_attrs(mdlactlDevice, malloc_data->size,
			(void *) malloc_data->kva, malloc_data->mva, 0);
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
#if 0
	struct ioctl_perf perf_data;
#endif

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
		struct mdla_run_cmd_sync cmd_data;

		mdla_cmd_debug("mdla :kernel IOCTL_RUN_CMD_SYNC\n");
		if (copy_from_user(&cmd_data, (void *) arg,
				sizeof(cmd_data))) {
			return -EFAULT;
		}
		mdla_cmd_debug("%s: RUN_CMD_SYNC: kva=%p, mva=0x%08x, phys_to_virt=%p\n",
			__func__,
			(void *)cmd_data.req.kva,
			cmd_data.req.mva,
			phys_to_virt(cmd_data.req.mva));
		retval = mdla_run_command_sync(
			&cmd_data.req,
			&cmd_data.res,
			&mdla_devices[0]);//modify this to UT mdla0/1
		if (copy_to_user((void *) arg, &cmd_data, sizeof(cmd_data)))
			return -EFAULT;
		break;
	}

#if 0
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
#endif
#ifdef CONFIG_MTK_MDLA_ION
	case IOCTL_ION_KMAP:
		return mdla_ion_kmap(arg);
	case IOCTL_ION_KUNMAP:
		return mdla_ion_kunmap(arg);
#endif
	case IOCTL_CONFIG:
		return mdla_ioctl_config(arg);
	default:
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

#endif

static void mdlactl_exit(void)
{
	mdla_drv_debug("MDLA: Goodbye from the LKM!\n");
	mdla_debugfs_exit();
	device_destroy(mdlactlClass, MKDEV(majorNumber, 0));
	class_destroy(mdlactlClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	platform_driver_unregister(&mdla_driver);

}

module_init(mdlactl_init);
module_exit(mdlactl_exit);


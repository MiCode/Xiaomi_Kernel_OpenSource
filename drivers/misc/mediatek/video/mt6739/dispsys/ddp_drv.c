/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#define LOG_TAG "ddp_drv"

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
/* ION */
/* #include <linux/ion.h> */
/* #include <linux/ion_drv.h> */
/* #include "m4u.h" */
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "mt-plat/mtk_smi.h"
/* #include <mach/mt_reg_base.h> */
/* #include <mach/mt_irq.h> */
#include "ddp_clkmgr.h"
/* #include "mach/mt_irq.h" */
#include "mt-plat/sync_write.h"
#include "mt-plat/mtk_smi.h"
#include "m4u.h"

#include "ddp_drv.h"
#include "ddp_reg.h"
#include "ddp_hal.h"
#include "ddp_log.h"
#include "ddp_irq.h"
#include "ddp_info.h"
#include "ddp_m4u.h"
#include "display_recorder.h"

#define DISP_NO_DPI  /* FIXME: tmp define */
#ifndef DISP_NO_DPI
#include "ddp_dpi_reg.h"
#endif
#include "disp_helper.h"

#define DISP_DEVNAME "DISPSYS"
/* device and driver */
static dev_t disp_devno;
static struct cdev *disp_cdev;
static struct class *disp_class;

struct disp_node_struct {
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head testList;
	spinlock_t node_lock;
};

static struct platform_device mydev;

#if 0 /* defined but not used */
static unsigned int ddp_ms2jiffies(unsigned long ms)
{
	return (ms * HZ + 512) >> 10;
}
#endif

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) &&                                   \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)

#include "mobicore_driver_api.h"
#include "tlcApitplay.h"

/* ------------------------------------------------------------------------- */
/* handle address for t-play */
/* ------------------------------------------------------------------------- */
static unsigned int *tplay_handle_virt_addr;
static dma_addr_t handle_pa;

/* allocate a fixed physical memory address for storing tplay handle */
void init_tplay_handle(struct device *dev)
{
	void *va;

	va = dma_alloc_coherent(dev, sizeof(unsigned int), &handle_pa,
				GFP_KERNEL);
	if (va != NULL)
		DDPDBG("[SVP] allocate handle_pa[%pa]\n", &va);
	else
		DDPERR("[SVP] failed to allocate handle_pa\n");

	tplay_handle_virt_addr = (unsigned int *)va;
}

static int write_tplay_handle(unsigned int handle_value)
{
	if (tplay_handle_virt_addr != NULL) {
		DDPDBG("[SVP] %s 0x%x\n", __func__, handle_value);
		*tplay_handle_virt_addr = handle_value;
		return 0;
	}
	return -EFAULT;
}

static const uint32_t mc_deviceId = MC_DEVICE_ID_DEFAULT;

static const struct mc_uuid_t MC_UUID_TPLAY = {
	{0x05, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00} };

static struct mc_session_handle tplaySessionHandle;
static tplay_tciMessage_t *pTplayTci;

static unsigned int opened_device;
static enum mc_result late_open_mobicore_device(void)
{
	enum mc_result mcRet = MC_DRV_OK;

	if (opened_device == 0) {
		DDPDBG("=============== open mobicore device ===============\n");
		/* Open MobiCore device */
		mcRet = mc_open_device(mc_deviceId);
		if (mcRet == MC_DRV_ERR_INVALID_OPERATION) {
			/* skip false alarm when called more than once */
			DDPDBG("mc_open_device already done\n");
		} else if (mcRet != MC_DRV_OK) {
			DDPERR("mc_open_device failed: %d @%s line %d\n",
			       mcRet, __func__, __LINE__);
			return mcRet;
		}
		opened_device = 1;
	}
	return MC_DRV_OK;
}

static int open_tplay_driver_connection(void)
{
	enum mc_result mcRet = MC_DRV_OK;

	if (tplaySessionHandle.session_id != 0) {
		DDPMSG("tplay TDriver session already created\n");
		return 0;
	}

	DDPDBG("============= late init tplay TDriver session ===========\n");
	do {
		late_open_mobicore_device();

		/* Allocating WSM for DCI */
		mcRet = mc_malloc_wsm(mc_deviceId, 0,
				      sizeof(tplay_tciMessage_t),
				      (uint8_t **)&pTplayTci, 0);
		if (mcRet != MC_DRV_OK) {
			DDPERR("mc_malloc_wsm failed: %d @%s line %d\n",
			       mcRet, __func__, __LINE__);
			return -1;
		}

		/* Open session the TDriver */
		memset(&tplaySessionHandle, 0, sizeof(tplaySessionHandle));
		tplaySessionHandle.device_id = mc_deviceId;
		mcRet = mc_open_session(&tplaySessionHandle, &MC_UUID_TPLAY,
					(uint8_t *)pTplayTci,
					(uint32_t)sizeof(tplay_tciMessage_t));
		if (mcRet != MC_DRV_OK) {
			DDPERR("mc_open_session failed: %d @%s line %d\n",
			       mcRet, __func__, __LINE__);
			/* if failed clear session handle */
			memset(&tplaySessionHandle, 0,
			       sizeof(tplaySessionHandle));
			return -1;
		}
	} while (0);

	return (mcRet != MC_DRV_OK) ? 0 : -1;
}

static int close_tplay_driver_connection(void)
{
	enum mc_result mcRet = MC_DRV_OK;

	DDPDBG("=============== close tplay TDriver session ===============\n");
	/* Close session */
	if (tplaySessionHandle.session_id != 0) {
		/* we have an valid session */
		mcRet = mc_close_session(&tplaySessionHandle);
		if (mcRet != MC_DRV_OK) {
			DDPERR("mc_close_session failed: %d @%s line %d\n",
			       mcRet, __func__, __LINE__);
			memset(&tplaySessionHandle, 0,
				sizeof(tplaySessionHandle));
			return -1;
		}
	}
	memset(&tplaySessionHandle, 0, sizeof(tplaySessionHandle));

	mcRet = mc_free_wsm(mc_deviceId, (uint8_t *)pTplayTci);
	if (mcRet != MC_DRV_OK) {
		DDPERR("mc_free_wsm failed: %d @%s line %d\n",
		       mcRet, __func__, __LINE__);
		return -1;
	}

	return 0;
}

/* return 0 for success and -1 for error */
static int set_tplay_handle_addr_request(void)
{
	int ret = 0;
	enum mc_result mcRet = MC_DRV_OK;

	DDPDBG("[SVP] %s\n", __func__);

	open_tplay_driver_connection();
	if (tplaySessionHandle.session_id == 0) {
		DDPERR("[SVP] invalid tplay session\n");
		return -1;
	}

	DDPDBG("[SVP] handle_pa=0x%pa\n", &handle_pa);
	/* set other TCI parameter */
	pTplayTci->tplay_handle_low_addr = (uint32_t)handle_pa;
	pTplayTci->tplay_handle_high_addr = (uint32_t)(handle_pa >> 32);
	/* set TCI command */
	pTplayTci->cmd.header.commandId = CMD_TPLAY_REQUEST;

	/* notify the trustlet */
	DDPDBG("[SVP] notify Tlsec trustlet CMD_TPLAY_REQUEST\n");
	mcRet = mc_notify(&tplaySessionHandle);
	if (mcRet != MC_DRV_OK) {
		DDPERR("[SVP] mc_notify failed: %d @%s line %d\n", mcRet,
		       __func__, __LINE__);
		ret = -1;
		goto _notify_to_trustlet_fail;
	}

	/* wait for response from the trustlet */
	mcRet = mc_wait_notification(&tplaySessionHandle, MC_INFINITE_TIMEOUT);
	if (mcRet != MC_DRV_OK) {
		DDPERR("[SVP] mc_wait_notification failed: %d @%s line %d\n",
		       mcRet, __func__, __LINE__);
		ret = -1;
		goto _notify_from_trustlet_fail;
	}

	DDPDBG("[SVP] CMD_TPLAY_REQUEST result=%d, return code=%d\n",
	       pTplayTci->result, pTplayTci->rsp.header.returnCode);

_notify_from_trustlet_fail:
_notify_to_trustlet_fail:
	close_tplay_driver_connection();

	return ret;
}

#ifdef TPLAY_DUMP_PA_DEBUG
static int dump_tplay_physcial_addr(void)
{
	DDPDBG("[SVP] %s\n", __func__);
	int ret = 0;
	enum mc_result mcRet = MC_DRV_OK;

	open_tplay_driver_connection();
	if (tplaySessionHandle.session_id == 0) {
		DDPERR("[SVP] invalid tplay session\n");
		return -1;
	}

	/* set TCI command */
	pTplayTci->cmd.header.commandId = CMD_TPLAY_DUMP_PHY;

	/* notify the trustlet */
	DDPMSG("[SVP] notify Tlsec trustlet CMD_TPLAY_DUMP_PHY\n");
	mcRet = mc_notify(&tplaySessionHandle);
	if (mcRet != MC_DRV_OK) {
		DDPERR("[SVP] mc_notify failed: %d @%s line %d\n",
		       mcRet, __func__, __LINE__);
		ret = -1;
		goto _notify_to_trustlet_fail;
	}

	/* wait for response from the trustlet */
	mcRet = mc_wait_notification(&tplaySessionHandle, MC_INFINITE_TIMEOUT);
	if (mcRet != MC_DRV_OK) {
		DDPERR("[SVP] mc_wait_notification failed: %d @%s line %d\n",
		       mcRet, __func__, __LINE__);
		ret = -1;
		goto _notify_from_trustlet_fail;
	}

	DDPDBG("[SVP] CMD_TPLAY_DUMP_PHY result=%d, return code=%d\n",
	       pTplayTci->result, pTplayTci->rsp.header.returnCode);

_notify_from_trustlet_fail:
_notify_to_trustlet_fail:
	close_tplay_driver_connection();

	return ret;
}
#endif /* TPLAY_DUMP_PA_DEBUG */

static int disp_path_notify_tplay_handle(unsigned int handle_value)
{
	int ret;
	static int executed; /* this function can execute only once */

	if (executed == 0) {
		if (set_tplay_handle_addr_request())
			return -EFAULT;
		executed = 1;
	}

	ret = write_tplay_handle(handle_value);

#ifdef TPLAY_DUMP_PA_DEBUG
	dump_tplay_physcial_addr();
#endif /* TPLAY_DUMP_PA_DEBUG */

	return ret;
}
#endif /* CONFIG_TRUSTONIC_TEE_SUPPORT && CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */

static long disp_unlocked_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
/* disp_node_struct *pNode = (disp_node_struct *) file->private_data; */

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) &&                                   \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if (cmd == DISP_IOCTL_SET_TPLAY_HANDLE) {
		unsigned int value;

		if (copy_from_user(&value, (void *)arg, sizeof(unsigned int))) {
			DDPERR("DISP_IOCTL_SET_TPLAY_HANDLE, copy_from_user failed\n");
			return -EFAULT;
		}
		if (disp_path_notify_tplay_handle(value)) {
			DDPERR("DISP_IOCTL_SET_TPLAY_HANDLE, disp_path_notify_tplay_handle failed\n");
			return -EFAULT;
		}
	}
#endif

	return 0;
}

static long disp_compat_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) &&                                   \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if (cmd == DISP_IOCTL_SET_TPLAY_HANDLE)
		return disp_unlocked_ioctl(file, cmd, arg);
#endif

	return 0;
}

static int disp_open(struct inode *inode, struct file *file)
{
	struct disp_node_struct *pNode = NULL;

	DDPDBG("enter %s() process:%s\n", __func__, current->comm);

	/* Allocate and initialize private data */
	file->private_data =
		kmalloc(sizeof(struct disp_node_struct), GFP_ATOMIC);
	if (file->private_data == NULL) {
		DDPMSG("Not enough entry for DDP open operation\n");
		return -ENOMEM;
	}

	pNode = (struct disp_node_struct *)file->private_data;
	pNode->open_pid = current->pid;
	pNode->open_tgid = current->tgid;
	INIT_LIST_HEAD(&(pNode->testList));
	spin_lock_init(&pNode->node_lock);

	return 0;
}

static ssize_t disp_read(struct file *file, char __user *data, size_t len,
			 loff_t *ppos)
{
	return 0;
}

static int disp_release(struct inode *inode, struct file *file)
{
	struct disp_node_struct *pNode = NULL;
	/* unsigned int index = 0; */
	DDPDBG("enter %s() process:%s\n", __func__, current->comm);

	pNode = (struct disp_node_struct *)file->private_data;

	spin_lock(&pNode->node_lock);

	spin_unlock(&pNode->node_lock);

	if (file->private_data != NULL) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0;
}

static int disp_flush(struct file *file, fl_owner_t a_id)
{
	return 0;
}

struct dispsys_device {
	struct device *dev;
};

struct device *disp_get_device(void)
{
	return &(mydev.dev);
}

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
static struct miscdevice disp_misc_dev;
#endif
/* Kernel interface */
static const struct file_operations disp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = disp_unlocked_ioctl,
	.compat_ioctl = disp_compat_ioctl,
	.open = disp_open,
	.release = disp_release,
	.flush = disp_flush,
	.read = disp_read,
};

/* disp_clk_init
 * 1. parsing dtsi
 * 2. clk force on
 */
static void disp_clk_init(struct platform_device *pdev)
{
	int i;
	struct clk *pclk = NULL;

	DDPMSG("DT disp clk parse beign\n");

	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		DDPMSG("DISPSYS get clock %s\n", ddp_get_clk_name(i));
		pclk = devm_clk_get(&pdev->dev, ddp_get_clk_name(i));
		if (IS_ERR(pclk)) {
			DDPERR("%s:%d, DISPSYS get %d,%s clock error!!!\n",
			       __FILE__, __LINE__, i, ddp_get_clk_name(i));
			continue;
		}
		ddp_set_clk_handle(pclk, i);
	}

	DDPMSG("DT disp clk parse end\n");

	/* disp-clk force on */
	ddp_clk_force_on(1);
}

static int disp_probe(struct platform_device *pdev)
{

	static unsigned int disp_probe_cnt;

	pr_info("disp driver(1) %s early\n", __func__);

	if (disp_probe_cnt != 0)
		return 0;

	pr_info("disp driver(1) %s begin\n", __func__);

	/* save pdev for disp_probe_1 */
	memcpy(&mydev, pdev, sizeof(mydev));

	disp_helper_option_init();

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
		disp_clk_init(pdev);

	disp_probe_cnt++;

	pr_info("disp driver(1) %s end\n", __func__);

	return 0;
}

/* begin for irq check */
static inline unsigned int gic_irq(struct irq_data *d)
{
	return d->hwirq;
}

static inline unsigned int virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc;
	unsigned int hwirq = 0;

	desc = irq_to_desc(virq);

	if (!desc)
		WARN_ON(1);
	else
		hwirq = gic_irq(&desc->irq_data);

	return hwirq;
}
/* end for irq check */

static int __init disp_probe_1(void)
{
	int ret = 0;
	int i;
	unsigned long va;
	unsigned int irq;

	pr_info("disp driver(1) %s begin\n", __func__);

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	disp_misc_dev.minor = MISC_DYNAMIC_MINOR;
	disp_misc_dev.name = "mtk_disp";
	disp_misc_dev.fops = &disp_fops;
	disp_misc_dev.parent = NULL;
	ret = misc_register(&disp_misc_dev);
	if (ret) {
		DISPERR("disp: fail to create mtk_disp node\n");
		return (unsigned long)(ERR_PTR(ret));
	}
	/* secure video path implementation: a physical address is allocated to
	 * place a handle for decryption buffer.
	 * Non-zero value for valid VA
	 */
	init_tplay_handle(disp_get_device());
#endif

	/* do disp_init_irq before register irq */
	disp_init_irq();

	/* iomap va and irq */
	for (i = 0; i < DISP_MODULE_NUM; i++) {
		int status;
		struct device_node *node = NULL;
		struct resource res;

		if (!is_ddp_module_has_reg_info(i))
			continue;

		node = of_find_compatible_node(NULL, NULL,
					       ddp_get_module_dtname(i));
		if (node == NULL) {
			DDPERR("[ERR]DT, i=%d, module=%s, unable to find node, dt_name=%s\n",
					i,
					ddp_get_module_name(i),
					ddp_get_module_dtname(i));
			continue;
		}

		va = (unsigned long)of_iomap(node, 0);
		if (!va) {
			DDPERR("[ERR]DT, i=%d, module=%s, unable to ge VA, of_iomap fail\n",
					i,
					ddp_get_module_name(i));
			continue;
		} else {
			ddp_set_module_va(i, va);
		}

		status = of_address_to_resource(node, 0, &res);
		if (status < 0) {
			DDPERR("[ERR]DT, i=%d, module=%s, unable to get PA\n",
					i,
					ddp_get_module_name(i));
			continue;
		}

		if (ddp_get_module_pa(i) != res.start)
			DDPERR("[ERR]DT, i=%d, module=%s, map_addr=%p, reg_pa=0x%lx!=0x%pa\n",
			       i, ddp_get_module_name(i),
			       (void *)ddp_get_module_va(i),
			       ddp_get_module_pa(i),
			       (void *)(uintptr_t)res.start);

		/* get IRQ ID and request IRQ */
		irq = irq_of_parse_and_map(node, 0);
		ddp_set_module_irq(i, irq);

		DDPMSG("DT, i=%d, module=%s, map_addr=%p, map_irq=%d, reg_pa=0x%lx\n",
		       i, ddp_get_module_name(i), (void *)ddp_get_module_va(i),
		       ddp_get_module_irq(i), ddp_get_module_pa(i));
	}

	/* register irq */
	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_is_irq_enable(i) == 1) {
			if (ddp_get_module_irq(i) == 0) {
				DDPERR("[ERR]DT, i=%d, module=%s, map_irq=%d\n",
				       i, ddp_get_module_name(i),
				       ddp_get_module_irq(i));
				ddp_module_irq_disable(i);
				continue;
			}
			DDPMSG("DT, i=%d, module=%s, map_irq=%d, hw_irq = %d\n",
			       i, ddp_get_module_name(i), ddp_get_module_irq(i),
			       virq_to_hwirq(ddp_get_module_irq(i)));

			/* IRQF_TRIGGER_NONE dose not take effect here, real
			 * trigger mode set in dts file
			 */
			ret = request_irq(ddp_get_module_irq(i),
					  (irq_handler_t)disp_irq_handler,
					  IRQF_TRIGGER_NONE,
					  ddp_get_module_name(i), NULL);
			if (ret) {
				DDPERR("[ERR]DT, i=%d, module=%s, request_irq(%d) fail\n",
				       i, ddp_get_module_name(i),
				       ddp_get_module_irq(i));
				continue;
			}
			DDPMSG("irq enabled, module=%s, irq=%d\n",
			       ddp_get_module_name(i), ddp_get_module_irq(i));
		}
	}

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
#if 0
		/* check all cg on when early porting and bring up */
		ASSERT(DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0) == 0);
		ASSERT((DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1)&0xff) == 0);
		DDPMSG("after power on MMSYS:0x%x,0x%x\n",
			DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
			DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));
#else
		/* workaround if cg not on */
		DDPMSG("before power on MMSYS:0x%x,0x%x\n",
		       DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
		       DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));

		DISP_REG_SET(NULL, DISP_REG_CONFIG_MMSYS_CG_CLR0, 0xFFFFFFFF);
		DISP_REG_SET(NULL, DISP_REG_CONFIG_MMSYS_CG_CLR1, 0xFFFFFFFF);

		DDPMSG("after power on MMSYS:0x%x,0x%x\n",
		       DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
		       DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));
#endif
	}

	ddp_path_init();
	disp_m4u_init();

	pr_info("disp driver(1) %s end\n", __func__);
	/* NOT_REFERENCED(class_dev); */
	return ret;
}

static int disp_remove(struct platform_device *pdev)
{

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	misc_deregister(&disp_misc_dev);
#endif
	return 0;
}

static void disp_shutdown(struct platform_device *pdev)
{
	/* Nothing yet */
}

/* PM suspend */
static int disp_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

/* PM resume */
static int disp_resume(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id dispsys_of_ids[] = {
	{.compatible = "mediatek,dispsys",},
	{}
};

static struct platform_driver dispsys_of_driver = {
	.driver = {
			.name = DISP_DEVNAME,
			.owner = THIS_MODULE,
			.of_match_table = dispsys_of_ids,
		},
	.probe = disp_probe,
	.remove = disp_remove,
	.shutdown = disp_shutdown,
	.suspend = disp_suspend,
	.resume = disp_resume,
};

static int __init disp_init(void)
{
	int ret = 0;

	init_log_buffer();
	DDPMSG("Register the disp driver\n");
	if (platform_driver_register(&dispsys_of_driver)) {
		DDPERR("failed to register disp driver\n");
		/* platform_device_unregister(&disp_device); */
		ret = -ENODEV;
		return ret;
	}
	DDPMSG("disp driver init done\n");
	return 0;
}

static void __exit disp_exit(void)
{
	ASSERT(0);
	/* disp-clk force on disable ??? */

	cdev_del(disp_cdev);
	unregister_chrdev_region(disp_devno, 1);

	platform_driver_unregister(&dispsys_of_driver);

	device_destroy(disp_class, disp_devno);
	class_destroy(disp_class);
}

static int __init disp_late(void)
{
	int ret = 0;

	DDPMSG("disp driver(1) %s begin\n", __func__);
	/* for rt5081 */
	/*ret = display_bias_regulator_init();*/
	if (ret < 0)
		DISPERR("get dsv_pos fail, ret = %d\n", ret);

	/*display_bias_enable();*/
	DDPMSG("disp driver(1) %s end\n", __func__);
	return 0;
}
#ifndef MTK_FB_DO_NOTHING
arch_initcall(disp_init);
module_init(disp_probe_1);
module_exit(disp_exit);
late_initcall(disp_late);
#endif
MODULE_AUTHOR("Tzu-Meng, Chung <Tzu-Meng.Chung@mediatek.com>");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");

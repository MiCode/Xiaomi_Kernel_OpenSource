/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/ktime.h>
#include "adsp_clk.h"
#include "adsp_mbox.h"
#include "adsp_reserved_mem.h"
#include "adsp_logger.h"
#include "adsp_excep.h"
#include "adsp_reg.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_core.h"

#include <linux/suspend.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <mt-plat/mtk_secure_api.h> /* for SMC ID table */
#include <mt6853-afe-common.h>

struct wait_queue_head adsp_waitq;
struct workqueue_struct *adsp_wq;
void __iomem *adsp_secure_base;
struct adsp_priv *adsp_cores[ADSP_CORE_TOTAL];
static u32 adsp_load;

static int adsp_core0_init(struct adsp_priv *pdata);
static int adsp_core0_suspend(void);
static int adsp_core0_resume(void);
static void adsp_logger_init0_cb(struct work_struct *ws);


static int adsp_after_bootup(struct adsp_priv *pdata);

static const struct adsp_description adsp_c0_desc = {
	.id = 0,
	.name = "adsp_0",
	.sharedmems = {
		[ADSP_SHAREDMEM_BOOTUP_MARK] = {0x0004, 0x0004},
		[ADSP_SHAREDMEM_SYS_STATUS] = {0x0008, 0x0004},
		[ADSP_SHAREDMEM_MPUINFO] = {0x0028, 0x0020},
		[ADSP_SHAREDMEM_WAKELOCK] = {0x002C, 0x0004},
		[ADSP_SHAREDMEM_IPCBUF] = {0x0300, 0x0200},
		[ADSP_SHAREDMEM_TIMESYNC] = {0x2530, 0x0020},
		[ADSP_SHAREDMEM_DVFSSYNC] = {0x253C, 0x000C},
		[ADSP_SHAREDMEM_SLEEPSYNC] = {0x2540, 0x0004},
		[ADSP_SHAREDMEM_BUS_MON_DUMP] = {0x25FC, 0x00BC},
		[ADSP_SHAREDMEM_INFRA_BUS_DUMP] = {0x269C, 0x00A0},
		[ADSP_SHAREDMEM_LATMON_DUMP] = {0x26B8, 0x001C},
	},
	.ops = {
		.initialize = adsp_core0_init,
		.after_bootup = adsp_after_bootup,
	}
};

/*------------------------------------------------------------*/
/* adsp operation */

int adsp_core0_init(struct adsp_priv *pdata)
{
	int ret = 0;

	pdata->debugfs = debugfs_create_file("audiodsp0", S_IFREG | 0644, NULL,
					     pdata, &adsp_debug_ops);

	adsp_wq = alloc_workqueue("adsp_wq", WORK_CPU_UNBOUND | WQ_HIGHPRI, 0);
	pdata->wq = adsp_wq;
	init_waitqueue_head(&adsp_waitq);

	init_adsp_feature_control(pdata->id, pdata->feature_set, 1000,
				adsp_wq,
				adsp_core0_suspend,
				adsp_core0_resume);

	adsp_update_mpu_memory_info(pdata);

	/* exception init & irq */
	init_adsp_exception_control(pdata->dev, adsp_wq, &adsp_waitq);
	adsp_irq_registration(pdata->id, ADSP_IRQ_WDT_ID, adsp_wdt_handler,
			      pdata);

	/* logger */
	pdata->log_ctrl = adsp_logger_init(ADSP_A_LOGGER_MEM_ID, adsp_logger_init0_cb);

	/* mailbox */
	mutex_init(&pdata->send_mbox->mutex_send);
	pdata->recv_mbox->pin_buf = vmalloc(SHARE_BUF_SIZE);
	if (!pdata->recv_mbox->pin_buf)
		return -ENOMEM;
	pdata->recv_mbox->prdata = &pdata->id;

	/* dram_remap */
	set_adsp_dram_remapping(pdata->sysram_phys,
				pdata->sysram_size);

	adsp_awake_init(pdata, ADSP_A_SW_INT);

	return ret;
}

bool is_adsp_load(void)
{
	return adsp_load;
}

static int adsp_after_bootup(struct adsp_priv *pdata)
{
#ifdef BRINGUP_ADSP
	/* disable adsp suspend by registering feature */
	_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0);
#endif
	return adsp_awake_unlock(pdata->id);
}

static bool is_adsp_core_suspend(struct adsp_priv *pdata)
{
	u32 status = 0;

	if (unlikely(!pdata))
		return false;

	adsp_copy_from_sharedmem(pdata,
				 ADSP_SHAREDMEM_SYS_STATUS,
				 &status, sizeof(status));

	return check_hifi_status(ADSP_A_IS_WFI) &&
	       check_hifi_status(ADSP_AXI_BUS_IS_IDLE) &&
	       (status == ADSP_SUSPEND);
}

int adsp_core0_suspend(void)
{
	int ret = 0, retry = 10;
	u32 status = 0;
	struct adsp_priv *pdata = adsp_cores[ADSP_A_ID];
	ktime_t start = ktime_get();

	if (get_adsp_state(pdata) == ADSP_RUNNING) {
		reinit_completion(&pdata->done);
		ret = adsp_push_message(ADSP_IPI_DVFS_SUSPEND, &status,
					sizeof(status), 2000, pdata->id);
		if (ret != ADSP_IPI_DONE) {
			ret = -EPIPE;
			goto ERROR;
		}

		/* wait core suspend ack timeout 2s */
		ret = wait_for_completion_timeout(&pdata->done, 2 * HZ);
		if (!ret) {
			ret = -ETIMEDOUT;
			goto ERROR;
		}

		while (--retry && !is_adsp_core_suspend(pdata))
			usleep_range(100, 200);

		if (retry == 0 || get_adsp_state(pdata) == ADSP_RESET) {
			ret = -ETIME;
			goto ERROR;
		}

		if (get_adsp_state(pdata) == ADSP_RESET) {
			ret = -EFAULT;
			goto ERROR;
		}

		adsp_mt_stop(pdata->id);
		switch_adsp_power(false);
		set_adsp_state(pdata, ADSP_SUSPEND);
	}
	pr_info("%s(), done elapse %lld us", __func__,
		ktime_us_delta(ktime_get(), start));
	return 0;
ERROR:
	pr_warn("%s(), can't going to suspend, ret(%d)\n", __func__, ret);
	adsp_aed_dispatch(EXCEP_KERNEL, pdata);
	return ret;
}

int adsp_core0_resume(void)
{
	int ret = 0;
	struct adsp_priv *pdata = adsp_cores[ADSP_A_ID];
	ktime_t start = ktime_get();

	if (get_adsp_state(pdata) == ADSP_SUSPEND) {
		switch_adsp_power(true);
		adsp_mt_sw_reset(pdata->id);

		set_adsp_dram_remapping(pdata->sysram_phys,
					pdata->sysram_size);
		timesync_to_adsp(pdata, APTIME_UNFREEZE);

		reinit_completion(&pdata->done);
		adsp_mt_run(pdata->id);
		ret = wait_for_completion_timeout(&pdata->done, 2 * HZ);

		if (get_adsp_state(pdata) != ADSP_RUNNING) {
			pr_warn("%s, can't going to resume\n", __func__);
			adsp_aed_dispatch(EXCEP_KERNEL, pdata);
			return -ETIME;
		}
	}
	pr_info("%s(), done elapse %lld us", __func__,
		ktime_us_delta(ktime_get(), start));
	return 0;
}

void adsp_logger_init0_cb(struct work_struct *ws)
{
	int ret;
	uint64_t info[6];

	info[0] = adsp_get_reserve_mem_phys(ADSP_A_LOGGER_MEM_ID);
	info[1] = adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);
	info[2] = adsp_get_reserve_mem_phys(ADSP_A_CORE_DUMP_MEM_ID);
	info[3] = adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID);
	info[4] = adsp_get_reserve_mem_phys(ADSP_A_DEBUG_DUMP_MEM_ID);
	info[5] = adsp_get_reserve_mem_size(ADSP_A_DEBUG_DUMP_MEM_ID);

	_adsp_register_feature(ADSP_A_ID, ADSP_LOGGER_FEATURE_ID, 0);

	ret = adsp_push_message(ADSP_IPI_LOGGER_INIT, (void *)info,
		sizeof(info), 20, ADSP_A_ID);

	_adsp_deregister_feature(ADSP_A_ID, ADSP_LOGGER_FEATURE_ID, 0);

	if (ret != ADSP_IPI_DONE)
		pr_err("[ADSP]logger initial fail, ipi ret=%d\n", ret);
}

/*---------------------------------------------------------------------------*/
static const struct of_device_id adsp_of_ids[] = {
	{ .compatible = "mediatek,adsp_core_0", .data = &adsp_c0_desc},
	{}
};

static const struct of_device_id adsp_common_of_ids[] = {
	{ .compatible = "mediatek,adsp_common"},
	{}
};

const struct attribute_group *adsp_common_attr_groups[] = {
	&adsp_excep_attr_group,
	NULL,
};

const struct attribute_group *adsp_core_attr_groups[] = {
	&adsp_default_attr_group,
	NULL,
};

static struct miscdevice adsp_common_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "adsp",
	.groups = adsp_common_attr_groups,
	.fops = &adsp_common_file_ops,
};

/* user-space event notify */
static int adsp_user_event_notify(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	struct device *dev = adsp_common_device.this_device;
	int ret = 0;

	if (!dev)
		return NOTIFY_DONE;

	switch (event) {
	case ADSP_EVENT_STOP:
		ret = kobject_uevent(&dev->kobj, KOBJ_OFFLINE);
		break;
	case ADSP_EVENT_READY:
		ret = kobject_uevent(&dev->kobj, KOBJ_ONLINE);
		break;
	default:
		pr_info("%s, ignore event %lu", __func__, event);
		break;
	}

	if (ret)
		pr_info("%s, uevnet(%lu) fail, ret %d", __func__, event, ret);

	return NOTIFY_OK;
}

struct notifier_block adsp_uevent_notifier = {
	.notifier_call = adsp_user_event_notify,
	.priority = AUDIO_HAL_FEATURE_PRI,
};

#ifdef CONFIG_PM
static int adsp_pm_event(struct notifier_block *notifier
			, unsigned long pm_event, void *unused)
{
	struct arm_smccc_res res;

	switch (pm_event) {
	case PM_POST_HIBERNATION:
		pr_notice("[ADSP] %s: reboot\n", __func__);
		adsp_reset();
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		if (adsp_feature_is_active(ADSP_A_ID))
			arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
				  MTK_AUDIO_SMC_OP_ADSP_REQUEST,
				  0, 0, 0, 0, 0, 0, &res);

		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		if (adsp_feature_is_active(ADSP_A_ID))
			arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
				  MTK_AUDIO_SMC_OP_ADSP_RELEASE,
				  0, 0, 0, 0, 0, 0, &res);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block adsp_pm_notifier_block = {
	.notifier_call = adsp_pm_event,
	.priority = 0,
};
#endif

static int adsp_common_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;

	/* indicate if adsp images is loaded successfully */
	of_property_read_u32(dev->of_node, "load", &adsp_load);
	if (!adsp_load)
		pr_info("%s adsp disable\n", __func__);

	/* get resource from platform_device */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adsp_secure_base = devm_ioremap_resource(dev, res);

	ret = adsp_clk_device_probe(pdev);
	if (ret) {
		pr_warn("%s(), clk probe fail, %d\n", __func__, ret);
		goto ERROR;
	}

	ret = adsp_mem_device_probe(pdev);
	if (ret) {
		pr_info("%s(), memory probe fail, %d\n", __func__, ret);
		goto ERROR;
	}

	ret = adsp_mbox_probe(pdev);
	if (ret) {
		pr_warn("%s(), mbox probe fail, %d\n", __func__, ret);
		goto ERROR;
	}

	ret = misc_register(&adsp_common_device);
	if (ret) {
		pr_warn("%s(), misc_register fail, %d\n", __func__, ret);
		goto ERROR;
	}

	adsp_register_notify(&adsp_uevent_notifier);

#ifdef CONFIG_PM
	ret = register_pm_notifier(&adsp_pm_notifier_block);
	if (ret)
		pr_warn("[ADSP] failed to register PM notifier %d\n", ret);
#endif

	pr_info("%s, success\n", __func__);
ERROR:
	return ret;
}

static int adsp_common_drv_remove(struct platform_device *pdev)
{
	return 0;
}

static int adsp_core_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct adsp_priv *pdata;
	const struct adsp_description *desc;
	const struct of_device_id *match;
	struct of_phandle_args spec;
	u32 temp = 0;

	/* create private data */
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	match = of_match_node(adsp_of_ids, dev->of_node);
	if (!match)
		return -ENODEV;

	desc = (struct adsp_description *)match->data;

	pdata->id = desc->id;
	pdata->name = desc->name;
	pdata->ops = &desc->ops;
	pdata->mapping_table = desc->sharedmems;

	pdata->dev = dev;
	init_completion(&pdata->done);

	/* get resource from platform_device */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pdata->cfg = devm_ioremap_resource(dev, res);
	pdata->cfg_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pdata->itcm = devm_ioremap_resource(dev, res);
	pdata->itcm_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	pdata->dtcm = devm_ioremap_resource(dev, res);
	pdata->dtcm_size = resource_size(res);

	pdata->irq[ADSP_IRQ_IPC_ID].seq = platform_get_irq(pdev, 0);
	pdata->irq[ADSP_IRQ_IPC_ID].clear_irq = adsp_mt_clr_sysirq;
	pdata->irq[ADSP_IRQ_WDT_ID].seq = platform_get_irq(pdev, 1);
	pdata->irq[ADSP_IRQ_WDT_ID].clear_irq = adsp_mt_disable_wdt;
	pdata->irq[ADSP_IRQ_AUDIO_ID].seq = platform_get_irq(pdev, 2);
	pdata->irq[ADSP_IRQ_AUDIO_ID].clear_irq = adsp_mt_clr_auidoirq;

	of_property_read_u32(dev->of_node, "sysram", &temp);
	pdata->sysram_phys = (phys_addr_t)temp;
	of_property_read_u32(dev->of_node, "sysram_size", &temp);
	pdata->sysram_size = (size_t)temp;
	if (pdata->sysram_phys == 0 || pdata->sysram_size == 0)
		return -ENODEV;
	pdata->sysram = ioremap_wc(pdata->sysram_phys, pdata->sysram_size);

	pdata->secure = adsp_secure_base;

	pdata->feature_set = 0xffffffff; //no need for one core platform
	/* mailbox channel parsing */
	if (of_parse_phandle_with_args(dev->of_node, "mboxes",
				       "#mbox-cells", 0, &spec)) {
		dev_dbg(dev, "%s: can't parse \"mboxes\" property\n", __func__);
		return -ENODEV;
	}
	pdata->send_mbox = get_adsp_mbox_pin_send(spec.args[0]);

	if (of_parse_phandle_with_args(dev->of_node, "mboxes",
				       "#mbox-cells", 1, &spec)) {
		dev_dbg(dev, "%s: can't parse \"mboxes\" property\n", __func__);
		return -ENODEV;
	}
	pdata->recv_mbox = get_adsp_mbox_pin_recv(spec.args[0]);

	/* register misc device */
	pdata->mdev.minor = MISC_DYNAMIC_MINOR;
	pdata->mdev.name = desc->name;
	pdata->mdev.fops = &adsp_core_file_ops;
	pdata->mdev.groups = adsp_core_attr_groups;

	ret = misc_register(&pdata->mdev);
	if (unlikely(ret != 0))
		goto ERROR;

	/* add to adsp_core list */
	adsp_cores[desc->id] = pdata;

	pr_info("%s, id:%d success\n", __func__, pdata->id);
	return 0;
ERROR:
	return ret;
}

static int adsp_core_drv_remove(struct platform_device *pdev)
{
	return 0;
}

static int adsp_ap_suspend(struct device *dev)
{
	int cid = 0, ret = 0;
	struct adsp_priv *pdata = NULL;

	for (cid = ADSP_CORE_TOTAL - 1; cid >= 0; cid--) {
		pdata = adsp_cores[cid];

		if (pdata->state == ADSP_RUNNING) {
			ret = flush_suspend_work(pdata->id);

			pr_info("%s, flush_suspend_work ret %d, cid %d",
				__func__, ret, cid);
		}
	}

#ifdef CONFIG_MTK_TIMER_TIMESYNC
	if (is_adsp_system_running()) {
		timesync_to_adsp(adsp_cores[ADSP_A_ID], APTIME_FREEZE);
		pr_info("%s, time sync freeze", __func__);
	}
#endif
	return 0;
}

static int adsp_ap_resume(struct device *dev)
{
#ifdef CONFIG_MTK_TIMER_TIMESYNC
	if (is_adsp_system_running()) {
		timesync_to_adsp(adsp_cores[ADSP_A_ID], APTIME_UNFREEZE);
		pr_info("%s, time sync unfreeze", __func__);
	}
#endif
	return 0;
}

static const struct dev_pm_ops adsp_ap_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(adsp_ap_suspend,
				adsp_ap_resume)
};

static struct platform_driver adsp_common_driver = {
	.probe = adsp_common_drv_probe,
	.remove = adsp_common_drv_remove,
	.driver = {
		.name = "adsp_common",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = adsp_common_of_ids,
#endif
#ifdef CONFIG_PM
		.pm = &adsp_ap_pm_ops,
#endif
	},
};

static struct platform_driver adsp_core0_driver = {
	.probe = adsp_core_drv_probe,
	.remove = adsp_core_drv_remove,
	.driver = {
		.name = "adsp_core0",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = adsp_of_ids,
#endif
	},
};


static struct platform_driver * const drivers[] = {
	&adsp_common_driver,
	&adsp_core0_driver,
};

int create_adsp_drivers(void)
{
	int ret = 0;

	ret = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	return (is_adsp_load() && adsp_cores[0] && ~ret);
}


// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>

#include <tcpm.h>

#define MTK_CTD_DRV_VERSION	"1.0.0_MTK"

struct mtk_ctd_info {
	struct device *dev;
	/* device tree */
	u32 bc12_sel;
	struct power_supply *bc12_psy;
	/* typec notify */
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	/* chg det */
	wait_queue_head_t attach_wq;
	atomic_t chrdet_start;
	struct task_struct *attach_task;
	struct mutex attach_lock;
	bool typec_attach;
	bool tcpc_kpoc;
};

enum {
	MTK_CTD_BY_SUBPMIC,
	MTK_CTD_BY_MAINPMIC,
	MTK_CTD_BY_EXTCHG,
};

static int mtk_ext_get_charger_type(struct mtk_ctd_info *mci, bool attach)
{
	union power_supply_propval prop;
	static struct power_supply *bc12_psy;
	int ret = 0;

	if (mci->bc12_sel == MTK_CTD_BY_MAINPMIC)
		bc12_psy = power_supply_get_by_name("mtk_charger_type");
	else if (mci->bc12_sel == MTK_CTD_BY_EXTCHG)
		bc12_psy = power_supply_get_by_name("ext_charger_type");
	if (IS_ERR_OR_NULL(bc12_psy)) {
		pr_notice("%s Couldn't get bc12_psy\n", __func__);
		return ret;
	}

	prop.intval = attach;
	return power_supply_set_property(bc12_psy,
					 POWER_SUPPLY_PROP_ONLINE, &prop);
}

static int typec_attach_thread(void *data)
{
	struct mtk_ctd_info *mci = data;
	int ret = 0;
	bool attach;
	union power_supply_propval val;

	pr_info("%s: ++\n", __func__);
	while (!kthread_should_stop()) {
		wait_event(mci->attach_wq,
			   atomic_read(&mci->chrdet_start) > 0 ||
							 kthread_should_stop());
		if (kthread_should_stop())
			break;
		mutex_lock(&mci->attach_lock);
		attach = mci->typec_attach;
		atomic_set(&mci->chrdet_start, 0);
		mutex_unlock(&mci->attach_lock);
		val.intval = attach;
		pr_notice("%s bc12_sel:%d, attach:%d\n",
			  __func__, mci->bc12_sel, attach);
		if (mci->bc12_sel == MTK_CTD_BY_SUBPMIC) {
			ret = power_supply_set_property(mci->bc12_psy,
						POWER_SUPPLY_PROP_ONLINE, &val);
			if (ret < 0)
				dev_info(mci->dev, "%s: set online fail(%d)\n",
					__func__, ret);
		} else
			mtk_ext_get_charger_type(mci, attach);
	}
	return ret;
}

static void handle_typec_attach(struct mtk_ctd_info *mci,
				bool en)
{
	mutex_lock(&mci->attach_lock);
	mci->typec_attach = en;
	atomic_inc(&mci->chrdet_start);
	wake_up(&mci->attach_wq);
	mutex_unlock(&mci->attach_lock);
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_ctd_info *mci = (struct mtk_ctd_info *)container_of(nb,
						    struct mtk_ctd_info, pd_nb);

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_info("%s USB Plug in, pol = %d\n", __func__,
					noti->typec_state.polarity);
			handle_typec_attach(mci, true);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			pr_info("%s USB Plug out\n", __func__);
			if (mci->tcpc_kpoc) {
				pr_info("%s: typec unattached, power off\n",
					__func__);
				kernel_power_off();
			}
			handle_typec_attach(mci, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("%s Source_to_Sink\n", __func__);
			handle_typec_attach(mci, true);
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s Sink_to_Source\n", __func__);
			handle_typec_attach(mci, false);
		}
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}

static void mtk_ctd_parse_dt(struct mtk_ctd_info *mci)
{
	struct device_node *np = mci->dev->of_node;
	int ret;

	ret = of_property_read_u32(np, "bc12_sel", &mci->bc12_sel);
	if (ret < 0) {
		dev_info(mci->dev,
			 "%s: not defined, set default bc12_sel to 0\n",
			 __func__);
		mci->bc12_sel = MTK_CTD_BY_SUBPMIC;
	} else
		dev_info(mci->dev,
			 "%s: bc12_sel = %d\n", __func__, mci->bc12_sel);
}

static int mtk_ctd_probe(struct platform_device *pdev)
{
	struct mtk_ctd_info *mci;
	int ret;

	mci = devm_kzalloc(&pdev->dev, sizeof(*mci), GFP_KERNEL);
	if (!mci)
		return -ENOMEM;

	mci->dev = &pdev->dev;
	mci->bc12_sel = MTK_CTD_BY_SUBPMIC;
	init_waitqueue_head(&mci->attach_wq);
	atomic_set(&mci->chrdet_start, 0);
	mutex_init(&mci->attach_lock);
	platform_set_drvdata(pdev, mci);

	mtk_ctd_parse_dt(mci);

	mci->bc12_psy = devm_power_supply_get_by_phandle(&pdev->dev,
							"bc12");
	if (IS_ERR(mci->bc12_psy)) {
		dev_notice(&pdev->dev, "Failed to get charger psy\n");
		return PTR_ERR(mci->bc12_psy);
	}

	mci->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!mci->tcpc_dev) {
		pr_notice("%s get tcpc device type_c_port0 fail\n", __func__);
		return -ENODEV;
	}

	mci->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(mci->tcpc_dev, &mci->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_notice("%s: register tcpc notifer fail\n", __func__);
		return -EINVAL;
	}

	mci->attach_task = kthread_run(typec_attach_thread, mci,
				       "attach_thread");
	if (IS_ERR(mci->attach_task)) {
		pr_notice("%s: run typec attach kthread fail\n", __func__);
		return PTR_ERR(mci->attach_task);
	}
	dev_info(mci->dev, "%s: successfully\n", __func__);
	return 0;
}

static int mtk_ctd_remove(struct platform_device *pdev)
{
	struct mtk_ctd_info *mci = platform_get_drvdata(pdev);

	dev_dbg(mci->dev, "%s\n", __func__);
	kthread_stop(mci->attach_task);
	return 0;
}

static const struct of_device_id __maybe_unused mtk_ctd_of_id[] = {
	{ .compatible = "mediatek,mtk_ctd", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ctd_of_id);

static struct platform_driver mtk_ctd_driver = {
	.driver = {
		.name = "mtk_ctd",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ctd_of_id),
	},
	.probe = mtk_ctd_probe,
	.remove = mtk_ctd_remove,
};

static int __init mtk_ctd_init(void)
{
	return platform_driver_register(&mtk_ctd_driver);
}
device_initcall_sync(mtk_ctd_init);

static void __exit mtk_ctd_exit(void)
{
	platform_driver_unregister(&mtk_ctd_driver);
}
module_exit(mtk_ctd_exit);

MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_DESCRIPTION("MTK CHARGER TYPE DETECT Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MTK_CTD_DRV_VERSION);

// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/gpio.h>

#include "subpmic_irq.h"
#include "subpmic.h"
#include "../../../../misc/mediatek/typec/tcpc/inc/tcpci_typec.h"

#define SUBPMIC_CID_VERSION             "0.0.1"

struct subpmic_cid_device {
        struct i2c_client *client;
        struct device *dev;
        struct regmap *rmap;

        struct work_struct cid_work;
        atomic_t cid_pending;
        // pd contrl
        struct tcpc_device *tcpc;
};

#define SUBPMIC_CID_FLAG            BIT(7)

static void cid_detect_workfunc(struct work_struct *work)
{
        struct subpmic_cid_device *sc = container_of(work,
                        struct subpmic_cid_device, cid_work);
        int ret = 0;
        u8 state = 0;
        // must get tcpc device
        while(sc->tcpc == NULL) {
                sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
                if (sc->tcpc)
                        break;
                msleep(500);
        }

        do {
                ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_GEN_STATE, &state, 1);
                if (ret) {
                        dev_err(sc->dev, "failed to read irq state\n");
                        return;
                }
                if (state & SUBPMIC_CID_FLAG) {
                        dev_info(sc->dev, "cid detect plug-in\n");
                        tcpm_typec_change_role(sc->tcpc, TYPEC_ROLE_TRY_SNK);
                } else {
                        dev_info(sc->dev, "cid detect plug-out\n");
                        tcpm_typec_change_role(sc->tcpc, TYPEC_ROLE_SNK);
                }
                atomic_dec_if_positive(&sc->cid_pending);
        } while(atomic_read(&sc->cid_pending));
}

static irqreturn_t subpmic_cid_alert_handler(int irq, void *data)
{
        struct subpmic_cid_device *sc = data;
        atomic_inc(&sc->cid_pending);
        schedule_work(&sc->cid_work);
        return IRQ_HANDLED;
}

static int subpmic_cid_probe(struct platform_device *pdev)
{
        struct device *dev = &pdev->dev;
        struct subpmic_cid_device *sc;
        int ret;

        dev_info(dev, "%s (%s)\n", __func__, SUBPMIC_CID_VERSION);

        sc = devm_kzalloc(dev, sizeof(*sc), GFP_KERNEL);
        if (!sc)
                return -ENOMEM;
        sc->rmap = dev_get_regmap(dev->parent, NULL);
        if (!sc->rmap) {
                dev_err(dev, "failed to get regmap\n");
                return -ENODEV;
        }
        sc->dev = dev;
        platform_set_drvdata(pdev, sc);

        INIT_WORK(&sc->cid_work, cid_detect_workfunc);
        atomic_set(&sc->cid_pending, 0);

        ret = platform_get_irq_byname(to_platform_device(sc->dev), "CID");
        if (ret < 0) {
                dev_err(sc->dev, "failed to get irq CID\n");
                return ret;
        }

        ret = devm_request_threaded_irq(sc->dev, ret, NULL,
                        subpmic_cid_alert_handler, IRQF_ONESHOT, dev_name(sc->dev), sc);
        if (ret < 0) {
                dev_err(sc->dev, "failed to request irq CID\n");
                return ret;
        }

        schedule_work(&sc->cid_work);

        dev_info(dev, "%s probe success\n", __func__);

        return 0;
}

static int subpmic_cid_remove(struct platform_device *pdev)
{
        return 0;
}

static void subpmic_cid_shutdown(struct platform_device *pdev)
{

}

static const struct of_device_id subpmic_cid_of_match[] = {
        {.compatible = "southchip,subpmic_cid",},
        {},
};
MODULE_DEVICE_TABLE(of, subpmic_cid_of_match);

static struct platform_driver subpmic_cid_driver = {
        .driver = {
                .name = "subpmic_cid",
                .of_match_table = of_match_ptr(subpmic_cid_of_match),
        },
        .probe = subpmic_cid_probe,
        .remove = subpmic_cid_remove,
        .shutdown = subpmic_cid_shutdown,
};

module_platform_driver(subpmic_cid_driver);

MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("sc6601A CID driver");
MODULE_LICENSE("GPL v2");

/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <mach/gpiomux.h>
#include <mach/msm_pcie.h>
#include <mach/subsystem_restart.h>
#include <mach/ramdump.h>
#include <net/cnss.h>
#define subsys_to_drv(d) container_of(d, struct cnss_data, subsys_desc)

#define VREG_ON			1
#define VREG_OFF		0
#define WLAN_EN_HIGH		1
#define WLAN_EN_LOW		0
#define PCIE_LINK_UP		1
#define PCIE_LINK_DOWN		0

#define QCA6174_VENDOR_ID	(0x168C)
#define QCA6174_DEVICE_ID	(0x003E)

#define WLAN_VREG_NAME		"vdd-wlan"
#define WLAN_EN_GPIO_NAME	"wlan-en-gpio"
#define PM_OPTIONS		0

#define POWER_ON_DELAY		2000
#define WLAN_ENABLE_DELAY	10000

struct cnss_wlan_gpio_info {
	char *name;
	u32 num;
	bool state;
	bool init;
};

struct cnss_wlan_vreg_info {
	struct regulator *wlan_reg;
	bool state;
};

/* device_info is expected to be fully populated after cnss_config is invoked.
 * The function pointer callbacks are expected to be non null as well.
 */
static struct cnss_data {
	struct dev_info *device_info;
	struct subsys_device *subsys;
	struct subsys_desc    subsysdesc;
	struct ramdump_device *ramdump_dev;
	u16 unsafe_ch_count;
	u16 unsafe_ch_list[CNSS_MAX_CH_NUM];
	struct cnss_wlan_driver *driver;
	struct pci_dev *pdev;
	const struct pci_device_id *id;
	struct cnss_wlan_vreg_info vreg_info;
	struct cnss_wlan_gpio_info gpio_info;
	bool pcie_link_state;
	struct pci_saved_state *saved_state;
} *penv;

static int cnss_wlan_vreg_set(struct cnss_wlan_vreg_info *vreg_info, bool state)
{
	int ret;

	if (vreg_info->state == state) {
		pr_debug("Already wlan vreg state is %s\n",
			 state ? "enabled" : "disabled");
		return 0;
	}

	if (state)
		ret = regulator_enable(vreg_info->wlan_reg);
	else
		ret = regulator_disable(vreg_info->wlan_reg);

	if (!ret) {
		vreg_info->state = state;
		pr_debug("%s: wlan vreg is now %s\n", __func__,
			state ? "enabled" : "disabled");
	}

	return ret;
}

static int cnss_wlan_gpio_init(struct cnss_wlan_gpio_info *info)
{
	int ret = 0;

	ret = gpio_request(info->num, info->name);

	if (ret) {
		pr_err("can't get gpio %s ret %d\n", info->name, ret);
		goto err_gpio_req;
	}

	ret = gpio_direction_output(info->num, info->init);

	if (ret) {
		pr_err("can't set gpio direction %s ret %d\n", info->name, ret);
		goto err_gpio_dir;
	}
	info->state = info->init;

	return ret;

err_gpio_dir:
	gpio_free(info->num);

err_gpio_req:

	return ret;
}

static void cnss_wlan_gpio_set(struct cnss_wlan_gpio_info *info, bool state)
{
	if (info->state == state) {
		pr_debug("Already %s gpio is %s\n",
			 info->name, state ? "high" : "low");
		return;
	}

	gpio_set_value(info->num, state);
	info->state = state;

	pr_debug("%s: %s gpio is now %s\n", __func__,
		 info->name, info->state ? "enabled" : "disabled");
}

static int cnss_wlan_get_resources(struct platform_device *pdev)
{
	int ret = 0;
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;

	vreg_info->wlan_reg = regulator_get(&pdev->dev, WLAN_VREG_NAME);

	if (IS_ERR(vreg_info->wlan_reg)) {
		if (PTR_ERR(vreg_info->wlan_reg) == -EPROBE_DEFER)
			pr_err("%s: vreg probe defer\n", __func__);
		else
			pr_err("%s: vreg regulator get failed\n", __func__);
		ret = PTR_ERR(vreg_info->wlan_reg);
		goto err_reg_get;
	}

	ret = regulator_enable(vreg_info->wlan_reg);

	if (ret) {
		pr_err("%s: vreg initial vote failed\n", __func__);
		goto err_reg_enable;
	}
	vreg_info->state = VREG_ON;

	ret = of_get_named_gpio((&pdev->dev)->of_node,
				gpio_info->name, 0);

	if (ret >= 0) {
		gpio_info->num = ret;
		ret = 0;
	} else {
		if (ret == -EPROBE_DEFER)
			pr_debug("get WLAN_EN GPIO probe defer\n");
		else
			pr_err("can't get gpio %s ret %d",
			       gpio_info->name, ret);
		goto err_get_gpio;
	}

	ret = cnss_wlan_gpio_init(gpio_info);

	if (ret) {
		pr_err("gpio init failed\n");
		goto err_gpio_init;
	}

	return ret;

err_gpio_init:
err_get_gpio:
	regulator_disable(vreg_info->wlan_reg);
	vreg_info->state = VREG_OFF;

err_reg_enable:
	regulator_put(vreg_info->wlan_reg);

err_reg_get:

	return ret;
}

static void cnss_wlan_release_resources(void)
{
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;

	gpio_free(gpio_info->num);
	regulator_put(vreg_info->wlan_reg);
	gpio_info->state = WLAN_EN_LOW;
	vreg_info->state = VREG_OFF;
}

static u8 cnss_get_pci_dev_bus_number(struct pci_dev *pdev)
{
	return pdev->bus->number;
}

static int cnss_wlan_pci_probe(struct pci_dev *pdev,
			       const struct pci_device_id *id)
{
	int ret = 0;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;

	penv->pdev = pdev;
	penv->id = id;

	if (penv->pcie_link_state) {
		pci_save_state(pdev);
		penv->saved_state = pci_store_saved_state(pdev);

		ret = msm_pcie_pm_control(MSM_PCIE_SUSPEND,
					  cnss_get_pci_dev_bus_number(pdev),
					  NULL, NULL, PM_OPTIONS);
		if (ret) {
			pr_err("Failed to shutdown PCIe link\n");
			goto err_pcie_suspend;
		}
		penv->pcie_link_state = PCIE_LINK_DOWN;
	}

	cnss_wlan_gpio_set(gpio_info, WLAN_EN_LOW);
	ret = cnss_wlan_vreg_set(vreg_info, VREG_OFF);

	if (ret)
		pr_err("can't turn off wlan vreg\n");

err_pcie_suspend:

	return ret;
}

static void cnss_wlan_pci_remove(struct pci_dev *pdev)
{

}

static int cnss_wlan_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int ret = 0;
	struct cnss_wlan_driver *wdriver = penv->driver;

	if (!wdriver)
		goto out;

	if (wdriver->suspend)
		ret = wdriver->suspend(pdev, state);

out:
	return ret;
}

static int cnss_wlan_pci_resume(struct pci_dev *pdev)
{
	int ret = 0;
	struct cnss_wlan_driver *wdriver = penv->driver;

	if (!wdriver)
		goto out;

	if (wdriver->resume)
		ret = wdriver->resume(pdev);

out:
	return ret;
}

static DEFINE_PCI_DEVICE_TABLE(cnss_wlan_pci_id_table) = {
	{ QCA6174_VENDOR_ID, QCA6174_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, cnss_wlan_pci_id_table);

struct pci_driver cnss_wlan_pci_driver = {
	.name     = "cnss_wlan_pci",
	.id_table = cnss_wlan_pci_id_table,
	.probe    = cnss_wlan_pci_probe,
	.remove   = cnss_wlan_pci_remove,
	.suspend  = cnss_wlan_pci_suspend,
	.resume   = cnss_wlan_pci_resume,
};

int cnss_wlan_register_driver(struct cnss_wlan_driver *driver)
{
	int ret = 0;
	struct cnss_wlan_driver *wdrv;
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	struct pci_dev *pdev;

	if (!penv)
		return -ENODEV;

	wdrv = penv->driver;
	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;
	pdev = penv->pdev;

	if (!wdrv) {
		penv->driver = wdrv = driver;
	} else {
		pr_err("driver already registered\n");
		return -EEXIST;
	}

	ret = cnss_wlan_vreg_set(vreg_info, VREG_ON);
	if (ret) {
		pr_err("wlan vreg ON failed\n");
		goto err_wlan_vreg_on;
	}

	usleep(POWER_ON_DELAY);

	cnss_wlan_gpio_set(gpio_info, WLAN_EN_HIGH);
	usleep(WLAN_ENABLE_DELAY);

	if (!penv->pcie_link_state) {
		ret = msm_pcie_pm_control(MSM_PCIE_RESUME,
					  cnss_get_pci_dev_bus_number(pdev),
					  NULL, NULL, PM_OPTIONS);

		if (ret) {
			pr_err("PCIe link bring-up failed\n");
			goto err_pcie_link_up;
		}
		penv->pcie_link_state = PCIE_LINK_UP;
	}

	if (pdev && wdrv->probe) {
		if (penv->saved_state)
			pci_load_and_free_saved_state(pdev, &penv->saved_state);

		pci_restore_state(pdev);

		ret = wdrv->probe(pdev, penv->id);
		if (ret)
			goto err_wlan_probe;
	}

	return ret;

err_wlan_probe:
	pci_save_state(pdev);
	penv->saved_state = pci_store_saved_state(pdev);
	msm_pcie_pm_control(MSM_PCIE_SUSPEND,
			    cnss_get_pci_dev_bus_number(pdev),
			    NULL, NULL, PM_OPTIONS);
	penv->pcie_link_state = PCIE_LINK_DOWN;

err_pcie_link_up:
	cnss_wlan_gpio_set(gpio_info, WLAN_EN_LOW);
	cnss_wlan_vreg_set(vreg_info, VREG_OFF);

err_wlan_vreg_on:
	penv->driver = NULL;

	return ret;
}
EXPORT_SYMBOL(cnss_wlan_register_driver);

void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver)
{
	struct cnss_wlan_driver *wdrv;
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	struct pci_dev *pdev;

	if (!penv)
		return;

	wdrv = penv->driver;
	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;
	pdev = penv->pdev;

	if (!wdrv) {
		pr_err("driver not registered\n");
		return;
	}

	if (!pdev)
		goto cut_power;

	if (wdrv->remove)
		wdrv->remove(pdev);

	if (penv->pcie_link_state) {
		pci_save_state(pdev);
		penv->saved_state = pci_store_saved_state(pdev);

		if (msm_pcie_pm_control(MSM_PCIE_SUSPEND,
					cnss_get_pci_dev_bus_number(pdev),
					NULL, NULL, PM_OPTIONS)) {
			pr_debug("Failed to shutdown PCIe link\n");
			return;
		}
		penv->pcie_link_state = PCIE_LINK_DOWN;
	}

cut_power:
	penv->driver = NULL;

	cnss_wlan_gpio_set(gpio_info, WLAN_EN_LOW);

	if (cnss_wlan_vreg_set(vreg_info, VREG_OFF))
		pr_err("wlan vreg OFF failed\n");
}
EXPORT_SYMBOL(cnss_wlan_unregister_driver);

int cnss_config(struct dev_info *device_info)
{
	if (!penv)
		return -ENODEV;

	penv->device_info = device_info;
	return 0;
}
EXPORT_SYMBOL(cnss_config);

void cnss_deinit(void)
{
	if (penv && penv->device_info)
		penv->device_info = NULL;
}
EXPORT_SYMBOL(cnss_deinit);

void cnss_device_crashed(void)
{
	if (penv && penv->device_info) {
		subsys_set_crash_status(penv->subsys, true);
		subsystem_restart_dev(penv->subsys);
	}
}
EXPORT_SYMBOL(cnss_device_crashed);

int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	if (!penv)
		return -ENODEV;

	if ((!unsafe_ch_list) || (ch_count > CNSS_MAX_CH_NUM))
		return -EINVAL;

	penv->unsafe_ch_count = ch_count;

	if (ch_count != 0)
		memcpy((char *)penv->unsafe_ch_list, (char *)unsafe_ch_list,
			ch_count * sizeof(u16));

	return 0;
}
EXPORT_SYMBOL(cnss_set_wlan_unsafe_channel);

int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
					u16 *ch_count, u16 buf_len)
{
	if (!penv)
		return -ENODEV;

	if (!unsafe_ch_list || !ch_count)
		return -EINVAL;

	if (buf_len < (penv->unsafe_ch_count * sizeof(u16)))
		return -ENOMEM;

	*ch_count = penv->unsafe_ch_count;
	memcpy((char *)unsafe_ch_list, (char *)penv->unsafe_ch_list,
			penv->unsafe_ch_count * sizeof(u16));

	return 0;
}
EXPORT_SYMBOL(cnss_get_wlan_unsafe_channel);

static int cnss_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	if (penv && penv->device_info &&
			penv->device_info->dev_shutdown)
		return penv->device_info->dev_shutdown();

	return 0;
}

static int cnss_powerup(const struct subsys_desc *subsys)
{
	if (penv && penv->device_info &&
			penv->device_info->dev_powerup)
		return penv->device_info->dev_powerup();

	return 0;
}

static int cnss_ramdump(int enable, const struct subsys_desc *subsys)
{
	int result = 0;
	struct ramdump_segment segment;
	if (!penv || !penv->device_info || !penv->device_info->dump_buffer)
		return -ENODEV;

	if (!enable)
		return result;

	segment.address = (unsigned long) penv->device_info->dump_buffer;
	segment.size = penv->device_info->dump_size;
	result = do_ramdump(penv->ramdump_dev, &segment, 1);
	return result;
}

static void cnss_crash_shutdown(const struct subsys_desc *subsys)
{
	if (penv && penv->device_info &&
			penv->device_info->dev_crashshutdown)
		penv->device_info->dev_crashshutdown();
}

static int cnss_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (penv)
		return -ENODEV;

	penv = devm_kzalloc(&pdev->dev, sizeof(*penv), GFP_KERNEL);
	if (!penv)
		return -ENOMEM;
	penv->subsysdesc.name = "AR6320";
	penv->subsysdesc.owner = THIS_MODULE;
	penv->subsysdesc.shutdown = cnss_shutdown;
	penv->subsysdesc.powerup = cnss_powerup;
	penv->subsysdesc.ramdump = cnss_ramdump;
	penv->subsysdesc.crash_shutdown = cnss_crash_shutdown;
	penv->subsysdesc.dev = &pdev->dev;
	penv->subsys = subsys_register(&penv->subsysdesc);
	if (IS_ERR(penv->subsys)) {
		ret = PTR_ERR(penv->subsys);
		goto err_subsys_reg;
	}

	penv->ramdump_dev = create_ramdump_device(penv->subsysdesc.name,
				penv->subsysdesc.dev);
	if (!penv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump_create;
	}

	penv->gpio_info.name = WLAN_EN_GPIO_NAME;
	penv->gpio_info.num = 0;
	penv->gpio_info.state = WLAN_EN_LOW;
	penv->gpio_info.init = WLAN_EN_HIGH;
	penv->vreg_info.wlan_reg = NULL;
	penv->vreg_info.state = VREG_OFF;

	ret = cnss_wlan_get_resources(pdev);
	if (ret)
		goto err_get_wlan_res;

	penv->pcie_link_state = PCIE_LINK_UP;
	ret = pci_register_driver(&cnss_wlan_pci_driver);
	if (ret)
		goto err_pci_reg;

	return ret;

err_pci_reg:
	cnss_wlan_release_resources();

err_get_wlan_res:
	destroy_ramdump_device(penv->ramdump_dev);

err_ramdump_create:
	subsys_unregister(penv->subsys);

err_subsys_reg:
	penv = NULL;

	return ret;
}

static int cnss_remove(struct platform_device *pdev)
{
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;

	cnss_wlan_gpio_set(gpio_info, WLAN_EN_LOW);
	if (cnss_wlan_vreg_set(vreg_info, VREG_OFF))
		pr_err("Failed to turn OFF wlan vreg\n");
	cnss_wlan_release_resources();

	return 0;
}

static const struct of_device_id cnss_dt_match[] = {
	{.compatible = "qcom,cnss"},
	{}
};

MODULE_DEVICE_TABLE(of, cnss_dt_match);

static struct platform_driver cnss_driver = {
	.probe  = cnss_probe,
	.remove = cnss_remove,
	.driver = {
		.name = "cnss",
		.owner = THIS_MODULE,
		.of_match_table = cnss_dt_match,
	},
};

static int __init cnss_initialize(void)
{
	return platform_driver_register(&cnss_driver);
}

static void __exit cnss_exit(void)
{
	if (penv->ramdump_dev)
		destroy_ramdump_device(penv->ramdump_dev);
	subsys_unregister(penv->subsys);
	platform_driver_unregister(&cnss_driver);
}

module_init(cnss_initialize);
module_exit(cnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "CNSS Driver");

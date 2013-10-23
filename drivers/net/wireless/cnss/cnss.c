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
#include <mach/subsystem_restart.h>
#include <mach/ramdump.h>
#include <net/cnss.h>
#define subsys_to_drv(d) container_of(d, struct cnss_data, subsys_desc)

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
} *penv;

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
		goto err;
	}

	penv->ramdump_dev = create_ramdump_device(penv->subsysdesc.name,
				penv->subsysdesc.dev);
	if (!penv->ramdump_dev) {
		subsys_unregister(penv->subsys);
		ret = -ENOMEM;
		goto err;
	}
err:
	return ret;
}

static const struct of_device_id cnss_dt_match[] = {
	{.compatible = "qcom,cnss"},
	{}
};

MODULE_DEVICE_TABLE(of, cnss_dt_match);

static struct platform_driver cnss_driver = {
	.probe = cnss_probe,
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


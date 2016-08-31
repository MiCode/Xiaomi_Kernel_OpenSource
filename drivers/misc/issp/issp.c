/*
 * Copyright (c) 2006-2013, Cypress Semiconductor Corporation
 * All rights reserved.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ihex.h>
#include <linux/gpio.h>

#include "issp_priv.h"

#define DRIVER_NAME "issp"

static int issp_check_fw(struct issp_host *host)
{
	const struct ihex_binrec *rec;
	int checks = 0;
	int size;

	size = host->pdata->block_size * host->pdata->blocks;
	rec = (const struct ihex_binrec *)host->fw->data;
	while (rec) {
		int addr, len;

		addr = be32_to_cpu(rec->addr);
		len = be16_to_cpu(rec->len);

		if (addr + len == size)
			checks++;

		if (addr == ISSP_FW_SECURITY_ADDR) {
			host->security_rec = rec;
			checks++;
		}

		if (addr == ISSP_FW_CHECKSUM_ADDR) {
			host->checksum_fw = rec->data[0] << 8 | rec->data[1];
			checks++;
		}

		if (host->pdata->version_addr > addr &&
			host->pdata->version_addr < addr + len) {
			host->version_fw =
				rec->data[host->pdata->version_addr - addr];
			checks++;
		}

		if (checks == 4)
			break;
		rec = ihex_next_binrec(rec);
	}

	if (checks < 4)
		return -EINVAL;
	else
		return 0;
}

void issp_fw_rewind(struct issp_host *host)
{
	host->cur_rec = (const struct ihex_binrec *)host->fw->data;
	host->cur_idx = 0;
}

void issp_fw_seek_security(struct issp_host *host)
{
	host->cur_rec = host->security_rec;
	host->cur_idx = 0;
}

uint8_t issp_fw_get_byte(struct issp_host *host)
{
	uint8_t byte;
	byte = host->cur_rec->data[host->cur_idx];
	host->cur_idx++;
	if (host->cur_idx >= be16_to_cpu(host->cur_rec->len)) {
		host->cur_rec = ihex_next_binrec(host->cur_rec);
		host->cur_idx = 0;
	}

	return byte;
}

static int issp_need_update(struct issp_host *host, bool *update)
{
	uint8_t idx, addr, ver_uc;
	int ret;

	idx = host->pdata->version_addr / host->pdata->block_size;
	addr = host->pdata->version_addr - idx * host->pdata->block_size;
	ret = issp_read_block(host, idx, addr, &ver_uc, 1);
	if (ret == -EACCES) {
		dev_err(&host->pdev->dev,
			"Version Block is protected, force upgrade!\n");
		*update = true;
	} else if (ret == 1) {
		*update = (ver_uc < host->version_fw) ||
				((ver_uc != host->version_fw) &&
				host->pdata->force_update);

		if (*update)
			dev_info(&host->pdev->dev, "firmware needs upgrade, "\
				"version 0x%02x -> 0x%02x\n",
				ver_uc, host->version_fw);
		else
			dev_info(&host->pdev->dev,
				"firmware version %02x is latest!\n", ver_uc);
	} else
		return ret;

	return 0;
}

static int __init issp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct issp_platform_data *pdata = dev->platform_data;
	struct issp_host *host;
	bool update;
	int ret;

	if (!pdata || !gpio_is_valid(pdata->reset_gpio)
		|| !gpio_is_valid(pdata->data_gpio)
		|| !gpio_is_valid(pdata->clk_gpio)) {
		dev_err(dev, "Invalid platform data!");
		return -EINVAL;
	}

	ret = devm_gpio_request(dev, pdata->reset_gpio, "issp reset");
	if (!ret)
		ret = devm_gpio_request(dev, pdata->data_gpio, "issp data");
	if (!ret)
		ret = devm_gpio_request(dev, pdata->clk_gpio, "issp clock");
	if (ret)
		return ret;

	/* set gpio directions */
	gpio_direction_output(pdata->reset_gpio, 0);
	gpio_direction_input(pdata->data_gpio);
	gpio_direction_output(pdata->clk_gpio, 0);

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->pdev = pdev;
	host->pdata = pdata;
	ret = request_ihex_firmware(&host->fw, pdata->fw_name, dev);
	if (ret) {
		dev_err(dev, "Request firmware %s failed!\n", pdata->fw_name);
		return ret;
	}

	ret = issp_check_fw(host);
	if (ret) {
		dev_err(dev, "Firmware %s invalid!\n", pdata->fw_name);
		goto err;
	}

	issp_uc_program(host);

	if (host->si_id[0] != pdata->si_id[0] ||
		host->si_id[1] != pdata->si_id[1] ||
		host->si_id[2] != pdata->si_id[2] ||
		host->si_id[3] != pdata->si_id[3]) {
		dev_err(dev, "Sillicon ID check failed!\n");
		goto err_id;
	}

	ret = issp_need_update(host, &update);
	if (ret)
		goto err_id;
	if (update) {
		ret = issp_program(host);
		if (!ret)
			dev_info(dev, "Firmware update successfully!\n");
		else
			dev_err(dev, "Firmware update failed!\n");
	}

err_id:
	issp_uc_run(host);

err:
	gpio_direction_input(pdata->data_gpio);
	gpio_direction_input(pdata->clk_gpio);
	release_firmware(host->fw);
	devm_kfree(dev, host);

	return 0;
}

static int __exit issp_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver issp_driver = {
	.remove		= __exit_p(issp_remove),
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	}
};

static int __init issp_init(void)
{
	return platform_driver_probe(&issp_driver, issp_probe);
}
subsys_initcall(issp_init);

static void __exit issp_exit(void)
{
	platform_driver_unregister(&issp_driver);
}
module_exit(issp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Zhao, nVidia <rizhao@nvidia.com>");
MODULE_DESCRIPTION("ISSP driver");

/*
 * Secure Digital Host Controller Interface ACPI driver.
 *
 * Copyright (c) 2012, Intel Corporation.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/compiler.h>
#include <linux/stddef.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>

#include <linux/mmc/host.h>
#include <linux/mmc/pm.h>
#include <linux/mmc/sdhci.h>
#include <linux/mmc/slot-gpio.h>

#include "sdhci.h"

enum {
	SDHCI_ACPI_SD_CD	= BIT(0),
	SDHCI_ACPI_RUNTIME_PM	= BIT(1),
};

struct sdhci_acpi_chip {
	const struct	sdhci_ops *ops;
	unsigned int	quirks;
	unsigned int	quirks2;
	unsigned long	caps;
	unsigned int	caps2;
	mmc_pm_flag_t	pm_caps;
};

struct sdhci_acpi_slot {
	const struct	sdhci_acpi_chip *chip;
	unsigned int	quirks;
	unsigned int	quirks2;
	unsigned long	caps;
	unsigned int	caps2;
	mmc_pm_flag_t	pm_caps;
	unsigned int	flags;
	int (*probe_slot) (struct platform_device *);
	int (*remove_slot)(struct platform_device *);
};

struct sdhci_acpi_host {
	struct sdhci_host		*host;
	const struct sdhci_acpi_slot	*slot;
	struct platform_device		*pdev;
	bool				use_runtime_pm;
	unsigned int			autosuspend_delay;
};

static inline bool sdhci_acpi_flag(struct sdhci_acpi_host *c, unsigned int flag)
{
	return c->slot && (c->slot->flags & flag);
}

static int sdhci_acpi_enable_dma(struct sdhci_host *host)
{
	return 0;
}

static void sdhci_acpi_int_hw_reset(struct sdhci_host *host)
{
	u8 reg;

	reg = sdhci_readb(host, SDHCI_POWER_CONTROL);
	reg |= 0x10;
	sdhci_writeb(host, reg, SDHCI_POWER_CONTROL);
	/* For eMMC, minimum is 1us but give it 9us for good measure */
	udelay(9);
	reg &= ~0x10;
	sdhci_writeb(host, reg, SDHCI_POWER_CONTROL);
	/* For eMMC, minimum is 200us but give it 300us for good measure */
	usleep_range(300, 1000);
}

static const struct sdhci_ops sdhci_acpi_ops_dflt = {
	.enable_dma = sdhci_acpi_enable_dma,
};

static const struct sdhci_ops sdhci_acpi_ops_int = {
	.enable_dma = sdhci_acpi_enable_dma,
	.hw_reset   = sdhci_acpi_int_hw_reset,
};

static const struct sdhci_acpi_chip sdhci_acpi_chip_int = {
	.ops = &sdhci_acpi_ops_int,
};

/*
 * This probe slot routine is being added to address an issue on the host
 * controller's IP where hangs will occur of the platform enters a C state
 * below C2 while there is an outstanding write transaction. This is observed
 * on Baytrail based platforms, and should only be enabled on platforms with
 * host controller IP blocks that exhibit this.  We're calling based on the
 * ACPI-ID of the IP block.
 *
 * Intel SDHCI host controller can support up to 4Mbytes request size in ADMA
 * mode, which is much larger than the default 512KBytes. Change to 4Mbytes
 * per the performance requirements
 */
static int sdhci_acpi_probe_slot(struct platform_device *pdev)
{
	struct sdhci_acpi_host *c = platform_get_drvdata(pdev);
	struct sdhci_host *host;

	if (!c || !c->host)
		return 0;

	host = c->host;
	host->mmc->qos = kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	pm_qos_add_request(host->mmc->qos, PM_QOS_CPU_DMA_LATENCY,
					PM_QOS_DEFAULT_VALUE);

	/* change to 4Mbytes */
	host->mmc->max_req_size = 4 * 1024 * 1024;

	return 0;
}

static int sdhci_acpi_sdio_probe_slot(struct platform_device *pdev)
{
	struct sdhci_acpi_host *c = platform_get_drvdata(pdev);
	struct sdhci_host *host;

	if (!c || !c->host)
		return 0;

	/* increase the auto suspend delay for SDIO to be 500ms.
	 * It can fix some latency issues after enabling rpm.
	 */
	c->autosuspend_delay = 500;

	return sdhci_acpi_probe_slot(pdev);
}

static int sdhci_acpi_sd_probe_slot(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_device *device;

	if (!acpi_bus_get_device(handle, &device))
		device->power.flags.power_resources = 0;

	return sdhci_acpi_probe_slot(pdev);
}

static int sdhci_acpi_remove_slot(struct platform_device *pdev)
{
	struct sdhci_acpi_host *c = platform_get_drvdata(pdev);
	struct sdhci_host *host;

	if (!c || !c->host)
		return 0;

	host = c->host;

	if (host->mmc && host->mmc->qos)
		kfree(host->mmc->qos);

	return 0;
}

static const struct sdhci_acpi_slot sdhci_acpi_slot_int_emmc = {
	.chip    = &sdhci_acpi_chip_int,
	.caps    = MMC_CAP_8_BIT_DATA | MMC_CAP_NONREMOVABLE | MMC_CAP_HW_RESET
		| MMC_CAP_1_8V_DDR,
	.caps2   = MMC_CAP2_HC_ERASE_SZ | MMC_CAP2_POLL_R1B_BUSY |
		MMC_CAP2_CACHE_CTRL | MMC_CAP2_HS200_1_8V_SDR |
		MMC_CAP2_CAN_DO_CMDQ | MMC_CAP2_PACKED_CMD,
	.flags   = SDHCI_ACPI_RUNTIME_PM,
	.quirks2 = SDHCI_QUIRK2_TUNING_POLL | SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.pm_caps = MMC_PM_TUNING_AFTER_RTRESUME,
	.probe_slot = sdhci_acpi_probe_slot,
	.remove_slot = sdhci_acpi_remove_slot,
};

static const struct sdhci_acpi_slot sdhci_acpi_slot_int_sdio = {
	.quirks2 = SDHCI_QUIRK2_HOST_OFF_CARD_ON |
		   SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_FAKE_VDD,
	.caps    = MMC_CAP_NONREMOVABLE | MMC_CAP_POWER_OFF_CARD,
	.flags   = SDHCI_ACPI_RUNTIME_PM,
	.pm_caps = MMC_PM_KEEP_POWER,
	.probe_slot = sdhci_acpi_sdio_probe_slot,
	.remove_slot = sdhci_acpi_remove_slot,
};

static const struct sdhci_acpi_slot sdhci_acpi_slot_int_sd = {
	.caps2   = MMC_CAP2_FULL_PWR_CYCLE,
	.flags   = SDHCI_ACPI_SD_CD | SDHCI_ACPI_RUNTIME_PM,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.probe_slot = sdhci_acpi_sd_probe_slot,
	.remove_slot = sdhci_acpi_remove_slot,
};

struct sdhci_acpi_uid_slot {
	const char *hid;
	const char *uid;
	const struct sdhci_acpi_slot *slot;
};

static const struct sdhci_acpi_uid_slot sdhci_acpi_uids[] = {
	{ "80860F14" , "1" , &sdhci_acpi_slot_int_emmc },
	{ "80860F14" , "3" , &sdhci_acpi_slot_int_sd   },
	{ "INT33BB"  , "2" , &sdhci_acpi_slot_int_sdio },
	{ "INT33BB"  , "3" , &sdhci_acpi_slot_int_sd   },
	{ "INT33C6"  , NULL, &sdhci_acpi_slot_int_sdio },
	{ "INT3436"  , NULL, &sdhci_acpi_slot_int_sdio },
	{ "PNP0D40"  },
	{ },
};

static const struct acpi_device_id sdhci_acpi_ids[] = {
	{ "80860F14" },
	{ "INT33BB"  },
	{ "INT33C6"  },
	{ "INT3436"  },
	{ "PNP0D40"  },
	{ },
};
MODULE_DEVICE_TABLE(acpi, sdhci_acpi_ids);

static const struct sdhci_acpi_slot *sdhci_acpi_get_slot_by_ids(const char *hid,
								const char *uid)
{
	const struct sdhci_acpi_uid_slot *u;

	for (u = sdhci_acpi_uids; u->hid; u++) {
		if (strcmp(u->hid, hid))
			continue;
		if (!u->uid)
			return u->slot;
		if (uid && !strcmp(u->uid, uid))
			return u->slot;
	}
	return NULL;
}

static const struct sdhci_acpi_slot *sdhci_acpi_get_slot(acpi_handle handle,
							 const char *hid)
{
	const struct sdhci_acpi_slot *slot;
	struct acpi_device_info *info;
	const char *uid = NULL;
	acpi_status status;

	status = acpi_get_object_info(handle, &info);
	if (!ACPI_FAILURE(status) && (info->valid & ACPI_VALID_UID))
		uid = info->unique_id.string;

	slot = sdhci_acpi_get_slot_by_ids(hid, uid);

	kfree(info);
	return slot;
}

#ifdef CONFIG_PM_RUNTIME

static int sdhci_acpi_add_own_cd(struct device *dev, struct mmc_host *mmc)
{
	struct gpio_desc *desc;
	int err, gpio;

	desc = devm_gpiod_get_index(dev, "sd_cd", 0);
	if (IS_ERR(desc)) {
		err = PTR_ERR(desc);
		goto out;
	}

	gpio = desc_to_gpio(desc);
	devm_gpiod_put(dev, desc);

	err = mmc_gpio_request_cd(mmc, gpio, 0);
	if (err)
		goto out;

	return 0;

out:
	dev_warn(dev, "failed to setup card detect wake up\n");
	return err;
}

#else

static int sdhci_acpi_add_own_cd(struct device *dev, struct mmc_host *mmc)
{
	return 0;
}

#endif

static int sdhci_acpi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_device *device, *child;
	struct sdhci_acpi_host *c;
	struct sdhci_host *host;
	struct resource *iomem;
	resource_size_t len;
	const char *hid;
	int err;

	if (acpi_bus_get_device(handle, &device))
		return -ENODEV;

	if (IS_ENABLED(CONFIG_MMC_SDHCI_ACPI_FORCE_POWER_ON)) {
		/* Power on the SDHCI controller and its children */
		acpi_device_fix_up_power(device);
		list_for_each_entry(child, &device->children, node)
			acpi_device_fix_up_power(child);
	}

	if (acpi_bus_get_status(device) || !device->status.present)
		return -ENODEV;

	hid = acpi_device_hid(device);

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iomem)
		return -ENOMEM;

	len = resource_size(iomem);
	if (len < 0x100)
		dev_err(dev, "Invalid iomem size!\n");

	if (!devm_request_mem_region(dev, iomem->start, len, dev_name(dev)))
		return -ENOMEM;

	host = sdhci_alloc_host(dev, sizeof(struct sdhci_acpi_host));
	if (IS_ERR(host))
		return PTR_ERR(host);

	c = sdhci_priv(host);
	c->host = host;
	c->slot = sdhci_acpi_get_slot(handle, hid);
	c->pdev = pdev;
	c->use_runtime_pm = sdhci_acpi_flag(c, SDHCI_ACPI_RUNTIME_PM);
	c->autosuspend_delay = 0;

	platform_set_drvdata(pdev, c);

	host->hw_name	= "ACPI";
	host->ops	= &sdhci_acpi_ops_dflt;
	host->irq	= platform_get_irq(pdev, 0);

	host->ioaddr = devm_ioremap_nocache(dev, iomem->start,
					    resource_size(iomem));
	if (host->ioaddr == NULL) {
		err = -ENOMEM;
		goto err_free;
	}

	if (!dev->dma_mask) {
		u64 dma_mask;

		if (sdhci_readl(host, SDHCI_CAPABILITIES) & SDHCI_CAN_64BIT) {
			/* 64-bit DMA is not supported at present */
			dma_mask = DMA_BIT_MASK(32);
		} else {
			dma_mask = DMA_BIT_MASK(32);
		}

		err = dma_coerce_mask_and_coherent(dev, dma_mask);
		if (err)
			goto err_free;
	}

	if (c->slot) {
		if (c->slot->probe_slot) {
			err = c->slot->probe_slot(pdev);
			if (err)
				goto err_free;
		}
		if (c->slot->chip) {
			host->ops            = c->slot->chip->ops;
			host->quirks        |= c->slot->chip->quirks;
			host->quirks2       |= c->slot->chip->quirks2;
			host->mmc->caps     |= c->slot->chip->caps;
			host->mmc->caps2    |= c->slot->chip->caps2;
			host->mmc->pm_caps  |= c->slot->chip->pm_caps;
		}
		host->quirks        |= c->slot->quirks;
		host->quirks2       |= c->slot->quirks2;
		host->mmc->caps     |= c->slot->caps;
		host->mmc->caps2    |= c->slot->caps2;
		host->mmc->pm_caps  |= c->slot->pm_caps;
	}

	host->mmc->caps2 |= MMC_CAP2_NO_PRESCAN_POWERUP;

	/* use async mode for suspend/resume */
	device_enable_async_suspend(dev);

	err = sdhci_add_host(host);
	if (err)
		goto remove_slot;

	if (sdhci_acpi_flag(c, SDHCI_ACPI_SD_CD)) {
		if (sdhci_acpi_add_own_cd(dev, host->mmc))
			c->use_runtime_pm = false;
	}

	if (c->use_runtime_pm) {
		pm_runtime_set_active(dev);
		if (c->autosuspend_delay)
			pm_runtime_set_autosuspend_delay(dev, c->autosuspend_delay);
		else
			pm_runtime_set_autosuspend_delay(dev, 50);
		pm_runtime_use_autosuspend(dev);
		pm_runtime_enable(dev);
	}

	return 0;

remove_slot:
	if (c->slot && c->slot->remove_slot)
		c->slot->remove_slot(pdev);
err_free:
	sdhci_free_host(c->host);
	return err;
}

static int sdhci_acpi_remove(struct platform_device *pdev)
{
	struct sdhci_acpi_host *c = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int dead;

	if (c->use_runtime_pm) {
		pm_runtime_get_sync(dev);
		pm_runtime_disable(dev);
		pm_runtime_put_noidle(dev);
	}

	if (c->slot && c->slot->remove_slot)
		c->slot->remove_slot(pdev);

	dead = (sdhci_readl(c->host, SDHCI_INT_STATUS) == ~0);
	sdhci_remove_host(c->host, dead);
	sdhci_free_host(c->host);

	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int sdhci_acpi_suspend(struct device *dev)
{
	struct sdhci_acpi_host *c = dev_get_drvdata(dev);

	return sdhci_suspend_host(c->host);
}

static int sdhci_acpi_resume(struct device *dev)
{
	struct sdhci_acpi_host *c = dev_get_drvdata(dev);

	return sdhci_resume_host(c->host);
}

#else

#define sdhci_acpi_suspend	NULL
#define sdhci_acpi_resume	NULL

#endif

#ifdef CONFIG_PM_RUNTIME

static int sdhci_acpi_runtime_suspend(struct device *dev)
{
	struct sdhci_acpi_host *c = dev_get_drvdata(dev);

	return sdhci_runtime_suspend_host(c->host);
}

static int sdhci_acpi_runtime_resume(struct device *dev)
{
	struct sdhci_acpi_host *c = dev_get_drvdata(dev);

	return sdhci_runtime_resume_host(c->host);
}

static int sdhci_acpi_runtime_idle(struct device *dev)
{
	return 0;
}

#else

#define sdhci_acpi_runtime_suspend	NULL
#define sdhci_acpi_runtime_resume	NULL
#define sdhci_acpi_runtime_idle		NULL

#endif

static const struct dev_pm_ops sdhci_acpi_pm_ops = {
	.suspend		= sdhci_acpi_suspend,
	.resume			= sdhci_acpi_resume,
	.runtime_suspend	= sdhci_acpi_runtime_suspend,
	.runtime_resume		= sdhci_acpi_runtime_resume,
	.runtime_idle		= sdhci_acpi_runtime_idle,
};

static struct platform_driver sdhci_acpi_driver = {
	.driver = {
		.name			= "sdhci-acpi",
		.owner			= THIS_MODULE,
		.acpi_match_table	= sdhci_acpi_ids,
		.pm			= &sdhci_acpi_pm_ops,
	},
	.probe	= sdhci_acpi_probe,
	.remove	= sdhci_acpi_remove,
};

module_platform_driver(sdhci_acpi_driver);

MODULE_DESCRIPTION("Secure Digital Host Controller Interface ACPI driver");
MODULE_AUTHOR("Adrian Hunter");
MODULE_LICENSE("GPL v2");

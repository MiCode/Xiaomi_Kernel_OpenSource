/*
 *  linux/drivers/mmc/core/bus.c
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright (C) 2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  MMC card bus driver model
 */

#include <linux/export.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/pm_runtime.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include "core.h"
#include "sdio_cis.h"
#include "bus.h"

#define to_mmc_driver(d)	container_of(d, struct mmc_driver, drv)
#define RUNTIME_SUSPEND_DELAY_MS 10000

static ssize_t mmc_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mmc_card *card = mmc_dev_to_card(dev);

	switch (card->type) {
	case MMC_TYPE_MMC:
		return sprintf(buf, "MMC\n");
	case MMC_TYPE_SD:
		return sprintf(buf, "SD\n");
	case MMC_TYPE_SDIO:
		return sprintf(buf, "SDIO\n");
	case MMC_TYPE_SD_COMBO:
		return sprintf(buf, "SDcombo\n");
	default:
		return -EFAULT;
	}
}

static struct device_attribute mmc_dev_attrs[] = {
	__ATTR(type, S_IRUGO, mmc_type_show, NULL),
	__ATTR_NULL,
};

/*
 * This currently matches any MMC driver to any MMC card - drivers
 * themselves make the decision whether to drive this card in their
 * probe method.
 */
static int mmc_bus_match(struct device *dev, struct device_driver *drv)
{
	return 1;
}

static int
mmc_bus_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct mmc_card *card = mmc_dev_to_card(dev);
	const char *type;
	int retval = 0;

	switch (card->type) {
	case MMC_TYPE_MMC:
		type = "MMC";
		break;
	case MMC_TYPE_SD:
		type = "SD";
		break;
	case MMC_TYPE_SDIO:
		type = "SDIO";
		break;
	case MMC_TYPE_SD_COMBO:
		type = "SDcombo";
		break;
	default:
		type = NULL;
	}

	if (type) {
		retval = add_uevent_var(env, "MMC_TYPE=%s", type);
		if (retval)
			return retval;
	}

	retval = add_uevent_var(env, "MMC_NAME=%s", mmc_card_name(card));
	if (retval)
		return retval;

	/*
	 * Request the mmc_block device.  Note: that this is a direct request
	 * for the module it carries no information as to what is inserted.
	 */
	retval = add_uevent_var(env, "MODALIAS=mmc:block");

	return retval;
}

static int mmc_bus_probe(struct device *dev)
{
	struct mmc_driver *drv = to_mmc_driver(dev->driver);
	struct mmc_card *card = mmc_dev_to_card(dev);

	return drv->probe(card);
}

static int mmc_bus_remove(struct device *dev)
{
	struct mmc_driver *drv = to_mmc_driver(dev->driver);
	struct mmc_card *card = mmc_dev_to_card(dev);

	drv->remove(card);

	return 0;
}

static void mmc_bus_shutdown(struct device *dev)
{
	struct mmc_driver *drv = to_mmc_driver(dev->driver);
	struct mmc_card *card = mmc_dev_to_card(dev);

	if (!drv) {
		pr_debug("%s: %s: drv is NULL\n", dev_name(dev), __func__);
		return;
	}

	if (!card) {
		pr_debug("%s: %s: card is NULL\n", dev_name(dev), __func__);
		return;
	}

	if (drv->shutdown)
		drv->shutdown(card);
}

#ifdef CONFIG_PM_SLEEP
static int mmc_bus_suspend(struct device *dev)
{
	struct mmc_driver *drv = to_mmc_driver(dev->driver);
	struct mmc_card *card = mmc_dev_to_card(dev);
	int ret = 0;

	if (dev->driver && drv->suspend)
		ret = drv->suspend(card);
	return ret;
}

static int mmc_bus_resume(struct device *dev)
{
	struct mmc_driver *drv = to_mmc_driver(dev->driver);
	struct mmc_card *card = mmc_dev_to_card(dev);
	int ret = 0;

	if (dev->driver && drv->resume)
		ret = drv->resume(card);
	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME

static int mmc_runtime_suspend(struct device *dev)
{
	struct mmc_card *card = mmc_dev_to_card(dev);

	if (mmc_use_core_runtime_pm(card->host)) {
		/*
		 * If idle time bkops is running on the card, let's not get
		 * into suspend.
		 */
		if (mmc_card_doing_bkops(card) && mmc_card_is_prog_state(card))
			return -EBUSY;
		else
			return 0;
	} else {
		return mmc_power_save_host(card->host);
	}
}

static int mmc_runtime_resume(struct device *dev)
{
	struct mmc_card *card = mmc_dev_to_card(dev);

	if (mmc_use_core_runtime_pm(card->host))
		return 0;
	else
		return mmc_power_restore_host(card->host);
}

static int mmc_runtime_idle(struct device *dev)
{
	struct mmc_card *card = mmc_dev_to_card(dev);
	struct mmc_host *host = card->host;
	int ret = 0;

	if (mmc_use_core_runtime_pm(card->host)) {
		ret = pm_schedule_suspend(dev, card->idle_timeout);
		if ((ret < 0) && (dev->power.runtime_error ||
				  dev->power.disable_depth > 0)) {
			pr_err("%s: %s: %s: pm_schedule_suspend failed: err: %d\n",
			       mmc_hostname(host), __func__, dev_name(dev),
			       ret);
			return ret;
		}
	}

	return ret;
}

#endif /* !CONFIG_PM_RUNTIME */

static const struct dev_pm_ops mmc_bus_pm_ops = {
	SET_RUNTIME_PM_OPS(mmc_runtime_suspend, mmc_runtime_resume,
			mmc_runtime_idle)
	SET_SYSTEM_SLEEP_PM_OPS(mmc_bus_suspend, mmc_bus_resume)
};

static ssize_t show_rpm_delay(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct mmc_card *card = mmc_dev_to_card(dev);

	if (!card) {
		pr_err("%s: %s: card is NULL\n", dev_name(dev), __func__);
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", card->idle_timeout);
}

static ssize_t store_rpm_delay(struct device *dev, struct device_attribute
			       *attr, const char *buf, size_t count)
{
	struct mmc_card *card = mmc_dev_to_card(dev);
	unsigned int delay;

	if (!card) {
		pr_err("%s: %s: card is NULL\n", dev_name(dev), __func__);
		return -EINVAL;
	}

	if (!kstrtou32(buf, 0, &delay)) {
		if (delay < 2000) {
			pr_err("%s: %s: less than 2 sec delay is unsupported\n",
			       mmc_hostname(card->host), __func__);
			return -EINVAL;
		}
		card->idle_timeout = delay;
	}

	return count;
}

static struct bus_type mmc_bus_type = {
	.name		= "mmc",
	.dev_attrs	= mmc_dev_attrs,
	.match		= mmc_bus_match,
	.uevent		= mmc_bus_uevent,
	.probe		= mmc_bus_probe,
	.remove		= mmc_bus_remove,
	.shutdown        = mmc_bus_shutdown,
	.pm		= &mmc_bus_pm_ops,
};

int mmc_register_bus(void)
{
	return bus_register(&mmc_bus_type);
}

void mmc_unregister_bus(void)
{
	bus_unregister(&mmc_bus_type);
}

/**
 *	mmc_register_driver - register a media driver
 *	@drv: MMC media driver
 */
int mmc_register_driver(struct mmc_driver *drv)
{
	drv->drv.bus = &mmc_bus_type;
	return driver_register(&drv->drv);
}

EXPORT_SYMBOL(mmc_register_driver);

/**
 *	mmc_unregister_driver - unregister a media driver
 *	@drv: MMC media driver
 */
void mmc_unregister_driver(struct mmc_driver *drv)
{
	drv->drv.bus = &mmc_bus_type;
	driver_unregister(&drv->drv);
}

EXPORT_SYMBOL(mmc_unregister_driver);

static void mmc_release_card(struct device *dev)
{
	struct mmc_card *card = mmc_dev_to_card(dev);

	sdio_free_common_cis(card);

	if (card->info)
		kfree(card->info);

	kfree(card);
}

/*
 * Allocate and initialise a new MMC card structure.
 */
struct mmc_card *mmc_alloc_card(struct mmc_host *host, struct device_type *type)
{
	struct mmc_card *card;

	card = kzalloc(sizeof(struct mmc_card), GFP_KERNEL);
	if (!card)
		return ERR_PTR(-ENOMEM);

	card->host = host;

	device_initialize(&card->dev);

	card->dev.parent = mmc_classdev(host);
	card->dev.bus = &mmc_bus_type;
	card->dev.release = mmc_release_card;
	card->dev.type = type;

	spin_lock_init(&card->bkops_info.bkops_stats.lock);
	spin_lock_init(&card->wr_pack_stats.lock);

	return card;
}

/*
 * Register a new MMC card with the driver model.
 */
int mmc_add_card(struct mmc_card *card)
{
	int ret;
	const char *type;
	const char *uhs_bus_speed_mode = "";
	static const char *const uhs_speeds[] = {
		[UHS_SDR12_BUS_SPEED] = "SDR12 ",
		[UHS_SDR25_BUS_SPEED] = "SDR25 ",
		[UHS_SDR50_BUS_SPEED] = "SDR50 ",
		[UHS_SDR104_BUS_SPEED] = "SDR104 ",
		[UHS_DDR50_BUS_SPEED] = "DDR50 ",
	};


	dev_set_name(&card->dev, "%s:%04x", mmc_hostname(card->host), card->rca);

	switch (card->type) {
	case MMC_TYPE_MMC:
		type = "MMC";
		break;
	case MMC_TYPE_SD:
		type = "SD";
		if (mmc_card_blockaddr(card)) {
			if (mmc_card_ext_capacity(card))
				type = "SDXC";
			else
				type = "SDHC";
		}
		break;
	case MMC_TYPE_SDIO:
		type = "SDIO";
		break;
	case MMC_TYPE_SD_COMBO:
		type = "SD-combo";
		if (mmc_card_blockaddr(card))
			type = "SDHC-combo";
		break;
	default:
		type = "?";
		break;
	}

	if (mmc_sd_card_uhs(card) &&
		(card->sd_bus_speed < ARRAY_SIZE(uhs_speeds)))
		uhs_bus_speed_mode = uhs_speeds[card->sd_bus_speed];

	if (mmc_host_is_spi(card->host)) {
		pr_info("%s: new %s%s%s card on SPI\n",
			mmc_hostname(card->host),
			mmc_card_highspeed(card) ? "high speed " : "",
			mmc_card_ddr_mode(card) ? "DDR " : "",
			type);
	} else {
		pr_info("%s: new %s%s%s%s%s%s card at address %04x\n",
			mmc_hostname(card->host),
			mmc_card_uhs(card) ? "ultra high speed " :
			(mmc_card_highspeed(card) ? "high speed " : ""),
			(mmc_card_hs400(card) ? "HS400 " : ""),
			(mmc_card_hs200(card) ? "HS200 " : ""),
			mmc_card_ddr_mode(card) ? "DDR " : "",
			uhs_bus_speed_mode, type, card->rca);
	}

#ifdef CONFIG_DEBUG_FS
	mmc_add_card_debugfs(card);
#endif
	mmc_init_context_info(card->host);

	ret = pm_runtime_set_active(&card->dev);
	if (ret)
		pr_err("%s: %s: failed setting runtime active: ret: %d\n",
		       mmc_hostname(card->host), __func__, ret);
	else if (!mmc_card_sdio(card) && mmc_use_core_runtime_pm(card->host))
		pm_runtime_enable(&card->dev);

	if (mmc_card_sdio(card)) {
		ret = device_init_wakeup(&card->dev, true);
		if (ret)
			pr_err("%s: %s: failed to init wakeup: %d\n",
			       mmc_hostname(card->host), __func__, ret);
	}
	ret = device_add(&card->dev);
	if (ret)
		return ret;

	if (mmc_use_core_runtime_pm(card->host) && !mmc_card_sdio(card)) {
		card->rpm_attrib.show = show_rpm_delay;
		card->rpm_attrib.store = store_rpm_delay;
		sysfs_attr_init(&card->rpm_attrib.attr);
		card->rpm_attrib.attr.name = "runtime_pm_timeout";
		card->rpm_attrib.attr.mode = S_IRUGO | S_IWUSR;

		ret = device_create_file(&card->dev, &card->rpm_attrib);
		if (ret)
			pr_err("%s: %s: creating runtime pm sysfs entry: failed: %d\n",
			       mmc_hostname(card->host), __func__, ret);
		/* Default timeout is 10 seconds */
		card->idle_timeout = RUNTIME_SUSPEND_DELAY_MS;
	}

	mmc_card_set_present(card);

	return 0;
}

/*
 * Unregister a new MMC card with the driver model, and
 * (eventually) free it.
 */
void mmc_remove_card(struct mmc_card *card)
{
#ifdef CONFIG_DEBUG_FS
	mmc_remove_card_debugfs(card);
#endif

	if (mmc_card_present(card)) {
		if (mmc_host_is_spi(card->host)) {
			pr_info("%s: SPI card removed\n",
				mmc_hostname(card->host));
		} else {
			pr_info("%s: card %04x removed\n",
				mmc_hostname(card->host), card->rca);
		}
		device_del(&card->dev);
	}

	kfree(card->wr_pack_stats.packing_events);
	kfree(card->cached_ext_csd);

	put_device(&card->dev);
}


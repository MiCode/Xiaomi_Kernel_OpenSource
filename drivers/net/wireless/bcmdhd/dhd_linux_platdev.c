/*
 * Linux platform device for DHD WLAN adapter
 *
 * Copyright (C) 1999-2014, Broadcom Corporation
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_linux_platdev.c 401742 2013-05-13 15:03:21Z $
 */
#include <typedefs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <bcmutils.h>
#include <linux_osl.h>
#include <dhd_dbg.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_bus.h>
#include <dhd_linux.h>
#include <wl_android.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>

#define WIFI_PLAT_NAME		"bcmdhd_wlan"
#define WIFI_PLAT_NAME2		"bcm4329_wlan"
#define WIFI_PLAT_EXT		"bcmdhd_wifi_platform"

bool cfg_multichip = FALSE;
bcmdhd_wifi_platdata_t *dhd_wifi_platdata = NULL;
static int wifi_plat_dev_probe_ret = 0;
static bool is_power_on = FALSE;
#ifdef DHD_OF_SUPPORT
static bool dts_enabled = TRUE;
extern struct resource dhd_wlan_resources;
extern struct wifi_platform_data dhd_wlan_control;
#else
static bool dts_enabled = FALSE;
struct resource dhd_wlan_resources = {0};
struct wifi_platform_data dhd_wlan_control = {0};
#endif /* CONFIG_OF && !defined(CONFIG_ARCH_MSM) */

static int dhd_wifi_platform_load(void);

extern void* wl_cfg80211_get_dhdp(void);

#ifdef ENABLE_4335BT_WAR
extern int bcm_bt_lock(int cookie);
extern void bcm_bt_unlock(int cookie);
static int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */
#endif /* ENABLE_4335BT_WAR */

wifi_adapter_info_t* dhd_wifi_platform_get_adapter(uint32 bus_type, uint32 bus_num, uint32 slot_num)
{
	int i;

	if (dhd_wifi_platdata == NULL)
		return NULL;

	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		wifi_adapter_info_t *adapter = &dhd_wifi_platdata->adapters[i];
		if ((adapter->bus_type == -1 || adapter->bus_type == bus_type) &&
			(adapter->bus_num == -1 || adapter->bus_num == bus_num) &&
			(adapter->slot_num == -1 || adapter->slot_num == slot_num)) {
			DHD_TRACE(("found adapter info '%s'\n", adapter->name));
			return adapter;
		}
	}
	return NULL;
}

void* wifi_platform_prealloc(wifi_adapter_info_t *adapter, int section, unsigned long size)
{
	void *alloc_ptr = NULL;
	struct wifi_platform_data *plat_data;

	if (!adapter || !adapter->wifi_plat_data)
		return NULL;
	plat_data = adapter->wifi_plat_data;
	if (plat_data->mem_prealloc) {
		alloc_ptr = plat_data->mem_prealloc(section, size);
		if (alloc_ptr) {
			DHD_INFO(("success alloc section %d\n", section));
			if (size != 0L)
				bzero(alloc_ptr, size);
			return alloc_ptr;
		} else {
			DHD_ERROR(("%s: failed to alloc static mem section %d\n",
				__FUNCTION__, section));
		}
	}

	return NULL;
}

void* wifi_platform_get_prealloc_func_ptr(wifi_adapter_info_t *adapter)
{
	struct wifi_platform_data *plat_data;

	if (!adapter || !adapter->wifi_plat_data)
		return NULL;
	plat_data = adapter->wifi_plat_data;
	return plat_data->mem_prealloc;
}

int wifi_platform_get_irq_number(wifi_adapter_info_t *adapter, unsigned long *irq_flags_ptr)
{
	if (adapter == NULL)
		return -1;
	if (irq_flags_ptr)
		*irq_flags_ptr = adapter->intr_flags;
	return adapter->irq_num;
}


int wifi_platform_bus_enumerate(wifi_adapter_info_t *adapter, bool device_present)
{
	int err = 0;
	struct wifi_platform_data *plat_data;

	if (!adapter || !adapter->wifi_plat_data)
		return -EINVAL;
	plat_data = adapter->wifi_plat_data;

	DHD_ERROR(("%s device present %d\n", __FUNCTION__, device_present));
	if (plat_data->set_carddetect) {
		err = plat_data->set_carddetect(device_present);
	}
	return err;

}

#define MAC_ADDRESS_LEN 12
static int wifi_get_external_mac_addr(unsigned char *buf)
{
	int ret = 0;
	int i;
	struct file *fp = NULL;
	unsigned char c_mac[MAC_ADDRESS_LEN];
	char fname[] = "/config/wifi/mac.txt";

	DHD_TRACE(("%s Enter\n", __func__));

	fp = dhd_os_open_image(fname);
	if (fp == NULL) {
		DHD_ERROR(("%s: unable to open %s\n", __func__, fname));
		return 1;
	}

	if (dhd_os_get_image_block(c_mac, MAC_ADDRESS_LEN, fp)
	    != MAC_ADDRESS_LEN){
		DHD_ERROR(("%s: Error reading from %s\n", __func__, fname));
		dhd_os_close_image(fp);
		return 1;
	}
	dhd_os_close_image(fp);

	for (i = 0; i < MAC_ADDRESS_LEN; i += 2) {
		c_mac[i] = bcm_isdigit(c_mac[i]) ?
			c_mac[i]-'0' : bcm_toupper(c_mac[i])-'A'+10;
		c_mac[i+1] = bcm_isdigit(c_mac[i+1]) ?
			c_mac[i+1]-'0' : bcm_toupper(c_mac[i+1])-'A'+10;

		buf[i/2] = c_mac[i]*16 + c_mac[i+1];
	}

	DHD_TRACE(("%s: read from file mac address: %x:%x:%x:%x:%x:%x\n",
		   __func__, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]));

	return ret;
}

int wifi_platform_get_mac_addr(wifi_adapter_info_t *adapter, unsigned char *buf)
{
	struct wifi_platform_data *plat_data;

	DHD_ERROR(("%s\n", __FUNCTION__));
	if (!buf)
		return -EINVAL;
	if (!adapter)
		return wifi_get_external_mac_addr(buf);
	plat_data = adapter->wifi_plat_data;
	if (!plat_data)
		return wifi_get_external_mac_addr(buf);
	else if (plat_data->get_mac_addr) {
		return plat_data->get_mac_addr(buf);
	}
	return -EOPNOTSUPP;
}

void *wifi_platform_get_country_code(wifi_adapter_info_t *adapter, char *ccode,
				     u32 flags)
{
	/* get_country_code was added after 2.6.39 */
#if	(LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
	struct wifi_platform_data *plat_data;

	if (!ccode || !adapter || !adapter->wifi_plat_data)
		return NULL;
	plat_data = adapter->wifi_plat_data;

	DHD_TRACE(("%s\n", __FUNCTION__));
	if (plat_data->get_country_code) {
		return plat_data->get_country_code(ccode, flags);
	}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)) */

	return NULL;
}

static int wifi_plat_dev_drv_probe(struct platform_device *pdev)
{
	struct resource *resource;
	wifi_adapter_info_t *adapter;

	/* Android style wifi platform data device ("bcmdhd_wlan" or "bcm4329_wlan")
	 * is kept for backward compatibility and supports only 1 adapter
	 */
	ASSERT(dhd_wifi_platdata != NULL);
	ASSERT(dhd_wifi_platdata->num_adapters == 1);
	adapter = &dhd_wifi_platdata->adapters[0];
	adapter->wifi_plat_data = (struct wifi_platform_data *)(pdev->dev.platform_data);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "bcmdhd_wlan_irq");
	if (resource == NULL)
		resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "bcm4329_wlan_irq");
	if (resource) {
		adapter->irq_num = resource->start;
		adapter->intr_flags = resource->flags & IRQF_TRIGGER_MASK;
	}

	wifi_plat_dev_probe_ret = dhd_wifi_platform_load();
	return wifi_plat_dev_probe_ret;
}

#ifdef CONFIG_ACPI
static int wifi_plat_dev_drv_probe_acpi(struct platform_device *pdev)
{
	wifi_adapter_info_t *adapter;
	acpi_handle handle;
	struct acpi_device *adev;
	int irq_num = -1;

	/* Android style wifi platform data device ("bcmdhd_wlan" or "bcm4329_wlan")
	 * is kept for backward compatibility and supports only 1 adapter
	 */
	ASSERT(dhd_wifi_platdata != NULL);
	ASSERT(dhd_wifi_platdata->num_adapters == 1);
	adapter = &dhd_wifi_platdata->adapters[0];
	dhd_wlan_control.get_mac_addr = wifi_get_external_mac_addr;
	adapter->wifi_plat_data = (void *)&dhd_wlan_control;

	if (ACPI_HANDLE(&pdev->dev)) {
		handle = ACPI_HANDLE(&pdev->dev);

		if (!handle || acpi_bus_get_device(handle, &adev))
			DHD_ERROR(("%s: could not get acpi pointer!\n", __FUNCTION__));
		else {
			/* Ignore SDIO ACPI power states and
			 * prevent ACPI PM at probing stage.
			 * power_manageable is reenabled when
			 * setting power for the first time
			 */
			adev->flags.power_manageable = 0;
			/* Ignore SDH SDIO controller ACPI PM state */
			adev->power.flags.ignore_parent = 1;
		}
#if defined(OOB_INTR_ONLY)
		irq_num = desc_to_gpio(gpiod_get_index(&pdev->dev, NULL, 0));
		pr_err("%s: Using ACPI table to get GPIO number: %d\n", __FUNCTION__, irq_num);
		if (irq_num > 0) {
			irq_num = gpio_to_irq(irq_num);
			pr_err("%s: IRQ number: %d\n", __FUNCTION__, irq_num);
		}
#endif
	} else {
		DHD_ERROR(("%s: Null ACPI_HANDLE, try legacy probe\n", __FUNCTION__));
		return wifi_plat_dev_drv_probe(pdev);
	}

	adapter->irq_num = irq_num;
	adapter->intr_flags = IRQF_TRIGGER_FALLING;

	wifi_plat_dev_probe_ret = dhd_wifi_platform_load();
	return wifi_plat_dev_probe_ret;
}
#endif /* CONFIG_ACPI */

static int wifi_plat_dev_drv_remove(struct platform_device *pdev)
{
	wifi_adapter_info_t *adapter;

	/* Android style wifi platform data device ("bcmdhd_wlan" or "bcm4329_wlan")
	 * is kept for backward compatibility and supports only 1 adapter
	 */
	ASSERT(dhd_wifi_platdata != NULL);
	ASSERT(dhd_wifi_platdata->num_adapters == 1);
	adapter = &dhd_wifi_platdata->adapters[0];
	if (is_power_on) {
#ifdef BCMPCIE
		wifi_platform_bus_enumerate(adapter, FALSE);
		OSL_SLEEP(100);
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
#else
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
		wifi_platform_bus_enumerate(adapter, FALSE);
#endif /* BCMPCIE */
	}

	return 0;
}

static int wifi_plat_dev_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	DHD_TRACE(("##> %s\n", __FUNCTION__));
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY) && \
	defined(BCMSDIO)
	bcmsdh_oob_intr_set(0);
#endif /* (OOB_INTR_ONLY) */
	return 0;
}

static int wifi_plat_dev_drv_resume(struct platform_device *pdev)
{
	DHD_TRACE(("##> %s\n", __FUNCTION__));
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY) && \
	defined(BCMSDIO)
	if (dhd_os_check_if_up(wl_cfg80211_get_dhdp()))
		bcmsdh_oob_intr_set(1);
#endif /* (OOB_INTR_ONLY) */
	return 0;
}

#ifdef CONFIG_ACPI
static struct acpi_device_id bcm_acpi_id[] = {
/* ACPI IDs here */
	{ "BCM43241" },
	{ "BCM4321" },
	{ "BCM43341" },
	{ "BCM4334" },
	{ }
};
MODULE_DEVICE_TABLE(acpi, bcm_acpi_id);
#endif

static struct platform_driver wifi_platform_dev_driver = {
#ifdef CONFIG_ACPI
	.probe          = wifi_plat_dev_drv_probe_acpi,
#else
	.probe          = wifi_plat_dev_drv_probe,
#endif
	.remove         = wifi_plat_dev_drv_remove,
	.suspend        = wifi_plat_dev_drv_suspend,
	.resume         = wifi_plat_dev_drv_resume,
	.driver         = {
#ifdef CONFIG_ACPI
	.acpi_match_table = ACPI_PTR(bcm_acpi_id),
#endif
	.name   = WIFI_PLAT_NAME,
	}
};

static struct platform_driver wifi_platform_dev_driver_legacy = {
	.probe          = wifi_plat_dev_drv_probe,
	.remove         = wifi_plat_dev_drv_remove,
	.suspend        = wifi_plat_dev_drv_suspend,
	.resume         = wifi_plat_dev_drv_resume,
	.driver         = {
	.name	= WIFI_PLAT_NAME2,
	}
};

#ifdef CONFIG_ACPI
static int wifi_acpi_match(struct device *dev, void *data)
{
	struct acpi_device_id *ids = data, *id;
	struct platform_device *pdev = to_platform_device(dev);

	for (id = ids; id->id[0]; id++) {
		if (!strncmp(id->id, pdev->name, strlen(id->id))) {
			DHD_ERROR(("found wifi acpi device %s\n", id->id));
			return TRUE;
		}
	}
	return FALSE;
}
#endif /* CONFIG_ACPI */


int wifi_platform_set_power(wifi_adapter_info_t *adapter, bool on, unsigned long msec)
{
	int err = 0;
	struct wifi_platform_data *plat_data;
#ifdef CONFIG_ACPI
	struct device *dev;
	struct acpi_device *adev;
	acpi_handle handle;
	struct platform_device *pdev;
	struct sdio_func *func = NULL;
#endif
	if (!adapter || !adapter->wifi_plat_data)
		return -EINVAL;
	plat_data = adapter->wifi_plat_data;

	DHD_ERROR(("%s = %d\n", __FUNCTION__, on));

	if (plat_data->set_power) {
#ifdef ENABLE_4335BT_WAR
		if (on) {
			printk("WiFi: trying to acquire BT lock\n");
			if (bcm_bt_lock(lock_cookie_wifi) != 0)
				printk("** WiFi: timeout in acquiring bt lock**\n");
			printk("%s: btlock acquired\n", __FUNCTION__);
		} else {
			/* For a exceptional case, release btlock */
			bcm_bt_unlock(lock_cookie_wifi);
		}
#endif /* ENABLE_4335BT_WAR */

		err = plat_data->set_power(on);
	}

#if defined(CONFIG_ACPI) && defined(BCMSDIO)
	dev = bus_find_device(&platform_bus_type, NULL, bcm_acpi_id, wifi_acpi_match);
	pdev = to_platform_device(dev);
	if (ACPI_HANDLE(&pdev->dev)) {
		handle = ACPI_HANDLE(&pdev->dev);
		if (acpi_bus_get_device(handle, &adev) == 0) {
			func = adapter->sdio_func;

			if (on) {
				acpi_device_set_power(adev, ACPI_STATE_D0);
				if (func && func->card && func->card->host)
					mmc_power_restore_host(func->card->host);
			} else {
				/* Only handle power after probe is completed */
				if (!adev->flags.power_manageable) {
					DHD_ERROR(("%s Now enabling ACPI PM in driver\n", __FUNCTION__));
					adev->flags.power_manageable = 1;
				}

				if (func && func->card && func->card->host)
					mmc_power_save_host(func->card->host);
				acpi_device_set_power(adev, ACPI_STATE_D3_COLD);
			}
		}
	}
#endif /* CONFIG_ACPI */

	if (msec && !err)
		OSL_SLEEP(msec);

	if (on && !err)
		is_power_on = TRUE;
	else
		is_power_on = FALSE;

	return err;
}

static int wifi_platdev_match(struct device *dev, void *data)
{
	char *name = (char*)data;
	struct platform_device *pdev = to_platform_device(dev);

	if (strcmp(pdev->name, name) == 0) {
		DHD_ERROR(("found wifi platform device %s\n", name));
		return TRUE;
	}

	return FALSE;
}

static int wifi_ctrlfunc_register_drv(void)
{
	int err = 0;
	struct device *dev1, *dev2 = NULL;
	wifi_adapter_info_t *adapter;

#ifdef CONFIG_ACPI
	dev1 = bus_find_device(&platform_bus_type, NULL, bcm_acpi_id, wifi_acpi_match);
	if (!dev1)
#endif /* CONFIG_ACPI */
	dev1 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME, wifi_platdev_match);
	dev2 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME2, wifi_platdev_match);
	if (!dts_enabled) {
		if (dev1 == NULL && dev2 == NULL) {
			DHD_ERROR(("no wifi platform data, skip\n"));
			return -ENXIO;
		}
	}

	/* multi-chip support not enabled, build one adapter information for
	 * DHD (either SDIO, USB or PCIe)
	 */
	adapter = kzalloc(sizeof(wifi_adapter_info_t), GFP_KERNEL);
	adapter->name = "DHD generic adapter";
	adapter->bus_type = -1;
	adapter->bus_num = -1;
	adapter->slot_num = -1;
	adapter->irq_num = -1;
	adapter->sdio_func = NULL;
	is_power_on = FALSE;
	wifi_plat_dev_probe_ret = 0;
	dhd_wifi_platdata = kzalloc(sizeof(bcmdhd_wifi_platdata_t), GFP_KERNEL);
	dhd_wifi_platdata->num_adapters = 1;
	dhd_wifi_platdata->adapters = adapter;

	if (dev1) {
		err = platform_driver_register(&wifi_platform_dev_driver);
		if (err) {
			DHD_ERROR(("%s: failed to register wifi ctrl func driver\n",
				__FUNCTION__));
			return err;
		}
	}
	if (dev2) {
		err = platform_driver_register(&wifi_platform_dev_driver_legacy);
		if (err) {
			DHD_ERROR(("%s: failed to register wifi ctrl func legacy driver\n",
				__FUNCTION__));
			return err;
		}
	}

	if (dts_enabled) {
		struct resource *resource;
		adapter->wifi_plat_data = (void *)&dhd_wlan_control;
		resource = &dhd_wlan_resources;
		adapter->irq_num = resource->start;
		adapter->intr_flags = resource->flags & IRQF_TRIGGER_MASK;
		wifi_plat_dev_probe_ret = dhd_wifi_platform_load();
	}

	/* return probe function's return value if registeration succeeded */
	return wifi_plat_dev_probe_ret;
}

void wifi_ctrlfunc_unregister_drv(void)
{
	struct device *dev1, *dev2 = NULL;

#ifdef CONFIG_ACPI
	dev1 = bus_find_device(&platform_bus_type, NULL, bcm_acpi_id, wifi_acpi_match);
	if (!dev1)
#endif /* CONFIG_ACPI */
	dev1 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME, wifi_platdev_match);
	dev2 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME2, wifi_platdev_match);
	if (!dts_enabled)
		if (dev1 == NULL && dev2 == NULL)
			return;

	DHD_ERROR(("unregister wifi platform drivers\n"));
	if (dev1)
		platform_driver_unregister(&wifi_platform_dev_driver);
	if (dev2)
		platform_driver_unregister(&wifi_platform_dev_driver_legacy);
	if (dts_enabled) {
		wifi_adapter_info_t *adapter;
		adapter = &dhd_wifi_platdata->adapters[0];
		if (is_power_on) {
			wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
			wifi_platform_bus_enumerate(adapter, FALSE);
		}
	}
	kfree(dhd_wifi_platdata->adapters);
	dhd_wifi_platdata->adapters = NULL;
	dhd_wifi_platdata->num_adapters = 0;
	kfree(dhd_wifi_platdata);
	dhd_wifi_platdata = NULL;
}

static int bcmdhd_wifi_plat_dev_drv_probe(struct platform_device *pdev)
{
	dhd_wifi_platdata = (bcmdhd_wifi_platdata_t *)(pdev->dev.platform_data);

	return dhd_wifi_platform_load();
}

static int bcmdhd_wifi_plat_dev_drv_remove(struct platform_device *pdev)
{
	int i;
	wifi_adapter_info_t *adapter;
	ASSERT(dhd_wifi_platdata != NULL);

	/* power down all adapters */
	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		adapter = &dhd_wifi_platdata->adapters[i];
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
		wifi_platform_bus_enumerate(adapter, FALSE);
	}
	return 0;
}

static struct platform_driver dhd_wifi_platform_dev_driver = {
	.probe          = bcmdhd_wifi_plat_dev_drv_probe,
	.remove         = bcmdhd_wifi_plat_dev_drv_remove,
	.driver         = {
	.name   = WIFI_PLAT_EXT,
	}
};

int dhd_wifi_platform_register_drv(void)
{
	int err = 0;
	struct device *dev;

	/* register Broadcom wifi platform data driver if multi-chip is enabled,
	 * otherwise use Android style wifi platform data (aka wifi control function)
	 * if it exists
	 *
	 * to support multi-chip DHD, Broadcom wifi platform data device must
	 * be added in kernel early boot (e.g. board config file).
	 */
	if (cfg_multichip) {
#ifdef CONFIG_ACPI
		dev = bus_find_device(&platform_bus_type, NULL, bcm_acpi_id, wifi_acpi_match);
		if (!dev)
#endif /* CONFIG_ACPI */
		dev = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_EXT, wifi_platdev_match);
		if (dev == NULL) {
			DHD_ERROR(("bcmdhd wifi platform data device not found!!\n"));
			return -ENXIO;
		}
		err = platform_driver_register(&dhd_wifi_platform_dev_driver);
	} else {
		err = wifi_ctrlfunc_register_drv();

		/* no wifi ctrl func either, load bus directly and ignore this error */
		if (err) {
			if (err == -ENXIO) {
				/* wifi ctrl function does not exist */
				err = dhd_wifi_platform_load();
			} else {
				/* unregister driver due to initialization failure */
				wifi_ctrlfunc_unregister_drv();
			}
		}
	}

	return err;
}

#ifdef BCMPCIE
static int dhd_wifi_platform_load_pcie(void)
{
	int err = 0;
	int i;
	wifi_adapter_info_t *adapter;

	BCM_REFERENCE(i);
	BCM_REFERENCE(adapter);

	if (dhd_wifi_platdata == NULL) {
		err = dhd_bus_register();
	} else {
		if (dhd_download_fw_on_driverload) {
			/* power up all adapters */
			for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
				int retry = POWERUP_MAX_RETRY;
				adapter = &dhd_wifi_platdata->adapters[i];

				DHD_ERROR(("Power-up adapter '%s'\n", adapter->name));
				DHD_INFO((" - irq %d [flags %d], firmware: %s, nvram: %s\n",
					adapter->irq_num, adapter->intr_flags, adapter->fw_path,
					adapter->nv_path));
				DHD_INFO((" - bus type %d, bus num %d, slot num %d\n\n",
					adapter->bus_type, adapter->bus_num, adapter->slot_num));

				do {
					err = wifi_platform_set_power(adapter,
						TRUE, WIFI_TURNON_DELAY);
					if (err) {
						DHD_ERROR(("failed to power up %s,"
							" %d retry left\n",
							adapter->name, retry));
						/* WL_REG_ON state unknown, Power off forcely */
						wifi_platform_set_power(adapter,
							FALSE, WIFI_TURNOFF_DELAY);
						continue;
					} else {
						err = wifi_platform_bus_enumerate(adapter, TRUE);
						if (err) {
							DHD_ERROR(("failed to enumerate bus %s, "
								"%d retry left\n",
								adapter->name, retry));
							wifi_platform_set_power(adapter, FALSE,
								WIFI_TURNOFF_DELAY);
						} else {
							break;
						}
					}
				} while (retry--);

				if (!retry) {
					DHD_ERROR(("failed to power up %s, max retry reached**\n",
						adapter->name));
					return -ENODEV;
				}
			}
		}

		err = dhd_bus_register();

		if (err) {
			DHD_ERROR(("%s: pcie_register_driver failed\n", __FUNCTION__));
			if (dhd_download_fw_on_driverload) {
				/* power down all adapters */
				for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
					adapter = &dhd_wifi_platdata->adapters[i];
					wifi_platform_bus_enumerate(adapter, FALSE);
					wifi_platform_set_power(adapter,
						FALSE, WIFI_TURNOFF_DELAY);
				}
			}
		}
	}

	return err;
}
#else
static int dhd_wifi_platform_load_pcie(void)
{
	return 0;
}
#endif /* BCMPCIE  */


void dhd_wifi_platform_unregister_drv(void)
{
	if (cfg_multichip)
		platform_driver_unregister(&dhd_wifi_platform_dev_driver);
	else
		wifi_ctrlfunc_unregister_drv();
}

extern int dhd_watchdog_prio;
extern int dhd_dpc_prio;
extern uint dhd_deferred_tx;
#if defined(BCMLXSDMMC)
extern struct semaphore dhd_registration_sem;
#endif

#ifdef BCMSDIO
static int dhd_wifi_platform_load_sdio(void)
{
	int i;
	int err = 0;
	wifi_adapter_info_t *adapter;

	BCM_REFERENCE(i);
	BCM_REFERENCE(adapter);
	/* Sanity check on the module parameters
	 * - Both watchdog and DPC as tasklets are ok
	 * - If both watchdog and DPC are threads, TX must be deferred
	 */
	if (!(dhd_watchdog_prio < 0 && dhd_dpc_prio < 0) &&
		!(dhd_watchdog_prio >= 0 && dhd_dpc_prio >= 0 && dhd_deferred_tx))
		return -EINVAL;

#if defined(BCMLXSDMMC)
	if (dhd_wifi_platdata == NULL) {
		DHD_ERROR(("DHD wifi platform data is required for Android build\n"));
		return -EINVAL;
	}

	sema_init(&dhd_registration_sem, 0);
	/* power up all adapters */
	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		bool chip_up = FALSE;
		int retry = POWERUP_MAX_RETRY;
		struct semaphore dhd_chipup_sem;

		adapter = &dhd_wifi_platdata->adapters[i];

		DHD_ERROR(("Power-up adapter '%s'\n", adapter->name));
		DHD_INFO((" - irq %d [flags %d], firmware: %s, nvram: %s\n",
			  adapter->irq_num, adapter->intr_flags, adapter->fw_path, adapter->nv_path));
		DHD_INFO((" - bus type %d, bus num %d, slot num %d\n\n",
			adapter->bus_type, adapter->bus_num, adapter->slot_num));

		do {
			sema_init(&dhd_chipup_sem, 0);
			err = dhd_bus_reg_sdio_notify(&dhd_chipup_sem);
			if (err) {
				DHD_ERROR(("%s dhd_bus_reg_sdio_notify fail(%d)\n\n",
					__FUNCTION__, err));
				return err;
			}
			err = wifi_platform_set_power(adapter, TRUE, WIFI_TURNON_DELAY);
			if (err) {
				/* WL_REG_ON state unknown, Power off forcely */
				wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
				continue;
			} else {
				wifi_platform_bus_enumerate(adapter, TRUE);
				err = 0;
			}

			if (down_timeout(&dhd_chipup_sem, msecs_to_jiffies(POWERUP_WAIT_MS)) == 0) {
				dhd_bus_unreg_sdio_notify();
				chip_up = TRUE;
				break;
			}

			DHD_ERROR(("failed to power up %s, %d retry left\n", adapter->name, retry));
			dhd_bus_unreg_sdio_notify();
			wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
			wifi_platform_bus_enumerate(adapter, FALSE);
		} while (retry--);

		if (!chip_up) {
			DHD_ERROR(("failed to power up %s, max retry reached**\n", adapter->name));
			return -ENODEV;
		}

	}

	err = dhd_bus_register();

	if (err) {
		DHD_ERROR(("%s: sdio_register_driver failed\n", __FUNCTION__));
		goto fail;
	}


	/*
	 * Wait till MMC sdio_register_driver callback called and made driver attach.
	 * It's needed to make sync up exit from dhd insmod  and
	 * Kernel MMC sdio device callback registration
	 */
	err = down_timeout(&dhd_registration_sem, msecs_to_jiffies(DHD_REGISTRATION_TIMEOUT));
	if (err) {
		DHD_ERROR(("%s: sdio_register_driver timeout or error \n", __FUNCTION__));
		dhd_bus_unregister();
		goto fail;
	}

	return err;

fail:
	/* power down all adapters */
	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		adapter = &dhd_wifi_platdata->adapters[i];
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
		wifi_platform_bus_enumerate(adapter, FALSE);
	}
#else

	/* x86 bring-up PC needs no power-up operations */
	err = dhd_bus_register();

#endif

	return err;
}
#else /* BCMSDIO */
static int dhd_wifi_platform_load_sdio(void)
{
	return 0;
}
#endif /* BCMSDIO */

static int dhd_wifi_platform_load_usb(void)
{
	return 0;
}

static int dhd_wifi_platform_load()
{
	int err = 0;

		wl_android_init();

	if ((err = dhd_wifi_platform_load_usb()))
		goto end;
	else if ((err = dhd_wifi_platform_load_sdio()))
		goto end;
	else
		err = dhd_wifi_platform_load_pcie();

end:
	if (err)
		wl_android_exit();
	else
		wl_android_post_init();

	return err;
}

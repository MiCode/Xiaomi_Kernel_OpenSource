/*
 * Custom file for dealing platform specific resouce
 *
 * Copyright (C) 1999-2015, Broadcom Corporation
 * Copyright (C) 2018 XiaoMi, Inc.
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
 * $Id: dhd_custom_msm.c 520105 2014-12-10 07:22:01Z $
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/mmc/host.h>
#include <linux/if.h>

#if defined(CONFIG_ARCH_MSM)
#if defined(CONFIG_64BIT)
#include <linux/msm_pcie.h>
#else
#include <mach/msm_pcie.h>
#endif
#endif

#include <linux/of_gpio.h>

#include <linux/fcntl.h>
#include <linux/fs.h>

#define BCM_DBG pr_debug

static int gpio_wl_reg_on = -1;
static int brcm_wake_irq = -1;

#if !defined(CONFIG_WIFI_CONTROL_FUNC)
#define WLAN_PLAT_NODFS_FLAG	0x01
#define WLAN_PLAT_AP_FLAG	0x02
#endif

#ifdef CONFIG_DHD_USE_STATIC_BUF

enum dhd_prealloc_index {
	DHD_PREALLOC_PROT = 0,
	DHD_PREALLOC_RXBUF,
	DHD_PREALLOC_DATABUF,
	DHD_PREALLOC_OSL_BUF,
	DHD_PREALLOC_SKB_BUF,
	DHD_PREALLOC_WIPHY_ESCAN0 = 5,
	DHD_PREALLOC_WIPHY_ESCAN1 = 6,
	DHD_PREALLOC_DHD_INFO = 7,
	DHD_PREALLOC_DHD_WLFC_INFO = 8,
	DHD_PREALLOC_IF_FLOW_LKUP = 9,
	DHD_PREALLOC_FLOWRING = 10,
	DHD_PREALLOC_MAX
};

#define STATIC_BUF_MAX_NUM	20
#define STATIC_BUF_SIZE	(PAGE_SIZE*2)

#define DHD_PREALLOC_PROT_SIZE   	(512)
#define DHD_PREALLOC_WIPHY_ESCAN0_SIZE	(64 * 1024)
#define DHD_PREALLOC_DHD_INFO_SIZE		(24 * 1024)
#ifdef CONFIG_64BIT
#define DHD_PREALLOC_IF_FLOW_LKUP_SIZE	(20 * 1024 * 2)
#else
#define DHD_PREALLOC_IF_FLOW_LKUP_SIZE	(20 * 1024)
#endif
#define DHD_PREALLOC_OSL_BUF_SIZE      (STATIC_BUF_MAX_NUM * STATIC_BUF_SIZE)

#define WLAN_SCAN_BUF_SIZE		(64 * 1024)

#if defined(CONFIG_64BIT)
#define WLAN_DHD_INFO_BUF_SIZE		(24 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE	(64 * 1024)
#else
#define WLAN_DHD_INFO_BUF_SIZE		(16 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE		(16 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE	(20 * 1024)
#endif				/* CONFIG_64BIT */
#define WLAN_DHD_MEMDUMP_SIZE		(800 * 1024)

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#ifdef CONFIG_BCMDHD_PCIE
#define DHD_SKB_1PAGE_BUFSIZE	(PAGE_SIZE*1)
#define DHD_SKB_2PAGE_BUFSIZE	(PAGE_SIZE*2)
#define DHD_SKB_4PAGE_BUFSIZE	(PAGE_SIZE*4)

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	0
#define WLAN_SECTION_SIZE_2	0
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_1PAGE_BUF_NUM	0
#define DHD_SKB_2PAGE_BUF_NUM	64
#define DHD_SKB_4PAGE_BUF_NUM	0

#else
#define DHD_SKB_HDRSIZE		336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_1PAGE_BUF_NUM	8
#define DHD_SKB_2PAGE_BUF_NUM	8
#define DHD_SKB_4PAGE_BUF_NUM	1
#endif				/* CONFIG_BCMDHD_PCIE */

#define WLAN_SKB_1_2PAGE_BUF_NUM	((DHD_SKB_1PAGE_BUF_NUM) + \
		(DHD_SKB_2PAGE_BUF_NUM))
#define WLAN_SKB_BUF_NUM	((WLAN_SKB_1_2PAGE_BUF_NUM) + \
		(DHD_SKB_4PAGE_BUF_NUM))

void *wlan_static_prot = NULL;
void *wlan_static_scan_buf0 = NULL;
void *wlan_static_scan_buf1 = NULL;
void *wlan_static_dhd_info_buf = NULL;
void *wlan_static_if_flow_lkup = NULL;
void *wlan_static_osl_buf = NULL;

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

static void *dhd_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == DHD_PREALLOC_PROT)
		return wlan_static_prot;

	if (section == DHD_PREALLOC_SKB_BUF)
		return wlan_static_skb;

	if (section == DHD_PREALLOC_WIPHY_ESCAN0)
		return wlan_static_scan_buf0;

	if (section == DHD_PREALLOC_WIPHY_ESCAN1)
		return wlan_static_scan_buf1;

	if (section == DHD_PREALLOC_OSL_BUF) {
		if (size > DHD_PREALLOC_OSL_BUF_SIZE) {
			pr_err
			    ("request OSL_BUF(%lu) is bigger than static size(%ld).\n",
			     size, DHD_PREALLOC_OSL_BUF_SIZE);
			return NULL;
		}
		return wlan_static_osl_buf;
	}

	if (section == DHD_PREALLOC_DHD_INFO) {
		if (size > DHD_PREALLOC_DHD_INFO_SIZE) {
			pr_err
			    ("request DHD_INFO size(%lu) is bigger than static size(%d).\n",
			     size, DHD_PREALLOC_DHD_INFO_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}
	if (section == DHD_PREALLOC_IF_FLOW_LKUP) {
		if (size > DHD_PREALLOC_IF_FLOW_LKUP_SIZE) {
			pr_err
			    ("request DHD_IF_FLOW_LKUP size(%lu) is bigger than static size(%d).\n",
			     size, DHD_PREALLOC_IF_FLOW_LKUP_SIZE);
			return NULL;
		}

		return wlan_static_if_flow_lkup;
	}
	if ((section < 0) || (section > DHD_PREALLOC_MAX))
		pr_err("request section id(%d) is out of max index %d\n",
		       section, DHD_PREALLOC_MAX);

	return NULL;
}

static int dhd_init_wlan_mem(void)
{

	int i;
	int j;

	for (i = 0; i < DHD_SKB_1PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i]) {
			goto err_skb_alloc;
		}
	}

	for (i = DHD_SKB_1PAGE_BUF_NUM; i < WLAN_SKB_1_2PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i]) {
			goto err_skb_alloc;
		}
	}

#if !defined(CONFIG_BCMDHD_PCIE)
	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i]) {
		goto err_skb_alloc;
	}
#endif				/* !CONFIG_BCMDHD_PCIE */

	wlan_static_prot = kmalloc(DHD_PREALLOC_PROT_SIZE, GFP_KERNEL);
	if (!wlan_static_prot) {
		pr_err("Failed to alloc wlan_static_prot\n");
		goto err_mem_alloc;
	}

	wlan_static_osl_buf = kmalloc(DHD_PREALLOC_OSL_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_osl_buf) {
		pr_err("Failed to alloc wlan_static_osl_buf\n");
		goto err_mem_alloc;
	}

	wlan_static_scan_buf0 =
	    kmalloc(DHD_PREALLOC_WIPHY_ESCAN0_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0) {
		pr_err("Failed to alloc wlan_static_scan_buf0\n");
		goto err_mem_alloc;
	}

	wlan_static_dhd_info_buf =
	    kmalloc(DHD_PREALLOC_DHD_INFO_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf) {
		pr_err("Failed to alloc wlan_static_dhd_info_buf\n");
		goto err_mem_alloc;
	}
#ifdef CONFIG_BCMDHD_PCIE
	wlan_static_if_flow_lkup =
	    kmalloc(DHD_PREALLOC_IF_FLOW_LKUP_SIZE, GFP_KERNEL);
	if (!wlan_static_if_flow_lkup) {
		pr_err("Failed to alloc wlan_static_if_flow_lkup\n");
		goto err_mem_alloc;
	}
#endif				/* CONFIG_BCMDHD_PCIE */

	return 0;

      err_mem_alloc:

	if (wlan_static_prot)
		kfree(wlan_static_prot);

	if (wlan_static_dhd_info_buf)
		kfree(wlan_static_dhd_info_buf);

	if (wlan_static_scan_buf1)
		kfree(wlan_static_scan_buf1);

	if (wlan_static_scan_buf0)
		kfree(wlan_static_scan_buf0);

	if (wlan_static_osl_buf)
		kfree(wlan_static_osl_buf);

#ifdef CONFIG_BCMDHD_PCIE
	if (wlan_static_if_flow_lkup)
		kfree(wlan_static_if_flow_lkup);
#endif
	pr_err("Failed to mem_alloc for WLAN\n");

	i = WLAN_SKB_BUF_NUM;

      err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0; j < i; j++) {
		dev_kfree_skb(wlan_static_skb[j]);
	}

	return -ENOMEM;
}
#endif				/* CONFIG_DHD_USE_STATIC_BUF */

int dhd_wifi_init_gpio(void)
{
	int wl_reg_on, wl_host_wake;
	char *wlan_node = "android,bcmdhd_wlan";
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!np) {
		WARN(1, "failed to get device node of BRCM WLAN\n");
		return -ENODEV;
	}

	/* get wlan_reg_on */
	wl_reg_on = of_get_named_gpio(np, "wl_reg_on", 0);
	if (wl_reg_on >= 0) {
		gpio_wl_reg_on = wl_reg_on;
		BCM_DBG("%s: gpio_wl_reg_on:%d.\n", __FUNCTION__,
			gpio_wl_reg_on);
	}

	/* get host_wake irq */
	wl_host_wake = of_get_named_gpio(np, "wl_host_wake", 0);
	if (wl_host_wake >= 0) {
		BCM_DBG("%s: wl_host_wake:%d.\n", __FUNCTION__, wl_host_wake);
		brcm_wake_irq = gpio_to_irq(wl_host_wake);
	}

	if (gpio_request(gpio_wl_reg_on, "WL_REG_ON"))
		pr_err("%s: Faiiled to request gpio %d for WL_REG_ON\n",
		       __func__, gpio_wl_reg_on);
	else
		pr_err("%s: gpio_request WL_REG_ON done\n", __func__);

	if (gpio_direction_output(gpio_wl_reg_on, 1))
		pr_err("%s: WL_REG_ON failed to pull up\n", __func__);
	else
		BCM_DBG("%s: WL_REG_ON is pulled up\n", __func__);

	if (gpio_get_value(gpio_wl_reg_on))
		BCM_DBG("%s: Initial WL_REG_ON: [%d]\n",
			__func__, gpio_get_value(gpio_wl_reg_on));

	return 0;
}

int dhd_wlan_power(int on)
{
	pr_info("%s Enter: power %s\n", __func__, on ? "on" : "off");

	if (on) {
		if (gpio_direction_output(gpio_wl_reg_on, 1)) {
			pr_err("%s: WL_REG_ON didn't output high\n", __func__);
			return -EIO;
		}
		if (!gpio_get_value(gpio_wl_reg_on))
			pr_err("[%s] gpio didn't set high.\n", __func__);
	} else {
		if (gpio_direction_output(gpio_wl_reg_on, 0)) {
			pr_err("%s: WL_REG_ON didn't output low\n", __func__);
			return -EIO;
		}
	}
	return 0;
}

EXPORT_SYMBOL(dhd_wlan_power);

static int dhd_wlan_reset(int onoff)
{
	return 0;
}

static int dhd_wlan_set_carddetect(int val)
{
	return 0;
}

/* Customized Locale table : OPTIONAL feature */
#define WLC_CNTRY_BUF_SZ        4
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int custom_locale_rev;
};

static struct cntry_locales_custom brcm_wlan_translate_custom_table[] = {
	/* Table should be filled out based on custom platform regulatory requirement */
	{"", "XT", 49},		/* Universal if Country code is unknown or empty */
	{"US", "US", 176},
	{"AE", "AE", 1},
	{"AR", "AR", 21},
	{"AT", "AT", 4},
	{"AU", "AU", 40},
	{"BE", "BE", 4},
	{"BG", "BG", 4},
	{"BN", "BN", 4},
	{"BR", "BR", 4},
	{"CA", "US", 176},	/* Previousely was CA/31 */
	{"CH", "CH", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DE", "DE", 7},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ES", "ES", 4},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GB", "GB", 6},
	{"GR", "GR", 4},
	{"HK", "HK", 2},
	{"HR", "HR", 4},
	{"HU", "HU", 4},
	{"IE", "IE", 5},
	{"IN", "IN", 28},
	{"IS", "IS", 4},
	{"IT", "IT", 4},
	{"ID", "ID", 5},
	{"JP", "JP", 86},
	{"KR", "KR", 57},
	{"KW", "KW", 5},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"LV", "LV", 4},
	{"MA", "MA", 2},
	{"MT", "MT", 4},
	{"MX", "MX", 20},
	{"MY", "MY", 16},
	{"NL", "NL", 4},
	{"NO", "NO", 4},
	{"NZ", "NZ", 4},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PY", "PY", 2},
	{"RO", "RO", 4},
	{"RU", "RU", 13},
	{"SE", "SE", 4},
	{"SG", "SG", 19},
	{"SI", "SI", 4},
	{"SK", "SK", 4},
	{"TH", "TH", 5},
	{"TR", "TR", 7},
	{"TW", "TW", 1},
	{"VN", "VN", 4},
};

struct cntry_locales_custom brcm_wlan_translate_nodfs_table[] = {
	{"", "XT", 50},		/* Universal if Country code is unknown or empty */
	{"US", "US", 177},
	{"AU", "AU", 41},
	{"BR", "BR", 18},
	{"CA", "US", 177},
	{"CH", "E0", 33},
	{"CY", "E0", 33},
	{"CZ", "E0", 33},
	{"DE", "E0", 33},
	{"DK", "E0", 33},
	{"EE", "E0", 33},
	{"ES", "E0", 33},
	{"EU", "E0", 33},
	{"FI", "E0", 33},
	{"FR", "E0", 33},
	{"GB", "E0", 33},
	{"GR", "E0", 33},
	{"HK", "SG", 20},
	{"HR", "E0", 33},
	{"HU", "E0", 33},
	{"IE", "E0", 33},
	{"IN", "IN", 29},
	{"ID", "ID", 5},
	{"IS", "E0", 33},
	{"IT", "E0", 33},
	{"JP", "JP", 87},
	{"KR", "KR", 79},
	{"KW", "KW", 5},
	{"LI", "E0", 33},
	{"LT", "E0", 33},
	{"LU", "E0", 33},
	{"LV", "LV", 4},
	{"MA", "MA", 2},
	{"MT", "E0", 33},
	{"MY", "MY", 17},
	{"MX", "US", 177},
	{"NL", "E0", 33},
	{"NO", "E0", 33},
	{"PL", "E0", 33},
	{"PT", "E0", 33},
	{"RO", "E0", 33},
	{"SE", "E0", 33},
	{"SG", "SG", 20},
	{"SI", "E0", 33},
	{"SK", "E0", 33},
	{"SZ", "E0", 33},
	{"TH", "TH", 9},
	{"TW", "TW", 60},
};

static void *dhd_wlan_get_country_code(char *ccode, u32 flags)
{
	struct cntry_locales_custom *locales;
	int size;
	int i;

	if (!ccode)
		return NULL;

	if (flags & WLAN_PLAT_NODFS_FLAG) {
		locales = brcm_wlan_translate_nodfs_table;
		size = ARRAY_SIZE(brcm_wlan_translate_nodfs_table);
	} else {
		locales = brcm_wlan_translate_custom_table;
		size = ARRAY_SIZE(brcm_wlan_translate_custom_table);
	}

	for (i = 0; i < size; i++)
		if (strcmp(ccode, locales[i].iso_abbrev) == 0)
			return &locales[i];
	return &locales[0];
}

static unsigned char brcm_mac_addr[IFHWADDRLEN] = { 0, 0x90, 0x4c, 0, 0, 0 };

static int __init dhd_mac_addr_setup(char *str)
{
	char macstr[IFHWADDRLEN * 3];
	char *macptr = macstr;
	char *token;
	int i = 0;

	if (!str)
		return 0;
	BCM_DBG("wlan MAC = %s\n", str);
	if (strlen(str) >= sizeof(macstr))
		return 0;
	strlcpy(macstr, str, sizeof(macstr));

	while (((token = strsep(&macptr, ":")) != NULL) && (i < IFHWADDRLEN)) {
		unsigned long val;
		int res;

		res = kstrtoul(token, 0x10, &val);
		if (res < 0)
			break;
		brcm_mac_addr[i++] = (u8) val;
	}

	if (i < IFHWADDRLEN && strlen(macstr) == IFHWADDRLEN * 2) {
		/* try again with wrong format (sans colons) */
		u64 mac;
		if (kstrtoull(macstr, 0x10, &mac) < 0)
			return 0;
		for (i = 0; i < IFHWADDRLEN; i++)
			brcm_mac_addr[IFHWADDRLEN - 1 - i] =
			    (u8) ((0xFF) & (mac >> (i * 8)));
	}

	return i == IFHWADDRLEN ? 1 : 0;
}

__setup("androidboot.wifimacaddr=", dhd_mac_addr_setup);

static int dhd_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	if (!buf)
		return -EFAULT;

	if ((brcm_mac_addr[4] == 0) && (brcm_mac_addr[5] == 0)) {
		prandom_seed((uint) jiffies);
		rand_mac = prandom_u32();
		brcm_mac_addr[3] = (unsigned char) rand_mac;
		brcm_mac_addr[4] = (unsigned char) (rand_mac >> 8);
		brcm_mac_addr[5] = (unsigned char) (rand_mac >> 16);
	}
	memcpy(buf, brcm_mac_addr, IFHWADDRLEN);
	return 0;
}

static int dhd_wlan_get_wake_irq(void)
{
	return brcm_wake_irq;
}

struct resource dhd_wlan_resources[] = {
	[0] = {
	       .name = "bcmdhd_wlan_irq",
	       .start = 0,	/* Dummy */
	       .end = 0,	/* Dummy */
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE | IORESOURCE_IRQ_HIGHLEVEL,	/* Dummy */
	       },
};

EXPORT_SYMBOL(dhd_wlan_resources);

struct wifi_platform_data dhd_wlan_control = {
	.set_power = dhd_wlan_power,
	.set_reset = dhd_wlan_reset,
	.set_carddetect = dhd_wlan_set_carddetect,
	.get_mac_addr = dhd_wifi_get_mac_addr,
#ifdef CONFIG_DHD_USE_STATIC_BUF
	.mem_prealloc = dhd_wlan_mem_prealloc,
#endif
	.get_wake_irq = dhd_wlan_get_wake_irq,
	.get_country_code = dhd_wlan_get_country_code,
};

EXPORT_SYMBOL(dhd_wlan_control);

int __init dhd_wlan_init(void)
{
	int ret;

	printk(KERN_INFO "%s: START\n", __FUNCTION__);

#ifdef CONFIG_DHD_USE_STATIC_BUF
	ret = dhd_init_wlan_mem();
#endif

	ret = dhd_wifi_init_gpio();
	dhd_wlan_resources[0].start = dhd_wlan_resources[0].end = brcm_wake_irq;

	return ret;
}

void __exit dhd_wlan_exit(void)
{
#ifdef CONFIG_DHD_USE_STATIC_BUF
	int i;

	for (i = 0; i < DHD_SKB_1PAGE_BUF_NUM; i++) {
		if (wlan_static_skb[i])
			dev_kfree_skb(wlan_static_skb[i]);
	}

	for (i = DHD_SKB_1PAGE_BUF_NUM; i < WLAN_SKB_1_2PAGE_BUF_NUM; i++) {
		if (wlan_static_skb[i])
			dev_kfree_skb(wlan_static_skb[i]);
	}

#if !defined(CONFIG_BCMDHD_PCIE)
	if (wlan_static_skb[i])
		dev_kfree_skb(wlan_static_skb[i]);
#endif				/* !CONFIG_BCMDHD_PCIE */

	if (wlan_static_prot)
		kfree(wlan_static_prot);

	if (wlan_static_osl_buf)
		kfree(wlan_static_osl_buf);

	if (wlan_static_scan_buf0)
		kfree(wlan_static_scan_buf0);

	if (wlan_static_dhd_info_buf)
		kfree(wlan_static_dhd_info_buf);

	if (wlan_static_scan_buf1)
		kfree(wlan_static_scan_buf1);

#ifdef CONFIG_BCMDHD_PCIE
	if (wlan_static_if_flow_lkup)
		kfree(wlan_static_if_flow_lkup);
#endif
#endif				/* CONFIG_BCMDHD_USE_STATIC_BUF */
	return;
}

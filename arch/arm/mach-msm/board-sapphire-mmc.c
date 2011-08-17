/* linux/arch/arm/mach-msm/board-sapphire-mmc.c
 * Copyright (C) 2007-2009 HTC Corporation.
 * Author: Thomas Tsai <thomas_tsai@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/err.h>
#include <linux/debugfs.h>

#include <linux/gpio.h>
#include <linux/io.h>
#include <asm/mach-types.h>

#include <mach/vreg.h>
#include <mach/htc_pwrsink.h>

#include <asm/mach/mmc.h>

#include "devices.h"
#include "gpio_chip.h"
#include "board-sapphire.h"
#include "proc_comm.h"

#define DEBUG_SDSLOT_VDD 0

extern int msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat,
			unsigned int stat_irq, unsigned long stat_irq_flags);

/* ---- COMMON ---- */
static void config_gpio_table(uint32_t *table, int len)
{
	int n;
	unsigned id;
	for (n = 0; n < len; n++) {
		id = table[n];
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	}
}

/* ---- SDCARD ---- */

static uint32_t sdcard_on_gpio_table[] = {
	PCOM_GPIO_CFG(62, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA), /* CLK */
	PCOM_GPIO_CFG(63, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA), /* CMD */
	PCOM_GPIO_CFG(64, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA), /* DAT3 */
	PCOM_GPIO_CFG(65, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA), /* DAT2 */
	PCOM_GPIO_CFG(66, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_4MA), /* DAT1 */
	PCOM_GPIO_CFG(67, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_4MA), /* DAT0 */
};

static uint32_t sdcard_off_gpio_table[] = {
	PCOM_GPIO_CFG(62, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* CLK */
	PCOM_GPIO_CFG(63, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* CMD */
	PCOM_GPIO_CFG(64, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT3 */
	PCOM_GPIO_CFG(65, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT2 */
	PCOM_GPIO_CFG(66, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT1 */
	PCOM_GPIO_CFG(67, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT0 */
};

static uint opt_disable_sdcard;

static int __init sapphire_disablesdcard_setup(char *str)
{
	int cal = simple_strtol(str, NULL, 0);

	opt_disable_sdcard = cal;
	return 1;
}

__setup("board_sapphire.disable_sdcard=", sapphire_disablesdcard_setup);

static struct vreg *vreg_sdslot;	/* SD slot power */

struct mmc_vdd_xlat {
	int mask;
	int level;
};

static struct mmc_vdd_xlat mmc_vdd_table[] = {
	{ MMC_VDD_165_195,	1800 },
	{ MMC_VDD_20_21,	2050 },
	{ MMC_VDD_21_22,	2150 },
	{ MMC_VDD_22_23,	2250 },
	{ MMC_VDD_23_24,	2350 },
	{ MMC_VDD_24_25,	2450 },
	{ MMC_VDD_25_26,	2550 },
	{ MMC_VDD_26_27,	2650 },
	{ MMC_VDD_27_28,	2750 },
	{ MMC_VDD_28_29,	2850 },
	{ MMC_VDD_29_30,	2950 },
};

static unsigned int sdslot_vdd = 0xffffffff;
static unsigned int sdslot_vreg_enabled;

static uint32_t sapphire_sdslot_switchvdd(struct device *dev, unsigned int vdd)
{
	int i, rc;

	BUG_ON(!vreg_sdslot);

	if (vdd == sdslot_vdd)
		return 0;

	sdslot_vdd = vdd;

	if (vdd == 0) {
#if DEBUG_SDSLOT_VDD
		printk(KERN_DEBUG "%s: Disabling SD slot power\n", __func__);
#endif
		config_gpio_table(sdcard_off_gpio_table,
				  ARRAY_SIZE(sdcard_off_gpio_table));
		vreg_disable(vreg_sdslot);
		sdslot_vreg_enabled = 0;
		return 0;
	}

	if (!sdslot_vreg_enabled) {
		rc = vreg_enable(vreg_sdslot);
		if (rc) {
			printk(KERN_ERR "%s: Error enabling vreg (%d)\n",
			       __func__, rc);
		}
		config_gpio_table(sdcard_on_gpio_table,
				  ARRAY_SIZE(sdcard_on_gpio_table));
		sdslot_vreg_enabled = 1;
	}

	for (i = 0; i < ARRAY_SIZE(mmc_vdd_table); i++) {
		if (mmc_vdd_table[i].mask == (1 << vdd)) {
#if DEBUG_SDSLOT_VDD
			printk(KERN_DEBUG "%s: Setting level to %u\n",
				__func__, mmc_vdd_table[i].level);
#endif
			rc = vreg_set_level(vreg_sdslot,
					    mmc_vdd_table[i].level);
			if (rc) {
				printk(KERN_ERR
				       "%s: Error setting vreg level (%d)\n",
				       __func__, rc);
			}
			return 0;
		}
	}

	printk(KERN_ERR "%s: Invalid VDD %d specified\n", __func__, vdd);
	return 0;
}

static unsigned int sapphire_sdslot_status(struct device *dev)
{
	unsigned int status;

	status = (unsigned int) gpio_get_value(SAPPHIRE_GPIO_SDMC_CD_N);
	return !status;
}

#define SAPPHIRE_MMC_VDD (MMC_VDD_165_195 | MMC_VDD_20_21 | MMC_VDD_21_22 \
			| MMC_VDD_22_23 | MMC_VDD_23_24 | MMC_VDD_24_25 \
			| MMC_VDD_25_26 | MMC_VDD_26_27 | MMC_VDD_27_28 \
			| MMC_VDD_28_29 | MMC_VDD_29_30)

static struct mmc_platform_data sapphire_sdslot_data = {
	.ocr_mask	= SAPPHIRE_MMC_VDD,
	.status		= sapphire_sdslot_status,
	.translate_vdd	= sapphire_sdslot_switchvdd,
};

/* ---- WIFI ---- */

static uint32_t wifi_on_gpio_table[] = {
	PCOM_GPIO_CFG(51, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_4MA), /* DAT3 */
	PCOM_GPIO_CFG(52, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_4MA), /* DAT2 */
	PCOM_GPIO_CFG(53, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_4MA), /* DAT1 */
	PCOM_GPIO_CFG(54, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_4MA), /* DAT0 */
	PCOM_GPIO_CFG(55, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA), /* CMD */
	PCOM_GPIO_CFG(56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA), /* CLK */
	PCOM_GPIO_CFG(29, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_4MA),  /* WLAN IRQ */
};

static uint32_t wifi_off_gpio_table[] = {
	PCOM_GPIO_CFG(51, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT3 */
	PCOM_GPIO_CFG(52, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT2 */
	PCOM_GPIO_CFG(53, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT1 */
	PCOM_GPIO_CFG(54, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* DAT0 */
	PCOM_GPIO_CFG(55, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* CMD */
	PCOM_GPIO_CFG(56, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA), /* CLK */
	PCOM_GPIO_CFG(29, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA),  /* WLAN IRQ */
};

static struct vreg *vreg_wifi_osc;	/* WIFI 32khz oscilator */
static int sapphire_wifi_cd = 0;	/* WIFI virtual 'card detect' status */

static struct sdio_embedded_func wifi_func = {
	.f_class	= SDIO_CLASS_WLAN,
	.f_maxblksize	= 512,
};

static struct embedded_sdio_data sapphire_wifi_emb_data = {
	.cis	= {
		.vendor		= 0x104c,
		.device		= 0x9066,
		.blksize	= 512,
		.max_dtr	= 20000000,
	},
	.cccr	= {
		.multi_block	= 0,
		.low_speed	= 0,
		.wide_bus	= 1,
		.high_power	= 0,
		.high_speed	= 0,
	},
	.funcs	= &wifi_func,
	.num_funcs = 1,
};

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

static int sapphire_wifi_status_register(void (*callback)(int card_present,
							  void *dev_id),
					 void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static unsigned int sapphire_wifi_status(struct device *dev)
{
	return sapphire_wifi_cd;
}

int sapphire_wifi_set_carddetect(int val)
{
	printk(KERN_DEBUG "%s: %d\n", __func__, val);
	sapphire_wifi_cd = val;
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		printk(KERN_WARNING "%s: Nobody to notify\n", __func__);
	return 0;
}
#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(sapphire_wifi_set_carddetect);
#endif

int sapphire_wifi_power_state=0;
int sapphire_bt_power_state=0;

int sapphire_wifi_power(int on)
{
	int rc;

	printk(KERN_DEBUG "%s: %d\n", __func__, on);

	if (on) {
		config_gpio_table(wifi_on_gpio_table,
				  ARRAY_SIZE(wifi_on_gpio_table));
		rc = vreg_enable(vreg_wifi_osc);
		if (rc)
			return rc;
		htc_pwrsink_set(PWRSINK_WIFI, 70);
	} else {
		config_gpio_table(wifi_off_gpio_table,
				  ARRAY_SIZE(wifi_off_gpio_table));
		htc_pwrsink_set(PWRSINK_WIFI, 0);
	}
	gpio_set_value(SAPPHIRE_GPIO_MAC_32K_EN, on);
	mdelay(100);
	gpio_set_value(SAPPHIRE_GPIO_WIFI_EN, on);
	mdelay(100);
	if (!on) {
		if(!sapphire_bt_power_state)
		{
		vreg_disable(vreg_wifi_osc);
			printk("WiFi disable vreg_wifi_osc.\n");
		}
		else
			printk("WiFi shouldn't disable vreg_wifi_osc. BT is using it!!\n");
	}
	sapphire_wifi_power_state = on;
	return 0;
}
#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(sapphire_wifi_power);
#endif

/* Eenable VREG_MMC pin to turn on fastclock oscillator : colin */
int sapphire_bt_fastclock_power(int on)
{
	int rc;

	printk(KERN_DEBUG "sapphire_bt_fastclock_power on = %d\n", on);
	if (vreg_wifi_osc) {
		if (on) {
			rc = vreg_enable(vreg_wifi_osc);
			printk(KERN_DEBUG "BT vreg_enable vreg_mmc, rc=%d\n",
			       rc);
			if (rc) {
				printk("Error turn sapphire_bt_fastclock_power rc=%d\n", rc);
				return rc;
			}
		} else {
			if (!sapphire_wifi_power_state) {
				vreg_disable(vreg_wifi_osc);
				printk(KERN_DEBUG "BT disable vreg_wifi_osc.\n");
			} else
				printk(KERN_DEBUG "BT shouldn't disable vreg_wifi_osc. WiFi is using it!!\n");
		}
	}
	sapphire_bt_power_state = on;
	return 0;
}
EXPORT_SYMBOL(sapphire_bt_fastclock_power);

static int sapphire_wifi_reset_state;
void sapphire_wifi_reset(int on)
{
	printk(KERN_DEBUG "%s: %d\n", __func__, on);
	gpio_set_value(SAPPHIRE_GPIO_WIFI_PA_RESETX, !on);
	sapphire_wifi_reset_state = on;
	mdelay(50);
}
#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(sapphire_wifi_reset);
#endif

static struct mmc_platform_data sapphire_wifi_data = {
	.ocr_mask		= MMC_VDD_28_29,
	.status			= sapphire_wifi_status,
	.register_status_notify	= sapphire_wifi_status_register,
	.embedded_sdio		= &sapphire_wifi_emb_data,
};

int __init sapphire_init_mmc(unsigned int sys_rev)
{
	wifi_status_cb = NULL;

	sdslot_vreg_enabled = 0;

	vreg_sdslot = vreg_get(0, "gp6");
	if (IS_ERR(vreg_sdslot))
		return PTR_ERR(vreg_sdslot);
	vreg_wifi_osc = vreg_get(0, "mmc");
	if (IS_ERR(vreg_wifi_osc))
		return PTR_ERR(vreg_wifi_osc);

	set_irq_wake(SAPPHIRE_GPIO_TO_INT(SAPPHIRE_GPIO_SDMC_CD_N), 1);

	msm_add_sdcc(1, &sapphire_wifi_data, 0, 0);

	if (!opt_disable_sdcard)
		msm_add_sdcc(2, &sapphire_sdslot_data,
			     SAPPHIRE_GPIO_TO_INT(SAPPHIRE_GPIO_SDMC_CD_N), 0);
	else
		printk(KERN_INFO "sapphire: SD-Card interface disabled\n");
	return 0;
}

#if defined(CONFIG_DEBUG_FS)
static int sapphiremmc_dbg_wifi_reset_set(void *data, u64 val)
{
	sapphire_wifi_reset((int) val);
	return 0;
}

static int sapphiremmc_dbg_wifi_reset_get(void *data, u64 *val)
{
	*val = sapphire_wifi_reset_state;
	return 0;
}

static int sapphiremmc_dbg_wifi_cd_set(void *data, u64 val)
{
	sapphire_wifi_set_carddetect((int) val);
	return 0;
}

static int sapphiremmc_dbg_wifi_cd_get(void *data, u64 *val)
{
	*val = sapphire_wifi_cd;
	return 0;
}

static int sapphiremmc_dbg_wifi_pwr_set(void *data, u64 val)
{
	sapphire_wifi_power((int) val);
	return 0;
}

static int sapphiremmc_dbg_wifi_pwr_get(void *data, u64 *val)
{

	*val = sapphire_wifi_power_state;
	return 0;
}

static int sapphiremmc_dbg_sd_pwr_set(void *data, u64 val)
{
	sapphire_sdslot_switchvdd(NULL, (unsigned int) val);
	return 0;
}

static int sapphiremmc_dbg_sd_pwr_get(void *data, u64 *val)
{
	*val = sdslot_vdd;
	return 0;
}

static int sapphiremmc_dbg_sd_cd_set(void *data, u64 val)
{
	return -ENOSYS;
}

static int sapphiremmc_dbg_sd_cd_get(void *data, u64 *val)
{
	*val = sapphire_sdslot_status(NULL);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sapphiremmc_dbg_wifi_reset_fops,
			sapphiremmc_dbg_wifi_reset_get,
			sapphiremmc_dbg_wifi_reset_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(sapphiremmc_dbg_wifi_cd_fops,
			sapphiremmc_dbg_wifi_cd_get,
			sapphiremmc_dbg_wifi_cd_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(sapphiremmc_dbg_wifi_pwr_fops,
			sapphiremmc_dbg_wifi_pwr_get,
			sapphiremmc_dbg_wifi_pwr_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(sapphiremmc_dbg_sd_pwr_fops,
			sapphiremmc_dbg_sd_pwr_get,
			sapphiremmc_dbg_sd_pwr_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(sapphiremmc_dbg_sd_cd_fops,
			sapphiremmc_dbg_sd_cd_get,
			sapphiremmc_dbg_sd_cd_set, "%llu\n");

static int __init sapphiremmc_dbg_init(void)
{
	struct dentry *dent;

	if (!machine_is_sapphire())
		return 0;

	dent = debugfs_create_dir("sapphiremmc_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("wifi_reset", 0644, dent, NULL,
			    &sapphiremmc_dbg_wifi_reset_fops);
	debugfs_create_file("wifi_cd", 0644, dent, NULL,
			    &sapphiremmc_dbg_wifi_cd_fops);
	debugfs_create_file("wifi_pwr", 0644, dent, NULL,
			    &sapphiremmc_dbg_wifi_pwr_fops);

	debugfs_create_file("sd_pwr", 0644, dent, NULL,
			    &sapphiremmc_dbg_sd_pwr_fops);
	debugfs_create_file("sd_cd", 0644, dent, NULL,
			    &sapphiremmc_dbg_sd_cd_fops);

	return 0;
}

device_initcall(sapphiremmc_dbg_init);

#endif

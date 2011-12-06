/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio_event.h>
#include <linux/usb/android.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/i2c.h>
#include <linux/android_pmem.h>
#include <linux/bootmem.h>
#include <linux/mfd/marimba.h>
#include <linux/power_supply.h>
#include <linux/input/rmi_platformdata.h>
#include <linux/input/rmi_i2c.h>
#include <linux/regulator/consumer.h>
#include <asm/mach/mmc.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_hsusb.h>
#include <mach/rpc_pmapp.h>
#include <mach/usbdiag.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_serial_hs.h>
#include <mach/pmic.h>
#include <mach/socinfo.h>
#include <mach/vreg.h>
#include <mach/rpc_pmapp.h>
#include <mach/msm_battery.h>
#include <mach/rpc_server_handset.h>
#include <mach/socinfo.h>
#include "board-msm7x27a-regulator.h"
#include "devices.h"
#include "devices-msm7x2xa.h"
#include <mach/pm.h>
#include "timer.h"
#include "pm-boot.h"
#include "board-msm7x27a-regulator.h"
#include "board-msm7627a.h"

#define PMEM_KERNEL_EBI1_SIZE	0x3A000
#define MSM_PMEM_AUDIO_SIZE	0x5B000
#define BAHAMA_SLAVE_ID_FM_REG 0x02
#define FM_GPIO	83
#define BT_PCM_BCLK_MODE  0x88
#define BT_PCM_DIN_MODE   0x89
#define BT_PCM_DOUT_MODE  0x8A
#define BT_PCM_SYNC_MODE  0x8B
#define FM_I2S_SD_MODE    0x8E
#define FM_I2S_WS_MODE    0x8F
#define FM_I2S_SCK_MODE   0x90
#define I2C_PIN_CTL       0x15
#define I2C_NORMAL        0x40

static struct platform_device msm_wlan_ar6000_pm_device = {
	.name           = "wlan_ar6000_pm_dev",
	.id             = -1,
};

static struct msm_gpio qup_i2c_gpios_io[] = {
	{ GPIO_CFG(60, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_scl" },
	{ GPIO_CFG(61, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_sda" },
	{ GPIO_CFG(131, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_scl" },
	{ GPIO_CFG(132, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_sda" },
};

static struct msm_gpio qup_i2c_gpios_hw[] = {
	{ GPIO_CFG(60, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_scl" },
	{ GPIO_CFG(61, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_sda" },
	{ GPIO_CFG(131, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_scl" },
	{ GPIO_CFG(132, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_sda" },
};

static void gsbi_qup_i2c_gpio_config(int adap_id, int config_type)
{
	int rc;

	if (adap_id < 0 || adap_id > 1)
		return;

	/* Each adapter gets 2 lines from the table */
	if (config_type)
		rc = msm_gpios_request_enable(&qup_i2c_gpios_hw[adap_id*2], 2);
	else
		rc = msm_gpios_request_enable(&qup_i2c_gpios_io[adap_id*2], 2);
	if (rc < 0)
		pr_err("QUP GPIO request/enable failed: %d\n", rc);
}

static struct msm_i2c_platform_data msm_gsbi0_qup_i2c_pdata = {
	.clk_freq		= 100000,
	.msm_i2c_config_gpio	= gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi1_qup_i2c_pdata = {
	.clk_freq		= 100000,
	.msm_i2c_config_gpio	= gsbi_qup_i2c_gpio_config,
};

#ifdef CONFIG_ARCH_MSM7X27A
#define MSM_PMEM_MDP_SIZE       0x1DD1000
#define MSM_PMEM_ADSP_SIZE      0x1000000

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_SIZE		0x260000
#else
#define MSM_FB_SIZE		0x195000
#endif

#endif

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_RMI4_I2C) || \
defined(CONFIG_TOUCHSCREEN_SYNAPTICS_RMI4_I2C_MODULE)

#ifndef CLEARPAD3000_ATTEN_GPIO
#define CLEARPAD3000_ATTEN_GPIO (48)
#endif

#ifndef CLEARPAD3000_RESET_GPIO
#define CLEARPAD3000_RESET_GPIO (26)
#endif

static int synaptics_touchpad_setup(void);

static struct msm_gpio clearpad3000_cfg_data[] = {
	{GPIO_CFG(CLEARPAD3000_ATTEN_GPIO, 0, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_6MA), "rmi4_attn"},
	{GPIO_CFG(CLEARPAD3000_RESET_GPIO, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), "rmi4_reset"},
};

static struct rmi_XY_pair rmi_offset = {.x = 0, .y = 0};
static struct rmi_range rmi_clipx = {.min = 48, .max = 980};
static struct rmi_range rmi_clipy = {.min = 7, .max = 1647};
static struct rmi_f11_functiondata synaptics_f11_data = {
	.swap_axes = false,
	.flipX = false,
	.flipY = false,
	.offset = &rmi_offset,
	.button_height = 113,
	.clipX = &rmi_clipx,
	.clipY = &rmi_clipy,
};

#define MAX_LEN		100

static ssize_t clearpad3000_virtual_keys_register(struct kobject *kobj,
		     struct kobj_attribute *attr, char *buf)
{
	char *virtual_keys = __stringify(EV_KEY) ":" __stringify(KEY_MENU) \
			     ":60:830:120:60" ":" __stringify(EV_KEY) \
			     ":" __stringify(KEY_HOME)   ":180:830:120:60" \
				":" __stringify(EV_KEY) ":" \
				__stringify(KEY_SEARCH) ":300:830:120:60" \
				":" __stringify(EV_KEY) ":" \
			__stringify(KEY_BACK)   ":420:830:120:60" "\n";

	return snprintf(buf, strnlen(virtual_keys, MAX_LEN) + 1 , "%s",
			virtual_keys);
}

static struct kobj_attribute clearpad3000_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.sensor00fn11",
		.mode = S_IRUGO,
	},
	.show = &clearpad3000_virtual_keys_register,
};

static struct attribute *virtual_key_properties_attrs[] = {
	&clearpad3000_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group virtual_key_properties_attr_group = {
	.attrs = virtual_key_properties_attrs,
};

struct kobject *virtual_key_properties_kobj;

static struct rmi_functiondata synaptics_functiondata[] = {
	{
		.function_index = RMI_F11_INDEX,
		.data = &synaptics_f11_data,
	},
};

static struct rmi_functiondata_list synaptics_perfunctiondata = {
	.count = ARRAY_SIZE(synaptics_functiondata),
	.functiondata = synaptics_functiondata,
};

static struct rmi_sensordata synaptics_sensordata = {
	.perfunctiondata = &synaptics_perfunctiondata,
	.rmi_sensor_setup	= synaptics_touchpad_setup,
};

static struct rmi_i2c_platformdata synaptics_platformdata = {
	.i2c_address = 0x2c,
	.irq_type = IORESOURCE_IRQ_LOWLEVEL,
	.sensordata = &synaptics_sensordata,
};

static struct i2c_board_info synaptic_i2c_clearpad3k[] = {
	{
	I2C_BOARD_INFO("rmi4_ts", 0x2c),
	.platform_data = &synaptics_platformdata,
	},
};

static int synaptics_touchpad_setup(void)
{
	int retval = 0;

	virtual_key_properties_kobj =
		kobject_create_and_add("board_properties", NULL);
	if (virtual_key_properties_kobj)
		retval = sysfs_create_group(virtual_key_properties_kobj,
				&virtual_key_properties_attr_group);
	if (!virtual_key_properties_kobj || retval)
		pr_err("failed to create ft5202 board_properties\n");

	retval = msm_gpios_request_enable(clearpad3000_cfg_data,
		    sizeof(clearpad3000_cfg_data)/sizeof(struct msm_gpio));
	if (retval) {
		pr_err("%s:Failed to obtain touchpad GPIO %d. Code: %d.",
				__func__, CLEARPAD3000_ATTEN_GPIO, retval);
		retval = 0; /* ignore the err */
	}
	synaptics_platformdata.irq = gpio_to_irq(CLEARPAD3000_ATTEN_GPIO);

	gpio_set_value(CLEARPAD3000_RESET_GPIO, 0);
	usleep(10000);
	gpio_set_value(CLEARPAD3000_RESET_GPIO, 1);
	usleep(50000);

	return retval;
}
#endif


static struct android_usb_platform_data android_usb_pdata = {
	.update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};

#ifdef CONFIG_USB_EHCI_MSM_72K
static void msm_hsusb_vbus_power(unsigned phy_info, int on)
{
	int rc = 0;
	unsigned gpio;

	gpio = QRD_GPIO_HOST_VBUS_EN;

	rc = gpio_request(gpio,	"i2c_host_vbus_en");
	if (rc < 0) {
		pr_err("failed to request %d GPIO\n", gpio);
		return;
	}
	gpio_direction_output(gpio, !!on);
	gpio_set_value_cansleep(gpio, !!on);
	gpio_free(gpio);
}

static struct msm_usb_host_platform_data msm_usb_host_pdata = {
	.phy_info       = (USB_PHY_INTEGRATED | USB_PHY_MODEL_45NM),
};

static void __init msm7627a_init_host(void)
{
	msm_add_host(0, &msm_usb_host_pdata);
}
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
static int hsusb_rpc_connect(int connect)
{
	if (connect)
		return msm_hsusb_rpc_connect();
	else
		return msm_hsusb_rpc_close();
}

static struct regulator *reg_hsusb;
static int msm_hsusb_ldo_init(int init)
{
	int rc = 0;

	if (init) {
		reg_hsusb = regulator_get(NULL, "usb");
		if (IS_ERR(reg_hsusb)) {
			rc = PTR_ERR(reg_hsusb);
			pr_err("%s: could not get regulator: %d\n",
					__func__, rc);
			goto out;
		}

		rc = regulator_set_voltage(reg_hsusb, 3300000, 3300000);
		if (rc) {
			pr_err("%s: could not set voltage: %d\n",
					__func__, rc);
			goto reg_free;
		}

		return 0;
	}
	/* else fall through */
reg_free:
	regulator_put(reg_hsusb);
out:
	reg_hsusb = NULL;
	return rc;
}

static int msm_hsusb_ldo_enable(int enable)
{
	static int ldo_status;

	if (IS_ERR_OR_NULL(reg_hsusb))
		return reg_hsusb ? PTR_ERR(reg_hsusb) : -ENODEV;

	if (ldo_status == enable)
		return 0;

	ldo_status = enable;

	return enable ?
		regulator_enable(reg_hsusb) :
		regulator_disable(reg_hsusb);
}

#ifndef CONFIG_USB_EHCI_MSM_72K
static int msm_hsusb_pmic_notif_init(void (*callback)(int online), int init)
{
	int ret = 0;

	if (init)
		ret = msm_pm_app_rpc_init(callback);
	else
		msm_pm_app_rpc_deinit(callback);

	return ret;
}
#endif

static struct msm_otg_platform_data msm_otg_pdata = {
#ifndef CONFIG_USB_EHCI_MSM_72K
	.pmic_vbus_notif_init	 = msm_hsusb_pmic_notif_init,
#else
	.vbus_power		 = msm_hsusb_vbus_power,
#endif
	.rpc_connect		 = hsusb_rpc_connect,
	.pemp_level		 = PRE_EMPHASIS_WITH_20_PERCENT,
	.cdr_autoreset		 = CDR_AUTO_RESET_DISABLE,
	.drv_ampl		 = HS_DRV_AMPLITUDE_DEFAULT,
	.se1_gating		 = SE1_GATING_DISABLE,
	.ldo_init		 = msm_hsusb_ldo_init,
	.ldo_enable		 = msm_hsusb_ldo_enable,
	.chg_init		 = hsusb_chg_init,
	.chg_connected		 = hsusb_chg_connected,
	.chg_vbus_draw		 = hsusb_chg_vbus_draw,
};
#endif

static struct msm_hsusb_gadget_platform_data msm_gadget_pdata = {
	.is_phy_status_timer_on = 1,
};

#ifdef CONFIG_SERIAL_MSM_HS
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.inject_rx_on_wakeup	= 1,
	.rx_to_inject		= 0xFD,
};
#endif
static struct msm_pm_platform_data msm7627a_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 1,
					.suspend_enabled = 1,
					.latency = 16000,
					.residency = 20000,
	},
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 1,
					.suspend_enabled = 1,
					.latency = 12000,
					.residency = 20000,
	},
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 0,
					.suspend_enabled = 1,
					.latency = 2000,
					.residency = 0,
	},
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 1,
					.suspend_enabled = 1,
					.latency = 2,
					.residency = 0,
	},
};

static struct msm_pm_boot_platform_data msm_pm_boot_pdata __initdata = {
	.mode = MSM_PM_BOOT_CONFIG_RESET_VECTOR_PHYS,
	.p_addr = 0,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 1,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static unsigned pmem_mdp_size = MSM_PMEM_MDP_SIZE;
static int __init pmem_mdp_size_setup(char *p)
{
	pmem_mdp_size = memparse(p, NULL);
	return 0;
}

early_param("pmem_mdp_size", pmem_mdp_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;
static int __init pmem_adsp_size_setup(char *p)
{
	pmem_adsp_size = memparse(p, NULL);
	return 0;
}

early_param("pmem_adsp_size", pmem_adsp_size_setup);

static unsigned fb_size = MSM_FB_SIZE;
static int __init fb_size_setup(char *p)
{
	fb_size = memparse(p, NULL);
	return 0;
}

early_param("fb_size", fb_size_setup);

static struct resource msm_fb_resources[] = {
	{
		.flags	= IORESOURCE_DMA,
	}
};

static int msm_fb_detect_panel(const char *name)
{
	int ret;

	if (!strncmp(name, "mipi_video_truly_wvga", 21))
		ret = 0;
	else
		ret = -ENODEV;

	return ret;
}

static int mipi_truly_set_bl(int on)
{
	gpio_set_value_cansleep(QRD_GPIO_BACKLIGHT_EN, !!on);

	return 1;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

static struct msm_panel_common_pdata mipi_truly_pdata = {
	.pmic_backlight = mipi_truly_set_bl,
};

static struct platform_device mipi_dsi_truly_panel_device = {
	.name	= "mipi_truly",
	.id	= 0,
	.dev	= {
		.platform_data = &mipi_truly_pdata,
	}
};

#define SND(desc, num) { .name = #desc, .id = num }
static struct snd_endpoint snd_endpoints_list[] = {
	SND(HANDSET, 0),
	SND(MONO_HEADSET, 2),
	SND(HEADSET, 3),
	SND(SPEAKER, 6),
	SND(TTY_HEADSET, 8),
	SND(TTY_VCO, 9),
	SND(TTY_HCO, 10),
	SND(BT, 12),
	SND(IN_S_SADC_OUT_HANDSET, 16),
	SND(IN_S_SADC_OUT_SPEAKER_PHONE, 25),
	SND(FM_DIGITAL_STEREO_HEADSET, 26),
	SND(FM_DIGITAL_SPEAKER_PHONE, 27),
	SND(FM_DIGITAL_BT_A2DP_HEADSET, 28),
	SND(STEREO_HEADSET_AND_SPEAKER, 31),
	SND(CURRENT, 0x7FFFFFFE),
	SND(FM_ANALOG_STEREO_HEADSET, 35),
	SND(FM_ANALOG_STEREO_HEADSET_CODEC, 36),
};
#undef SND

static struct msm_snd_endpoints msm_device_snd_endpoints = {
	.endpoints = snd_endpoints_list,
	.num = sizeof(snd_endpoints_list) / sizeof(struct snd_endpoint)
};

static struct platform_device msm_device_snd = {
	.name = "msm_snd",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_snd_endpoints
	},
};

#define DEC0_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC1_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC2_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC3_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC4_FORMAT (1<<MSM_ADSP_CODEC_MIDI)

static unsigned int dec_concurrency_table[] = {
	/* Audio LP */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DMA)), 0,
	0, 0, 0,

	/* Concurrency 1 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	 /* Concurrency 2 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 3 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 4 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 5 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 6 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	0, 0, 0, 0,

	/* Concurrency 7 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),
};

#define DEC_INFO(name, queueid, decid, nr_codec) { .module_name = name, \
	.module_queueid = queueid, .module_decid = decid, \
	.nr_codec_support = nr_codec}

static struct msm_adspdec_info dec_info_list[] = {
	DEC_INFO("AUDPLAY0TASK", 13, 0, 11), /* AudPlay0BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY1TASK", 14, 1, 11),  /* AudPlay1BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY2TASK", 15, 2, 11),  /* AudPlay2BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY3TASK", 16, 3, 11),  /* AudPlay3BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY4TASK", 17, 4, 1),  /* AudPlay4BitStreamCtrlQueue */
};

static struct msm_adspdec_database msm_device_adspdec_database = {
	.num_dec = ARRAY_SIZE(dec_info_list),
	.num_concurrency_support = (ARRAY_SIZE(dec_concurrency_table) / \
					ARRAY_SIZE(dec_info_list)),
	.dec_concurrency_table = dec_concurrency_table,
	.dec_info_list = dec_info_list,
};

static struct platform_device msm_device_adspdec = {
	.name = "msm_adspdec",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_adspdec_database
	},
};

static struct android_pmem_platform_data android_pmem_audio_pdata = {
	.name = "pmem_audio",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_audio_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_audio_pdata },
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 1,
	.memory_type = MEMTYPE_EBI1,
};
static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static u32 msm_calculate_batt_capacity(u32 current_voltage);

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design     = 2800,
	.voltage_max_design     = 4300,
	.avail_chg_sources      = AC_CHG | USB_CHG ,
	.batt_technology        = POWER_SUPPLY_TECHNOLOGY_LION,
	.calculate_capacity     = &msm_calculate_batt_capacity,
};

static u32 msm_calculate_batt_capacity(u32 current_voltage)
{
	u32 low_voltage	 = msm_psy_batt_data.voltage_min_design;
	u32 high_voltage = msm_psy_batt_data.voltage_max_design;

	return (current_voltage - low_voltage) * 100
			/ (high_voltage - low_voltage);
}

static struct platform_device msm_batt_device = {
	.name               = "msm-battery",
	.id                 = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};

static struct platform_device *qrd1_devices[] __initdata = {
	&msm_device_dmov,
	&msm_device_smd,
	&msm_device_uart1,
	&msm_device_uart_dm1,
	&msm_gsbi0_qup_i2c_device,
	&msm_gsbi1_qup_i2c_device,
	&msm_device_otg,
	&msm_device_gadget_peripheral,
	&android_usb_device,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&msm_fb_device,
	&android_pmem_audio_device,
	&msm_device_snd,
	&msm_device_adspdec,
	&msm_batt_device,
	&msm_kgsl_3d0,
#ifdef CONFIG_BT
	&msm_bt_power_device,
#endif
	&mipi_dsi_truly_panel_device,
	&msm_wlan_ar6000_pm_device,
	&asoc_msm_pcm,
	&asoc_msm_dai0,
	&asoc_msm_dai1,
};

static unsigned pmem_kernel_ebi1_size = PMEM_KERNEL_EBI1_SIZE;
static int __init pmem_kernel_ebi1_size_setup(char *p)
{
	pmem_kernel_ebi1_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_kernel_ebi1_size", pmem_kernel_ebi1_size_setup);

static unsigned pmem_audio_size = MSM_PMEM_AUDIO_SIZE;
static int __init pmem_audio_size_setup(char *p)
{
	pmem_audio_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_audio_size", pmem_audio_size_setup);

static void __init msm_msm7627a_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;

	size = fb_size ? : MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n", size,
						addr, __pa(addr));
}

static struct memtype_reserve msm7627a_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
	.mdp_rev = MDP_REV_303,
};

#define GPIO_LCDC_BRDG_PD      128
#define GPIO_LCDC_BRDG_RESET_N 129
#define GPIO_LCD_DSI_SEL 125

static unsigned mipi_dsi_gpio[] = {
		GPIO_CFG(GPIO_LCDC_BRDG_RESET_N, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), /* LCDC_BRDG_RESET_N */
		GPIO_CFG(GPIO_LCDC_BRDG_PD, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), /* LCDC_BRDG_PD */
};

static unsigned lcd_dsi_sel_gpio[] = {
	GPIO_CFG(GPIO_LCD_DSI_SEL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
			GPIO_CFG_2MA),
};

enum {
	DSI_SINGLE_LANE = 1,
	DSI_TWO_LANES,
};

static int msm_fb_get_lane_config(void)
{
	pr_info("DSI_TWO_LANES\n");
	return DSI_TWO_LANES;
}

static int mipi_truly_sel_mode(int video_mode)
{
	int rc = 0;

	rc = gpio_request(GPIO_LCD_DSI_SEL, "lcd_dsi_sel");
	if (rc < 0)
		goto gpio_error;

	rc = gpio_tlmm_config(lcd_dsi_sel_gpio[0], GPIO_CFG_ENABLE);
	if (rc)
		goto gpio_error;

	rc = gpio_direction_output(GPIO_LCD_DSI_SEL, 1);
	if (!rc) {
		gpio_set_value_cansleep(GPIO_LCD_DSI_SEL, video_mode);
		return rc;
	} else {
		goto gpio_error;
	}

gpio_error:
	pr_err("mipi_truly_sel_mode failed\n");
	gpio_free(GPIO_LCD_DSI_SEL);
	return rc;
}

static int msm_fb_dsi_client_qrd1_reset(void)
{
	int rc = 0;

	rc = gpio_request(GPIO_LCDC_BRDG_RESET_N, "lcdc_brdg_reset_n");
	if (rc < 0) {
		pr_err("failed to request lcd brdg reset_n\n");
		return rc;
	}

	rc = gpio_tlmm_config(mipi_dsi_gpio[0], GPIO_CFG_ENABLE);
	if (rc < 0) {
		pr_err("Failed to enable LCDC Bridge reset enable\n");
		return rc;
	}

	rc = gpio_direction_output(GPIO_LCDC_BRDG_RESET_N, 1);
	if (rc < 0) {
		pr_err("Failed GPIO bridge pd\n");
		gpio_free(GPIO_LCDC_BRDG_RESET_N);
		return rc;
	}

	mipi_truly_sel_mode(1);

	return rc;
}

static int msm_fb_dsi_client_reset(void)
{
	int rc = 0;

	rc = msm_fb_dsi_client_qrd1_reset();
	return rc;
}

static int dsi_gpio_initialized;

static int mipi_dsi_panel_qrd1_power(int on)
{
	int rc = 0;

	if (!dsi_gpio_initialized) {
		rc = gpio_request(QRD_GPIO_BACKLIGHT_EN, "gpio_bkl_en");
		if (rc < 0)
			return rc;

		rc = gpio_tlmm_config(GPIO_CFG(QRD_GPIO_BACKLIGHT_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		if (rc < 0) {
			pr_err("failed GPIO_BACKLIGHT_EN tlmm config\n");
			return rc;
		}

		rc = gpio_direction_output(QRD_GPIO_BACKLIGHT_EN, 1);
		if (rc < 0) {
			pr_err("failed to enable backlight\n");
			gpio_free(QRD_GPIO_BACKLIGHT_EN);
			return rc;
		}
		dsi_gpio_initialized = 1;
	}

	gpio_set_value_cansleep(QRD_GPIO_BACKLIGHT_EN, !!on);

	if (!on) {
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 1);
		msleep(20);
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 0);
		msleep(20);
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 1);

	}

	return rc;
}

static int mipi_dsi_panel_power(int on)
{
	int rc = 0;

	rc = mipi_dsi_panel_qrd1_power(on);
	return rc;
}

#define MDP_303_VSYNC_GPIO 97

#ifdef CONFIG_FB_MSM_MDP303
static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio		= MDP_303_VSYNC_GPIO,
	.dsi_power_save		= mipi_dsi_panel_power,
	.dsi_client_reset	= msm_fb_dsi_client_reset,
	.get_lane_config	= msm_fb_get_lane_config,
};
#endif

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
}

static void __init size_pmem_devices(void)
{
#ifdef CONFIG_ANDROID_PMEM
	android_pmem_adsp_pdata.size = pmem_adsp_size;
	android_pmem_pdata.size = pmem_mdp_size;
	android_pmem_audio_pdata.size = pmem_audio_size;
#endif
}

static void __init reserve_memory_for(struct android_pmem_platform_data *p)
{
	msm7627a_reserve_table[p->memory_type].size += p->size;
}

static void __init reserve_pmem_memory(void)
{
#ifdef CONFIG_ANDROID_PMEM
	reserve_memory_for(&android_pmem_adsp_pdata);
	reserve_memory_for(&android_pmem_pdata);
	reserve_memory_for(&android_pmem_audio_pdata);
	msm7627a_reserve_table[MEMTYPE_EBI1].size += pmem_kernel_ebi1_size;
#endif
}

static void __init msm7627a_calculate_reserve_sizes(void)
{
	size_pmem_devices();
	reserve_pmem_memory();
}

static int msm7627a_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info msm7627a_reserve_info __initdata = {
	.memtype_reserve_table = msm7627a_reserve_table,
	.calculate_reserve_sizes = msm7627a_calculate_reserve_sizes,
	.paddr_to_memtype = msm7627a_paddr_to_memtype,
};

static void __init msm7627a_reserve(void)
{
	reserve_info = &msm7627a_reserve_info;
	msm_reserve();
}

static void __init msm_device_i2c_init(void)
{
	msm_gsbi0_qup_i2c_device.dev.platform_data = &msm_gsbi0_qup_i2c_pdata;
	msm_gsbi1_qup_i2c_device.dev.platform_data = &msm_gsbi1_qup_i2c_pdata;
}

static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "7k_handset",
	.pwr_key_delay_ms = 500, /* 0 will disable end key */
};

static struct platform_device hs_pdev = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};

static struct platform_device msm_proccomm_regulator_dev = {
	.name   = PROCCOMM_REGULATOR_DEV_NAME,
	.id     = -1,
	.dev    = {
		.platform_data = &msm7x27a_proccomm_regulator_data
	}
};

static void __init msm7627a_init_regulators(void)
{
	int rc = platform_device_register(&msm_proccomm_regulator_dev);
	if (rc)
		pr_err("%s: could not register regulator device: %d\n",
				__func__, rc);
}

#define UART1DM_RX_GPIO		45
static void __init msm_qrd1_init(void)
{
	msm7x2x_misc_init();
	msm7627a_init_regulators();
	msm_device_i2c_init();
#ifdef CONFIG_SERIAL_MSM_HS
	msm_uart_dm1_pdata.wakeup_irq = gpio_to_irq(UART1DM_RX_GPIO);
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
	msm_otg_pdata.swfi_latency = msm7627a_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;
	msm_device_otg.dev.platform_data = &msm_otg_pdata;
#endif
	msm_device_gadget_peripheral.dev.platform_data =
		&msm_gadget_pdata;
	platform_add_devices(qrd1_devices,
			ARRAY_SIZE(qrd1_devices));
	msm7627a_init_mmc();

#ifdef CONFIG_USB_EHCI_MSM_72K
	msm7627a_init_host();
#endif
	msm_pm_set_platform_data(msm7627a_pm_data,
				ARRAY_SIZE(msm7627a_pm_data));
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));

	msm_fb_add_devices();

#if defined(CONFIG_BT) && defined(CONFIG_MARIMBA_CORE)
	msm7627a_bt_power_init();
#endif

	msm7627a_camera_init();

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_RMI4_I2C) || \
	defined(CONFIG_TOUCHSCREEN_SYNAPTICS_RMI4_I2C_MODULE)
	i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
				synaptic_i2c_clearpad3k,
				ARRAY_SIZE(synaptic_i2c_clearpad3k));
#endif
	platform_device_register(&hs_pdev);

#ifdef CONFIG_MSM_RPC_VIBRATOR
	msm_init_pmic_vibrator();
#endif
}

static void __init qrd7627a_init_early(void)
{
	msm_msm7627a_allocate_memory_regions();
}

MACHINE_START(MSM7627A_QRD1, "QRD MSM7627a QRD1")
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io		= msm_common_io_init,
	.reserve	= msm7627a_reserve,
	.init_irq	= msm_init_irq,
	.init_machine	= msm_qrd1_init,
	.timer		= &msm_timer,
	.init_early	= qrd7627a_init_early,
	.handle_irq	= vic_handle_irq,
MACHINE_END

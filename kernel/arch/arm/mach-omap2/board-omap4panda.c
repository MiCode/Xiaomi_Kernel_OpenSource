/*
 * Board support file for OMAP4430 based PandaBoard.
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: David Anders <x0132446@ti.com>
 *
 * Based on mach-omap2/board-4430sdp.c
 *
 * Author: Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Based on mach-omap2/board-3430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/omapfb.h>
#include <linux/reboot.h>
#include <linux/usb/otg.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/wl12xx.h>
#include <linux/memblock.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/platform_data/ram_console.h>

#include <mach/hardware.h>
#include <mach/omap4-common.h>
#include <mach/emif.h>
#include <mach/lpddr2-elpida.h>
#include <mach/dmm.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <video/omapdss.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/remoteproc.h>
#include <plat/vram.h>
#include <video/omap-panel-generic-dpi.h>
#include "timer-gp.h"

#include "hsmmc.h"
#include "control.h"
#include "mux.h"
#include "common-board-devices.h"
#include "prm-regbits-44xx.h"
#include "prm44xx.h"
#include "pm.h"
#include "resetreason.h"

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
#include <linux/input/synaptics_dsx.h>
#define TM_SAMPLE1	(1)	// 2D only
#define TM_SAMPLE2	(2)	// 2D + 0D x 2
#define TM_SAMPLE3	(3)	// 2D + 0D x 4
#define SYNAPTICS_MODULE TM_SAMPLE1
#endif

#define PANDA_RAMCONSOLE_START	(PLAT_PHYS_OFFSET + SZ_512M)
#define PANDA_RAMCONSOLE_SIZE	SZ_2M

#define GPIO_HUB_POWER		1
#define GPIO_HUB_NRESET		62
#define GPIO_WIFI_PMENA		43
#define GPIO_WIFI_IRQ		53
#define HDMI_GPIO_CT_CP_HPD     60
#define HDMI_GPIO_HPD 63 /* Hot plug pin for HDMI */
#define HDMI_GPIO_LS_OE 41 /* Level shifter for HDMI */
#define TPS62361_GPIO   7 /* VCORE1 power control */
#define PANDA_BT_GPIO 46


#define PHYS_ADDR_SMC_SIZE	(SZ_1M * 3)
#define PHYS_ADDR_SMC_MEM	(0x80000000 + SZ_1G - PHYS_ADDR_SMC_SIZE)
#define OMAP_ION_HEAP_SECURE_INPUT_SIZE	(SZ_1M * 90)
#define PHYS_ADDR_DUCATI_SIZE	(SZ_1M * 105)
#define PHYS_ADDR_DUCATI_MEM	(PHYS_ADDR_SMC_MEM - PHYS_ADDR_DUCATI_SIZE - \
				OMAP_ION_HEAP_SECURE_INPUT_SIZE)

#define WILINK_UART_DEV_NAME	"/dev/ttyO1"


/* Synaptics changes for PandaBoard */
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
static int synaptics_gpio_setup(unsigned gpio, bool configure)
{
	int retval = 0;

	if (configure) {
		retval = gpio_request(gpio, "rmi4_attn");
		if (retval) {
			pr_err("%s: Failed to get attn gpio %d (code: %d)",
					 __func__, gpio, retval);
			return retval;
		}
		omap_mux_init_signal("gpmc_ad15.gpio_39", OMAP_PIN_INPUT_PULLUP);

		retval = gpio_direction_input(gpio);
		if (retval) {
			pr_err("%s: Failed to setup attn gpio %d (code: %d)",
					__func__, gpio, retval);
			gpio_free(gpio);
		}
	} else {
		pr_warn("%s: No way to deconfigure gpio %d",
				__func__, gpio);
	}

	return retval;
}

 #if (SYNAPTICS_MODULE == TM_SAMPLE1)
#define TM_SAMPLE1_ADDR 0x20
#define TM_SAMPLE1_ATTN 130

static unsigned char TM_SAMPLE1_f1a_button_codes[] = {};

static struct synaptics_rmi4_capacitance_button_map TM_SAMPLE1_capacitance_button_map = {
	.nbuttons = ARRAY_SIZE(TM_SAMPLE1_f1a_button_codes),
	.map = TM_SAMPLE1_f1a_button_codes,
};

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.irq_flags = IRQF_TRIGGER_FALLING,
	.irq_gpio = TM_SAMPLE1_ATTN,
 	.gpio_config = synaptics_gpio_setup,
	.capacitance_button_map = &TM_SAMPLE1_capacitance_button_map,
};

static struct i2c_board_info bus4_i2c_devices[] = {
 	{
 		I2C_BOARD_INFO("synaptics_rmi4_i2c", TM_SAMPLE1_ADDR),
 		.platform_data = &rmi4_platformdata,
     	},	
};

#elif (SYNAPTICS_MODULE == TM_SAMPLE2)
#define TM_SAMPLE2_ADDR 0x20
#define TM_SAMPLE2_ATTN 130

static unsigned char TM_SAMPLE2_f1a_button_codes[] = {KEY_MENU, KEY_BACK};

static struct synaptics_rmi4_capacitance_button_map TM_SAMPLE2_capacitance_button_map = {
	.nbuttons = ARRAY_SIZE(TM_SAMPLE2_f1a_button_codes),
	.map = TM_SAMPLE2_f1a_button_codes,
};

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.irq_flags = IRQF_TRIGGER_FALLING,
	.irq_gpio = TM_SAMPLE2_ATTN,
 	.gpio_config = synaptics_gpio_setup,
 	.capacitance_button_map = &TM_SAMPLE2_capacitance_button_map,
};

static struct i2c_board_info bus4_i2c_devices[] = {
 	{
 		I2C_BOARD_INFO("synaptics_rmi4_i2c", TM_SAMPLE2_ADDR),
 		.platform_data = &rmi4_platformdata,
     	},	
};
};

#elif (SYNAPTICS_MODULE == TM_SAMPLE3)
#define TM_SAMPLE3_ADDR	0x20
#define TM_SAMPLE3_ATTN	130

static unsigned char TM_SAMPLE3_f1a_button_codes[] = {KEY_MENU, KEY_HOME,KEY_BACK,KEY_SEARCH};

static struct synaptics_rmi4_capacitance_button_map TM_SAMPLE3_capacitance_button_map = {
	.nbuttons = ARRAY_SIZE(TM_SAMPLE3_f1a_button_codes),
	.map = TM_SAMPLE3_f1a_button_codes,
};

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.irq_flags = IRQF_TRIGGER_FALLING,
	.irq_gpio = TM_SAMPLE3_ATTN,
	.gpio_config = synaptics_gpio_setup,
	.capacitance_button_map = &TM_SAMPLE3_capacitance_button_map,
};

static struct i2c_board_info bus4_i2c_devices[] = {
 	{
 		I2C_BOARD_INFO("synaptics_rmi4_i2c", TM_SAMPLE3_ADDR),
 		.platform_data = &rmi4_platformdata,
     	},	
};
#endif

void __init i2c_device_setup(void)
{
	pr_info(">>>>I2C device setup");
	if (ARRAY_SIZE(bus4_i2c_devices)) {
		i2c_register_board_info(4, bus4_i2c_devices,
				ARRAY_SIZE(bus4_i2c_devices));
	}
}
#endif
/* End of Synaptics changes for PandaBoard */

static struct gpio_led gpio_leds[] = {
	{
		.name			= "pandaboard::status1",
		.default_trigger	= "heartbeat",
		.gpio			= 7,
	},
	{
		.name			= "pandaboard::status2",
		.default_trigger	= "mmc0",
		.gpio			= 8,
	},
};

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
	},
};

/* GPIO_KEY for the panda */
static struct gpio_keys_button panda_gpio_keys_buttons[] = {
	[0] = {
		.code			= KEY_HOME,
		.gpio			= 113,
		.desc			= "user_button",
		.active_low		= 1,
		.debounce_interval	= 5,
	},
};

static struct gpio_keys_platform_data panda_gpio_keys = {
	.buttons		= panda_gpio_keys_buttons,
	.nbuttons		= ARRAY_SIZE(panda_gpio_keys_buttons),
	.rep			= 0,
};

static struct platform_device panda_gpio_keys_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &panda_gpio_keys,
	},
};

/* TODO: handle suspend/resume here.
 * Upon every suspend, make sure the wilink chip is
 * capable enough to wake-up the OMAP host.
 */
static int plat_wlink_kim_suspend(struct platform_device *pdev, pm_message_t
		state)
{
	return 0;
}

static int plat_wlink_kim_resume(struct platform_device *pdev)
{
	return 0;
}

/* wl128x BT, FM, GPS connectivity chip */
static struct ti_st_plat_data wilink_pdata = {
	.nshutdown_gpio = PANDA_BT_GPIO,
	.dev_name = WILINK_UART_DEV_NAME,
	.flow_cntrl = 1,
	.baud_rate = 3686400,
	.suspend = plat_wlink_kim_suspend,
	.resume = plat_wlink_kim_resume,
};

static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

/* wl127x BT, FM, GPS connectivity chip */
static struct platform_device wl1271_device = {
	.name	= "kim",
	.id	= -1,
	.dev.platform_data = &wilink_pdata,
};


static struct platform_device *panda_devices[] __initdata = {
	&leds_gpio,
	&wl1271_device,
	&btwilink_device,
	&panda_gpio_keys_device,
};

static void __init omap4_panda_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
}

static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[2] = OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset  = false,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = -EINVAL,
	.reset_gpio_port[2]  = -EINVAL
};

static struct gpio panda_ehci_gpios[] __initdata = {
	{ GPIO_HUB_POWER,	GPIOF_OUT_INIT_LOW,  "hub_power"  },
	{ GPIO_HUB_NRESET,	GPIOF_OUT_INIT_LOW,  "hub_nreset" },
};

static void __init omap4_ehci_init(void)
{
	int ret;
	struct clk *phy_ref_clk;

	/* FREF_CLK3 provides the 19.2 MHz reference clock to the PHY */
	phy_ref_clk = clk_get(NULL, "auxclk3_ck");
	if (IS_ERR(phy_ref_clk)) {
		pr_err("Cannot request auxclk3\n");
		return;
	}
	clk_set_rate(phy_ref_clk, 19200000);
	clk_enable(phy_ref_clk);

	/* disable the power to the usb hub prior to init and reset phy+hub */
	ret = gpio_request_array(panda_ehci_gpios,
				 ARRAY_SIZE(panda_ehci_gpios));
	if (ret) {
		pr_err("Unable to initialize EHCI power/reset\n");
		return;
	}

	gpio_export(GPIO_HUB_POWER, 0);
	gpio_export(GPIO_HUB_NRESET, 0);
	gpio_set_value(GPIO_HUB_NRESET, 1);

	usbhs_init(&usbhs_bdata);

	/* enable power to hub */
	gpio_set_value(GPIO_HUB_POWER, 1);
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_GADGET_MUSB_HDRC
	.mode			= MUSB_PERIPHERAL,
#else
	.mode			= MUSB_OTG,
#endif
	.power			= 100,
};

static struct twl4030_usb_data omap4_usbphy_data = {
	.phy_init	= omap4430_phy_init,
	.phy_exit	= omap4430_phy_exit,
	.phy_power	= omap4430_phy_power,
	.phy_set_clock	= omap4430_phy_set_clk,
	.phy_suspend	= omap4430_phy_suspend,
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_wp	= -EINVAL,
		.gpio_cd	= -EINVAL,
	},
	{
		.name		= "wl1271",
		.mmc		= 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.gpio_wp	= -EINVAL,
		.gpio_cd	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195,
		.nonremovable	= true,
	},
	{}	/* Terminator */
};

static struct regulator_consumer_supply omap4_panda_vmmc_supply[] = {
	{
		.supply = "vmmc",
		.dev_name = "omap_hsmmc.0",
	},
};

static struct regulator_consumer_supply omap4_panda_vmmc5_supply = {
	.supply = "vmmc",
	.dev_name = "omap_hsmmc.4",
};

static struct regulator_init_data panda_vmmc5 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &omap4_panda_vmmc5_supply,
};

static struct fixed_voltage_config panda_vwlan = {
	.supply_name = "vwl1271",
	.microvolts = 1800000, /* 1.8V */
	.gpio = GPIO_WIFI_PMENA,
	.startup_delay = 70000, /* 70msec */
	.enable_high = 1,
	.enabled_at_boot = 0,
	.init_data = &panda_vmmc5,
};

static struct platform_device omap_vwlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data = &panda_vwlan,
	},
};

struct wl12xx_platform_data omap_panda_wlan_data  __initdata = {
	.irq = OMAP_GPIO_IRQ(GPIO_WIFI_IRQ),
	/* PANDA ref clock is 38.4 MHz */
	.board_ref_clock = 2,
};

static int omap4_twl6030_hsmmc_late_init(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct omap_mmc_platform_data *pdata = dev->platform_data;

	if (!pdata) {
		dev_err(dev, "%s: NULL platform data\n", __func__);
		return -EINVAL;
	}
	/* Setting MMC1 Card detect Irq */
	if (pdev->id == 0) {
		ret = twl6030_mmc_card_detect_config();
		 if (ret)
			dev_err(dev, "%s: Error card detect config(%d)\n",
				__func__, ret);
		 else
			pdata->slots[0].card_detect = twl6030_mmc_card_detect;
	}
	return ret;
}

static __init void omap4_twl6030_hsmmc_set_late_init(struct device *dev)
{
	struct omap_mmc_platform_data *pdata;

	/* dev can be null if CONFIG_MMC_OMAP_HS is not set */
	if (!dev) {
		pr_err("Failed omap4_twl6030_hsmmc_set_late_init\n");
		return;
	}
	pdata = dev->platform_data;

	pdata->init =	omap4_twl6030_hsmmc_late_init;
}

static int __init omap4_twl6030_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	omap2_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(c->dev);

	return 0;
}

static struct regulator_init_data omap4_panda_vaux2 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap4_panda_vaux3 = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

/* VMMC1 for MMC1 card */
static struct regulator_init_data omap4_panda_vmmc = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = omap4_panda_vmmc_supply,
};

static struct regulator_init_data omap4_panda_vpp = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap4_panda_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap4_panda_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_consumer_supply panda_vdac_supply[] = {
	{
		.supply = "hdmi_vref",
	},
};

static struct regulator_init_data omap4_panda_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = ARRAY_SIZE(panda_vdac_supply),
	.consumer_supplies      = panda_vdac_supply,
};

static struct regulator_init_data omap4_panda_vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap4_panda_clk32kg = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static void omap4_audio_conf(void)
{
	/* twl6040 naudint */
	omap_mux_init_signal("sys_nirq2.sys_nirq2", \
		OMAP_PIN_INPUT_PULLUP);
}

static struct twl4030_codec_audio_data twl6040_audio = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step	= 0x0f,
	.hs_right_step	= 0x0f,
	.hf_left_step	= 0x1d,
	.hf_right_step	= 0x1d,
	.hs_switch_dev  = 0x1,
	.hs_forced_hs_state = 0x1
};

static struct twl4030_codec_data twl6040_codec = {
	.audio		= &twl6040_audio,
	.audpwron_gpio	= 127,
	.naudint_irq	= OMAP44XX_IRQ_SYS_2N,
	.irq_base	= TWL6040_CODEC_IRQ_BASE,
};

static struct twl4030_platform_data omap4_panda_twldata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* Regulators */
	.vmmc		= &omap4_panda_vmmc,
	.vpp		= &omap4_panda_vpp,
	.vana		= &omap4_panda_vana,
	.vcxio		= &omap4_panda_vcxio,
	.vdac		= &omap4_panda_vdac,
	.vusb		= &omap4_panda_vusb,
	.vaux2		= &omap4_panda_vaux2,
	.vaux3		= &omap4_panda_vaux3,
	.clk32kg	= &omap4_panda_clk32kg,
	.usb		= &omap4_usbphy_data,

	/* children */
	.codec		= &twl6040_codec,
};

/*
 * Display monitor features are burnt in their EEPROM as EDID data. The EEPROM
 * is connected as I2C slave device, and can be accessed at address 0x50
 */
static struct i2c_board_info __initdata panda_i2c_eeprom[] = {
	{
		I2C_BOARD_INFO("eeprom", 0x50),
	},
};

static int __init omap4_panda_i2c_init(void)
{
	omap4_pmic_init("twl6030", &omap4_panda_twldata);
	omap_register_i2c_bus(2, 400, NULL, 0);
	/*
	 * Bus 3 is attached to the DVI port where devices like the pico DLP
	 * projector don't work reliably with 400kHz
	 */
	omap_register_i2c_bus(3, 100, panda_i2c_eeprom,
					ARRAY_SIZE(panda_i2c_eeprom));
	if(ARRAY_SIZE(bus4_i2c_devices))
		omap_register_i2c_bus(4, 400, bus4_i2c_devices, ARRAY_SIZE(bus4_i2c_devices));
	else
		omap_register_i2c_bus(4, 400, NULL, 0);
	return 0;
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	/* WLAN IRQ - GPIO 53 */
	OMAP4_MUX(GPMC_NCS3, OMAP_MUX_MODE3 | OMAP_PIN_INPUT),
	/* WLAN POWER ENABLE - GPIO 43 */
	OMAP4_MUX(GPMC_A19, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT),
	/* WLAN SDIO: MMC5 CMD */
	OMAP4_MUX(SDMMC5_CMD, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC5 CLK */
	OMAP4_MUX(SDMMC5_CLK, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC5 DAT[0-3] */
	OMAP4_MUX(SDMMC5_DAT0, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT1, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT2, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT3, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* gpio 0 - TFP410 PD */
	OMAP4_MUX(KPD_COL1, OMAP_PIN_OUTPUT | OMAP_MUX_MODE3),
	/* dispc2_data23 */
	OMAP4_MUX(USBB2_ULPITLL_STP, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data22 */
	OMAP4_MUX(USBB2_ULPITLL_DIR, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data21 */
	OMAP4_MUX(USBB2_ULPITLL_NXT, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data20 */
	OMAP4_MUX(USBB2_ULPITLL_DAT0, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data19 */
	OMAP4_MUX(USBB2_ULPITLL_DAT1, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data18 */
	OMAP4_MUX(USBB2_ULPITLL_DAT2, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data15 */
	OMAP4_MUX(USBB2_ULPITLL_DAT3, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data14 */
	OMAP4_MUX(USBB2_ULPITLL_DAT4, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data13 */
	OMAP4_MUX(USBB2_ULPITLL_DAT5, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data12 */
	OMAP4_MUX(USBB2_ULPITLL_DAT6, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data11 */
	OMAP4_MUX(USBB2_ULPITLL_DAT7, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data10 */
	OMAP4_MUX(DPM_EMU3, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data9 */
	OMAP4_MUX(DPM_EMU4, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data16 */
	OMAP4_MUX(DPM_EMU5, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data17 */
	OMAP4_MUX(DPM_EMU6, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_hsync */
	OMAP4_MUX(DPM_EMU7, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_pclk */
	OMAP4_MUX(DPM_EMU8, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_vsync */
	OMAP4_MUX(DPM_EMU9, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_de */
	OMAP4_MUX(DPM_EMU10, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data8 */
	OMAP4_MUX(DPM_EMU11, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data7 */
	OMAP4_MUX(DPM_EMU12, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data6 */
	OMAP4_MUX(DPM_EMU13, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data5 */
	OMAP4_MUX(DPM_EMU14, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data4 */
	OMAP4_MUX(DPM_EMU15, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data3 */
	OMAP4_MUX(DPM_EMU16, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data2 */
	OMAP4_MUX(DPM_EMU17, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data1 */
	OMAP4_MUX(DPM_EMU18, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data0 */
	OMAP4_MUX(DPM_EMU19, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

static inline void __init board_serial_init(void)
{
	omap_serial_init();
}
#else
#define board_mux	NULL

static inline void __init board_serial_init(void)
{
	omap_serial_init();
}
#endif

/* Display DVI */
#define PANDA_DVI_TFP410_POWER_DOWN_GPIO	0

static int omap4_panda_enable_dvi(struct omap_dss_device *dssdev)
{
	gpio_set_value(dssdev->reset_gpio, 1);
	return 0;
}

static void omap4_panda_disable_dvi(struct omap_dss_device *dssdev)
{
	gpio_set_value(dssdev->reset_gpio, 0);
}

/* Using generic display panel */
static struct panel_generic_dpi_data omap4_dvi_panel = {
	.name			= "generic_720p",
	.platform_enable	= omap4_panda_enable_dvi,
	.platform_disable	= omap4_panda_disable_dvi,
};

struct omap_dss_device omap4_panda_dvi_device = {
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.name			= "dvi",
	.driver_name		= "generic_dpi_panel",
	.data			= &omap4_dvi_panel,
	.phy.dpi.data_lines	= 24,
	.reset_gpio		= PANDA_DVI_TFP410_POWER_DOWN_GPIO,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
};

int __init omap4_panda_dvi_init(void)
{
	int r;

	/* Requesting TFP410 DVI GPIO and disabling it, at bootup */
	r = gpio_request_one(omap4_panda_dvi_device.reset_gpio,
				GPIOF_OUT_INIT_LOW, "DVI PD");
	if (r)
		pr_err("Failed to get DVI powerdown GPIO\n");

	return r;
}

static struct gpio panda_hdmi_gpios[] = {
	{ HDMI_GPIO_CT_CP_HPD,	GPIOF_OUT_INIT_HIGH, "hdmi_gpio_hpd"   },
	{ HDMI_GPIO_LS_OE,	GPIOF_OUT_INIT_HIGH, "hdmi_gpio_ls_oe" },
};

static void omap4_panda_hdmi_mux_init(void)
{
	u32 r;
	int status;
	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_mux_init_signal("hdmi_hpd.hdmi_hpd",
				OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("gpmc_wait2.gpio_100",
			OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("hdmi_cec.hdmi_cec",
			OMAP_PIN_INPUT_PULLUP);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
	omap_mux_init_signal("hdmi_ddc_scl.hdmi_ddc_scl",
			OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("hdmi_ddc_sda.hdmi_ddc_sda",
			OMAP_PIN_INPUT_PULLUP);

	/* strong pullup on DDC lines using unpublished register */
	r = ((1 << 24) | (1 << 28)) ;
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_1);

	gpio_request(HDMI_GPIO_HPD, NULL);
	omap_mux_init_gpio(HDMI_GPIO_HPD, OMAP_PIN_INPUT | OMAP_PULL_ENA);
	gpio_direction_input(HDMI_GPIO_HPD);

	status = gpio_request_array(panda_hdmi_gpios,
			ARRAY_SIZE(panda_hdmi_gpios));
	if (status)
		pr_err("%s: Cannot request HDMI GPIOs %x \n", __func__, status);
}

static struct omap_dss_device  omap4_panda_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.clocks	= {
		.dispc	= {
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},
		.hdmi	= {
			.regn	= 15,
			.regm2	= 1,
		},
	},
	.hpd_gpio = HDMI_GPIO_HPD,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
};

static struct omap_dss_device *omap4_panda_dss_devices[] = {
	&omap4_panda_dvi_device,
	&omap4_panda_hdmi_device,
};

static struct omap_dss_board_info omap4_panda_dss_data = {
	.num_devices	= ARRAY_SIZE(omap4_panda_dss_devices),
	.devices	= omap4_panda_dss_devices,
	.default_device	= &omap4_panda_dvi_device,
};

/*
 * LPDDR2 Configeration Data:
 * The memory organisation is as below :
 *	EMIF1 - CS0 -	2 Gb
 *		CS1 -	2 Gb
 *	EMIF2 - CS0 -	2 Gb
 *		CS1 -	2 Gb
 *	--------------------
 *	TOTAL -		8 Gb
 *
 * Same devices installed on EMIF1 and EMIF2
 */
static __initdata struct emif_device_details emif_devices = {
	.cs0_device = &lpddr2_elpida_2G_S4_dev,
	.cs1_device = &lpddr2_elpida_2G_S4_dev
};

void omap4_panda_display_init(void)
{
	int r;

	r = omap4_panda_dvi_init();
	if (r)
		pr_err("error initializing panda DVI\n");

	omap4_panda_hdmi_mux_init();
	omap_display_init(&omap4_panda_dss_data);
}

static int panda_notifier_call(struct notifier_block *this,
					unsigned long code, void *cmd)
{
	void __iomem *sar_base;
	u32 v = OMAP4430_RST_GLOBAL_COLD_SW_MASK;

	sar_base = omap4_get_sar_ram_base();

	if (!sar_base)
		return notifier_from_errno(-ENOMEM);

	if ((code == SYS_RESTART) && (cmd != NULL)) {
		/* cmd != null; case: warm boot */
		if (!strcmp(cmd, "bootloader")) {
			/* Save reboot mode in scratch memory */
			strcpy(sar_base + 0xA0C, cmd);
			v |= OMAP4430_RST_GLOBAL_WARM_SW_MASK;
		} else if (!strcmp(cmd, "recovery")) {
			/* Save reboot mode in scratch memory */
			strcpy(sar_base + 0xA0C, cmd);
			v |= OMAP4430_RST_GLOBAL_WARM_SW_MASK;
		} else {
			v |= OMAP4430_RST_GLOBAL_COLD_SW_MASK;
		}
	}

	omap4_prm_write_inst_reg(0xfff, OMAP4430_PRM_DEVICE_INST,
			OMAP4_RM_RSTST);
	omap4_prm_write_inst_reg(v, OMAP4430_PRM_DEVICE_INST, OMAP4_RM_RSTCTRL);
	v = omap4_prm_read_inst_reg(WKUP_MOD, OMAP4_RM_RSTCTRL);

	return NOTIFY_DONE;
}

static struct notifier_block panda_reboot_notifier = {
	.notifier_call = panda_notifier_call,
};

#define PANDA_FB_RAM_SIZE                SZ_16M /* 1920?1080*4 * 2 */
static struct omapfb_platform_data panda_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = PANDA_FB_RAM_SIZE,
			},
		},
	},
};

static struct resource ramconsole_resources[] = {
	{
		.flags  = IORESOURCE_MEM,
		.start	= PANDA_RAMCONSOLE_START,
		.end	= PANDA_RAMCONSOLE_START + PANDA_RAMCONSOLE_SIZE - 1,
	},
};

static struct ram_console_platform_data ramconsole_pdata;

static struct platform_device ramconsole_device = {
	.name           = "ram_console",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(ramconsole_resources),
	.resource       = ramconsole_resources,
	.dev		= {
		.platform_data = &ramconsole_pdata,
	},
};

extern void __init omap4_panda_android_init(void);

static void __init omap4_panda_init(void)
{
	int package = OMAP_PACKAGE_CBS;
	int status;

	omap_emif_setup_device_details(&emif_devices, &emif_devices);

	if (omap_rev() == OMAP4430_REV_ES1_0)
		package = OMAP_PACKAGE_CBL;
	omap4_mux_init(board_mux, NULL, package);

	if (wl12xx_set_platform_data(&omap_panda_wlan_data))
		pr_err("error setting wl12xx data\n");

	register_reboot_notifier(&panda_reboot_notifier);
	ramconsole_pdata.bootinfo = omap4_get_resetreason();
	platform_device_register(&ramconsole_device);
	omap4_panda_i2c_init();
	omap4_audio_conf();

	if (cpu_is_omap4430())
		panda_gpio_keys_buttons[0].gpio = 121;

	platform_add_devices(panda_devices, ARRAY_SIZE(panda_devices));
	platform_device_register(&omap_vwlan_device);
	board_serial_init();
	omap4_twl6030_hsmmc_init(mmc);
	omap4_ehci_init();
	usb_musb_init(&musb_board_data);

	omap_dmm_init();
	omap_vram_set_sdram_vram(PANDA_FB_RAM_SIZE, 0);
	omapfb_set_platform_data(&panda_fb_pdata);
	omap4_panda_display_init();

	if (cpu_is_omap446x()) {
		/* Vsel0 = gpio, vsel1 = gnd */
		status = omap_tps6236x_board_setup(true, TPS62361_GPIO, -1,
					OMAP_PIN_OFF_OUTPUT_HIGH, -1);
		if (status)
			pr_err("TPS62361 initialization failed: %d\n", status);
	}
	omap_enable_smartreflex_on_init();
}

static void __init omap4_panda_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();
}

static void __init omap4_panda_reserve(void)
{
	/* do the static reservations first */
	memblock_remove(PANDA_RAMCONSOLE_START, PANDA_RAMCONSOLE_SIZE);
	memblock_remove(PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);
	memblock_remove(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE);
	/* ipu needs to recognize secure input buffer area as well */
	omap_ipu_set_static_mempool(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE +
					OMAP_ION_HEAP_SECURE_INPUT_SIZE);

	omap_reserve();
}

MACHINE_START(OMAP4_PANDA, "OMAP4 Panda board")
	/* Maintainer: David Anders - Texas Instruments Inc */
	.boot_params	= 0x80000100,
	.reserve	= omap4_panda_reserve,
	.map_io		= omap4_panda_map_io,
	.init_early	= omap4_panda_init_early,
	.init_irq	= gic_init_irq,
	.init_machine	= omap4_panda_init,
	.timer		= &omap_timer,
MACHINE_END

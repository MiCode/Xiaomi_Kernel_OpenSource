/* linux/arch/arm/mach-msm/board-sapphire-panel.c
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
#include <linux/leds.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <linux/io.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>

#include <mach/msm_fb.h>
#include <mach/vreg.h>
#include <mach/htc_pwrsink.h>

#include "gpio_chip.h"
#include "board-sapphire.h"
#include "proc_comm.h"
#include "devices.h"

#define DEBUG_SAPPHIRE_PANEL 0
#define userid 0xD10

#define VSYNC_GPIO 97

enum sapphire_panel_type {
	SAPPHIRE_PANEL_SHARP = 0,
	SAPPHIRE_PANEL_TOPPOLY,
	NUM_OF_SAPPHIRE_PANELS,
};
static int g_panel_id = -1 ;
static int g_panel_inited = 0 ;

#define SAPPHIRE_DEFAULT_BACKLIGHT_BRIGHTNESS	132
#define GOOGLE_DEFAULT_BACKLIGHT_BRIGHTNESS 	102
#define SDBB 					SAPPHIRE_DEFAULT_BACKLIGHT_BRIGHTNESS
#define GDBB 					GOOGLE_DEFAULT_BACKLIGHT_BRIGHTNESS

static int sapphire_backlight_off;
static int sapphire_backlight_brightness =
					SAPPHIRE_DEFAULT_BACKLIGHT_BRIGHTNESS;

static uint8_t sapphire_backlight_last_level = 33;
static DEFINE_MUTEX(sapphire_backlight_lock);

/* Divide dimming level into 12 sections, and restrict maximum level to 27 */
#define DIMMING_STEPS       12
static unsigned dimming_levels[NUM_OF_SAPPHIRE_PANELS][DIMMING_STEPS] = {
	{0, 1, 2, 3, 6, 9, 11, 13, 16, 19, 22, 25},         /* Sharp */
	{0, 1, 2, 4, 7, 10, 13, 15, 18, 21, 24, 27},        /* Toppolly */
};
static unsigned pwrsink_percents[] = {0, 6, 8, 15, 26, 34, 46, 54, 65, 77, 87,
				      100};

static void sapphire_set_backlight_level(uint8_t level)
{
	unsigned dimming_factor = 255/DIMMING_STEPS + 1;
	int index, new_level ;
	unsigned percent;
	unsigned long flags;
	int i = 0;

	/* Non-linear transform for the difference between two 
         * kind of default backlight settings. 
	 */
	new_level = level<=GDBB ? 
		level*SDBB/GDBB : (SDBB + (level-GDBB)*(255-SDBB) / (255-GDBB)) ;
	index = new_level/dimming_factor ;

#if DEBUG_SAPPHIRE_PANEL
	printk(KERN_INFO "level=%d, new level=%d, dimming_levels[%d]=%d\n",
		level, new_level, index, dimming_levels[g_panel_id][index]);
#endif
	percent = pwrsink_percents[index];
	level = dimming_levels[g_panel_id][index];

	if (sapphire_backlight_last_level == level)
		return;

	if (level == 0) {
		gpio_set_value(27, 0);
		msleep(2);
	} else {
		local_irq_save(flags);
		if (sapphire_backlight_last_level == 0) {
			gpio_set_value(27, 1);
			udelay(40);
			sapphire_backlight_last_level = 33;
		}
		i = (sapphire_backlight_last_level - level + 33) % 33;
		while (i-- > 0) {
			gpio_set_value(27, 0);
			udelay(1);
			gpio_set_value(27, 1);
			udelay(1);
		}
		local_irq_restore(flags);
	}
	sapphire_backlight_last_level = level;
	htc_pwrsink_set(PWRSINK_BACKLIGHT, percent);
}

#define MDDI_CLIENT_CORE_BASE  0x108000
#define LCD_CONTROL_BLOCK_BASE 0x110000
#define SPI_BLOCK_BASE         0x120000
#define I2C_BLOCK_BASE         0x130000
#define PWM_BLOCK_BASE         0x140000
#define GPIO_BLOCK_BASE        0x150000
#define SYSTEM_BLOCK1_BASE     0x160000
#define SYSTEM_BLOCK2_BASE     0x170000


#define	DPSUS       (MDDI_CLIENT_CORE_BASE|0x24)
#define	SYSCLKENA   (MDDI_CLIENT_CORE_BASE|0x2C)
#define	PWM0OFF	      (PWM_BLOCK_BASE|0x1C)

#define V_VDDE2E_VDD2_GPIO 0
#define V_VDDE2E_VDD2_GPIO_5M 89
#define MDDI_RST_N 82

#define	MDDICAP0    (MDDI_CLIENT_CORE_BASE|0x00)
#define	MDDICAP1    (MDDI_CLIENT_CORE_BASE|0x04)
#define	MDDICAP2    (MDDI_CLIENT_CORE_BASE|0x08)
#define	MDDICAP3    (MDDI_CLIENT_CORE_BASE|0x0C)
#define	MDCAPCHG    (MDDI_CLIENT_CORE_BASE|0x10)
#define	MDCRCERC    (MDDI_CLIENT_CORE_BASE|0x14)
#define	TTBUSSEL    (MDDI_CLIENT_CORE_BASE|0x18)
#define	DPSET0      (MDDI_CLIENT_CORE_BASE|0x1C)
#define	DPSET1      (MDDI_CLIENT_CORE_BASE|0x20)
#define	DPSUS       (MDDI_CLIENT_CORE_BASE|0x24)
#define	DPRUN       (MDDI_CLIENT_CORE_BASE|0x28)
#define	SYSCKENA    (MDDI_CLIENT_CORE_BASE|0x2C)
#define	TESTMODE    (MDDI_CLIENT_CORE_BASE|0x30)
#define	FIFOMONI    (MDDI_CLIENT_CORE_BASE|0x34)
#define	INTMONI     (MDDI_CLIENT_CORE_BASE|0x38)
#define	MDIOBIST    (MDDI_CLIENT_CORE_BASE|0x3C)
#define	MDIOPSET    (MDDI_CLIENT_CORE_BASE|0x40)
#define	BITMAP0     (MDDI_CLIENT_CORE_BASE|0x44)
#define	BITMAP1     (MDDI_CLIENT_CORE_BASE|0x48)
#define	BITMAP2     (MDDI_CLIENT_CORE_BASE|0x4C)
#define	BITMAP3     (MDDI_CLIENT_CORE_BASE|0x50)
#define	BITMAP4     (MDDI_CLIENT_CORE_BASE|0x54)

#define	SRST        (LCD_CONTROL_BLOCK_BASE|0x00)
#define	PORT_ENB    (LCD_CONTROL_BLOCK_BASE|0x04)
#define	START       (LCD_CONTROL_BLOCK_BASE|0x08)
#define	PORT        (LCD_CONTROL_BLOCK_BASE|0x0C)
#define	CMN         (LCD_CONTROL_BLOCK_BASE|0x10)
#define	GAMMA       (LCD_CONTROL_BLOCK_BASE|0x14)
#define	INTFLG      (LCD_CONTROL_BLOCK_BASE|0x18)
#define	INTMSK      (LCD_CONTROL_BLOCK_BASE|0x1C)
#define	MPLFBUF     (LCD_CONTROL_BLOCK_BASE|0x20)
#define	HDE_LEFT    (LCD_CONTROL_BLOCK_BASE|0x24)
#define	VDE_TOP     (LCD_CONTROL_BLOCK_BASE|0x28)
#define	PXL         (LCD_CONTROL_BLOCK_BASE|0x30)
#define	HCYCLE      (LCD_CONTROL_BLOCK_BASE|0x34)
#define	HSW         (LCD_CONTROL_BLOCK_BASE|0x38)
#define	HDE_START   (LCD_CONTROL_BLOCK_BASE|0x3C)
#define	HDE_SIZE    (LCD_CONTROL_BLOCK_BASE|0x40)
#define	VCYCLE      (LCD_CONTROL_BLOCK_BASE|0x44)
#define	VSW         (LCD_CONTROL_BLOCK_BASE|0x48)
#define	VDE_START   (LCD_CONTROL_BLOCK_BASE|0x4C)
#define	VDE_SIZE    (LCD_CONTROL_BLOCK_BASE|0x50)
#define	WAKEUP      (LCD_CONTROL_BLOCK_BASE|0x54)
#define	WSYN_DLY    (LCD_CONTROL_BLOCK_BASE|0x58)
#define	REGENB      (LCD_CONTROL_BLOCK_BASE|0x5C)
#define	VSYNIF      (LCD_CONTROL_BLOCK_BASE|0x60)
#define	WRSTB       (LCD_CONTROL_BLOCK_BASE|0x64)
#define	RDSTB       (LCD_CONTROL_BLOCK_BASE|0x68)
#define	ASY_DATA    (LCD_CONTROL_BLOCK_BASE|0x6C)
#define	ASY_DATB    (LCD_CONTROL_BLOCK_BASE|0x70)
#define	ASY_DATC    (LCD_CONTROL_BLOCK_BASE|0x74)
#define	ASY_DATD    (LCD_CONTROL_BLOCK_BASE|0x78)
#define	ASY_DATE    (LCD_CONTROL_BLOCK_BASE|0x7C)
#define	ASY_DATF    (LCD_CONTROL_BLOCK_BASE|0x80)
#define	ASY_DATG    (LCD_CONTROL_BLOCK_BASE|0x84)
#define	ASY_DATH    (LCD_CONTROL_BLOCK_BASE|0x88)
#define	ASY_CMDSET  (LCD_CONTROL_BLOCK_BASE|0x8C)

#define	SSICTL      (SPI_BLOCK_BASE|0x00)
#define	SSITIME     (SPI_BLOCK_BASE|0x04)
#define	SSITX       (SPI_BLOCK_BASE|0x08)
#define	SSIRX       (SPI_BLOCK_BASE|0x0C)
#define	SSIINTC     (SPI_BLOCK_BASE|0x10)
#define	SSIINTS     (SPI_BLOCK_BASE|0x14)
#define	SSIDBG1     (SPI_BLOCK_BASE|0x18)
#define	SSIDBG2     (SPI_BLOCK_BASE|0x1C)
#define	SSIID       (SPI_BLOCK_BASE|0x20)

#define	WKREQ       (SYSTEM_BLOCK1_BASE|0x00)
#define	CLKENB      (SYSTEM_BLOCK1_BASE|0x04)
#define	DRAMPWR     (SYSTEM_BLOCK1_BASE|0x08)
#define	INTMASK     (SYSTEM_BLOCK1_BASE|0x0C)
#define	GPIOSEL     (SYSTEM_BLOCK2_BASE|0x00)

#define	GPIODATA    (GPIO_BLOCK_BASE|0x00)
#define	GPIODIR     (GPIO_BLOCK_BASE|0x04)
#define	GPIOIS      (GPIO_BLOCK_BASE|0x08)
#define	GPIOIBE     (GPIO_BLOCK_BASE|0x0C)
#define	GPIOIEV     (GPIO_BLOCK_BASE|0x10)
#define	GPIOIE      (GPIO_BLOCK_BASE|0x14)
#define	GPIORIS     (GPIO_BLOCK_BASE|0x18)
#define	GPIOMIS     (GPIO_BLOCK_BASE|0x1C)
#define	GPIOIC      (GPIO_BLOCK_BASE|0x20)
#define	GPIOOMS     (GPIO_BLOCK_BASE|0x24)
#define	GPIOPC      (GPIO_BLOCK_BASE|0x28)
#define	GPIOID      (GPIO_BLOCK_BASE|0x30)

#define SPI_WRITE(reg, val) \
	{ SSITX,        0x00010000 | (((reg) & 0xff) << 8) | ((val) & 0xff) }, \
	{ 0, 5 },

#define SPI_WRITE1(reg) \
	{ SSITX,        (reg) & 0xff }, \
	{ 0, 5 },

struct mddi_table {
	uint32_t reg;
	uint32_t value;
};
static struct mddi_table mddi_toshiba_init_table[] = {
	{ DPSET0,       0x09e90046 },
	{ DPSET1,       0x00000118 },
	{ DPSUS,        0x00000000 },
	{ DPRUN,        0x00000001 },
	{ 1,            14         }, /* msleep 14 */
	{ SYSCKENA,     0x00000001 },
	/*{ CLKENB,       0x000000EF } */
	{ CLKENB,       0x0000A1EF },  /*    # SYS.CLKENB  # Enable clocks for each module (without DCLK , i2cCLK) */
	/*{ CLKENB,       0x000025CB },  Clock enable register */

	{ GPIODATA,     0x02000200 },  /*   # GPI .GPIODATA  # GPIO2(RESET_LCD_N) set to 0 , GPIO3(eDRAM_Power) set to 0 */
	{ GPIODIR,      0x000030D  },  /* 24D   # GPI .GPIODIR  # Select direction of GPIO port (0,2,3,6,9 output) */
	{ GPIOSEL,      0/*0x00000173*/},  /*   # SYS.GPIOSEL  # GPIO port multiplexing control */
	{ GPIOPC,       0x03C300C0 },  /*   # GPI .GPIOPC  # GPIO2,3 PD cut */
	{ WKREQ,        0x00000000 },  /*   # SYS.WKREQ  # Wake-up request event is VSYNC alignment */

	{ GPIOIBE,      0x000003FF },
	{ GPIOIS,       0x00000000 },
	{ GPIOIC,       0x000003FF },
	{ GPIOIE,       0x00000000 },

	{ GPIODATA,     0x00040004 },  /*   # GPI .GPIODATA  # eDRAM VD supply */
	{ 1,            1          }, /* msleep 1 */
	{ GPIODATA,     0x02040004 },  /*   # GPI .GPIODATA  # eDRAM VD supply */
	{ DRAMPWR,      0x00000001 }, /* eDRAM power */
};

static struct mddi_table mddi_toshiba_panel_init_table[] = {
	{ SRST,         0x00000003 }, /* FIFO/LCDC not reset */
	{ PORT_ENB,     0x00000001 }, /* Enable sync. Port */
	{ START,        0x00000000 }, /* To stop operation */
	/*{ START,        0x00000001 }, To start operation */
	{ PORT,         0x00000004 }, /* Polarity of VS/HS/DE. */
	{ CMN,          0x00000000 },
	{ GAMMA,        0x00000000 }, /* No Gamma correction */
	{ INTFLG,       0x00000000 }, /* VSYNC interrupt flag clear/status */
	{ INTMSK,       0x00000000 }, /* VSYNC interrupt mask is off. */
	{ MPLFBUF,      0x00000000 }, /* Select frame buffer's base address. */
	{ HDE_LEFT,     0x00000000 }, /* The value of HDE_LEFT. */
	{ VDE_TOP,      0x00000000 }, /* The value of VDE_TPO. */
	{ PXL,          0x00000001 }, /* 1. RGB666 */
				      /* 2. Data is valid from 1st frame of beginning. */
	{ HDE_START,    0x00000006 }, /* HDE_START= 14 PCLK */
	{ HDE_SIZE,     0x0000009F }, /* HDE_SIZE=320 PCLK */
	{ HSW,          0x00000004 }, /* HSW= 10 PCLK */
	{ VSW,          0x00000001 }, /* VSW=2 HCYCLE */
	{ VDE_START,    0x00000003 }, /* VDE_START=4 HCYCLE */
	{ VDE_SIZE,     0x000001DF }, /* VDE_SIZE=480 HCYCLE */
	{ WAKEUP,       0x000001e2 }, /* Wakeup position in VSYNC mode. */
	{ WSYN_DLY,     0x00000000 }, /* Wakeup position in VSIN mode. */
	{ REGENB,       0x00000001 }, /* Set 1 to enable to change the value of registers. */
	{ CLKENB,       0x000025CB }, /* Clock enable register */

	{ SSICTL,       0x00000170 }, /* SSI control register */
	{ SSITIME,      0x00000250 }, /* SSI timing control register */
	{ SSICTL,       0x00000172 }, /* SSI control register */
};


static struct mddi_table mddi_sharp_init_table[] = {
	{ VCYCLE,       0x000001eb },
	{ HCYCLE,       0x000000ae },
	{ REGENB,       0x00000001 }, /* Set 1 to enable to change the value of registers. */
	{ GPIODATA,     0x00040000 }, /* GPIO2 low */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ 1,            1          }, /* msleep 1 */
	{ GPIODATA,     0x00040004 }, /* GPIO2 high */
	{ 1,            10         }, /* msleep 10 */
	SPI_WRITE(0x5f, 0x01)
	SPI_WRITE1(0x11)
	{ 1,            200        }, /* msleep 200 */
	SPI_WRITE1(0x29)
	SPI_WRITE1(0xde)
	{ START,        0x00000001 }, /* To start operation */
};

static struct mddi_table mddi_sharp_deinit_table[] = {
	{ 1,            200        }, /* msleep 200 */
	SPI_WRITE(0x10, 0x1)
	{ 1,            100        }, /* msleep 100 */
	{ GPIODATA,     0x00040004 }, /* GPIO2 high */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ GPIODATA,     0x00040000 }, /* GPIO2 low */
	{ 1,            10         }, /* msleep 10 */
};

static struct mddi_table mddi_tpo_init_table[] = {
	{ VCYCLE,       0x000001e5 },
	{ HCYCLE,       0x000000ac },
	{ REGENB,       0x00000001 }, /* Set 1 to enable to change the value of registers. */
	{ 0,            20         }, /* udelay 20 */
	{ GPIODATA,     0x00000004 }, /* GPIO2 high */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ 0,            20         }, /* udelay 20 */

	SPI_WRITE(0x08, 0x01)
	{ 0,            500        }, /* udelay 500 */
	SPI_WRITE(0x08, 0x00)
	SPI_WRITE(0x02, 0x00)
	SPI_WRITE(0x03, 0x04)
	SPI_WRITE(0x04, 0x0e)
	SPI_WRITE(0x09, 0x02)
	SPI_WRITE(0x0b, 0x08)
	SPI_WRITE(0x0c, 0x53)
	SPI_WRITE(0x0d, 0x01)
	SPI_WRITE(0x0e, 0xe0)
	SPI_WRITE(0x0f, 0x01)
	SPI_WRITE(0x10, 0x58)
	SPI_WRITE(0x20, 0x1e)
	SPI_WRITE(0x21, 0x0a)
	SPI_WRITE(0x22, 0x0a)
	SPI_WRITE(0x23, 0x1e)
	SPI_WRITE(0x25, 0x32)
	SPI_WRITE(0x26, 0x00)
	SPI_WRITE(0x27, 0xac)
	SPI_WRITE(0x29, 0x06)
	SPI_WRITE(0x2a, 0xa4)
	SPI_WRITE(0x2b, 0x45)
	SPI_WRITE(0x2c, 0x45)
	SPI_WRITE(0x2d, 0x15)
	SPI_WRITE(0x2e, 0x5a)
	SPI_WRITE(0x2f, 0xff)
	SPI_WRITE(0x30, 0x6b)
	SPI_WRITE(0x31, 0x0d)
	SPI_WRITE(0x32, 0x48)
	SPI_WRITE(0x33, 0x82)
	SPI_WRITE(0x34, 0xbd)
	SPI_WRITE(0x35, 0xe7)
	SPI_WRITE(0x36, 0x18)
	SPI_WRITE(0x37, 0x94)
	SPI_WRITE(0x38, 0x01)
	SPI_WRITE(0x39, 0x5d)
	SPI_WRITE(0x3a, 0xae)
	SPI_WRITE(0x3b, 0xff)
	SPI_WRITE(0x07, 0x09)
	{ 0,            10         }, /* udelay 10 */
	{ START,        0x00000001 }, /* To start operation */
};

static struct mddi_table mddi_tpo_deinit_table[] = {
	SPI_WRITE(0x07, 0x19)
	{ START,        0x00000000 }, /* To stop operation */
	{ GPIODATA,     0x00040004 }, /* GPIO2 high */
	{ GPIODIR,      0x00000004 }, /* GPIO2 out */
	{ GPIODATA,     0x00040000 }, /* GPIO2 low */
	{ 0,            5        }, /* usleep 5 */
};


#define GPIOSEL_VWAKEINT (1U << 0)
#define INTMASK_VWAKEOUT (1U << 0)

static void sapphire_process_mddi_table(
				     struct msm_mddi_client_data *client_data,
				     const struct mddi_table *table,
				     size_t count)
{
	int i;
	for (i = 0; i < count; i++) {
		uint32_t reg = table[i].reg;
		uint32_t value = table[i].value;

		if (reg == 0)
			udelay(value);
		else if (reg == 1)
			msleep(value);
		else
			client_data->remote_write(client_data, value, reg);
	}
}

static struct vreg *vreg_lcm_2v85;

static void sapphire_mddi_power_client(struct msm_mddi_client_data *client_data,
				    int on)
{
	unsigned id, on_off;
#if DEBUG_SAPPHIRE_PANEL
	printk(KERN_INFO "sapphire_mddi_client_power:%d\r\n", on);
#endif
	if (on) {
		on_off = 0;
		id = PM_VREG_PDOWN_MDDI_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);

		gpio_set_value(SAPPHIRE_MDDI_1V5_EN, 1);
		mdelay(5); /* delay time >5ms and <10ms */

		if  (is_12pin_camera())
			gpio_set_value(V_VDDE2E_VDD2_GPIO_5M, 1);
		else
			gpio_set_value(V_VDDE2E_VDD2_GPIO, 1);

		gpio_set_value(SAPPHIRE_GPIO_MDDI_32K_EN, 1);
		msleep(3);
		id = PM_VREG_PDOWN_AUX_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);
		vreg_enable(vreg_lcm_2v85);
		msleep(3);
	} else {
		gpio_set_value(SAPPHIRE_GPIO_MDDI_32K_EN, 0);
		gpio_set_value(MDDI_RST_N, 0);
		msleep(10);
		vreg_disable(vreg_lcm_2v85);
		on_off = 1;
		id = PM_VREG_PDOWN_AUX_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);
		msleep(5);
		if (is_12pin_camera())
			gpio_set_value(V_VDDE2E_VDD2_GPIO_5M, 0);
		else
			gpio_set_value(V_VDDE2E_VDD2_GPIO, 0);

		msleep(200);
		gpio_set_value(SAPPHIRE_MDDI_1V5_EN, 0);
		id = PM_VREG_PDOWN_MDDI_ID;
		msm_proc_comm(PCOM_VREG_PULLDOWN, &on_off, &id);
	}
}

static int sapphire_mddi_toshiba_client_init(
			struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	int panel_id;

	/* Set the MDDI_RST_N accroding to MDDI client repectively(
	 * been set in sapphire_mddi_power_client() originally)
	 */
	gpio_set_value(MDDI_RST_N, 1);
	msleep(10);

	client_data->auto_hibernate(client_data, 0);
	sapphire_process_mddi_table(client_data, mddi_toshiba_init_table,
				 ARRAY_SIZE(mddi_toshiba_init_table));
	client_data->auto_hibernate(client_data, 1);
	g_panel_id = panel_id =
		(client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	if (panel_id > 1) {
#if DEBUG_SAPPHIRE_PANEL
		printk(KERN_ERR "unknown panel id at mddi_enable\n");
#endif
		return -1;
	}
	return 0;
}

static int sapphire_mddi_toshiba_client_uninit(
			struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	gpio_set_value(MDDI_RST_N, 0);
	msleep(10);

	return 0;
}

static int sapphire_mddi_panel_unblank(
			struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	int panel_id, ret = 0;

	sapphire_set_backlight_level(0);
	client_data->auto_hibernate(client_data, 0);
	sapphire_process_mddi_table(client_data, mddi_toshiba_panel_init_table,
		ARRAY_SIZE(mddi_toshiba_panel_init_table));
	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	switch (panel_id) {
	case 0:
#if DEBUG_SAPPHIRE_PANEL
		printk(KERN_DEBUG "init sharp panel\n");
#endif
		sapphire_process_mddi_table(client_data,
					 mddi_sharp_init_table,
					 ARRAY_SIZE(mddi_sharp_init_table));
		break;
	case 1:
#if DEBUG_SAPPHIRE_PANEL
		printk(KERN_DEBUG "init tpo panel\n");
#endif
		sapphire_process_mddi_table(client_data,
					 mddi_tpo_init_table,
					 ARRAY_SIZE(mddi_tpo_init_table));
		break;
	default:

		printk(KERN_DEBUG "unknown panel_id: %d\n", panel_id);
		ret = -1;
	};
	mutex_lock(&sapphire_backlight_lock);
	sapphire_set_backlight_level(sapphire_backlight_brightness);
	sapphire_backlight_off = 0;
	mutex_unlock(&sapphire_backlight_lock);
	client_data->auto_hibernate(client_data, 1);
	/* reenable vsync */
	client_data->remote_write(client_data, GPIOSEL_VWAKEINT,
				  GPIOSEL);
	client_data->remote_write(client_data, INTMASK_VWAKEOUT,
				  INTMASK);
	return ret;

}

static int sapphire_mddi_panel_blank(
			struct msm_mddi_bridge_platform_data *bridge_data,
			struct msm_mddi_client_data *client_data)
{
	int panel_id, ret = 0;

	panel_id = (client_data->remote_read(client_data, GPIODATA) >> 4) & 3;
	client_data->auto_hibernate(client_data, 0);
	switch (panel_id) {
	case 0:
		printk(KERN_DEBUG "deinit sharp panel\n");
		sapphire_process_mddi_table(client_data,
					 mddi_sharp_deinit_table,
					 ARRAY_SIZE(mddi_sharp_deinit_table));
		break;
	case 1:
		printk(KERN_DEBUG "deinit tpo panel\n");
		sapphire_process_mddi_table(client_data,
					 mddi_tpo_deinit_table,
					 ARRAY_SIZE(mddi_tpo_deinit_table));
		break;
	default:
		printk(KERN_DEBUG "unknown panel_id: %d\n", panel_id);
		ret = -1;
	};
	client_data->auto_hibernate(client_data, 1);
	mutex_lock(&sapphire_backlight_lock);
	sapphire_set_backlight_level(0);
	sapphire_backlight_off = 1;
	mutex_unlock(&sapphire_backlight_lock);
	client_data->remote_write(client_data, 0, SYSCLKENA);
	client_data->remote_write(client_data, 1, DPSUS);

	return ret;
}


/* Initial sequence of sharp panel with Novatek NT35399 MDDI client */
static const struct mddi_table sharp2_init_table[] = {
	{ 0x02A0, 0x00 },
	{ 0x02A1, 0x00 },
	{ 0x02A2, 0x3F },
	{ 0x02A3, 0x01 },
	{ 0x02B0, 0x00 },
	{ 0x02B1, 0x00 },
	{ 0x02B2, 0xDF },
	{ 0x02B3, 0x01 },
	{ 0x02D0, 0x00 },
	{ 0x02D1, 0x00 },
	{ 0x02D2, 0x00 },
	{ 0x02D3, 0x00 },
	{ 0x0350, 0x80 },	/* Set frame tearing effect(FTE) position */
	{ 0x0351, 0x00 },
	{ 0x0360, 0x30 },
	{ 0x0361, 0xC1 },
	{ 0x0362, 0x00 },
	{ 0x0370, 0x00 },
	{ 0x0371, 0xEF },
	{ 0x0372, 0x01 },

	{ 0x0B00, 0x10 },

	{ 0x0B10, 0x00 },
	{ 0x0B20, 0x22 },
	{ 0x0B30, 0x46 },
	{ 0x0B40, 0x07 },
	{ 0x0B41, 0x1C },
	{ 0x0B50, 0x0F },
	{ 0x0B51, 0x7A },
	{ 0x0B60, 0x16 },
	{ 0x0B70, 0x0D },
	{ 0x0B80, 0x04 },
	{ 0x0B90, 0x07 },
	{ 0x0BA0, 0x04 },
	{ 0x0BA1, 0x86 },
	{ 0x0BB0, 0xFF },
	{ 0x0BB1, 0x01 },
	{ 0x0BB2, 0xF7 },
	{ 0x0BB3, 0x01 },
	{ 0x0BC0, 0x00 },
	{ 0x0BC1, 0x00 },
	{ 0x0BC2, 0x00 },
	{ 0x0BC3, 0x00 },
	{ 0x0BE0, 0x01 },
	{ 0x0BE1, 0x3F },

	{ 0x0BF0, 0x03 },

	{ 0x0C10, 0x02 },

	{ 0x0C30, 0x22 },
	{ 0x0C31, 0x20 },
	{ 0x0C40, 0x48 },
	{ 0x0C41, 0x06 },

	{ 0xE00, 0x0028},
	{ 0xE01, 0x002F},
	{ 0xE02, 0x0032},
	{ 0xE03, 0x000A},
	{ 0xE04, 0x0023},
	{ 0xE05, 0x0024},
	{ 0xE06, 0x0022},
	{ 0xE07, 0x0012},
	{ 0xE08, 0x000D},
	{ 0xE09, 0x0035},
	{ 0xE0A, 0x000E},
	{ 0xE0B, 0x001A},
	{ 0xE0C, 0x003C},
	{ 0xE0D, 0x003A},
	{ 0xE0E, 0x0050},
	{ 0xE0F, 0x0069},
	{ 0xE10, 0x0006},
	{ 0xE11, 0x001F},
	{ 0xE12, 0x0035},
	{ 0xE13, 0x0020},
	{ 0xE14, 0x0043},
	{ 0xE15, 0x0030},
	{ 0xE16, 0x003C},
	{ 0xE17, 0x0010},
	{ 0xE18, 0x0009},
	{ 0xE19, 0x0051},
	{ 0xE1A, 0x001D},
	{ 0xE1B, 0x003C},
	{ 0xE1C, 0x0053},
	{ 0xE1D, 0x0041},
	{ 0xE1E, 0x0045},
	{ 0xE1F, 0x004B},
	{ 0xE20, 0x000A},
	{ 0xE21, 0x0014},
	{ 0xE22, 0x001C},
	{ 0xE23, 0x0013},
	{ 0xE24, 0x002E},
	{ 0xE25, 0x0029},
	{ 0xE26, 0x001B},
	{ 0xE27, 0x0014},
	{ 0xE28, 0x000E},
	{ 0xE29, 0x0032},
	{ 0xE2A, 0x000D},
	{ 0xE2B, 0x001B},
	{ 0xE2C, 0x0033},
	{ 0xE2D, 0x0033},
	{ 0xE2E, 0x005B},
	{ 0xE2F, 0x0069},
	{ 0xE30, 0x0006},
	{ 0xE31, 0x0014},
	{ 0xE32, 0x003D},
	{ 0xE33, 0x0029},
	{ 0xE34, 0x0042},
	{ 0xE35, 0x0032},
	{ 0xE36, 0x003F},
	{ 0xE37, 0x000E},
	{ 0xE38, 0x0008},
	{ 0xE39, 0x0059},
	{ 0xE3A, 0x0015},
	{ 0xE3B, 0x002E},
	{ 0xE3C, 0x0049},
	{ 0xE3D, 0x0058},
	{ 0xE3E, 0x0061},
	{ 0xE3F, 0x006B},
	{ 0xE40, 0x000A},
	{ 0xE41, 0x001A},
	{ 0xE42, 0x0022},
	{ 0xE43, 0x0014},
	{ 0xE44, 0x002F},
	{ 0xE45, 0x002A},
	{ 0xE46, 0x001A},
	{ 0xE47, 0x0014},
	{ 0xE48, 0x000E},
	{ 0xE49, 0x002F},
	{ 0xE4A, 0x000F},
	{ 0xE4B, 0x001B},
	{ 0xE4C, 0x0030},
	{ 0xE4D, 0x002C},
	{ 0xE4E, 0x0051},
	{ 0xE4F, 0x0069},
	{ 0xE50, 0x0006},
	{ 0xE51, 0x001E},
	{ 0xE52, 0x0043},
	{ 0xE53, 0x002F},
	{ 0xE54, 0x0043},
	{ 0xE55, 0x0032},
	{ 0xE56, 0x0043},
	{ 0xE57, 0x000D},
	{ 0xE58, 0x0008},
	{ 0xE59, 0x0059},
	{ 0xE5A, 0x0016},
	{ 0xE5B, 0x0030},
	{ 0xE5C, 0x004B},
	{ 0xE5D, 0x0051},
	{ 0xE5E, 0x005A},
	{ 0xE5F, 0x006B},

        { 0x0290, 0x01 },
};

#undef TPO2_ONE_GAMMA
/* Initial sequence of TPO panel with Novatek NT35399 MDDI client */

static const struct mddi_table tpo2_init_table[] = {
	/* Panel interface control */
	{ 0xB30, 0x44 },
	{ 0xB40, 0x00 },
	{ 0xB41, 0x87 },
	{ 0xB50, 0x06 },
	{ 0xB51, 0x7B },
	{ 0xB60, 0x0E },
	{ 0xB70, 0x0F },
	{ 0xB80, 0x03 },
	{ 0xB90, 0x00 },
	{ 0x350, 0x70 },        /* FTE is at line 0x70 */

	/* Entry Mode */
	{ 0x360, 0x30 },
	{ 0x361, 0xC1 },
	{ 0x362, 0x04 },

/* 0x2 for gray scale gamma correction, 0x12 for RGB gamma correction  */
#ifdef TPO2_ONE_GAMMA
	{ 0xB00, 0x02 },
#else
	{ 0xB00, 0x12 },
#endif
	/* Driver output control */
	{ 0x371, 0xEF },
	{ 0x372, 0x03 },

	/* DCDC on glass control */
	{ 0xC31, 0x10 },
	{ 0xBA0, 0x00 },
	{ 0xBA1, 0x86 },

	/* VCOMH voltage control */
	{ 0xC50, 0x3b },

	/* Special function control */
	{ 0xC10, 0x82 },

	/* Power control */
	{ 0xC40, 0x44 },
	{ 0xC41, 0x02 },

	/* Source output control */
	{ 0xBE0, 0x01 },
	{ 0xBE1, 0x00 },

	/* Windows address setting */
	{ 0x2A0, 0x00 },
	{ 0x2A1, 0x00 },
	{ 0x2A2, 0x3F },
	{ 0x2A3, 0x01 },
	{ 0x2B0, 0x00 },
	{ 0x2B1, 0x00 },
	{ 0x2B2, 0xDF },
	{ 0x2B3, 0x01 },

	/* RAM address setting */
	{ 0x2D0, 0x00 },
	{ 0x2D1, 0x00 },
	{ 0x2D2, 0x00 },
	{ 0x2D3, 0x00 },

	{ 0xF20, 0x55 },
	{ 0xF21, 0xAA },
	{ 0xF22, 0x66 },
	{ 0xF57, 0x45 },

/*
 * The NT35399 provides gray or RGB gamma correction table,
 * which determinated by register-0xb00, and following table
 */
#ifdef TPO2_ONE_GAMMA
	/* Positive Gamma setting */
	{ 0xE00, 0x04 },
	{ 0xE01, 0x12 },
	{ 0xE02, 0x18 },
	{ 0xE03, 0x10 },
	{ 0xE04, 0x29 },
	{ 0xE05, 0x26 },
	{ 0xE06, 0x1f },
	{ 0xE07, 0x11 },
	{ 0xE08, 0x0c },
	{ 0xE09, 0x3a },
	{ 0xE0A, 0x0d },
	{ 0xE0B, 0x28 },
	{ 0xE0C, 0x40 },
	{ 0xE0D, 0x4e },
	{ 0xE0E, 0x6f },
	{ 0xE0F, 0x5E },

	/* Negative Gamma setting */
	{ 0xE10, 0x0B },
	{ 0xE11, 0x00 },
	{ 0xE12, 0x00 },
	{ 0xE13, 0x1F },
	{ 0xE14, 0x4b },
	{ 0xE15, 0x33 },
	{ 0xE16, 0x13 },
	{ 0xE17, 0x12 },
	{ 0xE18, 0x0d },
	{ 0xE19, 0x2f },
	{ 0xE1A, 0x16 },
	{ 0xE1B, 0x2e },
	{ 0xE1C, 0x49 },
	{ 0xE1D, 0x41 },
	{ 0xE1E, 0x46 },
	{ 0xE1F, 0x55 },
#else
	/* Red Positive Gamma  */
	{ 0xE00, 0x0f },
	{ 0xE01, 0x19 },
	{ 0xE02, 0x22 },
	{ 0xE03, 0x0b },
	{ 0xE04, 0x23 },
	{ 0xE05, 0x23 },
	{ 0xE06, 0x14 },
	{ 0xE07, 0x13 },
	{ 0xE08, 0x0f },
	{ 0xE09, 0x2a },
	{ 0xE0A, 0x0d },
	{ 0xE0B, 0x26 },
	{ 0xE0C, 0x43 },
	{ 0xE0D, 0x20 },
	{ 0xE0E, 0x2a },
	{ 0xE0F, 0x5c },

	/* Red Negative Gamma   */
	{ 0xE10, 0x0d },
	{ 0xE11, 0x45 },
	{ 0xE12, 0x4c },
	{ 0xE13, 0x1c },
	{ 0xE14, 0x4d },
	{ 0xE15, 0x33 },
	{ 0xE16, 0x23 },
	{ 0xE17, 0x0f },
	{ 0xE18, 0x0b },
	{ 0xE19, 0x3a },
	{ 0xE1A, 0x19 },
	{ 0xE1B, 0x32 },
	{ 0xE1C, 0x4e },
	{ 0xE1D, 0x37 },
	{ 0xE1E, 0x38 },
	{ 0xE1F, 0x3b },

	/* Green Positive Gamma */
	{ 0xE20, 0x00 },
	{ 0xE21, 0x09 },
	{ 0xE22, 0x10 },
	{ 0xE23, 0x0f },
	{ 0xE24, 0x29 },
	{ 0xE25, 0x23 },
	{ 0xE26, 0x0b },
	{ 0xE27, 0x14 },
	{ 0xE28, 0x12 },
	{ 0xE29, 0x25 },
	{ 0xE2A, 0x12 },
	{ 0xE2B, 0x2f },
	{ 0xE2C, 0x43 },
	{ 0xE2D, 0x2d },
	{ 0xE2E, 0x52 },
	{ 0xE2F, 0x61 },

	/* Green Negative Gamma */
	{ 0xE30, 0x08 },
	{ 0xE31, 0x1d },
	{ 0xE32, 0x3f },
	{ 0xE33, 0x1c },
	{ 0xE34, 0x44 },
	{ 0xE35, 0x2e },
	{ 0xE36, 0x28 },
	{ 0xE37, 0x0c },
	{ 0xE38, 0x0a },
	{ 0xE39, 0x42 },
	{ 0xE3A, 0x17 },
	{ 0xE3B, 0x30 },
	{ 0xE3C, 0x4b },
	{ 0xE3D, 0x3f },
	{ 0xE3E, 0x43 },
	{ 0xE3F, 0x45 },

	/* Blue Positive Gamma */
	{ 0xE40, 0x32 },
	{ 0xE41, 0x32 },
	{ 0xE42, 0x31 },
	{ 0xE43, 0x06 },
	{ 0xE44, 0x08 },
	{ 0xE45, 0x0d },
	{ 0xE46, 0x04 },
	{ 0xE47, 0x14 },
	{ 0xE48, 0x0f },
	{ 0xE49, 0x1d },
	{ 0xE4A, 0x1a },
	{ 0xE4B, 0x39 },
	{ 0xE4C, 0x4c },
	{ 0xE4D, 0x1e },
	{ 0xE4E, 0x43 },
	{ 0xE4F, 0x61 },

	/* Blue Negative Gamma */
	{ 0xE50, 0x08 },
	{ 0xE51, 0x2c },
	{ 0xE52, 0x4e },
	{ 0xE53, 0x13 },
	{ 0xE54, 0x3a },
	{ 0xE55, 0x26 },
	{ 0xE56, 0x30 },
	{ 0xE57, 0x0f },
	{ 0xE58, 0x0a },
	{ 0xE59, 0x49 },
	{ 0xE5A, 0x34 },
	{ 0xE5B, 0x4a },
	{ 0xE5C, 0x53 },
	{ 0xE5D, 0x28 },
	{ 0xE5E, 0x26 },
	{ 0xE5F, 0x27 },

#endif
	/* Sleep in mode 		*/
	{ 0x110, 0x00 },
	{ 0x1,   0x23 },
	/* Display on mode 		*/
	{ 0x290, 0x00 },
	{ 0x1,   0x27 },
	/* Driver output control	*/
	{ 0x372, 0x01 },
	{ 0x1,   0x40 },
	/* Display on mode		*/
	{ 0x290, 0x01 },
};

static const struct mddi_table tpo2_display_on[] = {
	{ 0x290, 0x01 },
};

static const struct mddi_table tpo2_display_off[] = {
	{ 0x110, 0x01 },
	{ 0x290, 0x00 },
	{ 0x1,   100 },
};

static const struct mddi_table tpo2_power_off[] = {
	{ 0x0110, 0x01 },
};

static int nt35399_detect_panel(struct msm_mddi_client_data *client_data)
{
	int id = -1, i ;

	/* If the MDDI client is failed to report the panel ID,
	 * perform retrial 5 times.
	 */
	for( i=0; i < 5; i++ ) {
		client_data->remote_write(client_data, 0, 0x110);
		msleep(5);
		id = client_data->remote_read(client_data, userid) ;
		if( id == 0 || id == 1 ) {
			if(i==0) {
				printk(KERN_ERR "%s: got valid panel ID=%d, "
						"without retry\n",
						__FUNCTION__, id);
			}
			else {
				printk(KERN_ERR "%s: got valid panel ID=%d, "
						"after %d retry\n",
						__FUNCTION__, id, i+1);
			}
			break ;
		}
		printk(KERN_ERR "%s: got invalid panel ID:%d, trial #%d\n",
				__FUNCTION__, id, i+1);

		gpio_set_value(MDDI_RST_N, 0);
		msleep(5);

		gpio_set_value(MDDI_RST_N, 1);
		msleep(10);
		gpio_set_value(MDDI_RST_N, 0);
		udelay(100);
		gpio_set_value(MDDI_RST_N, 1);
		mdelay(10);
	}
	printk(KERN_INFO "%s: final panel id=%d\n", __FUNCTION__, id);

	switch(id) {
	case 0:
		return SAPPHIRE_PANEL_TOPPOLY;
	case 1:
		return SAPPHIRE_PANEL_SHARP;
	default :
		printk(KERN_ERR "%s(): Invalid panel ID: %d, "
				"treat as sharp panel.", __FUNCTION__, id);
		return SAPPHIRE_PANEL_SHARP;
	}
}

static int nt35399_client_init(
		struct msm_mddi_bridge_platform_data *bridge_data,
		struct msm_mddi_client_data *client_data)
{
	int panel_id;

	if (g_panel_inited == 0) {
		g_panel_id = panel_id = nt35399_detect_panel(client_data);
		g_panel_inited = 1 ;
	} else {
		gpio_set_value(MDDI_RST_N, 1);
		msleep(10);
		gpio_set_value(MDDI_RST_N, 0);
		udelay(100);
		gpio_set_value(MDDI_RST_N, 1);
		mdelay(10);

		g_panel_id = panel_id = nt35399_detect_panel(client_data);
		if (panel_id == -1) {
			printk("Invalid panel id\n");
			return -1;
		}

		client_data->auto_hibernate(client_data, 0);
		if (panel_id == SAPPHIRE_PANEL_TOPPOLY) {
			sapphire_process_mddi_table(client_data, tpo2_init_table,
						    ARRAY_SIZE(tpo2_init_table));
		} else if(panel_id == SAPPHIRE_PANEL_SHARP) {
			sapphire_process_mddi_table(client_data, sharp2_init_table,
						    ARRAY_SIZE(sharp2_init_table));
		}

		client_data->auto_hibernate(client_data, 1);
	}

	return 0;
}

static int nt35399_client_uninit(
		struct msm_mddi_bridge_platform_data *bridge_data,
		struct msm_mddi_client_data *cdata)
{
	return 0;
}

static int nt35399_panel_unblank(
		struct msm_mddi_bridge_platform_data *bridge_data,
		struct msm_mddi_client_data *client_data)
{
	int ret = 0;

	mdelay(20);
	sapphire_set_backlight_level(0);
	client_data->auto_hibernate(client_data, 0);

	mutex_lock(&sapphire_backlight_lock);
	sapphire_set_backlight_level(sapphire_backlight_brightness);
	sapphire_backlight_off = 0;
	mutex_unlock(&sapphire_backlight_lock);

	client_data->auto_hibernate(client_data, 1);

	return ret;
}

static int nt35399_panel_blank(
		struct msm_mddi_bridge_platform_data *bridge_data,
		struct msm_mddi_client_data *client_data)
{
	int ret = 0;

	client_data->auto_hibernate(client_data, 0);
	sapphire_process_mddi_table(client_data, tpo2_display_off,
			ARRAY_SIZE(tpo2_display_off));
	client_data->auto_hibernate(client_data, 1);

	mutex_lock(&sapphire_backlight_lock);
	sapphire_set_backlight_level(0);
	sapphire_backlight_off = 1;
	mutex_unlock(&sapphire_backlight_lock);

	return ret;
}

static void sapphire_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	mutex_lock(&sapphire_backlight_lock);
	sapphire_backlight_brightness = value;
	if (!sapphire_backlight_off)
		sapphire_set_backlight_level(sapphire_backlight_brightness);
	mutex_unlock(&sapphire_backlight_lock);
}

static struct led_classdev sapphire_backlight_led = {
	.name			= "lcd-backlight",
	.brightness = SAPPHIRE_DEFAULT_BACKLIGHT_BRIGHTNESS,
	.brightness_set = sapphire_brightness_set,
};

static int sapphire_backlight_probe(struct platform_device *pdev)
{
	led_classdev_register(&pdev->dev, &sapphire_backlight_led);
	return 0;
}

static int sapphire_backlight_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&sapphire_backlight_led);
	return 0;
}

static struct platform_driver sapphire_backlight_driver = {
	.probe		= sapphire_backlight_probe,
	.remove		= sapphire_backlight_remove,
	.driver		= {
		.name		= "sapphire-backlight",
		.owner		= THIS_MODULE,
	},
};

static struct resource resources_msm_fb[] = {
	{
		.start = SMI64_MSM_FB_BASE,
		.end = SMI64_MSM_FB_BASE + SMI64_MSM_FB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct msm_mddi_bridge_platform_data toshiba_client_data = {
	.init = sapphire_mddi_toshiba_client_init,
	.uninit = sapphire_mddi_toshiba_client_uninit,
	.blank = sapphire_mddi_panel_blank,
	.unblank = sapphire_mddi_panel_unblank,
	.fb_data = {
		.xres = 320,
		.yres = 480,
		.width = 45,
		.height = 67,
		.output_format = 0,
	},
};

#define NT35399_MFR_NAME	0x0bda
#define NT35399_PRODUCT_CODE 	0x8a47

static void nt35399_fixup(uint16_t * mfr_name, uint16_t * product_code)
{
	printk(KERN_DEBUG "%s: enter.\n", __func__);
	*mfr_name = NT35399_MFR_NAME ;
	*product_code= NT35399_PRODUCT_CODE ;
}

static struct msm_mddi_bridge_platform_data nt35399_client_data = {

	.init = nt35399_client_init,
	.uninit = nt35399_client_uninit,
	.blank = nt35399_panel_blank,
	.unblank = nt35399_panel_unblank,
	.fb_data = {
		.xres = 320,
		.yres = 480,
		.output_format = 0,
	},
};

static struct msm_mddi_platform_data mddi_pdata = {
	.clk_rate = 122880000,
	.power_client = sapphire_mddi_power_client,
	.fixup = nt35399_fixup,
	.vsync_irq = MSM_GPIO_TO_INT(VSYNC_GPIO),
	.fb_resource = resources_msm_fb,
	.num_clients = 2,
	.client_platform_data = {
		{
			.product_id = (0xd263 << 16 | 0),
			.name = "mddi_c_d263_0000",
			.id = 0,
			.client_data = &toshiba_client_data,
			.clk_rate = 0,
		},
		{
			.product_id =
				(NT35399_MFR_NAME << 16 | NT35399_PRODUCT_CODE),
			.name = "mddi_c_simple" ,
			.id = 0,
			.client_data = &nt35399_client_data,
			.clk_rate = 0,
		},
	},
};

static struct platform_device sapphire_backlight = {
	.name = "sapphire-backlight",
};

int __init sapphire_init_panel(void)
{
	int rc = -1;
	uint32_t config = PCOM_GPIO_CFG(27, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA); /* GPIO27 */

	if (!machine_is_sapphire())
		return 0;

	/* checking board as soon as possible */
	printk("sapphire_init_panel:machine_is_sapphire=%d, machine_arch_type=%d, MACH_TYPE_SAPPHIRE=%d\r\n", machine_is_sapphire(), machine_arch_type, MACH_TYPE_SAPPHIRE);
	if (!machine_is_sapphire())
		return 0;

	vreg_lcm_2v85 = vreg_get(0, "gp4");
	if (IS_ERR(vreg_lcm_2v85))
		return PTR_ERR(vreg_lcm_2v85);

		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, 0);

	/* setup FB by SMI size */
	if (sapphire_get_smi_size() == 32) {
		resources_msm_fb[0].start = SMI32_MSM_FB_BASE;
		resources_msm_fb[0].end = SMI32_MSM_FB_BASE + SMI32_MSM_FB_SIZE - 1;
	}

	rc = gpio_request(VSYNC_GPIO, "vsync");
	if (rc)
		return rc;
	rc = gpio_direction_input(VSYNC_GPIO);
	if (rc)
		return rc;
	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;
	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	rc = platform_device_register(&msm_device_mddi0);
	if (rc)
		return rc;
	platform_device_register(&sapphire_backlight);
	return platform_driver_register(&sapphire_backlight_driver);
}

device_initcall(sapphire_init_panel);

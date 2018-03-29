#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>

#if 0
#include <platform/upmu_common.h>
#include <platform/mt_pmic.h>
#endif				/* 0 */

#include <string.h>

#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else

#include <mt-plat/mt_gpio.h>
#include <mach/gpio_const.h>

#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#endif				/* CONFIG_MTK_LEGACY */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#endif

#ifdef CONFIG_MTK_LEGACY
#include <cust_i2c.h>
#endif				/* CONFIG_MTK_LEGACY */

#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL, fmt)
#else
#define LCD_DEBUG(fmt)  pr_err(fmt)
#endif

static const unsigned int BL_MIN_LEVEL = 20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)        (lcm_util.set_reset_pin((v)))
#define MDELAY(n)               (lcm_util.mdelay(n))
#define UDELAY(n)               (lcm_util.udelay(n))


/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;	/* haobing modified 2013.07.11 */
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
/* NT36850_QHD */
#define LCM_DSI_CMD_MODE		    (1)
#define FRAME_WIDTH				(1440)
#define FRAME_HEIGHT				(2560)

#ifdef CONFIG_MTK_LEGACY
#define GPIO_VCL_3V0_EN         GPIO_LCM_PWR2_EN	/* GPIO210 */
#define GPIO_DSV_ENM_EN     (GPIO178 | 0x80000000)
#define GPIO_TPD_VDD_1V8_EN         (GPIO245 | 0x80000000)
#define GPIO_LCD_VDD_1V8_EN         GPIO_LCM_PWR_EN
#define GPIO_LCD_RST_PIN     (GPIO180 | 0x80000000)
#else
#define GPIO_VCL_3V0_EN             (GPIO210|0x80000000)	/* GPIO210 */
#define GPIO_DSV_ENM_EN             (GPIO178|0x80000000)
#define GPIO_TPD_VDD_1V8_EN         (GPIO245|0x80000000)	/* (GPIO245 | 0x80000000) */
#define GPIO_LCD_VDD_1V8_EN         (GPIO209|0x80000000)	/* GPIO209 */
#define GPIO_LCD_RST_PIN                (GPIO180|0x80000000)

#define P_GPIO_VCL_3V0_EN             210	/* (GPIO210|0x80000000)     // GPIO210 */
#define P_GPIO_DSV_ENM_EN             178	/* (GPIO178|0x80000000) */
#define P_GPIO_TPD_VDD_1V8_EN         245	/* (GPIO245|0x80000000)      //(GPIO245 | 0x80000000) */
#define P_GPIO_LCD_VDD_1V8_EN        209	/* (GPIO209|0x80000000)         // GPIO209 */
#define P_GPIO_LCD_RST_PIN                180	/* (GPIO180|0x80000000) */

#endif				/* CONFIG_MTK_LEGACY */

#define REGFLAG_DELAY               (0xFC)
#define REGFLAG_UDELAY              (0xFB)

#define REGFLAG_END_OF_TABLE        (0xFD)
#define REGFLAG_RESET_LOW           (0xFE)
#define REGFLAG_RESET_HIGH          (0xFF)

#if 0
#ifndef BUILD_LK
static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;
#endif
#endif				/* 0 */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


/* --------------------------------------------------------------------------- */
/* platform init */
/* --------------------------------------------------------------------------- */

/* static unsigned int lcm_esd_test = FALSE;      ///only for ESD test */
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */


struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },		/* {05 01 00 00 14 00 02 28 00} */
	{REGFLAG_DELAY, 20 /*0x14 */ , {} },

	{0x10, 0, {} },		/* {05 01 00 00 3C 00 02 10 00} */
	{REGFLAG_DELAY, 60 /*0x3C */ , {} },
};

static struct LCM_setting_table lcm_initialization_setting[] = {
#if (LCM_DSI_CMD_MODE)
	{0x35, 1, {0x00} },	/* set tear on */
	{0x44, 2, {0x09, 0xD9} },	/* set tear scan line */
#else
	{0x3B, 3, {0x03, 0x08, 0x08} },	/* {39 01 00 00 00 00 04 3B 03 08 08}    // porch setting */
	{0xBB, 1, {0x13} },	/* {15 01 00 00 00 00 02 BB 13}          // video mode on */
#endif

	{0x11, 0, {} },		/* {05 01 00 00 78 00 01 11}             // exit_sleep_mode, wait 120ms */
	{REGFLAG_DELAY, 120 /*0x78 */ , {} },

	{0x36, 1, {0x00} },	/* {15 01 00 00 00 00 02 36 00}          // set_address_mode */

	{0x29, 0, {} },		/* {05 01 00 00 14 00 01 29}             // set_display_on   //Display On */
	{REGFLAG_DELAY, 20 /*0x14 */ , {} },
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH & 0xFF)} },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT & 0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	/* Sleep Mode On */
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

#if 0
static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif				/* 0 */

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->lcm_if = LCM_INTERFACE_DSI_DUAL;
	params->lcm_cmd_if = LCM_INTERFACE_DSI0;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	/* params->dsi.switch_mode = SYNC_PULSE_VDO_MODE; */
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	/* params->dsi.switch_mode = CMD_MODE; */
#endif
	/* params->dsi.switch_mode_enable = 0; */

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 1;	/* v-pulse-width */

	params->dsi.vertical_backporch = 7;	/* = 8; */
	params->dsi.vertical_frontporch = 8;	/* = 10; */
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 4;	/* h-pulse-width */
	params->dsi.horizontal_backporch = 120;
	params->dsi.horizontal_frontporch = 180;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	/* params->dsi.ssc_disable = 1; */
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 423;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 423;	/* 770;//450; //this value must be in MTK suggested table */
#endif
	params->dsi.ssc_disable = 1;
	params->dsi.ufoe_enable = 1;
	params->dsi.ufoe_params.lr_mode_en = 1;
	params->dsi.clk_lp_per_line_enable = 0;

	params->dsi.CLK_HS_POST = 60;
	params->dsi.LPX = 7;

	/* params->dsi.HS_PRPR = 5; */
	/* params->dsi.cont_clock = 1; */

	params->dsi.esd_check_enable = 0;	/* 1; */
	params->dsi.customization_esd_check_enable = 0;
	/* params->dsi.lcm_esd_check_table[0].cmd          = 0x53; */
	/* params->dsi.lcm_esd_check_table[0].count        = 1; */
	/* params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24; */

}


/*****************************************************************************
 * DSV power control
 *****************************************************************************/
static int ldo_dsv_vdd_io_1v8_on(void)
{

#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_LCD_VDD_1V8_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_VDD_1V8_EN, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO_TPD_VDD_1V8_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_TPD_VDD_1V8_EN, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO_LCD_VDD_1V8_EN, GPIO_OUT_ONE);
	mt_set_gpio_out(GPIO_TPD_VDD_1V8_EN, GPIO_OUT_ONE);
#else
	int ret = 0;
	/* disp_power_gpio_output(GPIO_TPD_VDD_EN, 1); */
	/* disp_power_gpio_output(GPIO_LCD_VDD_EN, 1); */

	/* gvalue = gpio_get_value(209); */
	ret = gpio_request(P_GPIO_LCD_VDD_1V8_EN, "lcd_vdd_1v8");
	if (ret)
		pr_err("[KERNEL][LCM]gpio_request (%d)fail\n", P_GPIO_LCD_VDD_1V8_EN);

	ret = gpio_direction_output(P_GPIO_LCD_VDD_1V8_EN, 0);
	if (ret)
		pr_err("[KERNEL][LCM]gpio_direction_output (%d)fail\n", P_GPIO_LCD_VDD_1V8_EN);

	gpio_set_value(P_GPIO_LCD_VDD_1V8_EN, 1);

	/* ============================================================================================ */
	ret = gpio_request(P_GPIO_TPD_VDD_1V8_EN, "tpd_vdd_1v8");
	if (ret)
		pr_err("[KERNEL][LCM]gpio_request (%d)fail\n", P_GPIO_TPD_VDD_1V8_EN);

	ret = gpio_direction_output(P_GPIO_TPD_VDD_1V8_EN, 0);
	if (ret)
		pr_err("[KERNEL][LCM]gpio_direction_output (%d)fail\n", P_GPIO_TPD_VDD_1V8_EN);

	gpio_set_value(P_GPIO_TPD_VDD_1V8_EN, 1);
#endif				/* CONFIG_MTK_LEGACY */

	return 0;
}

static int ldo_dsv_vcl_3v0_on(void)
{

#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_VCL_3V0_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_VCL_3V0_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_VCL_3V0_EN, GPIO_OUT_ONE);
#else
	/* disp_power_gpio_output(GPIO_VCL_EN, 1); */
	int ret = 0;

	ret = gpio_request(P_GPIO_VCL_3V0_EN, "vcl_3v0");
	if (ret)
		pr_err("[KERNEL][LCM]gpio_request (%d)fail\n", P_GPIO_VCL_3V0_EN);

	ret = gpio_direction_output(P_GPIO_VCL_3V0_EN, 0);
	if (ret)
		pr_err("[KERNEL][LCM]gpio_direction_output (%d)fail\n", P_GPIO_VCL_3V0_EN);

	gpio_set_value(P_GPIO_VCL_3V0_EN, 1);
#endif				/* CONFIG_MTK_LEGACY */

	return 0;
}

static int ldo_dsv_enm_en(void)
{

#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_DSV_ENM_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_DSV_ENM_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DSV_ENM_EN, GPIO_OUT_ONE);
#else
	/* disp_power_gpio_output(GPIO_DSV_EN, 1); */
	int ret = 0;

	ret = gpio_request(P_GPIO_DSV_ENM_EN, "lcd_dsv_enm");
	if (ret)
		pr_err("[KERNEL][LCM]gpio_request (%d)fail\n", P_GPIO_DSV_ENM_EN);

	ret = gpio_direction_output(P_GPIO_DSV_ENM_EN, 0);
	if (ret)
		pr_err("[KERNEL][LCM]gpio_direction_output (%d)fail\n", P_GPIO_DSV_ENM_EN);

	gpio_set_value(P_GPIO_DSV_ENM_EN, 1);
#endif				/* CONFIG_MTK_LEGACY */

	return 0;
}

static int lcm_reset(void)
{
#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_LCD_RST_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST_PIN, GPIO_OUT_ONE);
#else
	/* disp_power_gpio_output(GPIO_LCD_RST, 1); */

	int ret = 0;

	ret = gpio_request(P_GPIO_LCD_RST_PIN, "lcd_rst");
	if (ret)
		pr_err("[KERNEL][LCM]gpio_request (%d)fail\n", P_GPIO_LCD_RST_PIN);

	ret = gpio_direction_output(P_GPIO_LCD_RST_PIN, 0);
	if (ret)
		pr_err("[KERNEL][LCM]gpio_direction_output (%d)fail\n", P_GPIO_LCD_RST_PIN);

	gpio_set_value(P_GPIO_LCD_RST_PIN, 1);
#endif				/* CONFIG_MTK_LEGACY */

	return 0;
}

/*****************************************************************************
 * LCM Control API
 *****************************************************************************/
static void lcm_init(void)
{
	pr_err("[KERNEL][LCM]lcm_init\n");
	/* 1. LCD_VCL_EN_3V0 = GPIO210 */
	ldo_dsv_vcl_3v0_on();
	UDELAY(10);
	pr_err("[KERNEL][LCM]ldo_dsv_vcl_3v0_on\n");

	/* 2. LCD_VDD_IO_1V8 = GPIO209 */
	ldo_dsv_vdd_io_1v8_on();
	MDELAY(5);
	pr_err("[KERNEL][LCM]ldo_dsv_vdd_io_1v8_on\n");

	/* 3. DSV ENM enable */
	/* 4. AVDD(DDVH)/ AVEE(DDVL) */
	ldo_dsv_enm_en();
	pr_err("[KERNEL][LCM]ldo_dsv_enm_en\n");
	MDELAY(10);

	MDELAY(10);

	/* 5. RESET */
	pr_err("[KERNEL][LCM]SET_RESET_PIN\n");

	lcm_reset();
	MDELAY(15);

	/* when phone initial , config output high, enable backlight drv chip */
	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_VCL_3V0_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_VCL_3V0_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_VCL_3V0_EN, GPIO_OUT_ZERO);
#endif				/* CONFIG_MTK_LEGACY */

	push_table(lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

	/* SET_RESET_PIN(0); */
	MDELAY(10);
}

static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

#define LCM_ID_NT35595 (0x95)

#if 0
static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[2];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0];		/* we only need ID */
#ifdef BUILD_LK
	dprintf(0, "%s, LK nt35595 debug: nt35595 id = 0x%08x\n", __func__, id);
#else
	pr_debug("%s, kernel nt35595 horse debug: nt35595 id = 0x%08x\n", __func__, id);
#endif

	if (id == LCM_ID_NT35595)
		return 1;
	else
		return 0;

}



/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		pr_debug("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}

	pr_debug("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;

#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	pr_debug("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight(unsigned int level)
{
#ifdef BUILD_LK
	dprintf(0, "%s,lk nt35595 backlight: level = %d\n", __func__, level);
#else
	pr_debug("%s, kernel nt35595 backlight: level = %d\n", __func__, level);
#endif
	/* Refresh value of backlight level. */
	lcm_backlight_level_setting[0].para_list[0] = level;

	push_table(lcm_backlight_level_setting,
		   sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);

}


static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}
#endif				/* 0 */

LCM_DRIVER nt36850_wqhd_dsi_2k_cmd_lcm_drv = {
	.name = "nt36850_wqhd_dsi_2k_cmd",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,	/*tianma init fun. */
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	/* .compare_id = lcm_compare_id, */
	/* .init_power = lcm_init_power, */
	/* .resume_power = lcm_resume_power, */
	/* .suspend_power = lcm_suspend_power, */
	/* .esd_check = lcm_esd_check, */
	/* .set_backlight = lcm_setbacklight, */
	/* .ata_check = lcm_ata_check, */
	.update = lcm_update,
	/* .switch_mode = lcm_switch_mode, */
};

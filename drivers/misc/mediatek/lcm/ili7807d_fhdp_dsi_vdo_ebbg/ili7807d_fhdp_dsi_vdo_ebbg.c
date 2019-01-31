#include "lcm_drv.h"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/hqsysfs.h>
#endif
#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#define WHITE_POINT_ADDR	0xA1
#define X_coordinate		172
#define Y_coordinate		192
#define ata_id0 		0x00
#define ata_id1			0x01

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;
#ifdef MTK_PROJECT_LOTUS
extern char *white_point;
#endif

#define LOG_TAG "LCM"

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#ifdef BUILD_LK
#define GPIO_LCM_ID0	GPIO171
#define GPIO_LCM_ID1	GPIO172
#endif

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2280)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH (63504)
#define LCM_PHYSICAL_HEIGHT (134064)
#define LCM_DENSITY (480)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static unsigned int cabc_state = 0;
static unsigned int ce_state = 0;

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0xFF, 3, {0x78,0x07,0x00} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 10, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 150, {} }
};

static struct LCM_setting_table init_setting[] = {
	{0xFF, 3, {0x78,0x07,0x06} },
	{0xDF, 1, {0x20} },
	{0xFF, 3, {0x78,0x07,0x05} },
	{0x33, 1, {0x03} },
	{0xFF, 3, {0x78,0x07,0x00} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0xFF, 3, {0x78,0x07,0x07} },
	{0x44, 1, {0x07} },
	{0x46, 1, {0x01} },
	{0x12, 1, {0x22} },
	{0x32, 1, {0x78} },
	{0xFF, 3, {0x78,0x07,0x05} },
	{0x03, 1, {0x30} },//11bit
	{0x04, 1, {0x01} },//7.81k
	{0xFF, 3, {0x78,0x07,0x00} },
	{0x51, 2, {0x00,0x00} },
	{0x53, 1, {0x2C} },
	{0x55, 1, {0x00} },
	{0x35, 1, {0x00} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} },
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0x0F,0xFE} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

#ifdef CONFIG_PROJECT_LOTUS
static struct LCM_setting_table cabc_on[] = {
	{0xFF, 3, {0x78,0x07,0x05} },
	{0x05, 1, {0xB5} },
	{0x11, 1, {0xF6} },
	{0x28, 1, {0x16} },
	{0xD9, 1, {0x90} },
	{0xFF, 3, {0x78,0x07,0x00} },
	{0x55, 1, {0x01} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table cabc_off[] = {
	{0xFF, 3, {0x78,0x07,0x00} },
	{0x55, 1, {0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table ce_on[] = {
	{0xFF, 3, {0x78,0x07,0x05} },
	{0x80, 1, {0x13} },
	{0x81, 1, {0x59} },
	{0xFF, 3, {0x78,0x07,0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table ce_off[] = {
	{0xFF, 3, {0x78,0x07,0x05} },
	{0x80, 1, {0x13} },
	{0x81, 1, {0x50} },
	{0xFF, 3, {0x78,0x07,0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i, cmd;

	for (i = 0; i < count; i++) {
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
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	//params->density = LCM_DENSITY;


	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	params->dsi.switch_mode_enable = 1;

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

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 20;
	params->dsi.vertical_frontporch = 22;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 80;
	params->dsi.horizontal_frontporch = 82;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable  = 0;
	params->dsi.ssc_range = 4;

	params->dsi.PLL_CLOCK = 577;
#if 0  /*non-continous clk*/
	params->dsi.cont_clock = 0;
	params->dsi.clk_lp_per_line_enable = 1;
#else /*continuous clk*/
	params->dsi.cont_clock = 1;
#endif

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

	params->dsi.lcm_esd_check_table[1].cmd = 0x0B;
	params->dsi.lcm_esd_check_table[1].count = 1;
	params->dsi.lcm_esd_check_table[1].para_list[0] = 0x00;

	params->dsi.lcm_esd_check_table[2].cmd = 0x0D;
	params->dsi.lcm_esd_check_table[2].count = 1;
	params->dsi.lcm_esd_check_table[2].para_list[0] = 0x00;
}

static void lcm_init_power(void)
{
	LCM_LOGI(" %s enter\n", __func__);
#ifdef BUILD_LK
	int ret = 0;
	/*depend on ili9881c_hdp_dsi_vdo_ilitek_rt5081_ebbg.c*/
	ret = PMU_REG_MASK(0xB2, 40, (0x3F << 0)); // 4V + 40 * 0.05V/step=6v VBST = VSP+0.2
	if (ret < 0)
		LCM_LOGI("ili7807----mt6371----vbst cmd=%0x--i2c write error----\n", 0xB2);
	else
		LCM_LOGI("ili7807----mt6371----vbst cmd=%0x--i2c write success----\n", 0xB2);

	ret = PMU_REG_MASK(0xB3, 36, (0x3F << 0)); // 4V + 36 * 0.05V/step=5.8v VSP
	if (ret < 0)
		LCM_LOGI("ili7807----mt6371----vsp cmd=%0x--i2c write error----\n", 0xB3);
	else
		LCM_LOGI("ili7807----mt6371----vsp cmd=%0x--i2c write success----\n", 0xB3);

	ret = PMU_REG_MASK(0xB4, 36, (0x3F << 0)); // 4V + 36 * 0.05V/step=5.8v VSN
	if (ret < 0)
		LCM_LOGI("ili7807----mt6371----vsn cmd=%0x--i2c write error----\n", 0xB4);
	else
		LCM_LOGI("ili7807----mt6371----vsn cmd=%0x--i2c write success----\n", 0xB4);

	ret = PMU_REG_MASK(0xB1, (1<<3) | (1<<6), (1<<3) | (1<<6)); //enable VSP VSN----disable:(1<<3) | (1<<6) -> (0<<3) | (0<<6)
	if (ret < 0)
		LCM_LOGI("ili7807----mt6371----vsn cmd=%0x--i2c write error----\n", 0xB1);
	else
		LCM_LOGI("ili7807----mt6371----vsn cmd=%0x--i2c write success----\n", 0xB1);
#else
	display_bias_enable();
#endif
	LCM_LOGI(" %s exit\n", __func__);
}

static void lcm_suspend_power(void)
{
	LCM_LOGI(" %s enter\n", __func__);
#ifndef BUILD_LK
	display_bias_disable();
#endif
	LCM_LOGI(" %s exit\n", __func__);
}

static void lcm_resume_power(void)
{
	LCM_LOGI(" %s enter\n", __func__);
#ifndef BUILD_LK
	display_bias_enable();
#endif
	LCM_LOGI(" %s exit\n", __func__);
}

static void lcm_init(void)
{
	unsigned int size;
	LCM_LOGI(" %s enter\n", __func__);

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(15);

	size = sizeof(init_setting) /sizeof(struct LCM_setting_table);
	push_table(NULL, init_setting, size, 1);
	push_table(NULL, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI(" %s exit\n", __func__);
}

static void lcm_suspend(void)
{
	LCM_LOGI(" %s enter\n", __func__);
	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
	LCM_LOGI(" %s exit\n", __func__);
}

static void lcm_resume(void)
{
	unsigned int size;
	LCM_LOGI(" %s enter\n", __func__);

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(15);

	size = sizeof(init_setting) /sizeof(struct LCM_setting_table);
	push_table(NULL, init_setting, size, 1);

	if(ce_state == 1){
		push_table(NULL, ce_on,
			sizeof(ce_on) / sizeof(struct LCM_setting_table), 1);
	}else{
		push_table(NULL, ce_off,
			sizeof(ce_off) / sizeof(struct LCM_setting_table), 1);
	}
	if(cabc_state == 1){
		push_table(NULL, cabc_on,
			sizeof(cabc_on) / sizeof(struct LCM_setting_table), 1);
	}else{
		push_table(NULL, cabc_off,
			sizeof(cabc_off) / sizeof(struct LCM_setting_table), 1);
	}

	LCM_LOGI(" %s exit\n", __func__);
}

#ifdef MTK_PROJECT_LOTUS
static void lcm_read_white_point(void)
{
	unsigned int white_point_x = 0;
	unsigned int white_point_y = 0;
	unsigned int white_point_L = 0;
	unsigned char buffer[4];
	unsigned int array[16];

	array[0] = 0x00033700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(WHITE_POINT_ADDR, buffer, 3);
	white_point_x = buffer[0]+X_coordinate;
	white_point_y = buffer[1]+Y_coordinate;
	white_point_L = buffer[2];
	LCM_LOGI("[LCM]%s,white_point_x = 0x%x,white_point_y = 0x%x,white_point_L = 0x%x\n",
		__func__,white_point_x,white_point_y,white_point_L);
	white_point[0] = (white_point_x / 100) + '0';
	white_point[1] = ((white_point_x / 10) % 10) + '0';
	white_point[2] = (white_point_x % 10) + '0';
	white_point[3] = (white_point_y/ 100) + '0';
	white_point[4] = ((white_point_y / 10) % 10) + '0';
	white_point[5] = (white_point_y % 10) + '0';

}
#endif

static unsigned int lcm_compare_id(void)
{
	LCM_LOGI(" %s enter\n", __func__);
#ifdef BUILD_LK

	unsigned int lcm_id0 = 0;
	unsigned int lcm_id1 = 1;

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(15);

	mt_set_gpio_mode(GPIO_LCM_ID0, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_ID0, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_LCM_ID0, GPIO_PULL_ENABLE);

	mt_set_gpio_mode(GPIO_LCM_ID1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_ID1, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_LCM_ID1, GPIO_PULL_ENABLE);

	/*pull down ID0 ID1 PIN*/
	mt_set_gpio_pull_select(GPIO_LCM_ID0, GPIO_PULL_DOWN);
	mt_set_gpio_pull_select(GPIO_LCM_ID1, GPIO_PULL_DOWN);

	/* get ID0 ID1 status*/
	lcm_id0 = mt_get_gpio_in(GPIO_LCM_ID0);
	lcm_id1 = mt_get_gpio_in(GPIO_LCM_ID1);
	LCM_LOGI("[LCM]%s,module lcm_id0 = %d,lcm_id1 = %d\n",__func__,lcm_id0,lcm_id1);

	if( (lcm_id0 == 1) && (lcm_id1 == 0) ){
		LCM_LOGI("[LCM]%s,ili7807 moudle id compare success\n",__func__);
#ifdef MTK_PROJECT_LOTUS
		lcm_read_white_point();
#endif
		return 1;
	}
	LCM_LOGI("[LCM]%s,ili7807 moudle compare fail\n",__func__);
#endif
	LCM_LOGI(" %s exit\n", __func__);
	return 0;
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	unsigned int   level_low   = 0;
	unsigned int   level_high = 0;

	LCM_LOGI(" %s,ili7807 backlight: level = %d\n", __func__, level);

	if(level > 2047){
		level = 2047;
	}else if(level < 10 && level >= 2){
		level = level -2;
		LCM_LOGI(" %s,otm1911 backlight : low brightness level = %d\n", __func__, level);
	}

	level_low  = level>>7;
	level_high = ( level<<1 )&0xFE;
	bl_level[0].para_list[0] = level_low;
	bl_level[0].para_list[1] = level_high;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

#ifdef CONFIG_PROJECT_LOTUS
static void lcm_setcabc_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI(" %s,ili7807 cabc: mode = %d\n", __func__, level);
	cabc_state = level;
	if(level == 1){
		push_table(handle, cabc_on,
			sizeof(cabc_on) / sizeof(struct LCM_setting_table), 1);
	}else{
		push_table(handle, cabc_off,
			sizeof(cabc_off) / sizeof(struct LCM_setting_table), 1);
	}
}

static void lcm_setce_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI(" %s,ili7807 ce: mode = %d\n", __func__, level);
	ce_state = level;
	if(level == 1){
		push_table(handle, ce_on,
			sizeof(ce_on) / sizeof(struct LCM_setting_table), 1);
	}else{
		push_table(handle, ce_off,
			sizeof(ce_off) / sizeof(struct LCM_setting_table), 1);
	}
}

static void lcm_set_hw_info(void)
{
	hq_regiser_hw_info(HWID_LCM,"oncell,vendor:ebbg,IC:ili7807(ili)");
}
#endif

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned char buffer_ata[4];
	unsigned int array[16];
	unsigned int id0 = 0;
	unsigned int id1 = 0;

	struct LCM_setting_table switch_table_page1[] = {
		{ 0xFF, 0x03, {0x78, 0x07, 0x01} }
	};

	push_table(NULL, switch_table_page1, sizeof(switch_table_page1) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(ata_id0, buffer_ata,1);
	id0 = buffer_ata[0];
	read_reg_v2(ata_id1, buffer_ata, 1);
	id1 = buffer_ata[0];
	LCM_LOGI("[LCM]%s,ic id0 = 0x%x,id1 = 0x%x\n",__func__,id0,id1);

	if( id0 == 0x78 && id1 == 0x07 ){
		LCM_LOGI("[LCM]%s,ili7807 ata_id compare success\n",__func__);
		return 1;
	}
	return 0;
#else
	return 0;
#endif
}

struct LCM_DRIVER ili7807d_fhdp_dsi_vdo_ebbg_lcm_drv = {
	.name = "ili7807d_fhdp_dsi_vdo_ebbg",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
#ifdef CONFIG_PROJECT_LOTUS
	.set_cabc_cmdq = lcm_setcabc_cmdq,
	.set_ce_cmdq = lcm_setce_cmdq,
	.set_hw_info = lcm_set_hw_info,
#endif
};

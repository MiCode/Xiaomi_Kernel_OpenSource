// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/atomic.h>
/* #include <mach/mt_reg_base.h> */
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#if defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_MT6797) || \
	defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#include <ddp_clkmgr.h>
#endif
#endif
#include <ddp_pwm_mux.h>
/* #include <mach/mt_gpio.h> */
#include <disp_dts_gpio.h> /* DTS GPIO */
#if defined(LED_READY)
#include <mtk_leds_drv.h>
#include <mtk_leds_sw.h>
#else
#define MT65XX_LED_MODE_NONE (0)
#define MT65XX_LED_MODE_CUST_BLS_PWM (5)
#endif
#include <ddp_reg.h>
#include <ddp_path.h>
#include <primary_display.h>
#include <disp_drv_platform.h>
#include <ddp_drv.h>
#include <ddp_pwm.h>

#define PWM_DEFAULT_DIV_VALUE 0x0

/* #define GPIO_DVT_TEST */

#if defined(CONFIG_MACH_MT6799)
#define PWM0_CLK_NAMING (DISP_CLK_PWM0)
#else
#define PWM0_CLK_NAMING (DISP_PWM)
#endif

static int pwm_dbg_en;
#define PWM_ERR(fmt, arg...) pr_notice("[PWM] %s: " fmt "\n", __func__, ##arg)
#define PWM_NOTICE(fmt, arg...) pr_info("[PWM] %s: " fmt "\n", __func__, ##arg)
#define PWM_MSG(fmt, arg...) pr_debug("[PWM] %s: " fmt "\n", __func__, ##arg)
#define PWM_DBG(fmt, arg...) \
	do { if (pwm_dbg_en) pr_debug("[PWM] %s: " fmt "\n", __func__, ##arg); \
		} while (0)

#define PWM_LOG_BUFFER_SIZE 8

static enum disp_pwm_id_t g_pwm_main_id = DISP_PWM0;
static ddp_module_notify g_ddp_notify;

#if defined(CONFIG_MACH_MT6799)
#define PWM_TOTAL_MODULE_NUM (2)

#define pwm_get_reg_base(id) ((id == DISP_PWM0) ? \
	DISPSYS_PWM0_BASE : DISPSYS_PWM1_BASE)
#define pwm_get_id_from_module(module) ((module == DISP_MODULE_PWM0) ? \
	DISP_PWM0 : DISP_PWM1)
#define index_of_pwm(id) ((id == DISP_PWM0) ? 0 : 1)

#ifndef CONFIG_FPGA_EARLY_PORTING
static atomic_t g_pwm_backlight[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t g_pwm_en[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t g_pwm_max_backlight[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(1023), ATOMIC_INIT(1023) };
static atomic_t g_pwm_is_power_on[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0) };
static atomic_t g_pwm_value_before_power_off[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0) };
static atomic_t g_pwm_is_change_state[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0) };
#endif				/* not define CONFIG_FPGA_EARLY_PORTING */
#else
#define PWM_TOTAL_MODULE_NUM (1)

#define pwm_get_reg_base(id) (DISPSYS_PWM0_BASE)
#define pwm_get_id_from_module(module) (DISP_PWM0)
#define index_of_pwm(id) (0)

#ifndef CONFIG_FPGA_EARLY_PORTING
static atomic_t g_pwm_backlight[PWM_TOTAL_MODULE_NUM] = { ATOMIC_INIT(-1) };
static atomic_t g_pwm_en[PWM_TOTAL_MODULE_NUM] = { ATOMIC_INIT(-1) };
static atomic_t g_pwm_max_backlight[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(1023) };
static atomic_t g_pwm_is_power_on[PWM_TOTAL_MODULE_NUM] = { ATOMIC_INIT(0) };
static atomic_t g_pwm_value_before_power_off[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0) };
static atomic_t g_pwm_is_change_state[PWM_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0) };
#endif				/* not define CONFIG_FPGA_EARLY_PORTING */
#endif

static int g_pwm_led_mode = MT65XX_LED_MODE_NONE;

struct PWM_LOG {
	int value;
	unsigned long tsec;
	unsigned long tusec;
};

enum PWM_LOG_TYPE {
	NOTICE_LOG = 0,
	MSG_LOG,
};

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#define PWM_USE_HIGH_ULPOSC_FQ
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
static DEFINE_SPINLOCK(g_pwm_log_lock);
static struct PWM_LOG g_pwm_log_buffer[PWM_LOG_BUFFER_SIZE + 1];
static int g_pwm_log_index;
#if defined(PWM_USE_HIGH_ULPOSC_FQ)
static bool g_pwm_first_config[PWM_TOTAL_MODULE_NUM];
#endif		/* PWM_USE_HIGH_ULPOSC_FQ */
#endif		/* CONFIG_FPGA_EARLY_PORTING */
static int g_pwm_log_num = PWM_LOG_BUFFER_SIZE;

int disp_pwm_get_cust_led(unsigned int *clocksource, unsigned int *clockdiv)
{
	struct device_node *led_node = NULL;
	int ret = 0;
	int led_mode;
	int pwm_config[5] = { 0 };

	led_node = of_find_compatible_node(NULL, NULL,
		"mediatek,lcd-backlight");
	if (!led_node) {
		ret = -1;
		PWM_ERR("Cannot find LED node from dts\n");
	} else {
		ret = of_property_read_u32_array(led_node, "pwm_config",
			pwm_config, ARRAY_SIZE(pwm_config));
		if (!ret) {
			*clocksource = pwm_config[0];
			*clockdiv = pwm_config[1];
		} else {
			PWM_ERR("led dts can not get pwm config data.\n");
		}

		if (g_pwm_led_mode == MT65XX_LED_MODE_NONE) {
			ret = of_property_read_u32(led_node, "led_mode",
				&led_mode);
			if (!ret) {
				/* Save current LED mode */
				g_pwm_led_mode = led_mode;
			} else {
				PWM_ERR("led dts can not get led mode data.\n");
			}
		}
	}

	if (ret)
		PWM_ERR("get pwm cust info fail");

	return ret;
}

static void disp_pwm_backlight_status(enum disp_pwm_id_t id,
	bool is_power_on)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int index = index_of_pwm(id);
	const unsigned long reg_base = pwm_get_reg_base(id);
	unsigned int high_width;

	if (g_pwm_led_mode == MT65XX_LED_MODE_CUST_BLS_PWM) {
		/* Read PWM value from register */
		if (DISP_REG_GET(reg_base + DISP_PWM_EN_OFF) > 0) {
			high_width =
			DISP_REG_GET(reg_base + DISP_PWM_CON_1_OFF) >> 16;
		} else
			high_width = 0;
	} else {
		/* Set dummy backlight value */
		if (is_power_on == true)
			high_width = 1023;
		else
			high_width = 0;
	}

	if (is_power_on == true && high_width > 0) {
		PWM_NOTICE("backlight is on (%d), power:(%d), pwm id: (%d)",
			high_width, is_power_on, index);
		/* Change status when backlight turns on */
		atomic_set(&g_pwm_is_power_on[index], 1);
	} else if (is_power_on == false) {
		PWM_NOTICE("backlight is off, power:(%d), pwm id: (%d)",
			is_power_on, index);
		/* Save vlaue before clock off */
		atomic_set(&g_pwm_value_before_power_off[index], high_width);
		atomic_set(&g_pwm_is_power_on[index], 0);
	}
#endif
}

static void disp_pwm_query_backlight(char *debug_output)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	char *temp_buf = debug_output;
	const size_t buf_max_len = 100;
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);
	int index = index_of_pwm(DISP_PWM0);
	unsigned int high_width;

	if (atomic_read(&g_pwm_is_power_on[index]) == 1) {
		if (g_pwm_led_mode == MT65XX_LED_MODE_CUST_BLS_PWM) {
			/* Read PWM value from register */
			if (DISP_REG_GET(reg_base + DISP_PWM_EN_OFF) > 0) {
				high_width =
				DISP_REG_GET(reg_base + DISP_PWM_CON_1_OFF)
				>> 16;
			} else
				high_width = 0;
		} else {
			/* Set dummy backlight value */
			high_width = 1023;
		}
	} else {
		/* Read vlaue before clock off */
		high_width = atomic_read(&g_pwm_value_before_power_off[index]);
	}

	if (high_width > 0) {
		/* print backlight status */
		snprintf(temp_buf, buf_max_len,
			"backlight is on (%d), ddp_pwm power:(%d)",
			high_width, atomic_read(&g_pwm_is_power_on[index]));
	} else {
		snprintf(temp_buf, buf_max_len,
			"backlight is off, ddp_pwm power:(%d)",
			atomic_read(&g_pwm_is_power_on[index]));
	}

	PWM_NOTICE("%s", temp_buf);
#endif
}

static int disp_pwm_config_init(enum DISP_MODULE_ENUM module,
	struct disp_ddp_path_config *pConfig, void *cmdq)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	unsigned int pwm_div, pwm_src;
	unsigned long reg_base =
		pwm_get_reg_base(pwm_get_id_from_module(module));
	int index = index_of_pwm(pwm_get_id_from_module(module));
	int ret;
	bool config_instantly = false;

	pwm_div = PWM_DEFAULT_DIV_VALUE;

	ret = disp_pwm_get_cust_led(&pwm_src, &pwm_div);
	if (!ret) {
		disp_pwm_set_pwmmux(pwm_src);

		/* Some backlight chip/PMIC(e.g. MT6332) */
		/* only accept slower clock */
		pwm_div = (pwm_div == 0) ? PWM_DEFAULT_DIV_VALUE : pwm_div;
		pwm_div &= 0x3FF;
#if defined(PWM_USE_HIGH_ULPOSC_FQ)
		if ((pwm_src == 0 || pwm_src == 1) && (pwm_src < 0x3FF)) {
			/* add PWM clock division, due to ULPOSC frequency */
			/* is too high in some chip */
			pwm_div += 1;
		}
		if (g_pwm_first_config[index] == false) {
			config_instantly = true;
			g_pwm_first_config[index] = true;
		}
#endif
	}

	atomic_set(&g_pwm_is_change_state[index], 1);

	if (config_instantly == true) {
		/* Set PWM clock division instantly */
		/* to avoid frequency change dramaticly */
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_DEBUG, 0x3, 0x3);
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_CON_0_OFF,
			pwm_div << 16, (0x3ff << 16));
		udelay(40);
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_DEBUG, 0x0, 0x3);

		PWM_MSG("disp_pwm_init : PWM config data instantly (%d,%d)",
			pwm_src, pwm_div);
	}

	/* We don't enable PWM until we really need */
	DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_0_OFF, pwm_div << 16,
	(0x3ff << 16));

	/* 1024 levels */
	DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_1_OFF, 1023, 0x3ff);
	/* We don't init the backlight here until AAL/Android give */
#endif
	return 0;
}


static int disp_pwm_config(enum DISP_MODULE_ENUM module,
	struct disp_ddp_path_config *pConfig, void *cmdq)
{
	int ret = 0;

	if (pConfig->dst_dirty)
		ret |= disp_pwm_config_init(module, pConfig, cmdq);

	return ret;
}

static void disp_pwm_trigger_refresh(enum disp_pwm_id_t id, int quick)
{
	if (g_ddp_notify != NULL) {
#if defined(DISP_PATH_DELAYED_TRIGGER_33ms_SUPPORT)
		if (disp_aal_is_support() == true) {
			if (quick) { /* Turn off backlight immediately */
				g_ddp_notify(DISP_MODULE_PWM0,
					DISP_PATH_EVENT_TRIGGER);
			} else {
				/* If AAL is present, AAL will dominate */
				/* the refresh rate,maybe 17ms or 33ms. */
				/* 33ms will be the upper bound of latency. */

				g_ddp_notify(DISP_MODULE_PWM0,
				DISP_PATH_EVENT_DELAYED_TRIGGER_33ms);
			}
		}
#else
		g_ddp_notify(DISP_MODULE_PWM0, DISP_PATH_EVENT_TRIGGER);
#endif
	}
}


/* Set the PWM which acts by default (e.g. ddp_bls_set_backlight) */
void disp_pwm_set_main(enum disp_pwm_id_t main)
{
	g_pwm_main_id = main;
}


enum disp_pwm_id_t disp_pwm_get_main(void)
{
	return g_pwm_main_id;
}


#ifndef CONFIG_FPGA_EARLY_PORTING
static void disp_pwm_set_drverIC_en(enum disp_pwm_id_t id, int enabled)
{
#ifdef GPIO_LCM_LED_EN
	if (id == DISP_PWM0) {
		mt_set_gpio_mode(GPIO_LCM_LED_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCM_LED_EN, GPIO_DIR_OUT);

		if (enabled)
			mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ONE);
		else
			mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ZERO);
	}
#endif
}

static void disp_pwm_set_enabled(struct cmdqRecStruct *cmdq,
	enum disp_pwm_id_t id, int enabled)
{
	unsigned long reg_base = pwm_get_reg_base(id);
	int index = index_of_pwm(id);
	int old_en;

	old_en = atomic_xchg(&g_pwm_en[index], enabled);
	if (old_en == enabled)
		return;

	if (enabled) {
#if defined(CONFIG_MACH_MT6799)
		disp_dts_gpio_select_state(DTS_GPIO_STATE_DISP_PWM_TRANSPARENT);
#endif
		/* Always use CPU to config DISP_PWM EN */
		/* to avoid race condition */
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_EN_OFF, 0x1, 0x1);
		PWM_MSG("PWN_EN (by CPU) = 0x1");

		disp_pwm_set_drverIC_en(id, enabled);
	} else {
#if defined(CONFIG_MACH_MT6799)
		disp_dts_gpio_select_state(DTS_GPIO_STATE_DISP_PWM_GPIO_LOW);
		/* Always use CPU to config DISP_PWM EN */
		/* to avoid race condition */
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_EN_OFF, 0x0, 0x1);
		DISP_REG_SET(NULL, reg_base + DISP_PWM_RST_RX, 0x1);
		DISP_REG_SET(NULL, reg_base + DISP_PWM_RST_RX, 0x0);
#else
		/* Always use CPU to config DISP_PWM EN */
		/* to avoid race condition */
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_EN_OFF, 0x0, 0x1);
#endif
		PWM_MSG("PWN_EN (by CPU) = 0x0");

		disp_pwm_set_drverIC_en(id, enabled);
	}

}

/*
 * If you want to re-map the backlight level from user space to
 * the real level of hardware output, please modify here.
 *
 * Inputs:
 *  id		- DISP_PWM0 / DISP_PWM1
 *  level_1024	- Backlight value in [0, 1023]
 * Returns:
 *  PWM duty in [0, 1023]
 */
static int disp_pwm_level_remap(enum disp_pwm_id_t id, int level_1024)
{
	return level_1024;
}

#define LOGBUFFERSIZE 384
static void disp_pwm_log(int level_1024, int log_type)
{
	int i;
	struct timeval pwm_time;
	char buffer[LOGBUFFERSIZE] = "";
	int print_log;

	do_gettimeofday(&pwm_time);

	spin_lock(&g_pwm_log_lock);

	g_pwm_log_buffer[g_pwm_log_index].value = level_1024;
	g_pwm_log_buffer[g_pwm_log_index].tsec =
		(unsigned long)pwm_time.tv_sec % 1000;
	g_pwm_log_buffer[g_pwm_log_index].tusec =
		(unsigned long)pwm_time.tv_usec / 1000;
	g_pwm_log_index += 1;
	print_log = 0;

	if (g_pwm_log_index >= g_pwm_log_num || level_1024 == 0) {
		sprintf(buffer + strlen(buffer), "(latest=%2u): ",
			g_pwm_log_index);
		for (i = 0; i < g_pwm_log_index; i += 1) {
			sprintf(buffer + strlen(buffer), "%5d(%4lu,%4lu)",
				g_pwm_log_buffer[i].value,
				g_pwm_log_buffer[i].tsec,
				g_pwm_log_buffer[i].tusec);
		}

		g_pwm_log_index = 0;
		print_log = 1;

		for (i = 0; i < PWM_LOG_BUFFER_SIZE; i += 1) {
			g_pwm_log_buffer[i].tsec = 0;
			g_pwm_log_buffer[i].tusec = 0;
			g_pwm_log_buffer[i].value = -1;
		}
	}

	spin_unlock(&g_pwm_log_lock);

	if (print_log == 1) {
		if (log_type == MSG_LOG)
			PWM_MSG("%s", buffer);
		else
			PWM_NOTICE("%s", buffer);
	}

}
#endif				/* not define CONFIG_FPGA_EARLY_PORTING */

int disp_bls_set_max_backlight(unsigned int level_1024)
{
	return disp_pwm_set_max_backlight(disp_pwm_get_main(), level_1024);
}

int disp_pwm_set_max_backlight(enum disp_pwm_id_t id, unsigned int level_1024)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int index;

	if ((DISP_PWM_ALL & id) == 0) {
		PWM_ERR("[ERROR] invalid PWM ID = 0x%x",
			id);
		return -EFAULT;
	}

	index = index_of_pwm(id);
	atomic_set(&g_pwm_max_backlight[index], level_1024);
	PWM_MSG("(id = 0x%x, level = %u)", id, level_1024);

	atomic_set(&g_pwm_is_change_state[index], 1);
	disp_pwm_set_backlight(id, atomic_read(&g_pwm_backlight[index]));
#endif
	return 0;
}

int disp_pwm_get_max_backlight(enum disp_pwm_id_t id)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int index = index_of_pwm(id);

	return atomic_read(&g_pwm_max_backlight[index]);
#else
	return 1023;
#endif
}


/* For backward compatible */
int disp_bls_set_backlight(int level_1024)
{
	return disp_pwm_set_backlight(disp_pwm_get_main(), level_1024);
}

int disp_pwm_set_backlight(enum disp_pwm_id_t id, int level_1024)
{
	int ret;

#ifdef MTK_DISP_IDLE_LP
	disp_exit_idle_ex(__func__);
#endif

	/* Always write registers by CPU */
	ret = disp_pwm_set_backlight_cmdq(id, level_1024, NULL);

	if (ret >= 0) {
		/* For backlight turn-off, we have to trigger right away*/
		disp_pwm_trigger_refresh(id, (level_1024 == 0));
	}

	return 0;
}

int disp_pwm_set_backlight_cmdq(enum disp_pwm_id_t id,
	int level_1024, void *cmdq)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* PWM is excluded from FPGA bitfile */
	unsigned long reg_base;
	int old_pwm;
	int index;
	int abs_diff;
	int max_level_1024;

	if ((DISP_PWM_ALL & id) == 0) {
		PWM_ERR("[ERROR] invalid id = 0x%x", id);
		return -EFAULT;
	}

	index = index_of_pwm(id);

	/* we have to change backlight after config init */
	/* or max backlight changed */
	old_pwm = atomic_xchg(&g_pwm_backlight[index], level_1024);
	if (old_pwm != level_1024 ||
		atomic_cmpxchg(&g_pwm_is_change_state[index], 1, 0) == 1) {
		abs_diff = level_1024 - old_pwm;
		if (abs_diff < 0)
			abs_diff = -abs_diff;

		/* To be printed in UART log */
		disp_pwm_log(level_1024, MSG_LOG);

		if ((old_pwm == 0 || level_1024 == 0 || abs_diff > 64) &&
			old_pwm != level_1024) {
			/* Print information if backlight is changed */
			PWM_NOTICE("(id = 0x%x, level_1024 = %d), old = %d",
				id, level_1024, old_pwm);
		}

		max_level_1024 = disp_pwm_get_max_backlight(id);
		if (level_1024 > max_level_1024)
			level_1024 = max_level_1024;
		else if (level_1024 < 0)
			level_1024 = 0;

		level_1024 = disp_pwm_level_remap(id, level_1024);

		reg_base = pwm_get_reg_base(id);

		if (level_1024 > 0) {
			DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_1_OFF,
				level_1024 << 16, 0x1fff << 16);

			disp_pwm_set_enabled(cmdq, id, 1);
		} else {
			/* Avoid to set 0 */
			DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_1_OFF,
				1 << 16, 0x1fff << 16);
			/* To save power */
			disp_pwm_set_enabled(cmdq, id, 0);
		}

		DISP_REG_MASK(cmdq, reg_base + DISP_PWM_COMMIT_OFF, 1, ~0);
		DISP_REG_MASK(cmdq, reg_base + DISP_PWM_COMMIT_OFF, 0, ~0);
	}

	if (g_pwm_led_mode == MT65XX_LED_MODE_CUST_BLS_PWM &&
		atomic_read(&g_pwm_is_power_on[index]) == 0 && level_1024 > 0) {
		/* print backlight once after device resumed */
		disp_pwm_backlight_status(id, true);
	}
#endif
	return 0;
}

static int ddp_pwm_power_on(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int pwm_div = 0;
	unsigned int pwm_src = 0;
	enum disp_pwm_id_t id = pwm_get_id_from_module(module);
	int ret = -1;

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761)
	/* pwm ccf api */
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
#elif defined(CONFIG_MACH_MT6763)
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
	ddp_clk_prepare_enable(TOP_MUX_DISP_PWM);
#elif defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
	ddp_clk_prepare_enable(CLK_MUX_DISP_PWM);
#else
#ifdef ENABLE_CLK_MGR
	if (module == DISP_MODULE_PWM0) {
#ifdef CONFIG_MTK_CLKMGR /* MTK Clock Manager */
#if defined(CONFIG_MACH_MT6752)
		enable_clock(MT_CG_DISP1_DISP_PWM_26M, "PWM");
		enable_clock(MT_CG_DISP1_DISP_PWM_MM, "PWM");
#elif defined(CONFIG_MACH_MT6580)
		enable_clock(MT_CG_PWM_MM_SW_CG, "PWM");
#else
		enable_clock(MT_CG_PERI_DISP_PWM, "DISP_PWM");
#endif
#else /* Common Clock Framework */
		ddp_clk_enable(PWM0_CLK_NAMING);
#endif
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_PWM1) {
#ifndef CONFIG_MTK_CLKMGR /* Common Clock Framework */
		ddp_clk_enable(DISP_CLK_PWM1);
#else
		/* MTK Clock Manager implementation */
#endif
	}
#endif
#endif	/* ENABLE_CLK_MGR */
#endif

	ret = disp_pwm_get_cust_led(&pwm_src, &pwm_div);
	if (!ret)
		disp_pwm_clksource_enable(pwm_src);

	if (g_pwm_led_mode != MT65XX_LED_MODE_CUST_BLS_PWM)
		disp_pwm_backlight_status(id, true);

	return 0;
}

static int ddp_pwm_power_off(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int pwm_div = 0;
	unsigned int pwm_src = 0;
	enum disp_pwm_id_t id = pwm_get_id_from_module(module);
	int ret = -1;

	disp_pwm_backlight_status(id, false);

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761)
	/* pwm ccf api */
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
#elif defined(CONFIG_MACH_MT6763)
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
	ddp_clk_disable_unprepare(TOP_MUX_DISP_PWM);
#elif defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
	ddp_clk_disable_unprepare(CLK_MUX_DISP_PWM);
#else
#ifdef ENABLE_CLK_MGR
	if (module == DISP_MODULE_PWM0) {
		atomic_set(&g_pwm_backlight[0], 0);
#ifdef CONFIG_MTK_CLKMGR /* MTK Clock Manager */
#if defined(CONFIG_MACH_MT6752)
		disable_clock(MT_CG_DISP1_DISP_PWM_26M, "PWM");
		disable_clock(MT_CG_DISP1_DISP_PWM_MM, "PWM");
#elif defined(CONFIG_MACH_MT6580)
		disable_clock(MT_CG_PWM_MM_SW_CG, "PWM");
#else
		disable_clock(MT_CG_PERI_DISP_PWM, "DISP_PWM");
#endif
#else /* Common Clock Framework */
		ddp_clk_disable(PWM0_CLK_NAMING);
#endif
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_PWM1) {
#ifndef CONFIG_MTK_CLKMGR /* Common Clock Framework */
		ddp_clk_disable(DISP_CLK_PWM1);
#else
		/* MTK Clock Manager implementation */
#endif
	}
#endif
#endif	/* ENABLE_CLK_MGR */
#endif

	ret = disp_pwm_get_cust_led(&pwm_src, &pwm_div);
	if (!ret)
		disp_pwm_clksource_disable(pwm_src);

	return 0;
}

static int ddp_pwm_init(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
#if !defined(CONFIG_MACH_MT6759) && !defined(CONFIG_MACH_MT6739)
	ddp_pwm_power_on(module, cmq_handle);
#endif
	return 0;
}

static int ddp_pwm_set_listener(enum DISP_MODULE_ENUM module,
	ddp_module_notify notify)
{
	g_ddp_notify = notify;
	return 0;
}


struct DDP_MODULE_DRIVER ddp_driver_pwm = {
	.init = ddp_pwm_init,
	.config = disp_pwm_config,
	.power_on = ddp_pwm_power_on,
	.power_off = ddp_pwm_power_off,
	.set_listener = ddp_pwm_set_listener,
};

/* ---------------------------------------------------------------------- */
/* disp pwm clock source query api					  */
/* ---------------------------------------------------------------------- */

bool disp_pwm_is_osc(void)
{
	bool is_osc = false;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)

	is_osc = disp_pwm_mux_is_osc();
#endif
	return is_osc;
}

/* ---------------------------------------------------------------------- */
/* Test code								  */
/* Following is only for PWM functional test, not normal code		  */
/* Will not be linked into user build.					  */
/* ---------------------------------------------------------------------- */

static void disp_pwm_test_source(const char *cmd)
{
	unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);
	int sel = (cmd[0] - '0') & 0x3;

	DISP_REG_MASK(NULL, reg_base + DISP_PWM_CON_0_OFF, (sel << 4),
		(0x3 << 4));
}


static void disp_pwm_test_grad(const char *cmd)
{
#if defined(CONFIG_MACH_MT6799)
	const unsigned long reg_grad = pwm_get_reg_base(DISP_PWM0) + 0x20;
#else
	const unsigned long reg_grad = pwm_get_reg_base(DISP_PWM0) + 0x18;
#endif

	switch (cmd[0]) {
	case 'H':
		DISP_REG_SET(NULL, reg_grad, (1 << 16) | (1 << 8) | 1);
		disp_pwm_set_backlight(DISP_PWM0, 1023);
		break;

	case 'L':
		DISP_REG_SET(NULL, reg_grad, (1 << 16) | (1 << 8) | 1);
		disp_pwm_set_backlight(DISP_PWM0, 40);
		break;

	default:
		DISP_REG_SET(NULL, reg_grad, 0);
		disp_pwm_set_backlight(DISP_PWM0, 512);
		break;
	}
}


static void disp_pwm_test_div(const char *cmd)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);

	int div = cmd[0] - '0';

	if (div > 5)
		div = 5;

	DISP_REG_MASK(NULL, reg_base + DISP_PWM_CON_0_OFF, div << 16,
		(0x3ff << 16));
	disp_pwm_set_backlight(DISP_PWM0, 256 + div);	/* to be applied */
}


static void disp_pwm_enable_debug(const char *cmd)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);

	if (cmd[0] == '1')
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_DEBUG, 3, 0x3);
	else
		DISP_REG_MASK(NULL, reg_base + DISP_PWM_DEBUG, 0, 0x3);
}

static void disp_pwm_test_pin_mux(void)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM1);
#ifdef GPIO_DVT_TEST
	/* set gpio function for dvt test */

	/* For DVT PIN MUX verification only, not normal path */
	mt_set_gpio_mode(GPIO157, GPIO_MODE_01);

	/* For DVT PIN MUX verification only, not normal path */
	mt_set_gpio_dir(GPIO157, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO4, GPIO_MODE_01);
	mt_set_gpio_dir(GPIO4, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO14, GPIO_MODE_03);
	mt_set_gpio_dir(GPIO14, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO127, GPIO_MODE_02);
	mt_set_gpio_dir(GPIO127, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO134, GPIO_MODE_02);
	mt_set_gpio_dir(GPIO134, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO153, GPIO_MODE_05);
	mt_set_gpio_dir(GPIO153, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO186, GPIO_MODE_02);
	mt_set_gpio_dir(GPIO186, GPIO_DIR_OUT);
#endif
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_CON_1_OFF, 512 << 16,
	0x1fff << 16);
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_EN_OFF, 0x1, 0x1);
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_DEBUG, 3, 0x3);
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_COMMIT_OFF, 1, ~0);
	DISP_REG_MASK(NULL, reg_base + DISP_PWM_COMMIT_OFF, 0, ~0);
}

static int pwm_simple_strtoul(char *ptr, unsigned long *res)
{
	int i;
	char buffer[20];
	int end = 0;
	int ret = 0;

	for (i = 0; i < 20; i += 1) {
		end = i;
		PWM_MSG("%c\n", ptr[i]);
		if (ptr[i] < '0' || ptr[i] > '9')
			break;
	}

	if (end > 0) {
		strncpy(buffer, ptr, end);
		buffer[end] = '\0';
		ret = kstrtoul(buffer, 0, res);

	}
	return end;
}

static int pwm_parse_triple(const char *cmd, unsigned long *offset,
	unsigned long *value, unsigned long *mask)
{
	int count = 0;
	char *next = (char *)cmd;
	int end;

	*value = 0;
	*mask = 0;
	end = pwm_simple_strtoul(next, offset);
	next += end;
	if (*offset > 0x1000UL || (*offset & 0x3UL) != 0)  {
		*offset = 0UL;
		return 0;
	}

	count++;

	if (*next == ',')
		next++;

	end = pwm_simple_strtoul(next, value);
	next += end;
	count++;

	if (*next == ',')
		next++;

	end = pwm_simple_strtoul(next, mask);
	next += end;
	count++;

	return count;
}

static void disp_pwm_dump(void)
{
	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);
	unsigned int val;
	int offset;

	PWM_NOTICE("[DUMP] Base = 0x%lx", reg_base);
	for (offset = 0; offset <= 0x30; offset += 4) {
		val = DISP_REG_GET(reg_base + offset);

		PWM_NOTICE("[DUMP] [+0x%02x] = 0x%08x", offset, val);
	}
#if defined(CONFIG_MACH_MT6799)
	val = DISP_REG_GET(reg_base + DISP_PWM_DEBUG);

	PWM_NOTICE("[DUMP] [+0x%02x] = 0x%08x", DISP_PWM_DEBUG, val);
#endif
}

void disp_pwm_test(const char *cmd, char *debug_output)
{
	unsigned long offset, value, mask;

	const unsigned long reg_base = pwm_get_reg_base(DISP_PWM0);

	debug_output[0] = '\0';
	PWM_NOTICE("test cmd(%s)", cmd);

	if (strncmp(cmd, "src:", 4) == 0) {
		disp_pwm_test_source(cmd + 4);
	} else if (strncmp(cmd, "div:", 4) == 0) {
		disp_pwm_test_div(cmd + 4);
	} else if (strncmp(cmd, "grad:", 5) == 0) {
		disp_pwm_test_grad(cmd + 5);
	} else if (strncmp(cmd, "dbg:", 4) == 0) {
		disp_pwm_enable_debug(cmd + 4);
	} else if (strncmp(cmd, "set:", 4) == 0) {
		int count = pwm_parse_triple(cmd + 4, &offset, &value, &mask);

		if (count == 3)
			DISP_REG_MASK(NULL, reg_base + offset, value, mask);
		else if (count == 2) {
			DISP_REG_SET(NULL, reg_base + offset, value);
			mask = 0xffffffff;
		}
		if (count >= 2)
			PWM_MSG("[+0x%03lx] = 0x%08lx(%lu) & 0x%08lx",
			offset, value, value, mask);
	} else if (strncmp(cmd, "dump", 4) == 0) {
		disp_pwm_dump();
	} else if (strncmp(cmd, "pinmux", 6) == 0) {
		disp_pwm_test_pin_mux();
	} else if (strncmp(cmd, "pwmmux:", 7) == 0) {
		unsigned int clksrc = 0;

		clksrc = (unsigned int)(cmd[7] - '0');
		disp_pwm_set_pwmmux(clksrc);
	} else if (strncmp(cmd, "log_num:", 8) == 0) {
		unsigned long log_num = 0;

		pwm_simple_strtoul((char *)(cmd+8),
			(unsigned long *)(&log_num));
		log_num = (log_num < 1) ?
			1 : ((log_num > PWM_LOG_BUFFER_SIZE) ?
			PWM_LOG_BUFFER_SIZE : log_num);
		g_pwm_log_num = (int)log_num;
		PWM_MSG("combine %lu backlight change log in one line",
			log_num);
	} else if (strncmp(cmd, "queryBL", 7) == 0) {
		disp_pwm_query_backlight(debug_output);
	} else if (strncmp(cmd, "pwm_dbg:", 8) == 0) {
		if (cmd[8] == '0')
			pwm_dbg_en = 0;
		else
			pwm_dbg_en = 1;
	}
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799)
	else if (strncmp(cmd, "query_osc", 9) == 0) {
		disp_pwm_ulposc_query(debug_output);
		PWM_MSG("Trigger query ulposc");
	} else if (strncmp(cmd, "osc_cali", 8) == 0) {
		disp_pwm_ulposc_cali();
		PWM_MSG("Trigger ulposc calibration");
	}
#endif
}


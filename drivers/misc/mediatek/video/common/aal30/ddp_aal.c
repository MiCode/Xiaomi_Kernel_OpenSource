// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT6580) || defined(CONFIG_MACH_MT6779)
#include <mtk_leds_drv.h>
#else
#define backlight_brightness_set(x) do { } while (0)
#define MT65XX_LED_MODE_NONE (0)
#define MT65XX_LED_MODE_CUST_LCM (4)
#endif
#include <cmdq_record.h>
#include <ddp_reg.h>
#include <ddp_drv.h>
#include <ddp_path.h>
#include <primary_display.h>
#include <disp_drv_platform.h>
#ifdef CONFIG_MTK_SMI_EXT
#include <mtk_smi.h>
#include <smi_public.h>
#endif
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#include <ddp_clkmgr.h>
#endif
#endif
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#include <disp_lowpower.h>
#include <disp_helper.h>
#endif
#include <ddp_aal.h>
#include <ddp_pwm.h>
#include <ddp_color.h>

#if defined(CONFIG_MACH_MT6799)
/* #include "mt-plat/mtk_chip.h" */
#endif

#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define AAL0_MODULE_NAMING (DISP_MODULE_AAL0)
#else
#define AAL0_MODULE_NAMING (DISP_MODULE_AAL)
#endif

#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define AAL0_CLK_NAMING (DISP0_DISP_AAL0)
#else
#define AAL0_CLK_NAMING (DISP0_DISP_AAL)
#endif

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#define AAL_SUPPORT_PARTIAL_UPDATE
#endif

/* To enable debug log: */
/* # echo aal_dbg:1 > /sys/kernel/debug/dispsys */
int aal_dbg_en;

#define AAL_ERR(fmt, arg...) pr_notice("[AAL] %s: " fmt "\n", __func__, ##arg)
#define AAL_NOTICE(fmt, arg...) pr_info("[AAL] %s: " fmt "\n", __func__, ##arg)
#define AAL_DBG(fmt, arg...) \
	do { if (aal_dbg_en) pr_debug("[AAL] %s: " fmt "\n", __func__, ##arg); \
		} while (0)

static int disp_aal_write_init_regs(enum DISP_MODULE_ENUM module, void *cmdq);
static int disp_aal_write_param_to_reg(enum DISP_MODULE_ENUM module,
	struct cmdqRecStruct *cmdq, const struct DISP_AAL_PARAM *param);

static DECLARE_WAIT_QUEUE_HEAD(g_aal_hist_wq);
static DEFINE_SPINLOCK(g_aal_hist_lock);
static DEFINE_SPINLOCK(g_aal_irq_en_lock);

static struct DISP_AAL_HIST g_aal_hist = {
	.serviceFlags = 0,
	.backlight = -1,
#ifdef AAL_SUPPORT_KERNEL_API
	.essStrengthIndex = ESS_LEVEL_BY_CUSTOM_LIB,
	.ess_enable = ESS_EN_BY_CUSTOM_LIB,
	.dre_enable = DRE_EN_BY_CUSTOM_LIB
#endif
};
static struct DISP_AAL_HIST g_aal_hist_db;
static ddp_module_notify g_ddp_notify;
static atomic_t g_aal_hist_available = ATOMIC_INIT(0);
static atomic_t g_aal_is_init_regs_valid = ATOMIC_INIT(0);
static atomic_t g_aal_backlight_notified = ATOMIC_INIT(1023);
static atomic_t g_aal_initialed = ATOMIC_INIT(0);
static atomic_t g_aal_allowPartial = ATOMIC_INIT(0);
static atomic_t g_aal_force_enable_irq = ATOMIC_INIT(0);
#ifdef LOAD_AAL_SUPPORT_FROM_DTS
static atomic_t g_aal_support = ATOMIC_INIT(-1);
#endif
static atomic_t g_led_mode = ATOMIC_INIT(MT65XX_LED_MODE_NONE);
static bool g_aal_hw_offset;
static bool g_aal_dre_offset_separate;

#define AAL0_OFFSET (0)
#define AAL1_OFFSET (DISPSYS_AAL1_BASE - DISPSYS_AAL0_BASE)

#if defined(CONFIG_MACH_MT6799)
#define AAL_TOTAL_MODULE_NUM (2)

#define aal_get_module_from_id(id) ((id == DISP_AAL0) ? \
	AAL0_MODULE_NAMING : DISP_MODULE_AAL1)
#define aal_get_offset(module) ((module == AAL0_MODULE_NAMING) ? \
	AAL0_OFFSET : AAL1_OFFSET)
#define index_of_aal(module) ((module == AAL0_MODULE_NAMING) ? 0 : 1)

static const unsigned int g_aal_allowed_module[AAL_TOTAL_MODULE_NUM] = {
	AAL0_MODULE_NAMING, DISP_MODULE_AAL1};
static atomic_t g_aal_dirty_frame_retrieved[AAL_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(1), ATOMIC_INIT(1)};
static atomic_t g_aal_is_clock_on[AAL_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0)};
#else
#define AAL_TOTAL_MODULE_NUM (1)

#define aal_get_module_from_id(id) (AAL0_MODULE_NAMING)
#define aal_get_offset(module) (AAL0_OFFSET)
#define index_of_aal(module) (0)

static const unsigned int g_aal_allowed_module[AAL_TOTAL_MODULE_NUM] = {
	AAL0_MODULE_NAMING};
static atomic_t g_aal_dirty_frame_retrieved[AAL_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(1)};
static atomic_t g_aal_is_clock_on[AAL_TOTAL_MODULE_NUM] = {ATOMIC_INIT(0)};
#endif

static atomic_t g_aal_force_relay = ATOMIC_INIT(0);

enum AAL_UPDATE_HIST {
	UPDATE_NONE = 0,
	UPDATE_SINGLE,
	UPDATE_MULTIPLE
};

#ifdef CONFIG_MTK_DRE30_SUPPORT
#define AAL_DRE_HIST_START			(1024)
#define AAL_DRE_HIST_END			(4092)
#define AAL_DRE_GAIN_START			(4096)
#define AAL_DRE_GAIN_END			(6268)

static DEFINE_SPINLOCK(g_aal_dre3_gain_lock);
static atomic_t g_aal_force_hist_apb = ATOMIC_INIT(0);
static atomic_t g_aal_dre_halt = ATOMIC_INIT(0);
static atomic_t g_aal_dre_hw_init = ATOMIC_INIT(0);
static struct DISP_DRE30_INIT g_aal_init_dre30;
static struct DISP_DRE30_PARAM g_aal_gain;
static struct DISP_DRE30_PARAM g_aal_gain_db;
static struct DISP_DRE30_HIST g_aal_dre30_hist;
static struct DISP_DRE30_HIST g_aal_dre30_hist_db;

static DECLARE_WAIT_QUEUE_HEAD(g_aal_size_wq);
static bool g_aal_get_size_available;
static struct DISP_AAL_DISPLAY_SIZE g_aal_size;
#else
static struct DISP_AAL_HIST g_aal_hist_multi_pipe;
/* Locked by  g_aal#_hist_lock */
static unsigned int g_aal_module_hist_count[AAL_TOTAL_MODULE_NUM];
/* Locked by  g_aal_hist_lock */
static unsigned int g_aal_hist_count;

static atomic_t g_aal_reset_count = ATOMIC_INIT(0);
static atomic_t g_aal_prev_pipe = ATOMIC_INIT(UPDATE_NONE);

static DEFINE_SPINLOCK(g_aal0_hist_lock);
static DEFINE_SPINLOCK(g_aal1_hist_lock);

#define aal_index_hist_spin_trylock(index, flags, getlock) \
do { \
	if (index == 0) \
		getlock = spin_trylock_irqsave(&g_aal0_hist_lock, flags); \
	else \
		getlock = spin_trylock_irqsave(&g_aal1_hist_lock, flags); \
} while (0)

#define aal_index_hist_spin_lock(index, flags) \
do { \
	if (index == 0) \
		spin_lock_irqsave(&g_aal0_hist_lock, flags); \
	else \
		spin_lock_irqsave(&g_aal1_hist_lock, flags); \
} while (0)

#define aal_index_hist_spin_unlock(index, flags) \
do { \
	if (index == 0) \
		spin_unlock_irqrestore(&g_aal0_hist_lock, flags); \
	else \
		spin_unlock_irqrestore(&g_aal1_hist_lock, flags); \
} while (0)

#define AAL_MAX_HIST_COUNT	     (0xFFFFFFFF)
#endif			/* CONFIG_MTK_DRE30_SUPPORT */

#ifdef AAL_HAS_DRE3
static atomic_t g_aal_change_to_dre30 = ATOMIC_INIT(0);
#endif

#ifdef AAL_SUPPORT_KERNEL_API
static atomic_t g_aal_panel_type = ATOMIC_INIT(CONFIG_BY_CUSTOM_LIB);
static int g_aal_ess_level = ESS_LEVEL_BY_CUSTOM_LIB;
static int g_aal_dre_en = DRE_EN_BY_CUSTOM_LIB;
static int g_aal_ess_en = ESS_EN_BY_CUSTOM_LIB;
static int g_aal_ess_level_cmd_id;
static int g_aal_dre_en_cmd_id;
static int g_aal_ess_en_cmd_id;
#endif

#define aal_min(a, b)			(((a) < (b)) ? (a) : (b))

static inline bool disp_aal_check_module(enum DISP_MODULE_ENUM module,
	const char *func, int line)
{
	unsigned int i = 0;

	for (i = 0; i < AAL_TOTAL_MODULE_NUM; i++) {
		if (g_aal_allowed_module[i] == module)
			return true;
	}

	AAL_ERR("%s:%d, can not find module for (%d)\n", func, line, module);
	return false;
}

static int disp_aal_get_cust_led(void)
{
	struct device_node *led_node = NULL;
	int ret = 0;
	int led_mode;
	int pwm_config[5] = { 0 };

	led_node = of_find_compatible_node(NULL, NULL,
	"mediatek,lcd-backlight");
	if (!led_node) {
		ret = -1;
		AAL_ERR("Cannot find LED node from dts\n");
	} else {
		ret = of_property_read_u32(led_node, "led_mode", &led_mode);
		if (!ret)
			atomic_set(&g_led_mode, led_mode);
		else
			AAL_ERR("led dts can not get led mode data.\n");

		ret = of_property_read_u32_array(led_node,
	    "pwm_config", pwm_config, ARRAY_SIZE(pwm_config));
	}

	if (ret)
		AAL_ERR("get pwm cust info fail");
	AAL_DBG("mode=%u", atomic_read(&g_led_mode));

	return ret;
}
#ifdef LOAD_AAL_SUPPORT_FROM_DTS
static int disp_aal_get_support_from_dts(void)
{
	struct device_node *aal_node = NULL;
	int ret = 0;
	int aal_support = 0;

	aal_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_aal0");
	if (!aal_node) {
		ret = -1;
		AAL_ERR("Cannot find aal node from dts\n");
	} else {
		ret = of_property_read_u32(aal_node,
	    "aal_support", &aal_support);
		if (ret != 0)
			AAL_ERR("can not get aal support data.\n");
	}

	if (ret != 0)
		AAL_ERR("get aal cust info fail");

	AAL_NOTICE("aal_support = %d", aal_support);

	return aal_support;
}

bool disp_aal_is_support(void)
{
	int is_aal_support = atomic_read(&g_aal_support);

	if (is_aal_support < 0) {
		is_aal_support = disp_aal_get_support_from_dts();
		atomic_set(&g_aal_support, is_aal_support);
	}

	return (is_aal_support > 0) ? true : false;
}
#else
bool disp_aal_is_support(void)
{
#ifdef CONFIG_MTK_AAL_SUPPORT
	return true;
#else
	return false;
#endif
}
#endif
static void backlight_brightness_set_with_lock(int bl_1024)
{
	_primary_path_switch_dst_lock();
	primary_display_manual_lock();

	backlight_brightness_set(bl_1024);

	primary_display_manual_unlock();
	_primary_path_switch_dst_unlock();
}

static int disp_aal_exit_idle(const char *caller, int need_kick)
{
#ifdef MTK_DISP_IDLE_LP
	disp_exit_idle_ex(caller);
#endif
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799)
	if (need_kick == 1)
		if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS))
			primary_display_idlemgr_kick(__func__, 1);
#endif
	return 0;
}

static int disp_aal_init(enum DISP_MODULE_ENUM module, int width, int height,
	void *cmdq)
{
	const int index = index_of_aal(module);

	if (disp_aal_is_support() == true &&
		atomic_read(&g_aal_force_relay) != 1) {
		/* Enable AAL histogram, engine */
		DISP_REG_MASK(cmdq, DISP_AAL_CFG + aal_get_offset(module),
		    0x3 << 1, (0x3 << 1) | 0x1);
	} else {
		/* Disable AAL histogram, engine */
		DISP_REG_MASK(cmdq, DISP_AAL_CFG + aal_get_offset(module),
		    0x0 << 1, (0x3 << 1) | 0x1);
	}

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	/* disable stall cg for avoid display path hang */
	DISP_REG_MASK(cmdq, DISP_AAL_CFG + aal_get_offset(module), 0x1 << 4,
		0x1 << 4);
#endif
	/* get lcd-backlight mode from dts */
	if (atomic_read(&g_led_mode) == MT65XX_LED_MODE_NONE)
		disp_aal_get_cust_led();

	atomic_set(&g_aal_hist_available, 0);
	atomic_set(&g_aal_dirty_frame_retrieved[index], 1);

	return 0;
}

static void disp_aal_trigger_refresh(int latency)
{
	if (g_ddp_notify != NULL) {
		enum DISP_PATH_EVENT trigger_method = DISP_PATH_EVENT_TRIGGER;

#ifdef DISP_PATH_DELAYED_TRIGGER_33ms_SUPPORT
		if (latency == AAL_REFRESH_33MS)
			trigger_method = DISP_PATH_EVENT_DELAYED_TRIGGER_33ms;
#endif
		g_ddp_notify(AAL0_MODULE_NAMING, trigger_method);
		AAL_DBG("trigger_method = %d", trigger_method);
	}
}

static void disp_aal_set_interrupt_by_module(enum DISP_MODULE_ENUM module,
	int enabled)
{
	const int offset = aal_get_offset(module);
	const int index = index_of_aal(module);

	if (atomic_read(&g_aal_is_clock_on[index]) != 1) {
		AAL_DBG("clock is off");
		return;
	}

	if (enabled &&
		disp_aal_is_support() == true &&
		atomic_read(&g_aal_force_relay) != 1) {
		if (DISP_REG_GET(DISP_AAL_EN + offset) == 0)
			AAL_DBG("[WARNING] module(%d) DISP_AAL_EN not enabled!",
				module);

		/* Enable output frame end interrupt */
		DISP_CPU_REG_SET(DISP_AAL_INTEN + offset, 0x2);
		AAL_DBG("Module(%d) interrupt enabled", module);
	} else if (!enabled) {
		if (atomic_read(&g_aal_dirty_frame_retrieved[index]) == 1) {
			DISP_CPU_REG_SET(DISP_AAL_INTEN + offset, 0x0);
			AAL_DBG("Module(%d) interrupt disabled", module);
		} else {
			/* Dirty histogram was not retrieved. */
			/* Only if the dirty hist was retrieved, */
			/* interrupt can be disabled. */
			/* Continue interrupt until AALService can get */
			/* the latest histogram. */
		}
	}
}

static void disp_aal_set_interrupt(int enabled)
{
	enum DISP_MODULE_ENUM module = AAL0_MODULE_NAMING;
	int i;
	int config_module_num = 1;

#if defined(CONFIG_MACH_MT6799)
	if (primary_display_get_pipe_status() != SINGLE_PIPE)
		config_module_num = AAL_TOTAL_MODULE_NUM;
#endif
	for (i = 0; i < config_module_num; i++) {
		module += i;
		AAL_DBG("Set AAL%d interrupt enable (%d)", i, enabled);
		disp_aal_set_interrupt_by_module(module, enabled);
	}
}

static void disp_aal_notify_frame_dirty(enum DISP_MODULE_ENUM module)
{
	unsigned long flags;
	const int index = index_of_aal(module);

	if (disp_aal_is_support() == false)
		return;

	AAL_DBG("Module(%d)", module);

	disp_aal_exit_idle(__func__, 0);

	spin_lock_irqsave(&g_aal_irq_en_lock, flags);
	/* Interrupt can be disabled until dirty histogram is retrieved */
	atomic_set(&g_aal_dirty_frame_retrieved[index], 0);
	disp_aal_set_interrupt_by_module(module, 1);
	spin_unlock_irqrestore(&g_aal_irq_en_lock, flags);
}

static int disp_aal_wait_hist(unsigned long timeout)
{
	int ret = 0;

	if (atomic_read(&g_aal_hist_available) == 0) {
		ret = wait_event_interruptible(g_aal_hist_wq,
			atomic_read(&g_aal_hist_available) == 1);
		AAL_DBG("hist_available = 1, waken up, ret = %d",
			ret);
	} else {
		/* If g_aal_hist_available is already set, */
		/* means AALService was delayed */
		AAL_DBG("hist_available = 0");
	}

	return ret;
}

static inline bool disp_aal_reg_set(enum DISP_MODULE_ENUM module, void *cmdq,
			      unsigned long addr, unsigned int value)
{
	if (atomic_read(&g_aal_is_clock_on[index_of_aal(module)]) != 1) {
		AAL_DBG("disp_aal_reg_set: clock is off");
		return false;
	}

	DISP_REG_SET(cmdq, addr, value);
	return true;
}

static inline bool disp_aal_reg_get(enum DISP_MODULE_ENUM module,
			      unsigned long addr, unsigned int *value)
{
	if (atomic_read(&g_aal_is_clock_on[index_of_aal(module)]) != 1) {
		AAL_DBG("disp_aal_reg_get: clock is off");
		return false;
	}

	*value = DISP_REG_GET(addr);
	return true;
}

static bool disp_aal_read_single_hist(enum DISP_MODULE_ENUM module)
{
	const int offset = aal_get_offset(module);
#if defined(CONFIG_MACH_MT6799)
	const int color_offset = (module == AAL0_MODULE_NAMING) ?
		0 : (DISPSYS_COLOR1_BASE - DISPSYS_COLOR0_BASE);
	const enum DISP_MODULE_ENUM color_module =
		(module == AAL0_MODULE_NAMING) ?
		DISP_MODULE_COLOR0 : DISP_MODULE_COLOR1;
#else
	const int color_offset = 0;
	const enum DISP_MODULE_ENUM color_module = DISP_MODULE_COLOR0;
#endif
	bool read_success = false;
	int i;

	for (i = 0; i < AAL_HIST_BIN; i++) {
		read_success = disp_aal_reg_get(module,
			DISP_AAL_STATUS_00 + offset + (i << 2),
			&g_aal_hist.maxHist[i]);
		if (read_success != true)
			break;
	}
#ifdef AAL_HAS_YHIST
	for (i = 0; i < AAL_HIST_BIN; i++) {
		read_success = disp_aal_reg_get(module,
			DISP_Y_HISTOGRAM_00 + offset + (i << 2),
			&g_aal_hist.yHist[i]);
		if (read_success != true)
			break;
	}
#endif
	if (read_success == true) {
		read_success = disp_color_reg_get(color_module,
			DISP_COLOR_TWO_D_W1_RESULT + color_offset,
				&g_aal_hist.colorHist);
	}

	return read_success;
}

static void disp_aal_clear_irq(enum DISP_MODULE_ENUM module, bool cleared,
	bool is_log)
{
	unsigned int intsta;
	const int index = index_of_aal(module);
	const int offset = aal_get_offset(module);

	/* Check current irq status */
	do {
		intsta = 0;
		disp_aal_reg_get(module, DISP_AAL_INTSTA + offset, &intsta);
		disp_aal_reg_set(module, NULL, DISP_AAL_INTSTA + offset,
			(intsta & ~0x3));

		atomic_set(&g_aal_dirty_frame_retrieved[index], 1);
	} while (0);

	/*
	 * no need per-frame wakeup.
	 * We stop interrupt until next frame dirty.
	 */
	if (cleared == true)
		disp_aal_set_interrupt_by_module(module, 0);

	if (is_log == true) {
		/* print log */
		AAL_NOTICE("Module(%d), process:(%d)",
			module, cleared);
	} else {
		AAL_DBG("Module(%d), process:(%d)",
			module, cleared);
	}
}

#ifdef CONFIG_MTK_DRE30_SUPPORT
#define AAL_DRE_BLK_NUM			(16)
#define AAL_BLK_MAX_ALLOWED_NUM	(128)
#define AAL_DRE3_POINT_NUM		(17)
#define AAL_DRE_GAIN_POINT16_START	(512)

#define DRE_POLL_SLEEP_TIME_US	(10)
#define DRE_MAX_POLL_TIME_US	(1000)

static inline bool disp_aal_reg_mask(enum DISP_MODULE_ENUM module, void *cmdq,
	unsigned long addr, unsigned int value, unsigned int mask)
{
	if (atomic_read(&g_aal_is_clock_on[index_of_aal(module)]) != 1) {
		AAL_DBG("disp_aal_reg_mask: clock is off");
		return false;
	}

	DISP_REG_MASK(cmdq, addr, value, mask);
	return true;
}

static inline bool disp_aal_reg_poll(enum DISP_MODULE_ENUM module, void *cmdq,
	unsigned long addr, unsigned int value, unsigned int mask)
{
	bool return_value = false;
	unsigned int reg_value = 0;
	unsigned int polling_time = 0;

	if (cmdq == NULL) {
		do {
			if (disp_aal_reg_get(module, addr, &reg_value) != true)
				break;

			if ((reg_value & mask) == value) {
				return_value = true;
				break;
			}

			udelay(DRE_POLL_SLEEP_TIME_US);
			polling_time += DRE_POLL_SLEEP_TIME_US;
		} while (polling_time < DRE_MAX_POLL_TIME_US);
	} else {
		DISP_REG_CMDQ_POLLING(cmdq, addr, value, mask);
		return_value = true;
	}

	return return_value;
}

static inline bool disp_aal_sram_write(void *cmdq, unsigned int addr,
	unsigned int value)
{
	bool return_value = false;

	do {
		if (disp_aal_reg_set(AAL0_MODULE_NAMING, cmdq,
			DISP_AAL_SRAM_RW_IF_0, addr) != true)
			break;

		if (disp_aal_reg_poll(AAL0_MODULE_NAMING, cmdq,
			DISP_AAL_SRAM_STATUS, (0x1 << 16), (0x1 << 16)) != true)
			break;

		if (disp_aal_reg_set(AAL0_MODULE_NAMING, cmdq,
			DISP_AAL_SRAM_RW_IF_1, value) != true)
			break;

		return_value = true;
	} while (0);

	return return_value;
}

static inline bool disp_aal_sram_read(unsigned int addr, unsigned int *value)
{
	bool return_value = false;

	do {
		if (disp_aal_reg_set(AAL0_MODULE_NAMING, NULL,
			DISP_AAL_SRAM_RW_IF_2, addr) != true)
			break;

		if (disp_aal_reg_poll(AAL0_MODULE_NAMING, NULL,
			DISP_AAL_SRAM_STATUS, (0x1 << 17), (0x1 << 17)) != true)
			break;

		if (disp_aal_reg_get(AAL0_MODULE_NAMING, DISP_AAL_SRAM_RW_IF_3,
			value) != true)
			break;

		return_value = true;
	} while (0);

	return return_value;
}

static bool disp_aal_read_dre3(const int dre_blk_x_num,
	const int dre_blk_y_num)
{
	int hist_offset;
	int arry_offset = 0;
	unsigned int read_value;

	/* Read Global histogram for ESS */
	if (disp_aal_read_single_hist(AAL0_MODULE_NAMING) != true)
		return false;

	/* Read Local histogram for DRE 3 */
	for (hist_offset = AAL_DRE_HIST_START; hist_offset <= AAL_DRE_HIST_END;
		hist_offset += 4) {
		if (disp_aal_sram_read(hist_offset, &read_value) != true)
			return false;

		if (arry_offset >= AAL_DRE30_HIST_REGISTER_NUM)
			return false;
		g_aal_dre30_hist.dre_hist[arry_offset++] = read_value;
	}

	return true;
}

static bool disp_aal_write_dre3(void *cmq_handle)
{
	int gain_offset;
	int arry_offset = 0;
	unsigned int write_value;

	/* Write Local Gain Curve for DRE 3 */
	for (gain_offset = AAL_DRE_GAIN_START; gain_offset <= AAL_DRE_GAIN_END;
		gain_offset += 4) {
		if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		write_value = g_aal_gain.dre30_gain[arry_offset++];

		if (disp_aal_sram_write(cmq_handle, gain_offset, write_value) !=
			true)
			return false;
	}

	return true;
}

static void disp_aal_dre3_irq_handle(enum DISP_MODULE_ENUM module,
	int update_method)
{
	bool config_success = false;
	bool read_success = false;
	unsigned long flags;
	int dre_blk_x_num, dre_blk_y_num;
	unsigned int read_value;
	int hist_apb, hist_int;

	/* Only process AAL0 in single module state */
	if (module != AAL0_MODULE_NAMING) {
		AAL_ERR("dre3 error: Module(%d) in irq_handler",
			module);
		disp_aal_clear_irq(module, true, true);
		return;
	}
	disp_aal_clear_irq(module, false, false);

	if (update_method != UPDATE_SINGLE) {
		AAL_ERR("dre3 error: update(%d), Module(%d)",
			update_method, module);
		return;
	}

	if (atomic_read(&g_aal_change_to_dre30) != 0x3)
		return;

	if (atomic_read(&g_aal_dre_halt) == 0) {
		if (atomic_cmpxchg(&g_aal_force_hist_apb, 0, 1) == 0) {
			hist_apb = 0;
			hist_int = 1;
		} else if (atomic_cmpxchg(&g_aal_force_hist_apb, 1, 0) == 1) {
			hist_apb = 1;
			hist_int = 0;
		} else {
			AAL_ERR("Error when get hist_apb irq_handler");
			return;
		}
		AAL_DBG("hist_apb (%d), hist_int (%d) in irq_handler",
			hist_apb, hist_int);
		config_success = disp_aal_reg_mask(module, NULL,
			DISP_AAL_SRAM_CFG,
			(hist_int << 6)|(hist_apb << 5)|(1 << 4), (0x7 << 4));
		if (config_success != true)
			return;

		atomic_set(&g_aal_dre_halt, 1);

		read_success = disp_aal_reg_get(module,
			DISP_AAL_DRE_BLOCK_INFO_01, &read_value);
		if (read_success == true) {
			dre_blk_x_num = aal_min(AAL_DRE_BLK_NUM,
				read_value & 0x1F);
			dre_blk_y_num =
				aal_min(AAL_BLK_MAX_ALLOWED_NUM/dre_blk_x_num,
				    (read_value >> 5) & 0x1F);

			if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
				read_success = disp_aal_read_dre3(dre_blk_x_num,
					dre_blk_y_num);
				if (read_success == true) {
					g_aal_dre30_hist.dre_blk_x_num =
						dre_blk_x_num;
					g_aal_dre30_hist.dre_blk_y_num =
						dre_blk_y_num;
					atomic_set(&g_aal_hist_available, 1);
				}
				spin_unlock_irqrestore(&g_aal_hist_lock, flags);

				if (read_success == true)
					wake_up_interruptible(&g_aal_hist_wq);
			}

			if (spin_trylock_irqsave(&g_aal_dre3_gain_lock,
				flags)) {
				/* Write DRE 3.0 gain */
				config_success = disp_aal_write_dre3(NULL);
				spin_unlock_irqrestore(&g_aal_dre3_gain_lock,
					flags);
			}
		}

		atomic_set(&g_aal_dre_halt, 0);
	}
}

static int disp_aal_wait_size(unsigned long timeout)
{
	int ret = 0;

	if (g_aal_get_size_available == false) {
		ret = wait_event_interruptible(g_aal_size_wq,
		g_aal_get_size_available == true);
		AAL_DBG("size_available = 1, Waken up, ret = %d",
			ret);
	} else {
		/* If g_aal_get_size_available is already set, */
		/* means AALService was delayed */
		AAL_DBG("size_available = 0");
	}

	return ret;
}

static int disp_aal_copy_size_to_user
	(struct DISP_AAL_DISPLAY_SIZE __user *aal_size)
{
	int ret = -EFAULT;

	ret = copy_to_user(aal_size, &g_aal_size,
	sizeof(struct DISP_AAL_DISPLAY_SIZE));

	AAL_DBG("ret = %d", ret);

	return ret;
}

static void ddp_aal_dre3_write_curve_full(void *cmq_handle)
{
	DISP_REG_MASK(cmq_handle, DISP_AAL_SRAM_CFG, (1 << 6)|(0 << 5)|(1 << 4),
		(0x7 << 4));
	disp_aal_write_dre3(cmq_handle);
	DISP_REG_MASK(cmq_handle, DISP_AAL_SRAM_CFG, (0 << 6)|(1 << 5)|(1 << 4),
		(0x7 << 4));
	disp_aal_write_dre3(cmq_handle);
	atomic_set(&g_aal_force_hist_apb, 0);
}

static bool write_block(const unsigned int *dre3_gain,
	const int block_x, const int block_y, const int dre_blk_x_num)
{
	bool return_value = false;
	uint32_t block_offset = 4 * (block_y * dre_blk_x_num + block_x);

	do {
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[0] & 0xff) |
			((dre3_gain[1] & 0xff) << 8) |
			((dre3_gain[2] & 0xff) << 16) |
			((dre3_gain[3] & 0xff) << 24));

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[4] & 0xff) |
			((dre3_gain[5] & 0xff) << 8) |
			((dre3_gain[6] & 0xff) << 16) |
			((dre3_gain[7] & 0xff) << 24));

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[8] & 0xff) |
			((dre3_gain[9] & 0xff) << 8) |
			((dre3_gain[10] & 0xff) << 16) |
			((dre3_gain[11] & 0xff) << 24));

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[12] & 0xff) |
			((dre3_gain[13] & 0xff) << 8) |
			((dre3_gain[14] & 0xff) << 16) |
			((dre3_gain[15] & 0xff) << 24));

		return_value = true;
	} while (0);

	return return_value;
}

static bool write_curve16(const unsigned int *dre3_gain,
	const int dre_blk_x_num, const int dre_blk_y_num)
{
	int32_t blk_x, blk_y;
	const int32_t blk_num_max = dre_blk_x_num * dre_blk_y_num;
	unsigned int write_value = 0x0;
	uint32_t bit_shift = 0;
	uint32_t block_offset = AAL_DRE_GAIN_POINT16_START;

	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			write_value |=
				((dre3_gain[16] & 0xff) << (8*bit_shift));
			bit_shift++;

			if (bit_shift >= 4) {
				if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
					return false;
				g_aal_gain.dre30_gain[block_offset++] =
					write_value;

				write_value = 0x0;
				bit_shift = 0;
			}
		}
	}

	if ((blk_num_max>>2)<<2 != blk_num_max) {
		/* configure last curve */
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		g_aal_gain.dre30_gain[block_offset] = write_value;
	}

	return true;
}

static void disp_aal_dre3_init(void *cmq_handle)
{
	const int dre_blk_x_num = 8;
	const int dre_blk_y_num = 16;
	unsigned long flags;
	int blk_x, blk_y, curve_point;
	unsigned int dre3_gain[AAL_DRE3_POINT_NUM];

	for (curve_point = 0; curve_point < AAL_DRE3_POINT_NUM;
		curve_point++) {
		/* assign initial gain curve */
		dre3_gain[curve_point] = aal_min(255, 16 * curve_point);
	}

	spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			/* write each block dre curve */
			write_block(dre3_gain, blk_x, blk_y, dre_blk_x_num);
		}
	}
    /* write each block dre curve last point */
	write_curve16(dre3_gain, dre_blk_x_num, dre_blk_y_num);

	ddp_aal_dre3_write_curve_full(cmq_handle);
	spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
}

static int disp_aal_set_init_dre3(struct DISP_DRE30_INIT __user *user_regs)
{
	int ret = -EFAULT;
	struct DISP_DRE30_INIT *init_dre3;

	init_dre3 = &g_aal_init_dre30;

	ret = copy_from_user(init_dre3, user_regs,
		sizeof(struct DISP_DRE30_INIT));
	if (ret == 0) {
		/* Modify DRE3.0 config flag */
		atomic_or(0x2, &g_aal_change_to_dre30);
	} else {
		AAL_ERR("copy_from_user() failed");
	}

	AAL_DBG("ret = %d", ret);

	return ret;
}
#else
static void disp_aal_single_pipe_hist_update(enum DISP_MODULE_ENUM module)
{
	unsigned int intsta;
	unsigned long flags;
	const int index = index_of_aal(module);
	const int offset = aal_get_offset(module);
	bool read_success = false;

	do {
		/* Only process AAL0 in single module state */
		if (module != AAL0_MODULE_NAMING) {
			disp_aal_clear_irq(module, true, true);
			break;
		}

		intsta = 0;
		disp_aal_reg_get(module, DISP_AAL_INTSTA + offset, &intsta);
		AAL_DBG("Module(%d), intsta: 0x%x", module, intsta);

		/* Only process end of frame state */
		if ((intsta & 0x2) == 0x0)
			break;

		disp_aal_reg_set(module, NULL, DISP_AAL_INTSTA + offset,
			(intsta & ~0x3));

		/* Allow to disable interrupt */
		atomic_set(&g_aal_dirty_frame_retrieved[index], 1);

		if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
			read_success = disp_aal_read_single_hist(module);

			if (read_success == true)
				atomic_set(&g_aal_hist_available, 1);

			spin_unlock_irqrestore(&g_aal_hist_lock, flags);

			if (read_success == true)
				wake_up_interruptible(&g_aal_hist_wq);
		} else {
			/*
			 * Histogram was not be retrieved, but it's OK.
			 * Another interrupt will come until histogram available
			 * See: disp_aal_set_interrupt()
			 */
		}

		if (atomic_read(&g_aal_is_init_regs_valid) == 0) {
			/*
			 * AAL service is not running, not need per-frame wakeup
			 * We stop interrupt until next frame dirty.
			 */
			disp_aal_set_interrupt_by_module(module, 0);
		}
	} while (0);
}

static void disp_aal_reset_count(void)
{
	int i;
	unsigned long flags;

	AAL_DBG("triggered");
	for (i = 0; i < AAL_TOTAL_MODULE_NUM; i++) {
		aal_index_hist_spin_lock(i, flags);
		g_aal_module_hist_count[i] = 0;
		aal_index_hist_spin_unlock(i, flags);
	}

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	g_aal_hist_count = 0;
	atomic_set(&g_aal_reset_count, 0);
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);
}

static void disp_aal_multiple_pipe_hist_update(enum DISP_MODULE_ENUM module)
{
	unsigned int intsta;
	int i;
	unsigned long flags;
	unsigned int hist_count;
	int is_hist_available = 0;
	const int index = index_of_aal(module);
	const int offset = aal_get_offset(module);
#if defined(CONFIG_MACH_MT6799)
	const int color_offset = (module == AAL0_MODULE_NAMING) ?
	    0 : (DISPSYS_COLOR1_BASE - DISPSYS_COLOR0_BASE);
	const enum DISP_MODULE_ENUM color_module =
			(module == AAL0_MODULE_NAMING) ?
			DISP_MODULE_COLOR0 : DISP_MODULE_COLOR1;
#else
	const int color_offset = 0;
	const enum DISP_MODULE_ENUM color_module = DISP_MODULE_COLOR0;
#endif
	bool read_success = false;
	unsigned int temp_max_hist, temp_color_hist;
	int getlock;

	do {
		intsta = 0;
		disp_aal_reg_get(module, DISP_AAL_INTSTA + offset, &intsta);
		AAL_DBG("Module(%d), intsta: 0x%x", module, intsta);

		/* Only process end of frame state */
		if ((intsta & 0x2) == 0x0)
			break;

		disp_aal_reg_set(module, NULL, DISP_AAL_INTSTA + offset,
			(intsta & ~0x3));

		/* Check current irq status again */
		intsta = 0;
		disp_aal_reg_get(module, DISP_AAL_INTSTA + offset, &intsta);
		if ((intsta & 0x3) != 0) {
			/* print error message */
			AAL_ERR("intsta error:(0x%08x), Module(%d), in (%s)",
				intsta, module, __func__);
		}

		/* Allow to disable interrupt */
		atomic_set(&g_aal_dirty_frame_retrieved[index], 1);

		aal_index_hist_spin_trylock(index, flags, getlock);
		if (getlock <= 0)
			break;

		g_aal_module_hist_count[index] =
			(g_aal_module_hist_count[index] + 1) &
			AAL_MAX_HIST_COUNT;

		hist_count = g_aal_module_hist_count[index];
		aal_index_hist_spin_unlock(index, flags);

		if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
			if ((hist_count-g_aal_hist_count) == 1 ||
				(hist_count == 0 && g_aal_hist_count > 0)) {
				for (i = 0; i < AAL_HIST_BIN; i++) {
					read_success = disp_aal_reg_get(module,
			DISP_AAL_STATUS_00 + offset + (i << 2),
				&g_aal_hist_multi_pipe.maxHist[i]);
					if (read_success != true)
						break;
				}
				if (read_success == true) {
					read_success =
					disp_color_reg_get(color_module,
					DISP_COLOR_TWO_D_W1_RESULT +
					color_offset,
					&g_aal_hist_multi_pipe.colorHist);

					if (read_success == true)
						g_aal_hist_count = hist_count;
				}
				AAL_DBG("hist_combination: copy histogram");
			} else if (hist_count == g_aal_hist_count) {
				for (i = 0; i < AAL_HIST_BIN; i++) {
					read_success = disp_aal_reg_get(module,
					DISP_AAL_STATUS_00 + offset + (i << 2),
					&temp_max_hist);
					if (read_success != true)
						break;
					g_aal_hist.maxHist[i] =
					g_aal_hist_multi_pipe.maxHist[i] +
					temp_max_hist;
				}
				if (read_success == true) {
					read_success =
					disp_color_reg_get(color_module,
					DISP_COLOR_TWO_D_W1_RESULT +
					color_offset,
					&temp_color_hist);
					g_aal_hist.colorHist =
					g_aal_hist_multi_pipe.colorHist +
					temp_color_hist;

					if (read_success == true) {
						atomic_set(
							&g_aal_hist_available,
							1);
						is_hist_available = 1;
					}
				}
				AAL_DBG("hist_combination: hist is updated");
			} else {
				atomic_set(&g_aal_reset_count, 1);
				AAL_DBG("Can not update histogram now");
			}

			spin_unlock_irqrestore(&g_aal_hist_lock, flags);

			if (is_hist_available == 1)
				wake_up_interruptible(&g_aal_hist_wq);
		} else {
			/*
			 * Histogram was not be retrieved, but it's OK.
			 * Another interrupt will come until histogram available
			 * See: disp_aal_set_interrupt()
			 */
		}

		if (atomic_read(&g_aal_is_init_regs_valid) == 0) {
			/*
			 * AAL service is not running, not need per-frame wakeup
			 * We stop interrupt until next frame dirty.
			 */
			disp_aal_set_interrupt_by_module(module, 0);
		}
	} while (0);
}
#endif				/* CONFIG_MTK_DRE30_SUPPORT */

void disp_aal_on_end_of_frame_by_module(enum disp_aal_id_t id)
{
	int update_method = UPDATE_SINGLE;
	enum DISP_MODULE_ENUM module = aal_get_module_from_id(id);
#if defined(CONFIG_MACH_MT6799)
	int pipe_status = primary_display_get_pipe_status();

	if (pipe_status == SINGLE_PIPE) {
		update_method = UPDATE_SINGLE;
		AAL_DBG("single mode,  process Module(%d) in irq_handler",
			module);
	} else if (pipe_status == DUAL_PIPE) {
		update_method = UPDATE_MULTIPLE;
		AAL_DBG("dual mode,  process Module(%d) in irq_handler",
			module);
	} else {
		update_method = UPDATE_NONE;
		AAL_DBG("pipe_status (%d), process Module(%d) in irq_handler",
			pipe_status, module);
	}
#endif

	if (id < DISP_AAL0 || id >= DISP_AAL0 + AAL_TOTAL_MODULE_NUM)
		return;

	if (atomic_read(&g_aal_force_relay) == 1) {
		disp_aal_clear_irq(module, true, false);
		return;
	}

#ifdef CONFIG_MTK_DRE30_SUPPORT
	disp_aal_dre3_irq_handle(module, update_method);
#else
	if (update_method == UPDATE_SINGLE) {
		/* Process single AAL engine */
		disp_aal_single_pipe_hist_update(module);
	} else if (update_method == UPDATE_MULTIPLE) {
		/* Process multiple AAL engine */
		if (atomic_read(&g_aal_prev_pipe) != update_method ||
			atomic_read(&g_aal_reset_count) == 1)
			disp_aal_reset_count();
		disp_aal_multiple_pipe_hist_update(module);
	} else {
		disp_aal_clear_irq(module, false, true);
	}

	atomic_set(&g_aal_prev_pipe, update_method);
#endif				/* CONFIG_MTK_DRE30_SUPPORT */
}

void disp_aal_on_end_of_frame(void)
{
	disp_aal_on_end_of_frame_by_module(DISP_AAL0);
}


#define LOG_INTERVAL_TH 200
#define LOG_BUFFER_SIZE 4
static char g_aal_log_buffer[256] = "";
static int g_aal_log_index;
struct timeval g_aal_log_prevtime = {0};

static unsigned long timevaldiff(struct timeval *starttime,
	struct timeval *finishtime)
{
	unsigned long msec;

	msec = (finishtime->tv_sec-starttime->tv_sec)*1000;
	msec += (finishtime->tv_usec-starttime->tv_usec)/1000;

	return msec;
}

static void disp_aal_notify_backlight_log(int bl_1024)
{
	struct timeval aal_time;
	unsigned long diff_mesc = 0;
	unsigned long tsec;
	unsigned long tusec;

	do_gettimeofday(&aal_time);
	tsec = (unsigned long)aal_time.tv_sec % 100;
	tusec = (unsigned long)aal_time.tv_usec / 1000;

	diff_mesc = timevaldiff(&g_aal_log_prevtime, &aal_time);
	AAL_DBG("time diff = %lu", diff_mesc);

	if (diff_mesc > LOG_INTERVAL_TH) {
		if (g_aal_log_index == 0) {
			pr_debug("disp_aal_notify_backlight_changed: %d/1023\n",
				bl_1024);
		} else {
			sprintf(g_aal_log_buffer + strlen(g_aal_log_buffer),
				", %d/1023 %03lu.%03lu",
				bl_1024, tsec, tusec);
			pr_debug("%s\n", g_aal_log_buffer);
			g_aal_log_index = 0;
		}
	} else {
		if (g_aal_log_index == 0) {
			sprintf(g_aal_log_buffer,
			"disp_aal_notify_backlight_changed %d/1023 %03lu.%03lu",
			bl_1024, tsec, tusec);
			g_aal_log_index += 1;
		} else {
			sprintf(g_aal_log_buffer + strlen(g_aal_log_buffer),
				", %d/1023 %03lu.%03lu",
				bl_1024, tsec, tusec);
			g_aal_log_index += 1;
		}

		if ((g_aal_log_index >= LOG_BUFFER_SIZE) || (bl_1024 == 0)) {
			pr_debug("%s\n", g_aal_log_buffer);
			g_aal_log_index = 0;
		}
	}

	memcpy(&g_aal_log_prevtime, &aal_time, sizeof(struct timeval));
}

void disp_aal_notify_backlight_changed(int bl_1024)
{
	unsigned long flags;
	int max_backlight;
	unsigned int service_flags;

	/* pr_debug("disp_aal_notify_backlight_changed: %d/1023", bl_1024); */
	disp_aal_notify_backlight_log(bl_1024);

	disp_aal_exit_idle(__func__, 1);

	max_backlight = disp_pwm_get_max_backlight(DISP_PWM0);
	if (bl_1024 > max_backlight)
		bl_1024 = max_backlight;

	atomic_set(&g_aal_backlight_notified, bl_1024);

	service_flags = 0;
	if (bl_1024 == 0) {
		if (atomic_read(&g_led_mode) == MT65XX_LED_MODE_CUST_LCM)
			backlight_brightness_set_with_lock(0);
		else
			backlight_brightness_set(0);

		/* set backlight = 0 may be not from AAL, */
		/* we have to let AALService can turn on backlight */
		/* on phone resumption */
		service_flags = AAL_SERVICE_FORCE_UPDATE;
	} else if (atomic_read(&g_aal_is_init_regs_valid) == 0 ||
		atomic_read(&g_aal_force_relay) == 1) {
		/* AAL Service is not running */
		if (atomic_read(&g_led_mode) == MT65XX_LED_MODE_CUST_LCM)
			backlight_brightness_set_with_lock(bl_1024);
		else
			backlight_brightness_set(bl_1024);
	}
	AAL_DBG("led_mode=%d", atomic_read(&g_led_mode));

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	g_aal_hist.backlight = bl_1024;
	g_aal_hist.serviceFlags |= service_flags;
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	if (atomic_read(&g_aal_is_init_regs_valid) == 1) {
		spin_lock_irqsave(&g_aal_irq_en_lock, flags);
		atomic_set(&g_aal_force_enable_irq, 1);
		disp_aal_set_interrupt(1);
		spin_unlock_irqrestore(&g_aal_irq_en_lock, flags);
		/* Backlight latency should be as smaller as possible */
		disp_aal_trigger_refresh(AAL_REFRESH_17MS);
	}
}


static int disp_aal_copy_hist_to_user(struct DISP_AAL_HIST __user *hist)
{
	unsigned long flags;
	int ret = -EFAULT;

	/* We assume only one thread will call this function */

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	memcpy(&g_aal_hist_db, &g_aal_hist, sizeof(struct DISP_AAL_HIST));
#ifdef CONFIG_MTK_DRE30_SUPPORT
	memcpy(&g_aal_dre30_hist_db, &g_aal_dre30_hist,
	    sizeof(struct DISP_DRE30_HIST));
#endif
#ifdef AAL_SUPPORT_KERNEL_API
	g_aal_hist.panel_type = atomic_read(&g_aal_panel_type);
	g_aal_hist.essStrengthIndex = g_aal_ess_level;
	g_aal_hist.ess_enable = g_aal_ess_en;
	g_aal_hist.dre_enable = g_aal_dre_en;
#endif
	g_aal_hist.serviceFlags = 0;
	atomic_set(&g_aal_hist_available, 0);
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	do {
#ifdef CONFIG_MTK_DRE30_SUPPORT
		g_aal_hist_db.dre30_hist = g_aal_init_dre30.dre30_hist_addr;
#endif

		ret = copy_to_user(hist, &g_aal_hist_db,
			sizeof(struct DISP_AAL_HIST));
		if (ret != 0)
			break;

#ifdef CONFIG_MTK_DRE30_SUPPORT
		ret = copy_to_user(
				AAL_U32_PTR(g_aal_init_dre30.dre30_hist_addr),
				&g_aal_dre30_hist_db,
				sizeof(struct DISP_DRE30_HIST));
		if (ret != 0)
			break;
#endif
	} while (0);

	atomic_set(&g_aal_force_enable_irq, 0);

	AAL_DBG("ret = %d", ret);

	return ret;
}

#define CABC_GAINLMT(v0, v1, v2) (((v2) << 20) | ((v1) << 10) | (v0))

static struct DISP_AAL_INITREG g_aal_init_regs;
static struct DISP_AAL_PARAM g_aal_param;

static int disp_aal_set_init_reg(struct DISP_AAL_INITREG __user *user_regs,
	enum DISP_MODULE_ENUM module, void *cmdq)
{
	int ret = -EFAULT;
	struct DISP_AAL_INITREG *init_regs;

	if (disp_aal_is_support() == false)
		return ret;

	init_regs = &g_aal_init_regs;

	ret = copy_from_user(init_regs, user_regs,
		sizeof(struct DISP_AAL_INITREG));
	if (ret == 0) {
		atomic_set(&g_aal_is_init_regs_valid, 1);
		AAL_DBG("Set module(%d) init reg", module);
		ret = disp_aal_write_init_regs(module, cmdq);
	} else {
		AAL_ERR("copy_from_user() failed");
	}

	AAL_DBG("ret = %d", ret);

	return ret;
}

static void disp_aal_dre3_config(void *cmdq,
	const struct DISP_AAL_INITREG *init_regs)
{
#ifdef CONFIG_MTK_DRE30_SUPPORT
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_BLOCK_INFO_00,
		init_regs->act_win_x_end << 13, 0x1FFF << 13);
	DISP_REG_SET(cmdq, DISP_AAL_DRE_BLOCK_INFO_01,
		(init_regs->dre_blk_y_num << 5) | init_regs->dre_blk_x_num);
	DISP_REG_SET(cmdq, DISP_AAL_DRE_BLOCK_INFO_02,
		(init_regs->dre_blk_height << 13) | init_regs->dre_blk_width);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_BLOCK_INFO_04,
		(init_regs->dre_flat_length_slope << 13), 0x3FF << 13);
	DISP_REG_SET(cmdq, DISP_AAL_DRE_CHROMA_HIST_00,
		(init_regs->dre_s_upper << 24) |
		(init_regs->dre_s_lower << 16) |
		(init_regs->dre_y_upper << 8) | init_regs->dre_y_lower);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_CHROMA_HIST_01,
		(init_regs->dre_h_upper << 8) | init_regs->dre_h_lower, 0xFFFF);
	DISP_REG_SET(cmdq, DISP_AAL_DRE_ALPHA_BLEND_00,
		(init_regs->dre_y_alpha_shift_bit << 25) |
		(init_regs->dre_y_alpha_base << 16) |
		(init_regs->dre_x_alpha_shift_bit << 9) |
		init_regs->dre_x_alpha_base);
	DISP_REG_SET(cmdq, DISP_AAL_DRE_BLOCK_INFO_05,
		init_regs->dre_blk_area);
	DISP_REG_SET(cmdq, DISP_AAL_DRE_BLOCK_INFO_06,
		init_regs->dre_blk_area_min);
	DISP_REG_MASK(cmdq, DISP_AAL_SRAM_CFG,
		init_regs->hist_bin_type, 0x1);

#if defined(CONFIG_MACH_MT6779)
	DISP_REG_SET(cmdq, DISP_AAL_DUAL_PIPE_INFO_00,
		(0 << 13) | 0);
	DISP_REG_SET(cmdq, DISP_AAL_DUAL_PIPE_INFO_01,
		((init_regs->dre_blk_x_num-1) << 13) |
		(init_regs->dre_blk_width-1));
#endif

	/* Change to Local DRE version */
	DISP_REG_MASK(cmdq, DISP_AAL_CFG_MAIN, 1 << 4, 1 << 4);

	atomic_or(0x1, &g_aal_change_to_dre30);
#endif				/* CONFIG_MTK_DRE30_SUPPORT */
}

static int disp_aal_write_init_regs(enum DISP_MODULE_ENUM module, void *cmdq)
{
	int ret = -EFAULT;
	const int offset = aal_get_offset(module);

	if (atomic_read(&g_aal_is_init_regs_valid) == 1) {
		struct DISP_AAL_INITREG *init_regs = &g_aal_init_regs;

		int i, j = 0;
		int *gain;

		gain = init_regs->cabc_gainlmt;
		if (g_aal_hw_offset == true) {
#if defined(CONFIG_MACH_MT6799)
			DISP_REG_MASK(cmdq, DISP_AAL_DRE_MAPPING_00_2 + offset,
				(init_regs->dre_map_bypass << 4), 1 << 4);

			for (i = 0; i <= 10; i++) {
				DISP_REG_SET(cmdq,
					DISP_AAL_CABC_GAINLMT_TBL_2(i) + offset,
					CABC_GAINLMT(gain[j], gain[j + 1],
					gain[j + 2]));
				j += 3;
			}
#endif
		} else {
			DISP_REG_MASK(cmdq, DISP_AAL_DRE_MAPPING_00 + offset,
				(init_regs->dre_map_bypass << 4), 1 << 4);

			for (i = 0; i <= 10; i++) {
				DISP_REG_SET(cmdq,
					DISP_AAL_CABC_GAINLMT_TBL(i) + offset,
					CABC_GAINLMT(gain[j], gain[j + 1],
					gain[j + 2]));
				j += 3;
			}
		}

		if (module == AAL0_MODULE_NAMING)
			disp_aal_dre3_config(cmdq, init_regs);

		AAL_DBG("Module(%d) init done", module);
		ret = 0;
	}

	return ret;
}

int disp_aal_set_param(struct DISP_AAL_PARAM __user *param,
	enum DISP_MODULE_ENUM module, void *cmdq)
{
	int ret = -EFAULT;
	int backlight_value = 0;

	/* Not need to protect g_aal_param, */
	/* since only AALService can set AAL parameters. */
	if (copy_from_user(&g_aal_param, param,
	sizeof(struct DISP_AAL_PARAM)) == 0) {
		backlight_value = g_aal_param.FinalBacklight;
		/* set cabc gain zero when detect backlight */
		/* setting equal to zero */
		if (backlight_value == 0)
			g_aal_param.cabc_fltgain_force = 0;
		AAL_DBG("Set module(%d) parameter", module);
		ret = disp_aal_write_param_to_reg(module, cmdq, &g_aal_param);
		atomic_set(&g_aal_allowPartial, g_aal_param.allowPartial);
	}

	if (atomic_read(&g_aal_backlight_notified) == 0)
		backlight_value = 0;

	if (ret == 0)
		ret |= disp_pwm_set_backlight_cmdq(DISP_PWM0,
			backlight_value, cmdq);

	AAL_DBG("(ESS = %d, DRE[0,8] = %d,%d",
		g_aal_param.cabc_fltgain_force, g_aal_param.DREGainFltStatus[0],
		g_aal_param.DREGainFltStatus[8]);
	AAL_DBG("(latency = %d): ret = %d",
		g_aal_param.refreshLatency, ret);

	backlight_brightness_set(backlight_value);

	disp_aal_trigger_refresh(g_aal_param.refreshLatency);

	return ret;
}

void disp_aal_set_lcm_type(unsigned int panel_type)
{
#ifdef AAL_SUPPORT_KERNEL_API
	unsigned long flags;

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	atomic_set(&g_aal_panel_type, panel_type);
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	AAL_DBG("panel_type = %d", panel_type);
#else
	AAL_ERR("not supported");
#endif
}

#define DRE_REG_2(v0, off0, v1, off1) (((v1) << (off1)) | \
	((v0) << (off0)))
#define DRE_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) | (v1 << (off1)) | ((v0) << (off0)))

#ifdef CONFIG_MTK_DRE30_SUPPORT
static int disp_aal_write_dre3_to_reg(enum DISP_MODULE_ENUM module,
	struct cmdqRecStruct *cmdq, const struct DISP_AAL_PARAM *param)
{
	unsigned long flags;

	if (module == AAL0_MODULE_NAMING &&
		atomic_read(&g_aal_change_to_dre30) == 0x3) {
		if (copy_from_user(&g_aal_gain_db,
			      AAL_U32_PTR(param->dre30_gain),
			      sizeof(struct DISP_DRE30_PARAM)) == 0) {

			spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
			memcpy(&g_aal_gain, &g_aal_gain_db,
				sizeof(struct DISP_DRE30_PARAM));
			spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
		}
	}

	return 0;
}
#else
static int disp_aal_write_dre_to_reg(enum DISP_MODULE_ENUM module,
	struct cmdqRecStruct *cmdq, const struct DISP_AAL_PARAM *param)
{
	const int *gain;
	const int offset = aal_get_offset(module);

	gain = param->DREGainFltStatus;
#if defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761)
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(0) + offset,
	    DRE_REG_2(gain[0], 0, gain[1], 14), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(1) + offset,
		DRE_REG_2(gain[2], 0, gain[3], 13), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(2) + offset,
		DRE_REG_2(gain[4], 0, gain[5], 12), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(3) + offset,
		DRE_REG_2(gain[6], 0, gain[7], 12), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(4) + offset,
		DRE_REG_2(gain[8], 0, gain[9], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(5) + offset,
		DRE_REG_2(gain[10], 0, gain[11], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(6) + offset,
		DRE_REG_2(gain[12], 0, gain[13], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(7) + offset,
		DRE_REG_2(gain[14], 0, gain[15], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(8) + offset,
		DRE_REG_3(gain[16], 0, gain[17], 10, gain[18], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(9) + offset,
		DRE_REG_3(gain[19], 0, gain[20], 10, gain[21], 19), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(10) + offset,
		DRE_REG_3(gain[22], 0, gain[23], 9, gain[24], 18), ~0);

	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE_11 + offset,
		DRE_REG_3(gain[25], 0, gain[26], 9, gain[27], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE_12 + offset, gain[28], ~0);
#elif defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(0) + offset,
	    DRE_REG_2(gain[0], 0, gain[1], 14), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(1) + offset,
		DRE_REG_2(gain[2], 0, gain[3], 13), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(2) + offset,
		DRE_REG_2(gain[4], 0, gain[5], 12), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(3) + offset,
		DRE_REG_2(gain[6], 0, gain[7], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(4) + offset,
		DRE_REG_2(gain[8], 0, gain[9], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(5) + offset,
		DRE_REG_2(gain[10], 0, gain[11], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(6) + offset,
		DRE_REG_3(gain[12], 0, gain[13], 11, gain[14], 22), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(7) + offset,
		DRE_REG_3(gain[15], 0, gain[16], 10, gain[17], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(8) + offset,
		DRE_REG_3(gain[18], 0, gain[19], 10, gain[20], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(9) + offset,
		DRE_REG_3(gain[21], 0, gain[22], 9, gain[23], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(10) + offset,
		DRE_REG_3(gain[24], 0, gain[25], 9, gain[26], 18), ~0);
	if (g_aal_dre_offset_separate == true) {
		/* Write dre curve to different register */
#if defined(CONFIG_MACH_MT6799)
		DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE_11 + offset,
		    DRE_REG_2(gain[27], 0, gain[28], 9), ~0);
#endif
	} else {
		/* Write dre curve to different register */
		DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(11) + offset,
		    DRE_REG_2(gain[27], 0, gain[28], 9), ~0);
	}
#else
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(0) + offset,
	    DRE_REG_2(gain[0], 0, gain[1], 12), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(1) + offset,
		DRE_REG_2(gain[2], 0, gain[3], 12), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(2) + offset,
		DRE_REG_2(gain[4], 0, gain[5], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(3) + offset,
		DRE_REG_3(gain[6], 0, gain[7], 11, gain[8], 21), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(4) + offset,
		DRE_REG_3(gain[9], 0, gain[10], 10, gain[11], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(5) + offset,
		DRE_REG_3(gain[12], 0, gain[13], 10, gain[14], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(6) + offset,
		DRE_REG_3(gain[15], 0, gain[16], 10, gain[17], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(7) + offset,
		DRE_REG_3(gain[18], 0, gain[19], 9, gain[20], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(8) + offset,
		DRE_REG_3(gain[21], 0, gain[22], 9, gain[23], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(9) + offset,
		DRE_REG_3(gain[24], 0, gain[25], 9, gain[26], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(10) + offset,
		DRE_REG_2(gain[27], 0, gain[28], 9), ~0);
#endif

	return 0;
}
#endif				/* CONFIG_MTK_DRE30_SUPPORT */
#if defined(CONFIG_MTK_DRE30_SUPPORT) || !defined(NOT_SUPPORT_CABC_HW)
static int disp_aal_write_cabc_to_reg(enum DISP_MODULE_ENUM module,
	struct cmdqRecStruct *cmdq, const struct DISP_AAL_PARAM *param)
{
	int i;
	const int *gain;
	const int offset = aal_get_offset(module);

	DISP_REG_MASK(cmdq, DISP_AAL_CABC_00 + offset, 1 << 31, 1 << 31);
	DISP_REG_MASK(cmdq, DISP_AAL_CABC_02 + offset,
		param->cabc_fltgain_force, 0x3ff);

	gain = param->cabc_gainlmt;
	if (g_aal_hw_offset == true) {
#if defined(CONFIG_MACH_MT6799)
		for (i = 0; i <= 10; i++) {
			DISP_REG_SET(cmdq,
				DISP_AAL_CABC_GAINLMT_TBL_2(i) + offset,
				CABC_GAINLMT(gain[0], gain[1], gain[2]));
			gain += 3;
		}
#endif
	} else {
		for (i = 0; i <= 10; i++) {
			DISP_REG_SET(cmdq,
				DISP_AAL_CABC_GAINLMT_TBL(i) + offset,
				CABC_GAINLMT(gain[0], gain[1], gain[2]));
			gain += 3;
		}
	}

	return 0;
}
#endif				/* not define NOT_SUPPORT_CABC_HW */

static int disp_aal_write_param_to_reg(enum DISP_MODULE_ENUM module,
	struct cmdqRecStruct *cmdq, const struct DISP_AAL_PARAM *param)
{
#ifdef CONFIG_MTK_DRE30_SUPPORT
	disp_aal_write_dre3_to_reg(module, cmdq, param);
	disp_aal_write_cabc_to_reg(module, cmdq, param);
#else
	disp_aal_write_dre_to_reg(module, cmdq, param);
#ifndef NOT_SUPPORT_CABC_HW
	disp_aal_write_cabc_to_reg(module, cmdq, param);
#endif
#endif

	return 0;
}

static int aal_config(enum DISP_MODULE_ENUM module,
	struct disp_ddp_path_config *pConfig, void *cmdq)
{
	bool should_update = (module == AAL0_MODULE_NAMING ? true : false);
#ifdef AAL_HAS_DRE3
	int dre_alg_mode = 0;
#endif

	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return 0;

#if defined(CONFIG_MACH_MT6799)
	if (pConfig->is_dual)
		should_update = true;
#endif

	if (pConfig->dst_dirty) {
		int width, height;
		const int offset = aal_get_offset(module);

		width = pConfig->dst_w;
		height = pConfig->dst_h;

#ifdef DISP_PLATFORM_HAS_SHADOW_REG
		if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
			unsigned long aal_shadow_ctl;
			unsigned int shadow_mode;
#if defined(CONFIG_MACH_MT6799)
			if (g_aal_hw_offset == true)
				aal_shadow_ctl = DISP_AAL_SHADOW_CTL_2;
			else
				aal_shadow_ctl = DISP_AAL_SHADOW_CTL;
#else
			aal_shadow_ctl = DISP_AAL_SHADOW_CTL;
#endif
			shadow_mode =
				disp_helper_get_option(DISP_OPT_SHADOW_MODE);
			if (shadow_mode == 0) {
				/* full shadow mode*/
				DISP_REG_SET(cmdq,
				aal_shadow_ctl + offset, 0x0);
			} else if (shadow_mode == 1) {
				/* force commit */
				DISP_REG_SET(cmdq,
				aal_shadow_ctl + offset, 0x2);
			} else if (shadow_mode == 2) {
				/* bypass shadow */
				DISP_REG_SET(cmdq,
				aal_shadow_ctl + offset, 0x1);
			}
		}
#endif

#if defined(CONFIG_MTK_DRE30_SUPPORT)
		if (g_aal_get_size_available == false) {
			g_aal_size.height = height;
			g_aal_size.width = width;
			g_aal_get_size_available = true;
			wake_up_interruptible(&g_aal_size_wq);
		}
#endif

		DISP_REG_SET(cmdq, DISP_AAL_SIZE + offset,
			(width << 16) | height);
#if defined(CONFIG_MACH_MT6779)
		DISP_REG_SET(cmdq, DISP_AAL_OUTPUT_SIZE + offset,
			(width << 16) | height);
		DISP_REG_SET(cmdq, DISP_AAL_OUTPUT_OFFSET + offset,
			(0 << 16) | 0);
#endif
#ifdef AAL_HAS_DRE3
		if (atomic_read(&g_aal_change_to_dre30) & 0x1)
			dre_alg_mode = 1;
		if (module == AAL0_MODULE_NAMING) {
			DISP_REG_MASK(cmdq,
				DISP_AAL_CFG_MAIN + aal_get_offset(module),
				dre_alg_mode << 4, 1 << 4);
		}
#endif
		if (atomic_read(&g_aal_force_relay) == 1) {
			/* Set relay mode */
			DISP_REG_MASK(cmdq, DISP_AAL_CFG + offset, 1, 0x1);
		} else {
			/* Disable relay mode */
			DISP_REG_MASK(cmdq, DISP_AAL_CFG + offset, 0, 0x1);
		}

		disp_aal_init(module, width, height, cmdq);

		DISP_REG_MASK(cmdq, DISP_AAL_EN + offset, 0x1, 0x1);

		AAL_DBG("module:(%d), AAL_CFG = 0x%x, AAL_SIZE = 0x%x(%d, %d)",
			module, DISP_REG_GET(DISP_AAL_CFG + offset),
			DISP_REG_GET(DISP_AAL_SIZE + offset), width, height);
	}

	if ((pConfig->ovl_dirty || pConfig->rdma_dirty) &&
		should_update == true)
		disp_aal_notify_frame_dirty(module);

	return 0;
}


/*****************************************************************************
 * AAL Backup / Restore function
 *****************************************************************************/
#if defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define DRE_FLT_NUM	(13)
#elif defined(CONFIG_MACH_MT6799)
#define DRE_FLT_NUM	(12)
#else
#define DRE_FLT_NUM	(11)
#endif
#define LEGACY_DRE_FLT_NUM_MAX (11)

struct aal_backup { /* structure for backup AAL register value */
	unsigned int DRE_MAPPING;
	unsigned int DRE_FLT_FORCE[DRE_FLT_NUM];
	unsigned int CABC_00;
	unsigned int CABC_02;
	unsigned int CABC_GAINLMT[11];
#ifdef CONFIG_MTK_DRE30_SUPPORT
	unsigned int DRE_BLOCK_INFO_00;
	unsigned int DRE_BLOCK_INFO_01;
	unsigned int DRE_BLOCK_INFO_02;
	unsigned int DRE_BLOCK_INFO_04;
	unsigned int DRE_BLOCK_INFO_05;
	unsigned int DRE_BLOCK_INFO_06;
	unsigned int DRE_CHROMA_HIST_00;
	unsigned int DRE_CHROMA_HIST_01;
	unsigned int DRE_ALPHA_BLEND_00;
	unsigned int SRAM_CFG;
#if defined(CONFIG_MACH_MT6779)
	unsigned int DUAL_PIPE_INFO_00;
	unsigned int DUAL_PIPE_INFO_01;
#endif
#endif
};
struct aal_backup g_aal_backup;
static int g_aal_io_mask;

static void ddp_aal_dre3_backup(void)
{
#ifdef CONFIG_MTK_DRE30_SUPPORT
	g_aal_backup.DRE_BLOCK_INFO_00 =
		DISP_REG_GET(DISP_AAL_DRE_BLOCK_INFO_00);
	g_aal_backup.DRE_BLOCK_INFO_01 =
		DISP_REG_GET(DISP_AAL_DRE_BLOCK_INFO_01);
	g_aal_backup.DRE_BLOCK_INFO_02 =
		DISP_REG_GET(DISP_AAL_DRE_BLOCK_INFO_02);
	g_aal_backup.DRE_BLOCK_INFO_04 =
		DISP_REG_GET(DISP_AAL_DRE_BLOCK_INFO_04);
	g_aal_backup.DRE_CHROMA_HIST_00 =
		DISP_REG_GET(DISP_AAL_DRE_CHROMA_HIST_00);
	g_aal_backup.DRE_CHROMA_HIST_01 =
		DISP_REG_GET(DISP_AAL_DRE_CHROMA_HIST_01);
	g_aal_backup.DRE_ALPHA_BLEND_00 =
		DISP_REG_GET(DISP_AAL_DRE_ALPHA_BLEND_00);
	g_aal_backup.DRE_BLOCK_INFO_05 =
		DISP_REG_GET(DISP_AAL_DRE_BLOCK_INFO_05);
	g_aal_backup.DRE_BLOCK_INFO_06 =
		DISP_REG_GET(DISP_AAL_DRE_BLOCK_INFO_06);
	g_aal_backup.SRAM_CFG = DISP_REG_GET(DISP_AAL_SRAM_CFG);
#if defined(CONFIG_MACH_MT6779)
	g_aal_backup.DUAL_PIPE_INFO_00 =
		DISP_REG_GET(DISP_AAL_DUAL_PIPE_INFO_00);
	g_aal_backup.DUAL_PIPE_INFO_01 =
		DISP_REG_GET(DISP_AAL_DUAL_PIPE_INFO_01);
#endif
#endif				/* CONFIG_MTK_DRE30_SUPPORT */
}

static void ddp_aal_dre_backup(void)
{
	int i;

	if (g_aal_hw_offset == true) {
#if defined(CONFIG_MACH_MT6799)
		g_aal_backup.DRE_MAPPING =
		DISP_REG_GET(DISP_AAL_DRE_MAPPING_00_2);
#endif
	} else {
		g_aal_backup.DRE_MAPPING =
			DISP_REG_GET(DISP_AAL_DRE_MAPPING_00);
	}

	for (i = 0; i < LEGACY_DRE_FLT_NUM_MAX; i++)
		g_aal_backup.DRE_FLT_FORCE[i] =
		DISP_REG_GET(DISP_AAL_DRE_FLT_FORCE(i));

	if (g_aal_dre_offset_separate == true) {
#if defined(CONFIG_MACH_MT6799)
		g_aal_backup.DRE_FLT_FORCE[11] =
		    DISP_REG_GET(DISP_AAL_DRE_FLT_FORCE_11);

		return;
#endif
	}

#if defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761)
	g_aal_backup.DRE_FLT_FORCE[11] =
		DISP_REG_GET(DISP_AAL_DRE_FLT_FORCE_11);
	g_aal_backup.DRE_FLT_FORCE[12] =
		DISP_REG_GET(DISP_AAL_DRE_FLT_FORCE_12);
#elif defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT3967) || \
		defined(CONFIG_MACH_MT6779)
	g_aal_backup.DRE_FLT_FORCE[11] =
		DISP_REG_GET(DISP_AAL_DRE_FLT_FORCE(11));
#endif

}

static void ddp_aal_cabc_backup(void)
{
#if defined(CONFIG_MTK_DRE30_SUPPORT) || !defined(NOT_SUPPORT_CABC_HW)
	int i;

	g_aal_backup.CABC_00 = DISP_REG_GET(DISP_AAL_CABC_00);
	g_aal_backup.CABC_02 = DISP_REG_GET(DISP_AAL_CABC_02);

	if (g_aal_hw_offset == true) {
#if defined(CONFIG_MACH_MT6799)
		for (i = 0; i <= 10; i++)
			g_aal_backup.CABC_GAINLMT[i] =
			    DISP_REG_GET(DISP_AAL_CABC_GAINLMT_TBL_2(i));
#endif
	} else {
		for (i = 0; i <= 10; i++)
			g_aal_backup.CABC_GAINLMT[i] =
			    DISP_REG_GET(DISP_AAL_CABC_GAINLMT_TBL(i));
	}
#endif				/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_backup(void)
{
	ddp_aal_cabc_backup();
	ddp_aal_dre_backup();
	ddp_aal_dre3_backup();

	atomic_set(&g_aal_initialed, 1);
}

static void ddp_aal_dre3_restore(void *cmq_handle)
{
#ifdef CONFIG_MTK_DRE30_SUPPORT
	unsigned long flags;

	DISP_REG_MASK(cmq_handle, DISP_AAL_DRE_BLOCK_INFO_00,
		g_aal_backup.DRE_BLOCK_INFO_00 & (0x1FFF << 13), 0x1FFF << 13);
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_BLOCK_INFO_01,
		g_aal_backup.DRE_BLOCK_INFO_01);
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_BLOCK_INFO_02,
		g_aal_backup.DRE_BLOCK_INFO_02);
	DISP_REG_MASK(cmq_handle, DISP_AAL_DRE_BLOCK_INFO_04,
		g_aal_backup.DRE_BLOCK_INFO_04 & (0x3FF << 13), 0x3FF << 13);
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_CHROMA_HIST_00,
		g_aal_backup.DRE_CHROMA_HIST_00);
	DISP_REG_MASK(cmq_handle, DISP_AAL_DRE_CHROMA_HIST_01,
		g_aal_backup.DRE_CHROMA_HIST_01 & 0xFFFF, 0xFFFF);
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_ALPHA_BLEND_00,
		g_aal_backup.DRE_ALPHA_BLEND_00);
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_BLOCK_INFO_05,
		g_aal_backup.DRE_BLOCK_INFO_05);
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_BLOCK_INFO_06,
		g_aal_backup.DRE_BLOCK_INFO_06);
	DISP_REG_MASK(cmq_handle, DISP_AAL_SRAM_CFG,
		g_aal_backup.SRAM_CFG, 0x1);

#if defined(CONFIG_MACH_MT6779)
	DISP_REG_SET(cmq_handle, DISP_AAL_DUAL_PIPE_INFO_00,
		g_aal_backup.DUAL_PIPE_INFO_00);
	DISP_REG_SET(cmq_handle, DISP_AAL_DUAL_PIPE_INFO_01,
		g_aal_backup.DUAL_PIPE_INFO_01);
#endif

	spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
	ddp_aal_dre3_write_curve_full(cmq_handle);
	spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
#endif				/* CONFIG_MTK_DRE30_SUPPORT */
}

static void ddp_aal_dre_restore(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
	int i;
	const int offset = aal_get_offset(module);

	if (g_aal_hw_offset == true) {
#if defined(CONFIG_MACH_MT6799)
		DISP_REG_SET(cmq_handle, DISP_AAL_DRE_MAPPING_00_2 + offset,
		    g_aal_backup.DRE_MAPPING);
#endif
	} else {
		DISP_REG_SET(cmq_handle, DISP_AAL_DRE_MAPPING_00 + offset,
	    g_aal_backup.DRE_MAPPING);
	}

	for (i = 0; i < LEGACY_DRE_FLT_NUM_MAX; i++)
		DISP_REG_SET(cmq_handle, DISP_AAL_DRE_FLT_FORCE(i) + offset,
		    g_aal_backup.DRE_FLT_FORCE[i]);

	if (g_aal_dre_offset_separate == true) {
#if defined(CONFIG_MACH_MT6799)
		DISP_REG_SET(cmq_handle, DISP_AAL_DRE_FLT_FORCE_11 + offset,
		    g_aal_backup.DRE_FLT_FORCE[11]);

		return;
#endif
	}

#if defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761)
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_FLT_FORCE_11 + offset,
	    g_aal_backup.DRE_FLT_FORCE[11]);
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_FLT_FORCE_12 + offset,
		g_aal_backup.DRE_FLT_FORCE[12]);
#elif defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
	DISP_REG_SET(cmq_handle, DISP_AAL_DRE_FLT_FORCE(11) + offset,
	    g_aal_backup.DRE_FLT_FORCE[11]);
#endif
}

static void ddp_aal_cabc_restore(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
#if defined(CONFIG_MTK_DRE30_SUPPORT) || !defined(NOT_SUPPORT_CABC_HW)
	int i;
	const int offset = aal_get_offset(module);

	DISP_REG_SET(cmq_handle, DISP_AAL_CABC_00 + offset,
		g_aal_backup.CABC_00);
	DISP_REG_SET(cmq_handle, DISP_AAL_CABC_02 + offset,
		g_aal_backup.CABC_02);

	if (g_aal_hw_offset == true) {
#if defined(CONFIG_MACH_MT6799)
		for (i = 0; i <= 10; i++)
			DISP_REG_SET(cmq_handle,
			DISP_AAL_CABC_GAINLMT_TBL_2(i) + offset,
			    g_aal_backup.CABC_GAINLMT[i]);
#endif
	} else {
		for (i = 0; i <= 10; i++)
			DISP_REG_SET(cmq_handle,
			DISP_AAL_CABC_GAINLMT_TBL(i) + offset,
			    g_aal_backup.CABC_GAINLMT[i]);
	}
#endif				/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_restore(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
	if (atomic_read(&g_aal_initialed) != 1)
		return;

	AAL_DBG("module(%d)", module);

	ddp_aal_cabc_restore(module, cmq_handle);
	ddp_aal_dre_restore(module, cmq_handle);

	if (module == AAL0_MODULE_NAMING)
		ddp_aal_dre3_restore(cmq_handle);

}

static int aal_clock_on(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return 0;

#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS)
	/* aal is DCM , do nothing */
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
#else
#ifdef ENABLE_CLK_MGR
	if (module == AAL0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
		enable_clock(MT_CG_DISP0_DISP_AAL, "aal");
#else
		ddp_clk_enable(AAL0_CLK_NAMING);
		AAL_DBG("CG 0x%x", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#endif		/* CONFIG_MTK_CLKMGR */
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_AAL1) {
#ifndef CONFIG_MTK_CLKMGR
		ddp_clk_enable(DISP0_DISP_AAL1);
#endif		/* not define CONFIG_MTK_CLKMGR */
	}
#endif
#endif		/* ENABLE_CLK_MGR */
#endif

	atomic_set(&g_aal_is_clock_on[index_of_aal(module)], 1);
	if (module != AAL0_MODULE_NAMING && 1 ==
	atomic_read(&g_aal_is_clock_on[index_of_aal(AAL0_MODULE_NAMING)])) {
		/* backup sub-AAL register from main-AAL */
		ddp_aal_backup();
	}

	ddp_aal_restore(module, cmq_handle);

	return 0;
}

static int aal_clock_off(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return 0;

	if (module == AAL0_MODULE_NAMING)
		ddp_aal_backup();

	AAL_DBG("clock off");

	disp_aal_clear_irq(module, true, false);
#ifdef CONFIG_MTK_DRE30_SUPPORT
	atomic_set(&g_aal_force_hist_apb, 0);
	atomic_set(&g_aal_dre_halt, 0);
#endif			/* CONFIG_MTK_DRE30_SUPPORT */

#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS)
	/* aal is DCM , do nothing */
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
#else
#ifdef ENABLE_CLK_MGR
	if (module == AAL0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
		disable_clock(MT_CG_DISP0_DISP_AAL, "aal");
#else
		ddp_clk_disable(AAL0_CLK_NAMING);
#endif		/* CONFIG_MTK_CLKMGR */
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_AAL1) {
#ifndef CONFIG_MTK_CLKMGR
		ddp_clk_disable(DISP0_DISP_AAL1);
#endif		/* not define CONFIG_MTK_CLKMGR */
	}
#endif
#endif		/* ENABLE_CLK_MGR */
#endif

	atomic_set(&g_aal_is_clock_on[index_of_aal(module)], 0);
#if !defined(CONFIG_MTK_DRE30_SUPPORT)
	atomic_set(&g_aal_reset_count, 1);
#endif
	return 0;
}

static int aal_init(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return 0;

#if !defined(CONFIG_MACH_MT6759) && !defined(CONFIG_MACH_MT6739)
	aal_clock_on(module, cmq_handle);
#endif
#if defined(CONFIG_MACH_MT6799)
	if (mt_get_chip_sw_ver() >= CHIP_SW_VER_02)
		g_aal_hw_offset = true;
	else
		g_aal_dre_offset_separate = true;
#endif

#ifdef CONFIG_MTK_DRE30_SUPPORT
	if (module == AAL0_MODULE_NAMING)
		if (atomic_cmpxchg(&g_aal_dre_hw_init, 0, 1) == 0)
			disp_aal_dre3_init(NULL);
#endif

	return 0;
}

static int aal_deinit(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
#if !defined(CONFIG_MACH_MT6759)
	aal_clock_off(module, cmq_handle);
#endif
	return 0;
}

static int aal_set_listener(enum DISP_MODULE_ENUM module,
	ddp_module_notify notify)
{
	g_ddp_notify = notify;
	return 0;
}

int aal_bypass(enum DISP_MODULE_ENUM module, int bypass)
{
	int relay = 0;

	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return 0;

	if (bypass)
		relay = 1;

	DISP_REG_MASK(NULL, DISP_AAL_CFG + aal_get_offset(module), relay, 0x1);

	AAL_DBG("Module(%d) (bypass = %d)", module, bypass);

	return 0;
}

int aal_is_partial_support(void)
{
	int allowPartial;

	if (disp_aal_is_support() == true)
		allowPartial = atomic_read(&g_aal_allowPartial);
	else
		allowPartial = 1;

	AAL_DBG("%d", allowPartial);

	return allowPartial;
}

int aal_request_partial_support(int partial)
{
	unsigned long flags;

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	g_aal_hist.requestPartial = partial;
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	AAL_DBG("%d", partial);

	return 0;
}

#ifdef AAL_SUPPORT_PARTIAL_UPDATE
static int _aal_partial_update(enum DISP_MODULE_ENUM module, void *arg,
	void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;

	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return 0;

	DISP_REG_SET(cmdq, DISP_AAL_SIZE + aal_get_offset(module),
		(width << 16) | height);
#if defined(CONFIG_MACH_MT6779)
	DISP_REG_SET(cmdq, DISP_AAL_OUTPUT_SIZE + aal_get_offset(module),
		(width << 16) | height);
#endif
	AAL_DBG("Module(%d) w=%d h=%d", module, width, height);
	return 0;
}

void disp_aal_set_ess_level_impl(int level, int need_kick)
{
#ifdef AAL_SUPPORT_KERNEL_API
	unsigned long flags;
	int level_command = 0;

	spin_lock_irqsave(&g_aal_hist_lock, flags);

	g_aal_ess_level_cmd_id += 1;
	g_aal_ess_level_cmd_id = g_aal_ess_level_cmd_id % 64;
	level_command = AAL_CONTROL_CMD(g_aal_ess_level_cmd_id, level);

	g_aal_ess_level = level_command;

	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	disp_aal_exit_idle(__func__, need_kick);
	disp_aal_set_interrupt(1);
	disp_aal_trigger_refresh(AAL_REFRESH_17MS);
	AAL_DBG("level = %d (cmd = 0x%x)", level, level_command);
#else
	AAL_ERR("not supported");
#endif
}

void disp_aal_set_ess_level(int level)
{
	disp_aal_set_ess_level_impl(level, 1);
}

void disp_aal_set_ess_en(int enable)
{
#ifdef AAL_SUPPORT_KERNEL_API
	unsigned long flags;
	int enable_command = 0;
	int level_command = 0;

	spin_lock_irqsave(&g_aal_hist_lock, flags);

	g_aal_ess_en_cmd_id += 1;
	g_aal_ess_en_cmd_id = g_aal_ess_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(g_aal_ess_en_cmd_id, enable);

	g_aal_ess_en = enable_command;

	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	disp_aal_exit_idle(__func__, 1);
	disp_aal_set_interrupt(1);
	disp_aal_trigger_refresh(AAL_REFRESH_17MS);
	AAL_DBG("en = %d (cmd = 0x%x) level = 0x%08x (cmd = 0x%x)",
		enable, enable_command, ESS_LEVEL_BY_CUSTOM_LIB, level_command);
#else
	AAL_ERR("not supported");
#endif
}

void disp_aal_set_dre_en(int enable)
{
#ifdef AAL_SUPPORT_KERNEL_API
	unsigned long flags;
	int enable_command = 0;

	spin_lock_irqsave(&g_aal_hist_lock, flags);

	g_aal_dre_en_cmd_id += 1;
	g_aal_dre_en_cmd_id = g_aal_dre_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(g_aal_dre_en_cmd_id, enable);

	g_aal_dre_en = enable_command;

	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	disp_aal_exit_idle(__func__, 1);
	disp_aal_set_interrupt(1);
	disp_aal_trigger_refresh(AAL_REFRESH_17MS);
	AAL_DBG("en = %d (cmd = 0x%x)", enable, enable_command);
#else
	AAL_ERR("not supported");
#endif
}

static int aal_ioctl(enum DISP_MODULE_ENUM module, void *handle,
		enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return 0;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_aal_partial_update(module, params, handle);
		ret = 0;
	}

	return ret;
}
#endif

static int aal_io(enum DISP_MODULE_ENUM module, unsigned int msg,
		unsigned long arg, void *cmdq)
{
	int ret = 0;
	unsigned long flags;
	int enabled;

	if (g_aal_io_mask != 0) {
		AAL_DBG("aal_ioctl masked");
		return ret;
	}

	if (disp_aal_check_module(module, __func__, __LINE__) == false)
		return ret;

	switch (msg) {
	case DISP_IOCTL_AAL_EVENTCTL:
		if (copy_from_user(&enabled, (void *)arg,
			sizeof(enabled))) {
			AAL_ERR("DISP_IOCTL_AAL_EVENTCTL: failed");
			return -EFAULT;
		}

		spin_lock_irqsave(&g_aal_irq_en_lock, flags);
		if (atomic_read(&g_aal_force_enable_irq) == 1) {
			if (enabled == 0)
				AAL_NOTICE("force enable aal irq 0-->1");
			enabled = 1;
		}

		disp_aal_set_interrupt_by_module(module, enabled);
		spin_unlock_irqrestore(&g_aal_irq_en_lock, flags);

		if (enabled)
			disp_aal_trigger_refresh(AAL_REFRESH_33MS);

		break;

	case DISP_IOCTL_AAL_GET_HIST:
		disp_aal_wait_hist(60);

		if (disp_aal_copy_hist_to_user((struct DISP_AAL_HIST *) arg)
			< 0) {
			AAL_ERR("DISP_IOCTL_AAL_GET_HIST: failed");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_AAL_INIT_REG:
		if (disp_aal_set_init_reg((struct DISP_AAL_INITREG *) arg,
			module, cmdq) < 0) {
			AAL_ERR("DISP_IOCTL_AAL_INIT_REG: failed");
			return -EFAULT;
		}
		break;

#if defined(CONFIG_MTK_DRE30_SUPPORT)
	case DISP_IOCTL_AAL_INIT_DRE30:
		if (disp_aal_set_init_dre3((struct DISP_DRE30_INIT *) arg)
			< 0) {
			AAL_ERR("DISP_IOCTL_AAL_INIT_DRE30: failed");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_AAL_GET_SIZE:
		disp_aal_wait_size(60);

		if (disp_aal_copy_size_to_user((struct DISP_AAL_DISPLAY_SIZE *)
			arg) < 0) {
			AAL_ERR("DISP_IOCTL_AAL_GET_SIZE: failed");
			return -EFAULT;
		}
		break;
#endif
	case DISP_IOCTL_AAL_SET_PARAM:
		if (disp_aal_set_param((struct DISP_AAL_PARAM *) arg, module,
			cmdq) < 0) {
			AAL_ERR("DISP_IOCTL_AAL_SET_PARAM: failed");
			return -EFAULT;
		}
		break;
#ifdef AMS_AAL_SUPPORT_KERNEL_API
	case DISP_IOCTL_SET_SMARTBACKLIGHT:
		{
			int smart_ess_level;

			if (copy_from_user(&smart_ess_level, (void *)arg,
				sizeof(smart_ess_level))) {
				AAL_ERR("SET_SMARTBACKLIGHT: failed");
				return -EFAULT;
			}

			disp_aal_set_ess_level_impl(smart_ess_level, 0);
			break;
		}
#endif
	default:
		AAL_ERR("ioctl not supported failed\n");
		return -EFAULT;
	}

	return ret;
}

struct DDP_MODULE_DRIVER ddp_driver_aal = {
	.init = aal_init,
	.deinit = aal_deinit,
	.config = aal_config,
	.start = NULL,
	.trigger = NULL,
	.stop = NULL,
	.reset = NULL,
	.power_on = aal_clock_on,
	.power_off = aal_clock_off,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = NULL,
	.bypass = aal_bypass,
	.build_cmdq = NULL,
	.set_lcm_utils = NULL,
	.set_listener = aal_set_listener,
	.cmd = aal_io,
#ifdef AAL_SUPPORT_PARTIAL_UPDATE
	.ioctl = aal_ioctl,
#endif
};


/* ---------------------------------------------------------------------- */
/* Test code */
/* Will not be linked in user build. */
/* ---------------------------------------------------------------------- */

#define AAL_TLOG(fmt, arg...) pr_info("[AAL] %s: " fmt "\n", __func__, ##arg)

static void aal_test_en(enum DISP_MODULE_ENUM module, const char *cmd)
{
	int en = ((cmd[0] == '0') ? 0 : 1);
	const int offset = aal_get_offset(module);

	DISP_REG_SET(NULL, DISP_AAL_EN + offset, en);
	AAL_TLOG("EN = %d, read = %d", en, DISP_REG_GET(DISP_AAL_EN + offset));
}

static void aal_dump_block_histogram(void)
{
#ifdef CONFIG_MTK_DRE30_SUPPORT
	unsigned long flags;
	int i;
	int dre_blk_x_num;
	int dre_blk_y_num;
	struct DISP_DRE30_HIST *dre30_hist;

	dre30_hist = kmalloc(sizeof(struct DISP_DRE30_HIST), GFP_KERNEL);
	if (dre30_hist == NULL)
		return;

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	dre_blk_x_num = g_aal_dre30_hist.dre_blk_x_num;
	dre_blk_y_num = g_aal_dre30_hist.dre_blk_y_num;
	memcpy(dre30_hist, &g_aal_dre30_hist, sizeof(struct DISP_DRE30_HIST));
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	AAL_TLOG("Hist block number [y, x] [%d, %d]",
		dre_blk_y_num, dre_blk_x_num);

	for (i = 0; i + 4 <= AAL_DRE30_HIST_REGISTER_NUM; i += 4) {

		AAL_TLOG("hist[%d..%d] = 0x%08x 0x%08x 0x%08x 0x%08x",
			i, i+3, dre30_hist->dre_hist[i],
			dre30_hist->dre_hist[i+1], dre30_hist->dre_hist[i+2],
			dre30_hist->dre_hist[i+3]);
	}
	for (; i <= AAL_DRE30_HIST_REGISTER_NUM; i++) {
		/* print last histogram information */
		AAL_TLOG("hist[%d] = 0x%08x", i, dre30_hist->dre_hist[i]);
	}

	kfree(dre30_hist);
#endif
}

static void aal_dump_histogram(void)
{
	unsigned long flags;
	int i;
	struct DISP_AAL_HIST *hist;

	aal_dump_block_histogram();

	hist = kmalloc(sizeof(struct DISP_AAL_HIST), GFP_KERNEL);
	if (hist != NULL) {
		spin_lock_irqsave(&g_aal_hist_lock, flags);
		memcpy(hist, &g_aal_hist, sizeof(struct DISP_AAL_HIST));
		spin_unlock_irqrestore(&g_aal_hist_lock, flags);

		for (i = 0; i + 8 < AAL_HIST_BIN; i += 8) {
			AAL_TLOG("Hist[%d..%d]=%6d %6d %6d %6d %6d %6d %6d %6d",
			i, i + 7, hist->maxHist[i], hist->maxHist[i + 1],
			hist->maxHist[i + 2], hist->maxHist[i + 3],
			hist->maxHist[i + 4], hist->maxHist[i + 5],
			hist->maxHist[i + 6], hist->maxHist[i + 7]);
		}
		for (; i < AAL_HIST_BIN; i++)
			AAL_TLOG("Hist[%d] = %6d", i, hist->maxHist[i]);
#ifdef AAL_HAS_YHIST
		for (i = 0; i + 8 < AAL_HIST_BIN; i += 8) {
			AAL_TLOG("yHis[%d..%d]=%6d %6d %6d %6d %6d %6d %6d %6d",
			i, i + 7, hist->yHist[i], hist->yHist[i + 1],
			hist->yHist[i + 2], hist->yHist[i + 3],
			hist->yHist[i + 4], hist->yHist[i + 5],
			hist->yHist[i + 6], hist->yHist[i + 7]);
		}
		for (; i < AAL_HIST_BIN; i++)
			AAL_TLOG("yHist[%d] = %6d", i, hist->yHist[i]);
#endif

		kfree(hist);
	}
}

static void aal_dump_dre(void)
{
#ifdef CONFIG_MTK_DRE30_SUPPORT
	unsigned long flags;
	unsigned int read_value[4];
	int i, j;

	spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
	AAL_TLOG("========== dump dre gain in SW buffer ==========");
	for (i = 0; i + 4 <= AAL_DRE30_GAIN_REGISTER_NUM; i += 4) {
		for (j = 0; j < 4; j++)
			read_value[j] = g_aal_gain.dre30_gain[i+j];

		AAL_TLOG("dre gain[%d..%d] = 0x%08x 0x%08x 0x%08x 0x%08x",
			i, i+3, read_value[0], read_value[1],
			read_value[2], read_value[3]);
	}
	for (; i < AAL_DRE30_GAIN_REGISTER_NUM; i++) {
		read_value[0] = g_aal_gain.dre30_gain[i];

		AAL_TLOG("dre gain[%d] = 0x%08x", i, read_value[0]);
	}
	AAL_TLOG("===== Load from HW start (frame dirty is needed) =====");
	for (i = AAL_DRE_GAIN_START; i + (4*4) <= AAL_DRE_GAIN_END;
		i += (4*4)) {
		for (j = 0; j < 4; j++) {
			if (disp_aal_sram_read(i + (4*j), &read_value[j])
			!= true)
				break;
		}

		AAL_TLOG("dre gain sram[%d..%d] = 0x%08x 0x%08x 0x%08x 0x%08x",
			i, i+3, read_value[0], read_value[1],
			read_value[2], read_value[3]);
	}
	for (; i <= AAL_DRE_GAIN_END; i += 4) {
		if (disp_aal_sram_read(i, &read_value[0]) != true)
			break;
		AAL_TLOG("dre gain sram[%d] = 0x%08x", i, read_value[0]);
	}
	AAL_TLOG("========== dump dre gain End ==========");
	spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
#endif
}

static void aal_test_ink(enum DISP_MODULE_ENUM module, const char *cmd)
{
	int en = (cmd[0] - '0');
	const unsigned long cabc_04 = DISP_AAL_CABC_00 + 0x4 * 4;
	const int offset = aal_get_offset(module);

	switch (en) {
	case 1:
		DISP_REG_SET(NULL, cabc_04 + offset, (1 << 31) | (511 << 18));
		break;
	case 2:
		DISP_REG_SET(NULL, cabc_04 + offset, (1 << 31) | (511 << 9));
		break;
	case 3:
		DISP_REG_SET(NULL, cabc_04 + offset, (1 << 31) | (511 << 0));
		break;
	case 4:
		DISP_REG_SET(NULL, cabc_04 + offset, (1 << 31) | (511 << 18) |
			(511 << 9) | 511);
		break;
	default:
		DISP_REG_SET(NULL, cabc_04 + offset, 0);
		break;
	}
}


static void aal_ut_cmd(const char *cmd)
{
	if (strncmp(cmd, "reset", 5) == 0) {
		atomic_set(&g_aal_initialed, 0);
		memset(&g_aal_backup, 0, sizeof(struct aal_backup));
		AAL_DBG("ut:reset");
	} else if (strncmp(cmd, "ioctl_on", 8) == 0) {
		g_aal_io_mask = 0;
		AAL_DBG("ut:ioctl on");
	} else if (strncmp(cmd, "ioctl_off", 9) == 0) {
		g_aal_io_mask = 1;
		AAL_DBG("ut:ioctl off");
	}
}

void aal_test(const char *cmd, char *debug_output)
{
	enum DISP_MODULE_ENUM module = AAL0_MODULE_NAMING;
	int i;
	int config_module_num = 1;
#if defined(CONFIG_MACH_MT6799)
	if (primary_display_get_pipe_status() == DUAL_PIPE)
		config_module_num = AAL_TOTAL_MODULE_NUM;
#endif
	debug_output[0] = '\0';
	AAL_TLOG("test cmd(%s)", cmd);

	if (strncmp(cmd, "en:", 3) == 0) {
		for (i = 0; i < config_module_num; i++) {
			module += i;
			aal_test_en(module, cmd + 3);
		}
	} else if (strncmp(cmd, "histogram", 5) == 0) {
		aal_dump_histogram();
	} else if (strncmp(cmd, "ink:", 4) == 0) {
		for (i = 0; i < config_module_num; i++) {
			module += i;
			aal_test_ink(module, cmd + 4);
		}
		disp_aal_trigger_refresh(AAL_REFRESH_17MS);
	} else if (strncmp(cmd, "bypass:", 7) == 0) {
		int bypass = (cmd[7] == '1');

		atomic_set(&g_aal_force_relay, bypass);
		disp_aal_trigger_refresh(AAL_REFRESH_17MS);
	} else if (strncmp(cmd, "getBypass:", 10) == 0) {
		sprintf(debug_output, "AAL HW Relay: %d\n",
			atomic_read(&g_aal_force_relay));
	} else if (strncmp(cmd, "ut:", 3) == 0) { /* debug command for UT */
		aal_ut_cmd(cmd + 3);
	} else if (strncmp(cmd, "dre", 3) == 0) {
		aal_dump_dre();
#ifdef AAL_SUPPORT_KERNEL_API
	} else if (strncmp(cmd, "lcm_type:", 9) == 0) {
		unsigned int panel_type = cmd[9] - '0';

		disp_aal_set_lcm_type(panel_type);
	} else if (strncmp(cmd, "set_ess_level:", 14) == 0) {
		int level = cmd[14] - '0';

		disp_aal_set_ess_level(level);
	} else if (strncmp(cmd, "set_ess_en:", 11) == 0) {
		int en = (cmd[11] == '1') ? 1 : 0;

		disp_aal_set_ess_en(en);
	} else if (strncmp(cmd, "set_dre_en:", 11) == 0) {
		int en = (cmd[11] == '1') ? 1 : 0;

		disp_aal_set_dre_en(en);
#endif
	}
}


#include <linux/kernel.h>
#include <mach/ext_wd_drv.h>
#include <mach/local_wd_drv.h>
#include <mach/wd_api.h>
#include <linux/smp.h>

extern void wk_cpu_update_bit_flag(int cpu, int plug_status);
extern unsigned int get_check_bit(void);
extern unsigned int get_kick_bit(void);


static int wd_cpu_hot_plug_on_notify(int cpu);
static int wd_cpu_hot_plug_off_notify(int cpu);
static int spmwdt_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode);
static int thermal_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode);
static int confirm_hwreboot(void);
static void resume_notify(void);
static void suspend_notify(void);
static int mtk_wk_wdt_config(enum ext_wdt_mode mode, int timeout_val);
static unsigned int wd_get_check_bit(void);
static unsigned int wd_get_kick_bit(void);
static int disable_all_wd(void);
static int disable_ext(void);
static int disable_local(void);
static int wd_sw_reset(int type);
static int wd_restart(enum wd_restart_type type);
static int set_mode(enum ext_wdt_mode mode);
static int wd_dram_reserved_mode(bool enabled);
static int thermal_direct_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode);

static struct wd_api g_wd_api_obj = {
	.ready = 1,
	.wd_cpu_hot_plug_on_notify = wd_cpu_hot_plug_on_notify,	/* for cpu hot plug */
	.wd_cpu_hot_plug_off_notify = wd_cpu_hot_plug_off_notify,	/* for cpu hot plug */
	.wd_spmwdt_mode_config = spmwdt_mode_config,
	.wd_thermal_mode_config = thermal_mode_config,
	.wd_sw_reset = wd_sw_reset,
	.wd_restart = wd_restart,
	.wd_config = mtk_wk_wdt_config,
	.wd_set_mode = set_mode,
	.wd_aee_confirm_hwreboot = confirm_hwreboot,
	.wd_disable_ext = disable_ext,
	.wd_disable_local = disable_local,
	.wd_suspend_notify = suspend_notify,
	.wd_resume_notify = resume_notify,
	.wd_disable_all = disable_all_wd,
	.wd_get_check_bit = wd_get_check_bit,
	.wd_get_kick_bit = wd_get_kick_bit,
	.wd_dram_reserved_mode = wd_dram_reserved_mode,
	.wd_thermal_direct_mode_config = thermal_direct_mode_config,
};

/* struct wd_private_api  *g_wd_private_api_obj; */



/* apiimplimentation */
#ifdef CONFIG_MTK_WD_KICKER

static unsigned int wd_get_check_bit(void)
{
	return get_check_bit();
}

static unsigned int wd_get_kick_bit(void)
{
	return get_kick_bit();
}


static int wd_restart(enum wd_restart_type type)
{
#ifdef	CONFIG_LOCAL_WDT
#ifdef CONFIG_SMP
	on_each_cpu((smp_call_func_t) mpcore_wdt_restart, WD_TYPE_NORMAL, 0);
#else
	mpcore_wdt_restart(type);
#endif
#endif
	mtk_wdt_restart(type);
	return 0;
}


static int wd_cpu_hot_plug_on_notify(int cpu)
{
	int res = 0;
	wk_cpu_update_bit_flag(cpu, 1);
	mtk_wdt_restart(WD_TYPE_NOLOCK);	/* for KICK external wdt */
	printk("WD wd_cpu_hot_plug_on_notify kick ext wd\n");

	return res;
}

static int wd_cpu_hot_plug_off_notify(int cpu)
{
	int res = 0;
	wk_cpu_update_bit_flag(cpu, 0);
	return res;
}

static int wd_sw_reset(int type)
{
	wdt_arch_reset(type);
	return 0;
}

static int mtk_wk_wdt_config(enum ext_wdt_mode mode, int timeout_val)
{

	mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, TRUE);
	mtk_wdt_set_time_out_value(timeout_val);
#ifdef	CONFIG_LOCAL_WDT
	mpcore_wk_wdt_config(0, 0, timeout_val - 5);	/* local 25s time out */
	/* mpcore_wdt_set_heartbeat(timeout_val - 5);//local 25s time out */
#endif

	return 0;
}

static int disable_ext(void)
{
	mtk_wdt_enable(WK_WDT_DIS);
	return 0;
}


static int disable_local(void)
{
#ifdef CONFIG_LOCAL_WDT
#ifdef CONFIG_SMP
	on_each_cpu((smp_call_func_t) local_wdt_enable, WK_WDT_DIS, 0);
#else
	local_wdt_enable(WK_WDT_DIS);
#endif
#endif
	printk(" wd_api disable_local not support now\n");
	return 0;
}

static int set_mode(enum ext_wdt_mode mode)
{
	printk("  support only irq mode-20140522");
	switch (mode) {
	case WDT_DUAL_MODE:
		break;

	case WDT_HW_REBOOT_ONLY_MODE:
		break;

	case WDT_IRQ_ONLY_MODE:
		printk("wd set only irq mode for debug\n");
		mtk_wdt_mode_config(FALSE, TRUE, TRUE, FALSE, TRUE);
		break;
	}

	return 0;

}

static int confirm_hwreboot(void)
{
	mtk_wdt_confirm_hwreboot();
	return 0;
}

static void suspend_notify(void)
{
	mtk_wd_suspend();
}

static void resume_notify(void)
{
	mtk_wd_resume();
}

static int disable_all_wd(void)
{
	disable_ext();
#ifdef CONFIG_LOCAL_WDT
	disable_local();
#endif
	return 0;
}

static int spmwdt_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode)
{
	int res = 0;
	if (WD_REQ_EN == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_SCPSYS_MARK,WD_REQ_EN); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_SPM_SCPSYS_MARK, WD_REQ_EN);
	} else if (WD_REQ_DIS == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_SCPSYS_MARK,WD_REQ_DIS); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_SPM_SCPSYS_MARK, WD_REQ_DIS);
	} else {
		res = -2;
	}

	if (WD_REQ_IRQ_MODE == mode) {
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_SPM_SCPSYS_MARK, WD_REQ_IRQ_MODE);
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_SCPSYS_MARK,WD_REQ_IRQ_MODE); */
	} else if (WD_REQ_RST_MODE == mode) {
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_SPM_SCPSYS_MARK, WD_REQ_RST_MODE);
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_SCPSYS_MARK,WD_REQ_RST_MODE); */
	} else {
		res = -3;
	}
	return res;
}

static int thermal_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode)
{
	int res = 0;
	if (WD_REQ_EN == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_EN); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK, WD_REQ_EN);
	} else if (WD_REQ_DIS == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_DIS); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK, WD_REQ_DIS);
	} else {
		res = -2;
	}

	if (WD_REQ_IRQ_MODE == mode) {
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_IRQ_MODE); */
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK, WD_REQ_IRQ_MODE);
	} else if (WD_REQ_RST_MODE == mode) {
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_RST_MODE); */
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK, WD_REQ_RST_MODE);
	} else {
		res = -3;
	}
	return res;
}

static int thermal_direct_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode)
{
	int res = 0;
	printk("thermal_direct_mode_config(en:0x%x,mode:0x%x)\n", en, mode);
	if (WD_REQ_EN == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_EN); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_EN);
	} else if (WD_REQ_DIS == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_DIS); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_DIS);
	} else {
		res = -2;
	}

	if (WD_REQ_IRQ_MODE == mode) {
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_IRQ_MODE); */
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_IRQ_MODE);
	} else if (WD_REQ_RST_MODE == mode) {
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_RST_MODE); */
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_RST_MODE);
	} else {
		res = -3;
	}
	return res;
}


static int wd_dram_reserved_mode(bool enabled)
{
	int ret = 0;
	if (true == enabled) {
		mtk_wdt_swsysret_config(0x10000000, 1);
	} else {
		mtk_wdt_swsysret_config(0x10000000, 0);
	}
	return ret;
}


#else
/* dummy api */

static unsigned int wd_get_check_bit(void)
{
	printk("dummy wd_get_check_bit");
	return 0;
}

static unsigned int wd_get_kick_bit(void)
{
	printk("dummy wd_get_kick_bit");
	return 0;
}

static int wd_restart(enum wd_restart_type type)
{
	printk("dummy wd_restart");
	return 0;
}


static int wd_cpu_hot_plug_on_notify(int cpu)
{
	int res = 0;
	printk("dummy wd_cpu_hot_plug_on_notify");
	return res;
}

static int wd_cpu_hot_plug_off_notify(int cpu)
{
	int res = 0;
	printk("dummy wd_cpu_hot_plug_off_notify");
	return res;
}

static int wd_sw_reset(int type)
{
	printk("dummy wd_sw_reset");
	wdt_arch_reset(type);
	return 0;
}

static int mtk_wk_wdt_config(enum ext_wdt_mode mode, int timeout_val)
{

	printk("dummy mtk_wk_wdt_config");
	return 0;
}

static int disable_ext(void)
{
	printk("dummy disable_ext");
	return 0;
}

static int disable_local(void)
{
	printk("dummy disable_local");
	return 0;
}

static int set_mode(enum ext_wdt_mode mode)
{
	printk("dummy set_mode");
	return 0;

}

static int confirm_hwreboot(void)
{
	printk("dummy confirm_hwreboot");
	return 0;
}

static void suspend_notify(void)
{

	printk("dummy suspend_notify  \n ");

}

static void resume_notify(void)
{

	printk("dummy resume_notify \n ");

}

static int disable_all_wd(void)
{
	printk("dummy disable_all_wd \n ");
	return 0;
}

static int spmwdt_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode)
{
	int res = 0;
	printk("dummy spmwdt_mode_config \n ");
	return res;
}

static int thermal_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode)
{
	int res = 0;
	printk("dummy thermal_mode_config \n ");
	return res;
}

static int wd_dram_reserved_mode(bool enabled)
{
	int res = 0;
	printk("dummy wd_dram_reserved_mode \n ");
	return res;
}

static int thermal_direct_mode_config(WD_REQ_CTL en, WD_REQ_MODE mode)
{
	int res = 0;
	printk("thermal_direct_mode_config in dummy driver (en:0x%x,mode:0x%x)\n", en, mode);
	if (WD_REQ_EN == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_EN); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_EN);
	} else if (WD_REQ_DIS == en) {
		/* g_ext_wd_drv.reques_en_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_DIS); */
		res = mtk_wdt_request_en_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_DIS);
	} else {
		res = -2;
	}

	if (WD_REQ_IRQ_MODE == mode) {
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_IRQ_MODE); */
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_IRQ_MODE);
	} else if (WD_REQ_RST_MODE == mode) {
		/* g_ext_wd_drv.reques_mode_set(MTK_WDT_REQ_SPM_THERMAL_MARK,WD_REQ_RST_MODE); */
		res = mtk_wdt_request_mode_set(MTK_WDT_REQ_THERMAL_MARK, WD_REQ_RST_MODE);
	} else {
		res = -3;
	}
	return res;
}



#endif

/* public interface implimentation end */

int wd_api_init(void)
{

	int i = 0;
	long *check_p = NULL;
	int api_size = 0;

	api_size = (sizeof(g_wd_api_obj) / sizeof(long));
	printk("wd api_size=%d\n", api_size);
	/* check wd api */
	check_p = (long *)&g_wd_api_obj;
	for (i = 1; i < api_size; i++) {
		printk("p[%d]=%lx\n", i, *(check_p + i));
		if (0 == check_p[i]) {
			printk("wd_api init fail the %d api not init\n", i);
			g_wd_api_obj.ready = 0;
			return -1;
		}

	}
	printk("wd_api init ok\n");
	return 0;
}

int get_wd_api(struct wd_api **obj)
{
	int res = 0;
	*obj = &g_wd_api_obj;
	if (NULL == *obj) {
		res = -1;
		/* printk("get_wd_public_interface_obj null pointer error\n"); */
	}
	if ((*obj)->ready == 0) {
		res = -2;
		/* printk("get_wd_public_api not ready\n"); */
	}
	return res;
}

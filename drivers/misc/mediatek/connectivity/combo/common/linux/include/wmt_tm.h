#ifndef _WMT_TM_H
#define _WMT_TM_H
#if WMT_PLAT_ALPS

#define CONFIG_THERMAL_OPEN
#if  defined(CONFIG_THERMAL) &&  defined(CONFIG_THERMAL_OPEN)

struct wmt_thermal_ctrl_ops {
	INT32 (*query_temp)(VOID);
	INT32 (*set_temp)(INT32);
};

INT32 wmt_tm_init(struct wmt_thermal_ctrl_ops *ops);
INT32 wmt_tm_deinit(VOID);
INT32 wmt_tm_init_rt(VOID);
INT32 wmt_tm_deinit_rt(VOID);
#endif

#endif
#endif

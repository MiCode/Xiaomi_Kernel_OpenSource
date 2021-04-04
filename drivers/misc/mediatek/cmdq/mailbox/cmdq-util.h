/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CMDQ_UTIL_H__
#define __CMDQ_UTIL_H__

#include <linux/kernel.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

enum {
	CMDQ_LOG_FEAT_SECURE,
	CMDQ_LOG_FEAT_PERF,
	CMDQ_LOG_FEAT_NUM,
};

#define cmdq_util_log(feat, fmt, args...) \
	do { \
		cmdq_util_error_save("[cmdq] "fmt"\n", ##args); \
		if (cmdq_util_get_bit_feature() & (1 << feat)) \
			cmdq_msg(fmt, ##args); \
	} while (0)

#define cmdq_aee(fmt, args...) \
	pr_notice("[cmdq][aee] "fmt"\n", ##args)

#define cmdq_util_msg(fmt, args...) \
	do { \
		cmdq_msg(fmt, ##args); \
		cmdq_util_error_save("[cmdq] "fmt"\n", ##args); \
	} while (0)

#define cmdq_util_err(fmt, args...) \
	do { \
		cmdq_dump(fmt, ##args); \
		cmdq_util_error_save("[cmdq][err] "fmt"\n", ##args); \
	} while (0)

#define cmdq_util_user_msg(chan, fmt, args...) \
	do { \
		if (chan) {  \
			u32 gce = cmdq_util_hw_id( \
				(u32)cmdq_mbox_get_base_pa(chan)); \
			s32 thd = cmdq_mbox_chan_id(chan); \
			pr_notice("[%s]<%u>(%d)[cmdq] "fmt"\n", \
				cmdq_thread_module_dispatch(gce, thd), \
				gce, thd, ##args); \
			cmdq_util_error_save("[cmdq] "fmt"\n", ##args); \
		} else \
			cmdq_util_msg(fmt, ##args); \
	} while (0)

#define cmdq_util_user_err(chan, fmt, args...) \
	do { \
		if (chan) {  \
			u32 gce = cmdq_util_hw_id( \
				(u32)cmdq_mbox_get_base_pa(chan)); \
			s32 thd = cmdq_mbox_chan_id(chan); \
			pr_notice("[%s]<%u>(%d)[cmdq][err] "fmt"\n", \
				cmdq_thread_module_dispatch(gce, thd), \
				gce, thd, ##args); \
			cmdq_util_error_save("[cmdq][err] "fmt"\n", ##args); \
		} else \
			cmdq_util_msg(fmt, ##args); \
	} while (0)

#define DB_OPT_CMDQ	(DB_OPT_DEFAULT | DB_OPT_PROC_CMDQ_INFO | \
	DB_OPT_MMPROFILE_BUFFER | DB_OPT_FTRACE | DB_OPT_DUMP_DISPLAY)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define cmdq_util_aee(key, fmt, args...) \
	do { \
		char tag[LINK_MAX]; \
		int len = snprintf(tag, LINK_MAX, "CRDISPATCH_KEY:%s", key); \
		if (len >= LINK_MAX) \
			pr_debug("len:%d over max:%d\n", \
				__func__, __LINE__, len, LINK_MAX); \
		cmdq_aee(fmt, ##args); \
		cmdq_util_error_save("[cmdq][aee] "fmt"\n", ##args); \
		aee_kernel_warning_api(__FILE__, __LINE__, \
			DB_OPT_CMDQ, tag, fmt, ##args); \
	} while (0)
#else
#define cmdq_util_aee(key, fmt, args...) \
	do { \
		char tag[LINK_MAX]; \
		int len = snprintf(tag, LINK_MAX, "CRDISPATCH_KEY:%s", key); \
		if (len >= LINK_MAX) \
			pr_debug("len:%d over max:%d\n", \
				__func__, __LINE__, len, LINK_MAX); \
		cmdq_aee(fmt" (aee not ready)", ##args); \
		cmdq_util_error_save("[cmdq][aee] "fmt"\n", ##args); \
	} while (0)

#endif

struct cmdq_pkt;

u32 cmdq_util_get_bit_feature(void);
bool cmdq_util_is_feature_en(u8 feature);

void cmdq_util_error_enable(void); // TODO : need be called
void cmdq_util_error_disable(void);
void cmdq_util_dump_lock(void);
void cmdq_util_dump_unlock(void);
s32 cmdq_util_error_save_lst(const char *format, va_list args);
s32 cmdq_util_error_save(const char *format, ...);
void cmdq_util_dump_dbg_reg(void *chan);
void cmdq_util_track(struct cmdq_pkt *pkt);
void cmdq_util_dump_smi(void);
u8 cmdq_util_track_ctrl(void *cmdq, phys_addr_t base, bool sec);
void cmdq_util_set_first_err_mod(void *chan, const char *mod);
const char *cmdq_util_get_first_err_mod(void *chan);

/* function support in platform */
const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread);
const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread);
u32 cmdq_util_hw_id(u32 pa);
const char *cmdq_util_hw_name(void *chan);


#if IS_ENABLED(CONFIG_MACH_MT6873) || IS_ENABLED(CONFIG_MACH_MT6853)
bool cmdq_thread_ddr_user_check(const s32 thread);
#endif

#endif

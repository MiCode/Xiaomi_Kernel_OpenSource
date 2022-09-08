/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CMDQ_UTIL_H__
#define __CMDQ_UTIL_H__

#include <linux/kernel.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

/* Compatibility with 32-bit shift operation */
#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
#define DO_SHIFT_RIGHT(x, n) ({     \
	(n) < (8 * sizeof(u64)) ? (x) >> (n) : 0;	\
})
#define DO_SHIFT_LEFT(x, n) ({      \
	(n) < (8 * sizeof(u64)) ? (x) << (n) : 0;	\
})
#else
#define DO_SHIFT_RIGHT(x, n) ({     \
	(n) < (8 * sizeof(u32)) ? (x) >> (n) : 0;	\
})
#define DO_SHIFT_LEFT(x, n) ({      \
	(n) < (8 * sizeof(u32)) ? (x) << (n) : 0;	\
})
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
			u32 gce = cmdq_util_get_hw_id( \
				(u32)cmdq_mbox_get_base_pa(chan)); \
			s32 thd = cmdq_mbox_chan_id(chan); \
			pr_notice("[%s]<%u>(%d)[cmdq] "fmt"\n", \
				cmdq_util_thread_module_dispatch(gce, thd), \
				gce, thd, ##args); \
			cmdq_util_error_save("[cmdq] "fmt"\n", ##args); \
		} else \
			cmdq_util_msg(fmt, ##args); \
	} while (0)

#define cmdq_util_user_err(chan, fmt, args...) \
	do { \
		if (chan) {  \
			u32 gce = cmdq_util_get_hw_id( \
				(u32)cmdq_mbox_get_base_pa(chan)); \
			s32 thd = cmdq_mbox_chan_id(chan); \
			pr_notice("[%s]<%u>(%d)[cmdq][err] "fmt"\n", \
				cmdq_util_thread_module_dispatch(gce, thd), \
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

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define cmdq_util_aee_ex(aee, key, fmt, args...) \
	do { \
		char tag[LINK_MAX]; \
		int len = snprintf(tag, LINK_MAX, "CRDISPATCH_KEY:%s", key); \
		if (len >= LINK_MAX) \
			pr_debug("len:%d over max:%d\n", \
				__func__, __LINE__, len, LINK_MAX); \
		cmdq_aee(fmt, ##args); \
		cmdq_util_error_save("[cmdq][aee] "fmt"\n", ##args); \
		if (aee == CMDQ_AEE_WARN) \
			aee_kernel_warning_api(__FILE__, __LINE__, \
				DB_OPT_CMDQ, tag, fmt, ##args); \
		else if (aee == CMDQ_AEE_EXCEPTION) \
			aee_kernel_exception_api(__FILE__, __LINE__, \
				DB_OPT_CMDQ, tag, fmt, ##args); \
		else \
			cmdq_aee("skip aee"); \
	} while (0)
#else
#define cmdq_util_aee_ex(aee, key, fmt, args...) \
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

typedef const char *(*platform_thread_module_dispatch)(phys_addr_t gce_pa, s32 thread);
typedef const char *(*platform_event_module_dispatch)(phys_addr_t gce_pa, const u16 event,
	s32 thread);
typedef u32 (*platform_util_hw_id)(u32 pa);
typedef u32 (*platform_test_get_subsys_list)(u32 **regs_out);
typedef void (*platform_test_set_ostd)(void);
typedef const char *(*platform_util_hw_name)(void *chan);
typedef bool (*platform_thread_ddr_module)(const s32 thread);

struct cmdq_util_platform_fp {
	platform_thread_module_dispatch thread_module_dispatch;
	platform_event_module_dispatch event_module_dispatch;
	platform_util_hw_id util_hw_id;
	platform_test_get_subsys_list test_get_subsys_list;
	platform_test_set_ostd test_set_ostd;
	platform_util_hw_name util_hw_name;
	platform_thread_ddr_module thread_ddr_module;
};

void cmdq_util_set_fp(struct cmdq_util_platform_fp *cust_cmdq_platform);
void cmdq_util_reset_fp(struct cmdq_util_platform_fp *cust_cmdq_platform);
const char *cmdq_util_event_module_dispatch(phys_addr_t gce_pa, const u16 event, s32 thread);
const char *cmdq_util_thread_module_dispatch(phys_addr_t gce_pa, s32 thread);
u32 cmdq_util_get_hw_id(u32 pa);
u32 cmdq_util_test_get_subsys_list(u32 **regs_out);
void cmdq_util_test_set_ostd(void);

u32 cmdq_util_get_bit_feature(void);
bool cmdq_util_is_feature_en(u8 feature);

void cmdq_util_error_enable(void); // TODO : need be called
void cmdq_util_error_disable(void);
void cmdq_util_dump_lock(void);
void cmdq_util_dump_unlock(void);
s32 cmdq_util_error_save_lst(const char *format, va_list args);
s32 cmdq_util_error_save(const char *format, ...);
void cmdq_util_enable_disp_va(void);
void cmdq_util_prebuilt_set_client(const u16 hwid, struct cmdq_client *client);
void cmdq_util_prebuilt_init(const u16 mod);
void cmdq_util_prebuilt_enable(const u16 hwid);
void cmdq_util_prebuilt_disable(const u16 hwid);
void cmdq_util_prebuilt_dump(const u16 hwid, const u16 event);
void cmdq_util_mminfra_cmd(const u8 type);
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
bool cmdq_thread_ddr_module(const s32 thread);
void cmdq_util_enable_dbg(u32 id);
void cmdq_util_devapc_dump(void);
int cmdq_util_init(void);

extern void mt_irq_dump_status(unsigned int irq);
int cmdq_util_log_feature_set(void *data, u64 val);
#endif

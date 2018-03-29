/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if !defined(__AEE_IPANIC_H__)
#define __AEE_IPANIC_H__

#include <generated/autoconf.h>
#include <linux/kallsyms.h>
#include <linux/kmsg_dump.h>
/* #include "staging/android/logger.h" */
#include <mt-plat/aee.h>
#include "ipanic_version.h"

#define AEE_IPANIC_PLABEL "expdb"
#ifdef CONFIG_MTK_GPT_SCHEME_SUPPORT
#define AEE_EXPDB_PATH "/dev/block/platform/mtk-msdc.0/by-name/expdb"
#else
#define AEE_EXPDB_PATH "/dev/expdb"
#endif

#define IPANIC_MODULE_TAG "KERNEL-PANIC"

#define AEE_IPANIC_MAGIC 0xaee0dead
#define AEE_IPANIC_PHDR_VERSION   0x10
#define IPANIC_NR_SECTIONS		32
#if (AEE_IPANIC_PHDR_VERSION >= 0x10)
#define IPANIC_USERSPACE_READ		1
#endif

#define AEE_LOG_LEVEL 8
#define LOG_DEBUG(fmt, ...)			\
	do {	\
		if (aee_in_nested_panic())			\
			aee_nested_printf(fmt, ##__VA_ARGS__);	\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

#define LOG_ERROR(fmt, ...)			\
	do {	\
		if (aee_in_nested_panic())			\
			aee_nested_printf(fmt, ##__VA_ARGS__);	\
		else						\
			pr_err(fmt, ##__VA_ARGS__);	\
	} while (0)

#define LOGV(fmt, msg...)
#define LOGD LOG_DEBUG
#define LOGI LOG_DEBUG
#define LOGW LOG_ERROR
#define LOGE LOG_ERROR

struct ipanic_data_header {
	u32 type;		/* data type(0-31) */
	u32 valid;		/* set to 1 when dump succeded */
	u32 offset;		/* offset in EXPDB partition */
	u32 used;		/* valid data size */
	u32 total;		/* allocated partition size */
	u32 encrypt;		/* data encrypted */
	u64 id;
	/* u32 raw;             raw data or plain text */
	/* u32 compact;         data and header in same block, to save space */
	u8 name[32];
};

struct ipanic_header {
	/* The magic/version field cannot be moved or resize */
	u32 magic;
	u32 version;		/* ipanic version */
	u32 size;		/* ipanic_header size */
	u32 datas;		/* bitmap of data sections dumped */
	u32 dhblk;		/* data header blk size, 0 if no dup data headers */
	u32 blksize;
	u32 partsize;		/* expdb partition total size */
	u32 bufsize;
	u64 buf;
	struct ipanic_data_header data_hdr[IPANIC_NR_SECTIONS];
};

#define IPANIC_MMPROFILE_LIMIT		0x220000

struct ipanic_ops {

	struct aee_oops *(*oops_copy)(void);

	void (*oops_free)(struct aee_oops *oops, int erase);
};

void register_ipanic_ops(struct ipanic_ops *op);
struct aee_oops *ipanic_oops_copy(void);
void ipanic_oops_free(struct aee_oops *oops, int erase);
void ipanic_block_scramble(u8 *buf, int buflen);
/* for WDT timeout case : dump timer/schedule/irq/softirq etc... debug information */
extern void aee_wdt_dump_info(void);
void aee_disable_api(void);
int panic_dump_android_log(char *buf, size_t size, int type);

/* User space process support functions */
#define MAX_NATIVEINFO  (32 * 1024)
#define MAX_NATIVEHEAP  2048
extern char NativeInfo[MAX_NATIVEINFO];	/* check that 32k is enought?? */
extern unsigned long User_Stack[MAX_NATIVEHEAP];	/* 8K Heap */
int DumpNativeInfo(void);
/*
* Since ipanic_detail and usersapce info size is not known
* until run time, we do a guess here
*/
#define IPANIC_DETAIL_USERSPACE_SIZE    (100 * 1024)

#if 1
#ifdef CONFIG_MTK_AEE_IPANIC_TYPES
#define IPANIC_DT_DUMP			CONFIG_MTK_AEE_IPANIC_TYPES
#else
#define IPANIC_DT_DUMP			0x0fffffff
#endif
#define IPANIC_DT_ENCRYPT		0xfffffffe

enum IPANIC_DT {
	IPANIC_DT_HEADER = 0,
	IPANIC_DT_KERNEL_LOG = 1,
	IPANIC_DT_WDT_LOG,
	IPANIC_DT_WQ_LOG,
	IPANIC_DT_CURRENT_TSK = 6,
	IPANIC_DT_OOPS_LOG,
	IPANIC_DT_MINI_RDUMP = 8,
	IPANIC_DT_MMPROFILE,
	IPANIC_DT_MAIN_LOG,
	IPANIC_DT_SYSTEM_LOG,
	IPANIC_DT_EVENTS_LOG,
	IPANIC_DT_RADIO_LOG,
	IPANIC_DT_LAST_LOG,
	IPANIC_DT_ATF_LOG,
	IPANIC_DT_DISP_LOG,
	IPANIC_DT_RAM_DUMP = 28,
	IPANIC_DT_SHUTDOWN_LOG = 30,
	IPANIC_DT_RESERVED31 = 31,
};

struct ipanic_memory_block {
	unsigned long kstart;	/* start kernel addr of memory dump */
	unsigned long kend;	/* end kernel addr of memory dump */
	unsigned long pos;	/* next pos to dump */
	unsigned long reserved;	/* reserved */
};

struct ipanic_dt_op {
	char string[32];
	int size;
	int (*next)(void *data, unsigned char *buffer, size_t sz_buf);
};

struct ipanic_atf_log_rec {
	size_t total_size;
	size_t has_read;
	unsigned long start_idx;
};

#define ipanic_dt_encrypt(x)		((IPANIC_DT_ENCRYPT >> x) & 1)
#define ipanic_dt_active(x)		((IPANIC_DT_DUMP >> x) & 1)

/* copy from kernel/drivers/staging/android/logger.h */
/*
  SMP porting, we double the android buffer
* and kernel buffer size for dual core
*/
#ifdef CONFIG_SMP
#ifndef __MAIN_BUF_SIZE
#define __MAIN_BUF_SIZE (256 * 1024)
#endif

#ifndef __EVENTS_BUF_SIZE
#define __EVENTS_BUF_SIZE (256 * 1024)
#endif

#ifndef __RADIO_BUF_SIZE
#define __RADIO_BUF_SIZE (256 * 1024)
#endif

#ifndef __SYSTEM_BUF_SIZE
#define __SYSTEM_BUF_SIZE (256 * 1024)
#endif
#else
#ifndef __MAIN_BUF_SIZE
#define __MAIN_BUF_SIZE (256 * 1024)
#endif

#ifndef __EVENTS_BUF_SIZE
#define __EVENTS_BUF_SIZE (256 * 1024)
#endif

#ifndef __RADIO_BUF_SIZE
#define __RADIO_BUF_SIZE (64 * 1024)
#endif

#ifndef __SYSTEM_BUF_SIZE
#define __SYSTEM_BUF_SIZE (64 * 1024)
#endif
#endif

#ifndef __LOG_BUF_LEN
#define __LOG_BUF_LEN	(1 << CONFIG_LOG_BUF_SHIFT)
#endif

#define OOPS_LOG_LEN	__LOG_BUF_LEN
#define WDT_LOG_LEN	__LOG_BUF_LEN
#define LAST_LOG_LEN	(AEE_LOG_LEVEL == 8 ? __LOG_BUF_LEN : 32*1024)

#define ATF_LOG_SIZE	(32*1024)
#define DISP_LOG_SIZE	(30*16*1024)

char *expdb_read_size(int off, int len);
char *ipanic_read_size(int off, int len);
int ipanic_write_size(void *buf, int off, int len);
void ipanic_erase(void);
struct ipanic_header *ipanic_header(void);
void ipanic_msdc_init(void);
int ipanic_msdc_info(struct ipanic_header *iheader);
void ipanic_log_temp_init(void);
void ipanic_klog_region(struct kmsg_dumper *dumper);
int ipanic_klog_buffer(void *data, unsigned char *buffer, size_t sz_buf);
extern int ipanic_atflog_buffer(void *data, unsigned char *buffer, size_t sz_buf);
extern int panic_dump_disp_log(void *data, unsigned char *buffer, size_t sz_buf);

int ipanic_mem_write(void *buf, int off, int len, int encrypt);
void *ipanic_data_from_sd(struct ipanic_data_header *dheader, int encrypt);
struct ipanic_header *ipanic_header_from_sd(unsigned int offset, unsigned int magic);
#endif
extern int card_dump_func_read(unsigned char *buf, unsigned int len, unsigned long long offset,
			       int dev);
extern int card_dump_func_write(unsigned char *buf, unsigned int len, unsigned long long offset,
				int dev);
extern unsigned int reset_boot_up_device(int type);	/* force to re-initialize the emmc host controller */
/*#ifdef CONFIG_MTK_MMPROFILE_SUPPORT*/
#ifdef CONFIG_MMPROFILE
extern unsigned int MMProfileGetDumpSize(void);
extern void MMProfileGetDumpBuffer(unsigned int Start, unsigned long *pAddr, unsigned int *pSize);
#endif
extern void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs);
extern void mrdump_mini_ke_cpu_regs(struct pt_regs *regs);
extern void mrdump_mini_add_misc(unsigned long addr, unsigned long size, unsigned long start,
				 char *name);
extern void mrdump_mini_ipanic_done(void);
extern int mrdump_task_info(unsigned char *buffer, size_t sz_buf);
extern void aee_rr_rec_exp_type(unsigned int type);
extern unsigned int aee_rr_curr_exp_type(void);
extern void aee_rr_rec_scp(void);
#ifdef CONFIG_SCHED_DEBUG
extern int sysrq_sched_debug_show_at_AEE(void);
#endif
#ifdef CONFIG_MTK_WQ_DEBUG
extern void wq_debug_dump(void);
#endif
extern void __disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2(void);

#endif

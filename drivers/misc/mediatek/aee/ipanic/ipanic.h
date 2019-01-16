#if !defined(__AEE_IPANIC_H__)
#define __AEE_IPANIC_H__

#include <generated/autoconf.h>
#include <linux/kallsyms.h>
#include <linux/kmsg_dump.h>
/* #include "staging/android/logger.h" */
#include <linux/aee.h>
#include "ipanic_version.h"

#define AEE_IPANIC_PLABEL "expdb"

#define IPANIC_MODULE_TAG "KERNEL-PANIC"

#define AEE_IPANIC_MAGIC 0xaee0dead
#define AEE_IPANIC_PHDR_VERSION   0x10
#define IPANIC_NR_SECTIONS		32
#if (AEE_IPANIC_PHDR_VERSION >= 0x10)
#define IPANIC_USERSPACE_READ		1
#endif

#define AEE_LOG_LEVEL 8
#define LOG_DEBUG(fmt, ...)			\
	if (aee_in_nested_panic())			\
		aee_nested_printf(fmt, ##__VA_ARGS__);	\
	else						\
		pr_debug(fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...)			\
	if (aee_in_nested_panic())			\
		aee_nested_printf(fmt, ##__VA_ARGS__);	\
	else						\
		pr_err(fmt, ##__VA_ARGS__)

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
	u32 raw;		/* raw data or plain text */
	u32 compact;		/* data and header in same block, to save space */
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
	u32 partsize;		/* expdb partition totoal size */
	u32 bufsize;
	u64 buf;
	struct ipanic_data_header data_hdr[IPANIC_NR_SECTIONS];
};

#define IPANIC_MMPROFILE_LIMIT		0x220000

struct ipanic_ops {

	struct aee_oops *(*oops_copy) (void);

	void (*oops_free) (struct aee_oops *oops, int erase);
};

void register_ipanic_ops(struct ipanic_ops *op);
struct aee_oops *ipanic_oops_copy(void);
void ipanic_oops_free(struct aee_oops *oops, int erase);
void ipanic_block_scramble(u8 *buf, int buflen);
/* for WDT timeout case : dump timer/schedule/irq/softirq etc... debug information */
extern void aee_wdt_dump_info(void);
extern int mt_dump_wq_debugger(void);
void aee_disable_api(void);
int panic_dump_android_log(char *buf, size_t size, int type);

/* User space process support functions */
#define MAX_NATIVEINFO  32*1024
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

typedef enum {
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
	IPANIC_DT_RAM_DUMP = 28,
	IPANIC_DT_SHUTDOWN_LOG = 30,
	IPANIC_DT_RESERVED31 = 31,
} IPANIC_DT;

struct ipanic_memory_block {
	unsigned long kstart;	/* start kernel addr of memory dump */
	unsigned long kend;	/* end kernel addr of memory dump */
	unsigned long pos;	/* next pos to dump */
	unsigned long reserved;	/* reserved */
};

typedef struct ipanic_dt_op {
	char string[32];
	int size;
	int (*next) (void *data, unsigned char *buffer, size_t sz_buf);
} ipanic_dt_op_t;

typedef struct ipanic_atf_log_rec {
    size_t total_size;
    size_t has_read;
    unsigned long start_idx;
} ipanic_atf_log_rec_t;

#define ipanic_dt_encrypt(x)		((IPANIC_DT_ENCRYPT >> x) & 1)
#define ipanic_dt_active(x)		((IPANIC_DT_DUMP >> x) & 1)

/* copy from kernel/drivers/staging/android/logger.h */
/*
  SMP porting, we double the android buffer
* and kernel buffer size for dual core
*/
#ifdef CONFIG_SMP
#ifndef __MAIN_BUF_SIZE
#define __MAIN_BUF_SIZE 256*1024
#endif

#ifndef __EVENTS_BUF_SIZE
#define __EVENTS_BUF_SIZE 256*1024
#endif

#ifndef __RADIO_BUF_SIZE
#define __RADIO_BUF_SIZE 256*1024
#endif

#ifndef __SYSTEM_BUF_SIZE
#define __SYSTEM_BUF_SIZE 256*1024
#endif
#else
#ifndef __MAIN_BUF_SIZE
#define __MAIN_BUF_SIZE 256*1024
#endif

#ifndef __EVENTS_BUF_SIZE
#define __EVENTS_BUF_SIZE 256*1024
#endif

#ifndef __RADIO_BUF_SIZE
#define __RADIO_BUF_SIZE 64*1024
#endif

#ifndef __SYSTEM_BUF_SIZE
#define __SYSTEM_BUF_SIZE 64*1024
#endif
#endif

#ifndef __LOG_BUF_LEN
#define __LOG_BUF_LEN	(1 << CONFIG_LOG_BUF_SHIFT)
#endif

#define OOPS_LOG_LEN	__LOG_BUF_LEN
#define WDT_LOG_LEN	__LOG_BUF_LEN
#define WQ_LOG_LEN	32*1024
#define LAST_LOG_LEN	(AEE_LOG_LEVEL == 8 ? __LOG_BUF_LEN : 32*1024)

#define ATF_LOG_SIZE	(32*1024)

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
#endif
#endif

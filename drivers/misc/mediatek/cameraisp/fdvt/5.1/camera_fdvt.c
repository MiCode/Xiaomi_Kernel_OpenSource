/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 * camera_fdvt.c - Linux FDVT Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers FDVT relative functions
 *
 *****************************************************************************/
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
/* #include <asm/io.h> */
/* #include <asm/tcm.h> */
#include <linux/proc_fs.h>	/* proc file use */
/*  */
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include <linux/io.h> */
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>

/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "inc/camera_fdvt.h"
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */
/* #if defined(CONFIG_MTK_LEGACY) */
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/* #include <mach/mt_clkmgr.h> */
/* #endif */
#include <mt-plat/sync_write.h>	/* For mt65xx_reg_sync_writel(). */
/* For spm_enable_sodi()/spm_disable_sodi(). */
/* #include <mach/mt_spm_idle.h> */
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#else /* CONFIG_MTK_IOMMU_V2 */
#include <m4u.h>
#endif /* CONFIG_MTK_IOMMU_V2 */
#define CMDQ_MAIL_BOX
#ifdef CMDQ_MAIL_BOX
#include <linux/soc/mediatek/mtk-cmdq.h>
#else /* CMDQ_MAIL_BOX */
#include <cmdq_core.h>
#include <cmdq_record.h>
#endif /* CMDQ_MAIL_BOX */
#include <smi_public.h>
#include <linux/dma-mapping.h>
#include "mach/pseudo_m4u.h"
#include <cmdq-sec.h>

/* Measure the kernel performance
 * #define __FDVT_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif /* __FDVT_KERNEL_PERFORMANCE_MEASURE__ */

#if IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#ifdef CMDQ_MTEE
#include <linux/atomic.h>
static atomic_t m4u_gz_init = ATOMIC_INIT(0);
#endif
#endif

#if 0
/* Another Performance Measure Usage */
#include <linux/kallsyms.h>
#include <linux/ftrace_event.h>
static unsigned long __read_mostly tracing_mark_write_addr;

#define _kernel_trace_begin(name) {\
	tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");\
	event_trace_printk(tracing_mark_write_addr,\
	"B|%d|%s\n", current->tgid, name);\
}

#define _kernel_trace_end() {\
	event_trace_printk(tracing_mark_write_addr, "E\n");\
}

/* How to Use */
/* char strName[128]; */
/* sprintf(strName, "TAG_K_WAKEUP (%d)",sof_count[_PASS1]); */
/* _kernel_trace_begin(strName); */
/* _kernel_trace_end(); */
#endif /* 0 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
//#include"../../../smi/smi_debug.h"

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif /* CONFIG_COMPAT */

/* #include "smi_common.h" */

//#include <linux/wakelock.h>
#ifdef CONFIG_PM_SLEEP // modified by gasper for build pass
#include <linux/pm_wakeup.h>
#endif /* CONFIG_PM_SLEEP */

#ifndef M4U_PORT_L20_IPE_FDVT_RDA_DISP
#define M4U_PORT_L20_IPE_FDVT_RDA_DISP M4U_PORT_L20_IPE_FDVT_RDA
#endif /* M4U_PORT_L20_IPE_FDVT_RDA_DISP */

#ifndef M4U_PORT_L20_IPE_FDVT_WRB_DISP
#define M4U_PORT_L20_IPE_FDVT_WRB_DISP M4U_PORT_L20_IPE_FDVT_WRB
#endif /* M4U_PORT_L20_IPE_FDVT_WRB_DISP */

/* FDVT Command Queue */
/* #include "../../cmdq/mt6797/cmdq_record.h" */
/* #include "../../cmdq/mt6797/cmdq_core.h" */

/* CCF */
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
struct FDVT_CLK_STRUCT {
#define SMI_CLK
#ifndef SMI_CLK
	struct clk *CG_SCP_SYS_MM0;
	struct clk *CG_MM_SMI_COMMON;
	struct clk *CG_MM_SMI_COMMON_2X;
	struct clk *CG_MM_SMI_COMMON_GALS_M0_2X;
	struct clk *CG_MM_SMI_COMMON_GALS_M1_2X;
	struct clk *CG_MM_SMI_COMMON_UPSZ0;
	struct clk *CG_MM_SMI_COMMON_UPSZ1;
	struct clk *CG_MM_SMI_COMMON_FIFO0;
	struct clk *CG_MM_SMI_COMMON_FIFO1;
	struct clk *CG_MM_LARB5;
	struct clk *CG_SCP_SYS_ISP;
	struct clk *CG_IPESYS_LARB;
#endif /* SMI_CLK */
	struct clk *CG_IPESYS_FD;
};

struct FDVT_CLK_STRUCT fdvt_clk;
#endif
/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */

#ifndef MTRUE
#define MTRUE 1
#endif
#ifndef MFALSE
#define MFALSE 0
#endif

#define FDVT_DEV_NAME "camera-fdvt"
//#define EP_NO_CLKMGR // GASPER ADD
#define BYPASS_REG (0)
/* #define FDVT_WAITIRQ_LOG */
#define FDVT_USE_GCE
//#define FDVT_DEBUG_USE
#define DUMMY_FDVT (0)
/* #define FDVT_MULTIPROCESS_TIMEING_ISSUE */
/*I can' test the situation in FPGA due to slow FPGA. */
#define FDTAG "[FDVT]"
#define IRQTAG "KEEPER"

#define log_vrb(format, args...) \
pr_debug(FDTAG "[%s] " format, __func__, ##args)

#ifdef FDVT_DEBUG_USE
#define log_dbg(format, args...) \
pr_info(FDTAG "[%s] " format, __func__, ##args)
#else
#define log_dbg(format, args...)
#endif

#define log_inf(format, args...) \
pr_info(FDTAG "[%s] " format, __func__, ##args)
#define log_notice(format, args...) \
pr_notice(FDTAG "[%s] " format, __func__, ##args)
#define log_wrn(format, args...) \
pr_info(FDTAG "[%s] " format, __func__, ##args)
#define log_err(format, args...) \
pr_info(FDTAG "[%s] " format, __func__, ##args)
#define log_ast(format, args...) \
pr_debug(FDTAG "[%s] " format, __func__, ##args)

/*****************************************************************************
 *
 *****************************************************************************/
// For other projects.
// #define FDVT_WR32(addr, data)    iowrite32(data, addr)
// For 89 Only.   // NEED_TUNING_BY_PROJECT
#define FDVT_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define FDVT_RD32(addr)          ioread32(addr)
/*****************************************************************************
 *
 *****************************************************************************/
/* dynamic log level */
#define FDVT_DBG_DBGLOG              (0x00000001)
#define FDVT_DBG_INFLOG              (0x00000002)
#define FDVT_DBG_INT                 (0x00000004)
#define FDVT_DBG_READ_REG            (0x00000008)
#define FDVT_DBG_WRITE_REG           (0x00000010)
#define FDVT_DBG_TASKLET             (0x00000020)

/*
 * CAM interrupt status
 */

/* normal siganl : happens to be the same bit as register bit*/
/*#define FDVT_INT_ST           (1<<0)*/
/*
 *   IRQ signal mask
 */

#define INT_ST_MASK_FDVT     (FDVT_INT_ST)
#define CMDQ_REG_MASK 0xffffffff
#define FDVT_START_MASK      0x1
#define FDVT_IS_BUSY    0x1000000

/* static irqreturn_t FDVT_Irq_CAM_A(signed int irq, void *device_id); */
static irqreturn_t isp_irq_fdvt(signed int irq, void *device_id);
static bool config_fdvt(void);
static signed int config_fdvt_hw(struct fdvt_config *basic_config);
static signed int config_secure_fdvt_hw(struct fdvt_config *basic_config);
static void fdvt_schedule_work(struct work_struct *data);
static signed int fdvt_dump_reg(void);

typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE FDVT_IRQ_CB_TBL[FDVT_IRQ_TYPE_AMOUNT] = {
	{isp_irq_fdvt, FDVT_IRQ_BIT_ID, "fdvt"},
};

#else /* CONFIG_OF */
/* int number is got from kernel api */
const struct ISR_TABLE FDVT_IRQ_CB_TBL[FDVT_IRQ_TYPE_AMOUNT] = {
#if DUMMY_FDVT
	{isp_irq_fdvt, 0, "fdvt-dummy"},
#else /* DUMMY_FDVT */
	{isp_irq_fdvt, 0, "fdvt"},
#endif /* DUMMY_FDVT */
};
#endif /* CONFIG_OF */
/* ///////////////////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pFDVT_tkt;
};

struct tasklet_struct fdvt_tkt[FDVT_IRQ_TYPE_AMOUNT];

static void isp_tasklet_func_fdvt(unsigned long data);

static struct tasklet_table fdvt_tasklet[FDVT_IRQ_TYPE_AMOUNT] = {
	{isp_tasklet_func_fdvt, &fdvt_tkt[FDVT_IRQ_TYPE_INT_FDVT_ST]},
};

//struct wake_lock fdvt_wake_lock;
#ifdef CONFIG_PM_SLEEP
struct wakeup_source fdvt_wake_lock;
#endif /* CONFIG_PM_SLEEP */

static DEFINE_MUTEX(fdvt_mutex);
static DEFINE_MUTEX(fdvt_deque_mutex);
static DEFINE_MUTEX(fdvt_clk_mutex);

#ifdef CONFIG_OF

struct fdvt_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct fdvt_device *fdvt_devs;
static int nr_fdvt_devs;

/* Get HW modules' base address from device nodes */
#define FDVT_DEV_NODE_IDX 0
#define IPESYS_DEV_MODE_IDX 1
/* static unsigned long gISPSYS_Reg[FDVT_IRQ_TYPE_AMOUNT]; */

#define ISP_FDVT_BASE                  (fdvt_devs[FDVT_DEV_NODE_IDX].regs)
#define ISP_IPESYS_BASE               (fdvt_devs[IPESYS_DEV_MODE_IDX].regs)
/* #define ISP_FDVT_BASE                  (gISPSYS_Reg[FDVT_DEV_NODE_IDX]) */

#else /* CONFIG_OF */
#define ISP_FDVT_BASE                        (ISP_IPESYS_BASE + 0x1000)
#endif /* CONFIG_OF */

static unsigned int clock_enable_count;
static unsigned int fdvt_count;

#ifdef CONFIG_MTK_IOMMU_V2
static int FD_MEM_USE_VIRTUL = 1;
#endif

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32

enum FDVT_FRAME_STATUS_ENUM {
	FDVT_FRAME_STATUS_EMPTY,	/* 0 */
	FDVT_FRAME_STATUS_ENQUE,	/* 1 */
	FDVT_FRAME_STATUS_RUNNING,	/* 2 */
	FDVT_FRAME_STATUS_FINISHED,	/* 3 */
	FDVT_FRAME_STATUS_TOTAL
};

enum FDVT_REQUEST_STATE_ENUM {
	FDVT_REQUEST_STATE_EMPTY,	/* 0 */
	FDVT_REQUEST_STATE_PENDING,	/* 1 */
	FDVT_REQUEST_STATE_RUNNING,	/* 2 */
	FDVT_REQUEST_STATE_FINISHED,	/* 3 */
	FDVT_REQUEST_STATE_TOTAL
};

struct FDVT_REQUEST_STRUCT {
	enum FDVT_REQUEST_STATE_ENUM state;
	pid_t process_id; /* caller process ID */
	unsigned int caller_id; /* caller thread ID */
	/* to judge it belongs to which frame package */
	unsigned int enque_req_num;
	unsigned int frame_wr_idx; /* Frame write Index */
	unsigned int frame_rd_idx; /* Frame read Index */
	enum FDVT_FRAME_STATUS_ENUM
	fdvt_frame_status[MAX_FDVT_FRAME_REQUEST];
	struct fdvt_config frame_config[MAX_FDVT_FRAME_REQUEST];
};

struct FDVT_REQUEST_RING_STRUCT {
	signed int write_idx;	/* enque how many request */
	signed int read_idx;		/* read which request index */
	signed int hw_process_idx;	/* HWWriteIdx */
	struct FDVT_REQUEST_STRUCT
	req_struct[MAX_FDVT_REQUEST_RING_SIZE];
};

struct FDVT_CONFIG_STRUCT {
	struct fdvt_config frame_config[MAX_FDVT_FRAME_REQUEST];
};

struct S_START_T {
	unsigned int sec;
	unsigned int usec;
};

static struct FDVT_REQUEST_RING_STRUCT fdvt_req_ring;
static struct FDVT_CONFIG_STRUCT fdvt_enq_req;
static struct FDVT_CONFIG_STRUCT fdvt_deq_req;
static struct cmdq_client *fdvt_clt;
static struct cmdq_client *fdvt_secure_clt;
static s32 fdvt_event_id;

/*****************************************************************************
 *
 *****************************************************************************/
struct FDVT_USER_INFO_STRUCT {
	pid_t pid;
	pid_t tid;
};

enum FDVT_PROCESS_ID_ENUM {
	FDVT_PROCESS_ID_NONE,
	FDVT_PROCESS_ID_FDVT,
	FDVT_PROCESS_ID_AMOUNT
};

/*****************************************************************************
 *
 *****************************************************************************/
struct FDVT_IRQ_INFO_STRUCT {
	unsigned int status[FDVT_IRQ_TYPE_AMOUNT];
	signed int fdvt_irq_cnt;
	pid_t process_id[FDVT_PROCESS_ID_AMOUNT];
	unsigned int mask[FDVT_IRQ_TYPE_AMOUNT];
};

struct FDVT_INFO_STRUCT {
	spinlock_t spinlock_fdvt_ref;
	spinlock_t spinlock_fdvt;
	spinlock_t spinlock_irq[FDVT_IRQ_TYPE_AMOUNT];
	wait_queue_head_t wait_queue_head;
	struct work_struct schedule_fdvt_work;
	unsigned int user_count;	/* User count */
	unsigned int debug_mask;	/* Debug mask */
	signed int irq_num;
	struct FDVT_IRQ_INFO_STRUCT irq_info;
	signed int write_req_idx;
	signed int read_req_idx;
	pid_t process_id[MAX_FDVT_FRAME_REQUEST];
};

static struct FDVT_INFO_STRUCT fdvt_info;

enum LOG_TYPE {
	/* currently, only used at ipl_buf_ctrl for critical section */
	_LOG_DBG = 0,
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
};

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/* #define SV_LOG_STR_LEN NORMAL_STR_LEN */

#define LOG_PPNUM 2
static unsigned int m_CurrentPPB;
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
	struct S_START_T _lastIrqTime;
} *FDVT_PSV_LOG_STR;

static void *log_kmalloc;
static struct SV_LOG_STR sv_log[FDVT_IRQ_TYPE_AMOUNT];

/*
 *   for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *   limited:
 *   each log must shorter than 512 bytes
 *  total log length in each irq/logtype can't over 1024 bytes
 */
#if 1
#define IRQ_LOG_KEEPER(irq, ppb, log_t, fmt, ...) do {\
	char *ptr; \
	char *des;\
	signed int ava_len;\
	unsigned int *ptr2 = &sv_log[irq]._cnt[ppb][log_t];\
	unsigned int str_leng;\
	unsigned int i = 0;\
	unsigned int index = 0;\
	int ret; \
	struct SV_LOG_STR *src = &sv_log[irq];\
	if (log_t == _LOG_ERR) {\
		str_leng = NORMAL_STR_LEN * ERR_PAGE;\
	} else if (log_t == _LOG_DBG) {\
		str_leng = NORMAL_STR_LEN * DBG_PAGE;\
	} else if (log_t == _LOG_INF) {\
		str_leng = NORMAL_STR_LEN * INF_PAGE;\
	} else {\
		str_leng = 0;\
	} \
	ptr = des = \
	(char *)&(sv_log[irq]._str[ppb][log_t][sv_log[irq]._cnt[ppb][log_t]]);\
	ava_len = str_leng - 1 - sv_log[irq]._cnt[ppb][log_t];\
	if (ava_len > 1) {\
		ret = snprintf((char *)(des), ava_len, "[%d.%06d]" fmt,\
		sv_log[irq]._lastIrqTime.sec, sv_log[irq]._lastIrqTime.usec,\
		##__VA_ARGS__);\
		if (ret < 0) { \
			log_err("snprintf fail(%d)\n", ret); \
		} \
		if ('\0' != sv_log[irq]._str[ppb][log_t][str_leng - 1]) {\
			log_err("log str over flow(%d)", irq);\
		} \
		while (*ptr++ != '\0') {\
			(*ptr2)++;\
		} \
	} else { \
		log_inf("(%d)(%d)log str avalible=0, print log\n", irq, log_t);\
		ptr = src->_str[ppb][log_t];\
		if (src->_cnt[ppb][log_t] != 0) {\
			if (log_t == _LOG_DBG) {\
				for (i = 0; i < DBG_PAGE; i++) {\
					index = NORMAL_STR_LEN * (i + 1) - 1;\
					if (ptr[index] != '\0') {\
						ptr[index] = '\0';\
						log_dbg("%s", &ptr[NORMAL_STR_LEN * i]);\
					} else {\
						log_dbg("%s", &ptr[NORMAL_STR_LEN * i]);\
						break;\
					} \
				} \
			} \
			else if (log_t == _LOG_INF) {\
				for (i = 0; i < INF_PAGE; i++) {\
					index = NORMAL_STR_LEN * (i + 1) - 1;\
					if (ptr[index] != '\0') {\
						ptr[index] = '\0';\
						log_inf("%s", &ptr[NORMAL_STR_LEN * i]);\
					} else{\
						log_inf("%s", &ptr[NORMAL_STR_LEN * i]);\
						break;\
					} \
				} \
			} \
			else if (log_t == _LOG_ERR) {\
				for (i = 0; i < ERR_PAGE; i++) {\
					index = NORMAL_STR_LEN * (i + 1) - 1;\
					if (ptr[index] != '\0') {\
						ptr[index] = '\0';\
						log_err("%s", &ptr[NORMAL_STR_LEN * i]);\
					} else{\
						log_err("%s", &ptr[NORMAL_STR_LEN * i]);\
						break;\
					} \
				} \
			} \
			else {\
				log_err("N.S.%d", log_t);\
			} \
			ptr[0] = '\0';\
			src->_cnt[ppb][log_t] = 0;\
			ava_len = str_leng - 1;\
			ptr = des = \
			(char *)&src->_str[ppb][log_t][src->_cnt[ppb][log_t]];\
			ptr2 = &src->_cnt[ppb][log_t];\
		ret = snprintf((char *)(des), ava_len, fmt, ##__VA_ARGS__);\
		if (ret < 0) { \
			log_err("snprintf fail(%d)\n", ret); \
		} \
			while (*ptr++ != '\0') {\
				(*ptr2)++;\
			} \
		} \
	} \
} while (0)
#else
#define IRQ_LOG_KEEPER(irq, ppb, log_t, fmt, args...) \
pr_debug(IRQTAG fmt, ##args)
#endif

#if 1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *src = &sv_log[irq];\
	char *ptr;\
	unsigned int i;\
	unsigned int ppb = 0;\
	unsigned int log_t = 0;\
	unsigned int index = 0;\
	if (ppb_in > 1) {\
		ppb = 1;\
	} else{\
		ppb = ppb_in;\
	} \
	if (logT_in > _LOG_ERR) {\
		log_t = _LOG_ERR;\
	} else{\
		log_t = logT_in;\
	} \
	ptr = src->_str[ppb][log_t];\
	if (src->_cnt[ppb][log_t] != 0) {\
		if (log_t == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				index = NORMAL_STR_LEN * (i + 1) - 1;\
				if (ptr[index] != '\0') {\
					ptr[index] = '\0';\
					log_dbg("%s", &ptr[NORMAL_STR_LEN * i]);\
				} else{\
					log_dbg("%s", &ptr[NORMAL_STR_LEN * i]);\
					break;\
				} \
			} \
		} \
	else if (log_t == _LOG_INF) {\
		for (i = 0; i < INF_PAGE; i++) {\
			index = NORMAL_STR_LEN * (i + 1) - 1;\
			if (ptr[index] != '\0') {\
				ptr[index] = '\0';\
				log_inf("%s", &ptr[NORMAL_STR_LEN * i]);\
			} else{\
				log_inf("%s", &ptr[NORMAL_STR_LEN * i]);\
				break;\
			} \
		} \
	} \
	else if (log_t == _LOG_ERR) {\
		for (i = 0; i < ERR_PAGE; i++) {\
			index = NORMAL_STR_LEN * (i + 1) - 1;\
			if (ptr[index] != '\0') {\
				ptr[index] = '\0';\
				log_err("%s", &ptr[NORMAL_STR_LEN * i]);\
			} else{\
				log_err("%s", &ptr[NORMAL_STR_LEN * i]);\
				break;\
			} \
		} \
	} \
	else {\
		log_err("N.S.%d", log_t);\
	} \
		ptr[0] = '\0';\
		src->_cnt[ppb][log_t] = 0;\
	} \
} while (0)

#else
#define IRQ_LOG_PRINTER(irq, ppb, log_t)
#endif

#define IPESYS_REG_CG_CON                  (ISP_IPESYS_BASE + 0x0)
#define IPESYS_REG_CG_SET                  (ISP_IPESYS_BASE + 0x4)
#define IPESYS_REG_CG_CLR                  (ISP_IPESYS_BASE + 0x8)

#define FDVT_START_HW                      (FDVT_BASE_HW + 0x000)
#define FDVT_ENABLE_HW                     (FDVT_BASE_HW + 0x004)
#define FDVT_LOOP_HW                       (FDVT_BASE_HW + 0x008)
#define FDVT_YUV2RGB_CON_BASE_ADR_HW       (FDVT_BASE_HW + 0x00c)
#define FDVT_RS_CON_BASE_ADR_HW            (FDVT_BASE_HW + 0x010)
#define FDVT_FD_CON_BASE_ADR_HW            (FDVT_BASE_HW + 0x014)
#define FDVT_INT_EN_HW                     (FDVT_BASE_HW + 0x018)
#define FDVT_INT_HW                        (FDVT_BASE_HW + 0x01c)
#define FDVT_YUV2RGB_CON_HW                (FDVT_BASE_HW + 0x020)
#define FDVT_RS_CON_HW                     (FDVT_BASE_HW + 0x024)
#define FDVT_RS_FDRZ_CON0_HW               (FDVT_BASE_HW + 0x028)
#define FDVT_RS_FDRZ_CON1_HW               (FDVT_BASE_HW + 0x02c)
#define FDVT_RS_SRZ_CON0_HW                (FDVT_BASE_HW + 0x030)
#define FDVT_RS_SRZ_CON1_HW                (FDVT_BASE_HW + 0x034)
#define FDVT_RS_SRZ_CON2_HW                (FDVT_BASE_HW + 0x038)
#define FDVT_RS_SRZ_CON3_HW                (FDVT_BASE_HW + 0x03c)
#define FDVT_SRC_WD_HT_HW                  (FDVT_BASE_HW + 0x040)
#define FDVT_DES_WD_HT_HW                  (FDVT_BASE_HW + 0x044)
#define FDVT_CONV_WD_HT_HW                 (FDVT_BASE_HW + 0x048)
#define FDVT_KERNEL_HW                     (FDVT_BASE_HW + 0x04c)
#define FDVT_FD_PACK_MODE_HW               (FDVT_BASE_HW + 0x050)
#define FDVT_CONV0_HW                      (FDVT_BASE_HW + 0x054)
#define FDVT_CONV1_HW                      (FDVT_BASE_HW + 0x058)
#define FDVT_CONV2_HW                      (FDVT_BASE_HW + 0x05c)
#define FDVT_RPN_HW                        (FDVT_BASE_HW + 0x060)
#define FDVT_RPN_IMAGE_COORD_HW            (FDVT_BASE_HW + 0x064)
#define FDVT_FD_ANCHOR_0_HW                (FDVT_BASE_HW + 0x068)
#define FDVT_FD_ANCHOR_1_HW                (FDVT_BASE_HW + 0x06c)
#define FDVT_FD_ANCHOR_2_HW                (FDVT_BASE_HW + 0x070)
#define FDVT_FD_ANCHOR_3_HW                (FDVT_BASE_HW + 0x074)
#define FDVT_FD_ANCHOR_4_HW                (FDVT_BASE_HW + 0x078)
#define FDVT_ANCHOR_SHIFT_MODE_0_HW        (FDVT_BASE_HW + 0x07c)
#define FDVT_ANCHOR_SHIFT_MODE_1_HW        (FDVT_BASE_HW + 0x080)
#define FDVT_LANDMARK_SHIFT_MODE_0_HW      (FDVT_BASE_HW + 0x084)
#define FDVT_LANDMARK_SHIFT_MODE_1_HW      (FDVT_BASE_HW + 0x088)
#define FDVT_RESULT_0_HW                   (FDVT_BASE_HW + 0x08c)
#define FDVT_RESULT_1_HW                   (FDVT_BASE_HW + 0x090)
#define FDVT_DMA_CTL_HW                    (FDVT_BASE_HW + 0x094)
#define FDVT_CTRL_HW                       (FDVT_BASE_HW + 0x098)
#define FDVT_IN_BASE_ADR_0_HW              (FDVT_BASE_HW + 0x09c)
#define FDVT_IN_BASE_ADR_1_HW              (FDVT_BASE_HW + 0x0a0)
#define FDVT_IN_BASE_ADR_2_HW              (FDVT_BASE_HW + 0x0a4)
#define FDVT_IN_BASE_ADR_3_HW              (FDVT_BASE_HW + 0x0a8)
#define FDVT_OUT_BASE_ADR_0_HW             (FDVT_BASE_HW + 0x0ac)
#define FDVT_OUT_BASE_ADR_1_HW             (FDVT_BASE_HW + 0x0b0)
#define FDVT_OUT_BASE_ADR_2_HW             (FDVT_BASE_HW + 0x0b4)
#define FDVT_OUT_BASE_ADR_3_HW             (FDVT_BASE_HW + 0x0b8)
#define FDVT_KERNEL_BASE_ADR_0_HW          (FDVT_BASE_HW + 0x0bc)
#define FDVT_KERNEL_BASE_ADR_1_HW          (FDVT_BASE_HW + 0x0c0)
#define FDVT_IN_SIZE_0_HW                  (FDVT_BASE_HW + 0x0c4)
#define FDVT_IN_STRIDE_0_HW                (FDVT_BASE_HW + 0x0c8)
#define FDVT_IN_SIZE_1_HW                  (FDVT_BASE_HW + 0x0cc)
#define FDVT_IN_STRIDE_1_HW                (FDVT_BASE_HW + 0x0d0)
#define FDVT_IN_SIZE_2_HW                  (FDVT_BASE_HW + 0x0d4)
#define FDVT_IN_STRIDE_2_HW                (FDVT_BASE_HW + 0x0d8)
#define FDVT_IN_SIZE_3_HW                  (FDVT_BASE_HW + 0x0dc)
#define FDVT_IN_STRIDE_3_HW                (FDVT_BASE_HW + 0x0e0)
#define FDVT_OUT_SIZE_0_HW                 (FDVT_BASE_HW + 0x0e4)
#define FDVT_OUT_STRIDE_0_HW               (FDVT_BASE_HW + 0x0e8)
#define FDVT_OUT_SIZE_1_HW                 (FDVT_BASE_HW + 0x0ec)
#define FDVT_OUT_STRIDE_1_HW               (FDVT_BASE_HW + 0x0f0)
#define FDVT_OUT_SIZE_2_HW                 (FDVT_BASE_HW + 0x0f4)
#define FDVT_OUT_STRIDE_2_HW               (FDVT_BASE_HW + 0x0f8)
#define FDVT_OUT_SIZE_3_HW                 (FDVT_BASE_HW + 0x0fc)
#define FDVT_OUT_STRIDE_3_HW               (FDVT_BASE_HW + 0x100)
#define FDVT_KERNEL_SIZE_HW                (FDVT_BASE_HW + 0x104)
#define FDVT_KERNEL_STRIDE_HW              (FDVT_BASE_HW + 0x108)
#define FDVT_DEBUG_INFO_0_HW               (FDVT_BASE_HW + 0x10c)
#define FDVT_DEBUG_INFO_1_HW               (FDVT_BASE_HW + 0x110)
#define FDVT_DEBUG_INFO_2_HW               (FDVT_BASE_HW + 0x114)
#define FDVT_SPARE_CELL_HW                 (FDVT_BASE_HW + 0x118)
#define FDVT_VERSION_HW                    (FDVT_BASE_HW + 0x11c)
#define FDVT_PADDING_CON0_HW               (FDVT_BASE_HW + 0x120)
#define FDVT_PADDING_CON1_HW               (FDVT_BASE_HW + 0x124)
#define DMA_SOFT_RSTSTAT_HW                (FDVT_BASE_HW + 0x200)
#define TDRI_BASE_ADDR_HW                  (FDVT_BASE_HW + 0x204)
#define TDRI_OFST_ADDR_HW                  (FDVT_BASE_HW + 0x208)
#define TDRI_XSIZE_HW                      (FDVT_BASE_HW + 0x20c)
#define VERTICAL_FLIP_EN_HW                (FDVT_BASE_HW + 0x210)
#define DMA_SOFT_RESET_HW                  (FDVT_BASE_HW + 0x214)
#define LAST_ULTRA_EN_HW                   (FDVT_BASE_HW + 0x218)
#define SPECIAL_FUN_EN_HW                  (FDVT_BASE_HW + 0x21c)
#define FDVT_WRA_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x230)
#define FDVT_WRA_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x238)
#define FDVT_WRA_0_XSIZE_HW                (FDVT_BASE_HW + 0x240)
#define FDVT_WRA_0_YSIZE_HW                (FDVT_BASE_HW + 0x244)
#define FDVT_WRA_0_STRIDE_HW               (FDVT_BASE_HW + 0x248)
#define FDVT_WRA_0_CON_HW                  (FDVT_BASE_HW + 0x24c)
#define FDVT_WRA_0_CON2_HW                 (FDVT_BASE_HW + 0x250)
#define FDVT_WRA_0_CON3_HW                 (FDVT_BASE_HW + 0x254)
#define FDVT_WRA_0_CROP_HW                 (FDVT_BASE_HW + 0x258)
#define FDVT_WRA_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x260)
#define FDVT_WRA_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x268)
#define FDVT_WRA_1_XSIZE_HW                (FDVT_BASE_HW + 0x270)
#define FDVT_WRA_1_YSIZE_HW                (FDVT_BASE_HW + 0x274)
#define FDVT_WRA_1_STRIDE_HW               (FDVT_BASE_HW + 0x278)
#define FDVT_WRA_1_CON_HW                  (FDVT_BASE_HW + 0x27c)
#define FDVT_WRA_1_CON2_HW                 (FDVT_BASE_HW + 0x280)
#define FDVT_WRA_1_CON3_HW                 (FDVT_BASE_HW + 0x284)
#define FDVT_WRA_1_CROP_HW                 (FDVT_BASE_HW + 0x288)
#define FDVT_RDA_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x290)
#define FDVT_RDA_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x298)
#define FDVT_RDA_0_XSIZE_HW                (FDVT_BASE_HW + 0x2a0)
#define FDVT_RDA_0_YSIZE_HW                (FDVT_BASE_HW + 0x2a4)
#define FDVT_RDA_0_STRIDE_HW               (FDVT_BASE_HW + 0x2a8)
#define FDVT_RDA_0_CON_HW                  (FDVT_BASE_HW + 0x2ac)
#define FDVT_RDA_0_CON2_HW                 (FDVT_BASE_HW + 0x2b0)
#define FDVT_RDA_0_CON3_HW                 (FDVT_BASE_HW + 0x2b4)
#define FDVT_RDA_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x2c0)
#define FDVT_RDA_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x2c8)
#define FDVT_RDA_1_XSIZE_HW                (FDVT_BASE_HW + 0x2d0)
#define FDVT_RDA_1_YSIZE_HW                (FDVT_BASE_HW + 0x2d4)
#define FDVT_RDA_1_STRIDE_HW               (FDVT_BASE_HW + 0x2d8)
#define FDVT_RDA_1_CON_HW                  (FDVT_BASE_HW + 0x2dc)
#define FDVT_RDA_1_CON2_HW                 (FDVT_BASE_HW + 0x2e0)
#define FDVT_RDA_1_CON3_HW                 (FDVT_BASE_HW + 0x2e4)
#define FDVT_WRB_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x2f0)
#define FDVT_WRB_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x2f8)
#define FDVT_WRB_0_XSIZE_HW                (FDVT_BASE_HW + 0x300)
#define FDVT_WRB_0_YSIZE_HW                (FDVT_BASE_HW + 0x304)
#define FDVT_WRB_0_STRIDE_HW               (FDVT_BASE_HW + 0x308)
#define FDVT_WRB_0_CON_HW                  (FDVT_BASE_HW + 0x30c)
#define FDVT_WRB_0_CON2_HW                 (FDVT_BASE_HW + 0x310)
#define FDVT_WRB_0_CON3_HW                 (FDVT_BASE_HW + 0x314)
#define FDVT_WRB_0_CROP_HW                 (FDVT_BASE_HW + 0x318)
#define FDVT_WRB_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x320)
#define FDVT_WRB_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x328)
#define FDVT_WRB_1_XSIZE_HW                (FDVT_BASE_HW + 0x330)
#define FDVT_WRB_1_YSIZE_HW                (FDVT_BASE_HW + 0x334)
#define FDVT_WRB_1_STRIDE_HW               (FDVT_BASE_HW + 0x338)
#define FDVT_WRB_1_CON_HW                  (FDVT_BASE_HW + 0x33c)
#define FDVT_WRB_1_CON2_HW                 (FDVT_BASE_HW + 0x340)
#define FDVT_WRB_1_CON3_HW                 (FDVT_BASE_HW + 0x344)
#define FDVT_WRB_1_CROP_HW                 (FDVT_BASE_HW + 0x348)
#define FDVT_RDB_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x350)
#define FDVT_RDB_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x358)
#define FDVT_RDB_0_XSIZE_HW                (FDVT_BASE_HW + 0x360)
#define FDVT_RDB_0_YSIZE_HW                (FDVT_BASE_HW + 0x364)
#define FDVT_RDB_0_STRIDE_HW               (FDVT_BASE_HW + 0x368)
#define FDVT_RDB_0_CON_HW                  (FDVT_BASE_HW + 0x36c)
#define FDVT_RDB_0_CON2_HW                 (FDVT_BASE_HW + 0x370)
#define FDVT_RDB_0_CON3_HW                 (FDVT_BASE_HW + 0x374)
#define FDVT_RDB_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x380)
#define FDVT_RDB_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x388)
#define FDVT_RDB_1_XSIZE_HW                (FDVT_BASE_HW + 0x390)
#define FDVT_RDB_1_YSIZE_HW                (FDVT_BASE_HW + 0x394)
#define FDVT_RDB_1_STRIDE_HW               (FDVT_BASE_HW + 0x398)
#define FDVT_RDB_1_CON_HW                  (FDVT_BASE_HW + 0x39c)
#define FDVT_RDB_1_CON2_HW                 (FDVT_BASE_HW + 0x3a0)
#define FDVT_RDB_1_CON3_HW                 (FDVT_BASE_HW + 0x3a4)
#define DMA_ERR_CTRL_HW                    (FDVT_BASE_HW + 0x3b0)
#define FDVT_WRA_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3b4)
#define FDVT_WRA_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3b8)
#define FDVT_WRB_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3bc)
#define FDVT_WRB_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3c0)
#define FDVT_RDA_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3c4)
#define FDVT_RDA_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3c8)
#define FDVT_RDB_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3cc)
#define FDVT_RDB_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3d0)
#define DMA_DEBUG_ADDR_HW                  (FDVT_BASE_HW + 0x3e0)
#define DMA_RSV1_HW                        (FDVT_BASE_HW + 0x3e4)
#define DMA_RSV2_HW                        (FDVT_BASE_HW + 0x3e8)
#define DMA_RSV3_HW                        (FDVT_BASE_HW + 0x3ec)
#define DMA_RSV4_HW                        (FDVT_BASE_HW + 0x3f0)
#define DMA_DEBUG_SEL_HW                   (FDVT_BASE_HW + 0x3f4)
#define DMA_BW_SELF_TEST_HW                (FDVT_BASE_HW + 0x3f8)

#define FDVT_START_REG                     (ISP_FDVT_BASE + 0x000)
#define FDVT_ENABLE_REG                    (ISP_FDVT_BASE + 0x004)
#define FDVT_LOOP_REG                      (ISP_FDVT_BASE + 0x008)
#define FDVT_YUV2RGB_CON_BASE_ADR_REG      (ISP_FDVT_BASE + 0x00c)
#define FDVT_RS_CON_BASE_ADR_REG           (ISP_FDVT_BASE + 0x010)
#define FDVT_FD_CON_BASE_ADR_REG           (ISP_FDVT_BASE + 0x014)
#define FDVT_INT_EN_REG                    (ISP_FDVT_BASE + 0x018)
#define FDVT_INT_REG                       (ISP_FDVT_BASE + 0x01c)
#define FDVT_YUV2RGB_CON_REG               (ISP_FDVT_BASE + 0x020)
#define FDVT_RS_CON_REG                    (ISP_FDVT_BASE + 0x024)
#define FDVT_RS_FDRZ_CON0_REG              (ISP_FDVT_BASE + 0x028)
#define FDVT_RS_FDRZ_CON1_REG              (ISP_FDVT_BASE + 0x02c)
#define FDVT_RS_SRZ_CON0_REG               (ISP_FDVT_BASE + 0x030)
#define FDVT_RS_SRZ_CON1_REG               (ISP_FDVT_BASE + 0x034)
#define FDVT_RS_SRZ_CON2_REG               (ISP_FDVT_BASE + 0x038)
#define FDVT_RS_SRZ_CON3_REG               (ISP_FDVT_BASE + 0x03c)
#define FDVT_SRC_WD_HT_REG                 (ISP_FDVT_BASE + 0x040)
#define FDVT_DES_WD_HT_REG                 (ISP_FDVT_BASE + 0x044)
#define FDVT_CONV_WD_HT_REG                (ISP_FDVT_BASE + 0x048)
#define FDVT_KERNEL_REG                    (ISP_FDVT_BASE + 0x04c)
#define FDVT_FD_PACK_MODE_REG              (ISP_FDVT_BASE + 0x050)
#define FDVT_CONV0_REG                     (ISP_FDVT_BASE + 0x054)
#define FDVT_CONV1_REG                     (ISP_FDVT_BASE + 0x058)
#define FDVT_CONV2_REG                     (ISP_FDVT_BASE + 0x05c)
#define FDVT_RPN_REG                       (ISP_FDVT_BASE + 0x060)
#define FDVT_RPN_IMAGE_COORD_REG           (ISP_FDVT_BASE + 0x064)
#define FDVT_FD_ANCHOR_0_REG               (ISP_FDVT_BASE + 0x068)
#define FDVT_FD_ANCHOR_1_REG               (ISP_FDVT_BASE + 0x06c)
#define FDVT_FD_ANCHOR_2_REG               (ISP_FDVT_BASE + 0x070)
#define FDVT_FD_ANCHOR_3_REG               (ISP_FDVT_BASE + 0x074)
#define FDVT_FD_ANCHOR_4_REG               (ISP_FDVT_BASE + 0x078)
#define FDVT_ANCHOR_SHIFT_MODE_0_REG       (ISP_FDVT_BASE + 0x07c)
#define FDVT_ANCHOR_SHIFT_MODE_1_REG       (ISP_FDVT_BASE + 0x080)
#define FDVT_LANDMARK_SHIFT_MODE_0_REG     (ISP_FDVT_BASE + 0x084)
#define FDVT_LANDMARK_SHIFT_MODE_1_REG     (ISP_FDVT_BASE + 0x088)
#define FDVT_RESULT_0_REG                  (ISP_FDVT_BASE + 0x08c)
#define FDVT_RESULT_1_REG                  (ISP_FDVT_BASE + 0x090)
#define FDVT_DMA_CTL_REG                   (ISP_FDVT_BASE + 0x094)
#define FDVT_CTRL_REG                      (ISP_FDVT_BASE + 0x098)
#define FDVT_IN_BASE_ADR_0_REG             (ISP_FDVT_BASE + 0x09c)
#define FDVT_IN_BASE_ADR_1_REG             (ISP_FDVT_BASE + 0x0a0)
#define FDVT_IN_BASE_ADR_2_REG             (ISP_FDVT_BASE + 0x0a4)
#define FDVT_IN_BASE_ADR_3_REG             (ISP_FDVT_BASE + 0x0a8)
#define FDVT_OUT_BASE_ADR_0_REG            (ISP_FDVT_BASE + 0x0ac)
#define FDVT_OUT_BASE_ADR_1_REG            (ISP_FDVT_BASE + 0x0b0)
#define FDVT_OUT_BASE_ADR_2_REG            (ISP_FDVT_BASE + 0x0b4)
#define FDVT_OUT_BASE_ADR_3_REG            (ISP_FDVT_BASE + 0x0b8)
#define FDVT_KERNEL_BASE_ADR_0_REG         (ISP_FDVT_BASE + 0x0bc)
#define FDVT_KERNEL_BASE_ADR_1_REG         (ISP_FDVT_BASE + 0x0c0)
#define FDVT_IN_SIZE_0_REG                 (ISP_FDVT_BASE + 0x0c4)
#define FDVT_IN_STRIDE_0_REG               (ISP_FDVT_BASE + 0x0c8)
#define FDVT_IN_SIZE_1_REG                 (ISP_FDVT_BASE + 0x0cc)
#define FDVT_IN_STRIDE_1_REG               (ISP_FDVT_BASE + 0x0d0)
#define FDVT_IN_SIZE_2_REG                 (ISP_FDVT_BASE + 0x0d4)
#define FDVT_IN_STRIDE_2_REG               (ISP_FDVT_BASE + 0x0d8)
#define FDVT_IN_SIZE_3_REG                 (ISP_FDVT_BASE + 0x0dc)
#define FDVT_IN_STRIDE_3_REG               (ISP_FDVT_BASE + 0x0e0)
#define FDVT_OUT_SIZE_0_REG                (ISP_FDVT_BASE + 0x0e4)
#define FDVT_OUT_STRIDE_0_REG              (ISP_FDVT_BASE + 0x0e8)
#define FDVT_OUT_SIZE_1_REG                (ISP_FDVT_BASE + 0x0ec)
#define FDVT_OUT_STRIDE_1_REG              (ISP_FDVT_BASE + 0x0f0)
#define FDVT_OUT_SIZE_2_REG                (ISP_FDVT_BASE + 0x0f4)
#define FDVT_OUT_STRIDE_2_REG              (ISP_FDVT_BASE + 0x0f8)
#define FDVT_OUT_SIZE_3_REG                (ISP_FDVT_BASE + 0x0fc)
#define FDVT_OUT_STRIDE_3_REG              (ISP_FDVT_BASE + 0x100)
#define FDVT_KERNEL_SIZE_REG               (ISP_FDVT_BASE + 0x104)
#define FDVT_KERNEL_STRIDE_REG             (ISP_FDVT_BASE + 0x108)
#define FDVT_DEBUG_INFO_0_REG              (ISP_FDVT_BASE + 0x10c)
#define FDVT_DEBUG_INFO_1_REG              (ISP_FDVT_BASE + 0x110)
#define FDVT_DEBUG_INFO_2_REG              (ISP_FDVT_BASE + 0x114)
#define FDVT_SPARE_CELL_REG                (ISP_FDVT_BASE + 0x118)
#define FDVT_VERSION_REG                   (ISP_FDVT_BASE + 0x11c)
#define FDVT_PADDING_CON0_REG              (ISP_FDVT_BASE + 0x120)
#define FDVT_PADDING_CON1_REG              (ISP_FDVT_BASE + 0x124)
#define DMA_SOFT_RSTSTAT_REG               (ISP_FDVT_BASE + 0x200)
#define TDRI_BASE_ADDR_REG                 (ISP_FDVT_BASE + 0x204)
#define TDRI_OFST_ADDR_REG                 (ISP_FDVT_BASE + 0x208)
#define TDRI_XSIZE_REG                     (ISP_FDVT_BASE + 0x20c)
#define VERTICAL_FLIP_EN_REG               (ISP_FDVT_BASE + 0x210)
#define DMA_SOFT_RESET_REG                 (ISP_FDVT_BASE + 0x214)
#define LAST_ULTRA_EN_REG                  (ISP_FDVT_BASE + 0x218)
#define SPECIAL_FUN_EN_REG                 (ISP_FDVT_BASE + 0x21c)
#define FDVT_WRA_0_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x230)
#define FDVT_WRA_0_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x238)
#define FDVT_WRA_0_XSIZE_REG               (ISP_FDVT_BASE + 0x240)
#define FDVT_WRA_0_YSIZE_REG               (ISP_FDVT_BASE + 0x244)
#define FDVT_WRA_0_STRIDE_REG              (ISP_FDVT_BASE + 0x248)
#define FDVT_WRA_0_CON_REG                 (ISP_FDVT_BASE + 0x24c)
#define FDVT_WRA_0_CON2_REG                (ISP_FDVT_BASE + 0x250)
#define FDVT_WRA_0_CON3_REG                (ISP_FDVT_BASE + 0x254)
#define FDVT_WRA_0_CROP_REG                (ISP_FDVT_BASE + 0x258)
#define FDVT_WRA_1_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x260)
#define FDVT_WRA_1_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x268)
#define FDVT_WRA_1_XSIZE_REG               (ISP_FDVT_BASE + 0x270)
#define FDVT_WRA_1_YSIZE_REG               (ISP_FDVT_BASE + 0x274)
#define FDVT_WRA_1_STRIDE_REG              (ISP_FDVT_BASE + 0x278)
#define FDVT_WRA_1_CON_REG                 (ISP_FDVT_BASE + 0x27c)
#define FDVT_WRA_1_CON2_REG                (ISP_FDVT_BASE + 0x280)
#define FDVT_WRA_1_CON3_REG                (ISP_FDVT_BASE + 0x284)
#define FDVT_WRA_1_CROP_REG                (ISP_FDVT_BASE + 0x288)
#define FDVT_RDA_0_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x290)
#define FDVT_RDA_0_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x298)
#define FDVT_RDA_0_XSIZE_REG               (ISP_FDVT_BASE + 0x2a0)
#define FDVT_RDA_0_YSIZE_REG               (ISP_FDVT_BASE + 0x2a4)
#define FDVT_RDA_0_STRIDE_REG              (ISP_FDVT_BASE + 0x2a8)
#define FDVT_RDA_0_CON_REG                 (ISP_FDVT_BASE + 0x2ac)
#define FDVT_RDA_0_CON2_REG                (ISP_FDVT_BASE + 0x2b0)
#define FDVT_RDA_0_CON3_REG                (ISP_FDVT_BASE + 0x2b4)
#define FDVT_RDA_1_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x2c0)
#define FDVT_RDA_1_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x2c8)
#define FDVT_RDA_1_XSIZE_REG               (ISP_FDVT_BASE + 0x2d0)
#define FDVT_RDA_1_YSIZE_REG               (ISP_FDVT_BASE + 0x2d4)
#define FDVT_RDA_1_STRIDE_REG              (ISP_FDVT_BASE + 0x2d8)
#define FDVT_RDA_1_CON_REG                 (ISP_FDVT_BASE + 0x2dc)
#define FDVT_RDA_1_CON2_REG                (ISP_FDVT_BASE + 0x2e0)
#define FDVT_RDA_1_CON3_REG                (ISP_FDVT_BASE + 0x2e4)
#define FDVT_WRB_0_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x2f0)
#define FDVT_WRB_0_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x2f8)
#define FDVT_WRB_0_XSIZE_REG               (ISP_FDVT_BASE + 0x300)
#define FDVT_WRB_0_YSIZE_REG               (ISP_FDVT_BASE + 0x304)
#define FDVT_WRB_0_STRIDE_REG              (ISP_FDVT_BASE + 0x308)
#define FDVT_WRB_0_CON_REG                 (ISP_FDVT_BASE + 0x30c)
#define FDVT_WRB_0_CON2_REG                (ISP_FDVT_BASE + 0x310)
#define FDVT_WRB_0_CON3_REG                (ISP_FDVT_BASE + 0x314)
#define FDVT_WRB_0_CROP_REG                (ISP_FDVT_BASE + 0x318)
#define FDVT_WRB_1_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x320)
#define FDVT_WRB_1_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x328)
#define FDVT_WRB_1_XSIZE_REG               (ISP_FDVT_BASE + 0x330)
#define FDVT_WRB_1_YSIZE_REG               (ISP_FDVT_BASE + 0x334)
#define FDVT_WRB_1_STRIDE_REG              (ISP_FDVT_BASE + 0x338)
#define FDVT_WRB_1_CON_REG                 (ISP_FDVT_BASE + 0x33c)
#define FDVT_WRB_1_CON2_REG                (ISP_FDVT_BASE + 0x340)
#define FDVT_WRB_1_CON3_REG                (ISP_FDVT_BASE + 0x344)
#define FDVT_WRB_1_CROP_REG                (ISP_FDVT_BASE + 0x348)
#define FDVT_RDB_0_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x350)
#define FDVT_RDB_0_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x358)
#define FDVT_RDB_0_XSIZE_REG               (ISP_FDVT_BASE + 0x360)
#define FDVT_RDB_0_YSIZE_REG               (ISP_FDVT_BASE + 0x364)
#define FDVT_RDB_0_STRIDE_REG              (ISP_FDVT_BASE + 0x368)
#define FDVT_RDB_0_CON_REG                 (ISP_FDVT_BASE + 0x36c)
#define FDVT_RDB_0_CON2_REG                (ISP_FDVT_BASE + 0x370)
#define FDVT_RDB_0_CON3_REG                (ISP_FDVT_BASE + 0x374)
#define FDVT_RDB_1_BASE_ADDR_REG           (ISP_FDVT_BASE + 0x380)
#define FDVT_RDB_1_OFST_ADDR_REG           (ISP_FDVT_BASE + 0x388)
#define FDVT_RDB_1_XSIZE_REG               (ISP_FDVT_BASE + 0x390)
#define FDVT_RDB_1_YSIZE_REG               (ISP_FDVT_BASE + 0x394)
#define FDVT_RDB_1_STRIDE_REG              (ISP_FDVT_BASE + 0x398)
#define FDVT_RDB_1_CON_REG                 (ISP_FDVT_BASE + 0x39c)
#define FDVT_RDB_1_CON2_REG                (ISP_FDVT_BASE + 0x3a0)
#define FDVT_RDB_1_CON3_REG                (ISP_FDVT_BASE + 0x3a4)
#define DMA_ERR_CTRL_REG                   (ISP_FDVT_BASE + 0x3b0)
#define FDVT_WRA_0_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3b4)
#define FDVT_WRA_1_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3b8)
#define FDVT_WRB_0_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3bc)
#define FDVT_WRB_1_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3c0)
#define FDVT_RDA_0_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3c4)
#define FDVT_RDA_1_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3c8)
#define FDVT_RDB_0_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3cc)
#define FDVT_RDB_1_ERR_STAT_REG            (ISP_FDVT_BASE + 0x3d0)
#define DMA_DEBUG_ADDR_REG                 (ISP_FDVT_BASE + 0x3e0)
#define DMA_RSV1_REG                       (ISP_FDVT_BASE + 0x3e4)
#define DMA_RSV2_REG                       (ISP_FDVT_BASE + 0x3e8)
#define DMA_RSV3_REG                       (ISP_FDVT_BASE + 0x3ec)
#define DMA_RSV4_REG                       (ISP_FDVT_BASE + 0x3f0)
#define DMA_DEBUG_SEL_REG                  (ISP_FDVT_BASE + 0x3f4)
#define DMA_BW_SELF_TEST_REG               (ISP_FDVT_BASE + 0x3f8)

/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int fdvt_ms_to_jiffies(unsigned int ms)
{
	return ((ms * HZ + 512) >> 10);
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int fdvt_us_to_jiffies(unsigned int us)
{
	return (((us / 1000) * HZ + 512) >> 10);
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int fdvt_get_irq_state(unsigned int type,
					      unsigned int user_number,
					      unsigned int stus,
					      enum FDVT_PROCESS_ID_ENUM
							which_req,
					      int process_id)
{
	unsigned int ret = 0;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;

	spin_lock_irqsave(&fdvt_info.spinlock_irq[type], flags);
#ifdef FDVT_USE_GCE

#ifdef FDVT_MULTIPROCESS_TIMEING_ISSUE
	if (stus & FDVT_INT_ST) {
		ret = ((fdvt_info.irq_info.fdvt_irq_cnt > 0) &&
		       (fdvt_info.process_id[fdvt_info.read_req_idx] ==
				process_id));
	} else {
		log_err("WaitIRQ StatusErr, type:%d, userNum:%d, status:%d, which_req:%d,process_id:0x%x, read_req_idx:%d\n",
			type, user_number, stus, which_req, process_id,
			fdvt_info.read_req_idx);
	}

#else /*FDVT_MULTIPROCESS_TIMEING_ISSUE*/
	if (stus & FDVT_INT_ST) {
		ret = (fdvt_info.irq_info.fdvt_irq_cnt > 0 &&
		       fdvt_info.irq_info.process_id[which_req] ==
				process_id);
	} else {
		log_err("WaitIRQ status Error, type:%d, user_number:%d, status:%d, which_req:%d, process_id:0x%x\n",
			type, user_number, stus, which_req, process_id);
	}
#endif /* FDVT_MULTIPROCESS_TIMEING_ISSUE */
#else /* FDVT_USE_GCE */
	ret = ((fdvt_info.irq_info.status[type] & stus) &&
	      (fdvt_info.irq_info.process_id[which_req] == process_id));
#endif /* FDVT_USE_GCE */
	spin_unlock_irqrestore(&fdvt_info.spinlock_irq[type], flags);

	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int fdvt_jiffies_to_ms(unsigned int jiffies)
{
	return ((jiffies * 1000) / HZ);
}

#define dump_reg(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		log_dbg("[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",\
		(unsigned int)(ISP_FDVT_BASE + i),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i),\
		(unsigned int)(ISP_FDVT_BASE + i + 0x4),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i + 0x4),\
		(unsigned int)(ISP_FDVT_BASE + i + 0x8),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i + 0x8),\
		(unsigned int)(ISP_FDVT_BASE + i + 0xc),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i + 0xc));\
	} \
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline void fdvt_reset(void)
{
	log_dbg("- E.");

	log_dbg(" FDVT Reset start!\n");
	spin_lock(&fdvt_info.spinlock_fdvt_ref);

	if (fdvt_info.user_count > 1) {
		spin_unlock(&fdvt_info.spinlock_fdvt_ref);
		log_dbg("Curr user_count(%d) users exist", fdvt_info.user_count);
	} else {
		spin_unlock(&fdvt_info.spinlock_fdvt_ref);

		/* Reset FDVT flow */
		#if 0
		FDVT_WR32(FDVT_DMA_CTL_REG, 0x11111111);
		#endif
		log_dbg("FDVT Reset skip FDVT_DMA_CTL workaround!\n");
		FDVT_WR32(FDVT_START_REG, FDVT_RD32(FDVT_START_REG) | 0x20000);
		while (((FDVT_RD32(FDVT_START_REG) & 0x20000) != 0x0))
			log_dbg("FDVT resetting...\n");
		FDVT_WR32(FDVT_START_REG, 0x30000);
		FDVT_WR32(FDVT_START_REG, 0x0);
		log_dbg(" FDVT Reset end!\n");
	}
}

static inline void fdvt_reset_every_frame(void)
{
	log_dbg("- E.");

	log_dbg(" FDVT Reset Every Frame start!\n");

	/* Reset FDVT flow */
	#if 0
	FDVT_WR32(FDVT_DMA_CTL_REG, 0x11111111);
	#endif
	log_dbg("FDVT Reset skip FDVT_DMA_CTL workaround!\n");
	FDVT_WR32(FDVT_START_REG, FDVT_RD32(FDVT_START_REG) | 0x20000);
	while (((FDVT_RD32(FDVT_START_REG) & 0x20000) != 0x0))
		log_dbg("FDVT resetting...\n");
	FDVT_WR32(FDVT_START_REG, 0x30000);
	FDVT_WR32(FDVT_START_REG, 0x0);
	log_dbg(" FDVT Reset Every Frame end!\n");
}

/*****************************************************************************
 *
 *****************************************************************************/

static bool config_fdvt_request(signed int req_idx)
{
#ifdef FDVT_USE_GCE
	unsigned int j;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;
	struct FDVT_REQUEST_STRUCT *request;
	spinlock_t *spinlock_lrq_ptr; /* spinlock for irq */

	request = &fdvt_req_ring.req_struct[req_idx];
	spinlock_lrq_ptr = &fdvt_info.spinlock_irq[FDVT_IRQ_TYPE_INT_FDVT_ST];
	spin_lock_irqsave(spinlock_lrq_ptr, flags);
	if (request->state ==
		FDVT_REQUEST_STATE_PENDING) {
		request->state =
		FDVT_REQUEST_STATE_RUNNING;
		for (j = 0; j < MAX_FDVT_FRAME_REQUEST; j++) {
			if (FDVT_FRAME_STATUS_ENQUE
				== request->fdvt_frame_status[j]) {
				request->fdvt_frame_status[j] =
					FDVT_FRAME_STATUS_RUNNING;
				spin_unlock_irqrestore(spinlock_lrq_ptr, flags);
				if (request->frame_config[j].FDVT_IS_SECURE)
					config_secure_fdvt_hw(&request->frame_config[j]);
				else
					config_fdvt_hw(&request->frame_config[j]);
				spin_lock_irqsave(spinlock_lrq_ptr, flags);
			}
		}
	} else {
		log_err("FDVT state machine error!!, req_idx:%d, state:%d\n",
			req_idx, request->state);
	}
	spin_unlock_irqrestore(spinlock_lrq_ptr, flags);

	return MTRUE;
#else /* FDVT_USE_GCE */
	log_err("[%s] don't support this mode.!!\n", __func__);
	return MFALSE;
#endif /* FDVT_USE_GCE */
}

static bool config_fdvt(void)
{
	unsigned int i, j, k;
	struct FDVT_REQUEST_STRUCT *request;
	signed int *hw_process_idx;

#ifdef FDVT_USE_GCE

	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;
	spinlock_t *spinlock_lrq_ptr; /* spinlock for irq */

	spinlock_lrq_ptr = &fdvt_info.spinlock_irq[FDVT_IRQ_TYPE_INT_FDVT_ST];
	hw_process_idx = &fdvt_req_ring.hw_process_idx;

	spin_lock_irqsave(spinlock_lrq_ptr, flags);
	for (k = 0; k < MAX_FDVT_REQUEST_RING_SIZE; k++) {
		i = (*hw_process_idx + k) %
			MAX_FDVT_REQUEST_RING_SIZE;
		request = &fdvt_req_ring.req_struct[i];
		if (request->state == FDVT_REQUEST_STATE_PENDING) {
			request->state = FDVT_REQUEST_STATE_RUNNING;
			for (j = 0; j < MAX_FDVT_FRAME_REQUEST; j++) {
				if (FDVT_FRAME_STATUS_ENQUE ==
					request->fdvt_frame_status[j]) {
					/* break; */
					request->fdvt_frame_status[j] =
						FDVT_FRAME_STATUS_RUNNING;
					spin_unlock_irqrestore(spinlock_lrq_ptr,
							       flags);
					if (request->frame_config[j].FDVT_IS_SECURE) {
						config_secure_fdvt_hw(
							&request->frame_config[j]);
					} else {
						config_fdvt_hw(
							&request->frame_config[j]);
					}
					spin_lock_irqsave(spinlock_lrq_ptr,
							  flags);
				}
			}
			/* log_dbg("config_fdvt idx j:%d\n",j); */
			if (j != MAX_FDVT_FRAME_REQUEST) {
				log_err(
				"FDVT Config state is wrong! idx j(%d), hw_process_idx(%d), state(%d)\n",
				j, *hw_process_idx, request->state);
				/* request->
				 * fdvt_frame_status[j] =
				 * FDVT_FRAME_STATUS_RUNNING;
				 * spin_unlock_irqrestore(&
				 * (fdvt_info.
				 * spinlock_irq[FDVT_IRQ_TYPE_INT_FDVT_ST]),
				 * flags);
				 * config_fdvt_hw(&fdvt_req_ring.
				 * req_struct[i].frame_config[j]);
				 * return MTRUE;
				 */
				return MFALSE;
			}
			/* else {
			 * request->state =
			 * FDVT_REQUEST_STATE_RUNNING;
			 * log_err(
			 * "FDVT Config state is wrong!
			 * hw_process_idx(%d), state(%d)\n",
			 * *hw_process_idx,
			 * request->state);
			 * *hw_process_idx =
			 * (*hw_process_idx+1)
			 * %MAX_FDVT_REQUEST_RING_SIZE;
			 * }
			 */
		}
	}
	spin_unlock_irqrestore(
		spinlock_lrq_ptr,
		flags);
	if (k == MAX_FDVT_REQUEST_RING_SIZE)
		log_dbg("No any FDVT Request in Ring!!\n");

	return MTRUE;

#else /* FDVT_USE_GCE */
	//unsigned int flags;

	for (k = 0; k < MAX_FDVT_REQUEST_RING_SIZE; k++) {
		i = (*hw_process_idx + k) %
			MAX_FDVT_REQUEST_RING_SIZE;
		request = &fdvt_req_ring.req_struct[i];
		if (request->state == FDVT_REQUEST_STATE_PENDING) {
			for (j = 0; j < MAX_FDVT_FRAME_REQUEST; j++) {
				if (request->fdvt_frame_status[j] ==
				    FDVT_FRAME_STATUS_ENQUE)
					break;
			}

			log_dbg("%s idx j:%d\n", __func__, j);

			if (j != MAX_FDVT_FRAME_REQUEST) {
				request->fdvt_frame_status[j] =
					FDVT_FRAME_STATUS_RUNNING;
				if (request->frame_config[j].FDVT_IS_SECURE)
					config_secure_fdvt_hw(&request->frame_config[j]);
				else
					config_fdvt_hw(&request->frame_config[j]);
				return MTRUE;
			}
			log_err("FDVT Config state is wrong! hw_process_idx(%d), state(%d)\n",
				*hw_process_idx,
				request->state);
			*hw_process_idx =
				(*hw_process_idx + 1) %
				MAX_FDVT_REQUEST_RING_SIZE;
		}
	}

	if (k == MAX_FDVT_REQUEST_RING_SIZE)
		log_dbg("No any FDVT Request in Ring!!\n");

	return MFALSE;
#endif /* FDVT_USE_GCE */
}

static bool update_fdvt(pid_t *process_id)
{
	unsigned int i, j, next_idx;
	struct FDVT_REQUEST_STRUCT *request;
	signed int *hw_process_idx;
	bool bFinishRequest = MFALSE;

#ifdef FDVT_USE_GCE
	hw_process_idx = &fdvt_req_ring.hw_process_idx;

	for (i = *hw_process_idx;
	     i < MAX_FDVT_REQUEST_RING_SIZE; i++) {
		request = &fdvt_req_ring.req_struct[i];
		if (request->state == FDVT_REQUEST_STATE_RUNNING) {
			for (j = 0;
			     j < MAX_FDVT_FRAME_REQUEST; j++) {
				if (FDVT_FRAME_STATUS_RUNNING ==
					request->fdvt_frame_status[j]) {
					break;
				}
			}
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
				       m_CurrentPPB, _LOG_DBG,
				       "%s idx j:%d\n", __func__, j);
			if (j != MAX_FDVT_FRAME_REQUEST) {
				next_idx = j + 1;
				request->fdvt_frame_status[j] =
					FDVT_FRAME_STATUS_FINISHED;
				request->frame_config[j].RESULT =
					FDVT_RD32(FDVT_RESULT_0_REG);
				request->frame_config[j].RESULT1 =
					FDVT_RD32(FDVT_RESULT_1_REG);
				//fdvt_reset_every_frame();
				if (MAX_FDVT_FRAME_REQUEST
					== next_idx ||
				    (MAX_FDVT_FRAME_REQUEST
					> next_idx &&
				     FDVT_FRAME_STATUS_EMPTY ==
				     request->fdvt_frame_status[next_idx])) {
					fdvt_reset_every_frame();
					bFinishRequest = MTRUE;
					(*process_id) = request->process_id;
					request->state =
						FDVT_REQUEST_STATE_FINISHED;

					*hw_process_idx =
						(*hw_process_idx + 1) %
						MAX_FDVT_REQUEST_RING_SIZE;

					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_INF,
						"Finish FDVT Request i:%d, j:%d, hw_process_idx:%d\n",
						i, j,
						*hw_process_idx);
				} else {
					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_DBG,
						"Finish FDVT Frame i:%d, j:%d, hw_process_idx:%d\n",
						i, j,
						*hw_process_idx);
				}
				break;
			}
			/*else {*/
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
				       m_CurrentPPB, _LOG_ERR,
				       "FDVT state Machine is wrong! hw_process_idx(%d), state(%d)\n",
				       *hw_process_idx, request->state);
			request->state = FDVT_REQUEST_STATE_FINISHED;
			*hw_process_idx =
				(*hw_process_idx + 1)
				% MAX_FDVT_REQUEST_RING_SIZE;
			break;
			/*}*/
		}
	}

	return bFinishRequest;

#else /* FDVT_USE_GCE */
	hw_process_idx = &fdvt_req_ring.hw_process_idx;

	for (i = *hw_process_idx;
		i < MAX_FDVT_REQUEST_RING_SIZE; i++) {
		request = &fdvt_req_ring.req_struct[i];
		if (request->state == FDVT_REQUEST_STATE_PENDING) {
			for (j = 0; j < MAX_FDVT_FRAME_REQUEST; j++) {
				if (FDVT_FRAME_STATUS_RUNNING ==
				    request->fdvt_frame_status[j])
					break;
			}
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB,
				       _LOG_DBG, "%s idx j:%d\n", __func__, j);
			if (j != MAX_FDVT_FRAME_REQUEST) {
				next_idx = j + 1;
				request->fdvt_frame_status[j] =
					FDVT_FRAME_STATUS_FINISHED;

				if (next_idx == MAX_FDVT_FRAME_REQUEST ||
				    (next_idx < MAX_FDVT_FRAME_REQUEST &&
				    request->fdvt_frame_status[next_idx] ==
				    FDVT_FRAME_STATUS_EMPTY)) {
					bFinishRequest = MTRUE;
					(*process_id) = request->process_id;
					request->state =
						FDVT_REQUEST_STATE_FINISHED;
					*hw_process_idx =
						(*hw_process_idx + 1) %
						MAX_FDVT_REQUEST_RING_SIZE;
					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_INF,
						"Finish FDVT Request i:%d, j:%d, hw_process_idx:%d\n",
						i, j,
						*hw_process_idx);
				} else {
					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_DBG,
						"Finish FDVT Frame i:%d, j:%d, hw_process_idx:%d\n",
						i, j,
						*hw_process_idx);
				}
				break;
			}
			/*else {*/
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
				       m_CurrentPPB,
				       _LOG_ERR,
				       "FDVT state Machine is wrong! hw_process_idx(%d), state(%d)\n",
				       *hw_process_idx,
				       request->state);
			request->state = FDVT_REQUEST_STATE_FINISHED;
			*hw_process_idx =
				(*hw_process_idx + 1) %
					MAX_FDVT_REQUEST_RING_SIZE;
			break;
			/*}*/
		}
	}

	return bFinishRequest;
#endif /* FDVT_USE_GCE */
}

static signed int config_fdvt_hw(struct fdvt_config *basic_config)
#if !BYPASS_REG
{
#ifdef FDVT_USE_GCE
#ifdef CMDQ_MAIL_BOX
	struct cmdq_pkt *pkt;
#else /* CMDQ_MAIL_BOX */
	struct cmdqRecStruct *handle;
	int64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_FDVT);
#endif /* CMDQ_MAIL_BOX */
#endif /* FDVT_USE_GCE */
	if (FDVT_DBG_DBGLOG == (FDVT_DBG_DBGLOG & fdvt_info.debug_mask)) {
		log_dbg("config_fdvt_hw Start!\n");
		log_dbg("FDVT_YUV2RGB:0x%x!\n",
			(unsigned int)basic_config->FDVT_YUV2RGB);
		log_dbg("FDVT_YUV_SRC_WD_HT:0x%x!\n",
			(unsigned int)basic_config->FDVT_YUV_SRC_WD_HT);
		log_dbg("FDVT_RSCON_BASE_ADR:0x%x!\n",
			(unsigned int)basic_config->FDVT_RSCON_BASE_ADR);
		log_dbg("FDVT_FD_CON_BASE_ADR:0x%x!\n",
			(unsigned int)basic_config->FDVT_FD_CON_BASE_ADR);
		log_dbg("FDVT_YUV2RGBCON_BASE_ADR:0x%x!\n",
			(unsigned int)basic_config->FDVT_YUV2RGBCON_BASE_ADR);
		log_dbg("FD_MODE:0x%x!\n",
			(unsigned int)basic_config->FD_MODE);
	}

#ifdef FDVT_USE_GCE
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("config_fdvt_hw");
#endif
#ifdef CMDQ_MAIL_BOX
	pkt = cmdq_pkt_create(fdvt_clt);
#else /* CMDQ_MAIL_BOX */
	cmdqRecCreate(CMDQ_SCENARIO_ISP_FDVT, &handle);
	/* CMDQ driver dispatches CMDQ HW thread
	 * and HW thread's priority according to scenario
	 */
	cmdqRecSetEngine(handle, engineFlag);
	/* Use command queue to write register */
	/* BIT0 for INT_EN */
#endif /* CMDQ_MAIL_BOX */

#if 0
	cmdqRecWrite(handle, FDVT_WRA_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_WRA_1_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDA_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDA_1_CON3_HW, 0x0, CMDQ_REG_MASK);

	cmdqRecWrite(handle, FDVT_WRB_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_WRB_1_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDB_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDB_1_CON3_HW, 0x0, CMDQ_REG_MASK);
#endif

#ifdef CMDQ_MAIL_BOX
	log_dbg("fdvt use cmdq mail box api\n");
	if (basic_config->FD_MODE == 0) {
		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000111,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00006002,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x0, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_RS_CON_BASE_ADR_HW,
			       basic_config->FDVT_RSCON_BASE_ADR,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_FD_CON_BASE_ADR_HW,
			       basic_config->FDVT_FD_CON_BASE_ADR,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_YUV2RGB_CON_BASE_ADR_HW,
			       basic_config->FDVT_YUV2RGBCON_BASE_ADR,
			       CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000100,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00000300,
			       CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_FD_CON_BASE_ADR_HW,
			       basic_config->FDVT_FD_POSE_CON_BASE_ADR,
			       CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	} else if (basic_config->FD_MODE == 1) {
		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000101,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00001A00,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_RS_CON_BASE_ADR_HW,
			       basic_config->FDVT_RSCON_BASE_ADR,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_FD_CON_BASE_ADR_HW,
			       basic_config->FDVT_FD_CON_BASE_ADR,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_YUV2RGB_CON_BASE_ADR_HW,
			       basic_config->FDVT_YUV2RGBCON_BASE_ADR,
			       CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	} else if (basic_config->FD_MODE == 2) {
		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000101,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00001200,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_RS_CON_BASE_ADR_HW,
			       basic_config->FDVT_RSCON_BASE_ADR,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_FD_CON_BASE_ADR_HW,
			       basic_config->FDVT_FD_CON_BASE_ADR,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_YUV2RGB_CON_BASE_ADR_HW,
			       basic_config->FDVT_YUV2RGBCON_BASE_ADR,
			       CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);
	}

	/* non-blocking API, Please use cmdqRecFlushAsync() */
	log_dbg("FDVT CMDQ Task flush\n");

	cmdq_pkt_flush(pkt);
	/* release resource */
	cmdq_pkt_destroy(pkt);
#else /* CMDQ_MAIL_BOX */
	if (basic_config->FD_MODE == 0) {
		cmdqRecWrite(handle, FDVT_ENABLE_HW, 0x00000111,
			     CMDQ_REG_MASK);
		cmdqRecWrite(handle, FDVT_LOOP_HW, 0x00004202, CMDQ_REG_MASK);
	} else if (basic_config->FD_MODE == 1) {
		cmdqRecWrite(handle, FDVT_ENABLE_HW, 0x00000101,
			     CMDQ_REG_MASK);
		cmdqRecWrite(handle, FDVT_LOOP_HW, 0x00001200, CMDQ_REG_MASK);
	} else if (basic_config->FD_MODE == 2) {
		cmdqRecWrite(handle, FDVT_ENABLE_HW, 0x00000101,
			     CMDQ_REG_MASK);
		cmdqRecWrite(handle, FDVT_LOOP_HW, 0x00001200, CMDQ_REG_MASK);
	}
	cmdqRecWrite(handle, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RS_CON_BASE_ADR_HW,
		 basic_config->FDVT_RSCON_BASE_ADR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_FD_CON_BASE_ADR_HW,
		 basic_config->FDVT_FD_CON_BASE_ADR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_YUV2RGB_CON_BASE_ADR_HW,
		 basic_config->FDVT_YUV2RGBCON_BASE_ADR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, FDVT_START_HW, 0x1, CMDQ_REG_MASK);
	cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);
	cmdqRecWrite(handle, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	/* non-blocking API, Please use cmdqRecFlushAsync() */
	log_dbg("FDVT CMDQ Task flush\n");
	cmdq_task_flush_async_destroy(handle);  /* flush and destroy in cmdq */
	//fdvt_dump_reg(); // ADD by gasper
#endif /* CMDQ_MAIL_BOX */
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#else

#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("config_fdvt_hw");
#endif
#if 0
	/* FDVT Interrupt enabled in read-clear mode */
	FDVT_WR32(FDVT_INT_EN_REG, 0x1);
	FDVT_WR32(FDVT_ENABLE_REG, 0x00000111);
	FDVT_WR32(FDVT_RS_REG, 0x00000409);
	FDVT_WR32(FDVT_YUV2RGB_REG, basic_config->FDVT_YUV2RGB);
	FDVT_WR32(FDVT_FD_REG, 0x04000042);
	FDVT_WR32(FDVT_YUV_SRC_WD_HT_REG, basic_config->FDVT_YUV_SRC_WD_HT);
	FDVT_WR32(FDVT_RSCON_BASE_ADR_REG, basic_config->FDVT_RSCON_BASE_ADR);
	FDVT_WR32(FDVT_FD_CON_BASE_ADR_REG, basic_config->FDVT_FD_CON_BASE_ADR);
	FDVT_WR32(FDVT_YUV2RGBCON_BASE_ADR_REG,
		  basic_config->FDVT_YUV2RGBCON_BASE_ADR);
	FDVT_WR32(FDVT_FD_RLT_BASE_ADR_REG, NULL);

	FDVT_WR32(FDVT_START_REG, 0x1);	/* FDVT Interrupt read-clear mode */
#endif
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#endif
	return 0;
}
#else
{
	return 0;
}
#endif

static signed int config_secure_fdvt_hw(struct fdvt_config *basic_config)
#if !BYPASS_REG
{
#if IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)

#ifdef FDVT_USE_GCE
	struct cmdq_pkt *pkt;
#endif /* FDVT_USE_GCE */
	if (FDVT_DBG_DBGLOG == (FDVT_DBG_DBGLOG & fdvt_info.debug_mask)) {
		log_dbg("config_secure_fdvt_hw Start!\n");
		log_dbg("FDVT_YUV2RGB:0x%x!\n",
			(unsigned int)basic_config->FDVT_YUV2RGB);
		log_dbg("FDVT_YUV_SRC_WD_HT:0x%x!\n",
			(unsigned int)basic_config->FDVT_YUV_SRC_WD_HT);
		log_dbg("FDVT_RSCON_BASE_ADR:0x%x!\n",
			(unsigned int)basic_config->FDVT_RSCON_BASE_ADR);
		log_dbg("FDVT_FD_CON_BASE_ADR:0x%x!\n",
			(unsigned int)basic_config->FDVT_FD_CON_BASE_ADR);
		log_dbg("FDVT_YUV2RGBCON_BASE_ADR:0x%x!\n",
			(unsigned int)basic_config->FDVT_YUV2RGBCON_BASE_ADR);
		log_dbg("FD_MODE:0x%x!\n",
			(unsigned int)basic_config->FD_MODE);
	}

#ifdef FDVT_USE_GCE
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("config_secure_fdvt_hw");
#endif
	pkt = cmdq_pkt_create(fdvt_secure_clt);


#if 0
	if (basic_config->FDVT_IS_SECURE != 0)
		cmdq_sec_pkt_set_data(pkt,
			1LL << CMDQ_SEC_FDVT,
			1LL << CMDQ_SEC_FDVT,
			CMDQ_SEC_ISP_FDVT,
			CMDQ_METAEX_FD);
#else
	if (basic_config->FDVT_IS_SECURE != 0) {
		cmdq_sec_pkt_set_data(pkt,
			1LL << CMDQ_SEC_FDVT,
			1LL << CMDQ_SEC_FDVT,
			CMDQ_SEC_ISP_FDVT,
			CMDQ_METAEX_FD);
#ifdef CMDQ_MTEE
		cmdq_sec_pkt_set_mtee(pkt, true);
		if (atomic_cmpxchg(&m4u_gz_init, 0, 1) == 0)
			m4u_gz_sec_init(0);
#endif
	}
#endif

#if 0
	cmdqRecWrite(handle, FDVT_WRA_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_WRA_1_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDA_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDA_1_CON3_HW, 0x0, CMDQ_REG_MASK);

	cmdqRecWrite(handle, FDVT_WRB_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_WRB_1_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDB_0_CON3_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_RDB_1_CON3_HW, 0x0, CMDQ_REG_MASK);
#endif

	log_dbg("fdvt use cmdq mail box api\n");

	log_dbg("MetaData->FDMode: %d\n", basic_config->FDVT_METADATA_TO_GCE.FDMode);
	log_dbg("MetaData->srcImgFmt: %d\n", basic_config->FDVT_METADATA_TO_GCE.srcImgFmt);
	log_dbg("MetaData->srcImgWidth: %d\n", basic_config->FDVT_METADATA_TO_GCE.srcImgWidth);
	log_dbg("MetaData->srcImgHeight: %d\n", basic_config->FDVT_METADATA_TO_GCE.srcImgHeight);
	log_dbg("MetaData->rotateDegree: %d\n", basic_config->FDVT_METADATA_TO_GCE.rotateDegree);
	log_dbg("MetaData->featureTH: %d\n", basic_config->FDVT_METADATA_TO_GCE.featureTH);
	log_dbg("MetaData->ImgSrcY_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.ImgSrcY_Handler);
	log_dbg("MetaData->ImgSrcUV_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.ImgSrcUV_Handler);
	log_dbg("MetaData->YUVConfig_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.YUVConfig_Handler);
	log_dbg("MetaData->RSConfig_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.RSConfig_Handler);
	log_dbg("MetaData->RSOutBuf_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.RSOutBuf_Handler);
	log_dbg("MetaData->FDConfig_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.FDConfig_Handler);
	log_dbg("MetaData->FD_POSE_Config_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.FD_POSE_Config_Handler);
	log_dbg("MetaData->FDOutBuf_Handler: %x\n", basic_config->FDVT_METADATA_TO_GCE.FDOutBuf_Handler);
	log_dbg("MetaData->FDResultBuf_MVA: %x\n", basic_config->FDVT_METADATA_TO_GCE.FDResultBuf_MVA);
	log_dbg("MetaData->YUVConfigSize: %x\n", basic_config->FDVT_METADATA_TO_GCE.YUVConfigSize);
	log_dbg("MetaData->YUVOutBufSize: %x\n", basic_config->FDVT_METADATA_TO_GCE.YUVOutBufSize);
	log_dbg("MetaData->RSConfigSize: %x\n", basic_config->FDVT_METADATA_TO_GCE.RSConfigSize);
	log_dbg("MetaData->RSOutBufSize: %x\n", basic_config->FDVT_METADATA_TO_GCE.RSOutBufSize);
	log_dbg("MetaData->FDConfigSize: %x\n", basic_config->FDVT_METADATA_TO_GCE.FDConfigSize);
	log_dbg("MetaData->FDOutBufSize: %x\n", basic_config->FDVT_METADATA_TO_GCE.FDOutBufSize);
	log_dbg("MetaData->FDResultBufSize: %x\n", basic_config->FDVT_METADATA_TO_GCE.FDResultBufSize);
	log_dbg("MetaData->SecMemType: %d\n", basic_config->FDVT_METADATA_TO_GCE.SecMemType);
	if (basic_config->FD_MODE == 0) {
		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000111,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00006002,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x0, CMDQ_REG_MASK);
		cmdq_sec_pkt_write_reg(pkt,
			FDVT_RS_CON_BASE_ADR_HW,
			basic_config->FDVT_RSCON_BASE_ADR,
			CMDQ_IWC_PH_2_MVA,
			0,
			basic_config->FDVT_RSCON_BUFSIZE,
			M4U_PORT_L20_IPE_FDVT_RDA_DISP);
		cmdq_sec_pkt_write_reg(pkt,
			FDVT_FD_CON_BASE_ADR_HW,
			basic_config->FDVT_FD_CON_BASE_ADR,
			CMDQ_IWC_PH_2_MVA,
			0,
			basic_config->FDVT_FD_CON_BUFSIZE,
			M4U_PORT_L20_IPE_FDVT_RDA_DISP);
		cmdq_sec_pkt_write_reg(pkt,
			FDVT_YUV2RGB_CON_BASE_ADR_HW,
			basic_config->FDVT_YUV2RGBCON_BASE_ADR,
			CMDQ_IWC_PH_2_MVA,
			0,
			basic_config->FDVT_YUV2RGBCON_BUFSIZE,
			M4U_PORT_L20_IPE_FDVT_RDA_DISP);
		cmdq_sec_pkt_set_payload(pkt, 1, sizeof(basic_config->FDVT_METADATA_TO_GCE), (unsigned int *)&basic_config->FDVT_METADATA_TO_GCE);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000100,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00000300,
			       CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);

		cmdq_sec_pkt_write_reg(pkt,
			FDVT_FD_CON_BASE_ADR_HW,
			basic_config->FDVT_FD_POSE_CON_BASE_ADR,
			CMDQ_IWC_PH_2_MVA,
			0,
			basic_config->FDVT_FD_POSE_CON_BUFSIZE,
			M4U_PORT_L20_IPE_FDVT_RDA_DISP);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	} else if (basic_config->FD_MODE == 1) {
		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000101,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00001A00,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);

		cmdq_sec_pkt_write_reg(pkt,
			FDVT_RS_CON_BASE_ADR_HW,
			basic_config->FDVT_RSCON_BASE_ADR,
			CMDQ_IWC_PH_2_MVA,
			0,
			basic_config->FDVT_RSCON_BUFSIZE,
			M4U_PORT_L20_IPE_FDVT_RDA_DISP);
		cmdq_sec_pkt_write_reg(pkt,
			FDVT_FD_CON_BASE_ADR_HW,
			basic_config->FDVT_FD_CON_BASE_ADR,
			CMDQ_IWC_PH_2_MVA,
			0,
			basic_config->FDVT_FD_CON_BUFSIZE,
			M4U_PORT_L20_IPE_FDVT_RDA_DISP);
		cmdq_sec_pkt_write_reg(pkt,
			FDVT_YUV2RGB_CON_BASE_ADR_HW,
			basic_config->FDVT_YUV2RGBCON_BASE_ADR,
			CMDQ_IWC_PH_2_MVA,
			0,
			basic_config->FDVT_YUV2RGBCON_BUFSIZE,
			M4U_PORT_L20_IPE_FDVT_RDA_DISP);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	}

	/* non-blocking API, Please use cmdqRecFlushAsync() */
	log_dbg("FDVT CMDQ Task flush\n");

	cmdq_pkt_flush(pkt);
	cmdq_dump_pkt(pkt, 0, true);
	/* release resource */
	cmdq_pkt_destroy(pkt);

#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#else

#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("config_secure_fdvt_hw");
#endif
#if 0
	/* FDVT Interrupt enabled in read-clear mode */
	FDVT_WR32(FDVT_INT_EN_REG, 0x1);
	FDVT_WR32(FDVT_ENABLE_REG, 0x00000111);
	FDVT_WR32(FDVT_RS_REG, 0x00000409);
	FDVT_WR32(FDVT_YUV2RGB_REG, basic_config->FDVT_YUV2RGB);
	FDVT_WR32(FDVT_FD_REG, 0x04000042);
	FDVT_WR32(FDVT_YUV_SRC_WD_HT_REG, basic_config->FDVT_YUV_SRC_WD_HT);
	FDVT_WR32(FDVT_RSCON_BASE_ADR_REG, basic_config->FDVT_RSCON_BASE_ADR);
	FDVT_WR32(FDVT_FD_CON_BASE_ADR_REG, basic_config->FDVT_FD_CON_BASE_ADR);
	FDVT_WR32(FDVT_YUV2RGBCON_BASE_ADR_REG,
		  basic_config->FDVT_YUV2RGBCON_BASE_ADR);
	FDVT_WR32(FDVT_FD_RLT_BASE_ADR_REG, NULL);

	FDVT_WR32(FDVT_START_REG, 0x1);	/* FDVT Interrupt read-clear mode */
#endif
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif /* __FDVT_KERNEL_PERFORMANCE_MEASURE__ */

#endif
#endif /* IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT) */
	return 0;
}
#else
{
	return 0;
}
#endif

#ifndef FDVT_USE_GCE

static bool check_fdvt_is_busy(void)
#if !BYPASS_REG
{
	unsigned int Ctrl _Fsm;
	unsigned int Fdvt_Start;

	Ctrl_Fsm = FDVT_RD32(FDVT_INT_EN_REG);
	Fdvt_Start = FDVT_RD32(FDVT_START_REG);
	if ((FDVT_IS_BUSY == (Ctrl_Fsm & FDVT_IS_BUSY)) ||
	    (FDVT_START_MASK == (Fdvt_Start & FDVT_START_MASK)))
		return MTRUE;

	return MFALSE;
}
#else
{
	return MFLASE;
}
#endif
#endif

/*
 *
 */
static signed int fdvt_dump_reg(void)
{
	signed int ret = 0;
	signed int i = 0;
#if 0
	unsigned int i = 0;
	struct FDVT_REQUEST_STRUCT *request;

	request = &fdvt_req_ring.req_struct[i];
#endif
	log_inf("- E.");
	log_inf("FDVT Config Info\n");
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_START_HW),
		(unsigned int)FDVT_RD32(FDVT_START_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_ENABLE_HW),
		(unsigned int)FDVT_RD32(FDVT_ENABLE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_LOOP_HW),
		(unsigned int)FDVT_RD32(FDVT_LOOP_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_INT_EN_HW),
		(unsigned int)FDVT_RD32(FDVT_INT_EN_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_SRC_WD_HT_HW),
		(unsigned int)FDVT_RD32(FDVT_SRC_WD_HT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_DES_WD_HT_HW),
		(unsigned int)FDVT_RD32(FDVT_DES_WD_HT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_DEBUG_INFO_0_HW),
		(unsigned int)FDVT_RD32(FDVT_DEBUG_INFO_0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_DEBUG_INFO_1_HW),
		(unsigned int)FDVT_RD32(FDVT_DEBUG_INFO_1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_YUV2RGB_CON_HW),
		(unsigned int)FDVT_RD32(FDVT_YUV2RGB_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_RS_CON_BASE_ADR_HW),
		(unsigned int)FDVT_RD32(FDVT_RS_CON_BASE_ADR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_FD_CON_BASE_ADR_HW),
		(unsigned int)FDVT_RD32(FDVT_FD_CON_BASE_ADR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_YUV2RGB_CON_BASE_ADR_HW),
		(unsigned int)FDVT_RD32(FDVT_YUV2RGB_CON_BASE_ADR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_IN_BASE_ADR_0_HW),
		(unsigned int)FDVT_RD32(FDVT_IN_BASE_ADR_0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_IN_BASE_ADR_1_HW),
		(unsigned int)FDVT_RD32(FDVT_IN_BASE_ADR_1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_IN_BASE_ADR_2_HW),
		(unsigned int)FDVT_RD32(FDVT_IN_BASE_ADR_2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_IN_BASE_ADR_3_HW),
		(unsigned int)FDVT_RD32(FDVT_IN_BASE_ADR_3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_OUT_BASE_ADR_0_HW),
		(unsigned int)FDVT_RD32(FDVT_OUT_BASE_ADR_0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_OUT_BASE_ADR_1_HW),
		(unsigned int)FDVT_RD32(FDVT_OUT_BASE_ADR_1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_OUT_BASE_ADR_2_HW),
		(unsigned int)FDVT_RD32(FDVT_OUT_BASE_ADR_2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_OUT_BASE_ADR_3_HW),
		(unsigned int)FDVT_RD32(FDVT_OUT_BASE_ADR_3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_KERNEL_BASE_ADR_0_HW),
		(unsigned int)FDVT_RD32(FDVT_KERNEL_BASE_ADR_0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_KERNEL_BASE_ADR_1_HW),
		(unsigned int)FDVT_RD32(FDVT_KERNEL_BASE_ADR_1_REG));
#if 0
	log_inf("FDVT:hw_process_idx:%d, write_idx:%d, read_idx:%d\n",
		*hw_process_idx,
		fdvt_req_ring.write_idx,
		fdvt_req_ring.read_idx);

	for (i = 0; i < MAX_FDVT_REQUEST_RING_SIZE; i++) {
		log_inf("FDVT Req:state:%d, procID:0x%08X, caller_id:0x%08X, enque_req_num:%d, frame_wr_idx:%d, frame_rd_idx:%d\n",
			request->state,
			request->process_id,
			request->caller_id,
			request->enque_req_num,
			request->frame_wr_idx,
			request->frame_rd_idx);

		for (j = 0; j < MAX_FDVT_FRAME_REQUEST;) {
			log_inf("FDVT:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				j, request->fdvt_frame_status[j],
				j + 1, request->fdvt_frame_status[j + 1],
				j + 2, request->fdvt_frame_status[j + 2],
				j + 3, request->fdvt_frame_status[j + 3]);
				j = j + 4;
		}
	}
#endif

	log_inf("- X.\n");


	log_inf("FDVT DMA Debug Info\n");

	FDVT_WR32(FDVT_CTRL_REG,
		  ((unsigned int)FDVT_RD32(FDVT_CTRL_REG)) & 0xFFFF1FFF);
	log_inf("[FDVT_CTRL - %x]: 0x%08X %08X\n", i,
		(unsigned int)(FDVT_CTRL_HW),
		(unsigned int)FDVT_RD32(FDVT_CTRL_REG));

	FDVT_WR32(DMA_DEBUG_SEL_REG,
		  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
		   0xFFFFFF00) | 0x13);

	for (i = 0; i <= 0x27; i++) {
		if (i > 0x7 && i < 0x10)
			continue;
		FDVT_WR32(DMA_DEBUG_SEL_REG,
			  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
			   0xFFFF00FF) | (i << 8));
		log_inf("[FDVT_DEBUG_SEL - %x]: 0x%08X %08X\n", i,
			(unsigned int)(DMA_DEBUG_SEL_HW),
			(unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG));

		log_inf("[FDVT_DEBUG_INFO_2 - %x]: 0x%08X %08X\n", i,
			(unsigned int)(FDVT_DEBUG_INFO_2_HW),
			(unsigned int)FDVT_RD32(FDVT_DEBUG_INFO_2_REG));
	}

	log_inf("FDVT SMI Debug Info\n");
	log_inf("FDVT Write FDVT_A_DMA_DEBUG_SEL[15:8] = 0x1\n");
	log_inf("FDVT Write FDVT_A_DMA_DEBUG_SEL[23:16] = 0x0\n");
	FDVT_WR32(DMA_DEBUG_SEL_REG,
		  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
		   0xFFFF00FF) | (1 << 8));
	FDVT_WR32(DMA_DEBUG_SEL_REG,
		  ((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) & 0xFF00FFFF);

	for (i = 1; i <= 0xe; i++) {
		FDVT_WR32(DMA_DEBUG_SEL_REG,
			  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
			   0xFFFFFF00) | i);
		log_inf("[FDVT_DEBUG_SEL SMI - %x]: 0x%08X %08X\n", i,
			(unsigned int)(DMA_DEBUG_SEL_HW),
			(unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG));
		log_inf("[FDVT_DEBUG_INFO_2 SMI - %x]: 0x%08X %08X\n", i,
			(unsigned int)(FDVT_DEBUG_INFO_2_HW),
			(unsigned int)FDVT_RD32(FDVT_DEBUG_INFO_2_REG));
	}

	log_inf("FDVT fifo_debug_data_case1\n");
	log_inf("FDVT Write FDVT_A_DMA_DEBUG_SEL[15:8] = 0x2\n");
	log_inf("FDVT Write FDVT_A_DMA_DEBUG_SEL[23:16] = 0x1\n");
	FDVT_WR32(DMA_DEBUG_SEL_REG,
		  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
		   0xFFFF00FF) | (2 << 8));
	FDVT_WR32(DMA_DEBUG_SEL_REG,
		  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
		   0xFF00FFFF) | (1 << 16));

	for (i = 1; i <= 0xe; i++) {
		FDVT_WR32(DMA_DEBUG_SEL_REG,
			  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
			   0xFFFFFF00) | i);
		log_inf("[FDVT_DEBUG_SEL SMI - %x]: 0x%08X %08X\n", i,
			(unsigned int)(DMA_DEBUG_SEL_HW),
			(unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG));
		log_inf("[FDVT_DEBUG_INFO_2 SMI - %x]: 0x%08X %08X\n", i,
			(unsigned int)(FDVT_DEBUG_INFO_2_HW),
			(unsigned int)FDVT_RD32(FDVT_DEBUG_INFO_2_REG));
	}

	log_inf("FDVT fifo_debug_data_case3\n");
	log_inf("FDVT Write FDVT_A_DMA_DEBUG_SEL[15:8] = 0x2\n");
	log_inf("FDVT Write FDVT_A_DMA_DEBUG_SEL[23:16] = 0x3\n");
	FDVT_WR32(DMA_DEBUG_SEL_REG,
		  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
		   0xFFFF00FF) | (2 << 8));
	FDVT_WR32(DMA_DEBUG_SEL_REG,
		  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
		   0xFF00FFFF) | (3 << 16));

	for (i = 1; i <= 0xe; i++) {
		FDVT_WR32(DMA_DEBUG_SEL_REG,
			  (((unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG)) &
			   0xFFFFFF00) | i);
		log_inf("[FDVT_DEBUG_SEL SMI - %x]: 0x%08X %08X\n", i,
			(unsigned int)(DMA_DEBUG_SEL_HW),
			(unsigned int)FDVT_RD32(DMA_DEBUG_SEL_REG));
		log_inf("[FDVT_DEBUG_INFO_2 SMI - %x]: 0x%08X %08X\n", i,
			(unsigned int)(FDVT_DEBUG_INFO_2_HW),
			(unsigned int)FDVT_RD32(FDVT_DEBUG_INFO_2_REG));
	}

	/*  */
	return ret;
}

#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void fdvt_prepare_enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:
	 * CG_SCP_SYS_MM0-> CG_MM_SMI_COMMON ->
	 * CG_SCP_SYS_ISP -> FDVT clk
	 */
#ifndef SMI_CLK
	ret = clk_prepare_enable(fdvt_clk.CG_SCP_SYS_MM0);
	if (ret)
		log_err("cannot prepare and enable CG_SCP_SYS_MM0 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_2X clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_GALS_M0_2X clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_GALS_M1_2X clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_UPSZ0 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_UPSZ1 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_FIFO0);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_FIFO0 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_FIFO1);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_FIFO1 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_LARB5);
	if (ret)
		log_err("cannot prepare and enable CG_MM_LARB5 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_SCP_SYS_ISP);
	if (ret)
		log_err("cannot prepare and enable CG_SCP_SYS_ISP clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_IMGSYS_LARB);
	if (ret)
		log_err("cannot prepare and enable CG_IMGSYS_LARB clock\n");
#else
	smi_bus_prepare_enable(SMI_LARB20, "camera-fdvt");
#endif
	ret = clk_prepare_enable(fdvt_clk.CG_IPESYS_FD);
	if (ret)
		log_err("cannot prepare and enable CG_IPESYS_FD clock\n");
}

static inline void fdvt_disable_unprepare_ccf_clock(void)
{
	/* must keep this clk close order:
	 * FDVT clk -> CG_SCP_SYS_ISP ->
	 * CG_MM_SMI_COMMON -> CG_SCP_SYS_MM0
	 */
	clk_disable_unprepare(fdvt_clk.CG_IPESYS_FD);
#ifndef SMI_CLK
	clk_disable_unprepare(fdvt_clk.CG_IMGSYS_LARB);
	clk_disable_unprepare(fdvt_clk.CG_SCP_SYS_ISP);
	clk_disable_unprepare(fdvt_clk.CG_MM_LARB5);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_FIFO1);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_FIFO0);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_2X);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON);
	clk_disable_unprepare(fdvt_clk.CG_SCP_SYS_MM0);
#else
	smi_bus_disable_unprepare(SMI_LARB20, "camera-fdvt");
#endif
}
#endif

#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_iommu_port(void)
{
	struct M4U_PORT_STRUCT sPort;
	int ret = 0;
	int count_of_ports = 0;
	int i = 0;

	count_of_ports = M4U_PORT_L20_IPE_FDVT_WRB_DISP -
			 M4U_PORT_L20_IPE_FDVT_RDA_DISP + 1;

	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L20_IPE_FDVT_RDA_DISP + i;
		sPort.Virtuality = FD_MEM_USE_VIRTUL;
		log_inf("config M4U Port ePortID=%d\n", sPort.ePortID);
	#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			log_inf("config M4U Port %s to %s SUCCESS\n",
			iommu_get_port_name(M4U_PORT_L20_IPE_FDVT_RDA_DISP + i),
			FD_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			log_inf("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L20_IPE_FDVT_RDA_DISP + i),
			FD_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
	#endif
	}

	return ret;
}
#endif



/*****************************************************************************
 *
 *****************************************************************************/
static void fdvt_enable_clock(bool En)
{
#if defined(EP_NO_CLKMGR)
	unsigned int set_reg;
#endif
#ifdef CONFIG_MTK_IOMMU_V2
	int ret = 0;
#endif

	if (En) { /* Enable clock. */
		/* log_dbg("Dpe clock enbled. clock_enable_count: %d.",
		 * clock_enable_count);
		 */
		mutex_lock(&fdvt_clk_mutex);
		switch (clock_enable_count) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			fdvt_prepare_enable_ccf_clock();
#else
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			set_reg = 0xFFFFFFFF;
			//FDVT_WR32(IPESYS_REG_CG_CLR, set_reg);

#endif
#else
			enable_clock(MT_CG_DMFB0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			enable_clock(MT_CG_IMAGE_FD, "CAMERA");
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif /* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
			break;
		default:
			break;
		}
		clock_enable_count++;
		mutex_unlock(&fdvt_clk_mutex);
#ifdef CONFIG_MTK_IOMMU_V2
		if (clock_enable_count == 1) {
			ret = m4u_control_iommu_port();
			if (ret)
				log_err("cannot config M4U IOMMU PORTS\n");
		}
#endif
	} else { /* Disable clock. */

		/* log_dbg("Dpe clock disabled. clock_enable_count: %d.",
		 * clock_enable_count);
		 */
		mutex_lock(&fdvt_clk_mutex);
		clock_enable_count--;
		switch (clock_enable_count) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			fdvt_disable_unprepare_ccf_clock();
#else
			/* Disable clock by hardcode:
			 *  1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 *  2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			set_reg = 0xFFFFFFFF;
			//FDVT_WR32(IPESYS_REG_CG_SET, set_reg);

#endif
#else
			/* do disable clock */
			disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			disable_clock(MT_CG_IMAGE_FD, "CAMERA");
			disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_DMFB0_SMI_COMMON, "CAMERA");
#endif /* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
			break;
		default:
			break;
		}
		mutex_unlock(&fdvt_clk_mutex);
	}
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int fdvt_read_reg(FDVT_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int ret = 0;
	/*  */
	FDVT_REG_STRUCT reg;
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	FDVT_REG_STRUCT *pData = (FDVT_REG_STRUCT *)pRegIo->pData;

	if (!pRegIo->pData ||
	    pRegIo->count == 0 ||
	    pRegIo->count > (FDVT_REG_RANGE >> 2)) {
		log_err("%s pRegIo->pData is NULL, count:%d!!",
			__func__, pRegIo->count);
		ret = -EFAULT;
		goto EXIT;
	}

	for (i = 0; i < pRegIo->count; i++) {
		if (get_user(reg.addr, (unsigned int *)&pData->addr) != 0) {
			log_err("get_user failed");
			ret = -EFAULT;
			goto EXIT;
		}
		/* pData++; */
		/*  */
		if (ISP_FDVT_BASE + reg.addr >= ISP_FDVT_BASE
			&& reg.addr < FDVT_REG_RANGE
			&& (reg.addr & 0x3) == 0) {
			reg.val = FDVT_RD32(ISP_FDVT_BASE + reg.addr);
		} else {
			log_err("Wrong address(0x%p), FDVT_BASE(0x%p), addr(0x%lx)",
				(ISP_FDVT_BASE + reg.addr),
				ISP_FDVT_BASE,
				(unsigned long)reg.addr);
			reg.val = 0;
		}
		/*  */

		if (put_user(reg.val, (unsigned int *)&pData->val) != 0) {
			log_err("put_user failed");
			ret = -EFAULT;
			goto EXIT;
		}
		pData++;
		/*  */
	}
	/*  */
EXIT:
	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
/* Can write sensor's test model only,
 * if need write to other modules, need modify current code flow
 */
static signed int fdvt_write_reg_to_hw(FDVT_REG_STRUCT *pReg,
				       unsigned int count)
{
	signed int ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store fdvt_info.debug_mask &
	 * FDVT_DBG_WRITE_REG for saving lock time
	 */
	spin_lock(&fdvt_info.spinlock_fdvt);
	dbgWriteReg = fdvt_info.debug_mask & FDVT_DBG_WRITE_REG;
	spin_unlock(&fdvt_info.spinlock_fdvt);

	/*  */
	if (dbgWriteReg)
		log_dbg("- E.\n");

	/*  */
	for (i = 0; i < count; i++) {
		if (dbgWriteReg) {
			log_dbg("addr(0x%lx), val(0x%x)\n",
				(unsigned long)(ISP_FDVT_BASE + pReg[i].addr),
				(unsigned int)(pReg[i].val));
		}

		if (pReg[i].addr < FDVT_REG_RANGE &&
		    ((pReg[i].addr & 0x3) == 0)) {
			FDVT_WR32(ISP_FDVT_BASE + pReg[i].addr, pReg[i].val);
		} else {
			log_err("wrong address(0x%p), FDVT_BASE(0x%p), addr(0x%lx)\n",
				(ISP_FDVT_BASE + pReg[i].addr),
				ISP_FDVT_BASE,
				(unsigned long)pReg[i].addr);
		}
	}

	/*  */
	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int fdvt_write_reg(FDVT_REG_IO_STRUCT *pRegIo)
{
	signed int ret = 0;
	/* unsigned char* pData = NULL; */
	FDVT_REG_STRUCT *pData = NULL;
	/* */
	if (fdvt_info.debug_mask & FDVT_DBG_WRITE_REG)
		log_dbg(
		"Data(0x%p), count(%d)\n",
		(pRegIo->pData),
		(pRegIo->count));

	if (!pRegIo->pData || pRegIo->count == 0 ||
	    pRegIo->count > (FDVT_REG_RANGE >> 2)) {
		log_err("ERROR: pRegIo->pData is NULL or count:%d\n",
			pRegIo->count);
		ret = -EFAULT;
		goto EXIT;
	}
	/* pData = (unsigned char*)kmalloc(
	 * (pRegIo->count)*sizeof(FDVT_REG_STRUCT), GFP_ATOMIC);
	 */
	pData = kmalloc((pRegIo->count) * sizeof(FDVT_REG_STRUCT), GFP_KERNEL);
	if (!pData) {
		ret = -ENOMEM;
		goto EXIT;
	}

	if (copy_from_user
		(pData, (void __user *)pRegIo->pData,
		pRegIo->count * sizeof(FDVT_REG_STRUCT)) != 0) {
		log_err("copy_from_user failed\n");
		ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	ret = fdvt_write_reg_to_hw(pData, pRegIo->count);
	/*  */
EXIT:
	kfree(pData);
	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int fdvt_wait_irq(FDVT_WAIT_IRQ_STRUCT *wait_irq)
{
	signed int ret = 0;
	signed int timeout = wait_irq->timeout;
	enum FDVT_PROCESS_ID_ENUM which_req = FDVT_PROCESS_ID_NONE;

	/*unsigned int i;*/
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;
	unsigned int irqStatus;
	/*int cnt = 0;*/
	struct timeval time_getrequest;
	unsigned long long sec = 0;
	unsigned long usec = 0;

	/* do_gettimeofday(&time_getrequest); */
	//sec = cpu_clock(0);	/* ns */
	do_div(sec, 1000);	/* usec */
	usec = do_div(sec, 1000000);	/* sec and usec */
	time_getrequest.tv_usec = usec;
	time_getrequest.tv_sec = sec;

	/* Debug interrupt */
	if (fdvt_info.debug_mask & FDVT_DBG_INT) {
		if (wait_irq->status &
			fdvt_info.irq_info.mask[wait_irq->type]) {
			if (wait_irq->user_key > 0) {
				log_dbg("+wait_irq clr(%d), type(%d), Stat(0x%08X), timeout(%d),usr(%d), ProcID(%d)\n",
					wait_irq->clear, wait_irq->type,
					wait_irq->status, wait_irq->timeout,
					wait_irq->user_key,
					wait_irq->process_id);
			}
		}
	}

	/* 1. wait type update */
	if (wait_irq->clear == FDVT_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&fdvt_info.spinlock_irq[wait_irq->type],
				  flags);
		/* log_dbg("WARNING: clear(%d), type(%d):
		 * IrqStatus(0x%08X) has been cleared"
		 * ,wait_irq->EventInfo.clear,wait_irq->type,
		 * fdvt_info.irq_info.status[wait_irq->type]);
		 * fdvt_info.irq_info.status[wait_irq->type]
		 * [wait_irq->EventInfo.user_key] &=
		 * (~wait_irq->EventInfo.status);
		 */
		fdvt_info.irq_info.status[wait_irq->type] &=
			(~wait_irq->status);
		spin_unlock_irqrestore(&fdvt_info.spinlock_irq[wait_irq->type],
				       flags);
		return ret;
	}

	if (wait_irq->clear == FDVT_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(&fdvt_info.spinlock_irq[wait_irq->type],
				  flags);
		if (fdvt_info.irq_info.status[wait_irq->type] &
			wait_irq->status)
			fdvt_info.irq_info.status[wait_irq->type] &=
			(~wait_irq->status);

		spin_unlock_irqrestore(&fdvt_info.spinlock_irq[wait_irq->type],
				       flags);
	} else if (wait_irq->clear == FDVT_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(&fdvt_info.spinlock_irq[wait_irq->type],
				  flags);

		fdvt_info.irq_info.status[wait_irq->type] = 0;
		spin_unlock_irqrestore(&fdvt_info.spinlock_irq[wait_irq->type],
				       flags);
	}
	/* FDVT_IRQ_WAIT_CLEAR ==> do nothing */

	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&fdvt_info.spinlock_irq[wait_irq->type], flags);
	irqStatus = fdvt_info.irq_info.status[wait_irq->type];
	spin_unlock_irqrestore(&fdvt_info.spinlock_irq[wait_irq->type], flags);

	if (wait_irq->status & FDVT_INT_ST) {
		which_req = FDVT_PROCESS_ID_FDVT;
	} else {
		log_err("No Such Stats can be waited!! irq type/User/Sts/pid(0x%x/%d/0x%x/%d)\n",
			wait_irq->type, wait_irq->user_key,
			wait_irq->status, wait_irq->process_id);
	}

#ifdef FDVT_WAITIRQ_LOG
	log_inf("before wait_event:Tout(%d), clear(%d), type(%d), IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
		wait_irq->timeout, wait_irq->clear, wait_irq->type,
	irqStatus, wait_irq->status, wait_irq->user_key);
	log_inf("before wait_event:ProcID(%d), FdvtIrq(0x%08X), WriteReq(0x%08X), ReadReq(0x%08X), which_req(%d)\n",
		wait_irq->process_id, fdvt_info.irq_info.fdvt_irq_cnt,
		fdvt_info.write_req_idx, fdvt_info.read_req_idx, which_req);
#endif

	/* 2. start to wait signal */
	timeout = wait_event_interruptible_timeout(fdvt_info.wait_queue_head,
						   fdvt_get_irq_state
						   (wait_irq->type,
						   wait_irq->user_key,
						   wait_irq->status,
						   which_req,
						   wait_irq->process_id),
						   fdvt_ms_to_jiffies
						   (wait_irq->timeout));

	/* check if user is interrupted by system signal */
	if (timeout != 0 &&
		!fdvt_get_irq_state(wait_irq->type, wait_irq->user_key,
				   wait_irq->status, which_req,
				   wait_irq->process_id)) {
		log_err("interrupted by system, timeout(%d),irq type/User/Sts/which_req/pid(0x%x/%d/0x%x/%d/%d)\n",
			timeout, wait_irq->type, wait_irq->user_key,
		wait_irq->status, which_req, wait_irq->process_id);
		/* actually it should be -ERESTARTSYS */
		ret = -ERESTARTSYS;
		goto EXIT;
	}
#if 0
	if (wait_irq->isSecure != 0) {
		FDVT_switchPortToNonSecure();
	}
#endif
	/* timeout */
	if (timeout == 0) {
		/* Store irqinfo status in here
		 * to redeuce time of spin_lock_irqsave
		 */
		spin_lock_irqsave(&fdvt_info.spinlock_irq[wait_irq->type],
				  flags);
		irqStatus = fdvt_info.irq_info.status[wait_irq->type];
		spin_unlock_irqrestore(&fdvt_info.spinlock_irq[wait_irq->type],
				       flags);

		log_err("wait_irq timeout:Tout(%d) clr(%d) type(%d) IrqStat(0x%08X) WaitStat(0x%08X) usrKey(%d)\n",
			wait_irq->timeout, wait_irq->clear,
			wait_irq->type, irqStatus,
			wait_irq->status, wait_irq->user_key);
		log_err("wait_irq timeout:which_req(%d),ProcID(%d) fdvt_irq_cnt(0x%08X) WriteReq(0x%08X) ReadReq(0x%08X)\n",
			which_req, wait_irq->process_id,
			fdvt_info.irq_info.fdvt_irq_cnt,
			fdvt_info.write_req_idx, fdvt_info.read_req_idx);

		if (wait_irq->dump_reg)
			fdvt_dump_reg();

		ret = -EFAULT;
		goto EXIT;
	} else {
/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("[FDVT]wait_irq");
#endif
		spin_lock_irqsave(&fdvt_info.spinlock_irq[wait_irq->type],
				  flags);
		irqStatus = fdvt_info.irq_info.status[wait_irq->type];
		spin_unlock_irqrestore(&fdvt_info.spinlock_irq[wait_irq->type],
				       flags);

		if (wait_irq->clear == FDVT_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(&fdvt_info.spinlock_irq
						[wait_irq->type],
					  flags);
#ifdef FDVT_USE_GCE

#ifdef FDVT_MULTIPROCESS_TIMEING_ISSUE
			fdvt_info.read_req_idx =
				(fdvt_info.read_req_idx + 1) %
					MAX_FDVT_FRAME_REQUEST;
			/* actually, it doesn't happen the timging issue!! */
			/* wake_up_interruptible(&fdvt_info.wait_queue_head); */
#endif /* FDVT_MULTIPROCESS_TIMEING_ISSUE */
			if (wait_irq->status & FDVT_INT_ST) {
				fdvt_info.irq_info.fdvt_irq_cnt--;
				if (fdvt_info.irq_info.fdvt_irq_cnt == 0)
					fdvt_info.irq_info.status
						[wait_irq->type] &=
						(~wait_irq->status);
			} else {
				log_err("FDVT_IRQ_WAIT_CLEAR Error, type(%d), WaitStatus(0x%08X)",
					wait_irq->type, wait_irq->status);
			}
#else /* FDVT_USE_GCE */
			if (fdvt_info.irq_info.status[wait_irq->type] &
				wait_irq->status)
				fdvt_info.irq_info.status[wait_irq->type] &=
				(~wait_irq->status);
#endif /* FDVT_USE_GCE */
			spin_unlock_irqrestore(
				&fdvt_info.spinlock_irq[wait_irq->type], flags);
		}

#ifdef FDVT_WAITIRQ_LOG
		log_inf("no timeout:Tout(%d), clr(%d), type(%d), IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
			wait_irq->timeout, wait_irq->clear,
			wait_irq->type, irqStatus, wait_irq->status,
			wait_irq->user_key);
		log_inf("no timeout:ProcID(%d),FdvtIrq(0x%08X), WriteReq(0x%08X), ReadReq(0x%08X),which_req(%d)\n",
			wait_irq->process_id, fdvt_info.irq_info.fdvt_irq_cnt,
			fdvt_info.write_req_idx, fdvt_info.read_req_idx,
			which_req);
#endif

#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
	}
EXIT:
	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static long FDVT_ioctl(struct file *pFile,
		       unsigned int Cmd, unsigned long Param)
{
	signed int ret = 0;

	/*unsigned int pid = 0;*/
	FDVT_REG_IO_STRUCT RegIo;
	FDVT_WAIT_IRQ_STRUCT irq_info;
	FDVT_CLEAR_IRQ_STRUCT ClearIrq;
	struct fdvt_config fdvt_FdvtConfig;
	FDVT_Request fdvt_FdvtReq;
	signed int FdvtWriteIdx = 0;
	int idx;
	struct FDVT_USER_INFO_STRUCT *pUserInfo;
	int enqueNum;
	int dequeNum;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;
	struct FDVT_REQUEST_STRUCT *request;
	spinlock_t *spinlock_lrq_ptr; /* spinlock for irq */

	spinlock_lrq_ptr = &fdvt_info.spinlock_irq[FDVT_IRQ_TYPE_INT_FDVT_ST];

	/*  */
	if (!pFile->private_data) {
		log_wrn(
		"private_data is NULL,(process, pid, tgid)=(%s, %d, %d)",
		current->comm, current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct FDVT_USER_INFO_STRUCT *)pFile->private_data;
	/*  */
	switch (Cmd) {
	case FDVT_RESET:
	{
		spin_lock(&fdvt_info.spinlock_fdvt);
		fdvt_reset();
		spin_unlock(&fdvt_info.spinlock_fdvt);
		break;
	}

		/*  */
	case FDVT_DUMP_REG:
	{
		ret = fdvt_dump_reg();
		break;
	}
	case FDVT_DUMP_ISR_LOG:
	{
		unsigned int currentPPB = m_CurrentPPB;

		spin_lock_irqsave(spinlock_lrq_ptr, flags);
		m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
		spin_unlock_irqrestore(spinlock_lrq_ptr, flags);

		IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, currentPPB,
				_LOG_INF);
		IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, currentPPB,
				_LOG_ERR);
		break;
	}
	case FDVT_READ_REGISTER:
	{
		if (copy_from_user(&RegIo, (void *)Param,
		    sizeof(FDVT_REG_IO_STRUCT)) == 0) {
			/* 2nd layer behavoir
			 * of copy from user
			 * is implemented in
			 * fdvt_read_reg(...)
			 */
			ret = fdvt_read_reg(&RegIo);
		} else {
			log_err("FDVT_READ_REGISTER copy_from_user failed");
			ret = -EFAULT;
		}
		break;
	}
	case FDVT_WRITE_REGISTER:
	{
		if (copy_from_user(&RegIo, (void *)Param,
		    sizeof(FDVT_REG_IO_STRUCT)) == 0) {
			/* 2nd layer behavoir
			 * of copy from user
			 * is implemented in
			 * fdvt_write_reg(...)
			 */
			ret = fdvt_write_reg(&RegIo);
		} else {
			log_err("FDVT_WRITE_REGISTER copy_from_user failed");
			ret = -EFAULT;
		}
		break;
	}
	case FDVT_WAIT_IRQ:
	{
		if (copy_from_user(&irq_info, (void *)Param,
		    sizeof(FDVT_WAIT_IRQ_STRUCT)) == 0) {
			/*  */
			if (irq_info.type >= FDVT_IRQ_TYPE_AMOUNT ||
			    irq_info.type < 0) {
				ret = -EFAULT;
				log_err("invalid type(%d)", irq_info.type);
				goto EXIT;
			}

			if (irq_info.user_key >= IRQ_USER_NUM_MAX ||
			    irq_info.user_key < 0) {
				log_err("invalid userKey(%d), max(%d), force userkey = 0\n",
					irq_info.user_key,
					IRQ_USER_NUM_MAX);
				irq_info.user_key = 0;
			}

			log_inf(
			"IRQ clear(%d), type(%d), userKey(%d), timeout(%d), status(%d)\n",
			irq_info.clear, irq_info.type,
			irq_info.user_key, irq_info.timeout,
			irq_info.status);
			irq_info.process_id = pUserInfo->pid;
			ret = fdvt_wait_irq(&irq_info);

			if (copy_to_user
				((void *)Param, &irq_info,
				sizeof(FDVT_WAIT_IRQ_STRUCT)) != 0) {
				log_err("copy_to_user failed\n");
				ret = -EFAULT;
			}
		} else {
			log_err("FDVT_WAIT_IRQ copy_from_user failed");
			ret = -EFAULT;
		}
		break;
	}
	case FDVT_CLEAR_IRQ:
	{
		if (copy_from_user(&ClearIrq, (void *)Param,
		    sizeof(FDVT_CLEAR_IRQ_STRUCT)) == 0) {
			log_dbg("FDVT_CLEAR_IRQ type(%d)", ClearIrq.type);

			if (ClearIrq.type >= FDVT_IRQ_TYPE_AMOUNT ||
			    ClearIrq.type < 0) {
				ret = -EFAULT;
				log_err("invalid type(%d)", ClearIrq.type);
				goto EXIT;
			}

			/*  */
			if (ClearIrq.user_key >= IRQ_USER_NUM_MAX ||
			    ClearIrq.user_key < 0) {
				log_err("errUserEnum(%d)", ClearIrq.user_key);
				ret = -EFAULT;
				goto EXIT;
			}

			log_dbg("FDVT_CLEAR_IRQ:type(%d),status(0x%08X),IrqStatus(0x%08X)\n",
				ClearIrq.type, ClearIrq.status,
				fdvt_info.irq_info.status[ClearIrq.type]);
			spin_lock_irqsave(&fdvt_info.spinlock_irq
						[ClearIrq.type],
					  flags);
			fdvt_info.irq_info.status[ClearIrq.type] &=
				(~ClearIrq.status);
			spin_unlock_irqrestore(
				&fdvt_info.spinlock_irq[ClearIrq.type], flags);
		} else {
			log_err("FDVT_CLEAR_IRQ copy_from_user failed\n");
			ret = -EFAULT;
		}
		break;
	}
	case FDVT_ENQNUE_NUM:
		/* enqueNum */
		if (copy_from_user(&enqueNum, (void *)Param,
		    sizeof(int)) == 0) {
			request = &fdvt_req_ring.req_struct
					[fdvt_req_ring.write_idx];
			if (FDVT_REQUEST_STATE_EMPTY ==
				request->state) {
				spin_lock_irqsave(spinlock_lrq_ptr, flags);
				request->process_id =
					pUserInfo->pid;
				request->enque_req_num =
					enqueNum;
				spin_unlock_irqrestore(spinlock_lrq_ptr, flags);
				if (enqueNum >
					MAX_FDVT_FRAME_REQUEST) {
					log_err(
					"FDVT Enque Num is bigger than enqueNum:%d\n",
					enqueNum);
				}
				log_dbg("FDVT_ENQNUE_NUM:%d\n",
					enqueNum);
			} else {
				log_err(
				"WFME Enque request state is not empty:%d, writeIdx:%d, readIdx:%d\n",
				request->state,
				fdvt_req_ring.write_idx,
				fdvt_req_ring.read_idx);
			}
		} else {
			log_err("FDVT_EQNUE_NUM copy_from_user failed\n");
			ret = -EFAULT;
		}

		break;
	/* struct fdvt_config */
	case FDVT_ENQUE:
		if (copy_from_user(&fdvt_FdvtConfig, (void *)Param,
				   sizeof(struct fdvt_config)) == 0) {
	/* log_dbg("FDVT_CLEAR_IRQ:type(%d),
	 * status(0x%08X),IrqStatus(0x%08X)",
	 * ClearIrq.type, ClearIrq.status,
	 * fdvt_info.irq_info.status[ClearIrq.type]);
	 */
			request = &fdvt_req_ring.req_struct
				[fdvt_req_ring.write_idx];

			spin_lock_irqsave(spinlock_lrq_ptr, flags);

			if (request->state == FDVT_REQUEST_STATE_EMPTY &&
			    request->frame_wr_idx < request->enque_req_num) {
				request->fdvt_frame_status
					[request->frame_wr_idx] =
						FDVT_FRAME_STATUS_ENQUE;
				memcpy(&request->frame_config
					[request->frame_wr_idx++],
					&fdvt_FdvtConfig,
					sizeof(struct fdvt_config));
				if (request->frame_wr_idx ==
					request->enque_req_num) {
					request->state =
						FDVT_REQUEST_STATE_PENDING;
					fdvt_req_ring.write_idx =
						(fdvt_req_ring.write_idx + 1) %
						MAX_FDVT_REQUEST_RING_SIZE;
					log_dbg("FDVT enque done!!\n");
				} else {
					log_dbg("FDVT enque frame!!\n");
				}
			} else {
				log_err("No Buffer! write_idx(%d), Stat(%d), frame_wr_idx(%d), enque_req_num(%d)\n",
					fdvt_req_ring.write_idx,
					request->state,
					request->frame_wr_idx,
					request->enque_req_num);
			}
#ifdef FDVT_USE_GCE
			spin_unlock_irqrestore(spinlock_lrq_ptr, flags);
			log_dbg("config_fdvt!!\n");
			config_fdvt();
#else
			/* check the hw is running or not ? */
			if (check_fdvt_is_busy() == MFALSE) {
				/* config the fdvt hw and run */
				log_dbg("config_fdvt\n");
				config_fdvt();
			} else {
				log_inf("FDVT HW is busy!!\n");
			}
			spin_unlock_irqrestore(spinlock_lrq_ptr, flags);
#endif
		} else {
			log_err("FDVT_ENQUE copy_from_user failed\n");
			ret = -EFAULT;
		}

		break;
	case FDVT_ENQUE_REQ:
		if (copy_from_user(&fdvt_FdvtReq, (void *)Param,
		    sizeof(FDVT_Request)) == 0) {
			request = &fdvt_req_ring.req_struct
					[fdvt_req_ring.write_idx];
			log_dbg("FDVT_ENQNUE_NUM:%d, pid:%d\n",
				fdvt_FdvtReq.m_ReqNum,
				pUserInfo->pid);
			if (fdvt_FdvtReq.m_ReqNum > MAX_FDVT_FRAME_REQUEST) {
				log_err("FDVT Enque Num is bigger than enqueNum:%d\n",
					fdvt_FdvtReq.m_ReqNum);
				ret = -EFAULT;
				goto EXIT;
			}
			if (copy_from_user(
				fdvt_enq_req.frame_config,
				(void *)fdvt_FdvtReq.m_pFdvtConfig,
				fdvt_FdvtReq.m_ReqNum *
				sizeof(struct fdvt_config)) != 0) {
				log_err("copy FDVTConfig from request is fail!!\n");
				ret = -EFAULT;
				goto EXIT;
			}

			/* Protect the Multi Process */
			mutex_lock(&fdvt_mutex);

			spin_lock_irqsave(spinlock_lrq_ptr, flags);
			if (FDVT_REQUEST_STATE_EMPTY ==
				request->state) {
				request->process_id = pUserInfo->pid;
				request->enque_req_num = fdvt_FdvtReq.m_ReqNum;

				for (idx = 0; idx < fdvt_FdvtReq.m_ReqNum;
					idx++) {
					request->fdvt_frame_status
					[request->frame_wr_idx] =
						FDVT_FRAME_STATUS_ENQUE;
					memcpy(&request->frame_config
						[request->frame_wr_idx++],
						&fdvt_enq_req.frame_config[idx],
						sizeof(struct fdvt_config));
				}
				request->state =
					FDVT_REQUEST_STATE_PENDING;
				FdvtWriteIdx = fdvt_req_ring.write_idx;
				fdvt_req_ring.write_idx =
					(fdvt_req_ring.write_idx + 1) %
						MAX_FDVT_REQUEST_RING_SIZE;
				log_dbg("FDVT request enque done!!\n");
			} else {
				log_err("Enque req NG: write_idx(%d) Stat(%d) frame_wr_idx(%d) enque_req_num(%d)\n",
					fdvt_req_ring.write_idx,
					request->state,
					request->frame_wr_idx,
					request->enque_req_num);
			}
			spin_unlock_irqrestore(spinlock_lrq_ptr, flags);
			log_dbg("config_fdvt Request!!\n");
			config_fdvt_request(FdvtWriteIdx);

			mutex_unlock(&fdvt_mutex);
		} else {
			log_err("FDVT_ENQUE_REQ copy_from_user failed\n");
			ret = -EFAULT;
		}

		break;
	case FDVT_DEQUE_NUM:
		request =
			&fdvt_req_ring.req_struct[fdvt_req_ring.read_idx];
		if (request->state == FDVT_REQUEST_STATE_FINISHED) {
			dequeNum = request->enque_req_num;
			log_dbg("FDVT_DEQUE_NUM(%d)\n", dequeNum);
		} else {
			dequeNum = 0;
			log_err("DEQUE_NUM:No Buffer: read_idx(%d) state(%d) frame_rd_idx(%d) enque_req_num(%d)\n",
				fdvt_req_ring.read_idx,
				request->state,
				request->frame_rd_idx,
				request->enque_req_num);
		}
		if (copy_to_user((void *)Param, &dequeNum,
		    sizeof(unsigned int)) != 0) {
			log_err("FDVT_DEQUE_NUM copy_to_user failed\n");
			ret = -EFAULT;
		}

		break;
	case FDVT_DEQUE:
		spin_lock_irqsave(&(fdvt_info.spinlock_irq
					[FDVT_IRQ_TYPE_INT_FDVT_ST]),
					flags);
		request =
			&fdvt_req_ring.req_struct[fdvt_req_ring.read_idx];
		if (request->state == FDVT_REQUEST_STATE_FINISHED &&
		    request->frame_rd_idx < request->enque_req_num) {
			/* dequeNum = g_DVE_RequestRing.
			 * DVEReq_Struct[g_DVE_RequestRing.read_idx]
			 *.enque_req_num;
			 */
			if (FDVT_FRAME_STATUS_FINISHED ==
				request->fdvt_frame_status
					[request->frame_rd_idx]) {
				memcpy(&fdvt_FdvtConfig,
				       &request->frame_config
						[request->frame_rd_idx],
				       sizeof(struct fdvt_config));
				request->fdvt_frame_status
					[request->frame_rd_idx++]
						= FDVT_FRAME_STATUS_EMPTY;
			}
			if (request->frame_rd_idx ==
				request->enque_req_num) {
				request->state = FDVT_REQUEST_STATE_EMPTY;
				request->frame_wr_idx = 0;
				request->frame_rd_idx = 0;
				request->enque_req_num = 0;
				fdvt_req_ring.read_idx =
					(fdvt_req_ring.read_idx + 1) %
						MAX_FDVT_REQUEST_RING_SIZE;
				log_dbg("FDVT read_idx(%d)\n",
					fdvt_req_ring.read_idx);
			}
			spin_unlock_irqrestore(spinlock_lrq_ptr,
					       flags);
			if (copy_to_user
				((void *)Param,
				&request->frame_config[request->frame_rd_idx],
				sizeof(struct fdvt_config)) != 0) {
				log_err("FDVT_DEQUE copy_to_user failed\n");
				ret = -EFAULT;
			}

		} else {
			spin_unlock_irqrestore(spinlock_lrq_ptr,
					       flags);
			log_err("FDVT_DEQUE No Buffer: read_idx(%d)state(%d) frame_rd_idx(%d), enque_req_num(%d)\n",
				fdvt_req_ring.read_idx,
				request->state,
				request->frame_rd_idx,
				request->enque_req_num);
		}

		break;
	case FDVT_DEQUE_REQ:
		if (copy_from_user(&fdvt_FdvtReq, (void *)Param,
				   sizeof(FDVT_Request)) == 0) {
			/* Protect the Multi Process */
			mutex_lock(&fdvt_deque_mutex);

			spin_lock_irqsave(spinlock_lrq_ptr, flags);
			request = &fdvt_req_ring.req_struct
					[fdvt_req_ring.read_idx];
			if (FDVT_REQUEST_STATE_FINISHED ==
				request->state) {
				dequeNum = request->enque_req_num;
				log_dbg("FDVT_DEQUE_REQ(%d)\n", dequeNum);
			} else {
				dequeNum = 0;
				log_err("DEQUE_REQ no buf:RIdx(%d) Stat(%d) frame_rd_idx(%d) enque_req_num(%d)\n",
					fdvt_req_ring.read_idx,
					request->state,
					request->frame_rd_idx,
					request->enque_req_num);
			}
			fdvt_FdvtReq.m_ReqNum = dequeNum;

			for (idx = 0; idx < dequeNum; idx++) {
				if (request->fdvt_frame_status
					[request->frame_rd_idx]
						== FDVT_FRAME_STATUS_FINISHED) {
					memcpy(&fdvt_deq_req
						.frame_config[idx],
						&request->frame_config
						[request->frame_rd_idx],
						sizeof(struct fdvt_config));
					request->fdvt_frame_status
						[request->frame_rd_idx++] =
						FDVT_FRAME_STATUS_EMPTY;
				} else {
					log_err("deq err idx(%d) dequNum(%d) Rd(%d) RrameRD(%d) FrmStat(%d)\n",
						idx, dequeNum,
						fdvt_req_ring.read_idx,
						request->frame_rd_idx,
						request->fdvt_frame_status
						[request->frame_rd_idx]);
				}
			}
			request->state = FDVT_REQUEST_STATE_EMPTY;
			request->frame_wr_idx = 0;
			request->frame_rd_idx = 0;
			request->enque_req_num = 0;
			fdvt_req_ring.read_idx =
				(fdvt_req_ring.read_idx + 1) %
					MAX_FDVT_REQUEST_RING_SIZE;
			log_dbg("FDVT Request read_idx(%d)\n",
				fdvt_req_ring.read_idx);

			spin_unlock_irqrestore(spinlock_lrq_ptr, flags);

			mutex_unlock(&fdvt_deque_mutex);

			if (!fdvt_FdvtReq.m_pFdvtConfig) {
				log_err("NULL pointer:fdvt_FdvtReq.m_pFdvtConfig");
				ret = -EFAULT;
				goto EXIT;
			}

			if (copy_to_user((void *)fdvt_FdvtReq.m_pFdvtConfig,
					 &fdvt_deq_req.frame_config[0],
					 dequeNum *
					 sizeof(struct fdvt_config)) != 0) {
				log_err("FDVT_DEQUE_REQ copy_to_user frameconfig failed\n");
				ret = -EFAULT;
			}
			if (copy_to_user((void *)Param,
					 &fdvt_FdvtReq,
					 sizeof(FDVT_Request)) != 0) {
				log_err("FDVT_DEQUE_REQ copy_to_user failed\n");
				ret = -EFAULT;
			}
		} else {
			log_err("FDVT_CMD_FDVT_DEQUE_REQ copy_from_user failed\n");
			ret = -EFAULT;
		}

		break;
	default:
		log_err("Unknown Cmd(%d)", Cmd);
		log_err("Fail, Cmd(%d), Dir(%d), type(%d), Nr(%d),Size(%d)\n",
			Cmd, _IOC_DIR(Cmd),
			_IOC_TYPE(Cmd), _IOC_NR(Cmd), _IOC_SIZE(Cmd));
		ret = -EPERM;
		break;
	}
	/*  */
EXIT:
	if (ret != 0) {
		log_err(
		"Fail, Cmd(%d), pid(%d), (process, pid, tgid)=(%s, %d, %d)",
		Cmd, pUserInfo->pid, current->comm,
		current->pid, current->tgid);
	}
	/*  */
	return ret;
}

#ifdef CONFIG_COMPAT
/*****************************************************************************
 *
 *****************************************************************************/
static int compat_get_FDVT_read_register_data(compat_FDVT_REG_IO_STRUCT
					      __user *data32,
					      FDVT_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err;

	err = get_user(uptr, &data32->pData);
	err |= put_user(compat_ptr(uptr), &data->pData);
	err |= get_user(count, &data32->count);
	err |= put_user(count, &data->count);
	return err;
}

static int compat_put_FDVT_read_register_data(compat_FDVT_REG_IO_STRUCT
					      __user *data32,
					      FDVT_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->pData); */
	/* err |= put_user(uptr, &data32->pData); */
	err |= get_user(count, &data->count);
	err |= put_user(count, &data32->count);
	return err;
}

static int compat_get_FDVT_enque_req_data(compat_FDVT_Request __user *data32,
					  FDVT_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pFdvtConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pFdvtConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}

static int compat_put_FDVT_enque_req_data(compat_FDVT_Request __user *data32,
					  FDVT_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static int compat_get_FDVT_deque_req_data(compat_FDVT_Request __user *data32,
					  FDVT_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pFdvtConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pFdvtConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}

static int compat_put_FDVT_deque_req_data(compat_FDVT_Request __user *data32,
					  FDVT_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pFdvtConfig); */
	/* err |= put_user(uptr, &data32->m_pFdvtConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long FDVT_ioctl_compat(struct file *filp,
			      unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		log_err("no f_op !!!\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_FDVT_READ_REGISTER:
		{
			compat_FDVT_REG_IO_STRUCT __user *data32;
			FDVT_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (!data)
				return -EFAULT;

			err = compat_get_FDVT_read_register_data(data32, data);
			if (err) {
				log_inf("compat_get_FDVT_read_register_data error!!!\n");
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp,
							 FDVT_READ_REGISTER,
							(unsigned long)data);
			err = compat_put_FDVT_read_register_data(data32, data);
			if (err) {
				log_inf("compat_put_FDVT_read_register_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_FDVT_WRITE_REGISTER:
		{
			compat_FDVT_REG_IO_STRUCT __user *data32;
			FDVT_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (!data)
				return -EFAULT;

			err = compat_get_FDVT_read_register_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_WRITE_REGISTER error!\n");
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp,
							FDVT_WRITE_REGISTER,
							(unsigned long)data);
			return ret;
		}
	case COMPAT_FDVT_ENQUE_REQ:
		{
			compat_FDVT_Request __user *data32;
			FDVT_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (!data)
				return -EFAULT;

			err = compat_get_FDVT_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_ENQUE_REQ error!!!\n");
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp, FDVT_ENQUE_REQ,
							(unsigned long)data);
			err = compat_put_FDVT_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_FDVT_DEQUE_REQ:
		{
			compat_FDVT_Request __user *data32;
			FDVT_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (!data)
				return -EFAULT;

			err = compat_get_FDVT_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
				filp->f_op->unlocked_ioctl(filp, FDVT_DEQUE_REQ,
							(unsigned long)data);
			err = compat_put_FDVT_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case FDVT_WAIT_IRQ:
	case FDVT_CLEAR_IRQ:	/* structure (no pointer) */
	case FDVT_ENQNUE_NUM:
	case FDVT_ENQUE:
	case FDVT_DEQUE_NUM:
	case FDVT_DEQUE:
	case FDVT_RESET:
	case FDVT_DUMP_REG:
	case FDVT_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return FDVT_ioctl(filep, cmd, arg); */
	}
}
#endif

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_open(struct inode *pInode, struct file *pFile)
{
	signed int ret = 0;
	unsigned int i, j;
	/*int q = 0, p = 0;*/
	struct FDVT_USER_INFO_STRUCT *pUserInfo;
	struct FDVT_REQUEST_STRUCT *request;

	log_dbg("- E. user_count: %d.", fdvt_info.user_count);
	/*  */
	spin_lock(&fdvt_info.spinlock_fdvt_ref);

	pFile->private_data = NULL;
	pFile->private_data =
		kmalloc(sizeof(struct FDVT_USER_INFO_STRUCT), GFP_ATOMIC);
	if (!pFile->private_data) {
		log_dbg("ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
			current->comm, current->pid, current->tgid);
		ret = -ENOMEM;
	} else {
		pUserInfo =
			(struct FDVT_USER_INFO_STRUCT *)pFile->private_data;
		pUserInfo->pid = current->pid;
		pUserInfo->tid = current->tgid;
	}
	/*  */
	if (fdvt_info.user_count > 0) {
		fdvt_info.user_count++;
		spin_unlock(&fdvt_info.spinlock_fdvt_ref);
		log_dbg("Curr user_count(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			fdvt_info.user_count, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		fdvt_info.user_count++;
		spin_unlock(&fdvt_info.spinlock_fdvt_ref);
		log_dbg("Curr user_count(%d), (process, pid, tgid)=(%s, %d, %d), first user",
			fdvt_info.user_count, current->comm,
			current->pid, current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < MAX_FDVT_REQUEST_RING_SIZE; i++) {
		request = &fdvt_req_ring.req_struct[i];
		/* FDVT */
		request->process_id = 0x0;
		request->caller_id = 0x0;
		request->enque_req_num = 0x0;
		/* request->enqueIdx = 0x0; */
		request->state = FDVT_REQUEST_STATE_EMPTY;
		request->frame_wr_idx = 0x0;
		request->frame_rd_idx = 0x0;
		for (j = 0; j < MAX_FDVT_FRAME_REQUEST; j++)
			request->fdvt_frame_status[j] = FDVT_FRAME_STATUS_EMPTY;
	}
	fdvt_req_ring.write_idx = 0x0;
	fdvt_req_ring.read_idx = 0x0;
	fdvt_req_ring.hw_process_idx = 0x0;

	/* Enable clock */
	fdvt_enable_clock(MTRUE);

	fdvt_count = 0;
	log_dbg("FDVT open clock_enable_count: %d", clock_enable_count);
	/*  */

	for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++)
		fdvt_info.irq_info.status[i] = 0;

	for (i = 0; i < MAX_FDVT_FRAME_REQUEST; i++)
		fdvt_info.process_id[i] = 0;

	fdvt_info.write_req_idx = 0;
	fdvt_info.read_req_idx = 0;
	fdvt_info.irq_info.fdvt_irq_cnt = 0;

/*#define KERNEL_LOG*/
#ifdef KERNEL_LOG
	/* In EP, Add FDVT_DBG_WRITE_REG for debug. Should remove it after EP */
	fdvt_info.debug_mask =
	(FDVT_DBG_INT | FDVT_DBG_DBGLOG | FDVT_DBG_WRITE_REG);
#endif
	/*  */
EXIT:
	log_dbg("- X. ret: %d. user_count: %d.", ret, fdvt_info.user_count);
	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_release(struct inode *pInode, struct file *pFile)
{
	struct FDVT_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg;*/

	log_dbg("- E. user_count: %d.", fdvt_info.user_count);

	/*  */
	if (pFile->private_data) {
		pUserInfo =
		(struct FDVT_USER_INFO_STRUCT *)pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&fdvt_info.spinlock_fdvt_ref);
	fdvt_info.user_count--;

	if (fdvt_info.user_count > 0) {
		spin_unlock(&fdvt_info.spinlock_fdvt_ref);
		log_dbg("Curr user_count(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			fdvt_info.user_count, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		spin_unlock(&fdvt_info.spinlock_fdvt_ref);
	}
	/*  */
	log_dbg("Curr user_count(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		fdvt_info.user_count, current->comm, current->pid,
		current->tgid);

	/* Disable clock. */
	fdvt_enable_clock(MFALSE);
	log_dbg("FDVT release clock_enable_count: %d", clock_enable_count);
	/*  */
EXIT:
	log_dbg("- X. user_count: %d.", fdvt_info.user_count);
	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	long length = 0;
	unsigned long pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	log_inf("[%s] mmap:vm_pgoff(0x%lx) pfn(0x%lx) phy(0x%lx) vm_start(0x%lx) vm_end(0x%lx) length(0x%lx)",
		__func__, pVma->vm_pgoff, pfn, pVma->vm_pgoff << PAGE_SHIFT,
		pVma->vm_start, pVma->vm_end, length);

	switch (pfn) {
	case FDVT_BASE_HW:
		if (length > FDVT_REG_RANGE) {
			log_err("mmap range error :module:0x%lx length(0x%lx),FDVT_REG_RANGE(0x%x)!",
				pfn, length, FDVT_REG_RANGE);
			return -EAGAIN;
		}
		break;
	default:
		log_err("Illegal starting HW addr for mmap!");
		return -EAGAIN;
	}
	if (remap_pfn_range
		(pVma, pVma->vm_start, pVma->vm_pgoff,
		pVma->vm_end - pVma->vm_start,
		pVma->vm_page_prot)) {
		return -EAGAIN;
	}
	/*  */
	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/

static dev_t FDVTDevNo;
static struct cdev *pFDVTCharDrv;
static struct class *pFDVTClass;

static const struct file_operations FDVTFileOper = {
	.owner = THIS_MODULE,
	.open = FDVT_open,
	.release = FDVT_release,
	/* .flush = mt_FDVT_flush, */
	.mmap = FDVT_mmap,
	.unlocked_ioctl = FDVT_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = FDVT_ioctl_compat,
#endif
};

/**************************************************************
 *
 **************************************************************/
#ifdef CONFIG_MTK_IOMMU_V2
enum mtk_iommu_callback_ret_t
	FDVT_M4U_TranslationFault_callback(int port,
					   unsigned int mva,
					   void *data)
#else
enum m4u_callback_ret_t FDVT_M4U_TranslationFault_callback(int port,
							   unsigned int mva,
							   void *data)
#endif
{
	pr_info("[FDVT_M4U]fault call port=%d, mva=0x%x", port, mva);

	switch (port) {
#if 0
	case M4U_PORT_FDVT_RDA:
	case M4U_PORT_FDVT_RDB:
	case M4U_PORT_FDVT_WRA:
	case M4U_PORT_FDVT_WRB:
#endif
	default: //ISP_FDVT_BASE = 0x1b001000
		pr_info("FDVT_IN_BASE_ADR_0:0x%08x, FDVT_IN_BASE_ADR_1:0x%08x, FDVT_IN_BASE_ADR_2:0x%08x, FDVT_IN_BASE_ADR_3:0x%08x\n",
			FDVT_RD32(FDVT_IN_BASE_ADR_0_REG),
			FDVT_RD32(FDVT_IN_BASE_ADR_1_REG),
			FDVT_RD32(FDVT_IN_BASE_ADR_2_REG),
			FDVT_RD32(FDVT_IN_BASE_ADR_3_REG));
		pr_info("FDVT_OUT_BASE_ADR_0:0x%08x, FDVT_OUT_BASE_ADR_1:0x%08x, FDVT_OUT_BASE_ADR_2:0x%08x, FDVT_OUT_BASE_ADR_3:0x%08x\n",
			FDVT_RD32(FDVT_OUT_BASE_ADR_0_REG),
			FDVT_RD32(FDVT_OUT_BASE_ADR_1_REG),
			FDVT_RD32(FDVT_OUT_BASE_ADR_2_REG),
			FDVT_RD32(FDVT_OUT_BASE_ADR_3_REG));
		pr_info("FDVT_KERNEL_BASE_ADR_0:0x%08x, FDVT_KERNEL_BASE_ADR_1:0x%08x\n",
			FDVT_RD32(FDVT_KERNEL_BASE_ADR_0_REG),
			FDVT_RD32(FDVT_KERNEL_BASE_ADR_1_REG));
	break;
	}
#ifdef CONFIG_MTK_IOMMU_V2
	return MTK_IOMMU_CALLBACK_HANDLED;
#else
	return M4U_CALLBACK_HANDLED;
#endif
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline void FDVT_UnregCharDev(void)
{
	log_dbg("- E.");
	/*  */
	/* Release char driver */
	if (pFDVTCharDrv) {
		cdev_del(pFDVTCharDrv);
		pFDVTCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(FDVTDevNo, 1);
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline signed int FDVT_RegCharDev(void)
{
	signed int ret = 0;
	/*  */
	log_dbg("- E.");
	/*  */
	ret = alloc_chrdev_region(&FDVTDevNo, 0, 1, FDVT_DEV_NAME);
	if (ret < 0) {
		log_err("alloc_chrdev_region failed, %d", ret);
		return ret;
	}
	/* Allocate driver */
	pFDVTCharDrv = cdev_alloc();
	if (!pFDVTCharDrv) {
		log_err("cdev_alloc failed");
		ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pFDVTCharDrv, &FDVTFileOper);
	/*  */
	pFDVTCharDrv->owner = THIS_MODULE;
	/* Add to system */
	ret = cdev_add(pFDVTCharDrv, FDVTDevNo, 1);
	if (ret < 0) {
		log_err("Attatch file operation failed, %d", ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (ret < 0)
		FDVT_UnregCharDev();

	/*  */

	log_dbg("- X.");
	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_probe(struct platform_device *pDev)
{
	signed int ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3]; /* Record interrupts info from device tree */
	struct device *dev = NULL;
	struct fdvt_device *_fdvt_dev;
#ifdef CONFIG_OF
	struct fdvt_device *FDVT_dev;
#endif

	log_inf("- E. FDVT driver probe.\n");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (!pDev) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_fdvt_devs += 1;
	_fdvt_dev = krealloc(fdvt_devs, sizeof(struct fdvt_device) *
				nr_fdvt_devs, GFP_KERNEL);
	if (!_fdvt_dev) {
		dev_dbg(&pDev->dev, "Unable to allocate fdvt_devs\n");
		return -ENOMEM;
	}
	fdvt_devs = _fdvt_dev;

	FDVT_dev = &fdvt_devs[nr_fdvt_devs - 1];
	FDVT_dev->dev = &pDev->dev;

	/* iomap registers */
	FDVT_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_fdvt_devs - 1] = FDVT_dev->regs; */

	if (!FDVT_dev->regs) {
		dev_dbg(&pDev->dev,
			"Unable to ioremap registers, of_iomap fail, nr_fdvt_devs=%d, devnode(%s).\n",
			nr_fdvt_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		*(FDVT_dev->dev->dma_mask) =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
		FDVT_dev->dev->coherent_dma_mask =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
#endif

	log_inf("nr_fdvt_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_fdvt_devs,
		pDev->dev.of_node->name, (unsigned long)FDVT_dev->regs);

	/* get IRQ ID and request IRQ */
	FDVT_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (FDVT_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array
			(pDev->dev.of_node, "interrupts",
			irq_info, ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
				   FDVT_IRQ_CB_TBL[i].device_name) == 0) {
				ret = request_irq(FDVT_dev->irq,
						  (irq_handler_t)
						  FDVT_IRQ_CB_TBL[i].isr_fp,
						  irq_info[2],
						  (const char *)
						  FDVT_IRQ_CB_TBL[i].device_name,
						  NULL);
				if (ret) {
					dev_dbg(&pDev->dev,
						"Unable to request IRQ, request_irq fail, nr_fdvt_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
						nr_fdvt_devs,
						pDev->dev.of_node->name,
						FDVT_dev->irq,
						FDVT_IRQ_CB_TBL[i].device_name);
					return ret;
				}

				log_inf(
				"nr_fdvt_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
				nr_fdvt_devs,
				pDev->dev.of_node->name,
				FDVT_dev->irq,
				FDVT_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= FDVT_IRQ_TYPE_AMOUNT) {
			log_inf(
			"No corresponding ISR!!: nr_fdvt_devs=%d, devnode(%s), irq=%d\n",
			nr_fdvt_devs, pDev->dev.of_node->name, FDVT_dev->irq);
		}

	} else {
		log_inf("No IRQ!!: nr_fdvt_devs=%d, devnode(%s), irq=%d\n",
			nr_fdvt_devs,
			pDev->dev.of_node->name, FDVT_dev->irq);
	}

	fdvt_clt = cmdq_mbox_create(FDVT_dev->dev, 0);
	if (!fdvt_clt)
		log_err("cmdq mbox create fail\n");
	else
		log_inf("cmdq mbox create done\n");

	fdvt_secure_clt = cmdq_mbox_create(FDVT_dev->dev, 1);
	if (!fdvt_secure_clt)
		log_err("cmdq mbox create fail\n");
	else
		log_inf("cmdq mbox create done\n");

	of_property_read_u32(pDev->dev.of_node, "fdvt_frame_done",
			     &fdvt_event_id);
	log_inf("fdvt event id is %d\n", fdvt_event_id);

#endif
	/* Only register char driver in the 1st time */
	if (nr_fdvt_devs == 1) {
		/* Register char driver */
		ret = FDVT_RegCharDev();
		if (ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return ret;
		}
#ifndef EP_NO_CLKMGR
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			/*CCF: Grab clock pointer (struct clk*) */
#ifndef SMI_CLK
		fdvt_clk.CG_SCP_SYS_MM0 =
			devm_clk_get(&pDev->dev, "FDVT_SCP_SYS_MM0");
		fdvt_clk.CG_MM_SMI_COMMON =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG2_B11");
		fdvt_clk.CG_MM_SMI_COMMON_2X =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG2_B12");
		fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B12");
		fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B13");
		fdvt_clk.CG_MM_SMI_COMMON_UPSZ0 =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B14");
		fdvt_clk.CG_MM_SMI_COMMON_UPSZ1 =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B15");
		fdvt_clk.CG_MM_SMI_COMMON_FIFO0 =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B16");
		fdvt_clk.CG_MM_SMI_COMMON_FIFO1 =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B17");
		fdvt_clk.CG_MM_LARB5 =
			devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B10");
		fdvt_clk.CG_SCP_SYS_ISP =
			devm_clk_get(&pDev->dev, "FDVT_SCP_SYS_ISP");
		fdvt_clk.CG_IMGSYS_LARB =
			devm_clk_get(&pDev->dev, "FDVT_CLK_IMG_LARB");
#endif
		fdvt_clk.CG_IPESYS_FD =
			devm_clk_get(&pDev->dev, "FD_CLK_IPE_FD");

#ifndef SMI_CLK
		if (IS_ERR(fdvt_clk.CG_SCP_SYS_MM0)) {
			log_err("cannot get CG_SCP_SYS_MM0 clock\n");
			return PTR_ERR(fdvt_clk.CG_SCP_SYS_MM0);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON)) {
			log_err("cannot get CG_MM_SMI_COMMON clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_2X)) {
			log_err("cannot get CG_MM_SMI_COMMON_2X clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_2X);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X)) {
			log_err("cannot get CG_MM_SMI_COMMON_GALS_M0_2X clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X)) {
			log_err("cannot get CG_MM_SMI_COMMON_GALS_M1_2X clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0)) {
			log_err("cannot get CG_MM_SMI_COMMON_UPSZ0 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1)) {
			log_err("cannot get CG_MM_SMI_COMMON_UPSZ1 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO0)) {
			log_err("cannot get CG_MM_SMI_COMMON_FIFO0 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO0);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO1)) {
			log_err("cannot get CG_MM_SMI_COMMON_FIFO1 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO1);
		}
		if (IS_ERR(fdvt_clk.CG_MM_LARB5)) {
			log_err("cannot get CG_MM_LARB5 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_LARB5);
		}
		if (IS_ERR(fdvt_clk.CG_SCP_SYS_ISP)) {
			log_err("cannot get CG_SCP_SYS_ISP clock\n");
			return PTR_ERR(fdvt_clk.CG_SCP_SYS_ISP);
		}
		if (IS_ERR(fdvt_clk.CG_IMGSYS_LARB)) {
			log_err("cannot get CG_IMGSYS_LARB clock\n");
			return PTR_ERR(fdvt_clk.CG_IMGSYS_LARB);
		}
#endif
		if (IS_ERR(fdvt_clk.CG_IPESYS_FD)) {
			log_err("cannot get CG_IPESYS_FD clock\n");
			return PTR_ERR(fdvt_clk.CG_IPESYS_FD);
		}
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
#endif

		/* Create class register */
		pFDVTClass = class_create(THIS_MODULE, "FDVTdrv");
		if (IS_ERR(pFDVTClass)) {
			ret = PTR_ERR(pFDVTClass);
			log_err("Unable to create class, err = %d", ret);
			goto EXIT;
		}

		dev = device_create(pFDVTClass, NULL,
				    FDVTDevNo, NULL, FDVT_DEV_NAME);
		if (IS_ERR(dev)) {
			ret = PTR_ERR(dev);
			dev_dbg(&pDev->dev, "Failed to create device: /dev/%s, err = %d",
				FDVT_DEV_NAME, ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&fdvt_info.spinlock_fdvt_ref);
		spin_lock_init(&fdvt_info.spinlock_fdvt);
		for (n = 0; n < FDVT_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&fdvt_info.spinlock_irq[n]);

		/*  */
		init_waitqueue_head(&fdvt_info.wait_queue_head);
		INIT_WORK(&fdvt_info.schedule_fdvt_work, fdvt_schedule_work);

#ifdef CONFIG_PM_SLEEP
		wakeup_source_init(&fdvt_wake_lock, "fdvt_lock_wakelock");
#endif
		// wake_lock_init(
		// &fdvt_wake_lock, WAKE_LOCK_SUSPEND, "fdvt_lock_wakelock");

		for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(fdvt_tasklet[i].pFDVT_tkt,
				     fdvt_tasklet[i].tkt_cb, 0);

		/* Init fdvt_info */
		spin_lock(&fdvt_info.spinlock_fdvt_ref);
		fdvt_info.user_count = 0;
		spin_unlock(&fdvt_info.spinlock_fdvt_ref);
		/*  */
		fdvt_info.irq_info.mask
			[FDVT_IRQ_TYPE_INT_FDVT_ST] = INT_ST_MASK_FDVT;
	}

EXIT:
	if (ret < 0)
		FDVT_UnregCharDev();

	log_inf("- X. FDVT driver probe.");

	return ret;
}

/*****************************************************************************
 * Called when the device is being detached from the driver
 *****************************************************************************/
static signed int FDVT_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int irq_num;
	int i;
	/*  */
	log_dbg("- E.");
	/* unregister char driver. */
	FDVT_UnregCharDev();

	/* Release IRQ */
	disable_irq(fdvt_info.irq_num);
	irq_num = platform_get_irq(pDev, 0);
	free_irq(irq_num, NULL);

	/* kill tasklet */
	for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(fdvt_tasklet[i].pFDVT_tkt);
#if 0
	/* free all registered irq(child nodes) */
	FDVT_UnRegister_AllregIrq();
	/* free father nodes of irq user list */
	struct my_list_head *head;
	struct my_list_head *father;

	head = ((struct my_list_head *)(&SupIrqUserListHead.list));
	while (1) {
		father = head;
		if (father->nextirq != father) {
			father = father->nextirq;
			REG_IRQ_NODE *accessNode;

			typeof(((REG_IRQ_NODE *)0)->list) * __mptr = (father);
			accessNode =
				((REG_IRQ_NODE *)((char *)__mptr -
						offsetof(REG_IRQ_NODE, list)));
			log_inf("free father,reg_T(%d)\n", accessNode->reg_T);
			if (father->nextirq != father) {
				head->nextirq = father->nextirq;
				father->nextirq = father;
			} else {	/* last father node */
				head->nextirq = head;
				log_inf("break\n");
				break;
			}
			kfree(accessNode);
		}
	}
#endif
	/*  */
	device_destroy(pFDVTClass, FDVTDevNo);
	/*  */
	class_destroy(pFDVTClass);
	pFDVTClass = NULL;
	/*  */
	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int FDVT_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0;*/

	log_dbg("bPass1_On_In_Resume_TG1(%d)\n", bPass1_On_In_Resume_TG1);

	bPass1_On_In_Resume_TG1 = 0;

	if (clock_enable_count > 0) {
		fdvt_enable_clock(MFALSE);
		fdvt_count++;
	}
	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_resume(struct platform_device *pDev)
{
	log_dbg("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);

	if (fdvt_count > 0) {
		fdvt_enable_clock(MTRUE);
		fdvt_count--;
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int FDVT_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(!pdev);

	/* pr_debug("calling %s()\n", __func__); */
	log_inf("FDVT suspend clock_enable_count: %d, fdvt_count: %d",
		clock_enable_count, fdvt_count);

	return FDVT_suspend(pdev, PMSG_SUSPEND);
}

int FDVT_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(!pdev);

	/* pr_debug("calling %s()\n", __func__); */
	log_inf("FDVT resume clock_enable_count: %d, fdvt_count: %d",
		clock_enable_count, fdvt_count);

	return FDVT_resume(pdev);
}

#ifndef CONFIG_OF
/*extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);*/
/*extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);*/
#endif

int FDVT_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
/*	mt_irq_set_sens(FDVT_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);*/
/*	mt_irq_set_polarity(FDVT_IRQ_BIT_ID, MT_POLARITY_LOW);*/
#endif
	return 0;
}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define FDVT_pm_suspend NULL
#define FDVT_pm_resume NULL
#define FDVT_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF
/*
 * Note!!! The order and member of .compatible
 * must be the same with FDVT_DEV_NODE_IDX
 */
static const struct of_device_id FDVT_of_ids[] = {
/*	{.compatible = "mediatek,ipesyscq",},*/
	{.compatible = "mediatek,fdvt",},
	{}
};
#endif

const struct dev_pm_ops FDVT_pm_ops = {
	.suspend = FDVT_pm_suspend,
	.resume = FDVT_pm_resume,
	.freeze = FDVT_pm_suspend,
	.thaw = FDVT_pm_resume,
	.poweroff = FDVT_pm_suspend,
	.restore = FDVT_pm_resume,
	.restore_noirq = FDVT_pm_restore_noirq,
};

/*****************************************************************************
 *
 *****************************************************************************/
static struct platform_driver FDVTDriver = {
	.probe = FDVT_probe,
	.remove = FDVT_remove,
	.suspend = FDVT_suspend,
	.resume = FDVT_resume,
	.driver = {
			.name = FDVT_DEV_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = FDVT_of_ids,
#endif
#ifdef CONFIG_PM
			.pm = &FDVT_pm_ops,
#endif
	}
};

static int fdvt_dump_read(struct seq_file *m, void *v)
{
	int i, j;
	struct FDVT_REQUEST_STRUCT *request;
	signed int *hw_process_idx;

	spin_lock(&fdvt_info.spinlock_fdvt);
	if (clock_enable_count == 0) {
		spin_unlock(&fdvt_info.spinlock_fdvt);
		return 0;
	}
	spin_unlock(&fdvt_info.spinlock_fdvt);

	seq_puts(m, "\n============ fdvt dump register============\n");
	seq_puts(m, "FDVT Config Info\n");

	hw_process_idx = &fdvt_req_ring.hw_process_idx;

	if (fdvt_info.user_count > 0) {
		seq_puts(m, "FDVT Config Info\n");
		for (i = 0x4; i < 0x168; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
		seq_puts(m, "FDVT Debug Info\n");
		for (i = 0x16C; i < 0x174; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
		seq_puts(m, "FDVT DMA Info\n");
		for (i = 0x200; i < 0x3F8; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
	}
	seq_puts(m, "\n");
	seq_printf(m, "FDVT Clock count:%d\n", clock_enable_count);

	seq_printf(m, "FDVT:hw_process_idx:%d, write_idx:%d, read_idx:%d\n",
		   *hw_process_idx, fdvt_req_ring.write_idx,
		   fdvt_req_ring.read_idx);

	for (i = 0; i < MAX_FDVT_REQUEST_RING_SIZE; i++) {
		request = &fdvt_req_ring.req_struct[i];
		seq_printf(m,
			   "FDVT:state:%d, process_id:0x%08X, caller_id:0x%08X, enque_req_num:%d, frame_wr_idx:%d, frame_rd_idx:%d\n",
			   request->state, request->process_id,
			   request->caller_id, request->enque_req_num,
			   request->frame_wr_idx, request->frame_rd_idx);

		for (j = 0; j < MAX_FDVT_FRAME_REQUEST;) {
			seq_printf(m,
				   "FDVT:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				   j, request->fdvt_frame_status[j],
				   j + 1, request->fdvt_frame_status[j + 1],
				   j + 2, request->fdvt_frame_status[j + 2],
				   j + 3, request->fdvt_frame_status[j + 3]);
			j = j + 4;
		}
	}

	seq_puts(m, "\n============ fdvt dump debug ============\n");

	return 0;
}

static int proc_fdvt_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, fdvt_dump_read, NULL);
}

static const struct file_operations fdvt_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_fdvt_dump_open,
	.read = seq_read,
};

static int fdvt_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	seq_puts(m, "======== read fdvt register ========\n");

	if (fdvt_info.user_count > 0) {
		for (i = 0x0; i <= 0x3f8; i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
	}
	seq_printf(m, "[0x%08X 0x%08X]\n", (unsigned int)(DMA_DEBUG_ADDR_HW),
		   (unsigned int)FDVT_RD32(DMA_DEBUG_ADDR_REG));

	return 0;
}

// static int fdvt_reg_write(struct file *file, const char __user *buffer,
//		size_t count, loff_t *data)

static ssize_t fdvt_reg_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *data)
{
	char desc[128];
	unsigned int len = 0;
	/*char *pEnd;*/
	char addrSzBuf[24];
	char valSzBuf[24];
	char *pszTmp;
	int addr = 0, val = 0;
	long int tempval;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (fdvt_info.user_count <= 0)
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%23s %23s", addrSzBuf, valSzBuf) == 2) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (!pszTmp) {
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				log_err("scan decimal addr is wrong !!:%s",
					addrSzBuf);
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					log_err("scan hexadecimal addr is wrong !!:%s",
						addrSzBuf);
			} else {
				log_inf("FDVT Write addr Error!!:%s",
					addrSzBuf);
			}
		}

		pszTmp = strstr(valSzBuf, "0x");
		if (!pszTmp) {
			if (kstrtol(valSzBuf, 10, (long int *)&tempval) != 0)
				log_err("scan decimal value is wrong !!:%s",
					valSzBuf);
		} else {
			if (strlen(valSzBuf) > 2) {
				if (sscanf(valSzBuf + 2, "%x", &val) != 1)
					log_err("scan hexadecimal value is wrong !!:%s",
						valSzBuf);
			} else {
				log_inf("FDVT Write Value Error!:%s\n",
					valSzBuf);
			}
		}

		if (addr >= FDVT_START_HW && addr <= DMA_BW_SELF_TEST_HW
			&& (addr & 0x3) == 0) {
			log_inf("Write Request - addr:0x%x, value:0x%x\n",
				addr, val);
			FDVT_WR32((ISP_FDVT_BASE + (addr - FDVT_START_HW)),
				  val);
		} else {
			log_inf(
			"Write-Address Range exceeds the size of hw fdvt!! addr:0x%x, value:0x%x\n",
			addr, val);
		}

	} else if (sscanf(desc, "%23s", addrSzBuf) == 1) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (!pszTmp) {
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				log_err("scan decimal addr is wrong !!:%s",
					addrSzBuf);
			else
				addr = tempval;
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					log_err("scan hexadecimal addr is wrong !!:%s",
						addrSzBuf);
			} else {
				log_inf("FDVT Read addr Error!!:%s", addrSzBuf);
			}
		}

		if (addr >= FDVT_START_HW && addr <= DMA_BW_SELF_TEST_HW
			&& (addr & 0x3) == 0) {
			val =
			FDVT_RD32((ISP_FDVT_BASE + (addr - FDVT_BASE_HW)));
			log_inf("Read Request - addr:0x%x,value:0x%x\n",
				addr, val);
		} else {
			log_inf(
			"Read-Address Range exceeds the size of hw fdvt!! addr:0x%x, value:0x%x\n",
			addr, val);
		}
	}
	return count;
}

static int proc_fdvt_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, fdvt_reg_read, NULL);
}

static const struct file_operations fdvt_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_fdvt_reg_open,
	.read = seq_read,
	.write = fdvt_reg_write,
};

/*****************************************************************************
 *
 *****************************************************************************/

int32_t FDVT_ClockOnCallback(uint64_t engineFlag)
{
	/* log_dbg("FDVT_ClockOnCallback"); */
	/* log_dbg("+CmdqEn:%d", clock_enable_count); */
	/* fdvt_enable_clock(MTRUE); */

	return 0;
}

int32_t FDVT_DumpCallback(uint64_t engineFlag, int level)
{
	log_dbg("[FDVT]DumpCallback");

	fdvt_dump_reg();

	return 0;
}

int32_t FDVT_ResetCallback(uint64_t engineFlag)
{
	log_dbg("[FDVT]ResetCallback");
	fdvt_reset();

	return 0;
}

int32_t FDVT_ClockOffCallback(uint64_t engineFlag)
{
	/* log_dbg("FDVT_ClockOffCallback"); */
	/* fdvt_enable_clock(MFALSE); */
	/* log_dbg("-CmdqEn:%d", clock_enable_count); */
	return 0;
}

static signed int __init FDVT_Init(void)
{
	signed int ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
#if 0
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_fdvt_dir;
#endif
	int i;

	/*  */
	log_dbg("- E.");
	/*  */
	ret = platform_driver_register(&FDVTDriver);
	if (ret < 0) {
		log_err("platform_driver_register fail");
		return ret;
	}

#if 0
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,FDVT");
	if (!node) {
		log_err("find mediatek,FDVT node failed!!!\n");
		return -ENODEV;
	}
	ISP_FDVT_BASE = of_iomap(node, 0);
	if (!ISP_FDVT_BASE) {
		log_err("unable to map ISP_FDVT_BASE registers!!!\n");
		return -ENODEV;
	}
	log_dbg("ISP_FDVT_BASE: %lx\n", ISP_FDVT_BASE);
#endif

#if 0
	isp_fdvt_dir = proc_mkdir("fdvt", NULL);
	if (!isp_fdvt_dir) {
		log_err("[%s]: fail to mkdir /proc/fdvt\n", __func__);
		return 0;
	}
#endif

	// proc_entry = proc_create("pll_test", S_IRUGO | S_IWUSR,
	// isp_fdvt_dir, &pll_test_proc_fops);

#if 0
	proc_entry = proc_create("fdvt_dump", 0444,
				 isp_fdvt_dir, &fdvt_dump_proc_fops);

	proc_entry = proc_create("fdvt_reg", 0644,
				 isp_fdvt_dir, &fdvt_reg_proc_fops);
#endif

	/* isr log */
	if (PAGE_SIZE <
		((FDVT_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
		LOG_PPNUM)) {
		i = 0;
		while (i <
			((FDVT_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			 ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	log_kmalloc = kmalloc(i, GFP_KERNEL);
	if (!log_kmalloc) {
		log_err
			("log mem not enough\n");
		return -ENOMEM;
	}
	memset(log_kmalloc, 0x00, i);
	tmp = log_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < FDVT_IRQ_TYPE_AMOUNT; j++) {
			sv_log[j]._str[i][_LOG_DBG] = (char *)tmp;
			// tmp = (void*) ((unsigned int)tmp +
			// (NORMAL_STR_LEN*DBG_PAGE));
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * DBG_PAGE));
			sv_log[j]._str[i][_LOG_INF] = (char *)tmp;
			// tmp = (void*) ((unsigned int)tmp +
			// (NORMAL_STR_LEN*INF_PAGE));
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * INF_PAGE));
			sv_log[j]._str[i][_LOG_ERR] = (char *)tmp;
			// tmp =
			// (void*) ((unsigned int)tmp +
			// (NORMAL_STR_LEN*ERR_PAGE));
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * ERR_PAGE));
		}
		/* log buffer ,in case of overflow */
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		/* log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}

#if 0
	/* Cmdq */
	/* Register FDVT callback */
	log_dbg("register fdvt callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_FDVT,
			   FDVT_ClockOnCallback,
			   FDVT_DumpCallback,
			   FDVT_ResetCallback,
			   FDVT_ClockOffCallback);
#endif

#if 0
#ifdef CONFIG_MTK_IOMMU_V2
	mtk_iommu_register_fault_callback(M4U_PORT_FDVT_RDA,
					  FDVT_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_FDVT_RDB,
					  FDVT_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_FDVT_WRA,
					  FDVT_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_FDVT_WRB,
					  FDVT_M4U_TranslationFault_callback,
					  NULL);
#else
	m4u_register_fault_callback(M4U_PORT_FDVT_RDA,
				    FDVT_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_FDVT_RDB,
				    FDVT_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_FDVT_WRA,
				    FDVT_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_FDVT_WRB,
				    FDVT_M4U_TranslationFault_callback, NULL);
#endif
#endif

	log_dbg("- X. ret: %d.", ret);
	return ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void __exit FDVT_Exit(void)
{
	/*int i;*/

	log_dbg("- E.");
	/*  */
	platform_driver_unregister(&FDVTDriver);
	/*  */
#if 0
	/* Cmdq */
	/* Unregister FDVT callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_FDVT, NULL, NULL, NULL, NULL);
#endif

	kfree(log_kmalloc);

	/*  */
}

/*****************************************************************************
 *
 *****************************************************************************/
void fdvt_schedule_work(struct work_struct *data)
{
	if (FDVT_DBG_DBGLOG & fdvt_info.debug_mask)
		log_dbg("- E.");

#ifdef FDVT_USE_GCE
#else
	config_fdvt();
#endif
}

static irqreturn_t isp_irq_fdvt(signed int irq, void *device_id)
{
	unsigned int status;
	bool result = MFALSE;
	pid_t process_id;
	spinlock_t *spinlock_lrq_ptr; /* spinlock for irq */

	spinlock_lrq_ptr =
		&fdvt_info.spinlock_irq[FDVT_IRQ_TYPE_INT_FDVT_ST];

	status = FDVT_RD32(FDVT_INT_REG);	/* FDVT status */

	spin_lock(spinlock_lrq_ptr);

	if (FDVT_INT_ST == (FDVT_INT_ST & status)) {
		/* Update the frame status. */
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("fdvt_irq");
#endif

#ifndef FDVT_USE_GCE
		FDVT_WR32(FDVT_START_REG, 0);
#endif
		result = update_fdvt(&process_id);
		/* config_fdvt(); */
		if (result == MTRUE) {
			schedule_work(&fdvt_info.schedule_fdvt_work);
#ifdef FDVT_USE_GCE
			fdvt_info.irq_info.status
				[FDVT_IRQ_TYPE_INT_FDVT_ST] |= FDVT_INT_ST;
			fdvt_info.irq_info.process_id
				[FDVT_PROCESS_ID_FDVT] = process_id;
			fdvt_info.irq_info.fdvt_irq_cnt++;
			fdvt_info.process_id
				[fdvt_info.write_req_idx] = process_id;
			fdvt_info.write_req_idx =
				(fdvt_info.write_req_idx + 1) %
				MAX_FDVT_FRAME_REQUEST;
#ifdef FDVT_MULTIPROCESS_TIMEING_ISSUE
			/* check the write value is equal to read value ? */
			/* actually, it doesn't happen!! */
			if (fdvt_info.write_req_idx == fdvt_info.read_req_idx) {
				IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
					       m_CurrentPPB, _LOG_ERR,
					       "Irq_FDVT Err!!, write_req_idx:0x%x, read_req_idx:0x%x\n",
					       fdvt_info.write_req_idx,
					       fdvt_info.read_req_idx);
			}
#endif

#else
			fdvt_info.irq_info.status
				[FDVT_IRQ_TYPE_INT_FDVT_ST] |= FDVT_INT_ST;
			fdvt_info.irq_info.process_id
				[FDVT_PROCESS_ID_FDVT] = process_id;
#endif
		}
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(spinlock_lrq_ptr);

	if (result == MTRUE)
		wake_up_interruptible(&fdvt_info.wait_queue_head);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_INF,
		       "Irq_FDVT:%d, reg 0x%x : 0x%x, result:%d, FdvtHWSta:0x%x, fdvt_irq_cnt:0x%x, write_req_idx:0x%x, read_req_idx:0x%x\n",
		       irq, FDVT_INT_HW, status, result, status,
		       fdvt_info.irq_info.fdvt_irq_cnt,
		       fdvt_info.write_req_idx, fdvt_info.read_req_idx);
	/* IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_INF,
	 * "FdvtHWSta:0x%x, FdvtHWSta:0x%x,
	 * DpeDveSta0:0x%x\n", DveStatus, status, DpeDveSta0);
	 */

	if (status & FDVT_INT_ST)
		tasklet_schedule(
			fdvt_tasklet[FDVT_IRQ_TYPE_INT_FDVT_ST].pFDVT_tkt);

	return IRQ_HANDLED;
}

static void isp_tasklet_func_fdvt(unsigned long data)
{
	IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_ERR);
}

/*****************************************************************************
 *
 *****************************************************************************/
module_init(FDVT_Init);
module_exit(FDVT_Exit);
MODULE_DESCRIPTION("Camera FDVT driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");

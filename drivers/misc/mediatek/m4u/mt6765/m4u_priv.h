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

#ifndef __M4U_PRIV_H__
#define __M4U_PRIV_H__
#include <linux/ioctl.h>
#include <linux/fs.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include "m4u.h"
#include "m4u_reg.h"
#include "../2.0/m4u_pgtable.h"

#define M4UMSG(string, args...)	pr_info("[M4U] "string, ##args)
#define M4UINFO(string, args...) pr_info("[M4U] "string, ##args)

#if (defined(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	defined(CONFIG_MICROTRUST_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_TEE_GP_SUPPORT)
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define M4U_TEE_SERVICE_ENABLE
#elif defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define M4U_TEE_SERVICE_ENABLE
#endif
#endif

#include "m4u_hw.h"

#ifdef CONFIG_FPGA_EARLY_PORTING
///#define M4U_FPGAPORTING
#endif
#define M4U_PROFILE
#define M4U_DVT 0

#ifndef M4U_PROFILE
#define mmprofile_log_ex(...)
#define mmprofile_enable(...)
#define mmprofile_start(...)
#define mmprofile_enable_event(...)
#define mmp_event unsigned int
#else
#include <mmprofile.h>
#include <mmprofile_function.h>
#endif

#ifdef CONFIG_PM
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
#endif

#ifdef M4U_TEE_SERVICE_ENABLE
extern int gM4U_L2_enable;
#endif

extern void show_pte(struct mm_struct *mm, unsigned long addr);

#ifndef dmac_map_area
#define dmac_map_area __dma_map_area
#endif

#ifndef dmac_unmap_area
#define dmac_unmap_area __dma_unmap_area
#endif

#ifndef dmac_flush_range
#define dmac_flush_range __dma_flush_range
#endif

#ifndef outer_clean_all
#define outer_clean_all(...)
#endif

#ifndef outer_flush_all
#define outer_flush_all(...)
#endif


#if 1///def M4U_FPGAPORTING /*ION_TO_BE_IMPL*/
#define smp_inner_dcache_flush_all(...)
#else
extern void smp_inner_dcache_flush_all(void);
#endif

#include <linux/clk.h>

struct m4u_device {
	struct miscdevice dev;
	struct proc_dir_entry *m4u_dev_proc_entry;
	struct device *pDev[TOTAL_M4U_NUM];
	struct dentry *debug_root;
	unsigned long m4u_base[TOTAL_M4U_NUM];
	unsigned int irq_num[TOTAL_M4U_NUM];
	struct clk *infra_m4u;
};

struct m4u_domain {
	imu_pgd_t *pgd;
	dma_addr_t pgd_pa;
	struct mutex pgtable_mutex;
	unsigned int pgsize_bitmap;
};

struct m4u_buf_info {
	struct list_head link;
	unsigned long va;
	unsigned int mva;
	unsigned int size;
	M4U_PORT_ID port;
	unsigned int prot;
	unsigned int flags;
	struct sg_table *sg_table;

	unsigned int mva_align;
	unsigned int size_align;
	int seq_id;
	unsigned long mapped_kernel_va_for_debug;
};

struct M4U_MAU_STRUCT {
	M4U_PORT_ID port;
	bool write;
	unsigned int mva;
	unsigned int size;
	bool enable;
	bool force;
};

struct M4U_TF {
	M4U_PORT_ID port;
	bool fgEnable;
};

/* ================================ */
/* === define in m4u_mva.c========= */

typedef int(mva_buf_fn_t)(void *priv, unsigned int mva_start,
			  unsigned int mva_end, void *data);

void m4u_mvaGraph_init(void *priv_reserve);
void m4u_mvaGraph_dump_raw(void);
void m4u_mvaGraph_dump(void);
void *mva_get_priv_ext(unsigned int mva);
int mva_foreach_priv(mva_buf_fn_t *fn, void *data);
void *mva_get_priv(unsigned int mva);
unsigned int m4u_do_mva_alloc(unsigned long va, unsigned int size, void *priv);
unsigned int m4u_do_mva_alloc_fix(unsigned long va, unsigned int mva,
				  unsigned int size, void *priv);
unsigned int m4u_do_mva_alloc_start_from(unsigned long va, unsigned int mva,
					 unsigned int size, void *priv);
int m4u_do_mva_free(unsigned int mva, unsigned int size);

/* ================================= */
/* ==== define in m4u_pgtable.c===== */
void m4u_dump_pgtable(struct m4u_domain *domain, struct seq_file *seq);
void m4u_dump_pte_nolock(struct m4u_domain *domain, unsigned int mva);
void m4u_dump_pte(struct m4u_domain *domain, unsigned int mva);
int m4u_pgtable_init(struct m4u_device *m4u_dev,
		     struct m4u_domain *m4u_domain);
int m4u_map_4K(struct m4u_domain *m4u_domain, unsigned int mva, phys_addr_t pa,
	       unsigned int prot);
int m4u_clean_pte(struct m4u_domain *domain, unsigned int mva,
		  unsigned int size);

unsigned long m4u_get_pte(struct m4u_domain *domain, unsigned int mva);


/* ================================= */
/* ==== define in m4u_hw.c     ===== */
void m4u_invalid_tlb_by_range(struct m4u_domain *m4u_domain,
			      unsigned int mva_start, unsigned int mva_end);
struct m4u_domain *m4u_get_domain_by_port(M4U_PORT_ID port);
struct m4u_domain *m4u_get_domain_by_id(int id);
int m4u_get_domain_nr(void);
int m4u_reclaim_notify(int port, unsigned int mva, unsigned int size);
int m4u_hw_init(struct m4u_device *m4u_dev, int m4u_id);
int m4u_hw_deinit(struct m4u_device *m4u_dev, int m4u_id);
int m4u_reg_backup(void);
int m4u_reg_restore(void);
int m4u_insert_seq_range(M4U_PORT_ID port, unsigned int MVAStart,
			 unsigned int MVAEnd);
int m4u_invalid_seq_range_by_id(int port, int seq_id);
void m4u_print_port_status(struct seq_file *seq, int only_print_active);

int m4u_dump_main_tlb(int m4u_id, int m4u_slave_id);
int m4u_dump_pfh_tlb(int m4u_id);
int m4u_dump_victim_tlb(int m4u_id);
int m4u_domain_init(struct m4u_device *m4u_dev, void *priv_reserve);

/*int config_mau(struct M4U_MAU_STRUCT mau);*/
int m4u_enable_tf(int port, bool fgenable);


extern int gM4U_4G_DRAM_Mode;

/* ================================= */
/* ==== define in m4u.c     ===== */
int m4u_dump_buf_info(struct seq_file *seq);
int m4u_map_sgtable(struct m4u_domain *m4u_domain, unsigned int mva,
		    struct sg_table *sg_table, unsigned int size,
		    unsigned int prot);
int m4u_unmap(struct m4u_domain *domain, unsigned int mva, unsigned int size);


void m4u_get_pgd(struct m4u_client_t *client, M4U_PORT_ID port,
	void **pgd_va, void **pgd_pa, unsigned int *size);
unsigned long m4u_mva_to_pa(struct m4u_client_t *client,
	M4U_PORT_ID port, unsigned int mva);
int m4u_query_mva_info(unsigned int mva, unsigned int size,
	unsigned int *real_mva, unsigned int *real_size);

/* ================================= */
/* ==== define in m4u_debug.c ===== */
int m4u_debug_init(struct m4u_device *m4u_dev);

static inline dma_addr_t get_sg_phys(struct scatterlist *sg)
{
	dma_addr_t pa;

	pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

#define M4U_PGD_SIZE (16*1024)

#define M4U_LOG_LEVEL_HIGH    3
#define M4U_LOG_LEVEL_MID     2
#define M4U_LOG_LEVEL_LOW     1

extern int gM4U_log_level;
extern int gM4U_log_to_uart;
#define _M4ULOG(level, string, args...) \
	do {\
		if (level > gM4U_log_level) {\
			if (level > gM4U_log_to_uart)\
				pr_info("[M4U] "string, ##args);\
			else\
				pr_debug("[M4U] "string, ##args);\
		} \
	} while (0)

#define M4ULOG_LOW(string, args...) _M4ULOG(M4U_LOG_LEVEL_LOW, string, ##args)
#define M4ULOG_MID(string, args...) _M4ULOG(M4U_LOG_LEVEL_MID, string, ##args)
#define M4ULOG_HIGH(string, args...) _M4ULOG(M4U_LOG_LEVEL_HIGH, string, ##args)

#ifdef CONFIG_MTK_AEE_FEATURE
#define M4UERR(string, args...) do {\
	pr_info("[M4U] error:"string, ##args);  \
		aee_kernel_exception("M4U", "[M4U] error:"string, ##args);  \
	} while (0)

#define m4u_aee_print(string, args...) do {\
		char m4u_name[100];\
		snprintf(m4u_name, 100, "[M4U]"string, ##args); \
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		m4u_name, "[M4U] error"string, ##args); \
	pr_info("[M4U] error:"string, ##args);  \
	} while (0)
/*aee_kernel_warning(m4u_name, "[M4U] error:"string,##args); */
#else
#define M4UERR(string, args...) do {\
	pr_debug("[M4U] error:"string, ##args);  \
	} while (0)

#define m4u_aee_print(string, args...) do {\
		char m4u_name[100];\
		snprintf(m4u_name, 100, "[M4U]"string, ##args); \
	pr_debug("[M4U] error:"string, ##args);  \
	} while (0)

#endif
#define M4U_PRINT_SEQ(seq_file, fmt, args...) \
	do {\
		if (seq_file)\
			seq_printf(seq_file, fmt, ##args);\
		else\
			pr_info(fmt, ##args);\
	} while (0)

/* ======================================= */
/* ==== other macros ============ */
#define M4U_GET_PAGE_NUM(va, size) ((((va)&(PAGE_SIZE-1))+(size)+(PAGE_SIZE-1))>>12)
#define M4U_PAGE_MASK 0xfffL

enum M4U_MMP_TYPE {
	M4U_MMP_ALLOC_MVA = 0,
	M4U_MMP_DEALLOC_MVA,
	M4U_MMP_CONFIG_PORT,
	M4U_MMP_M4U_ERROR,
	M4U_MMP_CACHE_SYNC,
	M4U_MMP_TOGGLE_CG,
	M4U_MMP_MAX,
};
extern MMP_Event M4U_MMP_Events[M4U_MMP_MAX];

struct M4U_MOUDLE {
	M4U_PORT_ID port;
	unsigned long BufAddr;
	unsigned int BufSize;
	unsigned int prot;
	unsigned int MVAStart;
	unsigned int MVAEnd;
	unsigned int flags;
};

struct M4U_CACHE {
	M4U_PORT_ID port;
	enum M4U_CACHE_SYNC_ENUM eCacheSync;
	unsigned long va;
	unsigned int size;
	unsigned int mva;
};

struct M4U_DMA {
	M4U_PORT_ID port;
	enum M4U_DMA_TYPE eDMAType;
	enum M4U_DMA_DIR eDMADir;
	unsigned long va;
	unsigned int size;
	unsigned int mva;
};

/* IOCTL commnad */
#define MTK_M4U_MAGICNO 'g'
#define MTK_M4U_T_POWER_ON	    _IOW(MTK_M4U_MAGICNO, 0, int)
#define MTK_M4U_T_POWER_OFF	   _IOW(MTK_M4U_MAGICNO, 1, int)
#define MTK_M4U_T_DUMP_REG	    _IOW(MTK_M4U_MAGICNO, 2, int)
#define MTK_M4U_T_DUMP_INFO	   _IOW(MTK_M4U_MAGICNO, 3, int)
#define MTK_M4U_T_ALLOC_MVA	   _IOWR(MTK_M4U_MAGICNO, 4, int)
#define MTK_M4U_T_DEALLOC_MVA	 _IOW(MTK_M4U_MAGICNO, 5, int)
#define MTK_M4U_T_INSERT_TLB_RANGE    _IOW(MTK_M4U_MAGICNO, 6, int)
#define MTK_M4U_T_INVALID_TLB_RANGE   _IOW(MTK_M4U_MAGICNO, 7, int)
#define MTK_M4U_T_INVALID_TLB_ALL     _IOW(MTK_M4U_MAGICNO, 8, int)
#define MTK_M4U_T_MANUAL_INSERT_ENTRY _IOW(MTK_M4U_MAGICNO, 9, int)
#define MTK_M4U_T_CACHE_SYNC	  _IOW(MTK_M4U_MAGICNO, 10, int)
#define MTK_M4U_T_CONFIG_PORT	 _IOW(MTK_M4U_MAGICNO, 11, int)
#define MTK_M4U_T_CONFIG_ASSERT       _IOW(MTK_M4U_MAGICNO, 12, int)
#define MTK_M4U_T_INSERT_WRAP_RANGE   _IOW(MTK_M4U_MAGICNO, 13, int)
#define MTK_M4U_T_MONITOR_START       _IOW(MTK_M4U_MAGICNO, 14, int)
#define MTK_M4U_T_MONITOR_STOP	_IOW(MTK_M4U_MAGICNO, 15, int)
#define MTK_M4U_T_RESET_MVA_RELEASE_TLB  _IOW(MTK_M4U_MAGICNO, 16, int)
#define MTK_M4U_T_CONFIG_PORT_ROTATOR _IOW(MTK_M4U_MAGICNO, 17, int)
#define MTK_M4U_T_QUERY_MVA	   _IOW(MTK_M4U_MAGICNO, 18, int)
#define MTK_M4U_T_M4UDrv_CONSTRUCT    _IOW(MTK_M4U_MAGICNO, 19, int)
#define MTK_M4U_T_M4UDrv_DECONSTRUCT  _IOW(MTK_M4U_MAGICNO, 20, int)
#define MTK_M4U_T_DUMP_PAGETABLE      _IOW(MTK_M4U_MAGICNO, 21, int)
#define MTK_M4U_T_REGISTER_BUFFER     _IOW(MTK_M4U_MAGICNO, 22, int)
#define MTK_M4U_T_CACHE_FLUSH_ALL     _IOW(MTK_M4U_MAGICNO, 23, int)
#define MTK_M4U_T_CONFIG_PORT_ARRAY   _IOW(MTK_M4U_MAGICNO, 26, int)
#define MTK_M4U_T_CONFIG_MAU	  _IOW(MTK_M4U_MAGICNO, 27, int)
#define MTK_M4U_T_CONFIG_TF	   _IOW(MTK_M4U_MAGICNO, 28, int)
#define MTK_M4U_T_DMA_OP	      _IOW(MTK_M4U_MAGICNO, 29, int)

#define MTK_M4U_T_SEC_INIT	    _IOW(MTK_M4U_MAGICNO, 50, int)

#ifdef M4U_TEE_SERVICE_ENABLE
int m4u_config_port_tee(M4U_PORT_STRUCT *pM4uPort);
int m4u_larb_backup_sec(unsigned int larb_idx);
int m4u_larb_restore_sec(unsigned int larb_idx);
int m4u_config_port_array_tee(unsigned char *port_array);
int m4u_sec_init(void);
int larb_clock_on(int larb, bool config_mtcmos);
int larb_clock_off(int larb, bool config_mtcmos);
#endif

#endif

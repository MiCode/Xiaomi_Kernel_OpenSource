/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Honghui Zhang <honghui.zhang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_IOMMU_EXT_H_
#define _MTK_IOMMU_EXT_H_
#include <linux/io.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif

#define MMU_INT_REPORT(mmu, mmu_2nd_id, id) \
	pr_notice( \
	"iommu%d_%d " #id "(0x%x) int happens!!\n",\
		mmu, mmu_2nd_id, id)

#ifdef CONFIG_MTK_AEE_FEATURE
#define mmu_aee_print(string, args...) do {\
	char mmu_name[100];\
	snprintf(mmu_name, 100, "[MTK_IOMMU]"string, ##args); \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		mmu_name, "[MTK_IOMMU] error"string, ##args); \
	pr_info("[MTK_IOMMU] error:"string, ##args);  \
	} while (0)
#else
#define mmu_aee_print(string, args...) do {\
		char mmu_name[100];\
		snprintf(mmu_name, 100, "[MTK_IOMMU]"string, ##args); \
		pr_info("[MTK_IOMMU] error:"string, ##args);  \
	} while (0)

#endif

struct IOMMU_PERF_COUNT {
	unsigned int transaction_cnt;
	unsigned int main_tlb_miss_cnt;
	unsigned int pfh_tlb_miss_cnt;
	unsigned int pfh_cnt;
	unsigned int rs_perf_cnt;
};

struct mmu_tlb_t {
	unsigned int tag;
	unsigned int desc;
};

struct mmu_pfh_tlb_t {
	unsigned int va;
	unsigned int va_msk;
	char layer;
	char x16;
	char sec;
	char pfh;
	char valid;
	unsigned int desc[32];
	int set;
	int way;
	unsigned int page_size;
	unsigned int tag;
};

extern phys_addr_t mtkfb_get_fb_base(void);
size_t mtkfb_get_fb_size(void);
int smi_reg_backup_sec(void);
int smi_reg_restore_sec(void);

enum mtk_iommu_callback_ret_t {
	MTK_IOMMU_CALLBACK_HANDLED,
	MTK_IOMMU_CALLBACK_NOT_HANDLED,
};

typedef enum mtk_iommu_callback_ret_t (*mtk_iommu_fault_callback_t)(int port,
				unsigned long mva, void *cb_data);

int mtk_iommu_register_fault_callback(int port,
				      mtk_iommu_fault_callback_t fn,
				      void *cb_data);
int mtk_iommu_unregister_fault_callback(int port);
int mtk_iommu_enable_tf(int port, bool fgenable);
int mtk_iommu_iova_to_pa(struct device *dev,
			 dma_addr_t iova, unsigned long *pa);
int mtk_iommu_iova_to_va(struct device *dev,
			 dma_addr_t iova,
			 unsigned long *map_va,
			 size_t size);

bool enable_custom_tf_report(void);
bool report_custom_iommu_fault(
	void __iomem	*base,
	unsigned int	int_state,
	unsigned long	fault_iova,
	unsigned long	fault_pa,
	unsigned int	fault_id, bool is_vpu);

void mtk_iommu_debug_init(void);
void mtk_iommu_debug_reset(void);

#define DOMAIN_NUM (1 << \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT - 32))

enum ION_M4U_DOMAIN {
	DOMAIN_OF_4GB,
#if (DOMAIN_NUM > 1)
	DOMAIN_OF_8GB,
#endif
#if (DOMAIN_NUM > 2)
	DOMAIN_OF_12GB,
#endif
#if (DOMAIN_NUM > 3)
	DOMAIN_OF_16GB,
#endif
#if (DOMAIN_NUM > 4)
	DOMAIN_OF_32GB,
#endif
#if (DOMAIN_NUM > 5)
	DOMAIN_OF_64GB,
#endif
};

enum IOMMU_PROFILE_TYPE {
	IOMMU_ALLOC = 0,
	IOMMU_DEALLOC,
	IOMMU_MAP,
	IOMMU_UNMAP,
	IOMMU_EVENT_MAX,
};

void mtk_iommu_trace_map(unsigned long orig_iova,
			 phys_addr_t orig_pa,
			 size_t size);
void mtk_iommu_trace_unmap(unsigned long orig_iova,
			   size_t size,
			   size_t unmapped);
void mtk_iommu_trace_log(int event,
			 unsigned int data1,
			 unsigned int data2,
			 unsigned int data3);

void mtk_iommu_log_dump(void *seq_file);
int m4u_user2kernel_port(int userport);

int mtk_iommu_get_pgtable_base_addr(unsigned int *pgd_pa);
int mtk_iommu_port_clock_switch(unsigned int port, bool enable);
int mtk_iommu_larb_clock_switch(unsigned int larb, bool enable);
unsigned int mtk_get_iommu_index(unsigned int larb);
char *iommu_get_port_name(int port);
int mtk_iommu_get_larb_port(unsigned int tf_id, unsigned int m4uid,
		unsigned int *larb, unsigned int *port);
int mtk_iommu_switch_acp(struct device *dev,
			  unsigned long iova, size_t size, bool is_acp);
#if 0
void mtk_smi_larb_put(struct device *larbdev);
int mtk_smi_larb_ready(int larbid);
#endif

struct mau_config_info {
	int m4u_id;
	int slave;
	int mau;
	unsigned int start;
	unsigned int end;
	unsigned int port_mask;
	unsigned int larb_mask;
	unsigned int write_monitor;	/* :1; */
	unsigned int virt;	/* :1; */
	unsigned int io;	/* :1; */
	unsigned int start_bit32;	/* :1; */
	unsigned int end_bit32;	/* :1; */
};

int mau_start_monitor(unsigned int m4u_id, unsigned int slave,
		      unsigned int mau,
		      int wr, int vir, int io, int bit32,
		      unsigned int start, unsigned int end,
		      unsigned int port_mask, unsigned int larb_mask);
void mau_stop_monitor(unsigned int m4u_id, unsigned int slave,
		      unsigned int mau);
int iommu_perf_monitor_start(int m4u_id);
int iommu_perf_monitor_stop(int m4u_id);
void iommu_perf_print_counter(int m4u_index,
		      int m4u_slave_id, const char *msg);
char *mtk_iommu_get_vpu_port_name(unsigned int tf_id);
char *mtk_iommu_get_mm_port_name(unsigned int tf_id);
int mtk_dump_main_tlb(int m4u_id, int m4u_slave_id);
int mtk_dump_pfh_tlb(int m4u_id);
int mtk_iommu_dump_reg(int m4u_index, unsigned int start,
		unsigned int end);
int mtk_iommu_get_boundary_id(struct device *dev);
int mtk_iommu_get_iova_space(struct device *dev,
		unsigned long *base, unsigned long *max,
		struct list_head *list);
void mtk_iommu_put_iova_space(struct device *dev,
		struct list_head *list);
void mtk_iommu_dump_iova_space(void);
unsigned int mtk_iommu_get_larb_port_count(unsigned int larb);
int mtk_iommu_atf_call(unsigned int cmd, unsigned int m4u_id,
		unsigned int bank);
void mtk_iommu_atf_test(unsigned int m4u_id, unsigned int cmd);
bool mtk_dev_is_size_alignment(struct device *dev);
char *mtk_iommu_get_port_name(unsigned int m4u_id,
		unsigned int tf_id);
void mtk_dump_reg_for_hang_issue(void);
void mtk_iommu_switch_tf_test(bool enable, const char *msg);
#endif

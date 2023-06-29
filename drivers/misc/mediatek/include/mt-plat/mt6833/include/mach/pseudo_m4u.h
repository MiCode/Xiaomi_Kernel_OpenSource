/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __PSEUDO_M4U_H__
#define __PSEUDO_M4U_H__

#include <linux/ioctl.h>
#include <linux/fs.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#include <linux/seq_file.h>
#include <dt-bindings/memory/mt6833-larb-port.h>
#include <linux/list.h>
#include <linux/iova.h>
#include <linux/iommu.h>

#if (defined(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	defined(CONFIG_MICROTRUST_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_TEE_GP_SUPPORT)
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) && \
	!defined(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)
#define PSEUDO_M4U_TEE_SERVICE_ENABLE
#elif defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define PSEUDO_M4U_TEE_SERVICE_ENABLE
#elif defined(CONFIG_MTK_GZ_SUPPORT_SDSP)
#define PSEUDO_M4U_TEE_SERVICE_ENABLE
#endif
#endif

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define M4U_MTEE_SERVICE_ENABLE
#elif defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define M4U_MTEE_SERVICE_ENABLE
#elif defined(CONFIG_MTK_GZ_SUPPORT_SDSP)
#define M4U_MTEE_SERVICE_ENABLE
#endif
#endif

#if defined(CONFIG_MTK_CAM_GENIEZONE_SUPPORT) || \
	defined(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)
#define M4U_GZ_SERVICE_ENABLE
#endif

#define M4U_PAGE_SIZE		(0x1000)

#define M4U_DEVNAME		"m4u"

#define TOTAL_M4U_NUM		(2)
#define M4U_REG_SIZE		(0x5e0)
#ifndef SMI_LARB_NR
#define SMI_LARB_NR		(21)
#endif
#define ONE_SMI_PORT_NR		(32)


#define M4U_PAGE_MASK		(0xfff)

/* public flags */
/* engine access this buffer in sequncial way. */
#define M4U_FLAGS_SEQ_ACCESS	(1<<0)
/* fix allocation, we will use mva user specified. */
#define M4U_FLAGS_FIX_MVA	(1<<1)
/* the mva will share in SWd */
#define M4U_FLAGS_SEC_SHAREABLE	(1<<2)
/* the allocator will search free mva from user specified.  */
#define M4U_FLAGS_START_FROM	(1<<3)
#define M4U_FLAGS_SG_READY	(1<<4)
#ifdef CONFIG_MTK_IOMMU_V2
#define M4U_FLAGS_TO_ACP	(1<<5)
#endif

/* port related: virtuality, security, distance */
struct M4U_PORT_STRUCT {
	int ePortID;
	unsigned int Virtuality;
	unsigned int Security;
	unsigned int domain;            /*domain : 0 1 2 3*/
	unsigned int Distance;
	unsigned int Direction;         /* 0:- 1:+*/
	char name[128];
};

/* module related:  alloc/dealloc MVA buffer */
struct M4U_MOUDLE_STRUCT {
	int port;
	unsigned long BufAddr;
	unsigned long BufSize;
	unsigned int prot;
	unsigned long MVAStart;
	unsigned long MVAEnd;
	unsigned int flags;

};

struct mva_info_t {
	struct list_head link;
	unsigned long bufAddr;
	unsigned long mvaStart;
	unsigned long size;
	int eModuleId;
	unsigned int flags;
	int security;
	int cache_coherent;
	unsigned int mapped_kernel_va_for_debug;
};

struct m4u_buf_info_t {
	struct list_head link;
	unsigned long va;
	unsigned long mva;
	unsigned long size;
	int port;
	unsigned int prot;
	unsigned int flags;
	struct sg_table *sg_table;
	unsigned long mva_align;
	unsigned long size_align;
	int seq_id;
	unsigned long mapped_kernel_va_for_debug;
	unsigned long long timestamp;
	char task_comm[TASK_COMM_LEN];
	pid_t pid;
};

struct m4u_client_t {
	/* mutex to protect mvaList */
	/* should get this mutex whenever add/delete/interate mvaList */
	struct mutex dataMutex;
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head mvaList;
	unsigned long count;
};

struct port_mva_info_t {
	int emoduleid;
	unsigned long va;
	unsigned long buf_size;
	int security;
	int cache_coherent;
	unsigned int flags;
	unsigned int iova_start;
	unsigned int iova_end;
	unsigned long mva;
};

#if IS_ENABLED(CONFIG_PROC_FS)
#define DEFINE_PROC_ATTRIBUTE(__fops, __get, __set, __fmt)		  \
static int __fops ## _open(struct inode *inode, struct file *file)	  \
{									  \
	struct inode local_inode = *inode;				  \
									  \
	local_inode.i_private = PDE_DATA(inode);			  \
	__simple_attr_check_format(__fmt, 0ull);			  \
	return simple_attr_open(&local_inode, file, __get, __set, __fmt); \
}									  \
static const struct file_operations __fops = {				  \
	.owner	 = THIS_MODULE,						  \
	.open	 = __fops ## _open,					  \
	.release = simple_attr_release,					  \
	.read	 = simple_attr_read,					  \
	.write	 = simple_attr_write,					  \
	.llseek	 = generic_file_llseek,					  \
}
#endif

struct m4u_device {
	struct proc_dir_entry *m4u_dev_proc_entry;
#if IS_ENABLED(CONFIG_DEBUG_FS)
		struct dentry *debug_root;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
		struct proc_dir_entry *proc_root;
#endif
};

/* we use this for trace the mva<-->sg_table relation ship */
struct mva_sglist {
	struct list_head list;
	unsigned long mva;
	struct sg_table *table;
};

struct m4u_port_array {
#define M4U_PORT_ATTR_EN	(1<<0)
#define M4U_PORT_ATTR_VIRTUAL	(1<<1)
#define M4U_PORT_ATTR_SEC	(1<<2)
unsigned char ports[M4U_PORT_NR];
};


extern unsigned long max_pfn;
extern unsigned char *pMlock_cnt;
extern unsigned int mlock_cnt_size;
extern unsigned long pagetable_pa;

struct sg_table *m4u_create_sgtable(unsigned long va, unsigned long size);
int m4u_dealloc_mva_sg(int eModuleID,
		       struct sg_table *sg_table,
		       const unsigned long BufSize,
		       const unsigned long MVA);
int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
		     struct sg_table *sg_table);

int m4u_mva_map_kernel(unsigned long mva,
	unsigned long size, unsigned long *map_va,
	unsigned long *map_size, struct sg_table *table);
int m4u_mva_unmap_kernel(unsigned long mva, unsigned long size,
		       unsigned long va);
#ifndef IOVA_PFN
#define IOVA_PFN(addr)		((addr) >> PAGE_SHIFT)
#endif

int pseudo_get_reg_of_path(unsigned int port, bool is_va,
			unsigned int *reg, unsigned int *mask,
			unsigned int *value);
int m4u_config_port(struct M4U_PORT_STRUCT *pM4uPort);
int m4u_get_boundary(int port);
int pseudo_config_port_tee(int kernelport);
int m4u_switch_acp(unsigned int port,
		unsigned long iova, size_t size, bool is_acp);
void pseudo_m4u_db_debug(unsigned int m4uid,
		struct seq_file *s);
int m4u_get_dma_buf_port(struct device *dev);

static inline bool m4u_enable_4G(void)
{
	return (max_pfn > (0xffffffffUL >> PAGE_SHIFT));
}
struct iova *__alloc_iova(struct iova_domain *iovad, size_t size,
		dma_addr_t dma_limit);
void __free_iova(struct iova_domain *iovad, struct iova *iova);
void __iommu_dma_unmap(struct iommu_domain *domain, dma_addr_t dma_addr);

int pseudo_m4u_sec_init(int mtk_iommu_sec_id);

/* IOCTL commnad */
#define MTK_M4U_MAGICNO			'g'
#define MTK_M4U_T_POWER_ON		_IOW(MTK_M4U_MAGICNO, 0, int)
#define MTK_M4U_T_POWER_OFF		_IOW(MTK_M4U_MAGICNO, 1, int)
#define MTK_M4U_T_DUMP_REG		_IOW(MTK_M4U_MAGICNO, 2, int)
#define MTK_M4U_T_DUMP_INFO		_IOW(MTK_M4U_MAGICNO, 3, int)
#define MTK_M4U_T_ALLOC_MVA		_IOWR(MTK_M4U_MAGICNO, 4, int)
#define MTK_M4U_T_DEALLOC_MVA		_IOW(MTK_M4U_MAGICNO, 5, int)
#define MTK_M4U_T_INSERT_TLB_RANGE	_IOW(MTK_M4U_MAGICNO, 6, int)
#define MTK_M4U_T_INVALID_TLB_RANGE	_IOW(MTK_M4U_MAGICNO, 7, int)
#define MTK_M4U_T_INVALID_TLB_ALL	_IOW(MTK_M4U_MAGICNO, 8, int)
#define MTK_M4U_T_MANUAL_INSERT_ENTRY	_IOW(MTK_M4U_MAGICNO, 9, int)
#define MTK_M4U_T_CACHE_SYNC		_IOW(MTK_M4U_MAGICNO, 10, int)
#define MTK_M4U_T_CONFIG_PORT		_IOW(MTK_M4U_MAGICNO, 11, int)
#define MTK_M4U_T_CONFIG_ASSERT		_IOW(MTK_M4U_MAGICNO, 12, int)
#define MTK_M4U_T_INSERT_WRAP_RANGE	_IOW(MTK_M4U_MAGICNO, 13, int)
#define MTK_M4U_T_MONITOR_START		_IOW(MTK_M4U_MAGICNO, 14, int)
#define MTK_M4U_T_MONITOR_STOP		_IOW(MTK_M4U_MAGICNO, 15, int)
#define MTK_M4U_T_RESET_MVA_RELEASE_TLB	_IOW(MTK_M4U_MAGICNO, 16, int)
#define MTK_M4U_T_CONFIG_PORT_ROTATOR	_IOW(MTK_M4U_MAGICNO, 17, int)
#define MTK_M4U_T_QUERY_MVA		_IOW(MTK_M4U_MAGICNO, 18, int)
#define MTK_M4U_T_M4UDrv_CONSTRUCT	_IOW(MTK_M4U_MAGICNO, 19, int)
#define MTK_M4U_T_M4UDrv_DECONSTRUCT	_IOW(MTK_M4U_MAGICNO, 20, int)
#define MTK_M4U_T_DUMP_PAGETABLE	_IOW(MTK_M4U_MAGICNO, 21, int)
#define MTK_M4U_T_REGISTER_BUFFER	_IOW(MTK_M4U_MAGICNO, 22, int)
#define MTK_M4U_T_CACHE_FLUSH_ALL	_IOW(MTK_M4U_MAGICNO, 23, int)
#define MTK_M4U_T_CONFIG_PORT_ARRAY	_IOW(MTK_M4U_MAGICNO, 26, int)
#define MTK_M4U_T_CONFIG_MAU		_IOW(MTK_M4U_MAGICNO, 27, int)
#define MTK_M4U_T_CONFIG_TF		_IOW(MTK_M4U_MAGICNO, 28, int)
#define MTK_M4U_T_DMA_OP		_IOW(MTK_M4U_MAGICNO, 29, int)
#define MTK_M4U_T_SEC_INIT		_IOW(MTK_M4U_MAGICNO, 50, int)
#define MTK_M4U_GZ_SEC_INIT		_IOW(MTK_M4U_MAGICNO, 60, int)

#if IS_ENABLED(CONFIG_COMPAT)
struct COMPAT_M4U_MOUDLE_STRUCT {
	compat_uint_t port;
	compat_ulong_t BufAddr;
	compat_ulong_t BufSize;
	compat_uint_t prot;
	compat_ulong_t MVAStart;
	compat_ulong_t MVAEnd;
	compat_uint_t flags;
};

#define COMPAT_MTK_M4U_T_ALLOC_MVA	_IOWR(MTK_M4U_MAGICNO, 4, int)
#define COMPAT_MTK_M4U_T_DEALLOC_MVA	_IOW(MTK_M4U_MAGICNO, 5, int)
#define COMPAT_MTK_M4U_T_SEC_INIT	_IOW(MTK_M4U_MAGICNO, 50, int)
#endif

#ifdef M4U_MTEE_SERVICE_ENABLE
extern bool m4u_tee_en;
int smi_reg_restore_sec(void);
int smi_reg_backup_sec(void);
/*int m4u_config_port_array_tee(unsigned char* port_array);*/
int m4u_dump_secpgd(unsigned int port, unsigned long faultmva);
#endif
int pseudo_debug_init(struct m4u_device *data);
int pseudo_get_iova_space(int port,
		unsigned long *base, unsigned long *max,
		struct list_head *list);
void pseudo_put_iova_space(int port,
		struct list_head *list);
void m4u_dump_pgtable(unsigned int level, unsigned long target);
void __m4u_dump_pgtable(struct seq_file *s, unsigned int level,
		bool lock, unsigned long target);
int pseudo_dump_port(int port, bool ignore_power);
int pseudo_dump_all_port_status(struct seq_file *s);
int pseudo_dump_iova_reserved_region(struct seq_file *s);

/* =========== register defination =========== */
#define F_VAL(val, msb, lsb)		(((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_VAL_L(val, msb, lsb)		(((val)&((1L<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)			F_VAL(0xffffffff, msb, lsb)
#define F_MSK_L(msb, lsb)		F_VAL_L(0xffffffffffffffff, msb, lsb)
#define F_BIT_SET(bit)			(1<<(bit))
#define F_BIT_VAL(val, bit)		((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb)	(((regval)&F_MSK(msb, lsb))>>lsb)

#define SMI_LARB_NON_SEC_CONx(larb_port)(0x380 + ((larb_port)<<2))
#define F_SMI_NON_SEC_MMU_EN(en)	F_BIT_VAL(en, 0)
#define F_SMI_MMU_EN			F_BIT_SET(0)
#define F_SMI_ADDR_BIT32		F_MSK(15, 8)
#define F_SMI_ADDR_BIT32_VAL(regval)	F_MSK_SHIFT(regval, 15, 8)

#define SMI_LARB_SEC_CONx(larb_port)	(0xf80 + ((larb_port)<<2))
#define F_SMI_SEC_MMU_EN(en)		F_BIT_VAL(en, 0)
#define F_SMI_SEC_EN(sec)		F_BIT_VAL(sec, 1)
#define F_SMI_DOMN(domain)		F_VAL(domain, 8, 4)
#define F_SMI_DOMN_VAL(regval)		F_MSK_SHIFT(regval, 8, 4)

#endif

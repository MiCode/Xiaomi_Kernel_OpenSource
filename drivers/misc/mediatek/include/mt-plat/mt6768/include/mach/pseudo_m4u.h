/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __PSEUDO_M4U_PORT_H__
#define __PSEUDO_M4U_PORT_H__

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <dt-bindings/memory/mt6768-larb-port.h>
#include <linux/list.h>
#include <linux/iova.h>
#include <linux/iommu.h>

#if (defined(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	defined(CONFIG_MICROTRUST_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_TEE_GP_SUPPORT)
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define PSEUDO_M4U_TEE_SERVICE_ENABLE
#elif defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define PSEUDO_M4U_TEE_SERVICE_ENABLE
#endif
#endif

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define M4U_MTEE_SERVICE_ENABLE
#elif defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define M4U_MTEE_SERVICE_ENABLE
#endif
#endif

#define M4U_PAGE_SIZE	0x1000

#define M4U_DEVNAME "m4u"

#define NORMAL_M4U_NUMBER	1
#define TOTAL_M4U_NUM	1
#define M4U_REG_SIZE	0x5e0
#ifndef SMI_LARB_NR
#define SMI_LARB_NR MTK_IOMMU_LARB_NR
#endif

#define M4U_PAGE_MASK	0xfff

/* public flags */
/* engine access this buffer in sequncial way. */
#define M4U_FLAGS_SEQ_ACCESS (1<<0)
/* fix allocation, we will use mva user specified. */
#define M4U_FLAGS_FIX_MVA   (1<<1)
/* the mva will share in SWd */
#define M4U_FLAGS_SEC_SHAREABLE   (1<<2)
/* the allocator will search free mva from user specified.  */
#define M4U_FLAGS_START_FROM   (1<<3)
#define M4U_FLAGS_SG_READY   (1<<4)

/* port related: virtuality, security, distance */
struct M4U_PORT_STRUCT {
	int ePortID;
	unsigned int Virtuality;
	unsigned int Security;
	unsigned int domain;            /*domain : 0 1 2 3*/
	unsigned int Distance;
	unsigned int Direction;         /* 0:- 1:+*/
};

/* module related:  alloc/dealloc MVA buffer */
struct M4U_MOUDLE_STRUCT {
	int port;
	unsigned long BufAddr;
	unsigned int BufSize;
	unsigned int prot;
	unsigned int MVAStart;
	unsigned int MVAEnd;
	unsigned int flags;

};

struct mva_info_t {
	struct list_head link;
	unsigned long bufAddr;
	unsigned int mvaStart;
	unsigned int size;
	int eModuleId;
	unsigned int flags;
	int security;
	int cache_coherent;
	unsigned int mapped_kernel_va_for_debug;
};

struct m4u_buf_info_t {
	struct list_head link;
	unsigned long va;
	unsigned int mva;
	unsigned int size;
	int port;
	unsigned int prot;
	unsigned int flags;
	struct sg_table *sg_table;
	unsigned int mva_align;
	unsigned int size_align;
	int seq_id;
	unsigned long mapped_kernel_va_for_debug;
};

struct m4u_client_t {
	/* mutex to protect mvaList */
	/* should get this mutex whenever add/delete/interate mvaList */
	struct mutex dataMutex;
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head mvaList;
};

struct port_mva_info_t {
	int emoduleid;
	unsigned long va;
	unsigned int buf_size;
	int security;
	int cache_coherent;
	unsigned int flags;
	unsigned int iova_start;
	unsigned int iova_end;
	unsigned int mva;
};


struct m4u_device {
	/*struct miscdevice dev;*/
	struct proc_dir_entry *m4u_dev_proc_entry;
};

/* we use this for trace the mva<-->sg_table relation ship */
struct mva_sglist {
	struct list_head list;
	unsigned int mva;
	struct iova *iova;
	struct sg_table *table;
};

struct pseudo_device {
	struct device *dev;	/* record the device for config mmu use. */
	int larbid;		/* the larb id of this device*/
	bool mmuen;		/* whether this larb have been configed. */
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

struct sg_table *m4u_create_sgtable(unsigned long va, unsigned int size);
int m4u_dealloc_mva_sg(int eModuleID,
		       struct sg_table *sg_table,
		       const unsigned int BufSize,
		       const unsigned int MVA);
int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
		     struct sg_table *sg_table);

int m4u_mva_map_kernel(unsigned int mva, unsigned int size,
		       unsigned long *map_va, unsigned int *map_size);
int m4u_mva_unmap_kernel(unsigned int mva, unsigned int size,
		       unsigned long va);
#ifndef IOVA_PFN
#define IOVA_PFN(addr)		((addr) >> PAGE_SHIFT)
#endif

int m4u_config_port(struct M4U_PORT_STRUCT *pM4uPort);
int pseudo_config_port_tee(int kernelport);

static inline bool m4u_enable_4G(void)
{
	return (max_pfn > (0xffffffffUL >> PAGE_SHIFT));
}
struct iova *__alloc_iova(struct iova_domain *iovad, size_t size,
		dma_addr_t dma_limit);
void __free_iova(struct iova_domain *iovad, struct iova *iova);
void __iommu_dma_unmap(struct iommu_domain *domain, dma_addr_t dma_addr);


/* IOCTL commnad */
#define MTK_M4U_MAGICNO 'g'
#define MTK_M4U_T_POWER_ON            _IOW(MTK_M4U_MAGICNO, 0, int)
#define MTK_M4U_T_POWER_OFF           _IOW(MTK_M4U_MAGICNO, 1, int)
#define MTK_M4U_T_DUMP_REG            _IOW(MTK_M4U_MAGICNO, 2, int)
#define MTK_M4U_T_DUMP_INFO           _IOW(MTK_M4U_MAGICNO, 3, int)
#define MTK_M4U_T_ALLOC_MVA           _IOWR(MTK_M4U_MAGICNO, 4, int)
#define MTK_M4U_T_DEALLOC_MVA         _IOW(MTK_M4U_MAGICNO, 5, int)
#define MTK_M4U_T_INSERT_TLB_RANGE    _IOW(MTK_M4U_MAGICNO, 6, int)
#define MTK_M4U_T_INVALID_TLB_RANGE   _IOW(MTK_M4U_MAGICNO, 7, int)
#define MTK_M4U_T_INVALID_TLB_ALL     _IOW(MTK_M4U_MAGICNO, 8, int)
#define MTK_M4U_T_MANUAL_INSERT_ENTRY _IOW(MTK_M4U_MAGICNO, 9, int)
#define MTK_M4U_T_CACHE_SYNC          _IOW(MTK_M4U_MAGICNO, 10, int)
#define MTK_M4U_T_CONFIG_PORT         _IOW(MTK_M4U_MAGICNO, 11, int)
#define MTK_M4U_T_CONFIG_ASSERT       _IOW(MTK_M4U_MAGICNO, 12, int)
#define MTK_M4U_T_INSERT_WRAP_RANGE   _IOW(MTK_M4U_MAGICNO, 13, int)
#define MTK_M4U_T_MONITOR_START       _IOW(MTK_M4U_MAGICNO, 14, int)
#define MTK_M4U_T_MONITOR_STOP        _IOW(MTK_M4U_MAGICNO, 15, int)
#define MTK_M4U_T_RESET_MVA_RELEASE_TLB  _IOW(MTK_M4U_MAGICNO, 16, int)
#define MTK_M4U_T_CONFIG_PORT_ROTATOR _IOW(MTK_M4U_MAGICNO, 17, int)
#define MTK_M4U_T_QUERY_MVA           _IOW(MTK_M4U_MAGICNO, 18, int)
#define MTK_M4U_T_M4UDrv_CONSTRUCT    _IOW(MTK_M4U_MAGICNO, 19, int)
#define MTK_M4U_T_M4UDrv_DECONSTRUCT  _IOW(MTK_M4U_MAGICNO, 20, int)
#define MTK_M4U_T_DUMP_PAGETABLE      _IOW(MTK_M4U_MAGICNO, 21, int)
#define MTK_M4U_T_REGISTER_BUFFER     _IOW(MTK_M4U_MAGICNO, 22, int)
#define MTK_M4U_T_CACHE_FLUSH_ALL     _IOW(MTK_M4U_MAGICNO, 23, int)
#define MTK_M4U_T_CONFIG_PORT_ARRAY   _IOW(MTK_M4U_MAGICNO, 26, int)
#define MTK_M4U_T_CONFIG_MAU          _IOW(MTK_M4U_MAGICNO, 27, int)
#define MTK_M4U_T_CONFIG_TF           _IOW(MTK_M4U_MAGICNO, 28, int)
#define MTK_M4U_T_SEC_INIT            _IOW(MTK_M4U_MAGICNO, 50, int)

#if IS_ENABLED(CONFIG_COMPAT)
struct COMPAT_M4U_MOUDLE_STRUCT {
	compat_uint_t port;
	compat_ulong_t BufAddr;
	compat_uint_t BufSize;
	compat_uint_t prot;
	compat_uint_t MVAStart;
	compat_uint_t MVAEnd;
	compat_uint_t flags;
};

#define COMPAT_MTK_M4U_T_ALLOC_MVA	  _IOWR(MTK_M4U_MAGICNO, 4, int)
#define COMPAT_MTK_M4U_T_DEALLOC_MVA  _IOW(MTK_M4U_MAGICNO, 5, int)
#define COMPAT_MTK_M4U_T_CACHE_SYNC   _IOW(MTK_M4U_MAGICNO, 10, int)
#define COMPAT_MTK_M4U_T_SEC_INIT     _IOW(MTK_M4U_MAGICNO, 50, int)
#endif

#ifdef M4U_MTEE_SERVICE_ENABLE
extern bool m4u_tee_en;
int smi_reg_restore_sec(void);
int smi_reg_backup_sec(void);
/*int m4u_config_port_array_tee(unsigned char* port_array);*/
int m4u_dump_secpgd(unsigned int port, unsigned int faultmva);
#endif

/* =========== register defination =========== */
#define F_VAL(val, msb, lsb) (((val)&((1<<(msb-lsb+1))-1))<<lsb)
#define F_VAL_L(val, msb, lsb) (((val)&((1L<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)     F_VAL(0xffffffff, msb, lsb)
#define F_MSK_L(msb, lsb)     F_VAL_L(0xffffffffffffffff, msb, lsb)
#define F_BIT_SET(bit)	  (1<<(bit))
#define F_BIT_VAL(val, bit)  ((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb) (((regval)&F_MSK(msb, lsb))>>lsb)

#define SMI_LARB_NON_SEC_CONx(larb_port)	(0x380 + ((larb_port)<<2))
#define F_SMI_NON_SEC_MMU_EN(en)	F_BIT_VAL(en, 0)
#define F_SMI_MMU_EN	F_BIT_SET(0)

#define SMI_LARB_SEC_CONx(larb_port)	(0xf80 + ((larb_port)<<2))
#define F_SMI_SEC_MMU_EN(en)	F_BIT_VAL(en, 0)
#define F_SMI_SEC_EN(sec)	F_BIT_VAL(sec, 1)
#define F_SMI_DOMN(domain)	F_VAL(domain, 8, 4)

#endif

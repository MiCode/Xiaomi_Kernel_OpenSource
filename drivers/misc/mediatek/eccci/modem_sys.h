/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_SYS_H__
#define __MODEM_SYS_H__

#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_fsm.h"
#include "ccci_hif.h"
#include "ccci_port.h"

struct ccci_modem;

enum MD_COMM_TYPE {
	CCCI_MESSAGE,
	CCIF_INTERRUPT,
	CCIF_MPU_INTR,
};

enum MODEM_EE_FLAG {
	EE_FLAG_ENABLE_WDT = (1 << 0),
	EE_FLAG_DISABLE_WDT = (1 << 1),
};

enum LOGGING_MODE {
	MODE_UNKNOWN = -1,	  /* -1 */
	MODE_IDLE,			  /* 0 */
	MODE_USB,			   /* 1 */
	MODE_SD,				/* 2 */
	MODE_POLLING,		   /* 3 */
	MODE_WAITSD,			/* 4 */
};

#define MD_SETTING_ENABLE (1<<0)
#define MD_SETTING_RELOAD (1<<1)
#define MD_SETTING_FIRST_BOOT (1<<2)	/* this is the first time of boot up */
#define MD_SETTING_DUMMY  (1<<7)

#define MD_IMG_DUMP_SIZE (1<<8)
#define DSP_IMG_DUMP_SIZE (1<<9)

struct ccci_force_assert_shm_fmt {
	unsigned int  error_code;
	unsigned int  param[3];
	unsigned char reserved[0];
};

extern int current_time_zone;

struct ccci_dev_cfg {
	unsigned int major;
	unsigned int minor_base;
	unsigned int capability;
};

struct ccci_modem_ops {
	/* must-have */
	int (*init)(struct ccci_modem *md);
	int (*start)(struct ccci_modem *md);
	int (*pre_stop)(struct ccci_modem *md, unsigned int stop_type);
	int (*stop)(struct ccci_modem *md, unsigned int stop_type);
	int (*soft_start)(struct ccci_modem *md, unsigned int mode);
	int (*soft_stop)(struct ccci_modem *md, unsigned int mode);
	int (*start_queue)(struct ccci_modem *md,
		unsigned char qno, enum DIRECTION dir);
	int (*stop_queue)(struct ccci_modem *md,
		unsigned char qno, enum DIRECTION dir);
	int (*send_runtime_data)(struct ccci_modem *md, unsigned int tx_ch,
		unsigned int txqno, int skb_from_pool);
	int (*ee_handshake)(struct ccci_modem *md, int timeout);
	int (*force_assert)(struct ccci_modem *md, enum MD_COMM_TYPE type);
	int (*dump_info)(struct ccci_modem *md, enum MODEM_DUMP_FLAG flag,
		void *buff, int length);
	int (*ee_callback)(struct ccci_modem *md, enum MODEM_EE_FLAG flag);
	int (*send_ccb_tx_notify)(struct ccci_modem *md, int core_id);
	int (*reset_pccif)(struct ccci_modem *md);
};

struct md_sys1_info {
		int channel_id;		/* CCIF channel */
		atomic_t ccif_irq_enabled;
		unsigned int ap_ccif_irq_id;
		unsigned long ap_ccif_irq_flags;

		void __iomem *md_global_con0;

#ifdef MD_PEER_WAKEUP
		void __iomem *md_peer_wakeup;
#endif
		char peer_wakelock_name[32];
		struct wakeup_source *peer_wake_lock;

		void __iomem *md_bus_status;
		void __iomem *md_pc_monitor;
		void __iomem *md_topsm_status;
		void __iomem *md_ost_status;
		void __iomem *md_pll;
};

struct ccci_modem {
	unsigned char *private_data;

	struct ccci_modem_ops *ops;
	/* refer to port_proxy obj, no need used in sub class,
	 * if realy want to use, please define delegant api
	 * in ccci_modem class
	 */
	struct kobject kobj;
	struct ccci_mem_layout mem_layout;
	unsigned int sbp_code;
	unsigned int mdlg_mode;
	struct platform_device *plat_dev;
	/*
	 * The following members are readonly for CCCI core.
	 * They are maintained by modem and port_kernel.c.
	 * port_kernel.c should not be considered as part of CCCI core,
	 * we just move common part of modem message handling into this file.
	 * Current modem all follows the same message protocol during bootup
	 * and exception. if future modem abandoned this protocl, we can
	 * simply replace function set of kernel port to support it.
	 */
	unsigned int is_in_ee_dump;
	unsigned int is_force_asserted;
	phys_addr_t invalid_remap_base;
	int runtime_version;
	int multi_md_mpu_support;

	unsigned int md_wdt_irq_id;
	unsigned long md_wdt_irq_flags;
	atomic_t wdt_enabled;
	char trm_wakelock_name[32];
	struct wakeup_source *trm_wake_lock;
	atomic_t reset_on_going;

	unsigned int hif_flag;
	struct md_hw_info *hw_info;

	struct ccci_per_md per_md_data;
	void *ioremap_buff_src;
};

extern struct ccci_modem *modem_sys;

/****************************************************************************/
/* API Region called by sub-modem class, reuseable API */
/****************************************************************************/
struct ccci_modem *ccci_md_alloc(int private_size);
int ccci_md_register(struct ccci_modem *modem);

static inline struct ccci_modem *ccci_get_modem(void)
{
	return modem_sys;
}

static inline struct device *ccci_md_get_dev(void)
{
	return &(modem_sys->plat_dev->dev);
}

static inline void *ccci_md_get_hw_info(void)
{
	return modem_sys->hw_info;
}

static inline int ccci_md_recv_skb(unsigned char hif_id, struct sk_buff *skb)
{
	int flag = NORMAL_DATA;

	if (hif_id == MD1_NET_HIF)
		flag = CLDMA_NET_DATA;
	return ccci_port_recv_skb(hif_id, skb, flag);
}

/***************************************************************************/
/* API Region called by ccci modem object */
/***************************************************************************/

extern bool spm_is_md1_sleep(void);

//extern unsigned int trace_sample_time;

extern u32 mt_irq_get_pending(unsigned int irq);

extern int ccci_modem_init_common(struct platform_device *plat_dev,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *md_hw);

extern int mrdump_mini_add_extra_file(unsigned long vaddr, unsigned long paddr,
	unsigned long size, const char *name);
#if IS_ENABLED(CONFIG_MTK_IRQ_DBG)
extern void mt_irq_dump_status(unsigned int irq);
#endif
extern atomic_t en_flight_timeout;
extern atomic_t md_dapc_ke_occurred;

int ccci_md_start(void);
int ccci_md_soft_start(unsigned int sim_mode);
int ccci_md_send_runtime_data(void);
void ccci_md_dump_info(enum MODEM_DUMP_FLAG flag,
	void *buff, int length);
int ccci_md_pre_stop(unsigned int stop_type);
int ccci_md_stop(unsigned int stop_type);
int ccci_md_soft_stop(unsigned int sim_mode);
//int ccci_md_force_assert(enum MD_FORCE_ASSERT_TYPE type,
//	char *param, int len);
void ccci_md_exception_handshake(int timeout);
//int ccci_md_send_ccb_tx_notify(int core_id);
int ccci_md_pre_start(void);
int ccci_md_post_start(void);

#endif	/* __CCCI_MODEM_H__ */

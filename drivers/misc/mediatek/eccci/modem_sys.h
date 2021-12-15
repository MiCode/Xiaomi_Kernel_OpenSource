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

enum LOW_POEWR_NOTIFY_TYPE {
	LOW_BATTERY,
	BATTERY_PERCENT,
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

#define NORMAL_BOOT_ID 0
#define META_BOOT_ID 1
#define FACTORY_BOOT_ID	2

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
	unsigned int index;
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
	int (*low_power_notify)(struct ccci_modem *md,
		enum LOW_POEWR_NOTIFY_TYPE type, int level);
	int (*ee_callback)(struct ccci_modem *md, enum MODEM_EE_FLAG flag);
	int (*send_ccb_tx_notify)(struct ccci_modem *md, int core_id);
	int (*reset_pccif)(struct ccci_modem *md);
};

struct md_sys1_info {
		void __iomem *ap_ccif_base;
		void __iomem *md_ccif_base;
		int channel_id;		/* CCIF channel */
		atomic_t ccif_irq_enabled;
		unsigned int ap_ccif_irq_id;
		unsigned long ap_ccif_irq_flags;

#ifdef FEATURE_SCP_CCCI_SUPPORT
		struct work_struct scp_md_state_sync_work;
#endif
		void __iomem *md_rgu_base;
		void __iomem *l1_rgu_base;
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
		struct md_pll_reg *md_pll_base;

		void __iomem *md_boot_slave_Vector;
		void __iomem *md_boot_slave_Key;
		void __iomem *md_boot_slave_En;
};

struct md_sys3_info {
		void __iomem *md_rgu_base;
		void __iomem *ccirq_base[4];
		void __iomem *c2k_cgbr1_addr;
};

struct ccci_modem {
	unsigned char index;
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
};

extern struct ccci_modem *modem_sys[MAX_MD_NUM];

/****************************************************************************/
/* API Region called by sub-modem class, reuseable API */
/****************************************************************************/
struct ccci_modem *ccci_md_alloc(int private_size);
int ccci_md_register(struct ccci_modem *modem);

static inline struct ccci_modem *ccci_md_get_modem_by_id(int md_id)
{
	if (md_id >= MAX_MD_NUM || md_id < 0)
		return NULL;
	return modem_sys[md_id];
}

static inline struct device *ccci_md_get_dev_by_id(int md_id)
{
	if (md_id >= MAX_MD_NUM || md_id < 0)
		return NULL;
	return &modem_sys[md_id]->plat_dev->dev;
}

static inline int ccci_md_in_ee_dump(int md_id)
{
	if (md_id >= MAX_MD_NUM || md_id < 0)
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	return modem_sys[md_id]->per_md_data.is_in_ee_dump;
}

static inline void *ccci_md_get_hw_info(int md_id)
{
	if (md_id >= MAX_MD_NUM || md_id < 0)
		return NULL;
	return modem_sys[md_id]->hw_info;
}

static inline int ccci_md_recv_skb(unsigned char md_id,
	unsigned char hif_id, struct sk_buff *skb)
{
	int flag = NORMAL_DATA;

	if (hif_id == MD1_NET_HIF)
		flag = CLDMA_NET_DATA;
	return ccci_port_recv_skb(md_id, hif_id, skb, flag);
}

/****************************************************************************/
/* API Region called by port_proxy class */
/****************************************************************************/
struct ccci_modem *ccci_md_get_another(int md_id);
void ccci_md_set_reload_type(struct ccci_modem *md, int type);

int ccci_md_check_ee_done(struct ccci_modem *md, int timeout);
int ccci_md_store_load_type(struct ccci_modem *md, int type);
int ccci_md_get_ex_type(struct ccci_modem *md);

/***************************************************************************/
/* API Region called by ccci modem object */
/***************************************************************************/

#if defined(FEATURE_SYNC_C2K_MEID)
extern unsigned char tc1_read_meid_syncform(unsigned char *meid, int leng);
#endif

#if defined(FEATURE_TC1_CUSTOMER_VAL)
extern int get_md_customer_val(unsigned char *value, unsigned int len);
#endif
extern bool spm_is_md1_sleep(void);

extern unsigned int trace_sample_time;

extern u32 mt_irq_get_pending(unsigned int irq);

#define GF_PORT_LIST_MAX 128
extern int gf_port_list_reg[GF_PORT_LIST_MAX];
extern int gf_port_list_unreg[GF_PORT_LIST_MAX];
extern int ccci_ipc_set_garbage_filter(struct ccci_modem *md, int reg);
#endif	/* __CCCI_MODEM_H__ */

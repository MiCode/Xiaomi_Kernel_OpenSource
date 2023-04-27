/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef __MTK_MMC_SWCQHCI_H__
#define __MTK_MMC_SWCQHCI_H__

#include <linux/bitops.h>
#include <linux/spinlock_types.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>

#define MMC_SWCQ_DEBUG  0
#define SWCQ_TUNING_CMD 1
#define NUM_SLOTS 32

#ifdef CONFIG_MMC_CRYPTO

/* Crypto */
#define PERI_FDI_AES_SI_CTRL 0x448
#define MSDC_AES_EN       0x600
#define MSDC_AES_SWST     0x670
#define MSDC_AES_CFG_GP1  0x674
#define MSDC_AES_KEY_GP1  0x6A0
#define MSDC_AES_TKEY_GP1 0x6C0
#define MSDC_AES_IV0_GP1  0x680
#define MSDC_AES_CTR0_GP1 0x690
#define MSDC_AES_CTR1_GP1 0x694
#define MSDC_AES_CTR2_GP1 0x698
#define MSDC_AES_CTR3_GP1 0x69C
/* Crypto CQE */
#define MSDC_CRCAP        0x100
/* Crypto context fields in CQHCI data command task descriptor */
#define DATA_UNIT_NUM(x)	    (((u64)(x) & 0xFFFFFFFF) << 0)
#define CRYPTO_CONFIG_INDEX(x)	(((u64)(x) & 0xFF) << 32)
#define CRYPTO_ENABLE(x)	    (((u64)(x) & 0x1) << 47)

/* Crypto ATF */
#define MTK_SIP_KERNEL_HW_FDE_MSDC_CTL_AARCH32 (0x82000273 | 0x00000000)
#define MTK_SIP_KERNEL_HW_FDE_MSDC_CTL_AARCH64 (0xC2000273 | 0x40000000)

/* CQE */
#define CQ_TASK_DESC_TASK_PARAMS_SIZE 8
#define CQ_TASK_DESC_CE_PARAMS_SIZE 8


/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/
/* Crypto */
#define PERI_AES_CTRL_MSDC0_EN    (4)          /* RW */
#define MSDC_AES_MODE_1           (0x1F << 0)  /* RW */
#define MSDC_AES_BYPASS           (1 << 2)     /* RW */
#define MSDC_AES_SWITCH_START_ENC (1 << 0)     /* RW */
#define MSDC_AES_SWITCH_START_DEC (1 << 1)     /* RW */
#define MSDC_AES_ON               (0x1 << 0)   /* RW */
#define MSDC_AES_SWITCH_VALID0    (0x1 << 1)   /* RW */
#define MSDC_AES_SWITCH_VALID1    (0x1 << 2)   /* RW */
#define MSDC_AES_CLK_DIV_SEL      (0x7 << 4)   /* RW */

/*--------------------------------------------------------------------------*/
/* enum                                                    */
/*--------------------------------------------------------------------------*/
/* swcq crypto engine mode register */
enum msdc_crypto_alg {
	MSDC_CRYPTO_ALG_BITLOCKER_AES_CBC	= 1,
	MSDC_CRYPTO_ALG_AES_ECB				= 2,
	MSDC_CRYPTO_ALG_ESSIV_AES_CBC		= 3,
	MSDC_CRYPTO_ALG_AES_XTS				= 4,
};

union swcqhci_crypto_capabilities {
	__le32 reg_val;
	struct {
		u8 num_crypto_cap;
		u8 config_count;
		u8 reserved;
		u8 config_array_ptr;
	};
};

enum swcqhci_crypto_key_size {
	SWCQHCI_CRYPTO_KEY_SIZE_128		= 0,
	SWCQHCI_CRYPTO_KEY_SIZE_192		= 1,
	SWCQHCI_CRYPTO_KEY_SIZE_256		= 2,
	SWCQHCI_CRYPTO_KEY_SIZE_512		= 3,
	SWCQHCI_CRYPTO_KEY_SIZE_INVALID	= 4,
};

/* crypto capabilities algorithm */
enum swcqhci_crypto_alg {
	SWCQHCI_CRYPTO_ALG_AES_XTS				= 4,
	SWCQHCI_CRYPTO_ALG_BITLOCKER_AES_CBC	= 3,
	SWCQHCI_CRYPTO_ALG_AES_ECB				= 2,
	SWCQHCI_CRYPTO_ALG_ESSIV_AES_CBC		= 1,
	SWCQHCI_CRYPTO_ALG_INVALID				= 0,
};

/* x-CRYPTOCAP - Crypto Capability X */
union swcqhci_crypto_cap_entry {
	__le32 reg_val;
	struct {
		u8 algorithm_id;
		u8 sdus_mask; /* Supported data unit size mask */
		u8 key_size;
		u8 reserved;
	};
};

/* Please note that enable bit @ bit15 for spec */
#define MMC_CRYPTO_CONFIGURATION_ENABLE (1 << 7)
#define MMC_CRYPTO_KEY_MAX_SIZE 64

/* key info will be fill in here, find slot will use, # of array == # of slot */
union swcqhci_crypto_cfg_entry {
	__le32 reg_val[32];
	struct {
		u8 crypto_key[MMC_CRYPTO_KEY_MAX_SIZE];
		/* 4KB/512 = 8 */
		u8 data_unit_size;
		u8 crypto_cap_idx;
		u8 reserved_1;
		u8 config_enable;
		u8 reserved_multi_host;
		u8 reserved_2;
		u8 vsb[2];
		u8 reserved_3[56];
	};
};
#endif

struct swcq_ongoing_task {
/* If a task is running ,id from 0 ->31. And 99 means host
 * is idle and can perform IO operations.
 */
#define MMC_SWCQ_TASK_IDLE  99
	atomic_t  id;
	atomic_t done;
	atomic_t blksz;
};

struct swcq_host_ops {
	/* Add some ops
	 * maybe need use in future
	 */
	void  (*dump_info)(struct mmc_host *host);
	void  (*err_handle)(struct mmc_host *host);
	void  (*prepare_tuning)(struct mmc_host *host);

};

struct swcq_host {
	struct mmc_host *mmc;
	spinlock_t lock;
	/*
	 *  q_cnt is reqs total cnt
	 *  pre_tsks is the bit map of tasks need to queue in device
	 *  qnd_tsks is the bit map of queued tasks in device
	 *  rdy_tsks is the bit map of ready tasks in device
	 */
	atomic_t q_cnt;
	unsigned int pre_tsks;
	unsigned int qnd_tsks;
	unsigned int rdy_tsks;
	struct swcq_ongoing_task ongoing_task;
	struct task_struct	*cmdq_thread;
	wait_queue_head_t wait_cmdq_empty;
	wait_queue_head_t wait_dat_trans;
	struct mmc_request *mrq[NUM_SLOTS];
	const struct swcq_host_ops *ops;
#ifdef CONFIG_MMC_CRYPTO
	union swcqhci_crypto_capabilities crypto_capabilities;
	union swcqhci_crypto_cap_entry *crypto_cap_array;
	u8 crypto_cfg_register;
	union swcqhci_crypto_cfg_entry *crypto_cfgs;
#endif /* CONFIG_MMC_CRYPTO */
};

int swcq_init(struct swcq_host *swcq_host, struct mmc_host *mmc);

static inline int q_cnt(struct swcq_host *host)
{
	return atomic_read(&host->q_cnt);
}

static inline int swcq_tskid(struct swcq_host *host)
{
	return atomic_read(&host->ongoing_task.id);
}

static inline int swcq_tskdone(struct swcq_host *host)
{
	return atomic_read(&host->ongoing_task.done);
}

static inline int swcq_tskblksz(struct swcq_host *host)
{
	return atomic_read(&host->ongoing_task.blksz);
}

#define swcq_tskid_idle(host) (swcq_tskid(host) == MMC_SWCQ_TASK_IDLE)

#endif

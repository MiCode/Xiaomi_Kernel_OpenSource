/* drivers/soc/qcom/smd_private.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2014, 2017, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_
#define _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/remote_spinlock.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <soc/qcom/smd.h>
#include <soc/qcom/smsm.h>

#define VERSION_QDSP6     4
#define VERSION_APPS_SBL  6
#define VERSION_MODEM_SBL 7
#define VERSION_APPS      8
#define VERSION_MODEM     9
#define VERSION_DSPS      10

#define ID_SMD_CHANNELS SMEM_SMD_BASE_ID
#define ID_SHARED_STATE SMEM_SMSM_SHARED_STATE
#define ID_CH_ALLOC_TBL SMEM_CHANNEL_ALLOC_TBL

#define SMD_SS_CLOSED            0x00000000
#define SMD_SS_OPENING           0x00000001
#define SMD_SS_OPENED            0x00000002
#define SMD_SS_FLUSHING          0x00000003
#define SMD_SS_CLOSING           0x00000004
#define SMD_SS_RESET             0x00000005
#define SMD_SS_RESET_OPENING     0x00000006

#define SMD_HEADER_SIZE          20

/* 'type' field of smd_alloc_elm structure
 * has the following breakup
 * bits 0-7   -> channel type
 * bits 8-11  -> xfer type
 * bits 12-31 -> reserved
 */
struct smd_alloc_elm {
	char name[20];
	uint32_t cid;
	uint32_t type;
	uint32_t ref_count;
};

#define SMD_CHANNEL_TYPE(x) ((x) & 0x000000FF)
#define SMD_XFER_TYPE(x)    (((x) & 0x00000F00) >> 8)

struct smd_half_channel {
	unsigned int state;
	unsigned char fDSR;
	unsigned char fCTS;
	unsigned char fCD;
	unsigned char fRI;
	unsigned char fHEAD;
	unsigned char fTAIL;
	unsigned char fSTATE;
	unsigned char fBLOCKREADINTR;
	unsigned int tail;
	unsigned int head;
};

struct smd_half_channel_word_access {
	unsigned int state;
	unsigned int fDSR;
	unsigned int fCTS;
	unsigned int fCD;
	unsigned int fRI;
	unsigned int fHEAD;
	unsigned int fTAIL;
	unsigned int fSTATE;
	unsigned int fBLOCKREADINTR;
	unsigned int tail;
	unsigned int head;
};

struct smd_half_channel_access {
	void (*set_state)(volatile void __iomem *half_channel,
					unsigned int data);
	unsigned int (*get_state)(volatile void __iomem *half_channel);
	void (*set_fDSR)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fDSR)(volatile void __iomem *half_channel);
	void (*set_fCTS)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fCTS)(volatile void __iomem *half_channel);
	void (*set_fCD)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fCD)(volatile void __iomem *half_channel);
	void (*set_fRI)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fRI)(volatile void __iomem *half_channel);
	void (*set_fHEAD)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fHEAD)(volatile void __iomem *half_channel);
	void (*set_fTAIL)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fTAIL)(volatile void __iomem *half_channel);
	void (*set_fSTATE)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fSTATE)(volatile void __iomem *half_channel);
	void (*set_fBLOCKREADINTR)(volatile void __iomem *half_channel,
					unsigned char data);
	unsigned int (*get_fBLOCKREADINTR)(volatile void __iomem *half_channel);
	void (*set_tail)(volatile void __iomem *half_channel,
					unsigned int data);
	unsigned int (*get_tail)(volatile void __iomem *half_channel);
	void (*set_head)(volatile void __iomem *half_channel,
					unsigned int data);
	unsigned int (*get_head)(volatile void __iomem *half_channel);
};

int is_word_access_ch(unsigned int ch_type);

struct smd_half_channel_access *get_half_ch_funcs(unsigned int ch_type);

struct smd_channel {
	volatile void __iomem *send; /* some variant of smd_half_channel */
	volatile void __iomem *recv; /* some variant of smd_half_channel */
	unsigned char *send_data;
	unsigned char *recv_data;
	unsigned int fifo_size;
	struct list_head ch_list;

	unsigned int current_packet;
	unsigned int n;
	void *priv;
	void (*notify)(void *priv, unsigned int flags);

	int (*read)(smd_channel_t *ch, void *data, int len);
	int (*write)(smd_channel_t *ch, const void *data, int len,
			bool int_ntfy);
	int (*read_avail)(smd_channel_t *ch);
	int (*write_avail)(smd_channel_t *ch);
	int (*read_from_cb)(smd_channel_t *ch, void *data, int len);

	void (*update_state)(smd_channel_t *ch);
	unsigned int last_state;
	void (*notify_other_cpu)(smd_channel_t *ch);
	void * (*read_from_fifo)(void *dest, const void *src, size_t num_bytes);
	void * (*write_to_fifo)(void *dest, const void *src, size_t num_bytes);

	char name[20];
	struct platform_device pdev;
	unsigned int type;

	int pending_pkt_sz;

	char is_pkt_ch;

	/*
	 * private internal functions to access *send and *recv.
	 * never to be exported outside of smd
	 */
	struct smd_half_channel_access *half_ch;
};

extern spinlock_t smem_lock;

struct interrupt_stat {
	uint32_t smd_in_count;
	uint32_t smd_out_count;
	uint32_t smd_interrupt_id;

	uint32_t smsm_in_count;
	uint32_t smsm_out_count;
	uint32_t smsm_interrupt_id;
};
extern struct interrupt_stat interrupt_stats[NUM_SMD_SUBSYSTEMS];

struct interrupt_config_item {
	/* must be initialized */
	irqreturn_t (*irq_handler)(int req, void *data);
	/* outgoing interrupt config (set from platform data) */
	uint32_t out_bit_pos;
	void __iomem *out_base;
	uint32_t out_offset;
	int irq_id;
};

enum {
	MSM_SMD_DEBUG = 1U << 0,
	MSM_SMSM_DEBUG = 1U << 1,
	MSM_SMD_INFO = 1U << 2,
	MSM_SMSM_INFO = 1U << 3,
	MSM_SMD_POWER_INFO = 1U << 4,
	MSM_SMSM_POWER_INFO = 1U << 5,
};

struct interrupt_config {
	struct interrupt_config_item smd;
	struct interrupt_config_item smsm;
};

struct edge_to_pid {
	uint32_t	local_pid;
	uint32_t	remote_pid;
	char		subsys_name[SMD_MAX_CH_NAME_LEN];
	bool		initialized;
};

extern void *smd_log_ctx;
extern int msm_smd_debug_mask;

extern irqreturn_t smd_modem_irq_handler(int irq, void *data);
extern irqreturn_t smsm_modem_irq_handler(int irq, void *data);
extern irqreturn_t smd_dsp_irq_handler(int irq, void *data);
extern irqreturn_t smsm_dsp_irq_handler(int irq, void *data);
extern irqreturn_t smd_dsps_irq_handler(int irq, void *data);
extern irqreturn_t smsm_dsps_irq_handler(int irq, void *data);
extern irqreturn_t smd_wcnss_irq_handler(int irq, void *data);
extern irqreturn_t smsm_wcnss_irq_handler(int irq, void *data);
extern irqreturn_t smd_rpm_irq_handler(int irq, void *data);
extern irqreturn_t smd_modemfw_irq_handler(int irq, void *data);

extern int msm_smd_driver_register(void);
extern void smd_post_init(unsigned int remote_pid);
extern int smsm_post_init(void);

extern struct interrupt_config *smd_get_intr_config(uint32_t edge);
extern int smd_edge_to_remote_pid(uint32_t edge);
extern int smd_edge_to_local_pid(uint32_t edge);
extern void smd_set_edge_subsys_name(uint32_t edge, const char *subsys_name);
extern void smd_reset_all_edge_subsys_name(void);
extern void smd_proc_set_skip_pil(unsigned int pid, bool skip_pil);
extern void smd_set_edge_initialized(uint32_t edge);
extern void smd_cfg_smd_intr(uint32_t proc, uint32_t mask, void *ptr);
extern void smd_cfg_smsm_intr(uint32_t proc, uint32_t mask, void *ptr);
#endif

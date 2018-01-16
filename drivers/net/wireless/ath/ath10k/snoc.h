/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SNOC_H_
#define _SNOC_H_

#include "hw.h"
#include "ce.h"
#include "pci.h"
#include "qmi.h"
#include <linux/kernel.h>
#include <soc/qcom/service-locator.h>
#define ATH10K_SNOC_RX_POST_RETRY_MS 50
#define CE_POLL_PIPE 4
#define ATH10K_SERVICE_LOCATION_CLIENT_NAME			"ATH10K-WLAN"
#define ATH10K_WLAN_SERVICE_NAME					"wlan/fw"

/* struct snoc_state: SNOC target state
 * @pipe_cfg_addr: pipe configuration address
 * @svc_to_pipe_map: pipe services
 */
struct snoc_state {
	u32 pipe_cfg_addr;
	u32 svc_to_pipe_map;
};

/* struct ath10k_snoc_pipe: SNOC pipe configuration
 * @ath10k_ce_pipe: pipe handle
 * @pipe_num: pipe number
 * @hif_ce_state: pointer to ce state
 * @buf_sz: buffer size
 * @pipe_lock: pipe lock
 * @ar_snoc: snoc private structure
 */

struct ath10k_snoc_pipe {
	struct ath10k_ce_pipe *ce_hdl;
	u8 pipe_num;
	struct ath10k *hif_ce_state;
	size_t buf_sz;
	/* protect ce info */
	spinlock_t pipe_lock;
	struct ath10k_snoc *ar_snoc;
};

/* struct ath10k_snoc_supp_chip: supported chip set
 * @dev_id: device id
 * @rev_id: revison id
 */
struct ath10k_snoc_supp_chip {
	u32 dev_id;
	u32 rev_id;
};

/* struct ath10k_snoc_info: SNOC info struct
 * @v_addr: base virtual address
 * @p_addr: base physical address
 * @chip_id: chip id
 * @chip_family: chip family
 * @board_id: board id
 * @soc_id: soc id
 * @fw_version: fw version
 */
struct ath10k_snoc_info {
	void __iomem *v_addr;
	phys_addr_t p_addr;
	u32 chip_id;
	u32 chip_family;
	u32 board_id;
	u32 soc_id;
	u32 fw_version;
};

/* struct ath10k_target_info: SNOC target info
 * @target_version: target version
 * @target_type: target type
 * @target_revision: target revision
 * @soc_version: target soc version
 */
struct ath10k_target_info {
	u32 target_version;
	u32 target_type;
	u32 target_revision;
	u32 soc_version;
};

/* struct ath10k_service_notifier_context: service notification context
 * @handle: notifier handle
 * @instance_id: domain instance id
 * @name: domain name
 */
struct ath10k_service_notifier_context {
	void *handle;
	u32 instance_id;
	char name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
};

/* struct ath10k_snoc_ce_irq: copy engine irq struct
 * @irq_req_stat: irq request status
 * @irq_line: irq line
 */
struct ath10k_snoc_ce_irq {
	atomic_t irq_req_stat;
	u32 irq_line;
};

struct ath10k_wcn3990_vreg_info {
	struct regulator *reg;
	const char *name;
	u32 min_v;
	u32 max_v;
	u32 load_ua;
	unsigned long settle_delay;
	bool required;
};

struct ath10k_wcn3990_clk_info {
	struct clk *handle;
	const char *name;
	u32 freq;
	bool required;
};

static struct ath10k_wcn3990_vreg_info vreg_cfg[] = {
	{NULL, "vdd-0.8-cx-mx", 800000, 800000, 0, 0, false},
	{NULL, "vdd-1.8-xo", 1800000, 1800000, 0, 0, false},
	{NULL, "vdd-1.3-rfa", 1304000, 1304000, 0, 0, false},
	{NULL, "vdd-3.3-ch0", 3312000, 3312000, 0, 0, false},
};

#define ATH10K_WCN3990_VREG_INFO_SIZE		ARRAY_SIZE(vreg_cfg)

static struct ath10k_wcn3990_clk_info clk_cfg[] = {
	{NULL, "cxo_ref_clk_pin", 0, false},
};

#define ATH10K_WCN3990_CLK_INFO_SIZE		ARRAY_SIZE(clk_cfg)

enum ath10k_driver_state {
	ATH10K_DRIVER_STATE_PROBED,
	ATH10K_DRIVER_STATE_STARTED,
};

/* struct ath10k_snoc: SNOC info struct
 * @dev: device structure
 * @ar:ath10k base structure
 * @mem: mem base virtual address
 * @mem_pa: mem base physical address
 * @target_info: snoc target info
 * @mem_len: mempry map length
 * @pipe_info: pipe info struct
 * @ce_irqs: copy engine irq list
 * @ce_lock: protect ce structures
 * @ce_states: maps ce id to ce state
 * @rx_post_retry: rx buffer post processing timer
 * @vaddr_rri_on_ddr: virtual address for RRI
 * @is_driver_probed: flag to indicate driver state
 * @modem_ssr_nb: notifier callback for modem notification
 * @modem_notify_handler: modem notification handler
 * @service_notifier: notifier context for service notification
 * @service_notifier_nb: notifier callback for service notification
 * @total_domains: no of service domains
 * @get_service_nb: notifier callback for service discovery
 * @fw_crashed: fw state flag
 */
struct ath10k_snoc {
	struct bus_opaque opaque_ctx;
	struct platform_device *dev;
	struct ath10k *ar;
	void __iomem *mem;
	dma_addr_t mem_pa;
	struct ath10k_target_info target_info;
	size_t mem_len;
	struct ath10k_snoc_pipe pipe_info[CE_COUNT_MAX];
	struct timer_list rx_post_retry;
	struct ath10k_snoc_ce_irq ce_irqs[CE_COUNT_MAX];
	u32 *vaddr_rri_on_ddr;
	bool is_driver_probed;
	struct notifier_block modem_ssr_nb;
	struct notifier_block pm_notifier;
	void *modem_notify_handler;
	struct ath10k_service_notifier_context *service_notifier;
	struct notifier_block service_notifier_nb;
	int total_domains;
	struct notifier_block get_service_nb;
	atomic_t fw_crashed;
	atomic_t pm_ops_inprogress;
	struct ath10k_snoc_qmi_config qmi_cfg;
	struct ath10k_wcn3990_vreg_info vreg[ATH10K_WCN3990_VREG_INFO_SIZE];
	struct ath10k_wcn3990_clk_info clk[ATH10K_WCN3990_CLK_INFO_SIZE];
	enum ath10k_driver_state drv_state;
};

struct ath10k_event_pd_down_data {
	bool crashed;
	bool fw_rejuvenate;
};

static inline struct ath10k_snoc *ath10k_snoc_priv(struct ath10k *ar)
{
	return (struct ath10k_snoc *)ar->drv_priv;
}

void ath10k_snoc_write32(struct ath10k *ar, u32 offset, u32 value);
void ath10k_snoc_soc_write32(struct ath10k *ar, u32 addr, u32 val);
void ath10k_snoc_reg_write32(struct ath10k *ar, u32 addr, u32 val);
u32 ath10k_snoc_read32(struct ath10k *ar, u32 offset);
u32 ath10k_snoc_soc_read32(struct ath10k *ar, u32 addr);
u32 ath10k_snoc_reg_read32(struct ath10k *ar, u32 addr);

#endif /* _SNOC_H_ */

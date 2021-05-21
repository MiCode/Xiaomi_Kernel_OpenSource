/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

/* This file contain content copied from Synopsis driver,
 * provided under the license below
 */
/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * =========================================================================
 */

#ifndef _VETH_IPA_H_
#define _VETH_IPA_H_

#include <linux/ipa.h>
#include <linux/ipa_uc_offload.h>
#include <linux/ipc_logging.h>
#include <linux/in.h>
#include <linux/ip.h>



/* Feature enable disable flags*/
#define VETH_ENABLE_VLAN_TAG
#define VETH_MIN_VLAN_ID 1
#define VETH_MAX_VLAN_ID 4094

#define DRIVER_NAME             "veth_ipa"
#define IPA_VETH_IPC_LOGGING(buf, fmt, args...) \
	do { \
		if (buf) \
			ipc_log_string((buf), fmt, __func__, __LINE__, \
				## args); \
	} while (0)

static void *ipa_veth_logbuf;

#define VETH_IPA_DEBUG(fmt, args...) \
	do { \
		pr_debug(DRIVER_NAME " %s:%d "\
			fmt, __func__, __LINE__, ## args);\
		if (ipa_veth_logbuf) { \
			IPA_VETH_IPC_LOGGING(ipa_veth_logbuf, \
				DRIVER_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define VETH_IPA_DEBUG_XMIT(fmt, args...) \
	pr_debug(DRIVER_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define VETH_IPA_INFO(fmt, args...) \
	do { \
		pr_info(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args);\
		if (ipa_veth_logbuf) { \
			IPA_VETH_IPC_LOGGING(ipa_veth_logbuf, \
				DRIVER_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define VETH_IPA_ERROR(fmt, args...) \
	do { \
		pr_err(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args);\
		if (ipa_veth_logbuf) { \
			IPA_VETH_IPC_LOGGING(ipa_veth_logbuf, \
				DRIVER_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define NULL_CHECK(ptr) \
	do { \
		if (!(ptr)) { \
			VETH_IPA_ERROR("null pointer #ptr\n"); \
			ret = -EINVAL; \
		} \
	} \
	while (0)

#define VETH_IPA_LOG_ENTRY() VETH_IPA_DEBUG("begin\n")
#define VETH_IPA_LOG_EXIT() VETH_IPA_DEBUG("end\n")
#define VETH_RX_DESC_CNT   256    /*la uses 128*/
#define VETH_TX_DESC_CNT   256    /*la uses 128*/
/*IPA can support 2KB max pkt length*/

#define VETH_ETH_FRAME_LEN_IPA	(1<<12)
#define VETH_IPA_LOCK() mutex_lock(&pdata->prv_ipa.ipa_lock)
#define VETH_IPA_UNLOCK() mutex_unlock(&pdata->prv_ipa.ipa_lock)

enum IPA_OFFLOAD_EVENT {
	EV_INVALID = 0,
	EV_DEV_OPEN,
	EV_DEV_CLOSE,
	EV_IPA_READY,
	EV_IPA_UC_READY,
	EV_IPA_EMAC_INIT,
	EV_IPA_EMAC_SETUP,
	EV_PHY_LINK_UP,
	EV_EMAC_DEINIT,
	EV_EMAC_UP,
	EV_START_OFFLOAD,
};

struct s_RX_NORMAL_DESC {
	unsigned int RDES0;
	unsigned int RDES1;
	unsigned int RDES2;
	unsigned int RDES3;
};


struct s_TX_NORMAL_DESC {
	unsigned int TDES0;
	unsigned int TDES1;
	unsigned int TDES2;
	unsigned int TDES3;
};


struct veth_emac_exp {
	uint32_t tx_desc_exp_id;
	uint32_t rx_desc_exp_id;
	uint32_t tx_buff_exp_id;
	uint32_t rx_buff_exp_id;
	uint32_t rx_buf_pool_exp_id;
	uint32_t tx_buf_pool_exp_id;
	int      event_id;
};

struct veth_emac_export_mem {
	/* IPAs - this is not a virtual address*/
	void        *tx_desc_mem_va;
	phys_addr_t  tx_desc_mem_paddr;
	dma_addr_t   tx_desc_mem_iova;
	phys_addr_t  tx_desc_ring_base[VETH_TX_DESC_CNT];

	void        *tx_buf_mem_va;
	dma_addr_t   tx_buf_mem_paddr;
	dma_addr_t   tx_buf_mem_iova;

	uint32_t    *tx_buff_pool_base_va;
	dma_addr_t   tx_buff_pool_base_iova;
	dma_addr_t   tx_buff_pool_base_pa;

	void        *rx_desc_mem_va;
	phys_addr_t  rx_desc_mem_paddr;
	dma_addr_t   rx_desc_mem_iova;
	phys_addr_t  rx_desc_ring_base[VETH_RX_DESC_CNT];


	void        *rx_buf_mem_va;
	dma_addr_t   rx_buf_mem_paddr;
	dma_addr_t   rx_buf_mem_iova;

	uint32_t    *rx_buff_pool_base_va;
	dma_addr_t   rx_buff_pool_base_iova;
	dma_addr_t   rx_buff_pool_base_pa;

	struct veth_emac_exp exp_id;
	int    vc_id;
	bool   link_down;
	bool   init_complete;
};




/**
 * enum veth_ipa_state - specify the current driver internal
 *  state which is guarded by a state machine.
 *
 * The driver internal state changes due to its external API usage.
 * The driver saves its internal state to guard from caller illegal
 * call sequence.
 * states:
 * UNLOADED is the first state which is the default one and is also the state
 *  after the driver gets unloaded(cleanup).
 * INITIALIZED is the driver state once it finished registering
 *  the network device and all internal data struct were initialized
 * CONNECTED is the driver state once the EMAC pipes were
 * connected to IPA UP is the driver state after the interface
 * mode was set to UP but the
 *  pipes are not connected yet - this state is meta-stable state.
 * CONNECTED_AND_UP is the driver state when the pipe were connected and
 *  the interface got UP request from the network stack. this is the driver
 *   idle operation state which allows it to transmit/receive data.
 * INVALID is a state which is not allowed.
 */
enum veth_ipa_state {
	VETH_IPA_UNLOADED = 0,
	VETH_IPA_INITIALIZED,
	VETH_IPA_CONNECTED,
	VETH_IPA_UP,
	VETH_IPA_CONNECTED_AND_UP,
	VETH_IPA_DOWN,
	VETH_IPA_INVALID,
};


/**
 * enum VETH_ipa_operation - enumerations used to descibe the API operation
 *
 * Those enums are used as input for the driver state machine.
 */
enum veth_ipa_operation {
	VETH_IPA_INITIALIZE,
	VETH_IPA_CONNECT,
	VETH_IPA_OPEN,
	VETH_IPA_STOP,
	VETH_IPA_DISCONNECT,
	VETH_IPA_CLEANUP,
};



/**
 * enum veth_ipa_emac_commands - enumerations which are used in
 *
 * Those enums are used as input for the driver state machine.
 */
enum veth_ipa_emac_commands {
	VETH_IPA_OPEN_EV,
	VETH_IPA_SETUP_OFFLOAD,
	VETH_IPA_START_OFFLOAD,
	VETH_IPA_STOP_OFFLOAD,
	VETH_IPA_ACK,
	VETH_IPA_SETUP_COMPLETE,
};


#define VETH_IPA_STATE_DEBUG(veth_ipa_ctx) \
	VETH_IPA_DEBUG("Driver state - %s\n",\
	veth_ipa_state_string((veth_ipa_ctx)->state))



/*
 * @priv: private data given upon ipa_connect
 * @evt: event enum, should be IPA_WRITE_DONE
 * @data: for tx path the data field is the sent socket buffer.
 */
typedef void (*veth_ipa_callback)(void *priv,
	enum ipa_dp_evt_type evt,
	unsigned long data);


struct veth_ipa_client_data {
	phys_addr_t uc_db_rx_addr;
	phys_addr_t uc_db_tx_addr;
	u32 ipa_client_hndl;

	/*State of IPA readiness*/
	bool ipa_ready;

	/*State of IPA UC readiness*/
	bool ipa_uc_ready;

	/*State of IPA Offload intf registration with IPA driver*/
	bool ipa_offload_init;

	/*State of IPA pipes connection*/
	bool ipa_offload_conn;

	/*EMAC init*/
	bool emac_init;

	/*Dev state*/
	struct work_struct ntn_ipa_rdy_work;
	struct work_struct ntn_ipa_uc_rdy_work;
	struct work_struct ntn_emac_init_rdy_work;
	struct work_struct ntn_emac_open_rdy_work;
	struct work_struct ntn_emac_setup_rdy_work;
	struct work_struct ntn_emac_link_up_rdy_work;
	struct work_struct ntn_emac_start_offload_rdy_work;
	struct work_struct ntn_emac_de_init_rdy_work;

	struct mutex ipa_lock;
	bool vlan_enable;
	unsigned short vlan_id;
};


/*
 * struct veth_ipa_params - parameters for ecm_ipa
 * initialization API
 *
 * @device_ready_notify: callback supplied by USB core driver.
 * This callback shall be called by the Netdev once the device
 * is ready to receive data from tethered PC.
 *
 * @veth_ipa_rx_dp_notify: ecm_ipa will set this callback (out
 * parameter). this callback shall be supplied for ipa_connect
 * upon pipe connection (USB->IPA), once IPA driver receive data
 * packets from USB pipe destined for Apps this callback will be
 * called.
 *
 * @veth_ipa_tx_dp_notify: ecm_ipa will set this callback (out
 * parameter). this callback shall be supplied for ipa_connect
 * upon pipe connection (IPA->USB), once IPA driver send packets
 * destined for USB, IPA BAM will notify for Tx-complete. @priv:
 * ecm_ipa will set this pointer (out parameter). This pointer
 * will hold the network device for later interaction with
 * ecm_ipa APIs
 *
 * @host_ethaddr: host Ethernet address in network order
 *
 * @device_ethaddr: device Ethernet address in network order
 *
 * @skip_ep_cfg: boolean field that determines if
 *  Apps-processor should or should not configure this
 *  end-point.
 */

/**
 * struct veth_ipa_dev - main driver context parameters
 * @net: network interface struct implemented by this driver
 * @directory: debugfs directory for various debuging switches
 * @eth_ipv4_hdr_hdl: saved handle for ipv4 header-insertion table
 * @eth_ipv6_hdr_hdl: saved handle for ipv6 header-insertion table
 * @emac_to_ipa_hdl: save handle for IPA pipe operations
 * @ipa_to_emac_hdl: save handle for IPA pipe operations
 * @outstanding_pkts: number of packets sent to IPA without TX complete ACKed
 * @outstanding_high: number of outstanding packets allowed
 * @outstanding_low: number of outstanding packets which shall cause
 *  to netdev queue start (after stopped due to outstanding_high reached)
 * @state: current state of VETH_ipa driver
 * @device_ready_notify: callback supplied by USB core driver
 * This callback shall be called by the Netdev once the Netdev internal
 * state is changed to RNDIS_IPA_CONNECTED_AND_UP
 * @ipa_to_emac_client: consumer client
 * @emac_to_ipa_client: producer client
 * @ipa_rm_resource_name_prod: IPA resource manager producer resource
 * @ipa_rm_resource_name_cons: IPA resource manager consumer resource
 * @pm_hdl: handle for IPA PM
 * @is_vlan_mode: does the driver need to work in VLAN mode?
 */
struct veth_ipa_dev {
	/*should be part of private Struct*/
	struct net_device      *net;
	struct platform_device *pdev;
	//struct net_device_stats stats;
	atomic_t                outstanding_pkts;
	struct dentry     *directory;
	u8 outstanding_high;
	u8 outstanding_low;
	enum veth_ipa_state state;
	void (*device_ready_notify)(void);

	#ifdef VETH_PM_ENB
	u32 pm_hdl;
	#endif
	bool is_vlan_mode;

	/* Status of EMAC Device*/
	bool veth_emac_dev_ready;

	int speed;

	struct veth_ipa_client_data prv_ipa;
	struct veth_emac_export_mem veth_emac_mem;
	veth_ipa_callback veth_ipa_rx_dp_notify;
	veth_ipa_callback veth_ipa_tx_dp_notify;
	u8 host_ethaddr[ETH_ALEN];   /*not needed for veth driver ?*/
	unsigned char device_ethaddr[ETH_ALEN];
	void *private;
	bool skip_ep_cfg;
	struct ipa_ntn_setup_info *tx_setup_info;
	struct ipa_ntn_setup_info *rx_setup_info;
};

/* SMMU related */
struct emac_emb_smmu_cb_ctx {
	bool valid;
	struct platform_device *pdev_master;
	struct platform_device *smmu_pdev;
	struct dma_iommu_mapping *mapping;
	struct iommu_domain *iommu_domain;
	u32 va_start;
	u32 va_size;
	u32 va_end;
	int ret;
};


/* Maintain Order same on FE*/
struct emac_ipa_iovas {
	/*iova addresses*/
	void   *tx_desc_mem_iova;
	void   *tx_buf_mem_iova;
	void   *tx_buf_pool_base_iova;
	void   *rx_desc_mem_iova;
	void   *rx_buf_mem_iova;
	void   *rx_buf_pool_base_iova;
};

struct emac_hab_mm_message {
	int   event_id;
	union msg_type {
		struct emac_ipa_iovas iova;
	} msg_type;
};


#define GET_MEM_PDEV_DEV (emac_emb_smmu_ctx.valid ? \
			&emac_emb_smmu_ctx.smmu_pdev->dev : &params->pdev->dev)

#if IS_ENABLED(CONFIG_VETH_IPA)

int veth_ipa_connect(u32 emac_to_ipa_hdl, u32 ipa_to_emac_hdl, void *priv);

int veth_ipa_disconnect(void *priv);

void veth_ipa_cleanup(struct veth_ipa_dev *veth_ipa_ctx);

#else /* CONFIG_VETH_IPA*/

static inline int veth_ipa_connect(u32 emac_to_ipa_hdl, u32 ipa_to_emac_hdl,
	void *priv)
{
	return 0;
}

static inline int veth_ipa_disconnect(void *priv)
{
	return 0;
}

static inline void veth_ipa_cleanup(void *priv)
{

}



#endif /* CONFIG_VETH_IPA*/

#endif /* _VETH_IPA_H_ */


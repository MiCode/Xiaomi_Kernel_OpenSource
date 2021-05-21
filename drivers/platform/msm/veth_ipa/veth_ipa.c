// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/of_device.h>
#include <linux/ipa_uc_offload.h>
#include <linux/if_ether.h>
#include <linux/msm_ipa.h>
#include <linux/habmm.h>
#include <linux/habmmid.h>


#include "veth_ipa.h"
#include "./veth_emac_mgt.h"
//#include "../ipa/ipa_common_i.h"
//#include "../ipa/ipa_v3/ipa_pm.h"
#include <linux/ipa_fmwk.h>

#ifdef VETH_PM_ENB
//#include "../ipa/ipa_v3/ipa_pm.h"
#include <linux/ipa.h>
#endif

#define VETH_IFACE_NAME         "VETH"
#define VETH_IFACE_NUM0          0
#define VETH_IPA_IPV4_HDR_NAME  "veth_eth_ipv4"
#define VETH_IPA_IPV6_HDR_NAME  "veth_eth_ipv6"
#define INACTIVITY_MSEC_DELAY    100
#define DEFAULT_OUTSTANDING_HIGH 224
#define DEFAULT_OUTSTANDING_LOW  192
#define DEBUGFS_TEMP_BUF_SIZE    4
#define TX_TIMEOUT              (5 * HZ)
#define IPA_VETH_IPC_LOG_PAGES   50

#define PAGE_SIZE_1 4096

MODULE_LICENSE("GPL v2");

static int veth_ipa_open(struct net_device *net);
static void veth_ipa_packet_receive_notify
	(void *priv, enum ipa_dp_evt_type evt, unsigned long data);

static void veth_ipa_tx_timeout(struct net_device *net);
static int veth_ipa_stop(struct net_device *net);
static struct net_device_stats *veth_ipa_get_stats(struct net_device *net);

#ifdef VETH_PM_ENB
static int resource_request(struct veth_ipa_dev *veth_ipa_ctx);
static void resource_release(struct veth_ipa_dev *veth_ipa_ctx);
#endif
static netdev_tx_t veth_ipa_start_xmit
	(struct sk_buff *skb, struct net_device *net);
static int veth_ipa_debugfs_atomic_open(struct inode *inode, struct file *file);
static ssize_t veth_ipa_debugfs_atomic_read
	(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static void veth_ipa_debugfs_init(struct veth_ipa_dev *veth_ipa_ctx);
static void veth_ipa_debugfs_destroy(struct veth_ipa_dev *veth_ipa_ctx);
static int veth_ipa_set_device_ethernet_addr
	(u8 *dev_ethaddr, u8 device_ethaddr[]);
//static enum veth_ipa_state veth_ipa_next_state
//	(enum veth_ipa_state current_state, enum veth_ipa_operation operation);
static const char *veth_ipa_state_string(enum veth_ipa_state state);
static int veth_ipa_init_module(void);
static void veth_ipa_cleanup_module(void);

static int  veth_ipa_ready(struct veth_ipa_dev *pdata);
static void veth_ipa_ready_cb(void *user_data);

static int  veth_ipa_uc_ready(struct veth_ipa_dev *pdata);
static void veth_ipa_uc_ready_cb(void *user_data);

static int  veth_enable_ipa_offload(struct veth_ipa_dev *pdata);
static int  veth_disable_ipa_offload(struct veth_ipa_dev *pdata);
static int veth_ipa_emac_evt_mgmt(void *arg);
static int veth_ipa_send_msg(struct veth_ipa_dev *pdata,
	enum ipa_peripheral_event event);

static struct veth_ipa_dev *veth_pdata_p;
static struct emac_emb_smmu_cb_ctx emac_emb_smmu_ctx = { 0 };



static void veth_ipa_offload_event_handler(struct veth_ipa_dev *pdata,
	enum IPA_OFFLOAD_EVENT ev);

static int veth_map_rx_tx_setup_info_params(
	struct ipa_ntn_setup_info *rx_setup_info,
	struct ipa_ntn_setup_info *tx_setup_info,
	struct veth_emac_export_mem *gvm_ipa,
	struct veth_ipa_dev *pdata);

static const struct net_device_ops veth_ipa_netdev_ops = {
	.ndo_open		= veth_ipa_open,
	.ndo_stop		= veth_ipa_stop,
	.ndo_start_xmit         = veth_ipa_start_xmit,
	.ndo_set_mac_address    = eth_mac_addr,
	.ndo_tx_timeout         = veth_ipa_tx_timeout,
	.ndo_get_stats          = veth_ipa_get_stats,
};

static const struct file_operations veth_ipa_debugfs_atomic_ops = {
	.open = veth_ipa_debugfs_atomic_open,
	.read = veth_ipa_debugfs_atomic_read,
};

static const char * const IPA_OFFLOAD_EVENT_string[] = {
	"EV_INVALID",
	"EV_DEV_OPEN",
	"EV_DEV_CLOSE",
	"EV_IPA_READY",
	"EV_IPA_UC_READY",
	"EV_IPA_EMAC_INIT",
	"EV_IPA_EMAC_SETUP",
	"EV_PHY_LINK_UP",
	"EV_EMAC_DEINIT"
};

static bool veth_ipa_init_flag;


/**
 * veth_ipa_offload_init() - Called from driver to initialize
 * IPA offload data path. This function will add partial headers
 * and register the interface with IPA.
 *
 * IN: @pdata: NTN private structure handle that will be passed by IPA.
 * OUT: 0 on success and -1 on failure
 */
static int veth_ipa_offload_init(struct veth_ipa_dev *pdata)
{
	struct ipa_uc_offload_intf_params in;
	struct ipa_uc_offload_out_params out;
	/*Null pointer deref*/
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;
	struct ethhdr eth_l2_hdr_v4;
	struct ethhdr eth_l2_hdr_v6;
	bool ipa_vlan_mode;
	int ret;

#ifdef VETH_ENABLE_VLAN_TAG
	struct vlan_ethhdr eth_vlan_hdr_v4;
	struct vlan_ethhdr eth_vlan_hdr_v6;

	pdata->prv_ipa.vlan_enable = true;
	pdata->prv_ipa.vlan_id = 0;
#endif
	emac_emb_smmu_ctx.valid = true;

	VETH_IPA_DEBUG("veth_ipa_offload_init");

	ret = ipa_is_vlan_mode(IPA_VLAN_IF_EMAC, &ipa_vlan_mode);
	if (ret) {
		VETH_IPA_ERROR("Could not read ipa_vlan_mode\n");
		/* In case of failure, fallback to non vlan mode */
		ipa_vlan_mode = false;
	}

	/* hard code vlan mode to 1*/
	ipa_vlan_mode = true;

	VETH_IPA_DEBUG("IPA VLAN mode %d\n", ipa_vlan_mode);
	VETH_IPA_DEBUG("IPA VLAN mode %d vlan id %d mac %pM\n",
				   ipa_vlan_mode, pdata->prv_ipa.vlan_id,
				   pdata->device_ethaddr);
	pdata->prv_ipa.vlan_id = 1;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	/* Building ETH Header */
	if (!pdata->prv_ipa.vlan_id || !ipa_vlan_mode) {
		memset(&eth_l2_hdr_v4, 0, sizeof(eth_l2_hdr_v4));
		memset(&eth_l2_hdr_v6, 0, sizeof(eth_l2_hdr_v6));
		memcpy(&eth_l2_hdr_v4.h_source,
			pdata->device_ethaddr, ETH_ALEN);
		eth_l2_hdr_v4.h_proto = htons(ETH_P_IP);
		memcpy(&eth_l2_hdr_v6.h_source,
			pdata->device_ethaddr, ETH_ALEN);
		eth_l2_hdr_v6.h_proto = htons(ETH_P_IPV6);
		in.hdr_info[0].hdr = (u8 *)&eth_l2_hdr_v4;
		in.hdr_info[0].hdr_len = ETH_HLEN;
		in.hdr_info[1].hdr = (u8 *)&eth_l2_hdr_v6;
		in.hdr_info[1].hdr_len = ETH_HLEN;
	}
#ifdef VETH_ENABLE_VLAN_TAG
	else if ((pdata->prv_ipa.vlan_id > VETH_MIN_VLAN_ID &&
	     pdata->prv_ipa.vlan_id <= VETH_MAX_VLAN_ID)
		|| ipa_vlan_mode) {
		memset(&eth_vlan_hdr_v4, 0, sizeof(eth_vlan_hdr_v4));
		memset(&eth_vlan_hdr_v6, 0, sizeof(eth_vlan_hdr_v6));
		memcpy(&eth_vlan_hdr_v4.h_source,
			pdata->net->dev_addr, ETH_ALEN);
		eth_vlan_hdr_v4.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v4.h_vlan_encapsulated_proto = htons(ETH_P_IP);
		in.hdr_info[0].hdr = (u8 *)&eth_vlan_hdr_v4;
		in.hdr_info[0].hdr_len = VLAN_ETH_HLEN;
		memcpy(&eth_vlan_hdr_v6.h_source,
			pdata->net->dev_addr,
			ETH_ALEN);
		eth_vlan_hdr_v6.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v6.h_vlan_encapsulated_proto = htons(ETH_P_IPV6);
		in.hdr_info[1].hdr = (u8 *)&eth_vlan_hdr_v6;
		in.hdr_info[1].hdr_len = VLAN_ETH_HLEN;
	}
#endif

	/* Building IN params */
	in.netdev_name = pdata->net->name;
	in.priv = pdata;
	in.notify = veth_ipa_packet_receive_notify;
	in.proto = IPA_UC_NTN;
	in.hdr_info[0].dst_mac_addr_offset = 0;
	//in.hdr_info[0].hdr_type = IPA_HDR_L2_ETHERNET_II;
	in.hdr_info[0].hdr_type = IPA_HDR_L2_802_1Q;
	in.hdr_info[1].dst_mac_addr_offset = 0;
	//in.hdr_info[1].hdr_type = IPA_HDR_L2_ETHERNET_II;
	in.hdr_info[1].hdr_type =  IPA_HDR_L2_802_1Q;

	ret = ipa_uc_offload_reg_intf(&in, &out);
	if (ret) {
		VETH_IPA_ERROR("Could not register offload interface ret %d\n",
						ret);
		return -EAGAIN;
	}

	veth_ipa_send_msg(pdata, IPA_PERIPHERAL_CONNECT);
	if (ret < 0) {
		VETH_IPA_ERROR("%s : eth_ipa_send_msg connect failed\n",
					    __func__);
		return ret;
	}
	VETH_IPA_DEBUG("IPA offload connect event sent\n");
	VETH_IPA_DEBUG("Received IPA Offload Client Handle %d", out.clnt_hndl);
	ntn_ipa->ipa_client_hndl = out.clnt_hndl;
	VETH_IPA_DEBUG("Received IPA Offload Client Handle %d",
		ntn_ipa->ipa_client_hndl);
	return 0;
}

int veth_ipa_offload_cleanup(struct veth_ipa_dev *pdata)
{
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;
	int ret = 0;

	if (!pdata) {
		VETH_IPA_ERROR("Could not register offload interface ret %d\n",
					   ret);
		return -EINVAL;
	}

	ret = ipa_uc_offload_cleanup(ntn_ipa->ipa_client_hndl);

	if (ret) {
		VETH_IPA_ERROR("Could not disconnect the uC pipes %d\n",
					   ret);
		return ret;
	}

	VETH_IPA_INFO("uC pipes cleaned up successfully ");

	return 0;
}

int veth_ipa_offload_disconnect(struct veth_ipa_dev *pdata)
{
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;
	int ret = 0;

	VETH_IPA_INFO("Enter : %s\n", __func__);

	if (!pdata) {
		VETH_IPA_ERROR("Offload iface not registered %d\n",
						ret);
		return -EINVAL;
	}

	ret = ipa_uc_offload_disconn_pipes(ntn_ipa->ipa_client_hndl);

	if (ret) {
		VETH_IPA_ERROR("Can't disconnect the uC pipes %d\n",
						ret);
		return ret;
	}

	VETH_IPA_INFO("uC pipes disconnected successfully ");

	return 0;
}


/**
 * veth_set_ul_dl_smmu_ipa_params() - This will set the UL
 * params in ipa_ntn_setup_info structure to be used in the IPA
 * connect.
 * IN: @pdata: NTN private structure handle that will be passed
 *             by IPA.
 * IN: @veth_emac_mem: pointer to export memory struct.
 * IN: @ul pointer to ipa_ntn_setup_info uplink param.
 * IN: @dl pointer to ipa_ntn_setup_info downlink param.
 * OUT: 0 on success and -1 on failure
 *
 * Return: negative errno, or zero on success
 */
int veth_set_ul_dl_smmu_ipa_params(struct veth_ipa_dev *pdata,
	struct veth_emac_export_mem *veth_emac_mem,
	struct ipa_ntn_setup_info *ul,
	struct ipa_ntn_setup_info *dl
	)
{
	int ret = 0;

	if (!pdata) {
		VETH_IPA_ERROR("Null Param %s\n", __func__);
		return -EINVAL;
	}

	if (!ul || !dl) {
		VETH_IPA_ERROR("Null UL DL params %s\n", __func__);
		return -EINVAL;
	}

	/*Configure SGT for UL ring base*/
	ul->ring_base_sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!ul->ring_base_sgt)
		return -ENOMEM;

	ret = dma_get_sgtable(&pdata->pdev->dev,
		ul->ring_base_sgt,
		veth_emac_mem->rx_desc_mem_va,
		veth_emac_mem->rx_desc_mem_paddr,
		(sizeof(struct s_RX_NORMAL_DESC) *
			VETH_RX_DESC_CNT));
	if (ret) {
		VETH_IPA_ERROR("Failed to get IPA UL ring sgtable.\n");
		kfree(ul->ring_base_sgt);
		ul->ring_base_sgt = NULL;
		return -EAGAIN;
	}

	/*get pa*/
	ul->ring_base_pa = sg_phys(ul->ring_base_sgt->sgl);

	VETH_IPA_INFO(
		"%s:\n ul->ring_base_sgt = 0x%p , ul->ring_base_pa =0x%lx\n",
		__func__,
		ul->ring_base_sgt,
		ul->ring_base_pa);

	/*configure SGT for UL buff pool base*/
	ul->buff_pool_base_sgt = kzalloc(
		sizeof(struct sg_table), GFP_KERNEL);

	if (!ul->buff_pool_base_sgt) {
		kfree(ul->ring_base_sgt);
		return -ENOMEM;
	}

	ret = dma_get_sgtable(&pdata->pdev->dev,
		ul->buff_pool_base_sgt,
		veth_emac_mem->rx_buff_pool_base_va,
		veth_emac_mem->rx_buff_pool_base_pa,
		(sizeof(uint32_t) * VETH_RX_DESC_CNT * 4)
		);
	/*using ipa dev node for buff pool*/
	/*overallocating to satisfy hab page alignment*/
	if (ret) {
		VETH_IPA_ERROR("Failed to get IPA UL buff pool sgtable. Error Code:%d\n", ret);
		kfree(ul->ring_base_sgt);
		kfree(ul->buff_pool_base_sgt);
		ul->buff_pool_base_sgt = NULL;
		return -EAGAIN;
	}

	ul->buff_pool_base_pa = sg_phys(ul->buff_pool_base_sgt->sgl);
	veth_emac_mem->rx_buff_pool_base_pa = ul->buff_pool_base_pa;

	VETH_IPA_INFO(
		"%s:\n ul->buff_pool_base_sgt = 0x%p,ul->buff_pool_base_pa =0x%lx\n",
		__func__,
		ul->buff_pool_base_sgt,
		ul->buff_pool_base_pa);

	/*Configure SGT for DL ring base*/
	dl->ring_base_sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!dl->ring_base_sgt)
		return -ENOMEM;

	ret = dma_get_sgtable(&pdata->pdev->dev,
		dl->ring_base_sgt,
		veth_emac_mem->tx_desc_mem_va,
		veth_emac_mem->tx_desc_mem_paddr,
		(sizeof(struct s_TX_NORMAL_DESC) *
			VETH_TX_DESC_CNT));
	if (ret) {
		VETH_IPA_ERROR("Failed to get IPA DL ring sgtable.\n");
		kfree(ul->ring_base_sgt);
		kfree(ul->buff_pool_base_sgt);
		kfree(dl->ring_base_sgt);
		dl->ring_base_sgt = NULL;
		return -EAGAIN;
	}

	dl->ring_base_pa = sg_phys(dl->ring_base_sgt->sgl);
	VETH_IPA_INFO(
		"%s:\n dl->ring_base_sgt = 0x%p , dl->ring_base_pa =0x%lx\n",
		__func__,
		dl->ring_base_sgt,
		dl->ring_base_pa);

	/*configure SGT for DL buff pool base*/
	dl->buff_pool_base_sgt = kzalloc(
		sizeof(struct sg_table), GFP_KERNEL);

	if (!dl->buff_pool_base_sgt)
		return -ENOMEM;
	ret = dma_get_sgtable(&pdata->pdev->dev,
		dl->buff_pool_base_sgt,
		veth_emac_mem->tx_buff_pool_base_va,
		veth_emac_mem->tx_buff_pool_base_pa,
		(sizeof(uint32_t) * VETH_TX_DESC_CNT * 4)
		);
	if (ret) {
		VETH_IPA_ERROR("Failed to get IPA DL buff pool sgtable.\n");
		kfree(ul->ring_base_sgt);
		kfree(ul->buff_pool_base_sgt);
		kfree(dl->ring_base_sgt);
		kfree(dl->buff_pool_base_sgt);
		dl->buff_pool_base_sgt = NULL;
		ret = -EAGAIN;
	}

	if (dl->buff_pool_base_sgt != NULL) {
		dl->buff_pool_base_pa = sg_phys(dl->buff_pool_base_sgt->sgl);
		veth_emac_mem->tx_buff_pool_base_pa = dl->buff_pool_base_pa;

		VETH_IPA_INFO(
		"%s:dl->buff_pool_base_sgt = 0x%p , dl->buff_pool_base_pa =0x%lx",
		__func__,
		dl->buff_pool_base_sgt,
		dl->buff_pool_base_pa);
	}
	return ret;
}

static int veth_map_rx_tx_setup_info_params(
	struct ipa_ntn_setup_info *rx_setup_info,
	struct ipa_ntn_setup_info *tx_setup_info,
	struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata
	)
{
	int i = 0;
	int ret = 0;

	VETH_IPA_DEBUG("%s - begin\n", __func__);

	/* Configure RX setup info*/
	if (emac_emb_smmu_ctx.valid)
		rx_setup_info->smmu_enabled = true;
	else
		rx_setup_info->smmu_enabled = false;

	/* RX Descriptor Base Physical Address*/
	if (!rx_setup_info->smmu_enabled)
		rx_setup_info->ring_base_pa = veth_emac_mem->rx_desc_mem_paddr;

	/* RX Descriptor Base Virtual Address*/
	if (rx_setup_info->smmu_enabled)
		rx_setup_info->ring_base_iova = veth_emac_mem->rx_desc_mem_iova;

	/* RX Descriptor Count*/
	rx_setup_info->ntn_ring_size = VETH_RX_DESC_CNT;

	/* RX Buf pool base*/
	if (!rx_setup_info->smmu_enabled) {
		rx_setup_info->buff_pool_base_pa =
			veth_emac_mem->rx_buff_pool_base_pa;
	}

/*this may cause issues after smmu?*/
	rx_setup_info->buff_pool_base_iova =
		veth_emac_mem->rx_buff_pool_base_iova;

	/*Map TX Buff Pool*/
	if (emac_emb_smmu_ctx.valid) {
		/*store rx buf mem iova into buff pool addresses*/
		veth_emac_mem->rx_buff_pool_base_va[0] =
			veth_emac_mem->rx_buf_mem_iova;
	} else {
		/*store rx buf mem p addr into buff pool addresse*/
		veth_emac_mem->rx_buff_pool_base_va[0] =
			veth_emac_mem->rx_buf_mem_paddr;
	}
	for (i = 0; i < VETH_RX_DESC_CNT; i++) {
		veth_emac_mem->rx_buff_pool_base_va[i] =
			veth_emac_mem->rx_buff_pool_base_va[0] +
			i*VETH_ETH_FRAME_LEN_IPA;
		VETH_IPA_DEBUG(
			"%s: veth_emac_mem->rx_buff_pool_base[%d] 0x%x\n",
			__func__, i, veth_emac_mem->rx_buff_pool_base_va[i]);
	}

	/*RX buffer Count*/
	rx_setup_info->num_buffers = VETH_RX_DESC_CNT - 1;

	/* RX Frame length */
	rx_setup_info->data_buff_size = VETH_ETH_FRAME_LEN_IPA;

	/* Configure TX */
	if (emac_emb_smmu_ctx.valid)
		tx_setup_info->smmu_enabled = true;
	else
		tx_setup_info->smmu_enabled = false;

	if (!tx_setup_info->smmu_enabled)
		tx_setup_info->ring_base_pa =
			veth_emac_mem->tx_desc_mem_paddr;

	/* TX Descriptor Base Virtual Address*/
	if (tx_setup_info->smmu_enabled)
		tx_setup_info->ring_base_iova =
			veth_emac_mem->tx_desc_mem_iova;

	/* TX Descriptor Count*/
	tx_setup_info->ntn_ring_size = VETH_TX_DESC_CNT;

	/* Tx Buf pool base*/
	if (!tx_setup_info->smmu_enabled) {
		tx_setup_info->buff_pool_base_pa =
			veth_emac_mem->tx_buff_pool_base_pa;
	}

	tx_setup_info->buff_pool_base_iova =
		veth_emac_mem->tx_buff_pool_base_iova;

	/* TX buffer Count*/
	tx_setup_info->num_buffers = VETH_TX_DESC_CNT-1;

	/* TX Frame length */
	tx_setup_info->data_buff_size = VETH_ETH_FRAME_LEN_IPA;

	/*Map TX Buff Pool*/
	if (emac_emb_smmu_ctx.valid) {
		/*store tx buf iova addr in buff pool addresses*/
		/*store tx buf p addr in buff pool addresses*/
		veth_emac_mem->tx_buff_pool_base_va[0] =
			veth_emac_mem->tx_buf_mem_iova;
	} else {
		veth_emac_mem->tx_buff_pool_base_va[0] =
			veth_emac_mem->tx_buf_mem_paddr;
	}
	for (i = 0; i < VETH_TX_DESC_CNT; i++) {
		veth_emac_mem->tx_buff_pool_base_va[i] =
			veth_emac_mem->tx_buff_pool_base_va[0] +
			i*VETH_ETH_FRAME_LEN_IPA;
		VETH_IPA_DEBUG(
			"%s: veth_emac_mem->tx_buff_pool_base[%d] 0x%x\n",
			__func__, i, veth_emac_mem->tx_buff_pool_base_va[i]);
	}


	/* Allocate and Populate RX Buff List*/
	rx_setup_info->data_buff_list = kcalloc(rx_setup_info->num_buffers,
		sizeof(struct ntn_buff_smmu_map), GFP_KERNEL);
	if (rx_setup_info->data_buff_list == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	if (!rx_setup_info->smmu_enabled) {
		/* this case we use p addr in rx_buff_pool_base[0]*/
		rx_setup_info->data_buff_list[0].pa =
			veth_emac_mem->rx_buf_mem_paddr;
	} else {
		rx_setup_info->data_buff_list[0].pa =
			veth_emac_mem->rx_buf_mem_paddr;

		rx_setup_info->data_buff_list[0].iova =
			veth_emac_mem->rx_buf_mem_iova;
	}


	for (i = 0; i < rx_setup_info->num_buffers; i++) {
		rx_setup_info->data_buff_list[i].iova =
			rx_setup_info->data_buff_list[0].iova +
			i*VETH_ETH_FRAME_LEN_IPA;
		rx_setup_info->data_buff_list[i].pa =
			rx_setup_info->data_buff_list[0].pa +
			i*VETH_ETH_FRAME_LEN_IPA;
		//	veth_emac_mem->rx_buf_mem_paddr[i];
		VETH_IPA_INFO("rx_setup_info->data_buff_list[%d].iova = 0x%lx",
			i, rx_setup_info->data_buff_list[i].iova);
		VETH_IPA_INFO("rx_setup_info->data_buff_list[%d].pa = 0x%lx",
			i, rx_setup_info->data_buff_list[i].pa);
	}


	/* Allocate and Populate TX Buff List*/
	tx_setup_info->data_buff_list = kcalloc(tx_setup_info->num_buffers,
		sizeof(struct ntn_buff_smmu_map), GFP_KERNEL);
	if (tx_setup_info->data_buff_list == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	if (!tx_setup_info->smmu_enabled) {
		/* this case we use p addr in rx_buff_pool_base[0]*/
		tx_setup_info->data_buff_list[0].pa =
			veth_emac_mem->tx_buf_mem_paddr;
	} else {
		tx_setup_info->data_buff_list[0].pa =
			veth_emac_mem->tx_buf_mem_paddr;

		tx_setup_info->data_buff_list[0].iova =
			veth_emac_mem->tx_buf_mem_iova;
	}
	for (i = 0; i < tx_setup_info->num_buffers; i++) {
		tx_setup_info->data_buff_list[i].iova =
			tx_setup_info->data_buff_list[0].iova +
			i*VETH_ETH_FRAME_LEN_IPA;
		tx_setup_info->data_buff_list[i].pa =
			tx_setup_info->data_buff_list[0].pa +
			i*VETH_ETH_FRAME_LEN_IPA;
		VETH_IPA_INFO("tx_setup_info->data_buff_list[%d].iova = 0x%lx",
			i,
			tx_setup_info->data_buff_list[i].iova);
		VETH_IPA_INFO("tx_setup_info->data_buff_list[%d].pa = 0x%lx",
			i,
			tx_setup_info->data_buff_list[i].pa);
	}

	return ret;
}

int veth_ipa_offload_connect(struct veth_ipa_dev *pdata)
{

	struct veth_ipa_client_data *ipa_cdata = &pdata->prv_ipa;
	struct ipa_uc_offload_conn_in_params in;
	struct ipa_uc_offload_conn_out_params out;
	struct ipa_ntn_setup_info rx_setup_info;
	struct ipa_ntn_setup_info tx_setup_info;
	struct ipa_perf_profile profile;

	int ret = 0;

	/* Hard code SMMU Enable for PHASE 1*/
	emac_emb_smmu_ctx.valid = true;
	VETH_IPA_DEBUG("%s - begin smmu_s2_enb=%d\n", __func__,
		emac_emb_smmu_ctx.valid);

	if (!pdata) {
		VETH_IPA_ERROR("Null Param %s\n", __func__);
		return -EINVAL;
	}

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	memset(&profile, 0, sizeof(profile));
	memset(&tx_setup_info, 0, sizeof(tx_setup_info));
	memset(&rx_setup_info, 0, sizeof(rx_setup_info));

	/* Call for Export into Backend*/
    /*Map allocated memory to setup info structure*/
	ret = veth_map_rx_tx_setup_info_params(&rx_setup_info,
						&tx_setup_info,
						&(pdata->veth_emac_mem),
						pdata);
	if (ret) {
		pr_err("%s: veth_alloc_emac_export_mem failed error %d\n",
				__func__,
				ret);
		ret = -1;
		goto mem_free;
	}
	in.clnt_hndl = ipa_cdata->ipa_client_hndl;

	rx_setup_info.client = IPA_CLIENT_ETHERNET_PROD;
	tx_setup_info.client = IPA_CLIENT_ETHERNET_CONS;

	if (emac_emb_smmu_ctx.valid) {
		ret = veth_set_ul_dl_smmu_ipa_params(
			pdata, &(pdata->veth_emac_mem),
			&rx_setup_info, &tx_setup_info);

		if (ret) {
			pr_err(
				"Failed to build UL DL ipa_ntn_setup_info err:%d\n",
				ret);
			ret = -1;
			goto mem_free;
		}
	}

	/*switch to macros*/
	rx_setup_info.ntn_reg_base_ptr_pa = 0x00021100;
	tx_setup_info.ntn_reg_base_ptr_pa = 0x00021100;

	/* Dump UL and DL Setups */
	VETH_IPA_DEBUG(
		"IPA Offload UL client %d, ring_base_pa 0x%x, ",
		rx_setup_info.client,
		rx_setup_info.ring_base_pa
		);
	VETH_IPA_DEBUG(
		"ntn_ring_size %d, buff_pool_base_pa 0x%x, num_buffers %d, ",
		rx_setup_info.ntn_ring_size,
		rx_setup_info.buff_pool_base_pa,
		rx_setup_info.num_buffers
		);
	VETH_IPA_DEBUG(
		"data_buff_size %d, ntn_reg_base_ptr_pa 0x%x\n",
		rx_setup_info.data_buff_size,
		rx_setup_info.ntn_reg_base_ptr_pa
		);

	VETH_IPA_DEBUG(
		"IPA Offload DL client %d, ring_base_pa 0x%x, ",
		tx_setup_info.client,
		tx_setup_info.ring_base_pa
		);
	VETH_IPA_DEBUG(
		"ntn_ring_size %d, buff_pool_base_pa 0x%x, num_buffers %d, ",
		tx_setup_info.ntn_ring_size,
		tx_setup_info.buff_pool_base_pa,
		tx_setup_info.num_buffers
		);
	VETH_IPA_DEBUG(
		"data_buff_size %d, ntn_reg_base_ptr_pa 0x%x\n",
		tx_setup_info.data_buff_size,
		tx_setup_info.ntn_reg_base_ptr_pa
		);

	in.u.ntn.ul = rx_setup_info;
	in.u.ntn.dl = tx_setup_info;

	VETH_IPA_DEBUG(
		"IPA Offload UL clnt %d, ring_base_pa 0x%x, ntn_ring_size %d",
		in.u.ntn.ul.client,
		in.u.ntn.ul.ring_base_pa,
		in.u.ntn.ul.ntn_ring_size
		);
	VETH_IPA_DEBUG(
		", buff_pool_base_pa 0x%x, num_buffers %d, data_buff_size %d,",
		in.u.ntn.ul.buff_pool_base_pa,
		in.u.ntn.ul.num_buffers,
		in.u.ntn.ul.data_buff_size
		);
	VETH_IPA_DEBUG(
		"ntn_reg_base_ptr_pa 0x%x\n", in.u.ntn.ul.ntn_reg_base_ptr_pa);

	VETH_IPA_DEBUG(
		"IPA Offload DL client %d, ring_base_pa 0x%x, ",
		in.u.ntn.dl.client,
		in.u.ntn.dl.ring_base_pa
		);
	VETH_IPA_DEBUG(
		"ntn_ring_size %d, buff_pool_base_pa 0x%x, num_buffers %d, ",
		in.u.ntn.dl.ntn_ring_size,
		in.u.ntn.dl.buff_pool_base_pa,
		in.u.ntn.dl.num_buffers
		);
	VETH_IPA_DEBUG(
		"data_buff_size %d, ntn_reg_base_ptr_pa 0x%x\n",
		in.u.ntn.dl.data_buff_size,
		in.u.ntn.dl.ntn_reg_base_ptr_pa
		);

	ret = ipa_uc_offload_conn_pipes(&in, &out);

	if (ret) {
		pr_err("Could not connect IPA Offload Pipes %d\n", ret);
		ret = -1;
		goto mem_free;
	}

	ipa_cdata->uc_db_rx_addr = out.u.ntn.ul_uc_db_pa;
	ipa_cdata->uc_db_tx_addr = out.u.ntn.dl_uc_db_pa;

	/* Set Perf Profile For PROD/CONS Pipes,
	 * request link speed from BackEnd.
	 */
	profile.max_supported_bw_mbps = pdata->speed;
	profile.client = IPA_CLIENT_ETHERNET_PROD;

	ret = ipa_set_perf_profile(&profile);
	if (ret) {
		pr_err(
			"Err to set BW: IPA_RM_RESOURCE_ETHERNET_CONS err:%d\n",
			ret);
		ret = -1;
		goto mem_free;
	}

mem_free:
	if (emac_emb_smmu_ctx.valid) {
		if (rx_setup_info.ring_base_sgt) {
			sg_free_table(rx_setup_info.ring_base_sgt);
			kfree(rx_setup_info.ring_base_sgt);
			rx_setup_info.ring_base_sgt = NULL;
		}
		if (tx_setup_info.ring_base_sgt) {
			sg_free_table(tx_setup_info.ring_base_sgt);
			kfree(tx_setup_info.ring_base_sgt);
			tx_setup_info.ring_base_sgt = NULL;
		}
		if (rx_setup_info.buff_pool_base_sgt) {
			sg_free_table(rx_setup_info.buff_pool_base_sgt);
			kfree(rx_setup_info.buff_pool_base_sgt);
			rx_setup_info.buff_pool_base_sgt = NULL;
		}
		if (tx_setup_info.buff_pool_base_sgt) {
			sg_free_table(tx_setup_info.buff_pool_base_sgt);
			kfree(tx_setup_info.buff_pool_base_sgt);
			tx_setup_info.buff_pool_base_sgt = NULL;
		}
	}

	VETH_IPA_DEBUG("%s - end\n", __func__);
	return 0;

}

/**
 * veth_enable_ipa_offload() - Enable IPA offload.
 * @pdata: veth drvier context pointer.
 *
 * This is to be called after all of following are met
 *   1 Veth Dev Open,
 *   2 IPA ready,
 *   3 IPA UC ready,
 *   4 Emac Ready ,
 *   5 Phy is up
 *
 * Return: negative errno, or zero on success
 */
int veth_enable_ipa_offload(struct veth_ipa_dev *pdata)
{
	int ret = 0;

	if (!pdata->prv_ipa.ipa_offload_init) {
		ret = veth_ipa_offload_init(pdata);
		if (ret) {
			pdata->prv_ipa.ipa_offload_init = false;
			VETH_IPA_DEBUG("IPA Offload Init Failed\n");
			goto fail;
		}
		VETH_IPA_DEBUG("IPA Offload Initialized Successfully\n");
		pdata->prv_ipa.ipa_offload_init = true;
	}

	if (!pdata->prv_ipa.ipa_offload_conn) {
		ret = veth_ipa_offload_connect(pdata);
		if (ret) {
			VETH_IPA_DEBUG("IPA Offload Connect Failed\n");
			pdata->prv_ipa.ipa_offload_conn = false;
			goto fail;
		}
		VETH_IPA_DEBUG("IPA Offload Connect Successfully\n");
		pdata->prv_ipa.ipa_offload_conn = true;

		/*Initialize DMA CHs for offload*/
		/*Blocking call to Back end Driver on DMA Init*/

		if (ret) {
			VETH_IPA_DEBUG("Offload channel Init Failed\n");
			goto fail;
		}
	}
	VETH_IPA_DEBUG("IPA Offload Enabled successfully\n");
	return ret;

fail:
	if (pdata->prv_ipa.ipa_offload_conn) {
		if (veth_ipa_offload_disconnect(pdata))
			VETH_IPA_DEBUG("IPA Offload Disconnect Failed\n");
		else
			VETH_IPA_DEBUG(
				"IPA Offload Disconnect Successfully\n");
		pdata->prv_ipa.ipa_offload_conn = false;
	}

	if (pdata->prv_ipa.ipa_offload_init) {
		if (veth_ipa_offload_cleanup(pdata))
			VETH_IPA_DEBUG("IPA Offload Cleanup Failed\n");
		else
			VETH_IPA_DEBUG("IPA Offload Cleanup Success\n");
		pdata->prv_ipa.ipa_offload_init = false;
	}
	return ret;
}

int veth_disable_ipa_offload(struct veth_ipa_dev *pdata)
{
	int ret = 0;

	if (pdata->prv_ipa.ipa_offload_conn) {
		if (veth_ipa_offload_disconnect(pdata)) {
			VETH_IPA_DEBUG("IPA Offload Disconnect Failed\n");
			return -EINVAL;
		}
		VETH_IPA_DEBUG("IPA Offload Disconnect Successfully\n");
		pdata->prv_ipa.ipa_offload_conn = false;
	}

	if (pdata->prv_ipa.ipa_offload_init) {
		if (veth_ipa_offload_cleanup(pdata)) {
			VETH_IPA_DEBUG("IPA Offload Cleanup Failed\n");
			return -EINVAL;
		}
		VETH_IPA_DEBUG("IPA Offload Cleanup Success\n");
		pdata->prv_ipa.ipa_offload_init = false;
	}

	ret = veth_ipa_send_msg(pdata, IPA_PERIPHERAL_DISCONNECT);
	if (ret < 0) {
		VETH_IPA_ERROR("%s: eth_ipa_send_msg failed\n", __func__);
		return ret;
	}
	VETH_IPA_DEBUG("eth_ipa_send_msg complete\n");
	return 0;
}


static void veth_ipa_offload_event_handler(
	struct veth_ipa_dev *pdata,
	enum IPA_OFFLOAD_EVENT ev
	)
{
	int ret = 0;

	VETH_IPA_LOCK();
	VETH_IPA_LOG_ENTRY();
//	VETH_IPA_DEBUG(" Enter: event=%s\n", IPA_OFFLOAD_EVENT_string[ev]);

	switch (ev) {
	case EV_DEV_OPEN:
		{
		   VETH_IPA_DEBUG("%s:%d EV_DEV_OPEN offload handler\n",
						  __func__,
						  __LINE__);
			if (!pdata->prv_ipa.ipa_ready) {
				veth_ipa_ready(pdata); /*register */
				VETH_IPA_DEBUG("%s:%d VETH IPA registered\n",
							  __func__, __LINE__);
			}

			if (pdata->prv_ipa.ipa_ready) {
				if (!pdata->prv_ipa.ipa_offload_init) {
					if (!veth_ipa_offload_init(pdata))
						pdata->prv_ipa.ipa_offload_init
									= true;
				}

				if (!pdata->prv_ipa.ipa_uc_ready)
					veth_ipa_uc_ready(pdata);

				ret = veth_emac_open_notify(
					&(pdata->veth_emac_mem),
					pdata);
				if (ret < 0) {
					pr_err("%s: veth_emac_open_notify failed error %d\n",
						__func__,
						ret);
				}
			}
		}
		break;

	case EV_IPA_READY:
		{

			pdata->prv_ipa.ipa_ready = true;

			if (!pdata->prv_ipa.ipa_offload_init) {
				if (!veth_ipa_offload_init(pdata))
					pdata->prv_ipa.ipa_offload_init = true;
			}

			if (!pdata->prv_ipa.ipa_uc_ready)
				veth_ipa_uc_ready(pdata);
		}
		break;

	case EV_IPA_UC_READY:
		{

			if (!pdata->prv_ipa.ipa_uc_ready)
				veth_ipa_uc_ready(pdata);

			pdata->prv_ipa.ipa_uc_ready = true;
			VETH_IPA_DEBUG("%s:%d ipa uC is ready\n",
						   __func__,
						   __LINE__);

			VETH_IPA_INFO("Export buffers", __func__, __LINE__);
			ret = veth_emac_open_notify(
					&(pdata->veth_emac_mem),
					pdata);
			if (ret < 0) {
				pr_err("%s: veth_emac_open_notify failed error %d\n",
					__func__,
					ret);
			}
		}
		break;
	case EV_IPA_EMAC_INIT:{
			VETH_IPA_DEBUG("%s:%d EV_IPA_EMAC_INIT\n",
						   __func__,
						   __LINE__);
			/* Allocate memory that is to be exported to back end*/
			VETH_IPA_DEBUG("%s - veth_alloc_emac_export_mem  %p\n",
						   __func__,
						   pdata->pdev);

			ret = veth_alloc_emac_export_mem(
						&(pdata->veth_emac_mem),
						pdata);
			if (ret) {
				pr_err("%s: veth_alloc_emac_export_mem failed %d\n",
					  __func__,
					  ret);
				ret = -1;
			}

			/* Allocate memory that is to be exported to back end*/
			VETH_IPA_DEBUG("%s - veth_emac_init\n",
						   __func__);

			ret = veth_emac_init(&(pdata->veth_emac_mem),
					pdata,
					emac_emb_smmu_ctx.valid);
			if (ret) {
				pr_err("%s: veth_alloc_emac_export_mem failed error %d\n",
						__func__,
						ret);
				ret = -1;
			}
			ret = veth_emac_setup_be(&(pdata->veth_emac_mem),
									 pdata);
			if (ret) {
				pr_err("%s: veth_emac_setup_offload failed error %d\n",
						__func__,
						ret);
				ret = -1;
				//return ret;
			}

			pdata->state = VETH_IPA_CONNECTED;
			VETH_IPA_DEBUG("%s - veth_emac_setup_offload %p\n",
							__func__, pdata->pdev);
		}
		break;


	case EV_IPA_EMAC_SETUP:{
		if (pdata->prv_ipa.ipa_uc_ready)
			veth_enable_ipa_offload(pdata);

		ret = veth_emac_ipa_setup_complete(
						&(pdata->veth_emac_mem),
						pdata);
		if (ret) {
			pr_err("%s: veth_emac_setup_offload failed error %d\n",
					__func__,
					ret);
			ret = -1;
		}
	}
	break;

	case EV_PHY_LINK_UP:{
		VETH_IPA_DEBUG("%s:%d EV_PHY_LINK_UP offload handler\n",
						   __func__,
						   __LINE__);
		ret = veth_emac_start_offload(&(pdata->veth_emac_mem),
									pdata);
		if (ret) {
			pr_err("%s: veth_emac_setup_offload failed error %d\n",
					__func__,
					ret);
			ret = -1;
		}
	}
	break;

	case EV_START_OFFLOAD:
	{

		VETH_IPA_DEBUG("%s:%d EV_START_OFFLOAD offload handler\n",
						__func__, __LINE__);

		netif_carrier_on(pdata->net);

		VETH_IPA_DEBUG("netif_carrier_on() was called\n");

		netif_start_queue(pdata->net);

		VETH_IPA_DEBUG("netif_start_queue() was called");

		pdata->state = VETH_IPA_CONNECTED_AND_UP;

		pdata->veth_emac_dev_ready = true;
	}
	break;
	case EV_DEV_CLOSE:
		{
			if (pdata->prv_ipa.ipa_uc_ready) {
				pr_info("%s: EV_DEV_CLOSE veth_disable_ipa_offload\n",
						__func__);
				veth_disable_ipa_offload(pdata);
				ipa_uc_offload_dereg_rdyCB(IPA_UC_NTN);
			}

			VETH_IPA_DEBUG("%s:veth_alloc_emac_dealloc_mem",
							__func__);
			if (pdata->veth_emac_dev_ready) {
				veth_alloc_emac_dealloc_mem(
					&(pdata->veth_emac_mem),
					pdata);
				pdata->veth_emac_dev_ready = false;
			}

			pdata->state = VETH_IPA_DOWN;
		}
		break;

	case EV_INVALID:
		break;

	default:
		break;
	}

	VETH_IPA_UNLOCK();
	if (ev < 9)
		VETH_IPA_DEBUG("Exit: event=%s\n",
			IPA_OFFLOAD_EVENT_string[ev]);
}

static void  veth_ipa_emac_deinit_wq(struct work_struct *work)
{
	struct veth_ipa_client_data *ntn_ipa = container_of(work,
						   struct veth_ipa_client_data,
						   ntn_emac_de_init_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);

	veth_ipa_offload_event_handler(pdata, EV_DEV_CLOSE);
}

static void veth_ipa_emac_deinit_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}
	INIT_WORK(&ntn_ipa->ntn_emac_de_init_rdy_work, veth_ipa_emac_deinit_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_emac_de_init_rdy_work);
}


static void  veth_ipa_emac_start_offload_wq(struct work_struct *work)
{
	struct veth_ipa_client_data *ntn_ipa = container_of(work,
					struct veth_ipa_client_data,
					ntn_emac_start_offload_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);

	veth_ipa_offload_event_handler(pdata, EV_START_OFFLOAD);
}

static void veth_ipa_emac_start_offload_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}

	INIT_WORK(&ntn_ipa->ntn_emac_start_offload_rdy_work,
			veth_ipa_emac_start_offload_wq);
	queue_work(system_unbound_wq,
			&ntn_ipa->ntn_emac_start_offload_rdy_work);
}



static void  veth_ipa_emac_link_up_wq(struct work_struct *work)
{

	struct veth_ipa_client_data *ntn_ipa = container_of(work,
						   struct veth_ipa_client_data,
						   ntn_emac_link_up_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);
	VETH_IPA_DEBUG("%s:%d\n", __func__, __LINE__);
	veth_ipa_offload_event_handler(pdata, EV_PHY_LINK_UP);
}

static void veth_ipa_emac_link_up_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}

	INIT_WORK(&ntn_ipa->ntn_emac_link_up_rdy_work,
			veth_ipa_emac_link_up_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_emac_link_up_rdy_work);
}


static void  veth_ipa_emac_setup_done_wq(struct work_struct *work)
{
	struct veth_ipa_client_data *ntn_ipa = container_of(work,
						   struct veth_ipa_client_data,
						   ntn_emac_setup_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);


	VETH_IPA_DEBUG("%s:%d\n", __func__, __LINE__);
	veth_ipa_offload_event_handler(pdata, EV_IPA_EMAC_SETUP);
}

static void veth_ipa_emac_setup_done_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}

	INIT_WORK(&ntn_ipa->ntn_emac_setup_rdy_work,
			veth_ipa_emac_setup_done_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_emac_setup_rdy_work);
}

static void  veth_ipa_open_wq(struct work_struct *work)
{
	struct veth_ipa_client_data *ntn_ipa = container_of(work,
						   struct veth_ipa_client_data,
						   ntn_emac_open_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);

	veth_ipa_offload_event_handler(pdata, EV_DEV_OPEN);
}

static void veth_ipa_open_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}

	INIT_WORK(&ntn_ipa->ntn_emac_open_rdy_work, veth_ipa_open_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_emac_open_rdy_work);
}


static void  veth_ipa_emac_init_done_wq(struct work_struct *work)
{
	struct veth_ipa_client_data *ntn_ipa = container_of(work,
						   struct veth_ipa_client_data,
						   ntn_emac_init_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);

	veth_ipa_offload_event_handler(pdata, EV_IPA_EMAC_INIT);
}

static void veth_ipa_emac_init_done_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}
	VETH_IPA_INFO("%s IPA ready wq callback\n", __func__);
	INIT_WORK(&ntn_ipa->ntn_emac_init_rdy_work, veth_ipa_emac_init_done_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_emac_init_rdy_work);
}


static void veth_ipa_ready_wq(struct work_struct *work)
{
	struct veth_ipa_client_data *ntn_ipa = container_of(work,
						   struct veth_ipa_client_data,
						   ntn_ipa_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);

	VETH_IPA_DEBUG("%s:%d\n", __func__, __LINE__);
	VETH_IPA_INFO("%s IPA ready wq callback\n", __func__);
	veth_ipa_offload_event_handler(pdata, EV_IPA_READY);
}

static void veth_ipa_uc_ready_wq(struct work_struct *work)
{
	struct veth_ipa_client_data *ntn_ipa = container_of(work,
					   struct veth_ipa_client_data,
					   ntn_ipa_uc_rdy_work);
	struct veth_ipa_dev *pdata = container_of(ntn_ipa,
					 struct veth_ipa_dev,
					 prv_ipa);

	VETH_IPA_DEBUG("%s:%d veth_ipa_ready_wq\n", __func__, __LINE__);
	VETH_IPA_INFO("%s IPA UC ready wq callback\n", __func__);
	veth_ipa_offload_event_handler(pdata, EV_IPA_UC_READY);
}

/**
 * veth_ipa_ready_cb() - callback registered with IPA to
 * indicate that IPA is ready to receive configuration.
 * @user_data: veth context handle.
 */
static void veth_ipa_ready_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}

	VETH_IPA_INFO("%s Received IPA ready callback\n", __func__);

	INIT_WORK(&ntn_ipa->ntn_ipa_rdy_work, veth_ipa_ready_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_ipa_rdy_work);
}

/**
 * veth_ipa_uc_ready_cb() - callback registered with UC to
 * indicate that IPA and IPA uC is ready to receive config
 * commands.
 * @user_data: veth context handle.
 */

static void veth_ipa_uc_ready_cb(void *user_data)
{
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)user_data;
	struct veth_ipa_client_data *ntn_ipa = &pdata->prv_ipa;

	if (!pdata) {
		VETH_IPA_ERROR("%s Null Param pdata\n", __func__);
		return;
	}

	VETH_IPA_INFO("%s Received IPA UC ready callback\n", __func__);
	INIT_WORK(&ntn_ipa->ntn_ipa_uc_rdy_work, veth_ipa_uc_ready_wq);
	queue_work(system_unbound_wq, &ntn_ipa->ntn_ipa_uc_rdy_work);

	return;

}

/*register IPA ready callback*/
static int veth_ipa_ready(struct veth_ipa_dev *pdata)
{
	int ret;

	VETH_IPA_DEBUG("Enter\n");
#ifdef INT_NOIPA
	veth_ipa_ready_cb(pdata);
	ret = 1;
#else
	ret = ipa_register_ipa_ready_cb(veth_ipa_ready_cb, (void *)pdata);
#endif

	if (ret == -ENXIO) {
		VETH_IPA_DEBUG("%s: IPA driver context is not even ready\n",
			__func__);
		return ret;
	}
	if (ret != -EEXIST) {
		VETH_IPA_DEBUG("%s:%d register ipa ready cb\n",
			__func__,
			__LINE__);
		return ret;
	}

	pdata->prv_ipa.ipa_ready = true;

	return ret;

}

static int veth_ipa_uc_ready(struct veth_ipa_dev *pdata)
{
#ifndef INT_NOIPA
	struct ipa_uc_ready_params param;
#endif
	int ret;

	VETH_IPA_DEBUG("%s: Enter", __func__);

#ifdef INT_NOIPA
	veth_ipa_uc_ready_cb(pdata);
	ret = 1;
#else
	param.is_uC_ready = false;
	param.priv = pdata;
	param.notify = veth_ipa_uc_ready_cb;
	param.proto = IPA_UC_NTN;

	ret = ipa_uc_offload_reg_rdyCB(&param);
	if (ret == 0 && param.is_uC_ready) {
		VETH_IPA_DEBUG("%s:%d ipa uc ready\n", __func__, __LINE__);
		pdata->prv_ipa.ipa_uc_ready = true;
	}
#endif

	VETH_IPA_DEBUG("%s: Exit", __func__);
	return ret;
}


/**
 * veth_ipa_emac_evt_mgmt() - Called from driver for receiving
 * event from the BE driver regarding the EMAC
 *
 * IN: @arg: argument sent from kthread create, contains n/w
 * device info
 * OUT: 0 on success and
 * -1 on failure
 */
static int veth_ipa_emac_evt_mgmt(void *arg)
{
	/*Wait on HAV receive here*/
	int ret = 0;
	int timeout_ms = 100;
	struct emac_hab_mm_message pdata_recv;
	//veth_emac_import_iova msg;
	int pdata_size = sizeof(pdata_recv);
	struct veth_ipa_dev *pdata = (struct veth_ipa_dev *)arg;
	//memset(&msg, 0, sizeof(struct veth_emac_import_iova) );
	VETH_IPA_INFO("%s: vc_id %d\n", __func__, pdata->veth_emac_mem.vc_id);
	while (1) {
		ret = habmm_socket_recv(pdata->veth_emac_mem.vc_id,
				&pdata_recv,
				&pdata_size,
				timeout_ms,
				0x0);
		VETH_IPA_INFO("EVENT ID Received: %x", pdata_recv.event_id);
		if (!ret) {
			VETH_IPA_INFO("%s: msg->event_id %d\n", __func__, pdata_recv);
			switch (pdata_recv.event_id) {
			case EV_IPA_EMAC_INIT:
				/*
				 * To avoid spurious events, possibly not required once state
				 * machine is available
				 */
				if (!pdata->prv_ipa.emac_init) {
					VETH_IPA_INFO("EMAC_INIT event received\n");
					pr_info("%s: emac_init set to true\n", __func__);
					veth_ipa_emac_init_done_cb(pdata);
					pdata->prv_ipa.emac_init = true;
				}
			break;
			case EV_IPA_EMAC_SETUP:
				/*use memcpy_s later instead*/
				if (emac_emb_smmu_ctx.valid) {
					pdata->veth_emac_mem.tx_desc_mem_iova =
						(dma_addr_t)
					pdata_recv.msg_type.iova.tx_desc_mem_iova;
					pdata->veth_emac_mem.rx_desc_mem_iova =
						(dma_addr_t)
					pdata_recv.msg_type.iova.rx_desc_mem_iova;
					pdata->veth_emac_mem.tx_buf_mem_iova =
						(dma_addr_t)
					pdata_recv.msg_type.iova.tx_buf_mem_iova;
					pdata->veth_emac_mem.rx_buf_mem_iova =
						(dma_addr_t)
					pdata_recv.msg_type.iova.rx_buf_mem_iova;
					pdata->veth_emac_mem.tx_buff_pool_base_iova =
						(dma_addr_t)
					pdata_recv.msg_type.iova.tx_buf_pool_base_iova;
					pdata->veth_emac_mem.rx_buff_pool_base_iova =
						(dma_addr_t)
					pdata_recv.msg_type.iova.rx_buf_pool_base_iova;
					}
				VETH_IPA_INFO("EMAC_SETUP event received\n");
				VETH_IPA_INFO("union received: %x",
				pdata->veth_emac_mem.tx_buff_pool_base_iova);
				veth_ipa_emac_setup_done_cb(pdata);
			break;
			case EV_PHY_LINK_UP:
				VETH_IPA_INFO("EMAC_PHY_LINK_UP event received\n");
				veth_ipa_emac_link_up_cb(pdata);
			break;
			case EV_START_OFFLOAD:
				VETH_IPA_INFO("EV_START_OFFLOAD event received\n");
				veth_ipa_emac_start_offload_cb(pdata);
			break;
			case EV_EMAC_DEINIT:
				VETH_IPA_INFO("EMAC_DEINIT event received\n");
				veth_ipa_emac_deinit_cb(pdata);
				pdata->prv_ipa.emac_init = false;
			break;
			default:
				VETH_IPA_ERROR("Unknown event received\n");
			break;
			}
		}
	}
	return 0;
}
/**
 * veth_ipa_init() - create network device and initializes
 *  internal data structures
 * @params: in/out parameters required for VETH_ipa initialization
 *
 * Shall be called prior to pipe connection.
 * The out parameters (the callbacks) shall be supplied to ipa_connect.
 * Detailed description:
 *  - allocate the network device
 *  - set default values for driver internals
 *  - create debugfs folder and files
 *  - create IPA resource manager client
 *  - add header insertion rules for IPA driver (based on host/device
 *    Ethernet addresses given in input params)
 *  - register tx/rx properties to IPA driver (will be later used
 *    by IPA configuration manager to configure reset of the IPA rules)
 *  - set the carrier state to "off" (until VETH_ipa_connect is called)
 *  - register the network device
 *  - set the out parameters
 *
 * Returns negative errno, or zero on success
 */
static int veth_ipa_init(struct platform_device *pdev)
{
	int                  result = 0;
	struct net_device   *dev = NULL;
	struct veth_ipa_dev *veth_ipa_pdata = NULL;
	unsigned char        dev_ethaddr[ETH_ALEN] = {0x0, 0x55, 0x7B,
						0xB5, 0x7D, 0xF9};

	VETH_IPA_LOG_ENTRY();
	pr_info("veth start initialization\n");
	VETH_IPA_DEBUG("%s initializing\n", DRIVER_NAME);

	dev = alloc_etherdev(sizeof(struct veth_ipa_dev));
	if (!dev) {
		result = -ENOMEM;
		dev_err(&pdev->dev, "Unable to alloc new net device\n");
		VETH_IPA_ERROR("fail to allocate new net device\n");
		goto fail_alloc_etherdev;
	}
	VETH_IPA_DEBUG("network device was successfully allocated\n");

	dev->dev_addr[0] = dev_ethaddr[0];
	dev->dev_addr[1] = dev_ethaddr[1];
	dev->dev_addr[2] = dev_ethaddr[2];
	dev->dev_addr[3] = dev_ethaddr[3];
	dev->dev_addr[4] = dev_ethaddr[4];
	dev->dev_addr[5] = dev_ethaddr[5];

	/** Set the sysfs physical device reference for the network logical
	 * device if set prior to registration will cause a symlink
	 * during initialization.
	 */
	SET_NETDEV_DEV(dev, &pdev->dev);
	veth_ipa_pdata = netdev_priv(dev);
	if (!veth_ipa_pdata) {
		VETH_IPA_ERROR("fail to extract netdev priv\n");
		result = -ENOMEM;
		goto fail_netdev_priv;
	}


	memset(veth_ipa_pdata, 0, sizeof(*veth_ipa_pdata));
	VETH_IPA_DEBUG("veth_ipa_pdata; (private) = %pK\n", veth_ipa_pdata);

	platform_set_drvdata(pdev, dev);
	veth_ipa_pdata->pdev = pdev;
	veth_ipa_pdata->net  = dev;

	snprintf(dev->name,
		sizeof(dev->name),
		"%s%%d",
		VETH_IFACE_NAME,
		VETH_IFACE_NUM0);

	dev->netdev_ops = &veth_ipa_netdev_ops;
	VETH_IPA_DEBUG("internal data structures were initialized\n");

	veth_ipa_debugfs_init(veth_ipa_pdata);

	/*Make this configurable.*/
	result = veth_ipa_set_device_ethernet_addr(dev->dev_addr, dev_ethaddr);
	if (result) {
		VETH_IPA_ERROR("set device MAC failed\n");
		goto fail_set_device_ethernet;
	}


	VETH_IPA_DEBUG("Device Ethernet address set %pM\n", dev->dev_addr);

	VETH_IPA_DEBUG("is vlan mode %d\n", veth_ipa_pdata->is_vlan_mode);

	netif_carrier_off(dev);
	VETH_IPA_DEBUG("netif_carrier_off() was called\n");

	netif_stop_queue(veth_ipa_pdata->net);
	VETH_IPA_DEBUG("netif_stop_queue() was called");

	result = register_netdev(dev);
	if (result) {
		VETH_IPA_ERROR("register_netdev failed: %d\n", result);
		goto fail_register_netdev;
	}
	VETH_IPA_DEBUG("register_netdev succeeded\n");

#ifndef INT_NOIPA
	veth_ipa_pdata->state = VETH_IPA_INITIALIZED;
#endif

	mutex_init(&veth_ipa_pdata->prv_ipa.ipa_lock);
	veth_ipa_pdata->prv_ipa.emac_init = false;
	veth_ipa_pdata->veth_emac_mem.init_complete = false;
	pr_info("VETH_IPA init flag set to false\n");
	veth_ipa_init_flag = false;
	VETH_IPA_STATE_DEBUG(veth_ipa_pdata);
	veth_pdata_p = veth_ipa_pdata;
	VETH_IPA_INFO("VETH_IPA was initialized successfully\n");


	if (veth_ipa_pdata->prv_ipa.ipa_ready)
		veth_ipa_ready(veth_ipa_pdata);

	VETH_IPA_LOG_EXIT();

	return 0;

fail_register_netdev:
fail_set_device_ethernet:
	veth_ipa_debugfs_destroy(veth_ipa_pdata);
fail_netdev_priv:
	free_netdev(dev);
fail_alloc_etherdev:

	VETH_IPA_DEBUG("veth initialization complete return val=%d\n", result);
	return result;
}

/**
 * VETH_ipa_open() - notify Linux network stack to start sending packets
 * @net: the network interface supplied by the network stack
 *
 * Linux uses this API to notify the driver that the network interface
 * transitions to the up state.
 * The driver will instruct the Linux network stack to start
 * delivering data packets.
 */
static int veth_ipa_open(struct net_device *net)
{
	struct veth_ipa_dev *veth_ipa_ctx;
	struct task_struct  *emac_evt;
	unsigned int mmid;
#ifndef INT_NOIPA
	//int next_state;
#endif

	VETH_IPA_LOG_ENTRY();

	/*to do double check this. Redundant*/
	veth_ipa_ctx = netdev_priv(net);

#ifdef INT_NOIPA
	veth_ipa_offload_event_handler(veth_ipa_ctx, EV_DEV_OPEN);
#else

	//next_state = veth_ipa_next_state(veth_ipa_ctx->state, VETH_IPA_OPEN);

	//if (next_state == VETH_IPA_INVALID) {
	//	VETH_IPA_ERROR("can't bring driver up before initialize\n");
	//	return -EPERM;
	//}
	veth_ipa_ctx->state = VETH_IPA_UP;
	VETH_IPA_STATE_DEBUG(veth_ipa_ctx);

	if (veth_ipa_ctx->state == VETH_IPA_CONNECTED_AND_UP)
		VETH_IPA_DEBUG("data path enable is disabled for now\n");
	else
		VETH_IPA_DEBUG("data path was not enabled yet\n");

	if (!veth_ipa_ctx->veth_emac_mem.init_complete) {
		mmid = HAB_MMID_CREATE(MM_DATA_NETWORK_1,
					   MM_DATA_NETWORK_1_VM_HAB_MINOR_ID);
		veth_ipa_ctx->veth_emac_mem.vc_id =
		   veth_emac_ipa_hab_init(mmid);
		if (veth_ipa_ctx->veth_emac_mem.vc_id < 0) {
			VETH_IPA_ERROR("%s: HAB init failed, returning error\n",
							__func__);
			return -EAGAIN;
		}
		emac_evt =  kthread_run(&veth_ipa_emac_evt_mgmt,
						(void *)veth_ipa_ctx,
						"veth_ipa_emac_evt_mgmt");
		VETH_IPA_INFO("%s: Starting EMAC kthread\n", __func__);
		veth_ipa_ctx->veth_emac_mem.init_complete = true;
	}
	veth_ipa_init_flag = true;
	pr_info("VETH_IPA init flag set to true\n");
	veth_ipa_offload_event_handler(veth_ipa_ctx, EV_DEV_OPEN);


#endif

	VETH_IPA_LOG_EXIT();

	return 0;
}

/**
 * VETH_ipa_start_xmit() - send data from APPs to USB core via IPA core
 * @skb: packet received from Linux network stack
 * @net: the network device being used to send this packet
 *
 * Several conditions needed in order to send the packet to IPA:
 * - Transmit queue for the network driver is currently
 *   in "send" state
 * - The driver internal state is in "UP" state.
 * - Filter Tx switch is turned off
 * - The IPA resource manager state for the driver producer client
 *   is "Granted" which implies that all the resources in the dependency
 *   graph are valid for data flow.
 * - outstanding high boundary did not reach.
 *
 * In case all of the above conditions are met, the network driver will
 * send the packet by using the IPA API for Tx.
 * In case the outstanding packet high boundary is reached, the driver will
 * stop the send queue until enough packet were proceeded by the IPA core.
 */
static netdev_tx_t veth_ipa_start_xmit
(
	struct sk_buff *skb,
	struct net_device *net
	)
{
	int ret;
	netdev_tx_t status = NETDEV_TX_BUSY;
	struct veth_ipa_dev *veth_ipa_ctx = netdev_priv(net);

	netif_trans_update(net);
	VETH_IPA_DEBUG_XMIT("Tx, len=%d, skb->protocol=%d, outstanding=%d\n",
		skb->len,
		skb->protocol,
		atomic_read(&veth_ipa_ctx->outstanding_pkts));

	if (unlikely(netif_queue_stopped(net))) {
		VETH_IPA_ERROR("interface queue is stopped\n");
		status = NETDEV_TX_BUSY;
		goto fail_tx_packet;
	}

	if (unlikely(veth_ipa_ctx->state != VETH_IPA_CONNECTED_AND_UP))
		return NETDEV_TX_BUSY;

	if (atomic_read(&veth_ipa_ctx->outstanding_pkts) >=
			veth_ipa_ctx->outstanding_high) {
		VETH_IPA_DEBUG("outstanding high (%d)- stopping\n",
			veth_ipa_ctx->outstanding_high);
		netif_stop_queue(net);
		status = NETDEV_TX_BUSY;
		goto fail_tx_packet;
	}

	if (veth_ipa_ctx->is_vlan_mode)
		if (unlikely(skb->protocol != htons(ETH_P_8021Q)))
			VETH_IPA_DEBUG(
				"ether_type != ETH_P_8021Q && vlan, prot = 0x%X\n",
				skb->protocol);



	ret = ipa_tx_dp(IPA_CLIENT_ETHERNET_CONS, skb, NULL);

	if (ret) {
		VETH_IPA_ERROR("ipa transmit failed (%d)\n", ret);
		goto fail_tx_packet;
	}

	atomic_inc(&veth_ipa_ctx->outstanding_pkts);

	status = NETDEV_TX_OK;
	goto out;

fail_tx_packet:
	return status;
out:
	return status;

}

/**
 * VETH_ipa_packet_receive_notify() - Rx notify
 *
 * @priv: VETH driver context
 * @evt: event type
 * @data: data provided with event
 *
 * IPA will pass a packet to the Linux network stack with skb->data pointing
 * to Ethernet packet frame.
 */
static void veth_ipa_packet_receive_notify
(
	void *priv,
	enum ipa_dp_evt_type evt,
	unsigned long data
	)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct veth_ipa_dev *veth_ipa_ctx = priv;
	int result;
	unsigned int packet_len;

	pr_debug(" %s: entry\n", __func__);
	if (!skb) {
		VETH_IPA_ERROR("Bad SKB received from IPA driver\n");
		return;
	}

	packet_len = skb->len;

	if (unlikely(veth_ipa_ctx->state != VETH_IPA_CONNECTED_AND_UP)) {
		//VETH_IPA_DEBUG("Missing pipe connected and/or iface up\n");
		return;
	}

	if (evt == IPA_WRITE_DONE) {
		atomic_dec(&veth_ipa_ctx->outstanding_pkts);
		if
		(netif_queue_stopped(veth_ipa_ctx->net) &&
			netif_carrier_ok(veth_ipa_ctx->net) &&
			atomic_read(&veth_ipa_ctx->outstanding_pkts) <
					(veth_ipa_ctx->outstanding_low)) {
			netif_wake_queue(veth_ipa_ctx->net);
		}
		kfree_skb(skb);
		return;
	}
	if (evt != IPA_WRITE_DONE && evt != IPA_RECEIVE) {
		VETH_IPA_ERROR("Non TX_complete or Receive Event");
		return;
	}
	skb->dev = veth_ipa_ctx->net;
	skb->protocol = eth_type_trans(skb, veth_ipa_ctx->net);

	result = netif_receive_skb(skb);
	if (result)
		VETH_IPA_ERROR("fail on netif_rx\n");
	veth_ipa_ctx->net->stats.rx_packets++;
	veth_ipa_ctx->net->stats.rx_bytes += packet_len;
}

static void __ipa_eth_free_msg(void *buff, u32 len, u32 type) {}


static int veth_ipa_send_msg(struct veth_ipa_dev *pdata,
	enum ipa_peripheral_event event)
{
	struct ipa_msg_meta msg_meta;
	struct ipa_ecm_msg emac_msg;

	if (!pdata || !pdata->net) {
		VETH_IPA_ERROR("wrong pdata\n");
		return -EFAULT;
	}

	memset(&msg_meta, 0, sizeof(msg_meta));
	memset(&emac_msg, 0, sizeof(emac_msg));
	VETH_IPA_ERROR("ifindex  %d\n", pdata->net->ifindex);

	emac_msg.ifindex = pdata->net->ifindex;
	strlcpy(emac_msg.name, pdata->net->name, IPA_RESOURCE_NAME_MAX);

	msg_meta.msg_type = event;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);

	return ipa_send_msg(&msg_meta, &emac_msg, __ipa_eth_free_msg);
}


/** VETH_ipa_stop() - called when network device transitions to the down
 *     state.
 *  @net: the network device being stopped.
 *
 * This API is used by Linux network stack to notify the network driver that
 * its state was changed to "down"
 * The driver will stop the "send" queue and change its internal
 * state to "Connected".
 */
static int veth_ipa_stop(struct net_device *net)
{
	struct veth_ipa_dev *pdata = netdev_priv(net);
	int ret = 0;

	VETH_IPA_LOG_ENTRY();
	VETH_IPA_LOCK();

	if (pdata->state == VETH_IPA_DOWN) {
		VETH_IPA_ERROR("can't do network interface down without up\n");
		return 0;
	}

	pdata->state = VETH_IPA_DOWN;
	VETH_IPA_DEBUG("veth_ipa_ctx->state = %d\n", pdata->state);
	VETH_IPA_STATE_DEBUG(pdata);
	netif_carrier_off(net);
	VETH_IPA_DEBUG("Carrier off called\n");
	netif_stop_queue(net);
	VETH_IPA_DEBUG("network device stopped\n");

	if (pdata->prv_ipa.ipa_uc_ready) {
		pr_info("%s: veth_disable_ipa_offload\n",
				__func__);
		veth_disable_ipa_offload(pdata);
		ipa_uc_offload_dereg_rdyCB(IPA_UC_NTN);
		pdata->prv_ipa.ipa_uc_ready = false;
		pdata->prv_ipa.ipa_ready = false;
	}

	if (pdata->veth_emac_dev_ready) {
		veth_alloc_emac_dealloc_mem(&(pdata->veth_emac_mem), pdata);
		pdata->veth_emac_dev_ready = false;
	}

	VETH_IPA_UNLOCK();

	//HAB call for BE driver in the mutex lock causes a deadlock
	ret = veth_emac_stop_offload(&(pdata->veth_emac_mem), pdata);
	if (ret < 0) {
		pr_err("%s: failed\n", __func__);
		return ret;
	}

	VETH_IPA_LOG_EXIT();
	return 0;
}

/**
 * VETH_ipa_cleanup() - unregister the network interface driver and free
 *  internal data structs.
 * @priv: same value that was set by VETH_ipa_init(), this
 *   parameter holds the network device pointer.
 *
 * This function shall be called once the network interface is not
 * needed anymore, e.g: when the USB composition does not support VETH.
 * This function shall be called after the pipes were disconnected.
 * Detailed description:
 *  - delete the driver dependency defined for IPA resource manager and
 *   destroy the producer resource.
 *  -  remove the debugfs entries
 *  - deregister the network interface from Linux network stack
 *  - free all internal data structs
 */
void veth_ipa_cleanup(struct veth_ipa_dev *veth_ipa_ctx)
{
	VETH_IPA_LOG_ENTRY();

	if (!veth_ipa_ctx) {
		VETH_IPA_ERROR("veth_ipa_ctx NULL pointer\n");
		return;
	}
	VETH_IPA_STATE_DEBUG(veth_ipa_ctx);

	/*consider destroy rules*/
	veth_ipa_debugfs_destroy(veth_ipa_ctx);

	unregister_netdev(veth_ipa_ctx->net);
	free_netdev(veth_ipa_ctx->net);

	VETH_IPA_INFO("VETH_IPA was destroyed successfully\n");

	VETH_IPA_LOG_EXIT();
}
EXPORT_SYMBOL(veth_ipa_cleanup);

static struct net_device_stats *veth_ipa_get_stats(struct net_device *net)
{
	return &net->stats;
}

#ifdef VETH_PM_ENB
static int resource_request(struct veth_ipa_dev *veth_ipa_ctx)
{
	return ipa_rm_inactivity_timer_request_resource(
		IPA_RM_RESOURCE_ETHERNET_PROD);
}

static void resource_release(struct veth_ipa_dev *veth_ipa_ctx)
{
	ipa_rm_inactivity_timer_release_resource(
		IPA_RM_RESOURCE_ETHERNET_PROD);
}
#endif

static void veth_ipa_tx_timeout(struct net_device *net)
{
	struct veth_ipa_dev *veth_ipa_ctx = netdev_priv(net);

	VETH_IPA_ERROR("possible IPA stall was detected, %d outstanding\n",
		atomic_read(&veth_ipa_ctx->outstanding_pkts));

	net->stats.tx_errors++;
}

static int veth_ipa_debugfs_atomic_open(struct inode *inode, struct file *file)
{
	struct veth_ipa_dev *veth_ipa_ctx = inode->i_private;

	VETH_IPA_LOG_ENTRY();
	file->private_data = &veth_ipa_ctx->outstanding_pkts;

	VETH_IPA_LOG_EXIT();
	return 0;
}

static ssize_t veth_ipa_debugfs_atomic_read
	(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes;
	u8 atomic_str[DEBUGFS_TEMP_BUF_SIZE] = { 0 };
	atomic_t *atomic_var = file->private_data;

	nbytes = scnprintf(atomic_str, sizeof(atomic_str), "%zu\n",
		atomic_read(atomic_var));
	return simple_read_from_buffer(ubuf, count, ppos, atomic_str, nbytes);
}

#ifdef CONFIG_DEBUG_FS

static void veth_ipa_debugfs_init(struct veth_ipa_dev *veth_ipa_ctx)
{
	const mode_t flags_read_write = 0666;
	const mode_t flags_read_only = 0444;
	struct dentry *file;

	VETH_IPA_LOG_ENTRY();

	if (!veth_ipa_ctx)
		return;

	veth_ipa_ctx->directory = debugfs_create_dir("VETH_ipa", NULL);

	if (IS_ERR_OR_NULL(veth_ipa_ctx->directory)) {
		VETH_IPA_ERROR("could not create debugfs directory entry\n");
		goto fail_directory;
	}

	file = debugfs_create_u8("outstanding_high", flags_read_write,
		veth_ipa_ctx->directory,
		&veth_ipa_ctx->outstanding_high);

	if (!file) {
		VETH_IPA_ERROR("could not create outstanding_high file\n");
		goto fail_file;
	}
	file = debugfs_create_u8("outstanding_low", flags_read_write,
		veth_ipa_ctx->directory,
		&veth_ipa_ctx->outstanding_low);
	if (!file) {
		VETH_IPA_ERROR("could not create outstanding_low file\n");
		goto fail_file;
	}

	file = debugfs_create_file("outstanding", flags_read_only,
		veth_ipa_ctx->directory,
		veth_ipa_ctx, &veth_ipa_debugfs_atomic_ops);
	if (!file) {
		VETH_IPA_ERROR("could not create outstanding file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("is_vlan_mode", flags_read_only,
		veth_ipa_ctx->directory,
		&veth_ipa_ctx->is_vlan_mode);
	if (!file) {
		VETH_IPA_ERROR("could not create is_vlan_mode file\n");
		goto fail_file;
	}

	VETH_IPA_DEBUG("debugfs entries were created\n");
	VETH_IPA_LOG_EXIT();

	return;
fail_file:
	debugfs_remove_recursive(veth_ipa_ctx->directory);
fail_directory:
	return;
}

static void veth_ipa_debugfs_destroy(struct veth_ipa_dev *veth_ipa_ctx)
{
	debugfs_remove_recursive(veth_ipa_ctx->directory);
}

#else /* !CONFIG_DEBUG_FS*/

static void veth_ipa_debugfs_init(struct veth_ipa_dev *veth_ipa_ctx) { }

static void veth_ipa_debugfs_destroy(struct veth_ipa_dev *veth_ipa_ctx) { }

#endif /* CONFIG_DEBUG_FS */

/**
 * VETH_ipa_set_device_ethernet_addr() - set device etherenet address
 * @dev_ethaddr: device etherenet address
 *
 * Returns 0 for success, negative otherwise
 */
static int veth_ipa_set_device_ethernet_addr
(u8 *dev_ethaddr, u8 device_ethaddr[])
{
	if (!is_valid_ether_addr(device_ethaddr)) {
		pr_err("%s: invalid ethernet address:\n", __func__);
		return -EINVAL;
	}
	memcpy(dev_ethaddr, device_ethaddr, ETH_ALEN);
	VETH_IPA_DEBUG("device ethernet address: %pM\n", dev_ethaddr);
	pr_debug("device ethernet address: %pM\n", dev_ethaddr);
	return 0;
}

/** veth_ipa_next_state - return the next state of the driver
 * @current_state: the current state of the driver
 * @operation: an enum which represent the operation being made on the driver
 *  by its API.
 *
 * This function implements the driver internal state machine.
 * Its decisions are based on the driver current state and the operation
 * being made.
 * In case the operation is invalid this state machine will return
 * the value VETH_IPA_INVALID to inform the caller for a forbidden sequence.
 */

// static enum veth_ipa_state veth_ipa_next_state
// (
  // enum veth_ipa_state current_state,
  // enum veth_ipa_operation operation
// )
// {
	// int next_state = VETH_IPA_INVALID;

	// switch (current_state) {
	// case VETH_IPA_UNLOADED:
		// if (operation == VETH_IPA_INITIALIZE)
			// next_state = VETH_IPA_INITIALIZED;
		// break;
	// case VETH_IPA_INITIALIZED:
		// if (operation == VETH_IPA_CONNECT)
			// next_state = VETH_IPA_CONNECTED;
		// else if (operation == VETH_IPA_OPEN)
			// next_state = VETH_IPA_UP;
		// else if (operation == VETH_IPA_CLEANUP)
			// next_state = VETH_IPA_UNLOADED;
		// break;
	// case VETH_IPA_CONNECTED:
		// if (operation == VETH_IPA_DISCONNECT)
			// next_state = VETH_IPA_INITIALIZED;
		// else if (operation == VETH_IPA_OPEN)
			// next_state = VETH_IPA_CONNECTED_AND_UP;
		// break;
	// case VETH_IPA_UP:
		// if (operation == VETH_IPA_STOP)
			// next_state = VETH_IPA_INITIALIZED;
		// else if (operation == VETH_IPA_CONNECT)
			// next_state = VETH_IPA_CONNECTED_AND_UP;
		// else if (operation == VETH_IPA_CLEANUP)
			// next_state = VETH_IPA_UNLOADED;
		// break;
	// case VETH_IPA_CONNECTED_AND_UP:
		// if (operation == VETH_IPA_STOP)
			// next_state = VETH_IPA_CONNECTED;
		// else if (operation == VETH_IPA_DISCONNECT)
			// next_state = VETH_IPA_UP;
		// break;
	// default:
		// VETH_IPA_ERROR("State is not supported\n");
		// break;
	// }

	// VETH_IPA_DEBUG
		// ("state transition ( %s -> %s )- %s\n",
		// veth_ipa_state_string(current_state),
		// veth_ipa_state_string(next_state),
		// next_state == VETH_IPA_INVALID ? "Forbidden" : "Allowed");

	// return next_state;
// }

/**
 * VETH_ipa_state_string - return the state string representation
 * @state: enum which describe the state
 */
static const char *veth_ipa_state_string(enum veth_ipa_state state)
{
	switch (state) {
	case VETH_IPA_UNLOADED:
		return "VETH_IPA_UNLOADED";
	case VETH_IPA_INITIALIZED:
		return "VETH_IPA_INITIALIZED";
	case VETH_IPA_CONNECTED:
		return "VETH_IPA_CONNECTED";
	case VETH_IPA_UP:
		return "VETH_IPA_UP";
	case VETH_IPA_CONNECTED_AND_UP:
		return "VETH_IPA_CONNECTED_AND_UP";
	case VETH_IPA_DOWN:
		return "VETH_IPA_DOWN";
	default:
		return "Not supported";
	}
}

/* API to initialize device This function will be called
 * during platform_register_driver() for already existing
 * devices or later if new device get inserted.
 */
static int veth_ipa_probe(struct platform_device *pdev)
{
	/*Initialize and Register VETH iface*/
	veth_ipa_init(pdev);
	pr_debug("veth initialization probe complete\n");
	/*Clean up for init failure.*/
	return 0;
}

static int veth_ipa_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct veth_ipa_dev *pdata = veth_pdata_p;

	ret = veth_ipa_stop(pdata->net);
	if (ret < 0) {
		pr_err("%s: failed\n");
		return ret;
	}
	habmm_socket_close(pdata->veth_emac_mem.vc_id);
	veth_ipa_cleanup(pdata);
	return 0;
}


static int veth_ipa_ap_suspend(struct device *dev)
{
	int    ret = 0;
	struct veth_ipa_dev *pdata = veth_pdata_p;

	pr_info("VETH_IPA suspend init flag check\n");
	if (!veth_ipa_init_flag)
		return 0;

	pr_info("%s: veth_global_pdata->state = %d\n",
			__func__,
			veth_pdata_p->state);
	ret = veth_ipa_stop(pdata->net);
	pr_info("%s: %d\n",
			__func__,
			ret);
	return ret;
}


static int veth_ipa_ap_resume(struct device *dev)
{
	struct veth_ipa_dev *pdata = veth_pdata_p;

	pr_info("%s :\n", __func__);
	pr_info("VETH_IPA resume init flag check\n");

	if (!veth_ipa_init_flag)
		return 0;
	pr_info("%s: veth_global_pdata->state = %d\n",
			__func__,
			veth_pdata_p->state);
	veth_ipa_open_cb(pdata);
	return 0;
}


static const struct of_device_id veth_ipa_dt_match[] = {
	{ .compatible = "qcom,veth-ipa" },
	{ },
};

MODULE_DEVICE_TABLE(of, veth_ipa_dt_match);

static const struct dev_pm_ops veth_ipa_pm_ops = {
	.suspend = veth_ipa_ap_suspend,
	.resume = veth_ipa_ap_resume,
};

static struct platform_driver veth_ipa_driver = {
	.driver = {
		.name = "veth_ipa",
		.of_match_table = veth_ipa_dt_match,
		.pm = &veth_ipa_pm_ops,
	},
	.probe = veth_ipa_probe,
	.remove = veth_ipa_remove,
};

/**
 * VETH_ipa_init_module() - module initialization.
 * This will register driver with subsystem.
 *
 */
static int __init veth_ipa_init_module(void)
{
	pr_debug(" %s: entry\n", __func__);
	/* Initialize logging.*/
	VETH_IPA_LOG_ENTRY();
	ipa_veth_logbuf = ipc_log_context_create(IPA_VETH_IPC_LOG_PAGES,
		"ipa_veth", 0);
	if (ipa_veth_logbuf == NULL)
		VETH_IPA_DEBUG(
			"failed to create log context for VETH_IPA driver\n");
	VETH_IPA_LOG_EXIT();

	pr_debug(" %s exit\n", __func__);
	return platform_driver_register(&veth_ipa_driver);
}

/**
 * VETH_ipa_cleanup_module() - module cleanup
 *
 */
static void veth_ipa_cleanup_module(void)
{
	VETH_IPA_LOG_ENTRY();
	if (ipa_veth_logbuf)
		ipc_log_context_destroy(ipa_veth_logbuf);
	ipa_veth_logbuf = NULL;
	VETH_IPA_LOG_EXIT();
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("VETH IPA network interface");
MODULE_SOFTDEP("pre: ipa_fwmk");

/*Entry point for this module*/
module_init(veth_ipa_init_module);

/*Exit function for this module*/
module_exit(veth_ipa_cleanup_module);


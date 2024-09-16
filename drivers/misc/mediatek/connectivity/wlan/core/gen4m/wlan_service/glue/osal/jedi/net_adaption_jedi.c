/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "rt_config.h"
#include "net_adaption.h"

static s_int32 net_ad_open_inf(struct wifi_dev *wdev);
static s_int32 net_ad_close_inf(struct wifi_dev *wdev);
static s_int32 net_ad_enqueue_mlme_pkt(
	RTMP_ADAPTER *ad,
	void *pkt,
	struct wifi_dev *wdev,
	u_char q_idx,
	boolean is_data_queue);
static s_int32 net_ad_tx_pkt_handle(
	RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	struct _TX_BLK *tx_blk);
static s_int32 net_ad_tx(
	RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	struct _TX_BLK *tx_blk);

static struct test_thread_cb g_test_thread[SERV_THREAD_NUM];
struct test_ant_map test_ant_to_spe_idx_map[] = {
	/* All */
	{0x0, 0},
	{0xF, 0},
	/* 1 Ant */
	{0x1, 0},	/* Tx0 */
	{0x2, 1},	/* Tx1 */
	{0x4, 3},	/* Tx2 */
	{0x8, 9},	/* Tx3 */
	/* 2 Ant */
	{0x3, 0},
	{0x5, 2},
	{0x9, 8},
	{0x6, 4},
	{0xA, 6},
	{0xC, 16},
	/* 3 Ant */
	{0x7, 0},	/* 0_1_2 */
	{0xB, 10},	/* 0_1_3 */
	{0xD, 12},	/* 0_2_3 */
	{0xE, 18},	/* 1_2_3 */
};

/*****************************************************************************
 *	Internal functions
 *****************************************************************************/
static s_int32 net_ad_mps_check_stat(
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_mps_cb *mps_cb;
	struct test_mps_setting *mps_setting;
	u_int32 tx_cnt, txed_cnt;

	mps_cb = &configs->mps_cb;
	mps_setting = mps_cb->mps_setting;
	if (!mps_setting || !mps_cb->mps_cnt) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: mps_cb/mps_setting NULL %p/%p\n",
			__func__, mps_cb, mps_setting));

		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		return ret;
	}

	tx_cnt = configs->tx_stat.tx_cnt;
	txed_cnt = configs->tx_stat.txed_cnt;

	if ((mps_cb->stat & TEST_MPS_ITEM_RUNNING)
		&& (txed_cnt >= tx_cnt)) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: mps_idx finished idx=%d, mps_cnt=%d\n",
			__func__, mps_cb->ref_idx, mps_cb->mps_cnt));
		SERV_OS_SEM_LOCK(&mps_cb->lock);
		mps_cb->stat = 0;
		SERV_OS_SEM_UNLOCK(&mps_cb->lock);

		if (mps_cb->ref_idx > mps_cb->mps_cnt) {
			configs->op_mode &= ~fTEST_MPS;
			mps_cb->setting_inuse = FALSE;
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: mps all finished idx=%d, mps_cnt=%d\n",
				__func__, mps_cb->ref_idx, mps_cb->mps_cnt));

			ret = net_ad_mps_tx_operation(winfos, configs, FALSE);
		}
	}

	return ret;
}

static s_int32 net_ad_mps_dump_setting(
	struct test_configuration *configs,
	u_int16 idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_mps_cb *mps_cb;
	struct test_mps_setting *mps_setting;

	mps_cb = &configs->mps_cb;
	mps_setting = mps_cb->mps_setting;
	if (!mps_setting || !mps_cb->mps_cnt) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: mps_cb/mps_setting NULL %p/%p\n",
			__func__, mps_cb, mps_setting));

		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		return ret;
	}

	if (idx == 0xFFFF) {
		for (idx = 1; idx <= mps_cb->mps_cnt; idx++) {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
				("item_idx=%d, phy_mode=%d, ",
				idx, mps_setting[idx].tx_mode));
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
				("tx_ant=0x%x, mcs=%d, ",
				mps_setting[idx].tx_ant,
				mps_setting[idx].mcs));
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
				("pkt_len=%d, pkt_cnt=%d, ",
				mps_setting[idx].pkt_len,
				mps_setting[idx].pkt_cnt));
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
				("pwr=%d, nss=%d, pkt_bw=%d\n",
				mps_setting[idx].pwr,
				mps_setting[idx].nss,
				mps_setting[idx].pkt_bw));

		}
	} else {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
			("item_idx=%d, phy_mode=%d, ",
			idx, mps_setting[idx].tx_mode));
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
			("tx_ant=0x%x, mcs=%d, ",
			mps_setting[idx].tx_ant,
			mps_setting[idx].mcs));
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
			("pkt_len=%d, pkt_cnt=%d, ",
			mps_setting[idx].pkt_len,
			mps_setting[idx].pkt_cnt));
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
			("pwr=%d, nss=%d, pkt_bw=%d\n",
			mps_setting[idx].pwr,
			mps_setting[idx].nss,
			mps_setting[idx].pkt_bw));
	}

	return ret;
}

static s_int32 net_ad_mps_load_setting(
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_mps_cb *mps_cb;
	struct test_mps_setting *mps_setting;
	u_char *test_pkt;
	u_int32 idx, tx_mode, tx_ant, mcs, pwr;
	u_int32 pkt_len, pkt_cnt, nss, pkt_bw;

	/* TODO: factor out here for tx power */
#if 0
	ATE_TXPOWER TxPower;

	os_zero_mem(&TxPower, sizeof(TxPower));
#endif

	test_pkt = configs->test_pkt;
	mps_cb = &configs->mps_cb;
	mps_setting = mps_cb->mps_setting;
	if (!mps_cb || !mps_setting)
		goto err0;

	SERV_OS_SEM_LOCK(&mps_cb->lock);

	if (mps_cb->stat & TEST_MPS_ITEM_RUNNING)
		goto err1;

	mps_cb->stat |= TEST_MPS_ITEM_RUNNING;
	idx = mps_cb->ref_idx;

	if (idx > mps_cb->mps_cnt)
		goto err2;

	tx_mode = mps_setting[idx].tx_mode;
	tx_ant = mps_setting[idx].tx_ant;
	mcs = mps_setting[idx].mcs;
	pwr = mps_setting[idx].pwr;
	pkt_len = mps_setting[idx].pkt_len;
	pkt_cnt = mps_setting[idx].pkt_cnt;
	nss = mps_setting[idx].nss;
	pkt_bw = mps_setting[idx].pkt_bw;
	configs->tx_mode = tx_mode;
	configs->tx_ant = tx_ant;
	configs->mcs = mcs;
	configs->nss = nss;
	configs->per_pkt_bw = pkt_bw;
	configs->tx_len = pkt_len;
	configs->tx_stat.tx_cnt = pkt_cnt;
	configs->tx_stat.tx_done_cnt = 0;
	configs->tx_stat.txed_cnt = 0;

	/* TODO: factor out here for tx power */
#if 0
	ATECtrl->TxPower0 = pwr;
	TxPower.Power = pwr;
	TxPower.Channel = Channel;
	TxPower.Dbdc_idx = band_idx;
	TxPower.Band_idx = Ch_Band;
	ATECtrl->need_set_pwr = TRUE;
#endif

	SERV_OS_SEM_UNLOCK(&mps_cb->lock);

	/* TODO: factor out here for tx power */
#if 0
	ret = ate_op->SetTxPower0(pAd, TxPower);
#endif

	/* Here means no need to fill packet first time */
	if (mps_cb->ref_idx != 1) {
		ret = net_ad_fill_pkt(winfos, configs, test_pkt,
				configs->tx_len, configs->hdr_len);
		if (ret)
			goto err2;
	}

	ret = net_ad_mps_dump_setting(configs, mps_cb->ref_idx);
	mps_cb->ref_idx++;

	return ret;

err2:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: mps_cb->ref_idx=%d, mps_cnt=%d\n",
		__func__, mps_cb->ref_idx, mps_cb->mps_cnt));
err1:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: item=%d is running\n",
		__func__, mps_cb->ref_idx - 1));
	SERV_OS_SEM_UNLOCK(&mps_cb->lock);
	return ret;
err0:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: mps_cb/mps_setting NULL %p/%p\n",
		__func__, mps_cb, mps_setting));
	return ret;
}

static VOID net_ad_thread_set_service(
	struct test_wlan_info *winfos,
	u_char band_idx, u_int8 *stat)
{
	u_int8 mask = 0;

	if (IS_TEST_DBDC(winfos) && (band_idx == TEST_DBDC_BAND1))
		mask = 1 << TEST_DBDC_BAND1;
	else
		mask = 1 << TEST_DBDC_BAND0;

	*stat |= mask;
}

VOID net_ad_thread_proceed_tx(
	struct test_wlan_info *winfos, u_char band_idx)
{
	u_char thread_idx = SERV_THREAD_TEST;

	RTMP_SEM_LOCK(&g_test_thread[thread_idx].lock);
	net_ad_thread_set_service(winfos,
				  band_idx,
				  &g_test_thread[thread_idx].service_stat);
	RTMP_SEM_UNLOCK(&g_test_thread[thread_idx].lock);
	sys_ad_wakeup_os_task(&g_test_thread[thread_idx].task);
}

VOID net_ad_thread_stop_tx(struct test_wlan_info *winfos)
{
}

static s_int32 net_ad_thread_handler(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	PQUEUE_HEADER mgmt_swq = NULL;
	struct test_configuration *test_config;
	struct test_tx_stack *stack = NULL;
	struct ipg_param *ipg_param;
	struct tx_time_param *tx_time_param;
	struct wifi_dev *wdev;
	u_int32 op_mode, txed_cnt = 0, tx_cnt = 0, pkt_tx_time, ipg;
	s_int32 dequeue_size, multi_users = 0;
	u_short q_idx;
	u_int8 need_ampdu;
	u_char hwq_idx;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (winfos->chip_cap.swq_per_band)
	mgmt_swq = &ad->mgmt_que[band_idx];
	else
		mgmt_swq = &ad->mgmt_que[0];
	test_config = &configs[band_idx];
	stack = &test_config->stack;
	op_mode = test_config->op_mode;
	q_idx = test_config->ac_idx;
	txed_cnt = test_config->tx_stat.txed_cnt;
	tx_cnt = test_config->tx_stat.tx_cnt;
	ipg_param = &test_config->ipg_param;
	tx_time_param = &test_config->tx_time_param;
	pkt_tx_time = tx_time_param->pkt_tx_time;
	need_ampdu = tx_time_param->pkt_need_ampdu;
	ipg = ipg_param->ipg;
	wdev = (struct wifi_dev *)configs->stack.virtual_wtbl[0];
	hwq_idx = hif_get_resource_idx(ad->hdev_ctrl, wdev, TX_MGMT, q_idx);
	dequeue_size = g_test_thread[SERV_THREAD_TEST].deq_cnt;

	do {
		u_long free_num;

test_thread_dequeue:
		free_num = hif_get_tx_resource_free_num(ad->hdev_ctrl, hwq_idx);

#if 0
		if (multi_users > 0) {
			UCHAR *pate_pkt
			= TESTMODE_GET_PARAM(pAd, band_idx, test_pkt);

			ate_ctrl->wcid_ref = multi_users;
			ret = MT_ATEGenPkt(pAd, pate_pkt, band_idx);
		}
#endif

		if (op_mode & OP_MODE_STOP)
			break;

		if (!(op_mode & OP_MODE_TXFRAME))
			break;

		if (!free_num)
			break;

round_tx:
		if (((pkt_tx_time > 0) || (ipg > 0)) &&
			(mgmt_swq->Number >= MGMT_QUE_MAX_NUMS))
			break;

		/* For service thread tx packet counter control */
		if (tx_cnt <= txed_cnt)
			break;

		if ((pkt_tx_time > 0) && need_ampdu)
			q_idx = WMM_AC_BE;
		else
			q_idx = WMM_AC_BK;

		ret = net_ad_enq_pkt(winfos,
				     q_idx,
				     stack->virtual_wtbl[stack->q_idx],
				     stack->virtual_device[stack->q_idx],
				     stack->pkt_skb[stack->q_idx]);
		if (ret)
			break;

		txed_cnt++;
		stack->q_idx++;

		if (stack->q_idx == stack->index)
			stack->q_idx = 0;

		if (((pkt_tx_time > 0) && need_ampdu) || (ipg > 0)) {
			PKT_TOKEN_CB *cb = NULL;
			PKT_TOKEN_QUEUE *que = NULL;
			u_int32 free_token_cnt, pkt_tx_token_id_max;

			cb = hc_get_ct_cb(winfos->hdev_ctrl);
			que = cut_through_get_token_queue_by_band(cb, 0);
			free_token_cnt =
				cut_through_get_free_token_cnt(que);
			pkt_tx_token_id_max = que->pkt_tkid_max;
			free_num = hif_get_tx_resource_free_num(ad->hdev_ctrl,
								hwq_idx);

			if ((free_token_cnt
				> (pkt_tx_token_id_max - TEST_ENQ_PKT_NUM))
				&& (free_num > 0)
				&& (mgmt_swq->Number < MGMT_QUE_MAX_NUMS))
				goto round_tx;
		}

		dequeue_size--;
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: band_idx=%u, tx_cnt=%u, txed_cnt=%u, ",
			__func__, band_idx, tx_cnt, txed_cnt));
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("dequeue=%d, multi_user=%d, free=%lu\n",
			dequeue_size, multi_users, free_num));

		if (!dequeue_size) {
			multi_users--;
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: dequeue %d finish, multi_user=%d\n",
				__func__, dequeue_size, multi_users));
		} else
			goto test_thread_dequeue;
	} while (multi_users > 0);

	test_config->tx_stat.txed_cnt = txed_cnt;
	test_config->tx_stat.tx_cnt = tx_cnt;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: band_idx=%d, tx_cnt=%u, txed_cnt=%u, dequeue=%d\n",
		__func__, band_idx, tx_cnt, txed_cnt, dequeue_size));

	return ret;
}

static s_int32 net_ad_thread_get_band_idx(
	struct test_wlan_info *winfos,
	u_int8 *stat)
{
	u_int8 mask = 0;

	mask = 1 << TEST_DBDC_BAND0;

	if (*stat & mask) {
		*stat &= ~mask;
		return TEST_DBDC_BAND0;
	}

	mask = 1 << TEST_DBDC_BAND1;

	if (IS_TEST_DBDC(winfos) && (*stat & mask)) {
		*stat &= ~mask;
		return TEST_DBDC_BAND1;
	}

	return -1;
}

static s_int32 net_ad_thread(u_long context)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct serv_os_task *task = (SERV_OS_TASK *) context;
	struct test_wlan_info *winfos = NULL;
	struct test_configuration *configs = NULL;
	RTMP_ADAPTER *ad = NULL;
	u_char thread_idx = SERV_THREAD_TEST;
	s_int32 status;
	s_int32 band_idx = 0;
	u_char service_stat = 0;

	if (!task)
		goto err;

	winfos = (struct test_wlan_info *)SERV_OS_TASK_GET_WINFOS(task);
	configs = (struct test_configuration *)SERV_OS_TASK_GET_CONFIGS(task);

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		goto err;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: init thread_idx %u for band %d\n",
		__func__, thread_idx, band_idx));
	SERV_OS_COMPLETE(&g_test_thread[thread_idx].cmd_done);

	while (!SERV_OS_TASK_IS_KILLED(task)) {
		if (sys_ad_wait_os_task(NULL, task, &status) == FALSE) {
			RTMP_SET_FLAG(ad, fRTMP_ADAPTER_HALT_IN_PROGRESS);
			break;
		}

		SERV_OS_SEM_LOCK(&g_test_thread[thread_idx].lock);
		service_stat = g_test_thread[thread_idx].service_stat;

		do {
			if (!service_stat)
				break;

			band_idx = net_ad_thread_get_band_idx(winfos,
								&service_stat);

			if (band_idx == -1)
				break;

			ret = net_ad_thread_handler(
					winfos, configs, (u_char)band_idx);
		} while (1);

		g_test_thread[thread_idx].service_stat = service_stat;
		SERV_OS_SEM_UNLOCK(&g_test_thread[thread_idx].lock);

		if (band_idx == -1)
			goto err;

		if (configs->op_mode & fTEST_MPS) {
			ret = net_ad_mps_check_stat(winfos, configs);
			if (ret)
				break;

			ret = net_ad_mps_load_setting(winfos, configs);
			if (ret)
				break;
		}

		schedule();

		if (ret)
			break;
	}

err:
	if (ad)
		RTMP_CLEAR_FLAG(ad, fRTMP_ADAPTER_HALT_IN_PROGRESS);

	g_test_thread[thread_idx].is_init = FALSE;

	if (ret)
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: abnormal leave err=0x%08x\n", __func__, ret));

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: leave\n", __func__));

	return ret;
}

static s_int32 net_ad_init_payload(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char *packet, u_int32 len)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int32 policy, pl_len, pos;
	u_char *payload;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	policy = configs->fixed_payload;
	payload = configs->payload;
	pl_len = configs->pl_len;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: len=%d, pl_len=%u, policy=%x\n",
		__func__, len, pl_len, policy));

	if (policy == TEST_RANDOM_PAYLOAD) {
		for (pos = 0; pos < len; pos++)
			packet[pos] = RandomByte(ad);

		return ret;
	}

	if (!payload)
		return SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: payload=%x\n", __func__, payload[0]));

	if (pl_len == 0) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: payload length can't be 0!!\n", __func__));

		return SERV_STATUS_OSAL_NET_INVALID_LEN;
	}

	if (policy == TEST_USER_PAYLOAD) {
		sys_ad_zero_mem(packet, len);
		sys_ad_move_mem(packet, payload, pl_len);
	} else if (policy == TEST_FIXED_PAYLOAD) {
		for (pos = 0; pos < len; pos += pl_len)
			sys_ad_move_mem(&packet[pos], payload, pl_len);
	}

	return ret;
}

static s_int32 net_ad_fill_tmac_info(
	RTMP_ADAPTER *ad,
	TMAC_INFO *tmac_info,
	u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct service_test *serv_test;
	struct test_configuration *configs;
	struct tx_time_param *tx_time_param;
	struct wifi_dev *wdev = NULL;
	u_char *addr1 = NULL;
	u_char tx_mode, mcs, vht_nss, wmm_idx;
	u_int32 ant_sel, pkt_tx_time;
	u_int8 need_qos, need_amsdu, need_ampdu;
	boolean fgspe;

	/* Note: shall not use ad here */
	serv_test = (struct service_test *)ad->serv.serv_handle;
	configs = &serv_test->test_config[band_idx];
	wdev = configs->wdev[0];
	tx_time_param = &configs->tx_time_param;
	pkt_tx_time = tx_time_param->pkt_tx_time;
	need_qos = tx_time_param->pkt_need_qos;
	need_amsdu = tx_time_param->pkt_need_amsdu;
	need_ampdu = tx_time_param->pkt_need_ampdu;

	if (!wdev) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: cannot get wdev\n", __func__));

		return SERV_STATUS_OSAL_NET_INVALID_PARAM;
	}

	configs->hdr_len = LENGTH_802_11;
	addr1 = configs->addr1[0];
	ant_sel = configs->tx_ant;
	tx_mode = configs->tx_mode;
	mcs = configs->mcs;
	vht_nss = configs->nss;
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: addr1=%02x:%02x:%02x:%02x:%02x:%02x\n",
		__func__, SERV_PRINT_MAC(addr1)));

	/* Fill TMAC_INFO */
	sys_ad_zero_mem(tmac_info, sizeof(*tmac_info));
	tmac_info->LongFmt = TRUE;

	if (pkt_tx_time > 0) {
		tmac_info->WifiHdrLen = (u_int8)tx_time_param->pkt_hdr_len;
		tmac_info->PktLen = (u_int16)tx_time_param->pkt_msdu_len;
		tmac_info->NeedTrans = FALSE;

		if (need_qos | need_amsdu | need_ampdu) {
			tmac_info->HdrPad = 2;
			tmac_info->BmcPkt = FALSE;
			tmac_info->UserPriority = 0;
		} else {
			tmac_info->HdrPad = 0;
			tmac_info->BmcPkt = IS_BM_MAC_ADDR(addr1);
			tmac_info->UserPriority = 0;
		}
	} else {
		tmac_info->WifiHdrLen = (u_int8)configs->hdr_len;
		tmac_info->HdrPad = 0;
		tmac_info->PktLen = (u_int16)configs->tx_len;
		tmac_info->BmcPkt = IS_BM_MAC_ADDR(addr1);
	}

	/* no ack */
	if ((pkt_tx_time > 0) && (need_ampdu))
		tmac_info->bAckRequired = 1;
	else
		tmac_info->bAckRequired = 0;

	tmac_info->FrmType = FC_TYPE_DATA;
	tmac_info->SubType = SUBTYPE_QDATA;
	tmac_info->OwnMacIdx = wdev->OmacIdx;

	/* no frag */
	tmac_info->FragIdx = 0;

	/* no protection */
	tmac_info->CipherAlg = 0;

	/* tx path setting */
	tmac_info->VhtNss = vht_nss ? vht_nss : 1;
	tmac_info->AntPri = 0;
	tmac_info->SpeEn = 0;

	/* timing measure setting */
	if ((ad->pTmrCtrlStruct != NULL)
		&& (ad->pTmrCtrlStruct->TmrEnable == TMR_INITIATOR))
		tmac_info->TimingMeasure = 1;

	/* band_idx for tx ring choose */
	tmac_info->band_idx = band_idx;

	switch (ant_sel) {
	case 0: /* both */
		tmac_info->AntPri = 0;
		tmac_info->SpeEn = 1;
		break;

	case 1: /* tx0 */
		tmac_info->AntPri = 0;
		tmac_info->SpeEn = 0;
		break;

	case 2: /* tx1 */
		tmac_info->AntPri = 2; /* b'010 */
		tmac_info->SpeEn = 0;
		break;
	}


	/* Need to modify the way of getting wmm_idx */
	wmm_idx = configs->wmm_idx;
	tmac_info->WmmSet = wmm_idx;

	if (ant_sel & TEST_ANT_USER_SEL) {
		ant_sel &= ~TEST_ANT_USER_SEL;
		tmac_info->AntPri = ant_sel;
	} else {
		s_int32 map_idx;

		for (map_idx = 0;
			map_idx < SERV_ARRAY_SIZE(test_ant_to_spe_idx_map);
			map_idx++) {
			if (ant_sel ==
				test_ant_to_spe_idx_map[map_idx].ant_sel)
				break;
		}

		if (map_idx == SERV_ARRAY_SIZE(test_ant_to_spe_idx_map))
			tmac_info->AntPri = 0;
		else
			tmac_info->AntPri
				= test_ant_to_spe_idx_map[map_idx].spe_idx;
	}

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: ant_sel=%x, ant_pri=%x, vht_nss=%x, TxD.VhtNss=%x\n",
		__func__, ant_sel, tmac_info->AntPri,
		vht_nss, tmac_info->VhtNss));

	/* Fill transmit setting */
	tmac_info->TxRadioSet.RateCode = mcs;
	tmac_info->TxRadioSet.PhyMode = tx_mode;
	tmac_info->TxRadioSet.CurrentPerPktBW = configs->per_pkt_bw;
	tmac_info->TxRadioSet.ShortGI =	configs->sgi;
	tmac_info->TxRadioSet.Stbc = configs->stbc;
	tmac_info->TxRadioSet.Ldpc = configs->ldpc;

	tmac_info->QueIdx =
		asic_get_hwq_from_ac(ad, tmac_info->WmmSet, configs->ac_idx);

	if ((pkt_tx_time > 0) && (need_ampdu)) {
		tmac_info->Wcid = configs->wcid_ref;
		tmac_info->FixRate = 0;
		tmac_info->BaDisable = FALSE;
		tmac_info->RemainTxCnt = 1;
	} else {
		tmac_info->Wcid = 0;
		tmac_info->FixRate = 1;
		tmac_info->BaDisable = TRUE;
		tmac_info->RemainTxCnt = 15;
	}

	if (configs->txs_enable) {
		tmac_info->TxS2Host = TRUE;
		tmac_info->TxS2Mcu = FALSE;
		tmac_info->TxSFmt = 1;
	}

	if (tx_mode == TEST_MODE_CCK) {
		tmac_info->TxRadioSet.Premable = LONG_PREAMBLE;

		if (mcs == MCS_9) {
			tmac_info->TxRadioSet.RateCode = 0;
			tmac_info->TxRadioSet.Premable = SHORT_PREAMBLE;
		} else if (mcs == MCS_10) {
			tmac_info->TxRadioSet.RateCode = 1;
			tmac_info->TxRadioSet.Premable = SHORT_PREAMBLE;
		} else if (mcs == MCS_11) {
			tmac_info->TxRadioSet.RateCode = 2;
			tmac_info->TxRadioSet.Premable = SHORT_PREAMBLE;
		}
	}

	tmac_info->Wcid = configs->wcid_ref;
	if (tmac_info->AntPri >= 24)
		fgspe = TRUE;
	else
		fgspe = FALSE;

	if ((pkt_tx_time > 0) && (need_ampdu)) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: tmac_info->Wcid/Wmmset/QueIdx=%d/%d/%d\n",
			__func__, tmac_info->Wcid,
			tmac_info->WmmSet, tmac_info->QueIdx));
	}

	return ret;
}

static s_int32 net_ad_fill_non_offload_tx_blk(
	RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	void *tx_blk)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	PACKET_INFO pkt_info;
	void *packet;
	TX_BLK *txblk = (TX_BLK *)tx_blk;

	packet = txblk->pPacket;
	txblk->Wcid = RTMP_GET_PACKET_WCID(packet);
	RTMP_QueryPacketInfo(
		packet, &pkt_info, &txblk->pSrcBufHeader, &txblk->SrcBufLen);

	TX_BLK_SET_FLAG(txblk, fTX_CT_WithTxD);

	if (RTMP_GET_PACKET_CLEAR_EAP_FRAME(packet))
		TX_BLK_SET_FLAG(txblk, fTX_bClearEAPFrame);

	if (IS_ASIC_CAP(ad, fASIC_CAP_TX_HDR_TRANS)) {
		if ((txblk->TxFrameType == TX_LEGACY_FRAME)
			|| (txblk->TxFrameType == TX_AMSDU_FRAME)
			|| (txblk->TxFrameType == TX_MCAST_FRAME))
			TX_BLK_SET_FLAG(txblk, fTX_HDR_TRANS);
	}

	txblk->pSrcBufData = txblk->pSrcBufHeader;

	return ret;
}

static boolean net_ad_fill_offload_tx_blk(
	RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	void *tx_blk,
	boolean retry)
{
	struct _RTMP_CHIP_CAP *cap = hc_get_chip_cap(ad->hdev_ctrl);
	PACKET_INFO pkt_info;
	PNDIS_PACKET pPacket;
	TX_BLK *txblk = (TX_BLK *)tx_blk;

	pPacket = txblk->pPacket;
	txblk->Wcid = RTMP_GET_PACKET_WCID(pPacket);
	RTMP_QueryPacketInfo(pPacket,
						 &pkt_info,
						 &txblk->pSrcBufHeader,
						 &txblk->SrcBufLen);
	txblk->pSrcBufHeader += cap->TXWISize;
	/* Due to testmode allocate size include TXWISize */
	txblk->SrcBufLen -= cap->TXWISize;

	TX_BLK_SET_FLAG(txblk, fTX_CT_WithTxD);

	if (RTMP_GET_PACKET_CLEAR_EAP_FRAME(pPacket))
		TX_BLK_SET_FLAG(txblk, fTX_bClearEAPFrame);

	/* testmode data does not support fTX_HDR_TRANS yet
	if (IS_ASIC_CAP(pAd, fASIC_CAP_TX_HDR_TRANS)) {
		if ((txblk->TxFrameType == TX_LEGACY_FRAME)
			|| (txblk->TxFrameType == TX_AMSDU_FRAME)
			|| (txblk->TxFrameType == TX_MCAST_FRAME))
			TX_BLK_SET_FLAG(tx_blk, fTX_HDR_TRANS);
	}
	*/

	txblk->pSrcBufData = txblk->pSrcBufHeader;
	txblk->wmm_set = HcGetWmmIdx(ad, wdev);

	if (retry)
		TX_BLK_SET_FLAG(txblk, fTX_bRetryUnlimit);
	else
		TX_BLK_SET_FLAG(txblk, fTX_bNoRetry);

	txblk->UserPriority = 0;

	/*	no frag */
	txblk->FragIdx = 0;
	/* no protection */
	SET_CIPHER_NONE(txblk->CipherAlg);
	return TRUE;
}

/*****************************************************************************
 *	Extern functions
 *****************************************************************************/
struct service_test *net_ad_wrap_service(void *adapter)
{
	struct _RTMP_ADAPTER *ad = (struct _RTMP_ADAPTER *)adapter;
	struct service *serv = &ad->serv;

	return (struct service_test *)serv->serv_handle;
}

struct wifi_dev_ops serv_wdev_ops = {
	.open = net_ad_open_inf,
	.close = net_ad_close_inf,
	.send_mlme_pkt = net_ad_enqueue_mlme_pkt,
	.tx_pkt_handle = net_ad_tx_pkt_handle,
	.ate_tx = net_ad_tx,
	.disconn_act = wifi_sys_disconn_act
};

s_int32 net_ad_open_inf(struct wifi_dev *wdev)
{
	struct _RTMP_ADAPTER *ad = (struct _RTMP_ADAPTER *)wdev->sys_handle;

#ifdef RELEASE_EXCLUDE
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s --->\n", __func__));
#endif /* RELEASE_EXCLUDE */

	if (wifi_sys_open(wdev) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
			("%s: open fail!!!\n", __func__));
		return FALSE;
	}

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: inf up for ra_%x(func_idx) OmacIdx=%d\n",
		__func__, wdev->func_idx, wdev->OmacIdx));

	MlmeRadioOn(ad, wdev);

	wdev->bAllowBeaconing = FALSE;

#ifdef RELEASE_EXCLUDE
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s <---\n", __func__));
#endif /* RELEASE_EXCLUDE */

	return TRUE;
}

s_int32 net_ad_close_inf(struct wifi_dev *wdev)
{
	struct _RTMP_ADAPTER *ad = (struct _RTMP_ADAPTER *)wdev->sys_handle;

#ifdef RELEASE_EXCLUDE
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s --->\n", __func__));
#endif /* RELEASE_EXCLUDE */

	if (ad == NULL)
		return FALSE;

	if (wifi_sys_close(wdev) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: close fail!!!\n", __func__));
		return FALSE;
	}

#ifdef RELEASE_EXCLUDE
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s <---\n", __func__));
#endif /* RELEASE_EXCLUDE */

	return TRUE;
}

s_int32 net_ad_enqueue_mlme_pkt(
	RTMP_ADAPTER *ad,
	void *pkt,
	struct wifi_dev *wdev,
	u_char q_idx,
	boolean is_data_queue)
{
	s_int32 ret;
	struct qm_ops *ops = ad->qm_ops;

	RTMP_SET_PACKET_MGMT_PKT(pkt, 1);

	ret = ops->enq_mgmtq_pkt(ad, wdev, pkt);

	return ret;
}

s_int32 net_ad_tx_pkt_handle(
	RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	struct _TX_BLK *tx_blk)
{
	struct wifi_dev_ops *ops = NULL;
	s_int32 ret = NDIS_STATUS_SUCCESS;

	if (!wdev) {
		RELEASE_NDIS_PACKET(ad, tx_blk->pPacket, NDIS_STATUS_FAILURE);
		return NDIS_STATUS_FAILURE;
	}

	ops = wdev->wdev_ops;
	ret = ops->ate_tx(ad, wdev, tx_blk);

	return ret;
}

s_int32 net_ad_tx(
	RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	struct _TX_BLK *tx_blk)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	TMAC_INFO tmac_info;
	PQUEUE_ENTRY q_entry;
	RTMP_ARCH_OP *arch_ops = hc_get_arch_ops(ad->hdev_ctrl);
	u_int32 band_idx = HcGetBandByWdev(wdev);

	q_entry = RemoveHeadQueue(&tx_blk->TxPacketList);
	tx_blk->pPacket = QUEUE_ENTRY_TO_PACKET(q_entry);
	RTMP_SET_PACKET_WCID(tx_blk->pPacket, 0);

	/* Fill tx blk for test mode */
	ret = net_ad_fill_non_offload_tx_blk(ad, wdev, tx_blk);

	/* TMAC_INFO setup for test mode */
	ret = net_ad_fill_tmac_info(ad, &tmac_info, band_idx);
	if (ret)
		return ret;

	return arch_ops->ate_hw_tx(ad, &tmac_info, tx_blk);
}

s_int32 net_ad_tx_v2(
	struct _RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	struct _TX_BLK *tx_blk)
{
	u_int8 band_idx = 0, need_ampdu = 0, need_amsdu = 0;
	s_int32 ret = 0;
	PQUEUE_ENTRY q_entry;
	RTMP_ARCH_OP *arch_ops = hc_get_arch_ops(ad->hdev_ctrl);
	struct service_test *serv_test = NULL;
	struct test_configuration *configs = NULL;
	struct tx_time_param *tx_time_settings = NULL;

	serv_test = (struct service_test *)ad->serv.serv_handle;
	q_entry = RemoveHeadQueue(&tx_blk->TxPacketList);
	tx_blk->pPacket = QUEUE_ENTRY_TO_PACKET(q_entry);

	band_idx = HcGetBandByWdev(wdev);
	configs = &serv_test->test_config[band_idx];
	tx_time_settings = &configs->tx_time_param;
	need_ampdu = tx_time_settings->pkt_need_ampdu;
	need_amsdu = tx_time_settings->pkt_need_amsdu;

	if (tx_time_settings->pkt_need_qos) {
		tx_blk->wifi_hdr_len = (UINT8) tx_time_settings->pkt_hdr_len;
		tx_blk->MpduHeaderLen = (UINT8) tx_time_settings->pkt_hdr_len;
	} else {
		tx_blk->wifi_hdr_len = (UINT8) LENGTH_802_11;
		tx_blk->MpduHeaderLen = (UINT8) LENGTH_802_11;
	}

	if (need_ampdu) {
		TX_BLK_CLEAR_FLAG(tx_blk, fTX_ForceRate);
		TX_BLK_SET_FLAG(tx_blk, fTX_bAckRequired);
		TX_BLK_SET_FLAG(tx_blk, fTX_bAteAgg);
		tx_blk->HdrPadLen = 2;
	} else {
		TX_BLK_SET_FLAG(tx_blk, fTX_ForceRate);
		TX_BLK_CLEAR_FLAG(tx_blk, fTX_bAckRequired);
		tx_blk->HdrPadLen = 0;
	}

	/* Fill TX blk for ATE mode */
	if (configs->tx_mode == TEST_MODE_HE_MU && configs->retry)
		ret = net_ad_fill_offload_tx_blk(ad, wdev, tx_blk, TRUE);
	else
		ret = net_ad_fill_offload_tx_blk(ad, wdev, tx_blk, FALSE);

	tx_blk->QueIdx = configs->ac_idx;
	if (mt_engine_search_stack(configs,
		   RTMP_GET_PACKET_WCID(tx_blk->pPacket),
		   (void **)&tx_blk->pMacEntry) != SERV_STATUS_SUCCESS) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: wcid:%d is in-valid in stack!\n", __func__,
			RTMP_GET_PACKET_WCID(tx_blk->pPacket)));

		return NDIS_STATUS_FAILURE;
	}
#if defined(CONFIG_AP_SUPPORT)
	if (tx_blk->pMacEntry)
		tx_blk->pMbss = tx_blk->pMacEntry->pMbss;
#endif
	if (configs->txs_enable)
		TX_BLK_SET_FLAG(tx_blk, fTX_bAteTxsRequired);

	return arch_ops->hw_tx(ad, tx_blk);
}

s_int32 net_ad_init_thread(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	enum service_thread_list thread_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	/* Init test_thread_cb */
	if (thread_idx == SERV_THREAD_TEST) {
		g_test_thread[thread_idx].deq_cnt = 1;
		g_test_thread[thread_idx].cmd_expire = RTMPMsecsToJiffies(3000);
		SERV_OS_INIT_COMPLETION(&g_test_thread[thread_idx].cmd_done);

		if (!g_test_thread[thread_idx].is_init) {
			ret = sys_ad_init_os_task(
					&g_test_thread[thread_idx].task,
					"serv_thread_tx",
					(VOID *)winfos,
					(VOID *)configs);
			if (ret)
				goto err;

			NdisAllocateSpinLock(
				ad, &g_test_thread[thread_idx].lock);

			ret = sys_ad_attach_os_task(
				&g_test_thread[thread_idx].task,
				net_ad_thread,
				(ULONG)&g_test_thread[thread_idx].task);

			if (!RTMP_OS_WAIT_FOR_COMPLETION_TIMEOUT
				(&g_test_thread[thread_idx].cmd_done,
				g_test_thread[thread_idx].cmd_expire))
				goto err;

			if (ret)
				goto err;

			g_test_thread[thread_idx].is_init = TRUE;
		}

		g_test_thread[thread_idx].service_stat = 0;
	}

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: initialize thread_idx=%d\n", __func__, thread_idx));

	return ret;

err:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
		("%s: tx thread init fail err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 net_ad_release_thread(
	u_char thread_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	if (&g_test_thread[thread_idx].task)
		ret = sys_ad_kill_os_task(&g_test_thread[thread_idx].task);

	if (ret != SERV_STATUS_SUCCESS)
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			 ("kill test mode tx task failed!\n"));
	else
		g_test_thread[thread_idx].is_init = FALSE;

	NdisFreeSpinLock(&g_test_thread[thread_idx].lock);
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		 ("%s: release thread_idx=%d\n", __func__, thread_idx));

	return ret;
}

s_int32 net_ad_backup_cr(
	struct test_wlan_info *winfos,
	struct test_bk_cr *test_bkcr,
	u_long offset, enum test_bk_cr_type type)
{
	struct test_bk_cr *entry = NULL;
	RTMP_ADAPTER *ad = NULL;
	u_int32 entry_idx;

	if ((type >= SERV_TEST_BKCR_TYPE_NUM)
		|| (type == SERV_TEST_EMPTY_BKCR))
		return SERV_STATUS_OSAL_NET_FAIL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	for (entry_idx = 0; entry_idx < TEST_MAX_BKCR_NUM; entry_idx++) {
		struct test_bk_cr *tmp = &test_bkcr[entry_idx];

		if ((tmp->type == SERV_TEST_EMPTY_BKCR) && (entry == NULL)) {
			entry = tmp;
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: find empty bk entry %d\n",
				__func__, entry_idx));
		} else if ((tmp->type == type) && (tmp->offset == offset)) {
			entry = tmp;
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: update bk entry %d\n",
				__func__, entry_idx));
			break;
		}
	}

	if (!entry)
		return SERV_STATUS_OSAL_NET_FAIL;

	entry->type = type;
	entry->offset = offset;

	switch (type) {
	case SERV_TEST_MAC_BKCR:
		MAC_IO_READ32(ad->hdev_ctrl, offset, &entry->val);
		break;

	case SERV_TEST_HIF_BKCR:
		HIF_IO_READ32(ad->hdev_ctrl, offset, &entry->val);
		break;

	case SERV_TEST_PHY_BKCR:
		PHY_IO_READ32(ad->hdev_ctrl, offset, &entry->val);
		break;

	case SERV_TEST_HW_BKCR:
		HW_IO_READ32(ad->hdev_ctrl, offset, &entry->val);
		break;

	case SERV_TEST_MCU_BKCR:
		MCU_IO_READ32(ad->hdev_ctrl, offset, &entry->val);
		break;

	default:
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_WARN,
			("%s: bk-type not supported\n", __func__));
		entry->type = SERV_TEST_EMPTY_BKCR;
		entry->offset = 0;
		break;
	}

	return SERV_STATUS_SUCCESS;
}

s_int32 net_ad_restore_cr(
	struct test_wlan_info *winfos,
	struct test_bk_cr *test_bkcr,
	u_long offset)
{
	struct test_bk_cr *entry = NULL;
	RTMP_ADAPTER *ad = NULL;
	u_int32 entry_idx;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	for (entry_idx = 0; entry_idx < TEST_MAX_BKCR_NUM; entry_idx++) {
		struct test_bk_cr *tmp = &test_bkcr[entry_idx];

		if (tmp->offset == offset) {
			entry = tmp;
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: find entry %d\n", __func__, entry_idx));
			break;
		}
	}

	if (!entry)
		return SERV_STATUS_OSAL_NET_FAIL;

	switch (entry->type) {
	case SERV_TEST_MAC_BKCR:
		MAC_IO_WRITE32(ad->hdev_ctrl, offset, entry->val);
		break;

	case SERV_TEST_HIF_BKCR:
		HIF_IO_WRITE32(ad->hdev_ctrl, offset, entry->val);
		break;

	case SERV_TEST_PHY_BKCR:
		PHY_IO_WRITE32(ad->hdev_ctrl, offset, entry->val);
		break;

	case SERV_TEST_HW_BKCR:
		HW_IO_WRITE32(ad->hdev_ctrl, offset, entry->val);
		break;

	case SERV_TEST_MCU_BKCR:
		MCU_IO_WRITE32(ad->hdev_ctrl, offset, entry->val);
		break;

	default:
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_WARN,
			("%s: bk-type not supported\n", __func__));
		entry->type = SERV_TEST_EMPTY_BKCR;
		entry->offset = 0;
		break;
	}

	entry->type = SERV_TEST_EMPTY_BKCR;
	entry->offset = 0;
	entry->val = 0;

	return SERV_STATUS_SUCCESS;
}

s_int32 net_ad_cfg_queue(
	struct test_wlan_info *winfos, boolean enable)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
#ifdef CONFIG_AP_SUPPORT
	s_int32 bss_id, max_num_bss;
#endif /* CONFIG_AP_SUPPROT */

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

#ifdef CONFIG_AP_SUPPORT
	max_num_bss = ad->ApCfg.BssidNum;
#endif

	if (enable) {
		/* Start DMA */
		chip_set_hif_dma(ad, DMA_TX_RX, TRUE);

		/* Start to deq sw queue */
		RTMP_CLEAR_FLAG(ad, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);

		/* Start tcp/ip layer queue */
		RTMP_OS_NETDEV_START_QUEUE(ad->net_dev);
#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(ad) {
			if (max_num_bss > MAX_MBSSID_NUM(ad))
				max_num_bss = MAX_MBSSID_NUM(ad);

			for (bss_id = FIRST_MBSSID;
				bss_id < MAX_MBSSID_NUM(ad); bss_id++) {
				if (ad->ApCfg.MBSSID[bss_id].wdev.if_dev)
					RTMP_OS_NETDEV_START_QUEUE(
					ad->ApCfg.MBSSID[bss_id].wdev.if_dev);
			}
		}
#endif /* CONFIG_AP_SUPPROT */
	} else {
		/* Stop tcp/ip layer queue */
		RTMP_OS_NETDEV_STOP_QUEUE(ad->net_dev);
#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(ad) {
			if (max_num_bss > MAX_MBSSID_NUM(ad))
				max_num_bss = MAX_MBSSID_NUM(ad);

			for (bss_id = FIRST_MBSSID;
				bss_id < MAX_MBSSID_NUM(ad); bss_id++) {
				if (ad->ApCfg.MBSSID[bss_id].wdev.if_dev)
					RTMP_OS_NETDEV_STOP_QUEUE(
					ad->ApCfg.MBSSID[bss_id].wdev.if_dev);
			}
		}
#endif /* CONFIG_AP_SUPPROT */
		/* Stop to deq sw queue */
		RTMP_SET_FLAG(ad, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);

		/* Stop DMA */
		chip_set_hif_dma(ad, DMA_TX_RX, FALSE);
	}

	return ret;
}

s_int32 net_ad_startup_ap(
	struct test_wlan_info *winfos)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
#ifdef CONFIG_AP_SUPPORT
	BSS_STRUCT *mbss = NULL;
#endif /* CONFIG_AP_SUPPROT */

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

#ifdef CONFIG_AP_SUPPORT
	mbss = &ad->ApCfg.MBSSID[MAIN_MBSSID];
#endif /* CONFIG_AP_SUPPROT */

	ret = NICInitializeAdapter(ad);
	if (ret != NDIS_STATUS_SUCCESS)
		return SERV_STATUS_OSAL_NET_FAIL;

	RTMPSetTimer(&ad->Mlme.PeriodicTimer, MLME_TASK_EXEC_INTV);

#ifdef CONFIG_AP_SUPPORT
	APStartUp(ad, mbss, AP_BSS_OPER_ALL);
#endif /* CONFIG_AP_SUPPROT */

	/* Start tx path queues */
	ret = net_ad_cfg_queue(winfos, TRUE);

	return ret;
}

s_int32 net_ad_stop_ap(
	struct test_wlan_info *winfos)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	boolean cancelled;
#ifdef CONFIG_AP_SUPPORT
	s_int32 bss_id, max_num_bss;
	BSS_STRUCT *mbss = NULL;
#endif

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

#ifdef CONFIG_AP_SUPPORT
	max_num_bss = ad->ApCfg.BssidNum;
	mbss = &ad->ApCfg.MBSSID[MAIN_MBSSID];
#endif

	/* Stop tx path queues */
	ret = net_ad_cfg_queue(winfos, FALSE);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(ad) {
		if (max_num_bss > MAX_MBSSID_NUM(ad))
			max_num_bss = MAX_MBSSID_NUM(ad);

		/* First IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (bss_id = FIRST_MBSSID;
			bss_id < MAX_MBSSID_NUM(ad); bss_id++) {
			if (ad->ApCfg.MBSSID[bss_id].wdev.if_dev)
				ad->ApCfg.MBSSID[bss_id].wdev.protection = 0;
		}
	}
#endif
	chip_set_hif_dma(ad, DMA_TX_RX, TRUE);
#ifdef CONFIG_AP_SUPPORT
	APStop(ad, mbss, AP_BSS_OPER_ALL);
#endif /* CONFIG_AP_SUPPORT */
	RTMP_CLEAR_FLAG(ad, fRTMP_ADAPTER_HALT_IN_PROGRESS);

	RTMPCancelTimer(&ad->Mlme.PeriodicTimer, &cancelled);
	RTMP_SET_FLAG(ad, fRTMP_ADAPTER_SYSEM_READY);

	return ret;
}

s_int32 net_ad_enter_normal(
	struct test_wlan_info *winfos,
	struct test_backup_params *bak)
{
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	ad->CommonCfg.bEnableTxBurst = bak->en_tx_burst;
	ad->CommonCfg.BeaconPeriod = bak->bcn_prd;
	net_ad_startup_ap(winfos);
	ad->CommonCfg.bBssCoexEnable = bak->en_bss_coex;

	return SERV_STATUS_SUCCESS;
}

s_int32 net_ad_exit_normal(
	struct test_wlan_info *winfos,
	struct test_backup_params *bak)
{
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	net_ad_stop_ap(winfos);

	bak->en_tx_burst = ad->CommonCfg.bEnableTxBurst;
	ad->CommonCfg.bEnableTxBurst = FALSE;
	bak->en_bss_coex = ad->CommonCfg.bBssCoexEnable;
	/* To prevent BSS scan occupy execution time */
	ad->CommonCfg.bBssCoexEnable = FALSE;
	bak->bcn_prd = ad->CommonCfg.BeaconPeriod;
	/* To disable TBTT interrupt */
	ad->CommonCfg.BeaconPeriod = 0;

	return SERV_STATUS_SUCCESS;
}

s_int32 net_ad_update_wdev(
	u_int8 band_idx,
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct wifi_dev *wdev = NULL;
#if defined(DOT11_HE_AX)
	struct wifi_dev *wdev_txd = NULL;
#endif
	RTMP_ADAPTER *ad = NULL;
	u_char ch_band;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	/* To update wdev setting according to ch_band */
#if defined(DOT11_HE_AX)
	wdev_txd = (struct wifi_dev *)configs->wdev[1];

	if (!wdev_txd)
		goto err;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: wdev_idx(txd)=%d, ch=%d\n",
		__func__, wdev_txd->wdev_idx, wdev_txd->channel));

	HcReleaseRadioForWdev(ad, wdev_txd);
#endif /* DOT11_HE_AX */
	wdev = (struct wifi_dev *)configs->wdev[0];

	if (!wdev)
		goto err;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: wdev_idx=%d, ch=%d\n",
		__func__, wdev->wdev_idx, wdev->channel));

	HcReleaseRadioForWdev(ad, wdev);

/* #ifdef DBDC_MODE  */
#if 1
	if (IS_MT7915(ad) && IS_TEST_DBDC(winfos)) {
		u_int8 rx_sel = (configs->rx_ant & 0x3);

		if (band_idx == DBDC_BAND0)
			ad->dbdc_band0_rx_path = rx_sel;
		else
			ad->dbdc_band1_rx_path = rx_sel;
	} else
#endif	/* DBDC_MODE */
	{
		ad->Antenna.field.TxPath = configs->tx_ant;
		ad->Antenna.field.RxPath = configs->rx_ant;
	}

	ch_band = configs->ch_band;
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: ch_band=%d\n", __func__, ch_band));

	if (ch_band == 0)
		wdev->PhyMode = PHYMODE_CAP_24G;
	else
		wdev->PhyMode = PHYMODE_CAP_5G;
#if defined(DOT11_HE_AX)
	if (ch_band == 0)
		wdev_txd->PhyMode = PHYMODE_CAP_24G;
	else
		wdev_txd->PhyMode = PHYMODE_CAP_5G;
#endif
	/*
	 * QA mode used central ch,
	 * thus wdev ch set as qa mode's control ch of relating bw setting
	 */
	wdev->channel = configs->ctrl_ch;
	wlan_config_set_ch_band(wdev, ch_band);
	wlan_config_set_tx_stream(wdev, configs->tx_ant);
	wlan_config_set_rx_stream(wdev, configs->rx_ant);
	wlan_config_set_ht_bw(wdev,
		((configs->bw > TEST_BW_20) ? HT_BW_40 : HT_BW_20));
	wlan_config_set_ext_cha(wdev, configs->ch_offset);
	wlan_config_set_cen_ch_2(wdev, configs->channel_2nd);
	if (configs->bw > TEST_BW_5)
		wlan_config_set_vht_bw(wdev,
				(VHT_BW_80+(configs->bw-TEST_BW_5)));
	else
		wlan_config_set_vht_bw(wdev,
			((configs->bw > TEST_BW_40) ?
			(VHT_BW_80+(configs->bw-BW_80)) : VHT_BW_2040));
#if defined(DOT11_HE_AX)
	if (configs->per_pkt_bw > TEST_BW_80)
		wlan_config_set_ap_bw(wdev, VHT_BW_160);
	else
		wlan_config_set_ap_bw(wdev, configs->per_pkt_bw);
	wlan_config_set_ap_cen(wdev, configs->channel-configs->ch_offset);
#endif /* DOT11_HE_AX */

	if (wdev_attr_update(ad, wdev) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: error to update wdev\n", __func__));
		goto err;
	}

	if (wdev_edca_acquire(ad, wdev) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: error to acquire edca\n", __func__));
		goto err;
	}

#if defined(DOT11_HE_AX)
	wdev_txd->channel = configs->ctrl_ch;

	wlan_config_set_ch_band(wdev_txd, ch_band);
	wlan_config_set_tx_stream(wdev_txd, configs->tx_ant);
	wlan_config_set_rx_stream(wdev_txd, configs->rx_ant);
	wlan_config_set_ht_bw(wdev_txd,
		((configs->bw > TEST_BW_20) ? HT_BW_40 : HT_BW_20));
	wlan_config_set_ext_cha(wdev_txd, configs->ch_offset);
	wlan_config_set_cen_ch_2(wdev_txd, configs->channel_2nd);
	if (configs->bw > TEST_BW_5)
		wlan_config_set_vht_bw(wdev_txd,
			(VHT_BW_80+(configs->bw-TEST_BW_5)));
	else
		wlan_config_set_vht_bw(wdev_txd,
			((configs->bw > TEST_BW_40) ?
			(VHT_BW_80+(configs->bw-BW_80)) : VHT_BW_2040));
	if (configs->per_pkt_bw > TEST_BW_80)
		wlan_config_set_ap_bw(wdev, VHT_BW_160);
	else
		wlan_config_set_ap_bw(wdev_txd, configs->per_pkt_bw);
	wlan_config_set_ap_cen(wdev_txd, configs->channel-configs->ch_offset);

	if (wdev_attr_update(ad, wdev_txd) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: error to update wdev\n", __func__));
		goto err;
	}

	if (wdev_edca_acquire(ad, wdev_txd) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: error to acquire edca\n", __func__));
		goto err;
	}
#endif

	return ret;

err:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
		("%s: updats wdev failed!\n", __func__));

	return SERV_STATUS_OSAL_NET_FAIL_UPDATE_WDEV;
}

s_int32 net_ad_init_wdev(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct wifi_dev *wdev = NULL;
	RTMP_ADAPTER *ad = NULL;
	u_char *own_mac_addr = NULL, *bssid = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	/* To init wdev */
	wdev = (struct wifi_dev *)configs->wdev[0];

	if (wdev_init(ad, wdev, WDEV_TYPE_SERVICE_TXD,
			ad->wdev_list[band_idx]->if_dev,
			band_idx, NULL, (void *)ad) != TRUE)
		goto err;

	if (!wdev)
		return SERV_STATUS_OSAL_NET_INVALID_PARAM;

	if (IS_AXE(ad) || IS_MT7915(ad))
		serv_wdev_ops.ate_tx = net_ad_tx_v2;

	if (wdev_ops_register(wdev, WDEV_TYPE_SERVICE_TXD, &serv_wdev_ops, 0)
		!= TRUE)
		goto err;

	configs->wdev_idx = wdev->wdev_idx;
	wdev->channel = configs->channel;
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: wdev_idx=%d, channel=%d\n",
		__func__, wdev->wdev_idx, wdev->channel));

	if (wdev->channel > 14)
		wdev->PhyMode = PHYMODE_CAP_5G;
	else
		wdev->PhyMode = PHYMODE_CAP_24G;

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(ad)
		own_mac_addr = (u_char *)&configs->addr3[0];

#endif
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(ad)
		own_mac_addr = (u_char *)&configs->addr2[0];
#endif
	sys_ad_move_mem(wdev->if_addr, own_mac_addr, SERV_MAC_ADDR_LEN);

	if (wdev_do_open(wdev) != TRUE)
		goto err;

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(ad)
		bssid = (u_char *)&configs->addr2[0];

#endif
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(ad)
		bssid = (u_char *)&configs->addr1[0];
#endif
	sys_ad_move_mem(wdev->bssid, bssid, SERV_MAC_ADDR_LEN);

	if (wifi_sys_linkup(wdev, NULL) != TRUE)
		goto err;

#if defined(DOT11_HE_AX)
	wdev = (struct wifi_dev *)configs->wdev[1];

	if (wdev_init(ad, wdev, WDEV_TYPE_SERVICE_TXC,
			ad->wdev_list[band_idx]->if_dev,
			band_idx, NULL, (void *)ad) != TRUE)
		goto err;

	if (!wdev)
		return SERV_STATUS_OSAL_NET_INVALID_PARAM;

	if (IS_AXE(ad) || IS_MT7915(ad))
		serv_wdev_ops.ate_tx = net_ad_tx_v2;

	if (wdev_ops_register(wdev, WDEV_TYPE_SERVICE_TXC, &serv_wdev_ops, 0)
		!= TRUE)
		goto err;

	wdev->channel = configs->channel;
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
		("%s: wdev_idx=%d, channel=%d\n",
		__func__, wdev->wdev_idx, wdev->channel));

	if (wdev->channel > 14)
		wdev->PhyMode = PHYMODE_CAP_5G;
	else
		wdev->PhyMode = PHYMODE_CAP_24G;

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(ad)
		own_mac_addr = (u_char *)&configs->addr3[0];

#endif
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(ad)
		own_mac_addr = (u_char *)&configs->addr2[0];
#endif
	own_mac_addr[0] |= 0x2;
	sys_ad_move_mem(wdev->if_addr, own_mac_addr, SERV_MAC_ADDR_LEN);

	if (wdev_do_open(wdev) != TRUE)
		goto err;

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(ad)
		bssid = (u_char *)&configs->addr2[0];

#endif
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(ad)
		bssid = (u_char *)&configs->addr1[0];
#endif
	bssid[0] |= 0x2;
	sys_ad_move_mem(wdev->bssid, bssid, SERV_MAC_ADDR_LEN);

	if (wifi_sys_linkup(wdev, NULL) != TRUE)
		goto err;
#endif	/* DOT11_HE_AX */

	return ret;

err:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
		("%s: inits wdev failed!\n", __func__));

	return SERV_STATUS_OSAL_NET_FAIL_INIT_WDEV;
}

s_int32 net_ad_release_wdev(
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct wifi_dev *wdev;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s\n", __func__));

	/* To release wdev */
	wdev = configs->wdev[0];

	if (!wdev)
		goto err;

	if (wifi_sys_linkdown(wdev) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: linkdown failed!\n", __func__));
		goto err;
	}

	if (wdev_do_close(wdev) != TRUE)
		goto err;

	if (wdev_deinit(ad, wdev) != TRUE)
		goto err;

#if defined(DOT11_HE_AX)
	wdev = configs->wdev[1];

	if (!wdev)
		goto err;

	if (wifi_sys_linkdown(wdev) != TRUE) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: linkdown failed!\n", __func__));
		goto err;
	}

	if (wdev_do_close(wdev) != TRUE)
		goto err;

	if (wdev_deinit(ad, wdev) != TRUE)
		goto err;
#endif /* DOT11_HE_AX */

	return ret;

err:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
		("%s: releases wdev failed!\n", __func__));

	return SERV_STATUS_OSAL_NET_FAIL_RELEASE_WDEV;
}

s_int32 net_ad_alloc_wtbl(
	struct test_wlan_info *winfos,
	u_char *da,
	void *virtual_device,
	void **virtual_wtbl)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("%s: invalid adapter!\n", __func__));
		ret = SERV_STATUS_OSAL_NET_INVALID_PAD;
		goto err_out;
	}

	if (virtual_device == NULL) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("%s: invalid wdev!\n", __func__));
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		goto err_out;
	}

	*virtual_wtbl = (void *)MacTableInsertEntry(ad,
						    da,
						    virtual_device,
						    ENTRY_ATE,
						    OPMODE_ATE,
						    TRUE);

	if (*virtual_wtbl == NULL)
		ret = SERV_STATUS_OSAL_NET_FAIL;

err_out:
	return ret;
}

s_int32 net_ad_free_wtbl(
	struct test_wlan_info *winfos,
	u_char *da,
	void *virtual_wtbl)
{
	RTMP_ADAPTER *ad = NULL;
	struct _MAC_TABLE_ENTRY *mac_tbl_entry = NULL;
	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (virtual_wtbl) {
		mac_tbl_entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;
		MacTableDeleteEntry(ad, mac_tbl_entry->wcid, da);
	} else
		return SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;

	return SERV_STATUS_SUCCESS;
}

s_int32 net_ad_apply_wtbl(
	struct test_wlan_info *winfos,
	void *virtual_dev,
	void *virtual_wtbl)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	struct serv_chip_cap *chip_cap = NULL;
	struct caps_info *cap = NULL;
	struct ampdu_caps *ampdu = NULL;
	u_short bw_winsiz = 0, tid_idx = 0;
	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	chip_cap = &winfos->chip_cap;

	if (virtual_wtbl) {
		struct _MAC_TABLE_ENTRY *entry = NULL;
		struct phy_params *phy_info = NULL;

		entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;
		phy_info = &entry->phy_param;
		cap = &entry->cap;
		ampdu = &entry->cap.ampdu;

		if (phy_info->phy_mode > MODE_VHT)
			entry->MaxHTPhyMode.field.MODE = MODE_VHT;
		else
			entry->MaxHTPhyMode.field.MODE = phy_info->phy_mode;

		if (phy_info->phy_mode > MODE_OFDM) {
			entry->MaxRAmpduFactor = chip_cap->ht_ampdu_exp;
			ampdu->max_ht_ampdu_len_exp = chip_cap->ht_ampdu_exp;
		}
		if (phy_info->phy_mode > MODE_HTGREENFIELD) {
			entry->MaxRAmpduFactor = chip_cap->vht_ampdu_exp;
			ampdu->max_mpdu_len = chip_cap->max_mpdu_len;
			ampdu->max_vht_ampdu_len_exp = chip_cap->vht_ampdu_exp;
		}
#if defined(DOT11_HE_AX)
		if (phy_info->phy_mode > MODE_VHT) {
			cap->modes |= (HE_24G_SUPPORT | HE_5G_SUPPORT);
			cap->he_mac_cap |= HE_AMSDU_IN_ACK_EN_AMPDU;
			ampdu->max_he_ampdu_len_exp = chip_cap->he_ampdu_exp;
		}
#endif
		CLIENT_STATUS_SET_FLAG(entry, fCLIENT_STATUS_WMM_CAPABLE);
		cap->ch_bw.he_ch_width = BW_80;

		if (wifi_sys_conn_act(virtual_dev, virtual_wtbl) != TRUE) {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("%s(): connect action fail!\n", __func__));
		}

#if defined(DOT11_VHT_AC)
		if (IS_HIF_TYPE(ad, HIF_MT))
			RAInit(ad, entry);
#endif

		if (phy_info->phy_mode <= MODE_VHT)
			bw_winsiz = chip_cap->non_he_tx_ba_wsize;
#if defined(DOT11_HE_AX)
		else
			bw_winsiz = chip_cap->he_tx_ba_wsize;
#endif /* DIT11_HE_AX */

		for (tid_idx = 0; tid_idx < 8 ; tid_idx++)
			AsicUpdateBASession(ad,
					    entry->wcid,
					    tid_idx,
					    0,
					    bw_winsiz,
					    TRUE,
					    BA_SESSION_ORI,
					    0);

#ifdef RACTRL_FW_OFFLOAD_SUPPORT
		if (winfos->wm_fw_info.ra_offload == TRUE) {
			CMD_STAREC_AUTO_RATE_UPDATE_T rRaParam;
			RA_PHY_CFG_T *rate_cfg = NULL;
			u_int8 gi_type = phy_info->gi_type;
			u_int8 ltf_type = phy_info->ltf_type;

			entry->bAutoTxRateSwitch = FALSE;
			sys_ad_zero_mem(&rRaParam,
					sizeof(CMD_STAREC_AUTO_RATE_UPDATE_T));
			rate_cfg = &rRaParam.FixedRateCfg;
			rate_cfg->MODE = phy_info->phy_mode;
			if (phy_info->phy_mode == TEST_MODE_HE_MU ||
				phy_info->phy_mode == TEST_MODE_VHT_MIMO) {
				/* work-around to prevent TX CCK
				 * while 5GHz band
				 */
				rate_cfg->MODE = TEST_MODE_OFDM;
				phy_info->rate = 7;
			}
			rate_cfg->STBC = phy_info->stbc;
			if (phy_info->phy_mode < MODE_HE_SU) {
				if (phy_info->gi_type)
					rate_cfg->ShortGI = BIT(phy_info->bw);
				else
					rate_cfg->ShortGI = 0;
			}
#if defined(DOT11_HE_AX)
			else {
				switch (phy_info->bw) {
				case BW_40:
					rate_cfg->ShortGI = (gi_type << 2);
					rate_cfg->he_ltf = (ltf_type << 2);
					break;
				case BW_80:
					rate_cfg->ShortGI = (gi_type << 4);
					rate_cfg->he_ltf = (ltf_type << 4);
					break;
				case BW_160:
					rate_cfg->ShortGI = (gi_type << 6);
					rate_cfg->he_ltf = (ltf_type << 6);
					break;
				default:
					rate_cfg->ShortGI = gi_type;
					rate_cfg->he_ltf = ltf_type;
				}
			}
#endif /* DOT11_HE_AX */
			rate_cfg->BW = phy_info->bw;
			if (phy_info->ldpc) {
				switch (phy_info->phy_mode) {
				case MODE_HTMIX:
				case MODE_HTGREENFIELD:
					rate_cfg->ldpc = 1;
					break;
				case MODE_VHT:
					rate_cfg->ldpc = 2;
					break;
#if defined(DOT11_HE_AX)
				case MODE_HE_SU:
				case MODE_HE_EXT_SU:
				case MODE_HE_TRIG:
				case MODE_HE_MU:
					rate_cfg->ldpc = 4;
					break;
#endif /* DOT11_HE_AX */
				default:/* should not happen */
					rate_cfg->ldpc = 0;
				}
			}
			rate_cfg->MCS = phy_info->rate;
			if (phy_info->dcm)
				rate_cfg->MCS |= BIT(4);
			if (phy_info->su_ext_tone)
				rate_cfg->MCS |= BIT(5);
			rate_cfg->VhtNss = phy_info->vht_nss;
			#if 0 /* ToDO */
			rRaParam.ucShortPreamble =
						TESTMODE_GET_PARAM(pAd,
						HcGetBandByWdev(virtual_dev),
						preamble);
			#endif
			rRaParam.u4Field = RA_PARAM_FIXED_RATE;
			RAParamUpdate(ad, entry, &rRaParam);
		}
#endif
	} else
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;

	return ret;
}

s_int32 net_ad_match_wtbl(
	void *virtual_wtbl,
	u_int16 wcid)
{
	struct _MAC_TABLE_ENTRY *mac_tbl_entry = NULL;
	/* Get adapter from jedi driver first */

	mac_tbl_entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;

	if (mac_tbl_entry->wcid == wcid)
		return TRUE;

	return FALSE;
}

s_int32 net_ad_set_aid(
	void *virtual_wtbl,
	u_int16 aid)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct _MAC_TABLE_ENTRY *entry = NULL;

	entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;

	if (entry)
		entry->Aid = aid;
	else
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;

	return ret;
}

s_int32 net_ad_get_wmm_idx(
	void *virtual_device,
	u_int8 *wmm_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct wifi_dev *wdev = (struct wifi_dev *)virtual_device;

	if (wdev == NULL) {
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		goto err_out;
	}

	*wmm_idx = HcGetWmmIdx(NULL, (struct wifi_dev *)virtual_device);

err_out:
	return ret;
}

s_int32 net_ad_get_band_idx(
	void *virtual_device,
	u_char *band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	if (virtual_device == NULL)
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
	else {
		struct wifi_dev *wdev = (struct wifi_dev *)virtual_device;

		*band_idx = HcGetBandByWdev(wdev);
	}

	return ret;
}

s_int32 net_ad_get_omac_idx(
	struct test_wlan_info *winfos,
	void *virtual_device,
	u_char *omac_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL) {
		ret = SERV_STATUS_OSAL_NET_INVALID_PAD;
		goto err_out;
	}

	if (virtual_device == NULL)
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
	else {
		struct wifi_dev *wdev = (struct wifi_dev *)virtual_device;

		*omac_idx = HcGetOmacIdx(ad, wdev);
	}

err_out:
	return ret;
}

s_int32 net_ad_fill_phy_info(
	void *virtual_wtbl,
	struct test_tx_info *tx_info)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct _MAC_TABLE_ENTRY *entry = NULL;
	struct phy_params *phy_info = NULL;

	entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;
	phy_info = &entry->phy_param;

	sys_ad_zero_mem(phy_info, sizeof(*phy_info));

	phy_info->phy_mode = tx_info->tx_mode;
#ifdef TXBF_SUPPORT
	phy_info->tx_ibf = tx_info->ibf;
	phy_info->tx_ebf = tx_info->ebf;
#endif
	phy_info->stbc = tx_info->stbc;
	phy_info->ldpc = tx_info->ldpc;
	phy_info->bw = tx_info->bw;
	phy_info->vht_nss = tx_info->nss;
	phy_info->gi_type = tx_info->gi;
	phy_info->ltf_type = tx_info->ltf;

#if defined(DOT11_HE_AX)
	if (phy_info->phy_mode > TEST_MODE_VHT) {
		phy_info->rate = (tx_info->mcs & 0xf);
		/* b'5 for DCM */
		phy_info->dcm = (tx_info->mcs & BIT(5)) ? TRUE : FALSE;

		if (phy_info->phy_mode == TEST_MODE_HE_ER) {
			/* b'4 for tone*/
			if (tx_info->mcs & BIT(4))
				phy_info->su_ext_tone = TRUE;
			else
				phy_info->su_ext_tone = FALSE;
		}
	} else
#endif /* DOT11_HE_AX */
	{
		phy_info->rate = (tx_info->mcs & 0x1f);
	}

	return ret;
}

s_int32 net_ad_get_speidx(
	struct test_wlan_info *winfos,
	u_int16 ant_sel,
	u_int8 *spe_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int8 map_idx = 0;
	struct serv_spe_map *spe_map = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL) {
		ret = SERV_STATUS_OSAL_NET_INVALID_PAD;
		goto err_out;
	}

	*spe_idx = 0;
	spe_map = winfos->chip_cap.spe_map_list.spe_map;
	for (map_idx = 0;
		map_idx < winfos->chip_cap.spe_map_list.size;
		map_idx++) {
		if (ant_sel == spe_map[map_idx].ant_sel) {
			*spe_idx = spe_map[map_idx].spe_idx;
			break;
		}
	}
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s spe idx=%d(map_idx:%d/%d)\n",
		__func__, *spe_idx, map_idx,
		winfos->chip_cap.spe_map_list.size));

err_out:
	return ret;
}


s_int32 net_ad_fill_spe_antid(
	struct test_wlan_info *winfos,
	void *virtual_wtbl,
	u_int8 spe_idx,
	u_int8 ant_pri)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	struct _MAC_TABLE_ENTRY *entry = NULL;
	struct phy_params *phy_info = NULL;
#ifdef RACTRL_FW_OFFLOAD_SUPPORT
	CMD_STAREC_AUTO_RATE_UPDATE_T rRaParam;
#endif	/* RACTRL_FW_OFFLOAD_SUPPORT */

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL) {
		ret = SERV_STATUS_OSAL_NET_INVALID_PAD;
		goto err_out;
	}

	if (virtual_wtbl == NULL) {
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		goto err_out;
	}

	entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;
	phy_info = &entry->phy_param;

	phy_info->spe_idx = spe_idx;
	phy_info->ant_pri = ant_pri;

#ifdef RACTRL_FW_OFFLOAD_SUPPORT
	sys_ad_zero_mem(&rRaParam, sizeof(CMD_STAREC_AUTO_RATE_UPDATE_T));

	rRaParam.ucSpeEn = spe_idx;
	rRaParam.u4Field = RA_PARAM_SPE_UPDATE;
	RAParamUpdate(ad, entry, &rRaParam);
#endif	/* RACTRL_FW_OFFLOAD_SUPPORT */

err_out:
	return ret;
}


s_int32 net_ad_fill_pkt(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char *buf, u_int32 txlen, u_int32 hlen)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_char *tmac_info, *pheader, *payload;
	u_char *addr1, *addr2, *addr3, *template;
	u_int8 tx_hw_hdr_len;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	tx_hw_hdr_len = winfos->chip_cap.tx_wi_size;
	addr1 = configs->addr1[0];
	addr2 = configs->addr2[0];
	addr3 = configs->addr3[0];
	template = configs->template_frame;

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s(wcid:%d):: DA: %02x:%02x:%02x:%02x:%02x:%02x\n\t",
		__func__, configs->wcid_ref, SERV_PRINT_MAC(addr1)));
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("SA: %02x:%02x:%02x:%02x:%02x:%02x\n\t",
		SERV_PRINT_MAC(addr2)));
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("BSSID: %02x:%02x:%02x:%02x:%02x:%02x\n",
		SERV_PRINT_MAC(addr3)));

	/* Error check for txlen */
	if (txlen == 0) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: tx length can't be 0!!\n", __func__));

		return SERV_STATUS_OSAL_NET_INVALID_LEN;
	}

	tmac_info = buf;
	pheader = (buf + tx_hw_hdr_len);
	payload = (pheader + hlen);
	sys_ad_zero_mem(buf, TEST_PKT_LEN);

	/* TODO: factor out here for BF */
#if 0
#ifdef TXBF_SUPPORT
	{
		UCHAR iTxBf = TESTMODE_GET_PARAM(pAd, band_idx, ibf);
		UCHAR eTxBf = TESTMODE_GET_PARAM(pAd, band_idx, ebf);

		/* Use wcid 1~4 */
		if (iTxBf || eTxBf) {
			if ((ATECtrl->wcid_ref > ATE_BFMU_NUM)
				|| (ATECtrl->wcid_ref < 1))
				ATECtrl->wcid_ref = ATE_BF_WCID;

			addr1 = ATECtrl->pfmu_info[ATECtrl->wcid_ref - 1].addr;
		}

		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("%s:: DA: %02x:%02x:%02x:%02x:%02x:%02x, wcid=%u\n",
			__func__, PRINT_MAC(addr1), ATECtrl->wcid_ref));
	}
#endif
#endif

	sys_ad_move_mem(pheader, template, hlen);
	sys_ad_move_mem(pheader + 4, addr1, SERV_MAC_ADDR_LEN);
	sys_ad_move_mem(pheader + 10, addr2, SERV_MAC_ADDR_LEN);
	sys_ad_move_mem(pheader + 16, addr3, SERV_MAC_ADDR_LEN);

	ret = net_ad_init_payload(winfos, configs, payload, txlen - hlen);
	if (ret)
		return ret;

	/* TODO: factor out here for log dump */
#if 0
#if !defined(COMPOS_TESTMODE_WIN)

	if (ATECtrl->en_log & fATE_LOG_TXDUMP) {
		INT i = 0;
		PHEADER_802_11 hdr = (HEADER_802_11 *) pheader;

		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("[TXCONTENT DUMP START]\n"));
		asic_dump_tmac_info(pAd, tmac_info);
		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL,
			DBG_LVL_OFF, ("[TXD RAW]: "));

		for (i = 0; i < tx_hw_hdr_len; i++)
			MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
				("%04x", tmac_info[i]));

		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("\nADDR1: %02x:%02x:%02x:%02x:%02x:%02x\n",
			PRINT_MAC(hdr->Addr1)));
		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("ADDR2: %02x:%02x:%02x:%02x:%02x:%02x\n",
			PRINT_MAC(hdr->Addr2)));
		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("ADDR3: %02x:%02x:%02x:%02x:%02x:%02x\n",
			PRINT_MAC(hdr->Addr3)));
		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("FC: %04x\n", *(UINT16 *) (&hdr->FC)));
		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("\tFrom DS: %x\n", hdr->FC.FrDs));
		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("\tTo DS: %x\n", hdr->FC.ToDs));
		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("[CONTENT RAW]: "));

		for (i = 0; i < (txlen - hlen); i++)
			SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
				("%02x", payload[i]));

		SERV_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
			("\n[TXCONTENT DUMP END]\n"));
	}
#endif /* !defined(COMPOS_TESTMODE_WIN) */
#endif

#ifdef RT_BIG_ENDIAN
	RTMPFrameEndianChange(ad, (u_char *) pheader, DIR_WRITE, FALSE);
#ifdef MT_MAC
	if (IS_HIF_TYPE(ad, HIF_MT))
		MTMacInfoEndianChange(ad, tmac_info, TYPE_TMACINFO,
					sizeof(TMAC_TXD_L));
#endif
#endif

	return ret;
}

s_int32 net_ad_alloc_pkt(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_int32 mpdu_length,
	void **pkt_skb)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_char *src_buff;
	u_short qid;
	u_int8 tx_hw_hdr_len;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	src_buff = configs->test_pkt;
	qid = configs->ac_idx;
	tx_hw_hdr_len = winfos->chip_cap.tx_hw_hdr_len;

	if (!src_buff) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: invalid test_pkt\n", __func__));
		goto err_out;
	}

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: test_pkt=%p, ring idx=%u\n", __func__, src_buff, qid));

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: txlen=%d, tx_hw_hdr_len=%d, total=%d\n",
			__func__, mpdu_length, tx_hw_hdr_len,
			mpdu_length + tx_hw_hdr_len));

	ret = RTMPAllocateNdisPacket(ad,
				     pkt_skb,
				     NULL,
				     0,
				     src_buff,
				     mpdu_length + tx_hw_hdr_len);

	if (ret != NDIS_STATUS_SUCCESS) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("%s: AllocateNdisPacket fail\n", __func__));
		goto err_out;
	}

err_out:
	return ret;
}

s_int32 net_ad_free_pkt(
	struct test_wlan_info *winfos,
	void *pkt_skb)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	RTMPFreeNdisPacket(ad, pkt_skb);

	return ret;
}

s_int32 net_ad_enq_pkt(
	struct test_wlan_info *winfos,
	u_short q_idx,
	void *virtual_wtbl,
	void *virtual_device,
	void *pkt_skb)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	PNDIS_PACKET pkt = NULL;
	struct sk_buff *skb = NULL, *skb2 = NULL;
	struct wifi_dev *wdev = (struct wifi_dev *)virtual_device;
	struct _MAC_TABLE_ENTRY *entry = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL) {
		ret = SERV_STATUS_OSAL_NET_INVALID_PAD;
		goto err_out;
	}

	if (virtual_device)
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: wdev_idx=%d, q_idx=%d, pkt_va=%p\n",
			__func__, wdev->wdev_idx, q_idx, pkt_skb));
	else {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: invalid wdev(%p)!\n", __func__, wdev));
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		goto err_out;
	}

	if (virtual_wtbl)
		entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;
	else {
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		goto err_out;
	}

	if (pkt_skb) {
		skb = (struct sk_buff *)pkt_skb;
		SERV_OS_PKT_CLONE(skb, skb2, GFP_ATOMIC);

		if (skb2 == NULL) {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("%s: clone packet fail\n", __func__));
			ret = SERV_STATUS_OSAL_NET_FAIL;
			goto err_out;
		}  else {
			pkt = (PNDIS_PACKET)skb2;
			RTMP_SET_PACKET_WCID(pkt, entry->wcid);
			RTMP_SET_PACKET_WDEV(pkt, wdev->wdev_idx);

			RTMP_SET_PACKET_TXTYPE(pkt, TX_ATE_FRAME);

			if (q_idx > 0) {
				RTMP_SET_PACKET_QUEIDX(pkt, QID_AC_BE);
				RTMP_SET_PACKET_TYPE(pkt, TX_DATA);
			} else {
				RTMP_SET_PACKET_QUEIDX(pkt, 0);
				RTMP_SET_PACKET_TYPE(pkt, TX_MGMT);
			}
		}

		ret = send_mlme_pkt(ad, pkt, wdev, q_idx, FALSE);
	}

	if (ret)
		ret = SERV_STATUS_OSAL_NET_FAIL;

err_out:
	return ret;
}

s_int32 net_ad_trigger_tx(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_int8 band_idx,
	void *pkt)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int32 txdone_cnt = 0, tx_cnt = 0, op_mode = 0;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (band_idx >= TEST_DBDC_BAND_NUM) {
		ret = SERV_STATUS_OSAL_NET_INVALID_BANDIDX;
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: wrong band_idx %u, ret=0x%08x\n",
			__func__, band_idx, ret));
		goto done;
	}

	tx_cnt = configs->tx_stat.tx_cnt;
	op_mode = configs->op_mode;

	/* Do not count in packet number when tx is not in start stage */
	if (!(op_mode & OP_MODE_TXFRAME))
		goto done;

	/* Triggered when RX tasklet free token */
	if (pkt) {
		ad->RalinkCounters.KickTxCount++;
		txdone_cnt++;
	}

	if (configs->tx_strategy == TEST_TX_STRA_THREAD)
		net_ad_thread_proceed_tx(winfos, band_idx);
	else if (configs->tx_strategy == TEST_TX_STRA_TASKLET) {
		if ((op_mode & OP_MODE_TXFRAME) && (txdone_cnt < tx_cnt))
			ret = net_ad_enq_pkt(winfos,
					     configs->ac_idx,
					     configs->stack.virtual_wtbl[0],
					     configs->stack.virtual_device[0],
					     configs->stack.pkt_skb[0]);
		else if ((op_mode & OP_MODE_TXFRAME)
			&& (txdone_cnt == tx_cnt)) {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: all tx is done\n", __func__));

			if (op_mode & fTEST_MPS) {
				SERV_OS_COMPLETION *tx_wait;

				tx_wait = &configs->tx_wait;
				SERV_OS_COMPLETE(tx_wait);
				SERV_LOG(SERV_DBG_CAT_ADAPT,
					SERV_DBG_LVL_TRACE,
					("%s: finish one MPS item\n",
					__func__));
			}

			/* Tx status enters idle mode */
			configs->tx_status = 0;
		} else if (!(op_mode & OP_MODE_TXFRAME)) {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: stop tx bottom is pressed\n",
				__func__));

			if (op_mode & fTEST_MPS) {
				SERV_OS_COMPLETION *tx_wait;

				tx_wait = &configs->tx_wait;
				op_mode &= ~fTEST_MPS;
				configs->op_mode = op_mode;
				SERV_OS_COMPLETE(tx_wait);
				SERV_LOG(SERV_DBG_CAT_ADAPT,
					SERV_DBG_LVL_TRACE,
					("%s: MPS stop\n", __func__));
			}
		} else {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_WARN,
				("%s: do not match any condition, ",
				__func__));
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_WARN,
				("op_mode:0x%x, tx_cnt:%u, txdone_cnt:%u\n",
				op_mode, tx_cnt, txdone_cnt));
		}
	} else {
		ret = SERV_STATUS_OSAL_NET_INVALID_PARAM;
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: wrong tx strategy=%d, ret=0x%08x\n",
			__func__, configs->tx_strategy, ret));
		goto done;
	}

done:
	return ret;
}

s_int32 net_ad_rx_done_handle(
	struct test_wlan_info *winfos,
	void *rx_blk)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	struct service_test *serv_test;
	RX_BLK *rxblk = (RX_BLK *)rx_blk;
	u_char band_idx;
	u_int32 chfreq0 = 0, chfreq1 = 0;
	u_int32 bn0_cr_addr = RMAC_CHFREQ0;
/* #ifdef DBDC_MODE  */
#if 1
	u_int32 bn1_cr_addr = RMAC_CHFREQ1;
#endif /* DBDC_MODE */

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (IS_MT7915(ad))
		band_idx = rxblk->band;
	else {
		MAC_IO_READ32(ad->hdev_ctrl, bn0_cr_addr, &chfreq0);
/* #ifdef DBDC_MODE  */
#if 1
		MAC_IO_READ32(ad->hdev_ctrl, bn1_cr_addr, &chfreq1);
#endif /* DBDC_MODE */

		/* Note: shall not use ad here */
		serv_test = (struct service_test *)ad->serv.serv_handle;

		/* RX packet counter calculate by chfreq of RXD */
		if (rxblk->channel_freq == chfreq0)
			band_idx = TEST_DBDC_BAND0;
/* #ifdef DBDC_MODE  */
#if 1
		else if (rxblk->channel_freq == chfreq1)
			band_idx = TEST_DBDC_BAND1;
#endif /* DBDC_MODE */
		else {
			SERV_LOG(SERV_DBG_CAT_ALL, SERV_DBG_LVL_ERROR,
				("%s: wrong chfreq!!\n"
				 "\tRXD.ch_freq=%u, chfreq0=%u, chfreq1=%u\n",
				__func__, rxblk->channel_freq,
				chfreq0, chfreq1));
			return SERV_STATUS_OSAL_NET_INVALID_PARAM;
		}
	}

	return ret;
}

s_int32 net_ad_set_band_mode(
	struct test_wlan_info *winfos,
	struct test_band_state *band_state)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (band_state->band_mode == TEST_BAND_MODE_SINGLE) {
		if (band_state->band_type == TEST_BAND_TYPE_A)
			Set_WirelessMode_Proc(ad, "14");
		else if (band_state->band_type == TEST_BAND_TYPE_G)
			Set_WirelessMode_Proc(ad, "9");
		else
			ret = SERV_STATUS_OSAL_NET_INVALID_PARAM;
	}

	return ret;
}

s_int32 net_ad_set_txpwr_sku(
	struct test_wlan_info *winfos,
	u_char sku_ctrl, u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	ret = MtCmdTxPowerSKUCtrl(ad, sku_ctrl, band_idx);
	if (ret)
		ret = SERV_STATUS_OSAL_NET_FAIL_SEND_FWCMD;

	return ret;
}

s_int32 net_ad_set_txpwr_power_drop(
	struct test_wlan_info *winfos,
	u_char power_drop, u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	ret = MtCmdTxPowerDropCtrl(ad, power_drop, band_idx);
	if (ret)
		ret = SERV_STATUS_OSAL_NET_FAIL_SEND_FWCMD;

	return ret;
}

s_int32 net_ad_set_txpwr_percentage(
	struct test_wlan_info *winfos,
	u_char percentage_ctrl, u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	ret = MtCmdTxPowerPercentCtrl(ad, percentage_ctrl, band_idx);
	if (ret)
		ret = SERV_STATUS_OSAL_NET_FAIL_SEND_FWCMD;

	return ret;
}

s_int32 net_ad_set_txpwr_backoff(
	struct test_wlan_info *winfos,
	u_char backoff_ctrl, u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	ret = MtCmdTxBfBackoffCtrl(ad, backoff_ctrl, band_idx);
	if (ret)
		ret = SERV_STATUS_OSAL_NET_FAIL_SEND_FWCMD;

	return ret;
}

s_int32 net_ad_init_txpwr(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	if (band_idx >= TEST_DBDC_BAND_NUM)
		return SERV_STATUS_OSAL_NET_INVALID_BANDIDX;

	/* Disable tx power related function for test mode */
	ret = net_ad_set_txpwr_sku(winfos, configs->tx_pwr_sku_en, band_idx);
	if (ret)
		goto error;

	ret = net_ad_set_txpwr_power_drop(
			winfos, configs->tx_pwr_percentage_level, band_idx);
	if (ret)
		goto error;

	ret = net_ad_set_txpwr_percentage(
			winfos, configs->tx_pwr_percentage_en, band_idx);
	if (ret)
		goto error;

	ret = net_ad_set_txpwr_backoff(
			winfos, configs->tx_pwr_backoff_en, band_idx);
	if (ret)
		goto error;

	return ret;

error:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
		("%s: engine init tx power fail, err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 net_ad_handle_mcs32(
	struct test_wlan_info *winfos,
	void *virtual_wtbl, u_int8 bw)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	union WTBL_DW5 wtbl_txcap;
	u_int32 dw_mask = 0;
	struct _MAC_TABLE_ENTRY *entry = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (entry == NULL) {
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		goto err_out;
	} else
		entry = (struct _MAC_TABLE_ENTRY *)virtual_wtbl;

	dw_mask = ~(3 << 12);	/* only update fcap bit[13:12] */
	wtbl_txcap.field.fcap = bw;

	/* WTBLDW5 */
	ret = WtblDwSet(ad, entry->wcid, 1, 5, dw_mask, wtbl_txcap.word);
	if (ret)
		ret = SERV_STATUS_OSAL_NET_FAIL_SEND_FWCMD;

err_out:
	return ret;
}

s_int32 net_ad_cfg_wtbl(
	struct test_wlan_info *winfos,
	struct test_configuration *configs
)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	struct _RTMP_CHIP_CAP *cap = NULL;
	struct _EXT_CMD_ATE_TEST_MODE_T param;
	struct tx_time_param *tx_time_param;
	P_HT_CAP_T   wtbl_ht_cap;
	P_VHT_CAP_T  wtbl_vht_cap;
	P_ANT_CAP_T  wtbl_ant_cap;
	P_BA_CAP_T   wtbl_ba_cap;
	P_RATE_CAP_T wtbl_rate_cap;
	u_int8 need_qos, need_amsdu, need_ampdu;
	u_char tx_mode, mcs, nss, bw, sgi, stbc, ldpc, preamble, u4Stbc;
	u_int32 ant_sel = 0;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	cap = hc_get_chip_cap(ad->hdev_ctrl);
	tx_time_param = &configs->tx_time_param;
	wtbl_ht_cap   = &param.Data.rAteSetAmpduWtbl.rWtblHt;
	wtbl_vht_cap  = &param.Data.rAteSetAmpduWtbl.rWtblVht;
	wtbl_ant_cap  = &param.Data.rAteSetAmpduWtbl.rWtblAnt;
	wtbl_ba_cap   = &param.Data.rAteSetAmpduWtbl.rWtblBa;
	wtbl_rate_cap = &param.Data.rAteSetAmpduWtbl.rWtblRate;

	need_qos = tx_time_param->pkt_need_qos;
	need_amsdu = tx_time_param->pkt_need_amsdu;
	need_ampdu = tx_time_param->pkt_need_ampdu;
	tx_mode = configs->tx_mode;
	mcs = configs->mcs;
	nss = configs->nss;
	bw = configs->bw;
	sgi = configs->sgi;
	stbc = configs->stbc;
	ldpc = configs->ldpc;
	ant_sel = configs->tx_ant;
	preamble = configs->preamble;

	sys_ad_zero_mem(&param, sizeof(param));
	param.ucAteTestModeEn = TRUE;
	param.ucAteIdx = ENUM_ATE_SET_AMPDU_WTBL;

	switch (tx_mode) {
	case TEST_MODE_HTMIX:
	case TEST_MODE_HTGREENFIELD:
		wtbl_ht_cap->fgIsHT = TRUE;
		wtbl_ht_cap->fgLDPC = ldpc;

		if (cap)
			wtbl_ht_cap->ucAmpduFactor
				= winfos->chip_cap.ht_ampdu_exp;
		else
			wtbl_ht_cap->ucAmpduFactor = 3;

		break;

	case TEST_MODE_VHT:
		wtbl_ht_cap->fgIsHT = 1;
		wtbl_vht_cap->fgIsVHT = 1;
		wtbl_vht_cap->fgVhtLDPC = ldpc;

		if (cap)
			wtbl_ht_cap->ucAmpduFactor
				= winfos->chip_cap.vht_ampdu_exp;
		else
			wtbl_ht_cap->ucAmpduFactor = 7;

		break;

	default:
		wtbl_ht_cap->fgIsHT = 0;
		wtbl_vht_cap->fgIsVHT = 0;
		break;
	}

	if (need_ampdu) {
		if (ant_sel & TEST_ANT_USER_SEL) {
			ant_sel &= ~TEST_ANT_USER_SEL;
		} else {
			s_int32 map_idx = 0;
			s_int32 map_idx_len = sizeof(test_ant_to_spe_idx_map)
					/ sizeof(test_ant_to_spe_idx_map[0]);

			for (map_idx = 0; map_idx < map_idx_len; map_idx++) {
				if (ant_sel ==
				test_ant_to_spe_idx_map[map_idx].ant_sel)
					break;
			}
			if (map_idx == map_idx_len)
				ant_sel = 0;
			else
				ant_sel
				= test_ant_to_spe_idx_map[map_idx].spe_idx;
		}

		wtbl_ant_cap->ucSpe = (ant_sel & 0x1F);
		wtbl_ant_cap->AntIDConfig.ucANTIDSts0 = ant_sel;
		wtbl_ant_cap->AntIDConfig.ucANTIDSts1 = ant_sel;
		wtbl_ant_cap->AntIDConfig.ucANTIDSts2 = ant_sel;
		wtbl_ant_cap->AntIDConfig.ucANTIDSts3 = ant_sel;

		wtbl_ba_cap->ucBaEn = 1;
		wtbl_ba_cap->ucBaSize = 7;
		param.Data.rAteSetAmpduWtbl.ucIPsm = 1;
	}

	wtbl_rate_cap->ucFcap = bw;

	if (sgi) {
		switch (bw) {
		case TEST_BW_20:
			wtbl_rate_cap->fgG2 = TRUE;
			break;

		case TEST_BW_40:
			wtbl_rate_cap->fgG4 = TRUE;
			break;

		case TEST_BW_80:
			wtbl_rate_cap->fgG8 = TRUE;
			break;

		case TEST_BW_160C:
			wtbl_rate_cap->fgG16 = TRUE;
			break;

		default:
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_OFF,
				("%s: can't find such bw, use default\n",
				__func__));
			break;
		}
	}

	u4Stbc = raStbcSettingCheck(stbc, tx_mode, mcs, nss, 0, 0);

	wtbl_rate_cap->ucStbc = u4Stbc;
	wtbl_rate_cap->ucMode = tx_mode;
	wtbl_rate_cap->ucSgi = sgi;
	wtbl_rate_cap->ucBw = bw;
	wtbl_rate_cap->ucNss = nss;
	wtbl_rate_cap->ucPreamble = preamble;
	wtbl_rate_cap->ucLdpc = ldpc;
	wtbl_rate_cap->au2RateCode = mcs;

	if (need_qos)
		param.Data.rAteSetAmpduWtbl.ucQos = 1;

#ifdef CONFIG_HW_HAL_OFFLOAD
	ret = MtCmdATETest(ad, &param);
#endif

	return ret;
}

s_int32 net_ad_set_wmm_param_by_qid(
	u_char wmm_idx,
	u_int8 q_idx,
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	struct ipg_param *ipg_param;
	u_int16 slot_time, sifs_time, cw;
	u_int8 ac_num, aifsn;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	ipg_param = &configs->ipg_param;

	if (wmm_idx > 3) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: invalid wmm_idx=%d, ",
			__func__, wmm_idx));
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("reset to 0xff!\n"));
		wmm_idx = 0xFF;
	}

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: wmm_idx=%d\n", __func__, wmm_idx));

	if ((q_idx != QID_AC_BE)
		&& (q_idx != TxQ_IDX_ALTX0)
		&& (q_idx != TxQ_IDX_ALTX1)) {
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
			("%s: impossible!\n", __func__));
		return SERV_STATUS_OSAL_NET_INVALID_PARAM;
	}

	slot_time = ipg_param->slot_time;
	sifs_time = ipg_param->sifs_time;
	ac_num = q_idx;
	aifsn = ipg_param->aifsn;
	cw = ipg_param->cw;
	ret = AsicSetWmmParam(ad, wmm_idx,
				(u_int32) ac_num, WMM_PARAM_AIFSN,
				(u_int32) aifsn);
	if (ret)
		return ret;

	ret = AsicSetWmmParam(ad, wmm_idx,
				(u_int32) ac_num, WMM_PARAM_CWMIN,
				(u_int32) cw);
	if (ret)
		return ret;

	ret = AsicSetWmmParam(ad, wmm_idx,
				(u_int32) ac_num, WMM_PARAM_CWMAX,
				(u_int32) cw);

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: qid=%d, slot_time=%d, sifs_time=%d, ",
		__func__, q_idx, slot_time, sifs_time));
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("ac_num=%d, aifsn=%d, cw=%d\n", ac_num, aifsn, cw));

	return ret;
}

s_int32 net_ad_clean_sta_q(
	struct test_wlan_info *winfos, u_char wcid)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	struct qm_ops *ops = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	ops = ad->qm_ops;

	if (ops->sta_clean_queue) {
		ret = ops->sta_clean_queue(ad, wcid);
		if (ret)
			ret = SERV_STATUS_OSAL_NET_FAIL_SEND_FWCMD;
	}

	return ret;
}

s_int32 net_ad_set_auto_resp(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx, u_char mode)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_char *sa = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

#ifdef CONFIG_AP_SUPPORT
	sa = configs->addr3[0];
#endif
#ifdef CONFIG_STA_SUPPORT
	sa = configs->addr2[0];
#endif

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s\n", __func__));

	if (mode) {
		if (sa)
			sys_ad_move_mem(sa, &configs->own_mac,
					SERV_MAC_ADDR_LEN);

		AsicDevInfoUpdate(ad, 0x0, (u_int8 *)&configs->own_mac,
				band_idx, TRUE, DEVINFO_ACTIVE_FEATURE);
	} else {
		AsicDevInfoUpdate(ad, 0x0, (u_int8 *)ad->CurrentAddress,
				band_idx, TRUE, DEVINFO_ACTIVE_FEATURE);
	}

	return ret;
}

s_int32 net_ad_set_low_power(
	struct test_wlan_info *winfos, u_int32 control)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (control)
		MlmeLpEnter(ad);
	else
		MlmeLpExit(ad);

	return ret;
}

s_int32 net_ad_read_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	RTMP_IO_READ32(ad->hdev_ctrl, regs->cr_addr, regs->cr_val);

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: cr_addr=0x%08x, cr_val=0x%08x\n",
		__func__, regs->cr_addr, *regs->cr_val));

	return ret;
}

s_int32 net_ad_write_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	RTMP_IO_WRITE32(ad->hdev_ctrl, regs->cr_addr, *regs->cr_val);

	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
		("%s: cr_addr=0x%08x, cr_val=0x%08x\n",
		__func__, regs->cr_addr, *regs->cr_val));

	return ret;
}

s_int32 net_ad_read_bulk_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	struct test_register *regs)
{
#define REG_BLOCK_SIZE 128
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int32 reg_seq, addr, reg_total, value;
	u_char offset_byte = 0x4;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	addr = regs->cr_addr;
	reg_total = regs->cr_num;

	for (reg_seq = 0; reg_seq < reg_total; reg_seq++) {
		RTMP_IO_READ32(ad->hdev_ctrl,
				addr,
				&value);

		sys_ad_move_mem(regs->cr_val+reg_seq,
				&value, sizeof(value));
		addr += offset_byte;
	}

	return ret;
}

s_int32 net_ad_read_bulk_rf_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int32 idx, addr, value;
	u_int32 *dst;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	for (idx = 0; idx < regs->cr_num; idx++) {
		addr = regs->cr_addr + (idx << 2);
		dst = regs->cr_val + idx;
		ret = MtCmdRFRegAccessRead(ad, regs->wf_sel, addr, &value);
		if (ret) {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("wf_sel=%d, cr_addr=0x%08x, ",
				regs->wf_sel, addr));
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("cr_val=0x%08x fail\n", value));
			break;
		}

		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: wf_sel=%d, cr_addr=0x%08x, cr_val=0x%08x\n",
			__func__, regs->wf_sel, addr, value));

		sys_ad_move_mem(dst, &value, sizeof(value));
	}

	return ret;
}

s_int32 net_ad_write_bulk_rf_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int32 idx, addr, value;
	u_int32 *src;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	for (idx = 0; idx < regs->cr_num; idx++) {
		addr = regs->cr_addr + (idx << 2);
		src = regs->cr_val + idx;
		sys_ad_move_mem(&value, src, sizeof(value));

		ret = MtCmdRFRegAccessWrite(ad, regs->wf_sel, addr, value);
		if (ret) {
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("wf_sel=%d, cr_addr=0x%08x, ",
				regs->wf_sel, addr));
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
				("cr_val=0x%08x fail\n", value));
			break;
		}

		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: wf_sel=%d, cr_addr=0x%08x, cr_val=0x%08x\n",
			__func__, regs->wf_sel, addr, value));
	}

	return ret;
}

void net_ad_read_ca53_reg(struct test_register *regs)
{
	u_long offset;

	regs->cr_addr = (u_long)ioremap(regs->cr_addr, CA53_GPIO_REMAP_SIZE);
	sys_ad_move_mem((u_char *)&offset, (u_char *)&regs->cr_addr,
			sizeof(u_long));
	RTMP_SYS_IO_READ32(offset, regs->cr_val);
	iounmap((void *)offset);
}

void net_ad_write_ca53_reg(struct test_register *regs)
{
	u_long offset;

	regs->cr_addr = (u_long)ioremap(regs->cr_addr, CA53_GPIO_REMAP_SIZE);
	sys_ad_move_mem((u_char *)&offset, (u_char *)&regs->cr_addr,
			sizeof(u_long));
	RTMP_SYS_IO_WRITE32(offset, *regs->cr_val);
	iounmap((void *)offset);
}

s_int32 net_ad_read_write_eeprom(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms,
	boolean is_read)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int16 value;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (is_read) {
		RT28xx_EEPROM_READ16(ad, eprms->offset, value);
		sys_ad_move_mem(eprms->value, &value, sizeof(value));
	} else
		RT28xx_EEPROM_WRITE16(ad, eprms->offset, *eprms->value);

	return ret;
}

s_int32 net_ad_read_write_bulk_eeprom(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms,
	boolean is_read)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	u_int16 offset, length, value, eeprom_size = 0;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	eeprom_size = winfos->chip_cap.efuse_size;
	if (is_read) {
		RTMP_OS_NETDEV_STOP_QUEUE(ad->net_dev);
		for (offset = 0 ; offset < (eeprom_size >> 1) ; offset++) {
			RT28xx_EEPROM_READ16(ad, (offset << 1), value);
			eprms->value[offset] = value;
		}

		RTMP_OS_NETDEV_START_QUEUE(ad->net_dev);
	} else {
		offset = eprms->offset;
		length = eprms->length;

#if defined(RTMP_FLASH_SUPPORT)

		if (length == 16)
			sys_ad_move_mem(ad->EEPROMImage + offset,
					eprms->value + offset, length);

		else if (length == eeprom_size)
			rtmp_ee_flash_write_all(ad);

		if (length != 16)
#endif /* RTMP_FLASH_SUPPORT */
		{
			if ((offset + length) <= eeprom_size) {
				u_int16 val_seq = 0;

				for (val_seq = 0;
					val_seq < (length >> 1);
					val_seq++) {
					value = eprms->value[offset >> 1];
					RT28xx_EEPROM_WRITE16(ad,
								offset,
								value);
				}
			} else {
				SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
					("%s: exceed eeprom size(%d), ",
					__func__, EEPROM_SIZE));
				SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
					("offset=0x%08x, length=%d, ",
					offset, length));
				SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
					("offset+length=0x%08x\n",
					offset+length));
			}
		}
	}

	return ret;
}

s_int32 net_ad_get_free_efuse_block(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	eFuseGetFreeBlockCount(ad, &eprms->efuse_free_block);

	return ret;
}

s_int32 net_ad_mps_tx_operation(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	boolean is_start_tx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_mps_cb *mps_cb;
	struct test_mps_setting *mps_setting;

	mps_cb = &configs->mps_cb;
	mps_setting = mps_cb->mps_setting;
	if (!mps_setting || !mps_cb->mps_cnt) {
		ret = SERV_STATUS_OSAL_NET_INVALID_NULL_POINTER;
		goto err;
	}

	if (is_start_tx) {
		if (configs->op_mode & OP_MODE_MPS) {
			ret = SERV_STATUS_OSAL_NET_FAIL;
			goto err;
		}

		if (mps_cb->setting_inuse) {
			ret = SERV_STATUS_OSAL_NET_FAIL;
			goto err;
		}

		configs->op_mode |= fTEST_MPS;
		mps_cb->ref_idx = 1;
		mps_cb->setting_inuse = TRUE;
		ret = net_ad_mps_load_setting(winfos, configs);
		if (ret)
			goto err;

		ret = net_ad_mps_dump_setting(configs, 0xFFFF);
	} else {
		configs->op_mode &= ~OP_MODE_TXFRAME;
		SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
			("%s: op_mode=0x%x, inuse=0x%x, setting_addr=%p\n",
			__func__, configs->op_mode, mps_cb->setting_inuse,
			mps_setting));

		if (!(configs->op_mode & OP_MODE_MPS)
			&& !mps_cb->setting_inuse) {
			struct test_mps_setting **setting_addr =
						&(mps_cb->mps_setting);
			mps_cb->mps_cnt = 0;
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: before free mem=%p\n",
				__func__, mps_setting));
			sys_ad_free_mem(*setting_addr);
			SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_TRACE,
				("%s: after free mem=%p\n",
				__func__, mps_setting));
			*setting_addr = NULL;
		}
	}

	return ret;
err:
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
		("%s: error, mps_cnt=%d, mps_setting=%p\n",
		__func__, mps_cb->mps_cnt, mps_setting));
	SERV_LOG(SERV_DBG_CAT_ADAPT, SERV_DBG_LVL_ERROR,
		("%s: error, op_mode=0x%x, setting_inuse=0x%x\n",
		__func__, configs->op_mode, mps_cb->setting_inuse));

	return ret;
}

s_int32 net_ad_set_tmr(
	struct test_wlan_info *winfos,
	struct test_tmr_info *tmr_info)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	s_char tmr_setting[8], tmr_hw_version[8];

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	if (tmr_info->version == TMR_HW_VER_100)
		tmr_info->version = TMR_VER_1_0;
	else if (tmr_info->version == TMR_HW_VER_150)
		tmr_info->version = TMR_VER_1_5;
	else if (tmr_info->version == TMR_HW_VER_200)
		tmr_info->version = TMR_VER_2_0;
	else {
		SERV_LOG(SERV_DBG_CAT_ALL, SERV_DBG_LVL_ERROR,
			("%s: wrong version %d!!\n",
			__func__, tmr_info->version));
		return SERV_STATUS_OSAL_NET_INVALID_PARAM;
	}

	sprintf(tmr_setting, "%d", tmr_info->setting);
	sprintf(tmr_hw_version, "%d", tmr_info->version);

	ret = TmrUpdateParameter(ad, tmr_info->through_hold, tmr_info->iter);
	if (ret)
		return SERV_STATUS_OSAL_NET_FAIL;
	ret = setTmrVerProc(ad, tmr_hw_version);
	if (ret)
		return SERV_STATUS_OSAL_NET_FAIL;
	ret = setTmrEnableProc(ad, tmr_setting);

	return ret;
}

s_int32 net_ad_get_rxv_stat(
	struct test_wlan_info *winfos,
	u_char ctrl_band_idx,
	struct test_rx_stat *rx_stat)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;
	RX_STATISTIC_RXV *rxv_stat;
	u_char ant_idx = 0, user_idx = 0;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	rxv_stat = ad->rx_stat_rxv + ctrl_band_idx;

	/* update rx stat info (per antenna path) */
	for (ant_idx = 0; ant_idx < TEST_ANT_NUM; ant_idx++) {
		rx_stat->rx_st_path[ant_idx].fagc_ib_rssi =
			rxv_stat->FAGC_RSSI_IB[ant_idx];
		rx_stat->rx_st_path[ant_idx].fagc_wb_rssi =
			rxv_stat->FAGC_RSSI_WB[ant_idx];
		rx_stat->rx_st_path[ant_idx].rcpi =
			rxv_stat->RCPI[ant_idx];
		rx_stat->rx_st_path[ant_idx].rssi =
			rxv_stat->RSSI[ant_idx];
	}

	/* update rx stat info (per user) */
	for (user_idx = 0; user_idx < TEST_USER_NUM; user_idx++) {
		rx_stat->rx_st_user[user_idx].freq_offset_from_rx =
			rxv_stat->FreqOffsetFromRx[user_idx];
		rx_stat->rx_st_user[user_idx].snr =
			(u_int32)rxv_stat->SNR[user_idx];
		rx_stat->rx_st_user[user_idx].fcs_error_cnt =
			rxv_stat->fcs_error_cnt[user_idx];
	}

	return ret;
}

s_int32 net_ad_get_rxv_cnt(
	struct test_wlan_info *winfos,
	u_char ctrl_band_idx,
	u_int32 *byte_cnt)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	chip_get_rxv_cnt(ad, ctrl_band_idx, byte_cnt);

	return ret;
}

s_int32 net_ad_get_rxv_content(
	struct test_wlan_info *winfos,
	u_char ctrl_band_idx,
	void *content)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	RTMP_ADAPTER *ad = NULL;

	/* Get adapter from jedi driver first */
	GET_PAD_FROM_NET_DEV(ad, winfos->net_dev);
	if (ad == NULL)
		return SERV_STATUS_OSAL_NET_INVALID_PAD;

	chip_get_rxv_content(ad, ctrl_band_idx, content);

	return ret;
}

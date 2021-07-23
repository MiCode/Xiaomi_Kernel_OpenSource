/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/delay.h>

#include "ccu_ipc.h"
#include "ccu_cmn.h"
#include "ccu_reg.h"
#include "ccu_platform_def.h"
static DEFINE_MUTEX(ccu_ipc_mutex);

#define CCU_IPC_NO_ACK 0x66669999
#define CCU_IPC_CMD_TIMEOUT_SPEC 3000 //3000us = 3ms

struct CCU_IPC_DATA {
	MBOOL m_initialized;
	uint32_t *m_ccuIntTriggerPtr;
	struct ap2ccu_ipc_t *m_ccuIpcPtr;
	void *m_ipcInDataPtr;
	void *m_ipcOutDataPtr;
	uint32_t m_ipcInDataAddrCcu;
	uint32_t m_ipcOutDataAddrCcu;
	uint64_t ccu_base;
};


static struct CCU_IPC_DATA ccu_ipc;

void ccu_ipc_init(unsigned int *ccuDmBase, unsigned int *ccuCtrlBase)
{
	struct shared_buf_map *sb_map_ptr =
	(struct shared_buf_map *)(ccuDmBase + OFFSET_CCU_SHARED_BUF_MAP_BASE);

	ccu_ipc.m_ccuIntTriggerPtr =
	(uint32_t *)ccuCtrlBase + OFFSET_CCU_INT_TRG;
	ccu_ipc.m_ccuIpcPtr =
	(struct ap2ccu_ipc_t *)(((uint8_t *)ccuDmBase) +
	sb_map_ptr->ipc_base_offset);
	ccu_ipc.m_ipcInDataPtr =
	(void *)(((uint8_t *)ccuDmBase) + sb_map_ptr->ipc_in_data_base_offset);
	ccu_ipc.m_ipcOutDataPtr =
	(void *)(((uint8_t *)ccuDmBase) + sb_map_ptr->ipc_out_data_base_offset);
	ccu_ipc.m_ipcInDataAddrCcu = sb_map_ptr->ipc_in_data_addr_ccu;
	ccu_ipc.m_ipcOutDataAddrCcu = sb_map_ptr->ipc_out_data_addr_ccu;

	ccu_ipc.m_initialized = MTRUE;
	ccu_ipc.ccu_base = (unsigned long)ccuCtrlBase;

	LOG_DBG("ccuDmBase: %p", ccuDmBase);
	LOG_DBG("ccuCtrlBase: %p", ccuCtrlBase);
	LOG_DBG("m_ccuIntTriggerPtr: %p", ccu_ipc.m_ccuIntTriggerPtr);
	LOG_DBG("m_ccuIpcPtr: %p", ccu_ipc.m_ccuIpcPtr);
	LOG_DBG("m_ipcInDataPtr: %p", ccu_ipc.m_ipcInDataPtr);
	LOG_DBG("m_ipcOutDataPtr: %p", ccu_ipc.m_ipcOutDataPtr);

}

bool ccu_ipc_getIObuffer(void **ipcInDataPtr, void **ipcOutDataPtr,
	uint32_t *ipcInDataAddrCcu, uint32_t *ipcOutDataAddrCcu)
{
	LOG_DBG("+");

	if (ccu_ipc.m_initialized == MFALSE) {
		LOG_ERR("not initialized, invalid operation.");
		return MFALSE;
	}

	*ipcInDataPtr = ccu_ipc.m_ipcInDataPtr;
	*ipcOutDataPtr = ccu_ipc.m_ipcOutDataPtr;
	*ipcInDataAddrCcu = ccu_ipc.m_ipcInDataAddrCcu;
	*ipcOutDataAddrCcu = ccu_ipc.m_ipcOutDataAddrCcu;
	LOG_DBG("*ipcInDataPtr: %p", *ipcInDataPtr);
	LOG_DBG("*ipcOutDataPtr: %p", *ipcOutDataPtr);
	LOG_DBG("*ipcInDataAddrCcu: %p", *ipcInDataAddrCcu);
	LOG_DBG("*ipcOutDataAddrCcu: %p", *ipcOutDataAddrCcu);

	LOG_DBG("-");

	return MTRUE;
}

int ccu_ipc_send(struct ccu_msg *msg)
{
	uint32_t loopCount = 0;
	bool ackValue = 0;
	bool timeout = false;
	uint32_t write_cnt = 0;
	uint32_t read_cnt = 0;

	LOG_DBG("+, ftype(%d), msg_id(%d)", msg->feature_type, msg->msg_id);

	if (ccu_ipc.m_initialized == MFALSE) {
		LOG_ERR("not initialized, invalid operation.");
		return -1;
	}

	LOG_DBG("+, read_cnt1,%x", &ccu_ipc.m_ccuIpcPtr);
	//since ipc is synchronized, check no previous ipc
	write_cnt = readl(&(ccu_ipc.m_ccuIpcPtr->write_cnt));
	read_cnt = readl(&(ccu_ipc.m_ccuIpcPtr->read_cnt));
	if (read_cnt != write_cnt) {
		LOG_ERR("CCU IPC synchronization violation, rcnt:%d, wcnt:%d",
			read_cnt, write_cnt);
		return -1;
	}

	LOG_DBG("+, read_cnt2");
	writel(CCU_IPC_NO_ACK, &ccu_ipc.m_ccuIpcPtr->ack);
	write_cnt = readl(&ccu_ipc.m_ccuIpcPtr->write_cnt);
	writel(write_cnt + 1, &ccu_ipc.m_ccuIpcPtr->write_cnt);
	LOG_DBG("+set msg");

	writel(msg->feature_type, &ccu_ipc.m_ccuIpcPtr->msg.feature_type);
	writel(msg->msg_id, &ccu_ipc.m_ccuIpcPtr->msg.msg_id);
	writel(msg->in_data_ptr, &ccu_ipc.m_ccuIpcPtr->msg.in_data_ptr);
	writel(msg->out_data_ptr, &ccu_ipc.m_ccuIpcPtr->msg.out_data_ptr);
	writel(msg->tg_info, &ccu_ipc.m_ccuIpcPtr->msg.tg_info);
	writel(msg->sensor_idx, &ccu_ipc.m_ccuIpcPtr->msg.sensor_idx);

	LOG_DBG("+, CCU IPC TRIG");
	ccu_write_reg_bit(ccu_ipc.ccu_base, EXT2CCU_INT_CCU,
		EXT2CCU_INT_CCU, 1);
	while (readl(&ccu_ipc.m_ccuIpcPtr->ack) == CCU_IPC_NO_ACK) {
		loopCount++;
		udelay(100);
		if (loopCount > CCU_IPC_CMD_TIMEOUT_SPEC) {
			ackValue = readl(&ccu_ipc.m_ccuIpcPtr->ack);
			if (ackValue == CCU_IPC_NO_ACK)
				timeout = true;
			else
				LOG_DBG_MUST("CCU cmd timeout false alarm");
			break;
		}
	}

	if (timeout) {
		LOG_ERR("CCU cmd timeout");
		LOG_ERR("ftype(%d), msg_id(%d), ackValue(%d), loopCount(%d)",
			msg->feature_type, msg->msg_id, ackValue, loopCount);
		return -1;
	}

	LOG_DBG("-, ftype(%d), msg_id(%d), loopCount(%d)",
		msg->feature_type, msg->msg_id, loopCount);
	return 0;
}

bool _copyCmdInData(struct CcuIOBufInfo *inDataBufInfo,
	void *inDataPtr, uint32_t inDataSize)
{
	uint32_t i;

	LOG_DBG("+:%s\n", __func__s);
	if (inDataBufInfo->bufType == CCU_SRAM_IO) {
		// copy to sram
		uint32_t *realIoBuf = (uint32_t *)inDataBufInfo->addr_ap;
		//copy from _interIBuf to real IO buffer in unit of 4bytes,
		//to avoid APB bus constraint of non-4byte-aligned data access
		for (i = 0; i < (inDataSize/4); i++) {
			// realIoBuf[i] = ((uint32_t *)inDataPtr)[i];
			writel(((uint32_t *)inDataPtr)[i], &realIoBuf[i]);
		}
	}

	LOG_DBG("-:%s\n", __func__);

	return true;
}

bool _copyCmdOutData(struct CcuIOBufInfo *outDataBufInfo,
	void *outDataPtr, uint32_t outDataSize)
{
	uint32_t i;

	LOG_DBG("+:%s\n", __func__);
	if (outDataBufInfo->bufType == CCU_SRAM_IO) {
		uint32_t *realIoBuf = (uint32_t *)outDataBufInfo->addr_ap;

		//copy from _interIBuf to real IO buffer in unit of 4bytes,
		//to avoid APB bus constraint of non-4byte-aligned data access
		for (i = 0; i < (outDataSize/4); i++) {
			// ((uint32_t *)outDataPtr)[i] = realIoBuf[i];
			writel(realIoBuf[i], &((uint32_t *)outDataPtr)[i]);
		}
	}

	LOG_DBG("-:%s\n", __func__);

	return MTRUE;
}

bool _resolveCmdIOBuf(struct CcuIOBufInfo *inDataBufInfo,
	struct CcuIOBufInfo *outDataBufInfo,
	uint32_t inDataSize, uint32_t outDataSize)
{
	ccu_ipc_getIObuffer((void **)&inDataBufInfo->addr_ap,
		(void **)&outDataBufInfo->addr_ap,
		&inDataBufInfo->addr_ccu, &outDataBufInfo->addr_ccu);

	LOG_DBG("inDataSize(%d), sramInBufCapacity(%d)",
		inDataSize, CCU_IPC_IBUF_CAPACITY);
	LOG_DBG("outDataSize(%d), sramOutBufCapacity(%d)",
		outDataSize, CCU_IPC_OBUF_CAPACITY);


	// Resolve Input buffer
	if (inDataSize <= CCU_IPC_IBUF_CAPACITY) {
		LOG_DBG("using Sram input buffer");
		inDataBufInfo->bufType = CCU_SRAM_IO;
	} else {
		LOG_ERR("inDataSize excceed buffer capacity., sramBufCap(%d)",
			CCU_IPC_IBUF_CAPACITY);
		return false;
	}

	LOG_DBG("inDataBufInfo.addr_ap: %p, addr_ccu: %p",
		inDataBufInfo->addr_ap, inDataBufInfo->addr_ccu);

	// Resolve Output buffer
	if (outDataSize <= CCU_IPC_OBUF_CAPACITY) {
		LOG_DBG("using Sram output buffer");
		outDataBufInfo->bufType = CCU_SRAM_IO;
	} else {
		LOG_ERR("outDataSize excceed buffer capacity.");
		return false;
	}

	LOG_DBG("outDataBufInfo.addr_ap: %p, addr_ccu: %p",
		outDataBufInfo->addr_ap, outDataBufInfo->addr_ccu);

	return true;
}


bool ccuControl(
	enum ccu_feature_type featureType,
	enum IMGSENSOR_SENSOR_IDX sensorIdx,
	uint32_t msgId, void *inDataPtr,
	uint32_t inDataSize, void *outDataPtr, uint32_t outDataSize)
{
	struct CcuIOBufInfo inDataBufInfo;
	struct CcuIOBufInfo outDataBufInfo;
	struct ccu_msg msg = {featureType, msgId, inDataBufInfo.addr_ccu,
		outDataBufInfo.addr_ccu, CCU_CAM_TG_NONE, sensorIdx};
	uint32_t ret;

	mutex_lock(&ccu_ipc_mutex);

	LOG_DBG("[%s]+: ft(%d), sIdx(%d), msgId(%d)\n",
		 __func__, featureType, sensorIdx, msgId);

	//resolve IO buf
	ret = _resolveCmdIOBuf(&inDataBufInfo,
		&outDataBufInfo, inDataSize, outDataSize);
	if (ret == MFALSE) {
		LOG_ERR("resolveCmdIOBuf failed, cmd abort.");
		mutex_unlock(&ccu_ipc_mutex);
		return false;
	}

	//Check if need input data
	if (inDataSize != 0) {
		//copy input data to IPC buffer & command data buffer
		_copyCmdInData(&inDataBufInfo, inDataPtr, inDataSize);
	}

	msg.in_data_ptr = inDataBufInfo.addr_ccu;
	msg.out_data_ptr = outDataBufInfo.addr_ccu;
	ret = ccu_ipc_send(&msg);
	if (ret != 0) {
		LOG_ERR("sendCcuCommnadIpc failed, msgId(%d)", msgId);
		mutex_unlock(&ccu_ipc_mutex);
		return false;
	}

	//check if need to copy output data
	if (outDataSize != 0)
		_copyCmdOutData(&outDataBufInfo, outDataPtr, outDataSize);

	LOG_DBG("-:\n", __func__);
	mutex_unlock(&ccu_ipc_mutex);
	return true;
}

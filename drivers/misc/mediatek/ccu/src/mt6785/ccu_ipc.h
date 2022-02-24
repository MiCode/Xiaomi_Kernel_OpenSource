/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#include "ccu_ext_interface/ccu_ext_interface.h"
#include "kd_camera_feature.h"/*for sensoridx enum*/

enum IoBufType {
	SYS_DRAM_IO,
	CCU_SRAM_IO
};

struct CcuIOBufInfo {
	enum IoBufType bufType;
	uint8_t *addr_ap;
	uint8_t *addr_ap_interm;
	uint32_t addr_ccu;
};

bool ccuControl(
	enum ccu_tg_info tg_info,
	uint32_t msgId, void *inDataPtr, uint32_t inDataSize,
	void *outDataPtr, uint32_t outDataSize);
bool _copyCmdInData(struct CcuIOBufInfo *inDataBufInfo,
	void *inDataPtr, uint32_t inDataSize);
bool _copyCmdOutData(struct CcuIOBufInfo *outDataBufInfo,
	void *outDataPtr, uint32_t outDataSize);
void ccu_ipc_init(unsigned int *ccuDmBase, unsigned int *ccuCtrlBase);
int ccu_ipc_send(struct ccu_msg *msg);
bool ccu_ipc_getIObuffer(void **ipcInDataPtr, void **ipcOutDataPtr,
	uint32_t *ipcInDataAddrCcu, uint32_t *ipcOutDataAddrCcu);

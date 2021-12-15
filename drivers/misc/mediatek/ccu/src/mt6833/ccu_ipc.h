/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "ccu_ext_interface/ccu_ext_interface.h"
#include "kd_camera_feature.h"/*for sensoridx enum*/

struct __aligned(8) shared_buf_map {
	/*** from CCU->APMCU ***/
	MUINT32 ipc_in_data_addr_ccu;
	MUINT32 ipc_out_data_addr_ccu;
	MUINT32 ipc_in_data_base_offset;
	MUINT32 ipc_out_data_base_offset;
	MUINT32 ipc_base_offset;

	/*** from APMCU->CCU ***/
	MUINT32 bkdata_ddr_buf_mva;
};

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
	enum ccu_feature_type featureType,
	enum IMGSENSOR_SENSOR_IDX sensorIdx,
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

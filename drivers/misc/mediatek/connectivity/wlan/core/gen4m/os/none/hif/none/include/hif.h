/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*! \file   "hif.h"
 *    \brief  Functions for the driver to register bus and setup the IRQ
 *
 *    Functions for the driver to register bus and setup the IRQ
 */

#ifndef _HIF_H
#define _HIF_H

#if defined(_HIF_NONE)
#define HIF_NAME "NONE"
#else
#error "No HIF defined!"
#endif

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

struct GL_HIF_INFO;


/* host interface's private data structure, which is attached to os glue
 ** layer info structure.
 */
struct GL_HIF_INFO {
};

struct BUS_INFO {
};


/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
/* Common data type */
#define HIF_NUM_OF_QM_RX_PKT_NUM        512

/* chip dependent? used in wlanHarvardFormatDownload */
#define HIF_CR4_FWDL_SECTION_NUM            2
#define HIF_IMG_DL_STATUS_PORT_IDX          0

/* enable/disable TX resource control */
#define HIF_TX_RESOURCE_CTRL                1

/* enable/disable TX resource control PLE */
#define HIF_TX_RESOURCE_CTRL_PLE            0

#define HIF_IST_LOOP_COUNT              128

/* Min msdu count to trigger Tx during INT polling state */
#define HIF_IST_TX_THRESHOLD            32

#define HIF_TX_BUFF_COUNT_TC0               3
#define HIF_TX_BUFF_COUNT_TC1               3
#define HIF_TX_BUFF_COUNT_TC2               3
#define HIF_TX_BUFF_COUNT_TC3               3
#define HIF_TX_BUFF_COUNT_TC4               2
/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/* kal internal use */
#define glRegisterBus(_pfProbe, _pfRemove)

#define glUnregisterBus(_pfRemove)

#define glSetHifInfo(_prGlueInfo, _ulCookie)

#define glClearHifInfo(_prGlueInfo)

#define glBusInit(_pvData)

#define glBusRelease(_pData)

#define glBusSetIrq(_pvData, _pfnIsr, _pvCookie)

#define glBusFreeIrq(_pvData, _pvCookie)

#define glSetPowerState(_prGlueInfo, _ePowerMode)

#define glGetDev(_prCtx, _ppDev)

#define glGetHifDev(_prHif, _ppDev)

#define HAL_WAKE_UP_WIFI(_prAdapter)

#define halWpdmaInitRing(_glueinfo, __fgResetHif) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

uint8_t halTxRingDataSelect(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/* for nic/hal.h */
/*
 * kal_virt_write_tx_port: Write frame to data port
 * @ad: structure for adapter private data
 * @pid: port id to write data, eg.NIC_TX_INIT_CMD_PORT
 * @len: data len to write
 * @buf: data buf pointer
 * @buf_size: maximum size for data buffer, buf_size >= len
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_write_tx_port(struct ADAPTER *ad,
	uint16_t pid, uint32_t len, uint8_t *buf, uint32_t buf_size);

/*
 * kal_virt_get_wifi_func_stat: check HW status when check ready fail
 * @ad: structure for adapter private data
 * @res: status for return
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_get_wifi_func_stat(struct ADAPTER *ad, uint32_t *res);

/*
 * kal_virt_chk_wifi_func_off: check HW status for function OFF
 * @ad: structure for adapter private data
 * @ready_bits: asserted bit if function is off correctly
 * @res: status for return
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_chk_wifi_func_off(struct ADAPTER *ad, uint32_t ready_bits,
	uint8_t *res);

/*
 * kal_virt_chk_wifi_func_off: check HW status for function ready
 * @ad: structure for adapter private data
 * @ready_bits: asserted bit if function is ready
 * @res: status for return
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_chk_wifi_func_ready(struct ADAPTER *ad, uint32_t ready_bits,
	uint8_t *res);

/*
 * kal_virt_set_mailbox_readclear: set read clear on hw mailbox (?)
 * @ad: structure for adapter private data
 * @enable: enable read clear or not
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_set_mailbox_readclear(struct ADAPTER *ad, bool enable);

/*
 * kal_virt_set_int_stat_readclear: set hardware interrupt read clear
 * @ad: structure for adapter private data
 *
 * not: no disable command
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_set_int_stat_readclear(struct ADAPTER *ad);

/*
 * kal_virt_init_hif: do device related initialization
 * @ad: structure for adapter private data
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_init_hif(struct ADAPTER *ad);

/*
 * kal_virt_enable_fwdl: enable firmware download
 * @ad: structure for adapter private data
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_enable_fwdl(struct ADAPTER *ad, bool enable);

/*
 * kal_virt_get_int_status: read interrupt status
 * @ad: structure for adapter private data
 * @status: return value for interrupt status
 *
 * not: implementation for different HIF may refer to nic/hal.h
 */
void kal_virt_get_int_status(struct ADAPTER *ad, uint32_t *status);
#endif /* _HIF_H */

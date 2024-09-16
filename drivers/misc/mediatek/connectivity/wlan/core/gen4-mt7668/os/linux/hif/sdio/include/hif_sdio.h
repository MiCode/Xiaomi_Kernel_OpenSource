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
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/hif/sdio/include/hif_sdio.h#2
*/

/*! \file   "hif_sdio.h"
*    \brief
*
*
*/


#ifndef _HIF_SDIO_H
#define _HIF_SDIO_H
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#define HIF_SDIO_DEBUG  (0)	/* 0:turn off debug msg and assert, 1:turn off debug msg and assert */

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "mtk_porting.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define CFG_CLIENT_COUNT  (8)

#define HIF_DEFAULT_BLK_SIZE  (256)
#define HIF_DEFAULT_VENDOR    (0x037A)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* sdio driver data mapping */
typedef struct _MTK_WCN_SDIO_DRIVER_DATA_MAPPING {
	UINT_16 card_id;	/* TPLMID_CARD: card ID */
	struct mt66xx_hif_driver_data *mt66xx_driver_data;
} MTK_WCN_SDIO_DRIVER_DATA_MAPPING;


/* Function info provided by client driver */
typedef struct _MTK_WCN_HIF_SDIO_FUNCINFO MTK_WCN_HIF_SDIO_FUNCINFO;

/* Client context provided by hif_sdio driver for the following function call */
typedef ULONG MTK_WCN_HIF_SDIO_CLTCTX;

/* Callback functions provided by client driver */
typedef INT_32(*MTK_WCN_HIF_SDIO_PROBE)(MTK_WCN_HIF_SDIO_CLTCTX, const MTK_WCN_HIF_SDIO_FUNCINFO *prFuncInfo);
typedef INT_32(*MTK_WCN_HIF_SDIO_REMOVE)(MTK_WCN_HIF_SDIO_CLTCTX);
typedef INT_32(*MTK_WCN_HIF_SDIO_IRQ)(MTK_WCN_HIF_SDIO_CLTCTX);

/* Function info provided by client driver */
struct _MTK_WCN_HIF_SDIO_FUNCINFO {
	UINT_16 manf_id;	/* TPLMID_MANF: manufacturer ID */
	UINT_16 card_id;	/* TPLMID_CARD: card ID */
	UINT_16 func_num;	/* Function Number */
	UINT_16 blk_sz;		/* Function block size */
};

/* Client info provided by client driver */
typedef struct _MTK_WCN_HIF_SDIO_CLTINFO {
	const MTK_WCN_HIF_SDIO_FUNCINFO *func_tbl;	/* supported function info table */
	UINT_32 func_tbl_size;	/* supported function table info element number */
	MTK_WCN_HIF_SDIO_PROBE hif_clt_probe;	/* callback function for probing */
	MTK_WCN_HIF_SDIO_REMOVE hif_clt_remove;	/* callback function for removing */
	MTK_WCN_HIF_SDIO_IRQ hif_clt_irq;	/* callback function for interrupt handling */
} MTK_WCN_HIF_SDIO_CLTINFO;

/* function info provided by registed function */
typedef struct _MTK_WCN_HIF_SDIO_REGISTINFO {
	const MTK_WCN_HIF_SDIO_CLTINFO *sdio_cltinfo;	/* client's MTK_WCN_HIF_SDIO_CLTINFO pointer */
	const MTK_WCN_HIF_SDIO_FUNCINFO *func_info;	/* supported function info pointer */
} MTK_WCN_HIF_SDIO_REGISTINFO;

/* Card info provided by probed function */
typedef struct _MTK_WCN_HIF_SDIO_PROBEINFO {
	struct sdio_func *func;	/* probed sdio function pointer */
	INT_8 clt_idx;		/* registered function table info element number (initial value is -1) */
	MTK_WCN_BOOL interrupted;	/* TRUE: interrupted, FALSE: not interrupted */
	void *private_data_p;	/* clt's private data pointer */
} MTK_WCN_HIF_SDIO_PROBEINFO;

/* work queue info needed by worker */
typedef struct _MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO {
	struct work_struct probe_work;	/* work queue structure */
	MTK_WCN_HIF_SDIO_REGISTINFO *registinfo_p;	/* MTK_WCN_HIF_SDIO_REGISTINFO pointer of the client */
	INT_8 probe_idx;	/* probed function table info element number (initial value is -1) */
} MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO;

/* error code returned by hif_sdio driver (use NEGATIVE number) */
typedef enum {
	HIF_SDIO_ERR_SUCCESS = 0,
	HIF_SDIO_ERR_FAIL,	/* generic error */
	HIF_SDIO_ERR_INVALID_PARAM,
	HIF_SDIO_ERR_DUPLICATED,
	HIF_SDIO_ERR_UNSUP_MANF_ID,
	HIF_SDIO_ERR_UNSUP_CARD_ID,
	HIF_SDIO_ERR_INVALID_FUNC_NUM,
	HIF_SDIO_ERR_INVALID_BLK_SZ,
} MTK_WCN_HIF_SDIO_ERR;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*!
 * \brief A macro used to describe an SDIO function
 *
 * Fill an MTK_WCN_HIF_SDIO_FUNCINFO structure with function-specific information
 *
 * \param manf      the 16 bit manufacturer id
 * \param card      the 16 bit card id
 * \param func      the 16 bit function number
 * \param b_sz    the 16 bit function block size
 */
#define MTK_WCN_HIF_SDIO_FUNC(manf, card, func, b_sz) \
	.manf_id = (manf), .card_id = (card), .func_num = (func), .blk_sz = (b_sz)

/*!
 * \brief A debug print used to print debug messages while compiler flag HIF_SDIO_DEBUG on.
 *
 */
#if HIF_SDIO_DEBUG
#define DPRINTK(fmt, args...) pr_debug("%s: " fmt, __func__, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*!
 * \brief ASSERT function definition.
 *
 */
#if HIF_SDIO_DEBUG
#define ASSERT(expr) \
	{ \
		if (!(expr)) { \
			LOG_FUNC("assertion failed! %s[%d]: %s\n",\
				__func__, __LINE__, #expr); \
			WARN_ON(!(expr));\
		} \
	}
#else
#define ASSERT(expr)    do {} while (0)
#endif

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*!
 * \brief MTK hif sdio client registration function
 *
 * Client uses this function to do hif sdio registration
 *
 * \param pinfo     a pointer of client's information
 *
 * \retval 0    register successfully
 * \retval < 0  error code
 */
extern INT_32 mtk_wcn_hif_sdio_client_reg(const MTK_WCN_HIF_SDIO_CLTINFO *pinfo);

extern INT_32 mtk_wcn_hif_sdio_client_unreg(const MTK_WCN_HIF_SDIO_CLTINFO *pinfo);

extern INT_32 mtk_wcn_hif_sdio_readb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT_32 offset, PUINT_8 pvb);

extern INT_32 mtk_wcn_hif_sdio_writeb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT_32 offset, UINT_8 vb);

extern INT_32 mtk_wcn_hif_sdio_readl(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT_32 offset, PUINT_32 pvl);

extern INT_32 mtk_wcn_hif_sdio_writel(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT_32 offset, UINT_32 vl);

extern INT_32 mtk_wcn_hif_sdio_read_buf(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT_32 offset, PUINT_32 pbuf, UINT_32 len);

extern INT_32 mtk_wcn_hif_sdio_write_buf(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT_32 offset, PUINT_32 pbuf, UINT_32 len);

extern void mtk_wcn_hif_sdio_set_drvdata(MTK_WCN_HIF_SDIO_CLTCTX ctx, void *private_data_p);

extern void *mtk_wcn_hif_sdio_get_drvdata(MTK_WCN_HIF_SDIO_CLTCTX ctx);

extern void mtk_wcn_hif_sdio_get_dev(MTK_WCN_HIF_SDIO_CLTCTX ctx, struct device **dev);

extern void mtk_wcn_hif_sdio_enable_irq(MTK_WCN_HIF_SDIO_CLTCTX ctx, MTK_WCN_BOOL enable);

extern INT_32 mtk_wcn_hif_sdio_wake_up_ctrl(MTK_WCN_HIF_SDIO_CLTCTX ctx);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_SDIO_H */

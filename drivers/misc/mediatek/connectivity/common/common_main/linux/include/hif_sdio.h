/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*!
 * \file   "hif_sdio.h"
 * \brief
 */

/*
 *
 * 07 25 2010 george.kuo
 *
 * Move hif_sdio driver to linux directory.
 *
 * 07 23 2010 george.kuo
 *
 * Add MT6620 driver source tree
 * , including char device driver (wmt, bt, gps), stp driver,
 * interface driver (tty ldisc and hif_sdio), and bt hci driver.
**
**
*/

#ifndef _HIF_SDIO_H
#define _HIF_SDIO_H
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#define HIF_SDIO_DEBUG  (0)	/* 0:turn off debug msg and assert, 1:turn off debug msg and assert */
#define HIF_SDIO_API_EXTENSION      (0)
/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>
#include <sdio_ops.h>

#include <linux/mm.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>

#include "osal_typedef.h"
#include "osal.h"
#include "wmt_exp.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define CFG_CLIENT_COUNT (12)

#define HIF_DEFAULT_BLK_SIZE  (256)
#define HIF_DEFAULT_VENDOR    (0x037A)

#define HIF_SDIO_LOG_LOUD    4
#define HIF_SDIO_LOG_DBG     3
#define HIF_SDIO_LOG_INFO    2
#define HIF_SDIO_LOG_WARN    1
#define HIF_SDIO_LOG_ERR     0

#define CCCR_F8		(0X00F8)
#define SWPCDBGR	(0x0154)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* Function info provided by client driver */
typedef struct _MTK_WCN_HIF_SDIO_FUNCINFO MTK_WCN_HIF_SDIO_FUNCINFO;

/* Client context provided by hif_sdio driver for the following function call */
typedef UINT32 MTK_WCN_HIF_SDIO_CLTCTX;

/* Callback functions provided by client driver */
typedef INT32 (*MTK_WCN_HIF_SDIO_PROBE)(MTK_WCN_HIF_SDIO_CLTCTX,
		const MTK_WCN_HIF_SDIO_FUNCINFO *);
typedef INT32 (*MTK_WCN_HIF_SDIO_REMOVE)(MTK_WCN_HIF_SDIO_CLTCTX);
typedef INT32 (*MTK_WCN_HIF_SDIO_IRQ)(MTK_WCN_HIF_SDIO_CLTCTX);

/* Function info provided by client driver */
struct _MTK_WCN_HIF_SDIO_FUNCINFO {
	UINT16 manf_id;		/* TPLMID_MANF: manufacturer ID */
	UINT16 card_id;		/* TPLMID_CARD: card ID */
	UINT16 func_num;	/* Function Number */
	UINT16 blk_sz;		/* Function block size */
};

/* Client info provided by client driver */
typedef struct _MTK_WCN_HIF_SDIO_CLTINFO {
	const MTK_WCN_HIF_SDIO_FUNCINFO *func_tbl;	/* supported function info table */
	UINT32 func_tbl_size;	/* supported function table info element number */
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
	PVOID private_data_p;	/* clt's private data pointer */
	MTK_WCN_BOOL on_by_wmt;	/* TRUE: on by wmt, FALSE: not on by wmt */
	/* added for sdio irq sync and mmc single_irq workaround */
	MTK_WCN_BOOL sdio_irq_enabled;	/* TRUE: can handle sdio irq; FALSE: no sdio irq handling */
	INT32 clt_idx;		/* registered function table info element number (initial value is -1) */
} MTK_WCN_HIF_SDIO_PROBEINFO;

/* work queue info needed by worker */
typedef struct _MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO {
	struct work_struct probe_work;	/* work queue structure */
	MTK_WCN_HIF_SDIO_REGISTINFO *registinfo_p;	/* MTK_WCN_HIF_SDIO_REGISTINFO pointer of the client */
	INT8 probe_idx;		/* probed function table info element number (initial value is -1) */
} MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO;

/* global resource locks info of hif_sdio drv */
typedef struct _MTK_WCN_HIF_SDIO_LOCKINFO {
	spinlock_t probed_list_lock;	/* spin lock for probed list */
	spinlock_t clt_list_lock;	/* spin lock for client registed list */
} MTK_WCN_HIF_SDIO_LOCKINFO;

/* SDIO Deep Sleep Information by chip, maintained by HIF-SDIO itself */
typedef struct _MTK_WCN_HIF_SDIO_DS_CLT_INFO {
	MTK_WCN_HIF_SDIO_CLTCTX ctx;
	UINT16 func_num;
	UINT8 act_flag;
	UINT8 ds_en_flag;
} MTK_WCN_HIF_SDIO_DS_CLT_INFO;

typedef struct _MTK_WCN_HIF_SDIO_DS_INFO {
	UINT32 chip_id;		/*chipid */
	UINT32 reg_offset;	/*offset in CCCR of control register of deep sleep */
	UINT8 value;		/*value to set to CCCR reg_offset, when enable deep sleep */
	MTK_WCN_HIF_SDIO_DS_CLT_INFO clt_info[2];	/*currently, only BGF and WIFI function need this function */
	struct mutex lock;
} MTK_WCN_HIF_SDIO_DS_INFO;


/* error code returned by hif_sdio driver (use NEGATIVE number) */
typedef enum {
	HIF_SDIO_ERR_SUCCESS = 0,
	HIF_SDIO_ERR_FAIL = HIF_SDIO_ERR_SUCCESS - 1,	/* generic error */
	HIF_SDIO_ERR_INVALID_PARAM = HIF_SDIO_ERR_FAIL - 1,
	HIF_SDIO_ERR_DUPLICATED = HIF_SDIO_ERR_INVALID_PARAM - 1,
	HIF_SDIO_ERR_UNSUP_MANF_ID = HIF_SDIO_ERR_DUPLICATED - 1,
	HIF_SDIO_ERR_UNSUP_CARD_ID = HIF_SDIO_ERR_UNSUP_MANF_ID - 1,
	HIF_SDIO_ERR_INVALID_FUNC_NUM = HIF_SDIO_ERR_UNSUP_CARD_ID - 1,
	HIF_SDIO_ERR_INVALID_BLK_SZ = HIF_SDIO_ERR_INVALID_FUNC_NUM - 1,
	HIF_SDIO_ERR_NOT_PROBED = HIF_SDIO_ERR_INVALID_BLK_SZ - 1,
	HIF_SDIO_ERR_ALRDY_ON = HIF_SDIO_ERR_NOT_PROBED - 1,
	HIF_SDIO_ERR_ALRDY_OFF = HIF_SDIO_ERR_ALRDY_ON - 1,
	HIF_SDIO_ERR_CLT_NOT_REG = HIF_SDIO_ERR_ALRDY_OFF - 1,
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
 * \brief A macro used to generate hif_sdio client's context
 *
 * Generate a context for hif_sdio client based on the following input parameters
 * |<-card id (16bits)->|<-block size in unit of 256 bytes(8 bits)->|<-function number(4bits)->|<-index(4bits)->|
 *
 * \param manf      the 16 bit manufacturer id
 * \param card      the 16 bit card id
 * \param func      the 16 bit function number
 * \param b_sz    the 16 bit function block size
 */
#define CLTCTX(cid, func, blk_sz, idx) \
(MTK_WCN_HIF_SDIO_CLTCTX)((((UINT32)(cid) & 0xFFFFUL) << 16) | \
	(((UINT32)(func) & 0xFUL) << 4) | \
	(((UINT32)(blk_sz) & 0xFF00UL) << 0) | \
	(((UINT32)idx & 0xFUL) << 0))

/*!
 * \brief A set of macros used to get information out of an hif_sdio client context
 *
 * Generate a context for hif_sdio client based on the following input parameters
 */
#define CLTCTX_CID(ctx) (((ctx) >> 16) & 0xFFFF)
#define CLTCTX_FUNC(ctx) (((ctx) >> 4) & 0xF)
#define CLTCTX_BLK_SZ(ctx) (((ctx) >> 0) & 0xFF00)
#define CLTCTX_IDX(ctx) ((ctx) & 0xF)
#define CLTCTX_IDX_VALID(idx) ((idx >= 0) && (idx < CFG_CLIENT_COUNT))
#define CLTCTX_UIDX_VALID(idx) (idx < CFG_CLIENT_COUNT)


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

#ifdef DFT_TAG
#undef DFT_TAG
#endif

#ifndef DFT_TAG
#define DFT_TAG         "[HIF-SDIO]"
#endif

extern INT32 gHifSdioDbgLvl;


#define HIF_SDIO_LOUD_FUNC(fmt, arg...)	\
do { if (gHifSdioDbgLvl >= HIF_SDIO_LOG_LOUD)	\
	osal_warn_print(DFT_TAG"[L]%s:"  fmt, __func__, ##arg);	\
} while (0)
#define HIF_SDIO_DBG_FUNC(fmt, arg...)	\
do { if (gHifSdioDbgLvl >= HIF_SDIO_LOG_DBG)	\
	osal_warn_print(DFT_TAG"[D]%s:"  fmt, __func__, ##arg);	\
} while (0)
#define HIF_SDIO_INFO_FUNC(fmt, arg...)	\
do { if (gHifSdioDbgLvl >= HIF_SDIO_LOG_INFO)	\
	osal_warn_print(DFT_TAG"[I]%s:"  fmt, __func__, ##arg);	\
} while (0)
#define HIF_SDIO_WARN_FUNC(fmt, arg...)	\
do { if (gHifSdioDbgLvl >= HIF_SDIO_LOG_WARN)	\
	osal_warn_print(DFT_TAG"[W]%s(%d):"  fmt, __func__, __LINE__, ##arg);	\
} while (0)
#define HIF_SDIO_ERR_FUNC(fmt, arg...)	\
do { if (gHifSdioDbgLvl >= HIF_SDIO_LOG_ERR)	\
	osal_err_print(DFT_TAG"[E]%s(%d):"  fmt, __func__, __LINE__, ##arg);	\
} while (0)

/*!
 * \brief ASSERT function definition.
 *
 */
#if HIF_SDIO_DEBUG
#define HIF_SDIO_ASSERT(expr) \
{ \
		if (!(expr)) { \
			osal_warn_print("assertion failed! %s[%d]: %s\n",\
					__func__, __LINE__, #expr); \
			osal_bug_on(!(expr));\
		} \
}
#else
#define HIF_SDIO_ASSERT(expr)    do {} while (0)
#endif

/* define function 0 CR */
#define CCCR_06		(0x06)
#define CCCR_F0		(0xF0)

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
extern INT32 mtk_wcn_hif_sdio_client_reg(const MTK_WCN_HIF_SDIO_CLTINFO *pinfo);

extern INT32 mtk_wcn_hif_sdio_client_unreg(const MTK_WCN_HIF_SDIO_CLTINFO *pinfo);

extern INT32 mtk_wcn_hif_sdio_readb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, PUINT8 pvb);

extern INT32 mtk_wcn_hif_sdio_writeb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, UINT8 vb);

extern INT32 mtk_wcn_hif_sdio_readl(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, PUINT32 pvl);

extern INT32 mtk_wcn_hif_sdio_writel(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, UINT32 vl);

extern INT32 mtk_wcn_hif_sdio_read_buf(MTK_WCN_HIF_SDIO_CLTCTX ctx,
				       UINT32 offset, PUINT32 pbuf, UINT32 len);

extern INT32 mtk_wcn_hif_sdio_write_buf(MTK_WCN_HIF_SDIO_CLTCTX ctx,
					UINT32 offset, PUINT32 pbuf, UINT32 len);

extern INT32 mtk_wcn_hif_sdio_abort(MTK_WCN_HIF_SDIO_CLTCTX ctx);

INT32 hif_sdio_wake_up_ctrl(MTK_WCN_HIF_SDIO_CLTCTX ctx);

extern VOID mtk_wcn_hif_sdio_set_drvdata(MTK_WCN_HIF_SDIO_CLTCTX ctx, PVOID private_data_p);

extern PVOID mtk_wcn_hif_sdio_get_drvdata(MTK_WCN_HIF_SDIO_CLTCTX ctx);

extern INT32 mtk_wcn_hif_sdio_wmt_control(WMT_SDIO_FUNC_TYPE func_type, MTK_WCN_BOOL is_on);

extern INT32 mtk_wcn_hif_sdio_bus_set_power(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 pwrState);

extern VOID mtk_wcn_hif_sdio_get_dev(MTK_WCN_HIF_SDIO_CLTCTX ctx, struct device **dev);

extern INT32 mtk_wcn_hif_sdio_update_cb_reg(INT32(*ts_update)(VOID));

extern VOID mtk_wcn_hif_sdio_enable_irq(MTK_WCN_HIF_SDIO_CLTCTX ctx, MTK_WCN_BOOL enable);

extern INT32 mtk_wcn_hif_sdio_f0_writeb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, UINT8 vb);

extern INT32 mtk_wcn_hif_sdio_f0_readb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, PUINT8 pvb);
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
INT32 mtk_wcn_hif_sdio_deep_sleep_flag_set(MTK_WCN_BOOL flag);
#endif

#define DELETE_HIF_SDIO_CHRDEV 1
#if !(DELETE_HIF_SDIO_CHRDEV)
INT32 mtk_wcn_hif_sdio_tell_chipid(INT32 chipId);
INT32 mtk_wcn_hif_sdio_query_chipid(INT32 waitFlag);
#endif


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*******************************************************************************
*                   E X T E R N A L    F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif				/* _HIF_SDIO_H */

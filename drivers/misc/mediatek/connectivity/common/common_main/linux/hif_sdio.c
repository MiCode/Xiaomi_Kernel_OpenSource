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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#define HIF_SDIO_UPDATE (1)
#define HIF_SDIO_SUPPORT_SUSPEND (1)
#define HIF_SDIO_SUPPORT_WAKEUP (0)

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/proc_fs.h>
#include "hif_sdio.h"
/* #include "hif_sdio_chrdev.h" */

#if MTK_HIF_SDIO_AUTOK_ENABLED
#include <mt_boot.h>
#endif

#define mmc_power_up_ext(x)
#define mmc_power_off_ext(x)
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* #define DRV_NAME "[hif_sdio]" */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#if HIF_SDIO_SUPPORT_SUSPEND
static INT32 hif_sdio_suspend(struct device *dev);

static INT32 hif_sdio_resume(struct device *dev);
#endif
static INT32 hif_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id);

static VOID hif_sdio_remove(struct sdio_func *func);

static VOID hif_sdio_irq(struct sdio_func *func);

static _osal_inline_ INT32 hif_sdio_clt_probe_func(MTK_WCN_HIF_SDIO_REGISTINFO *registinfo_p,
		INT8 probe_idx);

static VOID hif_sdio_clt_probe_worker(struct work_struct *work);

static _osal_inline_ INT32 hif_sdio_find_probed_list_index_by_func(struct sdio_func *func);

#if 0				/* TODO:[ChangeFeature][George] remove obsolete function? */
static INT32 hif_sdio_find_probed_list_index_by_clt_index(INT32 clt_index);
#endif

static _osal_inline_ INT32 hif_sdio_find_probed_list_index_by_id_func(UINT16 vendor, UINT16 device,
		UINT16 func_num);

static _osal_inline_ VOID hif_sdio_init_clt_list(INT32 index);

static _osal_inline_ INT32 hif_sdio_find_clt_list_index(UINT16 vendor, UINT16 device, UINT16 func_num);

static _osal_inline_ INT32 hif_sdio_check_supported_sdio_id(UINT16 vendor, UINT16 device);

static _osal_inline_ INT32 hif_sdio_check_duplicate_sdio_id(UINT16 vendor, UINT16 device, UINT16 func_num);

static _osal_inline_ INT32 hif_sdio_add_clt_list(PINT32 clt_index_p, const MTK_WCN_HIF_SDIO_CLTINFO *pinfo,
		UINT32 tbl_index);

static _osal_inline_ INT32 hif_sdio_stp_on(VOID);

static _osal_inline_ INT32 hif_sdio_stp_off(VOID);

static _osal_inline_ INT32 hif_sdio_wifi_on(VOID);

static _osal_inline_ INT32 hif_sdio_wifi_off(VOID);

static _osal_inline_ INT32 hif_sdio_do_autok(struct sdio_func *func);

#if 0
static _osal_inline_ INT32 hif_sdio_is_autok_support(struct sdio_func *func);
#endif

static _osal_inline_ INT32 hif_sdio_deep_sleep_info_init(VOID);

static _osal_inline_ INT32 hif_sdio_deep_sleep_info_set_act(UINT32 chipid, UINT16 func_num,
		MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT8 act_flag);

static _osal_inline_ INT32 hif_sdio_deep_sleep_ctrl(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT8 en_flag);

static _osal_inline_ INT32 wmt_tra_sdio_update(VOID);

static _osal_inline_ INT32 hif_sdio_deep_sleep_info_dmp(MTK_WCN_HIF_SDIO_DS_INFO *p_ds_info);

static _osal_inline_ VOID hif_sdio_dump_probe_list(VOID);

static _osal_inline_ VOID hif_sdio_init_probed_list(INT32 index);

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* Supported SDIO device table */
static const struct sdio_device_id mtk_sdio_id_tbl[] = {
	/* MT6618 *//* Not an SDIO standard class device */
	{SDIO_DEVICE(0x037A, 0x018A)},	/* SDIO1:WIFI */
	{SDIO_DEVICE(0x037A, 0x018B)},	/* SDIO2:FUNC1:BT+FM */
	{SDIO_DEVICE(0x037A, 0x018C)},	/* 2-function (SDIO2:FUNC1:BT+FM, FUNC2:WIFI) */

	/* MT6619 *//* Not an SDIO standard class device */
	{SDIO_DEVICE(0x037A, 0x6619)},	/* SDIO2:FUNC1:BT+FM+GPS */

	/* MT6620 *//* Not an SDIO standard class device */
	{SDIO_DEVICE(0x037A, 0x020A)},	/* SDIO1:FUNC1:WIFI */
	{SDIO_DEVICE(0x037A, 0x020B)},	/* SDIO2:FUNC1:BT+FM+GPS */
	{SDIO_DEVICE(0x037A, 0x020C)},	/* 2-function (SDIO2:FUNC1:BT+FM+GPS, FUNC2:WIFI) */

	/* MT5921 *//* Not an SDIO standard class device */
	{SDIO_DEVICE(0x037A, 0x5921)},

	/* MT6628 *//* SDIO1: Wi-Fi, SDIO2: BGF */
	{SDIO_DEVICE(0x037A, 0x6628)},

	/* MT6630 *//* SDIO1: Wi-Fi, SDIO2: BGF */
	{SDIO_DEVICE(0x037A, 0x6630)},

	/* MT6632 *//* SDIO1: Wi-Fi */
	{SDIO_DEVICE(0x037A, 0x6602)},

	/* MT6632 *//* SDIO2: BGF */
	{SDIO_DEVICE(0x037A, 0x6632)},

	{ /* end: all zeroes */ },
};

#if HIF_SDIO_SUPPORT_SUSPEND
static const struct dev_pm_ops mtk_sdio_pmops = {
	.suspend = hif_sdio_suspend,
	.resume = hif_sdio_resume,
};
#endif

static struct sdio_driver mtk_sdio_client_drv = {
	.name = "mtk_sdio_client",	/* MTK SDIO Client Driver */
	.id_table = mtk_sdio_id_tbl,	/* all supported struct sdio_device_id table */
	.probe = hif_sdio_probe,
	.remove = hif_sdio_remove,
#if HIF_SDIO_SUPPORT_SUSPEND
	.drv = {
		.pm = &mtk_sdio_pmops,
		},
#endif
};

/* Registered client driver list */
/* static list g_hif_sdio_clt_drv_list */
static MTK_WCN_HIF_SDIO_REGISTINFO g_hif_sdio_clt_drv_list[CFG_CLIENT_COUNT];

/* MMC probed function list */
/* static list g_hif_sdio_probed_func_list */
static MTK_WCN_HIF_SDIO_PROBEINFO g_hif_sdio_probed_func_list[CFG_CLIENT_COUNT];

/* spin lock info for g_hif_sdio_clt_drv_list and g_hif_sdio_probed_func_list */
static MTK_WCN_HIF_SDIO_LOCKINFO g_hif_sdio_lock_info;

/* reference count, debug information? */
static INT32 gRefCount;
static INT32 (*fp_wmt_tra_sdio_update)(VOID);
static atomic_t hif_sdio_irq_enable_flag = ATOMIC_INIT(0);

/*deep sleep related information*/
MTK_WCN_HIF_SDIO_DS_INFO g_hif_sdio_ds_info_list[] = {
	{
	 .chip_id = 0x6630,
	 .reg_offset = 0xF1,
	 .value = 0x1,
	 },
	{
	 .chip_id = 0x6632,
	 .reg_offset = 0xF1,
	 .value = 0x1,
	 },
	{ /* end: all zeroes */ }
};



/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc WCN_SE_CS3");
MODULE_DESCRIPTION("MediaTek MT6620 HIF SDIO Driver");

MODULE_DEVICE_TABLE(sdio, mtk_sdio_id_tbl);

UINT32 gHifSdioDbgLvl = HIF_SDIO_LOG_INFO;


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

INT32 __weak mtk_wcn_sdio_irq_flag_set(INT32 falg)
{
	HIF_SDIO_INFO_FUNC("mtk_wcn_sdio_irq_flag_set is not define!!!!!\n");

	return 0;
}

INT32 mtk_wcn_hif_sdio_irq_flag_set(INT32 flag)
{

	if (0 == flag) {
		atomic_dec(&hif_sdio_irq_enable_flag);
		if (0 == atomic_read(&hif_sdio_irq_enable_flag))
			mtk_wcn_sdio_irq_flag_set(0);
	} else {
		atomic_inc(&hif_sdio_irq_enable_flag);
		if (1 == atomic_read(&hif_sdio_irq_enable_flag))
			mtk_wcn_sdio_irq_flag_set(1);
	}

	return 0;
}


/*!
 * \brief register the callback funciton for record the timestamp of sdio access
 *
 * \param  callback function
 *
 * \retval -EINVAL, when registered callback is invalid
 * \retval 0, when registered callback is valid
 */
INT32 mtk_wcn_hif_sdio_update_cb_reg(INT32(*ts_update) (VOID))
{
	if (ts_update) {
		fp_wmt_tra_sdio_update = ts_update;
		return 0;
	} else {
		return -EINVAL;
	}
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_update_cb_reg);

/*!
 * \brief update the accessing time of SDIO via callback function
 *
 * \param  void
 *
 * \retval -EINVAL, when callback is not registered
 * \retval returned value of callback
 */
static _osal_inline_ INT32 wmt_tra_sdio_update(VOID)
{
	if (fp_wmt_tra_sdio_update)
		return (*fp_wmt_tra_sdio_update) ();
	/* HIF_SDIO_WARN_FUNC("wmt_tra_sdio_update == NULL\n"); */
	return -EINVAL;
}

/*!
 * \brief Translate CLTCTX into a pointer to struct sdio_func if it is valid
 *
 * Translate a CLTCTX into a pointer to struct sdio_func if it is
 *  1) probed by mmc_core, and
 *  2) client driver is registered, and
 *  3) clt_idx of client driver is valid
 *
 * \param ctx a context provided by client driver
 *
 * \retval null if any condition is not valie
 * \retval a pointer to a struct sdio_func mapped by provided ctx
 */
static _osal_inline_ struct sdio_func *hif_sdio_ctx_to_func(MTK_WCN_HIF_SDIO_CLTCTX ctx)
{
	UINT32 probe_index;

	/* 4 <1> check if ctx is valid, registered, and probed */
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		return NULL;
	}
	/* the client has not been registered */
	if (unlikely(g_hif_sdio_probed_func_list[probe_index].clt_idx < 0)) {
		HIF_SDIO_WARN_FUNC
			("can't find client idx in probed list!ctx(0x%x) prob_idx(%d) clt_idx(%d)\n",
			 ctx, probe_index, g_hif_sdio_probed_func_list[probe_index].clt_idx);
		return NULL;
	}
	return g_hif_sdio_probed_func_list[probe_index].func;
}

static _osal_inline_ INT32 hif_sdio_deep_sleep_info_dmp(MTK_WCN_HIF_SDIO_DS_INFO *p_ds_info)
{
	UINT32 i = 0;
	MTK_WCN_HIF_SDIO_DS_CLT_INFO *ctl_info = NULL;
	UINT32 ctl_info_array_size = ARRAY_SIZE(p_ds_info->clt_info);

	mutex_lock(&p_ds_info->lock);
	HIF_SDIO_INFO_FUNC("p_ds_info: 0x%08x, chipid:0x%x, reg_offset:0x%x, value:0x%x\n",
			   p_ds_info, p_ds_info->chip_id, p_ds_info->reg_offset, p_ds_info->value);

	for (i = 0; i < ctl_info_array_size; i++) {
		ctl_info = &p_ds_info->clt_info[i];

		HIF_SDIO_INFO_FUNC
		    ("ctl_info[%d]--ctx:0x%08x, func_num:%d, act_flag:%d, en_flag:%d\n", i,
		     ctl_info->ctx, ctl_info->func_num, ctl_info->act_flag, ctl_info->ds_en_flag);
	}
	mutex_unlock(&p_ds_info->lock);
	return 0;
}

static _osal_inline_ INT32 hif_sdio_deep_sleep_info_init(VOID)
{
	UINT32 array_size = 0;
	UINT32 clt_info_size = 0;
	UINT32 i = 0;
	UINT32 j = 0;

	array_size = ARRAY_SIZE(g_hif_sdio_ds_info_list);

	/*set clt_info segment to 0 by default, when do stp/wifi on, write real information back */
	for (i = 0; i < array_size; i++) {
		mutex_init(&g_hif_sdio_ds_info_list[i].lock);
		clt_info_size = ARRAY_SIZE(g_hif_sdio_ds_info_list[i].clt_info);

		mutex_lock(&g_hif_sdio_ds_info_list[i].lock);
		for (j = 0; j < clt_info_size; j++)
			memset(&g_hif_sdio_ds_info_list[i].clt_info[j],
					0, sizeof(MTK_WCN_HIF_SDIO_DS_CLT_INFO));
		mutex_unlock(&g_hif_sdio_ds_info_list[i].lock);

		hif_sdio_deep_sleep_info_dmp(&g_hif_sdio_ds_info_list[i]);
	}

	return 0;
}

static _osal_inline_ INT32 hif_sdio_deep_sleep_info_set_act(UINT32 chipid, UINT16 func_num,
					       MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT8 act_flag)
{
	UINT32 i = 0;
	UINT32 array_size = 0;
	UINT32 clt_info_size = 0;
	UINT32 idx = 0;
	MTK_WCN_HIF_SDIO_DS_CLT_INFO *p_ds_clt_info = NULL;

	array_size = ARRAY_SIZE(g_hif_sdio_ds_info_list);

	/*search write index */
	for (i = 0; i < array_size; i++) {
		if (g_hif_sdio_ds_info_list[i].chip_id == chipid)
			break;
	}
	if (i >= array_size) {
		HIF_SDIO_WARN_FUNC("no valid ds info found for 0x%x\n", chipid);
		return -1;
	}
	HIF_SDIO_DBG_FUNC("valid ds info found for 0x%x\n", chipid);

	clt_info_size = ARRAY_SIZE(g_hif_sdio_ds_info_list[i].clt_info);

	if (func_num > clt_info_size) {
		HIF_SDIO_WARN_FUNC("func num <%d> exceed max clt info size <%d>\n", func_num,
				   clt_info_size);
		return -2;
	}
	idx = func_num - 1;
	p_ds_clt_info = &g_hif_sdio_ds_info_list[i].clt_info[idx];

	mutex_lock(&g_hif_sdio_ds_info_list[i].lock);
	p_ds_clt_info->func_num = func_num;
	p_ds_clt_info->ctx = ctx;
	p_ds_clt_info->act_flag = act_flag;
	p_ds_clt_info->ds_en_flag = 0;
	mutex_unlock(&g_hif_sdio_ds_info_list[i].lock);

	HIF_SDIO_INFO_FUNC("set act_flag to %d for ctx:0x%x whose chipid:0x%x, func_num:%d done\n",
			   act_flag, ctx, chipid, func_num);
	/* hif_sdio_deep_sleep_info_dmp(&g_hif_sdio_ds_info_list[0]); */

	return 0;
}

static _osal_inline_ INT32 hif_sdio_deep_sleep_ctrl(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT8 en_flag)
{
	UINT32 i = 0;
	UINT32 j = 0;
	INT32 ret = 0;

	UINT32 array_size = 0;
	UINT32 clt_info_size = 0;
	MTK_WCN_HIF_SDIO_DS_CLT_INFO *p_ds_clt_info = NULL;
	MTK_WCN_HIF_SDIO_DS_INFO *p_ds_info = NULL;
	UINT8 do_ds_op_flag = 0;

	array_size = ARRAY_SIZE(g_hif_sdio_ds_info_list);


	/*search write index */
	for (i = 0; i < array_size; i++) {
		mutex_lock(&g_hif_sdio_ds_info_list[i].lock);
		/* hif_sdio_deep_sleep_info_dmp(&g_hif_sdio_ds_info_list[i]); */
		clt_info_size = ARRAY_SIZE(g_hif_sdio_ds_info_list[i].clt_info);

		for (j = 0; j < clt_info_size; j++) {
			if (g_hif_sdio_ds_info_list[i].clt_info[j].ctx == ctx) {
				do_ds_op_flag = 1;
				break;
			}
		}

		if (0 != do_ds_op_flag)
			break;
		mutex_unlock(&g_hif_sdio_ds_info_list[i].lock);
	}

	if ((i >= array_size) || (j >= clt_info_size)) {
		HIF_SDIO_DBG_FUNC("no valid ds info found for ctx 0x%08x\n, en_flag:%d", ctx,
				  en_flag);
		return -1;
	}
	HIF_SDIO_DBG_FUNC("valid ds info found for ctx 0x%08x, en_flag:%d\n", ctx, en_flag);

	p_ds_info = &g_hif_sdio_ds_info_list[i];
	p_ds_clt_info = &p_ds_info->clt_info[j];

	if (0 != p_ds_clt_info->act_flag)
		p_ds_clt_info->ds_en_flag = en_flag;
	else
		HIF_SDIO_DBG_FUNC("!!!!!----this case should never happen----!!!!!\n");

	/*check if deep sleep operation is needed or not */
	do_ds_op_flag = 1;
	for (j = 0; j < clt_info_size; j++) {
		if ((p_ds_info->clt_info[j].ds_en_flag == 0)
		    && (p_ds_info->clt_info[j].act_flag == 1)) {
			do_ds_op_flag = 0;
			break;
		}
	}
	if (0 != do_ds_op_flag) {

#if 0
		ret = mtk_wcn_hif_sdio_f0_writeb(ctx, p_ds_info->reg_offset, p_ds_info->value);
		if (0 == ret) {
			func = hif_sdio_ctx_to_func(ctx);
			HIF_SDIO_DBG_FUNC("msdc_sdio_deep_sleep++\n");
			msdc_sdio_deep_sleep(func->card->host, 0);
			HIF_SDIO_DBG_FUNC("msdc_sdio_deep_sleep--\n");

			HIF_SDIO_DBG_FUNC
			    ("write deep sleep register:0x%x with value:0x%x succeed\n",
			     p_ds_info->reg_offset, p_ds_info->value);
		} else {
			HIF_SDIO_ERR_FUNC("write deep sleep register:0x%x with value:0x%x failed\n",
					  p_ds_info->reg_offset, p_ds_info->value);
		}
#endif
	} else
		HIF_SDIO_DBG_FUNC("no need to do deep sleep operation\n");

	mutex_unlock(&g_hif_sdio_ds_info_list[i].lock);

	return ret;
}

/*!
 * \brief MTK hif sdio client registration function
 *
 * Client uses this function to register itself to hif_sdio driver
 *
 * \param pinfo a pointer of client's information
 *
 * \retval 0 register successfully
 * \retval < 0 list error code here
 */
INT32 mtk_wcn_hif_sdio_client_reg(const MTK_WCN_HIF_SDIO_CLTINFO *pinfo)
{
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 clt_index = -1;
	UINT32 i = 0;
	UINT32 j = 0;
	MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO *clt_probe_worker_info = 0;

	HIF_SDIO_DBG_FUNC("start!\n");
	/* 4 <1> check input pointer is valid */
	HIF_SDIO_ASSERT(pinfo);

	/* 4 <2> check if input parameters are all supported and valid */
	for (i = 0; i < pinfo->func_tbl_size; i++) {
		ret =
		    hif_sdio_check_supported_sdio_id(pinfo->func_tbl[i].manf_id,
						     pinfo->func_tbl[i].card_id);
		if (ret) {
			HIF_SDIO_WARN_FUNC
			    ("vendor id(0x%x) and device id(0x%x) of sdio_func are not supported!\n",
			     pinfo->func_tbl[i].manf_id, pinfo->func_tbl[i].card_id);
			goto out;
		}
	}
	HIF_SDIO_DBG_FUNC("hif_sdio_check_supported_sdio_id() done!\n");

	/* 4 <3> check if the specific {manf id, card id, function number} tuple is */
	/* 4 already resigstered */
	for (i = 0; i < pinfo->func_tbl_size; i++) {
		ret =
		    hif_sdio_check_duplicate_sdio_id(pinfo->func_tbl[i].manf_id,
						     pinfo->func_tbl[i].card_id,
						     pinfo->func_tbl[i].func_num);
		if (ret) {
			HIF_SDIO_WARN_FUNC("vendor id(0x%x), device id(0x%x), and fun_num(%d) of\n",
					pinfo->func_tbl[i].manf_id, pinfo->func_tbl[i].card_id,
					pinfo->func_tbl[i].func_num);
			HIF_SDIO_WARN_FUNC("sdio_func are duplicated in g_hif_sdio_clt_drv_list!\n");
			goto out;
		}
	}
	HIF_SDIO_DBG_FUNC("hif_sdio_check_duplicate_sdio_id() done!\n");

	/* 4 <4> add the specified {manf id, card id, function number}
	 * tuple to registered client list */
	HIF_SDIO_DBG_FUNC("pinfo->func_tbl_size:%d\n", pinfo->func_tbl_size);
	for (i = 0; i < pinfo->func_tbl_size; i++) {
		ret = hif_sdio_add_clt_list(&clt_index, pinfo, i);
		if (ret) {
			HIF_SDIO_WARN_FUNC
			    ("client's info are added in registed client list failed (buffer is full)!\n");
			goto out;
		}
		HIF_SDIO_DBG_FUNC("hif_sdio_add_clt_list() done (gRefCount=%d)!\n", gRefCount);

		/* 4 <5> if the specific {manf id, card id, function number} tuple has already */
		/* 4 been probed by mmc, schedule another task to call client's .hif_clt_probe() */
		for (j = 0; j < CFG_CLIENT_COUNT; j++) {
			/* probed spin lock */
			spin_lock_bh(&g_hif_sdio_lock_info.probed_list_lock);
			if (g_hif_sdio_probed_func_list[j].func == 0) {
				/* probed spin unlock */
				spin_unlock_bh(&g_hif_sdio_lock_info.probed_list_lock);
				continue;
			}
			/* the function has been probed */
			if ((g_hif_sdio_clt_drv_list[clt_index].func_info->manf_id ==
			     g_hif_sdio_probed_func_list[j].func->vendor)
			    && (g_hif_sdio_clt_drv_list[clt_index].func_info->card_id ==
				g_hif_sdio_probed_func_list[j].func->device)
			    && (g_hif_sdio_clt_drv_list[clt_index].func_info->func_num ==
				g_hif_sdio_probed_func_list[j].func->num)) {
				g_hif_sdio_probed_func_list[j].clt_idx = clt_index;
				/* probed spin unlock */
				spin_unlock_bh(&g_hif_sdio_lock_info.probed_list_lock);

				/* use worker thread to perform the client's .hif_clt_probe() */
				clt_probe_worker_info =
				    vmalloc(sizeof(MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO));
				if (clt_probe_worker_info) {
					INIT_WORK(&clt_probe_worker_info->probe_work,
						  hif_sdio_clt_probe_worker);
					clt_probe_worker_info->registinfo_p =
					    &g_hif_sdio_clt_drv_list[clt_index];
					clt_probe_worker_info->probe_idx = j;
					schedule_work(&clt_probe_worker_info->probe_work);
				}
				/* 4 <5.1> remember to do claim_irq for the func if it's irq had been released. */
				if (!(g_hif_sdio_probed_func_list[j].func->irq_handler)) {
					sdio_claim_host(g_hif_sdio_probed_func_list[j].func);
					ret =
					    sdio_claim_irq(g_hif_sdio_probed_func_list[j].func,
							   hif_sdio_irq);
					mtk_wcn_hif_sdio_irq_flag_set(1);
					sdio_release_host(g_hif_sdio_probed_func_list[j].func);
					HIF_SDIO_INFO_FUNC
					    ("sdio_claim_irq for func(0x%p) j(%d) v(0x%x) d(0x%x) ok\n",
					     g_hif_sdio_probed_func_list[j].func, j,
					     g_hif_sdio_probed_func_list[j].func->vendor,
					     g_hif_sdio_probed_func_list[j].func->device);
				}
				/* 4 <5.2> Reset the block size of the function provided by client */
				HIF_SDIO_INFO_FUNC("Reset sdio block size: %d!\n",
						   g_hif_sdio_clt_drv_list[clt_index].
						   func_info->blk_sz);
				sdio_claim_host(g_hif_sdio_probed_func_list[j].func);
				ret = sdio_set_block_size(g_hif_sdio_probed_func_list[j].func,
							  g_hif_sdio_clt_drv_list
							  [clt_index].func_info->blk_sz);
				sdio_release_host(g_hif_sdio_probed_func_list[j].func);
			} else {
				/* probed spin unlock */
				spin_unlock_bh(&g_hif_sdio_lock_info.probed_list_lock);
			}
		}
		HIF_SDIO_DBG_FUNC
		    ("map g_hif_sdio_clt_drv_list to g_hif_sdio_probed_func_list done!\n");
	}
	ret = HIF_SDIO_ERR_SUCCESS;
	gRefCount++;

out:
	/* 4 <last> error handling */

	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_client_reg() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_client_reg);

/*!
 * \brief MTK hif sdio client un-registration function
 *
 * Client uses this function to un-register itself
 *
 * \param pinfo a pointer of client's information
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_client_unreg(const MTK_WCN_HIF_SDIO_CLTINFO *pinfo)
{
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 clt_list_index = 0;
	UINT32 i = 0;
	UINT32 j = 0;

	HIF_SDIO_INFO_FUNC("start!\n");

	/* 4 <1> check if input pointer is valid */
	HIF_SDIO_ASSERT(pinfo);

	/* 4 <2> check if input parameters are all supported and valid */
	for (i = 0; i < pinfo->func_tbl_size; i++) {
		ret =
		    hif_sdio_check_supported_sdio_id(pinfo->func_tbl[i].manf_id,
						     pinfo->func_tbl[i].card_id);
		if (ret) {
			HIF_SDIO_WARN_FUNC
			    ("vendor id(0x%x) and device id(0x%x) of sdio_func are not supported in mtk_sdio_id_tbl!\n",
			     pinfo->func_tbl[i].manf_id, pinfo->func_tbl[i].card_id);
			goto out;
		}
	}

	/* 4 <3> check if the specific {manf id, card id, function number} tuple is already resigstered */
	/* 4 and find the corresponding client ctx and call client's .hif_clt_remove() in THIS context */
	for (i = 0; i < pinfo->func_tbl_size; i++) {
		clt_list_index =
		    hif_sdio_find_clt_list_index(pinfo->func_tbl[i].manf_id,
						 pinfo->func_tbl[i].card_id,
						 pinfo->func_tbl[i].func_num);
		if (clt_list_index < 0) {
			HIF_SDIO_WARN_FUNC("vendor id(0x%x),", pinfo->func_tbl[i].manf_id);
			HIF_SDIO_WARN_FUNC(" device id(0x%x),", pinfo->func_tbl[i].card_id);
			HIF_SDIO_WARN_FUNC(" and fun_num(%d)", pinfo->func_tbl[i].func_num);
			HIF_SDIO_WARN_FUNC(" client info is not in the client's registed list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
		/* 4 <4> mark the specified {manf id, card id, function number} tuple as */
		/* 4 un-registered and invalidate client's context */
		hif_sdio_init_clt_list(clt_list_index);

		/* un-map g_hif_sdio_clt_drv_list index in g_hif_sdio_probed_func_list */
		for (j = 0; j < CFG_CLIENT_COUNT; j++) {
			if (g_hif_sdio_probed_func_list[j].clt_idx == clt_list_index)
				g_hif_sdio_probed_func_list[j].clt_idx = -1;
		}
	}
	gRefCount--;

	ret = HIF_SDIO_ERR_SUCCESS;
out:
	HIF_SDIO_INFO_FUNC("end (gRefCount=%d) !\n", gRefCount);
	return ret;
}				/* end of mtk_wcn_hif_sdio_client_unreg() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_client_unreg);

/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_readb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, PUINT8 pvb)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");
	HIF_SDIO_ASSERT(pvb);

	/* 4 <1> check if ctx is valid, registered, and probed */
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		goto out;
	}
	if (probe_index < 0 || probe_index >= CFG_CLIENT_COUNT) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif

	/* 4 <2> */
	osal_ftrace_print("%s|S\n", __func__);
	sdio_claim_host(func);
	*pvb = sdio_readb(func, offset, &ret);
	sdio_release_host(func);
	osal_ftrace_print("%s|E\n", __func__);

	/* 4 <3> check result code and return proper error code */

out:
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_client_unreg() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_readb);

/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_writeb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, UINT8 vb)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");

	/* 4 <1> check if ctx is valid, registered, and probed */
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		goto out;
	}
	if (probe_index < 0 || probe_index >= CFG_CLIENT_COUNT) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif

	/* 4 <1.1> check if input parameters are valid */

	/* 4 <2> */
	wmt_tra_sdio_update();
	osal_ftrace_print("%s|S\n", __func__);
	sdio_claim_host(func);
	sdio_writeb(func, vb, offset, &ret);
	sdio_release_host(func);
	osal_ftrace_print("%s|E\n", __func__);

	/* 4 <3> check result code and return proper error code */

out:
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_client_unreg() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_writeb);

/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_readl(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, PUINT32 pvl)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");
	HIF_SDIO_ASSERT(pvl);

	/* 4 <1> check if ctx is valid, registered, and probed */
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		goto out;
	}
	if (probe_index < 0 || probe_index >= CFG_CLIENT_COUNT) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif
	/* 4 <1.1> check if input parameters are valid */

	/* 4 <2> */
	osal_ftrace_print("%s|S\n", __func__);
	sdio_claim_host(func);
	*pvl = sdio_readl(func, offset, &ret);
	sdio_release_host(func);
	osal_ftrace_print("%s|E\n", __func__);

	/* 4 <3> check result code and return proper error code */

out:
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_client_unreg() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_readl);

/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_writel(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, UINT32 vl)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");

	/* 4 <1> check if ctx is valid, registered, and probed */
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		goto out;
	}
	if (probe_index < 0 || probe_index >= CFG_CLIENT_COUNT) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif
	/* 4 <1.1> check if input parameters are valid */

	/* 4 <2> */
	wmt_tra_sdio_update();
	osal_ftrace_print("%s|S\n", __func__);
	sdio_claim_host(func);
	sdio_writel(func, vl, offset, &ret);
	sdio_release_host(func);
	osal_ftrace_print("%s|E\n", __func__);

	/* 4 <3> check result code and return proper error code */

out:
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_client_unreg() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_writel);

/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_read_buf(MTK_WCN_HIF_SDIO_CLTCTX ctx,
				UINT32 offset, PUINT32 pbuf, UINT32 len)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");
	HIF_SDIO_ASSERT(pbuf);

	/* 4 <1> check if ctx is valid, registered, and probed */
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		goto out;
	}
	if (probe_index < 0 || probe_index >= CFG_CLIENT_COUNT) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif
	/* 4 <1.1> check if input parameters are valid */

	/* 4 <2> */
	osal_ftrace_print("%s|S|L|%d\n", __func__, len);
	sdio_claim_host(func);
	ret = sdio_readsb(func, pbuf, offset, len);
	sdio_release_host(func);
	osal_ftrace_print("%s|E|L|%d\n", __func__, len);

	/* 4 <3> check result code and return proper error code */

out:
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_read_buf() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_read_buf);


/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_write_buf(MTK_WCN_HIF_SDIO_CLTCTX ctx,
				 UINT32 offset, PUINT32 pbuf, UINT32 len)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");
	HIF_SDIO_ASSERT(pbuf);

	/* 4 <1> check if ctx is valid, registered, and probed */
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		goto out;
	}
	if (probe_index < 0 || probe_index >= CFG_CLIENT_COUNT) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif
	/* 4 <1.1> check if input parameters are valid */

	/* 4 <2> */
	wmt_tra_sdio_update();
	osal_ftrace_print("%s|S|L|%d\n", __func__, len);
	sdio_claim_host(func);
	ret = sdio_writesb(func, offset, pbuf, len);
	sdio_release_host(func);
	osal_ftrace_print("%s|E|L|%d\n", __func__, len);

	/* 4 <3> check result code and return proper error code */

out:
	HIF_SDIO_DBG_FUNC("ret(%d) end!\n", ret);

	return ret;
}				/* end of mtk_wcn_hif_sdio_write_buf() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_write_buf);

/*!
 * \brief store client driver's private data function.
 *
 *
 * \param clent's MTK_WCN_HIF_SDIO_CLTCTX.
 *
 * \retval none.
 */
VOID mtk_wcn_hif_sdio_set_drvdata(MTK_WCN_HIF_SDIO_CLTCTX ctx, PVOID private_data_p)
{
	UINT8 probed_idx = CLTCTX_IDX(ctx);

	if (unlikely(!CLTCTX_IDX_VALID(probed_idx))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid idx in ctx(0x%x), private_data_p not stored!\n", ctx);
	} else {
		/* store client driver's private data to dev driver */
		g_hif_sdio_probed_func_list[probed_idx].private_data_p = private_data_p;
		HIF_SDIO_DBG_FUNC("private_data_p(0x%p) for ctx(0x%x) probed idx(%d) stored!\n",
				  private_data_p, ctx, probed_idx);
	}
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_set_drvdata);

/*!
 * \brief get client driver's private data function.
 *
 *
 * \param clent's MTK_WCN_HIF_SDIO_CLTCTX.
 *
 * \retval private data pointer.
 */
PVOID mtk_wcn_hif_sdio_get_drvdata(MTK_WCN_HIF_SDIO_CLTCTX ctx)
{
	UINT8 probed_idx = CLTCTX_IDX(ctx);

	/* get client driver's private data to dev driver */
	if (likely(CLTCTX_IDX_VALID(probed_idx)))
		return g_hif_sdio_probed_func_list[probed_idx].private_data_p;
	/* invalid index in CLTCTX */
	HIF_SDIO_WARN_FUNC("invalid idx in ctx(0x%x), return null!\n", ctx);
	return NULL;
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_get_drvdata);

/*!
 * \brief control stp/wifi on/off from wmt.
 *
 *
 * \param (1)control function type, (2)on/off control.
 *
 * \retval (1)control results ,(2)unknown type: -5.
 * \retval 0:success, -11:not probed, -12:already on, -13:not registered, other errors.
 */
INT32 mtk_wcn_hif_sdio_wmt_control(WMT_SDIO_FUNC_TYPE func_type, MTK_WCN_BOOL is_on)
{
	/* TODO:[FixMe][George]: return value of this function shall distinguish */
	/* 1) not probed by mmc_core yet or */
	/* 2) probed by mmc_core but init fail... */
	switch (func_type) {
	case WMT_SDIO_FUNC_STP:
		if (is_on == MTK_WCN_BOOL_TRUE)
			return hif_sdio_stp_on();
		else
			return hif_sdio_stp_off();
		break;

	case WMT_SDIO_FUNC_WIFI:
		if (is_on == MTK_WCN_BOOL_TRUE)
			return hif_sdio_wifi_on();
		else
			return hif_sdio_wifi_off();
		break;

	default:
		HIF_SDIO_WARN_FUNC("unknown type(%d)\n", func_type);
		return HIF_SDIO_ERR_INVALID_PARAM;
	}
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_wmt_control);

/*!
 * \brief ???
 *
 * \detail ???
 *
 * \param ctx a context provided by client driver
 * \param struct device ** ???
 *
 * \retval none
 */
VOID mtk_wcn_hif_sdio_get_dev(MTK_WCN_HIF_SDIO_CLTCTX ctx, struct device **dev)
{
#if HIF_SDIO_UPDATE
	struct sdio_func *func;
#else
	UINT8 probe_index = CLTCTX_IDX(ctx);
#endif

#if HIF_SDIO_UPDATE
	*dev = NULL;		/* ensure we does not return any invalid value back. */
	func = hif_sdio_ctx_to_func(ctx);
	if (unlikely(!func)) {
		HIF_SDIO_WARN_FUNC("no valid *func with ctx(0x%x)\n", ctx);
		return;
	}
	*dev = &(func->dev);
	HIF_SDIO_DBG_FUNC("return *dev(0x%p) for ctx(0x%x)\n", *dev, ctx);
#else
	if (probe_index < 0) {
		HIF_SDIO_WARN_FUNC("func not probed, probe_index = %d", probe_index);
		return;
	}
	*dev = &g_hif_sdio_probed_func_list[probe_index].func->dev;
#endif
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_get_dev);

/*!
 * \brief client's probe() function.
 *
 *
 * \param work queue structure.
 *
 * \retval none.
 */
static _osal_inline_ INT32 hif_sdio_clt_probe_func(MTK_WCN_HIF_SDIO_REGISTINFO *registinfo_p, INT8 probe_idx)
{
	UINT16 card_id = 0;
	UINT16 func_num = 0;
	UINT16 blk_sz = 0;
	INT32 ret;

	HIF_SDIO_DBG_FUNC("start!\n");
	HIF_SDIO_ASSERT(registinfo_p);
	if (!registinfo_p) {
		HIF_SDIO_WARN_FUNC("registinfo_p NULL!!!\n");
		return -1;
	}

	/* special case handling: if the clt's unregister is called during probe procedures */
	if (!registinfo_p->func_info || !registinfo_p->sdio_cltinfo) {
		HIF_SDIO_WARN_FUNC("client's registinfo_p is cleared !!!\n");
		return -1;
	}

	card_id = registinfo_p->func_info->card_id;
	func_num = registinfo_p->func_info->func_num;
	blk_sz = registinfo_p->func_info->blk_sz;
	ret =
	    registinfo_p->sdio_cltinfo->hif_clt_probe(CLTCTX(card_id, func_num, blk_sz, probe_idx),
						      registinfo_p->func_info);

	HIF_SDIO_INFO_FUNC
	    ("clt_probe_func card_id(%x) func_num(%x) blk_sz(%d) prob_idx(%x) ret(%d) %s\n",
	     card_id, func_num, blk_sz, probe_idx, ret, (ret) ? "fail" : "ok");

	return ret;
}

/*!
 * \brief client's probe() worker.
 *
 *
 * \param work queue structure.
 *
 * \retval none.
 */
static VOID hif_sdio_clt_probe_worker(struct work_struct *work)
{
	MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO *clt_worker_info_p = 0;
	UINT16 card_id = 0;
	UINT16 func_num = 0;
	UINT16 blk_sz = 0;
	INT8 prob_idx = 0;

	HIF_SDIO_DBG_FUNC("start!\n");

	HIF_SDIO_ASSERT(work);

	/* get client's information */
	clt_worker_info_p = container_of(work, MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO, probe_work);
	HIF_SDIO_ASSERT(clt_worker_info_p);
	HIF_SDIO_ASSERT(clt_worker_info_p->registinfo_p);

	/* special case handling: if the clt's unregister is called during probe procedures */
	if ((clt_worker_info_p->registinfo_p->func_info == 0)
	    || (clt_worker_info_p->registinfo_p->sdio_cltinfo == 0)) {
		HIF_SDIO_WARN_FUNC("client's registinfo_p is cleared !!!\n");
		vfree(clt_worker_info_p);
		return;
	}

	card_id = clt_worker_info_p->registinfo_p->func_info->card_id;
	func_num = clt_worker_info_p->registinfo_p->func_info->func_num;
	blk_sz = clt_worker_info_p->registinfo_p->func_info->blk_sz;
	prob_idx = clt_worker_info_p->probe_idx;

	/* Execute client's probe() func */
	clt_worker_info_p->registinfo_p->
	    sdio_cltinfo->hif_clt_probe(CLTCTX(card_id, func_num, blk_sz, prob_idx),
					clt_worker_info_p->registinfo_p->func_info);

	vfree(clt_worker_info_p);

	HIF_SDIO_DBG_FUNC("card_id(0x%x) func_num(0x%x) blk_sz(0x%x) prob_idx(0x%x)\n", card_id,
			  func_num, blk_sz, prob_idx);
	HIF_SDIO_DBG_FUNC("end!\n");
}

/*!
 * \brief client's probe() worker.
 *
 *
 * \param work queue structure.
 *
 * \retval none.
 */
static _osal_inline_ VOID hif_sdio_dump_probe_list(VOID)
{
	INT32 i;

	HIF_SDIO_DBG_FUNC("== DUMP probed list start ==\n");

	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		if (g_hif_sdio_probed_func_list[i].func) {
			HIF_SDIO_DBG_FUNC("index(%d) func(0x%p) clt_idx(%d)\n",
					  i, g_hif_sdio_probed_func_list[i].func,
					  g_hif_sdio_probed_func_list[i].clt_idx);

			HIF_SDIO_DBG_FUNC("vendor(0x%x) device(0x%x) num(0x%x) state(%d)\n",
					  g_hif_sdio_probed_func_list[i].func->vendor,
					  g_hif_sdio_probed_func_list[i].func->device,
					  g_hif_sdio_probed_func_list[i].func->num,
					  g_hif_sdio_probed_func_list[i].on_by_wmt);

		}
	}

	HIF_SDIO_DBG_FUNC("== DUMP probed list end ==\n");
}


/*!
 * \brief Initialize g_hif_sdio_probed_func_list
 *
 *
 * \param index of g_hif_sdio_probed_func_list.
 *
 * \retval none.
 */
static _osal_inline_ VOID hif_sdio_init_probed_list(INT32 index)
{
	if ((index >= 0) && (index < CFG_CLIENT_COUNT)) {
		/* probed spin lock */
		spin_lock_bh(&g_hif_sdio_lock_info.probed_list_lock);
		g_hif_sdio_probed_func_list[index].func = 0;
		g_hif_sdio_probed_func_list[index].clt_idx = -1;
		g_hif_sdio_probed_func_list[index].private_data_p = 0;
		g_hif_sdio_probed_func_list[index].on_by_wmt = MTK_WCN_BOOL_FALSE;
		/* probed spin unlock */
		spin_unlock_bh(&g_hif_sdio_lock_info.probed_list_lock);
	} else
		HIF_SDIO_ERR_FUNC("index is out of g_hif_sdio_probed_func_list[] boundary!\n");
}


/*!
 * \brief Initialize g_hif_sdio_clt_drv_list
 *
 *
 * \param index of g_hif_sdio_clt_drv_list.
 *
 * \retval none.
 */
static _osal_inline_ VOID hif_sdio_init_clt_list(INT32 index)
{
	/* client list spin lock */
	spin_lock_bh(&g_hif_sdio_lock_info.clt_list_lock);
	if ((index >= 0) && (index < CFG_CLIENT_COUNT)) {
		g_hif_sdio_clt_drv_list[index].sdio_cltinfo = 0;
		g_hif_sdio_clt_drv_list[index].func_info = 0;
	} else
		HIF_SDIO_ERR_FUNC("index is out of g_hif_sdio_clt_drv_list[] boundary!\n");
	/* client list spin unlock */
	spin_unlock_bh(&g_hif_sdio_lock_info.clt_list_lock);
}


/*!
 * \brief find matched g_hif_sdio_probed_func_list index from sdio function handler
 *
 *
 * \param sdio function handler
 *
 * \retval -1    index not found
 * \retval >= 0  return found index
 */
static _osal_inline_ INT32 hif_sdio_find_probed_list_index_by_func(struct sdio_func *func)
{
	INT32 i = 0;

	HIF_SDIO_ASSERT(func);

	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		if (g_hif_sdio_probed_func_list[i].func == func)
			return i;
	}

	return -1;
}

/*!
 * \brief find matched g_hif_sdio_probed_func_list from vendor_id, device_id, and function number
 *
 *
 * \param vendor id, device id, and function number of the sdio card.
 *
 * \retval -1    index not found
 * \retval >= 0  return found index
 */
static _osal_inline_ INT32 hif_sdio_find_probed_list_index_by_id_func(UINT16 vendor, UINT16 device,
							UINT16 func_num)
{
	INT32 i;

	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		if (g_hif_sdio_probed_func_list[i].func) {
			HIF_SDIO_DBG_FUNC("probed entry: vendor(0x%x) device(0x%x) num(0x%x)\n",
					  g_hif_sdio_probed_func_list[i].func->vendor,
					  g_hif_sdio_probed_func_list[i].func->device,
					  g_hif_sdio_probed_func_list[i].func->num);
		}
	}
	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		if (!g_hif_sdio_probed_func_list[i].func) {
			continue;
		} else if ((g_hif_sdio_probed_func_list[i].func->vendor == vendor) &&
			   (g_hif_sdio_probed_func_list[i].func->device == device) &&
			   (g_hif_sdio_probed_func_list[i].func->num == func_num)) {
			return i;
		}
	}

	if (i == CFG_CLIENT_COUNT) {
		/*
		   pr_warn(DRV_NAME "Cannot find vendor:0x%x, device:0x%x, func_num:0x%x, i=%d\n",
		   vendor, device, func_num, i);
		 */
		/* client func has not been probed */
		return -1;
	}
	return -1;
}

/*!
 * \brief find matched g_hif_sdio_clt_drv_list index
 *
 * find the matched g_hif_sdio_clt_drv_list index from card_id and function number.
 *
 * \param vendor id, device id, and function number of the sdio card
 *
 * \retval -1    index not found
 * \retval >= 0  return found index
 */
static _osal_inline_ INT32 hif_sdio_find_clt_list_index(UINT16 vendor, UINT16 device, UINT16 func_num)
{
	INT32 i = 0;

	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		if (g_hif_sdio_clt_drv_list[i].func_info != 0) {
			if ((g_hif_sdio_clt_drv_list[i].func_info->manf_id == vendor) &&
			    (g_hif_sdio_clt_drv_list[i].func_info->card_id == device) &&
			    (g_hif_sdio_clt_drv_list[i].func_info->func_num == func_num)) {
				return i;
			}
		}
	}

	return -1;
}


/*!
 * \brief check if the vendor, device ids are supported in mtk_sdio_id_tbl.
 *
 *
 * \param vendor id and device id of the sdio card
 *
 * \retval (-HIF_SDIO_ERR_FAIL)  vendor, device ids are not supported
 * \retval HIF_SDIO_ERR_SUCCESS  vendor, device ids are supported
 */
static _osal_inline_ INT32 hif_sdio_check_supported_sdio_id(UINT16 vendor, UINT16 device)
{
	INT32 i = 0;

	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		if ((mtk_sdio_id_tbl[i].vendor == vendor) && (mtk_sdio_id_tbl[i].device == device))
			return HIF_SDIO_ERR_SUCCESS;	/* mtk_sdio_id is supported */
	}
	return -HIF_SDIO_ERR_FAIL;	/* mtk_sdio_id is not supported */
}


/*!
 * \brief check if the vendor, device ids are duplicated in g_hif_sdio_clt_drv_list.
 *
 *
 * \param vendor id, device id, and function number of the sdio card
 *
 * \retval (-HIF_SDIO_ERR_DUPLICATED)  vendor, device, func_num are duplicated
 * \retval HIF_SDIO_ERR_SUCCESS        vendor, device, func_num are not duplicated
 */
static _osal_inline_ INT32 hif_sdio_check_duplicate_sdio_id(UINT16 vendor, UINT16 device, UINT16 func_num)
{
	INT32 i = 0;

	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		if (g_hif_sdio_clt_drv_list[i].func_info != 0) {
			if ((g_hif_sdio_clt_drv_list[i].func_info->manf_id == vendor) &&
			    (g_hif_sdio_clt_drv_list[i].func_info->card_id == device) &&
			    (g_hif_sdio_clt_drv_list[i].func_info->func_num == func_num)) {
				return -HIF_SDIO_ERR_DUPLICATED;	/* duplicated */
			}
		}
	}
	return HIF_SDIO_ERR_SUCCESS;	/* Not duplicated */
}


/*!
 * \brief Add the client info into g_hif_sdio_clt_drv_list.
 *
 *
 * \param [output] client's index pointer.
 * \param MTK_WCN_HIF_SDIO_CLTINFO of client's contex.
 *
 * \retval (-HIF_SDIO_ERR_FAIL)  Add to clt_list successfully
 * \retval HIF_SDIO_ERR_SUCCESS  Add to clt_list failed (buffer is full)
 */
static _osal_inline_ INT32 hif_sdio_add_clt_list(PINT32 clt_index_p,
				   const MTK_WCN_HIF_SDIO_CLTINFO *pinfo, UINT32 tbl_index)
{
	INT32 i = 0;

	HIF_SDIO_ASSERT(clt_index_p);
	HIF_SDIO_ASSERT(pinfo);

	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		/* client list spin lock */
		spin_lock_bh(&g_hif_sdio_lock_info.clt_list_lock);
		if (g_hif_sdio_clt_drv_list[i].func_info == 0) {
			g_hif_sdio_clt_drv_list[i].func_info = &(pinfo->func_tbl[tbl_index]);
			g_hif_sdio_clt_drv_list[i].sdio_cltinfo = pinfo;
			/* client list spin unlock */
			spin_unlock_bh(&g_hif_sdio_lock_info.clt_list_lock);
			*clt_index_p = i;
			return HIF_SDIO_ERR_SUCCESS;	/* Add to client list successfully */
		}
		/* client list spin unlock */
		spin_unlock_bh(&g_hif_sdio_lock_info.clt_list_lock);
	}
	return -HIF_SDIO_ERR_FAIL;	/* Add to client list failed (buffer is full) */
}

#if HIF_SDIO_SUPPORT_SUSPEND
static INT32 hif_sdio_suspend(struct device *dev)
{
	struct sdio_func *func;
	mmc_pm_flag_t flag;
	INT32 ret;

	if (!dev)
		return -EINVAL;

	func = dev_to_sdio_func(dev);
	HIF_SDIO_DBG_FUNC("prepare for func(0x%p)\n", func);
	flag = sdio_get_host_pm_caps(func);
#if HIF_SDIO_SUPPORT_WAKEUP
	if (!(flag & MMC_PM_KEEP_POWER) || !(flag & MMC_PM_WAKE_SDIO_IRQ)) {
		HIF_SDIO_WARN_FUNC
		    ("neither MMC_PM_KEEP_POWER or MMC_PM_WAKE_SDIO_IRQ is supported by host, return -ENOTSUPP\n");
		return -ENOTSUPP;
	}

	/* set both */
	flag |= MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;
#else
	if (!(flag & MMC_PM_KEEP_POWER)) {
		HIF_SDIO_WARN_FUNC
		    ("neither MMC_PM_KEEP_POWER is supported by host, return -ENOTSUPP\n");
		return -ENOTSUPP;
	}
	flag |= MMC_PM_KEEP_POWER;
#endif
	ret = sdio_set_host_pm_flags(func, flag);
	if (ret) {
		HIF_SDIO_INFO_FUNC
		    ("set MMC_PM_KEEP_POWER to host fail(%d)\n", ret);
		return -EFAULT;
	}
#if HIF_SDIO_SUPPORT_WAKEUP
	sdio_claim_host(func);
#endif
	HIF_SDIO_INFO_FUNC("set MMC_PM_KEEP_POWER ok\n");
	return 0;
}

static INT32 hif_sdio_resume(struct device *dev)
{
#if HIF_SDIO_SUPPORT_WAKEUP
	struct sdio_func *func;
#endif
	if (!dev) {
		HIF_SDIO_WARN_FUNC("null dev!\n");
		return -EINVAL;
	}
#if HIF_SDIO_SUPPORT_WAKEUP
	func = dev_to_sdio_func(dev);
	sdio_release_host(func);
#endif
	HIF_SDIO_INFO_FUNC("do nothing\n");

	return 0;
}
#endif

/*!
 * \brief hif_sdio probe function
 *
 * hif_sdio probe function called by mmc driver when any matched SDIO function
 * is detected by it.
 *
 * \param func
 * \param id
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
static INT32 hif_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	INT32 ret = 0;
	INT32 i = 0;
	MTK_WCN_HIF_SDIO_PROBEINFO *hif_sdio_probed_funcp = 0;
	INT32 probe_index = -1;
	INT32 idx;
#if 0
	INT32 clt_index = -1;
	MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO *clt_probe_worker_info = 0;
#endif

	HIF_SDIO_INFO_FUNC("start!\n");
	HIF_SDIO_ASSERT(func);
#if !(DELETE_HIF_SDIO_CHRDEV)
	hif_sdio_match_chipid_by_dev_id(id);
#endif
	/* 4 <0> display debug information */
	HIF_SDIO_INFO_FUNC("vendor(0x%x) device(0x%x) num(0x%x)\n", func->vendor, func->device,
			   func->num);
	for (i = 0; i < func->card->num_info; i++)
		HIF_SDIO_INFO_FUNC("card->info[%d]: %s\n", i, func->card->info[i]);

	/* 4 <1> Check if this  is supported by us (mtk_sdio_id_tbl) */
	ret = hif_sdio_check_supported_sdio_id(func->vendor, func->device);
	if (ret) {
		HIF_SDIO_WARN_FUNC
		    ("vendor id and device id of sdio_func are not supported in mtk_sdio_id_tbl!\n");
		goto out;
	}
	/* 4 <2> Add this struct sdio_func *func to g_hif_sdio_probed_func_list */
	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		/* probed spin lock */
		spin_lock_bh(&g_hif_sdio_lock_info.probed_list_lock);
		if (g_hif_sdio_probed_func_list[i].func == 0) {
			hif_sdio_probed_funcp = &g_hif_sdio_probed_func_list[i];
			hif_sdio_probed_funcp->func = func;
			hif_sdio_probed_funcp->clt_idx =
			    hif_sdio_find_clt_list_index(func->vendor, func->device, func->num);
			hif_sdio_probed_funcp->on_by_wmt = MTK_WCN_BOOL_FALSE;
			hif_sdio_probed_funcp->sdio_irq_enabled = MTK_WCN_BOOL_FALSE;
			/* probed spin unlock */
			spin_unlock_bh(&g_hif_sdio_lock_info.probed_list_lock);
			probe_index = i;
			break;
		}
		/* probed spin unlock */
		spin_unlock_bh(&g_hif_sdio_lock_info.probed_list_lock);
	}
	if ((probe_index < 0) || (probe_index >= CFG_CLIENT_COUNT)) {
		HIF_SDIO_ERR_FUNC("probe function list if full!\n");
		goto out;
	}
	/* 4 <3> Initialize this function */
	if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {
		for (i = 0; i < CFG_CLIENT_COUNT; i++) {
			/* client list spin lock */
			spin_lock_bh(&g_hif_sdio_lock_info.clt_list_lock);
			if (g_hif_sdio_clt_drv_list[i].func_info == 0) {
				/* client list spin unlock */
				spin_unlock_bh(&g_hif_sdio_lock_info.clt_list_lock);
				continue;
			}
			HIF_SDIO_INFO_FUNC("manf_id:%x, card_id:%x, func_num:%d\n",
					   g_hif_sdio_clt_drv_list[i].func_info->manf_id,
					   g_hif_sdio_clt_drv_list[i].func_info->card_id,
					   g_hif_sdio_clt_drv_list[i].func_info->func_num);
			if ((g_hif_sdio_clt_drv_list[i].func_info->manf_id ==
			     g_hif_sdio_probed_func_list[probe_index].func->vendor)
			    && (g_hif_sdio_clt_drv_list[i].func_info->card_id ==
				g_hif_sdio_probed_func_list[probe_index].func->device)
			    && (g_hif_sdio_clt_drv_list[i].func_info->func_num ==
				g_hif_sdio_probed_func_list[probe_index].func->num)) {
				g_hif_sdio_probed_func_list[probe_index].clt_idx = i;
				/* client list spin unlock */
				spin_unlock_bh(&g_hif_sdio_lock_info.clt_list_lock);
				break;
			}
			/* client list spin unlock */
			spin_unlock_bh(&g_hif_sdio_lock_info.clt_list_lock);
		}
		HIF_SDIO_INFO_FUNC("map to g_hif_sdio_clt_drv_list[] done: %d\n",
				   g_hif_sdio_probed_func_list[probe_index].clt_idx);
	}
	/* 4 <3.1> enable this function */
	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	sdio_release_host(func);
	if (ret) {
		HIF_SDIO_ERR_FUNC("sdio_enable_func failed!\n");
		goto out;
	}
#if 0
	if (0 == hif_sdio_is_autok_support(func))
		/* hif_sdio_do_autok(func); */
#endif

	/* 4 <3.2> set block size according to the table storing function characteristics */
	if (hif_sdio_probed_funcp == 0) {
		HIF_SDIO_ERR_FUNC("hif_sdio_probed_funcp is null!\n");
		goto out;
	}
	if (hif_sdio_probed_funcp->clt_idx >= 0 &&
		hif_sdio_probed_funcp->clt_idx < CFG_CLIENT_COUNT) {
		/* The clt contex has been registed */
		sdio_claim_host(func);
		idx = hif_sdio_probed_funcp->clt_idx;
		ret = sdio_set_block_size(func, g_hif_sdio_clt_drv_list[idx].func_info->blk_sz);
		sdio_release_host(func);
	} else {		/* The clt contex has not been registed */

		sdio_claim_host(func);
		ret = sdio_set_block_size(func, HIF_DEFAULT_BLK_SIZE);
		sdio_release_host(func);
	}
	if (ret) {
		HIF_SDIO_ERR_FUNC("set sdio block size failed!\n");
		goto out;
	}

	HIF_SDIO_INFO_FUNC("cur_blksize(%d) max(%d), host max blk_size(%d) blk_count(%d)\n",
			   func->cur_blksize, func->max_blksize,
			   func->card->host->max_blk_size, func->card->host->max_blk_count);


	hif_sdio_dump_probe_list();

out:
	/* 4 <last> error handling */
	return ret;
}


/*!
 * \brief hif_sdio remove function
 *
 * hif_sdio probe function called by mmc driver when the probed func should be
 * removed.
 *
 * \param func
 *
 */
static VOID hif_sdio_remove(struct sdio_func *func)
{
	INT32 probed_list_index = 0;
#if 0
	INT32 registed_list_index = 0;
#endif

	HIF_SDIO_INFO_FUNC("start!\n");
	HIF_SDIO_ASSERT(func);

	/* 4 <1> check input parameter is valid and has been probed previously */
	if (func == NULL) {
		HIF_SDIO_ERR_FUNC("func null(%p)\n", func);
		return;
	}
	/* 4 <2> if this function has been initialized by any client driver, */
	/* 4 call client's .hif_clt_remove() call back in THIS context. */
	probed_list_index = hif_sdio_find_probed_list_index_by_func(func);
	if (probed_list_index < 0) {
		HIF_SDIO_WARN_FUNC
		    ("sdio function pointer is not in g_hif_sdio_probed_func_list!\n");
		return;
	}
#if 0
	registed_list_index = g_hif_sdio_probed_func_list[probed_list_index].clt_idx;
	if (registed_list_index >= 0) {
		g_hif_sdio_clt_drv_list[registed_list_index].sdio_cltinfo->hif_clt_remove(CLTCTX
											  (func->
											   device,
											   func->
											   num,
											   func->
											   cur_blksize,
											   probed_list_index));
	}
#endif

	/* 4 <3> mark this function as de-initialized and invalidate client's context */
	hif_sdio_init_probed_list(probed_list_index);

#if 0
	/* 4 <4> release irq for this function */
	sdio_claim_host(func);
	sdio_release_irq(func);
	sdio_release_host(func);
#endif

	/* 4 <5> disable this function */
	sdio_claim_host(func);
	sdio_disable_func(func);
	sdio_release_host(func);

	/* 4 <6> mark this function as removed */

	HIF_SDIO_INFO_FUNC("sdio func(0x%p) is removed successfully!\n", func);
}

/*!
 * \brief hif_sdio interrupt handler
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 */
static VOID hif_sdio_irq(struct sdio_func *func)
{
	INT32 probed_list_index = -1;
	INT32 registed_list_index = -1;

	HIF_SDIO_DBG_FUNC("start!\n");

	osal_ftrace_print("%s|S\n", __func__);
	/* 4 <1> check if func is valid */
	HIF_SDIO_ASSERT(func);

	/* 4 <2> if func has valid corresponding hif_sdio client's context, mark it */
	/* 4 host-locked, use it to call client's .hif_clt_irq() callback function in */
	/* 4 THIS context. */
	probed_list_index = hif_sdio_find_probed_list_index_by_func(func);
	if ((probed_list_index < 0) || (probed_list_index >= CFG_CLIENT_COUNT)) {
		HIF_SDIO_ERR_FUNC("probed_list_index not found!\n");
		return;
	}
	/* [George] added for sdio irq sync and mmc single_irq workaround. It's set
	 * enabled later by client driver call mtk_wcn_hif_sdio_enable_irq()
	 */
	/* skip smp_rmb() here */
	if (MTK_WCN_BOOL_FALSE == g_hif_sdio_probed_func_list[probed_list_index].sdio_irq_enabled) {
		HIF_SDIO_WARN_FUNC("func(0x%p),probed_idx(%d) sdio irq not enabled yet\n",
				   func, probed_list_index);
		return;
	}

	registed_list_index = g_hif_sdio_probed_func_list[probed_list_index].clt_idx;
/* g_hif_sdio_probed_func_list[probed_list_index].interrupted = MTK_WCN_BOOL_TRUE; */
	if ((registed_list_index >= 0)
	    && (registed_list_index < CFG_CLIENT_COUNT)) {
		HIF_SDIO_DBG_FUNC("[%d]SDIO IRQ (func:0x%p) v(0x%x) d(0x%x) n(0x%x)\n",
				  probed_list_index, func, func->vendor, func->device, func->num);

		hif_sdio_deep_sleep_ctrl(CLTCTX
					  (func->device, func->num, func->cur_blksize,
					   probed_list_index), 0);

		g_hif_sdio_clt_drv_list[registed_list_index].sdio_cltinfo->hif_clt_irq(CLTCTX
										       (func->
											device,
											func->num,
											func->
											cur_blksize,
											probed_list_index));
	} else {
		/* 4 <3> if func has no VALID hif_sdio client's context, release irq for this */
		/* 4 func and mark it in g_hif_sdio_probed_func_list (remember: donnot claim host in irq contex). */
		HIF_SDIO_WARN_FUNC("release irq (func:0x%p) v(0x%x) d(0x%x) n(0x%x)\n",
				   func, func->vendor, func->device, func->num);
		mtk_wcn_hif_sdio_irq_flag_set(0);
		sdio_release_irq(func);
	}
	osal_ftrace_print("%s|E\n", __func__);
}

/*!
 * \brief hif_sdio init function
 *
 * detailed descriptions
 *
 * \retval
 */
static INT32 hif_sdio_init(VOID)
{
	INT32 ret = 0;
	INT32 i = 0;

	HIF_SDIO_INFO_FUNC("start!\n");

	/* 4 <1> init all private variables */
	/* init reference count to 0 */
	gRefCount = 0;

	atomic_set(&hif_sdio_irq_enable_flag, 0);
	/* init spin lock information */
	spin_lock_init(&g_hif_sdio_lock_info.probed_list_lock);
	spin_lock_init(&g_hif_sdio_lock_info.clt_list_lock);

	/* init probed function list and g_hif_sdio_clt_drv_list */
	for (i = 0; i < CFG_CLIENT_COUNT; i++) {
		hif_sdio_init_probed_list(i);
		hif_sdio_init_clt_list(i);
	}

	/* 4 <2> register to mmc driver */
	ret = sdio_register_driver(&mtk_sdio_client_drv);
	if (ret != 0)
		HIF_SDIO_INFO_FUNC("sdio_register_driver() fail, ret=%d\n", ret);

#if !(DELETE_HIF_SDIO_CHRDEV)
	/* 4 <3> create thread for query chip id and device node for launcher to access */
	if (0 == hifsdiod_start())
		hif_sdio_create_dev_node();
#endif
	hif_sdio_deep_sleep_info_init();
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}

/*!
 * \brief hif_sdio init function
 *
 * detailed descriptions
 *
 * \retval
 */
static VOID hif_sdio_exit(VOID)
{
	HIF_SDIO_INFO_FUNC("start!\n");

#if !(DELETE_HIF_SDIO_CHRDEV)
	hif_sdio_remove_dev_node();
	hifsdiod_stop();
#endif

	/* 4 <0> if client driver is not removed yet, we shall NOT be called... */

	/* 4 <1> check reference count */
	if (gRefCount != 0)
		HIF_SDIO_WARN_FUNC("gRefCount=%d !!!\n", gRefCount);
	/* 4 <2> check if there is any hif_sdio-registered clients. There should be */
	/* 4 no registered client... */

	/* 4 <3> Reregister with mmc driver. Our remove handler hif_sdio_remove() */
	/* 4 will be called later by mmc_core. Clean up driver resources there. */
	sdio_unregister_driver(&mtk_sdio_client_drv);
	atomic_set(&hif_sdio_irq_enable_flag, 0);
	HIF_SDIO_DBG_FUNC("end!\n");
}				/* end of exitWlan() */

/*!
 * \brief stp on by wmt (probe client driver).
 *
 *
 * \param none.
 *
 * \retval 0:success, -11:not probed, -12:already on, -13:not registered, other errors.
 */
static _osal_inline_ INT32 hif_sdio_stp_on(VOID)
{
#if 0
	MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO *clt_probe_worker_info = 0;
#endif
	INT32 clt_index = -1;
	INT32 probe_index = -1;
	INT32 ret = -1;
	INT32 ret2 = -1;
	struct sdio_func *func;
	UINT32 chip_id = 0;
	UINT16 func_num = 0;

	const MTK_WCN_HIF_SDIO_FUNCINFO *func_info = NULL;

	HIF_SDIO_INFO_FUNC("start!\n");

	/* 4 <1> If stp client drv has not been probed, return error code */
	/* MT6620 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020B, 1);
	if (probe_index >= 0)
		goto stp_on_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020C, 1);
	if (probe_index >= 0)
		goto stp_on_exist;

	/* MT6628 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6628, 2);
	if (probe_index >= 0)
		goto stp_on_exist;
	/* MT6630 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6630, 2);
	if (probe_index >= 0) {
		chip_id = 0x6630;
		func_num = 2;
		goto stp_on_exist;
	}
	/* MT6632 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6632, 2);
	if (probe_index >= 0) {
		chip_id = 0x6632;
		func_num = 2;
		goto stp_on_exist;
	}
	/* MT6619 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6619, 1);
	if (probe_index >= 0)
		goto stp_on_exist;

	/* MT6618 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018B, 1);
	if (probe_index >= 0)
		goto stp_on_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018C, 1);
	if (probe_index >= 0)
		goto stp_on_exist;
	else {
		/* 4 <2> If stp client drv has not been probed, return error code */
		/* client func has not been probed */
		HIF_SDIO_INFO_FUNC("no supported func probed\n");
		return HIF_SDIO_ERR_NOT_PROBED;
	}

stp_on_exist:
	/* 4 <3> If stp client drv has been on by wmt, return error code */
	if (MTK_WCN_BOOL_FALSE != g_hif_sdio_probed_func_list[probe_index].on_by_wmt) {
		HIF_SDIO_INFO_FUNC("already on...\n");
		return HIF_SDIO_ERR_ALRDY_ON;
	}
	g_hif_sdio_probed_func_list[probe_index].on_by_wmt = MTK_WCN_BOOL_TRUE;

	clt_index = g_hif_sdio_probed_func_list[probe_index].clt_idx;
	if (clt_index >= 0) {	/* the function has been registered */
		g_hif_sdio_probed_func_list[probe_index].sdio_irq_enabled = MTK_WCN_BOOL_FALSE;
		/* 4 <4> claim irq for this function */
		func = g_hif_sdio_probed_func_list[probe_index].func;
		if (unlikely(!(func) || !(func->card) || !(func->card->host)
			     || mmc_card_removed(func->card))) {
			HIF_SDIO_ERR_FUNC("sdio host is missing\n");
			return HIF_SDIO_ERR_NOT_PROBED;
		}
		sdio_claim_host(func);
		ret = sdio_claim_irq(func, hif_sdio_irq);
		mtk_wcn_hif_sdio_irq_flag_set(1);
		sdio_release_host(func);
		if (ret) {
			HIF_SDIO_WARN_FUNC("sdio_claim_irq() for stp fail(%d)\n", ret);
			return ret;
		}
		HIF_SDIO_INFO_FUNC("sdio_claim_irq() for stp ok\n");

		/* 4 <5> If this struct sdio_func *func is supported by any driver in */
		/* 4 g_hif_sdio_clt_drv_list, schedule another task to call client's .hif_clt_probe() */
		/* TODO: [FixMe][George] WHY probe worker is removed??? */
#if 1
		/* Call client's .hif_clt_probe() */
		ret = hif_sdio_clt_probe_func(&g_hif_sdio_clt_drv_list[clt_index], probe_index);
		if (ret) {
			HIF_SDIO_WARN_FUNC("clt_probe_func() for stp fail(%d) release irq\n", ret);
			sdio_claim_host(func);
			mtk_wcn_hif_sdio_irq_flag_set(0);
			ret2 = sdio_release_irq(func);
			sdio_release_host(func);
			if (ret2)
				HIF_SDIO_WARN_FUNC("sdio_release_irq() for stp fail(%d)\n", ret2);

			g_hif_sdio_probed_func_list[probe_index].on_by_wmt = MTK_WCN_BOOL_FALSE;
			return ret;
		}
		g_hif_sdio_probed_func_list[probe_index].sdio_irq_enabled = MTK_WCN_BOOL_TRUE;

		/*set deep sleep information to global data struct */
		func_info = g_hif_sdio_clt_drv_list[clt_index].func_info;
		hif_sdio_deep_sleep_info_set_act(chip_id, func_num,
						  CLTCTX(func_info->card_id, func_info->func_num,
							 func_info->blk_sz, probe_index), 1);


		HIF_SDIO_INFO_FUNC("ok!\n");

		return 0;
#else
		/* use worker thread to perform the client's .hif_clt_probe() */
		clt_probe_worker_info = vmalloc(sizeof(MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO));
		INIT_WORK(&clt_probe_worker_info->probe_work, hif_sdio_clt_probe_worker);
		clt_probe_worker_info->registinfo_p = &g_hif_sdio_clt_drv_list[clt_index];
		clt_probe_worker_info->probe_idx = probe_index;
		schedule_work(&clt_probe_worker_info->probe_work);
#endif
	} else {
		/* TODO: [FixMe][George] check if clt_index is cleared in client's unregister function */
		HIF_SDIO_WARN_FUNC("probed but not registered yet (%d)\n", ret);
		return HIF_SDIO_ERR_CLT_NOT_REG;
	}
}

/*!
 * \brief stp off by wmt (remove client driver).
 *
 *
 * \param none.
 *
 * \retval 0:success, -11:not probed, -12:already off, -13:not registered, other errors.
 */
static _osal_inline_ INT32 hif_sdio_stp_off(VOID)
{
	INT32 clt_index = -1;
	INT32 probe_index = -1;
	INT32 ret = -1;
	INT32 ret2 = -1;
	struct sdio_func *func;
	UINT32 chip_id = 0;
	UINT16 func_num = 0;
	const MTK_WCN_HIF_SDIO_FUNCINFO *func_info = NULL;

	HIF_SDIO_INFO_FUNC("start!\n");

	/* 4 <1> If stp client drv has not been probed, return error code */
	/* MT6620 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020B, 1);
	if (probe_index >= 0)
		goto stp_off_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020C, 1);
	if (probe_index >= 0)
		goto stp_off_exist;

	/* MT6628 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6628, 2);
	if (probe_index >= 0)
		goto stp_off_exist;
	/* MT6630 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6630, 2);
	if (probe_index >= 0) {
		chip_id = 0x6630;
		func_num = 2;
		goto stp_off_exist;
	}
	/* MT6632 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6632, 2);
	if (probe_index >= 0) {
		chip_id = 0x6632;
		func_num = 2;
		goto stp_off_exist;
	}

	/* MT6619 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6619, 1);
	if (probe_index >= 0)
		goto stp_off_exist;

	/* MT6618 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018B, 1);
	if (probe_index >= 0)
		goto stp_off_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018C, 1);
	if (probe_index >= 0)
		goto stp_off_exist;
	else {
		/* 4 <2> If stp client drv has not been probed, return error code */
		/* client func has not been probed */
		return HIF_SDIO_ERR_NOT_PROBED;
	}

stp_off_exist:
	/* 4 <3> If stp client drv has been off by wmt, return error code */
	if (MTK_WCN_BOOL_FALSE == g_hif_sdio_probed_func_list[probe_index].on_by_wmt) {
		HIF_SDIO_WARN_FUNC("already off...\n");
		return HIF_SDIO_ERR_ALRDY_OFF;
	}
	g_hif_sdio_probed_func_list[probe_index].on_by_wmt = MTK_WCN_BOOL_FALSE;

#if 0				/* TODO: [FixMe][George] moved below as done in stp_on. */
	/* 4 <4> release irq for this function */
	func = g_hif_sdio_probed_func_list[probe_index].func;
	sdio_claim_host(func);
	ret = sdio_release_irq(func);
	sdio_release_host(func);
	if (ret)
		pr_warn(DRV_NAME "sdio_release_irq for stp fail(%d)\n", ret);
	else
		pr_warn(DRV_NAME "sdio_release_irq for stp ok\n");
#endif
	clt_index = g_hif_sdio_probed_func_list[probe_index].clt_idx;
	if (clt_index >= 0) {	/* the function has been registered */
		func = g_hif_sdio_probed_func_list[probe_index].func;

		if (unlikely(!(func) || !(func->card) || !(func->card->host)
			     || mmc_card_removed(func->card))) {
			HIF_SDIO_ERR_FUNC("sdio host is missing\n");
			return HIF_SDIO_ERR_ALRDY_OFF;
		}
		/* 4 <4> release irq for this function */
		sdio_claim_host(func);
		mtk_wcn_hif_sdio_irq_flag_set(0);
		ret2 = sdio_release_irq(func);
		sdio_release_host(func);

		if (ret2)
			HIF_SDIO_WARN_FUNC("sdio_release_irq() for stp fail(%d)\n", ret2);
		else
			HIF_SDIO_INFO_FUNC("sdio_release_irq() for stp ok\n");

		/* 4 <5> Callback to client driver's remove() func */
		ret =
		    g_hif_sdio_clt_drv_list[clt_index].
		    sdio_cltinfo->hif_clt_remove(CLTCTX
						 (func->device, func->num, func->cur_blksize,
						  probe_index));
		if (ret)
			HIF_SDIO_WARN_FUNC("clt_remove for stp fail(%d)\n", ret);
		else
			HIF_SDIO_INFO_FUNC("ok!\n");

		/*set deep sleep information to global data struct */
		func_info = g_hif_sdio_clt_drv_list[clt_index].func_info;
		hif_sdio_deep_sleep_info_set_act(chip_id, func_num,
						  CLTCTX(func_info->card_id, func_info->func_num,
							 func_info->blk_sz, probe_index), 0);
		return ret + ret2;
	}
	/* TODO: [FixMe][George] check if clt_index is cleared in client's unregister function */
	HIF_SDIO_WARN_FUNC("probed but not registered yet (%d)\n", ret);
	return HIF_SDIO_ERR_CLT_NOT_REG;
}

/*!
 * \brief wifi on by wmt (probe client driver).
 *
 *
 * \param none.
 *
 * \retval 0:success, -11:not probed, -12:already on, -13:not registered, other errors.
 */
static _osal_inline_ INT32 hif_sdio_wifi_on(VOID)
{
#if 0
	MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO *clt_probe_worker_info = 0;
#endif
	INT32 clt_index = -1;
	INT32 probe_index = -1;
	INT32 ret = 0;
	INT32 ret2 = 0;
	INT32 sdio_autok_flag = 0;
	struct sdio_func *func;
	UINT32 chip_id = 0;
	UINT16 func_num = 0;
	const MTK_WCN_HIF_SDIO_FUNCINFO *func_info = NULL;

	HIF_SDIO_INFO_FUNC("start!\n");

	/* 4 <1> If wifi client drv has not been probed, return error code */
	/* MT6620 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020A, 1);
	if (probe_index >= 0)
		goto wifi_on_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020C, 2);
	if (probe_index >= 0)
		goto wifi_on_exist;
	/* MT6628 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6628, 1);
	if (probe_index == 0)
		goto wifi_on_exist;
	/* MT6630 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6630, 1);
	 if (probe_index >= 0) {
		sdio_autok_flag = 1;
		chip_id = 0x6630;
		func_num = 1;
		goto wifi_on_exist;
	}
	/* MT6632 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6602, 1);
	 if (probe_index >= 0) {
		sdio_autok_flag = 1;
		chip_id = 0x6632;
		func_num = 1;
		goto wifi_on_exist;
	}

	/* MT6618 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018A, 1);
	if (probe_index == 0)
		goto wifi_on_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018C, 2);
	if (probe_index >= 0)
		goto wifi_on_exist;
	else {
		/* 4 <2> If wifi client drv has not been probed, return error code */
		/* client func has not been probed */
		return HIF_SDIO_ERR_NOT_PROBED;
	}

wifi_on_exist:
	/* 4 <3> If wifi client drv has been on by wmt, return error code */
	if (g_hif_sdio_probed_func_list[probe_index].on_by_wmt) {
		HIF_SDIO_INFO_FUNC("probe_index (%d), already on...\n", probe_index);
		return HIF_SDIO_ERR_ALRDY_ON;
	}
	clt_index = g_hif_sdio_probed_func_list[probe_index].clt_idx;
	if (clt_index >= 0) {	/* the function has been registered */
		if (sdio_autok_flag)
			hif_sdio_do_autok(g_hif_sdio_probed_func_list[probe_index].func);
		else
			HIF_SDIO_INFO_FUNC("sdio_autok_flag is not set\n", ret);
		g_hif_sdio_probed_func_list[probe_index].sdio_irq_enabled = MTK_WCN_BOOL_FALSE;
		/* 4 <4> claim irq for this function */
		func = g_hif_sdio_probed_func_list[probe_index].func;
		if (unlikely(!(func) || !(func->card) || !(func->card->host)
			     || mmc_card_removed(func->card))) {
			HIF_SDIO_ERR_FUNC("sdio host is missing\n");
			return HIF_SDIO_ERR_NOT_PROBED;
		}
		sdio_claim_host(func);
		ret = sdio_claim_irq(func, hif_sdio_irq);
		mtk_wcn_hif_sdio_irq_flag_set(1);
		sdio_release_host(func);
		if (ret) {
			HIF_SDIO_WARN_FUNC("sdio_claim_irq() for wifi fail(%d)\n", ret);
			return ret;
		}
		HIF_SDIO_INFO_FUNC("sdio_claim_irq() for wifi ok\n");

		/* 4 <5> If this struct sdio_func *func is supported by any driver in */
		/* 4 g_hif_sdio_clt_drv_list, schedule another task to call client's .hif_clt_probe() */
		/* TODO: [FixMe][George] WHY probe worker is removed??? */
#if 1
		/*set deep sleep information to global data struct */
		func_info = g_hif_sdio_clt_drv_list[clt_index].func_info;
		hif_sdio_deep_sleep_info_set_act(chip_id, func_num,
						  CLTCTX(func_info->card_id, func_info->func_num,
							 func_info->blk_sz, probe_index), 1);

		/* Call client's .hif_clt_probe() */
		ret = hif_sdio_clt_probe_func(&g_hif_sdio_clt_drv_list[clt_index], probe_index);
		if (ret) {
			HIF_SDIO_WARN_FUNC("clt_probe_func() for wifi fail(%d) release irq\n", ret);
			sdio_claim_host(func);
			mtk_wcn_hif_sdio_irq_flag_set(0);
			ret2 = sdio_release_irq(func);
			sdio_release_host(func);
			if (ret2)
				HIF_SDIO_WARN_FUNC("sdio_release_irq() for wifi fail(%d)\n", ret2);

			hif_sdio_deep_sleep_info_set_act(chip_id, func_num,
							  CLTCTX(func_info->card_id,
								 func_info->func_num,
								 func_info->blk_sz, probe_index),
							  0);
			g_hif_sdio_probed_func_list[probe_index].on_by_wmt = MTK_WCN_BOOL_FALSE;
			return ret;
		}
		g_hif_sdio_probed_func_list[probe_index].on_by_wmt = MTK_WCN_BOOL_TRUE;

		HIF_SDIO_INFO_FUNC("ok!\n");
		return 0;
#else
		/* use worker thread to perform the client's .hif_clt_probe() */
		clt_probe_worker_info = vmalloc(sizeof(MTK_WCN_HIF_SDIO_CLT_PROBE_WORKERINFO));
		INIT_WORK(&clt_probe_worker_info->probe_work, hif_sdio_clt_probe_worker);
		clt_probe_worker_info->registinfo_p = &g_hif_sdio_clt_drv_list[clt_index];
		clt_probe_worker_info->probe_idx = probe_index;
		schedule_work(&clt_probe_worker_info->probe_work);
#endif
	} else {
		/* TODO: [FixMe][George] check if clt_index is cleared in client's unregister function */
		HIF_SDIO_WARN_FUNC("probed but not registered yet (%d)\n", ret);
		return HIF_SDIO_ERR_CLT_NOT_REG;
	}
}

/*!
 * \brief wifi off by wmt (remove client driver).
 *
 *
 * \param none.
 *
 * \retval 0:success, -11:not probed, -12:already off, -13:not registered, other errors.
 */
static _osal_inline_ INT32 hif_sdio_wifi_off(VOID)
{
	INT32 clt_index = -1;
	INT32 probe_index = -1;
	INT32 ret = -1;
	INT32 ret2 = -1;
	struct sdio_func *func;
	UINT32 chip_id = 0;
	UINT16 func_num = 0;
	const MTK_WCN_HIF_SDIO_FUNCINFO *func_info = NULL;

	HIF_SDIO_INFO_FUNC("start!\n");

	/* 4 <1> If wifi client drv has not been probed, return error code */
	/* MT6620 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020A, 1);
	if (probe_index >= 0)
		goto wifi_off_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x020C, 2);
	if (probe_index >= 0)
		goto wifi_off_exist;

	/* MT6628 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6628, 1);
	if (probe_index >= 0)
		goto wifi_off_exist;
	/* MT6630 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6630, 1);
	if (probe_index >= 0) {
		chip_id = 0x6630;
		func_num = 1;
		goto wifi_off_exist;
	}
	/* MT6632 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x6602, 1);
	if (probe_index >= 0) {
		chip_id = 0x6632;
		func_num = 1;
		goto wifi_off_exist;
	}

	/* MT6618 */
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018A, 1);
	if (probe_index >= 0)
		goto wifi_off_exist;
	probe_index = hif_sdio_find_probed_list_index_by_id_func(0x037A, 0x018C, 2);
	if (probe_index >= 0)
		goto wifi_off_exist;
	else {
		/* 4 <2> If wifi client drv has not been probed, return error code */
		/* client func has not been probed */
		return HIF_SDIO_ERR_NOT_PROBED;
	}

wifi_off_exist:
	/* 4 <3> If wifi client drv has been off by wmt, return error code */
	if (MTK_WCN_BOOL_FALSE == g_hif_sdio_probed_func_list[probe_index].on_by_wmt) {
		HIF_SDIO_WARN_FUNC("already off...\n");
		return HIF_SDIO_ERR_ALRDY_OFF;
	}
	g_hif_sdio_probed_func_list[probe_index].on_by_wmt = MTK_WCN_BOOL_FALSE;


#if 0				/* TODO: [FixMe][George] moved below as done in wifi_on. */
	/* 4 <4> release irq for this function */
	func = g_hif_sdio_probed_func_list[probe_index].func;
	sdio_claim_host(func);
	ret = sdio_release_irq(func);
	sdio_release_host(func);
	if (ret)
		pr_warn(DRV_NAME "sdio_release_irq for wifi fail(%d)\n", ret);
	else
		pr_warn(DRV_NAME "sdio_release_irq for wifi ok\n");

#endif
	clt_index = g_hif_sdio_probed_func_list[probe_index].clt_idx;
	if (clt_index >= 0) {	/* the function has been registered */
		func = g_hif_sdio_probed_func_list[probe_index].func;

		/* 4 <4> Callback to client driver's remove() func */
		ret =
		    g_hif_sdio_clt_drv_list[clt_index].
		    sdio_cltinfo->hif_clt_remove(CLTCTX
						 (func->device, func->num, func->cur_blksize,
						  probe_index));
		if (ret)
			HIF_SDIO_WARN_FUNC("clt_remove for wifi fail(%d)\n", ret);
		else
			HIF_SDIO_INFO_FUNC("ok!\n");

		if (unlikely(!(func) || !(func->card) || !(func->card->host)
			     || mmc_card_removed(func->card))) {
			HIF_SDIO_ERR_FUNC("sdio host is missing\n");
			return HIF_SDIO_ERR_ALRDY_OFF;
		}
		/* 4 <5> release irq for this function */
		sdio_claim_host(func);
		mtk_wcn_hif_sdio_irq_flag_set(0);
		ret2 = sdio_release_irq(func);
		sdio_release_host(func);
		g_hif_sdio_probed_func_list[probe_index].sdio_irq_enabled = MTK_WCN_BOOL_FALSE;
		if (ret2)
			HIF_SDIO_WARN_FUNC("sdio_release_irq() for wifi fail(%d)\n", ret2);
		else
			HIF_SDIO_INFO_FUNC("sdio_release_irq() for wifi ok\n");

		/*set deep sleep information to global data struct */
		func_info = g_hif_sdio_clt_drv_list[clt_index].func_info;
		hif_sdio_deep_sleep_info_set_act(chip_id, func_num,
						  CLTCTX(func_info->card_id, func_info->func_num,
							 func_info->blk_sz, probe_index), 0);

		return ret + ret2;
	}
	/* TODO: [FixMe][George] check if clt_index is cleared in client's unregister function */
	HIF_SDIO_WARN_FUNC("probed but not registered yet (%d)\n", ret);
	return HIF_SDIO_ERR_CLT_NOT_REG;
}

/*!
 * \brief set mmc power up/off
 *
 * detailed descriptions
 *
 * \param: 1. ctx client's context variable, 2.power state: 1:power up, other:power off
 *
 * \retval 0:success, -1:fail
 */
INT32 mtk_wcn_hif_sdio_bus_set_power(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 pwrState)
{
	INT32 probe_index = -1;
	struct sdio_func *func = 0;

	HIF_SDIO_INFO_FUNC("turn Bus Power to: %d\n", pwrState);

	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		return -1;
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;

	if (!func) {
		HIF_SDIO_WARN_FUNC("Cannot find sdio_func !!!\n");
		return -1;
	}

	if (1 == pwrState) {
		sdio_claim_host(func);
		mmc_power_up_ext(func->card->host);
		sdio_release_host(func);
		HIF_SDIO_WARN_FUNC("SDIO BUS Power UP\n");
	} else {
		sdio_claim_host(func);
		mmc_power_off_ext(func->card->host);
		sdio_release_host(func);
		HIF_SDIO_WARN_FUNC("SDIO BUS Power OFF\n");
	}

	return 0;
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_bus_set_power);

VOID mtk_wcn_hif_sdio_enable_irq(MTK_WCN_HIF_SDIO_CLTCTX ctx, MTK_WCN_BOOL enable)
{
	UINT8 probed_idx = CLTCTX_IDX(ctx);

	if (unlikely(!CLTCTX_IDX_VALID(probed_idx))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid idx in ctx(0x%x), sdio_irq no change\n", ctx);
		return;
	}
	if (unlikely(!CLTCTX_IDX_VALID(probed_idx))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		return;
	}
	/* store client driver's private data to dev driver */
	g_hif_sdio_probed_func_list[probed_idx].sdio_irq_enabled = enable;
	smp_wmb();
	HIF_SDIO_INFO_FUNC("ctx(0x%x) sdio irq enable(%d)\n",
			   ctx, (MTK_WCN_BOOL_FALSE == enable) ? 0 : 1);


}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_enable_irq);

#if 0
static _osal_inline_ INT32 hif_sdio_is_autok_support(struct sdio_func *func)
{
	INT32 iRet = -1;

	if ((0x037A == func->vendor) && (0x6630 == func->device) && (1 == func->num))
		iRet = 0;

	return iRet;
}
#endif


static _osal_inline_ INT32 hif_sdio_do_autok(struct sdio_func *func)
{
	INT32 i_ret = 0;

#if MTK_HIF_SDIO_AUTOK_ENABLED
#if 0
	BOOTMODE boot_mode;

	boot_mode = get_boot_mode();
	if (boot_mode == META_BOOT) {
		HIF_SDIO_INFO_FUNC("omit autok in meta mode\n");
		i_ret = 0;
		return i_ret;
	}
#endif
	HIF_SDIO_INFO_FUNC("wait_sdio_autok_ready++\n");
	wait_sdio_autok_ready(func->card->host);
	HIF_SDIO_INFO_FUNC("wait_sdio_autok_ready--\n");
	i_ret = 0;

#else
	HIF_SDIO_ERR_FUNC("autok feature is not enabled.\n");
#endif
	return i_ret;
}


INT32 mtk_wcn_hif_sdio_do_autok(MTK_WCN_HIF_SDIO_CLTCTX ctx)
{
	INT32 i_ret = 0;

	UINT8 probe_index = 0;
	struct sdio_func *func = NULL;

	probe_index = CLTCTX_IDX(ctx);
	if (CFG_CLIENT_COUNT > probe_index)
		func = g_hif_sdio_probed_func_list[probe_index].func;
	else {
		HIF_SDIO_WARN_FUNC("probe_index is %d, out of range!\n", probe_index);
		return -1;
	}

	i_ret = hif_sdio_do_autok(func);

	return i_ret;
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_do_autok);


/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0    register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_f0_readb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, PUINT8 pvb)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");
	HIF_SDIO_ASSERT(pvb);

/*4 <1> check if ctx is valid, registered, and probed */
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		return -1;
	}
	if (probe_index < 0 || probe_index >= CFG_CLIENT_COUNT) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif

/*4 <2>*/
	sdio_claim_host(func);
	*pvb = sdio_f0_readb(func, offset, &ret);
	sdio_release_host(func);

/*4 <3> check result code and return proper error code*/

out:
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_f0_readb() */


/*!
 * \brief
 *
 * detailed descriptions
 *
 * \param ctx client's context variable
 *
 * \retval 0register successfully
 * \retval < 0  list error code here
 */
INT32 mtk_wcn_hif_sdio_f0_writeb(MTK_WCN_HIF_SDIO_CLTCTX ctx, UINT32 offset, UINT8 vb)
{
#if HIF_SDIO_UPDATE
	INT32 ret;
	struct sdio_func *func;
#else
	INT32 ret = -HIF_SDIO_ERR_FAIL;
	INT32 probe_index = -1;
	struct sdio_func *func = 0;
#endif

	HIF_SDIO_DBG_FUNC("start!\n");

/*4 <1> check if ctx is valid, registered, and probed*/
#if HIF_SDIO_UPDATE
	ret = -HIF_SDIO_ERR_FAIL;
	func = hif_sdio_ctx_to_func(ctx);
	if (!func) {
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	}
#else
	probe_index = CLTCTX_IDX(ctx);
	if (unlikely(!CLTCTX_IDX_VALID(probe_index))) {	/* invalid index in CLTCTX */
		HIF_SDIO_WARN_FUNC("invalid ctx(0x%x)\n", ctx);
		goto out;
	}
	if (probe_index < 0) {	/* the function has not been probed */
		HIF_SDIO_WARN_FUNC("can't find client in probed list!\n");
		ret = -HIF_SDIO_ERR_FAIL;
		goto out;
	} else {
		if (g_hif_sdio_probed_func_list[probe_index].clt_idx < 0) {	/* the client has not been registered */
			HIF_SDIO_WARN_FUNC("can't find client in registered list!\n");
			ret = -HIF_SDIO_ERR_FAIL;
			goto out;
		}
	}
	func = g_hif_sdio_probed_func_list[probe_index].func;
#endif

/*4 <1.1> check if input parameters are valid*/

/*4 <2>*/
	wmt_tra_sdio_update();
	sdio_claim_host(func);
	sdio_f0_writeb(func, vb, offset, &ret);
	sdio_release_host(func);

/*4 <3> check result code and return proper error code*/

out:
	HIF_SDIO_DBG_FUNC("end!\n");
	return ret;
}				/* end of mtk_wcn_hif_sdio_f0_writeb() */


INT32 mtk_wcn_hif_sdio_en_deep_sleep(MTK_WCN_HIF_SDIO_CLTCTX ctx)
{
	return hif_sdio_deep_sleep_ctrl(ctx, 1);
}				/* end of mtk_wcn_hif_sdio_deep_sleep() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_en_deep_sleep);


INT32 mtk_wcn_hif_sdio_dis_deep_sleep(MTK_WCN_HIF_SDIO_CLTCTX ctx)
{
	return hif_sdio_deep_sleep_ctrl(ctx, 0);
}				/* end of mtk_wcn_hif_sdio_wake_up() */
EXPORT_SYMBOL(mtk_wcn_hif_sdio_dis_deep_sleep);



#ifdef MTK_WCN_REMOVE_KERNEL_MODULE
INT32 mtk_wcn_hif_sdio_drv_init(VOID)
{
	return hif_sdio_init();

}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_drv_init);

VOID mtk_wcn_hif_sdio_driver_exit(VOID)
{
	return hif_sdio_exit();
}
EXPORT_SYMBOL(mtk_wcn_hif_sdio_driver_exit);

#else
module_init(hif_sdio_init);
module_exit(hif_sdio_exit);
#endif


/*! \file
    \brief brief description

    Detailed descriptions here.

*/



/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/



/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/device.h>
#include "wmt_dev.h"
#include "wmt_core.h"
#include "wmt_exp.h"
#include "wmt_lib.h"
#include "wmt_conf.h"
#include "psm_core.h"
#include "stp_core.h"
#include "stp_exp.h"
#include "hif_sdio.h"
#include "wmt_dbg.h"
#include "wmt_idc.h"
#include "osal.h"
#include <linux/suspend.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#if CONSYS_EARLYSUSPEND_ENABLE
#include <linux/earlysuspend.h>
#endif


#define MTK_WMT_VERSION  "Combo WMT Driver - v1.0"
#define MTK_WMT_DATE     "2011/10/04"
#define MTK_COMBO_DRIVER_VERSION "APEX.WCN.MT6620.JB2.MP.V1.0"
#define WMT_DEV_MAJOR 190	/* never used number */
#define WMT_DEV_NUM 1
#define WMT_DEV_INIT_TO_MS (2 * 1000)

#if CFG_WMT_PROC_FOR_AEE
static struct proc_dir_entry *gWmtAeeEntry = NULL;
#define WMT_AEE_PROCNAME "driver/wmt_aee"
#define WMT_PROC_AEE_SIZE 3072
static UINT32 g_buf_len = 0;
static PUINT8 pBuf = NULL;



#if USE_NEW_PROC_FS_FLAG
ssize_t wmt_aee_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t wmt_aee_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static struct file_operations wmt_aee_fops = {
	.read = wmt_aee_read,
	.write = wmt_aee_write,
};
#endif


#endif


#define WMT_DRIVER_NAME "mtk_stp_wmt"


P_OSAL_EVENT gpRxEvent = NULL;

UINT32 u4RxFlag = 0x0;
static atomic_t gRxCount = ATOMIC_INIT(0);

/* Linux UCHAR device */
static INT32 gWmtMajor = WMT_DEV_MAJOR;
static struct cdev gWmtCdev;
static atomic_t gWmtRefCnt = ATOMIC_INIT(0);
/* WMT driver information */
static UINT8 gLpbkBuf[WMT_LPBK_BUF_LEN] = { 0 };	/* modify for support 1024 loopback */

static UINT32 gLpbkBufLog;	/* George LPBK debug */
static INT32 gWmtInitDone = 0;
static wait_queue_head_t gWmtInitWq;

P_WMT_PATCH_INFO pPatchInfo = NULL;
UINT32 pAtchNum = 0;

#if CONSYS_WMT_REG_SUSPEND_CB_ENABLE || CONSYS_EARLYSUSPEND_ENABLE
static int mtk_wmt_func_off_background(void);
static int mtk_wmt_func_on_background(void);
#endif

#if CONSYS_EARLYSUSPEND_ENABLE

UINT32 g_early_suspend_flag = 0;
OSAL_SLEEPABLE_LOCK g_es_lr_lock;
static void wmt_dev_early_suspend(struct early_suspend *h)
{
    osal_lock_sleepable_lock(&g_es_lr_lock);
    g_early_suspend_flag = 1;
    osal_unlock_sleepable_lock(&g_es_lr_lock);
    WMT_INFO_FUNC("@@@@@@@@@@wmt enter early suspend@@@@@@@@@@@@@@\n");
    if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_off(WMTDRV_TYPE_LPBK)) {
        WMT_WARN_FUNC("WMT turn off LPBK fail\n");
    } else {
		wmt_lib_notify_stp_sleep();
		WMT_INFO_FUNC("WMT turn off LPBK suceed");
    }
}

static void wmt_dev_late_resume(struct early_suspend *h)
{
    WMT_INFO_FUNC("@@@@@@@@@@wmt enter late resume@@@@@@@@@@@@@@\n");
    osal_lock_sleepable_lock(&g_es_lr_lock);
    g_early_suspend_flag = 0;
    osal_unlock_sleepable_lock(&g_es_lr_lock);
    mtk_wmt_func_on_background();
}

struct early_suspend wmt_early_suspend_handler = {
    .suspend = wmt_dev_early_suspend,
    .resume = wmt_dev_late_resume,
};

#else
UINT32 g_early_suspend_flag = 0;
#endif
/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/


#if CONSYS_WMT_REG_SUSPEND_CB_ENABLE || CONSYS_EARLYSUSPEND_ENABLE

static INT32 wmt_pwr_on_thread (void *pvData)
{
	INT32 retryCounter = 1;
	WMT_INFO_FUNC("wmt_pwr_on_thread start to run\n");
#if CONSYS_EARLYSUSPEND_ENABLE
	osal_lock_sleepable_lock(&g_es_lr_lock);
	if (1 == g_early_suspend_flag)
	{
		WMT_INFO_FUNC("wmt_pwr_on_thread exit, do nothing due to early_suspend flag set\n");
		osal_unlock_sleepable_lock(&g_es_lr_lock);
		return 0;
	}
    osal_unlock_sleepable_lock(&g_es_lr_lock);
#endif
	do {
		if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_LPBK)) {
			WMT_WARN_FUNC("WMT turn on LPBK fail, retrying, retryCounter left:%d!\n", retryCounter);
			retryCounter--;
			osal_sleep_ms(1000);
		}
		else
		{
			wmt_lib_notify_stp_sleep();
			WMT_INFO_FUNC("WMT turn on LPBK suceed");
			break;
		}
	} while (retryCounter > 0);
#if CONSYS_EARLYSUSPEND_ENABLE	
	osal_lock_sleepable_lock(&g_es_lr_lock);
	if (1 == g_early_suspend_flag)
	{
		WMT_INFO_FUNC("turn off lpbk due to early_suspend flag set\n");
		mtk_wcn_wmt_func_off(WMTDRV_TYPE_LPBK);
	}
	osal_unlock_sleepable_lock(&g_es_lr_lock);
#endif	
	WMT_INFO_FUNC("wmt_pwr_on_thread exits\n");
	return 0;
}

static INT32 mtk_wmt_func_on_background(void)
{
	INT32 iRet = 0;
	OSAL_THREAD bgFuncOnThread;
	P_OSAL_THREAD pThread = &bgFuncOnThread;
	/* Create background power on thread */
    osal_strncpy(pThread->threadName, ("mtk_wmtd_pwr_bg"), sizeof(pThread->threadName));
    pThread->pThreadData = NULL;
    pThread->pThreadFunc = (VOID *)wmt_pwr_on_thread;
    iRet = osal_thread_create(pThread);
    if (iRet) {
        WMT_ERR_FUNC("osal_thread_create(0x%p) fail(%d)\n", pThread, iRet);
        return -1;
    }
	/* 3. start: start running background power on thread*/
    iRet = osal_thread_run(pThread);
    if (iRet) {
        WMT_ERR_FUNC("osal_thread_run(0x%p) fail(%d)\n", pThread, iRet);
        return -2;
    }
	return 0;
}
static INT32 mtk_wmt_func_off_background(void)
{
	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_off(WMTDRV_TYPE_LPBK)) {
		WMT_WARN_FUNC("WMT turn off LPBK fail\n");
	}
	else
	{
		wmt_lib_notify_stp_sleep();
		WMT_INFO_FUNC("WMT turn off LPBK suceed");
	}
	return 0;
}

#endif

#if CFG_WMT_PROC_FOR_AEE

#if USE_NEW_PROC_FS_FLAG
ssize_t wmt_aee_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 retval = 0;
	UINT32 len = 0;
	WMT_INFO_FUNC("%s: count %d pos %lld\n", __func__, count, *f_pos);

	if (0 == *f_pos) {
		pBuf = wmt_lib_get_cpupcr_xml_format(&len);
		g_buf_len = len;
		WMT_INFO_FUNC("wmt_dev:wmt for aee buffer len(%d)\n", g_buf_len);
	}

	if (g_buf_len >= count) {

		retval = copy_to_user(buf, pBuf, count);
		if (retval) {
			WMT_ERR_FUNC("copy to aee buffer failed, ret:%d\n", retval);
			retval = -EFAULT;
			goto err_exit;
		}

		*f_pos += count;
		g_buf_len -= count;
		pBuf += count;
		WMT_INFO_FUNC("wmt_dev:after read,wmt for aee buffer len(%d)\n", g_buf_len);

		retval = count;
	} else if (0 != g_buf_len) {

		retval = copy_to_user(buf, pBuf, g_buf_len);
		if (retval) {
			WMT_ERR_FUNC("copy to aee buffer failed, ret:%d\n", retval);
			retval = -EFAULT;
			goto err_exit;
		}

		*f_pos += g_buf_len;
		len = g_buf_len;
		g_buf_len = 0;
		pBuf += len;
		retval = len;
		WMT_INFO_FUNC("wmt_dev:after read,wmt for aee buffer len(%d)\n", g_buf_len);
	} else {
		WMT_INFO_FUNC("wmt_dev: no data avaliable for aee\n");
		retval = 0;
	}
 err_exit:
	return retval;
}


ssize_t wmt_aee_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	WMT_TRC_FUNC();
	return 0;
}

#else
static UINT32 passCnt;
static int wmt_dev_proc_for_aee_read(char *page, char **start, off_t off, int count, int *eof,
				     void *data)
{
	UINT32 len = 0;

	WMT_INFO_FUNC("wmt-dev:wmt for aee page(%p)off(%d)count(%d)\n", page, off, count);

	if (off == 0) {
		pBuf = wmt_lib_get_cpupcr_xml_format(&len);
		g_buf_len = len;

		/*pass 3k buffer for each proc read */
		passCnt = g_buf_len / WMT_PROC_AEE_SIZE;
		passCnt = (g_buf_len % WMT_PROC_AEE_SIZE) ? (passCnt + 1) : passCnt;
		WMT_INFO_FUNC("wmt_dev:wmt for aee buffer len(%d)passCnt(%d)\n", g_buf_len,
			      passCnt);
	}

	if (passCnt) {
		if (g_buf_len > WMT_PROC_AEE_SIZE) {
			osal_memcpy(page, pBuf, WMT_PROC_AEE_SIZE);
			*start += WMT_PROC_AEE_SIZE;
			g_buf_len -= WMT_PROC_AEE_SIZE;
			pBuf += WMT_PROC_AEE_SIZE;
			WMT_INFO_FUNC("wmt_dev:after read,wmt for aee buffer len(%d)\n", g_buf_len);
			*eof = 1;
			passCnt--;
			return WMT_PROC_AEE_SIZE;
		} else {
			osal_memcpy(page, pBuf, g_buf_len);
			*start += g_buf_len;
			len = g_buf_len;
			g_buf_len = 0;
			*eof = 1;
			passCnt--;
			pBuf += len;
			return len;
		}
	}

	return len;
}


static int wmt_dev_proc_for_aee_write(struct file *file, const char *buffer, unsigned long count,
				      void *data)
{
	return 0;
}

#endif

INT32 wmt_dev_proc_for_aee_setup(VOID)
{
	INT32 i_ret = 0;
#if USE_NEW_PROC_FS_FLAG
	gWmtAeeEntry = proc_create(WMT_AEE_PROCNAME, 0664, NULL, &wmt_aee_fops);
	if (gWmtAeeEntry == NULL) {
		WMT_ERR_FUNC("Unable to create / wmt_aee proc entry\n\r");
		i_ret = -1;
	}
#else

	gWmtAeeEntry = create_proc_entry(WMT_AEE_PROCNAME, 0664, NULL);
	if (gWmtAeeEntry == NULL) {
		WMT_ERR_FUNC("Unable to create / wmt_aee proc entry\n\r");
		i_ret = -1;
	}
	gWmtAeeEntry->read_proc = wmt_dev_proc_for_aee_read;
	gWmtAeeEntry->write_proc = wmt_dev_proc_for_aee_write;
#endif
	return i_ret;
}


INT32 wmt_dev_proc_for_aee_remove(VOID)
{
#if USE_NEW_PROC_FS_FLAG
	if (NULL != gWmtAeeEntry) {
		proc_remove(gWmtAeeEntry);
	}
#else

	if (NULL != gWmtAeeEntry) {
		remove_proc_entry(WMT_AEE_PROCNAME, NULL);
	}
#endif
	return 0;
}
#endif				/* end of "CFG_WMT_PROC_FOR_AEE" */


VOID wmt_dev_rx_event_cb(VOID)
{
	if (NULL != gpRxEvent) {
		u4RxFlag = 1;
		atomic_inc(&gRxCount);
		wake_up_interruptible(&gpRxEvent->waitQueue);
	} else {
		WMT_ERR_FUNC("null gpRxEvent, flush rx!\n");
		wmt_lib_flush_rx();
	}
}


INT32 wmt_dev_rx_timeout(P_OSAL_EVENT pEvent)
{

	UINT32 ms = pEvent->timeoutValue;
	INT32 lRet = 0;
	gpRxEvent = pEvent;
	if (0 != ms) {
		lRet =
		    wait_event_interruptible_timeout(gpRxEvent->waitQueue, 0 != u4RxFlag,
						     msecs_to_jiffies(ms));
	} else {
		lRet = wait_event_interruptible(gpRxEvent->waitQueue, u4RxFlag != 0);
	}
	u4RxFlag = 0;
/* gpRxEvent = NULL; */
	if (atomic_dec_return(&gRxCount)) {
		WMT_ERR_FUNC("gRxCount != 0 (%d), reset it!\n", atomic_read(&gRxCount));
		atomic_set(&gRxCount, 0);
	}

	return lRet;
}

INT32 wmt_dev_read_file(PUINT8 pName, const PPUINT8 ppBufPtr, INT32 offset, INT32 padSzBuf)
{
	INT32 iRet = -1;
	struct file *fd;
	/* ssize_t iRet; */
	INT32 file_len;
	INT32 read_len;
	PVOID pBuf;

	/* struct cred *cred = get_task_cred(current); */
	const struct cred *cred = get_current_cred();

	if (!ppBufPtr) {
		WMT_ERR_FUNC("invalid ppBufptr!\n");
		return -1;
	}
	*ppBufPtr = NULL;

	fd = filp_open(pName, O_RDONLY, 0);
	if (!fd || IS_ERR(fd) || !fd->f_op || !fd->f_op->read) {
		WMT_ERR_FUNC("failed to open or read!(0x%p, %d, %d)\n", fd, cred->fsuid,
			     cred->fsgid);
		return -1;
	}

	file_len = fd->f_path.dentry->d_inode->i_size;
	pBuf = vmalloc((file_len + BCNT_PATCH_BUF_HEADROOM + 3) & ~0x3UL);
	if (!pBuf) {
		WMT_ERR_FUNC("failed to vmalloc(%d)\n", (INT32) ((file_len + 3) & ~0x3UL));
		goto read_file_done;
	}

	do {
		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					WMT_ERR_FUNC("failed to seek!!\n");
					goto read_file_done;
				}
			} else {
				fd->f_pos = offset;
			}
		}

		read_len = fd->f_op->read(fd, pBuf + padSzBuf, file_len, &fd->f_pos);
		if (read_len != file_len) {
			WMT_WARN_FUNC("read abnormal: read_len(%d), file_len(%d)\n", read_len,
				      file_len);
		}
	} while (false);

	iRet = 0;
	*ppBufPtr = pBuf;

 read_file_done:
	if (iRet) {
		if (pBuf) {
			vfree(pBuf);
		}
	}

	filp_close(fd, NULL);

	return (iRet) ? iRet : read_len;
}

/* TODO: [ChangeFeature][George] refine this function name for general filesystem read operation, not patch only. */
INT32 wmt_dev_patch_get(PUINT8 pPatchName, osal_firmware **ppPatch, INT32 padSzBuf)
{
	INT32 iRet = -1;
	osal_firmware *pfw;
	uid_t orig_uid;
	gid_t orig_gid;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
	/* struct cred *cred = get_task_cred(current); */
	struct cred *cred = (struct cred *)get_current_cred();
#endif

	mm_segment_t orig_fs = get_fs();

	if (*ppPatch) {
		WMT_WARN_FUNC("f/w patch already exists\n");
		if ((*ppPatch)->data) {
			vfree((*ppPatch)->data);
		}
		kfree(*ppPatch);
		*ppPatch = NULL;
	}

	if (!osal_strlen(pPatchName)) {
		WMT_ERR_FUNC("empty f/w name\n");
		osal_assert((osal_strlen(pPatchName) > 0));
		return -1;
	}

	pfw = kzalloc(sizeof(osal_firmware), /*GFP_KERNEL */ GFP_ATOMIC);
	if (!pfw) {
		WMT_ERR_FUNC("kzalloc(%d) fail\n", sizeof(osal_firmware));
		return -2;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
	orig_uid = cred->fsuid;
	orig_gid = cred->fsgid;
	cred->fsuid = cred->fsgid = 0;
#else
	orig_uid = current->fsuid;
	orig_gid = current->fsgid;
	current->fsuid = current->fsgid = 0;
#endif

	set_fs(get_ds());

	/* load patch file from fs */
	iRet = wmt_dev_read_file(pPatchName, (const PPUINT8)&pfw->data, 0, padSzBuf);
	set_fs(orig_fs);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
	cred->fsuid = orig_uid;
	cred->fsgid = orig_gid;
#else
	current->fsuid = orig_uid;
	current->fsgid = orig_gid;
#endif

	if (iRet > 0) {
		pfw->size = iRet;
		*ppPatch = pfw;
		WMT_DBG_FUNC("load (%s) to addr(0x%p) success\n", pPatchName, pfw->data);
		return 0;
	} else {
		kfree(pfw);
		*ppPatch = NULL;
		WMT_ERR_FUNC("load file (%s) fail, iRet(%d)\n", pPatchName, iRet);
		return -1;
	}
}


INT32 wmt_dev_patch_put(osal_firmware **ppPatch)
{
	if (NULL != *ppPatch) {
		if ((*ppPatch)->data) {
			vfree((*ppPatch)->data);
		}
		kfree(*ppPatch);
		*ppPatch = NULL;
	}
	return 0;
}

VOID wmt_dev_patch_info_free(VOID)
{
	if (pPatchInfo) {
		kfree(pPatchInfo);
		pPatchInfo = NULL;
	}
}

MTK_WCN_BOOL wmt_dev_is_file_exist(PUINT8 pFileName)
{
	struct file *fd = NULL;
	/* ssize_t iRet; */
	INT32 fileLen = -1;
	const struct cred *cred = get_current_cred();
	if (pFileName == NULL) {
		WMT_ERR_FUNC("invalid file name pointer(%p)\n", pFileName);
		return MTK_WCN_BOOL_FALSE;
	}
	if (osal_strlen(pFileName) < osal_strlen(defaultPatchName)) {
		WMT_ERR_FUNC("invalid file name(%s)\n", pFileName);
		return MTK_WCN_BOOL_FALSE;
	}

	/* struct cred *cred = get_task_cred(current); */

	fd = filp_open(pFileName, O_RDONLY, 0);
	if (!fd || IS_ERR(fd) || !fd->f_op || !fd->f_op->read) {
		WMT_ERR_FUNC("failed to open or read(%s)!(0x%p, %d, %d)\n", pFileName, fd,
			     cred->fsuid, cred->fsgid);
		return MTK_WCN_BOOL_FALSE;
	}
	fileLen = fd->f_path.dentry->d_inode->i_size;
	filp_close(fd, NULL);
	fd = NULL;
	if (fileLen <= 0) {
		WMT_ERR_FUNC("invalid file(%s), length(%d)\n", pFileName, fileLen);
		return MTK_WCN_BOOL_FALSE;
	}
	WMT_ERR_FUNC("valid file(%s), length(%d)\n", pFileName, fileLen);
	return true;

}

static unsigned long count_last_access_sdio = 0;    
static unsigned long count_last_access_uart = 0;
static unsigned long jiffies_last_poll = 0;

static INT32 wmt_dev_tra_sdio_update(VOID)
{
	count_last_access_sdio += 1;
	/* WMT_INFO_FUNC("jiffies_last_access_sdio: jiffies = %ul\n", jiffies); */

	return 0;
}

extern INT32 wmt_dev_tra_uart_update(VOID)
{
	count_last_access_uart += 1;
	/* WMT_INFO_FUNC("jiffies_last_access_uart: jiffies = %ul\n", jiffies); */

	return 0;
}

static UINT32 wmt_dev_tra_sdio_poll(VOID)
{
#define TIME_THRESHOLD_TO_TEMP_QUERY 3000
#define COUNT_THRESHOLD_TO_TEMP_QUERY 200

	unsigned long sdio_during_count = 0;
	unsigned long poll_during_time = 0;

	if (jiffies > jiffies_last_poll) {
		poll_during_time = jiffies - jiffies_last_poll;
	} else {
		poll_during_time = 0xffffffff;
	}

	WMT_DBG_FUNC("**jiffies_to_mesecs(0xffffffff) = %u\n", jiffies_to_msecs(0xffffffff));

	if (jiffies_to_msecs(poll_during_time) < TIME_THRESHOLD_TO_TEMP_QUERY) {
		WMT_DBG_FUNC("**poll_during_time = %u < %u, not to query\n",
			     jiffies_to_msecs(poll_during_time), TIME_THRESHOLD_TO_TEMP_QUERY);
		return -1;
	}

	sdio_during_count = count_last_access_sdio;

	if (sdio_during_count < COUNT_THRESHOLD_TO_TEMP_QUERY) {
		WMT_DBG_FUNC("**sdio_during_count = %lu < %u, not to query\n",
			     sdio_during_count, COUNT_THRESHOLD_TO_TEMP_QUERY);
		return -1;
	}

	count_last_access_sdio = 0;
	jiffies_last_poll = jiffies;

	WMT_INFO_FUNC("**poll_during_time = %u > %u, sdio_during_count = %u > %u, query\n",
		      jiffies_to_msecs(poll_during_time), TIME_THRESHOLD_TO_TEMP_QUERY,
		      jiffies_to_msecs(sdio_during_count), COUNT_THRESHOLD_TO_TEMP_QUERY);

	return 0;
}

#if 0
static UINT32 wmt_dev_tra_uart_poll(void)
{
	/* we not support the uart case. */
	return -1;
}
#endif

INT32 wmt_dev_tm_temp_query(VOID)
{
#define HISTORY_NUM       5
#define TEMP_THRESHOLD   65
#define REFRESH_TIME    300	/* sec */

	static INT32 temp_table[HISTORY_NUM] = { 99 };	/* not query yet. */
	static INT32 idx_temp_table;
	static struct timeval query_time, now_time;

	INT8 query_cond = 0;
	INT8 ctemp = 0;
	INT32 current_temp = 0;
	INT32 index = 0;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;

	/* Query condition 1: */
	/* If we have the high temperature records on the past, we continue to query/monitor */
	/* the real temperature until cooling */
	for (index = 0; index < HISTORY_NUM; index++) {
		if (temp_table[index] >= TEMP_THRESHOLD) {
			query_cond = 1;
			WMT_INFO_FUNC
			    ("high temperature (current temp = %d), we must keep querying temp temperature..\n",
			     temp_table[index]);
		}
	}

	do_gettimeofday(&now_time);
#if 1
	/* Query condition 2: */
	/* Moniter the hif_sdio activity to decide if we have the need to query temperature. */
	if (!query_cond) {
		if (wmt_dev_tra_sdio_poll() == 0) {
			query_cond = 1;
            WMT_DBG_FUNC("sdio traffic , we must query temperature..\n");
		} else {
			WMT_DBG_FUNC("sdio idle traffic ....\n");
		}

		/* only WIFI tx power might make temperature varies largely */
#if 0
		if (!query_cond) {
			last_access_time = wmt_dev_tra_uart_poll();
			if (jiffies_to_msecs(last_access_time) < TIME_THRESHOLD_TO_TEMP_QUERY) {
				query_cond = 1;
				WMT_DBG_FUNC("uart busy traffic , we must query temperature..\n");
			} else {
				WMT_DBG_FUNC
				    ("uart still idle traffic , we don't query temp temperature..\n");
			}
		}
#endif
	}
#endif
	/* Query condition 3: */
	/* If the query time exceeds the a certain of period, refresh temp table. */
	/*  */
	if (!query_cond) {
		if ((now_time.tv_sec < query_time.tv_sec) ||	/* time overflow, we refresh temp table again for simplicity! */
		    ((now_time.tv_sec > query_time.tv_sec) &&
		     (now_time.tv_sec - query_time.tv_sec) > REFRESH_TIME)) {
			query_cond = 1;

			WMT_INFO_FUNC
			    ("It is long time (> %d sec) not to query, we must query temp temperature..\n",
			     REFRESH_TIME);
			for (index = 0; index < HISTORY_NUM; index++) {
				temp_table[index] = 99;
			}
		}
	}

	if (query_cond) {
		/* update the temperature record */
	    bRet = mtk_wcn_wmt_therm_ctrl(WMTTHERM_ENABLE);
		if (bRet == MTK_WCN_BOOL_TRUE)
		{
		    ctemp = mtk_wcn_wmt_therm_ctrl(WMTTHERM_READ);
		    bRet = mtk_wcn_wmt_therm_ctrl(WMTTHERM_DISABLE);
			if(bRet == MTK_WCN_BOOL_TRUE)
		    	wmt_lib_notify_stp_sleep();
			if (0 != (ctemp & 0x80))
			{
				ctemp &= 0x7f;
				current_temp = ~((INT32)ctemp - 1);
			}
			else
				current_temp = ctemp;
		}
		else
		{
			current_temp = -1;
			if (MTK_WCN_BOOL_TRUE == wmt_lib_is_therm_ctrl_support())
				WMT_WARN_FUNC("thermal function enable command failed, set current_temp = 0x%x \n", current_temp);
		}
		idx_temp_table = (idx_temp_table + 1) % HISTORY_NUM;
		temp_table[idx_temp_table] = current_temp;
		do_gettimeofday(&query_time);

		WMT_DBG_FUNC("[Thermal] current_temp = 0x%x \n", current_temp);
	} else {
		current_temp = temp_table[idx_temp_table];
		idx_temp_table = (idx_temp_table + 1) % HISTORY_NUM;
		temp_table[idx_temp_table] = current_temp;
	}

	/*  */
	/* Dump information */
	/*  */
	WMT_DBG_FUNC("[Thermal] idx_temp_table = %d\n", idx_temp_table);
	WMT_DBG_FUNC("[Thermal] now.time = %ld, query.time = %ld, REFRESH_TIME = %d\n",
		     now_time.tv_sec, query_time.tv_sec, REFRESH_TIME);

	WMT_DBG_FUNC("[0] = %d, [1] = %d, [2] = %d, [3] = %d, [4] = %d\n----\n",
		     temp_table[0], temp_table[1], temp_table[2], temp_table[3], temp_table[4]);

	return current_temp;
}

ssize_t WMT_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 iRet = 0;
	UINT8 wrBuf[NAME_MAX + 1] = { 0 };
	INT32 copySize = (count < NAME_MAX) ? count : NAME_MAX;

	WMT_LOUD_FUNC("count:%d copySize:%d\n", count, copySize);

	if (copySize > 0) {
		if (copy_from_user(wrBuf, buf, copySize)) {
			iRet = -EFAULT;
			goto write_done;
		}
		iRet = copySize;
		wrBuf[NAME_MAX] = '\0';

		if (!strncasecmp(wrBuf, "ok", NAME_MAX)) {
			WMT_DBG_FUNC("resp str ok\n");
			/* pWmtDevCtx->cmd_result = 0; */
			wmt_lib_trigger_cmd_signal(0);
		} else {
			WMT_WARN_FUNC("warning resp str (%s)\n", wrBuf);
			/* pWmtDevCtx->cmd_result = -1; */
			wmt_lib_trigger_cmd_signal(-1);
		}
		/* complete(&pWmtDevCtx->cmd_comp); */

	}

 write_done:
	return iRet;
}

ssize_t WMT_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 iRet = 0;
	PUINT8 pCmd = NULL;
	UINT32 cmdLen = 0;
	pCmd = wmt_lib_get_cmd();

	if (pCmd != NULL) {
		cmdLen = osal_strlen(pCmd) < NAME_MAX ? osal_strlen(pCmd) : NAME_MAX;
		WMT_DBG_FUNC("cmd str(%s)\n", pCmd);
		if (copy_to_user(buf, pCmd, cmdLen)) {
			iRet = -EFAULT;
		} else {
			iRet = cmdLen;
		}
	}
#if 0
	if (test_and_clear_bit(WMT_STAT_CMD, &pWmtDevCtx->state)) {
		iRet = osal_strlen(localBuf) < NAME_MAX ? osal_strlen(localBuf) : NAME_MAX;
		/* we got something from STP driver */
		WMT_DBG_FUNC("copy cmd to user by read:%s\n", localBuf);
		if (copy_to_user(buf, localBuf, iRet)) {
			iRet = -EFAULT;
			goto read_done;
		}
	}
#endif
	return iRet;
}

unsigned int WMT_poll(struct file *filp, poll_table *wait)
{
	UINT32 mask = 0;
	P_OSAL_EVENT pEvent = wmt_lib_get_cmd_event();

	poll_wait(filp, &pEvent->waitQueue, wait);
	/* empty let select sleep */
	if (MTK_WCN_BOOL_TRUE == wmt_lib_get_cmd_status()) {
		mask |= POLLIN | POLLRDNORM;	/* readable */
	}
#if 0
	if (test_bit(WMT_STAT_CMD, &pWmtDevCtx->state)) {
		mask |= POLLIN | POLLRDNORM;	/* readable */
	}
#endif
	mask |= POLLOUT | POLLWRNORM;	/* writable */
	return mask;
}

/* INT32 WMT_ioctl(struct inode *inode, struct file *filp, UINT32 cmd, unsigned long arg) */
long WMT_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
#define WMT_IOC_MAGIC        0xa0
#define WMT_IOCTL_SET_PATCH_NAME		_IOW(WMT_IOC_MAGIC, 4, char*)
#define WMT_IOCTL_SET_STP_MODE			_IOW(WMT_IOC_MAGIC, 5, int)
#define WMT_IOCTL_FUNC_ONOFF_CTRL		_IOW(WMT_IOC_MAGIC, 6, int)
#define WMT_IOCTL_LPBK_POWER_CTRL		_IOW(WMT_IOC_MAGIC, 7, int)
#define WMT_IOCTL_LPBK_TEST				_IOWR(WMT_IOC_MAGIC, 8, char*)
#define WMT_IOCTL_GET_CHIP_INFO			_IOR(WMT_IOC_MAGIC, 12, int)
#define WMT_IOCTL_SET_LAUNCHER_KILL		_IOW(WMT_IOC_MAGIC, 13, int)
#define WMT_IOCTL_SET_PATCH_NUM			_IOW(WMT_IOC_MAGIC, 14, int)
#define WMT_IOCTL_SET_PATCH_INFO		_IOW(WMT_IOC_MAGIC, 15, char*)
#define WMT_IOCTL_PORT_NAME			_IOWR(WMT_IOC_MAGIC, 20, char*)
#define WMT_IOCTL_WMT_CFG_NAME			_IOWR(WMT_IOC_MAGIC, 21, char*)
#define WMT_IOCTL_WMT_QUERY_CHIPID	_IOR(WMT_IOC_MAGIC, 22, int)
#define WMT_IOCTL_WMT_TELL_CHIPID	_IOW(WMT_IOC_MAGIC, 23, int)
#define WMT_IOCTL_WMT_COREDUMP_CTRL     _IOW(WMT_IOC_MAGIC, 24, int)
#define WMT_IOCTL_WMT_STP_ASSERT_CTRL   _IOW(WMT_IOC_MAGIC, 27, int)



	INT32 iRet = 0;
	UINT8 pBuffer[NAME_MAX + 1];
	WMT_DBG_FUNC("cmd (%u), arg (0x%lx)\n", cmd, arg);
	switch (cmd) {
	case WMT_IOCTL_SET_PATCH_NAME:	/* patch location */
		{

			if (copy_from_user(pBuffer, (void *)arg, NAME_MAX)) {
				iRet = -EFAULT;
				break;
			}
			pBuffer[NAME_MAX] = '\0';
			wmt_lib_set_patch_name(pBuffer);
		}
		break;

	case WMT_IOCTL_SET_STP_MODE:	/* stp/hif/fm mode */

		/* set hif conf */
		do {
			P_OSAL_OP pOp;
			MTK_WCN_BOOL bRet;
			P_OSAL_SIGNAL pSignal = NULL;
			P_WMT_HIF_CONF pHif = NULL;

			iRet = wmt_lib_set_hif(arg);
			if (0 != iRet) {
				WMT_INFO_FUNC("wmt_lib_set_hif fail (%lu)\n", arg);
				break;
			}

			pOp = wmt_lib_get_free_op();
			if (!pOp) {
				WMT_INFO_FUNC("get_free_lxop fail\n");
				break;
			}
			pSignal = &pOp->signal;
			pOp->op.opId = WMT_OPID_HIF_CONF;

			pHif = wmt_lib_get_hif();

			osal_memcpy(&pOp->op.au4OpData[0], pHif, sizeof(WMT_HIF_CONF));
			pOp->op.u4InfoBit = WMT_OP_HIF_BIT;
			pSignal->timeoutValue = 0;

			bRet = wmt_lib_put_act_op(pOp);
			WMT_DBG_FUNC("WMT_OPID_HIF_CONF result(%d)\n", bRet);
			iRet = (MTK_WCN_BOOL_FALSE == bRet) ? -EFAULT : 0;
		} while (0);

		break;

	case WMT_IOCTL_FUNC_ONOFF_CTRL:	/* test turn on/off func */

		do {
			MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
			if (arg & 0x80000000) {
				bRet = mtk_wcn_wmt_func_on(arg & 0xF);
			} else {
				bRet = mtk_wcn_wmt_func_off(arg & 0xF);
			}
			iRet = (MTK_WCN_BOOL_FALSE == bRet) ? -EFAULT : 0;
		} while (0);

		break;

	case WMT_IOCTL_LPBK_POWER_CTRL:
		/*switch Loopback function on/off
		   arg:     bit0 = 1:turn loopback function on
		   bit0 = 0:turn loopback function off
		 */
		do {
			MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
			if (arg & 0x01) {
				bRet = mtk_wcn_wmt_func_on(WMTDRV_TYPE_LPBK);
			} else {
				bRet = mtk_wcn_wmt_func_off(WMTDRV_TYPE_LPBK);
			}
			iRet = (MTK_WCN_BOOL_FALSE == bRet) ? -EFAULT : 0;
		} while (0);


		break;


	case WMT_IOCTL_LPBK_TEST:
		do {
			P_OSAL_OP pOp;
			MTK_WCN_BOOL bRet;
			UINT32 u4Wait;
			/* UINT8 lpbk_buf[1024] = {0}; */
			UINT32 effectiveLen = 0;
			P_OSAL_SIGNAL pSignal = NULL;

			if (copy_from_user(&effectiveLen, (void *)arg, sizeof(effectiveLen))) {
				iRet = -EFAULT;
				WMT_ERR_FUNC("copy_from_user failed at %d\n", __LINE__);
				break;
			}
			if (effectiveLen > sizeof(gLpbkBuf)) {
				iRet = -EFAULT;
				WMT_ERR_FUNC("length is too long\n");
				break;
			}
			WMT_DBG_FUNC("len = %d\n", effectiveLen);

			pOp = wmt_lib_get_free_op();
			if (!pOp) {
				WMT_WARN_FUNC("get_free_lxop fail\n");
				iRet = -EFAULT;
				break;
			}
			u4Wait = 2000;
			if (copy_from_user
			    (&gLpbkBuf[0], (void *)arg + sizeof(unsigned long), effectiveLen)) {
				WMT_ERR_FUNC("copy_from_user failed at %d\n", __LINE__);
				iRet = -EFAULT;
				break;
			}
			pSignal = &pOp->signal;
			pOp->op.opId = WMT_OPID_LPBK;
			pOp->op.au4OpData[0] = effectiveLen;	/* packet length */
			pOp->op.au4OpData[1] = (size_t) &gLpbkBuf[0];
			memcpy(&gLpbkBufLog,
			       &gLpbkBuf[((effectiveLen >= 4) ? effectiveLen - 4 : 0)], 4);
			pSignal->timeoutValue = MAX_EACH_WMT_CMD;
			WMT_INFO_FUNC("OPID(%d) type(%d) start\n",
				      pOp->op.opId, pOp->op.au4OpData[0]);
			if (DISABLE_PSM_MONITOR()) {
				WMT_ERR_FUNC("wake up failed\n");
				wmt_lib_put_op_to_free_queue(pOp);
				return -1;
			}

			bRet = wmt_lib_put_act_op(pOp);
			ENABLE_PSM_MONITOR();
			if (MTK_WCN_BOOL_FALSE == bRet) {
				WMT_WARN_FUNC("OPID(%d) type(%d) buf tail(0x%08x) fail\n",
					      pOp->op.opId, pOp->op.au4OpData[0], gLpbkBufLog);
				iRet = -1;
				break;
			} else {
				WMT_INFO_FUNC("OPID(%d) length(%d) ok\n",
					      pOp->op.opId, pOp->op.au4OpData[0]);
				iRet = pOp->op.au4OpData[0];
				if (copy_to_user
				    ((void *)arg + sizeof(unsigned long) + sizeof(UINT8[2048]), gLpbkBuf,
				     iRet)) {
					iRet = -EFAULT;
					break;
				}
			}
		} while (0);

		break;
#if 0
	case 9:
		{
#define LOG_BUF_SZ 300
			UINT8 buf[LOG_BUF_SZ];
			INT32 len = 0;
			INT32 remaining = 0;

			remaining = mtk_wcn_stp_btm_get_dmp(buf, &len);

			if (remaining == 0) {
				WMT_DBG_FUNC("waiting dmp\n");
				wait_event_interruptible(dmp_wq, dmp_flag != 0);
				dmp_flag = 0;
				remaining = mtk_wcn_stp_btm_get_dmp(buf, &len);

				/* WMT_INFO_FUNC("len = %d ###%s#\n", len, buf); */
			} else {
				WMT_LOUD_FUNC("no waiting dmp\n");
			}

			if (unlikely((len + sizeof(INT32)) >= LOG_BUF_SZ)) {
				WMT_ERR_FUNC("len is larger buffer\n");
				iRet = -EFAULT;
				goto fail_exit;
			}

			buf[sizeof(INT32) + len] = '\0';

			if (copy_to_user((void *)arg, (PUINT8)&len, sizeof(INT32))) {
				iRet = -EFAULT;
				goto fail_exit;
			}

			if (copy_to_user((void *)arg + sizeof(INT32), buf, len)) {
				iRet = -EFAULT;
				goto fail_exit;
			}
		}
		break;

	case 10:
		{
			WMT_INFO_FUNC("Enable combo trace32 dump\n");
			wmt_cdev_t32dmp_enable();
			WMT_INFO_FUNC("Enable STP debugging mode\n");
			mtk_wcn_stp_dbg_enable();
		}
		break;

	case 11:
		{
			WMT_INFO_FUNC("Disable combo trace32 dump\n");
			wmt_cdev_t32dmp_disable();
			WMT_INFO_FUNC("Disable STP debugging mode\n");
			mtk_wcn_stp_dbg_disable();
		}
		break;
#endif

	case 10:
		{
			wmt_lib_host_awake_get();
			mtk_wcn_stp_coredump_start_ctrl(1);
			osal_strcpy(pBuffer, "MT662x f/w coredump start-");
			if (copy_from_user
			    (pBuffer + osal_strlen(pBuffer), (void *)arg,
			     NAME_MAX - osal_strlen(pBuffer))) {
				/* osal_strcpy(pBuffer, "MT662x f/w assert core dump start"); */
				WMT_ERR_FUNC("copy assert string failed\n");
			}
			pBuffer[NAME_MAX] = '\0';
			osal_dbg_assert_aee(pBuffer, pBuffer);
		}
		break;
	case 11:
		{
			osal_dbg_assert_aee("MT662x f/w coredump end",
					    "MT662x firmware coredump ends");
			wmt_lib_host_awake_put();
		}
		break;


	case WMT_IOCTL_GET_CHIP_INFO:
		{
			if (0 == arg) {
				return wmt_lib_get_icinfo(WMTCHIN_CHIPID);
			} else if (1 == arg) {
				return wmt_lib_get_icinfo(WMTCHIN_HWVER);
			} else if (2 == arg) {
				return wmt_lib_get_icinfo(WMTCHIN_FWVER);
			}
		}
		break;

	case WMT_IOCTL_SET_LAUNCHER_KILL:{
			if (1 == arg) {
				WMT_INFO_FUNC("launcher may be killed,block abnormal stp tx.\n");
				wmt_lib_set_stp_wmt_last_close(1);
			} else {
				wmt_lib_set_stp_wmt_last_close(0);
			}

		}
		break;

	case WMT_IOCTL_SET_PATCH_NUM:{
			pAtchNum = arg;
			if (pAtchNum == 0 || pAtchNum > MAX_PATCH_NUM) {
				WMT_ERR_FUNC("patch num(%d) == 0 or > %d!\n", pAtchNum, MAX_PATCH_NUM);
				iRet = -1;
				break;
			}

			pPatchInfo = kzalloc(sizeof(WMT_PATCH_INFO) * pAtchNum, GFP_ATOMIC);
			if (!pPatchInfo) {
				WMT_ERR_FUNC("allocate memory fail!\n");
				iRet = -EFAULT;
				break;
			}

			WMT_INFO_FUNC(" get patch num from launcher = %d\n", pAtchNum);
			wmt_lib_set_patch_num(pAtchNum);
		}
		break;

	case WMT_IOCTL_SET_PATCH_INFO:{
			WMT_PATCH_INFO wMtPatchInfo;
			P_WMT_PATCH_INFO pTemp = NULL;
			UINT32 dWloadSeq;
			static UINT32 counter = 0;

			if (!pPatchInfo) {
				WMT_ERR_FUNC("NULL patch info pointer\n");
				break;
			}

			if (copy_from_user(&wMtPatchInfo, (void *)arg, sizeof(WMT_PATCH_INFO))) {
				WMT_ERR_FUNC("copy_from_user failed at %d\n", __LINE__);
				iRet = -EFAULT;
				break;
			}

			dWloadSeq = wMtPatchInfo.dowloadSeq;
			WMT_DBG_FUNC
			    ("current download seq no is %d,patch name is %s,addres info is 0x%02x,0x%02x,0x%02x,0x%02x\n",
			     dWloadSeq, wMtPatchInfo.patchName, wMtPatchInfo.addRess[0],
			     wMtPatchInfo.addRess[1], wMtPatchInfo.addRess[2],
			     wMtPatchInfo.addRess[3]);
			osal_memcpy(pPatchInfo + dWloadSeq - 1, &wMtPatchInfo,
				    sizeof(WMT_PATCH_INFO));
			pTemp = pPatchInfo + dWloadSeq - 1;
			if (++counter == pAtchNum) {
				wmt_lib_set_patch_info(pPatchInfo);
				counter = 0;
			}
		}
		break;

	case WMT_IOCTL_PORT_NAME:{
			INT8 cUartName[NAME_MAX + 1];
			if (copy_from_user(cUartName, (void *)arg, NAME_MAX)) {
				iRet = -EFAULT;
				break;
			}
			cUartName[NAME_MAX] = '\0';
			wmt_lib_set_uart_name(cUartName);
		}
		break;

	case WMT_IOCTL_WMT_CFG_NAME:
		{
			INT8 cWmtCfgName[NAME_MAX + 1];
			if (copy_from_user(cWmtCfgName, (void *)arg, NAME_MAX)) {
				iRet = -EFAULT;
				break;
			}
			cWmtCfgName[NAME_MAX] = '\0';
			wmt_conf_set_cfg_file(cWmtCfgName);
		}
		break;
	case WMT_IOCTL_WMT_QUERY_CHIPID:
		{
#if !(DELETE_HIF_SDIO_CHRDEV)
			iRet = mtk_wcn_hif_sdio_query_chipid(1);
#else
			iRet = mtk_wcn_wmt_chipid_query();
#endif
		}
		break;
	case WMT_IOCTL_WMT_TELL_CHIPID:
		{
#if !(DELETE_HIF_SDIO_CHRDEV)
			iRet = mtk_wcn_hif_sdio_tell_chipid(arg);
#endif

			if (0x6628 == arg || 0x6630 == arg) {
				wmt_lib_merge_if_flag_ctrl(1);
			} else {
				wmt_lib_merge_if_flag_ctrl(0);
			}
		}
		break;
	case WMT_IOCTL_WMT_COREDUMP_CTRL:
		{
			if (0 == arg) {
				mtk_wcn_stp_coredump_flag_ctrl(0);
			} else {
				mtk_wcn_stp_coredump_flag_ctrl(1);
			}
		}
		break;
		case WMT_IOCTL_WMT_STP_ASSERT_CTRL:
			if (MTK_WCN_BOOL_TRUE == wmt_lib_btm_cb(BTM_TRIGGER_STP_ASSERT_OP))
			{
				WMT_INFO_FUNC("trigger stp assert succeed\n");
				iRet = 0;
			}
			else
			{
				WMT_INFO_FUNC("trigger stp assert failed\n");
				iRet = -1;
			}
		break;
	default:
		iRet = -EINVAL;
		WMT_WARN_FUNC("unknown cmd (%d)\n", cmd);
		break;
	}


	return iRet;
}

static int WMT_open(struct inode *inode, struct file *file)
{
	INT32 ret;

	WMT_INFO_FUNC("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);
	if (0 == gWmtInitDone) {
		ret =
		    wait_event_timeout(gWmtInitWq, gWmtInitDone != 0,
				       msecs_to_jiffies(WMT_DEV_INIT_TO_MS));
		if (!ret) {
			WMT_WARN_FUNC("wait_event_timeout (%d)ms,(%ld)jiffies,return -EIO\n",
				      WMT_DEV_INIT_TO_MS, msecs_to_jiffies(WMT_DEV_INIT_TO_MS));
			return -EIO;
		}
	}
	if (atomic_inc_return(&gWmtRefCnt) == 1) {
		WMT_INFO_FUNC("1st call\n");
	}

	return 0;
}

static int WMT_close(struct inode *inode, struct file *file)
{
	WMT_INFO_FUNC("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	if (atomic_dec_return(&gWmtRefCnt) == 0) {
		WMT_INFO_FUNC("last call\n");
	}

	return 0;
}

struct file_operations gWmtFops = {
	.open = WMT_open,
	.release = WMT_close,
	.read = WMT_read,
	.write = WMT_write,
/* .ioctl = WMT_ioctl, */
	.unlocked_ioctl = WMT_unlocked_ioctl,
	.poll = WMT_poll,
};

#if REMOVE_MK_NODE
struct class *wmt_class = NULL;
#endif

#if CONSYS_WMT_REG_SUSPEND_CB_ENABLE
static int wmt_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch(pm_event) {
	case PM_HIBERNATION_PREPARE: /* Going to hibernate */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		mtk_wmt_func_off_background();
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION: /* Hibernation finished */
		mtk_wmt_func_on_background();
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block wmt_pm_notifier_block = {
    .notifier_call = wmt_pm_event,
    .priority = 0,
};
#endif /* CONSYS_WMT_REG_SUSPEND_CB_ENABLE */

static int WMT_init(void)
{
	dev_t devID = MKDEV(gWmtMajor, 0);
	INT32 cdevErr = -1;
	INT32 ret = -1;
#if REMOVE_MK_NODE
	struct device *wmt_dev = NULL;
#endif

	WMT_INFO_FUNC("WMT Version= %s DATE=%s\n", MTK_WMT_VERSION, MTK_WMT_DATE);
	WMT_INFO_FUNC("COMBO Driver Version= %s\n", MTK_COMBO_DRIVER_VERSION);
	/* Prepare a UCHAR device */
	/*static allocate chrdev */
	gWmtInitDone = 0;
	init_waitqueue_head((wait_queue_head_t *) &gWmtInitWq);

	stp_drv_init();

	ret = register_chrdev_region(devID, WMT_DEV_NUM, WMT_DRIVER_NAME);
	if (ret) {
		WMT_ERR_FUNC("fail to register chrdev\n");
		return ret;
	}

	cdev_init(&gWmtCdev, &gWmtFops);
	gWmtCdev.owner = THIS_MODULE;

	cdevErr = cdev_add(&gWmtCdev, devID, WMT_DEV_NUM);
	if (cdevErr) {
		WMT_ERR_FUNC("cdev_add() fails (%d)\n", cdevErr);
		goto error;
	}
	WMT_INFO_FUNC("driver(major %d) installed\n", gWmtMajor);
#if REMOVE_MK_NODE
	wmt_class = class_create(THIS_MODULE, "stpwmt");
	if (IS_ERR(wmt_class))
		goto error;
	wmt_dev = device_create(wmt_class, NULL, devID, NULL, "stpwmt");
	if (IS_ERR(wmt_dev))
		goto error;
#endif

#if 0
	pWmtDevCtx = wmt_drv_create();
	if (!pWmtDevCtx) {
		WMT_ERR_FUNC("wmt_drv_create() fails\n");
		goto error;
	}

	ret = wmt_drv_init(pWmtDevCtx);
	if (ret) {
		WMT_ERR_FUNC("wmt_drv_init() fails (%d)\n", ret);
		goto error;
	}

	WMT_INFO_FUNC("stp_btmcb_reg\n");
	wmt_cdev_btmcb_reg();

	ret = wmt_drv_start(pWmtDevCtx);
	if (ret) {
		WMT_ERR_FUNC("wmt_drv_start() fails (%d)\n", ret);
		goto error;
	}
#endif
	ret = wmt_lib_init();
	if (ret) {
		WMT_ERR_FUNC("wmt_lib_init() fails (%d)\n", ret);
		goto error;
	}
#if CFG_WMT_DBG_SUPPORT
	wmt_dev_dbg_setup();
#endif

#if CFG_WMT_PROC_FOR_AEE
	wmt_dev_proc_for_aee_setup();
#endif

    mtk_wcn_hif_sdio_update_cb_reg(wmt_dev_tra_sdio_update);
#if CONSYS_WMT_REG_SUSPEND_CB_ENABLE
	ret = register_pm_notifier(&wmt_pm_notifier_block);
	if (ret)
		WMT_ERR_FUNC("WMT failed to register PM notifier failed(%d)\n", ret);
#endif
	gWmtInitDone = 1;
	wake_up(&gWmtInitWq);
#if CONSYS_EARLYSUSPEND_ENABLE
    osal_sleepable_lock_init(&g_es_lr_lock);
    register_early_suspend(&wmt_early_suspend_handler);
    WMT_INFO_FUNC("register_early_suspend finished\n");
#endif
    WMT_INFO_FUNC("success \n");
    return 0;

 error:
	wmt_lib_deinit();
#if CFG_WMT_DBG_SUPPORT
	wmt_dev_dbg_remove();
#endif
#if REMOVE_MK_NODE
	if (!IS_ERR(wmt_dev))
		device_destroy(wmt_class, devID);
	if (!IS_ERR(wmt_class)) {
		class_destroy(wmt_class);
		wmt_class = NULL;
	}
#endif

	if (cdevErr == 0) {
		cdev_del(&gWmtCdev);
	}

	if (ret == 0) {
		unregister_chrdev_region(devID, WMT_DEV_NUM);
		gWmtMajor = -1;
	}

	WMT_ERR_FUNC("fail\n");

	return -1;
}

static void WMT_exit(void)
{
	dev_t dev = MKDEV(gWmtMajor, 0);

#if CONSYS_EARLYSUSPEND_ENABLE
    unregister_early_suspend(&wmt_early_suspend_handler);
    osal_sleepable_lock_deinit(&g_es_lr_lock);
    WMT_INFO_FUNC("unregister_early_suspend finished\n");
#endif

#if CONSYS_WMT_REG_SUSPEND_CB_ENABLE
	unregister_pm_notifier(&wmt_pm_notifier_block);
#endif
    wmt_lib_deinit();
    
#if CFG_WMT_DBG_SUPPORT
	wmt_dev_dbg_remove();
#endif

#if CFG_WMT_PROC_FOR_AEE
	wmt_dev_proc_for_aee_remove();
#endif

	cdev_del(&gWmtCdev);
	unregister_chrdev_region(dev, WMT_DEV_NUM);
	gWmtMajor = -1;
#if REMOVE_MK_NODE
	device_destroy(wmt_class, MKDEV(gWmtMajor, 0));
	class_destroy(wmt_class);
	wmt_class = NULL;
#endif
#ifdef MTK_WMT_WAKELOCK_SUPPORT
	WMT_WARN_FUNC("destroy func_on_off_wake_lock\n");
	wake_lock_destroy(&func_on_off_wake_lock);
#endif

	stp_drv_exit();

	WMT_INFO_FUNC("done\n");
}

#ifdef MTK_WCN_REMOVE_KERNEL_MODULE

INT32 mtk_wcn_combo_common_drv_init(VOID)
{
	return WMT_init();

}

VOID mtk_wcn_combo_common_drv_exit(VOID)
{
	return WMT_exit();
}


EXPORT_SYMBOL(mtk_wcn_combo_common_drv_init);
EXPORT_SYMBOL(mtk_wcn_combo_common_drv_exit);
#else
module_init(WMT_init);
module_exit(WMT_exit);
#endif
/* MODULE_LICENSE("Proprietary"); */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc WCN");
MODULE_DESCRIPTION("MTK WCN combo driver for WMT function");

module_param(gWmtMajor, uint, 0);

/*
 * Broadcom Dongle Host Driver (DHD), Linux-specific network interface
 * Selected code segments for XR
 *
 * Portions of this code are copyright (c) 2023 Cypress Semiconductor Corporation,
 * an Infineon company
 *
 * This program is the proprietary software of infineon and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and infineon (an "Authorized License").
 * Except as set forth in an Authorized License, infineon grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and infineon expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY INFINEON AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of infineon, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of infineon
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND INFINEON MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  INFINEON SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * INFINEON OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF INFINEON HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Infineon-WL-IPTag/Open:>>
 *
 * $Id$
 */

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <bcmstdlib_s.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/ip.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <net/addrconf.h>
#ifdef ENABLE_ADAPTIVE_SCHED
#include <linux/cpufreq.h>
#endif /* ENABLE_ADAPTIVE_SCHED */
#include <linux/rtc.h>
#include <linux/namei.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <dhd_linux_priv.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
#include <uapi/linux/sched/types.h>
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0) */

#include <epivers.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmdevs.h>
#include <bcmiov.h>

#include <ethernet.h>
#include <bcmevent.h>
#include <vlan.h>
#include <802.3.h>

#include <dhd_linux_wq.h>
#include <dhd.h>
#include <dhd_linux.h>
#include <dhd_linux_pktdump.h>
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <dhd_dbg_ring.h>
#include <dhd_debug.h>
#if defined(WL_CFG80211)
#include <wl_cfg80211.h>
#endif	/* WL_CFG80211 */

#ifdef WL_DHD_XR
#include <wl_cfg80211_xr.h>
#endif /* WL_DHD_XR */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
#include <uapi/linux/sched/types.h>
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0) */

#include <dhd_daemon.h>

#ifdef WL_DHD_XR
/* XR cmd thread priority */
int dhd_xr_cmd_prio = CUSTOM_XR_CMD_PRIO_SETTING;
module_param(dhd_xr_cmd_prio, int, 0);

int dhd_send_xr_cmd(dhd_pub_t *dest_dhdp, void *xr_cmd,
	int len, struct completion *comp, bool sync);
static void dhd_os_xrcmdlock(dhd_pub_t *pub);
static void dhd_os_xrcmdunlock(dhd_pub_t *pub);

static inline void dhd_xr_cmd_clr_store_buf(dhd_pub_t *dhdp)
{
	uint32 store_idx;
	xr_buf_t *xr_buf = NULL;
	xr_ctx_t *xr_ctx = NULL;

	if (!dhdp) {
		DHD_ERROR(("%s: NULL dhdp!!!\n", __func__));
		return;
	}

	xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(dhdp);
	dhd_os_xrcmdlock(dhdp);
	store_idx = xr_ctx->xr_cmd_store_idx;
	xr_buf = (xr_buf_t *) &xr_ctx->xr_cmd_buf[store_idx];
	if (xr_buf->in_use) {
		bzero(&xr_ctx->xr_cmd_buf[store_idx], sizeof(xr_buf_t));
		xr_ctx->xr_cmd_buf[store_idx].in_use = FALSE;
		xr_ctx->xr_cmd_buf[store_idx].sync = FALSE;
		xr_buf->in_use = FALSE;
		xr_buf->sync = FALSE;
		DHD_ERROR(("%s:flush cmdbuf %p, store idx %d\n", __func__,
			xr_buf, store_idx));
	}
	dhd_os_xrcmdunlock(dhdp);
	return;

}
static inline int dhd_xr_cmd_enqueue(dhd_pub_t *dhdp, void *cmd, int len, bool sync)
{
	uint32 store_idx;
	uint32 sent_idx;
	xr_buf_t *xr_buf = NULL;
	xr_ctx_t *xr_ctx = NULL;

	if (!dhdp) {
		DHD_ERROR(("%s: NULL dhdp!!!\n", __func__));
		return BCME_ERROR;
	}

	xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(dhdp);

	if (!cmd) {
		DHD_ERROR(("%s: NULL cmd!!!\n", __func__));
		return BCME_ERROR;
	}

	dhd_os_xrcmdlock(dhdp);
	store_idx = xr_ctx->xr_cmd_store_idx;
	sent_idx = xr_ctx->xr_cmd_sent_idx;
	xr_buf = (xr_buf_t *) &xr_ctx->xr_cmd_buf[store_idx];
	if (xr_buf->in_use) {
		dhd_os_xrcmdunlock(dhdp);
		dhd_xr_cmd_clr_store_buf(dhdp);
		dhd_os_xrcmdlock(dhdp);
		DHD_TRACE(("%s:clear stale cmdbuf %p, store idx %d sent idx %d\n", __func__,
			cmd, store_idx, sent_idx));
	}
	DHD_TRACE(("%s: Store cmd %p. idx %d -> %d\n", __func__,
		cmd, store_idx, (store_idx + 1) & (MAX_XR_CMD_NUM - 1)));
	xr_buf->in_use = TRUE;
	if (sync)
		xr_buf->sync = sync;
	xr_buf->len = len;
	bcopy((char *) cmd, (char *)&xr_buf->buf, xr_buf->len);
	xr_ctx->xr_cmd_store_idx = (store_idx + 1) & (MAX_XR_CMD_NUM - 1);
	dhd_os_xrcmdunlock(dhdp);

	return BCME_OK;
}

static inline int dhd_xr_cmd_dequeue(dhd_pub_t *dhdp, xr_buf_t *xr_buf)
{
	uint32 store_idx;
	uint32 sent_idx;
	int ret = BCME_OK;
	xr_buf_t *curr_buf;
	xr_ctx_t *xr_ctx = NULL;

	if (!dhdp) {
		DHD_ERROR(("%s: NULL dhdp!!!\n", __func__));
		return BCME_ERROR;
	}

	xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(dhdp);

	if (!xr_buf) {
		DHD_ERROR(("%s:xr_buf is null\n", __func__));
		return BCME_ERROR;
	}

	dhd_os_xrcmdlock(dhdp);

	store_idx = xr_ctx->xr_cmd_store_idx;
	sent_idx = xr_ctx->xr_cmd_sent_idx;

	curr_buf = &xr_ctx->xr_cmd_buf[sent_idx];
	bcopy(curr_buf, xr_buf, sizeof(xr_buf_t));
	bzero(&xr_ctx->xr_cmd_buf[sent_idx], sizeof(xr_buf_t));
	xr_ctx->xr_cmd_buf[sent_idx].in_use = FALSE;
	xr_ctx->xr_cmd_buf[sent_idx].sync = FALSE;
	curr_buf->in_use = FALSE;
	curr_buf->sync = FALSE;

	xr_ctx->xr_cmd_sent_idx = (sent_idx + 1) & (MAX_XR_CMD_NUM - 1);

	if (xr_buf->in_use == FALSE) {
		dhd_os_xrcmdunlock(dhdp);
		DHD_ERROR(("%s: Dequeued cmd is NULL, store idx %d sent idx %d\n", __func__,
			store_idx, sent_idx));
		return BCME_ERROR;
	}

	DHD_TRACE(("%s:xr_buf(%p), sent idx %d\n", __func__,
		xr_buf, sent_idx));

	dhd_os_xrcmdunlock(dhdp);

	return ret;
}

int
dhd_xr_cmd_thread(void *data)
{
	tsk_ctl_t *tsk = (tsk_ctl_t *)data;
	dhd_info_t *dhd = (dhd_info_t *)tsk->parent;
	dhd_pub_t *pub = &dhd->pub;
	int ret = BCME_OK;
	/* This thread doesn't need any user-level access,
	 * so get rid of all our resources
	 */
	if (dhd_xr_cmd_prio > 0) {
		struct sched_param param;
		param.sched_priority =
			(dhd_xr_cmd_prio < MAX_RT_PRIO)?dhd_xr_cmd_prio:(MAX_RT_PRIO-1);
		setScheduler(current, SCHED_FIFO, &param);
	}

#ifdef CUSTOM_SET_CPUCORE
	dhd->pub.current_xr_cmd = current;
#endif /* CUSTOM_SET_CPUCORE */
	/* Run until signal received */
	while (1) {
		if (down_interruptible(&tsk->sema) == 0) {
			xr_buf_t *xr_buf = NULL;
		/*
		 * xr_buf is freed by xr_cmd_handler,if cmd is handled
		 * in wq xr_buf is freed in the wq handler.
		*/

			xr_buf = (xr_buf_t *)kzalloc(sizeof(xr_buf_t), GFP_KERNEL);
#ifdef ENABLE_ADAPTIVE_SCHED
			dhd_sched_policy(dhd_xr_cmd_prio);
#endif /* ENABLE_ADAPTIVE_SCHED */

			SMP_RD_BARRIER_DEPENDS();
			if (tsk->terminated) {
				break;
			}

			ret = dhd_xr_cmd_dequeue(pub, xr_buf);

			if (ret != BCME_OK) {
				DHD_ERROR(("%s:Dequeue failed\n", __func__));
				if (xr_buf)
					kfree(xr_buf);
				continue;
			} else {
				ret = xr_cmd_handler(pub, xr_buf);
				if (ret != BCME_OK) {
					DHD_ERROR(("xr_cmd_handler failure\n"));
					continue;
				}
			/*
			 * xr_buf is handled by xr_cmd_handler so we need
			 * to make sure xr_buf is not dangling.
			 */
				xr_buf = NULL;
			}

			DHD_OS_WAKE_UNLOCK(pub);
		} else {
			break;
		}
	}
	COMPLETE_AND_EXIT(&tsk->completed, 0);

	return 0;
}

int
dhd_send_xr_cmd(dhd_pub_t *dest_dhdp, void *xr_cmd, int len, struct completion *comp, bool sync)
{
	dhd_info_t *dhd = (dhd_info_t *)dest_dhdp->info;
	int ret = BCME_OK;

	if (sync) {
		if (comp == NULL) {
			DHD_ERROR(("%s:sync command but completion variable is null\n", __func__));
			ret = BCME_ERROR;
			goto end;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
		init_completion(comp);
#else
		/* reinitialize completion to clear previous count */
		INIT_COMPLETION(comp);
#endif // endif
	}
	DHD_OS_WAKE_LOCK(dest_dhdp);
	ret = dhd_xr_cmd_enqueue(dest_dhdp, xr_cmd, len, sync);

	if (ret == BCME_ERROR) {
		DHD_ERROR(("send cmd retry failed\n"));
		DHD_OS_WAKE_UNLOCK(dest_dhdp);
		goto end;
	} else {
		if (dhd->thr_xr_cmd_ctl.thr_pid >= 0) {
			up(&dhd->thr_xr_cmd_ctl.sema);
		}
	}

	if (sync) {
		if (wait_for_completion_timeout(comp,
				msecs_to_jiffies(2000)) == 0) {
			DHD_ERROR(("dhd_send_xr_cmd: timeout occurred\n"));
			ret = BCME_ERROR;
			DHD_OS_WAKE_UNLOCK(dest_dhdp);
			goto end;
		}
	}
end:
	return ret;
}

void
dhd_deferred_work_xr_cmd_handler(void *handle, void *event_info, u8 event)
{
	dhd_info_t *dhd_info = handle;
	dhd_pub_t *dhd = &dhd_info->pub;
	xr_buf_t *xr_buf = (xr_buf_t *) event_info;
	int ret = BCME_OK;

	if (event != DHD_WQ_WORK_XR_CMD_HANDLER) {
		DHD_ERROR(("%s: unexpected event \n", __FUNCTION__));
		goto fail;
	}

	ret = xr_cmd_deferred_handler(dhd, xr_buf);

	if (ret != BCME_OK) {
		DHD_ERROR(("xr_cmd_deferred_handler failure\n"));
	}

fail:
	if (xr_buf)
		kfree(xr_buf);

	return;
}
/*	dhd_wq_xr_cmd_handler handles xr cmd through a workq
*	which allows the xr_cmd_thread to unblock.
*/
void dhd_wq_xr_cmd_handler(dhd_pub_t *dhdp, void *info)
{

	dhd_info_t *dhd_info = dhdp->info;

	dhd_deferred_schedule_work(dhd_info->dhd_deferred_wq, (void *)info,
			DHD_WQ_WORK_XR_CMD_HANDLER,
			dhd_deferred_work_xr_cmd_handler,
			DHD_WQ_WORK_PRIORITY_LOW);
	return;
}

static void
dhd_os_xrcmdlock(dhd_pub_t *pub)
{
	dhd_info_t *dhd;

	dhd = (dhd_info_t *)(pub->info);
	spin_lock_bh(&dhd->xr_cmd_lock);

}

static void
dhd_os_xrcmdunlock(dhd_pub_t *pub)
{
	dhd_info_t *dhd;

	dhd = (dhd_info_t *)(pub->info);
	spin_unlock_bh(&dhd->xr_cmd_lock);
}

#endif /* WL_DHD_XR */

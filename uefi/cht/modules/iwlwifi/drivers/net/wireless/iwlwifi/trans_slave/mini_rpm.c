/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * All rights reserved.
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
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include "shared.h"

static void mini_rpm_resume_work(struct work_struct *work)
{
	struct iwl_trans_slv *trans_slv;
	struct iwl_trans *trans;
	unsigned long flags;
	int ret;

	trans_slv = container_of(work, struct iwl_trans_slv, rpm_resume_work);
	trans = IWL_TRANS_SLV_GET_IWL_TRANS(trans_slv);

	spin_lock_irqsave(&trans_slv->rpm_lock, flags);

	/* suspend_work might have exited prematurely, so don't WARN_ON */
	if (!trans_slv->suspended) {
		spin_unlock_irqrestore(&trans_slv->rpm_lock, flags);
		return;
	}

	trans_slv->suspended = false;
	spin_unlock_irqrestore(&trans_slv->rpm_lock, flags);

	ret = trans_slv->rpm_config.runtime_resume(trans);
	if (ret)
		IWL_ERR(trans, "resume error: %d\n", ret);
}

static void mini_rpm_suspend_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct iwl_trans_slv *trans_slv;
	struct iwl_trans *trans;
	unsigned long flags;
	int ret;

	dwork = container_of(work, struct delayed_work, work);
	trans_slv = container_of(dwork, struct iwl_trans_slv, rpm_suspend_work);
	trans = IWL_TRANS_SLV_GET_IWL_TRANS(trans_slv);

	spin_lock_irqsave(&trans_slv->rpm_lock, flags);

	/* we might have resumed in the meantime */
	if (trans_slv->refcount != 0) {
		spin_unlock_irqrestore(&trans_slv->rpm_lock, flags);
		return;
	}

	if (WARN_ON(trans_slv->suspended)) {
		spin_unlock_irqrestore(&trans_slv->rpm_lock, flags);
		return;
	}

	trans_slv->suspended = true;
	spin_unlock_irqrestore(&trans_slv->rpm_lock, flags);

	ret = trans_slv->rpm_config.runtime_suspend(trans);
	if (ret)
		IWL_ERR(trans, "suspend error: %d\n", ret);
}

int mini_rpm_init(struct iwl_trans_slv *trans_slv,
		  struct slv_mini_rpm_config *rpm_config)
{
	INIT_DELAYED_WORK(&trans_slv->rpm_suspend_work, mini_rpm_suspend_work);
	INIT_WORK(&trans_slv->rpm_resume_work, mini_rpm_resume_work);
	spin_lock_init(&trans_slv->rpm_lock);
	trans_slv->rpm_wq = alloc_workqueue("slv_rpm_wq", WQ_UNBOUND, 1);
	trans_slv->suspended = false;
	trans_slv->refcount = 1;
	memcpy(&trans_slv->rpm_config, rpm_config, sizeof(*rpm_config));

	return 0;
}

void mini_rpm_destroy(struct iwl_trans_slv *trans_slv)
{
	flush_delayed_work(&trans_slv->rpm_suspend_work);
	flush_workqueue(trans_slv->rpm_wq);
	destroy_workqueue(trans_slv->rpm_wq);
	trans_slv->rpm_wq = NULL;
}

void mini_rpm_get(struct iwl_trans_slv *trans_slv)
{
	struct iwl_trans *trans = IWL_TRANS_SLV_GET_IWL_TRANS(trans_slv);
	unsigned long flags;

	if (WARN_ON_ONCE(!trans_slv->rpm_wq))
		return;

	spin_lock_irqsave(&trans_slv->rpm_lock, flags);

	IWL_DEBUG_RPM(trans, "refcount=%d\n", trans_slv->refcount);
	trans_slv->refcount++;
	if (trans_slv->refcount == 1) {
		if (trans_slv->suspended) {
			IWL_DEBUG_RPM(trans, "queue resume work\n");
			queue_work(trans_slv->rpm_wq,
				   &trans_slv->rpm_resume_work);
		} else {
			IWL_DEBUG_RPM(trans, "cancel suspend work\n");
			__cancel_delayed_work(&trans_slv->rpm_suspend_work);
		}
	}

	spin_unlock_irqrestore(&trans_slv->rpm_lock, flags);
}

void mini_rpm_put(struct iwl_trans_slv *trans_slv)
{
	struct iwl_trans *trans = IWL_TRANS_SLV_GET_IWL_TRANS(trans_slv);
	unsigned long flags;

	if (WARN_ON_ONCE(!trans_slv->rpm_wq))
		return;

	spin_lock_irqsave(&trans_slv->rpm_lock, flags);

	IWL_DEBUG_RPM(trans, "refcount=%d\n", trans_slv->refcount);
	if (WARN_ON(trans_slv->refcount == 0))
		goto unlock;

	trans_slv->refcount--;
	if (trans_slv->refcount == 0) {
		unsigned long delay =
		    msecs_to_jiffies(trans_slv->rpm_config.autosuspend_delay);
		IWL_DEBUG_RPM(trans, "queue suspend work\n");
		queue_delayed_work(trans_slv->rpm_wq,
				   &trans_slv->rpm_suspend_work, delay);
	}

unlock:
	spin_unlock_irqrestore(&trans_slv->rpm_lock, flags);
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/wait.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <uapi/linux/dma-heap.h>

#include "mtk_heap.h"

#include "mtk-aov-config.h"
#include "mtk-aov-drv.h"
#include "mtk-aov-core.h"
#include "mtk-aov-data.h"

#include "slbc_ops.h"
#include "scp.h"

#if AOV_EVENT_IN_PLACE
#define ALIGN16(x) ((void *)(((uint64_t)x + 0x0F) & ~0x0F))
#else
#define ALIGN16(x) (x)
#endif  // AOV_EVENT_IN_PLACE

static struct mtk_aov *curr_dev;

struct mtk_aov *aov_core_get_device(void)
{
	return curr_dev;
}

static void *buffer_acquire(struct aov_core *core_info)
{
	unsigned long flag, empty;
	struct buffer *buffer = NULL;

	spin_lock_irqsave(&core_info->event_lock, flag);
	empty = list_empty(&core_info->event_list);
	if (!empty) {
		buffer = list_first_entry(&core_info->event_list, struct buffer, entry);
		list_del(&buffer->entry);
	}
	spin_unlock_irqrestore(&core_info->event_lock, flag);

	return buffer ? &(buffer->data) : NULL;
}

static void buffer_release(struct aov_core *core_info, void *data)
{
	unsigned long flag;
	struct buffer *buffer =
		(struct buffer *)((uintptr_t)data - offsetof(struct buffer, data));

	spin_lock_irqsave(&core_info->event_lock, flag);
	list_add_tail(&buffer->entry, &core_info->event_list);
	spin_unlock_irqrestore(&core_info->event_lock, flag);
}

static int ipi_receive(unsigned int id, void *unused,
		void *data, unsigned int len)
{
	struct mtk_aov *aov_dev = aov_core_get_device();
	struct aov_core *core_info = &aov_dev->core_info;
	uint32_t aov_ready;
	uint32_t aov_session;
	struct packet *packet;
	struct aov_event *event;
	void *buffer;

	aov_ready = atomic_read(&(core_info->aov_ready));

	pr_debug("%s: receive id(%d), len(%d), ready(%d)\n", __func__, id, len, aov_ready);

	if (aov_ready == 0) {
		pr_info("%s: aov is not started", __func__);
		return 0;
	}

	packet = (struct packet *)data;
	if (packet->command & AOV_SCP_CMD_ACK) {
		uint32_t cmd = packet->command & ~AOV_SCP_CMD_ACK;

		if ((cmd > 0) && (cmd < AOV_SCP_CMD_MAX)) {
			atomic_set(&(core_info->ack_cmd[cmd]), 1);
			wake_up_interruptible(&core_info->ack_wq[cmd]);
		}
	} else {
		event = (struct aov_event *)(core_info->buf_va +
			(packet->buffer - core_info->buf_pa));

		aov_session = atomic_read(&(core_info->aov_session));

		if (event->session != aov_session) {
			pr_info("%s: invalid aov session mismatch(%d/%d)\n", __func__,
				event->session, aov_session);
			return 0;
		}

		buffer = buffer_acquire(core_info);
		if (buffer == NULL) {
			pr_info("%s: failed to acquire message\n", __func__);
			return 0;
		}

		memcpy(buffer, (void *)event, sizeof(struct aov_event));

		queue_push(&(core_info->queue), buffer);
		wake_up_interruptible(&core_info->poll_wq);
	}

	return 0;
}

static int send_cmd_internal(struct aov_core *core_info,
	struct packet *pkt, bool wait)
{
	int retry;
	int ret;
	unsigned long timeout;
	int scp_ready;

	pr_info("%s: send cmd(%d), session(%d)\n",
		__func__, pkt->command, pkt->session);

	retry = 0;
	do {
		if (wait) {
			timeout = msecs_to_jiffies(AOV_TIMEOUT_MS);
			ret = wait_event_interruptible_timeout(core_info->scp_queue,
				((scp_ready = atomic_read(&(core_info->scp_ready))) != 0), timeout);
			if (ret == 0) {
				pr_info("%s: send cmd(%d/%d) time out !\n",
					__func__, pkt->command, scp_ready);
				return -EIO;
			} else if (-ERESTARTSYS == ret) {
				pr_info("%s: send cmd(%d/%d) interrupted !\n",
					__func__, pkt->command, scp_ready);
				return -ERESTARTSYS;
			}
			pr_debug("%s: send cmd(%d/%d) done\n",
					__func__, pkt->command, scp_ready);
		} else {
			scp_ready = atomic_read(&(core_info->scp_ready));
		}

		if (scp_ready != 2) {
			pr_info("%s: scp not ready(%d)\n", __func__, scp_ready);
			return -EIO;
		}

		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_AOV_SCP,
			IPI_SEND_WAIT, pkt, 4, AOV_TIMEOUT_MS);
		if (ret < 0 && ret != IPI_PIN_BUSY) {
			pr_info("%s: failed to send packet: %d", __func__, ret);
			break;
		}
		if (ret == IPI_PIN_BUSY) {
			if (retry++ >= 1000)
				return -EBUSY;
			if (retry % 100 == 0)
				usleep_range(1000, 2000);
		}
	} while (ret == IPI_PIN_BUSY);

	return 0;
}

int aov_core_send_cmd(struct mtk_aov *aov_dev, uint32_t cmd,
	void *data, int len, bool ack)
{
	struct aov_core *core_info = &aov_dev->core_info;
	unsigned long flag;
	uint8_t *buf;
	int ret;
	struct packet pkt;
	unsigned long timeout;

	if (aov_dev->op_mode == 0) {
		pr_info("%s: bypass send command(%d)", __func__, cmd);
		return 0;
	}

	pr_info("%s+", __func__);

	if (data && len) {
		if (cmd == AOV_SCP_CMD_INIT) {
			if (atomic_read(&(core_info->aov_ready))) {
				pr_info("%s: invalid aov ready state\n");
				return -EIO;
			}

			spin_lock_irqsave(&core_info->buf_lock, flag);
			buf = tlsf_malloc(&(core_info->alloc), sizeof(struct aov_init));
			spin_unlock_irqrestore(&core_info->buf_lock, flag);
		} else {
			spin_lock_irqsave(&core_info->buf_lock, flag);
			buf = tlsf_malloc(&(core_info->alloc), len);
			spin_unlock_irqrestore(&core_info->buf_lock, flag);
		}
		if (buf) {
			if (cmd == AOV_SCP_CMD_INIT) {
				struct aov_user user;
				struct aov_init *init = (struct aov_init *)buf;

				pr_info("%s: init buffer %x, size (%d/%d)\n", __func__,
					buf, sizeof(struct aov_init), sizeof(struct aov_event));

				pr_debug("%s: copy aov user data %x, %d+\n",
					__func__, data, sizeof(struct aov_user));
				ret = copy_from_user((void *)&user,
					(void *)data, sizeof(struct aov_user));
				pr_debug("%s: copy aov user data %x, %d-\n",
					__func__, data, sizeof(struct aov_user));
				if (ret) {
					pr_info("%s: failed to copy aov user data: %d\n",
						__func__, ret);
					return -EFAULT;
				}

				memcpy(init, &user, AOV_MAX_USER_SIZE);

				if ((user.aaa_size > 0) &&
					(user.aaa_size <= AOV_MAX_AAA_SIZE)) {
					pr_debug("%s: copy aaa info %x, %d+\n",
						__func__, user.aaa_info, user.aaa_size);
					(void)copy_from_user(&(init->aaa_info),
						user.aaa_info, user.aaa_size);
					pr_debug("%s: copy aaa info %x, %d-\n",
						__func__, user.aaa_info, user.aaa_size);
				} else if (user.aaa_size > AOV_MAX_AAA_SIZE) {
					pr_info("%s: aaa info size(%d) overflow\n",
						__func__, user.aaa_size);
					return -ENOMEM;
				}

				if ((user.tuning_size > 0) &&
					(user.tuning_size <= AOV_MAX_TUNING_SIZE)) {
					pr_debug("%s: copy tuning info %x, %d+\n",
						__func__, user.tuning_info, user.tuning_size);
					(void)copy_from_user(&(init->tuning_info),
						user.tuning_info, user.tuning_size);
					pr_debug("%s: copy tuning info %x, %d-\n",
						__func__, user.tuning_info, user.tuning_size);
				} else if (user.tuning_size > AOV_MAX_TUNING_SIZE) {
					pr_info("%s: tuning info size(%d) overflow\n",
						__func__, user.tuning_size);
					return -ENOMEM;
				}
				/* set seninf aov parameters for scp use and
				 * switch i2c bus aux function here on scp side.
				 */
				pr_info("mtk_cam_seninf_s_aov_param(%d)+\n", user.sensor_id);
				ret = mtk_cam_seninf_s_aov_param(user.sensor_id,
					(void *)&(init->senif_info));
				if (ret < 0)
					pr_info(
						"mtk_cam_seninf_s_aov_param(%d) fail, ret: %d\n",
						user.sensor_id, ret);
				pr_info("mtk_cam_seninf_s_aov_param(%d)-\n", user.sensor_id);

				core_info->sensor_id = user.sensor_id;

				/* suspend and set clk parent here to prevent enque
				 * racing issue when power on/off on scp side.
				 */
				pr_info(
					"mtk_cam_seninf_aov_runtime_suspend(%d)+\n",
					core_info->sensor_id);
				ret = mtk_cam_seninf_aov_runtime_suspend(core_info->sensor_id);
				if (ret < 0)
					pr_info(
					"mtk_cam_seninf_aov_runtime_suspend(%d) fail, ret: %d\n",
					core_info->sensor_id, ret);
				pr_info(
					"mtk_cam_seninf_aov_runtime_suspend(%d)-\n",
					core_info->sensor_id);

				pr_info("mtk_aie_aov_memcpy+\n");
				mtk_aie_aov_memcpy((void *)&(init->aie_info));
				pr_info("mtk_aie_aov_memcpy-\n");

				/* debug use
				 * pr_info("out_pad %d\n", init->aov_seninf_param.vc.out_pad);
				 * pr_info("init->aov_seninf_param.sensor_idx%d\n",
				 *    init->aov_seninf_param.sensor_idx);
				 * pr_info("width %d\n", init->aov_seninf_param.width);
				 * pr_info("height %d\n", init->aov_seninf_param.height);
				 * pr_info("isp_freq %d\n", init->aov_seninf_param.isp_freq);
				 * pr_info("camtg %d\n", init->aov_seninf_param.camtg);
				 */
				init->session = user.session;

				atomic_set(&(core_info->aov_session), user.session);

				len = sizeof(struct aov_init);
			} else if (cmd == AOV_SCP_CMD_NOTIFY) {
				memcpy(buf, (void *)data, sizeof(struct aov_notify));
			} else {
				pr_info("%s: data buffer 0x%08x, size %d", __func__, buf, len);
				(void)copy_from_user(buf, (void *)data, len);
			}
		} else {
			pr_info("%s: failed to alloc buffer size(%d)", __func__, len);
			return -ENOMEM;
		}
#if AOV_SLB_ALLOC_FREE
	} else if (cmd == AOV_SCP_CMD_PWR_OFF) {
		struct slbc_data slb;

		slb.uid = UID_AOV_DC;
		slb.type = TP_BUFFER;
		ret = slbc_request(&slb);
		if (ret < 0) {
			pr_info("%s: failed to allocate slb buffer", __func__);
			return ret;
		}

		pr_info("%s: slb buffer base(0x%x), size(%ld): %x",
			__func__, (uintptr_t)slb.paddr, slb.size);

		buf = slb.paddr;
		len = slb.size;
#endif  // AOV_SLB_ALLOC_FREE
	} else {
		buf = NULL;
	}

	pkt.session = atomic_read(&(core_info->scp_session));
	pkt.command = cmd;

	if (ack)
		pkt.command |= AOV_SCP_CMD_ACK;

	if (buf) {
		pkt.buffer = core_info->buf_pa + (buf - core_info->buf_va);
		pkt.length = len;
	} else {
		pkt.buffer = 0;
		pkt.length = 0;
	}

	if (cmd == AOV_SCP_CMD_INIT) {
		struct aov_init *init = (struct aov_init *)buf;

		if (init == NULL) {
			pr_info("%s: invalid null aov init info", __func__);
			return -ENOMEM;
		}

		// Init queue to receive event
		queue_init(&(core_info->queue));

		// Setup debug mode
		atomic_set(&(core_info->debug_mode), init->debug_mode);

		// Setup display mode
		init->disp_mode = atomic_read(&(core_info->disp_mode));

		// Setup aie available
		init->aie_avail = atomic_read(&(core_info->aie_avail));

		// Record aov_init buffer
		core_info->aov_init = init;

		atomic_set(&(core_info->aov_ready), 1);
	} else if (cmd == AOV_SCP_CMD_PWR_OFF) {
		atomic_set(&(core_info->disp_mode), AOV_DISP_MODE_OFF);
	} else if (cmd == AOV_SCP_CMD_PWR_ON) {
		atomic_set(&(core_info->disp_mode), AOV_DiSP_MODE_ON);
	} else if (cmd == AOV_SCP_CMD_NOTIFY) {
		struct aov_notify *notify = (struct aov_notify *)buf;

		atomic_set(&(core_info->aie_avail), notify->status);
	}

	if (atomic_read(&(core_info->aov_ready))) {
		if (ack)
			atomic_set(&(core_info->ack_cmd[cmd]), 0);

		ret = send_cmd_internal(core_info, &pkt, true);
		if (ret < 0) {
			dev_info(aov_dev->dev, "%s: failed to send cmd(%d)\n", __func__, cmd);
			return ret;
		}

		if (ack) {
			timeout = msecs_to_jiffies(AOV_TIMEOUT_MS);
			ret = wait_event_interruptible_timeout(core_info->ack_wq[cmd],
				atomic_cmpxchg(&(core_info->ack_cmd[cmd]), 1, 0), timeout);
			if (ret == 0) {
				dev_info(aov_dev->dev, "%s: wait ack cmd(%d) timeout\n",
					__func__, cmd);
				/*
				 * clear un-success event to prevent unexpected flow
				 * cauesd be remaining data
				 */
				return -EIO;
			} else if (-ERESTARTSYS == ret) {
				dev_info(aov_dev->dev, "%s: wait cmd(%d) ack interrupted\n",
					__func__, cmd);
				return -ERESTARTSYS;
			}

			dev_dbg(aov_dev->dev, "%s: wait cmd(%d) ack done\n",
					__func__, cmd);
		}
	}

	if (cmd == AOV_SCP_CMD_INIT) {
		// Free aov_init buffer
		spin_lock_irqsave(&core_info->buf_lock, flag);
		tlsf_free(&(core_info->alloc), core_info->aov_init);
		spin_unlock_irqrestore(&core_info->buf_lock, flag);
	} else if (cmd == AOV_SCP_CMD_PWR_ON) {
#if AOV_SLB_ALLOC_FREE
		struct slbc_data slb;

		slb.uid = UID_AOV_DC;
		slb.type = TP_BUFFER;
		ret = slbc_release(&slb);
		if (ret < 0) {
			pr_info("failed to release slb buffer");
			return ret;
		}
#else
		struct slbc_data slb;

		slb.uid = UID_AOV_DC;
		slb.type = TP_BUFFER;
		ret = slbc_status(&slb);
		if (ret > 0) {
			dev_info(aov_dev->dev, "%s: force release slb(%d)\n", __func__, ret);

			slb.uid = UID_AOV_DC;
			slb.type = TP_BUFFER;
			ret = slbc_release(&slb);
			if (ret < 0)
				pr_info("%s: failed to release slb buffer\n", __func__);
		}
#endif  // AOV_SLB_ALLOC_FREE
	} else if (cmd == AOV_SCP_CMD_NOTIFY) {
		// Free aov_notify buffer
		spin_lock_irqsave(&core_info->buf_lock, flag);
		tlsf_free(&(core_info->alloc), buf);
		spin_unlock_irqrestore(&core_info->buf_lock, flag);
	} else if (cmd == AOV_SCP_CMD_DEINIT) {
		atomic_set(&(core_info->aov_ready), 0);

		// Reset queue to empty
		while (!queue_empty(&(core_info->queue))) {
			buf = queue_pop(&(core_info->queue));
			if (buf)
				buffer_release(core_info, buf);
		}

		queue_deinit(&(core_info->queue));

		pr_info("mtk_cam_seninf_aov_runtime_resume(%d)+\n", core_info->sensor_id);
		mtk_cam_seninf_aov_runtime_resume(core_info->sensor_id);
		pr_info("mtk_cam_seninf_aov_runtime_resume(%d)-\n", core_info->sensor_id);
	}

	dev_info(aov_dev->dev, "%s: mtk_ipi_send: %d-\n", __func__, len);

	pr_info("%s-\n", __func__);

	return 0;
}

int aov_core_notify(struct mtk_aov *aov_dev,
	void *data, bool enable)
{
	int ret;
	struct sensor_notify notify;
	int index;

	pr_info("%s+\n", __func__);

	ret = copy_from_user((void *)&notify,
		(void *)data, sizeof(struct sensor_notify));
	if (ret) {
		dev_info(aov_dev->dev, "%s: failed to copy senor notification: %d\n",
			__func__, ret);
		return -EFAULT;
	}

	if (notify.count >= AOV_MAX_SENSOR_COUNT) {
		dev_info(aov_dev->dev, "%s: invalid sensor notify count(%d)\n",
			__func__, notify.count);
		return -EINVAL;
	}


	for (index = 0; index < notify.count; index++) {
		dev_info(aov_dev->dev, "%s: notify sensor(%d), status(%d)\n",
			__func__, notify.sensor[index], enable);
	}

	pr_info("%s-\n", __func__);

	return 0;
}

static int scp_state_notify(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct mtk_aov *aov_dev = aov_core_get_device();
	struct aov_core *core_info = &aov_dev->core_info;
	struct packet pkt;
	uint32_t session;

	if (event == SCP_EVENT_STOP) {
		pr_info("%s: receive scp stop(%d)\n", __func__, event);
		atomic_set(&(core_info->scp_ready), 1);
	} else if (event == SCP_EVENT_READY) {
		atomic_set(&(core_info->scp_ready), 2);

		session = atomic_fetch_add(1, &(core_info->scp_session)) + 1;

		pr_info("%s: scp start event(%d), session(%d)\n",
			__func__, event, session);

		pkt.session = session;
		pkt.command = AOV_SCP_CMD_READY;
		pkt.buffer  = 0;
		pkt.length  = 0;
		(void)send_cmd_internal(core_info, &pkt, false);
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_state_notifier = {
	.notifier_call = scp_state_notify
};

int aov_core_init(struct mtk_aov *aov_dev)
{
	struct aov_core *core_info = &aov_dev->core_info;
	int index;
	int ret;
	struct dma_heap *dma_heap;

	curr_dev = aov_dev;

	init_waitqueue_head(&(core_info->scp_queue));
	atomic_set(&(core_info->scp_session), 0);
	atomic_set(&(core_info->scp_ready), 0);
	atomic_set(&(core_info->aov_session), 0);
	atomic_set(&(core_info->aov_ready), 0);

	if (curr_dev->op_mode) {
		ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_AOV,
			ipi_receive, NULL, &(core_info->packet));
		if (ret < 0) {
			pr_info("%s: failed to register ipi callback: %d\n", __func__, ret);
			return ret;
		}

		/* this call back can get scp power down status */
		scp_A_register_notify(&scp_state_notifier);

		core_info->buf_pa = scp_get_reserve_mem_phys(SCP_AOV_MEM_ID);
		core_info->buf_va = (void *)scp_get_reserve_mem_virt(SCP_AOV_MEM_ID);
		core_info->buf_size = scp_get_reserve_mem_size(SCP_AOV_MEM_ID);

		pr_info("%s: scp buffer pa(0x%x), va(0x%08x), size(0x%x)\n", __func__,
			core_info->buf_pa, core_info->buf_va, core_info->buf_size);

		tlsf_init(&(core_info->alloc), core_info->buf_va, core_info->buf_size);
	} else {
		pr_info("%s: init aov core in bypass mode\n", __func__);
	}

	spin_lock_init(&core_info->buf_lock);

	core_info->aov_init = NULL;

	// core_info->event_data = devm_kzalloc(
	//  aov_dev->dev, sizeof(struct buffer) * 3, GFP_KERNEL);
	// if (core_info->event_data == NULL) {
	//  dev_info(aov_dev->dev, "%s: failed to alloc event buffer\n", __func__);
	//  return -ENOMEM;
	// }

	dma_heap = dma_heap_find("mtk_mm");
	if (!dma_heap) {
		pr_info("%s: failed to find dma heap\n", __func__);
		return -ENOMEM;
	}

	core_info->dma_buf = dma_heap_buffer_alloc(
		dma_heap, sizeof(struct buffer) * 5, O_RDWR | O_CLOEXEC,
		DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(core_info->dma_buf)) {
		core_info->dma_buf = NULL;
		pr_info("%s: failed to alloc dma buffer\n", __func__);
		return -ENOMEM;
	}

	mtk_dma_buf_set_name(core_info->dma_buf, "AOV Event");

	ret = dma_buf_vmap(core_info->dma_buf, &(core_info->dma_map));
	if (ret) {
		pr_info("%s: failed to map dmap buffer(%d)\n", __func__, ret);
		return -ENOMEM;
	}

	core_info->event_data = (void *)core_info->dma_map.vaddr;

	INIT_LIST_HEAD(&core_info->event_list);
	for (index = 0; index < 5; index++)
		list_add_tail(&core_info->event_data[index].entry, &core_info->event_list);

	spin_lock_init(&core_info->event_lock);

	for (index = 0; index < AOV_SCP_CMD_MAX; index++)
		init_waitqueue_head(&core_info->ack_wq[index]);

	init_waitqueue_head(&core_info->poll_wq);

	atomic_set(&(core_info->debug_mode), 0);
	atomic_set(&(core_info->disp_mode), AOV_DiSP_MODE_ON);
	atomic_set(&(core_info->aie_avail), 1);

	return 0;
}

int aov_core_copy(struct mtk_aov *aov_dev, struct aov_dqevent *dequeue)
{
	struct aov_core *core_info = &aov_dev->core_info;
	struct aov_event *event;
	void *buffer;
	uint32_t debug_mode;
	int ret = 0;

	dev_info(aov_dev->dev, "%s: copy aov event+\n", __func__);

	if (atomic_read(&(core_info->aov_ready)) == 0) {
		dev_info(aov_dev->dev, "%s: aov is not started\n", __func__);
		return -EIO;
	}

	event = queue_pop(&(core_info->queue));
	if (event == NULL) {
		dev_info(aov_dev->dev, "%s: event is null\n", __func__);
		return -EIO;
	}

	put_user(event->session,
		(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, session)));
	put_user(event->frame_id,
		(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, frame_id)));
	put_user(event->frame_width,
		(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, frame_width)));
	put_user(event->frame_height,
		(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, frame_height)));
	put_user(event->frame_mode,
		(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, frame_mode)));
	put_user(event->detect_mode,
		(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, detect_mode)));

	debug_mode = atomic_read(&(core_info->debug_mode));
	if ((event->detect_mode) || (debug_mode == AOV_DEBUG_MODE_DUMP) ||
		(debug_mode == AOV_DEBUG_MODE_NDD)) {
		// Setup yuvo1 stride
		put_user(event->yuvo1_stride, (uint32_t *)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, yuvo1_stride)));

		// Copy yuvo1 buffer output
		get_user(buffer, (void **)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, yuvo1_output)));

		dev_info(aov_dev->dev, "%s: copy yuvo1 output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->yuvo1_output[0])),
				buffer, AOV_MAX_YUVO1_OUTPUT);

		ret = copy_to_user((void *)buffer,
			ALIGN16(&(event->yuvo1_output[0])), AOV_MAX_YUVO1_OUTPUT);
		if (ret) {
			buffer_release(core_info, event);
			dev_info(aov_dev->dev,
				"%s: failed to copy yuvo1 output(%d)\n", __func__, ret);
			return -EFAULT;
		}

		// Setup yuvo2 stride
		put_user(event->yuvo2_stride, (uint32_t *)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, yuvo2_stride)));

		// Copy yuvo2 buffer output
		get_user(buffer, (void **)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, yuvo2_output)));

		dev_info(aov_dev->dev, "%s: copy yuvo1 output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->yuvo2_output[0])),
				buffer, AOV_MAX_YUVO2_OUTPUT);

		ret = copy_to_user((void *)buffer,
			ALIGN16(&(event->yuvo2_output[0])), AOV_MAX_YUVO2_OUTPUT);
		if (ret) {
			buffer_release(core_info, event);
			dev_info(aov_dev->dev,
				"%s: failed to copy yuvo2 output(%d)\n", __func__, ret);
			return -EFAULT;
		}

		// Setup aie output size
		put_user(event->aie_size, (uint32_t *)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, aie_size)));

		if (event->aie_size) {
			if (event->aie_size > AOV_MAX_AIE_OUTPUT) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev, "%s: invalid aie output overflow(%d/%d)\n",
					__func__, event->aie_size, AOV_MAX_AIE_OUTPUT);
				return -ENOMEM;
			}

			// Copy aie buffer output
			get_user(buffer, (void **)((uintptr_t)dequeue +
				offsetof(struct aov_dqevent, aie_output)));

			dev_info(aov_dev->dev, "%s: copy aie output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->aie_output[0])),
				buffer, event->aie_size);

			ret = copy_to_user((void *)buffer,
				ALIGN16(&(event->aie_output[0])), event->aie_size);
			if (ret) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev,
					"%s: failed to copy aie output(%d)\n", __func__, ret);
				return -EFAULT;
			}
		}

		// Setup apu output size
		put_user(event->apu_size, (uint32_t *)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, apu_size)));

		if (event->apu_size) {
			if (event->apu_size > AOV_MAX_APU_OUTPUT) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev, "%s: invalid apu output overflow(%d/%d)\n",
					__func__, event->apu_size, AOV_MAX_APU_OUTPUT);
				return -ENOMEM;
			}

			// Copy apu buffer output
			get_user(buffer, (void **)((uintptr_t)dequeue +
				offsetof(struct aov_dqevent, apu_output)));

			dev_info(aov_dev->dev, "%s: copy apu output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->apu_output[0])),
				buffer, event->apu_size);

			ret = copy_to_user((void *)buffer,
				ALIGN16(&(event->apu_output[0])), event->apu_size);
			if (ret) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev,
					"%s: failed to copy apu output(%d)", __func__, ret);
				return -EFAULT;
			}
		}
	}

	if (debug_mode == AOV_DEBUG_MODE_NDD) {
		// Setup imgo stride
		put_user(event->imgo_stride, (uint32_t *)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, imgo_stride)));

		// Copy imgo buffer output
		get_user(buffer, (void **)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, imgo_output)));

		dev_info(aov_dev->dev, "%s: copy imgo output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->imgo_output[0])),
				buffer, AOV_MAX_IMGO_OUTPUT);

		ret = copy_to_user((void *)buffer,
			ALIGN16(&(event->imgo_output[0])), AOV_MAX_IMGO_OUTPUT);
		if (ret) {
			buffer_release(core_info, event);
			dev_info(aov_dev->dev,
				"%s: failed to copy imgo output(%d)\n", __func__, ret);
			return -EFAULT;
		}

		// Setup aao output size
		put_user(event->aao_size,
			(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, aao_size)));

		if (event->aao_size) {
			if (event->aao_size > AOV_MAX_AAO_OUTPUT) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev, "%s: invalid aao output overflow(%d/%d)\n",
					__func__, event->aao_size, AOV_MAX_AAO_OUTPUT);
				return -ENOMEM;
			}

			// Copy apu buffer output
			get_user(buffer, (void **)((uintptr_t)dequeue +
				offsetof(struct aov_dqevent, aao_output)));

			dev_info(aov_dev->dev, "%s: copy aao output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->aao_output[0])),
				buffer, event->aao_size);

			ret = copy_to_user((void *)buffer,
				ALIGN16(&(event->aao_output[0])), event->aao_size);
			if (ret) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev,
					"%s: failed to copy aao output(%d)\n", __func__, ret);
				return -EFAULT;
			}
		}

		// Setup aaho output size
		put_user(event->aaho_size, (uint32_t *)((uintptr_t)dequeue +
			offsetof(struct aov_dqevent, aaho_size)));

		if (event->aaho_size) {
			if (event->aaho_size > AOV_MAX_AAHO_OUTPUT) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev, "%s: invalid aaho output overflow(%d/%d)\n",
					__func__, event->aaho_size, AOV_MAX_AAHO_OUTPUT);
				return -ENOMEM;
			}

			// Copy apu buffer output
			get_user(buffer, (void **)((uintptr_t)dequeue +
				offsetof(struct aov_dqevent, aaho_output)));

			dev_info(aov_dev->dev, "%s: copy aaho output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->aaho_output[0])),
				buffer, event->aaho_size);

			ret = copy_to_user((void *)buffer,
				ALIGN16(&(event->aaho_output[0])), event->aaho_size);
			if (ret) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev,
					"%s: failed to copy aaho output(%d)\n", __func__, ret);
				return -EFAULT;
			}
		}

		// Setup meta output size
		put_user(event->meta_size,
			(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, meta_size)));

		if (event->meta_size) {
			if (event->meta_size > AOV_MAX_META_OUTPUT) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev, "%s: invalid meta output overflow(%d/%d)\n",
					__func__, event->meta_size, AOV_MAX_META_OUTPUT);
				return -ENOMEM;
			}

			// Copy apu buffer output
			get_user(buffer, (void **)((uintptr_t)dequeue +
				offsetof(struct aov_dqevent, meta_output)));

			dev_info(aov_dev->dev, "%s: copy meta output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->meta_output[0])),
				buffer, event->meta_size);

			ret = copy_to_user((void *)buffer,
				ALIGN16(&(event->meta_output[0])), event->meta_size);
			if (ret) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev,
					"%s: failed to copy meta output(%d)\n", __func__, ret);
				return -EFAULT;
			}
		}

		// Setup awb output size
		put_user(event->awb_size,
			(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, awb_size)));

		if (event->awb_size) {
			if (event->awb_size > AOV_MAX_AWB_OUTPUT) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev, "%s: invalid awb output overflow(%d/%d)\n",
					__func__, event->awb_size, AOV_MAX_AWB_OUTPUT);
				return -ENOMEM;
			}

			// Copy apu buffer output
			get_user(buffer, (void **)((uintptr_t)dequeue +
				offsetof(struct aov_dqevent, awb_output)));

			dev_info(aov_dev->dev, "%s: copy tuning output from(%x) to(%x) size(%d)\n",
				__func__, ALIGN16(&(event->awb_output[0])),
				buffer, event->awb_size);

			ret = copy_to_user((void *)buffer,
				ALIGN16(&(event->awb_output[0])), event->awb_size);
			if (ret) {
				buffer_release(core_info, event);
				dev_info(aov_dev->dev,
					"%s: failed to copy awb output(%d)\n", __func__, ret);
				return -EFAULT;
			}
		}
	}

	buffer_release(core_info, event);

	dev_info(aov_dev->dev, "%s: copy aov event-\n", __func__);

	return ret;
}

int aov_core_poll(struct mtk_aov *aov_dev, struct file *file,
	poll_table *wait)
{
	struct aov_core *core_info = &aov_dev->core_info;

	dev_dbg(aov_dev->dev, "%s: poll start+\n", __func__);

	if (!queue_empty(&(core_info->queue))) {
		dev_info(aov_dev->dev, "%s: poll start-: %d\n", __func__, POLLPRI);
		return POLLPRI;
	}

	poll_wait(file, &core_info->poll_wq, wait);

	if (!queue_empty(&(core_info->queue))) {
		dev_info(aov_dev->dev, "%s: poll start-: %d\n", __func__, POLLPRI);
		return POLLPRI;
	}

	dev_dbg(aov_dev->dev, "%s: poll start-: 0\n", __func__);

	return 0;
}

int aov_core_reset(struct mtk_aov *aov_dev)
{
	struct aov_core *core_info = &aov_dev->core_info;
	int ret = 0;

	if (atomic_read(&(core_info->aov_ready))) {
#if AOV_SLB_ALLOC_FREE
		struct slbc_data slb;
#endif  // AOV_SLB_ALLOC_FREE

		dev_info(aov_dev->dev, "%s: force aov deinit+\n", __func__);
		ret = aov_core_send_cmd(aov_dev,
			AOV_SCP_CMD_DEINIT, NULL, 0, false);

#if AOV_SLB_ALLOC_FREE
		slb.uid = UID_AOV_DC;
		slb.type = TP_BUFFER;
		ret = slbc_status(&slb);
		if (ret > 0) {
			dev_info(aov_dev->dev, "%s: force release slb(%d)\n", __func__, ret);

			slb.uid = UID_AOV_DC;
			slb.type = TP_BUFFER;
			ret = slbc_release(&slb);
			if (ret < 0)
				pr_info("%s: failed to release slb buffer\n", __func__);
		}
#endif  // AOV_SLB_ALLOC_FREE

		dev_info(aov_dev->dev, "%s: force aov deinit-: (%d)", __func__, ret);
	}

	return ret;
}

int aov_core_uninit(struct mtk_aov *aov_dev)
{
	struct aov_core *core_info = &aov_dev->core_info;

	//devm_kfree(aov_dev->dev, core_info->event_data);

	if (core_info->dma_buf) {
		if (core_info->event_data)
			dma_buf_vunmap(core_info->dma_buf, &(core_info->dma_map));

		dma_heap_buffer_free(core_info->dma_buf);
	}

	curr_dev = NULL;

	return 0;
}

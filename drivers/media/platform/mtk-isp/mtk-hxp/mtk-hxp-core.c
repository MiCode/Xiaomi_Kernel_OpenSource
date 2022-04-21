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

#include "mtk-hxp-drv.h"
#include "mtk-hxp-core.h"
#include "mtk-hxp-aov.h"

#include "scp.h"

static struct mtk_hxp *curr_dev;

struct mtk_hxp *hxp_core_get_device(void)
{
	return curr_dev;
}

static void *buffer_acquire(struct hxp_core *core_info)
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

static void buffer_release(struct hxp_core *core_info, void *data)
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
	struct mtk_hxp *hxp_dev = hxp_core_get_device();
	struct hxp_core *core_info = &hxp_dev->core_info;
	uint32_t aov_ready;
	uint32_t aov_session;
	struct packet *packet;
	struct aov_event *event;
	void *buffer;

	aov_ready = atomic_read(&(core_info->aov_ready));

	pr_debug("%s receive id(%d), len(%d), ready(%d)\n", __func__, id, len, aov_ready);

	if (aov_ready == 0) {
		pr_info("%s aov stream is not started", __func__);
		return 0;
	}

	packet = (struct packet *)data;
	if (packet->command & HXP_AOV_PACKET_ACK) {
		uint32_t cmd = packet->command & ~HXP_AOV_PACKET_ACK;

		if ((cmd > 0) && (cmd < HXP_AOV_CMD_MAX)) {
			atomic_set(&(core_info->ack_cmd[cmd]), 1);
			wake_up_interruptible(&core_info->ack_wq[cmd]);
		}
	} else {
		event = (struct aov_event *)(core_info->buf_va +
			(packet->buffer - core_info->buf_pa));

		aov_session = atomic_read(&(core_info->aov_session));

		if (event->session != aov_session) {
			pr_info("%s invalid aov session mismatch(%d/%d)\n", __func__,
				event->session, aov_session);
			return 0;
		}

		buffer = buffer_acquire(core_info);
		if (buffer == NULL) {
			pr_info("%s failed to acquire message\n", __func__);
			return 0;
		}

		memcpy(buffer, (void *)event, sizeof(struct aov_event));

		queue_push(&(core_info->queue), buffer);
		wake_up_interruptible(&core_info->poll_wq);
	}

	return 0;
}

static int send_cmd_internal(struct hxp_core *core_info,
	struct packet *pkt, bool wait)
{
	int retry;
	int ret;
	unsigned long timeout;
	int scp_ready;

	pr_info("%s send cmd(%d), session(%d)\n",
		__func__, pkt->command, pkt->session);

	retry = 0;
	do {
		if (wait) {
			timeout = msecs_to_jiffies(HXP_TIMEOUT_MS);
			ret = wait_event_interruptible_timeout(core_info->scp_queue,
				((scp_ready = atomic_read(&(core_info->scp_ready))) != 0), timeout);
			if (ret == 0) {
				pr_info("%s send cmd(%d) time out !\n",
					__func__, pkt->command);
				return -EIO;
			} else if (-ERESTARTSYS == ret) {
				pr_info("%s send cmd(%d) interrupted !\n",
					__func__, pkt->command);
				return -ERESTARTSYS;
			}
		} else {
			scp_ready = atomic_read(&(core_info->scp_ready));
		}

		if (scp_ready != 2) {
			pr_info("%s scp not ready(%d)\n", __func__, scp_ready);
			return -EIO;
		}

		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_AOV_SCP,
			IPI_SEND_WAIT, pkt, 4, HXP_TIMEOUT_MS);
		if (ret < 0 && ret != IPI_PIN_BUSY) {
			pr_info("%s failed to send packet: %d", __func__, ret);
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

int hxp_core_send_cmd(struct mtk_hxp *hxp_dev, uint32_t cmd,
	void *data, int len, bool ack)
{
	struct hxp_core *core_info = &hxp_dev->core_info;
	unsigned long flag;
	uint8_t *buf;
	int ret;
	struct packet pkt;
	unsigned long timeout;

	pr_info("%s+", __func__);

	if (data && len) {
		if (cmd == HXP_AOV_CMD_INIT) {
			spin_lock_irqsave(&core_info->buf_lock, flag);
			buf = tlsf_malloc(&(core_info->alloc), sizeof(struct aov_init));
			spin_unlock_irqrestore(&core_info->buf_lock, flag);
		} else {
			spin_lock_irqsave(&core_info->buf_lock, flag);
			buf = tlsf_malloc(&(core_info->alloc), len);
			spin_unlock_irqrestore(&core_info->buf_lock, flag);
		}
		if (buf) {
			if (cmd == HXP_AOV_CMD_INIT) {
				struct aov_user user;
				struct aov_init *init = (struct aov_init *)buf;

				pr_debug("%s init buffer %x, size %d\n",
					__func__, buf, sizeof(struct aov_init));

				pr_debug("%s copy aov user data %x, %d+\n",
					__func__, data, sizeof(struct aov_user));
				(void)copy_from_user((void *)&user,
					(void *)data, sizeof(struct aov_user));
				pr_debug("%s copy aov user data %x, %d-\n",
					__func__, data, sizeof(struct aov_user));

				memcpy(init, &user, HXP_MAX_USER_SIZE);

				if ((user.aaa_size > 0) &&
					(user.aaa_size <= HXP_MAX_AAA_SIZE)) {
					pr_debug("%s copy aaa info %x, %d+\n",
						__func__, user.aaa_info, user.aaa_size);
					(void)copy_from_user(&(init->aaa_info),
						user.aaa_info, user.aaa_size);
					pr_debug("%s copy aaa info %x, %d-\n",
						__func__, user.aaa_info, user.aaa_size);
				} else {
					pr_info("%s aaa info size(%d) overflow\n",
						__func__, user.aaa_size);
					//return -ENOMEM;
				}

				if ((user.tuning_size > 0) &&
					(user.tuning_size <= HXP_MAX_TUNING_SIZE)) {
					pr_debug("%s copy tuning info %x, %d+\n",
						__func__, user.tuning_info, user.tuning_size);
					(void)copy_from_user(&(init->tuning_info),
						user.tuning_info, user.tuning_size);
					pr_debug("%s copy tuning info %x, %d-\n",
						__func__, user.tuning_info, user.tuning_size);
				} else {
					pr_info("%s tuning info size(%d) overflow\n",
						__func__, user.tuning_size);
					//return -ENOMEM;
				}

				// pr_info("mtk_cam_seninf_s_aov_param+\n");
				// mtk_cam_seninf_s_aov_param(user.sensor_id,
				//  (void *)&(init->senif_info));
				//pr_info("mtk_cam_seninf_s_aov_param-\n");

				//pr_info("mtk_aie_aov_memcpy+\n");
				//mtk_aie_aov_memcpy((void *)&(init->aie_info));
				//pr_info("mtk_aie_aov_memcpy-\n");
				/* debug use
				 * pr_info("out_pad %d\n", init->aov_seninf_param.vc.out_pad);
				 * pr_info("init->aov_seninf_param.sensor_idx%d\n",
				 *    init->aov_seninf_param.sensor_idx);
				 * pr_info("width %d\n", init->aov_seninf_param.width);
				 * pr_info("height %d\n", init->aov_seninf_param.height);
				 * pr_info("isp_freq %d\n", init->aov_seninf_param.isp_freq);
				 * pr_info("camtg %d\n", init->aov_seninf_param.camtg);
				 */
				init->session   = user.session;

				atomic_set(&(core_info->aov_session), user.session);

				len = sizeof(struct aov_init);
			} else {
				pr_info("%s data buffer 0x%08x, size %d", __func__, buf, len);
				(void)copy_from_user(buf, (void *)data, len);
			}
		} else {
			pr_info("%s: failed to alloc buffer size(%d)", __func__, len);
			return -ENOMEM;
		}
	} else {
		buf = NULL;
	}

	pkt.session = atomic_read(&(core_info->scp_session));
	pkt.command = cmd;
	if (buf) {
		pkt.buffer = core_info->buf_pa + (buf - core_info->buf_va);
		pkt.length = len;
	} else {
		pkt.buffer = 0;
		pkt.length = 0;
	}

	if (cmd == HXP_AOV_CMD_INIT) {
		struct aov_init *init = (struct aov_init *)buf;

		// Init queue to receive event
		queue_init(&(core_info->queue));

		// Setup display mode
		init->disp_mode = atomic_read(&(core_info->disp_mode));

		// Record aov_init buffer
		core_info->aov_init = init;

		atomic_set(&(core_info->aov_ready), 1);
	} else if (cmd == HXP_AOV_CMD_PWR_OFF) {
		atomic_set(&(core_info->disp_mode), HXP_AOV_MODE_DISP_OFF);
	} else if (cmd == HXP_AOV_CMD_PWR_ON) {
		atomic_set(&(core_info->disp_mode), HXP_AOV_MODE_DISP_ON);
	}

	if (ack)
		atomic_set(&(core_info->ack_cmd[cmd]), 0);

	ret = send_cmd_internal(core_info, &pkt, true);
	if (ret < 0) {
		dev_info(hxp_dev->dev, "%s: failed to send cmd(%d)", __func__, cmd);
		return ret;
	}

	if (ack) {
		timeout = msecs_to_jiffies(HXP_TIMEOUT_MS);
		ret = wait_event_interruptible_timeout(core_info->ack_wq[cmd],
			atomic_cmpxchg(&(core_info->ack_cmd[cmd]), 1, 0), timeout);
		if (ret == 0) {
			dev_info(hxp_dev->dev, "%s wait ack cmd(%d) timeout\n",
				__func__, cmd);
			/*
			 * clear un-success event to prevent unexpected flow
			 * cauesd be remaining data
			 */
			return -EIO;
		} else if (-ERESTARTSYS == ret) {
			dev_info(hxp_dev->dev, "%s wait cmd(%d) ack interrupted\n",
				__func__, cmd);
			return -ERESTARTSYS;
		}
	}

	if (cmd == HXP_AOV_CMD_DEINIT) {
		atomic_set(&(core_info->aov_ready), 0);

		// Free aov_init buffer
		spin_lock_irqsave(&core_info->buf_lock, flag);
		tlsf_free(&(core_info->alloc), core_info->aov_init);
		spin_unlock_irqrestore(&core_info->buf_lock, flag);

		// Reset queue to empty
		while (!queue_empty(&(core_info->queue))) {
			buf = queue_pop(&(core_info->queue));
			if (buf)
				buffer_release(core_info, buf);
		}

		queue_deinit(&(core_info->queue));
	}

	dev_info(hxp_dev->dev, "%s mtk_ipi_send: %d-", __func__, len);

	pr_info("%s-", __func__);

	return 0;
}

static int scp_state_notify(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct mtk_hxp *hxp_dev = hxp_core_get_device();
	struct hxp_core *core_info = &hxp_dev->core_info;
	struct packet pkt;
	uint32_t session;

	if (event == SCP_EVENT_STOP) {
		pr_info("%s scp stop event(%d)\n", __func__, event);
		atomic_set(&(core_info->scp_ready), 1);
	} else if (event == SCP_EVENT_READY) {
		atomic_set(&(core_info->scp_ready), 2);

		session = atomic_fetch_add(1, &(core_info->scp_session)) + 1;

		pr_info("%s scp start event(%d), session(%d)\n",
			__func__, event, session);

		pkt.session = session;
		pkt.command = HXP_AOV_CMD_READY;
		pkt.buffer  = 0;
		pkt.length  = 0;
		send_cmd_internal(core_info, &pkt, false);
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_state_notifier = {
	.notifier_call = scp_state_notify
};

int hxp_core_init(struct mtk_hxp *hxp_dev)
{
	struct hxp_core *core_info = &hxp_dev->core_info;
	int index;
	int ret;
	struct dma_heap *dma_heap;

	curr_dev = hxp_dev;

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_AOV,
		ipi_receive, NULL, &(core_info->packet));
	if (ret < 0) {
		pr_info("%s: failed to register ipi: %d", __func__, ret);
		return ret;
	}

	init_waitqueue_head(&(core_info->scp_queue));
	atomic_set(&(core_info->scp_session), 0);
	atomic_set(&(core_info->scp_ready), 0);
	atomic_set(&(core_info->aov_session), 0);
	atomic_set(&(core_info->aov_ready), 0);

	/* this call back can get scp power down status */
	scp_A_register_notify(&scp_state_notifier);

	core_info->buf_pa = scp_get_reserve_mem_phys(SCP_AOV_MEM_ID);
	core_info->buf_va = (void *)scp_get_reserve_mem_virt(SCP_AOV_MEM_ID);
	core_info->buf_size = scp_get_reserve_mem_size(SCP_AOV_MEM_ID);

	pr_info("%s scp buffer pa(0x%x), va(0x%08x), size(0x%x)\n", __func__,
		core_info->buf_pa, core_info->buf_va, core_info->buf_size);

	tlsf_init(&(core_info->alloc), core_info->buf_va, core_info->buf_size);
	spin_lock_init(&core_info->buf_lock);

	core_info->aov_init = NULL;

	// core_info->event_data = devm_kzalloc(
	//  hxp_dev->dev, sizeof(struct buffer) * 3, GFP_KERNEL);
	// if (core_info->event_data == NULL) {
	//  dev_info(hxp_dev->dev, "%s: failed to alloc event buffer\n", __func__);
	//  return -ENOMEM;
	// }

	dma_heap = dma_heap_find("mtk_mm");
	if (!dma_heap) {
		pr_info("%s: failed to find dma heap\n");
		return -ENOMEM;
	}

	core_info->dma_buf = dma_heap_buffer_alloc(
		dma_heap, sizeof(struct buffer) * 5, O_RDWR | O_CLOEXEC,
		DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(core_info->dma_buf)) {
		core_info->dma_buf = NULL;
		pr_info("%s: failed to alloc dma buffer\n");
		return -ENOMEM;
	}

	mtk_dma_buf_set_name(core_info->dma_buf, "AOV Event");

	ret = dma_buf_vmap(core_info->dma_buf, &(core_info->dma_map));
	if (ret) {
		pr_info("%s: failed to map dmap buffer(%d)\n", ret);
		return -ENOMEM;
	}

	core_info->event_data = (void *)core_info->dma_map.vaddr;

	INIT_LIST_HEAD(&core_info->event_list);
	for (index = 0; index < 5; index++)
		list_add_tail(&core_info->event_data[index].entry, &core_info->event_list);

	spin_lock_init(&core_info->event_lock);

	for (index = 0; index < HXP_AOV_CMD_MAX; index++)
		init_waitqueue_head(&core_info->ack_wq[index]);

	init_waitqueue_head(&core_info->poll_wq);

	atomic_set(&(core_info->disp_mode), HXP_AOV_MODE_DISP_ON);

	return 0;
}

int hxp_core_copy(struct mtk_hxp *hxp_dev, struct aov_dqevent *dequeue)
{
	struct hxp_core *core_info = &hxp_dev->core_info;
	struct aov_event *event;
	void *buffer;
	int ret = 0;

	dev_info(hxp_dev->dev, "%s copy aove event+", __func__);

	event = queue_pop(&(core_info->queue));

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
	put_user(event->aie_size,
		(uint32_t *)((uintptr_t)dequeue + offsetof(struct aov_dqevent, aie_size)));

	if (event->aie_size) {
		get_user(buffer,
			(void **)((uintptr_t)dequeue + offsetof(struct aov_dqevent, aie_output)));

		dev_info(hxp_dev->dev, "%s copy aie output from(%x) to(%x) size(%d)+",
			__func__, &(event->aie_output[0]), buffer, event->aie_size);

		ret = copy_to_user((void *)buffer, &(event->aie_output[0]), event->aie_size);
	}

	buffer_release(core_info, event);

	dev_info(hxp_dev->dev, "%s copy aove event-", __func__);

	return ret;
}

int hxp_core_poll(struct mtk_hxp *hxp_dev, struct file *file,
	poll_table *wait)
{
	struct hxp_core *core_info = &hxp_dev->core_info;

	dev_info(hxp_dev->dev, "%s: poll start+", __func__);

	if (!queue_empty(&(core_info->queue))) {
		dev_info(hxp_dev->dev, "%s: poll start-: %d", __func__, POLLPRI);
		return POLLPRI;
	}

	poll_wait(file, &core_info->poll_wq, wait);

	if (!queue_empty(&(core_info->queue))) {
		dev_info(hxp_dev->dev, "%s: poll start-: %d", __func__, POLLPRI);
		return POLLPRI;
	}

	dev_info(hxp_dev->dev, "%s: poll start-: 0", __func__);
	return 0;
}

int hxp_core_uninit(struct mtk_hxp *hxp_dev)
{
	struct hxp_core *core_info = &hxp_dev->core_info;

	//devm_kfree(hxp_dev->dev, core_info->event_data);

	if (core_info->dma_buf) {
		if (core_info->event_data)
			dma_buf_vunmap(core_info->dma_buf, &(core_info->dma_map));

		dma_heap_buffer_free(core_info->dma_buf);
	}

	curr_dev = NULL;

	return 0;
}

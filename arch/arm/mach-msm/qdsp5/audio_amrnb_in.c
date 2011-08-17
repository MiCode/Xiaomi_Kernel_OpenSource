/* arch/arm/mach-msm/qdsp5/audio_amrnb_in.c
 *
 * amrnb encoder device
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This code is based in part on arch/arm/mach-msm/qdsp5/audio_in.c, which is
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <mach/msm_rpcrouter.h>
#include <linux/msm_audio_amrnb.h>

#include "audmgr.h"

#include <mach/qdsp5/qdsp5audpreproccmdi.h>
#include <mach/qdsp5/qdsp5audpreprocmsg.h>
#include <mach/qdsp5/qdsp5audreccmdi.h>
#include <mach/qdsp5/qdsp5audrecmsg.h>
#include <mach/debug_mm.h>

/* FRAME_NUM must be a power of two */
#define FRAME_NUM		(8)
#define FRAME_SIZE		(22 * 2)
#define DMASZ 			(FRAME_SIZE * FRAME_NUM)

struct buffer {
	void *data;
	uint32_t size;
	uint32_t read;
	uint32_t addr;
};

struct audio_amrnb_in {
	struct buffer in[FRAME_NUM];

	spinlock_t dsp_lock;

	atomic_t in_bytes;

	struct mutex lock;
	struct mutex read_lock;
	wait_queue_head_t wait;

	uint16_t audrec_obj_idx;

	/* configuration to use on enable */
	uint32_t buffer_size;
	uint32_t enc_type; /* 0 for WAV ,1 for AAC,10 for AMRNB */
	struct msm_audio_amrnb_enc_config amrnb_enc_cfg;

	uint32_t dsp_cnt;
	uint32_t in_head; /* next buffer dsp will write */
	uint32_t in_tail; /* next buffer read() will read */
	uint32_t in_count; /* number of buffers available to read() */

	struct audmgr audmgr;

	/* data allocated for various buffers */
	char *data;
	dma_addr_t phys;

	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped, cleared on flush */

};

static int audio_amrnb_in_dsp_enable(struct audio_amrnb_in *audio, int enable);
static int audio_amrnb_in_encmem_config(struct audio_amrnb_in *audio);
static int audio_amrnb_in_encparam_config(struct audio_amrnb_in *audio);
static int audio_amrnb_in_dsp_read_buffer(struct audio_amrnb_in *audio,
					uint32_t read_cnt);
static void audio_amrnb_in_flush(struct audio_amrnb_in *audio);
static void audio_amrnb_in_dsp_event(void *data, unsigned id, uint16_t *msg);

static struct audio_amrnb_in the_audio_amrnb_in;

/* must be called with audio->lock held */
static int audio_amrnb_in_enable(struct audio_amrnb_in *audio)
{
	struct audmgr_config cfg;
	int rc;

	if (audio->enabled)
		return 0;

	cfg.tx_rate = RPC_AUD_DEF_SAMPLE_RATE_8000;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_NONE;
	cfg.def_method = RPC_AUD_DEF_METHOD_RECORD;
	cfg.codec = RPC_AUD_DEF_CODEC_AMR_NB;
	cfg.snd_method = RPC_SND_METHOD_MIDI;

	rc = audmgr_enable(&audio->audmgr, &cfg);
	if (rc < 0)
		return rc;

	if (audrectask_enable(audio->enc_type, audio_amrnb_in_dsp_event,
		audio)) {
		audmgr_disable(&audio->audmgr);
		MM_ERR("audrec_enable failed\n");
		return -ENODEV;
	}

	audio->enabled = 1;
	audio_amrnb_in_dsp_enable(audio, 1);

	return 0;
}

/* must be called with audio->lock held */
static int audio_amrnb_in_disable(struct audio_amrnb_in *audio)
{
	if (audio->enabled) {
		audio->enabled = 0;
		audio_amrnb_in_dsp_enable(audio, 0);
		wake_up(&audio->wait);
		audrectask_disable(audio->enc_type, audio);
		audmgr_disable(&audio->audmgr);
	}
	return 0;
}

/* ------------------- dsp --------------------- */
struct audio_amrnb_in_frame {
	uint16_t frame_count_lsw;
	uint16_t frame_count_msw;
	uint16_t frame_length;
	uint16_t erased_pcm;
	unsigned char raw_bitstream[];
} __attribute__((packed));

static void audio_amrnb_in_get_dsp_frames(struct audio_amrnb_in *audio)
{
	struct audio_amrnb_in_frame *frame;
	uint32_t index;
	unsigned long flags;
	index = audio->in_head;

	frame = (void *) (((char *)audio->in[index].data) -
		sizeof(*frame));
	spin_lock_irqsave(&audio->dsp_lock, flags);
	audio->in[index].size = FRAME_SIZE - (sizeof(*frame)); /* Send
			Complete Transcoded Data, not actual frame part  */

	audio->in_head = (audio->in_head + 1) & (FRAME_NUM - 1);

	/* If overflow, move the tail index foward. */
	if (audio->in_head == audio->in_tail)
		audio->in_tail = (audio->in_tail + 1) & (FRAME_NUM - 1);
	else
		audio->in_count++;

	audio_amrnb_in_dsp_read_buffer(audio, audio->dsp_cnt++);
	spin_unlock_irqrestore(&audio->dsp_lock, flags);

	wake_up(&audio->wait);
}

static void audio_amrnb_in_dsp_event(void *data, unsigned id, uint16_t *msg)
{
	struct audio_amrnb_in *audio = data;

	switch (id) {
	case AUDREC_MSG_CMD_CFG_DONE_MSG:
		MM_DBG("CFG_DONE_MSG\n");
		if (msg[0] & AUDREC_MSG_CFG_DONE_ENC_ENA) {
			audio->audrec_obj_idx = msg[1];
			audio_amrnb_in_encmem_config(audio);
		} else {
			audio->running = 0;
		}
		break;
	case AUDREC_MSG_CMD_AREC_MEM_CFG_DONE_MSG: {
		MM_DBG("AREC_MEM_CFG_DONE_MSG\n");
		if (msg[0] == audio->audrec_obj_idx)
			audio_amrnb_in_encparam_config(audio);
		else
			MM_ERR("AREC_MEM_CFG_DONE_MSG ERR\n");
		break;
	}
	case AUDREC_MSG_CMD_AREC_PARAM_CFG_DONE_MSG: {
		MM_DBG("AREC_PARAM_CFG_DONE_MSG\n");
		if (msg[0] == audio->audrec_obj_idx)
			audio->running = 1;
		else
			MM_ERR("AREC_PARAM_CFG_DONE_MSG ERR\n");
		break;
	}
	case AUDREC_MSG_PACKET_READY_MSG: {
		MM_DBG("AUDREC_MSG_PACKET_READY_MSG\n");
		if (msg[0] == audio->audrec_obj_idx)
			audio_amrnb_in_get_dsp_frames(audio);
		else
			MM_ERR("PACKET_READY_MSG ERR\n");
		break;
	}
	case AUDREC_MSG_FATAL_ERR_MSG: {
		MM_ERR("FATAL_ERR_MSG %x\n", msg[0]);
		break;
	}
	default:
		MM_ERR("unknown event %d\n", id);
	}
}

static int audio_amrnb_in_dsp_enable(struct audio_amrnb_in *audio, int enable)
{
	struct audrec_cmd_enc_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_ENC_CFG;
	cmd.audrec_enc_type = (audio->enc_type) |
	(enable ? AUDREC_CMD_ENC_ENA : AUDREC_CMD_ENC_DIS);
	/* Don't care on enable, required on disable */
	cmd.audrec_obj_idx = audio->audrec_obj_idx;

	return audrectask_send_cmdqueue(&cmd, sizeof(cmd));
}

static int audio_amrnb_in_encmem_config(struct audio_amrnb_in *audio)
{
	struct audrec_cmd_arecmem_cfg cmd;
	uint16_t *data = (void *) audio->data;
	unsigned cnt;

	memset(&cmd, 0, sizeof(cmd));

	cmd.cmd_id = AUDREC_CMD_ARECMEM_CFG;
	cmd.audrec_obj_idx = audio->audrec_obj_idx;
	/* Rate at which packet complete message comes */
	cmd.audrec_up_pkt_intm_cnt = 1;
	cmd.audrec_extpkt_buffer_msw = audio->phys >> 16;
	cmd.audrec_extpkt_buffer_lsw = audio->phys;
	/* Max Buffer no available for frames */
	cmd.audrec_extpkt_buffer_num = FRAME_NUM;

	/* prepare buffer pointers:
	 * 4 halfword header + Frame Raw Packet (20ms data)
	 */
	for (cnt = 0; cnt < FRAME_NUM; cnt++) {
		audio->in[cnt].data = data + 4; /* Pointer to Raw Packet part*/
		MM_DBG(" audio->in[%d].data = %x \n", cnt,
				(unsigned int)audio->in[cnt].data);
		data += 22; /* Point to next Frame buffer */
	}

	return audrectask_send_cmdqueue(&cmd, sizeof(cmd));
}

static int audio_amrnb_in_encparam_config(struct audio_amrnb_in *audio)
{
	struct audrec_cmd_arecparam_amrnb_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.common.cmd_id = AUDREC_CMD_ARECPARAM_CFG;
	cmd.common.audrec_obj_idx = audio->audrec_obj_idx;
	cmd.samp_rate_idx = 0xb; /* 8k Sampling rate */
	cmd.voicememoencweight1 = audio->amrnb_enc_cfg.voicememoencweight1;
	cmd.voicememoencweight2 = audio->amrnb_enc_cfg.voicememoencweight2;
	cmd.voicememoencweight3 = audio->amrnb_enc_cfg.voicememoencweight3;
	cmd.voicememoencweight4 = audio->amrnb_enc_cfg.voicememoencweight4;
	cmd.update_mode = 0x8000 | 0x0000;
	cmd.dtx_mode = audio->amrnb_enc_cfg.dtx_mode_enable;
	cmd.test_mode = audio->amrnb_enc_cfg.test_mode_enable;
	cmd.used_mode = audio->amrnb_enc_cfg.enc_mode;

	MM_DBG("cmd.common.cmd_id = 0x%4x\n", cmd.common.cmd_id);
	MM_DBG("cmd.common.audrec_obj_idx = 0x%4x\n",
			cmd.common.audrec_obj_idx);
	MM_DBG("cmd.samp_rate_idx = 0x%4x\n", cmd.samp_rate_idx);
	MM_DBG("cmd.voicememoencweight1 = 0x%4x\n",
			cmd.voicememoencweight1);
	MM_DBG("cmd.voicememoencweight2 = 0x%4x\n",
			cmd.voicememoencweight2);
	MM_DBG("cmd.voicememoencweight3 = 0x%4x\n",
			cmd.voicememoencweight3);
	MM_DBG("cmd.voicememoencweight4 = 0x%4x\n",
			cmd.voicememoencweight4);
	MM_DBG("cmd.update_mode = 0x%4x\n", cmd.update_mode);
	MM_DBG("cmd.dtx_mode = 0x%4x\n", cmd.dtx_mode);
	MM_DBG("cmd.test_mode = 0x%4x\n", cmd.test_mode);
	MM_DBG("cmd.used_mode = 0x%4x\n", cmd.used_mode);

	return audrectask_send_cmdqueue(&cmd, sizeof(cmd));
}

static int audio_amrnb_in_dsp_read_buffer(struct audio_amrnb_in *audio,
		uint32_t read_cnt)
{
	audrec_cmd_packet_ext_ptr cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_PACKET_EXT_PTR;
	cmd.type = audio->audrec_obj_idx;
	cmd.curr_rec_count_msw = read_cnt >> 16;
	cmd.curr_rec_count_lsw = read_cnt;

	return audrectask_send_bitstreamqueue(&cmd, sizeof(cmd));
}

static void audio_amrnb_in_flush(struct audio_amrnb_in *audio)
{
	int i;

	audio->dsp_cnt = 0;
	audio->in_head = 0;
	audio->in_tail = 0;
	audio->in_count = 0;
	for (i = 0; i < FRAME_NUM; i++) {
		audio->in[i].size = 0;
		audio->in[i].read = 0;
	}
}


/* ------------------- device --------------------- */
static long audio_amrnb_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct audio_amrnb_in *audio = file->private_data;
	int rc;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->in_bytes);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		rc = audio_amrnb_in_enable(audio);
		break;
	case AUDIO_STOP:
		rc = audio_amrnb_in_disable(audio);
		audio->stopped = 1;
		break;
	case AUDIO_FLUSH:
		if (audio->stopped) {
			/* Make sure we're stopped and we wake any threads
			 * that might be blocked holding the read_lock.
			 * While audio->stopped read threads will always
			 * exit immediately.
			 */
			wake_up(&audio->wait);
			mutex_lock(&audio->read_lock);
			audio_amrnb_in_flush(audio);
			mutex_unlock(&audio->read_lock);
		}
	case AUDIO_SET_CONFIG: {
		rc = -EINVAL; /* Buffer size better to come from upper */
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config cfg;
		cfg.buffer_size = audio->buffer_size;
		cfg.buffer_count = FRAME_NUM;
		cfg.sample_rate = 8000;
		cfg.channel_count = 1;
		cfg.type = 10;
		cfg.unused[0] = 0;
		cfg.unused[1] = 0;
		cfg.unused[2] = 0;
		if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_GET_AMRNB_ENC_CONFIG: {
		if (copy_to_user((void *)arg, &audio->amrnb_enc_cfg,
			sizeof(audio->amrnb_enc_cfg)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_SET_AMRNB_ENC_CONFIG: {
		struct msm_audio_amrnb_enc_config cfg;
		if (copy_from_user
			(&cfg, (void *)arg, sizeof(cfg))) {
			rc = -EFAULT;
		} else
			rc = 0;
		audio->amrnb_enc_cfg.voicememoencweight1 =
					cfg.voicememoencweight1;
		audio->amrnb_enc_cfg.voicememoencweight2 =
					cfg.voicememoencweight2;
		audio->amrnb_enc_cfg.voicememoencweight3 =
					cfg.voicememoencweight3;
		audio->amrnb_enc_cfg.voicememoencweight4 =
					cfg.voicememoencweight4;
		audio->amrnb_enc_cfg.dtx_mode_enable = cfg.dtx_mode_enable;
		audio->amrnb_enc_cfg.test_mode_enable = cfg.test_mode_enable;
		audio->amrnb_enc_cfg.enc_mode = cfg.enc_mode;
		/* Run time change of Param */
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static ssize_t audio_amrnb_in_read(struct file *file,
				char __user *buf,
				size_t count, loff_t *pos)
{
	struct audio_amrnb_in *audio = file->private_data;
	unsigned long flags;
	const char __user *start = buf;
	void *data;
	uint32_t index;
	uint32_t size;
	int rc = 0;

	mutex_lock(&audio->read_lock);
	while (count > 0) {
		rc = wait_event_interruptible(
			audio->wait, (audio->in_count > 0) || audio->stopped);
		if (rc < 0)
			break;

		if (audio->stopped) {
			rc = -EBUSY;
			break;
		}

		index = audio->in_tail;
		data = (uint8_t *) audio->in[index].data;
		size = audio->in[index].size;
		if (count >= size) {
			dma_coherent_post_ops();
			if (copy_to_user(buf, data, size)) {
				rc = -EFAULT;
				break;
			}
			spin_lock_irqsave(&audio->dsp_lock, flags);
			if (index != audio->in_tail) {
				/* overrun -- data is invalid
					and we need to retry */
				spin_unlock_irqrestore(&audio->dsp_lock,
					flags);
				continue;
			}
			audio->in[index].size = 0;
			audio->in_tail = (audio->in_tail + 1) & (FRAME_NUM - 1);
			audio->in_count--;
			spin_unlock_irqrestore(&audio->dsp_lock, flags);
			count -= size;
			buf += size;
		} else {
			MM_ERR("short read\n");
			break;
		}
	}
	mutex_unlock(&audio->read_lock);

	if (buf > start)
		return buf - start;

	return rc;
}

static ssize_t audio_amrnb_in_write(struct file *file,
				const char __user *buf,
				size_t count, loff_t *pos)
{
	return -EINVAL;
}

static int audio_amrnb_in_release(struct inode *inode, struct file *file)
{
	struct audio_amrnb_in *audio = file->private_data;

	mutex_lock(&audio->lock);
	audio_amrnb_in_disable(audio);
	audio_amrnb_in_flush(audio);
	audio->opened = 0;
	if (audio->data) {
		dma_free_coherent(NULL, DMASZ, audio->data, audio->phys);
		audio->data = NULL;
	}
	mutex_unlock(&audio->lock);
	return 0;
}


static int audio_amrnb_in_open(struct inode *inode, struct file *file)
{
	struct audio_amrnb_in *audio = &the_audio_amrnb_in;
	int rc;

	mutex_lock(&audio->lock);
	if (audio->opened) {
		rc = -EBUSY;
		goto done;
	}

	if (!audio->data) {
		audio->data = dma_alloc_coherent(NULL, DMASZ,
				&audio->phys, GFP_KERNEL);
		if (!audio->data) {
			rc = -ENOMEM;
			goto done;
		}
	}

	rc = audmgr_open(&audio->audmgr);
	if (rc)
		goto err;
	audio->buffer_size = FRAME_SIZE - 8;
	audio->enc_type = 10;
	audio->amrnb_enc_cfg.voicememoencweight1 = 0x0000;
	audio->amrnb_enc_cfg.voicememoencweight2 = 0x0000;
	audio->amrnb_enc_cfg.voicememoencweight3 = 0x4000;
	audio->amrnb_enc_cfg.voicememoencweight4 = 0x0000;
	audio->amrnb_enc_cfg.dtx_mode_enable = 0;
	audio->amrnb_enc_cfg.test_mode_enable = 0;
	audio->amrnb_enc_cfg.enc_mode = 7;
	audio->dsp_cnt = 0;
	audio->stopped = 0;

	audio_amrnb_in_flush(audio);

	file->private_data = audio;
	audio->opened = 1;
	rc = 0;
	goto done;

err:
	dma_free_coherent(NULL, DMASZ, audio->data, audio->phys);
done:
	mutex_unlock(&audio->lock);
	return rc;
}

static const struct file_operations audio_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_amrnb_in_open,
	.release	= audio_amrnb_in_release,
	.read		= audio_amrnb_in_read,
	.write		= audio_amrnb_in_write,
	.unlocked_ioctl	= audio_amrnb_in_ioctl,
};

struct miscdevice audio_amrnb_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_amrnb_in",
	.fops	= &audio_fops,
};

#ifdef CONFIG_DEBUG_FS
static ssize_t audamrnb_in_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t audamrnb_in_debug_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	const int debug_bufmax = 1024;
	static char buffer[1024];
	int n = 0, i;
	struct audio_amrnb_in *audio = file->private_data;

	mutex_lock(&audio->lock);
	n = scnprintf(buffer, debug_bufmax, "opened %d\n", audio->opened);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"enabled %d\n", audio->enabled);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"stopped %d\n", audio->stopped);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"audrec_obj_idx %d\n", audio->audrec_obj_idx);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"dsp_cnt %d \n", audio->dsp_cnt);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"in_count %d \n", audio->in_count);
	for (i = 0; i < FRAME_NUM; i++)
		n += scnprintf(buffer + n, debug_bufmax - n,
			"audio->in[%d].size %d \n", i, audio->in[i].size);
	mutex_unlock(&audio->lock);
	/* Following variables are only useful for debugging when
	 * when record halts unexpectedly. Thus, no mutual exclusion
	 * enforced
	 */
	n += scnprintf(buffer + n, debug_bufmax - n,
			"running %d \n", audio->running);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"buffer_size %d \n", audio->buffer_size);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"in_head %d \n", audio->in_head);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"in_tail %d \n", audio->in_tail);
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations audamrnb_in_debug_fops = {
	.read = audamrnb_in_debug_read,
	.open = audamrnb_in_debug_open,
};
#endif

static int __init audio_amrnb_in_init(void)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif

	mutex_init(&the_audio_amrnb_in.lock);
	mutex_init(&the_audio_amrnb_in.read_lock);
	spin_lock_init(&the_audio_amrnb_in.dsp_lock);
	init_waitqueue_head(&the_audio_amrnb_in.wait);

#ifdef CONFIG_DEBUG_FS
	dentry = debugfs_create_file("msm_amrnb_in", S_IFREG | S_IRUGO, NULL,
		(void *) &the_audio_amrnb_in, &audamrnb_in_debug_fops);

	if (IS_ERR(dentry))
		MM_ERR("debugfs_create_file failed\n");
#endif
	return misc_register(&audio_amrnb_in_misc);
}

static void __exit audio_amrnb_in_exit(void)
{
	misc_deregister(&audio_amrnb_in_misc);
}

module_init(audio_amrnb_in_init);
module_exit(audio_amrnb_in_exit);

MODULE_DESCRIPTION("MSM AMRNB Encoder driver");
MODULE_LICENSE("GPL v2");

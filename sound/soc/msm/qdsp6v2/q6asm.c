/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/msm_audio.h>

#include <linux/memory_alloc.h>
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/atomic.h>
#include <linux/msm_audio_ion.h>
#include <linux/mm.h>

#include <asm/ioctls.h>

#include <mach/memory.h>
#include <mach/debug_mm.h>

#include <sound/apr_audio-v2.h>
#include <sound/q6asm-v2.h>
#include <sound/q6audio-v2.h>

#include "audio_acdb.h"


#define TRUE        0x01
#define FALSE       0x00

/* TODO, combine them together */
static DEFINE_MUTEX(session_lock);
struct asm_mmap {
	atomic_t ref_cnt;
	void *apr;
};

static struct asm_mmap this_mmap;
/* session id: 0 reserved */
static struct audio_client *session[SESSION_MAX+1];

struct asm_buffer_node {
	struct list_head list;
	uint32_t  buf_addr_lsw;
	uint32_t  mmap_hdl;
};
static int32_t q6asm_srvc_callback(struct apr_client_data *data, void *priv);
static int32_t q6asm_callback(struct apr_client_data *data, void *priv);
static void q6asm_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg);
static void q6asm_add_hdr_custom_topology(struct audio_client *ac,
					  struct apr_hdr *hdr,
					  uint32_t pkt_size, uint32_t cmd_flg);
static void q6asm_add_hdr_async(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg);
static int q6asm_memory_map_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt,
				bool is_contiguous);
static int q6asm_memory_unmap_regions(struct audio_client *ac, int dir);
static void q6asm_reset_buf_state(struct audio_client *ac);

static int q6asm_map_channels(u8 *channel_mapping, uint32_t channels);
void *q6asm_mmap_apr_reg(void);

static int q6asm_is_valid_session(struct apr_client_data *data, void *priv);

/* for ASM custom topology */
static struct audio_buffer common_buf[2];
static struct audio_client common_client;
static int set_custom_topology;
static int topology_map_handle;

#ifdef CONFIG_DEBUG_FS
#define OUT_BUFFER_SIZE 56
#define IN_BUFFER_SIZE 24

static struct timeval out_cold_tv;
static struct timeval out_warm_tv;
static struct timeval out_cont_tv;
static struct timeval in_cont_tv;
static long out_enable_flag;
static long in_enable_flag;
static struct dentry *out_dentry;
static struct dentry *in_dentry;
static int in_cont_index;
/*This var is used to keep track of first write done for cold output latency */
static int out_cold_index;
static char *out_buffer;
static char *in_buffer;


int q6asm_mmap_apr_dereg(void)
{
	int c;

	c = atomic_sub_return(1, &this_mmap.ref_cnt);
	if (c == 0) {
		apr_deregister(this_mmap.apr);
		pr_debug("%s: APR De-Register common port\n", __func__);
	} else if (c < 0) {
		pr_err("%s: APR Common Port Already Closed\n", __func__);
		atomic_set(&this_mmap.ref_cnt, 0);
	}

	return 0;
}

static int audio_output_latency_dbgfs_open(struct inode *inode,
							struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
static ssize_t audio_output_latency_dbgfs_read(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
	if (out_buffer == NULL) {
		pr_err("%s: out_buffer is null\n", __func__);
		return 0;
	}
	snprintf(out_buffer, OUT_BUFFER_SIZE, "%ld,%ld,%ld,%ld,%ld,%ld,",\
		out_cold_tv.tv_sec, out_cold_tv.tv_usec, out_warm_tv.tv_sec,\
		out_warm_tv.tv_usec, out_cont_tv.tv_sec, out_cont_tv.tv_usec);
	return  simple_read_from_buffer(buf, OUT_BUFFER_SIZE, ppos,
						out_buffer, OUT_BUFFER_SIZE);
}
static ssize_t audio_output_latency_dbgfs_write(struct file *file,
			const char __user *buf, size_t count, loff_t *ppos)
{
	char *temp;

	if (count > 2*sizeof(char))
		return -EINVAL;
	else
		temp  = kmalloc(2*sizeof(char), GFP_KERNEL);

	out_cold_index = 0;

	if (temp) {
		if (copy_from_user(temp, buf, 2*sizeof(char))) {
			kfree(temp);
			return -EFAULT;
		}
		if (!kstrtol(temp, 10, &out_enable_flag)) {
			kfree(temp);
			return count;
		}
		kfree(temp);
	}
	return -EINVAL;
}
static const struct file_operations audio_output_latency_debug_fops = {
	.open = audio_output_latency_dbgfs_open,
	.read = audio_output_latency_dbgfs_read,
	.write = audio_output_latency_dbgfs_write
};
static int audio_input_latency_dbgfs_open(struct inode *inode,
							struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
static ssize_t audio_input_latency_dbgfs_read(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
	if (in_buffer == NULL) {
		pr_err("%s: in_buffer is null\n", __func__);
		return 0;
	}
	snprintf(in_buffer, IN_BUFFER_SIZE, "%ld,%ld,",\
				in_cont_tv.tv_sec, in_cont_tv.tv_usec);
	return  simple_read_from_buffer(buf, IN_BUFFER_SIZE, ppos,
						in_buffer, IN_BUFFER_SIZE);
}
static ssize_t audio_input_latency_dbgfs_write(struct file *file,
			const char __user *buf, size_t count, loff_t *ppos)
{
	char *temp;

	if (count > 2*sizeof(char))
		return -EINVAL;
	else
		temp  = kmalloc(2*sizeof(char), GFP_KERNEL);
	if (temp) {
		if (copy_from_user(temp, buf, 2*sizeof(char))) {
			kfree(temp);
			return -EFAULT;
		}
		if (!kstrtol(temp, 10, &in_enable_flag)) {
			kfree(temp);
			return count;
		}
		kfree(temp);
	}
	return -EINVAL;
}
static const struct file_operations audio_input_latency_debug_fops = {
	.open = audio_input_latency_dbgfs_open,
	.read = audio_input_latency_dbgfs_read,
	.write = audio_input_latency_dbgfs_write
};

static void config_debug_fs_write_cb(void)
{
	if (out_enable_flag) {
		/* For first Write done log the time and reset
		out_cold_index*/
		if (out_cold_index != 1) {
			do_gettimeofday(&out_cold_tv);
			pr_debug("COLD: apr_send_pkt at %ld sec %ld microsec\n",
				out_cold_tv.tv_sec,\
				out_cold_tv.tv_usec);
			out_cold_index = 1;
		}
		pr_debug("out_enable_flag %ld",\
				out_enable_flag);
	}
}
static void config_debug_fs_read_cb(void)
{
	if (in_enable_flag) {
		/* when in_cont_index == 7, DSP would be
		* writing into the 8th 512 byte buffer and this
		* timestamp is tapped here.Once done it then writes
		* to 9th 512 byte buffer.These two buffers(8th, 9th)
		* reach the test application in 5th iteration and that
		* timestamp is tapped at user level. The difference
		* of these two timestamps gives us the time between
		* the time at which dsp started filling the sample
		* required and when it reached the test application.
		* Hence continuous input latency
		*/
		if (in_cont_index == 7) {
			do_gettimeofday(&in_cont_tv);
			pr_err("In_CONT:previous read buffer done at %ld sec %ld microsec\n",
				in_cont_tv.tv_sec, in_cont_tv.tv_usec);
		}
		in_cont_index++;
	}
}

static void config_debug_fs_reset_index(void)
{
	in_cont_index = 0;
}

static void config_debug_fs_run(void)
{
	if (out_enable_flag) {
		do_gettimeofday(&out_cold_tv);
		pr_debug("COLD: apr_send_pkt at %ld sec %ld microsec\n",\
				out_cold_tv.tv_sec, out_cold_tv.tv_usec);
	}
}

static void config_debug_fs_write(struct audio_buffer *ab)
{
	if (out_enable_flag) {
		char zero_pattern[2] = {0x00, 0x00};
		/* If First two byte is non zero and last two byte
		is zero then it is warm output pattern */
		if ((strncmp(((char *)ab->data), zero_pattern, 2)) &&
		(!strncmp(((char *)ab->data + 2), zero_pattern, 2))) {
			do_gettimeofday(&out_warm_tv);
			pr_debug("WARM:apr_send_pkt at %ld sec %ld microsec\n",
			 out_warm_tv.tv_sec,\
			out_warm_tv.tv_usec);
			pr_debug("Warm Pattern Matched");
		}
		/* If First two byte is zero and last two byte is
		non zero then it is cont ouput pattern */
		else if ((!strncmp(((char *)ab->data), zero_pattern, 2))
		&& (strncmp(((char *)ab->data + 2), zero_pattern, 2))) {
			do_gettimeofday(&out_cont_tv);
			pr_debug("CONT:apr_send_pkt at %ld sec %ld microsec\n",
			out_cont_tv.tv_sec,\
			out_cont_tv.tv_usec);
			pr_debug("Cont Pattern Matched");
		}
	}
}
static void config_debug_fs_init(void)
{
	out_buffer = kmalloc(OUT_BUFFER_SIZE, GFP_KERNEL);
	if (out_buffer == NULL) {
		pr_err("%s: kmalloc() for out_buffer failed\n", __func__);
		goto outbuf_fail;
	}
	in_buffer = kmalloc(IN_BUFFER_SIZE, GFP_KERNEL);
	if (in_buffer == NULL) {
		pr_err("%s: kmalloc() for in_buffer failed\n", __func__);
		goto inbuf_fail;
	}
	out_dentry = debugfs_create_file("audio_out_latency_measurement_node",\
				S_IRUGO | S_IWUSR | S_IWGRP,\
				NULL, NULL, &audio_output_latency_debug_fops);
	if (IS_ERR(out_dentry)) {
		pr_err("%s: debugfs_create_file failed\n", __func__);
		goto file_fail;
	}
	in_dentry = debugfs_create_file("audio_in_latency_measurement_node",\
				S_IRUGO | S_IWUSR | S_IWGRP,\
				NULL, NULL, &audio_input_latency_debug_fops);
	if (IS_ERR(in_dentry)) {
		pr_err("%s: debugfs_create_file failed\n", __func__);
		goto file_fail;
	}
	return;
file_fail:
	kfree(in_buffer);
inbuf_fail:
	kfree(out_buffer);
outbuf_fail:
	in_buffer = NULL;
	out_buffer = NULL;
	return;
}
#else
static void config_debug_fs_write(struct audio_buffer *ab)
{
	return;
}
static void config_debug_fs_run(void)
{
	return;
}
static void config_debug_fs_reset_index(void)
{
	return;
}
static void config_debug_fs_read_cb(void)
{
	return;
}
static void config_debug_fs_write_cb(void)
{
	return;
}
static void config_debug_fs_init(void)
{
	return;
}
#endif


static int q6asm_session_alloc(struct audio_client *ac)
{
	int n;
	for (n = 1; n <= SESSION_MAX; n++) {
		if (!session[n]) {
			session[n] = ac;
			return n;
		}
	}
	return -ENOMEM;
}

static void q6asm_session_free(struct audio_client *ac)
{
	pr_debug("%s: sessionid[%d]\n", __func__, ac->session);
	rtac_remove_popp_from_adm_devices(ac->session);
	session[ac->session] = 0;
	ac->session = 0;
	ac->perf_mode = LEGACY_PCM_MODE;
	ac->fptr_cache_ops = NULL;
	return;
}

void send_asm_custom_topology(struct audio_client *ac)
{
	struct acdb_cal_block		cal_block;
	struct cmd_set_topologies	asm_top;
	struct asm_buffer_node		*buf_node = NULL;
	struct list_head		*ptr, *next;
	int				result;
	int				size = 4096;

	if (!set_custom_topology)
		return;

	get_asm_custom_topology(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_debug("%s: no cal to send addr= 0x%pa\n",
				__func__, &cal_block.cal_paddr);
		return;
	}

	common_client.mmap_apr = q6asm_mmap_apr_reg();
	common_client.apr = common_client.mmap_apr;
	if (common_client.mmap_apr == NULL) {
		pr_err("%s: q6asm_mmap_apr_reg failed\n",
			__func__);
		result = -EPERM;
		goto mmap_fail;
	}
	/* Only call this once */
	set_custom_topology = 0;

	/* Use first asm buf to map memory */
	if (common_client.port[IN].buf == NULL) {
		pr_err("%s: common buf is NULL\n",
			__func__);
		goto err_map;
	}
	common_client.port[IN].buf->phys = cal_block.cal_paddr;

	result = q6asm_memory_map_regions(&common_client,
						IN, size, 1, 1);
	if (result < 0) {
		pr_err("%s: mmap did not work! addr = 0x%pa, size = %zd\n",
			__func__, &cal_block.cal_paddr,
			cal_block.cal_size);
		goto err_map;
	}

	list_for_each_safe(ptr, next,
			&common_client.port[IN].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
					list);
		if (buf_node->buf_addr_lsw == cal_block.cal_paddr) {
			topology_map_handle =  buf_node->mmap_hdl;
			break;
		}
	}

	q6asm_add_hdr_custom_topology(ac, &asm_top.hdr,
				      APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(asm_top)), TRUE);
	atomic_set(&ac->mem_state, 1);
	asm_top.hdr.opcode = ASM_CMD_ADD_TOPOLOGIES;
	asm_top.payload_addr_lsw = cal_block.cal_paddr;
	asm_top.payload_addr_msw = 0;
	asm_top.mem_map_handle = topology_map_handle;
	asm_top.payload_size = cal_block.cal_size;

	 pr_debug("%s: Sending ASM_CMD_ADD_TOPOLOGIES payload = 0x%x, size = %d, map handle = 0x%x\n",
		__func__, asm_top.payload_addr_lsw,
		asm_top.payload_size, asm_top.mem_map_handle);

	result = apr_send_pkt(ac->apr, (uint32_t *) &asm_top);
	if (result < 0) {
		pr_err("%s: Set topologies failed payload = 0x%x\n",
			__func__, cal_block.cal_paddr);
		goto err_unmap;
	}

	result = wait_event_timeout(ac->mem_wait,
			(atomic_read(&ac->mem_state) == 0), 5*HZ);
	if (!result) {
		pr_err("%s: Set topologies failed after timedout payload = 0x%x\n",
			__func__, cal_block.cal_paddr);
		goto err_unmap;
	}
	return;
err_unmap:
	q6asm_memory_unmap_regions(ac, IN);
err_map:
	q6asm_mmap_apr_dereg();
	set_custom_topology = 1;
mmap_fail:
	return;
}

int q6asm_map_rtac_block(struct rtac_cal_block_data *cal_block)
{
	int			result = 0;
	struct asm_buffer_node	*buf_node = NULL;
	struct list_head	*ptr, *next;
	pr_debug("%s\n", __func__);

	if (cal_block == NULL) {
		pr_err("%s: cal_block is NULL!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (cal_block->cal_data.paddr == 0) {
		pr_debug("%s: No address to map!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (common_client.mmap_apr == NULL) {
		common_client.mmap_apr = q6asm_mmap_apr_reg();
		if (common_client.mmap_apr == NULL) {
			pr_err("%s: q6asm_mmap_apr_reg failed\n",
				__func__);
			result = -EPERM;
			goto done;
		}
	}

	if (cal_block->map_data.map_size == 0) {
		pr_debug("%s: map size is 0!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	/* Use second asm buf to map memory */
	if (common_client.port[OUT].buf == NULL) {
		pr_err("%s: common buf is NULL\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	common_client.port[OUT].buf->phys = cal_block->cal_data.paddr;

	result = q6asm_memory_map_regions(&common_client,
			OUT, cal_block->map_data.map_size, 1, 1);
	if (result < 0) {
		pr_err("%s: mmap did not work! addr = 0x%x, size = %d\n",
			__func__, cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
		goto done;
	}

	list_for_each_safe(ptr, next,
		&common_client.port[OUT].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
					list);
		if (buf_node->buf_addr_lsw == cal_block->cal_data.paddr) {
			cal_block->map_data.map_handle =  buf_node->mmap_hdl;
			break;
		}
	}

	result = q6asm_mmap_apr_dereg();
	if (result < 0) {
		pr_err("%s: q6asm_mmap_apr_dereg failed, err %d\n",
			__func__, result);
	} else {
		common_client.mmap_apr = NULL;
	}
done:
	return result;
}

int q6asm_unmap_rtac_block(uint32_t *mem_map_handle)
{
	int	result = 0;
	int	result2 = 0;
	pr_debug("%s\n", __func__);

	if (mem_map_handle == NULL) {
		pr_debug("%s: Map handle is NULL, nothing to unmap\n",
			__func__);
		goto done;
	}

	if (*mem_map_handle == 0) {
		pr_debug("%s: Map handle is 0, nothing to unmap\n",
			__func__);
		goto done;
	}

	if (common_client.mmap_apr == NULL) {
		common_client.mmap_apr = q6asm_mmap_apr_reg();
		if (common_client.mmap_apr == NULL) {
			pr_err("%s: q6asm_mmap_apr_reg failed\n",
				__func__);
			result = -EPERM;
			goto done;
		}
	}


	result2 = q6asm_memory_unmap_regions(&common_client, OUT);
	if (result2 < 0) {
		pr_err("%s: unmap failed, err %d\n",
			__func__, result2);
		result = result2;
	} else {
		mem_map_handle = 0;
	}

	result2 = q6asm_mmap_apr_dereg();
	if (result2 < 0) {
		pr_err("%s: q6asm_mmap_apr_dereg failed, err %d\n",
			__func__, result2);
		result = result2;
	} else {
		common_client.mmap_apr = NULL;
	}
done:
	return result;
}

int q6asm_unmap_cal_blocks(void)
{
	int	result = 0;
	int	result2 = 0;
	pr_debug("%s\n", __func__);

	if (topology_map_handle == 0)
		goto done;

	if (common_client.mmap_apr == NULL) {
		common_client.mmap_apr = q6asm_mmap_apr_reg();
		if (common_client.mmap_apr == NULL) {
			pr_err("%s: q6asm_mmap_apr_reg failed\n",
				__func__);
			result = -EPERM;
			goto done;
		}
	}

	result2 = q6asm_memory_unmap_regions(&common_client, IN);
	if (result2 < 0) {
		pr_err("%s: unmap failed, err %d\n",
			__func__, result2);
		result = result2;
	} else {
		topology_map_handle = 0;
	}

	result2 = q6asm_mmap_apr_dereg();
	if (result2 < 0) {
		pr_err("%s: q6asm_mmap_apr_dereg failed, err %d\n",
			__func__, result2);
		result = result2;
	} else {
		common_client.mmap_apr = NULL;
	}

	set_custom_topology = 0;

done:
	return result;
}

int q6asm_audio_client_buf_free(unsigned int dir,
			struct audio_client *ac)
{
	struct audio_port_data *port;
	int cnt = 0;
	int rc = 0;
	pr_debug("%s: Session id %d\n", __func__, ac->session);
	mutex_lock(&ac->cmd_lock);
	if (ac->io_mode & SYNC_IO_MODE) {
		port = &ac->port[dir];
		if (!port->buf) {
			mutex_unlock(&ac->cmd_lock);
			return 0;
		}
		cnt = port->max_buf_cnt - 1;

		if (cnt >= 0) {
			rc = q6asm_memory_unmap_regions(ac, dir);
			if (rc < 0)
				pr_err("%s CMD Memory_unmap_regions failed\n",
								__func__);
		}

		while (cnt >= 0) {
			if (port->buf[cnt].data) {
				if (!rc || atomic_read(&ac->reset))
					msm_audio_ion_free(
						port->buf[cnt].client,
						port->buf[cnt].handle);

				port->buf[cnt].client = NULL;
				port->buf[cnt].handle = NULL;
				port->buf[cnt].data = NULL;
				port->buf[cnt].phys = 0;
				--(port->max_buf_cnt);
			}
			--cnt;
		}
		kfree(port->buf);
		port->buf = NULL;
	}
	mutex_unlock(&ac->cmd_lock);
	return 0;
}

int q6asm_audio_client_buf_free_contiguous(unsigned int dir,
			struct audio_client *ac)
{
	struct audio_port_data *port;
	int cnt = 0;
	int rc = 0;
	pr_debug("%s: Session id %d\n", __func__, ac->session);
	mutex_lock(&ac->cmd_lock);
	port = &ac->port[dir];
	if (!port->buf) {
		mutex_unlock(&ac->cmd_lock);
		return 0;
	}
	cnt = port->max_buf_cnt - 1;

	if (cnt >= 0) {
		rc = q6asm_memory_unmap(ac, port->buf[0].phys, dir);
		if (rc < 0)
			pr_err("%s CMD Memory_unmap_regions failed\n",
							__func__);
	}

	if (port->buf[0].data) {
		pr_debug("%s:data[%p]phys[%p][%p] , client[%p] handle[%p]\n",
			__func__,
			(void *)port->buf[0].data,
			(void *)port->buf[0].phys,
			(void *)&port->buf[0].phys,
			(void *)port->buf[0].client,
			(void *)port->buf[0].handle);
		if (!rc || atomic_read(&ac->reset))
			msm_audio_ion_free(port->buf[0].client,
					   port->buf[0].handle);
		port->buf[0].client = NULL;
		port->buf[0].handle = NULL;
	}

	while (cnt >= 0) {
		port->buf[cnt].data = NULL;
		port->buf[cnt].phys = 0;
		cnt--;
	}
	port->max_buf_cnt = 0;
	kfree(port->buf);
	port->buf = NULL;
	mutex_unlock(&ac->cmd_lock);
	return 0;
}

void q6asm_audio_client_free(struct audio_client *ac)
{
	int loopcnt;
	struct audio_port_data *port;
	if (!ac || !ac->session)
		return;

	mutex_lock(&session_lock);

	pr_debug("%s: Session id %d\n", __func__, ac->session);
	if (ac->io_mode & SYNC_IO_MODE) {
		for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
			port = &ac->port[loopcnt];
			if (!port->buf)
				continue;
			pr_debug("%s:loopcnt = %d\n", __func__, loopcnt);
			q6asm_audio_client_buf_free(loopcnt, ac);
		}
	}

	apr_deregister(ac->apr2);
	ac->apr2 = NULL;
	apr_deregister(ac->apr);
	ac->apr = NULL;
	ac->mmap_apr = NULL;
	rtac_set_asm_handle(ac->session, ac->apr);
	q6asm_session_free(ac);
	q6asm_mmap_apr_dereg();

	pr_debug("%s: APR De-Register\n", __func__);

/*done:*/
	kfree(ac);
	ac = NULL;
	mutex_unlock(&session_lock);

	return;
}

int q6asm_set_io_mode(struct audio_client *ac, uint32_t mode1)
{
	uint32_t mode;

	if (ac == NULL) {
		pr_err("%s APR handle NULL\n", __func__);
		return -EINVAL;
	}

	ac->io_mode &= 0xFF00;
	mode = (mode1 & 0xF);

	pr_debug("%s ac->mode after anding with FF00:0x[%x],\n",
		__func__, ac->io_mode);

	if ((mode == ASYNC_IO_MODE) || (mode == SYNC_IO_MODE)) {
		ac->io_mode |= mode1;
		pr_debug("%s:Set Mode to 0x[%x]\n", __func__, ac->io_mode);
		return 0;
	} else {
		pr_err("%s:Not an valid IO Mode:%d\n", __func__, ac->io_mode);
		return -EINVAL;
	}
}

void *q6asm_mmap_apr_reg(void)
{
	if ((atomic_read(&this_mmap.ref_cnt) == 0) ||
	    (this_mmap.apr == NULL)) {
		this_mmap.apr = apr_register("ADSP", "ASM", \
					(apr_fn)q6asm_srvc_callback,\
					0x0FFFFFFFF, &this_mmap);
		if (this_mmap.apr == NULL) {
			pr_debug("%s Unable to register APR ASM common port\n",
			 __func__);
			goto fail;
		}
	}
	atomic_inc(&this_mmap.ref_cnt);
	return this_mmap.apr;
fail:
	return NULL;
}

struct audio_client *q6asm_audio_client_alloc(app_cb cb, void *priv)
{
	struct audio_client *ac;
	int n;
	int lcnt = 0;

	ac = kzalloc(sizeof(struct audio_client), GFP_KERNEL);
	if (!ac)
		return NULL;

	mutex_lock(&session_lock);
	n = q6asm_session_alloc(ac);
	if (n <= 0) {
		mutex_unlock(&session_lock);
		goto fail_session;
	}
	ac->session = n;
	ac->cb = cb;
	ac->priv = priv;
	ac->io_mode = SYNC_IO_MODE;
	ac->perf_mode = LEGACY_PCM_MODE;
	ac->fptr_cache_ops = NULL;
	/* DSP expects stream id from 1 */
	ac->stream_id = 1;
	ac->apr = apr_register("ADSP", "ASM", \
				(apr_fn)q6asm_callback,\
				((ac->session) << 8 | 0x0001),\
				ac);

	if (ac->apr == NULL) {
		pr_err("%s Registration with APR failed\n", __func__);
		mutex_unlock(&session_lock);
		goto fail_apr1;
	}
	ac->apr2 = apr_register("ADSP", "ASM", \
				(apr_fn)q6asm_callback,\
				((ac->session) << 8 | 0x0002),\
				ac);

	if (ac->apr2 == NULL) {
		pr_err("%s Registration with APR-2 failed\n", __func__);
		mutex_unlock(&session_lock);
		goto fail_apr2;
	}

	rtac_set_asm_handle(n, ac->apr);

	pr_debug("%s Registering the common port with APR\n", __func__);
	ac->mmap_apr = q6asm_mmap_apr_reg();
	if (ac->mmap_apr == NULL) {
		mutex_unlock(&session_lock);
		goto fail_mmap;
	}

	init_waitqueue_head(&ac->cmd_wait);
	init_waitqueue_head(&ac->time_wait);
	init_waitqueue_head(&ac->mem_wait);
	atomic_set(&ac->time_flag, 1);
	atomic_set(&ac->reset, 0);
	INIT_LIST_HEAD(&ac->port[0].mem_map_handle);
	INIT_LIST_HEAD(&ac->port[1].mem_map_handle);
	pr_debug("%s: mem_map_handle list init'ed\n", __func__);
	mutex_init(&ac->cmd_lock);
	for (lcnt = 0; lcnt <= OUT; lcnt++) {
		mutex_init(&ac->port[lcnt].lock);
		spin_lock_init(&ac->port[lcnt].dsp_lock);
	}
	atomic_set(&ac->cmd_state, 0);
	atomic_set(&ac->nowait_cmd_cnt, 0);
	atomic_set(&ac->mem_state, 0);

	send_asm_custom_topology(ac);

	pr_debug("%s: session[%d]\n", __func__, ac->session);

	mutex_unlock(&session_lock);

	return ac;
fail_mmap:
	apr_deregister(ac->apr2);
fail_apr2:
	apr_deregister(ac->apr);
fail_apr1:
	q6asm_session_free(ac);
fail_session:
	kfree(ac);
	return NULL;
}

struct audio_client *q6asm_get_audio_client(int session_id)
{
	if (session_id == ASM_CONTROL_SESSION) {
		return &common_client;
	}

	if ((session_id <= 0) || (session_id > SESSION_MAX)) {
		pr_err("%s: invalid session: %d\n", __func__, session_id);
		goto err;
	}

	if (!session[session_id]) {
		pr_err("%s: session not active: %d\n", __func__, session_id);
		goto err;
	}
	return session[session_id];
err:
	return NULL;
}

int q6asm_audio_client_buf_alloc(unsigned int dir,
			struct audio_client *ac,
			unsigned int bufsz,
			unsigned int bufcnt)
{
	int cnt = 0;
	int rc = 0;
	struct audio_buffer *buf;
	int len;

	if (!(ac) || ((dir != IN) && (dir != OUT)))
		return -EINVAL;

	pr_debug("%s: session[%d]bufsz[%d]bufcnt[%d]\n", __func__, ac->session,
		bufsz, bufcnt);

	if (ac->session <= 0 || ac->session > 8)
		goto fail;

	if (ac->io_mode & SYNC_IO_MODE) {
		if (ac->port[dir].buf) {
			pr_debug("%s: buffer already allocated\n", __func__);
			return 0;
		}
		mutex_lock(&ac->cmd_lock);
		buf = kzalloc(((sizeof(struct audio_buffer))*bufcnt),
				GFP_KERNEL);

		if (!buf) {
			mutex_unlock(&ac->cmd_lock);
			goto fail;
		}

		ac->port[dir].buf = buf;

		while (cnt < bufcnt) {
			if (bufsz > 0) {
				if (!buf[cnt].data) {
					msm_audio_ion_alloc("audio_client",
					&buf[cnt].client, &buf[cnt].handle,
					      bufsz,
					      (ion_phys_addr_t *)&buf[cnt].phys,
					      (size_t *)&len,
					      &buf[cnt].data);
					if (rc) {
						pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
							__func__, rc);
						mutex_unlock(&ac->cmd_lock);
					goto fail;
					}

					buf[cnt].used = 1;
					buf[cnt].size = bufsz;
					buf[cnt].actual_size = bufsz;
					pr_debug("%s data[%p]phys[%p][%p]\n",
						__func__,
					   (void *)buf[cnt].data,
					   (void *)buf[cnt].phys,
					   (void *)&buf[cnt].phys);
					cnt++;
				}
			}
		}
		ac->port[dir].max_buf_cnt = cnt;

		mutex_unlock(&ac->cmd_lock);
		rc = q6asm_memory_map_regions(ac, dir, bufsz, cnt, 0);
		if (rc < 0) {
			pr_err("%s:CMD Memory_map_regions failed\n", __func__);
			goto fail;
		}
	}
	return 0;
fail:
	q6asm_audio_client_buf_free(dir, ac);
	return -EINVAL;
}

int q6asm_audio_client_buf_alloc_contiguous(unsigned int dir,
			struct audio_client *ac,
			unsigned int bufsz,
			unsigned int bufcnt)
{
	int cnt = 0;
	int rc = 0;
	struct audio_buffer *buf;
	int len;
	int bytes_to_alloc;

	if (!(ac) || ((dir != IN) && (dir != OUT)))
		return -EINVAL;

	pr_debug("%s: session[%d]bufsz[%d]bufcnt[%d]\n",
			__func__, ac->session,
			bufsz, bufcnt);

	if (ac->session <= 0 || ac->session > 8)
		goto fail;

	if (ac->port[dir].buf) {
		pr_debug("%s: buffer already allocated\n", __func__);
		return 0;
	}
	mutex_lock(&ac->cmd_lock);
	buf = kzalloc(((sizeof(struct audio_buffer))*bufcnt),
			GFP_KERNEL);

	if (!buf) {
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}

	ac->port[dir].buf = buf;

	bytes_to_alloc = bufsz * bufcnt;

	/* The size to allocate should be multiple of 4K bytes */
	bytes_to_alloc = PAGE_ALIGN(bytes_to_alloc);

	rc = msm_audio_ion_alloc("audio_client", &buf[0].client, &buf[0].handle,
		bytes_to_alloc,
		(ion_phys_addr_t *)&buf[0].phys, (size_t *)&len,
		&buf[0].data);
	if (rc) {
		pr_err("%s: Audio ION alloc is failed, rc = %d\n",
			__func__, rc);
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}

	buf[0].used = dir ^ 1;
	buf[0].size = bufsz;
	buf[0].actual_size = bufsz;
	cnt = 1;
	while (cnt < bufcnt) {
		if (bufsz > 0) {
			buf[cnt].data =  buf[0].data + (cnt * bufsz);
			buf[cnt].phys =  buf[0].phys + (cnt * bufsz);
			if (!buf[cnt].data) {
				pr_err("%s Buf alloc failed\n",
							__func__);
				mutex_unlock(&ac->cmd_lock);
				goto fail;
			}
			buf[cnt].used = dir ^ 1;
			buf[cnt].size = bufsz;
			buf[cnt].actual_size = bufsz;
			pr_debug("%s data[%p]phys[%p][%p]\n", __func__,
				   (void *)buf[cnt].data,
				   (void *)buf[cnt].phys,
				   (void *)&buf[cnt].phys);
		}
		cnt++;
	}
	ac->port[dir].max_buf_cnt = cnt;
	mutex_unlock(&ac->cmd_lock);
	rc = q6asm_memory_map_regions(ac, dir, bufsz, cnt, 1);
	if (rc < 0) {
		pr_err("%s:CMD Memory_map_regions failed\n", __func__);
		goto fail;
	}
	return 0;
fail:
	q6asm_audio_client_buf_free_contiguous(dir, ac);
	return -EINVAL;
}

static int32_t q6asm_srvc_callback(struct apr_client_data *data, void *priv)
{
	uint32_t sid = 0;
	uint32_t dir = 0;
	uint32_t i = IN;
	uint32_t *payload;
	unsigned long dsp_flags;
	struct asm_buffer_node *buf_node = NULL;
	struct list_head *ptr, *next;

	struct audio_client *ac = NULL;
	struct audio_port_data *port;

	if (!data) {
		pr_err("%s: Invalid CB\n", __func__);
		return 0;
	}

	payload = data->payload;

	if (data->opcode == RESET_EVENTS) {

		pr_debug("%s: Reset event is received: %d %d apr[%p]\n",
				__func__,
				data->reset_event,
				data->reset_proc,
				this_mmap.apr);
		atomic_set(&this_mmap.ref_cnt, 0);
		apr_reset(this_mmap.apr);
		this_mmap.apr = NULL;
		for (; i <= OUT; i++) {
			list_for_each_safe(ptr, next,
				&common_client.port[i].mem_map_handle) {
				buf_node = list_entry(ptr,
						struct asm_buffer_node,
						list);
				if (buf_node->buf_addr_lsw ==
				common_client.port[i].buf->phys) {
					list_del(&buf_node->list);
					kfree(buf_node);
				}
			}
			pr_debug("%s:Clearing custom topology\n", __func__);
		}
		reset_custom_topology_flags();
		set_custom_topology = 1;
		topology_map_handle = 0;
		rtac_clear_mapping(ASM_RTAC_CAL);
		return 0;
	}
	sid = (data->token >> 8) & 0x0F;
	ac = q6asm_get_audio_client(sid);
	if (!ac) {
		pr_debug("%s: session[%d] already freed\n", __func__, sid);
		return 0;
	}
	pr_debug("%s:ptr0[0x%x]ptr1[0x%x]opcode[0x%x] token[0x%x]payload_s[%d] src[%d] dest[%d]sid[%d]dir[%d]\n",
		__func__, payload[0], payload[1], data->opcode, data->token,
		data->payload_size, data->src_port, data->dest_port, sid, dir);
	pr_debug("%s:Payload = [0x%x] status[0x%x]\n",
			__func__, payload[0], payload[1]);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		switch (payload[0]) {
		case ASM_CMD_SHARED_MEM_MAP_REGIONS:
		case ASM_CMD_SHARED_MEM_UNMAP_REGIONS:
		case ASM_CMD_ADD_TOPOLOGIES:
			if (payload[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x sid:%d\n",
					__func__, payload[0], payload[1], sid);
				if (payload[0] ==
				    ASM_CMD_SHARED_MEM_UNMAP_REGIONS)
					atomic_set(&ac->unmap_cb_success, 0);
			} else {
				if (payload[0] ==
				    ASM_CMD_SHARED_MEM_UNMAP_REGIONS)
					atomic_set(&ac->unmap_cb_success, 1);
			}

			if (atomic_cmpxchg(&ac->mem_state, 1, 0))
				wake_up(&ac->mem_wait);
			pr_debug("%s:Payload = [0x%x] status[0x%x]\n",
					__func__, payload[0], payload[1]);
			break;
		default:
			pr_debug("%s:command[0x%x] not expecting rsp\n",
						__func__, payload[0]);
			break;
		}
		return 0;
	}

	dir = (data->token & 0x0F);
	port = &ac->port[dir];

	switch (data->opcode) {
	case ASM_CMDRSP_SHARED_MEM_MAP_REGIONS:{
		pr_debug("%s:PL#0[0x%x]PL#1 [0x%x] dir=%x s_id=%x\n",
				__func__, payload[0], payload[1], dir, sid);
		spin_lock_irqsave(&port->dsp_lock, dsp_flags);
		if (atomic_cmpxchg(&ac->mem_state, 1, 0)) {
			ac->port[dir].tmp_hdl = payload[0];
			wake_up(&ac->mem_wait);
		}
		spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);
		break;
	}
	case ASM_CMD_SHARED_MEM_UNMAP_REGIONS:{
		pr_debug("%s:PL#0[0x%x]PL#1 [0x%x]\n",
					__func__, payload[0], payload[1]);
		spin_lock_irqsave(&port->dsp_lock, dsp_flags);
		if (atomic_cmpxchg(&ac->mem_state, 1, 0))
			wake_up(&ac->mem_wait);
		spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);

		break;
	}
	default:
		pr_debug("%s:command[0x%x]success [0x%x]\n",
					__func__, payload[0], payload[1]);
	}
	if (ac->cb)
		ac->cb(data->opcode, data->token,
			data->payload, ac->priv);
	return 0;
}

static int32_t is_no_wait_cmd_rsp(uint32_t opcode, uint32_t *cmd_type)
{
	if (opcode == APR_BASIC_RSP_RESULT) {
		if (cmd_type != NULL) {
			switch (cmd_type[0]) {
			case ASM_SESSION_CMD_RUN_V2:
			case ASM_SESSION_CMD_PAUSE:
			case ASM_DATA_CMD_EOS:
				return 1;
			default:
				break;
			}
		} else
			pr_err("%s: null pointer!", __func__);
	} else if (opcode == ASM_DATA_EVENT_RENDERED_EOS)
		return 1;

	return 0;
}

static int32_t q6asm_callback(struct apr_client_data *data, void *priv)
{
	int i = 0;
	struct audio_client *ac = (struct audio_client *)priv;
	uint32_t token;
	unsigned long dsp_flags;
	uint32_t *payload;
	uint32_t wakeup_flag = 1;
	int32_t  ret = 0;


	if ((ac == NULL) || (data == NULL)) {
		pr_err("ac or priv NULL\n");
		return -EINVAL;
	}
	if (ac->session <= 0 || ac->session > 8) {
		pr_err("%s:Session ID is invalid, session = %d\n", __func__,
			ac->session);
		return -EINVAL;
	}

	payload = data->payload;
	if ((atomic_read(&ac->nowait_cmd_cnt) > 0) &&
		is_no_wait_cmd_rsp(data->opcode, payload)) {
		pr_debug("%s: nowait_cmd_cnt %d\n",
				__func__,
				atomic_read(&ac->nowait_cmd_cnt));
		atomic_dec(&ac->nowait_cmd_cnt);
		wakeup_flag = 0;
	}

	if (data->opcode == RESET_EVENTS) {
		atomic_set(&ac->reset, 1);
		if (ac->apr == NULL)
			ac->apr = ac->apr2;
		pr_debug("q6asm_callback: Reset event is received: %d %d apr[%p]\n",
				data->reset_event, data->reset_proc, ac->apr);
		if (ac->cb)
			ac->cb(data->opcode, data->token,
				(uint32_t *)data->payload, ac->priv);
		apr_reset(ac->apr);
		ac->apr = NULL;
		atomic_set(&ac->time_flag, 0);
		atomic_set(&ac->cmd_state, 0);
		wake_up(&ac->time_wait);
		wake_up(&ac->cmd_wait);
		return 0;
	}

	pr_debug("%s: session[%d]opcode[0x%x] token[0x%x]payload_s[%d] src[%d] dest[%d]\n",
		 __func__,
		ac->session, data->opcode,
		data->token, data->payload_size, data->src_port,
		data->dest_port);
	if ((data->opcode != ASM_DATA_EVENT_RENDERED_EOS) &&
	    (data->opcode != ASM_DATA_EVENT_EOS) &&
	    (data->opcode != ASM_SESSION_EVENT_RX_UNDERFLOW))
		pr_debug("%s:Payload = [0x%x] status[0x%x]\n",
			__func__, payload[0], payload[1]);
	if (data->opcode == APR_BASIC_RSP_RESULT) {
		token = data->token;
		if (payload[1] != 0) {
			pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
				__func__, payload[0], payload[1]);
		}
		switch (payload[0]) {
		case ASM_STREAM_CMD_SET_PP_PARAMS_V2:
			if (rtac_make_asm_callback(ac->session, payload,
					data->payload_size))
				break;
		case ASM_SESSION_CMD_PAUSE:
		case ASM_SESSION_CMD_SUSPEND:
		case ASM_DATA_CMD_EOS:
		case ASM_STREAM_CMD_CLOSE:
		case ASM_STREAM_CMD_FLUSH:
		case ASM_SESSION_CMD_RUN_V2:
		case ASM_SESSION_CMD_REGISTER_FORX_OVERFLOW_EVENTS:
		case ASM_STREAM_CMD_FLUSH_READBUFS:
		pr_debug("%s:Payload = [0x%x]\n", __func__, payload[0]);
		ret = q6asm_is_valid_session(data, priv);
		if (ret != 0)
			return ret;

		case ASM_STREAM_CMD_OPEN_READ_V3:
		case ASM_STREAM_CMD_OPEN_WRITE_V3:
		case ASM_STREAM_CMD_OPEN_READWRITE_V2:
		case ASM_STREAM_CMD_OPEN_LOOPBACK_V2:
		case ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2:
		case ASM_STREAM_CMD_SET_ENCDEC_PARAM:
		case ASM_DATA_CMD_REMOVE_INITIAL_SILENCE:
		case ASM_DATA_CMD_REMOVE_TRAILING_SILENCE:
		case ASM_SESSION_CMD_REGISTER_FOR_RX_UNDERFLOW_EVENTS:
		pr_debug("%s:Payload = [0x%x]stat[0x%x]\n",
				__func__, payload[0], payload[1]);
			if (atomic_read(&ac->cmd_state) && wakeup_flag) {
				atomic_set(&ac->cmd_state, 0);
				wake_up(&ac->cmd_wait);
			}
			if (ac->cb)
				ac->cb(data->opcode, data->token,
					(uint32_t *)data->payload, ac->priv);
			break;
		case ASM_CMD_ADD_TOPOLOGIES:
			pr_debug("%s:Payload = [0x%x]stat[0x%x]\n",
				__func__, payload[0], payload[1]);
			if (atomic_read(&ac->mem_state) && wakeup_flag) {
				atomic_set(&ac->mem_state, 0);
				wake_up(&ac->mem_wait);
			}
			if (ac->cb)
				ac->cb(data->opcode, data->token,
					(uint32_t *)data->payload, ac->priv);
			break;
		case ASM_STREAM_CMD_GET_PP_PARAMS_V2:
			pr_debug("%s: ASM_STREAM_CMD_GET_PP_PARAMS_V2\n",
				__func__);
			/* Should only come here if there is an APR */
			/* error or malformed APR packet. Otherwise */
			/* response will be returned as */
			/* ASM_STREAM_CMDRSP_GET_PP_PARAMS_V2 */
			if (payload[1] != 0) {
				pr_err("%s: ASM get param error = %d, resuming\n",
					__func__, payload[1]);
				rtac_make_asm_callback(ac->session, payload,
							data->payload_size);
			}
			break;
		default:
			pr_debug("%s:command[0x%x] not expecting rsp\n",
							__func__, payload[0]);
			break;
		}
		return 0;
	}

	switch (data->opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2:{
		struct audio_port_data *port = &ac->port[IN];
		pr_debug("%s: Rxed opcode[0x%x] status[0x%x] token[%d]",
				__func__, payload[0], payload[1],
				data->token);
		if (ac->io_mode & SYNC_IO_MODE) {
			if (port->buf == NULL) {
				pr_err("%s: Unexpected Write Done\n",
								__func__);
				return -EINVAL;
			}
			spin_lock_irqsave(&port->dsp_lock, dsp_flags);
			if (port->buf[data->token].phys !=
				payload[0]) {
				pr_err("Buf expected[%p]rxed[%p]\n",\
				   (void *)port->buf[data->token].phys,\
				   (void *)payload[0]);
				spin_unlock_irqrestore(&port->dsp_lock,
								dsp_flags);
				return -EINVAL;
			}
			token = data->token;
			port->buf[token].used = 1;
			spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);

			config_debug_fs_write_cb();

			for (i = 0; i < port->max_buf_cnt; i++)
				pr_debug("%d ", port->buf[i].used);

		}
		break;
	}
	case ASM_STREAM_CMDRSP_GET_PP_PARAMS_V2:
		pr_debug("%s: ASM_STREAM_CMDRSP_GET_PP_PARAMS_V2\n", __func__);
			if (payload[0] != 0)
				pr_err("%s: ASM_STREAM_CMDRSP_GET_PP_PARAMS_V2 returned error = 0x%x\n",
					__func__, payload[0]);
		rtac_make_asm_callback(ac->session, payload,
			data->payload_size);
		break;
	case ASM_DATA_EVENT_READ_DONE_V2:{

		struct audio_port_data *port = &ac->port[OUT];

		config_debug_fs_read_cb();

		pr_debug("%s:R-D: status=%d buff_add=%x act_size=%d offset=%d\n",
				__func__, payload[READDONE_IDX_STATUS],
				payload[READDONE_IDX_BUFADD_LSW],
				payload[READDONE_IDX_SIZE],
				payload[READDONE_IDX_OFFSET]);

		pr_debug("%s:R-D:msw_ts=%d lsw_ts=%d memmap_hdl=%x flags=%d id=%d num=%d\n",
				__func__, payload[READDONE_IDX_MSW_TS],
				payload[READDONE_IDX_LSW_TS],
				payload[READDONE_IDX_MEMMAP_HDL],
				payload[READDONE_IDX_FLAGS],
				payload[READDONE_IDX_SEQ_ID],
				payload[READDONE_IDX_NUMFRAMES]);

		if (ac->io_mode & SYNC_IO_MODE) {
			if (port->buf == NULL) {
				pr_err("%s: Unexpected Write Done\n", __func__);
				return -EINVAL;
			}
			spin_lock_irqsave(&port->dsp_lock, dsp_flags);
			token = data->token;
			port->buf[token].used = 0;
			if (port->buf[token].phys !=
				payload[READDONE_IDX_BUFADD_LSW]) {
				pr_err("Buf expected[%p]rxed[%p]\n",\
				   (void *)port->buf[token].phys,\
				   (void *)payload[READDONE_IDX_BUFADD_LSW]);
				spin_unlock_irqrestore(&port->dsp_lock,
							dsp_flags);
				break;
			}
			port->buf[token].actual_size =
				payload[READDONE_IDX_SIZE];
			spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);
		}
		break;
	}
	case ASM_DATA_EVENT_EOS:
	case ASM_DATA_EVENT_RENDERED_EOS:
		pr_debug("%s:EOS ACK received: rxed opcode[0x%x]\n",
				  __func__, data->opcode);
		break;
	case ASM_SESSION_EVENTX_OVERFLOW:
		pr_debug("ASM_SESSION_EVENTX_OVERFLOW\n");
		break;
	case ASM_SESSION_EVENT_RX_UNDERFLOW:
		pr_debug("ASM_SESSION_EVENT_RX_UNDERFLOW\n");
		break;
	case ASM_SESSION_CMDRSP_GET_SESSIONTIME_V3:
		pr_debug("%s: ASM_SESSION_CMDRSP_GET_SESSIONTIME_V3, payload[0] = %d, payload[1] = %d, payload[2] = %d\n",
				 __func__,
				 payload[0], payload[1], payload[2]);
		ac->time_stamp = (uint64_t)(((uint64_t)payload[2] << 32) |
				payload[1]);
		if (atomic_cmpxchg(&ac->time_flag, 1, 0))
			wake_up(&ac->time_wait);
		break;
	case ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY:
	case ASM_DATA_EVENT_ENC_SR_CM_CHANGE_NOTIFY:
		pr_debug("%s: ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY, payload[0] = %d, payload[1] = %d, payload[2] = %d, payload[3] = %d\n",
				 __func__,
				payload[0], payload[1], payload[2],
				payload[3]);
		break;
	}
	if (ac->cb)
		ac->cb(data->opcode, data->token,
			data->payload, ac->priv);

	return 0;
}

void *q6asm_is_cpu_buf_avail(int dir, struct audio_client *ac, uint32_t *size,
				uint32_t *index)
{
	void *data;
	unsigned char idx;
	struct audio_port_data *port;

	if (!ac || ((dir != IN) && (dir != OUT)))
		return NULL;

	if (ac->io_mode & SYNC_IO_MODE) {
		port = &ac->port[dir];

		mutex_lock(&port->lock);
		idx = port->cpu_buf;
		if (port->buf == NULL) {
			pr_debug("%s:Buffer pointer null\n", __func__);
			mutex_unlock(&port->lock);
			return NULL;
		}
		/*  dir 0: used = 0 means buf in use
			dir 1: used = 1 means buf in use */
		if (port->buf[idx].used == dir) {
			/* To make it more robust, we could loop and get the
			next avail buf, its risky though */
			pr_debug("%s:Next buf idx[0x%x] not available, dir[%d]\n",
			 __func__, idx, dir);
			mutex_unlock(&port->lock);
			return NULL;
		}
		*size = port->buf[idx].actual_size;
		*index = port->cpu_buf;
		data = port->buf[idx].data;
		pr_debug("%s:session[%d]index[%d] data[%p]size[%d]\n",
						__func__,
						ac->session,
						port->cpu_buf,
						data, *size);
		/* By default increase the cpu_buf cnt
		user accesses this function,increase cpu
		buf(to avoid another api)*/
		port->buf[idx].used = dir;
		port->cpu_buf = ((port->cpu_buf + 1) & (port->max_buf_cnt - 1));
		mutex_unlock(&port->lock);
		return data;
	}
	return NULL;
}

void *q6asm_is_cpu_buf_avail_nolock(int dir, struct audio_client *ac,
					uint32_t *size, uint32_t *index)
{
	void *data;
	unsigned char idx;
	struct audio_port_data *port;

	if (!ac || ((dir != IN) && (dir != OUT)))
		return NULL;

	port = &ac->port[dir];

	idx = port->cpu_buf;
	if (port->buf == NULL) {
		pr_debug("%s:Buffer pointer null\n", __func__);
		return NULL;
	}
	/*
	 * dir 0: used = 0 means buf in use
	 * dir 1: used = 1 means buf in use
	 */
	if (port->buf[idx].used == dir) {
		/*
		 * To make it more robust, we could loop and get the
		 * next avail buf, its risky though
		 */
		pr_debug("%s:Next buf idx[0x%x] not available, dir[%d]\n",
		 __func__, idx, dir);
		return NULL;
	}
	*size = port->buf[idx].actual_size;
	*index = port->cpu_buf;
	data = port->buf[idx].data;
	pr_debug("%s:session[%d]index[%d] data[%p]size[%d]\n",
		__func__, ac->session, port->cpu_buf,
		data, *size);
	/*
	 * By default increase the cpu_buf cnt
	 * user accesses this function,increase cpu
	 * buf(to avoid another api)
	 */
	port->buf[idx].used = dir;
	port->cpu_buf = ((port->cpu_buf + 1) & (port->max_buf_cnt - 1));
	return data;
}

int q6asm_is_dsp_buf_avail(int dir, struct audio_client *ac)
{
	int ret = -1;
	struct audio_port_data *port;
	uint32_t idx;

	if (!ac || (dir != OUT))
		return ret;

	if (ac->io_mode & SYNC_IO_MODE) {
		port = &ac->port[dir];

		mutex_lock(&port->lock);
		idx = port->dsp_buf;

		if (port->buf[idx].used == (dir ^ 1)) {
			/* To make it more robust, we could loop and get the
			next avail buf, its risky though */
			pr_err("Next buf idx[0x%x] not available, dir[%d]\n",
								idx, dir);
			mutex_unlock(&port->lock);
			return ret;
		}
		pr_debug("%s: session[%d]dsp_buf=%d cpu_buf=%d\n", __func__,
			ac->session, port->dsp_buf, port->cpu_buf);
		ret = ((port->dsp_buf != port->cpu_buf) ? 0 : -1);
		mutex_unlock(&port->lock);
	}
	return ret;
}

static void __q6asm_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg, uint32_t stream_id)
{
	pr_debug("%s:pkt_size=%d cmd_flg=%d session=%d stream_id=%d\n",
		 __func__, pkt_size, cmd_flg, ac->session, stream_id);
	if (ac->apr == NULL) {
		pr_err("%s: ac->apr is NULL", __func__);
		return;
	}

	mutex_lock(&ac->cmd_lock);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(sizeof(struct apr_hdr)),\
				APR_PKT_VER);
	hdr->src_svc = ((struct apr_svc *)ac->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_ASM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = ((ac->session << 8) & 0xFF00) | (stream_id);
	hdr->dest_port = ((ac->session << 8) & 0xFF00) | (stream_id);
	if (cmd_flg) {
		hdr->token = ac->session;
	}
	hdr->pkt_size  = pkt_size;
	mutex_unlock(&ac->cmd_lock);
	return;
}

static void q6asm_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg)
{
	__q6asm_add_hdr(ac, hdr, pkt_size, cmd_flg, ac->stream_id);
	return;
}

static void q6asm_stream_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg, int32_t stream_id)
{
	__q6asm_add_hdr(ac, hdr, pkt_size, cmd_flg, stream_id);
	return;
}

static void __q6asm_add_hdr_async(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg, uint32_t stream_id)
{
	pr_debug("%s pkt_size = %d, cmd_flg = %d, session = %d stream_id=%d\n",
		__func__, pkt_size, cmd_flg, ac->session, stream_id);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(sizeof(struct apr_hdr)),\
				APR_PKT_VER);
	if (ac->apr == NULL) {
		pr_err("%s: ac->apr is NULL", __func__);
		return;
	}
	hdr->src_svc = ((struct apr_svc *)ac->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_ASM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = ((ac->session << 8) & 0xFF00) | (stream_id);
	hdr->dest_port = ((ac->session << 8) & 0xFF00) | (stream_id);
	if (cmd_flg) {
		hdr->token = ac->session;
	}
	hdr->pkt_size  = pkt_size;
	return;
}

static void q6asm_add_hdr_async(struct audio_client *ac, struct apr_hdr *hdr,
				uint32_t pkt_size, uint32_t cmd_flg)
{
	__q6asm_add_hdr_async(ac, hdr, pkt_size, cmd_flg, ac->stream_id);
	return;
}

static void q6asm_stream_add_hdr_async(struct audio_client *ac,
					struct apr_hdr *hdr, uint32_t pkt_size,
					uint32_t cmd_flg, int32_t stream_id)
{
	__q6asm_add_hdr_async(ac, hdr, pkt_size, cmd_flg, stream_id);
	return;
}

static void q6asm_add_hdr_custom_topology(struct audio_client *ac,
					  struct apr_hdr *hdr,
					  uint32_t pkt_size, uint32_t cmd_flg)
{
	pr_debug("%s:pkt_size=%d cmd_flg=%d session=%d\n", __func__, pkt_size,
		cmd_flg, ac->session);
	if (ac->apr == NULL) {
		pr_err("%s: ac->apr is NULL", __func__);
		return;
	}

	mutex_lock(&ac->cmd_lock);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(sizeof(struct apr_hdr)),\
				APR_PKT_VER);
	hdr->src_svc = ((struct apr_svc *)ac->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_ASM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = ((ac->session << 8) & 0xFF00) | 0x01;
	hdr->dest_port = 0;
	if (cmd_flg) {
		hdr->token = ((ac->session << 8) | 0x0001) ;
	}
	hdr->pkt_size  = pkt_size;
	mutex_unlock(&ac->cmd_lock);
	return;
}

static void q6asm_add_mmaphdr(struct audio_client *ac, struct apr_hdr *hdr,
			u32 pkt_size, u32 cmd_flg, u32 token)
{
	pr_debug("%s:pkt size=%d cmd_flg=%d\n", __func__, pkt_size, cmd_flg);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr->src_port = 0;
	hdr->dest_port = 0;
	if (cmd_flg) {
		hdr->token = token;
	}
	hdr->pkt_size  = pkt_size;
	return;
}
static int __q6asm_open_read(struct audio_client *ac,
		uint32_t format, uint16_t bits_per_sample)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_read_v3 open;

	config_debug_fs_reset_index();

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s:session[%d]", __func__, ac->session);

	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);
	atomic_set(&ac->cmd_state, 1);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_READ_V3;
	/* Stream prio : High, provide meta info with encoded frames */
	open.src_endpointype = ASM_END_POINT_DEVICE_MATRIX;

	open.preprocopo_id = get_asm_topology();
	if (open.preprocopo_id == 0)
		open.preprocopo_id = ASM_STREAM_POSTPROC_TOPO_ID_NONE;
	open.bits_per_sample = bits_per_sample;
	open.mode_flags = 0x0;

	if (ac->perf_mode == LOW_LATENCY_PCM_MODE) {
		open.mode_flags |= ASM_LOW_LATENCY_STREAM_SESSION <<
				ASM_SHIFT_STREAM_PERF_MODE_FLAG_IN_OPEN_READ;
	} else {
		open.mode_flags |= ASM_LEGACY_STREAM_SESSION <<
				ASM_SHIFT_STREAM_PERF_MODE_FLAG_IN_OPEN_READ;
	}

	switch (format) {
	case FORMAT_LINEAR_PCM:
		open.mode_flags |= 0x00;
		open.enc_cfg_id = ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2;
		break;
	case FORMAT_MPEG4_AAC:
		open.mode_flags |= BUFFER_META_ENABLE;
		open.enc_cfg_id = ASM_MEDIA_FMT_AAC_V2;
		break;
	case FORMAT_V13K:
		open.mode_flags |= BUFFER_META_ENABLE;
		open.enc_cfg_id = ASM_MEDIA_FMT_V13K_FS;
		break;
	case FORMAT_EVRC:
		open.mode_flags |= BUFFER_META_ENABLE;
		open.enc_cfg_id = ASM_MEDIA_FMT_EVRC_FS;
		break;
	case FORMAT_AMRNB:
		open.mode_flags |= BUFFER_META_ENABLE ;
		open.enc_cfg_id = ASM_MEDIA_FMT_AMRNB_FS;
		break;
	case FORMAT_AMRWB:
		open.mode_flags |= BUFFER_META_ENABLE ;
		open.enc_cfg_id = ASM_MEDIA_FMT_AMRWB_FS;
		break;
	default:
		pr_err("Invalid format[%d]\n", format);
		goto fail_cmd;
	}
	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("open failed op[0x%x]rc[%d]\n", \
						open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout. waited for open read rc[%d]\n", __func__,
			rc);
		goto fail_cmd;
	}
	ac->io_mode |= TUN_READ_IO_MODE;
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_open_read(struct audio_client *ac,
		uint32_t format)
{
	return __q6asm_open_read(ac, format, 16);
}

int q6asm_open_read_v2(struct audio_client *ac, uint32_t format,
			uint16_t bits_per_sample)
{
	return __q6asm_open_read(ac, format, bits_per_sample);
}

static int __q6asm_open_write(struct audio_client *ac, uint32_t format,
				uint16_t bits_per_sample, uint32_t stream_id,
				bool is_gapless_mode)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_write_v3 open;

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: session[%d] wr_format[0x%x]", __func__, ac->session,
		format);

	q6asm_stream_add_hdr(ac, &open.hdr, sizeof(open), TRUE, stream_id);
	atomic_set(&ac->cmd_state, 1);
	/*
	 * Updated the token field with stream/session for compressed playback
	 * Platform driver must know the the stream with which the command is
	 * associated
	 */
	if (ac->io_mode & COMPRESSED_STREAM_IO)
		open.hdr.token = ((ac->session << 8) & 0xFFFF00) |
				 (stream_id & 0xFF);

	pr_debug("%s: token = 0x%x, stream_id  %d, session 0x%x\n",
		 __func__, open.hdr.token, stream_id, ac->session);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_WRITE_V3;
	open.mode_flags = 0x00;
	if (ac->perf_mode == ULTRA_LOW_LATENCY_PCM_MODE)
		open.mode_flags |= ASM_ULTRA_LOW_LATENCY_STREAM_SESSION;
	else if (ac->perf_mode == LOW_LATENCY_PCM_MODE)
		open.mode_flags |= ASM_LOW_LATENCY_STREAM_SESSION;
	else {
		open.mode_flags |= ASM_LEGACY_STREAM_SESSION;
		if (is_gapless_mode)
			open.mode_flags |= 1 << ASM_SHIFT_GAPLESS_MODE_FLAG;
	}

	/* source endpoint : matrix */
	open.sink_endpointype = ASM_END_POINT_DEVICE_MATRIX;
	open.bits_per_sample = bits_per_sample;

	open.postprocopo_id = get_asm_topology();
	if (open.postprocopo_id == 0)
		open.postprocopo_id = ASM_STREAM_POSTPROC_TOPO_ID_NONE;

	switch (format) {
	case FORMAT_LINEAR_PCM:
		open.dec_fmt_id = ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2;
		break;
	case FORMAT_MPEG4_AAC:
		open.dec_fmt_id = ASM_MEDIA_FMT_AAC_V2;
		break;
	case FORMAT_MPEG4_MULTI_AAC:
		open.dec_fmt_id = ASM_MEDIA_FMT_DOLBY_AAC;
		break;
	case FORMAT_WMA_V9:
		open.dec_fmt_id = ASM_MEDIA_FMT_WMA_V9_V2;
		break;
	case FORMAT_WMA_V10PRO:
		open.dec_fmt_id = ASM_MEDIA_FMT_WMA_V10PRO_V2;
		break;
	case FORMAT_MP3:
		open.dec_fmt_id = ASM_MEDIA_FMT_MP3;
		break;
	case FORMAT_AC3:
		open.dec_fmt_id = ASM_MEDIA_FMT_EAC3_DEC;
		break;
	case FORMAT_EAC3:
		open.dec_fmt_id = ASM_MEDIA_FMT_EAC3_DEC;
		break;
	default:
		pr_err("%s: Invalid format[%d]\n", __func__, format);
		goto fail_cmd;
	}
	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("%s: open failed op[0x%x]rc[%d]\n", \
					__func__, open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout. waited for open write rc[%d]\n", __func__,
			rc);
		goto fail_cmd;
	}
	ac->io_mode |= TUN_WRITE_IO_MODE;
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_open_write(struct audio_client *ac, uint32_t format)
{
	return __q6asm_open_write(ac, format, 16, ac->stream_id,
					 false /*gapless*/);
}

int q6asm_open_write_v2(struct audio_client *ac, uint32_t format,
		uint16_t bits_per_sample)
{
	return __q6asm_open_write(ac, format, bits_per_sample,
					 ac->stream_id, false /*gapless*/);
}

int q6asm_stream_open_write_v2(struct audio_client *ac, uint32_t format,
				uint16_t bits_per_sample, int32_t stream_id,
				bool is_gapless_mode)
{
	return __q6asm_open_write(ac, format, bits_per_sample,
					 stream_id, is_gapless_mode);
}

int q6asm_open_read_write(struct audio_client *ac,
			uint32_t rd_format,
			uint32_t wr_format)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_readwrite_v2 open;

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: session[%d]", __func__, ac->session);
	pr_debug("wr_format[0x%x]rd_format[0x%x]",
				wr_format, rd_format);

	ac->io_mode |= NT_MODE;
	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);
	atomic_set(&ac->cmd_state, 1);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_READWRITE_V2;

	open.mode_flags = BUFFER_META_ENABLE;
	open.bits_per_sample = 16;
	/* source endpoint : matrix */
	open.postprocopo_id = get_asm_topology();
	if (open.postprocopo_id == 0)
		open.postprocopo_id = ASM_STREAM_POSTPROC_TOPO_ID_NONE;

	switch (wr_format) {
	case FORMAT_LINEAR_PCM:
		open.dec_fmt_id = ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2;
		break;
	case FORMAT_MPEG4_AAC:
		open.dec_fmt_id = ASM_MEDIA_FMT_AAC_V2;
		break;
	case FORMAT_MPEG4_MULTI_AAC:
		open.dec_fmt_id = ASM_MEDIA_FMT_DOLBY_AAC;
		break;
	case FORMAT_WMA_V9:
		open.dec_fmt_id = ASM_MEDIA_FMT_WMA_V9_V2;
		break;
	case FORMAT_WMA_V10PRO:
		open.dec_fmt_id = ASM_MEDIA_FMT_WMA_V10PRO_V2;
		break;
	case FORMAT_AMRNB:
		open.dec_fmt_id = ASM_MEDIA_FMT_AMRNB_FS;
		break;
	case FORMAT_AMRWB:
		open.dec_fmt_id = ASM_MEDIA_FMT_AMRWB_FS;
		break;
	case FORMAT_AMR_WB_PLUS:
		open.dec_fmt_id = ASM_MEDIA_FMT_AMR_WB_PLUS_V2;
		break;
	case FORMAT_V13K:
		open.dec_fmt_id = ASM_MEDIA_FMT_V13K_FS;
		break;
	case FORMAT_EVRC:
		open.dec_fmt_id = ASM_MEDIA_FMT_EVRC_FS;
		break;
	case FORMAT_EVRCB:
		open.dec_fmt_id = ASM_MEDIA_FMT_EVRCB_FS;
		break;
	case FORMAT_EVRCWB:
		open.dec_fmt_id = ASM_MEDIA_FMT_EVRCWB_FS;
		break;
	case FORMAT_MP3:
		open.dec_fmt_id = ASM_MEDIA_FMT_MP3;
		break;
	default:
		pr_err("Invalid format[%d]\n", wr_format);
		goto fail_cmd;
	}

	switch (rd_format) {
	case FORMAT_LINEAR_PCM:
		open.enc_cfg_id = ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2;
		break;
	case FORMAT_MPEG4_AAC:
		open.enc_cfg_id = ASM_MEDIA_FMT_AAC_V2;
		break;
	case FORMAT_V13K:
		open.enc_cfg_id = ASM_MEDIA_FMT_V13K_FS;
		break;
	case FORMAT_EVRC:
		open.enc_cfg_id = ASM_MEDIA_FMT_EVRC_FS;
		break;
	case FORMAT_AMRNB:
		open.enc_cfg_id = ASM_MEDIA_FMT_AMRNB_FS;
		break;
	case FORMAT_AMRWB:
		open.enc_cfg_id = ASM_MEDIA_FMT_AMRWB_FS;
		break;
	default:
		pr_err("Invalid format[%d]\n", rd_format);
		goto fail_cmd;
	}
	pr_debug("%s:rdformat[0x%x]wrformat[0x%x]\n", __func__,
			open.enc_cfg_id, open.dec_fmt_id);

	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("open failed op[0x%x]rc[%d]\n", \
						open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for open read-write rc[%d]\n", rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_open_loopback_v2(struct audio_client *ac, uint16_t bits_per_sample)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_loopback_v2 open;

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("%s APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: session[%d]", __func__, ac->session);

	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);
	atomic_set(&ac->cmd_state, 1);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_LOOPBACK_V2;

	open.mode_flags = 0;
	open.src_endpointype = 0;
	open.sink_endpointype = 0;
	/* source endpoint : matrix */
	open.postprocopo_id = get_asm_topology();
	if (open.postprocopo_id == 0)
		open.postprocopo_id = DEFAULT_POPP_TOPOLOGY;
	open.bits_per_sample = bits_per_sample;
	open.reserved = 0;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("%s open failed op[0x%x]rc[%d]\n", __func__,
				open.hdr.opcode, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s timeout. waited for open_loopback rc[%d]\n",
				__func__, rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_run(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts)
{
	struct asm_session_cmd_run_v2 run;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s session[%d]", __func__, ac->session);
	q6asm_add_hdr(ac, &run.hdr, sizeof(run), TRUE);
	atomic_set(&ac->cmd_state, 1);

	run.hdr.opcode = ASM_SESSION_CMD_RUN_V2;
	run.flags    = flags;
	run.time_lsw = lsw_ts;
	run.time_msw = msw_ts;

	config_debug_fs_run();

	rc = apr_send_pkt(ac->apr, (uint32_t *) &run);
	if (rc < 0) {
		pr_err("Commmand run failed[%d]", rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for run success rc[%d]", rc);
		goto fail_cmd;
	}

	return 0;
fail_cmd:
	return -EINVAL;
}

static int __q6asm_run_nowait(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts, uint32_t stream_id)
{
	struct asm_session_cmd_run_v2 run;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("%s:APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("session[%d]", ac->session);
	q6asm_stream_add_hdr_async(ac, &run.hdr, sizeof(run), TRUE, stream_id);
	atomic_set(&ac->cmd_state, 1);
	run.hdr.opcode = ASM_SESSION_CMD_RUN_V2;
	run.flags    = flags;
	run.time_lsw = lsw_ts;
	run.time_msw = msw_ts;

	/* have to increase first avoid race */
	atomic_inc(&ac->nowait_cmd_cnt);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &run);
	if (rc < 0) {
		atomic_dec(&ac->nowait_cmd_cnt);
		pr_err("%s:Commmand run failed[%d]", __func__, rc);
		return -EINVAL;
	}
	return 0;
}

int q6asm_run_nowait(struct audio_client *ac, uint32_t flags,
			uint32_t msw_ts, uint32_t lsw_ts)
{
	return __q6asm_run_nowait(ac, flags, msw_ts, lsw_ts, ac->stream_id);
}

int q6asm_stream_run_nowait(struct audio_client *ac, uint32_t flags,
			uint32_t msw_ts, uint32_t lsw_ts, uint32_t stream_id)
{
	return __q6asm_run_nowait(ac, flags, msw_ts, lsw_ts, stream_id);
}

int q6asm_enc_cfg_blk_aac(struct audio_client *ac,
			 uint32_t frames_per_buf,
			uint32_t sample_rate, uint32_t channels,
			uint32_t bit_rate, uint32_t mode, uint32_t format)
{
	struct asm_aac_enc_cfg_v2 enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]SR[%d]ch[%d]bitrate[%d]mode[%d] format[%d]",
		 __func__, ac->session, frames_per_buf,
		sample_rate, channels, bit_rate, mode, format);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.encdec.param_id = ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2;
	enc_cfg.encdec.param_size = sizeof(struct asm_aac_enc_cfg_v2) -
				sizeof(struct asm_stream_cmd_set_encdec_param);
	enc_cfg.encblk.frames_per_buf = frames_per_buf;
	enc_cfg.encblk.enc_cfg_blk_size  = enc_cfg.encdec.param_size -
				sizeof(struct asm_enc_cfg_blk_param_v2);
	enc_cfg.bit_rate = bit_rate;
	enc_cfg.enc_mode = mode;
	enc_cfg.aac_fmt_flag = format;
	enc_cfg.channel_cfg = channels;
	enc_cfg.sample_rate = sample_rate;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for FORMAT_UPDATE\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_set_encdec_chan_map(struct audio_client *ac,
			uint32_t num_channels)
{
	struct asm_dec_out_chan_map_param chan_map;
	u8 *channel_mapping;
	int rc = 0;
	pr_debug("%s: Session %d, num_channels = %d\n",
			 __func__, ac->session, num_channels);
	q6asm_add_hdr(ac, &chan_map.hdr, sizeof(chan_map), TRUE);
	atomic_set(&ac->cmd_state, 1);
	chan_map.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	chan_map.encdec.param_id = ASM_PARAM_ID_DEC_OUTPUT_CHAN_MAP;
	chan_map.encdec.param_size = sizeof(struct asm_dec_out_chan_map_param) -
			 (sizeof(struct apr_hdr) +
			 sizeof(struct asm_stream_cmd_set_encdec_param));
	chan_map.num_channels = num_channels;
	channel_mapping = chan_map.channel_mapping;
	memset(channel_mapping, PCM_CHANNEL_NULL, MAX_CHAN_MAP_CHANNELS);
	if (q6asm_map_channels(channel_mapping, num_channels))
		return -EINVAL;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &chan_map);
	if (rc < 0) {
		pr_err("%s:Command opcode[0x%x]paramid[0x%x] failed\n",
			   __func__, ASM_STREAM_CMD_SET_ENCDEC_PARAM,
			   ASM_PARAM_ID_DEC_OUTPUT_CHAN_MAP);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
				 (atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout opcode[0x%x]\n", __func__,
			   chan_map.hdr.opcode);
		rc = -ETIMEDOUT;
		goto fail_cmd;
	}
	return 0;
fail_cmd:
		return rc;
}

static int __q6asm_enc_cfg_blk_pcm(struct audio_client *ac,
		uint32_t rate, uint32_t channels, uint16_t bits_per_sample)
{
	struct asm_multi_channel_pcm_enc_cfg_v2  enc_cfg;
	u8 *channel_mapping;
	u32 frames_per_buf = 0;

	int rc = 0;

	pr_debug("%s: Session %d, rate = %d, channels = %d\n", __func__,
			 ac->session, rate, channels);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);
	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.encdec.param_id = ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2;
	enc_cfg.encdec.param_size = sizeof(enc_cfg) - sizeof(enc_cfg.hdr) -
				sizeof(enc_cfg.encdec);
	enc_cfg.encblk.frames_per_buf = frames_per_buf;
	enc_cfg.encblk.enc_cfg_blk_size  = enc_cfg.encdec.param_size -
					sizeof(struct asm_enc_cfg_blk_param_v2);

	enc_cfg.num_channels = channels;
	enc_cfg.bits_per_sample = bits_per_sample;
	enc_cfg.sample_rate = rate;
	enc_cfg.is_signed = 1;
	channel_mapping = enc_cfg.channel_mapping;

	memset(channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);

	if (q6asm_map_channels(channel_mapping, channels))
		return -EINVAL;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd open failed\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout opcode[0x%x] ", enc_cfg.hdr.opcode);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_pcm(struct audio_client *ac,
			uint32_t rate, uint32_t channels)
{
	return __q6asm_enc_cfg_blk_pcm(ac, rate, channels, 16);
}

int q6asm_enc_cfg_blk_pcm_format_support(struct audio_client *ac,
		uint32_t rate, uint32_t channels, uint16_t bits_per_sample)
{
	 return __q6asm_enc_cfg_blk_pcm(ac, rate, channels, bits_per_sample);
}

int q6asm_enc_cfg_blk_pcm_native(struct audio_client *ac,
			uint32_t rate, uint32_t channels)
{
	struct asm_multi_channel_pcm_enc_cfg_v2  enc_cfg;
	u8 *channel_mapping;
	u32 frames_per_buf = 0;

	int rc = 0;

	pr_debug("%s: Session %d, rate = %d, channels = %d\n", __func__,
			 ac->session, rate, channels);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);
	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.encdec.param_id = ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2;
	enc_cfg.encdec.param_size = sizeof(enc_cfg) - sizeof(enc_cfg.hdr) -
				 sizeof(enc_cfg.encdec);
	enc_cfg.encblk.frames_per_buf = frames_per_buf;
	enc_cfg.encblk.enc_cfg_blk_size  = enc_cfg.encdec.param_size -
				sizeof(struct asm_enc_cfg_blk_param_v2);

	enc_cfg.num_channels = 0;/*channels;*/
	enc_cfg.bits_per_sample = 16;
	enc_cfg.sample_rate = 0;/*rate;*/
	enc_cfg.is_signed = 1;
	channel_mapping = enc_cfg.channel_mapping;


	memset(channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);

	if (q6asm_map_channels(channel_mapping, channels))
		return -EINVAL;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd open failed\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout opcode[0x%x] ", enc_cfg.hdr.opcode);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

static int q6asm_map_channels(u8 *channel_mapping, uint32_t channels)
{
	u8 *lchannel_mapping;
	lchannel_mapping = channel_mapping;
	pr_debug("%s channels passed: %d\n", __func__, channels);
	if (channels == 1)  {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
	} else if (channels == 2) {
		lchannel_mapping[0] = PCM_CHANNEL_FL;
		lchannel_mapping[1] = PCM_CHANNEL_FR;
	} else if (channels == 3) {
		lchannel_mapping[0] = PCM_CHANNEL_FL;
		lchannel_mapping[1] = PCM_CHANNEL_FR;
		lchannel_mapping[2] = PCM_CHANNEL_FC;
	} else if (channels == 4) {
		lchannel_mapping[0] = PCM_CHANNEL_FL;
		lchannel_mapping[1] = PCM_CHANNEL_FR;
		lchannel_mapping[2] = PCM_CHANNEL_RB;
		lchannel_mapping[3] = PCM_CHANNEL_LB;
	} else if (channels == 5) {
		lchannel_mapping[0] = PCM_CHANNEL_FL;
		lchannel_mapping[1] = PCM_CHANNEL_FR;
		lchannel_mapping[2] = PCM_CHANNEL_FC;
		lchannel_mapping[3] = PCM_CHANNEL_LB;
		lchannel_mapping[4] = PCM_CHANNEL_RB;
	} else if (channels == 6) {
		lchannel_mapping[0] = PCM_CHANNEL_FL;
		lchannel_mapping[1] = PCM_CHANNEL_FR;
		lchannel_mapping[2] = PCM_CHANNEL_FC;
		lchannel_mapping[3] = PCM_CHANNEL_LFE;
		lchannel_mapping[4] = PCM_CHANNEL_LS;
		lchannel_mapping[5] = PCM_CHANNEL_RS;
	} else if (channels == 8) {
		lchannel_mapping[0] = PCM_CHANNEL_FL;
		lchannel_mapping[1] = PCM_CHANNEL_FR;
		lchannel_mapping[2] = PCM_CHANNEL_FC;
		lchannel_mapping[3] = PCM_CHANNEL_LFE;
		lchannel_mapping[4] = PCM_CHANNEL_LB;
		lchannel_mapping[5] = PCM_CHANNEL_RB;
		lchannel_mapping[6] = PCM_CHANNEL_FLC;
		lchannel_mapping[7] = PCM_CHANNEL_FRC;
	} else {
		pr_err("%s: ERROR.unsupported num_ch = %u\n",
		 __func__, channels);
		return -EINVAL;
	}
	return 0;

}

int q6asm_enable_sbrps(struct audio_client *ac,
			uint32_t sbr_ps_enable)
{
	struct asm_aac_sbr_ps_flag_param  sbrps;
	u32 frames_per_buf = 0;

	int rc = 0;

	pr_debug("%s: Session %d\n", __func__, ac->session);

	q6asm_add_hdr(ac, &sbrps.hdr, sizeof(sbrps), TRUE);
	atomic_set(&ac->cmd_state, 1);

	sbrps.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	sbrps.encdec.param_id = ASM_PARAM_ID_AAC_SBR_PS_FLAG;
	sbrps.encdec.param_size = sizeof(struct asm_aac_sbr_ps_flag_param) -
				sizeof(struct asm_stream_cmd_set_encdec_param);
	sbrps.encblk.frames_per_buf = frames_per_buf;
	sbrps.encblk.enc_cfg_blk_size  = sbrps.encdec.param_size -
				sizeof(struct asm_enc_cfg_blk_param_v2);

	sbrps.sbr_ps_flag = sbr_ps_enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &sbrps);
	if (rc < 0) {
		pr_err("Command opcode[0x%x]paramid[0x%x] failed\n",
				ASM_STREAM_CMD_SET_ENCDEC_PARAM,
				ASM_PARAM_ID_AAC_SBR_PS_FLAG);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout opcode[0x%x] ", sbrps.hdr.opcode);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_cfg_dual_mono_aac(struct audio_client *ac,
			uint16_t sce_left, uint16_t sce_right)
{
	struct asm_aac_dual_mono_mapping_param dual_mono;

	int rc = 0;

	pr_debug("%s: Session %d, sce_left = %d, sce_right = %d\n",
			 __func__, ac->session, sce_left, sce_right);

	q6asm_add_hdr(ac, &dual_mono.hdr, sizeof(dual_mono), TRUE);
	atomic_set(&ac->cmd_state, 1);

	dual_mono.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	dual_mono.encdec.param_id = ASM_PARAM_ID_AAC_DUAL_MONO_MAPPING;
	dual_mono.encdec.param_size = sizeof(dual_mono.left_channel_sce) +
				sizeof(dual_mono.right_channel_sce);
	dual_mono.left_channel_sce = sce_left;
	dual_mono.right_channel_sce = sce_right;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &dual_mono);
	if (rc < 0) {
		pr_err("%s:Command opcode[0x%x]paramid[0x%x] failed\n",
				__func__, ASM_STREAM_CMD_SET_ENCDEC_PARAM,
				ASM_PARAM_ID_AAC_DUAL_MONO_MAPPING);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout opcode[0x%x]\n", __func__,
						dual_mono.hdr.opcode);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

/* Support for selecting stereo mixing coefficients for B family not done */
int q6asm_cfg_aac_sel_mix_coef(struct audio_client *ac, uint32_t mix_coeff)
{
	struct asm_aac_stereo_mix_coeff_selection_param_v2 aac_mix_coeff;
	int rc = 0;

	q6asm_add_hdr(ac, &aac_mix_coeff.hdr, sizeof(aac_mix_coeff), TRUE);
	atomic_set(&ac->cmd_state, 1);
	aac_mix_coeff.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	aac_mix_coeff.param_id =
		ASM_PARAM_ID_AAC_STEREO_MIX_COEFF_SELECTION_FLAG_V2;
	aac_mix_coeff.param_size =
		sizeof(struct asm_aac_stereo_mix_coeff_selection_param_v2);
	aac_mix_coeff.aac_stereo_mix_coeff_flag = mix_coeff;
	pr_debug("%s, mix_coeff = %u", __func__, mix_coeff);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &aac_mix_coeff);
	if (rc < 0) {
		pr_err("%s:Command opcode[0x%x]paramid[0x%x] failed\n",
			__func__, ASM_STREAM_CMD_SET_ENCDEC_PARAM,
			ASM_PARAM_ID_AAC_STEREO_MIX_COEFF_SELECTION_FLAG_V2);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
		(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout opcode[0x%x]\n",
			__func__, aac_mix_coeff.hdr.opcode);
		rc = -ETIMEDOUT;
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return rc;
}

int q6asm_enc_cfg_blk_qcelp(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t reduced_rate_level, uint16_t rate_modulation_cmd)
{
	struct asm_v13k_enc_cfg enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]min_rate[0x%4x]max_rate[0x%4x] reduced_rate_level[0x%4x]rate_modulation_cmd[0x%4x]",
		 __func__,
		ac->session, frames_per_buf, min_rate, max_rate,
		reduced_rate_level, rate_modulation_cmd);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);
	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.encdec.param_id = ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2;
	enc_cfg.encdec.param_size = sizeof(struct asm_v13k_enc_cfg) -
				sizeof(struct asm_stream_cmd_set_encdec_param);
	enc_cfg.encblk.frames_per_buf = frames_per_buf;
	enc_cfg.encblk.enc_cfg_blk_size  = enc_cfg.encdec.param_size -
				sizeof(struct asm_enc_cfg_blk_param_v2);

	enc_cfg.min_rate = min_rate;
	enc_cfg.max_rate = max_rate;
	enc_cfg.reduced_rate_cmd = reduced_rate_level;
	enc_cfg.rate_mod_cmd = rate_modulation_cmd;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for setencdec v13k resp\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_evrc(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t rate_modulation_cmd)
{
	struct asm_evrc_enc_cfg enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]min_rate[0x%4x]max_rate[0x%4x] rate_modulation_cmd[0x%4x]",
		 __func__, ac->session,
		frames_per_buf,	min_rate, max_rate, rate_modulation_cmd);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);
	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.encdec.param_id = ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2;
	enc_cfg.encdec.param_size = sizeof(struct asm_evrc_enc_cfg) -
				sizeof(struct asm_stream_cmd_set_encdec_param);
	enc_cfg.encblk.frames_per_buf = frames_per_buf;
	enc_cfg.encblk.enc_cfg_blk_size  = enc_cfg.encdec.param_size -
				sizeof(struct asm_enc_cfg_blk_param_v2);

	enc_cfg.min_rate = min_rate;
	enc_cfg.max_rate = max_rate;
	enc_cfg.rate_mod_cmd = rate_modulation_cmd;
	enc_cfg.reserved = 0;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for encdec evrc\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_amrnb(struct audio_client *ac, uint32_t frames_per_buf,
			uint16_t band_mode, uint16_t dtx_enable)
{
	struct asm_amrnb_enc_cfg enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]band_mode[0x%4x]dtx_enable[0x%4x]",
		__func__, ac->session, frames_per_buf, band_mode, dtx_enable);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);
	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.encdec.param_id = ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2;
	enc_cfg.encdec.param_size = sizeof(struct asm_amrnb_enc_cfg) -
				sizeof(struct asm_stream_cmd_set_encdec_param);
	enc_cfg.encblk.frames_per_buf = frames_per_buf;
	enc_cfg.encblk.enc_cfg_blk_size  = enc_cfg.encdec.param_size -
				sizeof(struct asm_enc_cfg_blk_param_v2);

	enc_cfg.enc_mode = band_mode;
	enc_cfg.dtx_mode = dtx_enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for set encdec amrnb\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_amrwb(struct audio_client *ac, uint32_t frames_per_buf,
			uint16_t band_mode, uint16_t dtx_enable)
{
	struct asm_amrwb_enc_cfg enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]band_mode[0x%4x]dtx_enable[0x%4x]",
		__func__, ac->session, frames_per_buf, band_mode, dtx_enable);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);
	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.encdec.param_id = ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2;
	enc_cfg.encdec.param_size = sizeof(struct asm_amrwb_enc_cfg) -
				sizeof(struct asm_stream_cmd_set_encdec_param);
	enc_cfg.encblk.frames_per_buf = frames_per_buf;
	enc_cfg.encblk.enc_cfg_blk_size  = enc_cfg.encdec.param_size -
				sizeof(struct asm_enc_cfg_blk_param_v2);

	enc_cfg.enc_mode = band_mode;
	enc_cfg.dtx_mode = dtx_enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for FORMAT_UPDATE\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}


static int __q6asm_media_format_block_pcm(struct audio_client *ac,
				uint32_t rate, uint32_t channels,
				uint16_t bits_per_sample)
{
	struct asm_multi_channel_pcm_fmt_blk_v2 fmt;
	u8 *channel_mapping;
	int rc = 0;

	pr_debug("%s:session[%d]rate[%d]ch[%d]\n", __func__, ac->session, rate,
		channels);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);
	atomic_set(&ac->cmd_state, 1);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2;
	fmt.fmt_blk.fmt_blk_size = sizeof(fmt) - sizeof(fmt.hdr) -
					sizeof(fmt.fmt_blk);
	fmt.num_channels = channels;
	fmt.bits_per_sample = bits_per_sample;
	fmt.sample_rate = rate;
	fmt.is_signed = 1;

	channel_mapping = fmt.channel_mapping;

	memset(channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);

	if (q6asm_map_channels(channel_mapping, channels))
		return -EINVAL;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &fmt);
	if (rc < 0) {
		pr_err("%s:Comamnd open failed\n", __func__);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout. waited for format update\n", __func__);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_media_format_block_pcm(struct audio_client *ac,
				uint32_t rate, uint32_t channels)
{
	return __q6asm_media_format_block_pcm(ac, rate,
				channels, 16);
}

int q6asm_media_format_block_pcm_format_support(struct audio_client *ac,
				uint32_t rate, uint32_t channels,
				uint16_t bits_per_sample)
{
	return __q6asm_media_format_block_pcm(ac, rate,
				channels, bits_per_sample);
}

static int __q6asm_media_format_block_multi_ch_pcm(struct audio_client *ac,
				uint32_t rate, uint32_t channels,
				bool use_default_chmap, char *channel_map,
				uint16_t bits_per_sample)
{
	struct asm_multi_channel_pcm_fmt_blk_v2 fmt;
	u8 *channel_mapping;
	int rc = 0;

	pr_debug("%s:session[%d]rate[%d]ch[%d]\n", __func__, ac->session, rate,
		channels);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);
	atomic_set(&ac->cmd_state, 1);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2;
	fmt.fmt_blk.fmt_blk_size = sizeof(fmt) - sizeof(fmt.hdr) -
					sizeof(fmt.fmt_blk);
	fmt.num_channels = channels;
	fmt.bits_per_sample = bits_per_sample;
	fmt.sample_rate = rate;
	fmt.is_signed = 1;

	channel_mapping = fmt.channel_mapping;

	memset(channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);

	if (use_default_chmap) {
		if (q6asm_map_channels(channel_mapping, channels)) {
			pr_err("%s: map channels failed", __func__);
			return -EINVAL;
		}
	} else {
		memcpy(channel_mapping, channel_map,
			 PCM_FORMAT_MAX_NUM_CHANNEL);
	}

	rc = apr_send_pkt(ac->apr, (uint32_t *) &fmt);
	if (rc < 0) {
		pr_err("%s:Comamnd open failed\n", __func__);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout. waited for format update\n", __func__);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_media_format_block_multi_ch_pcm(struct audio_client *ac,
		uint32_t rate, uint32_t channels,
		bool use_default_chmap, char *channel_map)
{
	return __q6asm_media_format_block_multi_ch_pcm(ac, rate,
			channels, use_default_chmap, channel_map, 16);
}

int q6asm_media_format_block_multi_ch_pcm_v2(
		struct audio_client *ac,
		uint32_t rate, uint32_t channels,
		bool use_default_chmap, char *channel_map,
		uint16_t bits_per_sample)
{
	return __q6asm_media_format_block_multi_ch_pcm(ac, rate,
			channels, use_default_chmap, channel_map,
			bits_per_sample);
}

static int __q6asm_media_format_block_multi_aac(struct audio_client *ac,
				struct asm_aac_cfg *cfg, int stream_id)
{
	struct asm_aac_fmt_blk_v2 fmt;
	int rc = 0;

	pr_debug("%s:session[%d]rate[%d]ch[%d]\n", __func__, ac->session,
		cfg->sample_rate, cfg->ch_cfg);

	q6asm_stream_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE, stream_id);
	atomic_set(&ac->cmd_state, 1);
	/*
	 * Updated the token field with stream/session for compressed playback
	 * Platform driver must know the the stream with which the command is
	 * associated
	 */
	if (ac->io_mode & COMPRESSED_STREAM_IO)
		fmt.hdr.token = ((ac->session << 8) & 0xFFFF00) |
				(stream_id & 0xFF);

	pr_debug("%s: token = 0x%x, stream_id  %d, session 0x%x\n",
		  __func__, fmt.hdr.token, stream_id, ac->session);
	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2;
	fmt.fmt_blk.fmt_blk_size = sizeof(fmt) - sizeof(fmt.hdr) -
					sizeof(fmt.fmt_blk);
	fmt.aac_fmt_flag = cfg->format;
	fmt.audio_objype = cfg->aot;
	/* If zero, PCE is assumed to be available in bitstream*/
	fmt.total_size_of_PCE_bits = 0;
	fmt.channel_config = cfg->ch_cfg;
	fmt.sample_rate = cfg->sample_rate;

	pr_info("%s:format=%x cfg_size=%d aac-cfg=%x aot=%d ch=%d sr=%d\n",
			__func__, fmt.aac_fmt_flag, fmt.fmt_blk.fmt_blk_size,
			fmt.aac_fmt_flag,
			fmt.audio_objype,
			fmt.channel_config,
			fmt.sample_rate);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &fmt);
	if (rc < 0) {
		pr_err("%s:Comamnd open failed\n", __func__);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout. waited for FORMAT_UPDATE\n", __func__);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_media_format_block_multi_aac(struct audio_client *ac,
				struct asm_aac_cfg *cfg)
{
	return __q6asm_media_format_block_multi_aac(ac, cfg, ac->stream_id);
}

int q6asm_media_format_block_aac(struct audio_client *ac,
			struct asm_aac_cfg *cfg)
{
	return __q6asm_media_format_block_multi_aac(ac, cfg, ac->stream_id);
}

int q6asm_stream_media_format_block_aac(struct audio_client *ac,
			struct asm_aac_cfg *cfg, int stream_id)
{
	return __q6asm_media_format_block_multi_aac(ac, cfg, stream_id);
}

int q6asm_media_format_block_wma(struct audio_client *ac,
				void *cfg)
{
	struct asm_wmastdv9_fmt_blk_v2 fmt;
	struct asm_wma_cfg *wma_cfg = (struct asm_wma_cfg *)cfg;
	int rc = 0;

	pr_debug("session[%d]format_tag[0x%4x] rate[%d] ch[0x%4x] bps[%d], balign[0x%4x], bit_sample[0x%4x], ch_msk[%d], enc_opt[0x%4x]\n",
		ac->session, wma_cfg->format_tag, wma_cfg->sample_rate,
		wma_cfg->ch_cfg, wma_cfg->avg_bytes_per_sec,
		wma_cfg->block_align, wma_cfg->valid_bits_per_sample,
		wma_cfg->ch_mask, wma_cfg->encode_opt);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);
	atomic_set(&ac->cmd_state, 1);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2;
	fmt.fmtblk.fmt_blk_size = sizeof(fmt) - sizeof(fmt.hdr) -
					sizeof(fmt.fmtblk);
	fmt.fmtag = wma_cfg->format_tag;
	fmt.num_channels = wma_cfg->ch_cfg;
	fmt.sample_rate = wma_cfg->sample_rate;
	fmt.avg_bytes_per_sec = wma_cfg->avg_bytes_per_sec;
	fmt.blk_align = wma_cfg->block_align;
	fmt.bits_per_sample =
			wma_cfg->valid_bits_per_sample;
	fmt.channel_mask = wma_cfg->ch_mask;
	fmt.enc_options = wma_cfg->encode_opt;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &fmt);
	if (rc < 0) {
		pr_err("%s:Comamnd open failed\n", __func__);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout. waited for FORMAT_UPDATE\n", __func__);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_media_format_block_wmapro(struct audio_client *ac,
				void *cfg)
{
	struct asm_wmaprov10_fmt_blk_v2 fmt;
	struct asm_wmapro_cfg *wmapro_cfg = (struct asm_wmapro_cfg *)cfg;
	int rc = 0;

	pr_debug("session[%d]format_tag[0x%4x] rate[%d] ch[0x%4x] bps[%d], balign[0x%4x], bit_sample[0x%4x], ch_msk[%d], enc_opt[0x%4x], adv_enc_opt[0x%4x], adv_enc_opt2[0x%8x]\n",
		ac->session, wmapro_cfg->format_tag, wmapro_cfg->sample_rate,
		wmapro_cfg->ch_cfg,  wmapro_cfg->avg_bytes_per_sec,
		wmapro_cfg->block_align, wmapro_cfg->valid_bits_per_sample,
		wmapro_cfg->ch_mask, wmapro_cfg->encode_opt,
		wmapro_cfg->adv_encode_opt, wmapro_cfg->adv_encode_opt2);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);
	atomic_set(&ac->cmd_state, 1);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2;
	fmt.fmtblk.fmt_blk_size = sizeof(fmt) - sizeof(fmt.hdr) -
						sizeof(fmt.fmtblk);

	fmt.fmtag = wmapro_cfg->format_tag;
	fmt.num_channels = wmapro_cfg->ch_cfg;
	fmt.sample_rate = wmapro_cfg->sample_rate;
	fmt.avg_bytes_per_sec =
				wmapro_cfg->avg_bytes_per_sec;
	fmt.blk_align = wmapro_cfg->block_align;
	fmt.bits_per_sample = wmapro_cfg->valid_bits_per_sample;
	fmt.channel_mask = wmapro_cfg->ch_mask;
	fmt.enc_options = wmapro_cfg->encode_opt;
	fmt.usAdvancedEncodeOpt = wmapro_cfg->adv_encode_opt;
	fmt.advanced_enc_options2 = wmapro_cfg->adv_encode_opt2;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &fmt);
	if (rc < 0) {
		pr_err("%s:Comamnd open failed\n", __func__);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout. waited for FORMAT_UPDATE\n", __func__);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_media_format_block_amrwbplus(struct audio_client *ac,
				struct asm_amrwbplus_cfg *cfg)
{
	struct asm_amrwbplus_fmt_blk_v2 fmt;
	int rc = 0;

	pr_debug("%s:session[%d]band-mode[%d]frame-fmt[%d]ch[%d]\n",
		__func__,
		ac->session,
		cfg->amr_band_mode,
		cfg->amr_frame_fmt,
		cfg->num_channels);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);
	atomic_set(&ac->cmd_state, 1);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2;
	fmt.fmtblk.fmt_blk_size = sizeof(fmt) - sizeof(fmt.hdr) -
					sizeof(fmt.fmtblk);
	fmt.amr_frame_fmt = cfg->amr_frame_fmt;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &fmt);
	if (rc < 0) {
		pr_err("%s:Comamnd media format update failed..\n", __func__);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
				(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout. waited for FORMAT_UPDATE\n", __func__);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_ds1_set_endp_params(struct audio_client *ac,
				int param_id, int param_value)
{
	struct asm_dec_ddp_endp_param_v2 ddp_cfg;
	int rc = 0;

	pr_debug("%s: session[%d]param_id[%d]param_value[%d]", __func__,
			ac->session, param_id, param_value);
	q6asm_add_hdr(ac, &ddp_cfg.hdr, sizeof(ddp_cfg), TRUE);
	atomic_set(&ac->cmd_state, 1);
	ddp_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	ddp_cfg.encdec.param_id = param_id;
	ddp_cfg.encdec.param_size = sizeof(struct asm_dec_ddp_endp_param_v2) -
				(sizeof(struct apr_hdr) +
				sizeof(struct asm_stream_cmd_set_encdec_param));
	ddp_cfg.endp_param_value = param_value;
	rc = apr_send_pkt(ac->apr, (uint32_t *) &ddp_cfg);
	if (rc < 0) {
		pr_err("%s:Command opcode[0x%x] failed\n",
			__func__, ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
		(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s:timeout opcode[0x%x]\n", __func__,
			ddp_cfg.hdr.opcode);
		rc = -ETIMEDOUT;
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return rc;
}

int q6asm_memory_map(struct audio_client *ac, uint32_t buf_add, int dir,
				uint32_t bufsz, uint32_t bufcnt)
{
	struct avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct avs_shared_map_region_payload  *mregions = NULL;
	struct audio_port_data *port = NULL;
	void	*mmap_region_cmd = NULL;
	void	*payload = NULL;
	struct asm_buffer_node *buffer_node = NULL;
	int	rc = 0;
	int	cmd_size = 0;

	if (!ac || ac->mmap_apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	buffer_node = kmalloc(sizeof(struct asm_buffer_node), GFP_KERNEL);
	if (!buffer_node)
		return -ENOMEM;
	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions)
			+ sizeof(struct avs_shared_map_region_payload) * bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (mmap_region_cmd == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		rc = -EINVAL;
		kfree(buffer_node);
		return rc;
	}
	mmap_regions = (struct avs_cmd_shared_mem_map_regions *)
							mmap_region_cmd;
	q6asm_add_mmaphdr(ac, &mmap_regions->hdr, cmd_size,
			TRUE, ((ac->session << 8) | dir));
	atomic_set(&ac->mem_state, 1);
	mmap_regions->hdr.opcode = ASM_CMD_SHARED_MEM_MAP_REGIONS;
	mmap_regions->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL;
	mmap_regions->num_regions = bufcnt & 0x00ff;
	mmap_regions->property_flag = 0x00;
	payload = ((u8 *) mmap_region_cmd +
		sizeof(struct avs_cmd_shared_mem_map_regions));
	mregions = (struct avs_shared_map_region_payload *)payload;

	ac->port[dir].tmp_hdl = 0;
	port = &ac->port[dir];
	pr_debug("%s, buf_add 0x%x, bufsz: %d\n", __func__, buf_add, bufsz);
	mregions->shm_addr_lsw = buf_add;
	/* Using only 32 bit address */
	mregions->shm_addr_msw = 0;
	mregions->mem_size_bytes = bufsz;
	++mregions;

	rc = apr_send_pkt(ac->mmap_apr, (uint32_t *) mmap_region_cmd);
	if (rc < 0) {
		pr_err("mmap op[0x%x]rc[%d]\n",
					mmap_regions->hdr.opcode, rc);
		rc = -EINVAL;
		kfree(buffer_node);
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->mem_wait,
			(atomic_read(&ac->mem_state) == 0 &&
			 ac->port[dir].tmp_hdl), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		kfree(buffer_node);
		goto fail_cmd;
	}
	buffer_node->buf_addr_lsw = buf_add;
	buffer_node->mmap_hdl = ac->port[dir].tmp_hdl;
	list_add_tail(&buffer_node->list, &ac->port[dir].mem_map_handle);
	ac->port[dir].tmp_hdl = 0;
	rc = 0;

fail_cmd:
	kfree(mmap_region_cmd);
	return rc;
}

int q6asm_memory_unmap(struct audio_client *ac, uint32_t buf_add, int dir)
{
	struct avs_cmd_shared_mem_unmap_regions mem_unmap;
	struct asm_buffer_node *buf_node = NULL;
	struct list_head *ptr, *next;

	int rc = 0;

	if (!ac || this_mmap.apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	q6asm_add_mmaphdr(ac, &mem_unmap.hdr,
			sizeof(struct avs_cmd_shared_mem_unmap_regions),
			TRUE, ((ac->session << 8) | dir));
	atomic_set(&ac->mem_state, 1);
	mem_unmap.hdr.opcode = ASM_CMD_SHARED_MEM_UNMAP_REGIONS;
	mem_unmap.mem_map_handle = 0;
	list_for_each_safe(ptr, next, &ac->port[dir].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
						list);
		if (buf_node->buf_addr_lsw == buf_add) {
			pr_debug("%s: Found the element\n", __func__);
			mem_unmap.mem_map_handle = buf_node->mmap_hdl;
			break;
		}
	}
	pr_debug("%s: mem_unmap-mem_map_handle: 0x%x",
		__func__, mem_unmap.mem_map_handle);

	if (mem_unmap.mem_map_handle == 0) {
		pr_err("%s Do not send null mem handle to DSP\n", __func__);
		rc = 0;
		goto fail_cmd;
	}
	rc = apr_send_pkt(ac->mmap_apr, (uint32_t *) &mem_unmap);
	if (rc < 0) {
		pr_err("mem_unmap op[0x%x]rc[%d]\n",
					mem_unmap.hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->mem_wait,
			(atomic_read(&ac->mem_state) == 0), 5 * HZ);
	if (!rc) {
		pr_err("%s timeout. waited for memory_unmap of handle 0x%x\n",
			__func__, mem_unmap.mem_map_handle);
		rc = -ETIMEDOUT;
		goto fail_cmd;
	} else if (atomic_read(&ac->unmap_cb_success) == 0) {
		pr_err("%s Error in mem unmap callback of handle 0x%x\n",
			__func__, mem_unmap.mem_map_handle);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = 0;
fail_cmd:
	list_for_each_safe(ptr, next, &ac->port[dir].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
						list);
		if (buf_node->buf_addr_lsw == buf_add) {
			list_del(&buf_node->list);
			kfree(buf_node);
			break;
		}
	}
	return rc;
}


static int q6asm_memory_map_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt,
				bool is_contiguous)
{
	struct avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct avs_shared_map_region_payload  *mregions = NULL;
	struct audio_port_data *port = NULL;
	struct audio_buffer *ab = NULL;
	void	*mmap_region_cmd = NULL;
	void	*payload = NULL;
	struct asm_buffer_node *buffer_node = NULL;
	int	rc = 0;
	int    i = 0;
	int	cmd_size = 0;
	uint32_t bufcnt_t;
	uint32_t bufsz_t;

	if (!ac || ac->mmap_apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	bufcnt_t = (is_contiguous) ? 1 : bufcnt;
	bufsz_t = (is_contiguous) ? (bufsz * bufcnt) : bufsz;

	if (is_contiguous) {
		/* The size to memory map should be multiple of 4K bytes */
		bufsz_t = PAGE_ALIGN(bufsz_t);
	}

	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions)
			+ (sizeof(struct avs_shared_map_region_payload)
							* bufcnt_t);

	buffer_node = kzalloc(sizeof(struct asm_buffer_node) * bufcnt,
				GFP_KERNEL);
	if (!buffer_node) {
		pr_err("%s: Mem alloc failed for asm_buffer_node\n",
				__func__);
		return -ENOMEM;
	}

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (mmap_region_cmd == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		rc = -EINVAL;
		kfree(buffer_node);
		return rc;
	}
	mmap_regions = (struct avs_cmd_shared_mem_map_regions *)
							mmap_region_cmd;
	q6asm_add_mmaphdr(ac, &mmap_regions->hdr, cmd_size, TRUE,
					((ac->session << 8) | dir));
	atomic_set(&ac->mem_state, 1);
	pr_debug("mmap_region=0x%p token=0x%x\n",
		mmap_regions, ((ac->session << 8) | dir));

	mmap_regions->hdr.opcode = ASM_CMD_SHARED_MEM_MAP_REGIONS;
	mmap_regions->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL;
	mmap_regions->num_regions = bufcnt_t; /*bufcnt & 0x00ff; */
	mmap_regions->property_flag = 0x00;
	pr_debug("map_regions->nregions = %d\n", mmap_regions->num_regions);
	payload = ((u8 *) mmap_region_cmd +
		sizeof(struct avs_cmd_shared_mem_map_regions));
	mregions = (struct avs_shared_map_region_payload *)payload;

	ac->port[dir].tmp_hdl = 0;
	port = &ac->port[dir];
	for (i = 0; i < bufcnt_t; i++) {
		ab = &port->buf[i];
		mregions->shm_addr_lsw = ab->phys;
	/* Using only 32 bit address */
		mregions->shm_addr_msw = 0;
		mregions->mem_size_bytes = bufsz_t;
		++mregions;
	}

	rc = apr_send_pkt(ac->mmap_apr, (uint32_t *) mmap_region_cmd);
	if (rc < 0) {
		pr_err("mmap_regions op[0x%x]rc[%d]\n",
					mmap_regions->hdr.opcode, rc);
		rc = -EINVAL;
		kfree(buffer_node);
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->mem_wait,
			(atomic_read(&ac->mem_state) == 0)
			 , 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		kfree(buffer_node);
		goto fail_cmd;
	}
	mutex_lock(&ac->cmd_lock);

	for (i = 0; i < bufcnt; i++) {
		ab = &port->buf[i];
		buffer_node[i].buf_addr_lsw = ab->phys;
		buffer_node[i].mmap_hdl = ac->port[dir].tmp_hdl;
		list_add_tail(&buffer_node[i].list,
			&ac->port[dir].mem_map_handle);
		pr_debug("%s: i=%d, bufadd[i] = 0x%x, maphdl[i] = 0x%x\n",
			__func__, i, buffer_node[i].buf_addr_lsw,
			buffer_node[i].mmap_hdl);
	}
	ac->port[dir].tmp_hdl = 0;
	mutex_unlock(&ac->cmd_lock);
	rc = 0;
	pr_debug("%s: exit\n", __func__);
fail_cmd:
	kfree(mmap_region_cmd);
	return rc;
}

static int q6asm_memory_unmap_regions(struct audio_client *ac, int dir)
{
	struct avs_cmd_shared_mem_unmap_regions mem_unmap;
	struct audio_port_data *port = NULL;
	struct asm_buffer_node *buf_node = NULL;
	struct list_head *ptr, *next;
	uint32_t buf_add;
	int	rc = 0;
	int	cmd_size = 0;

	if (!ac || ac->mmap_apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	cmd_size = sizeof(struct avs_cmd_shared_mem_unmap_regions);
	q6asm_add_mmaphdr(ac, &mem_unmap.hdr, cmd_size,
			TRUE, ((ac->session << 8) | dir));
	atomic_set(&ac->mem_state, 1);
	port = &ac->port[dir];
	buf_add = (uint32_t)port->buf->phys;
	mem_unmap.hdr.opcode = ASM_CMD_SHARED_MEM_UNMAP_REGIONS;
	mem_unmap.mem_map_handle = 0;
	list_for_each_safe(ptr, next, &ac->port[dir].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
						list);
		if (buf_node->buf_addr_lsw == buf_add) {
			pr_debug("%s: Found the element\n", __func__);
			mem_unmap.mem_map_handle = buf_node->mmap_hdl;
			break;
		}
	}

	pr_debug("%s: mem_unmap-mem_map_handle: 0x%x",
			__func__, mem_unmap.mem_map_handle);

	if (mem_unmap.mem_map_handle == 0) {
		pr_err("%s Do not send null mem handle to DSP\n", __func__);
		rc = 0;
		goto fail_cmd;
	}
	rc = apr_send_pkt(ac->mmap_apr, (uint32_t *) &mem_unmap);
	if (rc < 0) {
		pr_err("mmap_regions op[0x%x]rc[%d]\n",
					mem_unmap.hdr.opcode, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->mem_wait,
			(atomic_read(&ac->mem_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s timeout. waited for memory_unmap of handle 0x%x\n",
			__func__, mem_unmap.mem_map_handle);
		rc = -ETIMEDOUT;
		goto fail_cmd;
	} else if (atomic_read(&ac->unmap_cb_success) == 0) {
		pr_err("%s Error in mem unmap callback of handle 0x%x\n",
			__func__, mem_unmap.mem_map_handle);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;

fail_cmd:
	list_for_each_safe(ptr, next, &ac->port[dir].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
						list);
		if (buf_node->buf_addr_lsw == buf_add) {
			list_del(&buf_node->list);
			kfree(buf_node);
			break;
		}
	}
	return rc;
}

int q6asm_set_lrgain(struct audio_client *ac, int left_gain, int right_gain)
{
	struct asm_volume_ctrl_lr_chan_gain lrgain;
	int sz = 0;
	int rc  = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	sz = sizeof(struct asm_volume_ctrl_lr_chan_gain);
	q6asm_add_hdr_async(ac, &lrgain.hdr, sz, TRUE);
	atomic_set(&ac->cmd_state, 1);
	lrgain.hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS_V2;
	lrgain.param.data_payload_addr_lsw = 0;
	lrgain.param.data_payload_addr_msw = 0;
	lrgain.param.mem_map_handle = 0;
	lrgain.param.data_payload_size = sizeof(lrgain) -
				sizeof(lrgain.hdr) - sizeof(lrgain.param);
	lrgain.data.module_id = ASM_MODULE_ID_VOL_CTRL;
	lrgain.data.param_id = ASM_PARAM_ID_VOL_CTRL_LR_CHANNEL_GAIN;
	lrgain.data.param_size = lrgain.param.data_payload_size -
				sizeof(lrgain.data);
	lrgain.data.reserved = 0;
	lrgain.l_chan_gain = left_gain;
	lrgain.r_chan_gain = right_gain;
	rc = apr_send_pkt(ac->apr, (uint32_t *) &lrgain);
	if (rc < 0) {
		pr_err("%s: set-params send failed paramid[0x%x]\n", __func__,
						lrgain.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout, set-params paramid[0x%x]\n", __func__,
						lrgain.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_set_mute(struct audio_client *ac, int muteflag)
{
	struct asm_volume_ctrl_mute_config mute;
	int sz = 0;
	int rc  = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	sz = sizeof(struct asm_volume_ctrl_mute_config);
	q6asm_add_hdr_async(ac, &mute.hdr, sz, TRUE);
	atomic_set(&ac->cmd_state, 1);
	mute.hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS_V2;
	mute.param.data_payload_addr_lsw = 0;
	mute.param.data_payload_addr_msw = 0;
	mute.param.mem_map_handle = 0;
	mute.param.data_payload_size = sizeof(mute) -
				sizeof(mute.hdr) - sizeof(mute.param);
	mute.data.module_id = ASM_MODULE_ID_VOL_CTRL;
	mute.data.param_id = ASM_PARAM_ID_VOL_CTRL_MUTE_CONFIG;
	mute.data.param_size = mute.param.data_payload_size - sizeof(mute.data);
	mute.data.reserved = 0;
	mute.mute_flag = muteflag;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &mute);
	if (rc < 0) {
		pr_err("%s: set-params send failed paramid[0x%x]\n", __func__,
						mute.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout, set-params paramid[0x%x]\n", __func__,
						mute.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_set_volume(struct audio_client *ac, int volume)
{
	struct asm_volume_ctrl_master_gain vol;
	int sz = 0;
	int rc  = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	sz = sizeof(struct asm_volume_ctrl_master_gain);
	q6asm_add_hdr_async(ac, &vol.hdr, sz, TRUE);
	atomic_set(&ac->cmd_state, 1);
	vol.hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS_V2;
	vol.param.data_payload_addr_lsw = 0;
	vol.param.data_payload_addr_msw = 0;
	vol.param.mem_map_handle = 0;
	vol.param.data_payload_size = sizeof(vol) -
				sizeof(vol.hdr) - sizeof(vol.param);
	vol.data.module_id = ASM_MODULE_ID_VOL_CTRL;
	vol.data.param_id = ASM_PARAM_ID_VOL_CTRL_MASTER_GAIN;
	vol.data.param_size = vol.param.data_payload_size - sizeof(vol.data);
	vol.data.reserved = 0;
	vol.master_gain = volume;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &vol);
	if (rc < 0) {
		pr_err("%s: set-params send failed paramid[0x%x]\n", __func__,
						vol.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout, set-params paramid[0x%x]\n", __func__,
						vol.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_set_softpause(struct audio_client *ac,
			struct asm_softpause_params *pause_param)
{
	struct asm_soft_pause_params softpause;
	int sz = 0;
	int rc  = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	sz = sizeof(struct asm_soft_pause_params);
	q6asm_add_hdr_async(ac, &softpause.hdr, sz, TRUE);
	atomic_set(&ac->cmd_state, 1);
	softpause.hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS_V2;

	softpause.param.data_payload_addr_lsw = 0;
	softpause.param.data_payload_addr_msw = 0;
	softpause.param.mem_map_handle = 0;
	softpause.param.data_payload_size = sizeof(softpause) -
				sizeof(softpause.hdr) - sizeof(softpause.param);
	softpause.data.module_id = ASM_MODULE_ID_VOL_CTRL;
	softpause.data.param_id = ASM_PARAM_ID_SOFT_PAUSE_PARAMETERS;
	softpause.data.param_size = softpause.param.data_payload_size -
				sizeof(softpause.data);
	softpause.data.reserved = 0;
	softpause.enable_flag = pause_param->enable;
	softpause.period = pause_param->period;
	softpause.step = pause_param->step;
	softpause.ramping_curve = pause_param->rampingcurve;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &softpause);
	if (rc < 0) {
		pr_err("%s: set-params send failed paramid[0x%x]\n", __func__,
						softpause.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout, set-params paramid[0x%x]\n", __func__,
						softpause.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_set_softvolume(struct audio_client *ac,
			struct asm_softvolume_params *softvol_param)
{
	struct asm_soft_step_volume_params softvol;
	int sz = 0;
	int rc  = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	sz = sizeof(struct asm_soft_step_volume_params);
	q6asm_add_hdr_async(ac, &softvol.hdr, sz, TRUE);
	atomic_set(&ac->cmd_state, 1);
	softvol.hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS_V2;
	softvol.param.data_payload_addr_lsw = 0;
	softvol.param.data_payload_addr_msw = 0;
	softvol.param.mem_map_handle = 0;
	softvol.param.data_payload_size = sizeof(softvol) -
				sizeof(softvol.hdr) - sizeof(softvol.param);
	softvol.data.module_id = ASM_MODULE_ID_VOL_CTRL;
	softvol.data.param_id = ASM_PARAM_ID_SOFT_VOL_STEPPING_PARAMETERS;
	softvol.data.param_size = softvol.param.data_payload_size -
				sizeof(softvol.data);
	softvol.data.reserved = 0;
	softvol.period = softvol_param->period;
	softvol.step = softvol_param->step;
	softvol.ramping_curve = softvol_param->rampingcurve;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &softvol);
	if (rc < 0) {
		pr_err("%s: set-params send failed paramid[0x%x]\n", __func__,
						softvol.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout, set-params paramid[0x%x]\n", __func__,
						softvol.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_equalizer(struct audio_client *ac, void *eq_p)
{
	struct asm_eq_params eq;
	struct msm_audio_eq_stream_config *eq_params = NULL;
	int i  = 0;
	int sz = 0;
	int rc  = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	if (eq_p == NULL) {
		pr_err("%s[%d]: Invalid Eq param\n", __func__, ac->session);
		rc = -EINVAL;
		goto fail_cmd;
	}
	sz = sizeof(struct asm_eq_params);
	eq_params = (struct msm_audio_eq_stream_config *) eq_p;
	q6asm_add_hdr(ac, &eq.hdr, sz, TRUE);
	atomic_set(&ac->cmd_state, 1);

	eq.hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS_V2;
	eq.param.data_payload_addr_lsw = 0;
	eq.param.data_payload_addr_msw = 0;
	eq.param.mem_map_handle = 0;
	eq.param.data_payload_size = sizeof(eq) -
				sizeof(eq.hdr) - sizeof(eq.param);
	eq.data.module_id = ASM_MODULE_ID_EQUALIZER;
	eq.data.param_id = ASM_PARAM_ID_EQUALIZER_PARAMETERS;
	eq.data.param_size = eq.param.data_payload_size - sizeof(eq.data);
	eq.enable_flag = eq_params->enable;
	eq.num_bands = eq_params->num_bands;

	pr_debug("%s: enable:%d numbands:%d\n", __func__, eq_params->enable,
							eq_params->num_bands);
	for (i = 0; i < eq_params->num_bands; i++) {
		eq.eq_bands[i].band_idx =
					eq_params->eq_bands[i].band_idx;
		eq.eq_bands[i].filterype =
					eq_params->eq_bands[i].filter_type;
		eq.eq_bands[i].center_freq_hz =
					eq_params->eq_bands[i].center_freq_hz;
		eq.eq_bands[i].filter_gain =
					eq_params->eq_bands[i].filter_gain;
		eq.eq_bands[i].q_factor =
					eq_params->eq_bands[i].q_factor;
		pr_debug("%s: filter_type:%u bandnum:%d\n", __func__,
				eq_params->eq_bands[i].filter_type, i);
		pr_debug("%s: center_freq_hz:%u bandnum:%d\n", __func__,
				eq_params->eq_bands[i].center_freq_hz, i);
		pr_debug("%s: filter_gain:%d bandnum:%d\n", __func__,
				eq_params->eq_bands[i].filter_gain, i);
		pr_debug("%s: q_factor:%d bandnum:%d\n", __func__,
				eq_params->eq_bands[i].q_factor, i);
	}
	rc = apr_send_pkt(ac->apr, (uint32_t *)&eq);
	if (rc < 0) {
		pr_err("%s: set-params send failed paramid[0x%x]\n", __func__,
						eq.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout, set-params paramid[0x%x]\n", __func__,
						eq.data.param_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_read(struct audio_client *ac)
{
	struct asm_data_cmd_read_v2 read;
	struct asm_buffer_node *buf_node = NULL;
	struct list_head *ptr, *next;
	struct audio_buffer        *ab;
	int dsp_buf;
	struct audio_port_data     *port;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	if (ac->io_mode & SYNC_IO_MODE) {
		port = &ac->port[OUT];

		q6asm_add_hdr(ac, &read.hdr, sizeof(read), FALSE);

		mutex_lock(&port->lock);

		dsp_buf = port->dsp_buf;
		if (port->buf == NULL) {
			pr_err("%s buf is NULL\n", __func__);
			mutex_unlock(&port->lock);
			return -EINVAL;
		}
		ab = &port->buf[dsp_buf];

		pr_debug("%s:session[%d]dsp-buf[%d][%p]cpu_buf[%d][%p]\n",
					__func__,
					ac->session,
					dsp_buf,
					(void *)port->buf[dsp_buf].data,
					port->cpu_buf,
					(void *)port->buf[port->cpu_buf].phys);

		read.hdr.opcode = ASM_DATA_CMD_READ_V2;
		read.buf_addr_lsw = ab->phys;
		read.buf_addr_msw = 0;

		list_for_each_safe(ptr, next, &ac->port[OUT].mem_map_handle) {
			buf_node = list_entry(ptr, struct asm_buffer_node,
						list);
			if (buf_node->buf_addr_lsw == (uint32_t) ab->phys)
				read.mem_map_handle = buf_node->mmap_hdl;
		}
		pr_debug("memory_map handle in q6asm_read: [%0x]:",
			read.mem_map_handle);
		read.buf_size = ab->size;
		read.seq_id = port->dsp_buf;
		read.hdr.token = port->dsp_buf;
		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);
		mutex_unlock(&port->lock);
		pr_debug("%s:buf add[0x%x] token[%d] uid[%d]\n", __func__,
						read.buf_addr_lsw,
						read.hdr.token,
						read.seq_id);
		rc = apr_send_pkt(ac->apr, (uint32_t *) &read);
		if (rc < 0) {
			pr_err("read op[0x%x]rc[%d]\n", read.hdr.opcode, rc);
			goto fail_cmd;
		}
		return 0;
	}
fail_cmd:
	return -EINVAL;
}

int q6asm_read_nolock(struct audio_client *ac)
{
	struct asm_data_cmd_read_v2 read;
	struct asm_buffer_node *buf_node = NULL;
	struct list_head *ptr, *next;
	struct audio_buffer        *ab;
	int dsp_buf;
	struct audio_port_data     *port;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	if (ac->io_mode & SYNC_IO_MODE) {
		port = &ac->port[OUT];

		q6asm_add_hdr_async(ac, &read.hdr, sizeof(read), FALSE);


		dsp_buf = port->dsp_buf;
		ab = &port->buf[dsp_buf];

		pr_debug("%s:session[%d]dsp-buf[%d][%p]cpu_buf[%d][%p]\n",
					__func__,
					ac->session,
					dsp_buf,
					(void *)port->buf[dsp_buf].data,
					port->cpu_buf,
					(void *)port->buf[port->cpu_buf].phys);

		read.hdr.opcode = ASM_DATA_CMD_READ_V2;
		read.buf_addr_lsw = ab->phys;
		read.buf_addr_msw = 0;
		read.buf_size = ab->size;
		read.seq_id = port->dsp_buf;
		read.hdr.token = port->dsp_buf;

		list_for_each_safe(ptr, next, &ac->port[OUT].mem_map_handle) {
			buf_node = list_entry(ptr, struct asm_buffer_node,
						list);
			if (buf_node->buf_addr_lsw == (uint32_t)ab->phys) {
				read.mem_map_handle = buf_node->mmap_hdl;
				break;
			}
		}

		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);
		pr_debug("%s:buf add[0x%x] token[%d] uid[%d]\n", __func__,
					read.buf_addr_lsw,
					read.hdr.token,
					read.seq_id);
		rc = apr_send_pkt(ac->apr, (uint32_t *) &read);
		if (rc < 0) {
			pr_err("read op[0x%x]rc[%d]\n", read.hdr.opcode, rc);
			goto fail_cmd;
		}
		return 0;
	}
fail_cmd:
	return -EINVAL;
}

int q6asm_async_write(struct audio_client *ac,
					  struct audio_aio_write_param *param)
{
	int rc = 0;
	struct asm_data_cmd_write_v2 write;
	struct asm_buffer_node *buf_node = NULL;
	struct list_head *ptr, *next;
	struct audio_buffer        *ab;
	struct audio_port_data     *port;
	u32 lbuf_addr_lsw;
	u32 liomode;
	u32 io_compressed;
	u32 io_compressed_stream;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6asm_stream_add_hdr_async(
			ac, &write.hdr, sizeof(write), FALSE, ac->stream_id);
	port = &ac->port[IN];
	ab = &port->buf[port->dsp_buf];

	/* Pass physical address as token for AIO scheme */
	write.hdr.token = param->uid;
	write.hdr.opcode = ASM_DATA_CMD_WRITE_V2;
	write.buf_addr_lsw = param->paddr;
	write.buf_addr_msw = 0x00;
	write.buf_size = param->len;
	write.timestamp_msw = param->msw_ts;
	write.timestamp_lsw = param->lsw_ts;
	liomode = (ASYNC_IO_MODE | NT_MODE);
	io_compressed = (ASYNC_IO_MODE | COMPRESSED_IO);
	io_compressed_stream = (ASYNC_IO_MODE | COMPRESSED_STREAM_IO);

	if (ac->io_mode == liomode)
		lbuf_addr_lsw = (write.buf_addr_lsw - 32);
	else if (ac->io_mode == io_compressed ||
		ac->io_mode == io_compressed_stream)
		lbuf_addr_lsw = (write.buf_addr_lsw - param->metadata_len);
	else
		lbuf_addr_lsw = write.buf_addr_lsw;

	pr_debug("%s: token[0x%x], buf_addr_lsw[0x%x], buf_size[0x%x], ts_msw[0x%x], ts_lsw[0x%x], lbuf_addr_lsw: 0x[%x]\n",
		__func__,
		write.hdr.token, write.buf_addr_lsw,
		write.buf_size, write.timestamp_msw,
		write.timestamp_lsw, lbuf_addr_lsw);

	/* Use 0xFF00 for disabling timestamps */
	if (param->flags == 0xFF00)
		write.flags = (0x00000000 | (param->flags & 0x800000FF));
	else
		write.flags = (0x80000000 | param->flags);
	write.flags |= param->last_buffer << ASM_SHIFT_LAST_BUFFER_FLAG;
	write.seq_id = param->uid;
	list_for_each_safe(ptr, next, &ac->port[IN].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
						list);
		if (buf_node->buf_addr_lsw == lbuf_addr_lsw) {
			write.mem_map_handle = buf_node->mmap_hdl;
			break;
		}
	}

	rc = apr_send_pkt(ac->apr, (uint32_t *) &write);
	if (rc < 0) {
		pr_debug("[%s] write op[0x%x]rc[%d]\n", __func__,
			write.hdr.opcode, rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_async_read(struct audio_client *ac,
					  struct audio_aio_read_param *param)
{
	int rc = 0;
	struct asm_data_cmd_read_v2 read;
	struct asm_buffer_node *buf_node = NULL;
	struct list_head *ptr, *next;
	u32 lbuf_addr_lsw;
	u32 liomode;
	u32 io_compressed;
	int dir = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6asm_add_hdr_async(ac, &read.hdr, sizeof(read), FALSE);

	/* Pass physical address as token for AIO scheme */
	read.hdr.token = param->paddr;
	read.hdr.opcode = ASM_DATA_CMD_READ_V2;
	read.buf_addr_lsw = param->paddr;
	read.buf_addr_msw = 0;
	read.buf_size = param->len;
	read.seq_id = param->uid;
	liomode = (NT_MODE | ASYNC_IO_MODE);
	io_compressed = (ASYNC_IO_MODE | COMPRESSED_IO);
	if (ac->io_mode == liomode) {
		lbuf_addr_lsw = (read.buf_addr_lsw - 32);
		/*legacy wma driver case*/
		dir = IN;
	} else if (ac->io_mode == io_compressed) {
		lbuf_addr_lsw = (read.buf_addr_lsw - 64);
		dir = OUT;
	} else {
		lbuf_addr_lsw = read.buf_addr_lsw;
		dir = OUT;
	}

	list_for_each_safe(ptr, next, &ac->port[dir].mem_map_handle) {
		buf_node = list_entry(ptr, struct asm_buffer_node,
			list);
			if (buf_node->buf_addr_lsw == lbuf_addr_lsw) {
				read.mem_map_handle = buf_node->mmap_hdl;
				break;
		}
	}

	rc = apr_send_pkt(ac->apr, (uint32_t *) &read);
	if (rc < 0) {
		pr_debug("[%s] read op[0x%x]rc[%d]\n", __func__,
			read.hdr.opcode, rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_write(struct audio_client *ac, uint32_t len, uint32_t msw_ts,
		uint32_t lsw_ts, uint32_t flags)
{
	int rc = 0;
	struct asm_data_cmd_write_v2 write;
	struct asm_buffer_node *buf_node = NULL;
	struct audio_port_data *port;
	struct audio_buffer    *ab;
	int dsp_buf = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: session[%d] len=%d", __func__, ac->session, len);
	if (ac->io_mode & SYNC_IO_MODE) {
		port = &ac->port[IN];

		q6asm_add_hdr(ac, &write.hdr, sizeof(write),
				FALSE);
		mutex_lock(&port->lock);

		dsp_buf = port->dsp_buf;
		ab = &port->buf[dsp_buf];

		write.hdr.token = port->dsp_buf;
		write.hdr.opcode = ASM_DATA_CMD_WRITE_V2;
		write.buf_addr_lsw = ab->phys;
		write.buf_addr_msw = 0;
		write.buf_size = len;
		write.seq_id = port->dsp_buf;
		write.timestamp_lsw = lsw_ts;
		write.timestamp_msw = msw_ts;
		/* Use 0xFF00 for disabling timestamps */
		if (flags == 0xFF00)
			write.flags = (0x00000000 | (flags & 0x800000FF));
		else
			write.flags = (0x80000000 | flags);
		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);
		buf_node = list_first_entry(&ac->port[IN].mem_map_handle,
						struct asm_buffer_node,
						list);
		write.mem_map_handle = buf_node->mmap_hdl;

		pr_debug("%s:ab->phys[0x%x]bufadd[0x%x] token[0x%x]buf_id[0x%x]buf_size[0x%x]mmaphdl[0x%x]"
						, __func__,
						ab->phys,
						write.buf_addr_lsw,
						write.hdr.token,
						write.seq_id,
						write.buf_size,
						write.mem_map_handle);
		mutex_unlock(&port->lock);

		config_debug_fs_write(ab);

		rc = apr_send_pkt(ac->apr, (uint32_t *) &write);
		if (rc < 0) {
			pr_err("write op[0x%x]rc[%d]\n", write.hdr.opcode, rc);
			goto fail_cmd;
		}
		pr_debug("%s: WRITE SUCCESS\n", __func__);
		return 0;
	}
fail_cmd:
	return -EINVAL;
}

int q6asm_write_nolock(struct audio_client *ac, uint32_t len, uint32_t msw_ts,
			uint32_t lsw_ts, uint32_t flags)
{
	int rc = 0;
	struct asm_data_cmd_write_v2 write;
	struct asm_buffer_node *buf_node = NULL;
	struct audio_port_data *port;
	struct audio_buffer    *ab;
	int dsp_buf = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: session[%d] len=%d", __func__, ac->session, len);
	if (ac->io_mode & SYNC_IO_MODE) {
		port = &ac->port[IN];

		q6asm_add_hdr_async(ac, &write.hdr, sizeof(write),
					FALSE);

		dsp_buf = port->dsp_buf;
		ab = &port->buf[dsp_buf];

		write.hdr.token = port->dsp_buf;
		write.hdr.opcode = ASM_DATA_CMD_WRITE_V2;
		write.buf_addr_lsw = ab->phys;
		write.buf_addr_msw = 0;
		write.buf_size = len;
		write.seq_id = port->dsp_buf;
		write.timestamp_lsw = lsw_ts;
		write.timestamp_msw = msw_ts;
		buf_node = list_first_entry(&ac->port[IN].mem_map_handle,
						struct asm_buffer_node,
						list);
		write.mem_map_handle = buf_node->mmap_hdl;
		/* Use 0xFF00 for disabling timestamps */
		if (flags == 0xFF00)
			write.flags = (0x00000000 | (flags & 0x800000FF));
		else
			write.flags = (0x80000000 | flags);
		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);

		pr_debug("%s:ab->phys[0x%x]bufadd[0x%x]token[0x%x] buf_id[0x%x]buf_size[0x%x]mmaphdl[0x%x]"
							, __func__,
							ab->phys,
							write.buf_addr_lsw,
							write.hdr.token,
							write.seq_id,
							write.buf_size,
							write.mem_map_handle);

		rc = apr_send_pkt(ac->apr, (uint32_t *) &write);
		if (rc < 0) {
			pr_err("write op[0x%x]rc[%d]\n", write.hdr.opcode, rc);
			goto fail_cmd;
		}
		pr_debug("%s: WRITE SUCCESS\n", __func__);
		return 0;
	}
fail_cmd:
	return -EINVAL;
}

int q6asm_get_session_time(struct audio_client *ac, uint64_t *tstamp)
{
	struct apr_hdr hdr;
	int rc;

	if (!ac || ac->apr == NULL || tstamp == NULL) {
		pr_err("%s: APR handle NULL or tstamp NULL\n", __func__);
		return -EINVAL;
	}
	q6asm_add_hdr(ac, &hdr, sizeof(hdr), TRUE);
	hdr.opcode = ASM_SESSION_CMD_GET_SESSIONTIME_V3;
	atomic_set(&ac->time_flag, 1);

	pr_debug("%s: session[%d]opcode[0x%x]\n", __func__,
						ac->session,
						hdr.opcode);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &hdr);
	if (rc < 0) {
		pr_err("Commmand 0x%x failed\n", hdr.opcode);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->time_wait,
			(atomic_read(&ac->time_flag) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout in getting session time from DSP\n",
			__func__);
		goto fail_cmd;
	}

	*tstamp = ac->time_stamp;
	return 0;

fail_cmd:
	return -EINVAL;
}

int q6asm_send_audio_effects_params(struct audio_client *ac, char *params,
				    uint32_t params_length)
{
	char *asm_params = NULL;
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_pp_params_v2 payload_params;
	int sz, rc;

	pr_debug("%s\n", __func__);
	if (!ac || ac->apr == NULL || params == NULL) {
		pr_err("APR handle NULL or params NULL\n");
		return -EINVAL;
	}
	sz = sizeof(struct apr_hdr) +
	     sizeof(struct asm_stream_cmd_set_pp_params_v2) +
	     params_length;
	asm_params = kzalloc(sz, GFP_KERNEL);
	if (!asm_params) {
		pr_err("%s, adm params memory alloc failed", __func__);
		return -ENOMEM;
	}
	q6asm_add_hdr_async(ac, &hdr, (sizeof(struct apr_hdr) +
			    sizeof(struct asm_stream_cmd_set_pp_params_v2) +
			    params_length), TRUE);
	atomic_set(&ac->cmd_state, 1);
	hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS_V2;
	payload_params.data_payload_addr_lsw = 0;
	payload_params.data_payload_addr_msw = 0;
	payload_params.mem_map_handle = 0;
	payload_params.data_payload_size = params_length;
	memcpy(((u8 *)asm_params), &hdr, sizeof(struct apr_hdr));
	memcpy(((u8 *)asm_params + sizeof(struct apr_hdr)), &payload_params,
		sizeof(struct asm_stream_cmd_set_pp_params_v2));
	memcpy(((u8 *)asm_params + sizeof(struct apr_hdr) +
		 sizeof(struct asm_stream_cmd_set_pp_params_v2)),
		params, params_length);
	rc = apr_send_pkt(ac->apr, (uint32_t *) asm_params);
	if (rc < 0) {
		pr_err("%s: audio effects set-params send failed\n", __func__);
		goto fail_send_param;
	}
	rc = wait_event_timeout(ac->cmd_wait,
				(atomic_read(&ac->cmd_state) == 0), 1*HZ);
	if (!rc) {
		pr_err("%s: timeout, audio effects set-params\n", __func__);
		goto fail_send_param;
	}
	rc = 0;
fail_send_param:
	kfree(asm_params);
	return rc;
}

static int __q6asm_cmd(struct audio_client *ac, int cmd, uint32_t stream_id)
{
	struct apr_hdr hdr;
	int rc;
	atomic_t *state;
	int cnt = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	q6asm_stream_add_hdr(ac, &hdr, sizeof(hdr), TRUE, stream_id);
	atomic_set(&ac->cmd_state, 1);
	/*
	 * Updated the token field with stream/session for compressed playback
	 * Platform driver must know the the stream with which the command is
	 * associated
	 */
	if (ac->io_mode & COMPRESSED_STREAM_IO)
		hdr.token = ((ac->session << 8) & 0xFFFF00) |
			    (stream_id & 0xFF);
	pr_debug("%s: token = 0x%x, stream_id  %d, session 0x%x\n",
		    __func__, hdr.token, stream_id, ac->session);
	switch (cmd) {
	case CMD_PAUSE:
		pr_debug("%s:CMD_PAUSE\n", __func__);
		hdr.opcode = ASM_SESSION_CMD_PAUSE;
		state = &ac->cmd_state;
		break;
	case CMD_SUSPEND:
		pr_debug("%s:CMD_SUSPEND\n", __func__);
		hdr.opcode = ASM_SESSION_CMD_SUSPEND;
		state = &ac->cmd_state;
		break;
	case CMD_FLUSH:
		pr_debug("%s:CMD_FLUSH\n", __func__);
		hdr.opcode = ASM_STREAM_CMD_FLUSH;
		state = &ac->cmd_state;
		break;
	case CMD_OUT_FLUSH:
		pr_debug("%s:CMD_OUT_FLUSH\n", __func__);
		hdr.opcode = ASM_STREAM_CMD_FLUSH_READBUFS;
		state = &ac->cmd_state;
		break;
	case CMD_EOS:
		pr_debug("%s:CMD_EOS\n", __func__);
		hdr.opcode = ASM_DATA_CMD_EOS;
		atomic_set(&ac->cmd_state, 0);
		state = &ac->cmd_state;
		break;
	case CMD_CLOSE:
		pr_debug("%s:CMD_CLOSE\n", __func__);
		hdr.opcode = ASM_STREAM_CMD_CLOSE;
		state = &ac->cmd_state;
		break;
	default:
		pr_err("Invalid format[%d]\n", cmd);
		goto fail_cmd;
	}
	pr_debug("%s:session[%d]opcode[0x%x] ", __func__,
						ac->session,
						hdr.opcode);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &hdr);
	if (rc < 0) {
		pr_err("Commmand 0x%x failed\n", hdr.opcode);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait, (atomic_read(state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for response opcode[0x%x]\n",
							hdr.opcode);
		goto fail_cmd;
	}
	if (cmd == CMD_FLUSH)
		q6asm_reset_buf_state(ac);
	if (cmd == CMD_CLOSE) {
		/* check if DSP return all buffers */
		if (ac->port[IN].buf) {
			for (cnt = 0; cnt < ac->port[IN].max_buf_cnt;
								cnt++) {
				if (ac->port[IN].buf[cnt].used == IN) {
					pr_debug("Write Buf[%d] not returned\n",
									cnt);
				}
			}
		}
		if (ac->port[OUT].buf) {
			for (cnt = 0; cnt < ac->port[OUT].max_buf_cnt; cnt++) {
				if (ac->port[OUT].buf[cnt].used == OUT) {
					pr_debug("Read Buf[%d] not returned\n",
									cnt);
				}
			}
		}
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_cmd(struct audio_client *ac, int cmd)
{
	return __q6asm_cmd(ac, cmd, ac->stream_id);
}

int q6asm_stream_cmd(struct audio_client *ac, int cmd, uint32_t stream_id)
{
	return __q6asm_cmd(ac, cmd, stream_id);
}

static int __q6asm_cmd_nowait(struct audio_client *ac, int cmd,
			      uint32_t stream_id)
{
	struct apr_hdr hdr;
	int rc;

	if (!ac || ac->apr == NULL) {
		pr_err("%s:APR handle NULL\n", __func__);
		return -EINVAL;
	}
	q6asm_stream_add_hdr_async(ac, &hdr, sizeof(hdr), TRUE, stream_id);
	atomic_set(&ac->cmd_state, 1);
	/*
	 * Updated the token field with stream/session for compressed playback
	 * Platform driver must know the the stream with which the command is
	 * associated
	 */
	if (ac->io_mode & COMPRESSED_STREAM_IO)
		hdr.token = ((ac->session << 8) & 0xFFFF00) |
			    (stream_id & 0xFF);

	pr_debug("%s: token = 0x%x, stream_id  %d, session 0x%x\n",
		      __func__, hdr.token, stream_id, ac->session);
	switch (cmd) {
	case CMD_PAUSE:
		pr_debug("%s:CMD_PAUSE\n", __func__);
		hdr.opcode = ASM_SESSION_CMD_PAUSE;
		break;
	case CMD_EOS:
		pr_debug("%s:CMD_EOS\n", __func__);
		hdr.opcode = ASM_DATA_CMD_EOS;
		break;
	case CMD_CLOSE:
		pr_debug("%s:CMD_CLOSE\n", __func__);
		hdr.opcode = ASM_STREAM_CMD_CLOSE;
		break;
	default:
		pr_err("%s:Invalid format[%d]\n", __func__, cmd);
		goto fail_cmd;
	}
	pr_debug("%s:session[%d]opcode[0x%x] ", __func__,
						ac->session,
						hdr.opcode);
	/* have to increase first avoid race */
	atomic_inc(&ac->nowait_cmd_cnt);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &hdr);
	if (rc < 0) {
		atomic_dec(&ac->nowait_cmd_cnt);
		pr_err("%s:Commmand 0x%x failed\n", __func__, hdr.opcode);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_cmd_nowait(struct audio_client *ac, int cmd)
{
	pr_debug("%s: stream_id: %d\n", __func__, ac->stream_id);
	return __q6asm_cmd_nowait(ac, cmd, ac->stream_id);
}

int q6asm_stream_cmd_nowait(struct audio_client *ac, int cmd,
			    uint32_t stream_id)
{
	pr_debug("%s: stream_id: %d\n", __func__, stream_id);
	return __q6asm_cmd_nowait(ac, cmd, stream_id);
}

int __q6asm_send_meta_data(struct audio_client *ac, uint32_t stream_id,
			  uint32_t initial_samples, uint32_t trailing_samples)
{
	struct asm_data_cmd_remove_silence silence;
	int rc = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s session[%d]", __func__, ac->session);
	q6asm_stream_add_hdr_async(ac, &silence.hdr, sizeof(silence), FALSE,
				  stream_id);

	/*
	 * Updated the token field with stream/session for compressed playback
	 * Platform driver must know the the stream with which the command is
	 * associated
	 */
	if (ac->io_mode & COMPRESSED_STREAM_IO)
		silence.hdr.token = ((ac->session << 8) & 0xFFFF00) |
				    (stream_id & 0xFF);
	pr_debug("%s: token = 0x%x, stream_id  %d, session 0x%x\n",
	      __func__, silence.hdr.token, stream_id, ac->session);

	silence.hdr.opcode = ASM_DATA_CMD_REMOVE_INITIAL_SILENCE;
	silence.num_samples_to_remove    = initial_samples;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &silence);
	if (rc < 0) {
		pr_err("Commmand silence failed[%d]", rc);
		goto fail_cmd;
	}

	silence.hdr.opcode = ASM_DATA_CMD_REMOVE_TRAILING_SILENCE;
	silence.num_samples_to_remove    = trailing_samples;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &silence);
	if (rc < 0) {
		pr_err("Commmand silence failed[%d]", rc);
		goto fail_cmd;
	}

	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_stream_send_meta_data(struct audio_client *ac, uint32_t stream_id,
		uint32_t initial_samples, uint32_t trailing_samples)
{
	return __q6asm_send_meta_data(ac, stream_id, initial_samples,
				     trailing_samples);
}

int q6asm_send_meta_data(struct audio_client *ac, uint32_t initial_samples,
		uint32_t trailing_samples)
{
	return __q6asm_send_meta_data(ac, ac->stream_id, initial_samples,
				     trailing_samples);
}

static void q6asm_reset_buf_state(struct audio_client *ac)
{
	int cnt = 0;
	int loopcnt = 0;
	int used;
	struct audio_port_data *port = NULL;

	if (ac->io_mode & SYNC_IO_MODE) {
		used = (ac->io_mode & TUN_WRITE_IO_MODE ? 1 : 0);
		mutex_lock(&ac->cmd_lock);
		for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
			port = &ac->port[loopcnt];
			cnt = port->max_buf_cnt - 1;
			port->dsp_buf = 0;
			port->cpu_buf = 0;
			while (cnt >= 0) {
				if (!port->buf)
					continue;
				port->buf[cnt].used = used;
				cnt--;
			}
		}
		mutex_unlock(&ac->cmd_lock);
	}
}

int q6asm_reg_tx_overflow(struct audio_client *ac, uint16_t enable)
{
	struct asm_session_cmd_regx_overflow tx_overflow;
	int rc;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s:session[%d]enable[%d]\n", __func__,
						ac->session, enable);
	q6asm_add_hdr(ac, &tx_overflow.hdr, sizeof(tx_overflow), TRUE);
	atomic_set(&ac->cmd_state, 1);

	tx_overflow.hdr.opcode = \
			ASM_SESSION_CMD_REGISTER_FORX_OVERFLOW_EVENTS;
	/* tx overflow event: enable */
	tx_overflow.enable_flag = enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &tx_overflow);
	if (rc < 0) {
		pr_err("tx overflow op[0x%x]rc[%d]\n", \
						tx_overflow.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
				(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for tx overflow\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_reg_rx_underflow(struct audio_client *ac, uint16_t enable)
{
	struct asm_session_cmd_rgstr_rx_underflow rx_underflow;
	int rc;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s:session[%d]enable[%d]\n", __func__,
						ac->session, enable);
	q6asm_add_hdr_async(ac, &rx_underflow.hdr, sizeof(rx_underflow), FALSE);

	rx_underflow.hdr.opcode = \
			ASM_SESSION_CMD_REGISTER_FOR_RX_UNDERFLOW_EVENTS;
	/* tx overflow event: enable */
	rx_underflow.enable_flag = enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &rx_underflow);
	if (rc < 0) {
		pr_err("tx overflow op[0x%x]rc[%d]\n", \
						rx_underflow.hdr.opcode, rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_get_apr_service_id(int session_id)
{
	pr_debug("%s\n", __func__);

	if (session_id <= 0 || session_id > SESSION_MAX) {
		pr_err("%s: invalid session_id = %d\n", __func__, session_id);
		return -EINVAL;
	}

	return ((struct apr_svc *)session[session_id]->apr)->id;
}


static int q6asm_is_valid_session(struct apr_client_data *data, void *priv)
{

	struct audio_client *ac = (struct audio_client *)priv;
	uint32_t token = data->token;

	/*
	 * Some commands for compressed playback has token as session and
	 * other commands has session|stream. Check for both conditions
	 * before deciding if the callback was for a invalud session.
	 */
	if (ac->io_mode & COMPRESSED_STREAM_IO) {
		if ((token & 0xFFFFFF00) != ((ac->session << 8) & 0xFFFFFF00)
						 && (token != ac->session)) {
			pr_err("%s:Invalid compr session[%d] rxed expected[%d]",
				 __func__, token, ac->session);
			return -EINVAL;
		}
	} else {
		if (token != ac->session) {
			pr_err("%s:Invalid session[%d] rxed expected[%d]",
				__func__, token, ac->session);
			return -EINVAL;
		}
	}
	return 0;
}

static int __init q6asm_init(void)
{
	int lcnt;
	pr_debug("%s\n", __func__);

	memset(session, 0, sizeof(session));
	set_custom_topology = 1;

	/*setup common client used for cal mem map */
	common_client.session = ASM_CONTROL_SESSION;
	common_client.port[0].buf = &common_buf[0];
	common_client.port[1].buf = &common_buf[1];
	init_waitqueue_head(&common_client.cmd_wait);
	init_waitqueue_head(&common_client.time_wait);
	init_waitqueue_head(&common_client.mem_wait);
	atomic_set(&common_client.time_flag, 1);
	INIT_LIST_HEAD(&common_client.port[0].mem_map_handle);
	INIT_LIST_HEAD(&common_client.port[1].mem_map_handle);
	mutex_init(&common_client.cmd_lock);
	for (lcnt = 0; lcnt <= OUT; lcnt++) {
		mutex_init(&common_client.port[lcnt].lock);
		spin_lock_init(&common_client.port[lcnt].dsp_lock);
	}
	atomic_set(&common_client.cmd_state, 0);
	atomic_set(&common_client.nowait_cmd_cnt, 0);
	atomic_set(&common_client.mem_state, 0);

	config_debug_fs_init();

	return 0;
}

device_initcall(q6asm_init);

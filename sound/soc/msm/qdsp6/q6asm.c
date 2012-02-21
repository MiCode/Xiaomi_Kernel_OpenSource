
/*
 * Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/android_pmem.h>
#include <linux/memory_alloc.h>
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/atomic.h>

#include <asm/ioctls.h>

#include <mach/memory.h>
#include <mach/debug_mm.h>
#include <mach/peripheral-loader.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include <mach/qdsp6v2/rtac.h>
#include <mach/msm_subsystem_map.h>

#include <sound/apr_audio.h>
#include <sound/q6asm.h>


#define TRUE        0x01
#define FALSE       0x00
#define READDONE_IDX_STATUS 0
#define READDONE_IDX_BUFFER 1
#define READDONE_IDX_SIZE 2
#define READDONE_IDX_OFFSET 3
#define READDONE_IDX_MSW_TS 4
#define READDONE_IDX_LSW_TS 5
#define READDONE_IDX_FLAGS 6
#define READDONE_IDX_NUMFRAMES 7
#define READDONE_IDX_ID 8
#ifdef CONFIG_DEBUG_FS
#define OUT_BUFFER_SIZE 56
#define IN_BUFFER_SIZE 24
#endif
static DEFINE_MUTEX(session_lock);

/* session id: 0 reserved */
static struct audio_client *session[SESSION_MAX+1];
static int32_t q6asm_mmapcallback(struct apr_client_data *data, void *priv);
static int32_t q6asm_callback(struct apr_client_data *data, void *priv);
static void q6asm_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg);
static void q6asm_add_hdr_async(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg);
static int q6asm_memory_map_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt);
static int q6asm_memory_unmap_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt);

static void q6asm_reset_buf_state(struct audio_client *ac);

#ifdef CONFIG_DEBUG_FS
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
static int audio_output_latency_dbgfs_open(struct inode *inode,
							struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
static ssize_t audio_output_latency_dbgfs_read(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
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
		if (!strict_strtol(temp, 10, &out_enable_flag)) {
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
		if (!strict_strtol(temp, 10, &in_enable_flag)) {
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
#endif
struct asm_mmap {
	atomic_t ref_cnt;
	atomic_t cmd_state;
	wait_queue_head_t cmd_wait;
	void *apr;
};

static struct asm_mmap this_mmap;

static int q6asm_session_alloc(struct audio_client *ac)
{
	int n;
	mutex_lock(&session_lock);
	for (n = 1; n <= SESSION_MAX; n++) {
		if (!session[n]) {
			session[n] = ac;
			mutex_unlock(&session_lock);
			return n;
		}
	}
	mutex_unlock(&session_lock);
	return -ENOMEM;
}

static void q6asm_session_free(struct audio_client *ac)
{
	pr_debug("%s: sessionid[%d]\n", __func__, ac->session);
	rtac_remove_popp_from_adm_devices(ac->session);
	mutex_lock(&session_lock);
	session[ac->session] = 0;
	mutex_unlock(&session_lock);
	ac->session = 0;
	return;
}

int q6asm_audio_client_buf_free(unsigned int dir,
			struct audio_client *ac)
{
	struct audio_port_data *port;
	int cnt = 0;
	int rc = 0;
	pr_debug("%s: Session id %d\n", __func__, ac->session);
	mutex_lock(&ac->cmd_lock);
	if (ac->io_mode == SYNC_IO_MODE) {
		port = &ac->port[dir];
		if (!port->buf) {
			mutex_unlock(&ac->cmd_lock);
			return 0;
		}
		cnt = port->max_buf_cnt - 1;

		if (cnt >= 0) {
			rc = q6asm_memory_unmap_regions(ac, dir,
							port->buf[0].size,
							port->max_buf_cnt);
			if (rc < 0)
				pr_err("%s CMD Memory_unmap_regions failed\n",
								__func__);
		}

		while (cnt >= 0) {
			if (port->buf[cnt].data) {
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
				ion_unmap_kernel(port->buf[cnt].client,
						port->buf[cnt].handle);
				ion_free(port->buf[cnt].client,
						port->buf[cnt].handle);
				ion_client_destroy(port->buf[cnt].client);
#else
				pr_debug("%s:data[%p]phys[%p][%p] cnt[%d]"
					 "mem_buffer[%p]\n",
					__func__, (void *)port->buf[cnt].data,
					   (void *)port->buf[cnt].phys,
					   (void *)&port->buf[cnt].phys, cnt,
					   (void *)port->buf[cnt].mem_buffer);
				if (IS_ERR((void *)port->buf[cnt].mem_buffer))
					pr_err("%s:mem buffer invalid, error ="
						 "%ld\n", __func__,
				PTR_ERR((void *)port->buf[cnt].mem_buffer));
				else {
					if (msm_subsystem_unmap_buffer(
						port->buf[cnt].mem_buffer) < 0)
						pr_err("%s: unmap buffer"
							" failed\n", __func__);
				}
				free_contiguous_memory_by_paddr(
					port->buf[cnt].phys);

#endif
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
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		ion_unmap_kernel(port->buf[0].client, port->buf[0].handle);
		ion_free(port->buf[0].client, port->buf[0].handle);
		ion_client_destroy(port->buf[0].client);
		pr_debug("%s:data[%p]phys[%p][%p]"
			", client[%p] handle[%p]\n",
			__func__,
			(void *)port->buf[0].data,
			(void *)port->buf[0].phys,
			(void *)&port->buf[0].phys,
			(void *)port->buf[0].client,
			(void *)port->buf[0].handle);
#else
		pr_debug("%s:data[%p]phys[%p][%p]"
			"mem_buffer[%p]\n",
			__func__,
			(void *)port->buf[0].data,
			(void *)port->buf[0].phys,
			(void *)&port->buf[0].phys,
			(void *)port->buf[0].mem_buffer);
		if (IS_ERR((void *)port->buf[0].mem_buffer))
			pr_err("%s:mem buffer invalid, error ="
				"%ld\n", __func__,
				PTR_ERR((void *)port->buf[0].mem_buffer));
		else {
			if (msm_subsystem_unmap_buffer(
				port->buf[0].mem_buffer) < 0)
				pr_err("%s: unmap buffer"
					" failed\n", __func__);
		}
		free_contiguous_memory_by_paddr(port->buf[0].phys);
#endif
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
	pr_debug("%s: Session id %d\n", __func__, ac->session);
	if (ac->io_mode == SYNC_IO_MODE) {
		for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
			port = &ac->port[loopcnt];
			if (!port->buf)
				continue;
			pr_debug("%s:loopcnt = %d\n", __func__, loopcnt);
			q6asm_audio_client_buf_free(loopcnt, ac);
		}
	}

	apr_deregister(ac->apr);
	q6asm_session_free(ac);

	pr_debug("%s: APR De-Register\n", __func__);
	if (atomic_read(&this_mmap.ref_cnt) <= 0) {
		pr_err("%s: APR Common Port Already Closed\n", __func__);
		goto done;
	}

	atomic_dec(&this_mmap.ref_cnt);
	if (atomic_read(&this_mmap.ref_cnt) == 0) {
		apr_deregister(this_mmap.apr);
		pr_debug("%s:APR De-Register common port\n", __func__);
	}
done:
	kfree(ac);
	return;
}

int q6asm_set_io_mode(struct audio_client *ac, uint32_t mode)
{
	if (ac == NULL) {
		pr_err("%s APR handle NULL\n", __func__);
		return -EINVAL;
	}
	if ((mode == ASYNC_IO_MODE) || (mode == SYNC_IO_MODE)) {
		ac->io_mode = mode;
		pr_debug("%s:Set Mode to %d\n", __func__, ac->io_mode);
		return 0;
	} else {
		pr_err("%s:Not an valid IO Mode:%d\n", __func__, ac->io_mode);
		return -EINVAL;
	}
}

struct audio_client *q6asm_audio_client_alloc(app_cb cb, void *priv)
{
	struct audio_client *ac;
	int n;
	int lcnt = 0;

	ac = kzalloc(sizeof(struct audio_client), GFP_KERNEL);
	if (!ac)
		return NULL;
	n = q6asm_session_alloc(ac);
	if (n <= 0)
		goto fail_session;
	ac->session = n;
	ac->cb = cb;
	ac->priv = priv;
	ac->io_mode = SYNC_IO_MODE;
	ac->apr = apr_register("ADSP", "ASM", \
				(apr_fn)q6asm_callback,\
				((ac->session) << 8 | 0x0001),\
				ac);

	if (ac->apr == NULL) {
		pr_err("%s Registration with APR failed\n", __func__);
			goto fail;
	}
	rtac_set_asm_handle(n, ac->apr);

	pr_debug("%s Registering the common port with APR\n", __func__);
	if (atomic_read(&this_mmap.ref_cnt) == 0) {
		this_mmap.apr = apr_register("ADSP", "ASM", \
					(apr_fn)q6asm_mmapcallback,\
					0x0FFFFFFFF, &this_mmap);
		if (this_mmap.apr == NULL) {
			pr_debug("%s Unable to register \
				APR ASM common port \n", __func__);
			goto fail;
		}
	}

	atomic_inc(&this_mmap.ref_cnt);
	init_waitqueue_head(&ac->cmd_wait);
	init_waitqueue_head(&ac->time_wait);
	atomic_set(&ac->time_flag, 1);
	mutex_init(&ac->cmd_lock);
	for (lcnt = 0; lcnt <= OUT; lcnt++) {
		mutex_init(&ac->port[lcnt].lock);
		spin_lock_init(&ac->port[lcnt].dsp_lock);
	}
	atomic_set(&ac->cmd_state, 0);

	pr_debug("%s: session[%d]\n", __func__, ac->session);

	return ac;
fail:
	q6asm_audio_client_free(ac);
	return NULL;
fail_session:
	kfree(ac);
	return NULL;
}

struct audio_client *q6asm_get_audio_client(int session_id)
{
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
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	int len;
#endif

	if (!(ac) || ((dir != IN) && (dir != OUT)))
		return -EINVAL;

	pr_debug("%s: session[%d]bufsz[%d]bufcnt[%d]\n", __func__, ac->session,
		bufsz, bufcnt);

	if (ac->session <= 0 || ac->session > 8)
		goto fail;

	if (ac->io_mode == SYNC_IO_MODE) {
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
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
					buf[cnt].client = msm_ion_client_create
						(UINT_MAX, "audio_client");
					if (IS_ERR_OR_NULL((void *)
						buf[cnt].client)) {
						pr_err("%s: ION create client"
						" for AUDIO failed\n",
						__func__);
						goto fail;
					}
					buf[cnt].handle = ion_alloc
						(buf[cnt].client, bufsz, SZ_4K,
						(0x1 << ION_AUDIO_HEAP_ID));
					if (IS_ERR_OR_NULL((void *)
						buf[cnt].handle)) {
						pr_err("%s: ION memory"
					" allocation for AUDIO failed\n",
							__func__);
						goto fail;
					}

					rc = ion_phys(buf[cnt].client,
						buf[cnt].handle,
						(ion_phys_addr_t *)
						&buf[cnt].phys,
						(size_t *)&len);
					if (rc) {
						pr_err("%s: ION Get Physical"
						" for AUDIO failed, rc = %d\n",
							__func__, rc);
						goto fail;
					}

					buf[cnt].data = ion_map_kernel
					(buf[cnt].client, buf[cnt].handle,
							 0);
					if (IS_ERR_OR_NULL((void *)
						buf[cnt].data)) {
						pr_err("%s: ION memory"
				" mapping for AUDIO failed\n", __func__);
						goto fail;
					}
					memset((void *)buf[cnt].data, 0, bufsz);
#else
					unsigned int flags = 0;
					buf[cnt].phys =
					allocate_contiguous_ebi_nomap(bufsz,
						SZ_4K);
					if (!buf[cnt].phys) {
						pr_err("%s:Buf alloc failed "
						" size=%d\n", __func__,
						bufsz);
						mutex_unlock(&ac->cmd_lock);
						goto fail;
					}
					flags = MSM_SUBSYSTEM_MAP_KADDR |
						MSM_SUBSYSTEM_MAP_CACHED;
					buf[cnt].mem_buffer =
					msm_subsystem_map_buffer(buf[cnt].phys,
						bufsz, flags, NULL, 0);
					if (IS_ERR(
						(void *)buf[cnt].mem_buffer)) {
						pr_err("%s:map_buffer failed,"
							"error = %ld\n",
				__func__, PTR_ERR((void *)buf[cnt].mem_buffer));
						mutex_unlock(&ac->cmd_lock);
						goto fail;
					}
					buf[cnt].data =
						buf[cnt].mem_buffer->vaddr;
					if (!buf[cnt].data) {
						pr_err("%s:invalid vaddr,"
						" iomap failed\n", __func__);
						mutex_unlock(&ac->cmd_lock);
						goto fail;
					}
#endif
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
		rc = q6asm_memory_map_regions(ac, dir, bufsz, cnt);
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
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	int len;
#else
	int flags = 0;
#endif
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

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	buf[0].client = msm_ion_client_create(UINT_MAX, "audio_client");
	if (IS_ERR_OR_NULL((void *)buf[0].client)) {
		pr_err("%s: ION create client for AUDIO failed\n", __func__);
		goto fail;
	}
	buf[0].handle = ion_alloc(buf[0].client, bufsz * bufcnt, SZ_4K,
				  (0x1 << ION_AUDIO_HEAP_ID));
	if (IS_ERR_OR_NULL((void *) buf[0].handle)) {
		pr_err("%s: ION memory allocation for AUDIO failed\n",
			__func__);
		goto fail;
	}

	rc = ion_phys(buf[0].client, buf[0].handle,
		  (ion_phys_addr_t *)&buf[0].phys, (size_t *)&len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto fail;
	}

	buf[0].data = ion_map_kernel(buf[0].client, buf[0].handle, 0);
	if (IS_ERR_OR_NULL((void *) buf[0].data)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		goto fail;
	}
	memset((void *)buf[0].data, 0, (bufsz * bufcnt));
#else
	buf[0].phys = allocate_contiguous_ebi_nomap(bufsz * bufcnt,
						SZ_4K);
	if (!buf[0].phys) {
		pr_err("%s:Buf alloc failed "
			" size=%d, bufcnt=%d\n", __func__,
			bufsz, bufcnt);
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}

	flags = MSM_SUBSYSTEM_MAP_KADDR | MSM_SUBSYSTEM_MAP_CACHED;
	buf[0].mem_buffer = msm_subsystem_map_buffer(buf[0].phys,
				bufsz * bufcnt, flags, NULL, 0);
	if (IS_ERR((void *)buf[cnt].mem_buffer)) {
		pr_err("%s:map_buffer failed,"
			"error = %ld\n",
			__func__, PTR_ERR((void *)buf[0].mem_buffer));

		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}
	buf[0].data = buf[0].mem_buffer->vaddr;
#endif
	if (!buf[0].data) {
		pr_err("%s:invalid vaddr,"
			" iomap failed\n", __func__);
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
	rc = q6asm_memory_map(ac, buf[0].phys, dir, bufsz, cnt);
	if (rc < 0) {
		pr_err("%s:CMD Memory_map_regions failed\n", __func__);
		goto fail;
	}
	return 0;
fail:
	q6asm_audio_client_buf_free_contiguous(dir, ac);
	return -EINVAL;
}

static int32_t q6asm_mmapcallback(struct apr_client_data *data, void *priv)
{
	uint32_t token;
	uint32_t *payload = data->payload;

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: Reset event is received: %d %d apr[%p]\n",
				__func__,
				data->reset_event,
				data->reset_proc,
				this_mmap.apr);
		apr_reset(this_mmap.apr);
		this_mmap.apr = NULL;
		atomic_set(&this_mmap.cmd_state, 0);
		return 0;
	}

	pr_debug("%s:ptr0[0x%x]ptr1[0x%x]opcode[0x%x]"
		"token[0x%x]payload_s[%d] src[%d] dest[%d]\n", __func__,
		payload[0], payload[1], data->opcode, data->token,
		data->payload_size, data->src_port, data->dest_port);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		token = data->token;
		switch (payload[0]) {
		case ASM_SESSION_CMD_MEMORY_MAP:
		case ASM_SESSION_CMD_MEMORY_UNMAP:
		case ASM_SESSION_CMD_MEMORY_MAP_REGIONS:
		case ASM_SESSION_CMD_MEMORY_UNMAP_REGIONS:
			pr_debug("%s:command[0x%x]success [0x%x]\n",
					__func__, payload[0], payload[1]);
			if (atomic_read(&this_mmap.cmd_state)) {
				atomic_set(&this_mmap.cmd_state, 0);
				wake_up(&this_mmap.cmd_wait);
			}
			break;
		default:
			pr_debug("%s:command[0x%x] not expecting rsp\n",
						__func__, payload[0]);
			break;
		}
	}
	return 0;
}


static int32_t q6asm_callback(struct apr_client_data *data, void *priv)
{
	int i = 0;
	struct audio_client *ac = (struct audio_client *)priv;
	uint32_t token;
	unsigned long dsp_flags;
	uint32_t *payload;


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

	if (data->opcode == RESET_EVENTS) {
		pr_debug("q6asm_callback: Reset event is received: %d %d apr[%p]\n",
				data->reset_event, data->reset_proc, ac->apr);
			if (ac->cb)
				ac->cb(data->opcode, data->token,
					(uint32_t *)data->payload, ac->priv);
		apr_reset(ac->apr);
		return 0;
	}

	pr_debug("%s: session[%d]opcode[0x%x] \
		token[0x%x]payload_s[%d] src[%d] dest[%d]\n", __func__,
		ac->session, data->opcode,
		data->token, data->payload_size, data->src_port,
		data->dest_port);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		token = data->token;
		switch (payload[0]) {
		case ASM_STREAM_CMD_SET_PP_PARAMS:
			if (rtac_make_asm_callback(ac->session, payload,
					data->payload_size))
				break;
		case ASM_SESSION_CMD_PAUSE:
		case ASM_DATA_CMD_EOS:
		case ASM_STREAM_CMD_CLOSE:
		case ASM_STREAM_CMD_FLUSH:
		case ASM_SESSION_CMD_RUN:
		case ASM_SESSION_CMD_REGISTER_FOR_TX_OVERFLOW_EVENTS:
		case ASM_STREAM_CMD_FLUSH_READBUFS:
		pr_debug("%s:Payload = [0x%x]\n", __func__, payload[0]);
		if (token != ac->session) {
			pr_err("%s:Invalid session[%d] rxed expected[%d]",
					__func__, token, ac->session);
			return -EINVAL;
		}
		case ASM_STREAM_CMD_OPEN_READ:
		case ASM_STREAM_CMD_OPEN_WRITE:
		case ASM_STREAM_CMD_OPEN_READWRITE:
		case ASM_DATA_CMD_MEDIA_FORMAT_UPDATE:
		case ASM_STREAM_CMD_SET_ENCDEC_PARAM:
			if (atomic_read(&ac->cmd_state)) {
				atomic_set(&ac->cmd_state, 0);
				wake_up(&ac->cmd_wait);
			}
			if (ac->cb)
				ac->cb(data->opcode, data->token,
					(uint32_t *)data->payload, ac->priv);
			break;
		default:
			pr_debug("%s:command[0x%x] not expecting rsp\n",
							__func__, payload[0]);
			break;
		}
		return 0;
	}

	switch (data->opcode) {
	case ASM_DATA_EVENT_WRITE_DONE:{
		struct audio_port_data *port = &ac->port[IN];
		pr_debug("%s: Rxed opcode[0x%x] status[0x%x] token[%d]",
				__func__, payload[0], payload[1],
				data->token);
		if (ac->io_mode == SYNC_IO_MODE) {
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
#ifdef CONFIG_DEBUG_FS
			if (out_enable_flag) {
				/* For first Write done log the time and reset
				   out_cold_index*/
				if (out_cold_index != 1) {
					do_gettimeofday(&out_cold_tv);
					pr_debug("COLD: apr_send_pkt at %ld \
					sec %ld microsec\n",\
					out_cold_tv.tv_sec,\
					out_cold_tv.tv_usec);
					out_cold_index = 1;
				}
				pr_debug("out_enable_flag %ld",\
					out_enable_flag);
			}
#endif
			for (i = 0; i < port->max_buf_cnt; i++)
				pr_debug("%d ", port->buf[i].used);

		}
		break;
	}
	case ASM_STREAM_CMDRSP_GET_PP_PARAMS:
		rtac_make_asm_callback(ac->session, payload,
			data->payload_size);
		break;
	case ASM_DATA_EVENT_READ_DONE:{

		struct audio_port_data *port = &ac->port[OUT];
#ifdef CONFIG_DEBUG_FS
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
				pr_err("In_CONT:previous read buffer done \
				at %ld sec %ld microsec\n",\
				in_cont_tv.tv_sec, in_cont_tv.tv_usec);
			}
		}
#endif
		pr_debug("%s:R-D: status=%d buff_add=%x act_size=%d offset=%d\n",
				__func__, payload[READDONE_IDX_STATUS],
				payload[READDONE_IDX_BUFFER],
				payload[READDONE_IDX_SIZE],
				payload[READDONE_IDX_OFFSET]);
		pr_debug("%s:R-D:msw_ts=%d lsw_ts=%d flags=%d id=%d num=%d\n",
				__func__, payload[READDONE_IDX_MSW_TS],
				payload[READDONE_IDX_LSW_TS],
				payload[READDONE_IDX_FLAGS],
				payload[READDONE_IDX_ID],
				payload[READDONE_IDX_NUMFRAMES]);
#ifdef CONFIG_DEBUG_FS
		if (in_enable_flag) {
			in_cont_index++;
		}
#endif
		if (ac->io_mode == SYNC_IO_MODE) {
			if (port->buf == NULL) {
				pr_err("%s: Unexpected Write Done\n", __func__);
				return -EINVAL;
			}
			spin_lock_irqsave(&port->dsp_lock, dsp_flags);
			token = data->token;
			port->buf[token].used = 0;
			if (port->buf[token].phys !=
				payload[READDONE_IDX_BUFFER]) {
				pr_err("Buf expected[%p]rxed[%p]\n",\
				   (void *)port->buf[token].phys,\
				   (void *)payload[READDONE_IDX_BUFFER]);
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
	case ASM_DATA_CMDRSP_EOS:
		pr_debug("%s:EOS ACK received: rxed opcode[0x%x]\n",
				  __func__, data->opcode);
		break;
	case ASM_STREAM_CMDRSP_GET_ENCDEC_PARAM:
		break;
	case ASM_SESSION_EVENT_TX_OVERFLOW:
		pr_err("ASM_SESSION_EVENT_TX_OVERFLOW\n");
		break;
	case ASM_SESSION_CMDRSP_GET_SESSION_TIME:
		pr_debug("%s: ASM_SESSION_CMDRSP_GET_SESSION_TIME, "
				"payload[0] = %d, payload[1] = %d, "
				"payload[2] = %d\n", __func__,
				 payload[0], payload[1], payload[2]);
		ac->time_stamp = (uint64_t)(((uint64_t)payload[1] << 32) |
				payload[2]);
		if (atomic_read(&ac->time_flag)) {
			atomic_set(&ac->time_flag, 0);
			wake_up(&ac->time_wait);
		}
		break;
	case ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY:
	case ASM_DATA_EVENT_ENC_SR_CM_NOTIFY:
		pr_debug("%s: ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY, "
				"payload[0] = %d, payload[1] = %d, "
				"payload[2] = %d, payload[3] = %d\n", __func__,
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

	if (ac->io_mode == SYNC_IO_MODE) {
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
			pr_debug("%s:Next buf idx[0x%x] not available,\
				dir[%d]\n", __func__, idx, dir);
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
		pr_debug("%s:Next buf idx[0x%x] not available,\
			dir[%d]\n", __func__, idx, dir);
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

	if (ac->io_mode == SYNC_IO_MODE) {
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

static void q6asm_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg)
{
	pr_debug("%s:session=%d pkt size=%d cmd_flg=%d\n", __func__, pkt_size,
		cmd_flg, ac->session);
	mutex_lock(&ac->cmd_lock);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(sizeof(struct apr_hdr)),\
				APR_PKT_VER);
	hdr->src_svc = ((struct apr_svc *)ac->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_ASM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = ((ac->session << 8) & 0xFF00) | 0x01;
	hdr->dest_port = ((ac->session << 8) & 0xFF00) | 0x01;
	if (cmd_flg) {
		hdr->token = ac->session;
		atomic_set(&ac->cmd_state, 1);
	}
	hdr->pkt_size  = pkt_size;
	mutex_unlock(&ac->cmd_lock);
	return;
}

static void q6asm_add_mmaphdr(struct apr_hdr *hdr, uint32_t pkt_size,
							uint32_t cmd_flg)
{
	pr_debug("%s:pkt size=%d cmd_flg=%d\n", __func__, pkt_size, cmd_flg);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr->src_port = 0;
	hdr->dest_port = 0;
	if (cmd_flg) {
		hdr->token = 0;
		atomic_set(&this_mmap.cmd_state, 1);
	}
	hdr->pkt_size  = pkt_size;
	return;
}

int q6asm_open_read(struct audio_client *ac,
		uint32_t format)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_read open;
#ifdef CONFIG_DEBUG_FS
	in_cont_index = 0;
#endif
	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s:session[%d]", __func__, ac->session);

	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_READ;
	/* Stream prio : High, provide meta info with encoded frames */
	open.src_endpoint = ASM_END_POINT_DEVICE_MATRIX;

	open.pre_proc_top = get_asm_topology();
	if (open.pre_proc_top == 0)
		open.pre_proc_top = DEFAULT_POPP_TOPOLOGY;

	switch (format) {
	case FORMAT_LINEAR_PCM:
		open.uMode = STREAM_PRIORITY_HIGH;
		open.format = LINEAR_PCM;
		break;
	case FORMAT_MPEG4_AAC:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = MPEG4_AAC;
		break;
	case FORMAT_V13K:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = V13K_FS;
		break;
	case FORMAT_EVRC:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = EVRC_FS;
		break;
	case FORMAT_AMRNB:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = AMRNB_FS;
		break;
	case FORMAT_AMRWB:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = AMRWB_FS;
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
		pr_err("%s: timeout. waited for OPEN_WRITE rc[%d]\n", __func__,
			rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_open_write(struct audio_client *ac, uint32_t format)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_write open;

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: session[%d] wr_format[0x%x]", __func__, ac->session,
		format);

	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);

	open.hdr.opcode = ASM_STREAM_CMD_OPEN_WRITE;
	open.uMode = STREAM_PRIORITY_HIGH;
	/* source endpoint : matrix */
	open.sink_endpoint = ASM_END_POINT_DEVICE_MATRIX;
	open.stream_handle = 0x00;

	open.post_proc_top = get_asm_topology();
	if (open.post_proc_top == 0)
		open.post_proc_top = DEFAULT_POPP_TOPOLOGY;

	switch (format) {
	case FORMAT_LINEAR_PCM:
		open.format = LINEAR_PCM;
		break;
	case FORMAT_MULTI_CHANNEL_LINEAR_PCM:
		open.format = MULTI_CHANNEL_PCM;
		break;
	case FORMAT_MPEG4_AAC:
		open.format = MPEG4_AAC;
		break;
	case FORMAT_MPEG4_MULTI_AAC:
		open.format = MPEG4_MULTI_AAC;
		break;
	case FORMAT_WMA_V9:
		open.format = WMA_V9;
		break;
	case FORMAT_WMA_V10PRO:
		open.format = WMA_V10PRO;
		break;
	case FORMAT_MP3:
		open.format = MP3;
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
		pr_err("%s: timeout. waited for OPEN_WRITE rc[%d]\n", __func__,
			rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_open_read_write(struct audio_client *ac,
			uint32_t rd_format,
			uint32_t wr_format)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_read_write open;

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: session[%d]", __func__, ac->session);
	pr_debug("wr_format[0x%x]rd_format[0x%x]",
				wr_format, rd_format);

	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_READWRITE;

	open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_NORMAL;
	/* source endpoint : matrix */
	open.post_proc_top = get_asm_topology();
	if (open.post_proc_top == 0)
		open.post_proc_top = DEFAULT_POPP_TOPOLOGY;

	switch (wr_format) {
	case FORMAT_LINEAR_PCM:
		open.write_format = LINEAR_PCM;
		break;
	case FORMAT_MPEG4_AAC:
		open.write_format = MPEG4_AAC;
		break;
	case FORMAT_MPEG4_MULTI_AAC:
		open.write_format = MPEG4_MULTI_AAC;
		break;
	case FORMAT_WMA_V9:
		open.write_format = WMA_V9;
		break;
	case FORMAT_WMA_V10PRO:
		open.write_format = WMA_V10PRO;
		break;
	case FORMAT_AMRNB:
		open.write_format = AMRNB_FS;
		break;
	case FORMAT_AMRWB:
		open.write_format = AMRWB_FS;
		break;
	case FORMAT_V13K:
		open.write_format = V13K_FS;
		break;
	case FORMAT_EVRC:
		open.write_format = EVRC_FS;
		break;
	case FORMAT_EVRCB:
		open.write_format = EVRCB_FS;
		break;
	case FORMAT_EVRCWB:
		open.write_format = EVRCWB_FS;
		break;
	case FORMAT_MP3:
		open.write_format = MP3;
		break;
	default:
		pr_err("Invalid format[%d]\n", wr_format);
		goto fail_cmd;
	}

	switch (rd_format) {
	case FORMAT_LINEAR_PCM:
		open.read_format = LINEAR_PCM;
		break;
	case FORMAT_MPEG4_AAC:
		open.read_format = MPEG4_AAC;
		break;
	case FORMAT_V13K:
		open.read_format = V13K_FS;
		break;
	case FORMAT_EVRC:
		open.read_format = EVRC_FS;
		break;
	case FORMAT_AMRNB:
		open.read_format = AMRNB_FS;
		break;
	case FORMAT_AMRWB:
		open.read_format = AMRWB_FS;
		break;
	default:
		pr_err("Invalid format[%d]\n", rd_format);
		goto fail_cmd;
	}
	pr_debug("%s:rdformat[0x%x]wrformat[0x%x]\n", __func__,
			open.read_format, open.write_format);

	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("open failed op[0x%x]rc[%d]\n", \
						open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for OPEN_WRITE rc[%d]\n", rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_run(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts)
{
	struct asm_stream_cmd_run run;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s session[%d]", __func__, ac->session);
	q6asm_add_hdr(ac, &run.hdr, sizeof(run), TRUE);

	run.hdr.opcode = ASM_SESSION_CMD_RUN;
	run.flags    = flags;
	run.msw_ts   = msw_ts;
	run.lsw_ts   = lsw_ts;
#ifdef CONFIG_DEBUG_FS
	if (out_enable_flag) {
		do_gettimeofday(&out_cold_tv);
		pr_debug("COLD: apr_send_pkt at %ld sec %ld microsec\n",\
				out_cold_tv.tv_sec, out_cold_tv.tv_usec);
	}
#endif
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

int q6asm_run_nowait(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts)
{
	struct asm_stream_cmd_run run;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("%s:APR handle NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("session[%d]", ac->session);
	q6asm_add_hdr_async(ac, &run.hdr, sizeof(run), TRUE);

	run.hdr.opcode = ASM_SESSION_CMD_RUN;
	run.flags    = flags;
	run.msw_ts   = msw_ts;
	run.lsw_ts   = lsw_ts;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &run);
	if (rc < 0) {
		pr_err("%s:Commmand run failed[%d]", __func__, rc);
		return -EINVAL;
	}
	return 0;
}


int q6asm_enc_cfg_blk_aac(struct audio_client *ac,
			 uint32_t frames_per_buf,
			uint32_t sample_rate, uint32_t channels,
			uint32_t bit_rate, uint32_t mode, uint32_t format)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]SR[%d]ch[%d]bitrate[%d]mode[%d]"
		"format[%d]", __func__, ac->session, frames_per_buf,
		sample_rate, channels, bit_rate, mode, format);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);
	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = MPEG4_AAC;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_aac_read_cfg);
	enc_cfg.enc_blk.cfg.aac.bitrate = bit_rate;
	enc_cfg.enc_blk.cfg.aac.enc_mode = mode;
	enc_cfg.enc_blk.cfg.aac.format = format;
	enc_cfg.enc_blk.cfg.aac.ch_cfg = channels;
	enc_cfg.enc_blk.cfg.aac.sample_rate = sample_rate;

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

int q6asm_enc_cfg_blk_pcm(struct audio_client *ac,
			uint32_t rate, uint32_t channels)
{
	struct asm_stream_cmd_encdec_cfg_blk  enc_cfg;

	int rc = 0;

	pr_debug("%s: Session %d, rate = %d, channels = %d\n", __func__,
			 ac->session, rate, channels);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);
	enc_cfg.enc_blk.frames_per_buf = 1;
	enc_cfg.enc_blk.format_id = LINEAR_PCM;
	enc_cfg.enc_blk.cfg_size = sizeof(struct asm_pcm_cfg);
	enc_cfg.enc_blk.cfg.pcm.ch_cfg = channels;
	enc_cfg.enc_blk.cfg.pcm.bits_per_sample = 16;
	enc_cfg.enc_blk.cfg.pcm.sample_rate = rate;
	enc_cfg.enc_blk.cfg.pcm.is_signed = 1;
	enc_cfg.enc_blk.cfg.pcm.interleaved = 1;

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

int q6asm_enable_sbrps(struct audio_client *ac,
			uint32_t sbr_ps_enable)
{
	struct asm_stream_cmd_encdec_sbr  sbrps;

	int rc = 0;

	pr_debug("%s: Session %d\n", __func__, ac->session);

	q6asm_add_hdr(ac, &sbrps.hdr, sizeof(sbrps), TRUE);

	sbrps.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	sbrps.param_id = ASM_ENABLE_SBR_PS;
	sbrps.param_size = sizeof(struct asm_sbr_ps);
	sbrps.sbr_ps.enable = sbr_ps_enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &sbrps);
	if (rc < 0) {
		pr_err("Command opcode[0x%x]paramid[0x%x] failed\n",
				ASM_STREAM_CMD_SET_ENCDEC_PARAM,
				ASM_ENABLE_SBR_PS);
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
	struct asm_stream_cmd_encdec_dualmono dual_mono;

	int rc = 0;

	pr_debug("%s: Session %d, sce_left = %d, sce_right = %d\n",
			 __func__, ac->session, sce_left, sce_right);

	q6asm_add_hdr(ac, &dual_mono.hdr, sizeof(dual_mono), TRUE);

	dual_mono.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	dual_mono.param_id = ASM_CONFIGURE_DUAL_MONO;
	dual_mono.param_size = sizeof(struct asm_dual_mono);
	dual_mono.channel_map.sce_left = sce_left;
	dual_mono.channel_map.sce_right = sce_right;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &dual_mono);
	if (rc < 0) {
		pr_err("%s:Command opcode[0x%x]paramid[0x%x] failed\n",
				__func__, ASM_STREAM_CMD_SET_ENCDEC_PARAM,
				ASM_CONFIGURE_DUAL_MONO);
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

int q6asm_enc_cfg_blk_qcelp(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t reduced_rate_level, uint16_t rate_modulation_cmd)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]min_rate[0x%4x]max_rate[0x%4x] \
		reduced_rate_level[0x%4x]rate_modulation_cmd[0x%4x]", __func__,
		ac->session, frames_per_buf, min_rate, max_rate,
		reduced_rate_level, rate_modulation_cmd);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;

	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);

	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = V13K_FS;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_qcelp13_read_cfg);
	enc_cfg.enc_blk.cfg.qcelp13.min_rate = min_rate;
	enc_cfg.enc_blk.cfg.qcelp13.max_rate = max_rate;
	enc_cfg.enc_blk.cfg.qcelp13.reduced_rate_level = reduced_rate_level;
	enc_cfg.enc_blk.cfg.qcelp13.rate_modulation_cmd = rate_modulation_cmd;

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

int q6asm_enc_cfg_blk_evrc(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t rate_modulation_cmd)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]min_rate[0x%4x]max_rate[0x%4x] \
		rate_modulation_cmd[0x%4x]", __func__, ac->session,
		frames_per_buf,	min_rate, max_rate, rate_modulation_cmd);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;

	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);

	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = EVRC_FS;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_evrc_read_cfg);
	enc_cfg.enc_blk.cfg.evrc.min_rate = min_rate;
	enc_cfg.enc_blk.cfg.evrc.max_rate = max_rate;
	enc_cfg.enc_blk.cfg.evrc.rate_modulation_cmd = rate_modulation_cmd;
	enc_cfg.enc_blk.cfg.evrc.reserved = 0;

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

int q6asm_enc_cfg_blk_amrnb(struct audio_client *ac, uint32_t frames_per_buf,
			uint16_t band_mode, uint16_t dtx_enable)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]band_mode[0x%4x]dtx_enable[0x%4x]",
		__func__, ac->session, frames_per_buf, band_mode, dtx_enable);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;

	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);

	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = AMRNB_FS;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_amrnb_read_cfg);
	enc_cfg.enc_blk.cfg.amrnb.mode = band_mode;
	enc_cfg.enc_blk.cfg.amrnb.dtx_mode = dtx_enable;

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

int q6asm_enc_cfg_blk_amrwb(struct audio_client *ac, uint32_t frames_per_buf,
			uint16_t band_mode, uint16_t dtx_enable)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("%s:session[%d]frames[%d]band_mode[0x%4x]dtx_enable[0x%4x]",
		__func__, ac->session, frames_per_buf, band_mode, dtx_enable);

	q6asm_add_hdr(ac, &enc_cfg.hdr, sizeof(enc_cfg), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;

	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);

	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = AMRWB_FS;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_amrwb_read_cfg);
	enc_cfg.enc_blk.cfg.amrwb.mode = band_mode;
	enc_cfg.enc_blk.cfg.amrwb.dtx_mode = dtx_enable;

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

int q6asm_media_format_block_pcm(struct audio_client *ac,
				uint32_t rate, uint32_t channels)
{
	struct asm_stream_media_format_update fmt;
	int rc = 0;

	pr_debug("%s:session[%d]rate[%d]ch[%d]\n", __func__, ac->session, rate,
		channels);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;

	fmt.format = LINEAR_PCM;
	fmt.cfg_size = sizeof(struct asm_pcm_cfg);
	fmt.write_cfg.pcm_cfg.ch_cfg = channels;
	fmt.write_cfg.pcm_cfg.bits_per_sample = 16;
	fmt.write_cfg.pcm_cfg.sample_rate = rate;
	fmt.write_cfg.pcm_cfg.is_signed = 1;
	fmt.write_cfg.pcm_cfg.interleaved = 1;

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

int q6asm_media_format_block_multi_ch_pcm(struct audio_client *ac,
				uint32_t rate, uint32_t channels)
{
	struct asm_stream_media_format_update fmt;
	u8 *channel_mapping;
	int rc = 0;

	pr_debug("%s:session[%d]rate[%d]ch[%d]\n", __func__, ac->session, rate,
		channels);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;

	fmt.format = MULTI_CHANNEL_PCM;
	fmt.cfg_size = sizeof(struct asm_multi_channel_pcm_fmt_blk);
	fmt.write_cfg.multi_ch_pcm_cfg.num_channels = channels;
	fmt.write_cfg.multi_ch_pcm_cfg.bits_per_sample = 16;
	fmt.write_cfg.multi_ch_pcm_cfg.sample_rate = rate;
	fmt.write_cfg.multi_ch_pcm_cfg.is_signed = 1;
	fmt.write_cfg.multi_ch_pcm_cfg.is_interleaved = 1;
	channel_mapping =
		fmt.write_cfg.multi_ch_pcm_cfg.channel_mapping;

	memset(channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);

	if (channels == 1)  {
		channel_mapping[0] = PCM_CHANNEL_FL;
	} else if (channels == 2) {
		channel_mapping[0] = PCM_CHANNEL_FL;
		channel_mapping[1] = PCM_CHANNEL_FR;
	} else if (channels == 6) {
		channel_mapping[0] = PCM_CHANNEL_FC;
		channel_mapping[1] = PCM_CHANNEL_FL;
		channel_mapping[2] = PCM_CHANNEL_FR;
		channel_mapping[3] = PCM_CHANNEL_LB;
		channel_mapping[4] = PCM_CHANNEL_RB;
		channel_mapping[5] = PCM_CHANNEL_LFE;
	} else {
		pr_err("%s: ERROR.unsupported num_ch = %u\n", __func__,
				channels);
		return -EINVAL;
	}

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

int q6asm_media_format_block_aac(struct audio_client *ac,
				struct asm_aac_cfg *cfg)
{
	struct asm_stream_media_format_update fmt;
	int rc = 0;

	pr_debug("%s:session[%d]rate[%d]ch[%d]\n", __func__, ac->session,
		cfg->sample_rate, cfg->ch_cfg);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;

	fmt.format = MPEG4_AAC;
	fmt.cfg_size = sizeof(struct asm_aac_cfg);
	fmt.write_cfg.aac_cfg.format = cfg->format;
	fmt.write_cfg.aac_cfg.aot = cfg->aot;
	fmt.write_cfg.aac_cfg.ep_config = cfg->ep_config;
	fmt.write_cfg.aac_cfg.section_data_resilience =
					cfg->section_data_resilience;
	fmt.write_cfg.aac_cfg.scalefactor_data_resilience =
					cfg->scalefactor_data_resilience;
	fmt.write_cfg.aac_cfg.spectral_data_resilience =
					cfg->spectral_data_resilience;
	fmt.write_cfg.aac_cfg.ch_cfg = cfg->ch_cfg;
	fmt.write_cfg.aac_cfg.sample_rate = cfg->sample_rate;
	pr_info("%s:format=%x cfg_size=%d aac-cfg=%x aot=%d ch=%d sr=%d\n",
			__func__, fmt.format, fmt.cfg_size,
			fmt.write_cfg.aac_cfg.format,
			fmt.write_cfg.aac_cfg.aot,
			fmt.write_cfg.aac_cfg.ch_cfg,
			fmt.write_cfg.aac_cfg.sample_rate);
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
	struct asm_stream_media_format_update fmt;
	int rc = 0;

	pr_debug("%s:session[%d]rate[%d]ch[%d]\n", __func__, ac->session,
		cfg->sample_rate, cfg->ch_cfg);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;

	fmt.format = MPEG4_MULTI_AAC;
	fmt.cfg_size = sizeof(struct asm_aac_cfg);
	fmt.write_cfg.aac_cfg.format = cfg->format;
	fmt.write_cfg.aac_cfg.aot = cfg->aot;
	fmt.write_cfg.aac_cfg.ep_config = cfg->ep_config;
	fmt.write_cfg.aac_cfg.section_data_resilience =
					cfg->section_data_resilience;
	fmt.write_cfg.aac_cfg.scalefactor_data_resilience =
					cfg->scalefactor_data_resilience;
	fmt.write_cfg.aac_cfg.spectral_data_resilience =
					cfg->spectral_data_resilience;
	fmt.write_cfg.aac_cfg.ch_cfg = cfg->ch_cfg;
	fmt.write_cfg.aac_cfg.sample_rate = cfg->sample_rate;
	pr_info("%s:format=%x cfg_size=%d aac-cfg=%x aot=%d ch=%d sr=%d\n",
			__func__, fmt.format, fmt.cfg_size,
			fmt.write_cfg.aac_cfg.format,
			fmt.write_cfg.aac_cfg.aot,
			fmt.write_cfg.aac_cfg.ch_cfg,
			fmt.write_cfg.aac_cfg.sample_rate);
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



int q6asm_media_format_block(struct audio_client *ac, uint32_t format)
{

	struct asm_stream_media_format_update fmt;
	int rc = 0;

	pr_debug("%s:session[%d] format[0x%x]\n", __func__,
			ac->session, format);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);
	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;
	switch (format) {
	case FORMAT_V13K:
		fmt.format = V13K_FS;
		break;
	case FORMAT_EVRC:
		fmt.format = EVRC_FS;
		break;
	case FORMAT_AMRWB:
		fmt.format = AMRWB_FS;
		break;
	case FORMAT_AMRNB:
		fmt.format = AMRNB_FS;
		break;
	case FORMAT_MP3:
		fmt.format = MP3;
		break;
	default:
		pr_err("Invalid format[%d]\n", format);
		goto fail_cmd;
	}
	fmt.cfg_size = 0;

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

int q6asm_media_format_block_wma(struct audio_client *ac,
				void *cfg)
{
	struct asm_stream_media_format_update fmt;
	struct asm_wma_cfg *wma_cfg = (struct asm_wma_cfg *)cfg;
	int rc = 0;

	pr_debug("session[%d]format_tag[0x%4x] rate[%d] ch[0x%4x] bps[%d],\
		balign[0x%4x], bit_sample[0x%4x], ch_msk[%d], enc_opt[0x%4x]\n",
		ac->session, wma_cfg->format_tag, wma_cfg->sample_rate,
		wma_cfg->ch_cfg, wma_cfg->avg_bytes_per_sec,
		wma_cfg->block_align, wma_cfg->valid_bits_per_sample,
		wma_cfg->ch_mask, wma_cfg->encode_opt);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;

	fmt.format = WMA_V9;
	fmt.cfg_size = sizeof(struct asm_wma_cfg);
	fmt.write_cfg.wma_cfg.format_tag = wma_cfg->format_tag;
	fmt.write_cfg.wma_cfg.ch_cfg = wma_cfg->ch_cfg;
	fmt.write_cfg.wma_cfg.sample_rate = wma_cfg->sample_rate;
	fmt.write_cfg.wma_cfg.avg_bytes_per_sec = wma_cfg->avg_bytes_per_sec;
	fmt.write_cfg.wma_cfg.block_align = wma_cfg->block_align;
	fmt.write_cfg.wma_cfg.valid_bits_per_sample =
			wma_cfg->valid_bits_per_sample;
	fmt.write_cfg.wma_cfg.ch_mask = wma_cfg->ch_mask;
	fmt.write_cfg.wma_cfg.encode_opt = wma_cfg->encode_opt;
	fmt.write_cfg.wma_cfg.adv_encode_opt = 0;
	fmt.write_cfg.wma_cfg.adv_encode_opt2 = 0;
	fmt.write_cfg.wma_cfg.drc_peak_ref = 0;
	fmt.write_cfg.wma_cfg.drc_peak_target = 0;
	fmt.write_cfg.wma_cfg.drc_ave_ref = 0;
	fmt.write_cfg.wma_cfg.drc_ave_target = 0;

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
	struct asm_stream_media_format_update fmt;
	struct asm_wmapro_cfg *wmapro_cfg = (struct asm_wmapro_cfg *)cfg;
	int rc = 0;

	pr_debug("session[%d]format_tag[0x%4x] rate[%d] ch[0x%4x] bps[%d],"
		"balign[0x%4x], bit_sample[0x%4x], ch_msk[%d], enc_opt[0x%4x],\
		adv_enc_opt[0x%4x], adv_enc_opt2[0x%8x]\n",
		ac->session, wmapro_cfg->format_tag, wmapro_cfg->sample_rate,
		wmapro_cfg->ch_cfg,  wmapro_cfg->avg_bytes_per_sec,
		wmapro_cfg->block_align, wmapro_cfg->valid_bits_per_sample,
		wmapro_cfg->ch_mask, wmapro_cfg->encode_opt,
		wmapro_cfg->adv_encode_opt, wmapro_cfg->adv_encode_opt2);

	q6asm_add_hdr(ac, &fmt.hdr, sizeof(fmt), TRUE);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;

	fmt.format = WMA_V10PRO;
	fmt.cfg_size = sizeof(struct asm_wmapro_cfg);
	fmt.write_cfg.wmapro_cfg.format_tag = wmapro_cfg->format_tag;
	fmt.write_cfg.wmapro_cfg.ch_cfg = wmapro_cfg->ch_cfg;
	fmt.write_cfg.wmapro_cfg.sample_rate = wmapro_cfg->sample_rate;
	fmt.write_cfg.wmapro_cfg.avg_bytes_per_sec =
				wmapro_cfg->avg_bytes_per_sec;
	fmt.write_cfg.wmapro_cfg.block_align = wmapro_cfg->block_align;
	fmt.write_cfg.wmapro_cfg.valid_bits_per_sample =
				wmapro_cfg->valid_bits_per_sample;
	fmt.write_cfg.wmapro_cfg.ch_mask = wmapro_cfg->ch_mask;
	fmt.write_cfg.wmapro_cfg.encode_opt = wmapro_cfg->encode_opt;
	fmt.write_cfg.wmapro_cfg.adv_encode_opt = wmapro_cfg->adv_encode_opt;
	fmt.write_cfg.wmapro_cfg.adv_encode_opt2 = wmapro_cfg->adv_encode_opt2;
	fmt.write_cfg.wmapro_cfg.drc_peak_ref = 0;
	fmt.write_cfg.wmapro_cfg.drc_peak_target = 0;
	fmt.write_cfg.wmapro_cfg.drc_ave_ref = 0;
	fmt.write_cfg.wmapro_cfg.drc_ave_target = 0;

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

int q6asm_memory_map(struct audio_client *ac, uint32_t buf_add, int dir,
					uint32_t bufsz, uint32_t bufcnt)
{
	struct asm_stream_cmd_memory_map mem_map;
	int rc = 0;

	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}

	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	mem_map.hdr.opcode = ASM_SESSION_CMD_MEMORY_MAP;

	mem_map.buf_add = buf_add;
	mem_map.buf_size = bufsz * bufcnt;
	mem_map.mempool_id = 0; /* EBI */
	mem_map.reserved = 0;

	q6asm_add_mmaphdr(&mem_map.hdr,
			sizeof(struct asm_stream_cmd_memory_map), TRUE);

	pr_debug("buf add[%x]  buf_add_parameter[%x]\n",
					mem_map.buf_add, buf_add);

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_map);
	if (rc < 0) {
		pr_err("mem_map op[0x%x]rc[%d]\n",
				mem_map.hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
		(atomic_read(&this_mmap.cmd_state) == 0), 5 * HZ);
	if (!rc) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_memory_unmap(struct audio_client *ac, uint32_t buf_add, int dir)
{
	struct asm_stream_cmd_memory_unmap mem_unmap;
	int rc = 0;

	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	q6asm_add_mmaphdr(&mem_unmap.hdr,
			sizeof(struct asm_stream_cmd_memory_unmap), TRUE);
	mem_unmap.hdr.opcode = ASM_SESSION_CMD_MEMORY_UNMAP;
	mem_unmap.buf_add = buf_add;

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_unmap);
	if (rc < 0) {
		pr_err("mem_unmap op[0x%x]rc[%d]\n",
					mem_unmap.hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
			(atomic_read(&this_mmap.cmd_state) == 0), 5 * HZ);
	if (!rc) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int q6asm_set_lrgain(struct audio_client *ac, int left_gain, int right_gain)
{
	void *vol_cmd = NULL;
	void *payload = NULL;
	struct asm_pp_params_command *cmd = NULL;
	struct asm_lrchannel_gain_params *lrgain = NULL;
	int sz = 0;
	int rc  = 0;

	sz = sizeof(struct asm_pp_params_command) +
		+ sizeof(struct asm_lrchannel_gain_params);
	vol_cmd = kzalloc(sz, GFP_KERNEL);
	if (vol_cmd == NULL) {
		pr_err("%s[%d]: Mem alloc failed\n", __func__, ac->session);
		rc = -EINVAL;
		return rc;
	}
	cmd = (struct asm_pp_params_command *)vol_cmd;
	q6asm_add_hdr_async(ac, &cmd->hdr, sz, TRUE);
	cmd->hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS;
	cmd->payload = NULL;
	cmd->payload_size = sizeof(struct  asm_pp_param_data_hdr) +
				sizeof(struct asm_lrchannel_gain_params);
	cmd->params.module_id = VOLUME_CONTROL_MODULE_ID;
	cmd->params.param_id = L_R_CHANNEL_GAIN_PARAM_ID;
	cmd->params.param_size = sizeof(struct asm_lrchannel_gain_params);
	cmd->params.reserved = 0;

	payload = (u8 *)(vol_cmd + sizeof(struct asm_pp_params_command));
	lrgain = (struct asm_lrchannel_gain_params *)payload;

	lrgain->left_gain = left_gain;
	lrgain->right_gain = right_gain;
	rc = apr_send_pkt(ac->apr, (uint32_t *) vol_cmd);
	if (rc < 0) {
		pr_err("%s: Volume Command failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout in sending volume command to apr\n",
			__func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	kfree(vol_cmd);
	return rc;
}

static int q6asm_memory_map_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt)
{
	struct	 asm_stream_cmd_memory_map_regions *mmap_regions = NULL;
	struct asm_memory_map_regions *mregions = NULL;
	struct audio_port_data *port = NULL;
	struct audio_buffer *ab = NULL;
	void	*mmap_region_cmd = NULL;
	void	*payload = NULL;
	int	rc = 0;
	int	i = 0;
	int	cmd_size = 0;

	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	cmd_size = sizeof(struct asm_stream_cmd_memory_map_regions)
			+ sizeof(struct asm_memory_map_regions) * bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (mmap_region_cmd == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		rc = -EINVAL;
		return rc;
	}
	mmap_regions = (struct asm_stream_cmd_memory_map_regions *)
							mmap_region_cmd;
	q6asm_add_mmaphdr(&mmap_regions->hdr, cmd_size, TRUE);
	mmap_regions->hdr.opcode = ASM_SESSION_CMD_MEMORY_MAP_REGIONS;
	mmap_regions->mempool_id = 0;
	mmap_regions->nregions = bufcnt & 0x00ff;
	pr_debug("map_regions->nregions = %d\n", mmap_regions->nregions);
	payload = ((u8 *) mmap_region_cmd +
		sizeof(struct asm_stream_cmd_memory_map_regions));
	mregions = (struct asm_memory_map_regions *)payload;

	port = &ac->port[dir];
	for (i = 0; i < bufcnt; i++) {
		ab = &port->buf[i];
		mregions->phys = ab->phys;
		mregions->buf_size = ab->size;
		++mregions;
	}

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) mmap_region_cmd);
	if (rc < 0) {
		pr_err("mmap_regions op[0x%x]rc[%d]\n",
					mmap_regions->hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
			(atomic_read(&this_mmap.cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	kfree(mmap_region_cmd);
	return rc;
}

static int q6asm_memory_unmap_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt)
{
	struct asm_stream_cmd_memory_unmap_regions *unmap_regions = NULL;
	struct asm_memory_unmap_regions *mregions = NULL;
	struct audio_port_data *port = NULL;
	struct audio_buffer *ab = NULL;
	void	*unmap_region_cmd = NULL;
	void	*payload = NULL;
	int	rc = 0;
	int	i = 0;
	int	cmd_size = 0;

	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: Session[%d]\n", __func__, ac->session);

	cmd_size = sizeof(struct asm_stream_cmd_memory_unmap_regions) +
			sizeof(struct asm_memory_unmap_regions) * bufcnt;

	unmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (unmap_region_cmd == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		rc = -EINVAL;
		return rc;
	}
	unmap_regions = (struct asm_stream_cmd_memory_unmap_regions *)
							unmap_region_cmd;
	q6asm_add_mmaphdr(&unmap_regions->hdr, cmd_size, TRUE);
	unmap_regions->hdr.opcode = ASM_SESSION_CMD_MEMORY_UNMAP_REGIONS;
	unmap_regions->nregions = bufcnt & 0x00ff;
	pr_debug("unmap_regions->nregions = %d\n", unmap_regions->nregions);
	payload = ((u8 *) unmap_region_cmd +
			sizeof(struct asm_stream_cmd_memory_unmap_regions));
	mregions = (struct asm_memory_unmap_regions *)payload;
	port = &ac->port[dir];
	for (i = 0; i < bufcnt; i++) {
		ab = &port->buf[i];
		mregions->phys = ab->phys;
		++mregions;
	}

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) unmap_region_cmd);
	if (rc < 0) {
		pr_err("mmap_regions op[0x%x]rc[%d]\n",
					unmap_regions->hdr.opcode, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
			(atomic_read(&this_mmap.cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("timeout. waited for memory_unmap\n");
		goto fail_cmd;
	}
	rc = 0;

fail_cmd:
	kfree(unmap_region_cmd);
	return rc;
}

int q6asm_set_mute(struct audio_client *ac, int muteflag)
{
	void *vol_cmd = NULL;
	void *payload = NULL;
	struct asm_pp_params_command *cmd = NULL;
	struct asm_mute_params *mute = NULL;
	int sz = 0;
	int rc  = 0;

	sz = sizeof(struct asm_pp_params_command) +
		+ sizeof(struct asm_mute_params);
	vol_cmd = kzalloc(sz, GFP_KERNEL);
	if (vol_cmd == NULL) {
		pr_err("%s[%d]: Mem alloc failed\n", __func__, ac->session);
		rc = -EINVAL;
		return rc;
	}
	cmd = (struct asm_pp_params_command *)vol_cmd;
	q6asm_add_hdr_async(ac, &cmd->hdr, sz, TRUE);
	cmd->hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS;
	cmd->payload = NULL;
	cmd->payload_size = sizeof(struct  asm_pp_param_data_hdr) +
				sizeof(struct asm_mute_params);
	cmd->params.module_id = VOLUME_CONTROL_MODULE_ID;
	cmd->params.param_id = MUTE_CONFIG_PARAM_ID;
	cmd->params.param_size = sizeof(struct asm_mute_params);
	cmd->params.reserved = 0;

	payload = (u8 *)(vol_cmd + sizeof(struct asm_pp_params_command));
	mute = (struct asm_mute_params *)payload;

	mute->muteflag = muteflag;
	rc = apr_send_pkt(ac->apr, (uint32_t *) vol_cmd);
	if (rc < 0) {
		pr_err("%s: Mute Command failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout in sending mute command to apr\n",
			__func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	kfree(vol_cmd);
	return rc;
}

int q6asm_set_volume(struct audio_client *ac, int volume)
{
	void *vol_cmd = NULL;
	void *payload = NULL;
	struct asm_pp_params_command *cmd = NULL;
	struct asm_master_gain_params *mgain = NULL;
	int sz = 0;
	int rc  = 0;

	sz = sizeof(struct asm_pp_params_command) +
		+ sizeof(struct asm_master_gain_params);
	vol_cmd = kzalloc(sz, GFP_KERNEL);
	if (vol_cmd == NULL) {
		pr_err("%s[%d]: Mem alloc failed\n", __func__, ac->session);
		rc = -EINVAL;
		return rc;
	}
	cmd = (struct asm_pp_params_command *)vol_cmd;
	q6asm_add_hdr_async(ac, &cmd->hdr, sz, TRUE);
	cmd->hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS;
	cmd->payload = NULL;
	cmd->payload_size = sizeof(struct  asm_pp_param_data_hdr) +
				sizeof(struct asm_master_gain_params);
	cmd->params.module_id = VOLUME_CONTROL_MODULE_ID;
	cmd->params.param_id = MASTER_GAIN_PARAM_ID;
	cmd->params.param_size = sizeof(struct asm_master_gain_params);
	cmd->params.reserved = 0;

	payload = (u8 *)(vol_cmd + sizeof(struct asm_pp_params_command));
	mgain = (struct asm_master_gain_params *)payload;

	mgain->master_gain = volume;
	mgain->padding = 0x00;
	rc = apr_send_pkt(ac->apr, (uint32_t *) vol_cmd);
	if (rc < 0) {
		pr_err("%s: Volume Command failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout in sending volume command to apr\n",
			__func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	kfree(vol_cmd);
	return rc;
}

int q6asm_set_softpause(struct audio_client *ac,
			struct asm_softpause_params *pause_param)
{
	void *vol_cmd = NULL;
	void *payload = NULL;
	struct asm_pp_params_command *cmd = NULL;
	struct asm_softpause_params *params = NULL;
	int sz = 0;
	int rc  = 0;

	sz = sizeof(struct asm_pp_params_command) +
		+ sizeof(struct asm_softpause_params);
	vol_cmd = kzalloc(sz, GFP_KERNEL);
	if (vol_cmd == NULL) {
		pr_err("%s[%d]: Mem alloc failed\n", __func__, ac->session);
		rc = -EINVAL;
		return rc;
	}
	cmd = (struct asm_pp_params_command *)vol_cmd;
	q6asm_add_hdr_async(ac, &cmd->hdr, sz, TRUE);
	cmd->hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS;
	cmd->payload = NULL;
	cmd->payload_size = sizeof(struct  asm_pp_param_data_hdr) +
				sizeof(struct asm_softpause_params);
	cmd->params.module_id = VOLUME_CONTROL_MODULE_ID;
	cmd->params.param_id = SOFT_PAUSE_PARAM_ID;
	cmd->params.param_size = sizeof(struct asm_softpause_params);
	cmd->params.reserved = 0;

	payload = (u8 *)(vol_cmd + sizeof(struct asm_pp_params_command));
	params = (struct asm_softpause_params *)payload;

	params->enable = pause_param->enable;
	params->period = pause_param->period;
	params->step = pause_param->step;
	params->rampingcurve = pause_param->rampingcurve;
	pr_debug("%s: soft Pause Command: enable = %d, period = %d,"
			 "step = %d, curve = %d\n", __func__, params->enable,
			 params->period, params->step, params->rampingcurve);
	rc = apr_send_pkt(ac->apr, (uint32_t *) vol_cmd);
	if (rc < 0) {
		pr_err("%s: Volume Command(soft_pause) failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout in sending volume command(soft_pause)"
		       "to apr\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	kfree(vol_cmd);
	return rc;
}

int q6asm_set_softvolume(struct audio_client *ac,
			struct asm_softvolume_params *softvol_param)
{
	void *vol_cmd = NULL;
	void *payload = NULL;
	struct asm_pp_params_command *cmd = NULL;
	struct asm_softvolume_params *params = NULL;
	int sz = 0;
	int rc  = 0;

	sz = sizeof(struct asm_pp_params_command) +
		+ sizeof(struct asm_softvolume_params);
	vol_cmd = kzalloc(sz, GFP_KERNEL);
	if (vol_cmd == NULL) {
		pr_err("%s[%d]: Mem alloc failed\n", __func__, ac->session);
		rc = -EINVAL;
		return rc;
	}
	cmd = (struct asm_pp_params_command *)vol_cmd;
	q6asm_add_hdr_async(ac, &cmd->hdr, sz, TRUE);
	cmd->hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS;
	cmd->payload = NULL;
	cmd->payload_size = sizeof(struct  asm_pp_param_data_hdr) +
				sizeof(struct asm_softvolume_params);
	cmd->params.module_id = VOLUME_CONTROL_MODULE_ID;
	cmd->params.param_id = SOFT_VOLUME_PARAM_ID;
	cmd->params.param_size = sizeof(struct asm_softvolume_params);
	cmd->params.reserved = 0;

	payload = (u8 *)(vol_cmd + sizeof(struct asm_pp_params_command));
	params = (struct asm_softvolume_params *)payload;

	params->period = softvol_param->period;
	params->step = softvol_param->step;
	params->rampingcurve = softvol_param->rampingcurve;
	pr_debug("%s: soft Volume:opcode = %d,payload_sz =%d,module_id =%d,"
			 "param_id = %d, param_sz = %d\n", __func__,
			cmd->hdr.opcode, cmd->payload_size,
			cmd->params.module_id, cmd->params.param_id,
			cmd->params.param_size);
	pr_debug("%s: soft Volume Command: period = %d,"
			 "step = %d, curve = %d\n", __func__, params->period,
			 params->step, params->rampingcurve);
	rc = apr_send_pkt(ac->apr, (uint32_t *) vol_cmd);
	if (rc < 0) {
		pr_err("%s: Volume Command(soft_volume) failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout in sending volume command(soft_volume)"
		       "to apr\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	kfree(vol_cmd);
	return rc;
}

int q6asm_equalizer(struct audio_client *ac, void *eq)
{
	void *eq_cmd = NULL;
	void *payload = NULL;
	struct asm_pp_params_command *cmd = NULL;
	struct asm_equalizer_params *equalizer = NULL;
	struct msm_audio_eq_stream_config *eq_params = NULL;
	int i  = 0;
	int sz = 0;
	int rc  = 0;

	sz = sizeof(struct asm_pp_params_command) +
		+ sizeof(struct asm_equalizer_params);
	eq_cmd = kzalloc(sz, GFP_KERNEL);
	if (eq_cmd == NULL) {
		pr_err("%s[%d]: Mem alloc failed\n", __func__, ac->session);
		rc = -EINVAL;
		goto fail_cmd;
	}
	eq_params = (struct msm_audio_eq_stream_config *) eq;
	cmd = (struct asm_pp_params_command *)eq_cmd;
	q6asm_add_hdr(ac, &cmd->hdr, sz, TRUE);
	cmd->hdr.opcode = ASM_STREAM_CMD_SET_PP_PARAMS;
	cmd->payload = NULL;
	cmd->payload_size = sizeof(struct  asm_pp_param_data_hdr) +
				sizeof(struct asm_equalizer_params);
	cmd->params.module_id = EQUALIZER_MODULE_ID;
	cmd->params.param_id = EQUALIZER_PARAM_ID;
	cmd->params.param_size = sizeof(struct asm_equalizer_params);
	cmd->params.reserved = 0;
	payload = (u8 *)(eq_cmd + sizeof(struct asm_pp_params_command));
	equalizer = (struct asm_equalizer_params *)payload;

	equalizer->enable = eq_params->enable;
	equalizer->num_bands = eq_params->num_bands;
	pr_debug("%s: enable:%d numbands:%d\n", __func__, eq_params->enable,
							eq_params->num_bands);
	for (i = 0; i < eq_params->num_bands; i++) {
		equalizer->eq_bands[i].band_idx =
					eq_params->eq_bands[i].band_idx;
		equalizer->eq_bands[i].filter_type =
					eq_params->eq_bands[i].filter_type;
		equalizer->eq_bands[i].center_freq_hz =
					eq_params->eq_bands[i].center_freq_hz;
		equalizer->eq_bands[i].filter_gain =
					eq_params->eq_bands[i].filter_gain;
		equalizer->eq_bands[i].q_factor =
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
	rc = apr_send_pkt(ac->apr, (uint32_t *) eq_cmd);
	if (rc < 0) {
		pr_err("%s: Equalizer Command failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (!rc) {
		pr_err("%s: timeout in sending equalizer command to apr\n",
			__func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	kfree(eq_cmd);
	return rc;
}

int q6asm_read(struct audio_client *ac)
{
	struct asm_stream_cmd_read read;
	struct audio_buffer        *ab;
	int dsp_buf;
	struct audio_port_data     *port;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	if (ac->io_mode == SYNC_IO_MODE) {
		port = &ac->port[OUT];

		q6asm_add_hdr(ac, &read.hdr, sizeof(read), FALSE);

		mutex_lock(&port->lock);

		dsp_buf = port->dsp_buf;
		ab = &port->buf[dsp_buf];

		pr_debug("%s:session[%d]dsp-buf[%d][%p]cpu_buf[%d][%p]\n",
					__func__,
					ac->session,
					dsp_buf,
					(void *)port->buf[dsp_buf].data,
					port->cpu_buf,
					(void *)port->buf[port->cpu_buf].phys);

		read.hdr.opcode = ASM_DATA_CMD_READ;
		read.buf_add = ab->phys;
		read.buf_size = ab->size;
		read.uid = port->dsp_buf;
		read.hdr.token = port->dsp_buf;

		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);
		mutex_unlock(&port->lock);
		pr_debug("%s:buf add[0x%x] token[%d] uid[%d]\n", __func__,
						read.buf_add,
						read.hdr.token,
						read.uid);
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
	struct asm_stream_cmd_read read;
	struct audio_buffer        *ab;
	int dsp_buf;
	struct audio_port_data     *port;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	if (ac->io_mode == SYNC_IO_MODE) {
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

		read.hdr.opcode = ASM_DATA_CMD_READ;
		read.buf_add = ab->phys;
		read.buf_size = ab->size;
		read.uid = port->dsp_buf;
		read.hdr.token = port->dsp_buf;

		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);
		pr_debug("%s:buf add[0x%x] token[%d] uid[%d]\n", __func__,
					read.buf_add,
					read.hdr.token,
					read.uid);
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


static void q6asm_add_hdr_async(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg)
{
	pr_debug("session=%d pkt size=%d cmd_flg=%d\n", pkt_size, cmd_flg,
		ac->session);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(sizeof(struct apr_hdr)),\
				APR_PKT_VER);
	hdr->src_svc = ((struct apr_svc *)ac->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_ASM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = ((ac->session << 8) & 0xFF00) | 0x01;
	hdr->dest_port = ((ac->session << 8) & 0xFF00) | 0x01;
	if (cmd_flg) {
		hdr->token = ac->session;
		atomic_set(&ac->cmd_state, 1);
	}
	hdr->pkt_size  = pkt_size;
	return;
}

int q6asm_async_write(struct audio_client *ac,
					  struct audio_aio_write_param *param)
{
	int rc = 0;
	struct asm_stream_cmd_write write;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6asm_add_hdr_async(ac, &write.hdr, sizeof(write), FALSE);

	/* Pass physical address as token for AIO scheme */
	write.hdr.token = param->uid;
	write.hdr.opcode = ASM_DATA_CMD_WRITE;
	write.buf_add = param->paddr;
	write.avail_bytes = param->len;
	write.uid = param->uid;
	write.msw_ts = param->msw_ts;
	write.lsw_ts = param->lsw_ts;
	/* Use 0xFF00 for disabling timestamps */
	if (param->flags == 0xFF00)
		write.uflags = (0x00000000 | (param->flags & 0x800000FF));
	else
		write.uflags = (0x80000000 | param->flags);

	pr_debug("%s: session[%d] bufadd[0x%x]len[0x%x]", __func__, ac->session,
		write.buf_add, write.avail_bytes);

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
	struct asm_stream_cmd_read read;

	if (!ac || ac->apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6asm_add_hdr_async(ac, &read.hdr, sizeof(read), FALSE);

	/* Pass physical address as token for AIO scheme */
	read.hdr.token = param->paddr;
	read.hdr.opcode = ASM_DATA_CMD_READ;
	read.buf_add = param->paddr;
	read.buf_size = param->len;
	read.uid = param->uid;

	pr_debug("%s: session[%d] bufadd[0x%x]len[0x%x]", __func__, ac->session,
		read.buf_add, read.buf_size);

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
	struct asm_stream_cmd_write write;
	struct audio_port_data *port;
	struct audio_buffer    *ab;
	int dsp_buf = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: session[%d] len=%d", __func__, ac->session, len);
	if (ac->io_mode == SYNC_IO_MODE) {
		port = &ac->port[IN];

		q6asm_add_hdr(ac, &write.hdr, sizeof(write),
				FALSE);
		mutex_lock(&port->lock);

		dsp_buf = port->dsp_buf;
		ab = &port->buf[dsp_buf];

		write.hdr.token = port->dsp_buf;
		write.hdr.opcode = ASM_DATA_CMD_WRITE;
		write.buf_add = ab->phys;
		write.avail_bytes = len;
		write.uid = port->dsp_buf;
		write.msw_ts = msw_ts;
		write.lsw_ts = lsw_ts;
		/* Use 0xFF00 for disabling timestamps */
		if (flags == 0xFF00)
			write.uflags = (0x00000000 | (flags & 0x800000FF));
		else
			write.uflags = (0x80000000 | flags);
		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);

		pr_debug("%s:ab->phys[0x%x]bufadd[0x%x]token[0x%x]buf_id[0x%x]"
							, __func__,
							ab->phys,
							write.buf_add,
							write.hdr.token,
							write.uid);
		mutex_unlock(&port->lock);
#ifdef CONFIG_DEBUG_FS
		if (out_enable_flag) {
			char zero_pattern[2] = {0x00, 0x00};
			/* If First two byte is non zero and last two byte
			is zero then it is warm output pattern */
			if ((strncmp(((char *)ab->data), zero_pattern, 2)) &&
			(!strncmp(((char *)ab->data + 2), zero_pattern, 2))) {
				do_gettimeofday(&out_warm_tv);
				pr_debug("WARM:apr_send_pkt at \
				%ld sec %ld microsec\n", out_warm_tv.tv_sec,\
				out_warm_tv.tv_usec);
				pr_debug("Warm Pattern Matched");
			}
			/* If First two byte is zero and last two byte is
			non zero then it is cont ouput pattern */
			else if ((!strncmp(((char *)ab->data), zero_pattern, 2))
			&& (strncmp(((char *)ab->data + 2), zero_pattern, 2))) {
				do_gettimeofday(&out_cont_tv);
				pr_debug("CONT:apr_send_pkt at \
				%ld sec %ld microsec\n", out_cont_tv.tv_sec,\
				out_cont_tv.tv_usec);
				pr_debug("Cont Pattern Matched");
			}
		}
#endif
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
	struct asm_stream_cmd_write write;
	struct audio_port_data *port;
	struct audio_buffer    *ab;
	int dsp_buf = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: session[%d] len=%d", __func__, ac->session, len);
	if (ac->io_mode == SYNC_IO_MODE) {
		port = &ac->port[IN];

		q6asm_add_hdr_async(ac, &write.hdr, sizeof(write),
					FALSE);

		dsp_buf = port->dsp_buf;
		ab = &port->buf[dsp_buf];

		write.hdr.token = port->dsp_buf;
		write.hdr.opcode = ASM_DATA_CMD_WRITE;
		write.buf_add = ab->phys;
		write.avail_bytes = len;
		write.uid = port->dsp_buf;
		write.msw_ts = msw_ts;
		write.lsw_ts = lsw_ts;
		/* Use 0xFF00 for disabling timestamps */
		if (flags == 0xFF00)
			write.uflags = (0x00000000 | (flags & 0x800000FF));
		else
			write.uflags = (0x80000000 | flags);
		port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);

		pr_debug("%s:ab->phys[0x%x]bufadd[0x%x]token[0x%x]buf_id[0x%x]"
							, __func__,
							ab->phys,
							write.buf_add,
							write.hdr.token,
							write.uid);

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

uint64_t q6asm_get_session_time(struct audio_client *ac)
{
	struct apr_hdr hdr;
	int rc;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	q6asm_add_hdr(ac, &hdr, sizeof(hdr), TRUE);
	hdr.opcode = ASM_SESSION_CMD_GET_SESSION_TIME;
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
	return ac->time_stamp;

fail_cmd:
	return -EINVAL;
}

int q6asm_cmd(struct audio_client *ac, int cmd)
{
	struct apr_hdr hdr;
	int rc;
	atomic_t *state;
	int cnt = 0;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	q6asm_add_hdr(ac, &hdr, sizeof(hdr), TRUE);
	switch (cmd) {
	case CMD_PAUSE:
		pr_debug("%s:CMD_PAUSE\n", __func__);
		hdr.opcode = ASM_SESSION_CMD_PAUSE;
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

int q6asm_cmd_nowait(struct audio_client *ac, int cmd)
{
	struct apr_hdr hdr;
	int rc;

	if (!ac || ac->apr == NULL) {
		pr_err("%s:APR handle NULL\n", __func__);
		return -EINVAL;
	}
	q6asm_add_hdr_async(ac, &hdr, sizeof(hdr), TRUE);
	switch (cmd) {
	case CMD_PAUSE:
		pr_debug("%s:CMD_PAUSE\n", __func__);
		hdr.opcode = ASM_SESSION_CMD_PAUSE;
		break;
	case CMD_EOS:
		pr_debug("%s:CMD_EOS\n", __func__);
		hdr.opcode = ASM_DATA_CMD_EOS;
		break;
	default:
		pr_err("%s:Invalid format[%d]\n", __func__, cmd);
		goto fail_cmd;
	}
	pr_debug("%s:session[%d]opcode[0x%x] ", __func__,
						ac->session,
						hdr.opcode);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &hdr);
	if (rc < 0) {
		pr_err("%s:Commmand 0x%x failed\n", __func__, hdr.opcode);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

static void q6asm_reset_buf_state(struct audio_client *ac)
{
	int cnt = 0;
	int loopcnt = 0;
	struct audio_port_data *port = NULL;

	if (ac->io_mode == SYNC_IO_MODE) {
		mutex_lock(&ac->cmd_lock);
		for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
			port = &ac->port[loopcnt];
			cnt = port->max_buf_cnt - 1;
			port->dsp_buf = 0;
			port->cpu_buf = 0;
			while (cnt >= 0) {
				if (!port->buf)
					continue;
				port->buf[cnt].used = 1;
				cnt--;
			}
		}
		mutex_unlock(&ac->cmd_lock);
	}
}

int q6asm_reg_tx_overflow(struct audio_client *ac, uint16_t enable)
{
	struct asm_stream_cmd_reg_tx_overflow_event tx_overflow;
	int rc;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("%s:session[%d]enable[%d]\n", __func__,
						ac->session, enable);
	q6asm_add_hdr(ac, &tx_overflow.hdr, sizeof(tx_overflow), TRUE);

	tx_overflow.hdr.opcode = \
			ASM_SESSION_CMD_REGISTER_FOR_TX_OVERFLOW_EVENTS;
	/* tx overflow event: enable */
	tx_overflow.enable = enable;

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

int q6asm_get_apr_service_id(int session_id)
{
	pr_debug("%s\n", __func__);

	if (session_id < 0 || session_id > SESSION_MAX) {
		pr_err("%s: invalid session_id = %d\n", __func__, session_id);
		return -EINVAL;
	}

	return ((struct apr_svc *)session[session_id]->apr)->id;
}


static int __init q6asm_init(void)
{
	pr_debug("%s\n", __func__);
	init_waitqueue_head(&this_mmap.cmd_wait);
	memset(session, 0, sizeof(session));
#ifdef CONFIG_DEBUG_FS
	out_buffer = kmalloc(OUT_BUFFER_SIZE, GFP_KERNEL);
	out_dentry = debugfs_create_file("audio_out_latency_measurement_node",\
				S_IFREG | S_IRUGO | S_IWUGO,\
				NULL, NULL, &audio_output_latency_debug_fops);
	if (IS_ERR(out_dentry))
		pr_err("debugfs_create_file failed\n");
	in_buffer = kmalloc(IN_BUFFER_SIZE, GFP_KERNEL);
	in_dentry = debugfs_create_file("audio_in_latency_measurement_node",\
				S_IFREG | S_IRUGO | S_IWUGO,\
				NULL, NULL, &audio_input_latency_debug_fops);
	if (IS_ERR(in_dentry))
		pr_err("debugfs_create_file failed\n");
#endif
	return 0;
}

device_initcall(q6asm_init);

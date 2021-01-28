// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cacheflush.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/semaphore.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/freezer.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/mailbox_controller.h>
#include <linux/signal.h>
#include <trace/events/signal.h>
#include <linux/string.h>

#ifdef CONFIG_MTK_IOMMU_V2
#include <linux/iommu.h>
#endif
#include "mtk_vcodec_mem.h"
#include <uapi/linux/mtk_vcu_controls.h>
#include "mtk_vcu.h"

/*
#undef pr_debug
#define pr_debug pr_info

#undef dev_dbg
#define dev_dbg dev_info
*/

/**
 * VCU (Video Communication/Controller Unit) is a tiny processor
 * controlling video hardware related to video codec, scaling and color
 * format converting.
 * VCU interfaces with other blocks by share memory and interrupt.
 **/
#define VCU_PATH                "/dev/vpud"
#define MDP_PATH                "/dev/mdpd"
#define CAM_PATH                "/dev/camd"
#define VCU_DEVNAME             "vcu"

#ifdef CONFIG_MTK_ENG_BUILD
#define IPI_TIMEOUT_MS          16000U
#else
#define IPI_TIMEOUT_MS          6000U
#endif

#define VCU_FW_VER_LEN          16
#define VCODEC_INST_MAX         64
#define GCE_EVENT_MAX           64
#define GCE_THNUM_MAX           2
#define GCE_PENDING_CNT         10
/*mtk vcu support mpd max value*/
#define MTK_VCU_NR_MAX       3

/* vcu extended mapping length */
#define VCU_PMEM0_LEN(vcu_data) (vcu_data->extmem.p_len)
#define VCU_DMEM0_LEN(vcu_data) (vcu_data->extmem.d_len)
/* vcu extended user virtural address */
#define VCU_PMEM0_VMA(vcu_data) (vcu_data->extmem.p_vma)
#define VCU_DMEM0_VMA(vcu_data) (vcu_data->extmem.d_vma)
/* vcu extended kernel virtural address */
#define VCU_PMEM0_VIRT(vcu_data)        (vcu_data->extmem.p_va)
#define VCU_DMEM0_VIRT(vcu_data)        (vcu_data->extmem.d_va)
/* vcu extended phsyial address */
#define VCU_PMEM0_PHY(vcu_data) (vcu_data->extmem.p_pa)
#define VCU_DMEM0_PHY(vcu_data) (vcu_data->extmem.d_pa)
/* vcu extended iova address*/
#define VCU_PMEM0_IOVA(vcu_data)        (vcu_data->extmem.p_iova)
#define VCU_DMEM0_IOVA(vcu_data)        (vcu_data->extmem.d_iova)
#define VCU_SHMEM_SIZE 0x80000

#define MAP_SHMEM_ALLOC_BASE    0x80000000UL
#define MAP_SHMEM_ALLOC_RANGE   VCU_SHMEM_SIZE
#define MAP_SHMEM_ALLOC_END     (MAP_SHMEM_ALLOC_BASE + MAP_SHMEM_ALLOC_RANGE)
#define MAP_SHMEM_COMMIT_BASE   0x88000000UL
#define MAP_SHMEM_COMMIT_RANGE  VCU_SHMEM_SIZE
#define MAP_SHMEM_COMMIT_END    (MAP_SHMEM_COMMIT_BASE + MAP_SHMEM_COMMIT_RANGE)

#define MAP_SHMEM_MM_BASE       0x200000000UL
#define MAP_SHMEM_MM_CACHEABLE_BASE     0x400000000UL
#define MAP_SHMEM_PA_BASE       0x800000000UL
#define MAP_SHMEM_MM_RANGE      0x1FFFFFFFFUL
#define MAP_SHMEM_MM_END        (MAP_SHMEM_MM_BASE + MAP_SHMEM_MM_RANGE)
#define MAP_SHMEM_MM_CACHEABLE_END (MAP_SHMEM_MM_CACHEABLE_BASE \
+ MAP_SHMEM_MM_RANGE)

struct mtk_vcu *vcu_ptr;
static char *vcodec_param_string = "";

inline int ipi_id_to_inst_id(int id)
{
	/* Assume VENC uses instance 1 and others use 0. */
	if (id < IPI_VENC_COMMON && id >= IPI_VCU_INIT)
		return VCU_VDEC;
	else
		return VCU_VENC;
}

#define vcu_dbg_log(fmt, arg...) do { \
		if (vcu_ptr->enable_vcu_dbg_log) \
			pr_info(fmt, ##arg); \
	} while (0)

#define MAP_PA_BASE_1GB  0x40000000 /* < 1GB registers */
#define VCU_MAP_HW_REG_NUM 4
/* VDEC VDEC_LAT VENC_CORE0 VENC_CORE1 */

/* Default vcu_mtkdev[0] handle vdec, vcu_mtkdev[1] handle mdp */
static struct mtk_vcu *vcu_mtkdev[MTK_VCU_NR_MAX];

static struct task_struct *vcud_task;
static struct files_struct *files;

/* for protecting vpud file struct */
struct mutex vpud_file_mutex;

static __attribute__((used)) unsigned int time_ms_s, time_ms_e;
#define time_check_start() { \
		time_ms_s = jiffies_to_msecs(jiffies); \
	}
#define time_check_end(timeout_ms, debug) do { \
		time_ms_e = jiffies_to_msecs(jiffies); \
		if ((time_ms_e - time_ms_s) > timeout_ms || \
			debug) \
			pr_info("[VCU][Info] %s L:%d take %u timeout %u ms", \
				__func__, __LINE__, \
				time_ms_e - time_ms_s, \
				timeout_ms); \
	} while (0)

/**
 * struct vcu_mem - VCU memory information
 *
 * @p_vma:      the user virtual memory address of
 *              VCU extended program memory
 * @d_vma:      the user  virtual memory address of VCU extended data memory
 * @p_va:       the kernel virtual memory address of
 *              VCU extended program memory
 * @d_va:       the kernel virtual memory address of VCU extended data memory
 * @p_pa:       the physical memory address of VCU extended program memory
 * @d_pa:       the physical memory address of VCU extended data memory
 * @p_iova:     the iova memory address of VCU extended program memory
 * @d_iova:     the iova memory address of VCU extended data memory
 */
struct vcu_mem {
	unsigned long p_vma;
	unsigned long d_vma;
	void *p_va;
	void *d_va;
	dma_addr_t p_pa;
	dma_addr_t d_pa;
	dma_addr_t p_iova;
	dma_addr_t d_iova;
	unsigned long p_len;
	unsigned long d_len;
};

/**
 * struct vcu_run - VCU initialization status
 *
 * @signaled:           the signal of vcu initialization completed
 * @fw_ver:             VCU firmware version
 * @dec_capability:     decoder capability which is not used for now and
 *                      the value is reserved for future use
 * @enc_capability:     encoder capability which is not used for now and
 *                      the value is reserved for future use
 * @wq:                 wait queue for VCU initialization status
 */
struct vcu_run {
	u32 signaled;
	char fw_ver[VCU_FW_VER_LEN];
	unsigned int    dec_capability;
	unsigned int    enc_capability;
	wait_queue_head_t wq;
};

/**
 * struct vcu_ipi_desc - VCU IPI descriptor
 *
 * @handler:    IPI handler
 * @name:       the name of IPI handler
 * @priv:       the private data of IPI handler
 */
struct vcu_ipi_desc {
	ipi_handler_t handler;
	const char *name;
	void *priv;
};

struct map_hw_reg {
	unsigned long base;
	unsigned long len;
};

struct vcu_pa_pages {
	unsigned long pa;
	unsigned long kva;
	struct list_head list;
};

struct gce_callback_data {
	struct gce_cmdq_obj cmdq_buff;
	struct mtk_vcu *vcu_ptr;
	struct cmdq_pkt *pkt_ptr;
};

struct gce_ctx_info {
	void *v4l2_ctx;
	u64 user_hdl;
	atomic_t flush_done;
	/* gce callbacked but user not waited cnt */
	struct gce_callback_data buff[GCE_PENDING_CNT];
	atomic_t flush_pending;
	/* gce not callbacked cnt */
};

/**
 * struct mtk_vcu - vcu driver data
 * @extmem:             VCU extended memory information
 * @run:                VCU initialization status
 * @ipi_desc:           VCU IPI descriptor
 * @dev:                VCU struct device
 * @vcu_mutex:          protect mtk_vcu (except recv_buf) and ensure only
 *                      one client to use VCU service at a time. For example,
 *                      suppose a client is using VCU to decode VP8.
 *                      If the other client wants to encode VP8,
 *                      it has to wait until VP8 decode completes.
 * @vcu_gce_mutex       protect mtk_vcu gce flush & callback power sequence
 * @file:               VCU daemon file pointer
 * @is_open:            The flag to indicate if VCUD device is open.
 * @ack_wq:             The wait queue for each codec and mdp. When sleeping
 *                      processes wake up, they will check the condition
 *                      "ipi_id_ack" to run the corresponding action or
 *                      go back to sleep.
 * @ipi_id_ack:         The ACKs for registered IPI function sending
 *                      interrupt to VCU
 * @get_wq:             When sleeping process waking up, it will check the
 *                      condition "ipi_got" to run the corresponding action or
 *                      go back to sleep.
 * @ipi_got:            The flags for IPI message polling from user.
 * @ipi_done:           The flags for IPI message polling from user again, which
 *                      means the previous messages has been dispatched done in
 *                      daemon.
 * @user_obj:           Temporary share_obj used for ipi_msg_get.
 * @vcu_devno:          The vcu_devno for vcu init vcu character device
 * @vcu_cdev:           The point of vcu character device.
 * @vcu_class:          The class_create for create vcu device
 * @vcu_device:         VCU struct device
 * @vcuname:            VCU struct device name in dtsi
 * @path:               The path to keep mdpd path or vcud path.
 * @vpuid:              VCU device id
 *
 */
struct mtk_vcu {
	struct vcu_mem extmem;
	struct vcu_run run;
	struct vcu_ipi_desc ipi_desc[IPI_MAX];
	struct device *dev;
	struct mutex vcu_mutex[VCU_CODEC_MAX];
	struct mutex vcu_gce_mutex[VCU_CODEC_MAX];
	/* for protecting vcu data structure */
	struct mutex vcu_share;
	struct file *file;
	struct iommu_domain *io_domain;
	bool   iommu_padding;
	/* temp for 33bits larb adding bits "1" iommu */
	struct map_hw_reg map_base[VCU_MAP_HW_REG_NUM];
	bool   is_open;
	wait_queue_head_t ack_wq[VCU_CODEC_MAX];
	bool ipi_id_ack[IPI_MAX];
	wait_queue_head_t get_wq[VCU_CODEC_MAX];
	atomic_t ipi_got[VCU_CODEC_MAX];
	atomic_t ipi_done[VCU_CODEC_MAX];
	struct share_obj user_obj[VCU_CODEC_MAX];
	dev_t vcu_devno;
	struct cdev *vcu_cdev;
	struct class *vcu_class;
	struct device *vcu_device;
	const char *vcuname;
	const char *path;
	int vcuid;
	struct log_test_nofuse *vdec_log_info;
	wait_queue_head_t vdec_log_get_wq;
	atomic_t vdec_log_got;
	struct cmdq_base *clt_base;
	struct cmdq_client *clt_vdec[GCE_THNUM_MAX];
	struct cmdq_client *clt_venc[GCE_THNUM_MAX];
	struct vcu_pa_pages pa_pages;
	int gce_th_num[VCU_CODEC_MAX];
	int gce_codec_eid[GCE_EVENT_MAX];
	struct gce_cmds *gce_cmds[VCU_CODEC_MAX];
	void *curr_ctx[VCU_CODEC_MAX];
	struct vb2_queue *curr_src_vq[VCU_CODEC_MAX];
	struct vb2_queue *curr_dst_vq[VCU_CODEC_MAX];
	wait_queue_head_t gce_wq[VCU_CODEC_MAX];
	struct gce_ctx_info gce_info[VCODEC_INST_MAX];
	atomic_t gce_job_cnt[VCU_CODEC_MAX][GCE_THNUM_MAX];
	unsigned long flags[VCU_CODEC_MAX];
	int open_cnt;
	bool abort;
	struct semaphore vpud_killed;
	bool is_entering_suspend;
	u32 gce_gpr[GCE_THNUM_MAX];
	/* for gce poll timer, multi-thread sync */

	/* for vpud sig check */
	spinlock_t vpud_sig_lock;
	int vpud_is_going_down;

	/* for vcu dbg log*/
	int enable_vcu_dbg_log;
};

static inline bool vcu_running(struct mtk_vcu *vcu)
{
	return (bool)vcu->run.signaled;
}

int vcu_ipi_register(struct platform_device *pdev,
		     enum ipi_id id, ipi_handler_t handler,
		     const char *name, void *priv)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	struct vcu_ipi_desc *ipi_desc;
	int i = 0;

	if (vcu == NULL) {
		dev_err(&pdev->dev, "vcu device in not ready\n");
		return -EPROBE_DEFER;
	}

	if (id < IPI_VCU_INIT || id >= IPI_MAX) {
		dev_info(&pdev->dev, "[VCU] failed to register ipi message (Invalid arg.)\n");
		return -EINVAL;
	}

	i = ipi_id_to_inst_id(id);
	mutex_lock(&vcu->vcu_mutex[i]);

	if (id >= IPI_VCU_INIT && id < IPI_MAX && handler != NULL) {
		ipi_desc = vcu->ipi_desc;
		ipi_desc[id].name = name;
		ipi_desc[id].handler = handler;
		ipi_desc[id].priv = priv;
		mutex_unlock(&vcu->vcu_mutex[i]);
		return 0;
	}
	mutex_unlock(&vcu->vcu_mutex[i]);

	dev_err(&pdev->dev, "register vcu ipi id %d with invalid arguments\n",
		id);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(vcu_ipi_register);

int vcu_ipi_send(struct platform_device *pdev,
		 enum ipi_id id, void *buf,
		 unsigned int len, void *priv)
{
	int i = 0;
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	struct vcu_ipi_desc *ipi_desc;
	struct share_obj send_obj;
	unsigned long timeout;
	int ret;

	if (id <= IPI_VCU_INIT || id >= IPI_MAX ||
	    len > sizeof(send_obj.share_buf) || buf == NULL) {
		dev_err(&pdev->dev, "[VCU] failed to send ipi message (Invalid arg.)\n");
		return -EINVAL;
	}

	if (vcu_running(vcu) == false) {
		dev_err(&pdev->dev, "[VCU] %s: VCU is not running\n", __func__);
		return -EPERM;
	}

	if (vcu_ptr->abort) {
		if (vcu_ptr->open_cnt > 0) {
			dev_info(vcu->dev, "wait for vpud killed %d\n",
				vcu_ptr->vpud_killed.count);
			ret = down_interruptible(&vcu_ptr->vpud_killed);
		}
		dev_info(&pdev->dev, "[VCU] vpud killed\n");
		return -EIO;
	}

	i = ipi_id_to_inst_id(id);

	mutex_lock(&vcu->vcu_mutex[i]);
	vcu->ipi_id_ack[id] = false;

	if (id >= IPI_VCU_INIT && id < IPI_MAX) {
		ipi_desc = vcu->ipi_desc;
		ipi_desc[id].priv = priv;
	}

	/* send the command to VCU */
	memcpy((void *)vcu->user_obj[i].share_buf, buf, len);
	vcu->user_obj[i].len = len;
	vcu->user_obj[i].id = (int)id;
	atomic_set(&vcu->ipi_got[i], 1);
	atomic_set(&vcu->ipi_done[i], 0);
	wake_up(&vcu->get_wq[i]);
	ret = 0;

	if (ret != 0) {
		dev_err(&pdev->dev,
			"[VCU] failed to send ipi message (ret=%d)\n", ret);
		mutex_unlock(&vcu->vcu_mutex[i]);
		goto end;
	}

	/* wait for VCU's ACK */
	timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
	ret = wait_event_timeout(vcu->ack_wq[i], vcu->ipi_id_ack[id], timeout);
	vcu->ipi_id_ack[id] = false;
	mutex_unlock(&vcu->vcu_mutex[i]);

	if (vcu_ptr->abort || ret == 0) {
		dev_info(&pdev->dev, "vcu ipi %d ack time out !%d", id, ret);
		if (!vcu_ptr->abort) {
			task_lock(vcud_task);
			send_sig(SIGTERM, vcud_task, 0);
			task_unlock(vcud_task);
		}
		if (vcu_ptr->open_cnt > 0) {
			dev_info(vcu->dev, "wait for vpud killed %d\n",
				vcu_ptr->vpud_killed.count);
			ret = down_interruptible(&vcu_ptr->vpud_killed);
		}
		dev_info(&pdev->dev, "[VCU] vpud killed\n");
		ret = -EIO;
		goto end;
	} else if (-ERESTARTSYS == ret) {
		dev_err(&pdev->dev, "vcu ipi %d ack wait interrupted by a signal",
			id);
		ret = -ERESTARTSYS;
		goto end;
	} else
		ret = 0;

	/* Waiting ipi_done, success means the daemon receiver thread
	 * dispatchs ipi msg done and returns to kernel for get next
	 * ipi msg.
	 * The dispatched ipi msg is being processed by app service.
	 * Usually, it takes dozens of microseconds in average.
	 */
	while (atomic_read(&vcu->ipi_done[i]) == 0)
		cond_resched();

end:
	return ret;
}
EXPORT_SYMBOL_GPL(vcu_ipi_send);

static int vcu_ipi_get(struct mtk_vcu *vcu, unsigned long arg)
{
	int i = 0, ret;
	unsigned char *user_data_addr = NULL;
	struct share_obj share_buff_data;

	user_data_addr = (unsigned char *)arg;
	ret = (long)copy_from_user(&share_buff_data, user_data_addr,
				   (unsigned long)sizeof(struct share_obj));
	i = ipi_id_to_inst_id(share_buff_data.id);

	/* mutex protection here is unnecessary, since different app service
	 * threads of daemon are corresponding to different vcu_ipi_get thread.
	 * Different threads use differnet variables, e.g. ipi_done.
	 */
	atomic_set(&vcu->ipi_done[i], 1);
	ret = wait_event_freezable(vcu->get_wq[i],
				   atomic_read(&vcu->ipi_got[i]));

	if (ret != 0) {
		pr_info("[VCU][%d][%d] wait event return %d @%s\n",
			vcu->vcuid, i, ret, __func__);
		return ret;
	}
	ret = copy_to_user(user_data_addr, &vcu->user_obj[i],
			   (unsigned long)sizeof(struct share_obj));
	if (ret != 0) {
		pr_info("[VCU] %s(%d) Copy data to user failed!\n",
			__func__, __LINE__);
		ret = -EINVAL;
	}
	atomic_set(&vcu->ipi_got[i], 0);

	return ret;
}

static int vcu_log_get(struct mtk_vcu *vcu, unsigned long arg)
{
	int ret;
	unsigned char *user_data_addr = NULL;

	user_data_addr = (unsigned char *)arg;

	ret = wait_event_freezable(vcu->vdec_log_get_wq,
				   atomic_read(&vcu->vdec_log_got));
	if (ret != 0) {
		pr_info("[VCU][%d] wait event return %d @%s\n",
			vcu->vcuid, ret, __func__);
		return ret;
	}
	ret = copy_to_user(user_data_addr, vcu->vdec_log_info,
			   (unsigned long)sizeof(struct log_test_nofuse));
	if (ret != 0) {
		pr_info("[VCU] %s(%d) Copy data to user failed!\n",
			__func__, __LINE__);
		ret = -EINVAL;
	}
	atomic_set(&vcu->vdec_log_got, 0);

	return ret;
}


static int vcu_gce_set_inst_id(void *ctx, u64 gce_handle)
{
	int i;

	mutex_lock(&vcu_ptr->vcu_share);
	for (i = 0; i < VCODEC_INST_MAX; i++) {
		if (vcu_ptr->gce_info[i].v4l2_ctx == NULL) {
			vcu_ptr->gce_info[i].v4l2_ctx = ctx;
			vcu_ptr->gce_info[i].user_hdl = gce_handle;
			mutex_unlock(&vcu_ptr->vcu_share);
			pr_debug("[VCU] %s ctx %p hndl %llu create id %d\n",
				__func__, ctx, gce_handle, i);
			return i;
		}
	}
	mutex_unlock(&vcu_ptr->vcu_share);
	pr_info("[VCU] %s fail ctx %p hndl %llu\n",
		__func__, ctx, gce_handle);

	return -1;
}


static int vcu_gce_get_inst_id(u64 gce_handle)
{
	int i, temp;

	mutex_lock(&vcu_ptr->vcu_share);
	for (i = 0; i < VCODEC_INST_MAX; i++) {
		if (vcu_ptr->gce_info[i].user_hdl == gce_handle) {
			temp = atomic_read(&vcu_ptr->gce_info[i].flush_done);
			mutex_unlock(&vcu_ptr->vcu_share);
			pr_debug("[VCU] %s hndl %llu get id %d cnt %d\n",
				__func__, gce_handle, i, temp);
			return i;
		}
	}
	mutex_unlock(&vcu_ptr->vcu_share);

	return -1;
}

static void vcu_gce_clear_inst_id(void *ctx)
{
	int i, temp, temp2;
	u64 gce_handle;

	mutex_lock(&vcu_ptr->vcu_share);
	for (i = 0; i < VCODEC_INST_MAX; i++) {
		if (vcu_ptr->gce_info[i].v4l2_ctx == ctx) {
			gce_handle = vcu_ptr->gce_info[i].user_hdl;
			vcu_ptr->gce_info[i].v4l2_ctx = NULL;
			vcu_ptr->gce_info[i].user_hdl = 0;
			temp = atomic_read(&vcu_ptr->gce_info[i].flush_pending);
			/* flush_pending > 0, ctx hw not unprepared */
			temp2 = atomic_read(&vcu_ptr->gce_info[i].flush_done);
			/* flush_done > 0, user event not waited */
			atomic_set(&vcu_ptr->gce_info[i].flush_done, 0);
			atomic_set(&vcu_ptr->gce_info[i].flush_pending, 0);
			mutex_unlock(&vcu_ptr->vcu_share);
			if (temp > 0)
				vcu_aee_print(
					"%s ctx %p hndl %llu free id %d cnt %d %d\n",
					__func__, ctx, gce_handle,
					i, temp, temp2);
			else if (temp2 > 0)
				pr_info("[VCU] %s ctx %p hndl %llu free id %d cnt %d %d\n",
					__func__, ctx, gce_handle,
					i, temp, temp2);
			else
				pr_debug("[VCU] %s ctx %p hndl %llu free id %d cnt %d %d\n",
					__func__, ctx, gce_handle,
					i, temp, temp2);
			return;
		}
	}
	mutex_unlock(&vcu_ptr->vcu_share);
}


static void vcu_set_gce_cmd(struct cmdq_pkt *pkt,
	struct mtk_vcu *vcu, unsigned char cmd,
	u64 addr, u64 data, u32 mask, u32 gpr)
{
	switch (cmd) {
	case CMD_READ:
		cmdq_pkt_read_addr(pkt, addr, CMDQ_THR_SPR_IDX1);
	break;
	case CMD_WRITE:
		cmdq_pkt_write(pkt, vcu->clt_base, addr, data, mask);
	break;
	case CMD_POLL_REG:
		cmdq_pkt_poll_addr(pkt, data, addr, mask, gpr);
	break;
	case CMD_WAIT_EVENT:
		if (data < GCE_EVENT_MAX)
			cmdq_pkt_wfe(pkt, vcu->gce_codec_eid[data]);
		else
			pr_info("[VCU] %s got wrong eid %llu\n",
				__func__, data);
	break;
	case CMD_MEM_MV:
		cmdq_pkt_mem_move(pkt, vcu->clt_base, addr,
			data, CMDQ_THR_SPR_IDX1);
	break;
	case CMD_POLL_ADDR:
		cmdq_pkt_poll_timeout(pkt, data, SUBSYS_NO_SUPPORT,
			addr, mask, ~0, gpr);
	break;
	default:
		pr_debug("[VCU] unknown GCE cmd %d\n", cmd);
	break;
	}
}

static void vcu_gce_flush_callback(struct cmdq_cb_data data)
{
	int i, j;
	struct gce_callback_data *buff;
	struct mtk_vcu *vcu;
	unsigned int core_id;

	buff = (struct gce_callback_data *)data.data;
	i = (buff->cmdq_buff.codec_type == VCU_VDEC) ? VCU_VDEC : VCU_VENC;
	core_id = buff->cmdq_buff.core_id;

	vcu = buff->vcu_ptr;
	j = vcu_gce_get_inst_id(buff->cmdq_buff.gce_handle);

	if (j < 0) {
		pr_info("[VCU] flush_callback get_inst_id fail!!%d\n", j);
		return;
	}

	atomic_inc(&vcu->gce_info[j].flush_done);
	atomic_dec(&vcu->gce_info[j].flush_pending);

	mutex_lock(&vcu->vcu_gce_mutex[i]);
	venc_encode_pmqos_gce_end(vcu->gce_info[j].v4l2_ctx, core_id,
				vcu->gce_job_cnt[i][core_id].counter);
	if (atomic_dec_and_test(&vcu->gce_job_cnt[i][core_id]) &&
		vcu->gce_info[j].v4l2_ctx != NULL){
		if (i == VCU_VENC)
			venc_encode_unprepare(vcu->gce_info[j].v4l2_ctx,
				buff->cmdq_buff.core_id, &vcu->flags[i]);
	}
	mutex_unlock(&vcu->vcu_gce_mutex[i]);

	wake_up(&vcu->gce_wq[i]);

	vcu_dbg_log("[VCU][%d] %s: buff %p type %d order %d handle %llx\n",
		core_id, __func__, buff, buff->cmdq_buff.codec_type,
		buff->cmdq_buff.flush_order, buff->cmdq_buff.gce_handle);

	cmdq_pkt_destroy(buff->pkt_ptr);
}

static void vcu_gce_timeout_callback(struct cmdq_cb_data data)
{
	struct gce_callback_data *buff;
	struct mtk_vcu *vcu;
	struct list_head *p, *q;
	struct vcu_pa_pages *tmp;

	buff = (struct gce_callback_data *)data.data;
	vcu = buff->vcu_ptr;
	pr_debug("%s: buff %p vcu: %p, codec_typ: %d\n",
		__func__, buff, vcu, buff->cmdq_buff.codec_type);

	if (buff->cmdq_buff.codec_type == VCU_VENC)
		mtk_vcodec_gce_timeout_dump(vcu->curr_ctx[VCU_VENC]);
	else if (buff->cmdq_buff.codec_type == VCU_VDEC)
		mtk_vcodec_gce_timeout_dump(vcu->curr_ctx[VCU_VDEC]);

	list_for_each_safe(p, q, &vcu->pa_pages.list) {
		tmp = list_entry(p, struct vcu_pa_pages, list);
		pr_info("%s: vcu_pa_pages %lx kva %lx data %lx\n",
			__func__, tmp->pa, tmp->kva,
			*(unsigned long *)tmp->kva);
	}

}

static int vcu_gce_cmd_flush(struct mtk_vcu *vcu, unsigned long arg)
{
	int i, j, ret;
	unsigned char *user_data_addr = NULL;
	struct gce_callback_data buff;
	struct cmdq_pkt *pkt_ptr;
	struct cmdq_client *cl;
	struct gce_cmds *cmds;
	unsigned int suspend_block_cnt = 0;
	unsigned int core_id;

	pr_debug("[VCU] %s +\n", __func__);

	time_check_start();
	user_data_addr = (unsigned char *)arg;
	ret = (long)copy_from_user(&buff.cmdq_buff, user_data_addr,
				   (unsigned long)sizeof(struct gce_cmdq_obj));
	if (ret != 0L)
		pr_info("[VCU] %s(%d) gce_cmdq_obj copy_from_user failed!%d\n",
			__func__, __LINE__, ret);

	i = (buff.cmdq_buff.codec_type == VCU_VDEC) ? VCU_VDEC : VCU_VENC;
	cmds = vcu->gce_cmds[i];

	user_data_addr = (unsigned char *)
				   (unsigned long)buff.cmdq_buff.cmds_user_ptr;
	ret = (long)copy_from_user(cmds, user_data_addr,
				   (unsigned long)sizeof(struct gce_cmds));
	if (ret != 0L)
		pr_info("[VCU] %s(%d) gce_cmds copy_from_user failed!%d\n",
			__func__, __LINE__, ret);

	buff.cmdq_buff.cmds_user_ptr = (u64)(unsigned long)cmds;
	core_id = buff.cmdq_buff.core_id;

	if (buff.cmdq_buff.codec_type >= VCU_CODEC_MAX ||
		core_id >=
		vcu->gce_th_num[buff.cmdq_buff.codec_type]) {
		pr_info("[VCU] %s invalid core(th) id %d\n",
			__func__, core_id);
		return -EINVAL;
	}

	cl = (buff.cmdq_buff.codec_type == VCU_VDEC) ?
		vcu->clt_vdec[core_id] :
		vcu->clt_venc[core_id];

	if (cl == NULL) {
		pr_info("[VCU] %s gce thread is null id %d type %d\n",
			__func__, core_id,
			buff.cmdq_buff.codec_type);
		return -EINVAL;
	}

	buff.vcu_ptr = vcu;

	while (vcu_ptr->is_entering_suspend == 1) {
		suspend_block_cnt++;
		if (suspend_block_cnt > 500) {
			pr_info("[VCU] gce_flush blocked by suspend\n");
			suspend_block_cnt = 0;
		}
		usleep_range(10000, 20000);
	}

	j = vcu_gce_get_inst_id(buff.cmdq_buff.gce_handle);

	if (j < 0)
		j = vcu_gce_set_inst_id(vcu->curr_ctx[i],
			buff.cmdq_buff.gce_handle);
	if (j < 0)
		return -EINVAL;

	time_check_end(100, strlen(vcodec_param_string));

	time_check_start();
	mutex_lock(&vcu->vcu_gce_mutex[i]);
	if (atomic_read(&vcu->gce_job_cnt[i][core_id]) == 0 &&
		vcu->gce_info[j].v4l2_ctx != NULL){
		if (i == VCU_VENC) {
			venc_encode_prepare(vcu->gce_info[j].v4l2_ctx,
				core_id, &vcu->flags[i]);
		}
	}
	pr_debug("vcu gce_info[%d].v4l2_ctx %p\n",
		j, vcu->gce_info[j].v4l2_ctx);
	if (i == VCU_VENC) {
		venc_encode_pmqos_gce_begin(vcu->gce_info[j].v4l2_ctx, core_id,
			vcu->gce_job_cnt[i][core_id].counter);
	}
	atomic_inc(&vcu->gce_job_cnt[i][core_id]);
	mutex_unlock(&vcu->vcu_gce_mutex[i]);
	time_check_end(100, strlen(vcodec_param_string));

	time_check_start();
	pkt_ptr = cmdq_pkt_create(cl);
	if (IS_ERR(pkt_ptr)) {
		pr_info("[VCU] cmdq_pkt_create fail\n");
		pkt_ptr = NULL;
	}
	buff.pkt_ptr = pkt_ptr;

	if (cmds->cmd_cnt >= VCODEC_CMDQ_CMD_MAX) {
		pr_info("[VCU] cmd_cnt (%d) overflow!!\n", cmds->cmd_cnt);
		cmds->cmd_cnt = VCODEC_CMDQ_CMD_MAX;
		ret = -EINVAL;
	}
	for (i = 0; i < cmds->cmd_cnt; i++) {
		vcu_set_gce_cmd(pkt_ptr, vcu, cmds->cmd[i],
			cmds->addr[i], cmds->data[i],
			cmds->mask[i], vcu->gce_gpr[core_id]);
	}

	i = buff.cmdq_buff.flush_order % GCE_PENDING_CNT;
	memcpy(&vcu_ptr->gce_info[j].buff[i], &buff, sizeof(buff));

	pkt_ptr->err_cb.cb = vcu_gce_timeout_callback;
	pkt_ptr->err_cb.data = (void *)&vcu_ptr->gce_info[j].buff[i];

	pr_info("[VCU][%d] %s: buff %p type %d cnt %d order %d hndl %llx %d %d\n",
		core_id, __func__, &vcu_ptr->gce_info[j].buff[i],
		buff.cmdq_buff.codec_type,
		cmds->cmd_cnt, buff.cmdq_buff.flush_order,
		buff.cmdq_buff.gce_handle, ret, j);

	/* flush cmd async */
	cmdq_pkt_flush_threaded(pkt_ptr,
		vcu_gce_flush_callback, (void *)&vcu_ptr->gce_info[j].buff[i]);

	atomic_inc(&vcu_ptr->gce_info[j].flush_pending);
	time_check_end(100, strlen(vcodec_param_string));

	return ret;
}

static int vcu_wait_gce_callback(struct mtk_vcu *vcu, unsigned long arg)
{
	int ret, i, j;
	unsigned char *user_data_addr = NULL;
	struct gce_obj obj;

	user_data_addr = (unsigned char *)arg;
	ret = (long)copy_from_user(&obj, user_data_addr,
				   (unsigned long)sizeof(struct gce_obj));

	i = (obj.codec_type == VCU_VDEC) ? VCU_VDEC : VCU_VENC;
	pr_debug("[VCU] %s: type %d handle %llx\n",
		__func__, obj.codec_type, obj.gce_handle);

	/* use wait_event_interruptible not freezable due to
	 * slowmotion GCE case vcu_gce_cmd_flush will hold
	 * mutex in user process which cannot be freezed
	 */
	j = vcu_gce_get_inst_id(obj.gce_handle);

	if (j < 0)
		return -EINVAL;

	ret = wait_event_interruptible(vcu->gce_wq[i],
		atomic_read(&vcu->gce_info[j].flush_done) > 0);
	if (ret != 0) {
		pr_info("[VCU][%d][%d] wait event return %d @%s\n",
			vcu->vcuid, i, ret, __func__);
		return ret;
	}
	atomic_dec(&vcu->gce_info[j].flush_done);

	return ret;
}

int vcu_set_codec_ctx(struct platform_device *pdev,
		 void *codec_ctx, struct vb2_queue *src_vq,
		 struct vb2_queue *dst_vq, unsigned long type)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	vcu->curr_ctx[type] = codec_ctx;
	vcu->curr_src_vq[type] = src_vq;
	vcu->curr_dst_vq[type] = dst_vq;
	return 0;
}

int vcu_clear_codec_ctx(struct platform_device *pdev,
		 void *codec_ctx, unsigned long type)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	mutex_lock(&vcu->vcu_gce_mutex[type]);
	vcu_gce_clear_inst_id(codec_ctx);
	vcu->curr_ctx[type] = NULL;
	vcu->curr_src_vq[type] = NULL;
	vcu->curr_dst_vq[type] = NULL;
	mutex_unlock(&vcu->vcu_gce_mutex[type]);

	return 0;
}

unsigned int vcu_get_vdec_hw_capa(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	return vcu->run.dec_capability;
}
EXPORT_SYMBOL_GPL(vcu_get_vdec_hw_capa);

unsigned int vcu_get_venc_hw_capa(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	return vcu->run.enc_capability;
}
EXPORT_SYMBOL_GPL(vcu_get_venc_hw_capa);

void *vcu_mapping_dm_addr(struct platform_device *pdev,
			  uintptr_t dtcm_dmem_addr)
{
	struct mtk_vcu *vcu;
	uintptr_t d_vma, d_va_start;
	uintptr_t d_off, d_va;

	if (!IS_ERR_OR_NULL(pdev))
		vcu = platform_get_drvdata(pdev);
	else {
		dev_info(&pdev->dev, "[VCU] %s: Invalid pdev %p\n",
			__func__, pdev);
		return NULL;
	}

	d_vma = (uintptr_t)(dtcm_dmem_addr);
	d_va_start = (uintptr_t)VCU_DMEM0_VIRT(vcu);
	d_off = d_vma - VCU_DMEM0_VMA(vcu);

	if (dtcm_dmem_addr == 0UL || d_off > VCU_DMEM0_LEN(vcu)) {
		dev_info(&pdev->dev, "[VCU] %s: Invalid vma 0x%lx len %lx\n",
			__func__, dtcm_dmem_addr, VCU_DMEM0_LEN(vcu));
		return NULL;
	}

	d_va = d_va_start + d_off;
	dev_dbg(&pdev->dev, "[VCU] %s: 0x%lx -> 0x%lx\n",
		__func__, d_vma, d_va);

	return (void *)d_va;
}
EXPORT_SYMBOL_GPL(vcu_mapping_dm_addr);

struct platform_device *vcu_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *vcu_node = NULL;
	struct platform_device *vcu_pdev = NULL;

	dev_dbg(&pdev->dev, "[VCU] %s\n", __func__);

	vcu_node = of_parse_phandle(dev->of_node, "mediatek,vcu", 0);
	if (vcu_node == NULL) {
		dev_err(dev, "[VCU] can't get vcu node\n");
		return NULL;
	}

	vcu_pdev = of_find_device_by_node(vcu_node);
	if (WARN_ON(vcu_pdev == NULL) == true) {
		dev_err(dev, "[VCU] vcu pdev failed\n");
		of_node_put(vcu_node);
		return NULL;
	}

	return vcu_pdev;
}
EXPORT_SYMBOL_GPL(vcu_get_plat_device);

int vcu_load_firmware(struct platform_device *pdev)
{
	if (pdev == NULL) {
		dev_err(&pdev->dev, "[VCU] VCU platform device is invalid\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(vcu_load_firmware);

int vcu_compare_version(struct platform_device *pdev,
			const char *expected_version)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	int cur_major, cur_minor, cur_build, cur_rel, cur_ver_num;
	int major, minor, build, rel, ver_num;
	char *cur_version = vcu->run.fw_ver;

	cur_ver_num = sscanf(cur_version, "%d.%d.%d-rc%d",
			     &cur_major, &cur_minor, &cur_build, &cur_rel);
	if (cur_ver_num < 3)
		return -1;
	ver_num = sscanf(expected_version, "%d.%d.%d-rc%d",
			 &major, &minor, &build, &rel);
	if (ver_num < 3)
		return -1;

	if (cur_major < major)
		return -1;
	if (cur_major > major)
		return 1;

	if (cur_minor < minor)
		return -1;
	if (cur_minor > minor)
		return 1;

	if (cur_build < build)
		return -1;
	if (cur_build > build)
		return 1;

	if (cur_ver_num < ver_num)
		return -1;
	if (cur_ver_num > ver_num)
		return 1;

	if (ver_num > 3) {
		if (cur_rel < rel)
			return -1;
		if (cur_rel > rel)
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vcu_compare_version);

void vcu_get_file_lock(void)
{
	mutex_lock(&vpud_file_mutex);
}
EXPORT_SYMBOL_GPL(vcu_get_file_lock);

void vcu_put_file_lock(void)
{
	mutex_unlock(&vpud_file_mutex);
}
EXPORT_SYMBOL_GPL(vcu_put_file_lock);

int vcu_get_sig_lock(unsigned long flags)
{
	return spin_trylock_irqsave(&vcu_ptr->vpud_sig_lock, flags);
}
EXPORT_SYMBOL_GPL(vcu_get_sig_lock);

void vcu_put_sig_lock(unsigned long flags)
{
	spin_unlock_irqrestore(&vcu_ptr->vpud_sig_lock, flags);
}
EXPORT_SYMBOL_GPL(vcu_put_sig_lock);

int vcu_check_vpud_alive(void)
{
	return (vcu_ptr->vpud_is_going_down > 0) ? 0:1;
}
EXPORT_SYMBOL_GPL(vcu_check_vpud_alive);

void vcu_get_task(struct task_struct **task, struct files_struct **f,
		int reset)
{
	pr_debug("mtk_vcu_get_task %p\n", vcud_task);

	if (reset == 1) {
		vcud_task = NULL;
		files = NULL;
	}

	*task = vcud_task;
	*f = files;
}
EXPORT_SYMBOL_GPL(vcu_get_task);

static int vcu_ipi_handler(struct mtk_vcu *vcu, struct share_obj *rcv_obj)
{
	struct vcu_ipi_desc *ipi_desc = vcu->ipi_desc;
	int non_ack = 0;
	int ret = -1;
	int i = 0;

	i = ipi_id_to_inst_id(rcv_obj->id);

	if (rcv_obj->id < (int)IPI_MAX &&
	    ipi_desc[rcv_obj->id].handler != NULL) {
		non_ack = ipi_desc[rcv_obj->id].handler(rcv_obj->share_buf,
				rcv_obj->len,
				ipi_desc[rcv_obj->id].priv);
		if (rcv_obj->id > (int)IPI_VCU_INIT && non_ack == 0) {
			vcu->ipi_id_ack[rcv_obj->id] = true;
			wake_up(&vcu->ack_wq[i]);
		}
		ret = 0;
	} else
		dev_err(vcu->dev, "[VCU] No such ipi id = %d\n", rcv_obj->id);

	return ret;
}

static int vcu_ipi_init(struct mtk_vcu *vcu)
{
	vcu->is_open = false;
	mutex_init(&vcu->vcu_mutex[VCU_VDEC]);
	mutex_init(&vcu->vcu_mutex[VCU_VENC]);
	mutex_init(&vcu->vcu_gce_mutex[VCU_VDEC]);
	mutex_init(&vcu->vcu_gce_mutex[VCU_VENC]);
	mutex_init(&vcu->vcu_share);
	mutex_init(&vpud_file_mutex);

	return 0;
}

static int vcu_init_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_vcu *vcu = (struct mtk_vcu *)priv;
	struct vcu_run *run = (struct vcu_run *)data;
	int wait_cnt = 0;

	/* handle uninitialize message */
	if (vcu->run.signaled == 1u && run->signaled == 0u) {
		int i;
		/* wake up the threads in daemon
		 * clear all pending ipi_msg
		 * release worker waiting timeout
		 */
		vcu->abort = true;
		for (i = 0; i < IPI_MAX; i++)
			vcu->ipi_id_ack[i] = true;

		/* wait for GCE done & let IPI ack power off */
		while (
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VDEC][0]) > 0 ||
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VDEC][1]) > 0 ||
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VENC][0]) > 0 ||
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VENC][1]) > 0) {
			wait_cnt++;
			if (wait_cnt > 5) {
				pr_info("[VCU] Vpud killed gce status %d %d\n",
				atomic_read(
				&vcu_ptr->gce_job_cnt[VCU_VDEC][0]),
				atomic_read(
				&vcu_ptr->gce_job_cnt[VCU_VENC][0]));
				break;
			}
			usleep_range(10000, 20000);
		}

		for (i = 0; i < 2; i++) {
			atomic_set(&vcu->ipi_got[i], 1);
			atomic_set(&vcu->ipi_done[i], 0);
			memset(&vcu->user_obj[i], 0,
				sizeof(struct share_obj));
			wake_up(&vcu->get_wq[i]);
			wake_up(&vcu->ack_wq[i]);
		}

		atomic_set(&vcu->vdec_log_got, 1);
		wake_up(&vcu->vdec_log_get_wq);
		vcud_task = NULL;
		files = NULL;

		dev_info(vcu->dev, "[VCU] vpud killing\n");

		return 0;
	}

	vcu->run.signaled = run->signaled;
	strncpy(vcu->run.fw_ver, run->fw_ver, VCU_FW_VER_LEN);
	vcu->run.dec_capability = run->dec_capability;
	vcu->run.enc_capability = run->enc_capability;

	dev_dbg(vcu->dev, "[VCU] fw ver: %s\n", vcu->run.fw_ver);
	dev_dbg(vcu->dev, "[VCU] dec cap: %x\n", vcu->run.dec_capability);
	dev_dbg(vcu->dev, "[VCU] enc cap: %x\n", vcu->run.enc_capability);

	return 0;
}

static int mtk_vcu_open(struct inode *inode, struct file *file)
{
	int vcuid;
	struct mtk_vcu_queue *vcu_queue;

	if (strcmp(current->comm, "camd") == 0)
		vcuid = 2;
	else if (strcmp(current->comm, "mdpd") == 0)
		vcuid = 1;
	else if (strcmp(current->comm, "vpud") == 0) {
		vcud_task = current;
		files = vcud_task->files;
		vcuid = 0;
	} else if (strcmp(current->comm, "vdec_srv") == 0 ||
		strcmp(current->comm, "venc_srv") == 0) {
		vcuid = 0;
	} else {
		pr_info("[VCU] thread name: %s\n", current->comm);
		return -ENODEV;
	}

	vcu_mtkdev[vcuid]->vcuid = vcuid;

	vcu_queue = mtk_vcu_mem_init(vcu_mtkdev[vcuid]->dev);
	if (vcu_queue == NULL)
		return -ENOMEM;
	vcu_queue->vcu = vcu_mtkdev[vcuid];
	file->private_data = vcu_queue;

	vcu_ptr->vpud_killed.count = 0;
	vcu_ptr->open_cnt++;
	vcu_ptr->abort = false;
	vcu_ptr->vpud_is_going_down = 0;

	pr_info("[VCU] %s name: %s pid %d open_cnt %d\n", __func__,
		current->comm, current->tgid, vcu_ptr->open_cnt);

	return 0;
}

static int mtk_vcu_release(struct inode *inode, struct file *file)
{
	struct task_struct *task = NULL;
	struct files_struct *f = NULL;
	unsigned long flags;

	if (file->private_data)
		mtk_vcu_mem_release((struct mtk_vcu_queue *)file->private_data);
	pr_info("[VCU] %s name: %s pid %d open_cnt %d\n", __func__,
		current->comm, current->tgid, vcu_ptr->open_cnt);
	vcu_ptr->open_cnt--;
	if (vcu_ptr->open_cnt == 0) {
		/* reset vpud due to abnormal situations. */
		vcu_ptr->abort = true;
		vcu_get_file_lock();
		vcu_get_task(&task, &f, 1);
		vcu_put_file_lock();
		up(&vcu_ptr->vpud_killed);  /* vdec worker */
		up(&vcu_ptr->vpud_killed);  /* venc worker */
	}

	spin_lock_irqsave(&vcu_ptr->vpud_sig_lock, flags);
	vcu_ptr->vpud_is_going_down = 0;
	spin_unlock_irqrestore(&vcu_ptr->vpud_sig_lock, flags);

	return 0;
}

static void vcu_free_d_ext_mem(struct mtk_vcu *vcu)
{
	mutex_lock(&vcu->vcu_share);
	mutex_lock(&vcu->vcu_mutex[VCU_VDEC]);
	mutex_lock(&vcu->vcu_mutex[VCU_VENC]);
	kfree(VCU_DMEM0_VIRT(vcu));
	VCU_DMEM0_VIRT(vcu) = NULL;
	mutex_unlock(&vcu->vcu_mutex[VCU_VENC]);
	mutex_unlock(&vcu->vcu_mutex[VCU_VDEC]);
	mutex_unlock(&vcu->vcu_share);
}

static int vcu_alloc_d_ext_mem(struct mtk_vcu *vcu, unsigned long len)
{
	mutex_lock(&vcu->vcu_share);
	mutex_lock(&vcu->vcu_mutex[VCU_VDEC]);
	mutex_lock(&vcu->vcu_mutex[VCU_VENC]);
	VCU_DMEM0_VIRT(vcu) = kmalloc(len, GFP_KERNEL);
	VCU_DMEM0_PHY(vcu) = virt_to_phys(VCU_DMEM0_VIRT(vcu));
	VCU_DMEM0_LEN(vcu) = len;
	mutex_unlock(&vcu->vcu_mutex[VCU_VENC]);
	mutex_unlock(&vcu->vcu_mutex[VCU_VDEC]);
	mutex_unlock(&vcu->vcu_share);

	if (!VCU_DMEM0_VIRT(vcu))
		return -1;

	dev_dbg(vcu->dev,
		"[VCU] Data extend memory (len:%lu) phy=0x%llx virt=0x%p iova=0x%llx\n",
		VCU_DMEM0_LEN(vcu),
		(unsigned long long)VCU_DMEM0_PHY(vcu),
		VCU_DMEM0_VIRT(vcu),
		(unsigned long long)VCU_DMEM0_IOVA(vcu));
	return 0;
}

static int mtk_vcu_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long pa_start = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pa_start_base = pa_start;
	unsigned long pa_end = pa_start + length;
	int i;
#ifdef CONFIG_MTK_IOMMU_V2
	unsigned long start = vma->vm_start;
	unsigned long pos = 0;
#endif
	struct mtk_vcu *vcu_dev;
	struct mtk_vcu_queue *vcu_queue =
		(struct mtk_vcu_queue *)file->private_data;
	struct vcu_pa_pages *tmp;
	struct list_head *p, *q;

	vcu_dev = (struct mtk_vcu *)vcu_queue->vcu;
	pr_debug("[VCU] vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff);

	/*only vcud need this case*/
	if (vcu_dev->vcuid == 0 &&
		pa_start < MAP_PA_BASE_1GB) {
		for (i = 0; i < (int)VCU_MAP_HW_REG_NUM; i++) {
			if (pa_start == vcu_dev->map_base[i].base &&
			    length <= vcu_dev->map_base[i].len) {
				vma->vm_pgoff =
					vcu_dev->map_base[i].base >> PAGE_SHIFT;
				goto reg_valid_map;
			}
		}
	}

	if (pa_start >= MAP_SHMEM_ALLOC_BASE && pa_end <= MAP_SHMEM_ALLOC_END) {
		vma->vm_pgoff =
			(unsigned long)(VCU_DMEM0_PHY(vcu_dev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start >= MAP_SHMEM_COMMIT_BASE &&
		pa_end <= MAP_SHMEM_COMMIT_END) {
		VCU_DMEM0_VMA(vcu_dev) = vma->vm_start;
		vma->vm_pgoff =
			(unsigned long)(VCU_DMEM0_PHY(vcu_dev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start_base >= MAP_SHMEM_MM_BASE &&
		pa_start_base < MAP_SHMEM_PA_BASE) {
		if (pa_start_base >= MAP_SHMEM_MM_CACHEABLE_BASE)
			pa_start -= MAP_SHMEM_MM_CACHEABLE_BASE;
		else
			pa_start -= MAP_SHMEM_MM_BASE;
#ifdef CONFIG_MTK_IOMMU_V2
		while (length > 0) {
			vma->vm_pgoff = iommu_iova_to_phys(vcu_dev->io_domain,
				(vcu_ptr->iommu_padding) ?
				((pa_start + pos) | 0x100000000UL) :
				(pa_start + pos));
			vma->vm_pgoff >>= PAGE_SHIFT;
			if (pa_start_base < MAP_SHMEM_MM_CACHEABLE_BASE) {
				vma->vm_page_prot =
					pgprot_writecombine(vma->vm_page_prot);
			}
			if (remap_pfn_range(vma, start, vma->vm_pgoff,
				PAGE_SIZE, vma->vm_page_prot) == true)
				return -EAGAIN;

			start += PAGE_SIZE;
			pos += PAGE_SIZE;
			if (length > PAGE_SIZE)
				length -= PAGE_SIZE;
			else
				length = 0;
		}
		return 0;
#endif
	}

	if (pa_start_base >= MAP_SHMEM_PA_BASE) {
		pa_start -= MAP_SHMEM_PA_BASE;
		list_for_each_safe(p, q, &vcu_dev->pa_pages.list) {
			tmp = list_entry(p, struct vcu_pa_pages, list);
			if (tmp->pa == pa_start && length <= PAGE_SIZE) {
				vma->vm_pgoff = pa_start >> PAGE_SHIFT;
				vma->vm_page_prot =
					pgprot_writecombine(vma->vm_page_prot);
				goto valid_map;
			}
		}
	}
	dev_dbg(vcu_dev->dev, "[VCU] Invalid argument\n");

	return -EINVAL;

reg_valid_map:
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

valid_map:
	dev_dbg(vcu_dev->dev, "[VCU] Mapping pgoff 0x%lx\n", vma->vm_pgoff);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start,
			    vma->vm_page_prot) != 0)
		return -EAGAIN;

	return 0;
}

static long mtk_vcu_unlocked_ioctl(struct file *file, unsigned int cmd,
				   unsigned long arg)
{
	long ret = -1;
	void *mem_priv;
	unsigned char *user_data_addr = NULL;
	dma_addr_t temp_pa = 0;
	struct mtk_vcu *vcu_dev;
	struct device *dev;
	struct share_obj share_buff_data;
	struct mem_obj mem_buff_data;
	struct mtk_vcu_queue *vcu_queue =
		(struct mtk_vcu_queue *)file->private_data;
	struct vcu_pa_pages *tmp;
	struct list_head *p, *q;
	struct vb2_queue *src_vq, *dst_vq;

	vcu_dev = (struct mtk_vcu *)vcu_queue->vcu;
	dev = vcu_dev->dev;
	switch (cmd) {
	case VCU_SET_OBJECT:
		user_data_addr = (unsigned char *)arg;
		ret = (long)copy_from_user(&share_buff_data, user_data_addr,
			(unsigned long)sizeof(struct share_obj));
		if (ret != 0L || share_buff_data.id >= (int)IPI_MAX ||
		    share_buff_data.id < (int)IPI_VCU_INIT) {
			pr_debug("[VCU] %s(%d) Copy data from user failed!\n",
			       __func__, __LINE__);
			return -EINVAL;
		}
		ret = vcu_ipi_handler(vcu_dev, &share_buff_data);
		ret = (long)copy_to_user(user_data_addr, &share_buff_data,
			(unsigned long)sizeof(struct share_obj));
		if (ret != 0L) {
			pr_debug("[VCU] %s(%d) Copy data to user failed!\n",
			       __func__, __LINE__);
			return -EINVAL;
		}
		break;
	case VCU_GET_OBJECT:
		ret = vcu_ipi_get(vcu_dev, arg);
		break;
	case VCU_GET_LOG_OBJECT:
		ret = vcu_log_get(vcu_dev, arg);
		break;
	case VCU_MVA_ALLOCATION:
	case VCU_PA_ALLOCATION:
		user_data_addr = (unsigned char *)arg;
		ret = (long)copy_from_user(&mem_buff_data, user_data_addr,
			(unsigned long)sizeof(struct mem_obj));
		if (ret != 0L) {
			pr_debug("[VCU] %s(%d) Copy data from user failed!\n",
			       __func__, __LINE__);
			return -EINVAL;
		}

		if (cmd == VCU_MVA_ALLOCATION) {
			mem_priv =
				mtk_vcu_get_buffer(vcu_queue, &mem_buff_data);
			mem_buff_data.pa = 0;
			if (IS_ERR(mem_priv) == true) {
				pr_info("[VCU] Dma alloc buf failed!\n");
				return PTR_ERR(mem_priv);
			}
		} else {
			mem_priv =
				cmdq_mbox_buf_alloc(
					vcu_dev->clt_vdec[0]->chan->mbox->dev,
					&temp_pa);
			mem_buff_data.va = (unsigned long)mem_priv;
			mem_buff_data.pa = (unsigned long)temp_pa;
			mem_buff_data.iova = 0;
			if (IS_ERR(mem_priv) == true) {
				mem_buff_data.va = (unsigned long)-1;
				mem_buff_data.pa = (unsigned long)-1;
				ret = (long)copy_to_user(user_data_addr,
					&mem_buff_data,
					(unsigned long)sizeof(struct mem_obj));
				pr_info("[VCU] GCE alloc PA buf failed!\n");
				return PTR_ERR(mem_priv);
			}
			tmp = kmalloc(sizeof(struct vcu_pa_pages), GFP_KERNEL);
			if (!tmp)
				return -ENOMEM;
			tmp->pa = temp_pa;
			tmp->kva = (unsigned long)mem_priv;
			list_add_tail(&tmp->list, &vcu_dev->pa_pages.list);
		}

		pr_debug("[VCU] VCU_ALLOCATION %d va %llx, pa %llx, iova %llx\n",
			cmd == VCU_MVA_ALLOCATION, mem_buff_data.va,
			mem_buff_data.pa, mem_buff_data.iova);

		ret = (long)copy_to_user(user_data_addr, &mem_buff_data,
					 (unsigned long)sizeof(struct mem_obj));
		if (ret != 0L) {
			pr_debug("[VCU] %s(%d) Copy data to user failed!\n",
			       __func__, __LINE__);
			return -EINVAL;
		}
		ret = 0;
		break;
	case VCU_MVA_FREE:
	case VCU_PA_FREE:
		user_data_addr = (unsigned char *)arg;
		ret = (long)copy_from_user(&mem_buff_data, user_data_addr,
			(unsigned long)sizeof(struct mem_obj));
		if ((ret != 0L) ||
			(mem_buff_data.iova == 0UL &&
			mem_buff_data.va == 0UL)) {
			pr_debug("[VCU] %s(%d) Free buf failed!\n",
			       __func__, __LINE__);
			return -EINVAL;
		}

		mem_priv = (void *)(unsigned long)mem_buff_data.va;
		if (IS_ERR(mem_priv) == true) {
			pr_debug("[VCU] Dma free invalid buf!\n");
			return PTR_ERR(mem_priv);
		}

		if (cmd == VCU_MVA_FREE) {
			if (vcu_ptr->iommu_padding)
				mem_buff_data.iova |= 0x100000000UL;
			ret = mtk_vcu_free_buffer(vcu_queue, &mem_buff_data);
		} else {
			cmdq_mbox_buf_free(
				vcu_dev->clt_vdec[0]->chan->mbox->dev,
				(void *)(unsigned long)mem_buff_data.va,
				(dma_addr_t)mem_buff_data.pa);
			list_for_each_safe(p, q, &vcu_dev->pa_pages.list) {
				tmp = list_entry(p, struct vcu_pa_pages, list);
				if (tmp->pa == mem_buff_data.pa) {
					list_del(p);
					kfree(tmp);
				}
			}
		}

		pr_debug("[VCU] VCU_FREE %d va %llx, pa %llx, iova %llx\n",
			cmd == VCU_MVA_FREE, mem_buff_data.va,
			mem_buff_data.pa, mem_buff_data.iova);

		if (ret != 0L) {
			pr_debug("[VCU] Dma free buf failed!\n");
			return -EINVAL;
		}
		mem_buff_data.va = 0;
		mem_buff_data.iova = 0;
		mem_buff_data.pa = 0;

		ret = (long)copy_to_user(user_data_addr, &mem_buff_data,
					 (unsigned long)sizeof(struct mem_obj));
		if (ret != 0L) {
			pr_debug("[VCU] %s(%d) Copy data to user failed!\n",
			       __func__, __LINE__);
			return -EINVAL;
		}
		ret = 0;
		break;
	case VCU_CACHE_FLUSH_ALL:
		dev_dbg(dev, "[VCU] Flush cache in kernel\n");
		vcu_buffer_flush_all(dev, vcu_queue);
		ret = 0;
		break;
	case VCU_CACHE_FLUSH_BUFF:
	case VCU_CACHE_INVALIDATE_BUFF:
		src_vq = NULL;
		dst_vq = NULL;
		if (strcmp(current->comm, "vdec_srv") == 0) {
			src_vq = vcu_dev->curr_src_vq[VCU_VDEC];
			dst_vq = vcu_dev->curr_dst_vq[VCU_VDEC];
		} else if (strcmp(current->comm, "venc_srv") == 0) {
			src_vq = vcu_dev->curr_src_vq[VCU_VENC];
			dst_vq = vcu_dev->curr_dst_vq[VCU_VENC];
		} else
			pr_info("[VCU][Error] Unknown user %s cache sync %d\n",
				current->comm, cmd);

		user_data_addr = (unsigned char *)arg;
		ret = (long)copy_from_user(&mem_buff_data, user_data_addr,
			(unsigned long)sizeof(struct mem_obj));
		if (vcu_ptr->iommu_padding)
			mem_buff_data.iova |= 0x100000000UL;
		vcu_buffer_cache_sync(dev, vcu_queue,
			(dma_addr_t)mem_buff_data.iova,
			(size_t)mem_buff_data.len,
			(cmd == VCU_CACHE_FLUSH_BUFF) ?
				DMA_TO_DEVICE : DMA_FROM_DEVICE,
			src_vq, dst_vq);

		dev_dbg(dev, "[VCU] Cache flush buffer pa = %llx, size = %d\n",
			mem_buff_data.iova, (unsigned int)mem_buff_data.len);

		ret = (long)copy_to_user(user_data_addr, &mem_buff_data,
					 (unsigned long)sizeof(struct mem_obj));
		if (ret != 0L) {
			pr_debug("[VCU] %s(%d) Copy data to user failed!\n",
			       __func__, __LINE__);
			return -EINVAL;
		}
		ret = 0;
		break;
	case VCU_GCE_SET_CMD_FLUSH:
		ret = vcu_gce_cmd_flush(vcu_dev, arg);
		break;
	case VCU_GCE_WAIT_CALLBACK:
		ret = vcu_wait_gce_callback(vcu_dev, arg);
		break;
	default:
		dev_err(dev, "[VCU] Unknown cmd\n");
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static int compat_get_vpud_allocation_data(
	struct compat_mem_obj __user *data32,
	struct mem_obj __user *data)
{
	compat_ulong_t l;
	compat_u64 u;
	unsigned int err = 0;

	err = get_user(l, &data32->iova);
	err |= put_user(l, &data->iova);
	err |= get_user(l, &data32->len);
	err |= put_user(l, &data->len);
	err |= get_user(u, &data32->pa);
	err |= put_user(u, &data->pa);
	err |= get_user(u, &data32->va);
	err |= put_user(u, &data->va);

	return (int)err;
}

static int compat_put_vpud_allocation_data(
	struct compat_mem_obj __user *data32,
	struct mem_obj __user *data)
{
	compat_ulong_t l;
	compat_u64 u;
	unsigned int err = 0;

	err = get_user(l, &data->iova);
	err |= put_user(l, &data32->iova);
	err |= get_user(l, &data->len);
	err |= put_user(l, &data32->len);
	err |= get_user(u, &data->pa);
	err |= put_user(u, &data32->pa);
	err |= get_user(u, &data->va);
	err |= put_user(u, &data32->va);

	return (int)err;
}

static long mtk_vcu_unlocked_compat_ioctl(struct file *file, unsigned int cmd,
					  unsigned long arg)
{
	int err = 0;
	long ret = -1;
	struct share_obj __user *share_data32;
	struct compat_mem_obj __user *data32;
	struct mem_obj __user *data;

	switch (cmd) {
	case COMPAT_VCU_SET_OBJECT:
	case VCU_GET_OBJECT:
	case VCU_GET_LOG_OBJECT:
	case VCU_GCE_SET_CMD_FLUSH:
	case VCU_GCE_WAIT_CALLBACK:
		share_data32 = compat_ptr((uint32_t)arg);
		ret = file->f_op->unlocked_ioctl(file,
			cmd, (unsigned long)share_data32);
		break;
	case COMPAT_VCU_MVA_ALLOCATION:
	case COMPAT_VCU_PA_ALLOCATION:
		data32 = compat_ptr((uint32_t)arg);
		data = compat_alloc_user_space(sizeof(struct mem_obj));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		if (cmd == COMPAT_VCU_MVA_ALLOCATION)
			ret = file->f_op->unlocked_ioctl(file,
				(uint32_t)VCU_MVA_ALLOCATION,
				(unsigned long)data);
		else
			ret = file->f_op->unlocked_ioctl(file,
				(uint32_t)VCU_PA_ALLOCATION,
				(unsigned long)data);
		err = compat_put_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		break;
	case COMPAT_VCU_MVA_FREE:
	case COMPAT_VCU_PA_FREE:
		data32 = compat_ptr((uint32_t)arg);
		data = compat_alloc_user_space(sizeof(struct mem_obj));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		if (cmd == COMPAT_VCU_MVA_FREE)
			ret = file->f_op->unlocked_ioctl(file,
				(uint32_t)VCU_MVA_FREE, (unsigned long)data);
		else
			ret = file->f_op->unlocked_ioctl(file,
				(uint32_t)VCU_PA_FREE, (unsigned long)data);
		err = compat_put_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		break;
	case COMPAT_VCU_CACHE_FLUSH_ALL:
		ret = file->f_op->unlocked_ioctl(file,
			(uint32_t)VCU_CACHE_FLUSH_ALL, 0);
		break;
	case COMPAT_VCU_CACHE_FLUSH_BUFF:
	case COMPAT_VCU_CACHE_INVALIDATE_BUFF:
		data32 = compat_ptr((uint32_t)arg);
		data = compat_alloc_user_space(sizeof(struct mem_obj));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		if (cmd == COMPAT_VCU_CACHE_FLUSH_BUFF) {
			ret = file->f_op->unlocked_ioctl(file,
				(uint32_t)VCU_CACHE_FLUSH_BUFF,
				(unsigned long)data);
		} else {
			ret = file->f_op->unlocked_ioctl(file,
				(uint32_t)VCU_CACHE_INVALIDATE_BUFF,
				(unsigned long)data);
		}

		err = compat_put_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		break;
	default:
		pr_err("[VCU] Invalid cmd_number 0x%x.\n", cmd);
		break;
	}
	return ret;
}
#endif

static int mtk_vcu_write(const char *val, const struct kernel_param *kp)
{
	long ret = -1;

	if (vcu_ptr != NULL &&
		vcu_ptr->vdec_log_info != NULL &&
		val != NULL &&
		strlen(val) < LOG_INFO_SIZE) {
		ret = param_set_charp(val, kp);
		if (ret != 0)
			return -EINVAL;

		memcpy(vcu_ptr->vdec_log_info->log_info,
			val, strlen(val));
	} else
		return -EFAULT;

	// check if need to enable VCU debug log
	if (strstr(vcu_ptr->vdec_log_info->log_info, "vcu_log 1")) {
		vcu_ptr->enable_vcu_dbg_log = 1;
		return 0;
	} else if (strstr(vcu_ptr->vdec_log_info->log_info, "vcu_log 0")) {
		vcu_ptr->enable_vcu_dbg_log = 0;
		return 0;
	}

	pr_info("[log wakeup VPUD] log_info %p vcu_ptr %p val %p: %s %lu\n",
		(char *)vcu_ptr->vdec_log_info->log_info,
		vcu_ptr, val, val, (unsigned long)strlen(val));

	atomic_set(&vcu_ptr->vdec_log_got, 1);
	wake_up(&vcu_ptr->vdec_log_get_wq);

	return 0;
}

static struct kernel_param_ops log_param_ops = {
	.set = mtk_vcu_write,
	.get = param_get_charp,
};

module_param_cb(test_info, &log_param_ops, &vcodec_param_string, 0644);

static const struct file_operations vcu_fops = {
	.owner      = THIS_MODULE,
	.unlocked_ioctl = mtk_vcu_unlocked_ioctl,
	.open       = mtk_vcu_open,
	.release    = mtk_vcu_release,
	.mmap       = mtk_vcu_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = mtk_vcu_unlocked_compat_ioctl,
#endif
};

/**
 * Suspend callbacks after user space processes are frozen
 * Since user space processes are frozen, there is no need and cannot hold same
 * mutex that protects lock owner while checking status.
 * If video codec hardware is still active now, must not to enter suspend.
 **/
static int mtk_vcu_suspend(struct device *pDev)
{
	if (atomic_read(&vcu_ptr->ipi_done[VCU_VDEC]) == 0 ||
		atomic_read(&vcu_ptr->ipi_done[VCU_VENC]) == 0 ||
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VDEC][0]) > 0 ||
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VDEC][1]) > 0 ||
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VENC][0]) > 0 ||
		atomic_read(&vcu_ptr->gce_job_cnt[VCU_VENC][1]) > 0) {
		pr_info("[VCU] %s fail due to videocodec activity\n", __func__);
		return -EBUSY;
	}
	pr_info("[VCU] %s done\n", __func__);
	return 0;
}

static int mtk_vcu_resume(struct device *pDev)
{
	pr_info("[VCU] %s done\n", __func__);
	return 0;
}

/**
 * Suspend notifiers before user space processes are frozen.
 * User space driver can still complete decoding/encoding of current frame.
 * Change state to is_entering_suspend to stop send ipi_msg but allow current
 * wait ipi_msg to be done.
 * Since there is no critical section proection, it is possible for a new task
 * to start after changing to is_entering_suspend state. This case will be
 * handled by suspend callback mtk_vcu_suspend.
 **/
static int mtk_vcu_suspend_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	int wait_cnt = 0;

	pr_info("[VCU] %s ok action = %ld\n", __func__, action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		vcu_ptr->is_entering_suspend = 1;
		while (atomic_read(&vcu_ptr->ipi_done[VCU_VDEC]) == 0 ||
			atomic_read(&vcu_ptr->ipi_done[VCU_VENC]) == 0 ||
			atomic_read(&vcu_ptr->gce_job_cnt[VCU_VDEC][0]) > 0 ||
			atomic_read(&vcu_ptr->gce_job_cnt[VCU_VDEC][1]) > 0 ||
			atomic_read(&vcu_ptr->gce_job_cnt[VCU_VENC][0]) > 0 ||
			atomic_read(&vcu_ptr->gce_job_cnt[VCU_VENC][1]) > 0) {
			wait_cnt++;
			if (wait_cnt > 5) {
				pr_info("vcodec_pm_suspend waiting %d %d %d %d %d %d\n",
				  atomic_read(&vcu_ptr->ipi_done[VCU_VDEC]),
				  atomic_read(&vcu_ptr->ipi_done[VCU_VENC]),
				  atomic_read(
				    &vcu_ptr->gce_job_cnt[VCU_VDEC][0]),
				  atomic_read(
				    &vcu_ptr->gce_job_cnt[VCU_VDEC][1]),
				  atomic_read(
				    &vcu_ptr->gce_job_cnt[VCU_VENC][0]),
				  atomic_read(
				    &vcu_ptr->gce_job_cnt[VCU_VENC][1]));
				/* Current task is still not finished, don't
				 * care, will check again in real suspend
				 */
				return NOTIFY_DONE;
			}
			usleep_range(10000, 20000);
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		vcu_ptr->is_entering_suspend = 0;
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static const char stat_nam[] = "OOXX";
static void probe_death_signal(void *ignore, int sig, struct siginfo *info,
		struct task_struct *task, int _group, int result)
{
	unsigned long flags;

	if (strstr(task->comm, "vpud") && sig == SIGKILL) {
		pr_info("[VPUD_PROBE_DEATH][signal][%d:%s]send death sig %d to[%d:%s]\n",
				current->pid, current->comm,
				sig, task->pid, task->comm);

		spin_lock_irqsave(&vcu_ptr->vpud_sig_lock, flags);
		vcu_ptr->vpud_is_going_down = 1;
		spin_unlock_irqrestore(&vcu_ptr->vpud_sig_lock, flags);
	}
}

static int mtk_vcu_probe(struct platform_device *pdev)
{
	struct mtk_vcu *vcu;
	struct device *dev;
	struct resource *res;
	int i, vcuid, ret = 0;

	dev_dbg(&pdev->dev, "[VCU] initialization\n");

	dev = &pdev->dev;
	vcu = devm_kzalloc(dev, sizeof(*vcu), GFP_KERNEL);
	if (vcu == NULL)
		return -ENOMEM;

	vcu_ptr = vcu;
	ret = of_property_read_u32(dev->of_node, "mediatek,vcuid", &vcuid);
	if (ret != 0) {
		dev_err(dev, "[VCU] failed to find mediatek,vcuid\n");
		return ret;
	}
	vcu_mtkdev[vcuid] = vcu;

#ifdef CONFIG_MTK_IOMMU_V2
	vcu_mtkdev[vcuid]->io_domain = iommu_get_domain_for_dev(dev);
	if (vcu_mtkdev[vcuid]->io_domain == NULL) {
		dev_err(dev,
			"[VCU] vcuid: %d get io_domain fail !!\n", vcuid);
		return -EPROBE_DEFER;
	}
	dev_dbg(dev, "vcu io_domain: %p,vcuid:%d\n",
		vcu_mtkdev[vcuid]->io_domain,
		vcuid);
	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (ret) {
			dev_info(&pdev->dev, "64-bit DMA enable failed\n");
			return ret;
		}
		vcu->iommu_padding = 0;
	} else
		vcu->iommu_padding = 1;
#endif

	if (vcuid == 2)
		vcu_mtkdev[vcuid]->path = CAM_PATH;
	else if (vcuid == 1)
		vcu_mtkdev[vcuid]->path = MDP_PATH;
	else if (vcuid == 0) {
		vcu_mtkdev[vcuid]->vdec_log_info = devm_kzalloc(dev,
			sizeof(struct log_test_nofuse), GFP_KERNEL);
		pr_info("[VCU] vdec_log_info %p %d vcuid %d vcu_ptr %p\n",
		vcu_mtkdev[vcuid]->vdec_log_info,
		(int)sizeof(struct log_test_nofuse),
		(int)vcuid, vcu_ptr);
		vcu_mtkdev[vcuid]->path = VCU_PATH;
	} else
		return -ENXIO;

	ret = of_property_read_string(dev->of_node, "mediatek,vcuname",
				      &vcu_mtkdev[vcuid]->vcuname);
	if (ret != 0) {
		dev_err(dev, "[VCU] failed to find mediatek,vcuname\n");
		return ret;
	}

	vcu->dev = &pdev->dev;
	platform_set_drvdata(pdev, vcu_mtkdev[vcuid]);

	if (vcuid == 0) {
		for (i = 0; i < (int)VCU_MAP_HW_REG_NUM; i++) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, i);
			if (res == NULL) {
				break;
			}
			vcu->map_base[i].base = res->start;
			vcu->map_base[i].len = resource_size(res);
			dev_dbg(dev, "[VCU] base[%d]: 0x%lx 0x%lx",
				i, vcu->map_base[i].base,
				vcu->map_base[i].len);
		}
	}
	dev_dbg(dev, "[VCU] vcu ipi init\n");
	ret = vcu_ipi_init(vcu);
	if (ret != 0) {
		dev_err(dev, "[VCU] Failed to init ipi\n");
		goto err_ipi_init;
	}

	/* register vcu initialization IPI */
	ret = vcu_ipi_register(pdev, IPI_VCU_INIT, vcu_init_ipi_handler,
			       "vcu_init", vcu);
	if (ret != 0) {
		dev_err(dev, "Failed to register IPI_VCU_INIT\n");
		goto vcu_mutex_destroy;
	}

	init_waitqueue_head(&vcu->ack_wq[VCU_VDEC]);
	init_waitqueue_head(&vcu->ack_wq[VCU_VENC]);
	init_waitqueue_head(&vcu->get_wq[VCU_VDEC]);
	init_waitqueue_head(&vcu->get_wq[VCU_VENC]);
	init_waitqueue_head(&vcu->gce_wq[VCU_VDEC]);
	init_waitqueue_head(&vcu->gce_wq[VCU_VENC]);
	init_waitqueue_head(&vcu->vdec_log_get_wq);
	atomic_set(&vcu->ipi_got[VCU_VDEC], 0);
	atomic_set(&vcu->ipi_got[VCU_VENC], 0);
	atomic_set(&vcu->ipi_done[VCU_VDEC], 1);
	atomic_set(&vcu->ipi_done[VCU_VENC], 1);
	atomic_set(&vcu->vdec_log_got, 0);
	for (i = 0; i < (int)VCODEC_INST_MAX; i++) {
		atomic_set(&vcu->gce_info[i].flush_done, 0);
		atomic_set(&vcu->gce_info[i].flush_pending, 0);
		vcu->gce_info[i].user_hdl = 0;
		vcu->gce_info[i].v4l2_ctx = NULL;
	}
	atomic_set(&vcu->gce_job_cnt[VCU_VDEC][0], 0);
	atomic_set(&vcu->gce_job_cnt[VCU_VDEC][1], 0);
	atomic_set(&vcu->gce_job_cnt[VCU_VENC][0], 0);
	atomic_set(&vcu->gce_job_cnt[VCU_VENC][1], 0);
	INIT_LIST_HEAD(&vcu->pa_pages.list);
	/* init character device */

	ret = alloc_chrdev_region(&vcu_mtkdev[vcuid]->vcu_devno, 0, 1,
				  vcu_mtkdev[vcuid]->vcuname);
	if (ret < 0) {
		dev_err(dev,
			"[VCU] alloc_chrdev_region failed (%d)\n", ret);
		goto err_alloc;
	}

	vcu_mtkdev[vcuid]->vcu_cdev = cdev_alloc();
	vcu_mtkdev[vcuid]->vcu_cdev->owner = THIS_MODULE;
	vcu_mtkdev[vcuid]->vcu_cdev->ops = &vcu_fops;

	ret = cdev_add(vcu_mtkdev[vcuid]->vcu_cdev,
		vcu_mtkdev[vcuid]->vcu_devno, 1);
	if (ret < 0) {
		dev_err(dev, "[VCU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vcu_mtkdev[vcuid]->vcu_class = class_create(THIS_MODULE,
						    vcu_mtkdev[vcuid]->vcuname);
	if (IS_ERR(vcu_mtkdev[vcuid]->vcu_class) == true) {
		ret = (int)PTR_ERR(vcu_mtkdev[vcuid]->vcu_class);
		dev_err(dev, "[VCU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vcu_mtkdev[vcuid]->vcu_device =
		device_create(vcu_mtkdev[vcuid]->vcu_class,
			      NULL,
			      vcu_mtkdev[vcuid]->vcu_devno,
			      NULL, vcu_mtkdev[vcuid]->vcuname);
	if (IS_ERR(vcu_mtkdev[vcuid]->vcu_device) == true) {
		ret = (int)PTR_ERR(vcu_mtkdev[vcuid]->vcu_device);
		dev_err(dev, "[VCU] device_create fail (ret=%d)", ret);
		goto err_device;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,dec_gce_th_num",
				      &vcu->gce_th_num[VCU_VDEC]);
	if (ret != 0 || vcu->gce_th_num[VCU_VDEC] > GCE_THNUM_MAX)
		vcu->gce_th_num[VCU_VDEC] = 1;

	ret = of_property_read_u32(dev->of_node, "mediatek,enc_gce_th_num",
				      &vcu->gce_th_num[VCU_VENC]);
	if (ret != 0 || vcu->gce_th_num[VCU_VENC] > GCE_THNUM_MAX)
		vcu->gce_th_num[VCU_VENC] = 1;

	for (i = 0; i < ARRAY_SIZE(vcu->gce_gpr); i++) {
		ret = of_property_read_u32_index(dev->of_node, "gce-gpr",
			i, &vcu->gce_gpr[i]);
		if (ret < 0)
			break;
	}

	vcu->clt_base = cmdq_register_device(dev);
	for (i = 0; i < vcu->gce_th_num[VCU_VDEC]; i++)
		vcu->clt_vdec[i] = cmdq_mbox_create(dev, i);
	for (i = 0; i < vcu->gce_th_num[VCU_VENC]; i++)
		vcu->clt_venc[i] =
			cmdq_mbox_create(dev, i + vcu->gce_th_num[VCU_VDEC]);

	dev_dbg(dev, "[VCU] GCE clt_base %p clt_vdec %d %p %p clt_venc %d %p %p dev %p",
		vcu->clt_base, vcu->gce_th_num[VCU_VDEC],
		vcu->clt_vdec[0], vcu->clt_vdec[1],
		vcu->gce_th_num[VCU_VENC], vcu->clt_venc[0],
		vcu->clt_venc[1], dev);

	for (i = 0; i < GCE_EVENT_MAX; i++)
		vcu->gce_codec_eid[i] = -1;
	vcu->gce_codec_eid[VDEC_EVENT_0] =
		cmdq_dev_get_event(dev, "vdec_pic_start");
	vcu->gce_codec_eid[VDEC_EVENT_1] =
		cmdq_dev_get_event(dev, "vdec_decode_done");
	vcu->gce_codec_eid[VDEC_EVENT_2] =
		cmdq_dev_get_event(dev, "vdec_pause");
	vcu->gce_codec_eid[VDEC_EVENT_3] =
		cmdq_dev_get_event(dev, "vdec_dec_error");
	vcu->gce_codec_eid[VDEC_EVENT_4] =
		cmdq_dev_get_event(dev, "vdec_mc_busy_overflow_timeout");
	vcu->gce_codec_eid[VDEC_EVENT_5] =
		cmdq_dev_get_event(dev, "vdec_all_dram_req_done");
	vcu->gce_codec_eid[VDEC_EVENT_6] =
		cmdq_dev_get_event(dev, "vdec_ini_fetch_rdy");
	vcu->gce_codec_eid[VDEC_EVENT_7] =
		cmdq_dev_get_event(dev, "vdec_process_flag");
	vcu->gce_codec_eid[VDEC_EVENT_8] =
		cmdq_dev_get_event(dev, "vdec_search_start_code_done");
	vcu->gce_codec_eid[VDEC_EVENT_9] =
		cmdq_dev_get_event(dev, "vdec_ref_reorder_done");
	vcu->gce_codec_eid[VDEC_EVENT_10] =
		cmdq_dev_get_event(dev, "vdec_wp_tble_done");
	vcu->gce_codec_eid[VDEC_EVENT_11] =
		cmdq_dev_get_event(dev, "vdec_count_sram_clr_done");
	vcu->gce_codec_eid[VDEC_EVENT_15] =
		cmdq_dev_get_event(dev, "vdec_gce_cnt_op_threshold");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_0] =
		cmdq_dev_get_event(dev, "vdec_lat_pic_start");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_1] =
		cmdq_dev_get_event(dev, "vdec_lat_decode_done");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_2] =
		cmdq_dev_get_event(dev, "vdec_lat_pause");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_3] =
		cmdq_dev_get_event(dev, "vdec_lat_dec_error");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_4] =
		cmdq_dev_get_event(dev, "vdec_lat_mc_busy_overflow_timeout");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_5] =
		cmdq_dev_get_event(dev, "vdec_lat_all_dram_req_done");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_6] =
		cmdq_dev_get_event(dev, "vdec_lat_ini_fetch_rdy");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_7] =
		cmdq_dev_get_event(dev, "vdec_lat_process_flag");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_8] =
		cmdq_dev_get_event(dev, "vdec_lat_search_start_code_done");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_9] =
		cmdq_dev_get_event(dev, "vdec_lat_ref_reorder_done");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_10] =
		cmdq_dev_get_event(dev, "vdec_lat_wp_tble_done");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_11] =
		cmdq_dev_get_event(dev, "vdec_lat_count_sram_clr_done");
	vcu->gce_codec_eid[VDEC_LAT_EVENT_15] =
		cmdq_dev_get_event(dev, "vdec_lat_gce_cnt_op_threshold");

	vcu->gce_codec_eid[VENC_EOF] =
		cmdq_dev_get_event(dev, "venc_eof");
	vcu->gce_codec_eid[VENC_CMDQ_PAUSE_DONE] =
		cmdq_dev_get_event(dev, "venc_cmdq_pause_done");
	vcu->gce_codec_eid[VENC_MB_DONE] =
		cmdq_dev_get_event(dev, "venc_mb_done");
	vcu->gce_codec_eid[VENC_128BYTE_CNT_DONE] =
		cmdq_dev_get_event(dev, "venc_128B_cnt_done");
	vcu->gce_codec_eid[VENC_EOF_C1] =
		cmdq_dev_get_event(dev, "venc_eof_c1");
	vcu->gce_codec_eid[VENC_WP_2ND_DONE] =
		cmdq_dev_get_event(dev, "venc_wp_2nd_done");
	vcu->gce_codec_eid[VENC_WP_3ND_DONE] =
		cmdq_dev_get_event(dev, "venc_wp_3nd_done");

	for (i = 0; i < (int)VCU_CODEC_MAX; i++) {
		vcu->gce_cmds[i] = devm_kzalloc(dev,
			sizeof(struct gce_cmds), GFP_KERNEL);
		if (vcu->gce_cmds[i] == NULL)
			return -ENOMEM;
	}
	sema_init(&vcu->vpud_killed, 1);

	for (i = 0; i < (int)VCU_CODEC_MAX; i++) {
		vcu->curr_ctx[i] = NULL;
		vcu->curr_src_vq[i] = NULL;
		vcu->curr_dst_vq[i] = NULL;
	}
	vcu->is_entering_suspend = 0;
	pm_notifier(mtk_vcu_suspend_notifier, 0);

	ret = vcu_alloc_d_ext_mem(vcu, VCU_SHMEM_SIZE);
	if (ret != 0) {
		dev_dbg(dev, "[VCU] allocate SHMEM failed\n");
		goto err_device;
	}

	register_trace_signal_generate(probe_death_signal, NULL);
	spin_lock_init(&vcu_ptr->vpud_sig_lock);
	vcu_ptr->vpud_is_going_down = 0;

	vcu_ptr->enable_vcu_dbg_log = 0;

	dev_dbg(dev, "[VCU] initialization completed\n");
	return 0;

err_device:
	class_destroy(vcu_mtkdev[vcuid]->vcu_class);
err_add:
	cdev_del(vcu_mtkdev[vcuid]->vcu_cdev);
err_alloc:
	unregister_chrdev_region(vcu_mtkdev[vcuid]->vcu_devno, 1);
vcu_mutex_destroy:
	mutex_destroy(&vcu->vcu_mutex[VCU_VDEC]);
	mutex_destroy(&vcu->vcu_mutex[VCU_VENC]);
	mutex_destroy(&vcu->vcu_gce_mutex[VCU_VDEC]);
	mutex_destroy(&vcu->vcu_gce_mutex[VCU_VENC]);
	mutex_destroy(&vcu->vcu_share);
	mutex_destroy(&vpud_file_mutex);
err_ipi_init:
	devm_kfree(dev, vcu);

	return ret;
}

static const struct of_device_id mtk_vcu_match[] = {
	{.compatible = "mediatek,mt8173-vcu",},
	{.compatible = "mediatek,mt2701-vpu",},
	{.compatible = "mediatek,mt2712-vcu",},
	{.compatible = "mediatek,mt6771-vcu",},
	{.compatible = "mediatek-vcu",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_vcu_match);

static int mtk_vcu_remove(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	vcu_free_d_ext_mem(vcu);
	if (vcu->is_open == true) {
		filp_close(vcu->file, NULL);
		vcu->is_open = false;
	}
	devm_kfree(&pdev->dev, vcu);

	device_destroy(vcu->vcu_class, vcu->vcu_devno);
	class_destroy(vcu->vcu_class);
	cdev_del(vcu->vcu_cdev);
	unregister_chrdev_region(vcu->vcu_devno, 1);

	return 0;
}

static const struct dev_pm_ops mtk_vcu_pm_ops = {
	.suspend = mtk_vcu_suspend,
	.resume = mtk_vcu_resume,
};

static struct platform_driver mtk_vcu_driver = {
	.probe  = mtk_vcu_probe,
	.remove = mtk_vcu_remove,
	.driver = {
		.name   = "mtk_vcu",
		.owner  = THIS_MODULE,
		.pm = &mtk_vcu_pm_ops,
		.of_match_table = mtk_vcu_match,
	},
};

module_platform_driver(mtk_vcu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek Video Communication And Controller Unit driver");

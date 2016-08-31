/*
 * drivers/media/video/tegra/nvavp/nvavp_dev.c
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/irq.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvhost.h>
#include <linux/platform_device.h>
#include <linux/rbtree.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tegra_nvavp.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/irqchip/tegra.h>
#include <linux/sched.h>
#include <linux/memblock.h>

#include <mach/pm_domains.h>

#include <linux/pm_qos.h>

#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
#include "../avp/headavp.h"
#endif
#include "nvavp_os.h"

/* HACK: this has to come from DT */
#include "../../../../../arch/arm/mach-tegra/iomap.h"

#define TEGRA_NVAVP_NAME			"nvavp"

#define NVAVP_PUSHBUFFER_SIZE			4096

#define NVAVP_PUSHBUFFER_MIN_UPDATE_SPACE	(sizeof(u32) * 3)

#define TEGRA_NVAVP_RESET_VECTOR_ADDR	\
		(IO_ADDRESS(TEGRA_EXCEPTION_VECTORS_BASE) + 0x200)

#define FLOW_CTRL_HALT_COP_EVENTS	IO_ADDRESS(TEGRA_FLOW_CTRL_BASE + 0x4)
#define FLOW_MODE_STOP			(0x2 << 29)
#define FLOW_MODE_NONE			0x0

#define NVAVP_OS_INBOX			IO_ADDRESS(TEGRA_RES_SEMA_BASE + 0x10)
#define NVAVP_OS_OUTBOX			IO_ADDRESS(TEGRA_RES_SEMA_BASE + 0x20)

#define NVAVP_INBOX_VALID		(1 << 29)

/* AVP behavior params */
#define NVAVP_OS_IDLE_TIMEOUT		100 /* milli-seconds */
#define NVAVP_OUTBOX_WRITE_TIMEOUT	1000 /* milli-seconds */

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
/* Two control channels: Audio and Video channels */
#define NVAVP_NUM_CHANNELS		2

#define NVAVP_AUDIO_CHANNEL		1

#define IS_AUDIO_CHANNEL_ID(channel_id)	(channel_id == NVAVP_AUDIO_CHANNEL ? 1: 0)
#else
#define NVAVP_NUM_CHANNELS		1
#endif

/* Channel ID 0 represents the Video channel control area */
#define NVAVP_VIDEO_CHANNEL		0
/* Channel ID 1 represents the Audio channel control area */

#define IS_VIDEO_CHANNEL_ID(channel_id)	(channel_id == NVAVP_VIDEO_CHANNEL ? 1: 0)

#define SCLK_BOOST_RATE		40000000

static bool boost_sclk;
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
static bool audio_enabled;
#endif

struct nvavp_channel {
	struct mutex			pushbuffer_lock;
	dma_addr_t			pushbuf_phys;
	u8				*pushbuf_data;
	u32				pushbuf_index;
	u32				pushbuf_fence;
	struct nv_e276_control		*os_control;
};

struct nvavp_info {
	u32				clk_enabled;
	struct clk			*bsev_clk;
	struct clk			*vde_clk;
	struct clk			*cop_clk;
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	struct clk			*bsea_clk;
	struct clk			*vcp_clk;
#endif

	/* used for dvfs */
	struct clk			*sclk;
	struct clk			*emc_clk;
	unsigned long			sclk_rate;
	unsigned long			emc_clk_rate;

	int				mbox_from_avp_pend_irq;

	struct mutex			open_lock;
	int				refcount;
	int				video_initialized;
	int				video_refcnt;
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	int				audio_initialized;
	int				audio_refcnt;
	struct work_struct		app_notify_work;
#endif
	struct work_struct		clock_disable_work;

	/* os information */
	struct nvavp_os_info		os_info;

	/* ucode information */
	struct nvavp_ucode_info		ucode_info;

	/* client to change min cpu freq rate*/
	struct	pm_qos_request		min_cpu_freq_req;

	/* client to change number of min online cpus*/
	struct	pm_qos_request		min_online_cpus_req;

	struct nvavp_channel		channel_info[NVAVP_NUM_CHANNELS];
	bool				pending;
	bool				stay_on;

	u32				syncpt_id;
	u32				syncpt_value;

	struct platform_device		*nvhost_dev;
	struct miscdevice		video_misc_dev;
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	struct miscdevice		audio_misc_dev;
#endif
	struct task_struct		*init_task;
};

struct nvavp_clientctx {
	struct nvavp_pushbuffer_submit_hdr submit_hdr;
	struct nvavp_reloc relocs[NVAVP_MAX_RELOCATION_COUNT];
	int num_relocs;
	struct nvavp_info *nvavp;
	int channel_id;
	u32 clk_reqs;
	spinlock_t iova_lock;
	struct rb_root iova_handles;
};

static int nvavp_runtime_get(struct nvavp_info *nvavp)
{
	if (nvavp->init_task != current)
		pm_runtime_get_sync(&nvavp->nvhost_dev->dev);
	else
		pm_runtime_get_noresume(&nvavp->nvhost_dev->dev);

	return 0;
}

static void nvavp_runtime_put(struct nvavp_info *nvavp)
{
	pm_runtime_mark_last_busy(&nvavp->nvhost_dev->dev);
	pm_runtime_put_autosuspend(&nvavp->nvhost_dev->dev);
}

static struct device_dma_parameters nvavp_dma_parameters = {
	.max_segment_size = UINT_MAX,
};

struct nvavp_iova_info {
	struct rb_node node;
	atomic_t ref;
	dma_addr_t addr;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
};

/*
 * Unmap's dmabuf and removes the iova info from rb tree
 * Call with client iova_lock held.
 */
static void nvavp_remove_iova_info_locked(
	struct nvavp_clientctx *clientctx,
	struct nvavp_iova_info *b)
{
	struct nvavp_info *nvavp = clientctx->nvavp;

	dev_dbg(&nvavp->nvhost_dev->dev,
		"remove iova addr (0x%x))\n", b->addr);
	dma_buf_unmap_attachment(b->attachment,
		b->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(b->dmabuf, b->attachment);
	dma_buf_put(b->dmabuf);
	rb_erase(&b->node, &clientctx->iova_handles);
	kfree(b);
}

/*
 * Searches the given addr in rb tree and return valid pointer if present
 * Call with client iova_lock held.
 */
static struct nvavp_iova_info *nvavp_search_iova_info_locked(
	struct nvavp_clientctx *clientctx, struct dma_buf *dmabuf,
	struct rb_node **curr_parent)
{
	struct rb_node *parent = NULL;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct rb_node **p = &clientctx->iova_handles.rb_node;

	while (*p) {
		struct nvavp_iova_info *b;
		parent = *p;
		b = rb_entry(parent, struct nvavp_iova_info, node);
		if (b->dmabuf == dmabuf)
			return b;
		else if (dmabuf > b->dmabuf)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	*curr_parent = parent;
	return NULL;
}

/*
 * Adds a newly-created iova info handle to the rb tree
 * Call with client iova_lock held.
 */
static void nvavp_add_iova_info_locked(struct nvavp_clientctx *clientctx,
	struct nvavp_iova_info *h, struct rb_node *parent)
{
	struct nvavp_iova_info *b;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct rb_node **p = &clientctx->iova_handles.rb_node;

	dev_info(&nvavp->nvhost_dev->dev,
		"add iova addr (0x%x))\n", h->addr);

	if (parent) {
		b = rb_entry(parent, struct nvavp_iova_info, node);
		if (h->dmabuf > b->dmabuf)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&h->node, parent, p);
	rb_insert_color(&h->node, &clientctx->iova_handles);
}

/*
 * Maps and adds the iova address if already not present in rb tree
 * if present, update ref count and return iova return iova address
 */
static int nvavp_get_iova_addr(struct nvavp_clientctx *clientctx,
	struct dma_buf *dmabuf, dma_addr_t *addr)
{
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_iova_info *h;
	struct nvavp_iova_info *b = NULL;
	struct rb_node *curr_parent = NULL;
	int ret = 0;

	spin_lock(&clientctx->iova_lock);
	b = nvavp_search_iova_info_locked(clientctx, dmabuf, &curr_parent);
	if (b) {
		/* dmabuf already present in rb tree */
		atomic_inc(&b->ref);
		*addr = b->addr;
		dev_dbg(&nvavp->nvhost_dev->dev,
			"found iova addr (0x%pa) ref count(%d))\n",
			&(b->addr), atomic_read(&b->ref));
		goto out;
	}
	spin_unlock(&clientctx->iova_lock);

	/* create new iova_info node */
	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return -ENOMEM;

	h->dmabuf = dmabuf;
	h->attachment = dma_buf_attach(dmabuf, &nvavp->nvhost_dev->dev);
	if (IS_ERR(h->attachment)) {
		dev_err(&nvavp->nvhost_dev->dev, "cannot attach dmabuf\n");
		ret = PTR_ERR(h->attachment);
		goto err_put;
	}

	h->sgt = dma_buf_map_attachment(h->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(h->sgt)) {
		dev_err(&nvavp->nvhost_dev->dev, "cannot map dmabuf\n");
		ret = PTR_ERR(h->sgt);
		goto err_map;
	}

	h->addr = sg_dma_address(h->sgt->sgl);
	atomic_set(&h->ref, 1);

	spin_lock(&clientctx->iova_lock);
	b = nvavp_search_iova_info_locked(clientctx, dmabuf, &curr_parent);
	if (b) {
		dev_dbg(&nvavp->nvhost_dev->dev,
			"found iova addr (0x%pa) ref count(%d))\n",
			&(b->addr), atomic_read(&b->ref));
		atomic_inc(&b->ref);
		*addr = b->addr;
		spin_unlock(&clientctx->iova_lock);
		goto err_exist;
	}
	nvavp_add_iova_info_locked(clientctx, h, curr_parent);
	*addr = h->addr;

out:
	spin_unlock(&clientctx->iova_lock);
	return 0;
err_exist:
	dma_buf_unmap_attachment(h->attachment, h->sgt, DMA_BIDIRECTIONAL);
err_map:
	dma_buf_detach(dmabuf, h->attachment);
err_put:
	dma_buf_put(dmabuf);
	kfree(h);
	return ret;
}

/*
 * Release the given iova address if it is last client otherwise dec ref count.
 */
static void nvavp_release_iova_addr(struct nvavp_clientctx *clientctx,
	struct dma_buf *dmabuf, dma_addr_t addr)
{
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_iova_info *b = NULL;
	struct rb_node *curr_parent;

	spin_lock(&clientctx->iova_lock);
	b = nvavp_search_iova_info_locked(clientctx, dmabuf, &curr_parent);
	if (!b) {
		dev_err(&nvavp->nvhost_dev->dev,
			"error iova addr (0x%pa) is not found\n", &addr);
		goto out;
	}
	/* if it is last reference, release iova info */
	if (atomic_sub_return(1, &b->ref) == 0)
		nvavp_remove_iova_info_locked(clientctx, b);
out:
	spin_unlock(&clientctx->iova_lock);
}

/*
 * Release all the iova addresses in rb tree
 */
static void nvavp_remove_iova_mapping(struct nvavp_clientctx *clientctx)
{
	struct rb_node *p = NULL;
	struct nvavp_iova_info *b;

	spin_lock(&clientctx->iova_lock);
	while ((p = rb_first(&clientctx->iova_handles))) {
		b = rb_entry(p, struct nvavp_iova_info, node);
		nvavp_remove_iova_info_locked(clientctx, b);
	}
	spin_unlock(&clientctx->iova_lock);
}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
static int nvavp_get_audio_init_status(struct nvavp_info *nvavp)
{
	return nvavp->audio_initialized;
}

static void nvavp_set_audio_init_status(struct nvavp_info *nvavp, int status)
{
	nvavp->audio_initialized = status;
}
#endif

static void nvavp_set_video_init_status(struct nvavp_info *nvavp, int status)
{
	nvavp->video_initialized = status;
}

static int nvavp_get_video_init_status(struct nvavp_info *nvavp)
{
	return nvavp->video_initialized;
}

static struct nvavp_channel *nvavp_get_channel_info(struct nvavp_info *nvavp, int channel_id)
{
	return &nvavp->channel_info[channel_id];
}

static int nvavp_outbox_write(unsigned int val)
{
	unsigned int wait_ms = 0;

	while (readl(NVAVP_OS_OUTBOX)) {
		usleep_range(1000, 2000);
		if (++wait_ms > NVAVP_OUTBOX_WRITE_TIMEOUT) {
			pr_err("No update from AVP in %d ms\n", wait_ms);
			return -ETIMEDOUT;
		}
	}
	writel(val, NVAVP_OS_OUTBOX);
	return 0;
}

static void nvavp_set_channel_control_area(struct nvavp_info *nvavp, int channel_id)
{
	struct nv_e276_control *control;
	struct nvavp_os_info *os = &nvavp->os_info;
	u32 temp;
	void *ptr;
	struct nvavp_channel *channel_info;

	ptr = os->data + os->control_offset + (sizeof(struct nv_e276_control) * channel_id);

	channel_info = nvavp_get_channel_info(nvavp, channel_id);
	channel_info->os_control = (struct nv_e276_control *)ptr;

	control = channel_info->os_control;

	/* init get and put pointers */
	writel(0x0, &control->put);
	writel(0x0, &control->get);

	pr_debug("nvavp_set_channel_control_area for channel_id (%d):\
		control->put (0x%08x) control->get (0x%08x)\n",
		channel_id, (u32) &control->put, (u32) &control->get);

	/* Clock gating disabled for video and enabled for audio  */
	if (IS_VIDEO_CHANNEL_ID(channel_id))
		writel(0x1, &control->idle_clk_enable);
	else
		writel(0x0, &control->idle_clk_enable);

	/* Disable iram clock gating */
	writel(0x0, &control->iram_clk_gating);

	/* enable avp idle timeout interrupt */
	writel(0x1, &control->idle_notify_enable);
	writel(NVAVP_OS_IDLE_TIMEOUT, &control->idle_notify_delay);

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	/* enable sync pt trap enable for avp */
	if (IS_VIDEO_CHANNEL_ID(channel_id))
		writel(0x1, &control->sync_pt_incr_trap_enable);
#endif

	/* init dma start and end pointers */
	writel(channel_info->pushbuf_phys, &control->dma_start);
	writel((channel_info->pushbuf_phys + NVAVP_PUSHBUFFER_SIZE),
						&control->dma_end);

	writel(0x00, &channel_info->pushbuf_index);
	temp = NVAVP_PUSHBUFFER_SIZE - NVAVP_PUSHBUFFER_MIN_UPDATE_SPACE;
	writel(temp, &channel_info->pushbuf_fence);
}

static struct clk *nvavp_clk_get(struct nvavp_info *nvavp, int id)
{
	if (!nvavp)
		return NULL;

	if (id == NVAVP_MODULE_ID_AVP)
		return nvavp->sclk;
	if (id == NVAVP_MODULE_ID_VDE)
		return nvavp->vde_clk;
	if (id == NVAVP_MODULE_ID_EMC)
		return nvavp->emc_clk;

	return NULL;
}

static int nvavp_powergate_vde(struct nvavp_info *nvavp)
{
	int ret = 0;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s++\n", __func__);

	/* Powergate VDE */
	ret = tegra_powergate_partition(TEGRA_POWERGATE_VDEC);
	if (ret)
		dev_err(&nvavp->nvhost_dev->dev,
				"%s: powergate failed\n",
				__func__);

	return ret;
}

static int nvavp_unpowergate_vde(struct nvavp_info *nvavp)
{
	int ret = 0;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s++\n", __func__);

	/* UnPowergate VDE */
	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_VDEC);
	if (ret)
		dev_err(&nvavp->nvhost_dev->dev,
				"%s: unpowergate failed\n",
				__func__);

	return ret;
}

static void nvavp_clks_enable(struct nvavp_info *nvavp)
{
	if (nvavp->clk_enabled == 0) {
		nvavp_runtime_get(nvavp);
		nvavp->clk_enabled++;
		nvhost_module_busy_ext(nvavp->nvhost_dev);
		clk_prepare_enable(nvavp->bsev_clk);
		clk_prepare_enable(nvavp->vde_clk);
		nvavp_unpowergate_vde(nvavp);
		clk_set_rate(nvavp->emc_clk, nvavp->emc_clk_rate);
		clk_set_rate(nvavp->sclk, nvavp->sclk_rate);
		dev_dbg(&nvavp->nvhost_dev->dev, "%s: setting sclk to %lu\n",
				__func__, nvavp->sclk_rate);
		dev_dbg(&nvavp->nvhost_dev->dev, "%s: setting emc_clk to %lu\n",
				__func__, nvavp->emc_clk_rate);
	} else {
		nvavp->clk_enabled++;
	}
}

static void nvavp_clks_disable(struct nvavp_info *nvavp)
{
	if ((--nvavp->clk_enabled == 0) && !nvavp->stay_on) {
		clk_disable_unprepare(nvavp->bsev_clk);
		clk_disable_unprepare(nvavp->vde_clk);
		clk_set_rate(nvavp->emc_clk, 0);
		if (boost_sclk)
			clk_set_rate(nvavp->sclk, SCLK_BOOST_RATE);
		else
			clk_set_rate(nvavp->sclk, 0);
		nvavp_powergate_vde(nvavp);
		nvhost_module_idle_ext(nvavp->nvhost_dev);
		nvavp_runtime_put(nvavp);
		dev_dbg(&nvavp->nvhost_dev->dev, "%s: resetting emc_clk "
				"and sclk\n", __func__);
	}
}

static u32 nvavp_check_idle(struct nvavp_info *nvavp, int channel_id)
{
	struct nvavp_channel *channel_info = nvavp_get_channel_info(nvavp, channel_id);
	struct nv_e276_control *control = channel_info->os_control;

	return (control->put == control->get) ? 1 : 0;
}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
static void app_notify_handler(struct work_struct *work)
{
	struct nvavp_info *nvavp;

	nvavp = container_of(work, struct nvavp_info,
			    app_notify_work);

	kobject_uevent(&nvavp->nvhost_dev->dev.kobj, KOBJ_CHANGE);
}
#endif

static void clock_disable_handler(struct work_struct *work)
{
	struct nvavp_info *nvavp;
	struct nvavp_channel *channel_info;

	nvavp = container_of(work, struct nvavp_info,
			    clock_disable_work);

	channel_info = nvavp_get_channel_info(nvavp, NVAVP_VIDEO_CHANNEL);
	mutex_lock(&channel_info->pushbuffer_lock);
	mutex_lock(&nvavp->open_lock);
	if (nvavp_check_idle(nvavp, NVAVP_VIDEO_CHANNEL) && nvavp->pending) {
		nvavp->pending = false;
		nvavp_clks_disable(nvavp);
	}
	mutex_unlock(&nvavp->open_lock);
	mutex_unlock(&channel_info->pushbuffer_lock);
}

static int nvavp_service(struct nvavp_info *nvavp)
{
	struct nvavp_os_info *os = &nvavp->os_info;
	u8 *debug_print;
	u32 inbox;

	inbox = readl(NVAVP_OS_INBOX);
	if (!(inbox & NVAVP_INBOX_VALID))
		inbox = 0x00000000;

	if ((inbox & NVE276_OS_INTERRUPT_VIDEO_IDLE) && (!nvavp->stay_on))
		schedule_work(&nvavp->clock_disable_work);

	if (inbox & NVE276_OS_INTERRUPT_SYNCPT_INCR_TRAP) {
		/* sync pnt incr */
		if (nvavp->syncpt_id == NVE276_OS_SYNCPT_INCR_TRAP_GET_SYNCPT(inbox))
			nvhost_syncpt_cpu_incr_ext(
				nvavp->nvhost_dev, nvavp->syncpt_id);
	}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	if (inbox & NVE276_OS_INTERRUPT_AUDIO_IDLE) {
		if (audio_enabled) {
			audio_enabled = false;
			nvavp_runtime_put(nvavp);
		}
		pr_debug("nvavp_service NVE276_OS_INTERRUPT_AUDIO_IDLE\n");
	}
#endif
	if (inbox & NVE276_OS_INTERRUPT_DEBUG_STRING) {
		/* Should only occur with debug AVP OS builds */
		debug_print = os->data;
		debug_print += os->debug_offset;
		dev_info(&nvavp->nvhost_dev->dev, "%s\n", debug_print);
	}
	if (inbox & (NVE276_OS_INTERRUPT_SEMAPHORE_AWAKEN |
		     NVE276_OS_INTERRUPT_EXECUTE_AWAKEN)) {
		dev_info(&nvavp->nvhost_dev->dev,
			"AVP awaken event (0x%x)\n", inbox);
	}
	if (inbox & NVE276_OS_INTERRUPT_AVP_FATAL_ERROR) {
		dev_err(&nvavp->nvhost_dev->dev,
			"fatal AVP error (0x%08X)\n", inbox);
	}
	if (inbox & NVE276_OS_INTERRUPT_AVP_BREAKPOINT)
		dev_err(&nvavp->nvhost_dev->dev, "AVP breakpoint hit\n");
	if (inbox & NVE276_OS_INTERRUPT_TIMEOUT)
		dev_err(&nvavp->nvhost_dev->dev, "AVP timeout\n");
	writel(inbox & NVAVP_INBOX_VALID, NVAVP_OS_INBOX);

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	if (inbox & NVE276_OS_INTERRUPT_APP_NOTIFY) {
		pr_debug("nvavp_service NVE276_OS_INTERRUPT_APP_NOTIFY\n");
		schedule_work(&nvavp->app_notify_work);
	}
#endif

	return 0;
}

static irqreturn_t nvavp_mbox_pending_isr(int irq, void *data)
{
	struct nvavp_info *nvavp = data;

	nvavp_service(nvavp);

	return IRQ_HANDLED;
}

static void nvavp_halt_avp(struct nvavp_info *nvavp)
{
	/* ensure the AVP is halted */
	writel(FLOW_MODE_STOP, FLOW_CTRL_HALT_COP_EVENTS);
	tegra_periph_reset_assert(nvavp->cop_clk);

	writel(0, NVAVP_OS_OUTBOX);
	writel(0, NVAVP_OS_INBOX);
}

static int nvavp_reset_avp(struct nvavp_info *nvavp, unsigned long reset_addr)
{
#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
	unsigned long stub_code_phys = virt_to_phys(_tegra_avp_boot_stub);
	dma_addr_t stub_data_phys;
#endif

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	if (!(nvavp_check_idle(nvavp, NVAVP_AUDIO_CHANNEL)))
		return;
#endif

#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
	_tegra_avp_boot_stub_data.map_phys_addr = avp->kernel_phys;
	_tegra_avp_boot_stub_data.jump_addr = reset_addr;
	wmb();
	stub_data_phys = dma_map_single(NULL, &_tegra_avp_boot_stub_data,
					sizeof(_tegra_avp_boot_stub_data),
					DMA_TO_DEVICE);
	rmb();
	reset_addr = (unsigned long)stub_data_phys;
#endif
	writel(FLOW_MODE_STOP, FLOW_CTRL_HALT_COP_EVENTS);

	writel(reset_addr, TEGRA_NVAVP_RESET_VECTOR_ADDR);

	clk_prepare_enable(nvavp->sclk);
	clk_prepare_enable(nvavp->emc_clk);

	/* If sclk_rate and emc_clk is not set by user space,
	 * max clock in dvfs table will be used to get best performance.
	 */
	nvavp->sclk_rate = ULONG_MAX;
	nvavp->emc_clk_rate = ULONG_MAX;

	tegra_periph_reset_assert(nvavp->cop_clk);
	udelay(2);
	tegra_periph_reset_deassert(nvavp->cop_clk);

	writel(FLOW_MODE_NONE, FLOW_CTRL_HALT_COP_EVENTS);

#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
	dma_unmap_single(NULL, stub_data_phys,
			 sizeof(_tegra_avp_boot_stub_data),
			 DMA_TO_DEVICE);
#endif
	return 0;
}

static void nvavp_halt_vde(struct nvavp_info *nvavp)
{
	if (nvavp->clk_enabled && !nvavp->pending)
		BUG();

	if (nvavp->pending) {
		nvavp_clks_disable(nvavp);
		nvavp->pending = false;
	}

	tegra_periph_reset_assert(nvavp->bsev_clk);
	tegra_periph_reset_assert(nvavp->vde_clk);
}

static int nvavp_reset_vde(struct nvavp_info *nvavp)
{
	if (nvavp->clk_enabled)
		BUG();

	nvavp_clks_enable(nvavp);

	tegra_periph_reset_assert(nvavp->bsev_clk);
	udelay(2);
	tegra_periph_reset_deassert(nvavp->bsev_clk);

	tegra_periph_reset_assert(nvavp->vde_clk);
	udelay(2);
	tegra_periph_reset_deassert(nvavp->vde_clk);

	/*
	 * VDE clock is set to max freq by default.
	 * VDE clock can be set to different freq if needed
	 * through ioctl.
	 */
	clk_set_rate(nvavp->vde_clk, ULONG_MAX);

	nvavp_clks_disable(nvavp);

	return 0;
}

static int nvavp_pushbuffer_alloc(struct nvavp_info *nvavp, int channel_id)
{
	int ret = 0;

	struct nvavp_channel *channel_info = nvavp_get_channel_info(
							nvavp, channel_id);

	channel_info->pushbuf_data = dma_zalloc_coherent(&nvavp->nvhost_dev->dev,
							   NVAVP_PUSHBUFFER_SIZE,
							   &channel_info->pushbuf_phys,
							   GFP_KERNEL);

	if (!channel_info->pushbuf_data) {
		dev_err(&nvavp->nvhost_dev->dev,
			"cannot alloc pushbuffer memory\n");
		ret = -ENOMEM;
	}

	return ret;
}

static void nvavp_pushbuffer_free(struct nvavp_info *nvavp)
{
	int channel_id;

	for (channel_id = 0; channel_id < NVAVP_NUM_CHANNELS; channel_id++) {
		if (nvavp->channel_info[channel_id].pushbuf_data) {
			dma_free_coherent(&nvavp->nvhost_dev->dev,
					    NVAVP_PUSHBUFFER_SIZE,
					    nvavp->channel_info[channel_id].pushbuf_data,
					    nvavp->channel_info[channel_id].pushbuf_phys);
		}
	}
}


static int nvavp_pushbuffer_init(struct nvavp_info *nvavp)
{
	int ret, channel_id;

	for (channel_id = 0; channel_id < NVAVP_NUM_CHANNELS; channel_id++) {
		ret = nvavp_pushbuffer_alloc(nvavp, channel_id);
		if (ret) {
			dev_err(&nvavp->nvhost_dev->dev,
				"unable to alloc pushbuffer\n");
			return ret;
		}
		nvavp_set_channel_control_area(nvavp, channel_id);
		if (IS_VIDEO_CHANNEL_ID(channel_id)) {
			nvavp->syncpt_id = NVSYNCPT_AVP_0;
			nvavp->syncpt_value = nvhost_syncpt_read_ext(
				nvavp->nvhost_dev, nvavp->syncpt_id);
		}

	}
	return 0;
}

static void nvavp_pushbuffer_deinit(struct nvavp_info *nvavp)
{
	nvavp_pushbuffer_free(nvavp);
}

static int nvavp_pushbuffer_update(struct nvavp_info *nvavp, u32 phys_addr,
			u32 gather_count, struct nvavp_syncpt *syncpt,
			u32 ext_ucode_flag, int channel_id)
{
	struct nvavp_channel  *channel_info;
	struct nv_e276_control *control;
	u32 gather_cmd, setucode_cmd, sync = 0;
	u32 wordcount = 0;
	u32 index, value = -1;
	int ret = 0;

	nvavp_runtime_get(nvavp);
	channel_info = nvavp_get_channel_info(nvavp, channel_id);

	control = channel_info->os_control;
	pr_debug("nvavp_pushbuffer_update for channel_id (%d):\
		control->put (0x%x) control->get (0x%x)\n",
		channel_id, (u32) &control->put, (u32) &control->get);

	mutex_lock(&channel_info->pushbuffer_lock);

	/* check for pushbuffer wrapping */
	if (channel_info->pushbuf_index >= channel_info->pushbuf_fence)
		channel_info->pushbuf_index = 0;

	if (!ext_ucode_flag) {
		setucode_cmd =
			NVE26E_CH_OPCODE_INCR(NVE276_SET_MICROCODE_A, 3);

		index = wordcount + channel_info->pushbuf_index;
		writel(setucode_cmd, (channel_info->pushbuf_data + index));
		wordcount += sizeof(u32);

		index = wordcount + channel_info->pushbuf_index;
		writel(0, (channel_info->pushbuf_data + index));
		wordcount += sizeof(u32);

		index = wordcount + channel_info->pushbuf_index;
		writel(nvavp->ucode_info.phys,
			(channel_info->pushbuf_data + index));
		wordcount += sizeof(u32);

		index = wordcount + channel_info->pushbuf_index;
		writel(nvavp->ucode_info.size,
			(channel_info->pushbuf_data + index));
		wordcount += sizeof(u32);
	}

	gather_cmd = NVE26E_CH_OPCODE_GATHER(0, 0, 0, gather_count);

	if (syncpt) {
		value = ++nvavp->syncpt_value;
		/* XXX: NvSchedValueWrappingComparison */
		sync = NVE26E_CH_OPCODE_IMM(NVE26E_HOST1X_INCR_SYNCPT,
			(NVE26E_HOST1X_INCR_SYNCPT_COND_OP_DONE << 8) |
			(nvavp->syncpt_id & 0xFF));
	}

	/* write commands out */
	index = wordcount + channel_info->pushbuf_index;
	writel(gather_cmd, (channel_info->pushbuf_data + index));
	wordcount += sizeof(u32);

	index = wordcount + channel_info->pushbuf_index;
	writel(phys_addr, (channel_info->pushbuf_data + index));
	wordcount += sizeof(u32);

	if (syncpt) {
		index = wordcount + channel_info->pushbuf_index;
		writel(sync, (channel_info->pushbuf_data + index));
		wordcount += sizeof(u32);
	}

	/* enable clocks to VDE/BSEV */
	mutex_lock(&nvavp->open_lock);
	if (!nvavp->pending && IS_VIDEO_CHANNEL_ID(channel_id)) {
		nvavp_clks_enable(nvavp);
		nvavp->pending = true;
	}
	mutex_unlock(&nvavp->open_lock);

	/* update put pointer */
	channel_info->pushbuf_index = (channel_info->pushbuf_index + wordcount)&
					(NVAVP_PUSHBUFFER_SIZE - 1);

	writel(channel_info->pushbuf_index, &control->put);
	wmb();

	/* wake up avp */

	if (IS_VIDEO_CHANNEL_ID(channel_id)) {
		pr_debug("Wake up Video Channel\n");
		ret = nvavp_outbox_write(0xA0000001);
		if (ret < 0)
			goto err_exit;
	}
	else {
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
		if (IS_AUDIO_CHANNEL_ID(channel_id)) {
			pr_debug("Wake up Audio Channel\n");
			if (!audio_enabled) {
				nvavp_runtime_get(nvavp);
				audio_enabled = true;
			}
			ret = nvavp_outbox_write(0xA0000002);
			if (ret < 0)
				goto err_exit;
		}
#endif
	}
	/* Fill out fence struct */
	if (syncpt) {
		syncpt->id = nvavp->syncpt_id;
		syncpt->value = value;
	}

err_exit:
	mutex_unlock(&channel_info->pushbuffer_lock);
	nvavp_runtime_put(nvavp);

	return 0;
}

static void nvavp_unload_ucode(struct nvavp_info *nvavp)
{
	dma_free_coherent(&nvavp->nvhost_dev->dev,  nvavp->ucode_info.size,
			   nvavp->ucode_info.data, nvavp->ucode_info.phys);
	kfree(nvavp->ucode_info.ucode_bin);
}

static int nvavp_load_ucode(struct nvavp_info *nvavp)
{
	struct nvavp_ucode_info *ucode_info = &nvavp->ucode_info;
	const struct firmware *nvavp_ucode_fw;
	char fw_ucode_file[32];
	void *ptr;
	int ret = 0;

	if (!ucode_info->ucode_bin) {
		sprintf(fw_ucode_file, "nvavp_vid_ucode.bin");

		ret = request_firmware(&nvavp_ucode_fw, fw_ucode_file,
					nvavp->video_misc_dev.this_device);
		if (ret) {
			/* Try alternative version */
			sprintf(fw_ucode_file, "nvavp_vid_ucode_alt.bin");

			ret = request_firmware(&nvavp_ucode_fw,
						fw_ucode_file,
						nvavp->video_misc_dev.this_device);

			if (ret) {
				dev_err(&nvavp->nvhost_dev->dev,
					"cannot read ucode firmware '%s'\n",
					fw_ucode_file);
				goto err_req_ucode;
			}
		}

		dev_info(&nvavp->nvhost_dev->dev,
			"read ucode firmware from '%s' (%d bytes)\n",
			fw_ucode_file, nvavp_ucode_fw->size);

		ptr = (void *)nvavp_ucode_fw->data;

		if (strncmp((const char *)ptr, "NVAVPAPP", 8)) {
			dev_info(&nvavp->nvhost_dev->dev,
				"ucode hdr string mismatch\n");
			ret = -EINVAL;
			goto err_req_ucode;
		}
		ptr += 8;
		ucode_info->size = nvavp_ucode_fw->size - 8;

		ucode_info->ucode_bin = kzalloc(ucode_info->size,
						GFP_KERNEL);
		if (!ucode_info->ucode_bin) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot allocate ucode bin\n");
			ret = -ENOMEM;
			goto err_ubin_alloc;
		}

		ucode_info->data = dma_alloc_coherent(&nvavp->nvhost_dev->dev,
						ucode_info->size,
						&ucode_info->phys,
						GFP_KERNEL);
		if (!ucode_info->data) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot alloc memory for ucode\n");
			ret = -ENOMEM;
			goto err_ucode_alloc;
		}
		memcpy(ucode_info->ucode_bin, ptr, ucode_info->size);
		release_firmware(nvavp_ucode_fw);
	}

	memcpy(ucode_info->data, ucode_info->ucode_bin, ucode_info->size);
	return 0;

err_ucode_alloc:
	kfree(nvavp->ucode_info.ucode_bin);
err_ubin_alloc:
	release_firmware(nvavp_ucode_fw);
err_req_ucode:
	return ret;
}

static void nvavp_unload_os(struct nvavp_info *nvavp)
{
	dma_free_coherent(&nvavp->nvhost_dev->dev, SZ_1M,
		nvavp->os_info.data, nvavp->os_info.phys);
	kfree(nvavp->os_info.os_bin);
}

static int nvavp_load_os(struct nvavp_info *nvavp, char *fw_os_file)
{
	struct nvavp_os_info *os_info = &nvavp->os_info;
	const struct firmware *nvavp_os_fw;
	void *ptr;
	u32 size;
	int ret = 0;

	if (!os_info->os_bin) {
		ret = request_firmware(&nvavp_os_fw, fw_os_file,
					nvavp->video_misc_dev.this_device);
		if (ret) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot read os firmware '%s'\n", fw_os_file);
			goto err_req_fw;
		}

		dev_info(&nvavp->nvhost_dev->dev,
			"read firmware from '%s' (%d bytes)\n",
			fw_os_file, nvavp_os_fw->size);

		ptr = (void *)nvavp_os_fw->data;

		if (strncmp((const char *)ptr, "NVAVP-OS", 8)) {
			dev_info(&nvavp->nvhost_dev->dev,
				"os hdr string mismatch\n");
			ret = -EINVAL;
			goto err_os_bin;
		}

		ptr += 8;
		os_info->entry_offset = *((u32 *)ptr);
		ptr += sizeof(u32);
		os_info->control_offset = *((u32 *)ptr);
		ptr += sizeof(u32);
		os_info->debug_offset = *((u32 *)ptr);
		ptr += sizeof(u32);

		size = *((u32 *)ptr);    ptr += sizeof(u32);

		os_info->size = size;
		os_info->os_bin = kzalloc(os_info->size,
						GFP_KERNEL);
		if (!os_info->os_bin) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot allocate os bin\n");
			ret = -ENOMEM;
			goto err_os_bin;
		}

		memcpy(os_info->os_bin, ptr, os_info->size);
		memset(os_info->data + os_info->size, 0, SZ_1M - os_info->size);

		dev_info(&nvavp->nvhost_dev->dev,
			"entry=%08x control=%08x debug=%08x size=%d\n",
			os_info->entry_offset, os_info->control_offset,
			os_info->debug_offset, os_info->size);
		release_firmware(nvavp_os_fw);
	}

	memcpy(os_info->data, os_info->os_bin, os_info->size);
	os_info->reset_addr = os_info->phys + os_info->entry_offset;

	dev_info(&nvavp->nvhost_dev->dev,
		"AVP os at vaddr=%p paddr=%llx reset_addr=%llx\n",
		os_info->data, (u64)(os_info->phys), (u64)os_info->reset_addr);
	return 0;

err_os_bin:
	release_firmware(nvavp_os_fw);
err_req_fw:
	return ret;
}


static int nvavp_os_init(struct nvavp_info *nvavp)
{
	char fw_os_file[32];
	int ret = 0;
	int video_initialized, audio_initialized = 0;

	video_initialized = nvavp_get_video_init_status(nvavp);

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	audio_initialized = nvavp_get_audio_init_status(nvavp);
#endif
	pr_debug("video_initialized(%d) audio_initialized(%d)\n",
		video_initialized, audio_initialized);

	/* Video and Audio both are initialized */
	if (video_initialized || audio_initialized)
		return ret;

	/* Video or Audio both are uninitialized */
	pr_debug("video_initialized == audio_initialized (%d)\n",
		nvavp->video_initialized);
#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU) /* Tegra2 with AVP MMU */
	/* paddr is phys address */
	/* vaddr is AVP_KERNEL_VIRT_BASE */
	dev_info(&nvavp->nvhost_dev->dev,
		"using AVP MMU to relocate AVP os\n");
	sprintf(fw_os_file, "nvavp_os.bin");
	nvavp->os_info.reset_addr = AVP_KERNEL_VIRT_BASE;
#elif defined(CONFIG_TEGRA_AVP_KERNEL_ON_SMMU) /* Tegra3 with SMMU */
	/* paddr is any address behind SMMU */
	/* vaddr is TEGRA_SMMU_BASE */
	dev_info(&nvavp->nvhost_dev->dev,
		"using SMMU at %lx to load AVP kernel\n",
		(unsigned long)nvavp->os_info.phys);
	BUG_ON(nvavp->os_info.phys != 0xeff00000
		&& nvavp->os_info.phys != 0x0ff00000
		&& nvavp->os_info.phys != 0x8ff00000);
	sprintf(fw_os_file, "nvavp_os_%08lx.bin",
		(unsigned long)nvavp->os_info.phys);
	nvavp->os_info.reset_addr = nvavp->os_info.phys;
#else /* nvmem= carveout */
	dev_info(&nvavp->nvhost_dev->dev,
		"using nvmem= carveout at %llx to load AVP os\n",
		(u64)nvavp->os_info.phys);
	sprintf(fw_os_file, "nvavp_os_%08llx.bin", (u64)nvavp->os_info.phys);
	nvavp->os_info.reset_addr = nvavp->os_info.phys;
	nvavp->os_info.data = ioremap(nvavp->os_info.phys, SZ_1M);
#endif
	ret = nvavp_load_os(nvavp, fw_os_file);
	if (ret) {
		dev_err(&nvavp->nvhost_dev->dev,
			"unable to load os firmware '%s'\n", fw_os_file);
		goto err_exit;
	}

	ret = nvavp_pushbuffer_init(nvavp);
	if (ret) {
		dev_err(&nvavp->nvhost_dev->dev,
			"unable to init pushbuffer\n");
		goto err_exit;
	}
	tegra_init_legacy_irq_cop();
	enable_irq(nvavp->mbox_from_avp_pend_irq);
err_exit:
	return ret;
}

static int nvavp_init(struct nvavp_info *nvavp, int channel_id)
{
	int ret = 0;
	int video_initialized = 0, audio_initialized = 0;

	nvavp->init_task = current;

	ret = nvavp_os_init(nvavp);
	if (ret) {
		dev_err(&nvavp->nvhost_dev->dev,
			"unable to load os firmware and allocate buffers\n");
	}

	video_initialized = nvavp_get_video_init_status(nvavp);
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	audio_initialized = nvavp_get_audio_init_status(nvavp);
#endif

	if (IS_VIDEO_CHANNEL_ID(channel_id) && (!video_initialized)) {
		pr_debug("nvavp_init : channel_ID (%d)\n", channel_id);
		ret = nvavp_load_ucode(nvavp);
		if (ret) {
			dev_err(&nvavp->nvhost_dev->dev,
				"unable to load ucode\n");
			goto err_exit;
		}

		nvavp_reset_vde(nvavp);
		nvavp_reset_avp(nvavp, nvavp->os_info.reset_addr);

		nvavp_set_video_init_status(nvavp, 1);
	}
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	if (IS_AUDIO_CHANNEL_ID(channel_id) && (!audio_initialized)) {
		pr_debug("nvavp_init : channel_ID (%d)\n", channel_id);
		nvavp_reset_avp(nvavp, nvavp->os_info.reset_addr);
		nvavp_set_audio_init_status(nvavp, 1);
	}
#endif

err_exit:
	nvavp->init_task = NULL;
	return ret;
}

#define TIMER_PTV	0
#define TIMER_EN	(1 << 31)
#define TIMER_PERIODIC	(1 << 30)
#define TIMER_PCR	0x4
#define TIMER_PCR_INTR	(1 << 30)

static void nvavp_uninit(struct nvavp_info *nvavp)
{
	int video_initialized, audio_initialized = 0;
	unsigned int reg;

	video_initialized = nvavp_get_video_init_status(nvavp);

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	audio_initialized = nvavp_get_audio_init_status(nvavp);
#endif

	pr_debug("nvavp_uninit video_initialized(%d) audio_initialized(%d)\n",
		video_initialized, audio_initialized);

	/* Video and Audio both are uninitialized */
	if (!video_initialized && !audio_initialized)
		return;

	nvavp->init_task = current;

	if (video_initialized) {
		pr_debug("nvavp_uninit nvavp->video_initialized\n");
		cancel_work_sync(&nvavp->clock_disable_work);
		nvavp_halt_vde(nvavp);
		nvavp_set_video_init_status(nvavp, 0);
		video_initialized = 0;
	}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	if (audio_initialized) {
		cancel_work_sync(&nvavp->app_notify_work);
		nvavp_set_audio_init_status(nvavp, 0);
		audio_initialized = 0;
	}
#endif

	/* Video and Audio both becomes uninitialized */
	if (!video_initialized && !audio_initialized) {
		pr_debug("nvavp_uninit both channels uninitialized\n");

		clk_disable_unprepare(nvavp->sclk);
		clk_disable_unprepare(nvavp->emc_clk);
		disable_irq(nvavp->mbox_from_avp_pend_irq);
		nvavp_pushbuffer_deinit(nvavp);
		nvavp_halt_avp(nvavp);
	}

	/*
	 * WAR: turn off TMR2 for fix LP1 wake up by TMR2.
	 * turn off the periodic interrupt and the timer temporarily
	 */
	reg = readl(IO_ADDRESS(TEGRA_TMR2_BASE + TIMER_PTV));
	reg &= ~(TIMER_EN | TIMER_PERIODIC);
	writel(reg, IO_ADDRESS(TEGRA_TMR2_BASE + TIMER_PTV));

	/* write a 1 to the intr_clr field to clear the interrupt */
	reg = TIMER_PCR_INTR;
	writel(reg, IO_ADDRESS(TEGRA_TMR2_BASE + TIMER_PCR));

	nvavp->init_task = NULL;
}

static int nvcpu_set_clock(struct nvavp_info *nvavp,
				struct nvavp_clock_args config,
				unsigned long arg)
{
	dev_dbg(&nvavp->nvhost_dev->dev, "%s: update cpu freq to clk_rate=%u\n",
			__func__, config.rate);

	if (config.rate > 0)
		pm_qos_update_request(&nvavp->min_cpu_freq_req, config.rate);
	else
		pm_qos_update_request(&nvavp->min_cpu_freq_req,
					PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);

	return 0;
}

static int nvavp_map_iova(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_map_args map_arg;
	struct dma_buf *dmabuf;
	dma_addr_t addr = 0;
	int ret = 0;

	if (copy_from_user(&map_arg, (void __user *)arg,
		sizeof(struct nvavp_map_args))) {
		dev_err(&nvavp->nvhost_dev->dev,
			"failed to copy memory handle\n");
		return -EFAULT;
	}
	if (!map_arg.fd) {
		dev_err(&nvavp->nvhost_dev->dev,
			"invalid memory handle %08x\n", map_arg.fd);
		return -EINVAL;
	}

	dmabuf = dma_buf_get(map_arg.fd);
	if (IS_ERR(dmabuf)) {
		dev_err(&nvavp->nvhost_dev->dev,
			"invalid buffer handle %08x\n", map_arg.fd);
		return PTR_ERR(dmabuf);
	}

	ret = nvavp_get_iova_addr(clientctx, dmabuf, &addr);
	if (ret)
		goto out;

	map_arg.addr = (__u32)addr;

	if (copy_to_user((void __user *)arg, &map_arg,
		sizeof(struct nvavp_map_args))) {
		dev_err(&nvavp->nvhost_dev->dev,
			"failed to copy phys addr\n");
		ret = -EFAULT;
	}

out:
	return ret;
}

static int nvavp_unmap_iova(struct file *filp, unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_map_args map_arg;
	struct dma_buf *dmabuf;

	if (copy_from_user(&map_arg, (void __user *)arg,
		sizeof(struct nvavp_map_args))) {
		dev_err(&nvavp->nvhost_dev->dev,
			"failed to copy memory handle\n");
		return -EFAULT;
	}

	dmabuf = dma_buf_get(map_arg.fd);
	if (IS_ERR(dmabuf)) {
		dev_err(&nvavp->nvhost_dev->dev,
			"invalid buffer handle %08x\n", map_arg.fd);
		return PTR_ERR(dmabuf);
	}

	nvavp_release_iova_addr(clientctx, dmabuf, (dma_addr_t)map_arg.addr);
	return 0;
}

static int nvavp_set_clock_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct clk *c;
	struct nvavp_clock_args config;

	if (copy_from_user(&config, (void __user *)arg, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s: clk_id=%d, clk_rate=%u\n",
			__func__, config.id, config.rate);

	if (config.id == NVAVP_MODULE_ID_AVP)
		nvavp->sclk_rate = config.rate;
	else if	(config.id == NVAVP_MODULE_ID_EMC)
		nvavp->emc_clk_rate = config.rate;
	else if (config.id == NVAVP_MODULE_ID_CPU)
		return nvcpu_set_clock(nvavp, config, arg);

	c = nvavp_clk_get(nvavp, config.id);
	if (IS_ERR_OR_NULL(c))
		return -EINVAL;

	clk_prepare_enable(c);
	clk_set_rate(c, config.rate);

	config.rate = clk_get_rate(c);
	clk_disable_unprepare(c);
	if (copy_to_user((void __user *)arg, &config, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	return 0;
}

static int nvavp_get_clock_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct clk *c;
	struct nvavp_clock_args config;

	if (copy_from_user(&config, (void __user *)arg, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	c = nvavp_clk_get(nvavp, config.id);
	if (IS_ERR_OR_NULL(c))
		return -EINVAL;

	clk_prepare_enable(c);
	config.rate = clk_get_rate(c);
	clk_disable_unprepare(c);

	if (copy_to_user((void __user *)arg, &config, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	return 0;
}

static int nvavp_get_syncpointid_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	u32 id = nvavp->syncpt_id;

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &id, sizeof(u32)))
			return -EFAULT;
		else
			return 0;
	}
	return -EFAULT;
}

static int nvavp_pushbuffer_submit_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_pushbuffer_submit_hdr hdr;
	u32 *cmdbuf_data;
	struct dma_buf *cmdbuf_dmabuf;
	struct dma_buf_attachment *cmdbuf_attach;
	struct sg_table *cmdbuf_sgt;
	int ret = 0, i;
	phys_addr_t phys_addr;
	unsigned long virt_addr;
	struct nvavp_pushbuffer_submit_hdr *user_hdr =
			(struct nvavp_pushbuffer_submit_hdr *) arg;
	struct nvavp_syncpt syncpt;

	syncpt.id = NVSYNCPT_INVALID;
	syncpt.value = 0;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&hdr, (void __user *)arg,
			sizeof(struct nvavp_pushbuffer_submit_hdr)))
			return -EFAULT;
	}

	if (!hdr.cmdbuf.mem)
		return 0;

	if (copy_from_user(clientctx->relocs, (void __user *)hdr.relocs,
			sizeof(struct nvavp_reloc) * hdr.num_relocs)) {
		return -EFAULT;
	}

	cmdbuf_dmabuf = dma_buf_get(hdr.cmdbuf.mem);
	if (IS_ERR(cmdbuf_dmabuf)) {
		dev_err(&nvavp->nvhost_dev->dev,
			"invalid cmd buffer handle %08x\n", hdr.cmdbuf.mem);
		return PTR_ERR(cmdbuf_dmabuf);
	}

	cmdbuf_attach = dma_buf_attach(cmdbuf_dmabuf, &nvavp->nvhost_dev->dev);
	if (IS_ERR(cmdbuf_attach)) {
		dev_err(&nvavp->nvhost_dev->dev, "cannot attach cmdbuf_dmabuf\n");
		ret = PTR_ERR(cmdbuf_attach);
		goto err_dmabuf_attach;
	}

	cmdbuf_sgt = dma_buf_map_attachment(cmdbuf_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(cmdbuf_sgt)) {
		dev_err(&nvavp->nvhost_dev->dev, "cannot map cmdbuf_dmabuf\n");
		ret = PTR_ERR(cmdbuf_sgt);
		goto err_dmabuf_map;
	}

	phys_addr = sg_dma_address(cmdbuf_sgt->sgl);

	virt_addr = (unsigned long)dma_buf_vmap(cmdbuf_dmabuf);
	if (!virt_addr) {
		dev_err(&nvavp->nvhost_dev->dev, "cannot vmap cmdbuf_dmabuf\n");
		ret = -ENOMEM;
		goto err_dmabuf_vmap;
	}

	cmdbuf_data = (u32 *)(virt_addr + hdr.cmdbuf.offset);
	for (i = 0; i < hdr.num_relocs; i++) {
		struct dma_buf *target_dmabuf;
		struct dma_buf_attachment *target_attach;
		struct sg_table *target_sgt;
		u32 *reloc_addr, target_phys_addr;

		if (clientctx->relocs[i].cmdbuf_mem != hdr.cmdbuf.mem) {
			dev_err(&nvavp->nvhost_dev->dev,
				"reloc info does not match target bufferID\n");
			ret = -EPERM;
			goto err_reloc_info;
		}

		reloc_addr = cmdbuf_data +
			     (clientctx->relocs[i].cmdbuf_offset >> 2);

		target_dmabuf = dma_buf_get(clientctx->relocs[i].target);
		if (IS_ERR(target_dmabuf)) {
			ret = PTR_ERR(target_dmabuf);
			goto target_dmabuf_fail;
		}
		target_attach = dma_buf_attach(target_dmabuf,
					       &nvavp->nvhost_dev->dev);
		if (IS_ERR(target_attach)) {
			ret = PTR_ERR(target_attach);
			goto target_attach_fail;
		}
		target_sgt = dma_buf_map_attachment(target_attach,
						    DMA_BIDIRECTIONAL);
		if (IS_ERR(target_sgt)) {
			ret = PTR_ERR(target_sgt);
			goto target_map_fail;
		}

		target_phys_addr = sg_dma_address(target_sgt->sgl);
		target_phys_addr += clientctx->relocs[i].target_offset;
		writel(target_phys_addr, reloc_addr);
		dma_buf_unmap_attachment(target_attach, target_sgt,
					 DMA_BIDIRECTIONAL);
target_map_fail:
		dma_buf_detach(target_dmabuf, target_attach);
target_attach_fail:
		dma_buf_put(target_dmabuf);
target_dmabuf_fail:
		if (ret != 0)
			goto err_reloc_info;
	}

	if (hdr.syncpt) {
		ret = nvavp_pushbuffer_update(nvavp,
					     (phys_addr + hdr.cmdbuf.offset),
					      hdr.cmdbuf.words, &syncpt,
					      (hdr.flags & NVAVP_UCODE_EXT),
						clientctx->channel_id);

		if (copy_to_user((void __user *)user_hdr->syncpt, &syncpt,
				sizeof(struct nvavp_syncpt))) {
			ret = -EFAULT;
			goto err_reloc_info;
		}
	} else {
		ret = nvavp_pushbuffer_update(nvavp,
					     (phys_addr + hdr.cmdbuf.offset),
					      hdr.cmdbuf.words, NULL,
					      (hdr.flags & NVAVP_UCODE_EXT),
						clientctx->channel_id);
	}

err_reloc_info:
	dma_buf_vunmap(cmdbuf_dmabuf, (void *)virt_addr);
err_dmabuf_vmap:
	dma_buf_unmap_attachment(cmdbuf_attach, cmdbuf_sgt, DMA_BIDIRECTIONAL);
err_dmabuf_map:
	dma_buf_detach(cmdbuf_dmabuf, cmdbuf_attach);
err_dmabuf_attach:
	dma_buf_put(cmdbuf_dmabuf);
	return ret;
}

static int nvavp_wake_avp_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	wmb();
	/* wake up avp */
	return nvavp_outbox_write(0xA0000001);
}

static int nvavp_force_clock_stay_on_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_clock_stay_on_state_args clock;

	if (copy_from_user(&clock, (void __user *)arg,
			   sizeof(struct nvavp_clock_stay_on_state_args)))
		return -EFAULT;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s: state=%d\n",
		__func__, clock.state);

	if (clock.state != NVAVP_CLOCK_STAY_ON_DISABLED &&
		   clock.state !=  NVAVP_CLOCK_STAY_ON_ENABLED) {
		dev_err(&nvavp->nvhost_dev->dev, "%s: invalid argument=%d\n",
			__func__, clock.state);
		return -EINVAL;
	}

	if (clock.state) {
		mutex_lock(&nvavp->open_lock);
		if (clientctx->clk_reqs++ == 0) {
			nvavp_clks_enable(nvavp);
			nvavp->stay_on = true;
		}
		mutex_unlock(&nvavp->open_lock);
		cancel_work_sync(&nvavp->clock_disable_work);
	} else {
		mutex_lock(&nvavp->open_lock);
		if (--clientctx->clk_reqs == 0) {
			nvavp->stay_on = false;
			nvavp_clks_disable(nvavp);
		}
		mutex_unlock(&nvavp->open_lock);
	}
	return 0;
}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
static int nvavp_enable_audio_clocks(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_clock_args config;

	if (copy_from_user(&config, (void __user *)arg, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s: clk_id=%d\n",
			__func__, config.id);

	if (config.id == NVAVP_MODULE_ID_VCP)
		clk_prepare_enable(nvavp->vcp_clk);
	else if	(config.id == NVAVP_MODULE_ID_BSEA)
		clk_prepare_enable(nvavp->bsea_clk);

	return 0;
}

static int nvavp_disable_audio_clocks(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_clock_args config;

	if (copy_from_user(&config, (void __user *)arg, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s: clk_id=%d\n",
			__func__, config.id);

	if (config.id == NVAVP_MODULE_ID_VCP)
		clk_disable_unprepare(nvavp->vcp_clk);
	else if	(config.id == NVAVP_MODULE_ID_BSEA)
		clk_disable_unprepare(nvavp->bsea_clk);

	return 0;
}
#else
static int nvavp_enable_audio_clocks(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	return 0;
}

static int nvavp_disable_audio_clocks(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	return 0;
}
#endif

static int nvavp_set_min_online_cpus_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_num_cpus_args config;

	if (copy_from_user(&config, (void __user *)arg,
					sizeof(struct nvavp_num_cpus_args)))
		return -EFAULT;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s: min_online_cpus=%d\n",
			__func__, config.min_online_cpus);

	if (config.min_online_cpus > 0)
		pm_qos_update_request(&nvavp->min_online_cpus_req,
					config.min_online_cpus);
	else
		pm_qos_update_request(&nvavp->min_online_cpus_req,
					PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);

	return 0;
}

static int tegra_nvavp_open(struct inode *inode, struct file *filp, int channel_id)
{
	struct miscdevice *miscdev = filp->private_data;
	struct nvavp_info *nvavp = dev_get_drvdata(miscdev->parent);
	int ret = 0;
	struct nvavp_clientctx *clientctx;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s: ++\n", __func__);

	nonseekable_open(inode, filp);

	clientctx = kzalloc(sizeof(*clientctx), GFP_KERNEL);
	if (!clientctx)
		return -ENOMEM;

	mutex_lock(&nvavp->open_lock);

	pr_debug("tegra_nvavp_open channel_id (%d)\n", channel_id);

	clientctx->channel_id = channel_id;

	ret = nvavp_init(nvavp, channel_id);

	if (!ret) {
		nvavp->refcount++;
		if (IS_VIDEO_CHANNEL_ID(channel_id))
			nvavp->video_refcnt++;
		if (IS_AUDIO_CHANNEL_ID(channel_id))
			nvavp->audio_refcnt++;
	}

	clientctx->nvavp = nvavp;
	clientctx->iova_handles = RB_ROOT;

	filp->private_data = clientctx;

	mutex_unlock(&nvavp->open_lock);

	return ret;
}

static int tegra_nvavp_video_open(struct inode *inode, struct file *filp)
{
	pr_debug("tegra_nvavp_video_open NVAVP_VIDEO_CHANNEL\n");
	return tegra_nvavp_open(inode, filp, NVAVP_VIDEO_CHANNEL);
}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
static int tegra_nvavp_audio_open(struct inode *inode, struct file *filp)
{
	pr_debug("tegra_nvavp_audio_open NVAVP_AUDIO_CHANNEL\n");
	return tegra_nvavp_open(inode, filp, NVAVP_AUDIO_CHANNEL);
}
#endif

static int tegra_nvavp_release(struct inode *inode, struct file *filp, int channel_id)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	int ret = 0;

	dev_dbg(&nvavp->nvhost_dev->dev, "%s: ++\n", __func__);

	filp->private_data = NULL;

	mutex_lock(&nvavp->open_lock);

	if (!nvavp->refcount) {
		dev_err(&nvavp->nvhost_dev->dev,
			"releasing while in invalid state\n");
		ret = -EINVAL;
		goto out;
	}

	/* if this client had any requests, drop our clk ref */
	if (clientctx->clk_reqs)
		nvavp_clks_disable(nvavp);

	if (nvavp->refcount > 0)
		nvavp->refcount--;
	if (!nvavp->refcount)
		nvavp_uninit(nvavp);

	if (IS_VIDEO_CHANNEL_ID(channel_id))
		nvavp->video_refcnt--;
	if (IS_AUDIO_CHANNEL_ID(channel_id))
		nvavp->audio_refcnt--;

out:
	mutex_unlock(&nvavp->open_lock);
	nvavp_remove_iova_mapping(clientctx);
	kfree(clientctx);
	return ret;
}

static int tegra_nvavp_video_release(struct inode *inode, struct file *filp)
{
	return tegra_nvavp_release(inode, filp, NVAVP_VIDEO_CHANNEL);
}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
static int tegra_nvavp_audio_release(struct inode *inode, struct file *filp)
{
	return tegra_nvavp_release(inode, filp, NVAVP_AUDIO_CHANNEL);
}
#endif

static long tegra_nvavp_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;

	if (_IOC_TYPE(cmd) != NVAVP_IOCTL_MAGIC ||
	    _IOC_NR(cmd) < NVAVP_IOCTL_MIN_NR ||
	    _IOC_NR(cmd) > NVAVP_IOCTL_MAX_NR)
		return -EFAULT;

	switch (cmd) {
	case NVAVP_IOCTL_SET_NVMAP_FD:
		break;
	case NVAVP_IOCTL_GET_SYNCPOINT_ID:
		ret = nvavp_get_syncpointid_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_PUSH_BUFFER_SUBMIT:
		ret = nvavp_pushbuffer_submit_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_SET_CLOCK:
		ret = nvavp_set_clock_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_GET_CLOCK:
		ret = nvavp_get_clock_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_WAKE_AVP:
		ret = nvavp_wake_avp_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_FORCE_CLOCK_STAY_ON:
		ret = nvavp_force_clock_stay_on_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_ENABLE_AUDIO_CLOCKS:
		ret = nvavp_enable_audio_clocks(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_DISABLE_AUDIO_CLOCKS:
		ret = nvavp_disable_audio_clocks(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_SET_MIN_ONLINE_CPUS:
		ret = nvavp_set_min_online_cpus_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_MAP_IOVA:
		ret = nvavp_map_iova(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_UNMAP_IOVA:
		ret = nvavp_unmap_iova(filp, arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct file_operations tegra_video_nvavp_fops = {
	.owner		= THIS_MODULE,
	.open		= tegra_nvavp_video_open,
	.release	= tegra_nvavp_video_release,
	.unlocked_ioctl	= tegra_nvavp_ioctl,
};

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
static const struct file_operations tegra_audio_nvavp_fops = {
	.owner          = THIS_MODULE,
	.open           = tegra_nvavp_audio_open,
	.release        = tegra_nvavp_audio_release,
	.unlocked_ioctl = tegra_nvavp_ioctl,
};
#endif

static ssize_t boost_sclk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", boost_sclk);
}

static ssize_t boost_sclk_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct nvavp_info *nvavp = platform_get_drvdata(ndev);
	unsigned long val = 0;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	if (val)
		clk_set_rate(nvavp->sclk, SCLK_BOOST_RATE);
	else if (!val)
		clk_set_rate(nvavp->sclk, 0);

	boost_sclk = val;

	return count;
}

DEVICE_ATTR(boost_sclk, S_IRUGO | S_IWUSR, boost_sclk_show, boost_sclk_store);

enum nvavp_heap {
	NVAVP_USE_SMMU = (1 << 0),
	NVAVP_USE_CARVEOUT = (1 << 1)
};

static int nvavp_reserve_os_mem(struct nvavp_info *nvavp, dma_addr_t phys)
{
	int ret = -ENOMEM;
	if (!pfn_valid(__phys_to_pfn(phys))) {
		if (memblock_reserve(phys, SZ_1M)) {
			dev_err(&nvavp->nvhost_dev->dev,
				"failed to reserve mem block %lx\n", phys);
		} else
			ret = 0;
	}
	return ret;
}

static int tegra_nvavp_probe(struct platform_device *ndev)
{
	struct nvavp_info *nvavp;
	int irq;
	enum nvavp_heap heap_mask;
	int ret = 0, channel_id;

	irq = platform_get_irq_byname(ndev, "mbox_from_nvavp_pending");
	if (irq < 0) {
		dev_err(&ndev->dev, "invalid nvhost data\n");
		return -EINVAL;
	}

	/* Set the max segment size supported. */
	ndev->dev.dma_parms = &nvavp_dma_parameters;

	nvavp = kzalloc(sizeof(struct nvavp_info), GFP_KERNEL);
	if (!nvavp) {
		dev_err(&ndev->dev, "cannot allocate avp_info\n");
		return -ENOMEM;
	}

	memset(nvavp, 0, sizeof(*nvavp));

#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU) /* Tegra2 with AVP MMU */
	heap_mask = NVAVP_USE_CARVEOUT;
#elif defined(CONFIG_TEGRA_AVP_KERNEL_ON_SMMU) /* Tegra3 with SMMU */
	heap_mask = NVAVP_USE_SMMU;
#else /* nvmem= carveout */
	heap_mask = NVAVP_USE_CARVEOUT;
#endif
	switch (heap_mask) {
	case NVAVP_USE_SMMU:

		nvavp->os_info.phys = 0x8ff00000;
		nvavp->os_info.data = dma_alloc_at_coherent(
							&ndev->dev,
							SZ_1M,
							&nvavp->os_info.phys,
							GFP_KERNEL);

		if (!nvavp->os_info.data || nvavp->os_info.phys != 0x8ff00000) {
			nvavp->os_info.phys = 0x0ff00000;
			nvavp->os_info.data = dma_alloc_at_coherent(
							&ndev->dev,
							SZ_1M,
							&nvavp->os_info.phys,
							GFP_KERNEL);

			if (!nvavp->os_info.data ||
			    nvavp->os_info.phys != 0x0ff00000) {
				dev_err(&ndev->dev, "cannot allocate IOVA memory\n");
				ret = -ENOMEM;
			}
		}

		dev_info(&ndev->dev,
			"allocated IOVA at %lx for AVP os\n",
			(unsigned long)nvavp->os_info.phys);
		break;
	case NVAVP_USE_CARVEOUT:
		if (!nvavp_reserve_os_mem(nvavp, 0x8e000000))
			nvavp->os_info.phys = 0x8e000000;
		else if (!nvavp_reserve_os_mem(nvavp, 0xf7e00000))
			nvavp->os_info.phys = 0xf7e00000;
		else if (!nvavp_reserve_os_mem(nvavp, 0x9e000000))
			nvavp->os_info.phys = 0x9e000000;
		else if (!nvavp_reserve_os_mem(nvavp, 0xbe000000))
			nvavp->os_info.phys = 0xbe000000;
		else {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot find nvmem= carveout to load AVP os\n");
			dev_err(&nvavp->nvhost_dev->dev,
				"check kernel command line "
				"to see if nvmem= is defined\n");
			BUG();

		}

		dev_info(&ndev->dev,
			"allocated carveout memory at %lx for AVP os\n",
			(unsigned long)nvavp->os_info.phys);
		break;
	default:
		dev_err(&ndev->dev, "invalid/non-supported heap for AVP os\n");
		ret = -EINVAL;
		goto err_get_syncpt;
	}

	nvavp->mbox_from_avp_pend_irq = irq;
	mutex_init(&nvavp->open_lock);

	for (channel_id = 0; channel_id < NVAVP_NUM_CHANNELS; channel_id++)
		mutex_init(&nvavp->channel_info[channel_id].pushbuffer_lock);

	/* TODO DO NOT USE NVAVP DEVICE */
	nvavp->cop_clk = clk_get(&ndev->dev, "cop");
	if (IS_ERR(nvavp->cop_clk)) {
		dev_err(&ndev->dev, "cannot get cop clock\n");
		ret = -ENOENT;
		goto err_get_cop_clk;
	}

	nvavp->vde_clk = clk_get(&ndev->dev, "vde");
	if (IS_ERR(nvavp->vde_clk)) {
		dev_err(&ndev->dev, "cannot get vde clock\n");
		ret = -ENOENT;
		goto err_get_vde_clk;
	}

	nvavp->bsev_clk = clk_get(&ndev->dev, "bsev");
	if (IS_ERR(nvavp->bsev_clk)) {
		dev_err(&ndev->dev, "cannot get bsev clock\n");
		ret = -ENOENT;
		goto err_get_bsev_clk;
	}

	nvavp->sclk = clk_get(&ndev->dev, "sclk");
	if (IS_ERR(nvavp->sclk)) {
		dev_err(&ndev->dev, "cannot get avp.sclk clock\n");
		ret = -ENOENT;
		goto err_get_sclk;
	}

	nvavp->emc_clk = clk_get(&ndev->dev, "emc");
	if (IS_ERR(nvavp->emc_clk)) {
		dev_err(&ndev->dev, "cannot get emc clock\n");
		ret = -ENOENT;
		goto err_get_emc_clk;
	}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	nvavp->bsea_clk = clk_get(&ndev->dev, "bsea");
	if (IS_ERR(nvavp->bsea_clk)) {
		dev_err(&ndev->dev, "cannot get bsea clock\n");
		ret = -ENOENT;
		goto err_get_bsea_clk;
	}

	nvavp->vcp_clk = clk_get(&ndev->dev, "vcp");
	if (IS_ERR(nvavp->vcp_clk)) {
		dev_err(&ndev->dev, "cannot get vcp clock\n");
		ret = -ENOENT;
		goto err_get_vcp_clk;
	}
#endif

	nvavp->clk_enabled = 0;
	nvavp_halt_avp(nvavp);

	INIT_WORK(&nvavp->clock_disable_work, clock_disable_handler);

	nvavp->video_misc_dev.minor = MISC_DYNAMIC_MINOR;
	nvavp->video_misc_dev.name = "tegra_avpchannel";
	nvavp->video_misc_dev.fops = &tegra_video_nvavp_fops;
	nvavp->video_misc_dev.mode = S_IRWXUGO;
	nvavp->video_misc_dev.parent = &ndev->dev;

	ret = misc_register(&nvavp->video_misc_dev);
	if (ret) {
		dev_err(&ndev->dev, "unable to register misc device!\n");
		goto err_misc_reg;
	}

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	INIT_WORK(&nvavp->app_notify_work, app_notify_handler);
	nvavp->audio_misc_dev.minor = MISC_DYNAMIC_MINOR;
	nvavp->audio_misc_dev.name = "tegra_audio_avpchannel";
	nvavp->audio_misc_dev.fops = &tegra_audio_nvavp_fops;
	nvavp->audio_misc_dev.mode = S_IRWXUGO;
	nvavp->audio_misc_dev.parent = &ndev->dev;

	ret = misc_register(&nvavp->audio_misc_dev);
	if (ret) {
	dev_err(&ndev->dev, "unable to register misc device!\n");
		goto err_audio_misc_reg;
	}
#endif

	ret = request_irq(irq, nvavp_mbox_pending_isr, 0,
			  TEGRA_NVAVP_NAME, nvavp);
	if (ret) {
		dev_err(&ndev->dev, "cannot register irq handler\n");
		goto err_req_irq_pend;
	}
	disable_irq(nvavp->mbox_from_avp_pend_irq);

	nvavp->nvhost_dev = ndev;
	platform_set_drvdata(ndev, nvavp);

	tegra_pd_add_device(&ndev->dev);
	pm_runtime_use_autosuspend(&ndev->dev);
	pm_runtime_set_autosuspend_delay(&ndev->dev, 2000);
	pm_runtime_enable(&ndev->dev);

	ret = device_create_file(&ndev->dev, &dev_attr_boost_sclk);
	if (ret) {
		dev_err(&ndev->dev,
			"%s: device_create_file failed\n", __func__);
		goto err_req_irq_pend;
	}

	/* Add PM QoS request but leave it as default value */
	pm_qos_add_request(&nvavp->min_cpu_freq_req,
				PM_QOS_CPU_FREQ_MIN,
				PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&nvavp->min_online_cpus_req,
				PM_QOS_MIN_ONLINE_CPUS,
				PM_QOS_DEFAULT_VALUE);

	return 0;

err_req_irq_pend:
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	misc_deregister(&nvavp->audio_misc_dev);
err_audio_misc_reg:
#endif
	misc_deregister(&nvavp->video_misc_dev);
err_misc_reg:
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	clk_put(nvavp->vcp_clk);
err_get_vcp_clk:
	clk_put(nvavp->bsea_clk);
err_get_bsea_clk:
#endif
	clk_put(nvavp->emc_clk);
err_get_emc_clk:
	clk_put(nvavp->sclk);
err_get_sclk:
	clk_put(nvavp->bsev_clk);
err_get_bsev_clk:
	clk_put(nvavp->vde_clk);
err_get_vde_clk:
	clk_put(nvavp->cop_clk);
err_get_cop_clk:
err_get_syncpt:
	kfree(nvavp);
	return ret;
}

static int tegra_nvavp_remove(struct platform_device *ndev)
{
	struct nvavp_info *nvavp = platform_get_drvdata(ndev);

	if (!nvavp)
		return 0;

	mutex_lock(&nvavp->open_lock);
	if (nvavp->refcount) {
		mutex_unlock(&nvavp->open_lock);
		return -EBUSY;
	}
	mutex_unlock(&nvavp->open_lock);

	nvavp_unload_ucode(nvavp);
	nvavp_unload_os(nvavp);

	device_remove_file(&ndev->dev, &dev_attr_boost_sclk);

	misc_deregister(&nvavp->video_misc_dev);

#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	misc_deregister(&nvavp->audio_misc_dev);
	clk_put(nvavp->vcp_clk);
	clk_put(nvavp->bsea_clk);
#endif
	clk_put(nvavp->bsev_clk);
	clk_put(nvavp->vde_clk);
	clk_put(nvavp->cop_clk);

	clk_put(nvavp->emc_clk);
	clk_put(nvavp->sclk);

	if (!IS_ERR_OR_NULL(&nvavp->min_cpu_freq_req)) {
		pm_qos_update_request(&nvavp->min_cpu_freq_req,
				PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);
		pm_qos_remove_request(&nvavp->min_cpu_freq_req);
	}
	if (!IS_ERR_OR_NULL(&nvavp->min_online_cpus_req)) {
		pm_qos_update_request(&nvavp->min_online_cpus_req,
				PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);
		pm_qos_remove_request(&nvavp->min_online_cpus_req);
	}

	kfree(nvavp);
	return 0;
}

#ifdef CONFIG_PM
static int tegra_nvavp_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvavp_info *nvavp = platform_get_drvdata(pdev);
	int ret = 0;

	if (nvavp->refcount) {
		if (!nvavp->clk_enabled) {
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
			if (nvavp_check_idle(nvavp, NVAVP_AUDIO_CHANNEL))
				nvavp_uninit(nvavp);
			else
				ret = -EBUSY;
#else
			nvavp_uninit(nvavp);
#endif
		}
		else {
			ret = -EBUSY;
		}
	}

	return ret;
}

static int tegra_nvavp_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvavp_info *nvavp = platform_get_drvdata(pdev);

	if (nvavp->video_refcnt)
		nvavp_init(nvavp, NVAVP_VIDEO_CHANNEL);
#if defined(CONFIG_TEGRA_NVAVP_AUDIO)
	if (nvavp->audio_refcnt)
		nvavp_init(nvavp, NVAVP_AUDIO_CHANNEL);
#endif

	return 0;
}

static int tegra_nvavp_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvavp_info *nvavp = platform_get_drvdata(pdev);

	mutex_lock(&nvavp->open_lock);

	tegra_nvavp_runtime_resume(dev);

	mutex_unlock(&nvavp->open_lock);

	return 0;
}

static int tegra_nvavp_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvavp_info *nvavp = platform_get_drvdata(pdev);

	mutex_lock(&nvavp->open_lock);

	tegra_nvavp_runtime_suspend(dev);

	/* Partition vde has to be left on before suspend for the
	 * device to wakeup on resume
	 */
	nvavp_unpowergate_vde(nvavp);

	mutex_unlock(&nvavp->open_lock);

	return 0;
}

static const struct dev_pm_ops nvavp_pm_ops = {
	.runtime_suspend = tegra_nvavp_runtime_suspend,
	.runtime_resume = tegra_nvavp_runtime_resume,
	.suspend = tegra_nvavp_suspend,
	.resume = tegra_nvavp_resume,
};

#define NVAVP_PM_OPS	(&nvavp_pm_ops)

#else /* CONFIG_PM */

#define NVAVP_PM_OPS	NULL

#endif /* CONFIG_PM */

static struct platform_driver tegra_nvavp_driver = {
	.driver	= {
		.name	= TEGRA_NVAVP_NAME,
		.owner	= THIS_MODULE,
		.pm	= NVAVP_PM_OPS,
	},
	.probe		= tegra_nvavp_probe,
	.remove		= tegra_nvavp_remove,
};

static int __init tegra_nvavp_init(void)
{
	return platform_driver_register(&tegra_nvavp_driver);
}

static void __exit tegra_nvavp_exit(void)
{
	platform_driver_unregister(&tegra_nvavp_driver);
}

module_init(tegra_nvavp_init);
module_exit(tegra_nvavp_exit);

MODULE_AUTHOR("NVIDIA");
MODULE_DESCRIPTION("Channel based AVP driver for Tegra");
MODULE_VERSION("1.0");
MODULE_LICENSE("Dual BSD/GPL");

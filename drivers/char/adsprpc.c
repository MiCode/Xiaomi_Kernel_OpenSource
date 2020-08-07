// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/msm_ion.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/rpmsg.h>
#include <linux/ipc_logging.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/service-notifier.h>
#include <soc/qcom/service-locator.h>
#include <linux/scatterlist.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/iommu.h>
#include <linux/sort.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <asm/dma-iommu.h>
#include "adsprpc_compat.h"
#include "adsprpc_shared.h"
#include <soc/qcom/ramdump.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/pm_qos.h>
#include <linux/stat.h>
#include <linux/cpumask.h>

#define CREATE_TRACE_POINTS
#include <trace/events/fastrpc.h>

#define TZ_PIL_PROTECT_MEM_SUBSYS_ID 0x0C
#define TZ_PIL_CLEAR_PROTECT_MEM_SUBSYS_ID 0x0D
#define TZ_PIL_AUTH_QDSP6_PROC 1
#define ADSP_MMAP_HEAP_ADDR 4
#define ADSP_MMAP_REMOTE_HEAP_ADDR 8
#define ADSP_MMAP_ADD_PAGES 0x1000
#define ADSP_MMAP_ADD_PAGES_LLC  0x3000
#define FASTRPC_DMAHANDLE_NOMAP (16)

#define FASTRPC_ENOSUCH 39
#define VMID_SSC_Q6     5
#define VMID_ADSP_Q6    6
#define DEBUGFS_SIZE 3072
#define UL_SIZE 25
#define PID_SIZE 10

#define AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME   "audio_pdr_adsprpc"
#define AUDIO_PDR_ADSP_SERVICE_NAME              "avs/audio"
#define ADSP_AUDIOPD_NAME                        "msm/adsp/audio_pd"

#define SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME   "sensors_pdr_adsprpc"
#define SENSORS_PDR_ADSP_SERVICE_NAME              "tms/servreg"
#define ADSP_SENSORPD_NAME                       "msm/adsp/sensor_pd"

#define SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME "sensors_pdr_sdsprpc"
#define SENSORS_PDR_SLPI_SERVICE_NAME            SENSORS_PDR_ADSP_SERVICE_NAME
#define SLPI_SENSORPD_NAME                       "msm/slpi/sensor_pd"

#define RPC_TIMEOUT	(5 * HZ)
#define BALIGN		128
#define NUM_CHANNELS	4	/* adsp, mdsp, slpi, cdsp*/
#define NUM_SESSIONS	9	/*8 compute, 1 cpz*/
#define M_FDLIST	(16)
#define M_CRCLIST	(64)
#define SESSION_ID_INDEX (30)
#define FASTRPC_CTX_MAGIC (0xbeeddeed)
#define FASTRPC_CTX_MAX (256)
#define FASTRPC_CTXID_MASK (0xFF0)
#define NUM_DEVICES   2 /* adsprpc-smd, adsprpc-smd-secure */
#define MINOR_NUM_DEV 0
#define MINOR_NUM_SECURE_DEV 1
#define NON_SECURE_CHANNEL 0
#define SECURE_CHANNEL 1

#define IS_CACHE_ALIGNED(x) (((x) & ((L1_CACHE_BYTES)-1)) == 0)
#ifndef ION_FLAG_CACHED
#define ION_FLAG_CACHED (1)
#endif

#define ADSP_DOMAIN_ID (0)
#define MDSP_DOMAIN_ID (1)
#define SDSP_DOMAIN_ID (2)
#define CDSP_DOMAIN_ID (3)

/* ctxid of every message is OR-ed with fl->pd (0/1/2) before */
/* it is sent to DSP. So mask 2 LSBs to retrieve actual context */
#define CONTEXT_PD_CHECK (3)

#define RH_CID ADSP_DOMAIN_ID

#define PERF_KEYS \
	"count:flush:map:copy:rpmsg:getargs:putargs:invalidate:invoke"
#define FASTRPC_STATIC_HANDLE_PROCESS_GROUP (1)
#define FASTRPC_STATIC_HANDLE_DSP_UTILITIES (2)
#define FASTRPC_STATIC_HANDLE_LISTENER (3)
#define FASTRPC_STATIC_HANDLE_MAX (20)
#define FASTRPC_LATENCY_CTRL_ENB  (1)

/* Maximum PM timeout that can be voted through fastrpc*/
#define MAX_PM_TIMEOUT_MS 50

/* timeout in us for busy polling after early response from remote processor */
#define FASTRPC_POLL_TIME (4000)

/* timeout in us for polling without preempt */
#define FASTRPC_POLL_TIME_WITHOUT_PREEMPT (500)

/* timeout in us for polling completion signal after user early hint */
#define FASTRPC_USER_EARLY_HINT_TIMEOUT (500)

/* Early wake up poll completion number received from remote processor */
#define FASTRPC_EARLY_WAKEUP_POLL (0xabbccdde)

/* latency in us, early wake up signal used below this value */
#define FASTRPC_EARLY_WAKEUP_LATENCY (200)

/* response version number */
#define FASTRPC_RSP_VERSION2 (2)

/* CPU feature information to DSP */
#define FASTRPC_CPUINFO_DEFAULT (0)
#define FASTRPC_CPUINFO_EARLY_WAKEUP (1)

#define INIT_FILELEN_MAX (2*1024*1024)
#define INIT_MEMLEN_MAX  (8*1024*1024)
#define MAX_CACHE_BUF_SIZE (8*1024*1024)

#define PERF_END (void)0

#define PERF(enb, cnt, ff) \
	{\
		struct timespec startT = {0};\
		int64_t *counter = cnt;\
		if (enb && counter) {\
			getnstimeofday(&startT);\
		} \
		ff ;\
		if (enb && counter) {\
			*counter += getnstimediff(&startT);\
		} \
	}

#define GET_COUNTER(perf_ptr, offset)  \
	(perf_ptr != NULL ?\
		(((offset >= 0) && (offset < PERF_KEY_MAX)) ?\
			(int64_t *)(perf_ptr + offset)\
				: (int64_t *)NULL) : (int64_t *)NULL)

#define FASTRPC_GLINK_LOG_PAGES 8
#define LOG_FASTRPC_GLINK_MSG(ctx, x, ...)	\
	do {				\
		if (ctx)		\
			ipc_log_string(ctx, "%s (%d, %d): "x,	\
				current->comm, current->tgid, current->pid, \
				##__VA_ARGS__); \
	} while (0)

static int fastrpc_pdr_notifier_cb(struct notifier_block *nb,
					unsigned long code,
					void *data);
static struct dentry *debugfs_root;
static struct dentry *debugfs_global_file;

static inline void mem_barrier(void)
{
	__asm__ __volatile__("dmb sy":::"memory");
}

static inline uint64_t buf_page_start(uint64_t buf)
{
	uint64_t start = (uint64_t) buf & PAGE_MASK;
	return start;
}

static inline uint64_t buf_page_offset(uint64_t buf)
{
	uint64_t offset = (uint64_t) buf & (PAGE_SIZE - 1);
	return offset;
}

static inline uint64_t buf_num_pages(uint64_t buf, size_t len)
{
	uint64_t start = buf_page_start(buf) >> PAGE_SHIFT;
	uint64_t end = (((uint64_t) buf + len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	uint64_t nPages = end - start + 1;
	return nPages;
}

static inline uint64_t buf_page_size(uint32_t size)
{
	uint64_t sz = (size + (PAGE_SIZE - 1)) & PAGE_MASK;

	return sz > PAGE_SIZE ? sz : PAGE_SIZE;
}

static inline void *uint64_to_ptr(uint64_t addr)
{
	void *ptr = (void *)((uintptr_t)addr);

	return ptr;
}

static inline uint64_t ptr_to_uint64(void *ptr)
{
	uint64_t addr = (uint64_t)((uintptr_t)ptr);

	return addr;
}

struct secure_vm {
	int *vmid;
	int *vmperm;
	int vmcount;
};

struct qos_cores {
	int *coreno;
	int corecount;
};

struct fastrpc_file;

struct fastrpc_buf {
	struct hlist_node hn;
	struct hlist_node hn_rem;
	struct fastrpc_file *fl;
	void *virt;
	uint64_t phys;
	size_t size;
	unsigned long dma_attr;
	uintptr_t raddr;
	uint32_t flags;
	int remote;
};

struct fastrpc_ctx_lst;

struct overlap {
	uintptr_t start;
	uintptr_t end;
	int raix;
	uintptr_t mstart;
	uintptr_t mend;
	uintptr_t offset;
	int do_cmo;		/*used for cache maintenance of inrout buffers*/
};

struct smq_invoke_ctx {
	struct hlist_node hn;
	struct completion work;
	int retval;
	int pid;
	int tgid;
	remote_arg_t *lpra;
	remote_arg64_t *rpra;
	remote_arg64_t *lrpra;		/* Local copy of rpra for put_args */
	int *fds;
	unsigned int *attrs;
	struct fastrpc_mmap **maps;
	struct fastrpc_buf *buf;
	size_t used;
	struct fastrpc_file *fl;
	uint32_t handle;
	uint32_t sc;
	struct overlap *overs;
	struct overlap **overps;
	struct smq_msg msg;
	uint32_t *crc;
	unsigned int magic;
	uint64_t ctxid;
	/* response flags from remote processor */
	enum fastrpc_response_flags rspFlags;
	/* user hint of completion time in us */
	uint32_t earlyWakeTime;
	/* work done status flag */
	bool isWorkDone;
};

struct fastrpc_ctx_lst {
	struct hlist_head pending;
	struct hlist_head interrupted;
};

struct fastrpc_smmu {
	struct device *dev;
	const char *dev_name;
	int cb;
	int enabled;
	int faults;
	int secure;
	int coherent;
};

struct fastrpc_session_ctx {
	struct device *dev;
	struct fastrpc_smmu smmu;
	int used;
};

struct fastrpc_static_pd {
	char *servloc_name;
	char *spdname;
	struct notifier_block pdrnb;
	struct notifier_block get_service_nb;
	void *pdrhandle;
	uint64_t pdrcount;
	uint64_t prevpdrcount;
	int ispdup;
	int cid;
};

struct fastrpc_dsp_capabilities {
	uint32_t is_cached;	//! Flag if dsp attributes are cached
	uint32_t dsp_attributes[FASTRPC_MAX_DSP_ATTRIBUTES];
};

struct fastrpc_channel_ctx {
	char *name;
	char *subsys;
	struct rpmsg_device *rpdev;
	struct device *dev;
	struct fastrpc_session_ctx session[NUM_SESSIONS];
	struct fastrpc_static_pd spd[NUM_SESSIONS];
	struct completion work;
	struct completion workport;
	struct notifier_block nb;
	struct mutex smd_mutex;
	struct mutex rpmsg_mutex;
	uint64_t sesscount;
	uint64_t ssrcount;
	void *handle;
	uint64_t prevssrcount;
	int issubsystemup;
	int vmid;
	struct secure_vm rhvm;
	int ramdumpenabled;
	void *rh_dump_dev;
	/* Indicates, if channel is restricted to secure node only */
	int secure;
	struct fastrpc_dsp_capabilities dsp_cap_kernel;
	void *ipc_log_ctx;
	/* cpu capabilities shared to DSP */
	uint64_t cpuinfo_todsp;
	bool cpuinfo_status;
	struct smq_invoke_ctx *ctxtable[FASTRPC_CTX_MAX];
	spinlock_t ctxlock;
};

struct fastrpc_apps {
	struct fastrpc_channel_ctx *channel;
	struct cdev cdev;
	struct class *class;
	struct smq_phy_page range;
	struct hlist_head maps;
	uint32_t staticpd_flags;
	dev_t dev_no;
	int compat;
	struct hlist_head drivers;
	spinlock_t hlock;
	struct device *dev;
	unsigned int latency;
	int rpmsg_register;
	bool legacy_remote_heap;
	/* Unique job id for each message */
	uint64_t jobid[NUM_CHANNELS];
	struct wakeup_source *wake_source;
	struct qos_cores silvercores;
	uint32_t max_size_limit;
};

struct fastrpc_mmap {
	struct hlist_node hn;
	struct fastrpc_file *fl;
	struct fastrpc_apps *apps;
	int fd;
	uint32_t flags;
	struct dma_buf *buf;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct ion_handle *handle;
	uint64_t phys;
	size_t size;
	uintptr_t va;
	size_t len;
	int refs;
	uintptr_t raddr;
	int uncached;
	int secure;
	uintptr_t attr;
};

enum fastrpc_perfkeys {
	PERF_COUNT = 0,
	PERF_FLUSH = 1,
	PERF_MAP = 2,
	PERF_COPY = 3,
	PERF_LINK = 4,
	PERF_GETARGS = 5,
	PERF_PUTARGS = 6,
	PERF_INVARGS = 7,
	PERF_INVOKE = 8,
	PERF_KEY_MAX = 9,
};

struct fastrpc_perf {
	int64_t count;
	int64_t flush;
	int64_t map;
	int64_t copy;
	int64_t link;
	int64_t getargs;
	int64_t putargs;
	int64_t invargs;
	int64_t invoke;
	int64_t tid;
	struct hlist_node hn;
};

struct fastrpc_file {
	struct hlist_node hn;
	spinlock_t hlock;
	struct hlist_head maps;
	struct hlist_head cached_bufs;
	struct hlist_head remote_bufs;
	struct fastrpc_ctx_lst clst;
	struct fastrpc_session_ctx *sctx;
	struct fastrpc_buf *init_mem;
	struct fastrpc_session_ctx *secsctx;
	uint32_t mode;
	uint32_t profile;
	int sessionid;
	int tgid;
	int cid;
	uint64_t ssrcount;
	int pd;
	char *servloc_name;
	int file_close;
	int dsp_proc_init;
	struct fastrpc_apps *apps;
	struct hlist_head perf;
	struct dentry *debugfs_file;
	struct mutex perf_mutex;
	struct pm_qos_request pm_qos_req;
	int qos_request;
	struct mutex map_mutex;
	struct mutex internal_map_mutex;
	/* Identifies the device (MINOR_NUM_DEV / MINOR_NUM_SECURE_DEV) */
	int dev_minor;
	char *debug_buf;
	/* Flag to enable PM wake/relax voting for every remote invoke */
	int wake_enable;
	struct wakeup_source *wake_source;
	uint32_t ws_timeout;
};

static struct fastrpc_apps gfa;

static struct fastrpc_channel_ctx gcinfo[NUM_CHANNELS] = {
	{
		.name = "adsprpc-smd",
		.subsys = "adsp",
		.spd = {
			{
				.servloc_name =
					AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME,
				.spdname = ADSP_AUDIOPD_NAME,
				.pdrnb.notifier_call =
						fastrpc_pdr_notifier_cb,
				.cid = ADSP_DOMAIN_ID,
			},
			{
				.servloc_name =
				SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME,
				.spdname = ADSP_SENSORPD_NAME,
				.pdrnb.notifier_call =
						fastrpc_pdr_notifier_cb,
				.cid = ADSP_DOMAIN_ID,
			}
		},
		.cpuinfo_todsp = FASTRPC_CPUINFO_DEFAULT,
		.cpuinfo_status = false,
	},
	{
		.name = "mdsprpc-smd",
		.subsys = "modem",
		.spd = {
			{
				.cid = MDSP_DOMAIN_ID,
			}
		},
		.cpuinfo_todsp = FASTRPC_CPUINFO_DEFAULT,
		.cpuinfo_status = false,
	},
	{
		.name = "sdsprpc-smd",
		.subsys = "slpi",
		.spd = {
			{
				.servloc_name =
				SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME,
				.spdname = SLPI_SENSORPD_NAME,
				.pdrnb.notifier_call =
						fastrpc_pdr_notifier_cb,
				.cid = SDSP_DOMAIN_ID,
			}
		},
		.cpuinfo_todsp = FASTRPC_CPUINFO_DEFAULT,
		.cpuinfo_status = false,
	},
	{
		.name = "cdsprpc-smd",
		.subsys = "cdsp",
		.spd = {
			{
				.cid = CDSP_DOMAIN_ID,
			}
		},
		.cpuinfo_todsp = FASTRPC_CPUINFO_EARLY_WAKEUP,
		.cpuinfo_status = false,
	},
};

static int hlosvm[1] = {VMID_HLOS};
static int hlosvmperm[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};

static void fastrpc_pm_awake(struct fastrpc_file *fl);

static inline int64_t getnstimediff(struct timespec *start)
{
	int64_t ns;
	struct timespec ts, b;

	getnstimeofday(&ts);
	b = timespec_sub(ts, *start);
	ns = timespec_to_ns(&b);
	return ns;
}

static inline int64_t *getperfcounter(struct fastrpc_file *fl, int key)
{
	int err = 0;
	int64_t *val = NULL;
	struct fastrpc_perf *perf = NULL, *fperf = NULL;
	struct hlist_node *n = NULL;

	VERIFY(err, !IS_ERR_OR_NULL(fl));
	if (err)
		goto bail;

	mutex_lock(&fl->perf_mutex);
	hlist_for_each_entry_safe(perf, n, &fl->perf, hn) {
		if (perf->tid == current->pid) {
			fperf = perf;
			break;
		}
	}

	if (IS_ERR_OR_NULL(fperf)) {
		fperf = kzalloc(sizeof(*fperf), GFP_KERNEL);

		VERIFY(err, !IS_ERR_OR_NULL(fperf));
		if (err) {
			mutex_unlock(&fl->perf_mutex);
			kfree(fperf);
			goto bail;
		}

		fperf->tid = current->pid;
		hlist_add_head(&fperf->hn, &fl->perf);
	}

	val = ((int64_t *)fperf) + key;
	mutex_unlock(&fl->perf_mutex);
bail:
	return val;
}

static inline int poll_on_early_response(struct smq_invoke_ctx *ctx)
{
	int ii, jj, err = -EIO;
	uint32_t sc = ctx->sc;
	struct smq_invoke_buf *list;
	struct smq_phy_page *pages;
	uint64_t *fdlist;
	uint32_t *crclist, *poll;
	unsigned int inbufs, outbufs, handles;

	/* calculate poll memory location */
	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	handles = REMOTE_SCALARS_INHANDLES(sc) + REMOTE_SCALARS_OUTHANDLES(sc);
	list = smq_invoke_buf_start(ctx->rpra, sc);
	pages = smq_phy_page_start(sc, list);
	fdlist = (uint64_t *)(pages + inbufs + outbufs + handles);
	crclist = (uint32_t *)(fdlist + M_FDLIST);
	poll = (uint32_t *)(crclist + M_CRCLIST);

	/*
	 * poll on memory for actual completion after receiving
	 * early response from DSP. Return failure on timeout.
	 */
	preempt_disable();
	for (ii = 0, jj = 0; ii < FASTRPC_POLL_TIME; ii++, jj++) {
		if (*poll == FASTRPC_EARLY_WAKEUP_POLL) {
			err = 0;
			break;
		}
		if (jj == FASTRPC_POLL_TIME_WITHOUT_PREEMPT) {
			/* limit preempt disable time with no rescheduling */
			preempt_enable();
			mem_barrier();
			preempt_disable();
			jj = 0;
		}
		udelay(1);
	}
	preempt_enable_no_resched();
	return err;
}

static void fastrpc_buf_free(struct fastrpc_buf *buf, int cache)
{
	struct fastrpc_file *fl = buf == NULL ? NULL : buf->fl;
	int vmid;

	if (!fl)
		return;
	if (cache && buf->size < MAX_CACHE_BUF_SIZE) {
		spin_lock(&fl->hlock);
		hlist_add_head(&buf->hn, &fl->cached_bufs);
		spin_unlock(&fl->hlock);
		return;
	}
	if (buf->remote) {
		spin_lock(&fl->hlock);
		hlist_del_init(&buf->hn_rem);
		spin_unlock(&fl->hlock);
		buf->remote = 0;
		buf->raddr = 0;
	}
	if (!IS_ERR_OR_NULL(buf->virt)) {
		int destVM[1] = {VMID_HLOS};
		int destVMperm[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};

		if (fl->sctx->smmu.cb)
			buf->phys &= ~((uint64_t)fl->sctx->smmu.cb << 32);
		vmid = fl->apps->channel[fl->cid].vmid;
		if (vmid) {
			int srcVM[2] = {VMID_HLOS, vmid};

			hyp_assign_phys(buf->phys, buf_page_size(buf->size),
				srcVM, 2, destVM, destVMperm, 1);
		}
		trace_fastrpc_dma_free(fl->cid, buf->phys, buf->size);
		dma_free_attrs(fl->sctx->smmu.dev, buf->size, buf->virt,
					buf->phys, buf->dma_attr);
	}
	kfree(buf);
}

static void fastrpc_cached_buf_list_free(struct fastrpc_file *fl)
{
	struct fastrpc_buf *buf, *free;

	do {
		struct hlist_node *n;

		free = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			hlist_del_init(&buf->hn);
			free = buf;
			break;
		}
		spin_unlock(&fl->hlock);
		if (free)
			fastrpc_buf_free(free, 0);
	} while (free);
}

static void fastrpc_remote_buf_list_free(struct fastrpc_file *fl)
{
	struct fastrpc_buf *buf, *free;

	do {
		struct hlist_node *n;

		free = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->remote_bufs, hn_rem) {
			free = buf;
			break;
		}
		spin_unlock(&fl->hlock);
		if (free)
			fastrpc_buf_free(free, 0);
	} while (free);
}

static void fastrpc_mmap_add(struct fastrpc_mmap *map)
{
	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		struct fastrpc_apps *me = &gfa;

		spin_lock(&me->hlock);
		hlist_add_head(&map->hn, &me->maps);
		spin_unlock(&me->hlock);
	} else {
		struct fastrpc_file *fl = map->fl;

		hlist_add_head(&map->hn, &fl->maps);
	}
}

static int fastrpc_mmap_find(struct fastrpc_file *fl, int fd,
		uintptr_t va, size_t len, int mflags, int refs,
		struct fastrpc_mmap **ppmap)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_mmap *match = NULL, *map = NULL;
	struct hlist_node *n;

	if ((va + len) < va)
		return -EOVERFLOW;
	if (mflags == ADSP_MMAP_HEAP_ADDR ||
				 mflags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		spin_lock(&me->hlock);
		hlist_for_each_entry_safe(map, n, &me->maps, hn) {
			if (va >= map->va &&
				va + len <= map->va + map->len &&
				map->fd == fd) {
				if (refs) {
					if (map->refs + 1 == INT_MAX) {
						spin_unlock(&me->hlock);
						return -ETOOMANYREFS;
					}
					map->refs++;
				}
				match = map;
				break;
			}
		}
		spin_unlock(&me->hlock);
	} else {
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			if (va >= map->va &&
				va + len <= map->va + map->len &&
				map->fd == fd) {
				if (refs) {
					if (map->refs + 1 == INT_MAX)
						return -ETOOMANYREFS;
					map->refs++;
				}
				match = map;
				break;
			}
		}
	}
	if (match) {
		*ppmap = match;
		return 0;
	}
	return -ENOTTY;
}

static int dma_alloc_memory(dma_addr_t *region_phys, void **vaddr, size_t size,
					unsigned long dma_attr)
{
	int err = 0;
	struct fastrpc_apps *me = &gfa;

	if (me->dev == NULL) {
		pr_err("device adsprpc-mem is not initialized\n");
		return -ENODEV;
	}
	VERIFY(err, size > 0 && size < me->max_size_limit);
	if (err) {
		err = -EFAULT;
		pr_err("adsprpc: %s: invalid allocation size 0x%zx\n",
			__func__, size);
		return err;
	}
	*vaddr = dma_alloc_attrs(me->dev, size, region_phys,
					GFP_KERNEL, dma_attr);
	if (IS_ERR_OR_NULL(*vaddr)) {
		pr_err("adsprpc: %s: %s: dma_alloc_attrs failed for size 0x%zx, returned %pK\n",
				current->comm, __func__, size, (*vaddr));
		return -ENOMEM;
	}
	return 0;
}

static int fastrpc_mmap_remove(struct fastrpc_file *fl, uintptr_t va,
			       size_t len, struct fastrpc_mmap **ppmap)
{
	struct fastrpc_mmap *match = NULL, *map;
	struct hlist_node *n;
	struct fastrpc_apps *me = &gfa;

	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(map, n, &me->maps, hn) {
		if (map->raddr == va &&
			map->raddr + map->len == va + len &&
			map->refs == 1) {
			match = map;
			hlist_del_init(&map->hn);
			break;
		}
	}
	spin_unlock(&me->hlock);
	if (match) {
		*ppmap = match;
		return 0;
	}
	hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
		if (map->raddr == va &&
			map->raddr + map->len == va + len &&
			map->refs == 1) {
			match = map;
			hlist_del_init(&map->hn);
			break;
		}
	}
	if (match) {
		*ppmap = match;
		return 0;
	}
	return -ENOTTY;
}

static void fastrpc_mmap_free(struct fastrpc_mmap *map, uint32_t flags)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_file *fl;
	int vmid, cid = -1, err = 0;
	struct fastrpc_session_ctx *sess;

	if (!map)
		return;
	fl = map->fl;
	if (fl && !(map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR)) {
		cid = fl->cid;
		VERIFY(err, cid >= ADSP_DOMAIN_ID && cid < NUM_CHANNELS);
		if (err) {
			err = -ECHRNG;
			pr_err("adsprpc: ERROR:%s, Invalid channel id: %d, err:%d\n",
				__func__, cid, err);
			return;
		}
	}
	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		map->refs--;
		if (!map->refs)
			hlist_del_init(&map->hn);
		if (map->refs > 0)
			return;
	} else {
		map->refs--;
		if (!map->refs)
			hlist_del_init(&map->hn);
		if (map->refs > 0 && !flags)
			return;
	}
	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {

		if (me->dev == NULL) {
			pr_err("adsprpc: %s: %s: failed to free remote heap allocation\n",
				current->comm, __func__);
			return;
		}
		trace_fastrpc_dma_free(-1, map->phys, map->size);
		if (map->phys) {
			dma_free_attrs(me->dev, map->size, (void *)map->va,
			(dma_addr_t)map->phys, (unsigned long)map->attr);
		}
	} else if (map->flags == FASTRPC_DMAHANDLE_NOMAP) {
		trace_fastrpc_dma_unmap(fl->cid, map->phys, map->size);
		if (!IS_ERR_OR_NULL(map->table))
			dma_buf_unmap_attachment(map->attach, map->table,
					DMA_BIDIRECTIONAL);
		if (!IS_ERR_OR_NULL(map->attach))
			dma_buf_detach(map->buf, map->attach);
		if (!IS_ERR_OR_NULL(map->buf))
			dma_buf_put(map->buf);
	} else {
		int destVM[1] = {VMID_HLOS};
		int destVMperm[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};

		if (map->secure)
			sess = fl->secsctx;
		else
			sess = fl->sctx;

		vmid = fl->apps->channel[fl->cid].vmid;
		if (vmid && map->phys) {
			int srcVM[2] = {VMID_HLOS, vmid};

			hyp_assign_phys(map->phys, buf_page_size(map->size),
				srcVM, 2, destVM, destVMperm, 1);
		}
		trace_fastrpc_dma_unmap(fl->cid, map->phys, map->size);
		if (!IS_ERR_OR_NULL(map->table))
			dma_buf_unmap_attachment(map->attach, map->table,
					DMA_BIDIRECTIONAL);
		if (!IS_ERR_OR_NULL(map->attach))
			dma_buf_detach(map->buf, map->attach);
		if (!IS_ERR_OR_NULL(map->buf))
			dma_buf_put(map->buf);
	}
	kfree(map);
}

static int fastrpc_session_alloc(struct fastrpc_channel_ctx *chan, int secure,
					struct fastrpc_session_ctx **session);

static int fastrpc_mmap_create(struct fastrpc_file *fl, int fd,
	unsigned int attr, uintptr_t va, size_t len, int mflags,
	struct fastrpc_mmap **ppmap)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_session_ctx *sess;
	struct fastrpc_apps *apps = fl->apps;
	int cid = fl->cid;
	struct fastrpc_channel_ctx *chan = NULL;
	struct fastrpc_mmap *map = NULL;
	dma_addr_t region_phys = 0;
	void *region_vaddr = NULL;
	unsigned long flags;
	int err = 0, vmid, sgl_index = 0;
	struct scatterlist *sgl = NULL;

	VERIFY(err, cid >= ADSP_DOMAIN_ID && cid < NUM_CHANNELS);
	if (err)
		goto bail;
	chan = &apps->channel[cid];

	if (!fastrpc_mmap_find(fl, fd, va, len, mflags, 1, ppmap))
		return 0;
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	VERIFY(err, !IS_ERR_OR_NULL(map));
	if (err)
		goto bail;
	INIT_HLIST_NODE(&map->hn);
	map->flags = mflags;
	map->refs = 1;
	map->fl = fl;
	map->fd = fd;
	map->attr = attr;
	if (mflags == ADSP_MMAP_HEAP_ADDR ||
				mflags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		map->apps = me;
		map->fl = NULL;
		map->attr |= DMA_ATTR_SKIP_ZEROING | DMA_ATTR_NO_KERNEL_MAPPING;
		VERIFY(err, !dma_alloc_memory(&region_phys, &region_vaddr,
					len, (unsigned long) map->attr));
		if (err)
			goto bail;
		trace_fastrpc_dma_alloc(fl->cid, (uint64_t)region_phys, len,
			(unsigned long)map->attr, mflags);
		map->phys = (uintptr_t)region_phys;
		map->size = len;
		map->va = (uintptr_t)region_vaddr;
	} else if (mflags == FASTRPC_DMAHANDLE_NOMAP) {
		VERIFY(err, !IS_ERR_OR_NULL(map->buf = dma_buf_get(fd)));
		if (err)
			goto bail;
		VERIFY(err, !dma_buf_get_flags(map->buf, &flags));
		if (err)
			goto bail;
		map->secure = flags & ION_FLAG_SECURE;
		map->uncached = 1;
		map->va = 0;
		map->phys = 0;

		VERIFY(err, !IS_ERR_OR_NULL(map->attach =
				dma_buf_attach(map->buf, me->dev)));
		if (err)
			goto bail;

		map->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
		VERIFY(err, !IS_ERR_OR_NULL(map->table =
			dma_buf_map_attachment(map->attach,
				DMA_BIDIRECTIONAL)));
		if (err)
			goto bail;
		VERIFY(err, map->table->nents == 1);
		if (err)
			goto bail;
		map->phys = sg_dma_address(map->table->sgl);
		map->size = len;
		trace_fastrpc_dma_map(fl->cid, fd, map->phys, map->size,
			len, mflags, map->attach->dma_map_attrs);
	} else {
		if (map->attr && (map->attr & FASTRPC_ATTR_KEEP_MAP)) {
			pr_info("adsprpc: %s: buffer mapped with persist attr 0x%x\n",
				__func__, (unsigned int)map->attr);
			map->refs = 2;
		}
		VERIFY(err, !IS_ERR_OR_NULL(map->buf = dma_buf_get(fd)));
		if (err)
			goto bail;
		VERIFY(err, !dma_buf_get_flags(map->buf, &flags));
		if (err)
			goto bail;

		map->secure = flags & ION_FLAG_SECURE;
		if (map->secure) {
			if (!fl->secsctx)
				err = fastrpc_session_alloc(chan, 1,
							&fl->secsctx);
			if (err)
				goto bail;
		}
		if (map->secure)
			sess = fl->secsctx;
		else
			sess = fl->sctx;

		VERIFY(err, !IS_ERR_OR_NULL(sess));
		if (err)
			goto bail;

		map->uncached = !(flags & ION_FLAG_CACHED);
		if (map->attr & FASTRPC_ATTR_NOVA && !sess->smmu.coherent)
			map->uncached = 1;

		VERIFY(err, !IS_ERR_OR_NULL(map->attach =
				dma_buf_attach(map->buf, sess->smmu.dev)));
		if (err)
			goto bail;

		map->attach->dma_map_attrs |= DMA_ATTR_DELAYED_UNMAP;
		map->attach->dma_map_attrs |= DMA_ATTR_EXEC_MAPPING;
		if (map->attr & FASTRPC_ATTR_NON_COHERENT ||
			(sess->smmu.coherent && map->uncached))
			map->attach->dma_map_attrs |=
				DMA_ATTR_FORCE_NON_COHERENT |
				DMA_ATTR_SKIP_CPU_SYNC;
		else if (map->attr & FASTRPC_ATTR_COHERENT)
			map->attach->dma_map_attrs |= DMA_ATTR_FORCE_COHERENT;
		/*
		 * Skip CPU sync if IO Cohernecy is not supported
		 * as we flush later
		 */
		else if (!sess->smmu.coherent)
			map->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

		VERIFY(err, !IS_ERR_OR_NULL(map->table =
			dma_buf_map_attachment(map->attach,
				DMA_BIDIRECTIONAL)));
		if (err)
			goto bail;
		if (!sess->smmu.enabled) {
			VERIFY(err, map->table->nents == 1);
			if (err)
				goto bail;
		}
		map->phys = sg_dma_address(map->table->sgl);

		if (sess->smmu.cb) {
			map->phys += ((uint64_t)sess->smmu.cb << 32);
			for_each_sg(map->table->sgl, sgl, map->table->nents,
				sgl_index)
				map->size += sg_dma_len(sgl);
		} else {
			map->size = buf_page_size(len);
		}
		trace_fastrpc_dma_map(fl->cid, fd, map->phys, map->size,
			len, mflags, map->attach->dma_map_attrs);
		if (map->size < len) {
			err = -EFAULT;
			goto bail;
		}

		VERIFY(err, map->size >= len && map->size < me->max_size_limit);
		if (err) {
			err = -EFAULT;
			pr_err("adsprpc: %s: invalid map size 0x%zx len 0x%zx\n",
				__func__, map->size, len);
			goto bail;
		}

		vmid = fl->apps->channel[fl->cid].vmid;
		if (!sess->smmu.enabled && !vmid) {
			VERIFY(err, map->phys >= me->range.addr &&
			map->phys + map->size <=
			me->range.addr + me->range.size);
			if (err) {
				pr_err("adsprpc: %s: phys addr 0x%llx (size 0x%zx) out of CMA heap range\n",
					__func__, map->phys, map->size);
				goto bail;
			}
		}
		if (vmid) {
			int srcVM[1] = {VMID_HLOS};
			int destVM[2] = {VMID_HLOS, vmid};
			int destVMperm[2] = {PERM_READ | PERM_WRITE,
					PERM_READ | PERM_WRITE | PERM_EXEC};

			err = hyp_assign_phys(map->phys,
					buf_page_size(map->size),
					srcVM, 1, destVM, destVMperm, 2);
			if (err)
				goto bail;
		}
		map->va = va;
	}
	map->len = len;

	fastrpc_mmap_add(map);
	*ppmap = map;

bail:
	if (err && map)
		fastrpc_mmap_free(map, 0);
	return err;
}

static int fastrpc_buf_alloc(struct fastrpc_file *fl, size_t size,
			unsigned long dma_attr, uint32_t rflags,
			int remote, struct fastrpc_buf **obuf)
{
	int err = 0, vmid;
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_buf *buf = NULL, *fr = NULL;
	struct hlist_node *n;

	VERIFY(err, size > 0 && size < me->max_size_limit);
	if (err) {
		err = -EFAULT;
		pr_err("adsprpc: %s: invalid allocation size 0x%zx\n",
			__func__, size);
		goto bail;
	}

	if (!remote) {
		/* find the smallest buffer that fits in the cache */
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			if (buf->size >= size && (!fr || fr->size > buf->size))
				fr = buf;
		}
		if (fr)
			hlist_del_init(&fr->hn);
		spin_unlock(&fl->hlock);
		if (fr) {
			*obuf = fr;
			return 0;
		}
	}

	buf = NULL;
	VERIFY(err, NULL != (buf = kzalloc(sizeof(*buf), GFP_KERNEL)));
	if (err)
		goto bail;
	INIT_HLIST_NODE(&buf->hn);
	buf->fl = fl;
	buf->virt = NULL;
	buf->phys = 0;
	buf->size = size;
	buf->dma_attr = dma_attr;
	buf->flags = rflags;
	buf->raddr = 0;
	buf->remote = 0;
	buf->virt = dma_alloc_attrs(fl->sctx->smmu.dev, buf->size,
						(dma_addr_t *)&buf->phys,
						GFP_KERNEL, buf->dma_attr);
	if (IS_ERR_OR_NULL(buf->virt)) {
		/* free cache and retry */
		fastrpc_cached_buf_list_free(fl);
		buf->virt = dma_alloc_attrs(fl->sctx->smmu.dev, buf->size,
					(dma_addr_t *)&buf->phys, GFP_KERNEL,
					buf->dma_attr);
		VERIFY(err, !IS_ERR_OR_NULL(buf->virt));
	}
	if (err) {
		err = ENOMEM;
		pr_err("adsprpc: %s: %s: dma_alloc_attrs failed for size 0x%zx, returned %pK\n",
			current->comm, __func__, size, buf->virt);
		goto bail;
	}
	if (fl->sctx->smmu.cb)
		buf->phys += ((uint64_t)fl->sctx->smmu.cb << 32);
	trace_fastrpc_dma_alloc(fl->cid, buf->phys, size,
		dma_attr, (int)rflags);

	vmid = fl->apps->channel[fl->cid].vmid;
	if (vmid) {
		int srcVM[1] = {VMID_HLOS};
		int destVM[2] = {VMID_HLOS, vmid};
		int destVMperm[2] = {PERM_READ | PERM_WRITE,
					PERM_READ | PERM_WRITE | PERM_EXEC};

		err = hyp_assign_phys(buf->phys, buf_page_size(size),
			srcVM, 1, destVM, destVMperm, 2);
		if (err)
			goto bail;
	}

	if (remote) {
		INIT_HLIST_NODE(&buf->hn_rem);
		spin_lock(&fl->hlock);
		hlist_add_head(&buf->hn_rem, &fl->remote_bufs);
		spin_unlock(&fl->hlock);
		buf->remote = remote;
	}
	*obuf = buf;
 bail:
	if (err && buf)
		fastrpc_buf_free(buf, 0);
	return err;
}


static int context_restore_interrupted(struct fastrpc_file *fl,
				       struct fastrpc_ioctl_invoke_crc *inv,
				       struct smq_invoke_ctx **po)
{
	int err = 0;
	struct smq_invoke_ctx *ctx = NULL, *ictx = NULL;
	struct hlist_node *n;
	struct fastrpc_ioctl_invoke *invoke = &inv->inv;

	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(ictx, n, &fl->clst.interrupted, hn) {
		if (ictx->pid == current->pid) {
			if (invoke->sc != ictx->sc || ictx->fl != fl)
				err = -1;
			else {
				ctx = ictx;
				hlist_del_init(&ctx->hn);
				hlist_add_head(&ctx->hn, &fl->clst.pending);
			}
			break;
		}
	}
	spin_unlock(&fl->hlock);
	if (ctx)
		*po = ctx;
	return err;
}

#define CMP(aa, bb) ((aa) == (bb) ? 0 : (aa) < (bb) ? -1 : 1)
static int overlap_ptr_cmp(const void *a, const void *b)
{
	struct overlap *pa = *((struct overlap **)a);
	struct overlap *pb = *((struct overlap **)b);
	/* sort with lowest starting buffer first */
	int st = CMP(pa->start, pb->start);
	/* sort with highest ending buffer first */
	int ed = CMP(pb->end, pa->end);
	return st == 0 ? ed : st;
}

static int context_build_overlap(struct smq_invoke_ctx *ctx)
{
	int i, err = 0;
	remote_arg_t *lpra = ctx->lpra;
	int inbufs = REMOTE_SCALARS_INBUFS(ctx->sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(ctx->sc);
	int nbufs = inbufs + outbufs;
	struct overlap max;

	for (i = 0; i < nbufs; ++i) {
		ctx->overs[i].start = (uintptr_t)lpra[i].buf.pv;
		ctx->overs[i].end = ctx->overs[i].start + lpra[i].buf.len;
		if (lpra[i].buf.len) {
			VERIFY(err, ctx->overs[i].end > ctx->overs[i].start);
			if (err)
				goto bail;
		}
		ctx->overs[i].raix = i;
		ctx->overps[i] = &ctx->overs[i];
	}
	sort(ctx->overps, nbufs, sizeof(*ctx->overps), overlap_ptr_cmp, NULL);
	max.start = 0;
	max.end = 0;
	for (i = 0; i < nbufs; ++i) {
		if (ctx->overps[i]->start < max.end) {
			ctx->overps[i]->mstart = max.end;
			ctx->overps[i]->mend = ctx->overps[i]->end;
			ctx->overps[i]->offset = max.end -
				ctx->overps[i]->start;
			if (ctx->overps[i]->end > max.end) {
				max.end = ctx->overps[i]->end;
			} else {
				if ((max.raix < inbufs &&
					ctx->overps[i]->raix + 1 > inbufs) ||
					(ctx->overps[i]->raix < inbufs &&
					max.raix + 1 > inbufs))
					ctx->overps[i]->do_cmo = 1;
				ctx->overps[i]->mend = 0;
				ctx->overps[i]->mstart = 0;
			}
		} else  {
			ctx->overps[i]->mend = ctx->overps[i]->end;
			ctx->overps[i]->mstart = ctx->overps[i]->start;
			ctx->overps[i]->offset = 0;
			max = *ctx->overps[i];
		}
	}
bail:
	return err;
}

#define K_COPY_FROM_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_from_user((dst),\
			(void const __user *)(src),\
							(size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

#define K_COPY_TO_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_to_user((void __user *)(dst),\
						(src), (size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)


static void context_free(struct smq_invoke_ctx *ctx);

static int context_alloc(struct fastrpc_file *fl, uint32_t kernel,
			 struct fastrpc_ioctl_invoke_crc *invokefd,
			 struct smq_invoke_ctx **po)
{
	struct fastrpc_apps *me = &gfa;
	int err = 0, bufs, ii, size = 0, cid = -1;
	struct smq_invoke_ctx *ctx = NULL;
	struct fastrpc_ctx_lst *clst = &fl->clst;
	struct fastrpc_ioctl_invoke *invoke = &invokefd->inv;
	struct fastrpc_channel_ctx *chan = 0;
	unsigned long irq_flags = 0;

	bufs = REMOTE_SCALARS_LENGTH(invoke->sc);
	size = bufs * sizeof(*ctx->lpra) + bufs * sizeof(*ctx->maps) +
		sizeof(*ctx->fds) * (bufs) +
		sizeof(*ctx->attrs) * (bufs) +
		sizeof(*ctx->overs) * (bufs) +
		sizeof(*ctx->overps) * (bufs);

	VERIFY(err, NULL != (ctx = kzalloc(sizeof(*ctx) + size, GFP_KERNEL)));
	if (err)
		goto bail;

	INIT_HLIST_NODE(&ctx->hn);
	hlist_add_fake(&ctx->hn);
	ctx->fl = fl;
	ctx->maps = (struct fastrpc_mmap **)(&ctx[1]);
	ctx->lpra = (remote_arg_t *)(&ctx->maps[bufs]);
	ctx->fds = (int *)(&ctx->lpra[bufs]);
	ctx->attrs = (unsigned int *)(&ctx->fds[bufs]);
	ctx->overs = (struct overlap *)(&ctx->attrs[bufs]);
	ctx->overps = (struct overlap **)(&ctx->overs[bufs]);

	K_COPY_FROM_USER(err, kernel, (void *)ctx->lpra, invoke->pra,
					bufs * sizeof(*ctx->lpra));
	if (err)
		goto bail;

	if (invokefd->fds) {
		K_COPY_FROM_USER(err, kernel, ctx->fds, invokefd->fds,
						bufs * sizeof(*ctx->fds));
		if (err)
			goto bail;
	} else {
		ctx->fds = NULL;
	}
	if (invokefd->attrs) {
		K_COPY_FROM_USER(err, kernel, ctx->attrs, invokefd->attrs,
						bufs * sizeof(*ctx->attrs));
		if (err)
			goto bail;
	}
	ctx->crc = (uint32_t *)invokefd->crc;
	ctx->handle = invoke->handle;
	ctx->sc = invoke->sc;
	if (bufs) {
		VERIFY(err, 0 == context_build_overlap(ctx));
		if (err)
			goto bail;
	}
	ctx->retval = 0xDECAF;
	ctx->pid = current->pid;
	ctx->tgid = fl->tgid;
	init_completion(&ctx->work);
	ctx->magic = FASTRPC_CTX_MAGIC;
	ctx->rspFlags = NORMAL_RESPONSE;
	ctx->isWorkDone = false;

	spin_lock(&fl->hlock);
	hlist_add_head(&ctx->hn, &clst->pending);
	cid = (fl->cid >= ADSP_DOMAIN_ID && fl->cid < NUM_CHANNELS)
			? fl->cid : 0;
	chan = &me->channel[cid];
	spin_unlock(&fl->hlock);

	spin_lock_irqsave(&chan->ctxlock, irq_flags);
	me->jobid[cid]++;
	for (ii = 0; ii < FASTRPC_CTX_MAX; ii++) {
		if (!chan->ctxtable[ii]) {
			chan->ctxtable[ii] = ctx;
			ctx->ctxid = (me->jobid[cid] << 12) | (ii << 4);
			break;
		}
	}
	spin_unlock_irqrestore(&chan->ctxlock, irq_flags);
	VERIFY(err, ii < FASTRPC_CTX_MAX);
	if (err) {
		pr_err("adsprpc: out of context memory\n");
		goto bail;
	}
	trace_fastrpc_context_alloc((uint64_t)ctx,
		ctx->ctxid | fl->pd, ctx->handle, ctx->sc);
	*po = ctx;
bail:
	if (ctx && err)
		context_free(ctx);
	return err;
}

static void context_save_interrupted(struct smq_invoke_ctx *ctx)
{
	struct fastrpc_ctx_lst *clst = &ctx->fl->clst;

	spin_lock(&ctx->fl->hlock);
	hlist_del_init(&ctx->hn);
	hlist_add_head(&ctx->hn, &clst->interrupted);
	spin_unlock(&ctx->fl->hlock);
}

static void context_free(struct smq_invoke_ctx *ctx)
{
	int i;
	struct fastrpc_apps *me = &gfa;
	int nbufs = REMOTE_SCALARS_INBUFS(ctx->sc) +
		    REMOTE_SCALARS_OUTBUFS(ctx->sc);
	int cid = ctx->fl->cid;
	struct fastrpc_channel_ctx *chan = &me->channel[cid];
	unsigned long irq_flags = 0;

	spin_lock_irqsave(&chan->ctxlock, irq_flags);
	for (i = 0; i < FASTRPC_CTX_MAX; i++) {
		if (chan->ctxtable[i] == ctx) {
			chan->ctxtable[i] = NULL;
			break;
		}
	}
	spin_unlock_irqrestore(&chan->ctxlock, irq_flags);

	spin_lock(&ctx->fl->hlock);
	hlist_del_init(&ctx->hn);
	spin_unlock(&ctx->fl->hlock);

	mutex_lock(&ctx->fl->map_mutex);
	for (i = 0; i < nbufs; ++i)
		fastrpc_mmap_free(ctx->maps[i], 0);
	mutex_unlock(&ctx->fl->map_mutex);

	fastrpc_buf_free(ctx->buf, 1);
	kfree(ctx->lrpra);
	ctx->lrpra = NULL;
	ctx->magic = 0;
	ctx->ctxid = 0;

	trace_fastrpc_context_free((uint64_t)ctx,
		ctx->msg.invoke.header.ctx, ctx->handle, ctx->sc);
	kfree(ctx);
}

static void context_notify_user(struct smq_invoke_ctx *ctx,
		int retval, uint32_t rspFlags, uint32_t earlyWakeTime)
{
	fastrpc_pm_awake(ctx->fl);
	ctx->retval = retval;
	switch (rspFlags) {
	case NORMAL_RESPONSE:
		/* normal response with return value */
		ctx->retval = retval;
		ctx->isWorkDone = true;
		break;
	case USER_EARLY_SIGNAL:
		/* user hint of approximate time of completion */
		ctx->earlyWakeTime = earlyWakeTime;
		break;
	case EARLY_RESPONSE:
		/* rpc framework early response with return value */
		ctx->retval = retval;
		break;
	case COMPLETE_SIGNAL:
		/* rpc framework signal to clear if pending on ctx */
		ctx->isWorkDone = true;
		break;
	default:
		break;
	}
	ctx->rspFlags = (enum fastrpc_response_flags)rspFlags;
	trace_fastrpc_context_complete(ctx->fl->cid, (uint64_t)ctx, retval,
		ctx->msg.invoke.header.ctx, ctx->handle, ctx->sc);
	complete(&ctx->work);
}

static void fastrpc_notify_users(struct fastrpc_file *me)
{
	struct smq_invoke_ctx *ictx;
	struct hlist_node *n;

	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(ictx, n, &me->clst.pending, hn) {
		ictx->isWorkDone = true;
		trace_fastrpc_context_complete(me->cid, (uint64_t)ictx,
			ictx->retval, ictx->msg.invoke.header.ctx,
			ictx->handle, ictx->sc);
		complete(&ictx->work);
	}
	hlist_for_each_entry_safe(ictx, n, &me->clst.interrupted, hn) {
		ictx->isWorkDone = true;
		trace_fastrpc_context_complete(me->cid, (uint64_t)ictx,
			ictx->retval, ictx->msg.invoke.header.ctx,
			ictx->handle, ictx->sc);
		complete(&ictx->work);
	}
	spin_unlock(&me->hlock);
}


static void fastrpc_notify_users_staticpd_pdr(struct fastrpc_file *me)
{
	struct smq_invoke_ctx *ictx;
	struct hlist_node *n;

	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(ictx, n, &me->clst.pending, hn) {
		if (ictx->msg.pid) {
			ictx->isWorkDone = true;
			trace_fastrpc_context_complete(me->cid, (uint64_t)ictx,
				ictx->retval, ictx->msg.invoke.header.ctx,
				ictx->handle, ictx->sc);
			complete(&ictx->work);
		}
	}
	hlist_for_each_entry_safe(ictx, n, &me->clst.interrupted, hn) {
		if (ictx->msg.pid) {
			ictx->isWorkDone = true;
			trace_fastrpc_context_complete(me->cid, (uint64_t)ictx,
				ictx->retval, ictx->msg.invoke.header.ctx,
				ictx->handle, ictx->sc);
			complete(&ictx->work);
		}
	}
	spin_unlock(&me->hlock);
}


static void fastrpc_notify_drivers(struct fastrpc_apps *me, int cid)
{
	struct fastrpc_file *fl;
	struct hlist_node *n;

	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(fl, n, &me->drivers, hn) {
		if (fl->cid == cid)
			fastrpc_notify_users(fl);
	}
	spin_unlock(&me->hlock);
}

static void fastrpc_notify_pdr_drivers(struct fastrpc_apps *me,
		char *servloc_name)
{
	struct fastrpc_file *fl;
	struct hlist_node *n;

	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(fl, n, &me->drivers, hn) {
		if (fl->servloc_name && !strcmp(servloc_name, fl->servloc_name))
			fastrpc_notify_users_staticpd_pdr(fl);
	}
	spin_unlock(&me->hlock);
}

static void context_list_ctor(struct fastrpc_ctx_lst *me)
{
	INIT_HLIST_HEAD(&me->interrupted);
	INIT_HLIST_HEAD(&me->pending);
}

static void fastrpc_context_list_dtor(struct fastrpc_file *fl)
{
	struct fastrpc_ctx_lst *clst = &fl->clst;
	struct smq_invoke_ctx *ictx = NULL, *ctxfree;
	struct hlist_node *n;

	do {
		ctxfree = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(ictx, n, &clst->interrupted, hn) {
			hlist_del_init(&ictx->hn);
			ctxfree = ictx;
			break;
		}
		spin_unlock(&fl->hlock);
		if (ctxfree)
			context_free(ctxfree);
	} while (ctxfree);
	do {
		ctxfree = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(ictx, n, &clst->pending, hn) {
			hlist_del_init(&ictx->hn);
			ctxfree = ictx;
			break;
		}
		spin_unlock(&fl->hlock);
		if (ctxfree)
			context_free(ctxfree);
	} while (ctxfree);
}

static int fastrpc_file_free(struct fastrpc_file *fl);
static void fastrpc_file_list_dtor(struct fastrpc_apps *me)
{
	struct fastrpc_file *fl, *free;
	struct hlist_node *n;

	do {
		free = NULL;
		spin_lock(&me->hlock);
		hlist_for_each_entry_safe(fl, n, &me->drivers, hn) {
			hlist_del_init(&fl->hn);
			free = fl;
			break;
		}
		spin_unlock(&me->hlock);
		if (free)
			fastrpc_file_free(free);
	} while (free);
}

static int get_args(uint32_t kernel, struct smq_invoke_ctx *ctx)
{
	remote_arg64_t *rpra, *lrpra;
	remote_arg_t *lpra = ctx->lpra;
	struct smq_invoke_buf *list;
	struct smq_phy_page *pages, *ipage;
	uint32_t sc = ctx->sc;
	int inbufs = REMOTE_SCALARS_INBUFS(sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	int handles, bufs = inbufs + outbufs;
	uintptr_t args;
	size_t rlen = 0, copylen = 0, metalen = 0, lrpralen = 0;
	int i, oix;
	int err = 0;
	int mflags = 0;
	uint64_t *fdlist;
	uint32_t *crclist;
	uint32_t earlyHint;
	int64_t *perf_counter = NULL;

	if (ctx->fl->profile)
		perf_counter = getperfcounter(ctx->fl, PERF_COUNT);

	/* calculate size of the metadata */
	rpra = NULL;
	lrpra = NULL;
	list = smq_invoke_buf_start(rpra, sc);
	pages = smq_phy_page_start(sc, list);
	ipage = pages;

	PERF(ctx->fl->profile, GET_COUNTER(perf_counter, PERF_MAP),
	for (i = 0; i < bufs; ++i) {
		uintptr_t buf = (uintptr_t)lpra[i].buf.pv;
		size_t len = lpra[i].buf.len;

		mutex_lock(&ctx->fl->map_mutex);
		if (ctx->fds && (ctx->fds[i] != -1))
			err = fastrpc_mmap_create(ctx->fl, ctx->fds[i],
					ctx->attrs[i], buf, len,
					mflags, &ctx->maps[i]);
		mutex_unlock(&ctx->fl->map_mutex);
		if (err)
			goto bail;
		ipage += 1;
	}
	PERF_END);
	handles = REMOTE_SCALARS_INHANDLES(sc) + REMOTE_SCALARS_OUTHANDLES(sc);
	mutex_lock(&ctx->fl->map_mutex);
	for (i = bufs; i < bufs + handles; i++) {
		int dmaflags = 0;

		if (ctx->attrs && (ctx->attrs[i] & FASTRPC_ATTR_NOMAP))
			dmaflags = FASTRPC_DMAHANDLE_NOMAP;
		if (ctx->fds && (ctx->fds[i] != -1))
			err = fastrpc_mmap_create(ctx->fl, ctx->fds[i],
					FASTRPC_ATTR_NOVA, 0, 0, dmaflags,
					&ctx->maps[i]);
		if (err) {
			mutex_unlock(&ctx->fl->map_mutex);
			goto bail;
		}
		ipage += 1;
	}
	mutex_unlock(&ctx->fl->map_mutex);

	/* metalen includes meta data, fds, crc and early wakeup hint */
	metalen = copylen = (size_t)&ipage[0] + (sizeof(uint64_t) * M_FDLIST) +
			(sizeof(uint32_t) * M_CRCLIST) + sizeof(earlyHint);

	/* allocate new local rpra buffer */
	lrpralen = (size_t)&list[0];
	if (lrpralen) {
		lrpra = kzalloc(lrpralen, GFP_KERNEL);
		VERIFY(err, !IS_ERR_OR_NULL(lrpra));
		if (err)
			goto bail;
	}
	ctx->lrpra = lrpra;

	/* calculate len required for copying */
	for (oix = 0; oix < inbufs + outbufs; ++oix) {
		int i = ctx->overps[oix]->raix;
		uintptr_t mstart, mend;
		size_t len = lpra[i].buf.len;

		if (!len)
			continue;
		if (ctx->maps[i])
			continue;
		if (ctx->overps[oix]->offset == 0)
			copylen = ALIGN(copylen, BALIGN);
		mstart = ctx->overps[oix]->mstart;
		mend = ctx->overps[oix]->mend;
		VERIFY(err, (mend - mstart) <= LONG_MAX);
		if (err)
			goto bail;
		copylen += mend - mstart;
	}
	ctx->used = copylen;

	/* allocate new buffer */
	if (copylen) {
		err = fastrpc_buf_alloc(ctx->fl, copylen, 0, 0, 0, &ctx->buf);
		if (err)
			goto bail;
	}
	if (ctx->buf->virt && metalen <= copylen)
		memset(ctx->buf->virt, 0, metalen);

	/* copy metadata */
	rpra = ctx->buf->virt;
	ctx->rpra = rpra;
	list = smq_invoke_buf_start(rpra, sc);
	pages = smq_phy_page_start(sc, list);
	ipage = pages;
	args = (uintptr_t)ctx->buf->virt + metalen;
	for (i = 0; i < bufs + handles; ++i) {
		if (lpra[i].buf.len)
			list[i].num = 1;
		else
			list[i].num = 0;
		list[i].pgidx = ipage - pages;
		ipage++;
	}

	/* map ion buffers */
	PERF(ctx->fl->profile, GET_COUNTER(perf_counter, PERF_MAP),
	for (i = 0; rpra && i < inbufs + outbufs; ++i) {
		struct fastrpc_mmap *map = ctx->maps[i];
		uint64_t buf = ptr_to_uint64(lpra[i].buf.pv);
		size_t len = lpra[i].buf.len;

		rpra[i].buf.pv = 0;
		rpra[i].buf.len = len;
		if (!len)
			continue;
		if (map) {
			struct vm_area_struct *vma;
			uintptr_t offset;
			uint64_t num = buf_num_pages(buf, len);
			int idx = list[i].pgidx;

			if (map->attr & FASTRPC_ATTR_NOVA) {
				offset = 0;
			} else {
				down_read(&current->mm->mmap_sem);
				VERIFY(err, NULL != (vma = find_vma(current->mm,
								map->va)));
				if (err) {
					up_read(&current->mm->mmap_sem);
					goto bail;
				}
				offset = buf_page_start(buf) - vma->vm_start;
				up_read(&current->mm->mmap_sem);
				VERIFY(err, offset < (uintptr_t)map->size);
				if (err)
					goto bail;
			}
			pages[idx].addr = map->phys + offset;
			pages[idx].size = num << PAGE_SHIFT;
		}
		rpra[i].buf.pv = buf;
	}
	PERF_END);
	for (i = bufs; i < bufs + handles; ++i) {
		struct fastrpc_mmap *map = ctx->maps[i];
		if (map) {
			pages[i].addr = map->phys;
			pages[i].size = map->size;
		}
	}
	fdlist = (uint64_t *)&pages[bufs + handles];
	crclist = (uint32_t *)&fdlist[M_FDLIST];
	/* reset fds, crc and early wakeup hint memory */
	/* remote process updates these values before responding */
	memset(fdlist, 0, sizeof(uint64_t)*M_FDLIST +
			sizeof(uint32_t)*M_CRCLIST + sizeof(earlyHint));

	/* copy non ion buffers */
	PERF(ctx->fl->profile, GET_COUNTER(perf_counter, PERF_COPY),
	rlen = copylen - metalen;
	for (oix = 0; rpra && oix < inbufs + outbufs; ++oix) {
		int i = ctx->overps[oix]->raix;
		struct fastrpc_mmap *map = ctx->maps[i];
		size_t mlen;
		uint64_t buf;
		size_t len = lpra[i].buf.len;

		if (!len)
			continue;
		if (map)
			continue;
		if (ctx->overps[oix]->offset == 0) {
			rlen -= ALIGN(args, BALIGN) - args;
			args = ALIGN(args, BALIGN);
		}
		mlen = ctx->overps[oix]->mend - ctx->overps[oix]->mstart;
		VERIFY(err, rlen >= mlen);
		if (err)
			goto bail;
		rpra[i].buf.pv =
			 (args - ctx->overps[oix]->offset);
		pages[list[i].pgidx].addr = ctx->buf->phys -
					    ctx->overps[oix]->offset +
					    (copylen - rlen);
		pages[list[i].pgidx].addr =
			buf_page_start(pages[list[i].pgidx].addr);
		buf = rpra[i].buf.pv;
		pages[list[i].pgidx].size = buf_num_pages(buf, len) * PAGE_SIZE;
		if (i < inbufs) {
			K_COPY_FROM_USER(err, kernel, uint64_to_ptr(buf),
					lpra[i].buf.pv, len);
			if (err)
				goto bail;
		}
		args = args + mlen;
		rlen -= mlen;
	}
	PERF_END);

	PERF(ctx->fl->profile, GET_COUNTER(perf_counter, PERF_FLUSH),
	for (oix = 0; oix < inbufs + outbufs; ++oix) {
		int i = ctx->overps[oix]->raix;
		struct fastrpc_mmap *map = ctx->maps[i];

		if (i+1 > inbufs)	// Avoiding flush for outbufs
			continue;
		if (map && map->uncached)
			continue;
		if (ctx->fl->sctx->smmu.coherent &&
			!(map && (map->attr & FASTRPC_ATTR_NON_COHERENT)))
			continue;
		if (map && (map->attr & FASTRPC_ATTR_COHERENT))
			continue;
		if (map && (map->attr & FASTRPC_ATTR_FORCE_NOFLUSH))
			continue;

		if (rpra && rpra[i].buf.len && (ctx->overps[oix]->mstart ||
		ctx->overps[oix]->do_cmo == 1)) {
			if (map && map->buf) {
				if ((buf_page_size(ctx->overps[oix]->mend -
				ctx->overps[oix]->mstart)) == map->size) {
					dma_buf_begin_cpu_access(map->buf,
						DMA_TO_DEVICE);
					dma_buf_end_cpu_access(map->buf,
						DMA_TO_DEVICE);
					pr_debug("Debug: adsprpc: %s: %s: sc 0x%x pv 0x%llx, mend 0x%llx mstart 0x%llx, len %zu size %zu\n",
					current->comm, __func__, sc,
					rpra[i].buf.pv, ctx->overps[oix]->mend,
					ctx->overps[oix]->mstart,
					rpra[i].buf.len, map->size);
				} else {
					uintptr_t offset;
					uint64_t flush_len;
					struct vm_area_struct *vma;

					down_read(&current->mm->mmap_sem);
					VERIFY(err, NULL != (vma = find_vma(
						current->mm, rpra[i].buf.pv)));
					if (err) {
						up_read(&current->mm->mmap_sem);
						goto bail;
					}
					if (ctx->overps[oix]->do_cmo) {
						offset = rpra[i].buf.pv -
								vma->vm_start;
						flush_len = rpra[i].buf.len;
					} else {
						offset =
						ctx->overps[oix]->mstart
						- vma->vm_start;
						flush_len =
						ctx->overps[oix]->mend -
						ctx->overps[oix]->mstart;
					}
					up_read(&current->mm->mmap_sem);
					dma_buf_begin_cpu_access_partial(
						map->buf, DMA_TO_DEVICE, offset,
						flush_len);
					dma_buf_end_cpu_access_partial(
						map->buf, DMA_TO_DEVICE, offset,
						flush_len);
					pr_debug("Debug: adsprpc: %s: %s: sc 0x%x vm_start 0x%llx pv 0x%llx, offset 0x%llx, mend 0x%llx mstart 0x%llx, len %zu size %zu\n",
					current->comm, __func__, sc,
					vma->vm_start, rpra[i].buf.pv, offset,
					ctx->overps[oix]->mend,
					ctx->overps[oix]->mstart,
					rpra[i].buf.len, map->size);
				}
			} else
				dmac_flush_range(uint64_to_ptr(rpra[i].buf.pv),
					uint64_to_ptr(rpra[i].buf.pv
						+ rpra[i].buf.len));
		}
	}
	PERF_END);
	for (i = bufs; rpra && i < bufs + handles; i++) {
		rpra[i].dma.fd = ctx->fds[i];
		rpra[i].dma.len = (uint32_t)lpra[i].buf.len;
		rpra[i].dma.offset = (uint32_t)(uintptr_t)lpra[i].buf.pv;
	}

	/* Copy rpra to local buffer */
	if (ctx->lrpra && rpra && lrpralen > 0)
		memcpy(ctx->lrpra, rpra, lrpralen);
 bail:
	return err;
}

static int put_args(uint32_t kernel, struct smq_invoke_ctx *ctx,
		    remote_arg_t *upra)
{
	uint32_t sc = ctx->sc;
	struct smq_invoke_buf *list;
	struct smq_phy_page *pages;
	struct fastrpc_mmap *mmap;
	uint64_t *fdlist;
	uint32_t *crclist = NULL;

	remote_arg64_t *rpra = ctx->lrpra;
	int i, inbufs, outbufs, handles;
	int err = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	handles = REMOTE_SCALARS_INHANDLES(sc) + REMOTE_SCALARS_OUTHANDLES(sc);
	list = smq_invoke_buf_start(ctx->rpra, sc);
	pages = smq_phy_page_start(sc, list);
	fdlist = (uint64_t *)(pages + inbufs + outbufs + handles);
	crclist = (uint32_t *)(fdlist + M_FDLIST);

	for (i = inbufs; i < inbufs + outbufs; ++i) {
		if (!ctx->maps[i]) {
			K_COPY_TO_USER(err, kernel,
				ctx->lpra[i].buf.pv,
				uint64_to_ptr(rpra[i].buf.pv),
				rpra[i].buf.len);
			if (err)
				goto bail;
		} else {
			mutex_lock(&ctx->fl->map_mutex);
			fastrpc_mmap_free(ctx->maps[i], 0);
			mutex_unlock(&ctx->fl->map_mutex);
			ctx->maps[i] = NULL;
		}
	}
	mutex_lock(&ctx->fl->map_mutex);
	if (inbufs + outbufs + handles) {
		for (i = 0; i < M_FDLIST; i++) {
			if (!fdlist[i])
				break;
			if (!fastrpc_mmap_find(ctx->fl, (int)fdlist[i], 0, 0,
						0, 0, &mmap))
				fastrpc_mmap_free(mmap, 0);
		}
	}
	mutex_unlock(&ctx->fl->map_mutex);
	if (ctx->crc && crclist && rpra)
		K_COPY_TO_USER(err, kernel, ctx->crc,
			crclist, M_CRCLIST*sizeof(uint32_t));

 bail:
	return err;
}

static void inv_args(struct smq_invoke_ctx *ctx)
{
	int i, inbufs, outbufs;
	uint32_t sc = ctx->sc;
	remote_arg64_t *rpra = ctx->lrpra;
	int err = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (i = 0; i < inbufs + outbufs; ++i) {
		int over = ctx->overps[i]->raix;
		struct fastrpc_mmap *map = ctx->maps[over];

		if ((over + 1 <= inbufs))
			continue;

		if (map && map->uncached)
			continue;
		if (!rpra[over].buf.len)
			continue;
		if (ctx->fl->sctx->smmu.coherent &&
			!(map && (map->attr & FASTRPC_ATTR_NON_COHERENT)))
			continue;
		if (map && (map->attr & FASTRPC_ATTR_COHERENT))
			continue;
		if (map && (map->attr & FASTRPC_ATTR_FORCE_NOINVALIDATE))
			continue;

		if (buf_page_start(ptr_to_uint64((void *)rpra)) ==
				buf_page_start(rpra[over].buf.pv)) {
			continue;
		}
		if (ctx->overps[i]->mstart || ctx->overps[i]->do_cmo == 1) {
			if (map && map->buf) {
				if ((buf_page_size(ctx->overps[i]->mend -
				ctx->overps[i]->mstart)) == map->size) {
					dma_buf_begin_cpu_access(map->buf,
						DMA_TO_DEVICE);
					dma_buf_end_cpu_access(map->buf,
						DMA_FROM_DEVICE);
					pr_debug("Debug: adsprpc: %s: %s: sc 0x%x pv 0x%llx, mend 0x%llx mstart 0x%llx, len %zu size %zu\n",
					current->comm, __func__, sc,
					rpra[over].buf.pv, ctx->overps[i]->mend,
					ctx->overps[i]->mstart,
					rpra[over].buf.len, map->size);
				} else {
					uintptr_t offset;
					uint64_t inv_len;
					struct vm_area_struct *vma;

					down_read(&current->mm->mmap_sem);
					VERIFY(err, NULL != (vma = find_vma(
						current->mm,
						rpra[over].buf.pv)));
					if (err) {
						up_read(&current->mm->mmap_sem);
						goto bail;
					}
					if (ctx->overps[i]->do_cmo) {
						offset = rpra[over].buf.pv -
								vma->vm_start;
						inv_len = rpra[over].buf.len;
					} else {
						offset =
							ctx->overps[i]->mstart -
							vma->vm_start;
						inv_len =
							ctx->overps[i]->mend -
							ctx->overps[i]->mstart;
					}
					up_read(&current->mm->mmap_sem);
					dma_buf_begin_cpu_access_partial(
						map->buf, DMA_TO_DEVICE, offset,
						inv_len);
					dma_buf_end_cpu_access_partial(map->buf,
						DMA_FROM_DEVICE, offset,
						inv_len);
					pr_debug("Debug: adsprpc: %s: %s: sc 0x%x vm_start 0x%llx pv 0x%llx, offset 0x%llx, mend 0x%llx mstart 0x%llx, len %zu size %zu\n",
					current->comm, __func__, sc,
					vma->vm_start, rpra[over].buf.pv,
					offset, ctx->overps[i]->mend,
					ctx->overps[i]->mstart,
					rpra[over].buf.len, map->size);
			}
		} else
			dmac_inv_range((char *)uint64_to_ptr(rpra[over].buf.pv),
				(char *)uint64_to_ptr(rpra[over].buf.pv
						 + rpra[over].buf.len));
		}
	}
bail:
	return;
}

static int fastrpc_invoke_send(struct smq_invoke_ctx *ctx,
			       uint32_t kernel, uint32_t handle)
{
	struct smq_msg *msg = &ctx->msg;
	struct fastrpc_file *fl = ctx->fl;
	struct fastrpc_channel_ctx *channel_ctx = NULL;
	int err = 0, cid = -1;

	cid = fl->cid;
	VERIFY(err, cid >= ADSP_DOMAIN_ID && cid < NUM_CHANNELS);
	if (err) {
		err = -ECHRNG;
		goto bail;
	}

	channel_ctx = &fl->apps->channel[fl->cid];
	mutex_lock(&channel_ctx->smd_mutex);
	msg->pid = fl->tgid;
	msg->tid = current->pid;
	if (fl->sessionid)
		msg->tid |= (1 << SESSION_ID_INDEX);
	if (kernel)
		msg->pid = 0;
	msg->invoke.header.ctx = ctx->ctxid | fl->pd;
	msg->invoke.header.handle = handle;
	msg->invoke.header.sc = ctx->sc;
	msg->invoke.page.addr = ctx->buf ? ctx->buf->phys : 0;
	msg->invoke.page.size = buf_page_size(ctx->used);

	if (fl->ssrcount != channel_ctx->ssrcount) {
		err = -ECONNRESET;
		mutex_unlock(&channel_ctx->smd_mutex);
		goto bail;
	}
	mutex_unlock(&channel_ctx->smd_mutex);

	mutex_lock(&channel_ctx->rpmsg_mutex);
	VERIFY(err, !IS_ERR_OR_NULL(channel_ctx->rpdev));
	if (err) {
		err = -ECONNRESET;
		mutex_unlock(&channel_ctx->rpmsg_mutex);
		goto bail;
	}
	err = rpmsg_send(channel_ctx->rpdev->ept, (void *)msg, sizeof(*msg));
	trace_fastrpc_rpmsg_send(fl->cid, (uint64_t)ctx, msg->invoke.header.ctx,
		handle, ctx->sc, msg->invoke.page.addr, msg->invoke.page.size);
	LOG_FASTRPC_GLINK_MSG(channel_ctx->ipc_log_ctx,
		"sent pkt %pK (sz %d): ctx 0x%llx, handle 0x%x, sc 0x%x (rpmsg err %d)",
		(void *)msg, sizeof(*msg),
		msg->invoke.header.ctx, handle, ctx->sc, err);
	mutex_unlock(&channel_ctx->rpmsg_mutex);
 bail:
	return err;
}

static void fastrpc_init(struct fastrpc_apps *me)
{
	int i;

	INIT_HLIST_HEAD(&me->drivers);
	INIT_HLIST_HEAD(&me->maps);
	spin_lock_init(&me->hlock);
	me->channel = &gcinfo[0];
	for (i = 0; i < NUM_CHANNELS; i++) {
		init_completion(&me->channel[i].work);
		init_completion(&me->channel[i].workport);
		me->channel[i].sesscount = 0;
		/* All channels are secure by default except CDSP */
		me->channel[i].secure = SECURE_CHANNEL;
		mutex_init(&me->channel[i].smd_mutex);
		mutex_init(&me->channel[i].rpmsg_mutex);
		spin_lock_init(&me->channel[i].ctxlock);
	}
	/* Set CDSP channel to non secure */
	me->channel[CDSP_DOMAIN_ID].secure = NON_SECURE_CHANNEL;
}

static inline void fastrpc_pm_awake(struct fastrpc_file *fl)
{
	if (!fl->wake_enable || !fl->wake_source)
		return;
	/*
	 * Vote with PM to abort any suspend in progress and
	 * keep system awake for specified timeout
	 */
	pm_wakeup_ws_event(fl->wake_source, fl->ws_timeout, true);
}

static inline int fastrpc_wait_for_response(struct smq_invoke_ctx *ctx,
						uint32_t kernel)
{
	int interrupted = 0;

	if (kernel)
		wait_for_completion(&ctx->work);
	else
		interrupted = wait_for_completion_interruptible(&ctx->work);

	return interrupted;
}

static void fastrpc_wait_for_completion(struct smq_invoke_ctx *ctx,
		 int *pInterrupted, uint32_t kernel)
{
	int interrupted = 0, err = 0;
	int jj;
	bool wait_resp;
	uint32_t wTimeout = FASTRPC_USER_EARLY_HINT_TIMEOUT;
	uint32_t wakeTime = 0;

	if (!ctx) {
		/* This failure is not expected */
		err = *pInterrupted = EFAULT;
		pr_err("Error %d: adsprpc: %s: %s: ctx is NULL, cannot wait for response\n",
					err, current->comm, __func__);
		return;
	}
	wakeTime = ctx->earlyWakeTime;

	do {
		switch (ctx->rspFlags) {
		/* try polling on completion with timeout */
		case USER_EARLY_SIGNAL:
			/* try wait if completion time is less than timeout */
			/* disable preempt to avoid context switch latency */
			preempt_disable();
			jj = 0;
			wait_resp = false;
			for (; wakeTime < wTimeout && jj < wTimeout; jj++) {
				wait_resp = try_wait_for_completion(&ctx->work);
				if (wait_resp)
					break;
				udelay(1);
			}
			preempt_enable_no_resched();
			if (!wait_resp) {
				interrupted = fastrpc_wait_for_response(ctx,
									kernel);
				*pInterrupted = interrupted;
				if (interrupted || ctx->isWorkDone)
					return;
			}
			break;

		/* busy poll on memory for actual job done */
		case EARLY_RESPONSE:
			err = poll_on_early_response(ctx);

			/* Mark job done if poll on memory successful */
			/* Wait for completion if poll on memory timoeut */
			if (!err) {
				ctx->isWorkDone = true;
			} else if (!ctx->isWorkDone) {
				pr_info("adsprpc: %s: %s: poll timeout for handle 0x%x, sc 0x%x\n",
				__func__, current->comm, ctx->handle, ctx->sc);
				interrupted = fastrpc_wait_for_response(ctx,
									kernel);
				*pInterrupted = interrupted;
				if (interrupted || ctx->isWorkDone)
					return;
			}
			break;

		case COMPLETE_SIGNAL:
		case NORMAL_RESPONSE:
			interrupted = fastrpc_wait_for_response(ctx, kernel);
			*pInterrupted = interrupted;
			if (interrupted || ctx->isWorkDone)
				return;
			break;

		default:
			*pInterrupted = EBADR;
			pr_err("Error: adsprpc: %s: unsupported response flags 0x%x for handle 0x%x, sc 0x%x\n",
			current->comm, ctx->rspFlags, ctx->handle, ctx->sc);
			return;
		} /* end of switch */
	} while (!ctx->isWorkDone);
}

static void fastrpc_update_invoke_count(uint32_t handle, int64_t *perf_counter,
					struct timespec *invoket)
{
	/* update invoke count for dynamic handles */
	if (handle != FASTRPC_STATIC_HANDLE_LISTENER) {
		int64_t *count = GET_COUNTER(perf_counter, PERF_INVOKE);

		if (count)
			*count += getnstimediff(invoket);
	}
	if (handle > FASTRPC_STATIC_HANDLE_MAX) {
		int64_t *count = GET_COUNTER(perf_counter, PERF_COUNT);

		if (count)
			*count += 1;
	}
}

static int fastrpc_internal_invoke(struct fastrpc_file *fl, uint32_t mode,
				   uint32_t kernel,
				   struct fastrpc_ioctl_invoke_crc *inv)
{
	struct smq_invoke_ctx *ctx = NULL;
	struct fastrpc_ioctl_invoke *invoke = &inv->inv;
	int err = 0, interrupted = 0, cid = -1;
	struct timespec invoket = {0};
	int64_t *perf_counter = NULL;

	cid = fl->cid;
	VERIFY(err, cid >= ADSP_DOMAIN_ID && cid < NUM_CHANNELS);
	if (err) {
		err = -ECHRNG;
		goto bail;
	}
	VERIFY(err, fl->sctx != NULL);
	if (err) {
		pr_err("adsprpc: ERROR: %s: user application %s domain is not set\n",
			__func__, current->comm);
		err = -EBADR;
		goto bail;
	}
	if (fl->profile) {
		perf_counter = getperfcounter(fl, PERF_COUNT);
		getnstimeofday(&invoket);
	}

	if (!kernel) {
		VERIFY(err, invoke->handle !=
			FASTRPC_STATIC_HANDLE_PROCESS_GROUP);
		VERIFY(err, invoke->handle !=
			FASTRPC_STATIC_HANDLE_DSP_UTILITIES);
		if (err) {
			pr_err("adsprpc: ERROR: %s: user application %s trying to send a kernel RPC message to channel %d, handle 0x%x\n",
				__func__, current->comm, cid, invoke->handle);
			goto bail;
		}
	}

	if (!kernel) {
		err = context_restore_interrupted(fl, inv, &ctx);
		if (err)
			goto bail;
		if (fl->sctx->smmu.faults)
			err = FASTRPC_ENOSUCH;
		if (err)
			goto bail;
		if (ctx) {
			trace_fastrpc_context_restore(cid, (uint64_t)ctx,
				ctx->msg.invoke.header.ctx,
				ctx->handle, ctx->sc);
			goto wait;
		}
	}

	VERIFY(err, 0 == context_alloc(fl, kernel, inv, &ctx));
	if (err)
		goto bail;

	if (REMOTE_SCALARS_LENGTH(ctx->sc)) {
		PERF(fl->profile, GET_COUNTER(perf_counter, PERF_GETARGS),
		VERIFY(err, 0 == get_args(kernel, ctx));
		PERF_END);
		if (err)
			goto bail;
	}

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_INVARGS),
	inv_args(ctx);
	PERF_END);

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_LINK),
	VERIFY(err, 0 == fastrpc_invoke_send(ctx, kernel, invoke->handle));
	PERF_END);

	if (err)
		goto bail;
 wait:
	fastrpc_wait_for_completion(ctx, &interrupted, kernel);
	VERIFY(err, 0 == (err = interrupted));
	if (err)
		goto bail;

	if (!ctx->isWorkDone) {
		err = EPROTO;
		pr_err("Error: adsprpc: %s: %s: WorkDone state is invalid for handle 0x%x, sc 0x%x\n",
			__func__, current->comm, invoke->handle, ctx->sc);
		goto bail;
	}

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_INVARGS),
	inv_args(ctx);
	PERF_END);

	VERIFY(err, 0 == (err = ctx->retval));
	if (err)
		goto bail;

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_PUTARGS),
	VERIFY(err, 0 == put_args(kernel, ctx, invoke->pra));
	PERF_END);
	if (err)
		goto bail;
 bail:
	if (ctx && interrupted == -ERESTARTSYS) {
		trace_fastrpc_context_interrupt(cid, (uint64_t)ctx,
			ctx->msg.invoke.header.ctx, ctx->handle, ctx->sc);
		context_save_interrupted(ctx);
	}
	else if (ctx)
		context_free(ctx);
	if (fl->ssrcount != fl->apps->channel[cid].ssrcount)
		err = ECONNRESET;

	if (fl->profile && !interrupted)
		fastrpc_update_invoke_count(invoke->handle, perf_counter,
						&invoket);
	return err;
}

static int fastrpc_get_spd_session(char *name, int *session, int *cid)
{
	struct fastrpc_apps *me = &gfa;
	int err = 0, i, j, match = 0;

	for (i = 0; i < NUM_CHANNELS; i++) {
		for (j = 0; j < NUM_SESSIONS; j++) {
			if (!me->channel[i].spd[j].servloc_name)
				continue;
			if (!strcmp(name, me->channel[i].spd[j].servloc_name)) {
				match = 1;
				break;
			}
		}
		if (match)
			break;
	}
	VERIFY(err, i < NUM_CHANNELS && j < NUM_SESSIONS);
	if (err)
		goto bail;
	*cid = i;
	*session = j;
bail:
	return err;
}

static int fastrpc_mmap_remove_pdr(struct fastrpc_file *fl);
static int fastrpc_channel_open(struct fastrpc_file *fl);
static int fastrpc_mmap_remove_ssr(struct fastrpc_file *fl);
static int fastrpc_init_process(struct fastrpc_file *fl,
				struct fastrpc_ioctl_init_attrs *uproc)
{
	int err = 0, rh_hyp_done = 0;
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_ioctl_invoke_crc ioctl;
	struct fastrpc_ioctl_init *init = &uproc->init;
	struct smq_phy_page pages[1];
	struct fastrpc_mmap *file = NULL, *mem = NULL;
	struct fastrpc_buf *imem = NULL;
	unsigned long imem_dma_attr = 0;
	char *proc_name = NULL;

	VERIFY(err, 0 == (err = fastrpc_channel_open(fl)));
	if (err)
		goto bail;
	if (init->flags == FASTRPC_INIT_ATTACH ||
			init->flags == FASTRPC_INIT_ATTACH_SENSORS) {
		remote_arg_t ra[1];
		int tgid = fl->tgid;

		ra[0].buf.pv = (void *)&tgid;
		ra[0].buf.len = sizeof(tgid);
		ioctl.inv.handle = FASTRPC_STATIC_HANDLE_PROCESS_GROUP;
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(0, 1, 0);
		ioctl.inv.pra = ra;
		ioctl.fds = NULL;
		ioctl.attrs = NULL;
		ioctl.crc = NULL;
		if (init->flags == FASTRPC_INIT_ATTACH)
			fl->pd = 0;
		else if (init->flags == FASTRPC_INIT_ATTACH_SENSORS) {
			if (fl->cid == ADSP_DOMAIN_ID)
				fl->servloc_name =
				SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME;
			else if (fl->cid == SDSP_DOMAIN_ID)
				fl->servloc_name =
				SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME;
			fl->pd = 2;
		}
		VERIFY(err, !(err = fastrpc_internal_invoke(fl,
			FASTRPC_MODE_PARALLEL, 1, &ioctl)));
		if (err)
			goto bail;
	} else if (init->flags == FASTRPC_INIT_CREATE) {
		int memlen;

		remote_arg_t ra[6];
		int fds[6];
		int mflags = 0;
		struct {
			int pgid;
			unsigned int namelen;
			unsigned int filelen;
			unsigned int pageslen;
			int attrs;
			int siglen;
		} inbuf;

		inbuf.pgid = fl->tgid;
		inbuf.namelen = strlen(current->comm) + 1;
		inbuf.filelen = init->filelen;
		fl->pd = 1;

		VERIFY(err, access_ok(0, (void __user *)init->file,
			init->filelen));
		if (err)
			goto bail;
		if (init->filelen) {
			mutex_lock(&fl->map_mutex);
			VERIFY(err, !fastrpc_mmap_create(fl, init->filefd, 0,
				init->file, init->filelen, mflags, &file));
			mutex_unlock(&fl->map_mutex);
			if (err)
				goto bail;
		}
		inbuf.pageslen = 1;

		VERIFY(err, !init->mem);
		if (err) {
			err = -EINVAL;
			pr_err("adsprpc: %s: %s: ERROR: donated memory allocated in userspace\n",
				current->comm, __func__);
			goto bail;
		}
		memlen = ALIGN(max(1024*1024*3, (int)init->filelen * 4),
						1024*1024);
		imem_dma_attr = DMA_ATTR_EXEC_MAPPING |
						DMA_ATTR_DELAYED_UNMAP |
						DMA_ATTR_NO_KERNEL_MAPPING |
						DMA_ATTR_FORCE_NON_COHERENT;
		err = fastrpc_buf_alloc(fl, memlen, imem_dma_attr, 0, 0, &imem);
		if (err)
			goto bail;
		if (fl->init_mem)
			fastrpc_buf_free(fl->init_mem, 0);

		fl->init_mem = imem;
		inbuf.pageslen = 1;
		ra[0].buf.pv = (void *)&inbuf;
		ra[0].buf.len = sizeof(inbuf);
		fds[0] = -1;

		ra[1].buf.pv = (void *)current->comm;
		ra[1].buf.len = inbuf.namelen;
		fds[1] = -1;

		ra[2].buf.pv = (void *)init->file;
		ra[2].buf.len = inbuf.filelen;
		fds[2] = init->filefd;

		pages[0].addr = imem->phys;
		pages[0].size = imem->size;
		ra[3].buf.pv = (void *)pages;
		ra[3].buf.len = 1 * sizeof(*pages);
		fds[3] = -1;

		inbuf.attrs = uproc->attrs;
		ra[4].buf.pv = (void *)&(inbuf.attrs);
		ra[4].buf.len = sizeof(inbuf.attrs);
		fds[4] = -1;

		inbuf.siglen = uproc->siglen;
		ra[5].buf.pv = (void *)&(inbuf.siglen);
		ra[5].buf.len = sizeof(inbuf.siglen);
		fds[5] = -1;

		ioctl.inv.handle = FASTRPC_STATIC_HANDLE_PROCESS_GROUP;
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(6, 4, 0);
		if (uproc->attrs)
			ioctl.inv.sc = REMOTE_SCALARS_MAKE(7, 6, 0);
		ioctl.inv.pra = ra;
		ioctl.fds = fds;
		ioctl.attrs = NULL;
		ioctl.crc = NULL;
		VERIFY(err, !(err = fastrpc_internal_invoke(fl,
			FASTRPC_MODE_PARALLEL, 1, &ioctl)));
		if (err)
			goto bail;
	} else if (init->flags == FASTRPC_INIT_CREATE_STATIC) {
		remote_arg_t ra[3];
		uint64_t phys = 0;
		size_t size = 0;
		int fds[3];
		struct {
			int pgid;
			unsigned int namelen;
			unsigned int pageslen;
		} inbuf;

		if (!init->filelen)
			goto bail;

		proc_name = kzalloc(init->filelen, GFP_KERNEL);
		VERIFY(err, !IS_ERR_OR_NULL(proc_name));
		if (err)
			goto bail;
		VERIFY(err, 0 == copy_from_user((void *)proc_name,
			(void __user *)init->file, init->filelen));
		if (err)
			goto bail;

		fl->pd = 1;
		inbuf.pgid = current->tgid;
		inbuf.namelen = init->filelen;
		inbuf.pageslen = 0;

		if (!strcmp(proc_name, "audiopd")) {
			fl->servloc_name =
				AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME;
			VERIFY(err, !fastrpc_mmap_remove_pdr(fl));
			if (err)
				goto bail;
		}

		if (!me->staticpd_flags && !(me->legacy_remote_heap)) {
			inbuf.pageslen = 1;
			mutex_lock(&fl->map_mutex);
			err = fastrpc_mmap_create(fl, -1, 0, init->mem,
				 init->memlen, ADSP_MMAP_REMOTE_HEAP_ADDR,
				 &mem);
			mutex_unlock(&fl->map_mutex);
			if (err)
				goto bail;
			phys = mem->phys;
			size = mem->size;
			if (me->channel[fl->cid].rhvm.vmid) {
				err = hyp_assign_phys(phys,
					(uint64_t)size, hlosvm, 1,
					me->channel[fl->cid].rhvm.vmid,
					me->channel[fl->cid].rhvm.vmperm,
					me->channel[fl->cid].rhvm.vmcount);
				if (err) {
					pr_err("adsprpc: %s: rh hyp assign failed with %d for phys 0x%llx, size %zd\n",
						__func__, err, phys, size);
					goto bail;
				}
				rh_hyp_done = 1;
			}
			me->staticpd_flags = 1;
		}

		ra[0].buf.pv = (void *)&inbuf;
		ra[0].buf.len = sizeof(inbuf);
		fds[0] = -1;

		ra[1].buf.pv = (void *)proc_name;
		ra[1].buf.len = inbuf.namelen;
		fds[1] = -1;

		pages[0].addr = phys;
		pages[0].size = size;

		ra[2].buf.pv = (void *)pages;
		ra[2].buf.len = sizeof(*pages);
		fds[2] = -1;
		ioctl.inv.handle = FASTRPC_STATIC_HANDLE_PROCESS_GROUP;

		ioctl.inv.sc = REMOTE_SCALARS_MAKE(8, 3, 0);
		ioctl.inv.pra = ra;
		ioctl.fds = NULL;
		ioctl.attrs = NULL;
		ioctl.crc = NULL;
		VERIFY(err, !(err = fastrpc_internal_invoke(fl,
			FASTRPC_MODE_PARALLEL, 1, &ioctl)));
		if (err)
			goto bail;
	} else {
		err = -ENOTTY;
		goto bail;
	}
	fl->dsp_proc_init = 1;
bail:
	kfree(proc_name);
	if (err && (init->flags == FASTRPC_INIT_CREATE_STATIC))
		me->staticpd_flags = 0;
	if (mem && err) {
		if (mem->flags == ADSP_MMAP_REMOTE_HEAP_ADDR
			&& me->channel[fl->cid].rhvm.vmid && rh_hyp_done) {
			int hyp_err = 0;

			hyp_err = hyp_assign_phys(mem->phys,
					(uint64_t)mem->size,
					me->channel[fl->cid].rhvm.vmid,
					me->channel[fl->cid].rhvm.vmcount,
					hlosvm, hlosvmperm, 1);
			if (hyp_err)
				pr_warn("adsprpc: %s: %s: rh hyp unassign failed with %d for phys 0x%llx of size %zd\n",
						__func__, current->comm,
						hyp_err, mem->phys, mem->size);
		}
		mutex_lock(&fl->map_mutex);
		fastrpc_mmap_free(mem, 0);
		mutex_unlock(&fl->map_mutex);
	}
	if (err) {
		if (!IS_ERR_OR_NULL(fl->init_mem)) {
			fastrpc_buf_free(fl->init_mem, 0);
			fl->init_mem = NULL;
		}
	}
	if (file) {
		mutex_lock(&fl->map_mutex);
		fastrpc_mmap_free(file, 0);
		mutex_unlock(&fl->map_mutex);
	}
	return err;
}

static int fastrpc_kstat(const char *filename, struct kstat *stat)
{
	int result;
	mm_segment_t fs_old;

	fs_old = get_fs();
	set_fs(KERNEL_DS);
	result = vfs_stat((const char __user *)filename, stat);
	set_fs(fs_old);

	return result;
}

static int fastrpc_send_cpuinfo_to_dsp(struct fastrpc_file *fl)
{
	int err = 0;
	uint64_t cpuinfo = 0;
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_ioctl_invoke_crc ioctl;
	remote_arg_t ra[2];

	VERIFY(err, fl && fl->cid >= ADSP_DOMAIN_ID && fl->cid < NUM_CHANNELS);
	if (err)
		goto bail;

	cpuinfo = me->channel[fl->cid].cpuinfo_todsp;
	/* return success if already updated to remote processor */
	if (me->channel[fl->cid].cpuinfo_status)
		return 0;

	ra[0].buf.pv = (void *)&cpuinfo;
	ra[0].buf.len = sizeof(cpuinfo);
	ioctl.inv.handle = FASTRPC_STATIC_HANDLE_DSP_UTILITIES;
	ioctl.inv.sc = REMOTE_SCALARS_MAKE(1, 1, 0);
	ioctl.inv.pra = ra;
	ioctl.fds = NULL;
	ioctl.attrs = NULL;
	ioctl.crc = NULL;
	fl->pd = 1;

	err = fastrpc_internal_invoke(fl, FASTRPC_MODE_PARALLEL, 1, &ioctl);
	if (!err)
		me->channel[fl->cid].cpuinfo_status = true;
bail:
	return err;
}

static int fastrpc_get_info_from_dsp(struct fastrpc_file *fl,
				uint32_t *dsp_attr_buf,
				uint32_t dsp_attr_buf_len,
				uint32_t domain)
{
	int err = 0, dsp_support = 0;
	struct fastrpc_ioctl_invoke_crc ioctl;
	remote_arg_t ra[2];
	struct kstat sb;

	// Querying device about DSP support
	switch (domain) {
	case ADSP_DOMAIN_ID:
		if (!fastrpc_kstat("/dev/subsys_adsp", &sb))
			dsp_support = 1;
		break;
	case MDSP_DOMAIN_ID:
		//Modem not supported for fastRPC
		break;
	case SDSP_DOMAIN_ID:
		if (!fastrpc_kstat("/dev/subsys_slpi", &sb))
			dsp_support = 1;
		break;
	case CDSP_DOMAIN_ID:
		if (!fastrpc_kstat("/dev/subsys_cdsp", &sb))
			dsp_support = 1;
		break;
	default:
		dsp_support = 0;
		break;
	}
	dsp_attr_buf[0] = dsp_support;

	if (dsp_support == 0)
		goto bail;

	err = fastrpc_channel_open(fl);
	if (err)
		goto bail;

	ra[0].buf.pv = (void *)&dsp_attr_buf_len;
	ra[0].buf.len = sizeof(dsp_attr_buf_len);
	ra[1].buf.pv = (void *)(&dsp_attr_buf[1]);
	ra[1].buf.len = dsp_attr_buf_len * sizeof(uint32_t);
	ioctl.inv.handle = FASTRPC_STATIC_HANDLE_DSP_UTILITIES;
	ioctl.inv.sc = REMOTE_SCALARS_MAKE(0, 1, 1);
	ioctl.inv.pra = ra;
	ioctl.fds = NULL;
	ioctl.attrs = NULL;
	ioctl.crc = NULL;
	fl->pd = 1;

	err = fastrpc_internal_invoke(fl, FASTRPC_MODE_PARALLEL, 1, &ioctl);
bail:

	if (err)
		pr_err("adsprpc: %s: %s: could not obtain dsp information, err val 0x%x\n",
		current->comm, __func__, err);
	return err;
}

static int fastrpc_get_info_from_kernel(
		struct fastrpc_ioctl_dsp_capabilities *dsp_cap,
		struct fastrpc_file *fl)
{
	int err = 0;
	uint32_t domain_support;
	uint32_t domain = dsp_cap->domain;

	if (!gcinfo[domain].dsp_cap_kernel.is_cached) {
		/*
		 * Information not on kernel, query device for information
		 * and cache on kernel
		 */
		err = fastrpc_get_info_from_dsp(fl, dsp_cap->dsp_attributes,
				FASTRPC_MAX_DSP_ATTRIBUTES - 1,
				domain);
		if (err)
			goto bail;

		domain_support = dsp_cap->dsp_attributes[0];
		switch (domain_support) {
		case 0:
			memset(dsp_cap->dsp_attributes, 0,
				sizeof(dsp_cap->dsp_attributes));
			memset(&gcinfo[domain].dsp_cap_kernel.dsp_attributes,
				0, sizeof(dsp_cap->dsp_attributes));
			break;
		case 1:
			memcpy(&gcinfo[domain].dsp_cap_kernel.dsp_attributes,
				dsp_cap->dsp_attributes,
				sizeof(dsp_cap->dsp_attributes));
			break;
		default:
			err = -1;
			/*
			 * Reset is_cached flag to 0 so subsequent calls
			 * can try to query dsp again
			 */
			gcinfo[domain].dsp_cap_kernel.is_cached = 0;
			pr_warn("adsprpc: %s: %s: returned bad domain support value %d\n",
					current->comm,
					__func__,
					domain_support);
			goto bail;
		}
		gcinfo[domain].dsp_cap_kernel.is_cached = 1;
	} else {
		// Information on Kernel, pass it to user
		memcpy(dsp_cap->dsp_attributes,
			&gcinfo[domain].dsp_cap_kernel.dsp_attributes,
			sizeof(dsp_cap->dsp_attributes));
	}
bail:
	return err;
}

static int fastrpc_release_current_dsp_process(struct fastrpc_file *fl)
{
	int err = 0;
	struct fastrpc_ioctl_invoke_crc ioctl;
	remote_arg_t ra[1];
	int tgid = 0;

	VERIFY(err, fl->cid >= ADSP_DOMAIN_ID && fl->cid < NUM_CHANNELS);
	if (err)
		goto bail;
	VERIFY(err, fl->sctx != NULL);
	if (err)
		goto bail;
	VERIFY(err, fl->apps->channel[fl->cid].rpdev != NULL);
	if (err)
		goto bail;
	VERIFY(err, fl->apps->channel[fl->cid].issubsystemup == 1);
	if (err)
		goto bail;
	tgid = fl->tgid;
	ra[0].buf.pv = (void *)&tgid;
	ra[0].buf.len = sizeof(tgid);
	ioctl.inv.handle = FASTRPC_STATIC_HANDLE_PROCESS_GROUP;
	ioctl.inv.sc = REMOTE_SCALARS_MAKE(1, 1, 0);
	ioctl.inv.pra = ra;
	ioctl.fds = NULL;
	ioctl.attrs = NULL;
	ioctl.crc = NULL;
	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
		FASTRPC_MODE_PARALLEL, 1, &ioctl)));
	if (err && fl->dsp_proc_init)
		pr_err("adsprpc: %s: releasing DSP process failed with %d (0x%x) for %s\n",
				__func__, err, err, current->comm);
bail:
	return err;
}

static int fastrpc_unmap_on_dsp(struct fastrpc_file *fl,
		uintptr_t raddr, uint64_t phys, size_t size, uint32_t flags)
{
	struct fastrpc_ioctl_invoke_crc ioctl;
	remote_arg_t ra[1] = {};
	int err = 0;
	struct {
		int pid;
		uintptr_t vaddrout;
		size_t size;
	} inargs;

	inargs.pid = fl->tgid;
	inargs.size = size;
	inargs.vaddrout = raddr;
	ra[0].buf.pv = (void *)&inargs;
	ra[0].buf.len = sizeof(inargs);

	ioctl.inv.handle = FASTRPC_STATIC_HANDLE_PROCESS_GROUP;
	if (fl->apps->compat)
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(5, 1, 0);
	else
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(3, 1, 0);
	ioctl.inv.pra = ra;
	ioctl.fds = NULL;
	ioctl.attrs = NULL;
	ioctl.crc = NULL;
	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
		FASTRPC_MODE_PARALLEL, 1, &ioctl)));
	if (err)
		goto bail;
bail:
	return err;
}

static int fastrpc_mmap_on_dsp(struct fastrpc_file *fl, uint32_t flags,
					uintptr_t va, uint64_t phys,
					size_t size, uintptr_t *raddr)
{
	struct fastrpc_ioctl_invoke_crc ioctl;
	struct fastrpc_apps *me = &gfa;
	struct smq_phy_page page;
	int num = 1;
	remote_arg_t ra[3];
	int err = 0;
	struct {
		int pid;
		uint32_t flags;
		uintptr_t vaddrin;
		int num;
	} inargs;
	struct {
		uintptr_t vaddrout;
	} routargs;

	inargs.pid = fl->tgid;
	inargs.vaddrin = (uintptr_t)va;
	inargs.flags = flags;
	inargs.num = fl->apps->compat ? num * sizeof(page) : num;
	ra[0].buf.pv = (void *)&inargs;
	ra[0].buf.len = sizeof(inargs);
	page.addr = phys;
	page.size = size;
	ra[1].buf.pv = (void *)&page;
	ra[1].buf.len = num * sizeof(page);

	ra[2].buf.pv = (void *)&routargs;
	ra[2].buf.len = sizeof(routargs);

	ioctl.inv.handle = FASTRPC_STATIC_HANDLE_PROCESS_GROUP;
	if (fl->apps->compat)
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(4, 2, 1);
	else
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(2, 2, 1);
	ioctl.inv.pra = ra;
	ioctl.fds = NULL;
	ioctl.attrs = NULL;
	ioctl.crc = NULL;
	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
		FASTRPC_MODE_PARALLEL, 1, &ioctl)));
	*raddr = (uintptr_t)routargs.vaddrout;
	if (err)
		goto bail;
	if (flags == ADSP_MMAP_REMOTE_HEAP_ADDR
				&& me->channel[fl->cid].rhvm.vmid) {
		err = hyp_assign_phys(phys, (uint64_t)size,
				hlosvm, 1, me->channel[fl->cid].rhvm.vmid,
				me->channel[fl->cid].rhvm.vmperm,
				me->channel[fl->cid].rhvm.vmcount);
		if (err) {
			pr_err("adsprpc: %s: %s: rh hyp assign failed with %d for phys 0x%llx, size %zd\n",
					__func__, current->comm,
					err, phys, size);
			err = fastrpc_unmap_on_dsp(fl,
				*raddr, phys, size, flags);
			if (err) {
				pr_err("adsprpc: %s: %s: failed to unmap %d for phys 0x%llx, size %zd\n",
					__func__, current->comm,
					err, phys, size);
			}
			goto bail;
		}
	}
bail:
	return err;
}

static int fastrpc_munmap_on_dsp_rh(struct fastrpc_file *fl, uint64_t phys,
						size_t size, uint32_t flags)
{
	int err = 0;
	struct fastrpc_apps *me = &gfa;
	int tgid = 0;
	int destVM[1] = {VMID_HLOS};
	int destVMperm[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};

	if (flags == ADSP_MMAP_HEAP_ADDR) {
		struct fastrpc_ioctl_invoke_crc ioctl;
		remote_arg_t ra[2];
		int err = 0;
		struct {
			uint8_t skey;
		} routargs;

		if (fl == NULL)
			goto bail;
		tgid = fl->tgid;
		ra[0].buf.pv = (void *)&tgid;
		ra[0].buf.len = sizeof(tgid);

		ra[1].buf.pv = (void *)&routargs;
		ra[1].buf.len = sizeof(routargs);

		ioctl.inv.handle = FASTRPC_STATIC_HANDLE_PROCESS_GROUP;
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(9, 1, 1);
		ioctl.inv.pra = ra;
		ioctl.fds = NULL;
		ioctl.attrs = NULL;
		ioctl.crc = NULL;

		VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
				FASTRPC_MODE_PARALLEL, 1, &ioctl)));
		if (err)
			goto bail;
	} else if (flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		if (me->channel[fl->cid].rhvm.vmid) {
			err = hyp_assign_phys(phys,
					(uint64_t)size,
					me->channel[fl->cid].rhvm.vmid,
					me->channel[fl->cid].rhvm.vmcount,
					destVM, destVMperm, 1);
			if (err) {
				pr_err("adsprpc: %s: %s: rh hyp unassign failed with %d for phys 0x%llx, size %zd\n",
					__func__, current->comm,
					err, phys, size);
				goto bail;
			}
		}
	}

bail:
	return err;
}

static int fastrpc_munmap_on_dsp(struct fastrpc_file *fl, uintptr_t raddr,
				uint64_t phys, size_t size, uint32_t flags)
{
	int err = 0;

	VERIFY(err, 0 == (err = fastrpc_unmap_on_dsp(fl, raddr, phys,
						size, flags)));
	if (err)
		goto bail;
	if (flags == ADSP_MMAP_HEAP_ADDR ||
				flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		VERIFY(err, !fastrpc_munmap_on_dsp_rh(fl, phys, size, flags));
		if (err)
			goto bail;
	}
bail:
	return err;
}

static int fastrpc_mmap_remove_ssr(struct fastrpc_file *fl)
{
	struct fastrpc_mmap *match = NULL, *map = NULL;
	struct hlist_node *n = NULL;
	int err = 0, ret = 0;
	struct fastrpc_apps *me = &gfa;
	struct ramdump_segment *ramdump_segments_rh = NULL;

	VERIFY(err, fl->cid == RH_CID);
	if (err)
		goto bail;
	do {
		match = NULL;
		spin_lock(&me->hlock);
		hlist_for_each_entry_safe(map, n, &me->maps, hn) {
			match = map;
			hlist_del_init(&map->hn);
			break;
		}
		spin_unlock(&me->hlock);

		if (match) {
			err = fastrpc_munmap_on_dsp_rh(fl, match->phys,
						match->size, match->flags);
			if (err)
				goto bail;
			if (me->channel[RH_CID].ramdumpenabled) {
				ramdump_segments_rh = kcalloc(1,
				sizeof(struct ramdump_segment), GFP_KERNEL);
				if (ramdump_segments_rh) {
					ramdump_segments_rh->address =
					match->phys;
					ramdump_segments_rh->size = match->size;
					ret = do_elf_ramdump(
					 me->channel[RH_CID].rh_dump_dev,
					 ramdump_segments_rh, 1);
					if (ret < 0)
						pr_err("adsprpc: %s: unable to dump heap (err %d)\n",
							__func__, ret);
					kfree(ramdump_segments_rh);
				}
			}
			fastrpc_mmap_free(match, 0);
		}
	} while (match);
bail:
	if (err && match)
		fastrpc_mmap_add(match);
	return err;
}

static int fastrpc_mmap_remove_pdr(struct fastrpc_file *fl)
{
	struct fastrpc_apps *me = &gfa;
	int session = 0, err = 0, cid = -1;

	err = fastrpc_get_spd_session(fl->servloc_name,
			&session, &cid);
	if (err)
		goto bail;
	VERIFY(err, cid == fl->cid);
	if (err)
		goto bail;
	if (!me->channel[fl->cid].spd[session].ispdup &&
		me->channel[fl->cid].spd[session].pdrhandle) {
		err = -ENOTCONN;
		goto bail;
	}
	if (me->channel[fl->cid].spd[session].pdrcount !=
		me->channel[fl->cid].spd[session].prevpdrcount) {
		err = fastrpc_mmap_remove_ssr(fl);
		if (err)
			pr_warn("adsprpc: %s: %s: failed to unmap remote heap (err %d)\n",
					__func__, current->comm, err);
		me->channel[fl->cid].spd[session].prevpdrcount =
				me->channel[fl->cid].spd[session].pdrcount;
	}
bail:
	return err;
}

static inline void get_fastrpc_ioctl_mmap_64(
			struct fastrpc_ioctl_mmap_64 *mmap64,
			struct fastrpc_ioctl_mmap *immap)
{
	immap->fd = mmap64->fd;
	immap->flags = mmap64->flags;
	immap->vaddrin = (uintptr_t)mmap64->vaddrin;
	immap->size = mmap64->size;
}

static inline void put_fastrpc_ioctl_mmap_64(
			struct fastrpc_ioctl_mmap_64 *mmap64,
			struct fastrpc_ioctl_mmap *immap)
{
	mmap64->vaddrout = (uint64_t)immap->vaddrout;
}

static inline void get_fastrpc_ioctl_munmap_64(
			struct fastrpc_ioctl_munmap_64 *munmap64,
			struct fastrpc_ioctl_munmap *imunmap)
{
	imunmap->vaddrout = (uintptr_t)munmap64->vaddrout;
	imunmap->size = munmap64->size;
}

static int fastrpc_internal_munmap(struct fastrpc_file *fl,
				   struct fastrpc_ioctl_munmap *ud)
{
	int err = 0;
	struct fastrpc_mmap *map = NULL;
	struct fastrpc_buf *rbuf = NULL, *free = NULL;
	struct hlist_node *n;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		pr_err("adsprpc: ERROR: %s: user application %s trying to unmap without initialization\n",
			 __func__, current->comm);
		err = EBADR;
		return err;
	}
	mutex_lock(&fl->internal_map_mutex);

	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(rbuf, n, &fl->remote_bufs, hn_rem) {
		if (rbuf->raddr && ((rbuf->flags == ADSP_MMAP_ADD_PAGES) ||
				    (rbuf->flags == ADSP_MMAP_ADD_PAGES_LLC))) {
			if ((rbuf->raddr == ud->vaddrout) &&
				(rbuf->size == ud->size)) {
				free = rbuf;
				break;
			}
		}
	}
	spin_unlock(&fl->hlock);

	if (free) {
		VERIFY(err, !fastrpc_munmap_on_dsp(fl, free->raddr,
			free->phys, free->size, free->flags));
		if (err)
			goto bail;
		fastrpc_buf_free(rbuf, 0);
		mutex_unlock(&fl->internal_map_mutex);
		return err;
	}

	mutex_lock(&fl->map_mutex);
	VERIFY(err, !fastrpc_mmap_remove(fl, ud->vaddrout, ud->size, &map));
	mutex_unlock(&fl->map_mutex);
	if (err)
		goto bail;
	VERIFY(err, !fastrpc_munmap_on_dsp(fl, map->raddr,
				map->phys, map->size, map->flags));
	if (err)
		goto bail;
	mutex_lock(&fl->map_mutex);
	fastrpc_mmap_free(map, 0);
	mutex_unlock(&fl->map_mutex);
bail:
	if (err && map) {
		mutex_lock(&fl->map_mutex);
		fastrpc_mmap_add(map);
		mutex_unlock(&fl->map_mutex);
	}
	mutex_unlock(&fl->internal_map_mutex);
	return err;
}

/*
 *	fastrpc_internal_munmap_fd can only be used for buffers
 *	mapped with persist attributes. This can only be called
 *	once for any persist buffer
 */
static int fastrpc_internal_munmap_fd(struct fastrpc_file *fl,
				struct fastrpc_ioctl_munmap_fd *ud)
{
	int err = 0;
	struct fastrpc_mmap *map = NULL;

	VERIFY(err, (fl && ud));
	if (err)
		return err;
	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		pr_err("adsprpc: ERROR: %s: user application %s trying to unmap without initialization\n",
			__func__, current->comm);
		err = EBADR;
		return err;
	}
	mutex_lock(&fl->internal_map_mutex);
	mutex_lock(&fl->map_mutex);
	if (fastrpc_mmap_find(fl, ud->fd, ud->va, ud->len, 0, 0, &map)) {
		pr_err("adsprpc: mapping not found to unmap fd 0x%x, va 0x%llx, len 0x%x\n",
			ud->fd, (unsigned long long)ud->va,
			(unsigned int)ud->len);
		err = -1;
		mutex_unlock(&fl->map_mutex);
		goto bail;
	}
	if (map && (map->attr & FASTRPC_ATTR_KEEP_MAP)) {
		map->attr = map->attr & (~FASTRPC_ATTR_KEEP_MAP);
		fastrpc_mmap_free(map, 0);
	}
	mutex_unlock(&fl->map_mutex);
bail:
	mutex_unlock(&fl->internal_map_mutex);
	return err;
}


static int fastrpc_internal_mmap(struct fastrpc_file *fl,
				 struct fastrpc_ioctl_mmap *ud)
{
	struct fastrpc_mmap *map = NULL;
	struct fastrpc_buf *rbuf = NULL;
	unsigned long dma_attr = 0;
	uintptr_t raddr = 0;
	int err = 0;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		pr_err("adsprpc: ERROR: %s: user application %s trying to map without initialization\n",
			__func__, current->comm);
		err = EBADR;
		return err;
	}
	mutex_lock(&fl->internal_map_mutex);
	if ((ud->flags == ADSP_MMAP_ADD_PAGES) ||
	    (ud->flags == ADSP_MMAP_ADD_PAGES_LLC)) {
		if (ud->vaddrin) {
			err = EINVAL;
			pr_err("adsprpc: %s: %s: ERROR: adding user allocated pages is not supported\n",
					current->comm, __func__);
			goto bail;
		}
		dma_attr = DMA_ATTR_EXEC_MAPPING |
					DMA_ATTR_DELAYED_UNMAP |
					DMA_ATTR_NO_KERNEL_MAPPING |
					DMA_ATTR_FORCE_NON_COHERENT;
		if (ud->flags == ADSP_MMAP_ADD_PAGES_LLC)
			dma_attr |= DMA_ATTR_IOMMU_USE_UPSTREAM_HINT;
		err = fastrpc_buf_alloc(fl, ud->size, dma_attr, ud->flags,
								1, &rbuf);
		if (err)
			goto bail;
		err = fastrpc_mmap_on_dsp(fl, ud->flags, 0,
				rbuf->phys, rbuf->size, &raddr);
		if (err)
			goto bail;
		rbuf->raddr = raddr;
	} else {
		uintptr_t va_to_dsp;

		mutex_lock(&fl->map_mutex);
		VERIFY(err, !fastrpc_mmap_create(fl, ud->fd, 0,
				(uintptr_t)ud->vaddrin, ud->size,
				 ud->flags, &map));
		mutex_unlock(&fl->map_mutex);
		if (err)
			goto bail;

		if (ud->flags == ADSP_MMAP_HEAP_ADDR ||
				ud->flags == ADSP_MMAP_REMOTE_HEAP_ADDR)
			va_to_dsp = 0;
		else
			va_to_dsp = (uintptr_t)map->va;
		VERIFY(err, 0 == fastrpc_mmap_on_dsp(fl, ud->flags, va_to_dsp,
				map->phys, map->size, &raddr));
		if (err)
			goto bail;
		map->raddr = raddr;
	}
	ud->vaddrout = raddr;
 bail:
	if (err) {
		if (map) {
			mutex_lock(&fl->map_mutex);
			fastrpc_mmap_free(map, 0);
			mutex_unlock(&fl->map_mutex);
		}
		if (!IS_ERR_OR_NULL(rbuf))
			fastrpc_buf_free(rbuf, 0);
	}
	mutex_unlock(&fl->internal_map_mutex);
	return err;
}

static void fastrpc_context_list_dtor(struct fastrpc_file *fl);

static int fastrpc_session_alloc_locked(struct fastrpc_channel_ctx *chan,
			int secure, struct fastrpc_session_ctx **session)
{
	struct fastrpc_apps *me = &gfa;
	uint64_t idx = 0;
	int err = 0;

	if (chan->sesscount) {
		for (idx = 0; idx < chan->sesscount; ++idx) {
			if (!chan->session[idx].used &&
				chan->session[idx].smmu.secure == secure) {
				chan->session[idx].used = 1;
				break;
			}
		}
		if (idx >= chan->sesscount) {
			err = EUSERS;
			pr_err("adsprpc: ERROR %d: %s: max concurrent sessions limit (%d) already reached on %s\n",
				err, __func__, chan->sesscount, chan->subsys);
			goto bail;
		}
		chan->session[idx].smmu.faults = 0;
	} else {
		VERIFY(err, me->dev != NULL);
		if (err) {
			err = -ECONNREFUSED;
			goto bail;
		}
		chan->session[0].dev = me->dev;
		chan->session[0].smmu.dev = me->dev;
	}

	*session = &chan->session[idx];
 bail:
	return err;
}

static inline int get_cid_from_rpdev(struct rpmsg_device *rpdev)
{
	int err = 0, cid = -1;

	VERIFY(err, !IS_ERR_OR_NULL(rpdev));
	if (err)
		return -EINVAL;

	if (!strcmp(rpdev->dev.parent->of_node->name, "cdsp"))
		cid = CDSP_DOMAIN_ID;
	else if (!strcmp(rpdev->dev.parent->of_node->name, "adsp"))
		cid = ADSP_DOMAIN_ID;
	else if (!strcmp(rpdev->dev.parent->of_node->name, "dsps"))
		cid = SDSP_DOMAIN_ID;
	else if (!strcmp(rpdev->dev.parent->of_node->name, "mdsp"))
		cid = MDSP_DOMAIN_ID;

	return cid;
}

static int fastrpc_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int err = 0;
	int cid = -1;

	VERIFY(err, !IS_ERR_OR_NULL(rpdev));
	if (err)
		return -EINVAL;

	cid = get_cid_from_rpdev(rpdev);
	VERIFY(err, cid >= ADSP_DOMAIN_ID && cid < NUM_CHANNELS);
	if (err)
		goto bail;
	mutex_lock(&gcinfo[cid].rpmsg_mutex);
	gcinfo[cid].rpdev = rpdev;
	mutex_unlock(&gcinfo[cid].rpmsg_mutex);
	pr_info("adsprpc: %s: opened rpmsg channel for %s\n",
		__func__, gcinfo[cid].subsys);

#if IS_ENABLED(CONFIG_ADSPRPC_DEBUG)
	if (!gcinfo[cid].ipc_log_ctx)
		gcinfo[cid].ipc_log_ctx =
			ipc_log_context_create(FASTRPC_GLINK_LOG_PAGES,
				gcinfo[cid].name, 0);
	if (!gcinfo[cid].ipc_log_ctx)
		pr_warn("adsprpc: %s: failed to create IPC log context for %s\n",
			__func__, gcinfo[cid].subsys);
	else
		pr_info("adsprpc: %s: enabled IPC logging for %s\n",
			__func__, gcinfo[cid].subsys);
#endif
bail:
	if (err)
		pr_err("adsprpc: rpmsg probe of %s cid %d failed\n",
			rpdev->dev.parent->of_node->name, cid);
	return err;
}

static void fastrpc_rpmsg_remove(struct rpmsg_device *rpdev)
{
	int err = 0;
	int cid = -1;
	struct fastrpc_apps *me = &gfa;

	VERIFY(err, !IS_ERR_OR_NULL(rpdev));
	if (err)
		return;

	cid = get_cid_from_rpdev(rpdev);
	VERIFY(err, cid >= ADSP_DOMAIN_ID && cid < NUM_CHANNELS);
	if (err)
		goto bail;
	mutex_lock(&gcinfo[cid].rpmsg_mutex);
	gcinfo[cid].rpdev = NULL;
	mutex_unlock(&gcinfo[cid].rpmsg_mutex);
	fastrpc_notify_drivers(me, cid);
	pr_info("adsprpc: %s: closed rpmsg channel of %s\n",
		__func__, gcinfo[cid].subsys);
bail:
	if (err)
		pr_err("adsprpc: rpmsg remove of %s cid %d failed\n",
			rpdev->dev.parent->of_node->name, cid);
}

static int fastrpc_rpmsg_callback(struct rpmsg_device *rpdev, void *data,
	int len, void *priv, u32 addr)
{
	struct smq_invoke_rsp *rsp = (struct smq_invoke_rsp *)data;
	struct smq_invoke_rspv2 *rspv2 = NULL;
	struct fastrpc_apps *me = &gfa;
	uint32_t index, rspFlags = 0, earlyWakeTime = 0;
	int err = 0, cid = -1;
	struct fastrpc_channel_ctx *chan = NULL;
	unsigned long irq_flags = 0;

	cid = get_cid_from_rpdev(rpdev);
	VERIFY(err, (cid >= ADSP_DOMAIN_ID && cid <= NUM_CHANNELS));
	if (err)
		goto bail;

	chan = &me->channel[cid];
	VERIFY(err, (rsp && len >= sizeof(*rsp)));
	if (err)
		goto bail;

	if (len >= sizeof(struct smq_invoke_rspv2))
		rspv2 = (struct smq_invoke_rspv2 *)data;

	if (rspv2) {
		earlyWakeTime = rspv2->earlyWakeTime;
		rspFlags = rspv2->flags;
	}
	trace_fastrpc_rpmsg_response(cid, rsp->ctx,
		rsp->retval, rspFlags, earlyWakeTime);
#if IS_ENABLED(CONFIG_ADSPRPC_DEBUG)
	if (cid >= ADSP_DOMAIN_ID && cid < NUM_CHANNELS) {
		LOG_FASTRPC_GLINK_MSG(gcinfo[cid].ipc_log_ctx,
		"recvd pkt %pK (sz %d): ctx 0x%llx, retVal %d, flags %u, earlyWake %u",
		data, len, rsp->ctx, rsp->retval, rspFlags, earlyWakeTime);
	}
#endif

	index = (uint32_t)((rsp->ctx & FASTRPC_CTXID_MASK) >> 4);
	VERIFY(err, index < FASTRPC_CTX_MAX);
	if (err)
		goto bail;

	spin_lock_irqsave(&chan->ctxlock, irq_flags);
	VERIFY(err, !IS_ERR_OR_NULL(chan->ctxtable[index]));
	if (err)
		goto bail_unlock;

	VERIFY(err, ((chan->ctxtable[index]->ctxid ==
		(rsp->ctx & ~CONTEXT_PD_CHECK)) &&
			chan->ctxtable[index]->magic ==
				FASTRPC_CTX_MAGIC));
	if (err)
		goto bail_unlock;

	if (rspv2) {
		VERIFY(err, rspv2->version == FASTRPC_RSP_VERSION2);
		if (err)
			goto bail_unlock;
	}
	context_notify_user(chan->ctxtable[index], rsp->retval,
				 rspFlags, earlyWakeTime);
bail_unlock:
	spin_unlock_irqrestore(&chan->ctxlock, irq_flags);
bail:
	if (err)
		pr_err("adsprpc: ERROR: %s: invalid response (data %pK, len %d) from remote subsystem (err %d)\n",
				__func__, data, len, err);

	return err;
}

static int fastrpc_session_alloc(struct fastrpc_channel_ctx *chan, int secure,
					struct fastrpc_session_ctx **session)
{
	int err = 0;

	mutex_lock(&chan->smd_mutex);
	if (!*session)
		err = fastrpc_session_alloc_locked(chan, secure, session);
	mutex_unlock(&chan->smd_mutex);
	return err;
}

static void fastrpc_session_free(struct fastrpc_channel_ctx *chan,
				struct fastrpc_session_ctx *session)
{
	mutex_lock(&chan->smd_mutex);
	session->used = 0;
	mutex_unlock(&chan->smd_mutex);
}

static int fastrpc_file_free(struct fastrpc_file *fl)
{
	struct hlist_node *n = NULL;
	struct fastrpc_mmap *map = NULL, *lmap = NULL;
	struct fastrpc_perf *perf = NULL, *fperf = NULL;
	int cid;

	if (!fl)
		return 0;
	cid = fl->cid;

	(void)fastrpc_release_current_dsp_process(fl);

	spin_lock(&fl->apps->hlock);
	hlist_del_init(&fl->hn);
	spin_unlock(&fl->apps->hlock);
	if (fl->wake_source)
		wakeup_source_unregister(fl->wake_source);
	kfree(fl->debug_buf);

	if (!fl->sctx) {
		kfree(fl);
		return 0;
	}
	spin_lock(&fl->hlock);
	fl->file_close = 1;
	spin_unlock(&fl->hlock);
	if (!IS_ERR_OR_NULL(fl->init_mem))
		fastrpc_buf_free(fl->init_mem, 0);
	fastrpc_context_list_dtor(fl);
	fastrpc_cached_buf_list_free(fl);
	mutex_lock(&fl->map_mutex);
	do {
		lmap = NULL;
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			hlist_del_init(&map->hn);
			lmap = map;
			break;
		}
		fastrpc_mmap_free(lmap, 1);
	} while (lmap);
	mutex_unlock(&fl->map_mutex);

	if (fl->sctx)
		fastrpc_session_free(&fl->apps->channel[cid], fl->sctx);
	if (fl->secsctx)
		fastrpc_session_free(&fl->apps->channel[cid], fl->secsctx);

	mutex_lock(&fl->perf_mutex);
	do {
		struct hlist_node *pn = NULL;

		fperf = NULL;
		hlist_for_each_entry_safe(perf, pn, &fl->perf, hn) {
			hlist_del_init(&perf->hn);
			fperf = perf;
			break;
		}
		kfree(fperf);
	} while (fperf);
	fastrpc_remote_buf_list_free(fl);
	mutex_unlock(&fl->perf_mutex);
	mutex_destroy(&fl->perf_mutex);
	mutex_destroy(&fl->map_mutex);
	mutex_destroy(&fl->internal_map_mutex);
	kfree(fl);
	return 0;
}

static int fastrpc_device_release(struct inode *inode, struct file *file)
{
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;

	if (fl) {
		if (fl->qos_request && pm_qos_request_active(&fl->pm_qos_req))
			pm_qos_remove_request(&fl->pm_qos_req);
		if (fl->debugfs_file != NULL)
			debugfs_remove(fl->debugfs_file);
		fastrpc_file_free(fl);
		file->private_data = NULL;
	}
	return 0;
}

static int fastrpc_debugfs_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t fastrpc_debugfs_read(struct file *filp, char __user *buffer,
					 size_t count, loff_t *position) {
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_file *fl = filp->private_data;
	struct hlist_node *n;
	struct fastrpc_buf *buf = NULL;
	struct fastrpc_mmap *map = NULL;
	struct fastrpc_mmap *gmaps = NULL;
	struct smq_invoke_ctx *ictx = NULL;
	struct fastrpc_channel_ctx *chan = NULL;
	unsigned int len = 0;
	int i, j, sess_used = 0, ret = 0;
	char *fileinfo = NULL;
	char single_line[UL_SIZE] = "----------------";
	char title[UL_SIZE] = "=========================";

	fileinfo = kzalloc(DEBUGFS_SIZE, GFP_KERNEL);
	if (!fileinfo)
		goto bail;
	if (fl == NULL) {
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s %s %s\n", title, " CHANNEL INFO ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-7s|%-10s|%-14s|%-9s|%-13s\n",
			"subsys", "sesscount", "issubsystemup",
			"ssrcount", "session_used");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"-%s%s%s%s-\n", single_line, single_line,
			single_line, single_line);
		for (i = 0; i < NUM_CHANNELS; i++) {
			sess_used = 0;
			chan = &gcinfo[i];
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len, "%-7s", chan->subsys);
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len, "|%-10u",
				chan->sesscount);
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len, "|%-14d",
				chan->issubsystemup);
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len, "|%-9u",
				chan->ssrcount);
			for (j = 0; j < chan->sesscount; j++) {
				sess_used += chan->session[j].used;
			}
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len, "|%-13d\n", sess_used);
		}
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s%s%s\n", "=============",
			" CMA HEAP ", "==============");
		len += scnprintf(fileinfo + len,
			DEBUGFS_SIZE - len, "%-20s|%-20s\n", "addr", "size");
		len += scnprintf(fileinfo + len,
			DEBUGFS_SIZE - len, "--%s%s---\n",
			single_line, single_line);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"0x%-18llX", me->range.addr);
		len += scnprintf(fileinfo + len,
			DEBUGFS_SIZE - len, "|0x%-18llX\n", me->range.size);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n==========%s %s %s===========\n",
			title, " GMAPS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-20s|%-20s|%-20s\n",
			"fd", "phys", "size", "va");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(gmaps, n, &me->maps, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"%-20d|0x%-18llX|0x%-18X|0x%-20lX\n\n",
				gmaps->fd, gmaps->phys,
				(uint32_t)gmaps->size,
				gmaps->va);
		}
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-20s|%-20s|%-20s\n",
			"len", "refs", "raddr", "flags");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(gmaps, n, &me->maps, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-18X|%-20d|%-20lu|%-20u\n",
				(uint32_t)gmaps->len, gmaps->refs,
				gmaps->raddr, gmaps->flags);
		}
	} else {
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s %13s %d\n", "cid", ":", fl->cid);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %12s %d\n", "tgid", ":", fl->tgid);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %7s %d\n", "sessionid", ":", fl->sessionid);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %8s %u\n", "ssrcount", ":", fl->ssrcount);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %14s %d\n", "pd", ":", fl->pd);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %9s %s\n", "servloc_name", ":", fl->servloc_name);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %6s %d\n", "file_close", ":", fl->file_close);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %9s %d\n", "profile", ":", fl->profile);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %3s %d\n", "smmu.coherent", ":",
			fl->sctx->smmu.coherent);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %4s %d\n", "smmu.enabled", ":",
			fl->sctx->smmu.enabled);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %9s %d\n", "smmu.cb", ":", fl->sctx->smmu.cb);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %5s %d\n", "smmu.secure", ":",
			fl->sctx->smmu.secure);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s %5s %d\n", "smmu.faults", ":",
			fl->sctx->smmu.faults);

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n=======%s %s %s======\n", title,
			" LIST OF MAPS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-20s|%-20s\n", "va", "phys", "size");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n",
			single_line, single_line, single_line,
			single_line, single_line);
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-20lX|0x%-20llX|0x%-20zu\n\n",
				map->va, map->phys,
				map->size);
		}
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-20s|%-20s|%-20s\n",
			"len", "refs",
			"raddr", "uncached");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n",
			single_line, single_line, single_line,
			single_line, single_line);
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"%-20zu|%-20d|0x%-20lX|%-20d\n\n",
				map->len, map->refs, map->raddr,
				map->uncached);
		}
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-20s\n", "secure", "attr");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n",
			single_line, single_line, single_line,
			single_line, single_line);
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"%-20d|0x%-20lX\n\n",
				map->secure, map->attr);
		}

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n======%s %s %s======\n", title,
			" LIST OF BUFS ", title);
		spin_lock(&fl->hlock);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-19s|%-19s|%-19s\n",
			"virt", "phys", "size");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len,
				"0x%-17p|0x%-17llX|%-19zu\n",
				buf->virt, (uint64_t)buf->phys, buf->size);
		}

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s %s %s\n", title,
			" LIST OF PENDING SMQCONTEXTS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-10s|%-10s|%-10s|%-20s\n",
			"sc", "pid", "tgid", "used", "ctxid");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(ictx, n, &fl->clst.pending, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-18X|%-10d|%-10d|%-10zu|0x%-20llX\n\n",
				ictx->sc, ictx->pid, ictx->tgid,
				ictx->used, ictx->ctxid);
		}

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s %s %s\n", title,
			" LIST OF INTERRUPTED SMQCONTEXTS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-10s|%-10s|%-10s|%-20s\n",
			"sc", "pid", "tgid", "used", "ctxid");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(ictx, n, &fl->clst.interrupted, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20u|%-20d|%-20d|%-20zu|0x%-20llX\n\n",
			ictx->sc, ictx->pid, ictx->tgid,
			ictx->used, ictx->ctxid);
		}
		spin_unlock(&fl->hlock);
	}
	if (len > DEBUGFS_SIZE)
		len = DEBUGFS_SIZE;
	ret = simple_read_from_buffer(buffer, count, position, fileinfo, len);
	kfree(fileinfo);
bail:
	return ret;
}

static const struct file_operations debugfs_fops = {
	.open = fastrpc_debugfs_open,
	.read = fastrpc_debugfs_read,
};

static int fastrpc_channel_open(struct fastrpc_file *fl)
{
	struct fastrpc_apps *me = &gfa;
	int cid = -1, err = 0;

	VERIFY(err, fl && fl->sctx && fl->cid >= 0 && fl->cid < NUM_CHANNELS);
	if (err) {
		pr_err("adsprpc: ERROR: %s: kernel session not initialized yet for %s\n",
			__func__, current->comm);
		err = EBADR;
		return err;
	}
	cid = fl->cid;

	mutex_lock(&me->channel[cid].rpmsg_mutex);
	VERIFY(err, NULL != me->channel[cid].rpdev);
	if (err) {
		err = -ENOTCONN;
		mutex_unlock(&me->channel[cid].rpmsg_mutex);
		goto bail;
	}
	mutex_unlock(&me->channel[cid].rpmsg_mutex);

	mutex_lock(&me->channel[cid].smd_mutex);
	if (me->channel[cid].ssrcount !=
				 me->channel[cid].prevssrcount) {
		if (!me->channel[cid].issubsystemup) {
			err = -ENOTCONN;
			mutex_unlock(&me->channel[cid].smd_mutex);
			goto bail;
		}
	}
	fl->ssrcount = me->channel[cid].ssrcount;

	if (cid == ADSP_DOMAIN_ID && me->channel[cid].ssrcount !=
			 me->channel[cid].prevssrcount) {
		mutex_lock(&fl->map_mutex);
		err = fastrpc_mmap_remove_ssr(fl);
		if (err)
			pr_warn("adsprpc: %s: %s: failed to unmap remote heap for %s (err %d)\n",
					__func__, current->comm,
					me->channel[cid].subsys, err);
		mutex_unlock(&fl->map_mutex);
		me->channel[cid].prevssrcount =
					me->channel[cid].ssrcount;
	}
	mutex_unlock(&me->channel[cid].smd_mutex);

bail:
	return err;
}

static int fastrpc_device_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct dentry *debugfs_file;
	struct fastrpc_file *fl = NULL;
	struct fastrpc_apps *me = &gfa;
	char strpid[PID_SIZE];
	int buf_size = 0;

	/*
	 * Indicates the device node opened
	 * MINOR_NUM_DEV or MINOR_NUM_SECURE_DEV
	 */
	int dev_minor = MINOR(inode->i_rdev);

	VERIFY(err, ((dev_minor == MINOR_NUM_DEV) ||
			(dev_minor == MINOR_NUM_SECURE_DEV)));
	if (err) {
		pr_err("adsprpc: Invalid dev minor num %d\n", dev_minor);
		return err;
	}

	VERIFY(err, NULL != (fl = kzalloc(sizeof(*fl), GFP_KERNEL)));
	if (err)
		return err;

	snprintf(strpid, PID_SIZE, "%d", current->pid);
	buf_size = strlen(current->comm) + strlen("_") + strlen(strpid) + 1;
	VERIFY(err, NULL != (fl->debug_buf = kzalloc(buf_size, GFP_KERNEL)));
	if (err) {
		kfree(fl);
		return err;
	}
	snprintf(fl->debug_buf, UL_SIZE, "%.10s%s%d",
			current->comm, "_", current->pid);
	debugfs_file = debugfs_create_file(fl->debug_buf, 0644, debugfs_root,
						fl, &debugfs_fops);

	fl->wake_source = wakeup_source_register(fl->debug_buf);
	if (IS_ERR_OR_NULL(fl->wake_source)) {
		pr_err("adsprpc: Error: %s: %s: wakeup_source_register failed with err %ld\n",
			current->comm, __func__, PTR_ERR(fl->wake_source));
	}
	context_list_ctor(&fl->clst);
	spin_lock_init(&fl->hlock);
	INIT_HLIST_HEAD(&fl->maps);
	INIT_HLIST_HEAD(&fl->perf);
	INIT_HLIST_HEAD(&fl->cached_bufs);
	INIT_HLIST_HEAD(&fl->remote_bufs);
	INIT_HLIST_NODE(&fl->hn);
	fl->sessionid = 0;
	fl->tgid = current->tgid;
	fl->apps = me;
	fl->mode = FASTRPC_MODE_SERIAL;
	fl->cid = -1;
	fl->dev_minor = dev_minor;
	fl->init_mem = NULL;
	if (debugfs_file != NULL)
		fl->debugfs_file = debugfs_file;
	memset(&fl->perf, 0, sizeof(fl->perf));
	fl->qos_request = 0;
	fl->dsp_proc_init = 0;
	filp->private_data = fl;
	mutex_init(&fl->internal_map_mutex);
	mutex_init(&fl->map_mutex);
	spin_lock(&me->hlock);
	hlist_add_head(&fl->hn, &me->drivers);
	spin_unlock(&me->hlock);
	mutex_init(&fl->perf_mutex);
	return 0;
}

static int fastrpc_get_info(struct fastrpc_file *fl, uint32_t *info)
{
	int err = 0;
	uint32_t cid;

	VERIFY(err, fl != NULL);
	if (err)
		goto bail;
	if (fl->cid == -1) {
		cid = *info;
		VERIFY(err, cid < NUM_CHANNELS);
		if (err)
			goto bail;
		/* Check to see if the device node is non-secure */
		if (fl->dev_minor == MINOR_NUM_DEV) {
			/*
			 * For non secure device node check and make sure that
			 * the channel allows non-secure access
			 * If not, bail. Session will not start.
			 * cid will remain -1 and client will not be able to
			 * invoke any other methods without failure
			 */
			if (fl->apps->channel[cid].secure == SECURE_CHANNEL) {
				err = -EACCES;
				goto bail;
			}
		}
		fl->cid = cid;
		fl->ssrcount = fl->apps->channel[cid].ssrcount;
		mutex_lock(&fl->apps->channel[cid].smd_mutex);
		err = fastrpc_session_alloc_locked(&fl->apps->channel[cid],
				0, &fl->sctx);
		mutex_unlock(&fl->apps->channel[cid].smd_mutex);
		if (err)
			goto bail;
	}
	VERIFY(err, fl->sctx != NULL);
	if (err)
		goto bail;
	*info = (fl->sctx->smmu.enabled ? 1 : 0);
bail:
	return err;
}

static int fastrpc_internal_control(struct fastrpc_file *fl,
					struct fastrpc_ioctl_control *cp)
{
	int err = 0;
	unsigned int latency;
	cpumask_t mask;
	struct fastrpc_apps *me = &gfa;
	u32 len = me->silvercores.corecount, i = 0;

	VERIFY(err, !IS_ERR_OR_NULL(fl) && !IS_ERR_OR_NULL(fl->apps));
	if (err)
		goto bail;
	VERIFY(err, !IS_ERR_OR_NULL(cp));
	if (err)
		goto bail;

	switch (cp->req) {
	case FASTRPC_CONTROL_LATENCY:
		latency = cp->lp.enable == FASTRPC_LATENCY_CTRL_ENB ?
			fl->apps->latency : PM_QOS_DEFAULT_VALUE;
		VERIFY(err, latency != 0);
		if (err)
			goto bail;
		cpumask_clear(&mask);
		for (i = 0; i < len; i++)
			cpumask_set_cpu(me->silvercores.coreno[i], &mask);
		fl->pm_qos_req.type = PM_QOS_REQ_AFFINE_CORES;
		cpumask_copy(&fl->pm_qos_req.cpus_affine, &mask);

		if (!fl->qos_request) {
			pm_qos_add_request(&fl->pm_qos_req,
				PM_QOS_CPU_DMA_LATENCY, latency);
			fl->qos_request = 1;
		} else
			pm_qos_update_request(&fl->pm_qos_req, latency);

		/* Ensure CPU feature map updated to DSP for early WakeUp */
		fastrpc_send_cpuinfo_to_dsp(fl);
		break;
	case FASTRPC_CONTROL_KALLOC:
		cp->kalloc.kalloc_support = 1;
		break;
	case FASTRPC_CONTROL_WAKELOCK:
		if (fl->dev_minor != MINOR_NUM_SECURE_DEV) {
			pr_err("adsprpc: %s: %s: PM voting not allowed for non-secure device node %d\n",
				current->comm, __func__, fl->dev_minor);
			err = -EPERM;
			goto bail;
		}
		fl->wake_enable = cp->wp.enable;
		break;
	case FASTRPC_CONTROL_PM:
		if (!fl->wake_enable) {
			/* Kernel PM voting not requested by this application */
			err = -EACCES;
			goto bail;
		}
		if (cp->pm.timeout > MAX_PM_TIMEOUT_MS)
			fl->ws_timeout = MAX_PM_TIMEOUT_MS;
		else
			fl->ws_timeout = cp->pm.timeout;
		fastrpc_pm_awake(fl);
		break;
	default:
		err = -EBADRQC;
		break;
	}
bail:
	return err;
}

static int fastrpc_check_pd_status(struct fastrpc_file *fl, char *sloc_name)
{
	int err = 0, session = -1, cid = -1;
	struct fastrpc_apps *me = &gfa;

	if (fl->servloc_name && sloc_name
		&& !strcmp(fl->servloc_name, sloc_name)) {
		err = fastrpc_get_spd_session(sloc_name, &session, &cid);
		if (err || cid != fl->cid)
			goto bail;
		if (!me->channel[cid].spd[session].ispdup) {
			err = -ENOTCONN;
			goto bail;
		}
	}
bail:
	return err;
}

static int fastrpc_setmode(unsigned long ioctl_param,
				struct fastrpc_file *fl)
{
	int err = 0;

	switch ((uint32_t)ioctl_param) {
	case FASTRPC_MODE_PARALLEL:
	case FASTRPC_MODE_SERIAL:
		fl->mode = (uint32_t)ioctl_param;
		break;
	case FASTRPC_MODE_PROFILE:
		fl->profile = (uint32_t)ioctl_param;
		break;
	case FASTRPC_MODE_SESSION:
		fl->sessionid = 1;
		fl->tgid |= (1 << SESSION_ID_INDEX);
		break;
	default:
		err = -ENOTTY;
		break;
	}
	return err;
}

static int fastrpc_getperf(struct fastrpc_ioctl_perf *ioctl_perf,
				void *param, struct fastrpc_file *fl)
{
	int err = 0;

	K_COPY_FROM_USER(err, 0, ioctl_perf,
				param, sizeof(*ioctl_perf));
	if (err)
		goto bail;
	ioctl_perf->numkeys = PERF_KEY_MAX;
	if (ioctl_perf->keys) {
		char *keys = PERF_KEYS;

		K_COPY_TO_USER(err, 0, (void *)ioctl_perf->keys,
					 keys, strlen(keys)+1);
		if (err)
			goto bail;
	}
	if (ioctl_perf->data) {
		struct fastrpc_perf *perf = NULL, *fperf = NULL;
		struct hlist_node *n = NULL;

		mutex_lock(&fl->perf_mutex);
		hlist_for_each_entry_safe(perf, n, &fl->perf, hn) {
			if (perf->tid == current->pid) {
				fperf = perf;
				break;
			}
		}

		mutex_unlock(&fl->perf_mutex);

		if (fperf) {
			K_COPY_TO_USER(err, 0,
				(void *)ioctl_perf->data, fperf,
				sizeof(*fperf) - sizeof(struct hlist_node));
		}
	}
	K_COPY_TO_USER(err, 0, param, ioctl_perf, sizeof(*ioctl_perf));
	if (err)
		goto bail;
bail:
	return err;
}

static int fastrpc_control(struct fastrpc_ioctl_control *cp,
				void *param, struct fastrpc_file *fl)
{
	int err = 0;

	K_COPY_FROM_USER(err, 0, cp, param,
			sizeof(*cp));
	if (err)
		goto bail;
	VERIFY(err, 0 == (err = fastrpc_internal_control(fl, cp)));
	if (err)
		goto bail;
	if (cp->req == FASTRPC_CONTROL_KALLOC) {
		K_COPY_TO_USER(err, 0, param, cp, sizeof(*cp));
		if (err)
			goto bail;
	}
bail:
	return err;
}

static int fastrpc_get_dsp_info(struct fastrpc_ioctl_dsp_capabilities *dsp_cap,
				void *param, struct fastrpc_file *fl)
{
	int err = 0;

	K_COPY_FROM_USER(err, 0, dsp_cap, param,
			sizeof(struct fastrpc_ioctl_dsp_capabilities));
	VERIFY(err, dsp_cap->domain < NUM_CHANNELS);
	if (err)
		goto bail;

	err = fastrpc_get_info_from_kernel(dsp_cap, fl);
	if (err)
		goto bail;
	K_COPY_TO_USER(err, 0, param, dsp_cap,
			sizeof(struct fastrpc_ioctl_dsp_capabilities));
bail:
	return err;
}

static long fastrpc_device_ioctl(struct file *file, unsigned int ioctl_num,
				 unsigned long ioctl_param)
{
	union {
		struct fastrpc_ioctl_invoke_crc inv;
		struct fastrpc_ioctl_mmap mmap;
		struct fastrpc_ioctl_mmap_64 mmap64;
		struct fastrpc_ioctl_munmap munmap;
		struct fastrpc_ioctl_munmap_64 munmap64;
		struct fastrpc_ioctl_munmap_fd munmap_fd;
		struct fastrpc_ioctl_init_attrs init;
		struct fastrpc_ioctl_perf perf;
		struct fastrpc_ioctl_control cp;
		struct fastrpc_ioctl_dsp_capabilities dsp_cap;
	} p;
	union {
		struct fastrpc_ioctl_mmap mmap;
		struct fastrpc_ioctl_munmap munmap;
	} i;
	void *param = (char *)ioctl_param;
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;
	int size = 0, err = 0;
	uint32_t info;

	p.inv.fds = NULL;
	p.inv.attrs = NULL;
	p.inv.crc = NULL;

	err = fastrpc_check_pd_status(fl,
			AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME);
	if (err)
		goto bail;

	spin_lock(&fl->hlock);
	if (fl->file_close == 1) {
		err = EBADF;
		pr_warn("adsprpc: fastrpc_device_release is happening, So not sending any new requests to DSP\n");
		spin_unlock(&fl->hlock);
		goto bail;
	}
	spin_unlock(&fl->hlock);

	switch (ioctl_num) {
	case FASTRPC_IOCTL_INVOKE:
		size = sizeof(struct fastrpc_ioctl_invoke);
		/* fall through */
	case FASTRPC_IOCTL_INVOKE_FD:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_fd);
		/* fall through */
	case FASTRPC_IOCTL_INVOKE_ATTRS:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_attrs);
		/* fall through */
	case FASTRPC_IOCTL_INVOKE_CRC:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_crc);
		K_COPY_FROM_USER(err, 0, &p.inv, param, size);
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl, fl->mode,
						0, &p.inv)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MMAP:
		K_COPY_FROM_USER(err, 0, &p.mmap, param,
						sizeof(p.mmap));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_mmap(fl, &p.mmap)));
		if (err)
			goto bail;
		K_COPY_TO_USER(err, 0, param, &p.mmap, sizeof(p.mmap));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MUNMAP:
		K_COPY_FROM_USER(err, 0, &p.munmap, param,
						sizeof(p.munmap));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_munmap(fl,
							&p.munmap)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MMAP_64:
		K_COPY_FROM_USER(err, 0, &p.mmap64, param,
						sizeof(p.mmap64));
		if (err)
			goto bail;
		get_fastrpc_ioctl_mmap_64(&p.mmap64, &i.mmap);
		VERIFY(err, 0 == (err = fastrpc_internal_mmap(fl, &i.mmap)));
		if (err)
			goto bail;
		put_fastrpc_ioctl_mmap_64(&p.mmap64, &i.mmap);
		K_COPY_TO_USER(err, 0, param, &p.mmap64, sizeof(p.mmap64));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MUNMAP_64:
		K_COPY_FROM_USER(err, 0, &p.munmap64, param,
						sizeof(p.munmap64));
		if (err)
			goto bail;
		get_fastrpc_ioctl_munmap_64(&p.munmap64, &i.munmap);
		VERIFY(err, 0 == (err = fastrpc_internal_munmap(fl,
							&i.munmap)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MUNMAP_FD:
		K_COPY_FROM_USER(err, 0, &p.munmap_fd, param,
			sizeof(p.munmap_fd));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_munmap_fd(fl,
			&p.munmap_fd)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_SETMODE:
		err = fastrpc_setmode(ioctl_param, fl);
		break;
	case FASTRPC_IOCTL_GETPERF:
		err = fastrpc_getperf(&p.perf, param, fl);
		break;
	case FASTRPC_IOCTL_CONTROL:
		err = fastrpc_control(&p.cp, param, fl);
		break;
	case FASTRPC_IOCTL_GETINFO:
	    K_COPY_FROM_USER(err, 0, &info, param, sizeof(info));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_get_info(fl, &info)));
		if (err)
			goto bail;
		K_COPY_TO_USER(err, 0, param, &info, sizeof(info));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_INIT:
		p.init.attrs = 0;
		p.init.siglen = 0;
		size = sizeof(struct fastrpc_ioctl_init);
		/* fall through */
	case FASTRPC_IOCTL_INIT_ATTRS:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_init_attrs);
		K_COPY_FROM_USER(err, 0, &p.init, param, size);
		if (err)
			goto bail;
		VERIFY(err, p.init.init.filelen >= 0 &&
			p.init.init.filelen < INIT_FILELEN_MAX);
		if (err)
			goto bail;
		VERIFY(err, p.init.init.memlen >= 0 &&
			p.init.init.memlen < INIT_MEMLEN_MAX);
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_init_process(fl, &p.init)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_GET_DSP_INFO:
		err = fastrpc_get_dsp_info(&p.dsp_cap, param, fl);
		break;
	default:
		err = -ENOTTY;
		pr_info("bad ioctl: %d\n", ioctl_num);
		break;
	}
 bail:
	return err;
}

static int fastrpc_restart_notifier_cb(struct notifier_block *nb,
					unsigned long code,
					void *data)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_channel_ctx *ctx;
	struct notif_data *notifdata = (struct notif_data *)data;
	int cid = -1;

	pr_info("adsprpc: INFO: Entering callback %s code is %d\n",
			__func__, code);

	ctx = container_of(nb, struct fastrpc_channel_ctx, nb);
	cid = ctx - &me->channel[0];
	if (code == SUBSYS_BEFORE_SHUTDOWN) {
		pr_info("adsprpc: %s: %s subsystem is restarting\n",
			__func__, gcinfo[cid].subsys);
		mutex_lock(&me->channel[cid].smd_mutex);
		ctx->ssrcount++;
		ctx->issubsystemup = 0;
		mutex_unlock(&me->channel[cid].smd_mutex);
		if (cid == RH_CID)
			me->staticpd_flags = 0;
	} else if (code == SUBSYS_RAMDUMP_NOTIFICATION) {
		if (cid == RH_CID) {
			if (me->channel[RH_CID].rh_dump_dev &&
					notifdata->enable_ramdump) {
				me->channel[RH_CID].ramdumpenabled = 1;
			}
		}
		pr_info("adsprpc: %s: received RAMDUMP notification for %s\n",
			__func__, gcinfo[cid].subsys);
	} else if (code == SUBSYS_AFTER_POWERUP) {
		pr_info("adsprpc: %s: %s subsystem is up\n",
			__func__, gcinfo[cid].subsys);
		ctx->issubsystemup = 1;
	}

	pr_info("adsprpc: INFO: Exiting callback %s subsystem is %s code is %d\n",
			__func__, gcinfo[cid].subsys, code);
	return NOTIFY_DONE;
}

static int fastrpc_pdr_notifier_cb(struct notifier_block *pdrnb,
					unsigned long code,
					void *data)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_static_pd *spd;
	struct notif_data *notifdata = (struct notif_data *)data;

	spd = container_of(pdrnb, struct fastrpc_static_pd, pdrnb);
	if (code == SERVREG_NOTIF_SERVICE_STATE_DOWN_V01) {
		pr_info("adsprpc: %s: %s (%s) is down for PDR on %s\n",
			__func__, spd->spdname, spd->servloc_name,
			gcinfo[spd->cid].subsys);
		mutex_lock(&me->channel[spd->cid].smd_mutex);
		spd->pdrcount++;
		spd->ispdup = 0;
		mutex_unlock(&me->channel[spd->cid].smd_mutex);
		if (!strcmp(spd->servloc_name,
				AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME))
			me->staticpd_flags = 0;
		fastrpc_notify_pdr_drivers(me, spd->servloc_name);
	} else if (code == SUBSYS_RAMDUMP_NOTIFICATION) {
		if (spd->cid == RH_CID) {
			if (me->channel[RH_CID].rh_dump_dev &&
					notifdata->enable_ramdump) {
				me->channel[RH_CID].ramdumpenabled = 1;
			}
		}
		pr_info("adsprpc: %s: received %s RAMDUMP notification for %s (%s)\n",
			__func__, gcinfo[spd->cid].subsys,
			spd->spdname, spd->servloc_name);
	} else if (code == SERVREG_NOTIF_SERVICE_STATE_UP_V01) {
		pr_info("adsprpc: %s: %s (%s) is up on %s\n",
			__func__, spd->spdname, spd->servloc_name,
			gcinfo[spd->cid].subsys);
		spd->ispdup = 1;
	}

	return NOTIFY_DONE;
}

static int fastrpc_get_service_location_notify(struct notifier_block *nb,
				unsigned long opcode, void *data)
{
	struct fastrpc_static_pd *spd;
	struct pd_qmi_client_data *pdr = data;
	int curr_state = 0, i = 0;

	spd = container_of(nb, struct fastrpc_static_pd, get_service_nb);
	if (opcode == LOCATOR_DOWN) {
		pr_warn("adsprpc: %s: PDR notifier locator for %s is down for %s\n",
				__func__, gcinfo[spd->cid].subsys,
				spd->servloc_name);
		return NOTIFY_DONE;
	}
	for (i = 0; i < pdr->total_domains; i++) {
		if ((!strcmp(spd->servloc_name,
				AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME))
				&& (!strcmp(pdr->domain_list[i].name,
				ADSP_AUDIOPD_NAME))) {
			goto pdr_register;
		} else if ((!strcmp(spd->servloc_name,
				SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME))
				&& (!strcmp(pdr->domain_list[i].name,
				ADSP_SENSORPD_NAME))) {
			goto pdr_register;
		} else if ((!strcmp(spd->servloc_name,
				SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME))
				&& (!strcmp(pdr->domain_list[i].name,
				SLPI_SENSORPD_NAME))) {
			goto pdr_register;
		}
	}
	return NOTIFY_DONE;

pdr_register:
	if (!spd->pdrhandle) {
		spd->pdrhandle =
			service_notif_register_notifier(
			pdr->domain_list[i].name,
			pdr->domain_list[i].instance_id,
			&spd->pdrnb, &curr_state);
		if (IS_ERR_OR_NULL(spd->pdrhandle))
			pr_warn("adsprpc: %s: PDR notifier for %s register failed for %s (%s) with err %ld\n",
				__func__, gcinfo[spd->cid].subsys,
				pdr->domain_list[i].name, spd->servloc_name,
				PTR_ERR(spd->pdrhandle));
		else
			pr_info("adsprpc: %s: PDR notifier for %s registered for %s (%s)\n",
			__func__, gcinfo[spd->cid].subsys,
			pdr->domain_list[i].name, spd->servloc_name);
	} else {
		pr_warn("adsprpc: %s: %s (%s) notifier is already registered for %s\n",
			__func__, pdr->domain_list[i].name,
			spd->servloc_name, gcinfo[spd->cid].subsys);
	}

	if (curr_state == SERVREG_NOTIF_SERVICE_STATE_UP_V01) {
		pr_info("adsprpc: %s: %s (%s) PDR service for %s is up\n",
			__func__, spd->servloc_name, pdr->domain_list[i].name,
			gcinfo[spd->cid].subsys);
		spd->ispdup = 1;
	} else if (curr_state == SERVREG_NOTIF_SERVICE_STATE_UNINIT_V01) {
		pr_info("adsprpc: %s: %s (%s) PDR service for %s is uninitialized\n",
			__func__, spd->servloc_name, pdr->domain_list[i].name,
			gcinfo[spd->cid].subsys);
	}
	return NOTIFY_DONE;
}

static const struct file_operations fops = {
	.open = fastrpc_device_open,
	.release = fastrpc_device_release,
	.unlocked_ioctl = fastrpc_device_ioctl,
	.compat_ioctl = compat_fastrpc_device_ioctl,
};

static const struct of_device_id fastrpc_match_table[] = {
	{ .compatible = "qcom,msm-fastrpc-adsp", },
	{ .compatible = "qcom,msm-fastrpc-compute", },
	{ .compatible = "qcom,msm-fastrpc-compute-cb", },
	{ .compatible = "qcom,msm-adsprpc-mem-region", },
	{}
};

static int fastrpc_cb_probe(struct device *dev)
{
	struct fastrpc_channel_ctx *chan;
	struct fastrpc_session_ctx *sess;
	struct of_phandle_args iommuspec;
	struct fastrpc_apps *me = &gfa;
	const char *name;
	int err = 0, cid = -1, i = 0;
	u32 sharedcb_count = 0, j = 0;
	uint32_t dma_addr_pool[2] = {0, 0};

	VERIFY(err, NULL != (name = of_get_property(dev->of_node,
					 "label", NULL)));
	if (err)
		goto bail;

	for (i = 0; i < NUM_CHANNELS; i++) {
		if (!gcinfo[i].name)
			continue;
		if (!strcmp(name, gcinfo[i].name))
			break;
	}
	VERIFY(err, i < NUM_CHANNELS);
	if (err)
		goto bail;
	cid = i;
	chan = &gcinfo[i];
	VERIFY(err, chan->sesscount < NUM_SESSIONS);
	if (err)
		goto bail;

	err = of_parse_phandle_with_args(dev->of_node, "iommus",
						"#iommu-cells", 0, &iommuspec);
	if (err) {
		pr_err("adsprpc: %s: parsing iommu arguments failed for %s with err %d\n",
					__func__, dev_name(dev), err);
		goto bail;
	}
	sess = &chan->session[chan->sesscount];
	sess->used = 0;
	sess->smmu.coherent = of_property_read_bool(dev->of_node,
						"dma-coherent");
	sess->smmu.secure = of_property_read_bool(dev->of_node,
						"qcom,secure-context-bank");
	sess->smmu.cb = iommuspec.args[0] & 0xf;
	sess->smmu.dev = dev;
	sess->smmu.dev_name = dev_name(dev);
	sess->smmu.enabled = 1;

	if (!sess->smmu.dev->dma_parms)
		sess->smmu.dev->dma_parms = devm_kzalloc(sess->smmu.dev,
			sizeof(*sess->smmu.dev->dma_parms), GFP_KERNEL);

	dma_set_max_seg_size(sess->smmu.dev, DMA_BIT_MASK(32));
	dma_set_seg_boundary(sess->smmu.dev, (unsigned long)DMA_BIT_MASK(64));

	of_property_read_u32_array(dev->of_node, "qcom,iommu-dma-addr-pool",
			dma_addr_pool, 2);
	me->max_size_limit = (dma_addr_pool[1] == 0 ? 0x78000000 :
			dma_addr_pool[1]);

	if (of_get_property(dev->of_node, "shared-cb", NULL) != NULL) {
		err = of_property_read_u32(dev->of_node, "shared-cb",
				&sharedcb_count);
		if (err)
			goto bail;
		if (sharedcb_count > 0) {
			struct fastrpc_session_ctx *dup_sess;

			for (j = 1; j < sharedcb_count &&
					chan->sesscount < NUM_SESSIONS; j++) {
				chan->sesscount++;
				dup_sess = &chan->session[chan->sesscount];
				memcpy(dup_sess, sess,
					sizeof(struct fastrpc_session_ctx));
			}
		}
	}

	chan->sesscount++;
	debugfs_global_file = debugfs_create_file("global", 0644, debugfs_root,
							NULL, &debugfs_fops);
bail:
	return err;
}

static void init_secure_vmid_list(struct device *dev, char *prop_name,
						struct secure_vm *destvm)
{
	int err = 0;
	u32 len = 0, i = 0;
	u32 *rhvmlist = NULL;
	u32 *rhvmpermlist = NULL;

	if (!of_find_property(dev->of_node, prop_name, &len))
		goto bail;
	if (len == 0)
		goto bail;
	len /= sizeof(u32);
	VERIFY(err, NULL != (rhvmlist = kcalloc(len, sizeof(u32), GFP_KERNEL)));
	if (err)
		goto bail;
	VERIFY(err, NULL != (rhvmpermlist = kcalloc(len, sizeof(u32),
					 GFP_KERNEL)));
	if (err)
		goto bail;
	for (i = 0; i < len; i++) {
		err = of_property_read_u32_index(dev->of_node, prop_name, i,
								&rhvmlist[i]);
		rhvmpermlist[i] = PERM_READ | PERM_WRITE | PERM_EXEC;
		pr_info("adsprpc: %s: secure VMID = %d\n",
			__func__, rhvmlist[i]);
		if (err) {
			pr_err("adsprpc: %s: failed to read VMID\n", __func__);
			goto bail;
		}
	}
	destvm->vmid = rhvmlist;
	destvm->vmperm = rhvmpermlist;
	destvm->vmcount = len;
bail:
	if (err) {
		kfree(rhvmlist);
		kfree(rhvmpermlist);
	}
}

static void init_qos_cores_list(struct device *dev, char *prop_name,
						struct qos_cores *silvercores)
{
	int err = 0;
	u32 len = 0, i = 0;
	u32 *coreslist = NULL;

	if (!of_find_property(dev->of_node, prop_name, &len))
		goto bail;
	if (len == 0)
		goto bail;
	len /= sizeof(u32);
	VERIFY(err, NULL != (coreslist = kcalloc(len, sizeof(u32),
						 GFP_KERNEL)));
	if (err)
		goto bail;
	for (i = 0; i < len; i++) {
		err = of_property_read_u32_index(dev->of_node, prop_name, i,
								&coreslist[i]);
		if (err) {
			pr_err("adsprpc: %s: failed to read QOS cores list\n",
								 __func__);
			goto bail;
		}
	}
	silvercores->coreno = coreslist;
	silvercores->corecount = len;
bail:
	if (err) {
		kfree(coreslist);
	}
}

static void configure_secure_channels(uint32_t secure_domains)
{
	struct fastrpc_apps *me = &gfa;
	int ii = 0;
	/*
	 * secure_domains contains the bitmask of the secure channels
	 *  Bit 0 - ADSP
	 *  Bit 1 - MDSP
	 *  Bit 2 - SLPI
	 *  Bit 3 - CDSP
	 */
	for (ii = ADSP_DOMAIN_ID; ii <= CDSP_DOMAIN_ID; ++ii) {
		int secure = (secure_domains >> ii) & 0x01;

		me->channel[ii].secure = secure;
	}
}


static int fastrpc_probe(struct platform_device *pdev)
{
	int err = 0;
	struct fastrpc_apps *me = &gfa;
	struct device *dev = &pdev->dev;
	struct smq_phy_page range;
	struct device_node *ion_node, *node;
	struct platform_device *ion_pdev;
	struct cma *cma;
	uint32_t val;
	int ret = 0;
	uint32_t secure_domains;
	int session = -1, cid = -1;

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-fastrpc-compute")) {
		init_secure_vmid_list(dev, "qcom,adsp-remoteheap-vmid",
							&gcinfo[0].rhvm);
		init_qos_cores_list(dev, "qcom,qos-cores",
							&me->silvercores);

		of_property_read_u32(dev->of_node, "qcom,rpc-latency-us",
			&me->latency);
		if (of_get_property(dev->of_node,
			"qcom,secure-domains", NULL) != NULL) {
			VERIFY(err, !of_property_read_u32(dev->of_node,
					  "qcom,secure-domains",
			      &secure_domains));
			if (!err)
				configure_secure_channels(secure_domains);
			else
				pr_info("adsprpc: unable to read the domain configuration from dts\n");
		}
	}
	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-fastrpc-compute-cb"))
		return fastrpc_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-adsprpc-mem-region")) {
		me->dev = dev;
		range.addr = 0;
		ion_node = of_find_compatible_node(NULL, NULL, "qcom,msm-ion");
		if (ion_node) {
			for_each_available_child_of_node(ion_node, node) {
				if (of_property_read_u32(node, "reg", &val))
					continue;
				if (val != ION_ADSP_HEAP_ID)
					continue;
				ion_pdev = of_find_device_by_node(node);
				if (!ion_pdev)
					break;
				cma = dev_get_cma_area(&ion_pdev->dev);
				if (cma) {
					range.addr = cma_get_base(cma);
					range.size = (size_t)cma_get_size(cma);
				}
				break;
			}
		}
		if (range.addr && !of_property_read_bool(dev->of_node,
							 "restrict-access")) {
			int srcVM[1] = {VMID_HLOS};
			int destVM[4] = {VMID_HLOS, VMID_MSS_MSA, VMID_SSC_Q6,
						VMID_ADSP_Q6};
			int destVMperm[4] = {PERM_READ | PERM_WRITE | PERM_EXEC,
				PERM_READ | PERM_WRITE | PERM_EXEC,
				PERM_READ | PERM_WRITE | PERM_EXEC,
				PERM_READ | PERM_WRITE | PERM_EXEC,
				};

			err = hyp_assign_phys(range.addr, range.size,
					srcVM, 1, destVM, destVMperm, 4);
			if (err)
				goto bail;
			me->range.addr = range.addr;
			me->range.size = range.size;
		}
		return 0;
	}
	me->legacy_remote_heap = of_property_read_bool(dev->of_node,
					"qcom,fastrpc-legacy-remote-heap");
	if (of_property_read_bool(dev->of_node,
					"qcom,fastrpc-adsp-audio-pdr")) {
		err = fastrpc_get_spd_session(
			AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME, &session, &cid);
		if (err)
			goto spdbail;
		me->channel[cid].spd[session].get_service_nb.notifier_call =
					fastrpc_get_service_location_notify;
		ret = get_service_location(
				AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME,
				AUDIO_PDR_ADSP_SERVICE_NAME,
				&me->channel[cid].spd[session].get_service_nb);
		if (ret)
			pr_warn("adsprpc: %s: get service location failed with %d for %s (%s)\n",
				__func__, ret, AUDIO_PDR_ADSP_SERVICE_NAME,
				AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME);
		else
			pr_info("adsprpc: %s: service location enabled for %s (%s)\n",
				__func__, AUDIO_PDR_ADSP_SERVICE_NAME,
				AUDIO_PDR_SERVICE_LOCATION_CLIENT_NAME);
	}
	if (of_property_read_bool(dev->of_node,
					"qcom,fastrpc-adsp-sensors-pdr")) {
		err = fastrpc_get_spd_session(
		SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME, &session, &cid);
		if (err)
			goto spdbail;
		me->channel[cid].spd[session].get_service_nb.notifier_call =
					fastrpc_get_service_location_notify;
		ret = get_service_location(
				SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME,
				SENSORS_PDR_ADSP_SERVICE_NAME,
				&me->channel[cid].spd[session].get_service_nb);
		if (ret)
			pr_warn("adsprpc: %s: get service location failed with %d for %s (%s)\n",
				__func__, ret, SENSORS_PDR_SLPI_SERVICE_NAME,
				SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME);
		else
			pr_info("adsprpc: %s: service location enabled for %s (%s)\n",
				__func__, SENSORS_PDR_SLPI_SERVICE_NAME,
				SENSORS_PDR_ADSP_SERVICE_LOCATION_CLIENT_NAME);
	}
	if (of_property_read_bool(dev->of_node,
					"qcom,fastrpc-slpi-sensors-pdr")) {
		err = fastrpc_get_spd_session(
		SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME, &session, &cid);
		if (err)
			goto spdbail;
		me->channel[cid].spd[session].get_service_nb.notifier_call =
					fastrpc_get_service_location_notify;
		ret = get_service_location(
				SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME,
				SENSORS_PDR_SLPI_SERVICE_NAME,
				&me->channel[cid].spd[session].get_service_nb);
		if (ret)
			pr_warn("adsprpc: %s: get service location failed with %d for %s (%s)\n",
				__func__, ret, SENSORS_PDR_SLPI_SERVICE_NAME,
				SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME);
		else
			pr_info("adsprpc: %s: service location enabled for %s (%s)\n",
				__func__, SENSORS_PDR_SLPI_SERVICE_NAME,
				SENSORS_PDR_SLPI_SERVICE_LOCATION_CLIENT_NAME);
	}
spdbail:
	err = of_platform_populate(pdev->dev.of_node,
					  fastrpc_match_table,
					  NULL, &pdev->dev);
	if (err)
		goto bail;
bail:
	return err;
}

static void fastrpc_deinit(void)
{
	struct fastrpc_channel_ctx *chan = gcinfo;
	int i, j;

	for (i = 0; i < NUM_CHANNELS; i++, chan++) {
		for (j = 0; j < NUM_SESSIONS; j++) {
			struct fastrpc_session_ctx *sess = &chan->session[j];

			if (sess->smmu.dev)
				sess->smmu.dev = NULL;
		}
		kfree(chan->rhvm.vmid);
		kfree(chan->rhvm.vmperm);
	}
}

static struct platform_driver fastrpc_driver = {
	.probe = fastrpc_probe,
	.driver = {
		.name = "fastrpc",
		.of_match_table = fastrpc_match_table,
		.suppress_bind_attrs = true,
	},
};

static const struct rpmsg_device_id fastrpc_rpmsg_match[] = {
	{ FASTRPC_GLINK_GUID },
	{ },
};

static const struct of_device_id fastrpc_rpmsg_of_match[] = {
	{ .compatible = "qcom,msm-fastrpc-rpmsg" },
	{ },
};
MODULE_DEVICE_TABLE(of, fastrpc_rpmsg_of_match);

static struct rpmsg_driver fastrpc_rpmsg_client = {
	.id_table = fastrpc_rpmsg_match,
	.probe = fastrpc_rpmsg_probe,
	.remove = fastrpc_rpmsg_remove,
	.callback = fastrpc_rpmsg_callback,
	.drv = {
		.name = "qcom,msm_fastrpc_rpmsg",
		.of_match_table = fastrpc_rpmsg_of_match,
	},
};

static int __init fastrpc_device_init(void)
{
	struct fastrpc_apps *me = &gfa;
	struct device *dev = NULL;
	struct device *secure_dev = NULL;
	int err = 0, i;

	debugfs_root = debugfs_create_dir("adsprpc", NULL);
	memset(me, 0, sizeof(*me));
	fastrpc_init(me);
	me->dev = NULL;
	me->legacy_remote_heap = 0;
	VERIFY(err, 0 == platform_driver_register(&fastrpc_driver));
	if (err)
		goto register_bail;
	VERIFY(err, 0 == alloc_chrdev_region(&me->dev_no, 0, NUM_CHANNELS,
					DEVICE_NAME));
	if (err)
		goto alloc_chrdev_bail;
	cdev_init(&me->cdev, &fops);
	me->cdev.owner = THIS_MODULE;
	VERIFY(err, 0 == cdev_add(&me->cdev, MKDEV(MAJOR(me->dev_no), 0),
				NUM_DEVICES));
	if (err)
		goto cdev_init_bail;
	me->class = class_create(THIS_MODULE, "fastrpc");
	VERIFY(err, !IS_ERR(me->class));
	if (err)
		goto class_create_bail;
	me->compat = (fops.compat_ioctl == NULL) ? 0 : 1;

	/*
	 * Create devices and register with sysfs
	 * Create first device with minor number 0
	 */
	dev = device_create(me->class, NULL,
				MKDEV(MAJOR(me->dev_no), MINOR_NUM_DEV),
				NULL, DEVICE_NAME);
	VERIFY(err, !IS_ERR_OR_NULL(dev));
	if (err)
		goto device_create_bail;

	/* Create secure device with minor number for secure device */
	secure_dev = device_create(me->class, NULL,
				MKDEV(MAJOR(me->dev_no), MINOR_NUM_SECURE_DEV),
				NULL, DEVICE_NAME_SECURE);
	VERIFY(err, !IS_ERR_OR_NULL(secure_dev));
	if (err)
		goto device_create_bail;

	for (i = 0; i < NUM_CHANNELS; i++) {
		me->jobid[i] = 1;
		me->channel[i].dev = secure_dev;
		if (i == CDSP_DOMAIN_ID)
			me->channel[i].dev = dev;
		me->channel[i].ssrcount = 0;
		me->channel[i].prevssrcount = 0;
		me->channel[i].issubsystemup = 1;
		me->channel[i].ramdumpenabled = 0;
		me->channel[i].rh_dump_dev = NULL;
		me->channel[i].nb.notifier_call = fastrpc_restart_notifier_cb;
		me->channel[i].handle = subsys_notif_register_notifier(
							gcinfo[i].subsys,
							&me->channel[i].nb);
		if (IS_ERR_OR_NULL(me->channel[i].handle))
			pr_warn("adsprpc: %s: SSR notifier register failed for %s with err %d\n",
				__func__, gcinfo[i].subsys,
				PTR_ERR(me->channel[i].handle));
		else
			pr_info("adsprpc: %s: SSR notifier registered for %s\n",
				__func__, gcinfo[i].subsys);
	}

	err = register_rpmsg_driver(&fastrpc_rpmsg_client);
	if (err) {
		pr_err("adsprpc: %s: register_rpmsg_driver failed with err %d\n",
			__func__, err);
		goto device_create_bail;
	}
	me->rpmsg_register = 1;

	me->wake_source = wakeup_source_register("adsprpc");
	VERIFY(err, !IS_ERR_OR_NULL(me->wake_source));
	if (err) {
		pr_err("adsprpc: Error: %s: wakeup_source_register failed with err %d\n",
					__func__, PTR_ERR(me->wake_source));
		goto device_create_bail;
	}
	return 0;
device_create_bail:
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (me->channel[i].handle)
			subsys_notif_unregister_notifier(me->channel[i].handle,
							&me->channel[i].nb);
	}
	if (!IS_ERR_OR_NULL(dev))
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
						MINOR_NUM_DEV));
	if (!IS_ERR_OR_NULL(secure_dev))
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
						 MINOR_NUM_SECURE_DEV));
	class_destroy(me->class);
class_create_bail:
	cdev_del(&me->cdev);
cdev_init_bail:
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
alloc_chrdev_bail:
register_bail:
	fastrpc_deinit();
	return err;
}

static void __exit fastrpc_device_exit(void)
{
	struct fastrpc_apps *me = &gfa;
	int i;

	fastrpc_file_list_dtor(me);
	fastrpc_deinit();
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (!gcinfo[i].name)
			continue;
		if (me->channel[i].ipc_log_ctx)
			ipc_log_context_destroy(me->channel[i].ipc_log_ctx);
		subsys_notif_unregister_notifier(me->channel[i].handle,
						&me->channel[i].nb);
	}

	/* Destroy the secure and non secure devices */
	device_destroy(me->class, MKDEV(MAJOR(me->dev_no), MINOR_NUM_DEV));
	device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
					 MINOR_NUM_SECURE_DEV));

	class_destroy(me->class);
	cdev_del(&me->cdev);
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
	if (me->rpmsg_register == 1)
		unregister_rpmsg_driver(&fastrpc_rpmsg_client);
	if (me->wake_source)
		wakeup_source_unregister(me->wake_source);
	debugfs_remove_recursive(debugfs_root);
}

late_initcall(fastrpc_device_init);
module_exit(fastrpc_device_exit);

MODULE_LICENSE("GPL v2");

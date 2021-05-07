/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */
#ifndef ADSPRPC_SHARED_H
#define ADSPRPC_SHARED_H

#include <linux/types.h>

#define FASTRPC_IOCTL_INVOKE	_IOWR('R', 1, struct fastrpc_ioctl_invoke)
#define FASTRPC_IOCTL_MMAP	_IOWR('R', 2, struct fastrpc_ioctl_mmap)
#define FASTRPC_IOCTL_MUNMAP	_IOWR('R', 3, struct fastrpc_ioctl_munmap)
#define FASTRPC_IOCTL_MMAP_64	_IOWR('R', 14, struct fastrpc_ioctl_mmap_64)
#define FASTRPC_IOCTL_MUNMAP_64	_IOWR('R', 15, struct fastrpc_ioctl_munmap_64)
#define FASTRPC_IOCTL_INVOKE_FD	_IOWR('R', 4, struct fastrpc_ioctl_invoke_fd)
#define FASTRPC_IOCTL_SETMODE	_IOWR('R', 5, uint32_t)
#define FASTRPC_IOCTL_INIT	_IOWR('R', 6, struct fastrpc_ioctl_init)
#define FASTRPC_IOCTL_INVOKE_ATTRS \
				_IOWR('R', 7, struct fastrpc_ioctl_invoke_attrs)
#define FASTRPC_IOCTL_GETINFO	_IOWR('R', 8, uint32_t)
//#define FASTRPC_IOCTL_GETPERF	_IOWR('R', 9, struct fastrpc_ioctl_perf)
#define FASTRPC_IOCTL_INIT_ATTRS _IOWR('R', 10, struct fastrpc_ioctl_init_attrs)
#define FASTRPC_IOCTL_INVOKE_CRC _IOWR('R', 11, struct fastrpc_ioctl_invoke_crc)
#define FASTRPC_IOCTL_CONTROL   _IOWR('R', 12, struct fastrpc_ioctl_control)
#define FASTRPC_IOCTL_MUNMAP_FD _IOWR('R', 13, struct fastrpc_ioctl_munmap_fd)
#define FASTRPC_IOCTL_GET_DSP_INFO \
		_IOWR('R', 17, struct fastrpc_ioctl_capability)
#define FASTRPC_IOCTL_INVOKE2   _IOWR('R', 18, struct fastrpc_ioctl_invoke2)
#define FASTRPC_IOCTL_MEM_MAP   _IOWR('R', 19, struct fastrpc_ioctl_mem_map)
#define FASTRPC_IOCTL_MEM_UNMAP _IOWR('R', 20, struct fastrpc_ioctl_mem_unmap)
#define FASTRPC_IOCTL_INVOKE_PERF \
		_IOWR('R', 21, struct fastrpc_ioctl_invoke_perf)

#define FASTRPC_GLINK_GUID "fastrpcglink-apps-dsp"
#define FASTRPC_SMD_GUID "fastrpcsmd-apps-dsp"
#define DEVICE_NAME      "adsprpc-smd"
#define DEVICE_NAME_SECURE "adsprpc-smd-secure"

/* Set for buffers that have no virtual mapping in userspace */
#define FASTRPC_ATTR_NOVA 0x1

/* Set for buffers that are NOT dma coherent */
#define FASTRPC_ATTR_NON_COHERENT 0x2

/* Set for buffers that are dma coherent */
#define FASTRPC_ATTR_COHERENT 0x4

/* Fastrpc attribute for keeping the map persistent */
#define FASTRPC_ATTR_KEEP_MAP	0x8

/* Fastrpc attribute for no mapping of fd  */
#define FASTRPC_ATTR_NOMAP (16)

/*
 * Fastrpc attribute to skip flush by fastrpc
 */
#define FASTRPC_ATTR_FORCE_NOFLUSH  (32)

/*
 * Fastrpc attribute to skip invalidate by fastrpc
 */
#define FASTRPC_ATTR_FORCE_NOINVALIDATE (64)

/* Driver should operate in parallel with the co-processor */
#define FASTRPC_MODE_PARALLEL    0

/* Driver should operate in serial mode with the co-processor */
#define FASTRPC_MODE_SERIAL      1

/* Driver should operate in profile mode with the co-processor */
#define FASTRPC_MODE_PROFILE     2

/* Set FastRPC session ID to 1 */
#define FASTRPC_MODE_SESSION     4

/* INIT a new process or attach to guestos */
#define FASTRPC_INIT_ATTACH      0
#define FASTRPC_INIT_CREATE      1
#define FASTRPC_INIT_CREATE_STATIC  2
#define FASTRPC_INIT_ATTACH_SENSORS 3

/* Retrives number of input buffers from the scalars parameter */
#define REMOTE_SCALARS_INBUFS(sc)        (((sc) >> 16) & 0x0ff)

/* Retrives number of output buffers from the scalars parameter */
#define REMOTE_SCALARS_OUTBUFS(sc)       (((sc) >> 8) & 0x0ff)

/* Retrives number of input handles from the scalars parameter */
#define REMOTE_SCALARS_INHANDLES(sc)     (((sc) >> 4) & 0x0f)

/* Retrives number of output handles from the scalars parameter */
#define REMOTE_SCALARS_OUTHANDLES(sc)    ((sc) & 0x0f)

#define REMOTE_SCALARS_LENGTH(sc)	(REMOTE_SCALARS_INBUFS(sc) +\
					REMOTE_SCALARS_OUTBUFS(sc) +\
					REMOTE_SCALARS_INHANDLES(sc) +\
					REMOTE_SCALARS_OUTHANDLES(sc))

#define REMOTE_SCALARS_MAKEX(attr, method, in, out, oin, oout) \
		((((uint32_t)   (attr) & 0x7) << 29) | \
		(((uint32_t) (method) & 0x1f) << 24) | \
		(((uint32_t)     (in) & 0xff) << 16) | \
		(((uint32_t)    (out) & 0xff) <<  8) | \
		(((uint32_t)    (oin) & 0x0f) <<  4) | \
		((uint32_t)   (oout) & 0x0f))

#define REMOTE_SCALARS_MAKE(method, in, out) \
		REMOTE_SCALARS_MAKEX(0, method, in, out, 0, 0)

#ifdef VERIFY_PRINT_ERROR
#define VERIFY_EPRINTF(format, ...) pr_err(format, ##__VA_ARGS__)
#else
#define VERIFY_EPRINTF(format, args) (void)0
#endif

#ifndef VERIFY_PRINT_INFO
#define VERIFY_IPRINTF(args) (void)0
#endif

#ifndef VERIFY
#define __STR__(x) #x ":"
#define __TOSTR__(x) __STR__(x)
#define __FILE_LINE__ __FILE__ ":" __TOSTR__(__LINE__)
#define __ADSPRPC_LINE__ "adsprpc:" __TOSTR__(__LINE__)

#define VERIFY(err, val) \
do {\
	VERIFY_IPRINTF(__FILE_LINE__"info: calling: " #val "\n");\
	if ((val) == 0) {\
		(err) = (err) == 0 ? -1 : (err);\
		VERIFY_EPRINTF(__ADSPRPC_LINE__" error: %d: "#val "\n", (err));\
	} else {\
		VERIFY_IPRINTF(__FILE_LINE__"info: passed: " #val "\n");\
	} \
} while (0)
#endif

#define K_COPY_TO_USER_WITHOUT_ERR(kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			(void)copy_to_user((void __user *)(dst),\
			(src), (size));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

#define ADSPRPC_ERR(fmt, args...)\
	pr_err("Error: adsprpc (%d): %s: %s: " fmt, __LINE__,\
	current->comm, __func__, ##args)
#define ADSPRPC_INFO(fmt, args...)\
	pr_info("Info: adsprpc (%d): %s: %s: " fmt, __LINE__,\
	current->comm, __func__, ##args)
#define ADSPRPC_WARN(fmt, args...)\
	pr_warn("Warning: adsprpc (%d): %s: %s: " fmt, __LINE__,\
	current->comm, __func__, ##args)
#define ADSPRPC_DEBUG(fmt, args...)\
	pr_debug("Debug: adsprpc (%d): %s: %s: " fmt, __LINE__,\
	current->comm, __func__, ##args)

#define DEBUG_PRINT_SIZE_LIMIT (512*1024)

#define remote_arg64_t    union remote_arg64

struct remote_buf64 {
	uint64_t pv;
	uint64_t len;
};

struct remote_dma_handle64 {
	int fd;
	uint32_t offset;
	uint32_t len;
};

union remote_arg64 {
	struct remote_buf64	buf;
	struct remote_dma_handle64 dma;
	uint32_t h;
};

#define remote_arg_t    union remote_arg

struct remote_buf {
	void *pv;		/* buffer pointer */
	size_t len;		/* length of buffer */
};

struct remote_dma_handle {
	int fd;
	uint32_t offset;
};

union remote_arg {
	struct remote_buf buf;	/* buffer info */
	struct remote_dma_handle dma;
	uint32_t h;		/* remote handle */
};

struct fastrpc_ioctl_invoke {
	uint32_t handle;	/* remote handle */
	uint32_t sc;		/* scalars describing the data */
	remote_arg_t *pra;	/* remote arguments list */
};

struct fastrpc_ioctl_invoke_fd {
	struct fastrpc_ioctl_invoke inv;
	int *fds;		/* fd list */
};

struct fastrpc_ioctl_invoke_attrs {
	struct fastrpc_ioctl_invoke inv;
	int *fds;		/* fd list */
	unsigned int *attrs;	/* attribute list */
};

struct fastrpc_ioctl_invoke_crc {
	struct fastrpc_ioctl_invoke inv;
	int *fds;		/* fd list */
	unsigned int *attrs;	/* attribute list */
	unsigned int *crc;
};

struct fastrpc_ioctl_invoke_perf {
	struct fastrpc_ioctl_invoke inv;
	int *fds;
	unsigned int *attrs;
	unsigned int *crc;
	uint64_t *perf_kernel;
	uint64_t *perf_dsp;
};

struct fastrpc_async_job {
	uint32_t isasyncjob; /* flag to distinguish async job */
	uint64_t jobid;      /* job id generated by user */
	uint32_t reserved;   /* reserved */
};

struct fastrpc_ioctl_invoke_async {
	struct fastrpc_ioctl_invoke inv;
	int *fds;		/* fd list */
	unsigned int *attrs;	/* attribute list */
	unsigned int *crc;
	uint64_t *perf_kernel;
	uint64_t *perf_dsp;
	struct fastrpc_async_job *job; /* async job*/
};

struct fastrpc_ioctl_invoke_async_no_perf {
	struct fastrpc_ioctl_invoke inv;
	int *fds;		/* fd list */
	unsigned int *attrs;	/* attribute list */
	unsigned int *crc;
	struct fastrpc_async_job *job; /* async job*/
};

struct fastrpc_ioctl_async_response {
	uint64_t jobid;/* job id generated by user */
	int result; /* result from DSP */
	uint64_t *perf_kernel;
	uint64_t *perf_dsp;
	uint32_t handle;
	uint32_t sc;
};

enum fastrpc_invoke2_type {
	FASTRPC_INVOKE2_ASYNC		   = 1,
	FASTRPC_INVOKE2_ASYNC_RESPONSE = 2,
	FASTRPC_INVOKE2_KERNEL_OPTIMIZATIONS,
};

enum fastrpc_process_exit_states {
	/* Process Default State */
	FASTRPC_PROCESS_DEFAULT_STATE				= 0,
	/* Process exit initiated */
	FASTRPC_PROCESS_EXIT_START				= 1,
	/* Process exit issued to DSP */
	FASTRPC_PROCESS_DSP_EXIT_INIT				= 2,
	/* Process exit in DSP complete */
	FASTRPC_PROCESS_DSP_EXIT_COMPLETE			= 3,
};

struct fastrpc_ioctl_invoke2 {
	uint32_t req;       /* type of invocation request */
	uintptr_t invparam; /* invocation request param */
	uint32_t size;      /* size of invocation param */
	int err;            /* reserved */
};

struct fastrpc_ioctl_init {
	uint32_t flags;		/* one of FASTRPC_INIT_* macros */
	uintptr_t file;		/* pointer to elf file */
	uint32_t filelen;	/* elf file length */
	int32_t filefd;		/* ION fd for the file */
	uintptr_t mem;		/* mem for the PD */
	uint32_t memlen;	/* mem length */
	int32_t memfd;		/* ION fd for the mem */
};

struct fastrpc_ioctl_init_attrs {
		struct fastrpc_ioctl_init init;
		int attrs;
		unsigned int siglen;
};

struct fastrpc_ioctl_munmap {
	uintptr_t vaddrout;	/* address to unmap */
	size_t size;		/* size */
};

struct fastrpc_ioctl_munmap_64 {
	uint64_t vaddrout;	/* address to unmap */
	size_t size;		/* size */
};

struct fastrpc_ioctl_mmap {
	int fd;					/* ion fd */
	uint32_t flags;			/* flags for dsp to map with */
	uintptr_t vaddrin;		/* optional virtual address */
	size_t size;			/* size */
	uintptr_t vaddrout;		/* dsps virtual address */
};

struct fastrpc_ioctl_mmap_64 {
	int fd;				/* ion fd */
	uint32_t flags;			/* flags for dsp to map with */
	uint64_t vaddrin;		/* optional virtual address */
	size_t size;			/* size */
	uint64_t vaddrout;		/* dsps virtual address */
};

struct fastrpc_ioctl_munmap_fd {
	int     fd;				/* fd */
	uint32_t  flags;		/* control flags */
	uintptr_t va;			/* va */
	ssize_t  len;			/* length */
};

/**
 * Control flags for mapping memory on DSP user process
 */
enum fastrpc_map_flags {
	/**
	 * Map memory pages with RW- permission and CACHE WRITEBACK.
	 * The driver is responsible for cache maintenance when passed
	 * the buffer to FastRPC calls. Same virtual address will be
	 * assigned for subsequent FastRPC calls.
	 */
	FASTRPC_MAP_STATIC = 0,

	/* Reserved */
	FASTRPC_MAP_RESERVED,

	/**
	 * Map memory pages with RW- permission and CACHE WRITEBACK.
	 * Mapping tagged with a file descriptor. User is responsible for
	 * CPU and DSP cache maintenance for the buffer. Get virtual address
	 * of buffer on DSP using HAP_mmap_get() and HAP_mmap_put() APIs.
	 */
	FASTRPC_MAP_FD = 2,

	/**
	 * Mapping delayed until user call HAP_mmap() and HAP_munmap()
	 * functions on DSP. It is useful to map a buffer with cache modes
	 * other than default modes. User is responsible for CPU and DSP
	 * cache maintenance for the buffer.
	 */
	FASTRPC_MAP_FD_DELAYED,
	FASTRPC_MAP_MAX,
};

struct fastrpc_mem_map {
	int fd;			/* ion fd */
	int offset;		/* buffer offset */
	uint32_t flags;		/* flags defined in enum fastrpc_map_flags */
	int attrs;		/* buffer attributes used for SMMU mapping */
	uintptr_t vaddrin;	/* buffer virtual address */
	size_t length;		/* buffer length */
	uint64_t vaddrout;	/* [out] remote virtual address */
};

/* Map and unmap IOCTL methods reserved memory size for future extensions */
#define MAP_RESERVED_NUM (14)
#define UNMAP_RESERVED_NUM (10)

/* map memory to DSP device */
struct fastrpc_ioctl_mem_map {
	int version;		/* Initial version 0 */
	union {
		struct fastrpc_mem_map m;
		int reserved[MAP_RESERVED_NUM];
	};
};

struct fastrpc_mem_unmap {
	int fd;			/* ion fd */
	uint64_t vaddr;		/* remote process (dsp) virtual address */
	size_t length;		/* buffer size */
};

/* unmap memory to DSP device */
struct fastrpc_ioctl_mem_unmap {
	int version;		/* Initial version 0 */
	union {
		struct fastrpc_mem_unmap um;
		int reserved[UNMAP_RESERVED_NUM];
	};
};

/*
 * This enum is shared with DSP. So, existing values should NOT
 * be modified. Only new members can be added.
 */
enum dsp_map_flags {
	/* Add memory to static PD pool, protection thru XPU */
	ADSP_MMAP_HEAP_ADDR = 4,

	/* Add memory to static PD pool, protection thru hypervisor */
	ADSP_MMAP_REMOTE_HEAP_ADDR = 8,

	/* Add memory to userPD pool, for user heap */
	ADSP_MMAP_ADD_PAGES = 0x1000,

	/* Add memory to userPD pool, for LLC heap */
	ADSP_MMAP_ADD_PAGES_LLC = 0x3000,

	/* Map persistent header buffer on DSP */
	ADSP_MMAP_PERSIST_HDR = 0x4000,
};

enum fastrpc_control_type {
	FASTRPC_CONTROL_LATENCY		=	1,
	FASTRPC_CONTROL_SMMU		=	2,
	FASTRPC_CONTROL_KALLOC		=	3,
	FASTRPC_CONTROL_WAKELOCK	=	4,
	FASTRPC_CONTROL_PM		=	5,
/* Clean process on DSP */
	FASTRPC_CONTROL_DSPPROCESS_CLEAN	=	6,
};

struct fastrpc_ctrl_latency {
	uint32_t enable;	/* latency control enable */
	uint32_t latency;	/* latency request in us */
};

struct fastrpc_ctrl_kalloc {
	uint32_t kalloc_support;  /* Remote memory allocation from kernel */
};

struct fastrpc_ctrl_wakelock {
	uint32_t enable;	/* wakelock control enable */
};

struct fastrpc_ctrl_pm {
	uint32_t timeout;	/* timeout(in ms) for PM to keep system awake */
};

struct fastrpc_ioctl_control {
	uint32_t req;
	union {
		struct fastrpc_ctrl_latency lp;
		struct fastrpc_ctrl_kalloc kalloc;
		struct fastrpc_ctrl_wakelock wp;
		struct fastrpc_ctrl_pm pm;
	};
};

#define FASTRPC_MAX_DSP_ATTRIBUTES	(256)
#define FASTRPC_MAX_ATTRIBUTES	(258)
#define ASYNC_FASTRPC_CAP (9)

struct fastrpc_ioctl_capability {
	uint32_t domain;
	uint32_t attribute_ID;
	uint32_t capability;
};

struct smq_null_invoke {
	uint64_t ctx;			/* invoke caller context */
	uint32_t handle;	    /* handle to invoke */
	uint32_t sc;		    /* scalars structure describing the data */
};

struct smq_phy_page {
	uint64_t addr;		/* physical address */
	uint64_t size;		/* size of contiguous region */
};

struct smq_invoke_buf {
	int num;		/* number of contiguous regions */
	int pgidx;		/* index to start of contiguous region */
};

struct smq_invoke {
	struct smq_null_invoke header;
	struct smq_phy_page page;   /* remote arg and list of pages address */
};

struct smq_msg {
	uint32_t pid;           /* process group id */
	uint32_t tid;           /* thread id */
	struct smq_invoke invoke;
};

struct smq_invoke_rsp {
	uint64_t ctx;			/* invoke caller context */
	int retval;	             /* invoke return value */
};

enum fastrpc_response_flags {
	NORMAL_RESPONSE = 0,
	EARLY_RESPONSE = 1,
	USER_EARLY_SIGNAL = 2,
	COMPLETE_SIGNAL = 3
};

struct smq_invoke_rspv2 {
	uint64_t ctx;		  /* invoke caller context */
	int retval;		  /* invoke return value */
	uint32_t flags;		  /* early response flags */
	uint32_t early_wake_time; /* user predicted early wakeup time in us */
	uint32_t version;	  /* Version number for validation */
};

static inline struct smq_invoke_buf *smq_invoke_buf_start(remote_arg64_t *pra,
							uint32_t sc)
{
	unsigned int len = REMOTE_SCALARS_LENGTH(sc);

	return (struct smq_invoke_buf *)(&pra[len]);
}

static inline struct smq_phy_page *smq_phy_page_start(uint32_t sc,
						struct smq_invoke_buf *buf)
{
	unsigned int nTotal = REMOTE_SCALARS_LENGTH(sc);

	return (struct smq_phy_page *)(&buf[nTotal]);
}

#endif

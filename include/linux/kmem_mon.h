#ifndef __KMEM_MON_H__
#define __KMEM_MON_H__

#include <linux/sched.h>

#ifdef CONFIG_MTPROF_KMEM

#ifndef TRUE
/** @def TRUE Logical value of true. */
#define TRUE 1
#endif

#ifndef FALSE
/** @def FALSE Logical value of false. */
#define FALSE 0
#endif

/* TODO: relationship amoung differnent definations */
#define MAX_PROCESS_NUM (16*1024)	/* MAX PID defined by linux: 32 *1024 - 1 */
#define MAX_KMEM_MON_NUM (20*1024)
#define MAX_ADDR_TABLE (64*1024)	/* NOTICE: for the hash */
#define MAX_CALLER_TABLE (80*1024)
#define MAX_FAIL_PARAMETER 256
#define MAX_MEM_CLASS_NUM 64
#define MAX_CMD_LINE 64
#define MAX_PID_LEN 6		/* because 16*1024 = 16384 */
#define MAX_ADDR_IDX (16*1024)	/* 14 */
#define ADDR_SHIFT 14

typedef enum {
	KMEM_MON_TYPE_KMALLOC = 0,	/* kmalloc() or kfree(). */
	KMEM_MON_TYPE_KMEM_CACHE,	/* kmem_cache_*(). */
	KMEM_MON_TYPE_PAGES,	/* __get_free_pages() and friends. */
	KMEM_MON_TYPE_PMEM,
	KMEM_MON_TYPE_M4U,
	KMEM_MON_TYPE_VMALLOC,
	KMEM_MON_TYPE_ASHMEM,
	KMEM_MON_TYPE_KMALLOCWRAPPER,
} MEM_CLASS_T;

typedef struct mem_class_info_struct {
	MEM_CLASS_T mem_class;

	int index[MAX_KMEM_MON_NUM];
} mem_class_info_t;

/* TODO: keep caller backtrace */
typedef struct caller_info_struct {
	unsigned long caller_addr;
	int bytes_req, bytes_alloc;
	int bytes_free;
	int freq_alloc, freq_free;
	int pid;
	MEM_CLASS_T mem_class;
	int next_node;
} caller_info_t;

/* NOTICE: if the node is not used, caller_hash == 0 */
typedef struct addr_info_struct {
	unsigned long addr;
	int caller_hash;
	int next;
} addr_info_t;

typedef struct process_info_struct {
	pid_t pid;
	pid_t tgid;
	char cmdline[MAX_CMD_LINE];
	char comm[TASK_COMM_LEN];	/* executable name excluding path
					   - access with [gs]et_task_comm (which lock
					   it with task_lock())
					   - initialized normally by setup_new_exec */
	int start_idx;		/* ->kmalloc->pem... */
	/* TODO: use an array to arrage all mem nodes */
} process_info_t;

/* ========================================================================== */
/**
 * @struct kmem_info_struct
 *
 * @brief  keep information for kernel memory monitoring
 *
 */
/**
 * @typedef kmem_mon_info_t
 * @brief Type definition for the kmem_mon_info_struct.
 */
typedef struct mem_info_struct {
	MEM_CLASS_T mem_class;

/* these information should be kept in caller table */
	size_t total_bytes_req;
	size_t total_bytes_alloc;

	size_t total_bytes_free;

	int alloc_freq;
	int free_freq;

	int peak_every_req;	/* peak size of a single requirment */
	unsigned long peak_caller;	/* code position' */
/* ends */

	/* TODO: allocation failures */
#if 0
	int fail_freq;
	unsigned long fail_caller;
	char last_fail_parameter[MAX_FAIL_PARAMETER];	/* only record the latest failure's paramter */
#endif
	int caller_start_idx;
	int next_mem_node;
	/*  */
	/* TODO: additional info for different memory class */
	/*  */
} mem_info_t;

#endif				/* #ifdef CONFIG_MTPROF_KMEM */

extern void kmem_mon_kmalloc(unsigned long caller, const void *addr, int bytes_req,
			     int bytes_alloc);
extern void kmem_mon_kfree(unsigned long caller, const void *addr);
extern void kmem_mon_pmem_alloc(int req, int alloc);
extern void kmem_mon_pmem_free(int size);
extern void kmem_mon_m4u_alloc(int req, int alloc);
extern void kmem_mon_m4u_dealloc(const unsigned int addr, const unsigned int req_size);
extern void kmem_mon_vmalloc(unsigned long caller, const void *addr, int bytes_req,
			     int bytes_alloc);
extern void kmem_mon_vfree(int size);
extern void kmem_mon_ashmem_mmap(int size);
extern void kmem_mon_ashmem_release(int size);
extern void kmem_mon_kmallocwrapper(unsigned long caller, int size);
/* extern void kmem_mon_kmallocwrapper(unsigned long caller, const void *addr, int bytes_req, int bytes_alloc); */
extern void kmem_mon_kfreewrapper(const void *addr);
extern void kmem_mon_kmem_cache_alloc(unsigned long caller, const void *addr, size_t bytes_req,
				      size_t bytes_alloc);
extern void kmem_mon_kmem_cache_free(unsigned long caller, const void *addr);

#endif				/* __KMEM_MON_H__ */

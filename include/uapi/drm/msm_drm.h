/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 * Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_DRM_H__
#define __MSM_DRM_H__

#include <stddef.h>
#include <drm/drm.h>
#include <drm/sde_drm.h>

/* Please note that modifications to all structs defined here are
 * subject to backwards-compatibility constraints:
 *  1) Do not use pointers, use __u64 instead for 32 bit / 64 bit
 *     user/kernel compatibility
 *  2) Keep fields aligned to their size
 *  3) Because of how drm_ioctl() works, we can add new fields at
 *     the end of an ioctl if some care is taken: drm_ioctl() will
 *     zero out the new fields at the tail of the ioctl, so a zero
 *     value should have a backwards compatible meaning.  And for
 *     output params, userspace won't see the newly added output
 *     fields.. so that has to be somehow ok.
 */

#define MSM_PIPE_NONE        0x00
#define MSM_PIPE_2D0         0x01
#define MSM_PIPE_2D1         0x02
#define MSM_PIPE_3D0         0x10

/* timeouts are specified in clock-monotonic absolute times (to simplify
 * restarting interrupted ioctls).  The following struct is logically the
 * same as 'struct timespec' but 32/64b ABI safe.
 */
struct drm_msm_timespec {
	__s64 tv_sec;          /* seconds */
	__s64 tv_nsec;         /* nanoseconds */
};

#define MSM_PARAM_GPU_ID     0x01
#define MSM_PARAM_GMEM_SIZE  0x02
#define MSM_PARAM_CHIP_ID    0x03

struct drm_msm_param {
	__u32 pipe;           /* in, MSM_PIPE_x */
	__u32 param;          /* in, MSM_PARAM_x */
	__u64 value;          /* out (get_param) or in (set_param) */
};

/*
 * GEM buffers:
 */

#define MSM_BO_SCANOUT       0x00000001     /* scanout capable */
#define MSM_BO_GPU_READONLY  0x00000002
#define MSM_BO_CACHE_MASK    0x000f0000
/* cache modes */
#define MSM_BO_CACHED        0x00010000
#define MSM_BO_WC            0x00020000
#define MSM_BO_UNCACHED      0x00040000

#define MSM_BO_CONTIGUOUS    0x00100000

#define MSM_BO_FLAGS         (MSM_BO_SCANOUT | \
				MSM_BO_GPU_READONLY | \
				MSM_BO_CACHED | \
				MSM_BO_WC | \
				MSM_BO_UNCACHED | \
				MSM_BO_CONTIGUOUS)

struct drm_msm_gem_new {
	__u64 size;           /* in */
	__u32 flags;          /* in, mask of MSM_BO_x */
	__u32 handle;         /* out */
};

struct drm_msm_gem_info {
	__u32 handle;         /* in */
	__u32 hint;           /* in, 0: mmap offset; 1: GPU iova */
	__u64 offset;         /* out, offset to pass to mmap() */
};

#define MSM_PREP_READ        0x01
#define MSM_PREP_WRITE       0x02
#define MSM_PREP_NOSYNC      0x04

#define MSM_PREP_FLAGS       (MSM_PREP_READ | MSM_PREP_WRITE | MSM_PREP_NOSYNC)

struct drm_msm_gem_cpu_prep {
	__u32 handle;         /* in */
	__u32 op;             /* in, mask of MSM_PREP_x */
	struct drm_msm_timespec timeout;   /* in */
};

struct drm_msm_gem_cpu_fini {
	__u32 handle;         /* in */
};

/*
 * Cmdstream Submission:
 */

/* The value written into the cmdstream is logically:
 *
 *   ((relocbuf->gpuaddr + reloc_offset) << shift) | or
 *
 * When we have GPU's w/ >32bit ptrs, it should be possible to deal
 * with this by emit'ing two reloc entries with appropriate shift
 * values.  Or a new MSM_SUBMIT_CMD_x type would also be an option.
 *
 * NOTE that reloc's must be sorted by order of increasing submit_offset,
 * otherwise EINVAL.
 */
struct drm_msm_gem_submit_reloc {
	__u32 submit_offset;  /* in, offset from submit_bo */
#ifdef __cplusplus
	__u32 or_val;
#else
	__u32 or;             /* in, value OR'd with result */
#endif
	__s32  shift;          /* in, amount of left shift (can be negative) */
	__u32 reloc_idx;      /* in, index of reloc_bo buffer */
	__u64 reloc_offset;   /* in, offset from start of reloc_bo */
};

/* submit-types:
 *   BUF - this cmd buffer is executed normally.
 *   IB_TARGET_BUF - this cmd buffer is an IB target.  Reloc's are
 *      processed normally, but the kernel does not setup an IB to
 *      this buffer in the first-level ringbuffer
 *   CTX_RESTORE_BUF - only executed if there has been a GPU context
 *      switch since the last SUBMIT ioctl
 */
#define MSM_SUBMIT_CMD_BUF             0x0001
#define MSM_SUBMIT_CMD_IB_TARGET_BUF   0x0002
#define MSM_SUBMIT_CMD_CTX_RESTORE_BUF 0x0003
struct drm_msm_gem_submit_cmd {
	__u32 type;           /* in, one of MSM_SUBMIT_CMD_x */
	__u32 submit_idx;     /* in, index of submit_bo cmdstream buffer */
	__u32 submit_offset;  /* in, offset into submit_bo */
	__u32 size;           /* in, cmdstream size */
	__u32 pad;
	__u32 nr_relocs;      /* in, number of submit_reloc's */
	__u64 __user relocs;  /* in, ptr to array of submit_reloc's */
};

/* Each buffer referenced elsewhere in the cmdstream submit (ie. the
 * cmdstream buffer(s) themselves or reloc entries) has one (and only
 * one) entry in the submit->bos[] table.
 *
 * As a optimization, the current buffer (gpu virtual address) can be
 * passed back through the 'presumed' field.  If on a subsequent reloc,
 * userspace passes back a 'presumed' address that is still valid,
 * then patching the cmdstream for this entry is skipped.  This can
 * avoid kernel needing to map/access the cmdstream bo in the common
 * case.
 */
#define MSM_SUBMIT_BO_READ             0x0001
#define MSM_SUBMIT_BO_WRITE            0x0002

#define MSM_SUBMIT_BO_FLAGS            (MSM_SUBMIT_BO_READ | MSM_SUBMIT_BO_WRITE)

struct drm_msm_gem_submit_bo {
	__u32 flags;          /* in, mask of MSM_SUBMIT_BO_x */
	__u32 handle;         /* in, GEM handle */
	__u64 presumed;       /* in/out, presumed buffer address */
};

/* Each cmdstream submit consists of a table of buffers involved, and
 * one or more cmdstream buffers.  This allows for conditional execution
 * (context-restore), and IB buffers needed for per tile/bin draw cmds.
 */
struct drm_msm_gem_submit {
	__u32 pipe;           /* in, MSM_PIPE_x */
	__u32 fence;          /* out */
	__u32 nr_bos;         /* in, number of submit_bo's */
	__u32 nr_cmds;        /* in, number of submit_cmd's */
	__u64 __user bos;     /* in, ptr to array of submit_bo's */
	__u64 __user cmds;    /* in, ptr to array of submit_cmd's */
};

struct drm_msm_context_submit {
	__u32 context_id;     /* in, context id */
	__u32 timestamp;      /* in, user generated timestamp */
	__u32 nr_bos;         /* in, number of submit_bo's */
	__u32 nr_cmds;        /* in, number of submit_cmd's */
	__u64 __user bos;     /* in, ptr to array of submit_bo's */
	__u64 __user cmds;    /* in, ptr to array of submit_cmd's */
};

/* The normal way to synchronize with the GPU is just to CPU_PREP on
 * a buffer if you need to access it from the CPU (other cmdstream
 * submission from same or other contexts, PAGE_FLIP ioctl, etc, all
 * handle the required synchronization under the hood).  This ioctl
 * mainly just exists as a way to implement the gallium pipe_fence
 * APIs without requiring a dummy bo to synchronize on.
 */
struct drm_msm_wait_fence {
	__u32 fence;          /* in */
	__u32 pad;
	struct drm_msm_timespec timeout;   /* in */
};

/*
 * User space can get the last fence processed by the GPU
 * */
struct drm_msm_get_last_fence {
	uint32_t fence;          /* out*/
	uint32_t pad;
};

/**
 * struct drm_perfcounter_read_group - argument to DRM_IOCTL_MSM_PERFCOUNTER_READ
 * @groupid: Performance counter group IDs
 * @countable: Performance counter countable IDs
 * @value: Return performance counter reads
 *
 * Read in the current value of a performance counter given by the groupid
 * and countable.
 *
 */

struct drm_perfcounter_read_group {
	unsigned int groupid;
	unsigned int countable;
	unsigned long long value;
};

struct drm_perfcounter_read {
	struct drm_perfcounter_read_group __user *reads;
	unsigned int count;
	/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

/**
 * struct drm_perfcounter_query - argument to DRM_IOCTL_MSM_PERFCOUNTER_QUERY
 * @groupid: Performance counter group ID
 * @countable: Return active countables array
 * @size: Size of active countables array
 * @max_counters: Return total number counters for the group ID
 *
 * Query the available performance counters given a groupid.  The array
 * *countables is used to return the current active countables in counters.
 * The size of the array is passed in so the kernel will only write at most
 * size or counter->size for the group id.  The total number of available
 * counters for the group ID is returned in max_counters.
 * If the array or size passed in are invalid, then only the maximum number
 * of counters will be returned, no data will be written to *countables.
 * If the groupid is invalid an error code will be returned.
 *
 */
struct drm_perfcounter_query {
	unsigned int groupid;
	/* Array to return the current countable for up to size counters */
	unsigned int __user *countables;
	unsigned int count;
	unsigned int max_counters;
	/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

/**
 * struct drm_perfcounter_get - argument to DRM_IOCTL_MSM_PERFCOUNTER_GET
 * @groupid: Performance counter group ID
 * @countable: Countable to select within the group
 * @offset: Return offset of the reserved LO counter
 * @offset_hi: Return offset of the reserved HI counter
 *
 * Get an available performance counter from a specified groupid.  The offset
 * of the performance counter will be returned after successfully assigning
 * the countable to the counter for the specified group.  An error will be
 * returned and an offset of 0 if the groupid is invalid or there are no
 * more counters left.  After successfully getting a perfcounter, the user
 * must call drm_perfcounter_put(groupid, contable) when finished with
 * the perfcounter to clear up perfcounter resources.
 *
 */
struct drm_perfcounter_get {
	unsigned int groupid;
	unsigned int countable;
	unsigned int offset;
	unsigned int offset_hi;
	/* private: reserved for future use */
	unsigned int __pad; /* For future binary compatibility */
};

/**
 * struct drm_perfcounter_put - argument to DRM_IOCTL_MSM_PERFCOUNTER_PUT
 * @groupid: Performance counter group ID
 * @countable: Countable to release within the group
 *
 * Put an allocated performance counter to allow others to have access to the
 * resource that was previously taken.  This is only to be called after
 * successfully getting a performance counter from drm_perfcounter_get().
 *
 */
struct drm_perfcounter_put {
	unsigned int groupid;
	unsigned int countable;
	/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

struct drm_msm_context_create {
	__u32 flags; /* [in] */
	__u32 context_id; /* [out] */
};

struct drm_msm_context_destroy {
	__u32 context_id; /* [in] */
	__u32 pad;
};

struct drm_msm_wait_timestamp {
	__u32 context_id; /*[in]*/
	__u32 timestamp; /*[in]*/
	__u32 timeout; /*[in] amount of time to wait (in milliseconds)*/
	__u32 pad;
};

struct drm_msm_read_timestamp {
	__u32 context_id; /* [in] */
	__u32 type;       /* in, */
	__u32 timestamp; /* [out] */
	__u32 pad;
};

struct drm_msm_gem_close {
	__u32 handle;     /* in, GEM handle */
	__u32 context_id; /* in, context id */
	__u32 timestamp;  /* in, timestamp to free the GEM object */
	__u32 pad;
};

#define DRM_MSM_GET_PARAM              0x00
/* placeholder:
#define DRM_MSM_SET_PARAM              0x01
 */
#define DRM_MSM_GEM_NEW                0x02
#define DRM_MSM_GEM_INFO               0x03
#define DRM_MSM_GEM_CPU_PREP           0x04
#define DRM_MSM_GEM_CPU_FINI           0x05
#define DRM_MSM_GEM_SUBMIT             0x06
#define DRM_MSM_WAIT_FENCE             0x07
#define DRM_MSM_GET_LAST_FENCE         0x08
#define DRM_SDE_WB_CONFIG              0x09
#define DRM_MSM_PERFCOUNTER_READ       0x0A
#define DRM_MSM_PERFCOUNTER_QUERY      0x0B
#define DRM_MSM_PERFCOUNTER_GET        0x0C
#define DRM_MSM_PERFCOUNTER_PUT        0x0D
#define DRM_MSM_CONTEXT_CREATE         0x0E
#define DRM_MSM_CONTEXT_DESTROY        0x0F
#define DRM_MSM_WAIT_TIMESTAMP         0x10
#define DRM_MSM_READ_TIMESTAMP         0x11
#define DRM_MSM_CONTEXT_SUBMIT         0x12
#define DRM_MSM_GEM_CLOSE              0x13
#define DRM_MSM_NUM_IOCTLS             0x14

#define DRM_IOCTL_MSM_GET_PARAM        DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GET_PARAM, struct drm_msm_param)
#define DRM_IOCTL_MSM_GEM_NEW          DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_NEW, struct drm_msm_gem_new)
#define DRM_IOCTL_MSM_GEM_INFO         DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_INFO, struct drm_msm_gem_info)
#define DRM_IOCTL_MSM_GEM_CPU_PREP     DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_GEM_CPU_PREP, struct drm_msm_gem_cpu_prep)
#define DRM_IOCTL_MSM_GEM_CPU_FINI     DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_GEM_CPU_FINI, struct drm_msm_gem_cpu_fini)
#define DRM_IOCTL_MSM_GEM_SUBMIT       DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_SUBMIT, struct drm_msm_gem_submit)
#define DRM_IOCTL_MSM_WAIT_FENCE       DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_WAIT_FENCE, struct drm_msm_wait_fence)
#define DRM_IOCTL_MSM_GET_LAST_FENCE \
	(DRM_IOR((DRM_COMMAND_BASE + DRM_MSM_GET_LAST_FENCE), \
	struct drm_msm_get_last_fence))
#define DRM_IOCTL_MSM_CONTEXT_CREATE   \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_CONTEXT_CREATE,\
			struct drm_msm_context_create)
#define DRM_IOCTL_MSM_CONTEXT_DESTROY  \
	(DRM_IOW(DRM_COMMAND_BASE + DRM_MSM_CONTEXT_DESTROY,\
			struct drm_msm_context_destroy))
#define DRM_IOCTL_MSM_WAIT_TIMESTAMP   \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_WAIT_TIMESTAMP,\
			struct drm_msm_wait_timestamp)
#define DRM_IOCTL_MSM_READ_TIMESTAMP   \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_READ_TIMESTAMP,\
			struct drm_msm_read_timestamp)
#define DRM_IOCTL_MSM_CONTEXT_SUBMIT               \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_CONTEXT_SUBMIT,\
			struct drm_msm_context_submit)
#define DRM_IOCTL_MSM_GEM_CLOSE        \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_CLOSE,\
			struct drm_msm_gem_close)
#define DRM_IOCTL_SDE_WB_CONFIG \
	DRM_IOW((DRM_COMMAND_BASE + DRM_SDE_WB_CONFIG), struct sde_drm_wb_cfg)
#define DRM_IOCTL_MSM_PERFCOUNTER_READ \
	(DRM_IOWR((DRM_COMMAND_BASE + DRM_MSM_PERFCOUNTER_READ), \
	struct drm_perfcounter_read))
#define DRM_IOCTL_MSM_PERFCOUNTER_QUERY \
	(DRM_IOWR((DRM_COMMAND_BASE + DRM_MSM_PERFCOUNTER_QUERY), \
	struct drm_perfcounter_query))
#define DRM_IOCTL_MSM_PERFCOUNTER_GET \
	(DRM_IOWR((DRM_COMMAND_BASE + DRM_MSM_PERFCOUNTER_GET), \
	struct drm_perfcounter_get))
#define DRM_IOCTL_MSM_PERFCOUNTER_PUT \
	(DRM_IOWR((DRM_COMMAND_BASE + DRM_MSM_PERFCOUNTER_PUT), \
	struct drm_perfcounter_put))


/* Performance counter groups */

#define DRM_MSM_PERFCOUNTER_GROUP_CP 0x0
#define DRM_MSM_PERFCOUNTER_GROUP_RBBM 0x1
#define DRM_MSM_PERFCOUNTER_GROUP_PC 0x2
#define DRM_MSM_PERFCOUNTER_GROUP_VFD 0x3
#define DRM_MSM_PERFCOUNTER_GROUP_HLSQ 0x4
#define DRM_MSM_PERFCOUNTER_GROUP_VPC 0x5
#define DRM_MSM_PERFCOUNTER_GROUP_TSE 0x6
#define DRM_MSM_PERFCOUNTER_GROUP_RAS 0x7
#define DRM_MSM_PERFCOUNTER_GROUP_UCHE 0x8
#define DRM_MSM_PERFCOUNTER_GROUP_TP 0x9
#define DRM_MSM_PERFCOUNTER_GROUP_SP 0xA
#define DRM_MSM_PERFCOUNTER_GROUP_RB 0xB
#define DRM_MSM_PERFCOUNTER_GROUP_PWR 0xC
#define DRM_MSM_PERFCOUNTER_GROUP_VBIF 0xD
#define DRM_MSM_PERFCOUNTER_GROUP_VBIF_PWR 0xE
#define DRM_MSM_PERFCOUNTER_GROUP_MH 0xF
#define DRM_MSM_PERFCOUNTER_GROUP_PA_SU 0x10
#define DRM_MSM_PERFCOUNTER_GROUP_SQ 0x11
#define DRM_MSM_PERFCOUNTER_GROUP_SX 0x12
#define DRM_MSM_PERFCOUNTER_GROUP_TCF 0x13
#define DRM_MSM_PERFCOUNTER_GROUP_TCM 0x14
#define DRM_MSM_PERFCOUNTER_GROUP_TCR 0x15
#define DRM_MSM_PERFCOUNTER_GROUP_L2 0x16
#define DRM_MSM_PERFCOUNTER_GROUP_VSC 0x17
#define DRM_MSM_PERFCOUNTER_GROUP_CCU 0x18
#define DRM_MSM_PERFCOUNTER_GROUP_LRZ 0x19
#define DRM_MSM_PERFCOUNTER_GROUP_CMP 0x1A
#define DRM_MSM_PERFCOUNTER_GROUP_ALWAYSON 0x1B
#define DRM_MSM_PERFCOUNTER_GROUP_SP_PWR 0x1C
#define DRM_MSM_PERFCOUNTER_GROUP_TP_PWR 0x1D
#define DRM_MSM_PERFCOUNTER_GROUP_RB_PWR 0x1E
#define DRM_MSM_PERFCOUNTER_GROUP_CCU_PWR 0x1F
#define DRM_MSM_PERFCOUNTER_GROUP_UCHE_PWR 0x20
#define DRM_MSM_PERFCOUNTER_GROUP_CP_PWR 0x21
#define DRM_MSM_PERFCOUNTER_GROUP_GPMU_PWR 0x22
#define DRM_MSM_PERFCOUNTER_GROUP_ALWAYSON_PWR 0x23
#define DRM_MSM_PERFCOUNTER_GROUP_MAX 0x24

#define DRM_MSM_PERFCOUNTER_NOT_USED 0xFFFFFFFF
#define DRM_MSM_PERFCOUNTER_BROKEN 0xFFFFFFFE

#define PERFCOUNTER_FLAG_NONE 0x0
#define PERFCOUNTER_FLAG_KERNEL 0x1

#define DRM_MSM_PERFCOUNTER_NOT_USED 0xFFFFFFFF
#define DRM_MSM_PERFCOUNTER_BROKEN 0xFFFFFFFE

#endif /* __MSM_DRM_H__ */

/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
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

#if defined(__cplusplus)
extern "C" {
#endif

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

/* The pipe-id just uses the lower bits, so can be OR'd with flags in
 * the upper 16 bits (which could be extended further, if needed, maybe
 * we extend/overload the pipe-id some day to deal with multiple rings,
 * but even then I don't think we need the full lower 16 bits).
 */
#define MSM_PIPE_ID_MASK     0xffff
#define MSM_PIPE_ID(x)       ((x) & MSM_PIPE_ID_MASK)
#define MSM_PIPE_FLAGS(x)    ((x) & ~MSM_PIPE_ID_MASK)

/* timeouts are specified in clock-monotonic absolute times (to simplify
 * restarting interrupted ioctls).  The following struct is logically the
 * same as 'struct timespec' but 32/64b ABI safe.
 */
struct drm_msm_timespec {
	__s64 tv_sec;          /* seconds */
	__s64 tv_nsec;         /* nanoseconds */
};

/* From CEA.861.3 */
#define HDR_EOTF_SMTPE_ST2084	0x2
#define HDR_EOTF_HLG		0x3

/* hdr hdmi state takes possible values of 0, 1 and 2 respectively */
#define DRM_MSM_HDR_DISABLE  0
#define DRM_MSM_HDR_ENABLE   1
#define DRM_MSM_HDR_RESET    2

/*
 * HDR Metadata
 * These are defined as per EDID spec and shall be used by the sink
 * to set the HDR metadata for playback from userspace.
 */

#define HDR_PRIMARIES_COUNT   3

struct drm_msm_ext_panel_hdr_metadata {
	__u32 eotf;             /* electro optical transfer function */
	__u32 hdr_supported;    /* HDR supported */
	__u32 display_primaries_x[HDR_PRIMARIES_COUNT]; /* Primaries x */
	__u32 display_primaries_y[HDR_PRIMARIES_COUNT]; /* Primaries y */
	__u32 white_point_x;    /* white_point_x */
	__u32 white_point_y;    /* white_point_y */
	__u32 max_luminance;    /* Max luminance */
	__u32 min_luminance;    /* Min Luminance */
	__u32 max_content_light_level; /* max content light level */
	__u32 max_average_light_level; /* max average light level */
};

/**
 * HDR Control
 * This encapsulates the HDR metadata as well as a state control
 * for the HDR metadata as required by the HDMI spec to send the
 * relevant metadata depending on the state of the HDR playback.
 * hdr_state: Controls HDR state, takes values ENABLE(1)/DISABLE(0)
 * hdr_meta: Metadata sent by the userspace for the HDR clip
 */

#define DRM_MSM_EXT_PANEL_HDR_CTRL
struct drm_msm_ext_panel_hdr_ctrl {
	__u8 hdr_state;                                 /* HDR state */
	struct drm_msm_ext_panel_hdr_metadata hdr_meta; /* HDR metadata */
};

/**
 * HDR sink properties
 * These are defined as per EDID spec and shall be used by the userspace
 * to determine the HDR properties to be set to the sink.
 */
struct drm_msm_ext_panel_hdr_properties {
	__u8 hdr_metadata_type_one;   /* static metadata type one */
	__u32 hdr_supported;          /* HDR supported */
	__u32 hdr_eotf;               /* electro optical transfer function */
	__u32 hdr_max_luminance;      /* Max luminance */
	__u32 hdr_avg_luminance;      /* Avg luminance */
	__u32 hdr_min_luminance;      /* Min Luminance */
};

#define MSM_PARAM_GPU_ID             0x01
#define MSM_PARAM_GMEM_SIZE          0x02
#define MSM_PARAM_CHIP_ID            0x03
#define MSM_PARAM_MAX_FREQ           0x04
#define MSM_PARAM_TIMESTAMP          0x05
#define MSM_PARAM_GMEM_BASE          0x06
#define MSM_PARAM_NR_RINGS           0x07
#define MSM_PARAM_GPU_HANG_TIMEOUT   0xa0 /* timeout in ms */

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
#define MSM_BO_PRIVILEGED    0x00000004
#define MSM_BO_SECURE        0x00000008	    /* Allocate and map as secure */
#define MSM_BO_CACHE_MASK    0x000f0000
/* cache modes */
#define MSM_BO_CACHED        0x00010000
#define MSM_BO_WC            0x00020000
#define MSM_BO_UNCACHED      0x00040000

#define MSM_BO_FLAGS         (MSM_BO_SCANOUT | \
                              MSM_BO_GPU_READONLY | \
                              MSM_BO_SECURE | \
                              MSM_BO_CACHED | \
                              MSM_BO_WC | \
                              MSM_BO_UNCACHED)

struct drm_msm_gem_new {
	__u64 size;           /* in */
	__u32 flags;          /* in, mask of MSM_BO_x */
	__u32 handle;         /* out */
};

struct drm_msm_gem_svm_new {
	__u64 hostptr;        /* in, must be page-aligned */
	__u64 size;           /* in, must be page-aligned */
	__u32 flags;          /* in, mask of MSM_BO_x */
	__u32 handle;         /* out */
};

#define MSM_INFO_IOVA	0x01

#define MSM_INFO_FLAGS (MSM_INFO_IOVA)

struct drm_msm_gem_info {
	__u32 handle;         /* in */
	__u32 flags;	      /* in - combination of MSM_INFO_* flags */
	__u64 offset;         /* out, mmap() offset or iova */
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
	__u32 or; /* in, value OR'd with result */
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
 *   PROFILE_BUF - A profiling buffer written to by both GPU and CPU.
 */
#define MSM_SUBMIT_CMD_BUF             0x0001
#define MSM_SUBMIT_CMD_IB_TARGET_BUF   0x0002
#define MSM_SUBMIT_CMD_CTX_RESTORE_BUF 0x0003
#define MSM_SUBMIT_CMD_PROFILE_BUF     0x0004

struct drm_msm_gem_submit_cmd {
	__u32 type;           /* in, one of MSM_SUBMIT_CMD_x */
	__u32 submit_idx;     /* in, index of submit_bo cmdstream buffer */
	__u32 submit_offset;  /* in, offset into submit_bo */
	__u32 size;           /* in, cmdstream size */
	__u32 pad;
	__u32 nr_relocs;      /* in, number of submit_reloc's */
	__u64 relocs;         /* in, ptr to array of submit_reloc's */
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

/* Valid submit ioctl flags: */
#define MSM_SUBMIT_RING_MASK 0x000F0000
#define MSM_SUBMIT_RING_SHIFT 16

#define MSM_SUBMIT_FLAGS (MSM_SUBMIT_RING_MASK)

/* Each cmdstream submit consists of a table of buffers involved, and
 * one or more cmdstream buffers.  This allows for conditional execution
 * (context-restore), and IB buffers needed for per tile/bin draw cmds.
 */
struct drm_msm_gem_submit {
	__u32 flags;          /* MSM_PIPE_x | MSM_SUBMIT_x */
	__u32 fence;          /* out */
	__u32 nr_bos;         /* in, number of submit_bo's */
	__u32 nr_cmds;        /* in, number of submit_cmd's */
	__u64 bos;     /* in, ptr to array of submit_bo's */
	__u64 cmds;    /* in, ptr to array of submit_cmd's */
	__s32 fence_fd;       /* gap for the fence_fd which is upstream */
	__u32 queueid;         /* in, submitqueue id */
};

/*
 * Define a preprocessor variable to let the userspace know that
 * drm_msm_gem_submit_profile_buffer switched to only support a kernel timestamp
 * for submit time
 */
#define MSM_PROFILE_BUFFER_SUBMIT_TIME 1

struct drm_msm_gem_submit_profile_buffer {
	struct drm_msm_timespec time;   /* out, submission time */
	__u64 ticks_queued;    /* out, GPU ticks at ringbuffer submission */
	__u64 ticks_submitted; /* out, GPU ticks before cmdstream execution*/
	__u64 ticks_retired;   /* out, GPU ticks after cmdstream execution */
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

/**
 * struct drm_msm_event_req - Payload to event enable/disable ioctls.
 * @object_id: DRM object id. Ex: for crtc pass crtc id.
 * @object_type: DRM object type. Ex: for crtc set it to DRM_MODE_OBJECT_CRTC.
 * @event: Event for which notification is being enabled/disabled.
 *         Ex: for Histogram set - DRM_EVENT_HISTOGRAM.
 * @client_context: Opaque pointer that will be returned during event response
 *                  notification.
 * @index: Object index(ex: crtc index), optional for user-space to set.
 *         Driver will override value based on object_id and object_type.
 */
struct drm_msm_event_req {
	__u32 object_id;
	__u32 object_type;
	__u32 event;
	__u64 client_context;
	__u32 index;
};

/**
 * struct drm_msm_event_resp - payload returned when read is called for
 *                            custom notifications.
 * @base: Event type and length of complete notification payload.
 * @info: Contains information about DRM that which raised this event.
 * @data: Custom payload that driver returns for event type.
 *        size of data = base.length - (sizeof(base) + sizeof(info))
 */
struct drm_msm_event_resp {
	struct drm_event base;
	struct drm_msm_event_req info;
	__u8 data[];
};

#define MSM_COUNTER_GROUP_CP 0
#define MSM_COUNTER_GROUP_RBBM 1
#define MSM_COUNTER_GROUP_PC 2
#define MSM_COUNTER_GROUP_VFD 3
#define MSM_COUNTER_GROUP_HLSQ 4
#define MSM_COUNTER_GROUP_VPC 5
#define MSM_COUNTER_GROUP_TSE 6
#define MSM_COUNTER_GROUP_RAS 7
#define MSM_COUNTER_GROUP_UCHE 8
#define MSM_COUNTER_GROUP_TP 9
#define MSM_COUNTER_GROUP_SP 10
#define MSM_COUNTER_GROUP_RB 11
#define MSM_COUNTER_GROUP_VBIF 12
#define MSM_COUNTER_GROUP_VBIF_PWR 13
#define MSM_COUNTER_GROUP_VSC 23
#define MSM_COUNTER_GROUP_CCU 24
#define MSM_COUNTER_GROUP_LRZ 25
#define MSM_COUNTER_GROUP_CMP 26
#define MSM_COUNTER_GROUP_ALWAYSON 27
#define MSM_COUNTER_GROUP_SP_PWR 28
#define MSM_COUNTER_GROUP_TP_PWR 29
#define MSM_COUNTER_GROUP_RB_PWR 30
#define MSM_COUNTER_GROUP_CCU_PWR 31
#define MSM_COUNTER_GROUP_UCHE_PWR 32
#define MSM_COUNTER_GROUP_CP_PWR 33
#define MSM_COUNTER_GROUP_GPMU_PWR 34
#define MSM_COUNTER_GROUP_ALWAYSON_PWR 35

/**
 * struct drm_msm_counter - allocate or release a GPU performance counter
 * @groupid: The group ID of the counter to get/put
 * @counterid: For GET returns the counterid that was assigned. For PUT
 *	       release the counter identified by groupid/counterid
 * @countable: For GET the countable for the counter
 */
struct drm_msm_counter {
	__u32 groupid;
	int counterid;
	__u32 countable;
	__u32 counter_lo;
	__u32 counter_hi;
};

struct drm_msm_counter_read_op {
	__u64 value;
	__u32 groupid;
	int counterid;
};

/**
 * struct drm_msm_counter_read - Read a number of GPU performance counters
 * ops: Pointer to the list of struct drm_msm_counter_read_op operations
 * nr_ops: Number of operations in the list
 */
struct drm_msm_counter_read {
	__u64 __user ops;
	__u32 nr_ops;
};

#define MSM_GEM_SYNC_TO_DEV 0
#define MSM_GEM_SYNC_TO_CPU 1

struct drm_msm_gem_syncop {
	__u32 handle;
	__u32 op;
};

struct drm_msm_gem_sync {
	__u32 nr_ops;
	__u64 __user ops;
};

/*
 * Draw queues allow the user to set specific submission parameter. Command
 * submissions will specify a specific submit queue id to use. id '0' is
 * reserved as a "default" drawqueue with medium priority. The user can safely
 * use and query 0 but cannot destroy it.
 */

/*
 * Allows a process to bypass the 2 second quality of service timeout.
 * Only CAP_SYS_ADMIN capable processes can set this flag.
 */
#define MSM_SUBMITQUEUE_BYPASS_QOS_TIMEOUT 0x00000001

#define MSM_SUBMITQUEUE_FLAGS (MSM_SUBMITQUEUE_BYPASS_QOS_TIMEOUT)

struct drm_msm_submitqueue {
	__u32 flags;   /* in, MSM_SUBMITQUEUE_x */
	__u32 prio;    /* in, Priority level */
	__u32 id;      /* out, identifier */
};

#define MSM_SUBMITQUEUE_PARAM_FAULTS 0

struct drm_msm_submitqueue_query {
	__u64 data;
	__u32 id;
	__u32 param;
	__u32 len;
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
/* Gap for upstream DRM_MSM_GEM_MADVISE */
#define DRM_MSM_GEM_SVM_NEW            0x09
#define DRM_MSM_SUBMITQUEUE_NEW        0x0A
#define DRM_MSM_SUBMITQUEUE_CLOSE      0x0B
#define DRM_MSM_SUBMITQUEUE_QUERY      0x0C

#define DRM_SDE_WB_CONFIG              0x40
#define DRM_MSM_REGISTER_EVENT         0x41
#define DRM_MSM_DEREGISTER_EVENT       0x42
#define DRM_MSM_COUNTER_GET            0x43
#define DRM_MSM_COUNTER_PUT            0x44
#define DRM_MSM_COUNTER_READ           0x45
#define DRM_MSM_GEM_SYNC               0x46
#define DRM_MSM_RMFB2                  0x47

/**
 * Currently DRM framework supports only VSYNC event.
 * Starting the custom events at 0xff to provide space for DRM
 * framework to add new events.
 */
#define DRM_EVENT_HISTOGRAM 0xff
#define DRM_EVENT_AD 0x100

#define DRM_IOCTL_MSM_GET_PARAM        DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GET_PARAM, struct drm_msm_param)
#define DRM_IOCTL_MSM_GEM_NEW          DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_NEW, struct drm_msm_gem_new)
#define DRM_IOCTL_MSM_GEM_INFO         DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_INFO, struct drm_msm_gem_info)
#define DRM_IOCTL_MSM_GEM_CPU_PREP     DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_GEM_CPU_PREP, struct drm_msm_gem_cpu_prep)
#define DRM_IOCTL_MSM_GEM_CPU_FINI     DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_GEM_CPU_FINI, struct drm_msm_gem_cpu_fini)
#define DRM_IOCTL_MSM_GEM_SUBMIT       DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_SUBMIT, struct drm_msm_gem_submit)
#define DRM_IOCTL_MSM_WAIT_FENCE       DRM_IOW (DRM_COMMAND_BASE + DRM_MSM_WAIT_FENCE, struct drm_msm_wait_fence)
#define DRM_IOCTL_SDE_WB_CONFIG \
	DRM_IOW((DRM_COMMAND_BASE + DRM_SDE_WB_CONFIG), struct sde_drm_wb_cfg)
#define DRM_IOCTL_MSM_REGISTER_EVENT   DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_REGISTER_EVENT), struct drm_msm_event_req)
#define DRM_IOCTL_MSM_DEREGISTER_EVENT DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_DEREGISTER_EVENT), struct drm_msm_event_req)
#define DRM_IOCTL_MSM_COUNTER_GET \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_COUNTER_GET, struct drm_msm_counter)
#define DRM_IOCTL_MSM_COUNTER_PUT \
	DRM_IOW(DRM_COMMAND_BASE + DRM_MSM_COUNTER_PUT, struct drm_msm_counter)
#define DRM_IOCTL_MSM_COUNTER_READ \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_COUNTER_READ, \
		struct drm_msm_counter_read)
#define DRM_IOCTL_MSM_GEM_SYNC DRM_IOW(DRM_COMMAND_BASE + DRM_MSM_GEM_SYNC,\
		struct drm_msm_gem_sync)
#define DRM_IOCTL_MSM_GEM_SVM_NEW \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_GEM_SVM_NEW, \
		struct drm_msm_gem_svm_new)
#define DRM_IOCTL_MSM_SUBMITQUEUE_NEW \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_SUBMITQUEUE_NEW, \
		struct drm_msm_submitqueue)
#define DRM_IOCTL_MSM_SUBMITQUEUE_CLOSE \
	DRM_IOW(DRM_COMMAND_BASE + DRM_MSM_SUBMITQUEUE_CLOSE, \
		struct drm_msm_submitqueue)
#define DRM_IOCTL_MSM_SUBMITQUEUE_QUERY \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_SUBMITQUEUE_QUERY, \
		struct drm_msm_submitqueue_query)
#define DRM_IOCTL_MSM_RMFB2 DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_RMFB2), unsigned int)

#if defined(__cplusplus)
}
#endif

#endif /* __MSM_DRM_H__ */

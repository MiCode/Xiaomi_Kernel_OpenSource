#ifndef _KGSL_DRM_H_
#define _KGSL_DRM_H_

#include "drm.h"

#define DRM_KGSL_GEM_CREATE 0x00
#define DRM_KGSL_GEM_PREP   0x01
#define DRM_KGSL_GEM_SETMEMTYPE 0x02
#define DRM_KGSL_GEM_GETMEMTYPE 0x03
#define DRM_KGSL_GEM_MMAP 0x04
#define DRM_KGSL_GEM_ALLOC 0x05
#define DRM_KGSL_GEM_BIND_GPU 0x06
#define DRM_KGSL_GEM_UNBIND_GPU 0x07

#define DRM_KGSL_GEM_GET_BUFINFO 0x08
#define DRM_KGSL_GEM_SET_BUFCOUNT 0x09
#define DRM_KGSL_GEM_SET_ACTIVE 0x0A
/*
 * Do not use ioctl code 0x0B, 0x0C and 0x0D
 * to maintain backward compatibility
 */
#define DRM_KGSL_GEM_CREATE_FD 0x0E
#define DRM_KGSL_GEM_GET_ION_FD 0x0F
#define DRM_KGSL_GEM_CREATE_FROM_ION 0x10
#define DRM_KGSL_GEM_SET_GLOCK_HANDLES_INFO 0x11
#define DRM_KGSL_GEM_GET_GLOCK_HANDLES_INFO 0x12
#define DRM_KGSL_GEM_GET_BUFCOUNT 0x13
#define DRM_KGSL_GEM_SET_USERDATA 0x14
#define DRM_KGSL_GEM_GET_USERDATA 0x15
#define DRM_KGSL_GEM_CACHE_OPS 0x16


#define DRM_IOCTL_KGSL_GEM_CREATE \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_CREATE, struct drm_kgsl_gem_create)

#define DRM_IOCTL_KGSL_GEM_PREP \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_PREP, struct drm_kgsl_gem_prep)

#define DRM_IOCTL_KGSL_GEM_SETMEMTYPE \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_SETMEMTYPE, \
struct drm_kgsl_gem_memtype)

#define DRM_IOCTL_KGSL_GEM_GETMEMTYPE \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_GETMEMTYPE, \
struct drm_kgsl_gem_memtype)

#define DRM_IOCTL_KGSL_GEM_MMAP \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_MMAP, struct drm_kgsl_gem_mmap)

#define DRM_IOCTL_KGSL_GEM_ALLOC \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_ALLOC, struct drm_kgsl_gem_alloc)

#define DRM_IOCTL_KGSL_GEM_BIND_GPU \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_BIND_GPU, struct drm_kgsl_gem_bind_gpu)

#define DRM_IOCTL_KGSL_GEM_UNBIND_GPU \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_UNBIND_GPU, \
struct drm_kgsl_gem_bind_gpu)

#define DRM_IOCTL_KGSL_GEM_GET_BUFINFO \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_GET_BUFINFO, \
	 struct drm_kgsl_gem_bufinfo)

#define DRM_IOCTL_KGSL_GEM_SET_BUFCOUNT \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_SET_BUFCOUNT, \
	 struct drm_kgsl_gem_bufcount)

#define DRM_IOCTL_KGSL_GEM_GET_BUFCOUNT \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_GET_BUFCOUNT, \
	 struct drm_kgsl_gem_bufcount)

#define DRM_IOCTL_KGSL_GEM_SET_ACTIVE \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_SET_ACTIVE, \
	 struct drm_kgsl_gem_active)

#define DRM_IOCTL_KGSL_GEM_CREATE_FD \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_CREATE_FD, \
struct drm_kgsl_gem_create_fd)

#define DRM_IOCTL_KGSL_GEM_GET_ION_FD \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_GET_ION_FD, \
struct drm_kgsl_gem_get_ion_fd)

#define DRM_IOCTL_KGSL_GEM_CREATE_FROM_ION \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_CREATE_FROM_ION, \
struct drm_kgsl_gem_create_from_ion)

#define DRM_IOCTL_KGSL_GEM_SET_GLOCK_HANDLES_INFO \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_SET_GLOCK_HANDLES_INFO, \
struct drm_kgsl_gem_glockinfo)

#define DRM_IOCTL_KGSL_GEM_GET_GLOCK_HANDLES_INFO \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_GET_GLOCK_HANDLES_INFO, \
struct drm_kgsl_gem_glockinfo)

#define DRM_IOCTL_KGSL_GEM_SET_USERDATA \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_SET_USERDATA, \
struct drm_kgsl_gem_userdata)

#define DRM_IOCTL_KGSL_GEM_GET_USERDATA \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_GET_USERDATA, \
struct drm_kgsl_gem_userdata)

#define DRM_IOCTL_KGSL_GEM_CACHE_OPS \
DRM_IOWR(DRM_COMMAND_BASE + DRM_KGSL_GEM_CACHE_OPS, \
struct drm_kgsl_gem_cache_ops)


/* Maximum number of sub buffers per GEM object */
#define DRM_KGSL_GEM_MAX_BUFFERS 3

/* Memory types - these define the source and caching policies
   of the GEM memory chunk */

/* Legacy definitions left for compatability */

#define DRM_KGSL_GEM_TYPE_EBI          0
#define DRM_KGSL_GEM_TYPE_SMI          1
#define DRM_KGSL_GEM_TYPE_KMEM         2
#define DRM_KGSL_GEM_TYPE_KMEM_NOCACHE 3
#define DRM_KGSL_GEM_TYPE_MEM_SECURE   4
#define DRM_KGSL_GEM_TYPE_MEM_MASK     0xF

/* Contiguous memory (PMEM) */
#define DRM_KGSL_GEM_TYPE_PMEM       0x000100

/* PMEM memory types */
#define DRM_KGSL_GEM_PMEM_EBI        0x001000
#define DRM_KGSL_GEM_PMEM_SMI        0x002000

/* Standard paged memory */
#define DRM_KGSL_GEM_TYPE_MEM        0x010000

/* Secure memory */

/* Caching controls */
#define DRM_KGSL_GEM_CACHE_NONE        0x000000
#define DRM_KGSL_GEM_CLEAN_CACHES      0x100000
#define DRM_KGSL_GEM_INV_CACHES        0x200000
#define DRM_KGSL_GEM_CLEAN_INV_CACHES  0x400000
#define DRM_KGSL_GEM_CACHE_MASK        0xF00000




/* FD based objects */
#define DRM_KGSL_GEM_TYPE_FD_FBMEM   0x1000000
#define DRM_KGSL_GEM_TYPE_FD_MASK    0xF000000

struct drm_kgsl_gem_create {
	uint32_t size;
	uint32_t handle;
};

struct drm_kgsl_gem_prep {
	uint32_t handle;
	uint32_t phys;
	uint64_t offset;
};

struct drm_kgsl_gem_memtype {
	uint32_t handle;
	uint32_t type;
};

struct drm_kgsl_gem_mmap {
	uint32_t handle;
	uint32_t size;
	uint32_t hostptr;
	uint64_t offset;
};

struct drm_kgsl_gem_alloc {
	uint32_t handle;
	uint64_t offset;
};

struct drm_kgsl_gem_bind_gpu {
	uint32_t handle;
	uint32_t gpuptr;
};

struct drm_kgsl_gem_bufinfo {
	uint32_t handle;
	uint32_t count;
	uint32_t active;
	uint32_t offset[DRM_KGSL_GEM_MAX_BUFFERS];
	uint32_t gpuaddr[DRM_KGSL_GEM_MAX_BUFFERS];
};

struct drm_kgsl_gem_glockinfo {
	uint32_t handle;
	int glockhandle[DRM_KGSL_GEM_MAX_BUFFERS];
};

struct drm_kgsl_gem_bufcount {
	uint32_t handle;
	uint32_t bufcount;
};

struct drm_kgsl_gem_active {
	uint32_t handle;
	uint32_t active;
};

struct drm_kgsl_gem_create_fd {
	uint32_t fd;
	uint32_t handle;
};

struct drm_kgsl_gem_get_ion_fd {
	uint32_t ion_fd;
	uint32_t handle;
};

struct drm_kgsl_gem_create_from_ion {
	uint32_t ion_fd;
	uint32_t handle;
};

struct drm_kgsl_gem_userdata {
	uint32_t priv_data;
	uint32_t handle;
};

struct drm_kgsl_gem_cache_ops {
	uint32_t flags;
	uint32_t handle;
	void *vaddr;
	uint32_t length;
};

#endif

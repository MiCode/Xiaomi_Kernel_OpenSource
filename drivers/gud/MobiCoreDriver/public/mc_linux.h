/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_LINUX_H_
#define _MC_LINUX_H_

#include "version.h"

#ifndef __KERNEL__
#include <stdint.h>
#endif

#define MC_ADMIN_DEVNODE	"mobicore"
#define MC_USER_DEVNODE		"mobicore-user"

/*
 * Data exchange structure of the MC_DRV_MODULE_INIT ioctl command.
 * INIT request data to SWD
 */
struct mc_ioctl_init {
	/* length of notification queue */
	uint32_t  nq_length;
	/* mcp buffer start/length [16:16] [start, length] */
	uint32_t  mcp_offset;
	/* length of mcp buffer */
	uint32_t  mcp_length;
};

/*
 * Data exchange structure of the MC_DRV_MODULE_INFO ioctl command.
 * INFO request data to the SWD
 */
struct mc_ioctl_info {
	uint32_t  ext_info_id;	/* extended info ID */
	uint32_t  state;	/* state */
	uint32_t  ext_info;	/* extended info */
};

/*
 * Data exchange structure of the MC_IO_MAP_WSM and MC_IO_MAP_MCI commands.
 *
 * Allocate a contiguous memory buffer for a process.
 * The physical address can be used as for later calls to mmap.
 * The handle can be used to communicate about this buffer to the Daemon.
 * For MC_IO_MAP_MCI command, the reused field indicates that MCI was set up
 * already. I.e. Daemon was restarted.
 */
struct mc_ioctl_map {
	uint32_t	len;	/* Buffer length */
	uint32_t	handle;	/* WSM handle */
	uint64_t	phys_addr; /* physical address of WSM (or 0) */
	uint32_t	rfu;
	bool		reused;	/* if WSM memory was reused, or new allocated */
};

/*
 * Data exchange structure of the MC_IO_REG_WSM command.
 *
 * Allocates a physical MMU table and maps the buffer into this page.
 * Returns the physical address of the MMU table.
 * The page alignment will be created and the appropriated pSize and pOffsetMMU
 * will be modified to the used values.
 *
 * We assume the 64 bit compatible one to be the default and the
 * 32 bit one to be the compat one but we must serve both of them.
 */
struct mc_compat_ioctl_reg_wsm {
	uint32_t buffer;	/* base address of the virtual address  */
	uint32_t len;		/* size of the virtual address space */
	uint32_t pid;		/* process id */
	uint32_t handle;	/* driver handle for locked memory */
	uint64_t table_phys;	/* physical address of the MMU table */
};

struct mc_ioctl_reg_wsm {
	uint64_t buffer;	/* base address of the virtual address  */
	uint32_t len;		/* size of the virtual address space */
	uint32_t pid;		/* process id */
	uint32_t handle;	/* driver handle for locked memory */
	uint64_t table_phys;/* physical address of the MMU table */
};

/*
 * Data exchange structure of the MC_IO_RESOLVE_CONT_WSM ioctl command.
 */
struct mc_ioctl_resolv_cont_wsm {
	/* driver handle for buffer */
	uint32_t handle;
	/* length memory */
	uint32_t length;
	/* base address of memory */
	uint64_t phys;
	/* fd to owner of the buffer */
	int32_t fd;
};

/*
 * Data exchange structure of the MC_IO_RESOLVE_WSM ioctl command.
 */
struct mc_ioctl_resolv_wsm {
	/* driver handle for buffer */
	uint32_t handle;
	/* fd to owner of the buffer */
	int32_t fd;
	/* base address of memory */
	uint64_t phys;
};


/*
 * defines for the ioctl mobicore driver module function call from user space.
 */
/* MobiCore IOCTL magic number */
#define MC_IOC_MAGIC	'M'

#define MC_IO_INIT		_IOWR(MC_IOC_MAGIC, 0, struct mc_ioctl_init)
#define MC_IO_INFO		_IOWR(MC_IOC_MAGIC, 1, struct mc_ioctl_info)
#define MC_IO_VERSION		_IOR(MC_IOC_MAGIC, 2, uint32_t)
/*
 * ioctl parameter to send the YIELD command to the SWD.
 * Only possible in Privileged Mode.
 * ioctl(fd, MC_DRV_MODULE_YIELD)
 */
#define MC_IO_YIELD		_IO(MC_IOC_MAGIC, 3)
/*
 * ioctl parameter to send the NSIQ signal to the SWD.
 * Only possible in Privileged Mode
 * ioctl(fd, MC_DRV_MODULE_NSIQ)
 */
#define MC_IO_NSIQ		_IO(MC_IOC_MAGIC, 4)
/*
 * Free's memory which is formerly allocated by the driver's mmap
 * command. The parameter must be this mmaped address.
 * The internal instance data regarding to this address are deleted as
 * well as each according memory page and its appropriated reserved bit
 * is cleared (ClearPageReserved).
 * Usage: ioctl(fd, MC_DRV_MODULE_FREE, &address) with address being of
 * type long address
 */
#define MC_IO_FREE		_IO(MC_IOC_MAGIC, 5)
/*
 * Creates a MMU Table of the given base address and the size of the
 * data.
 * Parameter: mc_ioctl_reg_wsm
 *
 * Since the end ID is also based on the size of the structure it is
 * safe to use the same ID(6) for both
 */
#define MC_IO_REG_WSM		_IOWR(MC_IOC_MAGIC, 6, struct mc_ioctl_reg_wsm)
#define MC_COMPAT_REG_WSM	_IOWR(MC_IOC_MAGIC, 6, \
			struct mc_compat_ioctl_reg_wsm)

#define MC_IO_UNREG_WSM		_IO(MC_IOC_MAGIC, 7)
#define MC_IO_LOCK_WSM		_IO(MC_IOC_MAGIC, 8)
#define MC_IO_UNLOCK_WSM	_IO(MC_IOC_MAGIC, 9)

/*
 * Allocate contiguous memory for a process for later mapping with mmap.
 * MC_IO_MAP_WSM	usual operation, pages are registered in
 *					device structure and freed later.
 * MC_IO_MAP_MCI	get Instance of MCI, allocates or mmaps
 *					the MCI to daemon
 */
#define MC_IO_MAP_WSM		_IOWR(MC_IOC_MAGIC, 11, struct mc_ioctl_map)
#define MC_IO_MAP_MCI		_IOWR(MC_IOC_MAGIC, 12, struct mc_ioctl_map)

/*
 * Clean orphaned WSM buffers. Only available to the daemon and should
 * only be carried out if the TLC crashes or otherwise calls exit() in
 * an unexpected manner.
 * The clean is needed together with the lock/unlock mechanism so the daemon
 * has clear control of the mapped buffers so it can close a Trustlet before
 * release all the WSM buffers, otherwise the Trustlet would be able to write
 * to possibly kernel memory areas
 */
#define MC_IO_CLEAN_WSM		_IO(MC_IOC_MAGIC, 14)

/*
 * Get MMU phys address of a buffer handle allocated to the user.
 * Only available to the daemon.
 */
#define MC_IO_RESOLVE_WSM	_IOWR(MC_IOC_MAGIC, 15, \
					struct mc_ioctl_resolv_wsm)

/*
 * Get the phys address & length of a allocated contiguous buffer.
 * Only available to the daemon */
#define MC_IO_RESOLVE_CONT_WSM	_IOWR(MC_IOC_MAGIC, 16, \
					struct mc_ioctl_resolv_cont_wsm)

/*
 * Setup the mem traces when called.
 * Only available to the daemon */
#define MC_IO_LOG_SETUP		_IO(MC_IOC_MAGIC, 17)

#endif /* _MC_LINUX_H_ */

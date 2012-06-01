/** @addtogroup MCD_MCDIMPL_KMOD_API Mobicore Driver Module API
 * @ingroup  MCD_MCDIMPL_KMOD
 * @{
 * Interface to Mobicore Driver Kernel Module.
 * @file
 *
 * <h2>Introduction</h2>
 * The MobiCore Driver Kernel Module is a Linux device driver, which represents
 * the command proxy on the lowest layer to the secure world (Swd). Additional
 * services like memory allocation via mmap and generation of a L2 tables for
 * given virtual memory are also supported. IRQ functionallity receives
 * information from the SWd in the non secure world (NWd).
 * As customary the driver is handled as linux device driver with "open",
 * "close" and "ioctl" commands. Access to the driver is possible after the
 * device "/dev/mobicore" has been opened.
 * The MobiCore Driver Kernel Module must be installed via
 * "insmod mcDrvModule.ko".
 *
 *
 * <h2>Version history</h2>
 * <table class="customtab">
 * <tr><td width="100px"><b>Date</b></td><td width="80px"><b>Version</b></td>
 * <td><b>Changes</b></td></tr>
 * <tr><td>2010-05-25</td><td>0.1</td><td>Initial Release</td></tr>
 * </table>
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2010-2012 -->
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *	products derived from this software without specific prior
 *	written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MC_DRV_MODULEAPI_H_
#define _MC_DRV_MODULEAPI_H_

#include "version.h"

#define MC_DRV_MOD_DEVNODE		   "mobicore"
#define MC_DRV_MOD_DEVNODE_FULLPATH  "/dev/" MC_DRV_MOD_DEVNODE

/**
 * Data exchange structure of the MC_DRV_MODULE_INIT ioctl command.
 * INIT request data to SWD
 */
union mc_ioctl_init_params {
	struct {
		/** base address of mci buffer 4KB align */
		uint32_t  base;
		/** notification buffer start/length [16:16] [start, length] */
		uint32_t  nq_offset;
		/** length of notification queue */
		uint32_t  nq_length;
		/** mcp buffer start/length [16:16] [start, length] */
		uint32_t  mcp_offset;
		/** length of mcp buffer */
		uint32_t  mcp_length;
	} in;
	struct {
		/* nothing */
	} out;
};


/**
 * Data exchange structure of the MC_DRV_MODULE_INFO ioctl command.
 * INFO request data to the SWD
 */
union mc_ioctl_info_params {
	struct {
		uint32_t  ext_info_id; /**< extended info ID */
	} in;
	struct {
		uint32_t  state; /**< state */
		uint32_t  ext_info; /**< extended info */
	} out;
};

/**
 * Mmap allocates and maps contiguous memory into a process.
 * We use the third parameter, void *offset, to distinguish between some cases
 * offset = MC_DRV_KMOD_MMAP_WSM	usual operation, pages are registered in
					device structure and freed later.
 * offset = MC_DRV_KMOD_MMAP_MCI	get Instance of MCI, allocates or mmaps
					the MCI to daemon
 * offset = MC_DRV_KMOD_MMAP_PERSISTENTWSM	special operation, without
						registration of pages
 *
 * In mmap(), the offset specifies which of several device I/O pages is
 *  requested. Linux only transfers the page number, i.e. the upper 20 bits to
 *  kernel module. Therefore we define our special offsets as multiples of page
 *  size.
 */
enum mc_mmap_memtype {
	MC_DRV_KMOD_MMAP_WSM		= 0,
	MC_DRV_KMOD_MMAP_MCI		= 4096,
	MC_DRV_KMOD_MMAP_PERSISTENTWSM	= 8192
};

struct mc_mmap_resp {
	uint32_t  handle; /**< WSN handle */
	uint32_t  phys_addr; /**< physical address of WSM (or NULL) */
	bool	  is_reused; /**< if WSM memory was reused, or new allocated */
};

/**
 * Data exchange structure of the MC_DRV_KMOD_IOCTL_FREE ioctl command.
 */
union mc_ioctl_free_params {
	struct {
		uint32_t  handle; /**< driver handle */
		uint32_t  pid; /**< process id */
	} in;
	struct {
		/* nothing */
	} out;
};


/**
 * Data exchange structure of the MC_DRV_KMOD_IOCTL_APP_REGISTER_WSM_L2 command.
 *
 * Allocates a physical L2 table and maps the buffer into this page.
 * Returns the physical address of the L2 table.
 * The page alignment will be created and the appropriated pSize and pOffsetL2
 * will be modified to the used values.
 */
union mc_ioctl_app_reg_wsm_l2_params {
	struct {
		uint32_t  buffer; /**< base address of the virtual address  */
		uint32_t  len; /**< size of the virtual address space */
		uint32_t  pid; /**< process id */
	} in;
	struct {
		uint32_t  handle; /**< driver handle for locked memory */
		uint32_t  phys_wsm_l2_table; /* physical address of the L2 table */
	} out;
};


/**
 * Data exchange structure of the MC_DRV_KMOD_IOCTL_APP_UNREGISTER_WSM_L2
 * command.
 */
struct mc_ioctl_app_unreg_wsm_l2_params {
	struct {
		uint32_t  handle; /**< driver handle for locked memory */
		uint32_t  pid; /**< process id */
	} in;
	struct {
		/* nothing */
	} out;
};


/**
 * Data exchange structure of the MC_DRV_KMOD_IOCTL_DAEMON_LOCK_WSM_L2 command.
 */
struct mc_ioctl_daemon_lock_wsm_l2_params {
	struct {
		uint32_t  handle; /**< driver handle for locked memory */
	} in;
	struct {
		uint32_t phys_wsm_l2_table;
	} out;
};


/**
 * Data exchange structure of the MC_DRV_KMOD_IOCTL_DAEMON_UNLOCK_WSM_L2
 * command.
 */
struct mc_ioctl_daemon_unlock_wsm_l2_params {
	struct {
		uint32_t  handle; /**< driver handle for locked memory */
	} in;
	struct {
		/* nothing */
	} out;
};

/**
 * Data exchange structure of the MC_DRV_MODULE_FC_EXECUTE ioctl command.
 */
union mc_ioctl_fc_execute_params {
	struct {
		/**< base address of mobicore binary */
		uint32_t  phys_start_addr;
		/**< length of DDR area */
		uint32_t  length;
	} in;
	struct {
		/* nothing */
	} out;
};

/**
 * Data exchange structure of the MC_DRV_MODULE_GET_VERSION ioctl command.
 */
struct mc_ioctl_get_version_params {
	struct {
		uint32_t	kernel_module_version;
	} out;
};

/* @defgroup Mobicore_Driver_Kernel_Module_Interface IOCTL */




/* TODO: use IOCTL macros like _IOWR. See Documentation/ioctl/ioctl-number.txt,
	Documentation/ioctl/ioctl-decoding.txt */
/**
 * defines for the ioctl mobicore driver module function call from user space.
 */
enum mc_kmod_ioctl {

	/*
	 * get detailed MobiCore Status
	 */
	MC_DRV_KMOD_IOCTL_DUMP_STATUS  = 200,

	/*
	 * initialize MobiCore
	 */
	MC_DRV_KMOD_IOCTL_FC_INIT  = 201,

	/*
	 * get MobiCore status
	 */
	MC_DRV_KMOD_IOCTL_FC_INFO  = 202,

	/**
	 * ioctl parameter to send the YIELD command to the SWD.
	 * Only possible in Privileged Mode.
	 * ioctl(fd, MC_DRV_MODULE_YIELD)
	 */
	MC_DRV_KMOD_IOCTL_FC_YIELD =  203,
	/**
	 * ioctl parameter to send the NSIQ signal to the SWD.
	 * Only possible in Privileged Mode
	 * ioctl(fd, MC_DRV_MODULE_NSIQ)
	 */
	MC_DRV_KMOD_IOCTL_FC_NSIQ   =  204,
	/**
	 * ioctl parameter to tzbsp to start Mobicore binary from DDR.
	 * Only possible in Privileged Mode
	 * ioctl(fd, MC_DRV_KMOD_IOCTL_FC_EXECUTE)
	 */
	MC_DRV_KMOD_IOCTL_FC_EXECUTE =  205,

	/**
	 * Free's memory which is formerly allocated by the driver's mmap
	 * command. The parameter must be this mmaped address.
	 * The internal instance data regarding to this address are deleted as
	 * well as each according memory page and its appropriated reserved bit
	 * is cleared (ClearPageReserved).
	 * Usage: ioctl(fd, MC_DRV_MODULE_FREE, &address) with address beeing of
	 * type long address
	 */
	MC_DRV_KMOD_IOCTL_FREE = 218,

	/**
	 * Creates a L2 Table of the given base address and the size of the
	 * data.
	 * Parameter: mc_ioctl_app_reg_wsm_l2_params
	 */
	MC_DRV_KMOD_IOCTL_APP_REGISTER_WSM_L2 = 220,

	/**
	 * Frees the L2 table created by a MC_DRV_KMOD_IOCTL_APP_REGISTER_WSM_L2
	 * ioctl.
	 * Parameter: mc_ioctl_app_unreg_wsm_l2_params
	 */
	MC_DRV_KMOD_IOCTL_APP_UNREGISTER_WSM_L2 = 221,


	/* TODO: comment this. */
	MC_DRV_KMOD_IOCTL_DAEMON_LOCK_WSM_L2 = 222,
	MC_DRV_KMOD_IOCTL_DAEMON_UNLOCK_WSM_L2 = 223,

	/**
	 * Return kernel driver version.
	 * Parameter: mc_ioctl_get_version_params
	 */
	MC_DRV_KMOD_IOCTL_GET_VERSION = 224,
};


#endif /* _MC_DRV_MODULEAPI_H_ */
/** @} */

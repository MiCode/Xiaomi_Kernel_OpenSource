/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __TF_DEFS_H__
#define __TF_DEFS_H__

#include <linux/atomic.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#include "tf_protocol.h"

#ifdef CONFIG_TF_ION
#include <linux/ion.h>
#include <linux/omap_ion.h>
#endif

/*----------------------------------------------------------------------------*/

#define SIZE_1KB 0x400

/*
 * Maximum number of shared memory blocks that can be reigsters in a connection
 */
#define TF_SHMEM_MAX_COUNT   (64)

/*
 * Describes the possible types of shared memories
 *
 * TF_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM :
 *    The descriptor describes a registered shared memory.
 *    Its coarse pages are preallocated when initializing the
 *    connection
 * TF_SHMEM_TYPE_REGISTERED_SHMEM :
 *    The descriptor describes a registered shared memory.
 *    Its coarse pages are not preallocated
 * TF_SHMEM_TYPE_PM_HIBERNATE :
 *    The descriptor describes a power management shared memory.
 */
enum TF_SHMEM_TYPE {
	TF_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM = 0,
	TF_SHMEM_TYPE_REGISTERED_SHMEM,
	TF_SHMEM_TYPE_PM_HIBERNATE,
};


/*
 * This structure contains a pointer on a coarse page table
 */
struct tf_coarse_page_table {
	/*
	 * Identifies the coarse page table descriptor in
	 * free_coarse_page_tables list
	 */
	struct list_head list;

	/*
	 * The address of the coarse page table
	 */
	u32 *descriptors;

	/*
	 * The address of the array containing this coarse page table
	 */
	struct tf_coarse_page_table_array *parent;
};


#define TF_PAGE_DESCRIPTOR_TYPE_NORMAL       0
#define TF_PAGE_DESCRIPTOR_TYPE_PREALLOCATED 1

/*
 * This structure describes an array of up to 4 coarse page tables
 * allocated within a single 4KB page.
 */
struct tf_coarse_page_table_array {
	/*
	 * identifies the element in the coarse_page_table_arrays list
	 */
	struct list_head list;

	/*
	 * Type of page descriptor
	 * can take any of TF_PAGE_DESCRIPTOR_TYPE_XXX value
	 */
	u32 type;

	struct tf_coarse_page_table coarse_page_tables[4];

	/*
	 * A counter of the number of coarse pages currently used
	 * the max value should be 4 (one coarse page table is 1KB while one
	 * page is 4KB)
	 */
	u8 ref_count;
};


/*
 * This structure describes a list of coarse page table arrays
 * with some of the coarse page tables free. It is used
 * when the driver needs to allocate a new coarse page
 * table.
 */
struct tf_coarse_page_table_allocation_context {
	/*
	 * The spin lock protecting concurrent access to the structure.
	 */
	spinlock_t lock;

	/*
	 * The list of allocated coarse page table arrays
	 */
	struct list_head coarse_page_table_arrays;

	/*
	 * The list of free coarse page tables
	 */
	struct list_head free_coarse_page_tables;
};


/*
 * Fully describes a shared memory block
 */
struct tf_shmem_desc {
	/*
	 * Identifies the shared memory descriptor in the list of free shared
	 * memory descriptors
	 */
	struct list_head list;

	/*
	 * Identifies the type of shared memory descriptor
	 */
	enum TF_SHMEM_TYPE type;

	/*
	 * The identifier of the block of shared memory, as returned by the
	 * Secure World.
	 * This identifier is block field of a REGISTER_SHARED_MEMORY answer
	 */
	u32 block_identifier;

	/* Client buffer */
	u8 *client_buffer;

	/* Up to eight coarse page table context */
	struct tf_coarse_page_table *coarse_pg_table[TF_MAX_COARSE_PAGES];

	u32 coarse_pg_table_count;

	/* Reference counter */
	atomic_t ref_count;
};


/*----------------------------------------------------------------------------*/

/*
 * This structure describes the communication with the Secure World
 *
 * Note that this driver supports only one instance of the Secure World
 */
struct tf_comm {
	/*
	 * The spin lock protecting concurrent access to the structure.
	 */
	spinlock_t lock;

	/*
	 * Bit vector with the following possible flags:
	 *    - TF_COMM_FLAG_IRQ_REQUESTED: If set, indicates that
	 *      the IRQ has been successfuly requested.
	 *    - TF_COMM_FLAG_TERMINATING: If set, indicates that the
	 *      communication with the Secure World is being terminated.
	 *      Transmissions to the Secure World are not permitted
	 *    - TF_COMM_FLAG_W3B_ALLOCATED: If set, indicates that the
	 *      W3B buffer has been allocated.
	 *
	 * This bit vector must be accessed with the kernel's atomic bitwise
	 * operations.
	 */
	unsigned long flags;

	/*
	 * The virtual address of the L1 shared buffer.
	 */
	struct tf_l1_shared_buffer *l1_buffer;

	/*
	 * The wait queue the client threads are waiting on.
	 */
	wait_queue_head_t wait_queue;

#ifdef CONFIG_TF_TRUSTZONE
	/*
	 * The interrupt line used by the Secure World.
	 */
	int soft_int_irq;

	/* ----- W3B ----- */
	/* shared memory descriptor to identify the W3B */
	struct tf_shmem_desc w3b_shmem_desc;

	/* Virtual address of the kernel allocated shared memory */
	u32 w3b;

	/* offset of data in shared memory coarse pages */
	u32 w3b_shmem_offset;

	u32 w3b_shmem_size;

	struct tf_coarse_page_table_allocation_context
		w3b_cpt_alloc_context;
#endif
#ifdef CONFIG_TF_ZEBRA
	/*
	 * The SE SDP can only be initialized once...
	 */
	int se_initialized;

	/*
	 * Lock to be held by a client when executing an RPC
	 */
	struct mutex rpc_mutex;

	/*
	 * Lock to protect concurrent accesses to DMA channels
	 */
	struct mutex dma_mutex;
#endif
};


#define TF_COMM_FLAG_IRQ_REQUESTED		(0)
#define TF_COMM_FLAG_PA_AVAILABLE		(1)
#define TF_COMM_FLAG_TERMINATING		(2)
#define TF_COMM_FLAG_W3B_ALLOCATED		(3)
#define TF_COMM_FLAG_L1_SHARED_ALLOCATED	(4)

/*----------------------------------------------------------------------------*/

struct tf_device_stats {
	atomic_t stat_pages_allocated;
	atomic_t stat_memories_allocated;
	atomic_t stat_pages_locked;
};

/*
 * This structure describes the information about one device handled by the
 * driver. Note that the driver supports only a single device. see the global
 * variable g_tf_dev

 */
struct tf_device {
	/*
	 * The kernel object for the device
	 */
	struct kobject kobj;

	/*
	 * The device number for the device.
	 */
	dev_t dev_number;

	/*
	 * Interfaces the char device with the kernel.
	 */
	struct cdev cdev;

#ifdef CONFIG_TF_TEEC
	struct cdev cdev_teec;
#endif

#ifdef CONFIG_TF_ZEBRA
	struct cdev cdev_ctrl;

	/*
	 * Globals for CUS
	 */
	/* Current key handles loaded in HWAs */
	u32 aes1_key_context;
	u32 des_key_context;
	bool sham1_is_public;

	/* Object used to serialize HWA accesses */
	struct semaphore aes1_sema;
	struct semaphore des_sema;
	struct semaphore sha_sema;

	/*
	 * An aligned and correctly shaped pre-allocated buffer used for DMA
	 * transfers
	 */
	u32 dma_buffer_length;
	u8 *dma_buffer;
	dma_addr_t dma_buffer_phys;

	/* Workspace allocated at boot time and reserved to the Secure World */
	u32 workspace_addr;
	u32 workspace_size;

	/*
	* A Mutex to provide exclusive locking of the ioctl()
	*/
	struct mutex dev_mutex;
#endif

	/*
	 * Communications with the SM.
	 */
	struct tf_comm sm;

	/*
	 * Lists the connections attached to this device.  A connection is
	 * created each time a user space application "opens" a file descriptor
	 * on the driver
	 */
	struct list_head connection_list;

	/*
	 * The spin lock used to protect concurrent access to the connection
	 * list.
	 */
	spinlock_t connection_list_lock;

	struct tf_device_stats stats;
};

/*----------------------------------------------------------------------------*/
/*
 * This type describes a connection state.
 * This is used to determine whether a message is valid or not.
 *
 * Messages are only valid in a certain device state.
 * Messages may be invalidated between the start of the ioctl call and the
 * moment the message is sent to the Secure World.
 *
 * TF_CONN_STATE_NO_DEVICE_CONTEXT :
 *    The connection has no DEVICE_CONTEXT created and no
 *    CREATE_DEVICE_CONTEXT being processed by the Secure World
 * TF_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT :
 *    The connection has a CREATE_DEVICE_CONTEXT being processed by the Secure
 *    World
 * TF_CONN_STATE_VALID_DEVICE_CONTEXT :
 *    The connection has a DEVICE_CONTEXT created and no
 *    DESTROY_DEVICE_CONTEXT is being processed by the Secure World
 * TF_CONN_STATE_DESTROY_DEVICE_CONTEXT_SENT :
 *    The connection has a DESTROY_DEVICE_CONTEXT being processed by the Secure
 *    World
 */
enum TF_CONN_STATE {
	TF_CONN_STATE_NO_DEVICE_CONTEXT = 0,
	TF_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT,
	TF_CONN_STATE_VALID_DEVICE_CONTEXT,
	TF_CONN_STATE_DESTROY_DEVICE_CONTEXT_SENT
};


/*
 *  This type describes the  status of the command.
 *
 *  PENDING:
 *     The initial state; the command has not been sent yet.
 *	SENT:
 *     The command has been sent, we are waiting for an answer.
 *	ABORTED:
 *     The command cannot be sent because the device context is invalid.
 *     Note that this only covers the case where some other thread
 *     sent a DESTROY_DEVICE_CONTEXT command.
 */
enum TF_COMMAND_STATE {
	TF_COMMAND_STATE_PENDING = 0,
	TF_COMMAND_STATE_SENT,
	TF_COMMAND_STATE_ABORTED
};

/*
 * The origin of connection parameters such as login data and
 * memory reference pointers.
 *
 * PROCESS: the calling process. All arguments must be validated.
 * KERNEL: kernel code. All arguments can be trusted by this driver.
 */
enum TF_CONNECTION_OWNER {
	TF_CONNECTION_OWNER_PROCESS = 0,
	TF_CONNECTION_OWNER_KERNEL,
};


/*
 * This structure describes a connection to the driver
 * A connection is created each time an application opens a file descriptor on
 * the driver
 */
struct tf_connection {
	/*
	 * Identifies the connection in the list of the connections attached to
	 * the same device.
	 */
	struct list_head list;

	/*
	 * State of the connection.
	 */
	enum TF_CONN_STATE state;

	/*
	 * A pointer to the corresponding device structure
	 */
	struct tf_device *dev;

	/*
	 * A spinlock to use to access state
	 */
	spinlock_t state_lock;

	/*
	 * Counts the number of operations currently pending on the connection.
	 * (for debug only)
	 */
	atomic_t pending_op_count;

	/*
	 * A handle for the device context
	 */
	 u32 device_context;

	/*
	 * Lists the used shared memory descriptors
	 */
	struct list_head used_shmem_list;

	/*
	 * Lists the free shared memory descriptors
	 */
	struct list_head free_shmem_list;

	/*
	 * A mutex to use to access this structure
	 */
	struct mutex shmem_mutex;

	/*
	 * Counts the number of shared memories registered.
	 */
	atomic_t shmem_count;

	/*
	 * Page to retrieve memory properties when
	 * registering shared memory through REGISTER_SHARED_MEMORY
	 * messages
	 */
	struct vm_area_struct **vmas;

	/*
	 * coarse page table allocation context
	 */
	struct tf_coarse_page_table_allocation_context cpt_alloc_context;

	/* The origin of connection parameters such as login data and
	   memory reference pointers. */
	enum TF_CONNECTION_OWNER owner;

#ifdef CONFIG_TF_ZEBRA
	/* Lists all the Cryptoki Update Shortcuts */
	struct list_head shortcut_list;

	/* Lock to protect concurrent accesses to shortcut_list */
	spinlock_t shortcut_list_lock;
#endif

#ifdef CONFIG_TF_ION
	struct ion_client *ion_client;
#endif
};

/*----------------------------------------------------------------------------*/

/*
 * The operation_id field of a message points to this structure.
 * It is used to identify the thread that triggered the message transmission
 * Whoever reads an answer can wake up that thread using the completion event
 */
struct tf_answer_struct {
	bool answer_copied;
	union tf_answer *answer;
};

/*----------------------------------------------------------------------------*/

/**
 * The ASCII-C string representation of the base name of the devices managed by
 * this driver.
 */
#define TF_DEVICE_BASE_NAME	"tf_driver"


/**
 * The major and minor numbers of the registered character device driver.
 * Only 1 instance of the driver is supported.
 */
#define TF_DEVICE_MINOR_NUMBER	(0)

struct tf_device *tf_get_device(void);

#define CLEAN_CACHE_CFG_MASK	(~0xC) /* 1111 0011 */

/*----------------------------------------------------------------------------*/
/*
 * Kernel Differences
 */

#ifdef CONFIG_ANDROID
#define GROUP_INFO		get_current_groups()
#else
#define GROUP_INFO		(current->group_info)
#endif

#endif  /* !defined(__TF_DEFS_H__) */

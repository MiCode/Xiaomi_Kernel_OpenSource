/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __VPU_CMN_H__
#define __VPU_CMN_H__

#include <linux/wait.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include "vpu_drv.h"
#include "vpu_dbg.h"
#include "vpu_pool.h"

#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif


#define VPU_MET_READY
#define VPU_PORT_OF_IOMMU M4U_PORT_VPU

/* Common Structure */
struct vpu_device {
	struct proc_dir_entry *proc_dir;
	struct device *dev[MTK_VPU_CORE+3];
	bool vpu_hw_support[MTK_VPU_CORE];
	struct dentry *debug_root;
	unsigned long vpu_syscfg_base;
	unsigned long vpu_adlctrl_base;
	unsigned long vpu_vcorecfg_base;
	unsigned long vpu_base[MTK_VPU_CORE];
	unsigned long smi_cmn_base;
	unsigned long bin_base;
	unsigned long bin_pa;
	unsigned int bin_size;
	unsigned int irq_num[MTK_VPU_CORE];
	unsigned int irq_trig_level;
	struct mutex sdsp_control_mutex[MTK_VPU_CORE];
	struct mutex user_mutex;
	/* list of vlist_type(struct vpu_user) */
	struct list_head user_list;
	/* pool for each vpu core and common part */
	/* need error handle, pop all requests in pool */
	bool service_core_available[MTK_VPU_CORE];
	struct vpu_pool pool[MTK_VPU_CORE];
	struct vpu_pool pool_common;
	struct vpu_pool pool_multiproc;
	/* notify enque thread */
	wait_queue_head_t req_wait;
	/*priority number list*/
	unsigned int priority_list[MTK_VPU_CORE][VPU_REQ_MAX_NUM_PRIORITY];
};

struct vpu_user {
	pid_t open_pid;
	pid_t open_tgid;
	unsigned long *id;
	/* to enque/deque must have mutex protection */
	struct mutex data_mutex;
	bool running[MTK_VPU_CORE];
	bool deleting;
	bool flushing;
	bool locked;
	/* list of vlist_type(struct vpu_request) */
	struct list_head enque_list;
	struct list_head deque_list;
	struct list_head algo_list;
	wait_queue_head_t deque_wait;
	wait_queue_head_t delete_wait;
	uint8_t power_mode;
	uint8_t power_opp;
	uint8_t algo_num;
};

struct vpu_shared_memory_param {
	uint32_t size;
	bool require_pa;
	bool require_va;
	uint32_t fixed_addr;
};

struct vpu_shared_memory {
	void *handle;
	uint64_t va;
	uint32_t pa;
	uint32_t length;
};

enum vpu_power_param {
	VPU_POWER_PARAM_FIX_OPP,
	VPU_POWER_PARAM_DVFS_DEBUG,
	VPU_POWER_PARAM_JTAG,
	VPU_POWER_PARAM_LOCK,
	VPU_POWER_PARAM_VOLT_STEP,
	VPU_POWER_HAL_CTL,
	VPU_EARA_CTL,
	VPU_CT_INFO,
	VPU_POWER_PARAM_DISABLE_OFF,
};

enum vpu_debug_algo_param {
	VPU_DEBUG_ALGO_PARAM_DUMP_ALGO,
};

/* enum & structure */
enum VpuCoreState {
	VCT_SHUTDOWN	= 1,
	VCT_BOOTUP		= 2,
	VCT_EXECUTING	= 3,
	VCT_IDLE		= 4,
	VCT_VCORE_CHG	= 5,
	VCT_NONE		= -1,
};

enum VpuPowerOnType {
	/* power on previously by setPower */
	VPT_PRE_ON		= 1,

	/* power on by enque */
	VPT_ENQUE_ON	= 2,

	/* power on by enque, but want to immediately off(when exception) */
	VPT_IMT_OFF		= 3,

	/* same as VPT_IMT_OFF, from secure DSP */
	VPT_SDSP_OFF	= 4,
};


#define DECLARE_VLIST(type) \
struct type ## _list { \
	struct type node; \
	struct list_head link; \
}

/*
 * vlist_node_of - get the pointer to the node which has specific vlist
 * @ptr:	the pointer to struct list_head
 * @type:	the type of list node
 */
#define vlist_node_of(ptr, type) ({ \
	const struct list_head *mptr = (ptr); \
	(type *)((char *)mptr - offsetof(type ## _list, link)); })

/*
 * vlist_link - get the pointer to struct list_head
 * @ptr:	the pointer to struct vlist
 * @type:	the type of list node
 */
#define vlist_link(ptr, type) \
	((struct list_head *)((char *)ptr + offsetof(type ## _list, link)))

/*
 * vlist_type - get the type of struct vlist
 * @type:	the type of list node
 */
#define vlist_type(type) type ## _list

/*
 * vlist_node - get the pointer to the node of vlist
 * @ptr:	the pointer to struct vlist
 * @type:	the type of list node
 */
#define vlist_node(ptr, type) ((type *) ptr)

DECLARE_VLIST(vpu_user);
DECLARE_VLIST(vpu_algo);
DECLARE_VLIST(vpu_request);
DECLARE_VLIST(vpu_dev_debug_info);

/* ========================= define in vpu_emu.c  ========================= */

/**
 * vpu_init_emulator - init a hw emulator to serve driver's operation
 * @device:     the pointer of vpu_device.
 */
int vpu_init_emulator(struct vpu_device *device);

/**
 * vpu_uninit_emulator - close resources related to emulator
 */
int vpu_uninit_emulator(void);

/**
 * vpu_request_emulator_irq - it will callback to handler while an operation of
 *                            emulator is done
 * @irq:        irq number
 * @handler:    irq handler
 */
int vpu_request_emulator_irq(uint32_t irq, irq_handler_t handler);



/* ========================== define in vpu_hw.c  =========================== */

/**
 * vpu_init_hw - init the procedure related to hw,
 *               include irq register and enque thread
 * @core:	core index of vpu_device.
 * @device:     the pointer of vpu_device.
 */
int vpu_init_hw(int core, struct vpu_device *device);

/**
 * vpu_uninit_hw - close resources related to hw module
 */
int vpu_uninit_hw(void);

/**
 * vpu_boot_up - boot up the vpu power and framework
 * @core: index of device
 */
int vpu_boot_up(int core, bool secure);

/**
 * vpu_shut_down - shutdown the vpu framework and power
 * @core: index of device
 */
int vpu_shut_down(int core);

/**
 * vpu_hw_load_algo - call vpu program to load algo, by specifying the
 *                    start address
 * @core:	core index of device.
 * @algo:       the pointer to struct algo, which has right binary-data info.
 */
int vpu_hw_load_algo(int core, struct vpu_algo *algo);

/**
 * vpu_get_name_of_algo - get the algo's name by its id
 * @core:	core index of vpu device
 * @id          the serial id
 * @name:       return the algo's name
 */
int vpu_get_name_of_algo(int core, int id, char **name);

/**
 * vpu_get_entry_of_algo - get the address and length from binary data
 * @core:       core index of vpu device
 * @name:       the algo's name
 * @id          return the serial id
 * @mva:        return the mva of algo binary
 * @length:     return the length of algo binary
 */
int vpu_get_entry_of_algo(int core, char *name, int *id,
	unsigned int *mva, int *length);

int vpu_get_default_algo_num(int core, vpu_id_t *algo_num);

int vpu_total_algo_num(int core);
/**
 * vpu_hw_get_algo_info - prepare a memory for vpu program to dump algo info
 * @core:	core index of device.
 * @algo:       the pointer to memory block for algo dump.
 *
 * Query properties value and port info from vpu algo(kernel).
 * Should create enough of memory
 * for properties dump, and assign the pointer to vpu_props_t's ptr.
 */
int vpu_hw_get_algo_info(int core, struct vpu_algo *algo);

/**
 * vpu_hw_lock - acquire vpu's lock, stopping to consume requests
 * @user        the user asking to acquire vpu's lock
 */
void vpu_hw_lock(struct vpu_user *user);

/**
 * vpu_hw_unlock - release vpu's lock, re-starting to consume requests
 * @user        the user asking to release vpu's lock
 */
void vpu_hw_unlock(struct vpu_user *user);

/**
 * vpu_quick_suspend - suspend operation.
 */
int vpu_quick_suspend(int core);


/**
 * vpu_alloc_shared_memory - allocate a memory, which shares with VPU
 * @shmem:      return the pointer of struct memory
 * @param:      the pointer to the parameters of memory allocation
 */
int vpu_alloc_shared_memory(struct vpu_shared_memory **shmem,
					struct vpu_shared_memory_param *param);

/**
 * vpu_free_shared_memory - free a memory
 * @shmem:      the pointer of struct memory
 */
void vpu_free_shared_memory(struct vpu_shared_memory *shmem);

/**
 * vpu_ext_be_busy - change VPU's status to busy for 5 sec.
 */
int vpu_ext_be_busy(void);

/**
 * vpu_debug_func_core_state - change VPU's status(only for debug function use).
 * @core:		core index of vpu.
 * @state:		 expetected state.
 */
int vpu_debug_func_core_state(int core, enum VpuCoreState state);

/**
 * vpu_dump_vpu_memory - dump the vpu memory when vpu d2d time out, and
 *                       show the content of all fields.
 * @s:		the pointer to seq_file.
 */
int vpu_dump_vpu_memory(struct seq_file *s);

/**
 * vpu_dump_register - dump the register table, and show the content
 *                     of all fields.
 * @s:		the pointer to seq_file.
 */
int vpu_dump_register(struct seq_file *s);

/**
 * vpu_dump_buffer_mva - dump the buffer mva information.
 * @s:          the requeest.
 */
int vpu_dump_buffer_mva(struct vpu_request *request);

/**
 * vpu_dump_image_file - dump the binary information stored in flash storage.
 * @s:          the pointer to seq_file.
 */
int vpu_dump_image_file(struct seq_file *s);

/**
 * vpu_dump_mesg - dump the log buffer, which is wroted by VPU
 * @s:          the pointer to seq_file.
 */
int vpu_dump_mesg(struct seq_file *s);

/**
 * vpu_dump_opp_table - dump the OPP table
 * @s:          the pointer to seq_file.
 */
int vpu_dump_opp_table(struct seq_file *s);

/**
 * vpu_dump_power - dump the power parameters
 * @s:          the pointer to seq_file.
 */
int vpu_dump_power(struct seq_file *s);

/**
 * vpu_dump_vpu - dump the vpu status
 * @s:          the pointer to seq_file.
 */
int vpu_dump_vpu(struct seq_file *s);

/**
 * vpu_dump_device_dbg - dump the remaining user information to debug fd leak
 * @s:          the pointer to seq_file.
 */
int vpu_dump_device_dbg(struct seq_file *s);

/**
 * vpu_set_power_parameter - set the specific power parameter
 * @param:      the sepcific parameter to update
 * @argc:       the number of arguments
 * @args:       the pointer of arryf of arguments
 */
int vpu_set_power_parameter(uint8_t param, int argc, int *args);

/**
 * vpu_set_algo_parameter - set the specific algo parameter
 * @param:      the sepcific parameter to update
 * @argc:       the number of arguments
 * @args:       the pointer of arryf of arguments
 */
int vpu_set_algo_parameter(uint8_t param, int argc, int *args);

/* ========================== define in vpu_drv.c  ========================= */

/**
 * vpu_create_user - create vpu user, and add to user list
 * @ruser:      return the created user.
 */
int vpu_create_user(struct vpu_user **ruser);

/**
 * vpu_set_power - set the power mode by a user
 * @user:       the pointer to user.
 * @power:      the user's power mode.
 */
int vpu_set_power(struct vpu_user *user, struct vpu_power *power);

/**
 * vpu_sdsp_get_power - get the power by sdsp
 * @user:       the pointer to user.
 * @power:      the user's power mode.
 */
int vpu_sdsp_get_power(struct vpu_user *user);

/**
 * vpu_sdsp_put_power - get the power by sdsp
 * @user:       the pointer to user.
 * @power:      the user's power mode.
 */
int vpu_sdsp_put_power(struct vpu_user *user);

/**
 * vpu_delete_user - delete vpu user, and remove it from user list
 * @user:       the pointer to user.
 */
int vpu_delete_user(struct vpu_user *user);

/**
 * vpu_push_request_to_queue - add a request to user's queue
 * @user:       the pointer to user.
 * @req:        the request to be added to user's queue.
 */
int vpu_push_request_to_queue(struct vpu_user *user, struct vpu_request *req);
int vpu_put_request_to_pool(struct vpu_user *user, struct vpu_request *req);



/**
 * vpu_pop_request_from_queue - remove a request from user's queue
 * @user:       the pointer to user.
 * @rreq:       return the request to be removed.
 */
int vpu_pop_request_from_queue(struct vpu_user *user,
					struct vpu_request **rreq);


/**
 * vpu_get_request_from_queue - get a request from user's queue
 * @user:          the pointer to user.
 * @request_id: request id to identify which enqued request user want to get
 * @rreq:          return the request to be removed.
 */
int vpu_get_request_from_queue(struct vpu_user *user, uint64_t request_id,
					struct vpu_request **rreq);


/**
 * vpu_flush_requests_from_queue - flush all requests of user's queue
 * @user:       the pointer to user.
 *
 * It's a blocking call, and waits for the processing request done.
 * And push all remaining enque to the deque.
 */
int vpu_flush_requests_from_queue(struct vpu_user *user);

/**
 * vpu_dump_user - dump the count of user's input/output request
 * @s:          the pointer to seq_file.
 */
int vpu_dump_user(struct seq_file *s);

/**
 * vpu_dump_user_algo - dump user's created algo
 * @s:          the pointer to seq_file.
 */
int vpu_dump_user_algo(struct seq_file *s);

/* ========================== define in vpu_algo.c  ======================== */

/**
 * vpu_init_algo - init algo module
 * @device:     the pointer of vpu_device.
 */
int vpu_init_algo(struct vpu_device *device);

/**
 * vpu_dump_algo - dump the algo info, which have loaded into pool
 * @s:          the pointer to seq_file.
 */
int vpu_dump_algo(struct seq_file *s);

/**
 * vpu_add_algo_to_pool - add an allocated algo to pool
 * @algo:       the pointer to algo.
 */
int vpu_add_algo_to_pool(int core, struct vpu_algo *algo);

int vpu_find_algo_by_id(int core, vpu_id_t id,
	struct vpu_algo **ralgo, struct vpu_user *user);

int vpu_find_algo_by_name(int core, char *name, struct vpu_algo **ralgo,
	bool needload, struct vpu_user *user);

int vpu_get_algo_id_by_name(int core, char *name, struct vpu_user *user);

int vpu_alloc_algo(struct vpu_algo **ralgo);

int vpu_free_algo(struct vpu_algo *algo);

int vpu_alloc_request(struct vpu_request **rreq);

int vpu_free_request(struct vpu_request *req);

int vpu_add_algo_to_user(struct vpu_user *user,
	struct vpu_create_algo *create_algo);

int vpu_free_algo_from_user(struct vpu_user *user,
	struct vpu_create_algo *create_algo);

/* ========================== define in vpu_dbg.c  ========================= */

/**
 * vpu_init_debug - init debug module
 * @device:     the pointer of vpu_device.
 */
int vpu_init_debug(struct vpu_device *vpu_dev);


/* ========================== define in vpu_reg.c  ========================= */

/**
 * vpu_init_reg - init register module
 * @core:	  core index of vpu device.
 * @device:     the pointer of vpu_device.
 */
int vpu_init_reg(int core, struct vpu_device *vpu_dev);

/* ========================== define in vpu_profile.c  ===================== */

/**
 * vpu_init_profile - init profiling
 * @device:     the pointer of vpu_device.
 */
#define MET_POLLING_MODE
int vpu_init_profile(int core, struct vpu_device *vpu_dev);
int vpu_uninit_profile(void);
int vpu_profile_state_set(int core, int val);
int vpu_profile_state_get(void);
void vpu_met_event_enter(int core, int algo_id, int dsp_freq);
void vpu_met_event_leave(int core, int algo_id);
void vpu_met_packet(long long wclk, char action, int core, int pid,
	int sessid, char *str_desc, int val);
void vpu_met_event_dvfs(int vcore_opp,
	int dsp_freq, int ipu_if_freq, int dsp1_freq, int dsp2_freq);
uint8_t vpu_boost_value_to_opp(uint8_t boost_value);
bool vpu_update_lock_power_parameter(struct vpu_lock_power *vpu_lock_power);
bool vpu_update_unlock_power_parameter(struct vpu_lock_power *vpu_lock_power);
int vpu_lock_set_power(struct vpu_lock_power *vpu_lock_power);
int vpu_unlock_set_power(struct vpu_lock_power *vpu_lock_power);
/**
 * vpu_is_idle - check per vpu core is idle and can be used by user immediately.
 */
bool vpu_is_idle(int core);

/* LOG & AEE */
#define VPU_TAG "[vpu]"
/*#define VPU_DEBUG*/
#ifdef VPU_DEBUG
#define LOG_DBG(format, args...)    pr_debug(VPU_TAG " " format, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_DVFS(format, args...) \
	do { if (g_vpu_log_level > Log_STATE_MACHINE) \
		pr_info(VPU_TAG " " format, ##args); \
	} while (0)
#define LOG_INF(format, args...)    pr_info(VPU_TAG " " format, ##args)
#define LOG_WRN(format, args...)    pr_info(VPU_TAG "[warn] " format, ##args)
#define LOG_ERR(format, args...)    pr_info(VPU_TAG "[error] " format, ##args)

#define PRINT_LINE() pr_info(VPU_TAG " %s (%s:%d)\n", \
						__func__,  __FILE__, __LINE__)

#define vpu_print_seq(seq_file, format, args...) \
	{ \
		if (seq_file) \
			seq_printf(seq_file, format, ##args); \
		else \
			LOG_ERR(format, ##args); \
	}

#ifdef CONFIG_MTK_AEE_FEATURE
#define vpu_aee(key, format, args...) \
	do { \
		LOG_ERR(format, ##args); \
		aee_kernel_exception("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)

#define vpu_aee_warn(key, format, args...) \
	do { \
		LOG_ERR(format, ##args); \
		aee_kernel_warning("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define vpu_aee(key, format, args...)
#define vpu_aee_warn(key, format, args...)
#endif
/* Performance Measure */
#ifdef VPU_TRACE_ENABLED
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#include <linux/preempt.h>
static unsigned long __read_mostly vpu_tracing_writer;
#define vpu_trace_begin(format, args...) \
{ \
	if (vpu_tracing_writer == 0) \
		vpu_tracing_writer = \
			kallsyms_lookup_name("tracing_mark_write"); \
	preempt_disable(); \
	event_trace_printk(vpu_tracing_writer, \
			"B|%d|" format "\n", current->tgid, ##args); \
	preempt_enable(); \
}

#define vpu_trace_end() \
{ \
	preempt_disable(); \
	event_trace_printk(vpu_tracing_writer, "E\n"); \
	preempt_enable(); \
}
#define vpu_trace_dump(format, args...) \
{ \
	preempt_disable(); \
	event_trace_printk(vpu_tracing_writer, \
	"MET_DUMP|" format "\n", ##args); \
	preempt_enable(); \
}
#else
#define vpu_trace_begin(...)
#define vpu_trace_end()
#define vpu_trace_dump(...)
#endif

#endif

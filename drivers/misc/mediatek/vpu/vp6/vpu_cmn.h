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

#include <linux/cdev.h>
#include <linux/firmware.h>
#include <linux/wait.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <uapi/mediatek/vpu_ioctl.h>
#include "vpu_dbg.h"
#include "vpu_hw.h"
#include "vpubuf-core.h"

#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif

#if defined(CONFIG_MTK_QOS_SUPPORT)
#define ENABLE_PMQOS
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif

#define VPU_MET_READY
#define VPU_PORT_OF_IOMMU M4U_PORT_VPU

#define MET_VPU_LOG

enum vpu_load_image_state {
	VPU_LOAD_IMAGE_NONE = 0,
	VPU_LOAD_IMAGE_UNLOAD,
	VPU_LOAD_IMAGE_LOADING,
	VPU_LOAD_IMAGE_LOADED,
	VPU_LOAD_IMAGE_FAILED
};

/* Common Structure */
struct vpu_core;

/**
 * struct vpu_pm - Power management data structure
 */
struct vpu_pm {
	struct clk *apu_sel;
	struct clk *apu_if_sel;

	struct clk *clk26m_ck;
	struct clk *univpll_d2;
	struct clk *apupll_ck;
	struct clk *mmpll_ck;
	struct clk *syspll_d3;
	struct clk *univpll1_d2;
	struct clk *syspll1_d2;
	struct clk *syspll1_d4;

	struct clk *ifr_apu_axi;
	struct clk *smi_cam;

	struct clk *apu_ipu_ck;
	struct clk *apu_axi;
	struct clk *apu_jtag;
	struct clk *apu_if_ck;
	struct clk *apu_edma;
	struct clk *apu_ahb;

	struct device *dev;
	struct vpu_device *mtkdev;
};

struct vpu_image_ctx {
	struct vpu_device *vpu_device;
	int vpu_image_index;
};

struct vpu_device {
	struct device *dev;
	dev_t vpu_devt;
	struct cdev vpu_chardev;
	struct class *vpu_class;

	unsigned int core_num;

	struct vpu_core *vpu_core[MTK_VPU_CORE];
	struct proc_dir_entry *proc_dir;

	struct dentry *debug_root;
	unsigned long vpu_syscfg_base;
	unsigned long vpu_adlctrl_base;
	unsigned long vpu_vcorecfg_base;

	unsigned long smi_cmn_base;
	unsigned long bin_base;
	unsigned long bin_pa;
	unsigned long bin_dma;
	unsigned int bin_size;

	struct mutex user_mutex;
	/* list of vlist_type(struct vpu_user) */
	struct list_head user_list;
	/* pool for each vpu core and common part */
	/* need error handle, pop all requests in pool */
	struct mutex commonpool_mutex;
	struct list_head cmnpool_list;
	int commonpool_list_size;
	/* notify enque thread */
	wait_queue_head_t req_wait;

	wait_queue_head_t cmd_wait;

	/* shared data for all cores */
	struct vpu_shared_memory *core_shared_data;

	uint32_t vpu_dump_exception;

	/* workqueue */
	struct workqueue_struct *wq;

	struct delayed_work opp_keep_work;
	struct delayed_work sdsp_work;

	/* power */
	bool is_power_debug_lock;
	struct mutex opp_mutex;
	bool opp_keep_flag;
	uint8_t sdsp_power_counter;
	wait_queue_head_t waitq_change_vcore;
	wait_queue_head_t waitq_do_core_executing;
	uint8_t max_vcore_opp;	/* thermal */
	uint8_t max_dsp_freq;	/* thermal */
	struct mutex power_lock_mutex;

	/* dvfs */
	struct vpu_dvfs_opps opps;

	/* jtag */
	bool is_jtag_enabled;

	/* direct link */
	bool is_locked;
	struct mutex lock_mutex;
	wait_queue_head_t lock_wait;

	int vpu_init_done;

	struct list_head device_debug_list;
	struct mutex debug_list_mutex;
	unsigned int efuse_data;

	int vpu_num_users;

	uint32_t prop_info_data_length;
	int debug_algo_id;

	int vpu_log_level;
	int vpu_internal_log_level;
	unsigned int func_mask;

	int vpu_profile_state;
	struct hrtimer hr_timer;
	struct mutex profile_mutex;
	int profiling_counter;
	int stop_result;

	struct vpu_manage *vbuf_mng;
	struct vpu_map_ops *vpu_std_mapops;
	unsigned int method;

	struct m4u_client_t *m4u_client;
	struct ion_client *ion_client;
	struct ion_client *ion_drv_client;

	/* security */
	bool in_sec_world;

	enum vpu_load_image_state vpu_load_image_state;
	bool vpu_req_firmware_cbk[VPU_NUMS_IMAGE_HEADER];
	const struct firmware *fw[VPU_NUMS_IMAGE_HEADER];
	struct mutex vpu_load_image_lock;
	struct vpu_image_ctx *vpu_image_ctx;

	struct vpu_image_header *image_header;

	/* algo binary data */
	struct vpu_shared_memory *algo_binary_data;
};

struct vpu_user {
	struct device *dev;
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

	/* to check buffer list must have mutex protection */
	struct mutex dbgbuf_mutex;
	struct list_head dbgbuf_list;
};

struct vpu_shared_memory_param {
	uint32_t size;
	bool require_pa;
	uint32_t fixed_addr;
	uint64_t phy_addr;
	uint64_t kva_addr;
};

struct vpu_shared_memory {
	struct vpu_kernel_buf *vkbuf;
	void *handle;
	uint64_t va;
	uint32_t pa; /* iova */
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
};

enum vpu_debug_algo_param {
	VPU_DEBUG_ALGO_PARAM_DUMP_ALGO,
};

enum vpu_debug_sec_param {
	VPU_DEBUG_SEC_ATTACH,
	VPU_DEBUG_SEC_LOAD,
	VPU_DEBUG_SEC_EXCUTE,
	VPU_DEBUG_SEC_UNLOAD,
	VPU_DEBUG_SEC_DETACH,
	VPU_DEBUG_SEC_TEST,
};

enum vpu_debug_util_param {
	VPU_DEBUG_UTIL_PERIOD,
	VPU_DEBUG_UTIL_ENABLE,
};

/* enum & structure */
enum VpuCoreState {
	VCT_SHUTDOWN	= 1,
	VCT_BOOTUP	= 2,
	VCT_EXECUTING	= 3,
	VCT_IDLE	= 4,
	VCT_VCORE_CHG	= 5,
	VCT_NONE	= -1,
};

enum VpuPowerOnType {
	VPT_PRE_ON	= 1,	/* power on previously by setPower */
	VPT_ENQUE_ON	= 2,	/* power on by enque */
	VPT_IMT_OFF	= 3,	/* power on by enque, but want to immediately
				 * off(when exception)
				 */
};

struct my_ftworkQ_struct_t {
	struct list_head list;
	spinlock_t my_lock;
	int pid;
	struct work_struct my_work;
};

struct vpu_core {
	struct task_struct *srvc_task;

	uint64_t iram_data_mva;
	uint64_t algo_data_mva;
	vpu_id_t current_algo;
	vpu_id_t default_algo_num;

	struct mutex cmd_mutex;
	bool is_cmd_done;
	struct mutex state_mutex;
	enum VpuCoreState state;

	/* reset vector */
	struct vpu_shared_memory *reset_vector;

	/* main program */
	struct vpu_shared_memory *main_program;

	/* iram data */
	struct vpu_shared_memory *iram_data;

	/* working buffer */
	struct vpu_shared_memory *work_buf;

	/* execution kernel library */
	struct vpu_shared_memory *exec_kernel_lib;

	struct device *dev;
	u32 core;
	struct vpu_device *vpu_device;

	uint64_t vpu_base;
	uint64_t bin_base;

	s32 irq;
	uint64_t irq_flags;

	char name[8];
	bool vpu_hw_support;
	struct mutex servicepool_mutex;
	struct list_head pool_list;
	int servicepool_list_size;
	bool service_core_available;
	/*priority number list*/
	uint32_t priority_list[VPU_REQ_MAX_NUM_PRIORITY];

	struct list_head vpu_algo_pool;

	bool exception_isr_check;
	struct mutex sdsp_control_mutex;

	char vpu_wake_name[32];
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source vpu_wake_lock;
#else
	struct wake_lock vpu_wake_lock;
#endif

	/* workqueue */
	struct delayed_work power_counter_work;

	/* power */
	struct mutex power_mutex;
	struct mutex power_counter_mutex;
	int power_counter;

	bool is_power_on;
	bool force_change_vcore_opp;
	bool force_change_dsp_freq;
	bool change_freq_first;

	struct vpu_lock_power lock_power[VPU_OPP_PRIORIYY_NUM];

	uint8_t max_opp;
	uint8_t min_opp;
	uint8_t max_boost;
	uint8_t min_boost;

#ifdef ENABLE_PMQOS
	struct pm_qos_request vpu_qos_vcore_request;
#endif

#ifdef MET_VPU_LOG
	struct my_ftworkQ_struct_t ftrace_dump_work;
#endif

	bool vpu_on;
	int vpu_counter[4];

	struct vpu_pm pm;

	int vpu_hw_init_done;

	/* ivp utilization */
	struct vpu_util *vpu_util;
	enum VpuCoreState util_state;
	ktime_t working_start;
	unsigned long acc_busy;
	unsigned long prev_busy;
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



/* ========================== define in vpu_cmn.c =========================== */

/**
 * vpu_init_device - init the procedure related to mutil-core,
 * @device:     the pointer of vpu_device.
 */
int vpu_init_device(struct vpu_device *vpu_device);
int vpu_uninit_device(struct vpu_device *vpu_device);

/**
 * vpu_init_hw - init the procedure related to hw,
 *               include irq register and enque thread
 * @vpu_core:	the pointer of vpu_core.
 */
int vpu_init_hw(struct vpu_core *vpu_core);

/**
 * vpu_uninit_hw - close resources related to hw module
 * @vpu_core:	the pointer of vpu_core.
 */
int vpu_uninit_hw(struct vpu_core *vpu_core);

/**
 * vpu_boot_up - boot up the vpu power and framework
 * @vpu_core:	the pointer of vpu_core.
 */
int vpu_boot_up(struct vpu_core *vpu_core, bool secure);

/**
 * vpu_shut_down - shutdown the vpu framework and power
 * @vpu_core:	the pointer of vpu_core.
 */
int vpu_shut_down(struct vpu_core *vpu_core);

/**
 * vpu_hw_load_algo - call vpu program to load algo, by specifying the
 *                    start address
 * @vpu_core:	the pointer of vpu_core.
 * @algo:       the pointer to struct algo, which has right binary-data info.
 */
int vpu_hw_load_algo(struct vpu_core *vpu_core, struct vpu_algo *algo);

/**
 * vpu_get_name_of_algo - get the algo's name by its id
 * @vpu_core:	the pointer of vpu_core.
 * @id          the serial id
 * @name:       return the algo's name
 */
int vpu_get_name_of_algo(struct vpu_core *vpu_core, int id, char **name);

/**
 * vpu_get_entry_of_algo - get the address and length from binary data
 * @vpu_core:	the pointer of vpu_core.
 * @name:       the algo's name
 * @id          return the serial id
 * @mva:        return the mva of algo binary
 * @length:     return the length of algo binary
 */
int vpu_get_entry_of_algo(struct vpu_core *vpu_core, char *name, int *id,
				unsigned int *mva, int *length);

int vpu_get_default_algo_num(struct vpu_core *vpu_core, vpu_id_t *algo_num);

int vpu_total_algo_num(struct vpu_core *vpu_core);
/**
 * vpu_hw_get_algo_info - prepare a memory for vpu program to dump algo info
 * @core:	core index of device.
 * @algo:       the pointer to memory block for algo dump.
 *
 * Query properties value and port info from vpu algo(kernel).
 * Should create enough of memory
 * for properties dump, and assign the pointer to vpu_props_t's ptr.
 */
int vpu_hw_get_algo_info(struct vpu_core *vpu_core, struct vpu_algo *algo);

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
int vpu_quick_suspend(struct vpu_core *vpu_core);


/**
 * vpu_alloc_shared_memory - allocate a memory, which shares with VPU
 * @shmem:      return the pointer of struct memory
 * @param:      the pointer to the parameters of memory allocation
 */
int vpu_alloc_shared_memory(struct vpu_device *vpu_device,
				struct vpu_shared_memory **shmem,
				struct vpu_shared_memory_param *param);

/**
 * vpu_free_shared_memory - free a memory
 * @shmem:      the pointer of struct memory
 */
void vpu_free_shared_memory(struct vpu_device *vpu_device,
				struct vpu_shared_memory *shmem);

/**
 * vpu_ext_be_busy - change VPU's status to busy for 5 sec.
 */
int vpu_ext_be_busy(struct vpu_core *vpu_core);

/**
 * vpu_debug_func_core_state - change VPU's status(only for debug function use).
 * @core:		core index of vpu.
 * @state:		 expetected state.
 */
int vpu_debug_func_core_state(struct vpu_core *vpu_core,
				enum VpuCoreState state);

/**
 * vpu_dump_vpu_memory - dump the vpu memory when vpu d2d time out, and
 *                       show the content of all fields.
 * @s:		the pointer to seq_file.
 */
int vpu_dump_vpu_memory(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_register - dump the register table, and show the content
 *                     of all fields.
 * @s:		the pointer to seq_file.
 */
int vpu_dump_register(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_buffer_mva - dump the buffer mva information.
 * @s:          the requeest.
 */
int vpu_dump_buffer_mva(struct vpu_request *request);

/**
 * vpu_dump_image_file - dump the binary information stored in flash storage.
 * @s:          the pointer to seq_file.
 */
int vpu_dump_image_file(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_mesg - dump the log buffer, which is wroted by VPU
 * @s:          the pointer to seq_file.
 */
int vpu_dump_mesg(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_opp_table - dump the OPP table
 * @s:          the pointer to seq_file.
 */
int vpu_dump_opp_table(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_power - dump the power parameters
 * @s:          the pointer to seq_file.
 */
int vpu_dump_power(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_vpu - dump the vpu status
 * @s:          the pointer to seq_file.
 */
int vpu_dump_vpu(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_device_dbg - dump the remaining user information to debug fd leak
 * @s:          the pointer to seq_file.
 */
int vpu_dump_device_dbg(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_set_power_parameter - set the specific power parameter
 * @vpu_device: the pointer of vpu_device
 * @param:      the sepcific parameter to update
 * @argc:       the number of arguments
 * @args:       the pointer of arryf of arguments
 */
int vpu_set_power_parameter(struct vpu_device *vpu_device, uint8_t param,
				int argc, int *args);

/**
 * vpu_hw_boot_sequence - do booting sequence
 * @vpu_core:   the pointer of vpu_core
 */
int vpu_hw_boot_sequence(struct vpu_core *vpu_core);

/**
 * vpu_set_debug - set log buffer and size to VPU
 */
int vpu_hw_set_debug(struct vpu_core *vpu_core);

/**
 * vpu_hw_enable_jtag - start dsp debug via jtag
 */
int vpu_hw_enable_jtag(struct vpu_core *vpu_core, bool enabled);

/**
 * vpu_hw_enque_request - do DRAM-to-DRAM processing, and it will block
 *                        until done.
 * @vpu_core:   the pointer of vpu_core
 * @req:        the pointer to request
 */
int vpu_hw_enque_request(struct vpu_core *vpu_core, struct vpu_request *req);

/**
 * vpu_hw_processing_request - do whole processing for enque request, including
 *                             check algo, load algo, run d2d.
 * @vpu_core:   the pointer of vpu_core
 * @req:        the pointer to request
 */
int vpu_hw_processing_request(struct vpu_core *vpu_core,
				struct vpu_request *req);

/**
 * vpu_dump_debug_stack - for vpu timeout debug.
 */
void vpu_dump_debug_stack(struct vpu_core *vpu_core, int size);

/**
 * vpu_dump_algo_segment - for vpu timeout debug, dump source algo segment
 *                         from bin file.
 */
void vpu_dump_algo_segment(struct vpu_core *vpu_core, int algo_id, int size);

void vpu_dump_code_segment(struct vpu_core *vpu_core);
struct vpu_device *vpu_get_vpu_device(void);

struct vpu_core *vpu_get_vpu_core(int core);

void vpu_unmap_mva_of_bin(struct vpu_core *vpu_core);

/* ========================== define in vpu_drv.c  ========================= */

/**
 * vpu_create_user - create vpu user, and add to user list
 * @ruser:      return the created user.
 */
int vpu_create_user(struct vpu_device *vpu_device, struct vpu_user **ruser);

/**
 * vpu_get_power - get the power mode
 * @vpu_core:      specify vpu core
 * @secure:        secure mode
 */
int vpu_get_power(struct vpu_core *vpu_core, bool secure);

/**
 * vpu_put_power - put the power mode
 * @vpu_core:      specify vpu core
 * @type:          specify power control type
 */
void vpu_put_power(struct vpu_core *vpu_core, enum VpuPowerOnType type);

/**
 * vpu_set_power - set the power mode by a user
 * @user:       the pointer to user.
 * @power:      the user's power mode.
 */
int vpu_set_power(struct vpu_user *user, struct vpu_power *power);

/**
 * vpu_sdsp_get_power - get the power by sdsp
 * @vpu_device:       the pointer to vpu_device.
 */
int vpu_sdsp_get_power(struct vpu_device *vpu_device);

/**
 * vpu_sdsp_put_power - get the power by sdsp
 * @vpu_device:       the pointer to vpu_device.
 */
int vpu_sdsp_put_power(struct vpu_device *vpu_device);

/**
 * vpu_is_available - check vpu queue is empty or not.
 * @vpu_device:       the pointer to vpu_device.
 */
bool vpu_is_available(struct vpu_device *vpu_device);

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
int vpu_dump_user(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_user_algo - dump user's created algo
 * @s:          the pointer to seq_file.
 */
int vpu_dump_user_algo(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_dump_util - dump vpu utilization
 * @s:          the pointer to seq_file.
 */
int vpu_dump_util(struct seq_file *s, struct vpu_device *gvpu_device);

/* take care of leaking memory */
void vpu_add_dbg_buf(struct vpu_user *user, uint64_t buf_handle);
void vpu_delete_dbg_buf(struct vpu_user *user, uint64_t buf_handle);
void vpu_check_dbg_buf(struct vpu_user *user);

/* ========================== define in vpu_algo.c  ======================== */

/**
 * vpu_set_algo_parameter - set the specific algo parameter
 * @vpu_device: the pointer of vpu_device
 * @param:      the sepcific parameter to update
 * @argc:       the number of arguments
 * @args:       the pointer of arryf of arguments
 */
int vpu_set_algo_parameter(struct vpu_device *vpu_device, uint8_t param,
				int argc, int *args);

/**
 * vpu_init_algo - init algo module
 * @vpu_device: the pointer of vpu_device.
 */
int vpu_init_algo(struct vpu_device *vpu_device);

/**
 * vpu_dump_algo - dump the algo info, which have loaded into pool
 * @s:          the pointer to seq_file.
 */
int vpu_dump_algo(struct seq_file *s, struct vpu_device *gvpu_device);

/**
 * vpu_add_algo_to_pool - add an allocated algo to pool
 * @algo:       the pointer to algo.
 */
int vpu_add_algo_to_pool(struct vpu_core *vpu_core, struct vpu_algo *algo);

int vpu_find_algo_by_id(struct vpu_core *vpu_core, vpu_id_t id,
	struct vpu_algo **ralgo, struct vpu_user *user);

int vpu_find_algo_by_name(struct vpu_core *vpu_core, char *name,
				struct vpu_algo **ralgo,
				bool needload, struct vpu_user *user);

int vpu_get_algo_id_by_name(struct vpu_core *vpu_core, char *name,
				struct vpu_user *user);

int vpu_alloc_algo(struct vpu_device *vpu_device, struct vpu_algo **ralgo);

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

/**
 * vpu_deinit_debug - deinit debug module
 * @device:     the pointer of vpu_device.
 */
void vpu_deinit_debug(struct vpu_device *vpu_device);

/* ========================== define in vpu_reg.c  ========================= */

/**
 * vpu_init_reg - init register module
 * @core:	  core index of vpu device.
 * @device:     the pointer of vpu_device.
 */
int vpu_init_reg(struct vpu_core *vpu_core);

/* ========================== define in vpu_sec.c  ========================= */

/**
 * vpu_set_sec_test_parameter - set the specific security parameter
 * @vpu_device: the pointer of vpu_device.
 * @param:      the sepcific parameter to update
 * @argc:       the number of arguments
 * @args:       the pointer of arryf of arguments
 */
int vpu_set_sec_test_parameter(struct vpu_device *vpu_device, uint8_t param,
			       int argc, int *args);

/* ===================== define in vpu_utilization.c  ====================== */

/**
 * vpu_set_util_test_parameter - set the specific security parameter
 * @vpu_device: the pointer of vpu_device.
 * @param:      the sepcific parameter to update
 * @argc:       the number of arguments
 * @args:       the pointer of arryf of arguments
 */
int vpu_set_util_test_parameter(struct vpu_device *vpu_device, uint8_t param,
				int argc, int *args);

/* ========================== define in vpu_profile.c  ===================== */

/**
 * vpu_init_profile - init profiling
 * @device:     the pointer of vpu_device.
 */
#define MET_POLLING_MODE
int vpu_init_profile(struct vpu_device *vpu_device);
int vpu_uninit_profile(struct vpu_device *vpu_device);
int vpu_profile_state_set(struct vpu_core *vpu_core, int val);
int vpu_profile_state_get(struct vpu_device *vpu_device);
void vpu_met_event_enter(int core, int algo_id, int dsp_freq);
void vpu_met_event_leave(int core, int algo_id);
void vpu_met_packet(long long wclk, char action, int core, int pid,
	int sessid, char *str_desc, int val);
void vpu_met_event_dvfs(int vcore_opp, int apu_freq, int apu_if_freq);
void vpu_met_event_busyrate(int core, int busyrate);
uint8_t vpu_boost_value_to_opp(struct vpu_device *vpu_device,
				uint8_t boost_value);

bool vpu_update_lock_power_parameter(struct vpu_core *vpu_core,
					struct vpu_lock_power *vpu_lock_power);

bool vpu_update_unlock_power_parameter(struct vpu_core *vpu_core,
					struct vpu_lock_power *vpu_lock_power);

int vpu_lock_set_power(struct vpu_device *vpu_device,
			struct vpu_lock_power *vpu_lock_power);

int vpu_unlock_set_power(struct vpu_device *vpu_device,
				struct vpu_lock_power *vpu_lock_power);
#if defined(VPU_MET_READY)
void MET_Events_DVFS_Trace(struct vpu_device *vpu_device);
void MET_Events_BusyRate_Trace(struct vpu_device *vpu_device, int core);
void MET_Events_Trace(struct vpu_device *vpu_device, bool enter, int core,
			int algo_id);
#endif
/**
 * vpu_is_idle - check per vpu core is idle and can be used by user immediately.
 */
bool vpu_is_idle(struct vpu_core *vpu_core);

/* LOG & AEE */
#define VPU_TAG "[vpu]"
#define VPU_DEBUG
#ifdef VPU_DEBUG
#define LOG_DBG(format, args...)    pr_debug(VPU_TAG " " format, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_DVFS(format, args...) \
	do { if (vpu_device->vpu_log_level > Log_STATE_MACHINE) \
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

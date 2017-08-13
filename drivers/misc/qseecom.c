/*Qualcomm Secure Execution Environment Communicator (QSEECOM) driver
 *
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "QSEECOM: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/msm_ion.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/qseecom.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/freezer.h>
#include <linux/scatterlist.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/socinfo.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <soc/qcom/qseecomi.h>
#include <asm/cacheflush.h>
#include "qseecom_legacy.h"
#include "qseecom_kernel.h"
#include <crypto/ice.h>
#include <linux/delay.h>

#include <linux/compat.h>
#include "compat_qseecom.h"

#define QSEECOM_DEV			"qseecom"
#define QSEOS_VERSION_14		0x14
#define QSEEE_VERSION_00		0x400000
#define QSEE_VERSION_01			0x401000
#define QSEE_VERSION_02			0x402000
#define QSEE_VERSION_03			0x403000
#define QSEE_VERSION_04			0x404000
#define QSEE_VERSION_05			0x405000
#define QSEE_VERSION_20			0x800000
#define QSEE_VERSION_40			0x1000000  /* TZ.BF.4.0 */

#define QSEE_CE_CLK_100MHZ		100000000
#define CE_CLK_DIV			1000000

#define QSEECOM_MAX_SG_ENTRY			512
#define QSEECOM_SG_ENTRY_MSG_BUF_SZ_64BIT	\
			(QSEECOM_MAX_SG_ENTRY * SG_ENTRY_SZ_64BIT)

#define QSEECOM_INVALID_KEY_ID  0xff

/* Save partition image hash for authentication check */
#define	SCM_SAVE_PARTITION_HASH_ID	0x01

/* Check if enterprise security is activate */
#define	SCM_IS_ACTIVATED_ID		0x02

/* Encrypt/Decrypt Data Integrity Partition (DIP) for MDTP */
#define SCM_MDTP_CIPHER_DIP		0x01

/* Maximum Allowed Size (128K) of Data Integrity Partition (DIP) for MDTP */
#define MAX_DIP			0x20000

#define RPMB_SERVICE			0x2000
#define SSD_SERVICE			0x3000

#define QSEECOM_SEND_CMD_CRYPTO_TIMEOUT	2000
#define QSEECOM_LOAD_APP_CRYPTO_TIMEOUT	2000
#define TWO 2
#define QSEECOM_UFS_ICE_CE_NUM 10
#define QSEECOM_SDCC_ICE_CE_NUM 20
#define QSEECOM_ICE_FDE_KEY_INDEX 0

#define PHY_ADDR_4G	(1ULL<<32)

#define QSEECOM_STATE_NOT_READY         0
#define QSEECOM_STATE_SUSPEND           1
#define QSEECOM_STATE_READY             2
#define QSEECOM_ICE_FDE_KEY_SIZE_MASK   2

/*
 * default ce info unit to 0 for
 * services which
 * support only single instance.
 * Most of services are in this category.
 */
#define DEFAULT_CE_INFO_UNIT 0
#define DEFAULT_NUM_CE_INFO_UNIT 1

enum qseecom_clk_definitions {
	CLK_DFAB = 0,
	CLK_SFPB,
};

enum qseecom_ice_key_size_type {
	QSEECOM_ICE_FDE_KEY_SIZE_16_BYTE = (0 << QSEECOM_ICE_FDE_KEY_SIZE_MASK),
	QSEECOM_ICE_FDE_KEY_SIZE_32_BYTE = (1 << QSEECOM_ICE_FDE_KEY_SIZE_MASK),
	QSEECOM_ICE_FDE_KEY_SIZE_UNDEF = (0xF << QSEECOM_ICE_FDE_KEY_SIZE_MASK),
};

enum qseecom_client_handle_type {
	QSEECOM_CLIENT_APP = 1,
	QSEECOM_LISTENER_SERVICE,
	QSEECOM_SECURE_SERVICE,
	QSEECOM_GENERIC,
	QSEECOM_UNAVAILABLE_CLIENT_APP,
};

enum qseecom_ce_hw_instance {
	CLK_QSEE = 0,
	CLK_CE_DRV,
	CLK_INVALID,
};

static struct class *driver_class;
static dev_t qseecom_device_no;

static DEFINE_MUTEX(qsee_bw_mutex);
static DEFINE_MUTEX(app_access_lock);
static DEFINE_MUTEX(clk_access_lock);

struct sglist_info {
	uint32_t indexAndFlags;
	uint32_t sizeOrCount;
};

/*
 * The 31th bit indicates only one or multiple physical address inside
 * the request buffer. If it is set,  the index locates a single physical addr
 * inside the request buffer, and `sizeOrCount` is the size of the memory being
 * shared at that physical address.
 * Otherwise, the index locates an array of {start, len} pairs (a
 * "scatter/gather list"), and `sizeOrCount` gives the number of entries in
 * that array.
 *
 * The 30th bit indicates 64 or 32bit address; when it is set, physical addr
 * and scatter gather entry sizes are 64-bit values.  Otherwise, 32-bit values.
 *
 * The bits [0:29] of `indexAndFlags` hold an offset into the request buffer.
 */
#define SGLISTINFO_SET_INDEX_FLAG(c, s, i)	\
	((uint32_t)(((c & 1) << 31) | ((s & 1) << 30) | (i & 0x3fffffff)))

#define SGLISTINFO_TABLE_SIZE	(sizeof(struct sglist_info) * MAX_ION_FD)

#define FEATURE_ID_WHITELIST	15	/*whitelist feature id*/

#define MAKE_WHITELIST_VERSION(major, minor, patch) \
	(((major & 0x3FF) << 22) | ((minor & 0x3FF) << 12) | (patch & 0xFFF))

struct qseecom_registered_listener_list {
	struct list_head                 list;
	struct qseecom_register_listener_req svc;
	void  *user_virt_sb_base;
	u8 *sb_virt;
	phys_addr_t sb_phys;
	size_t sb_length;
	struct ion_handle *ihandle; /* Retrieve phy addr */
	wait_queue_head_t          rcv_req_wq;
	int                        rcv_req_flag;
	int                        send_resp_flag;
	bool                       listener_in_use;
	/* wq for thread blocked on this listener*/
	wait_queue_head_t          listener_block_app_wq;
	struct sglist_info sglistinfo_ptr[MAX_ION_FD];
	uint32_t sglist_cnt;
};

struct qseecom_registered_app_list {
	struct list_head                 list;
	u32  app_id;
	u32  ref_cnt;
	char app_name[MAX_APP_NAME_SIZE];
	u32  app_arch;
	bool app_blocked;
	u32  blocked_on_listener_id;
};

struct qseecom_registered_kclient_list {
	struct list_head list;
	struct qseecom_handle *handle;
};

struct qseecom_ce_info_use {
	unsigned char handle[MAX_CE_INFO_HANDLE_SIZE];
	unsigned int unit_num;
	unsigned int num_ce_pipe_entries;
	struct qseecom_ce_pipe_entry *ce_pipe_entry;
	bool alloc;
	uint32_t type;
};

struct ce_hw_usage_info {
	uint32_t qsee_ce_hw_instance;
	uint32_t num_fde;
	struct qseecom_ce_info_use *fde;
	uint32_t num_pfe;
	struct qseecom_ce_info_use *pfe;
};

struct qseecom_clk {
	enum qseecom_ce_hw_instance instance;
	struct clk *ce_core_clk;
	struct clk *ce_clk;
	struct clk *ce_core_src_clk;
	struct clk *ce_bus_clk;
	uint32_t clk_access_cnt;
};

struct qseecom_control {
	struct ion_client *ion_clnt;		/* Ion client */
	struct list_head  registered_listener_list_head;
	spinlock_t        registered_listener_list_lock;

	struct list_head  registered_app_list_head;
	spinlock_t        registered_app_list_lock;

	struct list_head   registered_kclient_list_head;
	spinlock_t        registered_kclient_list_lock;

	wait_queue_head_t send_resp_wq;
	int               send_resp_flag;

	uint32_t          qseos_version;
	uint32_t          qsee_version;
	struct device *pdev;
	bool  whitelist_support;
	bool  commonlib_loaded;
	bool  commonlib64_loaded;
	struct ion_handle *cmnlib_ion_handle;
	struct ce_hw_usage_info ce_info;

	int qsee_bw_count;
	int qsee_sfpb_bw_count;

	uint32_t qsee_perf_client;
	struct qseecom_clk qsee;
	struct qseecom_clk ce_drv;

	bool support_bus_scaling;
	bool support_fde;
	bool support_pfe;
	bool fde_key_size;
	uint32_t  cumulative_mode;
	enum qseecom_bandwidth_request_mode  current_mode;
	struct timer_list bw_scale_down_timer;
	struct work_struct bw_inactive_req_ws;
	struct cdev cdev;
	bool timer_running;
	bool no_clock_support;
	unsigned int ce_opp_freq_hz;
	bool appsbl_qseecom_support;
	uint32_t qsee_reentrancy_support;

	uint32_t app_block_ref_cnt;
	wait_queue_head_t app_block_wq;
	atomic_t qseecom_state;
	int is_apps_region_protected;
};

struct qseecom_sec_buf_fd_info {
	bool is_sec_buf_fd;
	size_t size;
	void *vbase;
	dma_addr_t pbase;
};

struct qseecom_param_memref {
	uint32_t buffer;
	uint32_t size;
};

struct qseecom_client_handle {
	u32  app_id;
	u8 *sb_virt;
	phys_addr_t sb_phys;
	unsigned long user_virt_sb_base;
	size_t sb_length;
	struct ion_handle *ihandle;		/* Retrieve phy addr */
	char app_name[MAX_APP_NAME_SIZE];
	u32  app_arch;
	struct qseecom_sec_buf_fd_info sec_buf_fd[MAX_ION_FD];
};

struct qseecom_listener_handle {
	u32               id;
};

static struct qseecom_control qseecom;

struct qseecom_dev_handle {
	enum qseecom_client_handle_type type;
	union {
		struct qseecom_client_handle client;
		struct qseecom_listener_handle listener;
	};
	bool released;
	int               abort;
	wait_queue_head_t abort_wq;
	atomic_t          ioctl_count;
	bool  perf_enabled;
	bool  fast_load_enabled;
	enum qseecom_bandwidth_request_mode mode;
	struct sglist_info sglistinfo_ptr[MAX_ION_FD];
	uint32_t sglist_cnt;
	bool use_legacy_cmd;
};

struct qseecom_key_id_usage_desc {
	uint8_t desc[QSEECOM_KEY_ID_SIZE];
};

struct qseecom_crypto_info {
	unsigned int unit_num;
	unsigned int ce;
	unsigned int pipe_pair;
};

static struct qseecom_key_id_usage_desc key_id_array[] = {
	{
		.desc = "Undefined Usage Index",
	},

	{
		.desc = "Full Disk Encryption",
	},

	{
		.desc = "Per File Encryption",
	},

	{
		.desc = "UFS ICE Full Disk Encryption",
	},

	{
		.desc = "SDCC ICE Full Disk Encryption",
	},
};

/* Function proto types */
static int qsee_vote_for_clock(struct qseecom_dev_handle *, int32_t);
static void qsee_disable_clock_vote(struct qseecom_dev_handle *, int32_t);
static int __qseecom_enable_clk(enum qseecom_ce_hw_instance ce);
static void __qseecom_disable_clk(enum qseecom_ce_hw_instance ce);
static int __qseecom_init_clk(enum qseecom_ce_hw_instance ce);
static int qseecom_load_commonlib_image(struct qseecom_dev_handle *data,
					char *cmnlib_name);
static int qseecom_enable_ice_setup(int usage);
static int qseecom_disable_ice_setup(int usage);
static void __qseecom_reentrancy_check_if_no_app_blocked(uint32_t smc_id);
static int qseecom_get_ce_info(struct qseecom_dev_handle *data,
						void __user *argp);
static int qseecom_free_ce_info(struct qseecom_dev_handle *data,
						void __user *argp);
static int qseecom_query_ce_info(struct qseecom_dev_handle *data,
						void __user *argp);

static int get_qseecom_keymaster_status(char *str)
{
	get_option(&str, &qseecom.is_apps_region_protected);
	return 1;
}
__setup("androidboot.keymaster=", get_qseecom_keymaster_status);

static int qseecom_scm_call2(uint32_t svc_id, uint32_t tz_cmd_id,
			const void *req_buf, void *resp_buf)
{
	int      ret = 0;
	uint32_t smc_id = 0;
	uint32_t qseos_cmd_id = 0;
	struct scm_desc desc = {0};
	struct qseecom_command_scm_resp *scm_resp = NULL;

	if (!req_buf || !resp_buf) {
		pr_err("Invalid buffer pointer\n");
		return -EINVAL;
	}
	qseos_cmd_id = *(uint32_t *)req_buf;
	scm_resp = (struct qseecom_command_scm_resp *)resp_buf;

	switch (svc_id) {
	case 6: {
		if (tz_cmd_id == 3) {
			smc_id = TZ_INFO_GET_FEATURE_VERSION_ID;
			desc.arginfo = TZ_INFO_GET_FEATURE_VERSION_ID_PARAM_ID;
			desc.args[0] = *(uint32_t *)req_buf;
		} else {
			pr_err("Unsupported svc_id %d, tz_cmd_id %d\n",
				svc_id, tz_cmd_id);
			return -EINVAL;
		}
		ret = scm_call2(smc_id, &desc);
		break;
	}
	case SCM_SVC_ES: {
		switch (tz_cmd_id) {
		case SCM_SAVE_PARTITION_HASH_ID: {
			u32 tzbuflen = PAGE_ALIGN(SHA256_DIGEST_LENGTH);
			struct qseecom_save_partition_hash_req *p_hash_req =
				(struct qseecom_save_partition_hash_req *)
				req_buf;
			char *tzbuf = kzalloc(tzbuflen, GFP_KERNEL);
			if (!tzbuf) {
				pr_err("error allocating data\n");
				return -ENOMEM;
			}
			memset(tzbuf, 0, tzbuflen);
			memcpy(tzbuf, p_hash_req->digest,
				SHA256_DIGEST_LENGTH);
			dmac_flush_range(tzbuf, tzbuf + tzbuflen);
			smc_id = TZ_ES_SAVE_PARTITION_HASH_ID;
			desc.arginfo = TZ_ES_SAVE_PARTITION_HASH_ID_PARAM_ID;
			desc.args[0] = p_hash_req->partition_id;
			desc.args[1] = virt_to_phys(tzbuf);
			desc.args[2] = SHA256_DIGEST_LENGTH;
			ret = scm_call2(smc_id, &desc);
			kzfree(tzbuf);
			break;
		}
		default: {
			pr_err("tz_cmd_id %d is not supported by scm_call2\n",
						tz_cmd_id);
			ret = -EINVAL;
			break;
		}
		} /* end of switch (tz_cmd_id) */
		break;
	} /* end of case SCM_SVC_ES */
	case SCM_SVC_TZSCHEDULER: {
		switch (qseos_cmd_id) {
		case QSEOS_APP_START_COMMAND: {
			struct qseecom_load_app_ireq *req;
			struct qseecom_load_app_64bit_ireq *req_64bit;
			smc_id = TZ_OS_APP_START_ID;
			desc.arginfo = TZ_OS_APP_START_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_load_app_ireq *)req_buf;
				desc.args[0] = req->mdt_len;
				desc.args[1] = req->img_len;
				desc.args[2] = req->phy_addr;
			} else {
				req_64bit =
					(struct qseecom_load_app_64bit_ireq *)
					req_buf;
				desc.args[0] = req_64bit->mdt_len;
				desc.args[1] = req_64bit->img_len;
				desc.args[2] = req_64bit->phy_addr;
			}
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_APP_SHUTDOWN_COMMAND: {
			struct qseecom_unload_app_ireq *req;
			req = (struct qseecom_unload_app_ireq *)req_buf;
			smc_id = TZ_OS_APP_SHUTDOWN_ID;
			desc.arginfo = TZ_OS_APP_SHUTDOWN_ID_PARAM_ID;
			desc.args[0] = req->app_id;
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_APP_LOOKUP_COMMAND: {
			struct qseecom_check_app_ireq *req;
			u32 tzbuflen = PAGE_ALIGN(sizeof(req->app_name));
			char *tzbuf = kzalloc(tzbuflen, GFP_KERNEL);
			if (!tzbuf) {
				pr_err("Allocate %d bytes buffer failed\n",
					tzbuflen);
				return -ENOMEM;
			}
			req = (struct qseecom_check_app_ireq *)req_buf;
			pr_debug("Lookup app_name = %s\n", req->app_name);
			strlcpy(tzbuf, req->app_name, sizeof(req->app_name));
			dmac_flush_range(tzbuf, tzbuf + tzbuflen);
			smc_id = TZ_OS_APP_LOOKUP_ID;
			desc.arginfo = TZ_OS_APP_LOOKUP_ID_PARAM_ID;
			desc.args[0] = virt_to_phys(tzbuf);
			desc.args[1] = strlen(req->app_name);
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			kzfree(tzbuf);
			break;
		}
		case QSEOS_APP_REGION_NOTIFICATION: {
			struct qsee_apps_region_info_ireq *req;
			struct qsee_apps_region_info_64bit_ireq *req_64bit;
			smc_id = TZ_OS_APP_REGION_NOTIFICATION_ID;
			desc.arginfo =
				TZ_OS_APP_REGION_NOTIFICATION_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qsee_apps_region_info_ireq *)
					req_buf;
				desc.args[0] = req->addr;
				desc.args[1] = req->size;
			} else {
				req_64bit =
				(struct qsee_apps_region_info_64bit_ireq *)
					req_buf;
				desc.args[0] = req_64bit->addr;
				desc.args[1] = req_64bit->size;
			}
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_LOAD_SERV_IMAGE_COMMAND: {
			struct qseecom_load_lib_image_ireq *req;
			struct qseecom_load_lib_image_64bit_ireq *req_64bit;
			smc_id = TZ_OS_LOAD_SERVICES_IMAGE_ID;
			desc.arginfo = TZ_OS_LOAD_SERVICES_IMAGE_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_load_lib_image_ireq *)
					req_buf;
				desc.args[0] = req->mdt_len;
				desc.args[1] = req->img_len;
				desc.args[2] = req->phy_addr;
			} else {
				req_64bit =
				(struct qseecom_load_lib_image_64bit_ireq *)
					req_buf;
				desc.args[0] = req_64bit->mdt_len;
				desc.args[1] = req_64bit->img_len;
				desc.args[2] = req_64bit->phy_addr;
			}
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_UNLOAD_SERV_IMAGE_COMMAND: {
			smc_id = TZ_OS_UNLOAD_SERVICES_IMAGE_ID;
			desc.arginfo = TZ_OS_UNLOAD_SERVICES_IMAGE_ID_PARAM_ID;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_REGISTER_LISTENER: {
			struct qseecom_register_listener_ireq *req;
			struct qseecom_register_listener_64bit_ireq *req_64bit;
			desc.arginfo =
				TZ_OS_REGISTER_LISTENER_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_register_listener_ireq *)
					req_buf;
				desc.args[0] = req->listener_id;
				desc.args[1] = req->sb_ptr;
				desc.args[2] = req->sb_len;
			} else {
				req_64bit =
				(struct qseecom_register_listener_64bit_ireq *)
					req_buf;
				desc.args[0] = req_64bit->listener_id;
				desc.args[1] = req_64bit->sb_ptr;
				desc.args[2] = req_64bit->sb_len;
			}
			smc_id = TZ_OS_REGISTER_LISTENER_SMCINVOKE_ID;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			if (ret) {
				smc_id = TZ_OS_REGISTER_LISTENER_ID;
				__qseecom_reentrancy_check_if_no_app_blocked(
					smc_id);
				ret = scm_call2(smc_id, &desc);
			}
			break;
		}
		case QSEOS_DEREGISTER_LISTENER: {
			struct qseecom_unregister_listener_ireq *req;
			req = (struct qseecom_unregister_listener_ireq *)
				req_buf;
			smc_id = TZ_OS_DEREGISTER_LISTENER_ID;
			desc.arginfo = TZ_OS_DEREGISTER_LISTENER_ID_PARAM_ID;
			desc.args[0] = req->listener_id;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_LISTENER_DATA_RSP_COMMAND: {
			struct qseecom_client_listener_data_irsp *req;
			req = (struct qseecom_client_listener_data_irsp *)
				req_buf;
			smc_id = TZ_OS_LISTENER_RESPONSE_HANDLER_ID;
			desc.arginfo =
				TZ_OS_LISTENER_RESPONSE_HANDLER_ID_PARAM_ID;
			desc.args[0] = req->listener_id;
			desc.args[1] = req->status;
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_LISTENER_DATA_RSP_COMMAND_WHITELIST: {
			struct qseecom_client_listener_data_irsp *req;
			struct qseecom_client_listener_data_64bit_irsp *req_64;

			smc_id =
			TZ_OS_LISTENER_RESPONSE_HANDLER_WITH_WHITELIST_ID;
			desc.arginfo =
			TZ_OS_LISTENER_RESPONSE_HANDLER_WITH_WHITELIST_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req =
				(struct qseecom_client_listener_data_irsp *)
				req_buf;
				desc.args[0] = req->listener_id;
				desc.args[1] = req->status;
				desc.args[2] = req->sglistinfo_ptr;
				desc.args[3] = req->sglistinfo_len;
			} else {
				req_64 =
			(struct qseecom_client_listener_data_64bit_irsp *)
				req_buf;
				desc.args[0] = req_64->listener_id;
				desc.args[1] = req_64->status;
				desc.args[2] = req_64->sglistinfo_ptr;
				desc.args[3] = req_64->sglistinfo_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_LOAD_EXTERNAL_ELF_COMMAND: {
			struct qseecom_load_app_ireq *req;
			struct qseecom_load_app_64bit_ireq *req_64bit;
			smc_id = TZ_OS_LOAD_EXTERNAL_IMAGE_ID;
			desc.arginfo = TZ_OS_LOAD_SERVICES_IMAGE_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_load_app_ireq *)req_buf;
				desc.args[0] = req->mdt_len;
				desc.args[1] = req->img_len;
				desc.args[2] = req->phy_addr;
			} else {
				req_64bit =
				(struct qseecom_load_app_64bit_ireq *)req_buf;
				desc.args[0] = req_64bit->mdt_len;
				desc.args[1] = req_64bit->img_len;
				desc.args[2] = req_64bit->phy_addr;
			}
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_UNLOAD_EXTERNAL_ELF_COMMAND: {
			smc_id = TZ_OS_UNLOAD_EXTERNAL_IMAGE_ID;
			desc.arginfo = TZ_OS_UNLOAD_SERVICES_IMAGE_ID_PARAM_ID;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
			}

		case QSEOS_CLIENT_SEND_DATA_COMMAND: {
			struct qseecom_client_send_data_ireq *req;
			struct qseecom_client_send_data_64bit_ireq *req_64bit;
			smc_id = TZ_APP_QSAPP_SEND_DATA_ID;
			desc.arginfo = TZ_APP_QSAPP_SEND_DATA_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_client_send_data_ireq *)
					req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->rsp_ptr;
				desc.args[4] = req->rsp_len;
			} else {
				req_64bit =
				(struct qseecom_client_send_data_64bit_ireq *)
					req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->rsp_ptr;
				desc.args[4] = req_64bit->rsp_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_CLIENT_SEND_DATA_COMMAND_WHITELIST: {
			struct qseecom_client_send_data_ireq *req;
			struct qseecom_client_send_data_64bit_ireq *req_64bit;

			smc_id = TZ_APP_QSAPP_SEND_DATA_WITH_WHITELIST_ID;
			desc.arginfo =
			TZ_APP_QSAPP_SEND_DATA_WITH_WHITELIST_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_client_send_data_ireq *)
					req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->rsp_ptr;
				desc.args[4] = req->rsp_len;
				desc.args[5] = req->sglistinfo_ptr;
				desc.args[6] = req->sglistinfo_len;
			} else {
				req_64bit =
				(struct qseecom_client_send_data_64bit_ireq *)
					req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->rsp_ptr;
				desc.args[4] = req_64bit->rsp_len;
				desc.args[5] = req_64bit->sglistinfo_ptr;
				desc.args[6] = req_64bit->sglistinfo_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_RPMB_PROVISION_KEY_COMMAND: {
			struct qseecom_client_send_service_ireq *req;
			req = (struct qseecom_client_send_service_ireq *)
				req_buf;
			smc_id = TZ_OS_RPMB_PROVISION_KEY_ID;
			desc.arginfo = TZ_OS_RPMB_PROVISION_KEY_ID_PARAM_ID;
			desc.args[0] = req->key_type;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_RPMB_ERASE_COMMAND: {
			smc_id = TZ_OS_RPMB_ERASE_ID;
			desc.arginfo = TZ_OS_RPMB_ERASE_ID_PARAM_ID;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_RPMB_CHECK_PROV_STATUS_COMMAND: {
			smc_id = TZ_OS_RPMB_CHECK_PROV_STATUS_ID;
			desc.arginfo = TZ_OS_RPMB_CHECK_PROV_STATUS_ID_PARAM_ID;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_GENERATE_KEY: {
			u32 tzbuflen = PAGE_ALIGN(sizeof
				(struct qseecom_key_generate_ireq) -
				sizeof(uint32_t));
			char *tzbuf = kzalloc(tzbuflen, GFP_KERNEL);
			if (!tzbuf)
				return -ENOMEM;
			memset(tzbuf, 0, tzbuflen);
			memcpy(tzbuf, req_buf + sizeof(uint32_t),
				(sizeof(struct qseecom_key_generate_ireq) -
				sizeof(uint32_t)));
			dmac_flush_range(tzbuf, tzbuf + tzbuflen);
			smc_id = TZ_OS_KS_GEN_KEY_ID;
			desc.arginfo = TZ_OS_KS_GEN_KEY_ID_PARAM_ID;
			desc.args[0] = virt_to_phys(tzbuf);
			desc.args[1] = tzbuflen;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			kzfree(tzbuf);
			break;
		}
		case QSEOS_DELETE_KEY: {
			u32 tzbuflen = PAGE_ALIGN(sizeof
				(struct qseecom_key_delete_ireq) -
				sizeof(uint32_t));
			char *tzbuf = kzalloc(tzbuflen, GFP_KERNEL);
			if (!tzbuf) {
				pr_err("Allocate %d bytes buffer failed\n",
					tzbuflen);
				return -ENOMEM;
			}
			memset(tzbuf, 0, tzbuflen);
			memcpy(tzbuf, req_buf + sizeof(uint32_t),
				(sizeof(struct qseecom_key_delete_ireq) -
				sizeof(uint32_t)));
			dmac_flush_range(tzbuf, tzbuf + tzbuflen);
			smc_id = TZ_OS_KS_DEL_KEY_ID;
			desc.arginfo = TZ_OS_KS_DEL_KEY_ID_PARAM_ID;
			desc.args[0] = virt_to_phys(tzbuf);
			desc.args[1] = tzbuflen;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			kzfree(tzbuf);
			break;
		}
		case QSEOS_SET_KEY: {
			u32 tzbuflen = PAGE_ALIGN(sizeof
				(struct qseecom_key_select_ireq) -
				sizeof(uint32_t));
			char *tzbuf = kzalloc(tzbuflen, GFP_KERNEL);
			if (!tzbuf) {
				pr_err("Allocate %d bytes buffer failed\n",
					tzbuflen);
				return -ENOMEM;
			}
			memset(tzbuf, 0, tzbuflen);
			memcpy(tzbuf, req_buf + sizeof(uint32_t),
				(sizeof(struct qseecom_key_select_ireq) -
				sizeof(uint32_t)));
			dmac_flush_range(tzbuf, tzbuf + tzbuflen);
			smc_id = TZ_OS_KS_SET_PIPE_KEY_ID;
			desc.arginfo = TZ_OS_KS_SET_PIPE_KEY_ID_PARAM_ID;
			desc.args[0] = virt_to_phys(tzbuf);
			desc.args[1] = tzbuflen;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			kzfree(tzbuf);
			break;
		}
		case QSEOS_UPDATE_KEY_USERINFO: {
			u32 tzbuflen = PAGE_ALIGN(sizeof
				(struct qseecom_key_userinfo_update_ireq) -
				sizeof(uint32_t));
			char *tzbuf = kzalloc(tzbuflen, GFP_KERNEL);
			if (!tzbuf) {
				pr_err("Allocate %d bytes buffer failed\n",
					tzbuflen);
				return -ENOMEM;
			}
			memset(tzbuf, 0, tzbuflen);
			memcpy(tzbuf, req_buf + sizeof(uint32_t), (sizeof
				(struct qseecom_key_userinfo_update_ireq) -
				sizeof(uint32_t)));
			dmac_flush_range(tzbuf, tzbuf + tzbuflen);
			smc_id = TZ_OS_KS_UPDATE_KEY_ID;
			desc.arginfo = TZ_OS_KS_UPDATE_KEY_ID_PARAM_ID;
			desc.args[0] = virt_to_phys(tzbuf);
			desc.args[1] = tzbuflen;
			__qseecom_reentrancy_check_if_no_app_blocked(smc_id);
			ret = scm_call2(smc_id, &desc);
			kzfree(tzbuf);
			break;
		}
		case QSEOS_TEE_OPEN_SESSION: {
			struct qseecom_qteec_ireq *req;
			struct qseecom_qteec_64bit_ireq *req_64bit;
			smc_id = TZ_APP_GPAPP_OPEN_SESSION_ID;
			desc.arginfo = TZ_APP_GPAPP_OPEN_SESSION_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_qteec_ireq *)req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->resp_ptr;
				desc.args[4] = req->resp_len;
			} else {
				req_64bit = (struct qseecom_qteec_64bit_ireq *)
						req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->resp_ptr;
				desc.args[4] = req_64bit->resp_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_TEE_OPEN_SESSION_WHITELIST: {
			struct qseecom_qteec_ireq *req;
			struct qseecom_qteec_64bit_ireq *req_64bit;

			smc_id = TZ_APP_GPAPP_OPEN_SESSION_WITH_WHITELIST_ID;
			desc.arginfo =
			TZ_APP_GPAPP_OPEN_SESSION_WITH_WHITELIST_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_qteec_ireq *)req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->resp_ptr;
				desc.args[4] = req->resp_len;
				desc.args[5] = req->sglistinfo_ptr;
				desc.args[6] = req->sglistinfo_len;
			} else {
				req_64bit = (struct qseecom_qteec_64bit_ireq *)
						req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->resp_ptr;
				desc.args[4] = req_64bit->resp_len;
				desc.args[5] = req_64bit->sglistinfo_ptr;
				desc.args[6] = req_64bit->sglistinfo_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_TEE_INVOKE_COMMAND: {
			struct qseecom_qteec_ireq *req;
			struct qseecom_qteec_64bit_ireq *req_64bit;
			smc_id = TZ_APP_GPAPP_INVOKE_COMMAND_ID;
			desc.arginfo = TZ_APP_GPAPP_INVOKE_COMMAND_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_qteec_ireq *)req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->resp_ptr;
				desc.args[4] = req->resp_len;
			} else {
				req_64bit = (struct qseecom_qteec_64bit_ireq *)
						req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->resp_ptr;
				desc.args[4] = req_64bit->resp_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_TEE_INVOKE_COMMAND_WHITELIST: {
			struct qseecom_qteec_ireq *req;
			struct qseecom_qteec_64bit_ireq *req_64bit;

			smc_id = TZ_APP_GPAPP_INVOKE_COMMAND_WITH_WHITELIST_ID;
			desc.arginfo =
			TZ_APP_GPAPP_INVOKE_COMMAND_WITH_WHITELIST_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_qteec_ireq *)req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->resp_ptr;
				desc.args[4] = req->resp_len;
				desc.args[5] = req->sglistinfo_ptr;
				desc.args[6] = req->sglistinfo_len;
			} else {
				req_64bit = (struct qseecom_qteec_64bit_ireq *)
						req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->resp_ptr;
				desc.args[4] = req_64bit->resp_len;
				desc.args[5] = req_64bit->sglistinfo_ptr;
				desc.args[6] = req_64bit->sglistinfo_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_TEE_CLOSE_SESSION: {
			struct qseecom_qteec_ireq *req;
			struct qseecom_qteec_64bit_ireq *req_64bit;
			smc_id = TZ_APP_GPAPP_CLOSE_SESSION_ID;
			desc.arginfo = TZ_APP_GPAPP_CLOSE_SESSION_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_qteec_ireq *)req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->resp_ptr;
				desc.args[4] = req->resp_len;
			} else {
				req_64bit = (struct qseecom_qteec_64bit_ireq *)
						req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->resp_ptr;
				desc.args[4] = req_64bit->resp_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_TEE_REQUEST_CANCELLATION: {
			struct qseecom_qteec_ireq *req;
			struct qseecom_qteec_64bit_ireq *req_64bit;
			smc_id = TZ_APP_GPAPP_REQUEST_CANCELLATION_ID;
			desc.arginfo =
				TZ_APP_GPAPP_REQUEST_CANCELLATION_ID_PARAM_ID;
			if (qseecom.qsee_version < QSEE_VERSION_40) {
				req = (struct qseecom_qteec_ireq *)req_buf;
				desc.args[0] = req->app_id;
				desc.args[1] = req->req_ptr;
				desc.args[2] = req->req_len;
				desc.args[3] = req->resp_ptr;
				desc.args[4] = req->resp_len;
			} else {
				req_64bit = (struct qseecom_qteec_64bit_ireq *)
						req_buf;
				desc.args[0] = req_64bit->app_id;
				desc.args[1] = req_64bit->req_ptr;
				desc.args[2] = req_64bit->req_len;
				desc.args[3] = req_64bit->resp_ptr;
				desc.args[4] = req_64bit->resp_len;
			}
			ret = scm_call2(smc_id, &desc);
			break;
		}
		case QSEOS_CONTINUE_BLOCKED_REQ_COMMAND: {
			struct qseecom_continue_blocked_request_ireq *req =
				(struct qseecom_continue_blocked_request_ireq *)
				req_buf;
			smc_id = TZ_OS_CONTINUE_BLOCKED_REQUEST_ID;
			desc.arginfo =
				TZ_OS_CONTINUE_BLOCKED_REQUEST_ID_PARAM_ID;
			desc.args[0] = req->app_id;
			ret = scm_call2(smc_id, &desc);
			break;
		}
		default: {
			pr_err("qseos_cmd_id 0x%d is not supported by armv8 scm_call2.\n",
						qseos_cmd_id);
			ret = -EINVAL;
			break;
		}
		} /*end of switch (qsee_cmd_id)  */
	break;
	} /*end of case SCM_SVC_TZSCHEDULER*/
	default: {
		pr_err("svc_id 0x%x is not supported by armv8 scm_call2.\n",
					svc_id);
		ret = -EINVAL;
		break;
	}
	} /*end of switch svc_id */
	scm_resp->result = desc.ret[0];
	scm_resp->resp_type = desc.ret[1];
	scm_resp->data = desc.ret[2];
	pr_debug("svc_id = 0x%x, tz_cmd_id = 0x%x, qseos_cmd_id = 0x%x, smc_id = 0x%x, param_id = 0x%x\n",
		svc_id, tz_cmd_id, qseos_cmd_id, smc_id, desc.arginfo);
	pr_debug("scm_resp->result = 0x%x, scm_resp->resp_type = 0x%x, scm_resp->data = 0x%x\n",
		scm_resp->result, scm_resp->resp_type, scm_resp->data);
	return ret;
}


static int qseecom_scm_call(u32 svc_id, u32 tz_cmd_id, const void *cmd_buf,
		size_t cmd_len, void *resp_buf, size_t resp_len)
{
	if (!is_scm_armv8())
		return scm_call(svc_id, tz_cmd_id, cmd_buf, cmd_len,
				resp_buf, resp_len);
	else
		return qseecom_scm_call2(svc_id, tz_cmd_id, cmd_buf, resp_buf);
}

static int __qseecom_is_svc_unique(struct qseecom_dev_handle *data,
		struct qseecom_register_listener_req *svc)
{
	struct qseecom_registered_listener_list *ptr;
	int unique = 1;
	unsigned long flags;

	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(ptr, &qseecom.registered_listener_list_head, list) {
		if (ptr->svc.listener_id == svc->listener_id) {
			pr_err("Service id: %u is already registered\n",
					ptr->svc.listener_id);
			unique = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);
	return unique;
}

static struct qseecom_registered_listener_list *__qseecom_find_svc(
						int32_t listener_id)
{
	struct qseecom_registered_listener_list *entry = NULL;
	unsigned long flags;

	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(entry, &qseecom.registered_listener_list_head, list)
	{
		if (entry->svc.listener_id == listener_id)
			break;
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	if ((entry != NULL) && (entry->svc.listener_id != listener_id)) {
		pr_err("Service id: %u is not found\n", listener_id);
		return NULL;
	}

	return entry;
}

static int __qseecom_set_sb_memory(struct qseecom_registered_listener_list *svc,
				struct qseecom_dev_handle *handle,
				struct qseecom_register_listener_req *listener)
{
	int ret = 0;
	struct qseecom_register_listener_ireq req;
	struct qseecom_register_listener_64bit_ireq req_64bit;
	struct qseecom_command_scm_resp resp;
	ion_phys_addr_t pa;
	void *cmd_buf = NULL;
	size_t cmd_len;

	/* Get the handle of the shared fd */
	svc->ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					listener->ifd_data_fd);
	if (IS_ERR_OR_NULL(svc->ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		return -ENOMEM;
	}

	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, svc->ihandle, &pa, &svc->sb_length);
	if (ret) {
		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		return ret;
	}
	/* Populate the structure for sending scm call to load image */
	svc->sb_virt = (char *) ion_map_kernel(qseecom.ion_clnt, svc->ihandle);
	if (IS_ERR_OR_NULL(svc->sb_virt)) {
		pr_err("ION memory mapping for listener shared buffer failed\n");
		return -ENOMEM;
	}
	svc->sb_phys = (phys_addr_t)pa;

	if (qseecom.qsee_version < QSEE_VERSION_40) {
		req.qsee_cmd_id = QSEOS_REGISTER_LISTENER;
		req.listener_id = svc->svc.listener_id;
		req.sb_len = svc->sb_length;
		req.sb_ptr = (uint32_t)svc->sb_phys;
		cmd_buf = (void *)&req;
		cmd_len = sizeof(struct qseecom_register_listener_ireq);
	} else {
		req_64bit.qsee_cmd_id = QSEOS_REGISTER_LISTENER;
		req_64bit.listener_id = svc->svc.listener_id;
		req_64bit.sb_len = svc->sb_length;
		req_64bit.sb_ptr = (uint64_t)svc->sb_phys;
		cmd_buf = (void *)&req_64bit;
		cmd_len = sizeof(struct qseecom_register_listener_64bit_ireq);
	}

	resp.result = QSEOS_RESULT_INCOMPLETE;

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, cmd_buf, cmd_len,
					 &resp, sizeof(resp));
	if (ret) {
		pr_err("qseecom_scm_call failed with err: %d\n", ret);
		return -EINVAL;
	}

	if (resp.result != QSEOS_RESULT_SUCCESS) {
		pr_err("Error SB registration req: resp.result = %d\n",
			resp.result);
		return -EPERM;
	}
	return 0;
}

static int qseecom_register_listener(struct qseecom_dev_handle *data,
					void __user *argp)
{
	int ret = 0;
	unsigned long flags;
	struct qseecom_register_listener_req rcvd_lstnr;
	struct qseecom_registered_listener_list *new_entry;

	ret = copy_from_user(&rcvd_lstnr, argp, sizeof(rcvd_lstnr));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	if (!access_ok(VERIFY_WRITE, (void __user *)rcvd_lstnr.virt_sb_base,
			rcvd_lstnr.sb_size))
		return -EFAULT;

	data->listener.id = 0;
	if (!__qseecom_is_svc_unique(data, &rcvd_lstnr)) {
		pr_err("Service is not unique and is already registered\n");
		data->released = true;
		return -EBUSY;
	}

	new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry) {
		pr_err("kmalloc failed\n");
		return -ENOMEM;
	}
	memcpy(&new_entry->svc, &rcvd_lstnr, sizeof(rcvd_lstnr));
	new_entry->rcv_req_flag = 0;

	new_entry->svc.listener_id = rcvd_lstnr.listener_id;
	new_entry->sb_length = rcvd_lstnr.sb_size;
	new_entry->user_virt_sb_base = rcvd_lstnr.virt_sb_base;
	if (__qseecom_set_sb_memory(new_entry, data, &rcvd_lstnr)) {
		pr_err("qseecom_set_sb_memoryfailed\n");
		kzfree(new_entry);
		return -ENOMEM;
	}

	data->listener.id = rcvd_lstnr.listener_id;
	init_waitqueue_head(&new_entry->rcv_req_wq);
	init_waitqueue_head(&new_entry->listener_block_app_wq);
	new_entry->send_resp_flag = 0;
	new_entry->listener_in_use = false;
	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_add_tail(&new_entry->list, &qseecom.registered_listener_list_head);
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	return ret;
}

static int qseecom_unregister_listener(struct qseecom_dev_handle *data)
{
	int ret = 0;
	unsigned long flags;
	uint32_t unmap_mem = 0;
	struct qseecom_register_listener_ireq req;
	struct qseecom_registered_listener_list *ptr_svc = NULL;
	struct qseecom_command_scm_resp resp;
	struct ion_handle *ihandle = NULL;		/* Retrieve phy addr */

	req.qsee_cmd_id = QSEOS_DEREGISTER_LISTENER;
	req.listener_id = data->listener.id;
	resp.result = QSEOS_RESULT_INCOMPLETE;

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, &req,
					sizeof(req), &resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call() failed with err: %d (lstnr id=%d)\n",
				ret, data->listener.id);
		return ret;
	}

	if (resp.result != QSEOS_RESULT_SUCCESS) {
		pr_err("Failed resp.result=%d,(lstnr id=%d)\n",
				resp.result, data->listener.id);
		return -EPERM;
	}

	data->abort = 1;
	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(ptr_svc, &qseecom.registered_listener_list_head,
			list) {
		if (ptr_svc->svc.listener_id == data->listener.id) {
			wake_up_all(&ptr_svc->rcv_req_wq);
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	while (atomic_read(&data->ioctl_count) > 1) {
		if (wait_event_freezable(data->abort_wq,
				atomic_read(&data->ioctl_count) <= 1)) {
			pr_err("Interrupted from abort\n");
			ret = -ERESTARTSYS;
			return ret;
		}
	}

	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(ptr_svc,
			&qseecom.registered_listener_list_head,
			list)
	{
		if (ptr_svc->svc.listener_id == data->listener.id) {
			if (ptr_svc->sb_virt) {
				unmap_mem = 1;
				ihandle = ptr_svc->ihandle;
				}
			list_del(&ptr_svc->list);
			kzfree(ptr_svc);
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	/* Unmap the memory */
	if (unmap_mem) {
		if (!IS_ERR_OR_NULL(ihandle)) {
			ion_unmap_kernel(qseecom.ion_clnt, ihandle);
			ion_free(qseecom.ion_clnt, ihandle);
			}
	}
	data->released = true;
	return ret;
}

static int __qseecom_set_msm_bus_request(uint32_t mode)
{
	int ret = 0;
	struct qseecom_clk *qclk;

	qclk = &qseecom.qsee;
	if (qclk->ce_core_src_clk != NULL) {
		if (mode == INACTIVE) {
			__qseecom_disable_clk(CLK_QSEE);
		} else {
			ret = __qseecom_enable_clk(CLK_QSEE);
			if (ret)
				pr_err("CLK enabling failed (%d) MODE (%d)\n",
							ret, mode);
		}
	}

	if ((!ret) && (qseecom.current_mode != mode)) {
		ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, mode);
		if (ret) {
			pr_err("Bandwidth req failed(%d) MODE (%d)\n",
							ret, mode);
			if (qclk->ce_core_src_clk != NULL) {
				if (mode == INACTIVE) {
					ret = __qseecom_enable_clk(CLK_QSEE);
					if (ret)
						pr_err("CLK enable failed\n");
				} else
					__qseecom_disable_clk(CLK_QSEE);
			}
		}
		qseecom.current_mode = mode;
	}
	return ret;
}

static void qseecom_bw_inactive_req_work(struct work_struct *work)
{
	mutex_lock(&app_access_lock);
	mutex_lock(&qsee_bw_mutex);
	if (qseecom.timer_running)
		__qseecom_set_msm_bus_request(INACTIVE);
	pr_debug("current_mode = %d, cumulative_mode = %d\n",
				qseecom.current_mode, qseecom.cumulative_mode);
	qseecom.timer_running = false;
	mutex_unlock(&qsee_bw_mutex);
	mutex_unlock(&app_access_lock);
	return;
}

static void qseecom_scale_bus_bandwidth_timer_callback(unsigned long data)
{
	schedule_work(&qseecom.bw_inactive_req_ws);
	return;
}

static int __qseecom_decrease_clk_ref_count(enum qseecom_ce_hw_instance ce)
{
	struct qseecom_clk *qclk;
	int ret = 0;
	mutex_lock(&clk_access_lock);
	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	else
		qclk = &qseecom.ce_drv;

	if (qclk->clk_access_cnt > 2) {
		pr_err("Invalid clock ref count %d\n", qclk->clk_access_cnt);
		ret = -EINVAL;
		goto err_dec_ref_cnt;
	}
	if (qclk->clk_access_cnt == 2)
		qclk->clk_access_cnt--;

err_dec_ref_cnt:
	mutex_unlock(&clk_access_lock);
	return ret;
}


static int qseecom_scale_bus_bandwidth_timer(uint32_t mode)
{
	int32_t ret = 0;
	int32_t request_mode = INACTIVE;

	mutex_lock(&qsee_bw_mutex);
	if (mode == 0) {
		if (qseecom.cumulative_mode > MEDIUM)
			request_mode = HIGH;
		else
			request_mode = qseecom.cumulative_mode;
	} else {
		request_mode = mode;
	}

	ret = __qseecom_set_msm_bus_request(request_mode);
	if (ret) {
		pr_err("set msm bus request failed (%d),request_mode (%d)\n",
			ret, request_mode);
		goto err_scale_timer;
	}

	if (qseecom.timer_running) {
		ret = __qseecom_decrease_clk_ref_count(CLK_QSEE);
		if (ret) {
			pr_err("Failed to decrease clk ref count.\n");
			goto err_scale_timer;
		}
		del_timer_sync(&(qseecom.bw_scale_down_timer));
		qseecom.timer_running = false;
	}
err_scale_timer:
	mutex_unlock(&qsee_bw_mutex);
	return ret;
}


static int qseecom_unregister_bus_bandwidth_needs(
					struct qseecom_dev_handle *data)
{
	int32_t ret = 0;

	qseecom.cumulative_mode -= data->mode;
	data->mode = INACTIVE;

	return ret;
}

static int __qseecom_register_bus_bandwidth_needs(
			struct qseecom_dev_handle *data, uint32_t request_mode)
{
	int32_t ret = 0;

	if (data->mode == INACTIVE) {
		qseecom.cumulative_mode += request_mode;
		data->mode = request_mode;
	} else {
		if (data->mode != request_mode) {
			qseecom.cumulative_mode -= data->mode;
			qseecom.cumulative_mode += request_mode;
			data->mode = request_mode;
		}
	}
	return ret;
}

static int qseecom_perf_enable(struct qseecom_dev_handle *data)
{
	int ret = 0;
	ret = qsee_vote_for_clock(data, CLK_DFAB);
	if (ret) {
		pr_err("Failed to vote for DFAB clock with err %d\n", ret);
		goto perf_enable_exit;
	}
	ret = qsee_vote_for_clock(data, CLK_SFPB);
	if (ret) {
		qsee_disable_clock_vote(data, CLK_DFAB);
		pr_err("Failed to vote for SFPB clock with err %d\n", ret);
		goto perf_enable_exit;
	}

perf_enable_exit:
	return ret;
}

static int qseecom_scale_bus_bandwidth(struct qseecom_dev_handle *data,
						void __user *argp)
{
	int32_t ret = 0;
	int32_t req_mode;

	if (qseecom.no_clock_support)
		return 0;

	ret = copy_from_user(&req_mode, argp, sizeof(req_mode));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	if (req_mode > HIGH) {
		pr_err("Invalid bandwidth mode (%d)\n", req_mode);
		return -EINVAL;
	}

	/*
	* Register bus bandwidth needs if bus scaling feature is enabled;
	* otherwise, qseecom enable/disable clocks for the client directly.
	*/
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, req_mode);
		mutex_unlock(&qsee_bw_mutex);
	} else {
		pr_debug("Bus scaling feature is NOT enabled\n");
		pr_debug("request bandwidth mode %d for the client\n",
				req_mode);
		if (req_mode != INACTIVE) {
			ret = qseecom_perf_enable(data);
			if (ret)
				pr_err("Failed to vote for clock with err %d\n",
						ret);
		} else {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		}
	}
	return ret;
}

static void __qseecom_add_bw_scale_down_timer(uint32_t duration)
{
	if (qseecom.no_clock_support)
		return;

	mutex_lock(&qsee_bw_mutex);
	qseecom.bw_scale_down_timer.expires = jiffies +
		msecs_to_jiffies(duration);
	mod_timer(&(qseecom.bw_scale_down_timer),
		qseecom.bw_scale_down_timer.expires);
	qseecom.timer_running = true;
	mutex_unlock(&qsee_bw_mutex);
}

static void __qseecom_disable_clk_scale_down(struct qseecom_dev_handle *data)
{
	if (!qseecom.support_bus_scaling)
		qsee_disable_clock_vote(data, CLK_SFPB);
	else
		__qseecom_add_bw_scale_down_timer(
			QSEECOM_LOAD_APP_CRYPTO_TIMEOUT);
	return;
}

static int __qseecom_enable_clk_scale_up(struct qseecom_dev_handle *data)
{
	int ret = 0;
	if (qseecom.support_bus_scaling) {
		ret = qseecom_scale_bus_bandwidth_timer(MEDIUM);
		if (ret)
			pr_err("Failed to set bw MEDIUM.\n");
	} else {
		ret = qsee_vote_for_clock(data, CLK_SFPB);
		if (ret)
			pr_err("Fail vote for clk SFPB ret %d\n", ret);
	}
	return ret;
}

static int qseecom_set_client_mem_param(struct qseecom_dev_handle *data,
						void __user *argp)
{
	ion_phys_addr_t pa;
	int32_t ret;
	struct qseecom_set_sb_mem_param_req req;
	size_t len;

	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&req, (void __user *)argp, sizeof(req)))
		return -EFAULT;

	if ((req.ifd_data_fd <= 0) || (req.virt_sb_base == NULL) ||
					(req.sb_len == 0)) {
		pr_err("Inavlid input(s)ion_fd(%d), sb_len(%d), vaddr(0x%pK)\n",
			req.ifd_data_fd, req.sb_len, req.virt_sb_base);
		return -EFAULT;
	}
	if (!access_ok(VERIFY_WRITE, (void __user *)req.virt_sb_base,
			req.sb_len))
		return -EFAULT;

	/* Get the handle of the shared fd */
	data->client.ihandle = ion_import_dma_buf(qseecom.ion_clnt,
						req.ifd_data_fd);
	if (IS_ERR_OR_NULL(data->client.ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		return -ENOMEM;
	}
	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, data->client.ihandle, &pa, &len);
	if (ret) {

		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		return ret;
	}

	if (len < req.sb_len) {
		pr_err("Requested length (0x%x) is > allocated (0x%zu)\n",
			req.sb_len, len);
		return -EINVAL;
	}
	/* Populate the structure for sending scm call to load image */
	data->client.sb_virt = (char *) ion_map_kernel(qseecom.ion_clnt,
							data->client.ihandle);
	if (IS_ERR_OR_NULL(data->client.sb_virt)) {
		pr_err("ION memory mapping for client shared buf failed\n");
		return -ENOMEM;
	}
	data->client.sb_phys = (phys_addr_t)pa;
	data->client.sb_length = req.sb_len;
	data->client.user_virt_sb_base = (uintptr_t)req.virt_sb_base;
	return 0;
}

static int __qseecom_listener_has_sent_rsp(struct qseecom_dev_handle *data)
{
	int ret;
	ret = (qseecom.send_resp_flag != 0);
	return ret || data->abort;
}

static int __qseecom_reentrancy_listener_has_sent_rsp(
			struct qseecom_dev_handle *data,
			struct qseecom_registered_listener_list *ptr_svc)
{
	int ret;

	ret = (ptr_svc->send_resp_flag != 0);
	return ret || data->abort;
}

static int __qseecom_qseos_fail_return_resp_tz(struct qseecom_dev_handle *data,
					struct qseecom_command_scm_resp *resp,
			struct qseecom_client_listener_data_irsp *send_data_rsp,
			struct qseecom_registered_listener_list *ptr_svc,
							uint32_t lstnr) {
	int ret = 0;

	send_data_rsp->status = QSEOS_RESULT_FAILURE;
	qseecom.send_resp_flag = 0;
	send_data_rsp->qsee_cmd_id = QSEOS_LISTENER_DATA_RSP_COMMAND;
	send_data_rsp->listener_id = lstnr;
	if (ptr_svc)
		pr_warn("listener_id:%x, lstnr: %x\n",
					ptr_svc->svc.listener_id, lstnr);
	if (ptr_svc && ptr_svc->ihandle) {
		ret = msm_ion_do_cache_op(qseecom.ion_clnt, ptr_svc->ihandle,
					ptr_svc->sb_virt, ptr_svc->sb_length,
					ION_IOC_CLEAN_INV_CACHES);
		if (ret) {
			pr_err("cache operation failed %d\n", ret);
			return ret;
		}
	}

	if (lstnr == RPMB_SERVICE) {
		ret = __qseecom_enable_clk(CLK_QSEE);
		if (ret)
			return ret;
	}
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, send_data_rsp,
				sizeof(send_data_rsp), resp, sizeof(*resp));
	if (ret) {
		pr_err("scm_call() failed with err: %d (app_id = %d)\n",
						ret, data->client.app_id);
		if (lstnr == RPMB_SERVICE)
			__qseecom_disable_clk(CLK_QSEE);
		return ret;
	}
	if ((resp->result != QSEOS_RESULT_SUCCESS) &&
			(resp->result != QSEOS_RESULT_INCOMPLETE)) {
		pr_err("fail:resp res= %d,app_id = %d,lstr = %d\n",
				resp->result, data->client.app_id, lstnr);
		ret = -EINVAL;
	}
	if (lstnr == RPMB_SERVICE)
		__qseecom_disable_clk(CLK_QSEE);
	return ret;
}

static void __qseecom_clean_listener_sglistinfo(
			struct qseecom_registered_listener_list *ptr_svc)
{
	if (ptr_svc->sglist_cnt) {
		memset(ptr_svc->sglistinfo_ptr, 0,
			SGLISTINFO_TABLE_SIZE);
		ptr_svc->sglist_cnt = 0;
	}
}

static int __qseecom_process_incomplete_cmd(struct qseecom_dev_handle *data,
					struct qseecom_command_scm_resp *resp)
{
	int ret = 0;
	int rc = 0;
	uint32_t lstnr;
	unsigned long flags;
	struct qseecom_client_listener_data_irsp send_data_rsp;
	struct qseecom_client_listener_data_64bit_irsp send_data_rsp_64bit;
	struct qseecom_registered_listener_list *ptr_svc = NULL;
	sigset_t new_sigset;
	sigset_t old_sigset;
	uint32_t status;
	void *cmd_buf = NULL;
	size_t cmd_len;
	struct sglist_info *table = NULL;

	while (resp->result == QSEOS_RESULT_INCOMPLETE) {
		lstnr = resp->data;
		/*
		 * Wake up blocking lsitener service with the lstnr id
		 */
		spin_lock_irqsave(&qseecom.registered_listener_list_lock,
					flags);
		list_for_each_entry(ptr_svc,
				&qseecom.registered_listener_list_head, list) {
			if (ptr_svc->svc.listener_id == lstnr) {
				ptr_svc->listener_in_use = true;
				ptr_svc->rcv_req_flag = 1;
				wake_up_interruptible(&ptr_svc->rcv_req_wq);
				break;
			}
		}
		spin_unlock_irqrestore(&qseecom.registered_listener_list_lock,
				flags);

		if (ptr_svc == NULL) {
			pr_err("Listener Svc %d does not exist\n", lstnr);
			__qseecom_qseos_fail_return_resp_tz(data, resp,
					&send_data_rsp, ptr_svc, lstnr);
			return -EINVAL;
		}

		if (!ptr_svc->ihandle) {
			pr_err("Client handle is not initialized\n");
			__qseecom_qseos_fail_return_resp_tz(data, resp,
					&send_data_rsp, ptr_svc, lstnr);
			return -EINVAL;
		}

		if (ptr_svc->svc.listener_id != lstnr) {
			pr_warn("Service requested does not exist\n");
			__qseecom_qseos_fail_return_resp_tz(data, resp,
					&send_data_rsp, ptr_svc, lstnr);
			return -ERESTARTSYS;
		}
		pr_debug("waking up rcv_req_wq and waiting for send_resp_wq\n");

		/* initialize the new signal mask with all signals*/
		sigfillset(&new_sigset);
		/* block all signals */
		sigprocmask(SIG_SETMASK, &new_sigset, &old_sigset);

		do {
			/*
			 * When reentrancy is not supported, check global
			 * send_resp_flag; otherwise, check this listener's
			 * send_resp_flag.
			 */
			if (!qseecom.qsee_reentrancy_support &&
				!wait_event_freezable(qseecom.send_resp_wq,
				__qseecom_listener_has_sent_rsp(data))) {
					break;
			}

			if (qseecom.qsee_reentrancy_support &&
				!wait_event_freezable(qseecom.send_resp_wq,
				__qseecom_reentrancy_listener_has_sent_rsp(
						data, ptr_svc))) {
					break;
			}
		} while (1);

		/* restore signal mask */
		sigprocmask(SIG_SETMASK, &old_sigset, NULL);
		if (data->abort) {
			pr_err("Abort clnt %d waiting on lstnr svc %d, ret %d",
				data->client.app_id, lstnr, ret);
			rc = -ENODEV;
			status = QSEOS_RESULT_FAILURE;
		} else {
			status = QSEOS_RESULT_SUCCESS;
		}

		qseecom.send_resp_flag = 0;
		ptr_svc->send_resp_flag = 0;
		table = ptr_svc->sglistinfo_ptr;
		if (qseecom.qsee_version < QSEE_VERSION_40) {
			send_data_rsp.listener_id  = lstnr;
			send_data_rsp.status = status;
			send_data_rsp.sglistinfo_ptr =
				(uint32_t)virt_to_phys(table);
			send_data_rsp.sglistinfo_len =
				SGLISTINFO_TABLE_SIZE;
			dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
			cmd_buf = (void *)&send_data_rsp;
			cmd_len = sizeof(send_data_rsp);
		} else {
			send_data_rsp_64bit.listener_id  = lstnr;
			send_data_rsp_64bit.status = status;
			send_data_rsp_64bit.sglistinfo_ptr =
				virt_to_phys(table);
			send_data_rsp_64bit.sglistinfo_len =
				SGLISTINFO_TABLE_SIZE;
			dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
			cmd_buf = (void *)&send_data_rsp_64bit;
			cmd_len = sizeof(send_data_rsp_64bit);
		}
		if (qseecom.whitelist_support == false)
			*(uint32_t *)cmd_buf = QSEOS_LISTENER_DATA_RSP_COMMAND;
		else
			*(uint32_t *)cmd_buf =
				QSEOS_LISTENER_DATA_RSP_COMMAND_WHITELIST;
		if (ptr_svc) {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
					ptr_svc->ihandle,
					ptr_svc->sb_virt, ptr_svc->sb_length,
					ION_IOC_CLEAN_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				return ret;
			}
		}

		if ((lstnr == RPMB_SERVICE) || (lstnr == SSD_SERVICE)) {
			ret = __qseecom_enable_clk(CLK_QSEE);
			if (ret)
				return ret;
		}

		ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
					cmd_buf, cmd_len, resp, sizeof(*resp));
		ptr_svc->listener_in_use = false;
		__qseecom_clean_listener_sglistinfo(ptr_svc);
		if (ret) {
			pr_err("scm_call() failed with err: %d (app_id = %d)\n",
				ret, data->client.app_id);
			if ((lstnr == RPMB_SERVICE) || (lstnr == SSD_SERVICE))
				__qseecom_disable_clk(CLK_QSEE);
			return ret;
		}
		if ((resp->result != QSEOS_RESULT_SUCCESS) &&
			(resp->result != QSEOS_RESULT_INCOMPLETE)) {
			pr_err("fail:resp res= %d,app_id = %d,lstr = %d\n",
				resp->result, data->client.app_id, lstnr);
			ret = -EINVAL;
		}
		if ((lstnr == RPMB_SERVICE) || (lstnr == SSD_SERVICE))
			__qseecom_disable_clk(CLK_QSEE);

	}
	if (rc)
		return rc;

	return ret;
}

int __qseecom_process_reentrancy_blocked_on_listener(
				struct qseecom_command_scm_resp *resp,
				struct qseecom_registered_app_list *ptr_app,
				struct qseecom_dev_handle *data)
{
	struct qseecom_registered_listener_list *list_ptr;
	int ret = 0;
	struct qseecom_continue_blocked_request_ireq ireq;
	struct qseecom_command_scm_resp continue_resp;
	sigset_t new_sigset, old_sigset;
	unsigned long flags;
	bool found_app = false;

	if (!resp || !data) {
		pr_err("invalid resp or data pointer\n");
		ret = -EINVAL;
		goto exit;
	}

	/* find app_id & img_name from list */
	if (!ptr_app) {
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(ptr_app, &qseecom.registered_app_list_head,
							list) {
			if ((ptr_app->app_id == data->client.app_id) &&
				(!strcmp(ptr_app->app_name,
						data->client.app_name))) {
				found_app = true;
				break;
			}
		}
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
					flags);
		if (!found_app) {
			pr_err("app_id %d (%s) is not found\n",
				data->client.app_id,
				(char *)data->client.app_name);
			ret = -ENOENT;
			goto exit;
		}
	}

	list_ptr = __qseecom_find_svc(resp->data);
	if (!list_ptr) {
		pr_err("Invalid listener ID\n");
		ret = -ENODATA;
		goto exit;
	}
	pr_debug("lsntr %d in_use = %d\n",
			resp->data, list_ptr->listener_in_use);
	ptr_app->blocked_on_listener_id = resp->data;
	/* sleep until listener is available */
	do {
		qseecom.app_block_ref_cnt++;
		ptr_app->app_blocked = true;
		sigfillset(&new_sigset);
		sigprocmask(SIG_SETMASK, &new_sigset, &old_sigset);
		mutex_unlock(&app_access_lock);
		do {
			if (!wait_event_freezable(
				list_ptr->listener_block_app_wq,
				!list_ptr->listener_in_use)) {
				break;
			}
		} while (1);
		mutex_lock(&app_access_lock);
		sigprocmask(SIG_SETMASK, &old_sigset, NULL);
		ptr_app->app_blocked = false;
		qseecom.app_block_ref_cnt--;
	} while (list_ptr->listener_in_use == true);
	ptr_app->blocked_on_listener_id = 0;
	/* notify the blocked app that listener is available */
	pr_warn("Lsntr %d is available, unblock app(%d) %s in TZ\n",
		resp->data, data->client.app_id,
		data->client.app_name);
	ireq.qsee_cmd_id = QSEOS_CONTINUE_BLOCKED_REQ_COMMAND;
	ireq.app_id = data->client.app_id;
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
			&ireq, sizeof(ireq),
			&continue_resp, sizeof(continue_resp));
	if (ret) {
		pr_err("scm_call for continue blocked req for app(%d) %s failed, ret %d\n",
			data->client.app_id,
			data->client.app_name, ret);
		goto exit;
	}
	/*
	 * After TZ app is unblocked, then continue to next case
	 * for incomplete request processing
	 */
	resp->result = QSEOS_RESULT_INCOMPLETE;
exit:
	return ret;
}

static int __qseecom_reentrancy_process_incomplete_cmd(
					struct qseecom_dev_handle *data,
					struct qseecom_command_scm_resp *resp)
{
	int ret = 0;
	int rc = 0;
	uint32_t lstnr = 0;
	unsigned long flags;
	struct qseecom_client_listener_data_irsp send_data_rsp;
	struct qseecom_client_listener_data_64bit_irsp send_data_rsp_64bit;
	struct qseecom_registered_listener_list *ptr_svc = NULL;
	sigset_t new_sigset;
	sigset_t old_sigset;
	uint32_t status;
	void *cmd_buf = NULL;
	size_t cmd_len;
	struct sglist_info *table = NULL;

	while (ret == 0 && rc == 0 && resp->result == QSEOS_RESULT_INCOMPLETE) {
		lstnr = resp->data;
		/*
		 * Wake up blocking lsitener service with the lstnr id
		 */
		spin_lock_irqsave(&qseecom.registered_listener_list_lock,
					flags);
		list_for_each_entry(ptr_svc,
				&qseecom.registered_listener_list_head, list) {
			if (ptr_svc->svc.listener_id == lstnr) {
				ptr_svc->listener_in_use = true;
				ptr_svc->rcv_req_flag = 1;
				wake_up_interruptible(&ptr_svc->rcv_req_wq);
				break;
			}
		}
		spin_unlock_irqrestore(&qseecom.registered_listener_list_lock,
				flags);

		if (ptr_svc == NULL) {
			pr_err("Listener Svc %d does not exist\n", lstnr);
			return -EINVAL;
		}

		if (!ptr_svc->ihandle) {
			pr_err("Client handle is not initialized\n");
			return -EINVAL;
		}

		if (ptr_svc->svc.listener_id != lstnr) {
			pr_warn("Service requested does not exist\n");
			return -ERESTARTSYS;
		}
		pr_debug("waking up rcv_req_wq and waiting for send_resp_wq\n");

		/* initialize the new signal mask with all signals*/
		sigfillset(&new_sigset);

		/* block all signals */
		sigprocmask(SIG_SETMASK, &new_sigset, &old_sigset);

		/* unlock mutex btw waking listener and sleep-wait */
		mutex_unlock(&app_access_lock);
		do {
			if (!wait_event_freezable(qseecom.send_resp_wq,
				__qseecom_reentrancy_listener_has_sent_rsp(
						data, ptr_svc))) {
					break;
			}
		} while (1);
		/* lock mutex again after resp sent */
		mutex_lock(&app_access_lock);
		ptr_svc->send_resp_flag = 0;
		qseecom.send_resp_flag = 0;

		/* restore signal mask */
		sigprocmask(SIG_SETMASK, &old_sigset, NULL);
		if (data->abort) {
			pr_err("Abort clnt %d waiting on lstnr svc %d, ret %d",
				data->client.app_id, lstnr, ret);
			rc = -ENODEV;
			status  = QSEOS_RESULT_FAILURE;
		} else {
			status  = QSEOS_RESULT_SUCCESS;
		}
		table = ptr_svc->sglistinfo_ptr;
		if (qseecom.qsee_version < QSEE_VERSION_40) {
			send_data_rsp.listener_id  = lstnr;
			send_data_rsp.status = status;
			send_data_rsp.sglistinfo_ptr =
				(uint32_t)virt_to_phys(table);
			send_data_rsp.sglistinfo_len = SGLISTINFO_TABLE_SIZE;
			dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
			cmd_buf = (void *)&send_data_rsp;
			cmd_len = sizeof(send_data_rsp);
		} else {
			send_data_rsp_64bit.listener_id  = lstnr;
			send_data_rsp_64bit.status = status;
			send_data_rsp_64bit.sglistinfo_ptr =
				virt_to_phys(table);
			send_data_rsp_64bit.sglistinfo_len =
				SGLISTINFO_TABLE_SIZE;
			dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
			cmd_buf = (void *)&send_data_rsp_64bit;
			cmd_len = sizeof(send_data_rsp_64bit);
		}
		if (qseecom.whitelist_support == false)
			*(uint32_t *)cmd_buf = QSEOS_LISTENER_DATA_RSP_COMMAND;
		else
			*(uint32_t *)cmd_buf =
				QSEOS_LISTENER_DATA_RSP_COMMAND_WHITELIST;
		if (ptr_svc) {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
					ptr_svc->ihandle,
					ptr_svc->sb_virt, ptr_svc->sb_length,
					ION_IOC_CLEAN_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				return ret;
			}
		}
		if (lstnr == RPMB_SERVICE) {
			ret = __qseecom_enable_clk(CLK_QSEE);
			if (ret)
				return ret;
		}

		ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
					cmd_buf, cmd_len, resp, sizeof(*resp));
		ptr_svc->listener_in_use = false;
		__qseecom_clean_listener_sglistinfo(ptr_svc);
		wake_up_interruptible(&ptr_svc->listener_block_app_wq);

		if (ret) {
			pr_err("scm_call() failed with err: %d (app_id = %d)\n",
				ret, data->client.app_id);
			goto exit;
		}

		switch (resp->result) {
		case QSEOS_RESULT_BLOCKED_ON_LISTENER:
			pr_warn("send lsr %d rsp, but app %d block on lsr %d\n",
					lstnr, data->client.app_id, resp->data);
			if (lstnr == resp->data) {
				pr_err("lstnr %d should not be blocked!\n",
					lstnr);
				ret = -EINVAL;
				goto exit;
			}
			ret = __qseecom_process_reentrancy_blocked_on_listener(
					resp, NULL, data);
			if (ret) {
				pr_err("failed to process App(%d) %s blocked on listener %d\n",
					data->client.app_id,
					data->client.app_name, resp->data);
				goto exit;
			}
		case QSEOS_RESULT_SUCCESS:
		case QSEOS_RESULT_INCOMPLETE:
			break;
		default:
			pr_err("fail:resp res= %d,app_id = %d,lstr = %d\n",
				resp->result, data->client.app_id, lstnr);
			ret = -EINVAL;
			goto exit;
		}
exit:
		if (lstnr == RPMB_SERVICE)
			__qseecom_disable_clk(CLK_QSEE);
	}
	if (rc)
		return rc;

	return ret;
}

/*
 * QSEE doesn't support OS level cmds reentrancy until RE phase-3,
 * and QSEE OS level scm_call cmds will fail if there is any blocked TZ app.
 * So, needs to first check if no app blocked before sending OS level scm call,
 * then wait until all apps are unblocked.
 */
static void __qseecom_reentrancy_check_if_no_app_blocked(uint32_t smc_id)
{
	sigset_t new_sigset, old_sigset;

	if (qseecom.qsee_reentrancy_support > QSEE_REENTRANCY_PHASE_0 &&
		qseecom.qsee_reentrancy_support < QSEE_REENTRANCY_PHASE_3 &&
		IS_OWNER_TRUSTED_OS(TZ_SYSCALL_OWNER_ID(smc_id))) {
		/* thread sleep until this app unblocked */
		while (qseecom.app_block_ref_cnt > 0) {
			sigfillset(&new_sigset);
			sigprocmask(SIG_SETMASK, &new_sigset, &old_sigset);
			mutex_unlock(&app_access_lock);
			do {
				if (!wait_event_freezable(qseecom.app_block_wq,
					(qseecom.app_block_ref_cnt == 0)))
					break;
			} while (1);
			mutex_lock(&app_access_lock);
			sigprocmask(SIG_SETMASK, &old_sigset, NULL);
		}
	}
}

/*
 * scm_call of send data will fail if this TA is blocked or there are more
 * than one TA requesting listener services; So, first check to see if need
 * to wait.
 */
static void __qseecom_reentrancy_check_if_this_app_blocked(
			struct qseecom_registered_app_list *ptr_app)
{
	sigset_t new_sigset, old_sigset;
	if (qseecom.qsee_reentrancy_support) {
		while (ptr_app->app_blocked || qseecom.app_block_ref_cnt > 1) {
			/* thread sleep until this app unblocked */
			sigfillset(&new_sigset);
			sigprocmask(SIG_SETMASK, &new_sigset, &old_sigset);
			mutex_unlock(&app_access_lock);
			do {
				if (!wait_event_freezable(qseecom.app_block_wq,
					(!ptr_app->app_blocked &&
					qseecom.app_block_ref_cnt <= 1)))
					break;
			} while (1);
			mutex_lock(&app_access_lock);
			sigprocmask(SIG_SETMASK, &old_sigset, NULL);
		}
	}
}

static int __qseecom_check_app_exists(struct qseecom_check_app_ireq req,
					uint32_t *app_id)
{
	int32_t ret;
	struct qseecom_command_scm_resp resp;
	bool found_app = false;
	struct qseecom_registered_app_list *entry = NULL;
	unsigned long flags = 0;

	if (!app_id) {
		pr_err("Null pointer to app_id\n");
		return -EINVAL;
	}
	*app_id = 0;

	/* check if app exists and has been registered locally */
	spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
	list_for_each_entry(entry,
			&qseecom.registered_app_list_head, list) {
		if (!strcmp(entry->app_name, req.app_name)) {
			found_app = true;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_app_list_lock, flags);
	if (found_app) {
		pr_debug("Found app with id %d\n", entry->app_id);
		*app_id = entry->app_id;
		return 0;
	}

	memset((void *)&resp, 0, sizeof(resp));

	/*  SCM_CALL  to check if app_id for the mentioned app exists */
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, &req,
				sizeof(struct qseecom_check_app_ireq),
				&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to check if app is already loaded failed\n");
		return -EINVAL;
	}

	if (resp.result == QSEOS_RESULT_FAILURE)
		return 0;

	switch (resp.resp_type) {
	/*qsee returned listener type response */
	case QSEOS_LISTENER_ID:
		pr_err("resp type is of listener type instead of app");
		return -EINVAL;
	case QSEOS_APP_ID:
		*app_id = resp.data;
		return 0;
	default:
		pr_err("invalid resp type (%d) from qsee",
				resp.resp_type);
		return -ENODEV;
	}
}

static int qseecom_load_app(struct qseecom_dev_handle *data, void __user *argp)
{
	struct qseecom_registered_app_list *entry = NULL;
	unsigned long flags = 0;
	u32 app_id = 0;
	struct ion_handle *ihandle;	/* Ion handle */
	struct qseecom_load_img_req load_img_req;
	int32_t ret = 0;
	ion_phys_addr_t pa = 0;
	size_t len;
	struct qseecom_command_scm_resp resp;
	struct qseecom_check_app_ireq req;
	struct qseecom_load_app_ireq load_req;
	struct qseecom_load_app_64bit_ireq load_req_64bit;
	void *cmd_buf = NULL;
	size_t cmd_len;
	bool first_time = false;

	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&load_img_req,
				(void __user *)argp,
				sizeof(struct qseecom_load_img_req))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	/* Check and load cmnlib */
	if (qseecom.qsee_version > QSEEE_VERSION_00) {
		if (!qseecom.commonlib_loaded &&
				load_img_req.app_arch == ELFCLASS32) {
			ret = qseecom_load_commonlib_image(data, "cmnlib");
			if (ret) {
				pr_err("failed to load cmnlib\n");
				return -EIO;
			}
			qseecom.commonlib_loaded = true;
			pr_debug("cmnlib is loaded\n");
		}

		if (!qseecom.commonlib64_loaded &&
				load_img_req.app_arch == ELFCLASS64) {
			ret = qseecom_load_commonlib_image(data, "cmnlib64");
			if (ret) {
				pr_err("failed to load cmnlib64\n");
				return -EIO;
			}
			qseecom.commonlib64_loaded = true;
			pr_debug("cmnlib64 is loaded\n");
		}
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret)
			return ret;
	}

	/* Vote for the SFPB clock */
	ret = __qseecom_enable_clk_scale_up(data);
	if (ret)
		goto enable_clk_err;

	req.qsee_cmd_id = QSEOS_APP_LOOKUP_COMMAND;
	load_img_req.img_name[MAX_APP_NAME_SIZE-1] = '\0';
	strlcpy(req.app_name, load_img_req.img_name, MAX_APP_NAME_SIZE);

	ret = __qseecom_check_app_exists(req, &app_id);
	if (ret < 0)
		goto loadapp_err;

	if (app_id) {
		pr_debug("App id %d (%s) already exists\n", app_id,
			(char *)(req.app_name));
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(entry,
		&qseecom.registered_app_list_head, list){
			if (entry->app_id == app_id) {
				entry->ref_cnt++;
				break;
			}
		}
		spin_unlock_irqrestore(
		&qseecom.registered_app_list_lock, flags);
		ret = 0;
	} else {
		first_time = true;
		pr_warn("App (%s) does'nt exist, loading apps for first time\n",
			(char *)(load_img_req.img_name));
		/* Get the handle of the shared fd */
		ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					load_img_req.ifd_data_fd);
		if (IS_ERR_OR_NULL(ihandle)) {
			pr_err("Ion client could not retrieve the handle\n");
			ret = -ENOMEM;
			goto loadapp_err;
		}

		/* Get the physical address of the ION BUF */
		ret = ion_phys(qseecom.ion_clnt, ihandle, &pa, &len);
		if (ret) {
			pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
				ret);
			goto loadapp_err;
		}
		if (load_img_req.mdt_len > len || load_img_req.img_len > len) {
			pr_err("ion len %zu is smaller than mdt_len %u or img_len %u\n",
					len, load_img_req.mdt_len,
					load_img_req.img_len);
			ret = -EINVAL;
			goto loadapp_err;
		}
		/* Populate the structure for sending scm call to load image */
		if (qseecom.qsee_version < QSEE_VERSION_40) {
			load_req.qsee_cmd_id = QSEOS_APP_START_COMMAND;
			load_req.mdt_len = load_img_req.mdt_len;
			load_req.img_len = load_img_req.img_len;
			strlcpy(load_req.app_name, load_img_req.img_name,
						MAX_APP_NAME_SIZE);
			load_req.phy_addr = (uint32_t)pa;
			cmd_buf = (void *)&load_req;
			cmd_len = sizeof(struct qseecom_load_app_ireq);
		} else {
			load_req_64bit.qsee_cmd_id = QSEOS_APP_START_COMMAND;
			load_req_64bit.mdt_len = load_img_req.mdt_len;
			load_req_64bit.img_len = load_img_req.img_len;
			strlcpy(load_req_64bit.app_name, load_img_req.img_name,
						MAX_APP_NAME_SIZE);
			load_req_64bit.phy_addr = (uint64_t)pa;
			cmd_buf = (void *)&load_req_64bit;
			cmd_len = sizeof(struct qseecom_load_app_64bit_ireq);
		}

		ret = msm_ion_do_cache_op(qseecom.ion_clnt, ihandle, NULL, len,
					ION_IOC_CLEAN_INV_CACHES);
		if (ret) {
			pr_err("cache operation failed %d\n", ret);
			goto loadapp_err;
		}

		/*  SCM_CALL  to load the app and get the app_id back */
		ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, cmd_buf,
			cmd_len, &resp, sizeof(resp));
		if (ret) {
			pr_err("scm_call to load app failed\n");
			if (!IS_ERR_OR_NULL(ihandle))
				ion_free(qseecom.ion_clnt, ihandle);
			ret = -EINVAL;
			goto loadapp_err;
		}

		if (resp.result == QSEOS_RESULT_FAILURE) {
			pr_err("scm_call rsp.result is QSEOS_RESULT_FAILURE\n");
			if (!IS_ERR_OR_NULL(ihandle))
				ion_free(qseecom.ion_clnt, ihandle);
			ret = -EFAULT;
			goto loadapp_err;
		}

		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			ret = __qseecom_process_incomplete_cmd(data, &resp);
			if (ret) {
				pr_err("process_incomplete_cmd failed err: %d\n",
					ret);
				if (!IS_ERR_OR_NULL(ihandle))
					ion_free(qseecom.ion_clnt, ihandle);
				ret = -EFAULT;
				goto loadapp_err;
			}
		}

		if (resp.result != QSEOS_RESULT_SUCCESS) {
			pr_err("scm_call failed resp.result unknown, %d\n",
				resp.result);
			if (!IS_ERR_OR_NULL(ihandle))
				ion_free(qseecom.ion_clnt, ihandle);
			ret = -EFAULT;
			goto loadapp_err;
		}

		app_id = resp.data;

		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			pr_err("kmalloc failed\n");
			ret = -ENOMEM;
			goto loadapp_err;
		}
		entry->app_id = app_id;
		entry->ref_cnt = 1;
		entry->app_arch = load_img_req.app_arch;
		/*
		* keymaster app may be first loaded as "keymaste" by qseecomd,
		* and then used as "keymaster" on some targets. To avoid app
		* name checking error, register "keymaster" into app_list and
		* thread private data.
		*/
		if (!strcmp(load_img_req.img_name, "keymaste"))
			strlcpy(entry->app_name, "keymaster",
					MAX_APP_NAME_SIZE);
		else
			strlcpy(entry->app_name, load_img_req.img_name,
					MAX_APP_NAME_SIZE);
		entry->app_blocked = false;
		entry->blocked_on_listener_id = 0;

		/* Deallocate the handle */
		if (!IS_ERR_OR_NULL(ihandle))
			ion_free(qseecom.ion_clnt, ihandle);

		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_add_tail(&entry->list, &qseecom.registered_app_list_head);
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
									flags);

		pr_warn("App with id %d (%s) now loaded\n", app_id,
		(char *)(load_img_req.img_name));
	}
	data->client.app_id = app_id;
	data->client.app_arch = load_img_req.app_arch;
	if (!strcmp(load_img_req.img_name, "keymaste"))
		strlcpy(data->client.app_name, "keymaster", MAX_APP_NAME_SIZE);
	else
		strlcpy(data->client.app_name, load_img_req.img_name,
					MAX_APP_NAME_SIZE);
	load_img_req.app_id = app_id;
	if (copy_to_user(argp, &load_img_req, sizeof(load_img_req))) {
		pr_err("copy_to_user failed\n");
		ret = -EFAULT;
		if (first_time == true) {
			spin_lock_irqsave(
				&qseecom.registered_app_list_lock, flags);
			list_del(&entry->list);
			spin_unlock_irqrestore(
				&qseecom.registered_app_list_lock, flags);
			kzfree(entry);
		}
	}

loadapp_err:
	__qseecom_disable_clk_scale_down(data);
enable_clk_err:
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}
	return ret;
}

static int __qseecom_cleanup_app(struct qseecom_dev_handle *data)
{
	int ret = 1;	/* Set unload app */
	wake_up_all(&qseecom.send_resp_wq);
	if (qseecom.qsee_reentrancy_support)
		mutex_unlock(&app_access_lock);
	while (atomic_read(&data->ioctl_count) > 1) {
		if (wait_event_freezable(data->abort_wq,
					atomic_read(&data->ioctl_count) <= 1)) {
			pr_err("Interrupted from abort\n");
			ret = -ERESTARTSYS;
			break;
		}
	}
	if (qseecom.qsee_reentrancy_support)
		mutex_lock(&app_access_lock);
	return ret;
}

static int qseecom_unmap_ion_allocated_memory(struct qseecom_dev_handle *data)
{
	int ret = 0;
	if (!IS_ERR_OR_NULL(data->client.ihandle)) {
		ion_unmap_kernel(qseecom.ion_clnt, data->client.ihandle);
		ion_free(qseecom.ion_clnt, data->client.ihandle);
		data->client.ihandle = NULL;
	}
	return ret;
}

static int qseecom_unload_app(struct qseecom_dev_handle *data,
				bool app_crash)
{
	unsigned long flags;
	unsigned long flags1;
	int ret = 0;
	struct qseecom_command_scm_resp resp;
	struct qseecom_registered_app_list *ptr_app = NULL;
	bool unload = false;
	bool found_app = false;
	bool found_dead_app = false;

	if (!data) {
		pr_err("Invalid/uninitialized device handle\n");
		return -EINVAL;
	}

	if (!memcmp(data->client.app_name, "keymaste", strlen("keymaste"))) {
		pr_debug("Do not unload keymaster app from tz\n");
		goto unload_exit;
	}

	__qseecom_cleanup_app(data);
	__qseecom_reentrancy_check_if_no_app_blocked(TZ_OS_APP_SHUTDOWN_ID);

	if (data->client.app_id > 0) {
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(ptr_app, &qseecom.registered_app_list_head,
									list) {
			if (ptr_app->app_id == data->client.app_id) {
				if (!strcmp((void *)ptr_app->app_name,
					(void *)data->client.app_name)) {
					found_app = true;
					if (app_crash || ptr_app->ref_cnt == 1)
						unload = true;
					break;
				} else {
					found_dead_app = true;
					break;
				}
			}
		}
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
								flags);
		if (found_app == false && found_dead_app == false) {
			pr_err("Cannot find app with id = %d (%s)\n",
				data->client.app_id,
				(char *)data->client.app_name);
			ret = -EINVAL;
			goto unload_exit;
		}
	}

	if (found_dead_app)
		pr_warn("cleanup app_id %d(%s)\n", data->client.app_id,
			(char *)data->client.app_name);

	if (unload) {
		struct qseecom_unload_app_ireq req;
		/* Populate the structure for sending scm call to load image */
		req.qsee_cmd_id = QSEOS_APP_SHUTDOWN_COMMAND;
		req.app_id = data->client.app_id;

		/* SCM_CALL to unload the app */
		ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, &req,
				sizeof(struct qseecom_unload_app_ireq),
				&resp, sizeof(resp));
		if (ret) {
			pr_err("scm_call to unload app (id = %d) failed\n",
								req.app_id);
			ret = -EFAULT;
			goto unload_exit;
		} else {
			pr_warn("App id %d now unloaded\n", req.app_id);
		}
		if (resp.result == QSEOS_RESULT_FAILURE) {
			pr_err("app (%d) unload_failed!!\n",
					data->client.app_id);
			ret = -EFAULT;
			goto unload_exit;
		}
		if (resp.result == QSEOS_RESULT_SUCCESS)
			pr_debug("App (%d) is unloaded!!\n",
					data->client.app_id);
		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			ret = __qseecom_process_incomplete_cmd(data, &resp);
			if (ret) {
				pr_err("process_incomplete_cmd fail err: %d\n",
									ret);
				goto unload_exit;
			}
		}
	}

	if (found_app) {
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags1);
		if (app_crash) {
			ptr_app->ref_cnt = 0;
			pr_debug("app_crash: ref_count = 0\n");
		} else {
			if (ptr_app->ref_cnt == 1) {
				ptr_app->ref_cnt = 0;
				pr_debug("ref_count set to 0\n");
			} else {
				ptr_app->ref_cnt--;
				pr_debug("Can't unload app(%d) inuse\n",
					ptr_app->app_id);
			}
		}
		if (unload) {
			list_del(&ptr_app->list);
			kzfree(ptr_app);
		}
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
								flags1);
	}
unload_exit:
	qseecom_unmap_ion_allocated_memory(data);
	data->released = true;
	return ret;
}

static phys_addr_t __qseecom_uvirt_to_kphys(struct qseecom_dev_handle *data,
						unsigned long virt)
{
	return data->client.sb_phys + (virt - data->client.user_virt_sb_base);
}

static uintptr_t __qseecom_uvirt_to_kvirt(struct qseecom_dev_handle *data,
						unsigned long virt)
{
	return (uintptr_t)data->client.sb_virt +
				(virt - data->client.user_virt_sb_base);
}

int __qseecom_process_rpmb_svc_cmd(struct qseecom_dev_handle *data_ptr,
		struct qseecom_send_svc_cmd_req *req_ptr,
		struct qseecom_client_send_service_ireq *send_svc_ireq_ptr)
{
	int ret = 0;
	void *req_buf = NULL;

	if ((req_ptr == NULL) || (send_svc_ireq_ptr == NULL)) {
		pr_err("Error with pointer: req_ptr = %pK, send_svc_ptr = %pK\n",
			req_ptr, send_svc_ireq_ptr);
		return -EINVAL;
	}

	/* Clients need to ensure req_buf is at base offset of shared buffer */
	if ((uintptr_t)req_ptr->cmd_req_buf !=
			data_ptr->client.user_virt_sb_base) {
		pr_err("cmd buf not pointing to base offset of shared buffer\n");
		return -EINVAL;
	}

	if (data_ptr->client.sb_length <
			sizeof(struct qseecom_rpmb_provision_key)) {
		pr_err("shared buffer is too small to hold key type\n");
		return -EINVAL;
	}
	req_buf = data_ptr->client.sb_virt;

	send_svc_ireq_ptr->qsee_cmd_id = req_ptr->cmd_id;
	send_svc_ireq_ptr->key_type =
		((struct qseecom_rpmb_provision_key *)req_buf)->key_type;
	send_svc_ireq_ptr->req_len = req_ptr->cmd_req_len;
	send_svc_ireq_ptr->rsp_ptr = (uint32_t)(__qseecom_uvirt_to_kphys(
			data_ptr, (uintptr_t)req_ptr->resp_buf));
	send_svc_ireq_ptr->rsp_len = req_ptr->resp_len;

	return ret;
}

int __qseecom_process_fsm_key_svc_cmd(struct qseecom_dev_handle *data_ptr,
		struct qseecom_send_svc_cmd_req *req_ptr,
		struct qseecom_client_send_fsm_key_req *send_svc_ireq_ptr)
{
	int ret = 0;
	uint32_t reqd_len_sb_in = 0;

	if ((req_ptr == NULL) || (send_svc_ireq_ptr == NULL)) {
		pr_err("Error with pointer: req_ptr = %pK, send_svc_ptr = %pK\n",
			req_ptr, send_svc_ireq_ptr);
		return -EINVAL;
	}

	reqd_len_sb_in = req_ptr->cmd_req_len + req_ptr->resp_len;
	if (reqd_len_sb_in > data_ptr->client.sb_length) {
		pr_err("Not enough memory to fit cmd_buf and resp_buf. ");
		pr_err("Required: %u, Available: %zu\n",
				reqd_len_sb_in, data_ptr->client.sb_length);
		return -ENOMEM;
	}

	send_svc_ireq_ptr->qsee_cmd_id = req_ptr->cmd_id;
	send_svc_ireq_ptr->req_len = req_ptr->cmd_req_len;
	send_svc_ireq_ptr->rsp_ptr = (uint32_t)(__qseecom_uvirt_to_kphys(
			data_ptr, (uintptr_t)req_ptr->resp_buf));
	send_svc_ireq_ptr->rsp_len = req_ptr->resp_len;

	send_svc_ireq_ptr->req_ptr = (uint32_t)(__qseecom_uvirt_to_kphys(
			data_ptr, (uintptr_t)req_ptr->cmd_req_buf));


	return ret;
}

static int __validate_send_service_cmd_inputs(struct qseecom_dev_handle *data,
				struct qseecom_send_svc_cmd_req *req)
{
	if (!req || !req->resp_buf || !req->cmd_req_buf) {
		pr_err("req or cmd buffer or response buffer is null\n");
		return -EINVAL;
	}

	if (!data || !data->client.ihandle) {
		pr_err("Client or client handle is not initialized\n");
		return -EINVAL;
	}

	if (data->client.sb_virt == NULL) {
		pr_err("sb_virt null\n");
		return -EINVAL;
	}

	if (data->client.user_virt_sb_base == 0) {
		pr_err("user_virt_sb_base is null\n");
		return -EINVAL;
	}

	if (data->client.sb_length == 0) {
		pr_err("sb_length is 0\n");
		return -EINVAL;
	}

	if (((uintptr_t)req->cmd_req_buf <
				data->client.user_virt_sb_base) ||
		((uintptr_t)req->cmd_req_buf >=
		(data->client.user_virt_sb_base + data->client.sb_length))) {
		pr_err("cmd buffer address not within shared bufffer\n");
		return -EINVAL;
	}
	if (((uintptr_t)req->resp_buf <
				data->client.user_virt_sb_base)  ||
		((uintptr_t)req->resp_buf >=
		(data->client.user_virt_sb_base + data->client.sb_length))) {
		pr_err("response buffer address not within shared bufffer\n");
		return -EINVAL;
	}
	if ((req->cmd_req_len == 0) || (req->resp_len == 0) ||
		(req->cmd_req_len > data->client.sb_length) ||
		(req->resp_len > data->client.sb_length)) {
		pr_err("cmd buf length or response buf length not valid\n");
		return -EINVAL;
	}
	if (req->cmd_req_len > UINT_MAX - req->resp_len) {
		pr_err("Integer overflow detected in req_len & rsp_len\n");
		return -EINVAL;
	}

	if ((req->cmd_req_len + req->resp_len) > data->client.sb_length) {
		pr_debug("Not enough memory to fit cmd_buf.\n");
		pr_debug("resp_buf. Required: %u, Available: %zu\n",
				(req->cmd_req_len + req->resp_len),
					data->client.sb_length);
		return -ENOMEM;
	}
	if ((uintptr_t)req->cmd_req_buf > (ULONG_MAX - req->cmd_req_len)) {
		pr_err("Integer overflow in req_len & cmd_req_buf\n");
		return -EINVAL;
	}
	if ((uintptr_t)req->resp_buf > (ULONG_MAX - req->resp_len)) {
		pr_err("Integer overflow in resp_len & resp_buf\n");
		return -EINVAL;
	}
	if (data->client.user_virt_sb_base >
					(ULONG_MAX - data->client.sb_length)) {
		pr_err("Integer overflow in user_virt_sb_base & sb_length\n");
		return -EINVAL;
	}
	if ((((uintptr_t)req->cmd_req_buf + req->cmd_req_len) >
		((uintptr_t)data->client.user_virt_sb_base +
					data->client.sb_length)) ||
		(((uintptr_t)req->resp_buf + req->resp_len) >
		((uintptr_t)data->client.user_virt_sb_base +
					data->client.sb_length))) {
		pr_err("cmd buf or resp buf is out of shared buffer region\n");
		return -EINVAL;
	}
	return 0;
}

static int qseecom_send_service_cmd(struct qseecom_dev_handle *data,
				void __user *argp)
{
	int ret = 0;
	struct qseecom_client_send_service_ireq send_svc_ireq;
	struct qseecom_client_send_fsm_key_req send_fsm_key_svc_ireq;
	struct qseecom_command_scm_resp resp;
	struct qseecom_send_svc_cmd_req req;
	void   *send_req_ptr;
	size_t req_buf_size;

	/*struct qseecom_command_scm_resp resp;*/

	if (copy_from_user(&req,
				(void __user *)argp,
				sizeof(req))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	if (__validate_send_service_cmd_inputs(data, &req))
		return -EINVAL;

	data->type = QSEECOM_SECURE_SERVICE;

	switch (req.cmd_id) {
	case QSEOS_RPMB_PROVISION_KEY_COMMAND:
	case QSEOS_RPMB_ERASE_COMMAND:
	case QSEOS_RPMB_CHECK_PROV_STATUS_COMMAND:
		send_req_ptr = &send_svc_ireq;
		req_buf_size = sizeof(send_svc_ireq);
		if (__qseecom_process_rpmb_svc_cmd(data, &req,
				send_req_ptr))
			return -EINVAL;
		break;
	case QSEOS_FSM_LTEOTA_REQ_CMD:
	case QSEOS_FSM_LTEOTA_REQ_RSP_CMD:
	case QSEOS_FSM_IKE_REQ_CMD:
	case QSEOS_FSM_IKE_REQ_RSP_CMD:
	case QSEOS_FSM_OEM_FUSE_WRITE_ROW:
	case QSEOS_FSM_OEM_FUSE_READ_ROW:
	case QSEOS_FSM_ENCFS_REQ_CMD:
	case QSEOS_FSM_ENCFS_REQ_RSP_CMD:
		send_req_ptr = &send_fsm_key_svc_ireq;
		req_buf_size = sizeof(send_fsm_key_svc_ireq);
		if (__qseecom_process_fsm_key_svc_cmd(data, &req,
				send_req_ptr))
			return -EINVAL;
		break;
	default:
		pr_err("Unsupported cmd_id %d\n", req.cmd_id);
		return -EINVAL;
	}

	if (qseecom.support_bus_scaling) {
		ret = qseecom_scale_bus_bandwidth_timer(HIGH);
		if (ret) {
			pr_err("Fail to set bw HIGH\n");
			return ret;
		}
	} else {
		ret = qseecom_perf_enable(data);
		if (ret) {
			pr_err("Failed to vote for clocks with err %d\n", ret);
			goto exit;
		}
	}

	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_CLEAN_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		goto exit;
	}
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
				(const void *)send_req_ptr,
				req_buf_size, &resp, sizeof(resp));
	if (ret) {
		pr_err("qseecom_scm_call failed with err: %d\n", ret);
		if (!qseecom.support_bus_scaling) {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		} else {
			__qseecom_add_bw_scale_down_timer(
				QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
		}
		goto exit;
	}
	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		goto exit;
	}
	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		pr_debug("qseos_result_incomplete\n");
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd fail with result: %d\n",
				resp.result);
		}
		if (req.cmd_id == QSEOS_RPMB_CHECK_PROV_STATUS_COMMAND) {
			pr_warn("RPMB key status is 0x%x\n", resp.result);
			if (put_user(resp.result,
				(uint32_t __user *)req.resp_buf)) {
				ret = -EINVAL;
				goto exit;
			}
			ret = 0;
		}
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("scm call failed with resp.result: %d\n", resp.result);
		ret = -EINVAL;
		break;
	default:
		pr_err("Response result %d not supported\n",
				resp.result);
		ret = -EINVAL;
		break;
	}
	if (!qseecom.support_bus_scaling) {
		qsee_disable_clock_vote(data, CLK_DFAB);
		qsee_disable_clock_vote(data, CLK_SFPB);
	} else {
		__qseecom_add_bw_scale_down_timer(
			QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
	}

exit:
	return ret;
}

static int __validate_send_cmd_inputs(struct qseecom_dev_handle *data,
				struct qseecom_send_cmd_req *req)

{
	if (!data || !data->client.ihandle) {
		pr_err("Client or client handle is not initialized\n");
		return -EINVAL;
	}
	if (((req->resp_buf == NULL) && (req->resp_len != 0)) ||
						(req->cmd_req_buf == NULL)) {
		pr_err("cmd buffer or response buffer is null\n");
		return -EINVAL;
	}
	if (((uintptr_t)req->cmd_req_buf <
				data->client.user_virt_sb_base) ||
		((uintptr_t)req->cmd_req_buf >=
		(data->client.user_virt_sb_base + data->client.sb_length))) {
		pr_err("cmd buffer address not within shared bufffer\n");
		return -EINVAL;
	}
	if (((uintptr_t)req->resp_buf <
				data->client.user_virt_sb_base)  ||
		((uintptr_t)req->resp_buf >=
		(data->client.user_virt_sb_base + data->client.sb_length))) {
		pr_err("response buffer address not within shared bufffer\n");
		return -EINVAL;
	}
	if ((req->cmd_req_len == 0) ||
		(req->cmd_req_len > data->client.sb_length) ||
		(req->resp_len > data->client.sb_length)) {
		pr_err("cmd buf length or response buf length not valid\n");
		return -EINVAL;
	}
	if (req->cmd_req_len > UINT_MAX - req->resp_len) {
		pr_err("Integer overflow detected in req_len & rsp_len\n");
		return -EINVAL;
	}

	if ((req->cmd_req_len + req->resp_len) > data->client.sb_length) {
		pr_debug("Not enough memory to fit cmd_buf.\n");
		pr_debug("resp_buf. Required: %u, Available: %zu\n",
				(req->cmd_req_len + req->resp_len),
					data->client.sb_length);
		return -ENOMEM;
	}
	if ((uintptr_t)req->cmd_req_buf > (ULONG_MAX - req->cmd_req_len)) {
		pr_err("Integer overflow in req_len & cmd_req_buf\n");
		return -EINVAL;
	}
	if ((uintptr_t)req->resp_buf > (ULONG_MAX - req->resp_len)) {
		pr_err("Integer overflow in resp_len & resp_buf\n");
		return -EINVAL;
	}
	if (data->client.user_virt_sb_base >
					(ULONG_MAX - data->client.sb_length)) {
		pr_err("Integer overflow in user_virt_sb_base & sb_length\n");
		return -EINVAL;
	}
	if ((((uintptr_t)req->cmd_req_buf + req->cmd_req_len) >
		((uintptr_t)data->client.user_virt_sb_base +
						data->client.sb_length)) ||
		(((uintptr_t)req->resp_buf + req->resp_len) >
		((uintptr_t)data->client.user_virt_sb_base +
						data->client.sb_length))) {
		pr_err("cmd buf or resp buf is out of shared buffer region\n");
		return -EINVAL;
	}
	return 0;
}

int __qseecom_process_reentrancy(struct qseecom_command_scm_resp *resp,
				struct qseecom_registered_app_list *ptr_app,
				struct qseecom_dev_handle *data)
{
	int ret = 0;

	switch (resp->result) {
	case QSEOS_RESULT_BLOCKED_ON_LISTENER:
		pr_warn("App(%d) %s is blocked on listener %d\n",
			data->client.app_id, data->client.app_name,
			resp->data);
		ret = __qseecom_process_reentrancy_blocked_on_listener(
					resp, ptr_app, data);
		if (ret) {
			pr_err("failed to process App(%d) %s is blocked on listener %d\n",
			data->client.app_id, data->client.app_name, resp->data);
			return ret;
		}

	case QSEOS_RESULT_INCOMPLETE:
		qseecom.app_block_ref_cnt++;
		ptr_app->app_blocked = true;
		ret = __qseecom_reentrancy_process_incomplete_cmd(data, resp);
		ptr_app->app_blocked = false;
		qseecom.app_block_ref_cnt--;
		wake_up_interruptible(&qseecom.app_block_wq);
		if (ret)
			pr_err("process_incomplete_cmd failed err: %d\n",
					ret);
		return ret;
	case QSEOS_RESULT_SUCCESS:
		return ret;
	default:
		pr_err("Response result %d not supported\n",
						resp->result);
		return -EINVAL;
	}
}

static int __qseecom_send_cmd(struct qseecom_dev_handle *data,
				struct qseecom_send_cmd_req *req)
{
	int ret = 0;
	u32 reqd_len_sb_in = 0;
	struct qseecom_client_send_data_ireq send_data_req = {0};
	struct qseecom_client_send_data_64bit_ireq send_data_req_64bit = {0};
	struct qseecom_command_scm_resp resp;
	unsigned long flags;
	struct qseecom_registered_app_list *ptr_app;
	bool found_app = false;
	void *cmd_buf = NULL;
	size_t cmd_len;
	struct sglist_info *table = data->sglistinfo_ptr;

	reqd_len_sb_in = req->cmd_req_len + req->resp_len;
	/* find app_id & img_name from list */
	spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
	list_for_each_entry(ptr_app, &qseecom.registered_app_list_head,
							list) {
		if ((ptr_app->app_id == data->client.app_id) &&
			 (!strcmp(ptr_app->app_name, data->client.app_name))) {
			found_app = true;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_app_list_lock, flags);

	if (!found_app) {
		pr_err("app_id %d (%s) is not found\n", data->client.app_id,
			(char *)data->client.app_name);
		return -ENOENT;
	}

	if (qseecom.qsee_version < QSEE_VERSION_40) {
		send_data_req.app_id = data->client.app_id;
		send_data_req.req_ptr = (uint32_t)(__qseecom_uvirt_to_kphys(
					data, (uintptr_t)req->cmd_req_buf));
		send_data_req.req_len = req->cmd_req_len;
		send_data_req.rsp_ptr = (uint32_t)(__qseecom_uvirt_to_kphys(
					data, (uintptr_t)req->resp_buf));
		send_data_req.rsp_len = req->resp_len;
		send_data_req.sglistinfo_ptr =
				(uint32_t)virt_to_phys(table);
		send_data_req.sglistinfo_len = SGLISTINFO_TABLE_SIZE;
		dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
		cmd_buf = (void *)&send_data_req;
		cmd_len = sizeof(struct qseecom_client_send_data_ireq);
	} else {
		send_data_req_64bit.app_id = data->client.app_id;
		send_data_req_64bit.req_ptr = __qseecom_uvirt_to_kphys(data,
					(uintptr_t)req->cmd_req_buf);
		send_data_req_64bit.req_len = req->cmd_req_len;
		send_data_req_64bit.rsp_ptr = __qseecom_uvirt_to_kphys(data,
					(uintptr_t)req->resp_buf);
		send_data_req_64bit.rsp_len = req->resp_len;
		/* check if 32bit app's phys_addr region is under 4GB.*/
		if ((data->client.app_arch == ELFCLASS32) &&
			((send_data_req_64bit.req_ptr >=
				PHY_ADDR_4G - send_data_req_64bit.req_len) ||
			(send_data_req_64bit.rsp_ptr >=
				PHY_ADDR_4G - send_data_req_64bit.rsp_len))){
			pr_err("32bit app %s PA exceeds 4G: req_ptr=%llx, req_len=%x, rsp_ptr=%llx, rsp_len=%x\n",
				data->client.app_name,
				send_data_req_64bit.req_ptr,
				send_data_req_64bit.req_len,
				send_data_req_64bit.rsp_ptr,
				send_data_req_64bit.rsp_len);
			return -EFAULT;
		}
		send_data_req_64bit.sglistinfo_ptr =
				(uint64_t)virt_to_phys(table);
		send_data_req_64bit.sglistinfo_len = SGLISTINFO_TABLE_SIZE;
		dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
		cmd_buf = (void *)&send_data_req_64bit;
		cmd_len = sizeof(struct qseecom_client_send_data_64bit_ireq);
	}

	if (qseecom.whitelist_support == false || data->use_legacy_cmd == true)
		*(uint32_t *)cmd_buf = QSEOS_CLIENT_SEND_DATA_COMMAND;
	else
		*(uint32_t *)cmd_buf = QSEOS_CLIENT_SEND_DATA_COMMAND_WHITELIST;

	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
					data->client.sb_virt,
					reqd_len_sb_in,
					ION_IOC_CLEAN_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		return ret;
	}

	__qseecom_reentrancy_check_if_this_app_blocked(ptr_app);

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
				cmd_buf, cmd_len,
				&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call() failed with err: %d (app_id = %d)\n",
					ret, data->client.app_id);
		return ret;
	}

	if (qseecom.qsee_reentrancy_support) {
		ret = __qseecom_process_reentrancy(&resp, ptr_app, data);
	} else {
		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			ret = __qseecom_process_incomplete_cmd(data, &resp);
			if (ret) {
				pr_err("process_incomplete_cmd failed err: %d\n",
						ret);
				return ret;
			}
		} else {
			if (resp.result != QSEOS_RESULT_SUCCESS) {
				pr_err("Response result %d not supported\n",
								resp.result);
				ret = -EINVAL;
			}
		}
	}
	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_INV_CACHES);
	if (ret)
		pr_err("cache operation failed %d\n", ret);
	return ret;
}

static int qseecom_send_cmd(struct qseecom_dev_handle *data, void __user *argp)
{
	int ret = 0;
	struct qseecom_send_cmd_req req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (__validate_send_cmd_inputs(data, &req))
		return -EINVAL;

	ret = __qseecom_send_cmd(data, &req);

	if (ret)
		return ret;

	return ret;
}

int __boundary_checks_offset(struct qseecom_send_modfd_cmd_req *req,
			struct qseecom_send_modfd_listener_resp *lstnr_resp,
			struct qseecom_dev_handle *data, int i) {

	if ((data->type != QSEECOM_LISTENER_SERVICE) &&
						(req->ifd_data[i].fd > 0)) {
			if ((req->cmd_req_len < sizeof(uint32_t)) ||
				(req->ifd_data[i].cmd_buf_offset >
				req->cmd_req_len - sizeof(uint32_t))) {
				pr_err("Invalid offset (req len) 0x%x\n",
					req->ifd_data[i].cmd_buf_offset);
				return -EINVAL;
			}
	} else if ((data->type == QSEECOM_LISTENER_SERVICE) &&
					(lstnr_resp->ifd_data[i].fd > 0)) {
			if ((lstnr_resp->resp_len < sizeof(uint32_t)) ||
				(lstnr_resp->ifd_data[i].cmd_buf_offset >
				lstnr_resp->resp_len - sizeof(uint32_t))) {
				pr_err("Invalid offset (lstnr resp len) 0x%x\n",
					lstnr_resp->ifd_data[i].cmd_buf_offset);
				return -EINVAL;
			}
		}
	return 0;
}

static int __qseecom_update_cmd_buf(void *msg, bool cleanup,
			struct qseecom_dev_handle *data)
{
	struct ion_handle *ihandle;
	char *field;
	int ret = 0;
	int i = 0;
	uint32_t len = 0;
	struct scatterlist *sg;
	struct qseecom_send_modfd_cmd_req *req = NULL;
	struct qseecom_send_modfd_listener_resp *lstnr_resp = NULL;
	struct qseecom_registered_listener_list *this_lstnr = NULL;
	uint32_t offset;
	struct sg_table *sg_ptr;

	if ((data->type != QSEECOM_LISTENER_SERVICE) &&
			(data->type != QSEECOM_CLIENT_APP))
		return -EFAULT;

	if (msg == NULL) {
		pr_err("Invalid address\n");
		return -EINVAL;
	}
	if (data->type == QSEECOM_LISTENER_SERVICE) {
		lstnr_resp = (struct qseecom_send_modfd_listener_resp *)msg;
		this_lstnr = __qseecom_find_svc(data->listener.id);
		if (IS_ERR_OR_NULL(this_lstnr)) {
			pr_err("Invalid listener ID\n");
			return -ENOMEM;
		}
	} else {
		req = (struct qseecom_send_modfd_cmd_req *)msg;
	}

	for (i = 0; i < MAX_ION_FD; i++) {
		if ((data->type != QSEECOM_LISTENER_SERVICE) &&
						(req->ifd_data[i].fd > 0)) {
			ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					req->ifd_data[i].fd);
			if (IS_ERR_OR_NULL(ihandle)) {
				pr_err("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			field = (char *) req->cmd_req_buf +
				req->ifd_data[i].cmd_buf_offset;
		} else if ((data->type == QSEECOM_LISTENER_SERVICE) &&
				(lstnr_resp->ifd_data[i].fd > 0)) {
			ihandle = ion_import_dma_buf(qseecom.ion_clnt,
						lstnr_resp->ifd_data[i].fd);
			if (IS_ERR_OR_NULL(ihandle)) {
				pr_err("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			field = lstnr_resp->resp_buf_ptr +
				lstnr_resp->ifd_data[i].cmd_buf_offset;
		} else {
			continue;
		}
		/* Populate the cmd data structure with the phys_addr */
		sg_ptr = ion_sg_table(qseecom.ion_clnt, ihandle);
		if (IS_ERR_OR_NULL(sg_ptr)) {
			pr_err("IOn client could not retrieve sg table\n");
			goto err;
		}
		if (sg_ptr->nents == 0) {
			pr_err("Num of scattered entries is 0\n");
			goto err;
		}
		if (sg_ptr->nents > QSEECOM_MAX_SG_ENTRY) {
			pr_err("Num of scattered entries");
			pr_err(" (%d) is greater than max supported %d\n",
				sg_ptr->nents, QSEECOM_MAX_SG_ENTRY);
			goto err;
		}
		sg = sg_ptr->sgl;
		if (sg_ptr->nents == 1) {
			uint32_t *update;
			if (__boundary_checks_offset(req, lstnr_resp, data, i))
				goto err;
			if ((data->type == QSEECOM_CLIENT_APP &&
				(data->client.app_arch == ELFCLASS32 ||
				data->client.app_arch == ELFCLASS64)) ||
				(data->type == QSEECOM_LISTENER_SERVICE)) {
				/*
				 * Check if sg list phy add region is under 4GB
				 */
				if ((qseecom.qsee_version >= QSEE_VERSION_40) &&
					(!cleanup) &&
					((uint64_t)sg_dma_address(sg_ptr->sgl)
					>= PHY_ADDR_4G - sg->length)) {
					pr_err("App %s sgl PA exceeds 4G: phy_addr=%pKad, len=%x\n",
						data->client.app_name,
						&(sg_dma_address(sg_ptr->sgl)),
						sg->length);
					goto err;
				}
				update = (uint32_t *) field;
				*update = cleanup ? 0 :
					(uint32_t)sg_dma_address(sg_ptr->sgl);
			} else {
				pr_err("QSEE app arch %u is not supported\n",
							data->client.app_arch);
				goto err;
			}
			len += (uint32_t)sg->length;
		} else {
			struct qseecom_sg_entry *update;
			int j = 0;

			if ((data->type != QSEECOM_LISTENER_SERVICE) &&
					(req->ifd_data[i].fd > 0)) {

				if ((req->cmd_req_len <
					 SG_ENTRY_SZ * sg_ptr->nents) ||
					(req->ifd_data[i].cmd_buf_offset >
						(req->cmd_req_len -
						SG_ENTRY_SZ * sg_ptr->nents))) {
					pr_err("Invalid offset = 0x%x\n",
					req->ifd_data[i].cmd_buf_offset);
					goto err;
				}

			} else if ((data->type == QSEECOM_LISTENER_SERVICE) &&
					(lstnr_resp->ifd_data[i].fd > 0)) {

				if ((lstnr_resp->resp_len <
						SG_ENTRY_SZ * sg_ptr->nents) ||
				(lstnr_resp->ifd_data[i].cmd_buf_offset >
						(lstnr_resp->resp_len -
						SG_ENTRY_SZ * sg_ptr->nents))) {
					goto err;
				}
			}
			if ((data->type == QSEECOM_CLIENT_APP &&
				(data->client.app_arch == ELFCLASS32 ||
				data->client.app_arch == ELFCLASS64)) ||
				(data->type == QSEECOM_LISTENER_SERVICE)) {
				update = (struct qseecom_sg_entry *)field;
				for (j = 0; j < sg_ptr->nents; j++) {
					/*
					* Check if sg list PA is under 4GB
					*/
					if ((qseecom.qsee_version >=
						QSEE_VERSION_40) &&
						(!cleanup) &&
						((uint64_t)(sg_dma_address(sg))
						>= PHY_ADDR_4G - sg->length)) {
						pr_err("App %s sgl PA exceeds 4G: phy_addr=%pKad, len=%x\n",
							data->client.app_name,
							&(sg_dma_address(sg)),
							sg->length);
						goto err;
					}
					update->phys_addr = cleanup ? 0 :
						(uint32_t)sg_dma_address(sg);
					update->len = cleanup ? 0 : sg->length;
					update++;
					len += sg->length;
					sg = sg_next(sg);
				}
			} else {
				pr_err("QSEE app arch %u is not supported\n",
							data->client.app_arch);
					goto err;
			}
		}

		if (cleanup) {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
					ihandle, NULL, len,
					ION_IOC_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				goto err;
			}
		} else {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
					ihandle, NULL, len,
					ION_IOC_CLEAN_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				goto err;
			}
			if (data->type == QSEECOM_CLIENT_APP) {
				offset = req->ifd_data[i].cmd_buf_offset;
				data->sglistinfo_ptr[i].indexAndFlags =
					SGLISTINFO_SET_INDEX_FLAG(
					(sg_ptr->nents == 1), 0, offset);
				data->sglistinfo_ptr[i].sizeOrCount =
					(sg_ptr->nents == 1) ?
					sg->length : sg_ptr->nents;
				data->sglist_cnt = i + 1;
			} else {
				offset = (lstnr_resp->ifd_data[i].cmd_buf_offset
					+ (uintptr_t)lstnr_resp->resp_buf_ptr -
					(uintptr_t)this_lstnr->sb_virt);
				this_lstnr->sglistinfo_ptr[i].indexAndFlags =
					SGLISTINFO_SET_INDEX_FLAG(
					(sg_ptr->nents == 1), 0, offset);
				this_lstnr->sglistinfo_ptr[i].sizeOrCount =
					(sg_ptr->nents == 1) ?
					sg->length : sg_ptr->nents;
				this_lstnr->sglist_cnt = i + 1;
			}
		}
		/* Deallocate the handle */
		if (!IS_ERR_OR_NULL(ihandle))
			ion_free(qseecom.ion_clnt, ihandle);
	}
	return ret;
err:
	if (!IS_ERR_OR_NULL(ihandle))
		ion_free(qseecom.ion_clnt, ihandle);
	return -ENOMEM;
}

static int __qseecom_allocate_sg_list_buffer(struct qseecom_dev_handle *data,
		char *field, uint32_t fd_idx, struct sg_table *sg_ptr)
{
	struct scatterlist *sg = sg_ptr->sgl;
	struct qseecom_sg_entry_64bit *sg_entry;
	struct qseecom_sg_list_buf_hdr_64bit *buf_hdr;
	void *buf;
	uint i;
	size_t size;
	dma_addr_t coh_pmem;

	if (fd_idx >= MAX_ION_FD) {
		pr_err("fd_idx [%d] is invalid\n", fd_idx);
		return -ENOMEM;
	}
	buf_hdr = (struct qseecom_sg_list_buf_hdr_64bit *)field;
	memset((void *)buf_hdr, 0, QSEECOM_SG_LIST_BUF_HDR_SZ_64BIT);
	/* Allocate a contiguous kernel buffer */
	size = sg_ptr->nents * SG_ENTRY_SZ_64BIT;
	size = (size + PAGE_SIZE) & PAGE_MASK;
	buf = dma_alloc_coherent(qseecom.pdev,
			size, &coh_pmem, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("failed to alloc memory for sg buf\n");
		return -ENOMEM;
	}
	/* update qseecom_sg_list_buf_hdr_64bit */
	buf_hdr->version = QSEECOM_SG_LIST_BUF_FORMAT_VERSION_2;
	buf_hdr->new_buf_phys_addr = coh_pmem;
	buf_hdr->nents_total = sg_ptr->nents;
	/* save the left sg entries into new allocated buf */
	sg_entry = (struct qseecom_sg_entry_64bit *)buf;
	for (i = 0; i < sg_ptr->nents; i++) {
		sg_entry->phys_addr = (uint64_t)sg_dma_address(sg);
		sg_entry->len = sg->length;
		sg_entry++;
		sg = sg_next(sg);
	}

	data->client.sec_buf_fd[fd_idx].is_sec_buf_fd = true;
	data->client.sec_buf_fd[fd_idx].vbase = buf;
	data->client.sec_buf_fd[fd_idx].pbase = coh_pmem;
	data->client.sec_buf_fd[fd_idx].size = size;

	return 0;
}

static int __qseecom_update_cmd_buf_64(void *msg, bool cleanup,
			struct qseecom_dev_handle *data)
{
	struct ion_handle *ihandle;
	char *field;
	int ret = 0;
	int i = 0;
	uint32_t len = 0;
	struct scatterlist *sg;
	struct qseecom_send_modfd_cmd_req *req = NULL;
	struct qseecom_send_modfd_listener_resp *lstnr_resp = NULL;
	struct qseecom_registered_listener_list *this_lstnr = NULL;
	uint32_t offset;
	struct sg_table *sg_ptr;

	if ((data->type != QSEECOM_LISTENER_SERVICE) &&
			(data->type != QSEECOM_CLIENT_APP))
		return -EFAULT;

	if (msg == NULL) {
		pr_err("Invalid address\n");
		return -EINVAL;
	}
	if (data->type == QSEECOM_LISTENER_SERVICE) {
		lstnr_resp = (struct qseecom_send_modfd_listener_resp *)msg;
		this_lstnr = __qseecom_find_svc(data->listener.id);
		if (IS_ERR_OR_NULL(this_lstnr)) {
			pr_err("Invalid listener ID\n");
			return -ENOMEM;
		}
	} else {
		req = (struct qseecom_send_modfd_cmd_req *)msg;
	}

	for (i = 0; i < MAX_ION_FD; i++) {
		if ((data->type != QSEECOM_LISTENER_SERVICE) &&
						(req->ifd_data[i].fd > 0)) {
			ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					req->ifd_data[i].fd);
			if (IS_ERR_OR_NULL(ihandle)) {
				pr_err("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			field = (char *) req->cmd_req_buf +
				req->ifd_data[i].cmd_buf_offset;
		} else if ((data->type == QSEECOM_LISTENER_SERVICE) &&
				(lstnr_resp->ifd_data[i].fd > 0)) {
			ihandle = ion_import_dma_buf(qseecom.ion_clnt,
						lstnr_resp->ifd_data[i].fd);
			if (IS_ERR_OR_NULL(ihandle)) {
				pr_err("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			field = lstnr_resp->resp_buf_ptr +
				lstnr_resp->ifd_data[i].cmd_buf_offset;
		} else {
			continue;
		}
		/* Populate the cmd data structure with the phys_addr */
		sg_ptr = ion_sg_table(qseecom.ion_clnt, ihandle);
		if (IS_ERR_OR_NULL(sg_ptr)) {
			pr_err("IOn client could not retrieve sg table\n");
			goto err;
		}
		if (sg_ptr->nents == 0) {
			pr_err("Num of scattered entries is 0\n");
			goto err;
		}
		if (sg_ptr->nents > QSEECOM_MAX_SG_ENTRY) {
			pr_warn("Num of scattered entries");
			pr_warn(" (%d) is greater than %d\n",
				sg_ptr->nents, QSEECOM_MAX_SG_ENTRY);
			if (cleanup) {
				if (data->client.sec_buf_fd[i].is_sec_buf_fd &&
					data->client.sec_buf_fd[i].vbase)
					dma_free_coherent(qseecom.pdev,
					data->client.sec_buf_fd[i].size,
					data->client.sec_buf_fd[i].vbase,
					data->client.sec_buf_fd[i].pbase);
			} else {
				ret = __qseecom_allocate_sg_list_buffer(data,
						field, i, sg_ptr);
				if (ret) {
					pr_err("Failed to allocate sg list buffer\n");
					goto err;
				}
			}
			len = QSEECOM_SG_LIST_BUF_HDR_SZ_64BIT;
			sg = sg_ptr->sgl;
			goto cleanup;
		}
		sg = sg_ptr->sgl;
		if (sg_ptr->nents == 1) {
			uint64_t *update_64bit;
			if (__boundary_checks_offset(req, lstnr_resp, data, i))
				goto err;
				/* 64bit app uses 64bit address */
			update_64bit = (uint64_t *) field;
			*update_64bit = cleanup ? 0 :
					(uint64_t)sg_dma_address(sg_ptr->sgl);
			len += (uint32_t)sg->length;
		} else {
			struct qseecom_sg_entry_64bit *update_64bit;
			int j = 0;

			if ((data->type != QSEECOM_LISTENER_SERVICE) &&
					(req->ifd_data[i].fd > 0)) {

				if ((req->cmd_req_len <
					 SG_ENTRY_SZ_64BIT * sg_ptr->nents) ||
					(req->ifd_data[i].cmd_buf_offset >
					(req->cmd_req_len -
					SG_ENTRY_SZ_64BIT * sg_ptr->nents))) {
					pr_err("Invalid offset = 0x%x\n",
					req->ifd_data[i].cmd_buf_offset);
					goto err;
				}

			} else if ((data->type == QSEECOM_LISTENER_SERVICE) &&
					(lstnr_resp->ifd_data[i].fd > 0)) {

				if ((lstnr_resp->resp_len <
					SG_ENTRY_SZ_64BIT * sg_ptr->nents) ||
				(lstnr_resp->ifd_data[i].cmd_buf_offset >
						(lstnr_resp->resp_len -
					SG_ENTRY_SZ_64BIT * sg_ptr->nents))) {
					goto err;
				}
			}
			/* 64bit app uses 64bit address */
			update_64bit = (struct qseecom_sg_entry_64bit *)field;
			for (j = 0; j < sg_ptr->nents; j++) {
				update_64bit->phys_addr = cleanup ? 0 :
					(uint64_t)sg_dma_address(sg);
				update_64bit->len = cleanup ? 0 :
						(uint32_t)sg->length;
				update_64bit++;
				len += sg->length;
				sg = sg_next(sg);
			}
		}
cleanup:
		if (cleanup) {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
					ihandle, NULL, len,
					ION_IOC_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				goto err;
			}
		} else {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
					ihandle, NULL, len,
					ION_IOC_CLEAN_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				goto err;
			}
			if (data->type == QSEECOM_CLIENT_APP) {
				offset = req->ifd_data[i].cmd_buf_offset;
				data->sglistinfo_ptr[i].indexAndFlags =
					SGLISTINFO_SET_INDEX_FLAG(
					(sg_ptr->nents == 1), 1, offset);
				data->sglistinfo_ptr[i].sizeOrCount =
					(sg_ptr->nents == 1) ?
					sg->length : sg_ptr->nents;
				data->sglist_cnt = i + 1;
			} else {
				offset = (lstnr_resp->ifd_data[i].cmd_buf_offset
					+ (uintptr_t)lstnr_resp->resp_buf_ptr -
					(uintptr_t)this_lstnr->sb_virt);
				this_lstnr->sglistinfo_ptr[i].indexAndFlags =
					SGLISTINFO_SET_INDEX_FLAG(
					(sg_ptr->nents == 1), 1, offset);
				this_lstnr->sglistinfo_ptr[i].sizeOrCount =
					(sg_ptr->nents == 1) ?
					sg->length : sg_ptr->nents;
				this_lstnr->sglist_cnt = i + 1;
			}
		}
		/* Deallocate the handle */
		if (!IS_ERR_OR_NULL(ihandle))
			ion_free(qseecom.ion_clnt, ihandle);
	}
	return ret;
err:
	for (i = 0; i < MAX_ION_FD; i++)
		if (data->client.sec_buf_fd[i].is_sec_buf_fd &&
			data->client.sec_buf_fd[i].vbase)
			dma_free_coherent(qseecom.pdev,
				data->client.sec_buf_fd[i].size,
				data->client.sec_buf_fd[i].vbase,
				data->client.sec_buf_fd[i].pbase);
	if (!IS_ERR_OR_NULL(ihandle))
		ion_free(qseecom.ion_clnt, ihandle);
	return -ENOMEM;
}

static int __qseecom_send_modfd_cmd(struct qseecom_dev_handle *data,
					void __user *argp,
					bool is_64bit_addr)
{
	int ret = 0;
	int i;
	struct qseecom_send_modfd_cmd_req req;
	struct qseecom_send_cmd_req send_cmd_req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	send_cmd_req.cmd_req_buf = req.cmd_req_buf;
	send_cmd_req.cmd_req_len = req.cmd_req_len;
	send_cmd_req.resp_buf = req.resp_buf;
	send_cmd_req.resp_len = req.resp_len;

	if (__validate_send_cmd_inputs(data, &send_cmd_req))
		return -EINVAL;

	/* validate offsets */
	for (i = 0; i < MAX_ION_FD; i++) {
		if (req.ifd_data[i].cmd_buf_offset >= req.cmd_req_len) {
			pr_err("Invalid offset %d = 0x%x\n",
				i, req.ifd_data[i].cmd_buf_offset);
			return -EINVAL;
		}
	}
	req.cmd_req_buf = (void *)__qseecom_uvirt_to_kvirt(data,
						(uintptr_t)req.cmd_req_buf);
	req.resp_buf = (void *)__qseecom_uvirt_to_kvirt(data,
						(uintptr_t)req.resp_buf);

	if (!is_64bit_addr) {
		ret = __qseecom_update_cmd_buf(&req, false, data);
		if (ret)
			return ret;
		ret = __qseecom_send_cmd(data, &send_cmd_req);
		if (ret)
			return ret;
		ret = __qseecom_update_cmd_buf(&req, true, data);
		if (ret)
			return ret;
	} else {
		ret = __qseecom_update_cmd_buf_64(&req, false, data);
		if (ret)
			return ret;
		ret = __qseecom_send_cmd(data, &send_cmd_req);
		if (ret)
			return ret;
		ret = __qseecom_update_cmd_buf_64(&req, true, data);
		if (ret)
			return ret;
	}

	return ret;
}

static int qseecom_send_modfd_cmd(struct qseecom_dev_handle *data,
					void __user *argp)
{
	return __qseecom_send_modfd_cmd(data, argp, false);
}

static int qseecom_send_modfd_cmd_64(struct qseecom_dev_handle *data,
					void __user *argp)
{
	return __qseecom_send_modfd_cmd(data, argp, true);
}



static int __qseecom_listener_has_rcvd_req(struct qseecom_dev_handle *data,
		struct qseecom_registered_listener_list *svc)
{
	int ret;
	ret = (svc->rcv_req_flag != 0);
	return ret || data->abort;
}

static int qseecom_receive_req(struct qseecom_dev_handle *data)
{
	int ret = 0;
	struct qseecom_registered_listener_list *this_lstnr;

	this_lstnr = __qseecom_find_svc(data->listener.id);
	if (!this_lstnr) {
		pr_err("Invalid listener ID\n");
		return -ENODATA;
	}

	while (1) {
		if (wait_event_freezable(this_lstnr->rcv_req_wq,
				__qseecom_listener_has_rcvd_req(data,
				this_lstnr))) {
			pr_debug("Interrupted: exiting Listener Service = %d\n",
						(uint32_t)data->listener.id);
			/* woken up for different reason */
			return -ERESTARTSYS;
		}

		if (data->abort) {
			pr_err("Aborting Listener Service = %d\n",
						(uint32_t)data->listener.id);
			return -ENODEV;
		}
		this_lstnr->rcv_req_flag = 0;
		break;
	}
	return ret;
}

static bool __qseecom_is_fw_image_valid(const struct firmware *fw_entry)
{
	unsigned char app_arch = 0;
	struct elf32_hdr *ehdr;
	struct elf64_hdr *ehdr64;

	app_arch = *(unsigned char *)(fw_entry->data + EI_CLASS);

	switch (app_arch) {
	case ELFCLASS32: {
		ehdr = (struct elf32_hdr *)fw_entry->data;
		if (fw_entry->size < sizeof(*ehdr)) {
			pr_err("%s: Not big enough to be an elf32 header\n",
					 qseecom.pdev->init_name);
			return false;
		}
		if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
			pr_err("%s: Not an elf32 header\n",
					 qseecom.pdev->init_name);
			return false;
		}
		if (ehdr->e_phnum == 0) {
			pr_err("%s: No loadable segments\n",
					 qseecom.pdev->init_name);
			return false;
		}
		if (sizeof(struct elf32_phdr) * ehdr->e_phnum +
		    sizeof(struct elf32_hdr) > fw_entry->size) {
			pr_err("%s: Program headers not within mdt\n",
					 qseecom.pdev->init_name);
			return false;
		}
		break;
	}
	case ELFCLASS64: {
		ehdr64 = (struct elf64_hdr *)fw_entry->data;
		if (fw_entry->size < sizeof(*ehdr64)) {
			pr_err("%s: Not big enough to be an elf64 header\n",
					 qseecom.pdev->init_name);
			return false;
		}
		if (memcmp(ehdr64->e_ident, ELFMAG, SELFMAG)) {
			pr_err("%s: Not an elf64 header\n",
					 qseecom.pdev->init_name);
			return false;
		}
		if (ehdr64->e_phnum == 0) {
			pr_err("%s: No loadable segments\n",
					 qseecom.pdev->init_name);
			return false;
		}
		if (sizeof(struct elf64_phdr) * ehdr64->e_phnum +
		    sizeof(struct elf64_hdr) > fw_entry->size) {
			pr_err("%s: Program headers not within mdt\n",
					 qseecom.pdev->init_name);
			return false;
		}
		break;
	}
	default: {
		pr_err("QSEE app arch %u is not supported\n", app_arch);
		return false;
	}
	}
	return true;
}

static int __qseecom_get_fw_size(const char *appname, uint32_t *fw_size,
					uint32_t *app_arch)
{
	int ret = -1;
	int i = 0, rc = 0;
	const struct firmware *fw_entry = NULL;
	char fw_name[MAX_APP_NAME_SIZE];
	struct elf32_hdr *ehdr;
	struct elf64_hdr *ehdr64;
	int num_images = 0;

	snprintf(fw_name, sizeof(fw_name), "%s.mdt", appname);
	rc = request_firmware(&fw_entry, fw_name,  qseecom.pdev);
	if (rc) {
		pr_err("error with request_firmware\n");
		ret = -EIO;
		goto err;
	}
	if (!__qseecom_is_fw_image_valid(fw_entry)) {
		ret = -EIO;
		goto err;
	}
	*app_arch = *(unsigned char *)(fw_entry->data + EI_CLASS);
	*fw_size = fw_entry->size;
	if (*app_arch == ELFCLASS32) {
		ehdr = (struct elf32_hdr *)fw_entry->data;
		num_images = ehdr->e_phnum;
	} else if (*app_arch == ELFCLASS64) {
		ehdr64 = (struct elf64_hdr *)fw_entry->data;
		num_images = ehdr64->e_phnum;
	} else {
		pr_err("QSEE %s app, arch %u is not supported\n",
						appname, *app_arch);
		ret = -EIO;
		goto err;
	}
	pr_debug("QSEE %s app, arch %u\n", appname, *app_arch);
	release_firmware(fw_entry);
	fw_entry = NULL;
	for (i = 0; i < num_images; i++) {
		memset(fw_name, 0, sizeof(fw_name));
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d", appname, i);
		ret = request_firmware(&fw_entry, fw_name, qseecom.pdev);
		if (ret)
			goto err;
		if (*fw_size > U32_MAX - fw_entry->size) {
			pr_err("QSEE %s app file size overflow\n", appname);
			ret = -EINVAL;
			goto err;
		}
		*fw_size += fw_entry->size;
		release_firmware(fw_entry);
		fw_entry = NULL;
	}

	return ret;
err:
	if (fw_entry)
		release_firmware(fw_entry);
	*fw_size = 0;
	return ret;
}

static int __qseecom_get_fw_data(const char *appname, u8 *img_data,
				uint32_t fw_size,
				struct qseecom_load_app_ireq *load_req)
{
	int ret = -1;
	int i = 0, rc = 0;
	const struct firmware *fw_entry = NULL;
	char fw_name[MAX_APP_NAME_SIZE];
	u8 *img_data_ptr = img_data;
	struct elf32_hdr *ehdr;
	struct elf64_hdr *ehdr64;
	int num_images = 0;
	unsigned char app_arch = 0;

	snprintf(fw_name, sizeof(fw_name), "%s.mdt", appname);
	rc = request_firmware(&fw_entry, fw_name,  qseecom.pdev);
	if (rc) {
		ret = -EIO;
		goto err;
	}

	load_req->img_len = fw_entry->size;
	if (load_req->img_len > fw_size) {
		pr_err("app %s size %zu is larger than buf size %u\n",
			appname, fw_entry->size, fw_size);
		ret = -EINVAL;
		goto err;
	}
	memcpy(img_data_ptr, fw_entry->data, fw_entry->size);
	img_data_ptr = img_data_ptr + fw_entry->size;
	load_req->mdt_len = fw_entry->size; /*Get MDT LEN*/

	app_arch = *(unsigned char *)(fw_entry->data + EI_CLASS);
	if (app_arch == ELFCLASS32) {
		ehdr = (struct elf32_hdr *)fw_entry->data;
		num_images = ehdr->e_phnum;
	} else if (app_arch == ELFCLASS64) {
		ehdr64 = (struct elf64_hdr *)fw_entry->data;
		num_images = ehdr64->e_phnum;
	} else {
		pr_err("QSEE %s app, arch %u is not supported\n",
						appname, app_arch);
		ret = -EIO;
		goto err;
	}
	release_firmware(fw_entry);
	fw_entry = NULL;
	for (i = 0; i < num_images; i++) {
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d", appname, i);
		ret = request_firmware(&fw_entry, fw_name,  qseecom.pdev);
		if (ret) {
			pr_err("Failed to locate blob %s\n", fw_name);
			goto err;
		}
		if ((fw_entry->size > U32_MAX - load_req->img_len) ||
			(fw_entry->size + load_req->img_len > fw_size)) {
			pr_err("Invalid file size for %s\n", fw_name);
			ret = -EINVAL;
			goto err;
		}
		memcpy(img_data_ptr, fw_entry->data, fw_entry->size);
		img_data_ptr = img_data_ptr + fw_entry->size;
		load_req->img_len += fw_entry->size;
		release_firmware(fw_entry);
		fw_entry = NULL;
	}
	return ret;
err:
	release_firmware(fw_entry);
	return ret;
}

static int __qseecom_allocate_img_data(struct ion_handle **pihandle,
			u8 **data, uint32_t fw_size, ion_phys_addr_t *paddr)
{
	size_t len = 0;
	int ret = 0;
	ion_phys_addr_t pa;
	struct ion_handle *ihandle = NULL;
	u8 *img_data = NULL;

	ihandle = ion_alloc(qseecom.ion_clnt, fw_size,
			SZ_4K, ION_HEAP(ION_QSECOM_HEAP_ID), 0);

	if (IS_ERR_OR_NULL(ihandle)) {
		pr_err("ION alloc failed\n");
		return -ENOMEM;
	}
	img_data = (u8 *)ion_map_kernel(qseecom.ion_clnt,
					ihandle);

	if (IS_ERR_OR_NULL(img_data)) {
		pr_err("ION memory mapping for image loading failed\n");
		ret = -ENOMEM;
		goto exit_ion_free;
	}
	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, ihandle, &pa, &len);
	if (ret) {
		pr_err("physical memory retrieval failure\n");
		ret = -EIO;
		goto exit_ion_unmap_kernel;
	}

	*pihandle = ihandle;
	*data = img_data;
	*paddr = pa;
	return ret;

exit_ion_unmap_kernel:
	ion_unmap_kernel(qseecom.ion_clnt, ihandle);
exit_ion_free:
	ion_free(qseecom.ion_clnt, ihandle);
	ihandle = NULL;
	return ret;
}

static void __qseecom_free_img_data(struct ion_handle **ihandle)
{
	ion_unmap_kernel(qseecom.ion_clnt, *ihandle);
	ion_free(qseecom.ion_clnt, *ihandle);
	*ihandle = NULL;
}

static int __qseecom_load_fw(struct qseecom_dev_handle *data, char *appname,
				uint32_t *app_id)
{
	int ret = -1;
	uint32_t fw_size = 0;
	struct qseecom_load_app_ireq load_req = {0, 0, 0, 0};
	struct qseecom_load_app_64bit_ireq load_req_64bit = {0, 0, 0, 0};
	struct qseecom_command_scm_resp resp;
	u8 *img_data = NULL;
	ion_phys_addr_t pa = 0;
	struct ion_handle *ihandle = NULL;
	void *cmd_buf = NULL;
	size_t cmd_len;
	uint32_t app_arch = 0;

	if (!data || !appname || !app_id) {
		pr_err("Null pointer to data or appname or appid\n");
		return -EINVAL;
	}
	*app_id = 0;
	if (__qseecom_get_fw_size(appname, &fw_size, &app_arch))
		return -EIO;
	data->client.app_arch = app_arch;

	/* Check and load cmnlib */
	if (qseecom.qsee_version > QSEEE_VERSION_00) {
		if (!qseecom.commonlib_loaded && app_arch == ELFCLASS32) {
			ret = qseecom_load_commonlib_image(data, "cmnlib");
			if (ret) {
				pr_err("failed to load cmnlib\n");
				return -EIO;
			}
			qseecom.commonlib_loaded = true;
			pr_debug("cmnlib is loaded\n");
		}

		if (!qseecom.commonlib64_loaded && app_arch == ELFCLASS64) {
			ret = qseecom_load_commonlib_image(data, "cmnlib64");
			if (ret) {
				pr_err("failed to load cmnlib64\n");
				return -EIO;
			}
			qseecom.commonlib64_loaded = true;
			pr_debug("cmnlib64 is loaded\n");
		}
	}

	ret = __qseecom_allocate_img_data(&ihandle, &img_data, fw_size, &pa);
	if (ret)
		return ret;

	ret = __qseecom_get_fw_data(appname, img_data, fw_size, &load_req);
	if (ret) {
		ret = -EIO;
		goto exit_free_img_data;
	}

	/* Populate the load_req parameters */
	if (qseecom.qsee_version < QSEE_VERSION_40) {
		load_req.qsee_cmd_id = QSEOS_APP_START_COMMAND;
		load_req.mdt_len = load_req.mdt_len;
		load_req.img_len = load_req.img_len;
		strlcpy(load_req.app_name, appname, MAX_APP_NAME_SIZE);
		load_req.phy_addr = (uint32_t)pa;
		cmd_buf = (void *)&load_req;
		cmd_len = sizeof(struct qseecom_load_app_ireq);
	} else {
		load_req_64bit.qsee_cmd_id = QSEOS_APP_START_COMMAND;
		load_req_64bit.mdt_len = load_req.mdt_len;
		load_req_64bit.img_len = load_req.img_len;
		strlcpy(load_req_64bit.app_name, appname, MAX_APP_NAME_SIZE);
		load_req_64bit.phy_addr = (uint64_t)pa;
		cmd_buf = (void *)&load_req_64bit;
		cmd_len = sizeof(struct qseecom_load_app_64bit_ireq);
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret) {
			ret = -EIO;
			goto exit_free_img_data;
		}
	}

	ret = __qseecom_enable_clk_scale_up(data);
	if (ret) {
		ret = -EIO;
		goto exit_unregister_bus_bw_need;
	}

	ret = msm_ion_do_cache_op(qseecom.ion_clnt, ihandle,
				img_data, fw_size,
				ION_IOC_CLEAN_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		goto exit_disable_clk_vote;
	}

	/* SCM_CALL to load the image */
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, cmd_buf, cmd_len,
			&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to load failed : ret %d\n", ret);
		ret = -EIO;
		goto exit_disable_clk_vote;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		*app_id = resp.data;
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret)
			pr_err("process_incomplete_cmd FAILED\n");
		else
			*app_id = resp.data;
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("scm call failed with response QSEOS_RESULT FAILURE\n");
		break;
	default:
		pr_err("scm call return unknown response %d\n", resp.result);
		ret = -EINVAL;
		break;
	}

exit_disable_clk_vote:
	__qseecom_disable_clk_scale_down(data);

exit_unregister_bus_bw_need:
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}

exit_free_img_data:
	__qseecom_free_img_data(&ihandle);
	return ret;
}

static int qseecom_load_commonlib_image(struct qseecom_dev_handle *data,
					char *cmnlib_name)
{
	int ret = 0;
	uint32_t fw_size = 0;
	struct qseecom_load_app_ireq load_req = {0, 0, 0, 0};
	struct qseecom_load_app_64bit_ireq load_req_64bit = {0, 0, 0, 0};
	struct qseecom_command_scm_resp resp;
	u8 *img_data = NULL;
	ion_phys_addr_t pa = 0;
	void *cmd_buf = NULL;
	size_t cmd_len;
	uint32_t app_arch = 0;

	if (!cmnlib_name) {
		pr_err("cmnlib_name is NULL\n");
		return -EINVAL;
	}
	if (strlen(cmnlib_name) >= MAX_APP_NAME_SIZE) {
		pr_err("The cmnlib_name (%s) with length %zu is not valid\n",
			cmnlib_name, strlen(cmnlib_name));
		return -EINVAL;
	}

	if (__qseecom_get_fw_size(cmnlib_name, &fw_size, &app_arch))
		return -EIO;

	ret = __qseecom_allocate_img_data(&qseecom.cmnlib_ion_handle,
						&img_data, fw_size, &pa);
	if (ret)
		return -EIO;

	ret = __qseecom_get_fw_data(cmnlib_name, img_data, fw_size, &load_req);
	if (ret) {
		ret = -EIO;
		goto exit_free_img_data;
	}
	if (qseecom.qsee_version < QSEE_VERSION_40) {
		load_req.phy_addr = (uint32_t)pa;
		load_req.qsee_cmd_id = QSEOS_LOAD_SERV_IMAGE_COMMAND;
		cmd_buf = (void *)&load_req;
		cmd_len = sizeof(struct qseecom_load_lib_image_ireq);
	} else {
		load_req_64bit.phy_addr = (uint64_t)pa;
		load_req_64bit.qsee_cmd_id = QSEOS_LOAD_SERV_IMAGE_COMMAND;
		load_req_64bit.img_len = load_req.img_len;
		load_req_64bit.mdt_len = load_req.mdt_len;
		cmd_buf = (void *)&load_req_64bit;
		cmd_len = sizeof(struct qseecom_load_lib_image_64bit_ireq);
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret) {
			ret = -EIO;
			goto exit_free_img_data;
		}
	}

	/* Vote for the SFPB clock */
	ret = __qseecom_enable_clk_scale_up(data);
	if (ret) {
		ret = -EIO;
		goto exit_unregister_bus_bw_need;
	}

	ret = msm_ion_do_cache_op(qseecom.ion_clnt, qseecom.cmnlib_ion_handle,
				img_data, fw_size,
				ION_IOC_CLEAN_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		goto exit_disable_clk_vote;
	}

	/* SCM_CALL to load the image */
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, cmd_buf, cmd_len,
							&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to load failed : ret %d\n", ret);
		ret = -EIO;
		goto exit_disable_clk_vote;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("scm call failed w/response result%d\n", resp.result);
		ret = -EINVAL;
		goto exit_disable_clk_vote;
	case  QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd failed err: %d\n", ret);
			goto exit_disable_clk_vote;
		}
		break;
	default:
		pr_err("scm call return unknown response %d\n",	resp.result);
		ret = -EINVAL;
		goto exit_disable_clk_vote;
	}

exit_disable_clk_vote:
	__qseecom_disable_clk_scale_down(data);

exit_unregister_bus_bw_need:
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}

exit_free_img_data:
	__qseecom_free_img_data(&qseecom.cmnlib_ion_handle);
	return ret;
}

static int qseecom_unload_commonlib_image(void)
{
	int ret = -EINVAL;
	struct qseecom_unload_lib_image_ireq unload_req = {0};
	struct qseecom_command_scm_resp resp;

	/* Populate the remaining parameters */
	unload_req.qsee_cmd_id = QSEOS_UNLOAD_SERV_IMAGE_COMMAND;

	/* SCM_CALL to load the image */
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, &unload_req,
			sizeof(struct qseecom_unload_lib_image_ireq),
						&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to unload lib failed : ret %d\n", ret);
		ret = -EIO;
	} else {
		switch (resp.result) {
		case QSEOS_RESULT_SUCCESS:
			break;
		case QSEOS_RESULT_FAILURE:
			pr_err("scm fail resp.result QSEOS_RESULT FAILURE\n");
			break;
		default:
			pr_err("scm call return unknown response %d\n",
					resp.result);
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

int qseecom_start_app(struct qseecom_handle **handle,
						char *app_name, uint32_t size)
{
	int32_t ret = 0;
	unsigned long flags = 0;
	struct qseecom_dev_handle *data = NULL;
	struct qseecom_check_app_ireq app_ireq;
	struct qseecom_registered_app_list *entry = NULL;
	struct qseecom_registered_kclient_list *kclient_entry = NULL;
	bool found_app = false;
	size_t len;
	ion_phys_addr_t pa;
	uint32_t fw_size, app_arch;
	uint32_t app_id = 0;

	if (atomic_read(&qseecom.qseecom_state) != QSEECOM_STATE_READY) {
		pr_err("Not allowed to be called in %d state\n",
				atomic_read(&qseecom.qseecom_state));
		return -EPERM;
	}
	if (!app_name) {
		pr_err("failed to get the app name\n");
		return -EINVAL;
	}

	if (strnlen(app_name, MAX_APP_NAME_SIZE) == MAX_APP_NAME_SIZE) {
		pr_err("The app_name (%s) with length %zu is not valid\n",
			app_name, strnlen(app_name, MAX_APP_NAME_SIZE));
		return -EINVAL;
	}

	*handle = kzalloc(sizeof(struct qseecom_handle), GFP_KERNEL);
	if (!(*handle)) {
		pr_err("failed to allocate memory for kernel client handle\n");
		return -ENOMEM;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("kmalloc failed\n");
		if (ret == 0) {
			kfree(*handle);
			*handle = NULL;
		}
		return -ENOMEM;
	}
	data->abort = 0;
	data->type = QSEECOM_CLIENT_APP;
	data->released = false;
	data->client.sb_length = size;
	data->client.user_virt_sb_base = 0;
	data->client.ihandle = NULL;

	init_waitqueue_head(&data->abort_wq);

	data->client.ihandle = ion_alloc(qseecom.ion_clnt, size, 4096,
				ION_HEAP(ION_QSECOM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(data->client.ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		kfree(data);
		kfree(*handle);
		*handle = NULL;
		return -EINVAL;
	}
	mutex_lock(&app_access_lock);

	app_ireq.qsee_cmd_id = QSEOS_APP_LOOKUP_COMMAND;
	strlcpy(app_ireq.app_name, app_name, MAX_APP_NAME_SIZE);
	ret = __qseecom_check_app_exists(app_ireq, &app_id);
	if (ret)
		goto err;

	strlcpy(data->client.app_name, app_name, MAX_APP_NAME_SIZE);
	if (app_id) {
		pr_warn("App id %d for [%s] app exists\n", app_id,
			(char *)app_ireq.app_name);
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(entry,
				&qseecom.registered_app_list_head, list){
			if (entry->app_id == app_id) {
				entry->ref_cnt++;
				found_app = true;
				break;
			}
		}
		spin_unlock_irqrestore(
				&qseecom.registered_app_list_lock, flags);
		if (!found_app)
			pr_warn("App_id %d [%s] was loaded but not registered\n",
					ret, (char *)app_ireq.app_name);
	} else {
		/* load the app and get the app_id  */
		pr_debug("%s: Loading app for the first time'\n",
				qseecom.pdev->init_name);
		ret = __qseecom_load_fw(data, app_name, &app_id);
		if (ret < 0)
			goto err;
	}
	data->client.app_id = app_id;
	if (!found_app) {
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			pr_err("kmalloc for app entry failed\n");
			ret =  -ENOMEM;
			goto err;
		}
		entry->app_id = app_id;
		entry->ref_cnt = 1;
		strlcpy(entry->app_name, app_name, MAX_APP_NAME_SIZE);
		if (__qseecom_get_fw_size(app_name, &fw_size, &app_arch)) {
			ret = -EIO;
			kfree(entry);
			goto err;
		}
		entry->app_arch = app_arch;
		entry->app_blocked = false;
		entry->blocked_on_listener_id = 0;
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_add_tail(&entry->list, &qseecom.registered_app_list_head);
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
									flags);
	}

	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, data->client.ihandle, &pa, &len);
	if (ret) {
		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		goto err;
	}

	/* Populate the structure for sending scm call to load image */
	data->client.sb_virt = (char *) ion_map_kernel(qseecom.ion_clnt,
							data->client.ihandle);
	if (IS_ERR_OR_NULL(data->client.sb_virt)) {
		pr_err("ION memory mapping for client shared buf failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->client.user_virt_sb_base = (uintptr_t)data->client.sb_virt;
	data->client.sb_phys = (phys_addr_t)pa;
	(*handle)->dev = (void *)data;
	(*handle)->sbuf = (unsigned char *)data->client.sb_virt;
	(*handle)->sbuf_len = data->client.sb_length;

	kclient_entry = kzalloc(sizeof(*kclient_entry), GFP_KERNEL);
	if (!kclient_entry) {
		pr_err("kmalloc failed\n");
		ret = -ENOMEM;
		goto err;
	}
	kclient_entry->handle = *handle;

	spin_lock_irqsave(&qseecom.registered_kclient_list_lock, flags);
	list_add_tail(&kclient_entry->list,
			&qseecom.registered_kclient_list_head);
	spin_unlock_irqrestore(&qseecom.registered_kclient_list_lock, flags);

	mutex_unlock(&app_access_lock);
	return 0;

err:
	kfree(data);
	kfree(*handle);
	*handle = NULL;
	mutex_unlock(&app_access_lock);
	return ret;
}
EXPORT_SYMBOL(qseecom_start_app);

int qseecom_shutdown_app(struct qseecom_handle **handle)
{
	int ret = -EINVAL;
	struct qseecom_dev_handle *data;

	struct qseecom_registered_kclient_list *kclient = NULL;
	unsigned long flags = 0;
	bool found_handle = false;

	if (atomic_read(&qseecom.qseecom_state) != QSEECOM_STATE_READY) {
		pr_err("Not allowed to be called in %d state\n",
				atomic_read(&qseecom.qseecom_state));
		return -EPERM;
	}

	if ((handle == NULL)  || (*handle == NULL)) {
		pr_err("Handle is not initialized\n");
		return -EINVAL;
	}
	data =	(struct qseecom_dev_handle *) ((*handle)->dev);
	mutex_lock(&app_access_lock);

	spin_lock_irqsave(&qseecom.registered_kclient_list_lock, flags);
	list_for_each_entry(kclient, &qseecom.registered_kclient_list_head,
				list) {
		if (kclient->handle == (*handle)) {
			list_del(&kclient->list);
			found_handle = true;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_kclient_list_lock, flags);
	if (!found_handle)
		pr_err("Unable to find the handle, exiting\n");
	else
		ret = qseecom_unload_app(data, false);

	mutex_unlock(&app_access_lock);
	if (ret == 0) {
		kzfree(data);
		kzfree(*handle);
		kzfree(kclient);
		*handle = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(qseecom_shutdown_app);

int qseecom_send_command(struct qseecom_handle *handle, void *send_buf,
			uint32_t sbuf_len, void *resp_buf, uint32_t rbuf_len)
{
	int ret = 0;
	struct qseecom_send_cmd_req req = {0, 0, 0, 0};
	struct qseecom_dev_handle *data;
	bool perf_enabled = false;

	if (atomic_read(&qseecom.qseecom_state) != QSEECOM_STATE_READY) {
		pr_err("Not allowed to be called in %d state\n",
				atomic_read(&qseecom.qseecom_state));
		return -EPERM;
	}

	if (handle == NULL) {
		pr_err("Handle is not initialized\n");
		return -EINVAL;
	}
	data = handle->dev;

	req.cmd_req_len = sbuf_len;
	req.resp_len = rbuf_len;
	req.cmd_req_buf = send_buf;
	req.resp_buf = resp_buf;

	if (__validate_send_cmd_inputs(data, &req))
		return -EINVAL;

	mutex_lock(&app_access_lock);
	if (qseecom.support_bus_scaling) {
		ret = qseecom_scale_bus_bandwidth_timer(INACTIVE);
		if (ret) {
			pr_err("Failed to set bw.\n");
			mutex_unlock(&app_access_lock);
			return ret;
		}
	}
	/*
	* On targets where crypto clock is handled by HLOS,
	* if clk_access_cnt is zero and perf_enabled is false,
	* then the crypto clock was not enabled before sending cmd
	* to tz, qseecom will enable the clock to avoid service failure.
	*/
	if (!qseecom.no_clock_support &&
		!qseecom.qsee.clk_access_cnt && !data->perf_enabled) {
		pr_debug("ce clock is not enabled!\n");
		ret = qseecom_perf_enable(data);
		if (ret) {
			pr_err("Failed to vote for clock with err %d\n",
						ret);
			mutex_unlock(&app_access_lock);
			return -EINVAL;
		}
		perf_enabled = true;
	}
	if (!strcmp(data->client.app_name, "securemm"))
		data->use_legacy_cmd = true;

	ret = __qseecom_send_cmd(data, &req);
	data->use_legacy_cmd = false;
	if (qseecom.support_bus_scaling)
		__qseecom_add_bw_scale_down_timer(
			QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);

	if (perf_enabled) {
		qsee_disable_clock_vote(data, CLK_DFAB);
		qsee_disable_clock_vote(data, CLK_SFPB);
	}

	mutex_unlock(&app_access_lock);

	if (ret)
		return ret;

	pr_debug("sending cmd_req->rsp size: %u, ptr: 0x%pK\n",
			req.resp_len, req.resp_buf);
	return ret;
}
EXPORT_SYMBOL(qseecom_send_command);

int qseecom_set_bandwidth(struct qseecom_handle *handle, bool high)
{
	int ret = 0;
	if ((handle == NULL) || (handle->dev == NULL)) {
		pr_err("No valid kernel client\n");
		return -EINVAL;
	}
	if (high) {
		if (qseecom.support_bus_scaling) {
			mutex_lock(&qsee_bw_mutex);
			__qseecom_register_bus_bandwidth_needs(handle->dev,
									HIGH);
			mutex_unlock(&qsee_bw_mutex);
		} else {
			ret = qseecom_perf_enable(handle->dev);
			if (ret)
				pr_err("Failed to vote for clock with err %d\n",
						ret);
		}
	} else {
		if (!qseecom.support_bus_scaling) {
			qsee_disable_clock_vote(handle->dev, CLK_DFAB);
			qsee_disable_clock_vote(handle->dev, CLK_SFPB);
		} else {
			mutex_lock(&qsee_bw_mutex);
			qseecom_unregister_bus_bandwidth_needs(handle->dev);
			mutex_unlock(&qsee_bw_mutex);
		}
	}
	return ret;
}
EXPORT_SYMBOL(qseecom_set_bandwidth);

static int qseecom_send_resp(void)
{
	qseecom.send_resp_flag = 1;
	wake_up_interruptible(&qseecom.send_resp_wq);
	return 0;
}

static int qseecom_reentrancy_send_resp(struct qseecom_dev_handle *data)
{
	struct qseecom_registered_listener_list *this_lstnr = NULL;

	pr_debug("lstnr %d send resp, wakeup\n", data->listener.id);
	this_lstnr = __qseecom_find_svc(data->listener.id);
	if (this_lstnr == NULL)
		return -EINVAL;
	qseecom.send_resp_flag = 1;
	this_lstnr->send_resp_flag = 1;
	wake_up_interruptible(&qseecom.send_resp_wq);
	return 0;
}

static int __validate_send_modfd_resp_inputs(struct qseecom_dev_handle *data,
			struct qseecom_send_modfd_listener_resp *resp,
			struct qseecom_registered_listener_list *this_lstnr)
{
	int i;

	if (!data || !resp || !this_lstnr) {
		pr_err("listener handle or resp msg is null\n");
		return -EINVAL;
	}

	if (resp->resp_buf_ptr == NULL) {
		pr_err("resp buffer is null\n");
		return -EINVAL;
	}
	/* validate resp buf length */
	if ((resp->resp_len == 0) ||
			(resp->resp_len > this_lstnr->sb_length)) {
		pr_err("resp buf length %d not valid\n", resp->resp_len);
		return -EINVAL;
	}

	if ((uintptr_t)resp->resp_buf_ptr > (ULONG_MAX - resp->resp_len)) {
		pr_err("Integer overflow in resp_len & resp_buf\n");
		return -EINVAL;
	}
	if ((uintptr_t)this_lstnr->user_virt_sb_base >
					(ULONG_MAX - this_lstnr->sb_length)) {
		pr_err("Integer overflow in user_virt_sb_base & sb_length\n");
		return -EINVAL;
	}
	/* validate resp buf */
	if (((uintptr_t)resp->resp_buf_ptr <
		(uintptr_t)this_lstnr->user_virt_sb_base) ||
		((uintptr_t)resp->resp_buf_ptr >=
		((uintptr_t)this_lstnr->user_virt_sb_base +
				this_lstnr->sb_length)) ||
		(((uintptr_t)resp->resp_buf_ptr + resp->resp_len) >
		((uintptr_t)this_lstnr->user_virt_sb_base +
						this_lstnr->sb_length))) {
		pr_err("resp buf is out of shared buffer region\n");
		return -EINVAL;
	}

	/* validate offsets */
	for (i = 0; i < MAX_ION_FD; i++) {
		if (resp->ifd_data[i].cmd_buf_offset >= resp->resp_len) {
			pr_err("Invalid offset %d = 0x%x\n",
				i, resp->ifd_data[i].cmd_buf_offset);
			return -EINVAL;
		}
	}

	return 0;
}

static int __qseecom_send_modfd_resp(struct qseecom_dev_handle *data,
				void __user *argp, bool is_64bit_addr)
{
	struct qseecom_send_modfd_listener_resp resp;
	struct qseecom_registered_listener_list *this_lstnr = NULL;

	if (copy_from_user(&resp, argp, sizeof(resp))) {
		pr_err("copy_from_user failed");
		return -EINVAL;
	}

	this_lstnr = __qseecom_find_svc(data->listener.id);
	if (this_lstnr == NULL)
		return -EINVAL;

	if (__validate_send_modfd_resp_inputs(data, &resp, this_lstnr))
		return -EINVAL;

	resp.resp_buf_ptr = this_lstnr->sb_virt +
		(uintptr_t)(resp.resp_buf_ptr - this_lstnr->user_virt_sb_base);

	if (!is_64bit_addr)
		__qseecom_update_cmd_buf(&resp, false, data);
	else
		__qseecom_update_cmd_buf_64(&resp, false, data);
	qseecom.send_resp_flag = 1;
	this_lstnr->send_resp_flag = 1;
	wake_up_interruptible(&qseecom.send_resp_wq);
	return 0;
}

static int qseecom_send_modfd_resp(struct qseecom_dev_handle *data,
						void __user *argp)
{
	return __qseecom_send_modfd_resp(data, argp, false);
}

static int qseecom_send_modfd_resp_64(struct qseecom_dev_handle *data,
						void __user *argp)
{
	return __qseecom_send_modfd_resp(data, argp, true);
}

static int qseecom_get_qseos_version(struct qseecom_dev_handle *data,
						void __user *argp)
{
	struct qseecom_qseos_version_req req;

	if (copy_from_user(&req, argp, sizeof(req))) {
		pr_err("copy_from_user failed");
		return -EINVAL;
	}
	req.qseos_version = qseecom.qseos_version;
	if (copy_to_user(argp, &req, sizeof(req))) {
		pr_err("copy_to_user failed");
		return -EINVAL;
	}
	return 0;
}

static int __qseecom_enable_clk(enum qseecom_ce_hw_instance ce)
{
	int rc = 0;
	struct qseecom_clk *qclk = NULL;

	if (qseecom.no_clock_support)
		return 0;

	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	if (ce == CLK_CE_DRV)
		qclk = &qseecom.ce_drv;

	if (qclk == NULL) {
		pr_err("CLK type not supported\n");
		return -EINVAL;
	}
	mutex_lock(&clk_access_lock);

	if (qclk->clk_access_cnt == ULONG_MAX) {
		pr_err("clk_access_cnt beyond limitation\n");
		goto err;
	}
	if (qclk->clk_access_cnt > 0) {
		qclk->clk_access_cnt++;
		mutex_unlock(&clk_access_lock);
		return rc;
	}

	/* Enable CE core clk */
	if (qclk->ce_core_clk != NULL) {
		rc = clk_prepare_enable(qclk->ce_core_clk);
		if (rc) {
			pr_err("Unable to enable/prepare CE core clk\n");
			goto err;
		}
	}
	/* Enable CE clk */
	if (qclk->ce_clk != NULL) {
		rc = clk_prepare_enable(qclk->ce_clk);
		if (rc) {
			pr_err("Unable to enable/prepare CE iface clk\n");
			goto ce_clk_err;
		}
	}
	/* Enable AXI clk */
	if (qclk->ce_bus_clk != NULL) {
		rc = clk_prepare_enable(qclk->ce_bus_clk);
		if (rc) {
			pr_err("Unable to enable/prepare CE bus clk\n");
			goto ce_bus_clk_err;
		}
	}
	qclk->clk_access_cnt++;
	mutex_unlock(&clk_access_lock);
	return 0;

ce_bus_clk_err:
	if (qclk->ce_clk != NULL)
		clk_disable_unprepare(qclk->ce_clk);
ce_clk_err:
	if (qclk->ce_core_clk != NULL)
		clk_disable_unprepare(qclk->ce_core_clk);
err:
	mutex_unlock(&clk_access_lock);
	return -EIO;
}

static void __qseecom_disable_clk(enum qseecom_ce_hw_instance ce)
{
	struct qseecom_clk *qclk;

	if (qseecom.no_clock_support)
		return;

	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	else
		qclk = &qseecom.ce_drv;

	mutex_lock(&clk_access_lock);

	if (qclk->clk_access_cnt == 0) {
		mutex_unlock(&clk_access_lock);
		return;
	}

	if (qclk->clk_access_cnt == 1) {
		if (qclk->ce_clk != NULL)
			clk_disable_unprepare(qclk->ce_clk);
		if (qclk->ce_core_clk != NULL)
			clk_disable_unprepare(qclk->ce_core_clk);
		if (qclk->ce_bus_clk != NULL)
			clk_disable_unprepare(qclk->ce_bus_clk);
	}
	qclk->clk_access_cnt--;
	mutex_unlock(&clk_access_lock);
}

static int qsee_vote_for_clock(struct qseecom_dev_handle *data,
						int32_t clk_type)
{
	int ret = 0;
	struct qseecom_clk *qclk;

	if (qseecom.no_clock_support)
		return 0;

	qclk = &qseecom.qsee;
	if (!qseecom.qsee_perf_client)
		return ret;

	switch (clk_type) {
	case CLK_DFAB:
		mutex_lock(&qsee_bw_mutex);
		if (!qseecom.qsee_bw_count) {
			if (qseecom.qsee_sfpb_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, 3);
			else {
				if (qclk->ce_core_src_clk != NULL)
					ret = __qseecom_enable_clk(CLK_QSEE);
				if (!ret) {
					ret =
					msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 1);
					if ((ret) &&
						(qclk->ce_core_src_clk != NULL))
						__qseecom_disable_clk(CLK_QSEE);
				}
			}
			if (ret)
				pr_err("DFAB Bandwidth req failed (%d)\n",
								ret);
			else {
				qseecom.qsee_bw_count++;
				data->perf_enabled = true;
			}
		} else {
			qseecom.qsee_bw_count++;
			data->perf_enabled = true;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	case CLK_SFPB:
		mutex_lock(&qsee_bw_mutex);
		if (!qseecom.qsee_sfpb_bw_count) {
			if (qseecom.qsee_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, 3);
			else {
				if (qclk->ce_core_src_clk != NULL)
					ret = __qseecom_enable_clk(CLK_QSEE);
				if (!ret) {
					ret =
					msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 2);
					if ((ret) &&
						(qclk->ce_core_src_clk != NULL))
						__qseecom_disable_clk(CLK_QSEE);
				}
			}

			if (ret)
				pr_err("SFPB Bandwidth req failed (%d)\n",
								ret);
			else {
				qseecom.qsee_sfpb_bw_count++;
				data->fast_load_enabled = true;
			}
		} else {
			qseecom.qsee_sfpb_bw_count++;
			data->fast_load_enabled = true;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	default:
		pr_err("Clock type not defined\n");
		break;
	}
	return ret;
}

static void qsee_disable_clock_vote(struct qseecom_dev_handle *data,
						int32_t clk_type)
{
	int32_t ret = 0;
	struct qseecom_clk *qclk;

	qclk = &qseecom.qsee;

	if (qseecom.no_clock_support)
		return;
	if (!qseecom.qsee_perf_client)
		return;

	switch (clk_type) {
	case CLK_DFAB:
		mutex_lock(&qsee_bw_mutex);
		if (qseecom.qsee_bw_count == 0) {
			pr_err("Client error.Extra call to disable DFAB clk\n");
			mutex_unlock(&qsee_bw_mutex);
			return;
		}

		if (qseecom.qsee_bw_count == 1) {
			if (qseecom.qsee_sfpb_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, 2);
			else {
				ret = msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 0);
				if ((!ret) && (qclk->ce_core_src_clk != NULL))
					__qseecom_disable_clk(CLK_QSEE);
			}
			if (ret)
				pr_err("SFPB Bandwidth req fail (%d)\n",
								ret);
			else {
				qseecom.qsee_bw_count--;
				data->perf_enabled = false;
			}
		} else {
			qseecom.qsee_bw_count--;
			data->perf_enabled = false;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	case CLK_SFPB:
		mutex_lock(&qsee_bw_mutex);
		if (qseecom.qsee_sfpb_bw_count == 0) {
			pr_err("Client error.Extra call to disable SFPB clk\n");
			mutex_unlock(&qsee_bw_mutex);
			return;
		}
		if (qseecom.qsee_sfpb_bw_count == 1) {
			if (qseecom.qsee_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 1);
			else {
				ret = msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 0);
				if ((!ret) && (qclk->ce_core_src_clk != NULL))
					__qseecom_disable_clk(CLK_QSEE);
			}
			if (ret)
				pr_err("SFPB Bandwidth req fail (%d)\n",
								ret);
			else {
				qseecom.qsee_sfpb_bw_count--;
				data->fast_load_enabled = false;
			}
		} else {
			qseecom.qsee_sfpb_bw_count--;
			data->fast_load_enabled = false;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	default:
		pr_err("Clock type not defined\n");
		break;
	}

}

static int qseecom_load_external_elf(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct ion_handle *ihandle;	/* Ion handle */
	struct qseecom_load_img_req load_img_req;
	int uret = 0;
	int ret;
	ion_phys_addr_t pa = 0;
	size_t len;
	struct qseecom_load_app_ireq load_req;
	struct qseecom_load_app_64bit_ireq load_req_64bit;
	struct qseecom_command_scm_resp resp;
	void *cmd_buf = NULL;
	size_t cmd_len;
	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&load_img_req,
				(void __user *)argp,
				sizeof(struct qseecom_load_img_req))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	/* Get the handle of the shared fd */
	ihandle = ion_import_dma_buf(qseecom.ion_clnt,
				load_img_req.ifd_data_fd);
	if (IS_ERR_OR_NULL(ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		return -ENOMEM;
	}

	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, ihandle, &pa, &len);
	if (ret) {
		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		return ret;
	}
	if (load_img_req.mdt_len > len || load_img_req.img_len > len) {
		pr_err("ion len %zu is smaller than mdt_len %u or img_len %u\n",
				len, load_img_req.mdt_len,
				load_img_req.img_len);
		return ret;
	}
	/* Populate the structure for sending scm call to load image */
	if (qseecom.qsee_version < QSEE_VERSION_40) {
		load_req.qsee_cmd_id = QSEOS_LOAD_EXTERNAL_ELF_COMMAND;
		load_req.mdt_len = load_img_req.mdt_len;
		load_req.img_len = load_img_req.img_len;
		load_req.phy_addr = (uint32_t)pa;
		cmd_buf = (void *)&load_req;
		cmd_len = sizeof(struct qseecom_load_app_ireq);
	} else {
		load_req_64bit.qsee_cmd_id = QSEOS_LOAD_EXTERNAL_ELF_COMMAND;
		load_req_64bit.mdt_len = load_img_req.mdt_len;
		load_req_64bit.img_len = load_img_req.img_len;
		load_req_64bit.phy_addr = (uint64_t)pa;
		cmd_buf = (void *)&load_req_64bit;
		cmd_len = sizeof(struct qseecom_load_app_64bit_ireq);
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret) {
			ret = -EIO;
			goto exit_cpu_restore;
		}
	}

	/* Vote for the SFPB clock */
	ret = __qseecom_enable_clk_scale_up(data);
	if (ret) {
		ret = -EIO;
		goto exit_register_bus_bandwidth_needs;
	}
	ret = msm_ion_do_cache_op(qseecom.ion_clnt, ihandle, NULL, len,
				ION_IOC_CLEAN_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		goto exit_disable_clock;
	}
	/*  SCM_CALL to load the external elf */
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, cmd_buf, cmd_len,
			&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to load failed : ret %d\n",
				ret);
		ret = -EFAULT;
		goto exit_disable_clock;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		pr_err("%s: qseos result incomplete\n", __func__);
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret)
			pr_err("process_incomplete_cmd failed: err: %d\n", ret);
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("scm_call rsp.result is QSEOS_RESULT_FAILURE\n");
		ret = -EFAULT;
		break;
	default:
		pr_err("scm_call response result %d not supported\n",
							resp.result);
		ret = -EFAULT;
		break;
	}

exit_disable_clock:
	__qseecom_disable_clk_scale_down(data);

exit_register_bus_bandwidth_needs:
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		uret = qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
		if (uret)
			pr_err("Failed to unregister bus bw needs %d, scm_call ret %d\n",
								uret, ret);
	}

exit_cpu_restore:
	/* Deallocate the handle */
	if (!IS_ERR_OR_NULL(ihandle))
		ion_free(qseecom.ion_clnt, ihandle);
	return ret;
}

static int qseecom_unload_external_elf(struct qseecom_dev_handle *data)
{
	int ret = 0;
	struct qseecom_command_scm_resp resp;
	struct qseecom_unload_app_ireq req;

	/* unavailable client app */
	data->type = QSEECOM_UNAVAILABLE_CLIENT_APP;

	/* Populate the structure for sending scm call to unload image */
	req.qsee_cmd_id = QSEOS_UNLOAD_EXTERNAL_ELF_COMMAND;

	/* SCM_CALL to unload the external elf */
	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1, &req,
			sizeof(struct qseecom_unload_app_ireq),
			&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to unload failed : ret %d\n",
				ret);
		ret = -EFAULT;
		goto qseecom_unload_external_elf_scm_err;
	}
	if (resp.result == QSEOS_RESULT_INCOMPLETE) {
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret)
			pr_err("process_incomplete_cmd fail err: %d\n",
					ret);
	} else {
		if (resp.result != QSEOS_RESULT_SUCCESS) {
			pr_err("scm_call to unload image failed resp.result =%d\n",
						resp.result);
			ret = -EFAULT;
		}
	}

qseecom_unload_external_elf_scm_err:

	return ret;
}

static int qseecom_query_app_loaded(struct qseecom_dev_handle *data,
					void __user *argp)
{

	int32_t ret;
	struct qseecom_qseos_app_load_query query_req;
	struct qseecom_check_app_ireq req;
	struct qseecom_registered_app_list *entry = NULL;
	unsigned long flags = 0;
	uint32_t app_arch = 0, app_id = 0;
	bool found_app = false;

	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&query_req,
				(void __user *)argp,
				sizeof(struct qseecom_qseos_app_load_query))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	req.qsee_cmd_id = QSEOS_APP_LOOKUP_COMMAND;
	query_req.app_name[MAX_APP_NAME_SIZE-1] = '\0';
	strlcpy(req.app_name, query_req.app_name, MAX_APP_NAME_SIZE);

	ret = __qseecom_check_app_exists(req, &app_id);
	if (ret) {
		pr_err(" scm call to check if app is loaded failed");
		return ret;	/* scm call failed */
	}
	if (app_id) {
		pr_debug("App id %d (%s) already exists\n", app_id,
			(char *)(req.app_name));
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(entry,
				&qseecom.registered_app_list_head, list){
			if (entry->app_id == app_id) {
				app_arch = entry->app_arch;
				entry->ref_cnt++;
				found_app = true;
				break;
			}
		}
		spin_unlock_irqrestore(
				&qseecom.registered_app_list_lock, flags);
		data->client.app_id = app_id;
		query_req.app_id = app_id;
		if (app_arch) {
			data->client.app_arch = app_arch;
			query_req.app_arch = app_arch;
		} else {
			data->client.app_arch = 0;
			query_req.app_arch = 0;
		}
		strlcpy(data->client.app_name, query_req.app_name,
				MAX_APP_NAME_SIZE);
		/*
		 * If app was loaded by appsbl before and was not registered,
		 * regiser this app now.
		 */
		if (!found_app) {
			pr_debug("Register app %d [%s] which was loaded before\n",
					ret, (char *)query_req.app_name);
			entry = kmalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry) {
				pr_err("kmalloc for app entry failed\n");
				return  -ENOMEM;
			}
			entry->app_id = app_id;
			entry->ref_cnt = 1;
			entry->app_arch = data->client.app_arch;
			strlcpy(entry->app_name, data->client.app_name,
				MAX_APP_NAME_SIZE);
			entry->app_blocked = false;
			entry->blocked_on_listener_id = 0;
			spin_lock_irqsave(&qseecom.registered_app_list_lock,
				flags);
			list_add_tail(&entry->list,
				&qseecom.registered_app_list_head);
			spin_unlock_irqrestore(
				&qseecom.registered_app_list_lock, flags);
		}
		if (copy_to_user(argp, &query_req, sizeof(query_req))) {
			pr_err("copy_to_user failed\n");
			return -EFAULT;
		}
		return -EEXIST;	/* app already loaded */
	} else {
		return 0;	/* app not loaded */
	}
}

static int __qseecom_get_ce_pipe_info(
			enum qseecom_key_management_usage_type usage,
			uint32_t *pipe, uint32_t **ce_hw, uint32_t unit)
{
	int ret = -EINVAL;
	int i, j;
	struct qseecom_ce_info_use *p = NULL;
	int total = 0;
	struct qseecom_ce_pipe_entry *pcepipe;

	switch (usage) {
	case QSEOS_KM_USAGE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION:
		if (qseecom.support_fde) {
			p = qseecom.ce_info.fde;
			total = qseecom.ce_info.num_fde;
		} else {
			pr_err("system does not support fde\n");
			return -EINVAL;
		}
		break;
	case QSEOS_KM_USAGE_FILE_ENCRYPTION:
		if (qseecom.support_pfe) {
			p = qseecom.ce_info.pfe;
			total = qseecom.ce_info.num_pfe;
		} else {
			pr_err("system does not support pfe\n");
			return -EINVAL;
		}
		break;
	default:
		pr_err("unsupported usage %d\n", usage);
		return -EINVAL;
	}

	for (j = 0; j < total; j++) {
		if (p->unit_num == unit) {
			pcepipe =  p->ce_pipe_entry;
			for (i = 0; i < p->num_ce_pipe_entries; i++) {
				(*ce_hw)[i] = pcepipe->ce_num;
				*pipe = pcepipe->ce_pipe_pair;
				pcepipe++;
			}
			ret = 0;
			break;
		}
		p++;
	}
	return ret;
}

static int __qseecom_generate_and_save_key(struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_generate_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", usage);
		return -EFAULT;
	}
	ret = __qseecom_enable_clk(CLK_QSEE);
	if (ret)
		return ret;

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
				ireq, sizeof(struct qseecom_key_generate_ireq),
				&resp, sizeof(resp));
	if (ret) {
		if (ret == -EINVAL &&
			resp.result == QSEOS_RESULT_FAIL_KEY_ID_EXISTS) {
			pr_debug("Key ID exists.\n");
			ret = 0;
		} else {
			pr_err("scm call to generate key failed : %d\n", ret);
			ret = -EFAULT;
		}
		goto generate_key_exit;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_FAIL_KEY_ID_EXISTS:
		pr_debug("Key ID exists.\n");
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			if (resp.result == QSEOS_RESULT_FAIL_KEY_ID_EXISTS) {
				pr_debug("Key ID exists.\n");
				ret = 0;
			} else {
				pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
			}
		}
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("gen key scm call failed resp.result %d\n", resp.result);
		ret = -EINVAL;
		break;
	}
generate_key_exit:
	__qseecom_disable_clk(CLK_QSEE);
	return ret;
}

static int __qseecom_delete_saved_key(struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_delete_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", usage);
		return -EFAULT;
	}
	ret = __qseecom_enable_clk(CLK_QSEE);
	if (ret)
		return ret;

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
				ireq, sizeof(struct qseecom_key_delete_ireq),
				&resp, sizeof(struct qseecom_command_scm_resp));
	if (ret) {
		if (ret == -EINVAL &&
			resp.result == QSEOS_RESULT_FAIL_MAX_ATTEMPT) {
			pr_debug("Max attempts to input password reached.\n");
			ret = -ERANGE;
		} else {
			pr_err("scm call to delete key failed : %d\n", ret);
			ret = -EFAULT;
		}
		goto del_key_exit;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
			if (resp.result == QSEOS_RESULT_FAIL_MAX_ATTEMPT) {
				pr_debug("Max attempts to input password reached.\n");
				ret = -ERANGE;
			}
		}
		break;
	case QSEOS_RESULT_FAIL_MAX_ATTEMPT:
		pr_debug("Max attempts to input password reached.\n");
		ret = -ERANGE;
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("Delete key scm call failed resp.result %d\n",
							resp.result);
		ret = -EINVAL;
		break;
	}
del_key_exit:
	__qseecom_disable_clk(CLK_QSEE);
	return ret;
}

static int __qseecom_set_clear_ce_key(struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_select_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", usage);
		return -EFAULT;
	}
	ret = __qseecom_enable_clk(CLK_QSEE);
	if (ret)
		return ret;

	if (qseecom.qsee.instance != qseecom.ce_drv.instance) {
		ret = __qseecom_enable_clk(CLK_CE_DRV);
		if (ret)
			return ret;
	}

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
				ireq, sizeof(struct qseecom_key_select_ireq),
				&resp, sizeof(struct qseecom_command_scm_resp));
	if (ret) {
		if (ret == -EINVAL &&
			resp.result == QSEOS_RESULT_FAIL_MAX_ATTEMPT) {
			pr_debug("Max attempts to input password reached.\n");
			ret = -ERANGE;
		} else if (ret == -EINVAL &&
			resp.result == QSEOS_RESULT_FAIL_PENDING_OPERATION) {
			pr_debug("Set Key operation under processing...\n");
			ret = QSEOS_RESULT_FAIL_PENDING_OPERATION;
		} else {
			pr_err("scm call to set QSEOS_PIPE_ENC key failed : %d\n",
				ret);
			ret = -EFAULT;
		}
		goto set_key_exit;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
			if (resp.result ==
				QSEOS_RESULT_FAIL_PENDING_OPERATION) {
				pr_debug("Set Key operation under processing...\n");
				ret = QSEOS_RESULT_FAIL_PENDING_OPERATION;
			}
			if (resp.result == QSEOS_RESULT_FAIL_MAX_ATTEMPT) {
				pr_debug("Max attempts to input password reached.\n");
				ret = -ERANGE;
			}
		}
		break;
	case QSEOS_RESULT_FAIL_MAX_ATTEMPT:
		pr_debug("Max attempts to input password reached.\n");
		ret = -ERANGE;
		break;
	case QSEOS_RESULT_FAIL_PENDING_OPERATION:
		pr_debug("Set Key operation under processing...\n");
		ret = QSEOS_RESULT_FAIL_PENDING_OPERATION;
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("Set key scm call failed resp.result %d\n", resp.result);
		ret = -EINVAL;
		break;
	}
set_key_exit:
	__qseecom_disable_clk(CLK_QSEE);
	if (qseecom.qsee.instance != qseecom.ce_drv.instance)
		__qseecom_disable_clk(CLK_CE_DRV);
	return ret;
}

static int __qseecom_update_current_key_user_info(
			struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_userinfo_update_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
			pr_err("Error:: unsupported usage %d\n", usage);
			return -EFAULT;
	}
	ret = __qseecom_enable_clk(CLK_QSEE);
	if (ret)
		return ret;

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
		ireq, sizeof(struct qseecom_key_userinfo_update_ireq),
		&resp, sizeof(struct qseecom_command_scm_resp));
	if (ret) {
		if (ret == -EINVAL &&
			resp.result == QSEOS_RESULT_FAIL_PENDING_OPERATION) {
			pr_debug("Set Key operation under processing...\n");
			ret = QSEOS_RESULT_FAIL_PENDING_OPERATION;
		} else {
			pr_err("scm call to update key userinfo failed: %d\n",
									ret);
			__qseecom_disable_clk(CLK_QSEE);
			return -EFAULT;
		}
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (resp.result ==
			QSEOS_RESULT_FAIL_PENDING_OPERATION) {
			pr_debug("Set Key operation under processing...\n");
			ret = QSEOS_RESULT_FAIL_PENDING_OPERATION;
		}
		if (ret)
			pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
		break;
	case QSEOS_RESULT_FAIL_PENDING_OPERATION:
		pr_debug("Update Key operation under processing...\n");
		ret = QSEOS_RESULT_FAIL_PENDING_OPERATION;
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("Set key scm call failed resp.result %d\n", resp.result);
		ret = -EINVAL;
		break;
	}

	__qseecom_disable_clk(CLK_QSEE);
	return ret;
}


static int qseecom_enable_ice_setup(int usage)
{
	int ret = 0;

	if (usage == QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION)
		ret = qcom_ice_setup_ice_hw("ufs", true);
	else if (usage == QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION)
		ret = qcom_ice_setup_ice_hw("sdcc", true);

	return ret;
}

static int qseecom_disable_ice_setup(int usage)
{
	int ret = 0;

	if (usage == QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION)
		ret = qcom_ice_setup_ice_hw("ufs", false);
	else if (usage == QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION)
		ret = qcom_ice_setup_ice_hw("sdcc", false);

	return ret;
}

static int qseecom_get_ce_hw_instance(uint32_t unit, uint32_t usage)
{
	struct qseecom_ce_info_use *pce_info_use, *p;
	int total = 0;
	int i;

	switch (usage) {
	case QSEOS_KM_USAGE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION:
		p = qseecom.ce_info.fde;
		total = qseecom.ce_info.num_fde;
		break;
	case QSEOS_KM_USAGE_FILE_ENCRYPTION:
		p = qseecom.ce_info.pfe;
		total = qseecom.ce_info.num_pfe;
		break;
	default:
		pr_err("unsupported usage %d\n", usage);
		return -EINVAL;
	}

	pce_info_use = NULL;

	for (i = 0; i < total; i++) {
		if (p->unit_num == unit) {
			pce_info_use = p;
			break;
		}
		p++;
	}
	if (!pce_info_use) {
		pr_err("can not find %d\n", unit);
		return -EINVAL;
	}
	return pce_info_use->num_ce_pipe_entries;
}

static int qseecom_create_key(struct qseecom_dev_handle *data,
			void __user *argp)
{
	int i;
	uint32_t *ce_hw = NULL;
	uint32_t pipe = 0;
	int ret = 0;
	uint32_t flags = 0;
	struct qseecom_create_key_req create_key_req;
	struct qseecom_key_generate_ireq generate_key_ireq;
	struct qseecom_key_select_ireq set_key_ireq;
	uint32_t entries = 0;

	ret = copy_from_user(&create_key_req, argp, sizeof(create_key_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (create_key_req.usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		create_key_req.usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("unsupported usage %d\n", create_key_req.usage);
		ret = -EFAULT;
		return ret;
	}
	entries = qseecom_get_ce_hw_instance(DEFAULT_CE_INFO_UNIT,
					create_key_req.usage);
	if (entries <= 0) {
		pr_err("no ce instance for usage %d instance %d\n",
			DEFAULT_CE_INFO_UNIT, create_key_req.usage);
		ret = -EINVAL;
		return ret;
	}

	ce_hw = kcalloc(entries, sizeof(*ce_hw), GFP_KERNEL);
	if (!ce_hw) {
		ret = -ENOMEM;
		return ret;
	}
	ret = __qseecom_get_ce_pipe_info(create_key_req.usage, &pipe, &ce_hw,
			DEFAULT_CE_INFO_UNIT);
	if (ret) {
		pr_err("Failed to retrieve pipe/ce_hw info: %d\n", ret);
		ret = -EINVAL;
		goto free_buf;
	}

	if (qseecom.fde_key_size)
		flags |= QSEECOM_ICE_FDE_KEY_SIZE_32_BYTE;
	else
		flags |= QSEECOM_ICE_FDE_KEY_SIZE_16_BYTE;

	generate_key_ireq.flags = flags;
	generate_key_ireq.qsee_command_id = QSEOS_GENERATE_KEY;
	memset((void *)generate_key_ireq.key_id,
			0, QSEECOM_KEY_ID_SIZE);
	memset((void *)generate_key_ireq.hash32,
			0, QSEECOM_HASH_SIZE);
	memcpy((void *)generate_key_ireq.key_id,
			(void *)key_id_array[create_key_req.usage].desc,
			QSEECOM_KEY_ID_SIZE);
	memcpy((void *)generate_key_ireq.hash32,
			(void *)create_key_req.hash32,
			QSEECOM_HASH_SIZE);

	ret = __qseecom_generate_and_save_key(data,
			create_key_req.usage, &generate_key_ireq);
	if (ret) {
		pr_err("Failed to generate key on storage: %d\n", ret);
		goto free_buf;
	}

	for (i = 0; i < entries; i++) {
		set_key_ireq.qsee_command_id = QSEOS_SET_KEY;
		if (create_key_req.usage ==
				QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION) {
			set_key_ireq.ce = QSEECOM_UFS_ICE_CE_NUM;
			set_key_ireq.pipe = QSEECOM_ICE_FDE_KEY_INDEX;

		} else if (create_key_req.usage ==
				QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION) {
			set_key_ireq.ce = QSEECOM_SDCC_ICE_CE_NUM;
			set_key_ireq.pipe = QSEECOM_ICE_FDE_KEY_INDEX;

		} else {
			set_key_ireq.ce = ce_hw[i];
			set_key_ireq.pipe = pipe;
		}
		set_key_ireq.flags = flags;

		/* set both PIPE_ENC and PIPE_ENC_XTS*/
		set_key_ireq.pipe_type = QSEOS_PIPE_ENC|QSEOS_PIPE_ENC_XTS;
		memset((void *)set_key_ireq.key_id, 0, QSEECOM_KEY_ID_SIZE);
		memset((void *)set_key_ireq.hash32, 0, QSEECOM_HASH_SIZE);
		memcpy((void *)set_key_ireq.key_id,
			(void *)key_id_array[create_key_req.usage].desc,
			QSEECOM_KEY_ID_SIZE);
		memcpy((void *)set_key_ireq.hash32,
				(void *)create_key_req.hash32,
				QSEECOM_HASH_SIZE);

		/* It will return false if it is GPCE based crypto instance or
		   ICE is setup properly */
		if (qseecom_enable_ice_setup(create_key_req.usage))
			goto free_buf;

		do {
			ret = __qseecom_set_clear_ce_key(data,
					create_key_req.usage,
					&set_key_ireq);
			/* wait a little before calling scm again to let other
			   processes run */
			if (ret == QSEOS_RESULT_FAIL_PENDING_OPERATION)
				msleep(50);

		} while (ret == QSEOS_RESULT_FAIL_PENDING_OPERATION);

		qseecom_disable_ice_setup(create_key_req.usage);

		if (ret) {
			pr_err("Failed to create key: pipe %d, ce %d: %d\n",
				pipe, ce_hw[i], ret);
			goto free_buf;
		} else {
			pr_err("Set the key successfully\n");
			if ((create_key_req.usage ==
				QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION) ||
			     (create_key_req.usage ==
				QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION))
				goto free_buf;
		}
	}

free_buf:
	kzfree(ce_hw);
	return ret;
}

static int qseecom_wipe_key(struct qseecom_dev_handle *data,
				void __user *argp)
{
	uint32_t *ce_hw = NULL;
	uint32_t pipe = 0;
	int ret = 0;
	uint32_t flags = 0;
	int i, j;
	struct qseecom_wipe_key_req wipe_key_req;
	struct qseecom_key_delete_ireq delete_key_ireq;
	struct qseecom_key_select_ireq clear_key_ireq;
	uint32_t entries = 0;

	ret = copy_from_user(&wipe_key_req, argp, sizeof(wipe_key_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (wipe_key_req.usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		wipe_key_req.usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("unsupported usage %d\n", wipe_key_req.usage);
		ret = -EFAULT;
		return ret;
	}

	entries = qseecom_get_ce_hw_instance(DEFAULT_CE_INFO_UNIT,
					wipe_key_req.usage);
	if (entries <= 0) {
		pr_err("no ce instance for usage %d instance %d\n",
			DEFAULT_CE_INFO_UNIT, wipe_key_req.usage);
		ret = -EINVAL;
		return ret;
	}

	ce_hw = kcalloc(entries, sizeof(*ce_hw), GFP_KERNEL);
	if (!ce_hw) {
		ret = -ENOMEM;
		return ret;
	}

	ret = __qseecom_get_ce_pipe_info(wipe_key_req.usage, &pipe, &ce_hw,
				DEFAULT_CE_INFO_UNIT);
	if (ret) {
		pr_err("Failed to retrieve pipe/ce_hw info: %d\n", ret);
		ret = -EINVAL;
		goto free_buf;
	}

	if (wipe_key_req.wipe_key_flag) {
		delete_key_ireq.flags = flags;
		delete_key_ireq.qsee_command_id = QSEOS_DELETE_KEY;
		memset((void *)delete_key_ireq.key_id, 0, QSEECOM_KEY_ID_SIZE);
		memcpy((void *)delete_key_ireq.key_id,
			(void *)key_id_array[wipe_key_req.usage].desc,
			QSEECOM_KEY_ID_SIZE);
		memset((void *)delete_key_ireq.hash32, 0, QSEECOM_HASH_SIZE);

		ret = __qseecom_delete_saved_key(data, wipe_key_req.usage,
					&delete_key_ireq);
		if (ret) {
			pr_err("Failed to delete key from ssd storage: %d\n",
				ret);
			ret = -EFAULT;
			goto free_buf;
		}
	}

	for (j = 0; j < entries; j++) {
		clear_key_ireq.qsee_command_id = QSEOS_SET_KEY;
		if (wipe_key_req.usage ==
				QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION) {
			clear_key_ireq.ce = QSEECOM_UFS_ICE_CE_NUM;
			clear_key_ireq.pipe = QSEECOM_ICE_FDE_KEY_INDEX;
		} else if (wipe_key_req.usage ==
			QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION) {
			clear_key_ireq.ce = QSEECOM_SDCC_ICE_CE_NUM;
			clear_key_ireq.pipe = QSEECOM_ICE_FDE_KEY_INDEX;
		} else {
			clear_key_ireq.ce = ce_hw[j];
			clear_key_ireq.pipe = pipe;
		}
		clear_key_ireq.flags = flags;
		clear_key_ireq.pipe_type = QSEOS_PIPE_ENC|QSEOS_PIPE_ENC_XTS;
		for (i = 0; i < QSEECOM_KEY_ID_SIZE; i++)
			clear_key_ireq.key_id[i] = QSEECOM_INVALID_KEY_ID;
		memset((void *)clear_key_ireq.hash32, 0, QSEECOM_HASH_SIZE);

		/* It will return false if it is GPCE based crypto instance or
		   ICE is setup properly */
		if (qseecom_enable_ice_setup(wipe_key_req.usage))
			goto free_buf;

		ret = __qseecom_set_clear_ce_key(data, wipe_key_req.usage,
					&clear_key_ireq);

		qseecom_disable_ice_setup(wipe_key_req.usage);

		if (ret) {
			pr_err("Failed to wipe key: pipe %d, ce %d: %d\n",
				pipe, ce_hw[j], ret);
			ret = -EFAULT;
			goto free_buf;
		}
	}

free_buf:
	kzfree(ce_hw);
	return ret;
}

static int qseecom_update_key_user_info(struct qseecom_dev_handle *data,
			void __user *argp)
{
	int ret = 0;
	uint32_t flags = 0;
	struct qseecom_update_key_userinfo_req update_key_req;
	struct qseecom_key_userinfo_update_ireq ireq;

	ret = copy_from_user(&update_key_req, argp, sizeof(update_key_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (update_key_req.usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		update_key_req.usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", update_key_req.usage);
		return -EFAULT;
	}

	ireq.qsee_command_id = QSEOS_UPDATE_KEY_USERINFO;

	if (qseecom.fde_key_size)
		flags |= QSEECOM_ICE_FDE_KEY_SIZE_32_BYTE;
	else
		flags |= QSEECOM_ICE_FDE_KEY_SIZE_16_BYTE;

	ireq.flags = flags;
	memset(ireq.key_id, 0, QSEECOM_KEY_ID_SIZE);
	memset((void *)ireq.current_hash32, 0, QSEECOM_HASH_SIZE);
	memset((void *)ireq.new_hash32, 0, QSEECOM_HASH_SIZE);
	memcpy((void *)ireq.key_id,
		(void *)key_id_array[update_key_req.usage].desc,
		QSEECOM_KEY_ID_SIZE);
	memcpy((void *)ireq.current_hash32,
		(void *)update_key_req.current_hash32, QSEECOM_HASH_SIZE);
	memcpy((void *)ireq.new_hash32,
		(void *)update_key_req.new_hash32, QSEECOM_HASH_SIZE);

	do {
		ret = __qseecom_update_current_key_user_info(data,
						update_key_req.usage,
						&ireq);
		/* wait a little before calling scm again to let other
		   processes run */
		if (ret == QSEOS_RESULT_FAIL_PENDING_OPERATION)
			msleep(50);

	} while (ret == QSEOS_RESULT_FAIL_PENDING_OPERATION);
	if (ret) {
		pr_err("Failed to update key info: %d\n", ret);
		return ret;
	}
	return ret;

}
static int qseecom_is_es_activated(void __user *argp)
{
	struct qseecom_is_es_activated_req req;
	struct qseecom_command_scm_resp resp;
	int ret;

	if (qseecom.qsee_version < QSEE_VERSION_04) {
		pr_err("invalid qsee version\n");
		return -ENODEV;
	}

	if (argp == NULL) {
		pr_err("arg is null\n");
		return -EINVAL;
	}

	ret = qseecom_scm_call(SCM_SVC_ES, SCM_IS_ACTIVATED_ID,
		&req, sizeof(req), &resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call failed\n");
		return ret;
	}

	req.is_activated = resp.result;
	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		pr_err("copy_to_user failed\n");
		return ret;
	}

	return 0;
}

static int qseecom_save_partition_hash(void __user *argp)
{
	struct qseecom_save_partition_hash_req req;
	struct qseecom_command_scm_resp resp;
	int ret;

	memset(&resp, 0x00, sizeof(resp));

	if (qseecom.qsee_version < QSEE_VERSION_04) {
		pr_err("invalid qsee version\n");
		return -ENODEV;
	}

	if (argp == NULL) {
		pr_err("arg is null\n");
		return -EINVAL;
	}

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	ret = qseecom_scm_call(SCM_SVC_ES, SCM_SAVE_PARTITION_HASH_ID,
		       (void *)&req, sizeof(req), (void *)&resp, sizeof(resp));
	if (ret) {
		pr_err("qseecom_scm_call failed\n");
		return ret;
	}

	return 0;
}

static int qseecom_mdtp_cipher_dip(void __user *argp)
{
	struct qseecom_mdtp_cipher_dip_req req;
	u32 tzbuflenin, tzbuflenout;
	char *tzbufin = NULL, *tzbufout = NULL;
	struct scm_desc desc = {0};
	int ret;

	do {
		/* Copy the parameters from userspace */
		if (argp == NULL) {
			pr_err("arg is null\n");
			ret = -EINVAL;
			break;
		}

		ret = copy_from_user(&req, argp, sizeof(req));
		if (ret) {
			pr_err("copy_from_user failed, ret= %d\n", ret);
			break;
		}

		if (req.in_buf == NULL || req.out_buf == NULL ||
			req.in_buf_size == 0 || req.in_buf_size > MAX_DIP ||
			req.out_buf_size == 0 || req.out_buf_size > MAX_DIP ||
				req.direction > 1) {
				pr_err("invalid parameters\n");
				ret = -EINVAL;
				break;
		}

		/* Copy the input buffer from userspace to kernel space */
		tzbuflenin = PAGE_ALIGN(req.in_buf_size);
		tzbufin = kzalloc(tzbuflenin, GFP_KERNEL);
		if (!tzbufin) {
			pr_err("error allocating in buffer\n");
			ret = -ENOMEM;
			break;
		}

		ret = copy_from_user(tzbufin, req.in_buf, req.in_buf_size);
		if (ret) {
			pr_err("copy_from_user failed, ret=%d\n", ret);
			break;
		}

		dmac_flush_range(tzbufin, tzbufin + tzbuflenin);

		/* Prepare the output buffer in kernel space */
		tzbuflenout = PAGE_ALIGN(req.out_buf_size);
		tzbufout = kzalloc(tzbuflenout, GFP_KERNEL);
		if (!tzbufout) {
			pr_err("error allocating out buffer\n");
			ret = -ENOMEM;
			break;
		}

		dmac_flush_range(tzbufout, tzbufout + tzbuflenout);

		/* Send the command to TZ */
		desc.arginfo = TZ_MDTP_CIPHER_DIP_ID_PARAM_ID;
		desc.args[0] = virt_to_phys(tzbufin);
		desc.args[1] = req.in_buf_size;
		desc.args[2] = virt_to_phys(tzbufout);
		desc.args[3] = req.out_buf_size;
		desc.args[4] = req.direction;

		ret = __qseecom_enable_clk(CLK_QSEE);
		if (ret)
			break;

		ret = scm_call2(TZ_MDTP_CIPHER_DIP_ID, &desc);

		__qseecom_disable_clk(CLK_QSEE);

		if (ret) {
			pr_err("scm_call2 failed for SCM_SVC_MDTP, ret=%d\n",
				ret);
			break;
		}

		/* Copy the output buffer from kernel space to userspace */
		dmac_flush_range(tzbufout, tzbufout + tzbuflenout);
		ret = copy_to_user(req.out_buf, tzbufout, req.out_buf_size);
		if (ret) {
			pr_err("copy_to_user failed, ret=%d\n", ret);
			break;
		}
	} while (0);

	kzfree(tzbufin);
	kzfree(tzbufout);

	return ret;
}

static int __qseecom_qteec_validate_msg(struct qseecom_dev_handle *data,
				struct qseecom_qteec_req *req)
{
	if (!data || !data->client.ihandle) {
		pr_err("Client or client handle is not initialized\n");
		return -EINVAL;
	}

	if (data->type != QSEECOM_CLIENT_APP)
		return -EFAULT;

	if (req->req_len > UINT_MAX - req->resp_len) {
		pr_err("Integer overflow detected in req_len & rsp_len\n");
		return -EINVAL;
	}

	if (req->req_len + req->resp_len > data->client.sb_length) {
		pr_debug("Not enough memory to fit cmd_buf.\n");
		pr_debug("resp_buf. Required: %u, Available: %zu\n",
		(req->req_len + req->resp_len), data->client.sb_length);
		return -ENOMEM;
	}

	if (req->req_ptr == NULL || req->resp_ptr == NULL) {
		pr_err("cmd buffer or response buffer is null\n");
		return -EINVAL;
	}
	if (((uintptr_t)req->req_ptr <
			data->client.user_virt_sb_base) ||
		((uintptr_t)req->req_ptr >=
		(data->client.user_virt_sb_base + data->client.sb_length))) {
		pr_err("cmd buffer address not within shared bufffer\n");
		return -EINVAL;
	}

	if (((uintptr_t)req->resp_ptr <
			data->client.user_virt_sb_base)  ||
		((uintptr_t)req->resp_ptr >=
		(data->client.user_virt_sb_base + data->client.sb_length))) {
		pr_err("response buffer address not within shared bufffer\n");
		return -EINVAL;
	}

	if ((req->req_len == 0) || (req->resp_len == 0)) {
		pr_err("cmd buf lengtgh/response buf length not valid\n");
		return -EINVAL;
	}

	if ((uintptr_t)req->req_ptr > (ULONG_MAX - req->req_len)) {
		pr_err("Integer overflow in req_len & req_ptr\n");
		return -EINVAL;
	}

	if ((uintptr_t)req->resp_ptr > (ULONG_MAX - req->resp_len)) {
		pr_err("Integer overflow in resp_len & resp_ptr\n");
		return -EINVAL;
	}

	if (data->client.user_virt_sb_base >
					(ULONG_MAX - data->client.sb_length)) {
		pr_err("Integer overflow in user_virt_sb_base & sb_length\n");
		return -EINVAL;
	}
	if ((((uintptr_t)req->req_ptr + req->req_len) >
		((uintptr_t)data->client.user_virt_sb_base +
						data->client.sb_length)) ||
		(((uintptr_t)req->resp_ptr + req->resp_len) >
		((uintptr_t)data->client.user_virt_sb_base +
						data->client.sb_length))) {
		pr_err("cmd buf or resp buf is out of shared buffer region\n");
		return -EINVAL;
	}
	return 0;
}

static int __qseecom_qteec_handle_pre_alc_fd(struct qseecom_dev_handle *data,
				uint32_t fd_idx, struct sg_table *sg_ptr)
{
	struct scatterlist *sg = sg_ptr->sgl;
	struct qseecom_sg_entry *sg_entry;
	void *buf;
	uint i;
	size_t size;
	dma_addr_t coh_pmem;

	if (fd_idx >= MAX_ION_FD) {
		pr_err("fd_idx [%d] is invalid\n", fd_idx);
		return -ENOMEM;
	}
	/*
	* Allocate a buffer, populate it with number of entry plus
	* each sg entry's phy addr and lenth; then return the
	* phy_addr of the buffer.
	*/
	size = sizeof(uint32_t) +
		sizeof(struct qseecom_sg_entry) * sg_ptr->nents;
	size = (size + PAGE_SIZE) & PAGE_MASK;
	buf = dma_alloc_coherent(qseecom.pdev,
			size, &coh_pmem, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("failed to alloc memory for sg buf\n");
		return -ENOMEM;
	}
	*(uint32_t *)buf = sg_ptr->nents;
	sg_entry = (struct qseecom_sg_entry *) (buf + sizeof(uint32_t));
	for (i = 0; i < sg_ptr->nents; i++) {
		sg_entry->phys_addr = (uint32_t)sg_dma_address(sg);
		sg_entry->len = sg->length;
		sg_entry++;
		sg = sg_next(sg);
	}
	data->client.sec_buf_fd[fd_idx].is_sec_buf_fd = true;
	data->client.sec_buf_fd[fd_idx].vbase = buf;
	data->client.sec_buf_fd[fd_idx].pbase = coh_pmem;
	data->client.sec_buf_fd[fd_idx].size = size;
	return 0;
}

static int __qseecom_update_qteec_req_buf(struct qseecom_qteec_modfd_req *req,
			struct qseecom_dev_handle *data, bool cleanup)
{
	struct ion_handle *ihandle;
	int ret = 0;
	int i = 0;
	uint32_t *update;
	struct sg_table *sg_ptr = NULL;
	struct scatterlist *sg;
	struct qseecom_param_memref *memref;

	if (req == NULL) {
		pr_err("Invalid address\n");
		return -EINVAL;
	}
	for (i = 0; i < MAX_ION_FD; i++) {
		if (req->ifd_data[i].fd > 0) {
			ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					req->ifd_data[i].fd);
			if (IS_ERR_OR_NULL(ihandle)) {
				pr_err("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			if ((req->req_len < sizeof(uint32_t)) ||
				(req->ifd_data[i].cmd_buf_offset >
				req->req_len - sizeof(uint32_t))) {
				pr_err("Invalid offset/req len 0x%x/0x%x\n",
					req->req_len,
					req->ifd_data[i].cmd_buf_offset);
				return -EINVAL;
			}
			update = (uint32_t *)((char *) req->req_ptr +
				req->ifd_data[i].cmd_buf_offset);
			if (!update) {
				pr_err("update pointer is NULL\n");
				return -EINVAL;
			}
		} else {
			continue;
		}
		/* Populate the cmd data structure with the phys_addr */
		sg_ptr = ion_sg_table(qseecom.ion_clnt, ihandle);
		if (IS_ERR_OR_NULL(sg_ptr)) {
			pr_err("IOn client could not retrieve sg table\n");
			goto err;
		}
		sg = sg_ptr->sgl;
		if (sg == NULL) {
			pr_err("sg is NULL\n");
			goto err;
		}
		if ((sg_ptr->nents == 0) || (sg->length == 0)) {
			pr_err("Num of scat entr (%d)or length(%d) invalid\n",
					sg_ptr->nents, sg->length);
			goto err;
		}
		/* clean up buf for pre-allocated fd */
		if (cleanup && data->client.sec_buf_fd[i].is_sec_buf_fd &&
			(*update)) {
			if (data->client.sec_buf_fd[i].vbase)
				dma_free_coherent(qseecom.pdev,
					data->client.sec_buf_fd[i].size,
					data->client.sec_buf_fd[i].vbase,
					data->client.sec_buf_fd[i].pbase);
			memset((void *)update, 0,
				sizeof(struct qseecom_param_memref));
			memset(&(data->client.sec_buf_fd[i]), 0,
				sizeof(struct qseecom_sec_buf_fd_info));
			goto clean;
		}

		if (*update == 0) {
			/* update buf for pre-allocated fd from secure heap*/
			ret = __qseecom_qteec_handle_pre_alc_fd(data, i,
				sg_ptr);
			if (ret) {
				pr_err("Failed to handle buf for fd[%d]\n", i);
				goto err;
			}
			memref = (struct qseecom_param_memref *)update;
			memref->buffer =
				(uint32_t)(data->client.sec_buf_fd[i].pbase);
			memref->size =
				(uint32_t)(data->client.sec_buf_fd[i].size);
		} else {
			/* update buf for fd from non-secure qseecom heap */
			if (sg_ptr->nents != 1) {
				pr_err("Num of scat entr (%d) invalid\n",
					sg_ptr->nents);
				goto err;
			}
			if (cleanup)
				*update = 0;
			else
				*update = (uint32_t)sg_dma_address(sg_ptr->sgl);
		}
clean:
		if (cleanup) {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
				ihandle, NULL, sg->length,
				ION_IOC_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				goto err;
			}
		} else {
			ret = msm_ion_do_cache_op(qseecom.ion_clnt,
				ihandle, NULL, sg->length,
				ION_IOC_CLEAN_INV_CACHES);
			if (ret) {
				pr_err("cache operation failed %d\n", ret);
				goto err;
			}
			data->sglistinfo_ptr[i].indexAndFlags =
				SGLISTINFO_SET_INDEX_FLAG(
				(sg_ptr->nents == 1), 0,
				req->ifd_data[i].cmd_buf_offset);
			data->sglistinfo_ptr[i].sizeOrCount =
				(sg_ptr->nents == 1) ?
				sg->length : sg_ptr->nents;
			data->sglist_cnt = i + 1;
		}
		/* Deallocate the handle */
		if (!IS_ERR_OR_NULL(ihandle))
			ion_free(qseecom.ion_clnt, ihandle);
	}
	return ret;
err:
	if (!IS_ERR_OR_NULL(ihandle))
		ion_free(qseecom.ion_clnt, ihandle);
	return -ENOMEM;
}

static int __qseecom_qteec_issue_cmd(struct qseecom_dev_handle *data,
				struct qseecom_qteec_req *req, uint32_t cmd_id)
{
	struct qseecom_command_scm_resp resp;
	struct qseecom_qteec_ireq ireq;
	struct qseecom_qteec_64bit_ireq ireq_64bit;
	struct qseecom_registered_app_list *ptr_app;
	bool found_app = false;
	unsigned long flags;
	int ret = 0;
	uint32_t reqd_len_sb_in = 0;
	void *cmd_buf = NULL;
	size_t cmd_len;
	struct sglist_info *table = data->sglistinfo_ptr;
	void *req_ptr = NULL;
	void *resp_ptr = NULL;

	ret  = __qseecom_qteec_validate_msg(data, req);
	if (ret)
		return ret;

	req_ptr = req->req_ptr;
	resp_ptr = req->resp_ptr;

	/* find app_id & img_name from list */
	spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
	list_for_each_entry(ptr_app, &qseecom.registered_app_list_head,
							list) {
		if ((ptr_app->app_id == data->client.app_id) &&
			 (!strcmp(ptr_app->app_name, data->client.app_name))) {
			found_app = true;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_app_list_lock, flags);
	if (!found_app) {
		pr_err("app_id %d (%s) is not found\n", data->client.app_id,
			(char *)data->client.app_name);
		return -ENOENT;
	}

	req->req_ptr = (void *)__qseecom_uvirt_to_kvirt(data,
						(uintptr_t)req->req_ptr);
	req->resp_ptr = (void *)__qseecom_uvirt_to_kvirt(data,
						(uintptr_t)req->resp_ptr);

	if ((cmd_id == QSEOS_TEE_OPEN_SESSION) ||
			(cmd_id == QSEOS_TEE_REQUEST_CANCELLATION)) {
		ret = __qseecom_update_qteec_req_buf(
			(struct qseecom_qteec_modfd_req *)req, data, false);
		if (ret)
			return ret;
	}

	if (qseecom.qsee_version < QSEE_VERSION_40) {
		ireq.app_id = data->client.app_id;
		ireq.req_ptr = (uint32_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)req_ptr);
		ireq.req_len = req->req_len;
		ireq.resp_ptr = (uint32_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)resp_ptr);
		ireq.resp_len = req->resp_len;
		ireq.sglistinfo_ptr = (uint32_t)virt_to_phys(table);
		ireq.sglistinfo_len = SGLISTINFO_TABLE_SIZE;
		dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
		cmd_buf = (void *)&ireq;
		cmd_len = sizeof(struct qseecom_qteec_ireq);
	} else {
		ireq_64bit.app_id = data->client.app_id;
		ireq_64bit.req_ptr = (uint64_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)req_ptr);
		ireq_64bit.req_len = req->req_len;
		ireq_64bit.resp_ptr = (uint64_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)resp_ptr);
		ireq_64bit.resp_len = req->resp_len;
		if ((data->client.app_arch == ELFCLASS32) &&
			((ireq_64bit.req_ptr >=
				PHY_ADDR_4G - ireq_64bit.req_len) ||
			(ireq_64bit.resp_ptr >=
				PHY_ADDR_4G - ireq_64bit.resp_len))){
			pr_err("32bit app %s (id: %d): phy_addr exceeds 4G\n",
				data->client.app_name, data->client.app_id);
			pr_err("req_ptr:%llx,req_len:%x,rsp_ptr:%llx,rsp_len:%x\n",
				ireq_64bit.req_ptr, ireq_64bit.req_len,
				ireq_64bit.resp_ptr, ireq_64bit.resp_len);
			return -EFAULT;
		}
		ireq_64bit.sglistinfo_ptr = (uint64_t)virt_to_phys(table);
		ireq_64bit.sglistinfo_len = SGLISTINFO_TABLE_SIZE;
		dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
		cmd_buf = (void *)&ireq_64bit;
		cmd_len = sizeof(struct qseecom_qteec_64bit_ireq);
	}
	if (qseecom.whitelist_support == true
		&& cmd_id == QSEOS_TEE_OPEN_SESSION)
		*(uint32_t *)cmd_buf = QSEOS_TEE_OPEN_SESSION_WHITELIST;
	else
		*(uint32_t *)cmd_buf = cmd_id;

	reqd_len_sb_in = req->req_len + req->resp_len;
	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
					data->client.sb_virt,
					reqd_len_sb_in,
					ION_IOC_CLEAN_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		return ret;
	}

	__qseecom_reentrancy_check_if_this_app_blocked(ptr_app);

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
				cmd_buf, cmd_len,
				&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call() failed with err: %d (app_id = %d)\n",
					ret, data->client.app_id);
		return ret;
	}

	if (qseecom.qsee_reentrancy_support) {
		ret = __qseecom_process_reentrancy(&resp, ptr_app, data);
	} else {
		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			ret = __qseecom_process_incomplete_cmd(data, &resp);
			if (ret) {
				pr_err("process_incomplete_cmd failed err: %d\n",
						ret);
				return ret;
			}
		} else {
			if (resp.result != QSEOS_RESULT_SUCCESS) {
				pr_err("Response result %d not supported\n",
								resp.result);
				ret = -EINVAL;
			}
		}
	}
	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		return ret;
	}

	if ((cmd_id == QSEOS_TEE_OPEN_SESSION) ||
			(cmd_id == QSEOS_TEE_REQUEST_CANCELLATION)) {
		ret = __qseecom_update_qteec_req_buf(
			(struct qseecom_qteec_modfd_req *)req, data, true);
		if (ret)
			return ret;
	}
	return 0;
}

static int qseecom_qteec_open_session(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct qseecom_qteec_modfd_req req;
	int ret = 0;

	ret = copy_from_user(&req, argp,
				sizeof(struct qseecom_qteec_modfd_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	ret = __qseecom_qteec_issue_cmd(data, (struct qseecom_qteec_req *)&req,
							QSEOS_TEE_OPEN_SESSION);

	return ret;
}

static int qseecom_qteec_close_session(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct qseecom_qteec_req req;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(struct qseecom_qteec_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	ret = __qseecom_qteec_issue_cmd(data, &req, QSEOS_TEE_CLOSE_SESSION);
	return ret;
}

static int qseecom_qteec_invoke_modfd_cmd(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct qseecom_qteec_modfd_req req;
	struct qseecom_command_scm_resp resp;
	struct qseecom_qteec_ireq ireq;
	struct qseecom_qteec_64bit_ireq ireq_64bit;
	struct qseecom_registered_app_list *ptr_app;
	bool found_app = false;
	unsigned long flags;
	int ret = 0;
	int i = 0;
	uint32_t reqd_len_sb_in = 0;
	void *cmd_buf = NULL;
	size_t cmd_len;
	struct sglist_info *table = data->sglistinfo_ptr;
	void *req_ptr = NULL;
	void *resp_ptr = NULL;

	ret = copy_from_user(&req, argp,
			sizeof(struct qseecom_qteec_modfd_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	ret = __qseecom_qteec_validate_msg(data,
					(struct qseecom_qteec_req *)(&req));
	if (ret)
		return ret;
	req_ptr = req.req_ptr;
	resp_ptr = req.resp_ptr;

	/* find app_id & img_name from list */
	spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
	list_for_each_entry(ptr_app, &qseecom.registered_app_list_head,
							list) {
		if ((ptr_app->app_id == data->client.app_id) &&
			 (!strcmp(ptr_app->app_name, data->client.app_name))) {
			found_app = true;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_app_list_lock, flags);
	if (!found_app) {
		pr_err("app_id %d (%s) is not found\n", data->client.app_id,
			(char *)data->client.app_name);
		return -ENOENT;
	}

	/* validate offsets */
	for (i = 0; i < MAX_ION_FD; i++) {
		if (req.ifd_data[i].fd) {
			if (req.ifd_data[i].cmd_buf_offset >= req.req_len)
				return -EINVAL;
		}
	}
	req.req_ptr = (void *)__qseecom_uvirt_to_kvirt(data,
						(uintptr_t)req.req_ptr);
	req.resp_ptr = (void *)__qseecom_uvirt_to_kvirt(data,
						(uintptr_t)req.resp_ptr);
	ret = __qseecom_update_qteec_req_buf(&req, data, false);
	if (ret)
		return ret;

	if (qseecom.qsee_version < QSEE_VERSION_40) {
		ireq.app_id = data->client.app_id;
		ireq.req_ptr = (uint32_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)req_ptr);
		ireq.req_len = req.req_len;
		ireq.resp_ptr = (uint32_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)resp_ptr);
		ireq.resp_len = req.resp_len;
		cmd_buf = (void *)&ireq;
		cmd_len = sizeof(struct qseecom_qteec_ireq);
		ireq.sglistinfo_ptr = (uint32_t)virt_to_phys(table);
		ireq.sglistinfo_len = SGLISTINFO_TABLE_SIZE;
		dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
	} else {
		ireq_64bit.app_id = data->client.app_id;
		ireq_64bit.req_ptr = (uint64_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)req_ptr);
		ireq_64bit.req_len = req.req_len;
		ireq_64bit.resp_ptr = (uint64_t)__qseecom_uvirt_to_kphys(data,
						(uintptr_t)resp_ptr);
		ireq_64bit.resp_len = req.resp_len;
		cmd_buf = (void *)&ireq_64bit;
		cmd_len = sizeof(struct qseecom_qteec_64bit_ireq);
		ireq_64bit.sglistinfo_ptr = (uint64_t)virt_to_phys(table);
		ireq_64bit.sglistinfo_len = SGLISTINFO_TABLE_SIZE;
		dmac_flush_range((void *)table,
				(void *)table + SGLISTINFO_TABLE_SIZE);
	}
	reqd_len_sb_in = req.req_len + req.resp_len;
	if (qseecom.whitelist_support == true)
		*(uint32_t *)cmd_buf = QSEOS_TEE_INVOKE_COMMAND_WHITELIST;
	else
		*(uint32_t *)cmd_buf = QSEOS_TEE_INVOKE_COMMAND;

	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
					data->client.sb_virt,
					reqd_len_sb_in,
					ION_IOC_CLEAN_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		return ret;
	}

	__qseecom_reentrancy_check_if_this_app_blocked(ptr_app);

	ret = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
				cmd_buf, cmd_len,
				&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call() failed with err: %d (app_id = %d)\n",
					ret, data->client.app_id);
		return ret;
	}

	if (qseecom.qsee_reentrancy_support) {
		ret = __qseecom_process_reentrancy(&resp, ptr_app, data);
	} else {
		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			ret = __qseecom_process_incomplete_cmd(data, &resp);
			if (ret) {
				pr_err("process_incomplete_cmd failed err: %d\n",
						ret);
				return ret;
			}
		} else {
			if (resp.result != QSEOS_RESULT_SUCCESS) {
				pr_err("Response result %d not supported\n",
								resp.result);
				ret = -EINVAL;
			}
		}
	}
	ret = __qseecom_update_qteec_req_buf(&req, data, true);
	if (ret)
		return ret;

	ret = msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_INV_CACHES);
	if (ret) {
		pr_err("cache operation failed %d\n", ret);
		return ret;
	}
	return 0;
}

static int qseecom_qteec_request_cancellation(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct qseecom_qteec_modfd_req req;
	int ret = 0;

	ret = copy_from_user(&req, argp,
				sizeof(struct qseecom_qteec_modfd_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	ret = __qseecom_qteec_issue_cmd(data, (struct qseecom_qteec_req *)&req,
						QSEOS_TEE_REQUEST_CANCELLATION);

	return ret;
}

static void __qseecom_clean_data_sglistinfo(struct qseecom_dev_handle *data)
{
	if (data->sglist_cnt) {
		memset(data->sglistinfo_ptr, 0,
			SGLISTINFO_TABLE_SIZE);
		data->sglist_cnt = 0;
	}
}


static int __qseecom_bus_scaling_enable(struct qseecom_dev_handle *data,
					bool *perf_enabled)
{
	int ret = 0;

	if (qseecom.support_bus_scaling) {
		if (!data->mode) {
			mutex_lock(&qsee_bw_mutex);
			__qseecom_register_bus_bandwidth_needs(
							data, HIGH);
			mutex_unlock(&qsee_bw_mutex);
		}
		ret = qseecom_scale_bus_bandwidth_timer(INACTIVE);
		if (ret) {
			pr_err("Failed to set bw\n");
			ret = -EINVAL;
			goto exit;
		}
	}
	/*
	* On targets where crypto clock is handled by HLOS,
	* if clk_access_cnt is zero and perf_enabled is false,
	* then the crypto clock was not enabled before sending cmd
	* to tz, qseecom will enable the clock to avoid service failure.
	*/
	if (!qseecom.no_clock_support &&
		!qseecom.qsee.clk_access_cnt && !data->perf_enabled) {
		pr_debug("ce clock is not enabled\n");
		ret = qseecom_perf_enable(data);
		if (ret) {
			pr_err("Failed to vote for clock with err %d\n",
					ret);
			ret = -EINVAL;
			goto exit;
		}
		*perf_enabled = true;
	}
exit:
	return ret;
}

static void __qseecom_bus_scaling_disable(struct qseecom_dev_handle *data,
					bool perf_enabled)
{
	if (qseecom.support_bus_scaling)
		__qseecom_add_bw_scale_down_timer(
			QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
	if (perf_enabled) {
		qsee_disable_clock_vote(data, CLK_DFAB);
		qsee_disable_clock_vote(data, CLK_SFPB);
	}
}

long qseecom_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret = 0;
	struct qseecom_dev_handle *data = file->private_data;
	void __user *argp = (void __user *) arg;
	bool perf_enabled = false;

	if (!data) {
		pr_err("Invalid/uninitialized device handle\n");
		return -EINVAL;
	}

	if (data->abort) {
		pr_err("Aborting qseecom driver\n");
		return -ENODEV;
	}

	switch (cmd) {
	case QSEECOM_IOCTL_REGISTER_LISTENER_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("reg lstnr req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		pr_debug("ioctl register_listener_req()\n");
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		data->type = QSEECOM_LISTENER_SERVICE;
		ret = qseecom_register_listener(data, argp);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed qseecom_register_listener: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("unreg lstnr req: invalid handle (%d) lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		pr_debug("ioctl unregister_listener_req()\n");
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_unregister_listener(data);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed qseecom_unregister_listener: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SEND_CMD_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("send cmd req: invalid handle (%d) app_id(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		ret = __qseecom_bus_scaling_enable(data, &perf_enabled);
		if (ret) {
			mutex_unlock(&app_access_lock);
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_send_cmd(data, argp);
		__qseecom_bus_scaling_disable(data, perf_enabled);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed qseecom_send_cmd: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SEND_MODFD_CMD_REQ:
	case QSEECOM_IOCTL_SEND_MODFD_CMD_64_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("send mdfd cmd: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		ret = __qseecom_bus_scaling_enable(data, &perf_enabled);
		if (ret) {
			mutex_unlock(&app_access_lock);
			break;
		}
		atomic_inc(&data->ioctl_count);
		if (cmd == QSEECOM_IOCTL_SEND_MODFD_CMD_REQ)
			ret = qseecom_send_modfd_cmd(data, argp);
		else
			ret = qseecom_send_modfd_cmd_64(data, argp);
		__qseecom_bus_scaling_disable(data, perf_enabled);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed qseecom_send_cmd: %d\n", ret);
		__qseecom_clean_data_sglistinfo(data);
		break;
	}
	case QSEECOM_IOCTL_RECEIVE_REQ: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("receive req: invalid handle (%d), lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_receive_req(data);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret && (ret != -ERESTARTSYS))
			pr_err("failed qseecom_receive_req: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SEND_RESP_REQ: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("send resp req: invalid handle (%d), lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		if (!qseecom.qsee_reentrancy_support)
			ret = qseecom_send_resp();
		else
			ret = qseecom_reentrancy_send_resp(data);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret)
			pr_err("failed qseecom_send_resp: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SET_MEM_PARAM_REQ: {
		if ((data->type != QSEECOM_CLIENT_APP) &&
			(data->type != QSEECOM_GENERIC) &&
			(data->type != QSEECOM_SECURE_SERVICE)) {
			pr_err("set mem param req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		pr_debug("SET_MEM_PARAM: qseecom addr = 0x%pK\n", data);
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_set_client_mem_param(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed Qqseecom_set_mem_param request: %d\n",
								ret);
		break;
	}
	case QSEECOM_IOCTL_LOAD_APP_REQ: {
		if ((data->type != QSEECOM_GENERIC) &&
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("load app req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->type = QSEECOM_CLIENT_APP;
		pr_debug("LOAD_APP_REQ: qseecom_addr = 0x%pK\n", data);
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_load_app(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed load_app request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_UNLOAD_APP_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("unload app req:invalid handle(%d) app_id(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		pr_debug("UNLOAD_APP: qseecom_addr = 0x%pK\n", data);
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_unload_app(data, false);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed unload_app request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ: {
		atomic_inc(&data->ioctl_count);
		ret = qseecom_get_qseos_version(data, argp);
		if (ret)
			pr_err("qseecom_get_qseos_version: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_PERF_ENABLE_REQ:{
		if ((data->type != QSEECOM_GENERIC) &&
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("perf enable req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if ((data->type == QSEECOM_CLIENT_APP) &&
			(data->client.app_id == 0)) {
			pr_err("perf enable req:invalid handle(%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		if (qseecom.support_bus_scaling) {
			mutex_lock(&qsee_bw_mutex);
			__qseecom_register_bus_bandwidth_needs(data, HIGH);
			mutex_unlock(&qsee_bw_mutex);
		} else {
			ret = qseecom_perf_enable(data);
			if (ret)
				pr_err("Fail to vote for clocks %d\n", ret);
		}
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_PERF_DISABLE_REQ:{
		if ((data->type != QSEECOM_SECURE_SERVICE) &&
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("perf disable req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if ((data->type == QSEECOM_CLIENT_APP) &&
			(data->client.app_id == 0)) {
			pr_err("perf disable: invalid handle (%d)app_id(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		if (!qseecom.support_bus_scaling) {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		} else {
			mutex_lock(&qsee_bw_mutex);
			qseecom_unregister_bus_bandwidth_needs(data);
			mutex_unlock(&qsee_bw_mutex);
		}
		atomic_dec(&data->ioctl_count);
		break;
	}

	case QSEECOM_IOCTL_SET_BUS_SCALING_REQ: {
		/* If crypto clock is not handled by HLOS, return directly. */
		if (qseecom.no_clock_support) {
			pr_debug("crypto clock is not handled by HLOS\n");
			break;
		}
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("set bus scale: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_scale_bus_bandwidth(data, argp);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("load ext elf req: invalid client handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->type = QSEECOM_UNAVAILABLE_CLIENT_APP;
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_load_external_elf(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed load_external_elf request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ: {
		if (data->type != QSEECOM_UNAVAILABLE_CLIENT_APP) {
			pr_err("unload ext elf req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_unload_external_elf(data);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed unload_app request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_APP_LOADED_QUERY_REQ: {
		data->type = QSEECOM_CLIENT_APP;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		pr_debug("APP_LOAD_QUERY: qseecom_addr = 0x%pK\n", data);
		ret = qseecom_query_app_loaded(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("send cmd svc req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->type = QSEECOM_SECURE_SERVICE;
		if (qseecom.qsee_version < QSEE_VERSION_03) {
			pr_err("SEND_CMD_SERVICE_REQ: Invalid qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_send_service_cmd(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_CREATE_KEY_REQ: {
		if (!(qseecom.support_pfe || qseecom.support_fde))
			pr_err("Features requiring key init not supported\n");
		if (data->type != QSEECOM_GENERIC) {
			pr_err("create key req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_05) {
			pr_err("Create Key feature unsupported: qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_create_key(data, argp);
		if (ret)
			pr_err("failed to create encryption key: %d\n", ret);

		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_WIPE_KEY_REQ: {
		if (!(qseecom.support_pfe || qseecom.support_fde))
			pr_err("Features requiring key init not supported\n");
		if (data->type != QSEECOM_GENERIC) {
			pr_err("wipe key req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_05) {
			pr_err("Wipe Key feature unsupported in qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_wipe_key(data, argp);
		if (ret)
			pr_err("failed to wipe encryption key: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_UPDATE_KEY_USER_INFO_REQ: {
		if (!(qseecom.support_pfe || qseecom.support_fde))
			pr_err("Features requiring key init not supported\n");
		if (data->type != QSEECOM_GENERIC) {
			pr_err("update key req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_05) {
			pr_err("Update Key feature unsupported in qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_update_key_user_info(data, argp);
		if (ret)
			pr_err("failed to update key user info: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("save part hash req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_save_partition_hash(argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("ES activated req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_is_es_activated(argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_MDTP_CIPHER_DIP_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("MDTP cipher DIP req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_mdtp_cipher_dip(argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_SEND_MODFD_RESP:
	case QSEECOM_IOCTL_SEND_MODFD_RESP_64: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("receive req: invalid handle (%d), lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		if (cmd == QSEECOM_IOCTL_SEND_MODFD_RESP)
			ret = qseecom_send_modfd_resp(data, argp);
		else
			ret = qseecom_send_modfd_resp_64(data, argp);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret)
			pr_err("failed qseecom_send_mod_resp: %d\n", ret);
		__qseecom_clean_data_sglistinfo(data);
		break;
	}
	case QSEECOM_QTEEC_IOCTL_OPEN_SESSION_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("Open session: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_40) {
			pr_err("GP feature unsupported: qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		ret = __qseecom_bus_scaling_enable(data, &perf_enabled);
		if (ret) {
			mutex_unlock(&app_access_lock);
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_qteec_open_session(data, argp);
		__qseecom_bus_scaling_disable(data, perf_enabled);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed open_session_cmd: %d\n", ret);
		__qseecom_clean_data_sglistinfo(data);
		break;
	}
	case QSEECOM_QTEEC_IOCTL_CLOSE_SESSION_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("Close session: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_40) {
			pr_err("GP feature unsupported: qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_qteec_close_session(data, argp);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed close_session_cmd: %d\n", ret);
		break;
	}
	case QSEECOM_QTEEC_IOCTL_INVOKE_MODFD_CMD_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("Invoke cmd: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_40) {
			pr_err("GP feature unsupported: qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		ret = __qseecom_bus_scaling_enable(data, &perf_enabled);
		if (ret) {
			mutex_unlock(&app_access_lock);
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_qteec_invoke_modfd_cmd(data, argp);
		__qseecom_bus_scaling_disable(data, perf_enabled);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed Invoke cmd: %d\n", ret);
		__qseecom_clean_data_sglistinfo(data);
		break;
	}
	case QSEECOM_QTEEC_IOCTL_REQUEST_CANCELLATION_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("Cancel req: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_40) {
			pr_err("GP feature unsupported: qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_qteec_request_cancellation(data, argp);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed request_cancellation: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_GET_CE_PIPE_INFO: {
		atomic_inc(&data->ioctl_count);
		ret = qseecom_get_ce_info(data, argp);
		if (ret)
			pr_err("failed get fde ce pipe info: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_FREE_CE_PIPE_INFO: {
		atomic_inc(&data->ioctl_count);
		ret = qseecom_free_ce_info(data, argp);
		if (ret)
			pr_err("failed get fde ce pipe info: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_QUERY_CE_PIPE_INFO: {
		atomic_inc(&data->ioctl_count);
		ret = qseecom_query_ce_info(data, argp);
		if (ret)
			pr_err("failed get fde ce pipe info: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	default:
		pr_err("Invalid IOCTL: 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static int qseecom_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct qseecom_dev_handle *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("kmalloc failed\n");
		return -ENOMEM;
	}
	file->private_data = data;
	data->abort = 0;
	data->type = QSEECOM_GENERIC;
	data->released = false;
	memset((void *)data->client.app_name, 0, MAX_APP_NAME_SIZE);
	data->mode = INACTIVE;
	init_waitqueue_head(&data->abort_wq);
	atomic_set(&data->ioctl_count, 0);
	return ret;
}

static int qseecom_release(struct inode *inode, struct file *file)
{
	struct qseecom_dev_handle *data = file->private_data;
	int ret = 0;

	if (data->released == false) {
		pr_debug("data: released=false, type=%d, mode=%d, data=0x%pK\n",
			data->type, data->mode, data);
		switch (data->type) {
		case QSEECOM_LISTENER_SERVICE:
			mutex_lock(&app_access_lock);
			ret = qseecom_unregister_listener(data);
			mutex_unlock(&app_access_lock);
			break;
		case QSEECOM_CLIENT_APP:
			mutex_lock(&app_access_lock);
			ret = qseecom_unload_app(data, true);
			mutex_unlock(&app_access_lock);
			break;
		case QSEECOM_SECURE_SERVICE:
		case QSEECOM_GENERIC:
			ret = qseecom_unmap_ion_allocated_memory(data);
			if (ret)
				pr_err("Ion Unmap failed\n");
			break;
		case QSEECOM_UNAVAILABLE_CLIENT_APP:
			break;
		default:
			pr_err("Unsupported clnt_handle_type %d",
				data->type);
			break;
		}
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		if (data->mode != INACTIVE) {
			qseecom_unregister_bus_bandwidth_needs(data);
			if (qseecom.cumulative_mode == INACTIVE) {
				ret = __qseecom_set_msm_bus_request(INACTIVE);
				if (ret)
					pr_err("Fail to scale down bus\n");
			}
		}
		mutex_unlock(&qsee_bw_mutex);
	} else {
		if (data->fast_load_enabled == true)
			qsee_disable_clock_vote(data, CLK_SFPB);
		if (data->perf_enabled == true)
			qsee_disable_clock_vote(data, CLK_DFAB);
	}
	kfree(data);

	return ret;
}

static const struct file_operations qseecom_fops = {
		.owner = THIS_MODULE,
		.unlocked_ioctl = qseecom_ioctl,
#ifdef CONFIG_COMPAT
		.compat_ioctl = compat_qseecom_ioctl,
#endif
		.open = qseecom_open,
		.release = qseecom_release
};

static int __qseecom_init_clk(enum qseecom_ce_hw_instance ce)
{
	int rc = 0;
	struct device *pdev;
	struct qseecom_clk *qclk;
	char *core_clk_src = NULL;
	char *core_clk = NULL;
	char *iface_clk = NULL;
	char *bus_clk = NULL;

	switch (ce) {
	case CLK_QSEE: {
		core_clk_src = "core_clk_src";
		core_clk = "core_clk";
		iface_clk = "iface_clk";
		bus_clk = "bus_clk";
		qclk = &qseecom.qsee;
		qclk->instance = CLK_QSEE;
		break;
	};
	case CLK_CE_DRV: {
		core_clk_src = "ce_drv_core_clk_src";
		core_clk = "ce_drv_core_clk";
		iface_clk = "ce_drv_iface_clk";
		bus_clk = "ce_drv_bus_clk";
		qclk = &qseecom.ce_drv;
		qclk->instance = CLK_CE_DRV;
		break;
	};
	default:
		pr_err("Invalid ce hw instance: %d!\n", ce);
		return -EIO;
	}

	if (qseecom.no_clock_support) {
		qclk->ce_core_clk = NULL;
		qclk->ce_clk = NULL;
		qclk->ce_bus_clk = NULL;
		qclk->ce_core_src_clk = NULL;
		return 0;
	}

	pdev = qseecom.pdev;

	/* Get CE3 src core clk. */
	qclk->ce_core_src_clk = clk_get(pdev, core_clk_src);
	if (!IS_ERR(qclk->ce_core_src_clk)) {
			rc = clk_set_rate(qclk->ce_core_src_clk,
						qseecom.ce_opp_freq_hz);
			if (rc) {
				clk_put(qclk->ce_core_src_clk);
				qclk->ce_core_src_clk = NULL;
				pr_err("Unable to set the core src clk @%uMhz.\n",
					qseecom.ce_opp_freq_hz/CE_CLK_DIV);
				return -EIO;
		}
	} else {
		pr_warn("Unable to get CE core src clk, set to NULL\n");
		qclk->ce_core_src_clk = NULL;
	}

	/* Get CE core clk */
	qclk->ce_core_clk = clk_get(pdev, core_clk);
	if (IS_ERR(qclk->ce_core_clk)) {
		rc = PTR_ERR(qclk->ce_core_clk);
		pr_err("Unable to get CE core clk\n");
		if (qclk->ce_core_src_clk != NULL)
			clk_put(qclk->ce_core_src_clk);
		return -EIO;
	}

	/* Get CE Interface clk */
	qclk->ce_clk = clk_get(pdev, iface_clk);
	if (IS_ERR(qclk->ce_clk)) {
		rc = PTR_ERR(qclk->ce_clk);
		pr_err("Unable to get CE interface clk\n");
		if (qclk->ce_core_src_clk != NULL)
			clk_put(qclk->ce_core_src_clk);
		clk_put(qclk->ce_core_clk);
		return -EIO;
	}

	/* Get CE AXI clk */
	qclk->ce_bus_clk = clk_get(pdev, bus_clk);
	if (IS_ERR(qclk->ce_bus_clk)) {
		rc = PTR_ERR(qclk->ce_bus_clk);
		pr_err("Unable to get CE BUS interface clk\n");
		if (qclk->ce_core_src_clk != NULL)
			clk_put(qclk->ce_core_src_clk);
		clk_put(qclk->ce_core_clk);
		clk_put(qclk->ce_clk);
		return -EIO;
	}

	return rc;
}

static void __qseecom_deinit_clk(enum qseecom_ce_hw_instance ce)
{
	struct qseecom_clk *qclk;

	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	else
		qclk = &qseecom.ce_drv;

	if (qclk->ce_clk != NULL) {
		clk_put(qclk->ce_clk);
		qclk->ce_clk = NULL;
	}
	if (qclk->ce_core_clk != NULL) {
		clk_put(qclk->ce_core_clk);
		qclk->ce_core_clk = NULL;
	}
	if (qclk->ce_bus_clk != NULL) {
		clk_put(qclk->ce_bus_clk);
		qclk->ce_bus_clk = NULL;
	}
	if (qclk->ce_core_src_clk != NULL) {
		clk_put(qclk->ce_core_src_clk);
		qclk->ce_core_src_clk = NULL;
	}
	qclk->instance = CLK_INVALID;
}

static int qseecom_retrieve_ce_data(struct platform_device *pdev)
{
	int rc = 0;
	uint32_t hlos_num_ce_hw_instances;
	uint32_t disk_encrypt_pipe;
	uint32_t file_encrypt_pipe;
	uint32_t hlos_ce_hw_instance[MAX_CE_PIPE_PAIR_PER_UNIT];
	int i;
	const int *tbl;
	int size;
	int entry;
	struct qseecom_crypto_info *pfde_tbl = NULL;
	struct qseecom_crypto_info *p;
	int tbl_size;
	int j;
	bool old_db = true;
	struct qseecom_ce_info_use *pce_info_use;
	uint32_t *unit_tbl = NULL;
	int total_units = 0;
	struct qseecom_ce_pipe_entry *pce_entry;

	qseecom.ce_info.fde = qseecom.ce_info.pfe = NULL;
	qseecom.ce_info.num_fde = qseecom.ce_info.num_pfe = 0;

	if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,qsee-ce-hw-instance",
				&qseecom.ce_info.qsee_ce_hw_instance)) {
		pr_err("Fail to get qsee ce hw instance information.\n");
		rc = -EINVAL;
		goto out;
	} else {
		pr_debug("qsee-ce-hw-instance=0x%x\n",
			qseecom.ce_info.qsee_ce_hw_instance);
	}

	qseecom.support_fde = of_property_read_bool((&pdev->dev)->of_node,
						"qcom,support-fde");
	qseecom.support_pfe = of_property_read_bool((&pdev->dev)->of_node,
						"qcom,support-pfe");

	if (!qseecom.support_pfe && !qseecom.support_fde) {
		pr_warn("Device does not support PFE/FDE");
		goto out;
	}

	if (qseecom.support_fde)
		tbl = of_get_property((&pdev->dev)->of_node,
			"qcom,full-disk-encrypt-info", &size);
	else
		tbl = NULL;
	if (tbl) {
		old_db = false;
		if (size % sizeof(struct qseecom_crypto_info)) {
			pr_err("full-disk-encrypt-info tbl size(%d)\n",
				size);
			rc = -EINVAL;
			goto out;
		}
		tbl_size = size / sizeof
				(struct qseecom_crypto_info);

		pfde_tbl = kzalloc(size, GFP_KERNEL);
		unit_tbl = kcalloc(tbl_size, sizeof(int), GFP_KERNEL);
		total_units = 0;

		if (!pfde_tbl || !unit_tbl) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}
		if (of_property_read_u32_array((&pdev->dev)->of_node,
			"qcom,full-disk-encrypt-info",
			(u32 *)pfde_tbl, size/sizeof(u32))) {
			pr_err("failed to read full-disk-encrypt-info tbl\n");
			rc = -EINVAL;
			goto out;
		}

		for (i = 0, p = pfde_tbl;  i < tbl_size; i++, p++) {
			for (j = 0; j < total_units; j++) {
				if (p->unit_num == *(unit_tbl + j))
					break;
			}
			if (j == total_units) {
				*(unit_tbl + total_units) = p->unit_num;
				total_units++;
			}
		}

		qseecom.ce_info.num_fde = total_units;
		pce_info_use = qseecom.ce_info.fde = kcalloc(
			total_units, sizeof(struct qseecom_ce_info_use),
				GFP_KERNEL);
		if (!pce_info_use) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}

		for (j = 0; j < total_units; j++, pce_info_use++) {
			pce_info_use->unit_num = *(unit_tbl + j);
			pce_info_use->alloc = false;
			pce_info_use->type = CE_PIPE_PAIR_USE_TYPE_FDE;
			pce_info_use->num_ce_pipe_entries = 0;
			pce_info_use->ce_pipe_entry = NULL;
			for (i = 0, p = pfde_tbl;  i < tbl_size; i++, p++) {
				if (p->unit_num == pce_info_use->unit_num)
					pce_info_use->num_ce_pipe_entries++;
			}

			entry = pce_info_use->num_ce_pipe_entries;
			pce_entry = pce_info_use->ce_pipe_entry =
				kcalloc(entry,
					sizeof(struct qseecom_ce_pipe_entry),
					GFP_KERNEL);
			if (pce_entry == NULL) {
				pr_err("failed to alloc memory\n");
				rc = -ENOMEM;
				goto out;
			}

			for (i = 0, p = pfde_tbl; i < tbl_size; i++, p++) {
				if (p->unit_num == pce_info_use->unit_num) {
					pce_entry->ce_num = p->ce;
					pce_entry->ce_pipe_pair =
							p->pipe_pair;
					pce_entry->valid = true;
					pce_entry++;
				}
			}
		}
		kfree(unit_tbl);
		unit_tbl = NULL;
		kfree(pfde_tbl);
		pfde_tbl = NULL;
	}

	if (qseecom.support_pfe)
		tbl = of_get_property((&pdev->dev)->of_node,
			"qcom,per-file-encrypt-info", &size);
	else
		tbl = NULL;
	if (tbl) {
		old_db = false;
		if (size % sizeof(struct qseecom_crypto_info)) {
			pr_err("per-file-encrypt-info tbl size(%d)\n",
				size);
			rc = -EINVAL;
			goto out;
		}
		tbl_size = size / sizeof
				(struct qseecom_crypto_info);

		pfde_tbl = kzalloc(size, GFP_KERNEL);
		unit_tbl = kcalloc(tbl_size, sizeof(int), GFP_KERNEL);
		total_units = 0;
		if (!pfde_tbl || !unit_tbl) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}
		if (of_property_read_u32_array((&pdev->dev)->of_node,
			"qcom,per-file-encrypt-info",
			(u32 *)pfde_tbl, size/sizeof(u32))) {
			pr_err("failed to read per-file-encrypt-info tbl\n");
			rc = -EINVAL;
			goto out;
		}

		for (i = 0, p = pfde_tbl;  i < tbl_size; i++, p++) {
			for (j = 0; j < total_units; j++) {
				if (p->unit_num == *(unit_tbl + j))
					break;
			}
			if (j == total_units) {
				*(unit_tbl + total_units) = p->unit_num;
				total_units++;
			}
		}

		qseecom.ce_info.num_pfe = total_units;
		pce_info_use = qseecom.ce_info.pfe = kcalloc(
			total_units, sizeof(struct qseecom_ce_info_use),
				GFP_KERNEL);
		if (!pce_info_use) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}

		for (j = 0; j < total_units; j++, pce_info_use++) {
			pce_info_use->unit_num = *(unit_tbl + j);
			pce_info_use->alloc = false;
			pce_info_use->type = CE_PIPE_PAIR_USE_TYPE_PFE;
			pce_info_use->num_ce_pipe_entries = 0;
			pce_info_use->ce_pipe_entry = NULL;
			for (i = 0, p = pfde_tbl; i < tbl_size; i++, p++) {
				if (p->unit_num == pce_info_use->unit_num)
					pce_info_use->num_ce_pipe_entries++;
			}

			entry = pce_info_use->num_ce_pipe_entries;
			pce_entry = pce_info_use->ce_pipe_entry =
				kcalloc(entry,
					sizeof(struct qseecom_ce_pipe_entry),
					GFP_KERNEL);
			if (pce_entry == NULL) {
				pr_err("failed to alloc memory\n");
				rc = -ENOMEM;
				goto out;
			}

			for (i = 0, p = pfde_tbl; i < tbl_size; i++, p++) {
				if (p->unit_num == pce_info_use->unit_num) {
					pce_entry->ce_num = p->ce;
					pce_entry->ce_pipe_pair =
							p->pipe_pair;
					pce_entry->valid = true;
					pce_entry++;
				}
			}
		}
		kfree(unit_tbl);
		unit_tbl = NULL;
		kfree(pfde_tbl);
		pfde_tbl = NULL;
	}

	if (!old_db)
		goto out1;

	if (of_property_read_bool((&pdev->dev)->of_node,
			"qcom,support-multiple-ce-hw-instance")) {
		if (of_property_read_u32((&pdev->dev)->of_node,
			"qcom,hlos-num-ce-hw-instances",
				&hlos_num_ce_hw_instances)) {
			pr_err("Fail: get hlos number of ce hw instance\n");
			rc = -EINVAL;
			goto out;
		}
	} else {
		hlos_num_ce_hw_instances = 1;
	}

	if (hlos_num_ce_hw_instances > MAX_CE_PIPE_PAIR_PER_UNIT) {
		pr_err("Fail: hlos number of ce hw instance exceeds %d\n",
			MAX_CE_PIPE_PAIR_PER_UNIT);
		rc = -EINVAL;
		goto out;
	}

	if (of_property_read_u32_array((&pdev->dev)->of_node,
			"qcom,hlos-ce-hw-instance", hlos_ce_hw_instance,
			hlos_num_ce_hw_instances)) {
		pr_err("Fail: get hlos ce hw instance info\n");
		rc = -EINVAL;
		goto out;
	}

	if (qseecom.support_fde) {
		pce_info_use = qseecom.ce_info.fde =
			kzalloc(sizeof(struct qseecom_ce_info_use), GFP_KERNEL);
		if (!pce_info_use) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}
		/* by default for old db */
		qseecom.ce_info.num_fde = DEFAULT_NUM_CE_INFO_UNIT;
		pce_info_use->unit_num = DEFAULT_CE_INFO_UNIT;
		pce_info_use->alloc = false;
		pce_info_use->type = CE_PIPE_PAIR_USE_TYPE_FDE;
		pce_info_use->ce_pipe_entry = NULL;
		if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,disk-encrypt-pipe-pair",
				&disk_encrypt_pipe)) {
			pr_err("Fail to get FDE pipe information.\n");
			rc = -EINVAL;
				goto out;
		} else {
			pr_debug("disk-encrypt-pipe-pair=0x%x",
				disk_encrypt_pipe);
		}
		entry = pce_info_use->num_ce_pipe_entries =
				hlos_num_ce_hw_instances;
		pce_entry = pce_info_use->ce_pipe_entry =
			kcalloc(entry,
				sizeof(struct qseecom_ce_pipe_entry),
				GFP_KERNEL);
		if (pce_entry == NULL) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}
		for (i = 0; i < entry; i++) {
			pce_entry->ce_num = hlos_ce_hw_instance[i];
			pce_entry->ce_pipe_pair = disk_encrypt_pipe;
			pce_entry->valid = 1;
			pce_entry++;
		}
	} else {
		pr_warn("Device does not support FDE");
		disk_encrypt_pipe = 0xff;
	}
	if (qseecom.support_pfe) {
		pce_info_use = qseecom.ce_info.pfe =
			kzalloc(sizeof(struct qseecom_ce_info_use), GFP_KERNEL);
		if (!pce_info_use) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}
		/* by default for old db */
		qseecom.ce_info.num_pfe = DEFAULT_NUM_CE_INFO_UNIT;
		pce_info_use->unit_num = DEFAULT_CE_INFO_UNIT;
		pce_info_use->alloc = false;
		pce_info_use->type = CE_PIPE_PAIR_USE_TYPE_PFE;
		pce_info_use->ce_pipe_entry = NULL;

		if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,file-encrypt-pipe-pair",
				&file_encrypt_pipe)) {
			pr_err("Fail to get PFE pipe information.\n");
			rc = -EINVAL;
			goto out;
		} else {
			pr_debug("file-encrypt-pipe-pair=0x%x",
				file_encrypt_pipe);
		}
		entry = pce_info_use->num_ce_pipe_entries =
						hlos_num_ce_hw_instances;
		pce_entry = pce_info_use->ce_pipe_entry =
			kcalloc(entry,
				sizeof(struct qseecom_ce_pipe_entry),
				GFP_KERNEL);
		if (pce_entry == NULL) {
			pr_err("failed to alloc memory\n");
			rc = -ENOMEM;
			goto out;
		}
		for (i = 0; i < entry; i++) {
			pce_entry->ce_num = hlos_ce_hw_instance[i];
			pce_entry->ce_pipe_pair = file_encrypt_pipe;
			pce_entry->valid = 1;
			pce_entry++;
		}
	} else {
		pr_warn("Device does not support PFE");
		file_encrypt_pipe = 0xff;
	}

out1:
	qseecom.qsee.instance = qseecom.ce_info.qsee_ce_hw_instance;
	qseecom.ce_drv.instance = hlos_ce_hw_instance[0];
out:
	if (rc) {
		if (qseecom.ce_info.fde) {
			pce_info_use = qseecom.ce_info.fde;
			for (i = 0; i < qseecom.ce_info.num_fde; i++) {
				pce_entry = pce_info_use->ce_pipe_entry;
				kfree(pce_entry);
				pce_info_use++;
			}
		}
		kfree(qseecom.ce_info.fde);
		qseecom.ce_info.fde = NULL;
		if (qseecom.ce_info.pfe) {
			pce_info_use = qseecom.ce_info.pfe;
			for (i = 0; i < qseecom.ce_info.num_pfe; i++) {
				pce_entry = pce_info_use->ce_pipe_entry;
				kfree(pce_entry);
				pce_info_use++;
			}
		}
		kfree(qseecom.ce_info.pfe);
		qseecom.ce_info.pfe = NULL;
	}
	kfree(unit_tbl);
	kfree(pfde_tbl);
	return rc;
}

static int qseecom_get_ce_info(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct qseecom_ce_info_req req;
	struct qseecom_ce_info_req *pinfo = &req;
	int ret = 0;
	int i;
	unsigned int entries;
	struct qseecom_ce_info_use *pce_info_use, *p;
	int total = 0;
	bool found = false;
	struct qseecom_ce_pipe_entry *pce_entry;

	ret = copy_from_user(pinfo, argp,
				sizeof(struct qseecom_ce_info_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	switch (pinfo->usage) {
	case QSEOS_KM_USAGE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION:
		if (qseecom.support_fde) {
			p = qseecom.ce_info.fde;
			total = qseecom.ce_info.num_fde;
		} else {
			pr_err("system does not support fde\n");
			return -EINVAL;
		}
		break;
	case QSEOS_KM_USAGE_FILE_ENCRYPTION:
		if (qseecom.support_pfe) {
			p = qseecom.ce_info.pfe;
			total = qseecom.ce_info.num_pfe;
		} else {
			pr_err("system does not support pfe\n");
			return -EINVAL;
		}
		break;
	default:
		pr_err("unsupported usage %d\n", pinfo->usage);
		return -EINVAL;
	}

	pce_info_use = NULL;
	for (i = 0; i < total; i++) {
		if (!p->alloc)
			pce_info_use = p;
		else if (!memcmp(p->handle, pinfo->handle,
						MAX_CE_INFO_HANDLE_SIZE)) {
			pce_info_use = p;
			found = true;
			break;
		}
		p++;
	}

	if (pce_info_use == NULL)
		return -EBUSY;

	pinfo->unit_num = pce_info_use->unit_num;
	if (!pce_info_use->alloc) {
		pce_info_use->alloc = true;
		memcpy(pce_info_use->handle,
			pinfo->handle, MAX_CE_INFO_HANDLE_SIZE);
	}
	if (pce_info_use->num_ce_pipe_entries >
					MAX_CE_PIPE_PAIR_PER_UNIT)
		entries = MAX_CE_PIPE_PAIR_PER_UNIT;
	else
		entries = pce_info_use->num_ce_pipe_entries;
	pinfo->num_ce_pipe_entries = entries;
	pce_entry = pce_info_use->ce_pipe_entry;
	for (i = 0; i < entries; i++, pce_entry++)
		pinfo->ce_pipe_entry[i] = *pce_entry;
	for (; i < MAX_CE_PIPE_PAIR_PER_UNIT; i++)
		pinfo->ce_pipe_entry[i].valid = 0;

	if (copy_to_user(argp, pinfo, sizeof(struct qseecom_ce_info_req))) {
		pr_err("copy_to_user failed\n");
		ret = -EFAULT;
	}
	return ret;
}

static int qseecom_free_ce_info(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct qseecom_ce_info_req req;
	struct qseecom_ce_info_req *pinfo = &req;
	int ret = 0;
	struct qseecom_ce_info_use *p;
	int total = 0;
	int i;
	bool found = false;

	ret = copy_from_user(pinfo, argp,
				sizeof(struct qseecom_ce_info_req));
	if (ret)
		return ret;

	switch (pinfo->usage) {
	case QSEOS_KM_USAGE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION:
		if (qseecom.support_fde) {
			p = qseecom.ce_info.fde;
			total = qseecom.ce_info.num_fde;
		} else {
			pr_err("system does not support fde\n");
			return -EINVAL;
		}
		break;
	case QSEOS_KM_USAGE_FILE_ENCRYPTION:
		if (qseecom.support_pfe) {
			p = qseecom.ce_info.pfe;
			total = qseecom.ce_info.num_pfe;
		} else {
			pr_err("system does not support pfe\n");
			return -EINVAL;
		}
		break;
	default:
		pr_err("unsupported usage %d\n", pinfo->usage);
		return -EINVAL;
	}

	for (i = 0; i < total; i++) {
		if (p->alloc &&
			!memcmp(p->handle, pinfo->handle,
					MAX_CE_INFO_HANDLE_SIZE)) {
			memset(p->handle, 0, MAX_CE_INFO_HANDLE_SIZE);
			p->alloc = false;
			found = true;
			break;
		}
		p++;
	}
	return ret;
}

static int qseecom_query_ce_info(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct qseecom_ce_info_req req;
	struct qseecom_ce_info_req *pinfo = &req;
	int ret = 0;
	int i;
	unsigned int entries;
	struct qseecom_ce_info_use *pce_info_use, *p;
	int total = 0;
	bool found = false;
	struct qseecom_ce_pipe_entry *pce_entry;

	ret = copy_from_user(pinfo, argp,
				sizeof(struct qseecom_ce_info_req));
	if (ret)
		return ret;

	switch (pinfo->usage) {
	case QSEOS_KM_USAGE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_UFS_ICE_DISK_ENCRYPTION:
	case QSEOS_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION:
		if (qseecom.support_fde) {
			p = qseecom.ce_info.fde;
			total = qseecom.ce_info.num_fde;
		} else {
			pr_err("system does not support fde\n");
			return -EINVAL;
		}
		break;
	case QSEOS_KM_USAGE_FILE_ENCRYPTION:
		if (qseecom.support_pfe) {
			p = qseecom.ce_info.pfe;
			total = qseecom.ce_info.num_pfe;
		} else {
			pr_err("system does not support pfe\n");
			return -EINVAL;
		}
		break;
	default:
		pr_err("unsupported usage %d\n", pinfo->usage);
		return -EINVAL;
	}

	pce_info_use = NULL;
	pinfo->unit_num = INVALID_CE_INFO_UNIT_NUM;
	pinfo->num_ce_pipe_entries  = 0;
	for (i = 0; i < MAX_CE_PIPE_PAIR_PER_UNIT; i++)
		pinfo->ce_pipe_entry[i].valid = 0;

	for (i = 0; i < total; i++) {

		if (p->alloc && !memcmp(p->handle,
				pinfo->handle, MAX_CE_INFO_HANDLE_SIZE)) {
			pce_info_use = p;
			found = true;
			break;
		}
		p++;
	}
	if (!pce_info_use)
		goto out;
	pinfo->unit_num = pce_info_use->unit_num;
	if (pce_info_use->num_ce_pipe_entries >
					MAX_CE_PIPE_PAIR_PER_UNIT)
		entries = MAX_CE_PIPE_PAIR_PER_UNIT;
	else
		entries = pce_info_use->num_ce_pipe_entries;
	pinfo->num_ce_pipe_entries = entries;
	pce_entry = pce_info_use->ce_pipe_entry;
	for (i = 0; i < entries; i++, pce_entry++)
		pinfo->ce_pipe_entry[i] = *pce_entry;
	for (; i < MAX_CE_PIPE_PAIR_PER_UNIT; i++)
		pinfo->ce_pipe_entry[i].valid = 0;
out:
	if (copy_to_user(argp, pinfo, sizeof(struct qseecom_ce_info_req))) {
		pr_err("copy_to_user failed\n");
		ret = -EFAULT;
	}
	return ret;
}

/*
 * Check whitelist feature, and if TZ feature version is < 1.0.0,
 * then whitelist feature is not supported.
 */
static int qseecom_check_whitelist_feature(void)
{
	int version = scm_get_feat_version(FEATURE_ID_WHITELIST);

	return version >= MAKE_WHITELIST_VERSION(1, 0, 0);
}

static int qseecom_probe(struct platform_device *pdev)
{
	int rc;
	int i;
	uint32_t feature = 10;
	struct device *class_dev;
	struct msm_bus_scale_pdata *qseecom_platform_support = NULL;
	struct qseecom_command_scm_resp resp;
	struct qseecom_ce_info_use *pce_info_use = NULL;

	qseecom.qsee_bw_count = 0;
	qseecom.qsee_perf_client = 0;
	qseecom.qsee_sfpb_bw_count = 0;

	qseecom.qsee.ce_core_clk = NULL;
	qseecom.qsee.ce_clk = NULL;
	qseecom.qsee.ce_core_src_clk = NULL;
	qseecom.qsee.ce_bus_clk = NULL;

	qseecom.cumulative_mode = 0;
	qseecom.current_mode = INACTIVE;
	qseecom.support_bus_scaling = false;
	qseecom.support_fde = false;
	qseecom.support_pfe = false;

	qseecom.ce_drv.ce_core_clk = NULL;
	qseecom.ce_drv.ce_clk = NULL;
	qseecom.ce_drv.ce_core_src_clk = NULL;
	qseecom.ce_drv.ce_bus_clk = NULL;
	atomic_set(&qseecom.qseecom_state, QSEECOM_STATE_NOT_READY);

	qseecom.app_block_ref_cnt = 0;
	init_waitqueue_head(&qseecom.app_block_wq);
	qseecom.whitelist_support = true;

	rc = alloc_chrdev_region(&qseecom_device_no, 0, 1, QSEECOM_DEV);
	if (rc < 0) {
		pr_err("alloc_chrdev_region failed %d\n", rc);
		return rc;
	}

	driver_class = class_create(THIS_MODULE, QSEECOM_DEV);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		pr_err("class_create failed %d\n", rc);
		goto exit_unreg_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, qseecom_device_no, NULL,
			QSEECOM_DEV);
	if (IS_ERR(class_dev)) {
		pr_err("class_device_create failed %d\n", rc);
		rc = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&qseecom.cdev, &qseecom_fops);
	qseecom.cdev.owner = THIS_MODULE;

	rc = cdev_add(&qseecom.cdev, MKDEV(MAJOR(qseecom_device_no), 0), 1);
	if (rc < 0) {
		pr_err("cdev_add failed %d\n", rc);
		goto exit_destroy_device;
	}

	INIT_LIST_HEAD(&qseecom.registered_listener_list_head);
	spin_lock_init(&qseecom.registered_listener_list_lock);
	INIT_LIST_HEAD(&qseecom.registered_app_list_head);
	spin_lock_init(&qseecom.registered_app_list_lock);
	INIT_LIST_HEAD(&qseecom.registered_kclient_list_head);
	spin_lock_init(&qseecom.registered_kclient_list_lock);
	init_waitqueue_head(&qseecom.send_resp_wq);
	qseecom.send_resp_flag = 0;

	qseecom.qsee_version = QSEEE_VERSION_00;
	rc = qseecom_scm_call(6, 3, &feature, sizeof(feature),
		&resp, sizeof(resp));
	pr_info("qseecom.qsee_version = 0x%x\n", resp.result);
	if (rc) {
		pr_err("Failed to get QSEE version info %d\n", rc);
		goto exit_del_cdev;
	}
	qseecom.qsee_version = resp.result;
	qseecom.qseos_version = QSEOS_VERSION_14;
	qseecom.commonlib_loaded = false;
	qseecom.commonlib64_loaded = false;
	qseecom.pdev = class_dev;
	/* Create ION msm client */
	qseecom.ion_clnt = msm_ion_client_create("qseecom-kernel");
	if (IS_ERR_OR_NULL(qseecom.ion_clnt)) {
		pr_err("Ion client cannot be created\n");
		rc = -ENOMEM;
		goto exit_del_cdev;
	}

	/* register client for bus scaling */
	if (pdev->dev.of_node) {
		qseecom.pdev->of_node = pdev->dev.of_node;
		qseecom.support_bus_scaling =
				of_property_read_bool((&pdev->dev)->of_node,
						"qcom,support-bus-scaling");
		rc = qseecom_retrieve_ce_data(pdev);
		if (rc)
			goto exit_destroy_ion_client;
		qseecom.appsbl_qseecom_support =
				of_property_read_bool((&pdev->dev)->of_node,
						"qcom,appsbl-qseecom-support");
		pr_debug("qseecom.appsbl_qseecom_support = 0x%x",
				qseecom.appsbl_qseecom_support);

		qseecom.commonlib64_loaded =
				of_property_read_bool((&pdev->dev)->of_node,
						"qcom,commonlib64-loaded-by-uefi");
		pr_debug("qseecom.commonlib64-loaded-by-uefi = 0x%x",
				qseecom.commonlib64_loaded);
		qseecom.fde_key_size = of_property_read_bool(
					(&pdev->dev)->of_node,
					"qcom,fde-key-size");
		qseecom.no_clock_support =
				of_property_read_bool((&pdev->dev)->of_node,
						"qcom,no-clock-support");
		if (!qseecom.no_clock_support) {
			pr_info("qseecom clocks handled by other subsystem\n");
		} else {
			pr_info("no-clock-support=0x%x",
			qseecom.no_clock_support);
		}

		if (of_property_read_u32((&pdev->dev)->of_node,
					"qcom,qsee-reentrancy-support",
					&qseecom.qsee_reentrancy_support)) {
			pr_warn("qsee reentrancy support phase is not defined, setting to default 0\n");
			qseecom.qsee_reentrancy_support = 0;
		} else {
			pr_warn("qseecom.qsee_reentrancy_support = %d\n",
				qseecom.qsee_reentrancy_support);
		}

		/*
		 * The qseecom bus scaling flag can not be enabled when
		 * crypto clock is not handled by HLOS.
		 */
		if (qseecom.no_clock_support && qseecom.support_bus_scaling) {
			pr_err("support_bus_scaling flag can not be enabled.\n");
			rc = -EINVAL;
			goto exit_destroy_ion_client;
		}

		if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ce-opp-freq",
				&qseecom.ce_opp_freq_hz)) {
			pr_debug("CE operating frequency is not defined, setting to default 100MHZ\n");
			qseecom.ce_opp_freq_hz = QSEE_CE_CLK_100MHZ;
		}
		rc = __qseecom_init_clk(CLK_QSEE);
		if (rc)
			goto exit_destroy_ion_client;

		if ((qseecom.qsee.instance != qseecom.ce_drv.instance) &&
				(qseecom.support_pfe || qseecom.support_fde)) {
			rc = __qseecom_init_clk(CLK_CE_DRV);
			if (rc) {
				__qseecom_deinit_clk(CLK_QSEE);
				goto exit_destroy_ion_client;
			}
		} else {
			struct qseecom_clk *qclk;

			qclk = &qseecom.qsee;
			qseecom.ce_drv.ce_core_clk = qclk->ce_core_clk;
			qseecom.ce_drv.ce_clk = qclk->ce_clk;
			qseecom.ce_drv.ce_core_src_clk = qclk->ce_core_src_clk;
			qseecom.ce_drv.ce_bus_clk = qclk->ce_bus_clk;
		}

		qseecom_platform_support = (struct msm_bus_scale_pdata *)
						msm_bus_cl_get_pdata(pdev);
		if (qseecom.qsee_version >= (QSEE_VERSION_02) &&
			(!qseecom.is_apps_region_protected &&
			!qseecom.appsbl_qseecom_support)) {
			struct resource *resource = NULL;
			struct qsee_apps_region_info_ireq req;
			struct qsee_apps_region_info_64bit_ireq req_64bit;
			struct qseecom_command_scm_resp resp;
			void *cmd_buf = NULL;
			size_t cmd_len;

			resource = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "secapp-region");
			if (resource) {
				if (qseecom.qsee_version < QSEE_VERSION_40) {
					req.qsee_cmd_id =
						QSEOS_APP_REGION_NOTIFICATION;
					req.addr = (uint32_t)resource->start;
					req.size = resource_size(resource);
					cmd_buf = (void *)&req;
					cmd_len = sizeof(struct
						qsee_apps_region_info_ireq);
					pr_warn("secure app region addr=0x%x size=0x%x",
							req.addr, req.size);
				} else {
					req_64bit.qsee_cmd_id =
						QSEOS_APP_REGION_NOTIFICATION;
					req_64bit.addr = resource->start;
					req_64bit.size = resource_size(
							resource);
					cmd_buf = (void *)&req_64bit;
					cmd_len = sizeof(struct
					qsee_apps_region_info_64bit_ireq);
					pr_warn("secure app region addr=0x%llx size=0x%x",
						req_64bit.addr, req_64bit.size);
				}
			} else {
				pr_err("Fail to get secure app region info\n");
				rc = -EINVAL;
				goto exit_deinit_clock;
			}
			rc = __qseecom_enable_clk(CLK_QSEE);
			if (rc) {
				pr_err("CLK_QSEE enabling failed (%d)\n", rc);
				rc = -EIO;
				goto exit_deinit_clock;
			}
			rc = qseecom_scm_call(SCM_SVC_TZSCHEDULER, 1,
					cmd_buf, cmd_len,
					&resp, sizeof(resp));
			__qseecom_disable_clk(CLK_QSEE);
			if (rc || (resp.result != QSEOS_RESULT_SUCCESS)) {
				pr_err("send secapp reg fail %d resp.res %d\n",
							rc, resp.result);
				rc = -EINVAL;
				goto exit_deinit_clock;
			}
		}
	/*
	 * By default, appsbl only loads cmnlib. If OEM changes appsbl to
	 * load cmnlib64 too, while cmnlib64 img is not present in non_hlos.bin,
	 * Pls add "qseecom.commonlib64_loaded = true" here too.
	 */
		if (qseecom.is_apps_region_protected ||
					qseecom.appsbl_qseecom_support)
			qseecom.commonlib_loaded = true;
	} else {
		qseecom_platform_support = (struct msm_bus_scale_pdata *)
						pdev->dev.platform_data;
	}
	if (qseecom.support_bus_scaling) {
		init_timer(&(qseecom.bw_scale_down_timer));
		INIT_WORK(&qseecom.bw_inactive_req_ws,
					qseecom_bw_inactive_req_work);
		qseecom.bw_scale_down_timer.function =
				qseecom_scale_bus_bandwidth_timer_callback;
	}
	qseecom.timer_running = false;
	qseecom.qsee_perf_client = msm_bus_scale_register_client(
					qseecom_platform_support);

	qseecom.whitelist_support = qseecom_check_whitelist_feature();
	pr_warn("qseecom.whitelist_support = %d\n",
				qseecom.whitelist_support);

	if (!qseecom.qsee_perf_client)
		pr_err("Unable to register bus client\n");

	atomic_set(&qseecom.qseecom_state, QSEECOM_STATE_READY);
	return 0;

exit_deinit_clock:
	__qseecom_deinit_clk(CLK_QSEE);
	if ((qseecom.qsee.instance != qseecom.ce_drv.instance) &&
		(qseecom.support_pfe || qseecom.support_fde))
		__qseecom_deinit_clk(CLK_CE_DRV);
exit_destroy_ion_client:
	if (qseecom.ce_info.fde) {
		pce_info_use = qseecom.ce_info.fde;
		for (i = 0; i < qseecom.ce_info.num_fde; i++) {
			kzfree(pce_info_use->ce_pipe_entry);
			pce_info_use++;
		}
		kfree(qseecom.ce_info.fde);
	}
	if (qseecom.ce_info.pfe) {
		pce_info_use = qseecom.ce_info.pfe;
		for (i = 0; i < qseecom.ce_info.num_pfe; i++) {
			kzfree(pce_info_use->ce_pipe_entry);
			pce_info_use++;
		}
		kfree(qseecom.ce_info.pfe);
	}
	ion_client_destroy(qseecom.ion_clnt);
exit_del_cdev:
	cdev_del(&qseecom.cdev);
exit_destroy_device:
	device_destroy(driver_class, qseecom_device_no);
exit_destroy_class:
	class_destroy(driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(qseecom_device_no, 1);
	return rc;
}

static int qseecom_remove(struct platform_device *pdev)
{
	struct qseecom_registered_kclient_list *kclient = NULL;
	unsigned long flags = 0;
	int ret = 0;
	int i;
	struct qseecom_ce_pipe_entry *pce_entry;
	struct qseecom_ce_info_use *pce_info_use;

	atomic_set(&qseecom.qseecom_state, QSEECOM_STATE_NOT_READY);
	spin_lock_irqsave(&qseecom.registered_kclient_list_lock, flags);

	list_for_each_entry(kclient, &qseecom.registered_kclient_list_head,
								list) {
		if (!kclient)
			goto exit_irqrestore;

		/* Break the loop if client handle is NULL */
		if (!kclient->handle)
			goto exit_free_kclient;

		if (list_empty(&kclient->list))
			goto exit_free_kc_handle;

		list_del(&kclient->list);
		mutex_lock(&app_access_lock);
		ret = qseecom_unload_app(kclient->handle->dev, false);
		mutex_unlock(&app_access_lock);
		if (!ret) {
			kzfree(kclient->handle->dev);
			kzfree(kclient->handle);
			kzfree(kclient);
		}
	}

exit_free_kc_handle:
	kzfree(kclient->handle);
exit_free_kclient:
	kzfree(kclient);
exit_irqrestore:
	spin_unlock_irqrestore(&qseecom.registered_kclient_list_lock, flags);

	if (qseecom.qseos_version > QSEEE_VERSION_00)
		qseecom_unload_commonlib_image();

	if (qseecom.qsee_perf_client)
		msm_bus_scale_client_update_request(qseecom.qsee_perf_client,
									0);
	if (pdev->dev.platform_data != NULL)
		msm_bus_scale_unregister_client(qseecom.qsee_perf_client);

	if (qseecom.support_bus_scaling) {
		cancel_work_sync(&qseecom.bw_inactive_req_ws);
		del_timer_sync(&qseecom.bw_scale_down_timer);
	}

	if (qseecom.ce_info.fde) {
		pce_info_use = qseecom.ce_info.fde;
		for (i = 0; i < qseecom.ce_info.num_fde; i++) {
			pce_entry = pce_info_use->ce_pipe_entry;
			kfree(pce_entry);
			pce_info_use++;
		}
	}
	kfree(qseecom.ce_info.fde);
	if (qseecom.ce_info.pfe) {
		pce_info_use = qseecom.ce_info.pfe;
		for (i = 0; i < qseecom.ce_info.num_pfe; i++) {
			pce_entry = pce_info_use->ce_pipe_entry;
			kfree(pce_entry);
			pce_info_use++;
		}
	}
	kfree(qseecom.ce_info.pfe);

	/* register client for bus scaling */
	if (pdev->dev.of_node) {
		__qseecom_deinit_clk(CLK_QSEE);
		if ((qseecom.qsee.instance != qseecom.ce_drv.instance) &&
				(qseecom.support_pfe || qseecom.support_fde))
			__qseecom_deinit_clk(CLK_CE_DRV);
	}

	ion_client_destroy(qseecom.ion_clnt);

	cdev_del(&qseecom.cdev);

	device_destroy(driver_class, qseecom_device_no);

	class_destroy(driver_class);

	unregister_chrdev_region(qseecom_device_no, 1);

	return ret;
}

static int qseecom_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct qseecom_clk *qclk;
	qclk = &qseecom.qsee;

	atomic_set(&qseecom.qseecom_state, QSEECOM_STATE_SUSPEND);
	if (qseecom.no_clock_support)
		return 0;

	mutex_lock(&qsee_bw_mutex);
	mutex_lock(&clk_access_lock);

	if (qseecom.current_mode != INACTIVE) {
		ret = msm_bus_scale_client_update_request(
			qseecom.qsee_perf_client, INACTIVE);
		if (ret)
			pr_err("Fail to scale down bus\n");
		else
			qseecom.current_mode = INACTIVE;
	}

	if (qclk->clk_access_cnt) {
		if (qclk->ce_clk != NULL)
			clk_disable_unprepare(qclk->ce_clk);
		if (qclk->ce_core_clk != NULL)
			clk_disable_unprepare(qclk->ce_core_clk);
		if (qclk->ce_bus_clk != NULL)
			clk_disable_unprepare(qclk->ce_bus_clk);
	}

	del_timer_sync(&(qseecom.bw_scale_down_timer));
	qseecom.timer_running = false;

	mutex_unlock(&clk_access_lock);
	mutex_unlock(&qsee_bw_mutex);
	cancel_work_sync(&qseecom.bw_inactive_req_ws);

	return 0;
}

static int qseecom_resume(struct platform_device *pdev)
{
	int mode = 0;
	int ret = 0;
	struct qseecom_clk *qclk;
	qclk = &qseecom.qsee;

	if (qseecom.no_clock_support)
		goto exit;

	mutex_lock(&qsee_bw_mutex);
	mutex_lock(&clk_access_lock);
	if (qseecom.cumulative_mode >= HIGH)
		mode = HIGH;
	else
		mode = qseecom.cumulative_mode;

	if (qseecom.cumulative_mode != INACTIVE) {
		ret = msm_bus_scale_client_update_request(
			qseecom.qsee_perf_client, mode);
		if (ret)
			pr_err("Fail to scale up bus to %d\n", mode);
		else
			qseecom.current_mode = mode;
	}

	if (qclk->clk_access_cnt) {
		if (qclk->ce_core_clk != NULL) {
			ret = clk_prepare_enable(qclk->ce_core_clk);
			if (ret) {
				pr_err("Unable to enable/prep CE core clk\n");
				qclk->clk_access_cnt = 0;
				goto err;
			}
		}
		if (qclk->ce_clk != NULL) {
			ret = clk_prepare_enable(qclk->ce_clk);
			if (ret) {
				pr_err("Unable to enable/prep CE iface clk\n");
				qclk->clk_access_cnt = 0;
				goto ce_clk_err;
			}
		}
		if (qclk->ce_bus_clk != NULL) {
			ret = clk_prepare_enable(qclk->ce_bus_clk);
			if (ret) {
				pr_err("Unable to enable/prep CE bus clk\n");
				qclk->clk_access_cnt = 0;
				goto ce_bus_clk_err;
			}
		}
	}

	if (qclk->clk_access_cnt || qseecom.cumulative_mode) {
		qseecom.bw_scale_down_timer.expires = jiffies +
			msecs_to_jiffies(QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
		mod_timer(&(qseecom.bw_scale_down_timer),
				qseecom.bw_scale_down_timer.expires);
		qseecom.timer_running = true;
	}

	mutex_unlock(&clk_access_lock);
	mutex_unlock(&qsee_bw_mutex);
	goto exit;

ce_bus_clk_err:
	if (qclk->ce_clk)
		clk_disable_unprepare(qclk->ce_clk);
ce_clk_err:
	if (qclk->ce_core_clk)
		clk_disable_unprepare(qclk->ce_core_clk);
err:
	mutex_unlock(&clk_access_lock);
	mutex_unlock(&qsee_bw_mutex);
	ret = -EIO;
exit:
	atomic_set(&qseecom.qseecom_state, QSEECOM_STATE_READY);
	return ret;
}
static struct of_device_id qseecom_match[] = {
	{
		.compatible = "qcom,qseecom",
	},
	{}
};

static struct platform_driver qseecom_plat_driver = {
	.probe = qseecom_probe,
	.remove = qseecom_remove,
	.suspend = qseecom_suspend,
	.resume = qseecom_resume,
	.driver = {
		.name = "qseecom",
		.owner = THIS_MODULE,
		.of_match_table = qseecom_match,
	},
};

static int qseecom_init(void)
{
	return platform_driver_register(&qseecom_plat_driver);
}

static void qseecom_exit(void)
{
	platform_driver_unregister(&qseecom_plat_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Secure Execution Environment Communicator");

module_init(qseecom_init);
module_exit(qseecom_exit);

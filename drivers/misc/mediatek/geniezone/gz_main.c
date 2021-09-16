// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */


#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/pm_wakeup.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>
#include <gz-trusty/trusty_ipc.h>
#include <kree/mem.h>
#include <kree/system.h>
#include <kree/tz_mod.h>
#include <tz_cross/ta_system.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/trustzone.h>
#include <linux/vmalloc.h>

#include "gz_main.h"
#include "mtee_ut/gz_ut.h"
#include "unittest.h"

#define enable_code 0 /*replace #if 0*/

#if enable_code
/*devapc related function is not supported in Kernel-4.19*/
#if IS_ENABLED(CONFIG_MTK_DEVAPC) && !IS_ENABLED(CONFIG_DEVAPC_LEGACY)
#include <mt-plat/devapc_public.h>
#endif

#endif

/* FIXME: MTK_PPM_SUPPORT is disabled temporarily */
#ifdef MTK_PPM_SUPPORT
#if IS_ENABLED(CONFIG_MACH_MT6758)
#include "legacy_controller.h"
#else
#include "mtk_ppm_platform.h"
#endif
#endif

#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
#include <m4u.h>
#include <m4u_port.h>
#include <kree/sdsp_m4u_mva.h>
#include <gz-trusty/smcall.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>

struct sg_table sg_sdsp_elf;
struct m4u_client_t *m4u_gz_client;
#endif

#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
uint32_t sdsp_elf_size[2] = { 0, 0 };
uint64_t sdsp_elf_pa[2] = { 0, 0 };
#endif

#define KREE_DEBUG(fmt...) pr_debug("[KREE]" fmt)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)

static const struct file_operations fops = {.owner = THIS_MODULE,
	.open = gz_dev_open,
	.release = gz_dev_release,
	.unlocked_ioctl = gz_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = gz_compat_ioctl,
#endif
};

static struct miscdevice gz_device = {.minor = MISC_DYNAMIC_MINOR,
	.name = "gz_kree",
	.fops = &fops
};

static int get_gz_version(void *args);

static const char *cases =
	" 0: GZ Version\n 1: TIPC\n 2.General Function\n"
	" 3: Shared Memory\n 4: GZ abort 5: Chunk Memory\n"
	" C: Secure Storage\n";

/************* sysfs operations ****************/
static ssize_t gz_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", cases);
}

static ssize_t gz_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t n)
{
	char tmp[50];
	char c;
	struct task_struct *th;

	if (n > 50) {
		KREE_DEBUG("err: n > 50\n");
		return n;
	}

	strncpy(tmp, buf, 1);
	tmp[n - 1] = '\0';
	c = tmp[0];

	KREE_DEBUG("GZ KREE test: %c\n", c);
	switch (c) {
	case '0':
		KREE_DEBUG("get gz version\n");
		th = kthread_run(get_gz_version, NULL, "GZ version");
		break;
	case '1':
		KREE_DEBUG("test tipc\n");
		th = kthread_run(gz_tipc_test, NULL, "GZ tipc test");
		break;
	case '2':
		KREE_DEBUG("test general functions\n");
		th = kthread_run(gz_test, NULL, "GZ KREE test");
		break;
	case '4':
		KREE_DEBUG("test GenieZone abort\n");
		th = kthread_run(gz_abort_test, NULL, "GZ KREE test");
		break;
	default:
		KREE_DEBUG("err: unknown test case\n");

		break;
	}
	return n;
}


DEVICE_ATTR_RW(gz_test);


static int create_files(void)
{
	int res;

	res = misc_register(&gz_device);

	if (res != 0) {
		KREE_DEBUG("ERR: misc register failed.");
		return res;
	}
	res = device_create_file(gz_device.this_device, &dev_attr_gz_test);
	if (res != 0) {
		KREE_DEBUG("ERR: create sysfs do_info failed.");
		return res;
	}
	return 0;
}

/*********** test case implementations *************/
static const char echo_srv_name[] = "com.mediatek.geniezone.srv.echo";
#define APP_NAME2 "com.mediatek.gz.srv.sync-ut"

static int get_gz_version(void *args)
{
	int ret;
	int i;
	int version_str_len;
	char *version_str;
	struct device *trusty_dev;

	if (IS_ERR_OR_NULL(tz_system_dev)) {
		KREE_ERR("GZ kree is not initialized\n");
		return TZ_RESULT_ERROR_NO_DATA;
	}

	trusty_dev = tz_system_dev->dev.parent;

	ret = trusty_fast_call32(trusty_dev,
				MTEE_SMCNR(SMCF_FC_GET_VERSION_STR, trusty_dev),
				-1, 0, 0);
	if (ret <= 0) {
		KREE_ERR("failed to get version: %d\n", ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	version_str_len = ret;

	version_str = kmalloc(version_str_len + 1, GFP_KERNEL);
	if (!version_str)
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;

	for (i = 0; i < version_str_len; i++) {
		ret = trusty_fast_call32(trusty_dev,
				MTEE_SMCNR(SMCF_FC_GET_VERSION_STR, trusty_dev),
				i, 0, 0);
		if (ret < 0)
			goto err_get_char;
		version_str[i] = ret;
	}
	version_str[i] = '\0';

	dev_info(gz_device.this_device, "GZ version: %s\n", version_str);
	KREE_DEBUG("GZ version is : %s.....\n", version_str);

err_get_char:
	kfree(version_str);
	version_str = NULL;

	return 0;
}

/************* kernel module file ops (dummy) ****************/
struct UREE_SHAREDMEM_PARAM_US {
	uint64_t buffer;	/* FIXME: userspace void* is 32-bit */
	uint32_t size;
	uint32_t region_id;
};

struct user_shm_param {
	uint32_t session;
	uint32_t shm_handle;
	struct UREE_SHAREDMEM_PARAM_US param;
};

/************ to close session while driver is cancelled *************/
struct session_info {
	int handle_num;		/*num of session handles */
	KREE_SESSION_HANDLE *handles;	/*session handles */
	struct mutex mux;
};

#define queue_SessionNum 32
#define non_SessionID (0xffffffff)

static int _init_session_info(struct file *fp)
{
	struct session_info *info;
	int i;

	info = kmalloc(sizeof(struct session_info), GFP_KERNEL);
	if (!info)
		return TZ_RESULT_ERROR_GENERIC;

	info->handles = kmalloc_array(queue_SessionNum,
				      sizeof(KREE_SESSION_HANDLE), GFP_KERNEL);
	if (!info->handles) {
		kfree(info);
		KREE_ERR("info->handles malloc fail. stop!\n");
		return TZ_RESULT_ERROR_GENERIC;
	}
	info->handle_num = queue_SessionNum;
	for (i = 0; i < info->handle_num; i++)
		info->handles[i] = non_SessionID;

	mutex_init(&info->mux);
	fp->private_data = info;
	return TZ_RESULT_SUCCESS;
}

static int _free_session_info(struct file *fp)
{
	struct session_info *info;
	int i, num;

	KREE_DEBUG("====> [%d] [start] %s is running.\n", __LINE__, __func__);

	info = (struct session_info *)fp->private_data;

	/* lock */
	mutex_lock(&info->mux);

	num = info->handle_num;
	for (i = 0; i < num; i++) {

		if (info->handles[i] == non_SessionID)
			continue;
		KREE_CloseSession(info->handles[i]);
		KREE_DEBUG("=====> session handle[%d] =%d is closed.\n", i,
			   info->handles[i]);

		info->handles[i] = (KREE_SESSION_HANDLE) non_SessionID;
	}

	/* unlock */
	fp->private_data = 0;
	mutex_unlock(&info->mux);

	kfree(info->handles);
	kfree(info);

	KREE_DEBUG("====> [%d] end of %s().\n", __LINE__, __func__);

	return TZ_RESULT_SUCCESS;
}


static int _register_session_info(struct file *fp, KREE_SESSION_HANDLE handle)
{
	struct session_info *info;
	int i, num, nspace, ret = -1;
	void *ptr;

	KREE_DEBUG("[%s][%d] in_handle=0x%x\n", __func__, __LINE__, handle);
	if (handle < 0)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	info = (struct session_info *)fp->private_data;

	/* lock */
	mutex_lock(&info->mux);

	/* find empty space. */
	num = info->handle_num;
	for (i = 0; i < num; i++) {
		if (info->handles[i] == non_SessionID) {
			ret = i;
			break;
		}
	}

	/* Try grow the space */
	if (ret == -1) {

		nspace = num * 2;
		ptr = krealloc(info->handles,
			       nspace * sizeof(KREE_SESSION_HANDLE),
			       GFP_KERNEL);
		if (!ptr) {
			mutex_unlock(&info->mux);
			return TZ_RESULT_ERROR_GENERIC;
		}

		ret = num;
		info->handle_num = nspace;
		info->handles = (int *)ptr;
		memset(&info->handles[num], (KREE_SESSION_HANDLE) non_SessionID,
		       (nspace - num) * sizeof(KREE_SESSION_HANDLE));
	}

	if (ret >= 0)
		info->handles[ret] = handle;

	KREE_DEBUG("[%s][%d] reg. session handle[%d]=0x%x\n",
		   __func__, __LINE__, ret, handle);

	/* unlock */
	mutex_unlock(&info->mux);

	return TZ_RESULT_SUCCESS;
}


static int _unregister_session_info(struct file *fp,
	KREE_SESSION_HANDLE in_handleID)
{
	struct session_info *info;
	int ret = TZ_RESULT_ERROR_GENERIC;
	int i;

	KREE_DEBUG("[%s][%d] in_handleID=0x%x\n", __func__, __LINE__,
		   in_handleID);
	if (in_handleID < 0)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	info = (struct session_info *)fp->private_data;

	/* lock */
	mutex_lock(&info->mux);

	for (i = 0; i < info->handle_num; i++) {
		if (info->handles[i] == in_handleID) {
			info->handles[i] = (KREE_SESSION_HANDLE) non_SessionID;
			KREE_DEBUG
			    ("[%s][%d]session handle[%d]=0x%x is unreg.\n",
			     __func__, __LINE__, i, in_handleID);
			ret = TZ_RESULT_SUCCESS;
			break;
		}
	}
	/* unlock */
	mutex_unlock(&info->mux);
	KREE_DEBUG("[%d][%s] done. ret=%d\n", __LINE__, __func__, ret);
	return ret;
}

int mtee_sdsp_enable(u32 on)
{
	if (IS_ERR_OR_NULL(tz_system_dev)) {
		KREE_ERR("GZ kree is not initialized\n");
		return TZ_RESULT_ERROR_NO_DATA;
	}

	return trusty_std_call32(tz_system_dev->dev.parent,
			MTEE_SMCNR(MT_SMCF_SC_VPU, tz_system_dev->dev.parent),
			on, 0, 0);
}

int gz_get_cpuinfo_thread(void *data)
{
#ifdef MTK_PPM_SUPPORT
	struct cpufreq_policy curr_policy;
#endif
#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
	int ret;
	uint32_t sdsp_elf_buf_mva;
	uint32_t sdsp_elf_buf_size;
	struct sg_table *sg;
#endif

	if (platform_driver_register(&tz_system_driver))
		KREE_ERR("%s driver register fail\n", __func__);

	KREE_DEBUG("%s driver register done\n", __func__);

#if IS_ENABLED(CONFIG_MACH_MT6758)
	msleep(3000);
#else
	msleep(1000);
#endif

#ifdef MTK_PPM_SUPPORT
	cpufreq_get_policy(&curr_policy, 0);
	cpus_cluster_freq[0].max_freq = curr_policy.cpuinfo.max_freq;
	cpus_cluster_freq[0].min_freq = curr_policy.cpuinfo.min_freq;
	cpufreq_get_policy(&curr_policy, 4);
	cpus_cluster_freq[1].max_freq = curr_policy.cpuinfo.max_freq;
	cpus_cluster_freq[1].min_freq = curr_policy.cpuinfo.min_freq;
	KREE_INFO("%s, cluster [0]=%u-%u, [1]=%u-%u\n", __func__,
		  cpus_cluster_freq[0].max_freq, cpus_cluster_freq[0].min_freq,
		  cpus_cluster_freq[1].max_freq, cpus_cluster_freq[1].min_freq);
#endif

#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
	if (!m4u_gz_client)
		m4u_gz_client = m4u_create_client();
	KREE_DEBUG("m4u_gz_client(%p)\n", m4u_gz_client);

	sdsp_elf_buf_mva = SDSP_VPU0_ELF_MVA;
	if ((sdsp_elf_size[1]) &&
	    ((sdsp_elf_pa[0] + sdsp_elf_size[0]) == sdsp_elf_pa[1])) {
		sdsp_elf_buf_size = sdsp_elf_size[0] + sdsp_elf_size[1];
	} else {
		sdsp_elf_buf_size = sdsp_elf_size[0];
		KREE_ERR("vpu0,vpu1 pa/size(0x%llx/0x%x)(0x%llx/0x%x)\n",
			 sdsp_elf_pa[0], sdsp_elf_size[0],
			 sdsp_elf_pa[1], sdsp_elf_size[1]);
	}
	sg = &sg_sdsp_elf;
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	KREE_DEBUG("%s elf sg_alloc_table %s(%d)\n",
		   __func__, ret == 0 ? "done" : "fail", ret);
	if (!ret) {
		sg_dma_address(sg->sgl) = sdsp_elf_pa[0];
		sg_dma_len(sg->sgl) = sdsp_elf_buf_size;
		ret = m4u_alloc_mva(m4u_gz_client,
				    M4U_PORT_VPU,
				    0, sg, sdsp_elf_buf_size,
				    M4U_PROT_READ | M4U_PROT_WRITE,
				    M4U_FLAGS_START_FROM, &sdsp_elf_buf_mva);
		KREE_INFO("%s elf m4u_alloc_mva(0x%x) %s(%d)\n",
			  __func__, sdsp_elf_buf_mva,
			  ret == 0 ? "done" : "fail", ret);
	}
#endif

	perf_boost_cnt = 0;
	mutex_init(&perf_boost_lock);

#if IS_ENABLED(CONFIG_PM_SLEEP)
	/*kernel-4.14*/
	//wakeup_source_init(&TeeServiceCall_wake_lock, "KREE_TeeServiceCall");

	/*kernel-4.19*/
	if (IS_ERR_OR_NULL(tz_system_dev))
		return TZ_RESULT_ERROR_GENERIC;

	TeeServiceCall_wake_lock =
		wakeup_source_register(
		tz_system_dev->dev.parent, "KREE_TeeServiceCall");
	if (!TeeServiceCall_wake_lock) {
		KREE_ERR("TeeServiceCall_wake_lock null\n");
		return TZ_RESULT_ERROR_GENERIC;
	}

#endif

	return 0;
}

#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U) && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
static int __init store_sdsp_fw1_setup(struct reserved_mem *rmem)
{
	KREE_DEBUG("%s %s base(0x%llx) size(0x%llx)\n",
		   __func__, rmem->name, rmem->base, rmem->size);
	sdsp_elf_pa[0] = rmem->base;
	sdsp_elf_size[0] = rmem->size;
	return 0;
}

static int __init store_sdsp_fw2_setup(struct reserved_mem *rmem)
{
	KREE_DEBUG("%s %s base(%pa) size(%pa)\n",
		   __func__, rmem->name, &rmem->base, &rmem->size);
	sdsp_elf_pa[1] = rmem->base;
	sdsp_elf_size[1] = rmem->size;
	return 0;
}

RESERVEDMEM_OF_DECLARE(store_sdsp_fw1, "mediatek,gz-sdsp1-fw",
	store_sdsp_fw1_setup);
RESERVEDMEM_OF_DECLARE(store_sdsp_fw2, "mediatek,gz-sdsp2-fw",
	store_sdsp_fw2_setup);
#endif

#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
#include <mtee_regions.h>
struct gz_mva_map_table_t {
	const uint32_t region_id;
	const uint32_t mva;
	KREE_SHAREDMEM_HANDLE handle;
	uint32_t size;
	void *pa;
	struct sg_table sg;
};

#define MAX_GZ_MVA_MAP 1
struct gz_mva_map_table_t gz_mva_map_table[MAX_GZ_MVA_MAP] = {
	{
#if IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE)
	 .region_id = MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE,
#elif IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_MTEE_TEE)
	 .region_id = MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE,
#elif IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_VPU_MTEE_TEE)
	 .region_id = MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE,
#else
	 .region_id = 0xFFFFFFFF,
#endif
	 .mva = SDSP_VPU0_DTA_MVA,
	 .handle = 0,
	 .size = 0,
	 .pa = NULL}
};

int gz_do_m4u_map(KREE_SHAREDMEM_HANDLE handle, phys_addr_t pa, uint32_t size,
	uint32_t region_id)
{
	uint32_t i;
	uint32_t map_mva;
	int ret;
	struct sg_table *sg;

	if (!m4u_gz_client) {
		KREE_ERR("%s not create m4u_gz_client\n", __func__);
		return -1;
	}

	for (i = 0; i < MAX_GZ_MVA_MAP; i++) {
		if (gz_mva_map_table[i].region_id == region_id) {
			if (gz_mva_map_table[i].handle != 0) {
				KREE_ERR("%s region has been MAP\n", __func__);
				return -1;
			}
			map_mva = gz_mva_map_table[i].mva;
			sg = &(gz_mva_map_table[i].sg);
			ret = sg_alloc_table(sg, 1, GFP_KERNEL);
			if (ret) {
				KREE_ERR("%s region%u alloc sg fail\n",
				__func__, gz_mva_map_table[i].region_id);

				return ret;
			}
			sg_dma_address(sg->sgl) = (dma_addr_t) pa;
			sg_dma_len(sg->sgl) = size;
			ret = m4u_alloc_mva(m4u_gz_client, M4U_PORT_VPU,
					0, sg, size,
					M4U_PROT_READ | M4U_PROT_WRITE,
					M4U_FLAGS_START_FROM, &map_mva);
			if (ret || map_mva != gz_mva_map_table[i].mva) {
				KREE_ERR("%s mva alloc fail\n", __func__);
				return -1;
			}
			gz_mva_map_table[i].handle = handle;
			gz_mva_map_table[i].size = size;
			gz_mva_map_table[i].pa = pa;
			KREE_DEBUG("%s map pa(%p) size(%x) to mva(0x%x)\n",
				__func__, gz_mva_map_table[i].pa,
				gz_mva_map_table[i].size,
				gz_mva_map_table[i].mva);
			return 0;
		}
	}
	return 0;
}

int gz_do_m4u_umap(KREE_SHAREDMEM_HANDLE handle)
{
	uint32_t i;
	int ret;
	struct sg_table *sg;

	if (m4u_gz_client == NULL) {
		KREE_ERR("%s not create m4u_gz_client\n", __func__);
		return -1;
	}

	for (i = 0; i < MAX_GZ_MVA_MAP; i++) {
		if (gz_mva_map_table[i].handle == handle) {
			if (!gz_mva_map_table[i].handle) {
				KREE_ERR("%s region no any MAP\n", __func__);
				return -1;
			}
			ret = m4u_dealloc_mva(m4u_gz_client,
					M4U_PORT_VPU,
					gz_mva_map_table[i].mva);
			if (ret) {
				KREE_ERR("%s mva dealloc fail\n", __func__);
				return ret;
			}
			sg = &(gz_mva_map_table[i].sg);
			sg_free_table(sg);
			KREE_DEBUG("%s ummap mva(0x%x) for region(%u)\n",
				__func__, gz_mva_map_table[i].mva,
				gz_mva_map_table[i].region_id);
			gz_mva_map_table[i].handle = 0;
			gz_mva_map_table[i].size = 0;
			gz_mva_map_table[i].pa = 0;
			return 0;
		}
	}

	return 0;
}
#endif

static int gz_dev_open(struct inode *inode, struct file *filp)
{
	return _init_session_info(filp);
}

static int gz_dev_release(struct inode *inode, struct file *filp)
{
	return _free_session_info(filp);
}


/**************************************************************************
 *  DEV DRIVER IOCTL
 *  Ported from trustzone driver
 **************************************************************************/
static long tz_client_open_session(struct file *filep, void __user *arg)
{
	struct kree_session_cmd_param param;
	unsigned long cret;
	char uuid[40];
	long len;
	TZ_RESULT ret;
	KREE_SESSION_HANDLE handle = 0;

	cret = copy_from_user(&param, arg, sizeof(param));
	if (cret)
		return -EFAULT;

	/* Check if can we access UUID string. 10 for min uuid len. */
	if (!access_ok(VERIFY_READ, (void *)param.data, 10))
		return -EFAULT;

	KREE_DEBUG("%s: uuid addr = 0x%llx\n", __func__, param.data);
	len = strncpy_from_user(uuid, (void *)param.data,
		sizeof(uuid));
	if (len <= 0)
		return -EFAULT;

	uuid[sizeof(uuid) - 1] = 0;
	ret = KREE_CreateSession(uuid, &handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_DEBUG("%s: kree crt session fail(0x%x)\n", __func__, ret);
		goto tz_client_open_session_end;
	}

	KREE_DEBUG("===> %s: handle =%d\n", __func__, handle);

	_register_session_info(filep, handle);

tz_client_open_session_end:
	param.ret = ret;
	param.handle = handle;
	cret = copy_to_user(arg, &param, sizeof(param));
	if (cret)
		return cret;

	return 0;
}

static long tz_client_close_session(struct file *filep, void __user *arg)
{
	struct kree_session_cmd_param param;
	unsigned long cret;
	TZ_RESULT ret;

	cret = copy_from_user(&param, arg, sizeof(param));
	if (cret)
		return -EFAULT;

	if (param.handle < 0 || param.handle >= KREE_SESSION_HANDLE_MAX_SIZE)
		return TZ_RESULT_ERROR_INVALID_HANDLE;

	ret = KREE_CloseSession(param.handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_DEBUG("%s: fail(0x%x)\n", __func__, ret);
		goto _tz_client_close_session_end;
	}

	_unregister_session_info(filep, param.handle);

_tz_client_close_session_end:
	param.ret = ret;
	cret = copy_to_user(arg, &param, sizeof(param));
	if (cret)
		return -EFAULT;

	return 0;
}

static long tz_client_tee_service(struct file *file, void __user *arg,
	unsigned int compat)
{
	struct kree_tee_service_cmd_param cparam;
	unsigned long cret;
	uint32_t tmpTypes;
	union MTEEC_PARAM param[4], oparam[4];
	uint i;
	TZ_RESULT ret;
	KREE_SESSION_HANDLE handle;
	void __user *ubuf;
	uint32_t ubuf_sz;

	cret = copy_from_user(&cparam, arg, sizeof(cparam));
	if (cret) {
		KREE_ERR("%s: copy_from_user(msg) failed\n", __func__);
		return -EFAULT;
	}

	if (cparam.paramTypes != TZPT_NONE || cparam.param) {
		if (!access_ok(VERIFY_READ, (void *)cparam.param,
			sizeof(oparam)))
			return -EFAULT;

		cret = copy_from_user(oparam,
				(void *)(unsigned long)cparam.param,
				sizeof(oparam));
		if (cret) {
			KREE_ERR("%s: copy_from_user(param) failed\n",
				__func__);
			return -EFAULT;
		}
	}

	handle = (KREE_SESSION_HANDLE) cparam.handle;
	KREE_DEBUG("%s: session handle = %u\n", __func__, handle);

	/* Parameter processing. */
	memset(param, 0, sizeof(param));
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_INPUT:
		case TZPT_VALUE_INOUT:
			param[i] = oparam[i];
			break;

		case TZPT_VALUE_OUTPUT:
		case TZPT_NONE:
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
#if IS_ENABLED(CONFIG_COMPAT)
			if (compat) {
				ubuf = compat_ptr(oparam[i].mem32.buffer);
				ubuf_sz = oparam[i].mem32.size;
			} else
#endif
			{
				ubuf = oparam[i].mem.buffer;
				ubuf_sz = oparam[i].mem.size;
			}

			KREE_DEBUG("%s: ubuf = %p, ubuf_sz = %u\n", __func__,
				ubuf, ubuf_sz);

			if (type != TZPT_MEM_OUTPUT) {
				if (!access_ok(VERIFY_READ, ubuf, ubuf_sz)) {
					KREE_ERR("%s: cannnot read mem\n",
						__func__);
					cret = -EFAULT;
					goto error;
				}
			}
			if (type != TZPT_MEM_INPUT) {
				if (!access_ok(VERIFY_WRITE, ubuf, ubuf_sz)) {
					KREE_ERR("%s: cannnot write mem\n",
						__func__);
					cret = -EFAULT;
					goto error;
				}
			}

			if (ubuf_sz > GP_MEM_MAX_LEN) {
				KREE_ERR("%s: ubuf_sz larger than max(%d)\n",
					__func__, GP_MEM_MAX_LEN);
				cret = -ENOMEM;
				goto error;
			}

			param[i].mem.size = ubuf_sz;
			param[i].mem.buffer =
			    kmalloc(param[i].mem.size, GFP_KERNEL);
			if (!param[i].mem.buffer) {
				KREE_ERR("%s: kmalloc failed\n", __func__);
				cret = -ENOMEM;
				goto error;
			}

			if (type != TZPT_MEM_OUTPUT) {
				cret = copy_from_user(param[i].mem.buffer, ubuf,
						param[i].mem.size);
				if (cret) {
					KREE_ERR("%s: copy_from_user failed\n",
						__func__);
					cret = -EFAULT;
					goto error;
				}
			}
			break;

		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
			param[i] = oparam[i];
			break;

		default:
			ret = TZ_RESULT_ERROR_BAD_FORMAT;
			goto error;
		}
	}

	ret = KREE_TeeServiceCallPlus(handle, cparam.command, cparam.paramTypes,
				      param, cparam.cpumask);

	cparam.ret = ret;
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_OUTPUT:
		case TZPT_VALUE_INOUT:
			oparam[i] = param[i];
			break;

		default:
		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
		case TZPT_VALUE_INPUT:
		case TZPT_NONE:
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
#if IS_ENABLED(CONFIG_COMPAT)
			if (compat)
				ubuf = compat_ptr(oparam[i].mem32.buffer);
			else
#endif
				ubuf = oparam[i].mem.buffer;

			if (type != TZPT_MEM_INPUT) {
				cret = copy_to_user(ubuf, param[i].mem.buffer,
					param[i].mem.size);
				if (cret) {
					cret = -EFAULT;
					goto error;
				}
			}

			kfree(param[i].mem.buffer);
			break;
		}
	}

	/* Copy data back. */
	if (cparam.paramTypes != TZPT_NONE) {
		cret = copy_to_user((void *)(unsigned long)cparam.param, oparam,
				sizeof(oparam));
		if (cret) {
			KREE_ERR("%s: copy_to_user(param) failed\n", __func__);
			return -EFAULT;
		}
	}


	cret = copy_to_user(arg, &cparam, sizeof(cparam));
	if (cret) {
		KREE_ERR("%s: copy_to_user(msg) failed\n", __func__);
		return -EFAULT;
	}
	return 0;

error:
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
			kfree(param[i].mem.buffer);
			break;

		default:
			break;
		}
	}
	return cret;
}

#define SZ_32KB (32*1024)
static TZ_RESULT _reg_shmem_from_userspace(
	uint32_t session, uint32_t region_id,
	struct user_shm_param *shm_data,
	uint32_t *shm_handle)
{
	unsigned long cret;
	struct page **page;
	int i;
	uint64_t map_p_sz, pin_sz;
	unsigned long *pfns;
	struct page **delpages;

	struct MTIOMMU_PIN_RANGE_T *pin = NULL;
	uint64_t *map_p = NULL;
	int numOfPA = 0;
	KREE_SHAREDMEM_PARAM shm_param = {0};

	KREE_DEBUG("[%s][%d] runs.\n", __func__, __LINE__);
	if (((*shm_data).param.size <= 0) || (!(*shm_data).param.buffer)) {
		KREE_DEBUG("[%s] [fail] size <= 0 OR !buffer\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	KREE_DEBUG("session: %u, shm_handle: %u, size: %u, buffer: 0x%llx\n",
		(*shm_data).session, (*shm_data).shm_handle,
		(*shm_data).param.size, (*shm_data).param.buffer);

	/*init value */
	pin = NULL;
	map_p = NULL;

	shm_param.buffer = NULL;
	shm_param.size = 0;
	shm_param.mapAry = NULL;
	shm_param.region_id = region_id;
	*shm_handle = 0;

	cret = TZ_RESULT_SUCCESS;
	/*
	 * map pages
	 */
	/*
	 * 1. get user pages
	 * note: 'pin' resource need to keep for unregister.
	 * It will be freed after unregisted.
	 */
	/*check alloc size if <= 32KB*/
	pin_sz = sizeof(struct MTIOMMU_PIN_RANGE_T);
	if (pin_sz >= SZ_32KB) {
		KREE_INFO("[%s]alloc pin sz(0x%llx)>=32KB\n",
			__func__, pin_sz);
		//return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	/*pin = kzalloc(sizeof(struct MTIOMMU_PIN_RANGE_T), GFP_KERNEL);*/
	/*pin = vmalloc(pin_sz);*/
	pin = kvmalloc(pin_sz, GFP_KERNEL);
	if (!pin) {
		KREE_DEBUG("[%s]zalloc fail: pin is null.\n", __func__);
		cret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto us_map_fail;
	}

	pin->pageArray = NULL;
	cret = _map_user_pages(pin,
		untagged_addr((unsigned long)(*shm_data).param.buffer),
		(*shm_data).param.size, 0);
	if (cret) {
		pin->pageArray = NULL;
		KREE_DEBUG("[%s]_map_user_pages fail. map user pages = 0x%x\n",
			__func__, (uint32_t) cret);
		cret = TZ_RESULT_ERROR_INVALID_HANDLE;
		goto us_map_fail;
	}

	if (!pin->pageArray) {
		KREE_ERR("[%s]pin->pageArray is null. fail.\n", __func__);
		cret = TZ_RESULT_ERROR_GENERIC;
		goto us_map_fail;
	}

	/* 2. build PA table */
	/*check alloc size if <= 32KB*/
	map_p_sz = sizeof(uint64_t) * (pin->nrPages + 1);
	if (map_p_sz >= SZ_32KB) {
		KREE_INFO("[%s]alloc map_p sz(0x%llx)>=32KB\n",
			__func__, map_p_sz);
		//cret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		//goto us_map_fail;
	}

	/*map_p = kzalloc(map_p_sz, GFP_KERNEL);*/
	/*map_p = vmalloc(map_p_sz);*/
	map_p = kvmalloc(map_p_sz, GFP_KERNEL);
	if (!map_p) {
		KREE_DEBUG("[%s]alloc fail: map_p is null.\n", __func__);
		cret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto us_map_fail;
	}
	map_p[0] = pin->nrPages;
	if (pin->isPage) {
		page = (struct page **)pin->pageArray;
		for (i = 0; i < pin->nrPages; i++) /* PA */
			map_p[1 + i] =
			(uint64_t) PFN_PHYS(page_to_pfn(page[i]));
	} else {		/* pfn */
		pfns = (unsigned long *)pin->pageArray;
		for (i = 0; i < pin->nrPages; i++) /* get PA */
			map_p[1 + i] = (uint64_t) PFN_PHYS(pfns[i]);
	}

	/* init register shared mem params */
	shm_param.buffer = NULL;
	shm_param.size = 0;
	shm_param.mapAry = (void *)map_p;

	numOfPA = pin->nrPages;
	//KREE_DEBUG("[%s]numOfPA=0x%x\n", __func__, numOfPA);

	cret = KREE_RegisterSharedmem(session, shm_handle, &shm_param);
	KREE_DEBUG("[%s]reg shmem ret hd=0x%x\n", __func__, *shm_handle);
	if ((cret != TZ_RESULT_SUCCESS) || (*shm_handle == 0)) {
		KREE_ERR("[%s]RegisterSharedmem fail\n", __func__);
		KREE_ERR("ret=0x%lx, shm_hd=0x%x)\n", cret, *shm_handle);
	}

	/*after reg. shmem, free PA list array*/
	if (map_p != NULL)
		kvfree(map_p);
		//vfree(map_p);
		/*kfree(map_p);*/

us_map_fail:
	if (pin) {
		if (pin->pageArray) {
			delpages = (struct page **)pin->pageArray;
			if (pin->isPage) {
				for (i = 0; i < pin->nrPages; i++)
					put_page(delpages[i]);
			}
			kfree(pin->pageArray);
		}
		/*kfree(pin);*/
		//vfree(pin);
		kvfree(pin);
	}

	return cret;
}

int gz_adjust_task_attr(struct trusty_task_attr *manual_task_attr)
{
	if (IS_ERR_OR_NULL(tz_system_dev)) {
		KREE_ERR("GZ KREE is still not initialized!\n");
		return -EINVAL;
	}

	return trusty_adjust_task_attr(tz_system_dev->dev.parent, manual_task_attr);
}

TZ_RESULT gz_manual_adjust_trusty_wq_attr(void __user *user_req)
{
	int err;
	char str[32];
	struct trusty_task_attr manual_task_attr;

	err = copy_from_user(&str, user_req, sizeof(str));
	if (err) {
		KREE_ERR("[%s]copy_from_user fail(0x%x)\n", __func__,
			err);
		return err;
	}
	str[31] = '\0';
	KREE_DEBUG("%s cmd=%s\n", __func__, str);

	memset(&manual_task_attr, 0, sizeof(manual_task_attr));
	err = sscanf(str, "0x%x %d 0x%x %d",
			&manual_task_attr.mask[TRUSTY_TASK_KICK_ID],
			&manual_task_attr.pri[TRUSTY_TASK_KICK_ID],
			&manual_task_attr.mask[TRUSTY_TASK_CHK_ID],
			&manual_task_attr.pri[TRUSTY_TASK_CHK_ID]);
	if (err != 4)
		return -EINVAL;

	KREE_DEBUG("%s 0x%x %d 0x%x %d\n", __func__,
		manual_task_attr.mask[TRUSTY_TASK_KICK_ID],
		manual_task_attr.pri[TRUSTY_TASK_KICK_ID],
		manual_task_attr.mask[TRUSTY_TASK_CHK_ID],
		manual_task_attr.pri[TRUSTY_TASK_CHK_ID]);

	tipc_set_default_cpumask(manual_task_attr.mask[TRUSTY_TASK_KICK_ID]);

	return gz_adjust_task_attr(&manual_task_attr);
}

static long _gz_ioctl(struct file *filep, unsigned int cmd, void __user *arg,
	unsigned int compat)
{
	int err;
	TZ_RESULT ret = 0;
	struct user_shm_param shm_data;
	KREE_SHAREDMEM_HANDLE shm_handle = 0;

	if (_IOC_TYPE(cmd) != MTEE_IOC_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case MTEE_CMD_SHM_REG:
		KREE_DEBUG("[%s]cmd=MTEE_CMD_SHM_REG(0x%x)\n", __func__, cmd);
		/* copy param from user */
		err = copy_from_user(&shm_data, arg, sizeof(shm_data));
		if (err) {
			KREE_ERR("[%s]copy_from_user fail(0x%x)\n", __func__,
				err);
			return err;
		}

		if ((shm_data.param.size <= 0) || (!shm_data.param.buffer)) {
			KREE_DEBUG("[%s]bad param:size=%x or !param.buffer\n",
				__func__, shm_data.param.size);
			return TZ_RESULT_ERROR_BAD_PARAMETERS;
		}

		ret = _reg_shmem_from_userspace(shm_data.session,
		shm_data.param.region_id, &shm_data, &shm_handle);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s] _reg_userspace_shmem ret fail(%d)\n",
				__func__, ret);
		}

		shm_data.shm_handle = shm_handle;

		/* copy result back to user */
		shm_data.session = ret;
		err = copy_to_user(arg, &shm_data, sizeof(shm_data));
		if (err) {
			KREE_ERR("[%s]copy_to_user fail(0x%x)\n", __func__,
				err);
			return err;
		}
		break;

	case MTEE_CMD_SHM_UNREG:
		/* do nothing */
		break;

	case MTEE_CMD_OPEN_SESSION:
		KREE_DEBUG("[%s]cmd=MTEE_CMD_OPEN_SESSION(0x%x)\n", __func__,
			cmd);
		ret = tz_client_open_session(filep, arg);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]tz_client_open_session() fail\n",
				__func__);
			return ret;
		}
		break;

	case MTEE_CMD_CLOSE_SESSION:
		KREE_DEBUG("[%s]cmd=MTEE_CMD_CLOSE_SESSION(0x%x)\n", __func__,
			cmd);
		ret = tz_client_close_session(filep, arg);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]tz_client_close_session() fail\n",
				__func__);
			return ret;
		}
		break;

	case MTEE_CMD_TEE_SERVICE:
		KREE_DEBUG("[%s]cmd=MTEE_CMD_TEE_SERVICE(0x%x)\n", __func__,
			cmd);
		return tz_client_tee_service(filep, arg, compat);

#ifndef CONFIG_MTK_GZ_SUPPORT_SDSP
	case MTEE_CMD_FOD_TEE_SHM_ON:
		KREE_DEBUG("====> MTEE_CMD_FOD_TEE_SHM_ON ====\n");
		ret = mtee_sdsp_enable(1);
		break;

	case MTEE_CMD_FOD_TEE_SHM_OFF:
		KREE_DEBUG("====> MTEE_CMD_FOD_TEE_SHM_OFF ====\n");
		ret = mtee_sdsp_enable(0);
		break;
#endif

	case MTEE_CMD_ADJUST_WQ_ATTR:
		ret = gz_manual_adjust_trusty_wq_attr(arg);
		break;

	default:
		KREE_ERR("[%s] undefined ioctl cmd 0x%x\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long gz_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	long ret;
	void __user *user_req = (void __user *)arg;

	ret = _gz_ioctl(filep, cmd, user_req, 0);
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long gz_compat_ioctl(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	long ret;
	void __user *user_req = (void __user *)compat_ptr(arg);

	ret = _gz_ioctl(filep, cmd, user_req, 1);
	return ret;
}
#endif

#if enable_code

#if IS_ENABLED(CONFIG_MTK_DEVAPC) && !IS_ENABLED(CONFIG_DEVAPC_LEGACY)
static void gz_devapc_vio_dump(void)
{
	pr_debug("%s:%d GZ devapc is triggered!\n", __func__, __LINE__);

	if (IS_ERR_OR_NULL(tz_system_dev)) {
		KREE_ERR("GZ KREE is still not initialized!\n");
		return;
	}

	trusty_fast_call32(tz_system_dev->dev.parent,
		MTEE_SMCNR(MT_SMCF_FC_DEVAPC_VIO, tz_system_dev->dev.parent),
		0, 0, 0);
}

static struct devapc_vio_callbacks gz_devapc_vio_handle = {
	.id = INFRA_SUBSYS_GZ,
	.debug_dump = gz_devapc_vio_dump,
};
#endif

#endif
uint64_t va_gz_test_store;

/************ kernel module init entry ***************/
static int __init gz_init(void)
{
	int res;

	tz_system_dev = NULL;

	va_gz_test_store = (uint64_t) &gz_test_store;
	//KREE_DEBUG("[gz_main.c]====> gz_test_store VA=0x%llx\n", va_gz_test_store);

	res = create_files();
	if (res) {
		KREE_DEBUG("create sysfs failed: %d\n", res);
	} else {
		struct task_struct *gz_get_cpuinfo_task;

		gz_get_cpuinfo_task =
		    kthread_create(gz_get_cpuinfo_thread, NULL,
				"gz_get_cpuinfo_task");
		if (IS_ERR(gz_get_cpuinfo_task)) {
			KREE_ERR("Unable to start kernel thread %s\n",
				__func__);
			res = PTR_ERR(gz_get_cpuinfo_task);
		} else
			wake_up_process(gz_get_cpuinfo_task);
	}

#if enable_code

#if IS_ENABLED(CONFIG_MTK_DEVAPC) && !IS_ENABLED(CONFIG_DEVAPC_LEGACY)
	register_devapc_vio_callback(&gz_devapc_vio_handle);
#endif

#endif

	return res;
}

/************ kernel module exit entry ***************/
static void __exit gz_exit(void)
{
	KREE_DEBUG("gz driver exit\n");
}


module_init(gz_init);
module_exit(gz_exit);

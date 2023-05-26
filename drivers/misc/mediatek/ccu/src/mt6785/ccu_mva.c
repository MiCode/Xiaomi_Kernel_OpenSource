/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "ccu_cmn.h"
#include "ccu_mva.h"
#include "ccu_platform_def.h"

#define ION_LOG_SIZE	(10*1024*1024)	// 10M

static struct ion_client *_ccu_ion_client;
static struct m4u_client_t *m4u_client;

struct CcuMemHandle ccu_buffer_handle[2];

static int _ccu_ion_get_mva(struct ion_client *client,
	struct ion_handle *handle,
	unsigned int *mva, bool cached);
static struct ion_handle *_ccu_ion_alloc(struct ion_client *client,
	unsigned int heap_id_mask, size_t align, unsigned int size,
	bool cached, bool ion_log);
static unsigned long get_ns_systemtime(void);

int ccu_ion_init(void)
{
	MBOOL need_init = MFALSE;

	ccu_lock_ion_client_mutex();
	if (!_ccu_ion_client && g_ion_device) {
		LOG_INF_MUST("CCU ION_client need init\n");
		need_init = MTRUE;
	}

	if (need_init == MTRUE) {
		_ccu_ion_client = ion_client_create(g_ion_device, "ccu");
		LOG_INF_MUST(
			"CCU ION_client create success: 0x%p\n",
			_ccu_ion_client);
	}

	m4u_client = m4u_create_client();
	if (IS_ERR_OR_NULL(m4u_client))
		LOG_ERR("create client fail!\n");

	ccu_unlock_ion_client_mutex();
	return 0;
}

int ccu_ion_uninit(void)
{
	MBOOL need_uninit = MFALSE;

	ccu_lock_ion_client_mutex();
	if (_ccu_ion_client && g_ion_device) {
		LOG_INF_MUST("CCU ION_client need uninit\n");
		need_uninit = MTRUE;
	}

	if (need_uninit == MTRUE) {
		ion_client_destroy(_ccu_ion_client);
		LOG_INF_MUST("CCU ION_client destroy done.\n");
		_ccu_ion_client = NULL;
	}

	if (m4u_client)
		m4u_destroy_client(m4u_client);

	ccu_unlock_ion_client_mutex();
	return 0;
}

int ccu_deallocate_mva(uint32_t mva)
{
	int ret = 0;

	LOG_DBG("X-:%s\n", __func__);
	if (m4u_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client is null!\n",
			__func__);
		return -EINVAL;
	}

	if (mva != 0) {
		ret = m4u_dealloc_mva(m4u_client, CCUG_OF_M4U_PORT, mva);
		if (ret)
			LOG_ERR("dealloc mva fail");
	}

	return ret;
}

int ccu_allocate_mva(uint32_t *mva, void *va, int buffer_size)
{
	int ret = 0;
	struct sg_table *sg_table;
	unsigned int flag;

	ret = ccu_config_m4u_port();
	if (ret) {
		LOG_ERR("fail to config m4u port!\n");
		return ret;
	}

	/* alloc mva */
	flag = M4U_FLAGS_START_FROM;
	ret = m4u_alloc_mva(m4u_client, CCUG_OF_M4U_PORT, (unsigned long)va,
		sg_table, buffer_size,
		M4U_PROT_READ | M4U_PROT_WRITE, flag, mva);
	if (ret)
		LOG_ERR("alloc mva fail");

	return ret;
}

int ccu_config_m4u_port(void)
{
	int ret = 0;

#if defined(CONFIG_MTK_M4U)
	struct M4U_PORT_STRUCT port;

	port.ePortID = CCUG_OF_M4U_PORT;
	port.Virtuality = 1;
	port.Security = 0;
	port.domain = 3;
	port.Distance = 1;
	port.Direction = 0;

	ret = m4u_config_port(&port);
#endif
	return ret;
}

int ccu_allocate_mem(struct CcuMemHandle *memHandle, int size, bool cached)
{
	int ret = 0;

	LOG_DBG_MUST("_ccuAllocMem+\n");
	LOG_DBG_MUST("size(%d) cached(%d) memHandle->ionHandleKd(%d)\n",
		size, cached, memHandle->ionHandleKd);

	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client is null!\n", __func__);
		return -EINVAL;
	}

	if (ccu_buffer_handle[cached].ionHandleKd != NULL) {
		LOG_ERR("idx %d handle %p is not empty\n", cached,
		ccu_buffer_handle[cached].ionHandleKd);
		return -EINVAL;
	}

	//allocate ion buffer handle
	memHandle->ionHandleKd = _ccu_ion_alloc(_ccu_ion_client,
		ION_HEAP_MULTIMEDIA_MASK,
		0, (size_t)size, (cached)?3:0, memHandle->meminfo.ion_log);

	if (!memHandle->ionHandleKd) {
		LOG_ERR("fail to get ion buffer handle (size=0x%lx)\n", size);
		return -1;
	}

	LOG_DBG_MUST("memHandle->ionHandleKd(%p)\n", memHandle->ionHandleKd);

	// get buffer virtual address
	memHandle->meminfo.size = size;
	memHandle->meminfo.cached = cached;
	memHandle->meminfo.va = (char *)ion_map_kernel(_ccu_ion_client,
		memHandle->ionHandleKd);
	if (memHandle->meminfo.va == NULL) {
		LOG_ERR("fail to get buffer kernel virtual address");
		return -EINVAL;
	}
	LOG_DBG_MUST("memHandle->va(0x%lx)\n", memHandle->meminfo.va);

	ret = _ccu_ion_get_mva(_ccu_ion_client, memHandle->ionHandleKd,
		&memHandle->meminfo.mva, cached);
	if (ret) {
		LOG_ERR("ccu ion_get_mva failed\n");
		return -1;
	}
	LOG_DBG_MUST("memHandle->mva(0x%lx)\n", memHandle->meminfo.mva);

	LOG_DBG_MUST("_ccuAllocMem-\n");

	ccu_buffer_handle[memHandle->meminfo.cached] = *memHandle;
	return (memHandle->ionHandleKd != NULL) ? 0 : -1;

}

int ccu_deallocate_mem(struct CcuMemHandle *memHandle)
{
	uint32_t idx = (memHandle->meminfo.cached != 0) ? 1 : 0;

	LOG_DBG_MUST("free idx(%d) mva(0x%x) fd(0x%x) cached(0x%x)\n", idx,
		ccu_buffer_handle[idx].meminfo.mva,
		ccu_buffer_handle[idx].meminfo.shareFd,
		memHandle->meminfo.cached);

	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client is null!\n", __func__);
		return -EINVAL;
	}
	if (ccu_buffer_handle[idx].ionHandleKd == 0) {
		LOG_ERR("idx %d handle %d is empty\n", idx,
			ccu_buffer_handle[idx].ionHandleKd);
		return -EINVAL;
	}

	ion_unmap_kernel(_ccu_ion_client,
		ccu_buffer_handle[idx].ionHandleKd);
	ion_free(_ccu_ion_client,
		ccu_buffer_handle[idx].ionHandleKd);
	if ((memHandle->meminfo.ion_log) && (memHandle->meminfo.size > ION_LOG_SIZE))  //10M
		LOG_INF_MUST("ion free size = %d, caller = CCU\n", memHandle->meminfo.size);

	memset(&(ccu_buffer_handle[idx]), 0,
		sizeof(struct CcuMemHandle));

	return 0;
}

#define ION_FLAG_FREE_WITHOUT_DEFER (4)

static struct ion_handle *_ccu_ion_alloc(struct ion_client *client,
	unsigned int heap_id_mask, size_t align, unsigned int size, bool cached, bool ion_log)
{
	unsigned long ts_start, ts_end;
	struct ion_handle *disp_handle = NULL;

	if (ion_log)
		ts_start = get_ns_systemtime();
	disp_handle = ion_alloc(client, size, align,
		heap_id_mask, ((cached)?3:0) | ION_FLAG_FREE_WITHOUT_DEFER);
	if (IS_ERR(disp_handle)) {
		LOG_ERR("disp_ion_alloc 1error %p\n", disp_handle);
		return NULL;
	}

	if ((ion_log) && (size > ION_LOG_SIZE)) { //10M
		ts_end = get_ns_systemtime();
		LOG_INF_MUST("ion alloc size = %d, caller = CCU, costTime = %lu ns\n",
			size, (unsigned long)(ts_end-ts_start));
	}

	LOG_DBG("disp_ion_alloc 1 %p\n", disp_handle);

	return disp_handle;

}

static int _ccu_ion_get_mva(struct ion_client *client,
	struct ion_handle *handle,
		unsigned int *mva, bool cached)
{
	int ret = 0;
	struct ion_mm_data mm_data;
	struct ion_sys_data sys_data;
	int err;
	size_t count = 0;
	char const *ccu_bufferName = "CCU_BUFFER";

	ret = ccu_config_m4u_port();
	if (ret) {
		LOG_ERR("fail to config m4u port!\n");
		return ret;
	}
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER_EXT;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.security    = 0;
	mm_data.config_buffer_param.coherent    = 1;
	mm_data.config_buffer_param.module_id = M4U_PORT_CCU0;
	if (cached == false) {
		mm_data.config_buffer_param.reserve_iova_start =
		CCU_DDR_BUF_MVA_LOWER_BOUND;
		mm_data.config_buffer_param.reserve_iova_end =
		CCU_DDR_BUF_MVA_UPPER_BOUND;
	} else if (cached == true) {
		mm_data.config_buffer_param.reserve_iova_start =
		CCU_CTRL_BUFS_LOWER_BOUND;
		mm_data.config_buffer_param.reserve_iova_end =
		CCU_CTRL_BUFS_UPPER_BOUND;
	}

	if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data) < 0) {
		LOG_ERR("ION_MM_CONFIG_BUFFER_EXT: config buffer failed.%p -%p\n",
			client, handle);

		ion_free(client, handle);
		return -1;
	}
	LOG_DBG_MUST("ION_MM_CONFIG_BUFFER_EXT done\n");

	sys_data.sys_cmd = ION_SYS_GET_PHYS;
	sys_data.get_phys_param.kernel_handle = handle;
	sys_data.get_phys_param.phy_addr = (M4U_PORT_CCU0<<24) |
		ION_FLAG_GET_FIXED_PHYS;
	sys_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
	if (ion_kernel_ioctl(client, ION_CMD_SYSTEM,
		(unsigned long)&sys_data) < 0) {
		LOG_ERR("ION_SYS_GET_PHYS: get iova failed.%p -%p\n",
			client, handle);

		ion_free(client, handle);
		return -1;
	}
	*mva = sys_data.get_phys_param.phy_addr;
	LOG_DBG_MUST("alloc mmu addr hnd=0x%p,mva=0x%lx\n",
		handle, sys_data.get_phys_param.phy_addr);

	LOG_DBG_MUST("alloc mmu addr hnd=0x%p,mva=0x%08x\n",
		handle, (unsigned int)*mva);

	mm_data.mm_cmd = ION_MM_SET_DEBUG_INFO;
	mm_data.buf_debug_info_param.kernel_handle = handle;
	// Check Length of "ccu_bufferName"
	if (strlen(ccu_bufferName) < ION_MM_DBG_NAME_LEN)
		count = strlen(ccu_bufferName);
	else
		count = ION_MM_DBG_NAME_LEN - 1;
	strncpy(mm_data.buf_debug_info_param.dbg_name, ccu_bufferName, count);
	mm_data.buf_debug_info_param.dbg_name[count] = '\0';
	mm_data.buf_debug_info_param.value1 = 67;
	mm_data.buf_debug_info_param.value2 = 97;
	mm_data.buf_debug_info_param.value3 = 109;
	mm_data.buf_debug_info_param.value4 = 0;
	err = ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data);
	if (err)
		LOG_ERR("ion_kernel_ioctl(ION_MM_SET_DEBUG_INFO) returns %d, client %p",
			err, client);
	return 0;
}

static unsigned long get_ns_systemtime(void)
{
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	getnstimeofday(&ts);
	return ((unsigned long)(ts.tv_sec)) * 1000000000 + (ts.tv_nsec);
}

void ccu_ion_free_import_handle(struct ion_handle *handle)
{
	if (!_ccu_ion_client) {
		LOG_ERR("invalid ion client!\n");
		return;
	}
	if (handle == NULL) {
		LOG_ERR("invalid ion handle!\n");
		return;
	}

	ion_free(_ccu_ion_client, handle);
}

struct ion_handle *ccu_ion_import_handle(int fd)
{
	struct ion_handle *handle = NULL;

	if (!_ccu_ion_client) {
		LOG_ERR("ccu invalid ion client!\n");
		return handle;
	}
	if (fd == -1) {
		LOG_ERR("ccu invalid ion fd!\n");
		return handle;
	}

	handle = ion_import_dma_buf_fd(_ccu_ion_client, fd);
	LOG_INF_MUST(
	"ccu_ion_import_fd : %d, %s : 0x%p\n",
	fd, __func__, handle);
	if (!(handle)) {
		LOG_ERR("ccu mport ion handle failed!\n");
		return NULL;
	}

	return handle;
}

struct CcuMemInfo *ccu_get_binary_memory(void)
{
	if (ccu_buffer_handle[0].meminfo.va != NULL)
		return &ccu_buffer_handle[0].meminfo;

	LOG_ERR("ccu ddr va not found!\n");
	return NULL;
}

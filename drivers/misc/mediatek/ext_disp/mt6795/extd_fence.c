#include <linux/xlog.h>
#include "extd_drv_log.h"
#include <linux/ion_drv.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <mach/m4u.h>
#include "mtk_sync.h"

#include "ddp_ovl.h"
#include "extd_fence.h"
#include "ddp_path.h"
#include "extd_platform.h"
/************************* log*********************/

static bool extd_fence_on = true;
#define EXTD_FENCE_LOG(fmt, arg...) \
    do { \
        if (extd_fence_on) DISP_LOG_PRINT(ANDROID_LOG_WARN, "fence", fmt, ##arg); \
    }while (0)
#define EXTD_FENCE_ERR(fmt, arg...) \
    do { \
        DISP_LOG_PRINT(ANDROID_LOG_ERROR, "fence", "error(%d):"fmt, __LINE__, ##arg); \
    }while (0)
#define EXTD_FENCE_LOG_D(fmt, arg...) \
	do { \
		DISP_LOG_PRINT(ANDROID_LOG_DEBUG, "fence", fmt, ##arg); \
	}while (0)
#define EXTD_FENCE_LOG_D_IF(con,fmt, arg...) \
	do { \
		if(con) DISP_LOG_PRINT(ANDROID_LOG_DEBUG, "fence", fmt, ##arg); \
	}while (0)

void extd_fence_log_enable(bool enable)
{
    extd_fence_on = enable;
    EXTD_FENCE_LOG_D("extd_fence log %s\n", enable? "enabled":"disabled");
}

#ifndef ASSERT
    #define ASSERT(expr)            BUG_ON(!(expr))
#endif

/************************* variable *********************/
static unsigned int isAEEEnabled = 0;
static int fence_clean_up_task_wakeup = 0;

static struct ion_client *ion_client =  NULL;

// how many counters prior to current timeline real-time counter
#define FENCE_STEP_COUNTER         (1)
#define MTK_FB_NO_ION_FD        ((int)(~0U>>1))
#define HW_OVERLAY_COUNT           (4)
#define DISP_DEFAULT_UI_LAYER_ID   (HW_OVERLAY_COUNT-1)



static LIST_HEAD(info_pool_head);
static DEFINE_MUTEX(fence_buffer_mutex);

static struct sw_sync_timeline* sync_timelines[HW_OVERLAY_COUNT];
static struct extd_fence_sync_info fence_info[HW_OVERLAY_COUNT] =
{
		{.layer_id = HW_OVERLAY_COUNT},
		{.layer_id = HW_OVERLAY_COUNT},
		{.layer_id = HW_OVERLAY_COUNT},
		{.layer_id = HW_OVERLAY_COUNT}
};

// ---------------------------------------------------------------------------
//  local function declarations
// ---------------------------------------------------------------------------


/********************ION*****************************************************/

#if defined (MTK_EXT_DISP_ION_SUPPORT)
static void extd_ion_init() {
	if (!ion_client && g_ion_device) {
		ion_client = ion_client_create(g_ion_device,"extd");
		if (!ion_client) {
			EXTD_FENCE_ERR("create ion client failed!\n");
			return;
		}
		EXTD_FENCE_LOG("create ion client 0x%p\n", ion_client);
	}
}

static void extd_ion_deinit() {
	if (ion_client) {
		ion_client_destroy(ion_client);
		ion_client = NULL;
		EXTD_FENCE_LOG("destroy ion client 0x%p\n", ion_client);
	}
}

/**
 * Import ion handle and configure this buffer
 * @client
 * @fd ion shared fd
 * @return ion handle
 */
static struct ion_handle * extd_ion_import_handle(struct ion_client *client, int fd) {
	struct ion_handle *handle = NULL;
	struct ion_mm_data mm_data;
	// If no need Ion support, do nothing!
	if (fd == MTK_FB_NO_ION_FD) {
		EXTD_FENCE_LOG("NO NEED ion support\n");
		return handle;
	}

	if (!ion_client) {
		EXTD_FENCE_ERR("invalid ion client!\n");
		return handle;
	}
	if (fd == MTK_FB_INVALID_ION_FD) {
		EXTD_FENCE_ERR("invalid ion fd!\n");
		return handle;
	}
	handle = ion_import_dma_buf(client, fd);
	if (!handle) {
		EXTD_FENCE_ERR("import ion handle failed!\n");
		return handle;
	}
    mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
    mm_data.config_buffer_param.handle = handle;
    mm_data.config_buffer_param.eModuleID = 0;
    mm_data.config_buffer_param.security = 0;
    mm_data.config_buffer_param.coherent = 0;
/*
	mm_data.config_buffer_param.handle = handle;
    mm_data.config_buffer_param.m4u_port = M4U_PORT_DISP_OVL0;
	mm_data.config_buffer_param.prot = M4U_PROT_READ|M4U_PROT_WRITE;
	mm_data.config_buffer_param.flags = M4U_FLAGS_SEQ_ACCESS;
*/
	
	if(ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA, &mm_data))
	{
		EXTD_FENCE_ERR("configure ion buffer failed!\n");
	}

	EXTD_FENCE_LOG("import ion handle fd=%d,hnd=0x%p\n", fd, handle);

	return handle;
}

static void extd_ion_free_handle(struct ion_client *client, struct ion_handle *handle) {
	if (!ion_client) {
		EXTD_FENCE_ERR("invalid ion client!\n");
		return ;
	}
	if (!handle) {
		return ;
	}
	ion_free(client, handle);
	EXTD_FENCE_LOG("free ion handle 0x%p\n",  handle);
}

static size_t extd_ion_phys_mmu_addr(struct ion_client *client, struct ion_handle *handle, unsigned int *mva) {
	size_t size;
	if (!ion_client) {
		EXTD_FENCE_ERR("invalid ion client!\n");
		return 0;
	}
	if (!handle) {
		return 0;
	}
	ion_phys(client, handle, (ion_phys_addr_t*)mva, &size);
	EXTD_FENCE_LOG("alloc mmu addr hnd=0x%p,mva=0x%08x\n",  handle, (unsigned int)*mva);
	return size;
}

static void extd_ion_cache_flush(struct ion_client *client, struct ion_handle *handle) {
	struct ion_sys_data sys_data;
	if (!ion_client || !handle) {
		return ;
	}
	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.handle = handle;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;

	if (ion_kernel_ioctl(client, ION_CMD_SYSTEM, &sys_data)) {
		EXTD_FENCE_ERR("ion cache flush failed!\n");
	}
}

unsigned int extd_query_buf_mva(unsigned int layer, unsigned int idx) {
	struct extd_fence_sync_info *info;
	struct extd_fence_buf_info *buf;
	unsigned int mva = 0x0;
	ASSERT(layer < HW_OVERLAY_COUNT);
	info = &fence_info[layer];
	if (layer != info->layer_id) {
		EXTD_FENCE_ERR("wrong layer id %d(rt), %d(in)!\n", info->layer_id, layer);
		return 0;
	}
	mutex_lock(&info->mutex_lock);
	list_for_each_entry(buf, &info->buf_list, list) {
		if (buf->idx == idx) {
			mva = buf->mva;
			break;
		}
	}
	mutex_unlock(&info->mutex_lock);
	if (mva != 0x0) {
		if (buf->cache_sync) {
			extd_ion_cache_flush(ion_client, buf->hnd);
		}
		EXTD_FENCE_LOG("query buf mva: layer=%d, idx=%d, mva=0x%08x\n", layer, idx, buf->mva);
	} else {
		//FIXME: non-ion buffer need cache sync here?
		EXTD_FENCE_ERR("cannot find this buf, layer=%d, idx=%d, fence_idx=%d, timeline_idx=%d, cur_idx=%d!\n", layer, idx, info->fence_idx, info->timeline_idx, info->cur_idx);
	}
	return mva;
}
#endif //#if defined (MTK_EXT_DISP_ION_SUPPORT)



/********************Fence*****************************************************/

/**
 * return the next index used to create fence
 */
static unsigned int extd_get_fence_index (unsigned int layer) {
	struct extd_fence_sync_info *info;
	unsigned int index = 0;
	ASSERT(layer < HW_OVERLAY_COUNT);

	info = &fence_info[layer];
	mutex_lock(&info->mutex_lock);
	index = info->fence_idx + FENCE_STEP_COUNTER;
	mutex_unlock(&info->mutex_lock);
	return index;
}

static unsigned int extd_inc_fence_index (unsigned int layer) {
	struct extd_fence_sync_info *info;
	unsigned int index = 0;
	ASSERT(layer < HW_OVERLAY_COUNT);
	info = &fence_info[layer];
	mutex_lock(&info->mutex_lock);
	info->fence_idx += FENCE_STEP_COUNTER;
	index = info->fence_idx;
	mutex_unlock(&info->mutex_lock);

	return index;
}

 void extd_init_fence(void) {
	struct extd_fence_sync_info *info;
    int i =0;
	DISPFUNC();
	EXTD_FENCE_LOG_D("extd init fences\n");
	for(i=0;i<HW_OVERLAY_COUNT;i++)
	{
	 	info = &fence_info[i];
	 	mutex_init(&info->mutex_lock);
	 	info->layer_id = i;
	 	info->fence_idx = 0;
	 	info->timeline_idx = 0;
	 	info->inc = 0;
	 	info->cur_idx = 0;
	    INIT_LIST_HEAD(&info->buf_list);
	}
	return;
}


static struct extd_fence_sync_info* extd_init_fence_info(unsigned int layer) {
	EXTD_FENCE_LOG_D("extd init layer %d fence\n",layer);
	struct extd_fence_sync_info *info;
	ASSERT(layer < HW_OVERLAY_COUNT);

	info = &fence_info[layer];
	mutex_init(&info->mutex_lock);
	info->layer_id = layer;
	info->fence_idx = 0;
	info->timeline_idx = 0;
	info->inc = 0;
	info->cur_idx = 0;
	INIT_LIST_HEAD(&info->buf_list);

	return info;
}

struct extd_fence_buf_info* extd_init_buf_info(struct extd_fence_buf_info *buf) {
	INIT_LIST_HEAD(&buf->list);
	buf->fence = MTK_FB_INVALID_FENCE_FD;
	buf->hnd = NULL;
	buf->idx = 0;
	buf->mva = 0;
	buf->cache_sync = 0;
	return buf;
}

/**
 * Query a @extd_fence_buf_info node from @info_pool_head, if empty create a new one
 */
static struct extd_fence_buf_info* extd_get_buf_info (void) {
	mutex_lock(&fence_buffer_mutex);
	struct extd_fence_buf_info *info;
	if (!list_empty(&info_pool_head)) {
		info = list_first_entry(&info_pool_head, struct extd_fence_buf_info, list);
		list_del_init(&info->list);
		extd_init_buf_info(info);
		mutex_unlock(&fence_buffer_mutex);
		return info;

	} else {
		info = kzalloc(sizeof(struct extd_fence_buf_info), GFP_KERNEL);
		extd_init_buf_info(info);
		EXTD_FENCE_LOG("create new extd_fence_buf_info node 0x%p\n", info);
		mutex_unlock(&fence_buffer_mutex);
		return info;
	}
	mutex_unlock(&fence_buffer_mutex);
}

static void extd_free_buf_pool (void) {
	struct extd_fence_buf_info *pos, *n;
	mutex_lock(&fence_buffer_mutex);
	list_for_each_entry_safe(pos, n, &info_pool_head, list) {
		kzfree(pos);
	}
	mutex_unlock(&fence_buffer_mutex);
}

int extd_fence_timeline_index(unsigned int layer)
{
    int timeline_idx = 0;
	struct extd_fence_sync_info *info = NULL;

    ASSERT(layer < HW_OVERLAY_COUNT);
	info = &fence_info[layer];
    mutex_lock(&info->mutex_lock);
    timeline_idx = info->timeline_idx;
    mutex_unlock(&info->mutex_lock);
    return timeline_idx;
}


/**
 * clean up buffer resources.
 * 1. free ion handle
 * 2. recycle @extd_fence_buf_info node, put it into pool list
 */
void extd_release_buf(unsigned int layer) {
	struct extd_fence_sync_info *info = NULL;
	struct extd_fence_buf_info *buf = NULL;
	struct extd_fence_buf_info *n = NULL;
	struct sw_sync_timeline *timeline = NULL;
	int timeline_idx = 0;
	ASSERT(layer < HW_OVERLAY_COUNT);
	info = &fence_info[layer];
	timeline = sync_timelines[layer];
	if(timeline != NULL ) // make sure fence has been initialized
	{
		mutex_lock(&info->mutex_lock);
	    timeline_idx =	info->timeline_idx;
    	list_for_each_entry_safe(buf,n,&info->buf_list, list) {
    		if (buf->idx <= timeline_idx) {
    			EXTD_FENCE_LOG("release fence buffer-%d-%d\n", layer, buf->idx);
    			list_del_init(&buf->list);
#if defined (MTK_EXT_DISP_ION_SUPPORT)
    			extd_ion_free_handle(ion_client, buf->hnd);
#endif
    			mutex_lock(&fence_buffer_mutex);
    			list_add_tail(&buf->list, &info_pool_head);
    			mutex_unlock(&fence_buffer_mutex);
    		}
    	}
	    mutex_unlock(&info->mutex_lock);
	}
}

/**
 * step forward timeline
 * all fence(sync_point) will be signaled prior to it's index
 * refer to @link sw_sync_timeline_inc
 */
void extd_signal_fence(unsigned int layer,int fence) {
	struct extd_fence_sync_info *info;
	struct extd_fence_buf_info *buf;
	struct extd_fence_buf_info *n;
	struct sw_sync_timeline *timeline;
	int num_fence = 0;
	ASSERT(layer < HW_OVERLAY_COUNT);
	info = &fence_info[layer];
	timeline = sync_timelines[layer];
	if(timeline != NULL) //make sure fence has been initialized
    {
    	num_fence = fence - info->timeline_idx;
		if (num_fence > 0 ) {
			//MMProfileLogEx(EXTD_MMP_Events.SignalSyncFence, MMProfileFlagStart, layer, info->timeline_idx);
			timeline_inc(timeline, num_fence);
			//MMProfileLogEx(EXTD_MMP_Events.SignalSyncFence, MMProfileFlagEnd, layer, timeline->value);
			mutex_lock(&info->mutex_lock);
			info->timeline_idx = fence;
			mutex_unlock(&info->mutex_lock);
		}
		if(fence != timeline->value)
		{
		    EXTD_FENCE_ERR("layer%d fence =%d timevalue %d\n", layer, fence, timeline->value);
		}
		EXTD_FENCE_LOG("layer%d  fence =%d timeidx %d timevalue %d\n", layer, fence, info->timeline_idx,timeline->value);
    }
}

static struct sw_sync_timeline* extd_create_timeline (unsigned int layer) {
	struct sw_sync_timeline *timeline;
	char name[32];
	const char *prefix = "ovl_timeline";
	sprintf(name, "%s-%d", prefix, layer);

	ASSERT(layer < HW_OVERLAY_COUNT);
	//MMProfileLogEx(EXTD_MMP_Events.CreateSyncTimeline, MMProfileFlagPulse, layer, 0);

	timeline = timeline_create(name);
	sync_timelines[layer] = timeline;
	if (timeline == NULL) {
		EXTD_FENCE_ERR("cannot create timeline! \n");
	}
	EXTD_FENCE_LOG("create timeline: %s\n", name);
	return timeline;
}

static struct fence_data extd_create_fence(unsigned int layer) {
	struct extd_fence_sync_info *info;
	struct sw_sync_timeline *timeline;
	int fenceFd = MTK_FB_INVALID_FENCE_FD;
	struct fence_data data;
	const char *prefix = "ovl_fence";
	ASSERT(layer < HW_OVERLAY_COUNT);
	// Init invalid fd
	data.fence = fenceFd;
	data.value = extd_get_fence_index(layer);
	sprintf(data.name, "%s-%d-%d", prefix, layer, data.value);

	info = &fence_info[layer];
	timeline = sync_timelines[layer];

	// The first time create fence, need to initialize timeline and extd_fence_sync_info
	if (timeline == NULL) {
		timeline = extd_create_timeline(layer);
		extd_init_fence_info(layer);
	}

	if (layer != info->layer_id) {
		if (info->layer_id != HW_OVERLAY_COUNT)
			EXTD_FENCE_ERR("wrong layer id %d(rt), %d(in)!\n", info->layer_id, layer);
		return data;
	}
	//MMProfileLogEx(EXTD_MMP_Events.CreateSyncFence, MMProfileFlagStart, layer, 0);

	if (timeline != NULL) {
		if (fence_create(timeline, &data)) {
			EXTD_FENCE_ERR("layer%d create Fence Object failed!\n", layer);
		} else {
			extd_inc_fence_index(layer);
			fenceFd = data.fence;
		}
		EXTD_FENCE_LOG("create fence %s(%d)\n", data.name, fenceFd);
	} else {
		EXTD_FENCE_ERR("layer%d has no Timeline!\n", layer);
	}


	//MMProfileLogEx(EXTD_MMP_Events.CreateSyncFence, MMProfileFlagEnd, data.value, fenceFd);

	return data;
}

/**
 * signal fence and release buffer
 * layer: set layer
 * fence: signal fence which value is not bigger than this param
 */
void extd_release_fence(unsigned int layer,int fence) 
{
	struct extd_fence_sync_info *info;
	struct extd_fence_buf_info *buf;
	struct extd_fence_buf_info *n;
	struct sw_sync_timeline *timeline;
	int num_fence = 0;

	///DISPFUNC();
	ASSERT(layer < HW_OVERLAY_COUNT);
	info = &fence_info[layer];
	timeline = sync_timelines[layer];
	if(timeline != NULL)
    {
		mutex_lock(&info->mutex_lock);
    	num_fence = fence - info->timeline_idx;
		mutex_unlock(&info->mutex_lock);
		if (num_fence > 0 ) {
           // MMProfileLogEx(EXTD_MMP_Events.SignalSyncFence, MMProfileFlagStart, layer, info->timeline_idx);
			timeline_inc(timeline, num_fence);
           // MMProfileLogEx(EXTD_MMP_Events.SignalSyncFence, MMProfileFlagEnd, layer, timeline->value);
		}
		mutex_lock(&info->mutex_lock);
		if(num_fence > 0)
		{
			info->timeline_idx = fence;
		}
		list_for_each_entry_safe(buf,n,&info->buf_list, list) {
			if (buf->idx <= fence) {
				list_del_init(&buf->list);
#if defined (MTK_EXT_DISP_ION_SUPPORT)
				extd_ion_free_handle(ion_client, buf->hnd);
#endif
				///DISPMSG("%s, %d\n", __func__, __LINE__);
				mutex_lock(&fence_buffer_mutex);
				///DISPMSG("%s, %d\n", __func__, __LINE__);
				list_add_tail(&buf->list, &info_pool_head);	
				///DISPMSG("%s, %d\n", __func__, __LINE__);
				mutex_unlock(&fence_buffer_mutex);
				///DISPMSG("%s, %d\n", __func__, __LINE__);
			}
	    }

		///DISPMSG("%s, %d\n", __func__, __LINE__);
		mutex_unlock(&info->mutex_lock);
		///DISPMSG("%s, %d\n", __func__, __LINE__);
		EXTD_FENCE_LOG_D_IF((fence != timeline->value),"layer%d fence =%d timevalue %d\n", layer, fence, timeline->value);
		EXTD_FENCE_LOG("layer%d f %d tl %d inc %d max %d\n", layer, fence, info->timeline_idx, num_fence,info->fence_idx);
    }
}

void extd_release_layer_fence(unsigned int layer) 
{
	///DISPFUNC();

	struct extd_fence_sync_info *info;
	int fence = 0;
	ASSERT(layer < HW_OVERLAY_COUNT);
	info = &fence_info[layer];
	mutex_lock(&info->mutex_lock);
	fence = info->fence_idx;
	mutex_unlock(&info->mutex_lock);
	EXTD_FENCE_LOG_D("layer%d release all fence %d\n", layer, fence);
	extd_release_fence(layer,fence);
}

/**
 * 1. query a @extd_fence_buf_info list node
 * 2. create fence object
 * 3. create ion mva
 * 4. save fence fd, mva to @extd_fence_buf_info node
 * 5. add @extd_fence_buf_info node to @extd_fence_sync_info.buf_list
 * @buf struct @fb_overlay_buffer
 * @return struct @extd_fence_buf_info
 */
struct extd_fence_buf_info* extd_prepare_buf(struct fb_overlay_buffer_t *buf)
{
	struct extd_fence_sync_info *info;
	struct sw_sync_timeline *timeline;
	struct extd_fence_buf_info *buf_info = NULL;
	struct fence_data data;
	struct ion_handle * handle = NULL;
	unsigned int mva = 0x0;
	const int layer = buf->layer_id;
	ASSERT(layer < HW_OVERLAY_COUNT);
	///DISPFUNC();
	//EXTD_FENCE_LOG("prepare 0x%08x\n", (buf->layer_id<<24)|(buf->layer_en<<16)|(buf->ion_fd==MTK_FB_NO_ION_FD?0:buf->ion_fd));
	info = &fence_info[layer];
	timeline = sync_timelines[layer];
	if (isAEEEnabled && (layer == DISP_DEFAULT_UI_LAYER_ID)) {
		EXTD_FENCE_ERR("Maybe hang: AEE is enabled, HWC cannot use FB_LAYER again!");
		return NULL;
	}
	if (layer != info->layer_id) {
		// Reset sync_fence info
		extd_init_fence_info(layer);
	}
	// The first time create fence, need to initialize timeline and extd_fence_sync_info
	if (timeline == NULL) {
		timeline = extd_create_timeline(layer);
	}
	buf_info = extd_get_buf_info();
	// Fence Object
	data = extd_create_fence(buf->layer_id);

#if defined (MTK_EXT_DISP_ION_SUPPORT)
	if (!ion_client) {
		extd_ion_init();
	}
	handle = extd_ion_import_handle(ion_client, buf->ion_fd);
	extd_ion_phys_mmu_addr(ion_client, handle, &mva);
#endif
	buf_info->fence = data.fence;
	buf_info->idx = data.value;
	buf_info->hnd = handle;
	buf_info->mva = mva;
	buf_info->cache_sync = buf->cache_sync;
	mutex_lock(&info->mutex_lock);
	list_add_tail(&buf_info->list, &info->buf_list);
	mutex_unlock(&info->mutex_lock);
	EXTD_FENCE_ERR("prepare 0x%08x, fence %d, idx %d\n", (buf->layer_id<<24)|(buf->layer_en<<16)|(buf->ion_fd==MTK_FB_NO_ION_FD?0:buf->ion_fd),buf_info->fence, buf_info->idx);
	return buf_info;
}

/**
 * FIXME: Global spinlock's initialization must not be in PCI probe function,
 * so switch to static initialization to fix this theoretical bugs
 * Now we just create the timeline of specified Layer when create its first
 * fence object
 */
static void extd_hw_sync_destroy(void) {
	struct sw_sync_timeline *timeline;
	unsigned int layer = 0;
    for (layer = 0; layer < HW_OVERLAY_COUNT; layer++) {
    	timeline = sync_timelines[layer];
    	if (timeline != NULL) {
    		EXTD_FENCE_LOG("destroy timeline %s(%d)\n", timeline->obj.name, timeline->value);
    		timeline_destroy(timeline);
    		sync_timelines[layer] = NULL;
    	}
    	extd_release_buf(layer);
    	extd_init_fence_info(layer);
    }
    extd_free_buf_pool();
  #if defined (MTK_EXT_DISP_ION_SUPPORT)
    extd_ion_deinit();
  #endif
}



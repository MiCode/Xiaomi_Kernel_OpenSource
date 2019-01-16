#include <mach/mt_typedefs.h>
#include <linux/types.h>
#include "primary_display.h"
#include "ddp_hal.h"
#include "disp_drv_log.h"
#include <linux/disp_assert_layer.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include "ddp_mmp.h"
#include "disp_drv_platform.h"

///common part
#define DAL_BPP             (2)
#define DAL_WIDTH           (DISP_GetScreenWidth())
#define DAL_HEIGHT          (DISP_GetScreenHeight())

//#ifdef CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER

#include <linux/string.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

#include "mtkfb_console.h"

// ---------------------------------------------------------------------------
#define DAL_FORMAT          (eRGB565)
#define DAL_BG_COLOR        (dal_bg_color)
#define DAL_FG_COLOR        (dal_fg_color)

#define RGB888_To_RGB565(x) ((((x) & 0xF80000) >> 8) |                      \
                             (((x) & 0x00FC00) >> 5) |                      \
                             (((x) & 0x0000F8) >> 3))

#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))

#define DAL_LOCK()                                                          \
    do {                                                                    \
        if (down_interruptible(&dal_sem)) {                                 \
            DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Can't get semaphore in %s()\n",          \
                   __FUNCTION__);                                           \
            return DAL_STATUS_LOCK_FAIL;                                    \
        }                                                                   \
    } while (0)
    
#define DAL_UNLOCK()                                                        \
    do {                                                                    \
        up(&dal_sem);                                                       \
    } while (0)


#define DAL_CHECK_MFC_RET(expr)                                             \
    do {                                                                    \
        MFC_STATUS ret = (expr);                                            \
        if (MFC_STATUS_OK != ret) {                                         \
            DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Warning: call MFC_XXX function failed "           \
                   "in %s(), line: %d, ret: %x\n",                          \
                   __FUNCTION__, __LINE__, ret);                            \
            return ret;                                                     \
        }                                                                   \
    } while (0)


#define DAL_CHECK_DISP_RET(expr)                                            \
    do {                                                                    \
        DISP_STATUS ret = (expr);                                           \
        if (DISP_STATUS_OK != ret) {                                        \
            DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Warning: call DISP_XXX function failed "          \
                   "in %s(), line: %d, ret: %x\n",                          \
                   __FUNCTION__, __LINE__, ret);                            \
            return ret;                                                     \
        }                                                                   \
    } while (0)

#define DAL_LOG(fmt, arg...) \
    do { \
        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", fmt, ##arg); \
    }while (0)
// ---------------------------------------------------------------------------

static MFC_HANDLE mfc_handle = NULL;

static void *dal_fb_addr = NULL;  
static UINT32 dal_fb_pa = 0;
unsigned int isAEEEnabled = 0;
extern BOOL is_early_suspended;
extern struct semaphore sem_early_suspend;

//static BOOL  dal_shown   = FALSE;
BOOL  dal_shown   = FALSE;
static BOOL  dal_enable_when_resume = FALSE;
static BOOL  dal_disable_when_resume = FALSE;
static unsigned int dal_fg_color = RGB888_To_RGB565(DAL_COLOR_WHITE);
static unsigned int dal_bg_color = RGB888_To_RGB565(DAL_COLOR_RED);

extern struct mutex OverlaySettingMutex;
extern atomic_t OverlaySettingDirtyFlag;
extern atomic_t OverlaySettingApplied;
extern OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];

primary_disp_input_config primary_input[PRIMARY_DISPLAY_SESSION_LAYER_COUNT];

//DECLARE_MUTEX(dal_sem);
DEFINE_SEMAPHORE(dal_sem);

static char dal_print_buffer[1024];
// ---------------------------------------------------------------------------



UINT32 DAL_GetLayerSize(void)
{
	// xuecheng, avoid lcdc read buffersize+1 issue
    return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

static DAL_STATUS DAL_SetRedScreen(UINT32 *addr)
{
    UINT32 i;
    const UINT32 BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_BG_COLOR, DAL_BG_COLOR);

    for(i = 0; i < DAL_GetLayerSize() / sizeof(UINT32); ++ i) {
        *addr ++ = BG_COLOR;
    }
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_SetScreenColor(DAL_COLOR color)
{
#if 1
	UINT32 i;
	UINT32 size;
	UINT32 BG_COLOR;
	MFC_CONTEXT *ctxt =NULL;
	UINT32 offset;
	unsigned int *addr;
	color=RGB888_To_RGB565(color);
	BG_COLOR = MAKE_TWO_RGB565_COLOR(color, color);
	
	ctxt = (MFC_CONTEXT *)mfc_handle;
	if(!ctxt)
		return DAL_STATUS_FATAL_ERROR; 
	if(ctxt->screen_color==color)
		return DAL_STATUS_OK;
	offset =  MFC_Get_Cursor_Offset(mfc_handle);
	addr=(unsigned int *)(ctxt->fb_addr+offset);
	
	size= DAL_GetLayerSize()-offset;
	for(i = 0; i < size/ sizeof(UINT32); ++ i) 
	{
	    *addr ++ = BG_COLOR;
	}
	ctxt->screen_color=color;

    return DAL_STATUS_OK;
}
DAL_STATUS DAL_Init(unsigned long layerVA, unsigned long layerPA)
{
    printk("%s, 64: layerVA=0x%lx, layerPA=0x%lx\n", __func__, layerVA, layerPA);

    dal_fb_addr = (void *)layerVA;
    dal_fb_pa = layerPA;

    //printk("%s, 32: layerVA=0x%08x, layerPA=0x%08x\n", __func__, dal_fb_addr, dal_fb_pa);
    DAL_CHECK_MFC_RET(MFC_Open(&mfc_handle, dal_fb_addr,
                               DAL_WIDTH, DAL_HEIGHT, DAL_BPP,
                               DAL_FG_COLOR, DAL_BG_COLOR));

    //DAL_Clean();
    DAL_SetScreenColor(DAL_COLOR_RED);

    return DAL_STATUS_OK;
}


DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
    if (NULL == mfc_handle) 
        return DAL_STATUS_NOT_READY;

    DAL_LOCK();
    dal_fg_color = RGB888_To_RGB565(fgColor);
    dal_bg_color = RGB888_To_RGB565(bgColor);
    DAL_CHECK_MFC_RET(MFC_SetColor(mfc_handle, dal_fg_color, dal_bg_color));
    DAL_UNLOCK();

    return DAL_STATUS_OK;
}

DAL_STATUS DAL_Dynamic_Change_FB_Layer(unsigned int isAEEEnabled)
{
#if 0
    static int ui_layer_tdshp = 0;

    printk("[DDP] DAL_Dynamic_Change_FB_Layer, isAEEEnabled=%d \n", isAEEEnabled);
    
    if(DISP_DEFAULT_UI_LAYER_ID==DISP_CHANGED_UI_LAYER_ID)
    {
    	 printk("[DDP] DAL_Dynamic_Change_FB_Layer, no dynamic switch \n");
    	 return DAL_STATUS_OK;
    }
    
    if(isAEEEnabled==1)
    {
        // change ui layer from DISP_DEFAULT_UI_LAYER_ID to DISP_CHANGED_UI_LAYER_ID
        memcpy((void*)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]), 
               (void*)(&cached_layer_config[DISP_DEFAULT_UI_LAYER_ID]), 
               sizeof(OVL_CONFIG_STRUCT));
        ui_layer_tdshp = cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp;
        cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp = 0;
        disp_path_change_tdshp_status(DISP_DEFAULT_UI_LAYER_ID, 0); // change global variable value, else error-check will find layer 2, 3 enable tdshp together
        FB_LAYER = DISP_CHANGED_UI_LAYER_ID;
    }
    else
    {
        memcpy((void*)(&cached_layer_config[DISP_DEFAULT_UI_LAYER_ID]), 
               (void*)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]), 
               sizeof(OVL_CONFIG_STRUCT));
        cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp = ui_layer_tdshp;
        FB_LAYER = DISP_DEFAULT_UI_LAYER_ID;
        memset((void*)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]), 0, sizeof(OVL_CONFIG_STRUCT));
    }

    // no matter memcpy or memset, layer ID should not be changed
    cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].layer = DISP_DEFAULT_UI_LAYER_ID;
    cached_layer_config[DISP_CHANGED_UI_LAYER_ID].layer = DISP_CHANGED_UI_LAYER_ID;
    cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isDirty = 1;
    cached_layer_config[DISP_CHANGED_UI_LAYER_ID].isDirty = 1;
#endif
    return DAL_STATUS_OK;
}

DAL_STATUS DAL_Clean(void)
{
    const UINT32 BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_BG_COLOR, DAL_BG_COLOR);
    DAL_STATUS ret = DAL_STATUS_OK;

    static int dal_clean_cnt = 0;
    MFC_CONTEXT *ctxt = (MFC_CONTEXT *)mfc_handle;
    printk("[MTKFB_DAL] DAL_Clean\n");
    if (NULL == mfc_handle) 
        return DAL_STATUS_NOT_READY;

//    if (LCD_STATE_POWER_OFF == LCD_GetState())
//        return DAL_STATUS_LCD_IN_SUSPEND;
	
	MMProfileLogEx(ddp_mmp_get_events()->dal_clean, MMProfileFlagStart, 0, 0);

    DAL_LOCK();

    DAL_CHECK_MFC_RET(MFC_ResetCursor(mfc_handle));

    ctxt->screen_color=0;
    DAL_SetScreenColor(DAL_COLOR_RED);
 
/*
    if (LCD_STATE_POWER_OFF == LCD_GetState()) {
	    DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "dal_clean in power off\n");
        dal_disable_when_resume = TRUE;
        ret = DAL_STATUS_LCD_IN_SUSPEND;
        goto End;
    }
    */
 
    //xuecheng, for debug
#if 0
    if(is_early_suspended){
        up(&sem_early_suspend);
        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "dal_clean in power off\n");
        goto End;
    }
#endif

    //TODO: if dal_shown=false, and 3D enabled, mtkfb may disable UI layer, please modify 3D driver
    if(isAEEEnabled==1)
    {
        primary_disp_input_config *input= &primary_input[0];
		input->addr = (unsigned int)dal_fb_pa;
		input->layer = primary_display_get_option("ASSERT_LAYER");
		input->layer_en = 0;
		input->src_x = 0;
		input->src_y = 0;
		input->src_w = DAL_WIDTH;
		input->src_h = DAL_HEIGHT;
		input->dst_x = 0;
		input->dst_y = 0;
		input->dst_w = DAL_WIDTH;
		input->dst_h = DAL_HEIGHT;
		input->alpha = 0x80;
		input->aen = 1;
		input->buff_idx = -1;
		input->src_pitch = DAL_WIDTH * DAL_BPP;
		input->dirty = 1;
		input->fmt = DAL_FORMAT;

		ret = primary_display_config_input_multiple(&primary_input);

        // DAL disable, switch UI layer to default layer 3
        printk("[DDP]* isAEEEnabled from 1 to 0, %d \n", dal_clean_cnt++);
        isAEEEnabled = 0;
        DAL_Dynamic_Change_FB_Layer(isAEEEnabled);  // restore UI layer to DEFAULT_UI_LAYER
    }
    
    dal_shown = FALSE;
    dal_disable_when_resume = FALSE;

    primary_display_trigger(0, NULL, 0);
                                         

End:
    DAL_UNLOCK();
	MMProfileLogEx(ddp_mmp_get_events()->dal_clean, MMProfileFlagEnd, 0, 0);
    return ret;
}

int is_DAL_Enabled(void)
{
	int ret = 0;
	DAL_LOCK();
	ret = isAEEEnabled;
	DAL_UNLOCK();

	return ret;
}

DAL_STATUS DAL_Printf(const char *fmt, ...)
{
    va_list args;
    uint i;
    DAL_STATUS ret = DAL_STATUS_OK;
    printk("%s", __func__);
    //printk("[MTKFB_DAL] DAL_Printf mfc_handle=0x%08X, fmt=0x%08X\n", mfc_handle, fmt);
    if (NULL == mfc_handle) 
        return DAL_STATUS_NOT_READY;
    
    if (NULL == fmt)
        return DAL_STATUS_INVALID_ARGUMENT;

	
		MMProfileLogEx(ddp_mmp_get_events()->dal_printf, MMProfileFlagStart, 0, 0);
    DAL_LOCK();
     if(isAEEEnabled==0)
    {
        printk("[DDP] isAEEEnabled from 0 to 1, ASSERT_LAYER=%d, dal_fb_pa %x\n", 
            ASSERT_LAYER, dal_fb_pa);
            
        isAEEEnabled = 1;
        DAL_Dynamic_Change_FB_Layer(isAEEEnabled); // default_ui_ layer coniig to changed_ui_layer
        
        DAL_CHECK_MFC_RET(MFC_Open(&mfc_handle, dal_fb_addr,
                                   DAL_WIDTH, DAL_HEIGHT, DAL_BPP,
                                   DAL_FG_COLOR, DAL_BG_COLOR));        
        //DAL_Clean();  

        primary_disp_input_config *input= &primary_input[0];
		input->addr = (unsigned int)dal_fb_pa;
		input->layer = primary_display_get_option("ASSERT_LAYER");
		input->layer_en = 1;
		input->src_x = 0;
		input->src_y = 0;
		input->src_w = DAL_WIDTH;
		input->src_h = DAL_HEIGHT;
		input->dst_x = 0;
		input->dst_y = 0;
		input->dst_w = DAL_WIDTH;
		input->dst_h = DAL_HEIGHT;
		input->alpha = 0x80;
		input->aen = 1;
		input->buff_idx = -1;
		input->src_pitch = DAL_WIDTH * DAL_BPP;
		input->dirty = 1;
		input->fmt = DAL_FORMAT;

		ret = primary_display_config_input_multiple(&primary_input);
    }
    va_start (args, fmt);
    i = vsprintf(dal_print_buffer, fmt, args);
    BUG_ON(i>=ARRAY_SIZE(dal_print_buffer));
    va_end (args);
    DAL_CHECK_MFC_RET(MFC_Print(mfc_handle, dal_print_buffer));

    flush_cache_all();
/*
    if (LCD_STATE_POWER_OFF == LCD_GetState()) {
        ret = DAL_STATUS_LCD_IN_SUSPEND;
        dal_enable_when_resume = TRUE;
        goto End;
    }
    */

#if 0
    if(is_early_suspended){
        up(&sem_early_suspend);
        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "DAL_Printf in power off\n");
        goto End;
    }
#endif

    if (!dal_shown) {
        dal_shown = TRUE;
    }
	ret = primary_display_trigger(0, NULL, 0);

End:
	
    DAL_UNLOCK();
	
	MMProfileLogEx(ddp_mmp_get_events()->dal_printf, MMProfileFlagEnd, 0, 0);

    return ret;
}


DAL_STATUS DAL_OnDispPowerOn(void)
{
#if 0
    DAL_LOCK();

    /* Re-enable assertion layer when display resumes */
    
    if (is_early_suspended)
    {
		if(dal_enable_when_resume) 
		{
        	dal_enable_when_resume = FALSE;
        	if (!dal_shown) 
        	{
                mutex_lock(&OverlaySettingMutex);
        		cached_layer_config[ASSERT_LAYER].src_x = 0;
        		cached_layer_config[ASSERT_LAYER].src_y = 0;
        		cached_layer_config[ASSERT_LAYER].src_w = DAL_WIDTH;
        		cached_layer_config[ASSERT_LAYER].src_h = DAL_HEIGHT;
        		cached_layer_config[ASSERT_LAYER].dst_x = 0;
        		cached_layer_config[ASSERT_LAYER].dst_y = 0;
        		cached_layer_config[ASSERT_LAYER].dst_w = DAL_WIDTH;
        		cached_layer_config[ASSERT_LAYER].dst_h = DAL_HEIGHT;
                cached_layer_config[ASSERT_LAYER].layer_en= TRUE;
                cached_layer_config[ASSERT_LAYER].isDirty = true;
                atomic_set(&OverlaySettingDirtyFlag, 1);
                atomic_set(&OverlaySettingApplied, 0);
                mutex_unlock(&OverlaySettingMutex);
            	dal_shown = TRUE;
        	}
			goto End;
    	}
		else if(dal_disable_when_resume)
		{
			dal_disable_when_resume = FALSE;
			mutex_lock(&OverlaySettingMutex);
            cached_layer_config[ASSERT_LAYER].layer_en= FALSE;
            cached_layer_config[ASSERT_LAYER].isDirty = true;
            atomic_set(&OverlaySettingDirtyFlag, 1);
            atomic_set(&OverlaySettingApplied, 0);
            mutex_unlock(&OverlaySettingMutex);
			dal_shown = FALSE;
			goto End;

		}
	}

End:
    DAL_UNLOCK();
#endif
    return DAL_STATUS_OK;
}

// ##########################################################################
//  !CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER
// ##########################################################################
#else
unsigned int isAEEEnabled = 0;

UINT32 DAL_GetLayerSize(void)
{
	// xuecheng, avoid lcdc read buffersize+1 issue
    return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

DAL_STATUS DAL_Init(UINT32 layerVA, UINT32 layerPA)
{
    NOT_REFERENCED(layerVA);
    NOT_REFERENCED(layerPA);
    
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
    NOT_REFERENCED(fgColor);
    NOT_REFERENCED(bgColor);

    return DAL_STATUS_OK;
}
DAL_STATUS DAL_Clean(void)
{
    printk("[MTKFB_DAL] DAL_Clean is not implemented\n");
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_Printf(const char *fmt, ...)
{
    NOT_REFERENCED(fmt);
    printk("[MTKFB_DAL] DAL_Printf is not implemented\n");
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_OnDispPowerOn(void)
{
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_SetScreenColor(DAL_COLOR color)
{
    return DAL_STATUS_OK;
}
#endif  // CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER

EXPORT_SYMBOL(DAL_SetColor);
EXPORT_SYMBOL(DAL_SetScreenColor);
EXPORT_SYMBOL(DAL_Printf);
EXPORT_SYMBOL(DAL_Clean);
#ifdef DAL_LOWMEMORY_ASSERT
EXPORT_SYMBOL(DAL_LowMemoryOn);
EXPORT_SYMBOL(DAL_LowMemoryOff);
#endif

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <linux/semaphore.h>
#include <linux/xlog.h>
#include <linux/mutex.h>
#include <linux/leds-mt65xx.h>
#include <linux/suspend.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
//#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

#include <mach/dma.h>
//#include <mach/irqs.h>
#include <linux/dma-mapping.h>
#include <linux/compat.h>
#include "mach/mt_boot.h"

#include "debug.h"

#include "ddp_hal.h"
#include "disp_drv_log.h"

#include "disp_lcm.h"
#include "mtkfb.h"
#include "mtkfb_console.h"
#include "mtkfb_fence.h"
#include "mtkfb_info.h"
#include "ddp_ovl.h"
#include "disp_drv_platform.h"
#include "primary_display.h"
#include "ddp_dump.h"
#include "display_recorder.h"
#include "fbconfig_kdebug_rome.h"


#include "mtk_ovl.h"
#include "mt_boot.h"
#include "disp_helper.h"
#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))
	

extern unsigned int isAEEEnabled;

struct notifier_block pm_nb;
unsigned int EnableVSyncLog = 0;

static u32 MTK_FB_XRES  = 0;
static u32 MTK_FB_YRES  = 0;
static u32 MTK_FB_BPP   = 0;
static u32 MTK_FB_PAGES = 0;
static u32 fb_xres_update = 0;
static u32 fb_yres_update = 0;
#ifdef CONFIG_CM865_MAINBOARD  //add by longcheer_liml_2015_11_13
extern void bq24296_set_otg_config(kal_uint32 val);
#endif

#define MTK_FB_XRESV (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT))
#define MTK_FB_YRESV (ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * MTK_FB_PAGES) /* For page flipping */
#define MTK_FB_BYPP  ((MTK_FB_BPP + 7) >> 3)
#define MTK_FB_LINE  (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) * MTK_FB_BYPP)
#define MTK_FB_SIZE  (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT))

#define MTK_FB_SIZEV (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * MTK_FB_PAGES)

#define CHECK_RET(expr)    \
    do {                   \
        int ret = (expr);  \
        ASSERT(0 == ret);  \
    } while (0)


static size_t mtkfb_log_on = 0;
#define MTKFB_LOG(fmt, arg...) \
    do { \
        if (mtkfb_log_on) pr_notice("[MTKFB]"fmt, ##arg); \
    }while (0)
// always show this debug info while the global debug log is off
#define MTKFB_LOG_DBG(fmt, arg...) \
    do { \
        if (!mtkfb_log_on) pr_notice("[MTKFB]"fmt, ##arg); \
    }while (0)

#define MTKFB_FUNC()	\
	do { \
		if(mtkfb_log_on) pr_notice("[MTKFB Func]%s\n", __func__); \
	}while (0)

#define PRNERR(fmt, args...)   pr_err("[MTKFB]"fmt, ##args);

void mtkfb_log_enable(int enable)
{
    mtkfb_log_on = enable;
	MTKFB_LOG("mtkfb log %s\n", enable?"enabled":"disabled");
}

// ---------------------------------------------------------------------------
//  local variables
// ---------------------------------------------------------------------------

unsigned long fb_pa = 0;

static const struct timeval FRAME_INTERVAL = {0, 30000};  // 33ms

atomic_t has_pending_update = ATOMIC_INIT(0);
struct fb_overlay_layer video_layerInfo;
UINT32 dbr_backup = 0;
UINT32 dbg_backup = 0;
UINT32 dbb_backup = 0;
bool fblayer_dither_needed = false;
static UINT32 mtkfb_using_layer_type = LAYER_2D;
static bool	hwc_force_fb_enabled = true;
bool is_ipoh_bootup = false;
struct fb_info         *mtkfb_fbi;
struct fb_overlay_layer fb_layer_context;
mtk_dispif_info_t dispif_info[MTKFB_MAX_DISPLAY_COUNT];
extern unsigned int isAEEEnabled;

/* This mutex is used to prevent tearing due to page flipping when adbd is
   reading the front buffer
*/
DEFINE_SEMAPHORE(sem_flipping);
DEFINE_SEMAPHORE(sem_early_suspend);
DEFINE_SEMAPHORE(sem_overlay_buffer);

DEFINE_MUTEX(OverlaySettingMutex);
atomic_t OverlaySettingDirtyFlag = ATOMIC_INIT(0);
atomic_t OverlaySettingApplied = ATOMIC_INIT(0);
unsigned int PanDispSettingPending = 0;
unsigned int PanDispSettingDirty = 0;
unsigned int PanDispSettingApplied = 0;

DECLARE_WAIT_QUEUE_HEAD(reg_update_wq);

unsigned int need_esd_check = 0;
DECLARE_WAIT_QUEUE_HEAD(esd_check_wq);

extern unsigned int disp_running;
extern wait_queue_head_t disp_done_wq;

DEFINE_MUTEX(ScreenCaptureMutex);

BOOL is_early_suspended = FALSE;
static int sem_flipping_cnt = 1;
static int sem_early_suspend_cnt = 1;
static int sem_overlay_buffer_cnt = 1;
static int vsync_cnt = 0;

extern BOOL is_engine_in_suspend_mode;
extern BOOL is_lcm_in_suspend_mode;

// ---------------------------------------------------------------------------
//  local function declarations
// ---------------------------------------------------------------------------

static int init_framebuffer(struct fb_info *info);
static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info* layerInfo);
static int mtkfb_update_screen(struct fb_info *info);
static void mtkfb_update_screen_impl(void);
unsigned int mtkfb_fm_auto_test(void);


#ifdef CONFIG_OF
extern int _parse_tag_videolfb(void);
#endif
// ---------------------------------------------------------------------------
//  Timer Routines
// ---------------------------------------------------------------------------
static struct task_struct *screen_update_task = NULL;
static struct task_struct *esd_recovery_task = NULL;
unsigned int lcd_fps = 6000;
wait_queue_head_t screen_update_wq;
extern BOOL dal_shown;


/*
 * ---------------------------------------------------------------------------
 *  mtkfb_set_lcm_inited() will be called in mt6516_board_init()
 * ---------------------------------------------------------------------------
 */
static BOOL is_lcm_inited = FALSE;
void mtkfb_set_lcm_inited(BOOL inited)
{
    is_lcm_inited = inited;
}

/*
 * ---------------------------------------------------------------------------
 * fbdev framework callbacks and the ioctl interface
 * ---------------------------------------------------------------------------
 */
/* Called each time the mtkfb device is opened */
static int mtkfb_open(struct fb_info *info, int user)
{
    NOT_REFERENCED(info);
    NOT_REFERENCED(user);
	DISPFUNC();
    MSG_FUNC_ENTER();
    MSG_FUNC_LEAVE();
    return 0;
}

/* Called when the mtkfb device is closed. We make sure that any pending
 * gfx DMA operations are ended, before we return. */
static int mtkfb_release(struct fb_info *info, int user)
{

    NOT_REFERENCED(info);
    NOT_REFERENCED(user);
	DISPFUNC();

    MSG_FUNC_ENTER();
    MSG_FUNC_LEAVE();
    return 0;
}

/* Store a single color palette entry into a pseudo palette or the hardware
 * palette if one is available. For now we support only 16bpp and thus store
 * the entry only to the pseudo palette.
 */
static int mtkfb_setcolreg(u_int regno, u_int red, u_int green,
                           u_int blue, u_int transp,
                           struct fb_info *info)
{
    int r = 0;
    unsigned bpp, m;

    NOT_REFERENCED(transp);

    MSG_FUNC_ENTER();

    bpp = info->var.bits_per_pixel;
    m = 1 << bpp;
    if (regno >= m)
    {
        r = -EINVAL;
        goto exit;
    }

    switch (bpp)
    {
    case 16:
        /* RGB 565 */
        ((u32 *)(info->pseudo_palette))[regno] =
            ((red & 0xF800) |
            ((green & 0xFC00) >> 5) |
            ((blue & 0xF800) >> 11));
        break;
    case 32:
        /* ARGB8888 */
        ((u32 *)(info->pseudo_palette))[regno] =
             (0xff000000)           |
            ((red   & 0xFF00) << 8) |
            ((green & 0xFF00)     ) |
            ((blue  & 0xFF00) >> 8);
        break;

    // TODO: RGB888, BGR888, ABGR8888

    default:
        ASSERT(0);
    }

exit:
    MSG_FUNC_LEAVE();
    return r;
}

static void mtkfb_update_screen_impl(void)
{
    BOOL down_sem = FALSE;
    MTKFB_FUNC();
    MMProfileLog(MTKFB_MMP_Events.UpdateScreenImpl, MMProfileFlagStart);
    if (down_interruptible(&sem_overlay_buffer)) {
        DISPMSG("[FB Driver] can't get semaphore in mtkfb_update_screen_impl()\n");
	} else {
        down_sem = TRUE;
        sem_overlay_buffer_cnt--;
    }

	//DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update));

    if(down_sem){
		sem_overlay_buffer_cnt++;
		up(&sem_overlay_buffer);
	}
	MMProfileLog(MTKFB_MMP_Events.UpdateScreenImpl, MMProfileFlagEnd);
}


static int mtkfb_update_screen(struct fb_info *info)
{
	MTKFB_FUNC();
    if (down_interruptible(&sem_early_suspend)) {
        DISPMSG("[FB Driver] can't get semaphore in mtkfb_update_screen()\n");
        return -ERESTARTSYS;
    }
	sem_early_suspend_cnt--;
    if (primary_display_is_sleepd()) goto End;
    mtkfb_update_screen_impl();

End:
	sem_early_suspend_cnt++;
    up(&sem_early_suspend);
    return 0;
}
static unsigned int BL_level = 0;
static BOOL BL_set_level_resume = FALSE;
int mtkfb_set_backlight_level(unsigned int level)
{
	MTKFB_FUNC();
	DISPMSG("mtkfb_set_backlight_level:%d Start\n", level);
	primary_display_setbacklight(level);
	DISPMSG("mtkfb_set_backlight_level End\n");
    return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_level);

int mtkfb_set_backlight_mode(unsigned int mode)
{
	MTKFB_FUNC();
	if (down_interruptible(&sem_flipping)) {
        DISPMSG("[FB Driver] can't get semaphore:%d\n", __LINE__);
        return -ERESTARTSYS;
    }
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
        DISPMSG("[FB Driver] can't get semaphore:%d\n", __LINE__);
		sem_flipping_cnt++;
		up(&sem_flipping);
        return -ERESTARTSYS;
    }

	sem_early_suspend_cnt--;
    if (primary_display_is_sleepd()) goto End;

	//DISP_SetBacklight_mode(mode);
End:
	sem_flipping_cnt++;
	sem_early_suspend_cnt++;
    up(&sem_early_suspend);
	up(&sem_flipping);
    return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_mode);


int mtkfb_set_backlight_pwm(int div)
{
	MTKFB_FUNC();
	if (down_interruptible(&sem_flipping)) {
        DISPMSG("[FB Driver] can't get semaphore:%d\n", __LINE__);
        return -ERESTARTSYS;
    }
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
        DISPMSG("[FB Driver] can't get semaphore:%d\n", __LINE__);
		sem_flipping_cnt++;
		up(&sem_flipping);
        return -ERESTARTSYS;
    }
	sem_early_suspend_cnt--;
    if (primary_display_is_sleepd()) goto End;
	//DISP_SetPWM(div);
End:
	sem_flipping_cnt++;
	sem_early_suspend_cnt++;
    up(&sem_early_suspend);
	up(&sem_flipping);
    return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_pwm);

int mtkfb_get_backlight_pwm(int div, unsigned int *freq)
{
	//DISP_GetPWM(div, freq);
    return 0;
}
EXPORT_SYMBOL(mtkfb_get_backlight_pwm);

void mtkfb_waitVsync(void)
{
	if(primary_display_is_sleepd()){
		DISPMSG("[MTKFB_VSYNC]:mtkfb has suspend, return directly\n");
		msleep(20);
		return;
	}
	vsync_cnt++;
	primary_display_wait_for_vsync(NULL);
	vsync_cnt--;
	return;
}
EXPORT_SYMBOL(mtkfb_waitVsync);
/* Used for HQA test */
/*-------------------------------------------------------------
   Note: The using scenario must be
         1. switch normal mode to factory mode when LCD screen is on
         2. switch factory mode to normal mode(optional)
-------------------------------------------------------------*/
static struct fb_var_screeninfo    fbi_var_backup;
static struct fb_fix_screeninfo    fbi_fix_backup;
static BOOL                         need_restore = FALSE;
static int mtkfb_set_par(struct fb_info *fbi);

static bool first_update = false;

static bool first_enable_esd = true;

static int _convert_fb_layer_to_disp_input(struct fb_overlay_layer* src, primary_disp_input_config *dst)
{
	unsigned int layerpitch = 0;
	unsigned int layerbpp = 0;

	dst->layer = src->layer_id;

	if (!src->layer_enable)
	{
		dst->layer_en = 0;
		dst->isDirty = true;
		return 0;
	}

	switch (src->src_fmt)
	{
		case MTK_FB_FORMAT_YUV422:
			dst->fmt = eYUY2;
			layerpitch = 2;
			layerbpp = 16;
			break;

		case MTK_FB_FORMAT_RGB565:
			dst->fmt = eRGB565;
			layerpitch = 2;
			layerbpp = 16;
			break;

		case MTK_FB_FORMAT_RGB888:
			dst->fmt = eRGB888;
			layerpitch = 3;
			layerbpp = 24;
			break;
			
		case MTK_FB_FORMAT_BGR888:
			dst->fmt = eBGR888;
			layerpitch = 3;
			layerbpp = 24;
			break;
			//xuecheng, ??????
		case MTK_FB_FORMAT_ARGB8888:
			dst->fmt = eBGRA8888;
			layerpitch = 4;
			layerbpp = 32;
			break;

		case MTK_FB_FORMAT_ABGR8888:
			//dst->fmt = eABGR8888;
			dst->fmt = eRGBA8888;
			layerpitch = 4;
			layerbpp = 32;
			break;
		case MTK_FB_FORMAT_XRGB8888:
			dst->fmt = eARGB8888;
			layerpitch = 4;
			layerbpp = 32;
			break;

		case MTK_FB_FORMAT_XBGR8888:
			dst->fmt = eRGBA8888;
			layerpitch = 4;
			layerbpp = 32;
			break;

		case MTK_FB_FORMAT_UYVY:
			dst->fmt = eUYVY;
			layerpitch = 2;
			layerbpp = 16;
			break;

		default:
			DISPERR("Invalid color format: 0x%x\n", src->src_fmt);
			return -1;
	}

		dst->vaddr = (unsigned long)src->src_base_addr;
		dst->security = src->security;
// set_overlay will not use fence+ion handle
#if 0
//#if defined (MTK_FB_ION_SUPPORT)
	if (src->src_phy_addr != NULL) 
	{
		dst->addr = (unsigned int)src->src_phy_addr;
	}
	else 
	{
		dst->addr = mtkfb_query_buf_mva(src->layer_id, (unsigned int)src->next_buff_idx);
	}
#else
		dst->addr = (unsigned long)src->src_phy_addr;
#endif
	DISPDBG("_convert_fb_layer_to_disp_input, dst->addr=0x%08x\n", dst->addr);

	dst->isTdshp = src->isTdshp;
	dst->buff_idx = src->next_buff_idx;
	dst->identity = src->identity;
	dst->connected_type = src->connected_type;

	//set Alpha blending
	dst->alpha = src->alpha;
	if (MTK_FB_FORMAT_ARGB8888 == src->src_fmt ||MTK_FB_FORMAT_ABGR8888 == src->src_fmt)
	{
			dst->aen = TRUE;
	}
	else
	{
			dst->aen = FALSE;
	}

	//set src width, src height
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;
	dst->dst_w = src->tgt_width;
	dst->dst_h = src->tgt_height;
	if (dst->dst_w > dst->src_w)   dst->dst_w = dst->src_w;
	if (dst->dst_h > dst->src_h)	dst->dst_h = dst->src_h;

	dst->src_pitch = src->src_pitch*layerpitch;

	//set color key
	dst->key = src->src_color_key;
	dst->keyEn = src->src_use_color_key;

	//data transferring is triggerred in MTKFB_TRIG_OVERLAY_OUT
	dst->layer_en= src->layer_enable;
	dst->isDirty = true;
		
#if 1
	DISPDBG("_convert_fb_layer_to_disp_input():id=%u, en=%u, next_idx=%u, vaddr=0x%x, paddr=0x%x, src fmt=%u, dst fmt=%u, pitch=%u, xoff=%u, yoff=%u, w=%u, h=%u\n",
						dst->layer,
						dst->layer_en,
						dst->buff_idx,
						(unsigned int)(dst->vaddr),
						(unsigned int)(dst->addr),
						src->src_fmt,
						dst->fmt,
						dst->src_pitch,
						dst->src_x,
						dst->src_y,
						dst->src_w,
						dst->src_h);
	DISPDBG("_convert_fb_layer_to_disp_input():target xoff=%u, target yoff=%u, target w=%u, target h=%u, aen=%u\n",
						dst->dst_x,
						dst->dst_y,
						dst->dst_w,
						dst->dst_h,
						dst->aen);
#endif

}

static int _overlay_info_convert(struct fb_overlay_layer* src, OVL_CONFIG_STRUCT *dst)
{
	unsigned int layerpitch = 0;
	unsigned int layerbpp = 0;

	dst->layer = src->layer_id;

	if (!src->layer_enable)
	{
		dst->layer_en = 0;
		dst->isDirty = true;
		return 0;
	}

	switch (src->src_fmt)
	{
		case MTK_FB_FORMAT_YUV422:
			dst->fmt = eYUY2;
			layerpitch = 2;
			layerbpp = 16;
			break;

		case MTK_FB_FORMAT_RGB565:
			dst->fmt = eRGB565;
			layerpitch = 2;
			layerbpp = 16;
			break;

		case MTK_FB_FORMAT_RGB888:
			dst->fmt = eRGB888;
			layerpitch = 3;
			layerbpp = 24;
			break;
			
		case MTK_FB_FORMAT_BGR888:
			dst->fmt = eBGR888;
			layerpitch = 3;
			layerbpp = 24;
			break;
			//xuecheng, ??????
		case MTK_FB_FORMAT_ARGB8888:
			dst->fmt = eRGBA8888;
			layerpitch = 4;
			layerbpp = 32;
			break;

		case MTK_FB_FORMAT_ABGR8888:
			//dst->fmt = eABGR8888;			
			dst->fmt = eRGBA8888;
			layerpitch = 4;
			layerbpp = 32;
			break;
		case MTK_FB_FORMAT_XRGB8888:
			dst->fmt = eARGB8888;
			layerpitch = 4;
			layerbpp = 32;
			break;

		case MTK_FB_FORMAT_XBGR8888:
			dst->fmt = eRGBA8888;
			layerpitch = 4;
			layerbpp = 32;
			break;

		case MTK_FB_FORMAT_UYVY:
			dst->fmt = eUYVY;
			layerpitch = 2;
			layerbpp = 16;
			break;

		default:
			DISPERR("Invalid color format: 0x%x\n", src->src_fmt);
			return -1;
	}

    	dst->vaddr = (unsigned long)src->src_base_addr;
    	dst->security = src->security;
// set overlay will not use fence+ion handle
#if 0
//#if defined (MTK_FB_ION_SUPPORT)
	if (src->src_phy_addr != NULL) 
	{
		dst->addr = (unsigned int)src->src_phy_addr;
	}
	else 
	{
		dst->addr = mtkfb_query_buf_mva(src->layer_id, (unsigned int)src->next_buff_idx);
	}
#else
    	dst->addr = (unsigned long)src->src_phy_addr;
#endif
	DISPMSG("_overlay_info_convert, dst->addr=0x%lx\n", dst->addr);
    	dst->isTdshp = src->isTdshp;
    	dst->buff_idx = src->next_buff_idx;
	dst->identity = src->identity;
	dst->connected_type = src->connected_type;

    	//set Alpha blending
	dst->alpha = src->alpha;
	if (MTK_FB_FORMAT_ARGB8888 == src->src_fmt || MTK_FB_FORMAT_ABGR8888 == src->src_fmt) {
	    	dst->aen = TRUE;
	} else {
	    	dst->aen = FALSE;
	}

#ifdef MTK_TODO
#error
	if(!strcmp(current->comm, "display_slt"))
	{
		dst->aen = FALSE;
		isAEEEnabled = 1;
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled); // default_ui_ layer coniig to changed_ui_layer
	}
#endif

	//set src width, src height
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;
	dst->dst_w = src->tgt_width;
	dst->dst_h = src->tgt_height;
	if (dst->dst_w > dst->src_w)
		dst->dst_w = dst->src_w;
	if (dst->dst_h > dst->src_h)
		dst->dst_h = dst->src_h;

	dst->src_pitch = src->src_pitch*layerpitch;
    	//set color key
	dst->key = src->src_color_key;
	dst->keyEn = src->src_use_color_key;

    	//data transferring is triggerred in MTKFB_TRIG_OVERLAY_OUT
    	dst->layer_en= src->layer_enable;
    	dst->isDirty = true;
		
#if 1
			DISPMSG("_convert_fb_layer_to_disp_input():id=%u, en=%u, next_idx=%u, vaddr=0x%lx, paddr=0x%lx, fmt=%u, pitch=%u, xoff=%u, yoff=%u, w=%u, h=%u\n",
								dst->layer,
								dst->layer_en,
								dst->buff_idx,
								dst->addr,
								dst->vaddr,
								dst->fmt,
								dst->src_pitch,
								dst->src_x,
								dst->src_y,
								dst->src_w,
								dst->src_h);
			DISPMSG("_convert_fb_layer_to_disp_input():target xoff=%u, target yoff=%u, target w=%u, target h=%u, aen=%u\n",
								dst->dst_x,
								dst->dst_y,
								dst->dst_w,
								dst->dst_h,
								dst->aen);
#endif

	return 0;
}
static int mtkfb_pan_display_impl(struct fb_var_screeninfo *var, struct fb_info *info)
{
	UINT32 offset = 0;
	UINT32 paStart = 0;
	char *vaStart = NULL, *vaEnd = NULL;
	int ret = 0;
	int wait_ret = 0;
	unsigned int layerpitch = 0;
	unsigned int src_pitch = 0;

	//DISPFUNC();

	if (first_update) {
		DISPMSG("the first time of mtkfb_pan_display_impl will be ignored\n");
		first_update = false;
		return ret;
	}

	DISPDBG("xoffset=%u, yoffset=%u, xres=%u, yres=%u, xresv=%u, yresv=%u\n", var->xoffset,
		var->yoffset, info->var.xres, info->var.yres, info->var.xres_virtual,
		info->var.yres_virtual);

	info->var.yoffset = var->yoffset;
	offset = var->yoffset * info->fix.line_length;
	paStart = fb_pa + offset;
	vaStart = info->screen_base + offset;
	vaEnd   = vaStart + info->var.yres * info->fix.line_length;
	
	primary_disp_input_config input;
	memset((void*)&input, 0, sizeof(primary_disp_input_config));

	input.addr = (unsigned long)paStart;
	input.vaddr = (unsigned long)vaStart;
	input.layer = primary_display_get_option("FB_LAYER");
	input.layer_en = 1;
	input.src_x = 0;
	input.src_y = 0;
	input.src_w = var->xres;
	input.src_h = var->yres;
	input.dst_x = 0;
	input.dst_y = 0;
	input.dst_w = var->xres;
	input.dst_h = var->yres;

	switch (var->bits_per_pixel) {
		case 16:
			input.fmt = eRGB565;
			layerpitch = 2;
			input.aen = FALSE;
			break;
		case 24:
			input.fmt = eRGB888;
			layerpitch = 3;
			input.aen = FALSE;
			break;
		case 32:
			// this is very useful for color format related debug
			#if 0
			{
				struct mtkfb_device *fbdev = (struct mtkfb_device *)info->par;
				unsigned int va = fbdev->fb_va_base+offset;
				DISPMSG("fb dump: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", *(unsigned int*)va, *(unsigned int*)(va+4), *(unsigned int*)(va+8), *(unsigned int*)(va+0xC));

			}
			DISPMSG("pan display, var->blue.offset=%d\n", var->blue.offset);
			#endif
			// home screen use eRGBA8888
			input.fmt = (0 == var->blue.offset) ?
                           eBGRA8888 :
                           eRGBA8888;
			layerpitch = 4;
			input.aen = FALSE;
			break;
		default:
			DISPERR("Invalid color format bpp: 0x%d\n", var->bits_per_pixel);
			return -1;
        }

		src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
        input.alpha = 0xFF;

        input.buff_idx = -1;
        input.src_pitch = src_pitch * layerpitch;
		input.isDirty = 1;

	ret = primary_display_config_input(&input);
	ret = primary_display_trigger(TRUE, NULL, 0);
	/* primary_display_diagnose(); */

    	return ret;
}


/* Set fb_info.fix fields and also updates fbdev.
 * When calling this fb_info.var must be set up already.
 */
static void set_fb_fix(struct mtkfb_device *fbdev)
{
    struct fb_info           *fbi   = fbdev->fb_info;
    struct fb_fix_screeninfo *fix   = &fbi->fix;
    struct fb_var_screeninfo *var   = &fbi->var;
    struct fb_ops            *fbops = fbi->fbops;

    strncpy(fix->id, MTKFB_DRIVER, sizeof(fix->id));
    fix->type = FB_TYPE_PACKED_PIXELS;

    switch (var->bits_per_pixel)
    {
    case 16:
    case 24:
    case 32:
        fix->visual = FB_VISUAL_TRUECOLOR;
        break;
    case 1:
    case 2:
    case 4:
    case 8:
        fix->visual = FB_VISUAL_PSEUDOCOLOR;
        break;
    default:
        ASSERT(0);
    }

    fix->accel       = FB_ACCEL_NONE;
    fix->line_length = ALIGN_TO(var->xres_virtual, MTK_FB_ALIGNMENT) * var->bits_per_pixel / 8;
    fix->smem_len    = fbdev->fb_size_in_byte;
    fix->smem_start  = fbdev->fb_pa_base;

    fix->xpanstep = 0;
    fix->ypanstep = 1;

    fbops->fb_fillrect  = cfb_fillrect;
    fbops->fb_copyarea  = cfb_copyarea;
    fbops->fb_imageblit = cfb_imageblit;
}


/* Check values in var, try to adjust them in case of out of bound values if
 * possible, or return error.
 */
static int mtkfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fbi)
{
    unsigned int bpp;
    unsigned long max_frame_size;
    unsigned long line_size;

    struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;

    //DISPFUNC();

    DISPDBG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, "
              "xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
        var->xres, var->yres, var->xres_virtual, var->yres_virtual,
        var->xoffset, var->yoffset, var->bits_per_pixel);

    bpp = var->bits_per_pixel;

    if (bpp != 16 && bpp != 24 && bpp != 32) {
        MTKFB_LOG("[%s]unsupported bpp: %d", __func__, bpp);
        return -1;
    }

    switch (var->rotate) {
    case 0:
    case 180:
        var->xres = MTK_FB_XRES;
        var->yres = MTK_FB_YRES;
        break;
    case 90:
    case 270:
        var->xres = MTK_FB_YRES;
        var->yres = MTK_FB_XRES;
        break;
    default:
        return -1;
    }

    if (var->xres_virtual < var->xres)
        var->xres_virtual = var->xres;
    if (var->yres_virtual < var->yres)
        var->yres_virtual = var->yres;

    max_frame_size = fbdev->fb_size_in_byte;
	DISPDBG("fbdev->fb_size_in_byte=0x%08lx\n", fbdev->fb_size_in_byte);
    line_size = var->xres_virtual * bpp / 8;

    if (line_size * var->yres_virtual > max_frame_size) {
        /* Try to keep yres_virtual first */
        line_size = max_frame_size / var->yres_virtual;
        var->xres_virtual = line_size * 8 / bpp;
        if (var->xres_virtual < var->xres) {
            /* Still doesn't fit. Shrink yres_virtual too */
            var->xres_virtual = var->xres;
            line_size = var->xres * bpp / 8;
            var->yres_virtual = max_frame_size / line_size;
        }
    }
	    DISPDBG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, "
              "xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
        var->xres, var->yres, var->xres_virtual, var->yres_virtual,
        var->xoffset, var->yoffset, var->bits_per_pixel);
    if (var->xres + var->xoffset > var->xres_virtual)
        var->xoffset = var->xres_virtual - var->xres;
    if (var->yres + var->yoffset > var->yres_virtual)
        var->yoffset = var->yres_virtual - var->yres;
	
    DISPDBG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, "
              "xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
        var->xres, var->yres, var->xres_virtual, var->yres_virtual,
        var->xoffset, var->yoffset, var->bits_per_pixel);

    if (16 == bpp) {
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
	} else if (24 == bpp) {
        var->red.length = var->green.length = var->blue.length = 8;
        var->transp.length = 0;

        // Check if format is RGB565 or BGR565

        ASSERT(8 == var->green.offset);
        ASSERT(16 == var->red.offset + var->blue.offset);
        ASSERT(16 == var->red.offset || 0 == var->red.offset);
	} else if (32 == bpp) {
		var->red.length = var->green.length = var->blue.length = var->transp.length = 8;

        // Check if format is ARGB565 or ABGR565

        ASSERT(8 == var->green.offset && 24 == var->transp.offset);
        ASSERT(16 == var->red.offset + var->blue.offset);
        ASSERT(16 == var->red.offset || 0 == var->red.offset);
    }

	var->red.msb_right = var->green.msb_right = var->blue.msb_right = var->transp.msb_right = 0;

    var->activate = FB_ACTIVATE_NOW;

    var->height    = UINT_MAX;
    var->width     = UINT_MAX;
    var->grayscale = 0;
    var->nonstd    = 0;

    var->pixclock     = UINT_MAX;
    var->left_margin  = UINT_MAX;
    var->right_margin = UINT_MAX;
    var->upper_margin = UINT_MAX;
    var->lower_margin = UINT_MAX;
    var->hsync_len    = UINT_MAX;
    var->vsync_len    = UINT_MAX;

    var->vmode = FB_VMODE_NONINTERLACED;
    var->sync  = 0;

    MSG_FUNC_LEAVE();
    return 0;
}

unsigned int FB_LAYER = 2;
#define ASSERT_LAYER    (DDP_OVL_LAYER_MUN-1)
#define DISP_DEFAULT_UI_LAYER_ID (DDP_OVL_LAYER_MUN-1)
#define DISP_CHANGED_UI_LAYER_ID (DDP_OVL_LAYER_MUN-2)

/* Switch to a new mode. The parameters for it has been check already by
 * mtkfb_check_var.
 */
static int mtkfb_set_par(struct fb_info *fbi)
{
    struct fb_var_screeninfo *var = &fbi->var;
    struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;
    struct fb_overlay_layer fb_layer;
    u32 bpp = var->bits_per_pixel;

   // DISPFUNC();
    memset(&fb_layer, 0, sizeof(struct fb_overlay_layer));
    switch(bpp)
    {
    case 16 :
        fb_layer.src_fmt = MTK_FB_FORMAT_RGB565;
        fb_layer.src_use_color_key = 1;
        fb_layer.src_color_key = 0xFF000000;
        break;

    case 24 :
        fb_layer.src_use_color_key = 1;
        fb_layer.src_fmt = (0 == var->blue.offset) ?
                           MTK_FB_FORMAT_RGB888 :
                           MTK_FB_FORMAT_BGR888;
        fb_layer.src_color_key = 0xFF000000;
        break;

    case 32 :
        fb_layer.src_use_color_key = 0;
		DISPDBG("set_par,var->blue.offset=%d\n", var->blue.offset);
        fb_layer.src_fmt = (0 == var->blue.offset) ?
                           MTK_FB_FORMAT_ARGB8888 :
                           MTK_FB_FORMAT_ABGR8888;
        fb_layer.src_color_key = 0;
        break;

    default :
        fb_layer.src_fmt = MTK_FB_FORMAT_UNKNOWN;
        DISPERR("[%s]unsupported bpp: %d", __func__, bpp);
        return -1;
    }

    // If the framebuffer format is NOT changed, nothing to do
    //
    //if (fb_layer.src_fmt == fbdev->layer_format[primary_display_get_option("FB_LAYER")]) {
    //    goto Done;
    //}

    // else, begin change display mode
    //
    set_fb_fix(fbdev);

    fb_layer.layer_id = primary_display_get_option("FB_LAYER");
    fb_layer.layer_enable = 1;
    fb_layer.src_base_addr = (void *)((unsigned long)fbdev->fb_va_base + var->yoffset * fbi->fix.line_length);
	DISPDBG("fb_pa=0x%08lx, var->yoffset=0x%08x,fbi->fix.line_length=0x%08x\n", fb_pa, var->yoffset, fbi->fix.line_length);
    fb_layer.src_phy_addr = (void *)(fb_pa + var->yoffset * fbi->fix.line_length);
    fb_layer.src_direct_link = 0;
    fb_layer.src_offset_x = fb_layer.src_offset_y = 0;
//    fb_layer.src_width = fb_layer.tgt_width = fb_layer.src_pitch = var->xres;
	//xuecheng, does HWGPU_SUPPORT still in use now?
#if defined(HWGPU_SUPPORT)
    fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
#else
    if(get_boot_mode() == META_BOOT || get_boot_mode() == FACTORY_BOOT
       || get_boot_mode() == ADVMETA_BOOT || get_boot_mode() == RECOVERY_BOOT)
        fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
    else
		fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
#endif
    fb_layer.src_width = fb_layer.tgt_width = var->xres;
    fb_layer.src_height = fb_layer.tgt_height = var->yres;
    fb_layer.tgt_offset_x = fb_layer.tgt_offset_y = 0;
	fb_layer.alpha = 0xff;
	/* fb_layer.src_color_key = 0; */
	fb_layer.layer_rotation = MTK_FB_ORIENTATION_0;
	fb_layer.layer_type = LAYER_2D;
	DISPDBG("mtkfb_set_par, fb_layer.src_fmt=%x\n", fb_layer.src_fmt);

	primary_disp_input_config temp;

	if(!isAEEEnabled)
	{
		DISPDBG("AEE is not enabled, will disable layer 3\n");
		memset((void*)&temp, 0, sizeof(primary_disp_input_config));
		temp.layer = primary_display_get_option("ASSERT_LAYER");
		temp.layer_en = 0;
		temp.isDirty = 1;
		primary_display_config_input(&temp);
	} else {
		DISPCHECK("AEE is enabled, should not disable layer 3\n");
	}
	
	memset((void*)&temp, 0, sizeof(primary_disp_input_config));
	_convert_fb_layer_to_disp_input(&fb_layer,&temp);
	primary_display_config_input(&temp);
    // backup fb_layer information.
    memcpy(&fb_layer_context, &fb_layer, sizeof(fb_layer));

Done:
    MSG_FUNC_LEAVE();
    return 0;
}


static int mtkfb_soft_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
    NOT_REFERENCED(info);
    NOT_REFERENCED(cursor);

    return 0;
}

static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info* layerInfo)
{
#if 0
    DISP_LAYER_INFO layer;
	if (layerInfo->layer_id >= DDP_OVL_LAYER_MUN) {
         return 0;
    }
    layer.id = layerInfo->layer_id;
 //   DISP_GetLayerInfo(&layer);
 	int id = layerInfo->layer_id;
     layer.curr_en = captured_layer_config[id].layer_en;
    layer.next_en = cached_layer_config[id].layer_en;
    layer.hw_en = realtime_layer_config[id].layer_en;
    layer.curr_idx = captured_layer_config[id].buff_idx;
    layer.next_idx = cached_layer_config[id].buff_idx;
    layer.hw_idx = realtime_layer_config[id].buff_idx;
    layer.curr_identity = captured_layer_config[id].identity;
    layer.next_identity = cached_layer_config[id].identity;
    layer.hw_identity = realtime_layer_config[id].identity;
    layer.curr_conn_type = captured_layer_config[id].connected_type;
    layer.next_conn_type = cached_layer_config[id].connected_type;
    layer.hw_conn_type = realtime_layer_config[id].connected_type;
    layerInfo->layer_enabled = layer.hw_en;
    layerInfo->curr_en = layer.curr_en;
    layerInfo->next_en = layer.next_en;
    layerInfo->hw_en = layer.hw_en;
    layerInfo->curr_idx = layer.curr_idx;
    layerInfo->next_idx = layer.next_idx;
    layerInfo->hw_idx = layer.hw_idx;
    layerInfo->curr_identity = layer.curr_identity;
    layerInfo->next_identity = layer.next_identity;
    layerInfo->hw_identity = layer.hw_identity;
    layerInfo->curr_conn_type = layer.curr_conn_type;
    layerInfo->next_conn_type = layer.next_conn_type;
    layerInfo->hw_conn_type = layer.hw_conn_type;
#if 0
    MTKFB_LOG("[FB Driver] mtkfb_get_overlay_layer_info():id=%u, layer en=%u, next_en=%u, curr_en=%u, hw_en=%u, next_idx=%u, curr_idx=%u, hw_idx=%u \n",
    		layerInfo->layer_id,
    		layerInfo->layer_enabled,
    		layerInfo->next_en,
    		layerInfo->curr_en,
    		layerInfo->hw_en,
    		layerInfo->next_idx,
    		layerInfo->curr_idx,
    		layerInfo->hw_idx);
#endif
#endif
    return 0;
}	


#include <linux/aee.h>
#define mtkfb_aee_print(string, args...) do{\
    aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_MMPROFILE_BUFFER, "sf-mtkfb blocked", string, ##args);  \
}while(0)

void mtkfb_dump_layer_info(void)
{
#if 0
	unsigned int i;
	DISPMSG("[mtkfb] start dump layer info, early_suspend=%d \n", primary_display_is_sleepd());
	DISPMSG("[mtkfb] cache(next): \n");
	for (i = 0; i < 4; i++) {
		DISPMSG("[mtkfb] layer=%d, layer_en=%d, idx=%d, fmt=%d, addr=0x%x, %d, %d, %d\n ", cached_layer_config[i].layer,	/* layer */
		       cached_layer_config[i].layer_en, cached_layer_config[i].buff_idx, cached_layer_config[i].fmt, cached_layer_config[i].addr,	/* addr */
	    cached_layer_config[i].identity,  
		       cached_layer_config[i].connected_type, cached_layer_config[i].security);
	}
  
  DISPMSG("[mtkfb] captured(current): \n");
	for (i = 0; i < 4; i++) {
		DISPMSG("[mtkfb] layer=%d, layer_en=%d, idx=%d, fmt=%d, addr=0x%x, %d, %d, %d\n ", captured_layer_config[i].layer,	/* layer */
		       captured_layer_config[i].layer_en, captured_layer_config[i].buff_idx, captured_layer_config[i].fmt, captured_layer_config[i].addr,	/* addr */
	    captured_layer_config[i].identity,  
		       captured_layer_config[i].connected_type, captured_layer_config[i].security);
	}
  DISPMSG("[mtkfb] realtime(hw): \n");
	for (i = 0; i < 4; i++) {
		DISPMSG("[mtkfb] layer=%d, layer_en=%d, idx=%d, fmt=%d, addr=0x%x, %d, %d, %d\n ", realtime_layer_config[i].layer,	/* layer */
		       realtime_layer_config[i].layer_en, realtime_layer_config[i].buff_idx, realtime_layer_config[i].fmt, realtime_layer_config[i].addr,	/* addr */
	    realtime_layer_config[i].identity,  
		       realtime_layer_config[i].connected_type, realtime_layer_config[i].security);
	}
	    
	// dump mmp data
	//mtkfb_aee_print("surfaceflinger-mtkfb blocked");
#endif 
}


static int mtkfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    DISP_STATUS ret = 0;
    int r = 0;

	DISPFUNC();
	/// M: dump debug mmprofile log info
	MMProfileLogEx(MTKFB_MMP_Events.IOCtrl, MMProfileFlagPulse, _IOC_NR(cmd), arg);
	DISPMSG("mtkfb_ioctl, info=%p, cmd=0x%08x, arg=0x%lx\n", info, (unsigned int)cmd, arg);

    switch (cmd)
    {    	

	case MTKFB_GET_FRAMEBUFFER_MVA:
       	return copy_to_user(argp, &fb_pa,  sizeof(fb_pa)) ? -EFAULT : 0;
	// remain this for engineer mode dfo multiple resolution
	#if 1
	case MTKFB_GET_DISPLAY_IF_INFORMATION:
	{
		int displayid = 0;
		if (copy_from_user(&displayid, (void __user *)arg, sizeof(displayid))) 
		{
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
			return -EFAULT;
		}

		if (displayid > MTKFB_MAX_DISPLAY_COUNT) 
		{
			DISPERR("[FB]: invalid display id:%d \n", displayid);
			return -EFAULT;
		}

		if(displayid == 0)
		{	
			dispif_info[displayid].displayWidth =primary_display_get_width();
			dispif_info[displayid].displayHeight = primary_display_get_height();
			
			dispif_info[displayid].lcmOriginalWidth = primary_display_get_original_width();
			dispif_info[displayid].lcmOriginalHeight = primary_display_get_original_height();			
			dispif_info[displayid].displayMode = primary_display_is_video_mode()?0:1;
		}
		else
		{
			DISPERR("information for displayid: %d is not available now\n", displayid);
		}
		
		if (copy_to_user((void __user *)arg, &(dispif_info[displayid]),  sizeof(mtk_dispif_info_t))) 
		{
			MTKFB_LOG("[FB]: copy_to_user failed! line:%d \n", __LINE__);
			r = -EFAULT;
		}
		
		return (r);
	}
	#endif
	case MTKFB_POWEROFF:
   	{
		MTKFB_FUNC();
			if (primary_display_is_sleepd()) {
			DISPMSG("[FB Driver] is still in MTKFB_POWEROFF!!!\n");
			return r;
		}

		DISPMSG("[FB Driver] enter MTKFB_POWEROFF\n");
			/* FIXME: temp for AOSP */
			/* cci400_sel_for_ddp(); */
		ret = primary_display_suspend();
			if (ret < 0) {
			DISPERR("primary display suspend failed\n");
		}
		DISPMSG("[FB Driver] leave MTKFB_POWEROFF\n");

			is_early_suspended = TRUE;	/* no care */
		return r;
	}

	case MTKFB_POWERON:
   	{
		MTKFB_FUNC();
			if (primary_display_is_alive()) {
			DISPMSG("[FB Driver] is still in MTKFB_POWERON!!!\n");
			return r;
		}
		DISPMSG("[FB Driver] enter MTKFB_POWERON\n");
		primary_display_resume();
		DISPMSG("[FB Driver] leave MTKFB_POWERON\n");
			is_early_suspended = FALSE;	/* no care */
		return r;
	}
    case MTKFB_GET_POWERSTATE:
    {
        int power_state;

        if(primary_display_is_sleepd())
            power_state = 0;
        else
            power_state = 1;
            
        if (copy_to_user(argp, &power_state,  sizeof(power_state))){
            printk("MTKFB_GET_POWERSTATE failed\n");
        	return -EFAULT;
        }

        return  0;
    }

    case MTKFB_CONFIG_IMMEDIATE_UPDATE:
    {
        MTKFB_LOG("[%s] MTKFB_CONFIG_IMMEDIATE_UPDATE, enable = %lu\n",  __func__, arg);
		if (down_interruptible(&sem_early_suspend)) {
        		MTKFB_LOG("[mtkfb_ioctl] can't get semaphore:%d\n", __LINE__);
        		return -ERESTARTSYS;
    	}
		sem_early_suspend_cnt--;
        //DISP_WaitForLCDNotBusy();
        //ret = DISP_ConfigImmediateUpdate((BOOL)arg);
	//	sem_early_suspend_cnt++;
		up(&sem_early_suspend);
        return (r);
    }

    case MTKFB_CAPTURE_FRAMEBUFFER:
    {
        unsigned int pbuf = 0;
        if (copy_from_user(&pbuf, (void __user *)arg, sizeof(pbuf)))
        {
            MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
            r = -EFAULT;
        }
        else
        {
            dprec_logger_start(DPREC_LOGGER_WDMA_DUMP, 0, 0);
            primary_display_capture_framebuffer_ovl(pbuf, eBGRA8888);
            dprec_logger_done(DPREC_LOGGER_WDMA_DUMP, 0, 0);
        }

        return (r);
    }

    case MTKFB_SLT_AUTO_CAPTURE:
    {
        struct fb_slt_catpure capConfig;
        if (copy_from_user(&capConfig, (void __user *)arg, sizeof(capConfig)))
        {
            MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
            r = -EFAULT;
        }
        else
        {
            unsigned int format;
            switch (capConfig.format)
            {
            case MTK_FB_FORMAT_RGB888:
                format = eRGB888;
                break;
            case MTK_FB_FORMAT_BGR888:
                format = eBGR888;
                break;
            case MTK_FB_FORMAT_ARGB8888:
                format = eARGB8888;
                break;
            case MTK_FB_FORMAT_RGB565:
                format = eRGB565;
                break;
            case MTK_FB_FORMAT_UYVY:
                format = eYUV_420_2P_UYVY;
                break;
            case MTK_FB_FORMAT_ABGR8888:
            default:
                format = eABGR8888;
                break;
            }
				primary_display_capture_framebuffer_ovl((unsigned long)capConfig.outputBuffer, format);
        }

        return (r);
    }

#ifdef MTK_FB_OVERLAY_SUPPORT
        case MTKFB_GET_OVERLAY_LAYER_INFO:
    {
        struct fb_overlay_layer_info layerInfo;
        MTKFB_LOG(" mtkfb_ioctl():MTKFB_GET_OVERLAY_LAYER_INFO\n");

        if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
            MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
            return -EFAULT;
        }
        if (mtkfb_get_overlay_layer_info(&layerInfo) < 0)
        {
            MTKFB_LOG("[FB]: Failed to get overlay layer info\n");
            return -EFAULT;
        }
        if (copy_to_user((void __user *)arg, &layerInfo, sizeof(layerInfo))) {
            MTKFB_LOG("[FB]: copy_to_user failed! line:%d \n", __LINE__);
            r = -EFAULT;
        }
        return (r);
    }
    case MTKFB_SET_OVERLAY_LAYER:
    {
		struct fb_overlay_layer layerInfo;
		
		if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) 
		{
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
			r = -EFAULT;
			} else {
				/* in early suspend mode ,will not update buffer index, info SF by return value */
				if (primary_display_is_sleepd()) {
					DISPMSG
					    ("[FB] error, set overlay in early suspend ,skip!\n");
		    		return MTKFB_ERROR_IS_EARLY_SUSPEND;
			}
			
			primary_disp_input_config input;
			memset((void*)&input, 0, sizeof(primary_disp_input_config));
			_convert_fb_layer_to_disp_input(&layerInfo,&input);
			primary_display_config_input(&input);
			primary_display_trigger(1, NULL, 0);
		}
		
		return (r);
    }

    case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT:
    {
        DISPMSG("[DDP] mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT  \n");
			/* call info dump function here */
			/* mtkfb_dump_layer_info(); */
        return (r);
    }

    case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT_AEE:
    {
        DISPMSG("[DDP] mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT  \n");
			/* call info dump function here */
			/* mtkfb_dump_layer_info(); */
			/* mtkfb_aee_print("surfaceflinger-mtkfb blocked"); */
        return (r);
    }
        
    case MTKFB_SET_VIDEO_LAYERS:
    {
        struct mmp_fb_overlay_layers
        {
		struct fb_overlay_layer Layer0;
		struct fb_overlay_layer Layer1;
		struct fb_overlay_layer Layer2;
		struct fb_overlay_layer Layer3;
        };

        struct fb_overlay_layer layerInfo[VIDEO_LAYER_COUNT];
        MTKFB_LOG(" mtkfb_ioctl():MTKFB_SET_VIDEO_LAYERS\n");
        MMProfileLog(MTKFB_MMP_Events.SetOverlayLayers, MMProfileFlagStart);

        if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) 
	{
		MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
		MMProfileLogMetaString(MTKFB_MMP_Events.SetOverlayLayers, MMProfileFlagEnd, "Copy_from_user failed!");
		r = -EFAULT;
        }
	else 
	{
		int32_t i;
		primary_disp_input_config input;
		//mutex_lock(&OverlaySettingMutex);
		for (i = 0; i < VIDEO_LAYER_COUNT; ++i) 
		{
			memset((void*)&input, 0, sizeof(primary_disp_input_config));
			_convert_fb_layer_to_disp_input(&layerInfo[i],& input);
			primary_display_config_input(&input);
		}
		//is_ipoh_bootup = false;
		//atomic_set(&OverlaySettingDirtyFlag, 1);
		//atomic_set(&OverlaySettingApplied, 0);
		//mutex_unlock(&OverlaySettingMutex);
		//MMProfileLogStructure(MTKFB_MMP_Events.SetOverlayLayers, MMProfileFlagEnd, layerInfo, struct mmp_fb_overlay_layers);
		
		primary_display_trigger(1, NULL, 0);
        }

        return (r);
    }

    	case MTKFB_TRIG_OVERLAY_OUT:
	{
		MTKFB_LOG(" mtkfb_ioctl():MTKFB_TRIG_OVERLAY_OUT\n");
		MMProfileLog(MTKFB_MMP_Events.TrigOverlayOut, MMProfileFlagPulse);
		primary_display_trigger(1, NULL, 0);
		//return mtkfb_update_screen(info);
		return 0;
	}

#endif // MTK_FB_OVERLAY_SUPPORT

    case MTKFB_META_RESTORE_SCREEN:
    {
        struct fb_var_screeninfo var;

		if (copy_from_user(&var, argp, sizeof(var)))
			return -EFAULT;

        info->var.yoffset = var.yoffset;
        init_framebuffer(info);

        return mtkfb_pan_display_impl(&var, info);
    }

	
    case MTKFB_GET_DEFAULT_UPDATESPEED:
	{
	    unsigned int speed;
		MTKFB_LOG("[MTKFB] get default update speed\n");
		//DISP_Get_Default_UpdateSpeed(&speed);

        DISPMSG("[MTKFB EM]MTKFB_GET_DEFAULT_UPDATESPEED is %d\n", speed);
			return copy_to_user(argp, &speed, sizeof(speed)) ? -EFAULT : 0;
    }

    case MTKFB_GET_CURR_UPDATESPEED:
	{
	    unsigned int speed;
		MTKFB_LOG("[MTKFB] get current update speed\n");
		//DISP_Get_Current_UpdateSpeed(&speed);

        DISPMSG("[MTKFB EM]MTKFB_GET_CURR_UPDATESPEED is %d\n", speed);
			return copy_to_user(argp, &speed, sizeof(speed)) ? -EFAULT : 0;
	}

	case MTKFB_CHANGE_UPDATESPEED:
	{
	    unsigned int speed;
		MTKFB_LOG("[MTKFB] change update speed\n");

		if (copy_from_user(&speed, (void __user *)arg, sizeof(speed))) {
            MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
            r = -EFAULT;
        } else {
			//DISP_Change_Update(speed);

            DISPMSG("[MTKFB EM]MTKFB_CHANGE_UPDATESPEED is %d\n", speed);

        }
        return (r);
	}

    case MTKFB_AEE_LAYER_EXIST:
    {
			/* DISPMSG("[MTKFB] isAEEEnabled=%d\n", isAEEEnabled); */
		return copy_to_user(argp, &isAEEEnabled,
                            sizeof(isAEEEnabled)) ? -EFAULT : 0;
    }
    case MTKFB_LOCK_FRONT_BUFFER:
        return 0;
    case MTKFB_UNLOCK_FRONT_BUFFER:
        return 0;
		
	case MTKFB_FACTORY_AUTO_TEST:
	{
		unsigned int result = 0;
		DISPMSG("factory mode: lcm auto test\n");
        result = mtkfb_fm_auto_test();
		return copy_to_user(argp, &result, sizeof(result)) ? -EFAULT : 0;
	}
	case MTKFB_META_SHOW_BOOTLOGO:
	{
      DISPMSG("MTKFB_META_SHOW_BOOTLOGO\n");
      struct mtkfb_device  *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
      /* text layer*/
      primary_disp_input_config input;
      memset((void*)&input, 0, sizeof(primary_disp_input_config));
      input.layer = 3;
      input.layer_en = 1;
      input.buffer_source = 0;
      input.fmt  = eRGBA8888;
      input.addr = fbdev->fb_pa_base + (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT)* ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT)*4);
      input.src_x = 0;
      input.src_y = 0;
      input.src_w = MTK_FB_XRES;
      input.src_h = MTK_FB_YRES;
      input.src_pitch = MTK_FB_XRES*4;
      input.dst_x = 0;
      input.dst_y = 0;
      input.dst_w = MTK_FB_XRES;
      input.dst_h = MTK_FB_YRES;
      input.keyEn = 0;
      input.aen = 1;
      input.alpha = 0xff;
      input.sur_aen = 0;
      input.isDirty = 1;
      primary_display_config_input(&input);
      
      /* fb layer*/
      input.layer = 0;
      input.layer_en = 1;
      input.buffer_source = 0;
      input.fmt = eRGBA8888;
      input.addr = fbdev->fb_pa_base;
      input.src_x = 0;
      input.src_y = 0;
      input.src_w = MTK_FB_XRES;
      input.src_h = MTK_FB_YRES;
      input.src_pitch = ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT)*4;
      input.dst_x = 0;
      input.dst_y = 0;
      input.dst_w = MTK_FB_XRES;
      input.dst_h = MTK_FB_YRES;
      input.keyEn = 0;
      input.aen = 1;
      input.alpha = 0xff;
      input.sur_aen = 0;
      input.isDirty = 1;
      primary_display_config_input(&input);

      primary_display_trigger(TRUE, NULL, 0);
      return 0;
  }			
//#if defined (MTK_FB_SYNC_SUPPORT)
#if 0
			case MTKFB_PREPARE_OVERLAY_BUFFER:
			{
				struct fb_overlay_buffer overlay_buffer;
				struct mtkfb_fence_buf_info *buf;
				if (copy_from_user(&overlay_buffer, (void __user *)arg, sizeof(overlay_buffer))) 
				{
					printk("[FB Driver]: copy_from_user failed! line:%d \n", __LINE__);
					r = -EFAULT;
				}
				else 
				{				
					if (overlay_buffer.layer_en) 
					{
						buf = mtkfb_prepare_buf((struct fb_overlay_buffer_t*)&overlay_buffer);
						if (buf != NULL) 
						{
							overlay_buffer.fence_fd = buf->fence;
							overlay_buffer.index = buf->idx;
						}
						else 
						{
							overlay_buffer.fence_fd = MTK_FB_INVALID_FENCE_FD; // invalid fd
							overlay_buffer.index = 0;
						}
					} 
					else 
					{
						overlay_buffer.fence_fd = MTK_FB_INVALID_FENCE_FD;	  // invalid fd
						overlay_buffer.index = 0;
					}
					if (copy_to_user((void __user *)arg, &overlay_buffer, sizeof(overlay_buffer)))
					{
						printk("[FB Driver]: copy_to_user failed! line:%d \n", __LINE__);
						return -EFAULT;
					}
				}
				return 0;
			}
			case MTKFB_SET_MULTIPLE_LAYERS:
			{
				struct mmp_fb_overlay_layers
				{
					struct fb_overlay_layer Layer0;
					struct fb_overlay_layer Layer1;
					struct fb_overlay_layer Layer2;
					struct fb_overlay_layer Layer3;
				};
				struct fb_overlay_layer layerInfo[HW_OVERLAY_COUNT];
				MTKFB_LOG(" mtkfb_ioctl():MTKFB_SET_MULTIPLE_LAYERS\n");
		//		MMProfileLog(MTKFB_MMP_Events.SetMultipleLayers, MMProfileFlagStart);
				if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) 
				{
					MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
					MMProfileLogMetaString(MTKFB_MMP_Events.SetMultipleLayers, MMProfileFlagEnd, "Copy_from_user failed!");
					r = -EFAULT;
				}
				else 
				{
					int32_t i;

					primary_disp_input_config input;
					memset((void*)&input, 0, sizeof(primary_disp_input_config));
					
					for (i = 0; i < HW_OVERLAY_COUNT; ++i) 
					{
						DISPMSG("mslayer(0x%08x) bidx (%d) \n",(layerInfo[i].layer_id <<24 | layerInfo[i].layer_enable),layerInfo[i].next_buff_idx);
						if (layerInfo[i].layer_id >= HW_OVERLAY_COUNT) 
						{
							continue;
						}
						#if 0
						if (layerInfo[i].layer_enable && (layerInfo[i].next_buff_idx == cached_layer_config[i].buff_idx)) 
						{
							MTKFB_ERR("layerId(%d) HWC reset the same buffer(%d)!\n", layerInfo[i].layer_id, layerInfo[i].next_buff_idx);
							//aee_kernel_warning("MTKFB","layerId(%d) HWC reset the same buffer(%d)!\n", layerInfo[i].layer_id, layerInfo[i].next_buff_idx);
							continue;
						}
						#endif
						if (layerInfo[i].layer_enable && (layerInfo[i].next_buff_idx <= mtkfb_fence_timeline_index(i)))
						{
							DISPERR("layerId(%d) HWC set the old buffer(%d),timeline_idx(%d)!\n", layerInfo[i].layer_id, layerInfo[i].next_buff_idx,mtkfb_fence_timeline_index(i));
							//aee_kernel_warning("MTKFB","layerId(%d) HWC set the old buffer(%d)!\n", layerInfo[i].layer_id, layerInfo[i].next_buff_idx);
							continue;
						}
						
						if(is_early_suspended)
						{
							DISPERR("in early suspend layer(0x%x),idx(%d),time_idx(%d)!\n", layerInfo[i].layer_id<<16|layerInfo[i].layer_enable, layerInfo[i].next_buff_idx,mtkfb_fence_timeline_index(i));
							//mtkfb_release_layer_fence(layerInfo[i].layer_id);
						}
						#if 1
						if(!(isAEEEnabled && i== DISP_DEFAULT_UI_LAYER_ID))
						{
							_overlay_info_convert(&layerInfo[i],&cached_layer_config[i]);
						}
						#endif
						
						_convert_fb_layer_to_disp_input(&layerInfo[i],&input);
						primary_display_config_input(&input);
					}
					
					primary_display_trigger(0);		
					//disp_primary_path_set_overlay_layer(cached_layer_config, HW_OVERLAY_COUNT);
					for(i=0; i< HW_OVERLAY_COUNT; i++)
					{
						if(!(isAEEEnabled && i== DISP_DEFAULT_UI_LAYER_ID))
						{
							if(layerInfo[i].layer_enable)
							{
								 mtkfb_release_fence(i, cached_layer_config[i].buff_idx-1);
							}
							else
							{
								 mtkfb_release_fence(i, cached_layer_config[i].buff_idx);
							}
						}
						else
						{
							mtkfb_release_layer_fence(i);
						}
					}
				}
				return (r);
			}
#endif 
    default:
        printk("mtkfb_ioctl Not support, info=0x%p, cmd=0x%08x, arg=0x%08lx\n", info, (unsigned int)cmd, arg);
        return -EINVAL;
    }
}

#ifdef CONFIG_COMPAT
//COMPAT
#define COMPAT_MTKFB_GET_POWERSTATE		       MTK_IOR(21, compat_ulong_t)	//0: power off  1: power on 
#define COMPAT_MTKFB_CAPTURE_FRAMEBUFFER       MTK_IOW(3, compat_ulong_t)

static int mtkfb_compat_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    DISP_STATUS ret = 0;
    int r = 0;

	DISPFUNC();
	MMProfileLogEx(MTKFB_MMP_Events.IOCtrl, MMProfileFlagPulse, _IOC_NR(cmd), arg);
	printk("mtkfb_compat_ioctl, info=%p, cmd nr=0x%08x, cmd size=0x%08x,arg=0x%lx\n", info, 
		   (unsigned int)_IOC_NR(cmd), (unsigned int)_IOC_SIZE(cmd),arg);

    switch(cmd)
    {
        case COMPAT_MTKFB_GET_POWERSTATE:
        {
            compat_uint_t __user *data32;
            int power_state = 0;
            
            data32 = compat_ptr(arg);   
            if(primary_display_is_sleepd())
                power_state = 0;
            else
                power_state = 1;
		  
            if (put_user(power_state, data32))
            {
                  printk("MTKFB_GET_POWERSTATE failed\n");
                  return -EFAULT;
            }
            printk("MTKFB_GET_POWERSTATE success %d\n",power_state);
            return 0;
        }
        case COMPAT_MTKFB_CAPTURE_FRAMEBUFFER:
        {
              compat_ulong_t __user *data32; 
              unsigned long  *pbuf; 
              compat_ulong_t l;
              
			  data32 = compat_ptr(arg);
              pbuf = compat_alloc_user_space(sizeof(unsigned long));
	          r = get_user(l, data32);
	          r |= put_user(l, pbuf);
              primary_display_capture_framebuffer_ovl(*pbuf, eBGRA8888);
			  return  0;
        }
        
        default:
          printk("error, unknown mtkfb_compat_ioctl, info=%p, cmd nr=0x%08x, cmd size=0x%08x\n", info, 
        (unsigned int)_IOC_NR(cmd), (unsigned int)_IOC_SIZE(cmd));
          return -EINVAL;
    }
}
#endif

static int mtkfb_pan_display_proxy(struct fb_var_screeninfo *var, struct fb_info *info)
{
#ifdef CONFIG_MTPROF_APPLAUNCH  // eng enable, user disable
	LOG_PRINT(ANDROID_LOG_INFO, "AppLaunch", "mtkfb_pan_display_proxy.\n");
#endif
    return mtkfb_pan_display_impl(var, info);
}


/* Callback table for the frame buffer framework. Some of these pointers
 * will be changed according to the current setting of fb_info->accel_flags.
 */
static struct fb_ops mtkfb_ops = {
    .owner          = THIS_MODULE,
    .fb_open        = mtkfb_open,
    .fb_release     = mtkfb_release,
    .fb_setcolreg   = mtkfb_setcolreg,
    .fb_pan_display = mtkfb_pan_display_proxy,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    .fb_imageblit   = cfb_imageblit,
    .fb_cursor      = mtkfb_soft_cursor,
    .fb_check_var   = mtkfb_check_var,
    .fb_set_par     = mtkfb_set_par,
    .fb_ioctl       = mtkfb_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = mtkfb_compat_ioctl,
#endif     
};

/*
 * ---------------------------------------------------------------------------
 * Sysfs interface
 * ---------------------------------------------------------------------------
 */

static int mtkfb_register_sysfs(struct mtkfb_device *fbdev)
{
    NOT_REFERENCED(fbdev);

    return 0;
}

static void mtkfb_unregister_sysfs(struct mtkfb_device *fbdev)
{
    NOT_REFERENCED(fbdev);
}

/*
 * ---------------------------------------------------------------------------
 * LDM callbacks
 * ---------------------------------------------------------------------------
 */
/* Initialize system fb_info object and set the default video mode.
 * The frame buffer memory already allocated by lcddma_init
 */
static int mtkfb_fbinfo_init(struct fb_info *info)
{
    struct mtkfb_device *fbdev = (struct mtkfb_device *)info->par;
    struct fb_var_screeninfo var;
    int r = 0;

    DISPFUNC();

    BUG_ON(!fbdev->fb_va_base);
    info->fbops = &mtkfb_ops;
    info->flags = FBINFO_FLAG_DEFAULT;
    info->screen_base = (char *) fbdev->fb_va_base;
    info->screen_size = fbdev->fb_size_in_byte;
    info->pseudo_palette = fbdev->pseudo_palette;

    r = fb_alloc_cmap(&info->cmap, 32, 0);
    if (r != 0)
        DISPERR("unable to allocate color map memory\n");

    // setup the initial video mode (RGB565)

    memset(&var, 0, sizeof(var));

    var.xres         = MTK_FB_XRES;
    var.yres         = MTK_FB_YRES;
    var.xres_virtual = MTK_FB_XRESV;
    var.yres_virtual = MTK_FB_YRESV;
    var.bits_per_pixel = 32;
	
    var.transp.offset   = 24; var.red.length   = 8;
#if 0
    var.red.offset   = 16; var.red.length   = 8;
    var.green.offset =  8; var.green.length = 8;
    var.blue.offset  =  0; var.blue.length  = 8;
#else
    var.red.offset	 =  0; var.red.length	= 8;
    var.green.offset =	8; var.green.length = 8;
    var.blue.offset  = 16; var.blue.length	= 8;
#endif

    var.width  = DISP_GetActiveWidth();
    var.height = DISP_GetActiveHeight();

    var.activate = FB_ACTIVATE_NOW;

    r = mtkfb_check_var(&var, info);
    if (r != 0)
        DISPERR("failed to mtkfb_check_var\n");

    info->var = var;

    r = mtkfb_set_par(info);
    if (r != 0)
        DISPERR("failed to mtkfb_set_par\n");

    MSG_FUNC_LEAVE();
    return r;
}

/* Release the fb_info object */
static void mtkfb_fbinfo_cleanup(struct mtkfb_device *fbdev)
{
    MSG_FUNC_ENTER();

    fb_dealloc_cmap(&fbdev->fb_info->cmap);

    MSG_FUNC_LEAVE();
}

#define RGB565_TO_ARGB8888(x)   \
    ((((x) &   0x1F) << 3) |    \
     (((x) &  0x7E0) << 5) |    \
     (((x) & 0xF800) << 8) |    \
     (0xFF << 24)) // opaque

/* Init frame buffer content as 3 R/G/B color bars for debug */
static int init_framebuffer(struct fb_info *info)
{
    void *buffer = info->screen_base +
                   info->var.yoffset * info->fix.line_length;

    // clean whole frame buffer as black
    memset(buffer, 0, info->screen_size);

    return 0;
}


/* Free driver resources. Can be called to rollback an aborted initialization
 * sequence.
 */
static void mtkfb_free_resources(struct mtkfb_device *fbdev, int state)
{
    int r = 0;

    switch (state) {
    case MTKFB_ACTIVE:
        r = unregister_framebuffer(fbdev->fb_info);
        ASSERT(0 == r);
      //lint -fallthrough
    case 5:
        mtkfb_unregister_sysfs(fbdev);
      //lint -fallthrough
    case 4:
        mtkfb_fbinfo_cleanup(fbdev);
      //lint -fallthrough
    case 3:
       // DISP_CHECK_RET(DISP_Deinit());
      //lint -fallthrough
    case 2:
        dma_free_coherent(0, fbdev->fb_size_in_byte,
                          fbdev->fb_va_base, fbdev->fb_pa_base);
      //lint -fallthrough
    case 1:
        dev_set_drvdata(fbdev->dev, NULL);
        framebuffer_release(fbdev->fb_info);
      //lint -fallthrough
    case 0:
      /* nothing to free */
        break;
    default:
        BUG();
    }
}

extern char* saved_command_line;
char mtkfb_lcm_name[256] = {0};

void disp_get_fb_address(unsigned long *fbVirAddr, unsigned long *fbPhysAddr)
{
    struct mtkfb_device  *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;

    *fbVirAddr = (unsigned long)fbdev->fb_va_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
    *fbPhysAddr =(unsigned long)fbdev->fb_pa_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
}

static int mtkfb_fbinfo_modify(struct fb_info *info)
{
    struct fb_var_screeninfo var;
    int r = 0;

    memcpy(&var, &(info->var), sizeof(var));
    var.activate		= FB_ACTIVATE_NOW;
    var.bits_per_pixel  = 32;
    var.transp.offset	= 24;
    var.transp.length	= 8;
	var.red.offset = 16;
	var.red.length = 8;
	var.green.offset = 8;
	var.green.length = 8;
	var.blue.offset = 0;
	var.blue.length = 8;
    var.yoffset         = var.yres;

    r = mtkfb_check_var(&var, info);
    if (r != 0)
        PRNERR("failed to mtkfb_check_var\n");

    info->var = var;

    r = mtkfb_set_par(info);
    if (r != 0)
        PRNERR("failed to mtkfb_set_par\n");

    return r;
}
extern unsigned char data_rgb888_64x64[12288];
int ddp_lcd_test_dpi(void);

static void _mtkfb_draw_point(unsigned int addr, unsigned int x, unsigned int y, unsigned int color)
{

}

static void _mtkfb_draw_block(unsigned long addr, unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color)
{
	int i = 0;
	int j = 0;
	unsigned long start_addr = addr+MTK_FB_XRESV*4*y+x*4;
	for(j=0;j<h;j++)
	{
		for(i = 0;i<w;i++)
		{
			*(unsigned int*)(start_addr + i*4 + j*MTK_FB_XRESV*4) = color;
		}
	}
}

char* mtkfb_find_lcm_driver(void)
{
	BOOL ret = FALSE;
	char *p, *q;

#ifdef CONFIG_OF
	_parse_tag_videolfb();
#else
	p = strstr(saved_command_line, "lcm=");
	if(p == NULL)
	{
		// we can't find lcm string in the command line, the uboot should be old version
		return NULL;
	}

	p += 6;
	if((p - saved_command_line) > strlen(saved_command_line+1))
	{
		return NULL;
	}

	printk("%s, %s\n", __func__, p);
	q = p;
	while(*q != ' ' && *q != '\0')
		q++;

	memset((void*)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char*)mtkfb_lcm_name, (const char*)p, (int)(q-p));
	mtkfb_lcm_name[q-p+1]='\0';
#endif
	DISPMSG("%s, %s\n", __func__, mtkfb_lcm_name);
	return mtkfb_lcm_name;
}


static long int get_current_time_us(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

UINT32 color = 0;
unsigned int mtkfb_fm_auto_test(void)
{
	unsigned int result = 0;
	unsigned int i=0;
	unsigned long fbVirAddr;
	UINT32 fbsize;
	int r = 0;
	unsigned int *fb_buffer;
    struct mtkfb_device  *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	struct fb_var_screeninfo var;
	fbVirAddr = (unsigned long)fbdev->fb_va_base;
	fb_buffer = (unsigned int*)fbVirAddr;

	memcpy(&var, &(mtkfb_fbi->var), sizeof(var));
	var.activate		= FB_ACTIVATE_NOW;
	var.bits_per_pixel	= 32;
	var.transp.offset	= 24;
	var.transp.length	= 8;
	var.red.offset		= 16; var.red.length	= 8;
	var.green.offset	= 8;  var.green.length	= 8;
	var.blue.offset 	= 0;  var.blue.length	= 8;

	r = mtkfb_check_var(&var, mtkfb_fbi);
	if (r != 0)
		PRNERR("failed to mtkfb_check_var\n");

	mtkfb_fbi->var = var;

#if 0
	r = mtkfb_set_par(mtkfb_fbi);

	if (r != 0)
		PRNERR("failed to mtkfb_set_par\n");
#endif
	if(color == 0)
		color = 0xFF00FF00;
	fbsize = ALIGN_TO(DISP_GetScreenWidth(),MTK_FB_ALIGNMENT)*DISP_GetScreenHeight()*MTK_FB_PAGES;
	for(i=0;i<fbsize;i++)
		*fb_buffer++ = color;
#if 0
	if(!primary_display_is_video_mode())
		primary_display_trigger(1, NULL, 0);
#endif
	mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
	msleep(100);

	result = primary_display_lcm_ATA();
	
	if(result == 0){
		DISPMSG("ATA LCM failed\n");
	}else{
		DISPMSG("ATA LCM passed\n");
	}
	
	return result;
}


static int _mtkfb_internal_test(unsigned long va, unsigned int w, unsigned int h)
{
	// this is for debug, used in bring up day	
	unsigned int i = 0;
	unsigned int color = 0;
	int _internal_test_block_size = 120;
	for(i=0;i<w*h/_internal_test_block_size/_internal_test_block_size;i++)
	{
		color = (i&0x1)*0xff;
		//color += ((i&0x2)>>1)*0xff00;
		//color += ((i&0x4)>>2)*0xff0000;
		color += 0xff000000U;
		_mtkfb_draw_block(va, 
						i%(w/_internal_test_block_size)*_internal_test_block_size, 
						i/(w/_internal_test_block_size)*_internal_test_block_size, 
						_internal_test_block_size, 
						_internal_test_block_size, 
						color);
	}
	//unsigned long ttt = get_current_time_us();
	//for(i=0;i<1000;i++)
	primary_display_trigger(1, NULL, 0);
	//ttt = get_current_time_us()-ttt;
	//DISPMSG("%s, update 1000 times, fps=%2d.%2d\n", __func__, (1000*100/(ttt/1000/1000))/100, (1000*100/(ttt/1000/1000))%100);
return 0;

	_internal_test_block_size = 20;
	for(i=0;i<w*h/_internal_test_block_size/_internal_test_block_size;i++)
	{
		color = (i&0x1)*0xff;
		color += ((i&0x2)>>1)*0xff00;
		color += ((i&0x4)>>2)*0xff0000;
		color += 0xff000000U;
		_mtkfb_draw_block(va, 
						i%(w/_internal_test_block_size)*_internal_test_block_size, 
						i/(w/_internal_test_block_size)*_internal_test_block_size, 
						_internal_test_block_size, 
						_internal_test_block_size, 
						color);
	}
	primary_display_trigger(1, NULL, 0);
	_internal_test_block_size = 30;
	for(i=0;i<w*h/_internal_test_block_size/_internal_test_block_size;i++)
	{
		color = (i&0x1)*0xff;
		color += ((i&0x2)>>1)*0xff00;
		color += ((i&0x4)>>2)*0xff0000;
		color += 0xff000000U;
		_mtkfb_draw_block(va, 
						i%(w/_internal_test_block_size)*_internal_test_block_size, 
						i/(w/_internal_test_block_size)*_internal_test_block_size, 
						_internal_test_block_size, 
						_internal_test_block_size, 
						color);
	}
	primary_display_trigger(1, NULL, 0);

	#if 0
	
	//memset(va, 0xff, w*h*4);   
	int i = 0;
	int j = 0;
	for(j = 0;j<10;j++)
	{
		for(i=0;i<64;i++)
		{
			memcpy((void*)(va+j*720*70*3+720*3*i), data_rgb888_64x64+64*3*i, 64*3);
		} 
	}
    

	for(j = 0;j<10;j++)
	{
		for(i=0;i<64;i++)
		{
			memcpy((void*)(va+720*1280*3+360*3+j*720*70*3+720*3*i), data_rgb888_64x64+64*3*i, 64*3);
		} 
	}

	primary_display_trigger(1);
	memset(va, 0xff, w*h*4);
	primary_display_trigger(1);
	memset(va, 0x00, w*h*4);
	primary_display_trigger(1);
	memset(va, 0x88, w*h*4);
	primary_display_trigger(1);
	memset(va, 0xcc, w*h*4);
	primary_display_trigger(1);
	memset(va, 0x22, w*h*4);
	primary_display_trigger(1);
#endif
	return 0;
}
#ifdef CONFIG_OF
struct tag_videolfb {
	unsigned long long fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1]; /* this is the minimum size */
};
unsigned int islcmconnected = 0;
unsigned int vramsize       = 0;
phys_addr_t fb_base        = 0;
static int is_videofb_parse_done;
static int fb_early_init_dt_get_chosen(unsigned long node, const char *uname, int depth, void *p_ret_node)
{
	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	*(unsigned long *)p_ret_node = node;
    return 1;
}

int __parse_tag_videolfb_extra(unsigned long node)
{
    void* prop;
	unsigned long size = 0;
	u32 fb_base_h, fb_base_l;
	int ret;

	prop = of_get_flat_dt_prop(node, "atag,videolfb-fb_base_h", NULL);
	if(!prop) return -1;
	fb_base_h = of_read_number(prop, 1);

	prop = of_get_flat_dt_prop(node, "atag,videolfb-fb_base_l", NULL);
	if(!prop) return -1;
	fb_base_l = of_read_number(prop, 1);

	fb_base = ((u64)fb_base_h<<32) | (u64)fb_base_l;

	prop = of_get_flat_dt_prop(node, "atag,videolfb-islcmfound", NULL);
	if(!prop) return -1;
	islcmconnected = of_read_number(prop, 1);

	prop = of_get_flat_dt_prop(node, "atag,videolfb-fps", NULL);
	if(!prop) return -1;
	lcd_fps = of_read_number(prop, 1);
	if(0 == lcd_fps) lcd_fps = 6000;

	prop = of_get_flat_dt_prop(node, "atag,videolfb-vramSize", NULL);
	if(!prop) return -1;
	vramsize = of_read_number(prop, 1);

	prop = of_get_flat_dt_prop(node, "atag,videolfb-fb_base_l", NULL);
	if(!prop) return -1;
	fb_base_l = of_read_number(prop, 1);

	prop = of_get_flat_dt_prop(node, "atag,videolfb-lcmname", &size);
	if(!prop) return -1;
	if(size >= sizeof(mtkfb_lcm_name)) {
		DISPCHECK("%s: error to get lcmname size=%ld\n", __FUNCTION__, size);
		return -1;
	}
	memset((void*)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char *)mtkfb_lcm_name, prop, sizeof(mtkfb_lcm_name));
	mtkfb_lcm_name[size] = '\0';

	return 0;
}

int __parse_tag_videolfb(unsigned long node)
{
    struct tag_videolfb *videolfb_tag = NULL;
	unsigned long size = 0;

	videolfb_tag = (struct tag_videolfb*)of_get_flat_dt_prop(node, "atag,videolfb", &size);
	if(videolfb_tag)
	{
			memset((void*)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
			strcpy((char *)mtkfb_lcm_name,videolfb_tag->lcmname);
			mtkfb_lcm_name[strlen(videolfb_tag->lcmname)] = '\0';

			lcd_fps = videolfb_tag->fps;
		if(0 == lcd_fps) lcd_fps = 6000;

			islcmconnected = videolfb_tag->islcmfound;
			vramsize = videolfb_tag->vram;
			fb_base  = videolfb_tag->fb_base;
		
		return 0;
	}
	else
	{
		DISPCHECK("[DT][videolfb] videolfb_tag not found\n");
		return -1;
	}
}


int _parse_tag_videolfb(void)
{
	int ret;
	unsigned long node = 0;

	DISPCHECK("[DT][videolfb]isvideofb_parse_done = %d\n",is_videofb_parse_done);

	if(is_videofb_parse_done) return;
	
	ret = of_scan_flat_dt(fb_early_init_dt_get_chosen, &node);
	if(node) {
		ret = __parse_tag_videolfb_extra(node);
		if(!ret)
			goto found;

		ret = __parse_tag_videolfb(node);
		if(!ret)
			goto found;
	}
	
	DISPCHECK("[DT][videolfb] of_chosen not found\n");
	return -1;

found:
	is_videofb_parse_done = 1;
	DISPCHECK("[DT][videolfb] islcmfound = %d\n", islcmconnected);
	DISPCHECK("[DT][videolfb] fps        = %d\n", lcd_fps);
	DISPCHECK("[DT][videolfb] fb_base    = 0x%pa\n", &fb_base);
	DISPCHECK("[DT][videolfb] vram       = %d\n", vramsize);
	DISPCHECK("[DT][videolfb] lcmname    = %s\n", mtkfb_lcm_name);
	return 0;
}

phys_addr_t mtkfb_get_fb_base(void)
{
	_parse_tag_videolfb();
	return fb_base;
}
size_t mtkfb_get_fb_size(void)
{
	_parse_tag_videolfb();
	return vramsize;
}
EXPORT_SYMBOL(mtkfb_get_fb_base);
EXPORT_SYMBOL(mtkfb_get_fb_size);
#endif


static int mtkfb_probe(struct device *dev)
{
	struct mtkfb_device    *fbdev = NULL;
	struct fb_info         *fbi;
	int                    init_state;
	int                    r = 0;
	char *p = NULL;
	printk("mtkfb_probe\n");
	if(get_boot_mode() == META_BOOT || get_boot_mode() == FACTORY_BOOT|| get_boot_mode() == ADVMETA_BOOT || get_boot_mode() == RECOVERY_BOOT)
		first_update = false;

	_parse_tag_videolfb();

    init_state = 0;


    fbi = framebuffer_alloc(sizeof(struct mtkfb_device), dev);
    if (!fbi) 
    {
        DISPERR("unable to allocate memory for device info\n");
        r = -ENOMEM;
        goto cleanup;
    }
    mtkfb_fbi = fbi;

    fbdev = (struct mtkfb_device *)fbi->par;
    fbdev->fb_info = fbi;
    fbdev->dev = dev;
    dev_set_drvdata(dev, fbdev);

	DISPMSG("mtkfb_probe: fb_pa = %pa\n", &fb_base);

	disp_hal_allocate_framebuffer(fb_base, (fb_base + vramsize - 1), (unsigned int*)&fbdev->fb_va_base, &fb_pa);
	fbdev->fb_pa_base = fb_base;

	primary_display_set_frame_buffer_address(fbdev->fb_va_base,fb_pa);

	// mtkfb should parse lcm name from kernel boot command line
	primary_display_init(mtkfb_find_lcm_driver(), lcd_fps);

	init_state++;		/* 1 */
	MTK_FB_XRES  = DISP_GetScreenWidth();
	MTK_FB_YRES  = DISP_GetScreenHeight();
	fb_xres_update = MTK_FB_XRES;
	fb_yres_update = MTK_FB_YRES;

	MTK_FB_BPP   = DISP_GetScreenBpp();
	MTK_FB_PAGES = DISP_GetPages();
	DISPMSG("MTK_FB_XRES=%d, MTKFB_YRES=%d, MTKFB_BPP=%d, MTK_FB_PAGES=%d, MTKFB_LINE=%d, MTKFB_SIZEV=%d\n", MTK_FB_XRES, MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES, MTK_FB_LINE, MTK_FB_SIZEV);
	fbdev->fb_size_in_byte = MTK_FB_SIZEV;

	/* Allocate and initialize video frame buffer */
	DISPMSG("[FB Driver] fbdev->fb_pa_base = %lx, fbdev->fb_va_base = 0x%lx\n", (unsigned long)&(fbdev->fb_pa_base), (unsigned long)(fbdev->fb_va_base));

    if (!fbdev->fb_va_base) 
    {
        DISPERR("unable to allocate memory for frame buffer\n");
        r = -ENOMEM;
        goto cleanup;
    }
    init_state++;   // 2

    /* Register to system */



    r = mtkfb_fbinfo_init(fbi);
    if (r)
    {
    	DISPERR("mtkfb_fbinfo_init fail, r = %d\n", r);
        goto cleanup;
    }
    init_state++;   // 4
	DISPMSG("\nmtkfb_fbinfo_init done\n");

	if(disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
	{
		// dal_init should after mtkfb_fbinfo_init, otherwise layer 3 will show dal background color
		DAL_STATUS ret;
		unsigned long fbVA = fbdev->fb_va_base;
		unsigned long fbPA = fb_pa;
		/// DAL init here
		fbVA += DISP_GetFBRamSize();
		fbPA += DISP_GetFBRamSize();
		ret = DAL_Init(fbVA, fbPA);
	}

	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
	{
		_mtkfb_internal_test(fbdev->fb_va_base, MTK_FB_XRES, MTK_FB_YRES); 
	}
	
    r = mtkfb_register_sysfs(fbdev);
	if (r) 
	{
	    	DISPERR("mtkfb_register_sysfs fail, r = %d\n", r);
		goto cleanup;
	}
    init_state++;   // 5

    r = register_framebuffer(fbi);
    if (r != 0) {
        DISPERR("register_framebuffer failed\n");
        goto cleanup;
    }

	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
	{
		primary_display_diagnose();
	}

    fbdev->state = MTKFB_ACTIVE;

    MSG_FUNC_LEAVE();
    return 0;

cleanup:
    mtkfb_free_resources(fbdev, init_state);

    MSG_FUNC_LEAVE();
    return r;
}

/* Called when the device is being detached from the driver */
static int mtkfb_remove(struct device *dev)
{
    struct mtkfb_device *fbdev = dev_get_drvdata(dev);
    enum mtkfb_state saved_state = fbdev->state;

    MSG_FUNC_ENTER();
    /* FIXME: wait till completion of pending events */

    fbdev->state = MTKFB_DISABLED;
    mtkfb_free_resources(fbdev, saved_state);

    MSG_FUNC_LEAVE();
    return 0;
}

/* PM suspend */
static int mtkfb_suspend(struct device *pdev, pm_message_t mesg)
{
    NOT_REFERENCED(pdev);
    MSG_FUNC_ENTER();
    MTKFB_LOG("[FB Driver] mtkfb_suspend(): 0x%x\n", mesg.event);
    ovl2mem_wait_done();
    
    MSG_FUNC_LEAVE();
    return 0;
}
bool mtkfb_is_suspend(void)
{
    return primary_display_is_sleepd();
}
EXPORT_SYMBOL(mtkfb_is_suspend);

int mtkfb_ipoh(struct notifier_block *nb,unsigned long val,void *ign)
{
	switch(val){
		case PM_HIBERNATION_PREPARE:
			primary_display_ipoh_hiberation_prepare();
			return NOTIFY_DONE;
		case PM_RESTORE_PREPARE:
			primary_display_ipoh_restore_prepare();
			return NOTIFY_DONE;
 		case PM_POST_HIBERNATION:
			primary_display_ipoh_post_hiberation();
			return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

int mtkfb_ipo_init(void)
{
	pm_nb.notifier_call = mtkfb_ipoh;
	pm_nb.priority = 0;
	register_pm_notifier(&pm_nb);
}

static void mtkfb_shutdown(struct device *pdev)
{
    	MTKFB_LOG("[FB Driver] mtkfb_shutdown()\n");
    	//mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF);
    #ifdef CONFIG_CM865_MAINBOARD 
  		bq24296_set_otg_config(0); //add by longcheer_liml_2015_11_13
  	#endif
    	if (!lcd_fps)
        	msleep(30);
    	else
        	msleep(2*100000/lcd_fps); // Delay 2 frames.

	if(primary_display_is_sleepd())
	{
		MTKFB_LOG("mtkfb has been power off\n");
		return;
	}

	MTKFB_LOG("[FB Driver] cci400_sel_for_ddp\n");
	cci400_sel_for_ddp();

	primary_display_suspend();
    	MTKFB_LOG("[FB Driver] leave mtkfb_shutdown\n");
}

void mtkfb_clear_lcm(void)
{
#if 0
	int i;
    unsigned int layer_status[DDP_OVL_LAYER_MUN]={0};
    mutex_lock(&OverlaySettingMutex);
    for(i=0;i<DDP_OVL_LAYER_MUN;i++)
    {
        layer_status[i] = cached_layer_config[i].layer_en;
        cached_layer_config[i].layer_en = 0;
        cached_layer_config[i].isDirty = 1;
    }
    atomic_set(&OverlaySettingDirtyFlag, 1);
    atomic_set(&OverlaySettingApplied, 0);
    mutex_unlock(&OverlaySettingMutex);

    //DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update));
    //DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update));
    //DISP_WaitForLCDNotBusy();
    mutex_lock(&OverlaySettingMutex);
    for(i=0;i<DDP_OVL_LAYER_MUN;i++)
    {
        cached_layer_config[i].layer_en = layer_status[i];
        cached_layer_config[i].isDirty = 1;
    }
    atomic_set(&OverlaySettingDirtyFlag, 1);
    atomic_set(&OverlaySettingApplied, 0);
    mutex_unlock(&OverlaySettingMutex);
#endif
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtkfb_early_suspend(struct early_suspend *h)
{
	int ret=0;
	
	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return;
	
	DISPMSG("[FB Driver] enter early_suspend\n");
	mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF);
	msleep(30);

	ret = primary_display_suspend();

	if(ret<0)
	{
		DISPERR("primary display suspend failed\n");
		return;
	}
	
	DISPMSG("[FB Driver] leave early_suspend\n");
	
	return;
}
#endif

/* PM resume */
static int mtkfb_resume(struct device *pdev)
{
    NOT_REFERENCED(pdev);
    MSG_FUNC_ENTER();
    MTKFB_LOG("[FB Driver] mtkfb_resume()\n");
    MSG_FUNC_LEAVE();
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtkfb_late_resume(struct early_suspend *h)
{
	int ret=0;

	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return;

	DISPMSG("[FB Driver] enter late_resume\n");   
	
	ret = primary_display_resume();

	if (ret) 
	{
		DISPERR("primary display resume failed\n");
		return;
	}
	
	DISPMSG("[FB Driver] leave late_resume\n");   

	return;
}
#endif

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int mtkfb_pm_suspend(struct device *device)
{
    //pr_debug("calling %s()\n", __func__);

    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return mtkfb_suspend((struct device *)pdev, PMSG_SUSPEND);
}

int mtkfb_pm_resume(struct device *device)
{
    //pr_debug("calling %s()\n", __func__);

    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return mtkfb_resume((struct device *)pdev);
}

int mtkfb_pm_freeze(struct device *device)
{
	primary_display_freeze();
	return 0;
}

int mtkfb_pm_restore_noirq(struct device *device)
{
    //disphal_pm_restore_noirq(device);
    is_ipoh_bootup = true;
    return 0;

}
/*---------------------------------------------------------------------------*/
#else /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/
#define mtkfb_pm_suspend NULL
#define mtkfb_pm_resume  NULL
#define mtkfb_pm_restore_noirq NULL
#define mtkfb_pm_freeze NULL
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/
static const struct of_device_id mtkfb_of_ids[] = {
	{ .compatible = "mediatek,MTKFB", },
	{}
};
struct dev_pm_ops mtkfb_pm_ops = {
    .suspend = mtkfb_pm_suspend,
    .resume = mtkfb_pm_resume,
    .freeze = mtkfb_pm_freeze,
    .thaw = mtkfb_pm_resume,
    .poweroff = mtkfb_pm_suspend,
    .restore = mtkfb_pm_resume,
    .restore_noirq = mtkfb_pm_restore_noirq,
};

static struct platform_driver mtkfb_driver =
{
    .driver = {
        .name    = MTKFB_DRIVER,
#ifdef CONFIG_PM
        .pm     = &mtkfb_pm_ops,
#endif
        .bus     = &platform_bus_type,
        .probe   = mtkfb_probe,
        .remove  = mtkfb_remove,
        .suspend = mtkfb_suspend,
        .resume  = mtkfb_resume,
	.shutdown = mtkfb_shutdown,
	.of_match_table = mtkfb_of_ids,
    },
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mtkfb_early_suspend_handler =
{
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = mtkfb_early_suspend,
	.resume = mtkfb_late_resume,
};
#endif

extern 	int is_DAL_Enabled(void);


int mtkfb_get_debug_state(char* stringbuf, int buf_len)
{
	int len = 0;
	struct mtkfb_device  *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;

	unsigned long va = (unsigned long)fbdev->fb_va_base;
	unsigned long mva =(unsigned long)fbdev->fb_pa_base;
	unsigned long pa = fbdev->fb_pa_base;
	unsigned int resv_size = vramsize;
	len += scnprintf(stringbuf+len, buf_len - len, "|--------------------------------------------------------------------------------------|\n");	
	//len += scnprintf(stringbuf+len, buf_len - len, "********MTKFB Driver General Information********\n");
	len += scnprintf(stringbuf+len, buf_len - len, "|Framebuffer VA:0x%lx, PA:0x%lx, MVA:0x%lx, Reserved Size:0x%08x|%d\n", va, pa, mva, resv_size, resv_size);
	len += scnprintf(stringbuf+len, buf_len - len, "|xoffset=%d, yoffset=%d\n", mtkfb_fbi->var.xoffset, mtkfb_fbi->var.yoffset);
	len += scnprintf(stringbuf+len, buf_len - len, "|framebuffer line alignment(for gpu)=%d\n", MTK_FB_ALIGNMENT);
	len += scnprintf(stringbuf+len, buf_len - len, "|xres=%d, yres=%d,bpp=%d,pages=%d,linebytes=%d,total size=%d\n", MTK_FB_XRES, MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES, MTK_FB_LINE, MTK_FB_SIZEV);
	// use extern in case DAL_LOCK is hold, then can't get any debug info
	len += scnprintf(stringbuf+len, buf_len - len, "|AEE Layer is %s\n", isAEEEnabled?"enabled":"disabled");
	
	return len;
}


/* Register both the driver and the device */
int __init mtkfb_init(void)
{
	int r = 0;

	MSG_FUNC_ENTER();

	if (platform_driver_register(&mtkfb_driver)) 
	{
		PRNERR("failed to register mtkfb driver\n");
		r = -ENODEV;
		goto exit;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&mtkfb_early_suspend_handler);
#endif
	PanelMaster_Init();
	DBG_Init();
    mtkfb_ipo_init();
exit:
    MSG_FUNC_LEAVE();
    return r;
}


static void __exit mtkfb_cleanup(void)
{
    MSG_FUNC_ENTER();

    platform_driver_unregister(&mtkfb_driver);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mtkfb_early_suspend_handler);
#endif

	PanelMaster_Deinit();
    DBG_Deinit();

    MSG_FUNC_LEAVE();
}


module_init(mtkfb_init);
module_exit(mtkfb_cleanup);

MODULE_DESCRIPTION("MEDIATEK framebuffer driver");
MODULE_AUTHOR("Xuecheng Zhang <Xuecheng.Zhang@mediatek.com>");
MODULE_LICENSE("GPL");

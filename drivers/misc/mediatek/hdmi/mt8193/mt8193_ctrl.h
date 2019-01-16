#ifndef __mt8193_ctrl_h__
#define __mt8193_ctrl_h__
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "hdmi_drv.h"

#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include "mach/eint.h"
#include "mach/irqs.h"

#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

extern size_t mt8193_log_on;

#define hdmiplllog         (0x1)
#define hdmidgilog         (0x2)
#define hdmitxhotpluglog   (0x4)
#define hdmitxvideolog     (0x8)
#define hdmitxaudiolog     (0x10)
#define hdmihdcplog        (0x20)
#define hdmiceclog         (0x40)
#define hdmiddclog         (0x80)
#define hdmiedidlog        (0x100)
#define hdmidrvlog         (0x200)

#define mt8193hdmialllog   (0xffff)

/* ////////////////////////////////////////////PLL////////////////////////////////////////////////////// */
#define MT8193_PLL_LOG(fmt, arg...) \
	do { \
		if (mt8193_log_on&hdmiplllog) {pr_debug("[mt8193_pll]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
	} while (0)

#define MT8193_PLL_FUNC()	\
	do { \
		if (mt8193_log_on&hdmiplllog) {pr_debug("[mt8193_pll] %s\n", __func__); } \
	} while (0)

/* ////////////////////////////////////////////DGI////////////////////////////////////////////////////// */

#define MT8193_DGI_LOG(fmt, arg...) \
    do { \
        if (mt8193_log_on&hdmidgilog) {pr_debug("[mt8193_dgi1]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
    } while (0)

#define MT8193_DGI_FUNC()	\
    do { \
        if (mt8193_log_on&hdmidgilog) {pr_debug("[mt8193_dgi1] %s\n", __func__); } \
    } while (0)

/* ////////////////////////////////////////////PLUG////////////////////////////////////////////////////// */

#define MT8193_PLUG_LOG(fmt, arg...) \
    do { \
        if (mt8193_log_on&hdmitxhotpluglog) {pr_debug("[mt8193_plug]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
    } while (0)

#define MT8193_PLUG_FUNC()	\
    do { \
        if (mt8193_log_on&hdmitxhotpluglog) {pr_debug("[mt8193_plug] %s\n", __func__); } \
    } while (0)


/* //////////////////////////////////////////////VIDEO//////////////////////////////////////////////////// */

#define MT8193_VIDEO_LOG(fmt, arg...) \
    do { \
        if (mt8193_log_on&hdmitxvideolog) {pr_debug("[mt8193_video]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
    } while (0)

#define MT8193_VIDEO_FUNC()	\
    do { \
        if (mt8193_log_on&hdmitxvideolog) {pr_debug("[mt8193_video] %s\n", __func__); } \
    } while (0)

/* //////////////////////////////////////////////AUDIO//////////////////////////////////////////////////// */

#define MT8193_AUDIO_LOG(fmt, arg...) \
    do { \
        if (mt8193_log_on&hdmitxaudiolog) {pr_debug("[mt8193_audio]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
    } while (0)

#define MT8193_AUDIO_FUNC()	\
    do { \
        if (mt8193_log_on&hdmitxaudiolog) {pr_debug("[mt8193_audio] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////HDCP/////////////////////////////////////////////////// */

#define MT8193_HDCP_LOG(fmt, arg...) \
    do { \
        if (mt8193_log_on&hdmihdcplog) {pr_debug("[mt8193_hdcp]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
    } while (0)

#define MT8193_HDCP_FUNC()	\
    do { \
        if (mt8193_log_on&hdmihdcplog) {pr_debug("[mt8193_hdcp] %s\n", __func__); } \
    } while (0)

/* ///////////////////////////////////////////////CEC/////////////////////////////////////////////////// */

#define MT8193_CEC_LOG(fmt, arg...) \
    do { \
        if (mt8193_log_on&hdmiceclog) {pr_debug("[mt8193_cec]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
    } while (0)

#define MT8193_CEC_FUNC()	\
    do { \
        if (mt8193_log_on&hdmiceclog) {pr_debug("[mt8193_cec] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////DDC////////////////////////////////////////////////// */
#define MT8193_DDC_LOG(fmt, arg...) \
	do { \
		if (mt8193_log_on&hdmiddclog) {pr_debug("[mt8193_ddc]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
	} while (0)

#define MT8193_DDC_FUNC()	\
	do { \
		if (mt8193_log_on&hdmiddclog) {pr_debug("[mt8193_ddc] %s\n", __func__); } \
	} while (0)
/* ///////////////////////////////////////////////EDID////////////////////////////////////////////////// */
#define MT8193_EDID_LOG(fmt, arg...) \
	do { \
		if (mt8193_log_on&hdmiedidlog) {pr_debug("[mt8193_edid]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
	} while (0)

#define MT8193_EDID_FUNC()	\
	do { \
		if (mt8193_log_on&hdmiedidlog) {pr_debug("[mt8193_edid] %s\n", __func__); } \
	} while (0)
/* ////////////////////////////////////////////////DRV///////////////////////////////////////////////// */

#define MT8193_DRV_LOG(fmt, arg...) \
    do { \
        if (mt8193_log_on&hdmidrvlog) {pr_debug("[mt8193_drv]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); } \
    } while (0)

#define MT8193_DRV_FUNC()	\
    do { \
        if (mt8193_log_on&hdmidrvlog) {pr_debug("[mt8193_drv] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////////////////////////////////////////////////////// */

#define RETNULL(cond)       if ((cond)) {MT8193_DRV_LOG("return in %d\n", __LINE__); return; }
#define RETINT(cond, rslt)       if ((cond)) {MT8193_DRV_LOG("return in %d\n", __LINE__); return (rslt); }


extern int mt8193_i2c_read(u16 addr, u32 *data);
extern int mt8193_i2c_write(u16 addr, u32 data);
extern void mt8193_write(u16 u2Reg, u32 u4Data);
extern void vSetClk(void);

#endif
#endif

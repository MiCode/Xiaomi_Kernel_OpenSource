#ifndef __KERNEL_RTPM_PRIO__
#define __KERNEL_RTPM_PRIO__

#define CONFIG_MT_RT_MONITOR
#ifdef CONFIG_MT_RT_MONITOR
#define MT_ALLOW_RT_PRIO_BIT 0x10000000
#else
#define MT_ALLOW_RT_PRIO_BIT 0x0
#endif

#define REG_RT_PRIO(x) ((x) | MT_ALLOW_RT_PRIO_BIT)

/***********************************************************************
 * Processes scheduled under one of the real-time policies (SCHED_FIFO, SCHED_RR)
 * have a sched_priority value in the range:
 * 1 (low) to 99 (high).
************************************************************************/
/* ////////////////////////////////////////////////////////////////////// */
/* DEFINE MM GROUP PRIORITY */
#define RTPM_PRIO_MM_GROUP_BASE			(10)
#define RTPM_PRIO_MM_GROUP_A			(RTPM_PRIO_MM_GROUP_BASE+0)
#define RTPM_PRIO_MM_GROUP_B			(RTPM_PRIO_MM_GROUP_BASE+10)
#define RTPM_PRIO_MM_GROUP_C			(RTPM_PRIO_MM_GROUP_BASE+20)
#define RTPM_PRIO_MM_GROUP_D			(RTPM_PRIO_MM_GROUP_BASE+30)
#define RTPM_PRIO_MM_GROUP_E			(RTPM_PRIO_MM_GROUP_BASE+40)
#define RTPM_PRIO_MM_GROUP_F			(RTPM_PRIO_MM_GROUP_BASE+50)
#define RTPM_PRIO_MM_GROUP_G			(RTPM_PRIO_MM_GROUP_BASE+60)
#define RTPM_PRIO_MM_GROUP_H			(RTPM_PRIO_MM_GROUP_BASE+70)
#define RTPM_PRIO_MM_GROUP_I			(RTPM_PRIO_MM_GROUP_BASE+80)

/* ////////////////////////////////////////////////////////////////////// */
/* DEFIN MTK RT PRIORITY */

#define RTPM_PRIO_CPU_CALLBACK              REG_RT_PRIO(98)
#define RTPM_PRIO_SWLOCKUP                  REG_RT_PRIO(98)
#define RTPM_PRIO_AED                       REG_RT_PRIO(28)
#define RTPM_PRIO_WDT                       REG_RT_PRIO(99)

#define RTPM_PRIO_TPD                       REG_RT_PRIO(4)
#define RTPM_PRIO_KSDIOIRQ                  REG_RT_PRIO(1)
#define RTPM_PRIO_MTLTE_SYS_SDIO_THREAD     REG_RT_PRIO(1)

#define RTPM_PRIO_AUDIO_PLAYBACK            REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+5)
#define RTPM_PRIO_VIDEO_PLAYBACK_THREAD     REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+5)	/* TimeEventQueue */
#define RTPM_PRIO_SCRN_UPDATE               REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+4)
#define RTPM_PRIO_AUDIO_COMMAND             REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+2)
#define RTPM_PRIO_AUDIO_CCCI_THREAD         REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+2)

#define RTPM_PRIO_CAMERA_TOPBASE            REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+1)
#define RTPM_PRIO_CAMERA_PREVIEW            REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+1)
#define RTPM_PRIO_CAMERA_COMPRESS           REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+0)

#define RTPM_PRIO_MATV_AUDIOPLAYER          REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+0)
#define RTPM_PRIO_FM_AUDIOPLAYER            REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+0)
#define RTPM_PRIO_AUDIO_I2S                 REG_RT_PRIO(RTPM_PRIO_MM_GROUP_I+0)

#define RTPM_PRIO_CAMERA_DISPLAY            REG_RT_PRIO(RTPM_PRIO_MM_GROUP_H+9)
#define RTPM_PRIO_CAMERA_SHUTTER            REG_RT_PRIO(RTPM_PRIO_MM_GROUP_H+9)
#define RTPM_PRIO_CAMERA_RECORD             REG_RT_PRIO(RTPM_PRIO_MM_GROUP_H+9)
#define RTPM_PRIO_FB_THREAD                 REG_RT_PRIO(RTPM_PRIO_MM_GROUP_H+7)
#define RTPM_PRIO_AUDIO_RECORD              REG_RT_PRIO(RTPM_PRIO_MM_GROUP_H+6)
#define RTPM_PRIO_VSYNC_THREAD              REG_RT_PRIO(RTPM_PRIO_MM_GROUP_H+5)
#define RTPM_PRIO_SURFACEFLINGER            REG_RT_PRIO(RTPM_PRIO_MM_GROUP_H+4)

#define RTPM_PRIO_VIDEO_YUV_BUFFER          REG_RT_PRIO(RTPM_PRIO_MM_GROUP_G+8)
#define RTPM_PRIO_OMX_AUDIO                 REG_RT_PRIO(RTPM_PRIO_MM_GROUP_G+6)
#define RTPM_PRIO_OMX_CMD_AUDIO             REG_RT_PRIO(RTPM_PRIO_MM_GROUP_G+6)
#define RTPM_PRIO_OMX_VIDEO_ENCODE          REG_RT_PRIO(RTPM_PRIO_MM_GROUP_G+5)
#define RTPM_PRIO_OMX_VIDEO                 REG_RT_PRIO(RTPM_PRIO_MM_GROUP_G+5)
#define RTPM_PRIO_OMX_VIDEO_DECODE          REG_RT_PRIO(RTPM_PRIO_MM_GROUP_G+4)

#define RTPM_PRIO_VIDEO_BS_BUFFER           REG_RT_PRIO(RTPM_PRIO_MM_GROUP_G+3)
#define RTPM_PRIO_MIDI_FILE                 REG_RT_PRIO(RTPM_PRIO_MM_GROUP_C+0)

#define RTPM_PRIO_AUDIOTRACK_THREAD         REG_RT_PRIO(1)
#define RTPM_PRIO_GPS_DRIVER				REG_RT_PRIO(1)
/* Total */
#define RTPM_PRIO_NUM	30
/* ////////////////////////////////////////////////////////////////////////////// */
/* Removed */
/* #define RTPM_PRIO_FB_THREAD                 REG_RT_PRIO(87) */
/* #define RTPM_PRIO_SURFACE_OUT               REG_RT_PRIO(80) */

#endif

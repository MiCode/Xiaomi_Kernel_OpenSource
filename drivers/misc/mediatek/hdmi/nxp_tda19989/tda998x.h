/*****************************************************************************/
/* Copyright (c) 2009 NXP Semiconductors BV                                  */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, using version 2 of the License.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307       */
/* USA.                                                                      */
/*                                                                           */
/*****************************************************************************/

#ifndef __tx_h__
#define __tx_h__



#include "tda998x_ioctl.h"

#define HDMITX_NAME "hdmitx"

#define POLLING_WQ_NAME "TDA_POLLING"
#define HDCP_CHECK_EVERY_MS 35
#define CHECK_EVERY_XX_MS 200
#define OMAP_LCD_GPIO 8

#define TDA_MAJOR 234		/* old-style interval of device numbers */
#define MAX_MINOR 1		/* 1 minor but 2 access : 1 more for pooling */


/* common I2C define with kernel */
/* should be the same as arch/arm/mach-omap2/board-zoom2.c */
#define TX_NAME "tda998X"
#define TDA998X_I2C_SLAVEADDRESS 0x70

#define TDA_IRQ_CALIB 107

#define EDID_BLOCK_COUNT 4
#define EDID_BLOCK_SIZE 128
#define MAX_EDID_TRIAL 5
#define NO_PHY_ADDR 0xFFFF

#define HDCP_IS_NOT_INSTALLED TMDL_HDMITX_HDCP_CHECK_NUM	/* ugly is bad ! */

#define LOG(type, fmt, args...) {if (this->param.verbose) {printk(type HDMITX_NAME":%s:" fmt, __func__, ## args); } }
/* not found the kernel "strerror" one! If someone knows, please replace it */
#define ERR_TO_STR(e)((e == -ENODATA)?"ENODATA, no data available" : \
                      (e == -ENOMEM) ? "ENOMEM, no memory available" : \
                      (e == -EINVAL) ? "EINVAL, invalid argument" : \
                      (e == -EIO) ? "EIO, input/output error" : \
                      (e == -ETIMEDOUT) ? "ETIMEOUT, timeout has expired" : \
                      (e == -EBUSY) ? "EBUSY, device or resource busy" : \
                      (e == -ENOENT) ? "ENOENT, no such file or directory" : \
                      (e == -EACCES) ? "EACCES, permission denied" : \
                      (e == 0) ? "" : \
		      "!UNKNOWN!")

#define TRY(fct) { \
      err = (fct);                                                        \
      if (err) {                                                        \
         pr_err("%s returned in %s line %d\n", hdmi_tx_err_string(err), __func__, __LINE__); \
	 goto TRY_DONE;                                                 \
      }                                                                 \
   }

typedef void (*cec_callback_t) (struct work_struct *dummy);

typedef struct {
	/* module params */
	struct {
		int verbose;
		int major;
		int minor;
	} param;
	/* driver */
	struct {
		struct class *class;
		struct device *dev;
		int devno;
		struct i2c_client *i2c_client;
		struct semaphore sem;
		int user_counter;
		int minor;
		wait_queue_head_t wait;
		bool poll_done;
#ifndef IRQ
		struct timer_list no_irq_timer;
#endif
		struct timer_list hdcp_check;
		cec_callback_t cec_callback;
		bool omap_dss_hdmi_panel;
		int gpio;
	} driver;
	/* HDMI */
	struct {
		int instance;
		tda_version version;
		tda_setup setup;
		tda_power power;
		tmdlHdmiTxHotPlug_t hot_plug_detect;
		bool rx_device_active;
		tda_video_format video_fmt;
		tda_set_in_out setio;
		bool audio_mute;
		tda_video_infoframe video_infoframe;
		tda_audio_infoframe audio_infoframe;
		tda_acp acp;
		tda_gcp gcp;
		tda_isrc1 isrc1;
		tda_isrc2 isrc2;
		tda_gammut gammut;
		tda_mps_infoframe mps_infoframe;
		tda_spd_infoframe spd_infoframe;
		tda_vs_infoframe vs_infoframe;
		tda_edid edid;
		tda_edid_dtd edid_dtd;
		tda_edid_md edid_md;
		tda_edid_audio_caps edid_audio_caps;
		tda_edid_video_caps edid_video_caps;
		tda_edid_video_timings edid_video_timings;
		tda_edid_tv_aspect_ratio edid_tv_aspect_ratio;
#ifdef TMFL_TDA19989
		tda_edid_latency edid_latency;
#endif
		unsigned short src_address;
		unsigned char raw_edid[EDID_BLOCK_COUNT * EDID_BLOCK_SIZE];
		tda_capabilities capabilities;
		tda_event event;
		tda_hdcp_status hdcp_status;
		bool hdcp_enable;
#if defined(TMFL_TDA19989) || defined(TMFL_TDA9984)
		tda_hdcp_fail hdcp_fail;
#endif
		unsigned char hdcp_raw_status;
	} tda;
} tda_instance;

#endif				/* __tx_h__ */

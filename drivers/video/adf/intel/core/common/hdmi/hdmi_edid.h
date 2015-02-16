/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/
#ifndef HDMI_EDID_H_
#define HDMI_EDID_H_

#define EDID_LENGTH 128
#define DDC_ADDR 0x50
#define DDC_SEGMENT_ADDR 0x30

#define DEFINE_MODE(nm, t, c, hd, hss, hse, ht, hsk, vd, vss, vse, vt, vs, f) \
	.name = nm, .type = (t), .clock = (c), \
	.hdisplay = (hd), .hsync_start = (hss), .hsync_end = (hse), \
	.htotal = (ht), .hskew = (hsk), .vdisplay = (vd), \
	.vsync_start = (vss), .vsync_end = (vse), .vtotal = (vt), \
	.vscan = (vs), .flags = (f), .vrefresh = 0

struct est_timings {
	u8 t1;
	u8 t2;
	u8 mfg_rsvd;
} __packed;

struct std_timing {
	u8 hsize; /* need to multiply by 8 then add 248 */
	u8 vfreq_aspect;
} __packed;

/* If detailed data is pixel timing */
struct detailed_pixel_timing {
	u8 hactive_lo;
	u8 hblank_lo;
	u8 hactive_hblank_hi;
	u8 vactive_lo;
	u8 vblank_lo;
	u8 vactive_vblank_hi;
	u8 hsync_offset_lo;
	u8 hsync_pulse_width_lo;
	u8 vsync_offset_pulse_width_lo;
	u8 hsync_vsync_offset_pulse_width_hi;
	u8 width_mm_lo;
	u8 height_mm_lo;
	u8 width_height_mm_hi;
	u8 hborder;
	u8 vborder;
	u8 misc;
} __packed;

/* If it's not pixel timing, it'll be one of the below */
struct detailed_data_string {
	u8 str[13];
} __packed;

struct detailed_data_monitor_range {
	u8 min_vfreq;
	u8 max_vfreq;
	u8 min_hfreq_khz;
	u8 max_hfreq_khz;
	u8 pixel_clock_mhz; /* need to multiply by 10 */
	u8 flags;
	union {
		struct {
			u8 reserved;
			u8 hfreq_start_khz; /* need to multiply by 2 */
			u8 c; /* need to divide by 2 */
			__le16 m;
			u8 k;
			u8 j; /* need to divide by 2 */
		} __packed gtf2;
		struct {
			u8 version;
			u8 data1; /* high 6 bits: extra clock resolution */
			u8 data2; /* plus low 2 of above: max hactive */
			u8 supported_aspects;
			u8 flags; /* preferred aspect and blanking support */
			u8 supported_scalings;
			u8 preferred_refresh;
		} __packed cvt;
	} formula;
} __packed;

struct detailed_data_wpindex {
	u8 white_yx_lo; /* Lower 2 bits each */
	u8 white_x_hi;
	u8 white_y_hi;
	u8 gamma; /* need to divide by 100 then add 1 */
} __packed;

struct cvt_timing {
	u8 code[3];
} __packed;

struct detailed_non_pixel {
	u8 pad1;
	u8 type; /* ff=serial, fe=string, fd=monitor range, fc=monitor name
		    fb=color point data, fa=standard timing data,
		    f9=undefined, f8=mfg. reserved */
	u8 pad2;
	union {
		struct detailed_data_string str;
		struct detailed_data_monitor_range range;
		struct detailed_data_wpindex color;
		struct std_timing timings[6];
		struct cvt_timing cvt[4];
	} data;
} __packed;

struct detailed_timing {
	__le16 pixel_clock; /* need to multiply by 10 KHz */
	union {
		struct detailed_pixel_timing pixel_data;
		struct detailed_non_pixel other_data;
	} data;
} __packed;

struct edid {
	u8 header[8];
	/* Vendor & product info */
	u8 mfg_id[2];
	u8 prod_code[2];
	u32 serial; /* FIXME: byte order */
	u8 mfg_week;
	u8 mfg_year;
	/* EDID version */
	u8 version;
	u8 revision;
	/* Display info: */
	u8 input;
	u8 width_cm;
	u8 height_cm;
	u8 gamma;
	u8 features;
	/* Color characteristics */
	u8 red_green_lo;
	u8 black_white_lo;
	u8 red_x;
	u8 red_y;
	u8 green_x;
	u8 green_y;
	u8 blue_x;
	u8 blue_y;
	u8 white_x;
	u8 white_y;
	/* Est. timings and mfg rsvd timings*/
	struct est_timings established_timings;
	/* Standard timings 1-8*/
	struct std_timing standard_timings[8];
	/* Detailing timings 1-4 */
	struct detailed_timing detailed_timings[4];
	/* Number of 128 byte ext. blocks */
	u8 extensions;
	/* Checksum */
	u8 checksum;
} __packed;

#define HDMI_IDENTIFIER 0x000C03
#define AUDIO_BLOCK	0x01
#define VIDEO_BLOCK     0x02
#define VENDOR_BLOCK    0x03
#define SPEAKER_BLOCK	0x04
#define VIDEO_CAPABILITY_BLOCK	0x07
#define EDID_BASIC_AUDIO	(1 << 6)
#define EDID_CEA_YCRCB444	(1 << 5)
#define EDID_CEA_YCRCB422	(1 << 4)
#define EDID_CEA_VCDB_QS	(1 << 6)

#define CEA_EXT	    0x02
#define VTB_EXT	    0x10
#define DI_EXT	    0x40
#define LS_EXT	    0x50
#define MI_EXT	    0x60

#define LEVEL_DMT	0
#define LEVEL_GTF	1
#define LEVEL_GTF2	2
#define LEVEL_CVT	3

/*
 * EDID blocks out in the wild have a variety of bugs, try to collect
 * them here (note that userspace may work around broken monitors first,
 * but fixes should make their way here so that the kernel "just works"
 * on as many displays as possible).
 */
/* First detailed mode wrong, use largest 60Hz mode */
#define EDID_QUIRK_PREFER_LARGE_60		(1 << 0)
/* Reported 135MHz pixel clock is too high, needs adjustment */
#define EDID_QUIRK_135_CLOCK_TOO_HIGH		(1 << 1)
/* Prefer the largest mode at 75 Hz */
#define EDID_QUIRK_PREFER_LARGE_75		(1 << 2)
/* Detail timing is in cm not mm */
#define EDID_QUIRK_DETAILED_IN_CM		(1 << 3)
/* Detailed timing descriptors have bogus size values, so just take the
 * maximum size and use that.
 */
#define EDID_QUIRK_DETAILED_USE_MAXIMUM_SIZE	(1 << 4)
/* Monitor forgot to set the first detailed is preferred bit. */
#define EDID_QUIRK_FIRST_DETAILED_PREFERRED	(1 << 5)
/* use +hsync +vsync for detailed mode */
#define EDID_QUIRK_DETAILED_SYNC_PP		(1 << 6)
/* Force reduced-blanking timings for detailed modes */
#define EDID_QUIRK_FORCE_REDUCED_BLANKING	(1 << 7)

#define EDID_DETAIL_EST_TIMINGS 0xf7
#define EDID_DETAIL_CVT_3BYTE 0xf8
#define EDID_DETAIL_COLOR_MGMT_DATA 0xf9
#define EDID_DETAIL_STD_MODES 0xfa
#define EDID_DETAIL_MONITOR_CPDATA 0xfb
#define EDID_DETAIL_MONITOR_NAME 0xfc
#define EDID_DETAIL_MONITOR_RANGE 0xfd
#define EDID_DETAIL_MONITOR_STRING 0xfe
#define EDID_DETAIL_MONITOR_SERIAL 0xff

/* 00=16:10, 01=4:3, 10=5:4, 11=16:9 */
#define EDID_TIMING_ASPECT_SHIFT 6
#define EDID_TIMING_ASPECT_MASK  (0x3 << EDID_TIMING_ASPECT_SHIFT)

/* need to add 60 */
#define EDID_TIMING_VFREQ_SHIFT  0
#define EDID_TIMING_VFREQ_MASK   (0x3f << EDID_TIMING_VFREQ_SHIFT)

typedef void detailed_cb(struct detailed_timing *timing, void *closure);

#define version_greater(edid, maj, min) \
	(((edid)->version > (maj)) || \
	 ((edid)->version == (maj) && (edid)->revision > (min)))

#define EDID_EST_TIMINGS 16
#define EDID_STD_TIMINGS 8
#define EDID_DETAILED_TIMINGS 4

#define DRM_EDID_INPUT_SERRATION_VSYNC (1 << 0)
#define DRM_EDID_INPUT_SYNC_ON_GREEN   (1 << 1)
#define DRM_EDID_INPUT_COMPOSITE_SYNC  (1 << 2)
#define DRM_EDID_INPUT_SEPARATE_SYNCS  (1 << 3)
#define DRM_EDID_INPUT_BLANK_TO_BLACK  (1 << 4)
#define DRM_EDID_INPUT_VIDEO_LEVEL     (3 << 5)
#define DRM_EDID_INPUT_DIGITAL         (1 << 7)
#define DRM_EDID_DIGITAL_DEPTH_MASK    (7 << 4)
#define DRM_EDID_DIGITAL_DEPTH_UNDEF   (0 << 4)
#define DRM_EDID_DIGITAL_DEPTH_6       (1 << 4)
#define DRM_EDID_DIGITAL_DEPTH_8       (2 << 4)
#define DRM_EDID_DIGITAL_DEPTH_10      (3 << 4)
#define DRM_EDID_DIGITAL_DEPTH_12      (4 << 4)
#define DRM_EDID_DIGITAL_DEPTH_14      (5 << 4)
#define DRM_EDID_DIGITAL_DEPTH_16      (6 << 4)
#define DRM_EDID_DIGITAL_DEPTH_RSVD    (7 << 4)
#define DRM_EDID_DIGITAL_TYPE_UNDEF    (0)
#define DRM_EDID_DIGITAL_TYPE_DVI      (1)
#define DRM_EDID_DIGITAL_TYPE_HDMI_A   (2)
#define DRM_EDID_DIGITAL_TYPE_HDMI_B   (3)
#define DRM_EDID_DIGITAL_TYPE_MDDI     (4)
#define DRM_EDID_DIGITAL_TYPE_DP       (5)

#define DRM_EDID_FEATURE_DEFAULT_GTF      (1 << 0)
#define DRM_EDID_FEATURE_PREFERRED_TIMING (1 << 1)
#define DRM_EDID_FEATURE_STANDARD_COLOR   (1 << 2)
/* If analog */
/* 00=mono, 01=rgb, 10=non-rgb, 11=unknown */
#define DRM_EDID_FEATURE_DISPLAY_TYPE     (3 << 3)
/* If digital */
#define DRM_EDID_FEATURE_COLOR_MASK	  (3 << 3)
#define DRM_EDID_FEATURE_RGB		  (0 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB444	  (1 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB422	  (2 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB	  (3 << 3) /* both 4:4:4 and 4:2:2 */

#define DRM_EDID_FEATURE_PM_ACTIVE_OFF    (1 << 5)
#define DRM_EDID_FEATURE_PM_SUSPEND       (1 << 6)
#define DRM_EDID_FEATURE_PM_STANDBY       (1 << 7)

#define DRM_EDID_PT_HSYNC_POSITIVE (1 << 1)
#define DRM_EDID_PT_VSYNC_POSITIVE (1 << 2)
#define DRM_EDID_PT_SEPARATE_SYNC  (3 << 3)
#define DRM_EDID_PT_STEREO         (1 << 5)
#define DRM_EDID_PT_INTERLACED     (1 << 7)

#define DRM_MODE_TYPE_BUILTIN	(1<<0)
#define DRM_MODE_TYPE_CLOCK_C	((1<<1) | DRM_MODE_TYPE_BUILTIN)
#define DRM_MODE_TYPE_CRTC_C	((1<<2) | DRM_MODE_TYPE_BUILTIN)
#define DRM_MODE_TYPE_PREFERRED	(1<<3)
#define DRM_MODE_TYPE_DEFAULT	(1<<4)
#define DRM_MODE_TYPE_USERDEF	(1<<5)
#define DRM_MODE_TYPE_DRIVER	(1<<6)

/* Video mode flags */
/* bit compatible with the xorg definitions. */
#define DRM_MODE_FLAG_PHSYNC	(1<<0)
#define DRM_MODE_FLAG_NHSYNC	(1<<1)
#define DRM_MODE_FLAG_PVSYNC	(1<<2)
#define DRM_MODE_FLAG_NVSYNC	(1<<3)
#define DRM_MODE_FLAG_INTERLACE	(1<<4)
#define DRM_MODE_FLAG_DBLSCAN	(1<<5)
#define DRM_MODE_FLAG_CSYNC	(1<<6)
#define DRM_MODE_FLAG_PCSYNC	(1<<7)
#define DRM_MODE_FLAG_NCSYNC	(1<<8)
#define DRM_MODE_FLAG_HSKEW	(1<<9) /* hskew provided */
#define DRM_MODE_FLAG_BCAST	(1<<10)
#define DRM_MODE_FLAG_PIXMUX	(1<<11)
#define DRM_MODE_FLAG_DBLCLK	(1<<12)
#define DRM_MODE_FLAG_CLKDIV2	(1<<13)

struct detailed_mode_closure {
	struct hdmi_monitor *monitor;
	struct edid *edid;
	bool preferred;
	u32 quirks;
	int modes;
};

struct edid *get_edid(struct i2c_adapter *adapter);
int probe_ddc_edid(struct i2c_adapter *adapter, unsigned char *buf,
					int block, int len);
bool detect_hdmi_monitor(struct edid *edid);
void parse_edid(struct hdmi_monitor *monitor, struct edid *edid);
bool edid_is_valid(struct edid *edid);
int mode_vrefresh(const struct drm_mode_modeinfo *mode);
int hdmi_avi_infoframe_from_mode(struct hdmi_monitor *monitor,
					const struct drm_mode_modeinfo *mode);
/* Added for HDMI audio */
bool detect_monitor_audio(struct edid *edid);
u8 match_cea_mode(const struct drm_mode_modeinfo *to_match);

#endif /* HDMI_EDID_H_ */

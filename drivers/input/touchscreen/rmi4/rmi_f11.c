/*
 * Copyright (c) 2011,2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 * Copyright (C) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define FUNCTION_DATA f11_data
#define FUNCTION_NUMBER 0x11

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/kconfig.h>
#include <linux/rmi.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "rmi_driver.h"

#define F11_MAX_NUM_OF_SENSORS		8
#define F11_MAX_NUM_OF_FINGERS		10
#define F11_MAX_NUM_OF_TOUCH_SHAPES	16

#define F11_REL_POS_MIN		-128
#define F11_REL_POS_MAX		127

#define FINGER_STATE_MASK	0x03
#define GET_FINGER_STATE(f_states, i) \
	((f_states[i / 4] >> (2 * (i % 4))) & FINGER_STATE_MASK)

#define F11_CTRL_SENSOR_MAX_X_POS_OFFSET	6
#define F11_CTRL_SENSOR_MAX_Y_POS_OFFSET	8

#define DEFAULT_XY_MAX 9999
#define DEFAULT_MAX_ABS_MT_PRESSURE 255
#define DEFAULT_MAX_ABS_MT_TOUCH 15
#define DEFAULT_MAX_ABS_MT_ORIENTATION 1
#define DEFAULT_MIN_ABS_MT_TRACKING_ID 1
#define DEFAULT_MAX_ABS_MT_TRACKING_ID 10
#define NAME_BUFFER_SIZE 256

/*
 *  F11_INPUT_SOURCE_SENSOR is default mode of operation.
 *  F11_INPUT_SOURCE_USER_APP is used when sensor images are being
 *  sent over to the host and the host has a user space app that
 *  processes the images and generates finger events
 */
#define F11_INPUT_SOURCE_SENSOR		0
#define F11_INPUT_SOURCE_USER_APP	1

/* character device name for fast image transfer (if enabled) */
/*
** the device_register will add another digit to this, making it "rawsensor0X"
*/
#define RAW_FINGER_DATA_CHAR_DEVICE_NAME "rawtouch0"


static ssize_t f11_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf);

static ssize_t f11_mode_store(struct device *dev,
					 struct device_attribute *attr,
	                      const char *buf,
	                      size_t count);

/** A note about RMI4 F11 register structure.
 *
 *  There may be one or more individual 2D touch surfaces associated with an
 * instance for F11.  For example, a handheld device might have a touchscreen
 * display on the front, and a touchpad on the back.  F11 represents these touch
 * surfaces as individual sensors, up to 7 on a given RMI4 device.
 *
 * The properties for
 * a given sensor are described by its query registers.  The number of query
 * registers and the layout of their contents are described by the F11 device
 * queries as well as the per-sensor query information.  The query registers
 * for sensor[n+1] immediately follow those for sensor[n], so the start address
 * of the sensor[n+1] queries can only be computed if you know the size of the
 * sensor[n] queries.  Because each of the sensors may have different
 * properties, the size of the query registers for each sensor must be
 * calculated on a sensor by sensor basis.
 *
 * Similarly, each sensor has control registers that govern its behavior.  The
 * size and layout of the control registers for a given sensor can be determined
 * by parsing that sensors query registers.  The control registers for
 * sensor[n+1] immediately follow those for sensor[n], so you can only know
 * the start address for the sensor[n+1] controls if you know the size (and
 * location) of the sensor[n] controls.
 *
 * And in a likewise fashion, each sensor has data registers where it reports
 * its touch data and other interesting stuff.  The size and layout of a
 * sensors data registers must be determined by parsing its query registers.
 * The data registers for sensor[n+1] immediately follow those for sensor[n],
 * so you can only know the start address for the sensor[n+1] controls if you
 * know the size (and location) of the sensor[n] controls.
 *
 * The short story is that we need to read and parse a lot of query
 * registers in order to determine the attributes of a sensor[0].  Then
 * we need to use that data to compute the size of the control and data
 * registers for sensor[0].  Once we have that figured out, we can then do
 * the same thing for each subsequent sensor.
 *
 * The end result is that we have a number of structs that aren't used to
 * directly generate the input events, but their size, location and contents
 * are critical to determining where the data we are interested in lives.
 *
 * At this time, the driver does not yet comprehend all possible F11
 * configuration options, but it should be sufficient to cover 99% of RMI4 F11
 * devices currently in the field.
 */

/**
 * @rezero - writing 1 to this will cause the sensor to calibrate to the
 * current capacitive state.
 */
struct f11_2d_commands {
		u8 rezero:1;
	u8 reserved:7;
} __attribute__((__packed__));

/** This query is always present, and is on a per device basis.  All other
 * queries are on a per-sensor basis.
 *
 * @nbr_of_sensors - the number of 2D sensors on the touch device.
 * @has_query9 - indicates the F11_2D_Query9 register exists.
 * @has_query11 - indicates the F11_2D_Query11 register exists.
 * @has_query12 - indicates the F11_2D_Query12 register exists.
 */
struct f11_2d_device_query {
			u8 nbr_of_sensors:3;
			u8 has_query9:1;
			u8 has_query11:1;
	u8 has_query12:1;
	u8 has_query27:1;
	u8 has_query28:1;
} __attribute__((__packed__));

/** Query registers 1 through 4 are always present.
 * @number_of_fingers - describes the maximum number of fingers the 2-D sensor
 * supports.
 * @has_rel - the sensor supports relative motion reporting.
 * @has_abs - the sensor supports absolute poition reporting.
 * @has_gestures - the sensor supports gesture reporting.
 * @has_sensitivity_adjust - the sensor supports a global sensitivity
 * adjustment.
 * @configurable - the sensor supports various configuration options.
 * @num_of_x_electrodes -  the maximum number of electrodes the 2-D sensor
 * supports on the X axis.
 * @num_of_y_electrodes -  the maximum number of electrodes the 2-D sensor
 * supports on the Y axis.
 * @max_electrodes - the total number of X and Y electrodes that may be
 * configured.
 */
struct f11_2d_sensor_info {
			/* query1 */
			u8 number_of_fingers:3;
			u8 has_rel:1;
			u8 has_abs:1;
			u8 has_gestures:1;
			u8 has_sensitivity_adjust:1;
			u8 configurable:1;
			/* query2 */
			u8 num_of_x_electrodes:7;
	u8 reserved_1:1;
			/* query3 */
			u8 num_of_y_electrodes:7;
	u8 reserved_2:1;
			/* query4 */
			u8 max_electrodes:7;
	u8 reserved_3:1;
} __attribute__((__packed__));

/** Query 5 - this is present if the has_abs bit is set.
 *
 * @abs_data_size - describes the format of data reported by the absolute
 * data source.  Only one format (the kind used here) is supported at this
 * time.
 * @has_anchored_finger - then the sensor supports the high-precision second
 * finger tracking provided by the manual tracking and motion sensitivity
 * options.
 * @has_adjust_hyst - the difference between the finger release threshold and
 * the touch threshold.
 * @has_dribble - the sensor supports the generation of dribble interrupts,
 * which may be enabled or disabled with the dribble control bit.
 * @has_bending_correction - Bending related data registers 28 and 36, and
 * control register 52..57 are present.
 * @has_large_object_suppression - control register 58 and data register 28
 * exist.
 * @has_jitter_filter - query 13 and control 73..76 exist.
 */
struct f11_2d_abs_info {
	u8 abs_data_size:2;
			u8 has_anchored_finger:1;
			u8 has_adj_hyst:1;
			u8 has_dribble:1;
	u8 has_bending_correction:1;
	u8 has_large_object_suppression:1;
	u8 has_jitter_filter:1;
} __attribute__((__packed__));

/** Gesture information queries 7 and 8 are present if has_gestures bit is set.
 *
 * @has_single_tap - a basic single-tap gesture is supported.
 * @has_tap_n_hold - tap-and-hold gesture is supported.
 * @has_double_tap - double-tap gesture is supported.
 * @has_early_tap - early tap is supported and reported as soon as the finger
 * lifts for any tap event that could be interpreted as either a single tap
 * or as the first tap of a double-tap or tap-and-hold gesture.
 * @has_flick - flick detection is supported.
 * @has_press - press gesture reporting is supported.
 * @has_pinch - pinch gesture detection is supported.
 * @has_palm_det - the 2-D sensor notifies the host whenever a large conductive
 * object such as a palm or a cheek touches the 2-D sensor.
 * @has_rotate - rotation gesture detection is supported.
 * @has_touch_shapes - TouchShapes are supported.  A TouchShape is a fixed
 * rectangular area on the sensor that behaves like a capacitive button.
 * @has_scroll_zones - scrolling areas near the sensor edges are supported.
 * @has_individual_scroll_zones - if 1, then 4 scroll zones are supported;
 * if 0, then only two are supported.
 * @has_multi_finger_scroll - the multifinger_scrolling bit will be set when
 * more than one finger is involved in a scrolling action.
 */
struct f11_2d_gesture_info {
			u8 has_single_tap:1;
			u8 has_tap_n_hold:1;
			u8 has_double_tap:1;
			u8 has_early_tap:1;
			u8 has_flick:1;
			u8 has_press:1;
			u8 has_pinch:1;
	u8 has_chiral:1;

			u8 has_palm_det:1;
			u8 has_rotate:1;
			u8 has_touch_shapes:1;
			u8 has_scroll_zones:1;
			u8 has_individual_scroll_zones:1;
			u8 has_multi_finger_scroll:1;
	u8 has_mf_edge_motion:1;
	u8 has_mf_scroll_inertia:1;
} __attribute__((__packed__));

/** Utility for checking bytes in the gesture info registers.  This is done
 * often enough that we put it here to declutter the conditionals.
 */
static bool has_gesture_bits(const struct f11_2d_gesture_info *info,
			     const u8 byte) {
	return ((u8 *) info)[byte] != 0;
}

/**
 * @has_pen - detection of a stylus is supported and registers F11_2D_Ctrl20
 * and F11_2D_Ctrl21 exist.
 * @has_proximity - detection of fingers near the sensor is supported and
 * registers F11_2D_Ctrl22 through F11_2D_Ctrl26 exist.
 * @has_palm_det_sensitivity -  the sensor supports the palm detect sensitivity
 * feature and register F11_2D_Ctrl27 exists.
 * @has_two_pen_thresholds - is has_pen is also set, then F11_2D_Ctrl35 exists.
 * @has_contact_geometry - the sensor supports the use of contact geometry to
 * map absolute X and Y target positions and registers F11_2D_Data18.* through
 * F11_2D_Data27 exist.
 */
struct f11_2d_query9 {
	u8 has_pen:1;
	u8 has_proximity:1;
	u8 has_palm_det_sensitivity:1;
	u8 has_suppress_on_palm_detect:1;
	u8 has_two_pen_thresholds:1;
	u8 has_contact_geometry:1;
	u8 has_pen_hover_discrimination:1;
	u8 has_pen_filters:1;
} __attribute__((__packed__));

/** Touch shape info (query 10) is present if has_touch_shapes is set.
 *
 * @nbr_touch_shapes - the total number of touch shapes supported.
 */
struct f11_2d_ts_info {
			u8 nbr_touch_shapes:5;
	u8 reserved:3;
} __attribute__((__packed__));

/** Query 11 is present if the has_query11 bit is set in query 0.
 *
 * @has_z_tuning - if set, the sensor supports Z tuning and registers
 * F11_2D_Ctrl29 through F11_2D_Ctrl33 exist.
 * @has_algorithm_selection - controls choice of noise suppression algorithm
 * @has_w_tuning - the sensor supports Wx and Wy scaling and registers
 * F11_2D_Ctrl36 through F11_2D_Ctrl39 exist.
 * @has_pitch_info - the X and Y pitches of the sensor electrodes can be
 * configured and registers F11_2D_Ctrl40 and F11_2D_Ctrl41 exist.
 * @has_finger_size -  the default finger width settings for the
 * sensor can be configured and registers F11_2D_Ctrl42 through F11_2D_Ctrl44
 * exist.
 * @has_segmentation_aggressiveness - the sensor’s ability to distinguish
 * multiple objects close together can be configured and register F11_2D_Ctrl45
 * exists.
 * @has_XY_clip -  the inactive outside borders of the sensor can be
 * configured and registers F11_2D_Ctrl46 through F11_2D_Ctrl49 exist.
 * @has_drumming_filter - the sensor can be configured to distinguish
 * between a fast flick and a quick drumming movement and registers
 * F11_2D_Ctrl50 and F11_2D_Ctrl51 exist.
 */
struct f11_2d_query11 {
	u8 has_z_tuning:1;
	u8 has_algorithm_selection:1;
	u8 has_w_tuning:1;
	u8 has_pitch_info:1;
	u8 has_finger_size:1;
	u8 has_segmentation_aggressiveness:1;
	u8 has_XY_clip:1;
	u8 has_drumming_filter:1;
} __attribute__((__packed__));

/**
 * @has_gapless_finger - control registers relating to gapless finger are
 * present.
 * @has_gapless_finger_tuning - additional control and data registers relating
 * to gapless finger are present.
 * @has_8bit_w - larger W value reporting is supported.
 * @has_adjustable_mapping - TBD
 * @has_info2 - the general info query14 is present
 * @has_physical_props - additional queries describing the physical properties
 * of the sensor are present.
 * @has_finger_limit - indicates that F11 Ctrl 80 exists.
 * @has_linear_coeff - indicates that F11 Ctrl 81 exists.
 */
struct f11_2d_query12 {
	u8 has_gapless_finger:1;
	u8 has_gapless_finger_tuning:1;
	u8 has_8bit_w:1;
	u8 has_adjustable_mapping:1;
	u8 has_info2:1;
	u8 has_physical_props:1;
	u8 has_finger_limit:1;
	u8 has_linear_coeff_2:1;
} __attribute__((__packed__));

/** This register is present if Query 5's has_jitter_filter bit is set.
 * @jitter_window_size - used by Design Studio 4.
 * @jitter_filter_type - used by Design Studio 4.
 */
struct f11_2d_query13 {
	u8 jtter_window_size:5;
	u8 jitter_filter_type:2;
	u8 reserved:1;
} __attribute__((__packed__));

/** This register is present if query 12's has_general_info2 flag is set.
 *
 * @light_control - Indicates what light/led control features are present, if
 * any.
 * @is_clear - if set, this is a clear sensor (indicating direct pointing
 * application), otherwise it's opaque (indicating indirect pointing).
 * @clickpad_props - specifies if this is a clickpad, and if so what sort of
 * mechanism it uses
 * @mouse_buttons - specifies the number of mouse buttons present (if any).
 * @has_advanced_gestures - advanced driver gestures are supported.
 */
struct f11_2d_query14 {
	u8 light_control:2;
	u8 is_clear:1;
	u8 clickpad_props:2;
	u8 mouse_buttons:2;
	u8 has_advanced_gestures:1;
} __attribute__((__packed__));

#define F11_LIGHT_CTL_NONE 0x00
#define F11_LUXPAD	   0x01
#define F11_DUAL_MODE      0x02

#define F11_NOT_CLICKPAD     0x00
#define F11_HINGED_CLICKPAD  0x01
#define F11_UNIFORM_CLICKPAD 0x02

/** See notes above for information about specific query register sets.
 */
struct f11_2d_sensor_queries {
	struct f11_2d_sensor_info info;

	struct f11_2d_abs_info abs_info;

	u8 f11_2d_query6;

	struct f11_2d_gesture_info gesture_info;

	struct f11_2d_query9 query9;

	struct f11_2d_ts_info ts_info;

	struct f11_2d_query11 features_1;

	struct f11_2d_query12 features_2;

	struct f11_2d_query13 jitter_filter;

	struct f11_2d_query14 info_2;
};

/**
 * @reporting_mode - controls how often finger position data is reported.
 * @abs_pos_filt - when set, enables various noise and jitter filtering
 * algorithms for absolute reports.
 * @rel_pos_filt - when set, enables various noise and jitter filtering
 * algorithms for relative reports.
 * @rel_ballistics - enables ballistics processing for the relative finger
 * motion on the 2-D sensor.
 * @dribble - enables the dribbling feature.
 * @report_beyond_clip - when this is set, fingers outside the active area
 * specified by the x_clip and y_clip registers will be reported, but with
 * reported finger position clipped to the edge of the active area.
 * @palm_detect_thresh - the threshold at which a wide finger is considered a
 * palm. A value of 0 inhibits palm detection.
 * @motion_sensitivity - specifies the threshold an anchored finger must move
 * before it is considered no longer anchored.  High values mean more
 * sensitivity.
 * @man_track_en - for anchored finger tracking, whether the host (1) or the
 * device (0) determines which finger is the tracked finger.
 * @man_tracked_finger - when man_track_en is 1, specifies whether finger 0 or
 * finger 1 is the tracked finger.
 * @delta_x_threshold - 2-D position update interrupts are inhibited unless
 * the finger moves more than a certain threshold distance along the X axis.
 * @delta_y_threshold - 2-D position update interrupts are inhibited unless
 * the finger moves more than a certain threshold distance along the Y axis.
 * @velocity - When rel_ballistics is set, this register defines the
 * velocity ballistic parameter applied to all relative motion events.
 * @acceleration - When rel_ballistics is set, this register defines the
 * acceleration ballistic parameter applied to all relative motion events.
 * @sensor_max_x_pos - the maximum X coordinate reported by the sensor.
 * @sensor_max_y_pos - the maximum Y coordinate reported by the sensor.
 */
struct f11_2d_ctrl0_9 {
	/* F11_2D_Ctrl0 */
	u8 reporting_mode:3;
	u8 abs_pos_filt:1;
	u8 rel_pos_filt:1;
	u8 rel_ballistics:1;
	u8 dribble:1;
	u8 report_beyond_clip:1;
	/* F11_2D_Ctrl1 */
	u8 palm_detect_thres:4;
	u8 motion_sensitivity:2;
	u8 man_track_en:1;
	u8 man_tracked_finger:1;
	/* F11_2D_Ctrl2 and 3 */
	u8 delta_x_threshold:8;
	u8 delta_y_threshold:8;
	/* F11_2D_Ctrl4 and 5 */
	u8 velocity:8;
	u8 acceleration:8;
	/* F11_2D_Ctrl6 thru 9 */
	u16 sensor_max_x_pos:12;
	u8 ctrl7_reserved:4;
	u16 sensor_max_y_pos:12;
	u8 ctrl9_reserved:4;
} __attribute__((__packed__));

/**
 * @single_tap_int_enable - enable tap gesture recognition.
 * @tap_n_hold_int_enable - enable tap-and-hold gesture recognition.
 * @double_tap_int_enable - enable double-tap gesture recognition.
 * @early_tap_int_enable - enable early tap notification.
 * @flick_int_enable - enable flick detection.
 * @press_int_enable - enable press gesture recognition.
 * @pinch_int_enable - enable pinch detection.
 */
struct f11_2d_ctrl10 {
	u8 single_tap_int_enable:1;
	u8 tap_n_hold_int_enable:1;
	u8 double_tap_int_enable:1;
	u8 early_tap_int_enable:1;
	u8 flick_int_enable:1;
	u8 press_int_enable:1;
	u8 pinch_int_enable:1;
	u8 reserved:1;
} __attribute__((__packed__));

/**
 * @palm_detect_int_enable - enable palm detection feature.
 * @rotate_int_enable - enable rotate gesture detection.
 * @touch_shape_int_enable - enable the TouchShape feature.
 * @scroll_zone_int_enable - enable scroll zone reporting.
 * @multi_finger_scroll_int_enable - enable the multfinger scroll feature.
 */
struct f11_2d_ctrl11 {
	u8 palm_detect_int_enable:1;
	u8 rotate_int_enable:1;
	u8 touch_shape_int_enable:1;
	u8 scroll_zone_int_enable:1;
	u8 multi_finger_scroll_int_enable:1;
	u8 reserved:3;
} __attribute__((__packed__));

/**
 * @sens_adjustment - allows a host to alter the overall sensitivity of a
 * 2-D sensor. A positive value in this register will make the sensor more
 * sensitive than the factory defaults, and a negative value will make it
 * less sensitive.
 * @hyst_adjustment - increase the touch/no-touch hysteresis by 2 Z-units for
 * each one unit increment in this setting.
 */
struct f11_2d_ctrl14 {
	s8 sens_adjustment:5;
	u8 hyst_adjustment:3;
} __attribute__((__packed__));

/**
 * @max_tap_time - the maximum duration of a tap, in 10-millisecond units.
 */
struct f11_2d_ctrl15 {
	u8 max_tap_time:8;
} __attribute__((__packed__));

/**
 * @min_press_time - The minimum duration required for stationary finger(s) to
 * generate a press gesture, in 10-millisecond units.
 */
struct f11_2d_ctrl16 {
	u8 min_press_time:8;
} __attribute__((__packed__));

/**
 * @max_tap_distance - Determines the maximum finger movement allowed during
 * a tap, in 0.1-millimeter units.
 */
struct f11_2d_ctrl17 {
	u8 max_tap_distance:8;
} __attribute__((__packed__));

/**
 * @min_flick_distance - the minimum finger movement for a flick gesture,
 * in 1-millimeter units.
 * @min_flick_speed - the minimum finger speed for a flick gesture, in
 * 10-millimeter/second units.
 */
struct f11_2d_ctrl18_19 {
	u8 min_flick_distance:8;
	u8 min_flick_speed:8;
} __attribute__((__packed__));

/**
 * @pen_detect_enable - enable reporting of stylus activity.
 * @pen_jitter_filter_enable - Setting this enables the stylus anti-jitter
 * filter.
 * @pen_z_threshold - This is the stylus-detection lower threshold. Smaller
 * values result in higher sensitivity.
 */
struct f11_2d_ctrl20_21 {
	u8 pen_detect_enable:1;
	u8 pen_jitter_filter_enable:1;
	u8 ctrl20_reserved:6;
	u8 pen_z_threshold:8;
} __attribute__((__packed__));

/**
 * These are not accessible through sysfs yet.
 *
 * @proximity_detect_int_en - enable proximity detection feature.
 * @proximity_jitter_filter_en - enables an anti-jitter filter on proximity
 * data.
 * @proximity_detection_z_threshold - the threshold for finger-proximity
 * detection.
 * @proximity_delta_x_threshold - In reduced-reporting modes, this is the
 * threshold for proximate-finger movement in the direction parallel to the
 * X-axis.
 * @proximity_delta_y_threshold - In reduced-reporting modes, this is the
 * threshold for proximate-finger movement in the direction parallel to the
 * Y-axis.
 * * @proximity_delta_Z_threshold - In reduced-reporting modes, this is the
 * threshold for proximate-finger movement in the direction parallel to the
 * Z-axis.
 */
struct f11_2d_ctrl22_26 {
	/* control 22 */
	u8 proximity_detect_int_en:1;
	u8 proximity_jitter_filter_en:1;
	u8 f11_2d_ctrl6_b3__7:6;

	/* control 23 */
	u8 proximity_detection_z_threshold;

	/* control 24 */
	u8 proximity_delta_x_threshold;

	/* control 25 */
	u8 proximity_delta_y_threshold;

	/* control 26 */
	u8 proximity_delta_z_threshold;
} __attribute__((__packed__));

/**
 * @palm_detecy_sensitivity - When this value is small, smaller objects will
 * be identified as palms; when this value is large, only larger objects will
 * be identified as palms. 0 represents the factory default.
 * @suppress_on_palm_detect - when set, all F11 interrupts except palm_detect
 * are suppressed while a palm is detected.
 */
struct f11_2d_ctrl27 {
	s8 palm_detect_sensitivity:4;
	u8 suppress_on_palm_detect:1;
	u8 f11_2d_ctrl27_b5__7:3;
} __attribute__((__packed__));

/**
 * @multi_finger_scroll_mode - allows choice of multi-finger scroll mode and
 * determines whether and how X or Y displacements are reported.
 * @edge_motion_en - enables the edge_motion feature.
 * @multi_finger_scroll_momentum - controls the length of time that scrolling
 * continues after fingers have been lifted.
 */
struct f11_2d_ctrl28 {
	u8 multi_finger_scroll_mode:2;
	u8 edge_motion_en:1;
	u8 f11_2d_ctrl28b_3:1;
	u8 multi_finger_scroll_momentum:4;
} __attribute__((__packed__));

/**
 * @z_touch_threshold - Specifies the finger-arrival Z threshold. Large values
 * may cause smaller fingers to be rejected.
 * @z_touch_hysteresis - Specifies the difference between the finger-arrival
 * Z threshold and the finger-departure Z threshold.
 */
struct f11_2d_ctrl29_30 {
	u8 z_touch_threshold;
	u8 z_touch_hysteresis;
} __attribute__((__packed__));


struct f11_2d_ctrl {
	struct f11_2d_ctrl0_9		 *ctrl0_9;
	u16				ctrl0_9_address;
	struct f11_2d_ctrl10		*ctrl10;
	struct f11_2d_ctrl11		*ctrl11;
	u8				ctrl12_size;
	struct f11_2d_ctrl14		*ctrl14;
	struct f11_2d_ctrl15		*ctrl15;
	struct f11_2d_ctrl16		*ctrl16;
	struct f11_2d_ctrl17		*ctrl17;
	struct f11_2d_ctrl18_19		*ctrl18_19;
	struct f11_2d_ctrl20_21		*ctrl20_21;
	struct f11_2d_ctrl22_26		*ctrl22_26;
	struct f11_2d_ctrl27		*ctrl27;
	struct f11_2d_ctrl28		*ctrl28;
	struct f11_2d_ctrl29_30		*ctrl29_30;
};

/**
 * @x_msb - top 8 bits of X finger position.
 * @y_msb - top 8 bits of Y finger position.
 * @x_lsb - bottom 4 bits of X finger position.
 * @y_lsb - bottom 4 bits of Y finger position.
 * @w_y - contact patch width along Y axis.
 * @w_x - contact patch width along X axis.
 * @z - finger Z value (proxy for pressure).
 */
struct f11_2d_data_1_5 {
	u8 x_msb;
	u8 y_msb;
	u8 x_lsb:4;
	u8 y_lsb:4;
	u8 w_y:4;
	u8 w_x:4;
	u8 z;
} __attribute__((__packed__));

/**
 * @delta_x - relative motion along X axis.
 * @delta_y - relative motion along Y axis.
 */
struct f11_2d_data_6_7 {
	s8 delta_x;
	s8 delta_y;
} __attribute__((__packed__));

/**
 * @single_tap - a single tap was recognized.
 * @tap_and_hold - a tap-and-hold gesture was recognized.
 * @double_tap - a double tap gesture was recognized.
 * @early_tap - a tap gesture might be happening.
 * @flick - a flick gesture was detected.
 * @press - a press gesture was recognized.
 * @pinch - a pinch gesture was detected.
 */
struct f11_2d_data_8 {
	u8 single_tap:1;
	u8 tap_and_hold:1;
	u8 double_tap:1;
	u8 early_tap:1;
	u8 flick:1;
	u8 press:1;
	u8 pinch:1;
} __attribute__((__packed__));

/**
 * @palm_detect - a palm or other large object is in contact with the sensor.
 * @rotate - a rotate gesture was detected.
 * @shape - a TouchShape has been activated.
 * @scrollzone - scrolling data is available.
 * @finger_count - number of fingers involved in the reported gesture.
 */
struct f11_2d_data_9 {
	u8 palm_detect:1;
	u8 rotate:1;
	u8 shape:1;
	u8 scrollzone:1;
	u8 finger_count:3;
} __attribute__((__packed__));

/**
 * @pinch_motion - when a pinch gesture is detected, this is the change in
 * distance between the two fingers since this register was last read.
 */
struct f11_2d_data_10 {
	s8 pinch_motion;
} __attribute__((__packed__));

/**
 * @x_flick_dist - when a flick gesture is detected,  the distance of flick
 * gesture in X direction.
 * @y_flick_dist - when a flick gesture is detected,  the distance of flick
 * gesture in Y direction.
 * @flick_time - the total time of the flick gesture, in 10ms units.
 */
struct f11_2d_data_10_12 {
	s8 x_flick_dist;
	s8 y_flick_dist;
	u8 flick_time;
} __attribute__((__packed__));

/**
 * @motion - when a rotate gesture is detected, the accumulated distance
 * of the rotate motion. Clockwise motion is positive and counterclockwise
 * motion is negative.
 * @finger_separation - when a rotate gesture is detected, the distance
 * between the fingers.
 */
struct f11_2d_data_11_12 {
	s8 motion;
	u8 finger_separation;
} __attribute__((__packed__));

/**
 * @shape_n - a bitmask of the currently activate TouchShapes (if any).
 */
struct f11_2d_data_13 {
	u8 shape_n;
} __attribute__((__packed__));

/**
 * @horizontal - chiral scrolling distance in the X direction.
 * @vertical - chiral scrolling distance in the Y direction.
 */
struct f11_2d_data_14_15 {
	s8 horizontal;
	s8 vertical;
} __attribute__((__packed__));

/**
 * @x_low - scroll zone motion along the lower edge of the sensor.
 * @y_right - scroll zone motion along the right edge of the sensor.
 * @x_upper - scroll zone motion along the upper edge of the sensor.
 * @y_left - scroll zone motion along the left edge of the sensor.
 */
struct f11_2d_data_14_17 {
	s8 x_low;
	s8 y_right;
	s8 x_upper;
	s8 y_left;
} __attribute__((__packed__));

struct f11_2d_data {
	u8				*f_state;
	const struct f11_2d_data_1_5	*abs_pos;
	const struct f11_2d_data_6_7	*rel_pos;
	const struct f11_2d_data_8	*gest_1;
	const struct f11_2d_data_9	*gest_2;
	const struct f11_2d_data_10	*pinch;
	const struct f11_2d_data_10_12	*flick;
	const struct f11_2d_data_11_12	*rotate;
	const struct f11_2d_data_13	*shapes;
	const struct f11_2d_data_14_15	*multi_scroll;
	const struct f11_2d_data_14_17	*scroll_zones;
};

/**
 * @axis_align - controls parameters that are useful in system prototyping
 * and bring up.
 * @sens_query - query registers for this particular sensor.
 * @data - the data reported by this sensor, mapped into a collection of
 * structs.
 * @max_x - The maximum X coordinate that will be reported by this sensor.
 * @max_y - The maximum Y coordinate that will be reported by this sensor.
 * @nbr_fingers - How many fingers can this sensor report?
 * @data_pkt - buffer for data reported by this sensor.
 * @pkt_size - number of bytes in that buffer.
 * @sensor_index - identifies this particular 2D touch sensor
 * @type_a - some early RMI4 2D sensors do not reliably track the finger
 * position when two fingers are on the device.  When this is true, we
 * assume we have one of those sensors and report events appropriately.
 * @sensor_type - indicates whether we're touchscreen or touchpad.
 * @input - input device for absolute pointing stream
 * @mouse_input - input device for relative pointing stream.
 * @input_phys - buffer for the absolute phys name for this sensor.
 * @input_mouse_phys - buffer for the relative phys name for this sensor.
 * @debugfs_flip - inverts one or both axes.  Useful in prototyping new
 * systems.
 * @debugfs_flip - coordinate clipping range for one or both axes.  Useful in
 * prototyping new systems.
 * @debugfs_delta_threshold - adjusts motion sensitivity for relative reports
 * and (in reduced reporting mode) absolute reports.  Useful in prototyping new
 * systems.
 * @debugfs_offset - offsets one or both axes.  Useful in prototyping new
 * systems.
 * @debugfs_swap - swaps X and Y axes.  Useful in prototyping new systems.
 * @debugfs_type_a - forces type A behavior.  Useful in bringing up old systems
 * when you're not sure if you've got a Type A or Type B sensor.
 * @input_mode - switches between normal input from the sensor(0), and input from Direct Touch (1)
 * @input_phys_mouse - buffer for the relative phys name for this sensor.
 */
struct f11_2d_sensor {
	struct rmi_f11_2d_axis_alignment axis_align;
	struct f11_2d_sensor_queries sens_query;
	struct f11_2d_data data;
	u16 max_x;
	u16 max_y;
	u8 nbr_fingers;
	u8 *data_pkt;
	int pkt_size;
	u8 sensor_index;
	u8 *button_map;
	struct rmi_f11_virtualbutton_map virtual_buttons;
	u32 type_a;
	enum rmi_f11_sensor_type sensor_type;
	struct input_dev *input;
	struct input_dev *mouse_input;
	int input_mode;

	struct rmi_function_container *fc;
	struct rmi_function_dev *fn_dev;
	char input_phys[NAME_BUFFER_SIZE];
	char input_phys_mouse[NAME_BUFFER_SIZE];

#ifdef CONFIG_RMI4_DEBUG
	struct dentry *sensor_root;
	struct dentry *debugfs_maxPos;
	struct dentry *debugfs_flip;
	struct dentry *debugfs_clip;
	struct dentry *debugfs_delta_threshold;
	struct dentry *debugfs_offset;
	struct dentry *debugfs_swap;
	struct dentry *debugfs_type_a;
#endif
};

struct raw_finger_data_feed_char_dev {
	/* mutex for file operation*/
	struct mutex mutex_file_op;
	/* main char dev structure */
	struct cdev raw_data_dev;
	struct class *raw_data_device_class;
	struct f11_data *my_parents_instance_data;
};

/** Data pertaining to F11 in general.  For per-sensor data, see struct
 * f11_2d_sensor.
 *
 * @dev_query - F11 device specific query registers.
 * @dev_controls - F11 device specific control registers.
 * @dev_controls_mutex - lock for the control registers.
 * @rezero_wait_ms - if nonzero, upon resume we will wait this many
 * milliseconds before rezeroing the sensor(s).  This is useful in systems with
 * poor electrical behavior on resume, where the initial calibration of the
 * sensor(s) coming out of sleep state may be bogus.
 * @sensors - per sensor data structures.
 * @debugfs_rezero_wait - allows control of the rezero_wait value.  Useful
 * during system prototyping.
 */

struct f11_data {
	struct f11_2d_device_query dev_query;
	struct f11_2d_ctrl dev_controls;
	struct mutex dev_controls_mutex;
	u16 rezero_wait_ms;
	struct f11_2d_sensor sensors[F11_MAX_NUM_OF_SENSORS];

#ifdef CONFIG_RMI4_DEBUG
	struct dentry *debugfs_rezero_wait;
#endif
	struct raw_finger_data_feed_char_dev *finger_data_feed;
};

enum finger_state_values {
	F11_NO_FINGER	= 0x00,
	F11_PRESENT	= 0x01,
	F11_INACCURATE	= 0x02,
	F11_RESERVED	= 0x03
};

static ssize_t f11_rezero_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct rmi_function_dev *fn_dev = NULL;
	unsigned int rezero;
	int retval = 0;

	fn_dev = to_rmi_function_dev(dev);

	if (sscanf(buf, "%u", &rezero) != 1)
		return -EINVAL;
	if (rezero > 1)
		return -EINVAL;

	/* Per spec, 0 has no effect, so we skip it entirely. */
	if (rezero) {
		/* Command register always reads as 0, so just use a local. */
		struct f11_2d_commands commands = {
			.rezero = true,
		};

		retval = rmi_write_block(fn_dev->rmi_dev,
					fn_dev->fd.command_base_addr,
					&commands, sizeof(commands));
		if (retval < 0) {
			dev_err(dev, "%s: failed to issue rezero command, error = %d.",
				__func__, retval);
			return retval;
		}
	}

	return count;
}

static struct device_attribute dev_attr_rezero =
	__ATTR(rezero, RMI_WO_ATTR, NULL, f11_rezero_store);
static struct device_attribute dev_attr_mode =
	__ATTR(mode, RMI_RW_ATTR, f11_mode_show, f11_mode_store);

static struct attribute *attrs[] = {
	&dev_attr_rezero.attr,
	&dev_attr_mode.attr,
	NULL,
};
static struct attribute_group fn11_attrs = GROUP(attrs);

static int rmi_f11_raw_finger_data_char_dev_register(struct f11_data *rmi_f11_instance_data);


#ifdef CONFIG_RMI4_DEBUG

struct sensor_debugfs_data {
	bool done;
	struct f11_2d_sensor *sensor;
};

static int sensor_debug_open(struct inode *inodep, struct file *filp)
{
	struct sensor_debugfs_data *data;
	struct f11_2d_sensor *sensor = inodep->i_private;

	data = kzalloc(sizeof(struct sensor_debugfs_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->sensor = sensor;
	filp->private_data = data;
	return 0;
}

static int sensor_debug_release(struct inode *inodep, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}
static ssize_t maxPos_read(struct file *filp, char __user *buffer, size_t size,
		    loff_t *offset) {
	int retval;
	char *local_buf;
	struct sensor_debugfs_data *data = filp->private_data;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size, "%u %u\n",
			data->sensor->max_x,
			data->sensor->max_y);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static const struct file_operations maxPos_fops = {
	.owner = THIS_MODULE,
	.open = sensor_debug_open,
	.release = sensor_debug_release,
	.read = maxPos_read,
};


static ssize_t flip_read(struct file *filp, char __user *buffer, size_t size,
		    loff_t *offset) {
	int retval;
	char *local_buf;
	struct sensor_debugfs_data *data = filp->private_data;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size, "%u %u\n",
			data->sensor->axis_align.flip_x,
			data->sensor->axis_align.flip_y);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t flip_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	unsigned int new_X;
	unsigned int new_Y;
	struct sensor_debugfs_data *data = filp->private_data;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u %u", &new_X, &new_Y);
	kfree(local_buf);
	if (retval != 2 || new_X > 1 || new_Y > 1)
		return -EINVAL;

	data->sensor->axis_align.flip_x = new_X;
	data->sensor->axis_align.flip_y = new_Y;

	return size;
}

static const struct file_operations flip_fops = {
	.owner = THIS_MODULE,
	.open = sensor_debug_open,
	.release = sensor_debug_release,
	.read = flip_read,
	.write = flip_write,
};

static ssize_t delta_threshold_read(struct file *filp, char __user *buffer,
		size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	struct sensor_debugfs_data *data = filp->private_data;
	struct f11_data *f11 = data->sensor->fn_dev->data;
	struct f11_2d_ctrl *ctrl = &f11->dev_controls;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size, "%u %u\n",
			ctrl->ctrl0_9->delta_x_threshold,
			ctrl->ctrl0_9->delta_y_threshold);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;

}

static ssize_t delta_threshold_write(struct file *filp,
		const char __user *buffer, size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	unsigned int new_X, new_Y;
	u8 save_X, save_Y;
	int rc;
	struct sensor_debugfs_data *data = filp->private_data;
	struct f11_data *f11 = data->sensor->fn_dev->data;
	struct f11_2d_ctrl *ctrl = &f11->dev_controls;
	struct rmi_device *rmi_dev =  data->sensor->fn_dev->rmi_dev;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u %u", &new_X, &new_Y);
	kfree(local_buf);
	if (retval != 2 || new_X > 1 || new_Y > 1)
		return -EINVAL;

	save_X = ctrl->ctrl0_9->delta_x_threshold;
	save_Y = ctrl->ctrl0_9->delta_y_threshold;

	ctrl->ctrl0_9->delta_x_threshold = new_X;
	ctrl->ctrl0_9->delta_y_threshold = new_Y;
	rc = rmi_write_block(rmi_dev, ctrl->ctrl0_9_address,
			ctrl->ctrl0_9, sizeof(*ctrl->ctrl0_9));
	if (rc < 0) {
		dev_warn(&data->sensor->fn_dev->dev,
			"Failed to write to delta_threshold. Code: %d.\n",
			rc);
		ctrl->ctrl0_9->delta_x_threshold = save_X;
		ctrl->ctrl0_9->delta_y_threshold = save_Y;
	}

	return size;
}

static const struct file_operations delta_threshold_fops = {
	.owner = THIS_MODULE,
	.open = sensor_debug_open,
	.release = sensor_debug_release,
	.read = delta_threshold_read,
	.write = delta_threshold_write,
};

static ssize_t offset_read(struct file *filp, char __user *buffer, size_t size,
		    loff_t *offset) {
	int retval;
	char *local_buf;
	struct sensor_debugfs_data *data = filp->private_data;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size, "%u %u\n",
			data->sensor->axis_align.offset_X,
			data->sensor->axis_align.offset_Y);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t offset_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset)
{
	int retval;
	char *local_buf;
	int new_X;
	int new_Y;
	struct sensor_debugfs_data *data = filp->private_data;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}
	retval = sscanf(local_buf, "%u %u", &new_X, &new_Y);
	kfree(local_buf);
	if (retval != 2)
		return -EINVAL;

	data->sensor->axis_align.offset_X = new_X;
	data->sensor->axis_align.offset_Y = new_Y;

	return size;
}

static const struct file_operations offset_fops = {
	.owner = THIS_MODULE,
	.open = sensor_debug_open,
	.release = sensor_debug_release,
	.read = offset_read,
	.write = offset_write,
};

static ssize_t clip_read(struct file *filp, char __user *buffer, size_t size,
		    loff_t *offset) {
	int retval;
	char *local_buf;
	struct sensor_debugfs_data *data = filp->private_data;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size, "%u %u %u %u\n",
			data->sensor->axis_align.clip_X_low,
			data->sensor->axis_align.clip_X_high,
			data->sensor->axis_align.clip_Y_low,
			data->sensor->axis_align.clip_Y_high);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t clip_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset)
{
	int retval;
	char *local_buf;
	unsigned int new_X_low, new_X_high, new_Y_low, new_Y_high;
	struct sensor_debugfs_data *data = filp->private_data;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u %u %u %u",
		&new_X_low, &new_X_high, &new_Y_low, &new_Y_high);
	kfree(local_buf);
	if (retval != 4)
		return -EINVAL;

	if (new_X_low >= new_X_high || new_Y_low >= new_Y_high)
		return -EINVAL;

	data->sensor->axis_align.clip_X_low = new_X_low;
	data->sensor->axis_align.clip_X_high = new_X_high;
	data->sensor->axis_align.clip_Y_low = new_Y_low;
	data->sensor->axis_align.clip_Y_high = new_Y_high;

	return size;
}

static const struct file_operations clip_fops = {
	.owner = THIS_MODULE,
	.open = sensor_debug_open,
	.release = sensor_debug_release,
	.read = clip_read,
	.write = clip_write,
};

static void rmi_f11_setup_sensor_debugfs(struct f11_2d_sensor *sensor)

{
	int retval = 0;
	char fname[NAME_BUFFER_SIZE];
	struct rmi_function_dev *fn_dev = sensor->fn_dev;
	struct dentry *sensor_root;
	char dirname[sizeof("sensorNN")];


	if (!fn_dev->debugfs_root)
		return;

	snprintf(dirname, sizeof(dirname), "input%u", sensor->sensor_index);
	sensor_root = debugfs_create_dir(dirname, fn_dev->debugfs_root);
	if (!sensor_root) {
		dev_warn(&fn_dev->dev,
			 "Failed to create debugfs directory %s for sensor %d\n",
			 dirname, sensor->sensor_index);
		return;
	}

	retval = snprintf(fname, NAME_BUFFER_SIZE, "maxPos");
	sensor->debugfs_maxPos = debugfs_create_file(fname, RMI_RO_ATTR,
				sensor_root, sensor, &maxPos_fops);
	if (!sensor->debugfs_maxPos)
		dev_warn(&fn_dev->dev, "Failed to create debugfs %s.\n",
			 fname);

	retval = snprintf(fname, NAME_BUFFER_SIZE, "flip");
	sensor->debugfs_flip = debugfs_create_file(fname, RMI_RW_ATTR,
				sensor_root, sensor, &flip_fops);
	if (!sensor->debugfs_flip)
		dev_warn(&fn_dev->dev, "Failed to create debugfs %s.\n",
			 fname);

	retval = snprintf(fname, NAME_BUFFER_SIZE, "clip");
	sensor->debugfs_clip = debugfs_create_file(fname, RMI_RW_ATTR,
				sensor_root, sensor, &clip_fops);
	if (!sensor->debugfs_clip)
		dev_warn(&fn_dev->dev, "Failed to create debugfs %s.\n",
			 fname);

	retval = snprintf(fname, NAME_BUFFER_SIZE, "delta_threshold");
	sensor->debugfs_clip = debugfs_create_file(fname, RMI_RW_ATTR,
				sensor_root, sensor,
				&delta_threshold_fops);
	if (!sensor->debugfs_delta_threshold)
		dev_warn(&fn_dev->dev, "Failed to create debugfs %s.\n",
			 fname);

	retval = snprintf(fname, NAME_BUFFER_SIZE, "offset");
	sensor->debugfs_offset = debugfs_create_file(fname, RMI_RW_ATTR,
				sensor_root, sensor, &offset_fops);
	if (!sensor->debugfs_offset)
		dev_warn(&fn_dev->dev, "Failed to create debugfs %s.\n",
			 fname);

	retval = snprintf(fname, NAME_BUFFER_SIZE, "swap");
	sensor->debugfs_swap = debugfs_create_bool(fname, RMI_RW_ATTR,
				sensor_root, &sensor->axis_align.swap_axes);
	if (!sensor->debugfs_swap)
		dev_warn(&fn_dev->dev,
			"Failed to create debugfs swap for sensor %d.\n",
			 sensor->sensor_index);

	retval = snprintf(fname, NAME_BUFFER_SIZE, "type_a");
	sensor->debugfs_type_a = debugfs_create_bool(fname, RMI_RW_ATTR,
				sensor_root, &sensor->type_a);
	if (!sensor->debugfs_type_a)
		dev_warn(&fn_dev->dev,
			 "Failed to create debugfs type_a for sensor %d.\n",
			 sensor->sensor_index);

	return;
}

struct f11_debugfs_data {
	bool done;
	struct rmi_function_dev *fn_dev;
};

static int f11_debug_open(struct inode *inodep, struct file *filp)
{
	struct f11_debugfs_data *data;
	struct rmi_function_dev *fn_dev = inodep->i_private;

	data = devm_kzalloc(&fn_dev->dev, sizeof(struct f11_debugfs_data),
		GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->fn_dev = fn_dev;
	filp->private_data = data;
	return 0;
}

static ssize_t rezero_wait_read(struct file *filp, char __user *buffer,
		size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	struct f11_debugfs_data *data = filp->private_data;
	struct f11_data *f11 = data->fn_dev->data;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size, "%u\n", f11->rezero_wait_ms);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t rezero_wait_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset)
{
	int retval;
	char *local_buf;
	int new_value;
	struct f11_debugfs_data *data = filp->private_data;
	struct f11_data *f11 = data->fn_dev->data;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u", &new_value);
	kfree(local_buf);
	if (retval != 1 || new_value > 65535)
		return -EINVAL;

	f11->rezero_wait_ms = new_value;
	return size;
}

static const struct file_operations rezero_wait_fops = {
	.owner = THIS_MODULE,
	.open = f11_debug_open,
	.read = rezero_wait_read,
	.write = rezero_wait_write,
};

static inline int rmi_f11_setup_debugfs(struct rmi_function_dev *fn_dev)
{
	struct f11_data *f11 = fn_dev->data;

	if (!fn_dev->debugfs_root)
		return -ENODEV;

	f11->debugfs_rezero_wait = debugfs_create_file("rezero_wait",
		RMI_RW_ATTR, fn_dev->debugfs_root, fn_dev, &rezero_wait_fops);
	if (!f11->debugfs_rezero_wait)
		dev_warn(&fn_dev->dev,
			 "Failed to create debugfs rezero_wait.\n");

	return 0;
}

#else
#define rmi_f11_setup_sensor_debugfs(s) 0
#define rmi_f11_setup_debugfs(d) 0
#endif
/* End adding debugfs */

/** F11_INACCURATE state is overloaded to indicate pen present. */
#define F11_PEN F11_INACCURATE

static int get_tool_type(struct f11_2d_sensor *sensor, u8 finger_state)
{
	if (IS_ENABLED(CONFIG_RMI4_F11_PEN) &&
			sensor->sens_query.query9.has_pen &&
			finger_state == F11_PEN)
		return MT_TOOL_PEN;
	return MT_TOOL_FINGER;
}

static void rmi_f11_rel_pos_report(struct f11_2d_sensor *sensor, u8 n_finger)
{
	struct f11_2d_data *data = &sensor->data;
	struct rmi_f11_2d_axis_alignment *axis_align = &sensor->axis_align;
	s8 x, y;
	s8 temp;

	x = data->rel_pos[n_finger].delta_x;
	y = data->rel_pos[n_finger].delta_y;

	x = min(F11_REL_POS_MAX, max(F11_REL_POS_MIN, (int)x));
	y = min(F11_REL_POS_MAX, max(F11_REL_POS_MIN, (int)y));

	if (axis_align->swap_axes) {
		temp = x;
		x = y;
		y = temp;
	}
	if (axis_align->flip_x)
		x = min(F11_REL_POS_MAX, -x);
	if (axis_align->flip_y)
		y = min(F11_REL_POS_MAX, -y);

	if (x || y) {
		input_report_rel(sensor->input, REL_X, x);
		input_report_rel(sensor->input, REL_Y, y);
		input_report_rel(sensor->mouse_input, REL_X, x);
		input_report_rel(sensor->mouse_input, REL_Y, y);
	}
	input_sync(sensor->mouse_input);
}

static void rmi_f11_abs_pos_report(struct f11_data *f11,
				   struct f11_2d_sensor *sensor,
					u8 finger_state, u8 n_finger)
{
	struct f11_2d_data *data = &sensor->data;
	struct rmi_f11_2d_axis_alignment *axis_align = &sensor->axis_align;
	int x, y, z;
	int w_x, w_y, w_max, w_min, orient;
	int temp;

	if (finger_state) {
		x = ((data->abs_pos[n_finger].x_msb << 4) |
			data->abs_pos[n_finger].x_lsb);
		y = ((data->abs_pos[n_finger].y_msb << 4) |
			data->abs_pos[n_finger].y_lsb);
		z = data->abs_pos[n_finger].z;
		w_x = data->abs_pos[n_finger].w_x;
		w_y = data->abs_pos[n_finger].w_y;
		w_max = max(w_x, w_y);
		w_min = min(w_x, w_y);

		if (axis_align->swap_axes) {
			temp = x;
			x = y;
			y = temp;
			temp = w_x;
			w_x = w_y;
			w_y = temp;
		}

		orient = w_x > w_y ? 1 : 0;

		if (axis_align->flip_x)
			x = max(sensor->max_x - x, 0);

		if (axis_align->flip_y)
			y = max(sensor->max_y - y, 0);

		/*
		* here checking if X offset or y offset are specified is
		*  redundant.  We just add the offsets or, clip the values
		*
		* note: offsets need to be done before clipping occurs,
		* or we could get funny values that are outside
		* clipping boundaries.
		*/
		x += axis_align->offset_X;
		y += axis_align->offset_Y;
		x =  max(axis_align->clip_X_low, x);
		y =  max(axis_align->clip_Y_low, y);
		if (axis_align->clip_X_high)
			x = min(axis_align->clip_X_high, x);
		if (axis_align->clip_Y_high)
			y =  min(axis_align->clip_Y_high, y);

	}


	/* Some UIs ignore W of zero, so we fudge it to 1 for pens.  This
	 * only appears to be an issue when reporting pens, not plain old
	 * fingers. */
	if (IS_ENABLED(CONFIG_RMI4_F11_PEN) &&
			get_tool_type(sensor, finger_state) == MT_TOOL_PEN) {
		w_max = max(1, w_max);
		w_min = max(1, w_min);
	}

	if (sensor->type_a) {
		input_report_abs(sensor->input, ABS_MT_TRACKING_ID, n_finger);
		input_report_abs(sensor->input, ABS_MT_TOOL_TYPE,
					get_tool_type(sensor, finger_state));
	} else {
		input_mt_slot(sensor->input, n_finger);
		input_mt_report_slot_state(sensor->input,
			get_tool_type(sensor, finger_state), finger_state);
	}

	if (finger_state) {
		input_report_abs(sensor->input, ABS_MT_PRESSURE, z);
		input_report_abs(sensor->input, ABS_MT_TOUCH_MAJOR, w_max);
		input_report_abs(sensor->input, ABS_MT_TOUCH_MINOR, w_min);
		input_report_abs(sensor->input, ABS_MT_ORIENTATION, orient);
		input_report_abs(sensor->input, ABS_MT_POSITION_X, x);
		input_report_abs(sensor->input, ABS_MT_POSITION_Y, y);
		dev_dbg(&sensor->fn_dev->dev,
			"finger[%d]:%d - x:%d y:%d z:%d w_max:%d w_min:%d\n",
			n_finger, finger_state, x, y, z, w_max, w_min);
	}
	/* MT sync between fingers */
	if (sensor->type_a)
	input_mt_sync(sensor->input);
}

#ifdef CONFIG_RMI4_VIRTUAL_BUTTON
static int rmi_f11_virtual_button_handler(struct f11_2d_sensor *sensor)
{
	int i;
	int x;
	int y;
	struct rmi_f11_virtualbutton_map *virtualbutton_map;
	struct virtualbutton_map virtualbutton;

	if (sensor->sens_query.has_gestures &&
				sensor->data.gest_1->single_tap) {
		virtualbutton_map = &sensor->virtual_buttons;
		x = ((sensor->data.abs_pos[0].x_msb << 4) |
			sensor->data.abs_pos[0].x_lsb);
		y = ((sensor->data.abs_pos[0].y_msb << 4) |
			sensor->data.abs_pos[0].y_lsb);
		for (i = 0; i < virtualbutton_map->buttons; i++) {
			virtualbutton = virtualbutton_map->map[i];
			if (x >= virtualbutton.x &&
				x < (virtualbutton.x + virtualbutton.width) &&
				y >= virtualbutton.y &&
				y < (virtualbutton.y + virtualbutton.height)) {
				input_report_key(sensor->input,
					virtualbutton_map->map[i].code, 1);
				input_report_key(sensor->input,
					virtualbutton_map->map[i].code, 0);
				input_sync(sensor->input);
				return 0;
			}
		}
	}
	return 0;
}
#else
#define rmi_f11_virtual_button_handler(sensor)
#endif
static void rmi_f11_finger_handler(struct f11_data *f11,
				   struct f11_2d_sensor *sensor)
{
	const u8 *f_state = sensor->data.f_state;
	u8 finger_state;
	u8 finger_pressed_count;
	u8 i;

	for (i = 0, finger_pressed_count = 0; i < sensor->nbr_fingers; i++) {
		/* Possible of having 4 fingers per f_statet register */
		finger_state = GET_FINGER_STATE(f_state, i);

		if (finger_state == F11_RESERVED) {
			pr_err("%s: Invalid finger state[%d]:0x%02x.", __func__,
					i, finger_state);
			continue;
		} else if ((finger_state == F11_PRESENT) ||
				(finger_state == F11_INACCURATE)) {
			finger_pressed_count++;
		}

		if (sensor->data.abs_pos)
			rmi_f11_abs_pos_report(f11, sensor, finger_state, i);

		if (sensor->data.rel_pos)
			rmi_f11_rel_pos_report(sensor, i);
	}
	input_report_key(sensor->input, BTN_TOUCH, finger_pressed_count);
	input_sync(sensor->input);
}

static int f11_2d_construct_data(struct f11_2d_sensor *sensor)
{
	struct f11_2d_sensor_queries *query = &sensor->sens_query;
	struct f11_2d_data *data = &sensor->data;
	int i;

	sensor->nbr_fingers = (query->info.number_of_fingers == 5 ? 10 :
				query->info.number_of_fingers + 1);

	sensor->pkt_size = DIV_ROUND_UP(sensor->nbr_fingers, 4);

	if (query->info.has_abs)
		sensor->pkt_size += (sensor->nbr_fingers * 5);

	if (query->info.has_rel)
		sensor->pkt_size +=  (sensor->nbr_fingers * 2);

	/* Check if F11_2D_Query7 is non-zero */
	if (has_gesture_bits(&query->gesture_info, 0))
		sensor->pkt_size += sizeof(u8);

	/* Check if F11_2D_Query7 or F11_2D_Query8 is non-zero */
	if (has_gesture_bits(&query->gesture_info, 0) ||
				has_gesture_bits(&query->gesture_info, 1))
		sensor->pkt_size += sizeof(u8);

	if (query->gesture_info.has_pinch || query->gesture_info.has_flick
			|| query->gesture_info.has_rotate) {
		sensor->pkt_size += 3;
		if (!query->gesture_info.has_flick)
			sensor->pkt_size--;
		if (!query->gesture_info.has_rotate)
			sensor->pkt_size--;
	}

	if (query->gesture_info.has_touch_shapes)
		sensor->pkt_size +=
			DIV_ROUND_UP(query->ts_info.nbr_touch_shapes + 1, 8);

	sensor->data_pkt = kzalloc(sensor->pkt_size, GFP_KERNEL);

	if (!sensor->data_pkt)
		return -ENOMEM;

	data->f_state = sensor->data_pkt;
	i = DIV_ROUND_UP(sensor->nbr_fingers, 4);

	if (query->info.has_abs) {
		data->abs_pos = (struct f11_2d_data_1_5 *)
				&sensor->data_pkt[i];
		i += (sensor->nbr_fingers * 5);
	}

	if (query->info.has_rel) {
		data->rel_pos = (struct f11_2d_data_6_7 *)
				&sensor->data_pkt[i];
		i += (sensor->nbr_fingers * 2);
	}

	if (has_gesture_bits(&query->gesture_info, 0)) {
		data->gest_1 = (struct f11_2d_data_8 *)&sensor->data_pkt[i];
		i++;
	}

	if (has_gesture_bits(&query->gesture_info, 0) ||
				has_gesture_bits(&query->gesture_info, 1)) {
		data->gest_2 = (struct f11_2d_data_9 *)&sensor->data_pkt[i];
		i++;
	}

	if (query->gesture_info.has_pinch) {
		data->pinch = (struct f11_2d_data_10 *)&sensor->data_pkt[i];
		i++;
	}

	if (query->gesture_info.has_flick) {
		if (query->gesture_info.has_pinch) {
			data->flick = (struct f11_2d_data_10_12 *)data->pinch;
			i += 2;
		} else {
			data->flick = (struct f11_2d_data_10_12 *)
					&sensor->data_pkt[i];
			i += 3;
		}
	}

	if (query->gesture_info.has_rotate) {
		if (query->gesture_info.has_flick) {
			data->rotate = (struct f11_2d_data_11_12 *)
					(data->flick + 1);
		} else {
			data->rotate = (struct f11_2d_data_11_12 *)
					&sensor->data_pkt[i];
			i += 2;
		}
	}

	if (query->gesture_info.has_touch_shapes)
		data->shapes = (struct f11_2d_data_13 *)&sensor->data_pkt[i];

	return 0;
}

static int f11_read_control_regs(struct rmi_function_dev *fn_dev,
				struct f11_2d_ctrl *ctrl, u16 ctrl_base_addr) {
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	u16 read_address = ctrl_base_addr;
	int error = 0;

	ctrl->ctrl0_9_address = read_address;
	error = rmi_read_block(rmi_dev, read_address, ctrl->ctrl0_9,
		sizeof(*ctrl->ctrl0_9));
	if (error < 0) {
		dev_err(&fn_dev->dev, "Failed to read ctrl0, code: %d.\n",
			error);
		return error;
	}
	read_address += sizeof(*ctrl->ctrl0_9);

	if (ctrl->ctrl10) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl10, sizeof(*ctrl->ctrl10));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl10, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl10);
	}

	if (ctrl->ctrl11) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl11, sizeof(*ctrl->ctrl11));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl11, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl11);
	}

	if (ctrl->ctrl14) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl14, sizeof(*ctrl->ctrl14));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl14, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl14);
	}

	if (ctrl->ctrl15) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl15, sizeof(*ctrl->ctrl15));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl15, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl15);
	}

	if (ctrl->ctrl16) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl16, sizeof(*ctrl->ctrl16));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl16, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl16);
	}

	if (ctrl->ctrl17) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl17, sizeof(*ctrl->ctrl17));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl17, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl17);
	}

	if (ctrl->ctrl18_19) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl18_19, sizeof(*ctrl->ctrl18_19));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl18_19, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl18_19);
	}

	if (ctrl->ctrl20_21) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl20_21, sizeof(*ctrl->ctrl20_21));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl20_21, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl20_21);
	}

	if (ctrl->ctrl22_26) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl22_26, sizeof(*ctrl->ctrl22_26));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl22_26, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl22_26);
	}

	if (ctrl->ctrl27) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl27, sizeof(*ctrl->ctrl27));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl27, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl27);
	}

	if (ctrl->ctrl28) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl28, sizeof(*ctrl->ctrl28));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl28, code: %d.\n", error);
			return error;
		}
		read_address += sizeof(*ctrl->ctrl28);
	}

	if (ctrl->ctrl29_30) {
		error = rmi_read_block(rmi_dev, read_address,
			ctrl->ctrl29_30, sizeof(*ctrl->ctrl29_30));
		if (error < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read ctrl29_30, code: %d.\n", error);
			return error;
	}
		read_address += sizeof(*ctrl->ctrl29_30);
	}
	return 0;
}

static int f11_allocate_control_regs(struct rmi_function_dev *fn_dev,
				struct f11_2d_device_query *device_query,
				struct f11_2d_sensor_queries *sensor_query,
				struct f11_2d_ctrl *ctrl,
				u16 ctrl_base_addr) {

	ctrl->ctrl0_9 = devm_kzalloc(&fn_dev->dev,
				     sizeof(struct f11_2d_ctrl0_9), GFP_KERNEL);
	if (!ctrl->ctrl0_9)
		return -ENOMEM;
	if (has_gesture_bits(&sensor_query->gesture_info, 0)) {
		ctrl->ctrl10 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f11_2d_ctrl10), GFP_KERNEL);
		if (!ctrl->ctrl10)
			return -ENOMEM;
	}

	if (has_gesture_bits(&sensor_query->gesture_info, 1)) {
		ctrl->ctrl11 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f11_2d_ctrl11), GFP_KERNEL);
		if (!ctrl->ctrl11)
			return -ENOMEM;
	}

	if (device_query->has_query9 && sensor_query->query9.has_pen) {
		ctrl->ctrl20_21 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f11_2d_ctrl20_21), GFP_KERNEL);
		if (!ctrl->ctrl20_21)
			return -ENOMEM;
	}

	if (device_query->has_query9 && sensor_query->query9.has_proximity) {
		ctrl->ctrl22_26 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f11_2d_ctrl22_26), GFP_KERNEL);
		if (!ctrl->ctrl22_26)
			return -ENOMEM;
	}

	if (device_query->has_query9 &&
		(sensor_query->query9.has_palm_det_sensitivity ||
		sensor_query->query9.has_suppress_on_palm_detect)) {
		ctrl->ctrl27 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f11_2d_ctrl27), GFP_KERNEL);
		if (!ctrl->ctrl27)
			return -ENOMEM;
	}

	if (sensor_query->gesture_info.has_multi_finger_scroll) {
		ctrl->ctrl28 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f11_2d_ctrl28), GFP_KERNEL);
		if (!ctrl->ctrl28)
			return -ENOMEM;
	}

	if (device_query->has_query11 &&
			sensor_query->features_1.has_z_tuning) {
		ctrl->ctrl29_30 = devm_kzalloc(&fn_dev->dev,
			sizeof(struct f11_2d_ctrl29_30), GFP_KERNEL);
		if (!ctrl->ctrl29_30)
			return -ENOMEM;
	}

	return 0;
}

static int f11_write_control_regs(struct rmi_function_dev *fn_dev,
					struct f11_2d_sensor_queries *query,
					struct f11_2d_ctrl *ctrl,
					u16 ctrl_base_addr)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	u16 write_address = ctrl_base_addr;
	int error;

		error = rmi_write_block(rmi_dev, write_address,
				ctrl->ctrl0_9,
				 sizeof(*ctrl->ctrl0_9));
		if (error < 0)
			return error;
	write_address += sizeof(ctrl->ctrl0_9);

	if (ctrl->ctrl10) {
		error = rmi_write_block(rmi_dev, write_address,
					ctrl->ctrl10, sizeof(*ctrl->ctrl10));
		if (error < 0)
			return error;
		write_address++;
	}

	if (ctrl->ctrl11) {
		error = rmi_write_block(rmi_dev, write_address,
					ctrl->ctrl11, sizeof(*ctrl->ctrl11));
		if (error < 0)
			return error;
		write_address++;
	}

	if (ctrl->ctrl14) {
		error = rmi_write_block(rmi_dev, write_address,
				ctrl->ctrl14, sizeof(ctrl->ctrl14));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl15);
	}

	if (ctrl->ctrl15) {
		error = rmi_write_block(rmi_dev, write_address,
				ctrl->ctrl15, sizeof(*ctrl->ctrl15));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl15);
	}

	if (ctrl->ctrl16) {
		error = rmi_write_block(rmi_dev, write_address,
				ctrl->ctrl16, sizeof(*ctrl->ctrl16));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl16);
	}

	if (ctrl->ctrl17) {
		error = rmi_write_block(rmi_dev, write_address,
				ctrl->ctrl17, sizeof(*ctrl->ctrl17));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl17);
	}

	if (ctrl->ctrl18_19) {
		error = rmi_write_block(rmi_dev, write_address,
			ctrl->ctrl18_19, sizeof(*ctrl->ctrl18_19));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl18_19);
	}

	if (ctrl->ctrl20_21) {
		error = rmi_write_block(rmi_dev, write_address,
			ctrl->ctrl20_21, sizeof(*ctrl->ctrl20_21));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl20_21);
	}

	if (ctrl->ctrl22_26) {
		error = rmi_write_block(rmi_dev, write_address,
			ctrl->ctrl22_26, sizeof(*ctrl->ctrl22_26));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl22_26);
	}

	if (ctrl->ctrl27) {
		error = rmi_write_block(rmi_dev, write_address,
			ctrl->ctrl27, sizeof(*ctrl->ctrl27));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl27);
	}

	if (ctrl->ctrl28) {
		error = rmi_write_block(rmi_dev, write_address,
			ctrl->ctrl28, sizeof(*ctrl->ctrl28));
		if (error < 0)
			return error;
		write_address += sizeof(*ctrl->ctrl28);
	}

	if (ctrl->ctrl29_30) {
		error = rmi_write_block(rmi_dev, write_address,
					ctrl->ctrl29_30,
					sizeof(struct f11_2d_ctrl29_30));
		if (error < 0)
			return error;
		write_address += sizeof(struct f11_2d_ctrl29_30);
	}

	return 0;
}

static int rmi_f11_get_query_parameters(struct rmi_device *rmi_dev,
			struct f11_2d_device_query *dev_query,
			struct f11_2d_sensor_queries *sensor_query,
			u16 query_base_addr)
{
	int query_size;
	int rc;

	rc = rmi_read_block(rmi_dev, query_base_addr,
			    &sensor_query->info, sizeof(sensor_query->info));
	if (rc < 0)
		return rc;
	query_size = sizeof(sensor_query->info);

	if (sensor_query->info.has_abs) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size,
					&sensor_query->abs_info);
		if (rc < 0)
			return rc;
		query_size++;
	}

	if (sensor_query->info.has_rel) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size,
					&sensor_query->f11_2d_query6);
		if (rc < 0)
			return rc;
		query_size++;
	}

	if (sensor_query->info.has_gestures) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
					&sensor_query->gesture_info,
					sizeof(sensor_query->gesture_info));
		if (rc < 0)
			return rc;
		query_size += sizeof(sensor_query->gesture_info);
	}

	if (dev_query->has_query9) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
					&sensor_query->query9,
					sizeof(sensor_query->query9));
		if (rc < 0)
			return rc;
		query_size += sizeof(sensor_query->query9);
	}

	if (sensor_query->gesture_info.has_touch_shapes) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
					&sensor_query->ts_info,
					sizeof(sensor_query->ts_info));
		if (rc < 0)
			return rc;
		query_size += sizeof(sensor_query->ts_info);
	}

	if (dev_query->has_query11) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
					&sensor_query->features_1,
					sizeof(sensor_query->features_1));
		if (rc < 0)
			return rc;
		query_size += sizeof(sensor_query->features_1);
	}

	if (dev_query->has_query12) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
					&sensor_query->features_2,
					sizeof(sensor_query->features_2));
		if (rc < 0)
			return rc;
		query_size += sizeof(sensor_query->features_2);
	}

	if (sensor_query->abs_info.has_jitter_filter) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
					&sensor_query->jitter_filter,
					sizeof(sensor_query->jitter_filter));
		if (rc < 0)
			return rc;
		query_size += sizeof(sensor_query->jitter_filter);
	}

	if (dev_query->has_query12 && sensor_query->features_2.has_info2) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
				    &sensor_query->info_2,
					sizeof(sensor_query->info_2));
		if (rc < 0)
			return rc;
		query_size += sizeof(sensor_query->info_2);
	}

	return query_size;
}

/* This operation is done in a number of places, so we have a handy routine
 * for it.
 */
static void f11_set_abs_params(struct rmi_function_dev *fn_dev, int index)
{
	struct f11_data *f11 = fn_dev->data;
	struct f11_2d_sensor *sensor = &f11->sensors[index];
	struct input_dev *input = sensor->input;
	int device_x_max =
		f11->dev_controls.ctrl0_9->sensor_max_x_pos;
	int device_y_max =
		f11->dev_controls.ctrl0_9->sensor_max_y_pos;
	int x_min, x_max, y_min, y_max;
	unsigned int input_flags;

	/* We assume touchscreen unless demonstrably a touchpad or specified
	 * as a touchpad in the platform data
	 */
	if (sensor->sensor_type == rmi_f11_sensor_touchpad ||
			(sensor->sens_query.features_2.has_info2 &&
				!sensor->sens_query.info_2.is_clear))
		input_flags = INPUT_PROP_POINTER;
	else
		input_flags = INPUT_PROP_DIRECT;
	set_bit(input_flags, input->propbit);

	if (sensor->axis_align.swap_axes) {
		int temp = device_x_max;
		device_x_max = device_y_max;
		device_y_max = temp;
	}
	/* Use the max X and max Y read from the device, or the clip values,
	 * whichever is stricter.
	 */
	x_min = sensor->axis_align.clip_X_low;
	if (sensor->axis_align.clip_X_high)
		x_max = min((int) device_x_max,
			sensor->axis_align.clip_X_high);
	else
		x_max = device_x_max;

	y_min = sensor->axis_align.clip_Y_low;
	if (sensor->axis_align.clip_Y_high)
		y_max = min((int) device_y_max,
			sensor->axis_align.clip_Y_high);
	else
		y_max = device_y_max;

	dev_dbg(&fn_dev->dev, "Set ranges X=[%d..%d] Y=[%d..%d].",
			x_min, x_max, y_min, y_max);

		input_set_abs_params(input, ABS_MT_PRESSURE, 0,
				DEFAULT_MAX_ABS_MT_PRESSURE, 0, 0);
		input_set_abs_params(input, ABS_MT_TOUCH_MAJOR,
				0, DEFAULT_MAX_ABS_MT_TOUCH, 0, 0);
		input_set_abs_params(input, ABS_MT_TOUCH_MINOR,
				0, DEFAULT_MAX_ABS_MT_TOUCH, 0, 0);
		input_set_abs_params(input, ABS_MT_ORIENTATION,
				0, DEFAULT_MAX_ABS_MT_ORIENTATION, 0, 0);
		input_set_abs_params(input, ABS_MT_TRACKING_ID,
				DEFAULT_MIN_ABS_MT_TRACKING_ID,
				DEFAULT_MAX_ABS_MT_TRACKING_ID, 0, 0);
		/* TODO get max_x_pos (and y) from control registers. */
		input_set_abs_params(input, ABS_MT_POSITION_X,
				x_min, x_max, 0, 0);
		input_set_abs_params(input, ABS_MT_POSITION_Y,
				y_min, y_max, 0, 0);
	if (!sensor->type_a)
		input_mt_init_slots(input, sensor->nbr_fingers, 0);
	if (IS_ENABLED(CONFIG_RMI4_F11_PEN) &&
			sensor->sens_query.query9.has_pen)
		input_set_abs_params(input, ABS_MT_TOOL_TYPE,
				     0, MT_TOOL_MAX, 0, 0);
	else
		input_set_abs_params(input, ABS_MT_TOOL_TYPE,
				     0, MT_TOOL_FINGER, 0, 0);
}

static int rmi_f11_initialize(struct rmi_function_dev *fn_dev)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f11_data *f11;
	struct f11_2d_ctrl *ctrl;
	u8 query_offset;
	u16 query_base_addr;
	u16 control_base_addr;
	u16 max_x_pos, max_y_pos, temp;
	int rc;
	int i;
	struct rmi_device_platform_data *pdata = to_rmi_platform_data(rmi_dev);

	dev_dbg(&fn_dev->dev, "Initializing F11 values for %s.\n",
		 pdata->sensor_name);

	/*
	** init instance data, fill in values and create any sysfs files
	*/
	f11 = devm_kzalloc(&fn_dev->dev, sizeof(struct f11_data), GFP_KERNEL);
	if (!f11)
		return -ENOMEM;

	fn_dev->data = f11;
	f11->rezero_wait_ms = pdata->f11_rezero_wait;

	query_base_addr = fn_dev->fd.query_base_addr;
	control_base_addr = fn_dev->fd.control_base_addr;

	rc = rmi_read(rmi_dev, query_base_addr, &f11->dev_query);
	if (rc < 0)
		return rc;

	query_offset = (query_base_addr + 1);
	/* Increase with one since number of sensors is zero based */
	for (i = 0; i < (f11->dev_query.nbr_of_sensors + 1); i++) {
		struct f11_2d_sensor *sensor = &f11->sensors[i];
		sensor->sensor_index = i;
		sensor->fn_dev = fn_dev;

		rc = rmi_f11_get_query_parameters(rmi_dev, &f11->dev_query,
				&sensor->sens_query, query_offset);
		if (rc < 0)
			return rc;
		query_offset += rc;

		rc = f11_allocate_control_regs(fn_dev,
				&f11->dev_query, &sensor->sens_query,
				&f11->dev_controls, control_base_addr);
		if (rc < 0) {
			dev_err(&fn_dev->dev,
				"Failed to allocate F11 control params.\n");
			return rc;
		}

		rc = f11_read_control_regs(fn_dev, &f11->dev_controls,
						control_base_addr);
		if (rc < 0) {
			dev_err(&fn_dev->dev,
				"Failed to read F11 control params.\n");
			return rc;
		}

		if (i < pdata->f11_sensor_count) {
			sensor->axis_align =
				pdata->f11_sensor_data[i].axis_align;
			sensor->virtual_buttons =
				pdata->f11_sensor_data[i].virtual_buttons;
			sensor->type_a = pdata->f11_sensor_data[i].type_a;
			sensor->sensor_type =
					pdata->f11_sensor_data[i].sensor_type;
		}

			rc = rmi_read_block(rmi_dev,
			  control_base_addr + F11_CTRL_SENSOR_MAX_X_POS_OFFSET,
			  (u8 *)&max_x_pos, sizeof(max_x_pos));
			if (rc < 0)
			return rc;

			rc = rmi_read_block(rmi_dev,
			  control_base_addr + F11_CTRL_SENSOR_MAX_Y_POS_OFFSET,
			  (u8 *)&max_y_pos, sizeof(max_y_pos));
			if (rc < 0)
			return rc;

		if (sensor->axis_align.swap_axes) {
			temp = max_x_pos;
			max_x_pos = max_y_pos;
			max_y_pos = temp;
		}
		sensor->max_x = max_x_pos;
		sensor->max_y = max_y_pos;

		rc = f11_2d_construct_data(sensor);
		if (rc < 0)
			return rc;

		ctrl = &f11->dev_controls;
		if (sensor->axis_align.delta_x_threshold) {
			ctrl->ctrl0_9->delta_x_threshold =
				sensor->axis_align.delta_x_threshold;
			rc = rmi_write_block(rmi_dev,
					ctrl->ctrl0_9_address,
					ctrl->ctrl0_9,
					sizeof(*ctrl->ctrl0_9));
			if (rc < 0)
				dev_warn(&fn_dev->dev, "Failed to write to delta_x_threshold %d. Code: %d.\n",
					i, rc);

		}

		if (sensor->axis_align.delta_y_threshold) {
			ctrl->ctrl0_9->delta_y_threshold =
				sensor->axis_align.delta_y_threshold;
			rc = rmi_write_block(rmi_dev,
					ctrl->ctrl0_9_address,
					ctrl->ctrl0_9,
					sizeof(*ctrl->ctrl0_9));
		if (rc < 0)
				dev_warn(&fn_dev->dev, "Failed to write to delta_y_threshold %d. Code: %d.\n",
					i, rc);
		}

		rmi_f11_setup_sensor_debugfs(sensor);

	}

	rmi_f11_setup_debugfs(fn_dev);

	mutex_init(&f11->dev_controls_mutex);
	return 0;
}

static void register_virtual_buttons(struct rmi_function_dev *fn_dev,
				     struct f11_2d_sensor *sensor) {
	int j;

	if (!sensor->sens_query.info.has_gestures)
		return;
	if (!sensor->virtual_buttons.buttons) {
		dev_warn(&fn_dev->dev, "No virtual button platform data for 2D sensor %d.\n",
			 sensor->sensor_index);
		return;
	}
	/* call devm_kcalloc when it will be defined in kernel */
	sensor->button_map = devm_kzalloc(&fn_dev->dev,
			sensor->virtual_buttons.buttons,
			GFP_KERNEL);
	if (!sensor->button_map) {
		dev_err(&fn_dev->dev, "Failed to allocate the virtual button map.\n");
		return;
	}

	/* manage button map using input subsystem */
	sensor->input->keycode = sensor->button_map;
	sensor->input->keycodesize = sizeof(u8);
	sensor->input->keycodemax = sensor->virtual_buttons.buttons;

	/* set bits for each button... */
	for (j = 0; j < sensor->virtual_buttons.buttons; j++) {
		sensor->button_map[j] =  sensor->virtual_buttons.map[j].code;
		set_bit(sensor->button_map[j], sensor->input->keybit);
	}
}

static int rmi_f11_register_devices(struct rmi_function_dev *fn_dev)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f11_data *f11 = fn_dev->data;
	struct input_dev *input_dev;
	struct input_dev *input_dev_mouse;
	struct rmi_driver_data *driver_data = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_driver *driver = rmi_dev->driver;
	int sensors_itertd = 0;
	int i;
	int rc;
	int board, version;

	board = driver_data->board;
	version = driver_data->rev;

	for (i = 0; i < (f11->dev_query.nbr_of_sensors + 1); i++) {
		struct f11_2d_sensor *sensor = &f11->sensors[i];
		sensors_itertd = i;
		input_dev = input_allocate_device();
		if (!input_dev) {
			rc = -ENOMEM;
			goto error_unregister;
		}

		sensor->input = input_dev;
		if (driver->set_input_params) {
			rc = driver->set_input_params(rmi_dev, input_dev);
			if (rc < 0) {
				dev_err(&fn_dev->dev,
				"%s: Error in setting input device.\n",
				__func__);
				goto error_unregister;
			}
		}
		sprintf(sensor->input_phys, "%s.abs%d/input0",
			dev_name(&fn_dev->dev), i);
		input_dev->phys = sensor->input_phys;
		input_dev->dev.parent = &rmi_dev->dev;
		input_set_drvdata(input_dev, f11);

		set_bit(EV_SYN, input_dev->evbit);
		set_bit(EV_ABS, input_dev->evbit);
		input_set_capability(input_dev, EV_KEY, BTN_TOUCH);
#if NV_NOTIFY_OUT_OF_IDLE
		input_set_capability(input_dev, EV_MSC, MSC_ACTIVITY);
#endif

		f11_set_abs_params(fn_dev, i);

		if (sensor->sens_query.info.has_rel) {
			set_bit(EV_REL, input_dev->evbit);
			set_bit(REL_X, input_dev->relbit);
			set_bit(REL_Y, input_dev->relbit);
		}
		rc = input_register_device(input_dev);
		if (rc < 0) {
			input_free_device(input_dev);
			sensor->input = NULL;
			goto error_unregister;
		}

		if (IS_ENABLED(CONFIG_RMI4_VIRTUAL_BUTTON))
			register_virtual_buttons(fn_dev, sensor);

		if (sensor->sens_query.info.has_rel) {
			/*create input device for mouse events  */
			input_dev_mouse = input_allocate_device();
			if (!input_dev_mouse) {
				rc = -ENOMEM;
				goto error_unregister;
			}

			sensor->mouse_input = input_dev_mouse;
			if (driver->set_input_params) {
				rc = driver->set_input_params(rmi_dev,
					input_dev_mouse);
				if (rc < 0) {
					dev_err(&fn_dev->dev,
					"%s: Error in setting input device.\n",
					__func__);
					goto error_unregister;
				}
			}
			sprintf(sensor->input_phys_mouse, "%s.rel%d/input0",
				dev_name(&fn_dev->dev), i);
			set_bit(EV_REL, input_dev_mouse->evbit);
			set_bit(REL_X, input_dev_mouse->relbit);
			set_bit(REL_Y, input_dev_mouse->relbit);

			set_bit(BTN_MOUSE, input_dev_mouse->evbit);
			/* Register device's buttons and keys */
			set_bit(EV_KEY, input_dev_mouse->evbit);
			set_bit(BTN_LEFT, input_dev_mouse->keybit);
			set_bit(BTN_MIDDLE, input_dev_mouse->keybit);
			set_bit(BTN_RIGHT, input_dev_mouse->keybit);

			rc = input_register_device(input_dev_mouse);
			if (rc < 0) {
				input_free_device(input_dev_mouse);
				sensor->mouse_input = NULL;
				goto error_unregister;
			}

				set_bit(BTN_RIGHT, input_dev_mouse->keybit);
		}

	}

	return 0;

error_unregister:
	for (; sensors_itertd > 0; sensors_itertd--) {
		if (f11->sensors[sensors_itertd].input) {
			if (f11->sensors[sensors_itertd].mouse_input) {
				input_unregister_device(
				   f11->sensors[sensors_itertd].mouse_input);
				f11->sensors[sensors_itertd].mouse_input = NULL;
			}
			input_unregister_device(f11->sensors[i].input);
			f11->sensors[i].input = NULL;
		}
	}

	return rc;
}

static void rmi_f11_free_devices(struct rmi_function_dev *fn_dev)
{
	struct f11_data *f11 = fn_dev->data;
	int i;

	for (i = 0; i < (f11->dev_query.nbr_of_sensors + 1); i++) {
		if (f11->sensors[i].input)
			input_unregister_device(f11->sensors[i].input);
		if (f11->sensors[i].mouse_input)
			input_unregister_device(f11->sensors[i].mouse_input);
	}
}

static int rmi_f11_create_sysfs(struct rmi_function_dev *fn_dev)
{
	struct f11_data *f11 = fn_dev->data;

	dev_dbg(&fn_dev->dev, "Creating sysfs files.\n");

	/* add a character device for user space apps so */
	/* that direct-touch daemon can report finger    */
	/* positions                                     */
	rmi_f11_raw_finger_data_char_dev_register(f11);

	if (sysfs_create_group(&fn_dev->dev.kobj, &fn11_attrs) < 0) {
		dev_err(&fn_dev->dev, "Failed to create query sysfs files.");
		return -ENODEV;
	}
	return 0;
}

static int rmi_f11_config(struct rmi_function_dev *fn_dev)
{
	struct f11_data *f11 = fn_dev->data;
	int i;
	int rc;

	for (i = 0; i < (f11->dev_query.nbr_of_sensors + 1); i++) {
		rc = f11_write_control_regs(fn_dev, &f11->sensors[i].sens_query,
				&f11->dev_controls, fn_dev->fd.query_base_addr);
		if (rc < 0)
	return rc;
	}

	return 0;
}

int rmi_f11_attention(struct rmi_function_dev *fn_dev,
						unsigned long *irq_bits)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f11_data *f11 = fn_dev->data;
	u16 data_base_addr = fn_dev->fd.data_base_addr;
	u16 data_base_addr_offset = 0;
	int error;
	int i;

	for (i = 0; i < f11->dev_query.nbr_of_sensors + 1; i++) {
		error = rmi_read_block(rmi_dev,
				data_base_addr + data_base_addr_offset,
				f11->sensors[i].data_pkt,
				f11->sensors[i].pkt_size);
		if (error < 0)
			return error;

		rmi_f11_finger_handler(f11, &f11->sensors[i]);
		rmi_f11_virtual_button_handler(&f11->sensors[i]);
		data_base_addr_offset += f11->sensors[i].pkt_size;
	}

	return 0;
}

#if NV_NOTIFY_OUT_OF_IDLE
static int rmi_f11_out_of_idle(struct rmi_function_dev *fn_dev)
{
	struct f11_data *f11 = fn_dev->data;
	struct f11_2d_sensor *sensor;
	int i;

	for (i = 0; i < f11->dev_query.nbr_of_sensors + 1; i++) {
		sensor = &f11->sensors[i];
		input_event(sensor->input, EV_MSC, MSC_ACTIVITY, 1);
	}

	return 0;
}
#endif

#ifdef CONFIG_PM
static int rmi_f11_resume(struct rmi_function_dev *fn_dev)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct f11_data *data = fn_dev->data;
	/* Command register always reads as 0, so we can just use a local. */
	struct f11_2d_commands commands = {
		.rezero = true,
	};
	int retval = 0;

	dev_dbg(&fn_dev->dev, "Resuming...\n");
	if (!data->rezero_wait_ms)
		return 0;

	mdelay(data->rezero_wait_ms);

	retval = rmi_write_block(rmi_dev, fn_dev->fd.command_base_addr,
			&commands, sizeof(commands));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "%s: failed to issue rezero command, error = %d.",
			__func__, retval);
		return retval;
	}

	return retval;
}
#endif /* CONFIG_PM */

static int rmi_f11_remove(struct rmi_function_dev *fn_dev)
{
	debugfs_remove_recursive(fn_dev->debugfs_root);
	sysfs_remove_group(&fn_dev->dev.kobj, &fn11_attrs);

	rmi_f11_free_devices(fn_dev);
	return 0;
}

static int rmi_f11_probe(struct rmi_function_dev *fn_dev)
{
	int rc;

	rc = rmi_f11_initialize(fn_dev);
	if (rc < 0)
		return rc;

	rc = rmi_f11_register_devices(fn_dev);
	if (rc < 0)
		return rc;

	rc = rmi_f11_create_sysfs(fn_dev);
	if (rc < 0)
		return rc;

	return 0;
}

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f11",
	},
	.func = FUNCTION_NUMBER,
	.probe = rmi_f11_probe,
	.remove = rmi_f11_remove,
	.config = rmi_f11_config,
	.attention = rmi_f11_attention,
#ifdef CONFIG_HAS_EARLYSUSPEND
	.late_resume = rmi_f11_resume,
#elif defined(CONFIG_PM)
	.resume = rmi_f11_resume,
#endif  /* defined(CONFIG_HAS_EARLYSUSPEND) */
#if NV_NOTIFY_OUT_OF_IDLE
	.out_of_idle = rmi_f11_out_of_idle,
#endif
};

static int __init rmi_f11_module_init(void)
{
	int error;

	error = driver_register(&function_driver.driver);
	if (error < 0) {
		pr_err("%s: register driver failed!\n", __func__);
		return error;
	}

	return 0;
}

static void __exit rmi_f11_module_exit(void)
{
	driver_unregister(&function_driver.driver);
}

static ssize_t f11_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t count)
{
	struct rmi_function_dev *fn_dev;
	struct f11_data *instance_data;
	unsigned int new_value;

	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;


	if (sscanf(buf, "%u", &new_value) != 1)
		return -EINVAL;
	if (new_value != F11_INPUT_SOURCE_SENSOR
	    &&
	    new_value != F11_INPUT_SOURCE_USER_APP)
		return -EINVAL;

	instance_data->sensors[0].input_mode = new_value;

	return count;
}

static ssize_t f11_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct rmi_function_dev *fn_dev;
	struct f11_data *instance_data;

	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			instance_data->sensors[0].input_mode);
}


static ssize_t rmi_f11_raw_finger_char_dev_write(struct file *,
						 const char __user *,
						 size_t, loff_t *);
static int     rmi_f11_raw_finger_char_dev_open(struct inode *,
						struct file *);
static int     rmi_f11_raw_finger_char_dev_release(struct inode *,
						   struct file *);


static const struct file_operations rmi_f11_raw_finger_data_dev_fops = {
	.owner =    THIS_MODULE,
	.write =    rmi_f11_raw_finger_char_dev_write,
	.open =     rmi_f11_raw_finger_char_dev_open,
	.release =  rmi_f11_raw_finger_char_dev_release,
};

/*store dynamically allocated major number of char device*/
static int rmi_f11_raw_finger_data_dev_major_num;


/*
 * rmi_f11_raw_finger_char_dev_write: - use to write data into RMI stream
 *
 * @filep : file structure for write
 * @buf: user-level buffer pointer contains data to be written
 * @count: number of byte be be written
 * @f_pos: offset (starting register address)
 *
 * @return number of bytes written from user buffer (buf) if succeeds
 *         negative number if error occurs.
 */
static ssize_t
rmi_f11_raw_finger_char_dev_write(struct file *filp, const char __user *buf,
				  size_t count, loff_t *f_pos)
{
	int retval = 0;
	struct f11_data *my_instance_data = NULL;
	struct raw_finger_data_feed_char_dev *char_dev_container = NULL;
	ssize_t ret_value  = 0;
	int i = 0;
	int len;

	if (!filp) {
		pr_err("%s: called with NULL file pointer\n", __func__);
		return -EINVAL;
	}

	char_dev_container = filp->private_data;

	if (!char_dev_container) {
		pr_err("%s: called with NULL private_data member\n",
			__func__);
		return -EINVAL;
	}

	my_instance_data = char_dev_container->my_parents_instance_data;

	if (count == 0) {
		pr_err("%s: count = %d -- no space to copy output to!!!\n",
			__func__, count);
		return -ENOMEM;
	}

	len = my_instance_data->sensors[0].pkt_size > count ? count
		: my_instance_data->sensors[0].pkt_size;
	retval =
		copy_from_user(my_instance_data->sensors[0].data_pkt,
			       buf, len);

	/*
	 * call the rmi_f11_finger_handler() as if the
	 * function rmi_f11_attention() were calling it?
	 */
	rmi_f11_finger_handler(my_instance_data, &my_instance_data->sensors[0]);


	return count;
}


/*
 * rmi_f11_raw_finger_char_dev_open: - get a new handle for reading raw Touch Sensor images
 * @inp : inode struture
 * @filp: file structure for read/write
 *
 * @return 0 if succeeds
 */
static int
rmi_f11_raw_finger_char_dev_open(struct inode *inp, struct file *filp)
{
	/* store the device pointer to file structure */

	struct raw_finger_data_feed_char_dev *my_dev ;
	my_dev = container_of(inp->i_cdev,
			      struct raw_finger_data_feed_char_dev,
			      raw_data_dev);

	int ret_value = 0;

	filp->private_data = my_dev;

	return ret_value;
}

/*
 *  rmi_f11_raw_finger_char_dev_release: - release an existing handle
 *  @inp: inode structure
 *  @filp: file structure for read/write
 *
 *  @return 0 if succeeds
 */
static int
rmi_f11_raw_finger_char_dev_release(struct inode *inp, struct file *filp)
{
	return 0;
}



/* rmi_f11_raw_finger_char_dev_char_dev_unregister - unregister char device (called from up-level)
 *
 * @phys: pointer to an rmi_phys_device structure
 */

void
rmi_f11_raw_finger_char_dev_unregister(struct raw_finger_data_feed_char_dev
				       *raw_char_dev)
{
	/* clean up */
  return;
}
EXPORT_SYMBOL(rmi_f11_raw_finger_char_dev_unregister);



/*
 * rmi_f11_raw_finger_data_char_dev_register - register char device
 * called from: rmi_fn_54_user_buffer_store()
 *
 * @phy: a pointer to an rmi_phys_devices structure
 *
 * @return: zero if suceeds
 */
static int rmi_f11_raw_finger_data_char_dev_register(
	struct f11_data *rmi_f11_instance_data)
{
	dev_t dev_no;
	int err;
	int result;
	struct device *device_ptr;
	struct raw_finger_data_feed_char_dev *char_dev;

	if (!rmi_f11_instance_data) {
	  pr_info("%s: No RMI F11 data structure instance to attach to!!!\n",
		  __func__);
	}

	pr_debug("%s: Major number of rmi_f11_raw_finger_data_dev: %d\n",
		__func__, rmi_f11_raw_finger_data_dev_major_num);

	if (rmi_f11_raw_finger_data_dev_major_num) {
		dev_no = MKDEV(rmi_f11_raw_finger_data_dev_major_num, 0);
		result = register_chrdev_region(dev_no, 1, RAW_FINGER_DATA_CHAR_DEVICE_NAME);
	} else {
		result = alloc_chrdev_region(&dev_no, 0, 1,
					     RAW_FINGER_DATA_CHAR_DEVICE_NAME);
		/* let kernel allocate a major for us */
		rmi_f11_raw_finger_data_dev_major_num = MAJOR(dev_no);
	}
	pr_debug("%s: Major nmbr of rmi_f11_raw_finger_data_dev_major_num:%d\n",
		__func__,  rmi_f11_raw_finger_data_dev_major_num);

	if (result < 0)
		return result;

	/*
	** allocate device space
	*/
	char_dev = kzalloc(sizeof(struct raw_finger_data_feed_char_dev),
			   GFP_KERNEL);
	if (!char_dev) {
	  pr_err("%s: Failed to allocate raw_finger_data_feed_char_dev.\n",
		  __func__);
		/* unregister the char device region */
		__unregister_chrdev(rmi_f11_raw_finger_data_dev_major_num,
				    MINOR(dev_no), 1,
				    RAW_FINGER_DATA_CHAR_DEVICE_NAME);
		return -ENOMEM;
	}

	mutex_init(&char_dev->mutex_file_op);

	rmi_f11_instance_data->finger_data_feed = char_dev;


	/*
	**  initialize the device
	*/
	cdev_init(&char_dev->raw_data_dev, &rmi_f11_raw_finger_data_dev_fops);


	char_dev->raw_data_dev.owner = THIS_MODULE;

	/*
	** tell the linux kernel to add the device
	*/
	err = cdev_add(&char_dev->raw_data_dev, dev_no, 1);

	if (err) {
	  pr_err("%s: Error %d adding raw_data_char_dev.\n", __func__, err);
		return err;
	}

	/* create device node */
	rmi_f11_instance_data->finger_data_feed->raw_data_device_class =
		class_create(THIS_MODULE, RAW_FINGER_DATA_CHAR_DEVICE_NAME);


	if (IS_ERR(rmi_f11_instance_data->finger_data_feed->
		   raw_data_device_class)) {
	  pr_err("%s: Failed to create /dev/%s.\n", __func__,
			RAW_FINGER_DATA_CHAR_DEVICE_NAME);
		return -ENODEV;
		}

	/* class creation */
	device_ptr = device_create(
			rmi_f11_instance_data->finger_data_feed->
			raw_data_device_class,
			NULL, dev_no, NULL,
			RAW_FINGER_DATA_CHAR_DEVICE_NAME"%d",
			MINOR(dev_no));
	if (IS_ERR(device_ptr)) {
		pr_err("Failed to create raw_data_read device.\n");
		return -ENODEV;
	}

	rmi_f11_instance_data->finger_data_feed->my_parents_instance_data =
		rmi_f11_instance_data;

	return 0;
}

/* Control sysfs files */
/*show_store_union_struct_unsigned(dev_controls, ctrl0_9, abs_pos_filt)
show_store_union_struct_unsigned(dev_controls, ctrl29_30, z_touch_threshold)
show_store_union_struct_unsigned(dev_controls, ctrl29_30, z_touch_hysteresis)*/

module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Christopher Heiny <cheiny@synaptics.com");
MODULE_DESCRIPTION("RMI F11 module");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_GEOFENCE_DEF_H_
#define _MTK_GEOFENCE_DEF_H_

#define GEOFENCE_TRANSITION_ENTERED     (1L<<0)
#define GEOFENCE_TRANSITION_EXITED      (1L<<1)
#define GEOFENCE_TRANSITION_UNCERTAIN   (1L<<2)

#define GEOFENCE_MONITOR_STATUS_UNAVAILABLE (1L<<0)
#define GEOFENCE_MONITOR_STATUS_AVAILABLE   (1L<<1)

#define GEOFENCE_RESULT_SUCCESS                       0
#define GEOFENCE_RESULT_ERROR                        -1
#define GEOFENCE_RESULT_INSUFFICIENT_MEMORY          -2
#define GEOFENCE_RESULT_TOO_MANY_GEOFENCES           -3
#define GEOFENCE_RESULT_ID_EXISTS                    -4
#define GEOFENCE_RESULT_ID_UNKNOWN                   -5
#define GEOFENCE_RESULT_INVALID_GEOFENCE_TRANSITION  -6


struct geofence_area {
	int32_t geofence_id;
	double latitude;
	double longitude;
	double radius_meters;
	int32_t last_transition;
	int32_t monitor_transitions;
	int32_t notification_responsiveness_ms;
	int32_t unknown_timer_ms;
};

struct geo_gnss_location {
	uint32_t flags;
	double lat;
	double lng;
	double alt;
	float speed;
	float bearing;
	float h_accuracy;  //horizontal
	float v_accuracy;  //vertical
	float s_accuracy;  //spedd
	float b_accuracy;  //bearing
	int64_t timestamp;
	uint32_t fix_type;
	int64_t utc_time;
};

struct scp2hal_geofence_ack {
	int32_t geofence_id;
	int32_t result;
};

struct scp2hal_geofence_transition {
	int32_t geofence_id;
	struct geo_gnss_location location;
	int32_t transition;
	int64_t timestamp;
};

struct scp2hal_geofence_status {
	int32_t status;
	struct geo_gnss_location last_location;
};

struct geofence_resume_option {
	int32_t geofence_id;
	int32_t monitor_transitions;
};

/****************************************************/
/* Message Definition */
/****************************************************/
enum conn_geofence_hal2scp_msg_id {
	GEOFENCE_HAL2SCP_NONE = 0,
	GEOFENCE_HAL2SCP_INIT,
	GEOFENCE_HAL2SCP_ADD_AREA,
	GEOFENCE_HAL2SCP_REMOVE_AREA,
	GEOFENCE_HAL2SCP_PAUSE,
	GEOFENCE_HAL2SCP_RESUME,
};

enum conn_geofence_scp2hal_msg_id {
	GEOFENCE_SCP2HAL_NONE = 0,
	GEOFENCE_SCP2HAL_ADD_AREA_ACK,
	GEOFENCE_SCP2HAL_REMOVE_AREA_ACK,
	GEOFENCE_SCP2HAL_TRANSITION_CB,
	GEOFENCE_SCP2HAL_STATUS_CB,
	GEOFENCE_SCP2HAL_PAUSE_ACK,
	GEOFENCE_SCP2HAL_RESUME_ACK,
	GEOFENCE_SCP2HAL_RESTART,
};


#endif // _MTK_GEOFENCE_DEF_H_

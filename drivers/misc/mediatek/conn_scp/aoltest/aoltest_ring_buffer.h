/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __AOLTEST_RING_BUFFER_H__
#define __AOLTEST_RING_BUFFER_H__

#include <linux/spinlock.h>
#include <linux/completion.h>

#define CORE_OP_SZ          8
#define MAC_ADDR_LEN        6
#define ELEM_MAX_LEN_SSID   32
#define WIFI_MAX_FRAMES     12
#define BT_MAX_ADDR         6
#define BT_MAX_FRAMES       12

struct wlan_sensorhub_beacon_frame {
	uint16_t ucRcpiValue;               /* Rcpi */
	uint8_t aucBSSID[MAC_ADDR_LEN];     /* BSSID */
	uint8_t ucChannel;                  /* Channel */
	uint8_t ucBand;                     /* 0: 2G4, 1: 5G, 2: 6G*/
	uint8_t aucSsid[ELEM_MAX_LEN_SSID]; /* Ssid */
};

struct aoltest_wifi_raw_data {
	uint32_t result; // scan result
	uint32_t timestamp;
	uint32_t size;
	struct wlan_sensorhub_beacon_frame frames[WIFI_MAX_FRAMES];
};

struct ll_ext_adv_report_event {
	uint8_t addr[BT_MAX_ADDR];
	uint8_t rssi;
	uint8_t reserved;
};

struct aoltest_bt_raw_data {
	uint32_t result;
	uint32_t timestamp;
	uint32_t size;
	struct ll_ext_adv_report_event events[BT_MAX_FRAMES];
};

struct gnss_gfnc_location {
	uint32_t flags;
	double lat;
	double lng;
	double alt;
	float speed;
	float bearing;
	float h_accuracy;  //horizontal
	float v_accuracy;  //vertical
	float s_accuracy;  //speed
	float b_accuracy;  //bearing
	int64_t timestamp;  //Milliseconds since January 1, 1970
};

struct aoltest_gps_raw_data {
	uint32_t result;
	struct gnss_gfnc_location location;
};

union aoltest_raw_data {
	struct aoltest_wifi_raw_data wifi_raw;
	struct aoltest_bt_raw_data bt_raw;
	struct aoltest_gps_raw_data gps_raw;
};

struct aoltest_rb_data {
	unsigned int type;
	union aoltest_raw_data raw_data;
	struct completion comp;
};

struct aoltest_core_rb_q {
	uint32_t write;
	uint32_t read;
	uint32_t size;
	spinlock_t lock;
	struct aoltest_rb_data *queue[CORE_OP_SZ];
};

struct aoltest_core_rb {
	spinlock_t lock;
	struct aoltest_rb_data queue[CORE_OP_SZ];
	struct aoltest_core_rb_q freeQ;
	struct aoltest_core_rb_q activeQ;
};

int aoltest_core_rb_init(struct aoltest_core_rb *rb);
int aoltest_core_rb_deinit(struct aoltest_core_rb *rb);

struct aoltest_rb_data *aoltest_core_rb_pop_free(struct aoltest_core_rb *rb);
struct aoltest_rb_data *aoltest_core_rb_pop_active(struct aoltest_core_rb *rb);
void aoltest_core_rb_push_free(struct aoltest_core_rb *rb, struct aoltest_rb_data *data);
void aoltest_core_rb_push_active(struct aoltest_core_rb *rb, struct aoltest_rb_data *data);

int aoltest_core_rb_has_pending_data(struct aoltest_core_rb *rb);

#endif

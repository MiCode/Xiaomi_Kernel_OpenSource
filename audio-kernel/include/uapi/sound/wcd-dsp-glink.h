#ifndef _WCD_DSP_GLINK_H
#define _WCD_DSP_GLINK_H

#include <linux/types.h>

#define WDSP_CH_NAME_MAX_LEN 50

enum {
	WDSP_REG_PKT = 1,
	WDSP_CMD_PKT,
	WDSP_READY_PKT,
};
#define WDSP_READY_PKT WDSP_READY_PKT

/*
 * struct wdsp_reg_pkt -  Glink channel information structure format
 * @no_of_channels:   Number of glink channels to open
 * @payload[0]:       Dynamic array contains all the glink channels information
 */
struct wdsp_reg_pkt {
	__u8 no_of_channels;
	__u8 payload[0];
};

/*
 * struct wdsp_cmd_pkt - WDSP command packet format
 * @ch_name:         Name of the glink channel
 * @payload_size:    Size of the payload
 * @payload[0]:      Actual data payload
 */
struct wdsp_cmd_pkt {
	char ch_name[WDSP_CH_NAME_MAX_LEN];
	__u32 payload_size;
	__u8 payload[0];
};

/*
 * struct wdsp_write_pkt - Format that userspace send the data to driver.
 * @pkt_type:      Type of the packet(REG or CMD PKT)
 * @payload[0]:    Payload is either cmd or reg pkt structure based on pkt type
 */
struct wdsp_write_pkt {
	__u8 pkt_type;
	__u8 payload[0];
};

/*
 * struct wdsp_glink_ch_cfg - Defines the glink channel configuration.
 * @ch_name:           Name of the glink channel
 * @latency_in_us:     Latency specified in micro seconds for QOS
 * @no_of_intents:     Number of intents prequeued
 * @intents_size[0]:   Dynamic array to specify size of each intent
 */
struct wdsp_glink_ch_cfg {
	char name[WDSP_CH_NAME_MAX_LEN];
	__u32 latency_in_us;
	__u32 no_of_intents;
	__u32 intents_size[0];
};
#endif /* _WCD_DSP_GLINK_H */

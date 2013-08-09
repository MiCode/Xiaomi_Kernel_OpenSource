#ifndef _UAPI_MSM_RMNET_H_
#define _UAPI_MSM_RMNET_H_

/* Bitmap macros for RmNET driver operation mode. */
#define RMNET_MODE_NONE     (0x00)
#define RMNET_MODE_LLP_ETH  (0x01)
#define RMNET_MODE_LLP_IP   (0x02)
#define RMNET_MODE_QOS      (0x04)
#define RMNET_MODE_MASK     (RMNET_MODE_LLP_ETH | \
			     RMNET_MODE_LLP_IP  | \
			     RMNET_MODE_QOS)

#define RMNET_IS_MODE_QOS(mode)  \
	((mode & RMNET_MODE_QOS) == RMNET_MODE_QOS)
#define RMNET_IS_MODE_IP(mode)   \
	((mode & RMNET_MODE_LLP_IP) == RMNET_MODE_LLP_IP)

/* IOCTL command enum
 * Values chosen to not conflict with other drivers in the ecosystem */
enum rmnet_ioctl_cmds_e {
	RMNET_IOCTL_SET_LLP_ETHERNET = 0x000089F1, /* Set Ethernet protocol  */
	RMNET_IOCTL_SET_LLP_IP       = 0x000089F2, /* Set RAWIP protocol     */
	RMNET_IOCTL_GET_LLP          = 0x000089F3, /* Get link protocol      */
	RMNET_IOCTL_SET_QOS_ENABLE   = 0x000089F4, /* Set QoS header enabled */
	RMNET_IOCTL_SET_QOS_DISABLE  = 0x000089F5, /* Set QoS header disabled*/
	RMNET_IOCTL_GET_QOS          = 0x000089F6, /* Get QoS header state   */
	RMNET_IOCTL_GET_OPMODE       = 0x000089F7, /* Get operation mode     */
	RMNET_IOCTL_OPEN             = 0x000089F8, /* Open transport port    */
	RMNET_IOCTL_CLOSE            = 0x000089F9, /* Close transport port   */
	RMNET_IOCTL_FLOW_ENABLE	     = 0x000089FA, /* Flow enable	     */
	RMNET_IOCTL_FLOW_DISABLE     = 0x000089FB, /* Flow disable	     */
	RMNET_IOCTL_FLOW_SET_HNDL    = 0x000089FC, /* Set flow handle        */
/* RmNet Data Required IOCTLs */
	RMNET_IOCTL_GET_SUPPORTED_FEATURES   = 0x00008A00, /* Get features    */
	RMNET_IOCTL_SET_MRU                  = 0x00008A01, /* Set MRU         */
	RMNET_IOCTL_GET_MRU                  = 0x00008A02, /* Get MRU         */
	RMNET_IOCTL_GET_EPID                 = 0x00008A03, /* Get endpoint ID */
	RMNET_IOCTL_GET_DRIVER_NAME          = 0x00008A04, /* Get driver name */
	RMNET_IOCTL_ADD_MUX_CHANNEL          = 0x00008A05, /* Add MUX ID      */
	RMNET_IOCTL_SET_EGRESS_DATA_FORMAT   = 0x00008A06, /* Set EDF         */
	RMNET_IOCTL_SET_INGRESS_DATA_FORMAT  = 0x00008A07, /* Set IDF         */
	RMNET_IOCTL_SET_AGGREGATION_COUNT    = 0x00008A08, /* Set agg count   */
	RMNET_IOCTL_GET_AGGREGATION_COUNT    = 0x00008A09, /* Get agg count   */
	RMNET_IOCTL_SET_AGGREGATION_SIZE     = 0x00008A0A, /* Set agg size    */
	RMNET_IOCTL_GET_AGGREGATION_SIZE     = 0x00008A0B, /* Get agg size    */
	RMNET_IOCTL_FLOW_CONTROL             = 0x00008A0C, /* Do flow control */
	RMNET_IOCTL_GET_DFLT_CONTROL_CHANNEL = 0x00008A0D, /* For legacy use  */
	RMNET_IOCTL_GET_HWSW_MAP             = 0x00008A0E, /* Get HW/SW map   */
	RMNET_IOCTL_SET_RX_HEADROOM          = 0x00008A0F, /* RX Headroom     */
	RMNET_IOCTL_GET_EP_PAIR              = 0x00008A10, /* Endpoint pair   */
	RMNET_IOCTL_MAX
};

/* Return values for the RMNET_IOCTL_GET_SUPPORTED_FEATURES IOCTL */
#define RMNET_IOCTL_FEAT_NOTIFY_MUX_CHANNEL              (1<<0)
#define RMNET_IOCTL_FEAT_SET_EGRESS_DATA_FORMAT          (1<<1)
#define RMNET_IOCTL_FEAT_SET_INGRESS_DATA_FORMAT         (1<<2)
#define RMNET_IOCTL_FEAT_SET_AGGREGATION_COUNT           (1<<3)
#define RMNET_IOCTL_FEAT_GET_AGGREGATION_COUNT           (1<<4)
#define RMNET_IOCTL_FEAT_SET_AGGREGATION_SIZE            (1<<5)
#define RMNET_IOCTL_FEAT_GET_AGGREGATION_SIZE            (1<<6)
#define RMNET_IOCTL_FEAT_FLOW_CONTROL                    (1<<7)
#define RMNET_IOCTL_FEAT_GET_DFLT_CONTROL_CHANNEL        (1<<8)
#define RMNET_IOCTL_FEAT_GET_HWSW_MAP                    (1<<9)

/* Input values for the RMNET_IOCTL_SET_EGRESS_DATA_FORMAT IOCTL  */
#define RMNET_IOCTL_EGRESS_FORMAT_MAP                  (1<<1)
#define RMNET_IOCTL_EGRESS_FORMAT_AGGREGATION          (1<<2)
#define RMNET_IOCTL_EGRESS_FORMAT_MUXING               (1<<3)
#define RMNET_IOCTL_EGRESS_FORMAT_CHECKSUM             (1<<4)

/* Input values for the RMNET_IOCTL_SET_INGRESS_DATA_FORMAT IOCTL */
#define RMNET_IOCTL_INGRESS_FORMAT_MAP                 (1<<1)
#define RMNET_IOCTL_INGRESS_FORMAT_DEAGGREGATION       (1<<2)
#define RMNET_IOCTL_INGRESS_FORMAT_DEMUXING            (1<<3)
#define RMNET_IOCTL_INGRESS_FORMAT_CHECKSUM            (1<<4)

/* Input values for the RMNET_IOCTL_ADD_MUX_CHANNEL IOCTL         */
struct rmnet_mux_val_s {
	uint32_t     mux_id;
	const char   *vchannel_name;
};

/* Input values for the RMNET_IOCTL_FLOW_CONTROL IOCTL            */
struct flow_control_prop_s {
	uint8_t   flow_mode;
	uint8_t   mux_id;
};

/* Return values for RMNET_IOCTL_GET_EP_PAIR */
struct ipa_ep_pair_s {
	uint32_t   consumer_pipe_num;
	uint32_t   producer_pipe_num;
};

/* QMI QoS header definition */
#define QMI_QOS_HDR_S  __attribute((__packed__)) qmi_qos_hdr_s
struct QMI_QOS_HDR_S {
	unsigned char    version;
	unsigned char    flags;
	unsigned long    flow_id;
};

#endif /* _UAPI_MSM_RMNET_H_ */

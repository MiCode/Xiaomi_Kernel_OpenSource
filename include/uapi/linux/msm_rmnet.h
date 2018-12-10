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

/* IOCTL commands
 * Values chosen to not conflict with other drivers in the ecosystem
 */

#define RMNET_IOCTL_SET_LLP_ETHERNET 0x000089F1 /* Set Ethernet protocol  */
#define RMNET_IOCTL_SET_LLP_IP       0x000089F2 /* Set RAWIP protocol     */
#define RMNET_IOCTL_GET_LLP          0x000089F3 /* Get link protocol      */
#define RMNET_IOCTL_SET_QOS_ENABLE   0x000089F4 /* Set QoS header enabled */
#define RMNET_IOCTL_SET_QOS_DISABLE  0x000089F5 /* Set QoS header disabled*/
#define RMNET_IOCTL_GET_QOS          0x000089F6 /* Get QoS header state   */
#define RMNET_IOCTL_GET_OPMODE       0x000089F7 /* Get operation mode     */
#define RMNET_IOCTL_OPEN             0x000089F8 /* Open transport port    */
#define RMNET_IOCTL_CLOSE            0x000089F9 /* Close transport port   */
#define RMNET_IOCTL_FLOW_ENABLE      0x000089FA /* Flow enable            */
#define RMNET_IOCTL_FLOW_DISABLE     0x000089FB /* Flow disable           */
#define RMNET_IOCTL_FLOW_SET_HNDL    0x000089FC /* Set flow handle        */
#define RMNET_IOCTL_EXTENDED         0x000089FD /* Extended IOCTLs        */

/* RmNet Data Required IOCTLs */
#define RMNET_IOCTL_GET_SUPPORTED_FEATURES     0x0000   /* Get features    */
#define RMNET_IOCTL_SET_MRU                    0x0001   /* Set MRU         */
#define RMNET_IOCTL_GET_MRU                    0x0002   /* Get MRU         */
#define RMNET_IOCTL_GET_EPID                   0x0003   /* Get endpoint ID */
#define RMNET_IOCTL_GET_DRIVER_NAME            0x0004   /* Get driver name */
#define RMNET_IOCTL_ADD_MUX_CHANNEL            0x0005   /* Add MUX ID      */
#define RMNET_IOCTL_SET_EGRESS_DATA_FORMAT     0x0006   /* Set EDF         */
#define RMNET_IOCTL_SET_INGRESS_DATA_FORMAT    0x0007   /* Set IDF         */
#define RMNET_IOCTL_SET_AGGREGATION_COUNT      0x0008   /* Set agg count   */
#define RMNET_IOCTL_GET_AGGREGATION_COUNT      0x0009   /* Get agg count   */
#define RMNET_IOCTL_SET_AGGREGATION_SIZE       0x000A   /* Set agg size    */
#define RMNET_IOCTL_GET_AGGREGATION_SIZE       0x000B   /* Get agg size    */
#define RMNET_IOCTL_FLOW_CONTROL               0x000C   /* Do flow control */
#define RMNET_IOCTL_GET_DFLT_CONTROL_CHANNEL   0x000D   /* For legacy use  */
#define RMNET_IOCTL_GET_HWSW_MAP               0x000E   /* Get HW/SW map   */
#define RMNET_IOCTL_SET_RX_HEADROOM            0x000F   /* RX Headroom     */
#define RMNET_IOCTL_GET_EP_PAIR                0x0010   /* Endpoint pair   */
#define RMNET_IOCTL_SET_QOS_VERSION            0x0011   /* 8/6 byte QoS hdr*/
#define RMNET_IOCTL_GET_QOS_VERSION            0x0012   /* 8/6 byte QoS hdr*/
#define RMNET_IOCTL_GET_SUPPORTED_QOS_MODES    0x0013   /* Get QoS modes   */
#define RMNET_IOCTL_SET_SLEEP_STATE            0x0014   /* Set sleep state */
#define RMNET_IOCTL_SET_XLAT_DEV_INFO          0x0015   /* xlat dev name   */
#define RMNET_IOCTL_DEREGISTER_DEV             0x0016   /* Dereg a net dev */
#define RMNET_IOCTL_GET_SG_SUPPORT             0x0017   /* Query sg support*/
#define RMNET_IOCTL_SET_OFFLOAD                0x0018   /* Set IPA offload */

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
#define RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA            (1<<5)

/* Input values for the RMNET_IOCTL_SET_OFFLOAD */
#define RMNET_IOCTL_OFFLOAD_FORMAT_NONE                   (0)
#define RMNET_IOCTL_COALESCING_FORMAT_TCP              (1<<0)
#define RMNET_IOCTL_COALESCING_FORMAT_UDP              (1<<1)

/* User space may not have this defined. */
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

struct rmnet_ioctl_extended_s {
	uint32_t   extended_ioctl;
	union {
		uint32_t data; /* Generic data field for most extended IOCTLs */

		/* Return values for
		 *    RMNET_IOCTL_GET_DRIVER_NAME
		 *    RMNET_IOCTL_GET_DFLT_CONTROL_CHANNEL
		 */
		int8_t if_name[IFNAMSIZ];

		/* Input values for the RMNET_IOCTL_ADD_MUX_CHANNEL IOCTL */
		struct {
			uint32_t  mux_id;
			int8_t    vchannel_name[IFNAMSIZ];
		} rmnet_mux_val;

		/* Input values for the RMNET_IOCTL_FLOW_CONTROL IOCTL */
		struct {
			uint8_t   flow_mode;
			uint8_t   mux_id;
		} flow_control_prop;

		/* Return values for RMNET_IOCTL_GET_EP_PAIR */
		struct {
			uint32_t   consumer_pipe_num;
			uint32_t   producer_pipe_num;
		} ipa_ep_pair;

		struct {
			uint32_t __data; /* Placeholder for legacy data*/
			uint32_t agg_size;
			uint32_t agg_count;
		} ingress_format;

		/* Input values for the RMNET_IOCTL_SET_OFFLOAD */
		struct {
			uint32_t   flags;
			uint8_t    mux_id;
		} offload_params;
	} u;
};

struct rmnet_ioctl_data_s {
	union {
		uint32_t	operation_mode;
		uint32_t	tcm_handle;
	} u;
};

#define RMNET_IOCTL_QOS_MODE_6   (1<<0)
#define RMNET_IOCTL_QOS_MODE_8   (1<<1)

/* QMI QoS header definition */
struct QMI_QOS_HDR_S {
	unsigned char    version;
	unsigned char    flags;
	uint32_t         flow_id;
} __attribute((__packed__));

/* QMI QoS 8-byte header. */
struct qmi_qos_hdr8_s {
	struct QMI_QOS_HDR_S   hdr;
	uint8_t                reserved[2];
} __attribute((__packed__));

#endif /* _UAPI_MSM_RMNET_H_ */

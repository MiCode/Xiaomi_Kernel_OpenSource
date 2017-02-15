#ifndef __UAPI_CAM_DEFS_H__
#define __UAPI_CAM_DEFS_H__

#include <linux/videodev2.h>
#include <linux/types.h>
#include <linux/ioctl.h>


/* camera op codes */
#define CAM_COMMON_OPCODE_BASE                  0
#define CAM_QUERY_CAP                           1
#define CAM_ACQUIRE_DEV                         2
#define CAM_START_DEV                           3
#define CAM_STOP_DEV                            4
#define CAM_CONFIG_DEV                          5
#define CAM_RELEASE_DEV                         6
#define CAM_COMMON_OPCODE_MAX                   7

/* camera handle type */
#define CAM_HANDLE_USER_POINTER                 1
#define CAM_HANDLE_MEM_HANDLE                   2

/**
 * struct cam_control - struct used by ioctl control for camera
 * @op_code:            This is the op code for camera control
 * @size:               control command size
 * @handle_type:        user pointer or shared memory handle
 * @reserved:           reserved field for 64 bit alignment
 * @handle:             control command payload
 */
struct cam_control {
	uint32_t        op_code;
	uint32_t        size;
	uint32_t        handle_type;
	uint32_t        reserved;
	uint64_t        handle;
};

/* camera IOCTL */
#define VIDIOC_CAM_CONTROL \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct cam_control)

/**
 * struct cam_hw_version - Structure for HW version of camera devices
 *
 * @major    : Hardware version major
 * @minor    : Hardware version minor
 * @incr     : Hardware version increment
 * @reserved : reserved for 64 bit aligngment
 */
struct cam_hw_version {
	uint32_t major;
	uint32_t minor;
	uint32_t incr;
	uint32_t reserved;
};

/**
 * struct cam_iommu_handle - Structure for IOMMU handles of camera hw devices
 *
 * @non_secure: Device Non Secure IOMMU handle
 * @secure:     Device Secure IOMMU handle
 *
 */
struct cam_iommu_handle {
	int32_t non_secure;
	int32_t secure;
};

/* camera secure mode */
#define CAM_SECURE_MODE_NON_SECURE             0
#define CAM_SECURE_MODE_SECURE                 1

/* Camera Format Type */
#define CAM_FORMAT_BASE                         0
#define CAM_FORMAT_MIPI_RAW_6                   1
#define CAM_FORMAT_MIPI_RAW_8                   2
#define CAM_FORMAT_MIPI_RAW_10                  3
#define CAM_FORMAT_MIPI_RAW_12                  4
#define CAM_FORMAT_MIPI_RAW_14                  5
#define CAM_FORMAT_MIPI_RAW_16                  6
#define CAM_FORMAT_MIPI_RAW_20                  7
#define CAM_FORMAT_QTI_RAW_8                    8
#define CAM_FORMAT_QTI_RAW_10                   9
#define CAM_FORMAT_QTI_RAW_12                   10
#define CAM_FORMAT_QTI_RAW_14                   11
#define CAM_FORMAT_PLAIN8                       12
#define CAM_FORMAT_PLAIN16_8                    13
#define CAM_FORMAT_PLAIN16_10                   14
#define CAM_FORMAT_PLAIN16_12                   15
#define CAM_FORMAT_PLAIN16_14                   16
#define CAM_FORMAT_PLAIN16_16                   17
#define CAM_FORMAT_PLAIN32_20                   18
#define CAM_FORMAT_PLAIN64                      19
#define CAM_FORMAT_PLAIN128                     20
#define CAM_FORMAT_ARGB                         21
#define CAM_FORMAT_ARGB_10                      22
#define CAM_FORMAT_ARGB_12                      23
#define CAM_FORMAT_ARGB_14                      24
#define CAM_FORMAT_DPCM_10_6_10                 25
#define CAM_FORMAT_DPCM_10_8_10                 26
#define CAM_FORMAT_DPCM_12_6_12                 27
#define CAM_FORMAT_DPCM_12_8_12                 28
#define CAM_FORMAT_DPCM_14_8_14                 29
#define CAM_FORMAT_DPCM_14_10_14                30
#define CAM_FORMAT_NV21                         31
#define CAM_FORMAT_NV12                         32
#define CAM_FORMAT_TP10                         33
#define CAM_FORMAT_YUV422                       34
#define CAM_FORMAT_PD8                          35
#define CAM_FORMAT_PD10                         36
#define CAM_FORMAT_UBWC_NV12                    37
#define CAM_FORMAT_UBWC_NV12_4R                 38
#define CAM_FORMAT_UBWC_TP10                    39
#define CAM_FORMAT_UBWC_P010                    40
#define CAM_FORMAT_PLAIN8_SWAP                  41
#define CAM_FORMAT_PLAIN8_10                    42
#define CAM_FORMAT_PLAIN8_10_SWAP               43
#define CAM_FORMAT_YV12                         44
#define CAM_FORMAT_Y_ONLY                       45
#define CAM_FORMAT_MAX                          46


/* camera packet */

/* camera rotaion */
#define CAM_ROTATE_CW_0_DEGREE                  0
#define CAM_ROTATE_CW_90_DEGREE                 1
#define CAM_RORATE_CW_180_DEGREE                2
#define CAM_ROTATE_CW_270_DEGREE                3

/* camera Color Space */
#define CAM_COLOR_SPACE_BASE                    0
#define CAM_COLOR_SPACE_BT601_FULL              1
#define CAM_COLOR_SPACE_BT601625                2
#define CAM_COLOR_SPACE_BT601525                3
#define CAM_COLOR_SPACE_BT709                   4
#define CAM_COLOR_SPACE_DEPTH                   5
#define CAM_COLOR_SPACE_MAX                     6

/* camera buffer direction */
#define CAM_BUF_INPUT                           1
#define CAM_BUF_OUTPUT                          2

/* camera packet device Type */
#define CAM_PACKET_DEV_BASE                     0
#define CAM_PACKET_DEV_IMG_SENSOR               1
#define CAM_PACKET_DEV_ACTUATOR                 2
#define CAM_PACKET_DEV_COMPANION                3
#define CAM_PACKET_DEV_EEPOM                    4
#define CAM_PACKET_DEV_CSIPHY                   5
#define CAM_PACKET_DEV_OIS                      6
#define CAM_PACKET_DEV_FLASH                    7
#define CAM_PACKET_DEV_FD                       8
#define CAM_PACKET_DEV_JPEG_ENC                 9
#define CAM_PACKET_DEV_JPEG_DEC                 10
#define CAM_PACKET_DEV_VFE                      11
#define CAM_PACKET_DEV_CPP                      12
#define CAM_PACKET_DEV_CSID                     13
#define CAM_PACKET_DEV_ISPIF                    14
#define CAM_PACKET_DEV_IFE                      15
#define CAM_PACKET_DEV_ICP                      16
#define CAM_PACKET_DEV_LRME                     17
#define CAM_PACKET_DEV_MAX                      18


/* constants */
#define CAM_PACKET_MAX_PLANES                   3

/**
 * struct cam_plane_cfg - plane configuration info
 *
 * @width:                      plane width in pixels
 * @height:                     plane height in lines
 * @plane_stride:               plane stride in pixel
 * @slice_height:               slice height in line (not used by ISP)
 *
 */
struct cam_plane_cfg {
	uint32_t                width;
	uint32_t                height;
	uint32_t                plane_stride;
	uint32_t                slice_height;
};

/**
 * struct cam_buf_io_cfg - Buffer io configuration for buffers
 *
 * @mem_handle:                 mem_handle array for the buffers.
 * @offsets:                    offsets for each planes in the buffer
 * @planes:                     per plane information
 * @width:                      main plane width in pixel
 * @height:                     main plane height in lines
 * @format:                     format of the buffer
 * @color_space:                color space for the buffer
 * @color_pattern:              color pattern in the buffer
 * @bpp:                        bit per pixel
 * @rotation:                   rotation information for the buffer
 * @resource_type:              resource type associated with the buffer
 * @fence:                      fence handle
 * @cmd_buf_index:              command buffer index to patch the buffer info
 * @cmd_buf_offset:             offset within the command buffer to patch
 * @flag:                       flags for extra information
 * @direction:                  buffer direction: input or output
 * @padding:                    padding for the structure
 *
 */
struct cam_buf_io_cfg {
	int32_t                         mem_handle[CAM_PACKET_MAX_PLANES];
	uint32_t                        offsets[CAM_PACKET_MAX_PLANES];
	struct cam_plane_cfg            planes[CAM_PACKET_MAX_PLANES];
	uint32_t                        width;
	uint32_t                        height;
	uint32_t                        format;
	uint32_t                        color_space;
	uint32_t                        color_pattern;
	uint32_t                        bpp;
	uint32_t                        rotation;
	uint32_t                        resource_type;
	int32_t                         fence;
	uint32_t                        cmd_buf_index;
	uint32_t                        cmd_buf_offset;
	uint32_t                        flag;
	uint32_t                        direction;
	uint32_t                        padding;
};

/**
 * struct cam_cmd_buf_desc - Command buffer descriptor
 *
 * @mem_handle:                 command buffer handle
 * @offset:                     command start offset
 * @size:                       size of the command buffer in bytes
 * @length:                     used memory in command buffer in bytes
 * @type:                       type of the command buffer
 * @meta_data:                  data type for private command buffer
 *                              between UMD and KMD
 *
 */
struct cam_cmd_buf_desc {
	int32_t                 mem_handle;
	uint32_t                offset;
	uint32_t                size;
	uint32_t                length;
	uint32_t                type;
	uint32_t                meta_data;
};

/**
 * struct cam_packet_header - camera packet header
 *
 * @op_code:                    camera packet opcode
 * @size:                       size of the camera packet in bytes
 * @request_id:                 request id for this camera packet
 * @flags:                      flags for the camera packet
 * @dev_type:                   camera packet device type
 *
 */
struct cam_packet_header {
	uint32_t                op_code;
	uint32_t                size;
	uint64_t                request_id;
	uint32_t                flags;
	uint32_t                dev_type;
};

/**
 * struct cam_patch_desc - Patch structure
 *
 * @dst_buf_hdl:                memory handle for the dest buffer
 * @dst_offset:                 offset byte in the dest buffer
 * @src_buf_hdl:                memory handle for the source buffer
 * @src_offset:                 offset byte in the source buffer
 *
 */
struct cam_patch_desc {
	int32_t                 dst_buf_hdl;
	uint32_t                dst_offset;
	int32_t                 src_buf_hdl;
	uint32_t                src_offset;
};

/**
 * struct cam_packet - cam packet structure
 *
 * @header:                     camera packet header
 * @cmd_buf_offset:             command buffer start offset
 * @num_cmd_buf:                number of the command buffer in the packet
 * @io_config_offset:           buffer io configuration start offset
 * @num_io_configs:             number of the buffer io configurations
 * @patch_offset:               patch offset for the patch structure
 * @num_patches:                number of the patch structure
 * @kmd_cmd_buf_index:          command buffer index which contains extra
 *                                      space for the KMD buffer
 * @kmd_cmd_buf_offset:         offset from the beginning of the command
 *                                      buffer for KMD usage.
 * @payload:                    camera packet payload
 *
 */
struct cam_packet {
	struct cam_packet_header        header;
	uint32_t                        cmd_buf_offset;
	uint32_t                        num_cmd_buf;
	uint32_t                        io_configs_offset;
	uint32_t                        num_io_configs;
	uint32_t                        patch_offset;
	uint32_t                        num_patches;
	uint32_t                        kmd_cmd_buf_index;
	uint32_t                        kmd_cmd_buf_offset;
	uint64_t                        payload[1];

};

/* Release Device */
/**
 * struct cam_release_dev_cmd - control payload for release devices
 *
 * @session_handle:             session handle for the release
 * @dev_handle:                 device handle for the release
 */
struct cam_release_dev_cmd {
	int32_t                 session_handle;
	int32_t                 dev_handle;
};

/* Start/Stop device */
/**
 * struct cam_start_stop_dev_cmd - control payload for start/stop device
 *
 * @session_handle:             session handle for the start/stop command
 * @dev_handle:                 device handle for the start/stop command
 *
 */
struct cam_start_stop_dev_cmd {
	int32_t                 session_handle;
	int32_t                 dev_handle;
};

/* Configure Device */
/**
 * struct cam_config_dev_cmd - command payload for configure device
 *
 * @session_handle:             session handle for the command
 * @dev_handle:                 device handle for the command
 * @offset:                     offset byte in the packet handle.
 * @packet_handle:              packet memory handle for the actual packet:
 *                                      struct cam_packet.
 *
 */
struct cam_config_dev_cmd {
	int32_t                 session_handle;
	int32_t                 dev_handle;
	uint64_t                offset;
	uint64_t                packet_handle;
};

/* Query Device Caps */
/**
 * struct cam_query_cap_cmd - payload for query device capability
 *
 * @size:               handle size
 * @handle_type:        user pointer or shared memory handle
 * @caps_handle:        device specific query command payload
 *
 */
struct cam_query_cap_cmd {
	uint32_t        size;
	uint32_t        handle_type;
	uint64_t        caps_handle;
};

/* Acquire Device */
/**
 * struct cam_acquire_dev_cmd - control payload for acquire devices
 *
 * @session_handle:     session handle for the acquire command
 * @dev_handle:         device handle to be returned
 * @handle_type:        resource handle type:
 *                             1 = user poniter, 2 = mem handle
 * @num_resources:      number of the resources to be acquired
 * @resources_hdl:      resource handle that refers to the actual
 *                             resource array. Each item in this
 *                             array is device specific resource structure
 *
 */
struct cam_acquire_dev_cmd {
	int32_t         session_handle;
	int32_t         dev_handle;
	uint32_t        handle_type;
	uint32_t        num_resources;
	uint64_t        resource_hdl;
};

#endif /* __UAPI_CAM_DEFS_H__ */

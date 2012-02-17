#ifndef __QSEECOM_H_
#define __QSEECOM_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define MAX_ION_FD  4
#define MAX_APP_NAME_SIZE  32

/*
 * struct qseecom_register_listener_req -
 *      for register listener ioctl request
 * @listener_id - service id (shared between userspace and QSE)
 * @ifd_data_fd - ion handle
 * @virt_sb_base - shared buffer base in user space
 * @sb_size - shared buffer size
 */
struct qseecom_register_listener_req {
	uint32_t listener_id; /* in */
	int32_t ifd_data_fd; /* in */
	uint32_t virt_sb_base; /* in */
	uint32_t sb_size; /* in */
};

/*
 * struct qseecom_send_cmd_req - for send command ioctl request
 * @cmd_req_len - command buffer length
 * @cmd_req_buf - command buffer
 * @resp_len - response buffer length
 * @resp_buf - response buffer
 */
struct qseecom_send_cmd_req {
	void *cmd_req_buf; /* in */
	unsigned int cmd_req_len; /* in */
	void *resp_buf; /* in/out */
	unsigned int resp_len; /* in/out */
};


/*
 * struct qseecom_ion_fd_info - ion fd handle data information
 * @fd - ion handle to some memory allocated in user space
 * @cmd_buf_offset - command buffer offset
 */
struct qseecom_ion_fd_info {
	int32_t fd;
	uint32_t cmd_buf_offset;
};
/*
 * struct qseecom_send_modfd_cmd_req - for send command ioctl request
 * @cmd_req_len - command buffer length
 * @cmd_req_buf - command buffer
 * @resp_len - response buffer length
 * @resp_buf - response buffer
 * @ifd_data_fd - ion handle to memory allocated in user space
 * @cmd_buf_offset - command buffer offset
 */
struct qseecom_send_modfd_cmd_req {
	void *cmd_req_buf; /* in */
	unsigned int cmd_req_len; /* in */
	void *resp_buf; /* in/out */
	unsigned int resp_len; /* in/out */
	struct qseecom_ion_fd_info ifd_data[MAX_ION_FD];
};
/*
 * struct qseecom_listener_send_resp_req - signal to continue the send_cmd req.
 * Used as a trigger from HLOS service to notify QSEECOM that it's done with its
 * operation and provide the response for QSEECOM can continue the incomplete
 * command execution
 * @resp_len - Length of the response
 * @resp_buf - Response buffer where the response of the cmd should go.
 */
struct qseecom_send_resp_req {
	void *resp_buf; /* in */
	unsigned int resp_len; /* in */
};

/*
 * struct qseecom_load_img_data - for sending image length information and
 * ion file descriptor to the qseecom driver. ion file descriptor is used
 * for retrieving the ion file handle and in turn the physical address of
 * the image location.
 * @mdt_len - Length of the .mdt file in bytes.
 * @img_len - Length of the .mdt + .b00 +..+.bxx images files in bytes
 * @ion_fd - Ion file descriptor used when allocating memory.
 * @img_name - Name of the image.
*/
struct qseecom_load_img_req {
	uint32_t mdt_len; /* in */
	uint32_t img_len; /* in */
	int32_t  ifd_data_fd; /* in */
	char	 img_name[MAX_APP_NAME_SIZE]; /* in */
	int app_id; /* out*/
};

struct qseecom_set_sb_mem_param_req {
	int32_t ifd_data_fd; /* in */
	uint32_t virt_sb_base; /* in */
	uint32_t sb_len; /* in */
};

/*
 * struct qseecom_qseos_version_req - get qseos version
 * @qseos_version - version number
 */
struct qseecom_qseos_version_req {
	unsigned int qseos_version; /* in */
};

#define QSEECOM_IOC_MAGIC    0x97


#define QSEECOM_IOCTL_REGISTER_LISTENER_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 1, struct qseecom_register_listener_req)

#define QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ \
	_IO(QSEECOM_IOC_MAGIC, 2)

#define QSEECOM_IOCTL_SEND_CMD_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 3, struct qseecom_send_cmd_req)

#define QSEECOM_IOCTL_SEND_MODFD_CMD_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 4, struct qseecom_send_modfd_cmd_req)

#define QSEECOM_IOCTL_RECEIVE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 5)

#define QSEECOM_IOCTL_SEND_RESP_REQ \
	_IO(QSEECOM_IOC_MAGIC, 6)

#define QSEECOM_IOCTL_LOAD_APP_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 7, struct qseecom_load_img_req)

#define QSEECOM_IOCTL_SET_MEM_PARAM_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 8, struct qseecom_set_sb_mem_param_req)

#define QSEECOM_IOCTL_UNLOAD_APP_REQ \
	_IO(QSEECOM_IOC_MAGIC, 9)

#define QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 10, struct qseecom_qseos_version_req)

#define QSEECOM_IOCTL_PERF_ENABLE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 11)

#define QSEECOM_IOCTL_PERF_DISABLE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 12)

#endif /* __QSEECOM_H_ */

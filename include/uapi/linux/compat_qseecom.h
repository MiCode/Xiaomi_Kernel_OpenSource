#ifndef _UAPI_COMPAT_QSEECOM_H_
#define _UAPI_COMPAT_QSEECOM_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>

/*
 * struct compat_qseecom_register_listener_req -
 *      for register listener ioctl request
 * @listener_id - service id (shared between userspace and QSE)
 * @ifd_data_fd - ion handle
 * @virt_sb_base - shared buffer base in user space
 * @sb_size - shared buffer size
 */
struct compat_qseecom_register_listener_req {
	compat_ulong_t listener_id; /* in */
	compat_long_t ifd_data_fd; /* in */
	compat_uptr_t virt_sb_base; /* in */
	compat_ulong_t sb_size; /* in */
};

/*
 * struct compat_qseecom_send_cmd_req - for send command ioctl request
 * @cmd_req_len - command buffer length
 * @cmd_req_buf - command buffer
 * @resp_len - response buffer length
 * @resp_buf - response buffer
 */
struct compat_qseecom_send_cmd_req {
	compat_uptr_t cmd_req_buf; /* in */
	compat_uint_t cmd_req_len; /* in */
	compat_uptr_t resp_buf; /* in/out */
	compat_uint_t resp_len; /* in/out */
};

/*
 * struct qseecom_ion_fd_info - ion fd handle data information
 * @fd - ion handle to some memory allocated in user space
 * @cmd_buf_offset - command buffer offset
 */
struct compat_qseecom_ion_fd_info {
	compat_long_t fd;
	compat_ulong_t cmd_buf_offset;
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
struct compat_qseecom_send_modfd_cmd_req {
	compat_uptr_t cmd_req_buf; /* in */
	compat_uint_t cmd_req_len; /* in */
	compat_uptr_t resp_buf; /* in/out */
	compat_uint_t resp_len; /* in/out */
	struct compat_qseecom_ion_fd_info ifd_data[MAX_ION_FD];
};

/*
 * struct compat_qseecom_listener_send_resp_req
 * signal to continue the send_cmd req.
 * Used as a trigger from HLOS service to notify QSEECOM that it's done with its
 * operation and provide the response for QSEECOM can continue the incomplete
 * command execution
 * @resp_len - Length of the response
 * @resp_buf - Response buffer where the response of the cmd should go.
 */
struct compat_qseecom_send_resp_req {
	compat_uptr_t resp_buf; /* in */
	compat_uint_t resp_len; /* in */
};

/*
 * struct compat_qseecom_load_img_data
 * for sending image length information and
 * ion file descriptor to the qseecom driver. ion file descriptor is used
 * for retrieving the ion file handle and in turn the physical address of
 * the image location.
 * @mdt_len - Length of the .mdt file in bytes.
 * @img_len - Length of the .mdt + .b00 +..+.bxx images files in bytes
 * @ion_fd - Ion file descriptor used when allocating memory.
 * @img_name - Name of the image.
*/
struct compat_qseecom_load_img_req {
	compat_ulong_t mdt_len; /* in */
	compat_ulong_t img_len; /* in */
	compat_long_t  ifd_data_fd; /* in */
	char	 img_name[MAX_APP_NAME_SIZE]; /* in */
	compat_int_t app_id; /* out*/
};

struct compat_qseecom_set_sb_mem_param_req {
	compat_long_t ifd_data_fd; /* in */
	compat_uptr_t virt_sb_base; /* in */
	compat_ulong_t sb_len; /* in */
};

/*
 * struct compat_qseecom_qseos_version_req - get qseos version
 * @qseos_version - version number
 */
struct compat_qseecom_qseos_version_req {
	compat_uint_t qseos_version; /* in */
};

/*
 * struct compat_qseecom_qseos_app_load_query - verify if app is loaded in qsee
 * @app_name[MAX_APP_NAME_SIZE]-  name of the app.
 * @app_id - app id.
 */
struct compat_qseecom_qseos_app_load_query {
	char app_name[MAX_APP_NAME_SIZE]; /* in */
	compat_int_t app_id; /* out */
};

struct compat_qseecom_send_svc_cmd_req {
	compat_ulong_t cmd_id;
	compat_uptr_t cmd_req_buf; /* in */
	compat_uint_t cmd_req_len; /* in */
	compat_uptr_t resp_buf; /* in/out */
	compat_uint_t resp_len; /* in/out */
};

struct compat_qseecom_create_key_req {
	unsigned char hash32[QSEECOM_HASH_SIZE];
	enum qseecom_key_management_usage_type usage;
};

struct compat_qseecom_wipe_key_req {
	enum qseecom_key_management_usage_type usage;
	compat_int_t wipe_key_flag;
};

struct compat_qseecom_update_key_userinfo_req {
	unsigned char current_hash32[QSEECOM_HASH_SIZE];
	unsigned char new_hash32[QSEECOM_HASH_SIZE];
	enum qseecom_key_management_usage_type usage;
};

/*
 * struct compat_qseecom_save_partition_hash_req
 * @partition_id - partition id.
 * @hash[SHA256_DIGEST_LENGTH] -  sha256 digest.
 */
struct compat_qseecom_save_partition_hash_req {
	compat_int_t partition_id; /* in */
	char digest[SHA256_DIGEST_LENGTH]; /* in */
};

/*
 * struct compat_qseecom_is_es_activated_req
 * @is_activated - 1=true , 0=false
 */
struct compat_qseecom_is_es_activated_req {
	compat_int_t is_activated; /* out */
};

/*
 * struct compat_qseecom_mdtp_cipher_dip_req
 * @in_buf - input buffer
 * @in_buf_size - input buffer size
 * @out_buf - output buffer
 * @out_buf_size - output buffer size
 * @direction - 0=encrypt, 1=decrypt
 */
struct compat_qseecom_mdtp_cipher_dip_req {
	compat_uptr_t in_buf;
	compat_uint_t in_buf_size;
	compat_uptr_t out_buf;
	compat_uint_t out_buf_size;
	compat_uint_t direction;
};

/*
 * struct qseecom_send_modfd_resp - for send command ioctl request
 * @req_len - command buffer length
 * @req_buf - command buffer
 * @ifd_data_fd - ion handle to memory allocated in user space
 * @cmd_buf_offset - command buffer offset
 */
struct compat_qseecom_send_modfd_listener_resp {
	compat_uptr_t resp_buf_ptr; /* in */
	compat_uint_t resp_len; /* in */
	struct compat_qseecom_ion_fd_info ifd_data[MAX_ION_FD]; /* in */
};

struct compat_qseecom_qteec_req {
	compat_uptr_t req_ptr;
	compat_ulong_t req_len;
	compat_uptr_t resp_ptr;
	compat_ulong_t resp_len;
};

struct compat_qseecom_qteec_modfd_req {
	compat_uptr_t req_ptr;
	compat_ulong_t req_len;
	compat_uptr_t resp_ptr;
	compat_ulong_t resp_len;
	struct compat_qseecom_ion_fd_info ifd_data[MAX_ION_FD];
};

struct file;
extern long compat_qseecom_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg);

#define COMPAT_QSEECOM_IOCTL_REGISTER_LISTENER_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 1, struct compat_qseecom_register_listener_req)

#define COMPAT_QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ \
	_IO(QSEECOM_IOC_MAGIC, 2)

#define COMPAT_QSEECOM_IOCTL_SEND_CMD_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 3, struct compat_qseecom_send_cmd_req)

#define COMPAT_QSEECOM_IOCTL_SEND_MODFD_CMD_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 4, struct compat_qseecom_send_modfd_cmd_req)

#define COMPAT_QSEECOM_IOCTL_RECEIVE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 5)

#define COMPAT_QSEECOM_IOCTL_SEND_RESP_REQ \
	_IO(QSEECOM_IOC_MAGIC, 6)

#define COMPAT_QSEECOM_IOCTL_LOAD_APP_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 7, struct compat_qseecom_load_img_req)

#define COMPAT_QSEECOM_IOCTL_SET_MEM_PARAM_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 8, struct compat_qseecom_set_sb_mem_param_req)

#define COMPAT_QSEECOM_IOCTL_UNLOAD_APP_REQ \
	_IO(QSEECOM_IOC_MAGIC, 9)

#define COMPAT_QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 10, struct compat_qseecom_qseos_version_req)

#define COMPAT_QSEECOM_IOCTL_PERF_ENABLE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 11)

#define COMPAT_QSEECOM_IOCTL_PERF_DISABLE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 12)

#define COMPAT_QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 13, struct compat_qseecom_load_img_req)

#define COMPAT_QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ \
	_IO(QSEECOM_IOC_MAGIC, 14)

#define COMPAT_QSEECOM_IOCTL_APP_LOADED_QUERY_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 15, struct compat_qseecom_qseos_app_load_query)

#define COMPAT_QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 16, struct compat_qseecom_send_svc_cmd_req)

#define COMPAT_QSEECOM_IOCTL_CREATE_KEY_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 17, struct compat_qseecom_create_key_req)

#define COMPAT_QSEECOM_IOCTL_WIPE_KEY_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 18, struct compat_qseecom_wipe_key_req)

#define COMPAT_QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 19, \
				struct compat_qseecom_save_partition_hash_req)

#define COMPAT_QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 20, struct compat_qseecom_is_es_activated_req)

#define COMPAT_QSEECOM_IOCTL_SEND_MODFD_RESP \
	_IOWR(QSEECOM_IOC_MAGIC, 21, \
				struct compat_qseecom_send_modfd_listener_resp)

#define COMPAT_QSEECOM_IOCTL_SET_BUS_SCALING_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 23, int)

#define COMPAT_QSEECOM_IOCTL_UPDATE_KEY_USER_INFO_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 24, \
			struct compat_qseecom_update_key_userinfo_req)

#define COMPAT_QSEECOM_QTEEC_IOCTL_OPEN_SESSION_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 30, struct compat_qseecom_qteec_modfd_req)

#define COMPAT_QSEECOM_QTEEC_IOCTL_CLOSE_SESSION_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 31, struct compat_qseecom_qteec_req)

#define COMPAT_QSEECOM_QTEEC_IOCTL_INVOKE_MODFD_CMD_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 32, struct compat_qseecom_qteec_modfd_req)

#define COMPAT_QSEECOM_QTEEC_IOCTL_REQUEST_CANCELLATION_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 33, struct compat_qseecom_qteec_modfd_req)

#define COMPAT_QSEECOM_IOCTL_MDTP_CIPHER_DIP_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 34, struct qseecom_mdtp_cipher_dip_req)

#endif
#endif /* _UAPI_COMPAT_QSEECOM_H_ */


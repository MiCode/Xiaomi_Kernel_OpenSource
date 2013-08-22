/* Qualcomm SMC Module API */

#ifndef __SMCMOD_H_
#define __SMCMOD_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define SMCMOD_DEV "smcmod"

#define SMCMOD_REG_REQ_MAX_ARGS	2

/**
 * struct smcmod_reg_req - for SMC register ioctl request
 *
 * @service_id - requested service.
 * @command_id - requested command.
 * @num_args - number of arguments.
 * @args - argument(s) to be passed to the secure world.
 * @return_val - return value from secure world operation.
 */
struct smcmod_reg_req {
	uint32_t service_id; /* in */
	uint32_t command_id; /* in */
	uint8_t  num_args; /* in */
	uint32_t args[SMCMOD_REG_REQ_MAX_ARGS]; /* in */
	uint32_t return_val; /* out */
};

/**
 * struct smcmod_buf_req - for SMC buffer ioctl request
 *
 * @service_id - requested service.
 * @command_id - requested command.
 * @ion_cmd_fd - fd obtained from ION_IOC_MAP or ION_IOC_SHARE.
 * @cmd_len - length of command data buffer in bytes.
 * @ion_resp_fd - fd obtained from ION_IOC_MAP or ION_IOC_SHARE.
 * @resp_len - length of response data buffer in bytes.
 * @return_val - return value from secure world operation.
 */
struct smcmod_buf_req {
	uint32_t service_id;/* in */
	uint32_t command_id; /* in */
	int32_t ion_cmd_fd; /* in */
	uint32_t cmd_len; /* in */
	int32_t ion_resp_fd; /* in */
	uint32_t resp_len; /* in */
	uint32_t return_val; /* out */
};

/**
 * struct smcmod_cipher_req - for SMC cipher command ioctl
 *
 * @algorithm - specifies the cipher algorithm.
 * @operation - specifies encryption or decryption.
 * @mode - specifies cipher mode.
 * @ion_key_fd - fd obtained form ION_IOC_MAP or ION_IOC_SHARE.
 * @key_size - key size in bytes.
 * @ion_plain_text_fd - fd obtained form ION_IOC_MAP or ION_IOC_SHARE.
 * @plain_text_size - size of plain text in bytes.
 * @ion_cipher_text_fd - fd obtained form ION_IOC_MAP or ION_IOC_SHARE.
 * @cipher_text_size - cipher text size in bytes.
 * @ion_init_vector_fd - fd obtained form ION_IOC_MAP or ION_IOC_SHARE.
 * @init_vector_size - size of initialization vector in bytes.
 * @key_is_null - indicates that the key is null.
 * @return_val - return value from secure world opreation.
 */
struct smcmod_cipher_req {
	uint32_t algorithm; /* in */
	uint32_t operation; /* in */
	uint32_t mode; /* in */
	int32_t ion_key_fd; /* in */
	uint32_t key_size; /* in */
	int32_t ion_plain_text_fd; /* in (encrypt)/out (decrypt) */
	uint32_t plain_text_size; /* in */
	int32_t ion_cipher_text_fd; /* out (encrypt)/in (decrypt) */
	uint32_t cipher_text_size; /* in */
	int32_t ion_init_vector_fd; /* in */
	uint32_t init_vector_size; /* in */
	uint32_t key_is_null; /* in */
	uint32_t return_val; /* out */
};

/**
 * struct smcmod_msg_digest_req - for message digest command ioctl
 *
 * @algorithm - specifies the cipher algorithm.
 * @ion_key_fd - fd obtained form ION_IOC_MAP or ION_IOC_SHARE.
 * @key_size - hash key size in bytes.
 * @ion_input_fd - fd obtained form ION_IOC_MAP or ION_IOC_SHARE.
 * @input_size - input data size in bytes.
 * @ion_output_fd - fd obtained form ION_IOC_MAP or ION_IOC_SHARE.
 * @output_size - size of output buffer in bytes.
 * @fixed_block - indicates whether this is a fixed block digest.
 * @key_is_null - indicates that the key is null.
 * @return_val - return value from secure world opreation.
 */
struct smcmod_msg_digest_req {
	uint32_t algorithm; /* in */
	int32_t ion_key_fd; /* in */
	uint32_t key_size; /* in */
	int32_t ion_input_fd; /* in */
	uint32_t input_size; /* in */
	int32_t ion_output_fd; /* in/out */
	uint32_t output_size; /* in */
	uint32_t fixed_block; /* in */
	uint32_t key_is_null; /* in */
	uint32_t return_val; /* out */
} __packed;

/**
 * struct smcmod_decrypt_req - used to decrypt image fragments.
 * @service_id - requested service.
 * @command_id - requested command.
 * @operation - specifies metadata parsing or image fragment decrypting.
 * @request - describes request parameters depending on operation.
 * @response - this is the response of the request.
 */
struct smcmod_decrypt_req {
	uint32_t service_id;
	uint32_t command_id;
#define SMCMOD_DECRYPT_REQ_OP_METADATA	1
#define SMCMOD_DECRYPT_REQ_OP_IMG_FRAG	2
	uint32_t operation;
	union {
		struct {
			uint32_t len;
			uint32_t ion_fd;
		} metadata;
		struct {
			uint32_t ctx_id;
			uint32_t last_frag;
			uint32_t frag_len;
			uint32_t ion_fd;
			uint32_t offset;
		} img_frag;
	} request;
	union {
		struct {
			uint32_t status;
			uint32_t ctx_id;
			uint32_t end_offset;
		} metadata;
		struct {
			uint32_t status;
		} img_frag;
	} response;
};

#define SMCMOD_IOC_MAGIC	0x97

/* Number chosen to avoid any conflicts */
#define SMCMOD_IOCTL_SEND_REG_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 32, struct smcmod_reg_req)
#define SMCMOD_IOCTL_SEND_BUF_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 33, struct smcmod_buf_req)
#define SMCMOD_IOCTL_SEND_CIPHER_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 34, struct smcmod_cipher_req)
#define SMCMOD_IOCTL_SEND_MSG_DIGEST_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 35, struct smcmod_msg_digest_req)
#define SMCMOD_IOCTL_GET_VERSION _IOWR(SMCMOD_IOC_MAGIC, 36, uint32_t)
#define SMCMOD_IOCTL_SEND_DECRYPT_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 37, struct smcmod_decrypt_req)

#endif /* __SMCMOD_H_ */

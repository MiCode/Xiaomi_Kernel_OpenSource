
#ifndef __TEEI_COMMON_H_
#define __TEEI_COMMON_H_


#define TEEI_MAX_REQ_PARAMS  12
#define TEEI_MAX_RES_PARAMS  8
#define TEEI_1K_SIZE 1024

/**
 * @brief Command status
 */
enum teei_cmd_status {
	TEEI_STATUS_INCOMPLETE = 0,
	TEEI_STATUS_COMPLETE,
	TEEI_STATUS_MAX  = 0x7FFFFFFF
};


/**
 * @brief Parameters type
 */
enum teeic_param_type {
	TEEIC_PARAM_IN = 0,
	TEEIC_PARAM_OUT
};

/**
 * @brief Shared memory for Notification
 */
struct teeic_notify_data {
	int dev_file_id;
	int service_id;
	int client_pid;
	int session_id;
	int enc_id;
};


enum teeic_param_value {
	TEEIC_PARAM_A = 0,
	TEEIC_PARAM_B
};

enum teeic_param_pos {
	TEEIC_PARAM_1ST = 0,
	TEEIC_PARAM_2ND,
	TEEIC_PARAM_3TD,
	TEEIC_PARAM_4TH
};


/**
 * @brief Metadata used for encoding/decoding
 */
struct teei_encode_meta {
	int type;
	int len;			/* data length */
	unsigned  long long usr_addr;		/* data address in user space */
	int ret_len;			/* return sizeof data */
	int value_flag;			/* value of a or b */
	int param_pos;			/* param order */
	int param_pos_type;		/* param type */
};

/**
 * @brief SMC command structure
 */
struct teei_smc_cmd {
	unsigned int    teei_cmd_type;
	unsigned int    id;
	unsigned int    context;
	unsigned int    enc_id;

	unsigned int    src_id;
	unsigned int    src_context;

	unsigned int    req_buf_len;
	unsigned int    resp_buf_len;
	unsigned int    ret_resp_buf_len;
	unsigned int    info_buf_len;
	unsigned int    cmd_status;
	unsigned int    req_buf_phys;
	unsigned int    resp_buf_phys;
	unsigned int    meta_data_phys;
	unsigned int    info_buf_phys;
	unsigned int    dev_file_id;
	unsigned int     error_code;
	struct semaphore *teei_sema;
};
#endif /* __TEEI_COMMON_H_ */

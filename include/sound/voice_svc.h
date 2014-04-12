#ifndef __VOICE_SVC_H__
#define __VOICE_SVC_H__

#include <linux/types.h>
#include <linux/ioctl.h>

#define VOICE_SVC_DRIVER_NAME "voice_svc"

#define VOICE_SVC_MVM_STR "MVM"
#define VOICE_SVC_CVS_STR "CVS"
#define MAX_APR_SERVICE_NAME_LEN  64

#define MSG_REGISTER 0x1
#define MSG_REQUEST  0x2
#define MSG_RESPONSE 0x3

struct voice_svc_write_msg {
	__u32 msg_type;
	__u8 payload[0];
};

struct voice_svc_register {
	char svc_name[MAX_APR_SERVICE_NAME_LEN];
	__u32 src_port;
	__u8 reg_flag;
};

struct voice_svc_cmd_response {
	__u32 src_port;
	__u32 dest_port;
	__u32 token;
	__u32 opcode;
	__u32 payload_size;
	__u8 payload[0];
};

struct voice_svc_cmd_request {
	char svc_name[MAX_APR_SERVICE_NAME_LEN];
	__u32 src_port;
	__u32 dest_port;
	__u32 token;
	__u32 opcode;
	__u32 payload_size;
	__u8 payload[0];
};

#endif

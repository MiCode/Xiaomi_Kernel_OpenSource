#ifndef __VOICE_SVC_H__
#define __VOICE_SVC_H__

#include <linux/types.h>
#include <linux/ioctl.h>

#define VOICE_SVC_DRIVER_NAME "voice_svc"

#define VOICE_SVC_MVM_STR "MVM"
#define VOICE_SVC_CVS_STR "CVS"
#define MAX_APR_SERVICE_NAME_LEN  64

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

#define VOICE_SVC_MAGIC 'N'

#define SNDRV_VOICE_SVC_REGISTER_SVC    _IOWR(VOICE_SVC_MAGIC, \
                    0x01, struct voice_svc_register)
#define SNDRV_VOICE_SVC_CMD_RESPONSE    _IOWR(VOICE_SVC_MAGIC, \
                    0x02, struct voice_svc_cmd_response)
#define SNDRV_VOICE_SVC_CMD_REQUEST    _IOWR(VOICE_SVC_MAGIC, \
                    0x03, struct voice_svc_cmd_request)
#endif

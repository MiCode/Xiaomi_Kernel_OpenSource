/*******************************************************************************
 * @file     hcmd.h
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#ifndef HOSTCOMMANDS_H
#define HOSTCOMMANDS_H

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HCMD_GET_DEVICE_INFO        ((FSC_U8)0x00)
#define HCMD_GET_SET_SERIAL_NUM     ((FSC_U8)0x01)
#define HCMD_QUERY_I2C_FCS_DEVS     ((FSC_U8)0x02)
#define HCMD_SET_IO_CONFIG          ((FSC_U8)0x10)
#define HCMD_GET_IO_CONFIG          ((FSC_U8)0x11)
#define HCMD_SET_I2C_CONFIG         ((FSC_U8)0x12)
#define HCMD_GET_I2C_CONFIG         ((FSC_U8)0x13)
#define HCMD_WRITE_IO_PORT          ((FSC_U8)0x40)
#define HCMD_READ_IO_PORT           ((FSC_U8)0x41)
#define HCMD_READ_ADC_CHANNEL       ((FSC_U8)0x42)
#define HCMD_WRITE_I2C_FCS_DEV      ((FSC_U8)0x60)
#define HCMD_READ_I2C_FCS_DEV       ((FSC_U8)0x61)
#define HCMD_DATA_COLLECT_START     ((FSC_U8)0x80)
#define HCMD_DATA_COLLECT_STOP      ((FSC_U8)0x81)
#define HCMD_DATA_COLLECT_READ      ((FSC_U8)0x82)
#define HCMD_AUDIO_CLASS            ((FSC_U8)0x90)
#define HCMD_USER_CLASS             ((FSC_U8)0x91)
#define HCMD_TYPEC_CLASS            ((FSC_U8)0x92)
#define HCMD_PD_CLASS               ((FSC_U8)0x93)
#define HCMD_DP_CLASS               ((FSC_U8)0x94)
#define HCMD_STATUS_SUCCESS         ((FSC_U8)0x00)
#define HCMD_STATUS_NOT_IMPLEMENTED ((FSC_U8)0x01)
#define HCMD_STATUS_WRONG_VERSION   ((FSC_U8)0x02)
#define HCMD_STATUS_BUSY            ((FSC_U8)0x03)
#define HCMD_STATUS_FAILED          ((FSC_U8)0xFF)

#define TCPD_EOP                    ((FSC_U8)0x00)

typedef enum {
    TYPEC_EOP              = TCPD_EOP,
    TYPEC_BYTE_CMD_START   = TYPEC_EOP,
    TYPEC_ENABLE           = 0x1,
    TYPEC_PORT_TYPE        = 0x2,
    TYPEC_ACC_SUPPORT      = 0x3,
    TYPEC_SRC_PREF         = 0x4,
    TYPEC_SNK_PREF         = 0x5,
    TYPEC_STATE            = 0x6,
    TYPEC_SUBSTATE         = 0x7,
    TYPEC_CC_ORIENT        = 0x8,
    TYPEC_CC_TERM          = 0x9,
    TYPEC_VCON_TERM        = 0xA,
    TYPEC_DFP_CURRENT_AD   = 0xB,
    TYPEC_UFP_CURRENT      = 0xC,
    TYPEC_FRS_MODE         = 0xD,
    TYPEC_DFP_CURRENT_DEF  = 0xE,
    TYPEC_BYTE_CMD_END,

    /** Multibyte commands */
    TYPEC_MULBYT_CMD_START = 0xA0,
    TYPEC_STATE_LOG        = TYPEC_MULBYT_CMD_START,
    TYPEC_MULBYT_CMD_END,
} HCMD_TYPEC_SUB_CMDS;

typedef enum {
    USBPD_EOP               = TCPD_EOP,
    USBPD_BYTE_CMD_START    = USBPD_EOP,
    PD_ENABLE               = 0x1,
    PD_ACTIVE               = 0x2,
    PD_HAS_CONTRACT         = 0x3,
    PD_SPEC_REV             = 0x4,
    PD_PWR_ROLE             = 0x5,
    PD_DATA_ROLE            = 0x6,
    PD_VCON_SRC             = 0x7,
    PD_STATE                = 0x8,
    PD_SUB_STATE            = 0x9,
    PD_PROTOCOL_STATE       = 0xA,
    PD_PROTOCOL_SUB_STATE   = 0xB,
    PD_TX_STATUS            = 0xC,
    PD_SNK_TX_STATUS        = 0xD,
    PD_CAPS_CHANGED         = 0xE,
    PD_HARD_RESET           = 0x14,
    PD_GOTOMIN_COMPAT       = 0x15,
    PD_USB_SUSPEND          = 0x16,
    PD_COM_CAPABLE          = 0x17,
    PD_SVID_ENABLE          = 0x18,
    PD_MODE_ENABLE          = 0x19,
    PD_SVID_AUTO_ENTRY      = 0x1A,
    PD_CABLE_RESET          = 0x1B,
    USBPD_BYTE_CMD_END,

    USBPD_MULBYT_CMD_START  = 0xA0,
    PD_SRC_CAP              = USBPD_MULBYT_CMD_START,
    PD_SNK_CAP              = 0xA1,
    PD_PE_LOG               = 0xA2,
    PD_PD_LOG               = 0xA3,
    PD_MAX_VOLTAGE          = 0xA4,
    PD_OP_POWER             = 0xA5,
    PD_MAX_POWER            = 0xA6,
    PD_PPR_SRC_CAP          = 0xA7,
    PD_PPR_SINK_CAP         = 0xA8,
    PD_MSG_WRITE            = 0xA9,
    PD_VDM_MODES            = 0xAA,
    USBPD_MULBYT_CMD_END,
} HCMD_PD_SUB_CMDS;

typedef enum {
    DP_EOP                  = TCPD_EOP,
    DP_BYTE_CMD_START       = DP_EOP,
    DP_ENABLE               = 0x01,
    DP_AUTO_MODE_ENTRY      = 0x02,
    DP_SEND_STATUS          = 0x03,
    DP_BYTE_CMD_END,

    DP_MULBYT_CMD_START     = 0xA0,
    DP_CAP                  = DP_MULBYT_CMD_START,
    DP_STATUS               = 0xA1,
    DP_MULBYT_CMD_END,
} HCMD_DP_SUB_CMDS;

/**
 * Generic command structure
 */
typedef struct cmd_host_cmd_req {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
        } rsp;
    } cmd;
    FSC_U8 data[60];
} GenericCmdRequest_t;

/**
 * GetDeviceInfo command
 */
typedef struct cmd_get_device_info {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 mcu;
            FSC_U8 device;
            FSC_U8 hostcom[2];
            FSC_U8 config[2];
            FSC_U8 serial[16];
            FSC_U8 fw[3];
        } rsp;
    } cmd;
} CmdGetDeviceInfo_t;

/**
 * SetIOConfig command
 */
typedef struct cmd_set_io_config {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 port;
            FSC_U8 direction[4];
            FSC_U8 analog[4];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
        } rsp;
    } cmd;
} CmdSetIOConfig_t;

/**
 * GetIOConfig command
 */
typedef struct cmd_get_io_config {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 port;
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 port;
            FSC_U8 direction[4];
            FSC_U8 analog[4];
        } rsp;
    } cmd;
} CmdGetIOConfig_t;

/**
 * SetI2C config
 */
typedef struct cmd_set_i2c_config {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 module;
            FSC_U8 enable;
            FSC_U8 setting[4];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
        } rsp;
    } cmd;
} CmdSetI2CConfig_t;

/**
 * GetI2CConfig
 */
typedef struct cmd_get_i2c_config {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[1];
            FSC_U8 module;
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 module;
            FSC_U8 enable;
            FSC_U8 settings[4];
        } rsp;
    } cmd;
} CmdGetI2CConfig_t;

/**
 * Write IO Port command
 */
typedef struct cmd_wr_io_port {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 port;
            FSC_U8 data[4];
            FSC_U8 mask[4];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
        };
    } cmd;
} CmdWrIOPort_t;

/**
 * Read IO Port command
 */
typedef struct cmd_rd_io_port {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 port;
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 port;
            FSC_U8 state[4];
            FSC_U8 mask;
        } rsp;
    } cmd;
} CmdRdIOPort_t;

/**
 * Read ADC Command
 */
typedef struct cmd_rd_adc {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 channel;
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 channel;
            FSC_U8 value[4];
            FSC_U8 ref[4];
        } rsp;
    };
} CmdRdADC_t;

/**
 * Write I2C device register command
 */
typedef struct cmd_wr_i2c_dev {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 module;
            FSC_U8 addr;
            FSC_U8 alen;       /* Address length */
            FSC_U8 dlen;       /* Data length */
            FSC_U8 pktlen;     /* Number of bytes to write */
            FSC_U8 inc;        /* Increment address everyo inc byte written */
            FSC_U8 reg[4];     /* starting register address */
            FSC_U8 data[50];   /* Data to be written. Size: (dlen/inc) bytes */
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
        } rsp;
    } cmd;
} CmdWrI2CDev_t;

/**
 * Read I2C device register command
 */
typedef struct cmd_rd_i2c_dev {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 module;
            FSC_U8 addr;
            FSC_U8 alen;
            FSC_U8 dlen;
            FSC_U8 pktlen;
            FSC_U8 inc;
            FSC_U8 reg[4];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 data[50];
        } rsp;
    } cmd;
} CmdRdI2CDev_t;

/**
 * Data collection start command
 */
typedef struct cmd_data_collect_start {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 reserved[3];
            FSC_U8 module;
            FSC_U8 addr;
            FSC_U8 config;
            FSC_U8 intp[2];
            FSC_U8 num;
            FSC_U8 reg[54];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
        } rsp;
    } cmd;
} CmdDataCollectStart_t;

/**
 * Data collection stop command
 */
typedef struct cmd_data_collect_stop {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
        } rsp;
    } cmd;
} CmdDataCollectStop_t;

/**
 * Query I2C devices command
 */
typedef struct cmd_query_i2c_dev {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 status;
            FSC_U8 reserved1;
            FSC_U8 error;
            FSC_U8 reserved2;
            FSC_U8 num;
            struct {
                FSC_U8 addr;
                FSC_U8 part;
            } dev[29];
        } rsp;
    } cmd;
} CmdQueryI2CDev_t;

/**
 * Audio class generic command
 */
typedef struct cmd_audio_class {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 cmd;
            FSC_U8 reserved[2];
            FSC_U8 payload[60];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 cmd;
            FSC_U8 error;
            FSC_U8 payload[60];
        } rsp;
    } cmd;
} CmdAudioClass_t;

/**
 * User class generic command
 */
typedef struct cmd_user_class {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 id;
            FSC_U8 port;
            FSC_U8 reserved;
            FSC_U8 payload[60];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 id;
            FSC_U8 error;
            FSC_U8 payload[60];
        } rsp;
    } cmd;
} CmdUserClass_t;

typedef struct cmd_typec {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 rw;
            FSC_U8 port;
            FSC_U8 reserved;
            FSC_U8 payload[60];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 payload[60];
        } rsp;
    } cmd;
} CmdTypeC_t;

typedef struct cmd_pd {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 rw;
            FSC_U8 port;
            FSC_U8 reserved;
            FSC_U8 payload[60];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 payload[60];
        } rsp;
    } cmd;
} CmdPd_t;

typedef struct cmd_dp {
    FSC_U8 opcode;
    union {
        struct {
            FSC_U8 rw;
            FSC_U8 port;
            FSC_U8 reserved;
            FSC_U8 payload[60];
        } req;
        struct {
            FSC_U8 status;
            FSC_U8 reserved;
            FSC_U8 error;
            FSC_U8 payload[60];
        } rsp;
    } cmd;
} CmdDp_t;

/**
 * Host command structure
 */
typedef union {
    FSC_U8 byte[64]; /* HID descriptor size */
    GenericCmdRequest_t request;
    CmdGetDeviceInfo_t deviceInfo;
    CmdSetIOConfig_t setIOConfig;
    CmdGetIOConfig_t getIOConfig;
    CmdSetI2CConfig_t setI2CConfig;
    CmdGetI2CConfig_t getI2CConfig;
    CmdWrIOPort_t wrIOPort;
    CmdRdIOPort_t rdIOPort;
    CmdRdADC_t rdADC;
    CmdQueryI2CDev_t queryI2CDev;
    CmdWrI2CDev_t wrI2CDev;
    CmdRdI2CDev_t rdI2CDev;
    CmdDataCollectStart_t dataCollectStart;
    CmdDataCollectStop_t dataCollectStop;
    CmdAudioClass_t audioClass;
    CmdUserClass_t userClass;
    CmdTypeC_t typeC;
    CmdPd_t pd;
    CmdDp_t dp;
} HostCmd_t;

#define HCMD_SIZE           sizeof(HostCmd_t)   /* Total size of buffer */
#define HCMD_PAYLOAD_SIZE   (HCMD_SIZE - 4)     /* 4 byte is used for header */
#ifdef __cplusplus
}
#endif

#endif /* HOSTCOMMANDS_H */

/**
 * @file   drFp_api.h
 * @brief  Contains DCI command definitions and data structures
 *
 * <Copyright goes here>
 */


#ifndef __DRFP_API_H__
#define __DRFP_API_H__


typedef uint32_t dciCommandId_t;
typedef uint32_t dciResponseId_t;
typedef uint32_t dciReturnCode_t;

/**< Responses have bit 31 set */
#define RSP_ID_MASK (1U << 31)
#define RSP_ID(cmdId) (((uint32_t)(cmdId)) | RSP_ID_MASK)
#define IS_CMD(cmdId) ((((uint32_t)(cmdId)) & RSP_ID_MASK) == 0)
#define IS_RSP(cmdId) ((((uint32_t)(cmdId)) & RSP_ID_MASK) == RSP_ID_MASK)


/**
 * Command ID's
  */
#define GF516M_SECDEV_CMD_TESTCASE  1
#define GF516M_SECDEV_CMD_FP_INIT  2
#define GF516M_SECDEV_CMD_READ_FW_VERSION  3
#define GF516M_SECDEV_CMD_RW_ADDR  4
#define GF516M_SECDEV_CMD_IRQ_STATUS  5
#define GF516M_SECDEV_CMD_FORWARD_IRQ  6
#define GF516M_SECDEV_CMD_SWITCH_SPI  7
#define GF516M_SECDEV_CMD_RESET_SAMPLE  8
#define GF516M_SECDEV_CMD_ESD_CHECK   9
#define GF516M_SECDEV_CMD_SEND_SENSOR 10
#define GF516M_SECDEV_CMD_GET_SET_MODE  11
#define GF516M_SECDEV_CMD_UPGRADE_FW_CFG 12
#define GF516M_SECDEV_CMD_CLEAR_IRQ  13
#define GF516M_SECDEV_CMD_READ_NVG_DATA  14


/*... add more command ids when needed */

/**
 * Maximum data length.
 */
 //the longest fp data each time?, need change later according the command
#define MAX_DATA_LEN (2048+5)
#define GF516M_VENDOR_ID_LEN	1

typedef enum {
	GF_NOEVENT = 0,
	GF_KEYDOWN_EVENT,
	GF_KEYUP_EVENT,
	GF_FP_DATA_EVENT,
	GF_HOMEKEY_EVENT,
    GF_REF_DATA_EVENT,
    GF_NVG_DATA_EVENT,
    GF_NVG_REF_DATA_EVENT,
}EVENT_TYPE;

typedef enum{
    GF516M_IMAGE_MODE = 0,
    GF516M_KEY_MODE = 0x01,
    GF516M_SLEEP_MODE = 0x02,
    GF516M_FF_MODE = 0x03,
    GF516M_NVG_MODE = 0x10,
    GF516M_NVG_MODE_NOTX = 0x11,
    GF516M_DEBUG_MODE = 0x56,
    GF516M_DEBUG_MODE_NOTX= 0x58,
    GF516M_UNKNOWN_MODE = 0xFF
}GF516M_MODE;

typedef enum{
    GF_MODE_IMAGE = 0x00,
    GF_MODE_KEY = 0x01,
    GF_MODE_BLANK_FRAME_WITH_TX = 0x02,
    GF_MODE_BLANK_FRAME_WITHOUT_TX = 0x03,
    GF_MODE_NAVIGATION = 0x04,
    GF_MODE_NAVIGATION_NO_TX = 0x05,
    GF_MODE_UNKNOWN = 0xFF
}GF_MODE;

//Jason Sep-30-2015
typedef enum {
    GF_UPDATE_FW_CHECK_CMD = 0x00,
    GF_UPDATE_FW_DAMAGE_CMD = 0x01,
    GF_UPDATE_CFG_CMD = 0x02,
}GF_UPDATE_FW_CFG_CMD;

typedef enum {
    GF_UPDATE_FW_RESULT_NO_NEED_UPDATE,
    GF_UPDATE_FW_RESULT_OK,
    GF_UPDATE_FW_RESULT_FAILED,
    GF_UPDATE_CFG_RESULT_OK,
    GF_UPDATE_CFG_RESULT_FAILED,
}GF_UPDATE_RESULT;


/**
 * DCI command header.
 */
typedef struct{
    dciCommandId_t commandId; /**< Command ID */
} dciCommandHeader_t;

/**
 * DCI response header.
 */
typedef struct{
    dciResponseId_t     responseId; /**< Response ID (must be command ID | RSP_ID_MASK )*/
    dciReturnCode_t     returnCode; /**< Return code of command */
} dciResponseHeader_t;


/**
 * command message.
 *
 * @param len Lenght of the data to process.
 * @param data Data to be processed
 */
typedef struct {
    dciCommandHeader_t  header;     /**< Command header */
    uint32_t            len;        /**< Length of data to process */
} cmd_t;


/**
 * Response structure
 */
typedef struct {
    dciResponseHeader_t header;     /**< Response header */
    uint32_t            len;
} rsp_t;


/**
 * SECDEV_CMD_TESTCASE data structure
 */
typedef struct {
    uint32_t len;
    uint8_t data[64];
} testcase_t;

/**
 * SECDEV_CMD_FP_INIT data structure
 */
typedef struct {
    uint8_t flag;
    uint8_t data[64];
} fpinit_t;

/**
 * SECDEV_CMD_READ_FW_VERSION data structure
 */
typedef struct {
    uint32_t len;
    uint16_t addr;
    uint8_t chipid[10];
    uint8_t fw[10];
    uint8_t vendor_id[GF516M_VENDOR_ID_LEN];	//Jason Aug-13-2015
} readfwVersion_t;

/**
 * SECDEV_CMD_RW_ADDR data structure
 */
typedef struct {
    uint32_t len;    //byte length for gf616
    uint8_t direction;  //0: read, 1: write
    uint8_t speed;   //0: low speed, 1: high speed
    uint8_t flag;      //0: whole 16 bits, 1: LSB 8 bits, 2: MSB 8 bits
    uint8_t reserve;
    uint16_t addr;
    uint8_t data[MAX_DATA_LEN];
} rw_data_t;

/**
 * SECDEV_CMD_IRQ_STATUS data structure
 */
typedef struct {
    uint32_t reg_group;
    uint8_t mode;
    uint8_t status;
     uint8_t status_l;   //Jason Aug-12-2015
} irq_status_t;

/**
 * SECDEV_CMD_FORWARD_IRQ data structure
 */
typedef struct {
    EVENT_TYPE event_type;   //0-invalid, 1-keydown, 2-keyon, 3-fp data ready
    uint8_t notify_application;    //whether notify this interrupt to high layer
    uint8_t mode;
    uint8_t status;
} forward_irq_t;

/**
 * SECDEV_CMD_RW_ADDR data structure
 */
typedef struct {
    uint8_t to_secure;   //0-to non_secure world, 1-to secure world
} switch_spi_t;

/**
 * SECDEV_CMD_RESET_SAMPLE data structure
 */
typedef struct {
    uint8_t do_reset_sample;
} reset_sample_t;

/**
 * SECDEV_CMD_ESD_REGISTER data structure
 */
typedef struct {
    uint8_t cmd;
    uint8_t ret;
} esd_check_t;

/**
 * SECDEV_CMD_SEND_SENSOR data structure
 */
typedef struct {
    uint8_t cmd_num;
     int8_t ret;
} send_sensor_t;

/**
 * SECDEV_CMD_SET_MODE data structure
 */
typedef struct {
    uint8_t flag;
    GF516M_MODE mode;
    uint8_t rw_flag;   //0: read mode, 1: set mode
} set_mode_t;

/**
 * SECDEV_CMD_UPGRADE_FW_CFG data structure
 */
typedef struct {
    uint8_t cmd;
    int32_t ret;     //0: no need upgrade, 2: upgrade success and restart sensor, others: error
} upgrade_fw_cfg_t;

/**
 * SECDEV_CMD_CLEAR_IRQ data structure
 */
typedef struct {
    uint8_t mode;
    uint8_t status;
} clear_irq_t;

/**
 * DCI message data, 1MB maximum.
 * ONLY USED BY ONE CLIENT
 */
typedef struct {
    union {
        cmd_t     command;
        rsp_t     response;
    };

    //TODO: union struct for different command, test temporary
    union {
        testcase_t  testcase;
        fpinit_t fpinit;
        readfwVersion_t  fw_version;
        rw_data_t rw_data;
        irq_status_t irq_status;
        forward_irq_t forward_irq;
        switch_spi_t switch_spi;
        reset_sample_t reset_sample;
        esd_check_t esd_check;
        send_sensor_t send_sensor;
        set_mode_t mode;
        upgrade_fw_cfg_t upgrade_fw_cfg;
        clear_irq_t clear_irq;
    };
} dciMessage_t, *dciMessage_ptr;

/**
 * Trustlet UUID
 */
#define DR_FP_UUID  { { 5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }

#endif // __DRFP_API_H__

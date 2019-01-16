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
#define GF616_SECDEV_CMD_TESTCASE  1
#define GF616_SECDEV_CMD_FP_INIT  2
#define GF616_SECDEV_CMD_READ_FW_VERSION  3
#define GF616_SECDEV_CMD_RW_ADDR  4
#define GF616_SECDEV_CMD_IRQ_STATUS  5
#define GF616_SECDEV_CMD_FORWARD_IRQ  6
#define GF616_SECDEV_CMD_SWITCH_SPI  7
#define GF616_SECDEV_CMD_RESET_SAMPLE  8
#define GF616_SECDEV_CMD_ESD_CHECK   9
#define GF616_SECDEV_CMD_SEND_SENSOR 10
#define GF616_SECDEV_CMD_GET_SET_MODE  11

/*... add more command ids when needed */

/**
 * Maximum data length.
 */
 //the longest fp data each time?, need change later according the command
#define MAX_DATA_LEN (2048+5)

typedef enum {
	GF_NOEVENT = 0,
	GF_KEYDOWN_EVENT,
	GF_KEYUP_EVENT,
	GF_FP_DATA_EVENT,
	GF_HOMEKEY_EVENT,
        GF_REF_DATA_EVENT,
}EVENT_TYPE;

typedef enum{
    HOLD_RELEASE_MCU = 0,
    ONLY_HOLD_MCU,
    ONLY_RELEASE_MCU,
    NO_HOLD_RELEASE_MCU,
    IN_INTERRUPT_STATUS = 8,
    IDLE_STATUS,
}HOLD_MCU_FLAG;

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
    HOLD_MCU_FLAG flag;
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
} readfwVersion_t;

/**
 * SECDEV_CMD_RW_ADDR data structure
 */
typedef struct {
    uint32_t len;    //byte length for gf616
    uint8_t direction;  //0: read, 1: write
    uint8_t speed;   //0: low speed, 1: high speed
    HOLD_MCU_FLAG flag;      //0: whole 16 bits, 1: LSB 8 bits, 2: MSB 8 bits
    uint8_t reserve;
    uint16_t addr;
    uint8_t data[MAX_DATA_LEN];
} rw_data_t;

/**
 * SECDEV_CMD_IRQ_STATUS data structure
 */
typedef struct {
    uint32_t reg_group;    //reg_group=0 for mode/status; reg_group=1 for req_type/chipid
    uint16_t mode;   //0x0B18
    uint16_t status;   //0x0B1A
    uint16_t req_type;   //0x0B1B
    uint16_t chipid;   //0x0020
} irq_status_t;

/**
 * SECDEV_CMD_FORWARD_IRQ data structure
 */
typedef struct {
    EVENT_TYPE event_type;   //0-invalid, 1-keydown, 2-keyon, 3-fp data ready
    uint8_t notify_application;    //whether notify this interrupt to high layer
    uint16_t mode;
    uint16_t status;
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
    HOLD_MCU_FLAG flag;
} reset_sample_t;

/**
 * SECDEV_CMD_ESD_REGISTER data structure
 */
typedef struct {
    uint16_t cur_value;
    uint8_t ret;
} esd_check_t;

/**
 * SECDEV_CMD_SEND_SENSOR data structure
 */
typedef struct {
    uint8_t cmd_num;
} send_sensor_t;

/**
 * SECDEV_CMD_SET_MODE data structure
 */
typedef enum{
    GF616_IMAGE_MODE = 0,
    GF616_KEY_MODE = 0x0100,
    //GF616_SLEEP_MODE = 0x0200,     //no sleep mode for Oswego
    GF616_FF_MODE = 0x0300,
    GF616_DEBUG_MODE = 0x5600,
    GF616_BASE_MODE = 0x5700,
    GF616_UNKNOWN_MODE = 0xFF00
}GF616_MODE;

typedef struct {
    HOLD_MCU_FLAG flag;
    GF616_MODE mode;
    uint8_t rw_flag;
} set_mode_t;


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
    };
} dciMessage_t, *dciMessage_ptr;

/**
 * Trustlet UUID
 */
#define DR_FP_UUID  { { 5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }

#endif // __DRFP_API_H__

/*
 * Copyright (C) 2013 LG Electironics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#define MAX_NUM_OF_BUTTON 4
#define MAX_NUM_OF_FINGER 10
#define DESCRIPTION_TABLE_START    0xE9
#define PAGE_SELECT_REG 0xFF
#define PAGE_MAX_NUM 4

#define REG_X_POSITION                    0
#define REG_Y_POSITION                    1
#define REG_YX_POSITION                    2
#define REG_WY_WX                        3
#define REG_Z                            4
#define NUM_OF_EACH_FINGER_DATA_REG        5

/* Define for Area based key button */
#define BUTTON_MARGIN                    50
#define TOUCH_BUTTON_PRESSED            2

/* Debug Mask setting */
#define SYNAPTICS_RMI4_I2C_DEBUG_PRINT 1
#define SYNAPTICS_RMI4_I2C_ERROR_PRINT 1
#define SYNAPTICS_RMI4_I2C_INFO_PRINT 1

#if defined(SYNAPTICS_RMI4_I2C_INFO_PRINT)
#define SYNAPTICS_INFO_MSG(fmt, args...) printk(KERN_ERR "[Touch] " fmt, ##args);
#else
#define SYNAPTICS_INFO_MSG(fmt, args...) {};
#endif

#if defined(SYNAPTICS_RMI4_I2C_DEBUG_PRINT)
#define SYNAPTICS_DEBUG_MSG(fmt, args...) printk(KERN_ERR "[Touch D] [%s %d] " fmt, __FUNCTION__, __LINE__, ##args);
#else
#define SYNAPTICS_DEBUG_MSG(fmt, args...) {};
#endif

#if defined(SYNAPTICS_RMI4_I2C_ERROR_PRINT)
#define SYNAPTICS_ERR_MSG(fmt, args...) printk(KERN_ERR "[Touch E] [%s %d] " fmt, __FUNCTION__, __LINE__, ##args);
#else
#define SYNAPTICS_ERR_MSG(fmt, args...) {};
#endif


enum {
    SYNAPTICS_RMI4_I2C_DEBUG_NONE                = 0,
    SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE            = 1U << 0,    /* 1 */
    SYNAPTICS_RMI4_I2C_DEBUG_INT_STATUS            = 1U << 1,    /* 2 */
    SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS        = 1U << 2,    /* 4 */
    SYNAPTICS_RMI4_I2C_DEBUG_FINGER_POSITION    = 1U << 3,    /* 8 */
    SYNAPTICS_RMI4_I2C_DEBUG_FINGER_REG            = 1U << 4,    /* 16 */
    SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_STATUS        = 1U << 5,    /* 32 */
    SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_REG            = 1U << 6,    /* 64 */
    SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL        = 1U << 7,    /* 128 */
    SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY        = 1U << 8,    /* 256 */
    SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME    = 1U << 9,    /* 512 */
    SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME    = 1U << 10,    /* 1024 */
    SYNAPTICS_RMI4_I2C_DEBUG_UPGRADE_DELAY        = 1U << 11,    /* 2048 */
};

enum {
    IC_CTRL_CODE_NONE = 0,
    IC_CTRL_BASELINE,
    IC_CTRL_READ,
    IC_CTRL_WRITE,
    IC_CTRL_RESET_CMD,
    IC_CTRL_REPORT_MODE,
};

enum {
    BASELINE_OPEN = 0,
    BASELINE_FIX,
    BASELINE_REBASE,
};

enum {
    TIME_EX_INIT_TIME,
    TIME_EX_FIRST_INT_TIME,
    TIME_EX_PREV_PRESS_TIME,
    TIME_EX_CURR_PRESS_TIME,
    TIME_EX_BUTTON_PRESS_START_TIME,
    TIME_EX_BUTTON_PRESS_END_TIME,
    TIME_EX_FIRST_GHOST_DETECT_TIME,
    TIME_EX_SECOND_GHOST_DETECT_TIME,
    TIME_EX_CURR_INT_TIME,
    TIME_EX_PROFILE_MAX
};

enum{
    IGNORE_INTERRUPT = 100,
    NEED_TO_OUT,
    NEED_TO_INIT,
};


struct synaptics_ts_platform_data {
    bool use_irq;
    unsigned long irqflags;
    unsigned short irq_num;
    //jhee//unsigned short int_gpio;
    unsigned int int_gpio;
    //jhee//unsigned short reset_gpio;
    unsigned int reset_gpio;
    int (*power)(int on);
    unsigned short ic_booting_delay; /* ms */
    unsigned long report_period; /* ns */
    unsigned char num_of_finger;
    unsigned char num_of_button;
    unsigned short button[MAX_NUM_OF_BUTTON];
    int x_max;
    int y_max;
    unsigned char fw_ver;
    unsigned int palm_threshold;
    unsigned int delta_pos_threshold;
    int use_ghost_detection;
    int report_mode;
};

typedef struct {
    u8 query_base;
    u8 command_base;
    u8 control_base;
    u8 data_base;
    u8 int_source_count;
    u8 id;
} ts_function_descriptor;

typedef struct {
    unsigned int pos_x[MAX_NUM_OF_FINGER];
    unsigned int pos_y[MAX_NUM_OF_FINGER];
    unsigned char pressure[MAX_NUM_OF_FINGER];
    int total_num;
    char palm;
} ts_finger_data;

struct synaptics_ts_timestamp {
    u64 start;
    u64 end;
    u64 result_t;
    unsigned long rem;
    atomic_t ready;
};

/* Device data structure */
struct synaptics_ts_data {
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct synaptics_ts_platform_data *pdata;
    bool is_downloading;        /* avoid power off during F/W upgrade */
    bool is_suspended;            /* avoid power off during F/W upgrade */
    unsigned int button_width;
    char button_prestate[MAX_NUM_OF_BUTTON];
    char finger_prestate[MAX_NUM_OF_FINGER];
    bool ic_init;
    bool is_probed;
    atomic_t interrupt_handled;
    ts_function_descriptor common_dsc;
    ts_function_descriptor finger_dsc;
    ts_function_descriptor button_dsc;
    ts_function_descriptor analog_dsc;
    ts_function_descriptor flash_dsc;
    u8 common_page;
    u8 finger_page;
    u8 button_page;
    u8 analog_page;
    u8 flash_page;
    unsigned char int_status_reg_asb0_bit;
    unsigned char int_status_reg_button_bit;
    unsigned char curr_int_mask;
    struct hrtimer timer;
    struct delayed_work work;
    struct delayed_work button_lock_work;
    struct synaptics_ts_timestamp int_delay;
    ts_finger_data pre_ts_data;
    char fw_rev;
    char manufcturer_id;
    char config_id[5];
    char product_id[11];
    char fw_path[256];
    unsigned char *fw_start;
    unsigned long fw_size;
    char fw_config_id[5];
    char fw_product_id[11];
    bool fw_force_upgrade;
};

typedef struct {
    unsigned char device_status_reg;
    unsigned char interrupt_status_reg;
    unsigned char button_data_reg;
} ts_sensor_ctrl;

typedef struct {
    unsigned char finger_state_reg[3];
    unsigned char finger_data[MAX_NUM_OF_FINGER][NUM_OF_EACH_FINGER_DATA_REG];
} ts_sensor_data;


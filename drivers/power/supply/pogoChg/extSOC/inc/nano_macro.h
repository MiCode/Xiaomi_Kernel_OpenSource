/** ***************************************************************************
 * @file nano_macro.h
 *
 */
#ifndef _NANO_MACRO_H_
#define _NANO_MACRO_H_

#include <linux/device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/battmngr/battmngr_notifier.h>
#include <linux/power_supply.h>
#include <linux/battmngr/battmngr_voter.h>
#include <linux/battmngr/platform_class.h>

#define DRV_VERSION  "1.0.16"

#define DRV_TOKEN    "nano_charge"

#define DEBUG_LEVEL	            (7)
#define INFO_LEVEL	            (5)
#define ERROR_LEVEL	            (3)
#define ALERT_LEVEL	            (1)

#define I2C_DATA_LENGTH_WRITE   (66)

#define I2C_DATA_LENGTH_READ    (68)

#define FIELD_HOST              (0x80)
#define FIELD_803X              (0x18)
#define FIELD_176X              (0x38)
#define FIELD_CHARGE            (0x81)

#define SET_CHARGE_POWER_OFF    (6)
#define SET_CHARGE_REG_ADDR_OFF (8)
#define SET_CHARGE_REG_LENG_OFF (9)
#define SET_CHARGE_REG_DATA_OFF (10)

#define CMD_OK                  (0)

#define CMD_MCU_RSP                  (0xF0)

#define CMD_DEVICE_CONN_STATUS_RSP   (0x52)

#define CMD_KEYBOARD_CONN_STATUS_RSP (0xA2)
#define CMD_KEYBOARD_READ_VERSION_RSP (0x1)

#define CMD_READ_CHARGE_STATUS       (0x40)
#define CMD_READ_CHARGE_STATUS_RSP   (0x41)

#define CMD_SET_CHARGE_REG           (0x42)
#define CMD_SET_CHARGE_REG_RSP       (0x43)


typedef int (*IIC_write_cb)(uint8_t* data,uint32_t datalen);
typedef int (*ADSP_read_cb)(uint8_t* data,uint32_t datalen);
typedef int (*i2c_write_cb)(void *buf, size_t len);

/*
 * Logging and Debugging
 */ 
    /* The main debug macro.  Uncomment to enable debug messages */
#define DEBUG

    /* Make sure we have a per-module token */
#ifndef DRV_TOKEN
#error Module must #define DRV_TOKEN to be a short nickname of the module.
#endif
    /* Error messages -- always sent to log */
#undef errprint
#undef errmsg
#define errprint(__format_and_args...) \
    printk(DRV_TOKEN ": ERROR: " __format_and_args)

#define errmsg(__msg...) errprint(DRV_TOKEN,##__msg)

static int debuglevel = 7;

#undef dbgprint
#undef dbgmsg
#define dbgprint(__level, __format,__args...)   \
do {                                            \
    if ((int)debuglevel >= (int)__level) {      \
        struct timespec64 ts;                   \
        struct rtc_time tm;                     \
        ktime_get_real_ts64(&ts);               \
        rtc_time64_to_tm(ts.tv_sec,&tm);        \
        switch((int)debuglevel) {               \
            case 0: \
            case 1: \
            case 2:  printk("[%02d:%02d:%02d.%03zu] <2>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000000, ##__args); break; \
            case 3:  printk("[%02d:%02d:%02d.%03zu] <3>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000000, ##__args); break; \
            case 4:  printk("[%02d:%02d:%02d.%03zu] <4>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000000, ##__args); break; \
            case 5:  printk("[%02d:%02d:%02d.%03zu] <5>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000000, ##__args); break; \
            case 6:  printk("[%02d:%02d:%02d.%03zu] <6>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000000, ##__args); break; \
            default: printk("[%02d:%02d:%02d.%03zu] <7>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000000, ##__args); break; \
        } \
    }\
} while (0)

#ifdef DEBUG
    /**
     * @brief Send a debug message to the log facility.
     * @ingroup albrxdoc_util
     * @details This macro sends a message to the logging facility with a
     * simple token prefix.  This message is delivered iff the __level
     * parameter is not greater than the value of the \c debug static variable.
     * @note If \c DEBUG is undefined (i.e. the \#define is not active), no
     * debug messages are sent to the logging facility, regardless of level.
     */
#define dbgmsg(__level,__msg...) dbgprint(__level,DRV_TOKEN,##__msg)
#else /* !DEBUG */
#define dbgmsg(__level,__msg...)
#endif
    /* Assertions -- only checked if DEBUG is defined */
#undef ASSERT
#ifdef DEBUG
#define ASSERT(__condition,__fail_action) \
    do { \
        if (!(__condition)) { \
            errmsg("ASSERT(%s:%d): %s\n",__FILE__,__LINE__,#__condition); \
            __fail_action ; \
        } \
    } while (0)
#else
#define ASSERT(__c,__f)
#endif

#define STREAM_TO_UINT8(u8, p)   {u8 = (unsigned char)(*(p)); (p) += 1;}
#define STREAM_TO_UINT16(u16, p) {u16 = ((unsigned short)(*(p)) + (((unsigned short)(*((p) + 1))) << 8)); (p) += 2;}

struct nanochg {
    struct device *dev;
	unsigned int    status;
	int		vbus_type;
	int		charge_type;
    u8    online;
    u8      pd_flag;
    u8      dpdm_in_state;
    u8      dpdm;
    int     protocol_volt;
    int     protocol_curr;

	bool	enabled;

	int		vbus_volt;
	int		vbat_volt;

	struct power_supply *batt_psy;
	struct power_supply *usb_psy;

	struct votable *fcc_votable;
	struct votable *usb_icl_votable;

	int old_type;
	int curr_flag;
	bool force_exit_flag;
	int count;
	int input_suspend;
	int dpdm_enabled;
	u8 vbus_state;
	u8 power_state;
	u8 gObject;

    struct mutex nano_i2c_write_mutex;
    i2c_write_cb pogo_charge_i2c_write;

    struct delayed_work	analyse_rawdata_work;
};

int Nanosic_Analysis_RawData(void *buf);

extern struct nanochg* g_nodev;

extern int  Nanosic_ADSP_Set_Charge_Status_Single(uint8_t addr, uint8_t addrlen, uint8_t* data, uint32_t datalen);
extern int  Nanosic_ADSP_Set_Charge_Status(uint8_t* data,uint32_t datalen);
extern int  Nanosic_ADSP_Read_Charge_Status(void);
extern int  Nanosic_ADSP_Control_Charge_Power(bool on);
extern int Nanosic_ADSP_Read_Charge_Status_Single(uint8_t addr);

#endif /* _ALBRX_H_ */


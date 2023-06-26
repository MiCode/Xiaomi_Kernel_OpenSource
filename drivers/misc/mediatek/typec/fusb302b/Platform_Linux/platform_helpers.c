#include <linux/kernel.h>
#include <linux/stat.h>                                                         // File permission masks
#include <linux/types.h>                                                        // Kernel datatypes
#include <linux/i2c.h>                                                          // I2C access, mutex
#include <linux/errno.h>                                                        // Linux kernel error definitions
#include <linux/hrtimer.h>                                                      // hrtimer
#include <linux/workqueue.h>                                                    // work_struct, delayed_work
#include <linux/delay.h>                                                        // udelay, usleep_range, msleep
#include <linux/pm_wakeup.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/power_supply.h>

#include "fusb30x_global.h"     // Chip structure access
#include "core.h"               // Core access
#include "platform_helpers.h"

#include "HostComm.h"
#include "Log.h"
#include "Port.h"
#include "PD_Types.h"           // State Log states
#include "TypeC_Types.h"        // State Log states
#include "sysfs_header.h"
#include "modules/observer.h"
#include "platform.h"

#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include <linux/usb/class-dual-role.h>
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

void fusb_init_event_handler(void);

void stop_usb_host(struct fusb30x_chip* chip);
void start_usb_host(struct fusb30x_chip* chip, bool ss);
void stop_usb_peripheral(struct fusb30x_chip* chip);
void start_usb_peripheral(struct fusb30x_chip* chip);

void handle_core_event(FSC_U32 event, FSC_U8 portId,
		void *usr_ctx, void *app_ctx);

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        GPIO Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
const char* FUSB_DT_INTERRUPT_INTN =    "fsc_interrupt_int_n";      // Name of the INT_N interrupt in the Device Tree
#define FUSB_DT_GPIO_INTN               "fairchild,int_n"           // Name of the Int_N GPIO pin in the Device Tree
#define FUSB_DT_GPIO_VBUS_5V            "fairchild,vbus5v"          // Name of the VBus 5V GPIO pin in the Device Tree
#define FUSB_DT_GPIO_VBUS_OTHER         "fairchild,vbusOther"       // Name of the VBus Other GPIO pin in the Device Tree
#define FUSB_DT_GPIO_DISCHARGE          "fairchild,discharge"       // Name of the Discharge GPIO pin in the Device Tree

#ifdef FSC_DEBUG
#define FUSB_DT_GPIO_DEBUG_SM_TOGGLE    "fairchild,dbg_sm"          // Name of the debug State Machine toggle GPIO pin in the Device Tree
#endif  // FSC_DEBUG

/* Internal forward declarations */
static irqreturn_t _fusb_isr_intn(int irq, void *dev_id);
static void work_function(struct work_struct *work);
static void init_work_function(struct work_struct *delayed_work);
static enum hrtimer_restart fusb_sm_timer_callback(struct hrtimer *timer);

FSC_S32 fusb_InitializeGPIO(void)
{
    FSC_S32 ret = 0;
    struct device_node* node = NULL;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return -ENOMEM;
    }
    /* Get our device tree node */
    node = chip->client->dev.of_node;

    /* Get our GPIO pins from the device tree, allocate them, and then set their direction (input/output) */
    chip->gpio_IntN = of_get_named_gpio(node, FUSB_DT_GPIO_INTN, 0);
    if (!gpio_is_valid(chip->gpio_IntN))
    {
        dev_err(&chip->client->dev, "%s - Error: Could not get named GPIO for Int_N! Error code: %d\n", __func__, chip->gpio_IntN);
        return chip->gpio_IntN;
    }

    // Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
    ret = gpio_request(chip->gpio_IntN, FUSB_DT_GPIO_INTN);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for Int_N! Error code: %d\n", __func__, ret);
        return ret;
    }

    ret = gpio_direction_input(chip->gpio_IntN);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to input for Int_N! Error code: %d\n", __func__, ret);
        return ret;
    }

#ifdef FSC_DEBUG
    /* Export to sysfs */
    gpio_export(chip->gpio_IntN, false);
    gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_INTN, chip->gpio_IntN);
#endif // FSC_DEBUG

    pr_info("FUSB  %s - INT_N GPIO initialized as pin '%d'\n", __func__, chip->gpio_IntN);

#if 0
    // VBus 5V
    chip->gpio_VBus5V = of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_5V, 0);
    if (!gpio_is_valid(chip->gpio_VBus5V))
    {
        dev_err(&chip->client->dev, "%s - Error: Could not get named GPIO for VBus5V! Error code: %d\n", __func__, chip->gpio_VBus5V);
        fusb_GPIO_Cleanup();
        return chip->gpio_VBus5V;
    }

    // Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
    ret = gpio_request(chip->gpio_VBus5V, FUSB_DT_GPIO_VBUS_5V);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for VBus5V! Error code: %d\n", __func__, ret);
        return ret;
    }

    ret = gpio_direction_output(chip->gpio_VBus5V, chip->gpio_VBus5V_value);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for VBus5V! Error code: %d\n", __func__, ret);
        fusb_GPIO_Cleanup();
        return ret;
    }

#ifdef FSC_DEBUG
    // Export to sysfs
    gpio_export(chip->gpio_VBus5V, false);
    gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_VBUS_5V, chip->gpio_VBus5V);
#endif // FSC_DEBUG

    pr_info("FUSB  %s - VBus 5V initialized as pin '%d' and is set to '%d'\n", __func__, chip->gpio_VBus5V, chip->gpio_VBus5V_value ? 1 : 0);

    // VBus other (eg. 12V)
    // NOTE - This VBus is optional, so if it doesn't exist then fake it like it's on.
    chip->gpio_VBusOther = of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_OTHER, 0);
    if (!gpio_is_valid(chip->gpio_VBusOther))
    {
        // Soft fail - provide a warning, but don't quit because we don't really need this VBus if only using VBus5v
        pr_warning("%s - Warning: Could not get GPIO for VBusOther! Error code: %d\n", __func__, chip->gpio_VBusOther);
    }
    else
    {
        // Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
        ret = gpio_request(chip->gpio_VBusOther, FUSB_DT_GPIO_VBUS_OTHER);
        if (ret < 0)
        {
            dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for VBusOther! Error code: %d\n", __func__, ret);
            return ret;
        }

        ret = gpio_direction_output(chip->gpio_VBusOther, chip->gpio_VBusOther_value);
        if (ret != 0)
        {
            dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for VBusOther! Error code: %d\n", __func__, ret);
            return ret;
        }
        else
        {
            pr_info("FUSB  %s - VBusOther initialized as pin '%d' and is set to '%d'\n", __func__, chip->gpio_VBusOther, chip->gpio_VBusOther_value ? 1 : 0);

        }
    }

    // Discharge
    chip->gpio_Discharge = of_get_named_gpio(node, FUSB_DT_GPIO_DISCHARGE, 0);
    if (!gpio_is_valid(chip->gpio_Discharge))
    {
        dev_err(&chip->client->dev, "%s - Error: Could not get named GPIO for Discharge! Error code: %d\n", __func__, chip->gpio_Discharge);
        fusb_GPIO_Cleanup();
        return chip->gpio_Discharge;
    }

    // Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
    ret = gpio_request(chip->gpio_Discharge, FUSB_DT_GPIO_DISCHARGE);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for Discharge! Error code: %d\n", __func__, ret);
        return ret;
    }

    ret = gpio_direction_output(chip->gpio_Discharge, chip->gpio_Discharge_value);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for Discharge! Error code: %d\n", __func__, ret);
        fusb_GPIO_Cleanup();
        return ret;
    }

    pr_info("FUSB  %s - Discharge GPIO initialized as pin '%d'\n", __func__, chip->gpio_Discharge);

#ifdef FSC_DEBUG
    // State Machine Debug Notification
    // Optional GPIO - toggles each time the state machine is called
    chip->dbg_gpio_StateMachine = of_get_named_gpio(node, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, 0);
    if (!gpio_is_valid(chip->dbg_gpio_StateMachine))
    {
        // Soft fail - provide a warning, but don't quit because we don't really need this VBus if only using VBus5v
        pr_warning("%s - Warning: Could not get GPIO for Debug GPIO! Error code: %d\n", __func__, chip->dbg_gpio_StateMachine);
    }
    else
    {
        // Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
        ret = gpio_request(chip->dbg_gpio_StateMachine, FUSB_DT_GPIO_DEBUG_SM_TOGGLE);
        if (ret < 0)
        {
            dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for Debug GPIO! Error code: %d\n", __func__, ret);
            return ret;
        }

        ret = gpio_direction_output(chip->dbg_gpio_StateMachine, chip->dbg_gpio_StateMachine_value);
        if (ret != 0)
        {
            dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for Debug GPIO! Error code: %d\n", __func__, ret);
            return ret;
        }
        else
        {
            pr_info("FUSB  %s - Debug GPIO initialized as pin '%d' and is set to '%d'\n", __func__, chip->dbg_gpio_StateMachine, chip->dbg_gpio_StateMachine_value ? 1 : 0);

        }

        // Export to sysfs
        gpio_export(chip->dbg_gpio_StateMachine, true); // Allow direction to change to provide max debug flexibility
        gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, chip->dbg_gpio_StateMachine);
    }
#endif  // FSC_DEBUG
#endif
    return 0;   // Success!
}

void fusb_GPIO_Set_VBus5v(FSC_BOOL set)
{
#if 0
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
    }

    // GPIO must be valid by this point
    if (gpio_cansleep(chip->gpio_VBus5V))
    {
        /*
         * If your system routes GPIO calls through a queue of some kind, then
         * it may need to be able to sleep. If so, this call must be used.
         */
        gpio_set_value_cansleep(chip->gpio_VBus5V, set ? 1 : 0);
    }
    else
    {
        gpio_set_value(chip->gpio_VBus5V, set ? 1 : 0);
    }
    chip->gpio_VBus5V_value = set;

    pr_debug("FUSB  %s - VBus 5V set to: %d\n", __func__, chip->gpio_VBus5V_value ? 1 : 0);
#endif
}

void fusb_GPIO_Set_VBusOther(FSC_BOOL set)
{
#if 0
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
    }

    // Only try to set if feature is enabled, otherwise just fake it
    if (gpio_is_valid(chip->gpio_VBusOther))
    {
        /*
        * If your system routes GPIO calls through a queue of some kind, then
        * it may need to be able to sleep. If so, this call must be used.
        */
        if (gpio_cansleep(chip->gpio_VBusOther))
        {
            gpio_set_value_cansleep(chip->gpio_VBusOther, set ? 1 : 0);
        }
        else
        {
            gpio_set_value(chip->gpio_VBusOther, set ? 1 : 0);
        }
    }
    chip->gpio_VBusOther_value = set;
#endif
}

FSC_BOOL fusb_GPIO_Get_VBus5v(void)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return false;
    }

    if (!gpio_is_valid(chip->gpio_VBus5V))
    {
        pr_debug("FUSB  %s - Error: VBus 5V pin invalid! Pin value: %d\n", __func__, chip->gpio_VBus5V);
    }

    return chip->gpio_VBus5V_value;
}

FSC_BOOL fusb_GPIO_Get_VBusOther(void)
{
#if 0
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return false;
    }

    return chip->gpio_VBusOther_value;
#endif
    return true;
}

void fusb_GPIO_Set_Discharge(FSC_BOOL set)
{
#if 0
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
    }

    // GPIO must be valid by this point
    if (gpio_cansleep(chip->gpio_Discharge))
    {
        /*
         * If your system routes GPIO calls through a queue of some kind, then
         * it may need to be able to sleep. If so, this call must be used.
         */
        gpio_set_value_cansleep(chip->gpio_Discharge, set ? 1 : 0);
    }
    else
    {
        gpio_set_value(chip->gpio_Discharge, set ? 1 : 0);
    }
    chip->gpio_Discharge_value = set;

    pr_debug("FUSB  %s - Discharge set to: %d\n", __func__, chip->gpio_Discharge_value ? 1 : 0);
#endif
}

FSC_BOOL fusb_GPIO_Get_IntN(void)
{
    FSC_S32 ret = 0;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return false;
    }
    else
    {
        /*
        * If your system routes GPIO calls through a queue of some kind, then
        * it may need to be able to sleep. If so, this call must be used.
        */
        if (gpio_cansleep(chip->gpio_IntN))
        {
            ret = !gpio_get_value_cansleep(chip->gpio_IntN);
        }
        else
        {
            ret = !gpio_get_value(chip->gpio_IntN); // Int_N is active low
        }
        return (ret != 0);
    }
}

#ifdef FSC_DEBUG
void dbg_fusb_GPIO_Set_SM_Toggle(FSC_BOOL set)
{
#if 0
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
    }

    if (gpio_is_valid(chip->dbg_gpio_StateMachine))
    {
        /*
        * If your system routes GPIO calls through a queue of some kind, then
        * it may need to be able to sleep. If so, this call must be used.
        */
        if (gpio_cansleep(chip->dbg_gpio_StateMachine))
        {
            gpio_set_value_cansleep(chip->dbg_gpio_StateMachine, set ? 1 : 0);
        }
        else
        {
            gpio_set_value(chip->dbg_gpio_StateMachine, set ? 1 : 0);
        }
        chip->dbg_gpio_StateMachine_value = set;
    }
#endif
}

FSC_BOOL dbg_fusb_GPIO_Get_SM_Toggle(void)
{
#if 0
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return false;
    }
    return chip->dbg_gpio_StateMachine_value;
#endif
    return true;
}
#endif  // FSC_DEBUG

void fusb_GPIO_Cleanup(void)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return;
    }

    if (gpio_is_valid(chip->gpio_IntN) && chip->gpio_IntN_irq != -1)    // -1 indicates that we don't have an IRQ to free
    {
        devm_free_irq(&chip->client->dev, chip->gpio_IntN_irq, chip);
    }
	wakeup_source_trash(&chip->fusb302_wakelock);

    if (gpio_is_valid(chip->gpio_IntN))
    {
#ifdef FSC_DEBUG
        gpio_unexport(chip->gpio_IntN);
#endif // FSC_DEBUG

        gpio_free(chip->gpio_IntN);
    }

    if (gpio_is_valid(chip->gpio_VBus5V))
    {
#ifdef FSC_DEBUG
        gpio_unexport(chip->gpio_VBus5V);
#endif // FSC_DEBUG

        gpio_free(chip->gpio_VBus5V);
    }

    if (gpio_is_valid(chip->gpio_VBusOther))
    {
        gpio_free(chip->gpio_VBusOther);
    }

#ifdef FSC_DEBUG
    if (gpio_is_valid(chip->dbg_gpio_StateMachine))
    {
        gpio_unexport(chip->dbg_gpio_StateMachine);
        gpio_free(chip->dbg_gpio_StateMachine);
    }
#endif  // FSC_DEBUG
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************         I2C Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
FSC_BOOL fusb_I2C_WriteData(FSC_U8 address, FSC_U8 length, FSC_U8* data)
{
    FSC_S32 i = 0;
    FSC_S32 ret = 0;

    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL || chip->client == NULL || data == NULL)
    {
        pr_err("%s - Error: %s is NULL!\n", __func__,
            (chip == NULL ? "Internal chip structure"
                : (chip->client == NULL ? "I2C Client"
                    : "Write data buffer")));
        return FALSE;
    }

    mutex_lock(&chip->lock);

    // Retry on failure up to the retry limit
    for (i = 0; i <= chip->numRetriesI2C; i++)
    {
        ret = i2c_smbus_write_i2c_block_data(chip->client, address,
            length, data);

        if (ret < 0)
        {
            // Errors report as negative
            dev_err(&chip->client->dev,
                "%s - I2C error block writing byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__,
                address, ret, i, chip->numRetriesI2C);
        }
        else
        {
            // Successful i2c writes should always return 0
            break;
        }
    }

    mutex_unlock(&chip->lock);

    return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadData(FSC_U8 address, FSC_U8* data)
{
    FSC_S32 i = 0;
    FSC_S32 ret = 0;

    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL || chip->client == NULL || data == NULL)
    {
        pr_err("%s - Error: %s is NULL!\n", __func__,
            (chip == NULL ? "Internal chip structure"
                : (chip->client == NULL ? "I2C Client"
                    : "read data buffer")));
        return FALSE;
    }

    mutex_lock(&chip->lock);

    // Retry on failure up to the retry limit
    for (i = 0; i <= chip->numRetriesI2C; i++)
    {
        // Read a byte of data from address
        ret = i2c_smbus_read_byte_data(chip->client, (u8)address);

        if (ret < 0)
        {
            // Errors report as a negative 32-bit value
            dev_err(&chip->client->dev,
                "%s - I2C error reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__,
                address, ret, i, chip->numRetriesI2C);
        }
        else
        {
            // On success, the low 8-bits holds the byte read from the device
            *data = (FSC_U8)ret;
            break;
        }
    }

    mutex_unlock(&chip->lock);

    return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadBlockData(FSC_U8 address, FSC_U8 length, FSC_U8* data)
{
    FSC_S32 i = 0;
    FSC_S32 ret = 0;

    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL || chip->client == NULL || data == NULL)
    {
        pr_err("%s - Error: %s is NULL!\n", __func__,
            (chip == NULL ? "Internal chip structure"
                : (chip->client == NULL ? "I2C Client"
                    : "block read data buffer")));
        return FALSE;
    }

    mutex_lock(&chip->lock);

    // Retry on failure up to the retry limit
    for (i = 0; i <= chip->numRetriesI2C; i++)
    {
        // Read a block of byte data from address
        ret = i2c_smbus_read_i2c_block_data(chip->client, (u8)address,
            (u8)length, (u8*)data);

        if (ret < 0)
        {
            // Errors report as a negative 32-bit value
            dev_err(&chip->client->dev,
                "%s - I2C error block reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__,
                address, ret, i, chip->numRetriesI2C);
        }
        else if (ret != length)
        {
            // Slave didn't provide the full read response
            dev_err(&chip->client->dev,
                "%s - Error: Block read request of %u bytes truncated to %u bytes.\n", __func__,
                length, I2C_SMBUS_BLOCK_MAX);
        }
        else
        {
            // Success, don't retry
            break;
        }
    }

    mutex_unlock(&chip->lock);

    return (ret == length);
}


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Timer Interface        ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

/*******************************************************************************
* Function:        _fusb_TimerHandler
* Input:           timer: hrtimer struct to be handled
* Return:          HRTIMER_RESTART to restart the timer, or HRTIMER_NORESTART otherwise
* Description:     Ticks state machine timer counters and rearms itself
********************************************************************************/

// Get the max value that we can delay in 10us increments at compile time
static const FSC_U32 MAX_DELAY_10US = (UINT_MAX / 10);
void fusb_Delay10us(FSC_U32 delay10us)
{
    FSC_U32 us = 0;
    if (delay10us > MAX_DELAY_10US)
    {
        pr_err("%s - Error: Delay of '%u' is too long! Must be less than '%u'.\n", __func__, delay10us, MAX_DELAY_10US);
        return;
    }

    us = delay10us * 10;                                    // Convert to microseconds (us)

    if (us <= 10)                                           // Best practice is to use udelay() for < ~10us times
    {
        udelay(us);                                         // BLOCKING delay for < 10us
    }
    else if (us < 20000)                                    // Best practice is to use usleep_range() for 10us-20ms
    {
        // TODO - optimize this range, probably per-platform
        usleep_range(us, us + (us / 10));                   // Non-blocking sleep for at least the requested time, and up to the requested time + 10%
    }
    else                                                    // Best practice is to use msleep() for > 20ms
    {
        msleep(us / 1000);                                  // Convert to ms. Non-blocking, low-precision sleep
    }
}

/*******************************************************************************
* Function:        fusb_Sysfs_Handle_Read
* Input:           output: Buffer to which the output will be written
* Return:          Number of chars written to output
* Description:     Reading this file will output the most recently saved hostcomm output buffer
* NOTE: Not used right now - separate functions for state logs.
********************************************************************************/
#define FUSB_MAX_BUF_SIZE 256   // Arbitrary temp buffer for parsing out driver data to sysfs

/* Reinitialize the FUSB302 */
static ssize_t _fusb_Sysfs_Reinitialize_fusb302(struct device* dev, struct device_attribute* attr, char* buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL)
    {
        return sprintf(buf, "FUSB302 Error: Internal chip structure pointer is NULL!\n");
    }

    /* Make sure that we are doing this in a thread-safe manner */
    /* Waits for current IRQ handler to return, then disables it */
    disable_irq(chip->gpio_IntN_irq);

    core_initialize(&chip->port, 0x00);
    pr_debug ("FUSB  %s - Core is initialized!\n", __func__);
    core_enable_typec(&chip->port, TRUE);
    pr_debug ("FUSB  %s - Type-C State Machine is enabled!\n", __func__);

    enable_irq(chip->gpio_IntN_irq);

    return sprintf(buf, "FUSB302 Reinitialized!\n");
}

// Define our device attributes to export them to sysfs
static DEVICE_ATTR(reinitialize, S_IRUSR | S_IRGRP | S_IROTH, _fusb_Sysfs_Reinitialize_fusb302, NULL);

#ifdef FSC_DEBUG
static ssize_t hcom_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    FSC_U8 *outBuf = HCom_OutBuf();
    memcpy(buf, outBuf, HCMD_SIZE);
    return HCMD_SIZE;
}

static ssize_t hcom_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    FSC_U8 *inBuf  = HCom_InBuf();
    memcpy(inBuf, buf, HCMD_SIZE);
    HCom_Process();
    return HCMD_SIZE;
}
#endif /* FSC_DEBUG */

static ssize_t typec_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip->port.ConnState < NUM_TYPEC_STATES) {
        return sprintf(buf, "%d %s\n", chip->port.ConnState,
                TYPEC_STATE_TBL[chip->port.ConnState]);
    }
    return 0;
}

static ssize_t pe_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip->port.PolicyState < NUM_PE_STATES) {
        return sprintf(buf, "%d %s\n", chip->port.PolicyState,
                PE_STATE_TBL[chip->port.PolicyState]);
    }
    return 0;
}

static ssize_t port_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.PortConfig.PortType);
}

static ssize_t cc_term_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();

    return sprintf(buf, "%s\n", CC_TERM_TBL[chip->port.CCTerm % NUM_CC_TERMS]);
}

static ssize_t vconn_term_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();

    return sprintf(buf, "%s\n", CC_TERM_TBL[chip->port.VCONNTerm % NUM_CC_TERMS]);
}

static ssize_t cc_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();

    return sprintf(buf, "%s\n",
        (chip->port.CCPin == CC1) ? "CC1" :
        (chip->port.CCPin == CC2) ? "CC2" : "None");
}

static ssize_t pwr_role_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%s\n",
        chip->port.PolicyIsSource == TRUE ? "Source" : "Sink");
}

static ssize_t data_role_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%s\n",
        chip->port.PolicyIsDFP == TRUE ? "DFP" : "UFP");
}

static ssize_t vconn_source_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.IsVCONNSource);
}

static ssize_t pe_enabled_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.USBPDEnabled);
}

static ssize_t pe_enabled_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    int enabled;
    if (sscanf(buf, "%d", &enabled)) {
        chip->port.USBPDEnabled = enabled;
    }
    return count;
}

static ssize_t pd_specrev_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#if 0
    struct fusb30x_chip* chip = fusb30x_GetChip();
    int len = 0;
    switch(chip->port.PdRevContract) {
    case USBPDSPECREV1p0:
        len = sprintf(buf, "1\n");
        break;
    case USBPDSPECREV2p0:
        len = sprintf(buf, "2\n");
        break;
    case USBPDSPECREV3p0:
        len = sprintf(buf, "3\n");
        break;
    default:
        len = sprintf(buf, "Unknown\n");
        break;
    }
#endif
    int len = 0;
    len = sprintf(buf, "3\n");
    return len;
}

static ssize_t sink_op_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.PortConfig.SinkRequestOpPower);
}

static ssize_t sink_op_power_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    int op_pwr;
    if (sscanf(buf, "%d", &op_pwr)) {
        chip->port.PortConfig.SinkRequestOpPower = op_pwr;
    }
    return count;
}

static ssize_t sink_max_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.PortConfig.SinkRequestMaxPower);
}

static ssize_t sink_max_power_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    int snk_pwr;
    if (sscanf(buf, "%d", &snk_pwr)) {
        chip->port.PortConfig.SinkRequestMaxPower = snk_pwr;
    }
    return count;
}

static ssize_t src_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.SourceCurrent);
}

static ssize_t src_current_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    int src_cur;
    if (sscanf(buf, "%d", &src_cur)) {
        core_set_advertised_current(&chip->port, src_cur);
    }
    return count;
}

static ssize_t snk_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.SinkCurrent);
}

static ssize_t snk_current_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    int amp;
    if (sscanf(buf, "%d", &amp)) {
        chip->port.SinkCurrent = amp;
    }

    return count;
}

static ssize_t acc_support_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.PortConfig.audioAccSupport);
}

static ssize_t acc_support_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (buf[0] == '1') {
        chip->port.PortConfig.audioAccSupport = TRUE;
    } else if (buf[0] == '0') {
        chip->port.PortConfig.audioAccSupport = FALSE;
    }
    return count;
}

static ssize_t src_pref_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.PortConfig.SrcPreferred);
}

static ssize_t src_pref_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (buf[0] == '1') {
        chip->port.PortConfig.SrcPreferred = TRUE;
        chip->port.PortConfig.SnkPreferred = FALSE;
    } else if (buf[0] == '0'){
        chip->port.PortConfig.SrcPreferred = FALSE;
    }
    return count;
}

static ssize_t snk_pref_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    return sprintf(buf, "%d\n", chip->port.PortConfig.SnkPreferred);
}

static ssize_t snk_pref_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (buf[0] == '1') {
        chip->port.PortConfig.SnkPreferred = TRUE;
        chip->port.PortConfig.SrcPreferred = FALSE;
    } else if (buf[0] == '0'){
        chip->port.PortConfig.SnkPreferred = FALSE;
    }

    return count;
}

#ifdef FSC_DEBUG
static DEVICE_ATTR(hostcom, S_IRUGO | S_IWUSR, hcom_show, hcom_store);
#endif /* FSC_DEBUG */

static DEVICE_ATTR(typec_state, S_IRUGO | S_IWUSR, typec_state_show, 0);
static DEVICE_ATTR(port_type, S_IRUGO | S_IWUSR, port_type_show, 0);
static DEVICE_ATTR(cc_term, S_IRUGO | S_IWUSR, cc_term_show, 0);
static DEVICE_ATTR(vconn_term, S_IRUGO | S_IWUSR, vconn_term_show, 0);
static DEVICE_ATTR(cc_pin, S_IRUGO | S_IWUSR, cc_pin_show, 0);

static DEVICE_ATTR(pe_state, S_IRUGO | S_IWUSR, pe_state_show, 0);
static DEVICE_ATTR(pe_enabled, S_IRUGO | S_IWUSR, pe_enabled_show, pe_enabled_store);
static DEVICE_ATTR(pwr_role, S_IRUGO | S_IWUSR, pwr_role_show, 0);
static DEVICE_ATTR(data_role, S_IRUGO | S_IWUSR, data_role_show, 0);
static DEVICE_ATTR(pd_specrev, S_IRUGO | S_IWUSR, pd_specrev_show, 0);
static DEVICE_ATTR(vconn_source, S_IRUGO | S_IWUSR, vconn_source_show, 0);

static DEVICE_ATTR(sink_op_power, S_IRUGO | S_IWUSR, sink_op_power_show, sink_op_power_store);
static DEVICE_ATTR(sink_max_power, S_IRUGO | S_IWUSR, sink_max_power_show, sink_max_power_store);

static DEVICE_ATTR(src_current, S_IRUGO | S_IWUSR, src_current_show, src_current_store);
static DEVICE_ATTR(sink_current, S_IRUGO | S_IWUSR, snk_current_show, snk_current_store);

static DEVICE_ATTR(acc_support, S_IRUGO | S_IWUSR, acc_support_show, acc_support_store);
static DEVICE_ATTR(src_pref, S_IRUGO | S_IWUSR, src_pref_show, src_pref_store);
static DEVICE_ATTR(sink_pref, S_IRUGO | S_IWUSR, snk_pref_show, snk_pref_store);

static struct attribute *fusb302_sysfs_attrs[] = {
    &dev_attr_reinitialize.attr,
#ifdef FSC_DEBUG
    &dev_attr_hostcom.attr,
#endif /* FSC_DEBUG */
    &dev_attr_typec_state.attr,
    &dev_attr_port_type.attr,
    &dev_attr_cc_term.attr,
    &dev_attr_vconn_term.attr,
    &dev_attr_cc_pin.attr,
    &dev_attr_pe_state.attr,
    &dev_attr_pe_enabled.attr,
    &dev_attr_pwr_role.attr,
    &dev_attr_data_role.attr,
    &dev_attr_pd_specrev.attr,
    &dev_attr_vconn_source.attr,
    &dev_attr_sink_op_power.attr,
    &dev_attr_sink_max_power.attr,
    &dev_attr_src_current.attr,
    &dev_attr_sink_current.attr,
    &dev_attr_acc_support.attr,
    &dev_attr_src_pref.attr,
    &dev_attr_sink_pref.attr,
    NULL
};

static struct attribute_group fusb302_sysfs_attr_grp = {
    .name = "control",
    .attrs = fusb302_sysfs_attrs,
};

void fusb_Sysfs_Init(void)
{
    FSC_S32 ret = 0;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL)
    {
        pr_err("%s - Chip structure is null!\n", __func__);
        return;
    }

#ifdef FSC_DEBUG
    HCom_Init(&chip->port, 1);
#endif /* FSC_DEBUG */

    /* create attribute group for accessing the FUSB302 */
    ret = sysfs_create_group(&chip->client->dev.kobj, &fusb302_sysfs_attr_grp);
    if (ret)
    {
        pr_err("FUSB %s - Error creating sysfs attributes!\n", __func__);
    }
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Driver Helpers         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
void fusb_InitializeCore(void)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return;
    }

    core_initialize(&chip->port, 0x00);
    pr_debug("FUSB  %s - Core is initialized!\n", __func__);
}

FSC_BOOL fusb_IsDeviceValid(void)
{
    FSC_U8 val = 0;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return FALSE;
    }

    // Test to see if we can do a successful I2C read
    if (!fusb_I2C_ReadData((FSC_U8)0x01, &val))
    {
        pr_err("FUSB  %s - Error: Could not communicate with device over I2C!\n", __func__);
        return FALSE;
    }
    pr_info("FUSB %s - FUSB302B ChipId is 0x%2x\n", __func__, val);
    return TRUE;
}

FSC_BOOL fusb_reset(void)
{
    FSC_U8 val = 1;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return FALSE;
    }

    // Reset the FUSB302 and including the I2C registers to their default value.
    if (fusb_I2C_WriteData((FSC_U8)0x0C, 1, &val) == FALSE)
    {
        pr_err("FUSB  %s - Error: Could not communicate with device over I2C!\n", __func__);
        return FALSE;
    }
    return TRUE;
}

void fusb_InitChipData(void)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL)
    {
        pr_err("%s - Chip structure is null!\n", __func__);
        return;
    }

#ifdef FSC_DEBUG
    chip->dbgTimerTicks = 0;
    chip->dbgTimerRollovers = 0;
    chip->dbgSMTicks = 0;
    chip->dbgSMRollovers = 0;
    chip->dbg_gpio_StateMachine = -1;
    chip->dbg_gpio_StateMachine_value = false;
#endif  // FSC_DEBUG

    /* GPIO Defaults */
    chip->gpio_VBus5V = -1;
    chip->gpio_VBus5V_value = false;
    chip->gpio_VBusOther = -1;
    chip->gpio_VBusOther_value = false;
    chip->gpio_IntN = -1;

    /* DPM Setup - TODO - Not the best place for this. */
    chip->port.PortID = 0;
    DPM_Init(&chip->dpm);
    DPM_AddPort(chip->dpm, &chip->port);
    chip->port.dpm = chip->dpm;

    chip->gpio_IntN_irq = -1;

    /* I2C Configuration */
    chip->InitDelayMS = INIT_DELAY_MS;                                              // Time to wait before device init
    chip->numRetriesI2C = RETRIES_I2C;                                              // Number of times to retry I2C reads and writes
    chip->use_i2c_blocks = false;                                                   // Assume failure

    /* Worker thread setup */
    INIT_WORK(&chip->sm_worker, work_function);
	INIT_DELAYED_WORK(&chip->init_worker, init_work_function);

    chip->queued = FALSE;

    chip->highpri_wq = alloc_workqueue("FUSB WQ", WQ_HIGHPRI|WQ_UNBOUND, 1);

    if (chip->highpri_wq == NULL)
    {
        pr_err("%s - Unable to allocate new work queue!\n", __func__);
        return;
    }
	schedule_delayed_work(&chip->init_worker, msecs_to_jiffies(9000));

}


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/******************************************      IRQ/Threading Helpers       *****************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
FSC_S32 fusb_EnableInterrupts(void)
{
    FSC_S32 ret = 0;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return -ENOMEM;
    }

	wakeup_source_init(&chip->fusb302_wakelock, "fusb302wakelock");
    /* Set up IRQ for INT_N GPIO */
    ret = gpio_to_irq(chip->gpio_IntN); // Returns negative errno on error
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Unable to request IRQ for INT_N GPIO! Error code: %d\n", __func__, ret);
        chip->gpio_IntN_irq = -1;   // Set to indicate error
        fusb_GPIO_Cleanup();
        return ret;
    }
    chip->gpio_IntN_irq = ret;
    pr_info("%s - Success: Requested INT_N IRQ: '%d'\n", __func__, chip->gpio_IntN_irq);

    /* Use NULL thread_fn as we will be queueing a work function in the handler.
     * Trigger is active-low, don't handle concurrent interrupts.
     * devm_* allocation/free handled by system
     */
    ret = devm_request_threaded_irq(&chip->client->dev, chip->gpio_IntN_irq,
        _fusb_isr_intn, NULL, IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
        FUSB_DT_INTERRUPT_INTN, chip);

    if (ret)
    {
        dev_err(&chip->client->dev, "%s - Error: Unable to request threaded IRQ for INT_N GPIO! Error code: %d\n", __func__, ret);
        fusb_GPIO_Cleanup();
        return ret;
    }
    enable_irq_wake(chip->gpio_IntN_irq);

    return 0;
}

void fusb_StartTimer(struct hrtimer *timer, FSC_U32 time_us)
{
    ktime_t ktime;
    struct fusb30x_chip *chip = fusb30x_GetChip();
    if (!chip) {
        pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
        return;
    }

    /* Set time in (seconds, nanoseconds) */
    ktime = ktime_set(0, time_us * 1000);
    hrtimer_start(timer, ktime, HRTIMER_MODE_REL);

    return;
}

void fusb_StopTimer(struct hrtimer *timer)
{
    struct fusb30x_chip *chip = fusb30x_GetChip();
    if (!chip) {
        pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
        return;
    }

    hrtimer_cancel(timer);

    return;
}

FSC_U32 get_system_time_us(void)
{
    unsigned long long us;
    us = jiffies * 1000*1000 / HZ;
    return (FSC_U32)us;
}

/*******************************************************************************
* Function:        _fusb_isr_intn
* Input:           irq - IRQ that was triggered
*                  dev_id - Ptr to driver data structure
* Return:          irqreturn_t - IRQ_HANDLED on success, IRQ_NONE on failure
* Description:     Activates the core
********************************************************************************/
static irqreturn_t _fusb_isr_intn(FSC_S32 irq, void *dev_id)
{
    struct fusb30x_chip* chip = dev_id;

    //pr_info("FUSB %s\n", __func__);

    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return IRQ_NONE;
    }

    fusb_StopTimer(&chip->sm_timer);

    /* Schedule the process to handle the state machine processing */
    if (!chip->queued)
    {
      chip->queued = TRUE;
      queue_work(chip->highpri_wq, &chip->sm_worker);
    }

    return IRQ_HANDLED;
}

static enum hrtimer_restart fusb_sm_timer_callback(struct hrtimer *timer)
{
    struct fusb30x_chip* chip =
        container_of(timer, struct fusb30x_chip, sm_timer);

    if (!chip)
    {
        pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
        return HRTIMER_NORESTART;
    }

    /* Schedule the process to handle the state machine processing */
    if (!chip->queued)
    {
        chip->queued = TRUE;
        queue_work(chip->highpri_wq, &chip->sm_worker);
    }

    return HRTIMER_NORESTART;
}

static enum hrtimer_restart fusb_wake_unlock_timer_callback(struct hrtimer *timer)
{
    struct fusb30x_chip* chip =
        container_of(timer, struct fusb30x_chip, wake_unlock_timer);

    if (!chip)
    {
        pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
        return HRTIMER_NORESTART;
    }

    if (!timer)
    {
        pr_err("FUSB  %s - High-Resolution timer is NULL!\n", __func__);
        return HRTIMER_NORESTART;
    }

	pr_info("FUSB %s wake unlock, run pm_relax for suspend\n", __func__);
	__pm_relax(&chip->fusb302_wakelock);

    return HRTIMER_NORESTART;
}

void fusb_initialize_timer(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
	    pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
	    return;
	}

	/* HRTimer Setup */
	hrtimer_init(&chip->sm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	chip->sm_timer.function = fusb_sm_timer_callback;

	hrtimer_init(&chip->wake_unlock_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	chip->wake_unlock_timer.function = fusb_wake_unlock_timer_callback;

	pr_info("FUSB %s hrtimer initialized\n", __func__);
	return;
}


static void work_function(struct work_struct *work)
{
    FSC_U32 timeout = 0;

    struct fusb30x_chip* chip =
        container_of(work, struct fusb30x_chip, sm_worker);

    if (!chip)
    {
        pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
        return;
    }

    /* Disable timer while processing */
    fusb_StopTimer(&chip->sm_timer);
    fusb_StopTimer(&chip->wake_unlock_timer);
	__pm_stay_awake(&chip->fusb302_wakelock);
    down(&chip->suspend_lock);

#ifdef FSC_DEBUG
    /* Toggle debug GPIO when SM is called to measure thread tick rate */
    //dbg_fusb_GPIO_Set_SM_Toggle(TRUE);
#endif /* FSC_DEBUG */

    /* Run the state machine */
    core_state_machine(&chip->port);

#ifdef FSC_DEBUG
    /* Toggle debug GPIO when SM is called to measure thread tick rate */
      //dbg_fusb_GPIO_Set_SM_Toggle(FALSE);
#endif /* FSC_DEBUG */

    /* Double check the interrupt line before exiting */
    if (platform_get_device_irq_state_fusb302(chip->port.PortID))
    {
        queue_work(chip->highpri_wq, &chip->sm_worker);
    }
    else
    {
        chip->queued = FALSE;

        /* Scan through the timers to see if we need a timer callback */
        timeout = core_get_next_timeout(&chip->port);

        if (timeout > 0)
        {
            if (timeout == 1)
            {
                /* A value of 1 indicates that a timer has expired
                 * or is about to expire and needs further processing.
                 */
                queue_work(chip->highpri_wq, &chip->sm_worker);
            }
            else
            {
                /* A non-zero time requires a future timer interrupt */
                fusb_StartTimer(&chip->sm_timer, timeout);
            }
        }
    }

	/* delay timer to suspend ,3s */
    	fusb_StartTimer(&chip->wake_unlock_timer, 3000 * 1000);
    up(&chip->suspend_lock);
}

static void init_work_function(struct work_struct *delayed_work)
{
	struct fusb30x_chip* chip =
		container_of(delayed_work, struct fusb30x_chip, init_worker.work);
	pr_info("FUSB - %s\n", __func__);
	if (!chip)
	{
		pr_err("FUSB %s - Chip structure is NULL\n", __func__);
		return;
	}

	schedule_work(&chip->sm_worker);
}

/* add for platform system interface */

#ifdef CONFIG_DUAL_ROLE_USB_INTF

void fusb_force_source(struct dual_role_phy_instance *dual_role)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	pr_debug("FUSB - %s\n", __func__);
	core_set_source(&chip->port);

	if (dual_role)
		dual_role_instance_changed(dual_role);
}
void fusb_force_sink(struct dual_role_phy_instance *dual_role)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	pr_debug("FUSB - %s\n", __func__);
	core_set_sink(&chip->port);
	if (dual_role)
		dual_role_instance_changed(dual_role);
}
unsigned int fusb_get_dual_role_mode(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int mode = DUAL_ROLE_PROP_MODE_NONE;

	if (chip->port.CCPin != CCNone) {
		if (chip->port.sourceOrSink == SOURCE) {
			mode = DUAL_ROLE_PROP_MODE_DFP;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_MODE_DFP, mode = %d\n",
					__func__, mode);
		} else {
			mode = DUAL_ROLE_PROP_MODE_UFP;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_MODE_UFP, mode = %d\n",
					__func__, mode);
		}
	}
	pr_debug("FUSB - %s mode = %d\n", __func__, mode);
	return mode;
}
unsigned int fusb_get_dual_role_power(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int current_pr = DUAL_ROLE_PROP_PR_NONE;

	pr_debug("FUSB %s\n", __func__);

	if (chip->port.CCPin != CCNone) {
		if (chip->port.sourceOrSink == SOURCE) {
			current_pr = DUAL_ROLE_PROP_PR_SRC;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_PR_SRC, current_pr = %d\n",
					__func__, current_pr);
		} else {
			current_pr = DUAL_ROLE_PROP_PR_SNK;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_PR_SNK, current_pr = %d\n",
					__func__, current_pr);
		}
	}
	pr_debug("FUSB - %s current_pr = %d\n", __func__, current_pr);
	return current_pr;
}
unsigned int fusb_get_dual_role_data(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int current_dr = DUAL_ROLE_PROP_DR_NONE;

	pr_debug("FUSB %s\n", __func__);

	if (chip->port.CCPin != CCNone) {
		if (chip->port.PolicyIsDFP) {
			current_dr = DUAL_ROLE_PROP_DR_HOST;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_DR_HOST, current_dr = %d\n",
					__func__, current_dr);
		} else {
			current_dr = DUAL_ROLE_PROP_DR_DEVICE;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_DR_DEVICE, current_dr = %d\n",
					__func__, current_dr);
		}
	}
	pr_debug("FUSB - %s current_dr = %d\n", __func__, current_dr);
	return current_dr;
}
int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, unsigned int *val)
{
	unsigned int mode = DUAL_ROLE_PROP_MODE_NONE;
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		mode = fusb_get_dual_role_mode();
		*val = mode;
		break;
	case DUAL_ROLE_PROP_PR:
		mode = fusb_get_dual_role_power();
		*val = mode;
		break;
	case DUAL_ROLE_PROP_DR:
		mode = fusb_get_dual_role_data();
		*val = mode;
		break;
	default:
		pr_err("FUSB unsupported property %d\n", prop);
		return -ENODATA;
	}
	pr_debug("FUSB %s + prop=%d, val=%d\n", __func__, prop, *val);
	return 0;
}
int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, const unsigned int *val)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	unsigned int mode = fusb_get_dual_role_mode();

	pr_debug("FUSB %s\n", __func__);

	if (!chip) {
		pr_err("FUSB %s - Error: Chip structure is NULL!\n", __func__);
		return -1;
	}
	pr_debug("FUSB %s + prop=%d,val=%d,mode=%d\n",
			__func__, prop, *val, mode);
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		if (*val != mode) {
			if (mode == DUAL_ROLE_PROP_MODE_UFP)
				fusb_force_source(dual_role);
			else if (mode == DUAL_ROLE_PROP_MODE_DFP)
				fusb_force_sink(dual_role);
		}
		break;
	case DUAL_ROLE_PROP_PR:
		pr_debug("FUSB - %s DUAL_ROLE_PROP_PR\n", __func__);
		break;
	case DUAL_ROLE_PROP_DR:
		pr_debug("FUSB - %s DUAL_ROLE_PROP_DR\n", __func__);
		break;
	default:
		pr_debug("FUSB - %s default case\n", __func__);
		break;
	}
	return 0;
}
int dual_role_is_writeable(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop)
{
	pr_debug("FUSB - %s\n", __func__);
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		return 1;
		break;
	case DUAL_ROLE_PROP_DR:
	case DUAL_ROLE_PROP_PR:
		return 0;
		break;
	default:
		break;
	}
	return 1;
}
#endif

void stop_usb_host(struct fusb30x_chip* chip)
{
	pr_info("FUSB - %s\n", __func__);
	pr_info("FUSB - %s: typec_attach_old= %d\n",
		__func__, chip->tcpc->typec_attach_old);

	chip->tcpc->typec_attach_new = TYPEC_UNATTACHED;
	tcpci_notify_typec_state(chip->tcpc);
	tcpci_source_vbus(chip->tcpc,
		TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);
	chip->is_vbus_present = FALSE;
	chip->tcpc->typec_attach_old = TYPEC_UNATTACHED;
}
void start_usb_host(struct fusb30x_chip* chip, bool ss)
{
	pr_info("FUSB - %s, ss=%d\n", __func__, ss);
	chip->tcpc_desc->rp_lvl = TYPEC_CC_RP_1_5;
	if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SRC) {
		chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SRC;
		tcpci_source_vbus(chip->tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_5V, 1500);
		chip->is_vbus_present = TRUE;
		tcpci_notify_typec_state(chip->tcpc);
		chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SRC;
	}
}
void stop_usb_peripheral(struct fusb30x_chip* chip)
{
	pr_info("FUSB - %s\n", __func__);
	pr_info("FUSB - %s: typec_attach_old= %d\n",
		__func__, chip->tcpc->typec_attach_old);
	//tcpci_force_sink_vbus(chip->tcpc, false);
	chip->tcpc->typec_attach_new = TYPEC_UNATTACHED;
	tcpci_notify_typec_state(chip->tcpc);

	tcpci_sink_vbus(chip->tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_0V, 0);
	chip->tcpc->typec_attach_old = TYPEC_UNATTACHED;
}
void start_usb_peripheral(struct fusb30x_chip* chip)
{
	pr_info("FUSB - %s\n", __func__);
	if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SNK) {
		chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
		tcpci_notify_typec_state(chip->tcpc);
		chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
	}
}

void handle_core_event(FSC_U32 event, FSC_U8 portId,
		void *usr_ctx, void *app_ctx)
{
	static int usb_state = 0;
	FSC_U32 set_voltage;
	FSC_U32 op_current;
	//int type;
	struct fusb30x_chip* chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	pr_info("FUSB %s - Notice, event=0x%x, TC_ST=%d, PE_ST=%d\n",
		__func__, event, chip->port.ConnState, chip->port.PolicyState);
	switch (event) {
	case CC1_ORIENT:
	case CC2_ORIENT:
		pr_info("FUSB %s:CC Changed=0x%x\n", __func__, event);
		chip->tcpc->typec_polarity = (event == CC2_ORIENT) ? true : false;
		if (chip->port.sourceOrSink == SINK) {
			start_usb_peripheral(chip);
			usb_state = 1;
			pr_info("FUSB %s start_usb_peripheral\n", __func__);
		} else if (chip->port.sourceOrSink == SOURCE) {
			start_usb_host(chip, true);
			usb_state = 2;
			pr_info("FUSB %s start_usb_host\n", __func__);
		}
		__pm_stay_awake(&chip->fusb302_wakelock);
		break;
	case CC_NO_ORIENT:
		pr_info("FUSB %s:CC_NO_ORIENT=0x%x\n", __func__, event);
		if (usb_state == 2) {
			stop_usb_host(chip);
			usb_state = 0;
			pr_info("FUSB - %s stop_usb_host,event=0x%x,usb_state=%d\n",
				__func__, event, usb_state);
		} else {
			stop_usb_peripheral(chip);
			usb_state = 0;
			pr_info("FUSB - %s stop_usb_peripheral,event=0x%x,usb_state=%d\n",
				__func__, event, usb_state);
		}
		tcpci_source_vbus(chip->tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);
		chip->is_vbus_present = FALSE;

		__pm_stay_awake(&chip->fusb302_wakelock);
		break;
	case PD_STATE_CHANGED:
		if (chip->port.PolicyState == peSinkReady &&
			chip->port.PolicyHasContract == TRUE) {
			pr_info("FUSB %s update power_supply properties\n", __func__);

			pr_info("FUSB %s req_obj=0x%x, sel_src_caps=0x%x\n",
				__func__, chip->port.USBPDContract.object,
				chip->port.SrcCapsReceived[
				chip->port.USBPDContract.FVRDO.ObjectPosition - 1].object);

			set_voltage = chip->port.SrcCapsReceived[
				chip->port.USBPDContract.FVRDO.ObjectPosition - 1].FPDOSupply.Voltage;
			op_current = chip->port.USBPDContract.FVRDO.OpCurrent;

			pr_info("FUSB %s set_voltage=%d * 50mV, op_current=%d * 10mA\n",
				__func__, set_voltage, op_current);

		}
		break;
	case PD_NO_CONTRACT:
		pr_info("FUSB %s:PD_NO_CONTRACT=0x%x, PE_ST=%d\n",
			__func__, event, chip->port.PolicyState);
		break;
	case PD_NEW_CONTRACT:

		break;
	case DATA_ROLE:
		pr_info("FUSB %s:DATA_ROLE=0x%x\n", __func__, event);

#ifdef CONFIG_DUAL_ROLE_USB_INTF
		/* dual role usb--> 0:ufp, 1:dfp */
		if (chip->port.PolicyIsDFP) {
			chip->tcpc->dual_role_mode = 1;
		} else {
			chip->tcpc->dual_role_mode = 0;
		}
		/* dual role usb--> 0:Device, 1:Host */
		if (chip->port.PolicyIsDFP) {
			chip->tcpc->dual_role_dr = 0;
		} else {
			chip->tcpc->dual_role_dr = 1;
		}

		//dual_role_instance_changed(chip->tcpc->dr_usb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */
		tcpci_notify_role_swap(chip->tcpc,
			TCP_NOTIFY_DR_SWAP, chip->port.PolicyIsDFP);
		break;
	case POWER_ROLE:
		pr_info("FUSB - %s:POWER_ROLE event=0x%x", __func__, event);

		if (chip->tcpc->typec_attach_old == TYPEC_ATTACHED_SRC) {
			usb_state = 1;
			tcpci_source_vbus(chip->tcpc,
				TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);
			chip->is_vbus_present = FALSE;
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
			//tcpci_force_sink_vbus(chip->tcpc, true);
			tcpci_sink_vbus(chip->tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, 500);		
		} else if (chip->tcpc->typec_attach_old == TYPEC_ATTACHED_SNK) {
			usb_state = 2;
			//tcpci_force_sink_vbus(chip->tcpc, false);
			tcpci_sink_vbus(chip->tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_0V, 0);
			chip->tcpc_desc->rp_lvl = TYPEC_CC_RP_1_5;
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SRC;
			if (!chip->is_vbus_present) {
				tcpci_source_vbus(chip->tcpc,
					TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_5V, 1500);
				chip->is_vbus_present = TRUE;
			}
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SRC;
		}
		break;
	case AUDIO_ACC:
		pr_info("FUSB - %s:AUDIO_ACC=0x%x", __func__, event);
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_AUDIO) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_AUDIO;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_AUDIO;
		}
		break;
	case CUSTOM_SRC:
		pr_info("FUSB - %s:CUSTOM_SRC =0x%x", __func__, event);
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_CUSTOM_SRC) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_CUSTOM_SRC;
			usb_state = 1;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_CUSTOM_SRC;
			//tcpci_force_sink_vbus(chip->tcpc, true);
			tcpci_sink_vbus(chip->tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, 500);
		}

		break;


		break;
	default:
		pr_info("FUSB - %s:default=0x%x", __func__, event);
		break;
	}
}
void fusb_init_event_handler(void)
{
	/* max observer is 10 */
	register_observer(CC_ORIENT_ALL|PD_CONTRACT_ALL|POWER_ROLE|
			PD_STATE_CHANGED|DATA_ROLE|AUDIO_ACC|CUSTOM_SRC|
			EVENT_ALL,
			handle_core_event, NULL);
}

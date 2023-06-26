/*
 * File:   fusb30x_driver.c
 * Company: Fairchild Semiconductor
 *
 * Created on September 2, 2015, 10:22 AM
 */

/* Standard Linux includes */
#include <linux/init.h>                                                         // __init, __initdata, etc
#include <linux/module.h>                                                       // Needed to be a module
#include <linux/kernel.h>                                                       // Needed to be a kernel module
#include <linux/i2c.h>                                                          // I2C functionality
#include <linux/slab.h>                                                         // devm_kzalloc
#include <linux/types.h>                                                        // Kernel datatypes
#include <linux/errno.h>                                                        // EINVAL, ERANGE, etc
#include <linux/of_device.h>                                                    // Device tree functionality

/* Driver-specific includes */
#include "fusb30x_global.h"                                                     // Driver-specific structures/types
#include "platform_helpers.h"                                                   // I2C R/W, GPIO, misc, etc
#include "../core/core.h"                                                       // GetDeviceTypeCStatus
#include "../core/TypeC.h"

#ifdef FSC_DEBUG
#include "dfs.h"
#endif // FSC_DEBUG

#include "fusb30x_driver.h"

#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include <linux/usb/class-dual-role.h>
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

#include <linux/power_supply.h>
#include <mt-plat/mtk_boot.h>
#include <linux/regulator/consumer.h>
//#include <mt-plat/mtk_charger.h>
#include <mtk_charger.h>

/*K19A-104 add by wangchao at 2021/4/10 start*/
extern bool g_pd_is_present;
/*K19A-104 add by wangchao at 2021/4/10 end*/
/******************************************************************************
* Driver functions
******************************************************************************/
static int __init fusb30x_init(void)
{
    pr_info("FUSB  %s - Start driver initialization...\n", __func__);

	return i2c_add_driver(&fusb30x_driver);
}

static void __exit fusb30x_exit(void)
{
	i2c_del_driver(&fusb30x_driver);
    pr_debug("FUSB  %s - Driver deleted...\n", __func__);
}

static int fusb302_i2c_resume(struct device* dev)
{
    struct fusb30x_chip *chip = NULL;
        struct i2c_client *client = to_i2c_client(dev);

        if (client) {
            chip = i2c_get_clientdata(client);
                if (chip)
                up(&chip->suspend_lock);
        }
     return 0;
}

static int fusb302_i2c_suspend(struct device* dev)
{
    struct fusb30x_chip* chip = NULL;
        struct i2c_client* client =  to_i2c_client(dev);

        if (client) {
             chip = i2c_get_clientdata(client);
                 if (chip)
                    down(&chip->suspend_lock);
        }
        return 0;
}

 enum dual_role_property fusb_drp_properties[] = {
	DUAL_ROLE_PROP_SUPPORTED_MODES,
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};

int fusb30x_alert_status_clear(struct tcpc_device *tcpc,
						uint32_t mask)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
int fusb30x_fault_status_clear(struct tcpc_device *tcpc,
						uint8_t status)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
int fusb30x_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
int fusb30x_get_alert_status(struct tcpc_device *tcpc,
						uint32_t *alert)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_get_power_status(struct tcpc_device *tcpc,
						uint16_t *pwr_status)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
int fusb30x_get_fault_status(struct tcpc_device *tcpc,
						uint8_t *status)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_cc(struct tcpc_device *tcpc, int pull)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_low_rp_duty(struct tcpc_device *tcpc,
						bool low_rp)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_vconn(struct tcpc_device *tcpc, int enable)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_tcpc_deinit(struct tcpc_device *tcpc_dev)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
int fusb30x_get_mode(struct tcpc_device *tcpc, int *typec_mode)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	if (chip->port.sourceOrSink == SINK) {
		*typec_mode = 1;
	}
	else if (chip->port.sourceOrSink == SOURCE) {
		*typec_mode = 2;
	}
	else {
		*typec_mode = 0;
	}

	pr_info("%s - typec_mode:%d,sourceOrSink:%x\n",
		__func__, *typec_mode, chip->port.sourceOrSink);
	return 0;
}
int fusb30x_set_role(struct tcpc_device *tcpc, int state)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_is_low_power_mode(struct tcpc_device *tcpc_dev)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_low_power_mode(
		struct tcpc_device *tcpc_dev, bool en, int pull)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
int fusb30x_set_watchdog(struct tcpc_device *tcpc_dev, bool en)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
int fusb30x_set_intrst(struct tcpc_device *tcpc_dev, bool en)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_protocol_reset(struct tcpc_device *tcpc_dev)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static int fusb30x_retransmit(struct tcpc_device *tcpc)
{
	pr_info("%s: enter \n",__func__);
	return 0;
}
static struct tcpc_ops fusb30x_tcpc_ops = {
	.init = fusb30x_tcpc_init,
	.alert_status_clear = fusb30x_alert_status_clear,
	.fault_status_clear = fusb30x_fault_status_clear,
	.get_alert_mask = fusb30x_get_alert_mask,
	.get_alert_status = fusb30x_get_alert_status,
	.get_power_status = fusb30x_get_power_status,
	.get_fault_status = fusb30x_get_fault_status,
	.get_cc = fusb30x_get_cc,
	.set_cc = fusb30x_set_cc,
	.set_polarity = fusb30x_set_polarity,
	.set_low_rp_duty = fusb30x_set_low_rp_duty,
	.set_vconn = fusb30x_set_vconn,
//	.set_role = fusb30x_set_role,
//	.get_mode = fusb30x_get_mode,
	.deinit = fusb30x_tcpc_deinit,
	.is_low_power_mode = fusb30x_is_low_power_mode,
	.set_low_power_mode = fusb30x_set_low_power_mode,
	.set_watchdog = fusb30x_set_watchdog,
//	.set_intrst = fusb30x_set_intrst,
	.set_msg_header = fusb30x_set_msg_header,
	.set_rx_enable = fusb30x_set_rx_enable,
	.protocol_reset = fusb30x_protocol_reset,
	.get_message = fusb30x_get_message,
	.transmit = fusb30x_transmit,
	.set_bist_test_mode = fusb30x_set_bist_test_mode,
	.set_bist_carrier_mode = fusb30x_set_bist_carrier_mode,
	.retransmit = fusb30x_retransmit,
};

static void fusb30x_reset_delay_work(struct work_struct *work)
{
	int ret;
	struct power_supply *usb_psy = NULL;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip)
		return;
#if 0
	usb_psy = power_supply_get_by_name("charger");
	if (!usb_psy) {
		pr_info("FUSB - %s Could not get charger power_supply, defer probe\n",
			__func__);
		schedule_delayed_work(delayed_work, msecs_to_jiffies(1000));
		return;
	}
#endif
	usb_psy = power_supply_get_by_name("battery");
	if (!usb_psy) {
		pr_info("FUSB - %s Could not get battery power_supply, defer probe\n",
			__func__);
		schedule_delayed_work(delayed_work, msecs_to_jiffies(1000));
		return;
	}
	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_info("FUSB - %s Could not get USB power_supply, defer probe\n",
			__func__);
		schedule_delayed_work(delayed_work, msecs_to_jiffies(1000));
		return;
	}
#if 0
	if (!chip->chg1_consumer->cm) {
		pr_info("charger cm is not ready, wait\n");
		schedule_delayed_work(delayed_work, msecs_to_jiffies(1000));
		return;
	}
#endif
	fusb_reset();
	/* Enable interrupts after successful core/GPIO initialization */
	ret = fusb_EnableInterrupts();
	if (ret)
	{
		pr_err("FUSB  %s - Error: Unable to enable interrupts! Error code: %d\n", __func__, ret);
		return;
	}

	/* 
	 * Initialize the core and enable the state machine (NOTE: timer and GPIO must be initialized by now)
	 *  Interrupt must be enabled before starting 302 initialization 
	 */
	fusb_InitializeCore();
	pr_info("FUSB  %s - Core is initialized!\n", __func__);	
}

static int fusb30x_probe (struct i2c_client* client,
                          const struct i2c_device_id* id)
{
    int ret = 0;
    struct fusb30x_chip* chip  = NULL;
    struct i2c_adapter* adapter = NULL;
    struct regulator	*vbus = NULL;
    struct power_supply *usb_psy = NULL;
    struct tcpc_desc *desc = NULL;
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	struct dual_role_phy_desc *dual_desc = NULL;
	struct dual_role_phy_instance *dual_role = NULL;
#endif

	pr_info("FUSB - %s\n", __func__);
    if (!client) {
        pr_err("FUSB  %s - Error: Client structure is NULL!\n", __func__);
        return -EINVAL;
    }
    dev_info(&client->dev, "%s\n", __func__);

    /* Make sure probe was called on a compatible device */
	if (!of_match_device(fusb30x_dt_match, &client->dev))
	{
		dev_err(&client->dev, "FUSB  %s - Error: Device tree mismatch!\n", __func__);
		return -EINVAL;
	}
    pr_debug("FUSB  %s - Device tree matched!\n", __func__);
#if 0
	vbus = devm_regulator_get(&client->dev, "vbus");
	if (IS_ERR(vbus)) {
		dev_err(&client->dev,
			"FUSB - %s - Error: defer probe due to no vbus present\n",
			__func__);
		return -EPROBE_DEFER;
	}
	usb_psy = power_supply_get_by_name("charger");
	if (!usb_psy) {
		pr_info("FUSB - %s Could not get charger power_supply, defer probe\n",
			__func__);
		return -EPROBE_DEFER;
	}
	usb_psy = power_supply_get_by_name("battery");
	if (!usb_psy) {
		pr_info("FUSB - %s Could not get battery power_supply, defer probe\n",
			__func__);
		return -EPROBE_DEFER;
	}
	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_info("FUSB - %s Could not get USB power_supply, defer probe\n",
			__func__);
		return -EPROBE_DEFER;
	}
#endif
    /* Allocate space for our chip structure (devm_* is managed by the device) */
    chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip)
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to allocate memory for g_chip!\n", __func__);
		return -ENOMEM;
	}
    chip->client = client;                                                      // Assign our client handle to our chip
    fusb30x_SetChip(chip);                                                      // Set our global chip's address to the newly allocated memory
    pr_debug("FUSB  %s - Chip structure is set! Chip: %p ... g_chip: %p\n", __func__, chip, fusb30x_GetChip());

    /* Verify that the system has our required I2C/SMBUS functionality (see <linux/i2c.h> for definitions) */
    adapter = to_i2c_adapter(client->dev.parent);
    if (i2c_check_functionality(adapter, FUSB30X_I2C_SMBUS_BLOCK_REQUIRED_FUNC))
    {
        chip->use_i2c_blocks = true;
    }
    else
    {
        // If the platform doesn't support block reads, try with block writes and single reads (works with eg. RPi)
        // NOTE: It is likely that this may result in non-standard behavior, but will often be 'close enough' to work for most things
        dev_warn(&client->dev, "FUSB  %s - Warning: I2C/SMBus block read/write functionality not supported, checking single-read mode...\n", __func__);
        if (!i2c_check_functionality(adapter, FUSB30X_I2C_SMBUS_REQUIRED_FUNC))
        {
            dev_err(&client->dev, "FUSB  %s - Error: Required I2C/SMBus functionality not supported!\n", __func__);
            dev_err(&client->dev, "FUSB  %s - I2C Supported Functionality Mask: 0x%x\n", __func__, i2c_get_functionality(adapter));
            return -EIO;
        }
    }
    pr_debug("FUSB  %s - I2C Functionality check passed! Block reads: %s\n", __func__, chip->use_i2c_blocks ? "YES" : "NO");

    /* Assign our struct as the client's driverdata */
    i2c_set_clientdata(client, chip);
    pr_debug("FUSB  %s - I2C client data set!\n", __func__);

    /* Verify that our device exists and that it's what we expect */
    if (!fusb_IsDeviceValid())
    {
        dev_err(&client->dev, "FUSB  %s - Error: Unable to communicate with device!\n", __func__);
        return -EIO;
    }
    pr_debug("FUSB  %s - Device check passed!\n", __func__);
    /*K19A-104 add by wangchao at 2021/4/10 start*/
    g_pd_is_present = true;
    /*K19A-104 add by wangchao at 2021/4/10 end*/
    /* Initialize the chip lock */
    mutex_init(&chip->lock);

    /* Initialize the chip's data members */
    fusb_InitChipData();
    pr_debug("FUSB  %s - Chip struct data initialized!\n", __func__);

	fusb_initialize_timer();
	desc = devm_kzalloc(&client->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name = kzalloc(13, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;

	strcpy((char *)desc->name, "type_c_port0");
	desc->role_def = TYPEC_ROLE_TRY_SRC;
	desc->rp_lvl = TYPEC_CC_RP_1_5;
	chip->tcpc_desc = desc;

	dev_info(&client->dev, "%s: type_c_port0, role=%d\n",
		__func__, desc->role_def);

	chip->tcpc = tcpc_device_register(&client->dev,
			desc, &fusb30x_tcpc_ops, chip);
	chip->tcpc->typec_attach_old = TYPEC_UNATTACHED;
	chip->tcpc->typec_attach_new = TYPEC_UNATTACHED;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
    if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		dual_desc = devm_kzalloc(&client->dev, sizeof(struct dual_role_phy_desc),
					GFP_KERNEL);
		if (!dual_desc) {
			dev_err(&client->dev,
				"%s: unable to allocate dual role descriptor\n",
				__func__);
			return -ENOMEM;
		}
		dual_desc->name = "otg_default";
		dual_desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		dual_desc->get_property = dual_role_get_local_prop;
		dual_desc->set_property = dual_role_set_prop;
		dual_desc->properties = fusb_drp_properties;
		dual_desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
		dual_desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(&client->dev, dual_desc);
		dual_role->drv_data = client;
		chip->dual_role = dual_role;
		chip->dr_desc = dual_desc;
	}
#endif
     chip->usb_psy = usb_psy;
     chip->vbus = vbus;
     chip->is_vbus_present = FALSE;
     fusb_init_event_handler();

#if 0
    /* reset fusb302*/
    fusb_reset();
#endif
    /* Initialize semaphore*/
    sema_init(&chip->suspend_lock, 1);

    /* Initialize the platform's GPIO pins and IRQ */
    ret = fusb_InitializeGPIO();
    if (ret)
    {
        dev_err(&client->dev, "FUSB  %s - Error: Unable to initialize GPIO!\n", __func__);
        return ret;
    }
    pr_debug("FUSB  %s - GPIO initialized!\n", __func__);

    /* Initialize sysfs file accessors */
    fusb_Sysfs_Init();
    pr_debug("FUSB  %s - Sysfs nodes created!\n", __func__);

#ifdef FSC_DEBUG
    /* Initialize debugfs file accessors */
    fusb_DFS_Init();
    pr_debug("FUSB  %s - DebugFS nodes created!\n", __func__);
#endif // FSC_DEBUG
#if 0
    /* Enable interrupts after successful core/GPIO initialization */
    ret = fusb_EnableInterrupts();
    if (ret)
    {
        dev_err(&client->dev, "FUSB  %s - Error: Unable to enable interrupts! Error code: %d\n", __func__, ret);
        return -EIO;
    }

    /* Initialize the core and enable the state machine (NOTE: timer and GPIO must be initialized by now)
    *  Interrupt must be enabled before starting 302 initialization */
    fusb_InitializeCore();
    pr_debug("FUSB  %s - Core is initialized!\n", __func__);
#endif

	INIT_DELAYED_WORK(&chip->reset_delay_work, fusb30x_reset_delay_work);
	schedule_delayed_work(&chip->reset_delay_work, msecs_to_jiffies(1000));
	//chip->chg1_consumer = charger_manager_get_by_name(&client->dev, "charger_port1");

    dev_info(&client->dev, "FUSB  %s - FUSB30X Driver loaded successfully!\n", __func__);
	return ret;
}

static int fusb30x_remove(struct i2c_client* client)
{
    pr_debug("FUSB  %s - Removing fusb30x device!\n", __func__);

#ifdef FSC_DEBUG
    /* Remove debugfs file accessors */
    fusb_DFS_Cleanup();
    pr_debug("FUSB  %s - DebugFS nodes removed.\n", __func__);
#endif // FSC_DEBUG

    fusb_GPIO_Cleanup();
    pr_debug("FUSB  %s - FUSB30x device removed from driver...\n", __func__);
    return 0;
}

static void fusb30x_shutdown(struct i2c_client *client)
{
	FSC_U8 reset = 0x01; /* regaddr is 0x01 */
	FSC_U8 data = 0x40; /* data is 0x40 */
	FSC_U8 length = 0x01; /* length is 0x01 */
	FSC_BOOL ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB shutdown - Chip structure is NULL!\n");
		return;
	}

	core_enable_typec(&chip->port, false);
	ret = DeviceWrite(((FSC_U8)(&chip->port.I2cAddr)), regControl3, length, &data);
	if (ret != 0)
		pr_err("send hardreset failed, ret = %d\n", ret);

	SetStateUnattached(&chip->port);
	/* Enable the pull-up on CC1 */
	chip->port.Registers.Switches.PU_EN1 = 1;
	/* Disable the pull-down on CC1 */
	chip->port.Registers.Switches.PDWN1 = 0;
	/* Enable the pull-up on CC2 */
	chip->port.Registers.Switches.PU_EN2 = 1;
	/* Disable the pull-down on CC2 */
	chip->port.Registers.Switches.PDWN2 = 0;
	/* Commit the switch state */
	DeviceWrite(((FSC_U8)(&chip->port.I2cAddr)), regSwitches0, 1,
		&chip->port.Registers.Switches.byte[0]);
	fusb_GPIO_Cleanup();
	/* keep the cc open status 20ms */
	mdelay(20);
	ret = fusb_I2C_WriteData((FSC_U8)regReset, length, &reset);
	if (ret != 0)
		pr_err("device Reset failed, ret = %d\n", ret);

	pr_info("FUSB shutdown - FUSB30x device shutdown!\n");
}


/*******************************************************************************
 * Driver macros
 ******************************************************************************/
module_init(fusb30x_init);                                                      // Defines the module's entrance function
module_exit(fusb30x_exit);                                                      // Defines the module's exit function

MODULE_LICENSE("GPL");                                                          // Exposed on call to modinfo
MODULE_DESCRIPTION("Fairchild FUSB30x Driver");                                 // Exposed on call to modinfo
MODULE_AUTHOR("Fairchild");                        								// Exposed on call to modinfo

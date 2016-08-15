/*
 * SiIxxxx <Firmware or Driver>
 *
 * Copyright (C) 2011 Silicon Image Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/kthread.h>

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>

#include <mach/gpio.h>
#include <linux/mhl_api.h>

#include "sii_9244_api.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_defs.h"
#include "sii_reg_access.h"
#include "si_drv_mhl_tx.h"

#define SiI9244DRIVER_INTERRUPT_MODE   1

#define	APP_DEMO_RCP_SEND_KEY_CODE 0x41

bool_t vbusPowerState = true;

#if (VBUS_POWER_CHK == ENABLE)

static bool_t Sii9244_mhl_set_vbuspower(int on);

///////////////////////////////////////////////////////////////////////////////
//
// AppVbusControl
//
// This function or macro is invoked from MhlTx driver to ask application to
// control the VBUS power. If powerOn is sent as non-zero, one should assume
// peer does not need power so quickly remove VBUS power.
//
// if value of "powerOn" is 0, then application must turn the VBUS power on
// within 50ms of this call to meet MHL specs timing.
//
// Application module must provide this function.
//
void AppVbusControl(bool_t powerOn)
{
	if (powerOn) {

		MHLSinkOrDonglePowerStatusCheck();
		TX_API_PRINT(("[MHL]App: Peer's POW bit is set. Turn the VBUS power OFF here.\n"));

		Sii9244_mhl_set_vbuspower(false);
	} else {

		TX_API_PRINT(("[MHL]App: Peer's POW bit is cleared. Turn the VBUS power ON here.\n"));

		Sii9244_mhl_set_vbuspower(true);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////////////////////////// Linux platform related //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


#undef dev_info
#define dev_info _dev_info
#define MHL_DRIVER_NAME "sii9244drv"

/***** public type definitions ***********************************************/

typedef struct {
	struct task_struct *pTaskStruct;
	uint8_t pendingEvent;
	uint8_t pendingEventData;

} MHL_DRIVER_CONTEXT_T, *PMHL_DRIVER_CONTEXT_T;

/***** global variables ********************************************/

MHL_DRIVER_CONTEXT_T gDriverContext;

static struct i2c_client *mhl_Sii9244_page0 = NULL;
static struct i2c_client *mhl_Sii9244_page1 = NULL;
static struct i2c_client *mhl_Sii9244_page2 = NULL;
static struct i2c_client *mhl_Sii9244_cbus = NULL;

static struct mhl_platform_data *Sii9244_plat_data;
#if defined(CONFIG_TEGRA_HDMI_MHL_RCP)
static struct input_dev *sii9244_input_dev;
#endif

//------------------------------------------------------------------------------
// Array of timer values
//------------------------------------------------------------------------------

uint16_t Int_count = 0;

static bool_t match_id(const struct i2c_device_id *id,
		       const struct i2c_client *client)
{
	if (strcmp(client->name, id->name) == 0)
		return true;

	return false;
}

static bool_t Sii9244_mhl_power_setup(int on)
{
	int rc = 0;

	Sii9244_plat_data = mhl_Sii9244_page0->dev.platform_data;
	if (Sii9244_plat_data->power_setup) {
		rc = Sii9244_plat_data->power_setup(on);
		if (rc)
			goto fail;
		return true;
	}
      fail:
	return false;
}

static bool_t Sii9244_mhl_reset(int on)
{
	Sii9244_plat_data = mhl_Sii9244_page0->dev.platform_data;
	if (Sii9244_plat_data->reset) {
		Sii9244_plat_data->reset(on);
		return true;
	}
	return false;
}

static bool_t Sii9244_mhl_set_vbuspower(int on)
{
	int rc = 0;
	Sii9244_plat_data = mhl_Sii9244_page0->dev.platform_data;
	if (Sii9244_plat_data->set_vbuspower) {
		rc = Sii9244_plat_data->set_vbuspower(&mhl_Sii9244_page0->dev,
					on);
		if (rc)
			goto fail;
		return true;
	}
      fail:
	return false;
}

/*****************************************************************************/
/**
 * @brief Wait for the specified number of milliseconds to elapse.
 *
 *****************************************************************************/
void HalTimerWait(uint16_t m_sec)
{
	unsigned long time_usec = m_sec * 1000;

	usleep_range(time_usec, time_usec);
}

//------------------------------------------------------------------------------
uint8_t I2C_ReadByte(uint8_t SlaveAddr, uint8_t RegAddr)
{
	uint8_t ReadData = 0;

	switch (SlaveAddr) {
	case PAGE_0_0X72:
		ReadData = i2c_smbus_read_byte_data(mhl_Sii9244_page0, RegAddr);
		break;
	case PAGE_1_0X7A:
		ReadData = i2c_smbus_read_byte_data(mhl_Sii9244_page1, RegAddr);
		break;
	case PAGE_2_0X92:
		ReadData = i2c_smbus_read_byte_data(mhl_Sii9244_page2, RegAddr);
		break;
	case PAGE_CBUS_0XC8:
		ReadData = i2c_smbus_read_byte_data(mhl_Sii9244_cbus, RegAddr);
		break;
	}
	return ReadData;
}

//------------------------------------------------------------------------------
void I2C_WriteByte(uint8_t SlaveAddr, uint8_t RegAddr, uint8_t Data)
{
	switch (SlaveAddr) {
	case PAGE_0_0X72:
		i2c_smbus_write_byte_data(mhl_Sii9244_page0, RegAddr, Data);
		break;
	case PAGE_1_0X7A:
		i2c_smbus_write_byte_data(mhl_Sii9244_page1, RegAddr, Data);
		break;
	case PAGE_2_0X92:
		i2c_smbus_write_byte_data(mhl_Sii9244_page2, RegAddr, Data);
		break;
	case PAGE_CBUS_0XC8:
		i2c_smbus_write_byte_data(mhl_Sii9244_cbus, RegAddr, Data);
		break;
	}
}

#if defined(CONFIG_TEGRA_HDMI_MHL_RCP)
void rcp_report_event(unsigned int type, unsigned int code, int value)
{
	pr_info("code %u value %d\n", code, value);
	input_event(sii9244_input_dev, type, code, !!value);
	input_sync(sii9244_input_dev);
}
#endif

#ifdef SiI9244DRIVER_INTERRUPT_MODE

//------------------------------------------------------------------------------
static irqreturn_t Sii9244_mhl_interrupt(int irq, void *dev_id)
{
	uint8_t Int_count = 0;
	extern uint8_t fwPowerState;
	pr_info("mhl_interrupt\n");

	for (Int_count = 0; Int_count < 10; Int_count++) {
		SiiMhlTxDeviceIsr();
		pr_info("Int_count=%d::::::::Sii9244 interrupt happened\n",
			Int_count);
		msleep(20);
		if (POWER_STATE_D3 == fwPowerState)
			break;
	}
	return IRQ_HANDLED;
}
#else
static int SiI9244_mhl_loop(void *nothing)
{

	pr_info("%s EventThread starting up\n", MHL_DRIVER_NAME);

	while (true) {
		/*
		 *  Event loop
		 */
		SiiMhlTxDeviceIsr();
		msleep(20);
	}
	return 0;
}

/*****************************************************************************/
/**
 * @brief Start driver's event monitoring thread.
 *
 *****************************************************************************/
void StartEventThread(void)
{
	gDriverContext.pTaskStruct = kthread_run(SiI9244_mhl_loop,
						 &gDriverContext,
						 MHL_DRIVER_NAME);
}

/*****************************************************************************/
/**
 * @brief Stop driver's event monitoring thread.
 *
 *****************************************************************************/
void StopEventThread(void)
{
	kthread_stop(gDriverContext.pTaskStruct);

}
#endif

static struct i2c_device_id mhl_Sii9244_idtable[] = {
	{"mhl_Sii9244_page0", 0},
	{"mhl_Sii9244_page1", 0},
	{"mhl_Sii9244_page2", 0},
	{"mhl_Sii9244_cbus", 0},
};

/*
 * i2c client ftn.
 */
static int __devinit mhl_Sii9244_probe(struct i2c_client *client,
				       const struct i2c_device_id *dev_id)
{
#if defined(CONFIG_TEGRA_HDMI_MHL_RCP)
	int i = 0;
#endif
	int ret = 0;
	uint8_t pollIntervalMs;

	pr_info("%s:%d:\n", __func__, __LINE__);
	/*
	   init_timer(&g_mhl_1ms_timer);
	   g_mhl_1ms_timer.function = TimerTickHandler;
	   g_mhl_1ms_timer.expires = jiffies + 10*HZ;
	   add_timer(&g_mhl_1ms_timer);
	 */
	if (match_id(&mhl_Sii9244_idtable[0], client)) {
		mhl_Sii9244_page0 = client;
		dev_dbg(&client->adapter->dev, "attached %s "
			"into i2c adapter successfully\n", dev_id->name);
	} else if (match_id(&mhl_Sii9244_idtable[1], client)) {
		mhl_Sii9244_page1 = client;
		dev_dbg(&client->adapter->dev, "attached %s "
			"into i2c adapter successfully \n", dev_id->name);
	} else if (match_id(&mhl_Sii9244_idtable[2], client)) {
		mhl_Sii9244_page2 = client;
		dev_dbg(&client->adapter->dev, "attached %s "
			"into i2c adapter successfully \n", dev_id->name);
	} else if (match_id(&mhl_Sii9244_idtable[3], client)) {
		mhl_Sii9244_cbus = client;
		dev_dbg(&client->adapter->dev, "attached %s "
			"into i2c adapter successfully\n", dev_id->name);

	} else {
		dev_info(&client->adapter->dev,
			 "invalid i2c adapter: can not found dev_id matched\n");
		return -EIO;
	}

	if (mhl_Sii9244_page0 != NULL
	    && mhl_Sii9244_page1 != NULL
	    && mhl_Sii9244_page2 != NULL && mhl_Sii9244_cbus != NULL) {

		pr_info("\n============================================\n");
		pr_info("SiI9244 Linux Driver V1.22 \n");
		pr_info("============================================\n");


		if (false == Sii9244_mhl_power_setup(1))
			goto power_fail;

		if (false == Sii9244_mhl_reset(1))
			goto reset_fail;

		if (false ==
		    SiiMhlTxInitialize(pollIntervalMs = MONITORING_PERIOD))
			goto init_fail;

#ifdef SiI9244DRIVER_INTERRUPT_MODE
		ret = request_threaded_irq(mhl_Sii9244_page0->irq, NULL, Sii9244_mhl_interrupt, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					   mhl_Sii9244_page0->name,
					   mhl_Sii9244_page0);
		if (ret) {
			pr_err("%s:%d:Sii9244 interrupt failed\n", __func__,
			       __LINE__);
			free_irq(mhl_Sii9244_page0->irq,
				 mhl_Sii9244_page0->name);
		}

		else {
			enable_irq_wake(mhl_Sii9244_page0->irq);
			pr_info("%s:%d:Sii9244 interrupt is sucessful\n",
				__func__, __LINE__);
		}

#else
		StartEventThread();	/* begin monitoring for events */
#endif
#if defined(CONFIG_TEGRA_HDMI_MHL_RCP)
		sii9244_input_dev = input_allocate_device();
		if (!sii9244_input_dev) {
			pr_info("RCP: Fail to alloc memory for input_dev\n");
		}
		sii9244_input_dev->name = "mhl_rcp";
		sii9244_input_dev->id.bustype = BUS_I2C;
		sii9244_input_dev->id.version = 0x0001;
		sii9244_input_dev->id.product = 0x0001;
		sii9244_input_dev->id.vendor = 0x0001;
		sii9244_input_dev->dev.parent = &client->dev;

		if (Sii9244_plat_data->mhl_key_codes) {
			for (i = 0; i < Sii9244_plat_data->mhl_key_num; i++) {
				if (Sii9244_plat_data->mhl_key_codes[i])
					input_set_capability(sii9244_input_dev,
							     EV_KEY,
							     Sii9244_plat_data->
							     mhl_key_codes[i]);
			}
		}

		ret = input_register_device(sii9244_input_dev);
		if (ret)
			pr_err("RCP: Fail to register input device\n");
#endif
	}
	return ret;

init_fail:
	Sii9244_mhl_reset(0);
reset_fail:
	Sii9244_mhl_power_setup(0);
power_fail:
	return -EIO;
}

static int mhl_Sii9244_remove(struct i2c_client *client)
{
	dev_info(&client->adapter->dev, "detached s5p_mhl "
		 "from i2c adapter successfully\n");
	return 0;
}

static int mhl_Sii9244_suspend(struct i2c_client *cl, pm_message_t mesg)
{
	if (match_id(&mhl_Sii9244_idtable[0], cl))
		SiiMhlSwitchStatus(3);
	return 0;
};

static int mhl_Sii9244_resume(struct i2c_client *cl)
{
	return 0;
};

MODULE_DEVICE_TABLE(i2c, mhl_Sii9244_idtable);

static struct i2c_driver mhl_Sii9244_driver = {
	.driver = {
		   .name = "Sii9244_Driver",
		   },
	.id_table = mhl_Sii9244_idtable,
	.probe = mhl_Sii9244_probe,
	.remove = __devexit_p(mhl_Sii9244_remove),

	.suspend = mhl_Sii9244_suspend,
	.resume = mhl_Sii9244_resume,
};

static int __init mhl_Sii9244_init(void)
{
	return i2c_add_driver(&mhl_Sii9244_driver);
}

static void __exit mhl_Sii9244_exit(void)
{
	i2c_del_driver(&mhl_Sii9244_driver);
}

late_initcall(mhl_Sii9244_init);
module_exit(mhl_Sii9244_exit);

MODULE_VERSION("1.22");
MODULE_AUTHOR
    ("gary <qiang.yuan@siliconimage.com>, Silicon image SZ office, Inc.");
MODULE_DESCRIPTION("sii9244 transmitter Linux driver");
MODULE_ALIAS("platform:MHL_sii9244");

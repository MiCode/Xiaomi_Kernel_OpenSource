#include <linux/printk.h>       // pr_err, printk, etc
#include "fusb30x_global.h"     // Chip structure
#include "platform_helpers.h"   // Implementation details
#include "platform.h"

/*******************************************************************************
* Function:        platform_set/get_vbus_lvl_enable
* Input:           UInt    - port index for multiport systems
*                  VBUS_LVL - requested voltage
*                  Boolean - enable this voltage level
*                  Boolean - turn off other supported voltages
* Return:          Boolean - on or off
* Description:     Provide access to the VBUS control pins.
******************************************************************************/
void platform_set_vbus_lvl_enable(FSC_U8 port, VBUS_LVL level, FSC_BOOL enable,
                                  FSC_BOOL disableOthers)
{
    // Additional VBUS levels can be added here as needed.
    struct fusb30x_chip* chip = fusb30x_GetChip();

    switch (level)
    {
    case VBUS_LVL_5V:
        // Enable/Disable the 5V Source
        fusb_GPIO_Set_VBus5v(enable == TRUE ? true : false);
		if (!chip->is_vbus_present) {
			if(enable) {
				pr_info("FUSB - %s, trying to enable 5V vbus\n", __func__);
				tcpci_source_vbus(chip->tcpc,
					TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_5V, 1500);
				chip->is_vbus_present = TRUE;
			} else {
				pr_info("FUSB - %s, trying to disable 5V vbus\n", __func__);
				tcpci_source_vbus(chip->tcpc,
					TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);
				chip->is_vbus_present = FALSE;
			}
		}
        break;
    case VBUS_LVL_HV:
        // Enable/Disable the HV Source
        fusb_GPIO_Set_VBusOther(enable == TRUE ? true : false);

        if (disableOthers)
            fusb_GPIO_Set_VBus5v(false);
        break;
    default:
        // Otherwise, do nothing.
        break;
    }

    // Turn off other levels, if requested
    if ((level == VBUS_LVL_ALL) && (enable == FALSE))
    {
        // Turn off all levels
		pr_info("FUSB - %s, trying to disable vbus\n", __func__);
		tcpci_source_vbus(chip->tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);
		chip->is_vbus_present = FALSE;	
    }

    return;
}

FSC_BOOL platform_get_vbus_lvl_enable(FSC_U8 port, VBUS_LVL level)
{
    // Additional VBUS levels can be added here as needed.
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB %s - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}
    switch (level)
    {
    case VBUS_LVL_5V:
        // Return the state of the 5V VBUS Source.
        return chip->is_vbus_present;

    case VBUS_LVL_HV:
        // Return the state of the HV VBUS Source.
        return fusb_GPIO_Get_VBusOther() ? TRUE : FALSE;

    default:
        // Otherwise, return FALSE.
        return FALSE;
    }
}

/* PPS source functionality - not used in this platform */
void platform_set_pps_voltage(FSC_U8 port, FSC_U32 mv)
{
}

FSC_U16 platform_get_pps_voltage(FSC_U8 port)
{
    return 0;
}

void platform_set_pps_current(FSC_U8 port, FSC_U32 ma)
{
}

FSC_U16 platform_get_pps_current(FSC_U8 port)
{
    return 0;
}


/*******************************************************************************
* Function:         platform_set_vbus_discharge
* Input:            UInt - Port ID for multiport systems
*                   Boolean - TRUE = ON
* Return:           None
* Description:      Enable/Disable Vbus Discharge Path
******************************************************************************/
void platform_set_vbus_discharge(FSC_U8 port, FSC_BOOL enable)
{
   //fusb_GPIO_Set_Discharge(enable);
}

/*******************************************************************************
* Function:        platform_get_device_irq_state_fusb302
* Input:           UInt - Port ID for multiport systems
* Return:          Boolean.  TRUE = Interrupt Active
* Description:     Get the state of the INT_N pin.  INT_N is active low.  This
*                  function handles that by returning TRUE if the pin is
*                  pulled low indicating an active interrupt signal.
******************************************************************************/
FSC_BOOL platform_get_device_irq_state_fusb302(FSC_U8 port)
{
    return fusb_GPIO_Get_IntN() ? TRUE : FALSE;
}

/*******************************************************************************
* Function:        platform_i2c_write
* Input:           SlaveAddress - Slave device bus address
*                  RegAddrLength - Register Address Byte Length
*                  DataLength - Length of data to transmit
*                  PacketSize - Maximum size of each transmitted packet
*                  IncSize - Number of bytes to send before incrementing addr
*                  RegisterAddress - Internal register address
*                  Data - Buffer of char data to transmit
* Return:          Error state
* Description:     Write a char buffer to the I2C peripheral.
******************************************************************************/
FSC_BOOL platform_i2c_write(FSC_U8 SlaveAddress,
                        FSC_U8 RegAddrLength,
                        FSC_U8 DataLength,
                        FSC_U8 PacketSize,
                        FSC_U8 IncSize,
                        FSC_U32 RegisterAddress,
                        FSC_U8* Data)
{
    FSC_BOOL ret = FALSE;
    if (Data == NULL)
    {
        pr_err("%s - Error: Write data buffer is NULL!\n", __func__);
    }
    else
    {
        ret = fusb_I2C_WriteData((FSC_U8)RegisterAddress, DataLength, Data);
    }

    return ret;
}

/*******************************************************************************
* Function:        platform_i2c_read
* Input:           SlaveAddress - Slave device bus address
*                  RegAddrLength - Register Address Byte Length
*                  DataLength - Length of data to attempt to read
*                  PacketSize - Maximum size of each received packet
*                  IncSize - Number of bytes to recv before incrementing addr
*                  RegisterAddress - Internal register address
*                  Data - Buffer for received char data
* Return:          Error state.
* Description:     Read char data from the I2C peripheral.
******************************************************************************/
FSC_BOOL platform_i2c_read(FSC_U8 SlaveAddress,
                       FSC_U8 RegAddrLength,
                       FSC_U8 DataLength,
                       FSC_U8 PacketSize,
                       FSC_U8 IncSize,
                       FSC_U32 RegisterAddress,
                       FSC_U8* Data)
{
    FSC_BOOL ret = FALSE;
    FSC_S32 i = 0;
    FSC_U8 temp = 0;

    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return FALSE;
    }

    if (Data == NULL)
    {
        pr_err("%s - Error: Read data buffer is NULL!\n", __func__);
    }
    else if (DataLength > 1 && chip->use_i2c_blocks)
    {
        // Do block reads if able and necessary
        ret = fusb_I2C_ReadBlockData(RegisterAddress, DataLength, Data);
    }
    else
    {
        // Otherwise one byte at a time
        for (i = 0; i < DataLength; i++)
        {
            if (fusb_I2C_ReadData((FSC_U8)RegisterAddress + i, &temp))
            {
                Data[i] = temp;
                ret = TRUE;
            }
            else
            {
                ret = FALSE;
                break;
            }
        }
    }

    return ret;
}

/*****************************************************************************
* Function:        platform_delay_10us
* Input:           delayCount - Number of 10us delays to wait
* Return:          None
* Description:     Perform a software delay in intervals of 10us.
******************************************************************************/
void platform_delay_10us(FSC_U32 delayCount)
{
    fusb_Delay10us(delayCount);
}

/*******************************************************************************
* Function:        platform_set_data_role
* Input:           PolicyIsDFP - Current data role
* Return:          None
* Description:     A callback used by the core to report the new data role after
*                  a data role swap.
*******************************************************************************/
void platform_set_data_role(FSC_BOOL PolicyIsDFP)
{
    // Optional: Control Data Direction
}

/* Returns the current time in us */
FSC_U32 platform_current_time(void)
{
    return get_system_time_us();
}

/* Return a "packed" timestamp value of seconds and tenths of ms in the
 * format 0xSSSSMMMM
 */
FSC_U32 platform_timestamp(void)
{
    FSC_U32 time_ms = get_system_time_us() / 1000;  /* uS to mS */
    FSC_U32 timestamp = time_ms / 1000;   /* Get seconds value */

    time_ms -= timestamp * 1000;          /* Get remainder in ms */
    timestamp = timestamp << 16;          /* Shift seconds over */
    timestamp += time_ms * 10;            /* Add fractional (tenths) part */

    return timestamp;
}

FSC_U32 platform_get_log_time(void)
{
    return platform_timestamp();
}

FSC_U32 platform_get_system_time()
{
    return get_system_time_us();
}

#ifdef FSC_HAVE_DP
FSC_BOOL platform_dp_enable_pins(FSC_BOOL enable, FSC_U32 config)
{
    return TRUE;
}

void platform_dp_status_update(FSC_U32 status)
{
}
#endif /* FSC_HAVE_DP */

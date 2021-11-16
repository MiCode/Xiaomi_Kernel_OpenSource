#ifndef __FUSB_PLATFORM_HELPERS_H_
#define __FUSB_PLATFORM_HELPERS_H_

#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include <linux/usb/class-dual-role.h>
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

#define INIT_DELAY_MS   500     // Time to wait before initializing the device, in ms
#define RETRIES_I2C 3           // Number of retries for I2C reads/writes

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        GPIO Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*******************************************************************************
* Function:        fusb_InitializeGPIO
* Input:           none
* Return:          0 on success, error code otherwise
* Description:     Initializes the platform's GPIO
********************************************************************************/
FSC_S32 fusb_InitializeGPIO(void);

/*******************************************************************************
* Function:        fusb_GPIO_Set_VBus5v/Other
* Input:           set: true to set pin, false to clear
* Return:          none
* Description:     Sets or clears the VBus5V/Other gpio pin
********************************************************************************/
void fusb_GPIO_Set_VBus5v(FSC_BOOL set);
void fusb_GPIO_Set_VBusOther(FSC_BOOL set);
void fusb_GPIO_Set_Discharge(FSC_BOOL set);

/*******************************************************************************
* Function:        fusb_GPIO_Get_VBus5v/Other/IntN
* Input:           none
* Return:          true if set, false if clear
* Description:     Returns the value of the GPIO pin
********************************************************************************/
FSC_BOOL fusb_GPIO_Get_VBus5v(void);
FSC_BOOL fusb_GPIO_Get_VBusOther(void);
FSC_BOOL fusb_GPIO_Get_IntN(void);

#ifdef FSC_DEBUG
/*******************************************************************************
* Function:        fusb_GPIO_Set_SM_Toggle
* Input:           set - true to set high, false to set low
* Return:          none
* Description:     Sets or clears the GPIO pin
********************************************************************************/
void dbg_fusb_GPIO_Set_SM_Toggle(FSC_BOOL set);

/*******************************************************************************
* Function:        fusb_GPIO_Get_SM_Toggle
* Input:           none
* Return:          true if set, false if clear
* Description:     Returns the value of the GPIO pin
********************************************************************************/
FSC_BOOL dbg_fusb_GPIO_Get_SM_Toggle(void);
#endif  // FSC_DEBUG

/*******************************************************************************
* Function:        fusb_GPIO_Cleanup
* Input:           none
* Return:          none
* Description:     Frees any GPIO resources we may have claimed (eg. INT_N, VBus5V)
********************************************************************************/
void fusb_GPIO_Cleanup(void);


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************         I2C Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*******************************************************************************
* Function:        fusb_I2C_WriteData
* Input:           address - Destination register for write data
*                  length - Number of data bytes to write
*                  data - Data to write to the register at address
* Return:          true on success, false otherwise
* Description:     Write an unsigned byte to the I2C peripheral.
*                  Blocking, with retries
********************************************************************************/
FSC_BOOL fusb_I2C_WriteData(FSC_U8 address, FSC_U8 length, FSC_U8* data);

/*******************************************************************************
* Function:        fusb_I2C_ReadData
* Input:           address - Source register for read data
*                  data - Output storage
* Return:          true on success, false otherwise
* Description:     Read an unsigned byte from the I2C peripheral.
*                  Blocking, with retries
********************************************************************************/
FSC_BOOL fusb_I2C_ReadData(FSC_U8 address, FSC_U8* data);

/*******************************************************************************
* Function:        fusb_I2C_ReadBlockData
* Input:           address - Source register for read data
*                  length - Number of sequential bytes to read
*                  data - Output buffer
* Return:          true on success, false otherwise
* Description:     Read a block of unsigned bytes from the I2C peripheral.
********************************************************************************/
FSC_BOOL fusb_I2C_ReadBlockData(FSC_U8 address, FSC_U8 length, FSC_U8* data);


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Timer Interface        ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

/*******************************************************************************
* Function:        fusb_Delay10us
* Input:           delay10us: Time to delay, in 10us increments
* Return:          none
* Description:     Delays the given amount of time
********************************************************************************/
void fusb_Delay10us(FSC_U32 delay10us);

FSC_U32 get_system_time_us(void);

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        SysFS Interface        ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*******************************************************************************
* Function:        fusb_Sysfs_Init
* Input:           none
* Return:          none
* Description:     Initializes the sysfs objects (used for user-space access)
********************************************************************************/
void fusb_Sysfs_Init(void);

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Driver Helpers         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*******************************************************************************
* Function:        fusb_InitializeCore
* Input:           none
* Return:          none
* Description:     Calls core initialization functions
********************************************************************************/
void fusb_InitializeCore(void);

/*******************************************************************************
* Function:        fusb_IsDeviceValid
* Input:           none
* Return:          true if device is valid, false otherwise
* Description:     Verifies device contents via I2C read
********************************************************************************/
FSC_BOOL fusb_IsDeviceValid(void);

/*******************************************************************************
* Function:        fusb_reset
* Input:           none
* Return:          none
* Description:     reset fusb302 by assert SW_RES
********************************************************************************/
FSC_BOOL fusb_reset(void);

/*******************************************************************************
* Function:        fusb_InitChipData
* Input:           none
* Return:          none
* Description:     Initializes our driver chip struct data
********************************************************************************/
void fusb_InitChipData(void);

/*******************************************************************************
* Function:        fusb_initialize_timer
* Input:           none
* Return:          none
* Description:     Initializes high-resolution timer
********************************************************************************/
void fusb_initialize_timer(void);

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************       Threading Helpers       ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

/*******************************************************************************
* Function:        fusb_EnableInterrupts
* Input:           none
* Return:          0 on success, error code on failure
* Description:     Initializaes and enables the INT_N interrupt.
*                  NOTE: The interrupt may be triggered at any point once this is called.
*                  NOTE: The Int_N GPIO must be intialized prior to using this function.
********************************************************************************/
FSC_S32 fusb_EnableInterrupts(void);

void fusb_init_event_handler(void);
void fusb_force_source(struct dual_role_phy_instance *dual_role);
void fusb_force_sink(struct dual_role_phy_instance *dual_role);
unsigned int fusb_get_dual_role_mode(void);
unsigned int fusb_get_dual_role_power(void);
unsigned int fusb_get_dual_role_data(void);
int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, unsigned int *val);
int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, const unsigned int *val);

int dual_role_is_writeable(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop);
#endif  // __FUSB_PLATFORM_HELPERS_H_

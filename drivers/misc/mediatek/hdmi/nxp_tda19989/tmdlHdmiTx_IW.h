/**
 * Copyright (C) 2007 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmdlHdmiTx_IW.h
 *
 * \version       $Revision: 1 $
 *
 * \date          $Date: 07/08/07 16:00 $
 *
 * \brief         devlib driver component API for the TDA998x HDMI Transmitters
 *
 * \section refs  Reference Documents
 * TDA998x Driver - FRS.doc,
 * TDA998x Driver - tmdlHdmiTx - SCS.doc
 *
 * \section info  Change Information
 *
 * \verbatim

   $History: tmdlHdmiTx_IW.h $
 *
 * *****************  Version 1  *****************
 * User: J. Lamotte Date: 07/08/07   Time: 16:00
 * Updated in $/Source/tmdlHdmiTx/inc
 * initial version
 *

   \endverbatim
 *
*/

#ifndef TMDLHDMITX_IW_H
#define TMDLHDMITX_IW_H

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

#ifdef TMFL_OS_WINDOWS
#define _WIN32_WINNT 0x0500
#include "windows.h"
#endif

#include "tmNxTypes.h"
#include "tmdlHdmiTx_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                       TYPE DEFINITIONS                                     */
/*============================================================================*/
	typedef void (*tmdlHdmiTxIWFuncPtr_t) (void);
	typedef UInt8 tmdlHdmiTxIWTaskHandle_t;
	typedef UInt8 tmdlHdmiTxIWQueueHandle_t;
#ifdef TMFL_OS_WINDOWS
#error
	typedef HANDLE tmdlHdmiTxIWSemHandle_t;
#else
#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
	typedef unsigned long tmdlHdmiTxIWSemHandle_t;
#else
#error "platform error"
	typedef UInt8 tmdlHdmiTxIWSemHandle_t;
#endif
#endif

/**
 * \brief Enum listing all available devices for enable/disable interrupts
 */
	typedef enum {
		TMDL_HDMI_IW_RX_1,
		TMDL_HDMI_IW_RX_2,
		TMDL_HDMI_IW_TX_1,
		TMDL_HDMI_IW_TX_2,
		TMDL_HDMI_IW_CEC_1,
		TMDL_HDMI_IW_CEC_2
	} tmdlHdmiIWDeviceInterrupt_t;

/*============================================================================*/
/*                       EXTERN FUNCTION PROTOTYPES                           */
/*============================================================================*/

/******************************************************************************
    \brief  This function creates a task and allocates all the necessary
	    resources. Note that creating a task do not start it automatically,
	    an explicit call to tmdlHdmiTxIWTaskStart must be made.

    \param  pFunc       Pointer to the function that will be executed in the task context.
    \param  Priority    Priority of the task. The minimum priority is 0, the maximum is 255.
    \param  StackSize   Size of the stack to allocate for this task.
    \param  pHandle     Pointer to the handle buffer.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWTaskCreate(tmdlHdmiTxIWFuncPtr_t pFunc, UInt8 priority,
					     UInt16 stackSize, tmdlHdmiTxIWTaskHandle_t *pHandle);

/******************************************************************************
    \brief  This function destroys an existing task and frees resources used by it.

    \param  Handle  Handle of the task to be destroyed, as returned by tmdlHdmiTxIWTaskCreate.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWTaskDestroy(tmdlHdmiTxIWTaskHandle_t handle);

/******************************************************************************
    \brief  This function start an existing task.

    \param  Handle  Handle of the task to be started.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_ALREADY_STARTED: the function is already started
	    - TMDL_ERR_DLHDMITX_NOT_STARTED: the function is not started
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWTaskStart(tmdlHdmiTxIWTaskHandle_t handle);

/******************************************************************************
    \brief  This function blocks the current task for the specified amount time. This is a passive wait.

    \param  Duration    Duration of the task blocking in milliseconds.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWWait(UInt16 duration);

/******************************************************************************
    \brief  This function creates a message queue.

    \param  QueueSize   Maximum number of messages in the message queue.
    \param  pHandle     Pointer to the handle buffer.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWQueueCreate(UInt8 queueSize, tmdlHdmiTxIWQueueHandle_t *pHandle);

/******************************************************************************
    \brief  This function destroys an existing message queue.

    \param  Handle  Handle of the queue to be destroyed.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWQueueDestroy(tmdlHdmiTxIWQueueHandle_t handle);

/******************************************************************************
    \brief  This function sends a message into the specified message queue.

    \param  Handle  Handle of the queue that will receive the message.
    \param  Message Message to be sent.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource
	    - TMDL_ERR_DLHDMITX_FULL: the queue is full

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWQueueSend(tmdlHdmiTxIWQueueHandle_t handle, UInt8 message);

/******************************************************************************
    \brief  This function reads a message from the specified message queue.

    \param  Handle      Handle of the queue from which to read the message.
    \param  pMessage    Pointer to the message buffer.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWQueueReceive(tmdlHdmiTxIWQueueHandle_t handle, UInt8 *pMessage);

/******************************************************************************
    \brief  This function creates a semaphore.

    \param  pHandle Pointer to the handle buffer.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWSemaphoreCreate(tmdlHdmiTxIWSemHandle_t *pHandle);

/******************************************************************************
    \brief  This function destroys an existing semaphore.

    \param  Handle  Handle of the semaphore to be destroyed.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWSemaphoreDestroy(tmdlHdmiTxIWSemHandle_t handle);

/******************************************************************************
    \brief  This function acquires the specified semaphore.

    \param  Handle  Handle of the semaphore to be acquired.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWSemaphoreP(tmdlHdmiTxIWSemHandle_t handle);

/******************************************************************************
    \brief  This function releases the specified semaphore.

    \param  Handle  Handle of the semaphore to be released.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxIWSemaphoreV(tmdlHdmiTxIWSemHandle_t handle);

/******************************************************************************
    \brief  This function disables the interrupts for a specific device.

    \param

    \return The call result:
	    - TM_OK: the call was successful

******************************************************************************/
	void tmdlHdmiTxIWDisableInterrupts(tmdlHdmiIWDeviceInterrupt_t device);

/******************************************************************************
    \brief  This function enables the interrupts for a specific device.

    \param

    \return The call result:
	    - TM_OK: the call was successful

******************************************************************************/
	void tmdlHdmiTxIWEnableInterrupts(tmdlHdmiIWDeviceInterrupt_t device);

#ifdef __cplusplus
}
#endif
#endif				/* TMDLHDMITX_IW_H */
/*============================================================================*//*                            END OF FILE                                     *//*============================================================================*/

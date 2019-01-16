/*
 * $Date: 2015-01-08 05:30:24 -0800 (Thu, 08 Jan 2015) $
 * $Revision: 2039 $
 */
 
/**
 * @file vl6180x_i2c.h
 *
 * @brief  CCI interface to "raw i2c" translation layer
 */

#ifndef VL6180_I2C_H_
#define VL6180_I2C_H_

#include "vl6180x_platform.h"

/**
 * @defgroup cci_i2c  CCI to RAW I2C translation layer
 *
 * This optional tranlation layer is implemented in __platform/cci-i2c__ directory. If user uses this translation layer for his platform, only @a VL6180x_I2CRead() and
 * @a VL6180x_I2CWrite() functions need to be implemented. Also, some code adaption (via macro) is required for multi-threading and for multiple device support.
 *
 * File vl6180x_i2c.c implements device register access via raw i2c access. If the targeted application and platform has no multi-thread,  no multi-cpu and uses single
 * device, then nothing else is required than the 2 mandatory function : @a VL6180x_I2CRead() and @a VL6180x_I2CWrite().\n
 * In other cases, review and customize @a VL6180x_GetI2CAccess() and @a VL6180x_DoneI2CAccess() functions as well as @a #VL6180x_I2C_USER_VAR macro. This should be enough
 * to conform to a wide range of platform OS and application requirements .\n
 *
 * If your configured i2c for per device buffer via  @a #I2C_BUFFER_CONFIG == 2, you must implement @a VL6180x_GetI2cBuffer()
 *
 * __I2C Port sample__ \n
 * A __linux kernel__ port need a "long flags" var for its spin_lock in all functions. the following code example declares a spin lock "lock" in the custom device structure. \n
 * @code
struct MyVL6180Dev_t {
     struct VL6180xDevData_t StData;
     ...
     spinlock_t i2c_lock;
};
typedef struct MyVL6180Dev_t *VL6180xDev_t;

#define VL6180x_I2C_USER_VAR   unsigned long flags;
#define GetI2CAccess(dev)      spin_lock_irqsave(dev->i2c_lock, flags)
#define DoneI2CAccess(dev)     spin_unlock_irqrestore(dev->i2c_lock,flags)
@endcode

*  __POSIX pthread__ application porting could be as follows :\n
* @code
struct MyVL6180Dev_t {
    struct VL6180xDevData_t StData;
    ...
    pthread_mutex_t *lock;
};
typedef struct MyVL6180Dev_t *VL6180xDev_t;

#define VL6180x_I2C_USER_VAR        //no need
#define VL6180x_GetI2CAccess(dev)   pthread_mutex_lock(dev->lock)
#define VL6180x_DoneI2CAcces(dev)   pthread_mutex_unlock(dev->lock)
 * @endcode
 */

/**
 * @def I2C_BUFFER_CONFIG
 *
 * @brief Configure device register I2C access
 *
 * @li 0 : one GLOBAL buffer \n
 *   Use one global buffer of MAX_I2C_XFER_SIZE byte in data space \n
 *   This solution is not multi-device compliant nor multi-thread cpu safe \n
 *   It can be the best option for small 8/16 bit MCU without stack and limited ram  (STM8s, 80C51 ...)
 *
 * @li 1 : ON_STACK/local \n
 *   Use local variable (on stack) buffer \n
 *   This solution is multi-thread with use of i2c resource lock or mutex see @a VL6180x_GetI2CAccess() \n
 *
 * @li 2 : User defined \n
 *    Per device potentially dynamic allocated. Requires @a VL6180x_GetI2cBuffer() to be implemented.
 * @ingroup Configuration
 */
#define I2C_BUFFER_CONFIG 1

/**
 * @brief       Write data buffer to VL6180x device via i2c
 * @param dev   The device to write to
 * @param buff  The data buffer
 * @param len   The length of the transaction in byte
 * @return      0 on success
 * @ingroup cci_i2c
 */
int  VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t  *buff, uint8_t len);

/**
 *
 * @brief       Read data buffer from VL6180x device via i2c
 * @param dev   The device to read from
 * @param buff  The data buffer to fill
 * @param len   The length of the transaction in byte
 * @return      0 on success
 * @ingroup  cci_i2c
 */
int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len);


/**
 * @brief Declare any required variables used by i2c lock (@a VL6180x_DoneI2CAccess() and @a VL6180x_GetI2CAccess())
 * and buffer access : @a VL6180x_GetI2cBuffer()
 *
 * @ingroup cci_i2c
 */
#define VL6180x_I2C_USER_VAR

/**
 *  @brief Acquire lock or mutex for access to i2c data buffer and bus.\n
 *  Delete the default VL6180x_GetI2CAccess 'do-nothing' macro below if you decide to implement this function.
 *
 *  This function is used to perform i2c bus level and multiple access locking required for multi thread/proccess system.\n
 *  Multiple access (read and update) will lock once and do multiple basic i2c rd/wr to complete the overall transfer.\n
 *  When no locking is needed this can be a void macro.\n
 *
 * @param dev  the device
 * @ingroup cci_i2c
 */
void VL6180x_GetI2CAccess(VL6180xDev_t dev);

/**
 * @def VL6180x_GetI2CAccess
 * @brief Default 'do-nothing' macro for @a VL6180x_GetI2CAccess(). Delete if used.
 * @ingroup cci_i2c
 */
#define VL6180x_GetI2CAccess(dev) (void)0 /* TODO delete if function used */

/**
 * @brief Release acquired lock or mutex for i2c access.\n
 * Delete default VL6180x_DoneI2CAccess 'do-nothing' macro below if implementing that function.
 *
 * This function is used to release the acquired lock.
 * @param dev The device
 * @ingroup cci_i2c
 */
void VL6180x_DoneI2CAccess(VL6180xDev_t dev);

/** @def VL6180x_DoneI2CAcces
 * @brief Default 'do-nothing' macro for @a VL6180x_DoneI2CAcces(). Delete if used.
 * @ingroup cci_i2c
 */
#define VL6180x_DoneI2CAcces(dev) (void)0  /*TODO delete  if function used */

/**
 * @brief Provided data buffer for i2c access for at least n_byte.
 *
 * You must implement it when i2c @a #I2C_BUFFER_CONFIG is set to 2 (User defined).\n
 * This is used used in the context of #VL6180x_I2C_USER_VAR
 *
 * @param dev     The device
 * @param n_byte  Minimal number of byte
 * @return        The buffer (cannot fail return not checked)
 * @ingroup cci_i2c
 */
uint8_t *VL6180x_GetI2cBuffer(VL6180xDev_t dev, int n_byte);
#if I2C_BUFFER_CONFIG == 2
#error /* TODO add your macro of code here for VL6180x_GetI2cBuffer */
#endif





#endif /* VL6180_I2C_H_ */

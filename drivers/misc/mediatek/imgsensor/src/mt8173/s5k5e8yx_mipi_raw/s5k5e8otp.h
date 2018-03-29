
#ifndef __CAM_CAL_H
#define __CAM_CAL_H

#define CAM_CAL_DEV_MAJOR_NUMBER 226

/* CAM_CAL READ/WRITE ID */
#define S5K5E8OTP_DEVICE_ID							0x20
/* define I2C_UNIT_SIZE                                  1 */ /* in byte */
/* #define OTP_START_ADDR                            0x0A04 */
/* #define OTP_SIZE                                      24 */

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

#endif /* __CAM_CAL_H */


/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*                     I2C/SPI Comunication				 *
*                                                                        *
**************************************************************************
**************************************************************************

*/




#include "ftsSoftware.h"
#include "ftsCrossCompile.h"

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define I2C_RETRY			3
#define I2C_WAIT_BEFORE_RETRY		2

int openChannel(struct i2c_client *clt);
struct device *getDev(void);
struct i2c_client *getClient(void);
int fts_readCmd(u8 *cmd, int cmdLenght, u8 *outBuf, int byteToRead);
int fts_writeCmd(u8 *cmd, int cmdLenght);
int fts_writeFwCmd(u8 *cmd, int cmdLenght);
int writeReadCmd(u8 *writeCmd, int writeCmdLenght, u8 *readCmd, int readCmdLenght, u8 *outBuf, int byteToRead);
int readCmdU16(u8 cmd, u16 address, u8 *outBuf, int byteToRead, int hasDummyByte);
int writeCmdU16(u8 WriteCmd, u16 address, u8 *dataToWrite, int byteToWrite);
int writeCmdU32(u8 writeCmd1, u8 writeCmd2, u32 address, u8 *dataToWrite, int byteToWrite);
int writeReadCmdU32(u8 wCmd, u8 rCmd, u32 address, u8 *outBuf, int byteToRead, int hasDummyByte);

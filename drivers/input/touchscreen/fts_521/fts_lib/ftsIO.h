/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                     I2C/SPI Communication								 *
*                                                                        *
**************************************************************************
**************************************************************************

*/
/*!
* \file ftsIO.h
* \brief Contains all the definitions and prototypes used and implemented in ftsIO.c
*/

#ifndef FTS_IO_H
#define FTS_IO_H

#include "ftsSoftware.h"

#define I2C_RETRY					3
#define I2C_WAIT_BEFORE_RETRY		2

#ifdef I2C_INTERFACE
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
struct i2c_client *getClient(void);
#else
#include <linux/spi/spi.h>
struct spi_device *getClient(void);
#endif



int openChannel(void *clt);
struct device *getDev(void);



/*************** NEW I2C API ****************/
int changeSAD(u8 sad);
int fts_read(u8 *outBuf, int byteToRead);
int fts_writeRead(u8 *cmd, int cmdLength, u8 *outBuf, int byteToRead);
int fts_write(u8 *cmd, int cmdLength);
int fts_read_dma_safe(u8 *outBuf, int byteToRead);
int fts_writeRead_dma_safe(u8 *cmd, int cmdLength, u8 *outBuf, int byteToRead);
int fts_write_dma_safe(u8 *cmd, int cmdLength);
int fts_writeFwCmd(u8 *cmd, int cmdLenght);
int fts_writeThenWriteRead(u8 *writeCmd1, int writeCmdLength, u8 *readCmd1,
			   int readCmdLength, u8 *outBuf, int byteToRead);
int fts_writeU8UX(u8 cmd, AddrSize addrSize, u64 address, u8 *data, int dataSize);
int fts_writeReadU8UX(u8 cmd, AddrSize addrSize, u64 address, u8 *outBuf, int byteToRead, int hasDummyByte);
int fts_writeU8UXthenWriteU8UX(u8 cmd1, AddrSize addrSize1, u8 cmd2, AddrSize addrSize2, u64 address, u8 *data, int dataSize);
int fts_writeU8UXthenWriteReadU8UX(u8 cmd1, AddrSize addrSize1, u8 cmd2, AddrSize addrSize2, u64 address, u8 *outBuf, int count, int hasDummyByte);
#endif

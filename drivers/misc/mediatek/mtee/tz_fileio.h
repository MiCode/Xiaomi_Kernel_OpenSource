/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef DRIVER_FILE_PR_IO_H
#define DRIVER_FILE_PR_IO_H
#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#define UINT64 unsigned long long
#define INT64 long long
/** FILE_Open
 *  Open a file for read or write
 *
 *  @param  path         File path to open
 *  @param  flags        Access flag see open(2)
 *  @param  mode         The mode/permission for created file, see open(2)
 *  @retval              File handle
 */
struct file *FILE_Open(const char *path, int flags, int mode);

/** FILE_Read
 *  Read data from a file.
 *
 *  @param  file         Opened file handle
 *  @param  offset       Pointer to begin offset to read.
 *  @param  data         user's buffer address.
 *  @param  size         readdata length in bytes.
 *  @retval   >0         SUCCESS, return actual bytes read.
 *  @retval   <0         Fail, errno
 */
int FILE_Read(struct file *file, unsigned char *data, unsigned int size,
		unsigned long long *offset);

/** FILE_Write
 *  Write data from a file.
 *
 *  @param  file         Opened file handle
 *  @param  offset       Pointer to begin offset to Write.
 *  @param  data         user's buffer address.
 *  @param  size         Writedata length in bytes.
 *  @retval   >0         SUCCESS, return actual bytes Write.
 *  @retval   <0         Fail, errno
 */
int FILE_Write(struct file *file, unsigned char *data, unsigned int size,
		unsigned long long *offset);

/** FILE_ReadData
 *  Read data from a file.
 *
 *  @param  path         File path name to read.
 *  @param  u4Offset     begin offset to read.
 *  @param  pData        user's buffer address.
 *  @param  i4Length     readdata length in bytes.
 *  @retval   >0         SUCCESS, return actual bytes read.
 *  @retval   <0         Fail, errno
 */
int FILE_ReadData(const char *path, unsigned int u4Offset, char *pData,
			int i4Length);


/** FILE_WriteData
 *  Write data to a file.
 *
 *  @param  path         File path name to write.
 *  @param  u4Offset     begin offset to write.
 *  @param  pData        user's buffer address.
 *  @param  i4Length     writedata length in bytes.
 *  @retval   >0         SUCCESS, return actual bytes writen.
 *  @retval   <0         Fail, errno
 */
int FILE_WriteData(const char *path, unsigned int u4Offset, char *pData,
			int i4Length);
#endif

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_fs.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT65XX CCCI FS Proxy Driver
 *
 ****************************************************************************/

#ifndef __CCCI_FS_H__
#define __CCCI_FS_H__
/*
 * define IOCTL commands
 */
#define CCCI_FS_IOC_MAGIC 'K'
#define CCCI_FS_IOCTL_GET_INDEX _IO(CCCI_FS_IOC_MAGIC, 1)
#define CCCI_FS_IOCTL_SEND      _IOR(CCCI_FS_IOC_MAGIC, 2, unsigned int)

#define  CCCI_FS_MAX_BUF_SIZE   (16384)
#define  CCCI_FS_MAX_BUFFERS    (5)

typedef struct
{
    unsigned length;
    unsigned index;
} fs_stream_msg_t;
  

typedef struct
{
    unsigned fs_ops;
    unsigned char buffer[CCCI_FS_MAX_BUF_SIZE];
} fs_stream_buffer_t;

#define CCCI_FS_SMEM_SIZE (sizeof(fs_stream_buffer_t) * CCCI_FS_MAX_BUFFERS)

#endif // __CCCI_FS_H__

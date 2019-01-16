#ifndef SEC_OSAL_H
#define SEC_OSAL_H

/**************************************************************************
 *  Operating System Abstract Layer - ERROR Definition
 **************************************************************************/
#define OSAL_FILE_NULL          (0)
#define OSAL_FILE_OPEN_FAIL     (-1)
#define OSAL_FILE_CLOSE_FAIL    (-2)
#define OSAL_FILE_SEEK_FAIL     (-3)
#define OSAL_FILE_GET_POS_FAIL  (-4)
#define OSAL_FILE_READ_FAIL     (-5)


/**************************************************************************
 *  Operating System Abstract Layer - External Function
 **************************************************************************/
extern void osal_kfree(void *buf);
extern void *osal_kmalloc(unsigned int size);
extern unsigned long osal_copy_from_user(void *to, void *from, unsigned long size);
extern unsigned long osal_copy_to_user(void *to, void *from, unsigned long size);
extern int osal_hacc_lock(void);
extern void osal_hacc_unlock(void);
extern int osal_verify_lock(void);
extern void osal_verify_unlock(void);
extern int osal_secro_lock(void);
extern void osal_secro_unlock(void);
extern int osal_secro_v5_lock(void);
extern void osal_secro_v5_unlock(void);
extern int osal_mtd_lock(void);
extern void osal_mtd_unlock(void);
extern int osal_rid_lock(void);
extern void osal_rid_unlock(void);
extern void osal_msleep(unsigned int msec);
extern void osal_assert(unsigned int val);
extern int osal_set_kernel_fs(void);
extern void osal_restore_fs(void);
extern int osal_filp_open_read_only(const char *file_path);
extern void *osal_get_filp_struct(int fp_id);
extern int osal_filp_close(int fp_id);
extern long long osal_filp_seek_set(int fp_id, long long off);
extern long long osal_filp_seek_end(int fp_id, long long off);
extern long long osal_filp_pos(int fp_id);
extern long osal_filp_read(int fp_id, char *buf, unsigned long len);
extern long osal_is_err(int fp_id);

/**************************************************************************
 *  Operating System Abstract Layer - Macro
 **************************************************************************/
#define SEC_ASSERT(a) osal_assert(a)

#define ASF_FILE int
#define ASF_FILE_NULL OSAL_FILE_NULL
#define ASF_GET_DS osal_set_kernel_fs();
#define ASF_PUT_DS osal_restore_fs();
#define ASF_OPEN(file_name) osal_filp_open_read_only(file_name)
#define ASF_FILE_ERROR(fp) (fp == OSAL_FILE_NULL)
#define ASF_CLOSE(fp) osal_filp_close(fp)
#define ASF_SEEK_SET(fp, off) osal_filp_seek_set(fp, off)
#define ASF_SEEK_END(fp, off) osal_filp_seek_end(fp, off)
#define ASF_FILE_POS(fp) osal_filp_pos(fp)
#define ASF_MALLOC(len) osal_kmalloc(len)
#define ASF_FREE(buf) osal_kfree(buf)
#define ASF_READ(fp, buf, len) osal_filp_read(fp, buf, len)
#define ASF_STRTOK(str, delim) strsep(&str, delim)
#define ASF_IS_ERR(fp) osal_is_err(fp)

#endif				/* SEC_OSAL_H */

// MIUI ADD: MiSight_LogEnhance

#ifndef _CHRDEV_MIEV_COMMON_H_
#define _CHRDEV_MIEV_COMMON_H_

/* write and read buffer max size */
#define BUF_MAX_SIZE 4096

int write_kbuf(char **kbuf, int offset, int size);
char** miev_get_work_msg(void);
void miev_release_work_msg(char **kbuf);
#endif
// END MiSight_LogEnhance

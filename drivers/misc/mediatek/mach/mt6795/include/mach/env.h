#ifndef __ENV_H__
#define __ENV_H__ 

#include <linux/ioctl.h> 

#define CFG_ENV_SIZE		0x4000     /*16KB*/
#define CFG_ENV_OFFSET		0x20000    /*128KB*/
#define ENV_PART		"PARA"

struct env_struct {
	char sig_head[8]; 
	char *env_data;
	char sig_tail[8];  
	int checksum; 
};

#define ENV_MAGIC		'e'
#define ENV_READ		_IOW(ENV_MAGIC, 1, int)
#define ENV_WRITE		_IOW(ENV_MAGIC, 2, int)

struct env_ioctl {
	char *name;
	int name_len;
	char *value;
	int value_len;	
};
extern int set_env(char *name,char *value);
extern char *get_env(const char *name);
#endif

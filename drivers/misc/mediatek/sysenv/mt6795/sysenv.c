#include <linux/module.h>
#include <linux/errno.h>      /* error define */
#include <linux/slab.h>
#include <linux/types.h>      /*typedef used*/
#include <linux/fs.h>         /*flip_open API*/
#include <linux/proc_fs.h>    /*proc_create API*/
#include <linux/statfs.h>     /* kstatfs struct */
#include <linux/file.h>       /*kernel write and kernel read*/
#include <asm/uaccess.h>      /*copy_to_user copy_from_user */
#include <mach/env.h>

static char env_get_char(int index);
static char *env_get_addr(int index);
static int envmatch(char *s1, int i2);
static int write_env_area(char *env_buf);
static int read_env_area(char *env_buf);
static void env_init(void);
static int get_env_valid_length(void);
static char *findenv(const char *name);

static char ENV_SIG[8] = "ENV_v1";
struct env_struct g_env;
static int env_valid = 0;
static char *env_buffer = NULL;
static int env_init_done = 0;

#define PARA_PATH   "/dev/block/platform/mtk-msdc.0/by-name/para"
#define MODULE_NAME "KL_ENV"
#define CFG_ENV_DATA_SIZE	\
	(CFG_ENV_SIZE-sizeof(g_env.checksum)-sizeof(g_env.sig_head)	\
	-sizeof(g_env.sig_tail))
#define CFG_ENV_DATA_OFFSET 	\
	(sizeof(g_env.sig_head))
#define CFG_ENV_SIG_1_OFFSET	\
	(CFG_ENV_SIZE - sizeof(g_env.checksum)-sizeof(g_env.sig_tail))
#define CFG_ENV_CHECKSUM_OFFSET		\
	(CFG_ENV_SIZE - sizeof(g_env.checksum))

static ssize_t 
env_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	char p[32];
	char *page = (char *)p;
	int err = 0;
	ssize_t len = 0;
	int env_valid_length = 0;
	
	env_init();

	if (!env_valid) {
		pr_debug("[%s]read no env valid\n", MODULE_NAME);
		page += sprintf(page, "\nno env valid\n");
		len = page - &p[0];
 			
 		if (*ppos >= len) {
			return 0;
		}
 		err = copy_to_user(buf,(char *)p,len);
 		*ppos += len;
 		if (err) {
 			return err;
 		}
 		return len;	
	} else {	
		env_valid_length = get_env_valid_length();
		if (*ppos >= env_valid_length) {
			return 0;
		}
		if ((size + *ppos) > env_valid_length) {
			size = env_valid_length - *ppos;
		}
		err = copy_to_user(buf, g_env.env_data + *ppos, size);
 		if (err) {
 			return err;
 		}
 		*ppos += size;
 		return size;
	}
}

static ssize_t 
env_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char *buffer = NULL;
	int ret = 0, i, v_index = 0;
	
	buffer = (char *)kzalloc(size,GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		pr_err("[%s]alloc buffer fail\n", MODULE_NAME);
		goto fail_malloc;	
	}

	if (copy_from_user(buffer, buf, size)) {
		ret = -EFAULT;
		goto end;
	}
	/*parse buffer into name and value*/
	for (i = 0; i < size; i++) {
		if (buffer[i] == '=') {
			v_index = i+1;
			buffer[i] = '\0';
			buffer[size-1] = '\0';
			break;
		}
	}
	if (i == size) {
		ret = -EFAULT;
		pr_err("[%s]write fail\n", MODULE_NAME);
		goto end;	
	} else {
		pr_notice("[%s]name :%s,value: %s\n", 
			  MODULE_NAME, buffer, buffer+v_index);
	}
	ret = set_env(buffer, buffer + v_index);
end:
	kfree(buffer);
fail_malloc:
	if (ret)
		return ret;
	else
		return size;
}

static long 
env_proc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct env_ioctl en_ctl;
	int ret = 0;
	char *name_buf = NULL;
	char *value_buf = NULL;
	char *value_r = NULL;

	memset(&en_ctl, 0x00, sizeof(struct env_ioctl));	
	
	if (copy_from_user((void *)&en_ctl, (void *)arg, sizeof(struct env_ioctl))) {
		ret = -EFAULT;
		goto fail;
	}
	if (en_ctl.name_len <= 0 || en_ctl.value_len <= 0) {
		ret = -EPERM;
		goto end;	
	}
	name_buf = (char *)kmalloc(en_ctl.name_len, GFP_KERNEL);
	if (!name_buf) {
		ret = -ENOMEM;
		goto fail;	
	}
	value_buf = (char *)kmalloc(en_ctl.value_len, GFP_KERNEL);
	if (!value_buf) {
		ret = -ENOMEM;
		goto fail_malloc;	
	}
	if (copy_from_user((void *)name_buf, (void *)en_ctl.name, en_ctl.name_len)) {
		ret = -EFAULT;
		goto end;
	}
	if (*name_buf == '\0') {
		ret = 0;
		goto end;	
	}
	switch (cmd) {
	case ENV_READ:
		value_r = get_env(name_buf);
		if (value_r == NULL) {
			ret = -EPERM;
			pr_notice("[%s]cann't find name=%s\n", 
				  MODULE_NAME, name_buf);
			goto end;
		}
		if ((strlen(value_r) + 1) > en_ctl.value_len) {
			ret = -EFAULT;
			goto end;
		}
		if (copy_to_user((void *)en_ctl.value, (void *)value_r, strlen(value_r) + 1)) {
			ret = -EFAULT;
			goto end;
		}
		break;
	case ENV_WRITE:
		if (copy_from_user((void *)value_buf, (void *)en_ctl.value, en_ctl.value_len)) {
			ret = -EFAULT;
			goto end;
		}
		ret = set_env(name_buf, value_buf);
		break;
	default:
		pr_notice("[%s]Undefined command\n", MODULE_NAME);
		ret = -EINVAL;
		goto end;
	}
end:
	kfree(value_buf);
fail_malloc:
	kfree(name_buf);	
fail:
	return ret;
}

static const struct file_operations env_proc_fops = {
	.read = env_proc_read,
	.write = env_proc_write,
	.unlocked_ioctl = env_proc_ioctl,
};

static int get_env_valid_length()
{
	int len = 0;
	if (!env_valid) {
		return 0;
	}
	for(len = 0;len < CFG_ENV_DATA_SIZE; len++) {
		if (g_env.env_data[len] == '\0' && g_env.env_data[len+1] == '\0')
			break;	
	}
	return len;
}

static void env_init(void)
{
	int ret, i;
	int checksum = 0;
	
	if (env_init_done) {
		return;
	}

	pr_notice("[%s]ENV initialize\n", MODULE_NAME);

	env_buffer = (char *)kzalloc(CFG_ENV_SIZE, GFP_KERNEL);
	if (!env_buffer) {
		pr_err("[%s]malloc env buffer fail\n", MODULE_NAME);
		return;
	}
	
	g_env.env_data = env_buffer + CFG_ENV_DATA_OFFSET;
	ret = read_env_area(env_buffer);
	if (ret < 0) {
		pr_err("[%s]read_env_area fail\n", MODULE_NAME);
		goto end_read_fail;
	}

	memcpy(g_env.sig_head, env_buffer, sizeof(g_env.sig_head));
	memcpy(g_env.sig_tail, env_buffer + CFG_ENV_SIG_1_OFFSET, sizeof(g_env.sig_tail));
	
	if (!strcmp(g_env.sig_head,ENV_SIG) && !strcmp(g_env.sig_tail,ENV_SIG)) {		
		g_env.checksum = *((int *)env_buffer + CFG_ENV_CHECKSUM_OFFSET/4);
		for (i = 0; i < CFG_ENV_DATA_SIZE; i++) {
			checksum += g_env.env_data[i];
		}
		if (checksum != g_env.checksum) {
			pr_err("[%s]checksum mismatch\n", MODULE_NAME);
			env_valid = 0;
			goto end;
		} else {
			pr_notice("[%s]ENV initialize sucess\n", 
				  MODULE_NAME);
			env_valid = 1;
		}
	} else {
		pr_err("[%s]ENV SIG Wrong\n", MODULE_NAME);
		env_valid = 0;
		goto end;
	}
end:
	if (!env_valid) 
	{
		memset(env_buffer, 0x00, CFG_ENV_SIZE);
	}
	env_init_done = 1;
	return;
end_read_fail:
	env_init_done = 0;
}

static char *findenv(const char *name)
{
	int i, nxt, val;
	for (i = 0; env_get_char(i) != '\0'; i = nxt+1) {
		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE) {
				return (NULL);
			}
		}
		if ((val=envmatch((char *)name, i)) < 0) {
			continue;
		}
		return ((char *)env_get_addr(val));
	}
	return (NULL);
}

char *get_env(const char *name)
{
	pr_notice("[%s]get env name=%s\n", 
		  MODULE_NAME, name);

	env_init();

	if (!env_valid) {
		return (NULL);
	}
	return (findenv(name));
}
EXPORT_SYMBOL(get_env);

static char env_get_char(int index)
{
	return *(g_env.env_data + index);
}

static char *env_get_addr(int index)
{
	return (g_env.env_data + index);

}

static int envmatch(char *s1, int i2)
{
	while (*s1 == env_get_char(i2++)) {
		if (*s1++ == '=') {
			return(i2);
		}
	}
	if (*s1 == '\0' && env_get_char(i2-1) == '=') {
		return(i2);
	}
	return (-1);
}

int set_env(char *name,char *value)
{
	int  len;
	int  oldval = -1;
	char *env, *nxt = NULL;
	int ret = 0;
	char *env_data = NULL;
	pr_notice("[%s]set env, name=%s,value=%s\n", 
		  MODULE_NAME, name, value);
	
	env_init();

	env_data = g_env.env_data;
	if (!env_buffer) {
		return (-1);
	}
	if (!env_valid) {
		env = env_data;
		goto add;
	}
/* find match name and return the val header pointer*/
	for (env = env_data; *env; env = nxt + 1) {
		for (nxt=env; *nxt; ++nxt) {
			;
		}
		if ((oldval = envmatch((char *)name, env-env_data)) >= 0)
			break;
	}/* end find */
	if (oldval>0) {
		if (*++nxt == '\0') {
			if (env > env_data) {
				env--;
			} else {
				*env = '\0';
			}
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	for (env=env_data; *env || *(env+1); ++env) {
		;
	}
	if (env > env_data) {
		++env;
	}
add:
	if (*value == '\0') {
		pr_notice("[%s]clear env name=%s\n", 
			  MODULE_NAME, name);
		goto write_env;
	}

	len = strlen(name) + 2;
	len += strlen(value) + 1;
	if (len > (&env_data[CFG_ENV_DATA_SIZE] - env)) {
		pr_err("[%s]env data overflow, %s deleted\n", 
			MODULE_NAME, name);
		return -1;
	}
	while ((*env = *name++) != '\0') {
		env++;
	}
	*env = '=';
	while ((*++env = *value++) != '\0') {
		;
	}
write_env:
/* end is marked with double '\0' */
	*++env = '\0';
	memset(env, 0x00, CFG_ENV_DATA_SIZE - (env - env_data));

	ret = write_env_area(env_buffer);
	if (ret < 0) {
		pr_err("[%s]%s error: write env area fail\n", 
			MODULE_NAME, __FUNCTION__);
		memset(env_buffer, 0x00, CFG_ENV_SIZE);
		return -1;
	}
	env_valid = 1;
	return 0;
}
EXPORT_SYMBOL(set_env);

static int write_env_area(char *env_buf)
{
	int i,checksum = 0;
	int result = 0;
	int ret = 0;
	loff_t pos = 0;
	mm_segment_t old_fs;
	struct file *write_fp;
	
	memcpy(env_buf, ENV_SIG, sizeof(g_env.sig_head));
	memcpy(env_buf + CFG_ENV_SIG_1_OFFSET, ENV_SIG, sizeof(g_env.sig_tail));

	for (i = 0; i < (CFG_ENV_DATA_SIZE); i++) {
		checksum += *(env_buf + CFG_ENV_DATA_OFFSET + i);
	}
	*((int *)env_buf + CFG_ENV_CHECKSUM_OFFSET / 4) = checksum;

	write_fp = filp_open(PARA_PATH, O_RDWR, 0);
	if (IS_ERR(write_fp)) {
			result = PTR_ERR(write_fp);
			pr_err("[%s]File open return fail,result=%d file=%p\n", 
				MODULE_NAME, result, write_fp);
			goto filp_open_fail;
	}

	pos += CFG_ENV_OFFSET;
	ret = kernel_write(write_fp, (char *)env_buf, CFG_ENV_SIZE, pos);
	if (ret < 0) {
		pr_err("[%s]Kernel write env fail\n", MODULE_NAME);
		result = -1;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	ret = vfs_fsync(write_fp, 0);
	if (ret < 0) {
	    pr_warn("[%s]Kernel write env sync fail\n", MODULE_NAME);
	}
	set_fs(old_fs);

	filp_close(write_fp, 0);
filp_open_fail:
	return (result);
}

static int read_env_area(char *env_buf)
{
	int result = 0;	
	int ret = 0;
	loff_t pos = 0;
	struct file *read_fp;
	
	read_fp = filp_open(PARA_PATH, O_RDWR, 0);
	if(IS_ERR(read_fp)) {
		result = PTR_ERR(read_fp);
		pr_err("[%s]File open return fail,result=%d,file=%p\n", 
			MODULE_NAME, result, read_fp);
		goto filp_open_fail;
	}
	
	pos += CFG_ENV_OFFSET;
	ret = kernel_read(read_fp, pos, (char *)env_buf, CFG_ENV_SIZE);
	if (ret < 0) {
		pr_err("[%s]Kernel read env fail\n", MODULE_NAME);
		result = -1;
	}
	filp_close(read_fp, 0);
filp_open_fail:
	return (result);
}

static int __init sysenv_init(void)
{
    struct proc_dir_entry *sysenv_proc;

    sysenv_proc = proc_create("lk_env", 0600, NULL, &env_proc_fops);
    if (!sysenv_proc) {
		pr_err("[%s]fail to create /proc/lk_env\n", MODULE_NAME);
    }   
    return 0;
}

static void __exit sysenv_exit(void)
{
    remove_proc_entry("lk_env", NULL);
}

module_init(sysenv_init);
module_exit(sysenv_exit);

/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include "hqsys_misc.h"


MISC_INFO(MISC_EMMC_SIZE,emmc_size);
MISC_INFO(MISC_RAM_SIZE,ram_size);
MISC_INFO(MISC_BOOT_MODE,boot_mode);
MISC_INFO(MISC_OTP_SN,otp_sn);

extern unsigned int msdc_get_capacity(int get_emmc_total);
extern char *get_emmc_name(void);

unsigned int round_kbytes_to_readable_mbytes(unsigned int k){
	unsigned int r_size_m = 0;
	unsigned int in_mega = k/1024;

	if (in_mega > 64*1024){
		r_size_m = 128*1024;
	}else if (in_mega > 32*1024){
		r_size_m = 64*1024;
	}else if (in_mega > 16*1024){
		r_size_m = 32*1024;
	}else if (in_mega > 8*1024){
		r_size_m = 16*1024;
	}else if (in_mega > 6*1024){
		r_size_m = 8*1024;
	}else if (in_mega > 4*1024){
		r_size_m = 6*1024;
	}else if (in_mega > 3*1024){
		r_size_m = 4*1024;
	}else if (in_mega > 2*1024){
		r_size_m = 3*1024;
	}else if (in_mega > 1024){
		r_size_m = 2*1024;
	}else if (in_mega > 512){
		r_size_m = 1024;
	}else if (in_mega > 256){
		r_size_m = 512;
	}else if (in_mega > 128){
		r_size_m = 256;
	}else{
		k = 0;
	}
	return r_size_m;
}

ssize_t hq_emmcinfo(char *buf)
{
	ssize_t count = -1;
	struct file *pfile = NULL;
	mm_segment_t old_fs;
	loff_t pos;
        ssize_t ret = 0;

	unsigned long long Size_buf = 0;
	char buf_size[qcom_emmc_len];
	memset(buf_size,0,sizeof(buf_size));

	pfile = filp_open(qcom_emmc,O_RDONLY,0);
	if (IS_ERR(pfile)) {
	    goto ERR_0;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	ret = vfs_read(pfile, buf_size, qcom_emmc_len, &pos);
	if (ret <=0)
	{
		goto ERR_1;
	}

	Size_buf = simple_strtoull(buf_size,NULL,0);

	Size_buf>>=1;


	count = sprintf(buf,"%dGB",round_kbytes_to_readable_mbytes((unsigned int)Size_buf)/1024);

ERR_1:

	filp_close(pfile, NULL);

	set_fs(old_fs);

	return count;

ERR_0:
	return count;

}

static struct attribute *hq_misc_attrs[] = {
	&misc_info_emmc_size.attr,
	&misc_info_ram_size.attr,
	&misc_info_boot_mode.attr,
	&misc_info_otp_sn.attr,
	NULL
};

extern int hq_read_sn_from_otp(char *sn);
extern int hq_write_sn_to_otp(char *sn,unsigned int len);
#define SN_LEN (12)

static ssize_t hq_misc_show(struct kobject *kobj, struct attribute *a, char *buf){
	ssize_t count = 0;

	struct misc_info *mi = container_of(a, struct misc_info , attr);

	switch(mi->m_id){
		case MISC_RAM_SIZE:
			{
				#define K(x) ((x) << (PAGE_SHIFT - 10))
				struct sysinfo i;
				si_meminfo(&i);


				if (round_kbytes_to_readable_mbytes(K(i.totalram)) >= 1024){
					count = sprintf(buf,"%dGB",round_kbytes_to_readable_mbytes(K(i.totalram))/1024);
				}else{
					count = sprintf(buf,"%dMB",round_kbytes_to_readable_mbytes(K(i.totalram)));
				}

			}
			break;
		case MISC_EMMC_SIZE:

			        count = hq_emmcinfo(buf);
			break;
		case MISC_OTP_SN:
#ifdef CONFIG_MTK_EMMC_SUPPORT_OTP
            {
                char temp[SN_LEN+1] = {0};
                int result = 0;
                int i = 0;

                result = hq_read_sn_from_otp(temp);

                if (0 == result){
                    #if 0

                    for (i = 0; i < SN_LEN; i++){
                        if (!isalnum(temp[i])){
                            count = sprintf(buf,"Not Valid SN\n");
                            goto r_error;
                        }
                    }
                    #endif
                    count = sprintf(buf,"%s",temp);
                }else{
                    count = sprintf(buf,"Read SN in OTP error %d\n",result);
                }
            }

#else
            count = sprintf(buf,"SN in OTP not enabled\n");
#endif
		    break;
		default:
			count = sprintf(buf,"Not support");
			break;
	}


	return count;
}

static ssize_t hq_misc_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count){

	struct misc_info *mi = container_of(a, struct misc_info , attr);

	switch(mi->m_id){
#ifdef CONFIG_MTK_EMMC_SUPPORT_OTP
		case MISC_OTP_SN:
		    {
		        char temp[SN_LEN+1] = {0};
		        int result = 0;
		        int i = 0;

		        if (0 != strncmp(buf,"SN:=",4)){
		            printk("[%s] invalid write sn command\n");
		            break;
		        }
		        for (i = 0; i < SN_LEN; i++){
		            temp[i] = buf[i+4];
		            if (('\n' == buf[i+4]) || ('\r' == buf[i+4])){
                        temp[i] = 0;
                        break;
		            }
		        }


		        result = hq_write_sn_to_otp(temp,strlen(temp));
		            if (0 != result)
		                printk("[%s] called write error %d\n",__func__,result);

		    }
		    break;
#endif
		default:
			break;
	}
	return count;
}

/* hq_misc object */
static struct kobject hq_misc_kobj;
static const struct sysfs_ops hq_misc_sysfs_ops = {
	.show = hq_misc_show,
	.store = hq_misc_store,
};

/* hq_misc type */
static struct kobj_type hq_misc_ktype = {
	.sysfs_ops = &hq_misc_sysfs_ops,
	.default_attrs = hq_misc_attrs
};


static int __init create_misc(void){
	int ret;

	/* add kobject */
	ret = register_kboj_under_hqsysfs(&hq_misc_kobj, &hq_misc_ktype, HUAQIN_MISC_NAME);
	if (ret < 0) {
		pr_err("%s fail to add hq_misc_kobj\n",__func__);
		return ret;
	}
	return 0;
}



static int __init hq_misc_sys_init(void)
{
	/* create sysfs entry at /sys/class/hq_misc/interface/misc */
	create_misc();

	return 0;
}


late_initcall(hq_misc_sys_init);
MODULE_AUTHOR("KaKa Ni <nigang@hq_misc.com>");
MODULE_DESCRIPTION("Huaqin Hardware Info Driver");
MODULE_LICENSE("GPL");


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>

#define AUDIO_INFO_NAME_MAX     (64)

struct lct_audio_info {
    char codec_name[AUDIO_INFO_NAME_MAX];
    char pa_name[AUDIO_INFO_NAME_MAX];
};

static DEFINE_MUTEX(g_audio_info_mutex_lock);
static struct lct_audio_info g_audio_info;

static ssize_t audio_codec_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(0 == strlen(g_audio_info.codec_name))
       sprintf(buf,"%s\n", "MT6357");
    else
       sprintf(buf,"%s\n", g_audio_info.codec_name);
    ret = strlen(buf) + 1;
    return ret;
}

static ssize_t audio_pa_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(0 == strlen(g_audio_info.pa_name))
       sprintf(buf,"%s\n", "AW87389");
    else
       sprintf(buf,"%s\n", g_audio_info.pa_name);
    ret = strlen(buf) + 1;
    return ret;
}

static DEVICE_ATTR(audio_codec, 0664, audio_codec_show, NULL);
static DEVICE_ATTR(audio_pa, 0664, audio_pa_show, NULL);

static struct kobject *android_audio;

int lct_audio_info_create_sysfs(void)
{
   int ret;

   android_audio = kobject_create_and_add("android_audio", NULL);
   if(android_audio == NULL) {
       pr_err(" android_audio_create_sysfs_ failed\n");
       ret=-ENOMEM;
       return ret;
   }

   //struct lct_audio_info audio_info;
   //g_audio_info = &audio_info;

   ret=sysfs_create_file(android_audio, &dev_attr_audio_codec.attr);
   if(ret) {
       pr_err("%s audio_codec failed \n", __func__);
       kobject_del(android_audio);
   }

   ret=sysfs_create_file(android_audio, &dev_attr_audio_pa.attr);
   if(ret) {
       pr_err("%s audio_pa failed \n", __func__);
       kobject_del(android_audio);
   }

   return 0;
}
EXPORT_SYMBOL(lct_audio_info_create_sysfs);

int lct_audio_info_set_codec_name(char *codec_name, int count)
{
    int max_count = count>AUDIO_INFO_NAME_MAX?AUDIO_INFO_NAME_MAX:count;
    mutex_lock(&g_audio_info_mutex_lock);
    strncpy(g_audio_info.codec_name, codec_name, max_count);
    mutex_unlock(&g_audio_info_mutex_lock);
    return strlen(g_audio_info.codec_name);
}
EXPORT_SYMBOL(lct_audio_info_set_codec_name);

int lct_audio_info_set_pa_name(char *pa_name, int count)
{
    int max_count = count>AUDIO_INFO_NAME_MAX?AUDIO_INFO_NAME_MAX:count;
    mutex_lock(&g_audio_info_mutex_lock);
    strncpy(g_audio_info.pa_name, pa_name, max_count);
    mutex_unlock(&g_audio_info_mutex_lock);
    return strlen(g_audio_info.pa_name);
}
EXPORT_SYMBOL(lct_audio_info_set_pa_name);



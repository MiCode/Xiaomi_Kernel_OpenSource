#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/namei.h>
#include <asm/segment.h>
#include <asm/uaccess.h>

struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

void file_close(struct file* file) {
    filp_close(file, NULL);
}

int file_read(struct file* file, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &file->f_pos);

    set_fs(oldfs);
    return ret;
}

int file_write(struct file* file, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &file->f_pos);

    set_fs(oldfs);
    return ret;
}

int remove_file(char *path)
{
    mm_segment_t oldfs;
    int ret;
    struct path ndpath;
    struct dentry *dentry;

    oldfs = get_fs();
    set_fs(get_ds());

    //ret = kern_path_parent(path, &nd);
    ret = kern_path(path,LOOKUP_PARENT,&ndpath);
    if (ret != 0) {
        return -ENOENT;
    }
    dentry = lookup_one_len(path, ndpath.dentry, strlen(path));
    if (IS_ERR(dentry)) {
        return -EACCES;
    }
    vfs_unlink(ndpath.dentry->d_inode, dentry);
    dput(dentry);

    set_fs(oldfs);
    return ret;
}

int save_data_to_file(char *path, char *data, int size)
{
        struct file *fp = NULL;
        
        fp = file_open(path, O_WRONLY | O_CREAT, 0);
        if (fp != NULL) {
                file_write(fp, data, size);
                file_close(fp);
                return 0;
        }
        return -1;
}


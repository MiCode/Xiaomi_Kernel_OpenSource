#include "sdfat.h"

#define SDFAT_VF_CLUS_MAX	7	/* 512 Byte ~ 32 KByte */
#define SDFAT_EF_CLUS_MAX	17	/* 512 Byte ~ 32 MByte */

enum {
	SDFAT_MNT_FAT12,
	SDFAT_MNT_FAT16,
	SDFAT_MNT_FAT32,
	SDFAT_MNT_EXFAT,
	SDFAT_MNT_RO,
	SDFAT_MNT_MAX
};

enum {
	SDFAT_OP_EXFAT_MNT,
	SDFAT_OP_MKDIR,
	SDFAT_OP_CREATE,
	SDFAT_OP_READ,
	SDFAT_OP_WRITE,
	SDFAT_OP_TRUNC,
	SDFAT_OP_MAX
};

enum {
	SDFAT_VOL_4G,
	SDFAT_VOL_8G,
	SDFAT_VOL_16G,
	SDFAT_VOL_32G,
	SDFAT_VOL_64G,
	SDFAT_VOL_128G,
	SDFAT_VOL_256G,
	SDFAT_VOL_512G,
	SDFAT_VOL_XTB,
	SDFAT_VOL_MAX
};

static struct sdfat_statistics {
	u32 clus_vfat[SDFAT_VF_CLUS_MAX];
	u32 clus_exfat[SDFAT_EF_CLUS_MAX];
	u32 mnt_cnt[SDFAT_MNT_MAX];
	u32 nofat_op[SDFAT_OP_MAX];
	u32 vol_size[SDFAT_VOL_MAX];
} statistics;

static struct kset *sdfat_statistics_kset;

static ssize_t vfat_cl_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return snprintf(buff, PAGE_SIZE, "\"VCL_512B_I\":\"%u\","
			"\"VCL_1K_I\":\"%u\",\"VCL_2K_I\":\"%u\","
			"\"VCL_4K_I\":\"%u\",\"VCL_8K_I\":\"%u\","
			"\"VCL_16K_I\":\"%u\",\"VCL_32K_I\":\"%u\"\n",
			statistics.clus_vfat[0], statistics.clus_vfat[1],
			statistics.clus_vfat[2], statistics.clus_vfat[3],
			statistics.clus_vfat[4], statistics.clus_vfat[5],
			statistics.clus_vfat[6]);
}

static ssize_t exfat_cl_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return snprintf(buff, PAGE_SIZE, "\"ECL_512B_I\":\"%u\","
			"\"ECL_1K_I\":\"%u\",\"ECL_2K_I\":\"%u\","
			"\"ECL_4K_I\":\"%u\",\"ECL_8K_I\":\"%u\","
			"\"ECL_16K_I\":\"%u\",\"ECL_32K_I\":\"%u\","
			"\"ECL_64K_I\":\"%u\",\"ECL_128K_I\":\"%u\","
			"\"ECL_256K_I\":\"%u\",\"ECL_512K_I\":\"%u\","
			"\"ECL_1M_I\":\"%u\",\"ECL_2M_I\":\"%u\","
			"\"ECL_4M_I\":\"%u\",\"ECL_8M_I\":\"%u\","
			"\"ECL_16M_I\":\"%u\",\"ECL_32M_I\":\"%u\"\n",
			statistics.clus_exfat[0], statistics.clus_exfat[1],
			statistics.clus_exfat[2], statistics.clus_exfat[3],
			statistics.clus_exfat[4], statistics.clus_exfat[5],
			statistics.clus_exfat[6], statistics.clus_exfat[7],
			statistics.clus_exfat[8], statistics.clus_exfat[9],
			statistics.clus_exfat[10], statistics.clus_exfat[11],
			statistics.clus_exfat[12], statistics.clus_exfat[13],
			statistics.clus_exfat[14], statistics.clus_exfat[15],
			statistics.clus_exfat[16]);
}

static ssize_t mount_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return snprintf(buff, PAGE_SIZE, "\"FAT12_MNT_I\":\"%u\","
			"\"FAT16_MNT_I\":\"%u\",\"FAT32_MNT_I\":\"%u\","
			"\"EXFAT_MNT_I\":\"%u\",\"RO_MNT_I\":\"%u\"\n",
			statistics.mnt_cnt[SDFAT_MNT_FAT12],
			statistics.mnt_cnt[SDFAT_MNT_FAT16],
			statistics.mnt_cnt[SDFAT_MNT_FAT32],
			statistics.mnt_cnt[SDFAT_MNT_EXFAT],
			statistics.mnt_cnt[SDFAT_MNT_RO]);
}

static ssize_t nofat_op_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return snprintf(buff, PAGE_SIZE, "\"NOFAT_MOUNT_I\":\"%u\","
			"\"NOFAT_MKDIR_I\":\"%u\",\"NOFAT_CREATE_I\":\"%u\","
			"\"NOFAT_READ_I\":\"%u\",\"NOFAT_WRITE_I\":\"%u\","
			"\"NOFAT_TRUNC_I\":\"%u\"\n",
			statistics.nofat_op[SDFAT_OP_EXFAT_MNT],
			statistics.nofat_op[SDFAT_OP_MKDIR],
			statistics.nofat_op[SDFAT_OP_CREATE],
			statistics.nofat_op[SDFAT_OP_READ],
			statistics.nofat_op[SDFAT_OP_WRITE],
			statistics.nofat_op[SDFAT_OP_TRUNC]);
}

static ssize_t vol_size_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return snprintf(buff, PAGE_SIZE, "\"VOL_4G_I\":\"%u\","
			"\"VOL_8G_I\":\"%u\",\"VOL_16G_I\":\"%u\","
			"\"VOL_32G_I\":\"%u\",\"VOL_64G_I\":\"%u\","
			"\"VOL_128G_I\":\"%u\",\"VOL_256G_I\":\"%u\","
			"\"VOL_512G_I\":\"%u\",\"VOL_XTB_I\":\"%u\"\n",
			statistics.vol_size[SDFAT_VOL_4G],
			statistics.vol_size[SDFAT_VOL_8G],
			statistics.vol_size[SDFAT_VOL_16G],
			statistics.vol_size[SDFAT_VOL_32G],
			statistics.vol_size[SDFAT_VOL_64G],
			statistics.vol_size[SDFAT_VOL_128G],
			statistics.vol_size[SDFAT_VOL_256G],
			statistics.vol_size[SDFAT_VOL_512G],
			statistics.vol_size[SDFAT_VOL_XTB]);
}

static struct kobj_attribute vfat_cl_attr = __ATTR_RO(vfat_cl);
static struct kobj_attribute exfat_cl_attr = __ATTR_RO(exfat_cl);
static struct kobj_attribute mount_attr = __ATTR_RO(mount);
static struct kobj_attribute nofat_op_attr = __ATTR_RO(nofat_op);
static struct kobj_attribute vol_size_attr = __ATTR_RO(vol_size);

static struct attribute *attributes_statistics[] = {
	&vfat_cl_attr.attr,
	&exfat_cl_attr.attr,
	&mount_attr.attr,
	&nofat_op_attr.attr,
	&vol_size_attr.attr,
	NULL,
};

static struct attribute_group attr_group_statistics = {
		.attrs = attributes_statistics,
};

int sdfat_statistics_init(struct kset *sdfat_kset)
{
	int err;

	sdfat_statistics_kset = kset_create_and_add("statistics", NULL, &sdfat_kset->kobj);
	if (!sdfat_statistics_kset) {
		pr_err("[SDFAT] failed to create sdfat statistics kobj\n");
		return -ENOMEM;
	}

	err = sysfs_create_group(&sdfat_statistics_kset->kobj, &attr_group_statistics);
	if (err) {
		pr_err("[SDFAT] failed to create sdfat statistics attributes\n");
		kset_unregister(sdfat_statistics_kset);
		sdfat_statistics_kset = NULL;
		return err;
	}

	return 0;
}

void sdfat_statistics_uninit(void)
{
	if (sdfat_statistics_kset) {
		sysfs_remove_group(&sdfat_statistics_kset->kobj, &attr_group_statistics);
		kset_unregister(sdfat_statistics_kset);
		sdfat_statistics_kset = NULL;
	}
	memset(&statistics, 0, sizeof(struct sdfat_statistics));
}

void sdfat_statistics_set_mnt(FS_INFO_T *fsi)
{
	if (fsi->vol_type == EXFAT) {
		statistics.mnt_cnt[SDFAT_MNT_EXFAT]++;
		statistics.nofat_op[SDFAT_OP_EXFAT_MNT] = 1;
		if (fsi->sect_per_clus_bits < SDFAT_EF_CLUS_MAX)
			statistics.clus_exfat[fsi->sect_per_clus_bits]++;
		else
			statistics.clus_exfat[SDFAT_EF_CLUS_MAX - 1]++;
		return;
	}

	if (fsi->vol_type == FAT32)
		statistics.mnt_cnt[SDFAT_MNT_FAT32]++;
	else if (fsi->vol_type == FAT16)
		statistics.mnt_cnt[SDFAT_MNT_FAT16]++;
	else if (fsi->vol_type == FAT12)
		statistics.mnt_cnt[SDFAT_MNT_FAT12]++;

	if (fsi->sect_per_clus_bits < SDFAT_VF_CLUS_MAX)
		statistics.clus_vfat[fsi->sect_per_clus_bits]++;
	else
		statistics.clus_vfat[SDFAT_VF_CLUS_MAX - 1]++;
}

void sdfat_statistics_set_mnt_ro(void)
{
	statistics.mnt_cnt[SDFAT_MNT_RO]++;
}

void sdfat_statistics_set_mkdir(u8 flags)
{
	if (flags != 0x03)
		return;
	statistics.nofat_op[SDFAT_OP_MKDIR] = 1;
}

void sdfat_statistics_set_create(u8 flags)
{
	if (flags != 0x03)
		return;
	statistics.nofat_op[SDFAT_OP_CREATE] = 1;
}

/* flags : file or dir flgas, 0x03 means no fat-chain.
 * clu_offset : file or dir logical cluster offset
 * create : BMAP_ADD_CLUSTER or not
 *
 * File or dir have BMAP_ADD_CLUSTER is no fat-chain write
 * when they have 0x03 flag and two or more clusters.
 * And don`t have BMAP_ADD_CLUSTER is no fat-chain read
 * when above same condition.
 */
void sdfat_statistics_set_rw(u8 flags, u32 clu_offset, s32 create)
{
	if ((flags == 0x03) && (clu_offset > 1)) {
		if (create)
			statistics.nofat_op[SDFAT_OP_WRITE] = 1;
		else
			statistics.nofat_op[SDFAT_OP_READ] = 1;
	}
}

/* flags : file or dir flgas, 0x03 means no fat-chain.
 * clu : cluster chain
 *
 * Set no fat-chain trunc when file or dir have 0x03 flag
 * and two or more clusters.
 */
void sdfat_statistics_set_trunc(u8 flags, CHAIN_T *clu)
{
	if ((flags == 0x03) && (clu->size > 1))
		statistics.nofat_op[SDFAT_OP_TRUNC] = 1;
}

void sdfat_statistics_set_vol_size(struct super_block *sb)
{
	u64 vol_size;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	vol_size = (u64)fsi->num_sectors << sb->s_blocksize_bits;

	if (vol_size <= ((u64)1 << 32))
		statistics.vol_size[SDFAT_VOL_4G]++;
	else if (vol_size <= ((u64)1 << 33))
		statistics.vol_size[SDFAT_VOL_8G]++;
	else if (vol_size <= ((u64)1 << 34))
		statistics.vol_size[SDFAT_VOL_16G]++;
	else if (vol_size <= ((u64)1 << 35))
		statistics.vol_size[SDFAT_VOL_32G]++;
	else if (vol_size <= ((u64)1 << 36))
		statistics.vol_size[SDFAT_VOL_64G]++;
	else if (vol_size <= ((u64)1 << 37))
		statistics.vol_size[SDFAT_VOL_128G]++;
	else if (vol_size <= ((u64)1 << 38))
		statistics.vol_size[SDFAT_VOL_256G]++;
	else if (vol_size <= ((u64)1 << 39))
		statistics.vol_size[SDFAT_VOL_512G]++;
	else
		statistics.vol_size[SDFAT_VOL_XTB]++;
}

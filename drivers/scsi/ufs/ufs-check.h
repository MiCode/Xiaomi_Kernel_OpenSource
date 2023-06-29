#ifndef _UFS_CHECK_
#define _UFS_CHECK_

void fill_wb_gb(struct ufs_hba *hba, unsigned int segsize, unsigned char unitsize, unsigned int rawval);
void fill_hpb_gb(struct ufs_hba *hba, unsigned short lu_rgs, int rg_size);
void fill_total_gb(struct ufs_hba *hba,  unsigned long long rawval);
void check_hpb_and_tw_provsion(struct ufs_hba *hba);
int check_wb_hpb_size(struct ufs_hba *hba);
#endif

#ifndef __MV_H__
#define __MV_H__

#define MV_NAME					"mv"

struct MV_UFS
{
	u16 vendor_id;
	u64 density;
	char product_name[18];
	char product_revision[6];
};

//void updata_mv_ufs(u16 w_manufacturer_id, u8 *inquiry, u64 qTotalTawDeviceCapacity);
int add_proc_mv_node(void);
int remove_proc_mv_node(void);

#endif
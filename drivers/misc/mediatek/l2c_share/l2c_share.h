#ifndef _L2_SHARE_H
#define _L2_SHARE_H

#define L2C_SIZE_CFG_OFF 8
#define L2C_SHARE_ENABLE 12

typedef struct _l2c_share_info{
	u32 share_cluster_num; 
	u32 cluster_borrow;
	u32 cluster_return;
}l2c_share_info;

enum options{
	BORROW_L2,
	RETURN_L2,
	BORROW_NONE
};

#endif

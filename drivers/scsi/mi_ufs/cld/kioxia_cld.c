/*
 * mi_cld1.c
 *
 *  Created on: 2020-10-20
 *      Author: shane
 */

#include "mi_cld.h"
#include "../mi-ufshcd.h"
#include "../mi_ufs_common_ops.h"

#define QUERY_ATTR_IDN_CLD_ENABLE 0x13
#define QUERY_ATTR_IDN_CLD_LEVEL 0x31
#define QUERY_ATTR_IDN_DEFRAG 0x32

enum KIOXIA_DEFRAG {
	DEFRAG_IDEL = 0x0,
	DEFRAG_OPERATING = 0x01,
	DEFRAG_OPERATION_STOPPED = 0x02,
	DEFRAG_OPERATION_COMPLETED = 0x03,
	DEFRAG_OPERATION_FAILURE = 0x04,
};

int kioxia_get_frag_level(struct ufscld_dev *cld, int *frag_level)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0, attr = -1;
	ret = mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_CLD_LEVEL, 0, 0, &attr);
	if (ret)
		return ret;
	if (attr == 0) {
		*frag_level = CLD_LEV_CLEAN;
	} else if (attr == 1) {
		*frag_level = CLD_LEV_WARN;
	} else if (attr == 2 || attr == 3) {
		*frag_level= CLD_LEV_CRITICAL;
	}else {
		pr_info("kioxia cld unknown level %d\n", attr);
		ret = -1;
		return ret;
	}
	return 0;
}

int kioxia_cld_set_trigger(struct ufscld_dev *cld, u32 trigger)
{
	struct ufs_hba *hba = cld->hba;

	return mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_SET_FLAG, (enum attr_idn)QUERY_ATTR_IDN_CLD_ENABLE, 0, 0, &trigger);
}

int kioxia_cld_get_trigger(struct ufscld_dev *cld, u32 *trigger)
{
	struct ufs_hba *hba = cld->hba;

	return mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_FLAG, (enum attr_idn)QUERY_ATTR_IDN_CLD_ENABLE, 0, 0, trigger);
}

static int kioxia_cld_get_defragprogress(struct ufscld_dev *cld, int *defragprogress)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0;

	*defragprogress = 0;

	ret = mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_DEFRAG, 0, 0, defragprogress);

	return 0;
}

int kioxia_cld_operation_status(struct ufscld_dev *cld, int *op_status)
{
	int ret = 0;
	int defragprogress = 0;

	ret = kioxia_cld_get_defragprogress(cld, &defragprogress);
	if (ret)
		ERR_MSG("get defragprogress failed ret=%d\n", ret);

	if (defragprogress == DEFRAG_OPERATION_COMPLETED || defragprogress == DEFRAG_OPERATION_FAILURE) {// if cld was done
		*op_status = CLD_STATUS_IDEL;
	} else if (defragprogress == DEFRAG_OPERATING || defragprogress == DEFRAG_OPERATION_STOPPED){
		*op_status = CLD_STATUS_PROGRESSING;
	} else {
		*op_status = CLD_STATUS_NA;
	}

	return 0;
}

struct ufscld_ops kioxia_cld_ops = {
		.cld_get_frag_level = kioxia_get_frag_level,
		.cld_set_trigger = kioxia_cld_set_trigger,
		.cld_get_trigger = kioxia_cld_get_trigger,
		.cld_operation_status = kioxia_cld_operation_status
};

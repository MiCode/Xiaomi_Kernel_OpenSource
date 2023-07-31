/*
 * mi_cld1.c
 *
 *  Created on: 2020-10-20
 *      Author: shane
 */

#include "mi_cld.h"
#include "../mi-ufshcd.h"
#include "../mi_ufs_common_ops.h"

#define QUERY_ATTR_IDN_CLD_ENABLE 0x12
#define QUERY_ATTR_IDN_CLD_LEVEL 0x13

int sk_get_frag_level(struct ufscld_dev *cld, int *frag_level)
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
	} else if (attr == 2) {
		*frag_level = CLD_LEV_CRITICAL;
	} else {
		pr_info("sk cld unknown level %d\n", *frag_level);
		ret = -1;
		return ret;
	}
	return 0;
}

int sk_cld_set_trigger(struct ufscld_dev *cld, u32 trigger)
{
	struct ufs_hba *hba = cld->hba;

	return mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, (enum attr_idn)QUERY_ATTR_IDN_CLD_ENABLE, 0, 0, &trigger);
}

int sk_cld_get_trigger(struct ufscld_dev *cld, u32 *trigger)
{
	struct ufs_hba *hba = cld->hba;

	return mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_CLD_ENABLE, 0, 0, trigger);
}

int sk_cld_operation_status(struct ufscld_dev *cld, int *op_status)
{
	enum CLD_LEVEL frag_level;
	int ret;

	ret = sk_get_frag_level(cld, (int*) &frag_level);
	if (ret)
		ERR_MSG("get frag level failed ret=%d\n", ret);

	if (frag_level == CLD_LEV_CLEAN) {// if cld was done
		*op_status = CLD_STATUS_IDEL;
	} else if (frag_level == CLD_LEV_WARN || frag_level == CLD_LEV_CRITICAL){
		*op_status = CLD_STATUS_PROGRESSING;
	} else {
		*op_status = CLD_STATUS_NA;
	}

	return 0;
}

struct ufscld_ops hynix_cld_ops = {
		.cld_get_frag_level = sk_get_frag_level,
		.cld_set_trigger = sk_cld_set_trigger,
		.cld_get_trigger = sk_cld_get_trigger,
		.cld_operation_status = sk_cld_operation_status
};

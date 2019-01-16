#ifndef _SEC_SIGN_FORMAT_V3_H
#define _SEC_SIGN_FORMAT_V3_H

#include "sec_sign_header.h"
#include "sec_cfg.h"
#include "sec_signfmt_def.h"

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
int sec_signfmt_verify_file_v3(ASF_FILE fp, SEC_IMG_HEADER *img_hdr);
unsigned int sec_signfmt_get_hash_length_v3(SECURE_IMG_INFO_V3 *img_if, ASF_FILE fp, SEC_IMG_HEADER *file_img_hdr, char *ext_buf);
unsigned int sec_signfmt_get_signature_length_v3(SECURE_IMG_INFO_V3 *img_if, ASF_FILE fp, SEC_IMG_HEADER *file_img_hdr, char *ext_buf);
unsigned int sec_signfmt_get_extension_length_v3(SECURE_IMG_INFO_V3 *img_if, ASF_FILE fp, SEC_IMG_HEADER *file_img_hdr);
int sec_signfmt_calculate_image_hash_v3(char* part_name, SECURE_IMG_INFO_V3 *img_if, char *final_hash_buf, unsigned int hash_len, char *ext_buf);


#endif


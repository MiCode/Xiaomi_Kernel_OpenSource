#ifndef _SEC_SIGN_FORMAT_V4_H
#define _SEC_SIGN_FORMAT_V4_H

#include "sec_sign_header.h"
#include "sec_cfg.h"
#include "sec_signfmt_def.h"

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
int sec_signfmt_verify_file_v4(ASF_FILE fp, SEC_IMG_HEADER *file_img_hdr_p);
unsigned int sec_signfmt_get_hash_length_v4(SECURE_IMG_INFO_V3 *img_if, ASF_FILE fp, SEC_IMG_HEADER *file_img_hdr_p, char *ext_buf);
unsigned int sec_signfmt_get_signature_length_v4(SECURE_IMG_INFO_V3 *img_if, ASF_FILE fp, SEC_IMG_HEADER *file_img_hdr_p, char *ext_buf);
unsigned int sec_signfmt_get_extension_length_v4(SECURE_IMG_INFO_V3 *img_if, ASF_FILE fp, SEC_IMG_HEADER *file_img_hdr_p);
int sec_signfmt_calculate_image_hash_v4(char* part_name, SECURE_IMG_INFO_V3 *img_if, char *final_hash_buf, unsigned int hash_len, char *ext_buf);


#endif


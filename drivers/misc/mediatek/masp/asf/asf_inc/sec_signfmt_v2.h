#ifndef _SEC_SIGN_FORMAT_V2_H
#define _SEC_SIGN_FORMAT_V2_H

#include "sec_sign_header.h"
#include "sec_signfmt_def.h"


/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
int sec_signfmt_verify_file_v2(ASF_FILE fp, SEC_IMG_HEADER *img_hdr);
unsigned int sec_signfmt_get_extension_length_v2(ASF_FILE fp);
int sec_signfmt_calculate_image_hash_v2(char* part_name, SEC_IMG_HEADER *img_hdr, unsigned int image_type, char *hash_buf, unsigned int hash_len);
unsigned int sec_signfmt_get_hash_length_v2(void);
unsigned int sec_signfmt_get_signature_length_v2(void);

#endif


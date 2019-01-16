#ifndef _SEC_SIGN_FORMAT_CORE_H
#define _SEC_SIGN_FORMAT_CORE_H

#include "sec_signfmt_def.h"

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
int sec_signfmt_get_hash_length(SEC_IMG_U *img_if, ASF_FILE fp, char *ext_buf);
int sec_signfmt_get_signature_length(SEC_IMG_U *img_if, ASF_FILE fp, char *ext_buf);
int sec_signfmt_get_extension_length(SEC_IMG_U *img_if, ASF_FILE fp);
int sec_signfmt_verify_file(char *file_path, SEC_IMG_HEADER *img_hdr, unsigned int *data_offset, unsigned int *data_sec_len);
int sec_signfmt_calculate_filelist_hash(char* part_name, SEC_IMG_U *img_if, char *file_path, 
    char *hash_buf, unsigned int hash_len, char *ext_buf); 
int sec_signfmt_calculate_image_hash(char* part_name, SEC_IMG_U *img_if, char *hash_ptr, unsigned int hash_len, char *ext_buf);

#endif


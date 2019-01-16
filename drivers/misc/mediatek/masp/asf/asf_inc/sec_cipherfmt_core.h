#ifndef _SEC_CIPHER_FORMAT_CORE_H
#define _SEC_CIPHER_FORMAT_CORE_H

#include "sec_signfmt_def.h"

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
int sec_cipherfmt_check_cipher(ASF_FILE fp, unsigned int start_off, unsigned int *img_len);
int sec_cipherfmt_decrypted(ASF_FILE fp, unsigned int start_off, char *buf, unsigned int buf_len, unsigned int *data_offset);


#endif


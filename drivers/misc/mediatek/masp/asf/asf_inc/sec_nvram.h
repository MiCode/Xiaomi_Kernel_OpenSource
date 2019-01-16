#ifndef SEC_META_H
#define SEC_META_H

/* used for META library */
#define NVRAM_CIPHER_LEN (16)

/******************************************************************************
 *  MODEM CONTEXT FOR BOTH USER SPACE PROGRAM AND KERNEL MODULE
 ******************************************************************************/
typedef struct
{
    unsigned char data[NVRAM_CIPHER_LEN];
    unsigned int ret;
    
} META_CONTEXT;

/******************************************************************************
 *  EXPORT FUNCTIONS
 ******************************************************************************/
extern int sec_nvram_enc (META_CONTEXT *meta_ctx);
extern int sec_nvram_dec (META_CONTEXT *meta_ctx);

#endif /* SEC_META_H*/

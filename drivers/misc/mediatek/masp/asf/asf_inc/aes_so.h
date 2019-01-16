#ifndef H_AES_BROM
#define H_AES_BROM

#define AES_ENCRYPT     1
#define AES_DECRYPT     0

typedef struct
{
    int nr;                    
    unsigned long *rk;         
    unsigned long buf[68];    
} a_ctx;

/**************************************************************************
 *  EXPORT FUNCTION
 **************************************************************************/
extern int aes_so_enc(unsigned char* in_buf,  unsigned int in_len, unsigned char* out_buf, unsigned int out_len);
extern int aes_so_dec(unsigned char* in_buf,  unsigned int in_len, unsigned char* out_buf, unsigned int out_len);
extern int aes_so_init_key(unsigned char* key_buf,  unsigned int key_len);
extern int aes_so_init_vector(void);

#endif

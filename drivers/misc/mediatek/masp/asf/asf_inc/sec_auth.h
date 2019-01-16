#ifndef SEC_AUTH_H
#define SEC_AUTH_H

/**************************************************************************
 * AUTH DATA STRUCTURE
**************************************************************************/
#define SIGNATURE_SIZE                      (128)   // 128 bytes
#define RSA_KEY_SIZE                        (128)

typedef struct 
{
    unsigned char                           content[SIGNATURE_SIZE];
    
} _signature;

/**************************************************************************
*  EXPORT FUNCTION
**************************************************************************/
extern int lib_init_key (unsigned char *nKey, unsigned int nKey_len, unsigned char *eKey, unsigned int eKey_len);
extern int lib_verify (unsigned char* data_buf,  unsigned int data_len, unsigned char* sig_buf, unsigned int sig_len);
extern int sec_auth_test (void);

#endif /* SEC_AUTH_H */                   


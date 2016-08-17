#if !defined(AES_EAX_H__INCLUDED__)
#define AES_EAX_H__INCLUDED__

#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CTR
#include "mbedtls/aes.h"

int compute_cmac_( mbedtls_aes_context *ctx,
		          const unsigned char *input,
		          size_t length,
		          unsigned char param,
		          unsigned char mac[16] );
		          
void gf128_double_( unsigned char val[16] );   

int eddy_aes_authcrypt_eax( mbedtls_aes_context *ctx,
                            int mode,                       /* ENCRYPT/DECRYPT */
                            const unsigned char *nonce,     /* 48-bit nonce */ 
                            size_t nonce_length,            /* = 6 */
                            const unsigned char *header,    /* Empty buffer */
                            size_t header_length,           /* = 0 */
                            size_t message_length,          /* Length of input & output buffers 12 */
                            const unsigned char *input,
                            unsigned char *output,
                            unsigned char *tag,
                            size_t tag_length );            /* = 2 */
                            
                        

#endif /* defined(AES_EAX_H__INCLUDED__) */
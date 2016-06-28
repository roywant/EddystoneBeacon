/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "EIDFrame.h"
#include "EddystoneService.h"

EIDFrame::EIDFrame(void)
{
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ecdh_init(&ecdh_ctx);
}


void EIDFrame::setData(uint8_t *rawFrame, int8_t advTxPower, const uint8_t* eidData)
{
    size_t index = 0;
    rawFrame[index++] = EID_LENGTH + 4;                         // EID length + overhead of four bytes below
    rawFrame[index++] = EDDYSTONE_UUID[0];                      // 16-bit Eddystone UUID
    rawFrame[index++] = EDDYSTONE_UUID[1];
    rawFrame[index++] = FRAME_TYPE_EID;                         // 1B  Type
    rawFrame[index++] = advTxPower;                             // 1B  Power @ 0meter

    memcpy(rawFrame + index, eidData, EID_LENGTH);              // EID = 8 BYTE ID
}

uint8_t* EIDFrame::getData(uint8_t* rawFrame)
{
        return &(rawFrame[3]);
}

uint8_t  EIDFrame::getDataLength(uint8_t* rawFrame)
{
     return rawFrame[0] - 2;
}

uint8_t* EIDFrame::getAdvFrame(uint8_t* rawFrame)
{
    return &(rawFrame[1]);
}

uint8_t EIDFrame::getAdvFrameLength(uint8_t* rawFrame)
{
    return rawFrame[0];
}

uint8_t* EIDFrame::getEid(uint8_t* rawFrame)
{
    return &(rawFrame[5]);
}

uint8_t EIDFrame::getEidLength(uint8_t* rawFrame)
{
    return rawFrame[0] - 4;
}

void EIDFrame::setAdvTxPower(uint8_t* rawFrame, int8_t advTxPower)
{
    rawFrame[4] = advTxPower;
}

// Mote: This is only called after the rotation period is due, or on writing/creating a new eidIdentityKey
void EIDFrame::update(uint8_t* rawFrame, uint8_t* eidIdentityKey, uint8_t rotationPeriodExp,  uint32_t timeSecs)
{  

    // Calculate the temporary key datastructure 1
    uint8_t ts[4]; // big endian representation of time
    ts[3] = timeSecs & 0xff;
    ts[2] = (timeSecs & 0xffff) >> 8;
    ts[1] = (timeSecs & 0xffffff) >> 16;
    ts[0] = timeSecs  >> 24;
    uint8_t tmpEidDS1[16] = { 0,0,0,0,0,0,0,0,0,0, SALT, 0, ts[0], ts[1] };
    
    // Perform the aes encryption to generate the final temporary key.
    uint8_t tmpKey[16];
    aes128Encrypt(eidIdentityKey, tmpEidDS1, tmpKey);
    
    // Compute the EID 
    uint8_t eid[16];
    uint8_t tmpEidDS2[16] = { 0,0,0,0,0,0,0,0,0,0, rotationPeriodExp, ts[0], ts[1], ts[2], ts[3] };
    aes128Encrypt(tmpKey, tmpEidDS2, eid);
    
    // copy the leading 8 bytes of the eid result (full result length = 16) into the ADV frame
    memcpy(rawFrame + 5, eid, EID_LENGTH); 
    
}

/** AES128 encrypts a 16-byte input array with a key, resulting in a 16-byte output array */
void EIDFrame::aes128Encrypt(uint8_t key[], uint8_t input[], uint8_t output[]) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx); 
    mbedtls_aes_setkey_enc(&ctx, key, 8 * sizeof(Lock_t));
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, input, output);
    mbedtls_aes_free(&ctx);
}

int EIDFrame::genBeaconKeys(PrivateEcdhKey_t beaconPrivateEcdhKey, PublicEcdhKey_t beaconPublicEcdhKey) {
//  DEBUG TO find memory overrun
    // mbedtls_entropy_context entropy;
    // mbedtls_entropy_init(&entropy);
    // mbedtls_ctr_drbg_context ctr_drbg;
    // mbedtls_ctr_drbg_init(&ctr_drbg);
    
    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0) != 0) {
        return EID_RND_FAIL;
    }
    

    // mbedtls_ecdh_context ecdh_ctx;

    // mbedtls_ecdh_init(&ecdh_ctx);
    
    if (mbedtls_ecp_group_load(&ecdh_ctx.grp, MBEDTLS_ECP_DP_CURVE25519) != 0) {
        return EID_GRP_FAIL;
    }
    if (mbedtls_ecdh_gen_public(&ecdh_ctx.grp, &ecdh_ctx.d, &ecdh_ctx.Q, mbedtls_ctr_drbg_random, &ctr_drbg) != 0) {
        return EID_GENKEY_FAIL;
    }
    
    mbedtls_mpi_write_binary(&ecdh_ctx.d, beaconPrivateEcdhKey, sizeof(PrivateEcdhKey_t));
    mbedtls_mpi_write_binary(&ecdh_ctx.Q.X, beaconPublicEcdhKey, sizeof(PublicEcdhKey_t));

    // mbedtls_entropy_free(&entropy);
    //
    return EID_SUCCESS;
}

int EIDFrame::genEcdhSharedKey(PrivateEcdhKey_t beaconPrivateEcdhKey, PublicEcdhKey_t beaconPublicEcdhKey, PublicEcdhKey_t serverPublicEcdhKey, EidIdentityKey_t eidIdentityKey) {
  
  //Compute the ECDH shared secret by multiplying the beacon's private key and the resolver's public key.
  // Reference: mbedtls/programs/pkey/ecdh_curve25519.c

  
  // Declare context
  // mbedtls_ecdh_context ecdh_ctx;  //RDB
  // initialize context
  // mbedtls_ecdh_init( &ecdh_ctx );
  mbedtls_ecp_group_load( &ecdh_ctx.grp, MBEDTLS_ECP_DP_CURVE25519 );

  // copy binary beacon private key (previously generated!) into context
  mbedtls_mpi_read_binary( &ecdh_ctx.d, beaconPrivateEcdhKey, sizeof(PrivateEcdhKey_t) );

  // copy server-public-key (received through GATT characteristic 10) into context
  mbedtls_mpi_lset( &ecdh_ctx.Qp.Z, 1 );
  mbedtls_mpi_read_binary( &ecdh_ctx.Qp.X, serverPublicEcdhKey , sizeof(PublicEcdhKey_t) ); 

  // ECDH point multiplication
  size_t olen; // actual size of shared secret 
  uint8_t sharedSecret[32]; // shared ECDH secret
  int ret = mbedtls_ecdh_calc_secret( &ecdh_ctx, &olen, sharedSecret, sizeof(sharedSecret), NULL, NULL );
  if (olen != sizeof(sharedSecret)) {
      return EID_GENKEY_FAIL;
  }
  if (ret == MBEDTLS_ERR_ECP_BAD_INPUT_DATA) {
      return EID_RC_SS_IS_ZERO;
  }

  // Convert the shared secret to key material using HKDF-SHA256. HKDF is used with 
  // the salt set to a concatenation of the resolver's public key and beacon's
  // public key, with a null context. 

  // build HKDF key
  unsigned char k[ 64 ];
  memcpy( &k[0], serverPublicEcdhKey, sizeof(PublicEcdhKey_t) );
  memcpy( &k[32], beaconPublicEcdhKey, sizeof(PublicEcdhKey_t) );
  mbedtls_mpi_write_binary( &ecdh_ctx.Q.X, &k[32], 32 );

  // compute HKDF: see https://tools.ietf.org/html/rfc5869
  // mbedtls_md_context_t md_ctx;
  mbedtls_md_init( &md_ctx );
  mbedtls_md_setup( &md_ctx, mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ), 1 );
  mbedtls_md_hmac_starts( &md_ctx, k, sizeof( k ) );
  mbedtls_md_hmac_update( &md_ctx, sharedSecret, sizeof(sharedSecret) );
  unsigned char prk[ 32 ];
  mbedtls_md_hmac_finish( &md_ctx, prk );
  mbedtls_md_hmac_starts( &md_ctx, prk, sizeof( prk ) );
  const unsigned char const1[] = { 0x01 };
  mbedtls_md_hmac_update( &md_ctx, const1, sizeof( const1 ) );
  unsigned char t[ 32 ];
  mbedtls_md_hmac_finish( &md_ctx, t );

  //Truncate the key material to 16 bytes (128 bits) to convert it to an AES-128 secret key.
  memcpy( eidIdentityKey, t, sizeof(EidIdentityKey_t) );
 
  return EID_SUCCESS;
}
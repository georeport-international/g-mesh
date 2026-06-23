#ifndef PQC_KYBER_H
#define PQC_KYBER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Costanti dimensionali per ML-KEM-512
#define KYBER_PUBLICKEYBYTES       800
#define KYBER_SECRETKEYBYTES       1632
#define KYBER_CIPHERTEXTBYTES      768
#define KYBER_SSBYTES              32

// Dichiariamo formalmente le funzioni native di PQClean in C
int PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(uint8_t *pk, uint8_t *sk);
int PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

// Mappiamo i nomi storici che usa il tuo codice G-Mesh su quelle native
#define pqc_kyber512_keypair       PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair
#define pqc_kyber512_encapsulate   PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc
#define pqc_kyber512_decapsulate   PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec

#ifdef __cplusplus
}
#endif

#endif // PQC_KYBER_H

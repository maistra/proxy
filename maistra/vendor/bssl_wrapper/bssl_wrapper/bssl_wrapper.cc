#include "bssl_wrapper.h"

int EVP_MD_CTX_cleanup(EVP_MD_CTX *ctx) {
  EVP_MD_CTX_free(ctx);
  return 1;
}

EVP_MD_CTX* EVP_MD_CTX_initialize() {
  return EVP_MD_CTX_new();
}

int BIO_mem_contents(const BIO *bio, const uint8_t **out_contents,
                     size_t *out_len) {
  size_t length = BIO_get_mem_data((BIO *)bio, out_contents);
  *out_len = length;
  return 1;
}

void bssl::bio_free(BIO *a){
  BIO_free(a);
}

void bssl::x509_free(X509 *a){
  X509_free(a);
}

void bssl::x509_info_free(X509_INFO *a){
  X509_INFO_free(a);
}

void bssl::x509_name_free(X509_NAME *a){
  X509_NAME_free(a);
}

void bssl::ssl_free(SSL *a){
  SSL_free(a);
}

void bssl::ssl_ctx_free(SSL_CTX *a){
  SSL_CTX_free(a);
}

void bssl::general_name_free(GENERAL_NAME *a){
  GENERAL_NAME_free(a);
}

void bssl::evp_pkey_free(EVP_PKEY *a){
  EVP_PKEY_free(a);
}

void bssl::ec_key_free(EC_KEY *a){
  EC_KEY_free(a);
}

void bssl::rsa_free(RSA *a){
  RSA_free(a);
}

void bssl::bn_free(BIGNUM *a){
//  BN_free(a);
}

void bssl::evp_md_ctx_free(EVP_MD_CTX *a){
  EVP_MD_CTX_free(a);
}

void bssl::ecdsa_sig_free(ECDSA_SIG *a){
  ECDSA_SIG_free(a);
}


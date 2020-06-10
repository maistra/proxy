#ifndef BSSL_WRAPPER_H
#define BSSL_WRAPPER_H

#define OPENSSL_IS_BORINGSSL

#include <string>
#include <utility>
#include <iostream>

#include "openssl/evp.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"

#define sk_X509_NAME_find(a,b,c) sk_X509_NAME_find((a), (c))

// SSL_TICKET_KEY_NAME_LEN is the length of the key name prefix of a session
// ticket.
#define SSL_TICKET_KEY_NAME_LEN 16

int EVP_MD_CTX_cleanup(EVP_MD_CTX *ctx);
EVP_MD_CTX* EVP_MD_CTX_initialize();

extern "C++" {

#include <memory>
#include <type_traits>

namespace bssl {

template <typename T, typename CleanupRet, T* (*init)(),
          CleanupRet (*cleanup)(T *)>
class StackAllocated {
 public:
  StackAllocated() { ctx_ = init(); }
  ~StackAllocated() { cleanup(ctx_); }

  StackAllocated(const StackAllocated<T, CleanupRet, init, cleanup> &) = delete;
  T& operator=(const StackAllocated<T, CleanupRet, init, cleanup> &) = delete;

  T *get() { return ctx_; }
  const T *get() const { return ctx_; }

  T *operator->() { return ctx_; }
  const T *operator->() const { return ctx_; }

  void Reset() {
    cleanup(ctx_);
    ctx_ = init();
  }

 private:
  T* ctx_;
};

using ScopedEVP_MD_CTX = StackAllocated<EVP_MD_CTX, int, EVP_MD_CTX_initialize, EVP_MD_CTX_cleanup>;

namespace internal {

// The Enable parameter is ignored and only exists so specializations can use
// SFINAE.
template <typename T, typename Enable = void>
struct DeleterImpl {};

template <typename T>
struct Deleter {
  void operator()(T *ptr) {
    // Rather than specialize Deleter for each type, we specialize
    // DeleterImpl. This allows bssl::UniquePtr<T> to be used while only
    // including base.h as long as the destructor is not emitted. This matches
    // std::unique_ptr's behavior on forward-declared types.
    //
    // DeleterImpl itself is specialized in the corresponding module's header
    // and must be included to release an object. If not included, the compiler
    // will error that DeleterImpl<T> does not have a method Free.
    DeleterImpl<T>::Free(ptr);
  }
};

template <typename T>
struct StackTraits {};

#define BORINGSSL_DEFINE_STACK_TRAITS(name, type, is_const) \
  extern "C++" {                                            \
  namespace bssl {                                          \
  namespace internal {                                      \
  template <>                                               \
  struct StackTraits<STACK_OF(name)> {                      \
    static constexpr bool kIsStack = true;                  \
    using Type = type;                                      \
    static constexpr bool kIsConst = is_const;              \
  };                                                        \
  }                                                         \
  }                                                         \
  }

// Stacks defined with |DEFINE_CONST_STACK_OF| are freed with |sk_free|.
template <typename Stack>
struct DeleterImpl<
    Stack, typename std::enable_if<StackTraits<Stack>::kIsConst>::type> {
  static void Free(Stack *sk) { sk_free(reinterpret_cast<_STACK *>(sk)); }
};

// Stacks defined with |DEFINE_STACK_OF| are freed with |sk_pop_free| and the
// corresponding type's deleter.
template <typename Stack>
struct DeleterImpl<
    Stack, typename std::enable_if<!StackTraits<Stack>::kIsConst>::type> {
  static void Free(Stack *sk) {
    sk_pop_free(
        reinterpret_cast<_STACK *>(sk),
        reinterpret_cast<void (*)(void *)>(
            DeleterImpl<typename StackTraits<Stack>::Type>::Free));
  }
};

template <typename Stack>
class StackIteratorImpl {
 public:
  using Type = typename StackTraits<Stack>::Type;
  // Iterators must be default-constructable.
  StackIteratorImpl() : sk_(nullptr), idx_(0) {}
  StackIteratorImpl(const Stack *sk, size_t idx) : sk_(sk), idx_(idx) {}

  bool operator==(StackIteratorImpl other) const {
    return sk_ == other.sk_ && idx_ == other.idx_;
  }
  bool operator!=(StackIteratorImpl other) const {
    return !(*this == other);
  }

  Type *operator*() const {
    return reinterpret_cast<Type *>(
        sk_value(reinterpret_cast<const _STACK *>(sk_), idx_));
  }

  StackIteratorImpl &operator++(/* prefix */) {
    idx_++;
    return *this;
  }

  StackIteratorImpl operator++(int /* postfix */) {
    StackIteratorImpl copy(*this);
    ++(*this);
    return copy;
  }

 private:
  const Stack *sk_;
  size_t idx_;
};

template <typename Stack>
using StackIterator = typename std::enable_if<StackTraits<Stack>::kIsStack,
                                              StackIteratorImpl<Stack>>::type;

}  // namespace internal

#define BORINGSSL_MAKE_DELETER(type, deleter)     \
  namespace internal {                            \
  template <>                                     \
  struct DeleterImpl<type> {                      \
    static void Free(type *ptr) { deleter(ptr); } \
  };                                              \
  }

void bio_free(BIO *a);
void x509_free(X509 *a);
void x509_info_free(X509_INFO *a);
void x509_name_free(X509_NAME *a);
void ssl_free(SSL *a);
void ssl_ctx_free(SSL_CTX *a);
void general_name_free(GENERAL_NAME *a);
void evp_pkey_free(EVP_PKEY *a);
void ec_key_free(EC_KEY *a);
void rsa_free(RSA *a);
void bn_free(BIGNUM *a);
void evp_md_ctx_free(EVP_MD_CTX *a);
void ecdsa_sig_free(ECDSA_SIG *a);

// Holds ownership of heap-allocated BoringSSL structures. Sample usage:
// //   bssl::UniquePtr<RSA> rsa(RSA_new());
// //   bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
template <typename T>
using UniquePtr = std::unique_ptr<T, internal::Deleter<T>>;

BORINGSSL_MAKE_DELETER(ASN1_OBJECT, ASN1_OBJECT_free)
BORINGSSL_MAKE_DELETER(ASN1_STRING, ASN1_STRING_free)
BORINGSSL_MAKE_DELETER(ASN1_TYPE, ASN1_TYPE_free)
BORINGSSL_MAKE_DELETER(BIO, bio_free)
BORINGSSL_MAKE_DELETER(BIGNUM, bn_free)
BORINGSSL_MAKE_DELETER(BN_CTX, BN_CTX_free)
BORINGSSL_MAKE_DELETER(BN_MONT_CTX, BN_MONT_CTX_free)
BORINGSSL_MAKE_DELETER(BUF_MEM, BUF_MEM_free)
BORINGSSL_MAKE_DELETER(EVP_CIPHER_CTX, EVP_CIPHER_CTX_free)
BORINGSSL_MAKE_DELETER(CONF, NCONF_free)
BORINGSSL_MAKE_DELETER(DH, DH_free)
BORINGSSL_MAKE_DELETER(EVP_MD_CTX, evp_md_ctx_free)
BORINGSSL_MAKE_DELETER(DSA, DSA_free)
BORINGSSL_MAKE_DELETER(DSA_SIG, DSA_SIG_free)
BORINGSSL_MAKE_DELETER(EC_POINT, EC_POINT_free)
BORINGSSL_MAKE_DELETER(EC_GROUP, EC_GROUP_free)
BORINGSSL_MAKE_DELETER(EC_KEY, ec_key_free)
BORINGSSL_MAKE_DELETER(ECDSA_SIG, ecdsa_sig_free)
BORINGSSL_MAKE_DELETER(EVP_PKEY, evp_pkey_free)
BORINGSSL_MAKE_DELETER(EVP_PKEY_CTX, EVP_PKEY_CTX_free)
//BORINGSSL_MAKE_DELETER(HMAC_CTX, HMAC_CTX_free)
BORINGSSL_MAKE_DELETER(char, OPENSSL_free)
BORINGSSL_MAKE_DELETER(uint8_t, OPENSSL_free)
//BORINGSSL_MAKE_DELETER(PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free)
BORINGSSL_MAKE_DELETER(RSA, rsa_free)
BORINGSSL_MAKE_DELETER(SSL, ssl_free)
BORINGSSL_MAKE_DELETER(SSL_CTX, ssl_ctx_free)
BORINGSSL_MAKE_DELETER(SSL_SESSION, SSL_SESSION_free)
//BORINGSSL_MAKE_DELETER(NETSCAPE_SPKI, NETSCAPE_SPKI_free)
BORINGSSL_MAKE_DELETER(X509, x509_free)
BORINGSSL_MAKE_DELETER(X509_ALGOR, X509_ALGOR_free)
BORINGSSL_MAKE_DELETER(X509_CRL, X509_CRL_free)
BORINGSSL_MAKE_DELETER(X509_CRL_METHOD, X509_CRL_METHOD_free)
BORINGSSL_MAKE_DELETER(X509_EXTENSION, X509_EXTENSION_free)
BORINGSSL_MAKE_DELETER(X509_INFO, x509_info_free)
BORINGSSL_MAKE_DELETER(X509_LOOKUP, X509_LOOKUP_free)
BORINGSSL_MAKE_DELETER(X509_NAME, x509_name_free)
BORINGSSL_MAKE_DELETER(X509_NAME_ENTRY, X509_NAME_ENTRY_free)
BORINGSSL_MAKE_DELETER(X509_PKEY, X509_PKEY_free)
BORINGSSL_MAKE_DELETER(X509_POLICY_TREE, X509_policy_tree_free)
//BORINGSSL_MAKE_DELETER(X509_PUBKEY, X509_PUBKEY_free)
//BORINGSSL_MAKE_DELETER(X509_REQ, X509_REQ_free)
BORINGSSL_MAKE_DELETER(X509_REVOKED, X509_REVOKED_free)
//BORINGSSL_MAKE_DELETER(X509_SIG, X509_SIG_free)
BORINGSSL_MAKE_DELETER(X509_STORE, X509_STORE_free)
BORINGSSL_MAKE_DELETER(X509_STORE_CTX, X509_STORE_CTX_free)
BORINGSSL_MAKE_DELETER(X509_VERIFY_PARAM, X509_VERIFY_PARAM_free)
//BORINGSSL_MAKE_DELETER(AUTHORITY_KEYID, AUTHORITY_KEYID_free)
//BORINGSSL_MAKE_DELETER(BASIC_CONSTRAINTS, BASIC_CONSTRAINTS_free)
BORINGSSL_MAKE_DELETER(DIST_POINT, DIST_POINT_free)
BORINGSSL_MAKE_DELETER(GENERAL_NAME, general_name_free)
//BORINGSSL_MAKE_DELETER(POLICYINFO, POLICYINFO_free)
}  // namespace bssl

// Define begin() and end() for stack types so C++ range for loops work.
template <typename Stack>
static inline bssl::internal::StackIterator<Stack> begin(const Stack *sk) {
  return bssl::internal::StackIterator<Stack>(sk, 0);
}

template <typename Stack>
static inline bssl::internal::StackIterator<Stack> end(const Stack *sk) {
  return bssl::internal::StackIterator<Stack>(
      sk, sk_num(reinterpret_cast<const _STACK *>(sk)));
}

}  // extern C++

BORINGSSL_DEFINE_STACK_TRAITS(X509_INFO, X509_INFO, false)
BORINGSSL_DEFINE_STACK_TRAITS(X509_NAME, X509_NAME, false)
BORINGSSL_DEFINE_STACK_TRAITS(GENERAL_NAME, GENERAL_NAME, false)

int BIO_mem_contents(const BIO *bio, const uint8_t **out_contents, size_t *out_len);
#endif // BSSL_WRAPPER_H

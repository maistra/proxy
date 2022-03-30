#include "source/extensions/transport_sockets/tls/io_handle_bio.h"

#include <iostream>

#include "envoy/buffer/buffer.h"
#include "envoy/network/io_handle.h"

#include "openssl/bio.h"
#include "openssl/err.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

namespace {

// NOLINTNEXTLINE(readability-identifier-naming)
inline Envoy::Network::IoHandle* bio_io_handle(BIO* bio) {
  return reinterpret_cast<Envoy::Network::IoHandle*>(BIO_get_data(bio));
}

// NOLINTNEXTLINE(readability-identifier-naming)
int io_handle_new(BIO* bio) {
  BIO_set_init(bio, 0);
  BIO_set_data(bio, nullptr);
  BIO_clear_flags(bio, INT_MAX); // clear all bits in a 64bit int

  return 1;
}

// NOLINTNEXTLINE(readability-identifier-naming)
int io_handle_free(BIO* bio) {
  if (bio == nullptr) {
    return 0;
  }

  if (BIO_get_shutdown(bio) == 1) {
    if (BIO_get_init(bio) == 1) {
      bio_io_handle(bio)->close();
    }
    BIO_set_init(bio, 0);
    BIO_clear_flags(bio, INT_MAX);
  }

  auto* meth = static_cast<BIO_METHOD*>(BIO_get_app_data(bio));
  if (meth != nullptr) {
    BIO_meth_free(meth);
    BIO_set_app_data(bio, nullptr);
  }

  return 1;
}

// NOLINTNEXTLINE(readability-identifier-naming)
int io_handle_read(BIO* b, char* out, int outl) {
  if (out == nullptr) {
    return 0;
  }

  Envoy::Buffer::RawSlice slice;
  slice.mem_ = out;
  slice.len_ = outl;
  auto result = bio_io_handle(b)->readv(outl, &slice, 1);
  BIO_clear_retry_flags(b);
  if (!result.ok()) {
    auto err = result.err_->getErrorCode();
    if (err == Api::IoError::IoErrorCode::Again || err == Api::IoError::IoErrorCode::Interrupt) {
      BIO_set_retry_read(b);
    } else {
      ERR_put_error(ERR_LIB_SYS, 0, result.err_->getSystemErrorCode(), __FILE__, __LINE__);
    }
    return -1;
  }
  return result.return_value_;
}

// NOLINTNEXTLINE(readability-identifier-naming)
int io_handle_write(BIO* b, const char* in, int inl) {
  Envoy::Buffer::RawSlice slice;
  slice.mem_ = const_cast<char*>(in);
  slice.len_ = inl;
  auto result = bio_io_handle(b)->writev(&slice, 1);
  BIO_clear_retry_flags(b);
  if (!result.ok()) {
    auto err = result.err_->getErrorCode();
    if (err == Api::IoError::IoErrorCode::Again || err == Api::IoError::IoErrorCode::Interrupt) {
      BIO_set_retry_write(b);
    } else {
      ERR_put_error(ERR_LIB_SYS, 0, result.err_->getSystemErrorCode(), __FILE__, __LINE__);
    }
    return -1;
  }
  return result.return_value_;
}

// NOLINTNEXTLINE(readability-identifier-naming)
long io_handle_ctrl(BIO* b, int cmd, long num, void*) {
  long ret = 1;

  switch (cmd) {
  case BIO_C_SET_FD:
    RELEASE_ASSERT(false, "should not be called");
    break;
  case BIO_C_GET_FD:
    RELEASE_ASSERT(false, "should not be called");
    break;
  case BIO_CTRL_GET_CLOSE:
    ret = BIO_get_shutdown(b);
    break;
  case BIO_CTRL_SET_CLOSE:
    BIO_set_shutdown(b, int(num));
    break;
  case BIO_CTRL_FLUSH:
    ret = 1;
    break;
  default:
    ret = 0;
    break;
  }
  return ret;
}
} // namespace

// NOLINTNEXTLINE(readability-identifier-naming)
BIO* BIO_new_io_handle(Envoy::Network::IoHandle* io_handle) {
  BIO* b;
  BIO_METHOD* meth = BIO_meth_new(BIO_get_new_index(), "io_handle");
  bool ok = meth != nullptr;

  ok = ok && BIO_meth_set_write(meth, io_handle_write) == 1;
  ok = ok && BIO_meth_set_read(meth, io_handle_read) == 1;
  ok = ok && BIO_meth_set_ctrl(meth, io_handle_ctrl) == 1;
  ok = ok && BIO_meth_set_create(meth, io_handle_new) == 1;
  ok = ok && BIO_meth_set_destroy(meth, io_handle_free) == 1;

  if (!ok && meth != nullptr) {
    BIO_meth_free(meth);
  }
  RELEASE_ASSERT(ok, "");

  b = BIO_new(meth);
  RELEASE_ASSERT(b != nullptr, "");

  // Initialize the BIO
  BIO_set_app_data(b, meth);
  BIO_set_data(b, io_handle);
  BIO_set_shutdown(b, 0);
  BIO_set_init(b, 1);

  return b;
}

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy

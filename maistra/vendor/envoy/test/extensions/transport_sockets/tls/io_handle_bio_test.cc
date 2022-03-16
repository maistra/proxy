#include "source/common/network/io_socket_error_impl.h"
#include "source/extensions/transport_sockets/tls/io_handle_bio.h"

#include "test/mocks/network/io_handle.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "openssl/err.h"
#include "openssl/ssl.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

class IoHandleBioTest : public testing::Test {
public:
  IoHandleBioTest() {
    bio_ = BIO_new_io_handle(&io_handle_);
    meth_ = static_cast<BIO_METHOD*>(BIO_get_app_data(bio_));
  }
  ~IoHandleBioTest() override { BIO_free(bio_); }

  BIO* bio_;
  BIO_METHOD* meth_;
  NiceMock<Network::MockIoHandle> io_handle_;
};

TEST_F(IoHandleBioTest, WriteError) {
  EXPECT_CALL(io_handle_, writev(_, 1))
      .WillOnce(Return(testing::ByMove(
          Api::IoCallUint64Result(0, Api::IoErrorPtr(new Network::IoSocketError(100),
                                                     Network::IoSocketError::deleteIoError)))));
  EXPECT_EQ(-1, BIO_write(bio_, nullptr, 10));
  const int err = ERR_get_error();
  EXPECT_EQ(ERR_GET_LIB(err), ERR_LIB_SYS);
  EXPECT_EQ(ERR_GET_REASON(err), 100);
}

TEST_F(IoHandleBioTest, TestMiscApis) {
  EXPECT_EQ(BIO_meth_get_destroy(meth_)(nullptr), 0);
  EXPECT_EQ(BIO_meth_get_read(meth_)(nullptr, nullptr, 0), 0);

  EXPECT_DEATH(BIO_meth_get_ctrl(meth_)(bio_, BIO_C_GET_FD, 0, nullptr), "should not be called");
  EXPECT_DEATH(BIO_meth_get_ctrl(meth_)(bio_, BIO_C_SET_FD, 0, nullptr), "should not be called");

  int ret = BIO_meth_get_ctrl(meth_)(bio_, BIO_CTRL_RESET, 0, nullptr);
  EXPECT_EQ(ret, 0);

  ret = BIO_meth_get_ctrl(meth_)(bio_, BIO_CTRL_FLUSH, 0, nullptr);
  EXPECT_EQ(ret, 1);

  ret = BIO_meth_get_ctrl(meth_)(bio_, BIO_CTRL_SET_CLOSE, 1, nullptr);
  EXPECT_EQ(ret, 1);

  ret = BIO_meth_get_ctrl(meth_)(bio_, BIO_CTRL_GET_CLOSE, 0, nullptr);
  EXPECT_EQ(ret, 1);

  EXPECT_CALL(io_handle_, close())
      .WillOnce(Return(testing::ByMove(Api::IoCallUint64Result{
          0, Api::IoErrorPtr(nullptr, Network::IoSocketError::deleteIoError)})));
  BIO_set_init(bio_, 1);
}

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy

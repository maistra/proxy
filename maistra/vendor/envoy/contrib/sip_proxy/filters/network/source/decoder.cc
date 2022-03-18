#include "contrib/sip_proxy/filters/network/source/decoder.h"

#include "envoy/buffer/buffer.h"
#include "envoy/common/exception.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/assert.h"
#include "source/common/common/macros.h"

#include "contrib/sip_proxy/filters/network/source/app_exception_impl.h"
#include "re2/re2.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SipProxy {

DecoderStateMachine::DecoderStatus DecoderStateMachine::transportBegin() {
  return {State::MessageBegin, handler_.transportBegin(metadata_)};
}

DecoderStateMachine::DecoderStatus DecoderStateMachine::messageBegin() {
  return {State::MessageEnd, handler_.messageBegin(metadata_)};
}

DecoderStateMachine::DecoderStatus DecoderStateMachine::messageEnd() {
  return {State::TransportEnd, handler_.messageEnd()};
}

DecoderStateMachine::DecoderStatus DecoderStateMachine::transportEnd() {
  return {State::Done, handler_.transportEnd()};
}

DecoderStateMachine::DecoderStatus DecoderStateMachine::handleState() {
  switch (state_) {
  case State::TransportBegin:
    return transportBegin();
  case State::MessageBegin:
    return messageBegin();
  case State::MessageEnd:
    return messageEnd();
  case State::TransportEnd:
    return transportEnd();
  default:
    /* test failed report "panic:     not reached" if reach here */
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

State DecoderStateMachine::run() {
  while (state_ != State::Done) {
    ENVOY_LOG(trace, "sip: state {}", StateNameValues::name(state_));

    DecoderStatus s = handleState();

    state_ = s.next_state_;

    ASSERT(s.filter_status_.has_value());
    if (s.filter_status_.value() == FilterStatus::StopIteration) {
      return State::StopIteration;
    }
  }

  return state_;
}

Decoder::Decoder(DecoderCallbacks& callbacks) : callbacks_(callbacks) {}

void Decoder::complete() {
  request_.reset();
  metadata_.reset();
  state_machine_ = nullptr;
  start_new_message_ = true;

  current_header_ = HeaderType::TopLine;
  raw_offset_ = 0;

  first_via_ = true;
  first_route_ = true;
}

FilterStatus Decoder::onData(Buffer::Instance& data) {
  ENVOY_LOG(debug, "sip: {} bytes available", data.length());

  reassemble(data);
  return FilterStatus::StopIteration;
}

int Decoder::reassemble(Buffer::Instance& data) {
  // ENVOY_LOG(trace, "received --> {}\n{}", data.length(), data.toString());

  Buffer::Instance& remaining_data = data;

  int ret = 0;
  size_t clen = 0;         // Content-Length value
  size_t full_msg_len = 0; // Length of the entire message

  while (remaining_data.length() != 0) {
    ssize_t content_pos = remaining_data.search("\n\r\n", strlen("\n\r\n"), 0);
    if (content_pos != -1) {
      // Get the Content-Length header value so that we can find
      // out the full message length.
      //
      content_pos += 3; // move to the line after the CRLF line.

      ssize_t content_length_start =
          remaining_data.search("Content-Length:", strlen("Content-Length:"), 0, content_pos);
      if (content_length_start == -1) {
        break;
      }

      ssize_t content_length_end = remaining_data.search(
          "\r\n", strlen("\r\n"), content_length_start + strlen("Content-Length:"), content_pos);
      /* The "\n\r\n" is always included in remaining_data, so could not return -1
      if (content_length_end == -1) {
        break;
      }
      */

      char len[10]{}; // temporary storage
      remaining_data.copyOut(content_length_start + strlen("Content-Length:"),
                             content_length_end - content_length_start - strlen("Content-Length:"),
                             len);

      clen = std::atoi(len);

      // Fail if Content-Length is less then zero
      //
      /* atoi return value >= 0, could not < 0
      if (clen < static_cast<size_t>(0)) {
        break;
      }
      */

      full_msg_len = content_pos + clen;
    }

    // Check for partial message received.
    //
    if ((full_msg_len == 0) || (full_msg_len > remaining_data.length())) {
      break;
    } else {
      // We have a full SIP message; put it on the dispatch queue.
      //
      Buffer::OwnedImpl message{};
      message.move(remaining_data, full_msg_len);
      /* status not used
      auto status = onDataReady(message);
      */
      onDataReady(message);
      message.drain(message.length());
      full_msg_len = 0;
      /* no handle for this if
      if (status != FilterStatus::StopIteration) {
        // break;
      }*/
    }
  } // End of while (remaining_data_len > 0)

  return ret;
}

FilterStatus Decoder::onDataReady(Buffer::Instance& data) {
  ENVOY_LOG(trace, "onDataReady {}\n{}", data.length(), data.toString());

  metadata_ = std::make_shared<MessageMetadata>(data.toString());

  decode();

  request_ = std::make_unique<ActiveRequest>(callbacks_.newDecoderEventHandler(metadata_));
  state_machine_ = std::make_unique<DecoderStateMachine>(metadata_, request_->handler_);
  State rv = state_machine_->run();

  if (rv == State::Done || rv == State::StopIteration) {
    complete();
  }

  return FilterStatus::StopIteration;
}

auto Decoder::sipHeaderType(absl::string_view sip_line) {
  static std::map<absl::string_view, HeaderType> sip_header_type_map{
      {"Call-ID", HeaderType::CallId},
      {"Via", HeaderType::Via},
      {"To", HeaderType::To},
      {"From", HeaderType::From},
      {"Contact", HeaderType::Contact},
      {"Record-Route", HeaderType::RRoute},
      {"CSeq", HeaderType::Cseq},
      {"Route", HeaderType::Route},
      {"Path", HeaderType::Path},
      {"Event", HeaderType::Event},
      {"Service-Route", HeaderType::SRoute},
      {"WWW-Authenticate", HeaderType::WAuth},
      {"Authorization", HeaderType::Auth},
      {"", HeaderType::Other}};

  auto header_type_str = sip_line.substr(0, sip_line.find_first_of(':'));
  if (auto result = sip_header_type_map.find(header_type_str);
      result != sip_header_type_map.end()) {
    return std::tuple<HeaderType, absl::string_view>{
        result->second, sip_line.substr(sip_line.find_first_of(':') + strlen(": "))};
  } else {
    return std::tuple<HeaderType, absl::string_view>{
        HeaderType::Other, sip_line.substr(sip_line.find_first_of(':') + strlen(": "))};
  }
}

MsgType Decoder::sipMsgType(absl::string_view top_line) {
  if (top_line.find("SIP/2.0 ") == absl::string_view::npos) {
    return MsgType::Request;
  } else {
    return MsgType::Response;
  }
}

MethodType Decoder::sipMethod(absl::string_view top_line) {
  if (top_line.find("INVITE") != absl::string_view::npos) {
    return MethodType::Invite;
  } else if (top_line.find("CANCEL") != absl::string_view::npos) {
    return MethodType::Cancel;
  } else if (top_line.find("REGISTER") != absl::string_view::npos) {
    return MethodType::Register;
  } else if (top_line.find("REFER") != absl::string_view::npos) {
    return MethodType::Refer;
  } else if (top_line.find("UPDATE") != absl::string_view::npos) {
    return MethodType::Update;
  } else if (top_line.find("SUBSCRIBE") != absl::string_view::npos) {
    return MethodType::Subscribe;
  } else if (top_line.find("NOTIFY") != absl::string_view::npos) {
    return MethodType::Notify;
  } else if (top_line.find("ACK") != absl::string_view::npos) {
    return MethodType::Ack;
  } else if (top_line.find("BYE") != absl::string_view::npos) {
    return MethodType::Bye;
  } else if (top_line.find("2.0 200") != absl::string_view::npos) {
    return MethodType::Ok200;
  } else if (top_line.find("2.0 4") != absl::string_view::npos) {
    return MethodType::Failure4xx;
  } else {
    return MethodType::NullMethod;
  }
}

Decoder::HeaderHandler::HeaderHandler(MessageHandler& parent)
    : parent_(parent), header_processors_{
                           {HeaderType::Via, &HeaderHandler::processVia},
                           {HeaderType::Route, &HeaderHandler::processRoute},
                           {HeaderType::Contact, &HeaderHandler::processContact},
                           {HeaderType::Cseq, &HeaderHandler::processCseq},
                           {HeaderType::RRoute, &HeaderHandler::processRecordRoute},
                           {HeaderType::SRoute, &HeaderHandler::processServiceRoute},
                           {HeaderType::WAuth, &HeaderHandler::processWwwAuth},
                           {HeaderType::Auth, &HeaderHandler::processAuth},
                       } {}

int Decoder::HeaderHandler::processPath(absl::string_view& header) {
  metadata()->deleteInstipOperation(rawOffset(), header);
  metadata()->addEPOperation(rawOffset(), header, parent_.parent_.getOwnDomain(),
                             parent_.parent_.getDomainMatchParamName());
  return 0;
}

int Decoder::HeaderHandler::processRoute(absl::string_view& header) {
  if (!isFirstRoute()) {
    return 0;
  }
  setFirstRoute(false);

  if (auto loc = header.find(";ep="); loc != absl::string_view::npos) {
    // No "" of ep string
    auto start = loc + strlen(";ep=");
    if (auto end = header.find_first_of(";>", start); end != absl::string_view::npos) {
      metadata()->setRouteEP(header.substr(start, end - start));
    }
  }

  metadata()->setTopRoute(header);
  metadata()->setDomain(header, parent_.parent_.getDomainMatchParamName());
  return 0;
}

int Decoder::HeaderHandler::processRecordRoute(absl::string_view& header) {
  if (!isFirstRecordRoute()) {
    return 0;
  }

  setFirstRecordRoute(false);

  metadata()->addEPOperation(rawOffset(), header, parent_.parent_.getOwnDomain(),
                             parent_.parent_.getDomainMatchParamName());
  return 0;
}

int Decoder::HeaderHandler::processWwwAuth(absl::string_view& header) {
  metadata()->addOpaqueOperation(rawOffset(), header);
  return 0;
}

int Decoder::HeaderHandler::processAuth(absl::string_view& header) {
  auto loc = header.find(", opaque=");
  if (loc == absl::string_view::npos) {
    return 0;
  }
  // has ""
  auto start = loc + strlen(", opaque=\"");
  auto end = header.find("\"", start);
  if (end == absl::string_view::npos) {
    return 0;
  }

  metadata()->setRouteOpaque(header.substr(start, end - start - 1));
  return 0;
}

//
// 200 OK Header Handler
//
int Decoder::OK200HeaderHandler::processCseq(absl::string_view& header) {
  if (header.find("INVITE") != absl::string_view::npos) {
    metadata()->setRespMethodType(MethodType::Invite);
  } else {
    /* need to set a value, else when processRecordRoute,
     *(metadata()->respMethodType() != MethodType::Invite) always false
     * TODO: need to handle non-invite 200OK
     */
    metadata()->setRespMethodType(MethodType::NullMethod);
  }
  return 0;
}

int Decoder::HeaderHandler::processContact(absl::string_view& header) {
  metadata()->deleteInstipOperation(rawOffset(), header);
  metadata()->addEPOperation(rawOffset(), header, parent_.parent_.getOwnDomain(),
                             parent_.parent_.getDomainMatchParamName());

  return 0;
}

int Decoder::HeaderHandler::processServiceRoute(absl::string_view& header) {
  if (!isFirstServiceRoute()) {
    return 0;
  }
  setFirstServiceRoute(false);

  metadata()->addEPOperation(rawOffset(), header, parent_.parent_.getOwnDomain(),
                             parent_.parent_.getDomainMatchParamName());
  return 0;
}

//
// SUBSCRIBE Header Handler
//
int Decoder::SUBSCRIBEHeaderHandler::processEvent(absl::string_view& header) {
  auto& parent = dynamic_cast<SUBSCRIBEHandler&>(this->parent_);
  parent.setEventType(StringUtil::trim(header.substr(header.find("Event:") + strlen("Event:"))));
  return 0;
}

void Decoder::REGISTERHandler::parseHeader(HeaderType& type, absl::string_view& header) {
  switch (type) {
  case HeaderType::Route:
    handler_->processRoute(header);
    break;
  case HeaderType::Via:
    handler_->processVia(header);
    break;
  case HeaderType::Contact:
    handler_->processContact(header);
    break;
  case HeaderType::Path:
    handler_->processPath(header);
    break;
  case HeaderType::RRoute:
    handler_->processRecordRoute(header);
    break;
  case HeaderType::Auth:
    handler_->processAuth(header);
    break;
  default:
    break;
  }
}

void Decoder::INVITEHandler::parseHeader(HeaderType& type, absl::string_view& header) {
  switch (type) {
  case HeaderType::Via:
    handler_->processVia(header);
    break;
  case HeaderType::Route:
    handler_->processRoute(header);
    break;
  case HeaderType::RRoute:
    handler_->processRecordRoute(header);
    break;
  case HeaderType::Contact:
    handler_->processContact(header);
    break;
  default:
    break;
  }
}

void Decoder::OK200Handler::parseHeader(HeaderType& type, absl::string_view& header) {
  switch (type) {
  case HeaderType::Cseq:
    handler_->processCseq(header);
    break;
  case HeaderType::Contact:
    handler_->processContact(header);
    break;
  case HeaderType::RRoute:
    handler_->processRecordRoute(header);
    break;
  case HeaderType::Via:
    handler_->processVia(header);
    break;
  case HeaderType::Path:
    handler_->processPath(header);
    break;
  case HeaderType::SRoute:
    handler_->processServiceRoute(header);
    break;
  default:
    break;
  }
}

void Decoder::GeneralHandler::parseHeader(HeaderType& type, absl::string_view& header) {
  switch (type) {
  case HeaderType::Route:
    handler_->processRoute(header);
    break;
  case HeaderType::Via:
    handler_->processVia(header);
    break;
  case HeaderType::Contact:
    handler_->processContact(header);
    break;
  case HeaderType::Path:
    handler_->processPath(header);
    break;
  case HeaderType::RRoute:
    handler_->processRecordRoute(header);
    break;
  default:
    break;
  }
}

void Decoder::SUBSCRIBEHandler::parseHeader(HeaderType& type, absl::string_view& header) {
  switch (type) {
  case HeaderType::Event:
    handler_->processEvent(header);
    break;
  case HeaderType::Route:
    handler_->processRoute(header);
    break;
  case HeaderType::Via:
    handler_->processVia(header);
    break;
  case HeaderType::Contact:
    handler_->processContact(header);
    break;
  case HeaderType::RRoute:
    handler_->processRecordRoute(header);
    break;
  default:
    break;
  }
}

void Decoder::FAILURE4XXHandler::parseHeader(HeaderType& type, absl::string_view& header) {
  switch (type) {
  case HeaderType::Contact:
    handler_->processContact(header);
    break;
  case HeaderType::WAuth:
    handler_->processWwwAuth(header);
    break;
  case HeaderType::Via:
    handler_->processVia(header);
    break;
  default:
    break;
  }
}

void Decoder::OthersHandler::parseHeader(HeaderType& type, absl::string_view& header) {
  switch (type) {
  case HeaderType::Via:
    handler_->processVia(header);
    break;
  case HeaderType::Contact:
    handler_->processContact(header);
    break;
  case HeaderType::Path:
    handler_->processPath(header);
    break;
  case HeaderType::RRoute:
    handler_->processRecordRoute(header);
    break;
  case HeaderType::SRoute:
    handler_->processServiceRoute(header);
    break;
  default:
    break;
  }
}

std::shared_ptr<Decoder::MessageHandler> Decoder::MessageFactory::create(MethodType type,
                                                                         Decoder& parent) {
  switch (type) {
  case MethodType::Invite:
    return std::make_shared<INVITEHandler>(parent);
  case MethodType::Ok200:
    return std::make_shared<OK200Handler>(parent);
  case MethodType::Register:
    return std::make_shared<REGISTERHandler>(parent);
  case MethodType::Subscribe:
    return std::make_shared<SUBSCRIBEHandler>(parent);
  case MethodType::Failure4xx:
    return std::make_shared<FAILURE4XXHandler>(parent);
  case MethodType::Ack:
  case MethodType::Bye:
  case MethodType::Cancel:
    return std::make_shared<GeneralHandler>(parent);
  default:
    return std::make_shared<OthersHandler>(parent);
  }
}

int Decoder::decode() {
  auto& metadata = metadata_;
  absl::string_view msg = absl::string_view(metadata->rawMsg());

  std::shared_ptr<MessageHandler> handler;

  while (!msg.empty()) {
    std::string::size_type crlf = msg.find("\r\n");
    // After message reassemble, this condition could not be true
    // if (crlf == absl::string_view::npos) {
    //   break;
    // }

    if (current_header_ == HeaderType::TopLine) {
      // Sip Request Line
      absl::string_view sip_line = msg.substr(0, crlf);

      parseTopLine(sip_line);
      current_header_ = HeaderType::Other;

      handler = MessageFactory::create(metadata->methodType(), *this);
    } else {
      // Normal Header Line
      absl::string_view sip_line = msg.substr(0, crlf);
      auto [current_header, header_value] = sipHeaderType(sip_line);
      this->current_header_ = current_header;
      handler->parseHeader(current_header, sip_line);
    }

    msg = msg.substr(crlf + strlen("\r\n"));
    raw_offset_ += crlf + strlen("\r\n");

#if __cplusplus > 201703L
    if (msg.starts_with("\r\n")) {
#else
    if (msg[0] == '\r' && msg[1] == '\n') {
#endif
      break;
    }
  }

  if (!metadata->topRoute().has_value() && metadata->msgType() == MsgType::Request) {
    metadata->setDomain(metadata->requestURI().value(), getDomainMatchParamName());
  }
  return 0;
}

int Decoder::HeaderHandler::processVia(absl::string_view& header) {
  if (!isFirstVia()) {
    return 0;
  }

  metadata()->setTransactionId(header);

  setFirstVia(false);
  return 0;
}

int Decoder::parseTopLine(absl::string_view& top_line) {
  auto metadata = metadata_;
  metadata->setMsgType(sipMsgType(top_line));
  metadata->setMethodType(sipMethod(top_line));

  if (metadata->msgType() == MsgType::Request) {
    metadata->setRequestURI(top_line);
  }

  if (auto loc = top_line.find(";ep="); loc != absl::string_view::npos) {
    // Need to exclude the "" of ep string
    auto start = loc + strlen(";ep=");

    if (auto end = top_line.find_first_of("; ", start); end != absl::string_view::npos) {
      metadata->setRouteEP(top_line.substr(start, end - start));
    }
  }
  return 0;
}

} // namespace SipProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy

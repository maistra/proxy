#include "source/common/network/apple_dns_impl.h"

#include <dns_sd.h>

#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "envoy/common/platform.h"
#include "envoy/event/file_event.h"

#include "source/common/common/assert.h"
#include "source/common/common/fmt.h"
#include "source/common/network/address_impl.h"
#include "source/common/network/utility.h"

#include "absl/strings/str_join.h"

namespace Envoy {
namespace Network {

void DnsService::dnsServiceRefDeallocate(DNSServiceRef sdRef) { DNSServiceRefDeallocate(sdRef); }

DNSServiceErrorType DnsService::dnsServiceCreateConnection(DNSServiceRef* sdRef) {
  return DNSServiceCreateConnection(sdRef);
}

dnssd_sock_t DnsService::dnsServiceRefSockFD(DNSServiceRef sdRef) {
  return DNSServiceRefSockFD(sdRef);
}

DNSServiceErrorType DnsService::dnsServiceProcessResult(DNSServiceRef sdRef) {
  return DNSServiceProcessResult(sdRef);
}

DNSServiceErrorType DnsService::dnsServiceGetAddrInfo(DNSServiceRef* sdRef, DNSServiceFlags flags,
                                                      uint32_t interfaceIndex,
                                                      DNSServiceProtocol protocol,
                                                      const char* hostname,
                                                      DNSServiceGetAddrInfoReply callBack,
                                                      void* context) {
  return DNSServiceGetAddrInfo(sdRef, flags, interfaceIndex, protocol, hostname, callBack, context);
}

AppleDnsResolverImpl::AppleDnsResolverImpl(Event::Dispatcher& dispatcher, Stats::Scope& root_scope)
    : dispatcher_(dispatcher), scope_(root_scope.createScope("dns.apple.")),
      stats_(generateAppleDnsResolverStats(*scope_)) {}

AppleDnsResolverStats AppleDnsResolverImpl::generateAppleDnsResolverStats(Stats::Scope& scope) {
  return {ALL_APPLE_DNS_RESOLVER_STATS(POOL_COUNTER(scope))};
}

AppleDnsResolverImpl::StartResolutionResult
AppleDnsResolverImpl::startResolution(const std::string& dns_name,
                                      DnsLookupFamily dns_lookup_family, ResolveCb callback) {
  ENVOY_LOG_EVENT(debug, "apple_dns_start", "DNS resolution for {} started", dns_name);

  // When an IP address is submitted to c-ares in DnsResolverImpl, c-ares synchronously returns
  // the IP without submitting a DNS query. Because Envoy has come to rely on this behavior, this
  // resolver implements a similar resolution path to avoid making improper DNS queries for
  // resolved IPs.
  auto address = Utility::parseInternetAddressNoThrow(dns_name);

  if (address != nullptr) {
    ENVOY_LOG_EVENT(debug, "apple_dns_immediate_resolution",
                    "DNS resolver resolved ({}) to ({}) without issuing call to Apple API",
                    dns_name, address->asString());
    callback(DnsResolver::ResolutionStatus::Success,
             {DnsResponse(address, std::chrono::seconds(60))});
    return {nullptr, true};
  }

  ENVOY_LOG(trace, "Performing DNS resolution via Apple APIs");
  auto pending_resolution = std::make_unique<PendingResolution>(*this, callback, dispatcher_,
                                                                dns_name, dns_lookup_family);

  DNSServiceErrorType error = pending_resolution->dnsServiceGetAddrInfo();
  if (error != kDNSServiceErr_NoError) {
    ENVOY_LOG(warn, "DNS resolver error ({}) in dnsServiceGetAddrInfo for {}", error, dns_name);
    chargeGetAddrInfoErrorStats(error);
    return {nullptr, false};
  }

  if (pending_resolution->synchronously_completed_) {
    return {nullptr, true};
  }

  // Hook up the query's UDS socket to the event loop to process updates.
  if (!pending_resolution->dnsServiceRefSockFD()) {
    ENVOY_LOG(warn, "DNS resolver error in dnsServiceRefSockFD for {}", dns_name);
    return {nullptr, false};
  }

  // Return the active resolution query, giving it ownership over itself so that it can
  // can clean itself up once it's done.
  pending_resolution->owned_ = true;

  return {std::move(pending_resolution), true};
}

ActiveDnsQuery* AppleDnsResolverImpl::resolve(const std::string& dns_name,
                                              DnsLookupFamily dns_lookup_family,
                                              ResolveCb callback) {
  auto pending_resolution_and_success = startResolution(dns_name, dns_lookup_family, callback);

  // If we synchronously failed the resolution, trigger a failure callback.
  if (!pending_resolution_and_success.second) {
    ENVOY_LOG_EVENT(debug, "apple_dns_immediate_failure", "DNS resolution for {} failed", dns_name);

    callback(DnsResolver::ResolutionStatus::Failure, {});
    return nullptr;
  }

  return pending_resolution_and_success.first.release();
}

void AppleDnsResolverImpl::chargeGetAddrInfoErrorStats(DNSServiceErrorType error_code) {
  switch (error_code) {
  case kDNSServiceErr_DefunctConnection:
    stats_.connection_failure_.inc();
    break;
  case kDNSServiceErr_NoRouter:
    stats_.network_failure_.inc();
    break;
  case kDNSServiceErr_Timeout:
    stats_.timeout_.inc();
    break;
  default:
    stats_.get_addr_failure_.inc();
    break;
  }
}

AppleDnsResolverImpl::PendingResolution::PendingResolution(AppleDnsResolverImpl& parent,
                                                           ResolveCb callback,
                                                           Event::Dispatcher& dispatcher,
                                                           const std::string& dns_name,
                                                           DnsLookupFamily dns_lookup_family)
    : parent_(parent), callback_(callback), dispatcher_(dispatcher), dns_name_(dns_name),
      pending_cb_({ResolutionStatus::Success, {}, {}}), dns_lookup_family_(dns_lookup_family) {}

AppleDnsResolverImpl::PendingResolution::~PendingResolution() {
  ENVOY_LOG(debug, "Destroying PendingResolution for {}", dns_name_);

  // dns_sd.h says:
  //   If the reference's underlying socket is used in a run loop or select() call, it should
  //   be removed BEFORE DNSServiceRefDeallocate() is called, as this function closes the
  //   reference's socket.
  sd_ref_event_.reset();

  // It is possible that DNSServiceGetAddrInfo returns a synchronous error, with a NULLed
  // DNSServiceRef, in AppleDnsResolverImpl::resolve.
  // Additionally, it is also possible that the query is cancelled before resolution starts, and
  // thus the DNSServiceRef is null.
  // Therefore, only deallocate if the ref is not null.
  if (sd_ref_) {
    ENVOY_LOG(debug, "DNSServiceRefDeallocate individual sd ref");
    DnsServiceSingleton::get().dnsServiceRefDeallocate(sd_ref_);
  }
}

void AppleDnsResolverImpl::PendingResolution::cancel(Network::ActiveDnsQuery::CancelReason) {
  // TODO(mattklein123): If cancel reason is timeout, do something more aggressive about destroying
  // and recreating the DNS system to maximize the chance of success in following queries.
  ENVOY_LOG(debug, "Cancelling PendingResolution for {}", dns_name_);
  ASSERT(owned_);
  // Because the query is self-owned, delete now.
  delete this;
}

void AppleDnsResolverImpl::PendingResolution::onEventCallback(uint32_t events) {
  ENVOY_LOG(debug, "DNS resolver file event ({})", events);
  RELEASE_ASSERT(events & Event::FileReadyType::Read,
                 fmt::format("invalid FileReadyType event={}", events));
  DNSServiceErrorType error = DnsServiceSingleton::get().dnsServiceProcessResult(sd_ref_);
  if (error != kDNSServiceErr_NoError) {
    ENVOY_LOG(warn, "DNS resolver error ({}) in DNSServiceProcessResult", error);
    parent_.stats_.processing_failure_.inc();
    // Similar to receiving an error in onDNSServiceGetAddrInfoReply, an error while processing fd
    // events indicates that the sd_ref state is broken.
    // Therefore, finish resolving with an error.
    pending_cb_.status_ = ResolutionStatus::Failure;
    finishResolve();
  }
}

std::list<DnsResponse>& AppleDnsResolverImpl::PendingResolution::finalAddressList() {
  switch (dns_lookup_family_) {
  case DnsLookupFamily::V4Only:
    return pending_cb_.v4_responses_;
  case DnsLookupFamily::V6Only:
    return pending_cb_.v6_responses_;
  case DnsLookupFamily::Auto:
    // Per API docs only give v4 if v6 is not available.
    if (pending_cb_.v6_responses_.empty()) {
      return pending_cb_.v4_responses_;
    }
    return pending_cb_.v6_responses_;
  case DnsLookupFamily::V4Preferred:
    // Per API docs only give v6 if v4 is not available.
    if (pending_cb_.v4_responses_.empty()) {
      return pending_cb_.v6_responses_;
    }
    return pending_cb_.v4_responses_;
  }
  NOT_REACHED_GCOVR_EXCL_LINE;
}

void AppleDnsResolverImpl::PendingResolution::finishResolve() {
  ENVOY_LOG_EVENT(debug, "apple_dns_resolution_complete",
                  "dns resolution for {} completed with status {}", dns_name_, pending_cb_.status_);
  callback_(pending_cb_.status_, std::move(finalAddressList()));

  if (owned_) {
    ENVOY_LOG(debug, "Resolution for {} completed (async)", dns_name_);
    delete this;
  } else {
    ENVOY_LOG(debug, "Resolution for {} completed (synchronously)", dns_name_);
    synchronously_completed_ = true;
  }
}

DNSServiceErrorType AppleDnsResolverImpl::PendingResolution::dnsServiceGetAddrInfo() {
  DNSServiceProtocol protocol;
  switch (dns_lookup_family_) {
  case DnsLookupFamily::V4Only:
    protocol = kDNSServiceProtocol_IPv4;
    break;
  case DnsLookupFamily::V6Only:
    protocol = kDNSServiceProtocol_IPv6;
    break;
  case DnsLookupFamily::Auto:
  case DnsLookupFamily::V4Preferred:
    protocol = kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6;
    break;
  }

  // TODO: explore caching: there are caching flags in the dns_sd.h flags, allow expired answers
  // from the cache?
  // TODO: explore validation via `DNSSEC`?
  return DnsServiceSingleton::get().dnsServiceGetAddrInfo(
      &sd_ref_, kDNSServiceFlagsTimeout, 0, protocol, dns_name_.c_str(),
      /*
       * About Thread Safety (taken from inline documentation there):
       * The dns_sd.h API does not presuppose any particular threading model, and consequently
       * does no locking internally (which would require linking with a specific threading library).
       * If the client concurrently, from multiple threads (or contexts), calls API routines using
       * the same DNSServiceRef, it is the client's responsibility to provide mutual exclusion for
       * that DNSServiceRef.
       */

      // Therefore, much like the c-ares implementation All calls and callbacks to the API need to
      // happen on the thread that owns the creating dispatcher. This is the case as callbacks are
      // driven by processing bytes in onEventCallback which run on the passed in dispatcher's event
      // loop.
      [](DNSServiceRef, DNSServiceFlags flags, uint32_t interface_index,
         DNSServiceErrorType error_code, const char* hostname, const struct sockaddr* address,
         uint32_t ttl, void* context) {
        static_cast<PendingResolution*>(context)->onDNSServiceGetAddrInfoReply(
            flags, interface_index, error_code, hostname, address, ttl);
      },
      this);
}

void AppleDnsResolverImpl::PendingResolution::onDNSServiceGetAddrInfoReply(
    DNSServiceFlags flags, uint32_t interface_index, DNSServiceErrorType error_code,
    const char* hostname, const struct sockaddr* address, uint32_t ttl) {
  ENVOY_LOG(debug,
            "DNS for {} resolved with: flags={}[MoreComing={}, Add={}], interface_index={}, "
            "error_code={}, hostname={}",
            dns_name_, flags, flags & kDNSServiceFlagsMoreComing ? "yes" : "no",
            flags & kDNSServiceFlagsAdd ? "yes" : "no", interface_index, error_code, hostname);

  // Make sure that we trigger the failure callback if we get an error back.
  if (error_code != kDNSServiceErr_NoError) {
    parent_.chargeGetAddrInfoErrorStats(error_code);

    pending_cb_.status_ = ResolutionStatus::Failure;
    pending_cb_.v4_responses_.clear();
    pending_cb_.v6_responses_.clear();

    finishResolve();
    // Note: Nothing can follow this call to flushPendingQueries due to deletion of this
    // object upon resolution.
    return;
  }

  // dns_sd.h does not call out behavior where callbacks to DNSServiceGetAddrInfoReply
  // would respond without the flag. However, Envoy's API is solely additive.
  // Therefore, only add this address to the list if kDNSServiceFlagsAdd is set.
  if (flags & kDNSServiceFlagsAdd) {
    ASSERT(address, "invalid to add null address");
    auto dns_response = buildDnsResponse(address, ttl);
    ENVOY_LOG(debug, "Address to add address={}, ttl={}",
              dns_response.address_->ip()->addressAsString(), ttl);
    if (dns_response.address_->ip()->ipv4()) {
      pending_cb_.v4_responses_.push_back(dns_response);
    } else {
      ASSERT(dns_response.address_->ip()->ipv6());
      pending_cb_.v6_responses_.push_back(dns_response);
    }
  }

  if (!(flags & kDNSServiceFlagsMoreComing)) {
    ENVOY_LOG(debug, "DNS Resolver flushing queries pending callback");
    finishResolve();
    // Note: Nothing can follow this call to finishResolve due to deletion of this
    // object upon resolution.
    return;
  }
}

bool AppleDnsResolverImpl::PendingResolution::dnsServiceRefSockFD() {
  auto fd = DnsServiceSingleton::get().dnsServiceRefSockFD(sd_ref_);
  // According to dns_sd.h: DnsServiceRefSockFD returns "The DNSServiceRef's underlying socket
  // descriptor, or -1 on error.". Although it gives no detailed description on when/why this call
  // would fail.
  if (fd == -1) {
    parent_.stats_.socket_failure_.inc();
    return false;
  }

  sd_ref_event_ = dispatcher_.createFileEvent(
      fd,
      // note: Event::FileTriggerType::Level is used here to closely resemble the c-ares
      // implementation in dns_impl.cc.
      [this](uint32_t events) { onEventCallback(events); }, Event::FileTriggerType::Level,
      Event::FileReadyType::Read);
  sd_ref_event_->setEnabled(Event::FileReadyType::Read);
  return true;
}

DnsResponse
AppleDnsResolverImpl::PendingResolution::buildDnsResponse(const struct sockaddr* address,
                                                          uint32_t ttl) {
  switch (address->sa_family) {
  case AF_INET:
    sockaddr_in address_in;
    memset(&address_in, 0, sizeof(address_in));
    address_in.sin_family = AF_INET;
    address_in.sin_port = 0;
    address_in.sin_addr = reinterpret_cast<const sockaddr_in*>(address)->sin_addr;
    return {std::make_shared<const Address::Ipv4Instance>(&address_in), std::chrono::seconds(ttl)};
  case AF_INET6:
    sockaddr_in6 address_in6;
    memset(&address_in6, 0, sizeof(address_in6));
    address_in6.sin6_family = AF_INET6;
    address_in6.sin6_port = 0;
    address_in6.sin6_addr = reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr;
    return {std::make_shared<const Address::Ipv6Instance>(address_in6), std::chrono::seconds(ttl)};
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

} // namespace Network
} // namespace Envoy

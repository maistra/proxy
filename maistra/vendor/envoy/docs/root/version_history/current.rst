1.20.4 (June 9, 2022)
=====================
1.20.6 (June 16, 2022)
======================
1.20.8 (Pending)
================

Incompatible Behavior Changes
-----------------------------
*Changes that are expected to cause an incompatibility if applicable; deployment changes are likely required*

* http: validate upstream request header names and values. The new runtime flag ``envoy.reloadable_features.validate_upstream_headers`` can be used for revert this behavior.

Minor Behavior Changes
----------------------
*Changes that may cause incompatibilities for some users, but should not for most*

Bug Fixes
---------
*Changes expected to improve the state of the world and are unlikely to have negative effects*

* grpc: when Envoy was configured to use ext_authz, ext_proc, tap, ratelimit filters, and grpc access log service and an http header with non-UTF-8 data was received, Envoy would generate an invalid protobuf message and send it to the configured service. The receiving service would typically generate an error when decoding the protobuf message. For ext_authz that was configured with ``failure_mode_allow: true``, the request would have been allowed in this case. For the other services, this could have resulted in other unforseen errors such as a lack of visibility into requests (eg request not logged). Envoy will now by default sanitize the values sent in gRPC service calls to be valid UTF-8, replacing data that is not valid UTF-8 with a '!' character. This behavioral change can be temporarily reverted by setting runtime guard ``envoy.reloadable_features.service_sanitize_non_utf8_strings`` to false
* http: fixed a bug where ``x-envoy-original-path`` was not being sanitized when sent from untrusted users. This behavioral change can be temporarily reverted by setting ``envoy.reloadable_features.sanitize_original_path`` to false.
*  http: stop forwarding ``:method`` value which is not a valid token defined in https://www.rfc-editor.org/rfc/rfc9110#section-5.6.2.
  Also, reject ``:method`` and ``:scheme`` headers with multiple values.
* http3: reject pseudo headers violating RFC 9114. Specifically, pseudo-header fields with more than one value for the ``:method`` (non-``CONNECT``),
  ``:scheme``, and ``:path``; or pseudo-header fields after regular header fields; or undefined pseudo-headers.
* lua: lua coroutine should not execute after local reply is sent.
* decompression: fixed CVE-2022-29225 due to which decompressors can be zip bombed. Previously decompressors were susceptible to memory inflation in takes in which specially crafted payloads could cause a large amount of memory usage by Envoy. The max inflation payload size is now limited.  This change can be reverted via the ``envoy.reloadable_features.enable_compression_bomb_protection`` runtime flag.
* health_check: fixed CVE-2022-29224 which caused a segfault in GrpcHealthCheckerImpl. An attacker-controlled upstream server that is health checked using gRPC health checking can crash Envoy via a null pointer dereference in certain circumstances.
* oauth: fixed CVE-2022-29226 due to which oauth filter allows trivial bypass. The OAuth filter implementation does not include a mechanism for validating access tokens, so by design when the HMAC signed cookie is missing a full authentication flow should be triggered. However, the current implementation assumes that access tokens are always validated thus allowing access in the presence of any access token attached to the request.
* oauth: fixed CVE-2022-29228 due to which oauth filter calls continueDecoding() from within decodeHeaders(). The OAuth filter would try to invoke the remaining filters in the chain after emitting a local response, which triggers an ASSERT() in newer versions and corrupts memory on earlier versions.
* router: fixed CVE-2022-29227 which caused an internal redirect crash for requests with body/trailers. Envoy would previously crash in some cases when processing internal redirects for requests with bodies or trailers if the redirect prompts an Envoy-generated local reply.
* ci: fix disk space issue that have prevented publication.
* oauth: fixed a bug where the oauth2 filter would crash if it received a redirect URL without a state query param set.

28-July-2023
------------
* Fix memory leak in nghttp2 when scheduled requests are cancelled due to the ``GOAWAY`` frame being received from the upstream service.
* Fixed a cookie validator bug that meant the HMAC calculation could be the same for different payloads.
  This prevents malicious clients from constructing credentials with permanent validity in some specific scenarios.
* Switched Envoy internal scheme checks from case sensitive to case insensitive. This behaviorial change can be temporarily
  reverted by setting runtime guard ``envoy.reloadable_features.handle_uppercase_scheme`` to ``false``.

Removed Config or Runtime
-------------------------

New Features
------------

Deprecated
----------

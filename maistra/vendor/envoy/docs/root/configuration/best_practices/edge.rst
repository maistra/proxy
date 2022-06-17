.. _best_practices_edge:

Configuring Envoy as an edge proxy
==================================

Envoy is a production-ready edge proxy, however, the default settings are tailored
for the service mesh use case, and some values need to be adjusted when using Envoy
as an edge proxy.

TCP proxies should configure:

* restrict access to the admin endpoint,
* :ref:`overload_manager <config_overload_manager>`,
* :ref:`listener buffer limits <envoy_api_field_Listener.per_connection_buffer_limit_bytes>` to 32 KiB,
* :ref:`cluster buffer limits <envoy_api_field_Cluster.per_connection_buffer_limit_bytes>` to 32 KiB.

HTTP proxies should additionally configure:

* :ref:`use_remote_address <envoy_api_field_config.filter.network.http_connection_manager.v2.HttpConnectionManager.use_remote_address>`
  to true (to avoid consuming HTTP headers from external clients, see :ref:`HTTP header sanitizing <config_http_conn_man_header_sanitizing>`
  for details),
* :ref:`connection and stream timeouts <faq_configuration_timeouts>`,
* :ref:`HTTP/2 maximum concurrent streams limit <envoy_api_field_core.Http2ProtocolOptions.max_concurrent_streams>` to 100,
* :ref:`HTTP/2 initial stream window size limit <envoy_api_field_core.Http2ProtocolOptions.initial_stream_window_size>` to 64 KiB,
* :ref:`HTTP/2 initial connection window size limit <envoy_api_field_core.Http2ProtocolOptions.initial_connection_window_size>` to 1 MiB.
* :ref:`headers_with_underscores_action setting <envoy_api_field_core.HttpProtocolOptions.headers_with_underscores_action>` to REJECT_REQUEST, to protect upstream services that treat '_' and '-' as interchangeable.
* :ref:`Listener connection limits. <config_listeners_runtime>`
* :ref:`Global downstream connection limits <config_overload_manager>`.

If Envoy is configured with RBAC filter or makes route selection based on URL path it is recommended to enable the following path normalization options to minimize probability of path confusion vulnerabilities. Path confusion vulnerabilities occur when parties participating in request use different path representations.

* Enable :ref:`normalize_path setting <envoy_v3_api_field_extensions.filters.network.http_connection_manager.v3.HttpConnectionManager.normalize_path>`.
* Enable :ref:`merge_slashes setting <envoy_v3_api_field_extensions.filters.network.http_connection_manager.v3.HttpConnectionManager.merge_slashes>`.

Additionally the :ref:`path_with_escaped_slashes_action setting <envoy_v3_api_field_extensions.filters.network.http_connection_manager.v3.HttpConnectionManager.path_with_escaped_slashes_action>` should be set according to following recommendations:

* REJECT_REQUEST if dowstream clients are expected to use rfc3986 compliant normalized paths (i.e. gRPC clients).
* UNESCAPE_AND_REDIRECT if downstream client supports HTTP redirect (i.e. a browser). This option minimizes possibility of path confusion by forcing request to be re-issued with the same path across all parties: downstream client, Envoy and upstream server. Note that gRPC requests will still be rejected with the INTERNAL (13) error code, as gRPC clients do not support redirect.
* KEEP_UNCHANGED for servers that are not rfc3986 compliant and require encoded slashes.
* UNESCAPE_AND_FORWARD for servers that are known to treat escaped and unescaped slashes equivalently. Choosing this option may increase probablity of path confusion vulnerabilities if intermediaries perform path based access control.

The following is a YAML example of the above recommendation.

.. code-block:: yaml

  overload_manager:
    refresh_interval: 0.25s
    resource_monitors:
    - name: "envoy.resource_monitors.fixed_heap"
      typed_config:
        "@type": type.googleapis.com/envoy.config.resource_monitor.fixed_heap.v2alpha.FixedHeapConfig
        # TODO: Tune for your system.
        max_heap_size_bytes: 2147483648 # 2 GiB
    actions:
    - name: "envoy.overload_actions.shrink_heap"
      triggers:
      - name: "envoy.resource_monitors.fixed_heap"
        threshold:
          value: 0.95
    - name: "envoy.overload_actions.stop_accepting_requests"
      triggers:
      - name: "envoy.resource_monitors.fixed_heap"
        threshold:
          value: 0.98

  admin:
    access_log_path: "/var/log/envoy_admin.log"
    address:
      socket_address:
        address: 127.0.0.1
        port_value: 9090

  static_resources:
    listeners:
    - address:
        socket_address:
          address: 0.0.0.0
          port_value: 443
      listener_filters:
      - name: "envoy.filters.listener.tls_inspector"
        typed_config: {}
      per_connection_buffer_limit_bytes: 32768 # 32 KiB
      filter_chains:
      - filter_chain_match:
          server_names: ["example.com", "www.example.com"]
        tls_context:
          common_tls_context:
            tls_certificates:
            - certificate_chain: { filename: "example_com_cert.pem" }
              private_key: { filename: "example_com_key.pem" }
        # Uncomment if Envoy is behind a load balancer that exposes client IP address using the PROXY protocol.
        # use_proxy_proto: true
        filters:
        - name: envoy.filters.network.http_connection_manager
          typed_config:
            "@type": type.googleapis.com/envoy.config.filter.network.http_connection_manager.v2.HttpConnectionManager
            stat_prefix: ingress_http
            use_remote_address: true
            normalize_path: true
            merge_slashes: true
            path_with_escaped_slashes_action: UNESCAPE_AND_REDIRECT
            common_http_protocol_options:
              idle_timeout: 3600s # 1 hour
              headers_with_underscores_action: REJECT_REQUEST
            http2_protocol_options:
              max_concurrent_streams: 100
              initial_stream_window_size: 65536 # 64 KiB
              initial_connection_window_size: 1048576 # 1 MiB
            stream_idle_timeout: 300s # 5 mins, must be disabled for long-lived and streaming requests
            request_timeout: 300s # 5 mins, must be disabled for long-lived and streaming requests
            route_config:
              virtual_hosts:
              - name: default
                domains: "*"
                routes:
                - match: { prefix: "/" }
                  route:
                    cluster: service_foo
                    idle_timeout: 15s # must be disabled for long-lived and streaming requests
    clusters:
      name: service_foo
      connect_timeout: 15s
      per_connection_buffer_limit_bytes: 32768 # 32 KiB
      hosts:
        socket_address:
          address: 127.0.0.1
          port_value: 8080
      http2_protocol_options:
        initial_stream_window_size: 65536 # 64 KiB
        initial_connection_window_size: 1048576 # 1 MiB

  layered_runtime:
    layers:
      - name: static_layer_0
        static_layer:
          envoy:
            resource_limits:
              listener:
                example_listener_name:
                  connection_limit: 10000
          overload:
            global_downstream_max_connections: 50000

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_extension_package",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "event_loop_latency_monitor_test",
    srcs = ["event_loop_latency_monitor_test.cc"],
    deps = [
        "//source/common/event:loop_latency_tracker_lib",
        "//source/extensions/resource_monitors/event_loop_latency:event_loop_latency_monitor",
        "//source/server:resource_monitor_config_lib",
        "//test/mocks/runtime:runtime_mocks",
        "//test/mocks/server:options_mocks",
        "//test/mocks/stats:stats_mocks",
        "//test/test_common:simulated_time_system_lib",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/extensions/resource_monitors/event_loop_latency/v3:pkg_cc_proto",
    ],
)

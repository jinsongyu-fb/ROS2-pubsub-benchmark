load(
  "@grpc//bazel:cc_grpc_library.bzl",
  "cc_grpc_library",
)

proto_library(
  name = "timing",
  srcs = ["timing.proto"],
)
cc_proto_library(
  name = "timing_cc",
  deps = [":timing"],
)
cc_grpc_library(
  name = "timing_cc_grpc",
  srcs = [":timing"],
  grpc_only = True,
  deps = [":timing_cc"],
)
cc_binary(
  name = "bench",
  srcs = ["bench.cpp"],
  deps = [
    "@grpc//:grpc++",
    ":timing_cc_grpc",
  ]
)
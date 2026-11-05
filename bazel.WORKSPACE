# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.

### build/kernel/kleaf/bazel.WORKSPACE contents ###
load("//build/kernel/kleaf:workspace.bzl", "define_kleaf_workspace")

define_kleaf_workspace()

# Optional epilog for analysis testing.
load("//build/kernel/kleaf:workspace_epilog.bzl", "define_kleaf_workspace_epilog")
define_kleaf_workspace_epilog()

### Qualcomm customizations ###
new_local_repository(
    name = "dtc",
    path = "external/dtc",
    build_file_content = """
cc_library(
    name = "libfdt",
    copts = [
        "-Werror",
        "-Wno-macro-redefined",
        "-Wno-sign-compare",
    ],
    includes = ["libfdt"],
    srcs = glob([
        "libfdt/*.h",
        "libfdt/*.c",
    ]),
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

copts = [
   "-Wall",
   "-Werror",
   "-Wno-sign-compare",
   "-Wno-missing-field-initializers",
   "-Wno-unused-parameter",
]

genrule(
    name = "lexer",
    srcs = ["dtc-lexer.l"],
    outs = ["dtc-lexer.lex.c"],
    cmd = "lex -o$@ $(location dtc-lexer.l)"
)

genrule(
    name = "parser",
    srcs = ["dtc-parser.y"],
    outs = ["dtc-parser.tab.c", "dtc-parser.tab.h"],
    cmd = \"\"\"
      bison -b dtc-parser -d $(location dtc-parser.y)
      cp ./*.tab.c $(location dtc-parser.tab.c)
      cp ./*.tab.h $(location dtc-parser.tab.h)
    \"\"\",
)

cc_library(
    name = "dtc_gen",
    copts = copts,
    srcs = [
        ":lexer",
        ":parser",
    ] + glob(["*.h"]),
    includes = ["."],
    deps = [":libfdt"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "dtc",
    copts = copts,
    defines = ["NO_YAML"],
    srcs = [
        "checks.c",
        "data.c",
        "dtc.c",
        "flattree.c",
        "fstree.c",
        "livetree.c",
        "srcpos.c",
        "treesource.c",
        "util.c",
    ] + glob(["*.h"]),
    includes = ["."],
    deps = [":libfdt", ":dtc_gen"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "fdtget",
    copts = copts,
    defines = ["NO_YAML"],
    srcs = [
        "fdtget.c",
        "util.c",
        "util.h",
        "version_non_gen.h",
    ],
    deps = [":libfdt"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "fdtput",
    copts = copts,
    defines = ["NO_YAML"],
    srcs = [
        "fdtput.c",
        "util.c",
        "util.h",
        "version_non_gen.h",
    ],
    deps = [":libfdt"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "fdtdump",
    copts = copts,
    defines = ["NO_YAML"],
    srcs = [
        "fdtdump.c",
        "util.c",
        "util.h",
        "version_non_gen.h",
    ],
    deps = [":libfdt"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "fdtoverlay",
    copts = copts,
    defines = ["NO_YAML"],
    srcs = [
        "fdtoverlay.c",
        "util.c",
        "util.h",
        "version_non_gen.h",
    ],
    deps = [":libfdt"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "fdtoverlaymerge",
    copts = copts,
    defines = ["NO_YAML"],
    srcs = [
        "fdtoverlaymerge.c",
        "util.c",
        "util.h",
        "version_non_gen.h",
    ],
    deps = [":libfdt"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

exports_files([
    "libfdt/fdt.h",
    "libfdt/libfdt.h",
    "libfdt/libfdt_env.h",
])
"""

)

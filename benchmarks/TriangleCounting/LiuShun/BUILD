cc_library(
  name = "two_level_tables",
  hdrs = ["two_level_tables.h"],
  deps = ["//ligra/pbbslib:sparse_table",
  ]
)

cc_library(
  name = "Triangle",
  hdrs = ["Triangle.h"],
  deps = [
  "//benchmarks/DegeneracyOrder/BarenboimElkin08:DegeneracyOrder",
  "//benchmarks/DegeneracyOrder/GoodrichPszona11:DegeneracyOrder",
  "//benchmarks/KCore/JulienneDBS17:KCore",
  "//ligra:ligra",
  "//pbbslib:sample_sort",
  "//pbbslib:monoid",
  "//ligra/pbbslib:sparse_table",
  ":two_level_tables",
  ]
)

cc_binary(
  name = "Triangle_main",
  srcs = ["Triangle.cc"],
  deps = [":Triangle"]
)

package(
  default_visibility = ["//visibility:public"],
)

NINJA_USE_BUILT=False
NINJA_COMMAND="ninja"
NINJA_DEP=[]
CMAKE_USE_BUILT=False
CMAKE_COMMAND="cmake"
CMAKE_DEP=[]

print("Please remove usage of @foreign_cc_platform_utils//:tools.bzl, as it is no longer needed.")
print("To specify the custom cmake and/or ninja tool, use the toolchains registration with rules_foreign_cc_dependencies() parameters.")

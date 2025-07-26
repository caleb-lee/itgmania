# Build and statically link lowlatencydancegamesdk project

add_subdirectory("extern/lowlatencydancegamesdk")

set_property(TARGET "lowlatencydancegamesdk" PROPERTY FOLDER "External Libraries")

# Make the headers available to ITGMania
target_include_directories("lowlatencydancegamesdk" PUBLIC "extern/lowlatencydancegamesdk/include")
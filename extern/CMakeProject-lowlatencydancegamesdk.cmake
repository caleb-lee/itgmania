# Build and statically link lowlatencydancegamesdk project

set(LOWLATENCYDANCEGAMESDK_SOURCE_DIR "${SM_EXTERN_DIR}/lowlatencydancegamesdk")

include(ExternalProject)
ExternalProject_Add(
  lowlatencydancegamesdk

  SOURCE_DIR "${LOWLATENCYDANCEGAMESDK_SOURCE_DIR}"
  INSTALL_COMMAND ""
)

ExternalProject_Get_Property(lowlatencydancegamesdk SOURCE_DIR BINARY_DIR)
set(LOWLATENCYDANCEGAMESDK_INCLUDE_DIR "${SOURCE_DIR}/include" CACHE INTERNAL "lowlatencydancegamesdk include")
if(WIN32)
  set(LOWLATENCYDANCEGAMESDK_LIBRARY "${BINARY_DIR}/lowlatencydancegamesdk.lib" CACHE INTERNAL "lowlatencydancegamesdk library")
else()
  set(LOWLATENCYDANCEGAMESDK_LIBRARY "${BINARY_DIR}/liblowlatencydancegamesdk.a" CACHE INTERNAL "lowlatencydancegamesdk library")
endif()

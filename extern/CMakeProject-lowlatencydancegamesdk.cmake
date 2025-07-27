set(INSTALL_DIR ${CMAKE_BINARY_DIR}/lowlatencydancegamesdk-install)
set(INCLUDE_DIR ${INSTALL_DIR}/include)
set(LIB_DIR ${INSTALL_DIR}/lib)

if(WIN32)
  set(LIB_PATH ${LIB_DIR}/lowlatencydancegamesdk.lib)
  set(LIBUSB_PATH ${LIB_DIR}/libusb-1.0.lib)
else()
  set(LIB_PATH ${LIB_DIR}/liblowlatencydancegamesdk.a)
  set(LIBUSB_PATH ${LIB_DIR}/libusb-1.0.a)
endif()

if(APPLE)
  LIST(APPEND ARCH_FLAGS
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
  )
elseif(WIN32)
  LIST(APPEND ARCH_FLAGS
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>
    -DCMAKE_C_FLAGS="/MT$<$<CONFIG:Debug>:d>"
    -DCMAKE_CXX_FLAGS="/MT$<$<CONFIG:Debug>:d>"
  )
endif()

include(ExternalProject)
ExternalProject_Add(
  lowlatencydancegamesdk_project

  SOURCE_DIR ${SM_EXTERN_DIR}/lowlatencydancegamesdk
  INSTALL_DIR ${INSTALL_DIR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
             -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
             ${ARCH_FLAGS}
  BUILD_IN_SOURCE OFF
  CONFIGURE_HANDLED_BY_BUILD ON
  BUILD_BYPRODUCTS "${LIB_PATH}"  # Needed for Ninja generator. See BUILD_BYPRODUCTS docs for details.
)

# This needs to be created immediately, otherwise top-level project generation will fail.
# It will be populated by the install step of the external project.
file(MAKE_DIRECTORY ${INCLUDE_DIR})

add_library(lowlatencydancegamesdk STATIC IMPORTED GLOBAL)
add_dependencies(lowlatencydancegamesdk lowlatencydancegamesdk_project)
target_include_directories(lowlatencydancegamesdk INTERFACE "${INCLUDE_DIR}")
set_property(TARGET lowlatencydancegamesdk PROPERTY IMPORTED_LOCATION "${LIB_PATH}")
if(APPLE)
  set_property(TARGET lowlatencydancegamesdk PROPERTY INTERFACE_LINK_LIBRARIES 
    "${LIBUSB_PATH};-framework Security;-framework CoreFoundation;-framework IOKit")
elseif(WIN32)
  set_property(TARGET lowlatencydancegamesdk PROPERTY INTERFACE_LINK_LIBRARIES 
    "${LIBUSB_PATH}" windowsapp)
else()
  set_property(TARGET lowlatencydancegamesdk PROPERTY INTERFACE_LINK_LIBRARIES "${LIBUSB_PATH}")
endif()
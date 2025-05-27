project(libsmx)

add_library(smx STATIC
    src/SMX.cpp
    src/Helpers.cpp
    src/SMXConfigPacket.cpp
    src/SMXDevice.cpp
    src/SMXDeviceConnection.cpp
    src/SMXDeviceSearch.cpp
    src/SMXDeviceSearchThreaded.cpp
    src/SMXGif.cpp
    src/SMXHelperThread.cpp
    src/SMXManager.cpp
    src/SMXPanelAnimation.cpp
    src/SMXPanelAnimationUpload.cpp
    src/SMXThread.cpp
)

target_include_directories(smx
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
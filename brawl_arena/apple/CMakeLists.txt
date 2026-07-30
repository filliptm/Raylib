cmake_minimum_required(VERSION 3.25)

project(BrawlArenaIOS LANGUAGES C OBJC OBJCXX)

if(NOT IOS)
    message(FATAL_ERROR "BrawlArenaIOS must be configured with -DCMAKE_SYSTEM_NAME=iOS")
endif()

set(BRAWL_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(RAYLIB_IOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/raylib-ios"
    CACHE PATH "Pinned raylib-iOS checkout")
set(BRAWL_DEVELOPMENT_TEAM "" CACHE STRING "Apple development team used for device signing")

foreach(required_path
        "${RAYLIB_IOS_ROOT}/src/rcore.c"
        "${RAYLIB_IOS_ROOT}/src/platforms/rcore_ios.c"
        "${RAYLIB_IOS_ROOT}/projects/Xcode26/ANGLE/libEGL.xcframework"
        "${RAYLIB_IOS_ROOT}/projects/Xcode26/ANGLE/libGLESv2.xcframework"
        "${BRAWL_ROOT}/build/assets/characters"
        "${BRAWL_ROOT}/build/assets/vfx")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR
            "Missing ${required_path}. Run 'make -C brawl_arena ios-bootstrap' first.")
    endif()
endforeach()

file(GLOB_RECURSE BRAWL_SOURCES CONFIGURE_DEPENDS
    "${BRAWL_ROOT}/src/*.c")

set(RAYLIB_SOURCES
    "${RAYLIB_IOS_ROOT}/src/rcore.c"
    "${RAYLIB_IOS_ROOT}/src/raudio.c"
    "${RAYLIB_IOS_ROOT}/src/rmodels.c"
    "${RAYLIB_IOS_ROOT}/src/rshapes.c"
    "${RAYLIB_IOS_ROOT}/src/rtext.c"
    "${RAYLIB_IOS_ROOT}/src/rtextures.c"
    "${RAYLIB_IOS_ROOT}/src/utils.c")

# UIKit and AVFoundation code is compiled inside these otherwise-C raylib modules.
set_source_files_properties(
    "${RAYLIB_IOS_ROOT}/src/rcore.c"
    "${RAYLIB_IOS_ROOT}/src/raudio.c"
    PROPERTIES LANGUAGE OBJC)

add_executable(BrawlArena MACOSX_BUNDLE
    ${BRAWL_SOURCES}
    ${RAYLIB_SOURCES}
    "${CMAKE_CURRENT_LIST_DIR}/BrawlIOSBridge.mm")

target_compile_features(BrawlArena PRIVATE c_std_99)
target_compile_options(BrawlArena PRIVATE
    "-include" "libGLESv2/GLES2/gl2.h"
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra>"
    "$<$<COMPILE_LANGUAGE:OBJC>:-Wall;-Wextra>")
target_compile_definitions(BrawlArena PRIVATE
    PLATFORM_IOS
    GRAPHICS_API_OPENGL_ES3
    GL_GLEXT_PROTOTYPES
    BRAWL_MOBILE)

target_include_directories(BrawlArena PRIVATE
    "${BRAWL_ROOT}/src/core"
    "${BRAWL_ROOT}/src/content"
    "${BRAWL_ROOT}/src/game"
    "${BRAWL_ROOT}/src/presentation"
    "${BRAWL_ROOT}/src/ui"
    "${BRAWL_ROOT}/src/devtools"
    "${BRAWL_ROOT}/src/app"
    "${CMAKE_CURRENT_LIST_DIR}"
    "${RAYLIB_IOS_ROOT}/src")

set(ANGLE_EGL
    "${RAYLIB_IOS_ROOT}/projects/Xcode26/ANGLE/libEGL.xcframework")
set(ANGLE_GLES
    "${RAYLIB_IOS_ROOT}/projects/Xcode26/ANGLE/libGLESv2.xcframework")

target_link_libraries(BrawlArena PRIVATE
    "${ANGLE_EGL}"
    "${ANGLE_GLES}"
    "-framework UIKit"
    "-framework Foundation"
    "-framework QuartzCore"
    "-framework CoreGraphics"
    "-framework AVFoundation"
    "-framework AVFAudio"
    "-framework AudioToolbox"
    "-framework CoreAudio")

set_target_properties(BrawlArena PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "Brawl Arena"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.filliptm.brawlarena"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.filliptm.brawlarena"
    XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "iphoneos iphonesimulator"
    XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST "NO"
    XCODE_ATTRIBUTE_SUPPORTS_XR_DESIGNED_FOR_IPHONE_IPAD "NO"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
    XCODE_EMBED_FRAMEWORKS "${ANGLE_EGL};${ANGLE_GLES}"
    XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY "YES"
    XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY "YES")

if(BRAWL_DEVELOPMENT_TEAM)
    set_target_properties(BrawlArena PROPERTIES
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${BRAWL_DEVELOPMENT_TEAM}")
endif()

function(add_brawl_resources root directory)
    file(GLOB_RECURSE resource_files CONFIGURE_DEPENDS
        "${root}/${directory}/*")
    foreach(resource IN LISTS resource_files)
        if(IS_DIRECTORY "${resource}")
            continue()
        endif()
        file(RELATIVE_PATH relative "${root}" "${resource}")
        get_filename_component(relative_directory "${relative}" DIRECTORY)
        set_source_files_properties("${resource}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/BrawlAssets/${relative_directory}")
        target_sources(BrawlArena PRIVATE "${resource}")
    endforeach()
endfunction()

add_brawl_resources("${BRAWL_ROOT}" "config")
add_brawl_resources("${BRAWL_ROOT}" "data")
add_brawl_resources("${BRAWL_ROOT}" "resources")
add_brawl_resources("${BRAWL_ROOT}" "build/assets")

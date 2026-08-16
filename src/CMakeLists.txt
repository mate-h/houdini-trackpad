cmake_minimum_required(VERSION 3.6)
project(TrackpadHook VERSION 0.1.0)

list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
set( library_name TrackpadHook )
# Add a library and its source files.
add_library( ${library_name} SHARED 
    multitouch/multitouch-device.h
    multitouch/multitouch-device.c
    trackpad-hook.cc
)

find_package( Houdini REQUIRED )
# Houdini 22 headers require this; hcustom sets it, but HoudiniConfig.cmake does not.
target_compile_definitions( ${library_name} PRIVATE USE_VULKAN=1 )
target_link_libraries( ${library_name}
    Houdini
)

# Link against Apple frameworks
find_library( LIBMT MultitouchSupport /System/Library/PrivateFrameworks )
message(STATUS "Found library: ${LIBMT}" )
target_link_libraries( ${library_name}
    ${LIBMT}
)

# Network-editor mousewheel consumer. Install into the user prefs tree, not $HFS
# (Houdini 22+ uses python3.13libs and the app bundle is not writable).
if(DEFINED ENV{HOUDINI_USER_PREF_DIR})
    set(_houdini_user_python "$ENV{HOUDINI_USER_PREF_DIR}/python3.13libs")
else()
    set(_houdini_user_python "$ENV{HOME}/Library/Preferences/houdini/22.0/python3.13libs")
endif()
file(MAKE_DIRECTORY "${_houdini_user_python}")
configure_file(scripts/nodegraphhooks.py "${_houdini_user_python}/nodegraphhooks.py" COPYONLY)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# Include ${CMAKE_CURRENT_BINARY_DIR} for the generated header.
target_include_directories( ${library_name} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Sets several common target properties, such as the library's output directory.
houdini_configure_target( ${library_name} )

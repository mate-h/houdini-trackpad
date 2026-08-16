cmake_minimum_required(VERSION 3.6)
project(TrackpadHook VERSION 0.1.0)

# Prefer an existing $HFS; otherwise use the Current Houdini app on macOS.
if(NOT DEFINED ENV{HFS} OR NOT EXISTS "$ENV{HFS}/toolkit/cmake/HoudiniConfig.cmake")
    set(_houdini_current
        "/Applications/Houdini/Current/Frameworks/Houdini.framework/Versions/Current/Resources"
    )
    if(EXISTS "${_houdini_current}/toolkit/cmake/HoudiniConfig.cmake")
        set(ENV{HFS} "${_houdini_current}")
    endif()
endif()
if(NOT EXISTS "$ENV{HFS}/toolkit/cmake/HoudiniConfig.cmake")
    message(FATAL_ERROR
        "Could not find Houdini. Install Houdini, or set HFS to its Resources directory."
    )
endif()
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

# Network-editor mousewheel consumer. Install into the user prefs tree, not $HFS.
if(DEFINED ENV{HOUDINI_USER_PREF_DIR})
    set(_houdini_user_pref "$ENV{HOUDINI_USER_PREF_DIR}")
else()
    set(_houdini_user_pref "$ENV{HOME}/Library/Preferences/houdini/${Houdini_VERSION_MAJOR}.${Houdini_VERSION_MINOR}")
endif()
file(GLOB _hfs_python_libs RELATIVE "$ENV{HFS}/houdini" "$ENV{HFS}/houdini/python3.*libs")
if(_hfs_python_libs)
    list(SORT _hfs_python_libs)
    list(GET _hfs_python_libs -1 _houdini_python_libdir)
    set(_houdini_user_python "${_houdini_user_pref}/${_houdini_python_libdir}")
    file(MAKE_DIRECTORY "${_houdini_user_python}")
    configure_file(scripts/nodegraphhooks.py "${_houdini_user_python}/nodegraphhooks.py" COPYONLY)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# Include ${CMAKE_CURRENT_BINARY_DIR} for the generated header.
target_include_directories( ${library_name} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Sets several common target properties, such as the library's output directory.
houdini_configure_target( ${library_name} )

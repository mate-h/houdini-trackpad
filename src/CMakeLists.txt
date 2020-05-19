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
target_link_libraries( ${library_name}
    Houdini
)

# Link against Apple frameworks
find_library( LIBMT MultitouchSupport /System/Library/PrivateFrameworks )
message(STATUS "Found library: ${LIBMT}" )
target_link_libraries( ${library_name}
    ${LIBMT}
)

configure_file(scripts/nodegraphhooks.py "$ENV{HFS}/houdini/python2.7libs" COPYONLY)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# Include ${CMAKE_CURRENT_BINARY_DIR} for the generated header.
target_include_directories( ${library_name} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Sets several common target properties, such as the library's output directory.
houdini_configure_target( ${library_name} )

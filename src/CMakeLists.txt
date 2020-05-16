cmake_minimum_required(VERSION 3.6)
project(TrackpadHook VERSION 0.1.0)

list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
set( library_name TrackpadHook )
# Add a library and its source files.
add_library( ${library_name} SHARED 
    trackpad/trackpad.h
    trackpad/trackpad.c
    trackpadhook.cpp
)

find_package( Houdini REQUIRED )
target_link_libraries( ${library_name}
    Houdini
)

# Link MultitouchSupport Apple framework
find_library( LIBS MultitouchSupport /System/Library/PrivateFrameworks )
message(STATUS "Found library: ${LIBS}" )
target_link_libraries( ${library_name}
    ${LIBS}
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# Include ${CMAKE_CURRENT_BINARY_DIR} for the generated header.
target_include_directories( ${library_name} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Sets several common target properties, such as the library's output directory.
houdini_configure_target( ${library_name} )

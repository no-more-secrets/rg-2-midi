cmake_minimum_required( VERSION 3.12...3.12 )

if( ${CMAKE_VERSION} VERSION_LESS 3.12 )
    cmake_policy(
      VERSION
      ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} )
endif()

project(
    rg-to-midi
    VERSION 0.1.0
    DESCRIPTION "Rosegarden to MIDI Exporter"
    LANGUAGES CXX
)

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/" )

include( cmake/generic.cmake )

add_subdirectory( src )

# This should not be necessary, but some subdirectories' cmake
# files are not playing nicely and just setting this to 11.
#set( CMAKE_CXX_STANDARD 17 )

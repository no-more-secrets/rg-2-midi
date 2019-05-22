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

# === ccache ======================================================

find_program( CCACHE_PROGRAM ccache )
if( CCACHE_PROGRAM )
    set( CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" )
else()
    message( STATUS "ccache not found." )
endif()

# === build type ==================================================

set( default_build_type "Debug" )
if( NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES )
    message( STATUS "Setting build type to '${default_build_type}'." )
    set( CMAKE_BUILD_TYPE "${default_build_type}" CACHE
         STRING "Choose the type of build." FORCE )
    # Set the possible values of build type for cmake-gui
    set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 "Debug" "Release" "RelWithDebInfo")
endif()
message( STATUS "Build type: ${CMAKE_BUILD_TYPE}" )

# === YCM =========================================================

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# === Subdirectories ==============================================

add_subdirectory( src )

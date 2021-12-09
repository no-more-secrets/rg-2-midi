find_package(
  Qt5 "5.1.0"
  REQUIRED
  COMPONENTS
  Core
  Xml
)

file( GLOB sources "[a-zA-Z]*.cpp" )
add_executable( rg2midi ${sources} )

target_compile_options(
  rg2midi PRIVATE
  # clang/GCC warnings
  $<$<CXX_COMPILER_ID:Clang>:
    -Wall
    -Wextra
    -Wno-unknown-warning-option
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-private-field
    -Wno-unused-but-set-variable
    -Wno-unused-but-set-parameter
    -Wno-delete-non-virtual-dtor
    -Wno-reorder
    -Wno-unused-function >
  $<$<CXX_COMPILER_ID:GNU>:
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-but-set-parameter
    -Wno-delete-non-virtual-dtor
    -Wno-reorder
    -Wno-stringop-truncation
    -Wno-unused-function >
)

target_link_libraries( rg2midi PRIVATE z Qt5::Core Qt5::Xml )

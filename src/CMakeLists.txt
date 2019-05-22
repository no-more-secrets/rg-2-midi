SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC" )
file( GLOB sources "[a-zA-Z]*.cpp" )
add_executable( rg2midi ${sources} )

#target_compile_features( rg2midi PUBLIC cxx_std_20 )
set_target_properties(
  rg2midi
  PROPERTIES
  CXX_EXTENSIONS OFF
)
#set_warning_options( rg2midi )

target_link_libraries( rg2midi PRIVATE z Qt5Core Qt5Xml )
target_include_directories(
  rg2midi
  PRIVATE
  /usr/include/x86_64-linux-gnu/qt5
  /usr/include/x86_64-linux-gnu/qt5/QtCore
  /usr/include/x86_64-linux-gnu/qt5/QtXml
)
target_compile_definitions( rg2midi PUBLIC emit )

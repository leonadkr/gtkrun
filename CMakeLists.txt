cmake_minimum_required( VERSION 3.16 )

project( gtkrun VERSION 1.0.0 LANGUAGES C )

set( PROGRAM_NAME ${CMAKE_PROJECT_NAME} )
set( PROGRAM_VERSION ${CMAKE_PROJECT_VERSION} )
configure_file( config.h.in config.h )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

add_compile_options( "-Wall" "-pedantic" )

add_subdirectory( man )
add_subdirectory( shared )
add_subdirectory( gui )
add_subdirectory( main )

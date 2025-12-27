cmake_minimum_required( VERSION 4.1 )

project( gtkrun VERSION 1.2.0 )

set( PROGRAM_NAME ${PROJECT_NAME} )
set( PROGRAM_VERSION ${PROJECT_VERSION} )
set( PROGRAM_APP_ID "com.github.leonadkr.gtkrun" )
string( TIMESTAMP PROGRAM_TIMESTAMP "%Y-%m-%d" )
set( PROGRAM_AUTHOR "leonadkr@gmail.com" )

set( PROGRAM_ENVIRONMENT_PATH "PATH" )
set( PROGRAM_CONFIGURE_FILE "config" )
set( PROGRAM_HISTORY_FILE "history" )

if( CMAKE_HOST_WIN32 )
	set( PROGRAM_LINE_BREAKER "\\r\\n" )
else()
	set( PROGRAM_LINE_BREAKER "\\n" )
endif()

if( NOT DEFINED MAIN_WINDOW_WIDTH )
	set( MAIN_WINDOW_WIDTH 400 )
endif()

if( NOT DEFINED MAIN_WINDOW_HEIGHT )
	set( MAIN_WINDOW_HEIGHT 200 )
endif()

set( MAIN_WINDOW_MAX_HEIGHT ${MAIN_WINDOW_HEIGHT} )

configure_file( config.h.in config.h )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

add_subdirectory( src )
add_subdirectory( man )

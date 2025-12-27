cmake_minimum_required( VERSION 4.1 )

project( man LANGUAGES C )

include( GNUInstallDirs )

find_program( BZIP2 NAMES bzip2 REQUIRED )

configure_file( gtkrun.1.in gtkrun.1 )

add_custom_target( man ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gtkrun.1.bz2
)

add_custom_command( OUTPUT gtkrun.1.bz2
	COMMAND ${BZIP2} --quiet --force ${CMAKE_CURRENT_BINARY_DIR}/gtkrun.1
	VERBATIM
	MAIN_DEPENDENCY	${CMAKE_CURRENT_BINARY_DIR}/gtkrun.1
)

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/gtkrun.1.bz2
	TYPE MAN
)

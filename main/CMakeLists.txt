project( main LANGUAGES C )

find_package( PkgConfig REQUIRED )
pkg_check_modules( GLIB2 REQUIRED glib-2.0 )

add_executable( ${PROJECT_NAME}
	main.c )

set_target_properties( ${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME ${PROGRAM_NAME}
	C_STANDARD 11
	C_STANDARD_REQUIRED TRUE )

target_include_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_INCLUDE_DIRS} )

target_link_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARY_DIRS} )

target_link_libraries( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARIES}
	path
	gui )

install( TARGETS ${PROJECT_NAME}
	RUNTIME )

project( gui LANGUAGES C )

find_package( PkgConfig REQUIRED )
pkg_check_modules( GLIB2 REQUIRED glib-2.0 )
pkg_check_modules( GTK4 REQUIRED gtk4 )

add_library( ${PROJECT_NAME}
	STATIC
	entry.c
	treeview.c
	window.c
	gui.c )

set_target_properties( ${PROJECT_NAME} PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED TRUE )

target_include_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_INCLUDE_DIRS}
	${GTK4_INCLUDE_DIRS}
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} )

target_link_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARY_DIRS}
	${GTK4_LIBRARY_DIRS} )

target_link_libraries( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARIES}
	${GTK4_LIBRARIES}
	path )

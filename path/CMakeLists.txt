project( path LANGUAGES C )

find_package( PkgConfig REQUIRED )
pkg_check_modules( GLIB2 REQUIRED glib-2.0 )
pkg_check_modules( GOBJECT2 REQUIRED gobject-2.0 )
pkg_check_modules( GIO2 REQUIRED gio-2.0 )

add_library( ${PROJECT_NAME}
	STATIC
	path.c )

set_target_properties( ${PROJECT_NAME} PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED TRUE )

target_include_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_INCLUDE_DIRS}
	${GOBJECT2_INCLUDE_DIRS}
	${GIO2_INCLUDE_DIRS}
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} )

target_link_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARY_DIRS}
	${GOBJECT2_LIBRARY_DIRS}
	${GIO2_LIBRARY_DIRS} )

target_link_libraries( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARIES}
	${GOBJECT2_LIBRARIES}
	${GIO2_LIBRARIES} )

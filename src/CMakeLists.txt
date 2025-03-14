cmake_minimum_required( VERSION 3.16 )

project( ${PROGRAM_NAME} LANGUAGES C )

find_package( PkgConfig REQUIRED )
pkg_check_modules( GOBJECT2 REQUIRED gobject-2.0 )
pkg_check_modules( GLIB2 REQUIRED glib-2.0 )
pkg_check_modules( GIO2 REQUIRED gio-2.0 )
pkg_check_modules( GTK4 REQUIRED gtk4 )

add_compile_options( "-Wall" "-pedantic" )

add_executable( ${CMAKE_PROJECT_NAME}
	grcommandlist.c
	grentry.c
	grlist.c
	grwindow.c
	grapplication.c
	main.c )

install( TARGETS ${CMAKE_PROJECT_NAME}
	RUNTIME )

target_include_directories( ${CMAKE_PROJECT_NAME}
	PRIVATE
	${GOBJECT2_INCLUDE_DIRS}
	${GLIB2_INCLUDE_DIRS}
	${GIO2_INCLUDE_DIRS}
	${GTK4_INCLUDE_DIRS} )

target_link_directories( ${CMAKE_PROJECT_NAME}
	PRIVATE
	${GOBJECT2_LIBRARY_DIRS}
	${GLIB2_LIBRARY_DIRS}
	${GIO2_LIBRARY_DIRS}
	${GTK$_LIBRARY_DIRS} )

target_link_libraries( ${CMAKE_PROJECT_NAME}
	PRIVATE
	${GOBJECT2_LIBRARIES}
	${GLIB2_LIBRARIES}
	${GIO2_LIBRARIES}
	${GTK4_LIBRARIES} )

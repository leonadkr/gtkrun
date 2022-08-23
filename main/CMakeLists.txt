project( main LANGUAGES C )

add_executable( ${PROJECT_NAME}
	main.c )

set_target_properties( ${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME ${PROGRAM_NAME}
	C_STANDARD 11
	C_STANDARD_REQUIRED TRUE )

target_link_libraries( ${PROJECT_NAME}
	PRIVATE
	gui )

install( TARGETS ${PROJECT_NAME}
	RUNTIME )

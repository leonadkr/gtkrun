#include "config.h"
#include "grapplication.h"

#include <locale.h>

int
main(
	int argc,
	char *argv[] )
{
	GrApplication *app;
	gint ret = EXIT_SUCCESS;

	setlocale( LC_ALL, "" );

	app = gr_application_new( PROGRAM_APP_ID );
	if( app == NULL )
		return EXIT_FAILURE;

	ret = g_application_run( G_APPLICATION( app ), argc, argv );
	g_object_unref( G_OBJECT( app ) );

	return ret;
}


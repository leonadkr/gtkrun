#include <stdlib.h>
#include <glib.h>
#include "config.h"
#include "path.h"
#include "gui.h"

int
main(
	gint argc,
	gchar *argv[] )
{
	gchar *cache_filepath;
	GrPath *path;
	gint ret = EXIT_FAILURE;

	/* init shared */
	cache_filepath = g_build_filename( g_get_user_cache_dir(), PROGRAM_NAME, CACHE_FILENAME, NULL );
	path = gr_path_new( cache_filepath, PATHENV );
	g_free( cache_filepath );

	ret = gui_start( argc, argv, path );

	/*release shared */
	g_object_unref( G_OBJECT( path ) );

	return ret;
}


#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <locale.h>
#include "config.h"
#include "shared.h"
#include "window.h"


#define PROGRAM_SUMMARY \
"This program does something\
\n\
For example"


static gboolean
parse_config(
	gchar *config_filepath,
	GrShared *shared )
{
	gboolean silent, no_cache;
	gint width, height, max_height;
	gchar *cache_filepath;
	GKeyFile *key_file;
	gboolean ret = TRUE;
	GError *error = NULL;

	g_return_val_if_fail( config_filepath != NULL, FALSE );
	g_return_val_if_fail( shared != NULL, FALSE );

	/* load config */
	key_file = g_key_file_new();
	if( !g_key_file_load_from_file( key_file, config_filepath, G_KEY_FILE_NONE, NULL ) )
	{
		ret = FALSE;
		goto out;
	}
		
	/* get options */
	silent = g_key_file_get_boolean( key_file, "Main", "silent", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		shared->silent = silent;

	width = g_key_file_get_integer( key_file, "Main", "width", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		shared->width = width;

	height = g_key_file_get_integer( key_file, "Main", "height", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		shared->height = height;

	max_height = g_key_file_get_integer( key_file, "Main", "max-height", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
	{
		shared->max_height = max_height;
		shared->max_height_set = TRUE;
	}

	no_cache = g_key_file_get_boolean( key_file, "Main", "no-cache", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		shared->no_cache = no_cache;

	cache_filepath = g_key_file_get_string( key_file, "Main", "cache", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
	{
		g_free( shared->cache_filepath );
		shared->cache_filepath = cache_filepath;
	}

out:
	g_key_file_free( key_file );
	
	return ret;
}

static GList*
prepend_filename_list(
	GList *filename_list,
	gchar *dirname )
{
	GFile *dir;
	GFileEnumerator *direnum;
	GFileInfo *fileinfo;
	GList *fn_list, *ret;
	GError *error = NULL;

	/* nothing to do */
	if( dirname == NULL )
		return NULL;

	dir = g_file_new_for_path( dirname );
	direnum = g_file_enumerate_children(
		dir,
		G_FILE_ATTRIBUTE_STANDARD_NAME,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		ret = filename_list;
		goto out1;
	}

	fn_list = NULL;
	while( TRUE )
	{
		g_file_enumerator_iterate( direnum, &fileinfo, NULL, NULL, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			g_list_free_full( fn_list, (GDestroyNotify)g_free );
			ret = filename_list;
			goto out2;
		}

		if( fileinfo == NULL )
			break;

		fn_list = g_list_prepend( fn_list, g_strdup( g_file_info_get_name( fileinfo ) ) );
	}
	ret = g_list_concat( fn_list, filename_list );

out2:
	g_object_unref( G_OBJECT( direnum ) );

out1:
	g_object_unref( G_OBJECT( dir ) );

	return ret;
}

static GList*
get_filename_list_from_env(
	const gchar *pathenv )
{
	const gchar delim[] = ":";

	const gchar *pathstr;
	gchar **patharr, **arr;
	GList *filename_list;

	/* nothing to do */
	if( pathenv == NULL )
		return NULL;

	pathstr = g_getenv( pathenv );
	patharr = g_strsplit( pathstr, delim, -1 );

	filename_list = NULL;
	for( arr = patharr; arr[0] != NULL; arr++ )
		filename_list = prepend_filename_list( filename_list, arr[0] );
	g_strfreev( patharr );

	filename_list = g_list_sort( filename_list, (GCompareFunc)g_strcmp0 );

	return filename_list;
}

static GList*
get_filename_list_from_cache(
	gchar *cache_filepath )
{
	GFile *file;
	GFileInputStream *file_stream;
	GDataInputStream *data_stream;
	GList *filename_list;
	gsize size;
	gchar *line;
	GList *ret = NULL;
	GError *error = NULL;

	/* nothing to do */
	if( cache_filepath == NULL )
		return NULL;

	file = g_file_new_for_path( cache_filepath );
	file_stream = g_file_read( file, NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		ret = NULL;
		goto out1;
	}
	data_stream = g_data_input_stream_new( G_INPUT_STREAM( file_stream ) );

	filename_list = NULL;
	while( TRUE )
	{
		line = g_data_input_stream_read_line( data_stream, &size, NULL, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			g_list_free_full( filename_list, (GDestroyNotify)g_free );
			ret = NULL;
			goto out2;
		}

		if( line == NULL )
			break;

		filename_list = g_list_prepend( filename_list, line );
	}
	ret = g_list_reverse( filename_list );

out2:
	g_object_unref( G_OBJECT( data_stream ) );
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );

	return ret;
}

static void
on_app_startup(
	GApplication *self,
	gpointer user_data )
{
	g_return_if_fail( G_IS_APPLICATION( self ) );

	g_application_set_resource_base_path( self, NULL );
}

static void
on_app_activate(
	GApplication *self,
	gpointer user_data )
{
	GrShared *shared = (GrShared*)user_data;
	GtkWindow *window;

	g_return_if_fail( G_IS_APPLICATION( self ) );

	window = gr_window_new( self, shared );
	gtk_widget_show( GTK_WIDGET( window ) );
}

static gint
on_app_handle_local_options(
	GApplication *self,
	GVariantDict *options,
	gpointer user_data )
{
	GrShared *shared = (GrShared*)user_data;
	gchar *config_filepath, *cache_filepath;

	/* set up options */
	g_variant_dict_lookup( options, "no-config", "b", &( shared->no_config ) );

	if( g_variant_dict_lookup( options, "config", "s", &config_filepath ) )
	{
		g_free( shared->config_filepath );
		shared->config_filepath = config_filepath;
	}
	if( !shared->no_config )
		parse_config( shared->config_filepath, shared );

	g_variant_dict_lookup( options, "no-cache", "b", &( shared->no_cache ) );

	if( g_variant_dict_lookup( options, "cache", "s", &cache_filepath ) )
	{
		g_free( shared->cache_filepath );
		shared->cache_filepath = cache_filepath;
	}

	g_variant_dict_lookup( options, "silent", "b", &( shared->silent ) );
	g_variant_dict_lookup( options, "width", "i", &( shared->width ) );
	g_variant_dict_lookup( options, "height", "i", &( shared->height ) );

	if( g_variant_dict_lookup( options, "max-height", "i", &( shared->max_height ) ) )
		shared->max_height_set = TRUE;

	/* set up shared data */
	shared->env_filename_list = get_filename_list_from_env( PATHENV );
	shared->cache_filename_list = get_filename_list_from_cache( shared->cache_filepath );

	return -1;
}

gint
main(
	gint argc,
	gchar *argv[] )
{
	GApplication *app;
	GrShared *shared;
	gint ret;
	const GOptionEntry entries[]=
	{
		{ "silent", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not show output", NULL },
		{ "width", 'w', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, NULL, "Window width", "WIDTH" },
		{ "height", 'h', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, NULL, "Window height", "HEIGHT" },
		{ "max-height", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, NULL, "Window maximum height", "MAX_HEIGHT" },
		{ "cache", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, NULL, "Path to cache file", "CACHE_FILEPATH" },
		{ "no-cache", 'A', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use cache file", NULL },
		{ "config", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, NULL, "Path to configure file", "CACHE_FILEPATH" },
		{ "no-config", 'O', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use configure file", NULL },
		{ NULL }
	};

	g_return_val_if_fail( g_application_id_is_valid( APP_ID ), EXIT_FAILURE );

	app = G_APPLICATION( gtk_application_new( APP_ID, G_APPLICATION_FLAGS_NONE ) );
	if( app == NULL )
		return EXIT_FAILURE;

	/* set default options */
	shared = gr_shared_new();
	shared->silent = FALSE;
	shared->width = DEFAULT_WINDOW_WIDTH;
	shared->height = DEFAULT_WINDOW_HEIGHT;
	shared->max_height = DEFAULT_WINDOW_MAX_HEIGHT;
	shared->cache_filepath = g_build_filename( g_get_user_cache_dir(), PROGRAM_NAME, CACHE_FILENAME, NULL );
	shared->no_cache = FALSE;
	shared->config_filepath = g_build_filename( g_get_user_config_dir(), PROGRAM_NAME, CONFIG_FILENAME, NULL );
	shared->no_config = FALSE;
	shared->max_height_set = FALSE;
	shared->cache_filename_list = NULL;
	shared->env_filename_list = NULL;

	g_application_add_main_option_entries( app, entries );
	g_application_set_option_context_summary( app, PROGRAM_SUMMARY );
	g_object_set_data_full( G_OBJECT( app ), "shared", shared, (GDestroyNotify)gr_shared_free );

	g_signal_connect( G_OBJECT( app ), "startup", G_CALLBACK( on_app_startup ), NULL );
	g_signal_connect( G_OBJECT( app ), "activate", G_CALLBACK( on_app_activate ), shared );
	g_signal_connect( G_OBJECT( app ), "handle-local-options", G_CALLBACK( on_app_handle_local_options ), shared );

	ret = g_application_run( app, argc, argv );

	g_object_unref( G_OBJECT( app ) );

	return ret;
}


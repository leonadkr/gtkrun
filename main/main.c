#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <locale.h>
#include "config.h"
#include "shared.h"
#include "window.h"

#define PROGRAM_SUMMARY "This program launches applications in a graphical environment supported by GTK4"

static void
shared_set_default(
	GrShared *self )
{
	g_return_if_fail( self != NULL );

	self->silent = FALSE;
	self->width = DEFAULT_WINDOW_WIDTH;
	self->height = DEFAULT_WINDOW_HEIGHT;
	self->max_height = DEFAULT_WINDOW_MAX_HEIGHT;
	self->max_height_set = FALSE;
	g_free( self->cache_dir );
	self->cache_dir = g_build_filename( g_get_user_cache_dir(), PROGRAM_NAME, NULL );
	self->no_cache = FALSE;
	g_free( self->config_path );
	self->config_path = g_build_filename( g_get_user_config_dir(), PROGRAM_NAME, CONFIG_FILENAME, NULL );
	self->no_config = FALSE;
	g_free( self->env );
	self->env = g_strdup( ENVPATH );
	self->conceal = FALSE;
	self->window = NULL;
}

static gboolean
parse_config(
	gchar *config_filepath,
	GrShared *shared )
{
	gboolean silent, no_cache, conceal;
	gint width, height, max_height;
	gchar *cache_dir;
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
	{
		shared->height = height;
		shared->max_height_set = FALSE;
	}

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

	cache_dir = g_key_file_get_string( key_file, "Main", "cache-dir", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
	{
		g_free( shared->cache_dir );
		shared->cache_dir = cache_dir;
	}

	conceal = g_key_file_get_boolean( key_file, "Main", "conceal", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		shared->conceal = conceal;

out:
	g_key_file_free( key_file );
	
	return ret;
}

static void
on_app_startup(
	GApplication *self,
	gpointer user_data )
{
	GrShared *shared = (GrShared*)user_data;

	g_return_if_fail( G_IS_APPLICATION( self ) );

	g_application_hold( self );

	g_application_set_resource_base_path( self, NULL );

	gr_shared_setup_private( shared );
	shared->app = self;
	shared->window = gr_window_new( shared );

	g_application_release( self );
}

static void
on_app_activate(
	GApplication *self,
	gpointer user_data )
{
	GrShared *shared = (GrShared*)user_data;

	g_return_if_fail( G_IS_APPLICATION( self ) );

	g_application_hold( self );
	gr_window_reset( shared->window, shared );
	gtk_widget_set_visible( GTK_WIDGET( shared->window ), TRUE );
	g_application_release( self );
}

static gint
on_app_handle_local_options(
	GApplication *self,
	GVariantDict *options,
	gpointer user_data )
{
	GrShared *shared = (GrShared*)user_data;
	gchar *config_path, *cache_dir;

	g_variant_dict_lookup( options, "no-config", "b", &( shared->no_config ) );

	/* scan config file, if it is set */
	if( g_variant_dict_lookup( options, "config", "s", &config_path ) )
	{
		g_free( shared->config_path );
		shared->config_path = config_path;
	}
	if( !shared->no_config )
		parse_config( shared->config_path, shared );

	g_variant_dict_lookup( options, "no-cache", "b", &( shared->no_cache ) );

	if( g_variant_dict_lookup( options, "cache-dir", "s", &cache_dir ) )
	{
		g_free( shared->cache_dir );
		shared->cache_dir = cache_dir;
	}

	g_variant_dict_lookup( options, "silent", "b", &( shared->silent ) );
	g_variant_dict_lookup( options, "width", "i", &( shared->width ) );

	if( g_variant_dict_lookup( options, "height", "i", &( shared->height ) ) )
		shared->max_height_set = FALSE;

	if( g_variant_dict_lookup( options, "max-height", "i", &( shared->max_height ) ) )
		shared->max_height_set = TRUE;

	g_variant_dict_lookup( options, "conceal", "b", &( shared->conceal ) );

	return -1;
}

static gint
on_app_command_line(
	GApplication *self,
	GApplicationCommandLine *command_line,
	gpointer user_data )
{
	GrShared *shared = (GrShared*)user_data;
	gboolean conceal;
	GtkWindow *window;
	GVariantDict *dict;

	g_return_val_if_fail( G_IS_APPLICATION( self ), EXIT_FAILURE );

	g_application_hold( self );

	if( !shared->conceal )
	{
		g_application_activate( self );
		goto out;
	}

	dict = g_application_command_line_get_options_dict( command_line );

	if( g_variant_dict_lookup( dict, "default", "b", NULL ) )
	{
		conceal = shared->conceal;
		window = shared->window;
		shared_set_default( shared );
		shared->conceal = conceal;
		shared->window = window;
		gr_shared_setup_private( shared );
		gr_window_reset( shared->window, shared );
		goto out;
	}

	if( g_variant_dict_lookup( dict, "kill", "b", NULL ) )
	{
		gtk_window_destroy( shared->window );
		goto out;
	}

	if( g_variant_dict_lookup( dict, "rescan", "b", NULL ) )
	{
		parse_config( shared->config_path, shared );
		gr_shared_setup_private( shared );
		gr_window_reset( shared->window, shared );
		goto out;
	}

	if( g_variant_dict_lookup( dict, "exhibit", "b", NULL ) )
	{
		g_application_activate( self );
		goto out;
	}

out:
	g_application_release( self );

	return EXIT_SUCCESS;
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
		{ "cache-dir", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, NULL, "Path to cache directory", "CACHE_DIR" },
		{ "no-cache", 'A', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use cache file", NULL },
		{ "config", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, NULL, "Path to configure file", "CONFIG_PATH" },
		{ "no-config", 'O', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use configure file", NULL },
		{ "conceal", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Start in conceal mode", NULL },
		{ "exhibit", 'e', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Exhibit window", NULL },
		{ "kill", 'k', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Kill program in conceal mode", NULL },
		{ "rescan", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Rescan options", NULL },
		{ "default", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Set default options", NULL },
		{ NULL }
	};

	g_return_val_if_fail( g_application_id_is_valid( APP_ID ), EXIT_FAILURE );

	app = G_APPLICATION( gtk_application_new( APP_ID, G_APPLICATION_DEFAULT_FLAGS | G_APPLICATION_HANDLES_COMMAND_LINE ) );
	if( app == NULL )
		return EXIT_FAILURE;

	/* set default options */
	shared = gr_shared_new();
	shared->silent = FALSE;
	shared->width = DEFAULT_WINDOW_WIDTH;
	shared->height = DEFAULT_WINDOW_HEIGHT;
	shared->max_height = DEFAULT_WINDOW_MAX_HEIGHT;
	shared->max_height_set = FALSE;
	shared->cache_dir = g_build_filename( g_get_user_cache_dir(), PROGRAM_NAME, NULL );
	shared->no_cache = FALSE;
	shared->config_path = g_build_filename( g_get_user_config_dir(), PROGRAM_NAME, CONFIG_FILENAME, NULL );
	shared->no_config = FALSE;
	shared->env = g_strdup( ENVPATH );
	shared->conceal = FALSE;
	shared->window = NULL;

	g_application_add_main_option_entries( app, entries );
	g_application_set_option_context_summary( app, PROGRAM_SUMMARY );
	g_application_set_option_context_description( app, PROGRAM_NAME" version "PROGRAM_VERSION );

	g_signal_connect( G_OBJECT( app ), "startup", G_CALLBACK( on_app_startup ), shared );
	g_signal_connect( G_OBJECT( app ), "activate", G_CALLBACK( on_app_activate ), shared );
	g_signal_connect( G_OBJECT( app ), "handle-local-options", G_CALLBACK( on_app_handle_local_options ), shared );
	g_signal_connect( G_OBJECT( app ), "command-line", G_CALLBACK( on_app_command_line ), shared );

	ret = g_application_run( app, argc, argv );

	gr_shared_free( shared );
	g_object_unref( G_OBJECT( app ) );

	return ret;
}


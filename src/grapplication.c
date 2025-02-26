#include "grapplication.h"

#include "config.h"
#include "grwindow.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define GR_APPLICATION_DESCRIPTION PROGRAM_NAME" version "PROGRAM_VERSION
#define GR_APPLICATION_SUMMARY "This program launches applications in a graphical environment supported by GTK4."

struct _GrApplication
{
	GtkApplication parent_instance;

	gboolean silent;
	gint width;
	gint height;
	gint max_height;
	gboolean max_height_set;
	gchar* cache_dir;
	gboolean no_cache;
	gchar* config_path;
	gboolean no_config;
	gboolean conceal;

	GrWindow *window;
};
typedef struct _GrApplication GrApplication;

enum _GrApplicationPropertyID
{
	PROP_0, /* 0 is reserved for GObject */

	PROP_SILENT,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_MAX_HEIGHT,
	PROP_MAX_HEIGHT_SET,
	PROP_CACHE_DIR,
	PROP_NO_CACHE,
	PROP_CONFIG_PATH,
	PROP_NO_CONFIG,
	PROP_CONCEAL,

	N_PROPS
};
typedef enum _GrApplicationPropertyID GrApplicationPropertyID;

static GParamSpec *object_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE( GrApplication, gr_application, GTK_TYPE_APPLICATION )

static void
gr_application_init(
	GrApplication *self )
{
	const GOptionEntry option_entries[]=
	{
		{ "silent", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not show output", NULL },
		{ "width", 'w', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, NULL, "Window width", "WIDTH" },
		{ "height", 'h', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, NULL, "Window height", "HEIGHT" },
		{ "max-height", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, NULL, "Window maximum height", "MAX_HEIGHT" },
		{ "cache-dir", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, NULL, "Path to cache directory", "CACHE_DIR" },
		{ "no-cache", 'A', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use cache file", NULL },
		{ "config", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, NULL, "Path to configure file", "CONFIG_PATH" },
		{ "no-config", 'O', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use configure file", NULL },
		{ "conceal", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Start in conceal mode", NULL },
		{ "exhibit", 'e', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Exhibit window", NULL },
		{ "kill", 'k', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Kill program in conceal mode", NULL },
		{ NULL }
	};

	gchar *program_name, *program_config_filename;

	g_application_set_option_context_description( G_APPLICATION( self ), GR_APPLICATION_DESCRIPTION );
	g_application_set_option_context_summary( G_APPLICATION( self ), GR_APPLICATION_SUMMARY );
	g_application_add_main_option_entries( G_APPLICATION( self ), option_entries );
	g_application_set_resource_base_path( G_APPLICATION( self ), NULL );

	program_name = g_filename_from_utf8( PROGRAM_NAME, -1, NULL, NULL, NULL );
	program_config_filename = g_filename_from_utf8( PROGRAM_CONFIGURE_FILE, -1, NULL, NULL, NULL );

	self->silent = FALSE;
	self->width = PROGRAM_WINDOW_WIDTH;
	self->height = PROGRAM_WINDOW_HEIGHT;
	self->max_height = PROGRAM_WINDOW_MAX_HEIGHT;
	self->max_height_set = FALSE;
	self->cache_dir = g_build_filename( g_get_user_cache_dir(), program_name, NULL );
	self->no_cache = FALSE;
	self->config_path = g_build_filename( g_get_user_config_dir(), program_name, program_config_filename, NULL );
	self->no_config = FALSE;
	self->conceal = FALSE;

	self->window = NULL;

	g_free( program_name );
	g_free( program_config_filename );
}

static void
gr_application_finalize(
	GObject *object )
{
	GrApplication *self = GR_APPLICATION( object );

	g_free( self->cache_dir );
	g_free( self->config_path );

	G_OBJECT_CLASS( gr_application_parent_class )->finalize( object );
}

static void
gr_application_get_property(
	GObject *object,
	guint prop_id,
	GValue *value,
	GParamSpec *pspec )
{
	GrApplication *self = GR_APPLICATION( object );

	switch( (GrApplicationPropertyID)prop_id )
	{
		case PROP_SILENT:
			g_value_set_boolean( value, self->silent );
			break;
		case PROP_WIDTH:
			g_value_set_int( value, self->width );
			break;
		case PROP_HEIGHT:
			g_value_set_int( value, self->height );
			break;
		case PROP_MAX_HEIGHT:
			g_value_set_int( value, self->max_height );
			break;
		case PROP_MAX_HEIGHT_SET:
			g_value_set_boolean( value, self->max_height_set );
			break;
		case PROP_CACHE_DIR:
			g_value_set_string( value, self->cache_dir );
			break;
		case PROP_NO_CACHE:
			g_value_set_boolean( value, self->no_cache );
			break;
		case PROP_CONFIG_PATH:
			g_value_set_string( value, self->config_path );
			break;
		case PROP_NO_CONFIG:
			g_value_set_boolean( value, self->no_config );
			break;
		case PROP_CONCEAL:
			g_value_set_boolean( value, self->conceal );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_application_activate(
	GApplication *app )
{
	GrApplication *self = GR_APPLICATION( app );

	G_APPLICATION_CLASS( gr_application_parent_class )->activate( app );

	gtk_window_present( GTK_WINDOW( self->window ) );
}

static void
gr_application_parse_config(
	GrApplication *self )
{
	gboolean silent, no_cache, conceal;
	gint width, height, max_height;
	gchar *cache_dir;
	GKeyFile *key_file;
	GError *error = NULL;

	g_return_if_fail( GR_IS_APPLICATION( self ) );

	/* load config */
	key_file = g_key_file_new();
	if( !g_key_file_load_from_file( key_file, self->config_path, G_KEY_FILE_NONE, NULL ) )
		goto out;
		
	/* get options */
	silent = g_key_file_get_boolean( key_file, "Main", "silent", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		self->silent = silent;

	width = g_key_file_get_integer( key_file, "Main", "width", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		self->width = width;

	height = g_key_file_get_integer( key_file, "Main", "height", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
	{
		self->height = height;
		self->max_height_set = FALSE;
	}

	max_height = g_key_file_get_integer( key_file, "Main", "max-height", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
	{
		self->max_height = max_height;
		self->max_height_set = TRUE;
	}

	cache_dir = g_key_file_get_string( key_file, "Main", "cache-dir", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
	{
		g_free( self->cache_dir );
		self->cache_dir = g_filename_from_utf8( cache_dir, -1, NULL, NULL, NULL );
		g_free( cache_dir );
	}

	no_cache = g_key_file_get_boolean( key_file, "Main", "no-cache", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		self->no_cache = no_cache;

	conceal = g_key_file_get_boolean( key_file, "Main", "conceal", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		self->conceal = conceal;

out:
	g_key_file_free( key_file );
}

static gint
gr_application_handle_local_options(
	GApplication *app,
	GVariantDict *options )
{
	GrApplication *self = GR_APPLICATION( app );
	gchar *config_path, *cache_dir;

	g_variant_dict_lookup( options, "no-config", "b", &self->no_config );

	/* scan config file, if it is set */
	if( g_variant_dict_lookup( options, "config", "^ay", &config_path ) )
	{
		g_free( self->config_path );
		self->config_path = config_path;
	}
	if( !self->no_config )
		gr_application_parse_config( self );

	g_variant_dict_lookup( options, "silent", "b", &self->silent );
	g_variant_dict_lookup( options, "width", "i", &self->width );

	if( g_variant_dict_lookup( options, "height", "i", &self->height ) )
		self->max_height_set = FALSE;

	if( g_variant_dict_lookup( options, "max-height", "i", &self->max_height ) )
		self->max_height_set = TRUE;

	g_variant_dict_lookup( options, "no-cache", "b", &self->no_cache );

	if( g_variant_dict_lookup( options, "cache-dir", "^ay", &cache_dir ) )
	{
		g_free( self->cache_dir );
		self->cache_dir = cache_dir;
	}

	g_variant_dict_lookup( options, "conceal", "b", &self->conceal );

	return -1;
}

static gint
gr_application_command_line(
	GApplication *app,
	GApplicationCommandLine *command_line )
{
	GrApplication *self = GR_APPLICATION( app );
	GVariantDict *options;

	g_return_val_if_fail( GR_IS_APPLICATION( self ), EXIT_FAILURE );

	/* if no conceal mode, create window and run as usual */
	if( !self->conceal )
	{
		g_application_hold( G_APPLICATION( self ) );

		if( self->window != NULL && GR_IS_WINDOW( self->window ) )
			gtk_window_destroy( GTK_WINDOW( self->window ) );
		self->window = gr_window_new( self );

		g_application_release( G_APPLICATION( self ) );

		g_application_activate( G_APPLICATION( self ) );

		return EXIT_SUCCESS;
	}

	/* get input options */
	options = g_application_command_line_get_options_dict( command_line );

	if( g_variant_dict_lookup( options, "exhibit", "b", NULL ) )
	{
		g_application_activate( G_APPLICATION( self ) );
		return EXIT_SUCCESS;
	}

	if( g_variant_dict_lookup( options, "kill", "b", NULL ) )
	{
		gtk_window_destroy( GTK_WINDOW( self->window ) );
		return EXIT_SUCCESS;
	}

	return EXIT_SUCCESS;
}

static void
gr_application_class_init(
	GrApplicationClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );
	GApplicationClass *app_class = G_APPLICATION_CLASS( klass );

	object_class->finalize = gr_application_finalize;
	object_class->get_property = gr_application_get_property;

	object_props[PROP_SILENT] = g_param_spec_boolean(
		"silent",
		"Silent",
		"Do not show output",
		FALSE,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_WIDTH] = g_param_spec_int(
		"width",
		"Width",
		"Window width",
		0,
		G_MAXINT,
		0,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_HEIGHT] = g_param_spec_int(
		"height",
		"Height",
		"Window height",
		0,
		G_MAXINT,
		0,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_MAX_HEIGHT] = g_param_spec_int(
		"max-height",
		"Maximum height",
		"Window maximum height",
		0,
		G_MAXINT,
		0,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_MAX_HEIGHT_SET] = g_param_spec_boolean(
		"max-height-set",
		"Setting of maximum height",
		"TRUE, if max-height is set",
		FALSE,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_CACHE_DIR] = g_param_spec_string(
		"cache-dir",
		"Cache directory",
		"Path to cache directory",
		NULL,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_NO_CACHE] = g_param_spec_boolean(
		"no-cache",
		"Not using cache",
		"Do not use cache file",
		FALSE,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_CONFIG_PATH] = g_param_spec_string(
		"config-path",
		"Configure path",
		"Path to configure file",
		NULL,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_NO_CONFIG] = g_param_spec_boolean(
		"no-config",
		"Not using configure file",
		"Do not use configure file",
		FALSE,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_CONCEAL] = g_param_spec_boolean(
		"conceal",
		"Conceal mode",
		"TRUE, if application runs in conceal mode",
		FALSE,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	g_object_class_install_properties( object_class, N_PROPS, object_props );

	app_class->activate = gr_application_activate;
	app_class->handle_local_options = gr_application_handle_local_options;
	app_class->command_line = gr_application_command_line;
}

GrApplication*
gr_application_new(
	const gchar *application_id )
{
	g_return_val_if_fail( g_application_id_is_valid( application_id ), NULL );

	return GR_APPLICATION( g_object_new( GR_TYPE_APPLICATION, "application-id", application_id, "flags", G_APPLICATION_DEFAULT_FLAGS | G_APPLICATION_HANDLES_COMMAND_LINE, NULL ) );
}

gboolean
gr_application_get_silent(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), FALSE );

	return self->silent;
}

gint
gr_application_get_width(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), 0 );

	return self->width;
}

gint
gr_application_get_height(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), 0 );

	return self->height;
}

gint
gr_application_get_max_height(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), 0 );

	return self->max_height;
}

gboolean
gr_application_get_max_height_set(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), FALSE );

	return self->max_height_set;
}

gchar*
gr_application_get_cache_dir(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), NULL );

	return g_strdup( self->cache_dir );
}

gboolean
gr_application_get_no_cache(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), FALSE );

	return self->no_cache;
}

gchar*
gr_application_get_config_path(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), NULL );

	return g_strdup( self->config_path );
}

gboolean
gr_application_get_no_config(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), FALSE );

	return self->no_config;
}

gboolean
gr_application_get_conceal(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), FALSE );

	return self->conceal;
}


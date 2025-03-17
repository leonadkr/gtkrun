#include "grapplication.h"

#include "config.h"
#include "grcommandlist.h"
#include "grwindow.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct _GrApplication
{
	GtkApplication parent_instance;

	gboolean silent;
	gint width;
	gint height;
	gint max_height;
	gboolean max_height_set;
	gchar* history_path;
	gboolean no_history;
	gchar* config_path;
	gboolean no_config;

	GrWindow *window;
	GrCommandList *com_list;
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
	PROP_HISTORY_PATH,
	PROP_NO_HISTORY,
	PROP_CONFIG_PATH,
	PROP_NO_CONFIG,
	PROP_COMMAND_LIST,

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
		{ "history-path", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, NULL, "Path to history file", "HISTORY_PATH" },
		{ "no-history", 'A', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use history file", NULL },
		{ "config", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, NULL, "Path to configure file", "CONFIG_PATH" },
		{ "no-config", 'C', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Do not use configure file", NULL },
		{ NULL }
	};

	gchar *program_name, *config_filename, *history_filename;

	g_application_set_option_context_description( G_APPLICATION( self ), PROGRAM_APPLICATION_DESCRIPTION );
	g_application_set_option_context_summary( G_APPLICATION( self ), PROGRAM_APPLICATION_SUMMARY );
	g_application_add_main_option_entries( G_APPLICATION( self ), option_entries );
	g_application_set_resource_base_path( G_APPLICATION( self ), NULL );

	program_name = g_filename_from_utf8( PROGRAM_NAME, -1, NULL, NULL, NULL );
	config_filename = g_filename_from_utf8( PROGRAM_CONFIGURE_FILE, -1, NULL, NULL, NULL );
	history_filename = g_filename_from_utf8( PROGRAM_HISTORY_FILE, -1, NULL, NULL, NULL );

	self->silent = FALSE;
	self->width = PROGRAM_WINDOW_WIDTH;
	self->height = PROGRAM_WINDOW_HEIGHT;
	self->max_height = PROGRAM_WINDOW_MAX_HEIGHT;
	self->max_height_set = FALSE;
	self->history_path = g_build_filename( g_get_user_cache_dir(), program_name, history_filename, NULL );
	self->no_history = FALSE;
	self->config_path = g_build_filename( g_get_user_config_dir(), program_name, config_filename, NULL );
	self->no_config = FALSE;

	g_free( program_name );
	g_free( config_filename );
	g_free( history_filename );

	self->window = NULL;
	self->com_list = NULL;
}

static void
gr_application_dispose(
	GObject *object )
{
	GrApplication *self = GR_APPLICATION( object );

	g_clear_object( &self->com_list );

	G_OBJECT_CLASS( gr_application_parent_class )->dispose( object );
}

static void
gr_application_finalize(
	GObject *object )
{
	GrApplication *self = GR_APPLICATION( object );

	g_free( self->history_path );
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
		case PROP_HISTORY_PATH:
			g_value_set_string( value, self->history_path );
			break;
		case PROP_NO_HISTORY:
			g_value_set_boolean( value, self->no_history );
			break;
		case PROP_CONFIG_PATH:
			g_value_set_string( value, self->config_path );
			break;
		case PROP_NO_CONFIG:
			g_value_set_boolean( value, self->no_config );
			break;
		case PROP_COMMAND_LIST:
			g_value_set_object( value, self->com_list );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_application_startup(
	GApplication *app )
{
	GrApplication *self = GR_APPLICATION( app );

	G_APPLICATION_CLASS( gr_application_parent_class )->startup( app );

	/* create command list */
	if( self->no_history )
		self->com_list = gr_command_list_new( NULL );
	else
		self->com_list = gr_command_list_new( self->history_path );

	/* create window */
	self->window = gr_window_new( self );
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
	gboolean silent, no_history;
	gint width, height, max_height;
	gchar *history_path;
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

	history_path = g_key_file_get_string( key_file, "Main", "history-path", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
	{
		g_free( self->history_path );
		self->history_path = g_filename_from_utf8( history_path, -1, NULL, NULL, NULL );
		g_free( history_path );
	}

	no_history = g_key_file_get_boolean( key_file, "Main", "no-history", &error );
	if( error != NULL )
		g_clear_error( &error );
	else
		self->no_history = no_history;

out:
	g_key_file_free( key_file );
}

static gint
gr_application_handle_local_options(
	GApplication *app,
	GVariantDict *options )
{
	GrApplication *self = GR_APPLICATION( app );
	gchar *config_path, *history_path;

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

	g_variant_dict_lookup( options, "no-history", "b", &self->no_history );

	if( g_variant_dict_lookup( options, "history-path", "^ay", &history_path ) )
	{
		g_free( self->history_path );
		self->history_path = history_path;
	}

	return -1;
}

static void
gr_application_class_init(
	GrApplicationClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );
	GApplicationClass *app_class = G_APPLICATION_CLASS( klass );

	object_class->dispose = gr_application_dispose;
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
	object_props[PROP_HISTORY_PATH] = g_param_spec_string(
		"history-path",
		"History path",
		"Path to history file",
		NULL,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_NO_HISTORY] = g_param_spec_boolean(
		"no-history",
		"Not using history",
		"Do not use history file",
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
	object_props[PROP_COMMAND_LIST] = g_param_spec_object(
		"command-list",
		"Command list",
		"Object storing the list of commands",
		GR_TYPE_COMMAND_LIST,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	g_object_class_install_properties( object_class, N_PROPS, object_props );

	app_class->startup = gr_application_startup;
	app_class->activate = gr_application_activate;
	app_class->handle_local_options = gr_application_handle_local_options;
}

GrApplication*
gr_application_new(
	const gchar *application_id )
{
	g_return_val_if_fail( g_application_id_is_valid( application_id ), NULL );

	return GR_APPLICATION( g_object_new( GR_TYPE_APPLICATION, "application-id", application_id, "flags", G_APPLICATION_DEFAULT_FLAGS, NULL ) );
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
gr_application_get_history_path(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), NULL );

	return g_strdup( self->history_path );
}

gboolean
gr_application_get_no_history(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), FALSE );

	return self->no_history;
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

GrCommandList*
gr_application_get_command_list(
	GrApplication *self )
{
	g_return_val_if_fail( GR_IS_APPLICATION( self ), NULL );

	return GR_COMMAND_LIST( g_object_ref( G_OBJECT( self->com_list ) ) );
}


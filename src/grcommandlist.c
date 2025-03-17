#include "grcommandlist.h"

#include "config.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>

struct _GrCommandList
{
	GObject parent_instance;

	gchar *his_file_path;

	GStrv his_arr;
	GSList *env_list;
};
typedef struct _GrCommandList GrCommandList;

enum _GrCommandListPropertyID
{
	PROP_0, /* 0 is reserved for GObject */

	PROP_HISTORY_FILE_PATH,

	N_PROPS
};
typedef enum _GrCommandListPropertyID GrCommandListPropertyID;

static GParamSpec *object_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE( GrCommandList, gr_command_list, G_TYPE_OBJECT )

static void
gr_command_list_load_history_array(
	GrCommandList *self )
{
	GFile *file;
	gchar *text_locale, *text_utf8;
	gsize size;
	GStrv arr;
	guint last_idx;

	g_return_if_fail( GR_IS_COMMAND_LIST( self ) );

	g_print( "gr_command_list_load_history_array()\n" );

	/* if the file cannot be loaded, do nothing */
	if( self->his_file_path == NULL )
		return;
	file = g_file_new_for_path( self->his_file_path );
	if( !g_file_load_contents( file, NULL, &text_locale, &size, NULL, NULL ) )
	{
		g_object_unref( G_OBJECT( file ) );
		return;
	}
	g_object_unref( G_OBJECT( file ) );

	/* load array */
	text_utf8 = g_locale_to_utf8( text_locale, size, NULL, NULL, NULL );
	g_free( text_locale );
	arr = g_strsplit( text_utf8, PROGRAM_LINE_BREAKER, -1 );
	g_free( text_utf8 );

	/* the last string may be zero-length, removing it */
	last_idx = g_strv_length( arr ) - 1;
	if( strlen( arr[last_idx] ) == 0 )
	{
		g_free( arr[last_idx] );
		arr[last_idx] = NULL;
	}

	g_strfreev( self->his_arr );
	self->his_arr = arr;
}

static GSList*
gr_command_list_load_environment_binaries_list(
	const gchar *env_path )
{
	const gchar env_delim[] = ":";
	const gchar *env_str = NULL;

	GStrv env_arr, a;
	GFile *dir;
	GFileEnumerator *dir_enum;
	GFileInfo *file_info;
	GSList *list;

	g_return_val_if_fail( env_path != NULL, NULL );

	g_print( "gr_command_list_load_environment_binaries_list()\n" );

	env_str = g_getenv( env_path );
	if( env_str == NULL )
		return NULL;

	env_arr = g_strsplit( env_str, env_delim, -1 );
	list = NULL;
	for( a = env_arr; *a != NULL; ++a )
	{
		dir = g_file_new_for_path( *a );
		dir_enum = g_file_enumerate_children( dir, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, G_FILE_QUERY_INFO_NONE, NULL, NULL );
		if( dir_enum == NULL )
		{
			g_object_unref( G_OBJECT( dir ) );
			continue;
		}

		while( TRUE )
		{
			if( !g_file_enumerator_iterate( dir_enum, &file_info, NULL, NULL, NULL ) )
				break;

			if( file_info == NULL )
				break;

			list = g_slist_insert_sorted( list, g_strdup( g_file_info_get_display_name( file_info ) ), (GCompareFunc)g_strcmp0 );
		}

		g_object_unref( G_OBJECT( dir_enum ) );
		g_object_unref( G_OBJECT( dir ) );
	}
	g_strfreev( env_arr );

	return list;
}

static void
gr_command_list_init(
	GrCommandList *self )
{
	self->his_file_path = NULL;

	/* setup empty command array */
	self->his_arr = g_new( gchar*, 1 );
	self->his_arr[0] = NULL;

	self->env_list = gr_command_list_load_environment_binaries_list( PROGRAM_ENVIRONMENT_PATH );
}

static void
gr_command_list_finalize(
	GObject *object )
{
	GrCommandList *self = GR_COMMAND_LIST( object );

	g_print( "gr_command_list_finalize()\n" );

	g_free( self->his_file_path );
	g_strfreev( self->his_arr );
	g_slist_free_full( self->env_list, (GDestroyNotify)g_free );

	G_OBJECT_CLASS( gr_command_list_parent_class )->finalize( object );
}

static void
gr_command_list_get_property(
	GObject *object,	
	guint prop_id,	
	GValue *value,	
	GParamSpec *pspec )
{
	GrCommandList *self = GR_COMMAND_LIST( object );

	switch( (GrCommandListPropertyID)prop_id )
	{
		case PROP_HISTORY_FILE_PATH:
			g_value_set_string( value, self->his_file_path );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_command_list_set_property(
	GObject *object,
	guint prop_id,
	const GValue *value,
	GParamSpec *pspec )
{
	GrCommandList *self = GR_COMMAND_LIST( object );

	switch( (GrCommandListPropertyID)prop_id )
	{
		case PROP_HISTORY_FILE_PATH:
			gr_command_list_set_history_file_path( self, g_value_get_string( value ) );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_command_list_class_init(
	GrCommandListClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );

	object_class->finalize = gr_command_list_finalize;
	object_class->get_property = gr_command_list_get_property;
	object_class->set_property = gr_command_list_set_property;

	object_props[PROP_HISTORY_FILE_PATH] = g_param_spec_string(
		"history-file-path",
		"History file path",
		"Path to the file containing the list of commands",
		NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS );
	g_object_class_install_properties( object_class, N_PROPS, object_props );
}

static gint
strncmp0(
	const gchar *s1,
	const gchar *s2,
	gsize n )
{
	if( s1 == NULL || s2 == NULL )
		return g_strcmp0( s1, s2 );

	return strncmp( s1, s2, n );
}

GrCommandList*
gr_command_list_new(
	const gchar *his_file_path )
{
	g_print( "gr_command_list_new()\n" );

	return GR_COMMAND_LIST( g_object_new( GR_TYPE_COMMAND_LIST, "history-file-path", his_file_path, NULL ) );
}

gchar*
gr_command_list_get_history_file_path(
	GrCommandList *self )
{
	g_return_val_if_fail( GR_IS_COMMAND_LIST( self ), NULL );

	return g_strdup( self->his_file_path );
}

void
gr_command_list_set_history_file_path(
	GrCommandList *self,
	const gchar *path )
{
	g_return_if_fail( GR_IS_COMMAND_LIST( self ) );

	g_print( "gr_command_list_set_history_file_path()\n" );

	g_object_freeze_notify( G_OBJECT( self ) );

	g_free( self->his_file_path );
	self->his_file_path = g_strdup( path );
	gr_command_list_load_history_array( self );

	g_object_notify_by_pspec( G_OBJECT( self ), object_props[PROP_HISTORY_FILE_PATH] );

	g_object_thaw_notify( G_OBJECT( self ) );
}

gchar*
gr_command_list_get_compared_string(
	GrCommandList *self,
	const gchar *str )
{
	gsize str_len;
	GStrv a;
	GSList *l;
	gint res;

	g_print( "gr_command_list_get_compared_string()\n" );

	g_return_val_if_fail( GR_IS_COMMAND_LIST( self ), NULL );

	if( str == NULL || *str == '\0' )
		return NULL;

	str_len = strlen( str );

	for( a = self->his_arr; *a != NULL; ++a )
		if( strncmp0( *a, str, str_len ) == 0 )
			return g_strdup( *a );

	for( l = self->env_list; l != NULL; l = l->next )
	{
		res = strncmp0( (const gchar*)l->data, str, str_len );

		if( res > 0 )
			return NULL;

		if( res == 0 )
			return g_strdup( (const gchar*)l->data );
	}

	return NULL;
}

GStrv
gr_command_list_get_compared_array(
	GrCommandList *self,
	const gchar *str )
{
	gsize str_len, list_len;
	GStrv arr, a;
	GSList *l, *list;
	gint res;

	g_print( "gr_command_list_get_compared_array()\n" );

	g_return_val_if_fail( GR_IS_COMMAND_LIST( self ), NULL );

	if( str == NULL || *str == '\0' )
		return NULL;

	str_len = strlen( str );

	list = NULL;
	list_len = 0;
	for( a = self->his_arr; *a != NULL; ++a )
		if( strncmp0( *a, str, str_len ) == 0 )
		{
			list = g_slist_prepend( list, *a );
			++list_len;
		}

	for( l = self->env_list; l != NULL; l = l->next )
	{
		res = strncmp0( (const gchar*)l->data, str, str_len );

		if( res > 0 )
			break;

		/* ignore a string already in the list */
		if( res == 0 && g_slist_find_custom( list, (const gchar*)l->data, (GCompareFunc)g_strcmp0 ) == NULL )
		{
			list = g_slist_prepend( list, (gchar*)l->data );
			++list_len;
		}
	}

	if( list == NULL )
		return NULL;

	arr = g_new( gchar*, list_len + 1 );
	arr[list_len] = NULL;
	for( a = &arr[list_len-1], l = list; l != NULL ; l = l->next, --a )
		*a = g_strdup( (const gchar*)l->data );
	g_slist_free( list );

	return arr;
}

void
gr_command_list_push(
	GrCommandList *self,
	const gchar *text )
{
	GStrvBuilder *builder;
	GStrv arr;
	GFile *file, *dir;
	gchar *s_locale, *s_utf8;
	gsize s_locale_len;
	GError *error = NULL;

	g_print( "gr_command_list_push()\n" );

	g_return_if_fail( GR_IS_COMMAND_LIST( self ) );

	/* nothing to push */
	if( text == NULL || *text == '\0' )
		return;

	/* if his_arr already contains text, do nothing */
	if( g_strv_contains( (const gchar**)self->his_arr, text ) )
		return;

	/* append text to array */
	builder = g_strv_builder_new();
	g_strv_builder_addv( builder, (const gchar**)self->his_arr );
	g_strv_builder_add( builder, g_strdup( text ) );
	arr = g_strv_builder_end( builder );
	g_strv_builder_unref( builder );

	g_strfreev( self->his_arr );
	self->his_arr = arr;

	/* if no file path, array will not be stored */
	if( self->his_file_path == NULL )
		return;

	/* store array to the file */
	file = g_file_new_for_path( self->his_file_path );

	/* if it cannot create directory, it will not store array */
	dir = g_file_get_parent( file );
	g_file_make_directory_with_parents( dir, NULL, &error );
	if( error != NULL )
	{
		if( error->code != G_IO_ERROR_EXISTS )
		{
			g_object_unref( G_OBJECT( dir ) );
			g_object_unref( G_OBJECT( file ) );
			g_clear_error( &error );
			return;
		}
		g_clear_error( &error );
	}
	g_object_unref( G_OBJECT( dir ) );

	s_locale = g_strjoinv( PROGRAM_LINE_BREAKER, self->his_arr );
	s_utf8 = g_strconcat( s_locale, PROGRAM_LINE_BREAKER, NULL );
	g_free( s_locale );

	s_locale = g_locale_from_utf8( s_utf8, -1, NULL, &s_locale_len, NULL );
	g_free( s_utf8 );

	g_file_replace_contents( file, s_locale, s_locale_len, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, NULL, NULL );
	g_free( s_locale );

	g_object_unref( G_OBJECT( file ) );
}


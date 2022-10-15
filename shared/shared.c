#include <glib.h>
#include <gio/gio.h>
#include "shared.h"


static GList*
get_compared_list(
	GList *filename_list,
	const gchar *text )
{
	gchar *s;
	GList *l, *compared_list;
	gint text_len;

	/* nothing to do */
	if( filename_list == NULL || text == NULL )
		return NULL;

	text_len = strlen( text );
	if( text_len == 0 )
		return NULL;

	compared_list = NULL;
	for( l = filename_list; l != NULL; l = l->next )
	{
		s = (gchar*)l->data;
		if( g_strstr_len( s, text_len, text ) != NULL )
			compared_list = g_list_prepend( compared_list, g_strdup( s ) );
	}

	return g_list_reverse( compared_list );
}

static gchar*
duplicate_string(
	gchar *str,
	gpointer user_data )
{
	g_return_val_if_fail( str != NULL, NULL );

	return g_strdup( str );
}


GrShared*
gr_shared_new(
	void )
{
	GrShared *self = g_new0( GrShared, 1 );
	self->silent = FALSE;
	self->width = 0;
	self->height = 0;
	self->max_height = 0;
	self->cache_filepath = NULL;
	self->no_cache = FALSE;
	self->config_filepath = NULL;
	self->no_config = FALSE;
	self->max_height_set = FALSE;
	self->env_filename_list = NULL;
	self->cache_filename_list = NULL;

	return self;
}

void
gr_shared_free(
	GrShared *self )
{
	g_return_if_fail( self != NULL );

	g_free( self->cache_filepath );
	g_free( self->config_filepath );
	g_list_free_full( self->env_filename_list, (GDestroyNotify)g_free );
	g_list_free_full( self->cache_filename_list, (GDestroyNotify)g_free );
	g_free( self );
}

GrShared*
gr_shared_dup(
	GrShared *self )
{
	GrShared *shared;

	g_return_val_if_fail( self != NULL, NULL );

	shared = gr_shared_new();
	shared->silent = self->silent;
	shared->width = self->width;
	shared->height = self->height;
	shared->max_height = self->max_height;
	shared->cache_filepath = g_strdup( self->cache_filepath );
	shared->no_cache = self->no_cache;
	shared->config_filepath = g_strdup( self->config_filepath );
	shared->no_config = self->no_config;
	shared->max_height_set = self->max_height_set;
	shared->env_filename_list = g_list_copy_deep( self->env_filename_list, (GCopyFunc)duplicate_string, NULL );
	shared->cache_filename_list = g_list_copy_deep( self->cache_filename_list, (GCopyFunc)duplicate_string, NULL );

	return shared;
}

GList*
gr_shared_get_compared_list(
	GrShared *self,
	const gchar *text )
{
	GList *cache_list, *env_list;
	GList *l, *exclist;

	g_return_val_if_fail( self != NULL, NULL );

	/* nothing to do */
	if( text == NULL )
		return NULL;

	if( self->no_cache )
		cache_list = NULL;
	else
		cache_list = get_compared_list( self->cache_filename_list, text );
	env_list = get_compared_list( self->env_filename_list, text );

	/* remove dublicates from cache_list and env_list */
	for( l = cache_list; l != NULL; l = l->next )
		while( TRUE )
		{
			exclist = g_list_find_custom( env_list, l->data, (GCompareFunc)g_strcmp0 );
			if( exclist == NULL )
				break;
			env_list = g_list_remove_link( env_list, exclist );
			g_list_free_full( exclist, (GDestroyNotify)g_free );
		}

	return g_list_concat( cache_list, env_list );
}

void
gr_shared_store_command_to_cache(
	GrShared *self,
	const gchar *command )
{
	GFile *file, *dir;
	GFileOutputStream *file_stream;
	GDataOutputStream *data_stream;
	gchar *line;
	GList *l;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( command == NULL || self->no_cache )
		return;

	/* if command is in cache -- nothing to do */
	for( l = self->cache_filename_list; l != NULL; l = l->next )
		if( g_strcmp0( (gchar*)l->data, command ) == 0 )
			return;

	file = g_file_new_for_path( self->cache_filepath );
	dir = g_file_get_parent( file );

	g_file_make_directory_with_parents( dir, NULL, &error );
	if( error != NULL && error->code != G_IO_ERROR_EXISTS )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		goto out;
	}
	else
		g_clear_error( &error );

	file_stream = g_file_append_to( file, G_FILE_CREATE_PRIVATE, NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		goto out;
	}
	data_stream = g_data_output_stream_new( G_OUTPUT_STREAM( file_stream ) );

	line = g_strconcat( command, "\n", NULL );
	g_data_output_stream_put_string( data_stream, line, NULL, &error );
	g_free( line );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
	}

	g_object_unref( G_OBJECT( data_stream ) );
	g_object_unref( G_OBJECT( file_stream ) );

out:
	g_object_unref( G_OBJECT( dir ) );
	g_object_unref( G_OBJECT( file ) );
}

void
gr_shared_system_call(
	GrShared *self,
	const gchar* command )
{
	GSubprocessFlags flags;
	GSubprocess *subproc;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( command == NULL )
		return;

	if( self->silent )
		flags = G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE;
	else
		flags = G_SUBPROCESS_FLAGS_NONE;

	subproc = g_subprocess_new( flags, &error, command, NULL );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
	}
	else
		g_object_unref( G_OBJECT( subproc ) );
}


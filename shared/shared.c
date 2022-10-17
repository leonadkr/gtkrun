#include <glib.h>
#include <gio/gio.h>
#include "shared.h"


static GList*
get_compared_list(
	GPtrArray *filenames,
	const gchar *text )
{
	gchar *s;
	guint i;
	GList *compared_list;
	gint text_len;

	/* nothing to do */
	if( filenames == NULL || text == NULL )
		return NULL;

	text_len = strlen( text );
	if( text_len == 0 )
		return NULL;

	compared_list = NULL;
	for( i = 0; i < filenames->len; ++i )
	{
		s = (gchar*)filenames->pdata[i];
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

static void
filename_array_add_from_path(
	GPtrArray *filenames,
	gchar *dirname,
	GError **error )
{
	GFile *dir;
	GFileEnumerator *direnum;
	GFileInfo *fileinfo;
	GError *error_loc = NULL;

	g_return_if_fail( filenames != NULL );

	/* nothing to do */
	if( dirname == NULL )
		return;

	dir = g_file_new_for_path( dirname );
	direnum = g_file_enumerate_children(
		dir,
		G_FILE_ATTRIBUTE_STANDARD_NAME,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&error_loc );
	if( error_loc != NULL )
	{
		g_propagate_error( error, error_loc );
		goto out1;
	}

	while( TRUE )
	{
		g_file_enumerator_iterate( direnum, &fileinfo, NULL, NULL, &error_loc );
		if( error_loc != NULL )
		{
			g_propagate_error( error, error_loc );
			goto out2;
		}

		if( fileinfo == NULL )
			break;

		g_ptr_array_add( filenames, g_strdup( g_file_info_get_name( fileinfo ) ) );
	}

out2:
	g_object_unref( G_OBJECT( direnum ) );

out1:
	g_object_unref( G_OBJECT( dir ) );
}

GrShared*
gr_shared_new(
	void )
{
	const guint array_size = 4096;

	GrShared *self = g_new0( GrShared, 1 );

	self->silent = FALSE;
	self->width = 0;
	self->height = 0;
	self->max_height = 0;
	self->max_height_set = FALSE;
	self->cache_filepath = NULL;
	self->no_cache = FALSE;
	self->config_filepath = NULL;
	self->no_config = FALSE;

	self->env_filenames = g_ptr_array_new_full( array_size, (GDestroyNotify)g_free );
	self->cache_filenames = g_ptr_array_new_full( array_size, (GDestroyNotify)g_free );

	return self;
}

void
gr_shared_free(
	GrShared *self )
{
	g_return_if_fail( self != NULL );

	g_free( self->cache_filepath );
	g_free( self->config_filepath );
	g_ptr_array_unref( self->env_filenames );
	g_ptr_array_unref( self->cache_filenames );
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
	shared->max_height_set = self->max_height_set;
	shared->cache_filepath = g_strdup( self->cache_filepath );
	shared->no_cache = self->no_cache;
	shared->config_filepath = g_strdup( self->config_filepath );
	shared->no_config = self->no_config;

	shared->env_filenames = g_ptr_array_copy( self->env_filenames, (GCopyFunc)duplicate_string, NULL );
	shared->cache_filenames = g_ptr_array_copy( self->cache_filenames, (GCopyFunc)duplicate_string, NULL );

	return shared;
}

void
gr_shared_set_filenames_from_env(
	GrShared *self,
	const gchar *pathenv )
{
	const gchar delim[] = ":";

	const gchar *pathstr;
	gchar **patharr, **arr;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( pathenv == NULL )
		return;

	pathstr = g_getenv( pathenv );
	patharr = g_strsplit( pathstr, delim, -1 );

	for( arr = patharr; arr[0] != NULL; arr++ )
	{
		filename_array_add_from_path( self->env_filenames, arr[0], &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
		}
	}
	g_strfreev( patharr );

	g_ptr_array_sort( self->env_filenames, (GCompareFunc)g_strcmp0 );
}

void
gr_shared_set_filenames_from_cache(
	GrShared *self,
	gchar *cache_filepath )
{
	GFile *file;
	GFileInputStream *file_stream;
	GDataInputStream *data_stream;
	gsize size;
	gchar *line;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( cache_filepath == NULL )
		return;

	file = g_file_new_for_path( cache_filepath );
	file_stream = g_file_read( file, NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		goto out1;
	}
	data_stream = g_data_input_stream_new( G_INPUT_STREAM( file_stream ) );

	while( TRUE )
	{
		line = g_data_input_stream_read_line( data_stream, &size, NULL, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			goto out2;
		}

		if( line == NULL )
			break;

		g_ptr_array_add( self->cache_filenames, line );
	}

out2:
	g_object_unref( G_OBJECT( data_stream ) );
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );
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
		cache_list = get_compared_list( self->cache_filenames, text );
	env_list = get_compared_list( self->env_filenames, text );

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
	const gchar lineend[] = "\r\n";

	GFile *file, *dir;
	GFileOutputStream *file_stream;
	GDataOutputStream *data_stream;
	gchar *line;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( command == NULL || self->no_cache )
		return;

	/* if command is in cache -- nothing to do */
	if( g_ptr_array_find_with_equal_func( self->cache_filenames, command, (GEqualFunc)g_str_equal, NULL ) )
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

	line = g_strconcat( command, lineend, NULL );
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


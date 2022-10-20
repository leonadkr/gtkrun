#include <glib.h>
#include <gio/gio.h>
#include "shared.h"


#define DEFAULT_ARRAY_SIZE 4096


/* add strings equal to text from filenames to arr */
/* but not more than count */
/* if count < 0, add all equals */
/* filenames should be sorted */
static void
set_compared_array(
	GPtrArray *arr,
	GPtrArray *filenames,
	const gchar *text,
	gint count )
{
	gboolean domain_found;
	guint i, n;
	gchar *s;
	gssize text_len;

	/* nothing to do */
	if( filenames == NULL || text == NULL )
		return;

	/* nothing to do */
	text_len = (gssize)strlen( text );
	if( text_len == 0 )
		return;

	domain_found = FALSE;
	for( i = 0, n = 0; i < filenames->len; ++i )
	{
		s = (gchar*)filenames->pdata[i];
		if( g_strstr_len( s, text_len, text ) != NULL )
		{
			g_ptr_array_add( arr, g_strdup( s ) );
			n++;
			domain_found = TRUE;
		}
		else if( domain_found )
			break;

		if( count > 0 && n >= count )
			break;
	}
}

static gchar*
duplicate_string(
	gchar *str,
	gpointer user_data )
{
	g_return_val_if_fail( str != NULL, NULL );

	return g_strdup( str );
}

static gint
compare_strings_by_points(
	gchar **s1,
	gchar **s2 )
{
	return g_strcmp0( *s1, *s2 );
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

	self->env_filenames = g_ptr_array_new_full( DEFAULT_ARRAY_SIZE, (GDestroyNotify)g_free );
	self->cache_filenames = g_ptr_array_new_full( DEFAULT_ARRAY_SIZE, (GDestroyNotify)g_free );

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
	g_ptr_array_sort( self->env_filenames, (GCompareFunc)compare_strings_by_points );
	g_strfreev( patharr );
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
	g_ptr_array_sort( self->cache_filenames, (GCompareFunc)compare_strings_by_points );

out2:
	g_object_unref( G_OBJECT( data_stream ) );
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );
}

gchar*
gr_shared_get_compared_string(
	GrShared *self,
	const gchar *text )
{
	gchar *s;
	GPtrArray *arr = g_ptr_array_new_full( 1, (GDestroyNotify)g_free );

	if( !self->no_cache )
		set_compared_array( arr, self->cache_filenames, text, 1 );

	if( arr->len < 1 )
		set_compared_array( arr, self->env_filenames, text, 1 );

	if( arr->len < 1 )
		s = NULL;
	else
		s = g_strdup( (gchar*)arr->pdata[0] );

	g_ptr_array_unref( arr );

	return s;
}

GPtrArray*
gr_shared_get_compared_array(
	GrShared *self,
	const gchar *text )
{
	guint i, j;
	gchar *si, *sj;
	GPtrArray *arr = g_ptr_array_new_full( DEFAULT_ARRAY_SIZE, (GDestroyNotify)g_free );

	if( !self->no_cache )
		set_compared_array( arr, self->cache_filenames, text, -1 );
	set_compared_array( arr, self->env_filenames, text, -1 );

	/* replace dublicates with NULL */
	for( i = 0; i < arr->len; ++i )
		for( j = i + 1; j < arr->len; ++j )
		{
			si = (gchar*)arr->pdata[i];
			sj = (gchar*)arr->pdata[j];

			if( si == NULL )
				continue;

			if( g_strcmp0( si, sj ) == 0 )
			{
				g_free( sj );
				arr->pdata[j] = NULL;
			}
		}

	return arr;
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


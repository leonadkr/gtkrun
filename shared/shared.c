#include <glib.h>
#include <gio/gio.h>
#include "shared.h"

#define DEFAULT_CACHE_ARRAY_SIZE 32
#define DEFAULT_ENV_ARRAY_SIZE 4096
#define DEFAULT_COMPARED_ARRAY_SIZE 1

static gchar*
duplicate_string(
	gchar *str,
	gpointer user_data )
{
	return g_strdup( str );
}

static gint
compare_strings_by_pointers(
	gchar **s1,
	gchar **s2 )
{
	return g_strcmp0( *s1, *s2 );
}

static gboolean
equal_strings(
	gchar *s1,
	gchar *s2 )
{
	return g_strcmp0( s1, s2 ) == 0 ? TRUE : FALSE;
}

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
	guint i, add_num;
	gchar *filename;
	gssize text_len;

	/* nothing to do */
	if( filenames == NULL || text == NULL )
		return;

	/* nothing to do */
	text_len = (gssize)strlen( text );
	if( text_len == 0 )
		return;

	domain_found = FALSE;
	add_num = 0;
	for( i = 0; i < filenames->len; ++i )
	{
		filename = (gchar*)filenames->pdata[i];

		/* for the filenames is sorted,*/
		/* comparable items will be placed in the certain domain */
		if( g_strstr_len( filename, text_len, text ) != NULL )
		{
			g_ptr_array_add( arr, filename );
			add_num++;
			domain_found = TRUE;
		}
		else if( domain_found )
			break;

		/* add no more count of items */
		if( count > 0 && add_num >= count )
			break;
	}
}

/* add file names to filenames from directory dir */
/* and exclude items in excl_arr */
static void
filename_array_add_from_path(
	GPtrArray *filenames,
	gchar *dirname,
	GPtrArray *excl_arr,
	GError **error )
{
	const gchar *filename;
	GFile *dir;
	GFileEnumerator *direnum;
	GFileInfo *fileinfo;
	GError *error_loc = NULL;

	g_return_if_fail( filenames != NULL );
	g_return_if_fail( dirname != NULL );
	g_return_if_fail( excl_arr != NULL );

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

		filename = g_file_info_get_name( fileinfo );

		/* ignore items in excl_arr */
		if( g_ptr_array_find_with_equal_func( excl_arr, filename, (GEqualFunc)equal_strings, NULL ) )
			continue;

		g_ptr_array_add( filenames, g_strdup( filename ) );
	}

out2:
	g_file_enumerator_close( direnum, NULL, &error_loc );
	if( error_loc != NULL )
		g_propagate_error( error, error_loc );
	g_object_unref( G_OBJECT( direnum ) );

out1:
	g_object_unref( G_OBJECT( dir ) );
}

static void
gr_shared_set_env_filenames(
	GrShared *self )
{
	const gchar delim[] = ":";

	gchar **patharr, **arr;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( self->path_env == NULL )
		return;

	patharr = g_strsplit( g_getenv( self->path_env ), delim, -1 );
	for( arr = patharr; *arr != NULL; arr++ )
	{
		filename_array_add_from_path( self->env_filenames, *arr, self->cache_filenames, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
		}
	}
	g_ptr_array_sort( self->env_filenames, (GCompareFunc)compare_strings_by_pointers );
	g_strfreev( patharr );
}

static void
gr_shared_set_cache_filenames(
	GrShared *self )
{
	GFile *file;
	GFileInputStream *file_stream;
	GDataInputStream *data_stream;
	gsize size;
	gchar *line;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( self->cache_filepath == NULL )
		return;

	file = g_file_new_for_path( self->cache_filepath );
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
	g_ptr_array_sort( self->cache_filenames, (GCompareFunc)compare_strings_by_pointers );

out2:
	g_object_unref( G_OBJECT( data_stream ) );
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );
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
	self->path_env = NULL;

	self->cache_filenames = g_ptr_array_new_full( DEFAULT_CACHE_ARRAY_SIZE, (GDestroyNotify)g_free );
	self->env_filenames = g_ptr_array_new_full( DEFAULT_ENV_ARRAY_SIZE, (GDestroyNotify)g_free );
	self->compared_array = g_ptr_array_new_full( DEFAULT_COMPARED_ARRAY_SIZE, NULL );

	return self;
}

void
gr_shared_free(
	GrShared *self )
{
	g_return_if_fail( self != NULL );

	g_free( self->cache_filepath );
	g_free( self->config_filepath );
	g_free( self->path_env );
	g_ptr_array_unref( self->cache_filenames );
	g_ptr_array_unref( self->env_filenames );
	g_ptr_array_unref( self->compared_array );
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
	shared->path_env = g_strdup( self->path_env );

	shared->cache_filenames = g_ptr_array_copy( self->cache_filenames, (GCopyFunc)duplicate_string, NULL );
	shared->env_filenames = g_ptr_array_copy( self->env_filenames, (GCopyFunc)duplicate_string, NULL );
	shared->compared_array = g_ptr_array_copy( self->cache_filenames, NULL, NULL );

	return shared;
}

void
gr_shared_setup_private(
	GrShared *self )
{
	g_return_if_fail( self != NULL );

	gr_shared_set_cache_filenames( self );
	gr_shared_set_env_filenames( self );
	g_ptr_array_set_size( self->compared_array, self->cache_filenames->len +self->env_filenames->len + 1 );
}

/* don't g_free() result */
gchar*
gr_shared_get_compared_string(
	GrShared *self,
	const gchar *text )
{
	gchar *s;
	GPtrArray *arr;

	arr = self->compared_array;
	arr->len = 0;

	if( !self->no_cache )
		set_compared_array( arr, self->cache_filenames, text, 1 );

	if( arr->len < 1 )
		set_compared_array( arr, self->env_filenames, text, 1 );

	if( arr->len < 1 )
		s = NULL;
	else
		s = (gchar*)arr->pdata[0];

	return s;
}

/* don't unref() result */
GPtrArray*
gr_shared_get_compared_array(
	GrShared *self,
	const gchar *text )
{
	GPtrArray *arr;

	arr = self->compared_array;
	arr->len = 0;

	if( !self->no_cache )
		set_compared_array( arr, self->cache_filenames, text, -1 );
	set_compared_array( arr, self->env_filenames, text, -1 );

	/* NULL-termination for inner array */
	g_ptr_array_add( arr, NULL );

	return arr;
}

void
gr_shared_store_command_to_cache(
	GrShared *self,
	const gchar *command )
{
	const gchar eol[] = "\n";

	GFile *file, *dir;
	GFileOutputStream *file_stream;
	GDataOutputStream *data_stream;
	gchar *line;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( command == NULL || self->no_cache )
		return;

	/* if command is in cache, do nothing */
	if( g_ptr_array_find_with_equal_func( self->cache_filenames, command, (GEqualFunc)equal_strings, NULL ) )
		return;

	file = g_file_new_for_path( self->cache_filepath );
	dir = g_file_get_parent( file );

	/* create cache dir, if it does not exist */
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

	/* open or create cache file with append mode */
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

	/* store command to cache file */
	line = g_strconcat( command, eol, NULL );
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


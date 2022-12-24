#include <glib.h>
#include <gio/gio.h>
#include "config.h"
#include "shared.h"

#define DEFAULT_STORED_ARRAY_SIZE 32
#define DEFAULT_ENV_ARRAY_SIZE 4096

static gint
pstrcmp0(
	gchar **s1,
	gchar **s2 )
{
	g_return_val_if_fail( s1 != NULL, 0 );
	g_return_val_if_fail( s2 != NULL, 0 );

	return g_strcmp0( *s1, *s2 );
}

static gboolean
strequal(
	gchar *s1,
	gchar *s2 )
{
	return g_strcmp0( s1, s2 ) == 0 ? TRUE : FALSE;
}

static gint
strncmp0(
	const gchar *s1,
	const gchar *s2,
	gint n )
{
	if( s1 == NULL || s2 == NULL )
		n = -1;
	
	if( n < 0 )
		return g_strcmp0( s1, s2 );

	return strncmp( s1, s2, n );
}

static void
ptr_array_unref_null(
	GPtrArray *self )
{
	if( self == NULL )
		return;
	
	g_ptr_array_unref( self );
}

static void
ptr_array_add_unique_string(
	GPtrArray *self,
	gchar *str )
{
	g_return_if_fail( self != NULL );

	/* ignore string if it is already in array */
	if( g_ptr_array_find_with_equal_func( self, str, (GEqualFunc)strequal, NULL ) )
			return;

	g_ptr_array_add( self, str );
}

/* add strings equal to text from sour to dest */
/* but not more than count */
/* if count < 0, add all equals */
/* avoiding string duplication */
/* sour should be sorted */
static void
set_compared_array(
	GPtrArray *dest,
	GPtrArray *sour,
	const gchar *str,
	gint count )
{
	gint start, end, med;
	gint cmp, str_len, add_num;

	/* nothing to do */
	if( str == NULL || sour == NULL || sour->len < 1 || ( str_len = (gint)strlen( str ) ) == 0 )
		return;

	/* find domain containing str in sour */
	start = 0;
	end = sour->len - 1;
	while( start <= end )
	{
		/* check the median item of sour */
		med = start + ( end - start ) / 2;
		cmp = strncmp0( (gchar*)sour->pdata[med], str, str_len );

		if( cmp == 0 )
		{
			/* find beginning of the domain */
			if( med == 0 )
				start = 0;
			else
				for( start = med - 1; start >= 0; start-- )
					if( strncmp0( (gchar*)sour->pdata[start], str, str_len ) != 0 )
					{
						start++;
						break;
					}

			/* adds items to dest, but no more than count */
			for( add_num = 0; start <= med && ( count < 0 || add_num < count ); start++, add_num++ )
				ptr_array_add_unique_string( dest, (gchar*)sour->pdata[start] );

			/* check if it has added enough */
			if( count > 0 && add_num >= count )
				break;

			/* add remained items, but no more than count - ( med - start ) */
			if( med < sour->len - 1 )
				for( end = med + 1; end <= sour->len - 1 && ( count < 0 || add_num < count ); end++ )
				{
					if( strncmp0( (gchar*)sour->pdata[end], str, str_len ) != 0 )
						break;
					else
					{
						ptr_array_add_unique_string( dest, (gchar*)sour->pdata[end] );
						add_num++;
					}
				}

			break;
		}
		else if( cmp < 0 )
			start = med + 1;
		else
			end = med - 1;
	}
}

/* add file names from directory dirpath into array self */
static void
ptr_array_add_from_dirpath(
	GPtrArray *self,
	gchar *dirpath,
	GError **error )
{
	GFile *dir;
	GFileEnumerator *direnum;
	GFileInfo *fileinfo;
	GError *loc_error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( dirpath == NULL )
		return;

	dir = g_file_new_for_path( dirpath );
	direnum = g_file_enumerate_children(
		dir,
		G_FILE_ATTRIBUTE_STANDARD_NAME,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		goto out1;
	}

	while( TRUE )
	{
		g_file_enumerator_iterate( direnum, &fileinfo, NULL, NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			goto out2;
		}

		if( fileinfo == NULL )
			break;

		g_ptr_array_add( self, g_strdup( g_file_info_get_name( fileinfo ) ) );
	}

out2:
	g_file_enumerator_close( direnum, NULL, &loc_error );
	if( loc_error != NULL )
		g_propagate_error( error, loc_error );
	g_object_unref( G_OBJECT( direnum ) );

out1:
	g_object_unref( G_OBJECT( dir ) );
}

static guint64
get_filepath_modification_time_uint64(
	const gchar *filepath,
	GError **error )
{
	GFile *file;
	GFileInfo *file_info;
	guint64 seconds;
	GError *loc_error = NULL;

	g_return_val_if_fail( filepath != NULL, 0 );

	file = g_file_new_for_path( filepath );

	file_info = g_file_query_info( file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE, NULL, &loc_error );
	if( loc_error != NULL )
	{
		/* if file does not exist, just set the modification time to 0 */
		if( loc_error->code != G_IO_ERROR_NOT_FOUND )
			g_propagate_error( error, loc_error );
		else
			g_clear_error( &loc_error );
		seconds = 0;
		goto out;
	}
	seconds = g_file_info_get_attribute_uint64( file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED );
	g_object_unref( G_OBJECT( file_info ) );

out:
	g_object_unref( G_OBJECT( file ) );

	return seconds;
}

/* create cache dir, if it does not exist */
static gboolean
gr_shared_create_cache_dir(
	GrShared *self )
{
	GFile *dir;
	gboolean ret = TRUE;
	GError *error = NULL;

	g_return_val_if_fail( self != NULL, FALSE );

	dir = g_file_new_for_path( self->cache_dir );
	g_file_make_directory_with_parents( dir, NULL, &error );
	if( error != NULL  )
	{
		if( error->code != G_IO_ERROR_EXISTS )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
			ret = FALSE;
		}
		g_clear_error( &error );
	}
	g_object_unref( G_OBJECT( dir ) );

	return ret;
}

static gboolean
gr_shared_check_env_cache_header_from_stream(
	GrShared *self,
	GInputStream *stream,
	GError **error )
{
	guint64 mod_seconds, header_seconds;
	gchar *envstr;
	gsize envstr_size;
	GStrv path;
	GError *loc_error = NULL;

	g_return_val_if_fail( self != NULL, FALSE );
	g_return_val_if_fail( stream != NULL, FALSE );

	/* read length of string of paths from cache file */
	g_input_stream_read( stream, &envstr_size, sizeof( envstr_size ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return FALSE;
	}

	/* read string of paths from cache file */
	envstr = g_new( gchar, envstr_size );
	g_input_stream_read( stream, envstr, envstr_size, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		g_free( envstr );
		return FALSE;
	}

	/* check whether string of paths in cache is equal to system one */
	if( g_strcmp0( envstr, self->envstr ) != 0 )
	{
		g_free( envstr );
		return FALSE;
	}
	g_free( envstr );
	
	/* check modification time of directories */
	for( path = self->envarr; *path != NULL; path++ )
	{
		mod_seconds = get_filepath_modification_time_uint64( *path, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return FALSE;
		}

		g_input_stream_read( stream, &header_seconds, sizeof( header_seconds ), NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return FALSE;
		}

		if( mod_seconds != header_seconds )
			return FALSE;
	}

	return TRUE;
}

static void
gr_shared_write_env_cache_header_to_stream(
	GrShared *self,
	GOutputStream *stream,
	GError **error )
{
	guint64 mod_seconds;
	gsize envstr_size;
	GStrv path;
	GError *loc_error = NULL;

	g_return_if_fail( self != NULL );
	g_return_if_fail( stream != NULL );

	/* write length of string of paths to cache file */
	envstr_size = strlen( self->envstr );
	g_output_stream_write( stream, &envstr_size, sizeof( envstr_size ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* write string of paths to cache file */
	g_output_stream_write( stream, self->envstr, envstr_size, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* write modification time of directories */
	for( path = self->envarr; *path != NULL; path++ )
	{
		mod_seconds = get_filepath_modification_time_uint64( *path, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return;
		}

		g_output_stream_write( stream, &mod_seconds, sizeof( mod_seconds ), NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return;
		}
	}
}

static void
ptr_array_read_from_stream(
	GPtrArray *self,
	GInputStream *stream,
	GError **error )
{
	guint len, buff_size;
	gchar *buff;
	GError *loc_error = NULL;

	g_return_if_fail( self != NULL && self->len == 0 );
	g_return_if_fail( stream != NULL );
	
	/* read size of array */
	g_input_stream_read( stream, &len, sizeof( len ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* reallocate more size if necessary */
	if( len > self->len )
		g_ptr_array_set_size( self, len );
	self->len = 0;

	/* read data into array */
	while( len-- > 0 )
	{
		g_input_stream_read( stream, &buff_size, sizeof( buff_size ), NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return;
		}

		buff = g_new( gchar, buff_size );
		g_input_stream_read( stream, buff, buff_size, NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			g_free( buff );
			return;
		}

		g_ptr_array_add( self, buff );
	}
}

static void
ptr_array_write_to_stream(
	GPtrArray *self,
	GOutputStream *stream,
	GError **error )
{
	guint i, buff_size;
	gchar *buff;
	GError *loc_error = NULL;

	g_return_if_fail( self != NULL );
	g_return_if_fail( stream != NULL );

	/* write size of array */
	g_output_stream_write( stream, &( self->len ), sizeof( self->len ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* write data of array */
	for( i = 0; i < self->len; ++i )
	{
		buff = (gchar*)self->pdata[i];
		buff_size = (guint)strlen( buff ) + 1;

		g_output_stream_write( stream, &buff_size, sizeof( buff_size ), NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return;
		}

		g_output_stream_write( stream, buff, buff_size, NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return;
		}
	}
}

static gboolean
gr_shared_set_env_array_from_cache(
	GrShared *self )
{
	GFile *file;
	GFileInputStream *file_stream;
	gboolean checked;
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail( self != NULL, FALSE );

	/* nothing to do */
	if( self->no_cache )
		return FALSE;

	/* open file to read header and array */
	file = g_file_new_for_path( self->env_cache_path );
	file_stream = g_file_read( file, NULL, &error );
	if( error != NULL )
	{
		/* the file may not exist */
		if( error->code != G_IO_ERROR_NOT_FOUND )
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		ret = FALSE;
		goto out1;
	}

	/* check header */
	checked = gr_shared_check_env_cache_header_from_stream( self, G_INPUT_STREAM( file_stream ), &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		ret = FALSE;
		goto out2;
	}
	if( !checked )
	{
		ret = FALSE;
		goto out2;
	}

	/* if header is OK, read array */
	ptr_array_read_from_stream( self->env_array, G_INPUT_STREAM( file_stream ), &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		ret = FALSE;
	}

out2:
	g_input_stream_close( G_INPUT_STREAM( file_stream ), NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
	}
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );
	
	return ret;
}

static void
gr_shared_write_env_array_to_cache(
	GrShared *self )
{
	GFile *file;
	GFileOutputStream *file_stream;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( self->no_cache )
		return;

	/* try to create cache dir */
	if( !gr_shared_create_cache_dir( self ) )
		return;

	/* open file to write header and array */
	file = g_file_new_for_path( self->env_cache_path );
	file_stream = g_file_replace( file, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out1;
	}

	/* write header and array to file */
	gr_shared_write_env_cache_header_to_stream( self, G_OUTPUT_STREAM( file_stream ), &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out2;
	}
	ptr_array_write_to_stream( self->env_array, G_OUTPUT_STREAM( file_stream ), &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
	}

out2:
	g_output_stream_close( G_OUTPUT_STREAM( file_stream ), NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
	}
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );
}

static void
gr_shared_set_stored_array(
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
	if( self->stored_path == NULL )
		return;

	/* open file to read stored commands */
	file = g_file_new_for_path( self->stored_path );
	file_stream = g_file_read( file, NULL, &error );
	if( error != NULL )
	{
		/* the file may not exist */
		if( error->code != G_IO_ERROR_NOT_FOUND )
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out1;
	}
	data_stream = g_data_input_stream_new( G_INPUT_STREAM( file_stream ) );

	/* read commands from file */
	while( TRUE )
	{
		line = g_data_input_stream_read_line( data_stream, &size, NULL, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
			g_clear_error( &error );
			goto out2;
		}

		if( line == NULL )
			break;

		/* add only full line */
		line = g_strstrip( line );
		if( strlen( line ) > 0 )
			g_ptr_array_add( self->stored_array, line );
	}
	g_ptr_array_sort( self->stored_array, (GCompareFunc)pstrcmp0 );

out2:
	g_object_unref( G_OBJECT( data_stream ) );
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );
}

static void
gr_shared_set_env_array(
	GrShared *self )
{
	GStrv path;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* try to get array from cache */
	if( gr_shared_set_env_array_from_cache( self ) )
		return;

	/* nothing to do */
	if( self->envstr == NULL )
		return;

	/* add commands from directories in path array */
	for( path = self->envarr; *path != NULL; path++ )
	{
		ptr_array_add_from_dirpath( self->env_array, *path, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
			g_clear_error( &error );
		}
	}
	g_ptr_array_sort( self->env_array, (GCompareFunc)pstrcmp0 );

	/* make new cache for env array */
	gr_shared_write_env_array_to_cache( self );
}

static void
gr_shared_store_command(
	GrShared *self,
	gchar *command )
{
	const gchar eol[] = "\n";

	GFile *file;
	GFileOutputStream *file_stream;
	GDataOutputStream *data_stream;
	gchar *line;
	GError *error = NULL;

	g_return_if_fail( self != NULL );
	g_return_if_fail( command != NULL );
	g_return_if_fail( strlen( command ) != 0 );

	/* nothing to do */
	if( self->no_cache )
		return;

	/* if command is already in stored array, do nothing */
	if( g_ptr_array_find_with_equal_func( self->stored_array, command, (GEqualFunc)strequal, NULL ) )
		return;

	/* try to create cache dir */
	if( !gr_shared_create_cache_dir( self ) )
		return;

	/* open or create file with append mode */
	file = g_file_new_for_path( self->stored_path );
	file_stream = g_file_append_to( file, G_FILE_CREATE_PRIVATE, NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out;
	}
	data_stream = g_data_output_stream_new( G_OUTPUT_STREAM( file_stream ) );

	/* store command to file */
	line = g_strconcat( command, eol, NULL );
	g_data_output_stream_put_string( data_stream, line, NULL, &error );
	g_free( line );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
	}

	g_object_unref( G_OBJECT( data_stream ) );
	g_output_stream_close( G_OUTPUT_STREAM( file_stream ), NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
	}
	g_object_unref( G_OBJECT( file_stream ) );

out:
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
	self->cache_dir = NULL;
	self->no_cache = FALSE;
	self->config_path = NULL;
	self->no_config = FALSE;
	self->env = NULL;

	self->envstr = NULL;
	self->envarr = NULL;
	self->stored_array = NULL;
	self->env_array = NULL;
	self->compared_array = NULL;
	self->stored_path = NULL;
	self->env_cache_path = NULL;

	return self;
}

void
gr_shared_free(
	GrShared *self )
{
	g_return_if_fail( self != NULL );

	g_free( self->cache_dir );
	g_free( self->config_path );
	g_free( self->env );

	g_free( self->envstr );
	g_strfreev( self->envarr );
	ptr_array_unref_null( self->stored_array );
	ptr_array_unref_null( self->env_array );
	ptr_array_unref_null( self->compared_array );
	g_free( self->stored_path );
	g_free( self->env_cache_path );

	g_free( self );
}

void
gr_shared_setup_private(
	GrShared *self )
{
	const gchar delim[] = ":";
	guint len = 0;

	g_return_if_fail( self != NULL );

	g_free( self->envstr );
	self->envstr = g_strdup( g_getenv( self->env ) );

	g_strfreev( self->envarr );
	self->envarr = g_strsplit( self->envstr, delim, -1 );

	if( !self->no_cache )
	{
		g_free( self->stored_path );
		self->stored_path = g_build_filename( self->cache_dir, STORED_FILENAME, NULL );

		g_free( self->env_cache_path );
		self->env_cache_path = g_build_filename( self->cache_dir, ENV_CACHE_FILENAME, NULL );

		ptr_array_unref_null( self->stored_array );
		self->stored_array = g_ptr_array_new_full( DEFAULT_STORED_ARRAY_SIZE, (GDestroyNotify)g_free );
		gr_shared_set_stored_array( self );
		len += self->stored_array->len;
	}

	ptr_array_unref_null( self->env_array );
	self->env_array = g_ptr_array_new_full( DEFAULT_ENV_ARRAY_SIZE, (GDestroyNotify)g_free );
	gr_shared_set_env_array( self );
	len += self->env_array->len;

	ptr_array_unref_null( self->compared_array );
	self->compared_array = g_ptr_array_new_full( len + 1, NULL );
}

/* don't g_free() result */
gchar*
gr_shared_get_compared_string(
	GrShared *self,
	const gchar *str )
{
	gchar *s;
	GPtrArray *arr;

	/* set length to 0 for reusing the array */
	arr = self->compared_array;
	arr->len = 0;

	if( !self->no_cache )
		set_compared_array( arr, self->stored_array, str, 1 );

	if( arr->len < 1 )
		set_compared_array( arr, self->env_array, str, 1 );

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

	/* set length to 0 for reusing the array */
	arr = self->compared_array;
	arr->len = 0;

	if( !self->no_cache )
		set_compared_array( arr, self->stored_array, text, -1 );
	set_compared_array( arr, self->env_array, text, -1 );

	/* NULL-termination to use the inner array as a strv */
	g_ptr_array_add( arr, NULL );

	return arr;
}

void
gr_shared_system_call(
	GrShared *self,
	const gchar* str )
{
	GSubprocessFlags flags;
	GSubprocess *subproc;
	gchar *command;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( str == NULL )
		return;

	command = g_strdup( str );
	g_strstrip( command );

	/* nothing to do */
	if( strlen( command ) == 0 )
	{
		g_free(command );
		return;
	}

	gr_shared_store_command( self, command );

	if( self->silent )
		flags = G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE;
	else
		flags = G_SUBPROCESS_FLAGS_NONE;

	subproc = g_subprocess_new( flags, &error, command, NULL );
	g_free(command );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
	}
	else
		g_object_unref( G_OBJECT( subproc ) );
}


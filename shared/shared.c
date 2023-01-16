#include <glib.h>
#include <gio/gio.h>
#include "config.h"
#include "array.h"
#include "shared.h"

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

/* add string to self */
/* avoiding duplicates */
/* return TRUE if string was added, */
/* else return FALSE */
static gboolean
strv_add_unique_string(
	GStrv self,
	gchar *str )
{
	GStrv s;

	g_return_val_if_fail( self != NULL, FALSE );

	if( str == NULL )
		return FALSE;

	for( s = self; *s != NULL; s++ )
		if( g_strcmp0( *s, str ) == 0 )
			return FALSE;

	*s = str;
	*(++s) = NULL;

	return TRUE;
}

/* add strings equal to text from sour to dest */
/* but not more than count */
/* if count < 0, add all equals */
/* avoiding string duplication */
/* dest should be created */
/* size of dest should be not less size */
/* sour should be sorted */
/* return number of added items */
static gsize
set_compared_array(
	GStrv dest,
	GrArray *sour,
	gsize size,
	const gchar *str,
	gssize count )
{
	gssize start, end, med;
	gsize str_len, add_num;
	gint cmp;

	/* nothing to do */
	if( str == NULL || sour == NULL || size < 1 || ( str_len = strlen( str ) ) == 0 )
		return 0;

	/* find domain containing str in sour */
	add_num = 0;
	start = 0;
	end = size - 1;
	while( start <= end )
	{
		/* check the median item of sour */
		med = start + ( end - start ) / 2;
		cmp = strncmp0( (gchar*)sour->p[med], str, str_len );

		if( cmp == 0 )
		{
			/* find beginning of the domain */
			if( med == 0 )
				start = 0;
			else
				for( start = med - 1; start >= 0; start-- )
					if( strncmp0( (gchar*)sour->p[start], str, str_len ) != 0 )
					{
						start++;
						break;
					}

			/* adds items to dest, but no more than count */
			for( ; start <= med && ( count < 0 || add_num < count ); start++ )
				if( strv_add_unique_string( dest, sour->p[start] ) )
					add_num++;

			/* check if it has added enough */
			if( count > 0 && add_num >= count )
				break;

			/* add remained items, but no more than count - ( med - start ) */
			if( med < size - 1 )
				for( end = med + 1; end <= size - 1 && ( count < 0 || add_num < count ); end++ )
				{
					if( strncmp0( sour->p[end], str, str_len ) != 0 )
						break;
					else if( strv_add_unique_string( dest, sour->p[end] ) )
							add_num++;
				}

			break;
		}
		else if( cmp < 0 )
			start = med + 1;
		else
			end = med - 1;
	}

	return add_num;
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

static guint64
add_file_names_to_list_from_directory(
	GSList **list,
	gchar *dirpath,
	GError **error )
{
	GFile *dir;
	GFileEnumerator *direnum;
	GFileInfo *fileinfo;
	guint64 seconds;
	GError *loc_error = NULL;

	/* nothing to do */
	if( dirpath == NULL )
		return 0;

	dir = g_file_new_for_path( dirpath );
	fileinfo = g_file_query_info( dir, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE, NULL, &loc_error );
	if( loc_error != NULL )
	{
		/* if file does not exist, just leave the modification time 0 */
		if( loc_error->code != G_IO_ERROR_NOT_FOUND )
			g_propagate_error( error, loc_error );
		else
			g_clear_error( &loc_error );
		seconds = 0;
		goto out1;
	}
	seconds = g_file_info_get_attribute_uint64( fileinfo, G_FILE_ATTRIBUTE_TIME_MODIFIED );
	g_object_unref( G_OBJECT( fileinfo ) );

	direnum = g_file_enumerate_children(
		dir,
		G_FILE_ATTRIBUTE_STANDARD_NAME,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		seconds = 0;
		goto out1;
	}

	while( TRUE )
	{
		g_file_enumerator_iterate( direnum, &fileinfo, NULL, NULL, &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			seconds = 0;
			goto out2;
		}

		if( fileinfo == NULL )
			break;

		*list = g_slist_prepend( *list, g_string_new( g_file_info_get_name( fileinfo ) ) );
	}

out2:
	g_file_enumerator_close( direnum, NULL, &loc_error );
	if( loc_error != NULL )
		g_propagate_error( error, loc_error );
	g_object_unref( G_OBJECT( direnum ) );

out1:
	g_object_unref( G_OBJECT( dir ) );

	return seconds;
}

static gboolean
create_cache_dir(
	gchar *path )
{
	GFile *dir;
	gboolean ret = TRUE;
	GError *error = NULL;

	if( path == NULL )
		return FALSE;

	dir = g_file_new_for_path( path );
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
	guint64 seconds;
	gchar *envstr;
	gsize i, envstr_size;
	GError *loc_error = NULL;

	g_return_val_if_fail( self != NULL, FALSE );
	g_return_val_if_fail( G_IS_INPUT_STREAM( stream ), FALSE );

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

	/* check whether string of paths in cache is equal to loaded one */
	if( g_strcmp0( envstr, self->envstr ) != 0 )
	{
		g_free( envstr );
		return FALSE;
	}
	g_free( envstr );

	/* read modification time array */
	g_input_stream_read( stream, self->dirmodarr, self->envarr_len * sizeof( self->dirmodarr[0] ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return FALSE;
	}

	/* check modification time of directories */
	for( i = 0; i < self->envarr_len; ++i )
	{
		seconds = get_filepath_modification_time_uint64( self->envarr[i], &loc_error );
		if( loc_error != NULL )
		{
			g_propagate_error( error, loc_error );
			return FALSE;
		}

		if( seconds != self->dirmodarr[i] )
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
	gsize envstr_size;
	GError *loc_error = NULL;

	g_return_if_fail( self != NULL );
	g_return_if_fail( G_IS_OUTPUT_STREAM( stream ) );

	/* write length of string of paths to cache file */
	envstr_size = strlen( self->envstr ) + 1;
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
	g_output_stream_write( stream, self->dirmodarr, self->envarr_len * sizeof( self->dirmodarr[0] ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}
}

static void
read_array_from_stream(
	GrArray *self,
	GInputStream *stream,
	GError **error )
{
	gsize i;
	GError *loc_error = NULL;

	g_return_if_fail( self != NULL );
	g_return_if_fail( G_IS_INPUT_STREAM( stream ) );
	
	/* read size of pointer array */
	g_input_stream_read( stream, &( self->n ), sizeof( self->n ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* create and read pointer array */
	self->p = g_new( gchar*, self->n );
	g_input_stream_read( stream, self->p, self->n * sizeof( self->p[0] ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* read size of data */
	g_input_stream_read( stream, &( self->size ), sizeof( self->size ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* create and read data array */
	self->data = g_new( gchar, self->size );
	g_input_stream_read( stream, self->data, self->size, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* change offset for pointer array */
	for( i = 0; i < self->n; ++i )
		self->p[i] = (gchar*)( (gsize)self->p[i] + (gsize)self->data );
}

static void
write_array_to_stream(
	GrArray *self,
	GOutputStream *stream,
	GError **error )
{
	gsize i;
	GError *loc_error = NULL;

	g_return_if_fail( self != NULL );
	g_return_if_fail( G_IS_OUTPUT_STREAM( stream ) );

	/* write size of pointer array */
	g_output_stream_write( stream, &( self->n ), sizeof( self->n ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* write pointer array without offset */
	for( i = 0; i < self->n; ++i )
		self->p[i] = (gchar*)( (gsize)self->p[i] - (gsize)self->data );
	g_output_stream_write( stream, self->p, self->n * sizeof( self->p[0] ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		for( i = 0; i < self->n; ++i )
			self->p[i] = (gchar*)( (gsize)self->p[i] + (gsize)self->data );
		return;
	}
	for( i = 0; i < self->n; ++i )
		self->p[i] = (gchar*)( (gsize)self->p[i] + (gsize)self->data );

	/* write size of data */
	g_output_stream_write( stream, &( self->size ), sizeof( self->size ), NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
	}

	/* write data */
	g_output_stream_write( stream, self->data, self->size, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return;
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

	read_array_from_stream( self->env_array, G_INPUT_STREAM( file_stream ), &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		ret = FALSE;
		goto out2;
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
	if( !create_cache_dir( self->cache_dir ) )
		return;

	file = g_file_new_for_path( self->env_cache_path );
	file_stream = g_file_replace( file, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out1;
	}

	gr_shared_write_env_cache_header_to_stream( self, G_OUTPUT_STREAM( file_stream ), &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out2;
	}

	write_array_to_stream( self->env_array, G_OUTPUT_STREAM( file_stream ), &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out2;
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
	GrArray *arr;
	gsize i;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( self->stored_path == NULL )
		return;

	arr = self->stored_array;

	/* read file */
	file = g_file_new_for_path( self->stored_path );
	g_file_load_contents( file, NULL, &( arr->data ), &( arr->size ), NULL, &error );
	if( error != NULL )
	{
		/* the file may not exist */
		if( error->code != G_IO_ERROR_NOT_FOUND )
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
		g_clear_error( &error );
		goto out;
	}

	/* replace EOL with '\0' */
	arr->data = g_strdelimit( arr->data, EOL, '\0' );

	/* count number of strings */
	for( arr->n = 0, i = 0; i < arr->size; ++i )
		if( arr->data[i] == '\0' )
			arr->n++;

	/* create pointer array */
	arr->p = g_new( gchar*, arr->n );

	/* connect pointers to data */
	arr->p[0] = arr->data;
	for( i = 1; i < arr->n; ++i )
		arr->p[i] = strchr( arr->p[i-1], (int)'\0' ) + 1;

	/* strip string in array */
	for( i = 0; i < arr->n; ++i )
		arr->p[i] = g_strstrip( arr->p[i] );

	gr_array_sort( arr );

out:
	g_object_unref( G_OBJECT( file ) );
}

static void
gr_shared_set_env_array(
	GrShared *self )
{
	GrArray *arr;
	GSList *list, *l;
	gsize i;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* try to get array from cache */
	if( gr_shared_set_env_array_from_cache( self ) )
		return;

	/* nothing to do */
	if( self->envarr == NULL || self->envarr[0] == NULL )
		return;

	/* get list of file names */
	list = NULL;
	for( i = 0; i < self->envarr_len; ++i )
	{
		self->dirmodarr[i] = add_file_names_to_list_from_directory( &list, self->envarr[i], &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE", error->message, NULL );
			g_clear_error( &error );
		}
	}

	arr = self->env_array;

	/* get size of data array and pointer array */
	arr->size = 0;
	arr->n = 0;
	for( l = list; l != NULL; l = l->next )
	{
		arr->size += ((GString*)l->data)->len + 1;
		arr->n++;
	}

	/* create and set arrays */
	arr->data = g_new( gchar, arr->size );
	arr->p = g_new( gchar*, arr->n );
	arr->n = 0;
	i = 0;
	for( l = list; l != NULL; l = l->next )
	{
		arr->p[arr->n++] = &( arr->data[i] );
		memcpy( &( arr->data[i] ), ((GString*)l->data)->str, ((GString*)l->data)->len + 1 );
		i += ((GString*)l->data)->len + 1;
		g_string_free( (GString*)l->data, TRUE );
	}
	g_slist_free( list );

	gr_array_sort( arr );

	/* make new cache for env_array */
	gr_shared_write_env_array_to_cache( self );
}

static void
gr_shared_store_command(
	GrShared *self,
	gchar *command )
{
	GFile *file;
	GFileOutputStream *file_stream;
	GDataOutputStream *data_stream;
	gchar *line;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( self->no_cache || command == NULL || strlen( command ) == 0 )
		return;

	/* if command is already in stored_array, do nothing */
	if( gr_array_find_equal( self->stored_array, command ) )
		return;

	/* try to create cache dir */
	if( !create_cache_dir( self->cache_dir ) )
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
	line = g_strconcat( command, EOL, NULL );
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
	self->dirmodarr = NULL;
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
	if( self == NULL )
		return;

	g_free( self->cache_dir );
	g_free( self->config_path );
	g_free( self->env );

	g_free( self->envstr );
	g_strfreev( self->envarr );
	g_free( self->dirmodarr );
	gr_array_free( self->stored_array );
	gr_array_free( self->env_array );
	g_free( self->compared_array );
	g_free( self->stored_path );
	g_free( self->env_cache_path );

	g_free( self );
}

void
gr_shared_setup_private(
	GrShared *self )
{
	const gchar delim[] = ":";
	const gchar *s = NULL;
	gsize len = 0;

	g_return_if_fail( self != NULL );

	g_free( self->envstr );
	s = g_getenv( self->env );
	self->envstr = s == NULL ? g_strdup( "" ) : g_strdup( s );

	g_strfreev( self->envarr );
	self->envarr = g_strsplit( self->envstr, delim, -1 );
	self->envarr_len = g_strv_length( self->envarr );

	self->dirmodarr = g_new( guint64, self->envarr_len );

	if( !self->no_cache )
	{
		g_free( self->stored_path );
		self->stored_path = g_build_filename( self->cache_dir, STORED_FILENAME, NULL );

		g_free( self->env_cache_path );
		self->env_cache_path = g_build_filename( self->cache_dir, ENV_CACHE_FILENAME, NULL );

		gr_array_free( self->stored_array );
		self->stored_array = gr_array_new();
		gr_shared_set_stored_array( self );
		len += self->stored_array->n;
	}

	gr_array_free( self->env_array );
	self->env_array = gr_array_new();
	gr_shared_set_env_array( self );
	len += self->env_array->n;

	g_free( self->compared_array );
	self->compared_array = g_new( gchar*, len + 1 ); /* +1 for NULL-termination */
	self->compared_array[0] = NULL;
}

/* don't g_free() result */
gchar*
gr_shared_get_compared_string(
	GrShared *self,
	const gchar *str )
{
	/* reset length of compared_array */
	self->compared_array[0] = NULL;

	if( !self->no_cache )
		set_compared_array( self->compared_array, self->stored_array, self->stored_array->n, str, 1 );

	if( self->compared_array[0] == NULL )
		set_compared_array( self->compared_array, self->env_array, self->env_array->n, str, 1 );

	return self->compared_array[0];
}

/* don't g_strfreev() result */
GStrv
gr_shared_get_compared_array(
	GrShared *self,
	const gchar *str )
{
	/* reset length of compared_array */
	self->compared_array[0] = NULL;

	if( !self->no_cache )
		set_compared_array( self->compared_array, self->stored_array, self->stored_array->n, str, -1 );
	set_compared_array( self->compared_array, self->env_array, self->env_array->n, str, -1 );

	return self->compared_array;
}

void
gr_shared_system_call(
	GrShared *self,
	const gchar* cmd )
{
	GSubprocessFlags flags;
	GSubprocess *subproc;
	gchar *command;
	GError *error = NULL;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( cmd == NULL )
		return;

	/* strip input to nice command string */
	command = g_strdup( cmd );
	g_strstrip( command );

	/* nothing to do */
	if( strlen( command ) == 0 )
	{
		g_free( command );
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


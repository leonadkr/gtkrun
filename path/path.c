#include <glib.h>
#include <gio/gio.h>
#include "config.h"


static GList*
gr_path_prepend_filename_list(
	GList *filename_list,
	gchar *dirname )
{
	GFile *dir;
	GFileEnumerator *direnum;
	GFileInfo *fileinfo;
	GList *fn_list, *ret;
	GError *error = NULL;

	dir = g_file_new_for_path( dirname );
	direnum = g_file_enumerate_children(
		dir,
		G_FILE_ATTRIBUTE_STANDARD_NAME,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		ret = filename_list;
		goto out1;
	}

	fn_list = NULL;
	while( TRUE )
	{
		g_file_enumerator_iterate( direnum, &fileinfo, NULL, NULL, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			g_list_free_full( fn_list, (GDestroyNotify)g_free );
			ret = filename_list;
			goto out2;
		}

		if( fileinfo == NULL )
			break;

		fn_list = g_list_prepend( fn_list, g_strdup( g_file_info_get_name( fileinfo ) ) );
	}
	ret = g_list_concat( fn_list, filename_list );

out2:
	g_object_unref( G_OBJECT( direnum ) );

out1:
	g_object_unref( G_OBJECT( dir ) );

	return ret;
}


GList*
gr_path_get_compared_list(
	GList *filename_list,
	const gchar *text )
{
	gchar *s;
	GList *l, *compared_list;
	gint text_len;

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

GList*
gr_path_get_filename_list_from_env(
	const gchar *pathenv )
{
	const gchar delim[] = ":";

	const gchar *pathstr;
	gchar **patharr, **arr;
	GList *filename_list;

	g_return_val_if_fail( pathenv != NULL, NULL );

	pathstr = g_getenv( pathenv );
	patharr = g_strsplit( pathstr, delim, -1 );

	filename_list = NULL;
	for( arr = patharr; arr[0] != NULL; arr++ )
		filename_list = gr_path_prepend_filename_list( filename_list, arr[0] );
	g_strfreev( patharr );

	filename_list = g_list_sort( filename_list, (GCompareFunc)g_strcmp0 );

	return filename_list;
}

GList*
gr_path_get_filename_list_from_cache(
	gchar *cache_filepath )
{
	GFile *file;
	GFileInputStream *file_stream;
	GDataInputStream *data_stream;
	GList *filename_list;
	gsize size;
	gchar *line;
	GList *ret = NULL;
	GError *error = NULL;

	g_return_val_if_fail( cache_filepath != NULL, NULL );

	file = g_file_new_for_path( cache_filepath );
	file_stream = g_file_read( file, NULL, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		ret = NULL;
		goto out1;
	}
	data_stream = g_data_input_stream_new( G_INPUT_STREAM( file_stream ) );

	filename_list = NULL;
	while( TRUE )
	{
		line = g_data_input_stream_read_line( data_stream, &size, NULL, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			g_list_free_full( filename_list, (GDestroyNotify)g_free );
			ret = NULL;
			goto out2;
		}

		if( line == NULL )
			break;

		filename_list = g_list_prepend( filename_list, line );
	}
	ret = g_list_reverse( filename_list );

out2:
	g_object_unref( G_OBJECT( data_stream ) );
	g_object_unref( G_OBJECT( file_stream ) );

out1:
	g_object_unref( G_OBJECT( file ) );

	return ret;
}

void
gr_path_store_command_to_cache(
	gchar *cache_filepath,
	const gchar *command )
{
	GFile *file, *dir;
	GFileOutputStream *file_stream;
	GDataOutputStream *data_stream;
	gchar *line;
	GError *error = NULL;

	g_return_if_fail( cache_filepath != NULL );
	g_return_if_fail( command != NULL );

	file = g_file_new_for_path( cache_filepath );
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

	line = g_strdup_printf( "%s\n", command );
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


#include <glib.h>
#include <gio/gio.h>
#include "config.h"

/*
	private
*/
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


/*
	public
*/
GList*
gr_path_get_filename_list(
	void )
{
	const gchar delim[] = ":";
	const gchar *pathstr;
	gchar **patharr, **arr;
	GList *filename_list;

	pathstr = g_getenv( "PATH" );
	patharr = g_strsplit( pathstr, delim, -1 );

	filename_list = NULL;
	for( arr = patharr; arr[0] != NULL; arr++ )
		filename_list = gr_path_prepend_filename_list( filename_list, arr[0] );
	g_strfreev( patharr );

	filename_list = g_list_sort( filename_list, (GCompareFunc)g_strcmp0 );

	return filename_list;
}

GList*
gr_path_get_compare_list(
	GList *filename_list,
	const gchar *text )
{
	gchar *s;
	GList *l, *compare_list;
	gint text_len;

	g_return_val_if_fail( filename_list != NULL, NULL );
	g_return_val_if_fail( text != NULL, NULL );

	text_len = strlen( text );
	if( text_len == 0 )
		return NULL;

	compare_list = NULL;
	for( l = filename_list; l != NULL; l = l->next )
	{
		s = (gchar*)l->data;
		if( g_strstr_len( s, text_len, text ) != NULL )
			compare_list = g_list_prepend( compare_list, g_strdup( s ) );
	}

	return g_list_reverse( compare_list );
}


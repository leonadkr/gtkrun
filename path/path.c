#include <gio/gio.h>
#include "path.h"


struct _GrPath
{
	GObject parent_instance;

	gchar *cache_filepath;
	gchar *pathenv;
	GList *env_filename_list;
	GList *cache_filename_list;
};
typedef struct _GrPath GrPath;

enum _GrPathPropertyID
{
	PROP_0, /* 0 is reserved for GObject */

	PROP_CACHE_FILEPATH,
	PROP_PATHENV,

	N_PROPS
};
typedef enum _GrPathPropertyID GrPathPropertyID;


static GParamSpec *object_props[N_PROPS] = { NULL, };

static GList* prepend_filename_list( GList *filename_list, gchar *dirname );
static GList* get_filename_list_from_env( const gchar *pathenv );
static GList* get_filename_list_from_cache( gchar *cache_filepath );
static GList* get_compared_list( GList *filename_list, const gchar *text );

G_DEFINE_FINAL_TYPE( GrPath, gr_path, G_TYPE_OBJECT )

static void
gr_path_init(
	GrPath *self )
{
	self->cache_filepath = NULL;
	self->pathenv = NULL;
	self->cache_filename_list = NULL;
	self->env_filename_list = NULL;
}

static void
gr_path_finalize(
	GObject *object )
{
	GrPath *self = GR_PATH( object );

	g_free( self->cache_filepath );
	g_free( self->pathenv );
	g_list_free_full( self->cache_filename_list, (GDestroyNotify)g_free );
	g_list_free_full( self->env_filename_list, (GDestroyNotify)g_free );

	G_OBJECT_CLASS( gr_path_parent_class )->finalize( object );
}

static void
gr_path_get_property(
	GObject *object,	
	guint prop_id,	
	GValue *value,	
	GParamSpec *pspec )
{
	GrPath *self = GR_PATH( object );

	switch( (GrPathPropertyID)prop_id )
	{
		case PROP_CACHE_FILEPATH:
			g_value_set_string( value, self->cache_filepath );
			break;
		case PROP_PATHENV:
			g_value_set_string( value, self->pathenv );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_path_set_property(
	GObject *object,
	guint prop_id,
	const GValue *value,
	GParamSpec *pspec )
{
	GrPath *self = GR_PATH( object );

	switch( (GrPathPropertyID)prop_id )
	{
		case PROP_CACHE_FILEPATH:
			g_free( self->cache_filepath );
			self->cache_filepath = g_value_dup_string( value );
			g_list_free_full( self->cache_filename_list, (GDestroyNotify)g_free );
			self->cache_filename_list = get_filename_list_from_cache( self->cache_filepath );
			break;
		case PROP_PATHENV:
			g_free( self->pathenv );
			self->pathenv = g_value_dup_string( value );
			g_list_free_full( self->env_filename_list, (GDestroyNotify)g_free );
			self->env_filename_list = get_filename_list_from_env( self->pathenv );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_path_class_init(
	GrPathClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );

	object_class->get_property = gr_path_get_property;
	object_class->set_property = gr_path_set_property;
	object_props[PROP_CACHE_FILEPATH] = g_param_spec_string(
		"cache-filepath",
		"Cache file path",
		"Path to the cache file",
		NULL,
		G_PARAM_READWRITE );
	object_props[PROP_PATHENV] = g_param_spec_string(
		"pathenv",
		"Environment path array",
		"Environment variable storing an array of pathes to binaries",
		NULL,
		G_PARAM_READWRITE );
	g_object_class_install_properties( object_class, N_PROPS, object_props );

	object_class->finalize = gr_path_finalize;
}

static GList*
prepend_filename_list(
	GList *filename_list,
	gchar *dirname )
{
	GFile *dir;
	GFileEnumerator *direnum;
	GFileInfo *fileinfo;
	GList *fn_list, *ret;
	GError *error = NULL;

	/* nothing to do */
	if( dirname == NULL )
		return NULL;

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

static GList*
get_filename_list_from_env(
	const gchar *pathenv )
{
	const gchar delim[] = ":";

	const gchar *pathstr;
	gchar **patharr, **arr;
	GList *filename_list;

	/* nothing to do */
	if( pathenv == NULL )
		return NULL;

	pathstr = g_getenv( pathenv );
	patharr = g_strsplit( pathstr, delim, -1 );

	filename_list = NULL;
	for( arr = patharr; arr[0] != NULL; arr++ )
		filename_list = prepend_filename_list( filename_list, arr[0] );
	g_strfreev( patharr );

	filename_list = g_list_sort( filename_list, (GCompareFunc)g_strcmp0 );

	return filename_list;
}

static GList*
get_filename_list_from_cache(
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

	/* nothing to do */
	if( cache_filepath == NULL )
		return NULL;

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


GrPath*
gr_path_new(
	const gchar *cache_filepath,
	const gchar *pathenv )
{
	return GR_PATH( g_object_new( GR_TYPE_PATH,
		"cache-filepath", cache_filepath,
		"pathenv", pathenv,
		NULL ) );
}

GList*
gr_path_get_compared_list(
	GrPath *self,
	const gchar *text )
{
	GList *cache_list, *env_list;
	GList *l, *exclist;

	g_return_val_if_fail( GR_IS_PATH( self ), NULL );

	/* nothing to do */
	if( text == NULL )
		return NULL;

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
gr_path_store_command_to_cache(
	GrPath *self,
	const gchar *command )
{
	GFile *file, *dir;
	GFileOutputStream *file_stream;
	GDataOutputStream *data_stream;
	gchar *line;
	GList *l;
	GError *error = NULL;

	g_return_if_fail( GR_IS_PATH( self ) );

	/* nothing to do */
	if( command == NULL )
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
gr_path_system_call(
	GrPath *self,
	const gchar* command )
{
	GSubprocess *subproc;
	GError *error = NULL;

	g_return_if_fail( GR_IS_PATH( self ) );

	/* nothing to do */
	if( command == NULL )
		return;

	subproc = g_subprocess_new(
		G_SUBPROCESS_FLAGS_NONE,
		&error,
		command,
		NULL );
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


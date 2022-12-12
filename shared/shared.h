#ifndef SHARED_H
#define SHARED_H

#include <glib.h>

struct _GrShared
{
	/* options */
	gboolean silent;
	gint width;
	gint height;
	gint max_height;
	gboolean max_height_set;
	gchar *cache_filepath;
	gboolean no_cache;
	gchar *config_filepath;
	gboolean no_config;
	gchar *path_env;

	/* shared */
	GPtrArray *env_filenames;
	GPtrArray *cache_filenames;
	GPtrArray *compared_array;
};
typedef struct _GrShared GrShared;

GrShared* gr_shared_new( void );
void gr_shared_free( GrShared *self );
GrShared* gr_shared_dup( GrShared *self );
void gr_shared_setup( GrShared *self );
gchar* gr_shared_get_compared_string( GrShared *self, const gchar *text );
GPtrArray* gr_shared_get_compared_array( GrShared *self, const gchar *text );
void gr_shared_store_command_to_cache( GrShared *self, const gchar *command );
void gr_shared_system_call( GrShared *self, const gchar* command );

#endif

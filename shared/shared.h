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
	gchar *cache_filepath;
	gboolean no_cache;
	gchar *config_filepath;
	gboolean no_config;

	/* shared */
	gboolean max_height_set;
	GList *env_filename_list;
	GList *cache_filename_list;
};
typedef struct _GrShared GrShared;

GrShared* gr_shared_new( void );
void gr_shared_free( GrShared *self );
GrShared* gr_shared_dup( GrShared *self );
GList* gr_shared_get_filename_list_from_env( const gchar *pathenv );
GList* gr_shared_get_filename_list_from_cache( gchar *cache_filepath );
GList* gr_shared_get_compared_list( GrShared *self, const gchar *text );
void gr_shared_store_command_to_cache( GrShared *self, const gchar *command );
void gr_shared_system_call( GrShared *self, const gchar* command );

#endif

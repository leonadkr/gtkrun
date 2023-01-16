#ifndef SHARED_H
#define SHARED_H

#include <glib.h>
#include "array.h"

struct _GrShared
{
	/* public */
	gboolean silent;
	gint width;
	gint height;
	gint max_height;
	gboolean max_height_set;
	gchar *cache_dir;
	gboolean no_cache;
	gchar *config_path;
	gboolean no_config;
	gchar *env;

	/* private */
	gchar *envstr;
	GStrv envarr;
	gsize envarr_len;
	guint64 *dirmodarr;
	gchar *stored_path;
	gchar *env_cache_path;
	GrArray *stored_array;
	GrArray *env_array;
	GStrv compared_array;
};
typedef struct _GrShared GrShared;

GrShared* gr_shared_new( void );
void gr_shared_free( GrShared *self );
void gr_shared_setup_private( GrShared *self );
gchar* gr_shared_get_compared_string( GrShared *self, const gchar *text );
GStrv gr_shared_get_compared_array( GrShared *self, const gchar *text );
void gr_shared_system_call( GrShared *self, const gchar* str );

#endif

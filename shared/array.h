#ifndef ARRAY_H
#define ARRAY_H

#include <glib.h>

struct _GrArray
{
	/* public */
	gchar **p;
	gsize n;

	/* private */
	gchar *data;
	gsize size;
};
typedef struct _GrArray GrArray;

GrArray* gr_array_new( void );
void gr_array_free( GrArray *self );
GrArray* gr_array_new_from_stolen_data( gchar *data, gsize size );
gboolean gr_array_find_equal( GrArray *self, gchar *str );
void gr_array_sort( GrArray *self );
void gr_array_add_string( GrArray *self, const gchar *str );

#endif

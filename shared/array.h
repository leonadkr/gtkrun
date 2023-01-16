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
gboolean gr_array_find_equal( GrArray *self, gchar *str );
void gr_array_sort( GrArray *self );

#endif

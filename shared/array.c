#include <stdlib.h>
#include <glib.h>
#include "array.h"

static int
strpcmp0(
	gchar **s1,
	gchar **s2 )
{
	g_return_val_if_fail( s1 != NULL, 0 );
	g_return_val_if_fail( s2 != NULL, 0 );

	return g_strcmp0( *s1, *s2 );
}

GrArray*
gr_array_new(
	void )
{
	GrArray *self = g_new( GrArray, 1 );
	self->p = NULL;
	self->n = 0;
	self->data = NULL;
	self->size = 0;

	return self;
}

void
gr_array_free(
	GrArray *self )
{
	if( self == NULL )
		return;

	g_free( self->p );
	g_free( self->data );
	g_free( self );
}

gboolean
gr_array_find_equal(
	GrArray *self,
	gchar *str )
{
	gsize i;

	g_return_val_if_fail( self != NULL, FALSE );

	for( i = 0; i < self->n; ++i )
		if( g_strcmp0( self->p[i], str ) == 0 )
			return TRUE;

	return FALSE;
}

void
gr_array_sort(
	GrArray *self )
{
	g_return_if_fail( self != NULL );

	qsort( (void*)self->p, self->n, sizeof( self->p[0] ), (int (*)(const void*, const void*))strpcmp0 );
}


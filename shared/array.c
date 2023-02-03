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

GrArray*
gr_array_new_from_stolen_data(
	gchar *data,
	gsize size )
{
	GrArray *self;
	gsize i;

	/* nothing to do */
	if( data == NULL || size == 0 )
		return NULL;

	self = gr_array_new();
	self->data = data;
	self->size = size;

	/* count number of strings */
	self->n = 0;
	for( i = 0; i < self->size; ++i )
		if( self->data[i] == '\0' )
			self->n++;

	/* create pointer array */
	self->p = g_new( gchar*, self->n );

	/* connect pointers to data */
	self->p[0] = self->data;
	for( i = 1; i < self->n; ++i )
		self->p[i] = strchr( self->p[i-1], (int)'\0' ) + 1;

	/* strip string in array */
	for( i = 0; i < self->n; ++i )
		self->p[i] = g_strstrip( self->p[i] );
	
	return self;
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

void
gr_array_add_string(
	GrArray *self,
	const gchar *str )
{
	gchar *data;
	gsize size, str_size;

	g_return_if_fail( self != NULL );

	/* nothing to do */
	if( str == NULL )
		return;

	str_size = strlen( str ) + 1;
	size = self->size + str_size;
	data = g_new( gchar, size );
	memcpy( data, self->data, self->size );
	memcpy( &( data[self->size] ), str, str_size );

	gr_array_free( self );
	self = gr_array_new_from_stolen_data( data, size );
}


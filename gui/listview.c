#include <glib.h>
#include <gtk/gtk.h>
#include "window.h"
#include "listview.h"
#include "shared.h"

struct _GrListViewPrivate
{
	GrShared *shared;
};
typedef struct _GrListViewPrivate GrListViewPrivate;

static G_DEFINE_QUARK( gr-tree-view-private, gr_list_view_private )

static void
gr_list_view_private_free(
	GrListViewPrivate *priv )
{
	g_return_if_fail( priv != NULL );

	g_free( priv );
}

static GrListViewPrivate*
gr_list_view_private_new(
	GtkListView *self )
{
	GrListViewPrivate *priv;

	g_return_val_if_fail( GTK_IS_LIST_VIEW( self ), NULL );

	priv = g_new( GrListViewPrivate, 1 );
	priv->shared = NULL;

	g_object_set_qdata_full( G_OBJECT( self ), gr_list_view_private_quark(), priv, (GDestroyNotify)gr_list_view_private_free );

	return priv;
}

static GrListViewPrivate*
gr_list_view_get_private(
	GtkListView *self )
{
	g_return_val_if_fail( GTK_IS_LIST_VIEW( self ), NULL );

	return (GrListViewPrivate*)g_object_get_qdata( G_OBJECT( self ), gr_list_view_private_quark() );
}

static void
on_item_factory_setup(
	GtkSignalListItemFactory *self,
	GtkListItem *list_item,
	gpointer user_data )
{
	GtkLabel *label;

	label = GTK_LABEL( gtk_label_new( NULL ) );
	gtk_widget_set_halign( GTK_WIDGET( label ), GTK_ALIGN_START );
	gtk_label_set_single_line_mode( label, TRUE );
	gtk_label_set_selectable( label, FALSE );
	gtk_list_item_set_child( list_item, GTK_WIDGET( label ) );
}

static void
on_item_factory_bind(
	GtkSignalListItemFactory *self,
	GtkListItem *list_item,
	gpointer user_data )
{
	gchar *string;

	g_object_get( gtk_list_item_get_item( list_item ), "string", &string, NULL );
	gtk_label_set_text( GTK_LABEL( gtk_list_item_get_child( list_item ) ), string );
	g_free( string );
}

static void
on_list_view_activate(
	GtkListView *self,
	guint position,
	gpointer user_data )
{
	GrListViewPrivate *priv = gr_list_view_get_private( self );
	GObject *object;
	gchar *command;

	object = G_OBJECT( g_list_model_get_item( G_LIST_MODEL( gtk_list_view_get_model( self ) ), position ) );
	g_object_get( object, "string", &command, NULL );
	gr_shared_system_call( priv->shared, command );
	g_free( command );

	gr_window_close( priv->shared->window, priv->shared );
}

GtkListView*
gr_list_view_new(
	GrShared *shared )
{
	GrListViewPrivate *priv;
	GtkListItemFactory *item_factory;
	GtkListView *list_view;

	g_return_val_if_fail( shared != NULL, NULL );

	/* create tree view */
	item_factory = GTK_LIST_ITEM_FACTORY( gtk_signal_list_item_factory_new() );
	list_view = GTK_LIST_VIEW( gtk_list_view_new( NULL, item_factory ) );
	gtk_list_view_set_show_separators( list_view, FALSE );
	gtk_list_view_set_single_click_activate( list_view, FALSE );
	gtk_list_view_set_enable_rubberband( list_view, FALSE );

	/* connect factory's signals */
	g_signal_connect( G_OBJECT( item_factory ), "setup", G_CALLBACK( on_item_factory_setup ), NULL );
	g_signal_connect( G_OBJECT( item_factory ), "bind", G_CALLBACK( on_item_factory_bind ), NULL );

	/* connect activate signal */
	g_signal_connect( G_OBJECT( list_view ), "activate", G_CALLBACK( on_list_view_activate ), NULL );

	/* collect private */
	priv = gr_list_view_private_new( list_view );
	priv->shared = shared;

	return list_view;
}

void
gr_list_view_set_model_by_text(
	GtkListView *self,
	const gchar* text )
{
	GrListViewPrivate *priv;
	GStrv arr;
	GtkStringList *string_list;

	g_return_if_fail( GTK_IS_LIST_VIEW( self ) );

	/* nothing to insert */
	if( text == NULL )
		return;

	priv = gr_list_view_get_private( self );
	arr = gr_shared_get_compared_array( priv->shared, text );

	/* if nothing to insert, reset the list view's model */
	if( arr[0] == NULL )
	{
		gtk_list_view_set_model( self, NULL );
		return;
	}

	string_list = gtk_string_list_new( (const gchar* const*)arr );
	gtk_list_view_set_model( self, GTK_SELECTION_MODEL( gtk_single_selection_new( G_LIST_MODEL( string_list ) ) ) );
}

gchar*
gr_list_view_get_selected_text(
	GtkListView *self )
{
	GtkSingleSelection *single_selection;
	GObject *object;
	gchar *string;

	g_return_val_if_fail( GTK_IS_LIST_VIEW( self ), NULL );

	single_selection = GTK_SINGLE_SELECTION( gtk_list_view_get_model( self ) );
	if( single_selection == NULL )
		return NULL;

	object = gtk_single_selection_get_selected_item( single_selection );
	if( object != NULL )
	{
		g_object_get( object, "string", &string, NULL );
		return string;
	}

	return NULL;
}


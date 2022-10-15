#include <glib.h>
#include <gtk/gtk.h>
#include "treeview.h"
#include "shared.h"


struct _GrTreeViewPrivate
{
	GtkWindow *window;
	GrShared *shared;
};
typedef struct _GrTreeViewPrivate GrTreeViewPrivate;


static G_DEFINE_QUARK( gr-tree-view-private, gr_tree_view_private )

static void
gr_tree_view_private_free(
	GrTreeViewPrivate *priv )
{
	g_return_if_fail( priv != NULL );

	g_free( priv );
}

static GrTreeViewPrivate*
gr_tree_view_private_new(
	GtkTreeView *self )
{
	GrTreeViewPrivate *priv;

	g_return_val_if_fail( GTK_IS_TREE_VIEW( self ), NULL );

	priv = g_new( GrTreeViewPrivate, 1 );
	priv->window = NULL;
	priv->shared = NULL;

	g_object_set_qdata_full( G_OBJECT( self ), gr_tree_view_private_quark(), priv, (GDestroyNotify)gr_tree_view_private_free );

	return priv;
}

static GrTreeViewPrivate*
gr_tree_view_get_private(
	GtkTreeView *self )
{
	g_return_val_if_fail( GTK_IS_TREE_VIEW( self ), NULL );

	return (GrTreeViewPrivate*)g_object_get_qdata( G_OBJECT( self ), gr_tree_view_private_quark() );
}

static gboolean
on_event_key_pressed(
	GtkEventControllerKey *self,
	guint keyval,
	guint keycode,
	GdkModifierType state,
	gpointer user_data )
{
	GtkTreeView *tree_view = GTK_TREE_VIEW( user_data );
	GrTreeViewPrivate *priv = gr_tree_view_get_private( tree_view );
	GtkTreeSelection *tree_selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *command;

	if( keyval == GDK_KEY_Return )
	{
		tree_selection = gtk_tree_view_get_selection( tree_view );
		if( gtk_tree_selection_get_selected( tree_selection, &model, &iter ) )
		{
			gtk_tree_model_get( model, &iter, 0, &command, -1 );
			gr_shared_system_call( priv->shared, command );
			gr_shared_store_command_to_cache( priv->shared, command );
			g_free( command );

			gtk_window_destroy( priv->window );
		}

		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}


GtkTreeView*
gr_tree_view_new(
	GtkWindow *window,
	GrShared *shared )
{
	GrTreeViewPrivate *priv;
	GtkTreeView *tree_view;
	GtkEventControllerKey *event_key;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	g_return_val_if_fail( GTK_IS_WINDOW( window ), NULL );
	g_return_val_if_fail( shared != NULL, NULL );

	/* create tree view */
	tree_view = GTK_TREE_VIEW( gtk_tree_view_new() );
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes( "Compared text", renderer, "text", 0, NULL );
	gtk_tree_view_append_column( tree_view, column );
	gtk_tree_view_set_grid_lines( tree_view, GTK_TREE_VIEW_GRID_LINES_NONE );
	gtk_tree_view_set_headers_visible( tree_view, FALSE );
	gtk_tree_view_set_enable_search( tree_view, FALSE );
	gtk_tree_view_set_show_expanders( tree_view, FALSE );

	/* event handler */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( tree_view ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), tree_view );

	/* collect private */
	priv = gr_tree_view_private_new( tree_view );
	priv->window = window;
	priv->shared = shared;

	return tree_view;
}

void
gr_tree_view_set_model_by_text(
	GtkTreeView *self,
	const gchar* text )
{
	GrTreeViewPrivate *priv;
	GList *list, *l;
	GtkListStore *list_store;
	GtkTreeIter iter;

	g_return_if_fail( GTK_IS_TREE_VIEW( self ) );

	/* nothing to insert */
	if( text == NULL )
		return;

	priv = gr_tree_view_get_private( self );
	list = gr_shared_get_compared_list( priv->shared, text );

	/* nothing to insert */
	if( list == NULL )
		return;

	list_store = gtk_list_store_new( 1, G_TYPE_STRING );
	for( l = list; l != NULL; l = l->next )
	{
		gtk_list_store_append( list_store, &iter );
		gtk_list_store_set( list_store, &iter, 0, (gchar*)l->data, -1 );
	}
	gtk_tree_view_set_model( self, GTK_TREE_MODEL( list_store ) );

	g_list_free_full( list, (GDestroyNotify)g_free );
}

gchar*
gr_tree_view_get_selected_text(
	GtkTreeView *self )
{
	GtkTreeSelection *tree_selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text;

	g_return_val_if_fail( GTK_IS_TREE_VIEW( self ), NULL );

	tree_selection = gtk_tree_view_get_selection( self );
	if( gtk_tree_selection_get_selected( tree_selection, &model, &iter ) )
	{
		gtk_tree_model_get( model, &iter, 0, &text, -1 );
		return text;
	}

	return NULL;
}


#include <gtk/gtk.h>
#include "path.h"

struct _GrEditablePrivate
{
	GtkWindow *window;
	gulong insert_text_handler_id;
	gulong delete_text_handler_id;
	guint insert_text_signal_id;
	guint delete_text_signal_id;
	GList *filename_list;
};
typedef struct _GrEditablePrivate GrEditablePrivate;


/*
	private
*/
static G_DEFINE_QUARK( gr-editable-private, gr_editable_private )

static void
gr_editable_private_free(
	GrEditablePrivate *priv )
{
	g_return_if_fail( priv != NULL );

	g_list_free_full( priv->filename_list, (GDestroyNotify)g_free );
	g_free( priv );
}

static GrEditablePrivate*
gr_editable_private_new(
	GtkEditable *self )
{
	GrEditablePrivate *priv;

	g_return_val_if_fail( GTK_IS_EDITABLE( self ), NULL );

	priv = g_new( GrEditablePrivate, 1 );
	priv->window = NULL;
	priv->insert_text_handler_id = 0;
	priv->delete_text_handler_id = 0;
	priv->insert_text_signal_id = 0;
	priv->delete_text_signal_id = 0;
	priv->filename_list = NULL;

	g_object_set_qdata_full( G_OBJECT( self ), gr_editable_private_quark(), priv, (GDestroyNotify)gr_editable_private_free );

	return priv;
}

static GrEditablePrivate*
gr_editable_get_private(
	GtkEditable *self )
{
	g_return_val_if_fail( GTK_IS_EDITABLE( self ), NULL );

	return (GrEditablePrivate*)g_object_get_qdata( G_OBJECT( self ), gr_editable_private_quark() );
}

static void
gr_editable_set_compared_text(
	GtkEditable *self,
	gint position )
{
	const gchar *text;

	GrEditablePrivate *priv;
	GList *compare_list;

	g_return_if_fail( GTK_IS_EDITABLE( self ) );

	priv = gr_editable_get_private( self );
	text = gtk_editable_get_chars( self, 0, position );
	compare_list = gr_path_get_compare_list( priv->filename_list, text );

	if( compare_list != NULL )
		gtk_editable_set_text( self, (gchar*)g_list_first( compare_list )->data );
	gtk_editable_set_position( self, position );

	g_list_free_full( compare_list, (GDestroyNotify)g_free );
}

static void
on_editable_insert_text(
	GtkEditable *self,
	gchar *text,
	gint length,
	gint *position,
	gpointer user_data )
{
	GrEditablePrivate *priv;

	g_return_if_fail( GTK_IS_EDITABLE( self ) );

	priv = gr_editable_get_private( self );

	g_signal_handler_block( G_OBJECT( self ), priv->insert_text_handler_id );
	g_signal_handler_block( G_OBJECT( self ), priv->delete_text_handler_id );

	gr_editable_set_compared_text( self, *position );

	g_signal_handler_unblock( G_OBJECT( self ), priv->insert_text_handler_id );
	g_signal_handler_unblock( G_OBJECT( self ), priv->delete_text_handler_id );

	g_signal_stop_emission( G_OBJECT( self ), priv->insert_text_signal_id, 0 );
}

static void
on_editable_delete_text(
	GtkEditable *self,
	gint start_pos,
	gint end_pos,
	gpointer user_data )
{
	GrEditablePrivate *priv;

	g_return_if_fail( GTK_IS_EDITABLE( self ) );

	priv = gr_editable_get_private( self );

	g_signal_handler_block( G_OBJECT( self ), priv->insert_text_handler_id );
	g_signal_handler_block( G_OBJECT( self ), priv->delete_text_handler_id );

	gr_editable_set_compared_text( self, start_pos );

	g_signal_handler_unblock( G_OBJECT( self ), priv->insert_text_handler_id );
	g_signal_handler_unblock( G_OBJECT( self ), priv->delete_text_handler_id );

	g_signal_stop_emission( G_OBJECT( self ), priv->delete_text_signal_id, 0 );
}

static gboolean
on_event_key_pressed(
	GtkEventControllerKey *self,
	guint keyval,
	guint keycode,
	GdkModifierType state,
	gpointer user_data )
{
	const gchar *command;

	GtkEditable *editable = GTK_EDITABLE( user_data );
	GrEditablePrivate *priv = gr_editable_get_private( editable );
	GSubprocess *subproc;
	GError *error = NULL;

	if( keyval == GDK_KEY_Return )
	{
		command = gtk_editable_get_text( editable );
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
		gtk_window_destroy( priv->window );

		return FALSE;
	}

	return FALSE;
}

/*
	public
*/
GtkEntry*
gr_entry_new(
	GtkWindow *window )
{
	GtkEntry *entry;
	GtkEditable *editable;
	GtkEventControllerKey *event_key;
	GrEditablePrivate *priv;

	g_return_val_if_fail( GTK_IS_WINDOW( window ), NULL );

	/* create entry */
	entry = GTK_ENTRY( gtk_entry_new() );

	/* get editable */
	editable = gtk_editable_get_delegate( GTK_EDITABLE( entry ) );
	priv = gr_editable_private_new( editable );
	priv->window = window;
	priv->insert_text_handler_id = g_signal_connect_after( G_OBJECT( editable ), "insert-text", G_CALLBACK( on_editable_insert_text ), window );
	priv->delete_text_handler_id = g_signal_connect_after( G_OBJECT( editable ), "delete-text", G_CALLBACK( on_editable_delete_text ), window );
	priv->insert_text_signal_id = g_signal_lookup( "insert-text", GTK_TYPE_EDITABLE );
	priv->delete_text_signal_id = g_signal_lookup( "delete-text", GTK_TYPE_EDITABLE );
	priv->filename_list = gr_path_get_filename_list();

	/* event handler */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( editable ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), editable );

	return entry;
}


#include <glib.h>
#include <gtk/gtk.h>
#include "entry.h"
#include "shared.h"


struct _GrEditablePrivate
{
	gulong insert_text_handler_id;
	gulong delete_text_handler_id;
	guint insert_text_signal_id;
	guint delete_text_signal_id;
	GtkWindow *window;
	GrShared *shared;
};
typedef struct _GrEditablePrivate GrEditablePrivate;


static G_DEFINE_QUARK( gr-editable-private, gr_editable_private )

static void
gr_editable_private_free(
	GrEditablePrivate *priv )
{
	g_return_if_fail( priv != NULL );

	g_free( priv );
}

static GrEditablePrivate*
gr_editable_private_new(
	GtkEditable *self )
{
	GrEditablePrivate *priv;

	g_return_val_if_fail( GTK_IS_EDITABLE( self ), NULL );

	priv = g_new( GrEditablePrivate, 1 );
	priv->insert_text_handler_id = 0;
	priv->delete_text_handler_id = 0;
	priv->insert_text_signal_id = 0;
	priv->delete_text_signal_id = 0;
	priv->window = NULL;
	priv->shared = NULL;

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
	GrEditablePrivate *priv;
	gchar *text, *s;

	g_return_if_fail( GTK_IS_EDITABLE( self ) );

	priv = gr_editable_get_private( self );

	text = gtk_editable_get_chars( self, 0, position );
	s = gr_shared_get_compared_string( priv->shared, text );

	if( s != NULL )
	{
		gtk_editable_set_text( self, s );
		gtk_editable_set_position( self, position );
	}
	else
		gtk_editable_set_text( self, text );

	g_free( text );
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

	if( keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter )
	{
		command = gtk_editable_get_text( editable );
		gr_shared_system_call( priv->shared, command );
		gr_shared_store_command_to_cache( priv->shared, command );
		gtk_window_destroy( priv->window );
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}


GtkEntry*
gr_entry_new(
	GtkWindow *window,
	GrShared *shared )
{
	GtkEntry *entry;
	GtkEditable *editable;
	GtkEventControllerKey *event_key;
	GrEditablePrivate *priv;

	g_return_val_if_fail( GTK_IS_WINDOW( window ), NULL );
	g_return_val_if_fail( shared != NULL, NULL );

	/* create entry */
	entry = GTK_ENTRY( gtk_entry_new() );

	/* get editable */
	editable = gtk_editable_get_delegate( GTK_EDITABLE( entry ) );
	priv = gr_editable_private_new( editable );
	priv->insert_text_handler_id = g_signal_connect_after( G_OBJECT( editable ), "insert-text", G_CALLBACK( on_editable_insert_text ), window );
	priv->delete_text_handler_id = g_signal_connect_after( G_OBJECT( editable ), "delete-text", G_CALLBACK( on_editable_delete_text ), window );
	priv->insert_text_signal_id = g_signal_lookup( "insert-text", GTK_TYPE_EDITABLE );
	priv->delete_text_signal_id = g_signal_lookup( "delete-text", GTK_TYPE_EDITABLE );
	priv->window = window;
	priv->shared = shared;

	/* event handler */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( editable ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), editable );

	return entry;
}

gchar*
gr_entry_get_text_before_cursor(
	GtkEntry *self )
{
	GtkEditable *editable;
	gint position;
	gchar *text;

	g_return_val_if_fail( GTK_IS_ENTRY( self ), NULL );

	editable = gtk_editable_get_delegate( GTK_EDITABLE( self ) );
	position = gtk_editable_get_position( editable );
	text = gtk_editable_get_chars( editable, 0, position );

	return text;
}

void
gr_entry_set_text(
	GtkEntry *self,
	const gchar* text )
{
	GtkEditable *editable;

	g_return_if_fail( GTK_IS_ENTRY( self ) );

	/* nothing to insert */
	if( text == NULL )
		return;

	editable = gtk_editable_get_delegate( GTK_EDITABLE( self ) );
	gtk_editable_set_text( editable, text );
}


#include "grlist.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct _GrList
{
	GtkWidget parent_instance;

	gint min_content_height;
	gint max_content_height;

	GtkScrolledWindow *scrolled_window;
	GtkListView *list_view;
};
typedef struct _GrList GrList;

enum _GrListPropertyID
{
	PROP_0, /* 0 is reserved for GObject */

	PROP_MIN_CONTENT_HEIGHT,
	PROP_MAX_CONTENT_HEIGHT,

	N_PROPS
};
typedef enum _GrListPropertyID GrListPropertyID;

static GParamSpec *object_props[N_PROPS] = { NULL, };

enum _GrListSignalID
{
	SIGNAL_ACTIVATE,

	N_SIGNALS
};
typedef enum _GrListSignalID GrListSignalID;

static guint gr_list_signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE( GrList, gr_list, GTK_TYPE_WIDGET )

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
	GrList *list = GR_LIST( user_data );
	
	g_signal_emit( list, gr_list_signals[SIGNAL_ACTIVATE], 0 );
}

static void
gr_list_init(
	GrList *self )
{
	GtkListItemFactory *item_factory;

	/* create widgets */
	self->scrolled_window = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new() );
	gtk_scrolled_window_set_policy( self->scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
	gtk_scrolled_window_set_has_frame( self->scrolled_window, FALSE );
	gtk_scrolled_window_set_kinetic_scrolling( self->scrolled_window, FALSE );
	gtk_scrolled_window_set_overlay_scrolling( self->scrolled_window, FALSE );
	gtk_scrolled_window_set_propagate_natural_height( self->scrolled_window, TRUE );

	item_factory = GTK_LIST_ITEM_FACTORY( gtk_signal_list_item_factory_new() );
	self->list_view = GTK_LIST_VIEW( gtk_list_view_new( NULL, item_factory ) );
	gtk_list_view_set_show_separators( self->list_view, FALSE );
	gtk_list_view_set_single_click_activate( self->list_view, FALSE );
	gtk_list_view_set_enable_rubberband( self->list_view, FALSE );

	g_signal_connect( G_OBJECT( item_factory ), "setup", G_CALLBACK( on_item_factory_setup ), self );
	g_signal_connect( G_OBJECT( item_factory ), "bind", G_CALLBACK( on_item_factory_bind ), self );

	g_signal_connect( G_OBJECT( self->list_view ), "activate", G_CALLBACK( on_list_view_activate ), self );

	g_object_bind_property( G_OBJECT( self ), "min-content-height", G_OBJECT( self->scrolled_window ), "min-content-height", G_BINDING_DEFAULT );
	g_object_bind_property( G_OBJECT( self ), "max-content-height", G_OBJECT( self->scrolled_window ), "max-content-height", G_BINDING_DEFAULT );

	/*layout widgets */
	gtk_widget_set_parent( GTK_WIDGET( self->scrolled_window ), GTK_WIDGET( self ) );
	gtk_scrolled_window_set_child( self->scrolled_window, GTK_WIDGET( self->list_view ) );
}

static void
gr_list_get_property(
	GObject *object,	
	guint prop_id,	
	GValue *value,	
	GParamSpec *pspec )
{
	GrList *self = GR_LIST( object );

	switch( (GrListPropertyID)prop_id )
	{
		case PROP_MIN_CONTENT_HEIGHT:
			g_value_set_int( value, self->min_content_height );
			break;
		case PROP_MAX_CONTENT_HEIGHT:
			g_value_set_int( value, self->max_content_height );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_list_set_property(
	GObject *object,
	guint prop_id,
	const GValue *value,
	GParamSpec *pspec )
{
	GrList *self = GR_LIST( object );

	switch( (GrListPropertyID)prop_id )
	{
		case PROP_MIN_CONTENT_HEIGHT:
			gr_list_set_min_content_height( self, g_value_get_int( value ) );
			break;
		case PROP_MAX_CONTENT_HEIGHT:
			gr_list_set_max_content_height( self, g_value_get_int( value ) );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_list_dispose(
	GObject *object )
{
	GrList *self = GR_LIST( object );

	gtk_widget_unparent( GTK_WIDGET( self->scrolled_window ) );
}

static void
gr_list_class_init(
	GrListClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( klass );

	object_class->get_property = gr_list_get_property;
	object_class->set_property = gr_list_set_property;
	object_class->dispose = gr_list_dispose;

	object_props[PROP_MIN_CONTENT_HEIGHT] = g_param_spec_int(
		"min-content-height",
		"Minimal content height",
		"Minimal height of the widget's content",
		0,
		G_MAXINT,
		0,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_MAX_CONTENT_HEIGHT] = g_param_spec_int(
		"max-content-height",
		"Maximal content height",
		"Maximal height of the widget's content",
		0,
		G_MAXINT,
		0,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS );
	g_object_class_install_properties( object_class, N_PROPS, object_props );

	gr_list_signals[SIGNAL_ACTIVATE] = g_signal_new(
		"activate",
		G_TYPE_FROM_CLASS( klass ),
		G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		0,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		0 );

	gtk_widget_class_set_layout_manager_type( widget_class, GTK_TYPE_BIN_LAYOUT );
}

GrList*
gr_list_new(
	void )
{
	return GR_LIST( g_object_new( GR_TYPE_LIST, NULL ) );
}

gint
gr_list_get_min_content_height(
	GrList *self )
{
	g_return_val_if_fail( GR_IS_LIST( self ), 0 );

	return self->min_content_height;
}

void
gr_list_set_min_content_height(
	GrList *self,
	gint height )
{
	g_return_if_fail( GR_IS_LIST( self ) );

	g_object_freeze_notify( G_OBJECT( self ) );

	self->min_content_height = height;

	g_object_notify_by_pspec( G_OBJECT( self ), object_props[PROP_MIN_CONTENT_HEIGHT] );

	g_object_thaw_notify( G_OBJECT( self ) );
}

gint
gr_list_get_max_content_height(
	GrList *self )
{
	g_return_val_if_fail( GR_IS_LIST( self ), 0 );

	return self->max_content_height;
}

void
gr_list_set_max_content_height(
	GrList *self,
	gint height )
{
	g_return_if_fail( GR_IS_LIST( self ) );

	g_object_freeze_notify( G_OBJECT( self ) );

	self->max_content_height = height;

	g_object_notify_by_pspec( G_OBJECT( self ), object_props[PROP_MAX_CONTENT_HEIGHT] );

	g_object_thaw_notify( G_OBJECT( self ) );
}

void
gr_list_set_array(
	GrList *self,
	const GStrv array )
{
	GtkSingleSelection *single_selection;

	g_return_if_fail( GR_IS_LIST( self ) );

	/* if nothing to insert, reset the list view's model */
	if( array == NULL || array[0] == NULL )
	{
		gtk_list_view_set_model( self->list_view, NULL );
		return;
	}

	single_selection = gtk_single_selection_new( G_LIST_MODEL( gtk_string_list_new( (const gchar* const*)array ) ) );
	gtk_list_view_set_model( self->list_view, GTK_SELECTION_MODEL( single_selection ) );
	g_object_unref( G_OBJECT( single_selection ) );
}

gchar*
gr_list_get_selected_text(
	GrList *self )
{
	GtkSingleSelection *single_selection;
	GObject *object;
	gchar *string;

	g_return_val_if_fail( GR_IS_LIST( self ), NULL );

	single_selection = GTK_SINGLE_SELECTION( gtk_list_view_get_model( self->list_view ) );
	if( single_selection == NULL )
		return NULL;

	object = gtk_single_selection_get_selected_item( single_selection );
	if( object == NULL )
		return NULL;

	g_object_get( object, "string", &string, NULL );

	return string;
}


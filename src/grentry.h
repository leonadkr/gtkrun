#ifndef GRENTRY_H
#define GRENTRY_H

#include "grcommandlist.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GR_TYPE_ENTRY ( gr_entry_get_type() )
G_DECLARE_FINAL_TYPE( GrEntry, gr_entry, GR, ENTRY, GtkWidget )

GrEntry* gr_entry_new( void );
void gr_entry_set_text( GrEntry *self, const gchar* text );
gchar* gr_entry_get_text( GrEntry *self );
gchar* gr_entry_get_text_befor_cursor( GrEntry *self );
GrCommandList* gr_entry_get_command_list( GrEntry *self );
void gr_entry_set_command_list( GrEntry *self, GrCommandList *com_list );

G_END_DECLS

#endif

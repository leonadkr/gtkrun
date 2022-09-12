#ifndef ENTRY_H
#define ENTRY_H

#include <glib.h>
#include <gtk/gtk.h>
#include "path.h"

GtkEntry* gr_entry_new( GtkWindow *window, GrPath *path );
gchar* gr_entry_get_text_before_cursor( GtkEntry *self );
void gr_entry_set_text( GtkEntry *self, const gchar* text );

#endif

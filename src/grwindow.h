#ifndef GRWINDOW_H
#define GRWINDOW_H

#include "grapplication.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GR_TYPE_WINDOW ( gr_window_get_type() )
G_DECLARE_FINAL_TYPE( GrWindow, gr_window, GR, WINDOW, GtkApplicationWindow )

GrWindow* gr_window_new( GrApplication *app );

G_END_DECLS

#endif

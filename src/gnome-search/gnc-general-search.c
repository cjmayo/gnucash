/*
 * gnc-general-select.c --  Widget to pop-up a search dialog and show
 *			the selected item.
 *
 * Copyright (C) 2001 Free Software Foundation
 * All rights reserved.
 *
 * Derek Atkins <warlord@MIT.EDU>
 *
 * Gnucash is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Gnucash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652
 * Boston, MA  02111-1307,  USA       gnu@gnu.org
 *
 */
/*
  @NOTATION@
 */

#include <config.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "gnc-component-manager.h"
#include "QueryCore.h"
#include "QueryObject.h"
#include "gncObject.h"
#include "gnc-general-search.h"

#define GNCGENERALSEARCH_CLASS	"gnc-general-search-widget"

/* Signal codes */
enum
{
	SELECTION_CHANGED,
	LAST_SIGNAL
};


static void gnc_general_search_init         (GNCGeneralSearch      *gsl);
static void gnc_general_search_class_init   (GNCGeneralSearchClass *class);
static void gnc_general_search_destroy      (GtkObject             *object);

#define _PRIVATE(x) (((GNCSearchString *)(x))->priv)

struct _GNCGeneralSearchPrivate {
	GUID			guid;
	GNCIdTypeConst		type;
	GNCSearchCB		search_cb;
	gpointer		user_data;
	GNCSearchWindow *	sw;
	const QofParam * get_guid;
	gint			component_id;
};

static GtkHBoxClass *parent_class;
static guint general_search_signals[LAST_SIGNAL];


/**
 * gnc_general_search_get_type:
 *
 * Returns the GtkType for the GNCGeneralSearch widget
 */
guint
gnc_general_search_get_type (void)
{
	static guint general_search_type = 0;

	if (!general_search_type){
		static const GTypeInfo our_info = {
			sizeof (GNCGeneralSearchClass),    /* class_size */
			NULL,   			   /* base_init */
			NULL,				   /* base_finalize */
			(GClassInitFunc) gnc_general_search_class_init,
			NULL,				   /* class_finalize */
			NULL,				   /* class_data */
			sizeof (GNCGeneralSearch),	   /* */
			0,				   /* n_preallocs */
			(GInstanceInitFunc) gnc_general_search_init,
		};

		general_search_type = g_type_register_static (GTK_TYPE_HBOX,
							      "GNCGeneralSearch",
							      &our_info, 0);
	}

	return general_search_type;
}

static void
gnc_general_search_class_init (GNCGeneralSearchClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gtk_hbox_get_type ());

	general_search_signals[SELECTION_CHANGED] =
		gtk_signal_new("changed",
			       GTK_RUN_FIRST,
			       GTK_CLASS_TYPE(object_class),
			       GTK_SIGNAL_OFFSET(GNCGeneralSearchClass,
						 changed),
			       gtk_marshal_NONE__NONE,
			       GTK_TYPE_NONE, 0);

	object_class->destroy = gnc_general_search_destroy;

	klass->changed = NULL;
}

static void
gnc_general_search_init (GNCGeneralSearch *gsl)
{
	gsl->priv = g_malloc0 (sizeof (*gsl->priv));
	gsl->selected_item = NULL;
}

static void
gnc_general_search_destroy (GtkObject *object)
{
	GNCGeneralSearch *gsl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNC_IS_GENERAL_SEARCH (object));

	gsl = GNC_GENERAL_SEARCH (object);

	gsl->entry = NULL;
	gsl->button = NULL;

	if (gsl->priv) {
		/* Clear the callbacks */
		if (gsl->priv->sw) {
			gnc_search_dialog_set_select_cb (gsl->priv->sw, NULL,
							 NULL, FALSE);
			gnc_search_dialog_disconnect (gsl->priv->sw, gsl);
			gsl->priv->sw = NULL;
		}

		/* Unregister ourselves */
		gnc_unregister_gui_component (gsl->priv->component_id);

		/* And let go */
		g_free (gsl->priv);
		gsl->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/* The "selection" contents have changed.  Change the text. */
static void
reset_selection_text (GNCGeneralSearch *gsl)
{
	const char *text;

	if (gsl->selected_item == NULL)
		text = "";
	else
		text = gncObjectPrintable (gsl->priv->type, gsl->selected_item);

	gtk_entry_set_text(GTK_ENTRY(gsl->entry), text);
}

/* We've got a refresh event */
static void
refresh_handler (GHashTable *changes, gpointer data)
{
	GNCGeneralSearch *gsl = data;
	const EventInfo *info;

	if (changes) {
		info = gnc_gui_get_entity_events (changes, &gsl->priv->guid);
		if (info) {
			if (info->event_mask & GNC_EVENT_DESTROY)
				gsl->selected_item = NULL;
			reset_selection_text (gsl);
		}
	}
}

/* The user has selected from the search dialog */
static void
new_item_selected_cb (gpointer item, gpointer user_data)
{
	GNCGeneralSearch *gsl = user_data;
	gnc_general_search_set_selected (gsl, item);
}

/* The search dialog has closed; let's forget about her */
static int
on_close_cb (GtkDialog *dialog, gpointer user_data)
{
	GNCGeneralSearch *gsl = user_data;
	gsl->priv->sw = NULL;
	return FALSE;
}

/* The user clicked on the button.  Pop up the selection window */
static void
search_cb(GtkButton * button, gpointer user_data)
{
	GNCGeneralSearch *gsl = user_data;
	GNCSearchWindow *sw;

	if (gsl->priv->sw) {
		gnc_search_dialog_raise (gsl->priv->sw);
		return;
	}

	sw = (gsl->priv->search_cb)(gsl->selected_item, gsl->priv->user_data);

	/* NULL means nothing to 'select' */
	if (sw == NULL)
		return;

	/* Ok, save this search window and setup callbacks */
	gsl->priv->sw = sw;

	/* Catch when the search dialog closes */
	gnc_search_dialog_connect_on_close (sw, GTK_SIGNAL_FUNC (on_close_cb),
					    gsl);

	/* Catch the selection */
	gnc_search_dialog_set_select_cb (sw, new_item_selected_cb,
					 gsl, gsl->allow_clear);

}

static void
create_children (GNCGeneralSearch *gsl, const char *label)
{
	gsl->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (gsl->entry), FALSE);
	gtk_box_pack_start (GTK_BOX (gsl), gsl->entry, TRUE, TRUE, 0);
	gtk_widget_show (gsl->entry);

	gsl->button = gtk_button_new_with_label (label);
	gtk_box_pack_start (GTK_BOX (gsl), gsl->button, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (gsl->button), "clicked",
			  G_CALLBACK (search_cb), gsl);
	gtk_widget_show (gsl->button);
}

/**
 * gnc_general_search_new:
 *
 * Creates a new GNCGeneralSearch widget which can be used to provide
 * an easy way to choose selections
 *
 * Returns a GNCGeneralSearch widget.
 */
GtkWidget *
gnc_general_search_new (GNCIdTypeConst type, const char *label,
			GNCSearchCB search_cb, gpointer user_data)
{
	GNCGeneralSearch *gsl;
	const QofParam *get_guid;

	g_return_val_if_fail (type && label && search_cb, NULL);

	get_guid = qof_class_get_parameter (type, QOF_PARAM_GUID);
	g_return_val_if_fail (get_guid, NULL);

	gsl = g_object_new (gnc_general_search_get_type (), NULL);

	create_children (gsl, label);

	gsl->priv->type = type;
	gsl->priv->search_cb = search_cb;
	gsl->priv->user_data = user_data;
	gsl->priv->get_guid = get_guid;
	gsl->priv->component_id =
		gnc_register_gui_component (GNCGENERALSEARCH_CLASS,
					    refresh_handler, NULL, gsl);

	return GTK_WIDGET (gsl);
}

/**
 * gnc_general_search_set_selected:
 * @gsl: the general selection widget
 * @selection: the selection to point to
 *
 * Sets the selection value of the widget to a particular pointer.
 *
 * Returns nothing.
 */
void
gnc_general_search_set_selected (GNCGeneralSearch *gsl, gpointer selection)
{
	g_return_if_fail(gsl != NULL);
	g_return_if_fail(GNC_IS_GENERAL_SEARCH(gsl));

	if (selection != gsl->selected_item) {
		gsl->selected_item = selection;
		reset_selection_text (gsl);
		gtk_signal_emit(GTK_OBJECT(gsl),
			      general_search_signals[SELECTION_CHANGED]);
	}

	gnc_gui_component_clear_watches (gsl->priv->component_id);

	if (selection) 
   {
      const QofParam *get_guid = gsl->priv->get_guid;
		gsl->priv->guid = * ((GUID *)(get_guid->param_getfcn
					      (gsl->selected_item, get_guid)));
		gnc_gui_component_watch_entity
			(gsl->priv->component_id, &(gsl->priv->guid),
			 GNC_EVENT_MODIFY | GNC_EVENT_DESTROY);
	} else
		gsl->priv->guid = *xaccGUIDNULL ();
}

/**
 * gnc_general_search_get_selected:
 * @gsl: the general selection widget
 *
 * Returns the current selection by the widget.
 */
gpointer
gnc_general_search_get_selected (GNCGeneralSearch *gsl)
{
	g_return_val_if_fail(gsl != NULL, NULL);
	g_return_val_if_fail(GNC_IS_GENERAL_SEARCH(gsl), NULL);

	return gsl->selected_item;
}

void
gnc_general_search_allow_clear (GNCGeneralSearch *gsl, gboolean allow_clear)
{
	g_return_if_fail (GNC_IS_GENERAL_SEARCH (gsl));
	gsl->allow_clear = allow_clear;
}

/*
  Local Variables:
  c-basic-offset: 8
  End:
*/
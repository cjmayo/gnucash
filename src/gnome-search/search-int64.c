/*
 *  Copyright (C) 2002 Derek Atkins
 *
 *  Authors: Derek Atkins <warlord@MIT.EDU>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "gnc-amount-edit.h"
#include "QueryCore.h"

#include "search-int64.h"

#define d(x)

static void editable_enters (GNCSearchCoreType *fe);
static void grab_focus (GNCSearchCoreType *fe);
static GNCSearchCoreType *gncs_clone(GNCSearchCoreType *fe);
static gboolean gncs_validate (GNCSearchCoreType *fe);
static GtkWidget *gncs_get_widget(GNCSearchCoreType *fe);
static QueryPredData_t gncs_get_predicate (GNCSearchCoreType *fe);

static void gnc_search_int64_class_init	(GNCSearchInt64Class *class);
static void gnc_search_int64_init	(GNCSearchInt64 *gspaper);
static void gnc_search_int64_finalize	(GObject *obj);

#define _PRIVATE(x) (((GNCSearchInt64 *)(x))->priv)

struct _GNCSearchInt64Private {
  GtkWidget *entry;
  GNCAmountEdit *gae;
};

static GNCSearchCoreTypeClass *parent_class;

guint
gnc_search_int64_get_type (void)
{
  static guint type = 0;
	
  if (!type) {
    GTypeInfo type_info = {
      sizeof(GNCSearchInt64Class),      /* class_size */
      NULL,   				/* base_init */
      NULL,				/* base_finalize */
      (GClassInitFunc)gnc_search_int64_class_init,
      NULL,				/* class_finalize */
      NULL,				/* class_data */
      sizeof(GNCSearchInt64),		/* */
      0,				/* n_preallocs */
      (GInstanceInitFunc)gnc_search_int64_init,
    };
		
    type = g_type_register_static (GNC_TYPE_SEARCH_CORE_TYPE,
				   "GNCSearchInt64",
				   &type_info, 0);
  }
	
  return type;
}

static void
gnc_search_int64_class_init (GNCSearchInt64Class *class)
{
  GObjectClass *object_class;
  GNCSearchCoreTypeClass *gnc_search_core_type = (GNCSearchCoreTypeClass *)class;

  object_class = G_OBJECT_CLASS (class);
  parent_class = g_type_class_peek_parent (class);

  object_class->finalize = gnc_search_int64_finalize;

  /* override methods */
  gnc_search_core_type->editable_enters = editable_enters;
  gnc_search_core_type->grab_focus = grab_focus;
  gnc_search_core_type->validate = gncs_validate;
  gnc_search_core_type->get_widget = gncs_get_widget;
  gnc_search_core_type->get_predicate = gncs_get_predicate;
  gnc_search_core_type->clone = gncs_clone;
}

static void
gnc_search_int64_init (GNCSearchInt64 *o)
{
  o->priv = g_malloc0 (sizeof (*o->priv));
  o->how = COMPARE_EQUAL;
}

static void
gnc_search_int64_finalize (GObject *obj)
{
  GNCSearchInt64 *o = (GNCSearchInt64 *)obj;
  g_assert (IS_GNCSEARCH_INT64 (o));

  g_free(o->priv);
	
  G_OBJECT_CLASS (parent_class)->finalize(obj);
}

/**
 * gnc_search_int64_new:
 *
 * Create a new GNCSearchInt64 object.
 * 
 * Return value: A new #GNCSearchInt64 object.
 **/
GNCSearchInt64 *
gnc_search_int64_new (void)
{
  GNCSearchInt64 *o = g_object_new(gnc_search_int64_get_type (), NULL);
  return o;
}

void
gnc_search_int64_set_value (GNCSearchInt64 *fi, gint64 value)
{
  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_INT64 (fi));
	
  fi->value = value;
}

void
gnc_search_int64_set_how (GNCSearchInt64 *fi, query_compare_t how)
{
  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_INT64 (fi));
  fi->how = how;
}

static gboolean
gncs_validate (GNCSearchCoreType *fe)
{
  GNCSearchInt64 *fi = (GNCSearchInt64 *)fe;
  gboolean valid = TRUE;

  g_return_val_if_fail (fi, FALSE);
  g_return_val_if_fail (IS_GNCSEARCH_INT64 (fi), FALSE);
	
  /* XXX */

  return valid;
}

static void
option_changed (GtkWidget *widget, GNCSearchInt64 *fe)
{
  fe->how = (query_compare_t)
    g_object_get_data (G_OBJECT (widget), "option");
}

static void
entry_changed (GNCAmountEdit *entry, GNCSearchInt64 *fe)
{
  gnc_numeric value = gnc_amount_edit_get_amount (entry);
  g_assert (value.denom == 1);
  fe->value = value.num;
}

static GtkWidget *
add_menu_item (GtkWidget *menu, gpointer user_data, char *label,
	       query_compare_t option)
{
  GtkWidget *item = gtk_menu_item_new_with_label (label);
  g_object_set_data (G_OBJECT (item), "option", (gpointer) option);
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (option_changed), user_data);
  gtk_menu_append (GTK_MENU (menu), item);
  gtk_widget_show (item);
  return item;
}

#define ADD_MENU_ITEM(str,op) { \
	item = add_menu_item (menu, fe, str, op); \
	if (fi->how == op) { current = index; first = item; } \
	index++; \
} 

static GtkWidget *
make_menu (GNCSearchCoreType *fe)
{
  GNCSearchInt64 *fi = (GNCSearchInt64 *)fe;
  GtkWidget *menu, *item, *first = NULL, *opmenu;
  int current = 0, index = 0;

  menu = gtk_menu_new ();

  ADD_MENU_ITEM (_("is less than"), COMPARE_LT);
  first = item;			/* Force one */
  ADD_MENU_ITEM (_("is less than or equal to"), COMPARE_LTE);
  ADD_MENU_ITEM (_("equals"), COMPARE_EQUAL);
  ADD_MENU_ITEM (_("does not equal"), COMPARE_NEQ);
  ADD_MENU_ITEM (_("is greater than"), COMPARE_GT);
  ADD_MENU_ITEM (_("is greater than or equal to"), COMPARE_GTE);

  opmenu = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (opmenu), menu);

  g_signal_emit_by_name (G_OBJECT (first), "activate", fe);
  gtk_option_menu_set_history (GTK_OPTION_MENU (opmenu), current);

  return opmenu;
}

static void
grab_focus (GNCSearchCoreType *fe)
{
  GNCSearchInt64 *fi = (GNCSearchInt64 *)fe;

  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_INT64 (fi));

  if (fi->priv->entry)
    gtk_widget_grab_focus (fi->priv->entry);
}

static void
editable_enters (GNCSearchCoreType *fe)
{
  GNCSearchInt64 *fi = (GNCSearchInt64 *)fe;

  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_INT64 (fi));

  if (fi->priv->entry)
    gtk_entry_set_activates_default(GTK_ENTRY (fi->priv->entry), TRUE);
}

static GtkWidget *
gncs_get_widget (GNCSearchCoreType *fe)
{
  GtkWidget *entry, *menu, *box;
  GNCSearchInt64 *fi = (GNCSearchInt64 *)fe;
	
  g_return_val_if_fail (fi, NULL);
  g_return_val_if_fail (IS_GNCSEARCH_INT64 (fi), NULL);

  box = gtk_hbox_new (FALSE, 3);

  /* Build and connect the option menu */
  menu = make_menu (fe);
  gtk_box_pack_start (GTK_BOX (box), menu, FALSE, FALSE, 3);

  /* Build and connect the entry window */
  entry = gnc_amount_edit_new ();
  gnc_amount_edit_set_print_info (GNC_AMOUNT_EDIT (entry),
				  gnc_integral_print_info ());
  if (fi->value) {
    gnc_numeric value = gnc_numeric_create (fi->value, 1);
    gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (entry), value);
  }
  g_signal_connect (G_OBJECT (entry), "amount_changed", G_CALLBACK (entry_changed), fe);
  gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 3);
  fi->priv->entry = gnc_amount_edit_gtk_entry (GNC_AMOUNT_EDIT (entry));
  fi->priv->gae = GNC_AMOUNT_EDIT (entry);

  /* And return the box */
  return box;
}

static QueryPredData_t gncs_get_predicate (GNCSearchCoreType *fe)
{
  GNCSearchInt64 *fi = (GNCSearchInt64 *)fe;

  g_return_val_if_fail (fi, NULL);
  g_return_val_if_fail (IS_GNCSEARCH_INT64 (fi), NULL);

  /* force the computation of the entry, because we may not get the signal */
  entry_changed (fi->priv->gae, fi);

  return gncQueryInt64Predicate (fi->how, fi->value);
}

static GNCSearchCoreType *gncs_clone(GNCSearchCoreType *fe)
{
  GNCSearchInt64 *se, *fse = (GNCSearchInt64 *)fe;

  g_return_val_if_fail (fse, NULL);
  g_return_val_if_fail (IS_GNCSEARCH_INT64 (fse), NULL);

  se = gnc_search_int64_new ();
  gnc_search_int64_set_value (se, fse->value);
  gnc_search_int64_set_how (se, fse->how);

  return (GNCSearchCoreType *)se;
}
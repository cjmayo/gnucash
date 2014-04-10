/********************************************************************\
 * dialog-commodities.c -- commodities dialog                       *
 * Copyright (C) 2001 Gnumatic, Inc.                                *
 * Author: Dave Peticolas <dave@krondo.com>                         *
 * Copyright (C) 2003,2005 David Hampton                            *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
\********************************************************************/

#include "config.h"

#include <gnome.h>

#include "dialog-commodity.h"
#include "dialog-utils.h"
#include "gnc-commodity.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-tree-view-commodity.h"
#include "gnc-ui.h"
#include "gnc-ui-util.h"
#include "gnc-gconf-utils.h"
#include "gnc-gnome-utils.h"
#include "messages.h"


#define DIALOG_COMMODITIES_CM_CLASS "dialog-commodities"
#define GCONF_SECTION "dialogs/edit_commodities"

/* This static indicates the debugging module that this .o belongs to.  */
/* static short module = MOD_GUI; */

typedef struct
{
  GtkWidget * dialog;

  GncTreeViewCommodity * commodity_tree;
  GtkWidget * edit_button;
  GtkWidget * remove_button;
  gboolean    show_currencies;

  gboolean new;
} CommoditiesDialog;


void gnc_commodities_window_destroy_cb (GtkObject *object, CommoditiesDialog *cd);
void gnc_commodities_dialog_response (GtkDialog *dialog, gint response, CommoditiesDialog *cd);
void gnc_commodities_show_currencies_toggled (GtkToggleButton *toggle, CommoditiesDialog *cd);



void
gnc_commodities_window_destroy_cb (GtkObject *object,   CommoditiesDialog *cd)
{
  gnc_unregister_gui_component_by_data (DIALOG_COMMODITIES_CM_CLASS, cd);

  g_free (cd);
}

static void
edit_clicked (CommoditiesDialog *cd)
{
	gnc_commodity *commodity;

	commodity = gnc_tree_view_commodity_get_selected_commodity (cd->commodity_tree);
	if (commodity == NULL)
		return;

	if (gnc_ui_edit_commodity_modal (commodity, cd->dialog))
		gnc_gui_refresh_all ();
}

static void
remove_clicked (CommoditiesDialog *cd)
{
  QofBook *book;
  GNCPriceDB *pdb;
  GList *node;
  GList *prices;
  GList *accounts;
  gboolean do_delete;
  gboolean can_delete;
  gnc_commodity *commodity;
  
  commodity = gnc_tree_view_commodity_get_selected_commodity (cd->commodity_tree);
  if (commodity == NULL)
    return;

  accounts = xaccGroupGetSubAccounts (gnc_get_current_group ());
  can_delete = TRUE;
  do_delete = FALSE;

  for (node = accounts; node; node = node->next)
  {
    Account *account = node->data;

    if (commodity == xaccAccountGetCommodity (account))
    {
      can_delete = FALSE;
      break;
    }
  }

  /* FIXME check for transaction references */

  if (!can_delete)
  {
    const char *message = _("That commodity is currently used by\n"
                            "at least one of your accounts. You may\n"
                            "not delete it.");

    gnc_warning_dialog (cd->dialog, message);
    g_list_free (accounts);
    return;
  }

  book = xaccGroupGetBook (xaccAccountGetRoot (accounts->data));
  pdb = gnc_pricedb_get_db (book);
  prices = gnc_pricedb_get_prices(pdb, commodity, NULL);
  if (prices)
  {
    const char *message = _("This commodity has price quotes. Are\n"
			    "you sure you want to delete the selected\n"
                            "commodity and its price quotes?");

    do_delete = gnc_verify_dialog (cd->dialog, TRUE, message);
  }
  else
  {
    const char *message = _("Are you sure you want to delete the\n"
                            "selected commodity?");

    do_delete = gnc_verify_dialog (cd->dialog, TRUE, message);
  }

  if (do_delete)
  {
    gnc_commodity_table *ct;

    ct = gnc_get_current_commodities ();
    for (node = prices; node; node = node->next)
      gnc_pricedb_remove_price(pdb, node->data);

    gnc_commodity_table_remove (ct, commodity);
    gnc_commodity_destroy (commodity);
    commodity = NULL;
  }

  gnc_price_list_destroy(prices);
  g_list_free (accounts);
  gnc_gui_refresh_all ();
}

static void
add_clicked (CommoditiesDialog *cd)
{
  gnc_commodity *commodity;
  const char *namespace;

  commodity = gnc_tree_view_commodity_get_selected_commodity (cd->commodity_tree);
  if (commodity)
    namespace = gnc_commodity_get_namespace (commodity);
  else
    namespace = NULL;

  commodity = gnc_ui_new_commodity_modal (namespace, cd->dialog);
}

void
gnc_commodities_dialog_response (GtkDialog *dialog,
				 gint response,
				 CommoditiesDialog *cd)
{
	switch (response) {
	 case GNC_RESPONSE_NEW:
	  add_clicked (cd);
	  return;

	 case GNC_RESPONSE_DELETE:
	  remove_clicked (cd);
	  return;

	 case GNC_RESPONSE_EDIT:
	  edit_clicked (cd);
	  return;

	 case GTK_RESPONSE_CLOSE:
	 default:
	  gnc_close_gui_component_by_data (DIALOG_COMMODITIES_CM_CLASS, cd);
	  return;
	}
}

static void
gnc_commodities_dialog_selection_changed (GtkTreeSelection *selection,
					  CommoditiesDialog *cd)
{
	gboolean sensitive = FALSE;
	gnc_commodity *commodity;

	commodity = gnc_tree_view_commodity_get_selected_commodity (cd->commodity_tree);
	sensitive = commodity && !gnc_commodity_is_iso(commodity);
	gtk_widget_set_sensitive (cd->edit_button, sensitive);
	gtk_widget_set_sensitive (cd->remove_button, sensitive);
}

void
gnc_commodities_show_currencies_toggled (GtkToggleButton *toggle,
					 CommoditiesDialog *cd)
{

	cd->show_currencies = gtk_toggle_button_get_active (toggle);
	gnc_tree_view_commodity_refilter (cd->commodity_tree);
}

static gboolean
gnc_commodities_dialog_filter_ns_func (gnc_commodity_namespace *namespace,
				       gpointer data)
{
	CommoditiesDialog *cd = data;
	const gchar *name;
	GList *list;

	/* Never show the template list */
	name = gnc_commodity_namespace_get_name (namespace);
	if (safe_strcmp (name, "template") == 0)
	  return FALSE;

	/* Check whether or not to show commodities */
	if (!cd->show_currencies && gnc_commodity_namespace_is_iso(name))
	  return FALSE;

	/* Show any other namespace that has commodities */
	list = gnc_commodity_namespace_get_commodity_list(namespace);
	return (list != NULL);
}

static gboolean
gnc_commodities_dialog_filter_cm_func (gnc_commodity *commodity,
				       gpointer data)
{
	CommoditiesDialog *cd = data;

	if (cd->show_currencies)
	  return TRUE;
	return !gnc_commodity_is_iso(commodity);
}

static void
gnc_commodities_dialog_create (GtkWidget * parent, CommoditiesDialog *cd)
{
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *scrolled_window;
  GladeXML *xml;
  GtkTreeView *view;
  GtkTreeSelection *selection;
 
  xml = gnc_glade_xml_new ("commodities.glade", "Commodities Dialog");
  dialog = glade_xml_get_widget (xml, "Commodities Dialog");

  cd->dialog = dialog;
  cd->show_currencies = gnc_gconf_get_bool(GCONF_SECTION, "include_iso", NULL);
  
  glade_xml_signal_autoconnect_full(xml, gnc_glade_autoconnect_full_func, cd);

  /* parent */
  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

  /* buttons */
  cd->remove_button = glade_xml_get_widget (xml, "remove_button");
  cd->edit_button = glade_xml_get_widget (xml, "edit_button");

  /* commodity tree */
    
    scrolled_window = glade_xml_get_widget (xml, "commodity_list_window");
    view = gnc_tree_view_commodity_new(gnc_get_current_book (),
				       "gconf-section", GCONF_SECTION,
				       "show-column-menu", TRUE,
				       NULL);
    cd->commodity_tree = GNC_TREE_VIEW_COMMODITY(view);
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET(view));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(cd->commodity_tree), TRUE);
    gnc_tree_view_commodity_set_filter (cd->commodity_tree,
					gnc_commodities_dialog_filter_ns_func,
					gnc_commodities_dialog_filter_cm_func,
					cd, NULL);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    g_signal_connect (G_OBJECT (selection), "changed",
		      G_CALLBACK (gnc_commodities_dialog_selection_changed), cd);


    /* Show currency button */
    button = glade_xml_get_widget (xml, "show_currencies_button");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), cd->show_currencies);

  gnc_restore_window_size (GCONF_SECTION, GTK_WINDOW(cd->dialog));
}

static void
close_handler (gpointer user_data)
{
  CommoditiesDialog *cd = user_data;

  gnc_save_window_size(GCONF_SECTION, GTK_WINDOW(cd->dialog));

  gnc_gconf_set_bool(GCONF_SECTION, "include_iso", cd->show_currencies, NULL);

  gtk_widget_destroy(cd->dialog);
}

static void
refresh_handler (GHashTable *changes, gpointer user_data)
{
  CommoditiesDialog *cd = user_data;

  g_return_if_fail(cd != NULL);

  gnc_tree_view_commodity_refilter (cd->commodity_tree);
}

static gboolean
show_handler (const char *class, gint component_id,
	      gpointer user_data, gpointer iter_data)
{
  CommoditiesDialog *cd = user_data;

  if (!cd)
    return(FALSE);
  gtk_window_present (GTK_WINDOW(cd->dialog));
  return(TRUE);
}

/********************************************************************\
 * gnc_commodities_dialog                                           *
 *   opens up a window to edit price information                    *
 *                                                                  * 
 * Args:   parent  - the parent of the window to be created         *
 * Return: nothing                                                  *
\********************************************************************/
void
gnc_commodities_dialog (GtkWidget * parent)
{
  CommoditiesDialog *cd;
  gint component_id;

  if (gnc_forall_gui_components (DIALOG_COMMODITIES_CM_CLASS,
				 show_handler, NULL))
      return;

  cd = g_new0 (CommoditiesDialog, 1);

  gnc_commodities_dialog_create (parent, cd);

  component_id = gnc_register_gui_component (DIALOG_COMMODITIES_CM_CLASS,
                                             refresh_handler, close_handler,
                                             cd);

  gtk_widget_grab_focus (GTK_WIDGET(cd->commodity_tree));

  gtk_widget_show (cd->dialog);
}
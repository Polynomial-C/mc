/** \file listbox-window.h
 *  \brief Header: Listbox widget, a listbox within dialog window
 */

#ifndef MC__LISTBOX_DIALOG_H
#define MC__LISTBOX_DIALOG_H

/*** typedefs(not structures) and defined constants **********************************************/

#define LISTBOX_APPEND_TEXT(l, h, t, d, f)                                                         \
    listbox_add_item (l->list, LISTBOX_APPEND_AT_END, h, t, d, f)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    WDialog *dlg;
    WListbox *list;
} Listbox;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Listbox utility functions */
Listbox *listbox_window_centered_new (int center_y, int center_x, int lines, int cols,
                                      const char *title, const char *help);
Listbox *listbox_window_new (int lines, int cols, const char *title, const char *help);
int listbox_run (Listbox *l);
void *listbox_run_with_data (Listbox *l, const void *select);

/*** inline functions ****************************************************************************/

#endif

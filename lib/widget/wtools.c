/*
   Widget based utility functions.

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.

   Authors:
   Miguel de Icaza, 1994, 1995, 1996
   Radek Doulik, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1995
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file wtools.c
 *  \brief Source: widget based utility functions
 */

#include <config.h>

#include <stdarg.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"  // tty_getch()
#include "lib/strutil.h"
#include "lib/util.h"  // tilde_expand()
#include "lib/widget.h"
#include "lib/event.h"  // mc_event_raise()

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static WDialog *last_query_dlg;

static int sel_pos = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** default query callback, used to reposition query */

static cb_ret_t
query_default_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_RESIZE:
        if ((w->pos_flags & WPOS_CENTER) == 0)
        {
            WDialog *prev_dlg = NULL;
            int ypos, xpos;
            WRect r;

            // get dialog under h
            if (top_dlg != NULL)
            {
                if (top_dlg->data != (void *) h)
                    prev_dlg = DIALOG (top_dlg->data);
                else
                {
                    GList *p;

                    /* Top dialog is current if it is visible.
                       Get previous dialog in stack */
                    p = g_list_next (top_dlg);
                    if (p != NULL)
                        prev_dlg = DIALOG (p->data);
                }
            }

            // if previous dialog is not fullscreen'd -- overlap it
            if (prev_dlg == NULL || (WIDGET (prev_dlg)->pos_flags & WPOS_FULLSCREEN) != 0)
                ypos = LINES / 3 - (w->rect.lines - 3) / 2;
            else
                ypos = WIDGET (prev_dlg)->rect.y + 2;

            // if dialog is too high, place it centered
            if (ypos + w->rect.lines < LINES / 2)
                w->pos_flags |= WPOS_CENTER;

            xpos = COLS / 2 - w->rect.cols / 2;

            // set position
            rect_init (&r, ypos, xpos, w->rect.lines, w->rect.cols);

            return dlg_default_callback (w, NULL, MSG_RESIZE, 0, &r);
        }
        MC_FALLTHROUGH;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Show message box from background */

#ifdef ENABLE_BACKGROUND
static void
bg_message (int dummy, int *flags, char *title, const char *text)
{
    (void) dummy;
    title = g_strconcat (_ ("Background process:"), " ", title, (char *) NULL);
    query_dialog (title, text, *flags, 1, _ ("&OK"));
    g_free (title);
}
#endif

/* --------------------------------------------------------------------------------------------- */

/**
 * Show dialog, not background safe.
 *
 * If the arguments "header" and "text" should be translated,
 * that MUST be done by the caller of fg_input_dialog_help().
 *
 * The argument "history_name" holds the name of a section
 * in the history file. Data entered in the input field of
 * the dialog box will be stored there.
 *
 */
static char *
fg_input_dialog_help (const char *header, const char *text, const char *help,
                      const char *history_name, const char *def_text, gboolean strip_password,
                      input_complete_t completion_flags)
{
    char *p_text;
    char histname[BUF_TINY] = "inp|";
    gboolean is_passwd = FALSE;
    char *my_str = NULL;

    // label text
    p_text = g_strstrip (g_strdup (text));

    // input history
    if (history_name != NULL && *history_name != '\0')
        g_strlcpy (histname + 3, history_name, sizeof (histname) - 3);

    /* The special value of def_text is used to identify password boxes
       and hide characters with "*".  Don't save passwords in history! */
    if (def_text == INPUT_PASSWORD)
    {
        is_passwd = TRUE;
        histname[3] = '\0';
        def_text = "";
    }

    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (p_text, input_label_above, def_text, histname, &my_str, NULL,
                             is_passwd, strip_password, completion_flags),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };

    WRect r = { -1, -1, 0, COLS / 2 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = header,
        .help = help,
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    const int ret = quick_dialog (&qdlg);

    g_free (p_text);

    return (ret != B_CANCEL) ? my_str : NULL;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
static int
wtools_parent_call (void *routine, gpointer ctx, int argc, ...)
{
    ev_background_parent_call_t event_data;

    event_data.routine = routine;
    event_data.ctx = ctx;
    event_data.argc = argc;
    va_start (event_data.ap, argc);
    mc_event_raise (MCEVENT_GROUP_CORE, "background_parent_call", (gpointer) &event_data);
    va_end (event_data.ap);
    return event_data.ret.i;
}

/* --------------------------------------------------------------------------------------------- */

static char *
wtools_parent_call_string (void *routine, int argc, ...)
{
    ev_background_parent_call_t event_data;

    event_data.routine = routine;
    event_data.argc = argc;
    va_start (event_data.ap, argc);
    mc_event_raise (MCEVENT_GROUP_CORE, "background_parent_call_string", (gpointer) &event_data);
    va_end (event_data.ap);
    return event_data.ret.s;
}
#endif

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Used to ask questions to the user */
int
query_dialog (const char *header, const char *text, int flags, int count, ...)
{
    WDialog *query_dlg;
    WGroup *g;
    GPtrArray *buttons = NULL;
    WButton *button;
    int win_width = 0;
    int i;
    int result = -1;
    int cols, lines;
    const int *query_colors = (flags & D_ERROR) != 0 ? alarm_colors : dialog_colors;
    widget_pos_flags_t pos_flags =
        (flags & D_CENTER) != 0 ? (WPOS_CENTER | WPOS_TRYUP) : WPOS_KEEP_DEFAULT;

    if (header == MSG_ERROR)
        header = _ ("Error");

    if (count > 0)
    {
        va_list ap;

        buttons = g_ptr_array_sized_new ((guint) count);

        va_start (ap, count);
        for (i = 0; i < count; i++)
        {
            const char *cp = va_arg (ap, char *);

            button = button_new (1, 1, B_USER + i, NORMAL_BUTTON, cp, NULL);
            win_width += button_get_width (button) + 1;
            g_ptr_array_add (buttons, button);
        }
        va_end (ap);
    }

    const int header_cols = str_term_width1 (header);

    // count coordinates
    str_msg_term_size (text, &lines, &cols);
    cols = 6 + MAX (win_width, MAX (header_cols, cols));
    lines += 4 + (count > 0 ? 2 : 0);

    // prepare dialog
    query_dlg = dlg_create (TRUE, 0, 0, lines, cols, pos_flags, FALSE, query_colors,
                            query_default_callback, NULL, "[QueryBox]", header);
    g = GROUP (query_dlg);

    if (count > 0)
    {
        WButton *defbutton = NULL;

        group_add_widget_autopos (g, label_new (2, 3, text), WPOS_KEEP_TOP | WPOS_CENTER_HORZ,
                                  NULL);
        group_add_widget (g, hline_new (lines - 4, -1, -1));

        cols = (cols - win_width - 2) / 2 + 2;

        for (i = 0; i < count; i++)
        {
            button = BUTTON (g_ptr_array_index (buttons, i));
            WIDGET (button)->rect.y = lines - 3;
            WIDGET (button)->rect.x = cols;
            group_add_widget (g, button);

            cols += button_get_width (button) + 1;

            if (i == sel_pos)
                defbutton = button;
        }

        g_ptr_array_free (buttons, FALSE);

        // do resize before running and selecting any widget
        send_message (query_dlg, NULL, MSG_RESIZE, 0, NULL);

        if (defbutton != NULL)
            widget_select (WIDGET (defbutton));

        // run dialog and make result
        switch (dlg_run (query_dlg))
        {
        case B_CANCEL:
            break;
        default:
            result = query_dlg->ret_value - B_USER;
        }

        // free used memory
        widget_destroy (WIDGET (query_dlg));
    }
    else
    {
        group_add_widget_autopos (g, label_new (2, 3, text), WPOS_KEEP_TOP | WPOS_CENTER_HORZ,
                                  NULL);
        group_add_widget (g, button_new (0, 0, 0, HIDDEN_BUTTON, "-", NULL));
        last_query_dlg = query_dlg;
    }
    sel_pos = 0;
    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
query_set_sel (int new_sel)
{
    sel_pos = new_sel;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create message dialog.  The caller must call dlg_run_done() and
 * widget_destroy() to dismiss it.  Not safe to call from background.
 */

WDialog *
create_message (int flags, const char *title, const char *text, ...)
{
    va_list args;
    WDialog *d;
    char *p;

    va_start (args, text);
    p = g_strdup_vprintf (text, args);
    va_end (args);

    query_dialog (title, p, flags, 0);
    d = last_query_dlg;

    // do resize before initing and running
    send_message (d, NULL, MSG_RESIZE, 0, NULL);

    dlg_init (d);
    g_free (p);

    return d;
}

/* --------------------------------------------------------------------------------------------- */
/** Show message box, background safe */

void
message (int flags, const char *title, const char *text, ...)
{
    char *p;
    va_list ap;

    va_start (ap, text);
    p = g_strdup_vprintf (text, ap);
    va_end (ap);

    if (title == MSG_ERROR)
        title = _ ("Error");

#ifdef ENABLE_BACKGROUND
    if (mc_global.we_are_background)
    {
        union
        {
            void *p;
            void (*f) (int, int *, char *, const char *);
        } func;

        func.f = bg_message;

        wtools_parent_call (func.p, NULL, 3, sizeof (flags), &flags, strlen (title), title,
                            strlen (p), p);
    }
    else
#endif
        query_dialog (title, p, flags, 1, _ ("&OK"));

    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */
/** Show error message box */

gboolean
mc_error_message (GError **mcerror, int *code)
{
    if (mcerror == NULL || *mcerror == NULL)
        return FALSE;

    if ((*mcerror)->code == 0)
        message (D_ERROR, MSG_ERROR, "%s", (*mcerror)->message);
    else
        message (D_ERROR, MSG_ERROR, _ ("%s (%d)"), (*mcerror)->message, (*mcerror)->code);

    if (code != NULL)
        *code = (*mcerror)->code;

    g_error_free (*mcerror);
    *mcerror = NULL;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Show input dialog, background safe.
 *
 * If the arguments "header" and "text" should be translated,
 * that MUST be done by the caller of these wrappers.
 */

char *
input_dialog_help (const char *header, const char *text, const char *help, const char *history_name,
                   const char *def_text, gboolean strip_password, input_complete_t completion_flags)
{
#ifdef ENABLE_BACKGROUND
    if (mc_global.we_are_background)
    {
        union
        {
            void *p;
            char *(*f) (const char *, const char *, const char *, const char *, const char *,
                        gboolean, input_complete_t);
        } func;
        func.f = fg_input_dialog_help;
        return wtools_parent_call_string (
            func.p, 7, strlen (header), header, strlen (text), text, strlen (help), help,
            strlen (history_name), history_name, strlen (def_text), def_text, sizeof (gboolean),
            strip_password, sizeof (input_complete_t), completion_flags);
    }
    else
#endif
        return fg_input_dialog_help (header, text, help, history_name, def_text, strip_password,
                                     completion_flags);
}

/* --------------------------------------------------------------------------------------------- */
/** Show input dialog with default help, background safe */

char *
input_dialog (const char *header, const char *text, const char *history_name, const char *def_text,
              input_complete_t completion_flags)
{
    return input_dialog_help (header, text, "[Input Line Keys]", history_name, def_text, FALSE,
                              completion_flags);
}

/* --------------------------------------------------------------------------------------------- */

char *
input_expand_dialog (const char *header, const char *text, const char *history_name,
                     const char *def_text, input_complete_t completion_flags)
{
    char *result;

    result = input_dialog (header, text, history_name, def_text, completion_flags);
    if (result)
    {
        char *expanded;

        expanded = tilde_expand (result);
        g_free (result);
        return expanded;
    }
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create status message window object and initialize it
 *
 * @param title window title
 * @param delay initial delay to raise window in seconds
 * @param init_cb callback to initialize user-defined part of status message
 * @param update_cb callback to update of status message
 * @param deinit_cb callback to deinitialize user-defined part of status message
 *
 * @return newly allocate status message window
 */

status_msg_t *
status_msg_create (const char *title, double delay, status_msg_cb init_cb,
                   status_msg_update_cb update_cb, status_msg_cb deinit_cb)
{
    status_msg_t *sm;

    sm = g_try_new (status_msg_t, 1);
    status_msg_init (sm, title, delay, init_cb, update_cb, deinit_cb);

    return sm;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Destroy status message window object
 *
 * @param sm status message window object
 */

void
status_msg_destroy (status_msg_t *sm)
{
    status_msg_deinit (sm);
    g_free (sm);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Initialize already created status message window object
 *
 * @param sm status message window object
 * @param title window title
 * @param delay initial delay to raise window in seconds
 * @param init_cb callback to initialize user-defined part of status message
 * @param update_cb callback to update of status message
 * @param deinit_cb callback to deinitialize user-defined part of status message
 */

void
status_msg_init (status_msg_t *sm, const char *title, double delay, status_msg_cb init_cb,
                 status_msg_update_cb update_cb, status_msg_cb deinit_cb)
{
    gint64 start;

    // repaint screen to remove previous finished dialog
    mc_refresh ();

    start = g_get_monotonic_time ();

    sm->dlg = dlg_create (TRUE, 0, 0, 7, MIN (MAX (40, COLS / 2), COLS), WPOS_CENTER, FALSE,
                          dialog_colors, NULL, NULL, NULL, title);
    sm->start = start;
    sm->delay = (gint64) (delay * G_USEC_PER_SEC);
    sm->block = FALSE;

    sm->init = init_cb;
    sm->update = update_cb;
    sm->deinit = deinit_cb;

    if (sm->init != NULL)
        sm->init (sm);

    if (mc_time_elapsed (&start, sm->delay))
    {
        // We will manage the dialog without any help, that's why we have to call dlg_init
        dlg_init (sm->dlg);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Deinitialize status message window object
 *
 * @param sm status message window object
 */

void
status_msg_deinit (status_msg_t *sm)
{
    if (sm == NULL)
        return;

    if (sm->deinit != NULL)
        sm->deinit (sm);

    // close and destroy dialog
    dlg_run_done (sm->dlg);
    widget_destroy (WIDGET (sm->dlg));
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Update status message window
 *
 * @param sm status message window object
 *
 * @return value of pressed key
 */

int
status_msg_common_update (status_msg_t *sm)
{
    int c;
    Gpm_Event event;

    if (sm == NULL)
        return B_ENTER;

    // This should not happen, but...
    if (sm->dlg == NULL)
        return B_ENTER;

    if (widget_get_state (WIDGET (sm->dlg), WST_CONSTRUCT))
    {
        // dialog is not shown yet

        // do not change sm->start
        gint64 start = sm->start;

        if (mc_time_elapsed (&start, sm->delay))
            dlg_init (sm->dlg);

        return B_ENTER;
    }

    event.x = -1;  // Don't show the GPM cursor
    c = tty_get_event (&event, FALSE, sm->block);
    if (c == EV_NONE)
        return B_ENTER;

    /* Reinitialize by non-B_CANCEL value to avoid old values
       after events other than selecting a button */
    sm->dlg->ret_value = B_ENTER;
    dlg_process_event (sm->dlg, c, &event);

    return sm->dlg->ret_value;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback to initialize already created simple status message window object
 *
 * @param sm status message window object
 */

void
simple_status_msg_init_cb (status_msg_t *sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    Widget *wd = WIDGET (sm->dlg);
    WGroup *wg = GROUP (sm->dlg);
    WRect r;

    const char *b_name = N_ ("&Abort");
    int b_width;
    int wd_width, y;
    Widget *b;

#ifdef ENABLE_NLS
    b_name = _ (b_name);
#endif

    b_width = str_term_width1 (b_name) + 4;
    wd_width = MAX (wd->rect.cols, b_width + 6);

    y = 2;
    ssm->label = label_new (y++, 3, NULL);
    group_add_widget_autopos (wg, ssm->label, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, NULL);
    group_add_widget (wg, hline_new (y++, -1, -1));
    b = WIDGET (button_new (y++, 3, B_CANCEL, NORMAL_BUTTON, b_name, NULL));
    group_add_widget_autopos (wg, b, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, NULL);

    r = wd->rect;
    r.lines = y + 2;
    r.cols = wd_width;
    widget_set_size_rect (wd, &r);
}

/* --------------------------------------------------------------------------------------------- */

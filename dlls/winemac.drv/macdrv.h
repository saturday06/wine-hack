/*
 * MACDRV windowing driver
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright 1999 Patrik Stridvall
 * Copyright 2011, 2012, 2013 Ken Thomases for CodeWeavers Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_MACDRV_H
#define __WINE_MACDRV_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include "macdrv_cocoa.h"
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wine/debug.h"
#include "wine/gdi_driver.h"


extern const char* debugstr_cf(CFTypeRef t) DECLSPEC_HIDDEN;

static inline CGRect cgrect_from_rect(RECT rect)
{
    if (rect.left >= rect.right || rect.top >= rect.bottom)
        return CGRectNull;
    return CGRectMake(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

static inline RECT rect_from_cgrect(CGRect cgrect)
{
    static const RECT empty;

    if (!CGRectIsNull(cgrect))
    {
        RECT rect = { CGRectGetMinX(cgrect), CGRectGetMinY(cgrect),
                      CGRectGetMaxX(cgrect), CGRectGetMaxY(cgrect) };
        return rect;
    }

    return empty;
}

static inline const char *wine_dbgstr_cgrect(CGRect cgrect)
{
    return wine_dbg_sprintf("(%g,%g)-(%g,%g)", CGRectGetMinX(cgrect), CGRectGetMinY(cgrect),
                            CGRectGetMaxX(cgrect), CGRectGetMaxY(cgrect));
}


/**************************************************************************
 * Mac GDI driver
 */

extern CGRect macdrv_get_desktop_rect(void) DECLSPEC_HIDDEN;
extern void macdrv_reset_device_metrics(void) DECLSPEC_HIDDEN;


/**************************************************************************
 * Mac USER driver
 */

/* Mac driver private messages, must be in the range 0x80001000..0x80001fff */
enum macdrv_window_messages
{
    WM_MACDRV_SET_WIN_REGION = 0x80001000,
    WM_MACDRV_UPDATE_DESKTOP_RECT,
    WM_MACDRV_RESET_DEVICE_METRICS,
    WM_MACDRV_DISPLAYCHANGE,
};

struct macdrv_thread_data
{
    macdrv_event_queue          queue;
    const macdrv_event         *current_event;
    CFDataRef                   keyboard_layout_uchr;
    CGEventSourceKeyboardType   keyboard_type;
    int                         iso_keyboard;
    CGEventFlags                last_modifiers;
    UInt32                      dead_key_state;
    HKL                         active_keyboard_layout;
    WORD                        keyc2vkey[128];
    WORD                        keyc2scan[128];
};

extern DWORD thread_data_tls_index DECLSPEC_HIDDEN;

extern struct macdrv_thread_data *macdrv_init_thread_data(void) DECLSPEC_HIDDEN;

static inline struct macdrv_thread_data *macdrv_thread_data(void)
{
    return TlsGetValue(thread_data_tls_index);
}


/* macdrv private window data */
struct macdrv_win_data
{
    HWND                hwnd;                   /* hwnd that this private data belongs to */
    macdrv_window       cocoa_window;
    RECT                window_rect;            /* USER window rectangle relative to parent */
    RECT                whole_rect;             /* Mac window rectangle for the whole window relative to parent */
    RECT                client_rect;            /* client area relative to parent */
    COLORREF            color_key;              /* color key for layered window; CLR_INVALID is not color keyed */
    BOOL                on_screen : 1;          /* is window ordered in? (minimized or not) */
    BOOL                shaped : 1;             /* is window using a custom region shape? */
    BOOL                layered : 1;            /* is window layered and with valid attributes? */
    BOOL                per_pixel_alpha : 1;    /* is window using per-pixel alpha? */
    BOOL                minimized : 1;          /* is window minimized? */
    struct window_surface *surface;
};

extern RGNDATA *get_region_data(HRGN hrgn, HDC hdc_lptodp) DECLSPEC_HIDDEN;
extern struct window_surface *create_surface(macdrv_window window, const RECT *rect, BOOL use_alpha) DECLSPEC_HIDDEN;
extern void set_window_surface(macdrv_window window, struct window_surface *window_surface) DECLSPEC_HIDDEN;
extern void set_surface_use_alpha(struct window_surface *window_surface, BOOL use_alpha) DECLSPEC_HIDDEN;

extern void macdrv_window_close_requested(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_frame_changed(HWND hwnd, CGRect frame) DECLSPEC_HIDDEN;
extern void macdrv_window_got_focus(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_window_lost_focus(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_app_deactivated(void) DECLSPEC_HIDDEN;
extern void macdrv_window_did_minimize(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_did_unminimize(HWND hwnd) DECLSPEC_HIDDEN;

extern void macdrv_mouse_button(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_mouse_moved(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_mouse_scroll(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;

extern void macdrv_compute_keyboard_layout(struct macdrv_thread_data *thread_data) DECLSPEC_HIDDEN;
extern void macdrv_keyboard_changed(const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_key_event(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;

extern void macdrv_displays_changed(const macdrv_event *event) DECLSPEC_HIDDEN;

#endif  /* __WINE_MACDRV_H */

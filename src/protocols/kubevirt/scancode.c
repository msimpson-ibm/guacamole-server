/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "config.h"
#include "scancode.h"
#include "keymap-data.h"
#include <stddef.h>

#include <stdint.h>
#include <X11/keysym.h>

/**
 * Mapping entry from X11 keysym to XT scancode with modifiers.
 */
typedef struct guac_kubevirt_scancode_map {
    uint32_t keysym;
    uint16_t scancode;
    int extended;  /* 1 if this requires 0xE0 prefix */
    int modifiers; /* Bitmask of GUAC_KUBEVIRT_MODIFIER_* flags */
} guac_kubevirt_scancode_map;

/**
 * Comprehensive X11 keysym to XT scancode mapping table.
 * Based on the standard PC/AT keyboard layout (scancode set 1).
 */
static const guac_kubevirt_scancode_map scancode_table[] = {
    /* Main alphanumeric keys */
    { XK_a, 0x1E, 0, 0 }, { XK_A, 0x1E, 0, 0 },
    { XK_b, 0x30, 0, 0 }, { XK_B, 0x30, 0, 0 },
    { XK_c, 0x2E, 0, 0 }, { XK_C, 0x2E, 0, 0 },
    { XK_d, 0x20, 0, 0 }, { XK_D, 0x20, 0, 0 },
    { XK_e, 0x12, 0, 0 }, { XK_E, 0x12, 0, 0 },
    { XK_f, 0x21, 0, 0 }, { XK_F, 0x21, 0, 0 },
    { XK_g, 0x22, 0, 0 }, { XK_G, 0x22, 0, 0 },
    { XK_h, 0x23, 0, 0 }, { XK_H, 0x23, 0, 0 },
    { XK_i, 0x17, 0, 0 }, { XK_I, 0x17, 0, 0 },
    { XK_j, 0x24, 0, 0 }, { XK_J, 0x24, 0, 0 },
    { XK_k, 0x25, 0 }, { XK_K, 0x25, 0 },
    { XK_l, 0x26, 0 }, { XK_L, 0x26, 0 },
    { XK_m, 0x32, 0 }, { XK_M, 0x32, 0 },
    { XK_n, 0x31, 0 }, { XK_N, 0x31, 0 },
    { XK_o, 0x18, 0 }, { XK_O, 0x18, 0 },
    { XK_p, 0x19, 0 }, { XK_P, 0x19, 0 },
    { XK_q, 0x10, 0 }, { XK_Q, 0x10, 0 },
    { XK_r, 0x13, 0 }, { XK_R, 0x13, 0 },
    { XK_s, 0x1F, 0 }, { XK_S, 0x1F, 0 },
    { XK_t, 0x14, 0 }, { XK_T, 0x14, 0 },
    { XK_u, 0x16, 0 }, { XK_U, 0x16, 0 },
    { XK_v, 0x2F, 0 }, { XK_V, 0x2F, 0 },
    { XK_w, 0x11, 0 }, { XK_W, 0x11, 0 },
    { XK_x, 0x2D, 0 }, { XK_X, 0x2D, 0 },
    { XK_y, 0x15, 0 }, { XK_Y, 0x15, 0 },
    { XK_z, 0x2C, 0 }, { XK_Z, 0x2C, 0 },

    /* Number row */
    { XK_1, 0x02, 0 }, { XK_exclam, 0x02, 0 },
    { XK_2, 0x03, 0 }, { XK_at, 0x03, 0 },
    { XK_3, 0x04, 0 }, { XK_numbersign, 0x04, 0 },
    { XK_4, 0x05, 0 }, { XK_dollar, 0x05, 0 },
    { XK_5, 0x06, 0 }, { XK_percent, 0x06, 0 },
    { XK_6, 0x07, 0 }, { XK_asciicircum, 0x07, 0 },
    { XK_7, 0x08, 0 }, { XK_ampersand, 0x08, 0 },
    { XK_8, 0x09, 0 }, { XK_asterisk, 0x09, 0 },
    { XK_9, 0x0A, 0 }, { XK_parenleft, 0x0A, 0 },
    { XK_0, 0x0B, 0 }, { XK_parenright, 0x0B, 0 },

    /* Special characters on number row */
    { XK_minus, 0x0C, 0 }, { XK_underscore, 0x0C, 0 },
    { XK_equal, 0x0D, 0 }, { XK_plus, 0x0D, 0 },

    /* Brackets and punctuation */
    { XK_bracketleft, 0x1A, 0 }, { XK_braceleft, 0x1A, 0 },
    { XK_bracketright, 0x1B, 0 }, { XK_braceright, 0x1B, 0 },
    { XK_semicolon, 0x27, 0 }, { XK_colon, 0x27, 0 },
    { XK_apostrophe, 0x28, 0 }, { XK_quotedbl, 0x28, 0 },
    { XK_grave, 0x29, 0 }, { XK_asciitilde, 0x29, 0 },
    { XK_backslash, 0x2B, 0 }, { XK_bar, 0x2B, 0 },
    { XK_comma, 0x33, 0 }, { XK_less, 0x33, 0 },
    { XK_period, 0x34, 0 }, { XK_greater, 0x34, 0 },
    { XK_slash, 0x35, 0 }, { XK_question, 0x35, 0 },

    /* Control keys */
    { XK_Escape, 0x01, 0 },
    { XK_Tab, 0x0F, 0 },
    { XK_ISO_Left_Tab, 0x0F, 0 },
    { XK_BackSpace, 0x0E, 0 },
    { XK_Return, 0x1C, 0 },
    { XK_space, 0x39, 0 },

    /* Modifier keys */
    { XK_Shift_L, 0x2A, 0 },
    { XK_Shift_R, 0x36, 0 },
    { XK_Control_L, 0x1D, 0 },
    { XK_Control_R, 0x1D, 1 },  /* Extended */
    { XK_Alt_L, 0x38, 0 },
    { XK_Alt_R, 0x38, 1 },  /* Extended (AltGr) */
    { XK_Meta_L, 0x5B, 1 },  /* Extended (Windows key) */
    { XK_Meta_R, 0x5C, 1 },  /* Extended (Windows key) */
    { XK_Super_L, 0x5B, 1 },  /* Extended (Windows key) */
    { XK_Super_R, 0x5C, 1 },  /* Extended (Windows key) */
    { XK_Menu, 0x5D, 1 },  /* Extended (Menu key) */

    /* Function keys */
    { XK_F1, 0x3B, 0 },
    { XK_F2, 0x3C, 0 },
    { XK_F3, 0x3D, 0 },
    { XK_F4, 0x3E, 0 },
    { XK_F5, 0x3F, 0 },
    { XK_F6, 0x40, 0 },
    { XK_F7, 0x41, 0 },
    { XK_F8, 0x42, 0 },
    { XK_F9, 0x43, 0 },
    { XK_F10, 0x44, 0 },
    { XK_F11, 0x57, 0 },
    { XK_F12, 0x58, 0 },

    /* Lock keys */
    { XK_Caps_Lock, 0x3A, 0 },
    { XK_Num_Lock, 0x45, 0 },
    { XK_Scroll_Lock, 0x46, 0 },

    /* Cursor control (extended keys) */
    { XK_Insert, 0x52, 1 },
    { XK_Delete, 0x53, 1 },
    { XK_Home, 0x47, 1 },
    { XK_End, 0x4F, 1 },
    { XK_Page_Up, 0x49, 1 },
    { XK_Prior, 0x49, 1 },
    { XK_Page_Down, 0x51, 1 },
    { XK_Next, 0x51, 1 },

    /* Arrow keys (extended) */
    { XK_Up, 0x48, 1 },
    { XK_Down, 0x50, 1 },
    { XK_Left, 0x4B, 1 },
    { XK_Right, 0x4D, 1 },

    /* Numeric keypad */
    { XK_KP_0, 0x52, 0 },
    { XK_KP_1, 0x4F, 0 },
    { XK_KP_2, 0x50, 0 },
    { XK_KP_3, 0x51, 0 },
    { XK_KP_4, 0x4B, 0 },
    { XK_KP_5, 0x4C, 0 },
    { XK_KP_6, 0x4D, 0 },
    { XK_KP_7, 0x47, 0 },
    { XK_KP_8, 0x48, 0 },
    { XK_KP_9, 0x49, 0 },
    { XK_KP_Decimal, 0x53, 0 },
    { XK_KP_Delete, 0x53, 0 },
    { XK_KP_Enter, 0x1C, 1 },  /* Extended */
    { XK_KP_Add, 0x4E, 0 },
    { XK_KP_Subtract, 0x4A, 0 },
    { XK_KP_Multiply, 0x37, 0 },
    { XK_KP_Divide, 0x35, 1 },  /* Extended */

    /* Print Screen / SysRq is special - requires two scancodes */
    { XK_Print, 0x37, 1 },  /* Extended, but also needs 0x2A prefix when pressed */
    { XK_Sys_Req, 0x54, 0 },

    /* Pause/Break is also special */
    { XK_Pause, 0x45, 0 },  /* Actually sends E1 1D 45, but simplified here */
    { XK_Break, 0x46, 0 },

    /* European keyboard keys */
    { XK_less, 0x56, 0 },  /* 102nd key on European keyboards */
    { XK_greater, 0x56, 0 },

    /* German/Latin characters - Unicode keysyms
     * These are typically produced via dead keys or AltGr on German keyboards
     * We map them to their corresponding physical key positions on German layout */
    { 0x00E4, 0x28, 0, 0 },  /* ä (a-umlaut) - German keyboard position */
    { 0x00F6, 0x27, 0, 0 },  /* ö (o-umlaut) - German keyboard position */
    { 0x00FC, 0x1A, 0, 0 },  /* ü (u-umlaut) - German keyboard position */
    { 0x00C4, 0x28, 0, 0 },  /* Ä (A-umlaut) */
    { 0x00D6, 0x27, 0, 0 },  /* Ö (O-umlaut) */
    { 0x00DC, 0x1A, 0, 0 },  /* Ü (U-umlaut) */
    { 0x00DF, 0x0C, 0, 0 },  /* ß (sharp s/eszett) - German keyboard position */

    /* End of table marker */
    { 0, 0, 0 }
};

int guac_kubevirt_keysym_to_scancode(uint32_t keysym, guac_kubevirt_scancode_mapping* mapping,
                                      const char* layout_name) {
    
    if (!mapping)
        return 0;
    
    /* First, try layout-specific mappings if a layout is specified */
    if (layout_name != NULL) {
        const guac_kubevirt_layout_map* layout_map = guac_kubevirt_get_layout_keymap(layout_name);
        if (layout_map != NULL) {
            for (int i = 0; layout_map[i].keysym != 0; i++) {
                if (layout_map[i].keysym == keysym) {
                    mapping->scancode = layout_map[i].scancode;
                    mapping->extended = layout_map[i].extended;
                    mapping->modifiers = layout_map[i].modifiers;
                    return 1;
                }
            }
        }
    }
    
    /* Fall back to base mapping table */
    for (int i = 0; scancode_table[i].keysym != 0; i++) {
        if (scancode_table[i].keysym == keysym) {
            mapping->scancode = scancode_table[i].scancode;
            mapping->extended = scancode_table[i].extended;
            mapping->modifiers = scancode_table[i].modifiers;
            return 1;
        }
    }

    /* No mapping found */
    mapping->scancode = 0;
    mapping->extended = 0;
    mapping->modifiers = 0;
    return 0;
}

// Made with Bob

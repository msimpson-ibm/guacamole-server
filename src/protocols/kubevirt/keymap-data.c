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
#include "keymap-data.h"
#include "scancode.h"

#include <string.h>
#include <strings.h>

/**
 * German keyboard layout (de-de-qwertz) specific mappings.
 * Based on QEMU keymap: pc-bios/keymaps/de
 */
const guac_kubevirt_layout_map guac_kubevirt_keymap_de[] = {
    /* QWERTZ layout - Y and Z are swapped compared to QWERTY */
    { 0x0079, 0x2C, 0, 0 },  /* y (lowercase) */
    { 0x0059, 0x2C, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* Y (uppercase) */
    { 0x007A, 0x15, 0, 0 },  /* z (lowercase) */
    { 0x005A, 0x15, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* Z (uppercase) */
    
    /* German-specific characters */
    { 0x00E4, 0x28, 0, 0 },  /* ä (a-umlaut) */
    { 0x00C4, 0x28, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* Ä (A-umlaut) = Shift+ä */
    { 0x00F6, 0x27, 0, 0 },  /* ö (o-umlaut) */
    { 0x00D6, 0x27, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* Ö (O-umlaut) = Shift+ö */
    { 0x00FC, 0x1A, 0, 0 },  /* ü (u-umlaut) */
    { 0x00DC, 0x1A, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* Ü (U-umlaut) = Shift+ü */
    { 0x00DF, 0x0C, 0, 0 },  /* ß (sharp s/eszett) */
    { 0x003F, 0x0C, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ? (question mark) = Shift+ß */
    
    /* German number row - different from US layout */
    { 0x0031, 0x02, 0, 0 },  /* 1 */
    { 0x0021, 0x02, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ! = Shift+1 */
    { 0x0032, 0x03, 0, 0 },  /* 2 */
    { 0x0022, 0x03, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* " = Shift+2 */
    { 0x0033, 0x04, 0, 0 },  /* 3 */
    { 0x00A7, 0x04, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* § = Shift+3 */
    { 0x0034, 0x05, 0, 0 },  /* 4 */
    { 0x0024, 0x05, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* $ = Shift+4 */
    { 0x0035, 0x06, 0, 0 },  /* 5 */
    { 0x0025, 0x06, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* % = Shift+5 */
    { 0x0036, 0x07, 0, 0 },  /* 6 */
    { 0x0026, 0x07, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* & = Shift+6 */
    { 0x0037, 0x08, 0, 0 },  /* 7 */
    { 0x002F, 0x08, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* / = Shift+7 */
    { 0x0038, 0x09, 0, 0 },  /* 8 */
    { 0x0028, 0x09, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ( = Shift+8 */
    { 0x0039, 0x0A, 0, 0 },  /* 9 */
    { 0x0029, 0x0A, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ) = Shift+9 */
    { 0x0030, 0x0B, 0, 0 },  /* 0 */
    { 0x003D, 0x0B, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* = = Shift+0 */
    
    /* German punctuation - different positions than US */
    { 0x002D, 0x35, 0, 0 },  /* - (minus/hyphen) */
    { 0x005F, 0x35, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* _ (underscore) = Shift+- */
    { 0x002B, 0x1B, 0, 0 },  /* + (plus) */
    { 0x002A, 0x1B, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* * (asterisk) = Shift++ */
    { 0x0023, 0x2B, 0, 0 },  /* # (hash/number sign) */
    { 0x0027, 0x2B, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ' (apostrophe) = Shift+# */
    { 0x003C, 0x56, 0, 0 },  /* < (less than) */
    { 0x003E, 0x56, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* > (greater than) = Shift+< */
    { 0x002C, 0x33, 0, 0 },  /* , (comma) */
    { 0x003B, 0x33, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ; (semicolon) = Shift+, */
    { 0x002E, 0x34, 0, 0 },  /* . (period) */
    { 0x003A, 0x34, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* : (colon) = Shift+. */
    { 0x005E, 0x29, 0, 0 },  /* ^ (caret/circumflex) - dead key on German keyboard */
    { 0x00B0, 0x29, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ° (degree) = Shift+^ */
    { 0x0060, 0x0D, 0, 0 },  /* ` (grave accent) - dead key */
    { 0x00B4, 0x0D, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ´ (acute accent) = Shift+` */
    
    /* Additional German punctuation and special characters */
    { 0x0027, 0x2B, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ' (apostrophe) = Shift+# */
    { 0x002A, 0x1B, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* * (asterisk) = Shift++ */
    { 0x003B, 0x33, 0, 0 },  /* ; (semicolon) */
    { 0x003A, 0x33, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* : (colon) = Shift+, */
    { 0x005F, 0x0C, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* _ (underscore) = Shift+- */
    
    /* AltGr combinations for special characters */
    { 0x007C, 0x56, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* | = AltGr+< */
    { 0x0040, 0x10, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* @ = AltGr+Q */
    { 0x007B, 0x08, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* { = AltGr+7 */
    { 0x007D, 0x09, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* } = AltGr+0 */
    { 0x005B, 0x09, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* [ = AltGr+8 */
    { 0x005D, 0x0A, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* ] = AltGr+9 */
    { 0x005C, 0x0C, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* \ = AltGr+ß */
    { 0x007E, 0x1B, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* ~ = AltGr++ */
    
    /* End marker */
    { 0, 0, 0, 0 }
};

/**
 * French keyboard layout (fr-fr-azerty) specific mappings.
 * Based on QEMU keymap: pc-bios/keymaps/fr
 */
const guac_kubevirt_layout_map guac_kubevirt_keymap_fr[] = {
    /* French-specific characters */
    { 0x00E9, 0x03, 0, 0 },  /* é (e-acute) */
    { 0x00E8, 0x08, 0, 0 },  /* è (e-grave) */
    { 0x00E7, 0x0A, 0, 0 },  /* ç (c-cedilla) */
    { 0x00E0, 0x0B, 0, 0 },  /* à (a-grave) */
    { 0x00F9, 0x28, 0, 0 },  /* ù (u-grave) */
    
    /* Special characters requiring modifiers on French layout */
    { 0x007C, 0x07, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* | = AltGr+6 */
    { 0x0040, 0x0B, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* @ = AltGr+à */
    { 0x0023, 0x04, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* # = AltGr+" */
    { 0x007B, 0x05, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* { = AltGr+' */
    { 0x007D, 0x0D, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* } = AltGr+= */
    { 0x005B, 0x06, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* [ = AltGr+( */
    { 0x005D, 0x0C, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* ] = AltGr+) */
    { 0x005C, 0x09, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* \ = AltGr+8 */
    { 0x0060, 0x08, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* ` = AltGr+è */
    { 0x007E, 0x03, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* ~ = AltGr+é */
    { 0x005E, 0x0A, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* ^ = AltGr+ç */
    
    /* End marker */
    { 0, 0, 0, 0 }
};

/**
 * Spanish keyboard layout (es-es-qwerty) specific mappings.
 * Based on QEMU keymap: pc-bios/keymaps/es
 */
const guac_kubevirt_layout_map guac_kubevirt_keymap_es[] = {
    /* Spanish-specific characters */
    { 0x00F1, 0x27, 0, 0 },  /* ñ (n-tilde) */
    { 0x00D1, 0x27, 0, 0 },  /* Ñ (N-tilde) */
    { 0x00A1, 0x0D, 0, 0 },  /* ¡ (inverted exclamation) */
    { 0x00BF, 0x0D, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* ¿ (inverted question) */
    
    /* Special characters requiring modifiers on Spanish layout */
    { 0x002F, 0x08, 0, GUAC_KUBEVIRT_MODIFIER_SHIFT },  /* / = Shift+7 */
    { 0x007C, 0x56, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* | = AltGr+1 */
    { 0x0040, 0x03, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* @ = AltGr+2 */
    { 0x0023, 0x04, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* # = AltGr+3 */
    { 0x007B, 0x08, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* { = AltGr+' */
    { 0x007D, 0x09, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* } = AltGr+ç */
    { 0x005B, 0x1A, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* [ = AltGr+` */
    { 0x005D, 0x1B, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* ] = AltGr++ */
    { 0x005C, 0x0B, 0, GUAC_KUBEVIRT_MODIFIER_ALTGR },  /* \ = AltGr+º */
    
    /* End marker */
    { 0, 0, 0, 0 }
};

const guac_kubevirt_layout_map* guac_kubevirt_get_layout_keymap(const char* layout_name) {
    if (layout_name == NULL)
        return NULL;
    
    /* German layouts */
    if (strcasecmp(layout_name, "de-de-qwertz") == 0 ||
        strcasecmp(layout_name, "de") == 0)
        return guac_kubevirt_keymap_de;
    
    /* French layouts */
    if (strcasecmp(layout_name, "fr-fr-azerty") == 0 ||
        strcasecmp(layout_name, "fr") == 0)
        return guac_kubevirt_keymap_fr;
    
    /* Spanish layouts */
    if (strcasecmp(layout_name, "es-es-qwerty") == 0 ||
        strcasecmp(layout_name, "es") == 0)
        return guac_kubevirt_keymap_es;
    
    /* No layout-specific mapping found */
    return NULL;
}

// Made with Bob
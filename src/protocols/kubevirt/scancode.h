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

#ifndef GUAC_KUBEVIRT_SCANCODE_H
#define GUAC_KUBEVIRT_SCANCODE_H

#include <stdint.h>

/**
 * Modifier flags for scancodes.
 */
#define GUAC_KUBEVIRT_MODIFIER_SHIFT  0x01
#define GUAC_KUBEVIRT_MODIFIER_ALTGR  0x02

/**
 * Structure representing a scancode mapping with modifiers.
 */
typedef struct guac_kubevirt_scancode_mapping {
    uint16_t scancode;
    int extended;
    int modifiers;  /* Bitmask of GUAC_KUBEVIRT_MODIFIER_* flags */
} guac_kubevirt_scancode_mapping;

/**
 * Translates an X11 keysym to an XT scancode (set 1) with modifiers.
 * Returns 0 if no translation is available.
 *
 * @param keysym
 *     The X11 keysym to translate.
 *
 * @param mapping
 *     Pointer to store the resulting scancode mapping.
 *
 * @param layout_name
 *     Optional layout name for layout-specific mappings (e.g., "de-de-qwertz").
 *     If NULL, only base mappings are used.
 *
 * @return
 *     Non-zero if a mapping was found, zero otherwise.
 */
int guac_kubevirt_keysym_to_scancode(uint32_t keysym, guac_kubevirt_scancode_mapping* mapping,
                                      const char* layout_name);

#endif

// Made with Bob

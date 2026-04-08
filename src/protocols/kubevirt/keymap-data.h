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

#ifndef GUAC_KUBEVIRT_KEYMAP_DATA_H
#define GUAC_KUBEVIRT_KEYMAP_DATA_H

#include "scancode.h"
#include <stdint.h>

/**
 * Layout-specific keymap entry.
 */
typedef struct guac_kubevirt_layout_map {
    uint32_t keysym;
    uint16_t scancode;
    int extended;
    int modifiers;
} guac_kubevirt_layout_map;

/**
 * German keyboard layout specific mappings.
 * These override or supplement the base mappings for German layout.
 */
extern const guac_kubevirt_layout_map guac_kubevirt_keymap_de[];

/**
 * French keyboard layout specific mappings.
 */
extern const guac_kubevirt_layout_map guac_kubevirt_keymap_fr[];

/**
 * Spanish keyboard layout specific mappings.
 */
extern const guac_kubevirt_layout_map guac_kubevirt_keymap_es[];

/**
 * Get layout-specific keymap by name.
 *
 * @param layout_name
 *     The layout name (e.g., "de-de-qwertz", "fr-fr-azerty", "es-es-qwerty")
 *
 * @return
 *     Pointer to layout-specific keymap, or NULL if not found.
 */
const guac_kubevirt_layout_map* guac_kubevirt_get_layout_keymap(const char* layout_name);

#endif

// Made with Bob
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

#ifndef GUAC_KUBEVIRT_KEYMAP_H
#define GUAC_KUBEVIRT_KEYMAP_H

/**
 * Simplified keymap structure for KubeVirt.
 * This is a lightweight version that stores just the layout name.
 * VNC uses X11 keysyms directly, so we don't need the complex
 * scancode mappings that RDP requires.
 */
typedef struct guac_kubevirt_keymap {

    /**
     * Descriptive name of this keymap (e.g., "en-us-qwerty", "de-de-qwertz")
     */
    const char* name;

} guac_kubevirt_keymap;

/**
 * The name of the default keymap.
 */
#define GUAC_KUBEVIRT_DEFAULT_KEYMAP "en-us-qwerty"

/**
 * Return the keymap having the given name, if any, or NULL otherwise.
 *
 * @param name
 *     The name of the keymap to find.
 *
 * @return
 *     The keymap having the given name, or NULL if no such keymap exists.
 */
const guac_kubevirt_keymap* guac_kubevirt_keymap_find(const char* name);

#endif

// Made with Bob

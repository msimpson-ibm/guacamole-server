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
#include "keymap.h"

#include <string.h>
#include <stdlib.h>

/* Define common keyboard layouts */
static const guac_kubevirt_keymap guac_kubevirt_keymap_en_us = {
    .name = "en-us-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_en_gb = {
    .name = "en-gb-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_de_de = {
    .name = "de-de-qwertz"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_fr_fr = {
    .name = "fr-fr-azerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_es_es = {
    .name = "es-es-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_it_it = {
    .name = "it-it-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_ja_jp = {
    .name = "ja-jp-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_pt_br = {
    .name = "pt-br-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_sv_se = {
    .name = "sv-se-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_da_dk = {
    .name = "da-dk-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_no_no = {
    .name = "no-no-qwerty"
};

static const guac_kubevirt_keymap guac_kubevirt_keymap_fi_fi = {
    .name = "fi-fi-qwerty"
};

/* NULL-terminated array of all keymaps */
static const guac_kubevirt_keymap* guac_kubevirt_keymaps[] = {
    &guac_kubevirt_keymap_en_us,
    &guac_kubevirt_keymap_en_gb,
    &guac_kubevirt_keymap_de_de,
    &guac_kubevirt_keymap_fr_fr,
    &guac_kubevirt_keymap_es_es,
    &guac_kubevirt_keymap_it_it,
    &guac_kubevirt_keymap_ja_jp,
    &guac_kubevirt_keymap_pt_br,
    &guac_kubevirt_keymap_sv_se,
    &guac_kubevirt_keymap_da_dk,
    &guac_kubevirt_keymap_no_no,
    &guac_kubevirt_keymap_fi_fi,
    NULL
};

const guac_kubevirt_keymap* guac_kubevirt_keymap_find(const char* name) {

    /* Return NULL if no name provided */
    if (name == NULL)
        return NULL;

    /* Search through all keymaps */
    const guac_kubevirt_keymap** current = guac_kubevirt_keymaps;
    while (*current != NULL) {
        
        /* Return matching keymap */
        if (strcmp((*current)->name, name) == 0)
            return *current;

        current++;
    }

    /* No such keymap */
    return NULL;

}

// Made with Bob

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

#include "settings.h"

#include <guacamole/mem.h>
#include <guacamole/user.h>

#include <stdlib.h>
#include <string.h>

const char* GUAC_KUBEVIRT_CLIENT_ARGS[] = {
    "hostname",
    "port",
    "namespace",
    "vm-name",
    "token",
    "use-ssl",
    "ignore-cert",
    "ca-cert",
    "read-only",
    "color-depth",
    "remote-cursor",
    "swap-red-blue",
    "lossless",
    "compress-level",
    "quality-level",
    "clipboard-buffer-size",
    "disable-copy",
    "disable-paste",
    "recording-path",
    "recording-name",
    "create-recording-path",
    "recording-exclude-output",
    "recording-exclude-mouse",
    "recording-include-keys",
    "recording-write-existing",
    "retries",
    NULL
};

enum KUBEVIRT_ARGS_IDX {
    IDX_HOSTNAME,
    IDX_PORT,
    IDX_NAMESPACE,
    IDX_VM_NAME,
    IDX_TOKEN,
    IDX_USE_SSL,
    IDX_IGNORE_CERT,
    IDX_CA_CERT,
    IDX_READ_ONLY,
    IDX_COLOR_DEPTH,
    IDX_REMOTE_CURSOR,
    IDX_SWAP_RED_BLUE,
    IDX_LOSSLESS,
    IDX_COMPRESS_LEVEL,
    IDX_QUALITY_LEVEL,
    IDX_CLIPBOARD_BUFFER_SIZE,
    IDX_DISABLE_COPY,
    IDX_DISABLE_PASTE,
    IDX_RECORDING_PATH,
    IDX_RECORDING_NAME,
    IDX_CREATE_RECORDING_PATH,
    IDX_RECORDING_EXCLUDE_OUTPUT,
    IDX_RECORDING_EXCLUDE_MOUSE,
    IDX_RECORDING_INCLUDE_KEYS,
    IDX_RECORDING_WRITE_EXISTING,
    IDX_RETRIES,
    KUBEVIRT_ARGS_COUNT
};

guac_kubevirt_settings* guac_kubevirt_parse_args(guac_user* user,
        int argc, const char** argv) {

    guac_kubevirt_settings* settings = guac_mem_zalloc(sizeof(guac_kubevirt_settings));

    /* Hostname is required */
    settings->hostname =
        guac_user_parse_args_string(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_HOSTNAME, NULL);

    /* Port defaults to 6443 (Kubernetes API default) */
    settings->port =
        guac_user_parse_args_int(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_PORT, 6443);

    /* Namespace is required */
    settings->namespace =
        guac_user_parse_args_string(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_NAMESPACE, NULL);

    /* VM name is required */
    settings->vm_name =
        guac_user_parse_args_string(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_VM_NAME, NULL);

    /* Token is required for authentication */
    settings->token =
        guac_user_parse_args_string(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_TOKEN, NULL);

    /* SSL is enabled by default */
    settings->use_ssl =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_USE_SSL, true);

    /* Certificate validation is enforced by default */
    settings->ignore_cert =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_IGNORE_CERT, false);

    /* CA certificate (optional) */
    settings->ca_cert =
        guac_user_parse_args_string(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_CA_CERT, NULL);

    /* Read-only mode is disabled by default */
    settings->read_only =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_READ_ONLY, false);

    /* Color depth defaults to 24 bits */
    settings->color_depth =
        guac_user_parse_args_int(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_COLOR_DEPTH, 24);

    /* Remote cursor is disabled by default (local cursor) */
    settings->remote_cursor =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_REMOTE_CURSOR, false);

    /* Swap red/blue is disabled by default */
    settings->swap_red_blue =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_SWAP_RED_BLUE, false);

    /* Lossless compression is disabled by default */
    settings->lossless =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_LOSSLESS, false);

    /* Compression level defaults to -1 (server default) */
    settings->compress_level =
        guac_user_parse_args_int(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_COMPRESS_LEVEL, -1);

    /* Quality level defaults to -1 (server default) */
    settings->quality_level =
        guac_user_parse_args_int(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_QUALITY_LEVEL, -1);

    /* Clipboard buffer size defaults to 256KB */
    settings->clipboard_buffer_size =
        guac_user_parse_args_int(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_CLIPBOARD_BUFFER_SIZE, 262144);

    /* Clipboard copy is enabled by default */
    settings->disable_copy =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_DISABLE_COPY, false);

    /* Clipboard paste is enabled by default */
    settings->disable_paste =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_DISABLE_PASTE, false);

    /* Recording path */
    settings->recording_path =
        guac_user_parse_args_string(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_RECORDING_PATH, NULL);

    /* Recording name */
    settings->recording_name =
        guac_user_parse_args_string(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_RECORDING_NAME, GUAC_KUBEVIRT_DEFAULT_RECORDING_NAME);

    /* Create recording path if it doesn't exist */
    settings->create_recording_path =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_CREATE_RECORDING_PATH, false);

    /* Recording exclusions/inclusions */
    settings->recording_exclude_output =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_RECORDING_EXCLUDE_OUTPUT, false);

    settings->recording_exclude_mouse =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_RECORDING_EXCLUDE_MOUSE, false);

    settings->recording_include_keys =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_RECORDING_INCLUDE_KEYS, false);

    /* Recording append mode */
    settings->recording_write_existing =
        guac_user_parse_args_boolean(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_RECORDING_WRITE_EXISTING, false);

    /* Connection retries default to 5 */
    settings->retries =
        guac_user_parse_args_int(user, GUAC_KUBEVIRT_CLIENT_ARGS, argv,
                IDX_RETRIES, 5);

    return settings;
}

void guac_kubevirt_settings_free(guac_kubevirt_settings* settings) {

    guac_mem_free(settings->hostname);
    guac_mem_free(settings->namespace);
    guac_mem_free(settings->vm_name);
    guac_mem_free(settings->token);
    guac_mem_free(settings->ca_cert);
    guac_mem_free(settings->recording_path);
    guac_mem_free(settings->recording_name);

    guac_mem_free(settings);
}

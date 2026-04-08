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

#ifndef GUAC_KUBEVIRT_SETTINGS_H
#define GUAC_KUBEVIRT_SETTINGS_H

#include "keymap.h"

#include <guacamole/user.h>
#include <stdbool.h>

/**
 * The filename to use for the screen recording, if not specified.
 */
#define GUAC_KUBEVIRT_DEFAULT_RECORDING_NAME "recording"

/**
 * KubeVirt-specific client data.
 */
typedef struct guac_kubevirt_settings {

    /**
     * The hostname or IP address of the Kubernetes API server.
     */
    char* hostname;

    /**
     * The port of the Kubernetes API server (typically 443 or 6443).
     */
    int port;

    /**
     * The namespace where the VM is located.
     */
    char* namespace;

    /**
     * The name of the VirtualMachineInstance.
     */
    char* vm_name;

    /**
     * The bearer token for authenticating with the Kubernetes API.
     */
    char* token;

    /**
     * Whether to use TLS/SSL for the connection.
     */
    bool use_ssl;

    /**
     * Whether to ignore TLS/SSL certificate errors.
     */
    bool ignore_cert;

    /**
     * CA certificate to verify the Kubernetes API server (PEM format).
     */
    char* ca_cert;

    /**
     * Whether this connection is read-only, and user input should be dropped.
     */
    bool read_only;

    /**
     * The color depth to request, in bits.
     */
    int color_depth;

    /**
     * Whether the cursor should be rendered on the server (remote) or on the
     * client (local).
     */
    bool remote_cursor;

    /**
     * Whether the red and blue components of each color should be swapped.
     */
    bool swap_red_blue;

    /**
     * Whether all graphical updates for this connection should use lossless
     * compression.
     */
    bool lossless;

    /**
     * The compression level to use for VNC (0-9, -1 for server default).
     */
    int compress_level;

    /**
     * The quality level to use for VNC (0-9, -1 for server default).
     */
    int quality_level;

    /**
     * The maximum number of bytes to allow within the clipboard.
     */
    int clipboard_buffer_size;

    /**
     * Whether outbound clipboard access should be blocked.
     */
    bool disable_copy;

    /**
     * Whether inbound clipboard access should be blocked.
     */
    bool disable_paste;

    /**
     * The path in which the screen recording should be saved, if enabled.
     */
    char* recording_path;

    /**
     * The filename to use for the screen recording, if enabled.
     */
    char* recording_name;

    /**
     * Whether the screen recording path should be automatically created.
     */
    bool create_recording_path;

    /**
     * Whether output should NOT be included in the session recording.
     */
    bool recording_exclude_output;

    /**
     * Whether mouse state should NOT be included in the session recording.
     */
    bool recording_exclude_mouse;

    /**
     * Whether key events should be included in the session recording.
     */
    bool recording_include_keys;

    /**
     * Whether existing files should be appended to when creating a recording.
     */
    bool recording_write_existing;

    /**
     * The number of connection attempts to make before giving up.
     */
    int retries;

    /**
     * The target frame duration in milliseconds. Lower values mean higher
     * refresh rates but more CPU/bandwidth usage. 0 means unlimited (as fast
     * as possible). Default is 0 for maximum responsiveness.
     */
    int frame_duration;

    /**
     * The keymap chosen as the layout of the server. This keymap is used
     * to translate X11 keysyms to the appropriate key events for the VNC
     * server, taking into account the server's keyboard layout.
     */
    const guac_kubevirt_keymap* server_layout;

} guac_kubevirt_settings;

/**
 * Parses all given args, storing them in a newly-allocated settings object.
 *
 * @param user
 *     The user who submitted the given arguments while joining the connection.
 *
 * @param argc
 *     The number of arguments within the argv array.
 *
 * @param argv
 *     The values of all arguments provided by the user.
 *
 * @return
 *     A newly-allocated settings object which must be freed with
 *     guac_kubevirt_settings_free() when no longer needed.
 */
guac_kubevirt_settings* guac_kubevirt_parse_args(guac_user* user,
        int argc, const char** argv);

/**
 * Frees the given guac_kubevirt_settings object.
 *
 * @param settings
 *     The settings object to free.
 */
void guac_kubevirt_settings_free(guac_kubevirt_settings* settings);

/**
 * NULL-terminated array of accepted client args.
 */
extern const char* GUAC_KUBEVIRT_CLIENT_ARGS[];

#endif /* GUAC_KUBEVIRT_SETTINGS_H */

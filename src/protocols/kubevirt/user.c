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

#include "argv.h"
#include "clipboard.h"
#include "input.h"
#include "kubevirt.h"
#include "settings.h"
#include "user.h"

#include <guacamole/argv.h>
#include <guacamole/client.h>
#include <guacamole/recording.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <pthread.h>
#include <string.h>

int guac_kubevirt_user_join_handler(guac_user* user, int argc, char** argv) {

    guac_kubevirt_client* kubevirt_client = (guac_kubevirt_client*) user->client->data;

    /* Parse arguments into client */
    guac_kubevirt_settings* settings = kubevirt_client->settings =
            guac_kubevirt_parse_args(user, argc, (const char**) argv);

    /* Fail if required settings are missing */
    if (settings->hostname == NULL || settings->namespace == NULL ||
        settings->vm_name == NULL || settings->token == NULL) {
        guac_user_log(user, GUAC_LOG_ERROR, "Required connection parameters "
                "are missing. Please provide hostname, namespace, vm-name, "
                "and token.");
        return 1;
    }

    /* Store owner's settings for shared use */
    if (user->owner) {

        /* Set up recording if requested */
        if (settings->recording_path != NULL) {
            kubevirt_client->recording = guac_recording_create(user->client,
                    settings->recording_path,
                    settings->recording_name,
                    settings->create_recording_path,
                    !settings->recording_exclude_output,
                    !settings->recording_exclude_mouse,
                    0, /* Touch events not supported */
                    settings->recording_include_keys,
                    settings->recording_write_existing);
        }

        /* Start client thread */
        if (pthread_create(&kubevirt_client->client_thread, NULL,
                    guac_kubevirt_client_thread, user->client)) {
            guac_user_log(user, GUAC_LOG_ERROR, "Unable to start KubeVirt "
                    "client thread.");
            return 1;
        }
    }

    /* Set up mouse and keyboard handlers only if not read-only */
    if (!settings->read_only) {
        user->mouse_handler = guac_kubevirt_user_mouse_handler;
        user->key_handler = guac_kubevirt_user_key_handler;
    }

    /* Set up clipboard handlers */
    if (!settings->disable_paste)
        user->clipboard_handler = guac_kubevirt_clipboard_receive_handler;

    /**
     * Update connection parameters if we own the connection.
     *
     * Note that the argv handler is called *regardless* of whether
     * or not the connection is read-only, as this allows authentication
     * to be prompted and processed even if the owner cannot send
     * input to the remote session. In the future, if other argv handling
     * is added to the KubeVirt protocol, checks may need to be done within
     * the argv handler to verify that read-only connections remain
     * read-only.
     *
     * Also, this is only handled for the owner - if the argv handler
     * is expanded to include non-owner users in the future, special
     * care will need to be taken to make sure that the arguments
     * processed by the handler do not have unintended security
     * implications for non-owner users.
     */
    if (user->owner)
        user->argv_handler = guac_argv_handler;

    return 0;
}

int guac_kubevirt_user_leave_handler(guac_user* user) {

    guac_kubevirt_client* kubevirt_client = (guac_kubevirt_client*) user->client->data;

    if (kubevirt_client->display)
        guac_display_notify_user_left(kubevirt_client->display, user);

    return 0;
}

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

#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

void guac_kubevirt_client_log_info(const char* format, ...) {

    char message[2048];

    /* Copy log message into buffer */
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* Log to syslog */
    syslog(LOG_INFO, "%s", message);

}

void guac_kubevirt_client_log_error(const char* format, ...) {

    char message[2048];

    /* Copy log message into buffer */
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* Log to syslog */
    syslog(LOG_ERR, "%s", message);

}

// Made with Bob

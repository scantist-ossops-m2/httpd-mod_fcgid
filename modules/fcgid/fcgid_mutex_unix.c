/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include "apr_global_mutex.h"
#include "httpd.h"
#include "ap_mmn.h"
#include "http_log.h"
#include "fcgid_mutex.h"

#if AP_MODULE_MAGIC_AT_LEAST(20091119,1)

#include "util_mutex.h"

apr_status_t fcgid_mutex_register(const char *mutex_type,
                                  apr_pool_t *pconf)
{
    return ap_mutex_register(pconf, mutex_type, NULL, APR_LOCK_DEFAULT, 0);
}

apr_status_t fcgid_mutex_create(apr_global_mutex_t **mutex,
                                const char **lockfile,
                                const char *mutex_type,
                                apr_pool_t *pconf,
                                server_rec *main_server)
{
    apr_status_t rv;

    *lockfile = NULL;
    rv = ap_global_mutex_create(mutex, mutex_type, NULL, main_server,
                                pconf, 0);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    *lockfile = apr_global_mutex_lockfile(*mutex);
    return APR_SUCCESS;
}

#else

#if AP_NEED_SET_MUTEX_PERMS
#include "unixd.h"
#endif

#if MODULE_MAGIC_NUMBER_MAJOR < 20081201
#define ap_unixd_set_global_mutex_perms unixd_set_global_mutex_perms
#endif

apr_status_t fcgid_mutex_register(const char *mutex_type,
                                  apr_pool_t *pconf)
{
    return APR_SUCCESS;
}

apr_status_t fcgid_mutex_create(apr_global_mutex_t **mutex,
                                const char **lockfilep,
                                const char *mutex_type,
                                apr_pool_t *pconf,
                                server_rec *s)
{
    apr_status_t rv;
    apr_lockmech_e mechanism = APR_LOCK_DEFAULT;
    char *lockfile;

    lockfile = apr_palloc(pconf, L_tmpnam);
    tmpnam(lockfile);
    rv = apr_global_mutex_create(mutex, lockfile, mechanism, pconf);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, rv, s,
                     "mod_fcgid: Can't create global %s mutex", mutex_type);
        return rv;
    }

#ifdef AP_NEED_SET_MUTEX_PERMS
    rv = ap_unixd_set_global_mutex_perms(*mutex);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, rv, s,
                     "mod_fcgid: Can't set global %s mutex perms", mutex_type);
        return rv;
    }
#endif

    *lockfilep = lockfile;
    return APR_SUCCESS;
}

#endif

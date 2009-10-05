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

#include "ap_config.h"
#include "ap_mmn.h"
#include "apr_strings.h"
#include "apr_hash.h"
#include "apr_lib.h"
#include "apr_tables.h"
#include "apr_version.h"
#include "http_main.h"
#include "httpd.h"
#include "http_config.h"
#include "fcgid_global.h"
#include "fcgid_conf.h"

#define DEFAULT_IDLE_TIMEOUT 300
#define DEFAULT_IDLE_SCAN_INTERVAL 120
#define DEFAULT_BUSY_TIMEOUT 300
#define DEFAULT_BUSY_SCAN_INTERVAL 120
#define DEFAULT_ERROR_SCAN_INTERVAL 3
#define DEFAULT_ZOMBIE_SCAN_INTERVAL 3
#define DEFAULT_PROC_LIFETIME (60*60)
#define DEFAULT_SOCKET_PREFIX "logs/fcgidsock"
#define DEFAULT_SHM_PATH "logs/fcgid_shm"
#define DEFAULT_SPAWNSOCRE_UPLIMIT 10
#define DEFAULT_SPAWN_SCORE 1
#define DEFAULT_TERMINATION_SCORE 2
#define DEFAULT_TIME_SCORE 1
#define DEFAULT_MAX_PROCESS_COUNT 1000
#define DEFAULT_MAX_CLASS_PROCESS_COUNT 100
#define DEFAULT_MIN_CLASS_PROCESS_COUNT 3
#define DEFAULT_IPC_CONNECT_TIMEOUT 3
#define DEFAULT_IPC_COMM_TIMEOUT 40
#define DEFAULT_OUTPUT_BUFFERSIZE 65536
#define DEFAULT_MAX_REQUESTS_PER_PROCESS 0
#define DEFAULT_MAX_REQUEST_LEN (1024*1024*1024)    /* 1G */
#define DEFAULT_MAX_MEM_REQUEST_LEN (1024*64)   /* 64k */
#define DEFAULT_WRAPPER_KEY "ALL"
#define WRAPPER_FLAG_VIRTUAL "virtual"

void *create_fcgid_server_config(apr_pool_t * p, server_rec * s)
{
    fcgid_server_conf *config = apr_pcalloc(p, sizeof(*config));

    if (!s->is_virtual) {
        config->busy_scan_interval = DEFAULT_BUSY_SCAN_INTERVAL;
        config->error_scan_interval = DEFAULT_ERROR_SCAN_INTERVAL;
        config->idle_scan_interval = DEFAULT_IDLE_SCAN_INTERVAL;
        config->max_process_count = DEFAULT_MAX_PROCESS_COUNT;
        config->shmname_path = ap_server_root_relative(p, DEFAULT_SHM_PATH);
        config->sockname_prefix =
          ap_server_root_relative(p, DEFAULT_SOCKET_PREFIX);
        config->spawn_score = DEFAULT_SPAWN_SCORE;
        config->spawnscore_uplimit = DEFAULT_SPAWNSOCRE_UPLIMIT;
        config->termination_score = DEFAULT_TERMINATION_SCORE;
        config->time_score = DEFAULT_TIME_SCORE;
        config->zombie_scan_interval = DEFAULT_ZOMBIE_SCAN_INTERVAL;
    }
    /* Redundant; pcalloc creates this structure;
     * config->default_init_env = NULL;
     * config->pass_headers = NULL;
     * config->php_fix_pathinfo_enable = 0;
     * config->*_set = 0;
     */
    config->cmdopts_hash = apr_hash_make(p);
    config->ipc_comm_timeout = DEFAULT_IPC_COMM_TIMEOUT;
    config->ipc_connect_timeout = DEFAULT_IPC_CONNECT_TIMEOUT;
    config->max_mem_request_len = DEFAULT_MAX_MEM_REQUEST_LEN;
    config->max_request_len = DEFAULT_MAX_REQUEST_LEN;
    config->max_requests_per_process = DEFAULT_MAX_REQUESTS_PER_PROCESS;
    config->output_buffersize = DEFAULT_OUTPUT_BUFFERSIZE;
    config->max_class_process_count = DEFAULT_MAX_CLASS_PROCESS_COUNT;
    config->min_class_process_count = DEFAULT_MIN_CLASS_PROCESS_COUNT;
    config->busy_timeout = DEFAULT_BUSY_TIMEOUT;
    config->idle_timeout = DEFAULT_IDLE_TIMEOUT;
    config->proc_lifetime = DEFAULT_PROC_LIFETIME;

    return config;
}

#define MERGE_SCALAR(base, local, merged, field) \
  if (!(local)->field##_set) { \
      merged->field = base->field; \
  }

void *merge_fcgid_server_config(apr_pool_t * p, void *basev, void *locv)
{
    fcgid_server_conf *base = (fcgid_server_conf *) basev;
    fcgid_server_conf *local = (fcgid_server_conf *) locv;
    fcgid_server_conf *merged =
      (fcgid_server_conf *) apr_pmemdup(p, local, sizeof(fcgid_server_conf));

    merged->cmdopts_hash = apr_hash_overlay(p, local->cmdopts_hash,
                                            base->cmdopts_hash);

    /* Merge environment variables */
    if (base->default_init_env == NULL) {
        /* merged already set to local */
    }
    else if (local->default_init_env == NULL) {
        merged->default_init_env = base->default_init_env;
    }
    else {
        merged->default_init_env = 
          apr_table_copy(p, base->default_init_env);
        apr_table_overlap(merged->default_init_env, 
                          local->default_init_env,
                          APR_OVERLAP_TABLES_SET);
    }

    /* Merge pass headers */
    if (base->pass_headers == NULL) {
        /* merged already set to local */
    }
    else if (local->pass_headers == NULL) {
        merged->pass_headers = base->pass_headers;
    }
    else {
        merged->pass_headers = 
          apr_array_append(p, 
                           base->pass_headers,
                           local->pass_headers);
    }

    /* Merge the scalar settings */

    MERGE_SCALAR(base, local, merged, ipc_comm_timeout);
    MERGE_SCALAR(base, local, merged, ipc_connect_timeout);
    MERGE_SCALAR(base, local, merged, max_mem_request_len);
    MERGE_SCALAR(base, local, merged, max_request_len);
    MERGE_SCALAR(base, local, merged, max_requests_per_process);
    MERGE_SCALAR(base, local, merged, output_buffersize);
    MERGE_SCALAR(base, local, merged, max_class_process_count);
    MERGE_SCALAR(base, local, merged, min_class_process_count);
    MERGE_SCALAR(base, local, merged, busy_timeout);
    MERGE_SCALAR(base, local, merged, idle_timeout);
    MERGE_SCALAR(base, local, merged, proc_lifetime);

    return merged;
}

void *create_fcgid_dir_config(apr_pool_t * p, char *dummy)
{
    fcgid_dir_conf *config = apr_pcalloc(p, sizeof(fcgid_dir_conf));

    config->wrapper_info_hash = apr_hash_make(p);
    /* config->authenticator_info = NULL; */
    config->authenticator_authoritative = 1;
    /* config->authorizer_info = NULL; */
    config->authorizer_authoritative = 1;
    /* config->access_info = NULL; */
    config->access_authoritative = 1;
    return (void *) config;
}

void *merge_fcgid_dir_config(apr_pool_t *p, void *basev, void *locv)
{
    fcgid_dir_conf *base = (fcgid_dir_conf *) basev;
    fcgid_dir_conf *local = (fcgid_dir_conf *) locv;
    fcgid_dir_conf *merged =
      (fcgid_dir_conf *) apr_pmemdup(p, local, sizeof(fcgid_dir_conf));

    merged->wrapper_info_hash =
      apr_hash_overlay(p, local->wrapper_info_hash,
                       base->wrapper_info_hash);

    if (!local->authenticator_info) {
        merged->authenticator_info = base->authenticator_info;
    }

    if (!local->authorizer_info) {
        merged->authorizer_info = base->authorizer_info;
    }

    if (!local->access_info) {
        merged->access_info = base->access_info;
    }

    MERGE_SCALAR(base, local, merged, authenticator_authoritative);
    MERGE_SCALAR(base, local, merged, authorizer_authoritative);
    MERGE_SCALAR(base, local, merged, access_authoritative);

    return merged;
}

const char *set_idle_timeout(cmd_parms * cmd, void *dummy, const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);

    config->idle_timeout = atol(arg);
    config->idle_timeout_set = 1;
    return NULL;
}

const char *set_idle_scan_interval(cmd_parms * cmd, void *dummy,
                                   const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->idle_scan_interval = atol(arg);
    return NULL;
}

const char *set_busy_timeout(cmd_parms * cmd, void *dummy, const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);

    config->busy_timeout = atol(arg);
    config->busy_timeout_set = 1;
    return NULL;
}

const char *set_busy_scan_interval(cmd_parms * cmd, void *dummy,
                                   const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->busy_scan_interval = atol(arg);
    return NULL;
}

const char *set_proc_lifetime(cmd_parms * cmd, void *dummy,
                              const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);

    config->proc_lifetime = atol(arg);
    config->proc_lifetime_set = 1;
    return NULL;
}

const char *set_error_scan_interval(cmd_parms * cmd, void *dummy,
                                    const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->error_scan_interval = atol(arg);
    return NULL;
}

const char *set_zombie_scan_interval(cmd_parms * cmd, void *dummy,
                                     const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->zombie_scan_interval = atol(arg);
    return NULL;
}

const char *set_socketpath(cmd_parms * cmd, void *dummy, const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->sockname_prefix = ap_server_root_relative(cmd->pool, arg);
    if (!config->sockname_prefix)
        return "Invalid socket path";

    return NULL;
}

const char *set_shmpath(cmd_parms * cmd, void *dummy, const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->shmname_path = ap_server_root_relative(cmd->pool, arg);
    if (!config->shmname_path)
        return "Invalid shmname path";

    return NULL;
}

const char *set_spawnscore_uplimit(cmd_parms * cmd, void *dummy,
                                   const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->spawnscore_uplimit = atol(arg);
    return NULL;
}

static int strtoff(apr_off_t *val, const char *arg)
{
    char *errp;

#if APR_MAJOR_VERSION < 1
    *val = (apr_off_t)strtol(arg, &errp, 10);
    if (*errp) {
        return 1;
    }
#else
    if (APR_SUCCESS != apr_strtoff(val, arg, &errp, 10) || *errp) {
        return 1;
    }
#endif
    return 0;
}

const char *set_max_request_len(cmd_parms * cmd, void *dummy,
                                const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);

    if (strtoff(&config->max_request_len, arg)
        || config->max_request_len < 0) {
        return "FCGIDMaxRequestLen requires a non-negative integer.";
    }

    config->max_request_len_set = 1;
    return NULL;
}

const char *set_max_mem_request_len(cmd_parms * cmd, void *dummy,
                                    const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    config->max_mem_request_len = atol(arg);
    config->max_mem_request_len_set = 1;
    return NULL;
}

const char *set_spawn_score(cmd_parms * cmd, void *dummy, const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->spawn_score = atol(arg);
    return NULL;
}

const char *set_time_score(cmd_parms * cmd, void *dummy, const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->time_score = atol(arg);
    return NULL;
}

const char *set_termination_score(cmd_parms * cmd, void *dummy,
                                  const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->termination_score = atol(arg);
    return NULL;
}

const char *set_max_process(cmd_parms * cmd, void *dummy, const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->max_process_count = atol(arg);
    return NULL;
}

const char *set_output_buffersize(cmd_parms * cmd, void *dummy,
                                  const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    config->output_buffersize = atol(arg);
    config->output_buffersize_set = 1;
    return NULL;
}

const char *set_max_class_process(cmd_parms * cmd, void *dummy,
                                  const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);

    config->max_class_process_count = atol(arg);
    config->max_class_process_count_set = 1;
    return NULL;
}

const char *set_min_class_process(cmd_parms * cmd, void *dummy,
                                  const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);

    config->min_class_process_count = atol(arg);
    config->min_class_process_count_set = 1;
    return NULL;
}

const char *set_php_fix_pathinfo_enable(cmd_parms * cmd, void *dummy,
                                        const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    config->php_fix_pathinfo_enable = atol(arg);
    return NULL;
}

const char *set_max_requests_per_process(cmd_parms * cmd, void *dummy,
                                         const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    if ((config->max_requests_per_process = atol(arg)) == -1) {
        config->max_requests_per_process = 0;
    }
    config->max_requests_per_process_set = 1;
    return NULL;
}

const char *set_ipc_connect_timeout(cmd_parms * cmd, void *dummy,
                                    const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    config->ipc_connect_timeout = atol(arg);
    config->ipc_connect_timeout_set = 1;
    return NULL;
}

const char *set_ipc_comm_timeout(cmd_parms * cmd, void *dummy,
                                 const char *arg)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *config =
        ap_get_module_config(s->module_config, &fcgid_module);
    config->ipc_comm_timeout = atol(arg);
    if (config->ipc_comm_timeout <= 0) {
        return "IPCCommTimeout must be greater than 0";
    }
    config->ipc_comm_timeout_set = 1;
    return NULL;
}

static void add_envvar_to_table(apr_table_t *t, apr_pool_t *p,
                                const char *name, const char *value)
{
#if defined(WIN32) || defined(OS2) || defined(NETWARE)
    /* Case insensitive environment platforms */
    char *pstr;
    for (name = pstr = apr_pstrdup(p, name); *pstr; ++pstr) {
        *pstr = apr_toupper(*pstr);
    }
#endif
    apr_table_set(t, name, value ? value : "");
}
                                
const char *add_default_env_vars(cmd_parms * cmd, void *dummy,
                                 const char *name, const char *value)
{
    fcgid_server_conf *config =
        ap_get_module_config(cmd->server->module_config, &fcgid_module);
    if (config->default_init_env == NULL)
        config->default_init_env = apr_table_make(cmd->pool, 20);

    add_envvar_to_table(config->default_init_env, cmd->pool, name, value);
    return NULL;
}

const char *add_pass_headers(cmd_parms * cmd, void *dummy,
                             const char *names)
{
    const char **header;
    fcgid_server_conf *config =
        ap_get_module_config(cmd->server->module_config, &fcgid_module);
    if (config->pass_headers == NULL)
        config->pass_headers =
            apr_array_make(cmd->pool, 10, sizeof(const char *));

    header = (const char **) apr_array_push(config->pass_headers);
    *header = ap_getword_conf(cmd->pool, &names);

    return header ? NULL : "Invalid PassHeaders";
}

apr_array_header_t *get_pass_headers(request_rec * r)
{
    fcgid_server_conf *config =
        ap_get_module_config(r->server->module_config, &fcgid_module);
    return config->pass_headers;
}

const char *set_authenticator_info(cmd_parms * cmd, void *config,
                                   const char *authenticator)
{
    apr_status_t rv;
    apr_finfo_t finfo;
    fcgid_dir_conf *dirconfig = (fcgid_dir_conf *) config;

    /* Is the wrapper exist? */
    if ((rv = apr_stat(&finfo, authenticator, APR_FINFO_NORM,
                       cmd->temp_pool)) != APR_SUCCESS) {
        return apr_psprintf(cmd->pool,
                            "can't get authenticator file info: %s, errno: %d",
                            authenticator, apr_get_os_error());
    }

    /* Create the wrapper node */
    dirconfig->authenticator_info =
        apr_pcalloc(cmd->server->process->pconf,
                    sizeof(*dirconfig->authenticator_info));
    apr_cpystrn(dirconfig->authenticator_info->path, authenticator,
                _POSIX_PATH_MAX);
    dirconfig->authenticator_info->inode = finfo.inode;
    dirconfig->authenticator_info->deviceid = finfo.device;
    dirconfig->authenticator_info->share_group_id = (apr_size_t) - 1;
    return NULL;
}

const char *set_authenticator_authoritative(cmd_parms * cmd,
                                            void *config, int arg)
{
    fcgid_dir_conf *dirconfig = (fcgid_dir_conf *) config;

    dirconfig->authenticator_authoritative = arg;
    dirconfig->authenticator_authoritative_set = 1;
    return NULL;
}

auth_conf *get_authenticator_info(request_rec * r, int *authoritative)
{
    fcgid_dir_conf *config =
        ap_get_module_config(r->per_dir_config, &fcgid_module);

    if (config != NULL && config->authenticator_info != NULL) {
        *authoritative = config->authenticator_authoritative;
        return config->authenticator_info;
    }

    return NULL;
}

const char *set_authorizer_info(cmd_parms * cmd, void *config,
                                const char *authorizer)
{
    apr_status_t rv;
    apr_finfo_t finfo;
    fcgid_dir_conf *dirconfig = (fcgid_dir_conf *) config;

    /* Is the wrapper exist? */
    if ((rv = apr_stat(&finfo, authorizer, APR_FINFO_NORM,
                       cmd->temp_pool)) != APR_SUCCESS) {
        return apr_psprintf(cmd->pool,
                            "can't get authorizer file info: %s, errno: %d",
                            authorizer, apr_get_os_error());
    }

    /* Create the wrapper node */
    dirconfig->authorizer_info =
        apr_pcalloc(cmd->server->process->pconf,
                    sizeof(*dirconfig->authorizer_info));
    apr_cpystrn(dirconfig->authorizer_info->path, authorizer,
                _POSIX_PATH_MAX);
    dirconfig->authorizer_info->inode = finfo.inode;
    dirconfig->authorizer_info->deviceid = finfo.device;
    dirconfig->authorizer_info->share_group_id = (apr_size_t) - 1;
    return NULL;
}

const char *set_authorizer_authoritative(cmd_parms * cmd,
                                         void *config, int arg)
{
    fcgid_dir_conf *dirconfig = (fcgid_dir_conf *) config;

    dirconfig->authorizer_authoritative = arg;
    dirconfig->authorizer_authoritative_set = 1;
    return NULL;
}

auth_conf *get_authorizer_info(request_rec * r, int *authoritative)
{
    fcgid_dir_conf *config =
        ap_get_module_config(r->per_dir_config, &fcgid_module);

    if (config != NULL && config->authorizer_info != NULL) {
        *authoritative = config->authorizer_authoritative;
        return config->authorizer_info;
    }

    return NULL;
}

const char *set_access_info(cmd_parms * cmd, void *config,
                            const char *access)
{
    apr_status_t rv;
    apr_finfo_t finfo;
    fcgid_dir_conf *dirconfig = (fcgid_dir_conf *) config;

    /* Is the wrapper exist? */
    if ((rv = apr_stat(&finfo, access, APR_FINFO_NORM,
                       cmd->temp_pool)) != APR_SUCCESS) {
        return apr_psprintf(cmd->pool,
                            "can't get access checker file info: %s, errno: %d",
                            access, apr_get_os_error());
    }

    /* Create the wrapper node */
    dirconfig->access_info =
        apr_pcalloc(cmd->server->process->pconf,
                    sizeof(*dirconfig->access_info));
    apr_cpystrn(dirconfig->access_info->path, access, _POSIX_PATH_MAX);
    dirconfig->access_info->inode = finfo.inode;
    dirconfig->access_info->deviceid = finfo.device;
    dirconfig->access_info->share_group_id = (apr_size_t) - 1;
    return NULL;
}

const char *set_access_authoritative(cmd_parms * cmd,
                                     void *config, int arg)
{
    fcgid_dir_conf *dirconfig = (fcgid_dir_conf *) config;

    dirconfig->access_authoritative = arg;
    dirconfig->access_authoritative_set = 1;
    return NULL;
}

auth_conf *get_access_info(request_rec * r, int *authoritative)
{
    fcgid_dir_conf *config =
        ap_get_module_config(r->per_dir_config, &fcgid_module);

    if (config != NULL && config->access_info != NULL) {
        *authoritative = config->access_authoritative;
        return config->access_info;
    }

    return NULL;
}

typedef struct {
    apr_hash_t *wrapper_id_hash;
    apr_size_t cur_id;
} wrapper_id_info;

const char *set_wrapper_config(cmd_parms * cmd, void *dirconfig,
                               const char *wrapperpath,
                               const char *extension,
                               const char *virtual)
{
    const char *path, *tmp;
    apr_status_t rv;
    apr_finfo_t finfo;
    const char *userdata_key = "fcgid_wrapper_id";
    wrapper_id_info *id_info;
    apr_size_t *wrapper_id;
    fcgid_wrapper_conf *wrapper = NULL;
    fcgid_dir_conf *config = (fcgid_dir_conf *) dirconfig;

    /* Sanity checks */

    if (wrapperpath == NULL)
        return "Invalid wrapper file";

    if (virtual == NULL && extension != NULL && !strcasecmp(extension, WRAPPER_FLAG_VIRTUAL)) {
        virtual = WRAPPER_FLAG_VIRTUAL;
        extension = NULL;
    }

    if (virtual != NULL && strcasecmp(virtual, WRAPPER_FLAG_VIRTUAL)) {
        return "Invalid wrapper flag";
    }

    if (extension != NULL
        && (*extension != '.' || *(extension + 1) == '\0'
            || ap_strchr_c(extension, '/') || ap_strchr_c(extension, '\\')))
        return "Invalid wrapper file extension";

    /* Get wrapper_id hash from user data */
    {
        void *id_info_vp;
        apr_pool_userdata_get(&id_info_vp, userdata_key,
                              cmd->server->process->pool);
        id_info = id_info_vp;
    }

    if (!id_info) {
        id_info =
            apr_pcalloc(cmd->server->process->pool, sizeof(*id_info));
        id_info->wrapper_id_hash =
            apr_hash_make(cmd->server->process->pool);
        apr_pool_userdata_set((const void *) id_info, userdata_key,
                              apr_pool_cleanup_null,
                              cmd->server->process->pool);
    }
    /* Get wrapper_id for wrapperpath */
    if ((wrapper_id =
         apr_hash_get(id_info->wrapper_id_hash, wrapperpath,
                      strlen(wrapperpath))) == NULL) {
        wrapper_id =
            apr_pcalloc(cmd->server->process->pool, sizeof(*wrapper_id));
        *wrapper_id = id_info->cur_id++;
        apr_hash_set(id_info->wrapper_id_hash, wrapperpath,
                     strlen(wrapperpath), wrapper_id);
    }

    wrapper = apr_pcalloc(cmd->server->process->pconf, sizeof(*wrapper));

    /* Get wrapper path */
    tmp = wrapperpath;
    path = ap_getword_white(cmd->temp_pool, &tmp);
    if (path == NULL || *path == '\0')
        return "Invalid wrapper config";

    /* Does the wrapper exist? */
    if ((rv = apr_stat(&finfo, path, APR_FINFO_NORM,
                       cmd->temp_pool)) != APR_SUCCESS) {
        return apr_psprintf(cmd->pool,
                            "can't get FastCGI file info: '%s' (%s), errno: %d",
                            wrapperpath, path, apr_get_os_error());
    }

    apr_cpystrn(wrapper->args, wrapperpath, _POSIX_PATH_MAX);
    wrapper->inode = finfo.inode;
    wrapper->deviceid = finfo.device;
    wrapper->share_group_id = *wrapper_id;
    wrapper->virtual = (virtual != NULL && !strcasecmp(virtual, WRAPPER_FLAG_VIRTUAL));
    (*wrapper_id)++;

    if (extension == NULL)
        extension = DEFAULT_WRAPPER_KEY;

    /* Add the node now */
    /* If an extension is configured multiple times, the last directive wins. */
    apr_hash_set(config->wrapper_info_hash, extension, strlen(extension),
                 wrapper);

    return NULL;
}

fcgid_wrapper_conf *get_wrapper_info(const char *cgipath, request_rec * r)
{
    const char *extension;
    fcgid_wrapper_conf *wrapper;
    fcgid_dir_conf *config =
        ap_get_module_config(r->per_dir_config, &fcgid_module);

    /* Get file name extension */
    extension = ap_strrchr_c(cgipath, '.');

    if (extension == NULL)
        extension = DEFAULT_WRAPPER_KEY;

    /* Search file name extension in per_dir_config */
    if (config) {
        wrapper = apr_hash_get(config->wrapper_info_hash, extension,
                               strlen(extension));
        if (wrapper == NULL)
            wrapper = apr_hash_get(config->wrapper_info_hash, DEFAULT_WRAPPER_KEY,
                                   strlen(DEFAULT_WRAPPER_KEY));
        return wrapper;
    }

    return NULL;
}

static int set_cmd_envvars(fcgid_cmd_options *cmdopts, apr_table_t *envvars)
{
    const apr_array_header_t *envvars_arr;
    const apr_table_entry_t *envvars_entry;
    int i;
    int overflow = 0;

    if (envvars) {
        envvars_arr = apr_table_elts(envvars);
        envvars_entry = (apr_table_entry_t *) envvars_arr->elts;
        if (envvars_arr->nelts > INITENV_CNT) {
            overflow = envvars_arr->nelts - INITENV_CNT;
        }

        for (i = 0; i < envvars_arr->nelts && i < INITENV_CNT; ++i) {
            if (envvars_entry[i].key == NULL
                || envvars_entry[i].key[0] == '\0')
                break;
            apr_cpystrn(cmdopts->initenv_key[i], envvars_entry[i].key,
                        INITENV_KEY_LEN);
            apr_cpystrn(cmdopts->initenv_val[i], envvars_entry[i].val,
                        INITENV_VAL_LEN);
        }
        if (i < INITENV_CNT) {
            cmdopts->initenv_key[i][0] = '\0';
        }
    }
    else {
        cmdopts->initenv_key[0][0] = '\0';
    }

    return overflow;
}

const char *set_cmd_options(cmd_parms *cmd, void *dummy, const char *args)
{
    server_rec *s = cmd->server;
    fcgid_server_conf *sconf =
        ap_get_module_config(s->module_config, &fcgid_module);
    const char *cmdname;
    fcgid_cmd_options *cmdopts;
    apr_table_t *envvars = NULL;
    int overflow;
    apr_finfo_t finfo;
    apr_status_t rv;

    cmdopts = apr_pcalloc(cmd->pool, sizeof *cmdopts);

    cmdopts->busy_timeout = DEFAULT_BUSY_TIMEOUT;
    cmdopts->idle_timeout = DEFAULT_IDLE_TIMEOUT;
    cmdopts->ipc_comm_timeout = DEFAULT_IPC_COMM_TIMEOUT;
    cmdopts->ipc_connect_timeout = DEFAULT_IPC_CONNECT_TIMEOUT;
    cmdopts->max_class_process_count = DEFAULT_MAX_CLASS_PROCESS_COUNT;
    cmdopts->max_requests_per_process = DEFAULT_MAX_REQUESTS_PER_PROCESS;
    cmdopts->min_class_process_count = DEFAULT_MIN_CLASS_PROCESS_COUNT;
    cmdopts->proc_lifetime = DEFAULT_PROC_LIFETIME;
    /* via pcalloc: cmdopts->initenv_key[0][0] = '\0'; */
    
    cmdname = ap_getword_conf(cmd->pool, &args);
    if (!strlen(cmdname)) {
        return "A command must be specified for FCGIDCmdOptions";
    }

    rv = apr_stat(&finfo, cmdname, APR_FINFO_NORM, cmd->temp_pool);
    if (rv != APR_SUCCESS) {
        return apr_psprintf(cmd->pool,
                            "File %s does not exist or cannot be accessed (error %d)",
                            cmdname, rv);
    }

    if (!*args) {
        return "At least one option must be specified for FCGIDCmdOptions";
    }

    while (*args) {
        const char *option = ap_getword_white(cmd->pool, &args);
        const char *val;

        /* don't support BusyTimeout until BZ #47483 is fixed
         * (The kludge in bucket_ctx_cleanup wouldn't have addressibility
         * to the cmd-specific busy timeout setting.)
         */

        if (!strcasecmp(option, "ConnectTimeout")) {
            val = ap_getword_white(cmd->pool, &args);
            if (!strlen(val)) {
                return "ConnectTimeout must have an argument";
            }
            cmdopts->ipc_connect_timeout = atoi(val);
            continue;
        }
            
        if (!strcasecmp(option, "IdleTimeout")) {
            val = ap_getword_white(cmd->pool, &args);
            if (!strlen(val)) {
                return "IdleTimeout must have an argument";
            }
            cmdopts->idle_timeout = atoi(val);
            continue;
        }

        if (!strcasecmp(option, "InitialEnv")) {
            char *name;
            char *eql;

            name = ap_getword_white(cmd->pool, &args);
            if (!strlen(name)) {
                return "InitialEnv must have an argument";
            }

            eql = strchr(name, '=');
            if (eql) {
                *eql = '\0';
                ++eql;
            }

            if (!envvars) {
                envvars = apr_table_make(cmd->pool, 20);
            }
            add_envvar_to_table(envvars, cmd->pool, name, eql);
            continue;
        }

        if (!strcasecmp(option, "IOTimeout")) {
            val = ap_getword_white(cmd->pool, &args);
            if (!strlen(val)) {
                return "IOTimeout must have an argument";
            }
            cmdopts->ipc_comm_timeout = atoi(val);
            continue;
        }
            
        if (!strcasecmp(option, "MaxProcesses")) {
            val = ap_getword_white(cmd->pool, &args);
            if (!strlen(val)) {
                return "MaxProcesses must have an argument";
            }
            cmdopts->max_class_process_count = atoi(val);
            continue;
        }

        if (!strcasecmp(option, "MaxProcessLifetime")) {
            val = ap_getword_white(cmd->pool, &args);
            if (!strlen(val)) {
                return "MaxProcessLifetime must have an argument";
            }
            cmdopts->proc_lifetime = atoi(val);
            continue;
        }

        if (!strcasecmp(option, "MaxRequestsPerProcess")) {
            val = ap_getword_white(cmd->pool, &args);
            if (!strlen(val)) {
                return "MaxRequestsPerProcess must have an argument";
            }
            cmdopts->max_requests_per_process = atoi(val);
            continue;
        }

        if (!strcasecmp(option, "MinProcesses")) {
            val = ap_getword_white(cmd->pool, &args);
            if (!strlen(val)) {
                return "MinProcesses must have an argument";
            }
            cmdopts->min_class_process_count = atoi(val);
            continue;
        }

        return apr_psprintf(cmd->pool,
                            "Invalid option for FCGIDCommandOptions: %s",
                            option);
    }

    if ((overflow = set_cmd_envvars(cmdopts, envvars)) != 0) {
        return apr_psprintf(cmd->pool, "mod_fcgid: environment variable table "
                            "overflow; increase INITENV_CNT in fcgid_pm.h from"
                            " %d to at least %d",
                            INITENV_CNT, INITENV_CNT + overflow);
    }

    apr_hash_set(sconf->cmdopts_hash, cmdname, strlen(cmdname), cmdopts);

    return NULL;
}

void get_cmd_options(request_rec *r, const char *cmdpath,
                     fcgid_cmd_options *cmdopts)
{
    fcgid_server_conf *sconf =
        ap_get_module_config(r->server->module_config, &fcgid_module);
    fcgid_cmd_options *cmd_specific = apr_hash_get(sconf->cmdopts_hash,
                                                   cmdpath,
                                                   strlen(cmdpath));
    int overflow;

    if (cmd_specific) { /* ignore request context configuration */
        *cmdopts = *cmd_specific;
        /* pick up configuration for values that can't be configured
         * on FCGIDCmdOptions
         */
        cmdopts->busy_timeout = sconf->busy_timeout;
        return;
    }

    cmdopts->busy_timeout = sconf->busy_timeout;
    cmdopts->idle_timeout = sconf->idle_timeout;
    cmdopts->ipc_comm_timeout = sconf->ipc_comm_timeout;
    cmdopts->ipc_connect_timeout = sconf->ipc_connect_timeout;
    cmdopts->max_class_process_count = sconf->max_class_process_count;
    cmdopts->max_requests_per_process = sconf->max_requests_per_process;
    cmdopts->min_class_process_count = sconf->min_class_process_count;
    cmdopts->proc_lifetime = sconf->proc_lifetime;

    if ((overflow = set_cmd_envvars(cmdopts, sconf->default_init_env)) != 0) {
        ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r,
                      "mod_fcgid: %d environment variables dropped; increase "
                      "INITENV_CNT in fcgid_pm.h from %d to at least %d",
                      overflow,
                      INITENV_CNT,
                      INITENV_CNT + overflow);
    }
}

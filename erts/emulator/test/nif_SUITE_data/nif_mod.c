/*
 * %CopyrightBegin%
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright Ericsson AB 2009-2025. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * %CopyrightEnd%
 */
#include <erl_nif.h>
#include <string.h>
#include <stdio.h>

#include "nif_mod.h"

#if ERL_NIF_MAJOR_VERSION*100 + ERL_NIF_MINOR_VERSION >= 215
# define HAVE_ENIF_MONITOR_PROCESS
#endif
#if ERL_NIF_MAJOR_VERSION*100 + ERL_NIF_MINOR_VERSION >= 216
# define HAVE_ENIF_DYNAMIC_RESOURCE_CALL
#endif

#define CHECK(X) ((void)((X) || (check_abort(__LINE__),1)))
#ifdef __GNUC__
static void check_abort(unsigned line) __attribute__((noreturn));
#endif
static void check_abort(unsigned line)
{
    enif_fprintf(stderr, "Test CHECK failed at %s:%u\r\n",
	    __FILE__, line);
    abort();
}

static int static_cntA; /* zero by default */
static int static_cntB = NIF_LIB_VER * 100;

static ERL_NIF_TERM am_true;
static ERL_NIF_TERM am_null;
static ERL_NIF_TERM am_resource_type;
static ERL_NIF_TERM am_resource_dtor_A;
static ERL_NIF_TERM am_resource_dtor_B;
static ERL_NIF_TERM am_resource_down_D;
static ERL_NIF_TERM am_resource_dyncall;
static ERL_NIF_TERM am_return;
static ERL_NIF_TERM am_unloaded;
static ERL_NIF_TERM am_tester;

static NifModPrivData* priv_data(ErlNifEnv* env)
{
    return (NifModPrivData*) enif_priv_data(env);
}

static void init(ErlNifEnv* env)
{
    am_true = enif_make_atom(env, "true");
    am_null = enif_make_atom(env, "null");
    am_resource_type = enif_make_atom(env, "resource_type");
    am_resource_dtor_A = enif_make_atom(env, "resource_dtor_A");
    am_resource_dtor_B = enif_make_atom(env, "resource_dtor_B");
    am_resource_down_D = enif_make_atom(env, "resource_down_D");
    am_resource_dyncall = enif_make_atom(env, "resource_dyncall");
    am_return = enif_make_atom(env, "return");
    am_unloaded = enif_make_atom(env, "unloaded");
    am_tester = enif_make_atom(env, "tester");
}

static void add_call_with_arg(ErlNifEnv* env, NifModPrivData* data, const char* func_name,
			      const char* arg, int arg_sz)
{
    CallInfo* call = (CallInfo*)enif_alloc(sizeof(CallInfo)+strlen(func_name) + arg_sz);
    strcpy(call->func_name, func_name);
    call->lib_ver = NIF_LIB_VER;
    call->static_cntA = ++static_cntA;
    call->static_cntB = ++static_cntB;
    call->arg_sz = arg_sz;
    if (arg != NULL) {
	call->arg = call->func_name + strlen(func_name) + 1;
	memcpy(call->arg, arg, arg_sz);
    }
    else {
	call->arg = NULL;
    }
    enif_mutex_lock(data->mtx);
    call->next = data->call_history;
    data->call_history = call;
    enif_mutex_unlock(data->mtx);
}

static void add_call(ErlNifEnv* env, NifModPrivData* data,const char* func_name)
{
    add_call_with_arg(env, data, func_name, NULL, 0);
}

#define ADD_CALL(FUNC_NAME) add_call(env, priv_data(env),FUNC_NAME)

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)

static void resource_dtor_A(ErlNifEnv* env, void* a)
{
    const char dtor_name[] = "resource_dtor_A_v"  STRINGIFY(NIF_LIB_VER); 

    add_call_with_arg(env, priv_data(env), dtor_name, (const char*)a,
		      enif_sizeof_resource(a));
}

static void resource_dtor_B(ErlNifEnv* env, void* a)
{
    const char dtor_name[] = "resource_dtor_B_v"  STRINGIFY(NIF_LIB_VER);

    add_call_with_arg(env, priv_data(env), dtor_name, (const char*)a,
		      enif_sizeof_resource(a));
}

#ifdef HAVE_ENIF_MONITOR_PROCESS
static void resource_down_D(ErlNifEnv* env, void* a, ErlNifPid* pid, ErlNifMonitor* mon)
{
    const char down_name[] = "resource_down_D_v"  STRINGIFY(NIF_LIB_VER);

    add_call_with_arg(env, priv_data(env), down_name, (const char*)a,
		      enif_sizeof_resource(a));
}
#endif

#ifdef HAVE_ENIF_DYNAMIC_RESOURCE_CALL
static void resource_dyncall(ErlNifEnv* env, void* obj, void* call_data)
{
    int* p = (int*)call_data;
    *p += NIF_LIB_VER;
}
#endif



/* {resource_type,
    Ix|null,
    ErlNifResourceFlags in,
    "TypeName",
    dtor(A|B|null),
    ErlNifResourceFlags out
    [, down(D|null)]}
*/
static void open_resource_type(ErlNifEnv* env, int arity, const ERL_NIF_TERM* arr)
{
    NifModPrivData* data = priv_data(env);
    char rt_name[30];
    union { ErlNifResourceFlags e; int i; } flags, exp_res, got_res;
    unsigned ix;
    ErlNifResourceDtor* dtor;
    ErlNifResourceType* got_ptr;

    CHECK(enif_is_identical(arr[0], am_resource_type));
    CHECK(enif_get_int(env, arr[2], &flags.i));
    CHECK(enif_get_string(env, arr[3], rt_name, sizeof(rt_name), ERL_NIF_LATIN1) > 0);
    CHECK(enif_get_int(env, arr[5], &exp_res.i));
	
    if (enif_is_identical(arr[4], am_null)) {
	dtor = NULL;
    }
    else if (enif_is_identical(arr[4], am_resource_dtor_A)) {
	dtor = resource_dtor_A;
    }
    else {
	CHECK(enif_is_identical(arr[4], am_resource_dtor_B));
	dtor = resource_dtor_B;
    }
#ifdef HAVE_ENIF_MONITOR_PROCESS
    if (arity == 7) {
        ErlNifResourceTypeInit init;
        init.dtor = dtor;
        init.stop = NULL;
	init.down = NULL;

#  ifdef HAVE_ENIF_DYNAMIC_RESOURCE_CALL
	init.members = 0xdead;
	init.dyncall = (ErlNifResourceDynCall*) 0xdeadbeaf;

	if (enif_is_identical(arr[6], am_resource_dyncall)) {
            init.dyncall = resource_dyncall;
	    init.members = 4;
	    got_ptr = enif_init_resource_type(env, rt_name, &init,
					      flags.e, &got_res.e);
        }
	else
#  endif
	{
	    if (enif_is_identical(arr[6], am_resource_down_D)) {
		init.down = resource_down_D;
	    }
	    else {
		CHECK(enif_is_identical(arr[6], am_null));
	    }
	    got_ptr = enif_open_resource_type_x(env, rt_name, &init,
						flags.e, &got_res.e);

	}
    }
    else
#endif
    {
        got_ptr = enif_open_resource_type(env, NULL, rt_name, dtor,
                                          flags.e, &got_res.e);
    }
    if (enif_get_uint(env, arr[1], &ix) && ix < RT_MAX && got_ptr != NULL) {
	data->rt_arr[ix] = got_ptr;
    }
    else {
	CHECK(enif_is_identical(arr[1], am_null));
	CHECK(got_ptr == NULL);
    }
    CHECK(got_res.e == exp_res.e);
}

static void do_load_info(ErlNifEnv* env, ERL_NIF_TERM load_info, int* retvalp)
{
    NifModPrivData* data = priv_data(env);
    ERL_NIF_TERM head, tail;
    unsigned ix;

    for (ix=0; ix<RT_MAX; ix++) {
	data->rt_arr[ix] = NULL;
    }
    for (head = load_info; enif_get_list_cell(env, head, &head, &tail);
	  head = tail) {
	const ERL_NIF_TERM* arr;
	int arity;

	CHECK(enif_get_tuple(env, head, &arity, &arr));
	switch (arity) {
	case 6:
        case 7:
	    open_resource_type(env, arity, arr);
	    break;
	case 2:
	    if (arr[0] == am_return)
                CHECK(enif_get_int(env, arr[1], retvalp));
	    else if (arr[0] == am_tester)
                CHECK(enif_get_local_pid(env, arr[1], &data->tester_pid));
            else
                CHECK(0);
	    break;
	default:
	    CHECK(0);
	}
    }    
    CHECK(enif_is_empty_list(env, head));
}

#if NIF_LIB_VER != 3
static int load(ErlNifEnv* env, void** priv, ERL_NIF_TERM load_info)
{
    NifModPrivData* data;
    int retval = 0;

    init(env);
    data = (NifModPrivData*) enif_alloc(sizeof(NifModPrivData));
    CHECK(data != NULL);
    *priv = data;
    data->mtx = enif_mutex_create("nif_mod_priv_data"); 
    data->ref_cnt = 1;
    data->call_history = NULL;
    enif_self(env, &data->tester_pid);

    add_call(env, data, "load");

    do_load_info(env, load_info, &retval);
    if (retval)
	NifModPrivData_release(data);
    return retval;
}

static int reload(ErlNifEnv* env, void** priv, ERL_NIF_TERM load_info)
{
    NifModPrivData* data = (NifModPrivData*) *priv;
    int retval = 0;
    init(env);
    add_call(env, data, "reload");
    
    do_load_info(env, load_info, &retval);
    return retval;
}

static int upgrade(ErlNifEnv* env, void** priv, void** old_priv_data, ERL_NIF_TERM load_info)
{
    NifModPrivData* data = (NifModPrivData*) *old_priv_data;
    int retval = 0;
    init(env);
    add_call(env, data, "upgrade");
    data->ref_cnt++;

    *priv = *old_priv_data;    
    do_load_info(env, load_info, &retval);
    if (retval)
	NifModPrivData_release(data);
    return retval;
}

static void unload(ErlNifEnv* env, void* priv)
{
    NifModPrivData* data = (NifModPrivData*) priv;

    add_call(env, data, "unload");
    enif_send(env, &data->tester_pid, NULL, am_unloaded);
    NifModPrivData_release(data);
}
#endif /*  NIF_LIB_VER != 3 */

static ERL_NIF_TERM lib_version(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ADD_CALL("lib_version");
    return enif_make_int(env, NIF_LIB_VER);
}

static ERL_NIF_TERM nif_api_version(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    /*ADD_CALL("nif_api_version");*/
    return enif_make_tuple2(env,
			    enif_make_int(env, ERL_NIF_MAJOR_VERSION),
			    enif_make_int(env, ERL_NIF_MINOR_VERSION));
}

static ERL_NIF_TERM get_priv_data_ptr(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    NifModPrivData** bin_data;
    ERL_NIF_TERM res;
    ADD_CALL("get_priv_data_ptr");
    bin_data = (NifModPrivData**)enif_make_new_binary(env, sizeof(void*), &res);
    *bin_data = priv_data(env);
    return res;
}

static ERL_NIF_TERM make_new_resource(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    NifModPrivData* data = priv_data(env);
    ErlNifBinary ibin;
    char* a;
    ERL_NIF_TERM ret;
    unsigned ix;
    if (!enif_get_uint(env, argv[0], &ix) || ix >= RT_MAX 
	|| !enif_inspect_binary(env, argv[1], &ibin)) {
	return enif_make_badarg(env);
    }
    a = (char*) enif_alloc_resource(data->rt_arr[ix], ibin.size);
    memcpy(a, ibin.data, ibin.size);
    ret = enif_make_resource(env, a);
    enif_release_resource(a);
    return ret;
}

static ERL_NIF_TERM get_resource(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    NifModPrivData* data = priv_data(env);
    ErlNifBinary obin;
    unsigned ix;
    void* a;
    if (!enif_get_uint(env, argv[0], &ix) || ix >= RT_MAX 
	|| !enif_get_resource(env, argv[1], data->rt_arr[ix], &a)
	|| !enif_alloc_binary(enif_sizeof_resource(a), &obin)) { 
	return enif_make_badarg(env);
    }
    memcpy(obin.data, a, obin.size);
    return enif_make_binary(env, &obin);
}

#ifdef HAVE_ENIF_MONITOR_PROCESS
static ERL_NIF_TERM monitor_process(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    NifModPrivData* data = priv_data(env);
    ErlNifPid pid;
    unsigned ix;
    void* obj;
    int ret;

    if (!enif_get_uint(env, argv[0], &ix) || ix >= RT_MAX
	|| !enif_get_resource(env, argv[1], data->rt_arr[ix], &obj)
        || !enif_get_local_pid(env, argv[2], &pid)) {
	return enif_make_badarg(env);
    }
    ret = enif_monitor_process(env, obj, &pid, NULL);
    return enif_make_int(env, ret);
}
#endif

static ERL_NIF_TERM trace_me(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ADD_CALL("lib_version");
    return enif_make_int(env, NIF_LIB_VER);
}

static ErlNifFunc nif_funcs[] =
{
    {"lib_version", 0, lib_version},
    {"nif_api_version", 0, nif_api_version},
    {"get_priv_data_ptr", 0, get_priv_data_ptr},
    {"make_new_resource", 2, make_new_resource},
    {"get_resource", 2, get_resource},
#ifdef HAVE_ENIF_MONITOR_PROCESS
    {"monitor_process", 3, monitor_process},
#endif
#if NIF_LIB_VER == 4
    {"non_existing", 2, get_resource},      /* error: not found */
#endif
#if NIF_LIB_VER == 5
    {"make_new_resource", 2, get_resource}, /* error: duplicate */
#endif
    {"trace_me", 1, trace_me},

    /* Keep lib_version_check last to maximize the loading "patch distance"
       between it and lib_version */
    {"lib_version_check", 0, lib_version}
};

#if NIF_LIB_VER != 3
ERL_NIF_INIT(nif_mod,nif_funcs,load,reload,upgrade,unload)
#else
ERL_NIF_INIT_GLOB /* avoid link error on windows */
#endif


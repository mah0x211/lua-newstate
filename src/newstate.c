/**
 *  Copyright (C) 2021 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <string.h>

#define MODULE_MT "newstate"

typedef struct {
    lua_State *L;
    int ref_fn;
} newstate_t;

#if (LUA_VERSION_NUM > 501)
#    define tbllen(L, idx) lua_rawlen(L, idx)
#else
#    define tbllen(L, idx) lua_objlen(L, idx)
#endif

static inline int loadfile(lua_State *L, const char *s, size_t len,
                           const char *name) {
    (void)len;
    (void)name;
    return luaL_loadfile(L, s);
}

static inline int loadstring(lua_State *L, const char *s, size_t len,
                             const char *name) {
    return luaL_loadbuffer(L, s, len, name);
}

static inline int moveit(lua_State *src, lua_State *dst, int idx, int eoi) {
    size_t len;

    while (idx <= eoi) {
        const int t = lua_type(src, idx);

        switch (t) {
        case LUA_TNIL:
            lua_pushnil(dst);
            break;

        case LUA_TBOOLEAN:
            lua_pushboolean(dst, lua_toboolean(src, idx));
            break;

        case LUA_TLIGHTUSERDATA:
            lua_pushlightuserdata(dst, lua_touserdata(src, idx));
            break;

        case LUA_TNUMBER:
            lua_pushnumber(dst, lua_tonumber(src, idx));
            break;

        case LUA_TSTRING:
            lua_pushlstring(dst, lua_tolstring(src, idx, &len), len);
            break;

        case LUA_TTABLE:
            lua_createtable(dst, tbllen(src, idx), 0);
            lua_pushnil(src);
            idx -= idx < 0;
            while (lua_next(src, idx) != 0) {
                if (moveit(src, dst, -2, -1) != 0) {
                    return 2;
                }
                lua_rawset(dst, -3);
                lua_pop(src, 1);
            }
            idx += idx < 0;
            break;

        // case LUA_TFUNCTION:
        // case LUA_TUSERDATA:
        // case LUA_TTHREAD:
        default:
            lua_pushboolean(src, 0);
            lua_pushfstring(src, "cannot exchange <%s> value",
                            lua_typename(src, t));
            lua_pushinteger(src, -1);
            return 3;
        }
        idx++;
    }

    return 0;
}

static inline int runit(lua_State *src, lua_State *dst) {
    int rc = lua_pcall(dst, lua_gettop(dst) - 1, LUA_MULTRET, 0);
    int nres = 0;

    if (rc) {
        size_t len = 0;
        lua_pushboolean(src, 0);
        lua_pushlstring(src, luaL_checklstring(dst, 1, &len), len);
        lua_pushinteger(src, rc);
        return 3;
    }

    lua_pushboolean(src, 1);
    nres = lua_gettop(dst);
    if (nres) {
        rc = moveit(dst, src, 1, nres);
        lua_settop(dst, 0);
        if (rc) {
            return rc;
        }
    }

    return 1 + nres;
}

static int run_lua(lua_State *L) {
    newstate_t *state = luaL_checkudata(L, 1, MODULE_MT);
    int rc = 0;

    lua_settop(state->L, 0);
    lua_rawgeti(state->L, LUA_REGISTRYINDEX, state->ref_fn);
    if ((rc = moveit(L, state->L, 2, lua_gettop(L)))) {
        lua_settop(state->L, 0);
        return rc;
    }

    return runit(L, state->L);
}

typedef int (*loadfn)(lua_State *L, const char *src, size_t len,
                      const char *name);

static inline int loadit(lua_State *src, lua_State *dst, int idx, loadfn fn) {
    size_t len = 0;
    const char *s = luaL_checklstring(src, idx, &len);
    int rc = 0;

    lua_settop(dst, 0);
    if ((rc = fn(dst, s, len, s))) {
        lua_pushboolean(src, 0);
        lua_pushlstring(src, luaL_checklstring(dst, 1, &len), len);
        lua_pushinteger(src, rc);
        return 3;
    }

    return 0;
}

static inline int load_lua(lua_State *L, loadfn fn) {
    newstate_t *state = luaL_checkudata(L, 1, MODULE_MT);
    int rc = loadit(L, state->L, 2, fn);

    if (rc) {
        lua_settop(state->L, 0);
        return rc;
    }
    // unref old func
    luaL_unref(state->L, LUA_REGISTRYINDEX, state->ref_fn);
    // maintain new func
    state->ref_fn = luaL_ref(state->L, LUA_REGISTRYINDEX);

    lua_pushboolean(L, 1);
    return 1;
}

static int loadstring_lua(lua_State *L) {
    return load_lua(L, loadstring);
}

static int loadfile_lua(lua_State *L) {
    return load_lua(L, loadfile);
}

static inline int do_lua(lua_State *L, loadfn fn) {
    newstate_t *state = luaL_checkudata(L, 1, MODULE_MT);
    int rc = 0;

    if ((rc = loadit(L, state->L, 2, fn)) ||
        (rc = moveit(L, state->L, 3, lua_gettop(L)))) {
        lua_settop(state->L, 0);
        return rc;
    }

    return runit(L, state->L);
}

static int dostring_lua(lua_State *L) {
    return do_lua(L, loadstring);
}

static int dofile_lua(lua_State *L) {
    return do_lua(L, loadfile);
}

static int gc_lua(lua_State *L) {
    newstate_t *state = luaL_checkudata(L, 1, MODULE_MT);
    int what = (int)luaL_checkinteger(L, 2);
    int arg = (int)luaL_optinteger(L, 3, 0);

    switch (what) {

#if defined(LUA_GCISRUNNING)
    case LUA_GCISRUNNING:
        lua_pushboolean(L, lua_gc(state->L, what, 0));
        return 1;
#endif

#if defined(LUA_GCINC)
    case LUA_GCINC: {
        int pause = arg;
        int stepmul = (int)luaL_optinteger(L, 4, 0);
        int stepsize = (int)luaL_optinteger(L, 5, 0);
        lua_pushinteger(L, lua_gc(state->L, pause, stepmul, stepsize));
        return 1;
    }

#endif

#if defined(LUA_GCGEN)
    case LUA_GCGEN: {
        int minormul = arg;
        int majormul = (int)luaL_optinteger(L, 4, 0);
        lua_pushinteger(L, lua_gc(state->L, minormul, majormul));
        return 1;
    }
#endif

    default:
        lua_pushinteger(L, lua_gc(state->L, what, arg));
        return 1;
    }
}

static int new_lua(lua_State *L) {
    int openlibs = 1;
    newstate_t *state = NULL;

    switch (lua_type(L, 1)) {
    case LUA_TNONE:
    case LUA_TNIL:
        break;
    default:
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        openlibs = lua_toboolean(L, 1);
    }

    state = lua_newuserdata(L, sizeof(newstate_t));
    state->L = luaL_newstate();
    if (state->L) {
        if (openlibs) {
            luaL_openlibs(state->L);
        }
        state->ref_fn = LUA_NOREF;
        luaL_getmetatable(L, MODULE_MT);
        lua_setmetatable(L, -2);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int gc__lua(lua_State *L) {
    newstate_t *state = (newstate_t *)lua_touserdata(L, 1);
    lua_close(state->L);
    return 0;
}

static int tostring_lua(lua_State *L) {
    lua_pushfstring(L, MODULE_MT ": %p", lua_touserdata(L, 1));
    return 1;
}

static void newmetatable_lua(lua_State *L) {
    struct luaL_Reg mmethods[] = {
        {"__gc", gc__lua}, {"__tostring", tostring_lua}, {NULL, NULL}};
    struct luaL_Reg methods[] = {{"dofile", dofile_lua},
                                 {"dostring", dostring_lua},
                                 {"loadfile", loadfile_lua},
                                 {"loadstring", loadstring_lua},
                                 {"run", run_lua},
                                 {"gc", gc_lua},
                                 {NULL, NULL}};
    struct luaL_Reg *fn = mmethods;

    // create metatable
    luaL_newmetatable(L, MODULE_MT);
    // add metamethods
    while (fn->name) {
        lua_pushstring(L, fn->name);
        lua_pushcfunction(L, fn->func);
        lua_rawset(L, -3);
        fn++;
    }
    // add methods
    lua_pushstring(L, "__index");
    lua_newtable(L);
    fn = methods;
    while (fn->name) {
        lua_pushstring(L, fn->name);
        lua_pushcfunction(L, fn->func);
        lua_rawset(L, -3);
        fn++;
    }
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

LUALIB_API int luaopen_newstate(lua_State *L) {
    struct luaL_Reg fns[] = {{"new", new_lua}, {NULL, NULL}};
    struct luaL_Reg *fn = fns;

    // create metatable
    newmetatable_lua(L);
    // create func table
    lua_newtable(L);
    while (fn->name) {
        lua_pushstring(L, fn->name);
        lua_pushcfunction(L, fn->func);
        lua_rawset(L, -3);
        fn++;
    }

    // export constants
    // GEN_ERRCODE_DECL
    // GEN_ERRCODE_DECL_END

    // export gc constants
    // GEN_GCWHAT_DECL
    // GEN_GCWHAT_DECL_END

    return 1;
}

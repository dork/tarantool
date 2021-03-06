/*
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "lua/session.h"
#include "lua/init.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "fiber.h"
#include "session.h"
#include "sio.h"

static const char *sessionlib_name = "box.session";

/**
 * Return a unique monotonic session
 * identifier. The identifier can be used
 * to check whether or not a session is alive.
 * 0 means there is no session (e.g.
 * a procedure is running in a detached
 * fiber).
 */
static int
lbox_session_id(struct lua_State *L)
{
	lua_pushnumber(L, fiber->sid);
	return 1;
}

/**
 * Check whether or not a session exists.
 */
static int
lbox_session_exists(struct lua_State *L)
{
	if (lua_gettop(L) != 1)
		luaL_error(L, "session.exists(sid): bad arguments");

	uint32_t sid = luaL_checkint(L, -1);
	lua_pushnumber(L, session_exists(sid));
	return 1;
}

/**
 * Pretty print peer name.
 */
static int
lbox_session_peer(struct lua_State *L)
{
	if (lua_gettop(L) > 1)
		luaL_error(L, "session.peer(sid): bad arguments");

	uint32_t sid = lua_gettop(L) == 1 ?
		luaL_checkint(L, -1) : fiber->sid;

	int fd = session_fd(sid);
	struct sockaddr_in addr;
	sio_getpeername(fd, &addr);

	lua_pushstring(L, sio_strfaddr(&addr));
	return 1;
}

struct lbox_session_trigger
{
	struct session_trigger *trigger;
	int coro_ref;
};

static struct lbox_session_trigger on_connect =
	{ &session_on_connect, LUA_NOREF};
static struct lbox_session_trigger on_disconnect =
	{ &session_on_disconnect, LUA_NOREF};

static void
lbox_session_run_trigger(void *param)
{
	struct lbox_session_trigger *trigger = param;
	/* Copy the referenced callable object object stack. */
	lua_rawgeti(tarantool_L, LUA_REGISTRYINDEX, trigger->coro_ref);

	struct lua_State *coro_L = lua_tothread(tarantool_L, -1);
	lua_pop(tarantool_L, 1);
	/*
	 * If there was junk from previous invocation of the
	 * trigger, clear it, only leaving on the stack the chunk
	 * to be run. The stack may be polluted when previous
	 * invocation ended up with an error.
	 */
	lua_settop(coro_L, 1);
	/* lua_call pops the function, make a copy */
	lua_pushvalue(coro_L, -1);
	@try {
		lua_call(coro_L, 0, 0);
	} @catch (tnt_Exception *e) {
		@throw;
	} @catch (...) {
		tnt_raise(ClientError, :ER_PROC_LUA,
			  lua_tostring(coro_L, -1));
	}
}

static int
lbox_session_set_trigger(struct lua_State *L,
			 struct lbox_session_trigger *trigger)
{
	if (lua_gettop(L) != 1 ||
	    (lua_type(L, -1) != LUA_TFUNCTION &&
	     lua_type(L, -1) != LUA_TNIL)) {
		luaL_error(L, "session.on_connect(chunk): bad arguments");
	}

	struct lua_State *coro_L;

	if (trigger->coro_ref != LUA_NOREF) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, trigger->coro_ref);
		coro_L = lua_tothread(L, -1);
		lua_pop(L, 1); /* pop the coro. */
		/* If there was junk from previous invocation, clear it. */
		lua_settop(coro_L, 1);
	} else {
		coro_L = lua_newthread(L);
		/* Reference the new thread and pop it. */
		trigger->coro_ref = luaL_ref(L, LUA_REGISTRYINDEX);
		lua_pushnil(coro_L);
	}
	/** Move the chunk to the coroutine in will be run in. */
	lua_xmove(L, coro_L, 1);
	/* Return the old chunk. */
	lua_insert(coro_L, -2);
	lua_xmove(coro_L, L, 1);
	/*
	 * Set or clear the trigger. Return the old value of the
	 * trigger.
	 */
	if (lua_type(coro_L, -1) == LUA_TNIL) {
		trigger->trigger->trigger = NULL;
		trigger->trigger->param = NULL;
	} else {
		trigger->trigger->trigger = lbox_session_run_trigger;
		trigger->trigger->param = trigger;
	}
	return 1;
}

static int
lbox_session_on_connect(struct lua_State *L)
{
	return lbox_session_set_trigger(L, &on_connect);
}

static int
lbox_session_on_disconnect(struct lua_State *L)
{
	return lbox_session_set_trigger(L, &on_disconnect);
}

static const struct luaL_reg lbox_session_meta [] = {
	{"id", lbox_session_id},
	{NULL, NULL}
};

static const struct luaL_reg sessionlib[] = {
	{"id", lbox_session_id},
	{"exists", lbox_session_exists},
	{"peer", lbox_session_peer},
	{"on_connect", lbox_session_on_connect},
	{"on_disconnect", lbox_session_on_disconnect},
	{NULL, NULL}
};

void
tarantool_lua_session_init(struct lua_State *L)
{
	luaL_register(L, sessionlib_name, sessionlib);
	lua_pop(L, 1);
}

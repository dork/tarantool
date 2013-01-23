#include "lua/remote.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../connector/c/include/tp.h"


static char *
buf_resizer(struct tp *p, size_t req, size_t *size)
{
	luaL_Buffer *b = p->obj;
	size_t new_size = req + tp_size(p);
	luaL_addsize(b, new_size);
	*size = new_size;
	return luaL_prepbuffer(b);
}

static void
buf_push_tuple(struct tp *req, struct lua_State *L, int idx)
{
	tp_tuple(req);
	/* TODO: tuple object */
	lua_pushnil(L);
	while(lua_next(L, idx) != 0) {
		size_t fl;
		const char *fd = lua_tolstring(L, -1, &fl);
		tp_field(req, fd, fl);
		lua_pop(L, 1);
	}
}


/**
 * box.remote.protocol.insert(id, space, flags, tuple)
 */
static int
lbox_tp_insert(struct lua_State *L)
{
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	struct tp req;

	tp_init(&req, "", 0, buf_resizer, &b);
	tp_insert(&req, lua_tointeger(L, 2), lua_tointeger(L, 3));
	tp_reqid(&req, lua_tointeger(L, 1));
	buf_push_tuple(&req, L, 4);
	luaL_pushresult(&b);
	return 1;
}


/**
 * box.remote.protocol.ping(id)
 */
static int
lbox_tp_ping(struct lua_State *L)
{
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	struct tp req;

	tp_init(&req, "", 0, buf_resizer, &b);
	tp_ping(&req);
	tp_reqid(&req, lua_tointeger(L, 1));
	luaL_pushresult(&b);
	return 1;
}

static void
tarantool_lua_remote_protocol_init(struct lua_State *L)
{
	static const struct luaL_reg pmeta [] =
	{
		{"ping",	lbox_tp_ping},
		{"insert",	lbox_tp_insert},
		{NULL, NULL}
	};

	lua_pushstring(L, "protocol");
	lua_newtable(L);
	luaL_register(L, NULL, pmeta); /* table for __index */

	lua_settable(L, -3);

}


void
tarantool_lua_remote_init(struct lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "box");

	lua_pushstring(L, "remote");
	lua_newtable(L);		/* box.remote table */
	tarantool_lua_remote_protocol_init(L);
	lua_settable(L, -3);
	lua_pop(L, 1);
}

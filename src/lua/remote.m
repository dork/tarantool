#include "fiber.h"
#include "lua/remote.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../connector/c/include/tp.h"

#define CHUNK_SIZE	1024

static char *
buf_resizer(struct tp *p, size_t req, size_t *size)
{

	size_t new_size = tp_size(p) + CHUNK_SIZE;
	if (tp_used(p) + req > new_size) {
		new_size = ((tp_used(p) + req) / CHUNK_SIZE + 1) * CHUNK_SIZE;
	}
	char *new_buf = palloc(fiber->gc_pool, new_size);
	if (!new_buf)
		luaL_error(p->obj, "Can't allocate memory for request");
	if (tp_used(p) > 0) {
		memcpy(new_buf, p->s, tp_used(p));
	}
	*size = new_size;
	return new_buf;
}

static void
tp_push_lua_tuple(struct tp *req, struct lua_State *L, int idx)
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
int
lbox_tp_insert(struct lua_State *L)
{
	struct tp req;
	tp_init(&req, "", 0, buf_resizer, L);
	tp_insert(&req, lua_tointeger(L, 2), lua_tointeger(L, 3));
	tp_reqid(&req, lua_tointeger(L, 1));
	tp_push_lua_tuple(&req, L, 4);
	lua_pushlstring(L, req.s, tp_used(&req));
	return 1;
}


/**
 * box.remote.protocol.ping(id)
 */
static int
lbox_tp_ping(struct lua_State *L)
{
	struct tp req;
	tp_init(&req, "", 0, buf_resizer, L);
	tp_ping(&req);
	tp_reqid(&req, lua_tointeger(L, 1));
	lua_pushlstring(L, req.s, tp_used(&req));
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

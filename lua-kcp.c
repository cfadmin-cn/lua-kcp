#define LUA_LIB

#include <core.h>
#include "ikcp.h"

typedef struct LUA_KCP{
  int fd;
  ikcpcb *ctx;
  lua_State *reader, *sender;
}LUA_KCP;

// static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
//   return 1;
// }

static int lcreate(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)lua_newuserdata(L, sizeof(struct LUA_KCP));
  if (!lua_kcp)
    return 0;
  lua_kcp->ctx = ikcp_create(luaL_checkinteger(L, 1), lua_kcp);
  lua_kcp->sender = lua_tothread(L, 2);
  lua_kcp->reader = lua_tothread(L, 3);
  lua_kcp->fd = -1;
  luaL_setmetatable(L, "__KCP__");
  return 1;
}

static int lrelease(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp)
    return 0;
  // 清除上下文资源
  if (lua_kcp->ctx) {
    ikcp_release(lua_kcp->ctx);
    lua_kcp->ctx = NULL;
  }
  // 清除fd资源
  if (lua_kcp->fd >= 0) {
    close(lua_kcp->fd);
    lua_kcp->fd = -1;
  }
  // 去除引用
  lua_kcp->sender = lua_kcp->reader = NULL;
  return 1;
}

static int lsetwnd(lua_State *L) {
  return 1;
}

static int lsetmode(lua_State *L) {
  return 1;
}

static int lsetmtu(lua_State *L) {
  return 1;
}

static int lsend(lua_State *L) {
  return 1;
}

static int lrecv(lua_State *L) {
  return 1;
}


LUAMOD_API int luaopen_tcp(lua_State *L){
  luaL_checkversion(L);
  // `hook`内存分配函数
  ikcp_allocator(xmalloc, xfree);
  // 创建`KCP`元对象;
  luaL_newmetatable(L, "__KCP__");
  lua_pushstring (L, "__index");
  lua_pushvalue(L, -2);
  lua_rawset(L, -3);
  lua_pushstring (L, "__gc");
  lua_pushcfunction(L, lrelease);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__mode");
  lua_pushliteral(L, "kv");
  lua_rawset(L, -3);
  luaL_Reg kcp_libs[] = {
    {"new", lcreate},
    {"send", lsend},
    {"recv", lrecv},
    {"setwnd", lsetwnd},
    {"setmtu", lsetmtu},
    {"setmode", lsetmode},
    {"release", lrelease},
    {NULL, NULL},
  };
  luaL_setfuncs(L, kcp_libs, 0);
  luaL_newlib(L, kcp_libs);
  return 1;
}
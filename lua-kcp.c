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
  return 1;
}

static int lrelease(lua_State *L) {
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
  luaL_newmetatable(L, "__TCP__");
  lua_pushstring (L, "__index");
  lua_pushvalue(L, -2);
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
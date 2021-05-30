#define LUA_LIB

#include <core.h>
#include "ikcp.h"

#define CLIENT (0)
#define SERVER (1)

typedef struct LUA_KCP{
  int fd;
  ikcpcb *ctx;
  lua_State *reader, *sender;
}LUA_KCP;

// static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
//   return 1;
// }

static inline void SETSOCKETOPT(int sockfd) {
	int Enable = 1;
	int ret = 0;
	/* 设置非阻塞 */
	non_blocking(sockfd);

/* 地址重用 */
#ifdef SO_REUSEADDR
  ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &Enable, sizeof(Enable));
  if (ret < 0) {
    LOG("ERROR", "Setting SO_REUSEADDR failed.");
    LOG("ERROR", strerror(errno));
    return core_exit();
  }
#endif

/* 端口重用 */
#ifdef SO_REUSEPORT
  ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &Enable, sizeof(Enable));
  if (ret < 0) {
    LOG("ERROR", "Setting SO_REUSEPORT failed.");
    LOG("ERROR", strerror(errno));
    return core_exit();
  }
#endif

/* 开启IPV6与ipv4双栈 */
#ifdef IPV6_V6ONLY
  int No = 0;
  ret = setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&No, sizeof(No));
  if (ret < 0){
    LOG("ERROR", "Setting IPV6_V6ONLY failed.");
    LOG("ERROR", strerror(errno));
    return core_exit();
  }
#endif

}

static inline int SETSOCKETMODE(int usocket, const char *ip, int port, int mode) {

  errno = 0;

  struct sockaddr_in6 SA;
  memset(&SA, 0x0, sizeof(SA));

  SA.sin6_family = AF_INET6;
  SA.sin6_port = htons(port);
  SA.sin6_addr = in6addr_any;

  struct in6_addr addr;
  /* 如果填写的是`::1` */
  if (!strcmp(ip, "::1") && inet_pton(AF_INET6, "::1", &addr) == 1){
    SA.sin6_addr = addr;
  /* 如果填写的是`127.0.0.1` */
  } else if (!strcmp(ip, "127.0.0.1") && inet_pton(AF_INET6, "::ffff:127.0.0.1", &addr) == 1) {
    SA.sin6_addr = addr;
  /* 如果填写的是其它`IPv6`地址 */
  } else if (inet_pton(AF_INET6, ip, &addr) == 1) {
    SA.sin6_addr = addr;
  /* 如果填写的是 `0.0.0.0` 则监听所有地址 */
  } else if (!strcmp(ip, "0.0.0.0")) {
    SA.sin6_addr = in6addr_any;
  /* 检查IPv4地址或者是非法IP地址 */
  } else {
    struct in_addr addr4;
    if (inet_pton(AF_INET, ip, &addr4) == 1) {
      char *ipv6 = alloca(strlen(ip) + 8);
      memset(ipv6, 0x0, strlen(ip) + 8);
      memmove(ipv6, "::ffff:", 7);
      memmove(ipv6 + 7, ip, strlen(ip));
      if (inet_pton(AF_INET6, ipv6, &addr) != 1){
        close(usocket);
        return -1;
      }
      SA.sin6_addr = addr;
    }
  }

  int ret;
  // 区分`客户端模式`还是`服务器模式`
  if (mode == CLIENT) {
    ret = connect(usocket, (const struct sockaddr*)&SA, (socklen_t)sizeof(SA));
  } else if (mode == SERVER) {
    ret = bind(usocket, (const struct sockaddr*)&SA, (socklen_t)sizeof(SA));
  } else {
    close(usocket);
    return -1;
  }
  if (ret < 0) {
    close(usocket);
    LOG("ERROR", strerror(errno));
    return -1;
  }
  return 0;
}

// 创建`KCP`对应的UDP套接字
static inline int NEWSOCKET(void) {

  errno = 0;

  int usocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (usocket < 0) {
    LOG("ERROR", strerror(errno));
    return -1;
  }

  SETSOCKETOPT(usocket);

  return usocket;
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

static int lconnect(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp)
    return 0;

  // 创建`udp`文件描述符
  if ((lua_kcp->fd = NEWSOCKET()) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  // 绑定`udp`文件描述符到指定`IP`与`port`
  if (SETSOCKETMODE(lua_kcp->fd, luaL_checkstring(L, 2), luaL_checkinteger(L, 3), CLIENT) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  return 1;
}

static int llisten(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp)
    return 0;

  // 创建`udp`文件描述符
  if ((lua_kcp->fd = NEWSOCKET()) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  // 绑定`udp`文件描述符到指定`IP`与`port`
  if (SETSOCKETMODE(lua_kcp->fd, luaL_checkstring(L, 2), luaL_checkinteger(L, 3), SERVER) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  return 1;
}

// 创建对象
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

// 释放资源
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
    {"connect", lconnect},
    {"listen", llisten},
    {"release", lrelease},
    {NULL, NULL},
  };
  luaL_setfuncs(L, kcp_libs, 0);
  luaL_newlib(L, kcp_libs);
  return 1;
}
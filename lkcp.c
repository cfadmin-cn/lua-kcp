#define LUA_LIB

#include <core.h>
#include "ikcp.h"

#define CLIENT (0)
#define SERVER (1)

#define KCP_BUFFER_SIZE (262144)

typedef struct LUA_KCP{
  int fd;
  ikcpcb *ctx;
  core_io *io;
  lua_State *reader, *sender;
}LUA_KCP;

// 当前时间戳
static inline int64_t current_timestamp() {
  struct timeval ts;
  gettimeofday(&ts, NULL);
  return (int64_t)(ts.tv_sec * 1e3 + (int32_t)(ts.tv_usec * 1e-3));
}

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

// 加大读缓冲区
#ifdef SO_RCVBUF
  int rcv_size = KCP_BUFFER_SIZE;
  ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void *)&rcv_size, sizeof(rcv_size));
  if (ret < 0){
    LOG("ERROR", "Setting IPV6_V6ONLY failed.");
    LOG("ERROR", strerror(errno));
    return core_exit();
  }
#endif

// 加大写缓冲区
#ifdef SO_SNDBUF
  int snd_size = KCP_BUFFER_SIZE;
  ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (void *)&snd_size, sizeof(snd_size));
  if (ret < 0){
    LOG("ERROR", "Setting IPV6_V6ONLY failed.");
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
    // printf("mode: [%d], ip:[%s], port:[%d]\n", mode, ip, port);
    ret = connect(usocket, (const struct sockaddr*)&SA, (socklen_t)sizeof(SA));
  } else if (mode == SERVER) {
    // printf("mode: [%d], ip:[%s], port:[%d]\n", mode, ip, port);
    SA.sin6_family = AF_INET6; SA.sin6_port = htons(port); SA.sin6_addr = in6addr_any;
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

// 输出
static int lua_kcp_output(const char *buffer, int bsize, ikcpcb *kcp, void *user) {
  errno = 0;

  LUA_KCP *lua_kcp = (LUA_KCP *)user;
  if (!lua_kcp || !lua_kcp->ctx || lua_kcp->fd < 0 || !buffer || bsize <= 0 )
    return 0;
  
  int wsize = write(lua_kcp->fd, buffer, bsize);
  if (wsize < 0)
    LOG("ERROR", strerror(errno));
    // printf("[(%d) ERROR]: %s\n", lua_kcp->fd, strerror(errno));

  lua_pushinteger(lua_kcp->sender, wsize);
  // lua_pushinteger(lua_kcp->sender, write(lua_kcp->fd, buffer, bsize));
  int status = CO_RESUME(lua_kcp->sender, NULL, lua_status(lua_kcp->sender) == LUA_YIELD ? lua_gettop(lua_kcp->sender) : lua_gettop(lua_kcp->sender) - 1);
  if (status != LUA_YIELD && status != LUA_OK) {
    LOG("ERROR", lua_tostring(lua_kcp->sender, -1));
    LOG("ERROR", "Error lua_kcp_output Method");
  }
  return 1;
}

// 监听可读事件
static void lua_kcp_reader(core_loop *loop, core_io *w, int revents) {
  if (revents == EV_READ) {
        errno = 0;
    LUA_KCP *lua_kcp = (LUA_KCP *)core_get_watcher_userdata(w);
    char buffer[KCP_BUFFER_SIZE];
    while (1) {
      int rsize = read(w->fd, buffer, KCP_BUFFER_SIZE);
      if (rsize <= 0) {
        if (errno == EINTR)
          continue;
        if (errno == EWOULDBLOCK || !rsize)
          return;
        // TODO: 出错处理?
      }
      /* 验证客户端. */
      if (lua_kcp->ctx->conv == ikcp_getconv(buffer)) {
        /* 将输入、输出响应传递到kcp内部. */
        ikcp_input(lua_kcp->ctx, buffer, rsize);
        ikcp_update(lua_kcp->ctx, current_timestamp());
        lua_pushinteger(lua_kcp->reader, rsize);
        int status = CO_RESUME(lua_kcp->reader, NULL, lua_status(lua_kcp->reader) == LUA_YIELD ? lua_gettop(lua_kcp->reader) : lua_gettop(lua_kcp->reader) - 1);
        if (status != LUA_YIELD && status != LUA_OK) {
          LOG("ERROR", lua_tostring(lua_kcp->reader, -1));
          LOG("ERROR", "Error Lua Reader Method");
        }
      }
    }
  }
}

// 监听连接事件
static void lua_kcp_accept(core_loop *loop, core_io *w, int revents) {
  if (revents == EV_READ) {
    errno = 0;
    // KCP对象
    LUA_KCP *lua_kcp = (LUA_KCP *)core_get_watcher_userdata(w);
    char buffer[KCP_BUFFER_SIZE];
    // memset(buffer, 0x0, KCP_BUFFER_SIZE);
    struct sockaddr_in6 addr;
    // memset(&addr, 0x0, sizeof(addr));
    socklen_t asize = sizeof(addr);
    int rsize = recvfrom(lua_kcp->fd, buffer, KCP_BUFFER_SIZE, 0, (struct sockaddr *)&addr, (socklen_t*)&asize);
    if (rsize <= 0 || lua_kcp->ctx->conv != ikcp_getconv(buffer))
      return; // 这里需要丢弃任何不符合KCP数据包规范与未认证的客户端数据包

    // 调试代码
    // char str[INET6_ADDRSTRLEN];
    // printf("数据: [%s]! 来自 IP:[%s], PORT:[%d], AF: [%d]\n", buffer, inet_ntop(addr.sin6_family, &addr.sin6_addr, str, INET6_ADDRSTRLEN), addr.sin6_port, addr.sin6_family);

    int ret = connect(lua_kcp->fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr));
    if (ret < 0)
      LOG("ERROR", strerror(errno));

    // 将输入、输出响应传递到kcp内部.
    ikcp_input(lua_kcp->ctx, buffer, rsize);
    ikcp_update(lua_kcp->ctx, current_timestamp());

    // 传递数据长度到内部
    lua_pushinteger(lua_kcp->reader, rsize);
    int status = CO_RESUME(lua_kcp->reader, NULL, lua_status(lua_kcp->reader) == LUA_YIELD ? lua_gettop(lua_kcp->reader) : lua_gettop(lua_kcp->reader) - 1);
    if (status != LUA_YIELD && status != LUA_OK) {
      LOG("ERROR", lua_tostring(lua_kcp->reader, -1));
      LOG("ERROR", "Error Lua KCP Accept.");
    }

    /* 后续直接使用读、写回调判断 */
    core_io_stop(loop, w);
    core_set_watcher_userdata(w, (void *)lua_kcp);
    core_io_init(w, lua_kcp_reader, lua_kcp->fd, EV_READ);
    core_io_start(loop, w);
  }
}

static int lsend(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: Invali KCP send context.");

  size_t bsize;
  const char *buffer = (const char *)luaL_checklstring(L, 2, &bsize);
  if (!buffer || bsize < 1)
    return 0;

  if (ikcp_send(lua_kcp->ctx, buffer, bsize))
    return 0;

  ikcp_update(lua_kcp->ctx, current_timestamp());
  lua_pushboolean(L, 1);
  return 1;
}

static int lrecv(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: Invali KCP recv context.");

  lua_Integer bsize = luaL_checkinteger(L, 2);
  if (bsize < 1)
    return luaL_error(L, "[KCP ERROR]: Invali KCP recv bsize %d.", bsize);

  // 如果需要`peek`, 则可以使用第三个变量控制行为.
  if (lua_isboolean(L, 3) && lua_toboolean(L, 3) == 1) {
    lua_pushinteger(L, ikcp_peeksize(lua_kcp->ctx));
    return 1;
  }

  char *buffer;
  if (bsize <= KCP_BUFFER_SIZE){
    bsize = KCP_BUFFER_SIZE;
    buffer = alloca(bsize);
  }else
    buffer = lua_newuserdata(L, bsize);

  int rsize = ikcp_recv(lua_kcp->ctx, buffer, bsize);
  if (rsize <= 0)
    return 0;

  ikcp_update(lua_kcp->ctx, current_timestamp());
  lua_pushlstring(L, buffer, rsize);
  return 1;
}

static int lconnect(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return 0;

  // 创建`udp`文件描述符
  if ((lua_kcp->fd = NEWSOCKET()) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  // 绑定`udp`文件描述符到指定`IP`与`port`
  if (SETSOCKETMODE(lua_kcp->fd, luaL_checkstring(L, 2), luaL_checkinteger(L, 3), CLIENT) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  // 监听
  core_io_init(lua_kcp->io, lua_kcp_reader, lua_kcp->fd, EV_READ);
  core_set_watcher_userdata(lua_kcp->io, (void *)lua_kcp);
  core_io_start(CORE_LOOP_ lua_kcp->io);

  return 0;
}

static int llisten(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return 0;

  // 创建`udp`文件描述符
  if ((lua_kcp->fd = NEWSOCKET()) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  // 绑定`udp`文件描述符到指定`IP`与`port`
  if (SETSOCKETMODE(lua_kcp->fd, luaL_checkstring(L, 2), luaL_checkinteger(L, 3), SERVER) < 0)
    return luaL_error(L, "[KCP ERROR]: %s", strerror(errno));

  // 监听
  core_io_init(lua_kcp->io, lua_kcp_accept, lua_kcp->fd, EV_READ);
  core_set_watcher_userdata(lua_kcp->io, (void *)lua_kcp);
  core_io_start(CORE_LOOP_ lua_kcp->io);

  return 0;
}

// 检查下次调用所需时间
static int lcheck(lua_State *L){
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: %s", "Invalid ctx or ikcp ptr in `ikcp_check`.");

  lua_pushinteger(L, ikcp_check(lua_kcp->ctx, current_timestamp()));
  return 1;
}

// 刷新缓冲区所有数据
static int lupdate(lua_State *L){
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: %s", "Invalid ctx or ikcp ptr in `ikcp_update`.");

  ikcp_update(lua_kcp->ctx, current_timestamp());
  return 1;
}

static int lsetwnd(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: %s", "Invalid ctx or ikcp ptr in `ikcp_wndsize`.");
  ikcp_wndsize(lua_kcp->ctx, luaL_checkinteger(L, 2), luaL_checkinteger(L, 2));
  return 0;
}

static int lsetmode(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: %s", "Invalid ctx or ikcp ptr in `ikcp_mode`.");

  ikcp_nodelay(lua_kcp->ctx, luaL_checkinteger(L, 2), luaL_checkinteger(L, 3), luaL_checkinteger(L, 4), luaL_checkinteger(L, 5));
  return 0;
}

static int lsetmtu(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: %s", "Invalid ctx or ikcp ptr in `ikcp_setmtu`.");

  ikcp_setmtu(lua_kcp->ctx, luaL_checkinteger(L, 2));
  return 0;
}

static int lgetsnd(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)luaL_checkudata(L, 1, "__KCP__");
  if (!lua_kcp || !lua_kcp->ctx)
    return luaL_error(L, "[KCP ERROR]: %s", "Invalid ctx or ikcp ptr in `ikcp_waitsnd`.");

  lua_pushinteger(L, ikcp_waitsnd(lua_kcp->ctx));
  return 1;
}

// 创建对象
static int lcreate(lua_State *L) {
  LUA_KCP *lua_kcp = (struct LUA_KCP *)lua_newuserdata(L, sizeof(struct LUA_KCP));
  if (!lua_kcp)
    return 0;
  lua_kcp->ctx = ikcp_create(luaL_checkinteger(L, 2), lua_kcp);
  // 设置流模式
  // lua_kcp->ctx->stream = 1;
  // 设置最小`rto`
  lua_kcp->ctx->rx_minrto = 10;
  // 设置读/写回调
  lua_kcp->sender = lua_tothread(L, 3);
  lua_kcp->reader = lua_tothread(L, 4);
  lua_kcp->fd = -1;
  lua_kcp->io = xmalloc(sizeof(core_io));
  // 设置输出接口;
  ikcp_setoutput(lua_kcp->ctx, lua_kcp_output);
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
  // 清除事件驱动对象
  if (lua_kcp->io) {
    core_io_stop(CORE_LOOP_ lua_kcp->io);
    xfree(lua_kcp->io);
    lua_kcp->io = NULL;
  }
  // 清除fd资源
  if (lua_kcp->fd >= 0) {
    close(lua_kcp->fd);
    lua_kcp->fd = -1;
  }
  // 断开引用
  lua_kcp->sender = lua_kcp->reader = NULL;
  // LOG("DEBUG", "销毁.");
  return 1;
}

LUAMOD_API int luaopen_lkcp(lua_State *L){
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
  // 注册库方法
  luaL_Reg kcp_libs[] = {
    {"new", lcreate},
    {"send", lsend},
    {"recv", lrecv},
    {"getsnd", lgetsnd},
    {"setwnd", lsetwnd},
    {"setmtu", lsetmtu},
    {"setmode", lsetmode},
    {"connect", lconnect},
    {"listen", llisten},
    {"check", lcheck},
    {"update", lupdate},
    {"release", lrelease},
    {NULL, NULL},
  };
  luaL_setfuncs(L, kcp_libs, 0);
  luaL_newlib(L, kcp_libs);
  return 1;
}
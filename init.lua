local lkcp = require "lkcp"

local lkcp_send = lkcp.send
local lkcp_recv = lkcp.recv
local lkcp_peek = lkcp.recv
local lkcp_update = lkcp.update
local lkcp_getsnd = lkcp.getsnd
local lkcp_setwnd = lkcp.setwnd
local lkcp_setmtu = lkcp.setmtu
local lkcp_setmode = lkcp.setmode
local lkcp_setstream = lkcp.setstream

local dns = require "protocol.dns"
local dns_resolve = dns.resolve

local sys = require "sys"
local new_tab = sys.new_tab

local cf = require "cf"
local cf_fork = cf.fork
local cf_wait = cf.wait
local cf_wakeup = cf.wakeup
local cf_sleep = cf.sleep

local co = coroutine
local co_self = co.running
local co_create = co.create
local co_yield = co.yield

local type = type
local pairs = pairs
local assert = assert
local lower = string.lower
local mrandom = math.random
local toint = math.tointeger

local MAX_RETRIES = 15

local Timer = {}

---comment 分散定时器的复杂度到不同的`Timer`.
---@param self table         @`Timer`
---@param interval integer   @`Timer`的时间
---@param kcp userdata       @`KCP`对象
Timer['dispatch'] = function (self, interval, kcp)
  -- print("开启定时器: ", interval)
  return cf_fork(function ()
    local index = 't' .. interval
    local map = assert(self[index], "[KCP ERROR]: Invalid Timer Map.")
    while true do
      local count = 0
      for obj, cnt in pairs(map) do
        lkcp_update(obj)
        count = count + 1
        -- 如果已经没有数据了, 那么就进入到
        if lkcp_getsnd(obj) < 1 then
          map[obj] = cnt - 1
        else
          if MAX_RETRIES > cnt then
            map[obj] = MAX_RETRIES
          end
        end
        -- 超出多少个时钟后如果还是没数据, 就暂时放弃主动刷新数据。
        if cnt < 1 then
          map[obj] = nil
          count = count - 1
        end
      end
      -- 让去执行权给到其它协程.
      cf_sleep(interval * 1e-3)
      -- 如果表内已经没有任何对象, 那就应该销毁定时器节省资源.
      if count == 0 then
        self[index] = nil
        return
      end
    end
  end)
end

---comment 创建定时器
for _, interval in ipairs({10, 11, 12, 13, 14, 15, 40}) do
  local index = 't' .. interval
  Timer[interval] = function (self, kcp)
    local tab = self[index]
    if tab then
      tab[kcp] = MAX_RETRIES
      return
    end
    self[index] = new_tab(0, 128)
    self[index][kcp] = MAX_RETRIES
    Timer:dispatch(interval, kcp)
    return
  end
end

---comment 移除定时器
Timer.remove = function (self, interval, kcp)
  local map = self['t' .. interval]
  if map then
    map[kcp] = nil
  end
end

local class = require "class"

local KCP = class("KCP")

function KCP:ctor(opt)
  opt = type(opt) == 'table' and opt or {}
  self.conv = assert(toint(opt.conv), "Invalid conv.")
  self.mtu, self.wnd, self.nodelay, self.interval, self.resend, self.nc = 1400, 128, 0, 40, 0, 0
  self.ip = assert(type(opt.ip) == 'string' and opt.ip, "[KCP ERROR]: Invalid IP.")
  self.port = assert(toint(opt.port), "[KCP ERROR]: Invalid port.")
end

function KCP:new_reader()
  self.reader = co_create(function (size)
    while true do
      print("reader", size)
      if not size then
        return self:close()
      end
      if self.closed then
        return
      end
      if self.read_co and lkcp_peek(self.kcp, 1, true) > 0 then
        cf_wakeup(self.read_co, lkcp_recv(self.kcp, lkcp_peek(self.kcp, 1, true)))
        self.read_co = nil
      end
      size = co_yield()
    end
  end)
  return self
end

function KCP:new_sender()
  self.sender = co_create(function (size)
    while true do
      -- print("sender", size)
      if self.closed then
        return
      end
      size = co_yield()
    end
  end)
  return self
end

function KCP:setstream()
  self.stream = true
end

---comment 设置KCP的最大传输单元
function KCP:setmtu(size)
  if self.kcp and toint(size) >= 1 and toint(size) <= 1500 then
    self.mtu = toint(size)
  end
  return self
end

---comment 设置KCP的滑动窗口
function KCP:setwnd(size)
  if self.kcp and toint(size) >= 1 and toint(size) <= 128 then
    self.wnd = toint(size)
  end
  return self
end

---comment 设置KCP的传输模式: 1. `normal`为普通模式; 2. `fast`为快速重传模式;
function KCP:setmode(mode)
  if type(mode) == 'string' and lower(mode) == "fast" then
    self.nodelay, self.interval, self.resend, self.nc = 1, mrandom(10, 15), 2, 1
  elseif type(mode) == 'string' and lower(mode) == "normal" then
    self.nodelay, self.interval, self.resend, self.nc = 0, 40, 0, 0
  end
  return self
end

-- 初始化初始化对象
function KCP:init(ip, port)
  self.ip, self.port = ip, port
  self:new_reader(); -- self:new_sender();
  self.kcp = lkcp:new(self.conv, self.sender, self.reader)
  lkcp_setmtu(self.kcp, self.mtu); lkcp_setwnd(self.kcp, self.wnd)
  lkcp_setmode(self.kcp, self.nodelay, self.interval, self.resend, self.nc)
  -- 流模式
  if self.stream then
    lkcp_setstream(self.kcp)
  end
  return self.kcp
end

-- 内部事件循环
function KCP:dispatch()
  assert(self.kcp, "[KCP ERROR]: `KCP` has not been initialized.")
  Timer[self.interval](Timer, self.kcp)
end

---comment 向对端发送数据
---@param buffer string @需要发送的数据
---@return boolean      @返回值永远为`true`.
function KCP:send(buffer)
  if not self.__MODE__ then
    local ok, ip = dns_resolve(self.ip)
    self.ip = assert(ok and ip, "[KCP ERROR]: Invalid domain or ip.")
    self:init(self.ip, self.port):connect(self.ip, self.port)
    self.__MODE__ = "CLIENT"
  end
  assert(not self.closed, "[KCP ERROR]: called reader when peer closed.")
  self:dispatch()
  -- 让出逻辑执行权的时候, 可以让框架有执行其它逻辑的机会.
  return lkcp_send(self.kcp, buffer)
end

---comment 接收对端发送的数据
---@return string | nil @按包个数接收数据
function KCP:recv()
  if not self.__MODE__ then
    self:init(self.ip, self.port):listen(self.ip, self.port)
    self.__MODE__ = "SERVER"
  end
  assert(not self.closed, "[KCP ERROR]: called sender when peer closed.")
  self:dispatch()
  local rsize = lkcp_peek(self.kcp, 1, true)
  if rsize > 0 then
    return lkcp_recv(self.kcp, rsize)
  end
  self.read_co = co_self()
  return cf_wait()
end

-- 获取发送缓冲区参与大小
function KCP:getsnd()
  return assert(self.kcp, "[KCP ERROR]: `KCP` has not been initialized."):getsnd()
end

-- 当前有多少的活跃连接.
function KCP:count()
  local COUNTER = {}
  for interval = 10, 15 do
    local count = 0
    local index = 't' .. interval
    for _, _ in pairs(Timer[index] or {}) do
      count = count + 1
    end
    COUNTER[index] = count
  end
  local count = 0
  for _, _ in pairs(Timer['t' .. '40'] or {}) do
    count = count + 1
  end
  COUNTER['t' .. '40'] = count
  return COUNTER
end

-- 销毁资源
function KCP:close()
  if self.closed then
    return
  end
  -- 清理回调
  if self.read_co then
    cf_wakeup(self.read_co)
  end
  -- 定时器
  Timer:remove(self.interval, self.kcp)
  -- 清理对象
  if self.kcp then
    self.kcp:release()
    self.kcp = nil
  end
  -- 清理协程
  self.sender = nil
  self.reader = nil
  self.closed = true
end

return KCP

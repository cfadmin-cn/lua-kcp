local lkcp = require "lkcp"

local lkcp_send = lkcp.send
local lkcp_recv = lkcp.recv
local lkcp_peek = lkcp.recv
local lkcp_update = lkcp.update
local lkcp_setwnd = lkcp.setwnd
local lkcp_setmtu = lkcp.setmtu
local lkcp_setmode = lkcp.setmode

local dns = require "protocol.dns"
local dns_resolve = dns.resolve

local sys = require "sys"
local new_tab = sys.new_tab

local cf = require "cf"
local cf_wait = cf.wait
local cf_wakeup = cf.wakeup
local cf_sleep = cf.sleep

local co = coroutine
local co_self = co.running
local co_create = co.create
local co_yield = co.yield

local next = next
local pairs = pairs
local assert = assert
local lower = string.lower
local mrandom = math.random
local toint = math.tointeger

local class = require "class"

local Timer = {}

---comment 检查定时器内的所有对象
---@param interval integer   @`Timer`的时间
---@param map table          @`Timer`的时间
Timer['check'] = function (self, interval, map)
  if not next(map) then
    self['t' .. interval] = nil
    return false
  end
  return true
end

---comment 分散定时器的复杂度到不同的`Timer`.
---@param self table         @`Timer`
---@param interval integer   @`Timer`的时间
---@param kcp userdata       @`KCP`对象
Timer['dispatch'] = function (self, interval, kcp)
  -- print("开启定时器: ", interval)
  return cf.fork(function ()
    local map = assert(self['t' .. interval], "[KCP ERROR]: Invalid Timer Map.")
    while true do
      for _, obj in pairs(map) do
        lkcp_update(obj)
      end
      cf_sleep(interval * 1e-3)
      -- 如果表内已经没有任何对象, 那么销毁定时器节省资源.
      if not Timer:check(interval, map) then
        return
      end
    end
  end)
end

---comment 创建定时器
for _, timeout in ipairs({10, 11, 12, 13, 14, 15, 40}) do
  Timer[timeout] = function (self, kcp)
    if not self['t' .. timeout] then
      self['t' .. timeout] = new_tab(0, 128)
    end
    self['t' .. timeout][kcp] = kcp
    return Timer:dispatch(timeout, kcp)
  end
end

---comment 移除定时器
Timer.remove = function (self, interval, kcp)
  self['t' .. interval][kcp] = nil
end

local KCP = class("KCP")

function KCP:ctor(opt)
  opt = type(opt) == 'table' and opt or {}
  self.conv = assert(toint(opt.conv), "Invalid conv.")
  self.mtu, self.wnd, self.nodelay, self.interval, self.resend, self.nc = 1400, 128, 0, 40, 0, 0
  self.ip = assert(type(opt.ip) == 'string' and opt.ip, "[KCP ERROR]: Invalid IP.")
  self.port = assert(toint(opt.port), "[KCP ERROR]: Invalid port.")
  self.sender = co_create(function (size)
    while true do
      -- print("sender", size)
      size = co_yield()
    end
  end)
  self.reader = co_create(function (size)
    while true do
      -- print("reader", size)
      if self.closed then
        return
      end
      if self.read_co then
        cf_wakeup(self.read_co, size)
      end
      size = co_yield()
    end
  end)
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
  else
    self.nodelay, self.interval, self.resend, self.nc = 0, 40, 0, 0
  end
  return self
end

-- 初始化初始化对象
function KCP:init(ip, port)
  self.ip, self.port = ip, port
  self.kcp = lkcp:new(self.conv, self.sender, self.reader)
  lkcp_setmtu(self.kcp, self.mtu); lkcp_setwnd(self.kcp, self.wnd)
  lkcp_setmode(self.kcp, self.nodelay, self.interval, self.resend, self.nc)
  if not self.__MODE__ then
    self:dispatch()
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
  return lkcp_send(self.kcp, buffer)
end

---comment 接收对端发送的数据
---@return string | nil @按包个数接收数据
function KCP:recv()
  if not self.__MODE__ then
    self:init(self.ip, self.port):listen(self.ip, self.port)
    self.__MODE__ = "SERVER"
  end
  local co = co_self()
  self.read_co = cf.fork(function ()
    while lkcp_peek(self.kcp, 1, true) < 0 do
      if not cf_wait() or self.closed then
        self.read_co = nil
        return cf_wakeup(co)
      end
    end
    self.read_co = nil
    return cf_wakeup(co, lkcp_recv(self.kcp, lkcp_peek(self.kcp, 1, true)))
  end)
  return cf_wait()
end


function KCP:getsnd()
  return assert(self.kcp, "[KCP ERROR]: `KCP` has not been initialized."):getsnd()
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
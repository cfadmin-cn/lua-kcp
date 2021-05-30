local lkcp = require "lkcp"

local dns = require "protocol.dns"
local dns_resolve = dns.resolve

local assert = assert
local lower = string.lower
local toint = math.tointeger

local co = coroutine
local co_create = co.create
local co_close = co.close or function () end

local class = require "class"

local KCP = class("KCP")

function KCP:ctor(conv)
  self.conv = toint(conv) or math.random(1, 4294967295)
  self.nodelay, self.interval, self.resend, self.nc = 0, 40, 0, 0
  self.mtu, self.wnd = 1400, 128
  self.sender = co_create(function (...)
    print("sender", ...)
  end)
  self.reader = co_create(function (...)
    print("reader", ...)
  end)
end

-- 设置KCP的最大传输单元
function KCP:setmtu(size)
  if self.kcp and toint(size) >= 1 and toint(size) <= 1500 then
    self.mtu = toint(size)
  end
  return self
end

-- 设置KCP的滑动窗口
function KCP:setwnd(size)
  if self.kcp and toint(size) >= 1 and toint(size) <= 128 then
    self.wnd = toint(size)
  end
  return self
end

-- 设置KCP的传输模式:
--   1. `normal`为普通模式;
--   2. `fast`为快速重传模式.
function KCP:setmode(mode)
  if lower(mode) == "fast" then
    self.nodelay, self.interval, self.resend, self.nc = 1, 10, 2, 1
  else
    self.nodelay, self.interval, self.resend, self.nc = 0, 40, 0, 0
  end
  return self
end

-- 初始化kcp对象
function KCP:init(ip, port)
  self.ip = assert(dns_resolve(ip), "[KCP ERROR]: Invalid domain or ip.")
  self.port = assert(toint(port), "[KCP ERROR]: Invalid port.")
  self.kcp = lkcp:new(self.conv, self.sender, self.reader)
  self.kcp:setmtu(self.mtu); self.kcp:setwnd(self.wnd)
  self.kcp:setmode(self.nodelay, self.interval, self.resend, self.nc)
end

-- 服务端模式
function KCP:listen(ip, port)
  self:init(ip, port)
end

-- 客户端模式
function KCP:connect(ip, port)
  self:init(ip, port)
end

-- 销毁资源
function KCP:close()
  if self.kcp then
    self.kcp:release()
    self.kcp = nil
  end
end

return KCP
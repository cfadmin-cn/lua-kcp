local lkcp = require "lkcp"

local dns = require "protocol.dns"
local dns_resolve = dns.resolve

local type = type
local assert = assert

local toint = math.tointeger

local co = coroutine
local co_create = co.create
local co_close = co.close or function () end

local class = require "class"

local KCP = class("KCP")

function KCP:ctor(conv)
  self.conv = toint(conv) or math.random(1, 4294967295)
  self.nodelay, self.interval, self.resend, self.nc = 0, 40, 0, 0
  self.sender = co_create(function (...)
    print("sender", ...)
  end)
  self.reader = co_create(function (...)
    print("reader", ...)
  end)
end

-- `normal`为普通模式, `fast`为快速重传模式.
function KCP:setmode(mode)
  if (mode) == "fast" then
    self.nodelay, self.interval, self.resend, self.nc = 1, 10, 2, 1
  else
    self.nodelay, self.interval, self.resend, self.nc = 0, 40, 0, 0
  end
  return self
end

-- 服务端模式
function KCP:listen(ip, port)
  ip, port = assert(dns_resolve(ip)), toint(port)
  self.kcp = lkcp:new(self.conv)
  self.kcp:setmode(self.nodelay, self.interval, self.resend, self.nc)
end

-- 客户端模式
function KCP:connect(ip, port)
  ip, port = assert(dns_resolve(ip)), toint(port)
end

-- 销毁
function KCP:close()
  if self.kcp then
    self.kcp = nil
  end
end

return KCP
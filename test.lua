local lua_kcp = require "lua-kcp"

local cf = require "cf"

cf.fork(function ()
	local k1 = lua_kcp:new { conv = 1, ip = "127.0.0.1", port = 8082 }
	print("1.开始")
	print("1.发送1:", k1:send("車"))
	print("1.发送2:", k1:send("先生"))
	print("1.发送3:", k1:send("車爪嘟"))
	print("1.发送4:", k1:send(("車太太"):rep(200)))
	print("1.结束", k1:getsnd())
	cf.wait()
end)

local k2 = lua_kcp:new { conv = 1, ip = "localhost", port = 8082 }
print("2.开始")
print("2.接收1:", k2:recv())
print("2.接收2:", k2:recv())
print("2.接收3:", k2:recv())
print("2.接收4:", k2:recv())
print("2.结束", k2:getsnd())

cf.wait()
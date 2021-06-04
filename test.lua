-- 注意:
-- 	1. 将KCP设置为`fast`模式可以提升实时性;
-- 	2. 建议使用KCP传输的数据包应该尽可能的小;
-- 	3. 使用前请先做技术调研确认是否必须使用KCP;
-- 	4. 不再使用的时候请注意销毁所有对象资源;

local lua_kcp = require "lua-kcp"

local cf = require "cf"

local k1 = lua_kcp:new { conv = 1, ip = "127.0.0.1", port = 9999 }
k1:setmode("fast")

local k2 = lua_kcp:new { conv = 1, ip = "localhost", port = 9999 }
k2:setmode("fast")


cf.fork(function ()
	print("1.开始")
	print("1.发送1:", k1:send("車"))
	print("1.发送2:", k1:send("先生"))
	print("1.发送3:", k1:send("車爪嘟"))
	print("1.发送4:", k1:send(("車太太"):rep(200)))
	print("1.结束", k1:getsnd())
	cf.wait()
end)

print("2.开始")
print("2.接收1:", k2:recv())
print("2.接收2:", k2:recv())
print("2.接收3:", k2:recv())
print("2.接收4:", k2:recv())
print("2.结束", k2:getsnd())

k1:close(); k2:close();

cf.wait()
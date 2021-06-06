local lua_kcp = require "lua-kcp"

local cf = require "cf"

local index = 0

for i = 82, index + 82 do
  local k = lua_kcp:new { conv = 1, ip = "localhost", port = 8000 + i }
  k:setmode("fast")

  cf.fork(function ()
    while true do
      print(k:recv())
    end
    cf.wait()
  end)

end

cf.wait()
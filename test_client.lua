local lua_kcp = require "lua-kcp"

local cf = require "cf"

local index = 0

for i = 82, index + 82 do
  local k = lua_kcp:new { conv = 1, ip = "127.0.0.1", port = 8000 + i }
  k:setmode("fast")

  cf.fork(function ()
    while true do
      k:send("車")
      k:send("先生")
      k:send("車爪嘟")
      -- cf.sleep(math.random() * 1e3 // 1 * 1e-3)
    end
  end)

end

cf.wait()
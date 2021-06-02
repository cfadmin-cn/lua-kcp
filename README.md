# Lua-kcp

  基于`cfadmin`的`Lua C API`版本封装的[KCP](https://github.com/skywind3000/kcp)实现.

## 优势

  - [x] 基于事件驱动模型, 不浪费CPU的空闲时间;

  - [x] 更加快的请求响应, 更优秀的公网交互体验;

  - [x] 完善的`API`提供, 简单好用更加适合自己;

  - [x] 自动内存分配管理, 想要就`new`、用完就`close`;

  - [x] 客户端、服务器双向交互判断, 去除恶意请求带来的影响;

  - [x] 其他更多优点, 请自行探索...

## 构建

  1. 将本项目克隆到`3rd`目录;
  
  2. 进入目录执行`make build`命令编译;

## 内置API

  使用前先导入`API`: `local lkcp = require "lua-kcp"`


### 1. 创建对象
  ---

  函数原型: `lkcp:new(opt) return kcp end`

  使用`lkcp:new(opt)`创建一个`kcp`实例对象, 这个对象可用于后续的所有操作.

  `opt`是一个`table`类型的参数, 内部必须包含以下属性作为参数:

  `opt.conv` - `kcp`用的`uint32`类型会话编号, 主要用作传输层的对端校验.

  `opt.ip` - 服务器或客户端用到的地址

  `opt.port` - 服务器或客户端用到的端口


### 2. 发送与接收

  ---

  函数原型: `kcp:send(buffer) return boolean end`

  使用此方法将会向对端发送的字符串.

  函数原型: `kcp:recv() return buffer end`

  使用此方法会接收对端发送的字符串.

### 3. 获取、修改属性

  ---

  函数原型: `kcp:setstream() return nil end`

  设置`KCP`为流模式(默认为包模式), 需要客户端设置才有效.

  函数原型: `kcp:setmtu(mtu) return nil end`

  设置`KCP`最大传输单元.

  函数原型: `kcp:setwnd(mtu) return nil end`

  设置`KCP`滑动窗口大小(包).

  函数原型: `kcp:setmode(mode) return nil end`

  设置`mode`传输模式: `fast`(快速), `normal`(普通).

  函数原型: `kcp:getsnd() return integer end`

  返回`KCP`待发送的数据包数量.


### 4. 关闭

  ---

  函数原型: `kcp:close() return nil end`

  释放资源.


## 示例

<details>
  <summary>示例代码 : </summary>

```lua
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
```

</details>

<details>
  <summary>输出结果 : </summary>

```bash
[candy@MacBookPro:~/Documents/cfadmin] $ ./cfadmin
2.开始
1.开始
1.发送1:	true
1.发送2:	true
1.发送3:	true
1.发送4:	true
1.结束	5
2.接收1:	車
2.接收2:	先生
2.接收3:	車爪嘟
2.接收4:	車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太車太太
2.结束	0
```

</details>

## 许可

  [MIT](https://github.com/CandyMi/lua-kcp/blob/master/LICENSE)

## 最后

  * 大、小包发送方法一致, 数据按包分割.

  * `conv`必须为`uint32`类型, 同时客户端与服务器必须一致.

  * 支持`IPv6`与`IPv4`同时监听, 不可单独指定监听的`IP`地址.

  * `UDP`服务端号分配方法请自行解决, 如无法自行解决请使用`TCP`.
  


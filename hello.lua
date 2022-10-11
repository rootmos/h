--os.execute("sudo -A id")
--os.execute("echo hello")

print(os.date("%FT%T%z"))

print(math.random())

local L = require("lib")
print(L.foo)

local f = io.tmpfile()
f:write("hello")
f:seek("set", 0)
print(f:read("*a"))

local fn = os.tmpname()
io.open(fn, "w"):close()
os.remove(fn)

os.exit(3)

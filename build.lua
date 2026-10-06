-- silly little OS detection
local is_windows = package.config:sub(1,1) == "\\"

function filename(file)
    return file:match("^.+/(.+)%..+")
end

function cc(cc, src, out, cflags)
    local cmd = cc.." "..src.." -I . -I ../4coder_byp/4coder_qol/custom -DFTECH_64_BIT "..cflags.." -o "..out

    print(cmd)
    return os.execute(cmd)
end

local cflags = "-g -Wno-null-dereference -rdynamic -fuse-ld=lld -O0 -Wno-write-strings"
local libs = ""

cflags = cflags.." -I /usr/include/"
cflags = cflags.." -I /usr/include/freetype2"
cflags = cflags.." -L /usr/lib"

if true then -- GCC
    cflags = cflags.." -D_GNU_SOURCE -fPIC  -Wno-unused-result -Wno-undefined-internal"
    libs = libs.." -lX11 -lXrandr -lm -lrt -lGL -ldl -lXfixes -lfreetype -fno-threadsafe-statics -pthread"
end

cc("clang++", "4ed_api_parser_main.cpp", "a.out", cflags) -- custom API gen
os.execute("./a.out 4ed_api_implementation.cpp")

cc("clang++", "platform_linux/linux_4ed.cpp", "../test_build/4ed2",        cflags.." "..libs)  -- platform layer
cc("clang",   "4ed_app_target.cpp  cbt.c",    "../test_build/4ed_app2.so", cflags.." -shared") -- 4coder main DLL

-- silly little OS detection
local is_windows = package.config:sub(1,1) == "\\"

function filename(file)
	return file:match("^.+/(.+)%..+")
end

function cc(cc, src, out, cflags)
	local cmd = cc.." "..src.." -I . -I ../4coder_qol/custom -DFTECH_64_BIT "..cflags.." -o "..out

	print(cmd)
	return os.execute(cmd)
end

local cflags = "-g -Wno-null-dereference -rdynamic -fuse-ld=lld -O1 -Wno-write-strings"
local libs = ""

cflags = cflags.." -I /home/linuxbrew/.linuxbrew/include/"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/libx11/include"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/freetype2/include/freetype2"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/xorgproto/include"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/libxcb/include"
cflags = cflags.." -L /home/linuxbrew/.linuxbrew/lib -L /home/linuxbrew/.linuxbrew/lib/gcc/15"
-- cflags = cflags.." -I /usr/include/freetype2"

if true then -- GCC
	cflags = cflags.." -D_GNU_SOURCE -fPIC"
    libs = libs.." -lX11 -lXrandr -lm -lrt -lGL -ldl -lXfixes -lfreetype -fno-threadsafe-statics -pthread -Wno-unused-result"
end

cc("clang++", "platform_linux/linux_4ed.cpp", "../test_build/4ed2",        cflags.." "..libs)  -- platform layer
cc("clang",   "4ed_app_target.cpp  cbt.c",    "../test_build/4ed_app2.so", cflags.." -shared") -- 4coder main DLL

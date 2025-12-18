-- silly little OS detection
local is_windows = package.config:sub(1,1) == "\\"

function filename(file)
	return file:match("^.+/(.+)%..+")
end

function cc(src, out, cflags)
	local cmd = "clang++ "..src.." -I . -I custom -DFTECH_64_BIT "..cflags.." -o "..out

	print(cmd)
	return os.execute(cmd)
end

local cflags = "-g -Wno-null-dereference -rdynamic -fuse-ld=lld -O0 -Wno-write-strings -DUSE_SPALL_AUTO -finstrument-functions"
local libs = ""

cflags = cflags.." -I /home/linuxbrew/.linuxbrew/include/"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/libx11/include"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/freetype2/include/freetype2"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/xorgproto/include"
cflags = cflags.." -I /home/linuxbrew/.linuxbrew/opt/libxcb/include"
cflags = cflags.." -L /home/linuxbrew/.linuxbrew/lib -L /home/linuxbrew/.linuxbrew/lib/gcc/15"

if true then -- GCC
	cflags = cflags.." -D_GNU_SOURCE -fPIC"
    libs = libs.." -lX11 -lXrandr -lm -lrt -lGL -ldl -lXfixes -lfreetype -fno-threadsafe-statics -pthread -Wno-unused-result -std=c++11"
end

cc("platform_linux/linux_4ed.cpp", "../build/4ed",        cflags.." "..libs)  -- platform layer
cc("4ed_app_target.cpp",           "../build/4ed_app.so", cflags.." -shared") -- 4coder main DLL

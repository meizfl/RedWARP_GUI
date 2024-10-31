RedWARPGUI: RedWARPGUI.cpp
	clang++ -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/c++/14 -g -fstack-protector-strong -Wformat -Werror=format-security -D_THREAD_SAFE -D_REENTRANT -O2 -flto -std=gnu++2c -march=x86-64 -o RedWARPGUI RedWARPGUI.cpp -lfltk -lX11 -lpthread -lstdc++


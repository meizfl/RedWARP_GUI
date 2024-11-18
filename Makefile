RedWARPGUI: RedWARPGUI.cpp
	clang++ -I/usr/local/include -I/usr/local/include/FL/images -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_THREAD_SAFE -D_REENTRANT -flto -O2 -std=gnu++2c -march=x86-64 -o RedWARPGUI RedWARPGUI.cpp /usr/local/lib64/libfltk_images.a /usr/lib64/libjpeg.so /usr/lib64/libpng.so /usr/lib64/libz.so /usr/local/lib64/libfltk.a -lm -lpthread -lXinerama -lXfixes -lXcursor -L/usr/lib64 -lpangoxft-1.0 -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0 -lharfbuzz -lfontconfig -lfreetype -lXft -lpangocairo-1.0 -lcairo -lgtk-3 -lgdk-3 -lgio-2.0 -lX11 -lXrender -lwayland-cursor -lwayland-client -lxkbcommon -ldbus-1 -ldl



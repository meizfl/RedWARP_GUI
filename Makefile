# Название бинарного файла
TARGET = RedWARPGUI

# Исходный файл
SRC = RedWARPGUI.cpp

# Компилятор и флаги
CXX = clang++
CXXFLAGS = -I/usr/local/include -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_THREAD_SAFE -D_REENTRANT
LDFLAGS = /usr/local/lib64/libfltk.a -lm -lpthread -lXinerama -lXfixes -lXcursor -L/usr/lib64 -lpangoxft-1.0 -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0 -lharfbuzz -lfontconfig -lfreetype -lXft -lpangocairo-1.0 -lcairo -lX11 -lXrender -lwayland-cursor -lwayland-client -lxkbcommon -ldecor-0 -ldl

# Путь к файлу info.toml
INFO_FILE = info.toml

# Сборка
all: $(TARGET) $(INFO_FILE)

# Правила для создания бинарника
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Правило для создания файла info.toml
$(INFO_FILE):
	@echo "[platform]" > $(INFO_FILE)
	@echo "os = \"$(shell uname -s)\"" >> $(INFO_FILE)
	@echo "arch = \"$(shell uname -m)\"" >> $(INFO_FILE)
	@echo "" >> $(INFO_FILE)
	@echo "[compiler]" >> $(INFO_FILE)
	@echo "name = \"$(shell $(CXX) --version | head -n 1)\"" >> $(INFO_FILE)
	@echo "version = \"$(shell $(CXX) --version | head -n 1 | cut -d' ' -f3)\"" >> $(INFO_FILE)
	@echo "" >> $(INFO_FILE)
	@echo "[build]" >> $(INFO_FILE)
	@echo "date = \"$(shell date '+%Y-%m-%d %H:%M:%S')\"" >> $(INFO_FILE)

# Очистка
clean:
	rm -f $(TARGET) $(INFO_FILE)

# Название бинарного файла
TARGET = RedWARPGUI
# Исходный файл
SRC = RedWARPGUI.cpp
# Компилятор
CXX = clang++
# Флаги из fltk-config
CXXFLAGS = $(shell fltk-config --cxxflags)
LDFLAGS  = $(shell fltk-config --ldflags)
# Путь к файлу info.toml
INFO_FILE = info.toml
# Сборка
all: $(TARGET) $(INFO_FILE)
# Правило для создания бинарника
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

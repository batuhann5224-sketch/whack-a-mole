# ==========================================
# Köstebek Vurmaca Oyunu - Makefile
# ==========================================

# Derleyici
CC = gcc

# Derleme Bayrakları (Hata ayıklama ve optimizasyon)
CFLAGS = -Wall -Wextra -std=c99 -O2

# İşletim sistemi tespiti ve uygun kütüphane bağlama ayarları
ifeq ($(OS),Windows_NT)
    # Windows (MinGW-w64 / MSYS2) için bağlayıcı bayrakları
    LDFLAGS = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_mixer
    TARGET = kostebek_vurmaca.exe
    CLEAN_CMD = del /Q /F $(TARGET) 2>NUL || rm -f $(TARGET)
else
    # Linux / macOS için bağlayıcı bayrakları
    LDFLAGS = -lSDL2 -lSDL2_image -lSDL2_mixer
    TARGET = kostebek_vurmaca
    CLEAN_CMD = rm -f $(TARGET)
endif

# Kaynak Kodları
SRCS = main.c

# Varsayılan derleme hedefi
all: $(TARGET)

# Hedef derleme kuralı
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

# Temizlik kuralı
clean:
	$(CLEAN_CMD)

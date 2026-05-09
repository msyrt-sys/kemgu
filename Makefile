# =============================================================================
# KEMGU Makefile — Dual-compiler: UCRT64 GCC (prod) + Clang64 (ASan testleri)
# =============================================================================
#
# PATH gereksinimi (her iki MSYS2 dagitimi da PATH'te olmalidir):
#   export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
#
# Sebep:
#   - UCRT64 GCC: prod kemgu.exe + lexer testi (hizli, optimize edilebilir)
#   - Clang64:   bellek alan modul testleri (ASan + UBSan runtime tam destek)
#                MinGW-w64 GCC Win11'de ASan runtime kutuphanelerini icermez,
#                Dr. Memory de Win11 26200'de DynamoRIO uyumsuzlugu nedeniyle
#                kullanilamaz. Bu yuzden test_arena (ve gelecek test_ast,
#                test_parser) icin clang gerekli.
# =============================================================================

CC = gcc          # Prod derleyici (UCRT64 MinGW-w64 GCC)
CC_ASAN = clang   # ASan test derleyicisi (Clang64)

CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -O0

# AddressSanitizer + UBSan — bellek alan modul testleri icin (Clang64 ile)
ASAN_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

SRCDIR = src
TESTDIR = test
BUILD = build

# Windows'ta GCC/Clang ciktiya otomatik .exe ekler
ifeq ($(OS),Windows_NT)
    EXE := .exe
else
    EXE :=
endif

SRCS = $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c $(SRCDIR)/hata.c \
       $(SRCDIR)/lexer.c $(SRCDIR)/arena.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean test calistir_lexer_test calistir_arena_test test_tumu

# === Ana hedef ===

all: $(BUILD)/kemgu$(EXE)

$(BUILD)/kemgu$(EXE): $(OBJS) $(BUILD)/ana.o
	$(CC) $(CFLAGS) -o $@ $^

# === Lexer testi (UCRT64 GCC, ASan'siz — lexer malloc kullanmiyor) ===

$(BUILD)/test_lexer$(EXE): $(OBJS) $(BUILD)/test_lexer.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_lexer.o: $(TESTDIR)/test_lexer.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c -o $@ $<

# === Arena testi (Clang64 + ASan AKTIF — tek-shot derleme) ===
# Runtime DLL: libclang_rt.asan_dynamic-x86_64.dll (Clang64/bin'de, PATH'te olmali)

$(BUILD)/test_arena$(EXE): $(SRCDIR)/arena.c $(TESTDIR)/test_arena.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Genel obje kurallari ===

$(BUILD)/%.o: $(SRCDIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# === Test calistirma hedefleri ===

test: $(BUILD)/test_lexer$(EXE)
	./$(BUILD)/test_lexer$(EXE)

calistir_lexer_test: test

calistir_arena_test: $(BUILD)/test_arena$(EXE)
	./$(BUILD)/test_arena$(EXE)

test_tumu: calistir_lexer_test calistir_arena_test
	@echo "Tum testler gecti!"

clean:
	rm -rf $(BUILD)

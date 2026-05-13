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

CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -O0 -MMD -MP
DEPFLAGS = -MMD -MP

# AddressSanitizer + UBSan — bellek alan modul testleri icin (Clang64 ile)
ASAN_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

SRCDIR = src
TESTDIR = test
BUILD = build

# Platform/mimari tespiti — Windows / Linux / macOS, x86_64 / ARM64
# (DGX Spark + Android NDK ARM64 portu icin altyapi)
ifeq ($(OS),Windows_NT)
    EXE := .exe
    PLATFORM := windows
    ARCH := x86_64
else
    EXE :=
    UNAME_S := $(shell uname -s)
    UNAME_M := $(shell uname -m)
    ifeq ($(UNAME_S),Linux)
        PLATFORM := linux
    endif
    ifeq ($(UNAME_S),Darwin)
        PLATFORM := macos
    endif
    ifeq ($(UNAME_M),aarch64)
        ARCH := arm64
    endif
    ifeq ($(UNAME_M),arm64)
        ARCH := arm64
    endif
    ifeq ($(UNAME_M),x86_64)
        ARCH := x86_64
    endif
endif

SRCS = $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c $(SRCDIR)/hata.c \
       $(SRCDIR)/lexer.c $(SRCDIR)/arena.c $(SRCDIR)/ast.c $(SRCDIR)/ast_yazdir.c \
       $(SRCDIR)/parser.c $(SRCDIR)/ifade.c $(SRCDIR)/tip.c $(SRCDIR)/sembol.c \
       $(SRCDIR)/tip_kontrol.c $(SRCDIR)/bolge.c $(SRCDIR)/bolge_atama.c \
       $(SRCDIR)/escape.c $(SRCDIR)/llvm.c $(SRCDIR)/json.c $(SRCDIR)/lsp.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean test calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_linear_test calistir_stdlib_check test_tumu

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

# === AST testi (Clang64 + ASan AKTIF — arena + ast + ast_yazdir) ===

$(BUILD)/test_ast$(EXE): $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                         $(SRCDIR)/ast_yazdir.c $(TESTDIR)/test_ast.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Parser testi (Clang64 + ASan AKTIF — tum bagimlilliklar) ===

$(BUILD)/test_parser$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                            $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                            $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                            $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                            $(SRCDIR)/ifade.c $(TESTDIR)/test_parser.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Tip testi (Clang64 + ASan — arena + tip) ===

$(BUILD)/test_tip$(EXE): $(SRCDIR)/arena.c $(SRCDIR)/tip.c \
                         $(TESTDIR)/test_tip.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Sembol testi (Clang64 + ASan — arena + tip + sembol) ===

$(BUILD)/test_sembol$(EXE): $(SRCDIR)/arena.c $(SRCDIR)/tip.c $(SRCDIR)/sembol.c \
                            $(TESTDIR)/test_sembol.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Tip kontrolu testi (Clang64 + ASan — tum bagimliliklar) ===

$(BUILD)/test_tip_kontrol$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                                  $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                                  $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                                  $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                                  $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                                  $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                                  $(TESTDIR)/test_tip_kontrol.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Bolge testi (Clang64 + ASan — arena + bolge) ===

$(BUILD)/test_bolge$(EXE): $(SRCDIR)/arena.c $(SRCDIR)/bolge.c \
                           $(TESTDIR)/test_bolge.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Bolge atama testi (Clang64 + ASan — tum bagimliliklar) ===

$(BUILD)/test_bolge_atama$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                                  $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                                  $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                                  $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                                  $(SRCDIR)/ifade.c $(SRCDIR)/bolge.c \
                                  $(SRCDIR)/bolge_atama.c $(SRCDIR)/escape.c \
                                  $(TESTDIR)/test_bolge_atama.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Escape analiz testi (Clang64 + ASan — parser + escape) ===

$(BUILD)/test_escape$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                            $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                            $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                            $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                            $(SRCDIR)/ifade.c $(SRCDIR)/escape.c \
                            $(TESTDIR)/test_escape.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === JSON testi (Clang64 + ASan) ===

$(BUILD)/test_json$(EXE): $(SRCDIR)/arena.c $(SRCDIR)/json.c \
                          $(TESTDIR)/test_json.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === LSP testi (Clang64 + ASan — tum bagimliliklar + json + lsp) ===

$(BUILD)/test_lsp$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                        $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                        $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                        $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                        $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                        $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                        $(SRCDIR)/json.c $(SRCDIR)/lsp.c \
                        $(TESTDIR)/test_lsp.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === LLVM backend entegrasyon testi (GCC, ASan'siz — system() ile harici cagri) ===
# kemgu.exe ve clang'a baglidir.

$(BUILD)/test_llvm$(EXE): $(TESTDIR)/test_llvm.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $<

# === Linear Types Spec V1 testi (Clang64 + ASan — full pipeline) ===

$(BUILD)/test_linear$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                            $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                            $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                            $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                            $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                            $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                            $(TESTDIR)/test_linear.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^


# === Genel obje kurallari ===

$(BUILD)/%.o: $(SRCDIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# Otomatik basligi degisikligi izleme (-MMD -MP ile uretilen .d dosyalari)
-include $(OBJS:.o=.d)
-include $(BUILD)/ana.d
-include $(BUILD)/test_lexer.d

# === Test calistirma hedefleri ===

test: $(BUILD)/test_lexer$(EXE)
	./$(BUILD)/test_lexer$(EXE)

calistir_lexer_test: test

calistir_arena_test: $(BUILD)/test_arena$(EXE)
	./$(BUILD)/test_arena$(EXE)

calistir_ast_test: $(BUILD)/test_ast$(EXE)
	./$(BUILD)/test_ast$(EXE)

calistir_parser_test: $(BUILD)/test_parser$(EXE)
	./$(BUILD)/test_parser$(EXE)

calistir_tip_test: $(BUILD)/test_tip$(EXE)
	./$(BUILD)/test_tip$(EXE)

calistir_sembol_test: $(BUILD)/test_sembol$(EXE)
	./$(BUILD)/test_sembol$(EXE)

calistir_tip_kontrol_test: $(BUILD)/test_tip_kontrol$(EXE)
	./$(BUILD)/test_tip_kontrol$(EXE)

calistir_bolge_test: $(BUILD)/test_bolge$(EXE)
	./$(BUILD)/test_bolge$(EXE)

calistir_bolge_atama_test: $(BUILD)/test_bolge_atama$(EXE)
	./$(BUILD)/test_bolge_atama$(EXE)

calistir_escape_test: $(BUILD)/test_escape$(EXE)
	./$(BUILD)/test_escape$(EXE)

calistir_json_test: $(BUILD)/test_json$(EXE)
	./$(BUILD)/test_json$(EXE)

calistir_lsp_test: $(BUILD)/test_lsp$(EXE)
	./$(BUILD)/test_lsp$(EXE)

calistir_llvm_test: $(BUILD)/test_llvm$(EXE) $(BUILD)/kemgu$(EXE)
	./$(BUILD)/test_llvm$(EXE)

calistir_linear_test: $(BUILD)/test_linear$(EXE)
	./$(BUILD)/test_linear$(EXE)

# Stdlib tip-kontrolu — saf KEMGU stdlib modullerinin --check'ten gecmesi
calistir_stdlib_check: $(BUILD)/kemgu$(EXE)
	@echo "stdlib tip kontrolu..."
	@for f in stdlib/temel/*.kem; do \
		./$(BUILD)/kemgu$(EXE) --check $$f || exit 1; \
	done
	@echo "Tum stdlib modulleri --check gecti!"

test_tumu: calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_linear_test calistir_stdlib_check
	@echo "Tum testler gecti!"

clean:
	rm -rf $(BUILD)

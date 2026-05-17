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
       $(SRCDIR)/lexer.c $(SRCDIR)/arena.c $(SRCDIR)/ast.c $(SRCDIR)/ast_yazdir.c \
       $(SRCDIR)/ast_kaynak.c \
       $(SRCDIR)/parser.c $(SRCDIR)/ifade.c $(SRCDIR)/tip.c $(SRCDIR)/sembol.c \
       $(SRCDIR)/tip_kontrol.c $(SRCDIR)/tekkez_kontrol.c \
       $(SRCDIR)/bolge.c $(SRCDIR)/bolge_atama.c \
       $(SRCDIR)/llvm.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean test calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_snapshot_test calistir_fuzz_test calistir_bench coverage test_tumu

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

# === Tekkez (linear types) testi (Clang64 + ASan) ===

$(BUILD)/test_tekkez$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                            $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                            $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                            $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                            $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                            $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                            $(SRCDIR)/tekkez_kontrol.c \
                            $(TESTDIR)/test_tekkez.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

calistir_tekkez_test: $(BUILD)/test_tekkez$(EXE)
	./$(BUILD)/test_tekkez$(EXE)

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
                                  $(SRCDIR)/bolge_atama.c \
                                  $(TESTDIR)/test_bolge_atama.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Snapshot testi (Clang64 + ASan — parser tum bagimliliklar) ===

$(BUILD)/test_snapshot$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                              $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                              $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                              $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                              $(SRCDIR)/ifade.c \
                              $(TESTDIR)/test_snapshot.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Fuzz testi (Clang64 + ASan — random input parser) ===

$(BUILD)/test_fuzz$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                          $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                          $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                          $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                          $(SRCDIR)/ifade.c \
                          $(TESTDIR)/test_fuzz.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Bench (UCRT64 GCC, ASan'SIZ — gercek perf — -O2 ile) ===

$(BUILD)/test_bench$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                           $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                           $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                           $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                           $(SRCDIR)/ifade.c $(SRCDIR)/tip.c $(SRCDIR)/sembol.c \
                           $(SRCDIR)/tip_kontrol.c $(SRCDIR)/bolge.c \
                           $(SRCDIR)/bolge_atama.c $(SRCDIR)/llvm.c \
                           $(TESTDIR)/test_bench.c | $(BUILD)
	$(CC) -Wall -Wextra -std=c11 -O2 -I$(SRCDIR) -o $@ $^

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

calistir_snapshot_test: $(BUILD)/test_snapshot$(EXE)
	./$(BUILD)/test_snapshot$(EXE) 2>/dev/null

# Fuzz: KEMGU hata mesajlari stderr'e flood eder, /dev/null'a at.
# ASan return code'u zaten ana kanalda kalir (bash $?).
calistir_fuzz_test: $(BUILD)/test_fuzz$(EXE)
	./$(BUILD)/test_fuzz$(EXE) 2>/dev/null

# Bench test_tumu icinde DEGIL — sadece elle calistirilir
calistir_bench: $(BUILD)/test_bench$(EXE)
	./$(BUILD)/test_bench$(EXE)

test_tumu: calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_tekkez_test calistir_bolge_test calistir_bolge_atama_test calistir_snapshot_test calistir_fuzz_test
	@echo "Tum testler gecti!"

# === Coverage (gcov ile parser/lexer/tip kontrol branch coverage) ===
# GCC --coverage flag'leri ile derler, calistirir, gcov raporu uretir.
# Cikti: build/coverage/<file>.c.gcov

COV_DIR = $(BUILD)/coverage
COV_FLAGS = --coverage -O0 -g
COV_SRCS = $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c $(SRCDIR)/hata.c \
           $(SRCDIR)/lexer.c $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
           $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c $(SRCDIR)/ifade.c \
           $(SRCDIR)/tip.c $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
           $(SRCDIR)/bolge.c $(SRCDIR)/bolge_atama.c

$(COV_DIR)/test_parser_cov$(EXE): $(COV_SRCS) $(TESTDIR)/test_parser.c
	mkdir -p $(COV_DIR)
	$(CC) -Wall -Wextra -std=c11 $(COV_FLAGS) -I$(SRCDIR) -o $@ $^

# UCRT64 gcov GCC --coverage output ile uyumlu (Clang gcov degil)
GCOV = /c/msys64/ucrt64/bin/gcov.exe

coverage: $(COV_DIR)/test_parser_cov$(EXE)
	./$(COV_DIR)/test_parser_cov$(EXE) > /dev/null 2>&1 || true
	@echo "=== Parser/Lexer/Tip coverage (test_parser uzerinden) ==="
	@cd $(COV_DIR) && for f in parser ifade lexer tip_kontrol bolge_atama ast utf8; do \
	    if [ -f "test_parser_cov-$$f.gcda" ]; then \
	        result=$$($(GCOV) -bc test_parser_cov-$$f.gcda 2>/dev/null \
	            | grep -E "Lines executed|Branches executed" \
	            | head -2 | tr '\n' ' '); \
	        printf "  %-15s %s\n" "$$f.c:" "$$result"; \
	    fi; \
	done
	@echo ""
	@echo "Detayli rapor: $(COV_DIR)/*.gcov"

clean:
	rm -rf $(BUILD)

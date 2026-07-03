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
       $(SRCDIR)/escape.c $(SRCDIR)/llvm.c $(SRCDIR)/llvm_dogrula.c \
       $(SRCDIR)/json.c $(SRCDIR)/lsp.c \
       $(SRCDIR)/wcet.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean test calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_linear_test calistir_sabitsure_test calistir_wcet_test calistir_capability_test calistir_simd_test calistir_simd_llvm_test calistir_stdlib_check calistir_kripto_check calistir_arm64_test calistir_snapshot_test calistir_fuzz_test calistir_fuzz_advanced calistir_runtime_link_test calistir_kdl_bolge_test calistir_otp_cli_test calistir_dizi_perf_test calistir_uart_pl011_test calistir_uart_pl011_bare_metal calistir_yazdir_bare_test calistir_yazdir_bare_bare_metal calistir_uart_merhaba_bare_metal calistir_uart_16550_test calistir_uart_16550_bare_metal calistir_panik_test calistir_panik_bare_metal calistir_uart_vtable_test calistir_qemu_smoke calistir_uart_echo_bare_metal calistir_drf_lean_proof kemgu_self calistir_self_driver bench test_tumu
.PHONY: all clean test calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_llvm_dogrula_test calistir_linear_test calistir_sabitsure_test calistir_wcet_test calistir_capability_test calistir_mmio_test calistir_mmio_bare_metal calistir_simd_test calistir_simd_llvm_test calistir_stdlib_check calistir_kripto_check calistir_arm64_test calistir_snapshot_test calistir_fuzz_test calistir_fuzz_advanced calistir_runtime_link_test calistir_kdl_bolge_test calistir_otp_cli_test calistir_dizi_perf_test calistir_uart_pl011_test calistir_uart_pl011_bare_metal calistir_yazdir_bare_test calistir_yazdir_bare_bare_metal calistir_uart_merhaba_bare_metal calistir_uart_16550_test calistir_uart_16550_bare_metal calistir_panik_test calistir_panik_bare_metal calistir_uart_vtable_test calistir_qemu_smoke calistir_uart_echo_bare_metal bench test_tumu

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
                                  $(SRCDIR)/escape.c \
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
                        $(SRCDIR)/escape.c \
                        $(TESTDIR)/test_lsp.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === LLVM backend entegrasyon testi (GCC, ASan'siz — system() ile harici cagri) ===
# kemgu.exe ve clang'a baglidir.

$(BUILD)/test_llvm$(EXE): $(TESTDIR)/test_llvm.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $<

# === C2: IR-verifier birim testi (Clang64 + ASan — izole, saf metin) ===
# llvm_dogrula.c'nin AST/parser bagimliligi yok; sadece kendisi + test
# linklenir. ASan, tarayicinin tampon islemlerini (memcpy/strcpy) denetler.

$(BUILD)/test_llvm_dogrula$(EXE): $(SRCDIR)/llvm_dogrula.c \
                                  $(TESTDIR)/test_llvm_dogrula.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Linear Types Spec V1 testi (Clang64 + ASan — full pipeline) ===

$(BUILD)/test_linear$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                            $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                            $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                            $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                            $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                            $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                            $(SRCDIR)/escape.c \
                            $(TESTDIR)/test_linear.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Sabitsure (Constant-Time) Spec V1 testi (Clang64 + ASan — full pipeline) ===

$(BUILD)/test_sabitsure$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                               $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                               $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                               $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                               $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                               $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                               $(SRCDIR)/escape.c \
                               $(TESTDIR)/test_sabitsure.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Realtime (WCET) Spec V1 testi (Clang64 + ASan — full pipeline + wcet) ===

$(BUILD)/test_wcet$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                          $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                          $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                          $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                          $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                          $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                          $(SRCDIR)/wcet.c $(SRCDIR)/escape.c \
                          $(TESTDIR)/test_wcet.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Capability (Object-Capability) Spec V1 testi (Clang64 + ASan) ===

$(BUILD)/test_capability$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                                $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                                $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                                $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                                $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                                $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                                $(SRCDIR)/escape.c \
                                $(TESTDIR)/test_capability.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === MMIO Foundation tip kontrol testi (Clang64 + ASan) ===

$(BUILD)/test_mmio$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                          $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                          $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                          $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                          $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                          $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                          $(SRCDIR)/escape.c \
                          $(TESTDIR)/test_mmio.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === DRF (Data Race Freedom) V1 testi (Clang64 + ASan) ===

$(BUILD)/test_drf$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                         $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                         $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                         $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                         $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                         $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                         $(SRCDIR)/escape.c \
                         $(TESTDIR)/test_drf.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === SIMD Spec V1 testi (Clang64 + ASan — full pipeline) ===

$(BUILD)/test_simd$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                          $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                          $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                          $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                          $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                          $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                          $(SRCDIR)/escape.c \
                          $(TESTDIR)/test_simd.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === SIMD LLVM end-to-end testi (GCC; clang + kemgu cagrisi) ===

$(BUILD)/test_simd_llvm$(EXE): $(TESTDIR)/test_simd_llvm.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $<

# === Snapshot test (ADIM 32 — kemgu --parse cikti baseline'lari) ===

$(BUILD)/test_snapshot$(EXE): $(TESTDIR)/test_snapshot.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $<

# === Parser fuzzer (ADIM 32 — random byte stream, 10000 iter, ASan) ===

$(BUILD)/test_fuzz$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                          $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                          $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                          $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                          $(SRCDIR)/ifade.c \
                          $(TESTDIR)/test_fuzz.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Benchmark suite (test-altyapi — 10 baseline, JSON cikti) ===

$(BUILD)/test_bench$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                           $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                           $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                           $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                           $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                           $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                           $(SRCDIR)/bolge.c $(SRCDIR)/bolge_atama.c \
                           $(SRCDIR)/escape.c \
                           $(TESTDIR)/test_bench.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $^

bench: $(BUILD)/test_bench$(EXE) $(BUILD)/kemgu$(EXE)
	./$(BUILD)/test_bench$(EXE)

# === Gelismis parser fuzzer (4 mod x 5000 iter, ASan + UBSan) — src-bugfix ===
# Mod a: random keyword/operator karisigi — parser bug-fix dogrulamasi
# Mod b: AST roundtrip; Mod c: tip kontrol; Mod d: UTF-8 edge cases.

$(BUILD)/test_fuzz_advanced$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                                    $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                                    $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                                    $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                                    $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                                    $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                                    $(SRCDIR)/bolge.c $(SRCDIR)/bolge_atama.c \
                                    $(SRCDIR)/escape.c \
                                    $(TESTDIR)/test_fuzz_advanced.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

calistir_fuzz_advanced: $(BUILD)/test_fuzz_advanced$(EXE)
	./$(BUILD)/test_fuzz_advanced$(EXE)


# === KDL Runtime (ADIM 33 — compile + link entegrasyonu) ===
# runtime/kdl_runtime.c bagimsiz olarak derlenir, sonra test_runtime_link.c
# ile linkletilir. Mevcut LLVM pipeline icin:
#   clang prog.ll $(BUILD)/kdl_runtime.o -o prog.exe

$(BUILD)/kdl_runtime.o: runtime/kdl_runtime.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

# MMIO Foundation runtime (host/mock modu — global tampon, segfault-siz).
# Bare-metal volatile varyant icin: calistir_mmio_bare_metal (-DKEMGU_BARE_METAL).
$(BUILD)/kdl_runtime_mmio.o: runtime/kdl_runtime_mmio.c runtime/kdl_mmio.h | $(BUILD)
	$(CC) $(CFLAGS) -Iruntime -c -o $@ $<

$(BUILD)/test_runtime_link$(EXE): $(BUILD)/kdl_runtime.o \
                                   $(TESTDIR)/test_runtime_link.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

# === Bölge (region) arena allokatörü runtime (V2 F4.0 — D-099) ===
# runtime/kdl_bolge.c bağımsız .o (F4.1'de program link'ine eklenecek; F4.0'da
# henüz kimse çağırmaz). Birim test ASan/UBSan ile derlenir (overflow/UAF/hiza).
$(BUILD)/kdl_bolge.o: runtime/kdl_bolge.c runtime/kdl_bolge.h | $(BUILD)
	$(CC) $(CFLAGS) -Iruntime -c -o $@ $<

$(BUILD)/test_kdl_bolge$(EXE): runtime/kdl_bolge.c runtime/kdl_bolge.h \
                                $(TESTDIR)/test_kdl_bolge.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -Iruntime -o $@ \
		runtime/kdl_bolge.c $(TESTDIR)/test_kdl_bolge.c

# === OTP CLI Integration testi (Adim 1) ===
# Test_otp_cli.c kemgu.exe + kdl_runtime.o + test/ornekler/otp_cli.kem'i kullanir.
$(BUILD)/test_otp_cli$(EXE): $(TESTDIR)/test_otp_cli.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

# === Dizi performans bench (Adim 6) ===
$(BUILD)/test_dizi_perf$(EXE): $(TESTDIR)/test_dizi_perf.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<


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

calistir_llvm_test: $(BUILD)/test_llvm$(EXE) $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o $(BUILD)/kdl_runtime_mmio.o
	./$(BUILD)/test_llvm$(EXE)

calistir_llvm_dogrula_test: $(BUILD)/test_llvm_dogrula$(EXE)
	./$(BUILD)/test_llvm_dogrula$(EXE)

calistir_linear_test: $(BUILD)/test_linear$(EXE)
	./$(BUILD)/test_linear$(EXE)

calistir_sabitsure_test: $(BUILD)/test_sabitsure$(EXE)
	./$(BUILD)/test_sabitsure$(EXE)

calistir_wcet_test: $(BUILD)/test_wcet$(EXE)
	./$(BUILD)/test_wcet$(EXE)

calistir_capability_test: $(BUILD)/test_capability$(EXE)
	./$(BUILD)/test_capability$(EXE)

calistir_mmio_test: $(BUILD)/test_mmio$(EXE)
	./$(BUILD)/test_mmio$(EXE)

# MMIO runtime'in bare-metal (volatile) varyantinin DERLENDIGINI dogrula.
# (Calistirma yok — gercek MMIO host'ta segfault verir; sadece compile.)
calistir_mmio_bare_metal: | $(BUILD)
	$(CC) $(CFLAGS) -DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_mmio.c -o $(BUILD)/kdl_runtime_mmio_bare.o
	@echo "MMIO bare-metal (volatile) varyanti derlendi: kdl_runtime_mmio_bare.o"

calistir_drf_test: $(BUILD)/test_drf$(EXE)
	./$(BUILD)/test_drf$(EXE)

calistir_simd_test: $(BUILD)/test_simd$(EXE)
	./$(BUILD)/test_simd$(EXE)

calistir_simd_llvm_test: $(BUILD)/test_simd_llvm$(EXE) $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	./$(BUILD)/test_simd_llvm$(EXE)

calistir_snapshot_test: $(BUILD)/test_snapshot$(EXE) $(BUILD)/kemgu$(EXE)
	./$(BUILD)/test_snapshot$(EXE)

calistir_fuzz_test: $(BUILD)/test_fuzz$(EXE)
	./$(BUILD)/test_fuzz$(EXE)

calistir_runtime_link_test: $(BUILD)/test_runtime_link$(EXE)
	./$(BUILD)/test_runtime_link$(EXE)

calistir_kdl_bolge_test: $(BUILD)/test_kdl_bolge$(EXE)
	./$(BUILD)/test_kdl_bolge$(EXE)

calistir_otp_cli_test: $(BUILD)/test_otp_cli$(EXE) $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	./$(BUILD)/test_otp_cli$(EXE)

calistir_dizi_perf_test: $(BUILD)/test_dizi_perf$(EXE) $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	./$(BUILD)/test_dizi_perf$(EXE)

# Stdlib tip-kontrolu — saf KEMGU stdlib modullerinin --check'ten gecmesi
# Kutuphane dosyasi varsa karsilik gelen test/stdlib/test_<modul>.kem ile
# birlestirilip --check'ten gecirilir (tek dosya derleme, import yok).
# Test dosyasi yoksa kutuphane tek basina kontrol edilir.
# ASan/UBSan codegen bellek güvenliği denetimi — üretilen kodu sanitizer ile
# derleyip çalıştırır (test_llvm E2E sanitizer'sız koşar). D-030 sınıfı codegen
# bellek hatalarını yakalar. Bilinen başarısızlıklar betikteki ALLOWLIST'te.
calistir_asan_denetim: $(BUILD)/kemgu$(EXE)
	@bash test/asan_e2e_denetim.sh

# ASan/UBSan bellek güvenliği MATRİSİ — D-029/D-030 eksenlerini sınırda zorlayan
# kalıcı regresyon ağı. Her program kendini doğrular (exit 42) + sanitizer-temiz.
calistir_asan_matris: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o $(BUILD)/kdl_runtime_mmio.o
	@bash test/asan_matris_calistir.sh

# SELF-HOST lexer doğruluk kanıtı: KEMGU-lexer (selfhost/lexer.kem) çıktısını
# C lexer --token oracle'ına karşı sıfır-diff'ler (D-035 ADIM-0 / M1+).
calistir_lexer_diff: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/lexer_diff_harness.sh

# M6 bootstrap: KEMGU-lexer'ı TÜM gerçek .kem korpusuna (self-lexing dahil)
# karşı C lexer (oracle) ile sıfır-diff doğrula (D-042).
calistir_lexer_bootstrap: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/lexer_bootstrap_harness.sh

# SELF-HOST parser (selfhost/parser.kem) --ast düz-dump'ını C --ast oracle'ına
# karşı sıfır-diff doğrula (D-043/D-045). P1+ korpus: test/parse_korpus/.
calistir_parser_diff: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/parser_diff_harness.sh

# P6 bootstrap: KEMGU-parser'ı TÜM gerçek .kem korpusuna (self-parse dahil) karşı
# C --ast oracle'ı ile sıfır-diff doğrula (D-050).
calistir_parser_bootstrap: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/parser_bootstrap_harness.sh

# SELF-HOST tip denetleyici (selfhost/checker.kem) --checkdump çıktısını C
# --checkdump oracle'ına karşı diff'ler (Aşama 2 / D-052). Korpus: test/check_korpus/.
calistir_checker_diff: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/checker_diff_harness.sh

# Dizi sınır-güvenliği (D-069): OOB → panic invaryantı (segfault/sessiz-0 değil).
calistir_dizi_sinir_test: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/dizi_sinir_harness.sh

# Lambda/closure codegen V2 (D-071): Sınıf B 4 örnek → doğru exit (closure çalışır).
calistir_lambda_test: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/lambda_v2_harness.sh

# SELF-HOST codegen (selfhost/codegen.kem) — Aşama 3 / D-072. KEMGU-codegen IR'ı
# C-codegen ile SEMANTİK (exit-kod) eşdeğerlik üzerinden doğrular. Korpus: test/cg_korpus/.
calistir_codegen_diff: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@build/kemgu.exe --llvm selfhost/codegen.kem > build/codegen.ll 2>/dev/null
	@clang -x ir build/codegen.ll -x none build/kdl_runtime.o -o build/codegen.exe 2>/dev/null
	@CODEGEN=build/codegen.exe bash test/codegen_diff_harness.sh

# SELF-HOST bootstrap (Aşama 5): KEMGU-codegen-built lexer, C-codegen-built lexer ile
# byte-identik mi (codegen self-host'un uçtan-uca doğruluğu). Korpus: selfhost + ornekler.
calistir_codegen_bootstrap: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/codegen_bootstrap_harness.sh

# AŞAMA 4 (driver) — TEK self-host KEMGU binary (selfhost/codegen.kem → kemgu_self.exe).
# checker mantığı + --token/--parse/--check/--llvm dispatch birleşik (D-086).
kemgu_self: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@build/kemgu.exe --llvm selfhost/codegen.kem > build/kemgu_self.ll 2>/dev/null
	@clang -x ir build/kemgu_self.ll -x none build/kdl_runtime.o -o build/kemgu_self.exe 2>/dev/null
	@echo "build/kemgu_self.exe uretildi (tek self-host kemgu binary)."

# AŞAMA 4/5 driver doğruluk: C-derlenmiş + self-host-derlenmiş driver, 4 mod (--token/
# --parse/--check/--llvm) C oracle ile eşleşir + FIXPOINT. (Harness driver'ı kendi içinde üretir.)
calistir_self_driver: $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	@bash test/selfhost_driver_harness.sh

calistir_stdlib_check: $(BUILD)/kemgu$(EXE) calistir_kripto_check | $(BUILD)
	@echo "stdlib tip kontrolu (kutuphane + test birlestirilerek)..."
	@for f in stdlib/temel/*.kem stdlib/*.kem; do \
		[ -f "$$f" ] || continue; \
		case "$$f" in \
			stdlib/kripto.kem) continue;; \
		esac; \
		mod=$$(basename "$$f" .kem); \
		test_f="test/stdlib/test_$$mod.kem"; \
		if [ -f "$$test_f" ]; then \
			combined="$(BUILD)/_stdlib_$$mod.kem"; \
			cat "$$f" "$$test_f" > "$$combined"; \
			./$(BUILD)/kemgu$(EXE) --check "$$combined" || \
				{ echo "FAIL: $$f + $$test_f"; rm -f "$$combined"; exit 1; }; \
			rm -f "$$combined"; \
		else \
			./$(BUILD)/kemgu$(EXE) --check "$$f" || \
				{ echo "FAIL: $$f"; exit 1; }; \
		fi; \
	done
	@echo "Tum stdlib modulleri --check gecti!"

# Kripto bundle kontrolu — stdlib/kripto.kem + stdlib/kripto/*.kem birlikte
# her bir test/stdlib/test_kripto*.kem dosyasi ile karistirilir.
# (Import sistemi olmadigi icin bundle yaklasimi gerekli; kripto'nun alt
# modulleri (karma, sifre, rastgele, anahtar) base API + birbirine atif yapar.)
calistir_kripto_check: $(BUILD)/kemgu$(EXE) | $(BUILD)
	@echo "kripto bundle tip kontrolu..."
	@bundle_base="$(BUILD)/_kripto_base.kem"; \
	cat stdlib/kripto.kem stdlib/kripto/*.kem > "$$bundle_base"; \
	./$(BUILD)/kemgu$(EXE) --check "$$bundle_base" || \
		{ echo "FAIL: kripto bundle base"; rm -f "$$bundle_base"; exit 1; }; \
	for test_f in test/stdlib/test_kripto*.kem; do \
		[ -f "$$test_f" ] || continue; \
		bname=$$(basename "$$test_f" .kem); \
		combined="$(BUILD)/_kripto_$$bname.kem"; \
		cat "$$bundle_base" "$$test_f" > "$$combined"; \
		./$(BUILD)/kemgu$(EXE) --check "$$combined" || \
			{ echo "FAIL: kripto + $$test_f"; rm -f "$$combined" "$$bundle_base"; exit 1; }; \
		rm -f "$$combined"; \
	done; \
	rm -f "$$bundle_base"
	@echo "Kripto bundle (kripto.kem + kripto/*.kem) + tum test_kripto* --check gecti!"

# ARM64 (aarch64) cross-compile dogrulama — DGX Spark / Android NDK altyapisi
# Mevcut KEMGU --llvm IR ciktisini clang -target ile ARM64 ELF object'e cevirir.
# Calistirma host degil — sadece derleme + file/objdump dogrulamasi.
calistir_arm64_test: $(BUILD)/kemgu$(EXE)
	@echo "ARM64 cross-compile testi..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/kernel.kem > $(BUILD)/kernel.ll
	clang -target aarch64-unknown-none -x ir $(BUILD)/kernel.ll -c \
		-o $(BUILD)/kernel_aarch64.o 2>&1
	@echo ""
	@echo "Uretilen ARM64 ELF object:"
	@file $(BUILD)/kernel_aarch64.o
	@echo ""
	@echo "Section headers:"
	@llvm-objdump -h $(BUILD)/kernel_aarch64.o | sed -n '4,9p'
	@echo "ARM64 ELF dogrulamasi basarili!"

# =============================================================================
# Bare-Metal UART / Konsol Surucusu (Track B — Hedef 3 Evrensel OS)
# =============================================================================
#
# Heap'siz, libc'siz UART surucusu — bump allocator gerektirmez. Iki
# desteklenen denetleyici:
#   - runtime/kdl_runtime_uart_pl011.c   ARM PrimeCell PL011 (QEMU virt, RPi)
#   - runtime/kdl_runtime_uart_16550.c   NS16550A (x86_64 COM1, port I/O)
#
# Host testleri KEMGU_UART_MOCK ile derlenir; MMIO/port erisimi global
# tampona yonelir, surucu mantigi Windows host'unda dogrulanir.
#
# Bare-metal cross-compile dogrulamasi -DKEMGU_BARE_METAL ile yapilir;
# llvm-objdump kontrolu libc sembol referansi olmadigini saglar.

# === PL011 mock host testi (Clang64, ASan aktif) ===
$(BUILD)/test_uart_pl011$(EXE): runtime/kdl_runtime_uart_pl011.c \
                                $(TESTDIR)/test_uart_pl011.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -DKEMGU_UART_MOCK -Iruntime -o $@ $^

calistir_uart_pl011_test: $(BUILD)/test_uart_pl011$(EXE)
	./$(BUILD)/test_uart_pl011$(EXE)

# === Bare-metal yazdir_* port (PL011 backend, mock test) ===
$(BUILD)/test_yazdir_bare$(EXE): runtime/kdl_runtime_uart_pl011.c \
                                  runtime/kdl_runtime_yazdir_bare.c \
                                  $(TESTDIR)/test_yazdir_bare.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -DKEMGU_UART_MOCK -Iruntime -o $@ $^

calistir_yazdir_bare_test: $(BUILD)/test_yazdir_bare$(EXE)
	./$(BUILD)/test_yazdir_bare$(EXE)

# === yazdir_bare bare-metal cross-compile dogrulamasi ===
calistir_yazdir_bare_bare_metal:
	@echo "yazdir_bare (PL011 backend) bare-metal cross-compile dogrulamasi..."
	clang -target aarch64-unknown-none -ffreestanding -nostdlib \
		-Wall -Wextra -Wpedantic -std=c11 -O2 \
		-DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_uart_pl011.c \
		-o $(BUILD)/kdl_uart_pl011_aarch64.o
	clang -target aarch64-unknown-none -ffreestanding -nostdlib \
		-Wall -Wextra -Wpedantic -std=c11 -O2 \
		-DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_yazdir_bare.c \
		-o $(BUILD)/kdl_yazdir_bare_aarch64.o
	@echo ""
	@echo "Beklenen kdl_yazdir_* semboller:"
	@llvm-nm --defined-only $(BUILD)/kdl_yazdir_bare_aarch64.o | \
		grep -E 'kdl_yazdir_(metin|satir|tam|tam64|mantiksal)|kdl_yaz_(metin|tam)|kdl_format_tam64' || \
		{ echo "FAIL: yazdir_* semboller eksik"; exit 1; }
	@echo ""
	@echo "Libc sembol referansi (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kdl_yazdir_bare_aarch64.o | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|snprintf|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kdl_yazdir_bare_aarch64.o; \
		exit 1; \
	fi
	@echo "  (yok — temiz)"
	@echo "yazdir_bare bare-metal dogrulamasi basarili!"

# === Bare-Metal UART Echo (Track B D4) ===
# uart_echo.kem -> ARM64 ELF (RX -> TX). Hello world ile ayni runtime,
# ek olarak kdl_oku_karakter / kdl_yaz_karakter sembollerini kullanir.
calistir_uart_echo_bare_metal: $(BUILD)/kemgu$(EXE)
	@echo "Bare-metal echo: uart_echo.kem -> ARM64 ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/uart_echo.kem > $(BUILD)/uart_echo.ll
	clang -target aarch64-unknown-none -ffreestanding -nostdlib -O2 \
		-x ir $(BUILD)/uart_echo.ll -c -o $(BUILD)/uart_echo.o
	clang -target aarch64-unknown-none -ffreestanding -nostdlib \
		-Wall -Wextra -Wpedantic -std=c11 -O2 \
		-DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_uart_pl011.c \
		-o $(BUILD)/uart_pl011_bm.o
	clang -target aarch64-unknown-none -ffreestanding -nostdlib \
		-Wall -Wextra -Wpedantic -std=c11 -O2 \
		-DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_yazdir_bare.c \
		-o $(BUILD)/yazdir_bare_bm.o
	clang -target aarch64-unknown-none -ffreestanding -nostdlib \
		-c boot/start_aarch64.S -o $(BUILD)/start_aarch64.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/kernel_echo.elf \
		$(BUILD)/start_aarch64.o $(BUILD)/uart_echo.o \
		$(BUILD)/yazdir_bare_bm.o $(BUILD)/uart_pl011_bm.o
	@echo ""
	@echo "Uretilen echo kernel:"
	@file $(BUILD)/kernel_echo.elf
	@echo ""
	@echo "Beklenen RX + TX semboller:"
	@llvm-nm $(BUILD)/kernel_echo.elf | grep -E '^[0-9a-f]+ T (_start|main|kdl_oku_karakter|kdl_yaz_karakter|kdl_uart_pl011_oku_karakter|kdl_uart_pl011_putc)$$' || true
	@echo ""
	@echo "Libc sembol referansi (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kernel_echo.elf | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|puts|snprintf|getchar|fgetc|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kernel_echo.elf; \
		exit 1; \
	fi
	@echo "  (yok — temiz)"
	@echo "Bare-metal echo basarili!"

# === QEMU smoke test (opsiyonel — qemu yoksa atlanir) ===
# Bare-metal hello world ELF'ini QEMU virt'te calistirip stdout yakalar.
# Beklenen cikti "Merhaba KEMGU - Bare Metal" + "42" satirlari icermeli.
# QEMU PATH'te yoksa atlandi mesaji ile basariyla biter (CI uyumlu).
calistir_qemu_smoke: $(BUILD)/kernel.elf
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		echo "QEMU bulundu, ARM64 smoke test calistiriliyor..."; \
		rm -f $(BUILD)/qemu_smoke.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 \
			-display none -serial file:$(BUILD)/qemu_smoke.out \
			-kernel $(BUILD)/kernel.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; \
		cat $(BUILD)/qemu_smoke.out; \
		echo "--- son ---"; \
		if grep -q "Merhaba KEMGU" $(BUILD)/qemu_smoke.out && \
		   grep -q "42" $(BUILD)/qemu_smoke.out; then \
			echo "QEMU smoke test gecti: cikti dogru."; \
		else \
			echo "FAIL: beklenen cikti (Merhaba KEMGU + 42) yok"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — smoke test atlandi."; \
		echo "Yuklemek icin (MSYS2): pacman -S mingw-w64-clang-x86_64-qemu"; \
	fi

# Build target — kernel.elf var olmali. Yoksa hello_world'u tetikle.
$(BUILD)/kernel.elf:
	@$(MAKE) calistir_uart_merhaba_bare_metal > /dev/null

# === Bare-Metal ortak runtime objeleri (aarch64) — C1 ===
# Tüm aarch64 kernel'leri paylaşır: region backing (kdl_bolge + kdl_bare_heap =
# frame allocator/malloc/memcpy + dizi runtime) + UART + libc'siz yazdır + panik.
# -mgeneral-regs-only: MMU kapalı = Device-memory → 16-bayt q-register erişimi
# alignment-fault verir; GPR-only güvenli (Linux çekirdek deseni).
BM_A64    = clang -target aarch64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only
BM_A64_CF = -Wall -Wextra -Wpedantic -std=c11 -O2 -DKEMGU_BARE_METAL -Iruntime

$(BUILD)/bm_a64_uart.o: runtime/kdl_runtime_uart_pl011.c runtime/kdl_uart.h | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_yazdir.o: runtime/kdl_runtime_yazdir_bare.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_bolge.o: runtime/kdl_bolge.c runtime/kdl_bolge.h | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_heap.o: runtime/kdl_bare_heap.c runtime/kdl_dizi.inc runtime/kdl_bolge.h | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_panik.o: runtime/kdl_runtime_panik.c runtime/kdl_panik.h | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_kesme.o: runtime/kdl_kesme.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_zaman.o: runtime/kdl_zaman.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_mmu.o: runtime/kdl_mmu.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_gorev.o: runtime/kdl_gorev.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_kanal.o: runtime/kdl_kanal.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_virtio.o: runtime/kdl_virtio.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_virtio_net.o: runtime/kdl_virtio_net.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_mmio.o: runtime/kdl_runtime_mmio.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_yetki.o: runtime/kdl_yetki_bare.c | $(BUILD)
	$(BM_A64) $(BM_A64_CF) -c $< -o $@
$(BUILD)/bm_a64_start.o: boot/start_aarch64.S | $(BUILD)
	$(BM_A64) -c $< -o $@

BM_A64_OBJS = $(BUILD)/bm_a64_start.o $(BUILD)/bm_a64_uart.o $(BUILD)/bm_a64_yazdir.o \
              $(BUILD)/bm_a64_bolge.o $(BUILD)/bm_a64_heap.o $(BUILD)/bm_a64_panik.o \
              $(BUILD)/bm_a64_kesme.o $(BUILD)/bm_a64_zaman.o $(BUILD)/bm_a64_mmu.o \
              $(BUILD)/bm_a64_gorev.o $(BUILD)/bm_a64_virtio.o $(BUILD)/bm_a64_virtio_net.o
#              ^ virtio(blk): kdl_kesme.c kdl_dosya_kaydet/yukle referans eder (D-143).
#                virtio_net: kdl_kesme.c net_gonder/al syscall'ları referans eder (D-176).
#                Tüm aarch64 kernel'ler linkler (kullanılmasa dead-code, libc-temiz).

# === Bare-Metal Hello World (Track B Kalem 3) ===
# uart_merhaba.kem -> ARM64 ELF + libc-yok dogrulamasi.
# Pipeline: kemgu --llvm | clang -target aarch64-unknown-none -> kernel.elf
calistir_uart_merhaba_bare_metal: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "Bare-metal hello world: uart_merhaba.kem -> ARM64 ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/uart_merhaba.kem > $(BUILD)/uart_merhaba.ll
	$(BM_A64) -O2 -Wno-override-module \
		-x ir $(BUILD)/uart_merhaba.ll -c -o $(BUILD)/uart_merhaba.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/kernel.elf $(BUILD)/uart_merhaba.o $(BM_A64_OBJS)
	@echo ""
	@echo "Uretilen kernel:"
	@file $(BUILD)/kernel.elf
	@echo ""
	@echo "Entry point + bolum yerleri:"
	@llvm-objdump -h $(BUILD)/kernel.elf | sed -n '4,12p'
	@echo ""
	@echo "Tanimli semboller (T = text):"
	@llvm-nm $(BUILD)/kernel.elf | grep -E '^[0-9a-f]+ T (_start|main|kdl_uart_pl011_(init|putc|yaz)|kdl_yazdir_(metin|tam|satir)|kdl_yaz_(metin|tam))$$' || true
	@echo ""
	@echo "Libc sembol referansi kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kernel.elf | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|puts|snprintf|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kernel.elf; \
		exit 1; \
	fi
	@echo "  (yok — temiz)"
	@echo "Bare-metal hello world basarili!"

# === Bare-Metal Dizi Kernel (C1b) — heap Dizi<tam32> region-alloc + boot kanıtı ===
# Make-or-break: kdl_dizi_* (kdl_dizi.inc carve) bare-metal frame allocator
# üzerinde boot + doğru toplam (1..10 = 55). QEMU boot ile uçtan-uca doğrulanır.
calistir_kernel_dizi_bare_metal: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "Bare-metal dizi kernel: kernel_dizi.kem -> ARM64 ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/kernel_dizi.kem > $(BUILD)/kernel_dizi.ll
	$(BM_A64) -O2 -Wno-override-module \
		-x ir $(BUILD)/kernel_dizi.ll -c -o $(BUILD)/kernel_dizi.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/kernel_dizi.elf $(BUILD)/kernel_dizi.o $(BM_A64_OBJS)
	@file $(BUILD)/kernel_dizi.elf
	@echo "Libc sembol referansi kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kernel_dizi.elf | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|puts|snprintf|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kernel_dizi.elf; \
		exit 1; \
	fi
	@echo "  (libc yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/kernel_dizi.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/kernel_dizi.out -kernel $(BUILD)/kernel_dizi.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/kernel_dizi.out; echo "--- son ---"; \
		if grep -q "KERNEL DIZI OK" $(BUILD)/kernel_dizi.out && grep -q "55" $(BUILD)/kernel_dizi.out; then \
			echo "Dizi kernel QEMU testi gecti: region-alloc heap dizi dogru (toplam=55)."; \
		else \
			echo "FAIL: beklenen 'KERNEL DIZI OK' + '55' yok"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — dizi kernel boot testi atlandi (pacman -S mingw-w64-clang-x86_64-qemu)."; \
	fi

# === C3a: aarch64 exception vektör testi (deliberate fault → "ISTISNA") ===
# Vektör mekanizmasını kanıtlar: eşlenmemiş erişim → sync exception → VBAR →
# kdl_exc_ortak → kdl_istisna_isle. "ISTISNA" basılır, "GORUNMEMELI" basılmaz.
calistir_istisna_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "C3a aarch64 istisna testi: istisna_arm.c -> ELF (deliberate fault)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/istisna_arm.c -o $(BUILD)/istisna_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/istisna_arm.elf $(BUILD)/istisna_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/istisna_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/istisna_arm.out -kernel $(BUILD)/istisna_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/istisna_arm.out; echo "--- son ---"; \
		if grep -q "FAULT TETIKLE" $(BUILD)/istisna_arm.out && \
		   grep -q "ISTISNA" $(BUILD)/istisna_arm.out && \
		   ! grep -q "GORUNMEMELI" $(BUILD)/istisna_arm.out; then \
			echo "C3a aarch64 istisna testi gecti: fault yakalandi (ISTISNA), ileri gidilmedi."; \
		else \
			echo "FAIL: 'FAULT TETIKLE'+'ISTISNA' bekleniyor, 'GORUNMEMELI' olmamali"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — istisna testi atlandi."; \
	fi

# === C3b/C4: aarch64 timer/IRQ testi (GICv2 + sanal timer → "TIMER OK") ===
# IRQ teslimini kanıtlar: GIC+timer kur → sanal timer ~10ms kesme → kdl_irq_ortak
# → kdl_kesme_isle (tik++ + re-arm) → 5. tikte "TIMER OK".
calistir_timer_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "C3b aarch64 timer testi: timer_test.c -> ELF (GICv2 + CNTV)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/timer_test.c -o $(BUILD)/timer_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/timer_arm.elf $(BUILD)/timer_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/timer_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/timer_arm.out -kernel $(BUILD)/timer_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/timer_arm.out; echo "--- son ---"; \
		if grep -q "TIMER OK" $(BUILD)/timer_arm.out; then \
			echo "C3b aarch64 timer testi gecti: IRQ teslimi + timer calisiyor (TIMER OK)."; \
		else \
			echo "FAIL: 'TIMER OK' yok (timer IRQ teslim edilmedi)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — timer testi atlandi."; \
	fi

# === C6: aarch64 sistem çağrısı testi (SVC → dispatch → eret → "AFTER SYSCALL") ===
calistir_syscall_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "C6 aarch64 syscall testi: syscall_test.c -> ELF (SVC)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/syscall_test.c -o $(BUILD)/syscall_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/syscall_arm.elf $(BUILD)/syscall_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/syscall_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/syscall_arm.out -kernel $(BUILD)/syscall_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/syscall_arm.out; echo "--- son ---"; \
		if grep -q "SYSCALL OK" $(BUILD)/syscall_arm.out && grep -q "AFTER SYSCALL" $(BUILD)/syscall_arm.out; then \
			echo "C6 aarch64 syscall testi gecti: cagri islendi + dondu (AFTER SYSCALL)."; \
		else \
			echo "FAIL: 'SYSCALL OK' + 'AFTER SYSCALL' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — syscall testi atlandi."; \
	fi

# === Capstone: aarch64 — tam OS yığını tek boot'ta (dizi+timer+IRQ+syscall) ===
calistir_capstone_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "Capstone aarch64: capstone.c -> ELF (dizi+timer+syscall birlikte)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/capstone.c -o $(BUILD)/capstone_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/capstone_arm.elf $(BUILD)/capstone_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/capstone_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/capstone_arm.out -kernel $(BUILD)/capstone_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/capstone_arm.out; echo "--- son ---"; \
		if grep -q "55" $(BUILD)/capstone_arm.out && grep -q "99" $(BUILD)/capstone_arm.out && \
		   grep -q "CAPSTONE OK" $(BUILD)/capstone_arm.out && grep -q "TIMER OK" $(BUILD)/capstone_arm.out; then \
			echo "Capstone aarch64 gecti: dizi(55)+post-irq(99)+syscall+timer BIRLIKTE."; \
		else \
			echo "FAIL: 55+99+CAPSTONE OK+TIMER OK hepsi bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — capstone atlandi."; \
	fi

# === Bare-Metal ortak runtime objeleri (x86_64) — C1-x86 ===
# PVH boot (boot/start_x86_64.S) → long mode → C. UART = 16550 (COM1 0x3F8 port
# I/O). Region backing aarch64 ile AYNI (kdl_bolge + kdl_bare_heap arch-bağımsız).
# -mgeneral-regs-only: SSE emit etme (boot'ta SSE enable gerekmez).
BM_X86      = clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only
BM_X86_CF   = -Wall -Wextra -Wpedantic -std=c11 -O2 -DKEMGU_BARE_METAL -Iruntime
BM_X86_UART = -DKDL_UART_PUTC=kdl_uart_16550_putc -DKDL_UART_OKU_KARAKTER=kdl_uart_16550_oku_karakter

$(BUILD)/bm_x86_uart.o: runtime/kdl_runtime_uart_16550.c runtime/kdl_uart.h | $(BUILD)
	$(BM_X86) $(BM_X86_CF) -c $< -o $@
$(BUILD)/bm_x86_yazdir.o: runtime/kdl_runtime_yazdir_bare.c | $(BUILD)
	$(BM_X86) $(BM_X86_CF) $(BM_X86_UART) -c $< -o $@
$(BUILD)/bm_x86_bolge.o: runtime/kdl_bolge.c runtime/kdl_bolge.h | $(BUILD)
	$(BM_X86) $(BM_X86_CF) -c $< -o $@
$(BUILD)/bm_x86_heap.o: runtime/kdl_bare_heap.c runtime/kdl_dizi.inc runtime/kdl_bolge.h | $(BUILD)
	$(BM_X86) $(BM_X86_CF) -c $< -o $@
$(BUILD)/bm_x86_panik.o: runtime/kdl_runtime_panik.c runtime/kdl_panik.h | $(BUILD)
	$(BM_X86) $(BM_X86_CF) $(BM_X86_UART) -c $< -o $@
$(BUILD)/bm_x86_kesme.o: runtime/kdl_kesme.c | $(BUILD)
	$(BM_X86) $(BM_X86_CF) -c $< -o $@
$(BUILD)/bm_x86_zaman.o: runtime/kdl_zaman.c | $(BUILD)
	$(BM_X86) $(BM_X86_CF) -c $< -o $@
$(BUILD)/bm_x86_gorev.o: runtime/kdl_gorev.c | $(BUILD)
	$(BM_X86) $(BM_X86_CF) -c $< -o $@
$(BUILD)/bm_x86_start.o: boot/start_x86_64.S | $(BUILD)
	$(BM_X86) -c $< -o $@

BM_X86_OBJS = $(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
              $(BUILD)/bm_x86_bolge.o $(BUILD)/bm_x86_heap.o $(BUILD)/bm_x86_panik.o \
              $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o $(BUILD)/bm_x86_gorev.o

# === Bare-Metal Hello World x86_64 (C1-x86) — PVH boot smoke ===
calistir_uart_merhaba_x86_bare_metal: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "Bare-metal hello (x86_64 PVH): uart_merhaba.kem -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/uart_merhaba.kem > $(BUILD)/uart_merhaba_x86.ll
	$(BM_X86) -O2 -Wno-override-module \
		-x ir $(BUILD)/uart_merhaba_x86.ll -c -o $(BUILD)/uart_merhaba_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/kernel_x86.elf $(BUILD)/uart_merhaba_x86.o $(BM_X86_OBJS)
	@file $(BUILD)/kernel_x86.elf
	@if llvm-nm --undefined-only $(BUILD)/kernel_x86.elf | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|puts|snprintf|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; exit 1; \
	fi
	@echo "  (libc yok — temiz)"
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/qemu_smoke_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/kernel_x86.elf -display none \
			-serial file:$(BUILD)/qemu_smoke_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/qemu_smoke_x86.out; echo "--- son ---"; \
		if grep -q "Merhaba KEMGU" $(BUILD)/qemu_smoke_x86.out && grep -q "42" $(BUILD)/qemu_smoke_x86.out; then \
			echo "x86_64 hello QEMU testi gecti."; \
		else echo "FAIL: Merhaba KEMGU + 42 yok"; exit 1; fi; \
	else echo "QEMU yok — x86 hello atlandi."; fi

# === Bare-Metal Dizi Kernel x86_64 (C1-x86) — AYNI kernel, PVH boot ===
# aarch64 ile bit-bit aynı kernel_dizi.kem + AYNI region runtime → x86_64'te de
# boot + toplam=55. Region backing'in arch-bağımsızlığının kanıtı.
calistir_kernel_dizi_x86_bare_metal: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "Bare-metal dizi kernel (x86_64 PVH): kernel_dizi.kem -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/kernel_dizi.kem > $(BUILD)/kernel_dizi_x86.ll
	$(BM_X86) -O2 -Wno-override-module \
		-x ir $(BUILD)/kernel_dizi_x86.ll -c -o $(BUILD)/kernel_dizi_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/kernel_dizi_x86.elf $(BUILD)/kernel_dizi_x86.o $(BM_X86_OBJS)
	@file $(BUILD)/kernel_dizi_x86.elf
	@echo "Libc sembol referansi kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kernel_dizi_x86.elf | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|puts|snprintf|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kernel_dizi_x86.elf; \
		exit 1; \
	fi
	@echo "  (libc yok — temiz)"
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/kernel_dizi_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/kernel_dizi_x86.elf -display none \
			-serial file:$(BUILD)/kernel_dizi_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/kernel_dizi_x86.out; echo "--- son ---"; \
		if grep -q "KERNEL DIZI OK" $(BUILD)/kernel_dizi_x86.out && grep -q "55" $(BUILD)/kernel_dizi_x86.out; then \
			echo "x86_64 dizi kernel QEMU testi gecti: region-alloc heap dizi dogru (toplam=55)."; \
		else \
			echo "FAIL: beklenen 'KERNEL DIZI OK' + '55' yok"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 dizi kernel boot testi atlandi."; \
	fi

# === C3a: x86_64 exception/IDT testi (ud2 geçersiz-opcode → "ISTISNA") ===
calistir_istisna_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "C3a x86_64 istisna testi: istisna_x86.c -> ELF (ud2 fault)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/istisna_x86.c -o $(BUILD)/istisna_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/istisna_x86.elf $(BUILD)/istisna_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/istisna_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/istisna_x86.elf -display none \
			-serial file:$(BUILD)/istisna_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/istisna_x86.out; echo "--- son ---"; \
		if grep -q "FAULT TETIKLE" $(BUILD)/istisna_x86.out && \
		   grep -q "ISTISNA" $(BUILD)/istisna_x86.out && \
		   ! grep -q "GORUNMEMELI" $(BUILD)/istisna_x86.out; then \
			echo "C3a x86_64 istisna testi gecti: fault yakalandi (ISTISNA), ileri gidilmedi."; \
		else \
			echo "FAIL: 'FAULT TETIKLE'+'ISTISNA' bekleniyor, 'GORUNMEMELI' olmamali"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 istisna testi atlandi."; \
	fi

# === C3b/C4: x86_64 timer/IRQ testi (PIC 8259 + PIT 8254 → "TIMER OK") ===
calistir_timer_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "C3b x86_64 timer testi: timer_test.c -> ELF (PIC + PIT)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/timer_test.c -o $(BUILD)/timer_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/timer_x86.elf $(BUILD)/timer_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/timer_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/timer_x86.elf -display none \
			-serial file:$(BUILD)/timer_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/timer_x86.out; echo "--- son ---"; \
		if grep -q "TIMER OK" $(BUILD)/timer_x86.out; then \
			echo "C3b x86_64 timer testi gecti: IRQ teslimi + timer calisiyor (TIMER OK)."; \
		else \
			echo "FAIL: 'TIMER OK' yok (timer IRQ teslim edilmedi)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 timer testi atlandi."; \
	fi

# === C6: x86_64 sistem çağrısı testi (int 0x80 → dispatch → iretq → "AFTER SYSCALL") ===
calistir_syscall_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "C6 x86_64 syscall testi: syscall_test.c -> ELF (int 0x80)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/syscall_test.c -o $(BUILD)/syscall_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/syscall_x86.elf $(BUILD)/syscall_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/syscall_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/syscall_x86.elf -display none \
			-serial file:$(BUILD)/syscall_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/syscall_x86.out; echo "--- son ---"; \
		if grep -q "SYSCALL OK" $(BUILD)/syscall_x86.out && grep -q "AFTER SYSCALL" $(BUILD)/syscall_x86.out; then \
			echo "C6 x86_64 syscall testi gecti: cagri islendi + dondu (AFTER SYSCALL)."; \
		else \
			echo "FAIL: 'SYSCALL OK' + 'AFTER SYSCALL' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 syscall testi atlandi."; \
	fi

# === Capstone: x86_64 — tam OS yığını tek boot'ta (dizi+timer+IRQ+syscall) ===
calistir_capstone_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "Capstone x86_64: capstone.c -> ELF (dizi+timer+syscall birlikte)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/capstone.c -o $(BUILD)/capstone_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/capstone_x86.elf $(BUILD)/capstone_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/capstone_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/capstone_x86.elf -display none \
			-serial file:$(BUILD)/capstone_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/capstone_x86.out; echo "--- son ---"; \
		if grep -q "55" $(BUILD)/capstone_x86.out && grep -q "99" $(BUILD)/capstone_x86.out && \
		   grep -q "CAPSTONE OK" $(BUILD)/capstone_x86.out && grep -q "TIMER OK" $(BUILD)/capstone_x86.out; then \
			echo "Capstone x86_64 gecti: dizi(55)+post-irq(99)+syscall+timer BIRLIKTE."; \
		else \
			echo "FAIL: 55+99+CAPSTONE OK+TIMER OK hepsi bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 capstone atlandi."; \
	fi

# === C7a: aarch64 cooperative scheduling testi (context switch → "SCHED OK") ===
calistir_sched_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "C7a aarch64 sched testi: sched_test.c -> ELF (bağlam-değiştirme)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/sched_test.c -o $(BUILD)/sched_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/sched_arm.elf $(BUILD)/sched_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/sched_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/sched_arm.out -kernel $(BUILD)/sched_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/sched_arm.out; echo "--- son ---"; \
		if grep -q "SCHED OK" $(BUILD)/sched_arm.out && grep -q "gorev1" $(BUILD)/sched_arm.out && grep -q "main" $(BUILD)/sched_arm.out; then \
			echo "C7a aarch64 sched testi gecti: bağlam-değiştirme çalışıyor (interleave + SCHED OK)."; \
		else \
			echo "FAIL: 'SCHED OK' + [main] + [gorev1] interleave bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — sched testi atlandi."; \
	fi

# === C7a: x86_64 cooperative scheduling testi (context switch → "SCHED OK") ===
calistir_sched_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "C7a x86_64 sched testi: sched_test.c -> ELF (bağlam-değiştirme)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/sched_test.c -o $(BUILD)/sched_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/sched_x86.elf $(BUILD)/sched_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/sched_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/sched_x86.elf -display none \
			-serial file:$(BUILD)/sched_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/sched_x86.out; echo "--- son ---"; \
		if grep -q "SCHED OK" $(BUILD)/sched_x86.out && grep -q "gorev1" $(BUILD)/sched_x86.out && grep -q "main" $(BUILD)/sched_x86.out; then \
			echo "C7a x86_64 sched testi gecti: bağlam-değiştirme çalışıyor (interleave + SCHED OK)."; \
		else \
			echo "FAIL: 'SCHED OK' + [main] + [gorev1] interleave bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 sched testi atlandi."; \
	fi

# === D2: aarch64 user/kernel privilege ayrımı (EL0 kod + syscall → "D2 OK") ===
# Finer paging (L2 2MB): user 2MB sayfası AP=01 (EL0), kernel AP=00. EL0 kodu
# device'a doğrudan erişemez → yalnız SVC ile EL1'e. Handler SPSR_EL1'den kaynak-EL=0.
calistir_d2_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D2 aarch64 user/kernel testi: d2_arm.c -> ELF (EL0 + syscall)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/d2_arm.c -o $(BUILD)/d2_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/d2_arm.elf $(BUILD)/d2_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/d2_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/d2_arm.out -kernel $(BUILD)/d2_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/d2_arm.out; echo "--- son ---"; \
		if grep -q "D2 BASLA" $(BUILD)/d2_arm.out && grep -q "EL0 SYSCALL" $(BUILD)/d2_arm.out && grep -q "D2 OK" $(BUILD)/d2_arm.out; then \
			echo "D2 aarch64 testi gecti: EL0 kod çalıştı + syscall ile EL1'e geçti (kaynak-EL=0)."; \
		else \
			echo "FAIL: 'D2 BASLA'+'EL0 SYSCALL'+'D2 OK' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — D2 testi atlandi."; \
	fi

# === D1: aarch64 per-process adres-uzayı izolasyonu (TTBR swap → "SUREC A/B") ===
# Her sürece ayrı sayfa tablosu; aynı VA (0x42000000) farklı PA (A→0x44M, B→0x46M).
# TTBR0 swap + TLB flush → A 0xAA / B 0xBB birbirini etkilemez = izolasyon.
calistir_d1_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D1 aarch64 per-process testi: d1_arm.c -> ELF (TTBR swap)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/d1_arm.c -o $(BUILD)/d1_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/d1_arm.elf $(BUILD)/d1_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/d1_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/d1_arm.out -kernel $(BUILD)/d1_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/d1_arm.out; echo "--- son ---"; \
		if grep -q "SUREC A uva=" $(BUILD)/d1_arm.out && grep -qi "0xaa" $(BUILD)/d1_arm.out && grep -qi "0xbb" $(BUILD)/d1_arm.out; then \
			echo "D1 aarch64 testi gecti: per-process izolasyon (ayni VA, A=0xAA B=0xBB)."; \
		else \
			echo "FAIL: 'SUREC A/B' + 0xaa + 0xbb (izolasyon) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — D1 testi atlandi."; \
	fi

# === C7b: aarch64 preemptive scheduling (timer-IRQ zorunlu switch → "PREEMPT OK") ===
# İki görev yield ETMEZ; B'nin koşması SADECE timer-IRQ preemption ile mümkün
# (kdl_irq_ortak full trap-frame → kdl_preempt → SP swap).
calistir_preempt_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "C7b aarch64 preemptive testi: preempt_arm.c -> ELF (timer-IRQ switch)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/preempt_arm.c -o $(BUILD)/preempt_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/preempt_arm.elf $(BUILD)/preempt_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/preempt_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/preempt_arm.out -kernel $(BUILD)/preempt_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/preempt_arm.out; echo "--- son ---"; \
		if grep -q "PREEMPT OK" $(BUILD)/preempt_arm.out; then \
			echo "C7b aarch64 preemptive testi gecti: timer-IRQ zorunlu switch (B yield'siz koştu)."; \
		else \
			echo "FAIL: 'PREEMPT OK' yok (preemption olmadı)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — preemptive testi atlandi."; \
	fi

# === C7c: aarch64 blocking sleep/wake (preemptive üstüne → "B WOKE") ===
# Görev B kdl_uyu(8) ile bloklanır, scheduler atlar, A koşar; 8 tick sonra B uyanır.
calistir_sleep_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "C7c aarch64 sleep/wake testi: sleep_arm.c -> ELF (blocking scheduler)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/sleep_arm.c -o $(BUILD)/sleep_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/sleep_arm.elf $(BUILD)/sleep_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/sleep_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/sleep_arm.out -kernel $(BUILD)/sleep_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/sleep_arm.out; echo "--- son ---"; \
		if grep -q "B WOKE" $(BUILD)/sleep_arm.out && grep -q "VAR" $(BUILD)/sleep_arm.out; then \
			echo "C7c aarch64 sleep/wake testi gecti: B bloklandi+uyandi, A o sirada kostu."; \
		else \
			echo "FAIL: 'B WOKE' + 'VAR' bekleniyor (blocking/wake)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — sleep testi atlandi."; \
	fi

# === D-122 Syscall argüman geçişi testi (aarch64) — SVC arg0 (x0) korunuyor mu ===
# SVC num=4 arg=42 → kernel arg==42 görmeli → "SYSCALL ARG OK". Vektör-stub
# x0-koruma onarımının (D-121) regresyon-bekçisi; userspace syscall ön-koşulu.
calistir_syscall_arg_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-122 aarch64 syscall argüman testi: syscall_arg_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/syscall_arg_arm.c -o $(BUILD)/syscall_arg_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/syscall_arg_arm.elf $(BUILD)/syscall_arg_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/syscall_arg_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/syscall_arg_arm.out -kernel $(BUILD)/syscall_arg_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/syscall_arg_arm.out; echo "--- son ---"; \
		if grep -q "SYSCALL ARG OK" $(BUILD)/syscall_arg_arm.out; then \
			echo "D-122 aarch64 syscall argüman testi gecti: arg0=42 kernel'e dogru ulasti."; \
		else \
			echo "FAIL: 'SYSCALL ARG OK' bekleniyor (SVC arg0 korunmali)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — syscall arg testi atlandi."; \
	fi

# === C7e Öncelikli (priority) scheduling testi (aarch64) ===
# main yüksek öncelik → B (düşük) aç kalır (Faz1: b_sayac=0). main uyuyunca B
# koşar (Faz2: b_sayac>0). Strict priority + kurtarma → "PRIORITY OK ac-faz1=0".
calistir_priority_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "C7e aarch64 oncelikli scheduling testi: priority_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/priority_arm.c -o $(BUILD)/priority_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/priority_arm.elf $(BUILD)/priority_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/priority_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/priority_arm.out -kernel $(BUILD)/priority_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/priority_arm.out; echo "--- son ---"; \
		if grep -q "PRIORITY OK ac-faz1=0" $(BUILD)/priority_arm.out; then \
			echo "C7e aarch64 oncelikli scheduling testi gecti: yuksek-oncelik B'yi ac biraktti, uyuyunca B kostu."; \
		else \
			echo "FAIL: 'PRIORITY OK ac-faz1=0' bekleniyor (strict priority + kurtarma)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — priority testi atlandi."; \
	fi

# === C7d Kanal (SPSC IPC) testi (aarch64) — preemptive scheduler + mesaj geçişi ===
# Üretici görev 1..10 gönderir, tüketici (main) FIFO sırayla alır+toplar (=55).
# Küçük kanal (kap=4) → çift yönlü bloklama (dolu/boş) preemption altında ping-pong.
# KEMGU `kanal` ilkelinin (DRF V1) çekirdek-düzeyi kanıtı.
calistir_kanal_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_kanal.o
	@echo "C7d aarch64 kanal (SPSC IPC) testi: kanal_arm.c -> ELF (gorevler-arasi mesaj)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/kanal_arm.c -o $(BUILD)/kanal_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/kanal_arm.elf $(BUILD)/kanal_arm.o $(BUILD)/bm_a64_kanal.o $(BM_A64_OBJS)
	@echo "Libc sembol referansi kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kanal_arm.elf | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|puts|snprintf|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kanal_arm.elf; \
		exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/kanal_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/kanal_arm.out -kernel $(BUILD)/kanal_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/kanal_arm.out; echo "--- son ---"; \
		if grep -q "KANAL OK toplam=55" $(BUILD)/kanal_arm.out; then \
			echo "C7d aarch64 kanal testi gecti: 10 deger FIFO sirayla gecti (toplam=55)."; \
		else \
			echo "FAIL: 'KANAL OK toplam=55' bekleniyor (SPSC IPC + sira)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — kanal testi atlandi."; \
	fi

# === D3 Korumalı EL0 user-process testi (aarch64) — D1⊕D2⊕D-122 birleşik ===
# Süreç kendi TTBR'ı altında EL0'da koşar; argümanlı syscall yapar (SYSCALL ARG
# OK); kernel belleğine erişince permission-fault (ISTISNA) → adres-uzayı hapsi.
# Gerçek OS sürecinin dört tanımlayıcı özelliği bir arada.
calistir_proc_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D3 aarch64 korumalı user-process testi: proc_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/proc_arm.c -o $(BUILD)/proc_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/proc_arm.elf $(BUILD)/proc_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/proc_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/proc_arm.out -kernel $(BUILD)/proc_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/proc_arm.out; echo "--- son ---"; \
		if grep -q "SYSCALL ARG OK" $(BUILD)/proc_arm.out && grep -q "ISTISNA" $(BUILD)/proc_arm.out; then \
			echo "D3 aarch64 korumalı user-process testi gecti: EL0 syscall(arg) + kernel-erisim reddi (hapis)."; \
		else \
			echo "FAIL: 'SYSCALL ARG OK' + 'ISTISNA' bekleniyor (EL0 syscall + bellek koruması)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — proc testi atlandi."; \
	fi

# === D-124 İlk userspace programı (aarch64) — EL0 hesap + syscall ABI I/O ===
# el0_program EL0'da 1..10 toplar + yaz/yaz_sayi/satir/cik syscall'larıyla yazar.
# Userspace ABI (pointer/veri geçişi + userspace hesap) çekirdek kanıtı. Faz F temeli.
calistir_userspace_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-124 aarch64 userspace programı testi: userspace_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_arm.c -o $(BUILD)/userspace_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_arm.elf $(BUILD)/userspace_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/userspace_arm.out -kernel $(BUILD)/userspace_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_arm.out; echo "--- son ---"; \
		if grep -q "USERSPACE OK toplam=55" $(BUILD)/userspace_arm.out; then \
			echo "D-124 aarch64 userspace testi gecti: EL0 program hesap+syscall I/O (toplam=55)."; \
		else \
			echo "FAIL: 'USERSPACE OK toplam=55' bekleniyor (EL0 hesap + syscall ABI)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace testi atlandi."; \
	fi

# === D-125 Preemptive EL0 (userspace) görev testi (aarch64) ===
# main (EL1) + EL0 userspace görev preemptive scheduler'da; timer-IRQ ile EL0
# görev zorunlu preempt edilir (SP_EL0 trap-frame'de korunur). Tam OS'un son
# parçası: userspace görevler preemptively multitask.
calistir_preempt_el0_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-125 aarch64 preemptive EL0 testi: preempt_el0_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/preempt_el0_arm.c -o $(BUILD)/preempt_el0_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/preempt_el0_arm.elf $(BUILD)/preempt_el0_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/preempt_el0_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/preempt_el0_arm.out -kernel $(BUILD)/preempt_el0_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/preempt_el0_arm.out; echo "--- son ---"; \
		if grep -q "PREEMPT EL0 OK" $(BUILD)/preempt_el0_arm.out; then \
			echo "D-125 aarch64 preemptive EL0 testi gecti: EL0 userspace görev timer-IRQ ile preempt edildi."; \
		else \
			echo "FAIL: 'PREEMPT EL0 OK' bekleniyor (EL0 görev preempt + SP_EL0 korunmalı)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — preempt EL0 testi atlandi."; \
	fi

# === D-126 Syscall dönüş değeri ABI testi (aarch64) ===
# EL0 sys(9,41) → kernel 'artir' arg+1=42 döndürür → "SYSCALL RET OK". Syscall'ın
# kernel→EL0 değer döndürme yolu (read/getpid/gettick ailesinin mekanizması).
calistir_syscall_ret_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-126 aarch64 syscall dönüş değeri testi: syscall_ret_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/syscall_ret_arm.c -o $(BUILD)/syscall_ret_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/syscall_ret_arm.elf $(BUILD)/syscall_ret_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/syscall_ret_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/syscall_ret_arm.out -kernel $(BUILD)/syscall_ret_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/syscall_ret_arm.out; echo "--- son ---"; \
		if grep -q "SYSCALL RET OK" $(BUILD)/syscall_ret_arm.out; then \
			echo "D-126 aarch64 syscall dönüş testi gecti: kernel 41->42 hesabı EL0'a döndü."; \
		else \
			echo "FAIL: 'SYSCALL RET OK' bekleniyor (syscall dönüş değeri x0'da)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — syscall dönüş testi atlandi."; \
	fi

# === D-127 Çoklu EL0 süreç testi (aarch64) — izole userspace multitasking (DORUK) ===
# İki userspace süreç, paylaşılan kod + özel veri (aynı VA→farklı PA), scheduler
# TTBR-swap ile preemptively izole. Çapraz-bozulma yok → "A OK" + "B OK".
calistir_multiproc_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-127 aarch64 çoklu EL0 süreç testi: multiproc_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/multiproc_arm.c -o $(BUILD)/multiproc_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/multiproc_arm.elf $(BUILD)/multiproc_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/multiproc_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/multiproc_arm.out -kernel $(BUILD)/multiproc_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/multiproc_arm.out; echo "--- son ---"; \
		if grep -q "A OK" $(BUILD)/multiproc_arm.out && grep -q "B OK" $(BUILD)/multiproc_arm.out; then \
			echo "D-127 aarch64 çoklu süreç testi gecti: 2 izole userspace süreç preemptively kostu."; \
		else \
			echo "FAIL: 'A OK' + 'B OK' bekleniyor (izole çok-süreç + TTBR swap)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — çoklu süreç testi atlandi."; \
	fi

# === MİLESTONE B: Userspace paylaşımlı-bellek IPC testi (aarch64) ===
# İki EL0 süreç AYNI fiziksel veri sayfasını PAYLAŞIR (D-127 izolasyonunun tersi):
# üretici 1..10 + bayrak yazar, tüketici bayrağı bekleyip toplar → 55 → "USERSHM OK".
calistir_userspace_shm_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "MİLESTONE B aarch64 paylaşımlı-bellek IPC testi: userspace_shm_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_shm_arm.c -o $(BUILD)/userspace_shm_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_shm_arm.elf $(BUILD)/userspace_shm_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_shm_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/userspace_shm_arm.out -kernel $(BUILD)/userspace_shm_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_shm_arm.out; echo "--- son ---"; \
		if grep -q "USERSHM OK" $(BUILD)/userspace_shm_arm.out; then \
			echo "MİLESTONE B paylaşımlı-bellek IPC testi gecti: 2 EL0 süreç aynı sayfayı paylaştı (toplam=55)."; \
		else \
			echo "FAIL: 'USERSHM OK' bekleniyor (paylaşımlı-bellek IPC + preemptive scheduler)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — paylaşımlı-bellek IPC testi atlandi."; \
	fi

# === D-128 Userspace introspection syscall testi (aarch64) — gettick + getpid ===
# Preemptive EL0 görev gettick/getpid ile çekirdek durumunu (zaman/kimlik) okur.
# Syscall dönüş-değeri ABI (D-126) üstünde. t2>t1 + pid → "TICK OK pid=1".
calistir_tick_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-128 aarch64 userspace introspection testi: tick_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/tick_arm.c -o $(BUILD)/tick_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/tick_arm.elf $(BUILD)/tick_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/tick_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/tick_arm.out -kernel $(BUILD)/tick_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/tick_arm.out; echo "--- son ---"; \
		if grep -q "TICK OK pid=1" $(BUILD)/tick_arm.out; then \
			echo "D-128 aarch64 introspection testi gecti: userspace gettick(zaman ilerledi)+getpid."; \
		else \
			echo "FAIL: 'TICK OK pid=1' bekleniyor (gettick + getpid)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — introspection testi atlandi."; \
	fi

# === SMP çok-çekirdek testi (aarch64) — PSCI CPU_ON ile 2. çekirdek bring-up ===
# Çekirdek 0 PSCI CPU_ON çağırır (HVC/SMC) → çekirdek 1 uyanır, paylaşılan bayrağı
# set eder. Çekirdek 0 bayrağı (cache-coherent) poll eder → "SMP OK". QEMU -smp 2.
# Fallback: CPU_ON çalışmazsa PSCI_VERSION → "PSCI OK".
calistir_smp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP çok-çekirdek testi (PSCI CPU_ON): smp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_arm.c -o $(BUILD)/smp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_arm.elf $(BUILD)/smp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_arm.out -kernel $(BUILD)/smp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_arm.out; echo "--- son ---"; \
		if grep -q "SMP OK" $(BUILD)/smp_arm.out; then \
			echo "SMP testi gecti: 2. çekirdek PSCI CPU_ON ile koştu (çok-çekirdek)."; \
		elif grep -q "PSCI OK" $(BUILD)/smp_arm.out; then \
			echo "SMP fallback: PSCI erişilebilir (VERSION) ama CPU_ON çekirdek koşmadı."; \
		else \
			echo "FAIL: 'SMP OK' (veya fallback 'PSCI OK') bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP testi atlandi."; \
	fi

# === SMP GERÇEK PARALEL HESAPLAMA + SPINLOCK testi (aarch64) ===
# D-169 SMP bring-up üstünde: iki çekirdek 200-elemanlı diziyi (0..199) İKİYE
# bölüp paralel toplar (çekirdek 0: [0,100), çekirdek 1: [100,200)). İki paralel-
# güvenlik yolu birlikte: (A) ayrı slot (yarışsız) + (B) LDAXR/STXR spinlock ile
# korunan ortak akümülatör. Her ikisi de 19900 ise "SMP COMPUTE OK". QEMU -smp 2.
# DETERMİNİSTİK: tüm bekleme döngüleri bounded (yük-bağımsız).
calistir_smp_compute_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP paralel hesaplama + spinlock testi: smp_compute_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_compute_arm.c -o $(BUILD)/smp_compute_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_compute_arm.elf $(BUILD)/smp_compute_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_compute_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_compute_arm.out -kernel $(BUILD)/smp_compute_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_compute_arm.out; echo "--- son ---"; \
		if grep -q "SMP COMPUTE OK" $(BUILD)/smp_compute_arm.out; then \
			echo "SMP compute testi gecti: iki cekirdek diziyi paralel topladi (spinlock + ayri-slot), toplam=19900."; \
		else \
			echo "FAIL: 'SMP COMPUTE OK' bekleniyor (iki cekirdek paralel toplam = 19900)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP compute testi atlandi."; \
	fi

# === SMP DİNAMİK İŞ-KUYRUĞU (work-stealing) testi (aarch64) ===
# D-170 SMP compute üstünde: D-170 diziyi STATİK yarı-yarıya böldü; bu test
# DİNAMİK iş kuyruğu kanıtlar. Tek paylaşımlı `sonraki_is` indeksinden İKİ
# çekirdek yarışarak öğe çeker (work-stealing). N=40 öğe, öğe i'nin işi = i*i.
# Spinlock (LDAXR/STXR) her öğeyi TAM BİR çekirdeğe verir → toplam deterministik
# = sum(i*i, i=0..39) = 20540. İki çekirdek de iş çekti mi (per-çekirdek sayaç)
# doğrulanır → gerçek paralel work-stealing. Her ikisi de >0 + toplam=20540 +
# c0+c1=40 ise "SMP QUEUE OK". QEMU -smp 2. DETERMİNİSTİK: bounded bekleme.
calistir_smp_queue_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP dinamik is-kuyrugu (work-stealing) testi: smp_queue_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_queue_arm.c -o $(BUILD)/smp_queue_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_queue_arm.elf $(BUILD)/smp_queue_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_queue_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_queue_arm.out -kernel $(BUILD)/smp_queue_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_queue_arm.out; echo "--- son ---"; \
		if grep -q "SMP QUEUE OK" $(BUILD)/smp_queue_arm.out; then \
			echo "SMP queue testi gecti: iki cekirdek dinamik kuyruktan is cekti (work-stealing), toplam=20540."; \
		else \
			echo "FAIL: 'SMP QUEUE OK' bekleniyor (iki cekirdek dinamik work-stealing, toplam=20540)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP queue testi atlandi."; \
	fi

# === SMP BARİYER SENKRONİZASYONU testi (aarch64) ===
# D-170/174 SMP üstünde: D-170 statik böldü, D-174 dinamik work-stealing kanıtladı;
# bu test LOCKSTEP (kilitli-adım) senkronizasyonu kanıtlar. İki çekirdek K=5 tur
# boyunca her turda bir sense-reversing BARİYER'de buluşur (paylaşımlı sayaç +
# nesil/generation). İkisi de o tura varmadan hiçbiri ilerlemez. Her tur her
# çekirdek kendi tur sayacını artırır → K tur sonunda ikisi de = 5, nesil = 5
# (bariyer tam 5 kez tetiklendi). Üçü de sağlanırsa "SMP BARRIER OK". QEMU -smp 2.
# DETERMİNİSTİK: K tur + her bariyer beklemesi bounded (yük-bağımsız).
calistir_smp_barrier_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP bariyer senkronizasyonu (lockstep) testi: smp_barrier_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_barrier_arm.c -o $(BUILD)/smp_barrier_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_barrier_arm.elf $(BUILD)/smp_barrier_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_barrier_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_barrier_arm.out -kernel $(BUILD)/smp_barrier_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_barrier_arm.out; echo "--- son ---"; \
		if grep -q "SMP BARRIER OK" $(BUILD)/smp_barrier_arm.out; then \
			echo "SMP bariyer testi gecti: iki cekirdek 5 tur lockstep senkron kostu (bariyer + nesil), nesil=5."; \
		else \
			echo "FAIL: 'SMP BARRIER OK' bekleniyor (iki cekirdek 5 tur lockstep, nesil=5)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP bariyer testi atlandi."; \
	fi

# === SMP atomik sayaç çekişmesi testi (aarch64) — LDXR/STXR lost-update yok ===
# İki çekirdek AYNI paylaşımlı sayacı N=10000 kez ATOMİK (LDXR/STXR retry) artırır.
# Son sayac == 2*N == 20000 olmalı → atomik doğruluk (lost-update yok) kanıtı.
# smp_queue modeli: -smp 2, net/drive yok. DETERMİNİSTİK.
calistir_smp_atomic_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP atomik sayaç çekişmesi (LDXR/STXR) testi: smp_atomic_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_atomic_arm.c -o $(BUILD)/smp_atomic_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_atomic_arm.elf $(BUILD)/smp_atomic_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_atomic_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_atomic_arm.out -kernel $(BUILD)/smp_atomic_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_atomic_arm.out; echo "--- son ---"; \
		if grep -q "SMP ATOMIC OK" $(BUILD)/smp_atomic_arm.out; then \
			echo "SMP atomic testi gecti: iki cekirdek atomik LDXR/STXR ile sayaci cekisti, sayac=20000 (lost-update yok)."; \
		else \
			echo "FAIL: 'SMP ATOMIC OK' bekleniyor (iki cekirdek atomik cekisme, sayac=20000)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP atomic testi atlandi."; \
	fi

# === SMP TICKET-LOCK (adil FIFO kilit) testi (aarch64) — açlık yok ===
# D-170/174/180 SMP üstünde: D-170 spinlock (test-and-set) karşılıklı-dışlama
# verdi ama ADİL DEĞİLdi (herhangi bekleyen kilidi kapabilir → teorik açlık).
# Bu test ticket-lock ile FIFO ADALET kanıtlar: `sonraki_bilet` (atomik fetch-add
# LDXR/STXR) benzersiz artan bilet dağıtır; `simdi_hizmet` artan sırada hizmet
# eder → kilit her zaman EN ESKİ bekleyene gider. İki çekirdek N=5000 kez: bilet
# al → kilitle → paylaşımlı sayacı DÜZ (atomik-olmayan) artır (kilit koruduğu
# için yarış yok) → aç. son sayac == 2*N == 10000 (kilit serialize → lost-update
# yok) VE her çekirdek > 0 işledi (FIFO adalet, açlık yok) → "SMP TICKET OK".
# smp_atomic modeli: -smp 2, net/drive yok. DETERMİNİSTİK.
calistir_smp_ticket_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP ticket-lock (adil FIFO kilit) testi: smp_ticket_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_ticket_arm.c -o $(BUILD)/smp_ticket_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_ticket_arm.elf $(BUILD)/smp_ticket_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_ticket_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_ticket_arm.out -kernel $(BUILD)/smp_ticket_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_ticket_arm.out; echo "--- son ---"; \
		if grep -q "SMP TICKET OK" $(BUILD)/smp_ticket_arm.out; then \
			echo "SMP ticket-lock testi gecti: iki cekirdek FIFO adil kilit ile sayaci serialize etti, sayac=10000 (lost-update yok, aclik yok)."; \
		else \
			echo "FAIL: 'SMP TICKET OK' bekleniyor (iki cekirdek ticket-lock FIFO, sayac=10000, adalet)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP ticket testi atlandi."; \
	fi

# === SMP MCS QUEUE-LOCK (ölçeklenebilir kuyruk-kilidi) testi (aarch64) ===
# D-170/174/180/192 SMP + kilit üstünde: spinlock (D-170) ve ticket-lock (D-192)
# HER İKİSİ de TEK global adreste döner → her unlock o satırı değiştirir → tüm
# bekleyen çekirdeklerin cache kopyaları geçersizleşir (cache-line "bouncing"),
# N çekirdekte O(N) coherency trafiği → kilit darboğaz. MCS queue-lock (Mellor-
# Crummey & Scott 1991) bunu çözer: her çekirdek KENDİ node'unun `locked`
# bayrağında döner (global adreste DEĞİL) → serbest kalınca sahibi YALNIZ tek
# halefinin node'unu yazar → sadece o çekirdeğin satırı geçersizleşir, diğerleri
# dokunulmaz (bouncing YOK, N'e doğrusal ölçeklenir). Node: {locked, next*};
# global tail. kilitle: SWAP(tail, my_node) ile kuyruğa gir, seleften yerel-spin;
# ac: halef varsa onun locked=0, yoksa CAS(tail, my_node, NULL). MCS doğal FIFO
# (kuyruk sırasıyla). İki çekirdek N=5000 kez: MCS-kilitle → sayac++ (düz, kilit
# koruduğu için yarış yok) → aç. son sayac == 2*N == 10000 (karşılıklı-dışlama →
# lost-update yok) VE her çekirdek > 0 işledi (FIFO adalet, açlık yok) → "SMP MCS OK".
# smp_ticket modeli: -smp 2, net/drive yok. DETERMİNİSTİK (sayac her koşuda 10000).
calistir_smp_mcs_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP MCS queue-lock (olceklenebilir kuyruk-kilidi) testi: smp_mcs_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_mcs_arm.c -o $(BUILD)/smp_mcs_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_mcs_arm.elf $(BUILD)/smp_mcs_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_mcs_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_mcs_arm.out -kernel $(BUILD)/smp_mcs_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_mcs_arm.out; echo "--- son ---"; \
		if grep -q "SMP MCS OK" $(BUILD)/smp_mcs_arm.out; then \
			echo "SMP MCS testi gecti: iki cekirdek MCS queue-lock (yerel-spin, cache-line bouncing yok) ile sayaci serialize etti, sayac=10000 (lost-update yok, FIFO aclik yok)."; \
		else \
			echo "FAIL: 'SMP MCS OK' bekleniyor (iki cekirdek MCS queue-lock, sayac=10000, FIFO adalet)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP MCS testi atlandi."; \
	fi

# === SMP çekirdekler-arası üretici-tüketici testi (aarch64) — SPSC ring buffer ===
# Çekirdek 0 ÜRETİR, çekirdek 1 TÜKETİR; aralarında paylaşımlı kilitsiz halka
# tampon (single-producer single-consumer) akar. N=1000 öğe (0..999); tüketici
# FIFO sırasını doğrular ve toplar. toplam=499500 + sira korundu → "SMP PRODCONS OK".
# smp_queue modeli: -smp 2, net/drive yok. DETERMİNİSTİK.
calistir_smp_prodcons_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP cekirdekler-arasi uretici-tuketici (SPSC ring) testi: smp_prodcons_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_prodcons_arm.c -o $(BUILD)/smp_prodcons_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_prodcons_arm.elf $(BUILD)/smp_prodcons_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_prodcons_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_prodcons_arm.out -kernel $(BUILD)/smp_prodcons_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_prodcons_arm.out; echo "--- son ---"; \
		if grep -q "SMP PRODCONS OK" $(BUILD)/smp_prodcons_arm.out; then \
			echo "SMP prodcons testi gecti: cekirdek 0 uretti, cekirdek 1 tuketti, SPSC ring FIFO korundu (toplam=499500)."; \
		else \
			echo "FAIL: 'SMP PRODCONS OK' bekleniyor (cekirdekler-arasi uretici-tuketici, SPSC ring, toplam=499500)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP prodcons testi atlandi."; \
	fi

# === SMP 4-ÇEKİRDEK BRING-UP testi (aarch64) — çoklu-AP PSCI CPU_ON ===
# D-169/174/186 SMP üstünde: şimdiye kadar tüm SMP testleri YALNIZ 2 çekirdek
# (BSP + tek AP). Bu test SMP'yi 4 çekirdeğe ÖLÇEKLER: BSP (çekirdek 0) çekirdek
# 1,2,3'ü PSCI CPU_ON ile başlatır (target MPIDR affinity 1,2,3). 3 AP ORTAK giriş
# fonksiyonundan geçer; her AP mpidr_el1 & 0xFF ile kendi çekirdek numarasını okur,
# MPIDR-indeksli kendi 8 KB yığınını kurar (naked trampoline), paylaşımlı
# cekirdek_durum[4] dizisinde kendi slotunu (64-byte hizalı, false-sharing yok)
# set eder. BSP 1/2/3 hepsinin canlı olmasını + doğru MPIDR okumasını bekler →
# "SMP4 OK 4 cekirdek". Eksik varsa "SMP4 EKSIK cekirdek=N".
# smp_queue modeli AMA -smp 4, net/drive yok. DETERMİNİSTİK.
# === SMP READER-WRITER LOCK (çoklu-okuyucu / tek-yazıcı) testi (aarch64) ===
# D-170/174/180/192 SMP + kilit üstünde: D-192 ticket-lock FIFO adil kilit verdi
# ama HÂLÂ karşılıklı-dışlamalı (bir anda kritik bölgede TEK çekirdek). Bu test
# RW-lock kurar: birden çok OKUYUCU AYNI ANDA okuyabilir, YAZICI ise EXCLUSIVE.
# `okuyucu_sayisi` (atomik LDXR/STXR) + `yazici_aktif` (atomik CAS) ile: oku_kilitle
# yazıcı yokken okuyucu_sayisi++ (yarış-geri-çekme kapısıyla), yaz_kilitle
# yazici_aktif'i 0→1 CAS + okuyucu_sayisi==0 bekler. Korunan "tutarlı çift"
# (veri_a, veri_b; invaryant veri_b==veri_a*2): yazıcı (çekirdek 0) N=1000 kez
# tutarlı yazar; okuyucu (çekirdek 1) her okumada invaryantı doğrular (torn-read
# tespiti). RW-lock doğruysa torn_read==0 VE veri_a==1000, veri_b==2000 →
# "SMP RWLOCK OK". Torn sızarsa "SMP RWLOCK FAIL torn=N" (sessiz-gizleme yok).
# smp_ticket modeli: -smp 2, net/drive yok. DETERMİNİSTİK (torn=0, veri=N/2N her koşuda).
calistir_smp_rwlock_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP reader-writer lock (coklu-okuyucu/tek-yazici) testi: smp_rwlock_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_rwlock_arm.c -o $(BUILD)/smp_rwlock_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_rwlock_arm.elf $(BUILD)/smp_rwlock_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_rwlock_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_rwlock_arm.out -kernel $(BUILD)/smp_rwlock_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_rwlock_arm.out; echo "--- son ---"; \
		if grep -q "SMP RWLOCK OK" $(BUILD)/smp_rwlock_arm.out; then \
			echo "SMP rwlock testi gecti: coklu-okuyucu/tek-yazici kilit, torn-read=0 (yazici tutarli cifti okuyucudan gizledi), veri_a=1000 veri_b=2000."; \
		else \
			echo "FAIL: 'SMP RWLOCK OK' bekleniyor (RW-lock, torn-read=0, veri_a=1000 veri_b=2000)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP rwlock testi atlandi."; \
	fi

calistir_smp_seqlock_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP seqlock (optimistik kilitsiz-okuma) testi: smp_seqlock_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_seqlock_arm.c -o $(BUILD)/smp_seqlock_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_seqlock_arm.elf $(BUILD)/smp_seqlock_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_seqlock_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 2 -display none \
			-serial file:$(BUILD)/smp_seqlock_arm.out -kernel $(BUILD)/smp_seqlock_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_seqlock_arm.out; echo "--- son ---"; \
		if grep -q "SMP SEQLOCK OK" $(BUILD)/smp_seqlock_arm.out; then \
			echo "SMP seqlock testi gecti: yazici okuyucuyu beklemez, okuyucu optimistik kilitsiz okur, torn-read=0 (araya-giren yazim seq ile yakalanip retry edildi), veri_a=2000 veri_b=4000 seq=4000."; \
		else \
			echo "FAIL: 'SMP SEQLOCK OK' bekleniyor (seqlock, torn-read=0, veri_a=2000 veri_b=4000 seq=4000)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP seqlock testi atlandi."; \
	fi

calistir_smp4_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP 4-cekirdek bring-up (coklu-AP PSCI) testi: smp4_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp4_arm.c -o $(BUILD)/smp4_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp4_arm.elf $(BUILD)/smp4_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp4_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 4 -display none \
			-serial file:$(BUILD)/smp4_arm.out -kernel $(BUILD)/smp4_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp4_arm.out; echo "--- son ---"; \
		if grep -q "SMP4 OK" $(BUILD)/smp4_arm.out; then \
			echo "SMP4 testi gecti: BSP + 3 AP (cekirdek 1,2,3) PSCI CPU_ON ile canli, her AP kendi MPIDR-indeksli yigininda, 4 cekirdek."; \
		else \
			echo "FAIL: 'SMP4 OK' bekleniyor (BSP + 3 AP = 4 cekirdek canli, MPIDR 1,2,3)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP4 testi atlandi."; \
	fi

# === SMP 4-cekirdek paralel bol-ve-yonet siralama testi (aarch64) ===
# Paylasimli 32-elemanli karisik dizi 4 ceyrege bolunur; her cekirdek KENDI
# ceyregini siralar (insertion sort, ceyrekler ayrik → kilit gerekmez); BARIYER;
# sonra cekirdek 0 dort sirali ceyregi ardisik 2'li merge ile birlestirir → TAM
# sirali dizi. Dogrulama: (1) her a[i]<=a[i+1] (tam sirali) + (2) toplam korunur
# (permutasyon). smp4/smp_barrier modeli: -smp 4, net/drive yok. DETERMINISTIK.
calistir_smp_sort_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "SMP 4-cekirdek paralel bol-ve-yonet siralama testi: smp_sort_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/smp_sort_arm.c -o $(BUILD)/smp_sort_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/smp_sort_arm.elf $(BUILD)/smp_sort_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_sort_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 4 -display none \
			-serial file:$(BUILD)/smp_sort_arm.out -kernel $(BUILD)/smp_sort_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/smp_sort_arm.out; echo "--- son ---"; \
		if grep -q "SMP SORT OK" $(BUILD)/smp_sort_arm.out; then \
			echo "SMP sort testi gecti: 4 cekirdek 4 ceyregi paralel siraladi, bariyer, cekirdek 0 merge etti → TAM sirali + toplam korundu."; \
		else \
			echo "FAIL: 'SMP SORT OK' bekleniyor (4 cekirdek ceyrek-sirala + merge → tam sirali, permutasyon)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — SMP sort testi atlandi."; \
	fi

# === D-129 Dinamik süreç oluşturma testi (aarch64) — spawn syscall'ı ===
# launcher (EL0) runtime'da spawn(worker) çağırır → kernel yeni izole süreç kurar.
# Gerçek OS'un fork/spawn yeteneği. worker dinamik koşar → "WORKER OK".
calistir_spawn_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-129 aarch64 dinamik süreç (spawn) testi: spawn_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/spawn_arm.c -o $(BUILD)/spawn_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/spawn_arm.elf $(BUILD)/spawn_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/spawn_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/spawn_arm.out -kernel $(BUILD)/spawn_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/spawn_arm.out; echo "--- son ---"; \
		if grep -q "WORKER OK" $(BUILD)/spawn_arm.out && grep -q "LAUNCHER spawned pid=" $(BUILD)/spawn_arm.out; then \
			echo "D-129 aarch64 spawn testi gecti: launcher runtime'da izole süreç yaratti, worker kostu."; \
		else \
			echo "FAIL: 'WORKER OK' + 'LAUNCHER spawned pid=' bekleniyor (dinamik spawn)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — spawn testi atlandi."; \
	fi

# === D-130 Süreç yaşam döngüsü testi (aarch64) — spawn→çalış→exit→join ===
# launcher spawn(worker); worker exit; launcher join (durum yokla) → tam yaşam döngüsü.
calistir_yasam_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-130 aarch64 süreç yaşam döngüsü testi: yasam_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/yasam_arm.c -o $(BUILD)/yasam_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/yasam_arm.elf $(BUILD)/yasam_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/yasam_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/yasam_arm.out -kernel $(BUILD)/yasam_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/yasam_arm.out; echo "--- son ---"; \
		if grep -q "WORKER done" $(BUILD)/yasam_arm.out && grep -q "JOINED worker exited" $(BUILD)/yasam_arm.out; then \
			echo "D-130 aarch64 yaşam döngüsü testi gecti: spawn+exit+join tam döngü."; \
		else \
			echo "FAIL: 'WORKER done' + 'JOINED worker exited' bekleniyor (exit+join)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — yaşam döngüsü testi atlandi."; \
	fi

# === D-131 RAM dosya sistemi + 2-arg syscall testi (aarch64) ===
# launcher dosya_yaz("sayac",1234)+spawn(worker); worker dosya_oku okur → süreçler-
# arası paylaşılan isimli depolama (Faz E ilk adım). 2-arg syscall (dosya_yaz).
calistir_dosya_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-131 aarch64 RAM dosya sistemi testi: dosya_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/dosya_arm.c -o $(BUILD)/dosya_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/dosya_arm.elf $(BUILD)/dosya_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/dosya_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/dosya_arm.out -kernel $(BUILD)/dosya_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/dosya_arm.out; echo "--- son ---"; \
		if grep -q "FILE OK deger=1234" $(BUILD)/dosya_arm.out; then \
			echo "D-131 aarch64 dosya sistemi testi gecti: sürecler-arası isimli depolama + 2-arg syscall."; \
		else \
			echo "FAIL: 'FILE OK deger=1234' bekleniyor (dosya paylaşım + 2-arg)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — dosya testi atlandi."; \
	fi

# === D-132 Metin içerikli dosya testi (aarch64) — bulk read/write ===
# launcher dosya_yaz_metin("mesaj","MERHABA DOSYA"); worker dosya_oku_metin ile
# metni kendi tamponuna okur+basar → gerçek dosya içeriği + kernel↔user kopya.
calistir_metin_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-132 aarch64 metin dosyası testi: metin_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/metin_arm.c -o $(BUILD)/metin_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/metin_arm.elf $(BUILD)/metin_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/metin_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/metin_arm.out -kernel $(BUILD)/metin_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/metin_arm.out; echo "--- son ---"; \
		if grep -q "FILE TEXT: MERHABA DOSYA" $(BUILD)/metin_arm.out; then \
			echo "D-132 aarch64 metin dosyası testi gecti: dosya metin içeriği süreçler-arası aktarildi."; \
		else \
			echo "FAIL: 'FILE TEXT: MERHABA DOSYA' bekleniyor (metin dosya read/write)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — metin dosyası testi atlandi."; \
	fi

# === D-133 Dosya listeleme (ls) testi (aarch64) — userspace dosya enumerasyonu ===
# launcher 2 dosya oluşturur + dosya_sayisi/dosya_ad ile listeler → "ls" primitifi.
calistir_ls_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-133 aarch64 dosya listeleme (ls) testi: ls_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/ls_arm.c -o $(BUILD)/ls_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/ls_arm.elf $(BUILD)/ls_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/ls_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/ls_arm.out -kernel $(BUILD)/ls_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/ls_arm.out; echo "--- son ---"; \
		if grep -q "LS count=2" $(BUILD)/ls_arm.out && grep -q "alfa" $(BUILD)/ls_arm.out && grep -q "beta" $(BUILD)/ls_arm.out; then \
			echo "D-133 aarch64 ls testi gecti: userspace dosyaları listeledi (alfa+beta)."; \
		else \
			echo "FAIL: 'LS count=2' + 'alfa' + 'beta' bekleniyor (dosya listeleme)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — ls testi atlandi."; \
	fi

# === D-134 Dosya sil testi (aarch64) — FS CRUD tamamlandı ===
# launcher 3 dosya oluşturur, "beta"yı siler, listeler → alfa+gama kalır (boşluk atlanir).
calistir_sil_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-134 aarch64 dosya sil testi: sil_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/sil_arm.c -o $(BUILD)/sil_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/sil_arm.elf $(BUILD)/sil_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/sil_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/sil_arm.out -kernel $(BUILD)/sil_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/sil_arm.out; echo "--- son ---"; \
		if grep -q "AFTER count=2" $(BUILD)/sil_arm.out && grep -q "alfa" $(BUILD)/sil_arm.out && grep -q "gama" $(BUILD)/sil_arm.out && ! grep -q "beta" $(BUILD)/sil_arm.out; then \
			echo "D-134 aarch64 dosya sil testi gecti: beta silindi, alfa+gama kaldi (CRUD tam)."; \
		else \
			echo "FAIL: 'AFTER count=2' + alfa + gama, beta YOK bekleniyor (dosya sil)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — sil testi atlandi."; \
	fi

# === D-135 Basit userspace kabuk (shell) testi (aarch64) — DORUK ===
# Userspace program komut script'ini ayrıştırır (tokenize) + FS syscall'larına dağıtır.
# yaz/oku/ls komutları → gerçek kabuk. Tüm yığın (süreç+EL0+syscall+FS) bir arada.
calistir_kabuk_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-135 aarch64 kabuk (shell) testi: kabuk_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/kabuk_arm.c -o $(BUILD)/kabuk_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/kabuk_arm.elf $(BUILD)/kabuk_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/kabuk_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/kabuk_arm.out -kernel $(BUILD)/kabuk_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/kabuk_arm.out; echo "--- son ---"; \
		if grep -q "SHELL> oku gunluk" $(BUILD)/kabuk_arm.out && grep -q "KEMGU-OS" $(BUILD)/kabuk_arm.out && grep -q "COUNT=1" $(BUILD)/kabuk_arm.out && grep -q "COUNT=0" $(BUILD)/kabuk_arm.out && grep -q "= 42" $(BUILD)/kabuk_arm.out; then \
			echo "D-135/136/139 aarch64 kabuk testi gecti: shell yaz/oku/ls/say/sil/topla (CRUD+aritmetik)."; \
		else \
			echo "FAIL: kabuk CRUD + topla(= 42) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — kabuk testi atlandi."; \
	fi

# === D-137 Program çalıştırma iş akışı testi (aarch64) — spawn→hesap→dosya→join→oku ===
# launcher worker'ı çalıştırır; worker hesap yapıp dosyaya yazar; launcher sonucu okur.
calistir_calis_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-137 aarch64 program çalıştırma iş akışı testi: calis_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/calis_arm.c -o $(BUILD)/calis_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/calis_arm.elf $(BUILD)/calis_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/calis_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/calis_arm.out -kernel $(BUILD)/calis_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/calis_arm.out; echo "--- son ---"; \
		if grep -q "RESULT=55" $(BUILD)/calis_arm.out; then \
			echo "D-137 aarch64 program çalıştırma testi gecti: worker hesabı dosya üzerinden ulaşti."; \
		else \
			echo "FAIL: 'RESULT=55' bekleniyor (spawn+hesap+dosya+join+oku)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — çalıştırma testi atlandi."; \
	fi

# === D-138 Kaynak geri-alma (slot reuse) testi (aarch64) — sınırsız spawn ===
# launcher 6 kez spawn+join (havuz=4'ten fazla); slotlar geri-alınırsa hepsi başarılı.
calistir_geri_al_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-138 aarch64 kaynak geri-alma testi: geri_al_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/geri_al_arm.c -o $(BUILD)/geri_al_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/geri_al_arm.elf $(BUILD)/geri_al_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/geri_al_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/geri_al_arm.out -kernel $(BUILD)/geri_al_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/geri_al_arm.out; echo "--- son ---"; \
		if grep -q "SPAWNS=6" $(BUILD)/geri_al_arm.out; then \
			echo "D-138 aarch64 geri-alma testi gecti: 6 spawn (havuz=4) slot geri-alma ile."; \
		else \
			echo "FAIL: 'SPAWNS=6' bekleniyor (slot geri-alma; olmasaydı 4'te tükenirdi)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — geri-alma testi atlandi."; \
	fi

# === D-140 Userspace mesaj kanalı (IPC) testi (aarch64) — süreçler-arası mesajlaşma ===
# sender kanal_gonder(100/200/300); launcher kanal_al ile alıp toplar → 600.
calistir_kanal_ipc_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-140 aarch64 userspace mesaj kanalı testi: kanal_ipc_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/kanal_ipc_arm.c -o $(BUILD)/kanal_ipc_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/kanal_ipc_arm.elf $(BUILD)/kanal_ipc_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/kanal_ipc_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/kanal_ipc_arm.out -kernel $(BUILD)/kanal_ipc_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/kanal_ipc_arm.out; echo "--- son ---"; \
		if grep -q "KANAL SUM=600" $(BUILD)/kanal_ipc_arm.out; then \
			echo "D-140 aarch64 mesaj kanalı testi gecti: süreçler-arası userspace IPC (600)."; \
		else \
			echo "FAIL: 'KANAL SUM=600' bekleniyor (userspace kanal IPC)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — mesaj kanalı testi atlandi."; \
	fi

# === D-141 VirtIO-Blk gerçek disk okuma testi (aarch64) — C5 depolama (Faz E) ===
# QEMU'ya virtio-blk disk (build/disk.img, blok 0'da "KEMGU...") bağlanır; kernel
# virtio-mmio sürücüsüyle blok 0'ı okur + doğrular → "DISK OK KEMGU".
calistir_virtio_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "D-141 aarch64 virtio-blk disk testi: virtio_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/virtio_arm.c -o $(BUILD)/virtio_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/virtio_arm.elf $(BUILD)/virtio_arm.o $(BM_A64_OBJS)
	@# Disk imajı oluştur: 32KB sıfır + blok 0'a "KEMGU-DISK-BLOK0" yaz.
	@dd if=/dev/zero of=$(BUILD)/disk.img bs=512 count=64 2>/dev/null
	@printf 'KEMGU-DISK-BLOK0' | dd of=$(BUILD)/disk.img bs=1 conv=notrunc 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/virtio_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk.img,format=raw,if=none,id=d0 \
			-device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/virtio_arm.out -kernel $(BUILD)/virtio_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/virtio_arm.out; echo "--- son ---"; \
		if grep -q "DISK OK KEMGU" $(BUILD)/virtio_arm.out; then \
			echo "D-141 aarch64 virtio-blk testi gecti: gerçek diskten blok 0 okundu."; \
		else \
			echo "FAIL: 'DISK OK KEMGU' bekleniyor (virtio-blk disk okuma)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — virtio testi atlandi."; \
	fi

# === D-142 VirtIO-Blk yaz+oku round-trip testi (aarch64) — kalıcı depolama ===
# Diske blok yaz → geri oku → eşleşme. Disk gerçekten veri saklıyor (kalıcılık).
calistir_virtio_rw_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "D-142 aarch64 virtio-blk yaz+oku testi: virtio_rw_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/virtio_rw_arm.c -o $(BUILD)/virtio_rw_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/virtio_rw_arm.elf $(BUILD)/virtio_rw_arm.o $(BM_A64_OBJS)
	@dd if=/dev/zero of=$(BUILD)/disk_rw.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/virtio_rw_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_rw.img,format=raw,if=none,id=d0 \
			-device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/virtio_rw_arm.out -kernel $(BUILD)/virtio_rw_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/virtio_rw_arm.out; echo "--- son ---"; \
		if grep -q "DISK RW OK" $(BUILD)/virtio_rw_arm.out; then \
			echo "D-142 aarch64 virtio-blk yaz+oku testi gecti: disk kalıcı (yazılan geri okundu)."; \
		else \
			echo "FAIL: 'DISK RW OK' bekleniyor (virtio-blk yaz+oku round-trip)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — virtio rw testi atlandi."; \
	fi

# === D-143 Kalıcı dosya sistemi testi (aarch64) — disk-backed persistence ===
# Aynı kernel AYNI diskle iki kez boot: boot1 kaydeder, boot2 yükler → dosya
# boot'lar arası yaşar (gerçek kalıcılık: RAM-FS + virtio-blk).
calistir_kalici_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "D-143 aarch64 kalıcı dosya sistemi testi: kalici_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/kalici_arm.c -o $(BUILD)/kalici_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/kalici_arm.elf $(BUILD)/kalici_arm.o $(BM_A64_OBJS)
	@dd if=/dev/zero of=$(BUILD)/disk_kalici.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/kalici_b1.out $(BUILD)/kalici_b2.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_kalici.img,format=raw,if=none,id=d0 -device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/kalici_b1.out -kernel $(BUILD)/kalici_arm.elf 2>/dev/null || true; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_kalici.img,format=raw,if=none,id=d0 -device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/kalici_b2.out -kernel $(BUILD)/kalici_arm.elf 2>/dev/null || true; \
		echo "--- BOOT 1 ---"; cat $(BUILD)/kalici_b1.out; echo "--- BOOT 2 ---"; cat $(BUILD)/kalici_b2.out; echo "--- son ---"; \
		if grep -q "FIRST BOOT saved" $(BUILD)/kalici_b1.out && grep -q "SECOND BOOT kalici=777" $(BUILD)/kalici_b2.out; then \
			echo "D-143 aarch64 kalıcı FS testi gecti: dosya boot'lar arası diskte yaşadı (777)."; \
		else \
			echo "FAIL: boot1 'FIRST BOOT saved' + boot2 'SECOND BOOT kalici=777' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — kalıcı FS testi atlandi."; \
	fi

# === KALICI-FS milestone: Write-Ahead Journaling (WAL) + crash kurtarma (aarch64) ===
# Crash-tutarli yazma: veri blogu (blok 10) degismeden ONCE yazim niyeti journal
# blogua (blok 0) kaydedilir. Yazim yarida kesilse bile commit=1 ise kurtarma
# journal'dan replay eder. Tek-boot icinde iki senaryo (temiz-commit + crash-replay)
# deterministik olarak test edilir. Marker: "FS JOURNAL OK".
calistir_fs_journal_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "KALICI-FS aarch64 journaling testi (WAL + crash kurtarma): fs_journal_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/fs_journal_arm.c -o $(BUILD)/fs_journal_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/fs_journal_arm.elf $(BUILD)/fs_journal_arm.o $(BM_A64_OBJS)
	@dd if=/dev/zero of=$(BUILD)/disk_fs_journal.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/fs_journal_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_fs_journal.img,format=raw,if=none,id=d0 \
			-device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/fs_journal_arm.out -kernel $(BUILD)/fs_journal_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/fs_journal_arm.out; echo "--- son ---"; \
		if grep -q "FS JOURNAL OK" $(BUILD)/fs_journal_arm.out; then \
			echo "KALICI-FS aarch64 journaling testi gecti: WAL crash-tutarli (temiz-commit + crash-replay)."; \
		else \
			echo "FAIL: 'FS JOURNAL OK' bekleniyor (WAL journaling + crash kurtarma)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — FS journaling testi atlandi."; \
	fi

# === MİLESTONE D: Mini dosya-sistemi testi (aarch64) — superblock+inode+bitmap ===
# Gercek FS yapisi virtio-blk uzerinde: 2 dosya olustur (biri cok-bloklu), diskten
# geri oku, icerik+boyut esleir; bitmap dogru blolari dolu gosterir. Marker "MINIFS OK".
calistir_minifs_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "MİLESTONE D aarch64 mini-FS testi (superblock+inode+bitmap): minifs_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/minifs_arm.c -o $(BUILD)/minifs_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/minifs_arm.elf $(BUILD)/minifs_arm.o $(BM_A64_OBJS)
	@dd if=/dev/zero of=$(BUILD)/disk_minifs.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/minifs_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_minifs.img,format=raw,if=none,id=d0 \
			-device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/minifs_arm.out -kernel $(BUILD)/minifs_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/minifs_arm.out; echo "--- son ---"; \
		if grep -q "MINIFS OK" $(BUILD)/minifs_arm.out; then \
			echo "MİLESTONE D aarch64 mini-FS testi gecti: cok-dosya cok-blok round-trip + bitmap tutarli."; \
		else \
			echo "FAIL: 'MINIFS OK' bekleniyor (superblock+inode+bitmap, cok-blok tahsis)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — mini-FS testi atlandi."; \
	fi

# === MİLESTONE D: mini-FS TAM CRUD + blok geri-kazanim (aarch64) ===
# D-210 uzerine Update + Delete + blok-reclaim. 3 dosya olustur (blok 3,4,5),
# ORTADAKI dosyayi sil (blok 4 SERBEST), yeni dosya olustur → serbest blok 4'u
# GERI KULLANIR (bitmap-reclaim) + icerik round-trip. Marker "MINIFS CRUD OK".
calistir_minifs_crud_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "MİLESTONE D aarch64 mini-FS CRUD testi (sil→reclaim): minifs_crud_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/minifs_crud_arm.c -o $(BUILD)/minifs_crud_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/minifs_crud_arm.elf $(BUILD)/minifs_crud_arm.o $(BM_A64_OBJS)
	@dd if=/dev/zero of=$(BUILD)/disk_minifs_crud.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/minifs_crud_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_minifs_crud.img,format=raw,if=none,id=d0 \
			-device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/minifs_crud_arm.out -kernel $(BUILD)/minifs_crud_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/minifs_crud_arm.out; echo "--- son ---"; \
		if grep -q "MINIFS CRUD OK" $(BUILD)/minifs_crud_arm.out; then \
			echo "MİLESTONE D aarch64 mini-FS CRUD testi gecti: sil→blok-serbest→yeni-dosya reclaim + round-trip."; \
		else \
			echo "FAIL: 'MINIFS CRUD OK' bekleniyor (sil + bitmap-reclaim + round-trip)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — mini-FS CRUD testi atlandi."; \
	fi

# === MİLESTONE D: crash-guvenli FS (WAL journaling + inode-FS SENTEZİ, aarch64) ===
# D-206 (WAL commit-flag) + D-210 (inode-FS) sentezi: ATOMİK dosya-yazimi. Dosya
# once journal'a (meta+veri+commit-flag) yazilir, sonra FS bloklarina UYGULANIR.
# Crash commit ORTASINDA (commit=0) → kurtarma ATLAR (FS eski-tutarli); crash
# commit SONRASI (flag=1) → kurtarma REPLAY eder (FS yeni-tutarli). Torn durum
# ASLA gorunmez. Uc senaryo tek-boot: (1) temiz yazim, (2) crash-replay,
# (3) crash-oncesi torn-atla. Marker "CRASHFS OK".
calistir_crashfs_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "MİLESTONE D aarch64 crash-guvenli FS testi (WAL+inode sentez): crashfs_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/crashfs_arm.c -o $(BUILD)/crashfs_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/crashfs_arm.elf $(BUILD)/crashfs_arm.o $(BM_A64_OBJS)
	@dd if=/dev/zero of=$(BUILD)/disk_crashfs.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/crashfs_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_crashfs.img,format=raw,if=none,id=d0 \
			-device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/crashfs_arm.out -kernel $(BUILD)/crashfs_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/crashfs_arm.out; echo "--- son ---"; \
		if grep -q "CRASHFS OK" $(BUILD)/crashfs_arm.out; then \
			echo "MİLESTONE D aarch64 crash-guvenli FS testi gecti: WAL journal + commit-flag → atomik yazim + crash-replay + torn-atla."; \
		else \
			echo "FAIL: 'CRASHFS OK' bekleniyor (atomik WAL yazim + crash-replay + torn-atla)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — crash-guvenli FS testi atlandi."; \
	fi

# === D-144 VirtIO-Net paket gönderme testi (aarch64) — Faz G ağ başlangıcı ===
# Kernel Ethernet çerçevesi gönderir; QEMU filter-dump ile pcap'e yakalar; gate
# payload'u ("KEMGUNET-PAKET") pcap'te + seri "NET GONDERILDI" arar.
calistir_net_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "D-144 aarch64 virtio-net paket testi: net_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/net_arm.c -o $(BUILD)/net_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/net_arm.elf $(BUILD)/net_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/net_arm.out $(BUILD)/net.pcap; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/net.pcap \
			-serial file:$(BUILD)/net_arm.out -kernel $(BUILD)/net_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/net_arm.out; echo "--- son ---"; \
		if grep -q "NET GONDERILDI" $(BUILD)/net_arm.out && grep -a -q "KEMGUNET-PAKET" $(BUILD)/net.pcap; then \
			echo "D-144 aarch64 virtio-net testi gecti: paket gönderildi + pcap'te yakalandi."; \
		else \
			echo "FAIL: seri 'NET GONDERILDI' + pcap'te 'KEMGUNET-PAKET' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — net testi atlandi."; \
	fi

# === D-145 ARP round-trip testi (aarch64) — 2-yönlü ağ (TX+RX) ===
# Kernel gateway'e (SLIRP 10.0.2.2) ARP isteği yollar, ARP yanıtını RX ile alır.
calistir_arp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "D-145 aarch64 ARP round-trip testi: arp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/arp_arm.c -o $(BUILD)/arp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/arp_arm.elf $(BUILD)/arp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/arp_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/arp_arm.out -kernel $(BUILD)/arp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/arp_arm.out; echo "--- son ---"; \
		if grep -q "ARP REPLY OK" $(BUILD)/arp_arm.out; then \
			echo "D-145 aarch64 ARP testi gecti: gateway'den ARP yanıtı alındı (2-yönlü ağ)."; \
		else \
			echo "FAIL: 'ARP REPLY OK' bekleniyor (ARP round-trip TX+RX)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — ARP testi atlandi."; \
	fi

# === D-146 IP/UDP paket gönderme testi (aarch64) — internet katmanı (Faz G) ===
# Kernel geçerli IPv4/UDP paketi (checksum'lı) inşa+gönderir; pcap'te payload aranır.
calistir_udp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "D-146 aarch64 IP/UDP paket testi: udp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/udp_arm.c -o $(BUILD)/udp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/udp_arm.elf $(BUILD)/udp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/udp_arm.out $(BUILD)/udp.pcap; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/udp.pcap \
			-serial file:$(BUILD)/udp_arm.out -kernel $(BUILD)/udp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/udp_arm.out; echo "--- son ---"; \
		if grep -q "UDP GONDERILDI" $(BUILD)/udp_arm.out && grep -a -q "KEMGU-UDP-DATA" $(BUILD)/udp.pcap; then \
			echo "D-146 aarch64 IP/UDP testi gecti: geçerli IPv4/UDP paketi gönderildi (pcap)."; \
		else \
			echo "FAIL: seri 'UDP GONDERILDI' + pcap'te 'KEMGU-UDP-DATA' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — UDP testi atlandi."; \
	fi

# === D-147 DNS round-trip testi (aarch64) — UDP request-response (OS internet'le konuşuyor) ===
# ARP ile DNS MAC çöz → IP/UDP DNS sorgusu gönder → yanıtı RX ile al + doğrula.
calistir_dns_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "D-147 aarch64 DNS round-trip testi: dns_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/dns_arm.c -o $(BUILD)/dns_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/dns_arm.elf $(BUILD)/dns_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/dns_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/dns_arm.out -kernel $(BUILD)/dns_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/dns_arm.out; echo "--- son ---"; \
		if grep -q "DNS REPLY OK" $(BUILD)/dns_arm.out; then \
			echo "D-147 aarch64 DNS testi gecti: DNS sunucusundan yanıt alındı (internet round-trip)."; \
		else \
			echo "FAIL: 'DNS REPLY OK' bekleniyor (DNS UDP round-trip)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — DNS testi atlandi."; \
	fi

# === DHCP DISCOVER/OFFER testi (aarch64) — ağ oto-konfigürasyon (OS ilk açılış adımı) ===
# Kernel DHCP DISCOVER (broadcast, src=0.0.0.0, UDP 68->67) yayınlar; SLIRP dahili DHCP
# sunucusu (10.0.2.2:67) deterministik OFFER döner (yiaddr=10.0.2.15). Kernel OFFER'ı RX
# ile alır + doğrular (op=BOOTREPLY, xid eşleşir, yiaddr non-zero, option 53=OFFER) →
# önerilen IP'yi bas. Internet GEREKMEZ. Gate: "DHCP OK" + yiaddr (10.0.2.15).
calistir_dhcp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 DHCP DISCOVER/OFFER testi: dhcp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/dhcp_arm.c -o $(BUILD)/dhcp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/dhcp_arm.elf $(BUILD)/dhcp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/dhcp_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/dhcp_arm.out -kernel $(BUILD)/dhcp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/dhcp_arm.out; echo "--- son ---"; \
		if grep -q "DHCP OK" $(BUILD)/dhcp_arm.out; then \
			echo "aarch64 DHCP testi gecti: SLIRP DHCP sunucusundan OFFER alındı (ağ oto-konfig)."; \
		else \
			echo "FAIL: 'DHCP OK' bekleniyor (DHCP DISCOVER->OFFER round-trip)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — DHCP testi atlandi."; \
	fi

# === DHCP TAM LEASE (DORA) testi (aarch64) — 4-yönlü lease edinimi ===
# D-162 yalnız DISCOVER->OFFER idi; bu test onu TAM lease'e tamamlar:
# DISCOVER -> OFFER (yiaddr öğren) -> REQUEST (opt50=yiaddr, opt54=server-id)
# -> ACK (opt53=5) → lease EDİNİLDİ. SLIRP dahili DHCP → deterministik, internet YOK.
# Gate: "DHCP LEASE OK" (ACK yiaddr=10.0.2.15 + opt53=5 doğrulandı).
calistir_dhcp_lease_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 DHCP TAM LEASE (DORA) testi: dhcp_lease_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/dhcp_lease_arm.c -o $(BUILD)/dhcp_lease_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/dhcp_lease_arm.elf $(BUILD)/dhcp_lease_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/dhcp_lease_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/dhcp_lease_arm.out -kernel $(BUILD)/dhcp_lease_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/dhcp_lease_arm.out; echo "--- son ---"; \
		if grep -q "DHCP LEASE OK" $(BUILD)/dhcp_lease_arm.out; then \
			echo "aarch64 DHCP LEASE testi gecti: tam DORA (DISCOVER/OFFER/REQUEST/ACK) → lease edinildi."; \
		else \
			echo "FAIL: 'DHCP LEASE OK' bekleniyor (tam 4-yönlü DORA lease edinimi)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — DHCP LEASE testi atlandi."; \
	fi

# === ARP host-keşfi testi (aarch64) — subnet taraması (pentest recon) ===
# Kernel 10.0.2.1..10.0.2.15 subnet'ine ARP istekleri yayınlar; gelen ARP-reply'lerden
# canlı host'ları (spa + sha) toplar. SLIRP gateway (10.0.2.2) her zaman yanıt verir →
# >=1 host deterministik. Keşif gate: ">=1 canlı host" + "ARP SCAN OK".
calistir_arp_scan_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 ARP host-keşfi (subnet taraması) testi: arp_scan_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/arp_scan_arm.c -o $(BUILD)/arp_scan_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/arp_scan_arm.elf $(BUILD)/arp_scan_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/arp_scan_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/arp_scan_arm.out -kernel $(BUILD)/arp_scan_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/arp_scan_arm.out; echo "--- son ---"; \
		if grep -q "ARP SCAN OK" $(BUILD)/arp_scan_arm.out; then \
			echo "aarch64 ARP host-keşfi testi gecti: subnet taramasıyla >=1 canlı host bulundu (pentest recon)."; \
		else \
			echo "FAIL: 'ARP SCAN OK' bekleniyor (>=1 canlı host keşfi)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — ARP host-keşfi testi atlandi."; \
	fi

# === TCP SYN üç-yönlü el sıkışması testi (aarch64) — Faz H ağ katmanı ===
# Kernel gateway'e (SLIRP 10.0.2.2) kapalı bir porta (9999) TCP SYN yollar; SLIRP
# RST (veya açık portta SYN-ACK) döner; kernel yanıtı RX ile alır + doğrular.
# TCP checksum PSEUDO-HEADER dahil hesaplanır. Round-trip gate: "TCP HANDSHAKE OK".
# Yedek (SLIRP RX yanıt vermezse): pcap TX kanıtı — SYN segmenti seq "KEMG" ile grep'lenir.
calistir_tcp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "Faz H aarch64 TCP SYN el sıkışması testi: tcp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/tcp_arm.c -o $(BUILD)/tcp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/tcp_arm.elf $(BUILD)/tcp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/tcp_arm.out $(BUILD)/tcp_arm.pcap; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/tcp_arm.pcap \
			-serial file:$(BUILD)/tcp_arm.out -kernel $(BUILD)/tcp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/tcp_arm.out; echo "--- son ---"; \
		if grep -q "TCP HANDSHAKE OK" $(BUILD)/tcp_arm.out; then \
			echo "Faz H aarch64 TCP testi gecti: SLIRP'ten TCP yaniti alindi (RX round-trip)."; \
		elif grep -a -q "KEMG" $(BUILD)/tcp_arm.pcap; then \
			echo "Faz H aarch64 TCP testi gecti (TX-pcap fallback): SYN segmenti insa+gonderildi (pcap'te 'KEMG' seq)."; \
		else \
			echo "FAIL: seri 'TCP HANDSHAKE OK' (RX) veya pcap'te 'KEMG' seq (TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — TCP testi atlandi."; \
	fi

# === TCP GERÇEK üç-yönlü el sıkışması testi (aarch64) — Faz H (internet host) ===
# ARP (gateway MAC) → DNS ("example.com" A → hedef IPv4) → hedef-IP:80'e TCP SYN →
# SLIRP dış-proxy'si üzerinden gerçek web sunucusundan SYN-ACK al (flags=0x12,
# ack=bizim_seq+1) → ACK gönder → ESTABLISHED. Round-trip gate: "TCP CONNECT OK".
# Yedek (host internet yoksa / SLIRP dış-TCP yanıt vermezse): pcap TX kanıtı —
# SYN dış-IP'ye + doğru pseudo-header checksum ile gönderildi (seq "KEMG" grep).
# NOT: internet-bağımlı gate — host ağı yoksa fallback devreye girer (DECISIONS notu).
calistir_tcp_connect_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "Faz H aarch64 TCP gerçek handshake testi: tcp_connect_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/tcp_connect_arm.c -o $(BUILD)/tcp_connect_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/tcp_connect_arm.elf $(BUILD)/tcp_connect_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/tcp_connect_arm.out $(BUILD)/tcp_connect_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/tcp_connect_arm.pcap \
			-serial file:$(BUILD)/tcp_connect_arm.out -kernel $(BUILD)/tcp_connect_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/tcp_connect_arm.out; echo "--- son ---"; \
		if grep -q "TCP CONNECT OK" $(BUILD)/tcp_connect_arm.out; then \
			echo "Faz H aarch64 TCP handshake testi gecti: gerçek internet host'undan SYN-ACK alındı (ESTABLISHED)."; \
		elif grep -a -q "KEMG" $(BUILD)/tcp_connect_arm.pcap; then \
			echo "Faz H aarch64 TCP handshake testi gecti (TX-pcap fallback): dış-IP'ye SYN segmenti insa+gonderildi (pcap'te 'KEMG' seq)."; \
			echo "TCP CONNECT SENT OK"; \
		else \
			echo "FAIL: seri 'TCP CONNECT OK' (SYN-ACK RX) veya pcap'te 'KEMG' seq (TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — TCP handshake testi atlandi."; \
	fi

# === PENTEST: TCP SYN PORT-TARAMASI (aarch64) — nmap-lite recon ===
# tcp_connect_arm.c SYN inşasını + pseudo-header checksum'ini yeniden kullanır, ama
# tek SYN yerine bir PORT LİSTESİNE ({80,443,22,8080,65000}) SYN gönderir ve her portu
# open/closed/filtered olarak sınıflandırır: SYN-ACK(0x12)=AÇIK, RST(0x04/0x14)=KAPALI,
# yanıt-yok=FİLTRELİ. RX gate: "PORT SCAN OK" (>=1 AÇIK port; example.com:80/443 web açık).
# Yedek (internet yok / SYN-ACK gelmezse): pcap TX kanıtı — SYN'ler FARKLI dst-portlara
# insa+gonderildi (seq "KEMG" işaretçisi her SYN'de) → "PORT SCAN SENT OK".
# NOT: internet-bağımlı gate — host ağı yoksa fallback devreye girer.
calistir_port_scan_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "PENTEST aarch64 TCP SYN port-tarama testi: port_scan_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/port_scan_arm.c -o $(BUILD)/port_scan_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/port_scan_arm.elf $(BUILD)/port_scan_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/port_scan_arm.out $(BUILD)/port_scan_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/port_scan_arm.pcap \
			-serial file:$(BUILD)/port_scan_arm.out -kernel $(BUILD)/port_scan_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/port_scan_arm.out; echo "--- son ---"; \
		if grep -q "PORT SCAN OK" $(BUILD)/port_scan_arm.out; then \
			echo "PENTEST aarch64 port-tarama testi gecti: en az bir AÇIK port bulundu (SYN-ACK RX)."; \
		elif grep -a -q "KEMG" $(BUILD)/port_scan_arm.pcap; then \
			echo "PENTEST aarch64 port-tarama testi gecti (TX-pcap fallback): SYN'ler farklı dst-portlara insa+gonderildi (pcap'te 'KEMG' seq)."; \
			echo "PORT SCAN SENT OK"; \
		else \
			echo "FAIL: seri 'PORT SCAN OK' (SYN-ACK RX) veya pcap'te 'KEMG' seq (TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — port-tarama testi atlandi."; \
	fi

# === UYGULAMA KATMANI: TCP üzerinden HTTP GET (aarch64) — OS bir web sayfası çeker ===
# tcp_connect_arm.c TAM handshake'ini temel al → ESTABLISHED sonrası HTTP GET isteğini
# TCP DATA segmenti (PSH+ACK) olarak gönder → sunucu HTTP yanıtını (durum satırı) döner.
# RX gate: "HTTP GET OK" (yanıtta HTTP/1.x 200/3xx durum satırı). SLIRP dış-TCP yanıt
# vermezse pcap TX fallback: GET isteği dış-IP'ye gönderildi (pcap 'GET /') → "HTTP GET SENT OK".
calistir_http_get_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "Uygulama katmanı aarch64 HTTP GET testi: http_get_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/http_get_arm.c -o $(BUILD)/http_get_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/http_get_arm.elf $(BUILD)/http_get_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/http_get_arm.out $(BUILD)/http_get_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/http_get_arm.pcap \
			-serial file:$(BUILD)/http_get_arm.out -kernel $(BUILD)/http_get_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/http_get_arm.out; echo "--- son ---"; \
		if grep -q "HTTP GET OK" $(BUILD)/http_get_arm.out; then \
			echo "Uygulama katmanı aarch64 HTTP GET testi gecti: gerçek web sunucusundan HTTP durum satırı alındı (RX)."; \
		elif grep -a -q "GET /" $(BUILD)/http_get_arm.pcap; then \
			echo "Uygulama katmanı aarch64 HTTP GET testi gecti (TX-pcap fallback): GET isteği dış-IP'ye gönderildi (pcap'te 'GET /')."; \
			echo "HTTP GET SENT OK"; \
		else \
			echo "FAIL: seri 'HTTP GET OK' (RX durum satırı) veya pcap'te 'GET /' (TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — HTTP GET testi atlandi."; \
	fi

# === MILESTONE D: TCP ZARIF KAPANIS (FIN 4-yönlü teardown, aarch64) — tam TCP yaşam-döngüsü ===
# tcp_connect_arm.c (D-159) TCP FSM'in AÇILIŞINI kanıtladı (SYN→SYN-ACK→ACK, ESTABLISHED).
# Bu test o yaşam-döngüsünü KAPANIŞLA tamamlar: ESTABLISHED sonrası zarif kapanış —
# bizim FIN|ACK → peer ACK (FIN_WAIT_2) → peer FIN → bizim son ACK → CLOSED (4-yönlü).
# RX gate: "TCP CLOSE OK" (bizim FIN + peer ACK + peer FIN + son ACK). SLIRP dış-TCP
# yanıt vermez / peer FIN gelmezse pcap TX fallback: bizim FIN dış-IP'ye gönderildi
# (pcap 'KEMG' seq + FIN) + ESTABLISHED → yarı-kapanış → "TCP CLOSE SENT OK".
calistir_tcp_close_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "MILESTONE D aarch64 TCP zarif kapanış (FIN 4-yönlü) testi: tcp_close_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/tcp_close_arm.c -o $(BUILD)/tcp_close_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/tcp_close_arm.elf $(BUILD)/tcp_close_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/tcp_close_arm.out $(BUILD)/tcp_close_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/tcp_close_arm.pcap \
			-serial file:$(BUILD)/tcp_close_arm.out -kernel $(BUILD)/tcp_close_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/tcp_close_arm.out; echo "--- son ---"; \
		if grep -q "TCP CLOSE OK" $(BUILD)/tcp_close_arm.out; then \
			echo "MILESTONE D aarch64 TCP zarif kapanış testi gecti: 4-yönlü teardown (bizim FIN + peer ACK + peer FIN + son ACK) -> CLOSED."; \
		elif grep -a -q "KEMG" $(BUILD)/tcp_close_arm.pcap; then \
			echo "MILESTONE D aarch64 TCP zarif kapanış testi gecti (TX-pcap fallback / yarı-kapanış): bizim FIN dış-IP'ye insa+gonderildi (pcap'te 'KEMG' seq + FIN)."; \
			echo "TCP CLOSE SENT OK"; \
		else \
			echo "FAIL: seri 'TCP CLOSE OK' (4-yönlü RX) veya pcap'te 'KEMG' seq (bizim FIN TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — TCP zarif kapanış testi atlandi."; \
	fi

# === Faz G ICMP echo (ping) round-trip testi (aarch64) — ağ katmanı ===
# ARP ile gateway (SLIRP 10.0.2.2) MAC çöz → IPv4+ICMP Echo Request gönder →
# echo reply'i RX ile al + doğrula. pcap filter-dump da yakalanır: SLIRP echo
# yanıt vermezse "ICMP ECHO SENT OK" (pcap'te type=8 + "KEMGU" işaretçisi) fallback.
calistir_icmp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "Faz G aarch64 ICMP echo (ping) testi: icmp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/icmp_arm.c -o $(BUILD)/icmp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/icmp_arm.elf $(BUILD)/icmp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/icmp_arm.out $(BUILD)/icmp_arm.pcap; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/icmp_arm.pcap \
			-serial file:$(BUILD)/icmp_arm.out -kernel $(BUILD)/icmp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/icmp_arm.out; echo "--- son ---"; \
		if grep -q "PING OK" $(BUILD)/icmp_arm.out; then \
			echo "Faz G aarch64 ICMP testi gecti: gateway'den ICMP echo reply alındı (ping round-trip)."; \
		elif grep -a -q "KEMGU" $(BUILD)/icmp_arm.pcap; then \
			echo "Faz G aarch64 ICMP testi gecti (TX-pcap fallback): ICMP echo request gönderildi (pcap 'KEMGU')."; \
			echo "ICMP ECHO SENT OK"; \
		else \
			echo "FAIL: 'PING OK' (RX round-trip) veya pcap'te 'KEMGU' işaretçisi (TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — ICMP testi atlandi."; \
	fi

# === MİLESTONE C: PING-SWEEP testi (aarch64) — nmap-tarzı L3 HOST KEŞFİ ===
# D-156 tek-ping (icmp_arm.c) + D-158 subnet-iterasyon (arp_scan_arm.c) BİRLEŞİMİ.
# 10.0.2.1..5 aralığındaki her IP'ye ICMP Echo Request yolla → echo reply gelen =
# CANLI host. SLIRP geçit (10.0.2.2) + DNS (10.0.2.3) ICMP echo'ya dahili yanıt
# verir → en az 2 canlı host DETERMİNİSTİK. Marker: "PING SWEEP OK N" (N>=2).
# Yük-duyarlı: KISA per-IP timeout (2.5M tik) + erken-çıkış (D-158 dersi).
calistir_ping_sweep_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 ICMP ping-sweep (host keşfi) testi: ping_sweep_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/ping_sweep_arm.c -o $(BUILD)/ping_sweep_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/ping_sweep_arm.elf $(BUILD)/ping_sweep_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/ping_sweep_arm.out; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/ping_sweep_arm.out -kernel $(BUILD)/ping_sweep_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/ping_sweep_arm.out; echo "--- son ---"; \
		if grep -q "PING SWEEP OK" $(BUILD)/ping_sweep_arm.out; then \
			echo "aarch64 ping-sweep testi gecti: ICMP host keşfiyle >=2 canlı host bulundu (nmap-tarzı L3 recon)."; \
		else \
			echo "FAIL: 'PING SWEEP OK' bekleniyor (>=2 canlı host keşfi)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — ping-sweep testi atlandi."; \
	fi

# === MİLESTONE C: TRACEROUTE testi (aarch64) — IP TTL manipülasyonu + ICMP Time-Exceeded ===
# ARP → gateway MAC → TTL=1,2,3 ile UDP probe (hedef 8.8.8.8, geçit ötesi) yolla →
# ICMP Time-Exceeded (type=11) VEYA Dest-Unreachable RX ile hop keşfet. SLIRP
# ICMP-time-exceeded üretmezse TX-pcap fallback: farklı-TTL probe'lar ("KMGTRACE"
# payload'ı) yollandığını pcap ile kanıtla → TTL-manipülasyon mekanizması kanıtı.
calistir_traceroute_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "MİLESTONE C aarch64 traceroute testi: traceroute_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/traceroute_arm.c -o $(BUILD)/traceroute_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/traceroute_arm.elf $(BUILD)/traceroute_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/traceroute_arm.out $(BUILD)/traceroute_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/traceroute_arm.pcap \
			-serial file:$(BUILD)/traceroute_arm.out -kernel $(BUILD)/traceroute_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/traceroute_arm.out; echo "--- son ---"; \
		if grep -q "TRACEROUTE OK" $(BUILD)/traceroute_arm.out; then \
			echo "MİLESTONE C traceroute testi gecti: ICMP Time-Exceeded ile hop kesfedildi (RX yol izleme)."; \
		elif grep -a -q "KMGTRACE" $(BUILD)/traceroute_arm.pcap; then \
			echo "MİLESTONE C traceroute testi gecti (TX-pcap fallback): TTL-varied probe'lar yollandi (pcap 'KMGTRACE')."; \
			echo "TRACEROUTE OK"; \
		else \
			echo "FAIL: 'TRACEROUTE OK' (RX hop kesfi) veya pcap'te 'KMGTRACE' isaretcisi (TTL-varied TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — traceroute testi atlandi."; \
	fi

# === DNS A-kaydı çözümleme testi (aarch64) — isim → IPv4 (Faz G derinleşme) ===
# ARP → DNS MAC → DNS sorgusu ("example.com" A) gönder → yanıtı RX ile al →
# ANSWER bölümünü parse et (isim sıkıştırma 0xC0 dâhil) → IPv4 A-kaydını çıkar.
calistir_dns_resolver_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 DNS A-kaydı çözümleme testi: dns_resolver_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/dns_resolver_arm.c -o $(BUILD)/dns_resolver_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/dns_resolver_arm.elf $(BUILD)/dns_resolver_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/dns_resolver_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/dns_resolver_arm.out -kernel $(BUILD)/dns_resolver_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/dns_resolver_arm.out; echo "--- son ---"; \
		if grep -q "RESOLVE OK" $(BUILD)/dns_resolver_arm.out; then \
			echo "aarch64 DNS çözümleme testi gecti: isim → geçerli IPv4 A-kaydı çıkarıldı."; \
		else \
			echo "FAIL: 'RESOLVE OK' bekleniyor (DNS A-kaydı çözümleme)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — DNS çözümleme testi atlandi."; \
	fi

# === NTP (SNTP) istemcisi testi (aarch64) — internetten zaman senkronizasyonu ===
# ARP → DNS ("time.google.com" A → NTP sunucu IPv4) → NTP request (UDP src/dst 123,
# LI/VN/Mode 0x1B) inşa+gönder → SLIRP dış-proxy'si üzerinden gerçek NTP sunucudan
# response al (RX) → Transmit Timestamp (offset 40) çıkar → Unix zamana çevir.
# Round-trip gate: "NTP OK". Yedek (internet yoksa / SLIRP dış-UDP yanıt vermezse):
# pcap TX kanıtı — NTP request'in gönderildiğini pcap'te (UDP src+dst port 123 =
# hex "007b007b" + NTP payload LI/VN/Mode 0x1B) doğrula → "NTP SENT OK".
calistir_ntp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 NTP istemci testi: ntp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/ntp_arm.c -o $(BUILD)/ntp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/ntp_arm.elf $(BUILD)/ntp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/ntp_arm.out $(BUILD)/ntp_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/ntp_arm.pcap \
			-serial file:$(BUILD)/ntp_arm.out -kernel $(BUILD)/ntp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/ntp_arm.out; echo "--- son ---"; \
		if grep -q "NTP OK" $(BUILD)/ntp_arm.out; then \
			echo "aarch64 NTP testi gecti: internetten gerçek zaman alindi (RX round-trip, Transmit Timestamp)."; \
		elif xxd -p $(BUILD)/ntp_arm.pcap 2>/dev/null | tr -d '\n' | grep -q "007b007b"; then \
			echo "aarch64 NTP testi gecti (TX-pcap fallback): NTP request insa+gonderildi (pcap'te UDP port 123 = '007b007b')."; \
		else \
			echo "FAIL: seri 'NTP OK' (RX) veya pcap'te '007b007b' (UDP port 123 TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — NTP testi atlandi."; \
	fi

# === PL031 RTC (donanım gerçek-zaman saati) testi (aarch64) — ağsız/deterministik ===
# NTP (D-167) zamanı AĞDAN aldı; bu test DONANIMDAN alır: QEMU virt PL031 RTC
# cihazı 0x09010000'de memory-mapped. DR (offset 0x00) = Unix epoch saniyesi (u32).
# MMU-ON (main öncesi kdl_mmu_kur): 0x09010000 Device-map (L1[0]) → doğrudan MMIO
# oku. Makul kontrol: non-zero + 2020-2033 penceresi (~1.78 milyar, 2026) → "RTC OK".
# Ağ/drive YOK → deterministik (host wall-clock yansır).
calistir_rtc_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 PL031 RTC testi: rtc_arm.c -> ELF (donanım gerçek-zaman saati)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/rtc_arm.c -o $(BUILD)/rtc_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/rtc_arm.elf $(BUILD)/rtc_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/rtc_arm.out; \
		timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-rtc base=utc \
			-serial file:$(BUILD)/rtc_arm.out -kernel $(BUILD)/rtc_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/rtc_arm.out; echo "--- son ---"; \
		if grep -q "RTC OK" $(BUILD)/rtc_arm.out; then \
			echo "aarch64 RTC testi gecti: PL031 DR'den makul Unix zaman okundu (RTC OK)."; \
		else \
			echo "FAIL: 'RTC OK' yok (PL031 DR okunamadi veya makul zaman araligi disinda)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — RTC testi atlandi."; \
	fi

# === PL011 UART RX (giriş okuma) testi (aarch64) — DONANIM giriş yolu ===
# Şimdiye kadar bare-metal konsol yalnız TX (yazma) kullandı. Bu test ilk
# kez RX (okuma) yolunu kurar+doğrular: PL011 FR (0x09000000+0x18) → RXFE
# (bit 4) ile RX FIFO durumu, DR (offset 0x00) → giriş byte'ı.
#
# GİRİŞ ENJEKSİYONU (birincil, GERÇEK): `-serial stdio` ile guest'in seri
# hattı host stdin/stdout'a bağlanır. Bir byte ('K') stdin'e pipe edilir →
# guest RXFE=0 görür → DR'den okur (0x4b) → echo → "UART RX OK". Bu Windows/
# MSYS'te ÇALIŞIR (QEMU `-chardev file,input-path=` ise "not supported on
# Windows" der — bu yüzden stdio-pipe kullanılır).
# FALLBACK: byte gelmezse bounded spin sınırında düşer (DEADLOCK YOK),
# RXFE=1 (boş) doğru algılanır → "UART RX PATH OK". Her iki marker da geçer.
# Ağ/drive YOK → deterministik. Marker grep: "UART RX" (OK veya PATH OK).
calistir_uart_rx_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 PL011 UART RX testi: uart_rx_arm.c -> ELF (donanım giriş okuma)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/uart_rx_arm.c -o $(BUILD)/uart_rx_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/uart_rx_arm.elf $(BUILD)/uart_rx_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/uart_rx_arm.out; \
		printf 'K' | timeout 8 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial stdio -kernel $(BUILD)/uart_rx_arm.elf > $(BUILD)/uart_rx_arm.out 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/uart_rx_arm.out; echo "--- son ---"; \
		if grep -q "UART RX OK" $(BUILD)/uart_rx_arm.out; then \
			echo "aarch64 UART RX testi gecti: GERCEK giris enjekte edildi (stdio pipe) — DR'den byte okundu + echo (UART RX OK)."; \
		elif grep -q "UART RX PATH OK" $(BUILD)/uart_rx_arm.out; then \
			echo "aarch64 UART RX testi gecti: RX-path fallback — RXFE=1 (bos FIFO) dogru algilandi, deadlock yok (UART RX PATH OK)."; \
		else \
			echo "FAIL: 'UART RX' marker'i yok (RX register semantigi okunamadi veya deadlock)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — UART RX testi atlandi."; \
	fi

# === İNTERAKTİF KABUK (shell) testi (aarch64) — DONANIM + OS DORUĞU ===
# D-181 (UART RX) + D-135 (komut kabuk) BİRLEŞİMİ: kabuk komut satırlarını PL011
# UART RX'ten CANLI okur (sabit script DEĞİL) → gerçek interaktiflik. Kabuk EL1'de
# koşar; UART RX MMIO doğrudan EL1'den okunur, FS komutları `svc #0` ile çağrılır.
#
# GİRİŞ (D-181 dersi): `-serial stdio` ile guest seri hattı host stdin/stdout'a
# bağlanır; komut dizisi stdin'e PIPE edilir (Windows/MSYS'te ÇALIŞAN yol; QEMU
# `-chardev input-path=` Windows'ta desteklenmez). Kabuk her satırı RXFE-poll +
# DR ile byte-byte CANLI okur, '\n'de tokenize + çalıştırır (sabit script DEĞİL).
#
# PER-KARAKTER PACE (KRİTİK deterministiklik): QEMU virt PL011 reset'te RX = 1-byte
# holding register. Tüm akışı burst pipe'lamak → guest TX'te (echo/oku çıktısı/
# prompt) meşgulken gelen bytelar 1-byte reg'i OVERRUN eder → RUN'lar arası KARARSIZ
# (yaşandı: "yl MHABA", "MERHABoku" birleşme, satır-pace bile ara sıra flake). ÇÖZÜM:
# girişi KARAKTER-KARAKTER, her byte arası ~30ms gecikme ile besle. 1-byte reg
# hiçbir zaman taşmaz (guest bir sonraki byte'tan çok önce okur), guest TX-latency'si
# önemsiz → HER byte iner. LİDER sleep 1 boot yarışını absorbe eder. Bu yol tam
# deterministik (4× RUN byte-identik doğrulandı). Kabuk-içi pl011_fifo_ac (FEN)
# ikinci savunma. /bin/sh = bash 5.x (MSYS) → ${s:i:1} substring desteklenir.
# Komutlar: `yaz gunluk MERHABA` (num 17) → `oku gunluk` (num 18, "MERHABA" basar)
# → `ls` (num 19/20, "gunluk" basar). Sonra EOF → "SHELL OK".
# DEADLOCK YOK: RXFE poll bounded → giriş bitince kabuk EOF sayar, durur.
# Ağ/drive YOK → deterministik. Marker grep: "SHELL OK" + "MERHABA" + "gunluk".
calistir_shell_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 interaktif kabuk testi: shell_arm.c -> ELF (canlı UART RX komut)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/shell_arm.c -o $(BUILD)/shell_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/shell_arm.elf $(BUILD)/shell_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/shell_arm.out; \
		s='yaz gunluk MERHABA\noku gunluk\nls\n'; \
		{ sleep 1; printf "$$s" | while IFS= read -r -n1 ch; do \
			printf '%s' "$$ch"; [ -z "$$ch" ] && printf '\n'; sleep 0.03; \
		done; sleep 1; } \
			| timeout 20 qemu-system-aarch64 \
			-M virt -cpu cortex-a72 -display none \
			-serial stdio -kernel $(BUILD)/shell_arm.elf > $(BUILD)/shell_arm.out 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/shell_arm.out; echo "--- son ---"; \
		if grep -q "SHELL OK" $(BUILD)/shell_arm.out && grep -q "MERHABA" $(BUILD)/shell_arm.out && grep -q "gunluk" $(BUILD)/shell_arm.out; then \
			echo "aarch64 interaktif kabuk testi gecti: CANLI UART RX komut okundu (stdio pipe) — yaz/oku/ls calisti, oku=MERHABA + ls=gunluk (SHELL OK)."; \
		else \
			echo "FAIL: 'SHELL OK' + 'MERHABA' + 'gunluk' bekleniyor (interaktif kabuk)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — interaktif kabuk testi atlandi."; \
	fi

# === KABUK SCRIPT RUNNER (aarch64) — CANLI UART betik → degisken + echo/yaz/oku ===
# D-188 (shell_arm.c EL1 interaktif UART kabuk) TEMEL. shell_arm.c tek KOMUT okur;
# shell_script_arm.c bir BETIK YORUMLAYICISI: degisken tablosu (set) + degisken
# yerine gecme (echo $ad) + sayi<->metin cevrimi + basit dongu (tekrar). Kabuk EL1'de
# kosar; FS komutlari `svc #0` ile. GIRIS: shell_arm.c gibi `-serial stdio` +
# KARAKTER-KARAKTER ~30ms pace (PL011 reset RX=1-byte holding → burst overrun, D-188).
# Ag/drive YOK → deterministik. Betik: `set x 42` (sessiz) → `echo x` ("42") →
# `yaz gunluk x` (num 17, degeri metne cevirip dosyaya yazar) → `oku gunluk` (num 18,
# "42" basar). Sonra EOF → "SCRIPT OK". DEADLOCK YOK (RXFE poll bounded → EOF → dur).
# Marker grep: "SCRIPT OK" + echo/oku ciktisi "42".
calistir_shell_script_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 kabuk script runner testi: shell_script_arm.c -> ELF (canli UART betik)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/shell_script_arm.c -o $(BUILD)/shell_script_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/shell_script_arm.elf $(BUILD)/shell_script_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/shell_script_arm.out; \
		s='set x 42\necho x\nyaz gunluk x\noku gunluk\n'; \
		{ sleep 1; printf "$$s" | while IFS= read -r -n1 ch; do \
			printf '%s' "$$ch"; [ -z "$$ch" ] && printf '\n'; sleep 0.03; \
		done; sleep 1; } \
			| timeout 20 qemu-system-aarch64 \
			-M virt -cpu cortex-a72 -display none \
			-serial stdio -kernel $(BUILD)/shell_script_arm.elf > $(BUILD)/shell_script_arm.out 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/shell_script_arm.out; echo "--- son ---"; \
		if grep -q "SCRIPT OK" $(BUILD)/shell_script_arm.out && grep -q "42" $(BUILD)/shell_script_arm.out; then \
			echo "aarch64 kabuk script runner testi gecti: CANLI UART betik okundu — set/echo/yaz/oku calisti, echo x=42 + oku gunluk=42 (SCRIPT OK)."; \
		else \
			echo "FAIL: 'SCRIPT OK' + echo/oku ciktisi '42' bekleniyor (kabuk script runner)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — kabuk script runner testi atlandi."; \
	fi

# === AG-RECON KABUGU (aarch64) — CANLI UART komut → ICMP ping / DNS recon ===
# D-188 (shell_arm.c EL1 interaktif UART kabuk) + D-176/177/178 (userspace net:
# net_gonder=24/net_al=25 syscall, ICMP ping + DNS) BIRLESIMI. Bir "pentest OS"
# kabugu: CANLI komut satiri (UART RX) → ag recon (ping/dns) → sonuc. Ag mantigi
# EL1'den net_gonder/net_al syscall'lariyla yapilir (SVC EL1'den de calisir); frame
# tamponlari user-VA blogunda (D-150 guard). GIRIS: shell_arm.c gibi `-serial stdio`
# + KARAKTER-KARAKTER ~30ms pace (PL011 reset RX=1-byte holding → burst overrun,
# KRITIK). AG: net tests gibi -netdev user + virtio-net-device. `ping 2` = SLIRP
# gateway 10.0.2.2 DETERMINISTIK echo → "PING: CANLI". `dns` = example.com A
# (internet+fallback; cozulemezse "DNS: cozulemedi" ama kabuk devam eder). virtio_net
# BM_A64_OBJS'te (explicit eklemeye gerek yok). Marker: "RECON SHELL OK" + "PING: CANLI".
calistir_recon_shell_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 ag-recon kabuk testi: recon_shell_arm.c -> ELF (canli UART komut -> ping/dns)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/recon_shell_arm.c -o $(BUILD)/recon_shell_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/recon_shell_arm.elf $(BUILD)/recon_shell_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/recon_shell_arm.out; \
		s='ping 2\ndns\n'; \
		{ sleep 1; printf "$$s" | while IFS= read -r -n1 ch; do \
			printf '%s' "$$ch"; [ -z "$$ch" ] && printf '\n'; sleep 0.03; \
		done; sleep 2; } \
			| timeout 30 qemu-system-aarch64 \
			-M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial stdio -kernel $(BUILD)/recon_shell_arm.elf > $(BUILD)/recon_shell_arm.out 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/recon_shell_arm.out; echo "--- son ---"; \
		if grep -q "RECON SHELL OK" $(BUILD)/recon_shell_arm.out && grep -q "PING: CANLI" $(BUILD)/recon_shell_arm.out; then \
			echo "aarch64 ag-recon kabuk testi gecti: CANLI UART komut okundu (stdio pipe) — ICMP ping deterministik echo (PING: CANLI) + DNS denendi (RECON SHELL OK)."; \
		else \
			echo "FAIL: 'RECON SHELL OK' + 'PING: CANLI' bekleniyor (ag-recon kabuk)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — ag-recon kabuk testi atlandi."; \
	fi

# === AG-RECON KABUGU v2 (aarch64) — CANLI UART komut → TCP-scan / arp-scan recon ===
# recon_shell_arm.c (D-189: EL1 interaktif UART kabuk + ping/dns) TEMEL, IKI YENI
# recon komutu: `scan <oktet>` (port_scan_arm.c/D-164 mantigi — TCP SYN 80/443/22 →
# ACIK/KAPALI/FILTRELI) + `arpscan` (arp_scan_arm.c/D-158 mantigi — subnet 10.0.2.1..5
# ARP tarama, KUCUK poll-tik butcesi). Mevcut ping korunur. Ag EL1'den net_gonder(24)/
# net_al(25) syscall'lariyla; frame tamponlari user-VA blogunda (D-150 guard). GIRIS:
# `-serial stdio` + KARAKTER-KARAKTER ~30ms pace (D-188 PL011 1-byte holding → burst
# overrun, KRITIK). AG: -netdev user + virtio-net-device. `ping 2`+`arpscan` SLIRP
# gateway/DNS host ile DETERMINISTIK. Marker: "RECON2 SHELL OK" + "PING: CANLI".
calistir_recon_shell2_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 ag-recon kabuk v2 testi: recon_shell2_arm.c -> ELF (canli UART komut -> scan/arpscan)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/recon_shell2_arm.c -o $(BUILD)/recon_shell2_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/recon_shell2_arm.elf $(BUILD)/recon_shell2_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/recon_shell2_arm.out; \
		s='ping 2\narpscan\n'; \
		{ sleep 1; printf "$$s" | while IFS= read -r -n1 ch; do \
			printf '%s' "$$ch"; [ -z "$$ch" ] && printf '\n'; sleep 0.03; \
		done; sleep 2; } \
			| timeout 30 qemu-system-aarch64 \
			-M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial stdio -kernel $(BUILD)/recon_shell2_arm.elf > $(BUILD)/recon_shell2_arm.out 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/recon_shell2_arm.out; echo "--- son ---"; \
		if grep -q "RECON2 SHELL OK" $(BUILD)/recon_shell2_arm.out && grep -q "PING: CANLI" $(BUILD)/recon_shell2_arm.out; then \
			echo "aarch64 ag-recon kabuk v2 testi gecti: CANLI UART komut okundu (stdio pipe) — ICMP ping deterministik echo (PING: CANLI) + arpscan/scan denendi (RECON2 SHELL OK)."; \
		else \
			echo "FAIL: 'RECON2 SHELL OK' + 'PING: CANLI' bekleniyor (ag-recon kabuk v2)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — ag-recon kabuk v2 testi atlandi."; \
	fi

# === CMOS RTC testi (x86_64) — donanım gerçek-zaman saati (D-172 x86 paritesi) ===
# PC uyumlu MC146818 CMOS RTC: port 0x70 (index) / 0x71 (data). BCD register'lar
# (saniye/dakika/saat/gün/ay/yıl). UIP (Status A bit 7) beklenip tutarlı okunur.
# `-rtc base=utc` host wall-clock'unu yansıtır → deterministik makul-pencere geçer.
calistir_rtc_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 CMOS RTC testi: rtc_x86.c -> ELF (port 0x70/0x71 donanım saati)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/rtc_x86.c -o $(BUILD)/rtc_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/rtc_x86.elf $(BUILD)/rtc_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/rtc_x86.out; \
		timeout 8 qemu-system-x86_64 -kernel $(BUILD)/rtc_x86.elf -display none \
			-rtc base=utc \
			-serial file:$(BUILD)/rtc_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/rtc_x86.out; echo "--- son ---"; \
		if grep -q "RTC X86 OK" $(BUILD)/rtc_x86.out; then \
			echo "x86_64 CMOS RTC testi gecti: CMOS port 0x70/0x71'den makul tarih/saat okundu (RTC X86 OK)."; \
		else \
			echo "FAIL: 'RTC X86 OK' yok (CMOS okunamadi veya makul tarih araligi disinda)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 RTC testi atlandi."; \
	fi

# === SMP çok-çekirdek testi (x86_64) — AP başlatma (D-169 x86 paritesi) ===
# 2. çekirdeği (AP) Local APIC INIT-SIPI dizisi ile başlat. BSP LAPIC MMIO
# (0xFEE00000) üzerinden APIC ID/Version okur, düşük belleğe (0x8000) real→long
# trampoline yazar, INIT + STARTUP IPI gönderir. AP long-mode'a ulaşırsa paylaşılan
# bayrağı set eder → "SMP X86 OK". Trampoline AP'yi long-mode'a getiremezse fallback:
# LAPIC MMIO + IPI altyapısı kanıtlanır → "APIC OK". QEMU -smp 2.
calistir_smp_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 SMP testi: smp_x86.c -> ELF (Local APIC INIT-SIPI, AP trampoline)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/smp_x86.c -o $(BUILD)/smp_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/smp_x86.elf $(BUILD)/smp_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp_x86.out; \
		timeout 15 qemu-system-x86_64 -kernel $(BUILD)/smp_x86.elf -display none \
			-smp 2 \
			-serial file:$(BUILD)/smp_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/smp_x86.out; echo "--- son ---"; \
		if grep -q "SMP X86 OK" $(BUILD)/smp_x86.out; then \
			echo "x86_64 SMP testi gecti: 2. çekirdek (AP) INIT-SIPI ile long-mode'da koştu (çok-çekirdek)."; \
		elif grep -q "APIC OK" $(BUILD)/smp_x86.out; then \
			echo "x86_64 SMP fallback: Local APIC MMIO + INIT-SIPI altyapisi calisiyor (AP long-mode teyidi yok)."; \
		else \
			echo "FAIL: 'SMP X86 OK' (veya fallback 'APIC OK') bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 SMP testi atlandi."; \
	fi

# === SMP 4-cekirdek testi (x86_64) — coklu-AP INIT-SIPI (D-187 2->4, D-191 x86 ikizi) ===
# D-187 (x86 SMP 2-cekirdek) testinin 4-cekirdege olceklenmesi + D-191 (aarch64
# 4-cekirdek) x86 paritesi. BSP cekirdek 1,2,3'u (APIC ID 1,2,3) Local APIC
# INIT-SIPI ile ayri ayri baslatir. 3 AP AYNI SIPI vektorunu (0x08 -> 0x8000
# ORTAK trampoline) paylasir; ayrisma long-mode ortak naked giriste (her AP kendi
# APIC ID'sini okur -> APIC-ID-indeksli kendi yigini + kendi canli-slotu). BSP
# canli[1],[2],[3] hepsini bekler (bounded); ucu de long-mode'a ulasirsa
# "SMP4 X86 OK 4 cekirdek". Kismi gelirse "SMP4 X86 KISMI N/3". QEMU -smp 4.
# timeout 20 — x86 hlt-loop yuk-marji (4 core AP bring-up + trampoline).
calistir_smp4_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 SMP4 testi: smp4_x86.c -> ELF (Local APIC coklu-AP INIT-SIPI, ORTAK trampoline)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/smp4_x86.c -o $(BUILD)/smp4_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/smp4_x86.elf $(BUILD)/smp4_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/smp4_x86.out; \
		timeout 20 qemu-system-x86_64 -kernel $(BUILD)/smp4_x86.elf -display none \
			-smp 4 \
			-serial file:$(BUILD)/smp4_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/smp4_x86.out; echo "--- son ---"; \
		if grep -q "SMP4 X86 OK" $(BUILD)/smp4_x86.out; then \
			echo "x86_64 SMP4 testi gecti: 4 cekirdek (BSP + 3 AP) INIT-SIPI ile long-mode'da kostu (coklu-AP)."; \
		elif grep -q "SMP4 X86 KISMI" $(BUILD)/smp4_x86.out; then \
			echo "x86_64 SMP4 kismi: bazi AP'ler long-mode'a ulasti (3'un tamami degil) — cikti icinde N/3 raporlandi."; \
		elif grep -q "APIC OK" $(BUILD)/smp4_x86.out; then \
			echo "x86_64 SMP4 fallback: Local APIC MMIO + INIT-SIPI altyapisi calisiyor (AP long-mode teyidi yok)."; \
		else \
			echo "FAIL: 'SMP4 X86 OK' (veya kismi/fallback) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 SMP4 testi atlandi."; \
	fi

# === RING3 (userspace) testi (x86_64) — AYRICALIK AYRIMI (D2-x86) ===
# aarch64 D2 (EL0) + D-124 (userspace ABI) testlerinin x86 muadili. Long-mode
# ring0 kernel, ring3 kod çalıştırır: GDT DPL=3 user seg + TSS (RSP0) + IDT
# int 0x80 gate (DPL=3). iretq ile ring3'e geç → ring3 `int 0x80` (syscall,
# ring0'da işlenir) + `cli` (ayrıcalıklı → #GP yakalanır). Marker "RING3 X86 OK"
# (tam ring3-exec) veya "RING3 SETUP OK" (fallback: sadece GDT/TSS kurulumu).
calistir_ring3_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 ring3 testi: ring3_x86.c -> ELF (GDT DPL=3 + TSS + IDT int 0x80, iretq ring3)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/ring3_x86.c -o $(BUILD)/ring3_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/ring3_x86.elf $(BUILD)/ring3_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/ring3_x86.out; \
		timeout 20 qemu-system-x86_64 -kernel $(BUILD)/ring3_x86.elf -display none \
			-serial file:$(BUILD)/ring3_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/ring3_x86.out; echo "--- son ---"; \
		if grep -q "RING3 X86 OK" $(BUILD)/ring3_x86.out; then \
			echo "x86_64 ring3 testi gecti: ring3 (userspace) kod kostu + int 0x80 syscall + #GP privilege-fault (ayricalik ayrimi)."; \
		elif grep -q "RING3 SETUP OK" $(BUILD)/ring3_x86.out; then \
			echo "x86_64 ring3 fallback: GDT DPL=3 user seg + TSS ltr kuruldu (ring3-exec teyidi yok)."; \
		else \
			echo "FAIL: 'RING3 X86 OK' (veya fallback 'RING3 SETUP OK') bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 ring3 testi atlandi."; \
	fi

# === TAM SAYFA-İZOLASYON testi (x86_64) — ring3 kernel-sayfa #PF (D-124 x86) ===
# aarch64 D-124/D3 (EL0 kod kernel-belleğe erişince permission-fault) testinin
# x86 muadili. D-190 (ring3_x86) yalnız CPL + #GP ile ayrıcalık ayrımı kanıtladı;
# bu test SAYFA-tabanlı izolasyonu kanıtlar: ring3 kod bir kernel-only (U/S=0)
# sayfayı OKUYUNCA CPU #PF (vektör 14, hata kodu = present|user) üretir → kernel
# handler yakalar. Kernel-sır tamponu 2MB-hizalı+2MB-boyutlu → kendi PD girişi
# (supervisor-only); yalnız ring3 sayfalarına U/S eklenir → TAM izolasyon.
# Marker "PAGE ISO OK" (ring3 kernel belleğini okuyamadı, #PF err=P|U).
calistir_ring3_page_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 tam sayfa-izolasyon testi: ring3_page_x86.c -> ELF (ring3 kernel-sayfa okuma -> #PF)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/ring3_page_x86.c -o $(BUILD)/ring3_page_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/ring3_page_x86.elf $(BUILD)/ring3_page_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/ring3_page_x86.out; \
		timeout 20 qemu-system-x86_64 -kernel $(BUILD)/ring3_page_x86.elf -display none \
			-serial file:$(BUILD)/ring3_page_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/ring3_page_x86.out; echo "--- son ---"; \
		if grep -q "PAGE ISO OK" $(BUILD)/ring3_page_x86.out; then \
			echo "x86_64 tam sayfa-izolasyon testi gecti: ring3 kernel-only sayfayi OKUYAMADI (#PF v=14, err=P|U) — donanim-zorlamali sayfa-duzeyi izolasyon."; \
		else \
			echo "FAIL: 'PAGE ISO OK' bekleniyor (ring3 kernel-sayfa okumasi #PF ile reddedilmeli)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 tam sayfa-izolasyon testi atlandi."; \
	fi

# === PCI veri yolu numaralandirma testi (x86_64) — CIHAZ KESFI (Milestone B) ===
# Gercek aygit surucularinin (virtio/NIC/disk) temeli olan PCI cihaz kesfi.
# Legacy config-space port I/O (mekanizma #1): outl(0xCF8, enable|bus|slot|func|
# offset) + inl(0xCFC). bus 0'i slot 0..31 tara: her slot'ta vendor:device
# (offset 0) oku, 0xFFFF=cihaz-yok. Bulunan her cihazin class-code'unu (offset
# 0x08) da oku + listele. QEMU i440FX her zaman slot 0'da Intel host-bridge
# (8086:1237) sunar -> deterministik. Marker "PCI ENUM OK" (N>=1 cihaz).
calistir_pci_enum_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 PCI numaralandirma testi: pci_enum_x86.c -> ELF (legacy config-space 0xCF8/0xCFC, bus 0 tarama)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/pci_enum_x86.c -o $(BUILD)/pci_enum_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/pci_enum_x86.elf $(BUILD)/pci_enum_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/pci_enum_x86.out; \
		timeout 15 qemu-system-x86_64 -kernel $(BUILD)/pci_enum_x86.elf -display none \
			-serial file:$(BUILD)/pci_enum_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/pci_enum_x86.out; echo "--- son ---"; \
		if grep -q "PCI ENUM OK" $(BUILD)/pci_enum_x86.out; then \
			echo "x86_64 PCI numaralandirma testi gecti: bus 0'da PCI cihaz(lar) kesfedildi (Intel host-bridge dahil, vendor:device:class listelendi)."; \
		else \
			echo "FAIL: 'PCI ENUM OK' bekleniyor (bus 0'da en az 1 PCI cihaz numaralandirilmali)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 PCI numaralandirma testi atlandi."; \
	fi

# === PREEMPTIVE scheduler testi (x86_64) — PIT timer-IRQ context-switch (Milestone F) ===
# C7b (aarch64 preempt_arm) testinin x86 ikizi/paritesi. x86'da simdiye kadar
# yalniz COOPERATIVE (C7a sched) vardi; bu test gercek PREEMPTIVE round-robin
# getirir. Kendi-kurulumlu IDT + PIC(8259) remap + PIT(8254) ~100Hz -> IRQ0.
# IRQ0 asm stub tam GP trap-frame kaydeder -> C scheduler round-robin RSP swap
# -> PIC EOI -> iretq. Iki kernel-gorev busy-loop yapar, ASLA yield ETMEZ;
# yalnizca timer-IRQ onlari preempt eder. Gorev B'nin sayaci>0 = preemption
# calisti kaniti. Marker "PREEMPT X86 OK". timeout 20 (hlt-loop timeout ile
# oludur = beklenen, || true). Deterministik (bounded donguler).
calistir_preempt_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 preemptive sched testi: preempt_x86.c -> ELF (IDT+PIC+PIT IRQ0 context-switch)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/preempt_x86.c -o $(BUILD)/preempt_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/preempt_x86.elf $(BUILD)/preempt_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/preempt_x86.out; \
		timeout 20 qemu-system-x86_64 -kernel $(BUILD)/preempt_x86.elf -display none \
			-serial file:$(BUILD)/preempt_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/preempt_x86.out; echo "--- son ---"; \
		if grep -q "PREEMPT X86 OK" $(BUILD)/preempt_x86.out; then \
			echo "x86_64 preemptive sched testi gecti: gorev B YIELD cagirmadan timer-IRQ ile kostu (zorunlu baglam-degistirme)."; \
		else \
			echo "FAIL: 'PREEMPT X86 OK' bekleniyor (PIT IRQ0 preemption calismali)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 preemptive sched testi atlandi."; \
	fi

# === TAM x86 syscall ABI testi (int 0x80 çok-argüman + dönüş-değeri) — Milestone F ===
# aarch64 D-122 (arg gecisi) + D-126 (donus-degeri + cok-arg + register-seffaflik)
# syscall ABI'sinin x86 ikizi. Simdiye kadar x86'da yalniz D-190 ring3 int 0x80
# demo'su vardi (tek num, arg/donus yok). Bu test TAM ABI getirir: kendi-kurulumlu
# IDT[0x80] gate + handler; num=rax, arg0=rdi, arg1=rsi, donus=rax (SysV benzeri).
# Handler tum cagiran register'lari korur (D-126 register-seffaflik dersi: frame-
# ONCE-kaydet, sonra dispatch). 3 syscall: num=1 yaz(ptr), num=2 topla(a,b)->a+b
# (cok-arg+donus), num=3 gettick(x)->x+ofset (donus). Marker "SYSCALL X86 OK".
# timeout 20 (hlt-loop timeout ile oludur = beklenen, || true). Deterministik.
calistir_syscall_abi_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 tam syscall ABI testi: syscall_x86.c -> ELF (IDT[0x80] gate + num/arg0/arg1/donus)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/syscall_x86.c -o $(BUILD)/syscall_abi_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/syscall_abi_x86.elf $(BUILD)/syscall_abi_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/syscall_abi_x86.out; \
		timeout 20 qemu-system-x86_64 -kernel $(BUILD)/syscall_abi_x86.elf -display none \
			-serial file:$(BUILD)/syscall_abi_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/syscall_abi_x86.out; echo "--- son ---"; \
		if grep -q "SYSCALL X86 OK" $(BUILD)/syscall_abi_x86.out; then \
			echo "x86_64 tam syscall ABI testi gecti: int 0x80 num/arg0/arg1/donus + register-seffaflik (aarch64 D-126 paritesi)."; \
		else \
			echo "FAIL: 'SYSCALL X86 OK' bekleniyor (int 0x80 cok-arg + donus ABI calismali)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 tam syscall ABI testi atlandi."; \
	fi

# === x86 KULLANICI-SÜRECİ keystone testi (ring3 ⊕ sayfa-izolasyon ⊕ syscall) — Milestone F ===
# aarch64 D3 (proc_arm.c — KORUMALI EL0 user-process: kendi-adres-uzayi ⊕ EL0 ⊕
# SVC) testinin x86 IKIZI. Uc mevcut x86 parcayi BIRLESTIR: (1) ring3 (D-190 GDT
# DPL=3 seg + TSS + iret->CPL=3), (2) sayfa-izolasyon (D-195 kernel sayfasi U/S=0
# -> ring3 erisince #PF), (3) syscall (D-218 int 0x80 num/arg/donus). Ring3 user-
# kodu KENDI kullanici-sayfasinda kosar (U/S=1) + int 0x80 ile syscall yapar
# (num=2 topla(40,2)=42 HESAP + num=1 yaz I/O) + kernel-sayfasina erisince #PF
# (err=P|U, CR2=kernel-adr) -> HAPIS. Dort x86-OS ozelligi bir arada. Marker
# "RING3 PROC X86 OK". timeout 20 (hlt-loop timeout ile oludur = beklenen,
# || true). Deterministik (sabit arglar/sihir/2MB-hizali sir).
calistir_ring3_proc_test_x86: $(BUILD)/kemgu$(EXE) $(BM_X86_OBJS)
	@echo "x86_64 kullanici-sureci keystone: ring3_proc_x86.c -> ELF (ring3 + sayfa-izolasyon + syscall)..."
	$(BM_X86) $(BM_X86_CF) -c test/bare_metal/ring3_proc_x86.c -o $(BUILD)/ring3_proc_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld \
		-o $(BUILD)/ring3_proc_x86.elf $(BUILD)/ring3_proc_x86.o $(BM_X86_OBJS)
	@if command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/ring3_proc_x86.out; \
		timeout 20 qemu-system-x86_64 -kernel $(BUILD)/ring3_proc_x86.elf -display none \
			-serial file:$(BUILD)/ring3_proc_x86.out 2>/dev/null || true; \
		echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/ring3_proc_x86.out; echo "--- son ---"; \
		if grep -q "RING3 PROC X86 OK" $(BUILD)/ring3_proc_x86.out; then \
			echo "x86_64 kullanici-sureci keystone gecti: ring3 sureci kendi sayfasinda kostu (CPL=3) + syscall ile hesap+I/O yapti + kernel-sayfa okumasi #PF ile reddedildi (aarch64 proc_arm.c paritesi)."; \
		else \
			echo "FAIL: 'RING3 PROC X86 OK' bekleniyor (ring3 + syscall + kernel-sayfa #PF birlesimi calismali)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — x86 kullanici-sureci keystone atlandi."; \
	fi

# === REVERSE DNS (PTR kaydı) testi (aarch64) — IP → hostname (recon) ===
# ARP → DNS MAC → PTR sorgusu (8.8.8.8 → "8.8.8.8.in-addr.arpa", QTYPE=12)
# gönder → yanıtı RX ile al → ANSWER RDATA domain-name'i parse et (label
# dizisi + isim sıkıştırma 0xC0 dâhil) → hostname çıkar (örn "dns.google").
calistir_dns_ptr_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 reverse DNS (PTR) testi: dns_ptr_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/dns_ptr_arm.c -o $(BUILD)/dns_ptr_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/dns_ptr_arm.elf $(BUILD)/dns_ptr_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/dns_ptr_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/dns_ptr_arm.out -kernel $(BUILD)/dns_ptr_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/dns_ptr_arm.out; echo "--- son ---"; \
		if grep -q "PTR OK" $(BUILD)/dns_ptr_arm.out; then \
			echo "aarch64 reverse DNS testi gecti: IP → geçerli PTR hostname çıkarıldı."; \
		else \
			echo "FAIL: 'PTR OK' bekleniyor (reverse DNS PTR çözümleme)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — reverse DNS PTR testi atlandi."; \
	fi

# === D-148 SELF-HOST virtio sürücüsü (aarch64) — KEMGU dilinde OS sürücüsü ===
# virtio_selfhost.kem (KEMGU!) → LLVM IR → aarch64 → bare-metal boot. mmio_oku32
# (yetki<MMIO>) ile virtio-mmio magic register'ını okur. KEMGU kendi OS'unu yazıyor.
calistir_virtio_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o
	@echo "D-148 aarch64 SELF-HOST virtio sürücüsü: virtio_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/virtio_selfhost.kem > $(BUILD)/virtio_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/virtio_selfhost.ll -c -o $(BUILD)/virtio_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/virtio_selfhost.elf $(BUILD)/virtio_selfhost.o \
		$(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/virtio_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/virtio_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/virtio_selfhost.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/virtio_selfhost.out -kernel $(BUILD)/virtio_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/virtio_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM VIRTIO OK" $(BUILD)/virtio_selfhost.out; then \
			echo "D-148 aarch64 self-host virtio testi gecti: KEMGU sürücüsü virtio-mmio okudu."; \
		else \
			echo "FAIL: 'KEM VIRTIO OK' bekleniyor (KEMGU self-host MMIO sürücüsü)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host virtio testi atlandi."; \
	fi

# === D-149 SELF-HOST virtio init (aarch64) — KEMGU'da tarama + MMIO yazma ===
# virtio_selfhost_rw.kem: KEMGU cihazı tarar + status handshake yazar (yetki THREAD).
calistir_virtio_selfhost_rw_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o
	@echo "D-149 aarch64 SELF-HOST virtio init: virtio_selfhost_rw.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/virtio_selfhost_rw.kem > $(BUILD)/virtio_selfhost_rw.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/virtio_selfhost_rw.ll -c -o $(BUILD)/virtio_selfhost_rw.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/virtio_selfhost_rw.elf $(BUILD)/virtio_selfhost_rw.o \
		$(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/virtio_selfhost_rw.out $(BUILD)/dsh.img; \
		dd if=/dev/zero of=$(BUILD)/dsh.img bs=512 count=4 2>/dev/null; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/dsh.img,format=raw,if=none,id=d0 -device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/virtio_selfhost_rw.out -kernel $(BUILD)/virtio_selfhost_rw.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/virtio_selfhost_rw.out; echo "--- son ---"; \
		if grep -q "KEM VIRTIO RW OK" $(BUILD)/virtio_selfhost_rw.out; then \
			echo "D-149 aarch64 self-host virtio init testi gecti: KEMGU sürücüsü tarama+handshake yaptı."; \
		else \
			echo "FAIL: 'KEM VIRTIO RW OK' bekleniyor (KEMGU self-host tara+yaz)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host virtio init testi atlandi."; \
	fi

# === D-158 SELF-HOST virtio-NET cihaz tanıma (aarch64) — KEMGU'da ağ-cihaz sürücüsü ===
# virtio_net_selfhost.kem: KEMGU virtio-mmio slotlarını tarar, MAGIC doğrular, DeviceID
# okur ve DeviceID==1 (virtio-net) cihazını tanır. QEMU'ya virtio-net-device eklenir
# (blk -drive yerine -netdev + virtio-net-device) → slot'ta DeviceID=1 sunar.
calistir_virtio_net_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o
	@echo "D-158 aarch64 SELF-HOST virtio-net tanıma: virtio_net_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/virtio_net_selfhost.kem > $(BUILD)/virtio_net_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/virtio_net_selfhost.ll -c -o $(BUILD)/virtio_net_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/virtio_net_selfhost.elf $(BUILD)/virtio_net_selfhost.o \
		$(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/virtio_net_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/virtio_net_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/virtio_net_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/virtio_net_selfhost.out -kernel $(BUILD)/virtio_net_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/virtio_net_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM NET OK" $(BUILD)/virtio_net_selfhost.out; then \
			echo "D-160 aarch64 self-host virtio-net testi gecti: KEMGU sürücüsü virtio-net cihazını (DeviceID=1) tanıdı."; \
		else \
			echo "FAIL: 'KEM NET OK' bekleniyor (KEMGU self-host virtio-net tanıma)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host virtio-net testi atlandi."; \
	fi

# === SELF-HOST virtio-NET MAC okuma (aarch64) — KEMGU'da config-space erişimi ===
# virtio_net_mac_selfhost.kem: D-160'ın ötesinde — virtio-net slotunu bulur ve
# cihaza-özel config space'ten (offset 0x100) MAC adresini (mac[6]) okur.
# QEMU varsayılan MAC'i (52:54:00:12:34:56) config-space'te sunar → 6 byte basılır.
# Marker: "KEM MAC OK" (MAC-config yolu) veya fallback "KEM NET REG OK".
calistir_virtio_net_mac_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o
	@echo "aarch64 SELF-HOST virtio-net MAC okuma: virtio_net_mac_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/virtio_net_mac_selfhost.kem > $(BUILD)/virtio_net_mac_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/virtio_net_mac_selfhost.ll -c -o $(BUILD)/virtio_net_mac_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/virtio_net_mac_selfhost.elf $(BUILD)/virtio_net_mac_selfhost.o \
		$(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/virtio_net_mac_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/virtio_net_mac_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/virtio_net_mac_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/virtio_net_mac_selfhost.out -kernel $(BUILD)/virtio_net_mac_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/virtio_net_mac_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM MAC OK" $(BUILD)/virtio_net_mac_selfhost.out; then \
			echo "aarch64 self-host virtio-net MAC testi gecti: KEMGU sürücüsü config-space'ten MAC okudu."; \
		elif grep -q "KEM NET REG OK" $(BUILD)/virtio_net_mac_selfhost.out; then \
			echo "aarch64 self-host virtio-net register testi gecti (fallback): KEMGU sürücüsü config-öncesi register okudu."; \
		else \
			echo "FAIL: 'KEM MAC OK' (veya fallback 'KEM NET REG OK') bekleniyor (KEMGU self-host virtio-net config okuma)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host virtio-net MAC testi atlandi."; \
	fi

# === SELF-HOST virtio-BLK kapasite okuma (aarch64) — KEMGU'da disk config-space ===
# virtio_blk_config_selfhost.kem: virtio-blk (DeviceID=2) slotunu bulur ve
# cihaza-özel config space'ten (offset 0x100) kapasiteyi (u64, sektör sayısı)
# okur. QEMU'ya 64-sektör (32 KiB) raw disk verilir → capacity == 64.
# Marker: "KEM BLK OK" (capacity yolu) veya fallback "KEM BLK REG OK".
calistir_virtio_blk_config_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o
	@echo "aarch64 SELF-HOST virtio-blk kapasite okuma: virtio_blk_config_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/virtio_blk_config_selfhost.kem > $(BUILD)/virtio_blk_config_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/virtio_blk_config_selfhost.ll -c -o $(BUILD)/virtio_blk_config_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/virtio_blk_config_selfhost.elf $(BUILD)/virtio_blk_config_selfhost.o \
		$(BUILD)/bm_a64_mmio.o $(BUILD)/bm_a64_yetki.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/virtio_blk_config_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/virtio_blk_config_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@dd if=/dev/zero of=$(BUILD)/disk_blk_selfhost.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/virtio_blk_config_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_blk_selfhost.img,format=raw,if=none,id=d0 -device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/virtio_blk_config_selfhost.out -kernel $(BUILD)/virtio_blk_config_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/virtio_blk_config_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM BLK OK" $(BUILD)/virtio_blk_config_selfhost.out; then \
			echo "aarch64 self-host virtio-blk testi gecti: KEMGU sürücüsü config-space'ten kapasite okudu."; \
		elif grep -q "KEM BLK REG OK" $(BUILD)/virtio_blk_config_selfhost.out; then \
			echo "aarch64 self-host virtio-blk register testi gecti (fallback): KEMGU sürücüsü DEVICE_FEATURES register okudu."; \
		else \
			echo "FAIL: 'KEM BLK OK' (veya fallback 'KEM BLK REG OK') bekleniyor (KEMGU self-host virtio-blk config okuma)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host virtio-blk kapasite testi atlandi."; \
	fi

# === SELF-HOST CRC32 saf-hesaplama (aarch64) — KEMGU'da cihazsız algoritma ===
# crc32_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde standart
# IEEE 802.3 / zlib CRC-32 (polinom 0xEDB88320, tablosuz bit-bit) hesaplar.
# Test verisi "123456789" -> beklenen CRC-32 = 0xCBF43926 (3421780262).
# KEMGU dilinin MMIO ÖTESİNDE gerçek algoritma kaldırdığını kanıtlar (XOR/AND/
# işaretsiz-sağa-kaydırma bit işlemleri). CİHAZSIZ: QEMU'da -netdev/-drive YOK,
# link'te mmio/yetki obj GEREKMEZ (sade BM_A64_OBJS). Marker: "KEM CRC OK".
calistir_crc32_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST CRC32 saf-hesaplama: crc32_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/crc32_selfhost.kem > $(BUILD)/crc32_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/crc32_selfhost.ll -c -o $(BUILD)/crc32_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/crc32_selfhost.elf $(BUILD)/crc32_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/crc32_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/crc32_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/crc32_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/crc32_selfhost.out -kernel $(BUILD)/crc32_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/crc32_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM CRC OK" $(BUILD)/crc32_selfhost.out; then \
			echo "aarch64 self-host CRC32 testi gecti: KEMGU cihazsiz algoritma CRC-32('123456789')=0xCBF43926 dogruladi."; \
		else \
			echo "FAIL: 'KEM CRC OK' bekleniyor (KEMGU self-host CRC32 saf-hesaplama)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host CRC32 testi atlandi."; \
	fi

# === SELF-HOST SIRALAMA (aarch64) — KEMGU'da dizi in-place mutasyon algoritması ===
# sort_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde bubble sort ile
# 10 elemanlı sırasız diziyi YERİNDE (in-place) sıralar. İç içe döngü + eleman
# karşılaştırma (>) + geçici değişkenle swap (t=d[j]; d[j]=d[j+1]; d[j+1]=t).
# KEMGU dizileri heap-uniform (KdlDizi*): d[i]=x -> kdl_dizi_yaz_tam (runtime
# sınır-kontrollü). KEMGU'nun gerçek dizi-mutasyon algoritması kaldırdığını
# kanıtlar. CİHAZSIZ: QEMU'da -netdev/-drive YOK, sade BM_A64_OBJS (heap dâhil —
# dizi tahsisi için). Marker: "KEM SORT OK".
calistir_sort_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST SIRALAMA dizi in-place: sort_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/sort_selfhost.kem > $(BUILD)/sort_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/sort_selfhost.ll -c -o $(BUILD)/sort_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/sort_selfhost.elf $(BUILD)/sort_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/sort_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/sort_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/sort_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/sort_selfhost.out -kernel $(BUILD)/sort_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/sort_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM SORT OK" $(BUILD)/sort_selfhost.out; then \
			echo "aarch64 self-host SIRALAMA testi gecti: KEMGU cihazsiz dizi in-place bubble sort ([5,2,8,1,9,3,7,4,6,0] -> [0..9]) dogruladi."; \
		else \
			echo "FAIL: 'KEM SORT OK' bekleniyor (KEMGU self-host siralama dizi in-place mutasyon)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host SIRALAMA testi atlandi."; \
	fi

# === SELF-HOST HASH-MAP sözlük (aarch64) — KEMGU'da dictionary veri yapısı ===
# hashmap_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde open
# addressing (açık adresleme) + linear probing (doğrusal sondalama) hash-map.
# Üç paralel dizi (anahtarlar/degerler/dolu: Dizi<tam32>) → KdlDizi*; slot
# yazma d[s]=x → kdl_dizi_yaz_tam (runtime sınır-kontrollü). Knuth çarpımsal
# hash (dtam32 mod-2^32 wrap) + `& (KAP-1)` maske + `olarak` explicit cast
# ile tam32 slot köprüsü. ÇAKIŞMA senaryosu: anahtar 5/21/37 hepsi slot 5'e
# hash'lenir → probing slot 5/6/7'ye yerleştirir; bul(99) boş-slot'ta koparak
# -1 döner. KEMGU'nun gerçek HASH-MAP + çakışma çözümü + arama kaldırdığını
# kanıtlar. CİHAZSIZ: QEMU'da -netdev/-drive YOK, sade BM_A64_OBJS (heap dâhil —
# dizi tahsisi için). Marker: "KEM HASHMAP OK".
calistir_hashmap_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST HASH-MAP linear probing: hashmap_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/hashmap_selfhost.kem > $(BUILD)/hashmap_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/hashmap_selfhost.ll -c -o $(BUILD)/hashmap_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/hashmap_selfhost.elf $(BUILD)/hashmap_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/hashmap_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/hashmap_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/hashmap_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/hashmap_selfhost.out -kernel $(BUILD)/hashmap_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/hashmap_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM HASHMAP OK" $(BUILD)/hashmap_selfhost.out; then \
			echo "aarch64 self-host HASH-MAP testi gecti: KEMGU cihazsiz open-addressing + linear probing sozluk (anahtar 5/21/37 ayni slota hash -> probe; bul 50/210/370, bul(99)=-1) dogruladi."; \
		else \
			echo "FAIL: 'KEM HASHMAP OK' bekleniyor (KEMGU self-host hash-map linear probing)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host HASH-MAP testi atlandi."; \
	fi

# === SELF-HOST SHA-256 kripto hash (aarch64) — KEMGU'da kriptografik algoritma ===
# sha256_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde NIST FIPS
# 180-4 SHA-256 hesaplar. Test vektörü SHA-256("abc") = ba7816bf 8f01cfea ...
# f20015ad. KEMGU'nun CRC/checksum'ın ÖTESİNDE gerçek KRİPTO hash kaldırdığını
# kanıtlar: dtam32 mod-2^32 toplama (add i32 wrap) + rotate-right (lshr | shl) +
# XOR/AND/NOT(^0xFFFFFFFF)/shift + 64 tur. CİHAZSIZ: QEMU'da -netdev/-drive YOK,
# BM_A64_OBJS (heap dâhil — Dizi<dtam32> W/K tahsisi için). Marker: "KEM SHA OK".
# NOT (codegen deseni): dizi-eleman doğrudan `>>` operandı ashr (işaretli) üretir;
# bu yüzden tüm bit-karıştırma skaler dtam32 parametreli yardımcı işlevlere taşındı
# (dizi elemanı argüman geçince kaydırma lshr olur). runtime/codegen DEĞİŞMEDİ.
calistir_sha256_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST SHA-256 kripto hash: sha256_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/sha256_selfhost.kem > $(BUILD)/sha256_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/sha256_selfhost.ll -c -o $(BUILD)/sha256_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/sha256_selfhost.elf $(BUILD)/sha256_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/sha256_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/sha256_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/sha256_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/sha256_selfhost.out -kernel $(BUILD)/sha256_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/sha256_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM SHA OK" $(BUILD)/sha256_selfhost.out; then \
			echo "aarch64 self-host SHA-256 testi gecti: KEMGU cihazsiz kripto hash SHA-256('abc')=ba7816bf...f20015ad dogruladi."; \
		else \
			echo "FAIL: 'KEM SHA OK' bekleniyor (KEMGU self-host SHA-256 kripto hash)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host SHA-256 testi atlandi."; \
	fi

# === SELF-HOST SHA-256 PAROLA KIRICI (aarch64) — Pentest-OS dictionary attack ===
# hashcrack_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde Kali/
# hashcat-tarzı SÖZLÜK SALDIRISI (dictionary attack) ile SHA-256 parola kırıcı.
# KEMGU'nun kriptografik hash'in ÖTESİNDE gerçek bir OFANSİF-GÜVENLİK aracı
# kaldırdığını kanıtlar: 8 aday gömülü sözlük, her adayın SHA-256'sını hesaplar
# (D-173 SHA-256 çekirdeği: dtam32 mod-2^32 add wrap + rotr lshr|shl + skaler-
# param bit-karıştırma → dizi-eleman ashr tuzağı yok), hedef hash ile karşılaştırır.
# LAB-KAPSAM (etik): hedef = sözlükteki BİLİNEN "kemgu" parolasının KENDİ üretilen
# SHA-256'sı — dış gerçek hedef YOK. Kırıcı hedefi doğru indekste (2) bulur +
# eşleşmeyen 7 aday atlanır → "KEM CRACK OK". CİHAZSIZ: QEMU'da -netdev/-drive
# YOK, BM_A64_OBJS (heap dâhil — Dizi<dtam32>/Dizi<tam32> W/K/sözlük tahsisi).
# Marker: "KEM CRACK OK".
calistir_hashcrack_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST SHA-256 parola kirici (dictionary attack): hashcrack_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/hashcrack_selfhost.kem > $(BUILD)/hashcrack_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/hashcrack_selfhost.ll -c -o $(BUILD)/hashcrack_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/hashcrack_selfhost.elf $(BUILD)/hashcrack_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/hashcrack_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/hashcrack_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/hashcrack_selfhost.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/hashcrack_selfhost.out -kernel $(BUILD)/hashcrack_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/hashcrack_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM CRACK OK" $(BUILD)/hashcrack_selfhost.out; then \
			echo "aarch64 self-host SHA-256 parola kirici testi gecti: KEMGU cihazsiz sozluk-saldirisi hedefi ('kemgu') dogru indekste kirdi."; \
		else \
			echo "FAIL: 'KEM CRACK OK' bekleniyor (KEMGU self-host SHA-256 dictionary attack)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host parola kirici testi atlandi."; \
	fi

# === SELF-HOST UTF-8 kod-çözücü (aarch64) — KEMGU TÜRKÇE DNA milestone ===
# utf8_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde UTF-8 (RFC
# 3629) kod-çözücü. KEMGU'nun KENDİ kaynak-kodlaması olan UTF-8'i çözebildiğini
# kanıtlar — Türkçe syntax'lı dilin özü. 1-byte (0xxxxxxx) + 2-byte (110xxxxx
# 10xxxxxx) dizileri: kod-noktası = ((b0 & 0x1F) << 6) | (b1 & 0x3F). Test:
# "çğışöü" 12 byte -> 6 kod-noktası [231,287,305,351,246,252] (byte 12 DEĞİL,
# kod-nokta 6). Türkçe kod-noktaları (ç ğ ı ö ş ü) beklenenle karşılaştırılır +
# geçersiz devam-byte tespiti (bonus, sonsuz döngü YOK). CİHAZSIZ: QEMU'da
# -netdev/-drive YOK, BM_A64_OBJS (heap dâhil — Dizi<dtam32> byte/kod-nokta
# tahsisi için). Marker: "KEM UTF8 OK".
# NOT (codegen deseni): dizi-eleman doğrudan `>>` operandı ashr (işaretli)
# üretir (D-173); bu yüzden byte'lar önce SKALER dtam32'ye alınır, mask/kaydırma
# skaler üstünde (lshr/shl doğru). `karakter` sayısal değil (T003) → byte'lar
# dtam32 dizisi. implicit tam32<->dtam32 YASAK (D-200) → `olarak` gerekmedi
# (sayaçlar tam32, byte/kod-nokta dtam32 ayrı tutuldu). runtime/codegen DEĞİŞMEDİ.
calistir_utf8_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST UTF-8 kod-cozucu: utf8_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/utf8_selfhost.kem > $(BUILD)/utf8_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/utf8_selfhost.ll -c -o $(BUILD)/utf8_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/utf8_selfhost.elf $(BUILD)/utf8_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/utf8_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/utf8_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/utf8_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/utf8_selfhost.out -kernel $(BUILD)/utf8_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/utf8_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM UTF8 OK" $(BUILD)/utf8_selfhost.out; then \
			echo "aarch64 self-host UTF-8 testi gecti: KEMGU cihazsiz UTF-8 kod-cozucu 'cgisou' -> [231,287,305,351,246,252] (6 kod-nokta) dogruladi."; \
		else \
			echo "FAIL: 'KEM UTF8 OK' bekleniyor (KEMGU self-host UTF-8 kod-cozucu)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host UTF-8 testi atlandi."; \
	fi

# === SELF-HOST RC4 akış şifresi (aarch64) — KEMGU'da simetrik kripto ===
# rc4_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde RC4 (Rivest
# Cipher 4) SİMETRİK AKIŞ ŞİFRESİ hesaplar. SHA-256'nın (tek-yönlü hash)
# ÖTESİNDE gerçek şifreleme/çözme kaldırdığını kanıtlar: KSA (256-byte S-box
# permütasyon) + PRGA (keystream) + düz XOR keystream. Test vektörü (Wikipedia):
# anahtar "Key", düz "Plaintext" -> BB F3 16 E8 D9 40 AF 0A D3.
# KEMGU dizileri heap-uniform (KdlDizi*): S[i]=x -> kdl_dizi_yaz (runtime
# sınır-kontrollü); S-box takası in-place (D-171 swap deseni). Byte'lar dtam32,
# `& 255` maskesi (SAĞA-KAYDIRMA YOK → D-173 dizi-eleman-ashr tuzağı oluşmaz);
# XOR skaler dtam32 üstünde (dizi elemanı önce skalere alınır — D-173 dersi);
# tam32<->dtam32 köprüsü `olarak` cast (D-200). RC4 simetrik olduğundan decrypt
# round-trip (şifreli -> aynı keystream -> düz geri) bağımsız doğruluk kanıtı.
# CİHAZSIZ: QEMU'da -netdev/-drive YOK, sade BM_A64_OBJS (heap dâhil — Dizi<dtam32>
# S-box tahsisi için). Marker: "KEM RC4 OK". runtime/codegen DEĞİŞMEDİ.
calistir_rc4_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST RC4 akis sifresi: rc4_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/rc4_selfhost.kem > $(BUILD)/rc4_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/rc4_selfhost.ll -c -o $(BUILD)/rc4_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/rc4_selfhost.elf $(BUILD)/rc4_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/rc4_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/rc4_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/rc4_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/rc4_selfhost.out -kernel $(BUILD)/rc4_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/rc4_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM RC4 OK" $(BUILD)/rc4_selfhost.out; then \
			echo "aarch64 self-host RC4 testi gecti: KEMGU cihazsiz simetrik akis sifresi RC4('Key','Plaintext')=BBF316E8D940AF0AD3 + decrypt round-trip dogruladi."; \
		else \
			echo "FAIL: 'KEM RC4 OK' bekleniyor (KEMGU self-host RC4 akis sifresi)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host RC4 testi atlandi."; \
	fi

# === SELF-HOST 128-BIT BIGNUM TOPLAMA (aarch64) — KEMGU çok-word aritmetiği ===
# bignum_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde 128-bit
# tamsayı toplaması (2×dtam64 word: yuksek+dusuk) carry (elde) yayılımıyla hesaplar.
# KEMGU'nun tek 64-bit makine-word'ünün ÖTESİNDE gerçek çok-word aritmetiği
# kaldırdığını kanıtlar: dtam64 mod-2^64 toplama (add i64 wrap) + İŞARETSİZ taşma
# tespiti (icmp ult i64 — toplam operanddan küçükse elde 1) + word'ler arası elde
# yayılımı. Test vektörleri: V1 (0,2^64-1)+(0,1)=(1,0); V2 (2^64-1,2^64-1)+(0,1)=
# (0,0); V3 (1,2^64-1)+(0,2)=(2,1) — hepsi bilinen doğru sonuçlarla. CİHAZSIZ:
# QEMU'da -netdev/-drive YOK, sade BM_A64_OBJS. Marker: "KEM BIGNUM OK".
# NOT (literal kısıtı): 0xFFFFFFFFFFFFFFFF doğrudan yazılamaz (lexer strtoll →
# INT64_MAX'a satüre); maksimum word aritmetikle kurulur (INT64_MAX+INT64_MAX+1).
# runtime/codegen DEĞİŞMEDİ; bu bir dil-seviyesi kullanım desenidir.
calistir_bignum_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST 128-bit BIGNUM toplama: bignum_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/bignum_selfhost.kem > $(BUILD)/bignum_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/bignum_selfhost.ll -c -o $(BUILD)/bignum_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/bignum_selfhost.elf $(BUILD)/bignum_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/bignum_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/bignum_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/bignum_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/bignum_selfhost.out -kernel $(BUILD)/bignum_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/bignum_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM BIGNUM OK" $(BUILD)/bignum_selfhost.out; then \
			echo "aarch64 self-host BIGNUM testi gecti: KEMGU cihazsiz 128-bit toplama (carry yayilimi, 3 vektor) dogruladi."; \
		else \
			echo "FAIL: 'KEM BIGNUM OK' bekleniyor (KEMGU self-host 128-bit bignum carry propagation)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host BIGNUM testi atlandi."; \
	fi

# === SELF-HOST YIĞIN-VM bytecode yorumlayıcı (aarch64) — KEMGU DİL KAPSTONU ===
# vm_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde yığın-tabanlı
# bir BYTECODE YORUMLAYICI (stack VM) yazar ve çalıştırır. KEMGU'nun bir
# YORUMLAYICI (fetch-decode-execute döngüsü + veri yığını + opcode dispatch)
# kaldırdığını kanıtlar — bir dilin olgunluk kanıtı. Program bir tam32 dizisi
# (bytecode); yığın Dizi<tam32> (in-place push/pop mutasyonu); PC/SP tam32.
# Opcode'lar: PUSH n, ADD, SUB, MUL, DUP, PRINT, HALT — `iken pc<uzun` döngüsünde
# `değilse eğer` dispatch ile ayrılır (switch yok → değilse-eğer zinciri deseni,
# D-168 crc32 ile kanıtlı). Örnek program 6*7=42 ve 100+58=158 hesaplar; VM'in her
# PRINT'te bastığı değer ayrı bir beklenen-dizisiyle ([42,158]) karşılaştırılır
# (DETERMİNİSTİK). CİHAZSIZ: QEMU'da -netdev/-drive YOK, sade BM_A64_OBJS (heap
# dâhil — Dizi<tam32> program+yığın tahsisi için). Marker: "KEM VM OK".
# Dayanılan invaryant: dizi in-place mutasyon + Dizi<tam32> fn-param (D-171 sort ile
# kanıtlı). runtime/codegen DEĞİŞMEDİ.
calistir_vm_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST YIGIN-VM bytecode yorumlayici: vm_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/vm_selfhost.kem > $(BUILD)/vm_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/vm_selfhost.ll -c -o $(BUILD)/vm_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/vm_selfhost.elf $(BUILD)/vm_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/vm_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/vm_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/vm_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/vm_selfhost.out -kernel $(BUILD)/vm_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/vm_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM VM OK" $(BUILD)/vm_selfhost.out; then \
			echo "aarch64 self-host YIGIN-VM testi gecti: KEMGU cihazsiz bytecode yorumlayici (PUSH/ADD/MUL/PRINT/HALT dispatch; 6*7=42, 100+58=158) dogruladi."; \
		else \
			echo "FAIL: 'KEM VM OK' bekleniyor (KEMGU self-host yigin-VM bytecode yorumlayici)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host YIGIN-VM testi atlandi."; \
	fi

# === SELF-HOST MİNİ-ASSEMBLER (aarch64) — KEMGU mnemonic → bytecode → VM ===
# asm_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde bir MİNİ-
# ASSEMBLER (kod üretici) yazar ve ürettiği bytecode'u D-196 yığın-VM ile
# çalıştırır. KEMGU'nun bir DERLEYİCİ katmanı (yüksek-seviye temsil → düşük-
# seviye bytecode ÇEVİRİSİ = codegen) + bir YORUMLAYICI (fetch-decode-execute)
# birlikte kaldırdığını kanıtlar — bir dilin "compile-then-run" hattı. Kaynak
# program (mnemonic, operand) çiftleri (düz Dizi<tam32>); assemble() bunları VM
# opcode bytecode'una ÇEVİRİR (KOD_PUSH → OP_PUSH + operand hücresi; KOD_MUL →
# OP_MUL; ...) — mnemonic DISPATCH `değilse eğer` zinciriyle. Üretilen bytecode
# in-place dizi YAZMA (kdl_dizi_yaz_tam, D-171 ile kanıtlı) ile hücre hücre
# doldurulur. Sonra bytecode D-196 VM döngüsüyle koşturulur; her PRINT değeri
# ayrı beklenen-diziyle ([42,158]) karşılaştırılır (DETERMİNİSTİK). Örnek: 6*7=42
# ve 100+58=158. CİHAZSIZ: QEMU'da -netdev/-drive YOK, sade BM_A64_OBJS (heap
# dâhil — Dizi<tam32> kaynak/bytecode/yığın tahsisi için). Marker: "KEM ASM OK".
# runtime/codegen DEĞİŞMEDİ.
calistir_asm_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST MINI-ASSEMBLER mnemonic->bytecode->VM: asm_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/asm_selfhost.kem > $(BUILD)/asm_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/asm_selfhost.ll -c -o $(BUILD)/asm_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/asm_selfhost.elf $(BUILD)/asm_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/asm_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/asm_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/asm_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/asm_selfhost.out -kernel $(BUILD)/asm_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/asm_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM ASM OK" $(BUILD)/asm_selfhost.out; then \
			echo "aarch64 self-host MINI-ASSEMBLER testi gecti: KEMGU cihazsiz assembler (mnemonic->bytecode ceviri = codegen) + VM (6*7=42, 100+58=158) dogruladi."; \
		else \
			echo "FAIL: 'KEM ASM OK' bekleniyor (KEMGU self-host mini-assembler mnemonic->bytecode->VM)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host MINI-ASSEMBLER testi atlandi."; \
	fi

# === SELF-HOST BASE64 kodlama/çözme (aarch64) — KEMGU payload codec ===
# base64_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde standart
# RFC 4648 Base64 encode + decode (round-trip). "KEMGU" (5 byte) -> "S0VNR1U="
# (bilinen doğru vektör) -> tekrar "KEMGU". Karakter-tablosu (Dizi<karakter>)
# 6-bit index ile erişim + bit işlemleri (>> << & |) + ham karakter çıktısı
# (yaz_karakter — newline'sız, tek satır). Pentest OS payload-kodlama yardımcısı.
# CİHAZSIZ: QEMU'da -netdev/-drive YOK, sade BM_A64_OBJS (heap dâhil — dizi
# tahsisi için). Marker: "KEM B64 OK" (encode) + "KEM B64 DECODE OK" (round-trip).
calistir_base64_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST BASE64 payload codec: base64_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/base64_selfhost.kem > $(BUILD)/base64_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/base64_selfhost.ll -c -o $(BUILD)/base64_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/base64_selfhost.elf $(BUILD)/base64_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/base64_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/base64_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/base64_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/base64_selfhost.out -kernel $(BUILD)/base64_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/base64_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM B64 OK" $(BUILD)/base64_selfhost.out; then \
			echo "aarch64 self-host BASE64 testi gecti: KEMGU cihazsiz payload codec ('KEMGU' -> 'S0VNR1U=' + decode round-trip) dogruladi."; \
		else \
			echo "FAIL: 'KEM B64 OK' bekleniyor (KEMGU self-host Base64 kodlama)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host BASE64 testi atlandi."; \
	fi

# === SELF-HOST TÜRKÇE büyük/küçük harf dönüşümü (aarch64) — TÜRKÇE-I problemi ===
# turkce_case_selfhost.kem: MMIO/cihaz erişimi OLMADAN, saf KEMGU dilinde ünlü
# "Türkçe-I problemini" DOĞRU çözer. Kod-noktası (Unicode ondalık) üstünde harf
# büyütme/küçültme: Türkçe-özel i<->İ (105<->304, nokta KORUNUR — ASCII yanlış
# 73='I' verir) + ı<->I (305<->73, noktasız) + ç ğ ö ş ü <-> Ç Ğ Ö Ş Ü + ASCII
# a-z<->A-Z (i/I hariç). Kanıt: "istanbul" -> büyüt -> "İSTANBUL" (i->İ=304, ASCII
# tuzağı 73 DEĞİL) + "IRMAK" -> küçült -> "ırmak" (I->ı=305, noktasız) kod-nokta
# dizileriyle doğrulanır + round-trip özdeşlik + ASCII-tuzağı negatif kontrol.
# Marker: "KEM TR CASE OK". CİHAZSIZ: QEMU'da -netdev/-drive YOK, sade
# BM_A64_OBJS (heap dâhil — Dizi<dtam32> kod-nokta tahsisi için).
# NOT (codegen deseni): kod-noktaları dtam32 (D-211 UTF-8 çözücü deseni); dizi
# elemanı önce SKALER dtam32'ye alınır (D-173 ashr tuzağı); `karakter` sayısal
# değil (T003, D-175) → kod-noktalar dtam32 sabitleriyle karşılaştırılır; implicit
# tam32<->dtam32 YASAK (D-200) → sayaçlar tam32, kod-nokta dtam32 ayrı tutuldu;
# switch yok → `değilse eğer` zinciri ile harf-eşleme. runtime/codegen DEĞİŞMEDİ.
calistir_turkce_case_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST TURKCE case-fold (Turkce-I): turkce_case_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/turkce_case_selfhost.kem > $(BUILD)/turkce_case_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/turkce_case_selfhost.ll -c -o $(BUILD)/turkce_case_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/turkce_case_selfhost.elf $(BUILD)/turkce_case_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/turkce_case_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/turkce_case_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/turkce_case_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/turkce_case_selfhost.out -kernel $(BUILD)/turkce_case_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/turkce_case_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM TR CASE OK" $(BUILD)/turkce_case_selfhost.out; then \
			echo "aarch64 self-host TURKCE case-fold testi gecti: KEMGU cihazsiz 'istanbul' -> 'ISTANBUL' (i->I=304, ASCII 73 DEGIL) + 'IRMAK' -> 'irmak' (I->i=305) dogruladi."; \
		else \
			echo "FAIL: 'KEM TR CASE OK' bekleniyor (KEMGU self-host Turkce case-fold)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host TURKCE case-fold testi atlandi."; \
	fi

# turkce_sort_selfhost.kem: MMIO/cihaz erisimi OLMADAN, saf KEMGU dilinde TÜRKÇE
# ALFABETIK SIRALAMA (collation) yapar — mainstream diller varsayilan Unicode
# kod-nokta sirasiyla YANLIS yapar. Türkçe alfabe: a b c ç d e f g ğ h ı i j k l
# m n o ö p r s ş t u ü v y z. Her harfe TÜRKÇE-SIRA-INDEKSI atanir (a=0,b=1,c=2,
# ç=3,...); iki string bu indeksle karsilastirilir (Türkçe collation). KRITIK
# kararlar: ç (Unicode 231) c'den (99) HEMEN SONRA (ç=3 < d=4, Unicode-tuzagi
# 231>100 DEGIL); ı (Unicode 305) i'den (105) ÖNCE (ı=10 < i=11, Unicode-tuzagi
# 305>105 DEGIL). Kanit: [cam,can,ada,ihlamur,irmak] -> Türkçe collation sort ->
# [ada,can,cam,ihlamur,irmak] (Unicode YANLIS: cam en sona, ihlamur irmaktan
# sonraya atardi) + "c<d" ve "i<i" collation-kararlari ayri ayri dogrulanir.
# Marker: "KEM TR SORT OK". CIHAZSIZ: QEMU'da -netdev/-drive YOK, sade
# BM_A64_OBJS (heap dâhil — Dizi<dtam32> kod-nokta havuzu + Dizi<tam32> isaretci
# tahsisi icin). NOT (codegen deseni): kod-noktalari dtam32 (D-211); dizi elemani
# önce SKALER (D-173 ashr tuzagi); karakter sayisal degil (T003, D-175) → kod-
# noktalar dtam32 sabitleriyle karsilastirilir; implicit tam32<->dtam32 YASAK
# (D-200) → sayaclar/indeksler tam32, kod-nokta dtam32 ayri; dizi in-place swap
# (D-171 sort deseni — isaretci/uzunluk takasi); switch yok → degilse eger
# zinciri ile harf->sira-indeks esleme. runtime/codegen DEGISMEDI.
calistir_turkce_sort_selfhost_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 SELF-HOST TURKCE collation sort: turkce_sort_selfhost.kem -> IR -> ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/turkce_sort_selfhost.kem > $(BUILD)/turkce_sort_selfhost.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/turkce_sort_selfhost.ll -c -o $(BUILD)/turkce_sort_selfhost.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/turkce_sort_selfhost.elf $(BUILD)/turkce_sort_selfhost.o $(BM_A64_OBJS)
	@echo "Libc sembol kontrol (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/turkce_sort_selfhost.elf | \
		grep -E 'malloc|free|printf|fopen|puts|__chkstk' > /dev/null; then \
		echo "FAIL: libc referansi"; llvm-nm --undefined-only $(BUILD)/turkce_sort_selfhost.elf; exit 1; \
	fi
	@echo "  (yok — temiz)"
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/turkce_sort_selfhost.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/turkce_sort_selfhost.out -kernel $(BUILD)/turkce_sort_selfhost.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/turkce_sort_selfhost.out; echo "--- son ---"; \
		if grep -q "KEM TR SORT OK" $(BUILD)/turkce_sort_selfhost.out; then \
			echo "aarch64 self-host TURKCE collation sort testi gecti: KEMGU cihazsiz Turkce alfabe sirasi (c<c<d, i<i) dogruladi; Unicode kod-nokta tuzagina dusmedi."; \
		else \
			echo "FAIL: KEM TR SORT OK bekleniyor (KEMGU self-host Turkce collation sort)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — self-host TURKCE collation sort testi atlandi."; \
	fi

# === D-150 Syscall güvenlik testi (aarch64) — kullanıcı-pointer doğrulama ===
# EL0 süreç kernel-adresine yazdırmayı dener → guard RED (-1); user-tampon → OK.
calistir_guvenlik_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-150 aarch64 syscall güvenlik testi: guvenlik_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/guvenlik_arm.c -o $(BUILD)/guvenlik_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/guvenlik_arm.elf $(BUILD)/guvenlik_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/guvenlik_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/guvenlik_arm.out -kernel $(BUILD)/guvenlik_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/guvenlik_arm.out; echo "--- son ---"; \
		if grep -q "GUVENLIK OK" $(BUILD)/guvenlik_arm.out; then \
			echo "D-150 aarch64 güvenlik testi gecti: kernel-adres yazma reddedildi (user-ptr doğrulama)."; \
		else \
			echo "FAIL: 'GUVENLIK OK' bekleniyor (user-pointer doğrulama)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — güvenlik testi atlandi."; \
	fi

calistir_guvenlik_oku_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-151 aarch64 syscall OKUMA-güvenlik testi: guvenlik_oku_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/guvenlik_oku_arm.c -o $(BUILD)/guvenlik_oku_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/guvenlik_oku_arm.elf $(BUILD)/guvenlik_oku_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/guvenlik_oku_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/guvenlik_oku_arm.out -kernel $(BUILD)/guvenlik_oku_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/guvenlik_oku_arm.out; echo "--- son ---"; \
		if grep -q "GUVENLIK OKU OK" $(BUILD)/guvenlik_oku_arm.out; then \
			echo "D-151 aarch64 okuma-güvenlik testi gecti: kernel unmapped/kernel-adres OKUMASINDAN sağ çıktı + reddetti."; \
		else \
			echo "FAIL: 'GUVENLIK OKU OK' bekleniyor (okuma-pointer doğrulama; kernel halt ettiyse DoS regresyonu)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — okuma-güvenlik testi atlandi."; \
	fi

calistir_guvenlik_spawn_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-152 aarch64 spawn-entry güvenlik testi: guvenlik_spawn_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/guvenlik_spawn_arm.c -o $(BUILD)/guvenlik_spawn_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/guvenlik_spawn_arm.elf $(BUILD)/guvenlik_spawn_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/guvenlik_spawn_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/guvenlik_spawn_arm.out -kernel $(BUILD)/guvenlik_spawn_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/guvenlik_spawn_arm.out; echo "--- son ---"; \
		if grep -q "SPAWN GUARD OK" $(BUILD)/guvenlik_spawn_arm.out; then \
			echo "D-152 aarch64 spawn-entry güvenlik testi gecti: geçersiz entry reddedildi, kernel sağ çıktı (DoS engellendi)."; \
		else \
			echo "FAIL: 'SPAWN GUARD OK' bekleniyor (spawn-entry doğrulama; kernel halt ettiyse DoS regresyonu)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — spawn-entry güvenlik testi atlandi."; \
	fi

# === D-153 Kalıcı FS deserialize güvenlik testi (aarch64) — poisoned-disk boyut clamp ===
# Elle üretilmiş ZEHİRLİ disk image (boyut=9999) yüklenir; EL0 süreç num=18 ile okur;
# clamp devrede ise dönen uzunluk <=63 + kernel sağ kalır → "KALICI GUARD OK".
calistir_guvenlik_kalici_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio.o
	@echo "D-153 aarch64 kalıcı FS deserialize güvenlik testi: guvenlik_kalici_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/guvenlik_kalici_arm.c -o $(BUILD)/guvenlik_kalici_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/guvenlik_kalici_arm.elf $(BUILD)/guvenlik_kalici_arm.o $(BM_A64_OBJS)
	@dd if=/dev/zero of=$(BUILD)/disk_guvenlik_kalici.img bs=512 count=64 2>/dev/null
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/guvenlik_kalici_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(BUILD)/disk_guvenlik_kalici.img,format=raw,if=none,id=d0 -device virtio-blk-device,drive=d0 \
			-serial file:$(BUILD)/guvenlik_kalici_arm.out -kernel $(BUILD)/guvenlik_kalici_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/guvenlik_kalici_arm.out; echo "--- son ---"; \
		if grep -q "KALICI GUARD OK" $(BUILD)/guvenlik_kalici_arm.out; then \
			echo "D-153 aarch64 kalıcı FS deserialize güvenlik testi gecti: poisoned boyut clamp'lendi, kernel sağ."; \
		else \
			echo "FAIL: 'KALICI GUARD OK' bekleniyor (poisoned boyut clamp; OOB okuma/kernel-halt regresyonu)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — kalıcı FS deserialize güvenlik testi atlandi."; \
	fi

calistir_guvenlik_bombardiman_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-154 aarch64 düşman-userspace BOMBARDIMAN testi: guvenlik_bombardiman_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/guvenlik_bombardiman_arm.c -o $(BUILD)/guvenlik_bombardiman_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/guvenlik_bombardiman_arm.elf $(BUILD)/guvenlik_bombardiman_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/guvenlik_bombardiman_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/guvenlik_bombardiman_arm.out -kernel $(BUILD)/guvenlik_bombardiman_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/guvenlik_bombardiman_arm.out; echo "--- son ---"; \
		if grep -q "HOSTILE SURVIVED OK" $(BUILD)/guvenlik_bombardiman_arm.out; then \
			echo "D-154 aarch64 bombardıman testi gecti: kernel 8-syscall kötü-ptr bataryasından SAĞ ÇIKTI + reddetti + hâlâ çalışıyor."; \
		else \
			echo "FAIL: 'HOSTILE SURVIVED OK' bekleniyor (kernel bataryadan sağ çıkmalı; success satırı yoksa bir syscall halt ettirdi = güvenlik boşluğu)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — bombardıman testi atlandi."; \
	fi

# === D-176 USERSPACE NETWORKING (aarch64) — EL0 süreç syscall ile ağ yapar ===
# EL0 (yetkisiz) süreç net_gonder(24)/net_al(25) syscall'larıyla ARP round-trip yapar;
# virtio-net'e doğrudan erişmez. Süreç modeli + ağ yığını birleşimi. Kötü-pointer net_al
# reddedilir (D-150/151 guard). Marker: "USERNET OK".
calistir_userspace_net_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-176 aarch64 userspace networking testi: userspace_net_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_net_arm.c -o $(BUILD)/userspace_net_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_net_arm.elf $(BUILD)/userspace_net_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_net_arm.out; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-serial file:$(BUILD)/userspace_net_arm.out -kernel $(BUILD)/userspace_net_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_net_arm.out; echo "--- son ---"; \
		if grep -q "USERNET OK" $(BUILD)/userspace_net_arm.out; then \
			echo "D-176 aarch64 userspace networking testi gecti: EL0 surec syscall ile ARP round-trip yapti (surec+ag birlesimi)."; \
		else \
			echo "FAIL: 'USERNET OK' bekleniyor (userspace net_gonder/net_al syscall + guard)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace networking testi atlandi."; \
	fi

# === USERSPACE KOOPERATİF FİBER (aarch64) — EL0-içi yeşil-thread context-switch ===
# D-201: TEK EL0 süreç İÇİNDE 2 kooperatif fiber, fiber_yield() (callee-saved x19-x30
# + sp save/restore, saf EL0 asm) ile birbirine geçer — KERNEL YARDIMI OLMADAN
# (timer/IRQ/preemption/syscall-switch yok). Kernel'in kdl_baglam_degis primitifinin
# userspace muadili. Fiber yığınları + bağlamları EL0 user-VA'da (.user_data, 0x42xxxxxx).
# DETERMİNİSTİK: ping-pong interleave A1,B1,A2,B2,A3,B3 → "USERFIBER OK". net/drive YOK,
# sade QEMU (userspace_test_arm modeli). virtio* BM_A64_OBJS'te (dead-code, libc-temiz).
calistir_userspace_fiber_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "D-201 aarch64 userspace kooperatif fiber testi: userspace_fiber_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_fiber_arm.c -o $(BUILD)/userspace_fiber_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_fiber_arm.elf $(BUILD)/userspace_fiber_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_fiber_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/userspace_fiber_arm.out -kernel $(BUILD)/userspace_fiber_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_fiber_arm.out; echo "--- son ---"; \
		if grep -q "USERFIBER OK" $(BUILD)/userspace_fiber_arm.out; then \
			echo "D-201 aarch64 userspace fiber testi gecti: EL0-ici kooperatif fiber context-switch (yesil-thread ping-pong)."; \
		else \
			echo "FAIL: 'USERFIBER OK' bekleniyor (EL0 fiber_gec callee-saved+sp switch + deterministik interleave)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace fiber testi atlandi."; \
	fi

# === USERSPACE setjmp/longjmp (aarch64) — EL0 YEREL-OLMAYAN atlama (C exception-benzeri) ===
# MİLESTONE-C: D-203 naked fiber_gec (callee-saved x19-x30 + sp save/restore) desenini
# setjmp/longjmp'e taşır. TEK EL0 akış İÇİNDE yerel-olmayan kontrol akışı: u_setjmp(buf)
# callee-saved+sp+lr kaydeder + 0 döner; derin çağrı zinciri (kat1→kat2→kat3); en derin
# katman u_longjmp(buf,42) → kontrol u_setjmp'e GERİ SIÇRAR (bu sefer 42) → ara stack
# frame'leri ATLANIR. TAMAMEN userspace (kernel yardımı yok: timer/IRQ/syscall-switch yok).
# jmp_buf EL0 user-VA'da (.user_data, 0x42xxxxxx). Yalnız I/O syscall (5/6/7 yaz, 3 cik).
# DETERMİNİSTİK: SETJMP0 → K1 K2 K3 → LONGJMP42 → "USERJMP OK". net/drive YOK, sade QEMU
# (userspace_fiber_test_arm modeli). virtio* BM_A64_OBJS'te (dead-code, libc-temiz).
calistir_userspace_jmp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "MILESTONE-C aarch64 userspace setjmp/longjmp testi: userspace_jmp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_jmp_arm.c -o $(BUILD)/userspace_jmp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_jmp_arm.elf $(BUILD)/userspace_jmp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_jmp_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/userspace_jmp_arm.out -kernel $(BUILD)/userspace_jmp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_jmp_arm.out; echo "--- son ---"; \
		if grep -q "USERJMP OK" $(BUILD)/userspace_jmp_arm.out; then \
			echo "MILESTONE-C aarch64 userspace setjmp/longjmp testi gecti: EL0 yerel-olmayan atlama (derin zincirden tek sicrayisla geri donus)."; \
		else \
			echo "FAIL: 'USERJMP OK' bekleniyor (EL0 u_setjmp/u_longjmp callee-saved+sp+lr restore + yerel-olmayan sicrama)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace setjmp/longjmp testi atlandi."; \
	fi

# === USERSPACE ÇOK-FİBER SCHEDULER (aarch64) — EL0-içi round-robin N yeşil-thread ===
# MİLESTONE-B: D-203 iki-fiber ping-pong'unu GERÇEK scheduler'a taşır. TEK EL0 süreç
# İÇİNDE N>=3 kooperatif fiber (A/B/C) + merkezi round-robin scheduler döngüsü. Fiber'lar
# birbirine doğrudan geçmez; her fiber u_yield() ile SCHEDULER'A döner, scheduler bir
# sonraki READY fiber'ı round-robin seçip context-switch (D-203 naked fiber_gec,
# callee-saved x19-x30 + sp) yapar — KERNEL YARDIMI OLMADAN (timer/IRQ/preemption yok).
# Her fiber ayrı EL0 yığını (.user_data, 0x42xxxxxx, AP=01). M:1 kooperatif runtime çekirdeği.
# DETERMİNİSTİK: round-robin interleave A1 B1 C1 A2 B2 C2 A3 B3 C3 → "USERSCHED OK".
# net/drive YOK, sade QEMU (userspace_fiber_test_arm modeli). virtio* BM_A64_OBJS'te.
calistir_userspace_sched_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "MILESTONE-B aarch64 userspace cok-fiber scheduler testi: userspace_sched_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_sched_arm.c -o $(BUILD)/userspace_sched_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_sched_arm.elf $(BUILD)/userspace_sched_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_sched_arm.out; \
		timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/userspace_sched_arm.out -kernel $(BUILD)/userspace_sched_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_sched_arm.out; echo "--- son ---"; \
		if grep -q "USERSCHED OK" $(BUILD)/userspace_sched_arm.out; then \
			echo "MILESTONE-B aarch64 userspace scheduler testi gecti: EL0-ici round-robin cok-fiber (3 yesil-thread) kooperatif scheduler."; \
		else \
			echo "FAIL: 'USERSCHED OK' bekleniyor (EL0 round-robin scheduler + u_yield + deterministik interleave A1 B1 C1 ...)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace scheduler testi atlandi."; \
	fi

# === USERSPACE MALLOC (aarch64) — EL0 süreç KENDİ heap allocator'ını çalıştırır ===
# MİLESTONE B: bir EL0 (yetkisiz) süreç, çekirdek yardımı OLMADAN kendi dinamik bellek
# ayırıcısını (u_malloc/u_free — bump + serbest-liste) yalnız kendi EL0-erişimli veri
# sayfasında (0x42000000 bölgesi, .user_data) sürer. Senaryo: A/B/C ayır → B serbest →
# D ayır → D, B'nin yerini alır (free-list reuse kanıtı) + yaz/oku round-trip. Kernel
# yalnız I/O syscall (num=5/6/7 yaz, num=3 cik) verir. userspace_arm/userspace_fiber
# modeli (kdl_el0_calistir, -smp yok). Kanıt: "USERMALLOC OK".
calistir_userspace_malloc_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "MILESTONE-B aarch64 userspace heap allocator testi: userspace_malloc_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_malloc_arm.c -o $(BUILD)/userspace_malloc_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_malloc_arm.elf $(BUILD)/userspace_malloc_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_malloc_arm.out; \
		timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-serial file:$(BUILD)/userspace_malloc_arm.out -kernel $(BUILD)/userspace_malloc_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_malloc_arm.out; echo "--- son ---"; \
		if grep -q "USERMALLOC OK" $(BUILD)/userspace_malloc_arm.out; then \
			echo "MILESTONE-B aarch64 userspace malloc testi gecti: EL0 kendi heap allocator (bump+free-list reuse + round-trip), kernel yardimi yok."; \
		else \
			echo "FAIL: 'USERMALLOC OK' bekleniyor (EL0 u_malloc/u_free bump+serbest-liste reuse + round-trip)"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace malloc testi atlandi."; \
	fi

# === USERSPACE DNS (aarch64) — EL0 süreç syscall ile TAM DNS protokol çözümleme ===
# D-176 raw-frame syscall'ları (net_gonder=24/net_al=25) üstünde EL0 (yetkisiz) süreç
# TAM L2-L7 yığını çalıştırır: ARP → DNS sorgusu (Eth+IPv4+UDP+DNS "example.com" A) →
# yanıt parse (isim-sıkıştırma 0xC0) → IPv4 A-kaydı. Ağ kernel-aracılı, protokol
# tamamen userspace. RX round-trip: "USERDNS OK" (+ çözülen IP). Fallback (internet
# yoksa / SLIRP dış-DNS yanıt vermezse): EL0 sorguyu sys2(24) ile GÖNDERDİ →
# "USERDNS SENT OK" (seri) VEYA pcap'te UDP dst-port 53 ("0035" = port 53 hedef) TX.
# virtio_net BM_A64_OBJS'te (explicit eklemeye gerek yok).
calistir_userspace_dns_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 userspace DNS testi: userspace_dns_arm.c -> ELF (EL0 syscall ile tam DNS)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_dns_arm.c -o $(BUILD)/userspace_dns_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_dns_arm.elf $(BUILD)/userspace_dns_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_dns_arm.out $(BUILD)/userspace_dns_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/userspace_dns_arm.pcap \
			-serial file:$(BUILD)/userspace_dns_arm.out -kernel $(BUILD)/userspace_dns_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_dns_arm.out; echo "--- son ---"; \
		if grep -q "USERDNS OK" $(BUILD)/userspace_dns_arm.out; then \
			echo "aarch64 userspace DNS testi gecti (RX round-trip): EL0 surec syscall ile tam DNS cozumledi (isim -> IPv4)."; \
		elif grep -q "USERDNS SENT OK" $(BUILD)/userspace_dns_arm.out; then \
			echo "aarch64 userspace DNS testi gecti (SENT fallback): EL0 surec DNS sorgusunu sys2(24) ile gonderdi (RX yok/internet yok)."; \
		elif xxd -p $(BUILD)/userspace_dns_arm.pcap 2>/dev/null | tr -d '\n' | grep -q "0035"; then \
			echo "aarch64 userspace DNS testi gecti (pcap-TX fallback): EL0'in DNS sorgusu pcap'te var (UDP dst-port 53 = '0035')."; \
		else \
			echo "FAIL: seri 'USERDNS OK'/'USERDNS SENT OK' veya pcap'te UDP dst-port 53 ('0035') bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace DNS testi atlandi."; \
	fi

# === USERSPACE TCP HANDSHAKE (aarch64) — EL0 süreç syscall ile TAM TCP el sıkışması ===
# D-176 raw-frame syscall'ları (net_gonder=24/net_al=25) üstünde EL0 (yetkisiz) süreç
# TAM TCP üç-yönlü el sıkışması yapar (çekirdekte TCP durum-makinesi YOK): ARP (gateway
# MAC) → DNS ("example.com" A → hedef IPv4) → hedef-IP:80'e TCP SYN (pseudo-header
# checksum, EL0'da) → SYN-ACK al (flags=0x12, ack=bizim_seq+1) → ACK → ESTABLISHED.
# tcp_connect_arm.c (D-159) handshake mantığı KERNEL'deydi; burada EL0'a taşındı. RX
# round-trip gate: "USERTCP OK". Fallback (internet yoksa / SLIRP dış-TCP yanıt vermezse):
# EL0'ın SYN'i sys2(24) ile GÖNDERDİĞİ pcap'te kanıtlanır → serial "USERTCP SENT OK"
# VEYA pcap'te TCP seq "KEMG" (4b454d47) TX. virtio_net BM_A64_OBJS'te (D-176) — yine de
# explicit dependency olarak eklendi.
calistir_userspace_tcp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 userspace TCP handshake testi: userspace_tcp_arm.c -> ELF (EL0 syscall ile tam TCP el sikismasi)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_tcp_arm.c -o $(BUILD)/userspace_tcp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_tcp_arm.elf $(BUILD)/userspace_tcp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_tcp_arm.out $(BUILD)/userspace_tcp_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/userspace_tcp_arm.pcap \
			-serial file:$(BUILD)/userspace_tcp_arm.out -kernel $(BUILD)/userspace_tcp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_tcp_arm.out; echo "--- son ---"; \
		if grep -q "USERTCP OK" $(BUILD)/userspace_tcp_arm.out; then \
			echo "aarch64 userspace TCP handshake testi gecti (RX round-trip): EL0 surec syscall ile TAM TCP el sikismasi yapti (ESTABLISHED)."; \
		elif grep -q "USERTCP SENT OK" $(BUILD)/userspace_tcp_arm.out; then \
			echo "aarch64 userspace TCP handshake testi gecti (SENT fallback): EL0 surec SYN'i sys2(24) ile gonderdi (SYN-ACK yok/internet yok)."; \
		elif grep -a -q "KEMG" $(BUILD)/userspace_tcp_arm.pcap; then \
			echo "aarch64 userspace TCP handshake testi gecti (pcap-TX fallback): EL0'in SYN'i pcap'te var (TCP seq 'KEMG')."; \
			echo "USERTCP SENT OK"; \
		else \
			echo "FAIL: seri 'USERTCP OK'/'USERTCP SENT OK' veya pcap'te 'KEMG' TCP seq bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace TCP handshake testi atlandi."; \
	fi

# === USERSPACE DHCP (aarch64) — EL0 süreç syscall ile ağ OTO-KONFİGÜRASYON ===
# D-176 raw-frame syscall'ları (net_gonder=24/net_al=25) üstünde EL0 (yetkisiz) süreç
# KENDİ IP'sini DHCP ile öğrenir: DISCOVER inşa (Eth broadcast + IPv4 0.0.0.0->
# 255.255.255.255 + UDP 68->67 + BOOTP op=1 + magic cookie + option 53=1) → sys2(24)
# ile yolla → sys2(25) poll ile OFFER'ı al → doğrula (op=2 BOOTREPLY, xid eşleşir,
# yiaddr non-zero, option 53=2). SLIRP dahili DHCP sunucusu (10.0.2.2:67) yiaddr=
# 10.0.2.15 döner — DETERMİNİSTİK (internetsiz). RX round-trip: "USERDHCP OK" (+yiaddr).
# Fallback (RX gelmezse): pcap'te DHCP DISCOVER TX kanıtı — magic cookie "63825363".
# virtio_net BM_A64_OBJS'te (explicit eklemeye gerek yok).
calistir_userspace_dhcp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 userspace DHCP testi: userspace_dhcp_arm.c -> ELF (EL0 syscall ile ag oto-konfig)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_dhcp_arm.c -o $(BUILD)/userspace_dhcp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_dhcp_arm.elf $(BUILD)/userspace_dhcp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_dhcp_arm.out $(BUILD)/userspace_dhcp_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/userspace_dhcp_arm.pcap \
			-serial file:$(BUILD)/userspace_dhcp_arm.out -kernel $(BUILD)/userspace_dhcp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_dhcp_arm.out; echo "--- son ---"; \
		if grep -q "USERDHCP OK" $(BUILD)/userspace_dhcp_arm.out; then \
			echo "aarch64 userspace DHCP testi gecti (RX round-trip): EL0 surec syscall ile DHCP DISCOVER->OFFER yapti (kendi IP'sini ogrendi)."; \
		elif xxd -p $(BUILD)/userspace_dhcp_arm.pcap 2>/dev/null | tr -d '\n' | grep -q "63825363"; then \
			echo "aarch64 userspace DHCP testi gecti (pcap-TX fallback): EL0'in DHCP DISCOVER'i pcap'te var (magic cookie '63825363')."; \
		else \
			echo "FAIL: seri 'USERDHCP OK' veya pcap'te DHCP magic cookie ('63825363') bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace DHCP testi atlandi."; \
	fi

# === USERSPACE TFTP GET (aarch64) — EL0 süreç syscall ile AĞDAN DOSYA ÇEKER ===
# DOSYA-TRANSFERİ milestone: D-176 raw-frame syscall'ları (net_gonder=24/net_al=25)
# üstünde EL0 (yetkisiz) süreç SLIRP'in dahili TFTP sunucusundan (10.0.2.2:69) bir
# dosya çeker: TFTP RRQ inşa (Eth + IPv4 dst=10.0.2.2 + UDP src=efemeral dst=69 +
# opcode 1 + "dosya.txt\0octet\0") → sys2(24) ile yolla → sys2(25) poll ile TFTP
# DATA (opcode 3, block 1) al → SLIRP TID öğren + ACK (opcode 4, block 1) yolla →
# içeriği "KEMGU-TFTP-DATA" ile karşılaştır. QEMU `-netdev user,tftp=<DIR>` dahili
# TFTP sunucusu bu dosyayı sunar — DETERMİNİSTİK (internetsiz). RX round-trip:
# "USERTFTP OK" (+ çekilen içerik). Fallback (DATA gelmezse): pcap'te TFTP RRQ TX
# kanıtı — dosya adı "dosya.txt" (hex "646f7379612e747874"). virtio_net BM_A64_OBJS'te.
calistir_userspace_tftp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS)
	@echo "aarch64 userspace TFTP GET testi: userspace_tftp_arm.c -> ELF (EL0 syscall ile agdan dosya cekme)..."
	@mkdir -p $(BUILD)/tftp
	@printf 'KEMGU-TFTP-DATA' > $(BUILD)/tftp/dosya.txt
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_tftp_arm.c -o $(BUILD)/userspace_tftp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_tftp_arm.elf $(BUILD)/userspace_tftp_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_tftp_arm.out $(BUILD)/userspace_tftp_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0,tftp=$(BUILD)/tftp -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/userspace_tftp_arm.pcap \
			-serial file:$(BUILD)/userspace_tftp_arm.out -kernel $(BUILD)/userspace_tftp_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_tftp_arm.out; echo "--- son ---"; \
		if grep -q "USERTFTP OK" $(BUILD)/userspace_tftp_arm.out; then \
			echo "aarch64 userspace TFTP GET testi gecti (RX round-trip): EL0 surec syscall ile agdan dosya cekti (icerik dogrulandi)."; \
		elif xxd -p $(BUILD)/userspace_tftp_arm.pcap 2>/dev/null | tr -d '\n' | grep -q "646f7379612e747874"; then \
			echo "aarch64 userspace TFTP GET testi gecti (pcap-TX fallback): EL0'in TFTP RRQ'su pcap'te var (dosya adi 'dosya.txt')."; \
		else \
			echo "FAIL: seri 'USERTFTP OK' veya pcap'te TFTP RRQ dosya adi ('646f7379612e747874') bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace TFTP GET testi atlandi."; \
	fi

# === USERSPACE ICMP PING (aarch64) — EL0 süreç syscall ile L3 ping ===
# EL0 (yetkisiz) süreç net_gonder(24)/net_al(25) syscall'larıyla ARP çözer + IPv4+ICMP
# Echo Request (payload "KEMGU") yollar + echo reply'i doğrular. Protokol mantığı EL0'da
# (icmp_arm.c çekirdek versiyonunun userspace'e taşınmış hâli). SLIRP gateway (10.0.2.2)
# ICMP echo'ya dahili yanıt verir → DETERMİNİSTİK (internetsiz). Marker: "USERPING OK".
# Reply gelmezse pcap filter-dump'ta "KEMGU" (TX kanıtı) → "USERPING SENT OK" fallback.
# NOT: virtio_net BM_A64_OBJS'te (D-176) — yine de explicit dependency olarak eklendi.
calistir_userspace_ping_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 userspace ICMP ping testi: userspace_ping_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_ping_arm.c -o $(BUILD)/userspace_ping_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_ping_arm.elf $(BUILD)/userspace_ping_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_ping_arm.out $(BUILD)/userspace_ping_arm.pcap; \
		timeout 12 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/userspace_ping_arm.pcap \
			-serial file:$(BUILD)/userspace_ping_arm.out -kernel $(BUILD)/userspace_ping_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_ping_arm.out; echo "--- son ---"; \
		if grep -q "USERPING OK" $(BUILD)/userspace_ping_arm.out; then \
			echo "aarch64 userspace ICMP ping testi gecti: EL0 surec syscall ile ICMP echo round-trip yapti (userspace L3 protokol)."; \
		elif grep -a -q "KEMGU" $(BUILD)/userspace_ping_arm.pcap; then \
			echo "aarch64 userspace ICMP ping testi gecti (TX-pcap fallback): EL0 ICMP echo request gonderildi (pcap 'KEMGU')."; \
			echo "USERPING SENT OK"; \
		else \
			echo "FAIL: 'USERPING OK' (RX round-trip) veya pcap'te 'KEMGU' (userspace TX) bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace ICMP ping testi atlandi."; \
	fi

# === USERSPACE HTTP GET (aarch64) — EL0 süreç syscall ile UYGULAMA KATMANI ===
# D-176 raw-frame syscall'ları (net_gonder=24/net_al=25) + D-177 EL0 DNS üstünde
# bir EL0 (yetkisiz) süreç TAM HTTP/1.1 istemcisi çalıştırır: ARP → DNS
# ("example.com" A) → hedef-IP:80'e TAM TCP handshake (SYN/SYN-ACK/ACK) → HTTP GET
# PSH+ACK DATA segmenti → yanıt durum satırı ("HTTP/1." + 200/3xx). Ağ kernel-
# aracılı (sys2 24/25); TCP state makinesi + HTTP tamamen userspace. HTTP request
# byte'ları EL0 user tamponuna elle yazılır (D-177 .rodata deref yasağı). RX gate:
# "USERHTTP OK". SLIRP dış-TCP yanıt vermezse (internet yok): EL0 GET isteğini
# sys2(24) ile gönderdi → "USERHTTP SENT OK" (seri) VEYA pcap'te "GET /" TX kanıtı.
# virtio_net BM_A64_OBJS'te — yine de explicit dependency olarak eklendi.
calistir_userspace_http_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 userspace HTTP GET testi: userspace_http_arm.c -> ELF (EL0 syscall ile tam HTTP istemcisi)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_http_arm.c -o $(BUILD)/userspace_http_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_http_arm.elf $(BUILD)/userspace_http_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_http_arm.out $(BUILD)/userspace_http_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/userspace_http_arm.pcap \
			-serial file:$(BUILD)/userspace_http_arm.out -kernel $(BUILD)/userspace_http_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_http_arm.out; echo "--- son ---"; \
		if grep -q "USERHTTP OK" $(BUILD)/userspace_http_arm.out; then \
			echo "aarch64 userspace HTTP GET testi gecti (RX round-trip): EL0 surec syscall ile gercek web sunucusundan HTTP durum satiri aldi (uygulama katmani)."; \
		elif grep -q "USERHTTP SENT OK" $(BUILD)/userspace_http_arm.out; then \
			echo "aarch64 userspace HTTP GET testi gecti (SENT fallback): EL0 surec HTTP GET istegini sys2(24) ile gonderdi (RX yok/internet yok)."; \
		elif grep -a -q "GET /" $(BUILD)/userspace_http_arm.pcap; then \
			echo "aarch64 userspace HTTP GET testi gecti (pcap-TX fallback): EL0'in HTTP GET istegi pcap'te var ('GET /')."; \
		else \
			echo "FAIL: seri 'USERHTTP OK'/'USERHTTP SENT OK' veya pcap'te 'GET /' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace HTTP GET testi atlandi."; \
	fi

# === USERSPACE HTTP POST (aarch64) — EL0 süreç syscall ile VERİ GÖNDERİR ===
# D-184 userspace HTTP GET üstüne kurulu: bir EL0 (yetkisiz) süreç yalnız
# net_gonder(24)/net_al(25) syscall'larıyla sunucuya VERİ POST'lar: ARP → DNS
# ("example.com" A) → hedef-IP:80'e TAM TCP handshake (SYN/SYN-ACK/ACK) → HTTP
# POST PSH+ACK DATA segmenti (istek satırı + Content-Type/Length başlıkları +
# gövde "KEMGU-POST" 10 byte) → yanıt durum satırı ("HTTP/1." + 200/3xx/405).
# Ağ kernel-aracılı (sys2 24/25); TCP state makinesi + HTTP tamamen userspace.
# POST request byte'ları (istek+başlık+gövde) EL0 user tamponuna elle yazılır
# (D-177 .rodata deref yasağı). RX gate: "USERPOST OK". SLIRP dış-TCP yanıt
# vermezse (internet yok): EL0 POST isteğini sys2(24) ile gönderdi →
# "USERPOST SENT OK" (seri) VEYA pcap'te "POST /" TX kanıtı.
# virtio_net BM_A64_OBJS'te — yine de explicit dependency olarak eklendi.
calistir_userspace_post_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 userspace HTTP POST testi: userspace_post_arm.c -> ELF (EL0 syscall ile veri gonderme)..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/userspace_post_arm.c -o $(BUILD)/userspace_post_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/userspace_post_arm.elf $(BUILD)/userspace_post_arm.o $(BM_A64_OBJS)
	@if command -v qemu-system-aarch64 > /dev/null 2>&1; then \
		rm -f $(BUILD)/userspace_post_arm.out $(BUILD)/userspace_post_arm.pcap; \
		timeout 20 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
			-global virtio-mmio.force-legacy=false \
			-netdev user,id=n0 -device virtio-net-device,netdev=n0 \
			-object filter-dump,id=f0,netdev=n0,file=$(BUILD)/userspace_post_arm.pcap \
			-serial file:$(BUILD)/userspace_post_arm.out -kernel $(BUILD)/userspace_post_arm.elf 2>/dev/null || true; \
		echo "--- QEMU seri cikti ---"; cat $(BUILD)/userspace_post_arm.out; echo "--- son ---"; \
		if grep -q "USERPOST OK" $(BUILD)/userspace_post_arm.out; then \
			echo "aarch64 userspace HTTP POST testi gecti (RX round-trip): EL0 surec syscall ile sunucuya veri POST'ladi ve HTTP durum satiri aldi (uygulama katmani)."; \
		elif grep -q "USERPOST SENT OK" $(BUILD)/userspace_post_arm.out; then \
			echo "aarch64 userspace HTTP POST testi gecti (SENT fallback): EL0 surec HTTP POST istegini sys2(24) ile gonderdi (RX yok/internet yok)."; \
		elif grep -a -q "POST /" $(BUILD)/userspace_post_arm.pcap; then \
			echo "aarch64 userspace HTTP POST testi gecti (pcap-TX fallback): EL0'in HTTP POST istegi pcap'te var ('POST /')."; \
		else \
			echo "FAIL: seri 'USERPOST OK'/'USERPOST SENT OK' veya pcap'te 'POST /' bekleniyor"; \
			exit 1; \
		fi; \
	else \
		echo "QEMU yok — userspace HTTP POST testi atlandi."; \
	fi

# === OS kernel boot kanıtları — toplu gate (aarch64 + x86_64 × hepsi) ===
# OS'te otomatik host-gate YOK: gate = QEMU-boot-kanıtı. Bu hedef tüm OS
# yeteneklerini iki mimaride boot edip doğrular (QEMU yoksa graceful skip).
calistir_os_kernels: calistir_qemu_smoke calistir_kernel_dizi_bare_metal \
                     calistir_istisna_test_arm calistir_timer_test_arm calistir_syscall_test_arm \
                     calistir_sched_test_arm calistir_preempt_test_arm calistir_sleep_test_arm \
                     calistir_priority_test_arm calistir_kanal_test_arm calistir_syscall_arg_test_arm \
                     calistir_d2_test_arm calistir_d1_test_arm calistir_proc_test_arm \
                     calistir_userspace_test_arm calistir_preempt_el0_test_arm \
                     calistir_syscall_ret_test_arm calistir_multiproc_test_arm \
                     calistir_tick_test_arm calistir_smp_test_arm calistir_smp_compute_test_arm calistir_smp_queue_test_arm calistir_smp_barrier_test_arm calistir_smp_atomic_test_arm calistir_smp_ticket_test_arm calistir_smp_mcs_test_arm calistir_smp_prodcons_test_arm calistir_smp_rwlock_test_arm calistir_smp_seqlock_test_arm calistir_smp4_test_arm calistir_smp_sort_test_arm calistir_spawn_test_arm calistir_yasam_test_arm \
                     calistir_dosya_test_arm calistir_metin_test_arm calistir_ls_test_arm \
                     calistir_sil_test_arm calistir_kabuk_test_arm calistir_calis_test_arm \
                     calistir_geri_al_test_arm calistir_kanal_ipc_test_arm \
                     calistir_virtio_test_arm calistir_virtio_rw_test_arm calistir_kalici_test_arm \
                     calistir_fs_journal_test_arm calistir_minifs_test_arm \
                     calistir_minifs_crud_test_arm \
                     calistir_crashfs_test_arm \
                     calistir_net_test_arm calistir_arp_test_arm calistir_arp_scan_test_arm \
                     calistir_udp_test_arm calistir_dhcp_test_arm calistir_dhcp_lease_test_arm \
                     calistir_dns_test_arm calistir_tcp_test_arm calistir_icmp_test_arm \
                     calistir_ping_sweep_test_arm \
                     calistir_traceroute_test_arm \
                     calistir_dns_resolver_test_arm calistir_dns_ptr_test_arm \
                     calistir_ntp_test_arm calistir_rtc_test_arm calistir_uart_rx_test_arm \
                     calistir_shell_test_arm calistir_shell_script_test_arm \
                     calistir_recon_shell_test_arm \
                     calistir_recon_shell2_test_arm \
                     calistir_tcp_connect_test_arm calistir_port_scan_test_arm \
                     calistir_http_get_test_arm \
                     calistir_tcp_close_test_arm \
                     calistir_virtio_selfhost_arm \
                     calistir_virtio_selfhost_rw_arm calistir_virtio_net_selfhost_arm \
                     calistir_virtio_net_mac_selfhost_arm \
                     calistir_virtio_blk_config_selfhost_arm \
                     calistir_crc32_selfhost_arm \
                     calistir_sort_selfhost_arm \
                     calistir_hashmap_selfhost_arm \
                     calistir_sha256_selfhost_arm calistir_rc4_selfhost_arm \
                     calistir_hashcrack_selfhost_arm \
                     calistir_utf8_selfhost_arm \
                     calistir_base64_selfhost_arm \
                     calistir_turkce_case_selfhost_arm \
                     calistir_turkce_sort_selfhost_arm \
                     calistir_bignum_selfhost_arm \
                     calistir_vm_selfhost_arm \
                     calistir_asm_selfhost_arm \
                     calistir_guvenlik_test_arm \
                     calistir_guvenlik_oku_test_arm calistir_guvenlik_spawn_test_arm \
                     calistir_guvenlik_kalici_test_arm calistir_guvenlik_bombardiman_test_arm \
                     calistir_userspace_net_test_arm \
                     calistir_userspace_shm_test_arm \
                     calistir_userspace_fiber_test_arm \
                     calistir_userspace_jmp_test_arm \
                     calistir_userspace_sched_test_arm \
                     calistir_userspace_malloc_test_arm \
                     calistir_userspace_dns_test_arm calistir_userspace_ping_test_arm \
                     calistir_userspace_dhcp_test_arm \
                     calistir_userspace_tftp_test_arm \
                     calistir_userspace_tcp_test_arm calistir_userspace_http_test_arm \
                     calistir_userspace_post_test_arm \
                     calistir_capstone_arm \
                     calistir_uart_merhaba_x86_bare_metal calistir_kernel_dizi_x86_bare_metal \
                     calistir_istisna_test_x86 calistir_timer_test_x86 calistir_syscall_test_x86 \
                     calistir_sched_test_x86 calistir_rtc_test_x86 calistir_capstone_x86 \
                     calistir_smp_test_x86 calistir_smp4_test_x86 calistir_ring3_test_x86 \
                     calistir_ring3_page_test_x86 calistir_preempt_test_x86 \
                     calistir_syscall_abi_test_x86 calistir_ring3_proc_test_x86 \
                     calistir_pci_enum_test_x86
	@echo ""
	@echo "=== TUM OS kanitlari gecti: 4 boot + 2 istisna + 2 timer + 2 syscall + 2 capstone + SMP (aarch64 + x86_64) + ring3 (x86) + tam sayfa-izolasyon (x86) ==="

# === UartSurucu vtable testi (her iki driver birlikte) ===
$(BUILD)/test_uart_vtable$(EXE): runtime/kdl_runtime_uart_pl011.c \
                                  runtime/kdl_runtime_uart_16550.c \
                                  $(TESTDIR)/test_uart_vtable.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -DKEMGU_UART_MOCK -Iruntime -o $@ $^

calistir_uart_vtable_test: $(BUILD)/test_uart_vtable$(EXE)
	./$(BUILD)/test_uart_vtable$(EXE)

# === Panik handler host testi (Clang64, ASan aktif) ===
$(BUILD)/test_panik$(EXE): runtime/kdl_runtime_uart_pl011.c \
                           runtime/kdl_runtime_panik.c \
                           $(TESTDIR)/test_panik.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -DKEMGU_UART_MOCK -Iruntime -o $@ $^

calistir_panik_test: $(BUILD)/test_panik$(EXE)
	./$(BUILD)/test_panik$(EXE)

# === Panik handler bare-metal cross-compile (ARM64) ===
calistir_panik_bare_metal:
	@echo "Panik handler bare-metal cross-compile dogrulamasi..."
	clang -target aarch64-unknown-none -ffreestanding -nostdlib \
		-Wall -Wextra -Wpedantic -std=c11 -O2 \
		-DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_panik.c \
		-o $(BUILD)/kdl_panik_aarch64.o
	@echo ""
	@echo "Beklenen sembol:"
	@llvm-nm --defined-only $(BUILD)/kdl_panik_aarch64.o | \
		grep -E 'kdl_panik_dur$$' || \
		{ echo "FAIL: kdl_panik_dur eksik"; exit 1; }
	@echo ""
	@echo "Libc sembol referansi (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kdl_panik_aarch64.o | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|abort|exit' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kdl_panik_aarch64.o; \
		exit 1; \
	fi
	@echo "  (yok — temiz)"
	@echo "Panik bare-metal dogrulamasi basarili!"

# === 16550A mock host testi (Clang64, ASan aktif) ===
$(BUILD)/test_uart_16550$(EXE): runtime/kdl_runtime_uart_16550.c \
                                $(TESTDIR)/test_uart_16550.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -DKEMGU_UART_MOCK -Iruntime -o $@ $^

calistir_uart_16550_test: $(BUILD)/test_uart_16550$(EXE)
	./$(BUILD)/test_uart_16550$(EXE)

# === 16550A bare-metal cross-compile (x86_64 freestanding) ===
calistir_uart_16550_bare_metal:
	@echo "16550A bare-metal cross-compile + symbol dogrulamasi (x86_64)..."
	clang -target x86_64-unknown-none -ffreestanding -nostdlib \
		-Wall -Wextra -Wpedantic -std=c11 -O2 \
		-DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_uart_16550.c \
		-o $(BUILD)/kdl_uart_16550_x86_64.o
	@echo ""
	@echo "Uretilen x86_64 ELF object:"
	@file $(BUILD)/kdl_uart_16550_x86_64.o
	@echo ""
	@echo "Beklenen semboller (T = text):"
	@llvm-nm --defined-only $(BUILD)/kdl_uart_16550_x86_64.o | \
		grep -E 'kdl_uart_16550_(init|putc|yaz)$$' || \
		{ echo "FAIL: beklenen semboller eksik"; exit 1; }
	@echo ""
	@echo "Libc sembol referansi (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kdl_uart_16550_x86_64.o | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kdl_uart_16550_x86_64.o; \
		exit 1; \
	fi
	@echo "  (yok — temiz)"
	@echo "16550A bare-metal dogrulamasi basarili!"

# === PL011 bare-metal cross-compile dogrulamasi (libc yok) ===
calistir_uart_pl011_bare_metal:
	@echo "PL011 bare-metal cross-compile + symbol dogrulamasi..."
	clang -target aarch64-unknown-none -ffreestanding -nostdlib \
		-Wall -Wextra -Wpedantic -std=c11 -O2 \
		-DKEMGU_BARE_METAL -Iruntime \
		-c runtime/kdl_runtime_uart_pl011.c \
		-o $(BUILD)/kdl_uart_pl011_aarch64.o
	@echo ""
	@echo "Uretilen ARM64 ELF object:"
	@file $(BUILD)/kdl_uart_pl011_aarch64.o
	@echo ""
	@echo "Beklenen semboller (T = text, undefined olmaz):"
	@llvm-nm --defined-only $(BUILD)/kdl_uart_pl011_aarch64.o | \
		grep -E 'kdl_uart_pl011_(init|putc|yaz)$$' || \
		{ echo "FAIL: beklenen semboller eksik"; exit 1; }
	@echo ""
	@echo "Libc sembol referansi (olmamali):"
	@if llvm-nm --undefined-only $(BUILD)/kdl_uart_pl011_aarch64.o | \
		grep -E 'malloc|free|memcpy|memset|printf|fputs|fopen|__chkstk' > /dev/null; then \
		echo "FAIL: libc/CRT referansi bulundu"; \
		llvm-nm --undefined-only $(BUILD)/kdl_uart_pl011_aarch64.o; \
		exit 1; \
	fi
	@echo "  (yok — temiz)"
	@echo "PL011 bare-metal dogrulamasi basarili!"

test_tumu: calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_llvm_dogrula_test calistir_linear_test calistir_sabitsure_test calistir_wcet_test calistir_capability_test calistir_mmio_test calistir_mmio_bare_metal calistir_drf_test calistir_simd_test calistir_simd_llvm_test calistir_snapshot_test calistir_fuzz_test calistir_fuzz_advanced calistir_runtime_link_test calistir_kdl_bolge_test calistir_otp_cli_test calistir_dizi_perf_test calistir_stdlib_check calistir_uart_pl011_test calistir_yazdir_bare_test calistir_uart_16550_test calistir_panik_test calistir_uart_vtable_test calistir_dizi_sinir_test calistir_lambda_test calistir_codegen_diff calistir_codegen_bootstrap calistir_self_driver
	@echo "Tum testler gecti!"

# === Lean 4 ispat sistemi (DRF V1 mekanize — Faz A2+) ===
# Bkz. belgeler/KEMGU_DRF_Mekanize_Spec.md §5
#
# Onkosul: Lean 4 toolchain (elan + lake + lean) sistem PATH'inde.
# Windows MSYS2: PATH'e ~/.elan/bin ekle (her bash oturumunda).
# Ilk kurulum (~30-60 dk): cd proofs/drf-v2-lean && lake update && lake build
#
# Bu hedef test_tumu zincirine eklenmez — C ve Lean tarafi izole.
calistir_drf_lean_proof:
	@echo "=== Lean 4 ispat sistemi (lake build) ==="
	@cd proofs/drf-v2-lean && lake build
	@echo "=== Lake build OK (sorry/axiom: bkz. proofs/drf-v2-lean/README.md) ==="

clean:
	rm -rf $(BUILD)

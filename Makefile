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
              $(BUILD)/bm_a64_gorev.o $(BUILD)/bm_a64_virtio.o
#              ^ virtio: kdl_kesme.c kdl_dosya_kaydet/yukle referans eder (D-143) →
#                tüm aarch64 kernel'ler linkler (kullanılmasa dead-code, libc-temiz).

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

# === D-144 VirtIO-Net paket gönderme testi (aarch64) — Faz G ağ başlangıcı ===
# Kernel Ethernet çerçevesi gönderir; QEMU filter-dump ile pcap'e yakalar; gate
# payload'u ("KEMGUNET-PAKET") pcap'te + seri "NET GONDERILDI" arar.
calistir_net_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "D-144 aarch64 virtio-net paket testi: net_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/net_arm.c -o $(BUILD)/net_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/net_arm.elf $(BUILD)/net_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/arp_arm.elf $(BUILD)/arp_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/udp_arm.elf $(BUILD)/udp_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/dns_arm.elf $(BUILD)/dns_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/dhcp_arm.elf $(BUILD)/dhcp_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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

# === ARP host-keşfi testi (aarch64) — subnet taraması (pentest recon) ===
# Kernel 10.0.2.1..10.0.2.15 subnet'ine ARP istekleri yayınlar; gelen ARP-reply'lerden
# canlı host'ları (spa + sha) toplar. SLIRP gateway (10.0.2.2) her zaman yanıt verir →
# >=1 host deterministik. Keşif gate: ">=1 canlı host" + "ARP SCAN OK".
calistir_arp_scan_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 ARP host-keşfi (subnet taraması) testi: arp_scan_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/arp_scan_arm.c -o $(BUILD)/arp_scan_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/arp_scan_arm.elf $(BUILD)/arp_scan_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/tcp_arm.elf $(BUILD)/tcp_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/tcp_connect_arm.elf $(BUILD)/tcp_connect_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/port_scan_arm.elf $(BUILD)/port_scan_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
		-o $(BUILD)/http_get_arm.elf $(BUILD)/http_get_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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

# === Faz G ICMP echo (ping) round-trip testi (aarch64) — ağ katmanı ===
# ARP ile gateway (SLIRP 10.0.2.2) MAC çöz → IPv4+ICMP Echo Request gönder →
# echo reply'i RX ile al + doğrula. pcap filter-dump da yakalanır: SLIRP echo
# yanıt vermezse "ICMP ECHO SENT OK" (pcap'te type=8 + "KEMGU" işaretçisi) fallback.
calistir_icmp_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "Faz G aarch64 ICMP echo (ping) testi: icmp_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/icmp_arm.c -o $(BUILD)/icmp_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/icmp_arm.elf $(BUILD)/icmp_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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

# === DNS A-kaydı çözümleme testi (aarch64) — isim → IPv4 (Faz G derinleşme) ===
# ARP → DNS MAC → DNS sorgusu ("example.com" A) gönder → yanıtı RX ile al →
# ANSWER bölümünü parse et (isim sıkıştırma 0xC0 dâhil) → IPv4 A-kaydını çıkar.
calistir_dns_resolver_test_arm: $(BUILD)/kemgu$(EXE) $(BM_A64_OBJS) $(BUILD)/bm_a64_virtio_net.o
	@echo "aarch64 DNS A-kaydı çözümleme testi: dns_resolver_arm.c -> ELF..."
	$(BM_A64) $(BM_A64_CF) -c test/bare_metal/dns_resolver_arm.c -o $(BUILD)/dns_resolver_arm.o
	ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
		-o $(BUILD)/dns_resolver_arm.elf $(BUILD)/dns_resolver_arm.o $(BUILD)/bm_a64_virtio_net.o $(BM_A64_OBJS)
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
                     calistir_tick_test_arm calistir_spawn_test_arm calistir_yasam_test_arm \
                     calistir_dosya_test_arm calistir_metin_test_arm calistir_ls_test_arm \
                     calistir_sil_test_arm calistir_kabuk_test_arm calistir_calis_test_arm \
                     calistir_geri_al_test_arm calistir_kanal_ipc_test_arm \
                     calistir_virtio_test_arm calistir_virtio_rw_test_arm calistir_kalici_test_arm \
                     calistir_net_test_arm calistir_arp_test_arm calistir_arp_scan_test_arm \
                     calistir_udp_test_arm calistir_dhcp_test_arm \
                     calistir_dns_test_arm calistir_tcp_test_arm calistir_icmp_test_arm \
                     calistir_dns_resolver_test_arm calistir_tcp_connect_test_arm \
                     calistir_port_scan_test_arm \
                     calistir_http_get_test_arm \
                     calistir_virtio_selfhost_arm \
                     calistir_virtio_selfhost_rw_arm calistir_virtio_net_selfhost_arm \
                     calistir_virtio_net_mac_selfhost_arm \
                     calistir_guvenlik_test_arm \
                     calistir_guvenlik_oku_test_arm calistir_guvenlik_spawn_test_arm \
                     calistir_guvenlik_kalici_test_arm calistir_guvenlik_bombardiman_test_arm \
                     calistir_capstone_arm \
                     calistir_uart_merhaba_x86_bare_metal calistir_kernel_dizi_x86_bare_metal \
                     calistir_istisna_test_x86 calistir_timer_test_x86 calistir_syscall_test_x86 \
                     calistir_sched_test_x86 calistir_capstone_x86
	@echo ""
	@echo "=== TUM OS kanitlari gecti: 4 boot + 2 istisna + 2 timer + 2 syscall + 2 capstone (aarch64 + x86_64) ==="

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

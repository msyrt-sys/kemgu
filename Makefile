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
       $(SRCDIR)/escape.c $(SRCDIR)/llvm.c $(SRCDIR)/json.c $(SRCDIR)/lsp.c \
       $(SRCDIR)/wcet.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean test calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_linear_test calistir_sabitsure_test calistir_wcet_test calistir_capability_test calistir_simd_test calistir_simd_llvm_test calistir_stdlib_check calistir_kripto_check calistir_arm64_test calistir_snapshot_test calistir_fuzz_test calistir_fuzz_advanced calistir_runtime_link_test calistir_otp_cli_test calistir_dizi_perf_test calistir_uart_pl011_test calistir_uart_pl011_bare_metal calistir_yazdir_bare_test calistir_yazdir_bare_bare_metal calistir_uart_merhaba_bare_metal calistir_uart_16550_test calistir_uart_16550_bare_metal calistir_panik_test calistir_panik_bare_metal calistir_uart_vtable_test calistir_qemu_smoke calistir_uart_echo_bare_metal bench test_tumu

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

# === Sabitsure (Constant-Time) Spec V1 testi (Clang64 + ASan — full pipeline) ===

$(BUILD)/test_sabitsure$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                               $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                               $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                               $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                               $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                               $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                               $(TESTDIR)/test_sabitsure.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Realtime (WCET) Spec V1 testi (Clang64 + ASan — full pipeline + wcet) ===

$(BUILD)/test_wcet$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                          $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                          $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                          $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                          $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                          $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                          $(SRCDIR)/wcet.c \
                          $(TESTDIR)/test_wcet.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === Capability (Object-Capability) Spec V1 testi (Clang64 + ASan) ===

$(BUILD)/test_capability$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                                $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                                $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                                $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                                $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                                $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                                $(TESTDIR)/test_capability.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === DRF (Data Race Freedom) V1 testi (Clang64 + ASan) ===

$(BUILD)/test_drf$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                         $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                         $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                         $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                         $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                         $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
                         $(TESTDIR)/test_drf.c | $(BUILD)
	$(CC_ASAN) $(CFLAGS) $(ASAN_FLAGS) -I$(SRCDIR) -o $@ $^

# === SIMD Spec V1 testi (Clang64 + ASan — full pipeline) ===

$(BUILD)/test_simd$(EXE): $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c \
                          $(SRCDIR)/hata.c $(SRCDIR)/lexer.c \
                          $(SRCDIR)/arena.c $(SRCDIR)/ast.c \
                          $(SRCDIR)/ast_yazdir.c $(SRCDIR)/parser.c \
                          $(SRCDIR)/ifade.c $(SRCDIR)/tip.c \
                          $(SRCDIR)/sembol.c $(SRCDIR)/tip_kontrol.c \
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

$(BUILD)/test_runtime_link$(EXE): $(BUILD)/kdl_runtime.o \
                                   $(TESTDIR)/test_runtime_link.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

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

calistir_llvm_test: $(BUILD)/test_llvm$(EXE) $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	./$(BUILD)/test_llvm$(EXE)

calistir_linear_test: $(BUILD)/test_linear$(EXE)
	./$(BUILD)/test_linear$(EXE)

calistir_sabitsure_test: $(BUILD)/test_sabitsure$(EXE)
	./$(BUILD)/test_sabitsure$(EXE)

calistir_wcet_test: $(BUILD)/test_wcet$(EXE)
	./$(BUILD)/test_wcet$(EXE)

calistir_capability_test: $(BUILD)/test_capability$(EXE)
	./$(BUILD)/test_capability$(EXE)

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

calistir_otp_cli_test: $(BUILD)/test_otp_cli$(EXE) $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	./$(BUILD)/test_otp_cli$(EXE)

calistir_dizi_perf_test: $(BUILD)/test_dizi_perf$(EXE) $(BUILD)/kemgu$(EXE) $(BUILD)/kdl_runtime.o
	./$(BUILD)/test_dizi_perf$(EXE)

# Stdlib tip-kontrolu — saf KEMGU stdlib modullerinin --check'ten gecmesi
# Kutuphane dosyasi varsa karsilik gelen test/stdlib/test_<modul>.kem ile
# birlestirilip --check'ten gecirilir (tek dosya derleme, import yok).
# Test dosyasi yoksa kutuphane tek basina kontrol edilir.
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
		timeout 5 qemu-system-aarch64 -M virt -cpu cortex-a72 \
			-nographic -kernel $(BUILD)/kernel.elf < /dev/null \
			2>/dev/null > $(BUILD)/qemu_smoke.out || true; \
		echo "--- QEMU stdout ---"; \
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

# === Bare-Metal Hello World (Track B Kalem 3) ===
# uart_merhaba.kem -> ARM64 ELF + libc-yok dogrulamasi.
# Pipeline: kemgu --llvm | clang -target aarch64-unknown-none -> kernel.elf
calistir_uart_merhaba_bare_metal: $(BUILD)/kemgu$(EXE)
	@echo "Bare-metal hello world: uart_merhaba.kem -> ARM64 ELF..."
	./$(BUILD)/kemgu$(EXE) --llvm test/ornekler/uart_merhaba.kem > $(BUILD)/uart_merhaba.ll
	clang -target aarch64-unknown-none -ffreestanding -nostdlib -O2 \
		-x ir $(BUILD)/uart_merhaba.ll -c -o $(BUILD)/uart_merhaba.o
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
		-o $(BUILD)/kernel.elf \
		$(BUILD)/start_aarch64.o $(BUILD)/uart_merhaba.o \
		$(BUILD)/yazdir_bare_bm.o $(BUILD)/uart_pl011_bm.o
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

test_tumu: calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_linear_test calistir_sabitsure_test calistir_wcet_test calistir_capability_test calistir_drf_test calistir_simd_test calistir_simd_llvm_test calistir_snapshot_test calistir_fuzz_test calistir_fuzz_advanced calistir_runtime_link_test calistir_otp_cli_test calistir_dizi_perf_test calistir_stdlib_check calistir_uart_pl011_test calistir_yazdir_bare_test calistir_uart_16550_test calistir_panik_test calistir_uart_vtable_test
	@echo "Tum testler gecti!"

clean:
	rm -rf $(BUILD)

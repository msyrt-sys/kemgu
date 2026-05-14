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

.PHONY: all clean test calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_linear_test calistir_sabitsure_test calistir_wcet_test calistir_capability_test calistir_simd_test calistir_simd_llvm_test calistir_stdlib_check calistir_kripto_check calistir_arm64_test calistir_snapshot_test calistir_fuzz_test calistir_fuzz_advanced calistir_runtime_link_test calistir_otp_cli_test calistir_dizi_perf_test bench test_tumu

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

test_tumu: calistir_lexer_test calistir_arena_test calistir_ast_test calistir_parser_test calistir_tip_test calistir_sembol_test calistir_tip_kontrol_test calistir_bolge_test calistir_bolge_atama_test calistir_escape_test calistir_json_test calistir_lsp_test calistir_llvm_test calistir_linear_test calistir_sabitsure_test calistir_wcet_test calistir_capability_test calistir_simd_test calistir_simd_llvm_test calistir_snapshot_test calistir_fuzz_test calistir_fuzz_advanced calistir_runtime_link_test calistir_otp_cli_test calistir_dizi_perf_test calistir_stdlib_check
	@echo "Tum testler gecti!"

clean:
	rm -rf $(BUILD)

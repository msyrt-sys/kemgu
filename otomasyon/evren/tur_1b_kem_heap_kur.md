# Tur 1b — kem_heap_kur — `deepseek-v4.1-flash`

- Tarih: 2026-10-12 05:49:20 · süre 59.0 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 20656, "total_tokens": 28934, "completion_tokens": 8278, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 20608}, "completion_tokens_details": {"reasoning_tokens": 4984}}`
- Değişiklik blokları: 4 — duzenle:boot/start_x86_64.S, yeni:test/ornekler/kem_os_x86.kem, duzenle:Makefile, duzenle:Makefile
- İstem: [tur_1b_kem_heap_kur_istem.md](tur_1b_kem_heap_kur_istem.md)

## Yanıt

```
```duzenle:boot/start_x86_64.S
<<<<<<< ESKI
    call    kdl_idt_kur            /* C3a: IDT kur (isr0..31 → istisna işleyici) */
    call    main                   /* KEMGU kernel main() */
=======
    call    kdl_idt_kur            /* C3a: IDT kur (isr0..31 → istisna işleyici) */

    /* 2c (aarch64 eşi): heap penceresini main'den ÖNCE kur. Saf-.kem allocator
     * (runtime/kem_heap.kem) kem_bump/kem_son'u bu çağrıdan alır; çağrılmazsa
     * kem_son=0 kalır → her @malloc null döner → dizi işlevleri sessizce 0
     * basar, panik YOK. kem_heap.kem ile bağlanMAyan C-malloc'lu x86
     * kernel'lerinde runtime/kdl_bare_heap.c'deki ZAYIF kem_heap_kur no-op'u
     * devreye girer (strong .kem sürümü override eder) → eski kernel'ler
     * bozulmaz. */
    movq    $__heap_start, %rdi    /* 1. argüman: heap tabanı (linker sembolü) */
    movq    $__heap_end, %rsi      /* 2. argüman: heap üst sınırı (dışlayıcı) */
    call    kem_heap_kur

    call    main                   /* KEMGU kernel main() */
>>>>>>> YENI
```
```yeni:test/ornekler/kem_os_x86.kem
// kem_os_x86.kem — İş 5: x86_64 kanıt çekirdeğinin `main`'i. kem_heap.kem ile
// CAT edilir (tek derleme birimi). Normal (çıplak OLMAYAN) işlev → prologu
// bölgeyi kdl_global_bolge_al → kdl_bolge_olustur → @malloc (saf-.kem) zinciriyle
// kurar; bu test TAM O ZİNCİĞİ ölçer: start_x86_64.S kem_heap_kur'u main'den
// önce çağırmazsa saf-.kem malloc null döner ve toplam=0 olur (panik YOK —
// bu yüzden marker değil DEĞER yazdırılıp denetlenir, D-264 dersi).
//
// Amaç: bölge + dizi + metin yüzeyini x86 QEMU'da DAVRANIŞSAL kanıtla. Yazdırma
// yerleşikleri (yazdir_metin/yazdir_tam/yazdir_satir) bu çekirdekte C
// runtime/kdl_runtime_yazdir_bare.c + 16550 UART üzerinden gider (İş 2 sonra
// .kem UART'a çevirecek — şimdilik dokunulmaz).
//
// Beklenen başarı çıktısı: "X86 HEAP DIZI OK toplam=55"

işlev main() -> tam32 {
    değişken d: Dizi<tam32> = dizi_olustur();

    // 1..10 ekle (bölge dizi büyümesi 4→8→16; saf-.kem kem_dizi_buyut).
    değişken i: tam32 = 1;
    için i = 1; i <= 10; i = i + 1 {
        dizi_ekle(d, i);
    }

    // Sınır-kontrollü oku ve topla (beklenen: 55).
    değişken toplam: tam32 = 0;
    değişken j: tam32 = 0;
    değişken n: tam32 = dizi_boyut(d);
    için j = 0; j < n; j = j + 1 {
        toplam = toplam + dizi_al(d, j);
    }

    // Değeri SABİT yazma: gerçek toplamı bas (marker değil DEĞER denetimi).
    eğer toplam == 55 {
        yazdir_metin("X86 HEAP DIZI OK toplam=");
        yazdir_tam(toplam);
        yazdir_satir();
    }
    değilse {
        yazdir_metin("X86 HEAP DIZI HATA toplam=");
        yazdir_tam(toplam);
        yazdir_satir();
    }

    ver 0;
}
```
```duzenle:Makefile
<<<<<<< ESKI
 calistir_qemu_cekirdek kemgu_self
=======
 calistir_qemu_cekirdek calistir_kem_os_x86 kemgu_self
>>>>>>> YENI
```
```duzenle:Makefile
<<<<<<< ESKI
	else echo "QEMU yok — x86 hello atlandi."; fi

# === Bare-Metal Dizi Kernel x86_64 (C1-x86) — AYNI kernel, PVH boot ===
=======
	else echo "QEMU yok — x86 hello atlandi."; fi

# === Bare-Metal KEMGU-OS x86_64 (İş 5) — saf-.kem allocator + bölge ile boot ===
# kem_heap.kem + kem_os_x86.kem CAT → malloc/free/memcpy/memset/bolge/dizi/metin
# TANIMLARI .o'dan gelir; bm_x86_heap.o ve bm_x86_bolge.o BİLEREK YOK (aksi hâlde
# C sürümleri .kem sembolleriyle çakışır). start_x86_64.S kem_heap_kur'u
# main'den önce çağırır → kem_son dolu. Üç kanıt: (1) nm'de ` T malloc` ve
# ` T kem_heap_kur`; (2) objdump'da kem_heap_kur'a giden call satırı (sabit
# metin DEĞİL, objdump'ın kendi satırı); (3) QEMU seri çıktıda
# "X86 HEAP DIZI OK toplam=55".
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
		$(BUILD)/bm_x86_panik.o
	@echo "Bare-metal kem_os x86_64 (saf-.kem allocator): CAT -> LLVM IR..."
	cat runtime/kem_heap.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
	./$(BUILD)/kemgu$(EXE) --llvm --mimari x86_64 $(BUILD)/kem_os_x86_comb.kem > $(BUILD)/kem_os_x86_ham.ll
	awk -f test/strip_defined_declares.awk $(BUILD)/kem_os_x86_ham.ll > $(BUILD)/kem_os_x86.ll
	$(BM_X86) -O2 -Wno-override-module -x ir $(BUILD)/kem_os_x86.ll -c -o $(BUILD)/kem_os_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o $(BUILD)/kem_os_x86.elf \
		$(BUILD)/kem_os_x86.o \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
		$(BUILD)/bm_x86_panik.o
	@echo "  Kanit 1: sembol kaynagi (saf-.kem malloc/kem_heap_kur)..."
	@if ! llvm-nm $(BUILD)/kem_os_x86.o | grep -q ' T malloc'; then \
		echo "FAIL: malloc .kem'den gelmedi (T malloc yok)"; exit 1; fi
	@if ! llvm-nm $(BUILD)/kem_os_x86.o | grep -q ' T kem_heap_kur'; then \
		echo "FAIL: kem_heap_kur .kem'den gelmedi (T kem_heap_kur yok)"; exit 1; fi
	@echo "  Kanit 2: kem_heap_kur call satiri (objdump'un kendi satiri)..."
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q 'call.*<kem_heap_kur>'; then \
		echo "FAIL: kem_heap_kur call satiri yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep 'call.*<kem_heap_kur>'
	@echo "  Kanit 3: QEMU davranis (toplam=55)..."
	@if ! command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		echo "FAIL: qemu-system-x86_64 yok — qemu kurun (sessiz atlama yok, D-486)"; exit 1; fi
	@rm -f $(BUILD)/kem_os_x86.out; \
	timeout 8 qemu-system-x86_64 -kernel $(BUILD)/kem_os_x86.elf -display none \
		-serial file:$(BUILD)/kem_os_x86.out 2>/dev/null || true; \
	echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/kem_os_x86.out; echo "--- son ---"; \
	if grep -q "X86 HEAP DIZI OK toplam=55" $(BUILD)/kem_os_x86.out; then \
		echo "kem_os x86_64 QEMU testi gecti."; \
	else echo "FAIL: X86 HEAP DIZI OK toplam=55 yok"; exit 1; fi

# === Bare-Metal Dizi Kernel x86_64 (C1-x86) — AYNI kernel, PVH boot ===
>>>>>>> YENI
```

Açıklama:
- `boot/start_x86_64.S`: `kdl_idt_kur` ile `main` arasına `kem_heap_kur(__heap_start, __heap_end)` çağrısı eklendi (SysV: rdi/rsi). Yorum aarch64/2c muadili olarak neden gerekli olduğunu ve `kdl_bare_heap.c`'deki ZAYIF no-op sayesinde C-malloc'lu eski x86 kernel'lerinin bozulmadığını açıklıyor.
- `test/ornekler/kem_os_x86.kem` (yeni): normal `işlev main() -> tam32`; `Dizi<tam32>` oluşturup 1..10 ekliyor, `için` ile toplayıp değeri `yazdir_tam` ile basıyor (sabit "55" değil, gerçek toplam).
- `Makefile`: `.PHONY` ilk satırına `calistir_kem_os_x86` eklendi; `calistir_uart_merhaba_x86_bare_metal`'in hemen ardına üç kanıtlı (nm sembol, objdump call satırı, QEMU davranış) yeni hedef eklendi. Bağlamadaki nesne kümesi birebir kullanıldı, `bm_x86_heap.o`/`bm_x86_bolge.o` yok; `--llvm` ile `awk` AYRI satırlarda (D-609); QEMU yoksa GÜRÜLTÜLÜ `exit 1` (D-486).

VARSAYIMLAR
- KEMGU'da yığın-dizisi oluşturma builtin'i `dizi_olustur()` (argümansız, tip `değişken` bildiriminden çıkarımlı) olarak varsayıldı; `dizi_ekle(d, x)`, `dizi_al(d, i)`, `dizi_boyut(d)` görevdeki adlarla birebir.
- `için` döngüsü C-benzeri başlık kabul eder: `için başlangıç; koşul; adım { }` (`iken` örneklerindeki `değişken = …` biçimiyle uyumlu). Nokta-aralık/koleksiyon biçimi varsayılmadı.
- `değilse` anahtar sözcüğü `eğer`in karşıtı olarak kullanıldı (kem_heap.kem `eğer/değilse` deseniyle uyumlu).
- `kem_heap_kur` sembolü `@kem_heap_kur` (saf-.kem `çıplak işlev kem_heap_kur(taban, son)`) ile eşleşir; C'deki `weak kem_heap_kur` bunu override eder.
- `__heap_start`/`__heap_end` linker script'te mutlak adres sembolü → `movq $sym, %rdi` immediate yüklemesi doğru kabul edildi.

## Düşünme (reasoning)

We need to produce edits. Let me carefully construct.

1. boot/start_x86_64.S edit: insert kem_heap_kur call between idt_kur and main.

2. New file test/ornekler/kem_os_x86.kem

3. Makefile edits: .PHONY line and new target.

Let me check the KEMGU syntax. Looking at kem_heap.kem pattern: `küresel değişken`, `çıplak işlev`, normal `işlev`. Types: `tam32`, `tam64`, `tam8`, `metin`, `doğru`. Comment style `//`.

For the main function, we need normal (non-bare) işlev because prolog calls kdl_bolge_olustur.

Body: create Dizi<tam32>, dizi_ekle 1..10, sum with için loop.

What are the KEMGU primitives? Let me search context for "Dizi" usage. We have primitives kdl_dizi_*. The language likely has `Dizi<tam32>` type and `dizi_ekle`, `dizi_boyut`, `dizi_al` methods or functions.

I need to write plausible KEMGU. Let me infer. Since we don't have examples, I'll follow natural KEMGU syntax. Something like:

```
işlev main() -> tam32 {
    değişken d: Dizi<tam32> = Dizi<1tam32>??? 
```

Hmm risky. Let me look for hints. The task says: "`Dizi<tam32>` oluştur, `dizi_ekle` ile 1..10 ekle". And `yazdir_tam(t)`, `yazdir_metin(m)`, `yazdir_satir()`.

I need to guess the construction syntax. Common in these languages: `dizi_olustur()` or `Dizi<tam32>::yeni()`. Given kem_heap has kdl_dizi_olustur with parameters (bolge, eleman_byte), the language likely provides a builtin.

Actually since we can't verify, let me try a simple approach that matches KEMGU. Search context: not present. Let me assume:

```
işlev main() -> tam32 {
    değişken d: Dizi<tam32> = dizi_olustur_tam32();
```

Hmm. Given uncertainty, I need something plausible. Let me think about what kem_os uses. Actually there's a hint: "dizi_ekle" and "için". Let me write with a general syntax.

I'll use:
```
değişken d: Dizi<tam32> = dizi_olustur();
dizi_ekle(d, i);
```

Actually for sum we may use `dizi_al(d, i)` or `d[i]`. Let's use `d[i]`.

Let me structure:

```
işlev main() -> tam32 {
    değişken d: Dizi<tam32> = Dizi<tam32>.yeni();
    değişken i: tam32 = 1;
    iken i <= 10 {
        dizi_ekle(d, i);
        i = i + 1;
    }
    değişken toplam: tam32 = 0;
    değişken j: tam32 = 0;
    iken j < dizi_boyut(d) {
        toplam = toplam + dizi_al(d, j);
        j = j + 1;
    }
    eğer toplam == 55 {
        yazdir_metin("X86 HEAP DIZI OK toplam=");
        yazdir_tam(toplam);
        yazdir_satir();
    } değilse {
        yazdir_metin("X86 HEAP DIZI HATA toplam=");
        yazdir_tam(toplam);
        yazdir_satir();
    }
    ver 0;
}
```

But task says use `için` loop. Let me use için. KEMGU `için` syntax uncertain. Could be `için i içinde 1..10 { }` or C-style. Let me just use it loosely.

Given I can't verify, I'll use a reasonable syntax. Let me use:

```
için i içinde 1..=10 {
    dizi_ekle(d, i);
}
```

Hmm. Risky. Perhaps simpler to keep `iken` but task explicitly says `için`. Let me try `için i = 1; i <= 10; i++ {}`. That's C-like. But KEMGU seems to use natural language.

Looking at kem_heap.kem, loops use `iken cond { }` (while). No for examples. I'll go with `için` C-like since that's likely what "için" maps to.

Actually a safer guess based on KEMGU Turkish style: `için i değer_1'den 10'a { }`. Too risky.

Let me just do:
```
için i = 1; i <= 10; i = i + 1 {
```

Hmm. I'll go with a decision and note as assumption.

Actually let me try to be more careful. Given D-608 style KEMGU snippets in the codebase use `iken`, and the task explicitly says `için`, KEMGU probably has a range-for. Common syntax in such language: `için i içinde 1..10`. Let me use that and note as assumption.

Wait — actually maybe simpler: use `için` with iterable. Since I can't be sure, let me use a construct I can argue for: `için i 1'den 10'a kadar`. Too speculative.

I'll go with:
```
için i = 1; i <= 10; i = i + 1 {
    dizi_ekle(d, i);
}
```

And note as assumption.

For the Dizi creation, let me look at common KEMGU. Since normal işlev main prologu calls kdl_bolge_olustur, there must be a builtin for creating arrays. Given C kdl_dizi_olustur(bolge, eleman_byte), and language likely emits `Dizi<tam32>` creation as `Dizi<tam32>{}` or `dizi_olustur(tam32)`.

Let me use: `değişken d: Dizi<tam32> = Dizi<tam32>{};`

Actually a common pattern in these languages is `[]` for empty array. Let me just do that.

Hmm this is getting quite speculative. Let me settle:

```
değişken d: Dizi<tam32> = Dizi<tam32>.yeni();
```
No, that's Swift-like.

Given the KEMGU fragments I've seen use functions with underscores, let me try:
```
değişken d: Dizi<tam32> = dizi_olustur();
dizi_ekle(d, i);
değişken n: tam32 = dizi_boyut(d);
değişken v: tam32 = dizi_al(d, j);
```

Or if methods: `d.ekle(i)`, `d.boyut()`. Given task says `dizi_ekle`, `dizi_al` explicitly with d as first arg pattern works.

For dizi_olustur it might need eleman tip. Let me try `dizi_olustur(Dizi<tam32>)`. Ugly.

I'll write:
```
değişken d: Dizi<tam32> = dizi_yeni_tam32();
```
Too made up.

OK, decision: I'll follow the task's guidance literally. It says "Dizi<tam32> oluştur, dizi_ekle ile 1..10 ekle". So use `Dizi<tam32>` type annotation and `dizi_ekle`.

Use:
```
değişken d: Dizi<tam32> = dizi_olustur();
```

Let me finalize and note all assumptions.

Also need `metin_uzunluk`? No, use yazdir_metin.

The output format: use yazdir_metin, yazdir_tam, yazdir_satir.

Alright. Also should return tam32 (0).

Now Makefile edits. Insert new target after the hello x86 target.

PHONY edit: ESKI ` calistir_qemu_cekirdek kemgu_self` → ` calistir_qemu_cekirdek calistir_kem_os_x86 kemgu_self`.

Let me write the new target:

```
# === Bare-Metal KEMGU-OS x86_64 (İş 5) — saf-.kem allocator + bölge ile boot ===
# kem_heap.kem + kem_os_x86.kem CAT → malloc/free/bölge/memcpy/dizi/metin
# TANIMLARI .o'dan gelir (bm_x86_heap.o/bm_x86_bolge.o YOK). kem_heap_kur
# start_x86_64.S'te main'den önce çağrılır → kem_son dolu. Kanıtlar: (1) nm'de
# malloc+kem_heap_kur T; (2) objdump'da kem_heap_kur call satırı; (3) QEMU
# seri çıktıda "X86 HEAP DIZI OK toplam=55".
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
		$(BUILD)/bm_x86_panik.o
	@echo "Bare-metal kem_os x86_64 (saf-.kem allocator): CAT -> LLVM IR..."
	cat runtime/kem_heap.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
	./$(BUILD)/kemgu$(EXE) --llvm --mimari x86_64 $(BUILD)/kem_os_x86_comb.kem > $(BUILD)/kem_os_x86_ham.ll
	awk -f test/strip_defined_declares.awk $(BUILD)/kem_os_x86_ham.ll > $(BUILD)/kem_os_x86.ll
	$(BM_X86) -O2 -Wno-override-module -x ir $(BUILD)/kem_os_x86.ll -c -o $(BUILD)/kem_os_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o $(BUILD)/kem_os_x86.elf \
		$(BUILD)/kem_os_x86.o \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
		$(BUILD)/bm_x86_panik.o
	@echo "  Kanit 1: sembol kaynagi (saf-.kem malloc/kem_heap_kur)..."
	@if ! llvm-nm $(BUILD)/kem_os_x86.o | grep -q ' T malloc'; then \
		echo "FAIL: malloc .kem'den gelmedi"; exit 1; fi
	@if ! llvm-nm $(BUILD)/kem_os_x86.o | grep -q ' T kem_heap_kur'; then \
		echo "FAIL: kem_heap_kur .kem'den gelmedi"; exit 1; fi
	@echo "  Kanit 2: kem_heap_kur call satiri (objdump)..."
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q 'call.*<kem_heap_kur>'; then \
		echo "FAIL: kem_heap_kur call yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep 'call.*<kem_heap_kur>'
	@echo "  Kanit 3: QEMU davranis (toplam=55)..."
	@if ! command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		echo "FAIL: qemu-system-x86_64 yok — qemu kurun (sessiz atlama yok)"; exit 1; fi
	@rm -f $(BUILD)/kem_os_x86.out; \
	timeout 8 qemu-system-x86_64 -kernel $(BUILD)/kem_os_x86.elf -display none \
		-serial file:$(BUILD)/kem_os_x86.out 2>/dev/null || true; \
	echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/kem_os_x86.out; echo "--- son ---"; \
	if grep -q "X86 HEAP DIZI OK toplam=55" $(BUILD)/kem_os_x86.out; then \
		echo "kem_os x86_64 QEMU testi gecti."; \
	else echo "FAIL: X86 HEAP DIZI OK toplam=55 yok"; exit 1; fi
```

Need to check: "bm_x86_heap.o ve bm_x86_bolge.o BİLEREK YOK". Do I have to worry about missing symbols? bm_x86_heap.o normally provides memcpy, memset, kdl_global_bolge_al, kdl_panik, kdl_dizi. But in this CAT, kem_heap.kem provides malloc, free, memcpy, memset, kdl_global_bolge_al, kdl_bolge_*, kdl_dizi_*, kdl_metin_*, kdl_mmio_*, so all good. kdl_panik needed? kem_heap.kem calls kem_panik, which is in the same file. Good. kdl_bolge.h symbols not needed since .kem provides. OK.

But wait — vag — the CAT includes only kem_heap.kem + kem_os_x86.kem. bm_x86_bolge.o provides kdl_bolge_ayir etc. in C. But now .kem provides them too — duplicate symbols! That's why bm_x86_bolge.o is excluded. Good.

bm_x86_heap.o also provides malloc etc as C — but it's guarded by `#ifndef KEMGU_KEM_MALLOC`. The makefile_x86_kesit.txt shows bm_x86_heap.o compiled without -DKEMGU_KEM_MALLOC. So if we linked it, we'd get duplicate malloc. Hence exclude. Good.

Now for linker script: __heap_start and __heap_end defined. Good.

Now start_x86_64.S: kem_heap_kur call needs rdi/rsi. In 64-bit mode:

```
    movq    $__heap_start, %rdi    /* İş 5: saf-.kem allocator pencere tabanı */
    movq    $__heap_end, %rsi      /* İş 5: heap üst sınırı (dışlayıcı) */
    call    kem_heap_kur
```

Wait, __heap_start is a linker symbol (address). `movq $__heap_start, %rdi` — in GAS, for a symbol at absolute address, this loads the address. But since linker script uses `.` symbol, __heap_start is an absolute address value → `movq $sym, %rdi` works as immediate.

Now the comment about weak no-op.

Let me write:

```
    call    kdl_idt_kur            /* C3a: IDT kur (isr0..31 → istisna işleyici) */

    /* İş 5 (aarch64 adım 2c eşi): heap penceresini main'den ÖNCE kur. Saf-.kem
     * allocator (kem_heap.kem) kem_bump/kem_son'u bu çağrıdan alır; yoksa
     * kem_son=0 kalır → her malloc null → dizi işlevleri sessizce 0 döner.
     * (kem_heap.kem ile bağlanmayan C-malloc'lu x86 kernel'lerinde
     * kdl_bare_heap.c'deki ZAYIF kem_heap_kur no-op'u devreye girer → bozulmaz.) */
    movq    $__heap_start, %rdi    /* 1. arg: heap tabanı */
    movq    $__heap_end, %rsi      /* 2. arg: heap üst sınırı (dışlayıcı) */
    call    kem_heap_kur

    call    main                   /* KEMGU kernel main() */
```

Now the ESKI must match exactly:
```
    call    kdl_idt_kur            /* C3a: IDT kur (isr0..31 → istisna işleyici) */
    call    main                   /* KEMGU kernel main() */
```

Good, insert between.

Now test/ornekler/kem_os_x86.kem content.

Let me finalize KEMGU syntax. I'll go with:

```kemgu
// kem_os_x86.kem — İş 5: x86_64 kanıt çekirdeğinin `main`'i. kem_heap.kem ile
// CAT edilir (tek derleme birimi). Normal (çıplak OLMAYAN) işlev → prologu
// bölgeyi kdl_global_bolge_al → kdl_bolge_olustur → @malloc (saf-.kem) zinciriyle
// kurar; bu test TAM O ZİNCİĞİ ölçer (x86 boot'ta start_x86_64.S kem_heap_kur
// çağırmazsa malloc null döner → toplam=0, panik YOK).
//
// Amaç: sınırlı sayıda ekle/oku/topla ile saf-.kem allocator + bölge + dizi
// yüzeyini x86 QEMU'da DAVRANIŞSAL kanıtla. Yazdırma yerleşikleri (yazdir_metin,
// yazdir_tam, yazdir_satir) bu çekirdekte C kdl_runtime_yazdir_bare.c + 16550
// UART üzerinden gider (İş 2 sonra .kem UART'a çevirecek).
//
// Başarı çıktısı: "X86 HEAP DIZI OK toplam=55"

işlev main() -> tam32 {
    değişken d: Dizi<tam32> = dizi_olustur();
    değişken i: tam32 = 1;
    için i = 1; i <= 10; i = i + 1 {
        dizi_ekle(d, i);
    }
    değişken toplam: tam32 = 0;
    değişken j: tam32 = 0;
    değişken n: tam32 = dizi_boyut(d);
    için j = 0; j < n; j = j + 1 {
        toplam = toplam + dizi_al(d, j);
    }
    eğer toplam == 55 {
        yazdir_metin("X86 HEAP DIZI OK toplam=");
        yazdir_tam(toplam);
        yazdir_satir();
    }
    değilse {
        yazdir_metin("X86 HEAP DIZI HATA toplam=");
        yazdir_tam(toplam);
        yazdir_satir();
    }
    ver 0;
}
```

Wait, syntax for `için` C-like with `;`? Uncertain. Let me commit and note assumption.

Actually if `için` is C-like with `i = 1; i <= 10; i = i + 1` — but "i = i + 1" is a statement. In some languages "i++" would be non-existent in KEMGU (no ++ operator seen). Let me use `i = i + 1`.

Hmm, actually in the `iken` examples they use `i = i + 1`. Good.

Let me also handle: task says `için` ile topla. So both loops should use için? It says "`Dizi<tam32>` oluştur, `dizi_ekle` ile 1..10 ekle, `için` ile topla." So the `için` is for sum. OK both work.

Now for `dizi_olustur()` — uncertain signature. Could need element type. Let me just use `dizi_olustur()`.

Alternatively use type-directed: `Dizi<tam32>.olustur()`. Hmm.

Final: I'll use `dizi_olustur()` and note as assumption.

Actually, thinking more, maybe KEMGU has a keyword `yeni` for constructing. E.g., `değişken d: Dizi<tam32> = Dizi<().yeni();` No.

I'll commit.

Now `.PHONY` edit.

ESKI: ` calistir_qemu_cekirdek kemgu_self`
YENI: ` calistir_qemu_cekirdek calistir_kem_os_x86 kemgu_self`

Let me produce the outputs.

For Makefile new target, the placeholder says "\t\t" for tabs? Let me use actual tab character representation in the output blocks. The user's format uses markdown code blocks. I need literal tab characters.

Actually the format says tabs. I'll write with real tab characters.

Let me build the final response.

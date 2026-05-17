[1mdiff --git a/CLAUDE.md b/CLAUDE.md[m
[1mindex b78ace5..517c095 100644[m
[1m--- a/CLAUDE.md[m
[1m+++ b/CLAUDE.md[m
[36m@@ -93,7 +93,20 @@[m [mkemgu/[m
 │       ├── hasta.kem                  — Mevcut örnek (TAMAMLANDI ✓)[m
 │       ├── fibonacci.kem              — Özyinelemeli fibonacci (TAMAMLANDI ✓)[m
 │       ├── yapilar.kem                — Generic yapılar + referans (TAMAMLANDI ✓)[m
[31m-│       └── eslesme.kem                — Pattern matching + döngü (TAMAMLANDI ✓)[m
[32m+[m[32m│       ├── eslesme.kem                — Pattern matching + döngü (TAMAMLANDI ✓)[m
[32m+[m[32m│       ├── lambda_boyut.kem           — Lambda IIFE + boyut<T> + lifting (ADIM 14)[m
[32m+[m[32m│       ├── faz1_kapsamli.kem          — Tüm OS-ready özellikler tek dosyada (ADIM 14 ek)[m
[32m+[m[32m│       ├── kernel/                    — Bare-metal kernel scaffold (ADIM 15)[m
[32m+[m[32m│       │   ├── kernel.kem             — VGA write + halt + _start naked entry[m
[32m+[m[32m│       │   ├── multiboot.kem          — Multiboot2 başlık (inline asm)[m
[32m+[m[32m│       │   ├── kernel.ld              — Linker script (1MB layout)[m
[32m+[m[32m│       │   ├── gdt.kem                — Global Descriptor Table (ADIM 16)[m
[32m+[m[32m│       │   ├── idt.kem                — Interrupt Descriptor Table (ADIM 16)[m
[32m+[m[32m│       │   ├── paging.kem             — Sayfa tablolari PML4/PDPT/PD/PT (ADIM 16)[m
[32m+[m[32m│       │   ├── serial.kem             — UART 16550 driver scaffold (ADIM 16)[m
[32m+[m[32m│       │   ├── pci.kem                — PCI Configuration Space (ADIM 16)[m
[32m+[m[32m│       │   └── README.md              — Derleme talimatları[m
[32m+[m[32m│       └── concurrency.kem            — Task + kanal kullanim ornegi (Katman 2)[m
 ```[m
 [m
 ---[m
[36m@@ -184,7 +197,7 @@[m [mRaw string:         r#"..."#[m
 eğer, değilse, için, iken, eşleş, ver, işlev, yapı, özellik, modül,[m
 değişken, sabit, doğru, yanlış, boş, ve, veya, değil, kullan, dışa,[m
 tamam, hata, bölge, uygula, kendin, seçimlik, sonuç, değer, hiç,[m
[31m-güvensiz[m
[32m+[m[32mgüvensiz, boyut[m
 ```[m
 [m
 ### Tip Sistemi[m
[36m@@ -383,11 +396,223 @@[m [mecho 'işlev main() -> tam32 { ver 1 + 2 * 3 + 35; }' > x.kem[m
 ./x.exe; echo $?    # → 42 ✓[m
 ```[m
 [m
[31m-### Sıradaki büyük seçenekler:[m
[31m-- **Tam Katman 1 escape analizi** (DFA tabanlı)[m
[31m-- **Bölge çözümleyici Katman 2** (concurrency: R-GÖREV, R-BİRLEŞTİR, R-KANAL)[m
[31m-- **LLVM backend genişletme** (parametreler, kontrol akışı, yapılar, dizi, çağrı)[m
[31m-- **`hiç`/`değer` ifade desteği + pattern binding** (esles desen tanımlayıcıları scope'a)[m
[32m+[m[32m### ADIM 14: Lambda + Pointer Aritmetik + boyut<T> (sizeof) — TAMAMLANDI ✓[m
[32m+[m
[32m+[m[32m**Yeni keyword: `boyut` (31'inci anahtar kelime).** Tip parametresi alir, `dtam64` doner:[m
[32m+[m
[32m+[m[32m```kem[m
[32m+[m[32mboyut<tam32>         // → 4 (dtam64)[m
[32m+[m[32mboyut<*tam32>        // → 8 (x86_64 pointer)[m
[32m+[m[32mboyut<kesirli64>     // → 8[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Bidirectional çıkarsama:** Tamsayı beklenen bağlamlarda otomatik daralma:[m
[32m+[m[32m```kem[m
[32m+[m[32mdeğişken b: tam32 = boyut<tam32>;   // dtam64 yerine tam32[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Pointer aritmetiği (tip kontrol):**[m
[32m+[m[32m- `*T + tamsayi` → `*T`[m
[32m+[m[32m- `tamsayi + *T` → `*T`[m
[32m+[m[32m- `*T - tamsayi` → `*T`[m
[32m+[m[32m- `*T - *T` → `tam64` (pointer farkı — hedef tipleri eşit olmalı, yoksa T028)[m
[32m+[m
[32m+[m[32m**LLVM backend genişletme (lambda + lifting):**[m
[32m+[m[32m- Parametreler: `define i32 @f(i32 %x, i32 %y)` — SSA isim olarak doğrudan[m
[32m+[m[32m- Tanımlayıcı arama: lokal sembol tablosu (parametre + `değişken` bagi)[m
[32m+[m[32m- `değişken x = expr;` → SSA register'a baglar[m
[32m+[m[32m- Dogrudan çağrı: `call i32 @f(...)`[m
[32m+[m[32m- **Lambda lifting:** `(|x| x+1)(41)` → `@__lambda_0` üst düzey fonksiyona dönüşür[m
[32m+[m[32m- Değişkene bağlı lambda: `değişken f = |a,b| a+b; f(10,3)` çalışır (capture YOK)[m
[32m+[m[32m- `boyut<T>` derleme zamanı sabite dönüşür[m
[32m+[m
[32m+[m[32m**Örnek:**[m
[32m+[m[32m```bash[m
[32m+[m[32m./build/kemgu --llvm test/ornekler/lambda_boyut.kem | clang -x ir - -o lb.exe[m
[32m+[m[32m./lb.exe; echo $?   # → 42 ✓[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Yeni testler:**[m
[32m+[m[32m- Lexer: 104/104 (+1 boyut)[m
[32m+[m[32m- Parser: 82/82 (+4: boyut/boyut_pointer/boyut_aritmetik + lambda_boyut.kem)[m
[32m+[m[32m- Tip kontrol: 98/98 (+8: 4 boyut + 4 pointer aritmetik)[m
[32m+[m[32m- **Toplam: 404/404 ✓ (önceden 392)**[m
[32m+[m
[32m+[m[32m### ADIM 14 EK: OS-ready LLVM backend — TAMAMLANDI ✓[m
[32m+[m
[32m+[m[32m**Alloca/load/store modeline geçiş + tam tip eşlemesi.** Tüm değişkenler artık `alloca`+`store`/`load` pattern'i kullanır (clang -O0 yaklaşımı). Bu sayede:[m
[32m+[m
[32m+[m[32m**Tip desteği (LLVM IR):**[m
[32m+[m[32m| KEMGU | LLVM |[m
[32m+[m[32m|-------|------|[m
[32m+[m[32m| tam8/dtam8 | i8 |[m
[32m+[m[32m| tam16/dtam16 | i16 |[m
[32m+[m[32m| tam32/dtam32 | i32 |[m
[32m+[m[32m| tam64/dtam64 | i64 |[m
[32m+[m[32m| kesirli32 | float |[m
[32m+[m[32m| kesirli64 | double |[m
[32m+[m[32m| mantıksal | i1 |[m
[32m+[m[32m| karakter | i32 (UTF-32) |[m
[32m+[m[32m| *T, &T | ptr |[m
[32m+[m[32m| Dizi\<T\> | { ptr, i64 } (slice) |[m
[32m+[m[32m| yapı X | %struct.X |[m
[32m+[m[32m| boş | void |[m
[32m+[m
[32m+[m[32m**Yeni dil özellikleri (codegen):**[m
[32m+[m[32m- **`değişken x: T = expr; x = yeni;`** — alloca + store, atama LLVM `store`[m
[32m+[m[32m- **`eğer / değilse`** — `br i1` + then/else/end basic block'ları[m
[32m+[m[32m- **`iken cond { ... }`** — header/body/end blokları[m
[32m+[m[32m- **`için x: koleksiyon { ... }`** — `Dizi<T>` üzerinde iterasyon (slice extract + indeks)[m
[32m+[m[32m- **`eşleş x { literal => ...; _ => ...; }`** — literal + joker + tanımlayıcı (binding) desen zinciri[m
[32m+[m[32m- **`yapı X { alan: T; }`** — `%struct.X = type {...}`, `getelementptr` ile alan erişim/atama[m
[32m+[m[32m- **`Dizi<T>` + `[1,2,3]` + `xs[i]`** — slice representation, indeks load/store[m
[32m+[m[32m- **Çoklu dosya derleme** — `kemgu f1.kem f2.kem` AST'leri birleştirir, harici çağrılar `declare` ile emit[m
[32m+[m[32m- **`-c` modu (object file)** — `kemgu -c f.kem -o f.o` clang ile .o üretir[m
[32m+[m[32m- **`--build` modu** — `kemgu --build *.kem -o app.exe` tam derleme + link[m
[32m+[m[32m- **Inline assembly + volatile MMIO** — sistem programlama intrinsics:[m
[32m+[m
[32m+[m[32m**OS programlama intrinsics (güvensiz blokta):**[m
[32m+[m[32m```kem[m
[32m+[m[32mgüvensiz {[m
[32m+[m[32m    _asm("hlt");                            // inline assembly[m
[32m+[m[32m    _yaz_volatile_dtam8(0xB8000, 65);       // MMIO write[m
[32m+[m[32m    değişken s = _oku_volatile_dtam32(0x1000);  // MMIO read[m
[32m+[m[32m}[m
[32m+[m[32m```[m
[32m+[m[32m9 intrinsic: `_asm`, `_yaz_volatile_dtam{8,16,32,64}`, `_oku_volatile_dtam{8,16,32,64}`. Tip kontrol tarafından da tanınır (predeclared global scope'ta).[m
[32m+[m
[32m+[m[32m**Yeni örnek:** `test/ornekler/faz1_kapsamli.kem` — 80 satır, tüm özellikleri kullanır (recursive fib, faktoriyel, struct, dizi+for, match, lambda IIFE, boyut<T>, inline asm), → exit 42[m
[32m+[m
[32m+[m[32m**Tüm dilin LLVM tarafından desteklenen pipeline'ı:**[m
[32m+[m[32m```bash[m
[32m+[m[32mkemgu --build prog.kem -o prog.exe        # tek dosya[m
[32m+[m[32mkemgu --build m1.kem m2.kem -o app.exe    # çoklu dosya[m
[32m+[m[32mkemgu -c lib.kem -o lib.o                 # object file (ayrı derleme)[m
[32m+[m[32mclang lib.o ana.o -o app.exe              # manuel linker[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Test sayisi: 406/406 ✓** (+1: faz1_kapsamli.kem parser dosya testi)[m
[32m+[m
[32m+[m[32m### ADIM 14 EK 2: OS-PROGRAMING TOOLCHAIN — TAMAMLANDI ✓ (ADIM 15)[m
[32m+[m
[32m+[m[32mKEMGU artık bare-metal kernel üretebilir. Yeni özellikler:[m
[32m+[m
[32m+[m[32m**Oznitelik sistemi (parser + LLVM):**[m
[32m+[m[32m```kem[m
[32m+[m[32m[bolum: ".text.boot"]      // linker section[m
[32m+[m[32m[ciplak]                   // naked function (prologue/epilogue yok)[m
[32m+[m[32m[kesme]                    // interrupt handler (x86_intrcc calling conv)[m
[32m+[m[32m[ciplak, bolum: ".multiboot"]   // birden cok oznitelik[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32mLLVM IR çıktı örnekleri:[m
[32m+[m[32m- `[ciplak]` → `define void @f() #0 { ... } attributes #0 = { naked }`[m
[32m+[m[32m- `[kesme]` → `define x86_intrcc void @f() { ... }`[m
[32m+[m[32m- `[bolum: ".text.boot"]` → `define void @f() section ".text.boot" { ... }`[m
[32m+[m
[32m+[m[32m**Atomic intrinsics (23 adet):**[m
[32m+[m[32m```kem[m
[32m+[m[32mgüvensiz {[m
[32m+[m[32m    değişken v: dtam32 = _atomik_oku_dtam32(adres);          // load atomic[m
[32m+[m[32m    _atomik_yaz_dtam32(adres, deger);                         // store atomic[m
[32m+[m[32m    değişken eski: dtam32 = _atomik_topla_dtam32(adres, 1);   // atomicrmw add[m
[32m+[m[32m    değişken takas: dtam32 = _atomik_takas_dtam32(adres, 99); // atomicrmw xchg[m
[32m+[m[32m    değişken basarili: mantıksal = _atomik_cas_dtam32(adres, eski, yeni); // cmpxchg[m
[32m+[m[32m    _bellek_engeli();     // fence seq_cst[m
[32m+[m[32m    _oku_engeli();        // fence acquire[m
[32m+[m[32m    _yaz_engeli();        // fence release[m
[32m+[m[32m}[m
[32m+[m[32m```[m
[32m+[m[32mTüm intrinsics 4 boyutta (dtam8/16/32/64): 4×5 = 20 atomic op + 3 fence = 23.[m
[32m+[m
[32m+[m[32m**CLI bayraklar:**[m
[32m+[m[32m```bash[m
[32m+[m[32mkemgu --freestanding              # -nostdlib -ffreestanding -fno-builtin[m
[32m+[m[32mkemgu --target=x86_64-unknown-none  # cross-compile, ELF cikti[m
[32m+[m[32mkemgu --linker-script=kernel.ld     # custom linker script (-Wl,-T,...)[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Sabit (constant) codegen:**[m
[32m+[m[32m```kem[m
[32m+[m[32msabit VGA_ADRES: dtam64 = 0xB8000;[m
[32m+[m[32m// → @VGA_ADRES = constant i64 753664[m
[32m+[m[32m// TANIMLAYICI ile load edilir[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Bidirectional çıkarsama: ikili op için yayım:**[m
[32m+[m[32m```kem[m
[32m+[m[32mdeğişken x: dtam64 = VGA_ADRES + 1;  // 1 dtam64'e çıkarsanır (eskiden tam32 idi)[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Bootloader/kernel scaffold:** `test/ornekler/kernel/`[m
[32m+[m[32m- `kernel.kem` — VGA write, halt loop, _start (naked)[m
[32m+[m[32m- `multiboot.kem` — Multiboot2 başlığı (inline asm)[m
[32m+[m[32m- `kernel.ld` — linker script (1MB layout)[m
[32m+[m[32m- `README.md` — derleme talimatı (GRUB ISO + QEMU)[m
[32m+[m
[32m+[m[32m**Sonuç:** `ld.lld -m elf_x86_64 -T kernel.ld kernel.o multiboot.o -o kernel.elf` → ELF 64-bit bare-metal executable, multiboot2 uyumlu, GRUB ile yüklenebilir, QEMU'da çalışır.[m
[32m+[m
[32m+[m[32m```[m
[32m+[m[32m$ llvm-objdump -h kernel.elf[m
[32m+[m[32m.multiboot    @ 0x100000  (Multiboot2 başlık)[m
[32m+[m[32m.text.boot    @ 0x100020  (_start entry, naked)[m
[32m+[m[32m.text         @ 0x100030  (kernel_main, vga_kemgu_yaz, sonsuz_halt)[m
[32m+[m[32m.rodata       @ 0x100200  (VGA_ADRES, sabit veriler)[m
[32m+[m[32m.bss          + stack_top (16KB stack)[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m### ADIM 16: KERNEL YAZARI HAZIRLIK — TAMAMLANDI ✓[m
[32m+[m
[32m+[m[32m**Sabit array codegen (`.rodata` raw bytes):**[m
[32m+[m[32m```kem[m
[32m+[m[32msabit BAYTLAR: Dizi<dtam8> = [65, 66, 67, 68];[m
[32m+[m[32m// @BAYTLAR.data = private constant [4 x i8] [i8 65, ...][m
[32m+[m[32m// @BAYTLAR = constant { ptr, i64 } { ptr @BAYTLAR.data, i64 4 }[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**String literal runtime (UTF-8 slice):** `metin` artik `{ ptr, i64 }` slice[m
[32m+[m[32m```kem[m
[32m+[m[32mdeğişken s: metin = "Merhaba";[m
[32m+[m[32m// @.str0 = [7 x i8], @.s0 = { ptr, i64 } { ptr @.str0, i64 7 }[m
[32m+[m[32m```[m
[32m+[m
[32m+[m[32m**Closure (free var capture):**[m
[32m+[m[32m```kem[m
[32m+[m[32mdeğişken y: tam32 = 10;[m
[32m+[m[32mdeğişken topla = |x: tam32| x + y;   // y captured[m
[32m+[m[32mver topla(32);  // -> 42[m
[32m+[m[32m```[m
[32m+[m[32m- Free var walker (recursive AST traversal)[m
[32m+[m[32m- Env struct alloca caller'da[m
[32m+[m[32m- Lambda `(ptr env, params...)` imzali — env'den captures load[m
[32m+[m[32m- Call site env_ptr'yi ilk arg olarak gecirir[m
[32m+[m
[32m+[m[32m**Bölge Katman 2 (concurrency aksiyomlari):**[m
[32m+[m[32m- `_gorev_baslat(handle) -> dtam64` — R-GÖREV (yeni ρ_sahip)[m
[32m+[m[32m- `_gorev_birlestir(handle)` — R-BİRLEŞTİR[m
[32m+[m[32m- `_kanal_olustur() -> dtam64` — R-KANAL (yeni ρ_kanal)[m
[32m+[m[32m- `_kanal_gonder(kanal, deger)` — deger ρ_kanal'a transfer[m
[32m+[m[32m- `_kanal_al(kanal) -> tam32`[m
[32m+[m[32m- bolge_atama.c bunlari taniyor + 5 yeni test[m
[32m+[m
[32m+[m[32m**Kernel yazimi ornek dosyalari (`test/ornekler/kernel/`):**[m
[32m+[m[32m- `gdt.kem` — Global Descriptor Table tanimi[m
[32m+[m[32m- `idt.kem` — Interrupt Descriptor Table + handler örnekleri[m
[32m+[m[32m- `paging.kem` — Sayfa tablolari (PML4/PDPT/PD/PT)[m
[32m+[m[32m- `serial.kem` — UART 16550 driver scaffold[m
[32m+[m[32m- `pci.kem` — PCI Configuration Space (Mechanism 1)[m
[32m+[m[32m- `concurrency.kem` — Task + kanal kullanimi (Katman 2)[m
[32m+[m
[32m+[m[32m**Test sayisi:** **411/411 ✓** (+5 Katman 2 bolge testleri)[m
[32m+[m
[32m+[m[32m### Hala kalan (gercek runtime + dil tamamlama):[m
[32m+[m
[32m+[m[32m- **Tam Katman 1 escape analizi** (DFA tabanlı — su an context-tracking)[m
[32m+[m[32m- **`hiç`/`değer` ifade desteği + pattern binding** (esles desen scope)[m
[32m+[m[32m- **Concurrency runtime** (scheduler, kanal implementation — sonradan)[m
[32m+[m[32m- **Bit shift / bit AND / bit OR** operatorleri (page table bit alanlari icin)[m
[32m+[m[32m- **Yapi initializer atama** (karisik literal kontrol)[m
[32m+[m[32m- **Generic monomorphization codegen** (su an tip sistemi farkinda ama LLVM yok)[m
 - **Bootstrapping** (uzun vade)[m
 [m
 ### İlerideki Fazlar[m
[36m@@ -457,9 +682,9 @@[m [mBelge dosyaları: Türkçe.[m
 [m
 ## Aktif Görev[m
 [m
[31m-- **Faz:** **🎉🎉 TİP + BÖLGE + LLVM FAZLARI TAMAMLANDI** (END-TO-END DERLEYİCİ!)[m
[31m-- **Tamamlanan:** Lexer → Parser → AST → Tip → Bölge (temel) → LLVM IR → native exe[m
[31m-- **Sıra:** ~~11.1-11.7~~ ✓ → ~~12.1-12.2~~ ✓ → ~~13.1~~ ✓ → **(genişletme: tam escape, Katman 2, LLVM cağrı/yapı/kontrol akışı)**[m
[32m+[m[32m- **Faz:** **🎉🎉🎉🎉🎉 KERNEL YAZARI HAZIR — TAM ÖZELLİK SETİ** (ADIM 16: sabit array + string slice + closure + Katman 2 + driver örnekleri)[m
[32m+[m[32m- **Tamamlanan:** Lexer → Parser → AST → Tip → Bölge (Katman 1 + Katman 2) → LLVM IR (alloca, tam tip, kontrol akışı, struct, slice, closure, sabit array, string literal, intrinsics, naked/section/x86_intrcc, atomic) → çoklu dosya / linker / object file → ELF bare-metal kernel[m
[32m+[m[32m- **Sıra:** ~~11.1-11.7~~ ✓ → ~~12.1-12.2~~ ✓ → ~~13.1~~ ✓ → ~~14: Lambda + boyut + ptr aritmetik~~ ✓ → ~~14 ek: alloca + kontrol akışı + struct/dizi + multi-file + asm/volatile~~ ✓ → ~~15: oznitelikler + atomic + freestanding + cross-compile + kernel scaffold~~ ✓ → ~~16: sabit array + string literal + closure + Katman 2 + driver örnekleri~~ ✓ → **(tam DFA escape analizi, bit operatörleri, hiç/değer ifade, concurrency runtime, generic monomorphization, bootstrap)**[m
 - **Tip sistemi tasarım kararları (kullanıcı onayladı):**[m
   - Çıkarsama: Lokal + Bidirectional (Rust/Swift tarzı)[m
   - Generic: Monomorphization (Rust gibi)[m

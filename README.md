# KEMGU

**Türkçe sözdizimli sistem programlama dili ve işletim sistemi projesi.**

KEMGU; saldırı yüzeylerini *yapısal olarak* daraltan, çöp toplayıcısız, çökmez bir
sistem dilidir. Hedefimiz; dilin kendisinden bütün bir işletim sistemine kadar
**aynı semantikle** çalışan ortak bir teknoloji yığınıdır — Türkçe terminolojiyle.

```kemgu
işlev ana() -> tam32 {
    değişken k: tekkez<tam32> = tekkez_yarat(42);
    ver kullan(k);     // lineer kaynak; tüketildi.
}
```

> Geliştirme aşamasında: derleyici çekirdeği çalışıyor; işletim sistemi yol haritada.
> Ayrıntı için [`belgeler/MIMARI.md`](belgeler/MIMARI.md).

---

## Üç Stratejik Hedef

KEMGU'nun var olma sebebi — her tasarım kararı bu üçüne göre değerlendirilir.
Direktif Ek v1.1 Bölüm 1.

### 1. Kırılamaz Güvenlik
Drone–yer haberleşmesi, askeri/tıbbi/finansal sistemler için saldırı sınıflarının
*derleme zamanında* imkansız kılınması:
- `tekkez<T>` (Linear Types V1) — kaynaklar yalnız bir kez tüketilir.
- `bölge` tabanlı bellek — kullanım sonrası serbestleme, sızıntı, ikinci serbestleme yok.
- `seçimlik<T>` / `sonuç<T,H>` — null ve istisna yok.
- Yan-kanal disiplini (`sabitsüre` qualifier, planlanmakta).
- Kapasiteye dayalı güvenlik modeli — Unix kullanıcı/grup ötesi (planlanmakta).

### 2. Maksimum Performans
AI, oyun ve sistem programcılığında *sıfır maliyetli soyutlamalar*:
- GC yok; bölge tahsisleri compile-time'da kararlaştırılır.
- LLVM IR backend; mevcut x86_64 + ARM64 (DGX Spark, Apple Silicon, Pi).
- Generic monomorphization — Rust tarzı; runtime dispatch tablosu yok.
- Roadmap: CUDA FFI birinci sınıf, `Tensor<T, Şekil>`, `gpu_bölge`, `gerçekzaman`
  alt kümesi (WCET teoremi), SIMD intrinsic'leri.

### 3. Evrensel İşletim Sistemi
ARM64 birincil, x86_64 ikincil. Drone'dan sunucuya tek yığın:
- Self-host hedefi: derleyici → çekirdek → sürücüler → kullanıcı alanı (KEMGU ile).
- Aşamalı bootstrap: C ABI FFI köprüsü.
- Mikroçekirdek + kapasiteye dayalı sürücü çatısı (planlanmakta).

---

## Mevcut Özellikler

| Bölüm                  | Durum  | Notlar |
|------------------------|--------|--------|
| Lexer                  | ✓      | 33 anahtar kelime, UTF-8 Türkçe doğal |
| Parser                 | ✓      | Recursive descent + Pratt, panik modu hata kurtarma |
| AST + Arena allocator  | ✓      | malloc/free yok, tek serbestleme |
| Tip sistemi            | ✓      | Bidirectional çıkarsama, generic monomorphization |
| Bölge sistemi          | ✓ K1+K2| R-* aksiyomları + concurrency iskeleti |
| Linear Types V1        | ✓      | `tekkez<T>`, `kullan`, `imha` + closure capture (LC-2/3) |
| Escape analizi         | ✓      | DFA + fixed-point iterasyon |
| Constraint v2          | ✓      | `özellik` / `uygula`, method dispatch, bound enforcement |
| LLVM backend           | ✓ v3   | Tam/kesirli, yapı by-value, dizi, çağrı, generic mangling |
| Bit operatörleri       | ✓      | `&`, `\|`, `^`, `~`, `<<`, `>>` — tam64 dahil |
| ARM64 cross-compile    | ✓      | `make calistir_arm64_test` — bare-metal ELF üretir |
| Bare-metal UART konsol | ✓ K8b  | ARM PL011 + x86_64 16550A, libc-yok yazdırma. Bkz. [BARE_METAL_DESTEK.md](BARE_METAL_DESTEK.md) |
| LSP server             | ✓ v2   | Hover + completion + tanıma git |
| Stdlib (saf KEMGU)     | ✓ 8 modül | matematik, dizi, opsiyonel, sonuç, metin (kısmen), dosya (skeleton) |
| Snapshot + fuzz testleri | ✓    | 10k iterasyon temiz |
| Self-host bootstrap    | ⏳      | Uzun vade |
| `görev` / `kanal` concurrency syntax | ⏳ | Katman 2 aksiyomları hazır, syntax bekleniyor |

---

## Hızlı Bakış

```kemgu
// Generic, lineer ve bölge bir arada — bağımlılık yok.
işlev anahtar_uret<T>() -> tekkez<T> { /* ... */ }

işlev kullan_bir_kere(k: tekkez<tam32>) -> tam32 {
    ver kullan(k);    // ikinci kullanım derlemede yakalanır (L002).
}

// Pattern matching + seçimlik (null değil).
işlev ad_ver(o: seçimlik<metin>) -> metin {
    eşleş o {
        değer(s) => { ver s; }
        hiç      => { ver "isimsiz"; }
    }
    ver "ulaşılmaz";
}
```

Daha fazla örnek: [`test/ornekler/`](test/ornekler/). Tutoriyal serisi:
`01_merhaba.kem` → `09_arm64.kem`.

---

## Kurulum (Windows + MSYS2)

KEMGU şu anda Windows üzerinde geliştiriliyor. Linux/macOS portu yol haritada
(Makefile zaten platform tespiti yapıyor).

### Gerekli paketler

```bash
# MSYS2 kurulumundan sonra (https://www.msys2.org/):

# UCRT64 — prod GCC derleyici + GNU Make
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make

# Clang64 — ASan/UBSan testleri için
pacman -S mingw-w64-clang-x86_64-clang

# Cross-compile (ARM64 doğrulama) ve LLVM araçları
pacman -S mingw-w64-clang-x86_64-llvm
```

### PATH

Her shell oturumunda iki MSYS2 dağıtımı PATH'te olmalı:

```bash
# Git Bash / MSYS shell
export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
```

> Neden iki tane? UCRT64 GCC ASan runtime'ı içermez. Clang64 testler için,
> UCRT64 prod ikilisi için kullanılır. Ayrıntı: [`CLAUDE.md`](CLAUDE.md) —
> "Win11 26200 — ASan / Dr. Memory Notu".

### Windows PowerShell / CMD

PowerShell veya CMD kullanıyorsanız PATH'i kalıcı yapmanız gerekmez — repo
köküne yerleşik build wrapper'ları MSYS2 yollarını oturum bazlı set eder:

```powershell
# PowerShell
.\build.ps1 test_tumu          # `mingw32-make test_tumu` ile aynı
.\build.ps1                    # build/kemgu.exe derler (default hedef)
.\build.ps1 clean
```

```cmd
REM CMD
build.bat test_tumu
build.bat
```

MSYS2 varsayılan dışı bir konumda (`C:\msys64` değilse) `MSYS2_ROOT` ortam
değişkeniyle override edin:

```powershell
$env:MSYS2_ROOT = 'D:\msys64'
.\build.ps1 test_tumu
```

> Wrapper PATH'i sadece kendi süreci için set eder — KEMGU dışındaki
> PowerShell oturumlarınızı kirletmez (`gcc` gibi yaygın isimler MSYS2'ye
> yönlenmez).

PowerShell `Set-ExecutionPolicy Restricted` ile çalışıyorsa (Windows
varsayılanı) `build.ps1` reddedilir. İki çözüm:

```powershell
# (a) Tek seferlik kalıcı izin (önerilir):
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# (b) Her çağrıda Bypass:
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 test_tumu
```

ExecutionPolicy değiştirmek istemiyorsanız `build.bat` her zaman çalışır
(CMD wrapper, policy gerektirmez).

### Linux / macOS (deneysel)

Makefile platform tespiti var; standart `gcc` + `clang` + `make` ile derlenir.
ASan testleri Linux'ta GCC ile de çalışır; macOS'ta Xcode Clang gerekli.

---

## İlk Derleyişin

```bash
# 1. Derleyiciyi derle.
mingw32-make                         # build/kemgu.exe oluşur

# 2. Bir kaynak dosya yaz.
echo 'işlev main() -> tam32 { ver 42; }' > merhaba.kem

# 3. Tip kontrolünden geçir (en hızlı geri bildirim).
./build/kemgu.exe --check merhaba.kem
# → OK: merhaba.kem — tip kontrolu basarili.

# 4. LLVM IR üret + native ikiliye dönüştür.
./build/kemgu.exe --llvm merhaba.kem > merhaba.ll
clang -x ir merhaba.ll -o merhaba.exe
./merhaba.exe; echo $?               # → 42
```

Çalıştırma modları:

| Bayrak     | Çıktı                              | Kullanım           |
|------------|------------------------------------|--------------------|
| (varsayılan) | AST yazdırma                     | Geliştirici debug  |
| `--token`  | Token dökümü                        | Lexer doğrulama    |
| `--parse`  | AST yazdırma                        | Parser doğrulama   |
| `--check`  | OK / HATA                           | CI / hızlı kontrol |
| `--llvm`   | LLVM IR (text)                      | Backend            |
| `--lsp`    | stdio LSP server (Content-Length)   | IDE entegrasyonu   |

---

## Test Paketini Çalıştırma

```bash
mingw32-make test_tumu
```

Bu komut bütün modülleri ve örnek programları (snapshot + fuzz dahil) çalıştırır.
Tek bir modülü çalıştırmak istersen:

```bash
mingw32-make calistir_lexer_test           # 103/103
mingw32-make calistir_parser_test          # 90/90
mingw32-make calistir_tip_kontrol_test     # 97/97
mingw32-make calistir_linear_test          # 54/54
mingw32-make calistir_llvm_test            # 30/30
mingw32-make calistir_stdlib_check         # 8/8 modül --check
mingw32-make calistir_arm64_test           # bare-metal ELF cross-compile
mingw32-make calistir_snapshot_test
mingw32-make calistir_fuzz_test            # 10000 iter
```

Bellek alan modüller AddressSanitizer + UBSan ile derlenir; herhangi bir test
sızıntıyla biterse `ERROR: AddressSanitizer: ...` çıkar ve adım onaylanmaz.

---

## Belgeler

| Belge                                                            | İçerik |
|------------------------------------------------------------------|--------|
| [`belgeler/KILAVUZ.md`](belgeler/KILAVUZ.md)                     | Dil rehberi — tipler, kontrol akışı, bölge, lineer, generic, modül |
| [`belgeler/MIMARI.md`](belgeler/MIMARI.md)                       | Derleyici iç yapısı — Lexer → LLVM IR |
| [`belgeler/BASLAMAK.md`](belgeler/BASLAMAK.md)                   | Yeni geliştirici onboarding'i |
| [`belgeler/KEMGU_Grammar_EBNF.md`](belgeler/KEMGU_Grammar_EBNF.md) | EBNF grameri (tam) |
| [`belgeler/KEMGU_Lexer_Spesifikasyonu.md`](belgeler/KEMGU_Lexer_Spesifikasyonu.md) | Token tabanları, UTF-8, Türkçe |
| [`belgeler/KEMGU_Bellek_Modeli.md`](belgeler/KEMGU_Bellek_Modeli.md) | Bölge sistemi formal — Katman 1 + 2 + 3 |
| [`belgeler/KEMGU_Linear_Types_Spec_V1.md`](belgeler/KEMGU_Linear_Types_Spec_V1.md) | `tekkez<T>` semantiği, kurallar, intrinsic'ler |
| [`belgeler/KEMGU_Direktif_Ek_v1.1.md`](belgeler/KEMGU_Direktif_Ek_v1.1.md) | Operasyonel kurallar, ASLA listesi |
| [`CONTRIBUTING.md`](CONTRIBUTING.md)                             | Katkı süreci (henüz açık değil) |

---

## Proje Yapısı

```
kemgu/
├── README.md                        ← bu dosya
├── CLAUDE.md                        # Claude Code proje bağlamı (geliştirici notları)
├── CONTRIBUTING.md                  # katkı süreci
├── KIRMIZI_QUEUE.md                 # Kırmızı (büyük) karar bekleyenler
├── Makefile                         # dual-compiler (UCRT64 GCC + Clang64)
├── belgeler/                        # tasarım belgeleri
├── src/                             # derleyici kaynak (C11)
├── runtime/                         # KDL runtime — kdl_runtime.c
├── stdlib/                          # saf KEMGU stdlib (8 modül)
├── editor/vscode-kemgu/             # VSCode syntax dosyaları
└── test/
    ├── test_*.c                     # birim testleri (modül başına)
    ├── ornekler/                    # .kem örnek programlar (tutorial dahil)
    └── stdlib/                      # stdlib --check test dosyaları
```

---

## Katkı

Şu an proje **kapalı geliştirme** aşamasında — direktif eki Bölüm 2 onayı ile
katkı kabulü açılacak. O zamana kadar:

- Hata raporu / öneri için issue açabilirsin.
- Branch disiplini: `feature/<konu>` — main'e doğrudan commit yasak.
- Türkçe kimlik: kod, yorum, hata mesajları, commit mesajları Türkçedir.
  Ayrıntı: [`CONTRIBUTING.md`](CONTRIBUTING.md).

---

## Lisans

Lisans henüz seçilmedi. **Tüm hakları saklıdır** (yayın aşamasında MIT ya da
Apache-2.0 değerlendirilecek). Direktif eki Bölüm 8.

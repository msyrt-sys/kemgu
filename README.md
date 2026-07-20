# KEMGU

**Türkçe sözdizimli, çöp toplayıcısız, bellek-güvenli bir sistem programlama dili.**

KEMGU iki şeyi bir arada hedefler:

1. **Gerçek bir sistem dili** — çöp toplayıcısı (GC) olmadan bellek güvenliği:
   lineer tipler (`tekkez<T>`), bölge tabanlı bellek (`bölge`) ve kabiliyet
   token'ları (`yetki<R>`) bütün saldırı sınıflarını *derleme zamanında* eler.
2. **Makine-denetimli güvenlik** — dilin eşzamanlılık çekirdeği için
   **Lean 4'te formel olarak ispatlanmış** veri-yarışı-yokluğu (DRF) ve bellek
   güvenliği teoremi. Kâğıt üstünde değil; `lake build` ile doğrulanabilir,
   `sorry`'siz bir ispat.

Bu kombinasyon — üretim hedefli bir sistem dili **artı** çekirdeği için
makine-denetimli eşzamanlılık-güvenliği ispatı — KEMGU'yu ayıran şeydir.

```kemgu
işlev main() -> tam32 {
    değişken k: tekkez<tam32> = tekkez_olustur(42);
    ver kullan(k);          // lineer kaynak; tam bir kez tüketilir.
    // ver kullan(k);       // ✗ L002 — tüketim sonrası kullanım (derleme hatası)
}
```

> Geliştirme aşamasında: derleyici çekirdeği uçtan uca çalışıyor (KEMGU kaynağı →
> native ikili), eşzamanlılık-güvenliği çekirdeği formel doğrulanmış — ve
> eşzamanlılık artık yalnız ispatlı değil, **çalışır**: `görev`/`kanal` gerçek
> thread'lere ve bloklayan kanallara indirgenir. Self-host bootstrap **fixpoint'i
> tuttu** (derleyici kendini bayt-birebir üretiyor); tam parite ve işletim sistemi
> yol haritasındadır (aşağıya bakın).

---

## Üç Stratejik Hedef

KEMGU'nun var olma sebebi — her tasarım kararı bu üçüne göre değerlendirilir.
Direktif Ek v1.1 Bölüm 1.

### 1. Kırılamaz Güvenlik
Drone–yer haberleşmesi, askeri/tıbbi/finansal sistemler için saldırı sınıflarının
*derleme zamanında* imkansız kılınması:
- `tekkez<T>` (Linear Types V1) — kaynaklar yalnız bir kez tüketilir
  (use-after-free, double-free yapısal olarak imkansız).
- `bölge` tabanlı bellek — GC yok; ömür compile-time'da kararlaştırılır.
- `seçimlik<T>` / `sonuç<T,H>` — null ve istisna **yok**.
- `yetki<R>` kabiliyet token'ları — kaynak erişimi (Bellek, MMIO, Dosya, …)
  lineer kabiliyetle kapılır; Unix kullanıcı/grup modelinin ötesi.
- Veri-yarışı-yokluğu (DRF) — eşzamanlılık çekirdeği için **formel ispatlı**.

### 2. Maksimum Performans
*Sıfır maliyetli soyutlamalar*:
- GC yok; bölge tahsisleri statik analiz ile çözülür.
- LLVM IR backend; native ikili (x86_64; ARM64 bare-metal cross-compile).
- Generic **monomorphization** — Rust tarzı; runtime dispatch tablosu yok.
- `sabitsüre<T>` sabit-zaman tipleri, `vektör` SIMD, `gerçekzamanlı` WCET
  alt kümesi (statik denetim).

### 3. Evrensel İşletim Sistemi (vizyon)
ARM64 ve x86_64 bare-metal'e kadar tek yığın — *uzun vadeli hedef*:
- Self-host: derleyici → çekirdek → sürücüler → kullanıcı alanı (KEMGU ile).
- Mevcut durum: libc-yok UART konsol sürücüleri + VirtIO protokol modelleri +
  bare-metal kernel ELF bring-up'ı (aşağıda "Sürücüler" bölümü). Tam OS yok.

---

## Mevcut Özellikler

Aşağıdaki her madde repo'daki kodla doğrulanmıştır. **Derinlik** sütunu
ne kadar olgun olduğunu gösterir:
*parse* (sözdizimi) · *tip* (tip kontrolü) · *codegen* (native ikiliye derlenir,
çalıştırılır) · *runtime* (test paketinde çalışır/ölçülür) · *ispat* (Lean'de
makine-denetimli).

### Dil

| Özellik | Derinlik | Notlar |
|---------|----------|--------|
| Lineer tipler `tekkez<T>` (`kullan`/`imha`) | codegen | L001 tüketilmedi, L002 move sonrası kullanım, L004 referans yasağı. Üretici `tekkez_olustur<T>`. Closure yakalarsa lambda da lineer olur. |
| Bölge tabanlı bellek `bölge` (**GC yok**) | tip | Bölge temsili + R-* atama aksiyomları + fixed-point escape analizi modülleri. Çalışma-zamanı tahsisinde GC/refcount yok. Otomatik-serbestleyen gerçek arena V2. |
| Kabiliyet token'ları `yetki<R>` | codegen | `yetki<Bellek>`, `yetki<MMIO>`, `yetki<Dosya>` … Lineer (sızıntı/çift-kullanım = CP005). `delege` (alt-token) + `geri_al` (iptal). |
| `seçimlik<T>` (null yok) · `sonuç<T,H>` (istisna yok) | codegen | `eşleş` ile `değer(s)`/`hiç`, `tamam(v)`/`hata(e)` deseni — değer bağlama codegen'de çalışır. |
| Etiketli birleşim / toplam tip `çeşit` | codegen | İsimli varyant kümesi + ayrık tag. `eşleş` exhaustiveness denetimi (M001). **v1 sınırı:** varyantlar payloadsuz (yalnız etiket; veri taşıyan varyant yok). |
| Jenerikler — **monomorphization** | codegen | İşlevler **ve** struct'lar; her örnekleme ayrı mangled fonksiyon (`@kimlik$i32`), runtime dispatch yok. **v1 sınırı:** tip argümanları yalnız *çıkarsanır* (açık `<T>` / turbofish henüz yok). |
| Satıriçi assembly `satıriçi_asm` | codegen | Yalnız `güvensiz` blokta. `mimari:` + `şablon:` ile gerçek LLVM inline-asm'e indirgenir (çoklu çıktı dahil). |
| Çok-dosya modüller (`kullan`, `genel`) | codegen | Aşağıda ayrı bölüm. |
| Çapraz-modül generic monomorphization | codegen | Generic fonk **ve** struct'lar dosyalar arası örneklenir (`kap::Liste<T>`). |
| Nitelikli tip annotasyonu (`mod::Tip<args>`) | codegen | Tip pozisyonunda modül-nitelikli tip: `değişken l: dizi::Liste<tam64> = dizi::oluştur(böl)`. Çapraz-modül struct alan erişimi dahil. **v1 sınırı:** nitelikli *inşa ifadesi* `mod::Yapı{...}` henüz yok (factory + çıkarsama ile). |
| Bit operatörleri (`&` `\|` `<<` `>>`) | codegen | Tamsayı genişlikleri arası otomatik dönüşüm. |
| Eşzamanlılık `görev<T>` / `kanal<T>` | codegen | `görev_başlat(\|\| …)` **gerçek işletim-sistemi thread'i** başlatır (Win32/pthread); `görev_birleştir` sonucu çağırana taşır. `kanal_oluştur/gönder/al` — **bloklayan** kanal, çift yönlü akış denetimi (dolu kanalda gönderim, boş kanalda alım bekler). Her görev **kendi bölgesini** sahiplenir (S1/S2) → paylaşılan-bölge yarışı yapısal olarak imkânsız. Statik denetim DRF001-006; `görev<T>` lineerdir (birleştirilmezse L001, iki kez birleştirilirse L002). **v1 sınırı:** `T` ∈ {≤64-bit tamsayı, işaretçi-benzeri (`metin`/`Dizi<T>`), `boş`} — kesirli `T` reddedilir (runtime tamsayı yazmacından okur). Görev bölgesi henüz serbest bırakılmaz (bilinçli sızıntı; hapsedilme kanıtı bekliyor). |

### Derleyici

- **C11 bootstrap derleyici** — UCRT64 GCC (prod) + Clang64 (ASan/UBSan testleri),
  `-Wall -Wextra -Wpedantic -std=c11`, sıfır uyarı.
- **LLVM-IR (text) backend** — `llvm.c` doğrudan `.ll` metni üretir (libLLVM
  bağımlılığı yok); `clang -x ir` ile native ikiliye derlenir. Üretilen IR
  varsayılan olarak bir doğrulama kapısından geçer (`--no-verify` ile kapatılır).
- **Pipeline:** lexer → parser → AST → çok-dosya modül yükleyici (whole-program)
  → tip + lineer kontrol → WCET → LLVM IR → native `.exe`.
- **Backend kapsamı:** çok-genişlikli tamsayı, `kesirli32/64` (float/double),
  struct-by-value, dizi, çağrı + özyineleme, jenerik monomorphization, metin
  literalleri, kontrol akışı, kabiliyet ve SIMD.

### Formel Doğrulama — *öne çıkan*

KEMGU'nun eşzamanlılık çekirdeği **Lean 4'te makine-denetimli** olarak
ispatlanmıştır (`proofs/drf-v2-lean/`, Lean 4 v4.29.0 + mathlib).

- **Ana teorem** `kemgu_soundness_v3`
  (`Kemgu/Soundness/Main.lean`):
  > İyi-tipli bir program Π için, başlangıç konfigürasyonundan **ulaşılabilir
  > her durum**: veri-yarışı-yok (DRF) ∧ bellek-güvenli ∧ fault-suz.
- **`sorry`'siz, aksiyom-bildirimsiz.** Tüm `.lean` kaynağında `sorry`,
  `axiom`, `admit`, `native_decide` yok — yalnız Lean/mathlib'in standart
  klasik temelleri kullanılır. Destekleyici lemmalar: DRF-L0..L7 (bölge-thread
  tekilliği, lineer cross-thread move, kanal atomik transfer, kabiliyet-lineer,
  bellek-erişim tip soundness) + MemSafety T1.
- **Anlamlılık tanığı (vakum değil):** somut bir **eşzamanlı** program — iki
  görev + bir kanal (`gönder`/`al`) — `IyiTipliCekirdek` ispatlanır ve teorem
  bunun üzerine uygulanır (`Kemgu/Soundness/EszamanliTanik.lean`). Teorem
  triviyal-sıralı parçalar hakkında değildir.
- **Dürüst kapsam:** Bu bir **V1 çekirdek alt-kümesi** teoremidir, *tam-dil DRF
  değildir* (`Kopru.lean` bunu açıkça not eder; çekirdek: görevler + kapasite-1
  kanallar + lineer move + bölgeler). Yan-kanal ve WCET (BET) bileşenleri
  teoremden **bilinçli olarak çıkarılmıştır** (V2 hedefi; şu an iskelet).
  Derleyici, ispatlanan çekirdeğin bir **üst-kümesini** kabul eder.

```bash
mingw32-make calistir_drf_lean_proof     # cd proofs/drf-v2-lean && lake build
```

### Standart Kütüphane (saf KEMGU)

Tümü `--check`'ten geçer; runtime/FFI bağımlılığı yok.

- `kütüphane/dizi.kem` — generic büyüyen liste **`Liste<T>`** (`oluştur`/`ekle`/
  `al`/`boy`/`büyü`; kabiliyet tabanlı `*T` tampon). Uçtan uca çalışır (KDL
  runtime'a bağlanır).
- `stdlib/temel/` — `matematik`, `karsilastir`, `sayisal` (saf jenerik yardımcılar).
- `stdlib/dizi.kem` — yerleşik `Dizi<T>` algoritmaları (`harita`/`filtre`/
  `indirgeme`/`sirala`/`bul`/…).
- `stdlib/opsiyonel.kem`, `stdlib/sonuc.kem` — fonksiyonel `seçimlik`/`sonuç` API.
- `stdlib/kripto/` — SHA-256 (tek blok) + ChaCha20 quarter-round + sabit-zaman
  primitifler (`sabitsüre<T>` ile). *Kısmi:* BLAKE3/HMAC ve kripto-güvenli RNG
  V2 placeholder'ı.
- `stdlib/metin.kem`, `stdlib/dosya.kem` — *kısmi* sarmalayıcılar (bazı
  işlemler stub; gerçek handle ABI ve codepoint-duyarlı bölme V2).

### Sürücüler / Bare-Metal

Bunlar **sürücü prototipleri ve konsol bring-up'ıdır** — tam bir işletim sistemi
*değildir* (zamanlayıcı, MMU/sayfalama, syscall yok).

- **VirtIO** (`drivers/virtio/`, 10 modül) — blok aygıtı + MMIO transport,
  virtqueue, durum makinesi, özellik anlaşması. Saf-KEMGU **protokol modelleri**
  (tip-kontrol seviyesi); `çeşit` hata-etiketleri, `yetki<MMIO>`, `delege`/
  `geri_al`, `eşleş`, `sonuç` kullanır.
- **UART konsol** (`runtime/`) — ARM **PL011** + x86_64 **16550A**, libc-yok.
  Host mock modunda çalışma-zamanı test edilir; bare-metal varyant cross-compile
  ile doğrulanır (libc sembol referansı yok). Hedef-bağımsız vtable + panik
  handler + RX/TX echo.
- **Bare-metal ELF** — `.kem` kaynağı → `kemgu --llvm` → `clang -target
  aarch64-unknown-none` → `ld.lld` → freestanding **kernel ELF** (UART'a yazan,
  libc'siz). x86_64 yolu da var; opsiyonel QEMU smoke hedefi.
  Bkz. [`BARE_METAL_DESTEK.md`](BARE_METAL_DESTEK.md).

### Diğer Doğrulanmış Altsistemler

| Altsistem | Derinlik | Not |
|-----------|----------|-----|
| `sabitsüre<T>` sabit-zaman | tip | Yan-kanal disiplini tip seviyesinde. |
| `gerçekzamanlı` / WCET | tip | RT001-RT005 realtime kuralları. |
| `vektör` SIMD | tip + codegen | Tip kontrolü + LLVM end-to-end. |
| MMIO temeli | tip + runtime | `yetki<MMIO>` zorunluluğu (MM001-003); C testi 23/23. |
| LSP sunucusu | runtime | stdio JSON-RPC; hover, completion, tanıma-git. |
| Self-host bootstrap | **fixpoint** | `selfhost/*.kem` — KEMGU ile yazılmış lexer + parser + checker + codegen. Doğrulama: dört bileşen de C derleyicinin çıktısıyla **bayt-birebir** (92/92 dosya), ve codegen kendini derleyince **stage1 == stage2** (33.371 satır IR, birebir). Tek binary sürücü 4 modda (`--token/--parse/--check/--llvm`) C ile eşleşir; semantik eşdeğerlik 76/76 korpus. `mingw32-make calistir_codegen_bootstrap`. **Sınır:** `kanal_*`/`dondur`/`görev_birleştir` port edildi (korpusta sınanıyor); `görev_başlat` + lambda dönüş çıkarsaması closure altyapısı beklediği için henüz yok. |

### Test & Kalite

- **1200+ birim/entegrasyon testi**, 32 paket, **0 başarısız** — canlı çalıştırıldı.
  Bellek alan paketler Clang64 **ASan + UBSan** ile derlenir, temiz.
  (Öne çıkanlar: `llvm` 263 uçtan-uca derle+çalıştır, `tip_kontrol` 174,
  `parser` 107, `lexer` 103, `linear` 57, `drf` 50, `snapshot` 50,
  `capability` 40, `görev_rt` 13 eşzamanlılık-runtime.)
- **30.000 fuzz iterasyonu** (10.000 + 4×5.000), **0 çökme**, ASan/UBSan altında.
- **Lean ispatı** `lake build` temiz, 0 `sorry`.

---

## Hızlı Bakış

```kemgu
// ----- Lineer tipler: KEMGU'nun imza güvenlik özelliği -----
işlev tek_kez_kullan() -> tam32 {
    değişken k: tekkez<tam32> = tekkez_olustur(42);
    ver kullan(k);                 // k tüketildi → 42
}

işlev tuket(t: tekkez<tam32>) { imha(t); }

işlev main() -> tam32 {
    değişken k = tekkez_olustur(7);
    tuket(k);                      // sahiplik tuket'e geçti
    // imha(k);                    // ✗ L002 — move sonrası kullanım (derleme hatası)
    ver tek_kez_kullan();          // → 42
}
```

```kemgu
// ----- Null yok: seçimlik<T> + eşleş desen bağlama -----
işlev ad_ver(o: seçimlik<metin>) -> metin {
    eşleş o {
        değer(s) => { ver s; }
        hiç      => { ver "isimsiz"; }
    }
    ver "ulaşılmaz";
}
```

```kemgu
// ----- Eşzamanlılık: görev + kanal (gerçek thread'ler) -----
işlev uretici(k: kanal<tam32>) -> tam32 {
    değişken i: tam32 = 1;
    iken i <= 5 {
        kanal_gönder(k, i);        // kanal doluysa BLOKLAR (akış denetimi)
        i = i + 1;
    }
    ver 0;
}

işlev main() -> tam32 {
    değişken k: kanal<tam32> = kanal_oluştur(2);   // kapasite 2

    // Ayrı bir thread'te çalışır; KENDİ bölgesini sahiplenir (S1/S2) —
    // çağıranla bölge paylaşmadığı için veri yarışı yapısal olarak imkânsız.
    değişken g: görev<tam32> = görev_başlat(|| uretici(k));

    değişken toplam: tam32 = 0;
    değişken n: tam32 = 0;
    iken n < 5 {
        toplam = toplam + kanal_al(k);   // kanal boşsa BLOKLAR
        n = n + 1;
    }

    görev_birleştir(g);    // g lineer: birleştirilmezse L001 (derleme hatası)
    ver toplam;            // → 15
}
```

Daha fazla örnek: [`test/ornekler/`](test/ornekler/) — tutoriyal serisi
`01_merhaba.kem` → `09_arm64.kem`; eşzamanlılık için `gorev_temel.kem` ve
`kanal_mesaj.kem`.

---

## Çok-Dosya Modüller

Her `.kem` dosyası, **adı dosya adından gelen** bir modüldür. `kullan ad;`
ifadesi `ad.kem`'i arama yolundan (önce dosyanın dizini, sonra proje kökü, sonra
`kütüphane/`) bulup tüm programı tek AST'de birleştirir (whole-program). Özel
bir bayrak gerekmez — giriş dosyasını verirsiniz, `kullan` grafiği otomatik
çözülür. `genel` ile dışa açarsınız (varsayılan: özel).

```kemgu
// ----- kap.kem  (modül adı = "kap") -----
yapı Liste<T> { veri: *T; boy: tam64; kapasite: tam64; }

genel işlev oluştur<T>(taban: T) -> Liste<T> { /* ... */ }
genel işlev ekle<T>(l: &Liste<T>, v: T)     { /* ... */ }
genel işlev al<T>(l: &Liste<T>, i: tam64, varsayilan: T) -> T { /* ... */ }
```

```kemgu
// ----- ana_kap.kem  -----
kullan kap;

işlev main() -> tam32 {
    değişken sifir: tam64 = 0;
    değişken l = kap::oluştur(sifir);        // Liste<tam64> — saf çıkarsama, çapraz-modül
    kap::ekle(&l, 10 olarak tam64);
    kap::ekle(&l, 32 olarak tam64);
    ver (kap::al(&l, 0, sifir) + kap::al(&l, 1, sifir)) olarak tam32;   // → 42
}
```

Generic `Liste<T>` struct'ı **kap** modülünden çapraz-dosya monomorphize edilir
(`@kap.oluştur$i64` …). Diğer biçimler:

```kemgu
kullan mat;                 // nitelikli erişim:  mat::topla(...)
kullan mat::{topla};        // seçili import:     topla(...)
kullan mat olarak m;        // alias:             m::topla(...)
```

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

# 3. Tip kontrolünden geçir (en hızlı geri bildirim — varsayılan mod).
./build/kemgu.exe --check merhaba.kem
# → OK: merhaba.kem — tip kontrolu basarili.

# 4. LLVM IR üret + native ikiliye dönüştür.
./build/kemgu.exe --llvm merhaba.kem | clang -x ir - -o merhaba.exe
./merhaba.exe; echo $?               # → 42
```

Çalıştırma modları:

| Bayrak       | Çıktı                              | Kullanım           |
|--------------|------------------------------------|--------------------|
| `--check`    | OK / HATA (**varsayılan**)         | CI / hızlı kontrol |
| `--token`    | Token dökümü                        | Lexer doğrulama    |
| `--parse`    | AST yazdırma                        | Parser doğrulama   |
| `--llvm`     | LLVM IR (text)                      | Backend            |
| `--lsp`      | stdio LSP server (Content-Length)   | IDE entegrasyonu   |
| `--no-verify`| IR doğrulama kapısını kapat          | `--llvm` ile       |

> Bölge tahsisi yapan programlar (ör. `yetki<Bellek>` + `bölge_al`) KDL
> runtime'ına ihtiyaç duyar:
> `mingw32-make build/kdl_runtime.o` sonra
> `clang -c -x ir prog.ll -o prog.o && clang prog.o build/kdl_runtime.o -o prog.exe`.

---

## Test Paketini Çalıştırma

```bash
mingw32-make test_tumu
```

Bu komut bütün modülleri ve örnek programları (snapshot + fuzz dahil) çalıştırır.
Tek bir modülü çalıştırmak istersen (gözlenen sayılar):

```bash
mingw32-make calistir_lexer_test           # 103/103
mingw32-make calistir_parser_test          # 107/107
mingw32-make calistir_tip_kontrol_test     # 174/174
mingw32-make calistir_linear_test          # 57/57
mingw32-make calistir_capability_test      # 40/40
mingw32-make calistir_llvm_test            # 205/205  (derle + çalıştır + exit kodu)
mingw32-make calistir_stdlib_check         # saf-KEMGU stdlib --check
mingw32-make calistir_arm64_test           # bare-metal ELF cross-compile
mingw32-make calistir_fuzz_test            # 10.000 iter
mingw32-make calistir_fuzz_advanced        # 4 mod × 5.000 iter
mingw32-make calistir_drf_lean_proof       # Lean 4 ispatı (lake build)
```

Bellek alan modüller AddressSanitizer + UBSan ile derlenir; herhangi bir test
sızıntıyla biterse `ERROR: AddressSanitizer: ...` çıkar ve adım onaylanmaz.

---

## Yol Haritası

Dürüst zaman-ufukları. **Bu bölümdeki hiçbir madde "mevcut" değildir** — mevcut
olanlar yukarıdaki "Mevcut Özellikler" tablolarındadır. Her madde *ne olduğu*,
*neden önemli olduğu* ve *neyin beklediği* ile birlikte yazılmıştır.

### Yakın — tanımlı, engelsiz iş

- **Self-host'ta closure / lambda codegen'i.** Paritenin kalan — ve en büyük —
  parçası. Kanal tarafı **port edildi** (`kanal_oluştur/gönder/al`, `dondur`,
  `görev_birleştir`) ve parite korpusunda sınanıyor; ama **`görev_başlat` port
  edilemedi**: fat-value closure (`{ptr, ptr}`) ister, self-host ise lifted lambda
  emit etmiyor — LAMBDA düğümünü ayrıştırıp yalnızca bölge-yönlendirmesinde kullanıyor.
  *Ek zorluk:* self-host doğrudan stdout'a yazar; C'nin ertelenmiş-kuyruk + geçici-dosya
  + yeniden-numaralandırma makinesi olduğu gibi taşınamaz — **ön-geçişli** lifted-lambda
  emisyonu gerekir.
  *Buna bağlı olanlar:* `görev_başlat` ve lambda dönüş-tipi çıkarsaması.
  *Nerede:* `selfhost/codegen.kem`, `mingw32-make calistir_codegen_bootstrap`.

- **Blok-form lambda dönüş-tipi çıkarsaması.** İfade-form (`|| 3.5`) çıkarsanıyor;
  blok-form (`|| { …; ver x; }`) dönüşü hâlâ sabit `tam32`.
  *Engel:* son-`ver`'i öğrenmek için gövdeyi emit etmek gerekir, ama emit sırasında
  dönüş tipi zaten lazım — **döngüsel bağımlılık**. Gövde ön-taraması gerekiyor.

- **Skaler referans okuma.** `&tam32` bugün **hiç okunamıyor** (`ver v` → T020,
  `v + 0` → T003, `*v` → T001); yalnız taşınıp döndürülebiliyor. Yapı referansı
  (`r.alan`) çalışıyor. Hiçbir örnek/test skaler referans kullanmadığı için
  gözden kaçmıştı.

- **Açık tip argümanı** (turbofish) — `f<T>(...)` / `Tip<T>{...}`. Şu an tip
  argümanları yalnız *çıkarsanıyor*; çıkarsamanın yetmediği yerde yazacak sözdizim yok.

- **Gerçek kabiliyet ödünç alma** — mevcut MOVE/`delege` çözümünün yerine.

- **Stdlib genişletme** — `Metin`/`Dosya` tamamlama, kripto (BLAKE3/HMAC), OS RNG.

### Orta — daha büyük ya da ön-koşullu

- **Görev bölgesinin serbest bırakılması.** Şu an her görev kendi bölgesini alıyor
  ama bölge **hiç serbest bırakılmıyor** (bilinçli sızıntı).
  *Neden bekliyor:* serbest bırakmak, görev gövdesindeki tahsislerin o bölgeye
  **hapsedildiğinin pozitif kanıtını** ister — gövde yakalanan bir `&değişken`e
  bölgeden işaretçi yazarsa serbest bırakma **use-after-free** olur. Kanıtsız
  serbest bırakma yapılmayacak; aynı disiplin `ρ_yerel` için de uygulanmıştı.

- **Semaforlar / bariyerler** — `görev`/`kanal` üstüne daha zengin senkronizasyon.

- **`kanal`'ın bare-metal (`.kem`) tarafında sınanması.** ABI hazır (host ile aynı
  imza), ama çekirdek tarafında testi yok.

- **Ayrık / artımlı derleme** — arayüz dosyaları, glob import, opak tipler, re-export.

- **Trait / bound sistemi** (`özellik`/`uygula`) — tam method dispatch.

- **Prosedürler-arası escape analizi** — çağrılan işlevin escape özeti (şu an çağrı
  sonuçları konservatif kabul ediliyor); bölge/escape analizini sürücü hattına
  bağlama + otomatik-serbestleyen arena.

- **Tek-kaynak konsolidasyon** — checker mantığı iki yerde (self-host driver +
  Aşama 2 referans `checker.kem`); ileride driver tek kaynak olabilir.

- **LSP v3** — artımlı senkronizasyon, workspace, semanticTokens, references.

### Uzun vade — açıkça hedef, "yakında" değil

- **Saf-KEMGU işletim sistemi** + sürücüler (zamanlayıcı, MMU, syscall). Bugün
  elde olan: libc'siz UART konsol, VirtIO protokol modelleri, bare-metal kernel
  ELF bring-up'ı. **Tam OS yok.**

- **DRF ispatı V2** — tam-dil kapsamı, per-thread bölgeler, operasyonel/runtime
  tanık, weak-memory (C++11) fence emisyonu, yan-kanal + WCET bileşenlerinin
  teoreme dahil edilmesi. Bugünkü teorem **V1 çekirdek alt-kümesi** içindir.

- **`çeşit` varyantlarına veri** — bugün varyantlar payloadsuz (yalnız etiket).

### Karar bekleyen — tasarım soruları (iş değil, karar)

Bunlar teknik olarak yapılabilir; bekleyen şey **dilin ne olacağına dair karar**.

- **`görev_başlat` başarısızlığı ne dönmeli?** Thread yaratılamazsa şu an
  `kdl_panik` çağrılıyor. Bu, KEMGU'nun "çökmezlik" ilkesiyle gerilim hâlinde —
  ama önceki davranış (görevi sıralı çalıştırmak) bloklayan bir kanal işleminde
  **kalıcı kilitlenme** üretiyordu, yani daha kötüydü. Muhtemel doğru çözüm
  `sonuç<görev<T>, Hata>`; bu bir dil kararı.

- **Kanalda yön garantisi.** Bugün tek, yönsüz `kanal<T>` var: aynı görev hem
  gönderip hem alabilir, tip sistemi engellemez. Bellek modelinin özgün tasarımı
  ayrık uçlardı (`gönderen<T>` / `alan<T>`) — geri dönülür mü, kararı bekliyor.

- **Custom ADT / enum sözdizimi** ve `eşleş` exhaustiveness'ın buna genişletilmesi.

### Bilinen sınırlar (bugün geçerli)

Yol haritası değil, **şu anki gerçek**: `görev<T>`/`kanal<T>` kesirli `T` kabul
etmez (runtime tamsayı yazmacından okur — sessiz bozulma yerine derleme hatası);
görev bölgesi sızdırılır; blok-form lambda dönüşü `tam32`; skaler `&T` okunamaz;
`çeşit` varyantları payloadsuz; turbofish yok; self-host derleyici en yeni codegen
özelliklerini içermez.

---

## Belgeler

| Belge | İçerik |
|-------|--------|
| [`belgeler/KILAVUZ.md`](belgeler/KILAVUZ.md) | Dil rehberi — tipler, kontrol akışı, bölge, lineer, generic, modül |
| [`belgeler/MIMARI.md`](belgeler/MIMARI.md) | Derleyici iç yapısı — Lexer → LLVM IR |
| [`belgeler/BASLAMAK.md`](belgeler/BASLAMAK.md) | Yeni geliştirici onboarding'i |
| [`belgeler/KEMGU_Linear_Types_Spec_V1.md`](belgeler/KEMGU_Linear_Types_Spec_V1.md) | `tekkez<T>` semantiği, kurallar, intrinsic'ler |
| [`belgeler/KEMGU_Capability_Spec_V1.md`](belgeler/KEMGU_Capability_Spec_V1.md) | `yetki<R>` object-capability modeli |
| [`belgeler/KEMGU_Bellek_Modeli.md`](belgeler/KEMGU_Bellek_Modeli.md) | Bölge sistemi formal — Katman 1 + 2 + 3 |
| [`belgeler/KEMGU_DRF_Mekanize_Spec.md`](belgeler/KEMGU_DRF_Mekanize_Spec.md) | DRF mekanize ispat spesifikasyonu |
| [`proofs/drf-v2-lean/README.md`](proofs/drf-v2-lean/README.md) | Lean 4 ispat projesi — kurulum + faz durumu |
| [`BARE_METAL_DESTEK.md`](BARE_METAL_DESTEK.md) | Bare-metal UART konsol + kernel ELF |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Katkı süreci (henüz açık değil) |

---

## Proje Yapısı

```
kemgu/
├── README.md                        ← bu dosya
├── CLAUDE.md                        # Claude Code proje bağlamı (geliştirici notları)
├── Makefile                         # dual-compiler (UCRT64 GCC + Clang64)
├── belgeler/                        # tasarım belgeleri + formel spec'ler
├── src/                             # derleyici kaynak (C11) — lexer→parser→tip→LLVM IR
├── proofs/drf-v2-lean/              # Lean 4 makine-denetimli DRF ispatı
├── runtime/                         # KDL runtime — string/dosya/dizi/yetki + UART sürücüleri
├── kütüphane/                       # saf-KEMGU stdlib: Liste<T>
├── stdlib/                          # saf-KEMGU stdlib: temel, dizi, opsiyonel, sonuc, metin, dosya, kripto
├── drivers/virtio/                  # VirtIO protokol modelleri (block + MMIO)
├── boot/ + linker/                  # bare-metal ARM64 başlangıç + linker script
├── editor/vscode-kemgu/             # VSCode syntax dosyaları
└── test/
    ├── test_*.c                     # birim testleri (modül başına)
    ├── ornekler/                    # .kem örnek programlar (tutorial dahil)
    ├── moduller/ + crossfile/       # çok-dosya modül + çapraz-modül testleri
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

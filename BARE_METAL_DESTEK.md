# KEMGU Stdlib Bare-Metal Uyumluluk Haritası

**Tarih:** 2026-05-15
**Faz:** Bare-Metal Hedef Genişletme — Kalem 4
**Yöntem:** Modül başına grep + manuel inceleme; tahmin değil, kanıtlı.

---

## Kategoriler

- **SAF (BARE-METAL-SAFE):** libc/runtime çağrısız; freestanding-uyumlu, saf
  KEMGU (sadece dil yapıları, basit aritmetik, struct/enum, generic).
  Bare-metal hedefte hiçbir port gerektirmez.

- **KISMI (BARE-METAL-MIXED):** Bazı fonksiyonlar libc bağımlı, diğerleri saf.
  Liste detayda; saf alt-küme bare-metal'de doğrudan kullanılır, bağımlı
  alt-küme port gerekir.

- **BAĞIMLI (BARE-METAL-INCOMPATIBLE):** Modülün **tüm** fonksiyonları runtime
  primitiflerine bağımlı. Bare-metal'de port edilen runtime yoksa hiçbir
  fonksiyon kullanılamaz.

---

## Modül Haritası

### `stdlib/temel/karsilastir.kem` (41 satır) — **SAF** ✓

- Operasyonlar: `esit_mi`, `farkli_mi`, `karsilastir`, `en_kucuk_uc`,
  `en_buyuk_uc`
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan, herhangi bir port gereksizdir.

### `stdlib/temel/matematik.kem` (166 satır) — **SAF** ✓

- Operasyonlar: `mutlak`, `en_kucuk`, `en_buyuk`, `kare`, `kup`, `sinirla`,
  `isaret`, ...
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan, herhangi bir port gereksizdir.
- Not: `kdl_mutlak`/`kdl_min`/`kdl_maks` runtime helpers de aynı semantik
  (BARE-METAL-SAFE) ama kdl_runtime.c içinde; KEMGU programcısı bunun
  yerine `stdlib::temel::matematik`'i tercih etmelidir.

### `stdlib/temel/sayisal.kem` (52 satır) — **SAF** ✓

- Operasyonlar: `ortalama`, `us`, `obe` (GCD), `ekok` (LCM)
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan.

### `stdlib/opsiyonel.kem` (222 satır) — **SAF** ✓

- Operasyonlar: `seçimlik<T>` üzerinde harita/filtre/bağla/yansıt sözel
  wrapper'lar (concrete tipler için — KIRMIZI Madde D generic callback
  inference V2 nedeniyle).
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan.

### `stdlib/sonuc.kem` (260 satır) — **SAF** ✓

- Operasyonlar: `tamam(v)` / `hata(m)` construction wrapper'ları
  (pattern matching destekli inspection KIRMIZI Madde C nedeniyle V2).
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan.

### `stdlib/kripto.kem` (231 satır) — **SAF** ✓

- Sabitler + üst düzey API tanımları (bundle wrapper). İçinde gerçek
  algoritmik çağrı yok; submodüllere delegate eder.
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan (submodüllere bağlı).

### `stdlib/kripto/karma.kem` (274 satır) — **SAF** ✓

- SHA-256 + HMAC saf implementasyon (yalniz aritmetik + bit op).
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan — kriptografik karma freestanding'de
  güvenle çalışır.

### `stdlib/kripto/sifre.kem` (321 satır) — **SAF** ✓

- ChaCha20 + Poly1305 + AEAD saf implementasyon.
- Runtime çağrısı: **YOK**
- Bare-metal kullanım: Doğrudan. Constant-time disiplini için
  `sabitsüre<T>` koruması KEMGU tip sisteminde.

### `stdlib/kripto/anahtar.kem` (249 satır) — **KISMI** ⚠️

- Saf: anahtar tipi tanımı, struct constructors
- Bağımlı: `otp_sifrele` (runtime allocator + dosya I/O olası)
- Bare-metal: Sadece anahtar tip wrapper'ları kullanılabilir; OTP üretimi
  port gerek (PRNG + storage).

### `stdlib/kripto/rastgele.kem` (179 satır) — **KISMI** ⚠️

- Saf: xorshift64 algoritması KEMGU içinde tanımlı (algoritma seviyesi)
- Bağımlı: `hw_rastgele_aktif_mi`, `hw_rastgele_u64` — runtime/hardware
  TRNG (RDRAND/RNDR) primitifi. Şu an stub.
- Bare-metal: xorshift seed-aware kısmı kullanılabilir; hw_rastgele port
  edilmeli (kernel HW RNG driver).

### `stdlib/metin.kem` (142 satır) — **BAĞIMLI** ✗

- Operasyonlar: uzunluk, birleştir, kes, böl, küçük/büyük (TR-aware),
  içerir, başlar_ile, biter_ile, kırp, yer_değiştir, tekrarla, yansıt
- Runtime çağrıları (16 ref): `kdl_metin_uzunluk`, `kdl_metin_birlestir`,
  `kdl_metin_kes`, `kdl_metin_kucuk`, `kdl_metin_buyuk`, `kdl_metin_icerir`,
  `kdl_metin_baslar`, `kdl_metin_biter`, `kdl_metin_kirp`,
  `kdl_metin_yer_degistir` (+ TR/ASCII aliasleri)
- Bare-metal port: malloc → bump_allocator; strlen/memcpy manuel; strstr
  inline. Trivial-ama-uzun port. Bkz. BUMP_ALLOCATOR_SPEC_TASLAK.md.

### `stdlib/dizi.kem` (284 satır) — **BAĞIMLI** ✗

- Operasyonlar: `Dizi<T>` üzerinde harita_yerinde_tam, fold, dilimle,
  birleştir, ve concrete (tam32 + metin) sürümler
- Runtime çağrıları (6 ref): `dizi_olustur`, `dizi_ekle`, `kdl_dizi_*`
- Bare-metal port: malloc/realloc → bump_allocator + free no-op.
  KdlDizi ABI korunabilir; chunk allocator port edilir.

### `stdlib/dosya.kem` (208 satır) — **BAĞIMLI** ✗

- Operasyonlar: ac/oku/yaz/kapat/var_mi/sil/yeniden_adlandir/boyut +
  Capability V1 wrapper'ları (sabit + dokümantasyon)
- Runtime çağrıları (39 ref): `dosya_ac`, `dosya_oku`, `dosya_yaz`,
  `dosya_kapat`, `dosya_var_mi`, `dosya_sil`, `dosya_yeniden_adlandir`,
  `dosya_boyut`, `dosya_ac_yetkili`, `dosya_oku_yetkili`,
  `dosya_yaz_yetkili`, `dosya_kapat_yetkili`, `yetki_id`
- Bare-metal port: fopen/fread/... → block I/O driver ABI; dosya
  sistemi (FAT32/ext2 minimal). Faz 5+ kapsam.

---

## Özet Tablo

| Modül | Satır | Kategori | Runtime ref | Bare-metal hazır? |
|-------|-------|----------|-------------|-------------------|
| temel/karsilastir | 41 | SAF | 0 | ✓ direkt |
| temel/matematik | 166 | SAF | 0 | ✓ direkt |
| temel/sayisal | 52 | SAF | 0 | ✓ direkt |
| opsiyonel | 222 | SAF | 0 | ✓ direkt |
| sonuc | 260 | SAF | 0 | ✓ direkt |
| kripto (base) | 231 | SAF | 0 | ✓ direkt |
| kripto/karma | 274 | SAF | 0 | ✓ direkt |
| kripto/sifre | 321 | SAF | 0 | ✓ direkt |
| kripto/anahtar | 249 | KISMI | 1 (otp_sifrele) | ⚠️ kısmen |
| kripto/rastgele | 179 | KISMI | 2 (hw_rastgele_*) | ⚠️ kısmen |
| metin | 142 | BAĞIMLI | 16 | ✗ port gerek |
| dizi | 284 | BAĞIMLI | 6 | ✗ port gerek |
| dosya | 208 | BAĞIMLI | 39 | ✗ port gerek |

**Toplam:** 2629 satır stdlib; **1816 satır SAF (%69)** doğrudan bare-metal
kullanılabilir.

---

## Bare-Metal'de Doğrudan Kullanılabilir (V1)

Şu an `--hedef=aarch64-unknown-none` ile derleyince çalışacak:
- `stdlib/temel/karsilastir.kem`
- `stdlib/temel/matematik.kem`
- `stdlib/temel/sayisal.kem`
- `stdlib/opsiyonel.kem`
- `stdlib/sonuc.kem`
- `stdlib/kripto.kem` (base bundle)
- `stdlib/kripto/karma.kem` (SHA-256, HMAC)
- `stdlib/kripto/sifre.kem` (ChaCha20, Poly1305, AEAD)

**Mevcut altyapı yeterli:** `test/ornekler/kernel.kem` cross-compile zaten
bu modülleri import etmiyor; ama eklenmesi durumunda libc declare emit
edilmediği için sorun çıkmaz.

---

## Bare-Metal Port Yol Haritası (Faz 5+ Önerisi)

### Aşama 1 — Bump Allocator (BUMP_ALLOCATOR_SPEC_TASLAK.md)

- `malloc/realloc/free` → statik buffer üzerinde bump allocator
- KdlDizi, kdl_metin_* için backing allocator değişir, ABI korunur
- Bağımlı: kripto/anahtar (OTP key buffer)
- **Açar:** `stdlib/metin.kem`, `stdlib/dizi.kem` (kısmi — string/dizi op'ları)

### Aşama 2 — UART/Console Driver

- `kdl_yazdir_*` → UART putchar (ARM PL011, RISC-V SBI putchar)
- Bağımlı: SBI/HVC kernel ABI veya MMIO direct
- **Açar:** Debug yazdırma — kernel-level

### Aşama 3 — Hardware RNG

- `hw_rastgele_u64` → RDRAND (x86), RNDR (ARM), DGX Spark TRNG
- Bağımlı: CPU feature detection, capability gate
- **Açar:** `stdlib/kripto/rastgele.kem` (HW seed)
- **Açar:** `stdlib/kripto/anahtar.kem` (OTP üretim)

### Aşama 4 — Block I/O Driver

- `dosya_*` → AHCI/VirtIO-Blk/NVMe driver + FAT32 minimal FS
- Bağımlı: Aşama 1 + capability layer + interrupt handler
- **Açar:** `stdlib/dosya.kem` (tam set)

### Aşama 5 — Threading + IPC

- `kdl_gorev_*`, `kdl_kanal_*` → cooperative scheduler + IRQ-disable lock
- Bağımlı: Aşama 1 + saatler + IRQ infrastructure
- **Açar:** Concurrency / DRF V1 lang syntax (görev/kanal) gerçek runtime

---

## Onay Bekleyenler (Mehmet)

- **Aşamaların sırası:** Yukarıdaki Aşama 1-5 öneri; Mehmet sırayı değiştirebilir.
- **OTP runtime port önceliği:** Aşama 3 öncesi anahtar/rastgele KISMI kalır.
  V2 spec ile birlikte ele alınabilir.
- **`stdlib/dizi.kem` ABI:** Bump allocator değişikliği KdlDizi struct
  layout'u korusun mu yoksa yenilensin mi? — Bkz. BUMP_ALLOCATOR_SPEC_TASLAK.md.

---

**END BARE_METAL_DESTEK.md**

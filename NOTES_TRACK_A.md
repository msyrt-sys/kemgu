# Track A — Capability + I/O Bug Fix Bundle

**Branch:** `claude/flamboyant-keller-47727c` (worktree flamboyant-keller-47727c).
**Görev:** Direktif Ek v1.1 + KIRMIZI_QUEUE odaklı 3 kalem + otonom continuation.
**Tarih:** 2026-05-16.

---

## Keşif özetı (ADIM 0)

- Worktree zaten kuruluydu; ayrı `kemgu-trackA` yaratmadım — tek branch'te
  ilerleme commit zinciri olarak okunabilir.
- `belgeler/KEMGU_Capability_Spec_V1.md` (V1 onaylı) okundu.
- `runtime/kdl_runtime.c` ~1344 satır; `kdl_yetki_*` capability runtime'i hazır.
  Önemli: **`kdl_yetki_izin(KdlYetki y) -> uint16_t`** raw bit-field döner.
  Yeni eklenecek `kdl_yetki_izin_var_mi(y, izin) -> uint8_t` boolean check.
- `src/llvm.c` zaten `declare i16 @kdl_yetki_izin(%kdl_yetki)` yapmış AMA
  `src/tip_kontrol.c`'de **kullanıcı seviyesinde built-in olarak kayıt yok**.
  Şu an `yetki_izin(y, ...)` çağrısı KEMGU kodunda T005 verir.
- `src/llvm.c` hardcoded `target triple = "x86_64-pc-windows-gnu"`.
  Bare-metal hedef için `--baremetal` bayrağı ve alias üretimi yok.
- `runtime/kdl_dosya_oku` boş dosya için `""` döner (`fread n=0 → buf[0]='\0'`),
  hata için `NULL`. KEMGU tarafında NULL handling segfault riski — yeni
  built-in `dosya_oku_son_hata()` ile son çağrı hata kodu izlenecek.

## Çekirdek plan

### Kalem 1 — G.3 `yetki_izin` built-in
- **Risk sınıfı:** 🟢 (spec-içi: Capability Spec V1 CP.2 query method'u eksiği).
- API: `yetki_izin(y: yetki<R>, izin: tam16) -> mantıksal`
- Tüketim YOK (delege gibi — query işlemi, y aktif kalır).
- Runtime: `kdl_yetki_izin_var_mi(KdlYetki y, uint16_t izin) -> uint8_t`
- Tip_kontrol: özel-case (delege/geri_al pattern'i).
- LLVM: yeni declare + özel emit (i1 dönüş).
- 5+ test: var, yok, çoklu (oku+yaz mask), iptal sonrası 0, kaynak tipi yanlış (CP004 değil — query her tip üzerinde).

### Kalem 2 — G.4 `dosya_oku` empty/error ayrımı
- **Risk sınıfı:** 🟢 (yeni built-in, ABI kırmıyor).
- Yeni runtime fn: `kdl_dosya_oku_durum(yol, *out_hata) -> ptr`
  - boş dosya: ret="" (allocated), *out_hata=0
  - I/O hata: ret=NULL, *out_hata=hata kodu (-1 yok, -2 erişim, ENOENT/EACCES)
- Yeni KEMGU built-in: `dosya_oku_son_hata() -> tam32` (son `dosya_oku`'nın hata
  kodu — 0=OK boş dahil, !=0 hata)
- `dosya_oku` davranışı: hata durumunda `""` döner + global hata flag set
  (segfault önlemek için NULL kullanılmaz)
- stdlib/dosya.kem: `oku_metin` artık `dosya_oku_son_hata()` ile ayrımı yapar
- 4+ test: boş dosya, normal dosya, dosya yok, izin yok

### Kalem 3 — K8d `_baslat` ⇄ `main` alias
- **Risk sınıfı:** 🟡 (yeni CLI bayrağı + emit modu; spec-dışı küçük ek).
- Yeni bayrak: `--baremetal` (ana.c)
- Yeni LLVM API: `llvm_ir_uret_secenek(prog, out, baremetal_flag)`
- Bayrak aktifse:
  - target triple → `aarch64-unknown-none` (default ARM64 bare-metal)
  - main emit edildikten sonra `@_baslat = alias void(), ptr @main`
  - **Manuel override**: kullanıcı kendi `işlev _baslat()` tanımlamışsa
    alias üretilmez (çakışma önlenir)
- 3 IR test: host (default) → @main alone, --baremetal → main + alias, manuel
  _baslat → ALIAS yok

## Çekirdek Sonuç (3/3 kalem ✓)

| Kalem | Commit | Test Δ | Tip |
|-------|--------|--------|-----|
| G.3 yetki_izin | 80258b4 | +7 (47/47 capability) | 🟢 spec-içi |
| G.4 dosya_oku empty/err | 4e68ddb | +5 (110/110 LLVM) | 🟢 ABI uyumlu |
| K8d _baslat alias | 2c4202c | +3 (113/113 LLVM) | 🟡 yeni CLI flag |

**Toplam:** 15 yeni test, 3 commit, ASan temiz, stdlib --check tum
modüller geçer. Host davranışı regression yok.

### G.3 öğrenilen ders
Win64 ABI'sinde **16-byte struct by-value arg geçişi LLVM IR'de
güvensiz** — clang ya `byval` attribute eklemeli ya da pointer
geçirilmeli. Mevcut `kdl_yetki_delege`, `kdl_yetki_kontrol` declare'ları
by-value alıyor ama KEMGU testleri sadece tip-kontrol; LLVM
end-to-end test yoktu → segfault tetiklendiğinde fark edildi.
**Continuation kaynak**: bu declare'lar by-pointer'a çevrilmeli.

## Continuation log

### C1 — Capability runtime by-pointer ABI (229ac23) 🟡 → ÇÖZÜLDÜ
- **Tetik:** G.3 implementasyon sırasında segfault — Win64 ABI 16-byte
  struct by-value arg gevşek.
- **Etki:** `delege`, `yetki_kontrol`, `yetki_id`, vb. tüm capability
  query LLVM IR cagrilari etkileniyordu — mevcut tip-kontrol testleri
  geçiyordu çünkü end-to-end test yoktu.
- **Eylem:** tüm `kdl_yetki_*` query'leri `const KdlYetki *` arg'a
  geçirildi; LLVM declare/emit güncellendi; 3 yeni end-to-end test
  (`test_capability_olustur_geri_al`, `test_capability_delege_e2e`,
  `test_capability_yetki_izin_zincir`).
- **Test Δ:** +3 LLVM (116/116 toplam).

### C2 — KIRMIZI_QUEUE güncelleme (3d54e2b) 🟢 → ÇÖZÜLDÜ
- G.3/G.4/K8d/C1 "ÇÖZÜLDÜ" notu.
- Yeni gözlem: "16-byte by-value struct arg ABI riski" (action item).
- Yeni queue maddesi: "`sonuç<T,E>`/`seçimlik<T>` LLVM tagged union"
  (LLVM v4 kapsam — C3 audit ile tespit).

### C3 — sonuç pattern matching audit 🟢 → KAPSAM AYIRT EDİLDİ
- **Tetik:** KIRMIZI_QUEUE C maddesi (TOK_TAMAM/TOK_HATA desen eksik).
- **Bulgu:** parser.c sat 1054-1055'te zaten var; tip-check geçer.
- **Kalan iş:** LLVM tagged union codegen yok → derleme hatası
  (`undefined value '@tamam'`). Bu yeni KIRMIZI_QUEUE maddesi olarak
  ayrı yere alındı — LLVM v4 doğal kapsamı, **🔴 değil 🟡** (sum type
  encoding internal detail, spec etkisi yok).
- **Karar:** sahaya **uygulamadım** (kapsam dışı, halt değil).

### C4 — Örnek dosyalar (56c3bcf) 🟢 → ÇÖZÜLDÜ
- `test/ornekler/yetki_izin.kem`: G.3 kullanım, Linear V1 single-consume.
- `test/ornekler/dosya_oku_g4.kem`: G.4 son_hata kontrol akışı.
- İkisi de --check OK + LLVM derlemesi başarılı.
- **Bulgu:** Linear V1 path-sensitive değil — `eğer dal_a { geri_al; ver }`
  + `geri_al; ver` desenli kod CP005 verir. Tek consume noktası şart.

### C5 — Capability Spec V1 güncelleme 🟡
- CP.2.1 intrinsic tablosuna `yetki_izin(y, istenen) -> mantıksal`.
- CP.3'e yeni inference kuralı: CP-QUERY.
- CP.6.2 declare bölümü by-pointer ABI ile güncel.
- Bu **spec patch** seviyesinde değişiklik (yeni intrinsic + ABI clarification);
  yeni teorem/breaking yok → 🟡.

## Halt kriteri gözlemleri

- §A 🔴: Yok (her aday 🟢/🟡 sınıfında uygulandı veya kapsam dışı bırakıldı).
- Regression: Yok (47/47 capability + 116/116 LLVM + stdlib --check).
- Spec yetersizliği: C3 LLVM kısmı — yeni queue maddesi olarak kaydedildi
  ama Mehmet onayı bekleyen 🔴 değil (🟡 LLVM v4 alt-kalemi).
- ASLA ihlali: Yok.
- 3 üst üste 🔴: Yok.

## Sonuç

Çekirdek + 5 continuation tamamlandı. Doğal kapanış: tek başına bağımsız
ek bir 🟢/🟡 aday kalmadı; geriye kalan iş §C3 LLVM v4 (büyük kapsam) ve
Mehmet review borcu (bumper allocator, Spec B.0, Linear V2, UART).

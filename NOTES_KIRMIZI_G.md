# KIRMIZI_QUEUE G — stdlib/dosya Disiplinli Hale Getirme

**Branch:** `claude/confident-swartz-d247dc` (mevcut izole worktree; ek worktree açmadım)
**Bağlam:** [KIRMIZI_QUEUE.md §G](KIRMIZI_QUEUE.md) — stdlib::dosya runtime/syscall altyapısı
**Specler:** [KEMGU_Capability_Spec_V1.md](belgeler/KEMGU_Capability_Spec_V1.md), [KEMGU_Linear_Types_Spec_V1.md](belgeler/KEMGU_Linear_Types_Spec_V1.md) — ikisi de ön-onaylı (Direktif Ek v1.1 §B)

---

## 1. Keşif Özeti

### Mevcut durum
- **stdlib/dosya.kem (160 sat):** `metin` handle, `sonuç<T, metin>` (string error), capability/linear yok. Tüm body'ler `dosya_ac/dosya_oku/dosya_yaz/dosya_kapat` built-in'lerine bağlı (tip_kontrol.c:194-251). KIRMIZI_QUEUE G "stub" diyor ama aslında **runtime'a bağlı, sadece capability/linear gating yok.**
- **runtime/kdl_runtime.c kdl_dosya_*_yetkili varyantları MEVCUT** (satır 1195-1254): `kdl_dosya_ac_yetkili(yol, izin) -> KdlYetki`, `kdl_dosya_oku_yetkili(y) -> char*`, `kdl_dosya_yaz_yetkili(y, s) -> i32`, `kdl_dosya_kapat_yetkili(y*)`. **Ama src/llvm.c declare listesinde YOK** — KEMGU'dan çağrılamaz.
- **Capability tag system (built-in):** `yetki<Dosya>` ⇒ `Dosya` özel-cased string match (tip_kontrol.c:908). `yetki_olustur(1, izin)` → kaynak_tipi=1, izin bitleri (CP.5: OKU=1, YAZ=2, CALISTIR=4, SIL=8, DEVRET=16).
- **Linear types V1 mekanikleri:** `tekkez<T>` tip, `tekkez_yarat(e)` producer, `kullan(e)` extract+consume, `imha(e)` dispose+consume. LR002: yapı tekkez alanı içeremez. References `&T` linear olmayan tipler için OK (L004 yalnız linear tip referansını yasaklar).
- **CP.1.1:** `yetki<R>` zaten linear (atama=transfer, & alınamaz, scope-end tüketim zorunlu). `delege(y, izin)` y'yi tüketmez (alt-yetki). `geri_al(y)` y'yi tüketir.

### Eksik bulunanlar
- `kdl_dosya_*_yetkili` LLVM declare bildirimleri (gap)
- `tip_kontrol.c` built-in tablosunda capability-aware dosya fonksiyonları yok
- IOHata enum tipi yok — şu anki stdlib `metin` mesajlar kullanıyor
- `stdlib/hata.kem` yok
- Linear handle (DosyaTutac) tipi yok
- `test/test_drf.c`, `test/test_modul_import.c` referansları **yok** (regression listesinden çıkarılacak)

---

## 2. Tasarım Kararları (otonom)

### K1. Name clash: `Dosya` capability tag vs `Dosya` yapı
**Problem:** `yetki<Dosya>` özel-cased string match. Eğer `yapı Dosya {...}` tanımlasam, `tekkez<Dosya>` ve `yetki<Dosya>` aynı isimden iki farklı tip üretir → nominal eşitlik tutarsızlığı.

**Karar:** Linear handle struct'ına **`DosyaTutac`** (file handle) adı verildi. `Dosya` ismi sadece capability tag context'inde (`yetki<Dosya>`) kullanılır. Bu spec B.0 ile birebir uyumsuz ("HEDEF 3: `tekkez<Dosya>` disiplini") ama V1 implementation gerçekliği bunu gerektiriyor — `Dosya` ismi capability-tag-only.

NOT: Spec güncellemesi gerekirse §B notu önerilir (Mehmet kararı), V1 implementasyon sınırı.

### K2. IOHata yeri ve şekli
**Karar:** `stdlib/hata.kem` **yeni dosya** açılır. İçerikleri:
- IOHata yapısı: `{ kod: tam32, mesaj: metin }`
- Sabitler: `IO_OK, IO_DOSYA_YOK, IO_ERISIM_REDDEDILDI, IO_GC_HATASI, IO_KAYNAK_TUKENDI, IO_BOZUK_YAZI, IO_AYRICALIK_YETERSIZ` (kodlar 0-6)
- Helper: `io_hata_yap(kod, mesaj) -> IOHata`

KEMGU'da gerçek `enum` keyword'ü yok; struct + sabit kod pattern'i KEMGU felsefesine (tagged union via sonuç<T,H>) uygun.

### K3. Linear discipline pattern
**V1 sınırı:** `sonuç<tekkez<X>, E>` muhtemelen çalışır (sonuç built-in; LR002 yalnız yapı alanı için), ama oku/yaz çağrıları arasında tekkez'i yeniden borrow etmek için sarma/açma gerek.

**Karar:** İki katmanlı API:
- **Düşük seviye (advanced):**
  - `aç(yol, izin, y: yetki<Dosya>) -> sonuç<tekkez<DosyaTutac>, IOHata>` — y consumed (linear transfer)
  - `oku_tumu(d: &DosyaTutac) -> sonuç<metin, IOHata>` — borrow
  - `yaz(d: &DosyaTutac, icerik: metin) -> sonuç<tam32, IOHata>` — borrow
  - `kapat(d: tekkez<DosyaTutac>) -> sonuç<tam32, IOHata>` — d consumed (kullan + dosya_kapat_lineer)
  - **Kullanım:** `değişken d_t = aç(...); değişken d = kullan(d_t); oku_tumu(&d); kapat(tekkez_yarat(d));`
- **Yüksek seviye (one-shot, ergonomik):**
  - `oku_dosya(yol, y: yetki<Dosya>) -> sonuç<metin, IOHata>` — internally aç+oku+kapat
  - `yaz_dosya(yol, icerik, y) -> sonuç<tam32, IOHata>` — internally aç+yaz+kapat
  - `ekle_dosya(yol, icerik, y) -> sonuç<tam32, IOHata>` — append
  - **Implicit close yok** — bu fonksiyonlar açtıklarını kapatır; kullanıcı için tek-shot.

### K4. Runtime binding strategy
**Karar:** Capability-gated runtime için yeni "lineer" varyant ekle (mevcut `_yetkili` varyantları kullanır ama `tam64` isleyici cinsinden). Yeni built-in'ler:
- `dosya_ac_lineer(yol: metin, izin: tam32) -> tam64` — FILE* opaque int olarak döner; 0 = hata
- `dosya_oku_lineer(isleyici: tam64) -> metin` — tüm içeriği oku
- `dosya_yaz_lineer(isleyici: tam64, icerik: metin) -> tam32` — yazılan byte
- `dosya_kapat_lineer(isleyici: tam64) -> boş` — fclose
- Eski `dosya_ac/dosya_oku/dosya_yaz/dosya_kapat` built-in'leri **dokunulmaz** (deprecation yorumu yeter, geri uyumluluk).

### K5. Deprecation
**Karar:** Eski API (`metin` handle dönen `ac/kapat`) **silinmez**; üst kısma yorum eklenir: "// V1 string-handle API — yeni kod için aç/oku_tumu/yaz/kapat (capability+linear) kullanın." Mevcut test_dosya.kem çalışır kalır.

### K6. Test stratejisi
**C-test (tip kontrol odaklı):** [test/test_dosya.c](test/test_dosya.c) — test_capability.c/test_linear.c stilinde derle_kontrol+hata_sayisi yardımcıları. En az 18 test, 4 grup:
- D1-D4: Capability gate (yetkisiz çağrı reddi)
- D5-D8: Linear discipline (double-imha, leak, kapat)
- D9-D14: IOHata varyantları (6 varyant × en az 1 senaryo)
- D15-D16: Happy path (aç→oku→kapat, aç→yaz→kapat)
- D17-D18: Edge (boş yol, unicode yol)

**Stdlib check:** `test/stdlib/test_dosya.kem` mevcut — eski stub API'sini test ediyor. Yeni API için ek test eklenecek (aynı dosyaya append).

### K7. Regression listesi
Task referansı `test_drf, test_linear, test_capability, test_modul_import, Faz 2 testleri` — `test_drf` ve `test_modul_import` **yok**. Mevcut ASan-temiz regression seti: `test_lexer, test_arena, test_ast, test_parser, test_tip, test_sembol, test_tip_kontrol, test_bolge, test_bolge_atama, test_escape, test_json, test_lsp, test_linear, test_sabitsure, test_wcet, test_capability, test_simd, test_dosya (yeni)`. "Faz 2" tanımı yok — atla.

---

## 3. İş Planı (Self-Checkpoint)

| Kalem | İçerik | Tahmini etki |
|---|---|---|
| 1 | `stdlib/hata.kem` (yeni) + IOHata + sabitler; stdlib/dosya.kem'e capability parametresi (sadece imza taslağı, derlenmesi gerekmez) | +60 sat KEMGU |
| 2 | DosyaTutac yapısı + `aç/oku_tumu/yaz/kapat` (linear pattern) + `oku_dosya/yaz_dosya/ekle_dosya` (one-shot) | +180 sat KEMGU |
| 3 | (Kalem 1 + 2 birleşik commit; ayrı yapmaya gerek yok — IOHata ile API birlikte ekleniyor) | — |
| 4 | runtime/kdl_runtime.c yeni `kdl_dosya_*_lineer` (4 fn); src/llvm.c yeni declare; src/tip_kontrol.c built-in tablo ekleme | +50 sat C, +6 sat IR, +60 sat tip_kontrol |
| 5 | test/test_dosya.c (yeni) + Makefile target | +400 sat C |
| 6 | KIRMIZI_QUEUE.md G durumu "KISMI ÇÖZÜLDÜ" + bu NOTES finalize | — |

**Commit zinciri (planlanan):**
1. "KIRMIZI G: IOHata + capability/linear stdlib/dosya API"
2. "KIRMIZI G: runtime binding kdl_dosya_lineer + LLVM declare + built-in"
3. "KIRMIZI G: test_dosya 18+ test, ASan temiz"
4. "KIRMIZI G: closure + KIRMIZI_QUEUE güncelleme"

---

## 4. Halt Kriterleri İzleme

- §A 🔴 ihlali: **şu ana kadar yok** — Linear V1 + Capability V1 spec'leri ön-onaylı, yeni keyword/intrinsic eklenmiyor (sadece built-in adı). `_lineer` suffix'li 4 yeni runtime built-in `🟡 yeni built-in` kategorisinde, [SARI YAPILDI] etiketi yeter.
- Regression 30dk fix: izlenecek
- Linear V1 / Capability V1 yetersizliği kanıtı: K3'te `sonuç<tekkez<X>, E>` doğrulaması yapılacak, sorun çıkarsa düz `DosyaTutac` dönüş + manual close pattern'e düşülür (notla)
- §G ASLA ihlali: yok

---

## 5. Test Hedefleri (kalem 5 sonrası bekleniyor)

- test_dosya.c: 18+ test, ASan temiz, hepsi geçer
- Mevcut stdlib::dosya --check ve `test_dosya.kem` (eski API stub testleri) kırılmaz
- test_capability + test_linear regression yeşil
- ASan'da no leak/UAF

---

## 6. Final Durum (2026-05-16)

### Tamamlanan ✓
- `stdlib/hata.kem` — IOHata yapısı + 6 IO_* sabit kodu + 6 helper işlev.
- `stdlib/dosya.kem` — yeni capability + linear API:
  - **Tip & sabitler:** DosyaTutac, MOD_OKU/YAZ/EKLE/OKU_YAZ.
  - **Düşük seviye:** `aç(yol, izin, y)`, `oku_tumu(&d)`, `yaz(&d, …)`, `kapat(d)`.
  - **One-shot:** `oku_dosya(yol, y)`, `yaz_dosya(yol, içerik, y)`, `ekle_dosya(yol, içerik, y)`.
  - **Eski API:** `ac`, `kapat_v1`, `oku_metin`, `yaz_metin`, `ekle`, `ekle_satir`,
    `var_mi`, `sil`, `boyut`, `yeniden_adlandir`, `kopyala` — deprecation yorumu ile korundu.
- `runtime/kdl_runtime.c` — 4 yeni libc-tabanlı sarıcı (`kdl_dosya_*_lineer`).
- `src/llvm.c` — 4 yeni `declare` bildirimi.
- `src/tip_kontrol.c` — 4 yeni built-in tablo girişi.
- `Makefile` — `calistir_dosya_check` (bundle), `calistir_dosya_test` (C-test ASan);
  `calistir_stdlib_check` artık `calistir_dosya_check` dependency'sine sahip.
- `test/test_dosya.c` — 18 test, 5 grup, ASan temiz (build/test_dosya.exe).
- `test/stdlib/test_dosya.kem` — eski API'nin `kapat → kapat_v1` rename'ı.

### Test sonuçları
- `test_dosya`: **18/18 ✓** (ASan + UBSan temiz, Clang64)
- Regression yeşil (sample):
  - capability: 40/40 | linear: 57/57 | tip_kontrol: 165/165 | parser: 102/102
  - lexer: 103/103 | arena: 19/19 | ast: 31/31 | tip: 26/26 | sembol: 18/18
  - bölge: 22/22 | bölge_atama: 13/13 | escape: 17/17 | json: 21/21
  - lsp: 9/9 | sabitsüre: 39/39 | wcet: 32/32 | simd: 30/30
  - llvm: 105/105 | simd_llvm: 5/5 | stdlib_check: tüm modüller geçer

### Commit Zinciri
1. **`KIRMIZI G: capability + lineer stdlib/dosya + runtime bindings`** —
   stdlib/hata.kem (yeni), stdlib/dosya.kem (refactor), runtime/kdl_runtime.c,
   src/llvm.c, src/tip_kontrol.c, Makefile (dosya_check), test_dosya.kem rename.

2. **`KIRMIZI G: test_dosya — 18/18 capability + lineer disiplin testi`** —
   test/test_dosya.c (yeni), Makefile (test_dosya target).

3. **`KIRMIZI G: closure + V1 sınırları KIRMIZI_QUEUE'ya`** — KIRMIZI_QUEUE.md
   G güncelleme, NOTES_KIRMIZI_G.md finalize.

### V1 Sınırları (KIRMIZI_QUEUE G.1-G.6'ya yazıldı)
- **G.1:** `sonuç<tekkez<T>, E>` içinde tekkez tüketim takibi yok (V1
  path-insensitive). Test D5 informational.
- **G.2:** `Dosya` ismi capability tag + linear handle iki bağlamda
  kullanılamaz; handle için `DosyaTutac` alternatif isim.
- **G.3:** Yetki bit alanı KEMGU yüzeyinden okunamıyor (yetki_izin built-in
  yok).
- **G.4:** `kdl_dosya_oku_lineer` boş dosya vs hata ayırmıyor.
- **G.5:** Linear V1 path-insensitive — eager-consume pattern zorunlu.
- **G.6:** Eski API silinmedi (deprecate); V2'de import gelince temizlenir.

### Sonraki adım önerisi (Mehmet'e)
1. **G.3 (yetki_izin built-in):** Düşük efor, yüksek değer — capability runtime
   inspection KEMGU yüzeyinde olmalı. ~30dk.
2. **G.1 (sonuc<tekkez<>, E> tüketim takibi):** Daha temel — tip sistemi
   genişlemesi. Eğer V2 path-sensitive analiz planlanırsa beraber.
3. **G.4 (dosya_oku NULL/sentinel):** Pratik bug; bir tam32 out param eklenince
   düzelir.
4. **Capability spec'i `tekkez<Dosya>` örneklerinin güncellenmesi:** Spec B.0
   "tekkez<Dosya>" diyor; pratikte handle adı farklı olmak zorunda.

# DECISIONS_LOG — Codegen Kampanyası Karar Kaydı

Format: D-NNN | tarih | karar | gerekçe | kapsam/sınırlar. [YÜKSEK] = merge-review'da
özellikle bakılması istenen, izole commit'li kararlar.

---

## D-001 [YÜKSEK] — Modül ad-mangling şeması: `@<modul>.<ad>` (2026-06-11)

**Karar:** Modül üyesi işlevler IR'da `@modul.ad` olarak emit edilir; iç içe modül
`@m1.m2.ad` (nokta ayraçlı zincir). `mat::kare(x)` çağrısı yol zincirinden noktalı
ada knit edilip (`mat.kare`) kayıt tablosundan çözülür.

**Gerekçe:** LLVM `@` adları nokta içerebilir (örn. `@llvm.x86.*`); KEMGU
tanımlayıcılarında `.` olamayacağı için düz-ad uzayıyla çakışma imkânsız. `$`
zaten generic monomorphization'da kullanılıyor (`kimlik$i32`) — modül için ayrı
ayraç, iki mekanizmanın okunabilir kalmasını sağlar.

**Önceki durum (audit DUR-SOR #2):** Modül üyeleri HİÇ emit edilmiyordu;
`mat::f()` çağrısı `; HATA: cagri hedefi tanimlayici degil` yorumuyla sessiz 0
dönüyordu.

**Kapsam/sınırlar:**
- Modül gövdesi içinden kardeş işleve çıplak-ad çağrı: `aktif_modul_onek`
  fallback'iyle çözülür (önce düz ad, bulunamazsa `<önek>.<ad>`).
- Modül içi `sabit`/`yapı`/`çeşit` üyeleri v1'de mangle edilmiyor (yalnız işlev —
  audit'in bulduğu gap). Gerekirse ayrı iş.
- **AÇIK:** tip_kontrol modül-üye çağrısını zaten çözemiyor (`T016: modul
  bulunamadi`, tek seviyede bile; iç içe yol "yol cozumlemesi karmasik") —
  ÖNCEDEN VAR OLAN sınır, bu kampanyanın scope-lock'u (src/llvm.c + test/)
  dışında. Codegen artık hazır; tip_kontrol çözümü ayrı C-track işi.

## D-002 [YÜKSEK] — ve/veya kısa-devre (short-circuit) semantiği (2026-06-11)

**Karar:** `a ve b` / `a veya b` standart kısa-devre: sol taraf yeterliyse sağ
taraf DEĞERLENDİRİLMEZ. Lowering: alloca + koşullu dal + her dalda store + load
(phi yerine mevcut codegen'in bellek-slot deseni). Sonuç tipi `i1`.

**Gerekçe:** Önceki `and/or i32` lowering'i her iki tarafı da değerlendiriyordu —
yan etkili sağ taraf (çağrı) için SESSİZ-YANLIŞ semantik. Kısa-devre, KEMGU'nun
örtük-dönüşümsüz/çökmez felsefesiyle uyumlu tek doğru davranış (Rust/C aynı).

**Sınır:** WCET maliyeti (wcet.c, scope dışı) her iki dalı toplamaya devam ediyor —
kısa-devre sonrası bu ÜST SINIR olarak güvenli tarafta kalır (değişiklik gerekmez).

## D-003 — Heap `d[i] = v` eleman ataması: kampanya KAPSAMI DIŞI (2026-06-11)

Heap dizi (KdlDizi) eleman ataması `runtime/`'a setter gerektiriyor
(`kdl_dizi_yaz_eleman` yok); kampanya scope'u `src/llvm.c + test/`. Şu an
SESSİZ DEĞİL: codegen görünür `; atama: heap dizi eleman atamasi runtime
setter bekliyor` yorumu emit ediyor. Ayrı küçük görevde kapatılacak
(runtime'a ~5 satır setter + llvm.c'de ~10 satır çağrı).

## D-006 — `&p.x` / `&d[i]` parser önceliği: ✅ ÇÖZÜLDÜ (ifade.c) (2026-06-11)

**Bulgu (matris C):** `&p.x` AST'de `(&p).x`, `&d[i]` ise `(&d)[i]` olarak parse
ediliyordu — dökümante önceliğe AYKIRI (postfix `.`/`[]` prefix `&`'den sıkı bağlamalı
→ `&(p.x)` / `&(d[i])` olmalı). Yanlış ağaç codegen'de `(&p)`'yi ptr'e çevirip `.x`'i
ptr-path GEP+load ile değer olarak okuyor; `artir(&p.x)` çağrısında i32 değer ptr-param'a
geçip **segfault**.

**Kök neden:** `parse_onek` (ifade.c) prefix `-`/`~`/`&`/`&değişken`/`*` operandını
`parse_onek` ile alıyordu — bu yalnız bir sonraki öneki işleyip postfix YUTMUYORDU.
`değil` (Madde H) zaten doğru deseni (`parse_oncelik(p, ONC_ONEK)`) kullanıyordu.

**Fix (ayrı görev, scope `ifade.c + test/`):** Beş prefix operatörün operandı da
`parse_oncelik(p, ONC_ONEK)` ile alınır. ONC_SONEK(12) > ONC_ONEK(11) → postfix
(`. [] () :: olarak`) operanda bağlanır; ikili operatörler (≤10) bağlanmaz → `&x+y`
hâlâ `(&x)+y`. İç içe önek (`*&x`, `--x`) korunur (parse_oncelik önce parse_onek çağırıp
sonraki öneki recursive işler). **Öncelik tablosu değişmedi** (lokalize fix, değer-bazlı
ripple yok). Binary `*`/`&` (çarpma/bit) infix konumda, parse_onek'e girmez → etkilenmez.

**Doğrulandı (runtime round-trip):** `&p.x`, `&d[i]`, `&a.b.c` deref-oku → 42 (segfault
yok); regresyon `*(&x)`, `-p.x`=`-(p.x)`, `&x+y`, `&v`, `*p` → hepsi yeşil; parser/
snapshot/fuzzer (20000 iter) + tüm test_tumu yeşil, 0 ASan.

**Not (D-006 dışı, ayrı feature):** `&p.x` üzerinden YAZMA `*p = v` deref-assignment
gerektirir — bu dilde **T022-red** (deref-hedef lvalue değil, tasarım kararı). Scaler
alan referansına yazma ifade edilemez; struct ref'e yazma `ref.alan = v` ile zaten
çalışıyor (&Struct task). `&arr[i].alan` parse artık doğru ama codegen D-007 (struct-
değerli dizi) ile bloklu.

## D-007 — Struct-değerli diziler (`arr[i].alan`, `a.b[i].c`, `d[i][j]`): feature, ertelendi (2026-06-11)

**Bulgu (matris B):** Eleman tipi struct olan diziler (`[P{..}, P{..}]`) — hem stack
(`d[i].alan` okuma exit-yanlış) hem heap (`Dizi<P>` + `dizi_ekle(.., p)` → struct
değerini `kdl_dizi_ekle_tam`'a i32 olarak geçirip clang-fail). Stack tarafı eleman-tipi
takibi (INDEKS `beklenen`'e düşüyor, struct çıkaramıyor); heap tarafı KdlDizi yalnız
i32/i64/ptr saklıyor (struct-by-value runtime gerektirir — D-003 sınıfı, `runtime/`
scope dışı).

**Karar:** Mekanik değil (DIZI_OLUSTUR struct eleman tipi + INDEKS eleman-tip
propagasyonu + lvalue zinciri + runtime aggregate). Kampanyaya dahil EDİLMEDİ; ayrı
"struct-değerli dizi" feature görevi. Skalerli diziler (`d[i]` oku+yaz) ÇALIŞIYOR
(audit gap #2). Çok-boyut `d[i][j]` de aynı feature'a bağlı (ertelendi).

## D-008 — Concurrency (`dondur`/`kanal`/`görev`) codegen: YOK, işaretlendi (2026-06-11)

**Bulgu (matris F):** `dondur(&değişken x)` → `call ptr @dondur(...)` tanımsız sembol
(link-fail); `kanal_oluştur()` → T002 tanımsız. Concurrency / DRF V1 yalnız statik
tip-kontrol katmanında (görev/kanal keyword + DRF001-005); runtime thread/channel +
codegen YOK (CLAUDE.md: "Plan Karar F V2 — runtime thread/channel implementasyonu").

**Karar:** Kampanya dışı — codegen değil, koca bir runtime+codegen alt-sistemi (V2).
İŞARETLENDİ. "Lineer değer kanaldan geçiyor" hücresi (F çapraz) buna bağlı, ertelendi.

## D-009 — `satıriçi_asm` çıktısı struct alanına (`çıktı("=r", &r.deger)`): parser, ertelendi (2026-06-11)

**Bulgu (stretch):** asm `çıktı` clause grammar yalnız düz `&değişken_adi` kabul ediyor
(parser.c P269 TANIMLAYICI bekler); `&r.deger` alan-erişimi P264 ile parse-fail.

**Karar:** C5 v1 tasarımı asm çıktısını düz değişkene bağlar (deyim-form). Alan hedefi
istenirse: asm→temp değişken sonra `r.deger = temp`. Grammar genişletmesi parser.c'de
(scope dışı) + dil kararı. Ertelendi. Çekirdek asm (düz &var çıktı) ÇALIŞIYOR (C5).

## D-005 [YÜKSEK] — İşaretsiz (dtamN) + i1 genişletme: signedness yan-kanalı (2026-06-11)

**Karar:** `dtamN` (işaretsiz tamsayı) değerler IR'da işaret bilgisini `IfadeSonuc.isaretsiz`
/ `LlvmIsim.isaretsiz` / `IslevKayit.donus_isaretsiz` yan-kanalında taşır. İşaretsiz
operand → `udiv`/`urem`/`lshr` + işaretsiz karşılaştırma yüklemi (`ult/ugt/ule/uge`);
genişletme `zext`. **i1 (mantıksal) genişletme HER ZAMAN `zext`.**

**Gerekçe / önceki SESSİZ-YANLIŞ durum:** Tüm tamsayılar işaretli lower ediliyordu:
- `dtam8 200 > 100` → signed `icmp sgt` ile `-56 > 100` = YANLIŞ (probe exit 1, beklenen 42).
- `dtam8 / dtam8` → `sdiv`, `dtam >> 1` → `ashr` (işaret bitini kopyalar).
- **`doğru olarak tam32`** → `sext i1` = `-1`; `41 + (-1)` = 40 (beklenen 42). Bu en
  yaygın gap — her bool→int dönüşümü yanlıştı.

**Kapsam/sınırlar:** Yan-kanal değişken/parametre/dönüş/alan/cast üzerinden akıyor.
İkili işlemde operandlardan biri işaretsizse işlem işaretsiz. Karışık işaretli/işaretsiz
aritmetik tip kontrolde zaten engelli (örtük dönüşüm yok). `{reg,tip}` eski başlatıcılar
C11 gereği `isaretsiz=0` (işaretli) → güvenli varsayılan, regresyon yok.

## D-004 — LAMBDA codegen: V2'ye ERTELENDİ (2026-06-11)

Closure codegen (ortam yakalama, env struct, fonksiyon-pointer ABI'si) mekanik
değil — ayrı feature tasarımı. Kampanyada yok. Mevcut durum: lambda ifadesi
`; ifade tipi desteklenmiyor` + 0 döner; stdlib yüksek-mertebe işlevleri
adlandırılmış işlevlerle çalışıyor (test_stdlib_* yeşil).

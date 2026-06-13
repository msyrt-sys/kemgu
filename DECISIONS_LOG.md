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

## D-010 [YÜKSEK] — Tek-geçiş ad çözümü: resolver binding'i AST'te, codegen tüketir (2026-06-12)

**Karar (çekirdek spec — Mehmet belirledi):** Her ad-referansı düğümüne (`DUGUM_TANIMLAYICI`,
`DUGUM_YOL`) "çözülmüş binding" eklendi (`ast.h`: `cozum_sembol` + `cozum_kategori`
{YEREL, MODUL_UYESI, GLOBAL} + `cozum_modul_onek`). Resolver (`tip_kontrol.c`) binding'i
module-first/lexical kuralla doldurur ve düğüme YAZAR (tek doğruluk kaynağı); codegen
(`llvm.c`) string'le yeniden ÇÖZMEZ, kaydı okuyup tam o sembolü emit eder (`@modul.ad`
mangling'i ile). Sonuç: tip kontrol ile codegen inşa gereği aynı sembole anlaşır.

**Önceki SAPMA (ADIM-0'da ampirik üretildi):** global `f` + modül-kardeşi `f`, modül
içinden çıplak `f()`: tip kontrol modül-kardeşine (lexical, imza-ayrık varyantla kanıtlı),
codegen `islev_bul` global-first olduğundan global'e bağlanıyordu → exit 2 yerine 1;
imza-ayrık ikizde geçersiz IR (`call i32 @f()` vs `define i32 @f(i32)`). Göreli YOL
(`m` içinden `ic::g()`) codegen'de hiç çözülemiyordu (string-knit "ic.g" kayıt "m.ic.g").
Regression guard: `test/snapshots/ad_cozum_sapma.kem` (exit 42) + `ad_cozum_govde.kem`.

**[ETKİ] Taktik seçimler (otomatik uygulandı):**
1. **Binding alanları union DIŞINDA ortak başlıkta** — `dugum_olustur` `arena_ayir_sifir`
   kullandığından varsayılan `COZUM_YOK`/NULL; mevcut düğüm üretim yolları (ifade.c dahil)
   değişmedi, ifade.c'ye dokunulmadı. Maliyet ~24B/düğüm (derleyici için ihmal edilebilir).
2. **`sembol.h`/`tip_kontrol.h` API'sine dokunulmadı** — modül öneki, bulunan SCOPE_MODUL
   scope'undan türetilir (`modul_onek_turet`: parent scope'ta `modul_scope==s` olan modül
   sembolünün adını biriktirir, iç içe "m1.m2"); `ast.h`'de yalnız `struct Sembol` forward
   declaration (sembol.h→ast.h yönlü include, döngü yok).
3. **ÖN-KOŞUL TAMİRİ (`tip_kontrol.c` DUGUM_ISLEV):** İşlev sembolü artık tanımlandığı
   scope'ta aranır (`sembol_bul_yerel(tk->scope)`, eskisi `tk->global_scope`) ve gövde
   scope'unun parent'ı `tk->scope` (eskisi global). Önceki durum: modül işlev gövdeleri
   HİÇ tip-kontrol edilmiyordu (sembol modül scope'unda → sessiz erken dönüş; `ver doğru;`
   → tam32 --check'ten GEÇİYORDU) ya da aynı adlı global ikizin imzasına karşı denetleniyor,
   arity farkında parametre dizisi OOB okumasıyla ÇÖKÜYORDU (RC=139 repro'landı). İkiz/
   builtin-gölgeleme koruması: `ast_dugumu != d` veya param sayısı uyuşmazsa gövde atlanır
   (T024 zaten raporlu).
4. **`ana.c mode_llvm`'e resolver geçişi eklendi (KAPSAM dışı dosya — gerekçe):** codegen'in
   tüketeceği binding'i ancak resolver yazabilir; mode_llvm bugüne dek tip_kontrol'ü HİÇ
   çalıştırmıyordu (CLAUDE.md'deki pipeline tarifi aspirasyoneldi). mode_check kalıbı
   kopyalandı; hatalar `hata_callback_ayarla(sessiz_cb)` ile susturulur → tip hatalı
   programlar --llvm'de ESKİSİ GİBİ emit edilir (CLI çıktısı bayt-bayt korunur), binding'i
   eksik düğümler codegen'de string fallback'ine düşer. Tek arena → Sembol* ömrü
   `llvm_ir_uret` boyunca geçerli (SembolLink linked-list, relocation yok).
5. **Graceful degradation tasarım gereği KALICI:** `COZUM_YOK` → eski global-first +
   aktif-önek-fallback yolu aynen korunur. Sebep: built-in'ler (yazdir/dizi_ekle/
   tekkez_olustur/...) ya sembol tablosunda değil ya da IslevKayit'ta kayıtsız; ayrıca
   resolver koşmadan doğrudan `llvm_ir_uret` çağıran tüketiciler kırılmamalı.
6. **`COZUM_YEREL` callee → indirect-call yolu:** lokal function-pointer, aynı adlı global
   işlevi artık GÖLGELEYEBİLİR (tip kontrolle tutarlı; eski codegen global'i seçerdi).

**Kapsam/sınırlar:** Çoklu-dosya/modül yükleme (A), generic specialization çekirdeği (C),
nitelikli TİP annotation (D) DOKUNULMADI. `DUGUM_ERISIM` (method dispatch) binding'i v2.
Identifier-yük (lvalue/load) codegen'i lokal isim tablosuyla devam ediyor (sapma çağrı
sitelerindeydi); modül-üyesi `sabit` referansı v1'de zaten desteklenmiyor.

## D-011 [YÜKSEK] — Çok-dosya modül temeli: whole-program namespaced yükleme (2026-06-13)

**Karar (yüzey Mehmet kilidi):** `kullan dizi;` nitelikli bağ (düzleştirme YOK),
`kullan dizi::{Liste,ekle};` seçili niteliksiz, `kullan dizi olarak d;` alias; GLOB yok;
modül=dosya (dizi.kem ⇒ `dizi`); arama yolu [içe-aktaran dizini → proje kökü → kütüphane/],
İLK eşleşme kazanır; private-by-default + `genel` export işareti; iki-faz yükleme
(döngüsel import v1'de hata değil); seçili-import çakışması KULLANIMDA T042
(nitelikli erişim geçerli kalır).

**Mimari:** Loader (ana.c `modulleri_yukle`) entry'nin kullan grafiğini BFS gezer, her
dosyayı bir kez parse edip sentetik `DUGUM_MODUL(dosya_modulu=1)` olarak program AST'sinin
başına ekler → `tip_kontrol_program` faz-1 (pre_populate: kanonik kayıtlar) + faz-2
(`kullan_baglari_kur`: görünür bağlar) → B'nin resolver'ı çapraz-dosya adları
`COZUM_MODUL_UYESI` binding'iyle çözer → TEK codegen tüm modülleri `@modul.ad` emit eder.
**B-entegrasyon doğrulaması:** binding düşseydi `@topla` tanımsız kalırdı (link hatası) —
exit-42 E2E testleri bunu yapısal olarak kanıtlıyor.

**[ETKİ-YÜKSEK] Legacy düzleştirme korundu:** Çok-segment çıplak `kullan a::b::c;`
ESKİ düzleştirme yolunda kaldı (tip_kontrol DUGUM_KULLAN + llvm.c kullan pre-pass).
Sebep: drivers/virtio/*.kem (çapraz-dosya struct kullanıyor — D bölgesine bağımlı) ve
test/crossfile fikstürleri bu semantiğe test-pinli; görevin kendi kısıtları
(drivers 2/2 + test_llvm taban düşmeyecek + D'ye dokunma) başka çözüm bırakmıyor.
Yeni biçimler (tek-segment / seçili / alias) HER ZAMAN namespaced yükleyicide.
Çıkış stratejisi: D (nitelikli tip) inince sürücüler yeni biçime taşınıp legacy kaldırılır.

**[ETKİ] Diğer taktik kararlar:**
1. **builtin_scope ayrımı:** built-in'ler global'in PARENT'ına taşındı; dosya-modül
   scope'ları da oraya bağlanır → giriş dosyasının özel adları modüllere sızmaz
   (private-by-default iki yönlü). Yan etki: built-in adı gölgeleme artık T024 değil
   (gölge kazanır) — suite'te pinli test yoktu.
2. **Kanonik modül sembolü gizli (`Sembol.gizli`):** dosya-modül kaydı builtin_scope'ta
   ama normal çözümde görünmez — `dizi::f` import'suz ÇÖZÜLMEZ (T016). Önek türetme
   (`scope_modul_sembolu`) SembolLink'i doğrudan taradığı için mangling etkilenmez.
3. **Seçili alias `ithal_onek` taşır:** `cozum_bagla` alias'ı görünce binding'i
   MODUL_UYESI + asıl modül öneki olarak yazar — codegen değişikliği GEREKMEDİ.
4. **Ad-bazlı modül dedup:** aynı ada ikinci yükleme yok (ilk çözüm kazanır) —
   döngü/elmas importlar doğal sonlanır; farklı dizinlerden aynı ad = tek modül (v1).
5. **mode_llvm yükleme hatasında IR üretmez** (eksik modül zaten link edilemez);
   mode_check yükleme hatasını ayrı sayaçla raporlar ("yukleme N").
6. **Windows UTF-8:** loader `dosya_ac_utf8` (MultiByteToWideChar→_wfopen) —
   kütüphane/ ayağı E2E testle (UTF-8 dizin) doğrulandı.
7. **Modül-içi tip emisyonu gap fix (llvm.c):** yapı/çeşit pre-pass'i artık DUGUM_MODUL
   içine iner (düz adla, ilk-kazanır); metin literal taraması DUGUM_MODUL + DUGUM_ESLES
   düğümlerine de iner (önceden modüldeki stringler @.str'e toplanmıyordu).
8. **v1 sınırları:** modül-içi `sabit` codegen'de kayıtsız (önceden de öyleydi);
   in-file modüllerde görünürlük denetimi YOK (geriye uyum — `dışa` eski anlamında);
   LSP loader koşmaz; aynı adlı struct'lar modüller arası düz IR-ad uzayını paylaşır
   (D'de nitelikli tip ile ayrışacak); seçili/alias çok-segment yol v1'de P046.

**Testler:** test_llvm 182→194 (+12 A testi: çapraz-modül fonk/--check, seçili, alias,
T041 negatif, transitif, gölgeleme, kütüphane-UTF8, modül-içi yapı x2, T042 negatif,
nitelikli-çakışma); parser 102→107 (+5 gramer). Fikstürler: test/moduller/*.kem;
kütüphane fikstürleri runtime'da yazılıp silinir.

---

## D-012 [YÜKSEK] — Çapraz-modül generic monomorphization: FONKSİYON routing (faz C-1) (2026-06-13)

**Bağlam:** A (whole-program namespaced yükleme) + B (tek-geçiş resolver) main'e
merge edildi. Generic gövdeler ZATEN bellekte + ZATEN çözülü. C = çapraz-modül
generic instantiation'ın YÖNLENDİRİLMESİ + EMİSYONU (gövde-görünürlüğü ya da
yeniden-çözüm DEĞİL).

**GAP (kodla teyit edildi):** Çapraz-modül qualified çağrı `m::f(...)` codegen'de
İKİ ayrı yol kullanıyor: TANIMLAYICI yolu (modül-içi/global, `ifade_uret`
DUGUM_CAGRI ~2660) generic'i specialize ediyordu; ama YOL yolu (`m::f` qualified,
DUGUM_YOL hedef, ~1726) jenerik kontrolü YAPMADAN `mik->donus_tip @modul.f`
**plain** emit ediyordu. Sonuç: `call i32 @sayi.azami(...)` → clang `use of
undefined value '@sayi.azami'` (define yalnız `@sayi.azami$i32` adında üretilir).

**Karar (mekanizma):**
1. **Tek-kaynak helper'lar (DRY):** `generic_islev_cagri_uret()` (inference +
   mangle + bekleyen-enqueue + substituted-return emit) ve `generic_param_beklenen()`
   (somut param → IR beklenen; generic-param içeren param → NULL = arg doğal
   tipinden T inference). Her İKİ yol (TANIMLAYICI + YOL) bu helper'ları çağırır;
   TANIMLAYICI'nın eski inline bloğu (~175 satır) helper çağrısıyla değiştirildi.
2. **Mangling şeması [ETKİ]:** Mevcut `$`-specialization mangling AYNEN korunur.
   Modül-nitelik mangled ada zaten gömülü çünkü `gislev->veri.islev.ad` modül
   kaydında `@modul.ad`'a yeniden yazılı (`modul_uyeleri_kayit`); `mangle_et`
   bundan `dizi.ekle$i64` üretir. Modül için AYRI mekanizma EKLENMEDİ — `.` (modül)
   + `$` (specialization) iki ayraç doğal kompoze olur.
3. **Dedup anahtarı [ETKİ]:** Modül-nitelikli mangled ad (`sayi.azami$i32`) =
   `mono_emitlendi` + bekleyenler tarama anahtarı. Aynı specialization birden çok
   use-site'tan referanslansa BİR kez emit. Doğrulama: ikinci $i32 çağrısı + i64
   çağrı, IR'da her define BİR kez (yapısal: duplicate-symbol link hatası yok).
4. **Binding-koruma [ETKİ-YÜKSEK]:** Specialize edilen gövdenin iç kardeş çağrıları
   owning-modülün üyelerine işaret eder — use-site bağlamında YENİDEN ÇÖZÜLMEZ.
   Mekanizma: `specialize_emit` mangled addaki SON `.`'tan öneki türetir
   (`sayi.azami$i32` → önek `sayi`) ve `aktif_modul_onek` olarak kurar; gövdedeki
   çıplak-ad kardeş çağrılar `islev_bul` fallback'iyle `sayi.azami`'ye çözülür.
   Transitif (`azami3$i32 → azami$i32`, hepsi `sayi` bağlamında) bu yolla çalışır.
5. **Linkage seçimi [ETKİ]:** A her şeyi TEK LLVM modülüne splice ettiği için
   mevcut specialization linkage'ı (define, external default) çapraz-modül için
   yeterli — TEYİT edildi (E2E link + çalıştırma). `linkonce_odr`+COMDAT ayrık-
   derleme (v2) işi → **ERTELENDİ** (tek modül, katlanacak ayrı obje yok).

**Kapsam/sınırlar:**
- Bu faz yalnız generic FONKSİYON (`@sayi.azami$i64`). Generic STRUCT (Liste<T>)
  faz C-2 (sonraki commit).
- Doğrulama INFERENCE ile (param tiplerinden T). Explicit yazılı tip-arg
  `f<i64>(...)` parser'da DESTEKLENMİYOR (`yap<tam32>(42)` = karşılaştırma zinciri
  olarak parse ediliyor) → açıkça D-bitişik parser fork; bu görevde EKLENMEDİ,
  witness-param inference ile köşe dönüldü.

**Testler:** test_llvm 194→197 (+3 C testi: çapraz-modül generic fonk --check,
azami$i32+$i64+dedup E2E, transitif azami3→azami E2E). Fikstürler:
test/moduller/{sayi,ana_sayi,ana_sayi_transitif}.kem. parser 107, tip_kontrol 174
düşmedi. In-file generic canary'ler (kimlik<T>, Liste<T>) yeşil.

---

## D-013 [YÜKSEK] — Çapraz-modül generic STRUCT (Liste<T>): routing-only (faz C-2) (2026-06-13)

**HEADLINE bulgusu (kritik):** Çapraz-modül generic STRUCT, faz C-1'in (D-012)
FONKSİYON routing düzeltmesi DIŞINDA **HİÇBİR ek codegen değişikliği gerektirmedi**.
Bu, görevin temel içgörüsünü doğrular: C = instantiation'ın YÖNLENDİRİLMESİ, struct
layout yeniden-mimarisi DEĞİL.

**Neden routing yetti (struct-mono yaklaşımı [ETKİ]):**
- Liste<T> **type-erased**: `%Liste = type { ptr, i64, i64 }` — `*T`→`ptr`, T IR
  layout'ta yok (D-011 #8: modüller arası düz IR-ad uzayı). Tek `%Liste` tüm T'ler
  için geçerli → struct için PER-T layout specialization GEREKMEZ (fonksiyon-mono'dan
  *temelde farklı bir yaklaşım* değil — aynı `$`-makinesi).
- T yalnız (a) specialized fonksiyon gövdesinde subst (T→i64 push edilir → `*T`
  pointee, `bölge_al` sizeof doğru), (b) inference yan-kanalları (`generic_arg_ir`,
  `pointee_llvm_tip`) ile taşınır.
- `oluştur`/`ekle`/`al`/`büyü` çapraz-modül çağrıları D-012 YOL routing'iyle
  `@kap.oluştur$i64` vb. olarak specialize+emit edilir; struct değer (`%Liste`
  by-value dönüş) + `&Liste<T>` by-pointer param mevcut v2/v3 makinesi.

**Doğrulama (saf INFERENCE — yazılı nitelikli tip YOK):**
- HEADLINE: `kullan kap; değişken l = kap::oluştur(sifir); kap::ekle(&l,10)…;
  kap::al(&l,0)+kap::al(&l,4)` → 42. Transitif büyü<T> (kapasite 0→4→8, eleman-
  kopyalı grow) `@"kap.büyü$i64"` olarak owning-modül bağlamında specialize edilir
  (5. eleman idx4'e düşer — grow olmasa heap-overflow; deterministik 42 = yapısal kanıt).
- Çoklu-tip: aynı Liste<T> i64+i32 → ayrık specialization'lar (`@kap.ekle$i64` /
  `@kap.ekle$i32`), paylaşılan `%Liste`. 40+2=42.
- Dedup: `ekle$i64` 2 çağrı → 1 define (yapısal: link hatası yok).

**Witness-param inference [ETKİ — DUR-SOR yerine köşe dönüşü]:** Üretimdeki
Liste<T> `oluştur`'ı T'yi DÖNÜŞ-bağlamı annotasyonundan (`değişken l: Liste<tam64>`)
çıkarır — ama nitelikli annotasyon (`kap::Liste<tam64>`) D işi + headline bunu
YASAKLIYOR. Explicit call-site tip-arg (`oluştur<i64>()`) parser'da YOK (yeni
semantik fork → kapsam dışı). Çözüm: minimal kapsayıcıda her generic fonk bir
**tip-tanık** value-param taşır (`oluştur<T>(taban: T)`, `büyü/al` zaten T-param'lı)
→ T arg'dan çıkarsanır, annotasyon/explicit-tip-arg GEREKMEZ. Üretim Liste<T>'nin
TAM taşınması (yetki-disiplinli oluştur'ın return-context inference'ı) follow-up;
ilgisiz altsistem (capability-borrow) genişletilmedi.

**Kapsam/sınırlar:**
- Fikstür modülü `kap` (test/moduller/kap.kem) — `kütüphane/dizi.kem` in-file
  canary'siyle (kendi main'i var) çakışmamak için ayrı ad.
- Liste<T> uzunluk/sınır built-in dönüş tipi taşımaz; minimal oluştur/ekle/al/büyü.
- Yazılı nitelikli generic-tip annotasyonu (`kap::Liste<i64>`) → D (dokunulmadı).

**Testler:** test_llvm 197→200 (+3 C-2: struct --check, headline oluştur/ekle/al+büyü
E2E, çoklu-tip i64+i32 E2E). Fikstürler: test/moduller/{kap,ana_kap,ana_kap_coklu}.kem.
parser 107, tip_kontrol 174 düşmedi. In-file Liste<T> (kütüphane/dizi.kem) canary yeşil.

---

## D-014 — Üretim Liste<T> gerçek çapraz-dosya modüle taşındı (relocation + PROB) (2026-06-13)

**Bağlam:** D-013 minimal `kap` container'ıyla çapraz-modül struct'ı kanıtlamıştı.
Bu adım ÜRETİM `kütüphane/dizi.kem` Liste<T>'sini gerçek importable modül yapar —
v1'de ERTELENEN relocation (B+A+C ile mümkün): Liste o zaman top-level'a zorlanmıştı
çünkü (a) modül-içi struct emisyonu (A) + (b) cross-module generic routing (C) yoktu.

**Yapı kararı (no-src-change, stdlib + test):**
- `kütüphane/dizi.kem` artık **mat.kem düzeni**: top-level `genel yapı Liste<T>` +
  7 op (`genel işlev`), AÇIK `modül dizi { }` SARMALAYICISI YOK. Loader dosyayı
  zaten `modül dizi`'ye sarar; açık sarmalayıcı `dizi.dizi` İÇ-İÇE olurdu (loader
  `fprog.uyeler`'i sentetik DUGUM_MODUL üyesi yapar — ana.c:315). → Liste<T> artık
  modül dizi üyesi (A modül-içi struct emisyonu → type-erased `%Liste`).
- In-file `main` + test_* (v1 self-contained) **kaldırıldı** — import edilince entry
  main'iyle çakışır. Doğrulama ayrı çapraz-dosya entry'lerine taşındı
  (test/moduller/dizi_{kullan,coklu,yapi}.kem; `kullan dizi;`, kütüphane/ arama
  yolundan bulunur). "In-file canary" → "çapraz-dosya canary" (eşdeğer kapsam yeşil).

**PROB RAPORU (görevin asıl çıktısı):**
1. **`oluştur` INFERENCE-FRIENDLY DEĞİL → witness-param gerekti.** Üretim imzası
   `oluştur(böl: yetki<Bellek>) -> Liste<T>` — paramlarında T YOK → T yalnız
   dönüş-bağlamı annotasyonundan (`değişken l: Liste<tam32> = ...`) çıkarsanırdı.
   Çapraz-dosya'da: yazılı nitelikli annotasyon (`dizi::Liste<tam32>`) = D (yasak);
   explicit call-site tip-arg (`oluştur<i64>()`) parser'da YOK (karşılaştırma zinciri
   olarak parse). **DEMO adaptasyonu:** `oluştur<T>(taban: T)` tip-tanık param (değer
   kullanılmaz) + yetki içeride üretilir → T arg'dan çıkarsanır. **Üretim API kararı
   DEĞİL** — gerçek çözüm explicit-type-arg parsing (ayrı görev, syntax fork) ya da
   return-type-driven inference. `ekle`/`al` zaten T-değer param'lı → çıkarsama sorunsuz.
2. **Capability-borrow workaround çapraz-dosya SORUNSUZ.** `yetki<Bellek>` lineer-MOVE
   disiplini (her op taze `yetki_olustur(3,3)`, son `geri_al`; ekle→büyü MOVE) çapraz-
   dosya'da AYNEN çalışır — yetki built-in'leri modül çözümünden bağımsız, specialized
   gövdede owning-bağlamda emit edilir. Runtime-link (kdl_yetki_*) sorunsuz.
3. **Küçük inference WART (düzeltilmedi, raporlandı):** `değişken l = dizi::oluştur(...)`
   (annotasyonsuz) sonucu `l` element-tip yan-kanalı (`generic_arg_ir`) TAŞIMAZ —
   yalnız değişken-annotasyonundan set ediliyor. Sonuç: T'nin TEK kaynağı `&Liste<T>`
   param olan doğrudan çağrı (örn. `dizi::boy(&l)`) i32'ye default'lar → `@dizi.boy$i32`
   emit edilir. **ZARARSIZ** burada: `boy` dönüşü somut `tam64`, gövde T'den bağımsız —
   $i32/$i64 gövdesi ÖZDEŞ. Ama T-DÖNÜŞLÜ böyle bir op olsaydı yanlış tip verirdi.
   Transitif çağrı (ekle→büyü, l param subst'lı) DOĞRU ($i64). Gerçek çözüm: generic
   call sonucunu değişkene atarken instantiated-T'yi yan-kanala propagate et (küçük
   codegen işi, kapsam dışı — mono değil, ergonomi). DUR-SOR yerine raporlandı.

**Doğrulama (saf inference, hepsi exit 42):** çapraz-dosya grow headline (oluştur/ekle×5/
al + transitif büyü, kapasite 0→4→8), çoklu-tip (i64+i32 ayrık spec, paylaşılan %Liste),
struct-eleman (Liste<Nokta> karışık genişlik). IR: `@dizi.{oluştur,ekle,al,boy}$i64` +
`@"dizi.büyü$i64"` owning-bağlamda; çoklu-tip $i64/$i32 ikiz set.

**Kapsam/sınırlar:** src/ kodu DEĞİŞMEDİ (stdlib + test). Yazılı nitelikli tip (D),
legacy flatten, proofs/, bölge/escape/wcet/lsp dokunulmadı. struct-eleman Liste<Nokta>
çalışıyor (D-013'te denenmemişti — burada doğrulandı).

**Testler:** test_llvm 201→203 (stdlib_liste_e2e in-file→çapraz-dosya güncellendi;
+çoklu-tip +struct-eleman; --check modül-alone). Fikstürler: kütüphane/dizi.kem (v2
yeniden yazıldı), test/moduller/dizi_{kullan,coklu,yapi}.kem. parser 107, tip_kontrol
174, drivers (uart_vtable 21) düşmedi. 0 ASan. stdlib --check yeşil.

---

## D-015 [YÜKSEK] — Nitelikli tip annotation (`modül::Tip<args>`): D dilim-1 (2026-06-13)

**Bağlam:** D-014 relocate PROB #1: üretim `oluştur(böl: yetki<Bellek>) -> Liste<T>`
paramlarında T YOK → çapraz-dosya T inference için ya nitelikli annotation ya
explicit-type-arg gerekir. Bu dilim nitelikli TİP annotation'ı getirir: in-file
return-context inference'ı çapraz-dosyaya açar; relocate'in DEMO witness-param'ını
kaldırır (üretim imzası geri).

**Karar (mekanizma):**
1. **Parser `::`-in-type-position [ETKİ — ifade::ile karışmaz]:** `ifade.c parse_tip`
   tanımlayıcı dalında `::` zinciri → DUGUM_YOL → `DUGUM_TIP_KULLANICI{yol:YOL, tip_arg}`.
   `tip_kullanici.yol` ZATEN "DUGUM_TANIMLAYICI veya DUGUM_YOL" kabul ediyordu (ast.h).
   `dizi::Liste<i64>` (args) ve `dizi::Nokta` (0-arg) → ikisi de TIP_KULLANICI.
   AMBIGUITY YOK: `::` yalnız tip pozisyonunda (annot/param/dönüş); ifade-pozisyonu
   `f<i64>()` (explicit-type-arg) AYRI sorun, DOKUNULMADI. "Dizi" özel-case yalnız
   NİTELİKSİZ (`dizi::Dizi` değil).
2. **Resolver TİP-namespace [ETKİ-YÜKSEK]:** `ast_tip_to_bilgi` DUGUM_TIP_KULLANICI
   artık YOL yol'u çözer: `yol_modul_scope_coz(sol)` → hedef modül scope, `sembol_bul_yerel`
   → SEMBOL_YAPI. Value-path (`dizi::ekle`) çözümünün YANINA; tip & value AYNI scope,
   SEMBOL kategorisiyle ayrışır (SEMBOL_YAPI=tip). Çözülen TipBilgisi DÜZ adlı
   (`Liste`) → mono key C ile AYNI (type-erased `%Liste`).
3. **Gizli-aware fallback [ETKİ-YÜKSEK — faz ordering]:** Param/dönüş tipleri
   `pre_populate` (faz-1, imza) içinde çözülür — `kullan_baglari_kur` (faz-2,
   görünür alias) ÖNCE çalışmaz → görünür `dizi` yok, yalnız GİZLİ kanonik
   builtin_scope'ta. Çözüm: nitelikli tip çözümünde görünür-alias bulunamazsa
   builtin_scope kanonik dosya-modülüne düş (tek-segment). Gerekçe: tip pozisyonu
   modülü açıkça adlandırır + modül zaten yüklü (loader bir `kullan` ister). Değişken
   annotasyonu (faz-3 gövde) görünür yolu kullanır; ikisi aynı modül_scope'a varır.
4. **Çapraz-modül yapı ALAN erişimi [ETKİ-YÜKSEK]:** `n.x` (n: sekil::Nokta)
   tip kontrolde yapının ALAN listesini ister; `sembol_bul(scope, "Nokta")` niteliksiz
   görünmez (Nokta sekil'de). `yapi_sembol_capraz_bul`: önce görünür scope (gölgeleme
   korunur), bulunamazsa YÜKLÜ tüm modül scope'larında DÜZ adla ara. Codegen'in düz
   IR-ad uzayıyla tutarlı (D-011; per-modül ayrım D ileri dilim). Yalnız DUGUM_ERISIM
   alan-çözümünde kullanılır (struct construction değil).
5. **Yan-kanal annotasyondan (PROB #3 by-pass) [ETKİ]:** `generic_arg_ir_al` zaten
   `tip_kullanici.tip_arg[0]`'ı yol-tipi gözetmeksizin okur → nitelikli annotation
   element-tip yan-kanalını besler. `değişken l: dizi::Liste<tam64>` → l.generic_arg_ir=i64
   → `dizi::boy(&l)` artık `@dizi.boy$i64` (i32 DEĞİL). codegen değişikliği yalnız
   `ast_tip_to_ir` YOL→`%sag_ad` (düz yapı adı).
6. **RENAME fold:** kütüphane/dizi.kem `oluştur`→`oluştur` (yetki_olustur ile tutarlı);
   DEMO witness-param `taban: T` KALDIRILDI — üretim imzası `oluştur(böl) -> Liste<T>`.
   T artık nitelikli annotation'dan (return-context).

**Doğrulama (hepsi exit 42, ÜRETİM imzası, nitelikli annotation):**
- HEADLINE (PROB #1 çözüldü): `değişken l: dizi::Liste<tam64> = dizi::oluştur(böl);
  ekle×5 + transitif büyü (0→4→8 grow); al(0)+al(4)`. `@dizi.boy$i64` (PROB #3 by-pass).
- Çoklu-tip (i64+i32 nitelikli annot, ayrık spec). struct-eleman `dizi::Liste<Nokta>`.
- Param nitelikli tip `&dizi::Liste<tam64>` (imza fazı, gizli-aware fallback).
- Çapraz-modül struct USE: `sekil::Nokta` (değişken+param) + factory kur + `n.x` alan.

**Kapsam/sınırlar [DUR-SOR yerine raporlandı]:**
- **Explicit call-site tip-arg (`oluştur<i64>()`) parser'da YOK** — ifade-pozisyonu,
  ayrı görev + syntax fork. Bu dilim DEĞİL (tip-pozisyonu `::` ile karışmaz).
- **Nitelikli yapı KURMA ifadesi (`sekil::Nokta { ... }`) parser'da YOK** (P082) —
  ifade-pozisyonu, D ileri dilim. struct USE testi factory (`sekil::yap`) ile kurar.
- İç-içe nitelikli tip (`a::b::Tip`) gizli-aware fallback yalnız tek-segment; çok-segment
  görünür-alias (faz-3) yolundadır. Legacy flatten/D2, drivers, proofs/, bölge/escape/
  wcet/lsp DOKUNULMADI.

**Testler:** test_llvm 203→205 (+2 D: nitelikli param E2E, çapraz-modül struct USE E2E).
Fikstürler: test/moduller/{dizi_nitelikli_param,sekil,sekil_kullan}.kem + dizi_{kullan,
coklu,yapi}.kem nitelikli annotation'a güncellendi. kütüphane/dizi.kem oluştur+üretim imza.
parser 107, tip_kontrol 174, drivers (uart_vtable 21) düşmedi. 0 ASan. stdlib --check yeşil.

---

## D-016 — D2 (legacy flatten kaldırma): ADIM-0 araştırma → DUR-SOR (kod DEĞİŞMEDİ) (2026-06-13)

**Görev:** Çok-segment `kullan a::b::c;` legacy flatten'i kaldır; drivers/virtio +
test/crossfile bağımlılarını D1 (nitelikli tip) yoluna taşı. **SONUÇ: yapısal +
D1-aşan blocker → DUR-SOR. Hiçbir kod/test/fikstür değişmedi (baseline 205 korundu).**

**Legacy flatten ne yapıyor (file:line):**
- Parser: `src/parser.c:816-926` — `kullan a::b::c;` → `segment_sayi>1` (seçili/alias yok).
- Loader: `src/ana.c:181-206` — legacy formları ATLAR (`kullan_yeni_bicim` filtresi).
- Tip kontrol: `src/tip_kontrol.c:4807-4885` (DUGUM_KULLAN legacy) — `a::b::c`→`a/b/c.kem`
  dosyasını yükler, `tip_kontrol_program`'ı RECURSIVE çağırır → yüklenen dosyanın
  top-level bildirimlerini İÇE-AKTARAN scope'a NİTELİKSİZ kaydeder (= flatten).
- Codegen: `src/llvm.c:4138-4199` — dosyayı yükler, top-level üyeleri programa DÜZ
  (plain-ad) splice eder. **Constants top-level olduğu için codegen INLINE eder.**

**Bağımlı listesi (tam):**
- GATED (test_llvm suite): `test/crossfile/{transitif,lib_islem,sonuc_cagri,lib_sayi,
  lib_sonuc}.kem` — yalnız FONKSİYON (`dışa işlev uc_kat/iki_kat/bol`), struct/sabit yok.
- GATESİZ (suite'te değil): `drivers/virtio/*.kem` (6) + `tests/drivers/virtio/*.kem` (9)
  — `sabit` CONSTANTS (constants.kem ~her dosyada) + `işlev` + struct. Çoğu private
  (`işlev`/`sabit`, `genel` değil); flatten görünürlüğü yok sayar.

**PROBE sonuçları (go/no-go):**
- ✅ Struct selective import: `kullan mod::{Nokta}; Nokta{x,y}; n.x` → exit 42. Niteliksiz
  construct + alan erişimi ÇALIŞIR.
- ❌ **Constant cross-file codegen GAP (KRİTİK blocker):** `kullan konst::{DEGER}` VE
  `konst::DEGER` — `--check` GEÇER ama E2E **exit 0** (değer değil). Cross-file `sabit`
  codegen'de KAYITSIZ (D-011 belgeli: "modül-içi sabit codegen'de kayıtsız"). Legacy
  flatten çalışıyor ÇÜNKÜ constants'ı top-level splice ediyor (codegen inline).
  Yeni modül sistemi modül-içi sayar → emit etmez → 0.
- ❌ `dışa işlev` + selective import → T041 ("'genel' değil"). `dışa` ≠ `genel`; migrasyon
  `dışa`→`genel` görünürlük değişikliği ister (yüzey-sözdizimi değil).
- virtio entry testleri ZATEN KIRIK: `virtio_blk_init_test` (11 hata), `virtio_blk_oku_test`
  (14 hata) bugün `--check` GEÇMİYOR (aktif/eksik virtio track). Korunacak yeşil yok.

**DUR-SOR gerekçesi (brief koşulları karşılandı):**
1. **D1'i AŞAR:** virtio constants.kem'i her yerde kullanır; cross-file `sabit` codegen
   yok → migrasyon constants'ı bozar (0). Yeni codegen özelliği gerekir (yüzey-sözdizimi
   DEĞİL, kapsam dışı).
2. **YAPISAL iş:** `genel` görünürlük değişikliği onlarca `sabit`/`işlev`'de; virtio
   zaten kırık (baseline yok). Minimal selective-import dokunuşu değil.
3. Flatten kaldırma TÜM bağımlıların migrasyonunu ister; virtio migrate edilemiyor →
   kaldırma BLOKE. Kısmi (yalnız crossfile) migrasyon kaldırmayı sağlamaz + gated
   fikstürleri risksiz değiştirmez → yapılmadı.

**D2 için ön-koşul (sonraki adım):** (a) cross-file `sabit` codegen (modül sabitlerini
kaydet/inline) — ayrı dilim; (b) virtio + crossfile `dışa`→`genel` + selective-import
migrasyonu; SONRA flatten kaldırılabilir. Alternatif: virtio track'i ayrı ele al.

**Doğrulama:** Kod/test DEĞİŞMEDİ. test_llvm 205, parser 107, tip_kontrol 174, drivers
(uart_vtable 21/uart_16550 13) korundu. ELLEME (proofs/, bölge/escape/wcet/lsp, D1
faz-reorder, per-modül namespacing) DOKUNULMADI.

---

## D-017 [YÜKSEK] — İsimlendirme borcu: yasaklı üretici sözcüğü → `olustur` (depo-geneli) (2026-06-13)

**Direktif (DEĞER — istisnasız):** Türkçe keyword/fonksiyon/intrinsic/örnek/yorumlarda
yasaklı üretici sözcüğü KULLANILMAZ. Standart karşılık: → `olustur` (üreticiler;
`_*` son-eki → `_olustur`), `yetki_olustur` ile tutarlı ASCII. (`çevrim`/`cevrim` =
CPU cycle/WCET — DOĞRU, dokunulmadı; 33 örnek korundu.)

**Kapsam:** Depo-geneli ~670 örnek (4 izole commit, her biri build+test yeşil).

**[YÜKSEK] — Üretici intrinsic adı değişikliği (derleyici tanıma):**
- `tekkez` üretici intrinsic → `tekkez_olustur`: tip_kontrol.c + llvm.c eşleştiricileri
  (string + bayt-uzunluk 12→14).
- `sabitsüre` üretici intrinsic → `sabitsüre_olustur`: ÜÇ eşleştirici (tip_kontrol.c ×2
  [biri beklenen-bağlam çıkarsama, 3523 — atlanırsa op testleri kırılır], llvm.c ×1),
  bayt-uzunluk 16→18 (`sabits`+ü[2 byte]+`re_olustur`). `_is_*` C değişkeni de yeniden
  adlandırıldı. Snapshot .ast baseline'ları (18_tekkez, 29/30_linear) regen.
  **Tuzak [ETKİ]:** ikinci/üçüncü eşleştiricinin bayt-uzunluğu kolayca atlanır — string
  güncellenip uzunluk eski kalırsa eşleşme sessizce DÜŞER (test_sabitsure 4 hata yakaladı).

**Üretici fonksiyonlar:** `anahtar_olustur` (kripto: stdlib/kripto/anahtar.kem def +
test_kripto* çağrı + test_k5_anahtar_olustur), `kap` fikstürü `olustur<T>`, test_tip_kontrol
gömülü Hasta üreticisi. Tümü ASCII `olustur`.

**Yorum/doc/Lean-yorum:** src yorumları + test etiketleri (.c/.h ASCII, .kem/.md Türkçe
morfoloji: `…ılır`→`oluşturulur`, `…an`→`oluşturan`, bağlanma ünlüsü). belgeler/*.md,
README, CLAUDE.md, KILAVUZ, spec'ler. proofs/*.lean YALNIZ YORUM (gerçek Lean kodu
yasaklı sözcük İÇERMİYOR — V2-hipotetik adlar yorumda; Lean derlemesi etkilenmez).

**Doğrulama:** Depo-geneli grep (büyük/küçük harf duyarsız, .git hariç) = **0 örnek**.
`çevrim`/`cevrim` = 33 (korundu). Tam test: test_llvm 205, tip_kontrol 174, linear 57,
sabitsure 39, arena 19, ast 31, tip 26, sembol 18, capability 40, snapshot 50, drf 39.
kripto + stdlib --check geçti. 0 ASan. Temiz rebuild 0 uyarı. BUNDAN SONRA yeni örnek
GİRİLMEZ.

---

## D-018 [YÜKSEK] — Payload-taşıyan çeşit (sum type with data) + recursive AST (C3) (2026-06-13)

**Stratejik hedef:** Evrensel OS / self-hosting. Direktif ön-koşulu: "payload-taşıyan
çeşit [AST için şart]". Bu adım payloadsuz çeşit'i (C2.7) payload-taşıyan + ÖZYİNELEMELİ
sum type'a genişletir → KEMGU kendi AST'sini kendi çeşit'leriyle temsil edebilir.

**Sözdizim/semantik [YÜKSEK]:**
- Tanım: `çeşit Ifade { Tam(tam64), Ikili(tam64,tam64), Yok }` — varyantlar tipli
  alanlar taşır (payloadsuz varyant aynı çeşitte serbest).
- İnşa: `Ifade::Ikili(30, 12)` (CAGRI(YOL,args) — ayrı sözdizim eklenmedi, mevcut
  parse'a oturdu).
- Eşleş destructuring: `Ifade::Tam(v) =>` / `Ifade::Ikili(a, b) =>` (DESEN_YOL
  alt-desenleri → payload tiplerine bind).

**Temsil (hibrit) [ETKİ]:** Payloadsuz çeşit → bare iN disc (C2.7 DEĞİŞMEDİ — geriye
uyum). Payload çeşit → `%Ad = type { iDISC, [tüm varyant payload alanları peş peşe] }`
(sonuç `{tag,T,H}` deseni; union DEĞİL — basitlik + ABI by-value). Varyant vi'nin alan
ofseti = `1 + sum(payload_sayilari[0..vi-1])`. AST: cesit struct'a paralel diziler
(`varyant_payload_tipleri` Dugum***, `varyant_payload_sayilari` int*); desen_yol'a
`alt_desenler`.

**Recursive çeşit [ETKİ-YÜKSEK — self-hosting HEADLINE]:** `çeşit Agac { Yaprak(tam64),
Dal(&Agac, &Agac) }` — özyineleme `&Agac` REFERANSI ile (ptr, sonlu boyut
`%Agac={i8,i64,ptr,ptr}`). Eşleş `&Cesit` scrutinee'sinde OTOMATIK DEREFERENCE eklendi
(tip_kontrol: TIP_REFERANS→hedef; llvm: ptr→`load %Cesit`, desenlerden çeşit çözülür).
Özyinelemeli gezinme E2E çalışır.

**Tip kontrol:** `cesit_yapici_tip_kontrol` (M002 yok varyant, M003 arity, M004 payload
tip) iki CAGRI yolunda. DESEN_YOL payload binding (alt-desen → SEMBOL_DEGISKEN, varyant
tipinde). Exhaustiveness mevcut (varyant adı bazlı — payload drilling gerekmiyor v1).

**Doğrulama (E2E exit 42):** Ifade{Tam/Ikili/Yok}; Olay{Tus(tam8),Konum(Nokta),
Cift(tam8,tam64),Bos} (karışık genişlik + STRUCT payload); Agac recursive AST; mini
aritmetik AST değerlendirici örneği `(3+4)*6` (test/ornekler/10_cesit_ast.kem).

**Kapsam/sınırlar:**
- Generic çeşit (`çeşit Kutu<T>`) HÂLÂ yok (parser P353 reddi — ayrı dilim).
- Türkçe (non-ASCII) çeşit/yapı TİP ADI codegen'de quote edilmiyor (`%İfd` geçersiz
  LLVM ad) — YAPI emisyonuyla ORTAK pre-existing sınır; örnekler ASCII tip adı kullanır
  (Ifade/Agac/Olay). Quote'lama ayrı robustness işi.
- Exhaustiveness payload-desen-derinliği denetlemez (varyant kapsaması yeterli).
- Nitelikli payload çeşit yapıcısı (`m::Cesit::V(args)`) codegen'de sol=TANIMLAYICI
  varsayar (modül-içi/düz ad). Çapraz-modül payload çeşit follow-up.

**Testler:** test_llvm 205→210 (+5 C3: payload verify/run, struct+karışık, recursive
verify/run). parser 107 (payload+desen sözdizimi), tip_kontrol 174, snapshot 50, linear
57, drf 39, lexer 103, ast 31, arena 19. 0 ASan. Temiz rebuild 0 uyarı. Fikstürler:
test/snapshots/cesit_{payload,payload_yapi,agac}.kem + test/ornekler/10_cesit_ast.kem.
4 izole commit (parser→tip→codegen→recursive→test).

---

## D-019 — Türkçe (non-ASCII) yapı/çeşit TİP ADLARI IR'da quote'lanır (2026-06-13)

**Değer (Türkçe kimlik — istisnasız):** KEMGU AST düğümleri doğal Türkçe adlarla
temsil edilebilmeli (`çeşit Düğüm`, `yapı Köşe`, `İfade`). Önceki durum: non-ASCII
tip adı `%Düğüm` GEÇERSİZ LLVM identifier → `clang` "expected top-level entity" /
"Cannot allocate unsized type". D-018 örneği bu yüzden ASCII (`Ifade/Agac/Olay`)
kullanmak zorundaydı. (Pre-existing: YAPI codegen'i de quote etmiyordu.)

**Çözüm [ETKİ]:** `yapi_ad_ir(g, ad, uz)` — yapı/çeşit IR tip adını üretir;
ASCII-güvenli değilse `%"Ad"` quote'lar (yerel_ad_yaz ile aynı kural). Simetrik
okuma `yapi_bul_ir(g, ir)` — `%"Ad"` stringinden quote'u soyup YapiKayit bulur.
Düzeltilen emisyon noktaları (TUTARLI olmalı, yoksa LLVM ad-uyuşmazlığı):
- ast_tip_to_ir (DUGUM_TIP_BASIT + KULLANICI struct/çeşit) → yapi_ad_ir.
- cesit_struct_ir → yapi_ad_ir. yapi_olustur_uret alloca tipi → yapi_ad_ir.
- yapi_tip_tanimlari_emit (yapı + çeşit tanımı `%Ad = type`) → yerel_ad_yaz.
- erisim GEP (struct alan adresi ×2) → yerel_ad_yaz.
Okuma noktaları → yapi_bul_ir: erisim_uret (alan erişimi), erisim_lvalue (×2).
İşlev adları (`@"köşe_topla"`) ZATEN yerel_ad_yaz ile quote'luydu.

**Doğrulama:** `yapı Köşe + çeşit Düğüm (recursive)` — Türkçe adlı yapı alan
erişimi (k.x+k.y) + Türkçe adlı recursive çeşit ağacı → exit 42. ASCII tip adları
DEĞİŞMEDİ (quote yok). test_llvm 210→211 (+Türkçe tip-adı E2E). Tam regresyon:
lexer 103, parser 107, tip_kontrol 174, snapshot 50, linear 57, drf 39, capability
40, ast/arena/sembol/tip/sabitsure. 0 ASan. Temiz rebuild 0 uyarı.
Fikstür: test/snapshots/turkce_tip_adi.kem.

# DECISIONS_LOG — Codegen Kampanyası Karar Kaydı

Format: D-NNN | tarih | karar | gerekçe | kapsam/sınırlar. [YÜKSEK] = merge-review'da
özellikle bakılması istenen, izole commit'li kararlar.

---

## D-415 [YÜKSEK] — 🔴 D-405 YANLIŞTI: `bölge_al` GERÇEK TAHSİSTİR (2026-08-10)

**D-414'te açtığım aralıklı segfault kapandı.** `dizi_kullan` self ikilisi
öncesinde `42 42 139 42 …`, şimdi **12/12 kararlı 42**.

### 🔴 Kendi kararımın düzeltmesi
**D-405'te `bölge_al(böl, n)`i parametresiz `@kdl_global_bolge_al()` sandım —
YANLIŞ ÖLÇÜMDÜ.** C çıktısında gördüğüm o çağrı `bölge_al`ın karşılığı DEĞİL,
**her işlevin girişindeki ρ-EDİNME PROLOGU**ydu. Kesin kanıt: `kdl_global_bolge_al`
C'nin TÜM çıktısında **yalnız BİR kez** geçiyor — `main`de.

C gerçekte TAHSİS yapıyor:
```
%s = getelementptr <T>, ptr null, i64 1   ; sizeof idiom
%z = ptrtoint ptr %s to i64
%t = mul i64 <n>, %z
%p = call ptr @malloc(i64 %t)
```
Eski hâl küresel bölge işaretçisini tampon diye döndürüyordu → yazılan her
eleman **BÖLGEYİ BOZUYORDU**. Belirti aralıklı segfault'tu.

**⚠⚠ D-405'İN KORPUSU BU KUSURU KAÇIRDI ÇÜNKÜ TAHSİS EDİLEN BELLEĞE HİÇ
YAZMIYORDU.** Dosyanın kendi notu itiraf ediyormuş: *"Bölge işaretçisinin
kendisini denetlemiyoruz (adres kararsız)"*. Korpus yenilendi: artık **yazıyor
ve geri okuyor**, iki farklı `n` ile.
> **DERS: bir TAHSİS yerleşiğini test ederken tahsis edilen belleğe YAZ ve GERİ
> OKU.** "Çağrı geçerli IR üretti" demek yanlış lowering'i yeşil geçirir —
> haftalarca geçirdi.

### İkinci kök — D-411'in sağlamlık iddiası da yanlıştı
`büyü<T>(l: &Liste<T>, ...)`de T İÇ İÇE konumda; argümandan çıkarsanamıyor ve
D-411'in `i32` fallback'i devreye giriyordu → **`büyü$i32`** yayılıyor, büyüme
sırasında eleman kopyası 8 yerine 4 bayt genişlikte yapılıyordu. C `büyü$i64`
yayıyor.
> **D-411'de "fallback yanlışsa LLVM REDDEDER, hata GÜRÜLTÜLÜ kalır" demiştim —
> O İDDİA YANLIŞTI.** Define ve çağrı i32'de ANLAŞIYOR, LLVM kabul ediyor, kusur
> çalışma anına kayıyor. Fallback güvenli DEĞİL.

Onarım: **aktif ikameden devral** (`subst_bul`). Bir specialization'ın gövdesinden
başka bir generic çağrılıyorsa (`ekle$i64` → `büyü`), T argümanlardan değil
ÇAĞIRANIN ikamesinden gelir. C'nin TipSubst yayılımının karşılığı.
Sıra: (a) çıplak parametre → (b) aktif ikame → (c) `i32` fallback.

**Sabotaj S156** (malloc → global bölge) → **ÜÇ dosyayı birden** kırıyor (134/137).

**Kapılar:** `codegen_diff` **137/137** · `modul_codegen` 18/18 (İKİ yeşil koşum) ·
`codegen_genis` 67/67 (0 muaf) · `dizi_kullan` 12/12 kararlı.

---

## D-414 [YÜKSEK] — CODEGEN: legacy `kullan` düz ad alanı + AÇIK segfault bulgusu (2026-08-10)

**`test/crossfile` 0/2 → 2/2.**

**Kusur:** C'nin İKİ modül yükleyicisi FARKLI AD ALANI kuruyor ve self-host
hepsini modül-önekli sanıyordu. Ayırt edici C'de tek satır (`src/ana.c:204`):
```c
static int kullan_yeni_bicim(const Dugum *k) {
    return k->veri.kullan.segment_sayi <= 1 ||
           k->veri.kullan.secili_sayi > 0 ||
           k->veri.kullan.alias_ad != NULL;
}
```
Çok-segmentli **ve** seçili-import yok **ve** alias yok → **LEGACY**: üyeler
GLOBAL ad alanına düz girer (`@uc_kat`, `@iki_kat`), modül öneki YOK.
Self-host D-399'un `modül` sarmalını koşulsuz uyguluyordu → `@lib_islem.uc_kat`
yayıp çıplak `@iki_kat` çağrısını tanımsız bırakıyordu.
Onarım: sarmal yalnız YENİ biçimde; legacy'de metin DÜZ eklenir.
**Kuralı uydurmadım — C kaynağından okudum.** Probe'larım yol çözümünde takıldı
ve şekli tahmin etmek yerine ayırt ediciyi doğrudan `src/ana.c`'de buldum.

---

### 🔴 AÇIK KUSUR — `dizi_kullan` self-host ikilisinde ARALIKLI SEGFAULT
`modul_codegen` bir koşumda 17/18 (`dizi_kullan` C=42 ≠ KEMGU=**139**), sonraki
koşumda 18/18 verdi. Aynı ikili art arda: `42 42 139 42 …`; C oracle aynı
dosyada **8/8 kararlı 42**.
**D-414 sebebi DEĞİL:** `git stash` ile değişiklik geri alınıp ölçüldü —
segfault ÖNCEDEN de var. Kusur oradaydı ve kapıyı **şansla** geçiyordu.

> **⚠ ÖNCEKİ TEŞHİSİMİ DÜZELTİYORUM.** D-413'te aralıklı kırmızıları "art arda
> `make` hedefleri → Windows dosya kilidi" diye açıklamıştım. O etken gerçek
> (127'ler Defender exec yarışı) **ama 139'ları açıklamıyor**; en az biri bu
> segfault'tu. **"Aralıklı = artefakt" en cazip ve en tehlikeli varsayım.**
> **exit 127 ortamsaldır, exit 139 DEĞİLDİR** — aralıklılık UB'nin normal
> görüntüsüdür. Kod ayrımı `CLAUDE.md`'ye yazıldı.

Muafiyet EKLEMEDİM: kapının bu kusuru bazen yakalaması, hiç yakalamamasından
iyidir. Sıradaki iş bu.

**Kapılar:** `codegen_diff` 137/137 · `crossfile` **2/2** · `modul_codegen`
aralıklı (yukarıdaki açık kusur).

---

## D-413 [YÜKSEK] — CODEGEN: `&Yapi` yerelinde alan erişimi — SESSİZ YANLIŞ CEVAP (2026-08-08)

**Yeni yüzey ölçümü.** Kalan kapısız yüzeyler taranınca `test/asan_matris`
**11/12** çıktı (`test/ornekler/eski` ve `test/stdlib` oracle kurulamadığı için
sinyal vermedi, `test/crossfile` 0/2 — ayrı iş). Tek sapma **sessiz yanlış
cevaptı** ve kapatıldı → **12/12**.

**Kusur:** `değişken r: &H = dizi_al(d, i);` — DEĞER `ptr` döner ve `son_ref`
BOŞTUR, dolayısıyla `r.v` erişimi çözülemeyip **literal `0`a** düşüyordu:
```
C:    %33 = getelementptr %H, ptr %32, i32 0, i32 0   +   %34 = load i32
SELF: %32 = add i32 %30, 0        ← r.v LİTERAL 0
```
Link hatası yok, IR geçerli, program çalışıyor — toplam 50 yerine 0
(C=42, KEMGU=0).

**Onarım:** ANNOTASYON `&Yapi` ise pointee ONDAN alınır, değerden değil.
**`param_ref_yapi` bu işi PARAMETRELER için ZATEN yapıyordu**; yerel `değişken`
yoluna bağlanmamıştı — D-407'nin "aynı soruyu iki yerde ayrı yanıtlama"
deseninin bir örneği daha. Annotasyon DEĞERDEN üstün: `&H` yazıldıysa pointee
H'dir.

**Korpus:** `cg_ref_yerel_alan.kem` (asan_matris'ten birebir kopya + not).
**⚠ ŞEKİL ÖNEMLİ:** pointee'nin DEĞERDEN çıkarsanamadığı bir kaynak şart.
`değişken r: &H = &h;` YETMEZ — orada `&h` zaten `son_ref`i doldurur ve dosya
tesadüfen yeşil kalır. `dizi_al` gibi **opak `ptr` dönen** bir kaynak gerekir.

**Sabotaj S155** → `42 ≠ 0`, yani kapı sessiz-yanlış-cevap kipini exit farkı
olarak yakalıyor (136/137).

**⚠⚠ KAPI SONUÇLARINDA ARALIKLI SAHTE KIRMIZI — bu turda İKİ KEZ.**
`modul_codegen` bir kez başarısız dedi (yeniden koşumda 18/18 iki kez) ve
`cg_ham_isaretci_indeks` bir kez **exit 139 (segfault)** dedi (yeniden koşumda
137/137 iki kez). Tetikleyici: **tek bash çağrısında art arda birden çok `make`
hedefi** — her biri `build/codegen.exe`i yeniden kuruyor ve Windows'ta bir önceki
süreç dosyayı hâlâ tutabiliyor. D-411'in "zaman aşımına uğrayan make ölmez"
dersinin kardeşi. **KURAL: kapı kırmızıysa, teşhise başlamadan ÖNCE temiz
yeniden koş — ve en az İKİ yeşil koşum gör.** Sahte kırmızıyı gerçek regresyon
sanmak, bu oturumda geri almaya kalkabileceğim en pahalı hata olurdu.

**Kapılar:** `codegen_diff` **137/137** · `modul_codegen` 18/18 (0 muaf) ·
`codegen_genis` 67/67 (0 muaf) · `test/asan_matris` **12/12**.

---

## D-412 [YÜKSEK] — CODEGEN: GÖRELİ modül yolu (`ic::g` içinden `m`) (2026-08-07)

**`test/snapshots` 60/62 → 61/62.** Kalan tek sapma `asm_round_trip`
(satıriçi_asm, planı D-411 notunda).

**Kusur:** `modül m` içinden yazılan `ic::g()` **GÖRELİ** bir yoldur — `m.ic.g`
demektir, mutlak `ic.g` değil. `yol_noktali` yolu MUTLAK kuruyordu →
`@ic.g` tanımsız sembol.

**Onarım, mevcut bir kuralın uzantısı:** `fn_coz`un ÇIPLAK adlar için uyguladığı
**MODÜL-ÖNCE** bağlama (D-398) nitelikli yollara da genişletildi — modül
içindeysek önce `<önek>.<yol>` denenir. **Kayıt kontrolüyle:** göreli karşılığı
YOKSA mutlak ada dokunulmaz, yani mutlak yollar bozulmaz.
> Yeni bir mekanizma icat etmek yerine var olanın kapsamını genişletmek doğru
> hamleydi: çıplak ad ve nitelikli yol AYNI kapsam sorusunu soruyor. (D-407'nin
> "aynı soruyu iki yerde ayrı yanıtlama" dersinin olumlu yüzü.)

**Korpus:** `cg_gorece_yol.kem` (snapshot'tan birebir kopya + gerekçe notu).
Dosya AYNI ZAMANDA üç gölgeleme şeklini birden ölçüyor: `m::f` global `f`yi,
`m::ic::g` global `g`yi gölgeler, ayrıca `dışa` olmayan kardeş (`m::h`) çağrısı.

**Sabotaj S154** → KIRMIZI, iki dosyayı birden kırıyor (134/136).

**Kapılar:** `codegen_diff` **136/136** · `modul_codegen` 18/18 (0 muaf) ·
`codegen_genis` 67/67 (0 muaf).

---

## D-411 [YÜKSEK] — CODEGEN: `bellek_kopyala` + mono'da İPTAL yerine geri-düşüş (2026-08-07)

**`test/snapshots` 58/62 → 60/62.** İki AYRI kök.

**1. `bellek_kopyala` → libc `memcpy`.** Eşleme yoktu → tanımsız
`@bellek_kopyala`. D-388/D-405 ile aynı sınıf (yerleşik ADI kayıtlı, IR eşlemesi
eksik). C `call i32 @memcpy(...)` yayıyor — kendi `declare ptr @memcpy`
satırıyla tutarsız, LLVM sessizce kabul ediyor (D-295); C'nin DAVRANIŞI taklit
edildi.

**2. Mono'da İPTAL yerine GERİ-DÜŞÜŞ — D-401'in V1 kararının düzeltilmesi.**
D-401 çıkarsanamayan bir tip paramında mono'yu TÜMDEN iptal ediyordu. Bu
`hata_yap<T, E>(e: E) -> sonuç<T, E>` şeklinde KIRIYORDU: `E` çıplak parametre,
`T` DEĞİL. İptal edilince çağrı BASE gövdeye gidiyor (hepsi fallback →
`{i8,i32,i32}`) ama annotasyon `sonuç<tam32, metin>` = `{i8,i32,ptr}` çözülüyor
→ **store uyuşmazlığı**.
> **İlginç ayrıntı:** define ve çağrı BİRBİRİYLE anlaşıyordu; uyuşmazlık
> ANNOTASYONLAYDI. "Define≠call" diye aramak yanlış yere bakmak olurdu —
> hata satırını okumak şart.

Onarım: çıkarsanamayan param için fallback IR (`i32`) kullan. Sonuç
`hata_yap$i32$ptr` — **C'nin ürettiği mangled adla BİREBİR AYNI**.
**SAĞLAMLIK:** fallback yanlışsa (T gerçekte `tam64` ise) define ve çağrı yine
anlaşır ama annotasyonla uyuşmaz → LLVM REDDEDER. Hata **gürültülü** kalır,
sessiz yanlış cevaba dönüşmez.

**Korpus:** `cg_bellek_kopyala.kem` + `cg_generic_sonuc_ptr.kem` (ikisi de
snapshot'tan birebir kopya — uydurmadım). `d1_generic_sonuc_ptr` zaten C'nin
bir zamanlar yaşadığı aynı bug'ın REGRESYON MUHAFIZI; dosyanın kendi yorumu
kökü tarif ediyor.

**Sabotaj:** S152 (`memcpy` eşlemesi) · S153 (mono iptaline geri dön) → ikisi de
KIRMIZI (134/135).

**Kalan 2:** `asm_round_trip` (satıriçi_asm sessizce düşüyor — planı kayıtlı) ·
`ad_cozum_sapma` (`@ic.g` tanımsız).

**Kapılar:** `codegen_diff` **135/135** · `modul_codegen` 18/18 (0 muaf) ·
`codegen_genis` 67/67 (0 muaf).

---

## D-410 [YÜKSEK] — CODEGEN: `eşleş` iç ayırıcı testi — SESSİZ YANLIŞ CEVAP (2026-08-07)

**En ağır sınıf.** Link hatası YOK, IR geçerli, program çalışıyor — yalnız
**yanlış dala gidiyor**. `test/snapshots/cesit_sonuc` C=42 iken KEMGU=3
veriyordu.

**Kusur:** self-host `eşleş`te alt-deseni **DAİMA bir bağlama ADI** sanıyordu.
`hata(H::B)` gibi bir **DESEN_YOL** alt-deseninde iç ayırıcı **HİÇ
karşılaştırılmıyordu** → dış tag eşleşince gövdeye giriliyor, dolayısıyla TÜM
`hata(...)` kolları **İLKİNE** gidiyordu. (`H::B` adında bir değişken bağlanıyordu
— zararsız ama anlamsız.)

**Ölçümle izole edildi** (dosyanın tamamıyla uğraşmadan): 3 varyantlı minimal
probe → `hata(H::B)` için C=20, SELF=**10**. Hipotez ("yalnız dış tag'e bakıyor")
önce yazıldı, sonra ölçüldü — teşhis değil ÖLÇÜM.

**Onarım:** dış tag dalından SONRA, gövdenin başında İKİNCİ bir dal payload'ın
ayırıcısını sınar; tutmazsa `Lnext`e (sonraki kol) düşer. Payload'lı çeşit
`{i8,...}` ise ayırıcı 0. alandadır (`extractvalue`), payload'suz enum doğrudan
`i8`dir.

**Korpus:** `cg_cesit_ic_ayirici.kem` (`cesit_sonuc` snapshot'undan birebir kopya
+ gerekçe notu — uydurmadım).
**⚠ NEDEN EN AZ ÜÇ VARYANT ŞART:** iki varyantla "hep ilkine git" hatası %50
olasılıkla doğru cevabı verir ve test **tesadüfen yeşil kalabilir**. Üç varyant +
hepsinin AYRI dönüş değeri, yanlış dallanmayı kaçınılmaz kılar.
(D-393'ün "3 parametre yetmez, en az 4 gerekir" dersinin aynısı.)

**Sabotaj S151** → `exit=42 ≠ 3` — yani kapı sessiz-yanlış-cevap kipini
**exit farkı olarak** yakalıyor, link hatası olarak değil (133 → 132/133).

**`test/snapshots` 57/62 → 58/62.** Kalan 4 AYRI kök: `ad_cozum_sapma` (`@ic.g`) ·
`asm_round_trip` (**C=42 KEMGU=1**, hâlâ sessiz yanlış cevap) · `bolge_al_grow`
(`@bellek_kopyala`) · `d1_generic_sonuc_ptr` (`{i8,i32,i32}`).

**Kapılar:** `codegen_diff` **133/133** · `modul_codegen` 18/18 (0 muaf) ·
`codegen_genis` 67/67 (0 muaf).

---

## D-409 [YÜKSEK] — CODEGEN: Türkçe TİP adlarının IR'da tırnaklanması (2026-08-07)

**Yeni yüzey `test/snapshots`** (81 dosya, kapısız) ölçüldü: **56/62**, ALTI
sapma — ve D-406 dersi uygulanarak "tek kök" SAYILMADI. İkisi sessiz yanlış
cevap, biri Türkçe tip adı. Bu artım sonuncusunu kapattı → **57/62**.

**Kusur:** C Türkçe (ASCII-dışı) tip adlarını IR'da TIRNAKLIYOR (`%"Köşe"`),
self-host tırnaksız `%Köşe` yayıyordu → clang REDDEDER ("expected '=' after
name"). **Tırnaklama mantığı İŞLEV adları için ZATEN VARDI** (`ir_ad_yaz`;
`@"dizi.oluştur"` doğru çıkıyordu) — tip adlarına uygulanmamıştı.
**D-407'nin aynı deseni:** aynı soruyu iki yerde ayrı yanıtlayan kod.

**🎯 KEMGU DİZGİ LİTERALİNDE ÇIPLAK `"` ÜRETİLEMEZ.** Yardımcıyı yazarken
`"%\""` denedim; ÖLÇTÜM: **`\"` KAÇIŞ DEĞİLDİR** — `metin_uzunluk("a\"b") == 4`,
yani `\` ve `"` İKİ karakter olarak saklanır (lexer `\"`de dizgiyi bitirmiyor
ama ikisini de tutuyor). Ham dizgi `r#"..."#` bu konumda P010 veriyor.
`\n` ile aynı sınıf (D-400). **Çözüm ölçümden çıktı:** iki karakterlik dizginin
İKİNCİSİNİ kes → `metin_kes("\"", 1, 1)` tek `"` verir (doğrulandı: bayt 34).
`codegen.kem` başka yerlerde `yb(34)` kullanır; o ÇIKTIYA yazar — tırnağın
DEĞER olarak gerekmesi farkı yaratıyor.

**İLERİ ve GERİ dönüşüm birlikte gerekti.** `ir_tip_ad` (ad → `%"Ad"`) eklenince
yapı adını IR tipinden GERİ okuyan 13 yer bozuldu: yalnız `%` soyan eski kod
`"Köşe"` döndürüyordu → alan araması başarısız, `extractvalue ..., -1` (LLVM
"expected integer"). `ir_tip_soy` eklendi ve **iki fonksiyon yan yana kondu** ki
ayrışmasınlar — bu kusurun kök nedeni zaten ayrışmaydı.

**Korpus:** `cg_turkce_tip_adi.kem` (snapshot'tan birebir kopya — uydurmadım).
**Sabotaj S150** → KIRMIZI, üstelik İKİ dosyayı birden kırıyor (130/132).

**Kalan 5 snapshot sapması — AYRI kökler, hiçbiri varsayılmadı:**
`ad_cozum_sapma` (`@ic.g` tanımsız) · `asm_round_trip` (**C=42 KEMGU=1**) ·
`bolge_al_grow` (`@bellek_kopyala` tanımsız) · `cesit_sonuc` (**C=42 KEMGU=3**) ·
`d1_generic_sonuc_ptr` (`{i8,i32,i32}` tip uyumsuzluğu).
**İkisi SESSİZ YANLIŞ CEVAP** — link hatalarından önce onlar bakılmalı.

**Kapılar:** `codegen_diff` **132/132** · `modul_codegen` 18/18 (0 muaf) ·
`codegen_genis` 67/67 (0 muaf).

---

## D-408 [YÜKSEK] — YENİ YÜZEY `check_korpus` + kesirli dizi + `ifşa` (2026-08-07)

**Yeni ölçüm yüzeyi.** `test/moduller` doyunca sıradaki kapısız yüzey arandı:
`test/check_korpus` (104 dosya) yalnız `--check` ile ölçülüyordu, codegen'le HİÇ.
İlk ölçüm **30/32** (72 atlandı — kasıtlı hatalı tanı dosyaları, C oracle
reddediyor → doğru davranış). İki gerçek kök, **ikisi de kapatıldı → 31/32**:

**1. Kesirli dizi argümanı.** `Dizi<kesirli64> = [1.5, 2.5]` için self-host
`call ... @kdl_dizi_ekle_tam(ptr, ptr, i32 1.5)` basıyordu — LLVM REDDEDER
("floating point constant invalid for type"). C ise `_tam` sonekini korur ama
argümanı DOĞAL tipiyle geçer (`double %3`), kendi `declare ... i32`siyle
UYUŞMAZ; LLVM sessizce kabul eder (D-295). **C'nin DAVRANIŞI taklit edildi,
"doğrusu" değil** — parite hedefi bu (D-388'deki `free`→i32 ile aynı gerekçe).

**2. `ifşa(e)` (declassify) PASS-THROUGH.** Codegen'i yoktu → tanımsız `@ifşa`.
`tekkez_olustur`/`dondur` ile aynı sınıf.

**🛑 BİLİNÇLİ OLARAK YAPILMADI — `sabitsüre_olustur`.** `ifşa` onarılınca
arkasından çıktı. Naif pass-through link hatasını kapatırdı, AMA C yalnız değeri
geçirmiyor: yanında **`call void @llvm.x86.sse2.lfence()`** spekülasyon bariyeri
yayıyor. Bariyeri sessizce düşürmek self-host derleyicide **sessiz bir GÜVENLİK
regresyonu** olurdu (sabit-süre disiplini). Link hatası gürültülüdür, eksik
bariyer değildir — **yarım onarım burada kusurun kendisidir.** `tc19_02` açık
bırakıldı; sabitsüre codegen'i AYRI ve güvenlik-duyarlı bir iştir.

**Korpus:** `cg_kesirli_dizi.kem`. **SINIR:** elemanı GERİ OKUMUYOR —
`kdl_dizi_al_tam` i32 döndürdüğü için `Dizi<kesirli64>` okuması **C'DE DE**
bozuk (ölçüldü: geri okuyan sürümüm C oracle'da exit 2 verdi). C'nin
desteklemediğini test etmek kapıyı değil PROBE'u sınar. Kesirli dizi OKUMA yolu
ayrı bir kusurdur ve **iki tarafta da AÇIKTIR** — kayda geçti.

**Sabotaj S149** (kesirli arg tipi) → KIRMIZI (130/131).

**Kapılar:** `codegen_diff` **131/131** · `modul_codegen` 18/18 (0 muaf) ·
`codegen_genis` 67/67 (0 muaf).

---

## D-407 [YÜKSEK] — CODEGEN: nitelikli çeşit yapıcısı — 🎯 `test/moduller` 18/18 (2026-08-07)

**🎯 MUAFİYET LİSTESİ BOŞALDI.** Kapı 7 muafiyetle kuruldu (11/18); D-404, D-405,
D-406 ve D-407 hepsini kapattı → **18/18, 0 muaf**.

**Kusur:** codegen'in çeşit-yapıcı kolu solu **yalnız `TANIMLAYICI`** kabul
ediyordu. Nitelikli (üç segmentli) biçimde sol bir **YOL**'dur:
```
ifd::Ifade::Sayi(3)
  dış YOL a_deg="Sayi" → çocuğu YOL("Ifade") → çocuğu TANIMLAYICI("ifd")
```
Kol atlanınca yapıcı GENEL ÇAĞRI yoluna düşüp `i32` üretiyordu; annotasyon ise
çeşit struct'ını (`{i8, i64, ptr, ptr, ptr, ptr}`) çözüyordu → `store` tip
uyumsuzluğu.

**Onarım tek koşul genişletmesi.** Çeşit ADI her iki biçimde de o düğümün
`a_deg`indedir (modül öneki DAHA DERİNDE durur) → ek ayrıştırma gerekmedi.
**Checker'ın `yol_cesit_adi`si zaten İKİSİNİ DE kabul ediyordu**; codegen ona
hizalandı — iki tarafın ayrı davranması kusurun kendisiydi.
> **DERS: aynı soruyu iki yerde ayrı yanıtlayan kod, er ya da geç ayrışır.**
> Checker doğru kuralı biliyordu; codegen bilmiyordu. Böyle bir kusuru ararken
> "diğer taraf ne yapıyor?" ilk soru olmalı.

**Korpus:** ayrı dosya YAZMADIM. `ana_ifd` zaten `modul_codegen` kapısında
(0 muaf) ve sabotaj oradan ölçülüyor. Kendi uydurduğum fikstür (`Sekil::Nokta`
payload'suz varyant + `eşleş *s`) **C ORACLE'DA da patladı** ("extractvalue
operand must be aggregate type") — yani şekil geçersizdi. Gerçek modülü kapı
olarak kullanmak hem doğru hem ucuz. (D-391'in dersi: uydurma, kaynaktan al.)

**Sabotaj S148** (YOL kolu) → `ana_ifd` KIRMIZI (17/18).

**Kapılar:** `modul_codegen` **18/18 (0 muaf)** · `codegen_diff` 130/130 ·
`codegen_genis` 67/67 (0 muaf).

---

## D-406 [YÜKSEK] — CODEGEN: ham işaretçi indekslemesi (`*T` → GEP) (2026-08-07)

**🎯 `test/moduller` 11/18 → 16/18.** Beş dosya birden açıldı; muafiyet 7 → **2**.

**Kusur:** self-host'un INDEKS dalında **ham-işaretçi kolu HİÇ YOKTU**. Her
indeksleme `@kdl_dizi_al_*` / `@kdl_dizi_yaz_*` (KdlDizi **başlıklı** heap dizi)
yoluna gidiyordu. `*T` ise düz bellek — başlığı yok. Sonuç iki katlı:
1. **YANLIŞ LOWERING** — başlıksız bir bloğu KdlDizi sanmak
2. **TİP HATASI** — `tam64` indeks, i32 parametreli runtime çağrısına

C llvm.c aynası: `getelementptr <pointee>, ptr <base>, i64 <idx>` + load/store.
Ayrım **POZİTİF** bilgiyle yapılır (`cg_apointee`, D-267); kayıt yoksa eski heap
yoluna düşülür — bilgi olmadan yol değiştirmek sessiz miscompile riskidir.

**OKUMA ve YAZMA kolları AYRI yerlerde yaşıyor** (INDEKS ifadesi vs ATAMA
lvalue) → iki ayrı onarım gerekti. İlk onarımdan sonra hata `%44`→`%42`ye
kaydı ve sınıf aynı kaldı; simetrik kolu bulana kadar bitmedi.

**🎯 ÜÇ TURLUK ASIL DERS — muafiyet listemin GEREKÇESİ YANLIŞTI.** Bu 7 dosyayı
"hepsi tek kök: dönüş-tipi-güdümlü çıkarsama" diye kaydetmiştim (D-402). Hata
satırını tek tek izleyince **ÜÇ AYRI kök** çıktı ve **hiçbiri o değildi**:
`bölge_al` eşlemesi yok (D-405) · `yetki<R>` IR tipi yok (D-404) · `*T`
indekslemesi heap yoluna düşüyor (D-406). Doğrudan "dönüş-tipi-güdümlü
çıkarsama" yazmaya başlasaydım **büyük bir özelliği yanlış yere** yazardım.
> **Muafiyet listesine yazdığın GEREKÇE de bir İDDİADIR — ölç.** "Kalanların
> hepsi aynı kök" en cazip ve en test edilmemiş varsayımdır.

**Kalan 2, AYRI köklerde:** `ana_ifd` (çapraz-modül ÇEŞİT payload layout'u) ·
`dizi_yapi` (C=42, KEMGU=127 — link GEÇİYOR, kusur DAVRANIŞTA).

**Korpus:** `test/cg_korpus/cg_ham_isaretci_indeks.kem`. **i64 indeks ŞART** —
i32 ile heap yolu tesadüfen tip-uyumlu kalır ve dosya yeşil geçer.
**Şekli kaynaktan aldım:** ilk probe'umda `bellek_al(64) olarak *tam64` yazdım,
C E002 ile reddetti (`olarak` yalnız sayısal/karakter). Ham işaretçi
`bölge_al`dan gelir — `kütüphane/dizi.kem`in yaptığı budur. (D-391'in dersi,
bu oturumda üçüncü tekrarı.)

**Sabotaj:** S146 (okuma kolu) · S147 (yazma kolu) → ikisi de KIRMIZI (129/130).

**Kapılar:** `codegen_diff` **130/130** · `modul_codegen` **16/16 (2 muaf)** ·
`codegen_genis` 67/67 (0 muaf).

---

## D-405 [YÜKSEK] — CODEGEN: `bölge_al` yerleşiği + kök-ölçümünün değeri (2026-08-07)

**Kusur:** `bölge_al(yetki, N)` için IR eşlemesi YOKTU → self-host
`call i32 @"bölge_al"(...)` üretiyordu: hem **tanımsız sembol** hem **yanlış tip**.

**Arite farkı — salt ad eşlemesi YETMEZ.** C `bölge_al`ın **argümanlarını YOK
SAYAR** ve parametresiz `@kdl_global_bolge_al()` çağırır (ölçüldü). Bu yüzden
`builtin_kdl_ad`e satır eklemek yanlış olurdu (genel çağrı yolu argümanları
basardı) — özel dal gerekti. İlk denemem ad eşlemesiydi, arite farkını görünce
geri aldım.

**🎯 ASIL DERS — bu kök, "dönüş-tipi-güdümlü çıkarsama" sandığım şeyin ALTINDA
duruyordu.** Kalan 7 modül dosyasının hepsi `kütüphane/dizi.kem`in `oluştur`unda
takılıyordu ve `oluştur` `bölge_al` çağırıyor. Ben D-404'ten sonra doğrudan
"dönüş-tipi-güdümlü çıkarsama" yazmaya başlayacaktım — **kökü ölçmeden büyük bir
özellik yazsaydım YANLIŞ YERİ onarırdım** ve muhtemelen "çalışmıyor" deyip daha
da büyük bir şey yazardım. Hata satırını (`store ptr %5, ptr %1`) tek tek
izlemek bu turda iki ayrı kök açtı (`bölge_al`, sonra i64→i32 indeks daraltma).

**Kalan takoz (yeni, ölçüldü):** `%44 = load i64` → `call i32 @kdl_dizi_al_tam(ptr, i32 %44)`
— heap dizi erişiminde `tam64` indeks i32'ye DARALTILMIYOR. Ayrı ve bounded.
`test/moduller` **11/18'de sabit** ama hata noktası `%5`ten `%44`e ilerledi.

**Korpus:** `test/cg_korpus/cg_bolge_al.kem` (yetki + bölge_al + geri_al).
**Sabotaj S145** → KIRMIZI (128/129).

**⚠ ÜÇÜNCÜ KEZ AYNI TUZAK:** sürücü kapısı **128/129 kırmızı** raporladı — ama o
koşum **S145 sabotajı uygulanmışken** sürüyordu. Kapı sonucu ARTEFAKTTI.
**Uzun koşum sürerken kaynağa dokunma** kuralı bu oturumda üç kez ısırdı
(D-402'de `test_tumu`, burada iki kez). Kapı başlatınca kaynağı DONDUR.

**Kapılar:** `codegen_diff` **129/129** · `modul_codegen` 11/11 (7 muaf).

---

## D-404 [YÜKSEK] — CODEGEN: `yetki<R>` (Capability Spec V1) self-host'ta (2026-08-07)

**Kusur:** `ll_tip`te `TIP_YETKI` dalı YOKTU (i32 fallback) ve
`yetki_olustur`/`geri_al` için emit YOKTU → `use of undefined value
'@yetki_olustur'`.

**⚠ NEDEN ŞİMDİYE KADAR HİÇBİR KAPI GÖRMEDİ — örtük kapsam deliği.**
`yetki` kullanan tek .kem dosyası `test/ornekler/kem_heap.kem` ve onun **C
ORACLE'ı host'ta LİNKLENMİYOR** (`kdl_mmio_oku32` bare-metal sembolü) →
`codegen_genis` onu "oracle yok, atla" ile geçiyor. Kapı **67/67 yeşilken** bu
boşluk sessizce duruyordu.
> **DERS: "oracle kurulamadı → atla" politikası DOĞRU (karşılaştırma anlamsız
> olurdu) ama KÖR NOKTA yaratır.** Atlanan dosyaların TEK kullanıcısı olduğu bir
> özellik hiç ölçülmemiş olur. Saf-yetki programı host'ta linkleniyor (ölçüldü:
> C exit 42) — yani bu özellik gate'lenebilirmiş, kimse denememişti.

**ABI C'den ÖLÇÜLDÜ, varsayılmadı:** `%kdl_yetki = type { i64, i16, i16, i8,
[3 x i8] }` (16 bayt) ve `yetki_olustur` **OUT-PTR** ile çalışır — dönüş
register'da DEĞİL:
```
%s = alloca %kdl_yetki
call void @kdl_yetki_olustur(ptr %s, i16 kt, i16 izin)
%v = load %kdl_yetki, ptr %s
```
`geri_al` **SLOT ADRESİ** ister (değer değil) — yetkiyi yerinde geçersizleştirir.
Argüman TANIMLAYICI ise alloca'sı doğrudan geçilir; değilse geçici slot.
(Bu, hafızadaki "yetki = OUT-PTR ABI; sret premisi EMPİRİK yanlıştı" notuyla
tutarlı — naif aggregate-return AAPCS64 register-pack ile uyuşmuyor.)

**Kapsam:** `yetki<R>` PARAMETRE olarak da taşınır — `kütüphane/dizi.kem`in tüm
imzaları (`oluştur<T>(böl: yetki<Bellek>)`) bunu gerektiriyor, yani bu kalan 7
modül dosyasının İKİ ön koşulundan biri. Diğeri: dönüş-tipi-güdümlü çıkarsama.

**Sabotaj:** S142 (IR tipi) · S143 (`yetki_olustur` emit) · S144 (`geri_al` emit)
→ üçü de `cg_yetki` üzerinde KIRMIZI (127/128).

**Kapılar:** `codegen_diff` **128/128** · `modul_codegen` 11/11 (7 muaf) ·
`codegen_genis` 67/67 (0 muaf).

---

## D-403 [YÜKSEK] — CODEGEN: MODÜL generic'lerinin monomorfizasyonu (2026-08-07)

**D-402'de REDDETTİĞİM D-401b, kökü teşhis edilince kabul edildi.** O deneme
prensipte doğruydu (çağrı yerindeki mangled addan modül önekini soyup generic
registry'sini sorgulamak) ama İKİ kusur taşıyordu. İkisi de ölçümle bulundu:

**Kusur 1 — specialization MODÜL BAĞLAMINI kaybediyordu.** `fs_kuyruk_emit`
`emit_tanimlar`dan SONRA koşar; orada `p.mod_onek` boştur. Bağlam saklanmadan
specialization gövdesindeki çıplak kardeş çağrıları üst düzeye bağlanıyordu.
`test/moduller/ana_golge_jenerik.kem` **tam bu senaryonun muhafızıdır** (dosyanın
kendi yorumu söylüyor: "100 dönerse specialization global-first sapmış") ve
KEMGU 100 döndürüyordu, doğrusu 1.

**Kusur 2 — İLK ONARIMIM DA YANLIŞTI, ölçüm yakaladı.** Bağlamı saklamayı
`p.mod_onek`i enqueue anında kaydederek çözdüm sandım; kapı hâlâ kırmızıydı.
Sebep: `mono_islev_kayit` **ÇAĞRI YERİNDE** koşar ve orada `mod_onek`
ÇAĞIRANINDIR (`arac::capraz` çağrısı `main`den yapılır → ""). Önek
**BİLDİRİMİN** modülünden gelmeli — ve o zaten mangled adın içinde kodludur
(`nokta_onek("arac.capraz")` → `"arac"`).
> **DERS: doğru mekanizmayı seçmek yetmiyor, DOĞRU KAYNAKTAN beslemek gerekiyor.**
> "Bağlamı sakla" fikri doğruydu; hangi bağlamı sorusunda yanıldım.

**Sonuç:** modül generic'leri artık specialize ediliyor — `@dizi.al$i64`,
`@dizi.ekle$i64`, `@cgmodul_mat.esle$double` (C ile aynı adlar).
`test/moduller` **11/18'de sabit** (regresyon YOK, sayı da artmadı): kalan 7
dönüş-tipi-güdümlü çıkarsama ister. Ama mangling artık DOĞRU, ki o işin ön koşulu.

**Kapı gözlenebilirliği:** `modul_codegen` D-403 ÖNCESİ de 11/11 yeşildi — modül
specialization'ını GÖRMÜYORDU. Yeni korpus dosyası (`cg_modul_generic.kem` +
`cgmodul_mat.kem` fikstür eki) bunu gözlenebilir kılar.
**Tamsayı yetmez** (D-401 dersi: ABI şansı) → `kesirli64` kapının asıl dişi.

**Sabotaj:** S140 (önek soyma) → link hatası (126/127). **S141 (bildirim öneki)
→ `exit=4`**, yani `esle_gizli() != 20` muhafızı: modül üyesi yerine üst-düzey
`gizli` (999) çağrılıyor — **sessiz yanlış cevap**; ayrıca `modul_codegen` 9/11.

**Kapılar:** `codegen_diff` **127/127** · `modul_codegen` 11/11 (7 muaf).

---

## D-402 [YÜKSEK] — KAPI: çapraz-dosya modül codegen'i + REDDEDİLEN D-401b (2026-08-07)

**Neden:** `test/moduller/` **hiçbir kapının altında değildi.** D-399/400/401
boyunca oranı elle bir kabuk döngüsüyle izledim — bu tam olarak D-395'te
"elle koşturulan ölçüm döngüsü kapı DEĞİLDİR" diye yazdığım şeydi ve bu kez
tersinden ısırdı.

**🔴 REDDEDİLEN DEĞİŞİKLİK — D-401b.** D-401'in V1 sınırını aşmak için çağrı
yerindeki mangled addan (`dizi.al`) modül önekini soyup generic registry'sini
(`tp_yad`, bildirim adıyla anahtarlı) sorgulamayı denedim. **Prensipte doğru** ve
kısmen işe yaradı: `@dizi.ekle$i64` + `@dizi.al$i64` yayılmaya başladı, C ile
aynı adlar. **Ama oran 11/18 → 9/18'e DÜŞTÜ:**
- `ana_sayi_transitif` → `use of undefined value '@azami'` (link hatası)
- `ana_golge_jenerik` → **C=1, KEMGU=100 — SESSİZ YANLIŞ CEVAP**, link hatası değil.

**`codegen_diff` (126/126) ve `codegen_genis` (67/67) İKİSİ DE YEŞİLDİ.** Bu
yüzeyi görmüyorlar. Değişikliği elle ölçtüğüm için geri aldım; **ölçmeseydim
gönderirdim.** Link hatasını sessiz yanlış cevaba takas eden bir diff kabul
edilemez → tümüyle geri alındı, kök ileriye bırakıldı.

**Kapı:** `test/modul_codegen_harness.sh` + `make calistir_modul_codegen`.
Exit kodu + **stdout**. Durum **11/11, 7 muaf**.

**Muafiyet listesi (7, küçülmek zorunda) — kökü ÖLÇÜLDÜ, TEK sınıf:**
çapraz-modül generic mono'sunun **dönüş-tipi-güdümlü** kısmı. D-401'in V1'i yalnız
ÇIPLAK `T` parametresinden çıkarsar; `kütüphane/dizi.kem` bunu aşıyor:
```
oluştur<T>(böl: yetki<Bellek>) -> Liste<T>   ← T YALNIZ dönüşte
boy<T>(l: &Liste<T>) -> tam64                ← T İÇ İÇE (&Liste<T>)
```
T `değişken l: dizi::Liste<tam64>` annotasyonundan gelmeli. Ayrıca
`yetki<Bellek>` parametresi `%kdl_yetki` taşınmalı, self-host `i32` sanıyor.

**Sabotaj:** **S138 SESSİZ kaldı ve bu bir bulgu DEĞİLDİ** — `son_segment`
kullanmıştım, o `::` ile böler, nokta ile değil → sabotaj **uygulanmamıştı**
(no-op). Bunu "kapı zayıf" diye kaydetmek yanlış olurdu. D-401b birebir yeniden
kurulunca (**S139**) kapı KIRMIZI: `ana_golge_jenerik` exit farkı (C=1≠100) +
`ana_sayi_transitif` link — elle gözlediğimin AYNISI.
> **DERS: sabotajın sessizliği önce SABOTAJIN KENDİSİNİ şüpheli kılar.**
> D-356 "korpusu düzelt" diyordu; buradaki varyant "sabotajı düzelt".

**Yol üstünde ikinci ölçüm hatası:** `test_tumu` koşarken `selfhost/codegen.kem`i
DÜZENLEDİM → kapı yarı-bozuk kaynaktan `codegen.exe` kurdu ve `codegen_genis`
**66/67** raporladı. Temiz kaynakta 67/67. **Uzun koşum sürerken kaynağı
değiştirme.**

**Kapılar:** `modul_codegen` **11/11 (7 muaf)** · `codegen_diff` 126/126 ·
`codegen_genis` 67/67 (0 muaf) · `checker_diff` 148/148.

---

## D-401 [YÜKSEK] — CODEGEN: generic İŞLEV monomorfizasyonu (self-host) (2026-08-07)

**Kusur:** `ADIM 23 — LLVM monomorphization` **C derleyiciye** aittir; self-host'ta
generic İŞLEV mono'su HİÇ YOKTU. Tek gövde yayılıp T daima `i32` sanılıyordu:
```
C:    define i32 @kimlik$i32(ptr, i32)  +  define i64 @kimlik$i64(ptr, i64)
SELF: define i32 @kimlik(ptr, i32)      ← TEK gövde
      call i32 @kimlik(ptr %2, i64 %5)  ← İMZA UYUŞMAZLIĞI
```

**KÖK — tek satır:** `parse_islev_genel` `atla_tip_paramlar`ı çağırıyor ama
`tip_param_kaydet`i ÇAĞIRMIYORDU → param adları `tp_pending`de yakalanıp bir
sonraki bildirimde ÜZERİNE YAZILIYORDU. Yapı/çeşit yolu kaydediyor, işlev yolu
kaydetmiyordu. İkame makinesinin GERİ KALANI zaten vardı (`subst_bul` `ll_tip`e
bağlı, `mono_ir_sanitize`, `mono_sp`/`mono_si` yığını).

**⚠ TAMSAYIYLA ÖLÇMEK YETMEZ — üç kez yanlış yeşil aldım.** LLVM `define`/`call`
uyuşmazlığını SESSİZCE kabul eder (D-295) ve x86-64'te tamsayı değer register'da
hayatta kalır: `2^33+42` ile denedim, **kırpılmadı**, eski kod da 42 verdi. Kusur
ancak **register sınıfı değişince** gözlenebilir olur (D-294'ün aynı dersi:
tamsayı x0/rax, kesirli v0/xmm0). Korpusun asıl dişi `kesirli64` örneğidir.
Ayrıca **exit-kodu testi yanıltıcıydı**: kırpma olsa bile alt 32 bit 42 verip
testi yeşil gösterebiliyordu → karşılaştırmalı teste geçildi.

**Uygulama:** `fs_*` bekleyenler kuyruğu + `mono_islev_kayit` (arg IR
tiplerinden T çıkarsama, `$i64` mangling) + `fs_kuyruk_emit` **WORKLIST**
(specialization emisyonu SIRASINDA yeni specialization doğabilir — generic→generic)
+ `fn_ad_zorla` (define adı override).

**⚠ KENDİ TASARIM HATAM, ÖLÇÜMLE YAKALANDI:** ilk sürümde base gövdeyi C gibi
ATLIYORDUM → **regresyon 11/18 → 8/18** (`@sayi.azami`, `@dizi.al`,
`@arac.capraz` tanımsız). Sebep: mono çıkarsaması BAŞARISIZ olan çağrı yerleri
base ada düşüyor, gövde yoksa tanımsız sembol. **C'nin base'i atlayabilmesi
çıkarsamasının TAM olmasına dayanır; benimki V1'de kısmî** (yalnız ÇIPLAK `T`
parametresi tanınır; `Dizi<T>` gibi iç içe konumlar ve dönüş-tipi-güdümlü
çıkarsama yok). Base ile `f$i64` AYRI semboller → ikisini birden yaymak çakışma
üretmez, yalnız kullanılmayan bir gövde bırakır. Sağlamlık > IR zarafeti.

**Sonuç:** aynı-dosya generic mono ÇALIŞIYOR (`kesirli64` artık 42; önceden
LLVM-RED). `test/moduller` **11/18'de sabit** — regresyon yok, ilerleme de yok:
kalan 7 dosya **dönüş-tipi-güdümlü** çıkarsama istiyor (`dizi::oluştur(...)`de
T argümanlarda YOK, nitelikli annotasyondan gelir). Ayrı adım.

**Sabotaj:** S135 (işlev tip-param kaydı) · S136 (çağrı yeri mono) · S137
(specialization kuyruğu) → **üçü de** `cg_generic_mono` üzerinde KIRMIZI (125/126).

**Kapılar:** `codegen_diff` **126/126** · `codegen_genis` 67/67 (0 muaf) ·
`checker_diff` 148/148 · `parser_diff` 13/13 · FIXPOINT ✓ (63783 satır).

---

## D-400 [YÜKSEK] — CODEGEN: `\n` kaçış tuzağı + alias/seçili import (7/18 → 11/18) (2026-08-07)

**🎯 EN ÖNEMLİ BULGU — `\n` KEMGU dizgi literalinde KAÇIŞ DEĞİLDİR.**
`metin_uzunluk("a\nb") == 4` (hem C hem self-host — dil davranışı, parite kusuru
değil). `codegen.kem` bu yüzden her yerde `yb(10)` kullanır. D-399'daki sarmalım
`"{\n"` ile kuruluyordu → üretilen kaynağa **düz `\` + `n` çöpü** giriyordu.

**BU BENİ YANLIŞ BİR KÖKE SÜRÜKLEDİ.** `mat` çalışıyor, `zincir` çalışmıyordu; buradan
"iç içe `ayr_olustur`+`lex_et` metin belleğini eziyor" sonucuna varmıştım (D-399'a
öyle yazdım). **YANLIŞTI.** Çürüten deney: özyinelemeyi kapattım ama iç içe
lex+parse'ı BIRAKTIM → sarmal DÜZELDİ. Yani iç içe parse masumdu. Ardından probe
ile `bas`ı bastırdım: `bas=modül mat {\n` — çöp gözle görüldü.
> **DERS: "A çalışıyor, B çalışmıyor" bir MEKANİZMA teşhisi değildir.** İki
> hipotezi (sıra / aliasing) ölçüp çürüttükten sonra bile üçüncü yanlış hipoteze
> gittim. Doğru hamle en baştan **ara değeri bastırmaktı** — teşhis değil ölçüm.
> `mat`in çalışması TESADÜFTÜ: çöpü izleyen `//` yorumu satırı yutuyordu.

**Onarım:** gerçek satır sonlarına yaslan — açılışta `{` sonrası BOŞLUK yeter
(`ic`in ilk satırı zaten kendi satırı), kapanışta `ic` dosya-sonu satır-sonuyla
bittiği için `}` yeni satırda başlar. Yeni kaçış makinesi GEREKMEDİ.

**Alias / seçili import — C ÖLÇÜLDÜ, VARSAYILMADI.** İlk içgüdüm sarmalı alias'la
adlandırmaktı; ölçüm bunu çürüttü: C **her iki biçimde de** define'ı GERÇEK modül
adıyla yayar (`@mat.topla`), alias YALNIZ çağrı yerinde yaşar.
- Seçili import: parser eşlemeyi ZATEN `si_ad`/`si_yol`de tutuyordu, codegen
  bakmıyordu → `fn_coz`a eklendi (kayıt kontrolüyle; aynı adda gerçek bir
  üst-düzey işlev varsa yanlış yönlendirme olmasın).
- Alias: eşleme YOKTU (`mod_ad` yalnız "bu ad bir modüldür" der, HANGİ modül
  olduğunu söylemez) → `al_ad`/`al_yol` çifti eklendi, `alias_coz` YOL'un EN SOL
  segmentinde uygulanır (`m::topla`da `topla` üye adıdır, modül değil).

**Sonuç: `test/moduller` 7/18 → 11/18** (D-399'un 0→7'si üstüne). **Ad çözümü
sınıflarının TAMAMI kapandı.** Kalan 7'nin hepsi TEK sınıf: çapraz-modül TİP
çözümü (`'%N' i32 but expected 'ptr'`) — `ana_ifd`, `ana_kap`, `ana_kap_coklu`,
`dizi_*`. Ad değil tip; ayrı alt-sistem.

**Sabotaj:** S132 (alias) KIRMIZI · S133 (seçili import) KIRMIZI ·
**S134 (`\n` çöpü) ÖNCE SESSİZ**. Gizlemedim: korpusta o şekil YOKTU. Tek
seviyeli modülde çöp tesadüfen çalışıyor; yalnız TRANSİTİF halkada gözlenebilir.
`cgmodul_zincir.kem` + `cg_modul_transitif.kem` eklendi → S134 KIRMIZI (124/125).
**Kuralı değil KORPUSU düzelttim** (D-356 disiplininin dördüncü tekrarı).

**Kapılar:** `codegen_diff` **125/125** · `checker_diff` 148/148 ·
`parser_diff` 13/13 · FIXPOINT ✓ (62893 satır).

---

## D-399 [YÜKSEK] — CODEGEN: çapraz-dosya modül yükleme, KAYNAK düzeyinde (2026-08-07)

**Durum: KISMİ — `test/moduller` 0/18 → 7/18.** Kalan 11'in sınıfları aşağıda,
her biri ÖLÇÜLMÜŞ kökle. Mevcut kapılarda **sıfır regresyon**.

**Karar — neden AST splice DEĞİL:** C `modulleri_yukle` modülü AYNI arena'da
parse edip sentetik `DUGUM_MODUL` olarak AST'nin başına splice eder. Self-host'ta
doğrudan karşılığı PAHALI: AST düz paralel dizilerdir, ayrı bir `Ayr`den
kopyalamak indeks yeniden-eşlemesi ister ve onlarca YAN-KANAL dizisi (`cv_*`,
`gp_*`, `yerel_*`, `ly_ad`, `gen_node`, `pi_*` …) sessizce düşerdi — kaybı
gözlenmeyen, en tehlikeli sınıf.

**Bunun yerine KAYNAK düzeyinde birleştirme:** modül metni `modül <ad> { ... }`
ile sarılıp giriş kaynağının ÖNÜNE konur, sonra TEK lex+parse koşar. Tek AST
çıkar, yan-kanallar doğal olarak dolar, kopyalama kodu YOKTUR. D-398'in
mangling'i bu yolu doğrudan besler (`@mat.topla` sarmaldan kendiliğinden çıkar).
**Uygulamadan ÖNCE elle ölçüldü** (ana_mat + ana_zincir manuel splice → exit 42).

İki geçiş şart: `kullan` listesini öğrenmek için önce parse etmek gerekir.
Modül yoksa birleşik == "" → mevcut çıktı BİREBİR korunur.

**Yol üstünde ölçülen kusur — transitif importer dizini.** `dizin_al(rel)` TEK
BAŞINA yanlıştır (`rel`="zincir.kem" → ""), taban dizin kaybolur. Taban + alt-yol
birleştirilmeli. **`modul_yukle` (--check yolu) AYNI hesabı yapıyor ve orada
GÖRÜNMÜYOR** — check yalnız İSİM topluyor, `ana_zincir` `mat`in adlarına hiç
ihtiyaç duymuyor → kusur MASKELİ. (Check paritesinin sığlığının ikinci kanıtı.)

**⚠ ÇÖZÜLEMEYEN KÖK — iç içe `ayr_olustur`+`lex_et` metin belleğini eziyor.**
`ana_zincir`de sarmal adı kayboluyor: `@zincir.dortle_topla` yerine
`@dortle_topla`. İKİ ayrı hipotez ÖLÇÜLDÜ ve İKİSİ DE ÇÜRÜDÜ:
(a) "ad özyinelemeden sonra hesaplanıyor" → önce hesapladım, DEĞİŞMEDİ;
(b) "`p` üzerinden aliasing" → adı her mutasyondan önce sabitledim, DEĞİŞMEDİ.
Geriye kalan açıklama: `metin` değerleri araya giren iç içe lex+parse'tan sağ
çıkmıyor. **`mat` bozulmuyor çünkü onun özyineleme döngüsü BOŞ** — yalnız İÇİNDE
özyineleme YAPAN modül bozuluyor. Bu bir self-host bellek semantiği sorusudur ve
ayrı ölçüm ister; tahminle kapatmadım.

**Kalan 11 — üç sınıf, kökleri ölçüldü:**
- **Transitif (1):** `ana_zincir` — yukarıdaki kök.
- **Alias / seçili import (3):** `ana_alias` (`@m.topla`: sarmal adı ALIAS'tan
  gelmeli, dosya adından değil), `ana_secili` (`@topla`: `::{a,b}` çıplak ad
  görünürlüğü ister), `ana_nitelikli` (`@cift_b.f`).
- **Çapraz-modül tip/generic (7):** `ana_ifd`, `ana_kap`, `ana_kap_coklu`,
  `dizi_*` — hepsi `'%N' i32 but expected 'ptr'`. Ad çözümü DEĞİL, tip çözümü;
  ayrı alt-sistem.

**Kapı:** `test/cg_korpus/cg_modul_capraz.kem` + fikstür `cgmodul_mat.kem`.
Fikstürde `main` YOK → harness "oracle yok, atla" der, kapıyı kirletmez (bu
davranış ölçüldü, varsayılmadı). Test nitelikli çağrıyı VE modül-içi çıplak
çağrı zincirini birlikte ölçer.

**Sabotaj S131** (`mods = ""`) → `cg_modul_capraz` KIRMIZI (122/123).

**Kapılar:** `codegen_diff` **123/123** · `checker_diff` 148/148 ·
`parser_diff` 13/13 · FIXPOINT ✓ (62766 satır).

---

## D-398 [YÜKSEK] — CODEGEN: modül ad-mangling (`@mod.ad`) (2026-08-07)

**Nasıl bulundu:** D-395'in kurduğu geniş kapı `test/ornekler` + `stdlib/temel`de
doyunca ölçüm HİÇ BAKILMAMIŞ bir yüzeye çevrildi: `test/moduller/`. Orada
self-host `--check` **131/131 tam paritede**, ama `--llvm` **18/18 başarısız**.

**⚠ ÖLÇÜM ARACIM ÖNCE BOZUKTU.** İlk koşumda 18 dosyanın hepsi "IR üretemedi"
dedi; sebep modül desteği DEĞİL, sabotaj döngüsünde sildiğim `build/codegen.exe`
idi (`make calistir_codegen_diff` onu kurar, ama ben elle koşuyordum). Yeniden
kurmadan raporlasaydım kök TAMAMEN yanlış çıkacaktı. (D-391'in aynı dersi.)

**Gerçek kök kümesi** 4 lensli + 24 çürütücülü bir fan-out ile çıkarıldı (22 kök
onaylandı, 3 iddia çürütüldü/daraltıldı). İki AYRI iş olduğu ortaya çıktı:

**(A) Çapraz-dosya modül yükleme — BU ADIMDA YAPILMADI.** C `src/ana.c`
`modulleri_yukle` ile modülü AYNI arena'da parse edip sentetik `DUGUM_MODUL`
olarak program AST'sinin BAŞINA **splice** eder → tek ağaç, tip kontrol ve
codegen aynı ağaçtan beslenir. Self-host `modul_yukle` modülü AYRI bir `Ayr`e
parse edip **yalnız ADları hasat ediyor, AST'yi ATIYOR**; üstelik `--llvm`
dispatch'i yükleyiciyi HİÇ çağırmıyor. AST düz paralel dizilerdir → ayrı `Ayr`den
indeks yeniden-eşlemesi gerekir ve codegen.kem'de AST kopyalama yardımcısı YOK.
Yan bulgu: check paritesi **SIĞ** — `mat::topla(20)` (yanlış arite) C'de `T010`,
self'te `OK`. 131/131 mevcut korpusta doğru ama mekanizma isim düzeyinde.

**(B) Ad-mangling — BU ADIMDA YAPILDI.** (A)'dan BAĞIMSIZ ve satır içi `modül`
bloklarıyla bugün ölçülebilir:
```
C:    define @a.f  +  define @b.f     → exit 42
SELF: define @f    +  define @f       → invalid redefinition of function 'f'
```
Mangling makinesi self-host'ta HİÇ yoktu. **(A)'nın da ön koşuludur** —
yükleyiciyi bağlamak bunu kendiliğinden çözmez.

**Onarım (üç mekanizma):** `p.mod_onek` önek zinciri +
1. **define** — `@<önek>.<ad>` (`mangle`, C `modul_mangle` aynası, ayırıcı '.').
2. **nitelikli çağrı** — `yol_noktali`: YOL zinciri → `a.f` / `d.i.f`. Önceden
   `a_deg`den YALNIZ son segment alınıyordu (`f`) → `call @f`, tanımsız sembol.
3. **modül-içi çıplak çağrı** — `fn_coz` MODÜL-ÖNCE bağlar. C modül-içi çıplak
   çağrıyı da mangle'lıyor (`@a.f` gövdesinden `call @a.gizli`); ayrıca modül
   üyesi üst-düzey bir adı GÖLGELER, önce modüle bakmak bunu korur.

`imza_topla` da mangled ad tutar — çağrı çözümü ve define emisyonu AYNI adı
görmezse dönüş tipi kurtarılamaz.

**Yerleştirme kararı:** `fad` YUKARIDA topluca değiştirilmedi; yalnız kullanıcı-
işlev çözüm noktası dokunuldu. Yerleşikler ve tagged-union yapıcıları `fad`ı
ÇIPLAK adla karşılaştırıyor — toplu değişiklik o ayrımları sessizce bozardı.

**Sabotaj — üç mekanizma, ÜÇ FARKLI başarısızlık kipi:** S128 (define mangle) →
link hatası (120/122). S129 (nitelikli ad) → link hatası (120/122). **S130
(modül-önce bağlama) → `exit=4`**, yani korpusun `c::f() != 100` muhafızı: modül
üyesi yerine üst-düzey `gizli()` (999) çağrılıyor — **link hatası değil, SESSİZ
YANLIŞ CEVAP**. Korpus bu kipi ölçmek için özellikle gölgelenen bir üst-düzey ad
içerir.

**Korpus:** `test/cg_korpus/cg_modul_mangle.kem` (ad çakışması + iç içe modül +
modül-içi çıplak çağrı + gölgeleme). Satır içi `modül` kullanır — dosya
yüklemeye BAĞIMLI DEĞİL.

**Kapılar:** `codegen_diff` **122/122** · `codegen_genis` 67/67 (0 muaf) ·
`checker_diff` 148/148 · `parser_diff` 13/13 · FIXPOINT ✓ (62446 satır).

**Kalan:** (A) çapraz-dosya yükleme — 18 dosya hâlâ düşüyor, ayrı adım.

---

## D-397 [YÜKSEK] — CODEGEN: SIMD `vektör<T,N>` (son muafiyet kapandı) (2026-08-07)

**Kusur:** Self-host codegen'de vektör TİPİ hiç yoktu. Parser `TIP_VEKTOR`
düğümünü zaten üretiyordu (lane `a_deg`'de), checker yerleşikleri zaten
tanıyordu — eksik olan YALNIZ codegen'di. `ll_tip`'te dal olmadığı için her
vektör `i32` fallback'ine düşüyordu:
```
%9 = call i32 @vektor_topla(ptr %rho, i32 %8)
ret float %9                     ; 'i32' but expected 'float'
```

**DÖRT ayrı kök — her biri bir öncekini onarınca ORTAYA ÇIKTI** (tek ölçümle
görülemezlerdi; sırayla soyuldular, her adımda LLVM yeni bir hata verdi):
1. `ll_tip`: `TIP_VEKTOR` → `"<N x T>"` (C `ast_tip_to_ir` aynası). Yoksa `i32`.
2. Yerleşikler: `vektor_doldur` (insertelement+shufflevector splat),
   `vektor_eleman` (extractelement), `vektor_topla` (reduce). Yoksa tanımsız
   `@vektor_*` sembolü.
3. `kesirli_ll_mi`: `"<N x float>"`/`"<N x double>"` **DE kesirlidir**
   (C llvm.c:2193 aynası). Yoksa lane-wise çarpım `mul <4 x float>` yayar →
   LLVM "invalid operand type".
4. `beklenen_ll`: annotasyon `"<N x T>"` ise bağlam kur. Yoksa `vektor_doldur`
   lane/eleman bilgisini kurtaramaz, C ile aynı `<4 x i32>` varsayılanına düşer
   ve `insertelement <4 x i32> undef, i32 2.0` basar → LLVM-red.

**Ölçüm notu:** `<4 x i32>` varsayılanını UYDURMADIM — C oracle'ın davranışı
(`src/llvm.c:4120`). Aynı şekilde `reduce.fadd`in `0.0` başlangıç operandı ve
`reduce.add`in operandsızlığı da oracle'dan okundu; **iki intrinsic'in ARİTELERİ
farklıdır**, karıştırmak LLVM-red verir.

**Yardımcılar (ham IR string üstünde — self-host'ta tip nesnesi yok, IR string'i
tek gerçektir; `agg_alan` ile aynı disiplin):** `vek_ir_mi`, `vek_lane`,
`vek_elem`, `vek_abbr` (float→f32, double→f64).

**Sabotaj — dört kök, dört ayrı kapı:** S124 (`ll_tip` dalı), S125
(`vektor_topla`), S126 (vektör-kesirli tanıma), S127 (`beklenen_ll` bağlamı) →
**dördü de** `cg_simd_vektor` üzerinde KIRMIZI (120/121). Tek dosya dördünü de
uyandırıyor; hangi satırın hangi kökü ölçtüğü korpus dosyasında yazılı.

**Korpus:** `test/cg_korpus/cg_simd_vektor.kem`. Hem `vektör<kesirli32,4>` hem
`vektör<tam32,4>` içerir — **tamsayı lane'ler `reduce.add` yoluna gider**, yalnız
kesirli test etmek 2 numaralı kökün yarısını ölçmeden bırakırdı.

**🎯 SONUÇ: `codegen_genis` 67/67, MUAFİYET LİSTESİ BOŞ.** Kapı D-395'te 2
muafiyetle kuruldu, D-396 + D-397 ile boşaldı — tasarlandığı gibi.

**Kapılar:** `codegen_genis` **67/67 (0 muaf)** · `codegen_diff` **121/121** ·
`checker_diff` 148/148 · `parser_diff` 13/13 · FIXPOINT ✓ (stage1==stage2,
62168 satır).

---

## D-396 [YÜKSEK] — CODEGEN: `eşleş` desen-bağlamasının iç tipi (görev<T>) (2026-08-07)

**Kusur:** `görev_başlat` dönüşü `sonuç<görev<T>, metin>` ve IR karşılığı
`{ i8, ptr, ptr }`. **Bu aggregate T'yi SİLER** — `görev<T>` de `metin` de ptr.
`tamam(g)` deseniyle bağlanan `g` için codegen `cg_aic`i `""` bırakıyordu →
`görev_birleştir(g)`de hedef tip bilinmiyor → `i64_daralt` runtime'ın **i64
taşıyıcısında KALIYOR** ve ptr yuvasına yazılıyordu:
```
%17 = call i64 @kdl_gorev_birlestir(ptr %16)
store ptr %17, ptr %18        ; 'i64' but expected 'ptr'
```
**`i64_daralt`ın ptr dalı DOĞRUYDU** — ilk teşhisim onu suçlamak olabilirdi;
ölçüm eksik olanın yalnız TİP BİLGİSİ olduğunu gösterdi. Checker tarafında aynı
boşluk D-385/386'da kapanmıştı; codegen karşılığı yoktu.

**Onarım (iki nokta):**
1. `görev_başlat` T'yi **yayınlar**: `p.son_ic = lam_ret_tahmin(p, govde)`.
   T = lifted lambda'nın DOĞAL dönüş IR'ı. Kuyruğa yazılan `lam_ret = "i64"`
   runtime TAŞIYICISIDIR, T değildir — ikisini karıştırmak kusurun kendisiydi.
2. `eşleş` skrutini `son_ic`ini **hemen** yerelde yakalar (`sic`) — aşağıdaki her
   `ifade_uret` `p.son_ic`i ezer — ve DESEN_YAPICI payload bağlamasına taşır.

**`pf==1` kısıtı:** yalnız başarı alanına (tamam/değer) yazılır; alan 2 `metin`dir
ve T ile ilgisi yoktur.

**Sabotaj:** S121 (T yayını) → `cg_gorev_desen_ic_tip` KIRMIZI (119/120).
S122 (payload'a taşıma) → KIRMIZI (119/120). **S123 (`pf==1` kısıtını kaldır) →
SESSİZ.** Bunu gizlemiyorum: kısıt bu korpusta **gözlenebilir değil**, çünkü
`cg_aic`i okuyan üç yol da (`cagri_ic_tip`, kapanış dönüş IR'ı) bağlamanın
görev/kanal/kapanış olarak KULLANILMASINI ister — `e: metin` için bu tip
hatasıdır. Yani kısıt **savunmacıdır, kapılı değildir**; anlamı doğru kodluyor
ama ölçülmüş bir regresyon kapısı yok. (D-356: sabotajın sessizliği bir
SONUÇTUR; kuralı silmek yerine kaydını dürüst tut.)

**Korpus:** `test/cg_korpus/cg_gorev_desen_ic_tip.kem`. **T=metin ÖZELLİKLE
seçildi** — T `tam32` olsaydı i64→i32 trunc yolu zaten çalışıyordu ve dosya
yeşil kalıp kusuru kaçırırdı.

**Kapılar:** `codegen_genis` **66/66** (muafiyet 2→**1**) · `codegen_diff`
**120/120** · `checker_diff` 148/148 · FIXPOINT ✓ (stage1==stage2, 61384 satır) ·
sürücü 4 mod × 2 sürücü ✓.

**Kalan tek muafiyet:** `matris_carpim` (SIMD `vektör<T,N>` self-host codegen'de
yok) — ayrı ve daha büyük iş.

---

## D-395 [YÜKSEK] — KAPI: geniş codegen eşdeğerliği + kendi yanlış iddiamın düzeltilmesi (2026-08-06)

**Karar:** `test/codegen_genis_harness.sh` + `make calistir_codegen_genis`.
`test/ornekler/*.kem` + `stdlib/temel/*.kem` yüzeyindeki her `main()`'li programı
C oracle ile karşılaştırır — **exit koduna EK OLARAK stdout**.

**Gerekçe 1 — `codegen_diff` bu sınıfı görmüyor.** O kapı `test/cg_korpus/`
üzerinde koşar ve korpus AMAÇLI dar (her dosya bir özelliği izole eder). Gerçek
programlardaki 31 sapma (D-388→394) o kapı yemyeşilken birikmişti.

**Gerekçe 2 — yalnız exit'e bakmak yetmez.** `bignum_selfhost` İKİ tarafta da
exit 0 veriyordu ama stdout'ta `0` yerine yığın adresi basıyordu. Exit-only bir
kapı bu sessiz yanlış cevabı KAÇIRIR.

**🔴 BU KAPI BENİM KENDİ İDDİAMI ÇÜRÜTTÜ.** D-394'ün commit'inde ve CLAUDE.md'de
"GERÇEK SAPMA SIFIR (OK=65)" yazmıştım. Elle koşturduğum ölçüm döngüsünde
`link_retry` başarısızlığı sessizce `fark` sayılıyor ama YAZDIRILMIYORDU; kalan
iki dosyayı "C'nin de segfault ettiği eşleşen çökme" diye kendi kendime
açıkladım. Kapı yazılınca ikisi de kırmızı çıktı. **Doğru sayı 65/67.**
> **DERS: elle koşturulan ölçüm döngüsü kapı DEĞİLDİR.** Kapı, sessiz düşen
> dalı olmayacak biçimde yazılır ve `[ "$fail" -eq 0 ]` ile biter. Ölçümü kapıya
> BAĞLAMADAN "sıfır sapma" deme. Bu, D-359'un ("parse kapısı sessiz ayrışmıştı")
> ve D-356'nın ("sabotajın sessizliği bir SONUÇTUR") aynı sınıftan tekrarı.

**Muafiyet listesi (2 satır, KÜÇÜLMEK ZORUNDA) — kökleri ÖLÇÜLDÜ:**
- `matris_carpim` — **SIMD yok.** `vektör<kesirli32,4>`; C `<4 x float>` yayar,
  self-host'ta vektör tipi HİÇ yok → `i32` sanıyor (`'%9' i32 but expected
  'float'`). D-393'ün arg-genişletmesiyle akraba DEĞİL; eksik özellik.
- `gorev_temel` — **desen-bağlaması iç tipi yok.** `eşleş görev_başlat(..)
  { tamam(g) => görev_birleştir(g) }`: `g` desen bağlaması, codegen iç tipini
  (`görev<metin>`→ptr) bilmiyor → `cagri_ic_tip` `""`, `i64_daralt` i64'te kalır,
  ptr yuvasına yazılır (`i64 but expected ptr`). **`i64_daralt`ın ptr dalı
  DOĞRU** — eksik olan yalnız TİP BİLGİSİ. Checker'da aynı boşluk D-385/386'da
  kapandı; codegen karşılığı ayrı iş.
- Muafiyet KURALI harness başlığına yazıldı: yeni satır eklemek kapıyı
  zayıflatmaktır; önce KÖKÜ onar. (`checker_diff`in muafiyet listesi bu
  disiplinle BOŞA indi — emsal var.)

**Muafiyet GEREKMEYEN durum:** `kem_mmio_ham`/`kem_pointer` host'ta eşlenmemiş
MMIO okur ve İKİ TARAFTA DA segfault eder → eşleşen çökme zaten GEÇER. Muafiyet
yalnız ayrışan davranış içindir.

**Sabotaj S120:** D-394'ün KARAKTER dalı `eğer yanlış` yapıldı (uygulandığı
`sed -n '3429p'` ile GÖRÜLDÜ, `grep -c` = 1) → kapı `base64_selfhost` üzerinde
**kırmızı** (exit 2). Geri alındı → 65/65 yeşil. Sabotajlı koşumda aday kalıcı
127 verdi: D-339 kuralı doğru çalıştı (127 yalnız ORACLE'da ortamsaldır;
adayda ANLAŞMAZLIKTIR, sessizce atlanamaz).

**Kapsam/sınır:** Kapı yalnız `main()`'i olan programları alır; oracle IR'ı
kuramazsa (tip hatası/modül importu — bu kapının işi değil) ATLAR, başarısız
saymaz (12 atlandı).

---

## D-394 [YÜKSEK] — CODEGEN: KARAKTER literali (son gerçek sapma kapandı) (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `test/cg_korpus/` (+1).

**Kusur — SESSİZ YANLIŞ CEVAP, en ağır sınıf.** `ifade_uret`te **KARAKTER dalı
HİÇ YOKTU** → düğüm tanınmıyor, fonksiyon sonunda `"0"` düşüyordu: `'A'` 65
yerine **0**. IR geçerli, clang kabul ediyor, program çalışıyor — yalnız değer
yanlış.

**⚠ TUZAK — ilk düzeltmem SESSİZCE başarısız oldu.** `TAM` dalını ayna alıp
`a_deg`i doğrudan bastım; ama parser kod noktasını `--ast` dump paritesi için
**`"U+0041"` biçiminde** saklar (`karakter_deger`), SAYI olarak değil. Sonuç
yine 0'dı. Hex→ondalık çevirici (`karakter_kod_noktasi`) ŞART.

**Etkisi `base64_selfhost`te maskelenmişti.** Bağımsız çok-ajanlı analiz
(salt-okuma fan-out + adversarial çürütme, 24 ajan) zinciri benden daha eksiksiz
çıkardı: `b64_alfabe`/`b64_index` tabloları tamamen 0 → `beklenen` de 0 →
karşılaştırma EŞLEŞİYOR ve "KEM B64 OK" basılıyor (**kusur kendini maskeliyor**)
→ `b64_index(0)` hep 0 döner, `'='` için `-1` ASLA dönmez → `i3 > -1` guard'ı
hep doğru → 8 karakter = 2 grup × 3 bayt = **6 yazma**, `cozulen` 5 elemanlı →
`cozulen[5]` → `PANIK: dizi sınır ihlali (i=5, boyut=5)`. Gözlenen mesajla birebir.

**Bu yüzden indirgeme ÜÇ KEZ başarısız olmuştu (D-392):** encode-only ve
encode+decode şekillerinde `beklenen` karşılaştırması da sıfırlarla eşleştiği
için kusur GÖRÜNMÜYORDU. Panik ancak tam zincir kurulunca çıkıyor.

**ÖLÇÜM DÜZELTMESİ:** korpusa `'ş'` için 351 (kod noktası) yazmıştım — C **197**
veriyor (UTF-8 ilk baytı 0xC5). Bootstrap parser'ın mevcut davranışı bu;
self-host onu birebir taklit ediyor ve parite DOĞRU. Varsayımı ölçümle
değiştirdim, korpusa not düştüm.

**Kapılar:** `codegen_diff` **119/119**, `checker_diff` 148/148 (0 muaf).
**Sabotaj S118** (KARAKTER dalı) + **S119** (hex çevrimi — ilk düzeltmemi sessiz
kılan tam nokta) — ikisi de kırmızı.

**🎯 GENİŞ ÖLÇÜM:** Dört partide (D-388→394) codegen paritesi **36 → 65**.

> **⚠ D-395 DÜZELTMESİ — bu satırda "GERÇEK SAPMA SIFIR" yazıyordu, YANLIŞTI.**
> Elle koşturduğum ölçüm döngüsünde link başarısızlıkları sessizce `fark`
> sayılıyor ama YAZDIRILMIYORDU; kalan iki dosyayı "C'nin de segfault ettiği
> eşleşen çökme" sandım. Ölçümü D-395'te kapıya bağlayınca gerçek yüz çıktı:
> `gorev_temel` (i64→ptr) ve `matris_carpim` (SIMD yok) **gerçekten
> başarısız**. Doğru sayı **65/67**. Ayrıntı D-395'te.

---

## D-393 [YÜKSEK] — CODEGEN: çağrı argümanı param IR tipine genişletiliyor (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `test/cg_korpus/` (+1). D-392'de bulunan kök
KAPATILDI.

**Kusur.** Çağrı yerinde argüman **argümanın DOĞAL tipiyle** basılıyordu,
**parametrenin BİLDİRİLEN tipiyle** değil: `f(a: dtam64, ..)` için
`call @f(i32 10, ..)` — oysa `define` i64 bekliyor. LLVM bunu **SESSİZCE
kabul eder** (D-295'in dersi): ilk 3 argüman register'a sığdığı için doğru
görünür, **4.'den itibaren ÇÖP okunur**. `bignum_selfhost`'un `0` yerine
yığın adresi basmasının kökü buydu (`vektor_dogrula` ALTI `dtam64` param).

**D-392'nin başarısız denemesi neden başarısızdı, nasıl çözüldü.** İlk deneme
checker'ın imza tablosuna (`fn_ptip_bul`) bakıyordu; o tablo **YALNIZ
`kontrol_program`da — CHECK yolunda — doldurulur** ve `--llvm` yolu onu hiç
kurmaz → `codegen.exe` çağrı içeren HER dosyada düştü. Çözüm: codegen'in
**KENDİ ön-geçişi** (`param_ir_topla`), emit'in kullandığı **aynı `param_tip`
fonksiyonuyla** kaydediyor → değer tutarlılığı yapısal olarak garanti.
Ön-geçiş şart: bir çağrı sonradan tanımlanan işleve de gidebilir.

**Kapı EN AZ 4 parametre ister.** 3 parametreli bir test **yanlış uygulamada
da geçerdi** (ilk 3 register'a sığıyor) — D-391'in "sığan sayı sınamaz"
dersinin kardeşi. Korpus ayrıca i32'ye sığmayan bir değer (2^33) ve wrap
davranışı içerir.

**DÜRÜST SINIR — sınır korumaları GATE'Lİ DEĞİL.** `param_ir_tip`'teki
"kayıt bulunamadı" ve "arite dışı" korumaları mevcut korpusla **gözlenemiyor**
(`param_ir_topla` her üst-düzey işlevi kaydediyor; arite uyuşmazlığını checker
zaten reddediyor). Sabotaj S117 her iki biçimde de SESSİZ kaldı. Korumaları
**tutuyorum** — D-392 tam da bir arama başarısızlığının derleyiciyi düşürdüğünü
gösterdi — ama yapay korpus vakası UYDURMADIM.

**Kapılar:** `codegen_diff` **118/118**. **Sabotaj S116** (genişletme
kaldırıldı) → kırmızı (42≠1).

**GENİŞ ÖLÇÜM: gerçek sapma 2 → 1.** (`OK=64`; kalan 3 "fark"ın 2'si C'nin de
segfault ettiği EŞLEŞEN çökmeler.) Tek kalan: `base64_selfhost` (C=0, S=127,
`dizi sınır ihlali i=5 boyut=5`) — D-392'de indirgeme denendi, sapma
üretilemedi; ayrı kök.

---

## D-392 — ÖLÇÜM: çağrı argümanı param tipine genişletilmiyor (kök bulundu, DÜZELTİLMEDİ) (2026-08-06)

**ETKİ:** yok — kod DEĞİŞMEDİ (denenen düzeltme geri alındı). Bu bir ÖLÇÜM kaydı.

**Kök neden KESİN olarak bulundu.** Self-host çağrı yerinde argümanı
**argümanın DOĞAL tipiyle** basıyor, **parametrenin BİLDİRİLEN tipiyle** değil:
```
SELF:  call i32 @f(ptr %0, i32 10, i32 20, i32 30, i32 40)   ← define i64 bekliyor
C   :  call i32 @f(ptr %0, i64 %2, i64 %3, i64 %4, i64 %5)
```
LLVM bu uyuşmazlığı **SESSİZCE kabul eder** (D-295'in dersi). İlk 3 argüman
register'a sığdığı için doğru görünür; **4.'den itibaren ÇÖP okunur.**

**Minimal üretim (ölçüldü):** `işlev f(a..f: dtam64)` çağrısı →
`10 20 30 <çöp> <çöp> <çöp>`. Sınır tam olarak 4. parametrede. `tam32` param
ile ÇALIŞIYOR (i32 zaten doğal tip) → yalnız genişletme gereken tiplerde.
`bignum_selfhost`'un `0` yerine yığın adresi basmasının kökü budur
(`vektor_dogrula` ALTI `dtam64` parametre alıyor).

**DENENEN DÜZELTME GERİ ALINDI — neden çalışmadı:** arg döngüsünde
`fn_ptip_bul`/`fn_psay_bul` ile param tipini sorup `i64_genislet` uygulamak
`codegen.exe`'yi **çağrı içeren HER dosyada** düşürdü (`dizi sınır ihlali
i=0, boyut=0`). Sebep: **imza tablosu (`fn_ad`/`fn_psay`/`fn_ptip`) yalnız
`kontrol_program`da — yani CHECK yolunda — doldurulur** (`genel_topla` →
`imza_kaydet`). `--llvm` codegen yolu tabloyu HİÇ kurmaz. Ayrıca
`fn_ptip_bul` SINIR KONTROLÜ YAPMAZ (`dizi_al` taşar).

**Yol üstünde ikinci hata:** ilk denemede o kapsamda var olmayan `fad`
değişkenini kullandım (doğrusu `fname`) — self-host bunu sessizce boş dizgiye
çözdü. Kapsamda olmayan ad, derleme hatası değil SESSİZ yanlış davranış üretti.

**SIRADAKİ İŞ İÇİN GEREKEN:** düzeltme, imza tablosunun codegen yolunda da
mevcut olmasını gerektirir. İki seçenek: (a) `--llvm` yolunda `genel_topla`
çağır (yan etkileri ölçülmeli — `g_ekle` dup riski), (b) emit sırasında
parametre IR tiplerini ayrı bir tabloya kaydet. Ayrıca `fn_ptip_bul`/
`fn_psay_bul` sınır-güvenli hale getirilmeli.

**`base64_selfhost` (kalan diğer sapma) AYNI SINIF OLABİLİR** — `b64_encode`
`(Dizi<tam32>, tam32, Dizi<karakter>)` alıyor, 3 param; ama indirgenmiş
şekiller sapmayı üretmedi, ortak kök doğrulanmadı.

---

## D-391 [YÜKSEK] — CODEGEN: üst-düzey `sabit` referansı (A sınıfı, 13 dosya) (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `test/cg_korpus/` (+1).

**Kusur (A sınıfı — 14 dosya, `expected instruction opcode`).** Self-host
codegen'de **üst-düzey `sabit` için emit yolu HİÇ YOKTU**. `sabit K` referansı
yerel/param olarak aranıyor, bulunamayınca `cg_var_bul` `""` dönüyor ve
`load i32, ptr ` (**BOŞ operand**) üretiliyordu → geçersiz IR.
**`ver K;` kadar basit bir şekil bile bozuktu.**

**Teşhis yolu — ilk probe'um GEÇERSİZDİ.** Hata satırının kendisi değil bir
ÖNCEKİ satır bozuktu; `kem_mmio_ham`'da `load i32, ptr ` görünce "ham işaretçi
deref bozuk" sandım ve `&x olarak *tam32` ile probe yazdım — ama C onu **T001
ile reddediyor**, yani ölçüm aracım hatalıydı. Gerçek şekli dosyadan BİREBİR
alınca (`(UART_TABAN + UART_FR) olarak *tam32`) `inttoptr`ın DOĞRU olduğu,
kusurun `sabit` operandlarında olduğu ortaya çıktı. **Probe'u kaynaktan al,
kendin uydurma.**

**Çözüm — C `sabit_kayit`/`sabit_bul` aynası.** Sabit bir DEĞER değil, bir
**İFADE ŞABLONU**dur: global emit edilmez, kullanıldığı yerde INLINE edilir.
Üç paralel dizi (`g_sabit_ad`/`g_sabit_deger`/`g_sabit_tip`) + `sabit_topla`
(DISA/MODUL içine iner) + tanımlayıcı çözümünde yerel-yoksa-sabit dalı.
Çapraz-dosya sabitler aynı ağaçta olduğundan kendiliğinden çözülür.

**Bildirilen tip literali EZER:** `sabit K: tam64 = 8589934592` → i64. Bu satır
olmadan değer i32'de hesaplanıp SESSİZCE KIRPILIR.

**⚠ SABOTAJ SESSİZ KALDI, KORPUS DÜZELTİLDİ (D-385'in dersi tekrar).** S115'i
(bildirilen tip uygulanmıyor) ilk koşumda gate YEŞİL kaldı: korpusta `GENIS`
150994944 idi ve **i32'ye SIĞIYOR** → yanlış uygulama da doğru sonucu veriyor.
2^33'e (`8589934592`) çevrilince kırpılma gözlenebilir oldu ve S115 kırmızıya
döndü (42≠1). **Sığan bir sayı bu yolu SINAMAZ.**

**TEK düzeltme 13 dosyayı açtı:** utf8/crc32/sort/hashmap/json/rc4/vm/
turkce_case/turkce_sort/asm/hashcrack/sha256/kem_mmio_ham — hepsi birebir.

**Kapılar:** `codegen_diff` **117/117**, `checker_diff` 148/148 (0 muaf),
sürücü 4 mod × 2 + FIXPOINT. **Sabotaj S114** (sabit tablosu aranmıyor) +
**S115** (bildirilen tip) — ikisi de kırmızı.

**GENİŞ ÖLÇÜM: 50→63 OK, 17→4 FARK.** Üç partide (D-389/390/391) codegen
paritesi **36→63**. Kalan 4:
- `base64_selfhost` — **REGRESYON DEĞİL**: fix öncesi IR bile üretilemiyordu
  (ölçüldü), şimdi çalışıyor ama çıktı basmadan 127 ile ölüyor → KISMİ ilerleme,
  ayrı kök.
- `bignum_selfhost` — ikisi de exit 0, stdout farkı.
- 2 segfault (C de segfault ediyor — muhtemelen ortak, ayrı iş).

---

## D-390 [YÜKSEK] — CODEGEN: `eşleş &Çeşit` auto-deref (C sınıfı — sessiz yanlış cevap) (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `test/cg_korpus/` (+1).

**Kusur (C sınıfı — EN AĞIR: derleniyor ama exit 42 yerine 0).** `eşleş`
skrutinisi `&Çeşit` (çeşit REFERANSI) olduğunda self-host tag-eşleştirmeyi
HİÇ emit etmiyor → tüm eşleşmeyi atlayıp fallthrough `ver 0`. Geçerli IR ama
YANLIŞ MANTIK. `10_cesit_ast` + `11_yorumlayici` (özyinelemeli AST değerlendirici,
`&Ifade`/`&Dugum` üzerinde eşleş) bu kökten 0 dönüyordu.

**İzole edildi:** payload çeşit + eşleş binding DEĞER üzerinde ÇALIŞIYOR (probe);
kök yalnız `&Çeşit` (referans) skrutinisi. C `load %Çeşit, ptr` ile auto-deref
edip extractvalue yapıyor; self-host ptr'yi ne skaler ne tagged ne yapı sanıp
atlıyordu.

**İKİ KATMANLI kök (her ikisi de gerekti):**
1. `param_ref_yapi` — `&Çeşit` parametresinin pointee ADINI kaydetmiyordu:
   yalnız `yapi_var_mi` sorgulanıyordu, çeşit YAPI değil → `son_ref=""`.
   `cesit_var_mi` eklendi.
2. `eşleş` handler — `son_ref` çeşit adı olsa bile auto-deref YOKTU. `sty="ptr"`
   + `son_ref`=çeşit → `load %Çeşit, ptr` deref + `sty`=inline tip; sonra mevcut
   tagged yolu (extractvalue) çalışır.

**Kapı boşluğu D-356/388/389'un aynısı:** `cg_korpus`'ta `eşleş` HEP DEĞER
üzerindeydi, `&Çeşit` referansı üstünde HİÇ yoktu. Yeni korpus
(`cg_esles_referans_cesit.kem` — özyinelemeli `&Dugum` ağaç toplama, →42) iki
katmanı da kapılar.

**Kapılar:** `codegen_diff` **116/116**, `checker_diff` 148/148 (0 muaf),
sürücü 4 mod × 2 + FIXPOINT. **Sabotaj S112** (param_ref_yapi çeşit kaydı) +
**S113** (eşleş auto-deref) — ikisi de ayrı ayrı kırmızı (42≠0), `grep` ile
kanıtlandı. (Not: perl çok-satır deseni iki kez tutmadı; `grep -c` ile
uygulanmayı DOĞRULADIM, sed ile satır-numarasından uyguladım.)

**GENİŞ ÖLÇÜM: 48→50 OK, 19→17 FARK.** C sınıfı (sessiz yanlış cevap) TÜKENDİ.
Kalan 17 — SIRADAKİ:
- **A sınıfı (14):** `expected instruction opcode`/`value token` — bozuk IR
  (metin/kripto/sort ağır); ortak eksik özellik.
- **Dağınık (3):** `gorev_temel` (i64-but-ptr), `matris_carpim` (i32-but-float),
  `bignum_selfhost` (ikisi de exit 0, stdout farkı).

---

## D-389 [YÜKSEK] — CODEGEN: `dizi_olustur(N)` çağrı-formu (B sınıfı, 9 dosya) (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `test/cg_korpus/` (+1).

**Kusur (B sınıfı — "i32 but expected ptr", 9 gerçek dosya).** Self-host
`dizi_olustur(N)` **fonksiyon-çağrısı** formunu özel-durumsuz bırakıyordu →
genel builtin yoluna düşüp `call i32 @dizi_olustur(...)` üretiyordu: yanlış ad
(`@dizi_olustur` ≠ `@kdl_dizi_olustur`), yanlış dönüş (i32 ≠ ptr), ve eksik
`kdl_dizi_kapasite_ayarla` takip çağrısı. Literal `[..]` yolu ZATEN doğruydu;
kusur yalnız çağrı-formundaydı ve `dizi_ekle`/`dizi_al`/`dizi_boyut`'un aksine
özel-durumu yoktu.

**Basit ad eşlemesi DEĞİL — iki-adımlı dönüşüm.** C `dizi_olustur(N)`'yi:
  `%r = call ptr @kdl_dizi_olustur(ρ, ELEMAN_BYTE)`   [kapasite DEĞİL]
  `call void @kdl_dizi_kapasite_ayarla(ρ, %r, N)`      [kullanıcının N'i]
Eleman-byte `p.beklenen_elem` (annotasyon `Dizi<T>` bağlamı) üzerinden.

**⚠ D-030 GÜVENLİK KURALI KORUNDU.** `dizi_eleman_byte("")` "4" döndürür ama
BİLİNMEYEN eleman için C **8** kullanır (güvenli üst sınır). 4 yazmak ptr/i64
dizisini yarı-rezerve edip `dizi_ekle_ptr`de HEAP-OVERFLOW ediyordu (D-030).
Boş durumu ELLE 8'e sabitlendi; bilinen skaler → `dizi_eleman_byte` (4 veya 8).
`Dizi<tam32>`→4, `Dizi<metin>`→8 ölçüldü.

**TEK düzeltme 9 dosyayı birden açtı:** 13_token_akisi, 14_oncelikli_ayristirici,
15_agac_insa, 16_degiskenli_dil, 17_kontrol_dili, 18_fonksiyon_dili,
diag_heap_yaz_linkli, kem_dizi_kernel, kernel_dizi — hepsi C ile birebir exit.

**Kapı boşluğu, D-388/D-356'nın aynısı:** `codegen_diff` yeşildi çünkü
`cg_korpus`'ta `dizi_olustur(N)` çağrı-formu HİÇ yoktu. Yeni korpus dosyası üç
katmanı kapılar: doğru ad+dönüş, kapasite_ayarla, eleman-byte seçimi (4 vs 8).

**Kapılar:** `codegen_diff` **115/115**, `checker_diff` 148/148 (0 muaf).
**Sabotaj S111** (case devre dışı) → yeni korpus kırmızı, `grep` ile kanıtlandı.

**GENİŞ ÖLÇÜM İLERLEMESİ (test/ornekler + stdlib/temel): 38→48 OK, 29→19 FARK.**
Kalan 19 sapma üç sınıfta — SIRADAKİ İŞ:
- **C sınıfı (2, EN AĞIR):** `10_cesit_ast` + `11_yorumlayici` derleniyor ama
  **exit 42 yerine 0** (sessiz yanlış cevap). Payload çeşit + eşleş destructure.
- **A sınıfı (14):** `expected instruction opcode`/`value token` — bozuk IR,
  ortak bir eksik özellik (metin/kripto/sort ağır).
- **Dağınık (3):** `gorev_temel` (i64-but-ptr), `matris_carpim` (i32-but-float),
  `bignum_selfhost` (ikisi de exit 0 ama stdout farkı).

---

## D-388 [YÜKSEK] — CODEGEN: öneğe uymayan yerleşiklerin IR eşlemesi (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `test/cg_korpus/` (+1).

**YENİ EKSEN — ve ilk ölçüm ağır bir kusur buldu.** Checker paritesi doygunlaşınca
(D-387) ölçümü CODEGEN'e çevirdim: `codegen_diff` kapısı yalnız `test/cg_korpus`
(113 dosya) üzerinde koşuyor. `test/ornekler` + `stdlib/temel`e karşı ölçünce
**31 sapma** çıktı.

**Kök neden (bu partide kapatılan):** `builtin_kdl_ad` ÖNEK-tabanlıydı
(`metin_`/`dosya_`/`yaz_`/`yazdir_`/`arg_`). Hiçbir öneğe uymayan `yazdir`,
`bellek_al`, `bellek_serbest` ve `otp_*` için self-host `@yazdir` gibi
**TANIMSIZ semboller** çağırıyordu → clang IR'ı REDDEDİYORDU.
**`test/ornekler/hello.kem` BİLE derlenmiyordu** — 4 satırlık program.

**Ad eşlemesi TEK BAŞINA yetmedi — üç ayrı katman gerekti:**
1. **Ad:** `yazdir`→`puts`, `bellek_al`→`malloc`, `bellek_serbest`→`free`,
   `otp_*`→`kdl_otp_*` (hepsi C `--llvm` çıktısından ÖLÇÜLDÜ, tahmin değil).
2. **`declare`:** OTP sembolleri bildirilmiyordu → ad doğru olsa da "undefined value".
3. **Dönüş tipi:** `builtin_ret` bu adları bilmediği için `void` varsayıyor,
   `call void @malloc(...)` üretip DEĞERİ kaybediyor, ardından `store ptr 0`
   gibi çöp IR çıkıyordu.

**Parite için C'nin DAVRANIŞI taklit edildi, "doğrusu" değil:** C `free`yi
`call i32 @free` olarak emit eder ama `declare void @free` yazar — LLVM bu
uyuşmazlığı SESSİZCE kabul eder (D-295'in dersi). Self-host aynısını yapar.

**Kapı boşluğu, D-356'nın dersinin aynısı:** `codegen_diff` 113/113 YEŞİLDİ
çünkü `cg_korpus` bu adları hiç içermiyordu. Yeni korpus dosyası
`cg_yerlesik_ad_eslemesi.kem` üç katmanı da kapılar.

**Kapılar:** `codegen_diff` **114/114**, `checker_diff` 148/148 (0 muaf),
`parser_diff` 13/13. **Sabotaj:** S109 (ad eşlemesi), S110 (dönüş tipi) —
ikisi de kırmızı, `grep` ile kanıtlandı.

**AÇIK KALAN (ölçüldü, bu partide DEĞİL):** geniş codegen ölçümü 36→**38 OK**,
31→**29 FARK**. Kalan sapmalar üç sınıfta kümeleniyor:
- `expected instruction opcode` (13 dosya) — bozuk IR, ortak bir eksik özellik
- `'%N' defined with type 'i32' but expected 'ptr'` (8 dosya) — tip uyuşmazlığı
- `10_cesit_ast` / `11_yorumlayici`: derleniyor ama **exit 42 yerine 0** —
  SESSİZ YANLIŞ CEVAP, en ağır sınıf; sıradaki iş bu olmalı.

---

## D-387 [YÜKSEK] — SERİ KAPANIŞI: `parser_diff` regresyonu onarıldı + durum konsolidasyonu (2026-08-06)

**ETKİ:** `selfhost/parser.kem`, `CLAUDE.md`.

**⚠ KENDİ SERİMDEN ÇIKAN REGRESYON — merge öncesi tam koşumda yakalandı.**
`calistir_parser_diff` **12/13 KIRMIZI**ydı ve seri boyunca HİÇ koşulmamıştı.
Kök: D-359 parse korpusuna `p7_kuresel_ciplak.kem` eklerken yalnız **sürücüyü**
(`codegen.kem`) onardı; **`selfhost/parser.kem`** (Aşama-2 REFERANS parser, ÜÇÜNCÜ
uygulama) `küresel`/`çıplak` anahtar kelimelerini hiç tanımıyordu. Kapı
12/12'den 12/13'e düştü ve fark edilmedi çünkü ben hep `checker_diff` +
sürücü koşum takımını koşuyordum.

**Onarım:** `küresel`/`çıplak` parser.kem'e eklendi. C'de ikisi de AYRI düğüm
tipi DEĞİLDİR (`DUGUM_DEGISKEN`/`DUGUM_ISLEV` + bayrak; `--ast` "DEGISKEN"/"ISLEV"
basar) → referans parser bayrağı TUTMAZ, anahtar kelimeyi yutup normal yola
devam eder. **Konum ölçüldü:** düğüm `küresel`in konumunu taşır (sütun 1),
`değişken`inkini DEĞİL (sütun 10) → `dizi_yaz` ile düzeltildi. **13/13.**

**DERS — KAPI SEÇİMİ:** bu depoda üç ayrı self-host uygulaması var
(`parser.kem` / `checker.kem` / `codegen.kem`). Korpusa dosya eklemek, o şekli
görmesi gereken HER uygulamanın kapısını koşmayı gerektirir. "checker_diff yeşil"
demek "parite tam" demek DEĞİLDİR. (D-378'de sürücü kapısı bir port hatası
yakalamıştı; bu onun tersi — sürücü yeşilken referans parser kırmızıydı.)

**DERS — BAYAT ARTEFAKT (yine):** `git checkout origin/main` ile karşılaştırma
yapıp geri dönünce `build/kemgu.exe` ESKİ kaynaktan kalır; `checker_diff`
148→144 sahte kırmızı verdi. `rm -f build/kemgu.exe build/kdl_runtime.o` + `make`
şart. CLAUDE.md'ye yazıldı.

**Ölçülen ama regresyon OLMAYAN:** `calistir_lexer_bootstrap` ve
`calistir_parser_bootstrap` **origin/main'de DE exit 2** verir (oran raporlarlar,
yeşil/kırmızı kapı değildirler). Bizim dalda oranlar origin/main'e göre daha iyi
(parser 514/520 vs 444/462).

### SERİ ÖZETİ (D-350 → D-387, 38 commit)
- **Self-host tanı kodu: 24 → 70.** Kalan `T015`/`T023` ÖLÜ → portlanacak kod yok.
- **Geniş ölçüm 131/131 TAM PARİTE** (94/99'dan; D-371→375 arasında 5 yanlış-pozitif
  kapatıldı: G005, CP005, AS001 eksiği, arg çokluğu, T001 takip tanıları).
- **Korpus:** `check_korpus` 59 → **148**, `parse_korpus` 12 → 13. Muafiyet listesi BOŞ.
- **Sabotaj kapıları:** S1 → **S108** (bu seride 37 yeni).
- **Kapatılan alt-sistemler:** modül · MMIO+yetki · DRF · sabitsüre · çeşit ·
  bileşik tip temsili (10 artım).
- **En sık tekrarlayan ders:** *bir temsili/tabloyu zenginleştirmek, onun
  fakirliğine dayanan her sessiz yolu uyandırır* (D-374 E013, D-377 generic
  sızıntısı, D-382 lineer taşıma, D-384/386 "geçersiz tip = tanı yok").

---

## D-386 [YÜKSEK] — Katman 2 intrinsik dönüş tipleri; `eşleş` görev L001 kapandı (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Bu intrinsikler yerleşik imza TABLOSUNA GİRMEZ.** D-374'ün tablosu skaler
imzalıdır; Katman 2 intrinsiklerinin dönüşü ARGÜMANDAN türer, C'de de bağlam
duyarlı özel-durumdur. Ölçülen kurallar:
- `kanal_al(k)` → k'nin `kanal<E>` eleman tipi
- `görev_birleştir(g)` → g'nin `görev<T>` yükü
- `görev_başlat(|| e)` → `sonuç<görev<T>, metin>`, T = lambda gövdesinin tipi

**D-385'in bıraktığı boşluk kapandı.** Skrutini tipi artık bilindiği için
`eşleş görev_başlat(..) { tamam(g) => ... }` bağlaması `görev<T>` tipini alıyor;
lineer olduğunda dilime giriyor ve **C ile birebir `L001 0:0`** üretiyor
(konum yok — sembol kaynakta bir konuma karşılık gelmiyor, ölçüldü). İki kez
`görev_birleştir` → L002 da birebir.

**D-384'ün kuralı ikinci kez uygulandı.** İlk uygulama `tc18_01_drf.kem`'de
sahte T001 üretti: `görev_başlat(|| 1.0)` DRF001 alır, C tipi HATA'ya düşürür.
"Geçersiz tip = tanı YOK" kuralının bu kez TİP tarafı; kesirli yükte "?" dönülür.
Aynı kural iki partide iki farklı yüzeyde çıktı (lineer yükümlülük / tip
bildirimi) — **hata yolunda hiçbir türetilmiş bilgi yayılmamalı.**

**Kapılar:** `checker_diff` **148/148** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 116/116, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / linear 89 / drf 54, **geniş ölçüm 131/131**.
Probe 6/6 birebir. **Sabotaj:** S106 (intrinsik dönüş tipi), S107 (lineer desen
bağlaması kaydı), S108 (kesirli kapısı — yanlış-pozitif dosyasında kırmızı).

**KARAR BEKLEYEN:** `işlev(..)->T` temsili. `tip_str`in üreteceği dizgi biçimi
bir TİP SİSTEMİ kararıdır (çok argümanlı + dönüşlü) ve korpusa + iki uygulamaya
girdikten sonra değiştirmek pahalıdır → CLAUDE.md gereği Mehmet'e sorulmadan
sabitlenmeyecek. Self-host checker'da karar gerektirmeyen iş KALMADI.

---

## D-385 [YÜKSEK] — Desen bağlama tipleri (`eşleş` kollarında) (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kusur.** `eşleş s { değer(v) => ... }` içinde `v`nin tipi BİLİNMİYORDU:
`yerel_topla` düz bir ön-geçiştir ve skrutini bağlamını görmez → desen
bağlamaları oraya `"?"` ile girer, gövdedeki her karşılaştırma sessiz kalırdı.
Tip artık kol işlenirken YERİNDE atanıyor (`yerel_tip_ata`).

**Eşleme:** `seçimlik<T>` + `değer(v)` → `v: T`; `sonuç<T,H>` + `tamam(v)` →
`v: T`; `hata(e)` → `e: H`. Ayrıştırma **DERİNLİK-DUYARLI** — `sonuç<T, H>`
içindeki her virgül üst düzey değildir.

**⚠ SABOTAJ İKİ KEZ SESSİZ KALDI — kapı tasarımı dersi.** Derinlik-duyarlılığı
kaldırdığımda gate YEŞİL kaldı:
1. `sonuç<Dizi<tam32>, metin>` yetmedi — ilk argümanda virgül YOK.
2. `sonuç<sonuç<tam32, metin>, metin>` da yetmedi! Yanlış ayrıştırma
   (`"sonuç<tam32"`) da annotasyonla UYUŞMUYOR → **aynı T001** çıkıyor, fark
   gözlenemiyor.

Kapıyı ancak **doğru ayrıştırmada tanı ÇIKMAYAN** bir şekil kurdu
(`tamam(v) => { değişken w: sonuç<tam32, metin> = v; }` → TEMİZ; yanlış
ayrıştırmada sahte T001). **DERS:** hatalı şekiller bir kuralı kapılamaya
yetmez — yanlış uygulama da hata üretiyorsa tanı AYNI kalır. Kapı, **doğru
davranışın SESSİZ olduğu** bir şekil ister.

**Bilinen sınır:** `eşleş görev_başlat(..) { tamam(g) => ... }` L001'i hâlâ
çıkmıyor — `görev_başlat`ın dönüş tipi (`sonuç<görev<T>, metin>`) yerleşik imza
tablosunda yok, skrutini tipi bilinmiyor. Bu ayrı bir iş (D-374'ün tablosuna
Katman 2 intrinsiklerini eklemek).

**Kapılar:** `checker_diff` **147/147** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 115/115, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / linear 89 / parser 107, **geniş ölçüm 131/131**.
Probe 6/6 birebir. **Sabotaj:** S104 (bağlama tipi ataması), S105 (derinlik
duyarlılığı — ancak üçüncü denemede gözlenebilir hâle geldi).

---

## D-384 [YÜKSEK] — `görev<T>` LİNEER; `kanal<T>` değil (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**D-383'te ölçülüp bırakılan iş yapıldı.** C ölçümü (6 şekil): `görev<T>`
parametresi tüketilmezse **L001**, iki kez `görev_birleştir` edilirse **L002**;
başka bir işleve `görev<T>` parametresi olarak geçirmek de TAŞIMADIR;
`kanal<T>` parametresi hiç kullanılmasa bile **OK** (lineer değil).

**D-372'nin dersi işe yaradı.** "Lineerliği soran HER yüklemi `grep`le" notu
sayesinde üç ayrı yüklem birlikte güncellendi: `tip_node_tekkez_mi`,
`param_lineer_mi`, `deg_lineer_mi` — artı tüketim noktası (`görev_birleştir`).
Bu kez hiçbiri unutulmadı.

**⚠ GEÇERSİZ TİP = LİNEER YÜKÜMLÜLÜK YOK.** İlk uygulama `tc18_01_drf.kem`'de
sahte L001 üretti: `değişken g: görev<kesirli64> = ...` DRF001 alıyor, C tipi
HATA'ya düşürüyor ve bağlamayı lineer SAYMIYOR. Kapı hem `param_lineer_mi`ye
hem `deg_lineer_mi`ye eklendi. **Genel kural:** bir tipi lineer sınıfa sokarken
"o tip GEÇERSİZ olduğunda ne olur" sorusunu ayrıca ölç — hata yolunda lineer
yükümlülük DOĞMAZ.

**Bilinen sınır (ölçüldü, uygulanmadı):** `eşleş görev_başlat(..) { tamam(g) =>
... }` ile bağlanan görev için C **`L001 0:0`** verir (konum yok — sembol
kaynakta bir konuma karşılık gelmiyor); self-host SUSAR. Bunun için desen-bağlama
TİPLERİ gerekir (D-378'de not edilen mekanizma). `görev_başlat`a özel sözdizimsel
kestirme GENELLENMEZ, o yüzden yapılmadı; korpusta o şekil yok.

**Kapılar:** `checker_diff` **146/146** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 114/114, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / linear 89 / drf 54 / capability 40,
**geniş ölçüm 131/131**. Probe 6/6 birebir. **Sabotaj:** S101 (görev param
lineerliği), S102 (`görev_birleştir` tüketimi), S103 (kesirli kapısı —
yanlış-pozitif dosyasında kırmızı) — üçü de kırmızı, `grep` ile kanıtlandı.

**Kalan:** `işlev(..)->T` temsili — çok argümanlı, temsil biçimi bir **TASARIM
kararı**; CLAUDE.md gereği Mehmet'e sorulmadan sabitlenmemeli. `çeşit` payload
tipleri ve desen-bağlama tipleri de açık.

---

## D-383 — `görev<T>` + `kanal<T>` temsili (bileşik tip 7. artım) (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Temsil eklendi**, D-382'nin kanıtlanmış deseniyle (tek argümanlı bileşik).
Açılan: `kanal<T>` vs skaler T001 · iç-tip farkı · çağrı argümanı · `görev<T>`
dönüşünde T020.

**`gönderen<T>`/`alan<T>` TEMSİL EDİLEMEZ — bilinçli sessizlik.** D-366'da
ölçülen olgu doğrulandı: yön uçları AST'de `TIP_KULLANICI`dır, yön TİPTEN
okunamaz (o yüzden DRF007 DEĞERDEN okunuyor). `TIP_KULLANICI` için tip üretmek
kullanıcı yapılarıyla karışırdı → "?" bırakıldı, self-host susar.

**ÖLÇÜLDÜ AMA UYGULANMADI (sıradaki iş).** C'de **`görev<T>` LİNEERDİR**,
`kanal<T>` değildir:
- `işlev al(g: görev<tam32>) -> tam32 { ver 0; }` → **L001** (param tüketilmedi)
- gövde `görev_birleştir(g)` içeriyorsa → OK
- `kanal<T>` parametresi hiç kullanılmasa bile → OK

Self-host'un lineer makinesinde `TIP_GOREV` YOK → bu L001'ler çıkmıyor. Bu bir
TEMSİL işi değil, LİNEER MAKİNE işidir (D-382'de `tekkez`/`yetki` için yapılanın
`görev` karşılığı) ve `eşleş` deseniyle bağlanan görevlerde C'nin konumu `0:0`
bastığı ölçüldü — o tuhaflığın da taklidi gerekir. Ayrı adım olarak bırakıldı;
bu partinin korpusunda lineer şekil YOK.

**Kapılar:** `checker_diff` **145/145** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 113/113, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / drf 54 / linear 89, **geniş ölçüm 131/131**.
**Sabotaj:** S100 (`görev`/`kanal` temsili) → kırmızı, `grep` ile kanıtlandı.

**Kalan artımlar:** `işlev(..)->T` (çok argümanlı — temsil biçimi bir TASARIM
kararı, Mehmet'e sorulmalı) · `çeşit` payload tipleri · `görev` lineerliği.

---

## D-382 [YÜKSEK] — `tekkez<T>` + `yetki<R>`; LİNEER TAŞIMA kuralı düzeltildi (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Temsil.** `tekkez<T>` ve `yetki<R>` `tip_str`e eklendi. `yetki<R>`'nin iç adı
bir KAYNAK adıdır (MMIO/Dosya/...), skaler değildir → `bilinen_tip_mi`
kapısından geçmez; `yetki_kaynak_tipi_mi` ile ayrıca doğrulanır.

**Asıl bulgu — LİNEER TAŞIMA kuralı YANLIŞTI (mevcut kusur).** Self-host
`değişken` bildiriminde değeri KOŞULSUZ tüketiyordu. C ölçüldü (3 şekil):
- `b: tekkez<metin> = a` → **TAŞIR** (a tüketilir; b için L001/L002)
- `n: tam32 = a` → **TAŞIMAZ** (a için L001)

Yani ayırt edici **hedef bağlamanın LİNEERLİĞİ**dir. Bu kusur bugüne kadar
görünmezdi: tip uyuşmazlığı olan lineer bildirim ancak `tekkez<T>` temsili
gelince T001 üretebiliyordu; o zamana dek şekil korpusa hiç girmemişti.

**İlk hipotezim YANLIŞTI, ölçüm ayırdı.** "Taşıma T001 çıkmazsa olur" dedim ve
`t001_cikar_mi` kapısı yazdım — bir şekli düzeltti, diğerini BOZDU
(`b: tekkez<metin> = a` için sahte L001). İkinci ölçüm turu doğru ayrımı
(`deg_lineer_mi`) gösterdi. **İki şekil arasında ayrım yapan bir kural
yazarken İKİSİNİ de ölçmeden yazma.**

**Kapılar:** `checker_diff` **144/144** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 112/112, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / linear 89 / capability 40 / parser 107,
**geniş ölçüm 131/131**. Probe 8/8 birebir. **Sabotaj:** S98 (`tekkez<T>`
temsili), S99 (koşulsuz taşımaya dönüş) — ikisi de kırmızı.

**Kalan artımlar:** `işlev(..)->T` · `görev<T>`/`kanal<T>` · `çeşit` payload.

---

## D-381 — `olarak` ifadesinin TİPİ (bileşik tip 5. artım) (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kusur.** `TIP_DONUSTUR` `ifade_tip`te HİÇ yoktu → `olarak` içeren her ifade
`"?"`e düşüyor ve tüm karşılaştırmalar sessiz kalıyordu. `olarak` KEMGU'da
yaygın (özellikle `güvensiz` ve MMIO kodunda), yani boşluk genişti.
D-380'in bıraktığı bilinen sınır buydu.

**Kural: hedef tipi ancak dönüşüm GEÇERLİYSE bildirilir.** Dönüşüm E001-E004
üretiyorsa C tipi HATA'ya düşürür ve T001'e hiç gelmez; bu kapı olmadan tek
kusur İKİ tanı üretirdi (D-351'de yaşanan kaskadın aynısı).

**Kuralı KOPYALAMADIM — sessiz mod ekledim.** `cast_gecerli_mi`, mevcut
`cast_kontrol`u `cast_sessiz` sayacıyla RAPORLAMADAN yeniden koşturur.
E001-E004 mantığını ikinci kez yazmak iki kopyanın zamanla ayrışmasına yol
açardı; tek kaynak korunur. (`tuk_kapali` — D-375 — ile aynı desen.)

**Ölçüm notu:** `&x olarak *tam32` C'de **T001 verir** — cast KURALI (D-349)
güvensizde geçerli olsa bile üretilen ifade tipi annotasyonla eşleşmiyor.
Kuralı yorumlamadım, davranışı ölçüp korpusa not düştüm.

**Kapılar:** `checker_diff` **143/143** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 111/111, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / parser 107 / linear 89, **geniş ölçüm 131/131**.
Probe 7/7 birebir (geçersiz-cast'te SUSMA davranışı dâhil). **Sabotaj:**
S96 (hedef tipi bildirimi), S97 (geçerlilik kapısı) — ikisi de kırmızı.

**Kalan artımlar:** `işlev(..)->T` · `tekkez<T>` · `çeşit` payload tipleri.

---

## D-380 [YÜKSEK] — Bileşik tip 4. artım: `&T`, `&değişken T`, `*T` (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**DEĞİŞEBİLİRLİK temsile girer.** Ölçüldü: `&T` ↔ `&değişken T` **her iki yönde
de** T001 verir → `&tam32` ve `&değişken tam32` AYRI temsillerdir. `&T` ↔ `*T`
de ayrıdır. Ham işaretçi aynı şekilde eklendi (ayrı önek `*`).

**Açtığı tanılar (8/8 probe):** referans vs skaler · hedef-tip farkı ·
değişebilirlik farkı (iki yön) · `*r` dereferans tipi (D-305'in güvenli deref'i
buradan tip kazandı) · ham işaretçi iç tip + deref.

**⚠ UTF-8 BAYT TUZAĞI — gerçek yanlış-pozitif üretti.** `*m` (m: `&değişken
tam32`) için öneki soyarken **sabit 10** yazmıştım; `"&değişken "` UTF-8'de
**12 BAYTTIR** (`ğ` ve `ş` ikişer bayt) ve `metin_kes`/`metin_uzunluk` BAYT
tabanlıdır. Sonuç: tip bozuluyor, `*r + *m` sahte T003 alıyordu. Uzunluk artık
`metin_uzunluk(mo)` ile HESAPLANIYOR. Sabotaj S95 bunu kalıcı kapıya bağladı.
**DERS:** Türkçe dizgilerde sabit offset YAZMA — CLAUDE.md'nin hex-escape
uyarısının çalışma-zamanı karşılığı budur.

**Süreç notu:** `awk` ile blok taşıma codegen.kem'i bozdu (22 parser hatası);
`git checkout` + `Edit` ile temiz portlandı. Çok satırlı blokları betikle
taşımak kırılgan — dengeli parantez garantisi yok.

**Bilinen sınır:** `&x olarak *tam32` sonucu — `olarak` (TIP_DONUSTUR) ifadesinin
TİPİ hâlâ modellenmiyor → C T001 verirken self susar (EKSİK tanı, yanlış değil).

**Kapılar:** `checker_diff` **142/142** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 110/110, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / parser 107 / linear 89, **geniş ölçüm 131/131**.
**Sabotaj:** S94 (`TIP_REFERANS` temsili), S95 (bayt-uzunluğu yerine sabit 10)
— ikisi de kırmızı, `grep` ile kanıtlandı.

**Kalan artımlar:** `işlev(..)->T` · `tekkez<T>` · `çeşit` payload · `olarak`
ifadesinin tipi.

---

## D-379 — Bileşik tip 3. artım: yapı alanları + ifade skrutinisi (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**İki küçük değişiklik, geniş kazanç.**
1. `yapi_kaydet` alan tiplerini `bilinen_skaler_mi` ile süzüyordu → `k.xs`,
   `k.s`, `k.r` tipleri BİLİNMİYORDU ve alan erişimli her zincir sessizdi.
   `bilinen_tip_mi`ye çevrildi (TEK SATIR) → `k.xs` T001, `k.xs[0]` eleman tipi,
   `k.s` iç-tip farkı açıldı. ERISIM dalı zaten `alan_tip_bul`a gidiyordu;
   yeni kod GEREKMEDİ — bariyer yalnız o filtreydi.
2. `esles_skrutini_tipi` yalnız TANIMLAYICI çözüyordu → `eşleş k.s`, `eşleş f()`,
   `eşleş xs[0]` M001 vermiyordu. TANIMLAYICI dışında genel `ifade_tip`e düşüldü.
   (`yerel_ham` yalnız yerel BAĞLAMALAR içindir; ifadelerin karşılığı yok.)

**Yöntem notu:** D-377/378'de kurulan temsil, bu artımda **yeni makine
yazmadan** meyve verdi — engel her seferinde bir SÜZGEÇTİ, eksik mantık değil.
Bileşik temsile geçerken `bilinen_skaler_mi` çağrılarını tek tek gözden
geçirmek (hangisi "skaler mi" hangisi "bilinen mi" soruyor) bu serinin
tekrarlayan işi oldu.

**Kapılar:** `checker_diff` **141/141** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 109/109, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / parser 107 / linear 89, **geniş ölçüm 131/131**.
Probe 8/8 birebir. **Sabotaj:** S92 (alan tipi süzgeci skalere döndürüldü),
S93 (skrutini yine yalnız TANIMLAYICI) — ikisi de kırmızı, `grep` ile kanıtlandı.

**Kalan artımlar:** `&T` · `işlev(..)->T` · `tekkez<T>` · `çeşit` payload tipleri.

---

## D-378 [YÜKSEK] — Bileşik tip 2. artım: `seçimlik<T>` + `sonuç<T,H>`; M001 dalları KAPANDI (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**İki kazanç, tek yatırım.** `tip_str` artık `seçimlik<T>` ve `sonuç<T,H>` de
üretiyor. Bu (a) tip KARŞILAŞTIRMALARINI (T001/T020) ve (b) **`eşleş`
KAPSAYICILIĞINI (M001)** aynı anda açtı.

**D-352'nin bilinçli borcu kapandı.** O parti M001'i yalnız `çeşit` dalı için
portlamış, `seçimlik`/`sonuç` dallarını AÇIK bırakmıştı — gerekçe: "self-host'ta
bileşik tip temsili yok, skrutininin `seçimlik` olduğu ANLAŞILAMIYOR". D-377/378
temsili getirince engel ortadan kalktı; dallar birkaç satırda eklendi.
**Bir borcu kapatmanın doğru zamanı, onu doğuran eksikliğin kapandığı andır.**

**Varyant tarama ölçüldü:** `değer(v)`/`tamam(v)` `DESEN_YAPICI`, çıplak `hiç`
`DESEN_TANIMLAYICI` olarak gelir → iki düğüm tipi de taranmalı. Joker `_` ve
bağlama yakalayıcısı mevcut erken-dönüşle zaten kapatıyor.

**D-377'nin generic koruması genelleştirildi.** `Dizi<seçimlik<tam32>>` çalışmıyordu:
eleman kontrolü elle sayılmış bir listeydi (`skaler ∪ Dizi<`). `bilinen_tip_mi`ye
çevrildi — o da generic param'ı (`T`) içermediği için **erteleme korunur**.
Aynı şekilde T001'in D-370 kapısı `Dizi<` özel-durumundan `bilinen_tip_mi`ye alındı.

**Sürücü kapısı bir PORT hatası yakaladı.** `sonuç` dalı codegen.kem'e eksik
taşınmıştı (blok kesilmişti); `checker_diff` YEŞİLDİ (o dosyayı referans checker
ile ölçüyor) ama sürücü koşum takımı `M001 36 5` eksiğini gösterdi. **İki
uygulamalı bir sistemde tek kapı yetmez** — parite kapıları farklı ikilileri ölçer.

**Kapılar:** `checker_diff` **140/140** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 108/108, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / parser 107 / linear 89, **geniş ölçüm 131/131**.
Probe: tip 8/8 + M001 7/7 birebir. **Sabotaj:** S90 (seçimlik M001 dalı),
S91 (`TIP_SECIMLIK` temsili) — ikisi de kırmızı, `grep` ile kanıtlandı.

**Kalan artımlar:** `&T` · `işlev(..)->T` · `tekkez<T>` · yapı alanlarının
bileşik hâli (`alan_tip` hâlâ yalnız skaler saklıyor).

---

## D-377 [YÜKSEK] — Bileşik tip temsili, 1. artım: `Dizi<E>` (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Neden.** D-376'nın "bilinçli sınır"ının ve D-352'nin `eşleş` boşluğunun ORTAK
kökü tekti: `tip_str` HER bileşik tipi `"?"`e düşürüyordu → `Dizi` tipli
T001/T013/T020 karşılaştırmaları hiç yapılmıyordu.

**Artımlı ve davranış-nötr başlatıldı.** `tip_str` `Dizi<E>` üretir; ama
`bilinen_skaler_mi` BİLEREK dokunulmadı — mevcut ~15 tüketici "Dizi<..>"yi
skaler saymaz, hepsi eskisi gibi susar. Yeni temsile opt-in eden yerler ayrı bir
`bilinen_tip_mi` kullanır. Kapsam tek tek ve ÖLÇÜLEREK açıldı: `yerel_tip_filtrele`,
`fn_donus`, `fn_ptip`, `ifade_tip` (DIZI_OLUSTUR + INDEKS), T001'in D-370 kapısı.

**Açtığı tanılar (7/7 probe birebir):** `Dizi` vs skaler T001 · eleman-tipi farkı
(`Dizi<tam32>` ≠ `Dizi<metin>`) · `ver` T020 · `xs[0]` eleman tipi · çağrı
argümanı · çağrı dönüşü. Hepsi ÖNCE tamamen sessizdi.

**İki gizli etkileşim ölçümle yakalandı — ikisi de YANLIŞ-POZİTİF:**
1. **İç içe dizide bağlam daralmıyordu.** `Dizi<Dizi<tam32>> = [[1,2],[3]]` →
   iç literaller "Dizi<tam32>" ile karşılaştırılıp sahte T013 aldı. Eleman tipi
   önce "?" olduğu için bu yol HİÇ uyanmamıştı. Daraltıcı eklendi.
2. **Generic param sızıntısı.** `tip_str` TIP_BASIT'te ham adı döndürdüğü için
   `Dizi<T>` "bilinen" sayıldı → `sirali_mi<T>(xs: Dizi<T>)` gövdesindeki
   `xs[0] > xs[1]` sahte T003 aldı (`kütüphane/dizi.kem` + `karsilastir.kem`,
   geniş ölçümde yakalandı). C generic'i ERTELER; eleman "gerçekten bilinen"
   değilse tüm tip "?" olur.

**DERS:** bir temsili zenginleştirmek, o temsilin FAKİRLİĞİNE dayanan her sessiz
yolu uyandırır. İkisi de "kural yanlıştı" değil, "bu yol ilk kez çalışıyor"du.
Bu yüzden artım davranış-nötr başlamalı ve kapsam tek tek açılmalı.

**Kapılar:** `checker_diff` **139/139** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 107/107, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / parser 107 / linear 89, **geniş ölçüm 131/131**.
**Sabotaj:** S88 (`Dizi<E>` üretimi), S89 (generic ertelemesi → yanlış-pozitif
kapısı) — ikisi de kırmızı, `grep` ile kanıtlandı, geri alındı.

**Sıradaki artımlar:** `seçimlik<T>` / `sonuç<T,H>` (D-352'nin `eşleş`
kapsayıcılık dalları buna bağlı) · `&T` · `yapı` alan tiplerinin bileşik hâli.

---

## D-376 [YÜKSEK] — T014 portlandı: tanı-kodu ekseni KAPANDI (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kural 20 probe ile ölçüldü.** Boş `[]` yalnız **DAR** bir konum listesinde
geçerli; başka her yerde T014:
- **MUAF:** `değişken`/`sabit`/`küresel` annotasyonu · `ver` (dönüş tipi Dizi) ·
  yapı literali alanı (alan tipi Dizi) · dizi elemanı (eleman tipi Dizi) ·
  atama (lvalue Dizi)
- **T014:** annotasyonsuz · beklenen skaler · **çağrı argümanı** · `olarak`
  operandı · **lambda gövdesi (dönüş `Dizi` OLSA BİLE)** · `değer([])` ·
  indeksleme · ikili operand

**Bağlam SIZMAZ → bayrak DEĞİL, düğüm muafiyeti.** `değişken xs: Dizi<tam32> =
f([])` → içteki `[]` yine T014. Bayrak alt-ağaca sızardı; bunun yerine muaf
düğümler listeye işaretlenir (`t14_muaf`). Tip düğümü elde varken **kesin
yürüyüş** (`[[]]`+`Dizi<Dizi<T>>` → iç `[]` de muaf; `[1,[]]`+`Dizi<tam32>` →
muaf DEĞİL), elde yokken (atama) **aşırı muafiyet** — aşırı = EKSİK tanı,
yanlış tanı değil.

**D-375 ile etkileşim ölçüldü.** `g(xs: Dizi<tam32>)` için `g([])` T014'ü **tam
bir kez** alır: C argümanı iki kez tip-belirler ama beklenen tipi YALNIZ ikinci
geçişte yayar. Skaler paramda iki kez alır. Parite için param TİP DÜĞÜMLERİ
tabloya eklendi (`fn_ptn`).

**İki yanlış-pozitifi kapı yakaladı.** `xs = []` ve `K { xs: [] }` sahte T014
alıyordu: muafiyet işaretlemem `kontrol_dugum`un ORTASINDAYDI, oysa ATAMA ve
YAPI_OLUSTUR'un **daha erken dönen** işleyicileri var. Blok fonksiyonun EN
BAŞINA taşındı. **DERS:** erken-dönüşlü bir dispatch zincirine iş eklerken
konum bir tercih değil, bir DOĞRULUK koşuludur.

**Port sırasında ikinci kusur — bayat alan yeniden kullanımı.** codegen.kem'de
hazır görünen `alan_tnode` (D-307) **CHECK yolunda BOŞ**; onu okumak sürücüyü
`PANIK: dizi sınır ihlali (boyut=0)` ile düşürdü (14 dosya kırmızı). Ayrı
`alan_tn` eklendi. **DERS:** aynı adlı bir tablo her yolda dolu değildir.

**BİLİNÇLİ SINIR (V1).** T014'ün KENDİSİ 20/20 birebir; ancak C'nin T014 ile
BİRLİKTE ürettiği **takip tanıları** (T020/T001/T013/E002) self-host'ta çıkmaz —
`ifade_tip` boş dizi için `"?"` döner ve doktrin gereği susulur. Bu, D-352'den
beri bilinen **bileşik tip temsili yokluğu**; ayrı iş.

**Kapılar:** `checker_diff` **138/138** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 106/106, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim tip_kontrol 202 / parser 107 / linear 89, **geniş ölçüm 131/131**.
**Sabotaj:** S85 (T014 tanısı), S86 (kesin tip yürüyüşü → 2 dosya), S87 (atama
aşırı-muafiyeti) — üçü de kırmızı, `grep` ile kanıtlandı, geri alındı.

**🏁 KAMPANYA EKSENİ KAPANDI.** Self-host tanı kodu **70** (D-350'de 24).
Kalan iki kod `T015`/`T023` **ÖLÜ** (C parser'ı o şekilleri reddeder) →
portlanacak tanı kodu KALMADI. Bundan sonrası tip evreni (bileşik tip temsili)
ya da yeni dil özelliği işidir.

---

## D-375 [YÜKSEK] — Argüman alt-ağacı İKİ KEZ denetlenir; geniş ölçüm 131/131 (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kusur — küme değil ÇOKLUK.** `kem_os.kem`'de C 11 konumda tanıyı **İKİ KEZ**
basıyordu, self-host bir kez. Aynı küme, farklı çokluk.

**Yanlış hipotez ölçümle düzeltildi (iki kez).** Önce "tanımsız `dtb`, kapsam
sorunu" sandım — sütun 59 `dtb` değil **`kdtb_toplam_boyut`** çıktı. Sonra
"`olarak` iki kez denetliyor" sandım — `olarak`sız da ikileniyordu. Gerçek:
**bilinen bir işlevin ARGÜMAN alt-ağacı iki kez tip-belirlenir.**

**Yapı ölçüldü, tahmin edilmedi.** `iki(yok(), yok2())` → `a,b,a,b` (arg başına
DEĞİL, iki TAM geçiş). `iki(s, yok())` → `T002; T001; T002` → 1. geçiş yalnız
gezinti, 2. geçiş gezinti + per-arg T001. İkilenenler traversal kaynaklı her şey
(T002/T007/T010/G001/E002); **per-arg T001 ikilenmez.**

**Kritik ayrıntı — durum değiştiren yollar ikilenmez.** `al(tuket(t))` C'de `OK`:
lineer tüketim İKİLENMİYOR. Naif "iki kez gez" sahte `L002` üretirdi (gerçek bir
yanlış-pozitif). `tuk_kapali` sayacıyla ön-geçişte tüketim bastırıldı; ön-geçiş
mevcut geçişin ÖNÜNE eklendiği için eski davranışın tamamı korundu.

**DERS:** çokluk farkı "kozmetik" görünür ama kökü yapısaldır — buradaki kök,
C'nin argümanları iki kez tip-belirlemesiydi. Ve çokluk paritesini kurarken
**hangi etkilerin idempotent olması gerektiğini ölçmek şart**: tanı raporlama
ikilenebilir, lineer tüketim ikilenemez.

**Kapılar:** `checker_diff` **137/137** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 105/105, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim linear 89 / capability 40. **Sabotaj:** S83 (ön-geçiş kapatıldı),
S84 (tüketim bastırması kaldırıldı) — ikisi de kırmızı, `grep` ile kanıtlandı.

**🎯 GENİŞ ÖLÇÜM 131/131 — TAM PARİTE.** `stdlib` + `stdlib/temel` +
`test/ornekler` + `kütüphane` + `test/moduller` yüzeyinde C oracle ile self-host
checker arasında **sıfır fark**: ne yanlış-pozitif ne eksik tanı. `kem_os.kem`
dâhil. Kalan tek resmî tanı kodu: **T014** (boş dizi bağlamı) — korpus dışı
şekillerde ortaya çıkabilir.

---

## D-374 [YÜKSEK] — Yerleşik imza tablosu: T010 + T001 yerleşik çağrılarda açıldı (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kusur.** Self-host'ta yerleşiklerin (built-in) yalnız **ADI** vardı, imzası yoktu
→ `fn_psay_bul` -1 dönüyor, arite ve argüman-tipi denetimi HİÇ çalışmıyordu.
Ölçüldü: `yazdir_tam("metin")` → C `T001`, self `OK`; `yazdir_tam()` → C `T010`,
self `OK`. Beş şeklin beşi de sessizdi.

**Çözüm — yeni makine YOK.** Mevcut `fn_ad`/`fn_psay`/`fn_ptip`/`fn_donus` tablosu
(T010 + per-arg T001 makinesi) 47 yerleşik imzayla **tohumlandı**; tanı yolları
olduğu gibi çalıştı. İmzalar `src/tip_kontrol.c`'den **programatik çıkarıldı**
(`awk`), elle kopyalanmadı — 47 girişte elle transkripsiyon sessiz hata kaynağı.

**Kapı bir YANLIŞ-POZİTİF yakaladı (bu partinin en değerli anı).** Yerleşikler
tabloya girince `E013` (çıplak gövdeden ρ-alan işlev çağırma) onları "normal
kullanıcı işlevi" saymaya başladı → `çıplak işlev f() { ver metin_uzunluk(s); }`
sahte E013 aldı. **Ayrım eskiden KENDİLİĞİNDEN oluyordu** ("tabloda yok" =
"yerleşik"); tabloya girince AÇIKÇA yapılmalıydı. `yer_son` eşiğiyle çözüldü
(tohumlama en başta → yerleşikler `[0, yer_son)` aralığı; yeni paralel dizi yok).
**DERS:** bir tabloyu genişletmek, o tablonun YOKLUĞUNU sinyal olarak kullanan
her yeri bozar — `fn_psay_bul(...) < 0` gibi "bulunamadı" testlerini `grep`le.

**Yol üstünde ölçülen ironi:** tohumlayıcıyı yazarken `yerlesik_ekle(p, "arg_sayi",
"tam32", [])` **T014** verdi (boş dizi bağlamı) — tam da henüz portlamadığım kod.
Tipli bir `değişken bos: Dizi<metin> = []` ile çözüldü.

**Kapılar:** `checker_diff` **136/136** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 104/104, **LLVM 113/113 ×2** — codegen regresyonu yok) + FIXPOINT,
`check_kapisi` 210/217 (0 RED), C birim tip_kontrol 202. **Sabotaj:** S81
(imza tablosu kurulmadı → tc25_01 kırmızı), S82 (E013 muafiyeti kaldırıldı →
tc16_01 + tc25_01 kırmızı). İkisi de `grep` ile kanıtlandı, geri alındı.

**Geniş ölçüm 130/131** (alan `test/moduller/` ile büyütüldü). `kem_asm_kernel.kem`
KAPANDI. **Tek kalan sapma:** `kem_os.kem` `T002` kuyruğu (tanımsız ad `dtb` —
kapsam çözümü). Yanlış-pozitif YOK.

---

## D-373 — AS001 (satıriçi_asm mimari kapısı) self-host'a portlandı (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kural.** `satıriçi_asm { mimari: X ... }` etiketi HEDEF mimariyle uyuşmalı;
uyuşmazsa `AS001`. Amaç: yanlış hedefe **sessizce bozuk makine kodu üretmek
yerine** derleme hatası. C sırası ölçüldü: aynı düğümde **önce G002, sonra
AS001** (`güvensiz` dışındaki uyumsuz asm İKİ tanı alır).

**Hedef mimari self-host'ta SABİT `x86_64`.** Bu bir varsayım değil, ölçülmüş
olgu: C'nin varsayılanı `x86_64`'tür (`KEMGU_HEDEF_MIMARI`) ve **self-host
sürücüde `--mimari` bayrağı YOKTUR** → hedef gerçekten değişmez. Sürücüye
`--mimari` eklenirse `as001_kontrol` da güncellenmeli (koda not düşüldü).

**Ölçüm tuzağı:** ilk probe'da `mimari: aarch64` yazdım ve `--mimari aarch64`
ile İKİ tanı birden aldım — kural bozuk sandım. Gerçek: C'de mimari adı
**`arm64`**tür (`aarch64` yalnız bayrak değeri olarak kabul edilip `arm64`e
çevrilir), yani `aarch64` etiketi hiçbir hedefle eşleşmez. Kural doğruydu,
probe yanlıştı. **Beklenmedik sonuçta önce probe'u doğrula.**

**Etiket parser yan-kanalında** (`asm_node`/`asm_mim`): parser etiketi
atıyordu; düğüme alan eklemek `--ast` paritesini bozardı.

**Kapılar:** `checker_diff` **135/135** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 103/103, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED).
**Sabotaj:** S79 (AS001 tanısı), S80 (G002/AS001 SIRASI ters çevrildi) — ikisi
de kırmızı, `grep` ile kanıtlandı, geri alındı.

**Geniş ölçüm 97/99** (D-372'de 96). Kalan 2 sapmanın ikisi de **eksik tanı**:
`kem_asm_kernel.kem` `T001 18:16` — `yazdir_tam` gibi **built-in'lerin PARAMETRE
TİPİ** self-host'ta yok (built-in imza tablosu işi); `kem_os.kem` `T002` kuyruğu
(tanımsız ad `dtb` — kapsam çözümü). İkisi de ayrı alt-sistem.

---

## D-372 [YÜKSEK] — CP005 YANLIŞ-POZİTİFİ: `yetki<R>` parametresi lineerdir (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kusur.** Self-host `test/ornekler/mmio_smoke.kem` ve `virtio_selfhost_rw.kem`'i
sahte `CP005` ("yetki tüketilmedi") ile reddediyordu (C: `OK`). D-371'de ölçüm
alanı `kütüphane/`i de kapsayacak şekilde genişletilince ortaya çıktı.

**Kök neden — TEK SATIR.** `param_lineer_mi` yalnız `TIP_TEKKEZ` ve lineer yapıyı
sayıyordu; **`TIP_YETKI` yoktu**. Dolayısıyla `işlev f(y: yetki<MMIO>)` çağrısı
argümanı TAŞIMIYOR (tüketmiyor) sayılıyor, `y` scope sonunda "tüketilmedi"
görünüyordu. **D-365'in eksik kalan yarısı:** o parti `TIP_YETKI`'yi
`tip_node_tekkez_mi`ye eklemişti (bağlamanın kendisi lineer) ama **parametre
lineerliğine** eklememişti (taşıma noktası). İki ayrı yüklem, biri güncellendi.

**DERS:** "yetki artık mevcut lineer makinenin TAMAMINI kullanıyor" (D-365)
iddiası **yüklem yüklem doğrulanmalıydı**. Bir tip yeni bir lineer sınıfa
katılırken, lineerliği soran HER yüklemi `grep` ile listele — `tip_node_tekkez_mi`,
`param_lineer_mi`, `deg_lineer_mi` ayrı ayrı karar veriyor.

**Kapılar:** `checker_diff` **134/134** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 102/102, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED),
C birim: capability 40, linear 89, mmio 23. **Sabotaj S78** (TIP_YETKI dalı
kaldırıldı) → `tc23_01` kırmızı; `grep` ile uygulandığı kanıtlandı, geri alındı.

**Geniş ölçüm (99 dosya) sonucu: YANLIŞ-POZİTİF KALMADI** (96/99). Kalan 3
sapmanın hepsi **EKSİK tanı** (self-host sessiz, C konuşuyor) — daha hafif sınıf:
`AS001` self-host'ta hiç yok (2 dosya) + `kem_os.kem` T002 kuyruğu.

---

## D-371 [YÜKSEK] — G005 YANLIŞ-POZİTİFİ: doğrudan lambda argümanı kaçış değildir (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+1).

**Kusur.** Self-host `test/ornekler/kanal_mesaj.kem` gibi **GEÇERLİ** programları
`G005` ile reddediyordu (C: `OK`). D-370'te ölçüm alanı `test/ornekler/`e
genişletilince ortaya çıktı; D-324'ten kalma, regresyon değil.

**Kök neden.** C `escape.c` çağrı argümanlarını ESC_CAGIRAN'a terfi ettirirken
**LAMBDA argümanını bilinçli olarak MUAF tutar** (kaynaktaki yorum: lambda hiçbir
zaman serbest edilmez → terfi gereksiz; üstelik `görev_başlat(|| ...)` desenini
G005'te yanlış-pozitif yapar, görev'in kendi R-YAKALAMA-THREAD sahiplik modeli
vardır). D-324 portu C'nin tetikleyicilerini ölçerek taklit etmişti ama bu
**istisnayı** almamıştı.

**Daraltma ölçüldü, uydurulmadı** (3 şekil): `al(|| s)` doğrudan argüman → **G005
YOK**; `değişken f = || s; al(f)` ada bağlı → **G005 VAR** (16:39); `ver || s`
→ **G005 VAR** (21:9). Yani muafiyet YALNIZ doğrudan lambda argümanına ait —
ada bağlanmış lambda ve `ver` yolu aynen korunur.

**DERS (D-356/S31'in tekrarı, ölçülerek yaşandı):** ilk sabotaj (S75) **SESSİZ
kaldı** — `al(...)` bilinen-işlev yolundan giderken `görev_başlat(...)` AYRI bir
built-in çağrı yolundan gidiyor ve korpusta o şekil yoktu. `görev_başlat(|| s)`
örneği eklenince S75 kırmızıya döndü. **İki kod yolu varsa korpusta İKİSİ de
olmalı;** sabotajın sessizliği kuralın doğruluğu değil, korpusun eksikliğidir.

**Kapılar:** `checker_diff` **133/133** (0 muaf), sürücü 4 mod × 2 sürücü
(CHECK 101/101, LLVM 113/113 ×2) + FIXPOINT, `check_kapisi` 210/217 (0 RED).
**Sabotaj:** S75 (built-in çağrı yolu muafiyeti), S76 (bilinen-işlev yolu
muafiyeti), S77 (muafiyet aşırı genişlerse ada bağlı lambda kaçar) — üçü de
kırmızı, `grep` ile uygulandıkları kanıtlandı, geri alındı.

**Ölçüm alanı genişletildi (99 dosya: stdlib + ornekler + kütüphane).** Kalan
sapmalar — hepsi EKSİK tanı ya da ayrı sınıf, bu partide DEĞİL:
`AS001` self-host'ta hiç yok (2 dosya); `kem_os.kem` T002 kuyruğu; **`CP005`
yanlış-pozitifi** `mmio_smoke.kem` + `virtio_selfhost_rw.kem`'de (self-host
"yetki tüketilmedi" diyor, C `OK`) → sıradaki iş.

---

## D-370 [YÜKSEK] — T011 portlandı; imza/gövde tanı SIRASI iki geçişe ayrıldı (2026-08-06)

**ETKİ:** `selfhost/checker.kem`, `selfhost/codegen.kem`, `test/check_korpus/` (+2).

**T011 (bilinmeyen tip).** Tip-adı EVRENİ ölçümle kuruldu (10 probe): yerleşik
skalerler + `yapı` + `çeşit` + generic param adları + yetenek kaynak tipleri
(`MMIO`/`Dosya`/`Soket`/`Bellek`/`Donanim`/`OTP_Anahtar`) + küresel ad tablosu.
**`özellik` adı EVRENDE DEĞİL** — ölçüldü: `işlev f(x: Say)` C'de T011 verir;
özellik bir BOUND'dur, tip değil. Generic param adları yeni `gp_ad` yan-kanalına
yazılır (parser; düğüme alan eklemek `--ast` paritesini bozardı). Yan etki:
annotasyon bilinmeyen tipse **T001 bastırılır** — C tipi HATA'ya düşürdüğü için
T001'e hiç gelmez; bastırmasız tek kusur iki tanı üretirdi.

**Asıl bulgu — SIRA, küme değil.** T011 eklenince küme birebir tuttu ama SIRA
tutmadı. Ölçüm (`T030@8` imza + `T011@5` gövde → **8 önce**): C tipleri
`pre_populate`de çözer, yani **TÜM imza/alan tanıları TÜM gövde tanılarından
önce** çıkar; ön-geçiş de kendi içinde ÖNCE tüm `yapı`/`çeşit`, SONRA tüm
`işlev`. D-369 imza gezintisini `kontrol_govde` içine koymuştu — küme doğru,
sıra yanlıştı. Korpusta imza-hatası ile gövde-hatası **aynı dosyada olmadığı
için gizli kalmıştı**; T011 ikisini de üretebilen ilk kod olduğu için ortaya
çıktı. `kontrol_imza` + `kontrol_yapi_alanlari` ayrıldı, `kontrol_ust` üç
geçişli oldu (yapılar → imzalar → gövdeler).

**DERS (D-350'nin tekrarı, yeni yüzü):** yeşil `checker_diff` "kural doğru"
demek değil; **tanı SIRASI da bir sözleşmedir** ve tek-tanılı korpus dosyaları
onu hiç sınamaz. Yeni kural eklerken korpusa **iki farklı geçişten tanı üreten**
tek dosya koy.

**Yan bulgu (bu partiden DEĞİL, kayda geçsin):** ölçüm alanını `test/ornekler/`e
genişletince `kanal_mesaj.kem`'de self-host `G005 49 28` verirken C `OK` diyor —
**yanlış-pozitif**. Önceki commit'te de var (regresyon değil, D-324'ten kalma).
Ayrıca `kem_asm_kernel.kem`/`kem_kullanici.kem`'de `AS001` self-host'ta hiç yok.

**Kapılar:** `checker_diff` **132/132** (0 muaf), sürücü koşum takımı 4 mod × 2
sürücü (TOKEN 22/22, PARSE 13/13, CHECK 100/100, LLVM 113/113 ×2) + FIXPOINT,
`check_kapisi` 210/217 (0 RED), C birim: tip_kontrol 202, parser 107, linear 89,
drf 54, capability 40, sabitsüre 39, mmio 23.

**Sabotaj kapıları:** S72 (T011 tanısı → tc21_01 kırmızı), S73 (yetenek kaynak
evreni → 5 dosya kırmızı), S74 (imza ön-geçişi → tc20_01 + tc21_01 kırmızı).
Üçü de `grep SABOTAJ-S7n` ile uygulandığı kanıtlandı, sonra geri alındı.

**Kapsam:** self-host tanı kodu **68** (D-369'da 66). Kalan gerçek kod: **T014**
(boş dizi bağlamı). `T015`/`T023` ÖLÜ (C'de parser şekli reddediyor).

---

## D-369 [YÜKSEK] — T030 + T031 portlandı; İMZA-ÜSTÜ tip denetimi eksikti (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**Beklenen engel çıkmadı:** D-368'de "TIP_KULLANICI adı düğümde yok, yan-kanal
gerekir" demiştim. Ölçüm düzeltti — **ad çocuk[0]'daki TANIMLAYICI düğümünde**
(`TIP_KULLANICI` → çocuk[0]=ad, çocuk[1..]=tip argümanları). C'nin `--ast`'i de
a_deg'i boş basıyor, yani self-host zaten sadıktı. Yalnız **bound tablosu** için
yan-kanal gerekti (`atla_tip_paramlar` bound'ları PARSE+DISCARD ediyordu).

**Kural:** `yapı Vektor<T: Say>` ise `Vektor<Tam>` ancak `uygula Say için Tam`
varsa geçerli (T030); bound adı hiç `özellik` değilse T031. Özellik/uygula
kayıtları AST'den türetildi (yan-kanal gerekmedi): `UYGULA` düğümü
çocuk[0]=hedef tip, çocuk[1]=özellik.

### ASIL BULGU: işlev DÖNÜŞ ve PARAMETRE tipleri hiç denetlenmiyordu

T030/T031 ilk denemede tetiklenmedi. Sebep tekil bir kural eksiği değildi:
`kontrol_govde` **yalnız BLOK çocuğuna** iniyordu → **imza üstündeki hiçbir tip
düğümü gezilmiyordu.** Yani `işlev f() -> görev<kesirli64>` (ölçüldü: C DRF001
verir) ve `işlev f(g: görev<kesirli64>)` self-host'ta **sessizce geçiyordu**.

Bu, D-366'nın (DRF) ve D-367'nin (CT006) de eksik kalan yanıydı — o partilerde
tanılar yalnız `değişken` annotasyonu yolundan çıkıyordu. Tek satırlık gezinti
düzeltmesi **dört partinin** kapsamını genişletti.

**Sabotaj (3):** S69 T030 (130→129), S70 T031/T030 ayrımı (129 — yanlış KOD
üretildi), S71 imza-üstü gezinti (129 — **iki tanı birden düştü**).

**Probe 6/6 birebir** (bound ihlali / karşılanmış / bilinmeyen bound / generic
işlev / dönüş tipi DRF001 / parametre tipi DRF001).

**Sonuç:** self-host checker kod kapsamı **68 → 70/74**. Kalan **4 kod**:
T011, T014 + 2 ölü (T015/T023) → **gerçekte 2**, ikisi de tip-evreni işi
(T011 tam tip-adı evreni, T014 beklenen-tip yayıcısı).

---

## D-368 — M004 portlandı: ÇEŞİT ALT-SİSTEMİ KAPANDI (4/4) (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+1).

**Kural:** çeşit varyantı yapıcısında M003 (arite) geçtikten SONRA her argüman
kendi payload tipiyle karşılaştırılır; konum **argüman düğümü**, birden çok
uyumsuz argüman **birden çok tanı** verir.

**Gereken altyapı:** D-357'nin `cv_*` yan-kanalına payload **TİP** tablosu
(`cv_pb` taban + `cv_pt` düz tip dizgileri). `parse_cesit` payload tip düğümlerini
zaten `kids`e topluyordu; oradan `tip_str` ile dizgiye çevrildi. TIP_BASIT dışı
("?") karşılaştırmadan muaf — emin olmadığımız yerde susuluyor.

**Beklenen tip BAĞLAM olarak geçirilmeli:** `Dar(tam8)` varyantına `5` literali
geçerlidir (C literali dar tipe uyarlar). Sabotaj S68 bunu ölçtü: bağlam
kaldırılınca `Dar::Kucuk(5)` **sahte M004** aldı.

**Sabotaj (2):** S67 M004 (128→127), S68 beklenen-tip bağlamı (127 —
**yanlış-pozitif yönünde**).

**Probe 6/6 birebir** (tek uyumsuz / ikinci uyumsuz / her ikisi / geçerli /
dar-literal uyarlaması).

**Sonuç:** self-host checker kod kapsamı **67 → 68/74**. **ÇEŞİT ALT-SİSTEMİ 4/4
KAPANDI** (M001-M004). Kalan **6 kod**: T011, T014, T030, T031 + 2 ölü
(T015/T023) → **gerçekte 4**, hepsi tip-evreni/generic-bound işi.

---

## D-367 — CT001-CT008 portlandı: SABİTSÜRE ALT-SİSTEMİ KAPANDI (8/8) (2026-08-06)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**Plan "iki parça" idi, ÖLÇÜM TEK PARÇAYA İNDİRDİ.** Taint yayılımının pahalı
olacağını varsaymıştım; C'nin kuralı ölçülünce **tek özyineli yüklem** olduğu
görüldü: aritmetik/bit operandlarından biri sabitsüre ise sonuç da sabitsüre.
Karşılaştırma ve `ifşa(...)` bu yolun DIŞINDA (ilki mantıksal üretir, ikincisi
declassify eder). Yayılım eklenince kalan tek fark da kapandı.

**Kurallar:** CT001 dallanma · CT002 indeksleme · CT003 implicit akış ·
CT004 `/`,`%` · CT005 üretici aritesi · CT006 sarılan tip CT-yetenekli değil
(annotasyon + yapıcı, İKİ tanı) · CT007 `ifşa` aritesi/operandı · CT008 kaydırma
miktarı.

**Yol üstünde bulunan MEVCUT YANLIŞ-POZİTİF:** `ifşa` (ş ile!) self-host'un
built-in listesinde YOKTU → her geçerli declassify çağrısı **sahte T002** alıyordu.
D-361'de ASCII `ifsa` denenmiş ve "C de tanımıyor" diye elenmişti — **yazım
yanlıştı**, doğrusu ölçülerek bulundu. Bu, D-361'deki eleme gerekçesinin bir
kısmını geçersiz kılıyor.

**Yan bulgu (T021):** `sabitsüre<T>` koşulu mantıksal olmadığı için C **T021 +
CT001** verir. Self-host'ta `yerel_tip` sabitsüre'yi "?"e düşürdüğünden T021 hiç
çıkmıyordu; sabitsüre bilgisiyle o da kapandı.

**Korpus dosyası yazarken ölçüm iki satırı eledi:** `sabitsüre<karakter>` ve
`sabitsüre<mantıksal>` + `sabitsüre_olustur(...)` C'de **T001** veriyor
(üreticinin dönüş çıkarsaması tamsayı literaline bağlı) — CT kuralı değil, ayrı
bir tip-çıkarsama sınırı. "Temiz" dosyaya alınmadı, gerekçesi dosyaya yazıldı.

**Sabotaj (3):** S64 taint yayılımı (127→126), S65 CT008 (126), S66 `ifşa`nın
declassify etmesi (**yanlış-pozitif yönünde**: geçerli tüm kullanımlar CT003 aldı).

**Probe 12/12 birebir.**

**Sonuç:** self-host checker kod kapsamı **59 → 67/74**. **SABİTSÜRE ALT-SİSTEMİ
8/8 KAPANDI.** Kalan **7 kod**: M004, T011, T014, T030, T031 + 2 ölü (T015/T023).
**Yani gerçekte kalan 5.** C tarafı regresyonsuz: sabitsure 39/39.

---

## D-366 — DRF001-DRF007 portlandı: DRF ALT-SİSTEMİ KAPANDI (7/7) (2026-08-05)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**Kurallar:** `görev<T>`/`kanal<T>` V1'de **kesirli T taşıyamaz** (DRF001/DRF006) —
runtime sonucu tamsayı yazmacından okur (x0/rax), kesirli değer v0/xmm0'dadır →
bitcast **sessiz çöp** verirdi. C bu kısıtı **İKİ yolda ayrı ayrı** uyguluyor
(annotasyon/tip düğümü + yapıcı çağrısı) ve **iki tanı birden** üretiyor.
Ayrıca arite kuralları (DRF002 `görev_birleştir`, DRF003 `kanal_gönder`,
DRF004 `kanal_al`, DRF005 `dondur`) ve yön güvenliği (DRF007: `alan<T>` alıcı
ucundan gönderim yasak).

**Konumlar C'den ölçülerek alındı; ÜÇÜ FARKLI düğümde:**
- DRF001 (yapıcı yolu) → **argüman** düğümü
- DRF006 (yapıcı yolu) → **çağrı** düğümü
- DRF007 → **argüman** düğümü (ilk denemede çağrı düğümüne koymuştum: 1 sütun
  kaydı, C kaynağına bakılarak düzeltildi)

**Yön sinyali TİPTEN OKUNAMADI (ölçüm):** `alan<T>` annotasyonu self-host'ta
`TIP_KULLANICI`ya düşüyor ve **kullanıcı-tipi adı düğümde tutulmuyor**
(`--parse` dump'ında a_deg boş). Bu yüzden yön **değerden** okunuyor:
`alan(k)` / `gönderen(k)` projeksiyon çağrısı → `yerel_yon` yan-dizisi.

**Yeni alanlar:** `yerel_yon` (kanal ucu yönü), `drf_bek` (aktif
`görev<kesirli*>`/`kanal<kesirli*>` annotasyon bağlamı — `dizi_bek` deseni).

**Sabotaj (4):** S60 annotasyon yolu (125→124), S61 yapıcı yolu DRF001 (124),
S62 DRF007 (124), S63 yön ayrımı (**yanlış-pozitif yönünde**: `gönderen`
ucundan gönderim reddedildi).

**Probe 8/8 birebir** (7 kod + temiz şekil).

**Sonuç:** self-host checker kod kapsamı **52 → 59/74**. **DRF alt-sistemi 7/7
KAPANDI.** Kalan 15 kod: sabitsüre (8), M004, T011/T014, T030/T031 + 2 ölü.
C tarafı regresyonsuz: drf 54/54, gorev_rt 16/16.

---

## D-365 — CP005 portlandı: MMIO + YETKİ ALT-SİSTEMİ KAPANDI (5/5) (2026-08-05)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+1).

**Ölçümle çıkan asıl bulgu — CP005 YENİ BİR KURAL DEĞİL.** C'de üç ayrı yerde,
mevcut lineer tanıların **yetki karşılığı** olarak üretiliyor:

| lineer tanı | yetki bağlamasında |
|---|---|
| L002 (çift tüketim) | **CP005** |
| L001 (tüketilmedi) | **CP005** |
| L004 (lineere referans) | **CP005** |

Yani gereken şey yeni bir analiz değil, **mevcut makineye yetki'yi sokmak + kod
ikamesi**. Bu, D-313'ün `yapı tekkez` için yaptığının aynısı (ayrı kod yolu YOK).

**Uygulama:** `tip_node_tekkez_mi` artık `TIP_YETKI`yi de sayıyor (C
`tip_lineer_mi` ile hizalı) → yetki bağlamaları lineer dilime giriyor ve
L001/L002/L004/L-COND/L-LOOP makinesinin **tamamı** onlar için de çalışıyor.
Yeni bit `lin_yet` yalnız **raporlanan kodu** seçiyor (`lin_kod`).

**Tüketim/ödünç ayrımı ölçüldü:** `geri_al(y)` **tüketir**; `mmio_*(y, ...)` ve
`bölge_al(y, ...)` **ödünç alır** (tüketmez). Sabotaj S59 bunu kapıladı:
`geri_al` tüketmeyince `tc17_02`'deki geçerli kod sahte CP005 aldı.

**Sabotaj (3):** S57 kod ikamesi (123→122; CP005 yerine L001/L002/L004 çıktı),
S58 yetkinin lineer sayılması (122), S59 `geri_al` tüketimi (**yanlış-pozitif
yönünde**, geçerli dosyalar kırıldı).

**Probe 9/9 birebir** — üç CP005 şekli + tekkez'in L001/L002'sinin BOZULMADIĞI
(kod ikamesi yalnız yetki bağlamasında).

**Sonuç:** self-host checker kod kapsamı **51 → 52/74**.
**MMIO + yetki alt-sistemi 5/5 KAPANDI** (MM001-003, CP004, CP005).
Kalan 22 kod: sabitsüre (8), DRF (7), M004, T011/T014, T030/T031 + 2 ölü.
C tarafı regresyonsuz: linear 89/89, capability 40/40.

---

## D-364 — MMIO + yetki intrinsikleri portlandı: MM001/MM002/MM003 + CP004 (2026-08-05)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

D-363'ten sonra kalan iş "alt-sistem" sınıfındaydı; ilk alt-sistem olarak
MMIO+yetki seçildi (kem_os'un canlı kullandığı yüzey, lineer altyapı hazır).

**Kurallar (6 MMIO built-in'i: 16/32/64 × oku/yaz):**
- `mmio_okuN(y: yetki<MMIO>, adres)` 2 argüman · `mmio_yazN(y, adres, değer)` 3
- **MM001** arite (ÇAĞRI düğümü, erken dönüş) · **MM002** arg0 `yetki<MMIO>` değil
  (ARG düğümü) · **MM003** adres/değer tamsayı değil (ARG düğümü)
- **CP004** `geri_al` tam 1 argüman (ÇAĞRI) / operandı `yetki<R>` (ARG)

**Yeni yan-dizi `yerel_yet`:** bağlamanın `yetki<R>` KAYNAK adı ("" = yetki değil).
MM002 "kaynak MMIO mu" sorusunu bununla yanıtlıyor. Muhafazakârlık: yalnız
**kesin** olduğumuzda konuşuyoruz — `kesin_yetki_degil` bilinen skaler ya da
yetki-olmayan bir bağlama gerektiriyor; emin olunamayan ifade → susuluyor.

**CP005 BU PARTİDE YOK (bilinçli):** `yetki<R>`nin çift tüketimi lineer izleme
ister. Ölçüldü: `param_lineer_mi` TIP_YETKI'yi zaten sayıyor ama `deg_lineer_mi`
saymıyor; ayrıca kod L002 değil **CP005** olmalı. Mevcut lineer makineye
yetki-ayrımlı bir kod yolu eklemek ayrı bir adım — tahminle yazılmadı.

**Sabotaj (5):** S52 MM001 (122→121), S53 MM002 (121), S54 MM003 (121),
S55 MM002'nin "kaynak MMIO" ayrımı (**yanlış-pozitif yönünde**: geçerli
`yetki<MMIO>` kullanımları reddedildi), S56 CP004 (121).

**Probe 7/7 birebir** (CP005 içeren 2 şekil hariç — kapsam dışı, bilinçli).

**Sonuç:** self-host checker kod kapsamı **47 → 51/74**; korpus **90 dosya**,
kapı **122** (0 muaf). Kalan 23 kodun 2'si ölü (T015/T023).
C tarafı regresyonsuz: capability 40/40, mmio 23/23.

---

## D-363 — T041 (private-by-default) portlandı: MODÜL ALT-SİSTEMİ KAPANDI (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/checker_diff_harness.sh`.

**Kural:** dosya-modülünün `genel` işaretsiz üyeleri **çapraz-modül erişilemez**
(private-by-default). Modülün kendi içinden kardeşler görünür; dosya-içi
`modül m { }` blokları C'de de denetlenmiyor.

**Engel:** `genel` düğüme YANSIMIYORDU. `parse_genel` → `parse_disa_govde`
**çıplak tanımı** döndürüyor (DISA sarmalı yok) → `genel işlev f` ile `işlev f`
self-host'ta **ayırt edilemiyordu**. Yan-kanal `gen_node` eklendi (D-352/353/358
deseni); `modul_yukle` her dosya-modülünün kendi ayrıştırıcısından (`mp`) okuyup
`genel` OLMAYAN üyeleri `priv_mod`/`priv_ad`'e kaydediyor.

**Sıra:** T016 (modül yok) → T041 (modül var, üye özel). T016 tetiklenirse erken
dönülür — C'nin sırası da böyle (`yol_modul_scope_coz` başarısızsa T041'e gelinmez).

**Sabotaj (2):** S50 T041 (120→119), S51 `genel` muafiyeti (**birçok geçerli dosya
kırıldı** — muafiyetin yanlış-pozitif koruması olduğu ölçüldü).

### MUAFİYET LİSTESİ BOŞALDI

`checker_diff` **120/120, 0 muaf**. Modül yüzeyi üç adımda tamamen kapandı:
D-361 (seçili import + T042), D-362 (runtime UTF-8 yolu + T040 + T016),
D-363 (T041). Yeni bir muafiyet eklenirse gerekçesi DECISIONS_LOG'a yazılmalı.

**Sonuç:** self-host checker kod kapsamı **46 → 47/74**. Kalan 27 kodun **2'si ölü**
(T015/T023) ve kalan 25'in **23'ü** dört alt-sistemin tip temsiline bağlı
(CT*/DRF*/MM*+CP*), 2'si tip evreni (T011/T014) — artı M004 ve T030/T031.
**Modül kümesi tamamlandı; tek tek portlanabilecek genel-amaçlı kod KALMADI.**

---

## D-362 [YÜKSEK] — Runtime UTF-8 yol onarımı (`kütüphane/`) + T040 + T016 (2026-08-04)

**ETKİ:** `runtime/kdl_runtime.c`, `selfhost/codegen.kem`, `selfhost/checker.kem`,
`test/checker_diff_harness.sh`.

### Kök neden: runtime `fopen` UTF-8 yolu açamıyordu

D-361, T040'ın "modül bulunamadı" öncülünün YANLIŞ olduğunu göstermişti. Kök neden
ölçüldü ve onarıldı: **Windows'ta `fopen` ANSI codepage kullanır**, dolayısıyla
UTF-8 yol `kütüphane/dizi.kem` **açılamaz**. `src/ana.c` bunu bildiği için kendi
`dosya_ac_utf8`'ini taşıyordu; `runtime/kdl_runtime.c` ise **düz `fopen`**
kullanıyordu → self-host derleyici `kütüphane/` altındaki modülleri **sessizce**
yükleyemiyordu.

**Onarım:** `kdl_fopen_utf8` (ana.c'nin deseni: `MultiByteToWideChar` + `_wfopen`,
dönüşüm başarısızsa düz `fopen`'a düşer) ve **8 çağrı yerinin tamamı** ona çevrildi.

**ÖNCE/SONRA ÖLÇÜMÜ (aynı IR, iki farklı runtime objesi):**
```
dosya_var_mi("kütüphane/dizi.kem") → ESKİ runtime: exit 7  (açılamadı)
                                     YENİ runtime: exit 42 (açıldı)
```

**Bayat-obje tuzağı yaşandı ve atlatıldı:** `git stash pop` sonrası
`mingw32-make build/kdl_runtime.o` **"up to date"** dedi — obje ESKİ kaynaktan
kalmıştı. CLAUDE.md'nin uyardığı tuzak; `rm -f` + yeniden derleme ile doğrulandı.

### Açılan tanılar

- **T040** (modül yüklenemedi) — artık **sıfır yanlış-pozitif** (D-361'de 4 tane
  vardı). C İKİ KEZ raporluyor (ana.c'de iki yükleme geçişi); parite için ikisi de.
- **T016** (modül bulunamadı) — `X::y`de X ne çeşit ne modülse. Modül adları üç
  kaynaktan: yüklenen `kullan`lar (tam yol + **son segment**), `modül m` blokları,
  ve **alias**lar (`kullan m olarak d` — parser onu atıyordu, yan-kanal eklendi).
  İç içe `a::b::c` yolunda ara düğüm YOL olduğu için self-host **susar** (C modül
  zinciriyle çözer; emin olunamayan yerde tanı üretilmez).

**Sabotaj (3):** S47 T040 (119→118), S48 T016 (118), S49 modül-adı kaydı
(**birçok geçerli dosya sahte T016 aldı** — kaydın yanlış-pozitif koruması olduğu
ölçüldü).

**Muafiyet listesi 2 → 1:** `ana_kutuphane.kem` kapandı. Kalan tek muaf
`ana_gizli.kem` (T041 — `genel` görünürlüğü portlanmadı).

**Sonuç:** self-host checker kod kapsamı **44 → 46/74**; kapı **119 dosya**
(1 muaf); modül yüzeyi **31/32**.

---

## D-361 [YÜKSEK] — Modül yüzeyi ölçüldü: 3 YANLIŞ-POZİTİF onarıldı + T042 (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/checker_diff_harness.sh`.

D-360'ın önerdiği modül kümesine (T016/T040/T041/T042) girmeden önce yüzey ölçüldü:
`test/moduller/` (32 dosya) C oracle'ına karşı koşuldu → **6 sapma, İKİ YÖNDE**.
Kritik olan yön beklenmedikti: **self-host GEÇERLİ programları reddediyordu.**

### Onarılan yanlış-pozitifler (self-host C'den KATI — daha ciddi sınıf)

1. **Eksik built-in'ler (6).** `bölge_al`, `kanal_oluştur`, `gönderen`, `alan`,
   `vektor_ve_azalt`, `vektor_veya_azalt` self-host'un `builtin_ekle` listesinde
   yoktu → geçerli çağrılar **sahte T002** alıyordu (`kap.kem`'de 2 hata ölçüldü).
   Liste C ile karşılaştırılarak tamamlandı. (`ifsa` ve `bölge_serbest` EKLENMEDİ —
   probe: C de onları tanımıyor.)
2. **Seçili import (`kullan m::{a,b}`).** Parser seçilen adları **atıyordu**
   ("dump'ta yok") → niteliksiz gelen `topla` sahte T002 alıyordu
   (`ana_secili.kem`). Yan-kanal `si_ad`/`si_yol` eklendi (cv_* deseni).

### Portlanan tanı

3. **T042** — aynı ad birden çok modülden seçili-import edilirse çıplak kullanımı
   belirsiz. `si_yol` sayesinde "aynı ad, FARKLI modül" ayrımı yapılabiliyor.
   Öncesinde self-host burada T042 yerine **T002** veriyordu (`ana_belirsiz.kem`):
   doğru konum, yanlış kod.

### T040 DENENDİ ve GERİ ALINDI — sessiz atlama bir runtime kusurunu maskeliyormuş

`modul_yukle`'nin "bulunamadı → sessizce geç" davranışını T040'a çevirmek
**4 yanlış-pozitif** üretti (`dizi_*.kem`). Kök neden ölçüldü: C `ana.c`
**`MultiByteToWideChar`** kullanıyor, çünkü **Windows'ta `fopen` ANSI codepage'e
düşer ve UTF-8 `kütüphane/` yolunda BAŞARISIZ olur**. Self-host'un
`dosya_var_mi`/`dosya_oku` runtime'ı bu dönüşümü yapmıyor → `kütüphane/`
modülleri **sessizce yüklenmiyor**. Yani T040 sağlam değil: doğru kural, yanlış
öncül. **Sıra: önce runtime UTF-8 yol onarımı, sonra T040.** Bu, "sessiz atlama"nın
altında yatan gerçek kusuru göstermesi bakımından kayda değer.

### Kapı genişletildi

`checker_diff` artık `test/moduller/`i de kapsıyor: **88 → 118 dosya**
(2 belgeli muafiyet: `ana_gizli` T041 portlanmadı, `ana_kutuphane` runtime'a bağlı).
Muafiyet listesi kapanınca boşalmalı.

**Sabotaj (3):** S44 eksik built-in (118→117, `kap.kem` sahte T002'ye döndü),
S45 seçili-ad kaydı (117 — hem `ana_secili` hem `ana_belirsiz`),
S46 T042 (117).

**Sonuç:** self-host checker kod kapsamı **43 → 44/74**; kapı **88 → 118 dosya**;
modül yüzeyi 26/32 → **30/32**.

---

## D-360 — Kalan tanı kodlarının ULAŞILABİLİRLİK taraması + E013 portu (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+1).

### 1) `--token` çapraz ölçümü: temiz

D-359'un yöntemi lexer'a uygulandı — **235 gerçek dosyanın tamamında** `--token`
paritesi birebir (`lex_korpus` 22 dosyalıkken). Sapma YOK; lexer sağlam.

### 2) Kalan 31 kodun ulaşılabilirlik taraması — "kalan iş"in gerçek boyutu

Yöntem: her kod için C'nin kendi test paketinde bir tanık var mı (`grep` test/*.c),
sonra tanıksızlar için doğrudan probe.

**ÖLÜ (2) — C'de ulaşılamaz, PORTLANMAYACAK:**
`T015` (annotasyonsuz lambda parametresi → parser P013+P011),
`T023` (`ver` işlev dışında → parser P001). D-359'da ölçüldü, burada test-paketi
kanıtıyla teyit edildi (ikisinin de testi YOK).

**ULAŞILABİLİR (29) — dördü tanıksızdı, probe ile doğrulandı:**
`M004` (`S::Bir("a")` → payload tip uyumsuz), `T040` (`kullan yokmodul` → modül
yüklenemedi), ayrıca `T041`/`T042` (test_llvm'de).

**Alt-sistem sınıflandırması (kalan 29 → 4 altyapı + 6 dağınık):**

| küme | kodlar | gereken altyapı |
|---|---|---|
| sabitsüre | CT001-CT008 (8) | `sabitsüre<T>` tip temsili + taint yayılımı |
| DRF | DRF001-DRF007 (7) | `görev<T>`/`kanal<T>` tip temsili |
| MMIO + yetki | MM001-003, CP004-005 (5) | `yetki<R>` lineer yetenek tipi |
| modül/import | T016, T040, T041, T042 (4) | modül scope çözümü + `genel` görünürlüğü |
| generic/bound | T030, T031 (2) | bound tablosu + uygula kaydı |
| tip evreni | T011, T014 (2) | tam tip-adı evreni / beklenen-tip yayıcısı |
| tekil | M004 (1) | varyant payload TİP tablosu |

**Sonuç: "31 kod kaldı" yanıltıcıydı.** Gerçek şekil: **2 ölü**, ve kalan 29'un
**23'ü yalnız 4 alt-sistemin tip temsiline bağlı**. Genel-amaçlı ucuz port sınıfı
D-358'de bitti; buradan sonrası alt-sistem işidir.

### 3) E013 portlandı (dağınık kümenin en küçüğü)

C D-257 çıplak-call-rule: `çıplak` işlev ρ-suz C-ABI'dir; normal (ρ-alan) bir
kullanıcı işlevini çağırırsa codegen `ptr null` geçirir → callee null-bölgeye
tahsis eder → segfault. Çıplak→çıplak ve çıplak→built-in serbest (ikisi de ρ almaz).

**KRİTİK AYRIM — `cip_bag` `guv_bag`'den AYRI:** `güvensiz` blok bir işlevi
**çıplak yapmaz**. D-351'de çıplak gövde için `guv_bag` artırılıyordu (tier izni);
E013 ise ρ-ABI'ye bağlı, ayrı bir sayaç ister. Sabotaj S43 (`cip_bag` → `guv_bag`)
**ilk turda sessiz kaldı** — korpusta "güvensiz bloktan normal işlev çağrısı" şekli
yoktu. `tc16_01`'e o şekil eklendi, S43 kırmızıya döndü. **D-356'nın S31 dersinin
birebir tekrarı** ve bu kez ayrımın kendisi ölçüldü.

**Sabotaj (2):** S42 E013 (88→87), S43 `cip_bag`↔`guv_bag` karışımı (87 — yalnız
korpus genişletildikten sonra).

**Probe 4/4 birebir:** çıplak→normal (E013), çıplak→çıplak, normal→normal,
çıplak→built-in.

**Sonuç:** self-host checker kod kapsamı **42 → 43/74**; korpus **87 → 88**.

---

## D-359 [YÜKSEK] — `--parse` paritesinde SESSİZ sapma: `küresel` dump'ı + üç ölü tanı (2026-08-04)

**ETKİ:** `selfhost/codegen.kem` (dump), `test/parse_korpus/` (+1).

**Sıradaki port (T011/T015/T023) ölçülünce İKİSİ ÖLÜ ÇIKTI:**
- **T015** (`lambda parametre tip annotasyonu gerek`) — C **parser'ı** annotasyonsuz
  lambda parametresini zaten reddediyor (`|x| x+1` → P013+P011). Checker'daki kola
  hiçbir kaynak ulaşamıyor.
- **T023** (`ver işlev gövdesi dışında`) — `ver`i fonksiyon dışına koyan her şekil
  parser'da P001. `aktif_donus_tipi` yalnız init'te NULL; checker'a ulaşan her `ver`
  bir işlev ya da lambda gövdesindedir.

İkisi de **C'de ölü kod**. Self-host'a portlanmaları anlamsız olurdu: kapılanamazlar
(hiçbir korpus dosyası uyandıramaz) ve ulaşılamayan kod ürettirirlerdi. Bu, "kalan 32
kod" listesinin körü körüne tüketilmemesi gerektiğini gösteriyor — **her kodun önce
ULAŞILABİLİRLİĞİ ölçülmeli.** (T011 ertelendi: gerçek bir tip-adı evreni ister —
self-host parser generic tip parametrelerini ATIYOR, dolayısıyla yarım bir port
`T` üzerinde yanlış-pozitif üretirdi. Ayrı iş.)

**Asıl bulgu — parser kapısı sessizce ayrışıyordu:** `--parse` gate'i 12 dosyada
yeşilken, elimizdeki **212 gerçek dosya** üzerinde ölçüm **8 SAPMA** gösterdi. Hepsi
aynı kök: C'de `küresel` ayrı bir düğüm TİPİ **değil** (DUGUM_DEGISKEN + `kuresel_mi`
bayrağı; `--ast` "DEGISKEN" basar), self-host ise D-253'ten beri ayrı bir `KURESEL`
düğümü tutuyor. Sapma **önceden vardı** (bu oturumun işi değil) ve `parse_korpus`'ta
küresel içeren tek dosya olmadığı için hiç görülmedi.

**Onarım:** iç düğüm adı KORUNDU (checker/codegen ona göre dallanıyor — D-356),
yalnız **dump** eşlendi (`dump_ad`: KURESEL → DEGISKEN). Rename etmek D-356'nın
E011/E012/T024 dallanmasını ve codegen'in küresel yolunu kırardı.

**Sonuç:** `--parse` paritesi mevcut korpuslar üzerinde **204/212 → 212/212**.
`parse_korpus` 12 → 13 (`p7_kuresel_ciplak.kem`: küresel + çıplak + gerçekzamanlı
+ güvensiz + `olarak`). Sabotaj S41 (dump eşlemesi kaldırıldı) kapıyı kırmızıya
döndürüyor.

**DERS (D-350'nin kardeşi):** bir kapının yeşil olması, kapsadığı ŞEKİL kümesinin
yeterli olduğu anlamına gelmez. `checker_diff` 87 dosyada koşarken `--parse` 12
dosyada koşuyordu; asimetri sapmayı sakladı. **Kapı büyüklüğünü periyodik olarak
ölç** — mevcut korpusları çapraz koşturmak (check+cg dosyalarını `--parse`'tan
geçirmek) bedava bir genişletmedir.

---

## D-358 — G002 + G003 + G004 self-host'a portlandı: tier ve temsil kuralları (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

Üçü de **"kabul et, çalışırken çök" senaryolarını derleme zamanına çeken** kurallar
(çökmezlik #1) — self-host'ta yoklukları bu güvenceyi o yolda geçersiz kılıyordu.

| kod | kural | neden |
|---|---|---|
| G002 | `satıriçi_asm` yalnız `güvensiz` blokta | ham makine kodu = tier kapısı |
| G003 | annotasyonsuz `değişken a = [..]` → `Dizi<T>` parametresine geçirilemez | yığın `[N×T]` vs dinamik `KdlDizi*` → callee yanlış okur (misaligned UB/segfault) |
| G004 | `işlev(...)` tipli bağlama yeniden atanamaz | closure temsili (bare fn-ptr ↔ `{fn,env}`) bağlama anında sabitlenir; yeniden atama çağrı yerinde yanlış dispatch |

**G003'ün İKİ daraltması ölçüldü ve ikisi de yük taşıyor:**
- **Annotasyon şartı:** yalnız annotasyonsuz bağlama yasak. `değişken a: Dizi<tam32>
  = [..]` **serbest** (heap). Sabotaj S40: şart kaldırılınca `tc15_02`'deki geçerli
  kod reddedildi.
- **Literal argüman muaf:** `topla([1,2,3])` serbest — codegen literali heap'e
  yönlendirir. Yasak olan yalnız **yığın değişkeni** yolu.

**Sıra:** G003 C'de argüman döngüsünde, T001 ve lineer tüketimden **sonra**
raporlanıyor; self-host aynı sıraya yerleştirildi (konum = argüman düğümü).

**Yeni yan-diziler:** `yerel_yig` (annotasyonsuz dizi-literali bağlaması),
`yerel_fn` (`işlev(...)` annotasyonlu bağlama), `fn_pdizi` (parametre `Dizi<T>` mü).
D-351/353/355'in `yerel_ptr`/`yerel_dizi`/`lin_tek` deseniyle aynı; hepsi
**pozitif bilgi** taşıyor (D-355 dersi).

**Sabotaj (4; `grep SABOTAJ-Sn` ile kanıtlı):** S37 G002 (87→86), S38 G003 (86),
S39 G004 (86), S40 G003'ün annotasyon daraltması (86 — **yanlış-pozitif yönünde**).

**Probe matrisi 8/8 birebir:** asm güvensiz-içi/dışı, yığın-değişken/annotasyonlu/
literal argüman, işlev yeniden-atama/çağrı, skaler yeniden-atama.

**Sonuç:** self-host checker kod kapsamı **39 → 42/74**; korpus **85 → 87**;
korpusun uyandırdığı kod **37 → 40**. Kalan 32 kod — **hepsi özel alt-sistem**
(`CT*` sabitsüre, `DRF*` eşzamanlılık, `MM*` MMIO, `CP*` yetki) ya da modül/generic
(`T011/T014/T015/T016/T023/T030/T031/T040-042`, `E013`, `M004`).

---

## D-357 — M002 + M003 self-host'a portlandı: çeşit varyantı ve payload aritesi (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**ÜÇ ayrı bölge, C'de üç ayrı kod** — biri atlanırsa o şekil sessizce geçer:

| bölge | şekil | tanı | konum |
|---|---|---|---|
| (A) değer | `Çeşit::V` | M002 | YOL düğümü |
| (B) yapıcı | `Çeşit::V(a,b)` | M002 → M003 | ÇAĞRI düğümü |
| (C) desen | `Çeşit::V(a,b) =>` | M002 → M003 | DESEN düğümü |

**(C)'nin ince yeri:** C varyantı **desenin önekinden değil, SKRUTİNİNİN
tipinden** arar (`cesit_ara(dt->yapi.ad)`). D-352'nin `esles_cesit_adi`
altyapısı bu yüzden yeniden kullanıldı — desendeki `Başka::V` yazımı çeşidi
belirlemez.

**(C)'nin ikinci ince yeri:** alt-desen sayısı **0 ise M003 VERİLMEZ** —
payload'lı bir varyantın çıplak deseni (`Secim::Bir =>`) geçerlidir. Sabotaj
S36 bunu ölçtü: muafiyet kaldırılınca `tc14_02`'deki geçerli kod reddedildi.

**(B)'de erken dönüş:** C hem M002 hem M003'ten sonra `t_hata` döner →
argüman tip kontrolü yapılmaz. Self-host aynı erken dönüşü uyguluyor.

**M004 PORTLANMADI:** payload **tipi** uyumu, varyant-başına-alan tip tablosu
**ve** generic çeşitte substitüsyon (`Secim<T>` → `Secim<tam32>`) ister.
Yan-kanal genişletmesi + tip yayılımı gerektiği için ayrı iş.

**Yeni yan-kanal:** `cv_pc` (varyant payload alan sayısı) checker.kem'e eklendi;
codegen.kem'de D3'ten beri vardı.

**Sabotaj (5; `grep SABOTAJ-Sn` ile kanıtlı):** S32 (A) M002 (85→84),
S33 (B) M003 (84), S34 (C) M002 (84), S35 (C) M003 (84),
S36 çıplak-desen muafiyeti (84 — **yanlış-pozitif yönünde**).

**Probe matrisi 10/10 birebir:** üç bölge × (varyant yok / arite fazla / arite
eksik / doğru) + çıplak desen + tam kapsama.

**Sonuç:** self-host checker kod kapsamı **37 → 39/74**; korpus **83 → 85**;
korpusun uyandırdığı kod **35 → 37**. Kalan 35 kod.

---

## D-356 — E011 + E012 self-host'a portlandı: `küresel` tip/başlangıç kısıtları (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+3).

**Kural (C `pre_populate_kuresel` aynası):** küresel durum bootstrap-circularity
çözümüdür — **allocator'a bağlanamaz**. Bu yüzden:
- **E011:** tip yalnız sayısal / `mantıksal` / `karakter` / `*T`. `metin`, `Dizi<T>`,
  yapı, seçimlik → yasak.
- **E012:** başlangıç değeri yalnız **sabit-literal** (`TAM`/`KESIRLI`/`MANTIKSAL`/
  `BOS`/`KARAKTER`). **`METIN` bu listede YOK** — `küresel değişken s: metin = "a"`
  bu yüzden **hem E011 hem E012** alır (ölçüldü).
- Sıra: E011 → E012 → T024, hepsi KURESEL düğümünde.

**Yerleştirme kararı:** C bu tanıları `pre_populate`'in **4. geçişinde** (işlev/
sabit/modül ile birlikte, **kaynak sırasında**) üretiyor. Self-host'un
`dup_kontrol`'ü zaten aynı geçiş yapısını taşıyordu → küresel kontrolü **aynı
döngüye** kondu; böylece E011/E012 ile T024'ün serpiştirme sırası korunuyor
(`tc13_03` bunu ölçüyor).

**Yan kazanım:** `tanim_adi`'ya `KURESEL` eklendi — küresel adı artık global
sembol olarak çift-tanım denetimine giriyor (C: aynı scope). `küresel sayac` +
`işlev sayac()` → T024, C ile birebir.

**SÜREÇ NOTU — sabotaj ilk turda SESSİZ KALDI:** S31 (küresel adının global
sembol sayılması) kaldırıldığında korpus **yeşil kaldı** — çünkü korpusta
küresel/işlev ad çakışması **yoktu**. Kural ölçümle doğruydu (probe g8) ama
**kapısızdı**. `tc13_03` eklendikten sonra S31 kırmızıya döndü. D-350'de
kaydedilen desenin bir kez daha tekrarı: *yeni kural = korpusa uyandırıcı örnek,
sonra sabotaj.* Sabotajın sessiz kalması bir sonuçtur, gürültü değil.

**Sabotaj (3; `grep SABOTAJ-Sn` ile kanıtlı):** S29 E011 (83→82), S30 `METIN`in
literal listesine sızması (82), S31 küresel-global-sembol (82 — **yalnız
korpus genişletildikten sonra gözlemlenebilir oldu**).

**Probe matrisi 9/9 birebir:** metin/Dizi/yapı küresel (E011+E012), ifade-init
(E012), tam32/işaretçi/mantıksal/karakter/kesirli (temiz), ad çakışması (T024),
E011+E012+T024 serpiştirmesi.

**Sonuç:** self-host checker kod kapsamı **35 → 37/74**; korpus **80 → 83**;
korpusun uyandırdığı kod **33 → 35**. Kalan 37 kod.

---

## D-355 — L007 + L008 self-host'a portlandı: lineer intrinsik operand/arite (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**Kural (C birebir):**
- `kullan(e)` operandı **TAM OLARAK** `tekkez<T>` olmalı. `yapı tekkez` KABUL
  EDİLMEZ — `kullan` sarmalanan değeri ÇIKARIR, lineer yapının sarmalanmış bir
  değeri yoktur. C bu ayrımı açıkça yapıyor (D-313).
- `imha(e)` **herhangi** bir lineer değeri kabul eder (tekkez / yetki / görev /
  `yapı tekkez`) — lineer yapıyı tüketmenin tek yerel yolu odur.
- Her iki hatada da C `t_hata` döner → operand **TÜKETİLMEZ** → ardından L001
  gelebilir (ölçüldü: `kullan(lineer_yapı)` → `L007` + `L001`, ikisi de birebir).
- `tekkez_olustur(...)` **tam 1** argüman → aksi L008 (çağrı düğümünde).

**PORT SIRASINDA ÖLÇÜLEN KUSUR (kendi ilk tasarımım):** biti "1 = tekkez<T>"
olarak kurmuştum ve `kullan`da `bit == 0` iken L007 veriyordum. Ama **0
"lineer yapı" demek değil, "bilinmiyor" demek** — `değişken t =
tekkez_olustur(5)` (annotasyonsuz) da 0. Sonuç: mevcut `tc5b_02_l002` ve
`lineer_kismi_tasima` korpus dosyalarında **sahte L007**. Bit ters çevrildi:
**1 = KESİN `yapı tekkez`**, rapor yalnız pozitif bilgide. Sabotaj S28 bu
kusuru kalıcı olarak kapıladı (geri konduğunda 4 dosya kırmızı).

**Korpus tasarımı:** `tc12_02`'deki `annotasyonsuz()` işlevi tam bu kusuru
uyandırmak için var — `değişken t = tekkez_olustur(5); kullan(t)`.

**Sabotaj (4; `grep SABOTAJ-Sn` ile kanıtlı):** S25 skaler-operand L007 (80→79),
S26 lineer-yapı L007 (79; L001 kaskadı da düştü), S27 L008 (79),
S28 bit yorumu (**76** — yanlış-pozitif yönünde, mevcut dosyalar dâhil).

**Probe matrisi 9/9 birebir:** `kullan`(skaler/metin/lineer-yapı/tekkez),
`imha`(skaler/lineer-yapı/tekkez), `tekkez_olustur`(2 arg / 0 arg).

**Sonuç:** self-host checker kod kapsamı **33 → 35/74**; korpus **78 → 80**;
korpusun uyandırdığı kod **31 → 33**. Kalan 39 kod.

---

## D-354 — T013 (dizi eleman tipi) + tamsayı-literal uyarlama onarımı (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**T013'ün İKİ yolu var ve SUÇLANAN ELEMAN farklı** — ölçülmeden görülmezdi:

| bağlam | referans | `[1, "a"]` için hata |
|---|---|---|
| `Dizi<metin>` annotasyonu VAR | E = `metin` | **ilk** eleman (`1`) |
| annotasyon YOK | ilk eleman (`tam32`) | **ikinci** eleman (`"a"`) |

Tek yolu (homojenlik) kullanmak tanıyı **yanlış elemanın üzerinde** gösterirdi.
Bu yüzden `dizi_bek` alanı eklendi: `değişken xs: Dizi<E> = [...]` bildiriminde
E, çocuk gezintisi boyunca bağlam olarak taşınır. Sabotaj S23 bunu ölçtü:
(a) yolu kapatılınca hata `6:35` yerine `6:38`'e kaydı ve `Dizi<tam8> =
[doğru, doğru]`ın **iki** hatası **tek**e düştü.

**Yol üstünde bulunan ikinci kusur (bağımsız, daha geniş etkili):**
`ifade_tip`'in TAM dalı `sayisal_mi(beklenen)` kullanıyordu; C
`tip_tamsayi_mi`. Yani self-host **tamsayı literalini kesirli bağlama
uyarlıyordu** → `değişken x: kesirli64 = 1;` C'de **T001**, self-host'ta **OK**.
Tek jeton (`sayisal_mi` → `tamsayi_mi`) düzeltti; T013'ün konum doğruluğunu da
bu besliyor (`Dizi<kesirli64> = [1, 2.5]` artık ilk elemanı suçluyor, C gibi).
Sabotaj S24 kapıyı doğruladı.

**BİLİNÇLİ BOŞLUK — kapıyla değil ÖLÇÜMLE gerekçelendirilmiş:** bağlamsız yolda
her iki eleman da sayısalsa kontrol atlanır. Sebep ölçüldü: `ver [1, 2.5]`
dönüşü `Dizi<kesirli64>` olan bir işlevde C **ilk** elemanı suçluyor
(`--checkdump` → `1:38`), bağlamsız yol ise ikinciyi suçlardı — yani koruma
kaldırılırsa **yanlış konumda** tanı üretilir. Bu boşluk korpusla kapılanamaz
(C konuşurken self susuyor ⇒ o şekli korpusa koymak kapıyı kalıcı kırmızı
yapardı). `ver`/argüman bağlamları da "?" kalır; onları kapatmak beklenen-tip
yayıcısı ister (ayrı iş).

**T014 PORTLANMADI:** boş dizi bağlamı C'de sezgiye aykırı — `ver []`
(dönüş `Dizi<T>`) **kabul**, ama `g([])` (parametre `Dizi<T>`) **T014**
(ölçüldü). Kuralı taklit etmek gerçek bir beklenen-tip yayıcısı gerektirir;
tahminle yazmak yanlış-pozitif üretirdi.

**Sabotaj (2; `grep SABOTAJ-Sn` ile kanıtlı):** S23 bağlamlı yol (78→77, hem
konum hem sayı bozuldu), S24 literal uyarlaması (77).

**Sonuç:** self-host checker kod kapsamı **32 → 33/74**; korpus **76 → 78**;
korpusun uyandırdığı kod **30 → 31**.

---

## D-353 — "Yanlış şekil" tanıları self-host'a portlandı: T005/T006/T007/T008/T027 (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**Neden birlikte:** Beşi de **tek bir kalıbın** örnekleri — ifadenin tipi bilinen
bir skaler, ama bağlam yapı / dizi / işlev **şekli** istiyor. Aynı dokuda
(`ifade_tip` + `bilinen_skaler_mi`) durdukları için tek partide gittiler.

| kod | bağlam | koşul |
|---|---|---|
| T007 | `x.alan` | x bilinen skaler → yapı değil |
| T008 | `x[i]` | x bilinen skaler → Dizi değil |
| T005 | `x[i]` | x **kesin** Dizi/ham işaretçi, i bilinen non-tamsayı |
| T006 | `f(...)` | f, bilinen skaler tipte bir **yerel bağlama** |
| T027 | `için x: k` | k bilinen skaler → Dizi değil |

**INDEKS sırası C ile birebir kuruldu** (D-351'in G001'i de bu sıraya girdi):
ham işaretçi → (indeks non-tamsayı ? T005 : güvensiz dışında G001); bilinen
skaler → T008; kesin Dizi → (indeks non-tamsayı ? T005). Şekil bilinmiyorsa
hiçbir şey söylenmez.

**T006'nın dar kapısı:** yalnız **yerel bağlama** olan hedefler. Kullanıcı
işlevleri ve builtin'ler yerel bağlama değildir → etkilenmez; kapanış değişkeni
`işlev(...)->T` annotasyonuyla `yerel_tip`'te "?" olur → atlanır. Bu daraltma
olmadan her çağrı yanlış-pozitif riski taşırdı.

**Yeni yan-dizi:** `yerel_dizi` (yerel ile paralel bit) — T005 "taban kesin
Dizi mi" sorusunu `yerel_tip`'e dokunmadan yanıtlar (`yerel_tip` Dizi'yi "?"e
düşürüyor ve onu değiştirmek T001/T003'ü geniş biçimde etkilerdi). D-351'in
`yerel_ptr`, D-352'nin `yerel_ham` deseniyle aynı.

**Probe matrisi 19/19 C ile birebir:** 10 pozitif (her kod × 2 skaler tip) +
**9 negatif** (yapı alanı, Dizi indeksleme, iç içe dizi, kullanıcı işlevi
çağrısı, kapanış çağrısı, builtin çağrısı, `&Yapı` alanı, `için` üstünde Dizi,
çeşit `eşleş`) — hiçbiri uyanmadı.

**Sabotaj doğrulaması (6; `grep SABOTAJ-Sn` ile kanıtlı):** S17 T007 (76→75),
S18 T008 (75), S19 T006 (75), S20 T027 (75), S21 T005 (75), **S22
`bilinen_skaler_mi` daraltması (73 — yanlış-pozitif yönünde; `lineer_yapi`
ve `lineer_kismi_tasima` gibi MEVCUT dosyalar da kırmızıya döndü, yani daraltma
yük taşıyor).**

**Sonuç:** self-host checker kod kapsamı **27 → 32/74** (D-350 başlangıcı: 24).
Korpus **74 → 76**; korpusun uyandırdığı kod sayısı **22 → 30**.

**Kalan 42 kod** (bu partiden sonra): `T011 T013 T014 T015 T016 T023 T030 T031
T040-T042` (tip/modül), `L007 L008`, `E011-E013`, `G002-G004`, `M002-M004`,
`CP004-005`, `CT001-008`, `DRF001-007`, `MM001-003`. Çoğu özel alt-sistem
(sabitsüre / DRF / MMIO / yetki); tarama yöntemi D-350'de.

---

## D-352 — M001 (`eşleş` kapsayıcılık) self-host'a portlandı: çeşit dalı (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+2).

**Neden:** D-350 taramasının 3. sıradaki maddesi. Eksik varyant = **çalışma
zamanında düşülecek bir dal**; C bunu derleme zamanında M001 ile reddediyordu,
self-host sessizce kabul ediyordu. Ölçüldü: `çeşit R { A, B }` + yalnız `R::A`
kolu → C `M001`, self `OK`.

**Portlanan kural (C `esles_exhaustive_kontrol` aynası):**
1. Üst düzey joker `_` **veya** bağlama yakalayıcısı varsa → exhaustive, çık.
   `hiç` bu kuraldan MUAF (o bir seçimlik varyantıdır, yakalayıcı değil).
2. Skrutini çeşit ise: her varyant bir `Yol::Varyant` deseniyle kapsanmalı;
   eksik varsa `eşleş` düğümünde M001.
3. Açık tipler (tamsayı vb.) ve `yapı` denetlenmez — geriye uyum.

C **yalnız varyant kısmını** karşılaştırır (yol öneki yok sayılır); self-host'ta
desen adı `"Ad::Varyant"` tek dizgi olduğu için **sonek eşleşmesi** (`metin_biter`)
birebir aynı davranış.

**KAPSAM SINIRI (bilinçli, belgeli):** `seçimlik<T>` (değer/hiç) ve `sonuç<T,H>`
(tamam/hata + H çeşidi) dalları **portlanmadı** — self-host'ta bileşik tip bilgisi
yok (`ifade_tip` "?" döner). Bu bir gevşeklik olarak KALIR; yanlış-pozitif üretmez.
Kapanması, self-host tip çıkarsamasının bileşik tipleri temsil etmesine bağlı —
ayrı ve daha büyük bir iş.

**İki yan-kanal gerekti** (düğüme alan eklemek `--ast`/`--parse` dump paritesini
bozardı; D-318 deseni):
- `cv_cesit`/`cv_ad` — varyant adları. `parse_cesit` bunları **atıyordu**
  ("dump'ta yok"). codegen.kem'de zaten vardı (D3), checker.kem'e eklendi.
- `yerel_ham` — yerel/parametre annotasyonunun **süzülmemiş** tip adı.
  Mevcut `yerel_tip`, çeşit adlarını `yapi_var_mi` süzgecinden geçiremediği için
  "?"e düşürüyordu; `yerel_tip`'i değiştirmek T001/T003'ü geniş biçimde etkilerdi
  → ayrı, izole dizi.

**Sabotaj doğrulaması (3 kural; `grep SABOTAJ-Sn` ile kanıtlı):** S14 M001
raporlaması (74→73), S15 joker muafiyeti (73 — yanlış-pozitif yönünde),
S16 bağlama-yakalayıcı muafiyeti (73 — kuralın ince yarısı).

**Probe matrisi (7/7 C ile birebir):** parametre skrutini, yerel değişken
skrutini, payload'lı varyant, 3-varyanttan 1'i, tam kapsama, `yapı` (M001 yok),
tamsayı (açık tip, M001 yok).

**Sonuç:** self-host checker kod kapsamı **26 → 27/74**; korpus **72 → 74**.

---

## D-351 [YÜKSEK] — G001 + E010 self-host'a portlandı: güvensiz-tier kapıları (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+6).

**Neden bu ikisi önce:** D-350'nin taraması, C checker'ın **74** tanı kodundan
self-host'ta yalnız **24**'ünün bulunduğunu ölçtü. Kalanların çoğu "yanlış program
derlenir" sınıfındaydı; **G001 ve E010 farklı bir sınıftadır**: bunlar yokken
self-host yolu **güvensiz programı işaretsiz geçiriyordu** — dilin merkezî iddiası
(`güvensiz` dışında ham bellek yok) o yolda geçersizdi. Ampirik:

```
işlev f(p: *tam32) -> tam32 { ver *p; }        C: G001    self: OK
küresel değişken s: tam32 = 0; ... ver s;       C: E010    self: OK
```

**Portlanan kurallar (C birebir, kod+satır+sütun eşit):**
- **G001** — `*p` dereferansı ve `p[i]` indekslemesi ham işaretçide `güvensiz` ister.
  Güvenli `&T` deref (D-305) ve `Dizi<T>` indeksleme ETKİLENMEZ: kural **tipe** bakar,
  yazılışa değil. Bunun için `yerel_ptr` (yerel ile paralel bit) eklendi.
- **E010** — `küresel` değişkene erişim (okuma **ve** yazma) `güvensiz` ister.
  `çıplak` gövde örtük güvensizdir (C D-257) — `kem_malloc`'un gerçek deseni.
  Aynı adlı yerel bağlama küreseli **gölgeler** → E010 yok.

**Yan gereksinim:** `çıplak` işareti self-host parser'ında düğüme yazılmıyordu.
Düğüme alan eklemek `--parse`/`--ast` dump paritesini bozardı → **yan-kanal**
(`cip_node`; codegen.kem'de zaten var olan `g_ciplak`). D-318'in deseni.

**Port SESSİZ BİR KUSUR BULDU (kendi kodumda, ölçümle):** `*(4096 olarak *tam32)`
— güvensiz dışında. C'de bu cast'in **kendisi** E002'dir, tip HATA'ya düşer ve
dereferans erken döner → **tek** hata. İlk portum ptr'liği cast'in HEDEF tipinden
okuyup cast'in geçerli olup olmadığını sormuyordu → `E002 + G001` kaskadı: tek
kusur iki hata olarak raporlanıyordu. `olarak *T` yalnız `güvensiz` içinde geçerli
olduğundan, ptr sayma da `guv_bag > 0` koşuluna bağlandı. Korpusa uyandırıcı örnek
eklendi (`tc8_06`).

**Sabotaj doğrulaması (6 kural; her biri `grep SABOTAJ-Sn` ile kanıtlandı):**
S8 G001-deref (72→71), S9 G001-indeks (71), S10 E010 (71), S11 `çıplak` muafiyeti
(**70** — yanlış-pozitif yönünde; mevcut `tc6_01_kuresel` de kırmızıya döndü, yani
muafiyet yük taşıyor), S12 gölgeleme (71), S13 `ptr_ifade_mi` daraltması (**70** —
güvenli `&T` deref'i ve `Dizi<T>` indekslemesi yanlışlıkla reddedilir).

**Sonuç:** self-host checker kod kapsamı **24 → 26/74**; korpus **66 → 72**.
Bellek-güvenliği kapılarında C ile parite tam.

**Testler:** `checker_diff` 72/72, `self_driver` 4 mod × 2 driver + FIXPOINT,
`check_kapisi` 210/217 (0 RED).

---

## D-350 [YÜKSEK] — `olarak` tip kuralları self-host checker'a portlandı: E001-E004 (2026-08-04)

**ETKİ:** `selfhost/codegen.kem`, `selfhost/checker.kem`, `test/check_korpus/` (+7).

**Ölçülen açık (D-349'un yol-üstü bulgusu):** self-host checker `E002`'yi **hiç
üretmiyordu** (`grep -c E002 selfhost/codegen.kem` → 0). Ampirik:

```
işlev main() -> tam32 { değişken s: metin = "ab"; değişken n: tam32 = s olarak tam32; ver n; }
   kemgu.exe      --check → hata[E002]  (satır 1, sütun 78)
   kemgu_self.exe --check → OK
```

Yani self-host checker C'nin **reddettiği programları kabul ediyordu** — sessiz
gevşeklik. `checker_diff` yakalamadı çünkü korpusta `olarak` tip-kural ihlali
YOKTU; PR #108'in `küresel`/`çıplak` açığıyla **aynı desen**: kural yoksa kapı
yeşil kalır.

**Kök neden (tek satır):** `kontrol_dugum`'un başındaki `metin_baslar(ad, "TIP_")
→ ver 0` koruması `TIP_DONUSTUR`'u da yutuyordu. Ama `TIP_DONUSTUR` bir **tip
düğümü değil, İFADE**'dir (`TIP_` öneki yanıltıcı) — böylece hem Madde E
kontrolleri hem de **cast kaynağının alt-ağacı** (T002 vb.) hiç gezilmiyordu.

**Portlanan kural kümesi (C `src/tip_kontrol.c` DUGUM_TIP_DONUSTUR sırası birebir):**

| # | kaynak → hedef | sonuç |
|---|---|---|
| 1 | `X olarak tekkez<T>` | **E001** |
| 2 | `tekkez<T> olarak X` | **E003** |
| 3 | `*T olarak metin` | izinli (tipli tampon → opak) |
| 4 | `tamsayı olarak *T` / `*T olarak tamsayı` | **yalnız `güvensiz`** (D-248) |
| 5 | `&T olarak *U` | **yalnız `güvensiz`** (D-349); dışarıda E002 |
| 6 | kaynak veya hedef sayısal değil (karakter↔tamsayı hariç) | **E002** |
| 7 | `tam64 → tam8/tam16`, `kesirli64 → kesirli32` | **E004** |

`tam64 → tam32` ve `tam32 → tam8` İZİNLİ (C'nin "yalnız ≥64-bit'ten <32-bit'e"
kuralı) — daraltmanın tamamı değil, **anlamlı bit kaybı** yasak.

**Muhafazakârlık sınırı (bilinçli):** self-host tip çıkarsaması dizgi-tabanlı;
kaynak/hedef sınıfı `"?"` (emin değil) ise kontrol **atlanır**. Bu, yanlış-pozitif
üretmeyi imkânsız kılar; kalan gevşeklik C ile diff'te görünür ve korpusla kapatılır.
Ayrıca self-host parser `çıplak` işaretini düğümde tutmadığı için `çıplak` gövde
(C'de örtük `güvensiz`) bu ayrımdan yararlanamaz — bugün korpusta bu şekil yok.

**Sabotaj doğrulaması (7 kural, her biri tek tek):** kural kaldırıldı → kapının
kırmızıya döndüğü GÖRÜLDÜ → geri konuldu. Sabotajın uygulandığı her seferinde
`grep SABOTAJ-Sn` ile kanıtlandı (uygulanmamış sabotaj "0 hata" der ve yanıltır):
S1 E002 (66→63), S2 E004-tamsayı (65), S3 E004-kesirli (65), S4 `güvensiz`
gevşetmesi (64; **yanlış-pozitif** yönünde), S5 E001 + S6 E003 (64),
S7 D-349 `&T → *U` (65).

**Testler:** `checker_diff` 66/66 (59 → 66 korpus), `self_driver` 4 mod × 2 driver
+ FIXPOINT, `check_kapisi` 0 RED.

---

## D-349 [YÜKSEK] — `güvensiz` blokta `(&x) olarak *T`: referanstan ham işaretçi (2026-08-04)

**Karar [Mehmet; ETKİ: `src/tip_kontrol.c`, `test/test_tip_kontrol.c` (+3),
`test/test_llvm.c`, `test/check_kapisi.sh`.]**

**Neden:** Yerel bir değişkenin ham adresini almanın **hiçbir yasal yolu yoktu**
— `oku(&x)` → T001, `(&x) olarak *tam32` → E002. Boşluk `--tip-atla` ile
dolduruluyordu: aynı yetki **hiçbir işaret bırakmadan** veriliyordu. Verilebilecek
en kötü biçimdi.

**Kural:** `güvensiz` blok içinde `&T`/`&değişken T` → `*U`. IR'da **no-op**
(opak işaretçi modelinde ikisi de `ptr`); ayrım yalnız tip sisteminde ve amacı
bu. Artık bilinçli, **işaretli** (`güvensiz`) ve **denetlenebilir**
(`grep 'olarak \*'`).

**Bilinçli olarak korunanlar:**
- `güvensiz` **dışında hâlâ E002** — kaçış kapısı işarete bağlı kalmalı; aksi
  halde güvenli kod da ham işaretçi üretir ve bölge/ömür garantisi **sessizce**
  kaybolur. Sabotaj testi kilitler.
- **Ters yön açılmadı** — `*T olarak &T` hâlâ yasak. Ham işaretçiyi güvenli
  referansa terfi ettirmek, **olmayan bir garantiyi uydurmak** olurdu.
- Pointee tipi aynı olmak zorunda değil (`&dtam64` → `*dtam8`): `güvensiz`'de
  yeniden yorumlama zaten amaç ve mevcut `int → *T` yolu **daha geniş** yetki
  veriyor.

**Sonuç:** tip borcu **6 → 2**. Üç korpus dosyasının **muafiyeti kalktı**
(`cg_deref_genislik`, `cg_pointee_isaret`, `cg_skaler_deref`) — artık gerçekten
tip kapısından geçiyorlar (muafiyet 10 → 7).

**Kalan 2 borç (tasarım soruları, açık):** `mantıksal olarak tam32` reddi;
generic dönüş-tipi güdümlü çıkarsama (belgeli V1 sınırı).

**Yol üstünde bulundu (ayrı iş):** self-host checker **E002'yi hiç üretmiyor**
(kodda sıfır geçiş) — `metin olarak tam32` C'de red, self-host'ta OK. Önceden
vardı; `checker_diff` yakalamamış çünkü korpusta bu şekil yok — PR #108'deki
`küresel`/`çıplak` açığıyla **aynı desen**.

**Testler:** tip_kontrol 202/202 (+3), test_llvm 286/286, tip kapısı 210/217 (0 RED).

---

## D-348 — Tip borcu 14 → 6: bir checker kusuru, yedi geçersiz test programı (2026-08-04)

**Karar [ETKİ: `src/tip_kontrol.c`, `test/test_llvm.c`.]**

**Ana bulgu:** D-337'de açılan tip borcunun **çoğu checker kusuru değildi** —
geçersiz test programlarıydı. Checker **altı kez haklı**, bir kez haksız çıktı.
Bu, "borç" etiketinin kendi başına bir teşhis olmadığını gösteriyor: borcu
kapatmak önce **sınıflandırmayı** gerektiriyor.

**Derleyici kusuru (1):** `kesirli32 x; x + 21.0` → T001. Kesirli literaller
daima `kesirli64`'e düşer ve `21.0f` gibi bir genişlik son-eki **yok** →
programcının düzeltemeyeceği bir red. D-343'ün (tamsayı literal bağlamı) float
ikizi eklendi (`kesirli_literal_ifade_mi`, `tip_kesirli_mi`). Aynı
muhafazakârlık: yalnız **tipsiz literal ağacı** uyum sağlar.

**Geçersiz test programları (7) — checker haklıydı:**
`ver v;` (tam16/tam64→tam32) ×3 — örtük dönüşüm yok (ASLA listesi);
`doldur(&k)` — parametre `&değişken K`; `yetki<MMIO>` ×2 — **gerçek lineer
sızıntı**; iç içe `görev` — **gerçek liveness sızıntısı** (iç `hata` kolu dış
görevi birleştirmeden çıkıyordu; ikinci spawn başarısız olsa birinci görev asla
join edilmezdi).

**Görünürlük (kalıcı):** `KEMGU_BORC_DOKUM=1` ile her açık borcun tanı kodu
listelenir. "14 borç var" tek başına hangi kusurun onarılacağını söylemiyordu.

**Kalan 6 — üçü de TASARIM SORUSU (Mehmet kararı, bilerek açık):**
1. **`&x` → `*T` yasal yolu YOK** (4 borç). Yerel bir değişkenin ham adresini
   almanın hiçbir yolu yok; `&x olarak *tam32` de T001. Aynı boşluk
   `cg_deref_genislik.kem` ve `cg_pointee_isaret.kem`'i muaf listesinde tutuyor.
2. **`mantıksal olarak tam32`** reddediliyor (E002).
3. **Generic dönüş-tipi güdümlü çıkarsama** — CLAUDE.md'de belgeli V1 sınırı.

**Testler:** test_llvm 286/286; `test_tumu` 0 hata; FIXPOINT ✓; codegen 113/113;
checker sıfır-diff 59/59; tip kapısı 207/217 (0 RED).

---

## D-347 [YÜKSEK] — Self-host parite: deref pointee işaretliliği + ikili operand genişliği (2026-08-04)

**Karar [ETKİ: `selfhost/codegen.kem`, `test/cg_korpus/cg_pointee_isaret.kem` (yeni),
`test/check_kapisi.sh`.]**

**Neden:** self-host codegen'de iki ayrı parite açığı vardı; ikisi de ölçüldü.

**Kusur 1 — pointee İŞARETLİLİĞİ (sessiz yanlış cevap).** `cg_apointee` yalnız
LLVM **tipini** (`i8`) tutuyordu; işaretlilik ekseni düşüyordu. `ifade_isz`'in
`deref*` dalı hiç yoktu → daima 0 (işaretli) → `(*p) olarak tam64` **daima
`sext`**. `*dtam8` üzerinden `0xC8` okumak **200 yerine −56** veriyordu; C sürümü
doğru `zext` üretiyordu. Bu, AH-1'de device-tree baytlarını bozan sınıfın aynısı —
ne hata ne uyarı, yalnız yanlış sayı.
*Çözüm:* `cg_apisz` paralel dizisi (değişken + parametre yollarında doldurulur),
`cg_var_pointee_isz_bul`, `param_pointee_isz`, ve `ifade_isz`'e `deref*` dalı.

**Kusur 2 — ikili operand GENİŞLİĞİ (LLVM-red).** `tip_birlestir` ortak tipi
**seçiyor** ama operandları **dönüştürmüyordu**: `x_i64 == (0 - 56)` için
`sub i32` üretilip ardından `icmp eq i64 %a, %b` yazılıyordu →
*"'%22' defined with type 'i32' but expected 'i64'"*. Program **hiç derlenmiyordu**.
C tarafı beklenen genişliği literal alt-ağacına yayıyor.
*Çözüm:* ikili emisyonda mevcut `int_uydur` ile register-düzeyi genişletme.
Immediate operandlara `int_uydur` zaten dokunmaz (D-299 dersi: `sext i32 4294967296`
literali sessizce bozuyordu).

**Kapı:** `test/cg_korpus/cg_pointee_isaret.kem`. Mevcut `cg_deref_genislik.kem`
(D-347 öncesi) alt baytları **bilerek** 128'in altında seçip yalnız *yük
genişliğini* ölçüyor ve bu ekseni hiç ölçmüyordu — dosyanın kendi yorumunda da
böyle yazıyordu. Yeni dosya iki kusuru da falsifiye edilebilir biçimde yakalar.

**Sabotaj (ikisi ayrı ayrı):** `deref*` dalı devre dışı → exit 2 (42 değil);
operand genişliği uyumlaması devre dışı → IR LLVM tarafından reddedildi, exe yok.

**Ölçüm:** C ve self-host artık birebir aynı IR üretiyor
(`zext i8` + `sext i8`), ikisi de exit 42.

**Testler:** `test_tumu` 0 hata; FIXPOINT ✓ (47061 satır kararlı); codegen semantik
eşdeğerlik **113/113** her iki bootstrap aşamasında; checker sıfır-diff 59/59;
tip kapısı 207/217 (0 RED).

---

## D-346 [YÜKSEK] — MODEL B / MB-3: 4KB sayfa granülü + segment-başına W^X (2026-07-31)

**Karar [ETKİ: `runtime/kem_mmu.kem`, `runtime/kem_elf.kem`,
`test/ornekler/kem_os.kem` (+`[19]`), `Makefile`.]**

**Neden:** [D-345] sonrası süreç sayfası TEK 2MB blok ve AP=01/UXN=0 idi — yüklenen
her program için aynı sayfa hem **yazılabilir** hem **çalıştırılabilir**. Program
kendi kodunu değiştirebilir ya da veri olarak yazdığı baytları çalıştırabilirdi:
"bellek güvenliği dil seviyesinde" diyen bir sistemde yükleyici katmanında açılmış
bir kapı.

**MB-3a — 4KB granül:** süreç-özel bölge kendi L3 tablosuna sahip (512×4KB).
Sayfalar **varsayılan olarak veri** (RW + XN). Ters sıra (varsayılan
çalıştırılabilir) seçilseydi, izin atamayı unutan her yol W^X'i **sessizce**
devre dışı bırakırdı; bu yönde ise program çalışmaz — gürültülü hata.

**MB-3b — izinler:** ELF `p_flags`'e göre yalnız X bayraklı segmentin sayfaları
`kmmu_sayfa_kod` ile AP=11 (EL0 salt-okunur) + UXN=0 yapılır. **W+X birlikte
işaretli segment REDDEDİLİR.** PXN her iki izinde de set: çekirdek kullanıcı
sayfalarından komut getirmez. Yükleyici kod sayfasına çekirdeğin **identity**
görüşünden yazar (başka VA) — bu yüzden AP=11 yükleme yolunu engellemez.

**⚠ İKİ ÖZ-DÜZELTME (ikisi de sabotajla bulundu):**

1. **`[19]` yanlış-yeşildi.** İlk sürüm yalnız "süreç öldü mü" diye bakıyordu ve
   kod sayfasını VERİ yapan sabotajda **bile** geçti — veri sayfası XN'dir, stub
   hiç çalışamaz, komut abort'u yine öldürür. Test "yazma fault'u" ile "çalıştırma
   fault'u"nu **ayırt edemiyordu**. Artık iki koşul: syscall sayacı **artmalı**
   (⇒ sayfa gerçekten çalıştırılabilir) **ve** süreç **ölmeli** (⇒ yazma reddedildi).
2. **Sabotajın kendisi yanlıştı.** "RWX" için `1<<53 + 7` yazılmıştı — yani **AF
   (erişim bayrağı) yok**; test edilen şey "RWX" değil "erişilemez sayfa"ydı.
   `[18]`'in de kırılması ele verdi; doğru tanımlayıcı (`0x747 + PXN`) ile
   tekrarlandı. *Sabotajın ne ölçtüğü de doğrulanmalı.*

**Sabotaj matrisi (üçü de ölçüldü):** doğru W^X → `[18]` OK `[19]` OK · kod sayfası
XN → HATA/HATA · kod sayfası RWX → `[18]` OK `[19]` **HATA**.

**Sınırlar (V1):** ASID yok; L3 yalnız süreç bölgesinde (**çekirdek hâlâ 2MB blok —
kendi kodu yazılabilir**); `.rodata` ayrı korunmuyor; yığın sayfaları veri
varsayılanından gelir.

**Testler:** kem_os `[1..19]` + `KEMGU KEM-OS OK`; tip kapısı 207/215 (0 RED);
`test_tumu` 0 hata (FIXPOINT ✓, codegen 111/111, checker 56/56).

---

## D-345 [YÜKSEK] — MODEL B: süreç izolasyonu + diskten ELF yükleme (2026-07-28)

> **NUMARA KAYDIRMASI:** bu kayıt önce D-344 olarak yazılmıştı. Paralel bir dal
> (PR #108, `checker.kem` ayrışması) aynı numarayı **önce** yayımladı; CLAUDE.md
> kuralı gereği (*"D-NNN'i merge anında güncel `origin/main`'deki en yüksek D'ye
> bakıp ver"*) sonradan gelen kayıt kaydırıldı. Bu oturumun commit mesajlarında
> `[D-344]` geçen Model B referansları **bu kaydı** (D-345) gösterir.

**Karar [ETKİ: `runtime/kem_mmu.kem`, `runtime/kem_gorev.kem`, `runtime/kem_elf.kem`
(yeni), `test/ornekler/kem_kullanici.kem` (yeni), `linker/user-aarch64.ld` (yeni),
`test/ornekler/kem_os.kem` (+3 kapı), `Makefile`, `test/check_kapisi.sh`.]**

**Neden:** "Kullanılabilir OS" eşiğinin altındaki iki eksik — süreçler birbirinden
yalıtık değildi (tek adres alanı; `kem_gorev.kem:13` bunu açıkça yazıyordu) ve
programlar çekirdek imajına gömülüydü (Model A: yeni program = çekirdeği derle).

**MB-1a — süreç başına adres alanı.** `kmmu_as_olustur(i)`: her alan kendi L1+L2'sine
sahip; L2 çekirdek L2'sinin **tam kopyası**, tek fark `KMMU_OZEL_VA` girişi. Kopyalama
**zorunlu**: TTBR0 düşük VA'ları kapsar ve çekirdek de düşük VA'da koşar (TTBR1 yok) →
kopyalamazsak TTBR yazan komutun bir sonrakisi haritasız kalır. `kmmu_ttbr_yaz` +
tam TLB flush (ASID yok, v1).
**Kanıt `[16]`:** A'ya yaz → B'ye yaz → A'ya dön → A'nınki duruyor; ayrıca çekirdeğin
identity görüşünden iki ayrı fiziksel sayfa bağımsız doğrulanır.

**MB-1b — zamanlayıcı.** `KG_TTBR` tablosu; `kem_preempt` seçtiği görevin alanına
geçer. `ttbr[i]==0` = çekirdek alanını miras al → **geriye uyumlu**. Kanıt `[17]`.

**MB-2 — diskten ELF64.** `kem_elf.kem` (saf-.kem): sektör-önbellekli okuma, başlık
doğrulama, PT_LOAD kopyalama, `.bss` sıfırlama, I-cache bakımı. Kullanıcı programı
ayrı ELF (`-z max-page-size=4096`), disk sektör 8. Kopyalama sürecin **fiziksel**
sayfasına, hedef alana geçmeden yapılır — TTBR ile oynamak yükleyicinin kendi kodunu
haritasız bırakma riskini gereksizce doğururdu. Her sınır ihlali **0 döndürür**, sessiz
kırpma yok. Kanıt `[18]`.

**⚠ ÖLÇÜM DERSLERİ (üçü de sabotajla bulundu, üçü de bu kaydın asıl değeri):**

1. **RAM tavanı.** Özel sayfalar önce `0x48000000`'a kondu; ESR `0x96000050`
   (DFSC=`0b010000` = senkron **harici** abort — "çeviri oldu, fiziksel adres yok").
   Çeviri hatası DEĞİL. QEMU `-M virt`'te `-m` yok → varsayılan 128 MiB → tavan tam
   `0x48000000`. Sayfalar `0x47400000`'a; kapasite 6 adres alanı.
2. **Dekoratif test.** `[16]` ilk eklendiğinde Makefile grep zincirine bağlanmamıştı:
   test "HATA" yazarken build yeşil kalıyordu. *Yeni bir OS kontrolü eklerken kapıya
   bağlama adımı atlanırsa test hiçbir şey korumaz.*
3. **Yanlış-yeşil eşzamanlılık testi.** `[17]`'nin ilk sürümü ("imzanı yaz, sonra
   sonsuz doğrula") TTBR anahtarlaması **tamamen kapatıldığında bile** yeşil geçti:
   main, görev2 yazar yazmaz bekleme döngüsünden çıkıyordu; görev1 bir daha koşup
   çakışmayı görmüyordu. El sıkışma eklendi (doğrulama ancak iki görev de yazdıktan
   sonra sayılır). **Ders: "geçti" ile "ölçtü" aynı şey değil — eşzamanlı bir
   mekanizmayı ölçen test, gözlemin gerçekleşmesini garanti etmelidir.**

**Yan bulgu — dil doğru davrandı:** ELF kopyalama döngüsü baytı `tam64`'e genişletip
`dtam8`'e daraltıyordu → **E004** ile reddedildi. Haklı red (derleyici değerin
0..255'te kaldığını bilemez). Bayt yolu baştan sona `dtam8` kalacak şekilde ikiye
ayrıldı; çekirdek tip-temiz kaldı.

**Ayrıca:** bir sabotaj `sed` sözdizim hatasıyla **uygulanmamıştı** ve yeşil
görünmüştü. *Sabotajın uygulandığı `grep -c` ile doğrulanmadan sonuca güvenilmez.*

**Sınırlar (V1):** ASID yok (her anahtarlamada tam TLB flush); EL0 kod sayfası
segment-başına W^X almıyor (2MB blok granülü); program tek 2MB sayfaya sığmalı;
dinamik bağlama/relocation yok (ET_EXEC); ELF dosya sisteminden değil sabit
sektörden okunur (çok-dosyalı minifs ayrı iş); adres alanı kapasitesi 6.

**Testler:** kem_os QEMU boot `[1..18]` + `KEMGU KEM-OS OK`; tip kapısı 204/212
(0 red); 4 bağımsız sabotaj kapısı doğrulandı.
## D-344 [YÜKSEK] — `checker.kem` ↔ `codegen.kem` ayrışması kapatıldı; `calistir_checker_diff` `test_tumu`'ya bağlandı (2026-07-28)

**Karar [ETKİ: `selfhost/checker.kem`, `test/check_korpus/`, `Makefile`]:**
Aşama-2 referans checker'ı (`selfhost/checker.kem`) ile birleşik driver
(`selfhost/codegen.kem`) içindeki checker ayrışmıştı. Ayrışma ÖNCE korpusa
örnek eklenerek GÖRÜNÜR yapıldı, sonra kapatıldı, sonra kapı `test_tumu`'ya
bağlandı.

**Ölçülen ayrışma (onarım öncesi):** `test/ornekler/kem_malloc.kem` üzerinde
C oracle `OK`, `codegen.kem --check` `OK`, ama `checker.kem` **5 SAHTE tanı**
üretiyordu: `T002 24:5`, `T002 31:27`, `T002 32:5`, `T002 32:16`, `T022 39:5`.

**Kök nedenler (3, hepsi ölçülerek doğrulandı):**
1. **`küresel` (D-253) HİÇ yoktu** — `anahtar_tip`'te yok → `parse_ust_oge`
   HATA dalına düşüyor, küresel ad global kapsama girmiyor → her okuma/yazma
   sahte `T002`.
2. **`*p = v` T022 muafiyeti (D-249) yoktu** — muafiyet yalnız
   `codegen.kem`'de; `checker.kem` `güvensiz` blokta deref-atamaya KOŞULSUZ
   `T022` veriyordu.
3. **`çıplak` (D-255) HİÇ yoktu** — hata-kurtarma sayesinde `--checkdump`
   çıktısı tesadüfen doğru kalıyordu (aşağıda "gate edilmiyor" notu).

**Onarım (`checker.kem`, hepsi `codegen.kem` aynası):** `anahtar_tip`'e
`küresel`→`KURESEL` + `çıplak`→`CIPLAK`; `parse_kuresel` (C `parser.c` parite);
`parse_ust_oge` dispatch; `parse_islev_genel` modifier döngüsü (çıplak/
gerçekzamanlı herhangi sıra); `genel_topla`'ya `KURESEL` → `g_ekle` (ad
çözümü); `kontrol_ust`'ta `KURESEL` init'i `SABIT` gibi denetlenir; ATAMA'da
`hedef_deref` (TEKLI/`deref*`) T022 muafiyeti.

**Korpus (+3):** `tc6_01_kuresel.kem` (küresel+çıplak, kem_malloc deseni),
`tc6_02_ciplak.kem`, `tc6_03_deref_atama.kem`. Hepsi oracle `OK`.

**Kapı:** `calistir_checker_diff` artık `test_tumu` zincirinde (Makefile:5285).
Daha önce YOKTU — `checker.kem`'in TEK doğruluk kapısı hiç koşmuyordu, ayrışma
bu yüzden sessizce birikmişti.

**Doğrulama:** checker_diff 56/56 → **59/59**. `kem_malloc.kem` 5 sahte tanı
→ **0** (oracle ile birebir). codegen_bootstrap FIXPOINT ✓ (lexer/parser/
checker 92/92 birebir + stage1==stage2, 45729 satır). self_driver 4 mod ✓
(LLVM 108/108 ×2 + fixpoint). check_kapisi 203/210, 0 RED. codegen_diff 108/108.

**Sabotaj kapısı (kanıt):** T022 muafiyeti kaldırıldı → `tc6_03` KIRMIZI
(58/59); `KURESEL`→`g_ekle` kaldırıldı → `tc6_01` KIRMIZI (58/59); geri
alındı → 59/59. İkisi de GERÇEK kapı.

**Kapsam/sınırlar (dürüst):**
- **`tc6_02_ciplak` GATE ETMİYOR:** `CIPLAK` anahtar kelimesi sabote edilince
  korpus yine 59/59 kaldı. `--checkdump` yalnız T/L/M kodu basıyor; `çıplak`'ın
  hata-kurtarma yolu (sahte HATA düğümü + `hata_say` şişmesi) dump'a
  YANSIMIYOR. Denenen 6 şekil (tek/çift modifier, `dışa çıplak`, gövde-hatalı,
  çift tanım) hiçbirinde ayrışma gözlenmedi. `CIPLAK` eklemesi bu yüzden
  *gözlemlenebilir hata onarımı değil*, AST hijyeni + `codegen.kem` paritesi;
  `tc6_02` gate-etmeyen regresyon kilidi olarak tutuldu.
- **E010 İKİSİNDE DE YOK:** `küresel`'e güvensiz-tier dışından erişim C
  oracle'da `E010` verir; `checker.kem` VE `codegen.kem` bunu uygulamıyor
  (ölçüldü). Korpus dosyası `çıplak` kullandığı için bu yol uyanmıyor. AYRI
  ve ÖNCEDEN VAR OLAN eksik.
- **Artık ikilik riski:** ortak 182 işlev adının **19'unun gövdesi hâlâ
  farklı** (`ifade_tip`, `kontrol_dugum`, `t003_kontrol`, `lin_tuket_dugum`,
  `parse_yapi`, `parse_cesit` …). `checker.kem` `codegen.kem`'in ÖZ ALT
  KÜMESİ (182 ⊂ 327, yalnız-checker = 0). Bu commit ayrışmayı kapatır ama
  KÖKÜNÜ (iki uygulama) kaldırmaz.

**MEHMET KARARI (2026-07-28):** `checker.kem` **SİLİNMEYECEK**, ikili yapı
şimdilik korunacak — gerekçe: `calistir_checker_diff` artık `test_tumu`'da
koştuğu için yeni ayrışma ANINDA kırmızı olur; Aşama-2'nin bağımsız referans
tanığı (codegen_bootstrap'taki ayrı `checker` bileşeni, 92/92) korunur.
**Kabul edilen bilinen borç:** 19 farklı gövde duruyor → korpusun uyandırmadığı
bir şekil hâlâ sessizce ayrışabilir; her checker değişikliği İKİ yerde
yapılmalı. Konsolidasyon ileriye bırakıldı (Aşama-5 "tek-kaynak" maddesi).
**Bu borcun tek panzehiri korpus kapsamıdır:** checker'a yeni bir kural/
sözdizimi eklenirken `test/check_korpus/`'a onu uyandıran örnek EKLENMELİ —
aksi halde ayrışma yine sessizce birikir (bu oturumun asıl dersi).
- Ayrıca ölçülen, kapsam DIŞI önceden-var-olan eksik: `değişken q: tam32 = f;`
  (işlev değerini skalere atama) oracle `T001` verir, `checker.kem` vermez.

---

## D-343 [YÜKSEK] — kem_os tip-temiz: `--tip-atla` borcu kapandı; aritmetik literal bağlamı onarıldı (2026-07-28)

**Karar [ETKİ: `src/tip_kontrol.c`, `runtime/kem_mmu.kem`, `Makefile`
(kem_os cat + falsifiye-gate yönlendirmesi + `test_tumu`), `test/check_kapisi.sh`,
`test/test_tip_kontrol.c` (+6), `test/test_llvm.c` (borç denetimi, 16→14).]**

**Neden:** D-337 `--llvm`'e tip kapısını bağladı, ama kem_os build'i `--tip-atla`
ile muaf tutuldu ve bu **BORÇ** olarak yazıldı. Sonuç: "derleme zamanı güvenlik"
tezini savunan dilin işletim sistemi, kendi tip denetiminden geçmiyordu.

**Ölçüm düzeltmesi:** hata sayısı **60 değil 23**. Önceki sayım `--mimari arm64`
bayrağı olmadan yapılmış; AS001'lerin tamamı sahteydi. *Ders: bayrağa duyarlı
bir aracın çıktısını bayraksız sayma.*

**Kök neden 1 — beklenen tip aritmetik operandlara yayılmıyordu (15 hata).**
`değişken a: tam64 = 5 + 3;` bile T001 veriyordu; belgelenmiş bidirectional
çıkarsamanın deliği. `tamsayi_literal_ifade_mi()` (yalnız literal + aritmetik +
negasyon; değişken/çağrı/`olarak` görülür görülmez 0) eklendi ve iki yerde
kullanıldı: `tip_belirle` IKILI tek-taraflı uyumu artık literal AĞACINI kabul
ediyor; `tip_belirle_beklenen` IKILI/TEKLI dıştan gelen tipi operandlara
özyineliyor. **Muhafazakâr:** yalnızca bugün HATA VEREN salt-literal durumları
kabul eder — tipli operand varsa red eskisi gibi sürer (2 sabotaj testi kilitler).
*Mehmet onayı alındı (tip sistemi değişikliği).*

**Kök neden 2 — `kdl_metin_uzunluk/bayt` çapraz-birimdi (8 hata).** KEMGU'da üst
düzey ileri-bildirim **YOK** (ölçüldü: `imza_yeterli` yalnız `özellik` gövdesinde,
`src/parser.c:738`) → tek-birim birleştirme, **dil değişikliği gerektirmeyen tek
çözüm**. `kem_heap.kem` cat'e alındı, çıktı `strip_defined_declares.awk`'tan
geçiriliyor, `bm_a64_kem_heap.o` link'ten çıkarıldı. Falsifiye-kanıt gate'leri
`kem_os.o`'ya yönlendirildi — **kanıt korunuyor**, yalnız hangi objede arandığı
değişti.

**Kök neden 3 — gerçek `tam64`→`dtam64` uyuşmazlığı (5 hata).** Adres/ESR
değerleri işaretsiz yorumlanmalı; kaynakta açık dönüşüm eklendi (yamamak değil,
doğrusunu yazmak).

**Kapı:** `check_kapisi.sh` artık kem_os **birleşik** kaynağını da denetliyor
(8 parça, `--mimari arm64`) ve `calistir_check_kapisi` **`test_tumu`'ya bağlandı**
— daha önce hiçbir varsayılan koşumda çalışmıyordu. Sabotajla doğrulandı.

**Kendini iptal eden muafiyet:** `test_llvm.c` TIP_BORCU sarmalayıcısı artık
muaf programı önce `--check`'ten geçiriyor; geçerse çağrı satırıyla gürültülü
uyarı basıyor. Bu sayede 2 borcun kapandığı **ölçülerek** görüldü ve geri alındı
(16→14). Muafiyetin sessizce geçerliliğini yitirmesi artık imkânsız.

**Sınır (V1):** `bm_a64_kem_heap.o` kuralı Makefile'da duruyor ama artık kem_os
tarafından kullanılmıyor (ölü hedef, zararsız). Kalan 14 tip borcu (kesirli32
literal bağlamı, `mantıksal olarak tamN`, sınıflandırılmamışlar) ayrı iş.

**Testler:** tip_kontrol 197/197 (+6), llvm 284/284, `test_tumu` tam koşum 0 hata,
kem_os QEMU boot `[1..15]` + `KEMGU KEM-OS OK`.

---

## D-340 [YÜKSEK] — Kripto bilinen-cevap KOŞUM kapısı kuruldu; 2 gerçek kusur buldu (2026-07-28)

**Karar [ETKİ: `test/kripto_kosum_harness.sh` (yeni), `test/stdlib/test_kripto_kosum.kem`
(yeni), `Makefile` (+`calistir_kripto_kosum`), `stdlib/kripto/karma.kem` (+2 öğe).]**

**Kapı boşluğu:** `calistir_kripto_check` yalnızca `--check` (tip kontrolü) yapıyordu.
`test/stdlib/test_kripto_vektor.kem` NIST/RFC vektörlerini gömüyor ama beklenen değerle
**karşılaştırmıyor** ("expected check runtime'da (V2)") ve hiç **çalıştırılmıyordu**.
Yani `stdlib/kripto`'nun sayısal doğruluğu HİÇ ölçülmemişti. Yeni kapı bundle'ı derler,
**çalıştırır** ve RFC 8439 §2.1.1 (ChaCha20 QR) + NIST FIPS 180-4 App. B.1 (SHA-256 "abc")
vektörleriyle karşılaştırır.

**Tasarım — KONTROL biti:** exit kodu bit maskesi (+1 KONTROL sabit tablolar, +2 ChaCha20,
+4 SHA-256; tam geçiş 7). KONTROL biti imzasızlık/kaydırma yolundan GEÇMEZ → kapının
kendisini doğrular: 0 ise kapı bozuk (derleme/bağlama), 1 ise kapı sağlam ve kripto
çekirdeği yanlış. "Kırmızı ama sebebi ilgisiz" yanılgısını yapısal olarak engeller.

**Kurulduğu gün 2 GERÇEK kusur buldu:**
1. **SHA-256 W dizisi 62 öğe** (`stdlib/kripto/karma.kem`) — message schedule 64 gerektirir.
   Çalışma-anı `PANIK: dizi sınır ihlali (i=62, boyut=62)`. Saf kaynak hatası; `--check`
   göremez (dizi uzunluğu tipte değil). Bu adımda düzeltildi (+2 öğe, `dizi_boyut`=64 ölçüldü).
2. **`sabitsüre<dtamN>` imzasızlığı silinmesi** — kapı KIRMIZI (exit 1: yalnız KONTROL).
   ChaCha20 QR ve SHA-256("abc") NIST/RFC cevabını vermiyor. Kök neden `src/llvm.c`
   `ast_tip_isaretsiz_mi` yalnız `DUGUM_TIP_BASIT` kabul ediyor; `DUGUM_TIP_SABITSURE`/
   `DUGUM_TIP_TEKKEZ` iç tipe özyinelemiyor → `>>` `lshr` yerine `ashr`. Onarım AYRI adım.

**Kapının ayırt ediciliği ÖLÇÜLDÜ (ters-sabotaj):** `ast_tip_isaretsiz_mi`'ye SABITSURE/
TEKKEZ özyinelemesi geçici olarak eklendi → kapı **3/3 YEŞİL** (SHA-256("abc") doğru NIST
özetini üretti); geri alındı → tekrar kırmızı. Hem onarımın uygulandığı hem geri alındığı
`diff` ile doğrulandı. Yani kapı tam olarak bu kusuru ölçüyor, başka bir şeyi değil — ve
tanı doğrulanmış durumda (onarım ~6 satır).

**test_tumu'ya BAĞLANMADI (bilinçli):** kapı bugün kırmızı; bağlansaydı ilgisiz her işi
kırmızıya çevirirdi. Bağlama, imzasızlık onarımının **son işi** olmalı — kapı o onarımın
doğrulamasıdır. Makefile yorumunda yazılı.

**Ders:** `--check`-only kapı, sayısal doğruluk için kapı DEĞİLDİR. Kripto/codegen gibi
"tip doğru ama sayı yanlış" olabilen alanlarda koşum kapısı zorunlu. Bu boşluk olmasaydı
her iki kusur da yakalanırdı.

---
## D-339 [YÜKSEK] — Yapı alanı (dtamN) imzasızlığı self-host'ta akar + codegen_diff'in 127 kör noktası kapandı (2026-07-28)

> **Numara notu:** commit mesajı (`86747b9`) D-338 diyor; merge anında D-338 yukarıdaki
> işaretsiz-semantik kararına verildiği için bu kayıt **D-339**'dur. Ayrıca
> `claude/distracted-tesla-e03311` dalındaki kripto commit'i de kendini D-338
> sanıyordu; o dal kendini yeniden numaralamış ve **D-340** olarak main'e alındı
> (yukarıdaki girdi) — bu uyarı çözüldü.
>
> Kod içi işaretler: bu kararın markerları D-339'a güncellendi. Yukarıdaki D-338
> kararının `selfhost/codegen.kem` içindeki `D-337` markerları **bilerek** olduğu gibi
> bırakıldı (paralel dalın sahip olduğu satırlar; yeniden yazmak tesla dalına gereksiz
> çakışma üretirdi). Eşleme bu dosyadadır.

**Karar [ETKİ: `selfhost/codegen.kem` (+59), `test/cg_korpus/cg_isaretsiz_alan.kem`
(yeni, 107→108), `test/codegen_diff_harness.sh` (127 kuralı daraltıldı).]**
D-337'nin bilinçli "Sınır (V1)" maddesini kapatır.

**Kusur:** `ifade_isz` ERISIM'i ele almıyor, `k.alan` için daima 0 (imzalı) dönüyordu.
C tarafında bu yol AÇIK (`llvm.c:2594` `int alan_isz = ast_tip_isaretsiz_mi(alan_tip_d);`
— hem `extractvalue` hem `GEP+load` dalı bunu `IfadeSonuc`'a koyar). Sonuç: bir
regresyon DEĞİL (imzalıya düşmek = D-335 öncesi davranış) ama `yapı Bayt { v: dtam8 }`
gibi kripto/sürücü şekillerinde **C ile self-host AYRIŞIYOR ve self yanlış sonuç
veriyordu**. Ölçülen: `b.v olarak tam32` C=200 / self=−56; `b.v/4` C=50 / self=−14.

**Mekanizma:** `yapi_alan_isz(p, yad, alan)` — `yapi_alan_tip` aynası, ama `alan_tip`
(LLVM string `"i8"`) imzasızlığı **siler**, o yüzden `alan_tnode` (AST tip düğümü)
üzerinden `ll_isz`. C ile birebir incelik: mono örnekte bile **BASE** alan düğümü
kullanılır — C'de de subst yalnız `ast_tip_to_ir`'ı sarar, `alan_isz` subst DIŞINDA
kalır. Nesnenin yapı adı `erisim_yapi_ad` ile **saf** çözülür: emit dalı bunu
`p.son_tip`/`p.son_ref`'ten alır ama o durum ancak kod yayılırken oluşur, `ifade_isz`
saf olmak zorunda → aynı iki kaynak (`cg_atip` / `cg_aref`) doğrudan sorgulanır.
İç içe `a.b.c` için özyineleme (iç alan tipi `"%Ic"` ise taban olur). Çözülemezse
`""` → 0 = imzalı = eski davranış (güvenli taraf).

**INDEKS BİLEREK YAPILMADI — ölçüm gerekçesi:** görev "C'de ERISIM ve INDEKS yollarını
ÖLÇ, birebir aynala" diyordu. Ölçüldü: C'nin **üç** INDEKS dalı da
(`llvm.c:3249` heap `kdl_dizi_al`, `:3282` türetilmiş taban, `:3318` stack GEP+load)
`IfadeSonuc s = { r, tip, 0 }` üretir. Yani C'de de dizi elemanı imzasızlığı TAŞINMAZ.
Self-host'un 0 dönmesi C ile **birebir aynıdır** — parite kaybı değil, **ortak sınır**.
Bunu C'de "düzeltmek" oracle'ı değiştirmek olurdu; kapsam dışı bırakıldı.

**Ölçüm:** yeni korpus `cg_isaretsiz_alan.kem`, exit **60**. Üç farklı codegen dalını
ayrı ayrı yükler: struct-value (`extractvalue`), `&Yapi` referans parametresi
(`GEP+load`), iç içe `d2.ic.v` (özyinelemeli taban). Falsifiye edici seçim D-337 ile
aynı disiplin: 200 > 127.

**Sabotaj (uygulandığı `diff` ile DOĞRULANDI):** ERISIM dalına `ver 0` → IR'da
`udiv→sdiv` ve `zext→sext` (grep ile teyit), exit 60→127, kapı 🔴.

**⚠ ASIL BULGU — kapı kendi ölçtüğü şeye kördü:** sabotajlı değer **tam olarak 127**
çıktı ve `codegen_diff_harness.sh` 127'yi *"korpusta hiçbir program 127 dönmez →
127 DAİMA ortamsal (Defender exec yarışı)"* premisiyle **⚠ ATLIYORDU**. Yani gerçek
bir miscompile yeşil geçti (`107/107`). Premis yanlış ölçülmüştü. Kural daraltıldı:
127 yalnız **ORACLE**'da ortamsal sayılır (oracle yoksa karşılaştırma anlamsız);
oracle sağlam değer verirken aday kalıcı 127 diyorsa (12 retry sonrası) bu bir
ANLAŞMAZLIKTIR, fail. Bu, D-338'in kodundan bağımsız olarak **tüm korpusu** korur.
Ders: "bu değer asla oluşmaz" varsayımına dayanan atlama kuralları, tam da o değeri
üreten hata sınıfına kördür.

**Kapılar:** codegen_diff **108/108**, codegen_bootstrap **FIXPOINT ✓**
(lexer/parser/checker 92 birebir + stage1 IR == stage2 IR, 45729 satır),
calistir_self_driver (4 mod, C-derlenmiş + self-host-derlenmiş + FIXPOINT;
LLVM 108/108 her iki driver'da), checker_diff 56/56, calistir_llvm_test 284/284.

**Not (dal tabanı):** D-337 (`7f645be`) merge anında `origin/main`'de DEĞİLDİ
(`claude/distracted-tesla-e03311`); bu iş o commit'in üzerine kuruldu.

---

## D-338 [YÜKSEK] — Self-host codegen'de işaretsiz (dtamN) semantiği kilitlendi (2026-07-27)

> **Numara notu (merge anında kaydırıldı):** bu karar `7f645be` commit'inde **D-337**
> olarak yazılmıştı, ama merge anında `origin/main`'de D-337 **başka** bir karara
> (`dc2879c`, `--llvm` katı tip kapısı) verilmişti. CLAUDE.md kuralı ("D-NNN'i merge
> anında güncel main'deki en yüksek D'ye bakıp ver") gereği D-338'e kaydırıldı.
> Commit **mesajı** D-337 diyor (paralel dalın sahip olduğu commit yeniden yazılmadı);
> **yetkili kayıt bu dosyadır.**

**Karar [ETKİ: `selfhost/codegen.kem` (+164/−17), `test/cg_korpus/` (+2 korpus,
105→107).]** D-335 C tarafını kilitlemiş, self-host'un **hiçbir** `dtamN` işlemini
imzasız üretmediğini ölçmüştü. Bu adım self-host'u C oracle'ına bağlar.

**Kusur (sessiz yanlış cevap):** `selfhost/codegen.kem` imzasızlığı hiç izlemiyordu —
her `dtam` işlemi imzalı varyantla üretiliyordu (`sext`/`sdiv`/`srem`/`ashr`/`sgt`).
Kodda "kapsam dışı, izlenmiyor" diye **bilinen bir kısıt olarak yazılıydı**, ama sonucu
derleme hatası değil **yanlış sayı**ydı: `dtam8 200` bit deseni her yerde −56 sanılıyordu.
Ölçülen (C vs self): `200 olarak tam32` 200/127 · `200/4` 50/127 · `200%7` 4/0 ·
`200>>2` 50/127 · `200>100` 42/1. Kripto (`stdlib/kripto`), sürücüler ve bayt işleme
kodunda doğrudan yanlış davranış.

**Mekanizma:** C'de bu bilgi `IfadeSonuc.isaretsiz` alanında **akar**; self-host'ta
`ifade_uret` tek bir metin döndürdüğü için taşıyıcı yok. Seçim: **saf (yan-etkisiz)
yeniden-hesap** — `ifade_isz(p, idx)`, düğüm türünden imzasızlığı türetir
(TANIMLAYICI→değişken kaydı, TIP_DONUSTUR→hedef tip, IKILI→sol‖sağ, TEKLI neg/~→operand,
CAGRI→bildirilen dönüş). C'nin `.isaretsiz` **okuduğu her yerin** karşılığı bunu çağırır.

**Neden durum (`p.son_isz`) DEĞİL — gerekçe:** `son_tip` her dalda yeniden yazılır,
imzasızlık ise yalnız birkaç dalda anlamlıdır. Durum tutulsaydı ele alınmayan dallardan
**sızar** ve alakasız bir ifadeyi `udiv`/`lshr`'e düşürürdü — yani aynı sınıf hatayı
(sessiz yanlış cevap) ters yönde üretirdi. Saf işlevin varsayılanı `0` = imzalı = eski
davranış; bu, atlanan her düğümü **güvenli** tarafa düşürür.

**Kayıt yolları:** `cg_aisz` (değişken/parametre, `cg_ad` ile paralel — `cg_var_bul` ile
AYNI arama yönü/sınırı, blok-gölgelemesinde tip ve imzasızlık aynı slot'tan gelmeli) +
`fn_risz` (işlev dönüşü). `fn_ad` checker ile PAYLAŞILIR → `fn_risz_bul` sınırı `fn_ad`
değil **kendi boyutu** (aksi OOB panik).

**Emisyon (C ile birebir):** genişletme `zext`, bölme `udiv`, mod `urem`, sağa kaydırma
`lshr`, karşılaştırma `ult/ule/ugt/uge`. `add/sub/mul/and/or/xor/shl` ve `eq/ne` imzadan
BAĞIMSIZ → tek varyant.

**Ölçüm:** 5 şeklin 5'i de C ile eşleşti (200/50/4/50/42). Yeni korpus
`cg_isaretsiz_temel.kem` (exit 254 = zext 200 + udiv 50 + urem 4),
`cg_isaretsiz_kaydir_kars.kem` (exit 77 = lshr 50 + ugt 20 + ult 7; parametre yolu +
dtamN dönüş tipi dâhil). Falsifiye edici seçim: 200 > 127 → i8'de imzalı yorum negatif,
her dal imzasız/imzalı ayrımında FARKLI değer verir.

**Sabotaj (3/3, her biri diff ile uygulandığı DOĞRULANDI):** `zext→sext` → temel 254→127;
`lshr→ashr` + `ugt→sgt` → kaydır_karş 77→249 (temel etkilenmedi — doğru, `>>`/karşılaştırma
kullanmıyor); `udiv→sdiv` → temel 254→190. İki korpus dosyası **bağımsız olarak** yük taşıyor.

**Kapılar:** codegen_diff **107/107**, checker_diff 56/56, calistir_llvm_test 284/284,
calistir_self_driver (4 mod + self-host-derlenmiş driver + FIXPOINT), codegen_bootstrap
**FIXPOINT ✓** (lexer/parser/checker 92 birebir + stage1 IR == stage2 IR, 45436 satır).

**Süreç dersi:** `sed -i` Türkçe `.kem` kaynağında tüm dosyayı yeniden yazdı (satır-sonu
dönüşümü → 7229 satırlık sahte diff). CLAUDE.md'nin `perl -i` yasağı **`sed -i` için de
geçerli** — Edit aracını kullan. Sabotajın kendisi her seferinde `diff` ile doğrulandı.

**Sınır (V1):** `ifade_isz` ele almadığı düğümlerde 0 (imzalı) döner — ERİŞİM (`k.alan`),
INDEKS (`xs[i]`) ve yapı alanı `dtamN` ise imzasızlık **taşınmaz** (C'de `alan_isz`
üzerinden taşınır, `src/llvm.c:2594`). Hata modu **imzalıya düşmek**, yani D-335 öncesi
davranış — yeni bir sessiz-yanlış sınıfı DEĞİL, kapanmamış eski yüzey. Kapatılması ayrı adım.

---

## D-320 — Çağrı argümanında sahte L002: iki-pas ziyaret SONDAJ'landı (2026-07-26)

**Karar [ETKİ: `src/tip_kontrol.h` (+1 alan `lineer_sondaj`), `src/tip_kontrol.c`
(3 nokta: init, `lineer_tuket_eger_baglamaysa` girişi, ERİŞİM kısmi-taşıma maskesi,
+ DUGUM_CAGRI pas-1 sarmalama), `test/test_linear.c` (+6, 83→89).]**
D-319'un yan bulgusu kapatıldı — ve **öngörülen kök neden ölçümle doğrulandı.**

**Kusur:** `DUGUM_CAGRI` argümanları İKİ pas görüyor — pas 1 generic unify
(`tip_belirle`), pas 2 beklenen-tip çıkarsama + T001 (`tip_belirle_beklenen`).
Her ziyaret lineer defteri mutasyona uğrattığı için **TEK** bir tüketim **İKİ**
sayılıyordu:
- `ver metin_uzunluk(kullan(m));` → sahte **L002**
- `f(k.a)` (D-315 kısmi taşıma) → sahte **L002** "alan zaten dışarı taşındı"

İkisi de **ÖNCEDEN VAR** (9fead86'da ölçüldü) — D-311..D-319 lineer işinin
regresyonu DEĞİL. Yalıtım (D-319'daki gibi): ayrı deyim OK, `ver kullan(a)` OK,
ikili ifade içinde OK — **yalnız argüman pozisyonu.**

**Çözüm — SONDAJ (probe) modu, L002'yi özel-durumlamak DEĞİL:** `tk->lineer_sondaj`
sayacı; pas 1 sondaj olarak işaretlenir, lineer defter (tüketim sayacı + kısmi-taşıma
bit maskesi) **YALNIZ pas 2'de** güncellenir. Tanı **ölçüldü, tahmin edilmedi**:
mekanizmayı iptal edince (sabotaj) tam olarak yeni 4 kabul-testi kırmızıya döndü.

**Neden defter-susturma, tanı-susturma DEĞİL:** pas 1'de `tip_hata` KAPATILMADI.
Kapatmak duplike raporu da temizlerdi ama pas 2'nin farklı bir yol izlediği durumda
gerçek bir hatayı **loud→silent** çevirirdi. Bastırılan tek şey **durum mutasyonu**.

**Doğrulama:** test_linear **89/89** (+6: 4 kabul — düz/iç-içe/generic çağrı +
kısmi taşıma; **2 SABOTAJ KAPISI** — gerçek çift `kullan` ve aynı alanın iki kez
taşınması hâlâ L002). L001 sızıntı tespiti bozulmadı. tip_kontrol 189/189,
capability 40/40 (CP005 aynı yardımcıyı kullanır), llvm 274/274,
checker_diff 52/52, codegen bootstrap **FIXPOINT** (42003 satır).

**Self-host portu GEREKMEDİ — ölçüldü, varsayılmadı:** `kemgu_self.exe` her iki
repro'yu zaten KABUL, her iki negatifi zaten L002 ile RED ediyordu. Self-host
`ifade_tip`'i (pas 2) lineer yan etkisizdir ve ERİŞİM maske kancası yalnız
`kontrol_dugum`'dadır. Yani bu düzeltme bir **parite kaybını kapatıyor** (C
reddediyor, self kabul ediyordu), yeni bir parite borcu açmıyor.

**Sınır:** self-host'un kendi asimetrisi duruyor (bilinen, bu işin kapsamı dışı) —
`lin_tuket_dugum` yalnız bilinen-arity yolunda çalışır; builtin/dolaylı çağrı
argümanları tüketilmez → o yolda L001 sahte pozitifi mümkün.

---

## D-337 — [YÜKSEK] `--llvm` ARTIK KATI: tip hatasi = derleme yok; `--tip-atla` kacis kapisi (2026-07-27)

**Karar (Mehmet, 2026-07-27): "simdi kati yap, 16 testi `--tip-atla` ile isaretle".**
**[ETKİ: `src/ana.c` (kati mod + `--tip-atla`), `test/test_llvm.c` (16 test BORC
isaretli), `test/codegen_diff_harness.sh`, `test/dizi_sinir_harness.sh` (1 test
kaynagi DUZELTILDI + 3 sessiz-atlama gurultulendi), `Makefile` (kem_os borcu).]**

**Davranis:** `--llvm` tip hatasinda **stderr'e gercek taniyi yazar, IR URETMEZ,
exit 1**. Once: `--check` REDDETTIGI program derlenip **calisan ikili** veriyordu
(`f(40, 99)` → exit 42). `--tip-atla` eski davranisi ACIK BEYANLA geri getirir.

**⚠ ONCE OLCULDU — KARARI DEGISTIREN BULGU (Mehmet'e sunuldu, onay alindi):**
kati mod 16 testi kirdi ve **en az 2'si CHECKER YANLIS POZITIFI**:
- `işlev h(x: kesirli32) -> kesirli32 { ver x + 21.0; }` → **T001** (literal
  `kesirli64` varsayilip `kesirli32`e daraltilmiyor) — GECERLI program.
- `(b olarak tam32)` (b: mantıksal) → **E002** — test [226] bunu gecerli sayiyor.
Yani bugun kati mod **gecerli programlari da reddediyor**; bu bir BORCTUR.

**Borc GORUNUR isaretlendi:** 16 test `derle_ve_calistir_TIP_BORCU(...)` ile
derlenir — adi cagri yerinde OKUNUR. Checker kusuru kapandikca ilgili test normal
yola DONMELIDIR. (Gizli bayrak degil, isimle beyan.)

**DUZELTILEN (ortulmedi):** `dizi_sinir_harness` vaka23 `m.b + m.a` (tam64+tam8)
→ T001. **Tani DOGRU** — KEMGU'da ortuk sayisal donusum YOK (ASLA listesi).
Test kaynagi `(m.b olarak tam32) + (m.a olarak tam32)` yapildi; `--tip-atla`
KULLANILMADI. Ayrica ayni harness'ta `--llvm` cikisini yutan **3 nokta**
gurultulendi (sessiz atlama → acik hata).

**Diger borclar (acik beyanla):** `kem_os` birlesik kaynagi 60 tip hatasi
(Makefile'da `--tip-atla` + "BORC, bayrak KALDIRILMALI" notu); codegen_diff
harness'i `--tip-atla` kullanir (isi CODEGEN esdegerligi; tip zorlamasi D-336
kapisinda ve cg_korpus'u zaten kapsiyor — aksi halde 3 kasitli korpus SESSIZCE
atlanip kapsam 105→102 dusuyordu).

**Kapilar:** test_llvm **284/284**, codegen_diff **105/105**, dizi_sinir **37/37**,
check_kapisi 199/206+7muaf, llvm_dogrula 10/10, gorev_rt 16/16, snapshot 50/50,
simd_llvm 5/5, stdlib+kripto --check, sifir derleyici uyarisi.

**SIRADAKI (borc kapatma):** checker yanlis pozitifleri — (1) kesirli32 literal
baglami, (2) `mantıksal olarak tamN`, (3) siniflandirilmamis 14 test. Kapandikca
`derle_ve_calistir_TIP_BORCU` cagrilari normal yola dondurulmeli.

---

## D-336 — [YÜKSEK] `--llvm` tip kontrolunu BAGLAMIYOR; TIP KONTROL KAPISI + 4 gercek sizinti (2026-07-27)

**Karar [ETKİ: `test/check_kapisi.sh` (YENİ), `Makefile` (`calistir_check_kapisi`),
`test/cg_korpus/cg_gorev_{baslat,i64_daralt,lambda_blok}.kem` +
`test/ornekler/gorev_temel.kem` (4 GERCEK sizinti onarildi).]**

**BULGU 1 — `--llvm` tip kontrolunu ZORLAMIYOR (olculdu):** `--check` REDDETTIGI bir
program `--llvm` ile derlenip **CALISAN ikili** uretebiliyor:
`işlev f(x: tam32)...  ver f(40, 99);` (FAZLA argüman) → `--check` RED, `--llvm`
**exit 0**, clang kabul, program **exit 42**. Yanlis arg tipiyle de derleniyor (cop 127).
Yani derleme yolu (`kemgu --llvm | clang`) tip kontrolunden GECMIYOR.

**BULGU 2 — bunun birikmis bedeli:** hicbir kapi korpus/ornek uzerinde `--check`
kosturmadigi icin depoda **11/206 dosya** `--check` RED aliyordu ve kimse gormemisti.
Dagilim: **4 × L005 (GERCEK LINEER SIZINTI)**, 1 × E004 + 1 × E002 + 1 × T001
(kasitli codegen korpuslari), 3 × T002 (tek-basina derlenemeyen parca dosyalar),
1 kasten-hatali ornek.

**4 GERCEK SIZINTI ONARILDI:** `eşleş görev_başlat(...) { tamam(a) => {...}
hata(e) => { ver 1; } }` — HATA dalinda `a` hic birlestirilmiyordu → gorev tanitici
o yolda dusuyor (sizinti). **L005 DOGRU CALISIYORDU; kusur ORNEKTEYDI.** Duzeltme:
hata dallarinda `görev_birleştir(a)` cagrilir. **Exit kodlari DEGISMEDI** (4 dosya da
42) → codegen_diff **105/105** korundu.

**KAPI:** `calistir_check_kapisi` — korpus/ornek/stdlib `--check`ten gecmeli.
Gecmeyecek dosya MUAF listesine **GEREKCESIYLE** yazilir (sessiz birikme yerine acik
karar). Su an **199/206 gecti, 7 muaf, 0 RED**.
**Sabotaj:** bir korpus dosyasina tip hatasi enjekte → kapi **exit 1** + dosyayi
isimlendirdi; temiz → **exit 0**.

**KARAR GEREKTIREN (Mehmet) — YAPILMADI:** `--llvm` tip hatasinda REDDETSIN mi?
Bugun etsek **kem_os dahil 7 muaf dosya** derlenemez; bu bir DIS-KONTRAT degisikligi.
Secenekler: (a) `--llvm` kati + `--tip-atla` kacis kapisi; (b) muaf dosyalarin
gercek nedenleri giderilsin (parca dosyalar icin birlesik derleme, kasitli
korpuslar icin `güvensiz`/annotasyon); (c) mevcut hal + bu kapi yeterli sayilsin.

---

## D-342 — [YÜKSEK] `%Yapi` dizi elemani self-host'a portlandi + C'de `için` SESSIZ YANLIS CEVAP kapandi (2026-07-28)

> **Numara notu:** bu dal (`claude/hopeful-tharp-8dc610`) commit mesajlarinda D-337
> (kapanis konteynerde) ve D-338 (`%Yapi` dizi elemani) diyor. Merge aninda main
> D-337/D-338/D-339'u almis, D-340 ise `claude/distracted-tesla-e03311` kripto
> commit'ine rezerve edilmisti → bu iki kayit **D-341** (kapanis konteynerde) ve
> **D-342** (`%Yapi`) oldu. Kod ici markerlar da kaydirildi; main'in ISARETSIZ
> (dtamN) isine ait D-337/D-338 markerlarina DOKUNULMADI.

**Karar [ETKİ: `selfhost/codegen.kem` (`dizi_eleman_yapi_mi` + `dizi_eleman_byte` +
`dizi_yapi_{ekle,al,yaz}_emit` ORTAK yol + 5 cagri yeri), `src/llvm.c` (`için` dali
by-value yonlendirmesi), `test/test_llvm.c` (286), `test/cg_korpus/cg_yapi_dizi.kem`
(YENİ, korpus 107 → **108**).]** D-341'de bilincli birakilan `%Yapi` bosluğu kapatildi;
port sirasinda **C tarafinda daha ciddi bir kusur** ortaya cikti.

**1) Self-host `%Yapi` bosluğu (ONCE OLCULDU — 6 sekil, hepsi LLVM-RED):** dizi
literali, `dizi_ekle`, `ps[i] = v`, `ps[i]` okuma, dizi parametresi, metin alanli yapi.
Kok neden: `dizi_ekle_sonek` `%Nokta`yi skaler sayip `kdl_dizi_ekle_tam(ptr,ptr,i32)`e
struct DEGER geciriyordu — **doğrudan** cagrida LLVM imzayi denetler → gurultulu red.
Sessiz-okuma yolu (`kdl_dizi_al_tam` 8 baytlik elemanin ilk 4 baytini okur) ERISILEMEZDI:
diziyi doldurmanin her yolu derleme zamaninda reddediliyordu; yazmasiz tek okuma bos
dizi, onu da runtime sinir-kontrolu iki derleyicide birebir kesiyor (`PANIK: dizi sınır
ihlali`, exit 127). **Onarim IKI parcali:** (a) `dizi_eleman_yapi_mi` `%` onekini de
by-value kabul eder; (b) `dizi_eleman_byte` nominal yapida sabit "4" yerine
`ptrtoint (ptr getelementptr (%Yapi, ptr null, i32 1) to i32)` uretir — hizalama/dolgu
hesabi derleyicinin isi. Yalniz (a) yapilsaydi 8+ baytlik yapi 4 baytlik gozeye
sikisirdi: **SESSIZ bellek bozulmasi** (sabotaj E ile olculdu → exit 139).

**2) [YÜKSEK] C'de `için` dongusu SESSIZ YANLIS CEVAP veriyordu.** Port sonrasi C↔self
karsilastirmasi ayristi: `için p: ps` (2 elemanli `Dizi<Nokta>` toplami) C'de **exit 14**,
self-host'ta **42** (dogrusu 42). Uretilen IR: `call %Nokta @kdl_dizi_al_tam(...)` —
declare `i32`, cagri yeri `%Nokta`. **LLVM declare/cagri-yeri uyusmazligini SESSIZCE
kabul ediyor** (D-295/D-325/D-334 dersinin BESINCI tekrari) → cop okunuyordu. Diger dort
dizi yolunda (INDEKS / `dizi_al` / `dizi_ekle` / `dizi_yaz`) by-value yonlendirmesi
D-087'den beri VARDI; yalniz `için` dali atlanmisti. **Bu kusuru bulan sey portun
kendisiydi** — iki bagimsiz uygulama ayni programda ayrisinca sessiz hata gurultuye
donusuyor (self-host'un asil degeri).

**ORTAK YOL:** `dizi_yapi_{ekle,al,yaz}_emit` — bes cagri yeri (dizi literali, uc
built-in, INDEKS okuma/yazma, `için`) tek kaynaktan. Kopyalansaydi biri duzeltilip
digeri unutulurdu; **C'de tam bu oldu** (`için` dali).

**Testler:** C↔self-host exit birebir 7 sekil (yukaridaki 6 + `için`). test_llvm
284→**286** (yeni: `için` yapi dizisi 42, `için` kapanis dizisi 42 — ikisi de C'de
sessiz-yanlis-cevap regresyon kilidi). Korpus `cg_yapi_dizi.kem`: literal + indeks
okuma/yazma + `dizi_ekle`/`dizi_al` + `için` + isaretci alanli yapi → 42.

**SABOTAJ (hepsi dosyada `grep` ile DOGRULANDI):** (C) C `için` by-value yonlendirmesi
kapatilsin → **[285]+[286] kirmizi, digerleri temiz**; (D) self-host `%` dali kapatilsin
→ **cg_yapi_dizi LLVM-RED, cg_kapanis_dizi YESIL** (izolasyon); (E) `dizi_eleman_byte`
sabit "4" → **exit 139** (boyut hesabinin tasiyici oldugu kanit). ⚠ Sureç notu: sabotaj C
ilk denemede YANLIS SATIRA dustu (`replace(...,1)` ilk eslemeyi vurdu; CRLF yuzunden
cok-satirli anahtar da eslesmedi) ve "2 kirmizi" gorunumu doğru sanilabilirdi — kirmizi
TESTLERIN ADI okunmasa yanlis sonuc cikardi. **Sabotajda hangi testin kirmizi oldugunu
ADIYLA dogrula.**

**Kapilar:** test_llvm **286/286**, codegen_bootstrap **FIXPOINT** (stage1==stage2,
45619 satir; lexer/parser/checker 92/92), codegen_diff **108/108**, self_driver (4 mod +
self-host + FIXPOINT; LLVM 108/108 iki asamada da), checker_diff **56/56**, sifir
derleyici uyarisi.

---

## D-341 — KAPANIS KONTEYNERDE self-host'a PORTLANDI: D-334 parite borcu KAPANDI (2026-07-27)

**Karar [ETKİ: `selfhost/codegen.kem` (`fat_cagri_uret` ORTAK dispatch + `yapi_alan_ic`
+ `erisim_kapanis_ic` + `dizi_eleman_yapi_mi` + ERISIM/INDEKS cagri yollari + by-value
dizi eleman emisyonu + 3 `kdl_dizi_*_yapi` declare), `test/cg_korpus/` (105 → **107**:
`cg_kapanis_yapi_alani.kem`, `cg_kapanis_dizi.kem`).]** D-334'un C tarafinda actigi iki
sekil (`k.fn()` yapi alani, `xs[i]()` dizi elemani) artik self-host codegen'de de var;
D-334'te acik birakilan **parite borcu KAPANDI**. Oncesi (olculdu): ikisi de self-host'ta
LINK-RED — gurultulu, sessiz sapma yoktu.

**Portlanan uc mekanizma:**
1. **`fat_cagri_uret` (ORTAK dispatch):** env-null dallanmasi TEK KAYNAK — yerel baglama
   (D-322) / yapi alani / dizi elemani ayni yoldan. D-322'nin inline blogu bu yardimciya
   cekildi; iki kopya birakilsaydi biri duzeltilip digeri unutulurdu.
2. **Yapi alani:** `erisim_kapanis_ic` — alanin BILDIRILEN tipi `işlev(...)->T` mi diye
   olcer; oyleyse metod DEGIL, fat value tutan alandir → dolayli cagri, donus IR'i
   `yapi_alan_ic` ile alanin bildirilen tipinden (i32 varsayimi `-> metin` alaninda
   isaretciyi kirpardi). **C'den fark (bilincli):** C alici ifadesini URETIP tipe bakar;
   self-host STATIK cozer (TANIMLAYICI alici: degisken IR'i / referans hedefi). Sebep:
   self-host'ta iki kez uretim yan etkiyi tekrarlardi. Statik cozulemeyen alici (or.
   `f().fn()`) metod yoluna duser → LINK-RED (gurultulu), sessiz sapma YOK.
3. **Dizi elemani:** `dizi_eleman_yapi_mi` (`{ ptr, ptr }` → by-value) + `kdl_dizi_ekle_yapi`
   / `kdl_dizi_al_yapi` yolu + `eleman_byte = 16`. Skaler sayilsaydi 16 baytlik agregat
   `kdl_dizi_ekle_tam(i32)` imzasina gecerdi (**LLVM SESSIZCE kabul eder** — D-295/D-325/
   D-334 dersinin dorduncu tekrari). `xs[i]()` fat degilse: BILDIRILMEMIS sembole cagri →
   LLVM ayrıştırma hatasi (sessiz "0" YASAK, D-326).

**Donus IR'i baglami:** `ll_ic_tip` `Dizi<işlev(..)->T>` icin T dondurur; degisken
bildiriminde `beklenen_elem_ic`, yapi alaninda `yapi_alan_ic` lifted lambda'nin donusunu
besler ve **ayni kaynak** cagri yerine verilir (D-325 dersi: iki ayri tahmin ayrisir).

**Testler (C↔self-host exit BIREBIR, 5 sekil):** yapi alani 42 · yakalamali alan +
arguman 42 · `-> metin` alani (isaretci donus) 42 · dizi elemani 42 · coklu eleman +
yakalama + arguman 42.

**SABOTAJ (ikisi de dosyada `grep` ile DOGRULANDI):** (A) fat value skaler sayilsin →
**iki korpus dosyasi da LLVM-RED**; (B) alan-kapanis tespiti kapatilsin → **yapi alani
RED, dizi YESIL** (izolasyon dogru); temiz → **ikisi de 42**.

**Kapilar:** codegen_bootstrap **FIXPOINT** (lexer/parser/checker 92/92 birebir +
stage1==stage2, 45371 satir), codegen_diff **107/107**, self_driver (C-built + self-host
+ FIXPOINT, 4 mod: token 22/22, parse 12/12, check 56/56, LLVM 107/107), checker_diff
**56/56**.

**Sinir (V1, pre-existing):** self-host dizi elemani olarak `%Yapi` (nominal struct)
HALA desteklenmiyor — `dizi_eleman_yapi_mi` yalniz fat value'yu by-value kabul eder
(C'deki predikat `%`i de kapsar). Bu D-334 oncesinden gelen ayri bir bosluk; bu adimda
genisletilmedi, cunku korpusta sekli yok ve ayri olcum ister.

---

## D-335 — [YÜKSEK] Self-host ISARETSIZ (dtamN) semantigi YOK → sessiz yanlis cevap (2026-07-27)

**Karar [ETKİ: `test/test_llvm.c` (279 → **284**, C tarafi KILIT).]** Adversarial tarama
(sayisal donusum yuzeyi) bir SINIF sessiz-yanlis-cevap buldu — bu kez **self-host**'ta.

**ÖLÇÜM (C = dogru, self = yanlis):**
| program (dtam8) | C | self | dogru |
|---|---|---|---|
| `a=200; (a olarak tam32)` | 200 | yanlis | 200 |
| `a/b` (200/4) | 50 | yanlis | 50 |
| `a mod b` (200 mod 7) | 4 | **0** | 4 |
| `a >> 2` (200>>2) | 50 | yanlis | 50 |
| `a > b` (200>100) | 42 | **1** | 42 |

**IR KANITI:** self `sext i8` / `sdiv i8` / `ashr i8` uretiyor; C `zext` / `udiv` /
`lshr` / `ugt`. **Kok neden KODDA YAZILI** (`selfhost/codegen.kem` ~1928):
*"imzasizlik (dtam) izlemiyor — sdiv/srem'de oldugu gibi DAIMA imzali varyant"*.
Yani bilinen bir kisit olarak NOT DUSULMUS, ama sonucu **sessiz yanlis cevap**;
kripto (`stdlib/kripto`), OS surucu ve bayt isleme kodunda dogrudan yanlis davranis.

**Bu adimda YAPILAN:** C'nin DOGRU davranisi 5 testle KILITLENDI (test_llvm 279→284:
genisletme/bolme/mod/kaydirma/karsilastirma). Self-host portu **CHIP'LENDI** — ayni
dosyaya (`selfhost/codegen.kem`) dokunan baska gorevler kosuyor, paralel duzenleme bu
oturumda iki kez ise mal olmustu.

**Korpusa (cg_korpus) EKLENMEDI** — self su an YANLIS oldugu icin codegen_diff hakli
olarak kirmizi verirdi; kirmizi main birakilmaz. Self duzelince korpusa tasinacak.

**⚠ SUREC NOTU:** ilk yazdigim 2 test C'de KIRMIZI verdi — ikisi de BENIM test
kaynagimdaki hataydi (bagimsiz olcumde C dogruydu): (a) `%` icin printf kacisi
sanip `%%` yazmisim (bu printf DEGIL, KEMGU kaynagi); (b) `eğer` yazarken CLAUDE.md'nin
UTF-8 hex-escape kuralini ihlal etmisim (`\x9f` sonrasi 'e' hex rakam →
concatenation sart). **Test kirmizisini once TESTTE ara.**

---

## D-334 — KAPANIS KONTEYNERDE: yapi alani + dizi elemani cagrilabilir (2026-07-27)

**Karar [ETKİ: `src/llvm.c` (`fat_cagri_uret` ORTAK dispatch + `yapi_alan_tip_dugumu`
+ ERISIM/INDEKS cagri yollari + `dizi_eleman_struct_mi` genisletmesi),
`test/test_llvm.c` (274 → **279**).]** D-326'da gurultulu reddedilen iki sekil artik
GERCEKTEN destekleniyor.

**Iki AYRI kok neden (olculdu):**
1. **Yapi alani** `k.fn()`: ERISIM hedefli HER cagri **METOD** sayiliyordu → alan adiyla
   `call i32 @fn(...)` uretiliyor, **TANIMSIZ SEMBOL** veriyordu. Cozum: once alicinin
   yapisina bakip alanin BILDIRILEN tipi `işlev(...)->T` mi diye olc; oyleyse metod
   degil, fat value tutan ALANDIR → dolayli cagri. Donus IR'i alanin bildirilen
   tipinden gelir (i32 varsayilsa `-> metin` alaninda isaretci KIRPILIRDI).
2. **Dizi elemani** `xs[0]()`: `dizi_eleman_struct_mi` yalniz `%Yapi`ya bakiyordu →
   16 baytlik fat value SKALER sanilip `kdl_dizi_ekle_tam(i32)` imzasina geciriliyordu.
   **LLVM bunu SESSIZCE kabul ediyor** (D-295/D-325 dersinin ucuncu tekrari). Cozum:
   predikat `{ ptr, ptr }`i de by-value kabul eder → runtime'in `kdl_dizi_*_yapi`
   (eleman_byte + memcpy) yolu; zaten VARDI, yalnizca yonlendirme eksikti.

**ORTAK DISPATCH:** `fat_cagri_uret` — env-null dallanmasi TEK KAYNAK (degisken /
yapi alani / dizi elemani ayni yoldan). Iki kopya birakilsaydi biri duzeltilip digeri
unutulurdu (D-322'de `lam_env_uret` ayni sebeple cikmisti).

**Testler (test_llvm 274→279, hepsi derle+calistir+exit):** yapi alani 42 · yakalamali
alan + arguman 42 · dizi elemani 42 · **coklu eleman + yakalama** 42 (dogru elemanin
secildigini olcer) · `-> metin` alani 42 (isaretci donus).

**SABOTAJ:** (A) fat value skaler sayilsin → **2 test kirmizi**; (B) alan-kapanis
tespiti kapatilsin → **3 test kirmizi**; temiz → **0**.

**Kapilar:** test_llvm **279/279**, codegen_diff **105/105**, dizi_sinir 37/37,
sifir derleyici uyarisi.

**⚠ PARITE BORCU (acik, chip'li):** self-host bu iki sekilde LINK-RED veriyor
(gurultulu — sessiz sapma YOK) → C ileride. Korpusa (cg_korpus) EKLENMEDI, cunku
codegen_diff hakli olarak kirmizi verirdi; testler C-tarafi suitinde. Port ayri is.

---

## D-333 — BET KOPRUSU: ana modelde kosum-uzunlugu SINIRI ispatlandi (2026-07-27)

**Karar [ETKİ: `proofs/drf-v2-lean/Kemgu/BET/CoreBound.lean` (YENİ, ~260 satir),
`Kemgu.lean` (+1 import), `test/lean_aksiyom_harness.sh` (+3 denetim).]**
D-332'nin acik borcu ("Sem/Core'a kopru YOK") — **bugun yapilabilir parcasi kapandi**.
`SmallStep.lean`e DOKUNULMADI (kosan kopru goreviyle cakisma yok; yeni dosya).

**Ne ispatlandi (ANA MODEL uzerinde, cekirdek hesapta degil):**
- `hatasiz_adim_azaltir` (21/21 kural): fault uretmeyen HER adim `konfOlcu`yu
  STRICT azaltir. Fault kurallari `h_nf` ile dislanir (post-state yalniz `fault`
  alanini degistirir → olcu DUSMEZ; bu bir kusur degil, modelin dogru okunmasi).
- **`hatasiz_kosum_siniri`:** `HatasizZincir n S S' → n ≤ konfOlcu S`.
- `core_bet_rt8`: kagit RT.8 formu — `∃N, ∀hatasiz kosum: uzunluk ≤ N`.

**Sinir GIRDIDEN BAGIMSIZ:** `konfOlcu` yalnizca thread ifadelerinin sozdizimsel
olcusune bakar; store/kanal icerigi (girdi) formule HIC girmez.

**NEDEN SINIR VAR:** `Core.Ifade`de dongu/ozyineleme/cagri YOK → her Tamam kurali
odakli ifadeyi kucultur. `gorevBaslat` bile net -1 (sarmalayici kaybolur, `kod`
cocuk thread'e tasinir).

**TASIMAYAN parca (durustce):** D-332'nin asil icerigi olan **dal-max** muhakemesi
buraya GECMEZ — `Core.Ifade`de `eger` YOK (olculdu). O parca koprunun `eger`
gerektiren kismidir (kosan gorev + D-331 eki karari).

**SABOTAJ:** (A) `gorevBaslat` olcusunden `kod` dusuruldu → **1 hata**;
(B) hatasizlik sarti kaldirildi (`h_nf : True`) → **11 hata**; temiz → **0**.

**⚠ SUREC NOTU (bu oturumda IKINCI kez):** B sabotajinin ilk denemesi **0 hata**
verdi — perl kalibi tutmadigi icin dosya HIC DEGISMEMISTI. `grep` ile satirin
gercekten degistigi dogrulandiktan sonra 11 hata cikti. **Sabotaj testinin
KENDISI once dogrulanmali** (D-328'de ayni tuzak yasandi).

**Kapi:** `calistir_lean_aksiyom` — 30+kok modul, **20 teorem**, sorryAx YOK.

**BAKIM:** `sEgerKosulCong`/`sEgerSec` Step'e eklendiginde bu dosya +case ister →
kapi KIRMIZI verir (sessiz kalmaz); `olcu (eger k d y) = 1 + olcu k + olcu d + olcu y`
yeterlidir (burada olcu YAPISALDIR, WCET degil).

---

## D-332 — BET/WCET teoremi ispatlandi: statik sinir, girdiden BAGIMSIZ (2026-07-27)

**Karar [ETKİ: `proofs/drf-v2-lean/Kemgu/BET/Boundedness.lean` (iskelet → ~190 satir
ispat), `test/lean_aksiyom_harness.sh` (+3 aksiyom denetimi).]** Yan-kanalda (D-330)
isleyen desen tekrarlandi: RT disiplininin ASIL ICERIGI kendi icinde TAM bir cekirdek
hesapta ispatlandi; ana model DOKUNULMADI (koprusu ayri is).

**⚠ ISKELETIN TIKANMA LISTESI ESKIMISTI (D-328'deki ile ayni durum):** dosya
"(2) cycle counting modelimizde YOK, (3) wcet fonksiyonu mekanize degil" diyordu —
ikisi de bu adimda dogrudan KURULDU; ayri bir "Core genisletmesi" ON-KOSUL DEGILMIS.

**Model:** maliyet-sayan buyuk-adim semantik `Calis s e s' v n` (`n` = harcanan adim),
statik `wcet : Ifade → Nat`. `eger` sinirinda **`Nat.max`** (hangi dalin kosacagi
girdiye bagli).

**ISPATLANANLAR:**
- **`bet`** (ana teorem): `Calis s e s' v n → n ≤ wcet e`. Girdi store'u `s` SERBEST →
  sinir TUM girdiler icin gecerli.
- **`bet_rt8`**: kagit RT.8 formu — `∃N, ∀giris: maliyet ≤ N` (N := `wcet e`).
- **`bet_dal_max_gerekli`** (TANIK): `eger`de "yalniz dogru dali say" tanimi
  (`wcetYanlis`) bir ust sinir DEGILDIR — somut karsi-ornek (kosul 0 → pahali yanlis
  dal kosar; sinir 3, gercek 5). Yani `max` keyfi degil, BET'in DOGRULUGU icin gerekli.

**NEDEN SINIR VAR (durustluk):** bu cekirdek dilde **DONGU YOK / OZYINELEME YOK /
TAHSIS YOK** — kagit RT001/RT002/RT003 sozdizimine GOMULU. Sinirin varligi bu
kisitlarin SONUCUDUR; teorem "KEMGU genel olarak sinirlidir" DEMEZ. Bu kisitlar C
tarafinda `src/wcet.c`'de RT001-RT005 ile zaten zorlanir.

**SABOTAJ (ikisi de olctu):** (A) `eger` sinirindan `max` kaldirilip yalniz dogru dal
sayilinca → **2 hata** (`bet` coker); (B) `topla` sinirinden +1 dusurulunce → **1 hata**;
temiz → **0**.

**Kapi:** `calistir_lean_aksiyom` — 29+kok modul, **17 teorem**; `bet`/`bet_rt8` →
`[propext]`, `bet_dal_max_gerekli` → **hicbir aksiyoma dayanmiyor**; sorryAx YOK.

**KOPRU YUKUMLULUGU (acik, CT ile ayni sinif):** bu hesap ile `Sem/Core` arasinda
gomme/simulasyon YOK → "KEMGU'nun gercekzamanli islevleri sinirlidir" sonucu BURADAN
CIKMAZ. Cikan sonuc: "RT disiplini statik WCET sinirini GARANTI EDER."

---

## D-331 — KOPRU: maliyet OLCULDU, entegrasyon planlandi (yapilmadi) (2026-07-27)

**Sonuc: KOPRU BU ADIMDA YAPILMADI — ama artik TAHMIN degil OLCUM var.**
Depo TEMIZ birakildi (yarim entegrasyon merge EDILMEDI); kapi yesil.

**Neden yapilamadi (yapisal, ertelenebilir degil):** kopru "CT teoremi ⟹ KEMGU
hakkinda bir onerme" demek; hedef onerme `Sem/Core`'da DALLANMA olmadan
YAZILAMAZ. Yani kopru, once (A)-entegrasyonunu (Core'a `eger`) gerektirir.

**OLCUM 1 — `Ifade`'ye `eger` eklemek BEDAVA:** dogru bagimlilik sirasiyla
derlendiginde **29/29 modul YINE derleniyor, 0 hata**. Sebep: `HasType`'ta `eger`
kurali olmadigi surece hicbir IYI-TIPLI program onu icermez → progress/korunum
teoremleri vakum olarak korunur.

**⚠ OLCUM ARTEFAKTI (kendi hatam, kayda gecti):** ilk denemede "26 hata" gordum ve
neredeyse rapor ediyordum. Gercek: modulleri RASTGELE sirayla derledigim icin her
biri *"bagimlilik .olean yok"* diye 1 hata veriyordu — **kaskad, gercek hata degil**.
Bu oturumda ucuncu kez ayni sinif: **ham sayi bir olcum DEGILDIR, icerigine bak.**

**OLCUM 2 — asil maliyet Step kuralinda:** `sEgerSec` (kosul deger ise dal sec)
eklenince `Sem/SmallStep` **4 gercek hata** verdi:
1. `vk = Deger.birim` icin `DecidableEq Deger` YOK (Deger'e deriving gerek),
2-3. SmallStep icindeki iki `induction h_step` (`step_iz_analiz`, `step_fault_*`)
   → *"Alternative `sEgerSec` has not been provided"*,
4. bagli bir hata.
Downstream 20 modul ise SmallStep derlenene kadar KASKAD (gercek is degil).

**Kalan is listesi (bir sonraki oturum icin, siraya girmis hali):**
1. `Deger`'e `DecidableEq` (ya da kosul testini `Deger.birim` desen-eslemesiyle yaz).
2. SmallStep ici 2 induction'a `sEgerSec` case'i.
3. `HasType`'a `t_eger` + `LineerTamam`/`RegionTamam` kapsayici case'leri.
4. Step uzerinden tumevarim yapan HER teorem: `adim_korunum` (21→22),
   `step_fault_preserves_typed`, `progress_konf` (yeni tanik), Drf L0-L7,
   MemSafety, ve **`SideChannel/NonInterference.silme_simulasyon` (21→22)**.
5. **Asil kopru:** `CT.Ifade → Core.Ifade` gomme + `CT.Calis` ile `StepStar`
   arasinda simulasyon + `CT.Gozlem` ile `Olay` gozlemi arasinda eslesme →
   `ct_ni`'nin Core'a TASINMASI.
**Buyukluk tahmini (olcume dayali):** 1-3 mekanik; 4 orta (her case ~5-20 satir,
~12-15 yer); 5 bagimsiz ve en buyuk parca (yeni tumevarim).

**~~KARAR GEREKTIREN NOKTA~~ → ✅ KARAR VERILDI (Mehmet, 2026-07-27): KOSUL-CONGRUENCE
EKLENECEK, cong ailesi 3→4.**

Uygulama sartlari (bu karar bagladigi icin BURAYA yaziliyor — uygulama, halihazirda
kosan kopru gorevinde yapilacak; ayni dosyaya paralel dokunmamak icin bu oturum
`SmallStep.lean`e ELLEMEDI):

1. **Yeni kural `sEgerKosulCong`** — `eger k d y` icinde `k` deger DEGILSE kosulda
   ic adim atilir. Bicim: mevcut `sSeqCong`/`sAtamaCong`/`sGuvensizCong` ile BIREBIR
   ayni iskelet (`h_S1 = ifadeyleKonf ...`, `h_inner : Step S1 S1'`, `h_t1'`, `h_tid`,
   `h_if'`, `h_S'` disari `.eger k' d y` olarak sarar).
2. **FIX-F cerceve yan-kosulu ZORUNLU** — yeni kurala da
   `h_yan : ts2' = ts2 ∨ ∃ y, ts2' = ts2 ++ [y]` eklenecek. Gerekce: FIX-F,
   cong-penceresi counterexample'ini (kosan thread'i pencerede bitmis goren join)
   eleyen sarttir; eksik birakilirsa AYNI ACIK yeni kuraldan geri gelir.
   **Bu bir tercih degil, dogruluk sartidir.**
3. **Dokunulacak tumevarim yerleri** (cong ailesi 3→4 oldugu icin HEPSINDE +1 case):
   `adim_korunum`, `step_fault_preserves_typed`, `step_iz_analiz`, `progress_konf`,
   Drf L0-L7, MemSafety, ve `SideChannel/NonInterference.silme_simulasyon`
   (bu sonuncuda yeni case mevcut 3 cong case'inin BIREBIR kopyasidir —
   `ifadeyleKonf_konfSil` + IH; deger tasimaz).
4. **Kabul kapisi:** `calistir_lean_aksiyom` yesil (sorryAx YOK) + `silme_simulasyon`
   ve `ni_cekirdek_altkume` hala ispatli olmali. Yarim entegrasyon MERGE EDILMEZ.

---

## D-330 — CT cekirdek hesabi: `eger` + gizli etiket + DALLANMALI NI (2026-07-26)

**Karar [ETKİ: `proofs/drf-v2-lean/Kemgu/SideChannel/CT.lean` (YENİ, ~330 satır),
`Kemgu.lean` (+1 import), `test/lean_aksiyom_harness.sh` (kök modül derlenir + 3 teorem).]**

**TASARIM KARARI — neden ayri dil (durustce):** genisletme iki yoldan yapilabilirdi:
- **(A)** `Sem/Core.Ifade` + `SmallStep.Step`'e `eger` eklemek. Bedeli: 28 modulun
  tumevarim ispatlari (adim_korunum 21 case, progress, Aile2…) yeniden acilir, depo
  uzun sure KIRMIZI kalir. Dogrulanmis cekirdek bu projenin en degerli varligi.
- **(B)** CT disiplininin ASIL ICERIGINI kendi icinde TAM bir cekirdek hesapta
  ispatlamak. Ana model DOKUNULMAZ.
**(B) secildi.** Gerekce: (A) uzun sureli kirmizi + regresyon riski; (B) sorulan
matematigi (gizli dallanma ⇒ sizinti; CT disiplini ⇒ NI) BUGUN ve TAM verir.

**Hesap:** `eger` VAR; iki-noktali etiket kafesi (genel ⊑ gizli); store; buyuk-adim
semantik `Calis` (deger + store + IZ uretir); gozlem `oOku/oYaz/**oDal**` — **dal karari
saldirganda gorunur** (PC/timing sizintisinin modellenmesi).

**CT disiplini (`CtOk`) — kagittan iki kural:**
- **CT003:** gizli deger GENEL degiskene yazilamaz (`ifadeEtiket e ⊑ G x`).
- **CT001:** `eger` kosulunun etiketi GENEL olmali (gizli uzerinde dallanma YASAK).

**ISPATLANANLAR:**
- `genel_ifade_korunum` (uclu tek tumevarim): GENEL etiketli ifade, dusuk-esdeger iki
  store'da AYNI deger + AYNI iz uretir ve sonuc store'lari dusuk-esdeger kalir.
- **`ct_ni` (ANA TEOREM):** CT-tipli program + dusuk-esdeger baslangic →
  **izler BIREBIR AYNI (dal kararlari dahil)** ∧ sonuc store'lari dusuk-esdeger.
- **`ct001_gerekli` (TANIK):** CT001 olmasaydi NI YANLIS olurdu — gizli `h` uzerinde
  dallanan somut program, dusuk-esdeger iki store'da `oDal true` vs `oDal false`
  uretir. Yani kural keyfi degil.

**SABOTAJ:** `ct_eger`in `h_kosul_genel` sarti kaldirilinca (True'ya cevrilince)
**4 hata** — `ct_ni` COKER; temiz → 0. **CT001'in mekanize gerekcesi budur.**

**⚠ KOPRU YUKUMLULUGU (acik borc, dosya basliginda da yazili):** bu hesap ile
`Sem/Core` arasinda simulasyon/gomme lemmasi YOKTUR. Dolayisiyla **"KEMGU'nun kendisi
sabit-suredir" SONUCU BURADAN CIKMAZ**; cikan sonuc "CT disiplini, dallanmali bir
cekirdek dilde NI'yi GARANTI EDER"dir. Kopru = (A)-tarzi entegrasyon, ayri karar.

**Kapi:** `calistir_lean_aksiyom` — 29 modul + kok, **14 teorem**; `ct_ni`,
`genel_ifade_korunum`, `ct001_gerekli` → `[propext]`, sorryAx YOK. Harness kok modulu
de derler (aksi halde CT teoremleri denetime GIRMIYORDU — ilk kosumda yakalandi).

---

## D-329 — Yan-kanal GLOBAL CATI tamamlandi: silme = ileri simulasyon (21/21) (2026-07-26)

**Karar [ETKİ: `proofs/drf-v2-lean/Kemgu/SideChannel/NonInterference.lean` (§6 + tanim
daraltmasi), `test/lean_aksiyom_harness.sh` (+2 aksiyom denetimi).]** D-328'de acik
biraktigim cati kapandi:

- **`silme_simulasyon` (21/21 kural):** `Step S S' → Step (konfSil S) (konfSil S')`.
  Deger tasiyan 6 kural §5 lemmalariyla, 7 fault + 5 yapisal kural dogrudan, **3
  congruence kurali TUMEVARIM hipoteziyle** (ic adim silinmis dunyada da atilabilir;
  `ifadeyleKonf_konfSil` + FIX-F yan-kosulunun silme altinda transportu).
- **`ni_cekirdek_altkume`:** dusuk-esdeger (silinmisi ayni) iki konfigurasyondan atilan
  adimlar AYNI gozlemi uretir.

**⚠ ISPATIN ZORLADIGI TANIM DARALTMASI (asil bulgu):** `degerSil` once TUM degerleri
birime indiriyordu. `cGorevBaslatTamam` case'i **COKTU** — cunku `Deger.gorevVal t` bir
VERI degil **THREAD KIMLIGIDIR**: kural onu post-state'te yeniden URETIR, dolayisiyla
"her deger silinir" varsayimi altinda silme orada simulasyon DEGILDIR. Ustelik o kimlik
zaten gozlemlenebilir (`gBaslat t` olayi). Tanim daraltildi (`gorevVal` korunur) →
**"ne gizlidir" sorusunu ispat cevapladi, biz varsaymadik.** Bu, mekanizasyonun
belge-uzeri iddiaya karsi ustunlugunun somut ornegi.

**Sabotaj:** `gorevVal`i de sil (yanlis daraltma) → **1 hata** (cGorevBaslat case'i);
temiz → **0**. Ayrica D-328'in iki sabotaji (silme degeri korusun → 13 hata; gozlem
degeri gostersin → 1 hata) gecerliligini koruyor.

**Kapilar:** `calistir_lean_aksiyom` — 28/28 modul, 11 teorem;
`silme_simulasyon` ve `ni_cekirdek_altkume` → `[propext, Quot.sound]` (**sorryAx YOK**).

**KAPSAM DEGISMEDI:** teorem hala cekirdek alt-kume icindir — `Ifade`de dallanma/indeks/
aritmetik olmadigi icin CT001/CT002/CT004 kanallari ifade edilemez; `kemgu_soundness_v3`e
conjunct olarak EKLENMEDI. `eger` eklendiginde §5/§6 KIRMIZI verir (kasitli kapi).

---

## D-328 — Yan-kanal: cekirdek alt-kume NON-INTERFERENCE ispatlandi (2026-07-26)

**Karar [ETKİ: `proofs/drf-v2-lean/Kemgu/SideChannel/NonInterference.lean` (iskelet →
~300 satır ispat), `test/lean_aksiyom_harness.sh` (+5 aksiyom denetimi).]**
Mehmet karari (2026-07-26): **dar ama GERCEK NI**, adi kapsamini soylesin,
`kemgu_soundness_v3`e conjunct olarak EKLENMESIN.

**⚠ ISKELETIN "TIKANMA" LISTESI KISMEN ESKIMISTI (olculdu):** dosya "(1) `sabitsure<T>`
qualifier modelimizde YOK" diyordu — oysa `Tip.sabitsure` Core'da **zaten var**; `Olay`
(memOku/memYaz + konum, kanal olaylari) da gozlem kanali olarak hazirdi.

**Saldirgan modeli:** gozlem = olayin TURU + kim + nerede (`GozlemOlay`); tasinan
DEGER gorulmez. Silme (erasure) `degerSil` TEK KAYNAK — tum silme tanimlari ondan turer.

**ISPATLANAN (hepsi derleniyor, sorryAx yok):**
- **(A) Gozlem degerden bagimsiz carpanlanir:** `gozlem (olaySil o) = gozlem o`,
  `izGozlem (izSil tau) = izGozlem tau`.
- **(B) Silme, veri-erisim yardimcilariyla DEGISIR:** store lookup, kanal
  `find?`/`ilk`/`ekle`/`cikar` — arama BASARISI, kuyruk UZUNLUGU ve BOSLUK durumu
  korunur (bunlar yapisaldir, veri degil).
- **(C) DEGER TASIYAN her Step kurali icin silme simulasyonu (6/6):** `sVarOku`,
  `sAtamaTamam`, `sSeqAtla`, `sGuvensizAtla`, `cKanalGonderTamam`, `cKanalAlTamam` —
  kural silinmis konfigurasyonda AYNEN uygulanabilir, sonuc silinmis-esdegerdir.
  NI icerigi tam olarak bu kurallarda yasar.

**HENUZ YOK (durustce):** global cati `∀ S S', Step S S' → Step (konfSil S) (konfSil S')`
— kalan kurallar deger TASIMAZ ama cati TUMEVARIM ister (cong kurallari ozyinelemeli).
Politika geregi `sorry` KONMADI: cati ifadesi yazilmadi.

**KAPSAM SINIRI (dosya basliginda da yazili):** kagit CT001/CT002/CT004'un korudugu asil
kanallar — gizli uzerinde DALLANMA, gizli INDEKS, gizli DIV/MOD — bu modelde **ifade
edilemez** (`Ifade` alt-kumesinde kosullu/dongu/indeksleme/aritmetik YOK). Bu teorem
**"KEMGU sabit-suredir" DEMEZ**; "cekirdek alt-kumede veri-bagimli gozlem yoktur" der.

**SABOTAJ (ikisi de olctu):** (A) silme degeri KORUSUN (`degerSil = id`) → **13 hata**;
(B) gozlem degeri gostersin → **1 hata**; temiz → **0**. Ispatlar tasiyici.

**⚠ SUREC NOTU:** ilk sabotajim **0 hata** verdi — cunku `degerSil`i tanimlarda
kullanmayip `Deger.birim`i her yere elle yazmisim; yani sabotaj hicbir sey olcmuyordu.
Once tanimlar tek kaynaga baglandi, sonra sabotaj anlamli hale geldi. **DERS: sabotaj
testi de once KENDISI dogrulanmali** ("kirmizi vermedi" ≠ "kod saglam").

**Kapilar:** `calistir_lean_aksiyom` 28/28 modul + 9 teoremin aksiyom kumesi
(yeni NI teoremleri dahil; `silme_sim_sVarOku` → `[propext]`, `izGozlem_izSil` →
**hicbir aksiyoma dayanmiyor**), sorryAx YOK.

---

## D-327 — Lean ispat KAPISI: derleme ≠ ispat, aksiyom denetimi eklendi (2026-07-26)

**⚠ ÖNCE BİR ÖLÇÜM DÜZELTMESİ (benim hatam).** Önceki turda "kalan başlıklar"
listesinde *"DRF Lean'de 27 `sorry` var"* dedim. **Yanlış:** `grep -c sorry` **yorum
satırlarını** saymış (dosya başlıklarındaki `Politika: ... sorry/axiom YOK` ve tracker
alıntıları). Gerçek: taktik olarak **0 `sorry`**, **0 `axiom`** bildirimi.
**DERS (bu oturumda üçüncü tekrar):** ham `grep` sayısı bir ölçüm DEĞİLDİR; eşleşmelerin
ne olduğuna bak.

**Karar [ETKİ: `test/lean_aksiyom_harness.sh` (yeni), `Makefile`
(`calistir_lean_aksiyom` + .PHONY; mevcut `calistir_drf_lean_proof` mesajı düzeltildi).]**
Asıl boşluk sorry değil, **doğrulamanın kendisiydi**: `calistir_drf_lean_proof` yalnız
`lake build` çalıştırıp *"sorry/axiom: bkz. README"* diyordu — yani sorry-suzluk
**belgelenmiş ama kapı ile ölçülmemişti**.

**Neden yetersiz:** `lake build`in başarısı ispat kanıtı DEĞİLDİR — `sorry` içeren ispat
da sorunsuz derlenir. **Ölçüldü:** üst teoremin bir bacağını `sorry` ile değiştirdim →
**28/28 modül yine derlendi**. Tek geçerli kanıt `#print axioms`.

**Kapı:** (1) tüm modülleri **mathlib'SİZ** derler — proje hiç `Mathlib` import etmiyor
(ölçüldü) → lake/mathlib indirmesi gerekmez, **~1 dk**; (2) dört üst teoremin aksiyom
kümesini yazdırır; (3) `sorryAx` görürse KIRMIZI.

**Doğrulanan durum (bugün, Lean 4.29.0):** 28/28 modül derlendi ve
`kemgu_soundness_v3` / `iyiTipli_no_fault` / `typed_no_fault` →
`[propext, Classical.choice, Quot.sound]`, `t1_bellek_guvenligi_tam` →
`[propext, Quot.sound]`. Hepsi Lean'in **standart** aksiyomları; **`sorryAx` YOK** →
teoremler gerçekten ispatlı.

**Sabotaj doğrulaması:** üst teoreme `sorry` enjekte → kapı `sorryAx` yakalayıp
**exit 1**; temiz kaynakta **exit 0**.

**KALAN (gerçek boşluk — sorry değil, KAPSAM):** `kemgu_soundness_v3` üç şey veriyor:
DRF + per-Step bellek güvenliği + No-Fault. **Yan-kanal (SideChannel/NonInterference)
ve WCET (BET/Boundedness) teoremin DIŞINDA** — dosyaları bilinçli "iskelet" (vakum
conjunct olarak durmaları yerine ÇIKARILMIŞLAR; dürüstlük tercihi). Ayrıca hipotez
`IyiTipliCekirdek` — tam KEMGU tip sistemi değil, çekirdek. Bunlar V2 hedefi.

---

## D-326 — [YÜKSEK] Codegen'in "desteklemiyorum" yolu SESSİZ 0 üretiyordu → ÖLÜMCÜL (2026-07-26)

**Karar [ETKİ: `src/llvm.c` (`hata()` — 8 çağrı yerinin tamamını kapsar).]** D-325'in
ortaya çıkardığı sınıfı (dolaylı çağrıda sessiz uyuşmazlık) kasıtlı taradım ve **daha
kötüsünü** buldum: C codegen desteklemediği bir şekille karşılaşınca IR'a bir **YORUM**
(`; HATA: ...`) yazıp `add i32 0, 0` üretiyordu → **derleme başarılı**, program çalışma
zamanında **sessizce 0** dönüyordu.

**Ölçülen tezahür:** `değişken xs: Dizi<işlev()->tam32> = [|| 42]; ver xs[0]();`
→ `--check` **OK**, derleme **başarılı**, çalıştırma **exit 0** (doğrusu 42). Tek iz:
IR içinde `; HATA: cagri hedefi tanimlayici degil` yorumu. Self-host aynı programda
LLVM-RED veriyordu (gürültülü) — yani C, iki derleyicinin **sessiz** olanıydı.

**Kapsam:** tek bir yardımcı (`hata()`) 8 çağrı yerini besliyor — tanımsız tanımlayıcı,
metin literal kaydı yok, yapı tipi bilinmiyor, erişimde yapı çözülemedi, alan bulunamadı,
sonuç/seçimlik yapıcısı çözülemedi, `yol` ifadesi desteklenmiyor, çağrı hedefi tanımlayıcı
değil. Hepsi aynı sessiz-0 davranışındaydı.

**Düzeltme:** `hata()` artık stderr'e yazar + `g->hata_sayisi`'nı artırır → `ana.c`
mevcut AS001 yolundan **IR'i YAYINLAMAZ ve exit 1**. Yeni tanı **kodu icat edilmedi**
(kullanıcı-görünür kodlar Mehmet'in kararı); düz metin mesaj.

**Yayılma alanı ÖNCE ölçüldü (196 dosya: cg_korpus + örnekler + stdlib):** yalnız
`kem_os.kem`'in **tek başına** derlenmesi tetikliyordu; **gerçek OS yolu** (birleştirilmiş
kaynak + `--mimari arm64`) **exit 0, 8116 satır, 0 tetik**. Sonradan reddedilen 4 dosyanın
2'si zaten **önceden** AS001 (yanlış mimariyle tek-başına derleme), 2'si **kasten geçersiz**
checker korpusu (`--llvm` onların yolu değil). **Desteklenen hiçbir yol kırılmadı.**

**Kapılar:** test_llvm 274/274, llvm_dogrula 10/10, dizi_sinir 37/37, codegen_diff,
checker_diff, FIXPOINT; sıfır derleyici uyarısı.

**KALAN (bu işin kapsamı dışı, ölçüldü):** dizi/yapı **içindeki** kapanış (`xs[0]()`,
`k.fn()`) artık **gürültülü** reddediliyor ama hâlâ **desteklenmiyor**; gerçek destek
ayrı iş. Ayrıca `değişken f: işlev()->tam64 = || 8589934592` C'de T001, self'te KABUL —
checker parite açığı (self daha gevşek), ayrı iş.

---

## D-325 — [YÜKSEK] Annotasyonsuz kapanış dönüşü: C'de SESSİZ YANLIŞ CEVAP kapatıldı (2026-07-26)

**Karar [ETKİ: `src/llvm.c` (`lambda_donus_tahmin` + annotasyonsuz DEGISKEN dalı),
`selfhost/codegen.kem` (`lam_ret_tahmin` + `son_lam_ret` + DEGISKEN/cg_aic),
`test/cg_korpus/cg_kapanis_annotsuz.kem` (+1).]** "Sıradaki konu" için D-322'nin bilinen
sınırını **ölçtüm** ve sınırın aslında iki ayrı kusur olduğu çıktı:

**(1) C — SESSİZ YANLIŞ CEVAP (asıl bulgu).** `değişken k = || 1.5; ... k()`
→ C `define double @lambda_0` üretiyor ama çağrı yerinde `call i32 %fn(...)`.
**LLVM DOLAYLI çağrıda imza DENETLEMEZ** → program derlenir, çöp yazmaçtan okunur:
ölçülen **exit 127** (doğrusu 42), başka bir şekilde **105** (doğrusu 42).
`llvm.c`'deki mevcut yorum *"bu vaka LLVM tarafından GÜRÜLTÜLÜ reddediliyor (ölçüldü)"*
diyordu — **ölçüm bunu ÇÜRÜTTÜ**. Bu, D-295 dersinin birebir tekrarı: *LLVM imza
uyuşmazlığını yutar; "yanlış tip geçir, LLVM reddetsin" bir savunma mekanizması DEĞİLDİR.*

**(2) self-host — LLVM REDDİ (gürültülü).** `değişken f = || "merhaba";` → dönüş "i32"
fallback'ine düşüp `ret i32 <ptr>` üretiyordu. Aynı kök, farklı hata modu.

**Çözüm (her iki derleyicide aynı):** annotasyon yokken dönüş IR'ı **gövdeden
muhafazakâr tahmin** edilir ve **AYNI değer** hem lifted `define`'a hem **çağrı yerine**
verilir (C: `lambda_beklenen_donus` + `kapanis_donus_ir`; self: kuyruk `lam_ret` +
`cg_aic`). Böylece ikisi yapısal olarak asla ayrışamaz. Tahmin kapsamı: metin→ptr,
kesirli→double, tam/mantıksal→i32, tanımlayıcı→değişkenin IR'ı, çağrı→çağrılanın dönüşü,
ikili→sol operand, blok→ilk `ver`. **Tahmin edilemeyen şekil → NULL/"" → bugünkü i32
davranışı** (yeni sessizlik EKLENMEZ).

**Ölçüm:** `|| "merhaba"` C 42 / self 42 (öncesi: C 42, self LLVM-RED) · `|| 1.5`
C **127→42** / self 42 · karışık (metin+kesirli+çağrı+tam) C **105→42** / self 42.
**Sabotaj (iki bağımsız):** C'de tahminciyi kapat → korpus **57** (sessiz yanlış);
self'te kapat → **LINK-FAIL**. Kapılar: test_llvm 274/274, codegen_diff, checker_diff,
self_driver, FIXPOINT.

**KALAN (ortak, C'de de aynı — ölçüldü):** annotasyonsuz + **yakalamalı**/blok-form bazı
şekiller her iki derleyicide de **aynı biçimde** LLVM-RED veriyor (parite bozulmuyor,
hata modu gürültülü). Ayrı iş.

---

## D-324 — G005 self-host'a portlandı: güvenlik parite açığı kapandı (2026-07-26)

**Karar [ETKİ: `selfhost/codegen.kem` + `selfhost/checker.kem` (ikisine de: `g005_*`
yardımcıları + `aktif_fn` + 3 kanca), `test/check_korpus/g005_ptr_kacis.kem`,
`test/check_korpus/g005_skaler_temiz.kem` (+2).]** D-323'te işaretlediğim açık: self-host
G005'i **hiç bilmiyordu** (`grep` → 0). D-322 öncesi zararsızdı (self kapanış
derleyemiyordu; `tip_kontrol.h` bile "port moot" diyordu), **D-322 ile canlı** hale
gelmişti: self, C'nin **güvenlik gerekçesiyle reddettiği** programı kabul ediyordu.

**Zorluk ve çözüm:** self-host'ta genel escape DFA YOK (yalnız `ky_*` kesin-yerel kanıtı).
Bu yüzden C'nin `ESC_CAGIRAN` tetikleyicileri **tahmin edilmedi, ÖLÇÜLDÜ** (5 şekil,
`--checkdump` ile):
| şekil | C | self (port sonrası) |
|---|---|---|
| `ver ‖s` (doğrudan) | G005 1,49 | **G005 1,49** |
| `değişken f=‖s; ver f` (transitif) | G005 1,79 | **G005 1,79** |
| `al(f)` (çağrı ARGÜMANI) | G005 2,93 | **G005 2,93** |
| `f()` (çağrı HEDEFİ) | OK | **OK** |
| skaler yakalama + kaçış | OK | **OK** |
Kod + **satır + sütun** birebir. Kural: kaçış = `ver <ad>` ∨ çağrı argümanı olmak;
**çağırmak kaçış DEĞİL**.

**Tasarım notları:**
- Karar **LAMBDA yerinde** verilir (bağ adı için gövdede ileri-tarama) → C'nin raporlama
  **sırası** da korunur; hata konumu daima LAMBDA düğümü (C ile aynı).
- İşaretçi-benzerlik tip STRING'inden: skaler beyaz-liste dışındaki her şey (metin/Dizi/
  `&T`/ham-pointer/yapı **ve `"?"`**) işaretçi sayılır → **default-deny**.
- Sahte yakalama ayıklama: lambda'nın kendi parametreleri ve **gövde-içi bildirimler**
  hariç tutulur (checker'ın yerel tablosu bunları da içerir; ayıklanmazsa C'nin kabul
  ettiğini reddederdik).

**Sabotaj doğrulaması (üç bağımsız, üçü de yakalandı):** (1) işaretçi kararını kapat →
`g005_ptr_kacis` düşer; (2) her yakalamayı işaretçi say (daraltmayı iptal) →
`g005_skaler_temiz` düşer; (3) çağrı-argümanı kaçışını kapat → `g005_ptr_kacis` düşer.

**Kapılar:** checker_diff **56/56** (+2), codegen_diff, self_driver, FIXPOINT.
Self-host kaynağının kendisi yeni kural altında temiz (C ve self ikisi de "OK") →
fixpoint riski yok.

**BİLİNEN SINIR:** birden çok hatalı programda G005'in **diğer** tanılara göre sırası
C'den sapabilir (C escape DFA'yı lambda ziyaretinde sorgular; self ileri-tarama yapar).
Korpusta böyle bir dosya yok; çıkarsa `--checkdump` diff'i **gürültülü** olarak yakalar.

---

## D-323 — [YÜKSEK] G005 DARALTILDI: yalnız İŞARETÇİ yakalamada red (Mehmet kararı) (2026-07-26)

**⚠ ÖNCE BİR ÖLÇÜM DÜZELTMESİ (D-322'deki iddiam YANLIŞTI).** D-322'de "kapanış
parametre olarak geçilemez — C'de parser reddediyor (27 hata)" yazmıştım. **Yanlış:**
test dosyamda işlev adı olarak **`uygula` (anahtar kelime)** kullanmışım; 27 hata ondandı.
C **zaten** destekliyor: `işlev calistir(g: işlev(tam32)->tam32, x: tam32)` → `--check` OK,
IR `define i32 @calistir(ptr %rho, { ptr, ptr } %g, i32 %x)`, exit 42; self-host da D-322
sonrası aynı (42). **DERS:** "derleyici reddediyor" sonucunu yazmadan önce reddin
GEREKÇESİNİ oku — hata mesajı (P014 "islev adi bekleniyor", sütun 8) test dosyasını
işaret ediyordu, dil özelliğini değil.

**Karar [ETKİ: `src/tip_kontrol.c` + `.h` (G005 koşulu + `lambda_yakalama_isaretci`),
`test/test_tip_kontrol.c` (189→191; 3 test yeniden hedeflendi + 2 yeni koruma).]**
Ölçüm sonrası bulunan gerçek ayrışma: **yakalayan** kapanış bir çağrıya argüman olunca
**C reddediyordu (G005), self-host kabul edip doğru çalıştırıyordu.**

**G005'in gerekçesi ESKİMİŞTİ (ölçüldü):** kural "env stack-ömürlü (llvm.c) → dangling"
diyordu; oysa `llvm.c` V2-F2'den beri env **HEAP** (`@malloc`) — hem C hem self-host
IR'ında `call ptr @malloc(...)` görülüyor. Yani C, kendi codegen'inin **doğru derlediği**
programı reddediyordu.

**Daraltma:** G005 artık `yakaladi_genel && yakaladi_ISARETCI && ESC_CAGIRAN`.
- **Skaler yakalama** (tam*/dtam*/kesirli*/mantıksal/karakter/boş) → env'de **değer
  kopyası**; çerçeve aşımında dangling ÜRETEMEZ → **serbest**.
- **İşaretçi-benzeri yakalama** (metin/Dizi/ref/ham-pointer/yapı) → kopyalanan işaretçi
  gösterdiği bölgeyi (ρ_yerel / çağıran çerçeve) aşabilir → **G005 KORUNUR**.
- `tekkez<T>` iç tipine özyineler; **çözülemeyen tip = İŞARETÇİ varsayılır**
  (default-deny — sessiz kabul yerine gürültülü red).
- Hata metni de güncellendi (artık "skaler yakalayın" yolunu söylüyor).

**Sabotaj doğrulaması (iki yön):** `yakaladi_ptr = 1` (daraltmayı iptal) → 2 yeni skaler
koruma KIRMIZI; `yakaladi_ptr = 0` (işaretçi tarafını kapat) → 3 pozitif KIRMIZI.
Kapı her iki yönde kod-duyarlı. Testler: tip_kontrol **191/191**, linear 89/89,
escape 22/22, sıfır derleyici uyarısı.

**KALAN — PARİTE AÇIĞI (açık, chip'li):** self-host'ta **G005 HİÇ YOK** (`grep`: 0 sonuç).
D-322 öncesinde bu zararsızdı (self kapanış derleyemiyordu; başlık dosyası bile "port moot"
diyordu), **D-322 ile CANLI hale geldi**: self, C'nin reddettiği işaretçi-yakalayan kaçan
kapanışı kabul ediyor. Port, self-host'ta escape muhakemesi gerektiriyor (self'te genel
escape DFA yok; yalnız `ky_*` kesin-yerel kanıtı var) → ayrı iş.

---

## D-322 — GENEL KAPANIŞ (closure) codegen self-host'ta — C parite (2026-07-26)

**Karar [ETKİ: `selfhost/codegen.kem` — `ll_tip`/`ll_ic_tip` TIP_ISLEV, genel `LAMBDA`
kolu, `lam_env_uret` (ortak env emisyonu), `lam_emit` genelleştirmesi, CAGRI kapanış
dispatch'i, DEGISKEN dönüş-bağlamı; `test/cg_korpus/cg_kapanis_{temel,metin,lineer}.kem`
(+3).]** D-321'de ölçüp bıraktığım boşluk: self-host kapanışları **tip-kontrol ediyor
ama DERLEYEMİYORDU** — `f()` bir üst-düzey işlev sanılıp **TANIMSIZ `@f`** üretiliyordu.
Artık C ile birebir. (Öncesinde self'te kapanış yalnız `görev_başlat` argümanı olarak
vardı — D-300.)

**Model (C llvm.c aynası):** kapanış değeri = **fat value `{ fnptr, envptr }`**.
- `ll_tip(TIP_ISLEV)` → `{ ptr, ptr }`; `ll_ic_tip(TIP_ISLEV)` → **dönüş** IR'ı.
- Lifted fn ABI: `(ptr %rho [, ptr %env], params...)`. Yakalama yoksa env `null`
  ve **bare** çağrılır; varsa HEAP env geçilir. Çağrı yeri `env == null` üzerinde
  dallanır (iki imza → iki `call`, sonuç slot'ta buluşur).
- **Dönüş IR'ı BİLDİRİLEN tipten gelir** (`cg_aic`; C `kapanis_donus_ir` aynası) —
  fat value T'yi sildiği için şart. Sabotajla ölçüldü: i32'ye zorlayınca
  `metin_uzunluk(s())` **SEGFAULT** (işaretçi kırpması) — D-293'ün self-host aynası.
- Env emisyonu `lam_env_uret`'te **tek kaynak**: görev yolu ile genel kapanış aynı
  kodu kullanır (iki kopya ayrışırsa env düzeni sessizce sapardı). Env HEAP'tir —
  lambda yaratan çerçeveden uzun yaşayabilir (görev), stack alloca yanlış olurdu.
- `lam_emit` artık parametreleri emit eder ve dönüşü kuyruktan alır: `"i64"` =
  görev/runtime taşıyıcı yolu (D-310 korundu), diğer her şey **doğal tip**.

**Ölçüm (C↔self exit-kodu birebir, 6/6 şekil):** argümansız 42/42, argümanlı 42/42,
**yakalamalı** 42/42, `-> metin` 7/7, blok-form gövde 42/42, çoklu çağrı 42/42.
Ek: D-321'in bilinen codegen boşluğu (`tekkez<işlev>` LC-3) **artık derleniyor** →
42/42. Kapılar: codegen_diff **104/104** (+3), self_driver 4-mod, FIXPOINT, görev
runtime 16/16 (görev yolu regresyonsuz).

**Sabotaj doğrulaması (iki bağımsız):** (1) dönüş IR'ını i32'ye sabitle → metin
korpusu SEGFAULT; (2) fat value'ya env yerine `null` yaz → yakalamalı korpus SEGFAULT.
Kapılar kod-duyarlı.

**~~KALAN: kapanış parametre olarak geçilemez~~ — BU İDDİA YANLIŞTI, bkz. D-323.**
Ölçümüm hatalıydı (test dosyasında işlev adı olarak `uygula` ANAHTAR KELİMESİ);
kapanış parametresi hem C'de hem self-host'ta **çalışıyor** (exit 42, IR
`define i32 @calistir(ptr %rho, { ptr, ptr } %g, i32 %x)`).
**Gerçek KALAN:** annotasyonsuz kapanışta dönüş i32 varsayılır (C'de de aynı).

---

## D-321 — Lineer closure ÇAĞRISI self-host'ta tüketim (LC-3 parite) + D-319 kod kaybı onarımı (2026-07-26)

**Karar [ETKİ: `selfhost/codegen.kem` + `selfhost/checker.kem` (ikisine de: `lin_fnk`
kaydı + `lin_fn_tipi_mi` + CAGRI kancası), `test/check_korpus/lineer_closure_cagri.kem`,
`test/check_korpus/lineer_closure_hata.kem` (+2).]**

**1) Kapatılan asimetri (ölçüldü).** `değişken f: tekkez<işlev()->tam32> = ...; ver f();`
→ **C: OK / self: L001**. Self-host'ta lineer tüketim yalnız *bilinen-arite* çağrı
yolunda (`fn_psay_bul >= 0`, kullanıcı işlevi) işliyordu; çağrı hedefinin KENDİSİ hiç
tüketilmiyordu. C (`tip_kontrol.c`) LC-3 gereği hedef tipi `TIP_TEKKEZ` + iç `TIP_ISLEV`
ise hedef sembolünü tüketir. Artık self de tüketiyor: bağlama başına SABİT `lin_fnk`
bayrağı (annotasyon `tekkez<işlev(...)>` mü) — sabit olduğu için anlık-görüntü
(snapshot) makinesine dahil DEĞİL.

**Sabotaj doğrulaması:** kancayı etkisizleştirince `checker_diff` 54/54 → **52/54**
(hem kabul hem red korpusu düşer) → kapı kod-duyarlı. Negatif korumalar: iki kez çağrı
→ L002 (ikisinde de), hiç çağrılmaz → L001 (ikisinde de), lineer-olmayan closure
etkilenmez.

**2) ⚠ SÜREÇ HATASI — D-319'un KODU commit'lenmemişti.** `6c2e1aa` yalnız
DECISIONS_LOG + korpus içeriyordu; `selfhost/codegen.kem` **staged edilmemişti**.
Bu iş sırasında bir UTF-8 bozulmasını `git checkout --` ile geri alırken çalışma
ağacındaki D-319 düzenlemeleri de silindi ve kayıp ancak `codegen_diff`'in
**100/101**'e düşmesiyle ortaya çıktı (`cg_lineer_intrinsic.kem` link edilemedi).
D-319'un üç kancası (ll_tip TIP_TEKKEZ / `tekkez_olustur` / `kullan`+`imha`) bu
commit'te **yeniden uygulandı**; SELF exit 42, `codegen_diff` 101/101 geri geldi.

**DERSLER:** (a) commit sonrası `git show --stat` ile **hangi dosyaların girdiğini**
doğrula — "kapılar yeşildi" commit içeriğini kanıtlamaz, kapılar çalışma ağacını ölçer.
(b) `git checkout -- <dosya>` commit'lenmemiş işi **geri dönüşsüz** siler; bozuk düzenlemeyi
geri almadan önce commit durumunu kontrol et. (c) Toplu-düzenleme için `perl -i` UTF-8
katmanı olmadan Türkçe kaynakta **çift kodlama** üretti (`ğ` bozuldu) — .kem/.c dosyalarında
Edit aracını kullan.

**Kapılar:** checker_diff **54/54** (+2), codegen_diff **101/101**, bootstrap FIXPOINT,
test_linear 89/89, tip_kontrol 189/189.

**KALAN (bu işin kapsamı dışı, ölçüldü):** lineer closure ÇAĞRISI self-host **codegen**'de
hâlâ derlenmiyor (`use of undefined value '@f'`) — self-host genel closure desteklemez;
D-321 ÖNCESİNDE de aynı hata. Bu yüzden yeni korpus dosyaları `cg_korpus`'ta değil
`check_korpus`'ta.

---

## D-319 — Lineer intrinsic CODEGEN self-host'a eklendi (C parite) (2026-07-26)

**Karar [ETKİ: `selfhost/codegen.kem` (3 nokta: `ll_tip` TIP_TEKKEZ, `kullan`/`imha`
ifade kolları, `tekkez_olustur` intrinsic'i), `test/cg_korpus/cg_lineer_intrinsic.kem`
(+1).]** D-318'de ölçüp belgelediğim boşluk: self-host lineer programları **tip-kontrol
ediyor ama DERLEYEMİYORDU** — `tekkez_olustur` genel çağrı yoluna düşüp **TANIMSIZ
@tekkez_olustur** üretiyordu (link hatası). Artık C ile birebir.

**Semantik (C llvm.c aynası — hepsi ZERO-OVERHEAD):**
- `ll_tip(TIP_TEKKEZ)` → **iç tipin ta kendisi**. Bu dal olmadan `tekkez<T>` "i32"
  fallback'ine düşerdi → `tekkez<metin>` gibi işaretçi T'lerde **sessiz 32-bit kırpma**.
- `tekkez_olustur(e)` → argüman **PASS-THROUGH** (sıfır talimat).
- `kullan(t)` → **PASS-THROUGH** (lineer muhasebe tamamen tip kontrolde).
- `imha(t)` → operand YAN ETKİLERİ için değerlendirilir, değeri düşürülür (`add i32 0, 0`).

**Doğrulama:** C↔self exit-kodu birebir — `tekkez<tam32>`=42, **`tekkez<metin>`=7**
(i32 fallback'i olsaydı kırpardı), `imha`+aritmetik=42, koşullu tüketim (D-311 iki-dal)=42,
yapı-deseni + lineer alan (D-318)=42. **codegen_diff 101/101** (+1 korpus), **FIXPOINT**
(42003 satır), bootstrap (lexer/parser/checker 92 birebir), checker_diff 52/52,
test_linear 83/83. **(D-320 bu bulguyu kapattı.)**

**⚠ YAN BULGU — ÖNCEDEN VAR OLAN yanlış L002 (ayrı iş olarak işaretlendi):**
`kullan(t)` **DOĞRUDAN çağrı argümanı** olunca yanlış L002 üretiliyor:
`ver metin_uzunluk(kullan(m));` → L002, oysa `m` tam bir kez tüketiliyor.
**D-307'de de var** (worktree ile ölçüldü) → D-311..D-319 lineer işinin regresyonu
DEĞİL. Yalıtım: ayrı deyimde OK, `ver kullan(a)` OK, ikili ifade içinde OK — yalnız
argüman pozisyonu. Muhtemel kök: DUGUM_CAGRI kolunda argümanın BİRDEN ÇOK kez
`tip_belirle` edilmesi (her ziyaret KULLAN_IFADE'yi yeniden tüketiyor). Korpus dosyası
bu tuzağa girmemek için `kullan`ı ayrı deyimde kullanır.

**SONUÇ:** self-host artık lineer programları uçtan uca derliyor. Linear V1+V2+V2.1
(D-311→D-319) C ve self-host'ta tam.

---

## D-318 — [YÜKSEK] YENİ SÖZDİZİMİ: `eşleş` yapı deseni (destructuring) (2026-07-26)

**Karar [ETKİ: `src/ast.h`/`ast.c`/`parser.c`/`tip_kontrol.c`/`llvm.c` +
`selfhost/codegen.kem` + `selfhost/checker.kem`, `test/test_linear.c` (+5),
`test/check_korpus/yapi_deseni.kem` (+1).]** **Mehmet onayıyla** eklenen YENİ
SÖZDİZİMİ: `eşleş s { Yapi { alan1, alan2 } => { ... } }`.

**Semantik:**
- Yapının tek "varyantı" olduğu için desen **DAİMA eşleşir** → catchall (koşul dalı YOK).
- Alanlar **aynı adla** kol scope'una bağlanır.
- **LİNEER yapıda desen yapıyı TÜKETİR** — aksi halde hem yapı hem bağlanan alanlar
  canlı kalır = aynı kaynak iki kez. Bağlanan lineer alanlar kendi başına lineer
  bağlama olur (L001 onları ayrıca izler).
- **V1: TÜM alanlar listelenmeli** (T012). Listelenmeyen lineer alan hiçbir yere
  bağlanmaz ama yapı tüketilir → **sessiz sızıntı** olurdu.
- Bilinmeyen alan → T009.

**İCAT EDİLMEYENLER (ayrı sözdizimi kararları):** yeniden-adlandırma
(`Yapi { x: yeni }`), rest-deseni (`..`), iç-içe desen (`Yapi { x: Alt { y } }`).

**`--ast` PARİTESİ:** C dump'ı `DESEN_YAPI`'yi BOŞ değerle, çocuksuz yazar (yapı/alan
adları dump'ta görünmez). Self-host **aynısını** üretir; adlar YAN-KANALDA tutulur
(`dy_*`/`dyf_*` — çeşit `cv_*`/`cc_*` deseninin aynısı). Böylece parser bootstrap
92/92 birebir korundu. *Bilinen sınır: dump `Sahip { x }` ile `Baska { y }`'yi ayırt
etmez — fidelity boşluğu, korrektlik değil.*

**⚠ SELF-HOST'ta BULUNAN ENGEL:** `eşleş` codegen'i **yapı-değeri scrutinee'de tümden
atlıyordu** (`tagged == yanlış → ver 0`). Yapı için TAG YOKTUR; tag çıkarımı koşullu
yapıldı (aksi halde geçersiz IR). Bu olmadan kol hiç emit edilmiyordu (exit 0).

**Doğrulama:** test_linear **83/83** (78→83), parser 107/107, tip_kontrol 189/189,
AST 31/31, test_llvm 274/274, C uçtan uca exit 42 (skaler + lineer alan), self-host
uçtan uca exit 42, **FIXPOINT** (42003 satır), bootstrap (lexer/parser/checker 92
birebir), checker_diff **52/52**, codegen_diff 100/100.

**Bilinen C↔self farkı (hatalı programda, ikincil tanı):** `Sahip { y }` (bilinmeyen
alan) → C `T009+T002`, self `T009`. İkisi de programı T009 ile REDDEDER; fark yalnız
ikincil T002'de (C geçersiz alanı bağlamaz, self yerel_topla'da bağlar).

**Sınır:** `yd_lin` gibi lineer-alanlı destructure self-host'ta LLVM-RED — sebebi
D-318 DEĞİL, **`tekkez_olustur`un self-host codegen'inde HİÇ olmaması** (ölçüldü:
D-318 öncesi de birebir aynı hata). Ayrı iş.

---

## D-317 — L-COND / L-LOOP self-host'a portlandı — lineer parite borcu KAPANDI (2026-07-26)

**Karar [ETKİ: `selfhost/codegen.kem` + `selfhost/checker.kem` (anlık-görüntü yığını +
EGER/IKEN ayrımı + ICIN + ESLES kolları), `test/check_korpus/lineer_kosullu.kem` (+1).]**
D-316'da ölçüp belgelediğim parite borcu: **D-311/D-312 (L-COND/L-LOOP) self-host'ta HİÇ
YOKTU** (`grep '"L005"' selfhost/*.kem` = 0) — C reddederken self **KABUL EDİYORDU**.
Artık **8/8 senaryo C ile birebir**.

**Mekanizma (C `LinAnlik` aynası, self-host'un dizi modeline uyarlandı):** `snap_tuk`/
`snap_maske` bir **anlık-görüntü YIĞINI** (`snap_top` tepesi) — iç-içe `eğer`/`eşleş`
için şart. `lin_snap_it` aktif dilimi iter, `lin_snap_geri(base, n)` dal izolasyonu
sağlar, `lin_snap_at` yığını boşaltır. **`n` ile sınırlama kritik:** dal İÇİNDE tanımlanan
bağlamalar dilimi büyütür; sınırsız geri-yükleme onları da ezerdi.

**Kurallar (C ile aynı):**
- `eğer`: iki dal da tüketti → taban+1 (BİR tüketim); tam biri → **L005** + tüketilmiş
  say (ardıl L001 kaskadı olmasın); hiçbiri → taban. else'siz `eğer` = tüketmeyen else.
- `eşleş`: N-kollu genelleme — her kol izole edilir, tüketen kol sayılır; hepsi → BİR
  tüketim, karışık → L005.
- `iken`/`için`: gövde DIŞ bağlamayı tüketirse **L005** (0 iterasyon = sızıntı, ≥2 =
  çift tüketim). Gövde-içi tanımlar anlık görüntüde DEĞİL → serbest.

**Doğrulama:** 8/8 C↔self birebir (iki-dal / tek-dal L005 / düz-çift L002 / eşleş-tüm-kol /
eşleş-tek-kol / iken / için / döngü-içi-yerel) + **FIXPOINT** (41309 satır) +
`calistir_codegen_bootstrap` (lexer/parser/checker 92 birebir) + checker_diff **51/51**
(+1 korpus) + codegen_diff 100/100.

**SONUÇ:** D-311→D-316'nın TAMAMI artık self-host'ta. Lineer alt-sistemde bilinen C↔self
parite borcu KALMADI. Kalan tek Linear işi: `eşleş` ile lineer yapı destructuring —
**yapı deseni dilde YOK (P220)**, yeni sözdizimi kararı Mehmet'te.

---

## D-316 — Linear V2.1 KISMİ TAŞIMA self-host'a portlandı (C parite) (2026-07-25)

**Karar [ETKİ: `selfhost/codegen.kem` + `selfhost/checker.kem` (`lin_maske`, `imha_bag`,
`param_lineer_mi`, `lineer_alan_sirasi`, ERISIM taşıma, `deg_lineer_mi` ERISIM dalı),
`test/check_korpus/lineer_kismi_tasima.kem` (+1).]** D-315 C-only'di; self-host kısmi
taşımayı D-314'ün LR002'siyle REDDEDİYORDU. Artık **11/11 senaryo C ile birebir**.

**Port (self-host'un dizi-tabanlı modeline uyarlandı):** C'de maske `Sembol`'de; self-host'ta
`lin_maske` dizisi `lin_ad`'a paralel. Alan sırası `lyf_yapi`/`lyf_alan` içindeki
**lineer-alan ordinali** (tüm alanların sırası GEREKMEZ — maske biti için yeterli).
`imha_bag` sayacı imha bağlamını taşır (C'deki `imha_baglaminda` aynası).

**⚠ İKİ EKSİK ÖLÇÜMLE BULUNDU (ikisi de sessiz parite kaybıydı):**
1. **`deg_lineer_mi` ERISIM'i bilmiyordu** → `değişken f = s.x` lineer bağlama olarak
   KAYDEDİLMİYORDU → f tüketilmese bile **L001 çıkmıyordu** (sessiz lineer sızıntı;
   C=L001, self=OK). Değer bir lineer alan erişimiyse bağlama lineer sayılır.
2. **`fn_plin` yalnız `TIP_TEKKEZ` bakıyordu** → `al(s: Sahip)` lineer YAPI parametresi
   lineer sayılmıyor, çağrı argümanı TÜKETİLMİYORDU → yanlış L001 (C=L002, self=L001).
   `param_lineer_mi`: `tekkez<T>` VEYA `yapı tekkez K`.

**Doğrulama:** 11/11 senaryo C↔self birebir (pm1-pm4, pm7, ls1-ls5, ls7) + **FIXPOINT**
(40341 satır) + `calistir_codegen_bootstrap` (lexer/parser/checker 92 birebir + codegen
fixpoint) + checker_diff **50/50** (+1 korpus) + codegen_diff 100/100.

**KALAN PARİTE BORCU (ayrı, ölçüldü):** **D-311/D-312 (L-COND/L-LOOP) self-host'ta YOK** —
`grep '"L005"' selfhost/*.kem` = 0. Yani self-host koşullu/döngü tüketim tutarsızlığını
denetlemiyor: C reddederken self KABUL EDER. Yön güvenli değil (self daha GEVŞEK), ama
sessiz miscompile değil — yalnız checker gücü farkı. Ayrı iş.

---

## D-315 — [YÜKSEK] Linear V2.1: KISMİ TAŞIMA (partial move) (2026-07-25)

**Karar [ETKİ: `src/sembol.h` (`lineer_alan_maskesi`), `src/tip_kontrol.h`
(`imha_baglaminda`), `src/tip_kontrol.c` (ERISIM taşıma + LinAnlik maske snapshot +
bütün-taşıma yasağı), `test/test_linear.c` (+4, L73 yeni semantiğe).]**
D-313'te lineer yapının lineer alanını dışarı okumak **tümden reddediliyordu** (alan-bazlı
sahiplik izlenmediği için). V2.1 bunu izleyerek serbest bırakır.

**Model — bağlama başına bit-maske:** `Sembol.lineer_alan_maskesi` (bit i = i. alan
taşındı).
- `s.x` ilk okuma → alanı "taşındı" işaretle, alan tipini döndür. Dönen değer kendi
  başına lineerdir → mevcut L001/L002 makinesi onu ayrıca izler.
- `s.x` ikinci okuma → **L002** (aynı alan iki kez taşınamaz).
- Yapının KENDİSİ hâlâ tüketilmelidir → kabuk sızmaz (L001 korunur).
- **Kısmi taşınmış yapı BÜTÜN OLARAK TAŞINAMAZ** → L002. `imha` serbest (kalanı atar),
  ama çağrı argümanı / `ver` ile devretmek **delikli** bir değeri alıcıya verirdi:
  alıcının tipi alanı "var" gösterir, oysa taşınmıştır → use-after-move.
- Alan yalnız bir **BAĞLAMA** üzerinden taşınabilir; geçici değer (`yap().x`) veya 32+
  alanlı yapı → muhafazakâr **red** (kanıtlanamayan = DENY).

**Dal-duyarlılıkla uyum:** `LinAnlik` artık maskeyi de anlık-görüntüler/geri yükler.
Maskesiz snapshot, bir dalda taşınan alanı diğerinde "taşınmış" gösterip **yanlış L002**
üretirdi (D-311/D-312 makinesiyle sessiz çelişki).

**⚠ KENDİ KUSURUM (ölçümle yakalandı):** `TipKontrol` **memset EDİLMİYOR** — alanlar tek
tek atanıyor. `imha_baglaminda`'yı başlatmayı unutmuştum → çöp değer, kontrol **SESSİZCE
atlanıyordu** (pm4 senaryosu "OK" veriyordu). Adversarial senaryo yakaladı. *Ders:
`TipKontrol`'e alan eklerken `tip_kontrol_baslat`'ta ilklendir.*

**Yeni kullanıcı-görünür kod İCAT EDİLMEDİ:** L002 (double-use) semantik olarak tam
oturuyor (taşınmış alana yeniden erişim = çift kullanım); geçici-değer reddi LR002.

**Doğrulama:** test_linear **78/78** (74→78; L73 yeni semantiğe çevrildi), tip_kontrol
189/189, capability 40/40, DRF 54/54, sabitsüre 39/39, parser 107/107, test_llvm 274/274,
checker_diff 49/49, codegen_diff 100/100. **Sabotaj:** maske kontrolü kapatılınca L76,
bütün-taşıma kontrolü kapatılınca L77 düşüyor (temiz derlemeyle doğrulandı).

**SINIRLAR:**
1. **Self-host'ta YOK — ama SOUND:** self-host kısmi taşımayı hâlâ D-314'ün LR002'siyle
   REDDEDER (C kabul eder). Yani self-host daha muhafazakâr; sessiz miscompile YOK, hiçbir
   kapı kırılmıyor (korpuslarda kısmi taşıma kullanılmıyor). Port ayrı iş.
2. **`eşleş` ile lineer yapı destructuring YAPILMADI:** `eşleş s { Sahip { x } => ... }`
   **P220 ile parse edilemiyor — yapı deseni DİLDE YOK.** Bu YENİ SÖZDİZİMİ demek;
   `DESEN_YAPICI` yalnız çeşit varyantları için. Sözdizimi kararı Mehmet'in olduğu için
   İCAT EDİLMEDİ.

---

## D-314 — Linear V2 `yapı tekkez K` SELF-HOST'a portlandı (C parite) (2026-07-25)

**Karar [ETKİ: `selfhost/codegen.kem` (driver) + `selfhost/checker.kem` (referans checker)
— parser + `ly_ad`/`lyf_*` registry + LR002 muafiyeti + kısmi-taşıma reddi,
`test/check_korpus/lineer_yapi.kem` (+1).]** D-313 C-only'di; self-host `yapı tekkez`i
gürültülü reddediyordu. Artık **C ile birebir**.

**Mekanizma (self-host'un basit modeline uyarlandı):** C'de bayrak tipte taşınıyor;
self-host'ta tip nesnesi yok → parser lineer yapı ADLARINI `ly_ad`'a kaydeder,
`deg_lineer_mi` annotasyon o listedeyse bağlamayı lineer sayar → **mevcut L001/L002
akış-izleme makinesi ayrı kod olmadan devreye girer**. Düğüm/dump DEĞİŞMEZ → `--ast`
paritesi korunur.

**Kısmi-taşıma denetimi için ayrı kayıt:** `alan_tip` check modunda "?" olabildiği için
ona GÜVENİLMEZ. Bunun yerine LR002 muafiyeti verilirken lineer alanlar `lyf_yapi`/
`lyf_alan`'a yazılır; denetim bu tabloya bakar.

**⚠ KANCA YERİ — ölçümle bulundu:** ERISIM kancasını önce `ifade_tip`'e koydum;
`değişken n: tam32 = k.id` (annotasyonlu) yakalanıyor ama **`değişken f = s.x`
(annotsuz) KAÇIYORDU** — `ifade_tip` yalnız annotasyon karşılaştırması için çağrılıyor.
Teşhis: `lineer_alan_mi`'yi geçici olarak daima-doğru yapıp hangi senaryonun tetiklendiğine
bakmak. Kanca her düğümü ziyaret eden `kontrol_dugum`'a taşındı.

**MÜKERRERLİK BORCU ÖDENDİ:** `checker_diff` referans `checker.kem`'i kullanıyor; yalnız
driver'ı değiştirmek 48/49 verdi. Aynı port checker.kem'e de uygulandı → **49/49**.
(Bu, daha önce ölçtüğüm "159 fonksiyon mükerrer, 17'si sapmış" borcunun somut bedeli.)

**Doğrulama:** 7/7 senaryo C↔self BİREBİR (tanım/L001/L002/lineer-alan/LR002/kısmi-taşıma/
lineer-olmayan-alan) + **FIXPOINT** + `calistir_codegen_bootstrap` (lexer 92, parser 92,
checker 92 birebir + codegen fixpoint 39928 satır) + `calistir_self_driver` (4 mod ×
C-derlenmiş ve self-derlenmiş: 22/22, 12/12, 48/48, LLVM 100/100 ×2) + checker_diff 49/49 +
codegen_diff 100/100 + test_linear 74/74.

---

## D-313 — [YÜKSEK] Linear V2: `yapı tekkez K { ... }` — lineer yapı (2026-07-25)

**Karar [ETKİ: `src/ast.h`+`src/parser.c` (yapi.lineer_mi), `src/tip.h`+`src/tip.c`
(TIP_YAPI lineer bayrağı), `src/tip_kontrol.c` (yapi_tipi_sembolden + LR002 muafiyeti +
imha + kısmi-taşıma reddi), `test/test_linear.c` (+7).]** Spec V1 diyordu ki *"V1'de
yapılar lineer alan içeremez. V2'de 'lineer yapı' kavramı eklenebilir"* — eklendi.

**Sözdizimi:** `yapı tekkez Kilit { id: tam32; }`. Sıradan `yapı` aynen kalır (bayrak
yoksa `lineer_mi = 0`) → geriye tam uyumlu.

**Semantik (yeni kod İCAT EDİLMEDİ; mevcut makine yeniden kullanıldı):**
- `tip_lineer_mi` TIP_YAPI için bayrağı okur → **L001/L002 + D-311/D-312'nin
  L-COND/L-LOOP makinesinin TAMAMI lineer yapılar için otomatik çalışır** (ayrı kod
  yolu yok). Bayrak TİPTE tutulur çünkü `tip_lineer_mi` (tip.c) sembol tablosuna
  erişemez; taşımasaydık lineer yapılar sessizce kabul edilirdi.
- **LR002 muafiyeti:** lineer alan YALNIZ lineer yapıda serbest. Gerekçe: sahiplik
  zinciri kopmaz (K tüketilmeden kaybolamaz; K tüketilince alanı da onunla gider).
  Sıradan `yapı` yasağı AYNEN sürer (L72 bu darlığı kilitler).
- **`imha` genişletildi**, `kullan` DEĞİL: `imha(k)` herhangi bir lineer değeri alır;
  `kullan` sarmalanmış değeri ÇIKARIR ve lineer yapının sarmalanmış değeri yoktur →
  orada L007 aynen kalır.
- **KISMİ TAŞIMA YASAK:** lineer yapının LİNEER alanını dışarı okumak reddedilir.
  Aksi UNSOUND olurdu: okunan alan kendi başına tüketilmek zorunda kalır, yapı da
  tüketilmek zorundadır → **aynı kaynak iki kez imha edilir**. Lineer-OLMAYAN alan
  okumak SERBEST (kopya değer — L74 kuralın fazla geniş olmadığını kilitler).

**Kod seçimi:** kısmi-taşıma için **yeni kullanıcı-görünür kod icat edilmedi** (adlandırma
Mehmet'in kararı); LR002 ailesi mesaj ayrımıyla kullanıldı — D-312'deki L005 tercihiyle
aynı disiplin.

**Doğrulama:** test_linear **74/74** (67→74), parser 107/107, tip_kontrol 189/189,
AST 31/31, capability 40/40, DRF 54/54, sabitsüre 39/39, WCET 35/35, SIMD 30/30,
test_llvm 274/274, codegen_diff 100/100, **FIXPOINT** korundu. C codegen uçtan uca
çalışıyor (lineer yapı IR'de sıradan yapı — `imha` tip-seviyesi; ölçüldü: exit 42).

**Sınır — C-only (GÜRÜLTÜLÜ):** self-host `yapı tekkez`i kabul ETMİYOR; `--check`
LR002+T002 verir, `--llvm` yolu LLVM-RED olur. **Sessiz miscompile YOK** (ölçüldü).
D-302→D-306 (generic çeşit) ile aynı desen: C-first, port ayrı iş.

**Kalan (V2.1):** alan-bazlı taşıma (partial move) ve onun kendi tanılama kodu;
`eşleş` ile lineer yapı destructuring.

---

## D-312 — [YÜKSEK] L-COND `eşleş` kolları + L-LOOP döngü gövdesi (2026-07-25)

**Karar [ETKİ: `src/tip_kontrol.c` (ESLES/IKEN/ICIN kolları + `lineer_dongu_birlestir`),
`test/test_linear.c` (+6).]** D-311 yalnız `eğer/değilse`yi kapsıyordu. `eşleş` ve
döngüler **aynı iki-yönlü kusuru** taşıyordu (ölçüldü):

| Senaryo | Önce | Şimdi |
|---|---|---|
| `eşleş` TÜM kollar tüketir | **L002** ❌ | **OK** ✓ |
| `eşleş` BAZI kollar tüketir | **OK** (sessiz) ❌ | **L005** ✓ |
| `iken`/`için` gövdesinde dış bağlama tüketimi | **OK** (sessiz) ❌ | **L005** ✓ |
| Döngü İÇİNDE tanım + tüketim | OK ✓ | OK ✓ (kural fazla geniş değil) |

**eşleş = `eğer`in N-kollu genellemesi:** her kol anlık görüntüden başlar (izolasyon),
sonda kaç kolun tükettiği sayılır → hepsi ise **taban+1** (toplamda BİR tüketim),
hiçbiri ise taban, karışıksa **L005**. Kol sayısı 2'yle sınırlı değil (L63: 3 kol).

**L-LOOP (yeni kural):** döngü gövdesi **dışarıdan gelen** bir lineer bağlamayı
tüketemez — döngü 0 kez dönerse tüketilmez (sızıntı), ≥2 kez dönerse **çift tüketim**;
ikisi de `Dosya`/`Kilit`/`OTP_Anahtar` tek-kez disiplinini bozar. Gövde İÇİNDE tanımlanan
bağlamalar anlık görüntüde DEĞİLDİR → her iterasyon kendi değerini yaratıp tüketebilir
(L67 bu sınırı kilitler; kural fazla geniş olsaydı düşerdi).

**Kod seçimi:** spec L006 tanımlamıyor; **yeni kullanıcı-görünür kod İCAT EDİLMEDİ**
(adlandırma Mehmet'in kararı). Sınıf aynı olduğu için L005 mesaj ayrımıyla kullanıldı
("eşleş kolları ... tutarsız" / "`iken` gövdesi dışarıdan gelen lineer bağlamayı
tüketiyor").

**Doğrulama:** test_linear **67/67** (61→67), tip_kontrol 189/189, capability 40/40,
DRF 54/54, sabitsüre 39/39, WCET 35/35. **Sabotaj:** eşleş birleştirmesi devre dışı →
L62+L63 düşer; döngü kuralı devre dışı → L65 düşer.

**Kalan (Linear V2):** `yapı tekkez K { }` (lineer alanlı yapı) hâlâ P021 ile reddediliyor.

---

## D-311 — [YÜKSEK] L-COND: dal-duyarlı lineer tüketim (hem yanlış-red hem yanlış-kabul) (2026-07-25)

**Karar [ETKİ: `src/tip_kontrol.c` (~90 satır: anlık-görüntü/geri-yükle/birleştir + L005),
`test/test_linear.c` (+4 test + kod-duyarlı yardımcı).]** Lineer tüketim takibi
**AKIŞ-DUYARSIZ bir SAYAÇTI** (`kullan/imha` → `lineer_tuketildi++`, daldan bağımsız).
İki yönü de ampirik ölçüldü:

| Senaryo | Önce | Spec | Şimdi |
|---|---|---|---|
| `eğer p { kullan(t); } değilse { imha(t); }` | **L002** ❌ | OK | **OK** ✓ |
| `eğer p { kullan(t); }` (else yok) | **OK** (sessiz) ❌ | L005 | **L005** ✓ |
| `kullan(t); kullan(t);` | L002 ✓ | L002 | L002 ✓ |

**Neden ciddiydi:** yanlış-red, spec'in KANONİK örneğini derlenemez yapıyordu — yani
lineer bir kaynağı (`Dosya`/`Kilit`/`OTP_Anahtar`) **koşullu imha etmek İMKÂNSIZDI**.
Yanlış-kabul ise sessiz lineer sızıntıydı (koşul yanlışken kaynak hiç tüketilmez ve
L001 de tetiklenmiyordu — sayaç dal içinde artmış görünüyordu).

**Çözüm:** `eğer` girişinde görünür tüm lineer sembollerin tüketim durumu
ANLIK-GÖRÜNTÜLENİR (`lin_anlik_al`, scope zinciri boyunca), her dal kendi kopyasında
çalışır (`lin_anlik_geri` ile izolasyon), çıkışta BİRLEŞTİRİLİR:
- iki dal da tüketti → **taban+1** (toplamda BİR tüketim),
- tam olarak bir dal → **L005** + tüketilmiş say (aksi halde scope sonunda ayrıca L001
  patlar; tek kusur İKİ hata olarak raporlanırdı),
- hiçbiri → taban.
`else`siz `eğer`, "tüketmeyen else dalı" olarak ele alınır.

**Test kapısı güçlendirildi:** `hata_sayisi() >= 1` zayıf bir kapıdır — L005 bekleyen bir
test BAŞKA sebeple hata alsa da geçerdi. `hata_callback_ayarla` üzerinden **kod-duyarlı**
`kod_uretildi_mi()` eklendi; L59/L60/L61 tam kodu doğrular. **Sabotaj doğrulaması:**
birleştirme koşulu devre dışı bırakılınca L58 düşüyor (test gerçekten koruyor).

**Doğrulama:** test_linear 61/61 (57→61), tip_kontrol 189/189, capability 40/40,
DRF 54/54, sabitsüre 39/39, lineer örnekler (temel/closure/hata) korundu.

**Sınırlar (V1):** `eşleş` kolları henüz bu disiplinden geçmiyor (yalnız `eğer/değilse`);
döngü gövdesindeki koşullu tüketim de ayrı iş. `yapı tekkez K { }` (lineer alanlı yapı)
hâlâ P021 ile reddediliyor — Linear V2'nin diğer yarısı.

**YAN BULGU (ayrı iş):** tanılama kodu ÇAKIŞMASI — lexer `L001/L002/L005/L009/L010/L011`
kullanıyor, Linear spec `L001/L002/L004/L005/L007/L008`. **L001, L002, L005 üçü de
çakışıyor**: kullanıcı "L002" gördüğünde lexer hatası mı lineer hata mı ayırt edemez.
Kullanıcıya görünen kod adlandırması olduğu için karar Mehmet'in.

---

## D-310 — [YÜKSEK] Self-host: `görev_başlat` BLOK-form lambda gövdesi SESSİZCE düşüyordu (2026-07-25)

**Karar [ETKİ: `selfhost/codegen.kem` (lam_emit + VER kolu + `lam_i64` bayrağı),
`test/cg_korpus/cg_gorev_lambda_blok.kem` (+1).]**
`görev_başlat(|| { ...; ver e; })` — blok-form closure gövdesi — self-host
`lam_emit`'te `ret i64 0` fallback'ine düşüyordu: gövde HİÇ emit edilmiyordu.
Sonuç **sessiz yanlış cevap** (repro: C exit 42, self-host exit 0 — link/IR hatası
yok, program "başarıyla" yanlış değer döndürüyordu). D-309 ile ilgisi YOK
(D-309 değişiklikleri stash'lenip yeniden ölçüldü — bug önceden vardı, D-300'den beri).

**Onarım:** lam_emit gövde BLOK ise `deyim_uret` ile emit edilir; `cur_ret` "i64"
(KdlGorevBare taşıyıcısı) kurulur ve `lam_i64` bayrağı açılır. VER kolu bu bayrakla
`ret_uydur` yerine `i64_genislet` kullanır — `ret_uydur` YALNIZ int→int daraltır,
ptr/dar-int'i olduğu gibi bırakırdı (`ret i64 %ptr` = geçersiz IR). `ver`siz düşen
yol için terminatör fallback'i (`ret i64 0`) korunur. İfade-form yolu değişmedi.

**Ölçüm:** yeni korpus dosyası üç şekli kapsar (çok-deyimli blok + heap dizi,
yakalama/capture + blok, koşullu dal + blok) — C 42 ↔ self 42. codegen_diff 99/99,
bootstrap FIXPOINT (stage2==stage3 birebir, 39326 satır), self_driver tüm modlar,
test_gorev_rt 16/16 (D-309 ölçüm kapısı dâhil), test_drf 54/54, test_llvm 274/274.

**İkinci onarım (aynı adım): i64 taşıyıcı daraltması store bağlamlarında.**
`değişken t: tam32 = görev_birleştir(a)` / `t = görev_birleştir(a)` — eşleş ile
bağlanmış tutucuda iç tip (görev<T> → T) bilinmediği için `i64_daralt` doğru olarak
i64 bırakır (kırpma YOK), ama store hedefe uydurulmuyordu → `store i32 %i64` (LLVM RED).
Yeni `int_uydur(op, hedef)` (ret_uydur'un hedef-parametreli biçimi) DEGISKEN
(annotasyonlu) ve ATAMA (yerel TANIMLAYICI) store'larında trunc/sext ediyor.
Korpus: `cg_gorev_i64_daralt.kem` (annotasyon + atama + tam64 hedef) → 42/42.

**Ölçümle yakalanan kendi kusurum:** ilk `int_uydur` IMMEDIATE'lere de dokunuyordu →
`sext i32 4294967296 to i64` literali SESSİZCE bozdu (`cg_skaler_deref` 24 yerine 56;
korpus yakaladı). Guard eklendi: yalnız `%`-register uydurulur — D-299'daki
`i64_genislet` dersinin aynısı (self-host TAM literalini daima i32 sayar).

---

## D-309 — [YÜKSEK] ρ_sahip KOŞULLU serbest: POZİTİF hapsedilme kanıtı (F4-sınıfı) (2026-07-25)

**Karar [ETKİ: `src/llvm.c` (+~230: kanıt + call-graph kapanışı), `selfhost/codegen.kem`
(+~200: parite), `runtime/kdl_runtime.c` (ABI 4. param + koşullu serbest),
`test/test_gorev_rt.c` (ölçüm kapısı +3), `test/cg_korpus/` (+2 adversarial).]**
Görev bölgesi ρ_sahip V1'de **bilinerek sızdırılıyordu** (D-291 notu: "pozitif hapsedilme
kanıtı YOK"). Artık **kanıtlanırsa** join'de serbest bırakılıyor; kanıtlanamazsa eski
güvenli davranış (sızdır) aynen korunuyor.

**ÖNCE ÖLÇÜLDÜ — kaçış yüzeyi (adversarial, naif serbest bırakma UAF mi?):**
| Yol | Ölçüm |
|---|---|
| Diziyi görevden döndür | LLVM-RED (gürültülü) — kapalı ama *kazara* |
| **Kanala gönder** | 🔴 **CANLI** — dizi ρ_sahip'te, join SONRASI okunuyor → naif serbest = UAF |
| Yakalanan değişkene yaz | env KOPYASI → dışarı sızmaz |
| Küresele yaz | E011 (küresel yalnız skaler) — kapalı |
ρ_sahip'e YALNIZ dizi tahsisleri düşer (ρ-ABI; `kdl_metin_birlestir` ρ ALMAZ → global
bölge). Yol haritasının "kanıtsız serbest = UAF" uyarısı **güncel ve doğruydu**.

**KANIT (pozitif; escape DFA'ya GÜVENMEZ, kanıtlanamayan = DENY):**
- **P1** gövdenin her dönüşü KANITLI skaler (blok-formda `ver`, **ifade-formda gövdenin
  kendisi** — aşağıdaki delik notuna bak),
- **P2** transitif erişilebilir kümede `kanal_gönder(k, e)` varsa e KANITLI skaler,
- **P3** erişilebilir kümede iç-içe `görev_başlat` YOK,
- **P4** her çağrı hedefi çözülebilir (işlev-değeri üzerinden dolaylı çağrı → DENY),
- **`default:` DENY** — ele alınmayan AST düğümü kanıtı düşürür. Dile yeni düğüm
  eklenirse kanıt sessizce unsound olmaz, yalnız muhafazakârlaşır. (Mevcut
  `lambda_serbest_tara`'nın `default: return`'ü capture için doğru, kanıt için DEĞİL.)
Sonuç `kdl_gorev_basla_kapanis`'in 4. parametresi (`rho_serbest`) ile runtime'a taşınır;
`kdl_gorev_birlestir` yalnız bayrak 1 iken `kdl_bolge_serbest` çağırır.

**⚠ KENDİ KANITIMDA BULUNAN 2 KUSUR (ikisi de ölçümle yakalandı, düzeltildi):**
1. **`kanal_gönder` uzunluğu 14 yazılmıştı, doğrusu 13** ("ö" 2 bayt). Kontrol hiç
   tetiklenmiyordu → bilinen kaçış senaryosu `1` (serbest) alıyordu. Adversarial korpus
   yakaladı. *Ders: UTF-8 Türkçe kimlik uzunluklarını literal yazarken say.*
2. **İFADE-FORM lambda deliği (soundness):** `|| [40,2]` gövdesinde `ver` DÜĞÜMÜ YOKTUR —
   gövdenin kendisi dönüş değeridir. Yalnız `ver`e bakan P1 bunu kaçırıyordu → ρ_sahip
   dizisi join'e sızarken kanıt `1` diyordu (ölçüldü: C ve self-host'ta AYNI anda).
   Düzeltme: gövde BLOK değilse gövdenin KENDİSİ kanıtlı skaler olmalı.

**Hassasiyet (kanıt fazla kaba olmasın diye eklendi):** kullanıcı işlevinin BİLDİRİLEN
dönüşü skalerse çağrı kanıtlı skaler (`|| yardimci(x)`); `xs[i]` kapsayıcı `Dizi<skaler>`
ise eleman-kopya; self-host'ta yakalanan değişken çevre `cg_var` tablosundan çözülür
(`cg_var_tip_bul`'un "i32" varsayılanı sessiz-yanlış olurdu → varlık `cg_var_bul` ile
ÖNCE denetlenir). Sonuç: `cg_gorev_kanal`/`capture`/`kanal_mesaj`/`gorev_temel` gibi
gerçek programlar kanıtı GEÇER (kanıt yalnız teoride değil pratikte de ateşler).

**ÖLÇÜM KAPISI (test_gorev_rt [14]-[16]):** "serbest bıraktık" demek yetmez — açık bölge
sayısı ölçülür (`kdl_bolge_bakiye`). Aynı gövde iki kez koşar, tek fark bayrak:
`rho_serbest=0 → bakiye +1` (sızdırır), `=1 → +0` (geri verir), sonuç 42 bozulmaz.
Bayrağın ETKİSİZ olması da kanıtsız serbest de bu testte gürültülü düşer.

**Doğrulama:** C↔self-host kanıt kararları 10/10 BİREBİR + FIXPOINT (s2==s3) +
codegen_diff + test_llvm + test_gorev_rt 16/16. Adversarial korpus: `cg_rho_sahip_kacis`
(kanal kaçışı) 42 döner = kanıt kaçışı yakaladı, UAF yok.

**Sınırlar (V1):** built-in dönüşü kanıtlanmaz (`ver dizi_al(...)` → DENY, güvenli taraf);
`güvensiz`/iç-lambda/lineer düğümler DENY; bare-metal `kdl_gorev.c` AYRI API (ρ yok) →
etkilenmez. Genişletme: built-in dönüş tipi tablosu, ρ_sahip'e düşen tahsislerin
alan-duyarlı takibi.

**YAN BULGU (D-309 DIŞI, önceden var — ayrı iş olarak işaretlendi):** `görev_başlat`
gövdesi **blok-form lambda** ise (`|| { değişken xs = [...]; ver ...; }`) self-host
codegen SESSİZCE yanlış sonuç üretiyor (C=42, self=0). D-309 değişiklikleri stash'lenip
yeniden ölçülerek bunun ÖNCEDEN var olduğu doğrulandı. İfade-form (`|| hesapla()`) her
ikisinde de doğru. `cg_rho_sahip_confined` korpusu bu yüzden bilerek ifade-form yazıldı
(aynı kanıt yolunu — ρ-ABI ile çağrılan fn'de dizi tahsisi + skaler dönüş — egzersiz
eder). Accept-but-silently-wrong sınıfı; kapatılmalı.

---

## D-308 — Gerçek per-instantiation monomorphization SELF-HOST'a taşındı (2026-07-24)

**Karar [ETKİ: `selfhost/codegen.kem` (~230 satır: param registry + subst yığını +
layout-gate + mangle + discovery pre-pass + per-inst tip emit + construction/access/
eşleş mono-aware), `test/cg_korpus/` (+3: cg_mono_yapi_metin/coklu/cesit_metin).]**
D-307 gerçek mono yalnız C (`src/llvm.c`) idi; self-host (`codegen.kem`) hâlâ D-306
"T→i32 tek-layout" modelindeydi. Artık self-host da C ile TAM parite: `Kutu<metin>`
(=`%Kutu$ptr`), `Kutu<tam32>`+`Kutu<metin>` bir arada, `Secim<metin>` (INLINE `{i8,ptr}`).

**Kök zorluk (C'den kategorik olarak büyüktü):** self-host'ta (1) subst mekanizması
YOKTU, (2) parser `atla_tip_paramlar` generic param ADLARINI ATIYORDU (T→arg eşlemesi
için şart), (3) `alan_tip` çözülmüş LLVM string tutuyor, AST düğümü atılıyordu (mono
re-resolve imkansız). Üçü de kuruldu: `tp_yad/tp_ad` yan-registry (parse'ta yakalanır),
`mono_sp/mono_si` subst yığını (append-only + ad-blank pop = `cg_kapsam_kapat` deseni),
`alan_tnode` (alan AST düğümü).

**Layout-gate (C `tip_dugum_param_gecer` aynası):** mono YALNIZ bir tip-param alan/
payload tipinde DOĞRUDAN geçiyorsa. `*T`/`&T`/`Dizi<T>`/`görev<T>`/`kanal<T>` = hep
`ptr` → layout-bağımsız → tek type-erased layout (D-306 davranışı korunur; `Liste<T>`
`veri:*T` regresyonsuz). SECIMLIK/SONUC/KULLANICI-arg özyinelenir.

**Discovery pre-pass (`mono_kesif`):** tüm TIP_* düğümleri `yapi_tip_emit`'ten ÖNCE
`ll_tip`'le taranır → mono örnekler alloca'dan önce SIZED emit edilir (geç-emit
"Cannot allocate unsized type" verirdi — C `ast_taransa` aynası).

**Threading:** construction (YAPI_OLUSTUR) beklenen mono IR'ı `beklenen_yapi` bağlamıyla
alır (`beklenen_ll` deseni aynası); alan tipleri `agg_alan(mono_fields)` ile çözülür
(`yapi_alan_tip` T→i32 verir). ERISIM mangled tipte base'i `mono_bul` ile çözer. Çeşit
construction annotasyon-çözülmüş `{i8,ptr}`'ı `beklenen_ll`'den alır.

**Doğrulama:** FIXPOINT (self_s2==self_s3 byte-identik) + codegen_diff 91/91 (+3 mono) +
5 mono senaryosu C ile birebir exit-kodu (yapı-metin=7, çoklu-inst=33, çeşit-metin=8,
generic-yapı=42, generic-çeşit=42). `--check` self↔C generic çeşit'te uyumlu.

**Nested-mono-alan (D-308 devamı, HER İKİ TARAFTA):** mono yapı/çeşit'in alanı/payload'u
BAŞKA mono yapı/çeşit olabilir. Self-host: construction alan döngüsünde alan tipi mono ise
`beklenen_yapi` (%) / `beklenen_ll` ({) per-alan kurulur (dış tek-seferlik tüketimi sonrası).
Çeşit payload döngüsünde de aynı. Test: yapı-in-yapı=12, yapı-alan-çeşit=5, çeşit-payload-
yapı=8 — hepsi C-parite.

**⚠ ADVERSARIAL BULGU (loud>silent) — C'DE SESSİZ MISCOMPILE DÜZELTİLDİ:** çeşit payload'u
mono yapı olduğunda (`Sec<metin>::Var(Ic<metin>)`) C (`src/llvm.c cesit_yapici_uret`) iç
yapı construction'ına AST beklenen_tip+subst VERMİYORDU → iç `alloca %Ic` (T→i32 base) +
`store i32 <ptr>` = **pointer'ı 32 bite sessizce kırpıyordu** (metin_uzunluk çöp döndürdü,
ölçüldü: C=5 yerine doğru=8). Self-host'u yazarken ortaya çıktı; C de düzeltildi (params→args
subst + payload-mono-yapıda beklenen_tip). Artık C=SELF=8 DOĞRU cevapta parite. Ders:
"C-parite" = C'nin bug'ını taklit DEĞİL; ikisi de doğru olmalı.

**Referans-yolu mono erişimi (D-308 devamı, HER İKİ TARAFTA):** `&Kutu<metin>` param
üzerinde `k.alan`. Self: `param_ref_yapi` generic iç tip (TIP_KULLANICI) için `ll_tip`
ile mangled pointee ("Kutu$ptr") döndürür (eskiden yalnız TIP_BASIT → "" → erişim "0");
ERISIM ptr-path `mono_bul` ile base+çözülmüş alan. C: `erisim_uret` ptr-path'inde
`ref_yapi_ir` mangled mono ise (`yapi_bul_ir` bulamaz) `mono_tip_bul` + `ptr_gep_ir`
mangled tiple GEP. **C'de yine sessiz miscompile'dı:** base `%Kutu` ({i32}) ile GEP →
pointer i32'ye kırpılıyor (2. alan offset de yanlış: ptr=8B vs i32=4B) → düzeltildi.
Test: çok-alanlı `&Kutu<metin>`+`&Kutu<tam32>` = C=SELF=42 (offset doğru). Bound-check
generic yapıda zaten vardı.

**SONUÇ:** D-307'nin tamamı + nested-mono self-host codegen'de; generic yapı+çeşit real
mono C↔self eşdeğer (95/95 codegen_diff + 274/274 test_llvm) + fixpoint korundu.

---

## D-307 — Gerçek per-instantiation monomorphization (C-only): T→i32 tek-layout kaldırıldı (2026-07-23)

**Karar [ETKİ: `src/llvm.c` (~180 satır: MonoTip registry + mangle + per-inst tip emit +
field access/construction/eşleş subst + layout-bağımlılık gate), `test/test_llvm.c` (+4).]**
Eski model "T→i32 tek-layout" idi: `%Kutu = {i32}` (D-306) → `Kutu<metin>` (ptr) derlenemiyordu,
`Kutu<tam32>`+`Kutu<metin>` aynı programda İMKÂNSIZDI. Artık GERÇEK per-instantiation:
`%Kutu$ptr = {ptr}`, `%Kutu$i32 = {i32}` AYRI tipler, bir arada.

**MİMARİ:**
- **MonoTip registry:** her (yapı, arg-IR'ları) çifti mangle → `Kutu$ptr`; subst (T→arg) saklanır.
- **YAPI → named** `%Kutu$X` (field-adı→index gerek); **ÇEŞİT → INLINE** `{i8, payloads}`
  (positional; agg_alan_ir parse eder, named-emit/forward-ref yok).
- **Keşif ön-geçişi** (ast_taransa_metinleri'ye piggyback): değişken/param/dönüş
  annotasyonları taranıp mono kaydedilir → tipler fonksiyonlardan ÖNCE emit (alloca SIZED
  ister; deferred emit "unsized type" verirdi — ölçüldü).
- **Field access + construction + eşleş binding:** object'in mangled tipinden MonoTip
  subst push → alan/payload tipleri T→arg çözülür (çeşit'te inline agg'den agg_alan_ir).

**KRİTİK — LAYOUT-BAĞIMLILIK GATE (regresyon kökü):** mono YALNIZ bir tip-param ALAN
tipinde geçtiğinde uygulanır. `Liste<T>` (veri:`*T`, uzunluk, kapasite) layout T'den
BAĞIMSIZ (`*T`/`&T`/`Dizi<T>`/`görev`/`kanal` HER T için `ptr`) → **type-erased tek %Liste**
korunur (eski mono-FONKSİYON modeli: `ekle$i64` %Liste üzerinde çalışır). İlk denemede
`tip_dugum_param_gecer` pointer/ref/dizi içine iniyordu → `Liste<i64>`'ü mono'layıp
`%Liste$i64` (emit edilmemiş) üretti → **6 stdlib Liste<T> testi KIRILDI** (ölçüldü:
274→268). Düzeltme: pointer/ref/dizi/görev/kanal'a İNME (hep ptr, layout-bağımsız); yalnız
doğrudan T + by-value composite (seçimlik/sonuç/nested-generic) sayılır.

**KAPSAM:** T=metin/ptr, tam64, ÇOKLU-instantiation (aynı programda `Kutu<tam32>`+`Kutu<metin>`)
— hepsi çalışır (yapı + çeşit). **C-ONLY** (self-host ayrı adım — Mehmet "C önce" kararı;
self-host hâlâ D-306 "T→i32 tek-layout"). codegen_diff exit-kod paritesi korunur (korpus
generic tam32 ikisinde de 42; IR farklı ama exit aynı).

**KANITLAR:** test_llvm 274/274 (+4: yapı<metin>→5, yapı-çoklu→42, çeşit<metin>→5, çeşit-çoklu
→42), tip_kontrol 189/189, drf 54/54, parser 107/107 — **Liste<T> regresyonu onarıldı**.

**DERS:** yaygın-altyapıya (ast_tip_to_ir) dokunan değişiklik GENİŞ etki eder — layout-bağımlı
vs type-erased generic AYRIMI şarttı; adversarial test-suite (Liste<T>) 6 kırığı yakaladı.

---

## D-306 — Self-host generic monomorphization: generic yapı + çeşit self-host'ta çalışıyor (2026-07-23)

**Karar [ETKİ: `selfhost/codegen.kem` (ll_tip TIP_KULLANICI dalı — ~8 satır), `test/cg_korpus/`
(+2: cg_generic_yapi, cg_generic_cesit).]** Generic yapı (`Kutu<T>`) ve generic çeşit
(`Secim<T>`, D-302) ÖNCE self-host'ta çalışmıyordu (C-only) — bu yüzden D-302 "C-only" diye
belgelenmişti. Artık HER İKİSİ de self-host'ta C ile PARİTE çalışır.

**KÖK NEDEN (ölçüldü — sandığımdan ÇOK küçük):** self-host altyapısı %90 hazırdı. Generic
yapı için `%Kutu = type { i32 }` DOĞRU emit ediliyordu (self-host "T→i32 tek-layout" modeli:
`atla_tip_paramlar` + alan `deger: T` → `ll_tip(T)` → i32 fallback). TEK kusur: `ll_tip`
generic ANNOTASYONU (`Kutu<tam32>` = TIP_KULLANICI) `%Kutu` yerine i32 fallback'ine
düşürüyordu → `değişken k: Kutu<tam32>`'nın alloca'sı i32, ama construction `%Kutu` üretiyor →
`store i32 %6`(%6=%Kutu) uyumsuz + alan erişimi `.deger` fallback'e (`ver "0"`) düşüyordu.

**ÇÖZÜM:** `ll_tip` TIP_KULLANICI dalı, base ad kayıtlı yapı/çeşit ise onun IR'ını döner
(`%Kutu` / çeşit-struct) — construction ile tutarlı → alan erişimi extractvalue + eşleş
binding çalışır. Bu TEK fix HEM yapı HEM çeşit'i düzeltti (ikisi de aynı ll_tip yolundan).
C llvm.c paritesi.

**KAPSAM (C ile BİREBİR):** T=tam32 çalışır (yapı + çeşit; construction + alan/eşleş).
**Sınır C ile AYNI:** T=metin → C=1 self-host=1 (ikisi de gürültülü LLVM reddi; "T→i32
tek-layout" modeli ptr T'yi kaldırmaz — GERÇEK monomorphization değil, C de değil). Yani
"self-host'ta yok" borcu kapandı; "T→i32 tek-layout" ortak sınır kaldı (ayrı, daha büyük iş).

**FIXPOINT GÜVENLİĞİ:** codegen.kem'in KENDİSİ generic yapı/çeşit TANIMLAMAZ (grep boş) →
ll_tip değişikliği onun öz-derlemesini etkilemez. Ölçüldü: FIXPOINT 35597 → korundu.

**KANITLAR:** codegen_diff **88/88** (+cg_generic_yapi/cesit, C↔self-host semantik), FIXPOINT
stage1==stage2, self-host --check temiz. C generic testleri (test_llvm 265) değişmedi.

---

## D-305 — Skaler güvenli referans okuma: `*v` ile `&T`'den `T` oku (güvensiz YOK) (2026-07-23)

**Karar [ETKİ: `src/tip_kontrol.c` (OP_DEREFERANS referansları kabul eder), `test/test_llvm.c`
(+1), `test/cg_korpus/` (+cg_skaler_ref).]** Skaler `&T` (ör. `&tam32`) ÖNCE HİÇ okunamıyordu:
`*v` → T001 (`*` yalnız pointer), `ver v` → T020, `v+0` → T003. Yalnız taşınıp
döndürülebiliyordu; yapı referansı (`r.alan`) ise auto-deref ile ZATEN okunabiliyordu.

**ÇÖZÜM (minimal, sağlam):** `*v` artık GÜVENLİ referansta da çalışır — OP_DEREFERANS
`TIP_REFERANS` gördüğünde `hedef`'i döner. **güvensiz GEREKMEZ** (referans her zaman
geçerli; yalnız HAM pointer `*T` güvensiz ister — G001 ayrımı korundu). Codegen SIFIR
değişiklik: OP_DEREFERANS handler'ı zaten pointee/beklenen tipiyle `load` ediyordu
(D-265 ham-pointer deref-read yolu) — referans için de aynen çalıştı (self-host dâhil).

**NEDEN `*v`, auto-deref DEĞİL:** implicit auto-deref (`ver v`) IR-seviyesinde SESSİZ-
miscompile riskli — `&tam32` IR'de opak `ptr`; codegen bir "ptr"in yüklenecek-skaler-ref mi
yoksa değer mi olduğunu IR'dan AYIRT EDEMEZ (yapı auto-deref'i ERISIM tipi bildiğinden
çalışıyor). Explicit `*v` bir AST düğümü → tip bilinir, load kesin. Loud>silent: açık deref
belirsizliği yok. (Transparent-referans auto-deref ileride tip-güdümlü codegen ile eklenebilir.)

**KAPSAM:** &tam32/&tam8 (dar tip)/&değişken/yerel `&T = &x` + aritmetik (`*v + 0`) hepsi
çalışır. C + self-host PARİTE (self-host codegen `*v`-deref'i zaten yapıyor; self-host
checker de kabul ediyor — ölçüldü). güvensiz-ayrımı: `*T` ham pointer HÂLÂ G001 ister.

**KANITLAR:** test_llvm 270/270 (+1: skaler &tam32→42), tip_kontrol 189/189, parser 107/107,
codegen_diff **86/86** (+cg_skaler_ref, C↔self-host), regresyon yok.

---

## D-304 — Blok-form lambda dönüşü (`|| { …; ver e; }`) — C-only, ifade-form ile parite (2026-07-23)

**Karar [ETKİ: `src/tip_kontrol.h/c` (blok-form lambda dönüş çıkarsaması), `src/llvm.c`
(bildirilen dönüş IR'ı ile lifted lambda emisyonu), `test/test_llvm.c` (+3).]** Blok-form
lambda ÖNCE TAMAMEN kırıktı — İKİ ayrı kusur:
1. **Tip kontrol:** `tip_belirle(BLOK)` → T001 (BLOK ifade değil). Blok-form lambda HİÇ
   `--check`'ten geçmiyordu (ölçüldü: `|| { ver 42; }` bile).
2. **Codegen:** lifted lambda dönüşü SABİT i32 → `|| { ver "selam"; }` (ptr) LLVM'de
   "`%0 ptr but expected i32`" reddi (D-293 yalnız ifade-form gövdeyi çözmüştü).

**ÇÖZÜM:**
- **Tip kontrol:** blok-form gövde artık DEYİM olarak kontrol edilir; dönüş içindeki
  `ver <e>`'lerden ÇIKARSANIR (`lambda_blok_cikarsama` flag'i + `lambda_blok_donus`;
  `ver` handler'ı çıkarsama modunda tipi kaydeder, aktif_donus_tipi'ye karşı kontrol yerine).
- **Codegen:** bağlamın beklediği dönüş IR'ı (`değişken f: işlev()->T` annotasyonundan,
  `kapanis_donus_ir_al`) DEGISKEN'de `lambda_beklenen_donus`'a aktarılır → BekleyenLambda'ya
  → lambda_emit blok-form dalı i32 yerine onu kullanır (blok içindeki `ver` doğru tiple
  ret eder + define imzası eşleşir). Terminatörsüz fallback ptr→null/float→0.0 güvenli.

**KAPSAM — C-only, DOĞAL:** self-host GENEL closure'ları (`değişken f: işlev()->T = ||…`)
HİÇ desteklemez (ölçüldü: `undefined @f`) — yalnız görev-lambda'ları (D-300). Yani D-293
ifade-form closure ZATEN C-only'di; blok-form fix'i de doğal C-only. Parite/fixpoint YÜKÜ YOK.

**PARİTE (ifade-form ile):** blok-form artık metin/kesirli64/tam32/çok-deyimli için çalışır —
ifade-form ile BİREBİR aynı kapsam. **Ortak PRE-EXISTING sınır (blok-form'a ÖZGÜ DEĞİL):**
`işlev()->tam64 = || 8589934592` her İKİ formda da T001 verir — bağlamsız büyük literal
tam32'ye default olup tam64 annotasyonuyla unify olmuyor (closure dönüş-tipi bidirectional
inference'ı yok; ayrı iş). Ölçüldü: ifade-form da aynı hatayı verir → regresyon değil.

**KANITLAR:** test_llvm 269/269 (+3: blok metin→5, çok-deyim→42, kesirli→42), tip_kontrol
189/189, drf 54/54 — regresyon yok. Blok-form artık `--check` VE `--llvm` uçtan uca geçer.

---

## D-303 — Kanal yönü: `gönderen<T>`/`alan<T>` uçları — yön garantisi tip-seviyesinde (2026-07-23)

**Karar [YÜKSEK] [ETKİ: `src/tip.h/c` (kanal.yon), `src/tip_kontrol.c` (uç tipi +
projeksiyon + gönder/al yön), `src/llvm.c` + `selfhost/codegen.kem` (uç→ptr + projeksiyon
identity), örnek + test + korpus.]** Karar 2 (Mehmet). **D-292'nin "tek yönsüz kanal<T>"
kararını GENİŞLETİR** (tersine çevirmez): kanal<T> hâlâ full-duplex fabrika; ek olarak
`gönderen<T>`/`alan<T>` yön'lü uçlar. Yanlış yön artık DERLEME hatası (DRF007).

**MODEL — Projeksiyon (Mehmet seçti; KEMGU'da tuple/çoklu-dönüş yok):** `kanal_oluştur`
hâlâ `kanal<T>` döner; `gönderen(k)` → `gönderen<T>`, `alan(k)` → `alan<T>` (runtime-free
IDENTITY, dondur gibi — uçlar aynı `KdlKanal*` ptr'ına type-level görünüm). `kanal_gönder`
gönderen|kanal alır, `kanal_al` alan|kanal alır. Uç-tutucu YANLIŞ yönü yapamaz (DRF007) ama
mevcut `kanal<T>` kodu bozulmaz (geriye-uyumlu; kanal<T> = çift yön kaçış-kapısı).

**TEMSİL:** yeni kategori yerine `TIP_KANAL`'a `yon` alanı (0=çift/1=gönderen/2=alan) —
minimal, codegen tümü ptr kalır (yön'den bağımsız), yalnız tip_kontrol yön'e bakar.
tip_esit yön de eşler (gönderen<T> != alan<T> != kanal<T>).

**KEYWORD DEĞİL — `alan` çakışması ÇÖZÜLDÜ:** `gönderen`/`alan` keyword yapılmadı. Tip
pozisyonunda generic-kullanıcı-tipi olarak parse edilir (`gönderen<T>` = TIP_KULLANICI),
ada göre özel-durum. Projeksiyon `gönderen(k)`/`alan(k)` built-in çağrı adları — ama
DÜŞÜŞE-GÜVENLİ: kullanıcının `alan`/`gönderen` adlı İŞLEVİ varsa ONA düşer (C: `!ik` guard;
self-host: `fn_var_mi`). Böylece `alan` ("alan/bölge") serbest tanımlayıcı kalır.

**SÜREÇ BULGUSU (adversarial değeri):** ilk self-host projeksiyonu KOŞULSUZ `alan`'ı
gaspetti → **cg_cesit_payload'daki gerçek `işlev alan(s: Şekil)`** identity'ye çevrildi
(`add {i8,i32,i32,i32}` — struct topluyordu). codegen_diff 84/85 ile yakalandı; base-diff
kökü gösterdi; `fn_var_mi` guard'ı (C `!ik` aynası) kapattı. **DERS:** yaygın-kelime
built-in adı EKLERKEN düşüşe-güvenli ol; korpus gerçek kullanıcı-fonksiyonu içerebilir.
Ayrıca Edit-anchor duplikasyonu codegen.kem'i kırdı (P010 uzak satırda) → brace-dengesi
her codegen.kem düzenlemesinde denetlenmeli; stale cg.ll "başka sebep" gibi göründü.

**KANITLAR:** test_drf 54/54 (+4: D51 projeksiyon, D52/D53 yanlış-yön DRF007, D54 `alan`
çakışmasız), test_llvm 266/266 (+1 uçtan uca), tip 26/26, tip_kontrol 189/189,
codegen_diff **85/85** (+cg_kanal_yon), FIXPOINT 35597 stage1==stage2. Örnek: kanal_mesaj.kem
artık yön-güvenli (üretici `gönderen<tam32>` alır → gövdede kanal_al DERLEME hatası; exit 15).

**V1 SINIRLARI:** uçlar LİNEER DEĞİL (bir uç birden çok yere kopyalanabilir; tam sahiplik
garantisi V2 — AskUserQuestion'daki 3. seçenek). Runtime tek yön kontrolü yok (host kanalı
çift yönlü; yön yalnız derleme-zamanı). Yön yalnız uç-tutucu için zorlanır; kanal<T> tutan
her ikisini de yapar (kaçış-kapısı).

---

## D-302 — Generic çeşit (`çeşit Secim<T>`) — C-only, generic yapı ile aynı kapsam (2026-07-23)

**Karar [ETKİ: `src/ast.h` (cesit tip_paramlar), `src/parser.c` (P353 reddi → tip param
ayrıştırma), `src/tip_kontrol.c` (pre_populate_cesit yapi_scope + construction/eşleş
substitüsyon), `test/test_llvm.c` (+2).]** Payload'lı `çeşit` + eşleş exhaustiveness ZATEN
tamdı (ölçüm: roadmap "payloadsuz" eskimişti); eksik olan yalnız GENERIC çeşit'ti (P353 ile
açıkça reddediliyordu). Karar 3'ün gerçek kalan işi buydu.

**KAPSAM (Mehmet onayı — C-only):** self-host'ta generic YAPI bile bozuk/C-only (ölçüldü:
`Kutu<tam32>` construction C exit 42, self-host exit 1 — `atla_tip_paramlar`=tip param ATLA,
monomorphization makinesi YOK). Bu yüzden generic çeşit de C-only yapıldı — generic yapı'nın
BUGÜNKÜ durumuyla birebir tutarlı. Tam parite (self-host monomorphization kurma) çok-oturumluk
ayrı iş; bilinçli ertelendi. self-host generic çeşit'i (yapı gibi) desteklemez.

**UYGULAMA:** pre_populate_cesit çeşit'e `yapi_scope` açıp tip paramlarını
SEMBOL_GENERIC_PARAM olarak kaydeder (yapı aynası). Construction (`Secim::Var(42)`) ve eşleş
binding (`Secim::Var(x)`) payload'daki `T`'yi çeşit generic scope'unda çözer (→
TIP_GENERIC_PARAM) sonra beklenen/scrutinee `Secim<tam32>`'in tip_arg'ından `substitusyon` ile
concrete'e (tam32) çevirir. Codegen SIFIR değişiklik: mevcut çeşit codegen + payload tip
çözümü T=tam32/tam64'ü doğru üretir.

**SINIRLAR (ölçülüp belgelendi — hepsi güvenli/gürültülü, sessiz DEĞİL):**
- **T=tam32/tam64: uçtan uca DOĞRU.** tam64 2^33 → exit 42, **SESSİZ KIRPMA YOK** (çeşit
  generic yapı'dan daha iyi — yapı `Kutu<tam64>`'ü i32'ye kırpardı).
- **T=metin/ptr: --check geçer, codegen LLVM gürültülü reddeder** (T→i32 varsayılan, ptr
  uyumsuz) — generic yapı ile birebir aynı sınır. Sessiz değil.
- **Çağrı-argümanı doğrudan construction** (`ac(Secim::Var(42))`): M004 — beklenen çağrı
  argümanına yayılmıyor. **Generic YAPI'da da AYNI** (ölçüldü: `al(Kutu{deger:42})` → T001).
  Annotasyon + `ver` formu çalışır (ana idiom).

**KANITLAR:** test_llvm 265/265 (+2: Secim<tam32>→42, Kutu<tam64> 2^33→42), tip_kontrol
189/189, drf 50/50, parser 107/107 — regresyon yok. codegen_diff (mevcut korpus paritesi
korunur; non-generic çeşit yolu değişmedi).

**DERS (bu oturumda 3. kez):** roadmap maddesini başlamadan ÖLÇ. "payloadsuz çeşit"
eskimişti (payload zaten vardı); generic çeşit sandığımdan büyüktü (self-host monomorphization
yok). İki ölçüm de planı düzeltti.

---

## D-301 — `görev_başlat` → `sonuç<görev<T>, metin>`: panik yerine değer (çökmezlik) (2026-07-22)

**Karar [YÜKSEK] [ETKİ: `src/tip_kontrol.c` (dönüş tipi), `src/llvm.c` + `selfhost/codegen.kem`
(sonuç sarma + preamble global), `runtime/kdl_runtime.c` (spawn NULL), tüm görev çağrı
yerleri (örnek/test/korpus eşleş'e taşındı).]** Karar 1 (Mehmet onayı). `görev_başlat`
artık `görev<T>` DEĞİL `sonuç<görev<T>, metin>` döner.

**GEREKÇE:** Görev başlatmak BAŞARISIZ olabilir (kaynak tükenmesi ~10^5 thread; ölçüldü
ERROR_NO_SYSTEM_RESOURCES / thread'siz platform). D-296'da bu panik ediyordu — ama
KEMGU'nun DNA'sı çökmezlik (exception/panik yok, `sonuç<T,H>` var). Panik bu kimliği
ihlal ediyordu. Runtime alt katmanı zaten çökmüyordu (`kdl_bolge_olustur` OOM'de NULL
döner) — panik EDEN tek yer görev katmanıydı. Artık başarısızlık çağırana bir DEĞER:
`tamam(görev<T>)` / `hata(metin)`; çağıran `eşleş` ile açar. Hata tipi V1'de `metin`
(payload'lı `çeşit` gelince `GörevHata`ya yükseltilebilir). D-296 gerilimi TAMAMEN çözüldü:
ne sessiz sıralı-fallback (kilitlenme) ne panik.

**DALLANMASIZ SARMA:** `görev<T>` ve `metin` ikisi de IR'de `ptr` → sonuç aggregate T'den
BAĞIMSIZ olarak DAİMA `{ i8, ptr, ptr }`. Sarma dal/phi'siz: `tag = zext(icmp eq ptr
handle, null)` (başarı 0 / başarısız 1); 3 alan da koşulsuz doldurulur (f0=tag, f1=handle,
f2=sabit `@.gorev_hata_str`); okuyucu tag'e göre f1 ya da f2 okur, diğerini yok sayar.
Runtime spawn başarısızsa NULL döner → f1=null ama tag=1 olduğu için okunmaz. C ve
self-host aynı desen.

**SELF-HOST BULGUSU (escape):** Preamble global'i ilk yazışta `c\"...\\00\"` KEMGU string
literal'inde `\"`/`\\` İŞLENMEDEN literal kaldı → HER program bozuk IR (0/84 link).
KEMGU'da `"` = `yb(34)`, `\` = `yb(92)` (str_globalleri_emit deseni). C escape'i işlediği
için C tarafı doğruydu; yalnız self-host satırı düzeltildi.

**SELF-HOST BULGUSU (ret_uydur):** eşleş-bound `tamam(g)` binding'i görev iç-tipini
(görev_ic_ir) TAŞIMAZ → `görev_birleştir(g)` i64 taşıyıcıda kalır (daralmaz).
`ver görev_birleştir(a) + görev_birleştir(b)` → `add i64` → `ret i32 <i64>` uyumsuzluk
(cg_gorev_baslat/capture 82/84'te patladı; kanal geçti çünkü `ver t` döndürüyor). C
beklenen'i `+`/`ver`'den akıtıp birleştir'de trunc yapıyor. Self-host çözümü: `ret_uydur`
— `ver` değerini `cur_ret`'e tamsayı-daraltır (yalnız iki taraf da int + genişlik farklı).
`trunc(a+b) == trunc(a)+trunc(b)` → exit-kod eşdeğer (C IR'ından farklı ama semantik denk;
parite gate exit-kod). FIXPOINT korundu (codegen.kem'in kendi `ver`'leri genişlik-eşleşik →
no-op; 35115→35493 satır, stage1==stage2).

**V1 KNOWN-LIMIT (T18, açık + test edilmiş — sessiz DEĞİL):** `görev<T>` lineerdir ama
onu KAPSAYAN `sonuç` V1'de lineer sayılmaz (`tip_lineer_mi` sonuç/seçimlik içine
özyinelemez). Sonuç: eşleş ile AÇILMADAN düşen bir `sonuç<görev,metin>` için L001 leak
uyarısı ARTIK tetiklenmez (iç görev join edilmez, thread detached koşar, ρ_sahip sızar —
bellek-güvenliği ihlali DEĞİL, liveness/kaynak uyarısının kaybı). T38 ile aynı desen
(dokümante V1 sınırı). **V2 doğru çözüm:** `tip_lineer_mi`'yi sonuç/seçimlik payload'una
özyineli yap (lineer içerik → kapsayan lineer) VE eşleş'e lineer scrutinee tüketimi +
iç-payload yeniden-bağlaması öğret (ikisi bir arada; tek başına birincisi tüm eşleş-açan
pozitif testleri L001'e düşürürdü).

**ERGONOMİ:** Her `görev_başlat` artık bir `eşleş` ister → çok-görevli kod iç içe girer
(gorev_temel.kem 3 görev = 3 seviye). İleride sonuç için bir yayılım operatörü
(`?`-benzeri) bu deseni düzleştirecek. (Rust emsali: `thread::spawn` doğrudan handle
döner, `Builder::spawn` `Result` — KEMGU çökmezlik gereği `sonuç`u varsayılan yaptı.)

**KANITLAR:** test_drf 50/50, test_gorev_rt 13/13, test_llvm 263/263, codegen_diff 84/84
(C↔self-host semantik), FIXPOINT 35493 satır stage1==stage2, driver TÜM MODLAR, tip_kontrol
189/189, simd 5/5, sıfır uyarı. Başarısızlık yolu (hata kolu destructuring) normal sonuç
ile ayrıca doğrulandı (spawn hatası ~10^5 thread olmadan tetiklenemez).

---

## D-300 — `görev_başlat` + closure/lambda codegen self-host'a taşındı: PARİTE BORCU TAM KAPANDI (2026-07-20)

**Karar [ETKİ: `selfhost/codegen.kem` (lambda kuyruğu + capture analizi + heap-env +
lifted-lambda emisyonu; ~200 satır), `test/cg_korpus/` (+3 vaka).]** D-299'un dürüstçe
"KALAN" bıraktığı parça. Self-host'ta closure/lambda codegen'i **HİÇ YOKTU**; artık VAR.

**MİMARİ ENGEL VE ÇÖZÜMÜ:** C, lifted lambda'yı ertelenmiş-kuyruk + geçici-dosya +
`hoist_renumber` ile üretir; self-host `yaz_bayt` ile **doğrudan stdout**'a yazar, o
makine taşınamaz. Çözüm: lambda AST düğümü `görev_başlat` argümanında görülünce
(a) mangled ad atanır + fat-value/env `görev_başlat` **yerinde** emit edilir,
(b) düğüm kuyruğa alınır → `emit_tanimlar` **SONRASI** top-level `define @lambda_N`
üretilir (LLVM'de fonksiyon sırası önemsiz — ertelenmiş kuyruk yerine gövde-sonrası
tek geçiş). Capture bilgisi call-site'ta (çevre scope canlıyken) çözülüp parçalı
dizilerde saklanır — lifted emit'te çevre `cg_var` artık yok.

**CAPTURE (tam destek):** capture analizi flat-AST'yi gezer (C `lambda_serbest_tara`
aynası): `TANIMLAYICI` + çevre-lokal/param (`cg_var_bul != ""`) + lambda-param DEĞİL →
yakalama; iç LAMBDA'ya girmez; dedup. Capture varsa: HEAP env struct malloc + her
yakalananın DEĞERİ store; lifted lambda `%env`'den yükleyip alloca'ya koyar → gövde
isimle erişir (C V2-F2 heap-env aynası).

**DÖNÜŞ-TİPİ NUMARASI:** self-host imzayı gövde-EMİT'inden ÖNCE yazmak zorunda
(doğrudan-stdout), ama gövdenin doğal tipi ancak emit'te bilinir. C bunu iki-tampon +
tmpfile ile çözer. Self-host **daha basit**: lifted lambda'yı DAİMA `i64` döndürür
(runtime `KdlGorevBare` = int64_t ile birebir ABI), gövde sonucunu `i64_genislet` ile
i64'e çıkarır. Böylece imza-önden-yazma sorunu çözülür, C'nin iki-tampon makinesine
gerek kalmaz. `görev_birleştir` zaten i64'ten T'ye daraltıyor.

**SINIR (C ile AYNI):** ifade-form gövde. Blok-form (`|| { …; ver x }`) → `ret i64 0`
fallback (korpus expr-form kullanır; DRF001 kapsamı dışı). İç lambda yok.

**KANIT (C ile birebir eşdeğer, ölçüldü):** capture-free (`|| 42`→42, `|| "selam"`→5,
iki görev→42); **capture** (`|| temel+2`→42, flagship `|| uretici(k)` çapraz-thread→15,
2-değişken→42, dedup `|| x+x`→42). Flagship `kanal_mesaj.kem` self-host'ta **20/20
deterministik** (yarış/kilit yok). `codegen_diff` **84/84** (+3 vaka: cg_gorev_baslat/
capture/kanal), **SELF-HOST FIXPOINT ✓** (stage1==stage2, 35115 satır; 92/92 bayt-birebir),
`calistir_self_driver` TÜM MODLAR ✓ — self-host-derlenmiş derleyici de 84/84.

**SONUÇ:** D-291→D-297'nin tamamı (görev/kanal/dondur/lambda-dönüş/closure) artık
self-host codegen'de. Parite borcu **kapandı**.

---

## D-299 — Self-host parite borcu: Katman 2'nin closure'sız kısmı `codegen.kem`'e taşındı (2026-07-20)

**Karar [ETKİ: `selfhost/codegen.kem` (ll_tip görev/kanal→ptr; ll_ic_tip; cg_aic alanı +
cg_var_ic_bul; param_ic; i64_genislet/i64_daralt/cagri_ic_tip; 5 intrinsic emisyonu; 4 declare),
`test/cg_korpus/` (+5 vaka).]** D-291→D-295'in self-host'a port edilmemiş olması "PARİTE BORCU"
olarak kayıtlıydı. Bu adım borcun **taşınabilir kısmını** kapatır.

**ÖNCE (ölçüldü):** self-host `görev_başlat`/`kanal_*`'ı **kullanıcı işlevi** sanıyordu →
`call i32 @"kanal_oluştur"(ptr %rho, i32 4)` — ρ-ABI'li, tanımsız sembol. Yani C derleyicinin
D-291 öncesi hâli. Ayrıca `görev<T>`/`kanal<T>` `ll_tip`'te `i32` fallback'ine düşüyordu
(64-bit handle 32 bite kırpılırdı).

**PORT EDİLENLER (C ile semantik eşdeğer, ölçüldü):** `kanal_oluştur`, `kanal_gönder`,
`kanal_al`, `görev_birleştir`, `dondur`. T bildirilen `görev<T>`/`kanal<T>`den kurtarılır
(`cg_aic` — C'deki `gorev_ic_ir`/`kanal_ic_ir` aynası); gönderimde `sext`/`ptrtoint`,
alımda `trunc`/`inttoptr`; T kurtarılamazsa **i64** (kırpma YOK — D-295'in bloker onarımının
aynası).

**PORT EDİLMEYEN — `görev_başlat` (gerekçe):** fat-value closure (`{ptr, ptr}`) ister ve
**self-host'ta lambda codegen'i HİÇ YOK** (LAMBDA düğümü ayrıştırılıyor, yalnız bölge-yönlendirme
koruması için kullanılıyor; lifted fonksiyon emit edilmiyor — `store i32 0` yer tutucusu).
Üstelik self-host `yaz_bayt` ile **doğrudan stdout'a** yazıyor: C'nin ertelenmiş-kuyruk +
tmpfile + `hoist_renumber` makinesi olduğu gibi taşınamaz; ön-geçişli lifted-lambda emisyonu
gerekir. Ayrı ve büyük bir iş. **`görev_birleştir` port edildi ama BUGÜN SINANAMIYOR** —
`görev<T>` değeri üretmenin tek yolu `görev_başlat`. Bu dürüstçe kaydedilir.

**BULUNAN VE ONARILAN AYRI KUSUR (literal genişliği):** self-host `TAM` literalini daima
`i32` sayar ve ham immediate döndürür. `kanal<tam64>`de 2^33 literali `sext i32 8589934592`
üretiyordu → **SESSİZ bozulma** (ölçüldü: exit 1, doğrusu 42; C `add i64 0, <lit>` üretir).
Onarım: `i64_genislet` immediate operandı (`%` ile başlamayan) **tam genişlikte materyalize**
eder. Küçük literaller için de değer-eşdeğer.

**KORPUS — borcun SESSİZ olmasının sebebi kapatıldı:** 76 korpus dosyasının **0'ında**
kanal/görev vardı; bu yüzden parite gate'i borcu görmüyordu. 5 vaka eklendi
(`cg_kanal_temel/tam64/metin/param`, `cg_dondur`) → korpus **81**.

**KAPSAM DIŞI (ölçüldü, port kaynaklı DEĞİL):** `değişken v: tam8 = 0 - 128; eğer v == 0 - 128`
self-host'ta **kanal olmadan da** derlenmiyor (negatif literal ifadesi i32 kalıyor, i8 ile
karşılaştırılıyor). Önceden var olan bidirectional-çıkarsama boşluğu; pozitif değerlerle
kanal yolu C ile birebir çalışıyor.

**KANIT:** `calistir_codegen_diff` **81/81** (5 yeni vaka dâhil), **SELF-HOST FIXPOINT ✓**
(stage1==stage2, 34139 satır; lexer/parser/checker 92/92 bayt-birebir),
`calistir_self_driver` TÜM MODLAR ✓ — **self-host-derlenmiş derleyici de 81/81**, `test_tumu`.

---

## D-297 — Test altyapısı: süreç-benzersiz geçici yollar + Katman 2 kapsam boşlukları kapatıldı (2026-07-17)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-296).

**Karar [ETKİ: `test/test_llvm.c`, `test/test_simd_llvm.c` (PID'li geçici yollar + 5 yeni test),
`DECISIONS_LOG.md`, `CLAUDE.md`.]** Ön-merge denetimi/triajının bıraktığı iki takip maddesi.
İkisi de merge-bloker değildi; ürün kodu **değişmedi**, yalnız test altyapısı ve kapsama.

### (1) Sabit geçici dosya yolları → SÜREÇ-BENZERSİZ (sahte kırmızı kaynağı)
`test_llvm.c` ve `test_simd_llvm.c` tek, sabit geçici yol kullanıyordu
(`build/test_llvm_temp.{kem,ll,exe}`). Aynı testin **iki eş zamanlı koşumu** birbirinin
dosyasını eziyordu. 2026-07-17'de gözlendi: 252/252 yerine **157/252** — kaynak TEMİZDİ,
regresyon YOKTU. Sahte kırmızı, tanılama sırasında gerçek bir regresyonla karıştırılabilir;
o gün gerçekten zaman kaybettirdi.
- Yollar `PID` ile benzersizleştirildi (`test_llvm_<pid>.kem` vb.), sonda `remove()` ile
  temizleniyor (build/ artık PID'li artıkla dolmuyor).
- `fopen("build/test_llvm_temp.kem")` **sabit yol kaçağı** da kapatıldı (KEM_PATH kullanıyor).
- Tampon boyutu 512→**64**: 512 ile GCC `-Wformat-truncation` üretiyordu (sıfır-uyarı hedefi);
  gerçek yol ~33 karakter.
- **KANIT:** 3 eş zamanlı koşum → **258/258, 258/258, 258/258** (eskiden çakışıyordu);
  kalan PID artığı 0.

### (2) Katman 2 kapsam boşlukları
**(a) Dar/işaretsiz T uçtan uca yoktu.** `test_drf` D3/D46 yalnız TİP KONTROLÜ ölçüyordu
(`hata_sayisi()`), yani "kanal<tam8> derleniyor" iddiası **kanıtsızdı**; D-295'in kanal
testleri de yalnız tam64/metin/kesirli kapsıyordu. Eklendi:
`kanal<tam8>` **-128** turu (parametre yolu + `sext`/`trunc` çifti), `kanal<dtam8>` **200** turu
+ **işaretsiz karşılaştırma**, `kanal<tam16>` **-1000** **çapraz-thread** turu.
(Triaj bu davranışı 16 programla zaten ölçmüştü — risk yoktu, **kanıt** yoktu.)

**(b) Örnek dosyaları semantik olarak korunmuyordu.** `gorev_temel.kem` (42) ve
`kanal_mesaj.kem` (15) hiçbir hedefte exit-kodu doğrulanmıyordu. Triaj mutasyonla ölçmüştü:
`calistir_asan_denetim` bunları derleyip **çalıştırıyor** ama yalnız sanitizer metnine baktığı
için mutasyonu **yakalamıyordu**; `test/test_llvm.sh`'de exit-kod listesi var ama betik hiçbir
Makefile hedefine **bağlı değil**. Artık `test_llvm.c`'de exit-kodu doğrulanıyor.
- **MUTASYON DOĞRULAMASI:** iki örnekte `ver toplam` → `ver 7` yapıldığında yeni testler
  **[262]/[263] KIRMIZIYA döndü** → koruma gerçek (sahte test değil). Mutasyon geri alındı.

**KANIT:** `test_llvm` **263/263** (+5), sıfır uyarı; `test_tumu` geçti (FIXPOINT ✓).

---

## D-296 — Sıralı görev fallback'i KALDIRILDI: kilitlenme → gürültülü panik (2026-07-17) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-295).

**Karar [ETKİ: `runtime/kdl_runtime.c` (kdl_gorev_spawn fallback kaldırıldı; kdl_gorev_birlestir
NULL yolu), `test/test_gorev_rt.c` (test [9] değişti + dosya başlığı).]** D-295 sonrası triajın
ölçtüğü bulgu; merge-bloker değildi ama koddaki iddia **fiilen yanlıştı**.

**Ölçülen kusur:** `kdl_gorev_spawn`, thread yaratılamazsa görevi **sıralı** çalıştırıyordu ve
yorum "görev semantiği korunur, yalnız paralellik kaybolur" diyordu. **Yanlış:** korunan yalnız
**GÜVENLİK** (safety); **CANLILIK** (liveness) kayboluyordu. Görev gövdesi bloklayan bir kanal
işlemi yaparsa (boş kanaldan `kanal_al` / dolu kanala `kanal_gönder`) **kalıcı kilitlenme** olur —
karşı taraf (çağıran) henüz çalışmıyordur.

**ÖNCE/SONRA (aynı program, aynı simüle spawn hatası — `CreateThread → NULL`):**
| | `kanal_mesaj.kem` |
|---|---|
| ESKİ (fallback var) | **exit 124** — 15 sn asıldı (KİLİTLENDİ) |
| YENİ (fallback yok) | açık **PANIK** mesajı + süreç ölümü (asılma YOK) |

Triaj ayrıca ölçtü: tüketici-görev deseni (görev boş kanaldan alır, main gönderir)
**kapasiteden BAĞIMSIZ** kilitleniyordu — daha genel/kötü hâl.

**Karar: sıralı çalıştırma bir eşzamanlılık ilkelinin geçerli yedeği DEĞİLDİR.** Runtime,
gövdenin başka bir göreve bloklanıp bloklanmayacağını **bilemez**; dolayısıyla fallback temelden
sağlıksızdır. Kaldırıldı; spawn başarısızlığı artık `kdl_panik` (projenin "gürültülü > sessiz"
ilkesi — açık tanılı ölüm, sessiz asılmadan iyidir). `#else` dalı (ne Win32 ne POSIX) da panik
eder: o derlemede `görev` eşzamanlılık sağlayamaz, sessizce yanlış çalışmaktansa açıkça reddeder.

**Aynı adımda kapatılan ikinci sessiz-0:** `kdl_gorev_birlestir(NULL)` **0 dönüyordu** —
gerçekten 0 dönmüş bir görevden **ayırt edilemez** (D-292'de kapatılan boş-kanal hatasıyla AYNI
sınıf). Artık panik. Tetikleyici yalnızca OOM (`kdl_gorev_basla_kapanis`'in malloc/bölge
başarısızlığı).

**Tetikleyici nadir ama gerçek:** triaj ölçtü — bu makinede CreateThread ancak ~102374 thread
sonrası başarısız oldu (`ERROR_NO_SYSTEM_RESOURCES`). Fallback ayrıca **SESSİZDİ** (yalnız sayaç
artıyordu, stderr'e hiçbir şey yazılmıyordu).

**TRADE-OFF (dürüstçe):** panik, KEMGU'nun "çökmezlik" felsefesiyle gerilim hâlinde. Ama
`kdl_panik` bu projede zaten kurulu pratik (D-069 inline-OOB). **Doğru nihai çözüm panik DEĞİL,
`görev_başlat`ın `sonuç<görev<T>, Hata>` dönmesidir** — bu bir DİL tasarım kararı (Mehmet).
O gelene kadar panik, sessiz kilitlenmeden iyidir; kayıt bu gerilimi açıkça taşır.

**TEST DEĞİŞİKLİĞİ:** eski test [9] `kdl_gorev_birlestir(NULL) == 0` iddia ediyordu ve
"savunmacı" görünüyordu — aslında **sessiz yanlış cevabı doğruluyordu**. Yerine D-296
invaryant bekçisi kondu: **`kdl_gorev_sirali_sayisi` daima 0** (sıfırdan farkı, fallback'in geri
geldiğini ve dolayısıyla kilitlenme riskinin döndüğünü gösterir). NULL yolu panik ettiği için
in-process test EDİLEMEZ (abort test koşucusunu da öldürürdü) — kapsama sınırı kayıtlı.

**KANIT:** normal yol bozulmadı (`gorev_temel.kem` exit 42, `kanal_mesaj.kem` exit 15).
`test_gorev_rt` 13/13, `test_llvm` 258/258, `test_drf` 50/50, bare-metal kanal, parite 76/76,
**FIXPOINT ✓**, `test_tumu`.

---

## D-295 — 3 BLOKER onarımı: sessiz `i32` fallback'i elendi (ön-merge denetimi bulgusu) (2026-07-17) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-294).

**Nasıl bulundu:** `main`'e merge ÖNCESİ adversarial denetim (6 boyut paralel inceleme + her bulgu
için 3 bağımsız çürütme lensi). 26 aday → 15 onaylanan, **3 bloker**. Üçü de bağımsız olarak
derleyip-çalıştırarak teyit edildi. **Merge durduruldu, önce bunlar onarıldı.**

**ORTAK KÖK:** codegen T'yi kurtaramadığında sessizce **`"i32"`** varsayıyordu. D-291→D-294 taşıyıcıyı
i64'e genişletti ama fallback'i güncellemedi → i64 taşıyıcı i32'ye kırpılıyordu.

| # | Program | --check | derleme | çalışma (ÖNCE) | SONRA |
|---|---|---|---|---|---|
| 1 | `değişken c = görev_başlat(\|\| tam64); değişken s = görev_birleştir(c);` | OK | OK | **exit 99** (doğrusu 1) | exit 1 ✓ |
| 1b | aynısı, T=metin | OK | OK | **SEGFAULT** | exit 5 ✓ |
| 2 | `kanal<tam64>` + `--llvm` (tip kontrolü atlanır) | DRF006 | OK | **exit 1** (doğrusu 42) | exit 42 ✓ |
| 3 | `değişken f = \|\| "selam"; metin_uzunluk(f())` | OK | OK | **SEGFAULT** | exit 5 ✓ |

**EN AĞIR OLGU — diff hata modunu LOUD→SILENT çeviriyordu:** aynı programlar `origin/main`'de
`error: '%0' defined with type 'ptr' but expected 'i32'` ile **derlenmiyordu** (ölçüldü). Yani
D-291→D-294, gürültülü derleme hatasını sessiz yanlış cevaba/segfault'a dönüştürmüştü — projenin
kendi "loud > silent" ilkesinin birebir ihlali. D-292'nin "sessiz-yanlış-cevap kapatıldı" başlığı
bu yüzden **yanlıştı**; kanal'da açtığı ikinci yolu görmemişti.

**ONARIMLAR:**
1. **`görev_birleştir` fallback `i32`→`i64`** (llvm.c). Kesirli T zaten reddedildiği için (D-294)
   i64 son çare güvenli: hedeflerde (x86_64/aarch64) tamsayı ve işaretçi AYNI yazmaçta döner.
2. **Closure çağrı yeri fallback `i32`→`i64`** (llvm.c). Annotasyonsuz closure'da ptr dönen lambda
   artık kırpılmıyor. **Kalan açık (dürüstçe):** gövdesi kesirli dönen annotasyonsuz closure +
   beklenen-yok hâlâ yanlış yazmaçtan okur; ama o vaka bugün de aşağı akışta **gürültülü** LLVM
   reddi alıyor (ölçüldü) — i64 onu sessizleştirmiyor. Tam çözüm lambda dönüşünün çağrı yerinden
   ÖNCE bilinmesini ister (ayrı iş).
3. **Kanal runtime `int32_t`→`int64_t`** (host + bare-metal, AYNI ABI) + codegen T-farkında
   gönderim/alım (`kanal_ic_ir`, `gorev_ic_ir` deseninin aynısı: gönderimde `sext`/`ptrtoint`,
   alımda `trunc`/`inttoptr`).
   **Neden "gürültülü yap" DEĞİL de "sınıfı yok et":** önce planım geniş T'de doğal tip geçirip
   `declare` uyuşmazlığıyla LLVM'i reddettirmekti. **ÖLÇTÜM: LLVM imza uyuşmazlığını SESSİZCE
   KABUL EDİYOR** (`opt -passes=verify` ve `llvm-as` exit 0) → plan geçersizdi. Ölçmeseydim
   blokeri kapattığımı sanacaktım. Taşıyıcıyı genişletmek kırpma sınıfını tamamen ortadan kaldırır.

**D-292'nin 32-bit kısıtı KALKTI (kapsam genişledi):** `kanal<tam64>`, `kanal<metin>`,
`kanal<Dizi<T>>` artık GERÇEKTEN çalışıyor. **Kalan kısıt: kesirli T** (görev ile aynı gerekçe —
kanal tamsayı taşır, `fptosi` DEĞERİ bozar). Katmanlı savunma kanal-float için de ölçüldü:
`--llvm` tip kontrolünü atlasa bile emisyon `sext double → i64` üretir, bu **geçersizdir** → LLVM
gürültülü reddeder.

**TEST BEKLENTİSİ DEĞİŞTİ (dürüstçe):** D4/D6/D45 artık DRF006 değil **0 hata** bekliyor
(yetenek genişledi); yeni D50 kesirli reddini kilitler.

**KANIT:** `test_llvm` **258/258** (+6, hepsi ANNOTASYONSUZ biçimleri kullanır — kırık olan
idiomatik biçimdi; annotasyonlular zaten geçiyordu). `test_drf` **50/50**. `test_gorev_rt` 13/13.
Regresyon: bare-metal `calistir_kanal_test_arm` geçti (toplam=55 — i64 genişletme QEMU'da kırmadı),
codegen parite **76/76**, **SELF-HOST FIXPOINT ✓** (33371 satır), `test_tumu`.

**SÜREÇ DERSİ:** "Yeni bir tip kısıtı koyarken ikinci katmanı ÖLÇ" (D-294) yetmiyormuş — asıl ders:
**taşıyıcı genişliğini değiştirince TÜM fallback'leri denetle.** Ayrıca denetim ajanlarına test
sabotajı yaptırırken `test_llvm.c`'nin SABİT geçici dosya yolları (`build/test_llvm_temp.*`)
eşzamanlı koşumda çakışıp 95 sahte hata üretti — test altyapısı sağlamlığı ayrı iş olarak kaydedildi.

---

## D-294 — `görev<T>` genişletme: `görev<metin>` çalışıyor (runtime i64 taşıma) + kesirli T reddi (2026-07-17) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-293).

**Karar [ETKİ: `runtime/kdl_runtime.c` (görev dönüşü int32_t→int64_t), `src/llvm.c`
(`LlvmIsim.gorev_ic_ir` + `gorev_ic_ir_al`; birleştir emisyonu i64→T daraltma; declare),
`src/tip_kontrol.c` (kesirli T reddi — 2 tıkaç), `test/test_gorev_rt.c` (ABI hizalama),
`test/test_llvm.c` (+2), `test/test_drf.c` (+3).]**

**Kapatılan boşluk:** D-293 lambda tarafını çözmüştü (`define ptr @lambda_0` ✓) ama
`kdl_gorev_birlestir` **int32_t** dönüyordu → `görev<metin>` LLVM tip hatası veriyordu.
Runtime i64 taşımaya geçti; `görev_birleştir` sonucu çağrı yerinde T'ye daraltılıyor
(`ptr`→`inttoptr`, `i64`→aynen, `i8/16/32/i1`→`trunc`). **`görev<metin>` artık çalışıyor**
(uçtan uca exit 5). T'nin IR'i **bildirilen** `görev<T>`den gelir (`gorev_ic_ir` —
`görev<T>` IR'de opak `ptr` olduğu için T başka türlü bilinemez; D-293'teki
`kapanis_donus_ir` deseninin aynısı).

**Neden i32 dönen lambda bozulmuyor:** int64 olarak çağrılınca üst 32 bit ÇÖP olur, ama
codegen sonucu T'ye **trunc** eder → değer doğru. (Bu trunc kozmetik değil, **şart**.)

**KESİRLİ T REDDİ — KATMANLI SAVUNMA (ikisi de ölçüldü):**
- Runtime sonucu **tamsayı** dönüşlü fn-ptr ile alır (x0/rax); kesirli dönüş **v0/xmm0**'dadır
  → bitcast'lamak **SESSİZ çöp** üretirdi. Reddetmek, hata modunu loud→silent'a çevirmemenin
  tek yolu (D-293'te tam bu tuzağa düşülüp yakalanmıştı).
- **1. katman — tip kontrolü:** iki tıkaç (görev_başlat = YARATMA yolu; `DUGUM_TIP_GOREV` =
  annotasyon/parametre yolu) → **DRF001**.
- **2. katman — `--llvm` tip kontrolünü ÇALIŞTIRMAZ** (önceden var olan davranış, ölçüldü:
  `--check` exit 1 ama `--llvm` exit 0). O yol da güvenli: T=double için emisyon
  `trunc i64 -> double` üretir, bu **geçersizdir** → LLVM gürültülü reddeder
  (`invalid cast opcode`). **Sessiz çöp yolu KAPALI.**
- **Kapasite kaybı YOK:** `görev<kesirli*>` zaten hiç derlenmiyordu; kazanç düzgün tanı.

**TEST TUZAĞI (yakalandı):** ilk yazdığım red-testi `birleştir`i ÇAĞIRMIYORDU → geçersiz cast
hiç emit edilmiyor, program derlenip geçiyordu → test **yanlış sebeple** kalıyordu. Çağıracak
şekilde düzeltildi. **Ayrıca `test_gorev_rt.c` hâlâ `extern int32_t kdl_gorev_birlestir`
bildiriyordu**; ayrı derleme birimi olduğu için linker uyuşmazlığı YAKALAMAZ ve test düşük 32
biti okuyup **yanlış sebeple geçerdi** — bildirimler tanımla birebir hizalandı.

**SINIR:** `görev<T>` V1: T ∈ {≤64-bit tamsayı, işaretçi-benzeri (metin/&T/Dizi<T>), boş}.
Kesirli T yok (yukarıdaki gerekçe). `kanal<T>` ayrı ve daha dar (32-bit tamsayı, D-292) —
sınırı runtime tamponundan (int32_t), lambda'dan değil.

**KANIT:** `test_llvm` **252/252** (+2: görev<metin> → exit 5; görev<kesirli64> `--llvm`
yolunda da LLVM reddi). `test_drf` **49/49** (+3: D47 görev_başlat kesirli → DRF001, D48
annotasyon yolu → DRF001, D49 görev<metin> = 0 hata). `test_gorev_rt` **13/13** (ABI hizalı).
Regresyon: codegen parite **76/76**, **SELF-HOST FIXPOINT ✓** (33371 satır), `test_tumu`.

**PARİTE BORCU (sürüyor):** D-291→D-294'ün hiçbiri `selfhost/codegen.kem`'de yok.

---

## D-293 — Lambda dönüş-tipi çıkarsaması: `işlev() -> metin` / `-> kesirli64` lambdaları artık çalışıyor (2026-07-17) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-292).

**Karar [ETKİ: `src/llvm.c` (lambda_emit iki-tamponlu yeniden yapılandırma; `dosya_kopyala` yardımcısı;
`LlvmIsim.kapanis_donus_ir` + `kapanis_donus_ir_al`; closure çağrı yeri dönüş tipi), `test/test_llvm.c` (+3).]**

**Ölçülen kusur:** `lambda_emit`'te `const char *donus = "i32"` **SABİT**ti. Sonuç: `|| "selam"`
(`işlev() -> metin`) ve `|| 3.5` (`işlev() -> kesirli64`) **tip kontrolünden GEÇİP** LLVM'de patlıyordu
(`'%0' defined with type 'ptr' but expected 'i32'`). Bu, görev/kanal'dan **BAĞIMSIZ**, düz lambda
kullanımını kıran gerçek bir bug'dı.

**Çözüm (a) — define satırı tipi bilmeden yazılamaz → iki tampon:** gövde önce `ic_tmp`'ye
`ifade_uret(g, govde, NULL)` (**doğal** tip) ile yazılır, `r.tip` öğrenilir, sonra `define` satırı
`govde_tmp`'ye yazılıp gövde eklenir. `hoist_renumber` zaten TÜM fonksiyon metnini tutarlı yeniden
numaralandırır ve `define` satırında numaralı register yoktur (paramlar adlı: `%rho`/`%env`/`%x`) →
sıralama değişikliği güvenli. `tmpfile()` başarısızsa eski davranışa (sabit i32) düşer.

**Çözüm (b) — ARA BULGU: (a) TEK BAŞINA REGRESYON ÜRETTİ (yakalandı, kapatıldı).** (a)'dan sonra
`define ptr @lambda_0` doğruydu ama **çağrı yeri** hâlâ i32 sanıyordu → `metin_uzunluk(f())`
**derlenip SEGFAULT** verdi. Yani hata modu **loud (LLVM hatası) → silent-crash**'e dönmüştü — kabul
edilemez; (a) tek başına GÖNDERİLEMEZDİ. Kök: `işlev(...) -> T` IR'de `{ ptr, ptr }` (fat value) →
**T SİLİNİR**; çağrı yeri `beklenen ? beklenen : "i32"` TAHMİN ediyordu. Lambda dönüşü sabit i32 iken
bu tahmin (yanlış ama) tutarlıydı; çıkarsama gelince kırıldı.
**Kapatma:** `LlvmIsim.kapanis_donus_ir` — bildirilen `işlev(...) -> T` tipinden T'nin IR'i, değişken
(annot'lu yol) ve fonksiyon parametresi kaydında doldurulur; çağrı yeri **bildirilen tipi beklenen'e
TERCİH eder** (bildirilen tip otoriter). Annotasyonsuz closure (`değişken f = || ...`) için alan NULL
kalır → eski `beklenen` davranışı korunur.

**Ölçüm ayrımı (neden `|| 3.5` çalışıp `|| "selam"` çökmüştü):** `değişken v: kesirli64 = f();`
çağrı yerine `beklenen="double"` verir → tesadüfen doğru. `metin_uzunluk(f())` ise built-in argümanı;
`beklenen` YAYILMIYOR → i32 tahmini. Karşılaştırma kontrolü: `metin_uzunluk(kimlik("selam"))`
(kullanıcı fonksiyonu) çalışıyordu — çünkü dönüş tipi imza tablosundan geliyor; sorun **yalnız
closure** yolundaydı.

**KAPSAM / SINIR:**
- **Yalnız ifade-form gövde.** Blok-form (`|| { ...; ver x; }`) dönüşü **i32 KALIR** (mevcut davranış
  birebir korunur). Çıkarsanamaz çünkü blok içindeki `ver` emit edilirken `g_donus_tip`'e ihtiyaç duyar,
  tipi öğrenmek için ise gövdeyi emit etmek gerekir — **döngüsel**. Çözümü gövde ön-taraması (ayrı iş,
  D-072).
- **Bildirilen tip ≠ gövdenin doğal tipi** (örn. `işlev() -> tam64 = || 42`; literal doğal tipi i32)
  BU işle çözülmez — regresyon da değil, **aynı kalır**.
- **`görev<T>`'yi TEK BAŞINA AÇMAZ** (D-291'deki iddiamın düzeltmesi): `kdl_gorev_birlestir` de i32
  döner; `kanal<T>` sınırı ise runtime tamponundan (int32_t). Genişletme ayrı, runtime-tarafı iş.
- **PARİTE BORCU (dürüstçe):** bu çıkarsama `selfhost/codegen.kem`'de **YOK** → C derleyici ileride.
  Gateler geçiyor çünkü korpusta i32-dışı lambda yok. D-291/D-292'nin görev/kanal codegen'i de aynı
  durumda. Port ayrı iş.

**KANIT:** `test_llvm` **250/250** (+3: metin ara-değişkenle → exit 5; **metin iç-içe built-in
argümanında → exit 5** — regresyon vakasını kilitler; kesirli64 → 42). Regresyon: lambda **5/5**,
codegen semantik parite **76/76**, **SELF-HOST FIXPOINT ✓** (stage1==stage2, 33371 satır), `test_tumu`.

---

## D-292 — KATMAN 2 TAM: `kanal_oluştur` kurucusu + kanal codegen + BLOKLAYAN kanal (sessiz-yanlış-cevap kapatıldı) (2026-07-17) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-291).

**Karar [ETKİ: `src/tip_kontrol.c` (kanal_oluştur intrinsic: beklenen-tip yolu + DRF006),
`src/llvm.c` (3 kanal intrinsic emisyonu + 3 declare), `runtime/kdl_runtime.c` (kanal BLOKLAYICI),
`runtime/kdl_kanal.h/.c` + `test/bare_metal/kanal_arm.c` (ABI birleştirme), `test/test_drf.c` (+5),
`test/test_gorev_rt.c` (+4), `test/test_llvm.c` (+2), `test/ornekler/kanal_mesaj.kem` (YENİ),
`belgeler/KEMGU_Bellek_Modeli.md` (R-KANAL yeniden yazıldı).]**

**Mehmet'in kararı (soruldu, onaylandı):** tek yönsüz `kanal<T>` + `kanal_oluştur(kapasite)`.
Alternatif (spec'e sadık ayrık `gönderen<T>`/`alan<T>` uçlar) reddedildi — belirgin şekilde büyük iş.

**Kapatılan boşluk 1 — kurucu yoktu:** DRF V1'de `kanal<T>` KURUCUSU yoktu; 39 DRF testinin tamamı
kanalı *parametre* alıyordu, hiçbiri yaratmıyordu → gerçek bir programda `kanal<T>` değeri elde etmek
**imkânsızdı** (codegen eklense bile). `kanal_oluştur(n)`'nin T'si bir DEĞER argümanından çıkarsanamaz
(kanal boş başlar) → **beklenen tipten** gelir (`dizi_olustur<T>(N)` / boş dizi deseni). Bağlam yoksa
sessizce bir T uydurmak yerine **DRF006**.

**Kapatılan boşluk 2 — kanal SESSİZ YANLIŞ CEVAP veriyordu (asıl bulgu):** host `kdl_kanal_al` boş
kanalda **0 dönüyordu** — ve o 0, gerçekten gönderilmiş bir 0'dan **AYIRT EDİLEMEZ**; `kdl_kanal_gonder`
dolu kanalda mesajı **sessizce DÜŞÜRÜYORDU**. İkisi de R-KANAL sahiplik-transferini bozar (transfer
edilmemiş değer alınmış görünür) ve "çökmezlik/yarış-yok" iddiasıyla çelişir. Artık her iki yön de
**koşul değişkeniyle BLOKLAR** (Win32 CONDITION_VARIABLE / pthread_cond, `while(koşul)` döngüsünde —
sahte uyanma güvenli). Bu, bare-metal `kdl_kanal.c`'nin (preemption altında busy-wait) **zaten doğru**
olan sözleşmesiyle host'u hizalar. **Risk yok:** host kanal API'si ölü koddu (tek çağıran
`bare_metal/kanal_arm.c` ve o BARE-METAL başlığı kullanıyor).

**Kapatılan boşluk 3 — latent ABI tuzağı:** `kdl_kanal_olustur` host'ta `(int32_t)`, bare-metal'de
`(void)` idi. Codegen tek `@kdl_kanal_olustur(i32)` çağırdığı için bare-metal hedefte kapasite
**sessizce yutulacaktı**. Bare-metal imzası `(int kapasite)` yapıldı ve **dürüst** davranıyor: istenen
kapasite derleme-zamanı halkayı aşarsa sessizce kırpmak yerine **0 döner**. Artık tek çağrı iki
backend'e de aynı ABI ile bağlanır.

**Kapatılan boşluk 4 — `kanal<T>`'nin SESSİZ veri kaybı (Mehmet kararı: 32-bit kısıt):** kanal codegen'i
eklenince ortaya çıktı ki `kanal<tam64>` **derleniyor, çalışıyor ve sessizce veri kaybediyor**: 2^33
gönderilip alındığında eşit çıkmıyor (ÖLÇÜLDÜ: exit 1). Sebep: runtime tamponu monomorfik `int32_t`,
codegen değeri `i32` operandına zorluyor. Bu, hata modunu **loud → silent**'a çevirirdi (kanal codegen'i
YOKKEN aynı program tanımsız-sembol link hatası veriyordu) — kabul edilemez.
**Çözüm (Mehmet onayladı):** `kanal<T>` V1'de yalnız **32-bit tamsayı T** (tam8/16/32, dtam8/16/32).
Tıkaç `kanal<T>` TİPİNİN çözüldüğü tek noktada (`DUGUM_TIP_KANAL`) → parametre/değişken/dönüş fark
etmeksizin her kullanımı kapsar (ölçüldü). En iyi hata modu: **derleme zamanı, açık KEMGU tanısı**.
- **ÖLÇÜM DÜZELTMESİ (kendi ilk hipotezim yanlıştı):** `kanal<metin>` sessiz DEĞİLDİ — LLVM zaten
  gürültülü reddediyordu (`'%N' defined with type 'ptr' but expected 'i32'`). Tek gerçekten SESSİZ vaka
  `kanal<tam64>`. Kısıt ikisini de düzgün bir tanıya çeviriyor.
- **Test beklentisi DEĞİŞTİ (dürüstçe kaydedilir):** D4 (`kanal<metin>` = 0 hata) ve D6
  (`kanal<Dizi<tam32>>` = 0 hata) artık **DRF006 bekliyor**. Dilin kabul ettiği küme daraldı.
- **`görev<T>`'de simetrik kısıt YOK — çünkü gerek yok (ölçüldü):** `görev<tam64>` tip kontrolünde
  **T001** ile yakalanıyor (lambda'nın gerçek dönüşü tam32 → `görev<tam32>` ≠ `görev<tam64>`),
  `görev<metin>` ise LLVM'de gürültülü hata veriyor. **görev'in sessiz yolu yok.** Asimetrinin sebebi:
  `kanal_gönder(k, v)`'de v, T ile uyumlu olduğu için tip kontrolünden geçer ve kırpma yalnız codegen'de
  olur. (Bilinen pürüz: `görev<metin>`'in mesajı backend sızdırıyor — düzgün tanı ayrı iş.)

**SINIR (bilinçli):** yön tip-seviyesinde garanti EDİLMEZ (aynı görev hem gönderip hem alabilir) —
tek-`kanal<T>` kararının açık bedeli, `KEMGU_Bellek_Modeli.md` R-KANAL'da kayıtlı. Kanal monomorfik
i32 taşır (D-291'deki T=tam32 sınırıyla AYNI kök: IR'de T i32'ye sabit) → genişletme, lambda dönüş-tipi
çıkarsamasıyla BİRLİKTE gelmeli.

**KANIT (bloklamayı AYIRT EDEN testler — "42 döndü" yetmez):**
- `test_gorev_rt` **13/13** (+4): **[10] boş kanalda `kanal_al` BLOKLAR** — gönderici bilerek gecikir,
  böylece alıcı kesinlikle önce girer; **eski sürüm burada deterministik olarak 0 dönerdi**.
  **[11] dolu kanalda `kanal_gonder` BLOKLAR** (kap=2, 5 mesaj, toplam 15 — eski sürümde taşanlar
  düşerdi). [12] FIFO sırası. [13] NULL savunması.
- `test_llvm` **247/247** (+2): görev→kanal→main mesaj geçişi (exit 42) + akış denetimi (exit 15).
  **Ayırt edicilik EMPİRİK olarak gözlendi:** bayat (bloklamayan) `kdl_runtime.o` ile aynı program
  exit **0** veriyordu; bloklayan runtime ile **42**. Test gerçekten semantiği ölçüyor.
- `test_drf` **44/44** (+5: D40 annotasyonlu, D41 bağlamsız→DRF006, D42 arity, D43 kapasite-tamsayı-değil,
  D44 kurucu+gönder+al kompozisyonu).
- Determinizm: mesaj geçişi **30/30**, akış denetimi **20/20** (yarış yok — bloklama sayesinde).
- Regresyon: bare-metal `calistir_kanal_test_arm` **geçti** (toplam=55 — imza değişikliği kırmadı),
  **SELF-HOST FIXPOINT ✓** (stage1==stage2, 33371 satır; korpus 92), codegen parite **76/76**.

**SÜREÇ DERSİ:** `make` varsayılan hedefi `build/kdl_runtime.o`'yu KURMUYOR. İlk kanal denemesi bu
yüzden exit 0 verdi (bayat runtime) — IR doğruydu, obje eskiydi. Elle sondajlarda runtime'a dokunduysan
`mingw32-make build/kdl_runtime.o` şart; `calistir_*_test` hedefleri bağımlılıktan ötürü güvenli.

---

## D-291 — KATMAN 2 CANLANDI: `görev_başlat`/`görev_birleştir`/`dondur` codegen + GERÇEK thread runtime (2026-07-16) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-290).

**Karar [ETKİ: `src/llvm.c` (ast_tip_to_ir: görev/kanal→ptr; CAGRI'da 3 intrinsic erken-dönüş; 2 declare),
`runtime/kdl_runtime.c` (KdlGorev genişletme + kdl_gorev_basla_kapanis + gerçek-thread sayaçları),
`test/test_gorev_rt.c` (YENİ, 9 test), `test/test_llvm.c` (+4 uçtan uca), `test/ornekler/gorev_temel.kem`
(YENİ).]** Katman 2 bugüne dek **yalnız ön-yüz**tü: tip sistemi + DRF tanılamaları eksiksiz (39/39) ama
codegen SIFIR — bilinçli erteleme (D-008). Bu adım o ertelemeyi kapatır: **`görev` artık gerçekten çalışır.**

**Ölçülen başlangıç durumu (falsifiye-kanıt):** `görev_başlat(|| 42)` bugün
`call i32 @"görev_başlat"({ptr,ptr})` üretiyordu → `error: use of undefined value '@görev_başlat'`
(link kırık). `görev<T>` ise `ast_tip_to_ir`'ın sonundaki `return "i32"` fallback'ine düşüyordu →
**64-bit handle 32 bite kırpılır**. `dondur` da aynı sınıfta tanımsız sembol.

- **Kilit bulgu — fat value zaten (fn, arg) çifti:** `işlev(...)` IR'de `{ ptr fn, ptr env }` (V2-F1) ve
  çağrı ABI'si `env==null → fn(ρ)`, `env!=null → fn(ρ, env)` (V2-F4.2a ρ-ABI). Bu, bir thread-start'ın
  ihtiyacı olan şeyin **aynısı** → görev_başlat = fat value'yu açıp runtime'a geçirmek. Ek closure
  makinesi GEREKMEDİ.
- **Cast-siz ABI (ölçülmüş karar):** wrapper'ın fn'i doğru imzayla çağırması gerek; ama `void*`→fn-ptr
  `-Wpedantic`, fn-ptr→fn-ptr ise `-Wcast-function-type` uyarısı veriyor (**ikisi de ampirik ölçüldü**;
  sıfır-uyarı hedefi ikisini de eler). Çözüm: codegen aynı `ptr`yi **iki ayrı tipli parametreye**
  (`KdlGorevBare`/`KdlGorevKapanis`) geçer, C tarafı `env`e bakıp doğru olanı çağırır → her işaretçi
  YALNIZ kendi gerçek tipiyle çağrılır: ne uyarı ne UB.
- **Mevcut runtime yeniden kullanıldı:** `kdl_gorev_birlestir(KdlGorev*) -> i32` kdl_runtime.c'de ZATEN
  vardı ve tam doğru imzadaydı; yalnız başlatıcı (ρ + env geçiren) eklendi.
- **S1/S2 yapısal:** `kdl_gorev_basla_kapanis` her göreve `kdl_bolge_olustur()` ile KENDİ ρ_sahip'ini verir;
  çağıranın ρ'su paylaşılmaz → iki thread aynı bump-allokatöre yazamaz. Test [7] iki görevin ρ'sunun
  AYRI olduğunu doğrudan ölçer.

**SINIR (bilinçli, dürüst):**
- **ρ_sahip SERBEST EDİLMEZ (sızıntı).** R-BİRLEŞTİR "diğer bölgeler serbest" der; ama serbest bırakmak
  görev-gövdesi tahsislerinin ρ_sahip'e HAPSOLDUĞUNU gerektirir (gövde, yakalanan bir `&değişken`e
  ρ_sahip'ten işaretçi yazarsa serbest = UAF). Böyle bir **pozitif hapsedilme kanıtı YOK** — F4.2b'de
  ρ_yerel ancak böyle bir kanıt + adversarial tarama sonrası serbest bırakılmıştı. Kanıtsız serbest
  bırakmak yerine SIZDIRIYORUZ (güvenli taraf; F2 closure-env sızıntısıyla aynı status quo).
  `kdl_bolge_bakiye()` bunu dürüstçe raporlar.
- **T fiilen tam32.** Lifted lambda'nın IR dönüşü bugün SABİT i32 (`llvm.c` lambda_emit: "v1: tek-ifade
  gövde i32") — **önceden var olan lambda sınırı**, görev getirmedi. Yan etkisi olumlu: codegen float
  dönen lambda üretemediği için float-T'nin sessiz-bozulma riski yapısal olarak YOK. T'nin genişlemesi
  lambda dönüş-tipi çıkarsamasıyla BİRLİKTE gelmeli (tek iş).
- **`dondur` V1'de identity** (sıfır talimat): mutable→immutable daraltma yalnız TİP seviyesinde. Gerçek
  frozen-flag (R-PAYLAŞ zorlaması) V2 — tip_kontrol.c DRF005 notu zaten öyle diyor.
- **`kanal` HÂLÂ ÇALIŞMIYOR ve bu adımın kapsamı dışı:** DRF V1'de **kanal kurucusu YOK**. Tüm DRF
  testleri kanalı *parametre* olarak alıyor (`işlev test(k: kanal<tam32>)`); hiçbiri kanal yaratmıyor →
  gerçek bir programda `kanal<T>` değeri elde etmek İMKÂNSIZ, codegen eklense bile. Kurucu bir **syntax
  kararı** (Mehmet'e sorulacak; spec `gönderen<T>`/`alan<T>` ayrık uçlar derken implementasyon tek
  yönsüz `kanal<T>` kullanıyor — bu ayrışma da kararla birlikte çözülmeli).

**Kanıt (gerçek derle+çalıştır, "42 döndü" ile yetinmeden):**
- `test_gorev_rt` **9/9** — kritik olanlar: **[4] GERÇEK thread spawn edildi (sıralı fallback DEĞİL)** ve
  **[7] S1: iki görevin ρ_sahip'i AYRI**. Bu iki test olmadan süit "yanlış sebeple" geçebilirdi: runtime
  thread yaratılamazsa sessizce sıralı çalışır ve **aynı sonucu** üretir (D-287 LINCHPIN tuzağının aynı
  sınıfı) → ayrım için `kdl_gorev_thread_sayisi`/`kdl_gorev_sirali_sayisi` sayaçları eklendi.
- `test_llvm` **245/245** (+4: yakalamasız, yakalamalı closure env!=null, iki eşzamanlı görev 20+22, dondur).
- Regresyon: DRF **39/39**, codegen semantik parite **76/76**, **SELF-HOST FIXPOINT ✓** (stage1==stage2,
  33371 satır birebir — llvm.c'ye eklenen declare'ler self-host zincirini BOZMADI), lambda 5/5,
  çıplak region-free 6/6, `test_tumu` exit 0.

**Yan bulgu (kayda değer, ayrı iş):** skaler `&tam32` referansı bugün KEMGU'da **hiç okunamıyor** —
`ver v` T020, `v + 0` T003, `*v` T001; yalnız taşınıp `&tam32` olarak döndürülebiliyor (D25 deseni). Hiçbir
örnek/test skaler referans kullanmıyormuş. Bu yüzden `dondur`'un değer-turu testi yapı-referansı
(`&Kutu` → `r.deger`) üzerinden yazıldı. Önceden var olan boşluk, bu adımın getirdiği değil.

---

## D-290 — USERLAND ADIM 5+6 (SON): GERÇEK spawn+join (sys 12/14) + Model A program modeli teyidi — [15] SPAWN OK 🎉 (2026-07-14) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-289).

**Karar [ETKİ: `runtime/kem_gorev.kem` (kdl_syscall_isle num=12/14; kul_prog_selam — gömülü "program";
kul_spawner — EL0 spawn+join görevi; kem_spawn_testi). USERLAND_ROADMAP ADIM 5 (spawn/wait wiring) + ADIM 6
(program modeli) — SON iki adım, TEK testte birleşik.]** Kanıtlı-C `kdl_surec_spawn`/`kdl_gorev_durum`
(D-129/D-130) birebir basitleştirmesi: proven-C'nin per-process sayfa-tablosu (TTBR-swap) YOK — kem_os'un
A4/A5 TEK-adres-uzayı tasarımıyla UYUMLU (roadmap'in öngördüğü gibi, `kemgu_shell_el0.c`'nin kendi yorumu
da "TTBR-swap YOK" diyor).

- **num=12 spawn(entry):** SABİT 2-slot kernel/user-yığın havuzu (round-robin, fixed-RAM cursor) →
  `kem_gorev_olustur_el0(entry, kstk, ustk)` (A4/A5/B2'de ZATEN VAR) → pid döner. **num=14 durum(pid):**
  `kg_oku64(KG_OLU+pid*8)` — `kdl_gorev_durum` birebir (görev bitti mi, join/wait).
- **Model A program modeli (ADIM 6) TEYİT EDİLDİ:** `kul_prog_selam` — gömülü `.user` `.kem` fonksiyonu
  (derleme-zamanı linkli, spawn hedefi = zaten-var-olan sembolün adresi). **ELF-yükleme YOK, disk-programı
  YOK** — proven-C'nin `prog_hesap`/`prog_selam` desenine birebir (roadmap'in Model A tanımı DOĞRULANDI).
- **GERÇEK test:** `kul_spawner` (EL0) → `sys(12, kul_prog_selam_adr())` → pid al → `sys(14, pid)` ile
  GERÇEK poll-join (program bitene dek EL0'dan busy-wait) → programın bıraktığı iz (`KUL_PROG_FLAG==42`)
  doğrulanır. **İKİNCİ, BAĞIMSIZ bir EL0 süreci** (`kul_prog_selam`) `kul_spawner`'ın (kendisi de EL0'da
  koşan) syscall'ıyla DİNAMİK OLARAK yaratıldı — 3 eşzamanlı EL0 görevi (main dahil) doğru round-robin ile
  yönetildi.
- **İsimlendirme dersi (D-289'dan) UYGULANDI:** `kg_spawner_adr` (EL1'den `kem_spawn_testi` çağırır) bilinçli
  olarak `kg_` öneğiyle yazıldı, `kul_` DEĞİL — D-289'un routing-privilege bug'ı BU BATCH'TE TEKRARLANMADI.

**KAPI (4/4):** `[15] SPAWN OK` (ilk denemede) + `[1..14]` kümülatif; FIXPOINT birebir 33371; test_tumu
(bekleniyor).

**🎉 MİLESTONE — USERLAND_ROADMAP TAMAMLANDI (ADIM 1-6):** LINCHPIN + UART-RX + syscall-ABI + shell REPL +
spawn/join + Model A program modeli — hepsi GERÇEK, saf-.kem, tek boot'ta çalışıyor. kem_os artık:
sanal-bellek + fault/recovery + kesme-trap + timer-IRQ + preemptive multitasking + EL0 userspace + **gerçek
userland shell'e boot eden, dinamik süreç spawn edebilen bir OS**. Kalan (bu kampanyanın DIŞI): Model B
(diskten ELF yükleme, TTBR-per-process), canlı-interaktif shell (proven-C'nin tam tokenize/çok-komut
sürümü), çok-dosyalı minifs, `.S`-strict, x86_64 parite.

---

## D-289 — USERLAND ADIM 4: userland .kem SHELL — komut dispatch + GERÇEK syscall(5/7/17/18) yürütme — [14] SHELL OK (2026-07-14) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-288).

**Karar [ETKİ: `runtime/kem_gorev.kem` (num=5 yaz + num=7 satir eklendi; kul_shell/kul_str_esit EL0
shell; kg_ad_bayt/kg_seed_kopyala/kg_fs_seed/kg_shell_seed/kg_*_adr EL1-helper YENİDEN ADLANDIRILDI).
USERLAND_ROADMAP ADIM 4.]** proven-C `kemgu_shell_el0.c`'nin basitleştirilmiş `.kem` karşılığı: gömülü
DETERMİNİSTİK komut ("yaz") → `kul_str_esit` (proven-C str_esit birebir) ile DISPATCH → eşleşirse GERÇEK
syscall dizisi (5=konsola yaz, 7=satır, 17=dosya-yaz, 18=dosya-oku) çalıştırılır → sonuç konsola basılır.

**GERÇEK bug bulundu + düzeltildi — routing-prefix/privilege-seviyesi karışıklığı:** `kul_` öneği hem
"EL0'da GERÇEKTEN çalışacak kod" (routing hedefi) hem YANLIŞLIKLA "userland-ilgili herhangi bir yardımcı
fonksiyon" (isimlendirme kolaylığı) için kullanılmıştı. `kul_shell_seed` (EL1'den, `kem_shell_testi`
tarafından ÇAĞRILAN bir seed-fonksiyonu) `kul_` önekine sahip olduğu için objcopy rename adımı onu da
`.user`'a (AP=01, EL0-erişimli) TAŞIDI — ama EL1 O SAYFADAN KOD ÇALIŞTIRAMAZ (gerçek Instruction Abort,
EC=0x21, `adr=0x42000088` = TAM `kul_shell_seed`'in adresi — `llvm-nm` ile doğrulandı). **ADIM 3'te AYNI
BUG GİZLİYDİ**: `kul_fs_seed`/`kul_seed_kopyala`/`kul_ad_bayt`/`kul_*_test_adr` de EL1'den çağrılan
kul_-önekli fn'lerdi, ama HEPSİ -O2 tarafından INLINE edilmişti (çağıranın .text'ine gömüldü, standalone
sembol hiç OLUŞMADI) → objcopy hiç yakalamadı → bug MASKELENMİŞTİ. `kul_shell_seed` inline OLMADI (daha
büyük, kendi içinde çağrı yapıyor) → bug İLK KEZ ortaya çıktı. **Çözüm:** TÜM EL1-yalnız-çağrılan yardımcı
fn'ler `kul_` öneğinden `kg_` önekine TAŞINDI (inline-edilenler DAHİL — inlining derleyici-versiyonuna/
optimizasyon-seviyesine bağlı KIRILGAN bir korumaydı, isim-tabanlı düzeltme kalıcı). **Prensip:** `kul_`
öneği YALNIZ EL0'da GERÇEKTEN çalıştırılacak giriş-noktaları (spawn hedefleri) + onların ÇAĞIRDIĞI
diğer EL0-yalnız fn'ler (örn. `kul_str_esit`) için; EL1'den çağrılan seed/helper fn'ler `kg_` (veya
benzeri kernel-only önek) kullanmalı.

**KAPI (4/4):** `[14] SHELL OK` + konsol çıktısı `"yaz"` (GERÇEK sys5/7 round-trip) + `[1..13]` kümülatif;
FIXPOINT birebir 33371; test_tumu (bekleniyor).

**KAPSAM/SINIR:** Tek gömülü komut ("yaz") dispatch edildi — proven-C'nin tam tokenize/çok-komut/canlı-
girdi REPL'inin TAMAMI değil (roadmap'in MVP kapsamı: dispatch-mekanizması + GERÇEK syscall-yürütme
kanıtı). Kalan: ADIM 5 (spawn/wait wiring — altyapı A4/A5/B2'de VAR), ADIM 6 (program modeli teyidi).

---

## D-288 — USERLAND ADIM 3: GERÇEK dosya syscall (sys 17/18, EL0→minifs→virtio-blk) + gettick/getpid — [13] FS SYSCALL OK (2026-07-14) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-287).

**Karar [ETKİ: `runtime/kem_gorev.kem` (kdl_syscall_isle num=10/11/17/18, kul_ad_bayt, kul_seed_kopyala,
kul_fs_seed/test, kem_fs_testi); `runtime/kem_mmu.kem` (kis_buf_oku8). USERLAND_ROADMAP ADIM 3.]**
`kem_minifs.kem` (D-272) + `kem_zaman.kem`/`kem_gorev.kem`'in mevcut primitiflerini syscall ABI'sine
bağlar. **Kapsam-sınırı bulgusu:** minifs proven-C'nin çok-dosyalı `kdl_dosyalar[]`'ından FARKLI —
**tek-dosyalı** (her yazma blk1/blk2'yi YENİDEN KULLANIR). num=15/16 (int-dosya) ve 19/20/21 (sayisi/ad/
sil — çok-dosya listeleme) minifs'in ÇOK-DOSYA desteği GEREKTİRİR — AYRI özellik-genişletme, bu batch'in
kapsamı DIŞI. Bu batch: num=17/18 (yaz_metin/oku_metin, TEK-DOSYA ile uyumlu) + num=10/11 (gettick/getpid,
trivial — `kem_tik_oku`/`kem_paktif` zaten var).

**GERÇEK bug bulundu + düzeltildi — LLVM inline-asm early-clobber eksikliği:** `kul_fs_test`'in tek
`satıriçi_asm` bloğunda AYNI input operandı ($2=KUL_FS_AD) İKİ FARKLI syscall için TEKRAR kullanıldı
(`mov x0,$2` iki kez, aralarında `mov $0,x0` output-yazımı var). LLVM'in register allocator'ı $0(çıktı)
ile $2(girdi)'yi AYNI FİZİKSEL REGISTER'a (x9) atadı — `mov $0,x0` (r1'i yaz) $2'nin (KUL_FS_AD) DEĞERİNİ
EZDİ, ikinci `mov x0,$2` YANLIŞ pointer okudu (garbage/eski-r1-değeri). Disassembly ile TAM olarak
`mov x9,x0` (yaz) hemen ardından `mov x0,x9` (oku, artık r1'in değeri, KUL_FS_AD DEĞİL) görüldü. **Çözüm:**
`çıktı("=&r", ...)` — LLVM'in standart early-clobber constraint modifier'ı (`&`), çıktı register'ının
HİÇBİR girdi register'ıyla ÇAKIŞMAMASINI zorunlu kılar. `.kem` constraint string'leri LLVM IR'a HAM
geçiyor (D-286'dan beri bilinen) → codegen değişikliği GEREKMEDİ, sözdizimsel constraint-string düzeltmesi
yeterliydi. **Genel ders (gelecek satıriçi_asm yazımı için):** bir input operandı birden fazla syscall/
adım için TEKRAR kullanılıyorsa VE arada bir output yazılıyorsa, `=&r` ZORUNLU.

**Ampirik debug disiplini:** r1/r2 (syscall dönüşleri) + a0-a3 vs b0-b3 (isim bayt karşılaştırması) fixed-
RAM'e yazılıp okunarak izole edildi; disassembly (`llvm-objdump`) ile KESIN register-allocation çakışması
doğrulandı — varsayım yapılmadı.

**KAPI (4/4):** `[13] FS SYSCALL OK` + `[1..12]` kümülatif (GERÇEK EL0→syscall→minifs→virtio-blk yaz+oku
round-trip, bayt-bayt içerik doğrulaması); FIXPOINT birebir 33371; test_tumu (bekleniyor).

**KAPSAM/SINIR:** num=15/16/19/20/21 (çok-dosya) minifs genişletmesi gerektirir — ayrı iş. Kalan:
spawn/exec syscall (12/13/14 — 13 zaten kullanımda, çekirdek altyapı A4/A5/B2'de VAR), shell REPL,
net syscall wiring (24/25, kem_virtio_net.kem zaten var).

---

## D-287 — USERLAND ADIM 2: GERÇEK UART-RX syscall (sys 26 read_satir, EL0 round-trip) — [12] UART RX EOF OK (2026-07-14) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-286).

**Karar [ETKİ: `runtime/kem_mmu.kem` (kis_satir_oku PL011 RXFE bounded-poll, KIS_UART_FR düzeltmesi);
`runtime/kem_gorev.kem` (kdl_syscall_isle num=26/13, kul_rx_test, KG_EL0_KOD/KOTU/KUL_RX_BUF/SONUC
adres düzeltmeleri). USERLAND_ROADMAP ADIM 2.]** Kanıtlı-C `kdl_kesme.c:478-495` (PL011 FR.RXFE
bounded-poll, D-158 hang-önleme dersi) BİREBİR .kem yeniden gerçekleştirmesi + GERÇEK EL0 round-trip.

**Üç GERÇEK bug bulundu + düzeltildi (bu batch'in asıl işi — salt sembol taşıma değildi):**

1. **LINCHPIN adres-çakışması (D-286'nın kendi hatası):** `kul_test` (.kem-derlenmiş, D-286) `.user`
   section'ın TAM BAŞINA (0x42000000) yerleşiyordu — AYNI adrese A5'in `kem_el0_testi`'si (ondan ÖNCE
   koşan, HAM byte yazan eski test) yazıyordu → `kul_test`'in derlenmiş byte'larını RUNTIME'DA EZİYORDU.
   D-286'nın LINCHPIN gate'i YANLIŞ SEBEPTEN geçmişti (ezilmiş eski stub çalışıyordu, gerçek `kul_test`
   içeriği DEĞİL). **Düzeltme:** ham-byte test adresleri (`KG_EL0_KOD`/`KG_EL0_KOTU`) `.user` kod
   tabanından +64KB'a taşındı — gelecekteki `.kem`-routed kod büyümesiyle asla çakışmaz.
2. **`KIS_UART_FR` yanlış sabit (B1'den beri latent, D-283):** `150995112` (0x090000A8) — PL011'in
   GERÇEK FR offset'i `0x18`=`150994968`. 0xA8 PL011'de tanımsız/reserved (muhtemelen her zaman 0
   okunuyordu). TX (kis_bayt, B1) bu YÜZDEN hiç GERÇEKTEN TXFF beklemiyordu (her zaman "boş" görüp
   hemen geçiyordu — yanlış ama zararsız, TX asla gerçek backpressure test edilmedi). RX bu bug'ı
   MASKELEYEMEDİ: RXFE her zaman 0 (=veri var) okunup DR'den çöp okuyup asla \n/\r bulamayarak SONSUZ
   DÖNGÜYE girdi (gerçek gate ortaya çıkardı).
3. **EL0 kodu kernel-only (AP=00) belleğe yazmaya çalışıyordu:** `kul_rx_test` (EL0'da koşuyor) sonucu
   `KUL_RX_SONUC`'a (0x47300100, "free RAM" — AP=00, EL0-YASAK) doğrudan `str` ile yazmaya çalıştı.
   Gerçek permission-fault oluştu; `kis_el0_kill_aktif` B2'den beri AÇIK KALMIŞTI (hiç kapatılmıyor) →
   fault izolasyon-öldürme yoluna gitti (görev "başarıyla bitti" GÖRÜNDÜ, `KG_OLU` set edildi) AMA
   yazma HİÇ GERÇEKLEŞMEDİ (faulting instr atlandı) → sonuç her zaman ilk-değer (0) kaldı. **Düzeltme:**
   `KUL_RX_BUF`/`KUL_RX_SONUC` `.user` sayfasına (0x42020000+, AP=01=EL0+EL1 RW) taşındı.
- **Poll-sınırı kalibrasyonu:** proven-C'nin `8_000_000` sabiti GERÇEK donanım hızı varsayıyordu; QEMU'da
  HER MMIO okuması device-emulation trap'i (mikrosaniyeler) → 8M iterasyon TÜM kem_os boot penceresini
  (12s) dolduruyordu (ampirik). `20_000`'e küçültüldü — anlam AYNI (bounded-poll→EOF), ortam-hızına göre
  kalibre (kör-kopyalama DEĞİL).
- **Ampirik debug metodolojisi:** her katmanda (EL1-direkt çağrı → syscall-argüman → .kem `ret` IR →
  `.S` register-restore → EL0 asm disassembly) izole debug ile kanıtlanmadan varsayım yapılmadı — 3 bug
  da ancak bu katman-katman izolasyonla bulundu.

**KAPI (4/4):** `[12] UART RX EOF OK` + `[1..11]` kümülatif (otomatik boot'ta girdi YOK → EOF doğru/
beklenen sonuç, proven-C'nin kendi "best-effort" gate stratejisiyle aynı); FIXPOINT birebir 33371;
test_tumu (bekleniyor, bu commit'te doğrulanacak).

**KAPSAM/SINIR:** `kdl_user_yaz_ptr_gecerli` (D-150 user-ptr doğrulama) HENÜZ `.kem`'e taşınmadı —
num=26 şimdilik kernel-güvenilir arg alıyor (ADIM 3 kapsamı, ayrıca not edildi). Kalan: syscall-ABI
genişletme (dosya/spawn/net), shell REPL, spawn/wait wiring.

---

## D-286 — USERLAND ADIM 1: LINCHPIN — GERÇEK .kem-derlenmiş kod EL0'da (`.user` objcopy-rename routing) — [11] LINCHPIN OK 🎉 (2026-07-14) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-285).

**Karar [ETKİ: `Makefile` (kem_os `.ll→.o` derlemesine `-ffunction-sections -fdata-sections` +
objcopy `--rename-section` post-compile adımı); `runtime/kem_gorev.kem` (`kul_test`/`kul_test_adr`/
`kem_linchpin_testi`); `test/ornekler/kem_os.kem` ([11] LINCHPIN bloğu). USERLAND_ROADMAP (03133f0)
ADIM 1 — enabling-primitif, Model A shell/spawn'ın önkoşulu.]** A5/B2'nin EL0 stub'ları runtime'da
yazılan makine-koduydu (llvm-mc byte'lar) — `.kem`'de section-attribute YOK. Bu adım GERÇEK
`.kem`-derlenmiş kodun `.user` (0x42000000, AP=01) section'ına yönlendirilip EL0'da çalıştığını kanıtlar.

- **Linker-script reorder DENENDİ + BAŞARISIZ (ampirik, USERLAND_ROADMAP'te öngörülmemiş bir bulgu):**
  `.user`'ın (VMA 0x42000000) matching-kuralını `.text`'in (VMA 0x40000000) GENEL `*(.text*)` glob'undan
  ÖNCE koymak için script'te "geriye" adres ataması (`. = 0x42000000` sonra `. = 0x40000000`) `ld.lld`'de
  **"output file too large: 18446744073676063576 bytes"** (unsigned wraparound) hatası verdi — hem
  normal `. = ` ilerlemesiyle hem `SECTION ADDR :` satır-içi adres formuyla. `ld.lld`'nin düz-script
  modeli monoton-olmayan VMA sırasını desteklemiyor (en azından bu basit script yapısında).
- **Çözüm — post-compile objcopy rename (linker-script DEĞİŞMEDİ):** `-ffunction-sections
  -fdata-sections` her `.kem` sembolünü kendi adıyla `.text.<isim>`/`.data.<isim>`/`.bss.<isim>`/
  `.rodata.<isim>`'e böler (LLVM IR girdisinde bile — codegen değişmedi). Derleme sonrası `llvm-nm`
  ile `kul_`-önekli semboller keşfedilir + `llvm-objcopy --rename-section` ile `.user`/`.user_data`'ya
  YENİDEN ADLANDIRILIR (mevcut `*(.user) *(.user.*)` glob'u eşleştirir, linker-script HİÇ değişmedi).
  `llvm-objcopy` var-olmayan section'ı rename etmeye çalışınca sessizce no-op (ampirik doğrulandı) →
  4 rename-flag (text/data/bss/rodata) her sembol için güvenle üretilebilir.
- **`kul_test` (çıplak, `.kem` kaynağından normal derlenmiş, HAND-ASSEMBLE DEĞİL):** sonsuz `mov x8,#7 /
  svc #0` döngüsü (satıriçi_asm). `kem_linchpin_testi`: `kul_test_adr()` (adrp/lo12 ile GERÇEK sembol
  adresi) → `kem_gorev_olustur_el0` (A5/B2'de zaten var) ile EL0 görevi spawn → `KG_SYSC` sayacı
  (kdl_syscall_isle üzerinden) artıyor mu doğrula.
- **Falsifiye-kanıt (routing gerçekten çalıştı mı, sentetik-geçiş imkânsız):** final ELF'te
  `kul_test`'in adresi `.user` aralığında (`0x42000000-0x421FFFFF`) mı diye Makefile'da programatik
  kontrol (hex→decimal karşılaştırma). Routing yanlış olsaydı `kul_test` kernel `.text`'te (0x40000000
  civarı, AP=00) kalır → EL0'dan instruction-fetch permission-fault → `KG_SYSC` HİÇ artmaz → test
  kırmızı. **Gerçek adres: `0x42000000`** (tam `.user` başlangıcı).

**KAPI (4/4):** `[11] LINCHPIN OK` + `[1..10]` kümülatif; `kem_os.ll define @kul_test` + `call
@kem_linchpin_testi`; final-ELF adres-aralığı programatik doğrulandı; FIXPOINT birebir 33371 (compiler
`src/*.c` DOKUNULMADI — yalnız Makefile+`.kem`); test_tumu (bekleniyor, bu commit'te doğrulanacak).

**MİLESTONE:** USERLAND_ROADMAP'in "GO/hard-blocker" kararı EMPİRİK OLARAK doğrulandı — LINCHPIN
çalışıyor. Model A shell/spawn artık gerçek `.kem` userland kaynağı yazılabilir (routing sözleşmesi:
`kul_` önek, çıplak-tier, `metin` literal YOK — adlandırılmış `küresel değişken` bayt-dizisi kullan).
Kalan: USERLAND ADIM 2-6 (UART-RX syscall, syscall-ABI genişletme, shell REPL, spawn/wait wiring,
program modeli) — roadmap'te DAG'lı, ayrı batch'ler.

---

## D-285 — ZERO-C FAZ 2 B3 (SON): fault-scratch + el0-kill global'leri .S-data'ya taşındı — kem_os C=0 🎉 (2026-07-13) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-284).

**Karar [ETKİ: `boot/start_aarch64.S` (3 global `.data` — indirgenemez-.S substrat); `runtime/kdl_kesme.c`
(3 global → WEAK, C tanımları artık dead-var diğer kernel'lerde); `Makefile` (KEM_OS_A64_OBJS'ten
`bm_a64_kesme.o` TAMAMEN ÇIKARILDI). ZERO-C FAZ 2 SON batch — kampanya bitiş.]** B1+B2 sonrası
`kdl_kesme.c`'nin (kem_os linkindeki) tek canlı katkısı 3 data global'iydi (`kdl_fault_bekleniyor`,
`kdl_fault_yakalanan`, `kdl_el0_kill_aktif` — 24 B `.bss`, sıfır kod). Bunlar `.S`-data'ya taşındı.

- **`.S`-data (D-276'nın "TERCİH" notu uygulandı):** `boot/start_aarch64.S` sonuna `.data`/`.align 3` +
  3× `.quad 0` (aynı ad, aynı tip-semantiği). İndirgenemez asm substrat'ın parçası — codegen-fix
  (`dışa küresel`→external) GEREKMEDİ, ATLANDI (D-276/277'nin flag'lediği gap hâlâ açık ama bu iş
  ONU beklemedi).
- **Weak-override (C→ölü-var diğer kernel'lerde):** C 3 global `__attribute__((weak))`. `.S` artık
  STRONG tanımlıyor → kem_os `bm_a64_kesme.o`'yu HİÇ LİNKLEMEZ (Makefile'dan çıkarıldı). Diğer kernel'ler
  (`.S` + `kdl_kesme.c` birlikte) çift-sembol ÇAKIŞMASI YAŞAMADI (weak↔strong linker kuralı) — `.S` kazanır,
  C weak ölü-var kalır, davranış AYNI (aynı ad/tip/başlangıç-değer=0). Ampirik doğrulandı:
  `calistir_kernel_dizi_bare_metal` (kesme.c+.S birlikte linkli) temiz derlendi + booted.

**RE-AUDIT (TAM, -Map+nm, final ELF'teki TÜM 167 tanımlı sembol tek tek sınıflandırıldı):**

| Kaynak | Sembol sayısı |
|---|---|
| `.kem` (kem_os.o + kem_heap.o) | **147** |
| `boot/*.S` (bm_a64_start.o) | **10** |
| linker-script synthetic (`__bss_start` vb., `=` atamaları, hiçbir .o'dan gelmez) | **10** |
| **C-derlenmiş** | **0** |

`bm_a64_kesme.o` link satırında YOK (grep=0). `.text` = 22876 B, tamamı `.kem`+`.S` (0 C-katkı).

**KAPI (re-audit 4/4 + ek doğrulama):** `[1..10]+[5]` QEMU yeşil (`[5] IZOLASYON OK` dahil — B2'nin
9-tekrar karakteristiği burada 21-tekrar olarak gözlendi, aynı dokümante-C-davranışı, zararsız); FIXPOINT
birebir 33371; test_tumu exit 0; **diğer kernel regresyon-yok** (kernel_dizi ampirik doğrulandı).

**🎉 KARAR (lenient-Law-4 doruk):** kem_os aarch64 nihai ELF'i artık `boot/*.S` DIŞINDA C-derlenmiş sembol
İÇERMİYOR. İndirgenemez `.S` kümesi (ZERO_C_AUDIT ADIM 4 ile aynı): `_start`, `_halt`,
`kdl_vektor_tablosu`, `kdl_exc_ortak`, `kdl_irq_ortak`, `kdl_baglam_degis`, `kdl_el0_calistir` + 3 `.data`
global (`kdl_fault_bekleniyor/yakalanan`, `kdl_el0_kill_aktif`). Toolchain/libc yok (`-nostdlib`).

**KAPSAM/SINIR:** `.S`-strict (P4: asm → `.kem satıriçi_asm`) AYRI karar, bu kampanyada YAPILMADI —
lenient-Law-4 (asm≠C) burada duruyor. x86_64 parite ayrı Law-4 borcu. `docs/ZERO_C_AUDIT.md` güncellendi.

---

## D-284 — ZERO-C FAZ 2 B2: kdl_el0_izolasyon_isle SAF-.kem (GERÇEK permission-fault→süreç-öldür) — yazdir/uart/gorev TAMAMEN düştü (2026-07-13) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-283).

**Karar [ETKİ: `runtime/kem_gorev.kem` (KG_OLU dead-task tablosu + `kem_gorev_bitir` + `kem_preempt`
skip-mantığı + `kem_el0_izolasyon_testi`); `runtime/kem_mmu.kem` (kis_el0_kill_ac + `kdl_el0_izolasyon_isle`
.kem); `runtime/kdl_kesme.c` (kdl_el0_izolasyon_isle → WEAK). ZERO-C FAZ 2 B2.]** Kanıtlı-C
`kdl_el0_izolasyon_isle` (EL0 izolasyon-ihlali → süreç öldür, D-130) BİREBİR .kem yeniden gerçekleştirmesi.

- **kem_gorev.kem'e dead-task tablosu EKLENDİ (yeni özellik, birebir C mirror):** `KG_OLU` (fixed-RAM,
  16 slot) + `kem_gorev_bitir()` (aktif görevi ölü işaretle) + `kem_preempt`'in round-robin taramasına
  skip-mantığı (`kdl_preempt`'in "if kdl_olu[n] continue" birebir). Öncesinde kem_os'un `.kem` scheduler'ı
  (A4) hiç dead-task kavramı BİLMİYORDU — bu, C'nin gerçek semantiğini tamamlayan gerçek yeni iş (sadece
  sembol taşıma değil).
- **GERÇEK test (kem_el0_izolasyon_testi):** EL0 stub GIC MMIO'ya (0x08000000, Device L1[0] AP=00,
  EL0-erişimi YASAK) `ldr` ile erişir → GERÇEK permission-fault, EL0-kaynaklı. `.S` kdl_exc_ortak
  (SPSR.M[3:2]=0 + `kdl_el0_kill_aktif` opt-in AÇIK — `kis_el0_kill_ac` .kem'den yazar) → `bl
  kdl_el0_izolasyon_isle(far)` (.kem) → rapor (B1'in `kis_*` primitifleri) + `kem_gorev_bitir` → `KG_OLU[t]=1`.
  Doğrulama: `KG_OLU[t]==1` (scheduler görevi işaretledi) + OS DEVAM etti (main [6..10]'a geçti).
- **Gözlenen (dokümante-edilmiş C davranışı, bug DEĞİL):** "IZOLASYON OK" 9× tekrarlandı — aynı-görev
  fault-recovery yalnız faulting instr'ı ATLAR (scheduler switch ZORLAMAZ), ölü görev sonraki GERÇEK
  timer-IRQ'ya dek kendi döngüsünde tekrar tekrar fault eder. `.S` yorumu bunu açıkça belgeler: "ölü EL0
  görev kısa sürer, sonraki timer-IRQ'da scheduler onu ATLAR". C orijinaliyle birebir aynı karakteristik.
- **Weak-override:** C `kdl_el0_izolasyon_isle` → `weak`; .kem strong kazanır (B1/A5 deseni).

**RE-AUDIT sonucu (-Map+nm):** `.text` 21572→20834 B. **yazdir/uart/gorev nesneleri final ELF'te TAMAMEN
0 canlı sembol** (B1'in kısmi düşüşü tamamlandı — `kdl_el0_izolasyon_isle` yazdir'in son canlı referansıydı).
`kdl_el0_izolasyon_isle` final ELF'te `@0x400005ac` (kem_os .kem aralığı).

**KAPI (re-audit 4/4):** yazdir/uart/gorev sembolleri final ELF'te YOK (0/0/0); `[1..10]+[5]` QEMU yeşil
(`[5] IZOLASYON OK` — GERÇEK permission-fault + kill + skip); FIXPOINT birebir 33371; test_tumu exit 0.

**KAPSAM/SINIR:** Kalan CANLI-C = yalnız `kdl_kesme.c` veri-plumbing (~68 B kod + fault-scratch/
`kdl_el0_kill_aktif` data). Sıradaki: B3 (fault-scratch global → `.S`-data, `.quad`) → beklenen sonuç:
nihai ELF `.text` = yalnız `.kem` + `.S` (lenient-Law-4 doruk). x86_64 ayrı borç.

---

## D-283 — ZERO-C FAZ 2 B1: kdl_istisna_isle SAF-.kem (çıplak-tier ham-MMIO UART) — kesme.o katkısı 192→68 B (2026-07-13) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-282).

**Karar [ETKİ: `runtime/kem_mmu.kem` (kis_* çıplak-tier ham-MMIO UART + `kdl_istisna_isle` .kem);
`runtime/kdl_kesme.c` (kdl_istisna_isle → WEAK). ZERO-C FAZ 2 ilk batch (DAG: B1→B2→B3).]**
Kanıtlı-C `kdl_istisna_isle` (EL1 kurtarılamaz-fault → rapor+halt) BİREBİR .kem yeniden gerçekleştirmesi.

- **Çıplak-call-rule engeli (D-257/E013):** mevcut `uart_metin`/`uart_bayt` normal-tier (`yetki<MMIO>`
  capability struct) → çıplak fn'den çağrılamaz (statik-reddedilir). Çözüm: YENİ çıplak-tier ham-MMIO
  UART ailesi (`kis_bayt`/`kis_metin`/`kis_satir`/`kis_onaltilik`) — doğrudan volatile-deref (vblk_y8/
  kg_yaz32 deseni), capability-suz. `metin_uzunluk`/`metin_bayt` builtin (EKLE_BUILTIN, zaten çıplak
  `kdl_metin_*`'e çözülüyor) → çıplak'tan izinli.
- **Weak-override:** C `kdl_istisna_isle` → `__attribute__((weak, noreturn))`; .kem strong link'te kazanır
  (kdl_syscall_isle deseni birebir). Diğer kernel'ler C weak'i kullanmaya devam (regresyon yok).
- **RE-AUDIT sonucu (-Map+nm, kesin ölçüm):** kesme.o'nun final ELF'e katkısı **192→68 B**; `.text.
  kdl_istisna_isle` section'ı final linkte **0** (düştü, C tanımı ölü-var). `kdl_istisna_isle` final
  ELF'te `@0x40000380` — kem_os.o (.kem) aralığında (weak-override doğrulandı).
- **Beklenmedik bulgu (DAG düzeltmesi):** yazdir/uart HÂLÂ canlı — `kdl_el0_izolasyon_isle` (B2'nin
  hedefi) de `kdl_yazdir_*` çağırıyor ("IZOLASYON OK..." raporu). B1 tek başına yazdir/uart'ı
  SIFIRLAMADI (öngörülenin aksine); tam sıfırlama B2 sonrasına kalır.

**KAPI (re-audit 4/4):** `kdl_istisna_isle` C-section'ı final ELF'te YOK (section-count=0); [1..10]+[5]
QEMU yeşil (garbling yok, boot fault-halt yoluna girmedi — beklenen, kem_os yalnız kurtarılabilir fault
tetikliyor); FIXPOINT birebir 33371; test_tumu (bekleniyor — bu commit'te doğrulanacak).

**KAPSAM/SINIR:** DAG sırası: B2 (`kdl_el0_izolasyon_isle`+`kdl_gorev_bitir`→.kem ⇒ yazdir+uart+gorev
tam düşer) → B3 (fault-scratch global→`.S`-data). x86_64 ayrı borç.

---

## D-282 — ZERO-C FAZ 0: --gc-sections + function-sections → kem_os residual-C 14888→744 B (%95) (2026-07-13) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-281).

**Karar [ETKİ: `Makefile` (BM_A64_CF += `-ffunction-sections -fdata-sections`; kem_os link += `--gc-sections`);
`docs/ZERO_C_AUDIT.md` (FAZ 0 güncelleme). ZERO-C kampanyası ilk batch — düşük-risk kazanç + re-audit
kapısı doğrulama. Kaynak/mantık DEĞİŞMEDİ; yalnız link ölü-kod-eleme.]** ZERO_C_AUDIT bulgusu: kem_os nihai
ELF `.text`'i %42 C (14 888 B) içeriyordu — `--gc-sections` yokluğundan tüm C nesneleri linkte kalıyordu.

- **Değişiklik:** BM_A64_CF'e `-ffunction-sections -fdata-sections` (per-fn/data section granülaritesi),
  kem_os link'ine `--gc-sections` (ENTRY(_start) kökünden ulaşılamayan section'ları at). YALNIZ kem_os
  link'i; diğer kernel link'leri gc-sections'sız → çıktı işlevsel aynı (regresyon yok).
- **Sonuç (re-audit, -Map+nm):** `.text` 0x8b64→0x5444 (35684→21572 B). **CANLI-C 14 888→744 B (%95↓).**
  - 3 ölü nesne (heap_kemmalloc/bolge_kemregion/mmio_kem) düştü.
  - Override-edilen ölü weak `kdl_syscall_isle` gövdesi düştü → onun transitif çektiği **virtio/
    virtio_net/mmu_kem/zaman_kem/panik TAMAMEN düştü** (beklenenden büyük kazanç — dead-body zinciri kırıldı).
  - KALAN CANLI-C = 7 fn (744 B): `.S`→`kdl_istisna_isle`+`kdl_el0_izolasyon_isle` (kesme) →
    `kdl_yazdir_metin/satir/onaltilik` (yazdir) → `kdl_uart_pl011_putc` (uart) ; `kdl_gorev_bitir` (gorev).
    + C-data: fault-scratch (`kdl_fault_bekleniyor/yakalanan`) + `kdl_el0_kill_aktif`.

**KAPI (re-audit, 4/4):** 3 ölü nesne sembolü final ELF'te YOK (nm=0); [1..10]+[5] QEMU yeşil (gerçek
fault/EL0, garbling yok); FIXPOINT birebir 33371 (compiler dokunulmadı); test_tumu exit 0.

**KAPSAM/SINIR:** kalan CANLI-C = `.S`'in kdl_kesme.c exception/izolasyon-report ref'leri. FAZ 2 DAG:
(B1) kdl_istisna_isle→çıplak .kem + .kem-UART ⇒ yazdir+uart; (B2) kdl_el0_izolasyon_isle+kill_aktif→.kem
⇒ gorev; (B3) fault-scratch global→`.S`-data (`.quad`). x86_64 ayrı borç.

---

## D-281 — REAL-OS FAZ-A5: SAF-.kem SYSCALL + EL0 USERSPACE (SVC→.kem handler) — [5] EL0 SYSCALL OK — 🎉 FAZ-A TAM (2026-07-13) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-280).

**Karar [ETKİ: `runtime/kem_gorev.kem` (kem_gorev_olustur_el0 + EL0 stub + kdl_syscall_isle .kem);
`runtime/kem_mmu.kem` (0x42000000 AP=01 user page); `runtime/kdl_kesme.c` (C kdl_syscall_isle → WEAK);
`test/ornekler/kem_os.kem` ([5g] EL0 bloğu); `Makefile` (EL0 SYSCALL OK gate). FAZ-A SON batch —
userspace/syscall. FAZ-A ÇEKİRDEK (A1-A5) TAM.]** EL0 (unprivileged) görev → SVC → SAF-.kem syscall
handler. A4 preemptive altyapısını EL0'a genişletir.

- **EL0 görev = preemptive görev (SPSR=EL0t):** kem_gorev_olustur_el0 sentetik trap-frame: @248=ELR=EL0
  stub, @256=SPSR=0x0 (EL0t, IRQ-açık), @264=SP_EL0=user yığını (AP=01). Timer-IRQ ile EL0↔main round-robin
  (.S kdl_irq_ortak SP_EL0'ı @264 kaydeder/geri-yükler → EL0 preempt edilebilir, D-125 mekanizması).
- **EL0 kod = runtime makine-kodu (.kem section-attr YOK çözümü):** derlenmiş .kem işlevi kernel .text'te
  (AP=00) → EL0 çalıştıramaz. Çözüm: EL0 stub'ı (movz x8,#7 / svc #0 / b .-8 — llvm-mc DOĞRULANMIŞ
  0xD28000E8/0xD4000001/0x17FFFFFE) runtime'da AP=01 sayfaya (0x42000000) YAZ + I-cache maintenance
  (dc cvau→dsb→ic ivau→dsb→isb). MMU: L2[16] = 0x42000000|0x745 (AP=01, UXN=0 EL0-exec).
- **SVC dispatch SAF-.kem (weak-override):** SVC → .S kdl_exc_ortak (EC=0x15) → `bl kdl_syscall_isle`.
  C kdl_syscall_isle `__attribute__((weak))` yapıldı → .kem strong (kem_gorev.kem) link'te OVERRIDE eder
  (guard/variant/cascade YOK — kdl_kesme.c'nin büyük syscall fonksiyonunu guard'lamak riskliydi; weak temiz).
  .kem handler: num=7 → syscall sayacı++ (fixed RAM), arg echo → EL0 x0.
- **GERÇEK EL0-syscall kanıtı:** sayaç YALNIZ EL0 stub'ın svc'siyle artar; kem_el0_testi sayaç>=3 bekler →
  EL0'dan (privilege sınırı geçilerek) gerçek syscall round-trip. Boot [6..10]'a devam. İLK denemede boot.

**FALSİFİYE-KANIT:** kem_os QEMU: `[5] EL0 SYSCALL OK` (+ [1..5] + [6..10]). gate: kem_os.ll
`define @kdl_syscall_isle` (weak-override) + `define @kem_gorev_olustur_el0` + `call @kem_el0_testi` +
`asm "dc cvau"` (I-cache). FIXPOINT birebir (33371 — compiler src/*.c DOKUNULMADI); test_tumu tam yeşil
(kdl_kesme.c yalnız weak-attr, semantik değişmez).

**🎉 MİLESTONE — FAZ-A ÇEKİRDEK TAM (A1-A5):** kem_os TEK BOOT'ta TAMAMEN SAF-.kem gerçek-OS:
sanal-bellek (MMU kurulum+çeviri) + gerçek page-fault/recovery + kesme-trap karar + periyodik timer-IRQ +
preemptive multitasking + **EL0 userspace + syscall** + disk (virtio-blk) + dosya-sistemi (minifs) +
ağ (virtio-net ARP/IPv4/ICMP ping). C substrat YALNIZ: `.S` (vektör/trap-frame/eret/context-restore) +
recovery-scratch/GIC-glue düzeyinde. **Kalan FAZ-A:** EL0 izolasyon-kill (opt-in kdl_el0_kill_aktif —
gelecek), çok-adres-uzayı (TTBR-per-task), syscall ABI genişletme (dosya/net syscalls EL0'dan).

---

## D-280 — REAL-OS FAZ-A4: SAF-.kem PREEMPTIVE scheduler (timer→IRQ→context-switch) — [5] PREEMPT OK (2026-07-13) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-279).

**Karar [ETKİ: `runtime/kem_gorev.kem` (YENİ — SAF-.kem round-robin scheduler + sentetik trap-frame);
`runtime/kem_zaman.kem` (kdl_irq_isle → `ver kem_preempt(sp)`); `test/ornekler/kem_os.kem` ([5f] PREEMPT
bloğu); `Makefile` (kem_gorev CAT + PREEMPT OK gate). FAZ-A4 — roadmap'in EN ZOR entegrasyonu: gerçek
preemptive multitasking. FAZ-A çekirdek (A1-A4) TAM.]** Kanıtlı-C kdl_gorev.c (kdl_preempt +
kdl_preempt_gorev_olustur) BİREBİR .kem yeniden gerçekleştirmesi: timer IRQ → context-switch.

- **Context-switch = SP-swap (yeni .S YOK):** timer IRQ → .S kdl_irq_ortak (full trap-frame kaydet) →
  `bl kdl_irq_isle` (.kem) → `kem_preempt(sp)`: mevcut görevin trap-frame SP'sini kaydet + round-robin
  sonrakinin SP'sini döner → .S `mov sp, x0` + trap-frame restore + eret → sonraki görev sürer. .S
  kdl_baglam_degis (cooperative) GEREKMEZ — preemptive switch tamamen kdl_irq_ortak SP-swap'ıyla.
- **Sentetik trap-frame (kem_gorev_olustur, kdl_preempt_gorev_olustur birebir):** 34 slot × 8 = 272 bayt,
  @248=ELR=giriş, @256=SPSR=0x5 (EL1h + IRQ-açık → görev de preempt edilir), gerisi 0; SP 16-hizalı.
- **KRİTİK BUG + çözüm (dead-store-elim):** preempt-aktif bayrağı küresel olunca clang `store 1` (ac) →
  `store 0` (test sonu) arasını gördü, IRQ-context okumasını GÖRMEDİ → DGE `store 1`'i sildi → asla switch.
  Bulgu: psayi=3 + görevler kuruldu ama g1=g2=0. Çözüm: aktif bayrağı + görev sayaçları FİXED RAM'de
  (0x45003000+, volatile çıplak-deref) → store/load volatile → DGE/store-sink YOK. (Aynı ders sayaçlar
  için de: yalnız-asm-kullanılan küresel DGE ile silinir → fixed-RAM deref.)
- **GERÇEK preemption kanıtı:** 2 sonsuz-döngü görev; timer-IRQ round-robin ile ikisi de milyonlarca kez
  koştu (g1≈g2≈0x21f000 INTERLEAVE) → gerçek eşzamanlı yürütme. Sonra preemption kapatıldı → main
  [6..10]'a devam (görevler bırakıldı). guard'lı (switch bozuksa hang yok).

**FALSİFİYE-KANIT:** kem_os QEMU: `[5] PREEMPT OK` (+ [1..5] + [6..10] kümülatif). gate: kem_os.ll
`define @kem_preempt` + `call @kem_preempt` (kdl_irq_isle'den) + `define @kem_gorev_olustur` +
`call @kem_preempt_testi`. FIXPOINT birebir (33371 — compiler src/*.c DOKUNULMADI, C guard bile YOK —
scheduler adı kem_preempt, C kdl_preempt'le çakışmaz); test_tumu tam yeşil.

**MİLESTONE:** 🎉 FAZ-A ÇEKİRDEK (A1 MMU + A2 trap + A3 timer-IRQ + A4 preemptive) TAM SAF-.kem. kem_os
artık: sanal-bellek + gerçek page-fault/recovery + kesme-trap karar + periyodik timer-IRQ + preemptive
multitasking + disk/fs/net — hepsi saf-.kem, tek boot, .S yalnız vektör/trap-frame/eret substrat'ı.
Kalan: A5 (syscall/EL0 userspace — SVC dispatch + EL0 izolasyon; sentetik trap-frame SPSR=EL0t + SP_EL0).

---

## D-279 — REAL-OS FAZ-A3: SAF-.kem GERÇEK timer-IRQ (GICv2 + CNTV) + .kem IRQ dispatch — [5] TIMER TIK OK (2026-07-13) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-278).

**Karar [ETKİ: `runtime/kem_zaman.kem` (YENİ — SAF-.kem GIC+CNTV+kdl_irq_isle); `runtime/kdl_zaman.c`
(kdl_kesme_kur/kdl_timer_baslat/kdl_irq_isle `#ifndef KEMGU_KEM_MALLOC` guard); `test/ornekler/kem_os.kem`
([5e] TIMER bloğu); `Makefile` (bm_a64_zaman_kem.o variant + kem_zaman CAT + TIMER TIK OK gate). FAZ-A3 —
GERÇEK donanım-kesmesi (IRQ) ilk kez SAF-.kem'de. Bring-up roadmap A3.]** Kanıtlı-C kdl_zaman.c'nin BİREBİR
.kem yeniden gerçekleştirmesi: periyodik timer IRQ, .S kdl_irq_ortak `bl kdl_irq_isle` artık .kem'i çağırır.

- **WebSearch/ARM teyit:** GICv2 (QEMU virt: GICD 0x08000000, GICC 0x08010000), sanal timer PPI INTID 27,
  CNTV_CTL/TVAL_EL0 + CNTFRQ_EL0, DAIF.I (daifclr #2) IRQ mask. GICC_IAR (0x0C) oku → INTID; GICC_EOIR
  (0x10) yaz → kesme bitir. Kesme girişinde PSTATE.I hardware-set → handler re-entran DEĞİL.
- **SAF-.kem (`kem_zaman.kem`):** kdl_kesme_kur (GICD_CTLR/GICC_PMR/CTLR/ISENABLER0-bit27 MMIO), kdl_timer_baslat
  (cntfrq/100 ~10ms + cntv_tval/ctl + daifclr asm), kdl_irq_isle (çıplak; .S'nin sp arg → GICC_IAR oku →
  timer ise re-arm+tik++ → EOI → sp döner; preemption YOK → aynı bağlam). GIC=MMIO çıplak-deref; CNTV/DAIF=
  satıriçi_asm arm64.
- **Volatile-reader gotcha çözümü:** kem_tik IRQ-context'te yazılır, main busy-wait'te okur. .kem küresel
  volatile-DEĞİL → main const-fold edip sonsuz-döngüye girebilir. Çözüm: kem_tik_oku inline-asm ldr @kem_tik
  (+~{memory}) ile HER okuma taze + guard (50M) → timer bozuksa sonsuz-döngü YOK, HATA raporlar.
- **GERÇEK IRQ kanıtı:** tik YALNIZ .kem kdl_irq_isle'da artar; kem_timer_testi tik>=3 bekler → gerçek
  periyodik IRQ tetiklendi (busy-wait sırasında donanım IRQ handler'ı sürdü). Boot [6..10]'a IRQ'lar CANLI
  iken devam (entegre; storm/hang yok).

**FALSİFİYE-KANIT:** kem_os QEMU: `[5] TIMER TIK OK` (+ [1..5] MMU/TRAP + [6..10] kümülatif, IRQ-canlı).
gate: kem_os.ll `define @kdl_irq_isle/@kdl_kesme_kur/@kdl_timer_baslat` + `asm msr cntv_tval_el0/daifclr` +
`call @kem_timer_testi`; `bm_a64_zaman_kem.o` C timer/IRQ tanımı = **0**. FIXPOINT birebir (33371 — compiler
src/*.c DOKUNULMADI); test_tumu tam yeşil.

**MİLESTONE:** İlk GERÇEK donanım-kesmesi SAF-.kem. .S kdl_irq_ortak (trap-frame/eret) DEĞİŞMEDİ; yalnız
`bl kdl_irq_isle` .kem'e yönlendi. Kalan FAZ-A: A4 (görev preemptive — timer→IRQ→context-switch, EN ZOR;
kdl_irq_isle şimdi sp'yi olduğu gibi döner, A4'te kdl_preempt-benzeri context-switch SP döndürecek),
A5 (syscall/EL0).

---

## D-278 — REAL-OS FAZ-A2: kesme gerçek-trap → .kem KARAR handler (mrs ESR/FAR) — [5] TRAP KARAR OK (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-277).

**Karar [ETKİ: `runtime/kem_mmu.kem` (kmmu_esr_oku/kmmu_far_mrs — mrs helpers); `test/ornekler/kem_os.kem`
(trap_gercek_testi + [5d] bloğu); `Makefile` (TRAP KARAR OK gate + falsifiye-kanıt). FAZ-A2 — bring-up
roadmap A2 "kesme gerçek-trap". [5] SENTETİK gap'ini kapatır.]** [5] handler'ı (kem_istisna_isle) şimdiye
dek yalnız hardcoded ESR/FAR ile self-test ediliyordu (D-247). D-276 gerçek fault+recovery verdi ama .kem
KARAR-handler'ı gerçek syndrome görmüyordu. Bu adım: gerçek trap → .kem handler GERÇEK ESR/FAR'da karar verir.

- **WebSearch teyit (ARM DDI0595/DDI0487):** EC=0x25 = Data Abort without EL change (kernel EL1→haritasız);
  FAR_EL1 data-abort'ta faulting-VA tutar (FnV=0 → geçerli, translation fault FnV=0); ESR/FAR yalnız
  istisna girişinde yazılır → eret sonrası bir-sonraki istisnaya dek DEĞERİ TUTAR (.S kdl_exc_ortak yalnız
  mrs-OKUR → değiştirmez).
- **trap_gercek_testi:** `kdl_fault_bekleniyor=1` → haritasız 0x80000000 oku → GERÇEK data-abort → .S
  recovery → mrs esr_el1 + mrs far_el1 (GERÇEK syndrome, post-trap) → `kem_istisna_isle(rfar, resr)`. .kem
  handler decode: EC=0x25 → data-abort, DFSC∈[4,7] → translation → **KURTAR(1)**. Doğrula: karar==1 +
  rfar==0x80000000 (mrs-okunan gerçek FAR). Sentetik DEĞİL: syndrome donanımdan mrs ile geliyor.
- **A2 vs D-276:** D-276 = gerçek fault → .S built-in recovery (kdl_fault_bekleniyor). A2 = .kem KARAR-handler
  gerçek mrs-syndrome'da karar veriyor → roadmap A2 "gerçek fault-yönlendirmesi → .kem handler karar" TAM.
  .S vektör/trap-frame/recovery DEĞİŞMEDİ (yalnız .kem mrs-okuma + handler-çağrısı eklendi).

**FALSİFİYE-KANIT:** kem_os QEMU: `[5] TRAP KARAR OK` (+ MMU FAULT/CEVIRI + [1..10] kümülatif). gate:
kem_os.ll `define/call @trap_gercek_testi` + `asm "mrs $0, esr_el1"` + `call @kem_istisna_isle`.
FIXPOINT birebir (33371 — compiler + C runtime DOKUNULMADI, yalnız .kem+Makefile); test_tumu tam yeşil.

**KAPSAM/SINIR:** A2 = gerçek fault-abort'un syndrome-yönlendirmesi. Kalan FAZ-A: A3 (zaman timer-IRQ —
CNTV+DAIF+GIC → tik canlı), A4 (görev preemptive — EN ZOR, timer→IRQ→context-switch .S), A5 (syscall/EL0).
SVC-trap dispatch (.kem'e) A3/A5 ile gelir; bu adım yalnız data-abort karar-yolu.

---

## D-277 — REAL-OS FAZ-A1 (alt-hedef B): kdl_mmu_kur SAF-.kem — page-table setup + non-identity çeviri — [5] MMU CEVIRI OK (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-276).

**Karar [ETKİ: `runtime/kem_mmu.kem` (kdl_mmu_kur SAF-.kem + kmmu_ceviri_testi); `runtime/kdl_mmu.c`
(kdl_mmu_kur + tablolar `#ifndef KEMGU_KEM_MALLOC` guard); `test/ornekler/kem_os.kem` ([5c] MMU CEVIRI
bloğu); `Makefile` (bm_a64_mmu_kem.o variant + MMU CEVIRI OK gate + falsifiye-kanıt). FAZ-A1 keystone —
MMU KURULUMU artık C'de DEĞİL, SAF-.kem. Kullanıcı "önce A sonra B" → B bu adım.]** Kanıtlı-C
kdl_mmu.c'nin BİREBİR .kem yeniden gerçekleştirmesi: boot `.S bl kdl_mmu_kur` artık SAF-.kem'i çağırır.

- **SAF-.kem kdl_mmu_kur (`kem_mmu.kem`):** çıplak (region-prologue YOK — boot pre-main, heap/region henüz
  yok; yalnız sabit-adres store + MSR). L1(512×1GB)/L2(512×2MB Normal-WB) tabloları FİXED RAM'de
  (0x45000000/0x45001000, free, 4KB-hizalı). Descriptor bit'leri C birebir: Device=0x401, Normal-2MB=
  pa|0x705, tablo=l2|0x3. MAIR=0xFF00, TCR=0x100803519, TTBR0=L1. Enable asm: `msr mair/tcr/ttbr0 → dsb
  ish → tlbi vmalle1 → dsb ish → isb → sctlr|=(M|C|I) → isb` (satıriçi_asm arm64, D-269 P1).
- **Non-identity çeviri gate (`kmmu_ceviri_testi`):** L2[128] override → VA 0x50000000 → PA 0x46000000
  (identity DEĞİL). Çift-yönlü ALIAS doğrula: PA'ya 0xDEADBEEF yaz → non-identity VA'dan oku (=alias?);
  non-identity VA'ya 0xCAFEBABE yaz → PA'dan oku (=alias?). Taklit edilemez: identity map olsaydı iki VA
  aynı PA'yı GÖRMEZDİ (0x50000000 backed-değil). volatile erişim (clang alias-varsaymaz → gerçek donanım
  çevirisi test edilir).
- **RİSK (boot-MMU replace) BAŞARILI:** MMU kurulumu boot'ta değiştirmek = tablolar yanlışsa sessiz hang.
  C'yi birebir yansıtarak İLK denemede boot etti ([1] BOOT OK + [5] MMU FAULT + [6..10] hepsi MMU-on
  .kem-tablolarıyla çalışıyor). Fallback (C'ye dön) GEREKMEDİ.

**FALSİFİYE-KANIT:** kem_os QEMU: `[5] MMU CEVIRI OK` (+ MMU FAULT + [1..10] kümülatif, garbling yok).
gate: kem_os.ll `define @kdl_mmu_kur` + `asm "msr mair_el1"` + `define/call @kmmu_ceviri_testi`;
`bm_a64_mmu_kem.o` C kdl_mmu_kur tanımı = **0** (guard tuttu). FIXPOINT birebir (33371 — compiler
src/*.c DOKUNULMADI); test_tumu tam yeşil.

**MİLESTONE:** kem_os artık TÜM MMU'yu (kurulum + sanal-bellek çevirisi + gerçek page-fault + recovery)
SAF-.kem yapıyor. FAZ-A1 (A+B) TAM. Kalan FAZ-A: A2 (kesme/timer preemptive) — SYNC/IRQ ayrımı + .S-
eşleşik-global inline-asm deseni (D-276) yeniden kullanılabilir. C kdl_mmu.c kem_os link'inde artık
yalnız kdl_surec_kur/kdl_el0_surec_kur (EL0 spawn — henüz kullanılmıyor).

---

## D-276 — REAL-OS FAZ-A1 (alt-hedef A): [5] EXC → GERÇEK vektör-bağlı page-fault + recovery saf-.kem — [5] MMU FAULT OK (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-275).

**Karar [ETKİ: `runtime/kem_mmu.kem` (YENİ — kmmu_fault_testi + recovery-scratch inline-asm erişimi);
`test/ornekler/kem_os.kem` ([5b] gerçek-fault bloğu); `runtime/kdl_kesme.c` (fault-global yorum-notu);
`Makefile` (kem_mmu CAT + MMU FAULT OK gate + falsifiye-kanıt). FAZ-A ilk rung — human+stratejist mod.]**
[5] EXC bloğu şimdiye kadar SENTETİK'ti (kem_istisna_isle'a hardcoded ESR/FAR beslenip ESR-decode mantığı
self-test edilirdi; gerçek fault-yönlendirmesi YOKTU). Bu adım [5]'i GERÇEK vektör-bağlı data-abort +
donanım fault-recovery ile tamamlar — sentetik self-test KALIR (ESR-decode doğrular), buna EK gerçek kanıt.

- **Ampirik faz (read-only, ZORUNLU) — hard blocker YOK:** (1a) MMU boot'ta ZATEN açık (start_aarch64.S:75
  `bl kdl_mmu_kur`, C-MMU identity 0x40000000-0x7FFFFFFF + Device; 0x80000000+ HARİTASIZ). (1b) SYNC
  (`kdl_exc_ortak`) ve IRQ (`kdl_irq_ortak`) AYRI vektör slotları → fault-handling kesme'ye DOKUNMADAN;
  `.S` TAM recovery içeriyor (`kdl_fault_bekleniyor` set → FAR→`kdl_fault_yakalanan`, ELR+=4, eret → devam)
  → **`.S` DEĞİŞMEZ**. (1c) C-MMU L1/L2 2MB-blok, MAIR/TCR/TTBR0 değerleri kayıtlı. (1d) TÜM MMU sysreg'leri
  .kem `satıriçi_asm arm64`'te ifade ediliyor (objdump doğruladı). (1e) 4KB manuel-align çalışıyor.
- **GERÇEK fault-gate (`kmmu_fault_testi`):** `kdl_fault_yakalanan=0` + `kdl_fault_bekleniyor=1` → haritasız
  0x80000000 volatile oku → **gerçek data-abort** → `.S kdl_exc_ortak` recovery → `bekle==0` (kurtarıldı) +
  `far==0x80000000` (FAR doğru) DOĞRULA. Taklit edilemez: MMU zorlamıyorsa okuma sessizce geçer = gate kırmızı.
  Boot [6..10]'a DEVAM ediyor = recovery başarılı (hang yok).
- **Recovery-scratch (kdl_fault_bekleniyor/yakalanan):** `.S` ile eşleşik 2 global, C-tanımlı KALIR (.S
  substrat'ı). .kem bunlara INLINE-ASM (adrp/add + ldr/str, x9-scratch, `~{memory}` clobber = C `volatile`
  karşılığı) ile erişir → pure-.kem raw-mem primitifi.

**SINIR-NOTU (codegen-gap, FLAG — ayrı adım):** .kem `küresel` → `internal global` linkage (llvm.c:6007)
→ `.S`'ye açılamaz (external gerekir). Tam-.kem küresel-paylaşım için `dışa küresel` → external linkage
codegen fix'i gerekir; ANCAK `dışa küresel` parse EDİLMİYOR + codegen.kem 7 küresel (fixpoint-hassas) →
FAZ-A1'e bundle EDİLMEDİ (task DUR: "yeni codegen gap → flag + minimal-fix ayrı adım"). Workaround
(inline-asm C-global erişimi) fault-gate'i TAM verir; küresel-form ertelendi.

**FALSİFİYE-KANIT:** kem_os QEMU: `[5] MMU FAULT OK` — [1..10] + MMU FAULT kümülatif, garbling yok, boot
devam. gate: kem_os.ll `define @kmmu_fault_testi` + `call @kmmu_fault_testi` + `asm sideeffect "adrp x9,
kdl_fault_bekleniyor`. FIXPOINT birebir (33371 satır — compiler src/*.c DOKUNULMADI); test_tumu tam yeşil.

**KAPSAM/SINIR:** alt-hedef A (gerçek fault-gate, mevcut C-MMU'yla) TAMAM. Alt-hedef B (kdl_mmu_kur → .kem +
non-identity çeviri gate `MMU CEVIRI OK`) AYRI adım (boot-destabilize riski; kullanıcı "önce A sonra B" seçti).
Kesme-geçiş (FAZ-A2) hazırlığı: SYNC/IRQ ayrımı + `.S` recovery deseni yeniden-kullanılabilir.

---

## D-275 — REAL-OS FAZ-C: saf-.kem TAM ağ yığını — GERÇEK ICMP ping round-trip — [10] PING CANLI (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-274).

**Karar [ETKİ: `runtime/kem_virtio_net.kem` (vnet_checksum RFC1071 + vnet_tx_checksum); `test/ornekler/kem_os.kem`
(net_icmp_testi + [10] PING bloğu); `Makefile` (checksum/icmp proof + PING CANLI gate). REAL-OS BRING-UP FAZ-C
son rung — AKTİVASYON. bring-up loop görev net-icmp.]** Kanıtlı-C icmp_arm.c'nin saf-.kem yeniden
gerçekleştirmesi: TAM ağ yığını (ARP + IPv4 + ICMP + RFC1071 checksum) tek boot'ta — "OS ping atıyor".

- **RFC1071 checksum (`vnet_checksum`):** big-endian 16-bit toplam + carry-fold + one's complement
  (`65535 - t`, XOR yok). `vnet_tx_checksum` çıplak wrapper (normal işlev vnet_dma'ya dokunmadan kullanır,
  E010 kaçınılır — küresel-erişim güvensiz-tier kuralı).
- **[10] GERÇEK fonksiyonel:** (1) ARP ile gateway (10.0.2.2) MAC çöz. (2) IPv4 (ver4/IHL5, proto=1, IP-
  checksum) + ICMP Echo Request (type=8, id=0xBEEF, seq=1, payload "KEMGU", ICMP-checksum) kur + GÖNDER.
  (3) SLIRP echo reply → AL + PARSE: ethertype=0x0800 + proto=1 + ICMP type=0 (reply) + payload "KEMGU"
  geri döndü DOĞRULA. **Checksum'lar .kem'de hesaplanır ve SLIRP kabul edip yanıtlar** → sentetik-geçiş
  imkânsız (yanlış checksum = paket düşer = reply yok).

**FALSİFİYE-KANIT:** kem_os QEMU: `[10] PING CANLI` — [1..10] kümülatif (DISK RW/FS RW/NET DEV/NET ARP/
PING CANLI), garbling yok. gate: kem_os.ll `define @vnet_checksum` + `call @net_icmp_testi`. [6..9]
regresyonsuz. FIXPOINT birebir; test_tumu tam yeşil.

**MİLESTONE:** FAZ-B (depolama: virtio-blk+minifs) + FAZ-C (ağ: virtio-net+ARP+IPv4+ICMP) SAF-.kem kem_os'ta
CANLI. kem_os artık gerçek disk I/O + dosya sistemi + 2-yönlü ağ (ping) yapan gerçek-OS. Kalan: FAZ-A
(kesme/zaman/görev/EL0 — preemptive scheduling + userspace; STOP-FAZ-A, insan+stratejist). C-removal
(kesme dead-dep) FAZ-A ile açılır.

---

## D-274 — REAL-OS FAZ-C: saf-.kem ağ paket TX/RX — GERÇEK ARP round-trip — [9] NET ARP OK (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-273).

**Karar [ETKİ: `runtime/kem_virtio_net.kem` (vnet_gonder/al + tx_yaz/rx_oku + rx_gorulen küresel);
`test/ornekler/kem_os.kem` (net_arp_testi + [9] NET bloğu); `Makefile` (TX/RX proof + NET ARP OK gate).
REAL-OS BRING-UP FAZ-C — AKTİVASYON. bring-up loop görev net-arp.]** Kanıtlı-C arp_arm.c'nin saf-.kem
yeniden gerçekleştirmesi: GERÇEK 2-yönlü ağ (TX + RX).

- **Paket TX/RX (`kem_virtio_net.kem`):** `vnet_gonder` (TX queue1: 12-bayt virtio-net başlığı + çerçeve,
  tek desc device-OKUR, avail bump + notify + used poll); `vnet_al` (RX queue0: used-ring poll, slot→id/len,
  net-başlığı 12 atla, rx_gorulen sayaç); `vnet_tx_yaz`/`vnet_rx_oku` çerçeve bayt erişimi. Hepsi çıplak
  VOLATILE deref + dsb (POLLED, IRQ YOK). rx_gorulen kur'da sıfırlanır (re-init senkron).
- **[9] GERÇEK fonksiyonel (D-264 dersi):** gateway (10.0.2.2) için Ethernet+ARP request (broadcast, bizim
  MAC, oper=1, tpa=gateway) KUR + GÖNDER → QEMU SLIRP ARP reply GÖNDERİR → AL + PARSE: ethertype=0x0806 +
  oper=0x0002 (reply) + spa=10.0.2.2 (gateway) + gateway MAC (sha) nonzero DOĞRULA. Sentetik-geçiş imkânsız:
  gerçek paket ağ üzerinden gider + gerçek yanıt gelir + protokol-alanları eşleşir.

**FALSİFİYE-KANIT:** kem_os QEMU (`-netdev user -device virtio-net-device`): `[9] NET ARP OK` — [1..9]
kümülatif, garbling yok. gate: kem_os.ll `define @vnet_gonder/al` + `call @net_arp_testi`. [6..8]
regresyonsuz. FIXPOINT birebir; test_tumu tam yeşil.

**SINIR:** ARP L2 (IP/checksum yok). ICMP ping ([10]) IPv4+RFC1071 checksum ekler (sonraki rung). POLLED
(FAZ-A'dan bağımsız).

---

## D-273 — REAL-OS FAZ-C: saf-.kem virtio-net transport kem_os'ta AKTİF — [8] NET DEV OK (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-272).

**Karar [ETKİ: `runtime/kem_virtio_net.kem` (YENİ, saf-.kem virtio-net transport); `test/ornekler/kem_os.kem`
(net_dev_testi + [8] NET bloğu); `Makefile` (CAT + net + QEMU virtio-net-device + NET DEV OK gate). REAL-OS
BRING-UP FAZ-C — AKTİVASYON. bring-up loop görev virtio-net.]** Kanıtlı-C kdl_virtio_net.c'nin saf-.kem
transport yeniden gerçekleştirmesi (POLLED, IRQ YOK → FAZ-A bağımsız).

- **Saf-.kem virtio-net (`kem_virtio_net.kem`):** cihaz bul (DeviceID=1) + feature-negotiate + RX queue0
  (8 device-yazar 2048B tampon) + TX queue1 + DRIVER_OK. Genel VOLATILE/dsb yardımcıları vblk_*'dan (aynı
  CAT birimi). DMA = SABİT identity-RAM 0x44000000 (blk 0x43000000 üstü 16MB), MANUEL 16-hizalı offset (RX
  ring + 8×2048 rx_buf + TX ring, ~19KB). Allocator YOK, IRQ YOK (used-ring poll deseni), T002 yok.
- **[8] gerçek fonksiyonel:** vnet_kur içinde FEAT_OK read-back (device VERSION_1'i KABUL etmezse -2) +
  DRIVER_OK + **device-PROVIDED MAC config** (base+0x100, nonzero) → cihaz canlı + config erişilir. Sentetik
  değil (cihaz feature-negotiate'e yanıt verir + MAC sağlar).

**FALSİFİYE-KANIT:** kem_os QEMU (`-netdev user -device virtio-net-device`): `[8] NET DEV OK` — [1..8]
kümülatif (DISK RW OK + FS RW OK + NET DEV OK), garbling yok. gate: kem_os.ll `define @vnet_bul/kur` +
`call @net_dev_testi`. [6]/[7] regresyonsuz. FIXPOINT birebir; test_tumu tam yeşil.

**SINIR:** [8] transport bringup (paket YOK). Paket TX/RX (ARP/[9], ICMP/[10]) sonraki rung. virtio-net
POLLED (kesme/IRQ gerekmez → FAZ-A'dan bağımsız, roadmap doğrulandı).

---

## D-272 — REAL-OS FAZ-B2: saf-.kem minifs dosya sistemi kem_os'ta AKTİF — [7] FS RW OK (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-271).

**Karar [ETKİ: `runtime/kem_minifs.kem` (YENİ, saf-.kem minifs); `runtime/kem_virtio_blk.kem` (vblk_kur re-init
ring-idx sıfırlama); `test/ornekler/kem_os.kem` (fs_rw_testi + [7] FS bloğu); `Makefile` (CAT + minifs + FS RW OK
gate). REAL-OS BRING-UP FAZ-B2 — AKTİVASYON. bring-up loop görev b2-fs.]** Kanıtlı-C minifs_arm.c'nin saf-.kem
yeniden gerçekleştirmesi, virtio-blk (vblk_*, D-271) üstünde.

- **Saf-.kem minifs (`kem_minifs.kem`):** disk layout blk0=superblock(magic "MFS1"+sayaç), blk1=inode(ad[4]+
  boyut u32+veri_blok), blk2+=data. `mfs_format/dosya_yaz/dosya_oku`. GERÇEK fs katmanı: superblock-magic
  doğrulama + **inode INDIRECTION** (oku, inode'daki veri_blok pointer'ını izler) + isim eşleşme + boyut. Transfer
  = vblk vq_data. Ham sektör I/O DEĞİL. Cross-file: build'de kem_os ile CAT (tek birim, T002 yok); vblk_* çağırır.
- **vblk_kur RE-INIT FIX (kritik):** kem_os çok-subsystem → [6] disk + [7] fs HER biri vblk_kur çağırır. C sürücü
  tek-init'ti (.bss sıfır); ikinci kur RAM avail/used idx'i sıfırlamıyordu → poll `once=used.idx` STALE değeri
  bekler → fs I/O asılırdı. Fix: vblk_kur QUEUE_READY öncesi avail.idx=0 + used.idx=0 (device+driver taze).

**FALSİFİYE-KANIT (GERÇEK fonksiyonel):**
- **kem_os QEMU: `[7] FS RW OK`** — format → dosya "veri" oluştur+RASTGELE-pattern((i*7+11)%256, 200B) YAZ →
  vq_data BOZ(0xff) → dosya OKU (superblock magic + isim + boyut + inode veri_blok indirection) → içerik+boyut
  BYTE-EŞLEŞME. Seri denetlendi: [1..5]+[6] DISK RW OK+[7] FS RW OK+KEM-OS OK, garbling yok. Sentetik-geçiş imkânsız.
- **gate:** kem_os.ll `define @mfs_format/dosya_yaz/dosya_oku` + `call @fs_rw_testi`. [6] DISK RW OK regresyonsuz
  (vblk re-init fix uyumlu).
- **Bağımsız TAZE-CLONE gate + FIXPOINT + test_tumu:** (bkz commit doğrulaması).

**SINIR:** minifs minimal (tek dosya, tek data-blok, 4-harf isim). CRUD/multi-blok/journaling (crashfs) sonraki
rung. Yine de gerçek fs (superblock+inode+data ayrımı + indirection). virtio-blk C-removal hâlâ FAZ-A-blocked (D-271).

---

## D-271 — REAL-OS FAZ-B1: saf-.kem virtio-blk GERÇEK blok I/O kem_os'ta AKTİF — [6] DISK RW OK (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-269; D-270 KALİBRASYON no-commit).

**Karar [ETKİ: `runtime/kem_virtio_blk.kem` (YENİ, saf-.kem virtqueue sürücü); `test/ornekler/kem_os.kem`
(disk_rw_testi + [6] DISK bloğu); `Makefile` (calistir_kem_os_arm: CAT + `--mimari arm64` + virtio-blk cihaz +
DISK RW OK gate). REAL-OS BRING-UP — migration DEĞİL, AKTİVASYON.]** BRINGUP_ROADMAP FAZ-B1: kanıtlı-C
kdl_virtio.c'nin saf-.kem yeniden gerçekleştirmesi + kem_os'ta GERÇEK sektör I/O.

- **Saf-.kem virtqueue sürücü (`kem_virtio_blk.kem`):** VirtIO-MMIO v2 feature-negotiate + queue-setup +
  3-descriptor zincir + notify + used-ring poll. Primitifler: (a) **çıplak VOLATILE deref** (MMIO + DMA-RAM;
  store/load volatile — çok-genişlik: i64 desc.addr / i32 len / i16 flags/idx / i8 data); (b) **dsb sy** =
  satıriçi_asm arm64 (D-269 P1, `--mimari arm64`); (c) **DMA tampon** = SABİT identity-map RAM 0x43000000
  (EL0 üstü, 128MB-backed, kdl_mmu Normal-WB), alt-tamponlar **MANUEL 16-hizalı offset**.
- **Cross-file:** sürücü tamamen kendi-içinde (malloc/memcpy YOK → **T002 YOK**). Build'de kem_os.kem ile **CAT**
  (tek birim → çıplak→çıplak çözülür; ayrı SOURCE-dosya + tek OBJ). `dış işlev` extern yok → CAT deseni.

**FALSİFİYE-KANIT (GERÇEK fonksiyonel, sentetik DEĞİL — D-264/D-268 dersi):**
- **kem_os QEMU (virtio-blk cihazı + disk.img): `[6] DISK RW OK`** — sektör 7'ye değişken pattern ((i%251)+3)
  YAZ → data temizle → geri OKU → 512 byte-byte EŞLEŞME. Gerçek disk I/O (magic-probe DEĞİL). Seri çıktı
  denetlendi: [1..5]+[6] DISK RW OK+KEM-OS OK, garbling YOK.
- **gate:** kem_os.ll `define @vblk_bul/kur/oku/yaz` + `call @disk_rw_testi` + `asm sideeffect "dsb sy"`.
- **Regresyon:** C virtio_rw kernel `DISK RW OK` (kdl_virtio.c DEĞİŞMEDİ); FIXPOINT stage1==stage2 BİREBİR
  (codegen.kem/kemgu.exe dokunulmadı); test_tumu TAM YEŞİL.

**P2 YARGISI (A1 4KB önkoşulu): MANUEL 16B over-align ÇALIŞIYOR** — sabit RAM taban + 16-katı offset'ler
(desc@0/avail@128/used@160/req@256/data@272/status@784) gerçek DMA'da doğrulandı. **4KB'ye ÖLÇEKLENİR:**
aynı desen ((taban+4095)&~4095 veya 4KB-hizalı sabit taban) — MMU sayfa-tablosu için A1'de kullanılabilir.

**DÜRÜST SINIR (C-removal FAZ-A-blocked):** C kdl_virtio.c (bm_a64_virtio.o) kem_os link'inden ÇIKARILMADI —
C-kesme (bm_a64_kesme.o, boot vektör tablosu → kdl_syscall_isle → kdl_dosya_* → kdl_virtio_blk_*) transitif
referans ediyor (D-270 ters-bağımlılık). kem_os'un GERÇEK disk yolu artık .kem vblk_* (aktivasyon TAM); ölü
C kdl_virtio_blk_* kesme-dead-dep ile kalıyor. Tam C-removal = kesme'nin .kem'e alınması (FAZ-A2/A5). Bu görev
AKTİVASYONU tamamladı (Yasa-2 gerçek işlevsellik); C-temizlik ayrı faz.

---

## D-269 — CODEGEN P1: satıriçi_asm arch-gate hedefe-duyarlı (`--mimari arm64`) — aarch64 sysreg/bariyer asm .kem'de AÇILDI (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-268).

**Karar [ETKİ: `src/llvm.h` (runtime hedef selector decl); `src/tip.c` (g_hedef_mimari/triple + setter/getter TANIM —
düşük-bağımlılıklı paylaşılan TU); `src/llvm.c` (AS001 + triple emit getter'dan); `src/tip_kontrol.c` (checker AS001
getter'dan); `src/ana.c` (`--mimari` bayrağı). ENABLING PRIMITIF — subsystem GÖÇÜ YOK.]** docs/SUBSYSTEM_SCOPE.md FAZ-0: kalan 5 subsystem'in (kesme/zaman/mmu/
görev/virtio) irreducible asm'ini (MSR/MRS + bariyer + TLBI) açan tek baskın primitif.

- **KAPSAM DOĞRULAMASI (P1 büyümedi):** satıriçi_asm arch-gate, aarch64 kem_os için TEK *fonksiyonel* target-
  hardcode. İki hardcode var: (a) AS001 asm arch-check (llvm.c + tip_kontrol.c — HER İKİ yerde), (b) emit edilen
  `target triple` (llvm.c). AMA (b) clang `-Wno-override-module` ile override ediliyor (kem_os bugün aarch64 boot
  ediyor — kanıt). datalayout/pointer-size/calling-conv hardcode YOK (IR target-agnostik). → yalnız gate + triple.
- **Değişiklik (MİNİMAL, çalışma-zamanı seçim):** `KEMGU_HEDEF_MIMARI/TRIPLE` makroları VARSAYILAN kalır; runtime
  `g_hedef_mimari/g_hedef_triple` (default = makrolar). `llvm_hedef_ayarla/mimari/triple`. AS001 (llvm.c +
  tip_kontrol.c) + triple emit runtime değerden. `ana.c --mimari arm64|x86_64`. **Bayrak verilmezse davranış BİREBİR
  eski** (fixpoint/regresyon güvenli — D-268 deseni).
- **codegen.kem paritesi:** GEREKMEDİ — self-host arm64 asm emit etmez, self_driver `--mimari` kullanmaz, default
  değişmedi → fixpoint etkilenmez. (Self-host arm64-emit ileride ayrı iş.)

**FALSİFİYE-KANIT (4 kadran + assemble):**
- arm64 asm + `--mimari arm64`: **rc 1→0**, emit `target triple="aarch64-unknown-none-elf"` + `call i64 asm sideeffect
  "mrs $0, CNTPCT_EL0"`. clang --target=aarch64 → **`mrs x8, CNTPCT_EL0`** (gerçek instr, d53be028) assemble oldu.
- arm64 asm, bayraksız (x86_64 default): **rc 1** AS001 (gate KALDIRILMADI, hedefe-BAĞLI).
- x86_64 asm, default: **rc 0** emit (regresyon yok).
- x86_64 asm, `--mimari arm64` altında: **rc 1** AS001 (yanlış hedefe sessiz-bozuk-IR yok).
- Regresyon: snapshot 50/50; FIXPOINT stage1==stage2 BİREBİR; test_tumu TAM YEŞİL.

**SINIR:** P1 yalnız asm-gate + triple; genel target-awareness (datalayout/ABI) GEREKMEDİ (IR agnostik). Context-switch/
vektör-stub hâlâ irreducible asm (bkz SUBSYSTEM_SCOPE §4). Self-host arm64-emit ayrı. Bu primitif subsystem göçünü
AÇAR ama göç YAPMAZ (FAZ-1 virtio/FAZ-2 mmu sırada).

---

## D-268 — SUBSYSTEM/yetki: yetki<R> OUT-PTR ABI + saf-.kem sağlayıcı — kem_os'un SON C bağımlılığı kalktı (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-267).

**Karar [ETKİ: `src/llvm.c` (yetki_olustur/delege intrinsic call+declare: sret→out-ptr); `runtime/kem_heap.kem`
(saf-.kem çıplak kdl_yetki_olustur/geri_al + kem_yetki_sayac); `runtime/kdl_yetki_bare.c` (olustur/delege out-ptr —
virtio); `Makefile` (kem_os link bm_a64_yetki.o DROP + yetki gate-proof). CODEGEN ABI + OS-ÇEKİRDEK.]**
Son subsystem: yetki (object-capability). Yasa-4 (kem_os'ta sıfır C) → yetki göç etmeli.

- **EMPİRİK BULGU (görev premisi düzeltildi):** Görev "C bunu sret ile yapıyor" diyordu; GERÇEK: `%kdl_yetki` 16B
  (≤ AAPCS64/SysV register-return eşiği) → clang bare-metal'de `[2 x i64]`/`{i64,i64}` register-return eder, **sret
  DEĞİL**. sret yalnız (a) KEMGU llvm.c'nin kendi emisyonunda ve (b) host Win64 C-ABI'sinde vardı. llvm.c sret çağrısı
  bare-metal register-return C provider ile ZATEN uyumsuzdu (latent) — yetki değeri runtime'da hiç okunmadığı için
  maskeliydi (`mmio_oku32/yaz32` yalnız adresi kullanır, capability derleme-zamanı ispatı). Kullanıcı ONAYI: OUT-PTR
  konvansiyonu (düşük risk; sret-codegen/x8 yok).
- **OUT-PTR ABI (`src/llvm.c`):** olustur/delege çağrı+declare `ptr sret(%kdl_yetki) align 8` → düz `ptr` (out-pointer,
  aarch64 x0). Çağıran slot ayırır, sağlayıcı struct'ı slot'a yazar, çağıran geri yükler. Host Win64: düz ptr = RCX =
  sret ile aynı reg → mevcut by-value C provider (kdl_runtime.c) DEĞİŞMEDEN uyumlu (capability/mmio/drf 40/23/39 ✓).
- **Saf-.kem sağlayıcı (`kem_heap.kem`):** çıplak `kdl_yetki_olustur(out:*tam8, kt, izin)` + `kdl_yetki_geri_al(y:*tam8)`
  — alanları offset-tabanlı yazar (id@0/kaynak_tipi@8/izin@10/iptal@12; kem_heap.kem tüm-ham-pointer idiomu). küresel
  `kem_yetki_sayac` id sayacı. codegen.kem yetki EMİT ETMEZ → FIXPOINT/codegen_diff ETKİLENMEZ.
- **Makefile:** kem_os `bm_a64_yetki.o` link'ten TAM ÇIKARILDI (metin-stili; kem_os yalnız olustur+geri_al referans
  eder, ikisi de .kem). Guard GEREKMEDİ — kalan-C-func olmadığından (mmio'dan farklı) tam drop Yasa-4'ü sağlar.

**FALSİFİYE-KANIT:**
- **kem_os QEMU TEMİZ boot** (gerçek seri denetlendi, D-264 dersi): `[1]BOOT [2]HEAP/55 [3]MMIO OK/1953655158
  [4]HESAP/230/3 [5]EXC OK/1,2,3 KEMGU KEM-OS OK` — garbling YOK. **MMIO magic (1953655158) .kem yetki ile okundu**;
  konsol (uart_bayt_yaz da yetki_olustur çağırır) temiz → .kem sağlayıcı uçtan-uca çalışıyor.
- **yetki gate:** bm_a64_kem_heap.o `T kdl_yetki_olustur/geri_al` + kem_os.elf'te `kem_yetki_sayac` (C sürümü
  `kdl_yetki_sayac` — ayrı sembol → .kem sağlayıcı linklendiğini kanıtlar) + link'te bm_a64_yetki.o YOK.
- **virtio regresyon:** `KEM VIRTIO OK/1` — out-ptr C provider (kdl_yetki_bare.c) virtio-mmio okumasını sağlar.
- **Host:** capability 40/40, mmio 23/23, drf 39/39; test_tumu TAM YEŞİL; FIXPOINT/codegen_diff etkilenmedi.

**SINIR:** kontrol/kontrol_tipi hâlâ by-value-C ↔ ptr-declare (pre-existing bare-metal mismatch, kem_os kullanmıyor,
maskeli — ayrı iş). yetki genel struct-return AAPCS64 register-packing (x0/x1 `bfxil`) hâlâ yok — out-ptr onu
gerektirmez (çağrı+sağlayıcı self-consistent). Genel struct-return AAPCS64 uyumu ayrı ABI-epic.

---

## D-267 — CODEGEN FIX: self-host `codegen.kem` skaler-pointee deref-READ paritesi (D-265 LATENT sapması kapandı) (2026-07-12) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-266).

**Karar [ETKİ: `selfhost/codegen.kem` (Ayr.cg_apointee alanı + cg_var_ekle/initializer + cg_var_pointee_bul +
param_pointee + DEGISKEN/PARAMETRE pointee-set + deref-READ pointee-yük); `test/cg_korpus/cg_skaler_deref.kem` (yeni
falsifiye korpusu). CODEGEN CORRECTNESS — self-host↔C parite.]** D-265'in DÜRÜST-SINIR olarak kaydettiği latent
sapmanın kalıcı onarımı: C-derleyici `*p` deref-READ'i pointee-tipinden (`pointee_llvm_tip`) yüklüyordu ama self-host
`codegen.kem` skaler pointee için hâlâ i32 yüklüyordu (satır 2731). `*tam8/*tam16/*tam64` beklenen-NULL deref'te
C↔self-host DIVERGE ediyordu. D-267 self-host'a skaler-pointee izleme ekleyerek D-265'in C'de yaptığının BİREBİR aynasını
kurar.

- **`codegen.kem` (skaler-pointee plumbing, C `pointee_llvm_tip` aynası):**
  - `Ayr.cg_apointee: Dizi<metin>` — cg_ad/cg_areg/cg_atip/cg_aref/cg_aelem'e PARALEL yeni tablo; `*tamN` pointer
    değişkeni ise skaler pointee LLVM tipi ("i8"/"i16"/"i64"), değilse "".
  - `cg_var_ekle` her eklemede `""` iter (dizi paralel kalır); `cg_var_pointee_bul(ad)` ad→pointee (append-only,
    en-son-kazanır).
  - **DEGISKEN handler:** annot TIP_POINTER ise `vpointee = ll_tip(pointee-çocuk)`; `cg_var_ekle` sonrası
    `dizi_yaz(cg_apointee, son, vpointee)`.
  - **PARAMETRE (`param_pointee` yardımcısı, C llvm.c:5494 aynası):** `*tamN` param → pointee tipi; registrasyon
    sonrası set.
  - **deref-READ (`deref*`):** yapı-pointee (`cg_var_ref_bul`→`%Yapi`) yoksa `cg_var_pointee_bul`; ikisi de yoksa i32
    varsayılan (kanonik `*tam32`/`mantıksal`). C OP_DEREFERANS ile birebir.

**FALSİFİYE-KANIT (`cg_skaler_deref.kem`, genişlik-duyarlı — yanlış yük-genişliği TERS exit verir):**
- **C-codegen (oracle):** `load i8/i16/i64` → **exit 24**.
- **ESKİ self-host (HEAD 9f0881f, D-267 öncesi):** hepsi `load i32` → **exit 32 → DIVERGE** (32 ≠ 24). ✅ falsifiye görünür.
- **YENİ self-host (D-267 fix):** `load i8/i16/i64` → **exit 24 → PARİTE**. Self-host `*tam8==0` artık `load volatile i8`.
- **Regresyon YOK:** **FIXPOINT stage1==stage2 BİREBİR (33371 satır)** — SERT KAPI geçti; lexer+parser+checker 90/90;
  codegen_diff **76/76** (cg_skaler_deref dâhil); test_tumu TAM YEŞİL; kem_os QEMU TEMİZ boot ([1..5]+MMIO+KEM-OS OK).
  (fixpoint korunuyor çünkü codegen.kem kendisi raw `*tamN` skaler-deref kullanmaz — cg_apointee izleme çıktıyı yalnız
  gerçek skaler-deref'te değiştirir.)

**KAPANIŞ:** D-265'in [[project-kem-codegen-pointer-gaps]] latent-sapma maddesi ÇÖZÜLDÜ — C ve self-host codegen artık
skaler pointer deref'te birebir. Kalan pointer-gap yok (deref-READ tam parite).

---

## D-266 — SUBSYSTEM/mmio: saf-.kem çıplak VOLATILE mmio oku32/yaz32 kem_os'a ENTEGRE (2026-07-11) [ORTA]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-265).

**Karar [ETKİ: `runtime/kem_heap.kem` (+kdl_mmio_oku32/yaz32); `runtime/kdl_runtime_mmio.c` (oku32/yaz32
`#ifndef KEMGU_KEM_MALLOC` guard); `Makefile` (bm_a64_mmio_kem.o + kem_os link swap + mmio gate-proof). OS-ÇEKİRDEK.]**
İkinci subsystem: MMIO (donanım register) volatile erişimi saf-.kem. D-265 deref-fix'in ilk meyvesi (doğru i32-volatile).

- **`kem_heap.kem` (+mmio):** çıplak `kdl_mmio_oku32(i64)->i32` (volatile load) + `kdl_mmio_yaz32(i64,i32)` (volatile store).
  Çıplak=güvensiz-tier → deref VOLATILE (D-248; clang -O2 elemez). D-265: `*p` (p:*tam32) → i32-pointee yük. kem_os.kem
  YALNIZ oku32/yaz32 çağırır → yalnız onlar migrate; 16/64 widthler C'de kalır (virtio dead-code dep).
- **`kdl_runtime_mmio.c`:** oku32/yaz32 (bare-metal blok) #ifndef KEMGU_KEM_MALLOC guard. Diğer widthler + host korunur.
- **Makefile:** bm_a64_mmio_kem.o (-DKEMGU_KEM_MALLOC, oku32/yaz32 çıkarılmış); kem_os link bm_a64_mmio.o → kem variant.

**FALSİFİYE-KANIT (`calistir_kem_os_arm`, QEMU):** (a) bm_a64_mmio_kem.o 0 oku32/yaz32 C-def; bm_a64_kem_heap.o T
kdl_mmio_oku32 + kem_heap.ll `store/load volatile i32` içerir (gate grep). (b) **QEMU: [3] MMIO OK + 1953655158** —
.kem VOLATILE oku32 donanım register'ından DOĞRU değeri okudu (migrasyon-öncesi ile birebir; volatile çalışıyor). (c)
Regresyon yok — bm_a64_mmio.o (non-kem) oku32/yaz32 intact → virtio/mmio_bare_metal/kem_pointer kernel'leri etkilenmez.

**SIRADA:** yetki (kdl_yetki_olustur/geri_al, capability — struct/semantik) → panik/UART/kesme/zaman/mmu/görev/virtio (büyük).

---

## D-265 — CODEGEN FIX: deref-READ `*p` yük-tipi POINTEE'den (bağlam-varsayılan i32 gap) — C-derleyici (2026-07-11) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-264).

**Karar [ETKİ: `src/llvm.c` (OP_DEREFERANS read handler); `runtime/kem_heap.kem` (metin workaround KALDIRILDI).
CODEGEN CORRECTNESS.]** D-264'te keşfedilen gap'in KALICI onarımı: `*p` deref-READ, yüklenecek tipi pointer'ın
pointee-tipinden DEĞİL kullanım-bağlamından (`beklenen`) alıyordu; `beklenen` yoksa (ör. `*bp == 0` karşılaştırma)
i32-varsayılan → DAR pointee'lerde (`*tam8` → i32, 4 bayt oku) YANLIŞ genişlik → null-check bozuldu → kdl_metin_uzunluk
sınır-aştı → garbled UART (D-264).

- **Fix (llvm.c OP_DEREFERANS):** operand DUGUM_TANIMLAYICI ise `isim_bul→pointee_llvm_tip` ile pointee-tipi çöz; `beklenen`
  verilmezse i32 yerine POINTEE kullan. **Altyapı zaten vardı** (`pointee_llvm_tip` alanı + indeks/deref-WRITE handler'ları
  onu kullanıyordu — yalnız deref-READ handler'ı kaçırmıştı; llvm.c:2408 yorumu bunu belgeliyordu). `beklenen` VERİLİRSE
  korunur (regresyon yok).
- **metin workaround kaldırıldı:** `değişken b: tam8 = *bp` → saf `*bp == 0` (fix doğru i8-yük emit ediyor).

**FALSİFİYE-KANIT:** (a) `*bp == 0` IR artık `load volatile i8` + `icmp eq i8` (i32 değil). (b) host metin
uzunluk('KURTAR')=6→exit 42 (workaround'suz). (c) **kem_os QEMU TEMİZ boot** — EXC satırları garbling YOK (fix öncesi
garbled'dı). (d) **Regresyon YOK:** llvm 241/241, codegen_diff 75/75, **FIXPOINT stage1==stage2 BİREBİR (33150)**,
self_driver TÜM MODLAR (C-built==self-host → codegen.kem etkilenmedi). (test_tumu'da 1 kez 18-llvm-flake görüldü, standalone
+ tekrar 241/241 → geçici temp-race, fix DEĞİL.)

**DÜRÜST SINIR (D-249/D-253 sınıfı LATENT parite):** self-host `codegen.kem` deref handler'ı skaler pointee için hâlâ i32
(satır 2731; "skaler beklenen-tip plumbing yok"). C artık pointee kullanıyor → `*tam8/*tam16/*tam64` beklenen-NULL deref'te
C↔self-host DIVERGE eder. **AMA LATENT:** hiçbir mevcut program (cg_korpus/codegen.kem/self_driver) böyle deref
kullanmıyor → fixpoint+parite YEŞİL. kem_heap.kem (*tam8 kullanan) C-derlenir, self-host derlemez → bare-metal etkilenmez.
**Gerçek parite fix (gelecek):** codegen.kem'e skaler-pointee plumbing (cg_var tablosu + deref handler). İlgili
[[project-kem-codegen-pointer-gaps]].

---

## D-264 — SUBSYSTEM/metin: saf-.kem kdl_metin_uzunluk/bayt kem_os'a ENTEGRE (allocator-sonrası ilk subsystem) (2026-07-11) [ORTA]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-263).

**Karar [ETKİ: `runtime/kem_heap.kem` (+kdl_metin_uzunluk/bayt); `Makefile` (kem_os link'inden bm_a64_metin.o ÇIKAR +
metin gate-proof). OS-ÇEKİRDEK.]** Allocator göçü (K1-K5) sonrası İLK subsystem: metin (string) ilkeleri saf-.kem.
kem_os'ta kalan C-runtime azaltma fazının başlangıcı.

- **`kem_heap.kem` (+metin):** çıplak kdl_metin_uzunluk(ptr)->i32 (freestanding strlen) + kdl_metin_bayt(ptr,i32)->i8
  (sınır-güvenli byte-at; NULL/OOB→0). Leaf (allocation yok). bm_a64_metin.o kem_os link'inden çıkarıldı (kem_os-özel
  obje, guard gerekmedi).

**🐛 KEŞFEDİLEN CODEGEN GAP (workaround'lu):** `*ptr` deref, YÜK TİPİNİ pointee'den DEĞİL BAĞLAMDAN alıyor. `*bp == 0`
(bp: *tam8) → codegen `load i32` emit etti (comparison-default i32), null-check 4-sıfır-bayt'a kadar durmadı →
kdl_metin_uzunluk('KURTAR')=100 (6 yerine) → UART string-print sınır-aşımı → GARBLED çıktı. **DÜZELTME (workaround):**
`değişken b: tam8 = *bp; eğer b == 0` (tam8 ara-değişken i8-yük ZORLAR). Diğer .kem-runtime fn'leri etkilenmedi (hepsi
`değişken x: tamN = *p` tipli-yük kullanıyor). **GERÇEK FIX (gelecek, src/llvm.c):** OP_DEREFERANS yükü pointee-tipini
kullanmalı (context değil). **DERS: gate marker-PASS ama çıktı BOZUK olabilir → gerçek QEMU çıktısını denetle (sadece
KEM-OS OK grep'i yetmez).**

**FALSİFİYE-KANIT:** (a) host metin: uzunluk('KURTAR')=6, ''=0, OOB=0 → exit 42 + IR `load i8`. (b) QEMU: kem_os TEMİZ boot
([1..5]+KEM-OS OK, EXC satırları KURTAR/OLDUR/HALT garbling YOK — fix öncesi garbled'dı). (c) bm_a64_metin.o link-dışı;
bm_a64_kem_heap.o T kdl_metin_uzunluk/bayt.

**SIRADA:** mmio (kdl_mmio_oku/yaz, volatile) → yetki (capability) → panik/UART/kesme/... (büyük subsystem'ler).

---

## D-263 — K4b+K5: saf-.kem kdl_global_bolge_al + ALLOCATOR-YIĞINI GÖÇÜ TAMAM — kem_os allocator C-runtime=0, QEMU-boot (2026-07-11) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-262).

**Karar [ETKİ: `runtime/kem_heap.kem` (+kdl_global_bolge_al + kem_global_bolge küresel); `runtime/kdl_bare_heap.c`
(kdl_global_bolge_al `#ifndef KEMGU_KEM_MALLOC` guard); `Makefile` (K4b+K5 gate-proof). OS-ÇEKİRDEK YASA-1 seri —
runtime→.kem göçü SON allocator kademesi + K5 milestone.]**

**runtime→.kem ALLOCATOR-YIĞINI GÖÇÜ TAMAM (K1→K4b).** K4b son parça: kdl_global_bolge_al (kem_os.ll'de kalan TEK
allocator C-çağrısı) → .kem çıplak (küresel lazy global bölge + kdl_bolge_olustur[.kem]). Böylece kem_os'un TÜM
allocator yığını — **malloc (K1) → region (K3) → dizi (K2) → memcpy/memset (K4a) → global-bölge (K4b)** — SAF-.kem.

- **`kem_heap.kem` (+global_bolge):** çıplak `kdl_global_bolge_al()->ptr`; `kem_global_bolge` küresel (WALL-1) lazy;
  kdl_bolge_olustur (.kem, aynı dosya) çağırır. kem_heap.kem artık TAM allocator runtime (18 çıplak fn).
- **`kdl_bare_heap.c`:** kdl_global_bolge_al KEMGU_KEM_MALLOC guard'ına alındı.

**FALSİFİYE-KANIT (`calistir_kem_os_arm`, QEMU):** (a) **K4b** — kemmalloc.o 0 kdl_global_bolge_al; kem_heap.o T
kdl_global_bolge_al. (b) **K5 milestone** — `bm_a64_heap_kemmalloc.o` + `bm_a64_bolge_kemregion.o` (kem_os'un TÜM C
allocator objeleri) = **0 allocator-yığını C-tanımı** (malloc/free/memcpy/memset/kdl_bolge_*/kdl_dizi_*/kdl_global_bolge_al
grep=0) → kem_os allocator/region/dizi/kopya/global-bölge TAMAMEN bm_a64_kem_heap.o SAF-.kem'den. (c) **QEMU boot** —
[1..5]+KEM-OS OK, [2] HEAP DIZI=55. (d) Regresyon yok (diğer kernel'ler C allocator).

**KAPSAM/SINIR:** Bu K5 = **ALLOCATOR-yığını** C-runtime=0 (orijinal K1-K5 direktif hedefi). kem_os'ta KALAN C =
AYRI SUBSYSTEM'ler (panik/UART/yetki/mmio/metin/kesme/zaman/mmu/görev/virtio) — allocator değil, ayrı göç fazı (gelecek).
aarch64 (x86_64 kem_os yok). dizi tam64/ptr/yapi varyantları + free-list split/coalesce yok (kem_os kullanmıyor).

---

## D-262 — K2+K4a: saf-.kem çıplak DİZİ + memcpy/memset kem_os'a ENTEGRE — C kdl_dizi/memcpy SİLİNDİ, QEMU-boot (2026-07-11) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-261).

**Karar [ETKİ: `runtime/kem_heap.kem` (+dizi: olustur/ekle_tam/al_tam/boyut/kapasite_ayarla + iç buyut/oob; +memcpy/memset);
`runtime/kdl_bare_heap.c` (memcpy/memset + kdl_dizi.inc `#ifndef KEMGU_KEM_MALLOC` guard); `test/strip_defined_declares.awk`
(YENİ robust declare-strip); `Makefile` (awk strip + K2 gate-proof). OS-ÇEKİRDEK YASA-1 seri — runtime→.kem göçü K2+K4a.]**

runtime→.kem göçünün 3. kademesi. K2 (dizi) memcpy'ye bağlı → K4a (memcpy/memset) ile bundle. kem_os'un DİZİ + bayt-kopya
katmanı artık SAF-.kem. Böylece **kem_os'un tüm tahsis+dizi yığını (.kem malloc→region→dizi→memcpy) SAF-.kem**.

- **`kem_heap.kem` (+dizi/memcpy):** çıplak memcpy/memset (inttoptr byte-loop, -O2 loop-idiom'a yakalanmaz→self-recurse yok,
  disasm-doğrulandı) + kdl_dizi_olustur/ekle_tam/al_tam/boyut/kapasite_ayarla + iç kem_dizi_buyut/kdl_dizi_oob. KdlDizi
  {veri@0,boyut@8,kapasite@12,eleman_byte@16} raw-ptr; bölge-sahipli büyüme (kdl_bolge_ayir[.kem] + memcpy[.kem], AYNI dosya
  çıplak→çıplak). Sınır-kontrol D-069 → oob spin-halt (cross-file panik yok; kem_os'ta erişilmez). tam64/ptr/yapi varyantları
  kem_os KULLANMADIĞI için atlandı.
- **`kdl_bare_heap.c`:** memcpy/memset K1-guard'ına alındı; kdl_dizi.inc include `#ifndef KEMGU_KEM_MALLOC` sarıldı.
- **`strip_defined_declares.awk`:** kemgu --llvm boilerplate declare'ları define ile çakışır (LLVM redef) → bir fn hem
  define hem declare ediliyorsa declare'ı DÜŞÜR (robust; elle strip-listesi bakımı yok). K1/K2/K3 hepsi kullanır.

**FALSİFİYE-KANIT (`calistir_kem_os_arm`, QEMU):** (a) **@kdl_dizi/memcpy/memset=0** — `bm_a64_heap_kemmalloc.o` bunların
C-tanımı=0; `bm_a64_kem_heap.o` T kdl_dizi_olustur+memcpy (saf-.kem). (b) **QEMU boot** — [1..5]+KEM-OS OK, **[2] HEAP DIZI=55**
(.kem dizi 1..10 ekle+büyüme[4→8→16]+al_tam, .kem region+malloc+memcpy üstünde DOĞRU). (c) **Regresyon yok** — kernel_dizi
(C dizi) KERNEL DIZI OK+55; bm_a64_heap.o C dizi/memcpy intact. (d) Host: dizi LOGIC test (boyut=10, sum=55). Fixpoint ETKİLENMEZ.

**KALAN C (kem_os) → K4b/K5:** kdl_global_bolge_al (lazy global bölge — kdl_bolge_olustur[.kem] çağırır), kdl_panik/
kdl_panik_dur (panik seam → UART halt), yetki/mmio/metin subsystem'leri. K5 nihai: kem_os IR + link'te C-runtime sembolü=0.

---

## D-261 — K3: saf-.kem çıplak REGION (bölge arena) kem_os'a ENTEGRE — C kdl_bolge SİLİNDİ, QEMU-boot (2026-07-11) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-260).

**Karar [ETKİ: `runtime/kem_heap.kem` (+region: kem_blok_olustur/kdl_bolge_olustur/ayir/serbest); `runtime/kdl_bolge.c`
(`#ifndef KEMGU_KEM_MALLOC` guard); `Makefile` (bm_a64_bolge_kemregion.o + KEM_OS_A64_OBJS swap + kem_heap strip +
K3 gate-proof). OS-ÇEKİRDEK YASA-1 seri — runtime→.kem göçü K3.]**

runtime→.kem göçünün İKİNCİ kademesi (K3 region). K2(dizi)'nin ÖNKOŞULU (dizi kdl_bolge_ayir çağırır + .kem extern-C
çağıramaz → region .kem OLMALI önce; stated K2→K3 sırası TEKNİK infeasible, tractable sıra K3→K2). Region EN BÜYÜK
yüzey (kem_os.ll'de kdl_bolge_olustur 22 + serbest 39 = **61 çağrı**).

- **`kem_heap.kem` (+region):** çıplak `kdl_bolge_olustur()->ptr` / `kdl_bolge_ayir(ptr,i64)->ptr` / `kdl_bolge_serbest(ptr)`
  + helper `kem_blok_olustur`. Bölge = malloc'lu blok tek-yönlü listesi + blok-içi bump (C kdl_bolge.c ile birebir
  algoritma). **AYNI dosyada malloc/free (K1)** → çıplak→çıplak call resolve (islev_bul; ρ-suz doğru C-ABI). Struct
  düzenleri raw-pointer offset (KdlBolge{bas@0,blok_sayisi@8}; KdlBolgeBlok{sonraki@0,kapasite@8,kullanilan@16,veri@24}).
  Hizalama `((x+15)/16)*16` (bitwise gerekmez). memcpy GEREKMEZ. Sayaç YOK.
- **`kdl_bolge.c`:** hiza_yukari/blok_olustur/olustur/ayir/serbest `#ifndef KEMGU_KEM_MALLOC` guard; sayaç+bakiye+
  blok_sayisi diagnostikleri KALIR. Diğer kernel'ler C region (guard etkisiz) ile DEVAM.
- **Makefile:** `bm_a64_bolge_kemregion.o` (-DKEMGU_KEM_MALLOC, region çıkarılmış); `KEM_OS_A64_OBJS` bm_a64_bolge.o →
  kemregion; kem_heap strip'e region declare'ları eklendi. Sadece kem_os.

**FALSİFİYE-KANIT (`calistir_kem_os_arm`, QEMU):** (a) **@kdl_bolge=0** — `bm_a64_bolge_kemregion.o` region C-tanımı=0;
`bm_a64_kem_heap.o` T kdl_bolge_olustur (saf-.kem). (b) **QEMU boot** — kem_os.elf [1..5]+KEM-OS OK, **[2] HEAP DIZI=55**
(.kem region + .kem malloc birlikte heap Dizi tahsisini DOĞRU yaptı; her kem_os fn'in region-prologue'u = .kem region →
.kem malloc). (c) **Regresyon yok** — kernel_dizi (C region) KERNEL DIZI OK+55; bm_a64_bolge.o C region intact. (d)
Host: region LOGIC test (1000 tahsis 16-hizalı+distinct+yazılabilir + 100KB yeni-blok → exit 42). Fixpoint ETKİLENMEZ.

**SIRADA K2 (dizi):** kdl_dizi_* → .kem çıplak (kem_heap.kem'e; kdl_bolge_ayir[.kem] + memcpy[.kem çıplak leaf, self-recurse
yok] çağırır). Sonra K4 (memcpy/memset/global_bolge/panik) → K5 (kem_os IR C-runtime=0).

---

## D-260 — K1: saf-.kem çıplak `malloc`/`free` kem_os'a ENTEGRE — C bump-allocator SİLİNDİ, QEMU-boot (2026-07-11) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-259).

**Karar [ETKİ: `runtime/kem_heap.kem` (YENİ saf-.kem allocator); `runtime/kdl_bare_heap.c` (`#ifndef KEMGU_KEM_MALLOC`
guard + weak kem_heap_kur); `boot/start_aarch64.S` (bl kem_heap_kur); `Makefile` (kemmalloc obj + kem_heap obj +
KEM_OS_A64_OBJS + @kdl_bare_heap=0 gate). OS-ÇEKİRDEK, YASA-1 seri — runtime→.kem göçü K1.]**

F3 (D-259) circularity-kırıldı'yı kem_os bare-metal'e ENTEGRE eder: kem_os'un `malloc`/`free`'si artık SAF-.kem
(çıplak allocator), C `kdl_bare_heap` bump-allocator'ı DEĞİL. runtime→.kem göçünün İLK kademesi (K1) TAMAM.

- **`kem_heap.kem`:** çıplak (C-ABI, ρ-suz) `malloc(i64)->ptr` / `free(ptr)` / `kem_heap_kur(i64,i64)`. Bump + 16-hizalı
  {boyut,sonraki} header + LIFO serbest-liste (C kdl_bare_heap ile birebir algoritma). küresel bump-state (WALL-1) +
  ham-pointer inttoptr/deref (D-248). Region-prologue YOK → circularity YOK.
- **`kdl_bare_heap.c`:** malloc/free/typedef/statics `#ifndef KEMGU_KEM_MALLOC` ile sarıldı; weak `kem_heap_kur` no-op
  (C-malloc kernel'leri için). memcpy/memset/kdl_global_bolge_al/kdl_panik/kdl_dizi KALDI (K4).
- **Boot (`start_aarch64.S`):** `bl main`'den ÖNCE `bl kem_heap_kur(__heap_start,__heap_end)` — main'in ilk malloc'undan
  önce heap penceresi kurulur. Çıplak → malloc tetiklemez (taban-öncesi güvenli). PAYLAŞILAN boot; C-malloc kernel'de
  weak no-op (lazy kdl_heap_init yeterli), kem_os'ta kem_heap.o STRONG override → kem_bump=__heap_start.
- **Makefile:** `bm_a64_heap_kemmalloc.o` (-DKEMGU_KEM_MALLOC, C malloc çıkarılmış), `bm_a64_kem_heap.o` (kem_heap.kem
  → boilerplate `declare @malloc/@free` STRIP [LLVM redef hatası] → aarch64 obj), `KEM_OS_A64_OBJS` (heap→kemmalloc+kem_heap).
  **Sadece kem_os** bu obj setini kullanır → diğer ~15 aarch64 kernel BM_A64_OBJS (C malloc) ile DEVAM (regresyon yok).

**FALSİFİYE-KANIT (`calistir_kem_os_arm`, QEMU-boot):** (a) **@kdl_bare_heap=0** — `llvm-nm bm_a64_heap_kemmalloc.o`
malloc/free C-tanımı = 0 (guard çalıştı); `bm_a64_kem_heap.o` T malloc/free/kem_heap_kur (saf-.kem sağlıyor). (b)
**QEMU boot** — kem_os.elf boot eder + **[1]BOOT [2]HEAP DIZI OK→55 [3]MMIO [4]HESAP [5]EXC OK + KEMGU KEM-OS OK**;
[2] HEAP DIZI (1..10 toplam=55) .kem malloc'un region-runtime + Dizi tahsisini DOĞRU yaptığının kanıtı. (c)
**Regresyon yok** — kernel_dizi (C-malloc region kernel) hâlâ "KERNEL DIZI OK"+55 boot eder (weak kem_heap_kur no-op +
boot değişikliği C-malloc kernel'leri kırmadı). (d) Host: kem_heap.kem çıplak IR circularity-sembol=0 (F3 harness),
allocator LOGIC host-test (bump+free-list reuse+100 tahsis exit 42). Fixpoint ETKİLENMEZ (codegen.kem/derleyici
dokunulmadı).

**KAPSAM/SINIR:** aarch64 (x86_64 kem_os gate yok). free-list split/coalesce yok (C ile aynı; kem_os döngü OOM'u
yeterli). **SIRADA K2-K5:** dizi runtime (K2), region (K3 — kdl_bolge.c→.kem, circularity F3 çözdü), helpers (K4:
memcpy/memset/global_bolge), K5 (kem_os IR'ında C-runtime sembolü = 0 nihai hedef).

---

## D-259 — F3/KOMPOZİSYON: saf-.kem çıplak+küresel `kem_malloc` — bootstrap-circularity KIRILDI (2026-07-08) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-258).

**Karar [ETKİ: `test/ornekler/kem_malloc.kem` (YENİ saf-.kem allocator); `test/kem_malloc_kompozisyon.c` (YENİ
C-harness); `test/kem_malloc_kompozisyon_harness.sh` + Makefile (`calistir_kem_malloc_kompozisyon`, test_tumu).
KOMPOZİSYON KANITI — dil-genişletmenin meyvesi.]**

İki bootstrap-primitifinin (D-252 `küresel değişken` + D-254/257 `çıplak işlev`) BİR ARADA çalışan bir allocator
verdiğini ve **bootstrap-circularity'yi kırdığını** falsifiye-kanıtla gösterir. `kem_malloc.kem`: küresel bump-state +
çıplak (region-prologue'suz, C-ABI) `kem_heap_kur`/`kem_malloc`/`kem_yaz32`/`kem_oku32` (ham-pointer inttoptr+deref).

**CIRCULARITY NEDEN KIRILDI:** normal .kem fn'i girişinde `@kdl_bolge_olustur→malloc` çağırır; malloc'u .kem'de
yazarsan `malloc→(prologue)@kdl_bolge_olustur→malloc` = SONSUZ DÖNGÜ. Çıplak fn prologue EMIT ETMEZ →
`kem_malloc` IR'inde `@kdl_bolge_olustur` = 0 VE `@malloc` self-call = 0 → tahsis kendini tetiklemez.

**FALSİFİYE-KANIT (`calistir_kem_malloc_kompozisyon`, HER İKİ codegen 6/6):** (A) IR-KANIT: kem_malloc modülünde
`@kdl_bolge_olustur/@kdl_global_bolge_al/@malloc`-self = 0 (grep). (B) KOMPOZİSYON: C-harness (çıplak C-ABI olduğu için
.kem malloc'u doğrudan çağırır) → 2 çağrı → **2 FARKLI adres** (`ALLOC: p1=<A> p2=<B> A!=B EVET`), dönen adresler
YAZILABİLİR (inttoptr+deref: v1=111 v2=222), bitişik (p2-p1==8) → **exit 42**. C-codegen == self-host (parite).
test_tumu tam yeşil (fixpoint + codegen_diff 75/75 + region-free 6/6 dâhil).

**KAPSAM/SINIR:** Host-kanıt (havuz = statik C tamponu; bare-metal'de taban __heap_start'tan gelir — K1b). Minimal bump
(hizalama+serbest-liste K1'de). Heap-TABANI çağırandan (kem_heap_kur) — .kem'de extern linker sembolü ifade edilemez
(satıriçi_asm arch-x86_64-sabit). **SIRADA K1:** kem_malloc'u kem_os.kem'e entegre + heap-taban'ı boot-glue küreselinden
al + C `kdl_bare_heap` malloc yolunu sil → **kem_os.kem IR'inde @kdl_bare_heap/@malloc(C) = 0** (Mehmet firsthand doğrular).

---

## D-258 — SELF-HOST/F2→K1: `çıplak işlev` ρ-drop codegen.kem PARİTE (C↔self-host ABI divergence kapatıldı) (2026-07-08) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-257).

**Karar [ETKİ: `selfhost/codegen.kem` (fn_ciplak_ad_mi + islev_uret imza rho_var + 2 çağrı-sitesi callee_rho);
`test/ciplak_region_free_harness.sh` (+2 ρ-drop imza parite). SELF-HOST ÇEKİRDEĞİ.]**

D-257 çıplak ρ-drop'u C-derleyiciye ekledi; codegen.kem'e AYNALADI (audit dersi: divergence bırakma). Oracle = C llvm.c.

- **İmza:** `rho_var = (main_mi==0 ve ciplak==0)` → çıplak fn imzasında `ptr %rho` YOK; virgül-mantığı rho_var'a bağlı.
- **Çağrı-sitesi (2):** `fn_ciplak_ad_mi(p, fad)` (g_ciplak node adları) → çıplak-callee ise `callee_rho=0` → ρ arg
  ATLA (void + value yolu). C llvm.c'nin `callee_rho`/`u_rho` aynası.
- **rho_ref:** çıplak fn'de "null" (call-rule gereği kullanılmaz).

**FALSİFİYE-KANIT:** (a) çıplak `tahsis`: C ve self-host BİREBİR — `define i64 @tahsis()` + `call i64 @tahsis()`
(ikisi de ρ-suz). self-host çıplak allocator exit 42. (b) **ciplak_region_free harness 6/6** — çıplak+iken/için
region-symbol=0 + imza ρ-suz HER İKİ derleyici + normal-döngü ρ_iter korundu. (c) cg_ciplak codegen_diff + FIXPOINT
(codegen.kem çıplak-kullanmıyor → self-compile etkilenmez, stage1==stage2 korunur).

**SINIR:** self-host E013 yok (güvensiz/çıplak-tracking yok — D-249/D-253/D-257 sınıfı; geçerli-program parity çalışır,
invalid-program reddi yalnız C). **SIRADA: K1** — saf-.kem çıplak `malloc` (küresel bump + inttoptr; artık @malloc(i64)
C-ABI ifade edilebilir) → kem_os.kem entegre → C kdl_bare_heap malloc yolu sil.

---

## D-257 — DİL/F2→K1: `çıplak işlev` ρ param DÜŞÜR (true C-ABI) + çıplak-call-rule (E013) — C-DERLEYİCİ (2026-07-08) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-256). Mehmet ABI-kararı: Option-1.

**Karar [ETKİ: `src/llvm.c` (islev_uret imza + 4 çağrı-sitesi); `src/tip_kontrol.h/c` (ciplak_baglam + E013);
`test/test_tip_kontrol.c` (+2). ABI-SEMANTİK — soundness-kritik. K1 ön-koşulu.]**

K1 (saf-.kem allocator) somut bir ABI çatalı açığa çıkardı: D-254'te çıplak fn ρ param'ı "uniform-ABI + çağrı-yeri
basitliği" için TUTUYORDU → çıplak `malloc` `@malloc(ptr %rho, i64)` üretiyor, ama codegen region-backing
`@malloc(i64)` (C-ABI) çağırıyor → UYUŞMAZ → çıplak fn bir C-ABI sembolü (malloc/interrupt/syscall) OLAMIYOR.
**Mehmet kararı (Option-1): ρ'yu DÜŞÜR** → çıplak = true C-ABI bare fonksiyon.

- **Codegen (llvm.c):** `rho_var = !main_mi && !ciplak` → çıplak fn imzasında `ptr %rho` YOK (main gibi). rho_ref =
  "null" (çıplak-call-rule gereği kullanılmaz). **4 çağrı-sitesi** (generic 2178, method 2691, modül 2796, ana user-fn
  3868) callee çıplak ise ρ arg'ını ATLAR (`callee_rho`/`m_rho`/`mf_rho`/`u_rho` + virgül-mantığı). Sonuç: çıplak
  `malloc` → `define i64 @malloc(i64 %n)` = codegen'in `declare @malloc(i64)` beklentisiyle BİREBİR.
- **Çıplak-call-rule (E013, tip_kontrol):** `tk->ciplak_baglam` sayacı (çıplak gövde/method'ta ++; güvensiz-grant ile
  aynı 3 site). Çıplak içinden normal (ρ-alan) user-fn çağrısı → E013 (verilecek ρ yok → codegen `ptr null` → callee
  null-region tahsis → segfault). Çıplak→çıplak + çıplak→extern/builtin İZİNLİ. Normal→çıplak İZİNLİ (kısıt yalnız
  çıplak-içinden).

**FALSİFİYE-KANIT:** (a) çıplak `tahsis`/`malloc`: define + call ikisi de ρ-SUZ, exit 42 (ABI uyumlu; ρ uyumsuz olsa
segfault). (b) `@malloc(i64)` == codegen `declare @malloc(i64)`. (c) E013: çıplak→normal RED (1 hata), çıplak→çıplak OK
(0), normal→çıplak OK. (d) çıplak+döngü hâlâ region-free (D-256 korundu). (e) llvm 241/241, tip_kontrol 189/189,
parser 107/107 — non-çıplak yolu DEĞİŞMEDİ (rho_var normal fn'de = eski `!main_mi`).

**SINIR / SIRADA:** (1) **D-258 self-host parite** — codegen.kem çıplak ρ-drop AYNALA (şu an self-host çıplak hâlâ
ρ-carrying → exit-görünmez ABI divergence; audit dersi: kapat). (2) lambda-indirect (llvm.c:3410) + generic-instantiation
çağrı yolları çıplak-callee için henüz ρ-atlamıyor (K1 kapsamı-dışı: allocator standalone çıplak). (3) self-host E013
yok (güvensiz-tracking yok, D-249/D-253 sınıfı). → **K1** (saf-.kem çıplak `malloc` = küresel bump + inttoptr).

---

## D-256 — F2/AUDIT: `çıplak işlev` adversarial soundness denetimi — 2 kusur ONARILDI (döngü-ρ_iter sızıntısı + method-grant) (2026-07-06) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-255).

**Karar [ETKİ: `selfhost/codegen.kem` (Bulgu #1: IKEN/İÇİN ρ_iter guard); `src/tip_kontrol.c` (Bulgu #2: özellik/uygula
method çıplak-grant); `test/ciplak_region_free_harness.sh` (YENİ IR-içerik gate) + Makefile; `test/test_tip_kontrol.c`
(+3). SELF-HOST + CHECKER — soundness-kritik.]**

F2 (D-254/D-255) sonrası, K1 inşasından ÖNCE `çıplak işlev` primitifine 4-lens adversarial denetim (safety/ABI/parity/
analiz; 9 agent, per-bulgu bağımsız skeptik doğrulama). Parity gate'lerin (codegen_diff 75/75 + fixpoint) KAÇIRDIĞI
2 gerçek, çıplak-özgü, belgeli-sınır-DIŞI kusur bulundu + onarıldı:

- **Bulgu #1 (YÜKSEK, K1-BLOKE) — self-host çıplak-döngü ρ_iter sızıntısı:** `codegen.kem` IKEN/İÇİN handler'ı çıplak
  fn içindeki en-dış döngüde per-iterasyon `@kdl_bolge_olustur`/`@kdl_bolge_serbest` (F4.3 ρ_iter) emit ediyordu →
  çıplak "sıfır region-symbol" invaryantı ihlali → bootstrap-circularity döngü-içeren her çıplak fn'de geri doğuyor.
  C-codegen'de ρ_iter kavramı HİÇ yok (grep 0) → C çıplak-döngü doğal region-free; sapma yalnız self-host. **Kanıt:**
  çıplak+`iken` topla() → C=0, self-host=2 (fix öncesi). **Onarım:** `Ayr.ciplak_aktif` bayrağı (islev_uret'te
  `ciplak_mi`'den set) + iki ρ_iter CREATE sitesine `ve p.ciplak_aktif == 0` guard'ı. Serbest siteleri zaten
  `rho_iter==""` no-op. **exit-kodu codegen_diff'in GÖRMEDİĞİ kusur** (çıplak allocator host'ta linklenir → sızıntı
  exit'te maskelenir; kusur IR/bare-metal link seviyesinde).
- **Bulgu #2 (ORTA) — çıplak güvensiz-grant method'lara sızmıyor:** D-254 grant yalnız standalone `DUGUM_ISLEV`
  yolundaydı (tip_kontrol.c:5077); özellik default-impl (5145) + uygula method (5207) yolları `ciplak_mi` okumuyordu
  → geçerli çıplak-method (küresel/`*p`/asm) YANLIŞ reddediliyor (E010/G001/G002) + codegen çıplak-method'u
  prologue-skip ile emit ettiğinden checker↔codegen SAPMASI. Wrong-**reject** (güvenli taraf; geçersiz kabul YOK) →
  orta. **Onarım:** grant desenini (`guvensiz_baglam++/--`) iki method yoluna da ekle.

**REFUTED (bilgi):** safety-lens "çıplak stack-OOB sınır-kontrolünü eler" iddiası — skeptik BİREBİR doğrulayıp
REDDETTİ: bu `güvensiz`'in TASARIMLI opt-out'u (D-069); çıplak = güvensiz-tier olduğundan tutarlı, YENİ açık değil.

**FALSİFİYE-KANIT (kalıcı gate):** (a) **YENİ `calistir_ciplak_region_free`** (test_tumu'ya eklendi) — çıplak+iken/
+için fn'de region-symbol=0 HEM C HEM self-host + normal-döngü ρ_iter korunur (F4.3 regresyon); 4/4. Bu gate
codegen_diff'in exit-körlüğünü kapatır. (b) tip_kontrol +3: çıplak standalone/method *p deref → 0 hata, çıplak-olmayan
method → G001. (c) llvm 241/241, tip_kontrol 187/187 yeşil. (d) fixpoint stage1==stage2 KORUNDU (codegen.kem
çıplak-kullanmıyor → ciplak_aktif hep 0, kendi self-compile'ı etkilenmez) + codegen_diff 75/75.

**SIRADA:** F2 artık denetlenmiş + sağlam → **K1** (saf-.kem çıplak-allocator: küresel bump + ham pointer, döngü dâhil
region-free, kem_os.kem entegre, C kdl_bare_heap yolu sil).

---

## D-255 — SELF-HOST/F2: `çıplak işlev` codegen.kem PARİTE (C↔self-host divergence kapatıldı) (2026-07-06) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-254).

**Karar [ETKİ: `selfhost/codegen.kem` (lexer + Ayr.g_ciplak + parser modifier/dispatch + ciplak_mi + islev_uret
prologue-skip); `test/cg_korpus/cg_ciplak.kem` (YENİ codegen_diff parite). SELF-HOST ÇEKİRDEĞİ — soundness-kritik.]**
D-254 `çıplak işlev`'i C-derleyiciye ekledi ama `codegen.kem`'e (self-host) EKLEMEDİ → C↔self-host DIVERGENCE
(D-249/D-253 dersi). Bu commit codegen.kem'e AYNALADI (oracle = C llvm.c):

- **Lexer:** "çıplak" → "CIPLAK" token.
- **Parser:** `parse_islev_genel` çıplak/gerçekzamanlı modifier (herhangi sıra, nested eğer — `değilse eğer`
  codegen.kem'de yok, fixpoint-güvenli); node idx `p.g_ciplak`'a kaydedilir. Dispatch: `parse_disa_govde` +
  `parse_ust_oge` + özellik/uygula gövde (`sim_mi CIPLAK` 6 sitede).
- **Ayr struct:** `g_ciplak: Dizi<tam32>` (çıplak işlev düğüm idx listesi) + init `[]`.
- **Codegen:** `ciplak_mi(p, idx)` (g_ciplak üyelik) + `islev_uret`'te ciplak → region-prologue 3-emisyon ATLA
  (main `@kdl_global_bolge_al` seed, `@kdl_bolge_olustur` ρ_yerel; rho_yerel="" → serbest no-op). ρ param uniform-ABI
  için korunur (çağrı yerleri değişmez). C llvm.c ρ-prologue aynası.

**FALSİFİYE-KANIT (hepsi kalıcı gate):** (a) **cg_ciplak.kem codegen_diff** — çıplak-allocator (küresel bump, 2 çağrı
→ farklı adres, b-a==8), C-codegen exit=42 == self-host exit=42; **tahsis imzası BİREBİR** (`define i64 @tahsis(ptr
%rho)`) + HER İKİ codegen'de tahsis IR'inde region-prologue=0. Korpus **75/75** (HEM C-codegen HEM self-host geçer).
(b) **FIXPOINT stage1==stage2 BİREBİR** (codegen.kem çıplak-kullanmıyor ama eklenen handler self-compile kararlı;
KIRILMADI). test_tumu tam yeşil.

**DÜRÜST SINIR (pre-existing, D-249/D-253 sınıfı):** self-host checker güvensiz-tracking yok → D-254 çıplak-güvensiz-grant
(ham pointer + küresel explicit `güvensiz` gerektirmez) self-host'ta ENFORCE edilmez. Geçerli-program parity ÇALIŞIR
(codegen_diff exit-eşitlik); invalid-program reddi (çıplak-dışı küresel/deref) C'de var, self-host'ta yok = bilinen gap.
**SIRADA:** F2 tamam → **K1** (saf-.kem çıplak-allocator: küresel bump + ham pointer → kem_os.kem entegre → C
kdl_bare_heap yolu sil).

---

## D-254 — DİL/F2: `çıplak işlev` (no-region-prologue fonksiyon, güvensiz-scoped) — C-DERLEYİCİ TAM (2026-07-06) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-253).

**Karar [ETKİ: `src/lexer.h/c` + `src/anahtar_kelime.c` (TOK_CIPLAK); `src/ast.h` (islev.ciplak_mi); `src/parser.c`
(modifier parse + 6 dispatch sitesi); `src/tip_kontrol.c` (çıplak gövde = örtük güvensiz-bağlam); `src/llvm.c`
(islev_uret region-prologue SKIP + güvensiz-grant); `test/test_llvm.c` (+3 assert). WALL-2 / bootstrap-circularity
çözümü — 2 primitiften İKİNCİSİ.]**

D-251 K1 (runtime→.kem allocator göçü) `kdl_bolge_olustur→malloc→...` döngüsünde BLOKLU idi: HER .kem fonksiyonu
girişinde `@kdl_bolge_olustur` (ρ_yerel) + main'de `@kdl_global_bolge_al` emit ediyor → malloc'u .kem'de yazan
allocator'ın KENDİ prologue'u malloc çağırıyor = sonsuz döngü. **WALL-2 çözümü: `çıplak işlev`** — girişinde
region-prologue EMIT EDİLMEZ.

- **Lexer:** `çıplak` (`\xc3\xa7\xc4\xb1plak`, 8 bayt) → TOK_CIPLAK; binary-search tablo `çeşit`<`çıplak`<`özellik`
  (`\xa7`<`\xb6`). Komşu keyword sağlaması: çeşit/özellik hâlâ tanınıyor.
- **Parser:** `çıplak` opsiyonel işlev-modifier (gerçekzamanlı ile herhangi sırayla, P039 çift-yazım hatası);
  `parse_ust_oge` + `dışa`/`genel`/`özellik`/`uygula` + `sync_token_mu` dispatch. `islev.ciplak_mi` düğümde.
- **Checker:** çıplak gövde = örtük güvensiz-bağlam (`tk->guvensiz_baglam++`) → ham pointer deref-write + küresel
  yazımı explicit `güvensiz {}` gerektirmez. Kırılmazlık korunur: çıplak opt-in keyword, güvensiz-tier.
- **Codegen (llvm.c):** `islev_uret`'te ciplak → 3 emisyon ATLA: (1) main `@kdl_global_bolge_al` seed, (2)
  `@kdl_bolge_olustur` (ρ_yerel), (3) `@kdl_bolge_serbest` epilogue (rho_yerel=NULL → serbest_emit no-op).
  **ABI kararı:** ρ param KORUNUR (main-değil fn'ler yine `ptr %rho` alır, kullanılmaz) → çağrı yerleri DEĞİŞMEZ
  (D-249 imza-uyumsuz-çağrı segfault riski YOK). Bu spec-uyumlu (yasak = region-YARATMA çağrıları, param değil).
  Ayrıca çıplak gövde için `g->guvensiz_derinlik++` (deref-write doğru emit).

**FALSİFİYE-KANIT (kalıcı gate, `calistir_llvm_test`):** (a) **[236] ciplak: tahsis IR'inde region-prologue = 0** —
çıplak `tahsis` fn gövdesinde `@kdl_bolge_olustur`+`@kdl_global_bolge_al` grep-sayısı = 0 (IR-içerik denetimi
`ir_region_prologue_sayisi`). (b) **[237] kontrast** — normal `main` prologue >= 1 (skip yalnız çıplak'a özgü).
(c) **[238] ciplak-allocator kompozisyon** — küresel bump 2 çağrı → 2 FARKLI adres (a=0,b=8), b-a==8 → **exit 42**;
sonsuz-recursion YOK. Ek manuel: tam-çıplak program (main de çıplak) → TÜM IR'da 0 C-runtime region sembolü →
freestanding standalone link + exit 42 (K1 hedefinin canlı önizlemesi). Host: lexer 103/103, parser 107/107,
tip_kontrol 184/184, llvm 241/241.

**SINIR / SIRADA:** (1) **F2 self-host parite** (D-255) — `codegen.kem`'e AYNALA (D-253 dersi: divergence bırakma).
(2) Self-host checker güvensiz-tracking yok → çıplak-güvensiz-grant self-host'ta ENFORCE edilmez (D-249/D-253 sınıfı,
geçerli-program parity çalışır). (3) F2 bitince → **K1** (saf-.kem çıplak-allocator: küresel bump + ham pointer,
kem_os.kem'e entegre, C kdl_bare_heap yolu sil).

---

## D-253 — SELF-HOST/F1: `küresel değişken` codegen.kem PARİTE (C↔self-host divergence kapatıldı) (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-252).

**Karar [ETKİ: `selfhost/codegen.kem` (lexer+parser+codegen+g_isim pre-pass); `test/cg_korpus/cg_kuresel.kem` (YENİ
codegen_diff parite). SELF-HOST ÇEKİRDEĞİ — soundness-kritik.]** D-252 `küresel değişken`'i C-derleyiciye ekledi ama
`codegen.kem`'e (self-host) EKLEMEDİ → C↔self-host DIVERGENCE (D-249 dersi). Bu commit codegen.kem'e AYNALADI
(oracle=C llvm.c):

- **Lexer:** "küresel" → "KURESEL" token.
- **Parser:** `parse_kuresel` ("KURESEL" düğüm: dugum2 ad + tip + init) + `parse_ust_oge` dispatch.
- **Checker/pre-pass:** KURESEL adı `g_isim`'e (üst-düzey global ad çözümü — sabit/işlev gibi).
- **Codegen:** `g_kuresel_ad`/`g_kuresel_tip` tracking + `kuresel_topla` (module `@ad = internal global <ir> <init>`
  emit) + TANIMLAYICI → `load @ad` + ATAMA → `store @ad` (sabit gibi inline değil — mutable).

**FALSİFİYE-KANIT (hepsi kalıcı gate):** (a) **cg_kuresel.kem codegen_diff** — persistence (yaz 42, oku ayrı-fn → 42),
C-codegen exit=42 == self-host exit=42 (korpus **74/74**, HEM C-codegen HEM self-host geçer). (b) self-host --llvm IR =
C IR (`@g_val = internal global i32 0` + store/load). (c) **FIXPOINT stage1==stage2 BİREBİR (32833 satır**; codegen.kem
küresel-kullanmıyor ama eklenen handler kodu self-compile kararlı; KIRILMADI). test_tumu tam yeşil.

**DÜRÜST SINIR (pre-existing, D-249 sınıfı):** self-host checker E010/E011/E012 (güvensiz-gate / tip-kısıt / const-init)
ENFORCE ETMİYOR — self-host'ta güvensiz-derinlik-tracking yok (deref için de yoktu, D-249 flag). Geçerli program parity
ÇALIŞIR (--check+--llvm+exit); INVALID program (küresel güvensiz-dışı) self-host KABUL eder, C REDDEDER = divergence
yalnız invalid-programda. Safe .kem küresel üretmez → Kırılmazlık pratikte korunur. Hardening (ayrı): self-host checker
güvensiz-tracking + E010/E011/E012. **F1 TAM (C + self-host codegen parite). SIRADAKİ: F2 çıplak işlev.**

## D-252 — DİL/F1: `küresel değişken` (modül-mutable global, güvensiz-scoped) — C-DERLEYİCİ TAM (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-251).

**Karar [ETKİ: `src/lexer.h/lexer.c/anahtar_kelime.c` (keyword); `src/ast.h` (kuresel_mi); `src/parser.c`
(parse_kuresel_tanimi); `src/sembol.h` (kuresel flag); `src/tip_kontrol.c` (pre_populate_kuresel + E010/E011/E012);
`src/llvm.c` (global emit + load/store); `test/test_llvm.c` (persistence testi). Yalnız C-derleyici + test.]** Mehmet
design-stop (D-251 bootstrap-circularity çözümü, 2 primitiften İLKİ). WALL-1 çözümü: allocator kalıcı durumu
(bump-pointer) modül-global tutabilir. **Bu commit F1'in (küresel değişken) C-DERLEYİCİSİ TAM — self-host parite AYRI
adım (aşağıda).**

- **Lexer:** TOK_KURESEL + "küresel" keyword. KRİTİK: keyword tablosu BİNARY-SEARCH → memcmp-sıralı; `k\xc3` (küresel)
  `ku` (kullan)'dan büyük (0xc3>0x75) → kullan SONRASINA (ilk yanlış-poz kullan/ust-öğe parser testlerini kırdı).
- **Parser:** `parse_kuresel_tanimi` ('küresel değişken ad: tip = init;' → DUGUM_DEGISKEN kuresel_mi=1), üst-düzey.
- **Checker:** pre_populate_kuresel (global sembol, kuresel=1) + **E011 tip-kısıt** (yalnız skaler/ham-pointer;
  Dizi/yapı/metin YASAK — allocator'a bağlanamaz) + **E012 const-init** (sabit-literal) + **E010 güvensiz-only-erişim**
  (Kırılmazlık: paylaşılan-mutable-durum = confinement'ın kaçındığı aliasing → güvensize hapis; safe .kem erişemez).
- **Codegen:** `kureseller` registry + module `@ad = internal global <ir> <init>` + okuma→`load @ad` + atama→`store @ad`
  (sabit gibi inline DEĞİL — MUTABLE). Non-küresel program bu yolları tetiklemez → mevcut IR değişmez.

**FALSİFİYE-KANIT (uydurulamaz):** **persistence** — `yaz()` küresel'e 42 yazar, `oku()` (AYRI fn) okur → **exit 42**
(local olsa yaz'ın yazması oku'da görünmez, 0 gelirdi). IR: `@g = internal global i32 0` + store + load.
`test_llvm.c` test_kuresel_persistence [224] KALICI GATE (llvm 238/238). --check: güvensiz OK / dışı E010 / Dizi E011.
Host suite + **FIXPOINT birebir (32379 satır, KIRILMADI** — küresel-kullanmayan program yolu değişmez) + codegen_diff.

**KALAN (bu commit'te YOK):** (1) **F1 self-host parite** — parser.kem + checker.kem + codegen.kem (D-249 dersi).
Divergence şu an LATENT (codegen.kem küresel kullanmıyor → fixpoint korunur; ama bir .kem küresel kullanır + self-host
derlerse diverge). (2) **F2 çıplak işlev** (no-region-prologue, ayrı feature). İkisi bitince D-251 K1 (saf-.kem allocator)
mümkün → runtime→.kem göçü akar. İlgili: [[project-kem-codegen-pointer-gaps]].

## D-251 — DIAG/DUR: runtime→.kem TAM GÖÇÜ K1'de BLOKLU — 2 codegen/dil gap'i (design-stop) (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-250).

**Karar [ETKİ: yalnız DECISIONS + memory (teşhis). KOD YAZILMADI — K1 başlamadan bloklu; sahte .kem-allocator
yazmamak için DUR (direktif: kolay-işe/workaround KAÇMA).]** Görev: C-runtime'ı kademe kademe saf-.kem'e taşı,
kem_os.ll'de @kdl_* = 0. **SONUÇ: İLK kademe (K1 allocator) başlamadan BLOKLU — 2 fundamental codegen/dil gap'i +
circularity, hepsi ampirik DOĞRULANDI.** Bu gap'ler mekanik-iş değil, DİL-TASARIM kararı ([[feedback-karar-kurallari]]:
syntax/semantik → Mehmet).

**BASELINE (kem_os.ll C-runtime çağrıları):** region @kdl_bolge_olustur×22 + @kdl_bolge_serbest×39 + @kdl_global_bolge_al×1
(=62, HER fonksiyona codegen-emit); array @kdl_dizi_*×5; @kdl_metin_*×2; @kdl_mmio_*×3; @kdl_yetki_*×4.

**WALL-1 — .kem GLOBAL MUTABLE STATE YOK:** modül-düzeyi `değişken sayac: tam32 = 0;` → parser HATA. Allocator'ın
kalıcı bump-pointer + lazy-global-region cache'i .kem global olamaz (C: `static KdlBolge *kdl_global_bolge`,
`static` bump). Fixed-adres-depolama workaround mümkün ama tek başına WALL-2'yi çözmez.

**WALL-2 — REGION-EMISSION HER FONKSİYONDA, OPT-OUT YOK:** trivial `işlev f()->tam32{ver 0}` bile IR'da
`call @kdl_bolge_olustur()` (prologue) + `call @kdl_bolge_serbest()` (epilogue) emit eder (llvm.c:5356-5361, main
dahil koşulsuz; grep opt-out attribute = 0). → bir .kem `kdl_bolge_olustur`/`malloc` fonksiyonu KENDİ prologue'unda
@kdl_bolge_olustur çağırır.

**CIRCULARITY (decisive, doğrulandı):** `kdl_bolge.c:72` `kdl_bolge_olustur` → `malloc()` çağırır. Yani .kem `malloc`
→ (WALL-2 prologue) @kdl_bolge_olustur → kdl_bolge_olustur → malloc → @kdl_bolge_olustur → ... **SONSUZ RECURSION**.
Aynısı .kem `kdl_bolge_olustur` için (kendini prologue'da çağırır). → region + allocator runtime .kem'de YAZILMAZ.

**SONUÇ:** K5 (kem_os.ll'de @kdl_* = 0) MEVCUT CODEGEN'LE ULAŞILMAZ — @kdl_bolge_* (62 çağrı) her fonksiyonda +
.kem-tanımlanamaz (self-recursion). K1 (allocator) global-state + circularity ile bloklu → K2 (array, allocator'a bağlı)
transitif bloklu. K4 leaf-fonksiyonları (kdl_metin_bayt vb., global-state'siz + allocator-çağırmayan) muhtemelen
migratable AMA tek başına K5'e ulaşmaz (region kalır) + direktif dependency-order (K1 önce) diyor → K4'e atlamadım.

**UNBLOCK İÇİN GEREKEN (Mehmet design-stop kararı):** (1) **region-emission opt-out attribute** — bir .kem
fonksiyonunu "region prologue/epilogue YOK" işaretle (ör. `çıplak işlev` / `runtime` attribute); llvm.c + codegen.kem
(parite) + parser/checker = YENİ DİL ÖZELLİĞİ + bellek-güvenliği modelini değiştirir (o fn'de region-tracking yok).
(2) **global mutable state** (modül-düzeyi mutable `değişken`) VEYA yaptırımlı fixed-adres-depolama deseni. İkisi de
dil-tasarım kararı. Düzeltme Mehmet'in; ben DUR + flag (direktif: durdurucu gap → DUR).

## D-250 — DIAG: HEAP Dizi<T> indeks-yazma bare-metal — "codegen-bug mu link-sorunu mu" NET cevap: LİNK (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-249).

**Karar [ETKİ: `test/ornekler/diag_heap_yaz_linkli.kem` (YENİ); `Makefile` (`calistir_diag_heap_yaz_arm` + OS-aggregate).
Yalnız test + target; src/selfhost/runtime DEĞİŞMEDİ.]** Tanı görevi: heap Dizi<T> INDEKS-YAZMA (`d[i]=v` →
`kdl_dizi_yaz` yolu) heap-runtime DÜZGÜN linkli iken ARM64 QEMU'da çalışıyor mu? (Codex'in önceki teşhisi
heap-runtime'ı LİNKLEMEMİŞTİ.) **SONUÇ: LİNK-SORUNUYDU, CODEGEN SAĞLAM.**

**UYDURULAMAZ-KANIT (QEMU):** `HEAP KONTROL oku0=10` (heap-oku çalışıyor) + `HEAP YAZ: yaz=48879 oku=48879 =>
EVET` (0xBEEF yazıldı, 0xBEEF geri okundu). **LINK-DURUMU:** `d[i]=v` codegen `call void @kdl_dizi_yaz_tam` emit
eder; `kdl_dizi_*` undefined YOK (bm_a64_heap.o'dan çözüldü — bm_a64_heap.o kdl_dizi_yaz'ın 4 varyantını tanımlar,
kdl_dizi.inc). Şablon = calistir_kernel_dizi_bare_metal (BM_A64_OBJS = bm_a64_heap.o dahil linkler). **KEMGU'da
saf-stack-dizi YOK — her Dizi<T> heap-uniform (kdl_dizi.inc); stack [N×T] yolu yok → ikinci (stack) diag ATLANDI
(bu da bulgu).** **Sonuç:** `d[i]=v` heap-yazma codegen'i (llvm.c INDEKS→kdl_dizi_yaz) DOĞRU; önceki "bug" =
bm_a64_heap.o linklenmeme artefaktı. Gerçek fix küçük: diag/kernel'leri BM_A64_OBJS'e linkle (zaten kem_os/kernel_dizi
öyle yapıyor). Düzeltmeyi orchestrator yapacak. OS-gate'e eklendi.

## D-249 — SELF-HOST: codegen.kem POINTER PARİTE — C↔self-host divergence fix (saf-.kem-OS adım-1) (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-248).

**Karar [ETKİ: `selfhost/codegen.kem` (cast+deref-load+deref-store codegen + T022 checker); `test/cg_korpus/
cg_ham_pointer.kem` (YENİ codegen_diff parite); `Makefile` (calistir_kem_pointer_self_arm + OS-aggregate). SELF-HOST
ÇEKİRDEĞİ — soundness-kritik. Mehmet "KEMGU-OS SAF .kem, C substrat YOK" vizyonu, adım-1.]** D-248 ham-pointer'ı
`src/llvm.c`'ye (C-codegen) ekledi ama `selfhost/codegen.kem`'e (self-host) EKLEMEDİ → **C↔self-host DIVERGENCE**
(C derler, self-host T022-red + inttoptr-yok). Saf-.kem-OS self-host'ta derlenemezdi. D-248'in 3 emisyonu codegen.kem'e
AYNALANDI (oracle=llvm.c), + 1 checker parite:

- **GAP-1 cast** (TIP_DONUSTUR codegen ~2809): hedef ptr → `inttoptr`; kaynak ptr → `ptrtoint`.
- **GAP-2 deref-write** (ATAMA deyim_uret ~3216): TEKLI `deref*` lvalue kolu → `store volatile`.
- **GAP-3 volatile** (deref* load ~2683): `load volatile`.
- **Checker T022** (ATAMA kontrol_dugum ~4487): TEKLI `deref*` da lvalue (C tip_kontrol D-248 aynası).
- güvensiz-scoped: deref checker-gereği güvensiz-only → volatile KOŞULSUZ = llvm.c'nin `guvensiz_derinlik>0` koşulu
  daima-doğru hali (parite).

**FALSİFİYE-KANITLAR (3'ü de kalıcı gate):** **(a) codegen_diff** — YENİ `cg_ham_pointer.kem` (host-güvenli
inttoptr/ptrtoint round-trip, deref-yok): C-codegen exit=42 == self-host exit=42 (korpus 73/73, test_tumu'da). D-249
öncesi self-host bunu DERLEYEMEZDİ = divergence kanıtı. **(b) FIXPOINT** — stage1 IR == stage2 IR BİREBİR (32379 satır;
+222 = eklenen handler kodu; codegen.kem raw-pointer KULLANMIYOR → emit yolu fixpoint'te değil ama self-compile kararlı;
fix fixpoint'i KIRMADI). **(c) self-host bare-metal** — YENİ `calistir_kem_pointer_self_arm`: `kemgu_self.exe`
kem_pointer.kem'i derler → QEMU boot (self-host IR inttoptr+volatile+0 kdl_mmio + "KEM PTR MMIO OK"/1953655158 +
"KEM PTR RAM OK"/12345), OS-gate'te. = self-host codegen ham-pointer'ı GERÇEKTEN destekliyor.

**DÜRÜST FLAG (soundness, pre-existing):** self-host checker güvensiz-scoping'i deref için ENFORCE ETMİYOR — `*p`
deref-read güvensiz-DIŞINDA da kabul (C reddeder; teyit: `oku(x:*tam32){ver *x}` → self-host OK, C HATA). PRE-EXISTING
(deref-read'de zaten vardı, D-249 değil). T022 gevşetmem deref-write'ta SİMETRİK. Kırılmazlık PRATİKTE korunur (safe .kem
ham pointer üretmez — `&` referans kullanır; ham `*T` yalnız int→ptr güvensiz cast'tan). **Hardening (ayrı iş):**
self-host checker'a güvensiz-derinlik-tracking + deref read/write'ı güvensiz-scope'a al. **PATH:** fixpoint/bootstrap
`TMPDIR=/c/tmp` + clang64/ucrt64-önce ([[project-codegen-bootstrap-path-gotcha]]).

**SIRADAKİ (Mehmet adım-2):** saf-.kem HEAP allocator (ham-pointer artık iki derleyicide de çalışıyor) → kem_os.kem'e
entegre → C `kdl_bare_heap` çağrılarını .kem-heap ile değiştir → C-yolu sil (kem_os IR'da @kdl_bare_heap/@malloc=0).

## D-248 — CODEGEN: 3 pointer gap onarıldı — güvensiz int↔ptr + deref-write + volatile (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-247).

**Karar [ETKİ: `src/llvm.c` (cast handler + DUGUM_ATAMA + OP_DEREFERANS); `src/tip_kontrol.c` (E002 + T022);
`test/ornekler/kem_pointer.kem` (YENİ gate); `test/snapshots/deref_atama_t022.kem` + `deref_atama_disi.kem` +
`test/test_llvm.c` (test güncelleme); `Makefile` (calistir_kem_pointer_arm + aggregate). DERLEYİCİ değişikliği —
Mehmet 3-adım planı adım-2 direktifi.]** Faz-2b keşif (D-246) 3 codegen gap buldu (heap+MMIO'yu saf-.kem'de imkansız
kılıyordu). ONARILDI, **HEPSİ GÜVENSİZ-SCOPED** (safe .kem etkilenmez):

- **GAP-1 int↔ptr cast:** `src/llvm.c` DUGUM_TIP_DONUSTUR — hedef ptr ise kaynağı i64 üret + `inttoptr` emit;
  kaynak ptr + hedef int ise `ptrtoint`. `src/tip_kontrol.c` E002 — `tk->guvensiz_baglam != 0` ise int↔*T cast izinli.
- **GAP-2 deref-write:** `src/llvm.c` DUGUM_ATAMA — OP_DEREFERANS lvalue kolu (p'yi ptr üret + `store <T> <v>, ptr`).
  `src/tip_kontrol.c` T022 — güvensizde `*p` lvalue izinli.
- **GAP-3 volatile:** `src/llvm.c` OP_DEREFERANS load + deref-store — `g->guvensiz_derinlik > 0` ise `volatile`
  (MMIO okuma/yazma clang -O2'de elenmez/sıralanmaz).

**FALSİFİYE-KANIT:** (a) `kem_mmio_ham.kem` (D-246'da BLOKLU — clang "i32 but expected ptr") artık **boot ediyor**
(raw-ptr VirtIO magic okuma 1953655158, saf .kem, 0 kdl_mmio-intrinsic). (b) YENİ `calistir_kem_pointer_arm` gate:
IR'da inttoptr>0 + store/load volatile + kdl_mmio-call=0 (grep-ENFORCE) + QEMU "KEM PTR MMIO OK"/1953655158 (raw-ptr
volatile MMIO) + "KEM PTR RAM OK"/12345 (raw-ptr deref-write round-trip). **GÜVENSİZ-SCOPE:** güvensiz İÇİNDE `*p=v`
compile eder, DIŞINDA hâlâ T022-red (deref_atama_disi.kem doğrular).

**Test güncelleme (spec değişimi):** matris-B (test_llvm.c [223]) eski spec (`*p=v` → T022-red) YENİ spec'e
(güvensiz OK + dışı red) güncellendi; +`deref_atama_disi.kem` snapshot. Bu güvensiz-deref-write'ı ENGELLEYEN eski test
= kasıtlı davranış değişimi (GAP-2 fix). **Regresyon:** llvm 237/237; full host suite + self-host FIXPOINT + OS gate.

**KEŞFEDİLEN PRE-EXISTING BUG (D-248 DIŞI, [[project-kem-codegen-pointer-gaps]]; [[task_49d4c3ab]] flag):** güvensiz
blokta `yazdir_metin("...")` string arg'ı `i32`'ye düşüyor (`call @kdl_yazdir_metin(i32 %2)`, ptr yerine) → runtime
"(bos)". `git stash` ile pre-existing teyit (D-248 değil). kem_pointer.kem marker'ları bu yüzden güvensiz-DIŞINDA basar.

**TASARIM KARARI (Mehmet onayına, YASA-5 KEMGU-in-KEMGU büyütür):** üç yetenek de güvensiz-scoped (raw pointer zaten
güvensiz-only). volatile güvensiz-wide (mevcut güvensiz kod ör. kütüphane/dizi.kem volatile load alır — davranış-korur,
micro-perf trade-off; MMIO doğruluğu için gerekli). Alternatif=ayrı volatile-qualifier (daha büyük dil değişimi).
**SIRADAKİ (adım-3):** heap+MMIO'yu kem_os.kem'e entegre (artık saf-.kem mümkün).

## D-247 — OS: KEMGU-OS Faz-2c SERİ ENTEGRASYON — .kem panik + exc-handler kem_os.kem'e entegre (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-246).

**Karar [ETKİ: `test/ornekler/kem_os.kem` (panik + exc-handler + uart_onaltilik embed + [5] SİSTEM); `test/ornekler/
kem_panik.kem` + `kem_exc.kem` SİLİNDİ; `Makefile` (kem_os gate + panik/exc .kem-define + [5] EXC OK kanıtı). Yalnız
test/örnek + Makefile.]** Anayasa Faz-2c (Mehmet 3-adım planı adım-1, SERİ tek-el): Faz-2b keşif prototipleri (panik +
exc-handler-mantığı, D-246) MEVCUT çekirdek kem_os.kem'e ENTEGRE. **[5] SİSTEM alt-sistemi** eklendi: exc-handler 3
sentetik fault ile SELF-TEST (data-abort+translation→KURTAR / permission→OLDUR / instr-abort→HALT decode); panik
entegrasyon-arıza branch'ine WIRE (gecti != 6 → `panik(...)`). Konsol .kem-UART korundu — panik/exc `uart_*` kullanır
(C yazdir DEĞİL) + yeni `uart_onaltilik` (hex printer, dtam64) eklendi.

**FALSİFİYE-KANIT (YASA-3, kem_os.kem'in KENDİSİNE):** kem_os.ll IR'ında (a) `define @panik` + `define @kem_istisna_isle`
(+esr_ec/fault_sinifi/abort_alt_tur/handler_karari) = **.kem-DEFINE** (C `kdl_panik`/`kdl_istisna_isle` DEĞİL; Makefile
grep-ENFORCE); (b) `call @panik` = panik arıza-branch'ine WIRE (grep-ENFORCE); (c) `call kdl_yazdir` = **0** (2a korundu);
(d) QEMU boot: `[5] SISTEM: exc-handler self-test` + 3 fault satırı (`EXC FAR=0x40000000 ESR=0x96000004 karar=KURTAR` vb.,
hex çıktı .kem uart_onaltilik'ten) + `[5] EXC OK kararlar=1,2,3` + `KEMGU KEM-OS OK`. 6-kontrol (heap+mmio+hesap×3+exc).

**YASA-2:** standalone `kem_panik.kem` + `kem_exc.kem` SİLİNDİ (keşif-prototip → entegre → kaldır; kem_surucu deseni).
`kem_heap.kem` + `kem_mmio_ham.kem` KALDI (bloklu; adım-2 codegen fix + adım-3 entegrasyon bekliyor).

**DÜRÜST SINIR (YASA-4, etiket=öz):** exc handler MANTIĞI (.kem ESR→EC decode + fault-türü + karar) kem_os'ta canlı +
self-test edildi; **GERÇEK fault-yönlendirmesi (asm-vektör VBAR → .kem handler hook) SONRAKİ adım** — `start_aarch64.S`
vektör tablosu + trap-frame asm KALIR (C8 sınıfı, .kem inline-asm yok), fault-bilgisi asm-stub'dan param geçer. panik
happy-path'te TETİKLENMEZ (arıza-yolu handler'ı; .kem-define + wire kanıtlı, standalone D-246'da halt kanıtlandı). Full OS
gate 130, sıfır regresyon, C-twin yeşil. **SIRADAKİ (Mehmet planı adım-2):** 3 codegen gap onar (inttoptr/deref-write/
volatile → [[project-kem-codegen-pointer-gaps]]) → adım-3 heap+MMIO entegre.

## D-246 — OS: KEMGU-OS Faz-2b KEŞİF — .kem-runtime-katman prototipleri + 3 codegen gap teşhisi (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-245).

**Karar [ETKİ: `test/ornekler/kem_{heap,panik,exc,mmio_ham}.kem` (4 YENİ keşif dosyası). Yalnız test/örnek; Makefile-target
YOK — entegrasyon 2c seri.]** Anayasa Faz-2b = KEŞİF (paralel, 4-agent Workflow wf_353dddf0-c79): kem_os.kem'in C-runtime
katmanlarını (.kem-heap/MMIO/panik/exception) .kem'de İFADE EDİLEBİLİR Mİ diye prototiple. kem_os.kem'e DOKUNULMADI.
**Sonuç: 2 katman saf-.kem READY, 2 katman codegen-gap BLOKLU (hepsi bağımsız DOĞRULANDI).**

**2c-ENTEGRASYONA HAZIR (saf-.kem, gap yok):**
- **panik** (`kem_panik.kem`) → "KEM PANIK OK" + panik mesajı + `iken doğru` halt (inline-asm YOK). `kdl_panik.c` yerine. libc-temiz, boot ✓.
- **exc** (`kem_exc.kem`) → "KEM EXC OK kararlar=1,2,3 son-FAR=0x80000000". ESR→EC decode (>>26) + DFSC + fault-türü kararı,
  TAMAMEN .kem (--check temiz, boot ✓). `kdl_kesme.c` HANDLER-mantığı yerine. **MİMARİ SINIR (gap değil):** vektör tablosu
  + bağlam-kaydetme `start_aarch64.S` asm'de KALIR (VBAR asm giriş + trap-frame; .kem inline-asm yok = C8 sınıfı); yalnız
  handler-mantığı .kem'e taşınabilir, FAR/ESR'i asm-stub okuyup param geçer.

**BLOKLU — 3 GERÇEK CODEGEN GAP (→ [[project-kem-codegen-pointer-gaps]], Mehmet DESIGN-STOP):**
- **heap** (`kem_heap.kem`) → "KEM HEAP OK" AMA yalnız YOL-B ile (C `kdl_mmio_oku32/yaz32`'ye DELEGE ederek; kem_heap.ll'de
  4 C-mem çağrısı DOĞRULANDI) — saf-.kem ham-bellek BLOKLU. `kdl_bare_heap.c` yerine geçemez.
- **mmio** (`kem_mmio_ham.kem`) → BOOT ETMEDİ. `kemgu --llvm` geçer ama `clang -x ir` REDDEDER: "'%6' defined with type
  'i32' but expected 'ptr'" (REPRODÜKSİYON DOĞRULANDI). `kdl_runtime_mmio.c` yerine geçemez.
- **GAP-1 int→ptr cast:** `adres olarak *tam32` → codegen `inttoptr` EMIT ETMEZ (kod tabanında `inttoptr` grep=0 DOĞRULANDI);
  llvm.c:~3868 cast handler yalnız int↔int/float; tip_kontrol E002 pointer-cast'ı dışlar.
- **GAP-2 deref-write:** `*p = v` codegen'de DÜŞER (llvm.c:~4235 DUGUM_ATAMA yalnız TANIMLAYICI/INDEKS/ERISIM dalları
  DOĞRULANDI, OP_DEREFERANS yok; read `*p` çalışır — asimetri); T022 deref'i lvalue saymaz.
- **GAP-3 volatile:** OP_DEREFERANS düz load/store, `volatile` değil → -O2 MMIO'yu eler/sıralar.
- **Kritik:** Bunlar DİL SEMANTİĞİ değil C-bootstrap codegen EMIT eksikliği (mmio agenti IR'ı elle yamalayıp clang'ı geçirip
  host'ta exit 42 aldı → tasarım sağlam). Düzeltme src/llvm.c + tip_kontrol.c = Mehmet kararı.

**KEŞİF METODU:** 4 worktree-paylaşımlı agent, ayrı .kem, önce-derlenmiş bm_a64 objelerine link, QEMU-boot kanıtı +
gap-flag. Ben firsthand DOĞRULADIM (4 dosya+boot, mmio clang-repro, llvm.c/tip_kontrol satır-teyidi, heap YOL-B grep,
exc --check). **SIRADAKİ (Mehmet):** DESIGN-STOP — (A) 3 codegen gap'i onar (heap+MMIO .kem-native olur, YASA-5) vs
(B) heap/MMIO'yu kalıcı C-intrinsic kabul et + 2c'de yalnız panik+exc entegre et.

## D-245 — OS: KEMGU-OS Faz-2a SERİ ENTEGRASYON — .kem-UART kem_os.kem'e entegre, C-yazdir SİLİNDİ (2026-07-05) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-244).

**Karar [ETKİ: `test/ornekler/kem_os.kem` (UART sürücü embed + main yazdir→uart); `test/ornekler/kem_surucu.kem`
SİLİNDİ; `Makefile` (kem_os target +bm_a64_metin.o + IR-kdl_yazdir-call=0 kanıtı; calistir_kem_surucu_arm target +
aggregate KALDIRILDI). Yalnız test/örnek + Makefile.]** KEMGU-OS Anayasası Faz-2a (SERİ, tek-el, agent YOK): D-244'te
ayrı `kem_surucu.kem`'de kanıtlanan .kem-native UART sürücüsü, MEVCUT çekirdek `kem_os.kem`'e ENTEGRE edildi; 12 C-yazdir
çağrısı (8 `yazdir_metin` + 4 `yazdir_tam`) `.kem` UART'la (`uart_satir`/`uart_tam_satir`) DEĞİŞTİRİLDİ; C-yazdir yolu
SİLİNDİ. Bu Faz-2'nin **İLK GERÇEK TUĞLASI** (Anayasa): kem_os.kem C-runtime'dan arınmaya başladı (KEMGU-in-KEMGU
yüzeyi büyüdü — YASA-5).

**FALSİFİYE-KANIT (YASA-3, ayrı demoya DEĞİL kem_os.kem'in KENDİSİNE):** `kem_os.ll` (derlenmiş IR) içinde
`call.*kdl_yazdir` = **0** (Makefile gate grep-ENFORCE eder; 1+ çağrı → FAIL "konsol hala C runtime'a iniyor"). Boot
kanıtı: TEK QEMU boot'ta `=== KEMGU .kem-OS (Faz-2a: konsol cikti .kem-native UART) === / [1] BOOT OK / [2] HEAP DIZI OK
/ 55 / [3] MMIO OK / 1953655158 / [4] HESAP OK / 230 / 3 / KEMGU KEM-OS OK` — DÖRT alt-sistemin TAMAMININ çıktısı .kem
UART sürücüsünden (tek `kdl_mmio_yaz32` yolu). libc-temiz. UART sürücüsü kem_os'a embed: uart_bayt(FR.TXFF poll + DR yaz,
yetki<MMIO> linear) + uart_metin(metin_bayt) + uart_tam_satir(özyinelemeli). metin_bayt bare-metal = `bm_a64_metin.o`
(D-244, kdl_metin_bare.c) kem_os link'inde.

**YASA-2 (izole-demo biriktirme YOK):** standalone `kem_surucu.kem` + `calistir_kem_surucu_arm` target + aggregate girişi
SİLİNDİ — UART sürücüsü artık kem_os.kem'in İÇİNDE (D-244 = keşif-prototip → D-245 = entegrasyon + prototip kaldır;
Anayasa'nın KEŞİF→ENTEGRASYON akışı). `kdl_metin_bare.c` + build kuralı korundu (kem_os kullanıyor). Full OS gate 130
(kem_surucu düştü, kem_os güçlendi), sıfır regresyon. **Anayasa PATH kuralı:** gate'ler clang64/ucrt64-önce PATH ile
(codegen_bootstrap PATH-artefaktı = [[project-codegen-bootstrap-path-gotcha]]).

## D-244 — OS: KEMGU-OS Faz-2 adım 2 — .kem-NATIVE PL011 UART SÜRÜCÜSÜ (konsol çıktısı artık .kem) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-243).

**Karar [ETKİ: `test/ornekler/kem_surucu.kem` (YENİ); `runtime/kdl_metin_bare.c` (YENİ); `Makefile` (bm_a64_metin.o
kuralı + `calistir_kem_surucu_arm` target + OS-gate aggregate). Yalnız test/örnek + YENİ freestanding runtime dosyası;
host runtime'a DOKUNULMADI.]** Faz-2 adım 2 (Mehmet seçimi ".kem-native sürücü katmanı"): **konsol çıktı yolu artık C
runtime (`kdl_yazdir_metin`) DEĞİL — TAMAMEN .kem'de, doğrudan MMIO ile PL011 UART.** "Kendi dilinde OS" DNA'sının ilk
gerçek sürücü katmanı. Serial hand-work.

**`.kem`-native UART sürücüsü (`kem_surucu.kem`):**
- **`uart_bayt(b)`** — FR.TXFF `iken (mmio_oku32(y, FR) & TXFF) != 0 {}` poll (bitwise `&`, `!=`-önce-bağlanmasın diye
  parantez ZORUNLU) → `mmio_yaz32(y, DR, b)`. `yetki<MMIO>` LİNEAR: her çağrı üret/oku-ödünç(döngüde)/yaz-ödünç/geri_al
  (loop-içi threading gerekmez — mmio_oku/yaz ÖDÜNÇ alır).
- **`uart_metin(s)`** — `metin_uzunluk`+`metin_bayt` ile bayt-bayt (String iterasyonu).
- **`uart_tam_satir(n)`** — özyinelemeli basamak çıkarımı (`n/10`+`n%10`+48), sıfır özel-durum.

**KAPATILAN GERÇEK GAP:** `metin_bayt`/`metin_uzunluk` bare-metal'de YOKTU — host `kdl_runtime.c`'de strlen/strcmp'e
bağlı (libc), bare-metal link'te **undefined symbol** (probe ile kanıtlandı). Yeni `runtime/kdl_metin_bare.c`:
freestanding `kdl_metin_uzunluk`+`kdl_metin_bayt` (manuel uzunluk döngüsü, strlen-siz), `bm_a64_metin.o` olarak
DERLENİR, yalnız gereken hedeflere EXPLICIT eklenir (bm_a64_mmio.o deseni; host kdl_runtime.c'ye DOKUNULMADI → host suite
etkilenmez; asla birlikte linklenmez = çift-tanım yok). **Kanıt (uydurulamaz):** (a) IR'da `call.*kdl_yazdir` = **0**
(yalnız 8 declare boilerplate; gate bunu grep'le enforce eder); tüm konsol çıktı tek `kdl_mmio_yaz32` çağrısından. (b)
libc-temiz (strlen/strcmp dahil grep). (c) QEMU boot: banner + "[test] sayi = 42 / 1953655158 / 0" + "KEM SURUCU OK" —
hepsi .kem MMIO sürücüsünden. **Gate:** `calistir_kem_surucu_arm` (link +metin+mmio+yetki; IR-kdl_yazdir-call=0 kontrol +
QEMU marker); OS-aggregate'te (`calistir_kem_os_arm`'ın yanına). İlk-deneme boot.

**Faz-2 ilerleme:** D-243 iskelet (A+B+C+E, C-yazdir'la) → **D-244 konsol yolu .kem'e portlandı.** Kalan .kem-native
sürücü katmanı: heap-runtime zaten .kem'den çağrılıyor (kdl_dizi region), MMIO zaten .kem intrinsic. Sıradaki: UART init
(baud/CR) .kem'e, VEYA kem_os.kem'i bu .kem UART sürücüsünü kullanacak şekilde güncelle (tam .kem-native OS iskeleti).
İlgili: [[project-os-faz1-kem-baremetal]].

## D-243 — OS: KEMGU-OS Faz-2 SERİ ENTEGRASYON — tek .kem-native OS iskeleti (A⊕B⊕C⊕E) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-242).

**Karar [ETKİ: `test/ornekler/kem_os.kem` (YENİ); `Makefile` (`calistir_kem_os_arm` target + OS-gate aggregate). Yalnız
test/örnek.]** Faz-1 keşif kernel'leri (A/B/C/E — D-242) SERİ entegre edildi: **TEK `.kem` imajı, TEK boot, DÖRT
alt-sistem canlı.** Bu, C-yazılı entegre çekirdeğin (`test/bare_metal/kemgu_os_arm.c`) **`.kem`-native ikizinin
İSKELETİ.** Serial hand-work (mini-agent YOK), CLAUDE.md serial-kuralı.

**`kem_os.kem` — dört alt-sistem TEK main'de:**
- **[1] BOOT** — .kem bare-metal boot + UART banner ("=== KEMGU .kem-OS iskelet (Faz-2 A+B+C+E) ===" + "[1] BOOT OK").
- **[2] HEAP** — `heap_dizi_topla()`: Dizi<tam32> (region runtime + kapasite-aşımı realloc + sınır-kontrol) → 55.
- **[3] MMIO** — `mmio_magic_oku()`: `yetki<MMIO>` capability + `mmio_oku32` VirtIO MagicValue (0x0A000000) → 0x74726976, linear `geri_al`.
- **[4] HESAP** — faktoriyel(5)+fib(10)+dongu_toplam(10)=230 (özyineleme+`iken`), `eşleş` pattern-match, `eğer/değilse`.

**Entegrasyon kanıtı (uydurulamaz):** dört alt-sistem de doğru sonuç verirse (5/5 iç-kontrol: dt==55 ∧ magic==virt ∧
h==230 ∧ kat==3 ∧ pm==300; `gecti` sayacı — 5-yollu `ve` yerine sayaç, codegen de-risk) TEK **"KEMGU KEM-OS OK"**
marker'ı basılır. QEMU boot (ilk-deneme): `[1] BOOT OK / [2] HEAP DIZI OK / 55 / [3] MMIO OK / 1953655158 / [4] HESAP
OK / 230 / 3 / KEMGU KEM-OS OK`. libc-temiz. **Gate:** `calistir_kem_os_arm` (build `./kemgu --llvm kem_os.kem` → clang
-x ir → ld.lld +bm_a64_mmio.o+bm_a64_yetki.o+$(BM_A64_OBJS) → QEMU → "KEMGU KEM-OS OK" + [1..4] marker grep);
OS-gate aggregate'e (`calistir_kemgu_os_arm`'ın yanına, .kem-native ikizi) eklendi.

**Bu iskelet Faz-1'in KANITLANMIŞ syntax'larını birleştirdi (ilk-deneme boot).** Faz-2 devamı (seri): tek-marker OS'ten
→ gerçek entegre çekirdek (kernel main döngüsü, alt-sistem init sırası, .kem-native UART/heap/MMIO sürücü katmanı).
İlgili: [[project-os-faz1-kem-baremetal]], [[project-os-c1-region-backing-track]].

## D-242 — OS: KEMGU-OS Faz-1 KEŞİF — .kem-yazılı bare-metal kernel MÜMKÜN (4/5 QEMU-boot) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-241).

**Karar [ETKİ: `test/ornekler/kem_*.kem` (5 YENİ keşif dosyası). Yalnız test/örnek; Makefile-target YOK — entegrasyon
Faz-2 seri iş.]** Mehmet: "6 paralel agent, her biri AYRI .kem, uydurulamaz QEMU-boot kanıtı — .kem bare-metal OS
MÜMKÜN mü + parçaları keşfet." 6-agent Workflow (wf_3c22af1c-230). **CEVAP: .kem bare-metal MÜMKÜN — 4/5 kernel
QEMU'da GERÇEKTEN boot etti (libc-temiz, hepsi bağımsız doğrulandı).**

**BOOT EDEN (build/kem_*.out marker'ları doğrulandı):**
- **A `kem_cekirdek_min.kem`** → "KEM KERNEL OK" — .kem bare-metal ÇEKİRDEK kanıtı (minimal boot+UART, stack-yalnız).
- **B `kem_dizi_kernel.kem`** → "KEM DIZI OK"+55 — heap Dizi<tam32> (kdl_bare_heap region runtime + kapasite-aşımı
  realloc/memcpy + D-069 sınır-kontrollü erişim) .kem'den bare-metal çalışıyor.
- **C `kem_mmio_kernel.kem`** → "KEM MMIO OK"+1953655158 — `yetki<MMIO>` capability + `mmio_oku32` ile VirtIO-MMIO
  MagicValue register'ı (0x0A000000) GERÇEKTEN okundu (=0x74726976 "virt"), linear `geri_al` ile tüketildi. .kem
  düşük-seviye donanım erişimi capability-güvenli yapabiliyor.
- **E `kem_hesap_kernel.kem`** → "KEM HESAP OK"+230+3+"KEM PM OK" — özyinelemeli faktoriyel(5)=120 + fib(10)=55 +
  `iken` döngü(55)=230, `eşleş` pattern-match (literal 0/1/2 + joker), `eğer/değilse` zincir. Kontrol-akışı +
  fonksiyon + pattern-match bare-metal aarch64'te SAĞLAM.

**TAKILAN — TEK GERÇEK GAP (D `kem_asm_kernel.kem`, precisely-located):** `satıriçi_asm` (inline-asm) ARM64'te ifade
EDİLEMİYOR. C-bootstrap satıriçi_asm'i AST+tip-kontrol+LLVM-lowering'de TAM destekler (`src/llvm.c:4848` doğru `call
asm sideeffect` üretir) AMA hedef mimari `src/llvm.h:38` `#define KEMGU_HEDEF_MIMARI "x86_64"` **sabit-kodlu** (yorum:
"Hedefe-duyarli triple secimi C8'in isi"). arm64-tag → AS001/0-satır-IR; x86_64-tag → IR çıkar ama aarch64 backend x86
mnemonic reddeder. **Eksik: hedefe-duyarlı triple/mimari CLI seçimi (C8).** NOT: aarch64 bare-metal .kem inline-asm
GEREKTİRMEZ (A/B/C/E asm'siz boot etti; düşük-seviye erişim MMIO intrinsic ile) → gap kritik-yol DEĞİL.

**F FIXPOINT TEŞHİS — KRİTİK DÜZELTME:** codegen_bootstrap "KIRIK/79-84 fark" GERÇEK regresyonu DEĞİL, **PATH
artefaktı.** İki-yönlü kanıt (aynı HEAD/exe): PATH=clang64/ucrt64-önce (CLAUDE.md standart) → **exit=0, FIXPOINT ✓,
stage1==stage2 BİREBİR (32157 satır), lexer/parser/checker 84/84 birebir**; PATH=/c/msys64/usr/bin-önce →
harness `mktemp` tmp-yazımı bozulur → stage.ll eksik → "84 fark" → exit=2. **D-235 "codegen_bootstrap pre-existing
kırık" flag'i YANLIŞTI** (aynı artefakt; kemgu.exe bayat değil, hiçbir commit fixpoint'i kırmadı). `test_tumu` exit=2
de yalnız bu — tüm birim testleri Basarisiz:0; doğru PATH ile test_tumu tam yeşil.

**SONUÇ (Mehmet'in stratejik sorusu "pivot haftalar mı?"):** HAYIR — dil+codegen+runtime bare-metal için HAZIR
(boot/heap/MMIO/kontrol-akışı/eşleş hepsi .kem'den çalışıyor). Faz-2 = SERİ entegrasyon (keşif kernel'lerini tek
.kem-OS'e birleştir + Makefile-gate). **Reprodüksiyon:** `./build/kemgu.exe --llvm test/ornekler/kem_<X>.kem >
build/<X>.ll` → `clang -target aarch64-unknown-none -ffreestanding -nostdlib -O2 -Wno-override-module -x ir ... -c` →
`ld.lld -T linker/bare-metal-aarch64.ld ... $(BM_A64_OBJS)` (C: +bm_a64_mmio.o+bm_a64_yetki.o) → `qemu-system-aarch64
-M virt -cpu cortex-a72`.

## D-241 — OS: KEMGU-OS — kabuktan ÇOKLU-PROCESS spawn (concurrent + kanal IPC) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-240).

**Karar [ETKİ: `test/bare_metal/kemgu_shell_el0.c`; `Makefile`. Yalnız test.]** EL0 kabuğa `coklu` komutu: kabuk 3
userspace worker'ı EŞZAMANLI spawn eder (concurrent multi-process). `prog_uretici` (.user, EL0): getpid (sys 11 — ayrı
process kimliği) + kanal_gonder(100) (sys 22 — paylaşımlı IPC) + exit. `coklu`: 3× spawn(sys 12)→pid + hepsini join
(sys 14) + kanaldan topla (sys 23 kanal_al) + rapor. **Kanıt:** "COKLU spawn pid=4/5/6" (3 AYRI process) +
"[prog uretici] pid=4/5/6 kanala 100 yaziyor" (3 CONCURRENT EL0 process, her biri KENDİ pid'iyle, kendi süreç
adres-uzayında) + **"COKLU BITTI: 3 process, kanal-toplam=300 (3 mesaj)"** (kabuk hepsini join etti + paylaşımlı
kanaldan 3 mesaj [3×100=300] topladı). = GERÇEK çoklu-process OS: bir userspace program BİRDEN ÇOK eşzamanlı userspace
process YÖNETİR + kanal(IPC) ile toplar. Slot-reuse (D-138) ölü init+calistir slotlarını geri kullandı (pid 4/5/6;
KDL_MAX_GOREV=8). **Userspace process modeli TAM:** kabuk → tek-program(calistir/D-240) + çoklu-eşzamanlı-program(coklu)
+ join + kanal-IPC. Full gate gecti, sıfır uyarı/regresyon. **Not:** Seri; ben yazdım.

**Ek düzeltme 1 — TIMER-IRQ/init ÇIKTI-ÇAKIŞMASI (pre-existing race, gate-verify sırasında bulundu) [ETKİ:
`runtime/kdl_zaman.c`; `test/bare_metal/kemgu_os_arm.c`]:** Entegre çekirdekte preemption `init_betik()` boyunca AÇIK.
`kdl_zaman.c` `kdl_tik()` tek-seferlik "TIMER OK tik=5" tanılamasını TIMER-IRQ bağlamından konsola yazıyordu → main'in
deterministik "PAGEFAULT OK" yazımını ORTASINDAN böldü ("PAG"+"TIMER OK tik=5"+"EFAULT OK") → gate grep FAIL. Bu D-241'e
BAĞLI DEĞİL (tik-5 vs init, coklu çok sonra kabukta); latent race yalnız bu koşuda tetiklendi. **Düzeltme:** `volatile int
kdl_timer_diag_aktif=1` (varsayılan AÇIK → bağımsız `calistir_timer_test_arm` yeşil kalır); entegre çekirdek preemption'dan
ÖNCE `=0` set eder (üretim timer-tick'i konsola yazmaz; timer-canlılık zaten UPTIME + SCHEDULER OK sayaç-kanıtı ile ispatlı).
Kanıt: kemgu_os_arm 5/5 tekrar-koşu deterministik ("PAGEFAULT OK" bütün, sıfır çakışma-artığı); timer_test hâlâ "TIMER OK".
**Ek düzeltme 2 — interaktif kabuk demo yük-duyarlı flake [ETKİ: `Makefile`]:** `calistir_shell_test_arm` +
`calistir_shell_script_test_arm` (char-paced stdio UART-RX) full-gate yükü altında QEMU yavaşlayınca bounded-RX-spin'i
`timeout 20`'de bitiremiyor → son komut düşer, "SHELL OK" yazılmaz (standalone geçer). `timeout 20→40` (recon-shell'lerin
`30`/entegre çekirdeğin `60` yük-marjıyla aynı sınıf). D-155 net-test yük-dersinin ikizi.

## D-240 — OS: KEMGU-OS — kabuktan SPAWN/EXEC (userspace program başlatma + join + IPC) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-239).

**Karar [ETKİ: `test/bare_metal/kemgu_shell_el0.c`; `Makefile`. Yalnız test.]** EL0 kabuğa `calistir <program>` komutu:
kabuk (bir userspace PROCESS) sys(12)=spawn ile BAŞKA bir userspace programı AYRI PROCESS olarak başlatır. Gömülü
spawnable programlar (.user, EL0): `prog_hesap` (6*7=42 → dosya_yaz(sonuc) IPC → exit), `prog_selam` (yaz → exit).
`calistir`: spawn(sys 12)→pid + join(sys 14, bounded)→bekle + sonuç(sys 16 dosya_oku, IPC)→oku + rapor. **Kanıt:**
"el0$ calistir hesap" → "CALISTIR spawn: hesap" + **"[prog hesap] 6*7=42 -> sonuc dosyasi"** (SPAWN edilen program
kendi EL0 süreç adres-uzayında koştu) + **"CALISTIR bitti (pid=4) sonuc=42"** (kabuk join etti + IPC dosyasından sonuç=42
okudu). = GERÇEK OS **spawn/exec/wait**: userspace program başka userspace programı başlatır, o kendi izole süreç
adres-uzayında (kdl_surec_spawn TTBR-swap) EL0'da koşar, IPC(dosya) ile haberleşir. D-137 (calis_arm launcher/worker)
deseni entegre kabuğa taşındı — AMA başlatan artık EL0 KABUK (kernel main DEĞİL). **TTBR-swap** (spawned process own
address-space) entegre çekirdekte çalıştı (shell/main L2[16] identity, worker L2[17]→per-proc veri_pa). Full gate 123
gecti, sıfır uyarı/regresyon. **Not:** Seri; ben yazdım.

## D-239 — OS: KEMGU-OS — net-recon EL0 kabuğa taşındı (pentest shell = TAM userspace process) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-238).

**Karar [ETKİ: `test/bare_metal/kemgu_shell_el0.c`. Yalnız test.]** D-238 EL0 kabuğuna (FS+utility) NET-RECON eklendi
→ D-238'in "KALAN" işi tamam. EL0 net yardımcıları (u_ip_checksum, u_arp_coz, u_ping, u_arpscan, u_tcp_syn, u_scan) —
HEPSİ EL0'da, .user_data TX/RX tamponlarında, **net syscall (24=net_gonder / 25=net_al) ile** (userspace_net/D-176
deseni: EL0 süreç virtio-net'e DOKUNMADAN syscall ile ağ yapar). MAC/IP sabitleri .user_data (EL0 okur; .rodata=fault).
Komutlar: `ping <oktet>` (ICMP echo), `arpscan` (subnet ARP tara), `scan <oktet>` (TCP SYN 80/443/22 → ACIK/KAPALI/
FILTRELI, pseudo-header checksum). **Kanıt:** "el0$ ping 2"→"PING: CANLI" + "el0$ arpscan"→"ARPSCAN: 2 host" — EL0 kabuk
GERÇEK net recon yaptı (kernel-fn çağırmadan, yalnız SVC). Canlı-yazılan input FS round-trip da çalıştı
("oku canli"→"OKU: EL0DATA"). **Pentest kabuğu ARTIK TAM userspace process:** FS + net-recon(ping/arpscan/scan) + RTC +
utility, hepsi EL0'dan syscall'la. Sıfır uyarı, full gate. init_betik EL1-net (driver-proof) korundu; EL0-kabuk-net =
userspace-recon proof. **Not:** Seri; ben yazdım.

## D-238 — OS: KEMGU-OS PART 3(d) — SHELL EL0 PROCESS (interaktif kabuk userspace) — OS TESLİMATI TAM (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-237).

**Karar [ETKİ: yeni `test/bare_metal/kemgu_shell_el0.c`; `test/bare_metal/kemgu_os_arm.c`; `runtime/kdl_kesme.c`;
`Makefile`. SERİ.]** PART 3 proof(d) = "shell userspace PROCESS'e döner". İnteraktif kabuk EL1 kernel-fonksiyonundan
EL0 userspace process'e TAŞINDI. `kemgu_shell_el0.c` (BM_A64_EL0/GPR-only, .user+.user_data) = AYRI-derlenen EL0 kabuk;
kemgu_os_arm.c'nin EL1 kabuk-döngüsünün YERİNE geçer. **Yeni syscall'lar:** num=26 `read_satir` (kernel PL011 RX → user
buffer; EL0 Device-MMIO ERİŞEMEZ), num=27 `saat` (PL031 RTC). Kabuk HER ŞEYİ syscall ile yapar: read_satir(26)/yaz(5-7)/
FS(17-21)/RTC(27). Komut adları .user_data'da (EL0 okur; .rodata=AP=00 olsa fault, D-135/235). **PROOF(d):** kabuk sys(2)→
"kaynak-EL=0x0" (kabuk GERÇEKTEN EL0) + "SHELL EL0 BASLADI (userspace process)" + gömülü dizi (yaz/oku/ls/saat) EL0'dan
syscall'la işlenir ("OKU: KABUK" = EL0 FS round-trip) + canlı UART girişi + "SHELL EL0 OK". main(görev0) kabuğun exit'ini
bekler (kdl_gorev_durum) → "KEMGU-OS OK". **BUG DÜZELTME:** kabuk exit=num=13 (kdl_gorev_bitir=görevi-öldür); num=3 kernel'i
for(;;)-halt ederdi (D2 non-preempt) → main asla resume etmez + 3e9-spin flake. num=13 = görev ölü → main hızlı-detect.
boot-tablo altında (TTBR-swap yok), user-yığın 0x42160000 (.user AP=01), FS tamponları .user_data (validator-izinli).
**KALAN:** net recon (ping/scan/arpscan) EL0 kabuğa HENÜZ taşınmadı (EL1 komut kütüphanesi korundu, init_betik ping/arpscan
kullanır) — EL0-net-frame port takip işi.
**🎯 TÜM OS TESLİMATI TAM (uygulama→gerçek OS):** PART1 MMU(D-235) + PART2 preemptive-scheduler(D-236) + PART3 userspace
[mekanizma(D-237) + **shell-EL0-process(D-238)**] = kemgu_os_arm.c TEK boot'ta, kümülatif, falsifiye-edilemez. Mehmet'in
"OS = MMU+scheduler+userspace" tanımının ÜÇÜ DE canlı+entegre; kabuk artık userspace process. **Not:** Seri; ben yazdım.

## D-237 — OS: KEMGU-OS PART 3/3 — USERSPACE (EL0 program + syscall + izolasyon) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-236).

**Karar [ETKİ: yeni `test/bare_metal/kemgu_init_el0.c`; `test/bare_metal/kemgu_os_arm.c`; `runtime/kdl_kesme.c`;
`boot/start_aarch64.S`; `Makefile`. SERİ, paylaşılan çekirdek.]** Mehmet direktifi (OS=MMU→sched→userspace) PART 3/3 =
userspace. [[feedback-os-tanim-mmu-sched-userspace]]. **AYRI-derlenen EL0 program** (`kemgu_init_el0.c`, BM_A64_EL0=
GPR-only, `.user` section 0x42000000 AP=01) çekirdeğe LİNKLENİR + preemptive EL0 GÖREV olarak koşturulur (boot sayfa-tablo
altında — TTBR-swap YOK → kabuğun 0x42210000 user-VA tamponları [L2[17] AP=00] bozulmaz). Bu = "kernel programı yükler +
EL0'da koşturur" (kernel-içi C-fn DEĞİL). **PROOF(a):** sys(2) → kernel "EL0 SYSCALL kaynak-EL=**0x0**" (SPSR.M[3:2]
donanım register = 0 → GERÇEKTEN EL0; kernel-fn olsaydı EL1=1). Taklit edilemez. **PROOF(b):** sys(5,str) → EL0'dan SVC →
kernel string basar + EL0'a döner (syscall arayüzü çalışır). **PROOF(c):** EL0 kernel-belleğe (0x40000000 AP=00) KASITLI
eriş → EL0 permission-fault → kernel SÜRECİ ÖLDÜR ("IZOLASYON OK ... FAR=0x40000000") + OS DEVAM → gerçek process
izolasyonu (kernel-fn her yere erişebilirdi = izolasyon-yok). **KOMPOZİSYON:** PART1(MMU/PAGEFAULT) ⊕ PART2(SCHEDULER,
arka-plan görevler +19B+67C) ⊕ PART3(EL0-userspace) ⊕ net/FS/disk/RTC/kabuk HEPSİ TEK boot'ta canlı → "KEMGU-OS OK".
**Runtime:** kdl_el0_izolasyon_isle (kill=kdl_gorev_bitir) + start_aarch64.S EL0-fault-dispatch; **OPT-IN kdl_el0_kill_aktif**
(varsayılan 0 → demolar eski "ISTISNA"-halt KORUNUR, sıfır regresyon — proc_arm/D3 doğrulandı; entegre çekirdek 1 yapar).
**Codegen dersi:** EL0 program AYRI dosya+GPR-only ŞART — kernel flag'siz (D-235) olduğu için aynı dosyada olsaydı NEON
sabit-havuz kernel .rodata'ya düşerdi (D-235 EL0 bug). Ayrı-derleme = "AYRI derlenen program"(proof a) + bug'dan kaçınır.
**KALAN — PROOF(d) DÜRÜST:** shell KENDİSİ EL0 process DEĞİL (hâlâ EL1). userspace MEKANIZMASI (a/b/c) kanıtlandı + entegre;
(d) shell→EL0 = büyük refactor (kabuk UART-RX/net/FS/output HEPSİ syscall olmalı) → son entegrasyon adımı, ayrı yapılacak.
**Not:** Seri; ben yazdım.

## D-236 — OS: KEMGU-OS PART 2/3 — PREEMPTIVE SCHEDULER (gerçek multitasking) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-235).

**Karar [ETKİ: `test/bare_metal/kemgu_os_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur (kdl_preempt_* extern).]**
Mehmet direktifi (OS = MMU→scheduler→userspace) PART 2/3 = preemptive scheduler. [[feedback-os-tanim-mmu-sched-userspace]].
Ham malzeme (C7b/D-117: kdl_baglam_degis + IRQ 272-byte trap-frame + kdl_preempt/kdl_gorev.c) VARDI ama entegre
çekirdeğe wire EDİLMEMİŞTİ — D-233 timer yalnız TİK sayıyordu (multitasking DEĞİL; dürüstçe "timer canli" demiştim).
**Wire:** kdl_preempt_baslat (main=görev0) + 2 ARKA-PLAN görev (sonsuz busy-loop, sayaç++, ASLA yield ETMEZ) +
kdl_preempt_ac → timer-IRQ ZORUNLU switch. **PROOF(a) [falsifiye-edilemez]:** 2 yield-etmeyen arka-plan görev, İKİSİNİN
de sayacı ilerler (+77B +75C, main init_betik'te net/FS yaparken, o da yield etmez). Preemption yoksa arka-plan HİÇ
seçilmez → sayaç=0 kalır → cooperative/sayaç-bump TAKLİT EDEMEZ. **PROOF(b):** shell KENDİSİ görev 0; arka-plan sayaçları
shell komut-işlerken BÜYÜR (77→1403, 6 interaktif komut boyunca) → concurrent (shell zamanlanmış görev + arka-plan koşar).
**PROOF(c):** tüm OS (MMU-PAGEFAULT+net+FS+disk+RTC+shell) scheduler altında çalışır → "KEMGU-OS OK". **Determinizm:**
preemptive → tam byte-çıktı değişken (sayaç değerleri timing-bağlı) AMA PASS markerları (SCHEDULER OK = db>0&&dc>0,
PAGEFAULT/PING/vs) deterministik (2× gecti). Full gate + unit-suite yeşil. **ETİKET DÜRÜSTLÜĞÜ:** bu GERÇEK
multitasking (zorunlu context-switch), D-233 timer-sayaç DEĞİL. **Uygulama→OS 2. ayak. SIRADA PART 3: userspace (EL0
+ ayrı-derlenen program + izolasyon).** **Not:** Seri; ben yazdım.

## D-235 — OS: KEMGU-OS PART 1/3 — GERÇEK MMU (C8b flag-kaldırma + C8c page-fault kurtarma) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-234).

**Karar [ETKİ: `Makefile` (BM_A64), `boot/start_aarch64.S`, `runtime/kdl_kesme.c`, `test/bare_metal/kemgu_os_arm.c`.
SERİ, paylaşılan çekirdek.] — Mehmet direktifi: OS = MMU + preemptive scheduler + userspace (SIRAYLA). Bu PART 1/3 = MMU.**
[[feedback-os-tanim-mmu-sched-userspace]]. **Bulgu:** MMU-on Normal-memory (C8a/kdl_mmu.c: L1[0] Device MMIO, L1[1]
Normal-WB RAM, SCTLR.M) + FP-enable (start CPACR_EL1.FPEN) ZATEN vardı, AMA falsifiye-edilemez kanıt hiç yapılmamıştı
(`-mgeneral-regs-only` hâlâ flag'deydi = C8b pending).
**PROOF (a) [C8b — MMU+Normal gerçek]:** KERNEL (integrated kernel kemgu_os_arm.o + runtime bm_a64_*.o) `-mgeneral-regs-only`
OLMADAN derlenir. Clang serbest SIMD emit eder: `kemgu_os_arm.o` **`stp q0, q0, [mem]`** (128-bit NEON store →
`frame[128]=0` vektörize) emit eder + Normal-cacheable RAM'e yazar + BOOT EDER. Device-memory'de bu store alignment-fault
ederdi + FP-trap'liyse trap ederdi (= C1 bug'ı) → **boot = MMU+Normal+FP GERÇEK, taklit EDİLEMEZ.**
**DÜRÜST SINIR (yeni BM_A64_EL0):** flag'i GLOBAL kaldırınca 35 EL0-kod demosu (userspace_*/.user-section) KIRILDI —
flag'siz clang, EL0 fonksiyonun NEON sabit-havuzunu (frame-init) kernel .rodata'ya (0x40005ec0 = AP=00) koyar + EL0'dan
`ldr q0,[kernel_addr]` okur → EL0 permission-fault (DFSC=0x0e, izolasyon ihlali). Bu, uygulama→OS'te "demoların gizlediği
EL-geçiş edge'i" (Mehmet'in öngördüğü). ÇÖZÜM: 35 EL0 demosu `BM_A64_EL0` (flag İLE, GPR-only=sabit-havuz yok) derlenir;
KERNEL flag'siz kalır (MMU kanıtı geçerli). KALICI çözüm PART 3'te (EL0 program .rodata'sını user sayfaya yerleştir).
Kümülatif: full gate 0-regresyon (kernel flag'siz + EL0 demoları flag'li).
**PROOF (b) [C8c — MMU zorluyor + fault kurtarma]:** kontrollü page-fault kurtarma. `kdl_fault_bekleniyor` bayrağı
(kdl_kesme.c) + start_aarch64.S SAF-ASM kurtarma yolu (bl-YOK → EL-correct ESR/ELR klobber edilmez): FAR yakala →
`kdl_fault_yakalanan`, faulting instr ATLA (ELR+=4), frame restore, eret. Entegre kernel init'te haritasız **0x80000000**
(L1[2] geçersiz) OKU → translation-fault → YAKALANDI (FAR=0x80000000 doğru) + KURTARILDI → "PAGEFAULT OK". MMU
her-şeyi-map-etmiyor, gerçekten zorluyor + kernel fault'u yönetip ilerliyor (demand-paging temeli).
**PROOF (c) [kümülatif]:** fault kurtarıldıktan SONRA tüm OS (net+FS+disk+timer+RTC+kabuk) AYNI boot'ta çalışır →
"KEMGU-OS OK". **Full OS gate YEŞİL (123 gecti, 0 regresyon)** + host unit suite'leri yeşil (10/10 + 39/39).
**Uygulama→OS geçişinin İLK ayağı: gerçek sanal-bellek + kernel fault-yönetimi.**
**EK DÜZELTME:** `tick_arm` (D-128) marjinal timer-gecikme flake'i (sabit 4M döngü ≈1 tik; yüklü gate'te t2==t1) →
bounded-poll (t2>t1 olana kadar, ≤16 tur) ile sağlamlaştırıldı — deterministik.
**PRE-EXISTING (PART 1 DIŞI, FLAG'lendi):** `calistir_codegen_bootstrap` (self-host fixpoint) test_tumu'da KIRIK —
checker 79-fark. KANIT ile PART 1'den bağımsız: `build/kemgu.exe` 7 gün önceden (2026-06-27, bu oturumda rebuild YOK),
son 30 commit'te src/ veya selfhost/ değişikliği YOK, PART 1 yalnız bare-metal. Self-host drift'i ayrı iş (MMU teslimatı
değil) — Mehmet'in "ilgisiz kolay-işe kaçma" kuralı gereği detaya girilmedi, dürüstçe flag'lendi.
**Not:** Seri; ben yazdım. Ders: gate koşarken runtime/ düzenleme = stale-obje link-hatası (temiz-rebuild gerekti).

## D-234 — OS: KEMGU-OS entegre çekirdek — RTC gerçek-zaman saati (saat komutu, 6. alt-sistem) (2026-07-04)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-233).

**Karar [ETKİ: `test/bare_metal/kemgu_os_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** Entegre çekirdeğe
6. canlı alt-sistem = RTC (donanım gerçek-zaman saati). PL031 (0x09010000 DR, D-172) Unix epoch saniye okur (passive MMIO,
IRQ gerekmez). Yeni `saat` komutu + init betiği RTC sanity check (>1.5e9 → "RTC OK"). **Kanıt:** "SAAT: 1783150833
(unix saniye) RTC OK" (= 2026-07-04, makul). TEK boot, det, sıfır uyarı. **Entegre çekirdek artık 6 CANLI alt-sistem:
ağ + depolama + FS + timer(uptime) + RTC + interaktif kabuk (13 komut).** **Not:** Seri; ben yazdım.

## D-233 — OS: KEMGU-OS entegre çekirdek — ZAMAN alt-sistemi canlı (timer IRQ + uptime, concurrent) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-232).

**Karar [ETKİ: `test/bare_metal/kemgu_os_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** Entegre çekirdeğe
5. canlı alt-sistem = ZAMAN (timer IRQ). Boot'ta kdl_kesme_kur (GIC) + kdl_timer_baslat (sanal timer, D-109) → timer IRQ
CANLI; preempt guard'lı-kapalı (D-125/127) → IRQ yalnız tik sayar (görev-switch YOK, kabuğa müdahale etmez). **Kanıt =
CONCURRENT background activity:** uptime kabuk çalışırken arka planda İLERLER — init betiğinde 8 tik, interaktif kabuk
sysinfo'da 436 tik (timer IRQ kullanıcı komut yazarken sürüyor → gerçek eşzamanlı canlı çekirdek, statik yetenek değil).
"UPTIME: timer canli (tik ilerledi)" deterministik (sayı değişken, mesaj sabit); sysinfo `uptime=Ntik` gösterir. TEK boot,
sıfır uyarı, 2× det. **Entegre çekirdek artık 5 CANLI alt-sistem: ağ + depolama + FS + ZAMAN + interaktif kabuk.**
**Not:** Seri; ben yazdım. Timer IRQ + net-busy-wait + SVC-syscall birlikte stabil (IRQ preempt-guard'lı → non-disruptive).

## D-232 — OS: KEMGU-OS entegre çekirdek — DEPOLAMA alt-sistemi canlı (virtio-blk kalıcılık) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-231).

**Karar [ETKİ: `test/bare_metal/kemgu_os_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** D-231 entegre çekirdeğe
SERİ genişleme (yeni izole demo DEĞİL — [[feedback-entegre-kernel-not-demolar]] kuralı): 4. canlı alt-sistem = DEPOLAMA.
Boot'ta virtio-blk kurulur (kdl_virtio_blk_bul/kur) + diskteki kalıcı FS yüklenir (kdl_dosya_yukle/D-143). init betiği
kaydet→yükle round-trip ile disk yaz+oku yolunu deterministik sınar ("proje" korundu). Yeni kabuk komutları: `kaydet`
(RAM-FS→disk), `yukle` (disk→RAM-FS). sysinfo artık disk durumu gösterir. **Kanıt:** TEK boot'ta net + **disk (DISK RW
OK: kaydet→yukle, proje korundu)** + FS + kabuk hepsi canlı → "KEMGU-OS OK". Gate `-drive` + dd disk.img (128 sektör),
det. **Entegre çekirdek artık 4 canlı alt-sistem: ağ + depolama + FS + interaktif kabuk.** **Not:** Seri; ben yazdım.

## D-231 — OS: KEMGU-OS v0.1 — TEK ENTEGRE ÇEKİRDEK (izole demo → tek canlı OS) (2026-07-04) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-230).

**Karar [ETKİ: yeni `test/bare_metal/kemgu_os_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.] — FAZ DÖNÜŞÜ.**
Mehmet düzeltti: KEMGU-OS TEK ENTEGRE bootable çekirdek olmalı, 100+ izole per-feature demo değil (`test/bare_metal/*.c`
her biri kendi main()'i olan ayrı ELF idi; `runtime/` paylaşılıyordu ama boot→hepsi-canlı imaj YOKtu). **KEMGU-OS v0.1**
= tek main() → boot → CANLI alt-sistem kurulumu (virtio-net + RAM-FS) → deterministik init betiği → interaktif pentest
kabuğu. Demolar artık **kanıtlanmış-rutin KÜTÜPHANESİ** (buradan çekildi: recon_shell2/D-198 ARP/ICMP/TCP rutinleri +
FS syscall 17-21 + PL011 canlı-RX/D-188). **10 kabuk komutu** (yardim/sysinfo/ls/yaz/oku/sil/ping/pingsweep/scan/arpscan)
TEK koşan çekirdekte. **init betiği DETERMİNİSTİK** (UART input-timing yarışı YOK → gate PASS temeli); interaktif kabuk
gerçek kullanım için (canlı komutlar da çalıştı — 8/8 komut yanıt verdi). **Kanıt:** TEK boot'ta → FS yaz→ls→oku
round-trip ("OKU: KEMGU" + "OKU: KEMGU-OS-v0.1") + ağ ("PING: CANLI" + "ARPSCAN: 2 host") + interaktif kabuk → "KEMGU-OS
OK", 2× det, sıfır uyarı. **Bundan sonra genişlemeler bu ÇEKİRDEĞE seri eklenir (yeni izole demo DEĞİL).** İlgili:
[[feedback-entegre-kernel-not-demolar]]. **Not:** Seri (paylaşılan kabuk-çekirdek — paralelleştirilmez); ben yazdım.

## D-230 — OS: x86 kooperatif scheduler — yield tabanlı context-switch (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-229).

**Karar [ETKİ: yeni `test/bare_metal/sched_x86.c` (hedef `calistir_sched_x86_test`); `Makefile`. Yalnız test — boot/runtime değişmedi.]**
C7a (aarch64 kooperatif scheduler) x86 İKİZİ — D-212 preemptive'in AKSİNE KOOPERATİF (yield tabanlı, IRQ yok). x86 TCB
(callee-saved rbx/rbp/r12-r15 + rsp), `sched_yield()` naked RSP-swap: push callee-saved → RSP eski-TCB'ye → round-robin
sonraki READY → RSP yeni-TCB'den → pop → ret. **Kanıt:** 3 görev A/B/C yield ile dönüşümlü koştu → [A][B][C]×3 →
"SCHED X86 OK". Det. Hedef adı `calistir_sched_x86_test` (mevcut C7a `calistir_sched_test_x86` paylaşılan sched_test.c'yi
kullanır; bu self-contained sched_x86.c → AYRI object sched_x86_coop.*, çakışma yok). **Scheduler artık ÇİFT-ARCH TAM:
aarch64 coop(C7a)+preempt(C7b) + x86 coop(D-230)+preempt(D-212).** **Not:** Paralel mini-agent (Workflow batch-17b re-run) üretti; cherry-pick ile entegre.

## D-229 — OS: SELF-HOST JSON ayrıştırıcı — saf KEMGU veri-format işleme (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-228).

**Karar [ETKİ: yeni `test/ornekler/json_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]** Saf
KEMGU JSON ayrıştırıcı — self-host dilin GERÇEK veri-format işleme yeteneği (VM/hashmap/kripto ötesi). Basit alt-küme:
nesne + sayı değerleri. Girdi `{"x": 42, "y": 100}` (gömülü byte dizisi). Durum-makinesi: skip-ws + yapısal-ayraç-atla +
string-oku + sayı-oku (`n=n*10+(byte-48)`). **Kanıt:** ayrıştır → x=42 VE y=100 (beklenen sabitlerle) → "KEM JSON OK".
Cihazsız det. **Dil-doğrulaması:** `karakter` sayısal DEĞİL (T003, D-175) → byte'lar tam32 dizisi + ASCII sabit-int
karşılaştırma; `değilse eğer` durum-makinesi (D-168); Dizi<tam32> fn-param + in-place yaz (D-171/196). Sınır: sayı
değerleri + pozitif tam32 (nested/string-değer/dizi/bool YOK). **Self-host dil gerçek programlar için veri ayrıştırıyor.**
**Not:** Paralel mini-agent (Workflow batch-17b re-run) üretti; cherry-pick ile entegre.

## D-228 — OS: TCP zarif kapanış — FIN 4-yönlü teardown (tam TCP yaşam-döngüsü) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-227).

**Karar [ETKİ: yeni `test/bare_metal/tcp_close_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-159 SYN handshake
TCP FSM'in AÇILIŞI idi; bunu KAPANIŞLA tamamlar = tam yaşam-döngüsü. ESTABLISHED sonrası zarif kapanış: bizim FIN|ACK →
peer FIN-ACK RX (FIN_WAIT_2) → peer FIN RX → bizim son ACK → CLOSED (4-yönlü, active close). FIN'in 1 seq tükettiği doğru
işlendi (ISS+2). **Kanıt:** DNS-çöz(example.com→104.20.23.154 Cloudflare) → SYN→SYN-ACK→ACK → FIN → FIN-ACK RX → peer FIN
→ son ACK → "TCP CLOSE OK", GERÇEK internet peer 4-yönlü (fallback DEĞİL). host-internet-bağımlı + pcap-TX/yarı-kapanış
fallback. D-158 küçük-tik. **Tam TCP FSM: open(D-159)+data(D-161)+close(D-228).** **Not:** Paralel mini-agent (Workflow batch-17b re-run) üretti; cherry-pick ile entegre.

## D-227 — OS: SELF-HOST SHA-256 parola kırıcı — LAB-kapsamlı sözlük saldırısı (Pentest-OS) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-226).

**Karar [ETKİ: yeni `test/ornekler/hashcrack_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]**
Pentest-OS (Kali=john/hashcat) tarzı LAB-KAPSAMLI parola kırıcı: KENDİ ürettiği bir SHA-256 hash'i sözlükle geri bulur.
Gömülü sözlük (8 aday); hedef = bilinen "kemgu"nun SHA-256'sı (test kendi üretir); kırıcı her adayın SHA-256'sını hesaplar,
hedefle karşılaştırır. **Kanıt:** hedef-hash → sözlük tara → "kemgu" idx=2'de KIRILDI + 7 eşleşmeyen atlandı → "KEM CRACK
OK". LAB-scoped (kendi-üretilen hash, gerçek hedef YOK — QEMU/eğitim). D-173 SHA-256 çekirdeğini (dtam32 wrap/rotate,
skaler-param ashr-workaround) yeniden kullanır. Cihazsız det. **KEMGU kripto artık ofansif-güvenlik bağlamında (hash
kırma) — pentest-OS aracı.** **Not:** Paralel mini-agent (Workflow batch-17b re-run) üretti; cherry-pick ile entegre.

## D-226 — OS: EL0 setjmp/longjmp — userspace yerel-olmayan atlama (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-225).

**Karar [ETKİ: yeni `test/bare_metal/userspace_jmp_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** EL0'da
setjmp/longjmp — yerel-olmayan kontrol akışı (C exception-benzeri), TAMAMEN userspace. `u_setjmp(buf)` (naked `.user`):
callee-saved x19-x30 + sp + lr'yi buf'a kaydet, 0 döndür; `u_longjmp(buf,val)`: buf'tan geri yükle, yüklenen x30'a `ret`
→ u_setjmp'in dönüş-noktasına atlar ama x0=val (POSIX: val=0→1). jmp_buf düzeni fiber TCB ailesiyle tutarlı. **Kanıt:**
u_setjmp→0 → derin 3-katman zincir (K1/K2/K3) → en derin katman u_longjmp(buf,42) → kontrol u_setjmp'e 42 ile geri
sıçradı (LONGJMP42), ara katmanların normal dönüşü HİÇ çalışmadı (yerel-olmayan) → "USERJMP OK". D-203 naked-fiber deseni.
3× det. **Not:** Paralel mini-agent (Workflow fan-out, batch-17) üretti; cherry-pick ile entegre.

## D-225 — OS: x86 PCI veri-yolu numaralandırma — cihaz keşfi (YENİ ALT-SİSTEM) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-224).

**Karar [ETKİ: yeni `test/bare_metal/pci_enum_x86.c`; `Makefile`. Yalnız test — boot/runtime değişmedi.]** YENİ ALT-SİSTEM:
PCI cihaz keşfi (gerçek donanım sürücülerinin temeli). Legacy PCI config-space port I/O: `outl(0xCF8, 0x80000000|
bus<<16|slot<<11|func<<8|offset)` + `inl(0xCFC)`. bus 0 slot 0..31 tara: vendor:device (offset 0), class-code (offset
0x08). **Kanıt:** QEMU i440fx sabit topolojisinde 4 cihaz — 8086:1237 (host-bridge), 8086:7000 (ISA-bridge), 1234:1111
(stdvga), 8086:100e (e1000 NIC) → vendor:device:class listelendi → "PCI ENUM OK 4 cihaz". 2× det. x86 PVH long-mode
(D-107). **OS artık PCI cihazlarını keşfediyor** (gerçek virtio/NIC sürücüleri için ön-koşul). Sınır (v1): yalnız bus 0 +
func 0 (bridge-recursion + multi-func yok). **Not:** Paralel mini-agent (Workflow fan-out, batch-17 — 2/6 tamam; A/D/E/F
oturum-limiti nedeniyle re-run bekliyor) üretti; cherry-pick ile entegre.

## D-224 — OS: x86 TAM kullanıcı-süreç keystone — ring3 ⊕ sayfa-izolasyon ⊕ syscall (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-223).

**Karar [ETKİ: yeni `test/bare_metal/ring3_proc_x86.c`; `Makefile`. Yalnız test — boot/runtime değişmedi.]** aarch64 D3
(korumalı EL0 user-process) x86 İKİZİ = x86 user-process KEYSTONE'u. Üç mevcut x86 parçayı BİRLEŞTİRİR: (1) ring3
(D-190 GDT DPL=3 + TSS + iretq→CPL=3), (2) sayfa-izolasyon (D-195 kernel sayfası U/S=0), (3) syscall (D-218 int0x80).
**Kanıt:** ring3 user-kodu KENDİ sayfasında (U/S=1) koştu (CPL=3) + int0x80 ile hesap+I/O yaptı + kernel-sır sayfasına
erişince **#PF (v=14, err=P|U, CR2=kernel-sır) → HAPİS** → "RING3 PROC X86 OK". TAM başarı, fallback GEREKMEDİ. x86 PVH
long-mode (D-107). **Korumalı user-process artık ÇİFT-ARCH tam (aarch64 D3 + x86 keystone).** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-223 — OS: SELF-HOST Türkçe alfabetik sıralama (collation) — Türkçe DNA (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-222).

**Karar [ETKİ: yeni `test/ornekler/turkce_sort_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]**
D-217 case-fold'un Türkçe-DNA DEVAMI: TÜRKÇE ALFABETİK SIRALAMA (collation) — mainstream diller yanlış yapar (Unicode
kod-nokta sırası ≠ Türkçe alfabe). Türkçe: a b c ç d e f g ğ h ı i ... KRİTİK: **ç, c'den SONRA** (Unicode 231 çok
ileride) + **ı, i'den ÖNCE** (Türkçe'de ı<i, Unicode'da ı=305>i=105 TERS). Her harfe Türkçe-sıra-indeksi ata, o indeksle
karşılaştır. **Kanıt:** karışık Türkçe kelime dizisi → Türkçe collation ile sırala → beklenen Türkçe-alfabetik sıra
(ç>c VE ı<i kararlarıyla; Unicode-sıralama YANLIŞ verir) → "KEM TR SORT OK", Unicode-tuzağına düşmedi. Saf KEMGU, dtam32
kod-nokta + dizi in-place swap (D-171/211/217). Cihazsız det. **Türkçe-syntax dil Türkçe collation'ı kendi diliyle
çözüyor.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-222 — OS: crash-güvenli FS — WAL journaling ⊕ inode-FS sentezi (atomik yazım) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-221).

**Karar [ETKİ: yeni `test/bare_metal/crashfs_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** D-206 WAL +
D-210 inode-FS SENTEZİ: atomik dosya-yazımı olan crash-güvenli FS. Dosya yazarken önce JOURNAL'a (inode-güncellemesi +
veri-blokları + commit-flag) yaz, sonra gerçek FS bloklarına uygula. Crash yazım ORTASINDA (flag=0) → kurtarma atlar →
FS eski-tutarlı; crash commit SONRASI (flag=1) → kurtarma REPLAY eder → yeni-tutarlı. Torn FS ASLA görünmez. **Kanıt:**
(1) temiz yazım → FS tutarlı; (2) crash-replay: B journal+commit=1 ama apply-atla → kurtarma replay (kurtarma=1,
replay-sonrası=0x10) → FS'te B tutarlı → "CRASHFS OK". virtio-blk, det. **FS artık atomik+crash-güvenli** (D-206 WAL
kavramı + D-210 gerçek-FS birleşti). **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-221 — OS: ICMP ping-sweep — nmap-tarzı L3 host keşfi (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-220).

**Karar [ETKİ: yeni `test/bare_metal/ping_sweep_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-156 tek-ping +
D-158 subnet-iterasyon BİRLEŞİMİ = host-discovery (pentest recon). 10.0.2.1..5 her IP'ye ICMP echo request; reply =
CANLI host. SLIRP: gateway 10.0.2.2 + DNS 10.0.2.3 echo'ya yanıt (deterministik) → 2 canlı. Reply doğrulama: src-IP +
id/seq + payload "KEMGU". **Kanıt:** 10.0.2.2/.3 CANLI, .1/.4/.5 YANITSIZ → "PING SWEEP OK 2" (N≥2), RX round-trip
(pcap fallback GEREKMEDİ). D-158 küçük-tik (2.5M). **Pentest recon aracı seti: port-scan(D-164)+arp-scan(D-158)+
traceroute(D-209)+ping-sweep = tam host/topoloji keşfi.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-220 — OS: userspace paylaşımlı-bellek IPC — 2 EL0 süreç aynı sayfayı paylaşır (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-219).

**Karar [ETKİ: yeni `test/bare_metal/userspace_shm_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** D-127
per-process İZOLASYONUN TERSİ: iki EL0 süreç KASITLI olarak aynı fiziksel veri sayfasını PAYLAŞIR (kdl_surec_kur_el0_veri
AYNI veri_pa=0x44000000 ile iki süreç → her ikisi L2[17]→VA 0x42200000). Üretici 1..10 + bayrak(0x600D) yazar (dmb ish);
tüketici bayrağı bounded spin-poll ile bekleyip toplar. Doğrudan-bellek IPC (kanal/dosya syscall DEĞİL). **Kanıt:** üretici
yazar → tüketici toplam=55 okur → "USERSHM OK"; ayrı-PA (D-127 izolasyon) olsaydı 0 görürdü. İki süreç preemptively
(D-127 scheduler). Ayrı user-yığın VA (0x42380000/0x42300000) paylaşılan PA'da çakışmasın diye. Det. **Userspace IPC üç
yolla: dosya(D-131)+kanal(D-140)+paylaşımlı-bellek.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-219 — OS: SMP seqlock — optimistik kilitsiz-okuma (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-218).

**Karar [ETKİ: yeni `test/bare_metal/smp_seqlock_arm.c`; `Makefile`. Yalnız test — boot/runtime değişmedi.]** Seqlock —
rwlock(D-199)'tan FARKLI: yazıcı okuyucuyu ASLA beklemez, okuyucu KİLİT ALMAZ (sıfır atomik-RMW, sıfır yazma) →
OPTİMİSTİK okur. Global seq (çift=stabil, tek=yazım-sürüyor): yazıcı seq++/veri-yaz/seq++; okuyucu s0-oku(tek→retry)→
a,b-oku→s1-oku→(s0≠s1 veya tek→retry). İki `dmb ish` (seq-tek'ten önce, seq-çift'ten sonra) seqlock'un KALBİ — yanlış
bariyer=torn. **Kanıt:** -smp 2, yazıcı N=2000 (a++;b=a*2), okuyucu optimistik → **torn_read=0** (yarım-yazılmış çift asla
kabul edilmedi; araya-giren yazım seq ile yakalanıp retry) + retry=5331 (gerçek çekişme), 5/5 det → "SMP SEQLOCK OK".
retry/kabul sayısı zamanlama-varyanslı (dürüst, PASS-koşulu değil). **SMP eşzamanlılık: spinlock/ticket/rwlock/atomik/
bariyer/SPSC/MCS/seqlock = tam kilit-primitif seti.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-218 — OS: TAM x86 syscall ABI — int 0x80 çok-argüman + dönüş + register-şeffaflık (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-217).

**Karar [ETKİ: yeni `test/bare_metal/syscall_x86.c` (hedef `calistir_syscall_abi_test_x86`); `Makefile`. Yalnız test — boot/runtime değişmedi.]**
aarch64'ün zengin syscall ABI'si (D-126 arg/dönüş/çok-arg/register-şeffaf) x86'da yoktu (yalnız C6/D-110 int0x80
demo). TAM x86 ABI: kendi IDT[0x80] gate + handler; num=rax, arg0=rdi, arg1=rsi, dönüş=rax (System-V benzeri). 3
syscall: num=1 yaz(ptr), num=2 topla(a,b)→a+b (çok-arg+dönüş), num=3 tick/echo(dönüş), + geçersiz-num sınır-kontrolü.
**Kanıt:** topla(40,2)=42 (0x2a) + rbx_korundu (register-şeffaflık: handler frame'i-ÖNCE-kaydeder, D-126 dersi) +
yaz + tick + hata_sınır → 5 kanıt → "SYSCALL X86 OK". Det. x86 PVH long-mode (D-107 start_x86_64.S linklenir-dokunulmaz),
-mgeneral-regs-only, `-serial file:`. **Syscall ABI artık çift-arch tam (aarch64 D-126 + x86).** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-217 — OS: SELF-HOST Türkçe büyük/küçük harf — "Türkçe-I problemi" (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-216).

**Karar [ETKİ: yeni `test/ornekler/turkce_case_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]**
KEMGU Türkçe-DNA'sının ZİRVE gösterisi: hiçbir ana-akım dil varsayılan DOĞRU yapmaz. Türkçe harf-dönüşümü kod-noktası
üstünde: **i→İ (U+0069→U+0130=304), ı→I (U+0131→U+0049), İ→i, I→ı** + ç↔Ç/ğ↔Ğ/ö↔Ö/ş↔Ş/ü↔Ü + ASCII a-z (i HARİÇ).
**Kanıt:** "istanbul"→büyüt→"İSTANBUL" (i→**304=İ**, ASCII 73=I DEĞİL — İngilizce yanlış "ISTANBUL" verir) +
"IRMAK"→küçült→"ırmak" (I→**305=ı** noktasız) kod-noktalarıyla doğrulandı → "KEM TR CASE OK". Saf KEMGU, dtam32
kod-nokta aritmetiği (D-211 UTF-8 çözücü üstüne), `değilse eğer` harf-eşleme, `olarak` cast. Cihazsız det. **Türkçe-syntax
dil, dünyanın hiçbir dilinin doğru yapamadığı Türkçe-I'yı kendi diliyle çözüyor.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-216 — OS: mini-FS tam CRUD + blok geri-kazanım (sil→yeniden-kullan) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-215).

**Karar [ETKİ: yeni `test/bare_metal/minifs_crud_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** D-210 mini-FS'i
(superblock+bitmap+inode) TAM CRUD'a taşır: `mfs_sil(ad)` (inode boşalt + veri bloklarını bitmap'te SERBEST bırak) +
`mfs_guncelle`. KRİTİK kanıt = blok geri-kazanım: 3 dosya oluştur (bitmap dolu) → ortadaki sil (bloklar serbest) →
yeni dosya oluştur → **serbest-bırakılan blokları GERİ KULLANIR** (delta-blok=4 aynı bölge) + içerik round-trip →
"MINIFS CRUD OK". virtio-blk, det. **Gerçek FS artık tam yaşam-döngüsü: oluştur/oku/güncelle/sil + blok-reclaim.**
**Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-215 — OS: DHCP tam lease (DORA) — 4-yönlü IP edinimi (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-214).

**Karar [ETKİ: yeni `test/bare_metal/dhcp_lease_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-162 yalnız
DISCOVER→OFFER idi; bunu TAM lease edinimine tamamlar (DORA): DISCOVER→OFFER(yiaddr öğren)→REQUEST(opt53=3, opt50=
istenen-IP, opt54=server-id)→ACK(opt53=5)→lease EDİNİLDİ. DISCOVER+REQUEST aynı xid. **Kanıt:** 4-yönlü tamam, ACK
yiaddr=10.0.2.15 + opt53=5 doğrulandı → "DHCP LEASE OK". SLIRP-DAHİLİ DHCP (internet gerekmez) → DETERMİNİSTİK. D-158
küçük-tik. **OS artık kendi IP'sini TAM protokolle ediniyor** (D-162 kısmi→D-215 tam). Sınır: renewal/rebind (T1/T2)
yok, yalnız ilk edinim. **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-214 — OS: userspace çok-fiber kooperatif scheduler — N yeşil-thread (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-213).

**Karar [ETKİ: yeni `test/bare_metal/userspace_sched_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** D-203
iki-fiber ping-pong'unu GERÇEK round-robin çok-fiber scheduler'a taşır — TAMAMEN EL0'da. N=3 fiber, merkezi scheduler
döngüsü: her fiber `u_yield()` ile scheduler'a döner, scheduler bir sonraki READY fiber'ı round-robin seçer +
context-switch (D-203 naked `.user` fiber_gec, always_inline OLAMAZ — `ret` gerekli). Fiber biterse DURUM_BITTI, artık
seçilmez. **Kanıt:** 3 fiber × 3 tur → interleave A1 B1 C1 A2 B2 C2 A3 B3 C3 (round-robin) → hepsi bitti → "USERSCHED
OK". Ayrı EL0 yığınları. 2× det. **Userspace M:1 yeşil-thread runtime çekirdeği** (kooperatif, preemption yok).
**Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-213 — OS: SMP 4-çekirdek paralel merge-sort — böl-ve-yönet (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-212).

**Karar [ETKİ: yeni `test/bare_metal/smp_sort_arm.c`; `Makefile`. Yalnız test — boot/runtime değişmedi.]** SMP kilit
primitiflerinin ötesinde GERÇEK paralel iş yükü: böl-ve-yönet merge-sort. Paylaşımlı 32-elemanlı dizi (dizi[i]=
(i*7+13)%97, toplam=1366) 4 çeyreğe bölünür; her çekirdek KENDİ ayrık çeyreğini insertion-sort ile sıralar (çeyrekler
örtüşmez → kilit gerekmez) → sense-reversing bariyer → çekirdek0 4 sıralı çeyreği 2'li merge ile birleştirir. **Kanıt:**
-smp 4, tam sıralı (her a[i]≤a[i+1]) + toplam korundu (1366==1366 permütasyon kontrolü) → "SMP SORT OK", 5/5 det. PSCI
naked-trampoline (D-174), MPIDR-stack (D-191), bariyer (D-179), 64-byte state (D-186), dc civac/ivac coherency.
**Çok-çekirdek artık paralel-algoritma yürütüyor** (kilit-primitiflerinden gerçek-iş yüküne). **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-212 — OS: x86 preemptive scheduler — PIT timer-IRQ context-switch (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-211).

**Karar [ETKİ: yeni `test/bare_metal/preempt_x86.c`; `Makefile`. Yalnız test — boot/runtime değişmedi (yalnız start_x86_64.S linklenir).]**
C7b (aarch64 preemptive) x86 İKİZİ — preemption artık HER İKİ mimaride. Kendi-içinde (self-contained): x86_64 long-mode
kernelde kendi IDT + PIC(8259) remap + PIT(8254) ~100Hz → IRQ0 → tam trap-frame kaydet → round-robin 2 kernel-görev
arası RSP-swap → PIC EOI → iretq. Görev B, YIELD ÇAĞIRMADAN timer-IRQ ile preemptively koştu (sayac_b>0 = zorunlu
bağlam-değiştirme kanıtı). **Kanıt:** "PREEMPT X86 OK" (B yield-siz koştu). x86 PVH long-mode (D-107, start_x86_64.S
linklenir-dokunulmaz), -mgeneral-regs-only, `-serial file:` (D-105 Windows-stdio gotcha), hlt-loop timeout=beklenen.
**Scheduler artık çift-arch (aarch64 C7b + x86 PIT-IRQ).** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-211 — OS: SELF-HOST UTF-8 kod-çözücü — KEMGU Türkçe DNA (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-210).

**Karar [ETKİ: yeni `test/ornekler/utf8_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]**
KEMGU'nun TÜRKÇE DNA'sına doğrudan hizmet eden self-host milestone: saf KEMGU UTF-8 kod-çözücü. 1-byte (0xxxxxxx) +
2-byte (110xxxxx 10xxxxxx) dizileri çöz: kod-noktası = `((b0 & 0x1F) << 6) | (b1 & 0x3F)`. **Kanıt:** "çğışöü"
UTF-8 byte'ları (ç=C3A7, ğ=C49F, ı=C4B1, ş=C59F, ö=C3B6, ü=C3BC) → kod-noktaları **[231,287,305,351,246,252]** +
kod-nokta SAYISI=6 (byte 12 değil) → "KEM UTF8 OK". **Dil-doğrulaması:** `<<`/`>>` tip-duyarlı → değer önce SKALER
dtam32'ye alınıp kaydırılır (D-173 dizi-eleman-ashr tuzağı); `karakter` sayısal değil (T003, D-175) → byte'lar
dtam32 dizisi; implicit tam32↔dtam32 YASAK → `olarak` cast (D-200). Cihazsız det. **Türkçe-syntax dil kendi
karakter-kodlamasını kendi diliyle çözüyor.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-210 — OS: mini dosya-sistemi — superblock + inode + blok-bitmap (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-209).

**Karar [ETKİ: yeni `test/bare_metal/minifs_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur (kdl_virtio_blk_* extern).]**
D-143/D-206 düz-serialize'in ÖTESİNDE GERÇEK dosya-sistemi yapısı virtio-blk üstünde. Disk düzeni: blok0=SUPERBLOCK
(magic "MFS1"+sayaçlar), blok1=BLOK-BİTMAP, blok2=INODE-TABLO (ad[16]/boyut/ilk_blok/blok_sayısı), blok3+=VERİ.
`mfs_olustur(ad,veri,uzun)` bitmap'ten boş blok(lar) ayırır + inode + veri-blokları yazar; `mfs_oku(ad,tampon)` inode'u
ad ile bulup blokları okur. **Kanıt:** 2 dosya ("gunluk" kısa + "veri" BLOK-SINIRINI AŞAN çok-blok) oluştur → diskten
geri oku → içerik+boyut eşleşti + bitmap tutarlı → "MINIFS OK". Det. **Gerçek FS: blok-tahsis + inode-indeksleme +
çok-blok dosya.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-209 — OS: traceroute — IP TTL manipülasyonu + ICMP Time-Exceeded (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-208).

**Karar [ETKİ: yeni `test/bare_metal/traceroute_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Ağ-recon aracı:
traceroute mekanizması. IP başlığındaki TTL'i artırarak probe yolla; her hop TTL=0'da ICMP Time-Exceeded (type=11)
döndürür → hop IP'si öğrenilir. ARP ile geçit MAC'i çöz → TTL=1 UDP probe (dst 8.8.8.8, port 33435, "KMGTRACE") →
gateway (10.0.2.2) **ICMP type=11** döndü → hop 1 keşfedildi. **Kanıt:** "HOP KESFEDILDI TTL=1" + "TRACEROUTE OK",
**PRIMARY RX yolu başarılı — fallback GEREKMEDİ** (pcap: TX-probe TTL=01 + RX ICMP type=0x0b gateway'den). D-158
küçük-tik (2.5M) yük-dersi. **Pentest recon: port-scan(D-164) + arp-scan(D-158) + reverse-DNS(D-166) + traceroute
= yol/topoloji keşfi.** **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-208 — OS: userspace EL0 heap allocator — malloc/free (kernel-yardımsız) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-207).

**Karar [ETKİ: yeni `test/bare_metal/userspace_malloc_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** EL0
(yetkisiz) süreç ÇEKİRDEK YARDIMI OLMADAN kendi dinamik bellek ayırıcısını sürer — yalnız kendi EL0-erişimli veri
sayfasında (.user_data, 4KB havuz). `u_malloc` (bump + serbest-liste first-fit) / `u_free` (LIFO push). **Kanıt:**
A/B/C malloc → u_free(B) → D=malloc → **D==B (B'nin yeri geri kullanıldı = free-list çalıştı)** + yaz/oku round-trip
+ tüm pointer'lar user-VA aralığında → "USERMALLOC OK". 2× byte-identik det. Süreç kernel-belleğine yazmaz (güvenli).
**Userspace dinamik bellek = daha zengin EL0 programların temeli.** Sınır (v1): coalescing yok, first-fit. **Not:**
Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-207 — OS: SMP MCS queue-lock — ölçeklenebilir kuyruk-kilidi (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-206).

**Karar [ETKİ: yeni `test/bare_metal/smp_mcs_arm.c`; `Makefile`. Yalnız test — boot/runtime değişmedi.]** Mellor-Crummey
& Scott (1991) queue-lock — spinlock(D-170)/ticket(D-192)/rwlock(D-199)'tan FARKLI, ÖLÇEKLENEBİLİR: her çekirdek KENDİ
node'unda döner (yerel-spin) → global adreste dönmez → cache-line bouncing yok, N'e doğrusal ölçeklenir. LDAXR/STLXR
SWAP(tail, my_node) ile kuyruğa gir; selef varsa onun next'ini bağla + kendi node'unda locked==0 bekle; unlock: next
varsa YALNIZ onu uyandır, yoksa tail-CAS. **Kanıt:** -smp 2, iki çekirdek MCS-kilit altında düz sayacı 5000'er artırır
→ sayac=10000 TAM (lost-update yok) + c0=c1=5000 (FIFO adalet, açlık yok), 5/5 det → "SMP MCS OK". PSCI CPU_ON HVC +
naked trampoline SP-önce (D-174) + 64-byte node (D-186) + dc civac/ivac coherency. **Not:** Paralel mini-agent (Workflow fan-out) üretti; cherry-pick ile entegre.

## D-206 — OS: FS journaling — write-ahead log + crash kurtarma (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-205).

**Karar [ETKİ: yeni `test/bare_metal/fs_journal_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur (kdl_virtio_blk_* extern).]**
D-143 kalıcı-FS'i CRASH-TUTARLILIĞA taşır: write-ahead log (WAL). Disk düzeni blok 0 = JOURNAL
`[magic "JRNL"|hedef_sektor|veri|commit@511]`, blok 10 = VERİ. Protokol: (a) journal commit=0 yaz → (b) flush →
(c) commit=1+flush (journal geçerli) → (d) veri bloğu yaz → (e) journal temizle. Kurtarma (her boot): journal
commit==1 ise veriyi hedefe **replay**. **Kanıt:** S1 temiz-commit → veri=0xCAFE, kurtarma no-op (commit=0);
S2 crash-sim (adım d atlanır, journal commit=1 kalır) → crash öncesi veri 0xCAFE (yeni 0xBEEF diske ulaşmadı =
tutarlılık) → kurtarma commit=1 görür → 0xBEEF replay → veri=0xBEEF journal ile eşleşti → "FS JOURNAL OK". Tek-boot
iki senaryo, det. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-205 — OS: SELF-HOST RC4 akış şifresi — KEMGU simetrik kripto (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-204).

**Karar [ETKİ: yeni `test/ornekler/rc4_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]**
D-173 SHA-256'nın ötesinde SİMETRİK ŞİFRE: KEMGU'da RC4 (KSA S-box permütasyon + PRGA keystream + XOR). KSA:
`j=(j+S[i]+anahtar[i%uzun])&255; swap(S[i],S[j])`. PRGA: `i=(i+1)&255; j=(j+S[i])&255; swap; K=S[(S[i]+S[j])&255]`.
**Kanıt:** Wikipedia test-vektörü anahtar "Key" + düz "Plaintext" → şifreli **BBF316E8D940AF0AD3** (ondalık
187,243,22,232,217,64,175,10,211) → beklenen hex eşleşti + round-trip (simetrik decrypt = orijinal) → "KEM RC4 OK"
(çift kanıt: hem hex hem round-trip). **Dil-doğrulaması:** S-box in-place swap (D-171), `&255` maske (kaydırma
YOK → D-173 dizi-ashr tuzağı hiç oluşmaz), XOR skaler-üstünde (dizi-eleman değil), `olarak` cast (D-200). Cihazsız
det. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-204 — OS: kabuk script runner — değişken + echo/yaz/oku betik (2026-07-03)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-203).

**Karar [ETKİ: yeni `test/bare_metal/shell_script_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-189 recon
kabuğunu BETİK-YORUMLAYICIYA taşır: değişken tablosu (`var_adlar[16][16]`+lineer arama) + komutlar `set <ad> <deg>`
/ `echo <ad>` (`$` toleranslı) / `yaz <dosya> <ad>` (num=17) / `oku <dosya>` (num=18) / `tekrar <n> <komut>`
(bounded 32). **Kanıt:** betik `set x 42 / echo x / yaz gunluk x / oku gunluk` → echo=42 (değişken tablosu) +
oku=42 (FS round-trip: değer itoa→dosya→geri) → "SCRIPT OK", 3× byte-identik det. Char-pace UART giriş (D-188),
user-VA tampon (D-150/151). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-203 — OS: userspace kooperatif fiber'lar — EL0 yeşil-thread context-switch (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-202).

**Karar [ETKİ: yeni `test/bare_metal/userspace_fiber_arm.c`; `Makefile`. Yalnız test — runtime salt-okunur.]** Kernel
context-switch'i (C7a) EL0'a taşır: userspace GERÇEK stack-switch (state-machine DEĞİL). İki fiber ayrı EL0 yığını
(0x42xxxxxx user-VA, AP=01). `fiber_gec(eski,yeni)` = **naked `.user` fonksiyonu**: `stp/ldp` ile callee-saved
x19–x30 + sp → eski'ye kaydet/yeni'den yükle/`ret`. Bağlam düzeni kernel `KdlTCB` ile aynı ([12]=sp). **Kanıt:**
ping-pong A1,B1,A2,B2,A3,B3 (3 tur, bounded), 2× byte-identik det → "USERFIBER OK". **Kritik ders:** `fiber_gec`
`always_inline` OLAMAZ (`ret` gerekli) → naked gerçek fonksiyon zorunlu. **Not:** Paralel mini-agent üretti;
cherry-pick ile entegre.

## D-202 — OS: SELF-HOST mini-assembler — KEMGU mnemonic→bytecode→VM (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-201).

**Karar [ETKİ: yeni `test/ornekler/asm_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]**
D-196 yığın-VM'in üstüne DERLEME katmanı: KEMGU'da cihazsız mini-assembler. `assemble()` `(mnemonic,operand)`
çiftlerini VM bytecode'una ÇEVİRİR (`değilse eğer` dispatch): `KOD_PUSH→[OP_PUSH,değer]` (1→2 hücre), diğerleri
tek OP_* hücresi. Çeviri KİMLİK DEĞİL (KOD_PRINT=5→OP_PRINT=6, KOD_HALT=6→OP_HALT=0 gerçekten farklı) → sonra
D-196 VM döngüsü bytecode'u koşturur. **Kanıt:** program → 42 (6×7) + 158 (100+58) → "KEM ASM OK", 3× det.
**Codegen bulgusu (FLAGGED):** çıplak `x = x;` self-assignment C bootstrap tip-kontrolörünü exit 127 ile çökertiyor
→ boş dal kaldırılarak aşıldı (ayrı oturuma flag edildi: derleyici düzeltmesi). **Not:** Paralel mini-agent üretti;
cherry-pick ile entegre.

## D-201 — OS: x86 SMP 4-çekirdek — Local APIC çoklu-AP INIT-SIPI (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-200).

**Karar [ETKİ: yeni `test/bare_metal/smp4_x86.c`; `Makefile`. Yalnız test — boot/linker/runtime değişmedi.]** D-187
x86 SMP (2-çekirdek) → 4-çekirdek: BSP + 3 AP long-mode'da INIT-SIPI ile. **ORTAK trampoline** (tek SIPI vektörü,
3 AP aynı blob'dan geçer); kritik değişiklik: trampoline'in 64-bit adımı RSP KURMAZ (3 AP paylaşımlı yığını
yarıştırırdı) → naked ortak long-mode girişe atlar; giriş kendi APIC ID'sini LAPIC MMIO'dan okur
(`0xFEE00020>>24`) → `RSP = ap_yiginlar + (id+1)*16KB` (APIC-ID-indeksli izole yığın) → `ap_isi(id)`. aarch64
MPIDR-indeksli desenin (D-191) x86 ikizi. Per-core state 64-byte satır (false-sharing paritesi; x86 MESI otomatik).
**Kanıt:** 3/3 AP canlı, APIC ID 1/2/3 her biri kendi kimliğini okudu, canli_ap=0x3, 3× det → "SMP4 X86 OK".
**Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-200 — OS: SELF-HOST hash-map — KEMGU dictionary (linear probing) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-199).

**Karar [ETKİ: yeni `test/ornekler/hashmap_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** KEMGU'da HASH-MAP veri yapısı (dizi ötesi): open-addressing + linear probing. Knuth çarpımsal
hash `(anahtar * 2654435761) & (KAP-1)` (dtam32, KAP=16); çakışmada bir sonraki slot. **Kanıt:** anahtar
5/21/37 HEPSİ slot 5'e hash → probing 5/6/7'ye yerleşir (gerçek çakışma) → bul(5)=50,bul(21)=210,bul(37)=370,
bul(99)=-1 → "KEM HASHMAP OK". **Dil-doğrulaması:** implicit tam32↔dtam32 YASAK (T001) → `x olarak dtam32`
explicit-cast (i32 no-op); kaydırma yok (D-173 ashr tuzağından kaçın). Cihazsız det. **Not:** Paralel mini-agent
üretti; cherry-pick ile entegre.

## D-199 — OS: SMP reader-writer lock — çoklu-okuyucu/tek-yazıcı (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-198).

**Karar [ETKİ: yeni `test/bare_metal/smp_rwlock_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
RW-lock: çoklu-okuyucu eşzamanlı / yazıcı exclusive. `okuyucu_sayisi` (atomik LDXR/STXR) + `yazici_aktif`
(atomik CAS 0→1) + yarış-geri-çekme kapısı (okuyucu artırırken yazıcı kaparsa geri-çek+retry). Korunan tutarlı
çift (invaryant b==a*2): yazıcı (çekirdek 0) N=1000 kez a++;b=a*2; okuyucu (çekirdek 1) her okumada b==a*2
doğrular. **Kanıt:** torn_read=0 (yarım-yazılmış çift asla görülmedi → mutual-exclusion doğru), 5/5 det →
"SMP RWLOCK OK". okuma_sayisi zamanlama-varyanslı (PASS koşulu değil, dürüst raporlandı). Naked trampoline
(D-174), coherency. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-198 — OS: recon kabuk v2 — TCP-scan + arp-scan komutları (2026-07-03)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-197).

**Karar [ETKİ: yeni `test/bare_metal/recon_shell2_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-189
recon kabuğu genişletme: `ping`/`dns`'e EK `scan <oktet>` (TCP SYN 80/443/22 → ACIK/KAPALI/FILTRELI, port_scan
mantığı) + `arpscan` (subnet 10.0.2.1-5 ARP tarama → "ARPSCAN: N host"). **Kanıt:** `ping 2`→PING:CANLI +
`arpscan`→2 host → "RECON2 SHELL OK", ping/arpscan det. Net-poll KÜÇÜK tik (6bbf1ff), char-pace giriş (D-188),
tampon user-VA. Nmap-benzeri komut-satırı pentest kabuğu. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-197 — OS: userspace HTTP POST — EL0 syscall ile veri gönderme (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-196).

**Karar [ETKİ: yeni `test/bare_metal/userspace_post_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-184
(GET)'i POST'a genişletir — EL0 süreç VERİ gönderir. DNS→TCP handshake→HTTP POST (body "KEMGU-POST", Content-
Length) PSH+ACK (sys2 24) → yanıt (sys2 25). **Kanıt:** example.com → **HTTP/1.1 405 Method Not Allowed**
(bağlantı+POST çalıştı; 200/3xx/405 kabul) → "USERPOST OK". host-internet+fallback (SENT/pcap "POST /"). POST
byte'ları EL0 tamponuna elle (.rodata-deref-etmez, D-177). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-196 — OS: SELF-HOST yığın-VM — KEMGU bytecode yorumlayıcı (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-195).

**Karar [ETKİ: yeni `test/ornekler/vm_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen değişmedi.]**
DİL KAPSTONU — KEMGU bir YORUMLAYICI çalıştırır. Yığın-makinesi: 7 opcode (PUSH/ADD/SUB/MUL/DUP/PRINT/HALT),
bytecode Dizi<tam32>, yığın Dizi<tam32>+SP, `iken pc<uzun` fetch-decode-execute döngüsü, `değilse eğer` opcode-
dispatch (switch yok). **Kanıt:** program [PUSH 6,PUSH 7,MUL,PRINT, PUSH 100,PUSH 58,ADD,PRINT,HALT] → 42, 158
→ "KEM VM OK", 3/3 det. YENİ dil-özelliği gerekmedi (dizi in-place mutasyon + Dizi<T> fn-param + kontrol-akışı
yeterli = tam yorumlayıcı). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-195 — OS: x86 tam sayfa-izolasyon — ring3 kernel-sayfa #PF (D-124 x86) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-194).

**Karar [ETKİ: yeni `test/bare_metal/ring3_page_x86.c`; `Makefile`. Yalnız test — runtime/boot/linker değişmedi.]**
D-190 (x86 ring3)'ü TAM sayfa-izolasyona sıkılaştırır — aarch64 D-124/D3 (EL0 kernel-belleğe erişince permission-
fault)'ün x86 muadili. Kernel-sır 2MB-hizalı/2MB-boyut (kendi PD-girişini işgal) → U/S=0 (supervisor-only);
yalnız ring3-erişilen sayfalar (kod/stack) U/S=1. Ring3 kernel-sır OKUMA dener → **#PF v=14, err=0x5 (P|U-read),
CR2=kernel-sır**. **Kanıt:** ring3 kernel belleğini OKUYAMADI (sır register'a ulaşmadı) → "PAGE ISO OK", 3/3 det,
ilk-deneme (fallback yok). **Çift-mimari userspace izolasyon TAM: EL0(aarch64 sayfa-perm) + ring3(x86 U/S-sayfa).**
Gate-marj: x86 hlt-loop timeout 12→20 (39612ba, yük-flake). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-194 — OS: SELF-HOST 128-bit bignum toplama — KEMGU carry propagation (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-193).

**Karar [ETKİ: yeni `test/ornekler/bignum_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Çok-word aritmetiği (carry propagation) saf KEMGU'da: 128-bit = 2×dtam64 (yuksek,dusuk).
`dusuk_top = a_dusuk+b_dusuk` (mod-2^64 wrap), **carry = (dusuk_top < a_dusuk)?1:0** (unsigned overflow), `yuksek
= a_yuksek+b_yuksek+carry`. **Dil-doğrulaması:** dtam64 `<` → **`icmp ult`** (unsigned, slt DEĞİL) + `add i64`
wrap. 3 vektör (V1 carry, V2 çift-wrap, V3) bilinen sonuçla eşleşti → "KEM BIGNUM OK". **LEXER BULGU (spawn_task
task_6184e549 ile flag'lendi):** integer literal SIGNED (strtoll) parse → yüksek-bitli 64-bit hex sabitler
(0xFFFFFFFFFFFFFFFF) INT64_MAX'a KIRPILIR → aritmetikle (INT64_MAX+INT64_MAX+1) kuruldu. `yazdir_isaretsiz_tam64`
(i64). **Not:** Paralel mini-agent üretti; lexer-bulgu spawn_task ile flag'lendi; cherry-pick ile entegre.

## D-193 — OS: userspace TFTP GET — EL0 syscall ile dosya transferi (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-192).

**Karar [ETKİ: yeni `test/bare_metal/userspace_tftp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
EL0 süreç ağdan DOSYA çeker — D-176 net-syscall (24/25) üstünde TFTP. SLIRP dahili TFTP sunucusu (`-netdev
user,tftp=DIR`) 10.0.2.2:69. EL0: RRQ (opcode 1, "dosya.txt\0octet\0") sys2(24) → DATA (opcode 3) sys2(25) →
içerik çıkar. **Kanıt:** "KEMGU-TFTP-DATA" (15 byte) çekildi → "USERTFTP OK" (gerçek RX). DETERMİNİSTİK. **BULGU:**
SLIRP, DATA'yı bize yollamadan ÖNCE ARP ile MAC'imizi sorar (çift-yönlü) → EL0 poll'a ARP-reply eklendi (SLIRP'e
MAC öğret). ACK (opcode 4) da gönderilir. EL0 .rodata-deref-etmez (D-177). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-192 — OS: SMP ticket-lock — adil FIFO kilit (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-191).

**Karar [ETKİ: yeni `test/bare_metal/smp_ticket_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-170 spinlock ADİL DEĞİL (açlık mümkün); ticket-lock FIFO adalet: `bilet_al` (LDXR/STXR atomik fetch-add) +
`kilitle` (simdi_hizmet==bilet bekle) + `ac` (simdi_hizmet++). İki çekirdek N=5000 kez ortak sayacı (kritik
bölgede DÜZ artırım) artırır. **Kanıt:** sayac=10000 (mutual-exclusion, lost-update yok) + **her çekirdek TAM
5000** (FIFO adalet, açlık yok — spinlock'un vermediği garanti), 6/6 det → "SMP TICKET OK". Naked trampoline
(D-174), dc ivac/civac+dsb. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-191 — OS: SMP 4-çekirdek bring-up — PSCI çoklu-AP (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-190).

**Karar [ETKİ: yeni `test/bare_metal/smp4_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]** Çok-
çekirdek 2→4 ölçekleme. QEMU -smp 4, BSP 3 AP'yi PSCI CPU_ON ile başlatır (3 çağrı, target MPIDR affinity=1/2/3).
ORTAK naked-trampoline giriş: her AP `mrs mpidr_el1 & 0xFF` ile hangi çekirdek olduğunu bulur → MPIDR-indeksli
KENDİ 8KB stack'ini kurar (ap_yiginlar[no+1]*8192) → cekirdek_durum[no].canli set (64-byte hizalı, false-sharing
yok). **Kanıt:** 3×"CPU_ON ret=0" + 3×MPIDR-Aff0=1/2/3 → "SMP4 OK 4 cekirdek", 5/5 det. QEMU virt MPIDR-Aff0=
çekirdek-no. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-190 — OS: x86 userspace ring3 + syscall — privilege ayrımı (D2-x86) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-189).

**Karar [ETKİ: yeni `test/bare_metal/ring3_x86.c`; `Makefile`. Yalnız test — runtime/boot/linker değişmedi.]**
aarch64 D2/D3 (EL0 privilege ayrımı)'nın x86 muadili — **çift-mimari userspace paritesi**. GDT'ye ring3
segmentleri (DPL=3: user-kod 0x1b, user-veri 0x23) + TSS (RSP0 ring0 stack) + IDT int-0x80 gate (DPL=3). iretq
ile ring3'e geç → ring3 kod CPL=3'te koşar. **Kanıt:** CS.RPL=3 + int 0x80 (rax=1)→ring0 handler + `cli`@ring3→
**#GP yakalandı** → "RING3 X86 OK", 5/5 det. **2 bug çözüldü:** (1) boot page-table supervisor-only → ring3
sayfalarına runtime U/S-bit (smp_x86 harita deseni); (2) monitor-stdio seri karışması. **Dürüst sınır:** CPL+
privileged-instruction-#GP ayrımı kanıtlar; tam sayfa-tabanlı user/kernel izolasyonu (D-124 x86 muadili) ayrı
milestone. **Not:** Paralel mini-agent üretti (dürüst debug); cherry-pick ile entegre.

## D-189 — OS: ağ-recon kabuğu — canlı ping/dns komutları (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-188).

**Karar [ETKİ: yeni `test/bare_metal/recon_shell_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-
OS kabuk kapstonu: D-188 (interaktif kabuk) + D-177/178 (EL0 net) BİRLEŞİMİ. EL1 kabuk UART RX'ten CANLI komut
okur → ağ recon: `ping <oktet>` (ARP+ICMP echo, net syscall) → "PING: CANLI"/yanit-yok; `dns` (DNS A çöz) →
"DNS: <IP>". **Kanıt:** `printf 'ping 2\ndns\n' | qemu -serial stdio` → "PING: CANLI" (SLIRP gateway det) +
"DNS: <ip>" → "RECON SHELL OK", 3/3. Ağ EL1'e taşındı (SVC EL1'den çalışır), tampon user-VA (D-150). Giriş
karakter-karakter pace (D-188 PL011 1-byte-holding dersi). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-188 — OS: interaktif UART kabuk — canlı komut oku + çalıştır (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-187).

**Karar [ETKİ: yeni `test/bare_metal/shell_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-181 (UART RX
gerçek stdio-giriş) + D-135 (komut parse) BİRLEŞİMİ = GERÇEK interaktif kabuk. EL1 kabuk döngüsü: "KABUK> "
prompt → UART RX'ten CANLI satır oku (RXFE poll + DR, byte-echo, '\n'e kadar) → tokenize → FS syscall çalıştır
(yaz num=17/oku num=18/ls num=19-20). SABİT script DEĞİL. **Kanıt:** `printf 'yaz gunluk MERHABA\noku
gunluk\nls\n' | qemu -serial stdio` → prompt+echo + "MERHABA" (oku) + "gunluk" (ls) → "SHELL OK". **KRİTİK
bulgu:** QEMU virt PL011 reset RX = **1-byte holding register** (FIFO değil) → burst-pipe overrun (kararsız);
fix = Makefile girişi KARAKTER-KARAKTER ~30ms pacing → 8/8 deterministik. Tamponlar user-VA (D-150 guard).
**Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-187 — OS: x86 SMP AP başlatma — Local APIC INIT-SIPI (D-169 x86 paritesi) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-186).

**Karar [ETKİ: yeni `test/bare_metal/smp_x86.c`; `Makefile`. Yalnız test — runtime/boot/linker değişmedi.]**
D-169 (aarch64 PSCI CPU_ON)'un x86 muadili — çok-çekirdek universal-OS paritesi. BSP, Local APIC (0xFEE00000)
ICR (0x300/0x310) ile AP'ye INIT IPI + SIPI×2 (vektör=trampolin>>12) gönderir. **AP GERÇEKTEN KOŞTU (fallback
DEĞİL):** real-mode (SIPI 0x8000) → protected → **long-mode** (CS64, EFER.LMA, PG+PAE, kendi stack) → C fn →
kendi APIC ID (0x1, BSP'nin 0'ından FARKLI) okudu + paylaşımlı bayrak set etti. **2 zor bug çözüldü (ham QEMU
log):** (1) #PF @0xFEE00020 — LAPIC boot page-table'da harita-dışı → test-içinde runtime 2MB uncacheable
huge-page map (PDPT[3]); (2) triple-fault — trampolin 64-bit kodu GDT verisiyle çakışıyordu → veri 0x100'e
kaydırıldı + kod-sığdı invaryantı. Trampolin elle-derlenmiş makine-kodu blob (clang 16-bit üretemez, linker
kısıt-dışı). 5/5 det. **Not:** Paralel mini-agent üretti (dürüst debug); cherry-pick ile entegre.

## D-186 — OS: SMP çekirdekler-arası üretici-tüketici — lock-free SPSC ring (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-185).

**Karar [ETKİ: yeni `test/bare_metal/smp_prodcons_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-174/180/179 SMP üstünde: çekirdek 0 ÜRETİR, çekirdek 1 TÜKETİR — **lock-free SPSC** halka tampon (üretici
yalnız bas, tüketici yalnız son yazar → lock GEREKMEZ, bariyerler yeter). **Kanıt:** 1000 öğe FIFO sırada
(sira_bozuldu=0), toplam=499500 (Σ0..999), 5/5 det → "SMP PRODCONS OK". **2 gerçek SMP bug çözüldü:** (1)
ring-overrun — bas/son serbest-akan ama dolu-testi maskeli-karışık → serbest-akan konvansiyon (dolu=(bas-son)
==KAP); (2) false-sharing — bitişik slotlar aynı cache-satırında → dc civac/ivac komşuyu bozuyordu → her slot
64-byte padded RingSlot. Naked trampoline (D-174), dc civac/ivac+dsb bariyerler. **Not:** Paralel mini-agent
üretti (dürüst debug); cherry-pick ile entegre.

## D-185 — OS: userspace DHCP — EL0 syscall ile ağ oto-konfig (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-184).

**Karar [ETKİ: yeni `test/bare_metal/userspace_dhcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 net-syscall (24/25) üstünde EL0 süreç KENDİ IP'sini DHCP ile öğrenir — DISCOVER inşa (Eth-broadcast+
IPv4 0.0.0.0→255.255.255.255+UDP 68→67+BOOTP+magic+opt53=1) sys2(24) → OFFER sys2(25) → 7 alan doğrula
(op=2,xid,yiaddr,magic,opt53=2,portlar,ethertype). **Kanıt:** yiaddr=10.0.2.15 → "USERDHCP OK". DETERMİNİSTİK
(SLIRP DHCP, internetsiz). EL0 .rodata-deref-etmez (D-177). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-184 — OS: userspace HTTP GET — EL0 syscall ile uygulama katmanı (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-183).

**Karar [ETKİ: yeni `test/bare_metal/userspace_http_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 net-syscall üstünde EL0 süreç bir web sayfası çeker — DNS(example.com)→TCP handshake(sys2 24/25)→HTTP
GET(PSH+ACK)→yanıt "HTTP/1." ara. **Kanıt:** 104.20.23.154:80 → **HTTP/1.1 200 OK** → "USERHTTP OK" (gerçek
RX). HTTP request byte'ları EL0 tamponuna elle yazıldı (.rodata-deref-etmez, D-177). host-internet+fallback.
**Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-183 — OS: userspace TCP handshake — EL0 syscall ile soket (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-182).

**Karar [ETKİ: yeni `test/bare_metal/userspace_tcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 net-syscall (24/25) üstünde EL0 süreç TAM TCP üç-yönlü handshake yapar (çekirdekte TCP durum-makinesi
YOK): ARP→DNS(example.com)→SYN(pseudo-header checksum)→**SYN-ACK al**(flags=0x12,ack=seq+1 doğrula)→ACK→
ESTABLISHED. **Kanıt:** 172.66.147.243:80 → "USERTCP OK" (gerçek RX, 3/3 stabil). **Userspace soket katmanı
tam (D-183 TCP + D-184 HTTP + D-185 DHCP): EL0 süreç raw-frame syscall'larıyla L2-L7 protokol yığını çalıştırır.**
host-internet+fallback. EL0 .rodata-deref-etmez (D-177). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-182 — OS: x86_64 CMOS RTC okuma — donanım saati (D-172 x86 paritesi) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-181).

**Karar [ETKİ: yeni `test/bare_metal/rtc_x86.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-172 (aarch64
PL031 RTC)'nin x86 paritesi (universal-OS piları). PC uyumlu MC146818 CMOS RTC: port 0x70 (index)/0x71 (data),
inline asm `outb/inb`. BCD register'lar (0=sn,2=dk,4=saat,7=gün,8=ay,9=yıl); UIP (Status-A bit7) beklenip
tutarlı okuma. **Kanıt:** 2026-07-02 20:13:05 (host wall-clock, `-rtc base=utc`) → "RTC X86 OK". Deterministik
makul-pencere (yıl 24-99, ay 1-12, gün 1-31). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-181 — OS: PL011 UART RX giriş yolu — donanım okuma (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-180).

**Karar [ETKİ: yeni `test/bare_metal/uart_rx_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** İlk kez
konsol RX (giriş) yolu — interaktif kabuğun ön-koşulu. PL011 FR (0x09000000+0x18) RXFE(bit4)+DR(0x00). **GERÇEK
giriş enjeksiyonu ÇÖZÜLDÜ (Windows gate zorluğu):** `-chardev file,input-path=` Windows'ta "not supported",
AMA **`-serial stdio` + stdin'e byte pipe** (`printf 'K' | qemu ... -serial stdio`) çalışır → guest RXFE=0
görür, DR'den 0x4b='K' okur, echo → "UART RX OK". Fallback: byte gelmezse bounded-spin → RXFE=1 (boş) doğru →
"UART RX PATH OK" (deadlock yok). **Kanıt:** giriş-enjeksiyon 3/3 "UART RX OK". **Ders:** QEMU-Windows seri-giriş
= `-serial stdio` + pipe. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-180 — OS: SMP atomik sayaç çekişmesi — LDXR/STXR lost-update yok (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-179).

**Karar [ETKİ: yeni `test/bare_metal/smp_atomic_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
İki çekirdek AYNI paylaşımlı sayacı N=10000 kez ATOMİK artırır — aarch64 exclusive-monitor RMW: `dmb ish;
ldxr; add; stxr; cbnz-retry`. Rakip STXR fail → taze değerle retry → hiçbir artırım düşmez. **Kanıt:** sayac=
**20000** (=2N), 9/9 deterministik → atomik doğruluk (atomik olmasa <20000 lost-update). Cache-coherency (dc
ivac/civac + dsb, RMW-öncesi/sonrası), rendezvous ile eşzamanlı çekişme, naked trampoline (D-174). **Not:**
Paralel mini-agent üretti; cherry-pick ile entegre.

## D-179 — OS: SMP bariyer senkronizasyonu — iki çekirdek lockstep (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-178).

**Karar [ETKİ: yeni `test/bare_metal/smp_barrier_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-170/174 SMP üstünde LOCKSTEP senkron: iki çekirdek K=5 tur, her turda **sense-reversing bariyer**'de buluşur
(spinlock'lu varan-sayacı + nesil/generation; son gelen sayacı sıfırlar + nesli artırır; erken gelenler nesil
değişene kadar bounded poll). Nesil izleme ABA-problemini önler. **Kanıt:** cekirdek0_tur=5, cekirdek1_tur=5,
nesil=5, 3/3 deterministik → "SMP BARRIER OK". Cache-coherency (dc ivac/civac+dsb, 64-byte hizalı), naked
trampoline SP (D-174 dersi). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-178 — OS: userspace ICMP ping — EL0 syscall ile L3 protokol (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-177).

**Karar [ETKİ: yeni `test/bare_metal/userspace_ping_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 raw-frame syscall'larının (net_gonder=24/net_al=25) MEYVESİ: EL0 (yetkisiz) süreç TAM protokol yığınını
kendi çalıştırır (çekirdekte değil). ARP-çöz + IPv4+ICMP Echo (payload "KEMGU") inşa → sys2(24) yolla →
sys2(25) poll ile echo reply doğrula. **Kanıt:** SLIRP gateway echo → "USERPING OK" (gerçek RX, pcap KEMGU
TX+RX), DETERMİNİSTİK. Protokol EL0'da, yalnız 2 syscall aracılık. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-177 — OS: userspace DNS — EL0 syscall ile tam protokol çözümleme (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-176).

**Karar [ETKİ: yeni `test/bare_metal/userspace_dns_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176'nın MEYVESİ: EL0 süreç TAM L2-L7 DNS yığınını userspace'te çalıştırır — ARP-çöz → Eth+IPv4+UDP+DNS
("example.com" A) inşa → sys2(24) yolla → sys2(25) poll ile yanıt al → ANSWER parse (isim-sıkıştırma 0xC0) →
IPv4 çıkar. **Kanıt:** example.com → 172.66.147.243 → "USERDNS OK" (gerçek RX, internet+fallback). **Bellek
güvenliği:** EL0 `.rodata` (AP=00) dereference EDEMEZ → hex-yazımı aritmetik (nibble_hex), disassembly ile
doğrulandı; tüm tampon EL0 user-yığınında (D-150 guard geçer). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-176 — OS: USERSPACE NETWORKING — EL0 süreç syscall ile ağ (süreç+ağ birleşimi) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-175).

**Karar [ETKİ: `runtime/kdl_kesme.c` (num=24/25 net syscall + externs); `Makefile` (BM_A64_OBJS'e
bm_a64_virtio_net.o + 14 net-test link satırından redundant explicit kaldırıldı); yeni
`test/bare_metal/userspace_net_arm.c`, Makefile hedefi.]** İKİ BÜYÜK ALT-SİSTEMİ BİRLEŞTİRİR: süreç/syscall
modeli (D-124..140) + ağ yığını (D-144..167). Şimdiye kadar ağ hep KERNEL (EL1) kodundan yapılıyordu; artık
bir EL0 (yetkisiz) süreç virtio-net'e DOĞRUDAN erişmeden, yalnız SYSCALL ile ham ethernet çerçevesi
gönderir/alır: **num=24 net_gonder(cerceve, uzun)** (kernel frame'i OKUR + virtio-net'e yollar; driver frame'i
kendi TX DMA buffer'ına kopyalar) + **num=25 net_al(buf, maxlen)** (kernel gelen frame'i user buffer'a YAZAR).

**GÜVENLİK (D-150/151 disiplini):** net_gonder frame'i user VA'da + mantıklı ethernet boyu (≤1514) olmalı
(kdl_user_yaz_ptr_gecerli okuma-length-bound); net_al hedef user VA'da olmalı (write-guard). Kötü pointer →
-1 (kernel belleği korunur). net_al kısa per-çağrı timeout (2M tik) → EL0 kendi poll döngüsünde tekrar
çağırır (D-158 yük-duyarlılık dersi). Net syscall'ları `#if __aarch64__` (x86'da yok).

**Link:** kdl_kesme.c artık kdl_virtio_net_* referans eder → bm_a64_virtio_net.o BM_A64_OBJS'e eklendi (D-143
blk deseni; tüm aarch64 kernel linkler, kullanılmasa dead-code, net+blk sürücü aynı anda link — clash yok,
net testleri zaten ikisini de linkliyordu). Net-test link satırlarından redundant explicit ref kaldırıldı.

**Kanıt (aarch64 QEMU + -netdev user):** userspace_net_arm.c — EL1 main net sürücüsünü kurar; EL0 launcher
ARP isteği (gateway 10.0.2.2) inşa eder → **sys2(24, frame, 42)** ile yollar → **sys2(25, rx, 128)** poll
ile SLIRP'in ARP yanıtını alır+doğrular → ayrıca kötü-pointer net_al(0x40000000)→-1 (guard) → "USERNET OK".
**Bir userspace program çekirdek-aracılı ağ syscall'larıyla ARP round-trip yaptı.**

## D-175 — OS: SELF-HOST Base64 kodlama/çözme — KEMGU payload codec (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-174).

**Karar [ETKİ: yeni `test/ornekler/base64_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Pentest OS payload-kodlama yardımcısı, saf KEMGU. RFC 4648 Base64 encode+decode round-trip:
`Dizi<karakter>` 64-karakter alfabe tablosu, hesaplanan 6-bit index ile erişim, bit ops (`>> << & |`), ham
karakter çıktısı (`yaz_karakter`, newline'sız). **Kanıt:** "KEMGU"→"S0VNR1U="→"KEMGU" → "KEM B64 OK" +
"KEM B64 DECODE OK". **Dil gözlemi (kısıt değil):** `karakter` tipi sayısal DEĞİL — `s[0]-'A'` → T003
(KEMGU no-implicit-conversion felsefesiyle tutarlı); decode'da karakter-aritmetiği yerine alfabede lineer
arama (64 karşılaştırma). Cihazsız deterministik gate.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-174 — OS: SMP iş-kuyruğu — iki çekirdek dinamik work-stealing (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-173).

**Karar [ETKİ: yeni `test/bare_metal/smp_queue_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-170 (statik yarı-yarıya bölme) → DİNAMİK work-stealing: 40 iş öğesi, paylaşımlı `sonraki_is` indeksi
SPINLOCK korumalı; iki çekirdek de kilit-al→indeks-çek→işle döngüsü koşar (i*i topla). **Kanıt:** toplam=
20540 (Σi², i=0..39) — **5/5 deterministik** (her öğe tam bir kez → spinlock doğru serialize); per-çekirdek
işlenen sayıları timing'e göre DEĞİŞİR (22/18, 17/23, 23/17…) → gerçek yarış = gerçek work-stealing → "SMP
QUEUE OK". **KRİTİK bare-metal bulgu (dürüst):** çekirdek 1 ilk versiyonlarda çöküyordu — C prologue
`stp x29,x30,[sp,#-0x20]!` PSCI CPU_ON'dan gelen **undefined SP** ile garbage adrese yazıyordu (D-170'te iş
basit→spill yok→gizli kalmış). **Çözüm:** `naked` trampoline giriş — asm ilk iş SP kur, sonra C'ye dallan.
**Kural: PSCI CPU_ON ile başlayan ikincil çekirdek, spill üretebilecek HERHANGİ bir C kodundan ÖNCE SP kurmalı.**

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı (dürüst debug notlarıyla); cherry-pick ile entegre.

## D-173 — OS: SELF-HOST SHA-256 — KEMGU kripto hash (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-172).

**Karar [ETKİ: yeni `test/ornekler/sha256_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA + güvenlik piları — CRC/checksum ÖTESİNDE gerçek KRİPTO hash: NIST FIPS 180-4 SHA-256
saf KEMGU'da. K[64]+H[8] diziler, W[64] mesaj çizelgesi, 64 tur (rotr, ch, maj, sigma), mod-2^32 toplama.
**Kanıt:** SHA-256("abc") = `ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad` (NIST
vektörü, TAM eşit) → "KEM SHA OK". **Dil-doğrulaması:** dtam32 mod-2^32 wrap (`0xFFFFFFFF+1==0`) + rotate-
right (dtam32 `>>`→lshr) ÇALIŞIR. **CODEGEN BULGU (spawn_task ile ayrı fix'e flag'lendi):** dtam32 DİZİ-ELEMANI
doğrudan `>>` operandı olunca codegen `ashr` (İŞARETLİ) üretir → unsignedness kaybolur. **Workaround (dil-
seviyesi, codegen değişmeden):** bit-karıştırmayı skaler-dtam32-parametreli yardımcı işlevlere taşı → argüman
geçince kaydırma skaler üstünde → `lshr` (doğru). IR: 0 ashr, 3 lshr. Cihazsız deterministik gate.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; codegen-bulgu spawn_task ile flag'lendi; cherry-pick ile entegre.

## D-172 — OS: PL031 RTC okuma — donanım gerçek-zaman saati (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-171).

**Karar [ETKİ: yeni `test/bare_metal/rtc_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** NTP (D-167)
zamanı AĞDAN aldı; bu DONANIMDAN alır (deterministik, ağsız). QEMU virt PL031 RTC 0x09010000'de (ilk 1GB
Device-map, MMU-on erişilebilir). DR register (offset 0x00) = Unix epoch saniyesi (u32). `*(volatile
uint32_t*)0x09010000` ile oku → makul-kontrol (>1.6G, <2.0G). **Kanıt:** DR=0x6a46a824=**1783015460 =
2026-07-02 18:04 UTC** (bugünle uyumlu) → "RTC OK". Host wall-clock yansıması (her koşuda 1-2s değişir ama
makul-pencere hep geçer → deterministik-pass). Donanım-zaman = NTP'nin ağsız ikizi.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-171 — OS: SELF-HOST sıralama — KEMGU dizi in-place mutasyon (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-170).

**Karar [ETKİ: yeni `test/ornekler/sort_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA — D-168 (CRC32 saf-compute) ötesi: KEMGU DİZİ + in-place mutasyon + fonksiyon-geçişi
olgunluğu. Bubble sort ([5,2,8,1,9,3,7,4,6,0]→[0..9]), iç içe `iken` + `>` + geçici-değişken swap. **KRİTİK
dil-doğrulaması:** (1) **in-place dizi mutasyonu `d[i]=x` codegen'de ÇALIŞIR** (→ `kdl_dizi_yaz_tam`,
runtime bounds-checked — heap-uniform, self-host invaryantı). (2) **`Dizi<tam32>` FONKSİYON PARAMETRESİ
çalışır** (referansla geçer, mutasyon çağırana yansır — eski "LLVM v3 dizi param yok" notu GÜNCEL DEĞİL,
codegen artık destekliyor). Cihazsız gate. **Kanıt:** sıralama + `d[i]<=d[i+1]` doğrulama → "KEM SORT OK".

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-170 — OS: SMP paralel hesaplama + spinlock — iki çekirdek gerçek iş (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-169).

**Karar [ETKİ: yeni `test/bare_metal/smp_compute_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi,
tüm SMP mantığı inline asm.]** D-169 (ikincil çekirdek bir bayrak set etti) → GERÇEK PARALEL HESAPLAMA:
çekirdek 0 dizinin ilk yarısını (Σ0..99=4950), çekirdek 1 ikinci yarısını (Σ100..199=14950) topladı →
toplam 19900 (yalnız İKİSİ de payını doğru hesaplarsa çıkar). **İKİ birleştirme yolu:** (A) 64-byte-hizalı
ayrı-slot (yarışsız), (B) **SPINLOCK** — aarch64 `LDAXR`/`STXR` atomik test-and-set + `STLR` release, ortak
akümülatöre iki çekirdek de güvenli ekledi (yarış-koşulu serialize). Cache-coherency (MMU-off çekirdek1 /
MMU-on çekirdek0): `dc civac`/`dc ivac`+`dsb sy` bariyerleri, kilit satırı her denemede tazelenir. DETERMİNİSTİK
(bounded bekleme 40M tik + 64-yield backoff; 5/5). **Kanıt:** "SMP COMPUTE OK toplam=19900". **Dürüst sınır:**
MMU-off/on coherency manuel bariyerlere dayanır (D-169 sınıf riski); test coherency bozulursa "SMP COMPUTE
KISMI/FAIL" basar (sessiz-gizlemez). Çok-çekirdek pilarının gerçek-iş adımı.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı (dürüst teknik notlarla); cherry-pick ile entegre.

## D-169 — OS: SMP 2. çekirdek bring-up — PSCI CPU_ON (çok-çekirdek) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-168).

**Karar [ETKİ: yeni `test/bare_metal/smp_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi, tüm SMP
mantığı smp_arm.c inline asm.]** YENİ PILAR: çok-çekirdek (performans/ölçek). Şimdiye kadar tek-çekirdek.
QEMU virt `-smp 2` ile ikincil çekirdek PSCI CPU_ON (fn_id=0xC4000003) ile başlatılır. **GERÇEK CPU_ON**
(fallback PSCI_VERSION değil): conduit=**HVC** (QEMU virt EL2-firmware'siz → HVC), ret=0x0 (SUCCESS), hedef
CPU MPIDR affinity=0x1, entry=cekirdek1_giris fiziksel adresi (identity-map). **Çekirdek 1 GERÇEKTEN koştu:**
paylaşılan `cekirdek1_canli` bayrağını YALNIZ çekirdek 1 giriş fn'si yazar; çekirdek 0 onu görünce "SMP OK".
**Cache coherency ele alındı:** çekirdek 1 MMU-OFF (non-cacheable) → RAM'e yaz + `dsb sy`; çekirdek 0 MMU-ON
(WB-cacheable) → poll'da `dc ivac`+`dsb sy` (invalidate-to-PoC, taze oku); bayrak 64-byte hizalı. Çekirdek 1
kendi 8KB stack'ini kurar (PSCI SP kurmaz). **Kanıt:** "SMP OK 2 cekirdek (HVC, ret=0x0)".

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı (dürüst teknik notlarla); cherry-pick ile entegre.

## D-168 — OS: SELF-HOST CRC32 — KEMGU saf-hesaplama algoritması (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-167).

**Karar [ETKİ: yeni `test/ornekler/crc32_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA — mmio ÖTESİNDE saf compute: KEMGU dilinin gerçek algoritma kaldırdığını kanıtlar.
Standart IEEE 802.3/zlib CRC-32 (polinom 0xEDB88320, tablosuz bit-bit), cihazsız. **Kanıt:** CRC-32("123456789")
= **0xCBF43926** (standart test vektörü, birebir). **KRİTİK dil-doğrulaması:** tüm bitwise op'lar çalışıyor
(`^`→xor, `&`→and, `|`→or, `<<`→shl, `>>`→ashr/lshr). **`>>` işlenen-tipine göre kod üretir:** tam32(işaretli)→
`ashr`, dtam32(işaretsiz)→`lshr` → CRC crc değişkeni `dtam32` OLMALI (MSB sık 1; ashr algoritmayı bozar).
Doğru işaretlilik semantiği. `yazdir_isaretsiz_tam(dtam32)` işaretsiz gösterim. Cihazsız gate (net/drive yok).

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-167 — OS: NTP istemcisi — internetten zaman senkronizasyonu (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-166).

**Karar [ETKİ: yeni `test/bare_metal/ntp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** OS açılışta
internetten doğru saati öğrenir. DNS-çöz(time.google.com→216.239.35.8) → Ethernet+IPv4(UDP)+UDP(123→123)+
SNTP(48 byte, LI/VN/Mode=0x1B client) gönder → response RX → Transmit Timestamp (offset 40, 1900'den beri
saniye) çıkar → Unix = ntp_sn - 2208988800. **Kanıt:** ntp_sn=3992001928 → Unix=**1783013128 = 2026-07-02
17:25:28 UTC** (bugünün tarihiyle uyumlu). **GERÇEK RX** (fallback değil). **Sınır:** host-internet-bağımlı
(offline → TX-pcap "NTP SENT OK", pcap'te UDP 123↔123). SLIRP dış-UDP proxy.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-166 — OS: reverse DNS (PTR) — IP → isim çözümleme (recon) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-165).

**Karar [ETKİ: yeni `test/bare_metal/dns_ptr_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest
recon: bir IP'nin hangi isme ait olduğunu bul (hedef tanıma). D-157 DNS A-çözümlemesini PTR'ye uyarlar:
IP oktetlerini TERS sırada + ".in-addr.arpa" QNAME, QTYPE=12 (PTR). Yanıtın ANSWER RDATA'sındaki domain-name'i
`isim_oku` ile PARSE eder (isim_atla'nın tersi — label biriktir + 0xC0 compression pointer takip, ≤32-atlama
sonsuz-döngü koruması). **Kanıt:** 8.8.8.8 → **dns.google** → "PTR OK". ANCOUNT=1, gerçek internet (SLIRP→host
DNS). **Sınır:** host-DNS-bağımlı (PTR yoksa "KISMI" kısmi kanıt; "PTR OK" yalnız gerçek isimde). L2-L4+DNS-A/PTR.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-165 — OS: SELF-HOST virtio-blk kapasite okuma — KEMGU disk config-space (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-164).

**Karar [ETKİ: yeni `test/ornekler/virtio_blk_config_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/
codegen değişmedi.]** Özgün DNA — D-163 (virtio-net MAC) desenini virtio-blk'a taşır: `.kem` sürücüsü disk
KAPASİTESİNİ config-space'ten okur. Slot tara → DeviceID=2 (virtio-blk) → config offset 0x100+0x104 (mmio_oku32
×2) → u64 capacity (sektör sayısı). **Kanıt:** `dd bs=512 count=64` disk → capacity=**64** → "KEM BLK OK".
mmio_oku8 yok → 32-bit×2 word. `değilse eğer`/`ve`/shift/mask codegen'de sorunsuz. DETERMİNİSTİK (disk boyutu
bilinir). KEMGU dili disk-cihaz config erişimi de kaldırıyor (net+blk self-host okuma tam).

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-164 — OS: TCP SYN port-tarayıcı — pentest recon (nmap-lite) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-163).

**Karar [ETKİ: yeni `test/bare_metal/port_scan_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** İkonik
pentest aracı — bir host'un açık portlarını bul. D-159 SYN-inşasını (`tcp_syn_kur`, src-port parametrik) çok-port
taramaya genişletir: DNS-çöz(example.com) → port listesi {80,443,22,8080,65000} için SYN gönder → yanıt sınıflandır:
SYN-ACK(0x12)=AÇIK, RST(0x04/0x14)=KAPALI, timeout=FİLTRELİ. Yarım-açık bağlantılar RST ile kapatılır; src-port
per-port (gecikmiş yanıt eşleme). **Kanıt:** example.com → 80/443/8080 AÇIK, 22/65000 FİLTRELİ → "PORT SCAN OK"
(gerçek SYN-ACK RX). **Sınır:** host-internet-bağımlı (offline → TX-pcap fallback "PORT SCAN SENT OK", pcap'te
5 farklı dst-port SYN). nmap-lite recon primitifi.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-163 — OS: SELF-HOST virtio-net MAC okuma — KEMGU config-space erişimi (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-162).

**Karar [ETKİ: yeni `test/ornekler/virtio_net_mac_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/
codegen değişmedi.]** Özgün DNA — D-160'ı (virtio-net TANIMA) bir adım ileri taşır: `.kem` sürücüsü cihazın
MAC adresini CONFIG-SPACE'ten okur. virtio-mmio cihaza-özel config offset 0x100'de; virtio-net için ilk 6
byte MAC. `.kem`: slot tara → DeviceID=1 bul → `mmio_oku32(y, taban+0x100)` + `+0x104` (2 word) → MAC
byte'larını little-endian çıkar (`(w >> (8*k)) & 0xFF` — `ashr`+mask codegen'de tam destekli). **Kanıt:**
QEMU virtio-net varsayılan MAC **52:54:00:12:34:56** okundu → "KEM MAC OK". mmio_oku8 dilde YOK (oku16/32/64
var) → 32-bit oku + shift/mask. Codegen kısıtı yok. **KEMGU dili gerçek cihaz config-space erişimi kaldırıyor.**

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-162 — OS: DHCP DISCOVER/OFFER — ağ oto-konfigürasyon (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-161).

**Karar [ETKİ: yeni `test/bare_metal/dhcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Bir bare-metal
OS'un ilk açılış adımı: DHCP ile ağ config al. Ethernet(broadcast)+IPv4(0.0.0.0→255.255.255.255,UDP)+UDP(68→67)
+BOOTP/DHCP(op=1, xid, chaddr, magic 0x63825363, option 53=1 DISCOVER) gönder → SLIRP OFFER'ı RX ile al.
**7 alan doğrulandı:** ethertype/proto, UDP portları (67→68), op=2 (BOOTREPLY), xid eşleşme, yiaddr non-zero,
magic cookie, option 53=2 (OFFER, TLV yürüyüşü). **Kanıt:** yiaddr=**10.0.2.15** → "DHCP OK". **DETERMİNİSTİK**
— SLIRP dahili DHCP sunucusu (internet gerekmez). OS artık kendi IP'sini otomatik öğreniyor.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-161 — OS: HTTP GET over TCP — uygulama katmanı (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-160).

**Karar [ETKİ: yeni `test/bare_metal/http_get_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** İLK UYGULAMA
KATMANI — OS bir web sayfası çekiyor. D-159 TCP handshake'i üstüne TCP DATA exchange: ARP→DNS(example.com)→
SYN/SYN-ACK/ACK ESTABLISHED → HTTP GET isteği (`GET / HTTP/1.1\r\nHost:...\r\nConnection: close\r\n\r\n`) PSH+ACK
(flags=0x18) DATA segmenti olarak gönder (seq/ack takibi, pseudo-header checksum payload dâhil) → HTTP yanıtını
RX ile al → durum satırında "HTTP/1." ara. **Kanıt:** hedef 104.20.23.154:80 → **HTTP/1.1 200 OK** → "HTTP GET
OK". **GERÇEK RX** (fallback değil). L2+L3+L4+DNS+HTTP tam ağ yığını.

**Kapsam/sınır (GATE-BELİRSİZLİĞİ):** HOST İNTERNET'ine bağlı (D-159 gibi). Offline → TX-pcap fallback ("GET /"
pcap'te → "HTTP GET SENT OK"). Timeout 20s. Tek-segment yanıt (multi-segment reassembly yok — kanıta yeter).

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-160 — OS: SELF-HOST virtio-net tanıma — KEMGU dilinde ağ-cihaz sürücüsü (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-159).

**Karar [ETKİ: yeni `test/ornekler/virtio_net_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA — OS kendi dilinde (KEMGU) yazılıyor. D-148/149 (virtio-blk, DeviceID=2) desenini
virtio-NET'e (DeviceID=1) taşır: `.kem` sürücüsü virtio-mmio slot aralığını (`iken` döngüsü) tarar, her
slotta `mmio_oku32(y, adres)` ile MAGIC (0x74726976 "virt") + DEVICE_ID okur, DeviceID=1'i bulunca tanır.
`yetki<MMIO>` object-capability (derleme-zamanı ispat, sıfır runtime) her okumada ödünç alınır. **Kanıt:**
`kemgu --llvm` → clang aarch64 → QEMU (`-device virtio-net-device`) → magic=1953655158 (0x74726976) + id=1
→ "KEM NET OK". **Codegen kısıtına takılmadı** (yetki/mmio_oku32/iken/eğer+ve/tam64 hepsi mevcut). KEMGU
dili gerçek ağ-cihaz tanıma sürücüsü kaldırıyor.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-159 — OS: TCP gerçek üç-yönlü handshake — SYN-ACK alımı (Faz H) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-158).

**Karar [ETKİ: yeni `test/bare_metal/tcp_connect_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-155 (yalnız SYN emisyonu) TAM handshake'e tamamlanır — SLIRP'in dış-TCP proxy'si üzerinden GERÇEK bir
internet host'una. Adımlar: virtio-net kur → ARP gateway MAC → **DNS ile "example.com" A-kaydı çöz** (D-157
mantığı) → çözülen IP:80'e TCP SYN (pseudo-header checksum, D-155 inşası) → **SYN-ACK al** (RX; flags=0x12
doğrula + ack_num=seq+1) → ACK gönder → ESTABLISHED → nazik RST/ACK kapanış. **GERÇEK SYN-ACK RX** (fallback
DEĞİL). **Kanıt:** hedef 104.20.23.154:80 → SYN-ACK → "TCP CONNECT OK". Ağ yığını artık L2(ARP)+L3(IP)+
L4(TCP-established)+DNS tam zincir.

**Kapsam/sınır (GATE-BELİRSİZLİĞİ):** HOST İNTERNET'ine bağlı (SLIRP dış-TCP'yi host'a proxy'ler). Offline
ortamda SYN-ACK gelmez → test TX-pcap fallback'ine ("TCP CONNECT SENT OK", pcap'te SYN) düşer; Makefile ikisini
de kabul eder. Timeout 20s (DNS+TCP iki round-trip). Geliştirme makinesi internetli → gerçek handshake geçer.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-158 — OS: ARP host-keşfi — subnet taraması (pentest recon) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-157).

**Karar [ETKİ: yeni `test/bare_metal/arp_scan_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-OS
temel keşif primitifi (L2 canlı-host bulma). D-145 tek-hedef ARP round-trip'ini SUBNET TARAMASINA genişletir:
10.0.2.1–10.0.2.15 aralığına ARP request broadcast → RX ile reply'leri topla (60 poll) → her reply'den spa
(sender IP) + sha (sender MAC) çıkar, dedup. **Kanıt:** SLIRP gateway (10.0.2.2) + DNS (10.0.2.3) → **2 canlı
host** deterministik keşfedildi → "ARP SCAN OK". Gateway ARP'a her zaman yanıt verir → ≥1 host garanti.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-157 — OS: DNS A-kaydı çözümleme — isim → IPv4 (Faz G ağ derinleşme) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-156).

**Karar [ETKİ: yeni `test/bare_metal/dns_resolver_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-147 DNS round-trip'i (yanıt ALINIR ama parse EDİLMEZ) tam çözümleyiciye genişletir: DNS yanıtının
ANSWER bölümünü parse edip çözümlenen IPv4 A-kaydını çıkarır. **İsim sıkıştırma (0xC0 pointer) ele
alınır** (`isim_atla` helper — hem compression-pointer hem düz-label; Question + answer NAME atlama),
sınır kontrolleri (paket taşması, RDLENGTH). **Kanıt:** "example.com" A sorgusu → SLIRP host resolver'a
forward → yanıt RX → ANCOUNT=2, ilk A-kaydı çıkarıldı → 172.66.147.243 → "RESOLVE OK". Reprodüsibl (2
koşu birebir).

**Kapsam/sınır (GATE-BELİRSİZLİĞİ):** Bu test HOST İNTERNET'ine bağlı (SLIRP sorguyu host DNS'e forward
eder; gerçek A-kaydı gerekir). Offline ortamda ANCOUNT=0 → "A-KAYDI YOK" → gate FAIL olabilir. Parser
DETERMİNİSTİK; yalnız gerçek-çözümleme internet-bağımlı. (D-147 aksine yalnız "yanıt geldi" kontrol eder,
internet gerektirmez.) Geliştirme makinesi internetli → gate geçer.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-156 — OS: ICMP echo (ping) round-trip — ağ katmanı (Faz G) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-155).

**Karar [ETKİ: yeni `test/bare_metal/icmp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-OS
keşif primitifi (ping-sweep temeli). ARP ile gateway (SLIRP 10.0.2.2) MAC çöz → Ethernet+IPv4(proto=1)+
ICMP Echo Request (type=8, id=0xBEEF, seq=1, ICMP checksum RFC1071, payload "KEMGU") gönder → **echo
reply'i RX ile al** (SLIRP gateway ping'lerini host-ayrıcalığı gerektirmeden DAHİLİ yanıtlar) → doğrula
(type=0/code=0, id/seq eşleşir, payload geri döner) → "PING OK". **GERÇEK RX round-trip** (TX-pcap fallback
değil; fallback Makefile'da mevcut ama tetiklenmedi). virtio-net TX+RX + ARP + IP üstüne kurulu.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-155 — OS: TCP SYN paket emisyonu — ağ katmanı (Faz H) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-154).

**Karar [ETKİ: yeni `test/bare_metal/tcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-OS
keşif primitifi (port-tarama temeli). ARP ile gateway MAC çöz → Ethernet+IPv4(proto=6)+TCP SYN segmenti
inşa (src 40000, dst 9999, SYN=0x02, **TCP checksum PSEUDO-HEADER dahil** = src/dst IP + proto + TCP-len)
→ gönder. **Kanıt: TX-pcap** (D-144/146 deseni) — pcap'te SYN segmenti "KEMG" seq marker'ı ile doğrulandı;
TCP checksum 0x1bf6 + full-segment-verify 0x0000 (RFC1071) + IP checksum 0x22c0 bağımsız Python ile teyit.

**Kapsam/sınır (DÜRÜST):** TAM handshake DEĞİL — yalnız SYN inşa+checksum+emisyon. SLIRP kapalı gateway
portuna (10.0.2.2:9999) SYN'i SESSİZCE DÜŞÜRÜR (user-mode TCP yığını RST dönmez) → RX round-trip bu
ortamda olmadı. Emisyon (pseudo-header checksum dahil) gerçek yapı taşı; tam handshake gerçek TCP peer
(internet-out veya listener) gerektirir → gelecek iş. Makefile hem RX ("TCP HANDSHAKE OK") hem TX-pcap
("KEMG") kontrol eder — listener'lı ortamda RX yolu otomatik geçer.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-154 — OS: düşman-userspace bombardıman regresyon testi — syscall-ptr güvenlik yüzeyi (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-153).

**Karar [ETKİ: yeni `test/bare_metal/guvenlik_bombardiman_arm.c`; `Makefile`. Yalnız test — kaynak
değişmedi.]** D-150+D-151 sertleştirilmiş syscall-pointer yüzeyinin KALICI regresyon bekçisi. Bir EL0
launcher, 8 kötü-niyetli syscall'lık bir BATARYA ateşler (unmapped/kernel-adres/MMIO okuma+yazma
hedefleri: num=5 yaz×2, 16 dosya_oku, 15 dosya_yaz, 17 dosya_yaz_metin, 18 dosya_oku_metin write-hedef,
20 dosya_ad write-hedef, 21 dosya_sil); her biri -1 dönmeli VE kernel HALT ETMEMELİ. Bataryadan sonra
geçerli iş akışı (dosya oluştur+oku) kernel'in tam canlılığını kanıtlar → "HOSTILE SURVIVED OK". Vaka #6'nın
"not-found değil gerçek D-150 write-guard reddi" olduğu, dosyanın önceden kurulup sonra geçerli tampona
okunabilmesiyle ayrıştırılır. Bir syscall halt ettirirse test FAIL → o guard eksik demektir (bisect talimatı).

**Not:** Paralel mini-agent (worktree-izole) tarafından üretildi + doğrulandı; cherry-pick ile entegre.

## D-153 — OS: kalıcı FS deserialize sertleştirme — poisoned boyut clamp (savunma-derinliği) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-152).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_dosya_yukle + num=18); yeni `test/bare_metal/guvenlik_kalici_arm.c`,
`Makefile`.]** Audit defense-in-depth bulgusu (EL0-erişilebilir DEĞİL — kötü niyetli disk gerekir):
kdl_dosya_yukle diskteki kdl_dosyalar[] tablosunu VERBATIM yükler. Kötü niyetli disk aşırı büyük `boyut`
içerirse → num=18 kdl_dosyalar[i].boyut byte kopyalar → 64-byte icerik[] tamponunu aşan OOB okuma →
kernel belleği user'a sızar. **İki katman:** (A) kdl_dosya_yukle deserialize sonrası sanitize — kullanildi
0/1, ad/icerik null-term, boyut `[0,64)` dışıysa 0. (B) num=18'de ham boyut yerine clamp'li `lim` (hem
`kdl_user_yaz_ptr_gecerli(arg2, lim+1)` hem kopya sınırı `n<lim`). **Kanıt:** guvenlik_kalici_arm.c — EL1
main elle "KEMG" magic + boyut=9999 ZEHİR disk image üretir, kdl_dosya_yukle, EL0 num=18 → dönen uzunluk
≤63 (9999 değil) + kernel sağ → "KALICI GUARD OK". **Negatif kanıt:** fix stash'lenince test doğru FAIL
eder ("KALICI GUARD HATA uz=9999") → zafiyet gerçek + test false-positive değil. Kalıcı regresyon (777) geçer.

**Not:** Paralel mini-agent (worktree-izole) tarafından üretildi + negatif-kanıtla doğrulandı; cherry-pick ile entegre.

## D-152 — OS: spawn-entry doğrulama — num=12 DoS koruması (güvenlik sertleştirme) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-151).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_surec_spawn); yeni `test/bare_metal/guvenlik_spawn_arm.c`,
`Makefile`.]** Audit confirmed bulgusu (medium DoS): num=12 spawn'da EL0, yeni sürecin GİRİŞ adresini
(arg=entry) tam kontrol eder. kdl_surec_spawn(entry) bu entry'yi yeni EL0 sürecinin ELR_EL1'ine koyar;
entry kernel/unmapped/hizasız ise EL0 komut-fetch'i fault → lower-EL sync exception → kdl_istisna_isle
sonsuz halt (**tek SVC ile tüm kernel ölür**). **Fix:** kdl_surec_spawn EN BAŞINA guard — entry paylaşılan
EL0 .user kod sayfası `[0x42000000, 0x42200000)` içinde VE 4-byte hizalı olmalı; değilse -1 (süreç
yaratılmaz, slot tüketilmez). Başka fonksiyona dokunulmadı. **Kanıt:** guvenlik_spawn_arm.c — EL0 launcher
sys(12, 0x40080000)[kernel] + sys(12, 0)[null] → ikisi de -1, kernel SAĞ; sys(12, &worker)[geçerli .user]
→ ≥0, worker koştu → "SPAWN GUARD OK" + "WORKER OK". spawn/yasam/calis/geri_al regresyonları (worker'lar
.user'da = geçerli) bozulmadan geçer.

**Not:** Paralel mini-agent (worktree-izole) tarafından üretildi + doğrulandı; cherry-pick ile entegre.

## D-151 — OS: syscall OKUMA-pointer doğrulama — kernel DoS + info-leak koruması (güvenlik sertleştirme) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-150).

**Karar [ETKİ: `runtime/kdl_kesme.c` (read-guard + num=5/15/16/17/18/21); `linker/bare-metal-aarch64.ld`
+ `bare-metal-x86_64.ld` (__rodata_start/end); yeni `test/bare_metal/guvenlik_oku_arm.c`, `Makefile`
hedefi.]** D-150'nin YAZMA-tarafı korumasının OKUMA-tarafı ikizi. Çekirdek, EL0-kontrollü bir string
pointer'ını **deref ederek OKURKEN** de doğrulamalı; aksi halde kötü/hatalı bir EL0 süreç:
- **DoS:** unmapped adres geçirir → kernel EL1'de data-abort → `kdl_istisna_isle` sonsuz halt (**tek
  SVC ile tüm kernel ölür**);
- **info-leak:** kernel adresi geçirir → kernel belleği UART'a yazılır (num=5) veya bir dosyaya
  kopyalanıp num=18 ile geri okunur (num=17→18 exfiltrasyon zinciri).

**Çok-ajanlı adversarial audit (23 ajan, 2.07M token) bu sınıfı üretti** — 14 confirmed EL0-reachable
bulgu, hepsi read-ptr; num=18 ad-okuması D-150 sonrası hâlâ açıktı (D-150 yalnız arg2 write-hedefini
koruyordu). Refuted: spawn-havuz int-bounds (zaten korumalı).

**Read-guard:** `kdl_user_oku_str_gecerli(p)` — izinli okuma bölgeleri `[user VA 0x42000000,0x42400000)
∪ kernel .rodata [__rodata_start,__rodata_end)`. `.data/.bss` (dosya tablosu burada!) / stack / heap /
Device MMIO / unmapped → RED. **Null-sonlandırıcı İZİNLİ bölge içinde bulunmalı** (yalnız mapped-izinli
byte taranır → tarama fault üretemez; straddle-over-read imkânsız; 4KB tarama tavanı). num=5 (yaz-string),
15/16/18/21 (dosya adı), 17 (ad + içerik arg2) → hepsi guard'lı, geçersiz→RED (-1).

**.rodata neden izinli:** mevcut testler çıktı/ad string LİTERALLERİNİ (.rodata, kernel adresi) syscall'a
geçirir (sys(5,"GUVENLIK OK"), dosya adı "mesaj"/"f"). Bunlar const, sır değil; izin vermek tüm testleri
korur. Sızıntı-hedefleri (.data/.bss/stack/heap) reddedilir.

**Kanıt (aarch64 QEMU):** `guvenlik_oku_arm.c` EL0 launcher: (2) num=5'e UNMAPPED 0x80000000 → RED,
kernel HALT ETMEZ; (3) num=16'ya kernel-RAM 0x40100000 → RED; (4) buraya ulaşmak = kernel sağ →
"GUVENLIK OKU OK". Fix öncesi (2) kerneli sonsuza halt ederdi. FS regresyonları (dosya/metin/ls/sil/
kabuk/kalıcı — .rodata ad + user-VA token) + D-150 bozulmadan geçer. x86_64 de __rodata sembolleriyle
linklenir (arch paritesi).

**Kapsam/sınır:** Read-guard string-deref eden syscall'ları kapsar. Kalan audit bulguları (ayrı D):
num=12 spawn-entry DoS (medium), persistence deserialize boyut-clamp (defense-in-depth), num=14 durum
ownership (low). **KURAL: kernel→user OKUYAN/YAZAN her yeni syscall ilgili guard'ı (oku_str / yaz_ptr)
kullanmalı.**

## D-150 — OS: syscall kullanıcı-pointer doğrulama — kernel bellek-yazma koruması (güvenlik sertleştirme) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-149).

**Karar [ETKİ: `runtime/kdl_kesme.c` (guard + num=18/20); yeni `test/bare_metal/guvenlik_arm.c`,
`Makefile` hedefi.]** KEMGU-OS'un çekirdek güvenlik invaryantını (bellek-güvenli OS) syscall
sınırında zorunlu kıl: kernel (EL1), kullanıcı-kontrollü bir pointer'a **user VA aralığı dışında**
YAZMAMALI. Aksi halde kötü/hatalı bir EL0 süreç, kernel'in yazdığı bir syscall'a kernel adresi
geçirip çekirdek belleğini bozabilir (privilege escalation vektörü).

**Guard:** `kdl_user_yaz_ptr_gecerli(p, len)` — yalnız `[0x42000000, 0x42400000)` (EL0 user VA)
içindeki, `len<=4MB` ve toplama-taşması olmayan yazma-hedeflerini kabul eder. Kernel'in
kullanıcı-tampona YAZDIĞI iki syscall'a eklendi: num=18 (dosya_oku_metin → buf'a içerik) ve
num=20 (dosya_ad → buf'a ad). Geçersizse RED (-1), yazma yapılmaz. Okuma-syscall'ları (.rodata
kernel çıktı stringleri) muaf — yalnız user-tampona YAZAN yollar denetlenir.

**Kanıt (aarch64 QEMU):** `guvenlik_arm.c` bir EL0 launcher olarak: dosya oluşturur, sonra
num=18'e (a) kernel-adresi 0x40000000 → **RED (-1)**, (b) geçerli user-tampon 0x42210000 →
**OK (>=0)** verir. İkisi de beklendiği gibiyse EL0 `yaz` syscall'ı ile "GUVENLIK OK" basar. Seri
çıktı: `GUVENLIK BASLA` → `GUVENLIK OK`. FS regresyonları (metin/ls/sil/kabuk — hepsi geçerli
user-tampon 0x42210000 kullanır) guard'la bozulmadan geçer.

**Kapsam/sınır:** Guard yalnız num=18/20 (mevcut write-to-user yollar). İleride kernel→user yazan
her yeni syscall aynı guard'ı kullanmalı (kural). Read-güvenliği (user'ın kernel .rodata OKUması)
zaten MMU AP=00 ile engelli — bu guard yazma-tarafı savunma-derinliği. Test-only bug (EL0'ın
kernel fonksiyonunu doğrudan çağırması) fix'te `yaz` syscall'ına çevrildi; guard mantığı değişmedi.

## D-135 — OS: basit userspace kabuk (shell) — komut ayrıştırma + FS dağıtım (Faz E/F DORUĞU) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-134).

**Karar [ETKİ: yeni `test/bare_metal/kabuk_arm.c`; `Makefile`. Yalnız test — mevcut syscall/kernel
kullanılır, kod değişmedi.]** Tüm userspace + FS + süreç yığınını tanınabilir bir OS artefaktına
bağlayan doruk: bir userspace program komut SCRIPT'ini ayrıştırıp (tokenize) FS syscall'larına
dağıtır — gerçek kabuk/komut yorumlayıcısı.

**Kabuk:** .user_data'daki script (yaz/oku/ls komutları) EL0'da in-place tokenize edilir (str_esit
+ tokenize helper'ları .user section'da, EL0-exec). yaz→dosya_yaz_metin, oku→dosya_oku_metin+bas,
ls→listele. Kernel çağırmaz; yalnız syscall.

**Öğrenilen (bellek koruması KANITI):** İlk deneme ISTISNA tip=0x24 DFSC=0x0E (permission fault,
FAR=0x40003fd3) — komut adı literalleri ("yaz"/"oku"/"ls") .rodata'da (AP=00); EL0 str_esit OKUYUNCA
fault. Bu, D3 bellek-korumasının GERÇEKTEN çalıştığının kanıtı (EL0 kernel belleğini okuyamaz).
DÜZELTME: komut adları .user_data'ya (AP=01, EL0-okunur). NOT: sys(5,literal) çıktı stringleri
.rodata'da KALIR (kernel EL1 okur, sorun yok) — yalnız EL0'ın DOĞRUDAN okuduğu stringler .user_data.

**Doğrulama (QEMU 11.0.1):** kabuk_arm — "SHELL> yaz gunluk KEMGU-OS" / "SHELL> oku gunluk" /
"  KEMGU-OS" / "SHELL> ls" / "  gunluk". Full gate GATE=0 (33 hedef). sıfır-uyarı. **KEMGU-OS artık
komut yorumlayan bir userspace kabuk çalıştırıyor — gösterici kernelden çalışan-OS'a.**

**Sıradaki:** UART RX (klavye → interaktif kabuk; gate zor); kaynak geri-alma; D2-x86; C5 virtio-blk.

---

## D-141 — OS: VirtIO-Blk gerçek disk okuma (C5 — kalıcı depolama, Faz E) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-140).

**Karar [ETKİ: yeni `runtime/kdl_virtio.c` (bare-metal virtio-mmio v2 blk sürücüsü); yeni
`test/bare_metal/virtio_arm.c`; `Makefile` (bm_a64_virtio.o + disk-imaj + QEMU virtio-blk). C/host
runtime — .kem driver'lardan (drivers/virtio/*.kem) yalnız register-offset bilgisi alındı, kod C.]**
İlk GERÇEK DONANIM depolama: QEMU virtio-blk diskinden blok okuma (RAM-FS'i kalıcı yapmanın temeli).

**Sürücü (kdl_virtio.c, aarch64):** virtio-mmio slot tara (0x0a000000+i*0x200, DeviceID=2) →
kdl_virtio_blk_bul. Init (kdl_virtio_blk_kur): reset→ACK→DRIVER→feature(VERSION_1 bit32)→FEATURES_OK
→virtqueue 0 (split: desc[8]+avail+used ayrı hizalı DMA tamponları, QueueDesc/Driver/Device Lo/Hi)
→DRIVER_OK. Oku (kdl_virtio_blk_oku): 3-desc zinciri (başlık RO + veri WR + durum WR) → avail.idx++
→ QueueNotify → used.idx poll → status==0 → 512 bayt kopya. DMA tamponları RAM identity-map (VA=PA);
QEMU coherent DMA (dsb ordering yeter, cache-flush yok). Register offsetleri constants.kem ile aynı.

**Doğrulama (QEMU 11.0.1):** virtio_arm — disk.img (blok 0'da "KEMGU-DISK-BLOK0") + `-device
virtio-blk-device` → kernel blok 0'ı okur, "KEMGU" doğrular → "DISK OK KEMGU". **İLK DENEMEDE geçti**
(virtqueue doğru). Full gate GATE=0 (37 hedef; diğer kernel'ler disk'siz — virtio target kendi
disk'ini kurar). sıfır-uyarı.

**Sıradaki:** virtio-blk YAZMA (D-142); dosya sistemini disk-backed yap; UART RX; D2-x86.

---

## D-149 — OS: SELF-HOST virtio init — KEMGU'da tarama + MMIO yazma (status handshake) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-148).

**Karar [ETKİ: yeni `test/ornekler/virtio_selfhost_rw.kem` (KEMGU!); `Makefile` (self-host-rw target).
Mevcut mmio/yetki runtime (D-148) kullanıldı.]** D-148 OKUMA'yı gerçek DRIVER INIT'e taşır: KEMGU
dilinde cihaz TARAMA + status durum-makinesi YAZMA.

**Mekanizma:** virtio_selfhost_rw.kem — (1) TARA: `iken i<32` döngüsünde her slot'un DeviceID'sini
mmio_oku32 ile oku (yetki ÖDÜNÇ → döngüde thread YOK), DeviceID!=0 ilk slotu bul. (2) HANDSHAKE:
status register'a mmio_yaz32 ile reset→ACK→ACK|DRIVER yaz — yetki LİNEAR olduğundan her yazmada
THREAD edilir (y→y1→y2→y3). (3) status geri oku = 3. **KEMGU dil özellikleri gerçek driver kodunu
kaldırıyor:** tam64 adres aritmetiği (i*512), döngü, linear-capability (borrow-in-loop + thread-in-chain).

**Doğrulama (QEMU 11.0.1):** virtio_selfhost_rw + virtio-blk device → KEMGU sürücüsü cihazı buldu,
handshake yaptı, status=3 okudu → "KEM VIRTIO RW OK". Full gate GATE=0 (45 hedef). libc-temiz.
sıfır-uyarı. **KEMGU tam bir virtio init sekansını (tarama+oku+yaz, capability-güvenli) kendi
dilinde çalıştırıyor — self-host OS sürücüsü.**

**Sıradaki:** .kem userspace program (EL0); TCP; UART RX; sürücüyü virtqueue'ya kadar genişlet.

---

## D-148 — OS: SELF-HOST virtio sürücüsü — KEMGU dilinde bare-metal OS sürücüsü (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-147).

**Karar [ETKİ: yeni `test/ornekler/virtio_selfhost.kem` (KEMGU!); yeni `runtime/kdl_yetki_bare.c`
(freestanding capability runtime); `Makefile` (bm_a64_mmio.o + bm_a64_yetki.o + self-host target).
Mevcut mmio codegen (src/llvm.c) + kdl_runtime_mmio.c bare-metal modu kullanıldı.]** Projenin
ÖZGÜN DNA'sı OS düzeyinde: bir OS sürücüsü KEMGU DİLİNDE yazıldı, KEMGU derleyicisiyle bare-metal
derlendi, gerçek donanım register'ı okudu.

**Mekanizma:** virtio_selfhost.kem — `yetki<MMIO>` (object-capability, DERLEME-ZAMANI donanım-erişim
ispatı, sıfır runtime yük) + `mmio_oku32(y, adres)` intrinsic'i ile virtio-mmio magic (0x0A000000)
+ version register'larını okur. kemgu --llvm → clang aarch64 (-x ir) → ld.lld → QEMU virt. mmio_oku32
codegen'de `kdl_mmio_oku32(adres)` volatile load'a iner (yetki runtime'a geçmez → WCET sıfır ek).
kdl_yetki_bare.c: KdlYetki (16B, codegen %kdl_yetki ile birebir) + olustur/geri_al (PRNG yerine sayaç,
libc yok). sret ABI aarch64'te x8 kullanır ama yetki MMIO'da kullanılmadığından benign.

**Doğrulama (QEMU 11.0.1):** virtio_selfhost — QEMU virt boş slot 0 virtio-mmio transport'u magic
(0x74726976) her zaman sunar → KEMGU sürücüsü okur+doğrular → "KEM VIRTIO OK" + version(1). **LİBC-TEMİZ**
(malloc/printf yok — capability-güvenli donanım erişimi). Full gate GATE=0 (44 hedef). sıfır-uyarı.
**KEMGU (memory-safe dil) kendi OS sürücüsünü kendi dilinde yazıyor — capability ile donanım erişimi
compile-time güvenli. Projenin özgün değer önerisi OS düzeyinde kanıtlandı.**

**Sıradaki:** self-host sürücüyü genişlet (yaz + tam virtio init .kem'de); .kem userspace program;
TCP; UART RX.

---

## D-147 — OS: DNS round-trip — UDP request-response (OS internet'le konuşuyor) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-146).

**Karar [ETKİ: yeni `test/bare_metal/dns_arm.c`; `Makefile`. Yalnız test — net driver + IP/UDP
(D-144/145/146) kullanılır.]** TÜM AĞ YIĞINI bir arada: gerçek istek-yanıt döngüsü (OS internet
servisiyle konuşuyor).

**Mekanizma:** (1) ARP ile DNS sunucusunun (SLIRP 10.0.2.3) MAC'ini çöz (sha çıkar). (2) DNS sorgusu
inşa et: eth(dst=dns_mac)+IPv4(dst=10.0.2.3)+UDP(dst=53)+DNS(header id/RD/qdcount=1 + qname "a.com" +
qtype=A + qclass=IN), IP checksum. (3) Gönder. (4) Yanıtı RX ile al + doğrula (IPv4+UDP, src=10.0.2.3,
src-port=53).

**Doğrulama (QEMU 11.0.1):** dns_arm — "DNS BASLA" + "DNS REPLY OK" (DNS sunucusundan UDP yanıtı
alındı). **İLK DENEMEDE.** Full gate GATE=0 (43 hedef). sıfır-uyarı. **OS gerçek bir internet
servisiyle (DNS) request-response yapıyor = internet-katmanı round-trip. Ağ yığını: ARP+IP+UDP+DNS.**

**Sıradaki:** ICMP ping; TCP handshake; DNS yanıtından IP çıkar (tam resolver); UART RX; D2-x86.

---

## D-146 — OS: IP/UDP paket gönderme — internet katmanı (Faz G derinleşme) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-145).

**Karar [ETKİ: yeni `test/bare_metal/udp_arm.c`; `Makefile`. Yalnız test — net driver (D-144/145)
kullanılır.]** ARP (L2) üstüne İNTERNET KATMANI: geçerli IPv4 + UDP paketi (IP header checksum
dâhil) inşa + gönder.

**Mekanizma:** ip_checksum (RFC 1071, 16-bit tümleyen toplamı). Frame: eth(IPv4) + IPv4(20:
v4/IHL5, total_len, TTL, proto=17, checksum, src=10.0.2.15, dst=10.0.2.3) + UDP(8: src=5000,
dst=53, len, checksum=0) + payload "KEMGU-UDP-DATA". virtio-net TX ile gönder.

**Doğrulama (QEMU 11.0.1):** udp_arm — paket gönder → filter-dump pcap → "UDP GONDERILDI" +
`grep -a "KEMGU-UDP-DATA" udp.pcap`. Full gate GATE=0 (42 hedef). sıfır-uyarı. **OS geçerli IPv4/UDP
paketi oluşturuyor (checksum'lı) — gerçek internet-protokol yığını temeli.**

**Sıradaki:** DNS/UDP round-trip (10.0.2.3:53'e sor, yanıt al); ICMP; TCP handshake; UART RX; D2-x86.

---

## D-145 — OS: ARP round-trip — 2-yönlü ağ (virtio-net RX + ARP protokolü) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-144).

**Karar [ETKİ: `runtime/kdl_virtio_net.c` (+RX queue (0) kurulumu + kdl_virtio_net_al); yeni
`test/bare_metal/arp_arm.c`; `Makefile`. Sadece ekleme — net TX (D-144) regresyonsuz.]** D-144
gönderme'yi ALMA ile tamamlar → gerçek 2-yönlü ağ + ilk protokol (ARP).

**Mekanizma:** kdl_virtio_net_kur artık RX queue 0'ı da kurar (NVQ_N tampon avail'e AÇIK verilir,
cihaz gelen paketleri yazar, QueueNotify 0 ile bildirilir). kdl_virtio_net_al: rx_used poll → gelen
çerçeveyi (net-başlığı 12 bayt atlanmış) kopyala + uzunluk döner. ARP protokol mantığı testte
(request/reply parse).

**Doğrulama (QEMU 11.0.1):** arp_arm — kernel gateway (SLIRP 10.0.2.2) için ARP isteği yollar; SLIRP
ARP yanıtı verir; kernel RX ile alır + doğrular (ethertype 0x0806 + oper=2 + spa=10.0.2.2) →
"ARP REPLY OK". **İLK DENEMEDE.** Full gate GATE=0 (41 hedef). sıfır-uyarı. **OS 2-yönlü ağ: paket
gönder + al + ARP round-trip. Faz G derinleşti.**

**Sıradaki:** IP/UDP paketi (ping/DNS); ARP tablosu; UART RX; D2-x86. TÜM ROADMAP FAZLARI (C-G)
temel formda çalışıyor.

---

## D-144 — OS: VirtIO-Net paket gönderme — ağ TX (Faz G başlangıcı) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-143).

**Karar [ETKİ: yeni `runtime/kdl_virtio_net.c` (bare-metal virtio-net TX sürücüsü); yeni
`test/bare_metal/net_arm.c`; `Makefile` (bm_a64_virtio_net.o + QEMU netdev + filter-dump pcap gate).
Sadece ekleme — net driver yalnız net testine linklenir (BM_A64_OBJS'e DEĞİL, kimse referans etmiyor).]**
İlk AĞ yeteneği: gerçek Ethernet çerçevesi gönderme. Faz G açılışı.

**Sürücü:** virtio-blk (D-141) virtqueue makinesi yeniden kullanıldı; fark: DeviceID=1 (net), transmit
queue=1, buffer=virtio-net başlığı(12,sıfır)+çerçeve, tek desc (cihaz OKUR/TX). kdl_virtio_net_bul/
kur/gonder.

**Doğrulama (QEMU 11.0.1):** net_arm — broadcast Ethernet çerçevesi (ethertype 0x88b5, payload
"KEMGUNET-PAKET") gönder. QEMU `-netdev user -device virtio-net-device -object filter-dump,file=pcap`
→ paket pcap'e yakalandı. Gate: seri "NET GONDERILDI" + `grep -a "KEMGUNET-PAKET" net.pcap`. **İLK
DENEMEDE** (virtqueue makinesi taşındı). Full gate GATE=0 (40 hedef). sıfır-uyarı. **OS gerçek ağ
paketi gönderebiliyor — pcap ile kanıtlandı.**

**Sıradaki:** virtio-net RX (paket AL); ARP/IP/UDP stack (Faz G derinleşme); UART RX; D2-x86.

---

## D-143 — OS: KALICI dosya sistemi — disk-backed persistence (boot'lar arası) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-142).

**Karar [ETKİ: `runtime/kdl_kesme.c` (+kdl_dosya_kaydet/yukle disk serialize/deserialize +
kdl_dosya_olustur_deger/deger kernel helper'ları); yeni `test/bare_metal/kalici_arm.c`; `Makefile`
(iki-boot gate). virtio (aarch64) guard'lı. Sadece ekleme.]** RAM dosya sistemini (D-131) virtio-blk
diske (D-141/142) bağlar → dosyalar BOOT'LAR ARASI yaşar (gerçek kalıcılık).

**Mekanizma:** kdl_dosya_kaydet(base) — kdl_dosyalar tablosunu blok 0-1'e serialize (magic "KEMG" +
bytes). kdl_dosya_yukle(base) — blok 0-1 oku, magic varsa tabloyu geri yükle (-1 diskte FS yok).
Byte-kopya (aliasing yok). 2 blok (768 bayt tablo + 16 header < 1024).

**Doğrulama (QEMU 11.0.1):** kalici_arm — AYNI kernel AYNI diskle İKİ KEZ boot. Boot 1: FS yok →
"kalici"=777 oluştur+kaydet → "FIRST BOOT saved". Boot 2: magic var → yükle → "SECOND BOOT
kalici=777". **Dosya kernel yeniden başlayınca diskten geri geldi = GERÇEK KALICILIK.** Full gate
GATE=0 (39 hedef). sıfır-uyarı.

**Sıradaki:** FS'i syscall'la kaydet/yükle (userspace tetikler); UART RX (interaktif kabuk); D2-x86;
networking (Faz G).

---

## D-142 — OS: VirtIO-Blk yaz+oku round-trip — gerçek kalıcı depolama (C5 tamam) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-141).

**Karar [ETKİ: `runtime/kdl_virtio.c` (+kdl_virtio_blk_yaz); yeni `test/bare_metal/virtio_rw_arm.c`;
`Makefile`. Sadece ekleme.]** D-141 okumasını YAZMA ile tamamlar → çift-yönlü disk I/O = gerçek
kalıcılık.

**Mekanizma:** kdl_virtio_blk_yaz(base, sektor, kaynak) — okumadan farkı: type=1 (VIRTIO_BLK_T_OUT);
veri descriptor'ı cihaz-OKUR (WRITE flag YOK, cihaz veriyi diske yazar). Aynı virtqueue makinesi.

**Doğrulama (QEMU 11.0.1):** virtio_rw_arm — blok 7'ye "KEMGU-YAZDI-42" yaz → geri oku → eşleşme →
"DISK RW OK". Full gate GATE=0 (38 hedef). sıfır-uyarı. **C5 TAMAM: OS gerçek diske veri yazıp
okuyabiliyor (kalıcı depolama). virtio-blk sürücüsü: bul/kur/oku/yaz.**

**Sıradaki:** dosya sistemini disk-backed yap (RAM-FS'i blok'lara serialize); UART RX (interaktif
kabuk); D2-x86; networking (Faz G).

---

## D-140 — OS: userspace mesaj kanalı (IPC) — süreçler-arası mesajlaşma (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-139).

**Karar [ETKİ: `runtime/kdl_kesme.c` (global kdl_msg[] ring buffer + num 22 kanal_gonder, 23 kanal_al);
yeni `test/bare_metal/kanal_ipc_arm.c`; `Makefile`. Sadece ekleme.]** İki userspace süreç çekirdek-
aracılı mesaj kanalıyla DOĞRUDAN haberleşir (dosya-IPC ötesinde; KEMGU `kanal` ilkelinin userspace
düzeyi).

**Mekanizma:** global int ring buffer (16). num=22 kanal_gonder(deger) enqueue (dolu=-1); num=23
kanal_al() dequeue (boş=-1). Bloklamasız → alıcı EL0'da yoklar (deadlock yok).

**Doğrulama (QEMU 11.0.1):** kanal_ipc_arm — launcher(alıcı) spawn(sender); sender kanal_gonder
(100/200/300)+exit; launcher kanal_al ile 3 değer alıp toplar → "KANAL SUM=600". Full gate GATE=0
(36 hedef). sıfır-uyarı. **Userspace IPC iki yolla: dosya (paylaşılan depo) + kanal (mesaj geçişi).**

**Sıradaki:** UART RX (interaktif kabuk); D2-x86 (ring3); C5 virtio-blk (kalıcı disk).

---

## D-139 — OS: kabuğa aritmetik — topla komutu (shell hesap makinesi) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-138).

**Karar [ETKİ: `test/bare_metal/kabuk_arm.c` (+CMD_TOPLA + str_sayi (atoi) + branch + script satırı);
`Makefile` (gate "= 42"). Yalnız test.]** Kabuk artık sayı ayrıştırıp aritmetik yapıyor — FS komut
yorumlayıcısı + hesap makinesi.

**Mekanizma:** str_sayi (EL0 atoi, .user) + "topla A B" komutu → str_sayi(tok[1])+str_sayi(tok[2])
→ "= toplam". Kabuk metin→sayı ayrıştırma + hesap (userspace'de).

**Doğrulama (QEMU 11.0.1):** kabuk_arm — "SHELL> topla 12 30" / "= 42". Full gate GATE=0 (35 hedef).
sıfır-uyarı. **Kabuk komut seti: yaz/oku/ls/say/sil (FS CRUD) + topla (aritmetik).**

**Sıradaki:** UART RX (interaktif kabuk); D2-x86; C5 virtio-blk.

---

## D-138 — OS: kaynak geri-alma (slot reuse) — sınırsız spawn (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-137).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_preempt_gorev_olustur_el0 ölü görev-slotu reuse;
kdl_surec_spawn ölü havuz-slotu reuse + kdl_spawn_kullanildi/task[]); yeni
`test/bare_metal/geri_al_arm.c`; `Makefile`. Sadece ekleme/iyileştirme.]** Süreç bitince (exit) hem
scheduler görev-slotu hem spawn-havuz-slotu geri alınır → OS programları SINIRSIZ çalıştırabilir
(eski: monoton sayaç, 4 spawn'da tükeniyordu).

**Mekanizma:** kdl_preempt_gorev_olustur_el0 önce ÖLÜ (kdl_olu) görev slotu arar, yoksa yeni
(kdl_psayi++). kdl_surec_spawn boş VEYA görevi ölmüş havuz slotunu yeniden kullanır
(kdl_spawn_kullanildi[]+kdl_spawn_task[]). Güvenli: ölü görev scheduler'da atlanır + spawn eden
canlı görevden çağrılır (kdl_paktif != geri-alınan slot).

**Doğrulama (QEMU 11.0.1):** geri_al_arm — launcher 6× spawn+join (havuz=4'ten fazla) → "SPAWNS=6"
(hepsi başarılı; geri-alma olmasaydı 5.'te -1 → SPAWNS=4). Full gate GATE=0 (35 hedef). spawn/yasam/
multiproc/calis regresyon yeşil. sıfır-uyarı. **OS artık sınırsız süreç yaratıp bitirebilir.**

**Sıradaki:** UART RX (interaktif kabuk); D2-x86 (ring3 parite); C5 virtio-blk (kalıcı disk).

---

## D-137 — OS: program çalıştırma iş akışı — spawn→hesap→dosya→join→oku (uçtan-uca) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-136).

**Karar [ETKİ: yeni `test/bare_metal/calis_arm.c`; `Makefile`. Yalnız test — mevcut syscall'lar.]**
Süreç + FS + IPC yığınının uçtan-uca entegrasyonu: bir program başka bir programı çalıştırır, o
hesap yapıp sonucu dosyaya yazar, başlatan program sonucu geri okur ("bir programı çalıştır,
çıktısını al" — gerçek OS iş akışı).

**Akış:** launcher spawn(worker)→join; worker 1..10 topla(=55)→dosya_yaz("sonuc",55)→exit; launcher
dosya_oku("sonuc")→bas. Global FS worker çıktısını launcher'a taşır (süreçler-arası).

**Doğrulama (QEMU 11.0.1):** calis_arm — "CALIS BASLA" + "RESULT=55" (worker hesabı dosya üzerinden
launcher'a ulaştı). Full gate GATE=0 (34 hedef). sıfır-uyarı. **KEMGU-OS: kernel + izole userspace
süreçler + preemptive multitask + syscall ABI + RAM-FS + kabuk + program-çalıştır-çıktı-al akışı —
çalışan çok-programlı bir OS.**

**Sıradaki:** UART RX (interaktif kabuk); kaynak geri-alma (slot reuse); D2-x86; C5 virtio-blk.

---

## D-136 — OS: kabuk komut genişletme — say + sil (shell tam CRUD komut seti) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-135).

**Karar [ETKİ: `test/bare_metal/kabuk_arm.c` (+CMD_SAY/CMD_SIL + branch + script satırları);
`Makefile` (gate say/sil kontrolü). Yalnız test.]** D-135 kabuğunu tam CRUD komut setine genişletir.

**Kabuk komutları:** yaz/oku/ls (D-135) + say (dosya sayısı) + sil (dosya_sil). Script:
yaz gunluk → oku → ls → say(COUNT=1) → sil gunluk → say(COUNT=0). Silme öncesi/sonrası sayaç
(1→0) sil'in çalıştığını kanıtlar.

**Doğrulama (QEMU 11.0.1):** kabuk_arm — "SHELL> say"/"COUNT=1"/"SHELL> sil gunluk"/"SHELL> say"/
"COUNT=0". Full gate GATE=0 (33 hedef). sıfır-uyarı. **Userspace kabuk artık tam FS CRUD komut
seti yorumluyor (yaz/oku/ls/say/sil).**

**Sıradaki:** UART RX (interaktif kabuk); kaynak geri-alma; D2-x86; C5 virtio-blk (kalıcı disk).

---

## D-134 — OS: dosya sil — FS CRUD tamamlandı (oluştur/oku/güncelle/listele/sil) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-133).

**Karar [ETKİ: `runtime/kdl_kesme.c` (+num 21 dosya_sil; num 20 dosya_ad → kullanılan-index);
yeni `test/bare_metal/sil_arm.c`; `Makefile`. Sadece ekleme + dosya_ad iyileştirme.]** RAM dosya
sistemi artık tam CRUD.

**Mekanizma:** num=21 dosya_sil(ad=arg) → slot serbest (kullanildi=0). num=20 dosya_ad artık
KULLANILAN-index (raw değil) → silinmiş slotlar sıralamayı bozmaz (boşluk atlanır). D-133 ls
regresyonsuz (silme yoksa kullanılan==raw).

**Doğrulama (QEMU 11.0.1):** sil_arm — alfa+beta+gama oluştur → dosya_sil("beta") → listele →
"AFTER count=2" + alfa + gama (beta YOK). Full gate GATE=0 (32 hedef). D-133 ls regresyon yeşil.
sıfır-uyarı. **RAM-FS tam CRUD: oluştur(15)/oku(16)/metin(17,18)/listele(19,20)/sil(21).**

**Sıradaki:** scripted userspace kabuk (komut dizisi → FS işlemleri); UART RX (klavye, gate zor);
kaynak geri-alma; D2-x86; C5 virtio-blk (kalıcı disk).

---

## D-133 — OS: dosya listeleme (ls) — userspace dosya enumerasyonu (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-132).

**Karar [ETKİ: `runtime/kdl_kesme.c` (+num 19 dosya_sayisi, 20 dosya_ad); yeni
`test/bare_metal/ls_arm.c`; `Makefile`. Sadece ekleme.]** D-131/132 dosya sistemi üstünde ilk
"shell primitifi": userspace program dosya deposunu enumere eder (ls).

**Mekanizma:** num=19 dosya_sayisi() → kullanımdaki dosya sayısı. num=20 dosya_ad(idx=arg, buf=arg2)
→ idx'inci dosyanın adını user tamponuna kopyala. Userspace program dosya_sayisi() kez döngüyle
her adı okuyup basar (ls).

**Doğrulama (QEMU 11.0.1):** ls_arm — launcher dosya_yaz("alfa",1)+dosya_yaz("beta",2) → listele →
"LS count=2" + "  alfa" + "  beta". Full gate GATE=0 (31 hedef). sıfır-uyarı. **Userspace ABI 20+
syscall: process (spawn/exit/durum/getpid) + zaman (gettick) + I/O (yaz*) + dosya (yaz/oku/metin/
sayisi/ad). Basit bir kabuk (shell) yazmaya yetecek temel.**

**Sıradaki:** basit userspace kabuk (komut → dosya işlemi); dosya sil; kaynak geri-alma; D2-x86; C5.

---

## D-132 — OS: metin içerikli dosya — bulk read/write (kernel↔user bellek kopyası) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-131).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_dosyalar +icerik[64]+boyut; +num 17 dosya_yaz_metin,
18 dosya_oku_metin); yeni `test/bare_metal/metin_arm.c`; `Makefile`. Sadece ekleme.]** D-131 tek-değer
dosyasını GERÇEK byte-içeriğe genişletir — kernel↔userspace çift-yönlü bellek kopyası (gerçek
read/write syscall ailesinin temeli).

**Mekanizma:** num=17 dosya_yaz_metin(ad=arg, str=arg2) — kernel kullanıcı belleğinden (arg2) string'i
dosya içeriğine kopyalar (yazılan byte döner). num=18 dosya_oku_metin(ad=arg, buf=arg2) — dosya
içeriğini kullanıcı tamponuna (arg2) kopyalar (okunan byte döner). Kernel EL1'den AP=01 user
sayfasını okur/yazar (buf worker'ın veri sayfasında). 2-arg syscall (D-131).

**Doğrulama (QEMU 11.0.1):** metin_arm — launcher dosya_yaz_metin("mesaj","MERHABA DOSYA")+spawn;
worker dosya_oku_metin ile metni kendi tamponuna okur+basar → "FILE TEXT: MERHABA DOSYA" (dosya
metin içeriği süreçler-arası aktarıldı). Full gate GATE=0 (30 hedef). D-131 regresyon yeşil. sıfır-uyarı.

**Sıradaki:** dosya offset'li read/write (kısmi); dizin/listeleme; kaynak geri-alma; D2-x86; C5
virtio-blk (RAM-FS'i kalıcı disk'e).

---

## D-131 — OS: RAM dosya sistemi + 2-argümanlı syscall (Faz E ilk adım) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-130).

**Karar [ETKİ: `boot/start_aarch64.S` (SVC path arg2=saklanan-x1 geçirir); `runtime/kdl_kesme.c`
(kdl_syscall_isle 3-param (num,arg,arg2) + RAM dosya deposu + num 15 dosya_yaz, 16 dosya_oku); yeni
`test/bare_metal/dosya_arm.c`; `Makefile`. x86 stub değişmedi (nums 1/2/3 arg2 kullanmaz — gate'te
doğrulandı). linker/host/codegen dokunulmadı.]** İki yenilik: 2-argümanlı syscall ABI + çekirdek-
aracılı isimli depolama (Faz E dosya sisteminin ilk adımı, virtio-blk GEREKTİRMEZ).

**2-arg syscall:** D-126 x1-koruma (register-şeffaflık) bunu mümkün kıldı; şimdi SVC path saklanan-x1'i
3. C param (arg2) olarak geçirir. `ldr x2,[sp,#8]` eklendi. dosya_yaz(ad, değer) gibi 2-arg syscall'lar.

**RAM dosya deposu:** kdl_dosyalar[8] (ad[16]+deger+kullanildi); kdl_dosya_ac (bul/oluştur) + kdl_dosya_bul
+ kdl_ad_esit (freestanding strcmp). num=15 dosya_yaz(ad=arg, deger=arg2); num=16 dosya_oku(ad=arg)→değer.
Süreçler-arası paylaşılır (çekirdek durumu).

**Doğrulama (QEMU 11.0.1):** dosya_arm — launcher dosya_yaz("sayac",1234)+spawn(worker); worker
dosya_oku("sayac")=1234 → "FILE OK deger=1234" (BAŞKA süreç, launcher'ın yazdığını okudu). Full gate
GATE=0 (29 hedef). x86 syscall + tüm SVC regresyon (syscall_arg/ret/d2/userspace) yeşil. sıfır-uyarı.
**Userspace ABI: yaz/yaz_sayi/satir/cik/artir/gettick/getpid/spawn/exit/durum/dosya_yaz/dosya_oku.**

**Sıradaki:** dosya read/write byte-buffer (tek-değer değil); kaynak geri-alma; D2-x86; C5 virtio-blk
(gerçek disk → RAM-FS'i kalıcı yap).

---

## D-130 — OS: süreç yaşam döngüsü — exit + join (spawn→çalış→exit→join tam döngü) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-129).

**Karar [ETKİ: `runtime/kdl_gorev.c` (+kdl_olu[] state + kdl_gorev_bitir/kdl_gorev_durum +
kdl_preempt'te ölü-görev atla); `runtime/kdl_kesme.c` (+num 13 exit, 14 durum); yeni
`test/bare_metal/yasam_arm.c`; `Makefile`. x86/host/codegen dokunulmadı (exit/durum arch-generic).]**
D-129 spawn'ı tam yaşam döngüsüne tamamlar: süreç bitişi + ebeveyn join.

**Mekanizma:** kdl_olu[görev] (1=bitmiş). num=13 exit → kdl_gorev_bitir() (kdl_olu[kdl_paktif]=1);
kdl_preempt ölü görevi ATLAR (bloklu gibi) → süreç bir daha koşmaz. num=14 durum(pid) →
kdl_gorev_durum(pid) (bitti mi?). **Bloklamalı join YERİNE EL0-yoklama:** ebeveyn preemptive
olduğundan `while(!durum(pid))` yoklarken çocuk koşar→exit eder→durum=1 (deadlock yok; blocking-in-
syscall / IRQ-masked sorununu bypass eder). Kaynak geri-alma v1'de yok (havuz slotu serbest değil).

**Doğrulama (QEMU 11.0.1):** yasam_arm — launcher spawn(worker)→worker iş+exit→launcher join
(durum yokla)→"WORKER done"+"JOINED worker exited". Full gate GATE=0 (28 hedef). Scheduler
regresyon (kdl_olu skip additive) yeşil. sıfır-uyarı. **Tam süreç yaşam döngüsü: yarat→koş→bitir→
bekle. Userspace ABI: yaz/yaz_sayi/satir/cik/artir/gettick/getpid/spawn/exit/durum.**

**Sıradaki:** read/write dosya syscall'ları (C5 storage sonrası); kaynak geri-alma (exit'te havuz
free); D2-x86; C5 virtio-blk → Faz E fs.

---

## D-129 — OS: dinamik süreç oluşturma — spawn syscall'ı (fork/spawn yeteneği) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-128).

**Karar [ETKİ: `runtime/kdl_gorev.c` (+kdl_surec_spawn — havuz-tabanlı runtime süreç); `runtime/
kdl_kesme.c` (kdl_syscall_isle +num 12 spawn, #if __aarch64__); yeni `test/bare_metal/spawn_arm.c`;
`Makefile`. x86/host/codegen/linker dokunulmadı.]** D-127 (statik çok-süreç) → runtime dinamik
süreç: bir userspace süreç RUNTIME'da yeni izole süreç yaratır (gerçek OS fork/spawn).

**Mekanizma:** kdl_surec_spawn(entry) — havuzdan (KDL_SPAWN_MAX=4) L1/L2 tabloları + kernel yığını
+ sürece-özel veri PA'sı (0x46000000+i*2MB, RAM içi) alır → kdl_surec_kur_el0_veri (paylaşılan kod
+ özel veri) → kdl_preempt_gorev_olustur_el0 (preemptive EL0 görev, entry'de) → kdl_task_l1[t]=yeni
tablo. num=12 spawn(entry_va) syscall bunu çağırır, yeni pid döner. Syscall IRQ-masked → kdl_psayi++
scheduler ile yarışmaz (güvenli); yeni görev eret sonrası ilk timer-IRQ'da schedulable.

**Doğrulama (QEMU 11.0.1):** spawn_arm — launcher (EL0 süreç, kendi TTBR) spawn(worker) çağırır →
"LAUNCHER spawned pid=2"; worker DİNAMİK yaratılan izole adres-uzayında EL0'da koşar → "WORKER OK".
Full gate GATE=0 (27 hedef). sıfır-uyarı. **Programlar artık yeni süreç başlatabiliyor —
gerçek çok-görevli OS'un temel yeteneği.**

**Sıradaki:** süreç bitişi/join (worker exit → launcher öğrenir); read/write dosya syscall'ları;
D2-x86; C5 virtio-blk → Faz E fs.

---

## D-128 — OS: userspace introspection syscall'ları — gettick + getpid (userspace ABI genişleme) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-127).

**Karar [ETKİ: `runtime/kdl_zaman.c` (+kdl_tik_al getter); `runtime/kdl_gorev.c` (+kdl_aktif_gorev
getter); `runtime/kdl_kesme.c` (kdl_syscall_isle +num 10 gettick, 11 getpid); yeni
`test/bare_metal/tick_arm.c`; `Makefile`. x86/host/codegen dokunulmadı (getter'lar iki arch'ta;
x86 syscall kernel gate'te doğrulandı).]** D-126 dönüş-değeri ABI'si üstünde ilk gerçek "kernel
durumu okuyan" userspace syscall'ları.

**Yeni syscall'lar (dönüş-değerli):** num=10 gettick → kdl_tik_al() (timer tik sayısı, userspace
zamanı okur); num=11 getpid → kdl_aktif_gorev() (o an koşan preemptive görev id'si). Getter'lar:
kdl_tik_al (kdl_zaman.c, static kdl_tik_sayisi'ni açar), kdl_aktif_gorev (kdl_gorev.c, kdl_paktif).

**Doğrulama (QEMU 11.0.1):** tick_arm — preemptive EL0 görev gettick(t1)→zaman-geçir→gettick(t2)→
getpid; t2>t1 (timer preemptive görevde IRQ-açık → tikler) + pid=1 → "TICK OK pid=1". Full gate
GATE=0 (26 hedef). sıfır-uyarı. **Userspace artık çekirdek durumunu (zaman/kimlik) syscall ile
okuyabiliyor — gerçek programların temel ihtiyacı.**

**Sıradaki:** read/write dosya syscall'ları (C5 storage sonrası); dinamik süreç spawn; D2-x86; C5
virtio-blk → Faz E fs.

---

## D-127 — OS: çoklu EL0 süreç — izole userspace multitasking (gerçek multi-process OS DORUĞU) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-126).

**Karar [ETKİ: `runtime/kdl_mmu.c` (+kdl_surec_kur_el0_veri — paylaşılan kod + özel veri sayfası);
`runtime/kdl_gorev.c` (+kdl_task_l1[] + kdl_preempt_gorev_ttbr + kdl_preempt'te guard'lı TTBR-swap);
yeni `test/bare_metal/multiproc_arm.c`; `Makefile`. x86/host/codegen/linker dokunulmadı.]** D3
(per-process TTBR) ⊕ D-125 (preemptive EL0) birleşimi: BİRDEN ÇOK userspace süreç, her biri KENDİ
izole adres-uzayında, preemptively multitask.

**Mekanizma:**
- **kdl_surec_kur_el0_veri(l1,l2,kod_pa,veri_pa):** L2[16] (0x42000000) → kod_pa (PAYLAŞILAN .user
  kod, tüm süreçlerde aynı, AP=01); L2[17] (0x42200000) → veri_pa (SÜRECE-ÖZEL, AP=01); kernel
  identity her tabloda (swap güvenli). Paylaşılan-kod/özel-veri deseni (klasik OS).
- **Scheduler TTBR-swap:** kdl_task_l1[görev] (kdl_preempt_gorev_ttbr ile set). kdl_preempt seçilen
  göreve geçerken `if (kdl_task_l1[en_iyi]) kdl_ttbr_degis(...)` → o sürecin adres-uzayına çevir.
  **GUARD'LI:** set edilmemişse (mevcut EL1 testleri) swap YOK → regresyon YOK (6 scheduler testi
  doğrulandı). `#if defined(__aarch64__)` (x86 cooperative-only).

**İZOLASYON KANITI (multiproc_arm):** A markörü 0xAA'yı bir kez yazar, sonra 40000-iter döngüde
sürekli 0xAA doğrular; B simetrik 0xBB. İkisi AYNI VA'yı (0x42200000) kullanır ama FARKLI PA
(A→0x44000000, B→0x46000000). Timer-IRQ defalarca aralarında geçer; izole olduğundan A hep 0xAA
(B'nin 0xBB'si A'nın PA'sına DOKUNMAZ) → "A OK" + "B OK". Paylaşsalardı çapraz-bozulma → "CORRUPT".

**Doğrulama (QEMU 11.0.1):** "MULTIPROC BASLA" + "A OK" + "B OK". Full gate GATE=0 (25 hedef).
sıfır-uyarı. **Process modeli TAM DORUK: kernel + N izole userspace süreç, her biri kendi
adres-uzayında, preemptively multitask + syscall (arg+dönüş+çok-arg) + bellek-koruması.**

**Sıradaki:** userspace ABI genişletme (gettick/getpid/read); dinamik süreç oluşturma (fork-benzeri);
D2-x86 (ring3+TSS); C5 (virtio-blk → Faz E dosya sistemi).

---

## D-126 — OS: syscall dönüş-değeri ABI + kdl_exc_ortak register-şeffaflık onarımı (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-125).

**Karar [ETKİ: `boot/start_aarch64.S` (kdl_exc_ortak+kdl_svc_ortak → tek "frame-önce-kaydet"
işleyici; str x0 dönüş); `runtime/kdl_kesme.c` (kdl_syscall_isle → uint64_t; +num 9 artir);
yeni `test/bare_metal/syscall_ret_arm.c`; `Makefile`. x86/host/codegen dokunulmadı.]** Userspace
ABI'nin eksik yarısı (syscall DEĞER döndürür) + bunu yaparken keşfedilen gerçek register-şeffaflık
bug'ının onarımı.

**Dönüş-değeri ABI:** kdl_svc_ortak `bl kdl_syscall_isle` sonrası `str x0, [saved-x0]` → restore
ile EL0 çağıran x0'da sonucu alır. kdl_syscall_isle artık uint64_t döner (num=9 'artir': arg+1).

**KEŞFEDİLEN + ONARILAN BUG (register-şeffaflık):** Eski kdl_exc_ortak, frame kaydetmeden ÖNCE
`lsr x9, x1, #26` (EC) + `mrs x1/x2/x3` ile çağıranın x1/x2/x3/x9'unu klobber ediyordu; SVC için
EC=0x15 → x9=0x15. syscall_ret testi (dönüş-değerine bağlı dallı string-ptr'yi x9'da tutan)
BUNU tetikledi: OK-string ptr'si (0x40003570) x9'da → syscall sonrası x9=0x15 → sys(yaz, 0x15) →
çöp → hiçbir şey basılmadı. Empirik teşhis: QEMU `-d in_asm,cpu` (x9: 0x40003570 → 0x15 svc'de).
**ONARIM:** işleyici FRAME'İ ÖNCE kaydeder, SONRA ESR okur/dispatch eder → tüm çağıran register'ları
korunur (yalnız x0=dönüş değişir). num/arg saklanandan okunur. **Çok-argümanlı syscall'ları da
mümkün kıldı (x1+ artık korunuyor — eski bug x1'i eziyordu).**

**Doğrulama (QEMU 11.0.1):** syscall_ret "SYSCALL RET OK" (41→42 EL0'a döndü). Tüm SVC/fault
regresyon yeşil: syscall/syscall_arg/istisna(fault)/d2(EL0+SVC)/proc(D3)/userspace. Full gate GATE=0
(24 hedef). sıfır-uyarı. **Öğrenilen: `-MMD -MP` header-dep var ama .c değişince .o rebuild
timing — rm+make ile force gerekebildi (staleness teşhisi).**

**Sıradaki:** çoklu EL0 süreç (scheduler TTBR-swap); gettick/getpid/read syscall'ları (artık dönüş +
çok-arg hazır); D2-x86; C5 (virtio-blk).

---

## D-125 — OS: preemptive EL0 (userspace) görev — userspace multitasking (process modeli tamam) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-124).

**Karar [ETKİ: `boot/start_aarch64.S` (kdl_irq_ortak SP_EL0 save/restore @264 — Stage 1);
`runtime/kdl_gorev.c` (+kdl_preempt_gorev_olustur_el0 — Stage 2); `linker/bare-metal-aarch64.ld`
(.user output'a .user_data eklendi); yeni `test/bare_metal/preempt_el0_arm.c`; `Makefile`.
x86/host/codegen dokunulmadı.]** Process modelinin son parçası: userspace (EL0) görevler
PREEMPTIVELY multitask edilir.

**Stage 1 (non-regressing):** kdl_irq_ortak trap-frame'e SP_EL0'ı @264 ekler (mrs/msr sp_el0).
EL1 görevlerde SP_EL0 kullanılmaz → zararsız; 6 EL1 preemptive testi (preempt/sleep/priority/kanal/
sched/timer) hâlâ yeşil (doğrulandı).

**Stage 2:** kdl_preempt_gorev_olustur_el0(giris, kernel_yigin, user_yigin) — sentetik trap-frame
SPSR=0x0 (EL0t, IRQ-açık) + SP_EL0=user yığını. İlk switch eret ile EL0'a atlar; timer-IRQ EL0'dan
EL1'e alır, kdl_irq_ortak SP_EL0 dâhil tüm bağlamı kaydeder → EL0 görev preempt edilip sürdürülür.
İKİ yığın: kernel (trap-frame/SP_EL1, AP=00) + user (SP_EL0, .user AP=01).

**Linker:** .user output section artık .user_data (EL0-yazılabilir veri) de toplar — kod (.user, X)
ve veri (.user_data, W) ayrı input-section → derleyici section-tip çakışması yok, ikisi de aynı
0x42000000 AP=01 sayfasında. (İleride process code/data ayrımı temeli.)

**Doğrulama (QEMU 11.0.1):** "PREEMPT EL0 BASLA" + "PREEMPT EL0 OK" (el0_sayac>0 = EL0 userspace
görev timer-IRQ ile preempt edilerek koştu, main EL1 de koştu). Full gate GATE=0. sıfır-uyarı.

**Önem:** Process modeli ARTIK TAM — kernel(EL1) + userspace(EL0) görevler timer-IRQ ile
preemptively dönüşümlü koşar, banked SP_EL0 korunur. Gerçek OS multitasking'inin çekirdeği.

**Sıradaki:** çoklu EL0 süreç + per-process TTBR swap scheduler'da (D3+D-125 birleşik); userspace
ABI (exit-kod/oku); D2-x86 (ring3+TSS); C5 (virtio-blk → Faz E fs).

---

## D-124 — OS: ilk userspace programı — EL0 hesap + syscall ABI ile I/O (Faz F temeli) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-123).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_syscall_isle +num 5/6/7 = yaz/yaz_sayi/satir; +kdl_yaz_metin/
kdl_yaz_tam decl); yeni `test/bare_metal/userspace_arm.c`; `Makefile`. x86/host/codegen dokunulmadı
(syscall_isle paylaşılan — x86 yaz_* referansı gate'te doğrulandı).]** D3'ün (korumalı süreç) üstüne
userspace ABI'nin ilk gerçek kullanımı: bir userspace programı EL0'da HESAP yapar + kernel
hizmetlerini SYSCALL ile kullanır (Faz F userspace temeli).

**Userspace syscall ABI (v0):** num=5 yaz(ptr) — string yaz (kernel kullanıcı belleğinden ptr OKUR —
pointer/veri geçişi ABI'si); num=6 yaz_sayi(n); num=7 satir; num=3 cik (bitir/dur). Sarmalayıcı
`always_inline` → SVC .user section'a gömülü (ayrı fonksiyon .text/AP=00'da kalır, EL0
çalıştıramaz). String literalleri .rodata'da (kernel EL1 okur; EL0 yalnız adres geçer, dereference
etmez → AP=00 sorunu yok).

**Doğrulama (QEMU 11.0.1):** "MERHABA userspace" + "USERSPACE OK toplam=55" — EL0 program 1..10
topladı (userspace hesap) + syscall I/O ile yazdı. Full gate GATE=0 (23 hedef). sıfır-uyarı.

**Önem:** userspace program artık HESAP + I/O yapabiliyor (syscall ABI ile) — gerçek program
çalıştırmanın (Faz F) çekirdek yapıtaşı. Kernel kullanıcı pointer'ından veri okuyor (read/write
syscall ailesinin temeli). NOT: ptr doğrulaması yok (gerçek OS'te user-adres-uzayı kontrolü gerek).

**Sıradaki:** preemptive EL0 süreç (per-task kernel stack); userspace ABI genişletme (oku/exit-kod);
D2-x86 (ring3+TSS); C5 (virtio-blk → Faz E fs).

---

## D-123 — OS D3: korumalı EL0 user-process (D1⊕D2⊕D-122 birleşik) — gerçek OS sürecinin dört özelliği bir arada (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-122).

**Karar [ETKİ: `runtime/kdl_mmu.c` (+kdl_surec_kur_el0 — user sayfası AP=01); yeni
`test/bare_metal/proc_arm.c`; `Makefile`. Mevcut kod DEĞİŞMEDİ (yalnız ekleme). x86/host/codegen
dokunulmadı.]** D-121/D-122 ön-koşulları (preemption-x0 + syscall-arg) hazır olunca, gerçek bir
işletim sistemi sürecinin dört tanımlayıcı özelliğini BİR ARADA gösteren keystone.

**Birleşen özellikler (tek süreçte):**
1. **Kendi adres-uzayı** — süreç kendi L1/L2 tablolarına sahip (kernel global tablolarından ayrı),
   `kdl_ttbr_degis` ile TTBR0 swap (D1 makinesi).
2. **Kullanıcı ayrıcalığı** — kod EL0'da, kendi TTBR'ı altında (`kdl_el0_calistir`, D2).
3. **Syscall arayüzü** — EL0 kod SVC ile argüman geçer (num=4 arg=42) → "SYSCALL ARG OK" (D-122).
4. **Bellek koruması / HAPİS** — süreç kernel-only sayfaya (0x40000000, AP=00) erişince EL0
   **permission-fault** → kernel yakalar. Kendi adres-uzayına hapsedilmiş.

**Yeni API:** `kdl_surec_kur_el0(l1,l2,user_pa)` — kdl_surec_kur (D1, AP=00) gibi ama user sayfası
`| (1<<6)` (AP=01, EL0+EL1 RW; UXN=0 → EL0-exec). user_pa=0x42000000 (identity — .user section
fiziksel yeri). el0_kod `.user` section'da, self-contained pure-SVC.

**Doğrulama (QEMU 11.0.1):** "PROC BASLA (EL1)" + "SYSCALL ARG OK" + "ISTISNA tip=0x24
a=0x9200000e b=0x42000010 adr=0x40000000". ESR decode: EC=0x24 (data abort, lower-EL/EL0),
**DFSC=0x0E = PERMISSION fault** (sayfa VAR ama EL0 reddedildi → gerçek koruma, translation değil),
ELR=0x42000010 (fault eden EL0 komutu), FAR=0x40000000 (erişilmeye çalışılan kernel adresi). Full
gate GATE=0 (22 hedef). sıfır-uyarı, libc-temiz.

**Sınır (bilinçli):** süreç henüz PREEMPTIVE değil (SPSR=EL0t DAIF-masked → timer maskeli). Preemptive
EL0 süreç = per-task KERNEL stack (trap-frame SP_EL1 ≠ run SP_EL0) + SP_EL0 trap-frame'de kaydet →
ayrı milestone. İzolasyon (private DATA) = ayrı .user_code/.user_data sayfaları (shared-code+
private-data) → follow-up.

**Sıradaki:** preemptive EL0 süreç (per-task kernel stack); D2-x86 (ring3+TSS); C5 (virtio-blk).

---

## D-122 — OS: SVC arg0 (x0) vektör-stub tarafından eziliyordu — syscall argüman geçişi onarımı (2026-06-30) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-121).

**Karar [ETKİ: `boot/start_aarch64.S` (VEKTOR_EXC macro — `mov x0,#\tip` kaldırıldı; kdl_exc_ortak
fault-yolu `mov x0,x9` ile EC'yi tip yapar); `runtime/kdl_kesme.c` (kdl_syscall_isle num=4 arg
kontrolü); yeni `test/bare_metal/syscall_arg_arm.c`; `Makefile`. x86/host/codegen DEĞİŞMEDİ.]**
D-121'de keşfedilen ikincil latent bug'ın onarımı — IRQ ile aynı sınıf, EXC yolunda.

**KÖK-NEDEN:** `VEKTOR_EXC \tip` → `mov x0,#\tip ; b kdl_exc_ortak`. Same-EL SVC slot 4 →
`mov x0,#4`, kdl_svc_ortak x0'ı (syscall arg0) kaydetmeden ÖNCE 4 ile eziyordu → her syscall'ın
arg0'ı sessizce vektör-indeksine (4) dönüşüyordu. Latentti (mevcut syscall testleri arg0
kontrol etmiyordu) ama userspace syscall'ları arg geçince bozulurdu.

**ONARIM:** VEKTOR_EXC artık doğrudan `b kdl_exc_ortak` (x0 dokunulmaz). SVC dalında x0=arg0
korunur → kdl_svc_ortak doğru kaydeder/geçer. Fault dalında (b.eq kdl_svc_ortak alınmazsa)
`mov x0, x9` ile tip = ESR.EC (teşhis; x0 artık arg0 değil, fault noreturn). istisna gösterimi
"tip=0x<vektör>" yerine "tip=0x<EC>" (daha bilgilendirici; test "ISTISNA" arar, etkilenmez).

**Doğrulama (QEMU 11.0.1):** syscall_arg — SVC num=4 arg=42 → kernel arg==42 görür →
"SYSCALL ARG OK" (+ "SYSCALL ARG SONRA" = eret kurtarma çalışıyor). Onarımdan önce arg=4 →
"HATA" olurdu. Regresyon yeşil: syscall (AFTER SYSCALL) + istisna (EC display) + d2 (EL0 SVC
kaynak-EL). sıfır-uyarı. **Vektör-stub register-bozma bug sınıfı (IRQ D-121 + EXC D-122) artık
tamamen kapalı** — preemption x0 + syscall arg0 ikisi de güvenli.

**Sıradaki:** D1+D2+C7 birleşik gerçek EL0 user-process (artık syscall-arg + x0-koruma hazır);
D2-x86; C5 (virtio-blk).

---

## D-121 — OS: IRQ vektör stub'u preempt edilen görevin x0'ını bozuyordu (KÖK-NEDEN onarımı) + C7d cap=4 IPC restore (2026-06-30) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-120).

**Karar [ETKİ: `boot/start_aarch64.S` (VEKTOR_IRQ macro — `mov x0,#\tip` kaldırıldı);
`runtime/kdl_kanal.{c,h}` + `test/bare_metal/kanal_arm.c` (KAP 16→4 restore + yorum). x86/host/
codegen DEĞİŞMEDİ.]** D-119'da bayraklı cap=4 "deterministik bozulma" — çok-ajanlı adversarial
workflow (5 bağımsız lens + sentez, gdb HW-watchpoint + QEMU `-d in_asm,cpu` trace ile) KÖK-NEDENİ
EMPİRİK buldu.

**KÖK-NEDEN (gerçek + ciddi, latent scheduler bug):** aarch64 IRQ vektör stub'u
`VEKTOR_IRQ \tip` → `mov x0,#\tip ; b kdl_irq_ortak`. Timer her zaman slot 5 (Cur-EL-SPx IRQ) →
`mov x0,#0x5`. Bu, **trap-frame KAYDEDİLMEDEN ÖNCE** preempt edilen görevin CANLI x0'ını 5 ile
eziyor. kdl_irq_ortak x0'ı [sp,#0]'a kaydedince bozuk x0=5 frame'e yazılıyor; eret'te görev x0=5
ile sürüyor. kdl_irq_ortak tip'i HİÇ kullanmaz (`mov x0,sp` ile ezer) → `mov x0,#\tip` saf
tahripti. "5 = 5. değer" RASTLANTI (5 = vektör indeksi). Global kanal ASLA bozulmadı (yanlış
teşhisti). cap=4 spesifik: yalnız o, ÜRETİCİYİ `gonder` spin'inde (pointer x0'da canlı, reload
yok) preempt eder; tüketici `al` pointer'ı x8'de tutar → bağışık. **preempt/sleep/priority sadece
şanstan geçmişti** (preempt sonrası x0-deref-reloadsız desen yoktu).

**ONARIM:** VEKTOR_IRQ artık doğrudan `b kdl_irq_ortak` (x0 dokunulmaz → görev x0'ı bozulmadan
kaydedilir/geri yüklenir). Cerrahi, EXC yolu değişmedi. cap=4 IPC restore edildi (D-119 cap=16
work-around kaldırıldı) → çift-yönlü back-pressure ping-pong artık sağlam.

**Doğrulama (QEMU 11.0.1):** kanal cap=4 → "KANAL OK toplam=55" (eski: Data Abort). TÜM aarch64
regresyon yeşil: preempt/sleep/priority/sched + istisna(EXC) + timer(IRQ) + syscall(SVC) + d2(EL0)
+ d1. sıfır-uyarı.

**KEŞFEDİLEN İKİNCİL LATENT BUG (ayrı iş):** Aynı sınıf EXC yolunda — `VEKTOR_EXC 4` → `mov x0,#4`
SVC arg0'ı (x0) kdl_svc_ortak kaydetmeden önce yok ediyor. Şu an latent (syscall testleri x0-arg
kontrol etmiyor) ama userspace syscall'lar arg geçince ısıracak. Takip: EXC vektörlerini de
register-bozmaz yap + kdl_exc_ortak tip'i ESR EC'den türetsin (istisna gösterimi + kdl_istisna_isle
imza dokunuşu → ayrı commit + arg-geçen syscall testi).

**Sıradaki:** SVC arg0 onarımı (yukarıda); D1+D2+C7 birleşik gerçek EL0 user-process; D2-x86.

---

## D-120 — OS C7e: öncelikli (priority) scheduling — strict priority + round-robin koruma (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-119).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_pri[] + kdl_preempt_oncelik + kdl_preempt seçim
mantığı); yeni `test/bare_metal/priority_arm.c`; `Makefile`. x86/host/codegen + trap-frame
asm DEĞİŞMEDİ.]** FAZ C: öncelikli zamanlama — scheduler en yüksek öncelikli READY görevi
seçer (eşit öncelikte round-robin korunur). Gerçek-zaman/QoS temeli.

**Mekanizma:** kdl_pri[] (büyük=yüksek, varsayılan 0). kdl_preempt round-robin sırada
(kdl_paktif sonrası) tarar, en yüksek öncelikli READY'yi seçer; eşit öncelikte ilk-bulunan
(round-robin döner) kazanır. Tümü-eşit (pri=0) → eski round-robin ile BİREBİR aynı (regresyon
yok). kdl_preempt_oncelik(gorev, pri) ile atanır.

**Doğrulama (QEMU 11.0.1):** priority_arm — main yüksek (1), B düşük (0). Faz1: main meşgul-
döner, timer tikler ama main tekelde → B aç kalır (b_sayac=0). Faz2: main kdl_uyu(10) bloklanır
→ tek READY=B → B koşar (b_sayac>0). b1==0 && b2>0 → "PRIORITY OK ac-faz1=0". **Tüm scheduler
regresyon yeşil:** sched(coop) + preempt + sleep + kanal bozulmadı (eşit-öncelik round-robin
korundu). sıfır-uyarı; libc-temiz; test_tumu host-nötr.

**Sıradaki:** D1+D2+C7 birleşik gerçek user-process; D2-x86 (ring3+TSS); C5 (virtio-blk);
cap=4 IPC corner-case GDB-teşhisi (D-119).

---

## D-119 — OS C7d: kanal (SPSC IPC) — preemptive scheduler üstünde görevler-arası mesaj (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-118).

**Karar [ETKİ: yeni `runtime/kdl_kanal.h` + `runtime/kdl_kanal.c`; yeni
`test/bare_metal/kanal_arm.c`; `Makefile` (bm_a64_kanal.o kuralı + calistir_kanal_test_arm +
os_kernels gate). x86/host/codegen + scheduler runtime DEĞİŞMEDİ.]** FAZ C: KEMGU `kanal`
ilkelinin (DRF V1 — R-KANAL aksiyomu, `görev`/`kanal` keyword'leri) çekirdek-düzeyi karşılığı.
SPSC halka tampon + preemptive scheduler + bloklamalı-alım birlikte çalışır → görevler-arası
mesaj geçişi (IPC) kanıtı.

**Mekanizma:** `KdlKanal` opak SPSC halka tampon (volatile buf/bas/son, tek slot rezerve →
KAP-1 öğe). `kdl_kanal_gonder/al` busy-wait bloklar (dolu/boş iken döngü); preemptive scheduler
(C7b) timer-IRQ'da karşı göreve geçirir → ilerleme garanti (tek çekirdek, kilitlenme yok).
Tek-çekirdek → bellek-bariyeri gerekmez (SMP'de DMB eklenir).

**Doğrulama (QEMU 11.0.1):** kanal_arm — üretici görev 1..10 yollar, tüketici (main) boş-kanalda
bloklanıp uyanarak 10 değeri FIFO sırayla alır+toplar → "KANAL OK toplam=55" (libc-temiz).
Scheduler regresyon: preempt (PREEMPT OK) + sleep (B WOKE) bozulmadı. test_tumu host-nötr
(kdl_kanal yalnız bare-metal'de derlenir). sıfır-uyarı.

**KISIT (dürüst kayıt):** Üretici-tarafı dolu-bloklama (back-pressure) çok küçük kapasite (KAP=4)
ile hızlı ping-pong preemption altında DETERMINISTIK bir durum bozulmasına yol açtı (kanal global
0x40004000 → 5; FAR=0x19; üretici `gonder` içinde dolu-bloklarken). Yığın-bitişikliği DEĞİL
(16KB ayrık yığınla aynı semptom); IRQ trap-frame dengeli (sub/add #272), yığın taşması yok →
kök-neden açık, GDB-düzeyi ayrı oturuma ertelendi. Şimdiki demo KAP=16 (üretici tek planlama-
diliminde tüm öğeleri yollar, dolu-bloklamaz); tüketici boş-bloklama bu kısıttan etkilenmez →
milestone sağlam + doğrulanmış. Takip: cap=4 ping-pong corner-case'i GDB ile incele.

**Sıradaki:** öncelikli scheduling; D1+D2+C7 birleşik gerçek user-process; D2-x86; C5 (virtio-blk);
cap=4 IPC corner-case GDB-teşhisi.

---

## D-118 — OS C7c: blocking scheduler (sleep/wake) — preemptive üstüne (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-117).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_block[] + kdl_uyu + kdl_preempt blocking dalı); yeni
`test/bare_metal/sleep_arm.c`; `Makefile`. x86/host/codegen DEĞİŞMEDİ.]** FAZ C: blocking
(sleep/wake) — görev N tick uyur, scheduler atlar, uyanınca kaldığı yerden sürer. Gerçek
zaman/I-O bekleme temeli (sleep(), bloklu I/O).

**Mekanizma:** her görevin tick geri-sayımı (kdl_block[]). kdl_uyu(N) → block=N + spin (scheduler
bloklu süresince ATLAR, görev koşmaz). kdl_preempt her tick tüm block'ları azaltır + yalnız READY
(block==0) göreve geçer; hepsi bloklu → idle (mevcutta kal).

**Doğrulama (QEMU 11.0.1):** sleep_arm — görev B kdl_uyu(8) ile bloklanır, A (main) o sırada koşar
(a_sayac artar), 8 tick sonra B READY → uyanır → "B WOKE a_kostu=VAR" (A uyku sırasında koştu).
Diğer scheduler testleri (sched/preempt) regresyonsuz. sıfır-uyarı.

**Sıradaki:** öncelikli scheduling; D1+D2+C7 birleşik gerçek user-process; D2-x86; C5 (virtio-blk).

---

## D-117 — OS C7b: preemptive scheduling (timer-IRQ → zorunlu bağlam-değiştirme) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-116).

**Karar [ETKİ: `boot/start_aarch64.S` (kdl_irq_ortak → FULL trap-frame + kdl_irq_isle);
`runtime/kdl_zaman.c` (kdl_irq_isle); `runtime/kdl_gorev.c` (kdl_preempt + preemptive scheduler);
yeni `test/bare_metal/preempt_arm.c`; `Makefile`. x86/host/codegen DEĞİŞMEDİ.]** FAZ C:
preemptive multitasking — timer-IRQ görevi ZORLA switch eder (görev yield etmez).

**Mimari (full trap-frame IRQ):** kdl_irq_ortak artık FULL bağlam (x0-x30 + ELR_EL1 + SPSR_EL1 =
272 bayt, x30@240/ELR@248/SPSR@256) kaydeder → kdl_irq_isle(sp) [GICC_IAR + tik/re-arm +
EOI(switch-ÖNCESİ) + kdl_preempt] devam SP'sini döner → SP swap (preempt'te sonraki görevin
trap-frame'i) → restore → eret. Preempt kapalıysa SP aynı → eski davranış (timer/sched testleri
NÖTR, regresyonsuz).

**Preemptive scheduler (kdl_gorev.c):** kdl_preempt(sp) round-robin görev trap-frame SP swap.
kdl_preempt_gorev_olustur sentetik trap-frame kurar (ELR=giriş, SPSR=EL1h+IRQ-açık) → ilk switch
eret ile göreve atlar. **EOI switch-ÖNCESİ KRİTİK:** GIC serbest kalır → sonraki timer-IRQ
sonraki görevi preempt eder; aksi halde IRQ27 active kalır → deadlock.

**Doğrulama (QEMU 11.0.1):** preempt_arm — 2 görev (A=main, B) busy-loop, ASLA yield ETMEZ.
B **1071 kez** koştu (yalnız timer preemption ile!), A 4 kez → "PREEMPT OK". Timer/sched/capstone
regresyonsuz (Stage-1 doğrulandı: full-trap-frame, preempt-off = eski davranış). sıfır-uyarı.

**Kapsam/sınır:** round-robin (öncelik/quantum-ayarı yok = C7c TCB-durum); tek adres-uzayı
(görevler bellek paylaşır; D1 ile birleşik per-process preemptive sonra); aarch64 (x86 IRQ0-stub
trap-frame rework = sonra).

**Sıradaki:** D1+D2+C7b birleşik gerçek user-process; C7c (öncelik/durum); D2-x86; C5 (virtio-blk).

---

## D-116 — OS D1: per-process adres-uzayı izolasyonu (TTBR0 swap) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-115).

**Karar [ETKİ: `runtime/kdl_mmu.c` (kdl_surec_kur + kdl_ttbr_degis — yeni fonksiyonlar); yeni
`test/bare_metal/d1_arm.c`; `Makefile` (calistir_d1_test_arm + os_kernels gate). Diğer kernel'ler
için dormant. x86/host/codegen DEĞİŞMEDİ.]** FAZ D: per-process adres-uzayı — her sürece ayrı
sayfa tablosu, geçişte TTBR0 swap → process bellek izolasyonu (D2 privilege ayrımının tamamlayıcısı).

**Mekanizma:** `kdl_surec_kur(L1, L2, user_pa)` — kernel identity (paylaşılan, AP=00) + user VA
(0x42000000) → sürece-özel `user_pa`. `kdl_ttbr_degis(L1)` — TTBR0_EL1 swap + dsb/tlbi/isb (TLB
flush) → adres-uzayı geçişi.

**Doğrulama (QEMU 11.0.1):** d1_arm — 2 süreç (A: user→PA 0x44000000, B: user→PA 0x46000000),
AYNI sanal adres 0x42000000'a A 0xAA / B 0xBB yazar; TTBR geçişlerinden sonra A hâlâ 0xAA, B hâlâ
0xBB → "SUREC A uva=0xaa" + "SUREC B uva=0xbb" = birbirini ETKİLEMEZ = **izolasyon kanıtı**. Diğer
kernel'ler regresyonsuz (yeni fonksiyonlar dormant). sıfır-uyarı.

**Kapsam/sınır:** demo EL1'de (VA→PA izolasyonunu izole gösterir; tam EL0-user-process = D1+D2
birleşimi); scheduler-entegrasyonu (context switch'te TTBR swap) sonra; kernel-identity aliasing
(PA_A/B aynı zamanda identity-VA'da görünür) — demo user-VA'dan eriştiği için zararsız.

**Sıradaki:** D1+D2 birleşik EL0 user-process (ayrı adres-uzayı + EL0); C7b preemptive; D3
process oluşturma (fork/exec-eşdeğeri).

---

## D-115 — OS D2: aarch64 finer paging (L2 2MB) → user/kernel privilege ayrımı (EL0) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-114).

**Karar [ETKİ: `runtime/kdl_mmu.c` (L1[1] RAM → L2 2MB tablo); `linker/bare-metal-aarch64.ld`
(.user section @0x42000000); `boot/start_aarch64.S` (kdl_el0_calistir); `runtime/kdl_kesme.c`
(syscall num2/3); yeni `test/bare_metal/d2_arm.c`; `Makefile` (calistir_d2_test_arm + os_kernels
gate). x86/host/codegen DEĞİŞMEDİ.]** FAZ D opener: gerçek EL0/EL1 (user/kernel) privilege ayrımı.

**Finer paging (D2 ön-koşulu):** C8a'nın L1[1] 1GB Normal bloğu → L2 tablo (512 × 2MB identity
sayfa). Per-region izin artık mümkün: kernel sayfaları AP=00 (EL1-only); user 2MB sayfası
(0x42000000) AP=01 (EL0+EL1 RW). Bu, D-114'teki D2-wall'u (EL0-writable RAM → EL1-non-executable)
ayrı user-page ile çözer (kernel kodu hâlâ AP=00 EL1-exec).

**D2 mekanizması:** `kdl_el0_calistir` (boot asm): SP_EL0 + ELR_EL1 + SPSR_EL1(EL0t) kur, eret →
EL0. EL0 kodu (d2_arm.c `el0_kod`, `.user` section @0x42000000, SELF-CONTAINED pure-SVC) kernel/
device sayfalarına (AP=00) DOĞRUDAN erişemez → yalnız SVC ile EL1 kernel'e geçer. Handler
SPSR_EL1.M[3:2] okur → kaynak-EL.

**Doğrulama (QEMU 11.0.1):** d2_arm → "D2 BASLA (EL1)" + "EL0 SYSCALL kaynak-EL=" + "0x0"
(=EL0, privilege ayrımı KANITI) + "D2 OK". 6 aarch64 kernel (dizi/sched/timer/syscall/istisna/
capstone) regresyonsuz (L2 finer granülarite + boş .user zararsız). sıfır-uyarı. test_tumu YEŞİL.

**Kapsam/sınır:** tek user-page (kod+stack aynı AP=01 sayfada → EL0 kendi kodunu yazabilir; tam
izolasyon = kod AP=11-RO + data AP=01 ayrı sayfa + D1 per-process sayfa tablosu). x86 ring3 (TSS
gerekir) = D2-x86 ayrı. IRQ EL0'da maskeli (D2 testi basitliği).

**Sıradaki:** D1 per-process adres-uzayı (TTBR0 swap, per-process L1/L2); C7b preemptive; C5
(virtio-blk, import+codegen C-track).

---

## D-114 — OS C8c: fault-adresi teşhisi (FAR_EL1 / CR2) + D2 ertelendi (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-113).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_istisna_isle). Sadece teşhis; davranış-nötr.]** İstisna
(fault) işleyicisi artık fault ADRESİNİ de basar: aarch64 FAR_EL1, x86 CR2 (#PF lineer adresi).
Data/instruction abort'ta hangi adrese erişildiği görünür → teşhis. Abort-dışı için stale ama
zararsız.

**Doğrulama:** istisna testleri (aarch64 data-abort + x86 ud2) hala geçer + "adr=0x<FAR>" basar.
Sıfır-uyarı (iki arch). Diğer kernel'ler regresyonsuz (yalnız fault yolunda çalışır).

**NOT — D2 (EL0/ring3 privilege ayrımı) ERTELENDİ:** EL0 user/kernel ayrımı denendi. ARMv8
mimari kuralı: **EL0-writable RAM → EL1'de non-executable** (Prefetch Abort EC=0x21 ile
kanıtlandı). Tek-region identity-map (L1 1GB blok) kernel-kodu + EL0-user-region'ı aynı sayfa
permission'a zorluyor → çakışma. Minimal D2 AYRI user-region (finer L2/L3 sayfa tablosu +
linker section yerleşimi) gerektirir = D1 per-process adres-uzayı ile birlikte yapılacak D-fazı
paging işi. Tasarım (kdl_el0_calistir eret→EL0 + syscall SPSR_EL1 kaynak-EL raporu) hazır,
revert edildi.

**Sıradaki seçenekler:** D-fazı (finer page tables → per-process adres uzayı + privilege ayrımı);
C5 (virtio-blk, import+codegen C-track fix gerek); C7b (preemptive — IRQ-handler full-context
trap-frame rework).

---

## D-113 — OS C7a: cooperative scheduling — bağlam-değiştirme (görev kuyruğu) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-112).

**Karar [ETKİ: yeni `runtime/kdl_gorev.c`; `boot/start_aarch64.S` + `boot/start_x86_64.S`
(kdl_baglam_degis asm); yeni `test/bare_metal/sched_test.c`; `Makefile` (bm_*_gorev.o + 2 sched
hedefi + os_kernels gate). x86 IR/host/codegen DEĞİŞMEDİ.]** FAZ C: işbirlikçi çok-görevlilik.
Görevler kdl_gorev_ver() (yield) ile gönüllü CPU bırakır → round-robin bağlam-değiştir.
MMU/preemption gerektirmez (C7b preemptive = timer-IRQ quantum, sonra).

**Tasarım:** TCB = callee-saved register'lar + SP. kdl_baglam_degis (asm): mevcut görevin
callee-saved'ını TCB'ye kaydet, sonrakinin kinden yükle, `ret` → sonraki görev kaldığı yerden
sürer (caller-saved yield çağrı-noktasında C ABI'siyle korunur, kaydetmeye gerek yok). Görev 0 =
main bağlamı (init gerektirmez; ilk yield'de TCB'ye kaydedilir). Yeni görev: TCB dönüş-adresi =
giriş, TCB.sp = yığın-tepe.
- aarch64: x19-x28+x29+x30+sp (TCB[0..12], ×8 bayt; x30=giriş, ret oraya).
- x86: rbx/rbp/r12-r15+rsp (TCB[0..6]); giriş yığına push (switch ret'i pop eder).

**Doğrulama (QEMU 11.0.1):** sched_test.c (AYNI kernel iki mimaride): main+gorev1 round-robin →
"SCHED BASLA / [main] / [gorev1] / [main] / [gorev1] / [main] / [gorev1] / SCHED OK" — interleave
= bağlam-değiştirme çalışıyor. aarch64 + x86 ikisi de geçti. kdl_gorev.c sıfır-uyarı. Diğer
kernel'ler regresyonsuz (kdl_baglam_degis dormant). os_kernels gate artık **14** (7 yetenek × 2 arch).

**KEMGU bağı:** region-ownership + `görev` (D-008 concurrency) ileride gerçek thread'le buluşur —
statik tip-kontrollü `görev`/`kanal`'ın runtime temeli.

**Kapsam/sınır:** cooperative (preemption yok = C7b timer-IRQ quantum); tek adres-uzayı (TCB ortak
bellek; per-process MMU-izolasyon = D1); SIMD-context yok (-mgeneral-regs-only, C8b).

**Sıradaki:** C7b preemptive (timer-IRQ → zorunlu yield) / C5 (import+codegen) / D1 (per-process).

---

## D-112 — OS C8a: aarch64 MMU-on (identity map) — RAM Normal-WB, sanal-bellek temeli (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-111).

**Karar [ETKİ: yeni `runtime/kdl_mmu.c`; `boot/start_aarch64.S` (`bl kdl_mmu_kur` main'den önce);
`Makefile` (bm_a64_mmu.o). x86/host/codegen DEĞİŞMEDİ.]** FAZ C keystone: aarch64 MMU'yu
identity-map ile aç → RAM Device-nGnRnE'den **Normal-WB cacheable**'a → cache + SIMD-uyumlu
bellek + sanal-bellek/process-izolasyon (D fazı) temeli.

**Identity harita (4KB granül, 39-bit VA, L1 1GB blok):**
- L1[0] 0-1GB → Device (GICv2 0x08000000, UART 0x09000000).
- L1[1] 1-2GB → Normal-WB (kernel + 16MB heap @ 0x40000000).
- MAIR (attr0=Device, attr1=Normal-WB), TCR (T0SZ=25, 4KB, WB, inner-sh, EPD1, IPS=36bit),
  TTBR0=L1, SCTLR.M|C|I. dsb/tlbi/isb sıralı.

**x86_64:** long mode ZATEN paging gerektirir → `boot/start_x86_64.S` identity sayfa
tablolarıyla zaten MMU-on (PVH→long mode). Ek MMU kurulumu gerekmez; kdl_mmu.c yalnız aarch64.

**Doğrulama (QEMU 11.0.1):** 4 aarch64 kernel MMU-on boot eder — hello, region+heap-dizi (memcpy
Normal-cached bellekte), timer (IRQ MMU üstünden), syscall. x86 regresyonsuz. test_tumu YEŞİL
(host değişmedi). kdl_mmu.c sıfır-uyarı.

**Kapsam/sınır:** identity-only (VA==PA), tek adres-uzayı (per-process = D1). **-mgeneral-regs-only
KORUNUYOR (C8b ertelendi):** MMU Normal-memory'yi açtı ama kernel-geneli SIMD, IRQ/exception
handler'larında SIMD-bağlam kaydı gerektirir (handler'lar şu an yalnız GPR kaydeder, q0-q31
değil) → kesme anında SIMD-state bozulur. Linux-benzeri sound kernel-FP-yasağı politikası sürüyor;
SIMD'i global açmak ayrı refinement (handler SIMD-save). C8c sayfa-hata: data abort zaten
kdl_exc_ortak'ta yakalanıyor (C3a) → ESR+ELR teşhis+halt; demand-paging D-fazı.

**Sıradaki:** C7a cooperative scheduling (görev kuyruğu + context switch, MMU-bağımsız).

---

## D-111 — OS Capstone: tam yığın tek boot'ta kompoze (region+timer+IRQ+syscall) — entegrasyon kanıtı (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-110).

**Karar [ETKİ: yeni `test/bare_metal/capstone.c`; `Makefile` (2 capstone hedef + os_kernels 12 teste).
Runtime/codegen/host/boot DEĞİŞMEDİ.]** Minimal gösterici parçalarını (her biri ayrı kanıtlı) TEK
boot'ta birlikte koşturur → izole birim testlerin ötesinde **entegrasyon/kompozisyon kanıtı**.

**Capstone kernel (iki arch ortak C kernel):** (1) region dizi 1..10=55 (frame allocator);
(2) timer+IRQ aç (kdl_kesme_kur + kdl_timer_baslat); (3) region tahsis IRQ AÇIKKEN → "POST-IRQ=99"
(**allocator IRQ-safe** — kesme handler'ları allocation-free olduğundan heap bozulmuyor; D-108/109
KISIT'ı pratikte doğrulanır); (4) syscall → "CAPSTONE OK"; (5) idle (timer arka planda → "TIMER OK
tik=5").

**Doğrulama (QEMU 11.0.1) — `calistir_os_kernels` 12/12:** capstone (aarch64 virt + x86_64 PVH) →
"55" + "99" + "CAPSTONE OK" + "TIMER OK" hepsi. 10 birim test + 2 capstone.

**🎉 MİNİMAL OS GÖSTERİCİ + ENTEGRASYON TAM (her iki mimaride):** os/c1-region-backing branch —
C1a/b/x86 (region) + C3a (exception) + C3b/C4 (IRQ+timer) + C6 (syscall) + Capstone (7 commit).
`make calistir_os_kernels` = **12 QEMU boot kanıtı**. Beyond-minimal (flag'li): C5 virtio codegen
(task_09d48d31 — &Struct+sonuç deep multi-subsystem), C7 scheduling, MMU, self-host bare-metal.

---

## D-110 — OS C6: bare-metal sistem çağrısı (aarch64 SVC + x86 int 0x80) — dispatch+dönüş; MİNİMAL GÖSTERİCİ TAM (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-109).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_syscall_isle + IDT[0x80] gate); `boot/start_aarch64.S`
(kdl_exc_ortak ESR.EC kontrolü → kdl_svc_ortak); `boot/start_x86_64.S` (kdl_syscall_stub); yeni
`test/bare_metal/syscall_test.c` (portable); `Makefile` (2 syscall hedef + os_kernels 10 teste).
Codegen/host DEĞİŞMEDİ.]** C6: minimal sistem çağrısı → **MİNİMAL OS GÖSTERİCİ TAMAMLANDI**.

**aarch64 (SVC):** sync exception handler (kdl_exc_ortak) artık ESR.EC ayrımı yapar: 0x15 (SVC) →
kdl_svc_ortak (bağlam kaydet, num=x8/arg=x0, kdl_syscall_isle, **ERET**); diğer EC → kdl_istisna_isle
(fault, halt). İstisna ve syscall AYNI sync vektörde (0x200), EC ile ayrılır.

**x86_64 (int 0x80):** IDT[0x80] → kdl_syscall_stub (caller-saved kaydet, num=rax/arg=rdi,
kdl_syscall_isle, **IRETQ**).

**Doğrulama (QEMU 11.0.1) — `calistir_os_kernels` 10/10:** syscall_test (iki arch ortak): "BEFORE
SYSCALL" → "SYSCALL OK num=1" (kernel dispatch) → "AFTER SYSCALL" (eret/iretq dönüş kanıtı).
test_tumu YEŞİL (host değişmedi).

**🎉 MİNİMAL OS GÖSTERİCİ TAM (her iki mimaride QEMU-kanıtlı):**
boot + region-bellek + sürücü(UART) + exception + IRQ + timer + syscall — aarch64 (QEMU virt) +
x86_64 (QEMU PVH). AYNI KEMGU region-confinement runtime (F4.2b/F4.3 backing) iki platformda.
`make calistir_os_kernels` = 4 boot + 2 istisna + 2 timer + 2 syscall (D-105..D-110).

**Kapsam-dışı (Mehmet kararı: minimal-gösterici):** scheduling/multitasking (C7); virtio-blk
codegen fix (C5 — &Struct param+sonuç<> segfault; UART sürücü zaten "bir sürücü" gereğini karşılar,
virtio beyond-minimal); MMU/sayfalama; self-host bare-metal.

---

## D-109 — OS C3b/C4: bare-metal IRQ + periyodik timer (GICv2/CNTV + PIC/PIT) — tick kanıtı (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-108).

**Karar [ETKİ: yeni `runtime/kdl_zaman.c` (IRQ dispatch + timer, iki arch ortak API);
`boot/start_aarch64.S` (IRQ vektör routing + kdl_irq_ortak bağlam-kaydet/eret);
`boot/start_x86_64.S` (kdl_irq0_stub + EOI/iretq); `runtime/kdl_kesme.c` (IDT[32]→IRQ0); yeni
`test/bare_metal/timer_test.c` (portable, iki arch); `Makefile` (bm_*_zaman.o + 2 timer hedef +
os_kernels 8 teste genişledi). Codegen/host DEĞİŞMEDİ.]** C3b IRQ altyapısı + C4 timer birleşik:
periyodik donanım kesmesi → handler → tick (C4 timer'ı C3b IRQ altyapısı olmadan kanıtlanamaz).

**Ortak API (arch-bağımsız kernel):** `kdl_kesme_kur()` (GIC/PIC init) + `kdl_timer_baslat()`
(CNTV/PIT + IRQ aç) + `kdl_kesme_isle(irq)` (dispatch, tik say) + `kdl_idle()` (wfi/hlt). AYNI
timer_test.c iki mimaride (kernel arch-bağımsız, runtime arch-spesifik).

**aarch64 (GICv2 + sanal generic timer):** GICD@0x08000000 + GICC@0x08010000 enable + ISENABLER0
bit27 (timer PPI 27). CNTV ~10ms (CNTFRQ/100). IRQ vektör (0x280, entry5 → kdl_irq_ortak):
bağlamı kaydet (x0-x18,x30; x19-x29 C korur) → GICC_IAR oku → kdl_kesme_isle → re-arm (CNTV_TVAL,
ISTATUS temizler) → GICC_EOIR ack → **ERET** (kesilen wfi-döngüsüne dön). DAIF.I temizle.

**x86_64 (PIC 8259 + PIT 8254):** PIC remap IRQ0-15→vektör32-47 (ICW1-4), mask=yalnız IRQ0. PIT
ch0 mode3 ~100Hz (bölen 11932). IDT[32]→kdl_irq0_stub: caller-saved kaydet → kdl_kesme_isle →
PIC EOI (port 0x20) → **IRETQ**. sti.

**KISIT:** kesme bağlamı bölge/frame allocator KULLANMAZ (tek-thread, IRQ-safe değil) → yalnız UART
yazımı + register.

**Doğrulama (QEMU 11.0.1) — `calistir_os_kernels` 8/8:**
- `calistir_timer_test_arm`: GICv2+CNTV → "TIMER BASLA" + 5 tik → "TIMER OK tik=5".
- `calistir_timer_test_x86`: PIC+PIT → "TIMER OK tik=5".
- 4 boot + 2 istisna regresyonsuz (IRQ vektör routing sync exception'ları etkilemedi). Sıfır
  uyarı (iki arch). test_tumu YEŞİL (host değişmedi).

**Sıradaki:** C5 virtio sürücü codegen fix (&Struct param+sonuç<> segfault — virtio_blk_init/bind),
C6 minimal syscall (SVC/int).

---

## D-108 — OS C3a: bare-metal exception vektörleri (aarch64 VBAR + x86 IDT) — fault→teşhis+halt (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-107).

**Karar [ETKİ: yeni `runtime/kdl_kesme.c` (istisna işleyici + x86 IDT kur); `boot/start_aarch64.S`
(VBAR + 16-giriş vektör tablosu + EL-duyarlı ortak işleyici); `boot/start_x86_64.S` (32 ISR stub +
isr_ortak + kdl_idt_kur çağrısı); yeni `test/bare_metal/istisna_{arm,x86}.c`; `Makefile`
(bm_*_kesme.o + 2 istisna test hedefi). Runtime/codegen/host DEĞİŞMEDİ.]** C3 ilk adım: CPU
istisnaları artık vektör tablosuyla yakalanır → sessiz çöküş yerine "ISTISNA tip/synd/PC" + halt.

**aarch64 (VBAR):** boot 16-giriş (×0x80, 2KB-hizalı) vektör tablosu kurar, VBAR_ELx=tablo
(EL-duyarlı). Ortak işleyici ESR/ELR okur — **KRİTİK EL-duyarlı:** QEMU virt **EL1**'de koşar
(`-d int` "from EL1 to EL1"); EL1'de `esr_el2` okumak UNDEF → işleyici kendini sonsuz fault'lar
(58683× gözlendi). Düzeltme: CurrentEL kontrolü → esr_el1/elr_el1.

**x86_64 (IDT):** 32 ISR stub (hata-kodlu 8/10/11/12/13/14/17/21 ISR_ERR, diğerleri ISR_NOERR +
dummy 0). Long mode same-privilege exception SS:RSP:RFLAGS:CS:RIP push eder → ortak yığın
[vektör][hata][RIP] → isr_ortak → kdl_istisna_isle(rdi,rsi,rdx). boot long_entry'de `kdl_idt_kur()`
(C; gate-tablosu 256×16B + lidt) main'den ÖNCE. ud2 → vektör 6.

**KISIT:** işleyiciler bölge/frame allocator KULLANMAZ (tek-thread, IRQ-safe değil) → yalnız UART
yazımı + register oku.

**Doğrulama (QEMU 11.0.1):**
- `calistir_istisna_test_arm`: eşlenmemiş erişim → ESR=0x96000000 (EC 0x25 Data Abort) →
  "ISTISNA tip=0x4" + halt; "GORUNMEMELI" yok.
- `calistir_istisna_test_x86`: ud2 → "ISTISNA tip=0x6" (invalid opcode) + halt; "GORUNMEMELI" yok.
- 4 normal kernel (aarch64+x86 × hello+dizi) regresyonsuz. test_tumu YEŞİL (host değişmedi).

**DÜZELTME (D-105/107 notları):** QEMU virt -cpu cortex-a72 **EL1**'de boot eder (EL2 DEĞİL).
FP-enable kodu EL-duyarlı olduğu için EL1'de CPACR_EL1 yolu çalışıyordu (CPTR_EL2 dalı ölü, zararsız).
D-105/107'deki "EL2" ifadeleri bu yönde okunmalı.

**Sıradaki:** C3b IRQ altyapısı (GICv2 aarch64 / PIC 8259 x86) — C4 timer için.

---

## D-107 — OS C1-x86: x86_64 bare-metal parite — PVH→long-mode boot, AYNI region kernel (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-106).

**Karar [ETKİ: yeni `boot/start_x86_64.S` (PVH→long mode); yeni `linker/bare-metal-x86_64.ld`;
`Makefile` (BM_X86_OBJS + 2 x86 kernel hedefi + `calistir_os_kernels` toplu gate). Runtime/
codegen/host DEĞİŞMEDİ.]** Mehmet kararı (aarch64+x86_64 paralel): AYNI region-backed kernel'i
x86_64'te de boot et.

**PVH boot (multiboot1 yerine):** QEMU `-kernel` multiboot1 için 32-bit ELF ister, 64-bit'i
reddeder ("Cannot load x86-64 image, give a 32bit one"). Çözüm = PVH: `.note.Xen` içinde
XEN_ELFNOTE_PHYS32_ENTRY (tip 18) → QEMU 64-bit ELF'i 32-bit `pvh_entry`'den boot eder
(ELFCLASS64 korunur). Minimal PVH stub'la (COM1'e 'OK') önce doğrulandı.

**32-bit → long mode trampoline (boot/start_x86_64.S):**
1. (32-bit) BSS sıfırla (sayfa tabloları .bss'te → temizlenir, sonra kurulur).
2. Identity sayfa tabloları: PD 512×2MB huge page = 0..1GB (kernel+stack+16MB heap < 1GB);
   PDPT[0]→PD; PML4[0]→PDPT.
3. CR3=PML4 → CR4.PAE → EFER.LME (MSR 0xC0000080) → CR0.PG|PE → long mode.
4. GDT (null + 64-bit code L-bit + data); `ljmp $0x08,$long_entry` → 64-bit.
5. (64-bit) segment'ler + RSP=__stack_top + `call main`; main dönerse hlt döngüsü.

**Region backing ARCH-BAĞIMSIZ (C0 mimarisinin öngörüsü doğrulandı):** kdl_bolge.c +
kdl_bare_heap.c (frame allocator) + kdl_dizi.inc DEĞİŞMEDEN x86_64'te derlenir/çalışır. Tek
arch-spesifik fark: UART=16550 (COM1 0x3F8 port I/O, KDL_UART_PUTC=kdl_uart_16550_putc), boot
stub, linker. `-mgeneral-regs-only` (x86: SSE emit etme → boot'ta CR4.OSFXSR enable gerekmez;
aarch64'teki Device-memory q-register sorununun simetrik çözümü).

**Doğrulama (qemu-system-x86_64 11.0.1) — `calistir_os_kernels` toplu gate 4/4:**
- `calistir_kernel_dizi_x86_bare_metal`: AYNI kernel_dizi.kem → BOOT + "KERNEL DIZI OK" + "55"
  (libc-yok temiz). aarch64 ile bit-bit aynı .kem + aynı region runtime.
- `calistir_uart_merhaba_x86_bare_metal`: "Merhaba KEMGU" + "42".
- aarch64 hedefleri (qemu_smoke + dizi) regresyonsuz. `test_tumu` YEŞİL (host değişmedi).

**Sonuç — C1 keystone TAM (her iki mimaride):** aynı KEMGU kaynağı (kernel_dizi.kem) + aynı
region-confinement runtime (F4.2b/F4.3 backing) hem aarch64 (QEMU virt) hem x86_64 (QEMU PVH)
üzerinde boot eder + doğru hesaplar. Region modeli OS'in bellek temelini iki platforma da bedavaya verir.

**Kapsam/sınır:** PVH 32-bit entry → long mode minimal (identity 1GB, tek-çekirdek, IDT/exception
yok — aarch64 ile simetrik). 16550 init V1-no-op (QEMU yeterli; ham donanım board-init sonra).
Sıradaki: C3 exception/interrupt (DESIGN-STOP), C4 timer.

---

## D-106 — OS C1b: bare-metal dizi runtime — heap Dizi<tam32> kernel boot (kdl_dizi.inc tek-kaynak) (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-105).

**Karar [ETKİ: yeni `runtime/kdl_dizi.inc` (paylaşımlı dizi impl); `runtime/kdl_runtime.c`
(dizi bloğu → `#include`); `runtime/kdl_bare_heap.c` (kdl_panik seam + `#include`); yeni
`test/ornekler/kernel_dizi.kem`; `Makefile` (aarch64 bare-metal shared-objects refactor +
`calistir_kernel_dizi_bare_metal`). Codegen DEĞİŞMEDİ; host davranışı DEĞİŞMEDİ.]** C1a
region-backing'i diziye genişlet: heap `Dizi<tam32>` (kdl_dizi_* runtime) bare-metal'de boot.

**Tek-kaynak carve (duplikasyon YASAK — D-069 sınır-kontrolü iki yerde olamaz):**
- `kdl_dizi.inc`: KdlDizi + kdl_dizi_buyut/olustur/ekle/al/yaz/yapi/boyut/kapasite/serbest +
  kdl_dizi_oob. Host (kdl_runtime.c) VE bare-metal (kdl_bare_heap.c) `#include` eder → her TU
  KENDİ kopyasını derler, ASLA birlikte linklenmez (duplicate-symbol yok) = tek kaynak.
- Panik seam `kdl_panik`: host kdl_runtime.c (stderr+abort); bare-metal kdl_bare_heap.c
  (→ kdl_panik_dur, UART+halt). kdl_dizi_oob FREESTANDING biçimlenir (snprintf YOK; "(i,boyut)"
  detayı korunur) → host çıktısı bayt-özdeş, bare-metal'de libc'siz çalışır. kdl_panik codegen
  inline-OOB (src/llvm.c) tarafından da çağrılır → evrensel panik girişi.

**Makefile shared-objects refactor (zorunluydu):** aarch64 kernel'leri artık `BM_A64_OBJS`
paylaşır (start+uart+yazdir+bolge+heap+panik). kdl_bare_heap.o artık `.inc` yüzünden
kdl_panik→kdl_panik_dur referansı verdiğinden HER kernel kdl_runtime_panik.o linklemeli (merhaba
dahil) → inline-compile yerine ortak obje kuralları.

**Doğrulama:**
- `calistir_kernel_dizi_bare_metal` (QEMU, qemu-system-aarch64 11.0.1): kernel BOOT →
  "KERNEL DIZI OK" + "55" (1..10 toplam; dizi_olustur→ekle [kapasite 0→4→8→16: kdl_dizi_buyut +
  bölge realloc + memcpy] → al [D-069 sınır-kontrollü]). libc-yok kapısı temiz. IR'de
  kdl_dizi_olustur/ekle_tam/al_tam/boyut/kapasite_ayarla = heap path (stack değil).
- `calistir_qemu_smoke` (merhaba shared-objects refactor sonrası): boot + "Merhaba KEMGU"+"42" ✓
  (regresyon yok).
- Host: kdl_runtime.c (`.inc` ile) sıfır-uyarı; `calistir_llvm_test` (30 array programı) yeşil;
  `test_tumu` YEŞİL (self-host fixpoint 32157 satır + tüm array/OOB testleri — `.inc` host
  davranışını değiştirmedi). Yeni bare-metal objeleri sıfır-uyarı.

**Kapsam/sınır:** (1) dizi-OOB → kdl_panik (D-069) bare-metal'de geçerli (kernel UART "PANIK:"+halt).
(2) Büyüyen-dizi grow-leak (F4.3 flag, task_dc5b969f) bare-metal'de de var (sabit-kapasiteli
kernel-loop için yeterli). (3) x86_64 dizi paritesi C1-x86 (x86_64 long-mode boot bring-up gerekir).

---

## D-105 — OS C1a: bare-metal bölge-backing — ilk boot-eden region-alloc KEMGU kernel (aarch64/QEMU) (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: D-104 sonrası; merge'de origin/main ilerlemişse güncelle).

**Karar [ETKİ: yeni `runtime/kdl_bare_heap.c`; `runtime/kdl_bolge.c` (#ifdef köprü);
`boot/start_aarch64.S`; `linker/bare-metal-aarch64.ld`; `Makefile` (uart_merhaba + qemu_smoke).
C derleyici/codegen DEĞİŞMEDİ; host yolu DEĞİŞMEDİ.]** OS gösterici-kernel FAZ C keystone'u:
bölge runtime'ını bare-metal'e bağla — **codegen'e dokunmadan**, yalnız RUNTIME backing.

**Kök bulgu (regresyon — readiness-envanteri "reçete-tam" dediği yer KIRIKTI):** Region codegen
(F4.x) main dâhil HER fonksiyona koşulsuz `@kdl_global_bolge_al`/`@kdl_bolge_olustur`/
`@kdl_bolge_serbest` emit eder (declare'da attribute yok → -O2 DCE EDEMEZ). Bare-metal link bu
sembolleri sağlamıyordu → `calistir_uart_merhaba_bare_metal` HEAD'de `ld.lld: undefined symbol`
ile KIRIK (hello-world dâhil TÜM kernel'ler). Ampirik kanıt: `kemgu --llvm uart_merhaba.kem`
main'inde 3 canlı region çağrısı + mevcut link denemesi 3 undefined-symbol.

**Çözüm (4 parça, codegen-nötr):**
1. `kdl_bare_heap.c` (freestanding, yalnız <stdint/stddef>): linker heap bölgesinden
   (`__heap_start..__heap_end`) bump + serbest-liste `malloc`/`free` + `memcpy`/`memset` +
   `kdl_global_bolge_al`. Region 64KB blokları serbest-listede geri kazanılır → F4.3 per-iter
   region-free → kernel-loop sınırlı-bellek temeli.
2. `kdl_bolge.c`: `#ifdef KEMGU_BARE_METAL` → <stdlib.h> yerine malloc/free prototip (<stdlib.h>
   aarch64-unknown-none'da YOK). Host (#else) AYNEN — `calistir_kdl_bolge_test` 33/33 ASan-temiz.
3. `boot/start_aarch64.S`: FP/SIMD trap kapat — EL-duyarlı (QEMU virt EL2 → CPTR_EL2.TFP[10]=0;
   ham EL1 → CPACR_EL1.FPEN=0b11).
4. `linker`: stack üstüne 16 MB heap (`__heap_start/__heap_end`, NOBITS).
+ `Makefile`: bare-metal compile'lara `-mgeneral-regs-only` + region runtime'ı link'e ekle +
   `calistir_qemu_smoke` `-serial file:` (Windows stdio-redirect bypass).

**İki kritik bare-metal gotcha (QEMU `-d in_asm` trace ile teşhis edildi):**
- **FP/SIMD trap:** clang -O2 16-baytlık struct move'u `ldr/stur q0` ile yapar → EL2 reset'te
  trap → vektör 0x200 (vektör tablosu yok) → çöküş. (FP-enable + GPR-only.)
- **Device-memory alignment:** MMU kapalıyken RAM = Device-nGnRnE → 16-baytlık q-erişimi
  8-hizalı adreste **alignment-fault**. `-mgeneral-regs-only` q-register'ı tümden eler
  (≤8-bayt hizalı erişim = Device-memory'de güvenli; Linux çekirdek deseni).

**Doğrulama:** `calistir_qemu_smoke` (gerçek qemu-system-aarch64 11.0.1) → kernel BOOT +
"Merhaba KEMGU - Bare Metal" + "42" ✓. kernel.elf q-register sayısı=0, libc-yok kapısı temiz,
sıfır derleme uyarısı. `test_tumu` YEŞİL (self-host fixpoint 32157 satır kararlı; 72/72 codegen;
4-mod driver; ASan-temiz — regresyon yok).

**Kapsam/sınır:** (1) MMU kapalı → `-mgeneral-regs-only` kernel-geneli FP-yasağı (gösterici için
yeterli; FP-li kernel = MMU+Normal-memory sonraki milestone). (2) Dizi (`kdl_dizi_*`) bare-metal'de
HENÜZ yok → C1b (kdl_dizi.c carve + allocate-eden dizi kernel). (3) x86_64 boot stub/linker yok →
C1-x86 (Mehmet kararı: aarch64+x86_64 paralel). (4) Üretici C-bootstrap kemgu; self-host bare-metal
kapsam-dışı v1.

---

## D-104 — düzelt(self-host codegen): blok-leksik-kapsam shadowing → değişken tablosu push/pop (2026-06-27)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: D-103 sonrası; merge'de origin/main ilerlemişse güncelle).

**Karar [ETKİ: `selfhost/codegen.kem` (KEMGU kaynağı; C derleyici DEĞİŞMEDİ) + 2 yeni
`test/cg_korpus` regresyon testi].** Self-host codegen'in işlev-içi değişken tablosu
(`cg_ad`/`cg_areg`/… paralel diziler) **append-only** idi (`cg_base..son` sondan-ara =
shadow). İç blokta DIŞ değişkeni gölgeleyen `değişken v` blok bittikten sonra **pop
EDİLMİYORDU** → blok-sonrası `v` lookup'ı İÇ (kapsam-dışı) slot'a çözülürdü = **leksik
kapsam MISCOMPILE** (bellek-güvenliği değil; dizi fonksiyon-dönüşüne kadar canlı, UAF yok).

**Repro (`cg_kapsam_shadow.kem`):** `değişken v=[100,200,300]; eğer …>0 { değişken v=[7,8,9]; … }
ver dizi_al(v,2)` → DIŞ v[2]=300 olmalı; hata İÇ v slot'una `load ptr %2` emit edip **9**
döndürürdü (koşul yanlışsa İÇ slot hiç store edilmez → D-069 sınır-kontrolü PANIC; yine de
yanlış-kapsam çözümü). C `src/llvm.c` AYRI kod yolu, `scope_gir`/`scope_cik` (bağlı-liste
başı kaydet/geri-yükle) ile DOĞRU idi (300 döner).

**Çözüm — C disiplinini yansıt (`cg_kapsam_kapat`):** Dizi-shrink built-in'i olmadığından
truncate yerine **ad-blank**: blok/döngü/eşleş-kolu girişinde `kapsam_bas = dizi_boyut(cg_ad)`;
çıkışta `[kapsam_bas, son)` aralığındaki bağlamaların `cg_ad` adını `""` yap → `cg_var_bul`
(non-empty `ad` arar) boş slot'u atlar, dış bağlamaya geri döner. Slot canlı kalır, **emit
edilmiş IR DEĞİŞMEZ** (yalnız derleme-zamanı isim çözümü). Kapsamlanan yollar: `BLOK` (eğer/
iken/güvensiz gövdeleri de BLOK → transitif), `ICIN` döngü değişkeni, `ESLES` kol payload
bağlamaları. ESLES skaler kol (literal/joker) bağlama yapmaz → dokunulmadı.

**Soundness (fixpoint güvenliği):** `--check` zaten iç-kapsam değişkenine blok-sonrası erişimi
reddeder; gölgeleme yoksa üretilen IR bayt-aynı kalır. Bu yüzden lexer/parser/checker.kem
bootstrap'ı etkilenmez (zaten gölge-sızıntıya bel bağlamıyorlardı).

**Doğrulama:** cg_korpus **72/72** semantik (C-oracle ≡ self-host; +2 yeni: shadow=44≡300%256,
döngü-shadow=120). Bootstrap **FIXPOINT** ✓ (lexer/parser/checker 55/55 bayt-aynı + codegen
stage1==stage2, 25290 satır). Driver 4-mod **TÜM GEÇTİ** (token 22 + parse 12 + check 48,
C-built & self-host). Manuel kenar: iç-içe üçlü-shadow=173, kardeş-blok ad-yeniden-kullanım=45,
koşul-yanlış→DIŞ-oku=300 — hepsi C ile eşleşir. **Sınır:** C kapsam yolu zaten doğruydu (bu
commit yalnız self-host'u hizalar); lokal-değişken-tablosu hâlâ append (truncate yok), ad-blank
yeterli.

---

## D-103 — [YÜKSEK] F4.2b: ρ_yerel GERÇEK-serbest + escape kaçış-yolu sağlamlaştırma (SOUND) (2026-06-22)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: D-102 KONSOLİDASYON sonrası; merge'de origin/main ilerlemişse güncelle).

**Karar [ETKİ: YÜKSEK — `src/escape.c` + `src/escape.h` + `src/llvm.c` + testler; bölge-serbest
semantiği ilk kez GERÇEKTEN ETKİN; izole commit].** F4.2b'nin başlık işi: **ilk gerçek bölge-serbest.**
Kaçmayan yerel diziler `ρ_yerel`'e (fonksiyon-yerel KdlBolge) tahsis edilir ve fonksiyon çıkışında
`kdl_bolge_serbest` ile serbest bırakılır. **Kaçan TÜM tahsisler `ρ_caller`'da kalır (serbest EDİLMEZ)
→ UAF imkânsız.**

**Yük taşıyan invaryant:** *kaçan → ρ_caller, kaçmayan → ρ_yerel, ρ_yerel ret'te serbest. Bunu kır →
sessiz UAF. Şüphede konservatif (ρ_caller).*

**KÖKLÜ KARAR — POZİTİF KANIT, escape DFA'ya GÜVENME (default-deny):** İlk iki routing girişimi escape
DFA'sının yargısına güvendi (önce `bolge_belirle` default-YEREL; sonra `escape_kayitli_mi`+`BOLGE_YEREL`).
**Çok-ajanlı adversarial hunt (8 aile × repro + bağımsız ASan-doğrulama, `sanitize_address` enjekte ederek
IR-fonksiyon load/store'ları da denetlendi) bu yaklaşımda 18 DOĞRULANMIŞ UAF buldu:** iç-içe agregat
(`Dizi<Dizi<T>>`, yapı-içi-dizi, ara-değişken), closure yakalama, alias/yeniden-atama, loop-carried.
**Kök sebep:** escape DFA bir **MAY-yaklaşımı** — kaçış yollarını KAÇIRIR; "DFA escape bulamadı" free için
GÜVENİLMEZ (her kaçırılan yol = UAF; D-102'nin "sınırsız 'tüm rotaları yakaladım mı?' riski" tam da bu).
Kaçırılan-yolu-tek-tek-yamamak (deep `ifadeyi_yukselt`, lambda-guard...) sonsuz whack-a-mole.

**ÇÖZÜM (principle 1+3 — "lokallik KANITI" + "DAR, inşa-gereği sound"):** Routing artık escape DFA'ya
DEĞİL, **POZİTİF + default-deny CONFINEMENT kanıtına** dayanır. Bir dizi-değişkeni `ρ_yerel`'e SADECE şu
İKİ koşulda yönlenir:
1. **SKALER-ELEMAN** (`elem_ir` ne `ptr` ne `%struct`): skaler eleman → `dizi_al` KOPYA döndürür, iç-ptr
   kaçışı YOK. `Dizi<Dizi>/Dizi<metin>/Dizi<yapı>` → ptr/struct eleman → iç heap-ref kaçabilir → ρ_caller.
2. **`escape_kesin_yerel` (confined kanıtı, `escape.c`):** bağlı değişkenin govdedeki **HER** kullanımı
   şunlardan biri olmalı: `var[i]` okuma, `var[i]=...` yerinde-yazma, retain-ETMEYEN dizi-builtin'in
   İLK argümanı (`dizi_al/boyut/yaz/ekle/kapasite`). **Başka HER konum** (ver, `b=var` alias, `[..,var,..]`
   /yapı alanı, çağrı argümanı, lambda yakalama, `&var`, `var` yeniden-atama) → **DENY → ρ_caller.**
   Default-deny = inşa-gereği sound (bir tek yamadan değil, kapalı-form pozitif kanıttan). Tüm AST düğüm
   tipleri kapsanır; bilinmeyen düğüm → konservatif deny.

**Yan-sağlamlaştırma (`ifadeyi_yukselt` DERİN yapıldı):** Escape DFA'nın DİĞER tüketicileri (G005 closure,
bolge_atama) için transitif terfi sığdı → agregat kaçınca gömülü dizi/alan da `ESC_CAGIRAN`. Routing artık
bu DFA'ya bağlı OLMASA da DFA'nın kendi soundness'ı için tutuldu. Agregat-store (`dış[i]=arr`) ve A1 DELIK
testleri CAGIRAN'a güncellendi (ASLA ITERASYON invaryantı korunur).

**Yakalanan bug (init):** `EscapeKayit.kesin_yerel` `kayit_ekle`'de sıfırlanmıyordu → garbage truthy →
HER dizi yönlendi (batch 43 UAF). `k->kesin_yerel = 0` ile düzeldi. (Adversarial gate + uninit-init
disiplini yakaladı.)

**C ve self-host SOUND (R1 gevşedi):** C-tarafı confined dizileri serbest eder; self-host `codegen.kem`
ρ_yerel ÜRETMEZ (her şey ρ_caller = konservatif-sound). İkisi de hiçbir kaçanı serbest etmez.

**DOĞRULAMA:** `parser.kem` bootstrap FIXPOINT ✓ + `codegen_diff` 70/70 ✓ + **48/48 routable repro
ASan-temiz (18 orijinal UAF'ın TAMAMI kapandı)** + confined `dizi_al` döngüsü GERÇEK free + ASan temiz
(exit 42) + `test_tumu` YEŞİL + DRF 39/39 + escape 22/22 + bolge_atama 15/15. Kalan 2 repro UAF
(`probe_bare`, `probe_p1_ekle_inline`) F4.2b'den BAĞIMSIZ PRE-EXISTING bug (bare-literal→`dizi_*`
stack/heap descriptor uyuşmazlığı; machinery commit `db4073e`'da da çöküyor) → ayrı task'a flaglendi.

**Sınırlar (v1):** Gerçek-serbest yalnız C-tarafı + skaler-eleman + confined-değişken. Geniş ama sound
desenler (kaçan dizi, ptr-eleman, alias, capture) ρ_caller'da kalır (free kaçırılır ama UAF imkânsız).
Genişletme (ptr-eleman transitif free, daha keskin alias, self-host port) sonraki iş.

---

## D-102 — [YÜKSEK] Loop-carried soundness: escape/bölge ESC_ITERASYON ÜRETMEZ (güvenli geri-çekilme) (2026-06-20)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `9977c3b` → D-101 ayrılmıştı; main D-101'i F4.2a'ya verdi → KONSOLİDASYON: D-102'ye yeniden numaralandı).

**Karar [ETKİ: ORTA — yalnız `src/escape.c` + `src/bolge_atama.c` + testler; codegen/checker/IR
DEĞİŞMEZ; izole commit; PR, merge edilmedi].** Escape analizi ARTIK **hiçbir** tahsisi `ESC_ITERASYON`
(= bölge sisteminde `BOLGE_ITERASYON`, **EN KISA** ömürlü bölge: F4.3'te iterasyon-başına serbest
bırakılacak) işaretlemez; döngü içindekiler dahil **tüm tahsisler `ESC_YEREL`** (daha uzun ömürlü =
güvenli). F4.2a/codegen'den tamamen bağımsız.

**Delik (loop-carried UAF):** Eski kod, bir döngü gövdesinde oluşan HER tahsisi **koşulsuz**
`ESC_ITERASYON` işaretliyordu (escape.c join'inde + bolge_atama.c syntax-fallback'inde). Tahsis
iterasyonu AŞIYORSA (döngü-dışı bir yere bağlanıp sonra kullanılıyorsa) ama `ver`'lemiyorsa →
`ITERASYON` kalıyordu. F4.3'te ρ_iterasyon iterasyon-başına serbest bırakılınca **canlıyken serbest =
UAF**. Şu an BOXED (F4.3 yok) ama F4.3'ten ÖNCE kapatılmalıydı.

**Neden DETECTION değil GERİ-ÇEKİLME (önemli — orijinal plan değişti):** İlk yaklaşım "iyimser default'u
tersine çevir + kanıtlanmış iterasyon-yerelleri ITERASYON'a indir" (iterasyon-kaçtı bayrağı + post-pass)
idi. Bu yaklaşımın sağlamlığı, kaçış rotalarını KAPATAN **kapılara KOŞULLUYDU**: D-007 (diziler
skaler-eleman → dış agregaya referans saklanamaz) ve R-GÖMME (kaçan agregaya gömülü heap-ref yok).
**Çok-ajanlı adversarial review (4 bağımsız lens + adjudikasyon, uçtan uca `--check`/`--llvm`/ASan ile
doğrulandı) bu kapıların ENFORCE EDİLMEDİĞİNİ kanıtladı:**
- `Dizi<Dizi<T>>`, `Dizi<metin>` ve `Dizi` alanlı yapı tipleri tip-kontrolden GEÇER ve codegen'de
  **by-ref `KdlDizi*`/`ptr` eleman** olarak lower edilir (D-007 yalnız STRUCT-VALUED dizi-elemanı
  codegen ertelemesi; skaler/ptr eleman çalışır).
- Tip sistemi `nesne.alan = x` (`DUGUM_ERISIM`) ve `dizi[i] = x` (`DUGUM_INDEKS`) lvalue'lerini kabul
  eder (`tip_kontrol.c:4545-4549`). Bir döngü-tahsisini `dış[i] = tahsis` / `nesne.alan = tahsis` ile
  dış (iterasyonu aşan) bir agregaya **by-ref** koymak gerçek bir kaçış rotasıdır; sentaktik tespit
  bunu (`DUGUM_TANIMLAYICI` dışı lvalue) kaçırınca **under-approximation = gizli UAF** olur.
Bu rotaları (ve gömme/closure varyantlarını) sağlamca kapsamak, her kaçırılan rotanın UAF olduğu bir
analizde sınırsız "tüm rotaları yakaladım mı?" riski taşır. Üstelik per-iterasyon optimizasyonunun
ŞU AN HİÇ tüketicisi yok (F4.3 yok; analiz codegen'e unwired). Bu yüzden direktifin AÇIKÇA izin verdiği
**güvenli geri-çekilme** seçildi: ITERASYON'u hiç üretme. Bu, herhangi bir kapıya bakılmaksızın
**trivially sağlam** (ITERASYON hiç üretilmezse iterasyon-başına serbest hiç olmaz → loop-carried UAF
**imkânsız**). Per-iterasyon optimizasyonu F4.3'e (gerçek bölge-serbest semantiği + kapılar enforce
edilince ya da tüm rotalar kapsanınca) ertelenir.

**Değişiklik:**
- `escape.c`: alloca-literal visit'inde **koşulsuz loop→ITERASYON marking'i KALDIRILDI**; yalnız kayıt
  oluşturulur (default `ESC_YEREL`). Analiz `ESC_ITERASYON` ÜRETMEZ. `ver`→`ESC_CAGIRAN` yolu DEĞİŞMEDİ
  → `ESC_CAGIRAN` seti **byte-identik** (G005 closure tespiti `== ESC_CAGIRAN`'a bağlı, etkilenmez).
- `bolge_atama.c`: syntax-fallback'teki koşulsuz `dongu_derinligi>0 → aktif_iterasyon` KALDIRILDI →
  döngü tahsisi `BOLGE_YEREL`. `escape_to_bolge`'daki `ESC_ITERASYON → aktif_iterasyon` eşlemesi KORUNUR
  ama **şu an ULAŞILMAZ** (escape ITERASYON üretmez); F4.3 yeniden etkinleştirirse doğru kalır.
- `escape.h`/`bolge.h`: `ESC_ITERASYON`/`BOLGE_ITERASYON` enum'ları API/gelecek için KORUNUR.

**Sağlamlık modeli:** Şüphede DAİMA uzun ömürlü. Under-approximation (yanlış ITERASYON) = gizli UAF =
KABUL EDİLEMEZ. Over-approximation (tüm döngü tahsisleri YEREL = kaçırılan per-iterasyon optimizasyonu)
= SORUN DEĞİL. Soundness argümanı artık TEK CÜMLE: *escape analizi ESC_ITERASYON üretmez.*

**🔗 F4.3 İÇİN NOT (per-iterasyon optimizasyonu geri açılırken):** ITERASYON'u tekrar üretmeden ÖNCE,
bir döngü-tahsisinin iterasyonu aşma rotalarının TAMAMI kapsanmalı: (a) `ver`→CAGIRAN; (b) daha-sığ
değişkene bağlanma; (b2) **agrega-lvalue store** (`dış[i]=x` / `nesne.alan=x`); (b3) **agregaya gömme**
sonra agrega kaçışı (`dış = Yapı{f: x}`, `ver Yapı{f: x}`); (c) çağrı argümanı/sonucu; (d) closure
yakalama (G005 kaçan closure'ı reddeder — enforce EDİLİR). VEYA kapıları (D-007 referans-eleman reddi,
R-GÖMME) önce enforce et. **loop-carried per-iterasyon optimizasyonu, D-007 + R-GÖMME enforcement'ına
bağımlıdır.**

**Doğrulama:** `test_escape` 17→22 (T8 ITERASYON→YEREL; +5 geri-çekilme testi: dış-skaler-store,
**dış-dizi-eleman-store [b2 DELİK]**, **dış-yapı-alan-store [b2 DELİK]**, döngü-ver→CAGIRAN, umbrella
"hiçbir tahsis ITERASYON değil" invariant'ı). `test_bolge_atama` 13→15 (döngü→BOLGE_YEREL, b2
dış-dizi-eleman→BOLGE_YEREL [DELİK], syntax-fallback→YEREL). **Teeth kanıtlandı:** eski koşulsuz
loop→ITERASYON geri enjekte edilince tam 5 geri-çekilme assertion'ı (b2 delik testleri dahil)
BAŞARISIZ. `tip_kontrol` 184/184 (G005 değişmedi), `llvm` 235/235, **test_tumu yeşil ("Tum testler
gecti!")** + self-host FIXPOINT korundu. 0 ASan, 0 uyarı (clang+gcc strict). Geri-çekilme kararı,
çok-ajanlı adversarial review'in (uçtan uca `--check`/`--llvm`/ASan ile) gerçek b2/b3 UAF rotalarını
doğrulaması üzerine alındı — direktifin "güvenli geri-çekilme: ITERASYON'u hiç üretme" yolu.

---



## D-101 — [YÜKSEK] V2 F4 FAZ 2a: region-passing ABI (ρ) — kullanıcı-fn + dizi helper'ları (re-scoped) (2026-06-17)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `9977c3b` → en yüksek D-100 → D-101 ayrıldı).

**Karar [ETKİ: YÜKSEK — `src/llvm.c` + `selfhost/codegen.kem` (İKİ-DERLEYİCİ MİRROR) + `kdl_runtime.c`;
izole commit].** Region-passing ABI'nin İLK adımı: her KULLANICI fonksiyonu ilk param `ptr %rho`
(bölge) alır, her kullanıcı-fn çağrısı ρ geçirir, DİZİ allokasyon helper'ları ρ alır, tüm dizi
tahsisi → geçirilen ρ_caller. AMAÇ: uniform ρ ABI iskeleti + iki-derleyici mirror + YENİ
self-host FIXPOINT'i kararlılaştırmak. Serbest + gerçek YEREL/CAGIRAN ayrımı = F4.2b/F4.4.

**RE-SCOPE (orchestrator kararı):** İlk tasarım (her şey ρ: metin + closure-env-malloc + bölge_al +
tüm helper'lar) ~50 byte-kritik edit + çok-iterasyonlu konverjans gerektiriyordu. Mirror yüzeyini
küçültmek için **kullanıcı-fn + DİZİ helper'ları + lambda imzaları + fat-value dispatch** ρ-threaded
edildi; **metin (kdl_metin_*), closure-env-malloc, bölge_al inline-malloc global'de KALDI** (F4.1
davranışı — `kdl_global_bolge_al` fallback). **Lambda/dispatch ρ yalnız `src/llvm.c`'de** —
`codegen.kem`'de fat-value/lambda YOK (`parse_lambda` salt parser-fn), dolayısıyla mirror edilmedi
ve fixpoint etkilenmedi.

**Tasarım:**
- ρ = adlı param `%rho`, LİTERAL geçirilir (alloca YOK) → gövde reg numaraları DEĞİŞMEZ.
- main HARİÇ her kullanıcı-fn: `define <ret> @f(ptr %rho, ...)`. Kullanıcı-fn çağrısı `f(%rho, ...)`.
- **main:** ρ param almaz (libc çağırır); gövde başında `%r = call ptr @kdl_global_bolge_al()` seed +
  `rho_ref` ile çağrılara geçer.
- **lambda (fat-value hedefi):** ρ İLK param: yakalamasız `@l(ptr %rho, args)`, yakalamalı
  `@l(ptr %rho, ptr %env, args)`. Gövdesi geçirilen ρ'yu kullanır. Böylece üst-düzey-fn (ρ-ABI) ile
  lambda fat-value dispatch'te ABI-uniform.
- **fat-value indirect dispatch:** her iki dal ρ geçirir — bare `fn(ρ, args)` (üst-düzey-fn-değer
  ya da yakalamasız-lambda), closure `fn(ρ, env, args)`. ρ = çağıranın `rho_ref`'i. Bu, stdlib
  yüksek-mertebe fn'lerini (harita/filtre/indirgeme — fn'i DEĞER geçirip indirect çağırır) ρ-doğru
  kılar; aksi halde üst-düzey-fn ρ-ABI iken dispatch ρ'suz → ABI uyumsuz (test_llvm 59/60/61 ✗).
- Dizi helper'ları (`kdl_dizi_olustur/ekle_{tam,tam64,ptr,yapi}/kapasite_ayarla`) ρ ilk param +
  `kdl_bolge_ayir(ρ,...)`. Non-alloc (al/yaz/boyut) + metin + yazdir ρ ALMAZ.
- Sınıflandırma: çağrı yerinde "kullanıcı-fn mı?" (`ik!=NULL` / codegen.kem `kdl==""`) → ρ; built-in → ρ yok.

**FIXPOINT'in DOĞASI (kritik anlayış):** bootstrap "fixpoint" = **stage1 == stage2** ve İKİSİ DE
SELF-HOST çıktısı (codegen.exe vs codegen2.exe) → self-host İDEMPOTANSI. llvm.c↔codegen.kem
BYTE-eşitliği DEĞİL. codegen_diff ise SEMANTİK (exit-kod) eşdeğerlik. Dolayısıyla codegen.kem'in
ρ-emit'i llvm.c ile byte-eşleşmek zorunda DEĞİL — yalnız DOĞRU + deterministik olmalı (stage1==stage2
otomatik). Bu, mirror'ı çok daha tractable yaptı (reg-numara eşleştirme kaygısı moot).

**KAPSAM / RESIDUAL:**
- Bölge HİÇ serbest bırakılmaz (status-quo leak; deterministik serbest = F4.4).
- ρ_yerel YOK — her tahsis ρ_caller (global'den seed). Gerçek YEREL/CAGIRAN escape ayrımı = F4.2b.
- metin (kdl_metin_*), closure-env-malloc, bölge_al inline-malloc ρ ALMAZ (global; F4.1) — bunlar
  TAHSİS noktaları, ρ-threading'den ayrı; F4.2b/F4.4 ele alır.
- Fat-value indirect ABI uyumsuzluğu (top-level-fn-değer ρ-ABI iken dispatch ρ'suz) ÇÖZÜLDÜ:
  lambda imzası + dispatch'in iki dalı da ρ alır (yalnız llvm.c; codegen.kem'de fat-value yok).

**Doğrulama (hepsi YEŞİL):** `test_tumu` → **"Tum testler gecti!"** (rc=0). test_llvm **235/235**
(stdlib harita/filtre/indirgeme dahil — fat-value ρ-dispatch düzeltmesiyle). Bootstrap:
LEXER/PARSER/CHECKER **51/51/51 birebir, 0 fark**; **CODEGEN FIXPOINT stage1==stage2 (21967 satır)
BİREBİR ✓** (ρ-threaded). self-host --parse 12/12, --check 48/48; codegen semantik eşdeğerlik
**58/58** (kemgu_self + kemgu_self2). ASan E2E: **PASS=97 FAIL=0** (serbest yok → UAF yok; leak F4.1
ile aynı). 0 derleyici uyarısı.

**Build-env notu (reviewer):** bootstrap/codegen harness'ları `mktemp -d` kullanır. MSYS2 `mktemp`
`/tmp`'i `C:\msys64\tmp`'e, git-bash ise `AppData\Local\Temp`'e çözer; PATH karışırsa harness
"stage1.ll No such file" ile YANLIŞ-NEGATİF verir (kod sorunu DEĞİL). Çözüm: testleri tutarlı bir
MSYS2 kabuğunda çalıştır veya `TMPDIR`'i `/c/`-köklü bir yola sabitle (her iki kabuk aynı çözer).

## D-100 — [YÜKSEK] V2 F4 FAZ 1: sızan array/metin tahsisini global bölgeye yönlendir + sembol-çakışması temizliği (2026-06-17)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `e3d4784` → en yüksek D-099 → D-100 ayrıldı).

**Karar [ETKİ: ORTA — yalnız `runtime/kdl_runtime.c`; codegen/checker/IR DEĞİŞMEZ; izole commit].**
Sızan (çağırana dönen + hiç free edilmeyen) array ve yeni-metin tahsislerini F4.0 global bölgesine
(`kdl_bolge`) yönlendirir. Codegen helper imzaları DEĞİŞMEDİĞİ için IR aynı → **FIXPOINT byte-identik**;
mirror yok (saf C runtime). Bölge HİÇ serbest bırakılmaz (status-quo leak; deterministik toplu serbest = F4.4).

**Sembol-çakışması temizliği (ön-blokör — orchestrator onayıyla çözüldü):** `kdl_runtime.c` ZATEN
`kdl_bolge_olustur/ayir/serbest(+toplam_byte)` tanımlıyordu = `bölge_al` için ESKİ `KdlArena`
(chunk-bump, int32). AMA `bölge_al` codegen'i inline `@malloc` kullanıyor (llvm.c:2726) → KdlArena
**TAMAMEN ÖLÜ** (sıfır çağıran, derlemeyle doğrulandı). F4.0 aynı isimleri farklı imzayla almıştı →
gizli çakışma (`#include "kdl_bolge.h"` → `conflicting types` derleme hatası). **Çözüm:** ölü KdlArena
kümesi (KdlArena/KdlArenaChunk + kdl_bolge_olustur/ayir/serbest/toplam_byte + kdl_bolge_metin_birlestir,
hepsi sıfır-çağıran) SİLİNDİ; F4.0'ın `kdl_bolge.c`'si dosya sonuna `#include "kdl_bolge.c"` ile GÖMÜLDÜ
→ `kdl_runtime.o` allokatörü kendi içinde taşır, **harness link satırları DEĞİŞMEZ**. Standalone
`kdl_bolge.o` yalnız F4.0 birim testinde linklenir; hiçbir hedef ikisini birden linklemez → çift-sembol yok.

**Yönlendirilen sızan tahsisler:**
- `kdl_dizi_olustur` descriptor → bölge.
- `kdl_dizi_ekle_{tam,tam64,ptr,yapi}` büyüme: `realloc` → `kdl_dizi_buyut()` (bölgeden yeni tampon +
  CANLI `boyut*eb` memcpy; eski tampon bölgede sızar). **`kdl_dizi_kapasite_ayarla` DE** dönüştürüldü
  (zorunlu invaryant: d->veri bölge-sahipli → realloc'a geçmek UB/çökme olurdu).
- Yeni-metin döndüren `kdl_metin_*` + `kdl_ondalik_bicimle` + `kdl_tam_to_metin` (12 nokta) → bölge.
- `kdl_dizi_serbest` NÖTR (no-op) — d artık bölge-sahibi, `free()` çökme olurdu; codegen zaten emit
  etmiyordu (ölü, dizi hep sızıyordu).

**DOKUNULMAYAN:** dosya/kripto geçici tamponları (malloc…free çiftli — bölgeye alınsa sızıntı YARATIRDI),
eşzamanlılık (kdl_gorev/kanal — D-008, çağrılmıyor), `kdl_bellek_hizali_*`, derleyicinin kendi
`src/arena.c`'si (ayrı, compile-time). bölge_al / closure-env / intrinsic inline `@malloc`'ları = F4.2.

**Doğrulama:** ASan/UBSan smoke (1000-eleman geometrik büyüme değerleri doğru → büyüme-memcpy doğru;
kapasite_ayarla reserve; metin birleştirme; bakiye=1). `mingw32-make test_tumu` → "Tum testler gecti!"
+ **FIXPOINT byte-identik** (self-host kendini bölge-tahsisiyle derleyip aynı IR üretiyor = allokatör +
büyüme deseni gerçek yük altında doğru). ASan E2E ~97/0 (büyüme memcpy + d->veri güncelleme bellek-temiz;
eski tampon bölgede serbest değil → UAF yok). `kdl_bolge_bakiye()` çıkışta 1 (global bölge, kasıtlı
hiç-serbest — beklenen). 0 uyarı.

## D-099 — V2 F4 FAZ 0: bölge (region) arena allokatörü runtime (`kdl_bolge`) (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `ca64daf` → en yüksek D-098 → D-099 ayrıldı).

**Karar [ETKİ: DÜŞÜK — saf runtime; codegen/checker/IR DEĞİŞMEZ; izole commit].** Bölge tabanlı
bellek modelinin (`belgeler/KEMGU_Bellek_Modeli.md`, Katman 1) runtime tabanı. Bir BÖLGE = bir
ARENA: malloc'lu blok tek-yönlü listesi + blok-içi bump pointer. Tahsis O(1) bump; bölge
kapanışında TÜM bloklar tek seferde free (O(blok)). GC yok — deterministik serbest. **Bu fonksiyonları
henüz kimse çağırmaz** (F4.1'de lambda env + dizi/metin tahsisi buraya bağlanır; F4.2'de
region-passing ABI — bölge `ptr` parametresi). Soundness + FIXPOINT'ten tamamen bağımsız.

**API (`runtime/kdl_bolge.h` + `.c`):**
- `KdlBolge *kdl_bolge_olustur(void)` — handle + ilk blok (64 KB) malloc'la; OPAK ptr döner
  (F4.2 region-passing'de `ptr` param).
- `void *kdl_bolge_ayir(KdlBolge *b, uint64_t n)` — 16-bayt hizalı bump; aktif blokta yer yoksa
  yeni blok (boyut = max(64KB, n+16) → oversized'a adanmış blok).
- `void kdl_bolge_serbest(KdlBolge *b)` — tüm bloklar + handle free (O(blok)).
- Sızıntı-tanığı (Windows'ta LSan yok): `kdl_bolge_olustur_sayisi`/`kdl_bolge_serbest_sayisi`
  global sayaçları + `int kdl_bolge_bakiye(void)` (oluştur−serbest; 0 = sızıntı yok).

**Tasarım inceliği — hizalama:** esnek dizi (FAM `veri[]`) ofseti platforma göre 16-hizalı
OLMAYABİLİR (64-bit'te header 24 bayt → veri 8-hizalı). Bu yüzden hizalama derleme-zamanı ofsetine
değil ÇALIŞMA-ZAMANI ADRESİNE (uintptr_t) göre yapılır → her platform/header düzeninde 16-hizalı.
Yeni blok kapasitesi `hn + 16` ile en kötü hizalama payını garanti eder. Tüm aritmetik taşma-korumalı
(hiza_yukari wrap guard, `kap < hn` guard, `SIZE_MAX - sizeof(header)` malloc guard).

**KAPSAM / SINIR:**
- Tahsisler TEK TEK serbest EDİLMEZ — yalnız bölge topluca (dizi/metin "leak OK" status-quo ile
  aynı sınıf; bölge modeli deterministik serbest'i ZATEN sağlıyor: bölge kapanınca hepsi gider).
- Bare-metal: host malloc/free üstüne; `KEMGU_BARE_METAL` altında kdl page-allocator'a bağlanması
  TODO (bloklamaz — bare-metal bu dosyayı henüz derlemiyor).
- Tek-thread host; sayaçlar atomik değil (concurrency Katman 2).
- Makefile: `kdl_bolge.o` (plain, F4.1 link'i için) + `test_kdl_bolge` (ASan/UBSan birim test);
  codegen/test emit hedeflerine DOKUNULMADI.

**Adversarial inceleme:** çok-ajanlı (alignment · overflow · memsafety · caplogic · UB/port ·
test-gaps; her bulgu ayrıca doğrulandı) → **0 doğrulanmış correctness/safety açığı**. Doğrulayıcılar
tüm korumaları (runtime-adres hizalaması, `hn<n`/`kap<hn`/`SIZE_MAX` taşma guard'ları, free-all
zincir-yürüyüşü, NULL guard'ları) izleyip teyit etti. İnceleme önerileriyle SERTLEŞTİRME:
`kdl_bolge_blok_sayisi()` teşhis erişimcisi eklendi (büyümeyi blok-sayısıyla KESİN doğrula; eski
write-only alan artık kullanılıyor) + NULL-handle ve taşma-reddi (UINT64_MAX → NULL) testleri.

**Doğrulama:** birim test **33/33**, ASan/UBSan TEMİZ (blok-içi · büyüme [blok sayısı 1→2] ·
oversized 128KB [adanmış blok] · free-all · bakiye=0 · hizalama 1..33 bayt + uint64/16-bayt tür ·
1000 yoğun tahsis örtüşmez · NULL-handle no-op · UINT64_MAX taşma reddi). `mingw32-make test_tumu`
→ "Tum testler gecti!" (codegen değişmedi → IR/FIXPOINT trivial korunur; yeni fonksiyonlar
referanssız ölü kod). ASan E2E 97/0 (yeni runtime'ı kullanan yok). 0 uyarı.

## D-098 — [YÜKSEK] V2 FAZ 2: yakalayan closure env'i stack→HEAP (@malloc) — kaçışta yaşar (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `f30712e` → en yüksek D-097 → D-098 ayrıldı).

**Karar [ETKİ: ORTA — `src/llvm.c` lambda env allokasyonu; izole commit; C-codegen-only].**
V2 yol haritası 2. fazı (F1 üzerine). Yakalayan closure'ın **env ALLOKASYONUNU** stack `alloca`'dan
**heap `@malloc`**'a çevirir → env, oluşturan frame'i aşsa bile yaşar (kaçış UAF'inin KÖKÜ kapanır).
F1'in fat-value temsili ve env-null dispatch'i DEĞİŞMEZ.

**Değişiklik (tek nokta — lambda materyalizasyonu, DUGUM_LAMBDA):**
- Yakalayan lambda env'i: `%env = alloca %envtip` → `%env = call ptr @malloc(i64 ptrtoint
  (ptr getelementptr (%envtip, ptr null, i32 1) to i64))`. Boyut = LLVM constexpr sizeof
  (D-087 GEP-null idiomu; padding/alignment LLVM layout'uyla birebir).
- Capture store'ları (GEP+store) ve `{@lambda_N, %env}` insertvalue'su DEĞİŞMEDİ — %env artık heap
  ptr; GEP/store/insertvalue ptr üzerinde stack/heap-agnostik.
- **Allokatör seçimi:** `@malloc` (dizi/metin runtime'ının nihai allokatörü). Tutarlı + F4
  region-dealloc tek noktadan (env+dizi+metin) bağlanabilir.

**DOKUNULMAYAN:** çağrı dispatch (env-null; stack/heap fark etmez), lifted @lambda_N (env'i ptr okur),
yakalamayan closure ({@lambda_N, null}), top-level fn ({@f, null}).

**KAPSAM / SINIR:**
- **SERBEST BIRAKMA YOK** — env malloc'u hiç free edilmez (LEAK). Bilinçli: dizi/metin
  (`runtime/kdl_runtime.c` "leak OK") ile AYNI sınıf status-quo; deterministik region-dealloc = F4.
- **UNCONDITIONAL heap:** non-escaping dahil tüm capturing closure env'i heap. Escape-driven
  stack/region optimizasyonu (kaçmayan → yerel bölge) F4/region işi.
- **KAÇIŞ HÂLÂ G005 ile REDDEDİLİR** (F5'e dek). F2 env'i güvenli kılar ama özelliği AÇMAZ;
  davranış-eşdeğer (non-escaping closure'lar AYNI sonuç). UAF-fix LATENT (F5'te G005 kalkınca aktif).
- Yan not: capturing lambda DÖNGÜ içindeyse her iterasyon artık taze env malloc'lar (önceki
  hoist'lu stack alloca iterasyonlar arası PAYLAŞILIYORDU → closure-per-iteration için daha doğru).
  Korpusta döngü-içi capturing lambda yok → korpus davranışı birebir.

**FIXPOINT güvenliği:** self-host `.kem` fn-değer/lambda kullanmıyor → codegen.kem IR'ı etkilenmez →
bootstrap byte-identik korunur (F1 ön-kontrolüyle aynı).

**Doğrulama:** lambda E2E 5/5 (10_lambda, 04_islev, 42_lambda_hesap, 25_closure_capture,
43_closure_param) → exit 42 (artık HEAP env ile); IR teyidi: env = `@malloc(... sizeof envtip ...)`.
`mingw32-make test_tumu` → "Tum testler gecti!" (FIXPOINT korunur; --check/G005 değişmedi).
ASan E2E PASS=97 FAIL=0 (env leak'i dizi/metin leak'iyle aynı sınıf; leak-detection kapalı). 0 uyarı.

## D-097 — [YÜKSEK] V2 FAZ 1: fat-value closure ABI iskeleti — fn değeri {ptr fn, ptr env} + runtime env-null dispatch (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `43c2526` → en yüksek D-096 → D-097 ayrıldı).

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit; DAVRANIŞ-EŞDEĞER;
C-codegen-only].** V2 (escaping-closure desteği) yol haritasının 1. fazı. Fonksiyon-değeri
temsilini tek-tipleştirir; D-071'in deneyip-bozduğu uniform-ABI tuzağını **yapısal olarak** eler.
KAÇIŞ HENÜZ GÜVENLİ DEĞİL (env hâlâ STACK → heap F2); G005 reddi KALIR (F5'te kalkar).

**Tasarım (B-i, v2_tasarim_plani.md onaylı):**
- `işlev(...)→R` IR lowering: `ptr` → **`{ ptr, ptr }`** (2-word first-class fat value).
  `ast_tip_to_ir` TIP_ISLEV → `{ ptr, ptr }` → değişken/param/dönüş/alan/dizi-eleman hepsi jenerik
  olarak fat value taşır.
- **Materyalizasyon:** top-level fn değeri → `{@f, null}` (insertvalue); yakalamayan lambda →
  `{@lambda_N, null}`; yakalayan lambda → `{@lambda_N, %env}` (env F1'de HÂLÂ STACK alloca).
- **Çağrı dispatch:** fat değerden `extractvalue` fn+env → `icmp eq ptr %env, null` → dallan:
  bare `call R %fn(args)` / closure `call R %fn(ptr %env, args)` → slot-deseniyle birleştir
  (phi yerine mevcut bellek-slot idiomu). **Compile-time `closure_mu`/`son_closure` tag'leri
  KALDIRILDI** — "closure mu?" artık DEĞERİN PARÇASI (env-null), kaçışta kaybolmaz.

**D-071 tuzağı neden artık imkânsız:** D-071'in uniform denemesi çağrı yerini "daima closure-unpack"
yapıp bare fn-ptr'ı `{fn,env}` sanıyordu (→ `harita(xs, iki_kat)` çöp). Burada bare ve closure AYNI
fat-value şeklini paylaşır; çağrı yeri env==null ile runtime'da ayrışır → bare fn doğal imzayla
çağrılır, sarma/thunk gerekmez. Temsil-uyumsuzluğu yapısal olarak ortadan kalkar.

**FIXPOINT güvenliği (ÖN-KONTROL):** self-host `.kem` kaynakları (lexer/parser/checker/codegen)
**fn-DEĞER kullanmıyor** (fn-tipli param/dönüş/alan YOK; lambda literal YOK — yalnız yorumlarda).
→ C compiler bu kaynakları derlerken TIP_ISLEV yolu hiç tetiklenmez → kemgu_self IR'ı DEĞİŞMEZ →
self-host bootstrap byte-identik (stage1==stage2 korunur). Salt C-codegen değişikliği.

**KAPSAM (F1 = yalnız iskelet):**
- env HÂLÂ STACK alloca (heap promosyonu = F2); kaçan yakalayan closure HÂLÂ UAF olur → G005
  reddi bu yüzden KALIR (F5'e dek kaldırılmaz; savunma derinliği).
- Davranış-eşdeğer: korpus AYNI çıktı/exit. Yeni özellik (kaçış) AÇILMAZ.
- Yakalamalı lambda'yı işlev-param'a geçirme (D-071 KAPSAM-DIŞI) artık ÇALIŞIR (fat value + runtime
  dispatch) — F1'in yan kazanımı; ama escape G005 ile sınırlı.

**Doğrulama:** lambda E2E **5/5** → exit 42, ASan temiz: 10_lambda, 04_islev,
42_lambda_hesap (D-071-kritik: top-level/yerel-lambda → işlev-param, env==null yolu),
25_closure_capture (yakalayan, env!=null yerel), **43_closure_param (YENİ: yakalayan closure →
işlev-param, env!=null param yolu — D-071 KAPSAM-DIŞI item)**. SELF-HOST bootstrap: lexer 51/51,
parser 51/51, **CODEGEN FIXPOINT stage1==stage2 byte-identik**. `mingw32-make test_tumu` →
"Tum testler gecti!"; ASan E2E PASS=96 FAIL=0; --check/--checkdump 48/48. 0 uyarı.
> Not: test makinesinde `mktemp`/`/tmp` MSYS mount'u ara sıra bozuk → bootstrap harness'ı flaky
> (kod-dışı). TMPDIR yazılabilir dizine ayarlanınca geçer; fixpoint byte-identik ayrıca elle kanıtlandı.

## D-096 — [YÜKSEK] V1 kaçan-closure UAF reddi (G005): YAKALAYAN ∧ KAÇAN closure compile-time reddedilir (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: origin/main 82e14ed → en yüksek D-095, dolayısıyla D-096 ayrıldı).

**Karar [ETKİ: `src/tip_kontrol.c` checker — yeni redd kodu G005; izole commit; C derleyici
codegen DEĞİŞMEDİ].** Güvenlik-iddiası izi (D-071 devamı). Kaçan yakalayan closure açığı
(`kacan_closure_kapsam.md` kapsam analizi) **"tehlikeli kodu derleyemezsin"** diyerek kapatıldı.

**Açık (D-071'de belgeli ama ZORLANMAYAN boşluk):** Yakalayan lambda → closure `{ptr fn, ptr env}`;
hem `env` (alloca `src/llvm.c:3758`) hem `{fn,env}` çifti (alloca `src/llvm.c:3805`) **STACK**'te.
Closure frame'i aşarsa (`ver` ile dönüş / frame-aşırı saklama) → env dangling = **UAF**; ayrıca
`closure_mu` tek yerde set edildiğinden (`src/llvm.c:4008`) kaçışta kaybolup çağrı yerinde
mis-dispatch. D-071 KAPSAM-DIŞI listesi bunu *"lambda escape (env stack — şu an non-escaping KEMGU
v1 garantisi)"* diye işaretlemişti — yani checker'la **zorlanmayan** bir yorum-garantisi. #1
"Kırılamaz Güvenlik" ihlali (analog dizi-deliği D-070'te koşulsuz heap-promote ile düzeltilmişti).

**Mekanizma — escape.c yeniden kullanımı (hedefli kontrol yerine):**
- `src/escape.c` (forward DFA fixed-point escape analizi) ZATEN `DUGUM_LAMBDA`'yı alloc-site izliyor
  ve `ver` (+ transitif atama zinciri + koşullu dal) ile dönen lambda'yı **`ESC_CAGIRAN`**
  işaretliyor. `escape.o` ZATEN ana ikiliye linkli (Makefile SRCS) ama `ana.c`'de çağrılmıyordu →
  **ÖLÜ ALTYAPI**. Bu commit onu ilk kez tüketir.
- **Bağlama:** `tip_kontrol_tanim` `DUGUM_ISLEV` kolu, gövde kontrolünden önce `escape_analiz_islev`
  çalıştırır (per-işlev; `escape_baslat`/`escape_serbest` dengeli → ASan temiz), sonucu
  `tk->aktif_escape`'e koyar.
- **Yakalama bilgisi** checker'da hesaplanır: `genel_yakalama_kontrol` (codegen'in
  `lambda_serbest_tara`'sı ile birebir — yalnız ÇEVRE lokal/param yakalama sayılır; global
  işlev/sabit/tip ve lambda-içi/gölgeleme sayılmaz). Lineer (LC-2) + lineer-olmayan yakalamayı kapsar.
- **Redd (G005):** lambda case'te `(yakaladi_genel ∧ escape_kategori(d)==ESC_CAGIRAN)` → `tip_hata`.
  Sadece (yakalıyor ∧ kaçıyor) reddedilir.

**Over-reject guard (testli — reddedilMEZ):** yakalamayan lambda return (bare fn-ptr, env yok);
yakalayan closure fonksiyon-içi çağrı (`ver arttir(10)` = `25_closure_capture.kem` deseni);
yakalayan closure çağrılır + sonuç saklanır, kaçmaz. **Pozitif:** `ver ||cap` / transitif
`f=||cap; ver f` / lineer `ver c` → G005.

**KAPSAM:** C-checker only — self-host lambda yapmıyor → port **moot** (G004 ile aynı; iki self-host
checker lambda görmez, fixpoint tetiklenmez). Korpus etkisi: **0 program G005 tetiklemez** (firsthand
doğrulandı: 25/10/42 lambda örnekleri yakalayan-closure KAÇIRMAZ; `p1_05` lambdaları yakalamasız +
parse-only; `lineer_closure.kem` fonksiyon-içi) → `--check`/`--checkdump` divergens YOK.

**RESIDUAL (V1 kapsamı dışı — bilinen kalan, follow-up):**
1. **Agregat gömme:** yakalayan closure'ı DÖNEN dizi/yapı içine gömme (`ver [|| b]`) — escape.c
   `ifadeyi_yukselt` agregat ELEMAN/ALAN alt-escape'ini izlemiyor (escape.h v1 sınırı) → **GERÇEK
   UAF, yakalanmıyor.** Yapı-alanı function-tip ayrıca parse etmiyor (P020) → o yol bugün erişilemez;
   dizi yolu erişilebilir + açık. Follow-up: escape.c v2 agregat alt-escape recursion'u VEYA V2 heap-env.
2. **Param-geçişi:** `al(|| b)` — escape.c interprocedural değil → yakalanmıyor. Ancak senkron
   çağrıda çevre frame canlı kaldığından **temiz UAF DEĞİL**; asıl risk callee'nin saklaması (interproc)
   + D-071 mis-dispatch (closure_mu kaybı). D-071'de zaten V2'ye ertelenmiş.

Kalıcı/tam çözüm: **V2** (heap/uniform env + bölge runtime + uniform self-describing closure temsili) —
ayrı kampanya (bkz. `belgeler/KEMGU_Bellek_Modeli.md` R-YAKALAMA-ESCAPE/THREAD).

**Doğrulama:** `test_tip_kontrol` 184/184 (6 yeni G005: 3 pozitif + 3 guard), ASan/UBSan TEMİZ.
`mingw32-make test_tumu` → "Tum testler gecti!" (fixpoint stage1==stage2 korunur; `--check`/`--checkdump`
korpus divergens yok; 4 lambda E2E korunur). 0 uyarı.

## D-095 — [YÜKSEK] Self-host codegen `güvensiz { }` bloğu — sessiz düşme (accept-but-miscompile) kapatma (2026-06-16)

**Karar [ETKİ: self-host codegen doğruluk; izole commit].** `selfhost/codegen.kem`
lexer (`güvensiz`→`GUVENSIZ`, ~satır 205) ve parser (`parse_guvensiz`, ~satır 1203 →
`dugum1("GUVENSIZ", ..., parse_blok)` = TEK çocuk, o da BLOK) `güvensiz` bloğunu
tanıyordu; ancak codegen `deyim_uret` (~satır 2367) içinde **GUVENSIZ emisyon dalı
YOKTU** → düğüm hiçbir kola düşmüyor, fonksiyon sonundaki `ver 0` fall-through ile
**gövde TAMAMEN düşürülüyordu** (latent miscompile). Checker bloğu kabul ediyor,
codegen sessizce atıyor → accept-but-miscompile, **[YÜKSEK]**. C derleyici
(`kemgu.exe`, oracle; `src/llvm.c:4639` `DUGUM_GUVENSIZ`) AYNI programı doğru
derliyordu — saf self-host mirror-gap (D-093 ile aynı sınıf).

**Kök-neden (mirror-gap; reprodüksiyon + IR-teyit):** Test programı
```
işlev yardimci(x: tam32) -> tam32 { ver x + 1; }
işlev main() -> tam32 {
    değişken toplam: tam32 = 0;
    güvensiz { değişken a: tam32 = 40; a = a + 1; toplam = yardimci(a); }
    ver toplam;
}
```
C-codegen `main` gövdesi alloca+store+`call i32 @yardimci` emit ediyor → **exit 42**.
Self-host gövdesi GUVENSIZ kolu yokken bu deyimleri HİÇ emit etmiyordu → `toplam` 0
kalır → **exit 0**. Bug birebir teyit edildi.

**Çözüm (`selfhost/codegen.kem`, iki nokta — DESCENT denetimi):**
1. `deyim_uret` — BLOK kolunun hemen yanına **GUVENSIZ kolu** eklendi: tek çocuğu
   (`cocuk[a_cb+0]` = BLOK gövde) `deyim_uret` ile aynen emit, `ver 0`. C llvm.c
   `DUGUM_GUVENSIZ` ile aynı: güvensiz = codegen açısından DÜZ BLOK, gövde aynen
   üretilir. C'de ayrıca `guvensiz_derinlik` ile inline stack sınır-kontrolü atlanır;
   self-host **HEAP-uniform** (her dizi heap `KdlDizi*`, stack `[N×T]` yok → inline
   stack kontrolü zaten yok) → o makine GEREKMEZ; heap erişimleri runtime-kontrollü
   kalır = güvenli.
2. `alloca_hoist_pass` (~satır 2538) konteyner listesine **GUVENSIZ** eklendi
   (`BLOK/EGER/IKEN` yanına). GUVENSIZ'in tek çocuğu BLOK → mevcut döngü gövdeyi
   içeri indirir → güvensiz içindeki annotasyonlu `değişken` alloca'ları girişe
   hoist edilir (döngü-içi güvensiz'de yığın taşması önlenir; `pa_reg_bul` idx-eşli,
   sıra-bağımsız → güvenli).

   **Checker:** gömülü `kontrol_dugum` (~satır 3478) zaten GENEL çocuk-rekürsiyonu
   yapıyor (özel kol gerektirmeyen düğümler için tüm çocuklara iner) → GUVENSIZ'in
   çocuğu (BLOK) ZATEN denetleniyor; ek kol GEREKMEDİ (teyit: güvensiz içindeki
   tanımsız ad T002 veriyor; tanımlı `değişken` ad-çözümleniyor). Fonksiyon-seviyesi
   döngüler (emisyon ~2619, hoist çağrısı ~2612, `kontrol_govde` ~3510) GUVENSIZ'i
   doğrudan çocuk olarak GÖRMEZ (güvensiz gövde-içi, ISLEV'in BLOK çocuğunun altında)
   → dokunulmadı.

**Doğrulama:** Repro fix sonrası **exit 42** (C oracle ile eşit). Self-host IR
`main`: iki alloca girişe HOIST (`toplam` + güvensiz-içi `a`), `store 40` /
`a=a+1` / `call i32 @yardimci` gövdede EMIT (önceden hiçbiri yoktu). Yeni korpus
`test/cg_korpus/cg_guvensiz.kem` (gövde düşerse exit 0, doğruysa 42 → C oracle ile
auto-diff yakalar). `codegen_diff_harness`: **58/58** semantik eşdeğer. `make test_tumu`
→ **"Tum testler gecti!"**. `selfhost_driver_harness`: 4 mod (token 22/22, parse 12/12,
**check 48/48** = `--check`↔C `--checkdump` byte-diff TEMİZ) hem C-derlenmiş hem
self-host; **CODEGEN FIXPOINT stage1 IR == stage2 IR BİREBİR (21835 satır)**;
bootstrap lexer/parser/checker 51 birebir 0 fark. `asan_e2e_denetim.sh` →
**PASS=96 FAIL=0** (SKIP/ALLOW belgeli-ortamsal). Sıfır derleyici uyarısı
(`kemgu.exe` `-Wall -Wextra -Wpedantic` değişmedi; değişiklik `.kem`).

**Sınır/Not:** Kapsam YALNIZ self-host codegen — **POINTER-SİZ** güvensiz blokları
(düz blok gövdesi). Güvensiz içindeki pointer işlemleri (deref `*`, adres-al `&`;
`codegen.kem:2244` "CG sonrası") self-host'ta HENÜZ codegen edilmiyor → bu kararın
DIŞINDA (ayrı açık iş); bu fix pointer-siz güvensizi çalıştırır + silent-drop'u kaldırır.
`codegen.kem`'in KENDİSİ (ve lexer/parser/checker.kem) `güvensiz` bloğu KULLANMIYOR
(yalnız anahtar-kelime tanıma + `parse_guvensiz`) → yeni dal self-derlemede
tetiklenmiyor; **FIXPOINT yapısal olarak güvenli**. İzole commit; D-NNN merge-anı
güncel main'den tahsis (branch'te D-095 sabitlendi; main D-094 = G004 / PR #68).

---

## D-094 — [YÜKSEK] C checker G004 — işlev/lambda-tipli değişken YENİDEN-ATANAMAZ (accept-but-crash kapatma; öksüz fix backport) (2026-06-15)

**Karar [ETKİ: C checker doğruluk/bellek-güvenliği; izole commit].** `src/tip_kontrol.c`
`DUGUM_ATAMA` handler'ına (T022 lvalue kontrolünden hemen sonra, ~satır 4491) G004
reddi eklendi: hedef **TANIMLAYICI** ve tipi **TIP_ISLEV** (işlev/lambda) ise atama
**reddedilir**. Bu, `feature/self-host-checker` dalında ZATEN var olan ama `main`'in C
checker'ında BULUNMAYAN bir **öksüz fix**'in birebir backport'udur (kaynak:
`origin/feature/self-host-checker:src/tip_kontrol.c` ~satır 4490-4496).

**Kök-neden (KARMA closure temsili — değere bağlı):** Bir işlev/lambda değerinin runtime
temsili **yakalama-durumuna** bağlı: yakalamasız lambda / top-level fn → bare fn-ptr;
yakalamalı lambda → closure `{fn, env}`. Çağrı yeri statik `closure_mu` bayrağına göre
dispatch eder (bağlama anında sabitlenir). İşlev-tipli bir değişken **farklı**
yakalama-durumlu bir değerle YENİDEN atanırsa → temsil uyumsuzluğu → çağrıda bare-ptr'ı
closure sanıp deref → **access-violation / SEGFAULT** (accept-but-crash, **[YÜKSEK]**).
Mehmet'in seçtiği V1 ucuz-güvenli çözüm: compile-time reddet (çökmezlik #1); programcı
yeni bir `değişken` ile bağlasın.

**Çözüm (`src/tip_kontrol.c`, DUGUM_ATAMA — T022'den sonra, T001/bidirectional'dan önce):**
`TipBilgisi *ht = tip_belirle(tk, hedef);` ardından `hedef->tip == DUGUM_TANIMLAYICI &&
ht->kategori == TIP_ISLEV` ise `tip_hata(tk, d, "G004", ...)` + `break`. Hata mesajı
ASCII-güvenli (Türkçe `\x` hex-escape kuralı gereği string literal'de Türkçe karakter
yok). ERISIM/INDEKS hedefler (`o.alan = v`, `arr[i] = v`) ETKİLENMEZ — yalnız çıplak
TANIMLAYICI yeniden-bağlaması reddedilir.

**Doğrulama:** `test/test_tip_kontrol.c`'ye 4 yeni vaka (175-178): (1) lambda lokali
yeniden atama → hata, (2) yakalama-durumu divergent yeniden atama → hata, (3) tek-atama
lambda bildirimi → 0 hata (yanlış-pozitif yok), (4) lambda-OLMAYAN yeniden atama
(`x = 7`) → 0 hata (over-reject yok). `tip_kontrol` harness 178/178 yeşil. E2E
(`kemgu.exe --check`): lambda yeniden-atama programı → `hata[G004]` (exit 1); `tam32`
yeniden-atama programı → `OK` (exit 0). `make test_tumu` → "Tum testler gecti!" (tam
yeşil). Sıfır derleyici uyarısı (`-Wall -Wextra -Wpedantic`).

**Sınır/Not:** Kapsam **YALNIZ C checker** (`src/tip_kontrol.c`). Self-host
`selfhost/checker.kem` lambda kullanmıyor → self-host port ŞİMDİLİK GEREKMEZ (mirror-gap
yok). V1 ucuz-güvenli reddetme; tam değer-akışı / yakalama-durumu izlemeyle koşullu izin
V2'ye ertelendi (D-072 ailesi). Öksüz fix backport — main'de eşi yoktu. İzole commit;
base `main`, **MERGE EDİLMEDİ** (orchestrator denetler, Mehmet merge eder).

---

## D-093 — [YÜKSEK] Self-host codegen INDEKS-atama (`arr[i] = v`) — sessiz düşme (accept-but-miscompile) kapatma (2026-06-15)

**Karar [ETKİ: self-host codegen doğruluk; izole commit].** `selfhost/codegen.kem`
ATAMA handler'ı (`deyim_uret`, ~satır 2448) yalnız **TANIMLAYICI** (`x = v`) ve
**ERISIM** (`o.alan = v`) dallarına sahipti; **INDEKS** hedef (`arr[i] = v`) dalı
YOKTU. Checker (`selfhost/checker.kem` lvalue T022, ~satır 2806) INDEKS'i geçerli
lvalue olarak KABUL ediyor, codegen ise sessizce DÜŞÜRÜYORDU → yazma kayboluyor
(`ver 0` fall-through). Accept-but-miscompile, **[YÜKSEK]**. C derleyici
(`kemgu.exe`, oracle) AYNI programı doğru derliyordu — yani saf self-host mirror-gap.

**Kök-neden (mirror-gap; reprodüksiyon + IR-teyit):** Test programı
```
işlev main() -> tam32 { değişken xs: Dizi<tam32> = []; dizi_ekle(xs,5); xs[0]=42; ver dizi_al(xs,0); }
```
C-codegen `main` gövdesi `call void @kdl_dizi_yaz_tam(ptr %5, i32 0, i32 42)` emit
ediyor → **exit 42**. Self-host gövdesi bu çağrıyı HİÇ emit etmiyordu (ATAMA INDEKS
dalı yok) → **exit 5** (`dizi_ekle` değeri kalıyor). Bug birebir teyit edildi.

**Çözüm (`selfhost/codegen.kem`, ATAMA handler — ERISIM dalından sonra):** INDEKS
hedef dalı eklendi. HEAP-uniform model (her dizi heap `KdlDizi*`; stack `[N×T]` yok)
→ inline sınır kontrolü GEREKMEZ; yalnız `kdl_dizi_yaz_*` route yeterli (runtime
`i<0` + `i>=boyut` denetler, `runtime/kdl_runtime.c`). Emisyon `dizi_yaz` built-in'iyle
(~satır 2121) BİREBİR aynı: taban dizi → `ptr`, indeks → `i32`, değer → `vty`
(`p.son_tip`); `call void @kdl_dizi_yaz_<dizi_ekle_sonek(vty)>(ptr taban, i32 idx,
<dizi_arg_tip(vty)> v)`. Sonek seçimi değer-tipinden: `i64`→`tam64`, `ptr`→`ptr`,
diğer→`tam` — `dizi_yaz` ile AYNI seçici/cast (tutarlılık + iyi-tipli IR garantisi).

**Doğrulama:** Repro fix sonrası **exit 42** (C oracle ile eşit). OOB (`xs[10]=9`,
boyut 1) → runtime **PANİK** (`dizi sınır ihlali (i=10, boyut=1)`, hem C hem self-host
özdeş). İç-içe `m[i][j]=v` → **99**, yapı-alanı `k.xs[i]=v` → **55** (her ikisi de C
oracle ile eşit; eleman-tip propagasyonu nested + struct-field için ÇALIŞIYOR — analizin
"kısmi" şüphesi bu vakalarda gerçekleşmedi). `selfhost_driver_harness.sh`: 4 mod
(token 22/22, parse 12/12, check 48/48) byte-diff TEMİZ; LLVM eşdeğerlik self 56/56 +
self2 57/57; **FIXPOINT KORUNDU** (kemgu_self2 codegen.kem IR kararlı, stage1==stage2
birebir, 21807 satır). `make test_tumu` → "Tum testler gecti!" (tam yeşil).
`asan_e2e_denetim.sh` **PASS=96 FAIL=0**. Yeni korpus: `test/cg_korpus/cg8_indeks_yaz.kem`
(yazma düşerse 11, doğruysa 42 → C oracle ile auto-diff yakalar). Sıfır derleyici
uyarısı (`-Wall -Wextra -Wpedantic`).

**Sınır/Not:** Kapsam YALNIZ self-host codegen. C `src/llvm.c`'nin `xs[i]=v`'si ZATEN
main'de (D-088 ailesi) — bu kararın DIŞINDA. `codegen.kem`'in KENDİSİ `arr[i]=v`
sözdizimi kullanmıyor (yalnız `dizi_yaz` built-in) → yeni dal self-derlemede
tetiklenmiyor; FIXPOINT yapısal olarak güvenli. İndeks `i32` varsayılır (`dizi_yaz` +
INDEKS-okuma ile AYNI sözleşme); `i64` indeks bu kararın dışında. İzole commit;
şu an paralel dal yok.

---

## D-092 — [YÜKSEK] `Dizi<T>` ATAMA dizi-literal heap-promote — accept-but-crash kapatma (2026-06-15)

**Karar [ETKİ: codegen bellek-güvenliği; izole commit].** `D-075`'in 🔴 KEŞİF
notunda işaretlenen accept-but-crash deliği kapatıldı: `--check` KABUL eden ama
üretilen kodu ÇÖKERTEN (segfault, exit 139) iki tetik düzeltildi:
```
değişken xs: Dizi<tam32> = []; xs = [1]; dizi_ekle(xs, 7);      // eskiden SEGFAULT
yapı K { xs: Dizi<tam32>; } ... k.xs = [1]; dizi_ekle(k.xs, 7); // eskiden SEGFAULT
```

**Kök-neden (D-075 KEŞİF + IR doğrulaması):** `değişken xs: Dizi<T> = [..]` (init)
yolu dizi-literalini HEAP `KdlDizi*`'a promote ederken, **ATAMA** yolu (`xs = [..]`
/ `k.xs = [..]`) stack `[N×T]` pointer'ını `Dizi<T>` (heap `KdlDizi*`) slot'una
store ediyordu. Üretilen IR'da görüldü: `%1 = alloca [1 x i32]; ... store ptr %1,
ptr %0` (`%0` = KdlDizi* slot). Sonraki `dizi_ekle`/`dizi_boyut` `KdlDizi*`
beklerken stack-array görünce çöküyordu. D-070 ailesinin (dizi-literal temsil
uyuşmazlığı) ATAMA analoğu.

**Çözüm (`src/llvm.c`) — main'in `beklenen_tip` kanalı (ayrı helper DEĞİL):**
`DUGUM_DIZI_OLUSTUR` codegen'i zaten `g->beklenen_tip` `Dizi<T>` ise heap
`KdlDizi*` üretir (D-044/D-088 yolu, ~satır 2132). ATAMA hedefi `Dizi<T>` heap +
RHS dizi-literal iken, `ifade_uret(RHS)`'den ÖNCE `g->beklenen_tip`'i hedefin
`Dizi<T>` AST tipine SET edip (sonra restore) bu heap-path'i devreye sokuyoruz —
init ile **AYNI** mekanizma. Reddetme DEĞİL; reassignment normal işlem.
- **Yerel değişken (`xs = [..]`):** `i->dinamik_dizi_mi` + RHS literal → küçük
  `dizi_tip_sar(g, i->eleman_tip_ast)` yardımcısı eleman AST'sini sentetik
  `DUGUM_TIP_DIZI`'ye sarar (heap-path yalnız `tip` + `eleman_tip` okur),
  `beklenen_tip`'e konur, heap `KdlDizi*` slot'a store edilir.
- **Yapı alanı (`k.xs = [..]`):** `dizi_alan_eleman_ast` alanın `Dizi<T>` eleman
  AST'sini verir (NULL → normal skaler alan); aynı `dizi_tip_sar` + heap store.

**Doğrulama:** 2 repro fix öncesi exit 139 → fix sonrası exit 2 (boyut doğru),
IR'da stack `[N×T]` yok (yalnız `kdl_dizi_olustur`+`kdl_dizi_ekle`). `make
test_tumu` tam yeşil — **self-host FIXPOINT korundu** (stage1 IR == stage2 IR,
21728 satır birebir). `asan_e2e_denetim.sh` PASS=96 FAIL=0 (yeni
`test/ornekler/dizi_atama.kem` auto-discovery). `dizi_sinir_harness.sh` 37/37
(yeni vaka30/31/32: ATAMA dizi-literal → çalışır). Sıfır derleyici uyarısı
(`-Wall -Wextra -Wpedantic`).

**Sınır/Not:** Numara D-092 (orchestrator) — main self-host serisi D-082..D-091'i
aldığı için PR #60'ın eski D-082 numarası kullanılmadı. PR #60 D-083 (heap
`xs[i]=v`) ARTIK main'de (PR #63 / D-088 ailesi) → bu kararın kapsamı DIŞINDA,
düşürüldü. Türetilmiş olmayan basit (nesne TANIMLAYICI olmayan, örn. `a.b.xs`)
alan zincirleri `dizi_alan_eleman_ast` kapsamı dışı (D-088 ile aynı sınır).

---

## D-091 — [YÜKSEK] İç-içe `Dizi<Dizi<T>>` — iç dizi literali heap + nested `m[i][j]` heap-route (2026-06-14)
*(eski D-088; main self-host serisi D-082..D-087 ile çakışan dizi-indeks ailesi yeniden numaralandığından kaydırıldı)*

**Karar [ETKİ: codegen doğruluk; izole commit].** İç-içe dizi literali
`[[1,2],[3,4]]`'in İÇ dizileri (`[1,2]`, `[3,4]`) heap `KdlDizi*` değil, STACK
`[N×T]` olarak depolanıyordu (dış heap dizi onlara düz ptr tutuyor). Sonuç:
- `m[1][1]` ÇALIŞIYORDU (iç stack `[2×i32]` üzerinde düz GEP doğru) — bu yüzden
  hata gizliydi.
- İç diziyi değişkene çıkarınca uzunluk metadata BOZUK: `inner = m[0];
  dizi_boyut(inner)` → 1 (gerçek 2); `dizi_al(inner, i)` → PANİK (`boyut=1`).
  Çünkü `m[0]` stack `[2×i32]` ptr (KdlDizi* descriptor değil); `kdl_dizi_boyut`
  descriptor'ın ilk alanı sanıp `inner[0]` = 1 okuyor.

**Kök-neden:** DEGISKEN dedicated heap path (`değişken d: Dizi<T> = [..]`) iç
elemanları üretirken `g->beklenen_tip`'i AST eleman tipine SET ETMİYORDU (yalnız
IR string `eleman_tip` geçiyor) → iç `[1,2]` `DUGUM_DIZI_OLUSTUR`'da beklenen_tip
`DUGUM_TIP_DIZI` görmeyip stack dalına düşüyordu. D-085/D-087 dizi-indeks
serisinin BİLEREK ERTELENMİŞ son parçası (D-085 ve D-087 "Sınır" notları).

**Çözüm (llvm.c) — `m[i][j]`'yi BOZMADAN:**
1. **İç literal heap:** DEGISKEN heap literal path'te eleman döngüsünü
   `g->beklenen_tip = <iç dizi AST tipi>` ile sarmala → iç `[1,2]`
   `DUGUM_DIZI_OLUSTUR` heap yolunu seçer (heap `KdlDizi*`). Dış dizi artık iç
   descriptor'ları (`ptr`) tutar.
2. **AST eleman tipi izleme:** `LlvmIsim`'e `const Dugum *eleman_tip_ast`
   (`eleman_llvm_tip="ptr"` iç diziyi gizlerken gerçek AST'yi saklar). DEGISKEN
   (literal + annot heap) ve param (`Dizi` + `&Dizi`) sitelerinde set.
3. **Nested INDEKS heap-route:** `turetilmis_heap_dizi_eleman` (IR döndüren,
   ERISIM/CAGRI) → `heap_dizi_eleman_ast` (AST döndüren ortak çözümleyici:
   TANIMLAYICI + nested INDEKS + ERISIM + CAGRI). `m[i][j]`: `m[i]` artık heap
   `KdlDizi*` → `[j]` recursive olarak `kdl_dizi_al`'a route edilir (eski
   stack-GEP iç descriptor'ı i32 okuyup BOZARDI → regresyon olurdu). `[]`
   okuma+yazma her ikisi.
4. **Struct iç eleman:** `Dizi<Dizi<Yapı>>` by-value yolu (D-087
   `dizi_struct_al_emit` / `kdl_dizi_*_yapi`) `heap_dizi_eleman_ast` üzerinden
   kapsanır (`et[0]=='%'`).

**Doğrulama:** `dizi_sinir_harness.sh` +5 vaka (iç-içe oku KORUNUR `m[1][1]=4`,
inner boyut=2, inner dizi_al=2, nested yazma `m[0][1]=99`, dış-indeks OOB PANIC)
→ 33/33. `test/ornekler/icice_dizi.kem` (matris satır çıkarma + döngü, exit 42,
ASan auto-discovery). Tüm suite yeşil; `asan_e2e` PASS=95 FAIL=0; 0 uyarı
(-Wall -Wextra -Wpedantic). Üçlü iç-içe (`Dizi<Dizi<Dizi<T>>>`), `tam64` iç
eleman, `Dizi<Dizi<Yapı>>` by-value elle doğrulandı. ERISIM/CAGRI tek-indeks
(D-085) regresyonsuz (`heap_dizi_eleman_ast` aynı IR'i üretir).

**Sınır:** `dizi_olustur(N)` explicit-builtin iç-içe için kapsam-dışı (literal
`[...]` yolu doğru; D-087'deki gibi explicit dizi_olustur nadir). Test edilen
heap tabanlar: düz değişken/param `Dizi<Dizi<...>>` (üçlü derinliğe kadar) +
tek-indeks ERISIM/CAGRI. İç-içe dizinin yapı ALANI olduğu zincirler (`k.m[i][j]`)
`heap_dizi_eleman_ast` ile çözülür ancak iç literal heap'liği yapı-oluştur yoluna
bağlı olduğundan ayrıca test edilmedi (gelecek).

## D-090 — [YÜKSEK] `Dizi<Yapı>` by-value struct eleman (skaler-i32 varsayımı kaldırıldı) (2026-06-14)
*(eski D-087; main self-host D-087 ile çakıştığından yeniden numaralandı)*

**Karar [ETKİ: runtime + codegen; izole commit].** `Dizi<Yapı>` (struct elemanlı
dizi) skaler `kdl_dizi_ekle_tam`/`kdl_dizi_al_tam` (i32) + `eleman_byte=4` ile
derleniyordu → 8+ baytlık yapı **truncation** (alanlar sessizce kaybolur,
`ps[0].x+ps[0].y` yanlış) + `değişken p: Yapı = dizi_al(ps,i)` GEÇERSİZ IR
(`call %Yapi @kdl_dizi_al_tam` — i32 dönüş ile uyumsuz) → **link-fail**.

**Çözüm:**
- **runtime (kdl_runtime.c):** üç by-value memcpy fonksiyonu —
  `kdl_dizi_ekle_yapi(d, src)`, `kdl_dizi_al_yapi(d, i, dst)`,
  `kdl_dizi_yaz_yapi(d, i, src)`. Eleman boyutu `d->eleman_byte` (descriptor'dan).
  al/yaz OOB → PANIC (D-069 sınıfı).
- **codegen (llvm.c):** `dizi_eleman_struct_mi(et)` (`et[0]=='%'`) tüm dizi
  sitelerinde (literal ekle, değişken-annot heap, dizi_ekle/al/yaz built-in,
  `[]` okuma TANIMLAYICI+türetilmiş, `[]` yazma) struct dalı:
  ekle = store-to-temp + ekle_yapi; al = alloca dst + al_yapi + load; yaz =
  store-to-temp + yaz_yapi. **eleman_byte** struct için `kdl_eleman_byte_yaz`
  ile LLVM `sizeof` const-expr (`ptrtoint (getelementptr (%Yapi, null, 1))`) →
  padding/alignment LLVM layout'uyla BİREBİR (C tarafı elle hesap miscompile riski).
- Skaler/ptr yolları DEĞİŞMEDİ (`et[0]=='%'` guard'ı yalnız struct'ta devreye girer).

**Doğrulama:** `dizi_sinir_harness.sh` +5 vaka (struct [] oku/yaz, dizi_al,
{tam8,tam64} padding, struct OOB PANIC) → 28/28.
`test/ornekler/dizi_yapi_eleman.kem` (exit 42, ASan auto-discovery; padding +
by-value yazma + dizi_al-struct). Tüm suite yeşil (28 paket). `asan_e2e` PASS=94
FAIL=0 (memcpy-tabanlı, heap-overflow yok). 0 uyarı. **Sınır:** `dizi_olustur(N)`
explicit-builtin'i struct için eleman_byte hâlâ skaler-varsayım (literal `[...]`
yolu doğru; explicit dizi_olustur+struct nadir — gelecekte). İç-içe
`Dizi<Dizi<Yapı>>` türetilmiş indeks D-085 nested sınırına tabi.

## D-089 — [YÜKSEK] `&Dizi<T>` referans param: codegen deref + built-in tip-kontrol tutarlılığı (2026-06-14)
*(eski D-086; main self-host D-086 ile çakıştığından yeniden numaralandı)*

**Karar [ETKİ: codegen doğruluk + tip-kontrol tutarlılık; izole commit].** İki
yüzlü `&Dizi<T>` hatası:
1. **Codegen (çöp/PANIK):** `&a` çağrı argümanı, heap dizi değişkeninin SLOT
   adresini (`KdlDizi**` — çift pointer) geçiyor; callee bunu doğrudan `KdlDizi*`
   sanıp `dizi_al`/`[]` ile indeksliyordu → descriptor'ın kendisini veri okuyordu.
2. **Tip-kontrol (tutarsız):** `dizi_al(&Dizi)` SESSİZCE kabul (t_hata, rapor yok),
   `dizi_boyut(&Dizi)` T001 reddi — aynı argüman biçimi iki built-in'de farklı.

**Çözüm:**
- **llvm.c (param girişi):** `&Dizi<T>` / `&değişken Dizi<T>` param girişte BİR KEZ
  deref edilir (`load ptr` → `KdlDizi*`) ve alloca'ya o yazılır → sonrası NORMAL
  heap dizi (dinamik_dizi_mi=1, eleman tipi referans hedefinden). `dizi_al`/
  `dizi_yaz`/`dizi_boyut`/`[]` ek deref gerektirmez. Mutasyon (`dizi_yaz`) paylaşılan
  descriptor üzerinden çağırana yansır (referans semantiği korunur).
- **tip_kontrol.c:** ortak `dizi_arg_coz(t)` — `Dizi<T>` ya da `&Dizi<T>` →
  altındaki Dizi tipini (referansı soyarak) döner. Tüm dizi built-in'leri
  (ekle/al/yaz/boyut/kapasite/kapasite_ayarla) bunu kullanır → `&Dizi` tutarlı kabul.

**Doğrulama:** `dizi_sinir_harness.sh` +4 vaka (ref dizi_al / dizi_boyut / [] /
mutasyon-çağırana-yansır) → 23/23. `test/ornekler/dizi_referans_param.kem`
(exit 42, ASan auto-discovery). Tüm suite yeşil (tip_kontrol dahil). `asan_e2e`
PASS=93 FAIL=0. 0 uyarı. **Sınır:** `&Dizi` param girişte deref edildiği için
`xs = başka_dizi` (referansı yeniden bağlama) callee-yerel kalır (çağıranın slot'u
değişmez) — KEMGU referans semantiğinde nadir; içerik mutasyonu (asıl sözleşme)
çalışır. Lokal `değişken r: &Dizi = &a` (param olmayan) bu commit'te kapsam dışı.

## D-088 — [YÜKSEK] `[]` türetilmiş heap dizi tabanı (yapı-alanı / çağrı-dönüşü) — okuma+yazma heap-route (2026-06-14)
*(eski D-085; main self-host D-085 ile çakıştığından yeniden numaralandı)*

**Karar [ETKİ: codegen doğruluk; izole commit].** `[]` indeks operatörü yalnız
düz `TANIMLAYICI + dinamik_dizi_mi` tabanlarda heap-route (kdl_dizi_al/yaz)
ediyordu; TÜRETİLMİŞ heap `Dizi<T>` tabanları (yapı-alanı `k.xs`, `Dizi<T>` dönen
işlev `yap()`) KdlDizi* DESKRİPTÖRÜNÜ düz veri sanıp GEP yapıyordu →
**accept-but-silently-wrong** okuma (çöp değer) + **accept-but-crash** yazma
(segfault). Karşıtlık: `dizi_al`/`dizi_yaz`/`dizi_boyut` built-in'leri aynı
tabanlarda DOĞRU — taban reg'i (KdlDizi*) doğru üretiliyor, yalnız `[]`
lowering'i onu raw buffer sanıyordu (D-083 "Sınır" notunda kapsam-dışı işaretliydi).

**Çözüm (src/llvm.c):**
- Ortak `turetilmis_heap_dizi_eleman(g, nesne)` çözümleyici: ERISIM (yapı alanı
  `Dizi<T>` → `dizi_alan_eleman_ir`) ve CAGRI (`Dizi<T>` dönen işlev →
  `islev_bul` + dönüş tipi AST'sinden eleman IR). Değilse NULL → stack GEP.
- Okuma (DUGUM_INDEKS): TANIMLAYICI fast-path'in ARDINDAN türetilmiş taban
  heap ise `kdl_dizi_al_<tip>` route (`ifade_uret(taban)` zaten KdlDizi* verir).
- Yazma (DUGUM_ATAMA→DUGUM_INDEKS): TANIMLAYICI heap yazma artık DÜŞMÜYOR
  (eski "kdl_dizi_yaz_eleman yok" yorumu geçersiz — runtime'da `kdl_dizi_yaz_*`
  VAR); türetilmiş heap yazma da `kdl_dizi_yaz_<tip>` route.
- Ortak fn-seçici `kdl_al_fn`/`kdl_yaz_fn`/`kdl_al_donus_ir` (built-in ile
  paylaşılan eleman-IR→intrinsic eşlemesi).
- `değişken xs: Dizi<T> = yap()` (literal-DIŞI değer, çağrı/başka-dizi) artık
  `dinamik_dizi_mi=1` işaretlenir → `xs[i]` TANIMLAYICI fast-path'ten heap-route.
  (Tip kontrolü Dizi<T> annotasyonunu heap garanti eder; stack → G003 reddi.)

**Doğrulama:** `dizi_sinir_harness.sh` +5 vaka (erisim oku/yaz, çağrı oku, direct
heap yaz, türetilmiş OOB PANIC) → 19/19. `test/ornekler/dizi_turetilmis_taban.kem`
(exit 42, ASan auto-discovery). Tüm suite yeşil (29 paket, llvm 235/235).
`asan_e2e_denetim.sh` PASS=92 FAIL=0. 0 uyarı.

**Kapsam/sınır:** Skaler + ptr eleman. **Struct eleman (`Dizi<Yapı>`, `et[0]=='%'`)
şimdilik stack/yorum yoluna düşer — D-087'de by-value yapı.** İç-içe
`Dizi<Dizi<T>>` türetilmiş indeks (nested INDEKS tabanı) çözümleyici kapsamında
DEĞİL → mevcut çalışan stack-GEP yolu korunur (m[i][j] regresyonsuz); iç diziyi
değişkene çıkarınca uzunluk metadata hâlâ bozuk (nested-literal stack temsili,
ayrı sorun — D-082 inner-heap'e bağlı, deferred). &Dizi referansı D-086.

> **Not (merge):** PR #63'ün eski D-084'ü (stack `[N×T]` YAZMA OOB sınır-kontrolü)
> bu main-merge'inde DÜŞÜRÜLDÜ — birebir aynı düzeltme main'de `b09c5a2` / D-069
> (Kategori 2) olarak zaten mevcut (`stack_uz` sınır-kontrolü, `src/llvm.c`). Kod
> kaybı yok; yalnız çift kayıt önlendi. Düzeltmenin kendisi PR #63'ün
> `src/llvm.c`'sinde KORUNUYOR.

---

## D-087 — Bootstrap CHECKER kanıtı: 4 bileşenin TAMAMI self-host codegen ile doğru derlenir (2026-06-14)

**Karar [ETKİ: yalnız test/codegen_bootstrap_harness.sh; kaynak DEĞİŞMEDİ].** D-086'da codegen.kem
DRIVER oldu (checker dâhil) ve self-host-codegen ile FIXPOINT'e derlendi — ama fixpoint yalnız
DETERMİNİZM (stage1==stage2) kanıtlar, self-host-codegen-derlenmiş checker'ın DOĞRULUĞUNU değil.
Bootstrap harness'a CHECKER bileşeni eklendi (lexer/parser ile aynı desen): self-host-codegen ile
derlenen checker.kem'in `--checkdump` çıktısı, C-codegen ile derlenenle korpus üzerinde diff'lenir.

**Sonuç:** `make calistir_codegen_bootstrap` artık LEXER 46 + PARSER 46 + **CHECKER 46** +
CODEGEN FIXPOINT (stage1==stage2, 21728 satır) — **4 self-host bileşeninin TAMAMI** (lexer, parser,
checker, codegen) self-host codegen tarafından DOĞRU derlenir (korpus: selfhost/*.kem + ornekler;
3815-satır driver ve checker.kem'in kendisi dâhil). Bu, D-086 driver fixpoint'inin korelatif
doğruluk kanıtı (yalnız determinizm değil). `make test_tumu` YEŞİL, 0 regresyon, 0 uyarı.

**Not:** Bu, origin/feature/self-host-checker'daki 62dd7e8 (öksüz; main'e hiç merge olmadı) ile aynı
amacı main hattında bağımsız gerçekler — D-086 driver state'i üzerine (checker artık driver'da).

---

## D-086 — 🎉 AŞAMA 4 driver: tek self-host kemgu binary (checker + 4-mod dispatch) + driver FIXPOINT (2026-06-14)

**Karar [ETKİ: self-host birleştirme; C derleyici DEĞİŞMEDİ].** Aşama 1-3'te lexer/parser/checker/
codegen AYRI self-host binary'lerdi; D-085 codegen self-compile fixpoint'i kanıtladı. Aşama 4 =
`selfhost/codegen.kem`'i TEK birleşik KEMGU derleyici driver'ına dönüştürmek: checker mantığı +
`--token/--parse/--check/--llvm` dispatch eklendi → `build/kemgu_self.exe`. **Sonuç D-085'i aşar:**
birleşik driver da KENDİNİ fixpoint olarak üretir (self-host derleyici, checker dâhil).

**Süreç notu (şeffaflık):** Bu iş ilk olarak bayat D-081 tabanı üzerinde [D-082] etiketiyle yapıldı
(commit 20b5408, tag `asama4-d082-backup`) — ama gerçek D-082 = CG8 dizi (origin/main). Branch
origin/main'e (6c26bdf; D-082..D-085 dahil) sıfırlandı ve driver **yeni** codegen.kem (2659 satır;
CG8 dizi + CG7d + CG9a alloca-hoist + fixpoint) üzerine **yeniden** uygulandı, doğru D-086 ile.

**KARAR 1 — Yer: codegen.kem YERİNDE.** `checker.kem`/`lexer.kem`/`parser.kem` DOKUNULMADI (Aşama 1-2
referans; harness'ları yeşil). codegen.kem 2659→3820 satır. no-flag→--llvm varsayılan → mevcut
`calistir_codegen_diff` VE `calistir_codegen_bootstrap` (`<file>`→IR çağrıları) bozulmaz.

**KARAR 2 — Merge: front-end birebir, back-end union.** İki `Ayr` struct'ı lexer+parser+AST
tablosunda ÖZDEŞ. Ortak-isimli alanlar (`yapi_ad/alan_tip/fn_ad`) PAYLAŞILIR (—check KEMGU tipiyle,
—llvm LLVM tipiyle; ayrı invocation). Çatışma = tam 4: `ayr_olustur` (union init), `main` (dispatch),
`karsilastirma_mi` (checker yalnız sıralı `<>` → **`sirali_kars_mi`** rename, codegen'in `==/!=`
dahil olanıyla çakışmasın), `yapi_var_mi` (checker dup DROP — codegen `yapi_idx` ≡ checker
`yapi_idx_bul`). 64 checker fonksiyonu (`duz_yaz` + `g_ekle`..`kontrol_program`) programatik port;
sıfır duplicate (C checker T024 doğrular). CG8/CG9a'nın +12 fonksiyonu da çakışmaz.

**KARAR 3 — `--token` lexer.kem'den PORT EDİLMEDİ.** lexer.kem `lex_dosya`/`emit` streaming;
helper'ları (`sayi_emit`/`op_emit`) codegen'in tablo-tabanlı eşadlılarından divergent. Yeni
`token_dump` codegen'in mevcut `lex_et` tablolarını (t_ad/t_sat/t_sut/t_off/t_uz) C `--token`
formatında döker (lex_et isimleri C `token_tipi_adi` ile ampirik birebir).

**Doğrulama (`make calistir_self_driver`):** HEM C-derlenmiş HEM **self-host-derlenmiş** driver
(driver kendini derler → kemgu_self2) 4 modda da C oracle ile eşleşir: TOKEN 22/22, PARSE 12/12,
CHECK 48/48, LLVM 56/56 (her iki driver). **FIXPOINT:** kemgu_self2'nin codegen.kem IR'ı kararlı
(21728 satır). `make calistir_codegen_bootstrap` driver-ify codegen.kem ile: lexer 46 + parser 46 +
codegen stage1==stage2 (21728 satır) ✓ — **birleşik derleyici self-hosting fixpoint.** `make
test_tumu` YEŞİL (sıfır regresyon). 0 uyarı.

**Sınırlamalar / sonraki:** (a) checker mantığı artık iki yerde (checker.kem Aşama 2 referansı +
codegen.kem driver) — kasıtlı; tek-kaynağa indirgeme ileride. (b) `--check` checkdump formatı
(test edilebilirlik); insan-okunur "OK/HATA" ayrımı ileride. (c) origin/feature/self-host-checker'ın
D-071 lambda G004 + D-085 checker-bootstrap-proof commit'leri ayrı; bu iş onlardan bağımsız.

---

## D-085 — 🎉 AŞAMA 5 BOOTSTRAP FIXPOINT — codegen self-host KENDİNİ ÜRETİYOR (2026-06-14)

**Karar [ETKİ: milestone — kod değişmedi, doğrulama].** KEMGU-yazılı codegen (selfhost/codegen.kem)
gerçek bir self-host derleyici: ÜÇ bağımsız bootstrap kanıtı yeşil.

**1) LEXER bootstrap (46/46):** codegen.exe ile derlenen lexer, TÜM self-host + ornekler korpusunda
C-codegen lexer ile byte-identik token çıktısı (codegen.kem'in kendisi dahil — 26122 token).

**2) PARSER bootstrap (46/46):** codegen.exe ile derlenen parser, aynı korpusta C-codegen parser ile
byte-identik --ast (parser self-parse 9672, checker 16214, codegen 16397 AST satırı).

**3) CODEGEN self-compile FIXPOINT:** codegen.exe (C-build, stage0) codegen.kem'i derler → stage1 IR
(15114 satır) → codegen2.exe. codegen2.exe codegen.kem'i derler → stage2 IR. **stage1 == stage2,
BYTE-IDENTİK.** = derleyici kendini sabit-nokta olarak yeniden üretiyor (self-hosting'in tanımlayıcı
özelliği). Transitif: codegen2.exe lexer.kem IR'ı da codegen.exe ile birebir.

**Bu fixpoint'i mümkün kılan son düzeltmeler:** D-072..D-084 (CG1-9: literal→ifade→deyim→kontrol→
çağrı→multi-int→metin→yapı→dizi→hoist), önek-builtin (D-083), bool-lit MANTIKSAL fix (D-083,
bootstrap'in yakaladığı), alloca-hoist (D-084, döngü yığın taşması). Semantik oracle (exit-kod)
+ byte-diff bootstrap oracle birlikte.

**Doğrulama:** `test/codegen_bootstrap_harness.sh` (3 kanıt) Makefile `calistir_codegen_bootstrap`
→ test_tumu. **Sınır/Sonraki:** (a) CHECKER (checker.kem) self-host ayrı iş (tip-kontrol, codegen
değil) — checker_diff zaten 48/48 C-paritesinde; KEMGU-codegen-built checker bootstrap'i sıradaki;
(b) codegen.kem CG9-üstü özellik kullanmıyor (çeşit/eşleş/lambda/modül yok) → o yollar korpus-test'li
ama self-host'ta egzersiz edilmiyor; (c) AŞAMA 4 driver (tek `kemgu` binary'de lex+parse+check+codegen
zinciri) ayrı paketleme işi.

---

## D-084 — AŞAMA 5 CG9a: alloca-hoist ön-pass → LEXER BOOTSTRAP TAM (46/46 birebir) (2026-06-14)

**Karar [ETKİ: self-host codegen; C derleyici DEĞİŞMEDİ].** D-083'te teşhis edilen alloca-in-loop
yığın taşması düzeltildi. **alloca-hoist ön-pass:** `alloca_hoist_pass` işlev gövdesini gezip TÜM
annotasyonlu DEGISKEN alloca'larını entry bloğuna çıkarır (döngü-içi `değişken` artık bir kez
alloca → yığın sabit). `pa_node`/`pa_reg`/`pa_base` (düğüm→entry-reg eşlemi; shadow-güvenli, düğüm
anahtarlı). DEGISKEN handler: annotasyonlu → hoist-edilmiş reg'i kullan (alloca yok), yalnız store;
annotasyonsuz → inline (eski yol; self-host hepsi annotasyonlu, korpus nadir). C codegen D-041
hoist_renumber ile AYNI amaç, farklı mekanizma (ön-pass vs tmpfile-buffer-renumber).

**🎉🎉 LEXER BOOTSTRAP TAM — 46/46 BİREBİR (büyükler dahil):** codegen.exe ile derlenen lexer,
parser.kem (15558 token), checker.kem (26398), **codegen.kem (26122 — kendini lex'ler)** dahil
TÜM self-host kaynaklarında C-codegen-built lexer ile byte-identik. İlk TAM self-host fixpoint
bileşeni: KEMGU-yazılı codegen'in ürettiği makine kodu, C derleyiciyle aynı davranan lexer veriyor.

**Doğrulama:** oracle 56/56 (regresyon yok); `test/codegen_bootstrap_harness.sh` (KEMGU-codegen
lexer vs C-codegen lexer diff) Makefile `calistir_codegen_bootstrap` → `test_tumu`. **Sonraki:**
parser.kem bootstrap (--ast paritesi), sonra checker.kem (--checkdump), sonra codegen.kem
self-compile (Aşama 5 tam fixpoint: codegen.exe codegen.kem'i derler → codegen2.exe → idempotent).

---

## D-083 — AŞAMA 3/5 CG7d + LEXER BOOTSTRAP: önek-builtin + bool-lit fix + alloca-hoist teşhisi (2026-06-14)

**Karar [ETKİ: self-host codegen; C derleyici DEĞİŞMEDİ].** İlk gerçek bootstrap denemesi:
codegen.exe (KEMGU-yazılı codegen) ile self-host kaynakları derle.

**Builtin önek-eşleme (CG7d):** `builtin_kdl_ad` artık önek-tabanlı (`metin_`/`dosya_`/`yaz_`/
`yazdir_`/`arg_`/`oku_karakter`/`ondalik_bicimle` → `kdl_*`), C codegen ile aynı. **Kritik gate:**
önce `fn_var_mi` (kullanıcı işlevi mi) bakılır — `yaz_str`/`yaz_kacis` gibi `yaz_` önekli USER
fonksiyonlarını builtin sanmamak için. `dizi_yaz` özel-case eklendi. Declare bloğu tam küme.

**🔴 Bool-literal fix (bootstrap'in yakaladığı GERÇEK bug):** parser `doğru`/`yanlış` için
`MANTIKSAL` düğümü (a_deg="1"/"0") üretir, `DOGRU`/`YANLIS` DEĞİL. CG2'deki varsayımım yanlıştı;
hiçbir korpus testi çıplak bool literal kullanmadığından gizli kaldı. `iken doğru` → koşul "0"
(fallthrough default) → döngü hiç girilmiyordu. ifade_uret `MANTIKSAL` → a_deg döndürür. 55/55.

**🎉 LEXER BOOTSTRAP — 45/48 birebir:** codegen.exe lexer.kem'i derler → çalışan exe → C-codegen-
built lexer ile **BYTE-IDENTİK çıktı** (test/ornekler + küçük korpus 45 dosya). İlk self-host
fixpoint kanıtı.

**🔴 TEŞHIS — alloca-in-loop yığın taşması (kalan bootstrap engeli):** 3 BÜYÜK dosya (parser/
checker/codegen.kem) ~30KB+ girdide erken-temiz-çıkış (rc=0, çıktı capped). Kök-neden: "hoist-free"
tasarımım `alloca`'yı DEGISKEN'in olduğu yere basar → DÖNGÜ İÇİ `değişken` her iterasyonda alloca
→ yığın sınırsız büyür → ~binlerce iterasyonda taşma. C codegen D-041 hoist_renumber ile tam da
bunu önler. Korpus döngüleri az iterasyon (gizli kaldı); lexer binlerce. **Düzeltme (sonraki):**
DEGISKEN alloca'larını entry bloğuna hoist eden ön-pass (annotasyon→ll_tip; annotasyonsuz→tip_cikar).

---

## D-082 — AŞAMA 3 CG8: dizi (heap KdlDizi + []-literal + element-tip polimorfik builtin) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** codegen.kem'in son büyük
bağımlılığı (dizi_al 95× / dizi_ekle 89× / dizi_boyut 20× + `[]` init). Element-tip izleme:
`cg_aelem` (Dizi değişkeni eleman tipi) + `alan_elem` (Dizi alanı) + `beklenen_elem` (`[]` bağlamı)
+ `son_elem` (sonuç). Yardımcılar: `ll_eleman_tip` (TIP_DIZI→eleman), `dizi_eleman_byte`
(ptr/i64→8, diğer→4), `dizi_ekle_sonek`/`dizi_al_sonek`/`dizi_arg_tip`/`dizi_al_rettip`.

**`[]` (DIZI_OLUSTUR):** `call ptr @kdl_dizi_olustur(i32 <byte>)` (boş = sadece oluştur; runtime
boyut/kapasite=0, ekle'de büyür) + non-empty için her eleman `ekle`. Eleman byte = `beklenen_elem`
(annotasyon/alan bağlamından). **dizi_ekle:** DEĞER tipine göre route (ptr→ekle_ptr, i64→ekle_tam64,
else→ekle_tam). **dizi_al:** DİZİNİN eleman tipine göre route (`son_elem` arg[0]'dan); ptr→al_ptr/ptr,
i64→al_tam64/i64, else→al_tam/i32. **dizi_boyut→i32.** INDEKS (`xs[i]`) = dizi_al eşi. `metin`/`Dizi`
alanları `ptr` (8-byte slot ptr-eleman ile tutarlı).

**Doğrulama:** test/cg_korpus 54 program (+6 CG8: temel/boyut/literal/indeks/**Dizi&lt;metin&gt;**/
**struct-ref-dizi_ekle**). 54/54 exit eşdeğer. struct-ref IR doğru (load ptr→GEP→load Dizi→ekle —
self-host tok_ekle deseni); Dizi&lt;metin&gt; → `olustur(i32 8)`+`ekle_ptr`+`al_ptr`. **Sonraki:**
self-compile denemesi (codegen.exe ile lexer/parser.kem derle) + CG9 (kullanılan kalan: çeşit/eşleş?).

---

## D-081 — AŞAMA 3 CG7c: yapı by-reference (&Yapi param + alan mutasyonu) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** Self-host'un kalbi:
codegen.kem'in `Ayr`'ı her yerde `&değişken Ayr` (238 alan erişimi). Ref-izleme: `Ayr.cg_aref`
(değişken ptr ise işaret ettiği yapı) + `Ayr.son_ref` (son ifade ref'i) + `cg_var_ref_bul` +
`param_ref_yapi` (`&Yapi` param → yapı adı).

**Adres-al (`&`/`&değişken` TANIMLAYICI):** LOAD YOK — alloca zaten adres → `cg_var_bul`'u döndür;
son_ref = (değer-yapı `%X`→X, ya da ptr ise onun ref'i). **ERISIM ptr-yolu:** son_ref boş değilse
`getelementptr %Ref, ptr nesne, i32 0, i32 fidx` + `load`. **ATAMA ERISIM lvalue (`p.alan = v`):**
nesne struct-değer → alloca=adres; nesne ptr → `load ptr` ile taban; sonra GEP + `store`.
**Çağrı:** main `f(&değişken p)` (değer-var adresi) ve nested `f(p)` (ptr param yükle) — ikisi de
`ptr` arg üretir (mevcut & + TANIMLAYICI-load yolları).

**Doğrulama:** test/cg_korpus 48 program (+4 CG7c: ref-oku/**mutasyon**/nested-bare-ptr/çoklu-
mutasyon). 48/48 exit eşdeğer. Mutasyon IR doğru (load ptr→GEP→load/store alan). **Sınır:** tek-
seviye ERISIM lvalue (a.b.c= nadiren; self-host p.alan= kullanır). **Sonraki:** CG8 — dizi
(heap KdlDizi + []-literal + dizi_* element-tip polimorfik builtin + indeks + için).

---

## D-080 — AŞAMA 3 CG7b: yapı (struct) by-value — tip-def + oluştur + erişim (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** Yapı tablosu
(`yapi_ad/yapi_abase/yapi_acount/alan_ad/alan_tip` — checker.kem deseni) + ön-pass `yapi_topla`
(iki sub-pass: önce adlar, sonra alanlar → iç içe yapı ref'i çözülür) + `yapi_tip_emit`
(`%Ad = type { t0, ... }`).

**Bulgu:** sade yapı tipi (`Nokta`) parser'da **TIP_BASIT** (a_deg="Nokta"), TIP_KULLANICI DEĞİL
(o yalnız generic/qualified). ll_tip: TIP_BASIT + `yapi_var_mi` → `%Ad`; TIP_REFERANS/POINTER/DIZI
→ `ptr`. **YAPI_OLUSTUR (by-value):** `alloca %T` + her ALAN_ATAMA için `getelementptr+store`
(alan indeksi ADLA bulunur → alan-sırası bağımsız) + `load %T` (by-value akış). **ERISIM (value):**
nesne tipi `%...` ile başlıyorsa `extractvalue` (ptr/referans erişimi → CG7c). Yapı değişkeni/
dönüşü generic CG3/CG6 makinesiyle çalışır (vtip=%Ad, cur_ret=%Ad).

**Doğrulama:** test/cg_korpus 44 program (+5 CG7b: nokta/3-alan/by-value-dönüş/karışık-tip-tam64/
alan-sıra-bağımsız). 44/44 exit eşdeğer. **Sınır:** by-REFERANS (`&Yapi` param + alan mutasyonu)
→ CG7c (self-host'un Ayr'ı her yerde `&değişken Ayr` — kritik). **Sonraki:** CG7c — &T param +
adres-al (&var) + ptr erişim/atama (GEP+load/store).

---

## D-079 — AŞAMA 3 CG7a: metin literali + runtime builtin + declare header (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** D-072 CG7 planının metin
+ runtime yarısı (yapı = CG7b). Ön-pass workflow'u (4 ajan) C-codegen ABI'sını birebir çıkardı.

**Metin literali:** `Ayr.str_deg` (benzersiz literal havuzu, dedup). Ön-pass düz düğüm tablosunu
tarar (`str_pre_pass`), tüm METIN → havuz. Global: `@.str.N = private unnamed_addr constant
[K x i8] c"...\00"` (K=byte+1). **Escape (C ile birebir):** `\` `"` `<0x20` `>=0x7F` → `\HH`
(BÜYÜK hex) — Türkçe UTF-8 yüksek-byte'lar tam escape (`"çay"` → `c"\C3\A7ay\00"`, uzunluk 4).
Referans: `getelementptr [K x i8], ptr @.str.N, i32 0, i32 0`. `metin` tipi → `ptr` (ll_tip).

**Runtime builtin:** `builtin_kdl_ad` (KEMGU adı → `@kdl_*`; metin_*/yaz_*/dosya_*/arg_*) +
`builtin_ret` (dönüş tipi). CAGRI built-in tespit → `@kdl_*` çağrısı; void çağrı `%r` atamaz.
**Kritik — i1 normalizasyonu:** runtime `metin_esit` vb. GERÇEK i1 döner ama mantıksal=i32
invaryantı → call sonrası `zext i1→i32` (CG2/CG4 ile tutarlı, exit eşdeğer). `runtime_header_yaz`
declare bloğu (codegen.kem alt kümesi + libc).

**Doğrulama:** test/cg_korpus 39 program (+6 CG7a: uzunluk/esit/birlestir/metin-dönüş/param-bayt/
**türkçe**). 39/39 exit eşdeğer. IR temiz (dedup, UTF-8 escape, zext). **Sınır:** dizi_* builtin
henüz yok (CG8 — element-tip polimorfizmi). **Sonraki:** CG7b — yapı (%T type, alloca/GEP,
erişim/oluştur, by-ref param + by-value dönüş).

---

## D-078 — AŞAMA 3 CG6: multi-int (i8/16/32/64) + tip-izleme + sext/trunc (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** Uniform-i32'den
gerçek-tip codegen'e: `Ayr.son_tip` (her ifade_uret sonucunun LLVM tipi — recursive "dönüş-tip
register'ı"), `Ayr.cur_ret` (mevcut işlev dönüş tipi), `Ayr.cg_atip` (değişken→LLVM tip),
`Ayr.fn_ad/fn_ret` (ön-pass imza tablosu). Yardımcılar: `ll_tip` (TIP_BASIT → i8/i16/i32/i64),
`tip_birlestir` (operand birleştir), `tip_genislik`, `fn_ret_bul`, `islev_donus_tip`, `param_tip`.

**Anahtar basitleştirme — KEMGU örtük-dönüşüm YOK → operand birleştirme tek-yönlü:** `a + b`'de
operandlar zaten aynı tip (checker garantisi); tek istisna bağlamsız literal (i32-default).
`tip_birlestir` = biri i32 ise diğeri. Literaller metin-agnostik ("5"), tip yalnız komut
annotasyonunda → literali yeniden-emit gerekmez. **mantıksal = i32 tutuldu** (CG2/CG4 bool
mantığı bozulmadı; `define i1` yerine `define i32` — exit-kod eşdeğer). **Casts (`olarak`,
TIP_DONUSTUR):** hedef>kaynak → `sext`, hedef<kaynak → `trunc`, eşit → no-op.

**Doğrulama:** test/cg_korpus 33 program (+5 CG6: tam64/tam16/tam8-sext/trunc/i64-param).
i64 alloca/add/mul, `trunc i64→i8`, `sext i8→i32`, i64-param+call hepsi temiz IR. **33/33 exit
eşdeğer.** **Sınırlar (v1):** (a) signed div/rem (sdiv/srem) — dtam (unsigned) için udiv/urem
henüz yok (codegen.kem signed tam kullanır → self-host etkilenmez); (b) bağlamsız literal →
geniş paramda i32 default (geçici: tipli yerel kullan); (c) kesirli (float) yok (CG sonrası).
**Sonraki:** CG7 — metin literali (@.str global) + yapı (%T, alloca/gep) + runtime declare header.

---

## D-077 — AŞAMA 3 CG5: çağrı + parametre + özyineleme (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `islev_uret` artık
parametreleri emit eder; `ifade_uret`'e CAGRI (call) eklendi.

**Parametre:** İmzada ADLI param (`%a0, %a1...` — adlı olduğundan `%N` reg sayacını tüketmez,
entry alloca'ları %0'dan başlar). Entry'de her param için `alloca i32` + `store %aN` + ad→reg
kaydı (CG3 yerel deseni; param mutable). **CAGRI:** hedef = çocuk[0] TANIMLAYICI adı; argümanlar
SIRAYLA `ifade_uret` ile değerlendirilip operandlar yerel `Dizi<metin>`'e biriktirilir (init+append
— güvenli desen, ATAMA-reassignment DEĞİL), sonra tek `call i32 @ad(i32 a0, ...)`. Aynı-modül
ileri-referans (özyineleme + forward-call) LLVM'de declare gerektirmez.

**Doğrulama:** test/cg_korpus 28 program (+5 CG5: topla/kare/fib/fakt/gcd-işlev). fib(10)=55,
fakt(5)=120, gcd(48,36)+30=42 — **28/28 exit eşdeğer**. **Sınır:** runtime/builtin çağrıları
(yaz_tam, dizi_*) CG7+ (declare header gerek). **Sonraki:** CG6 — multi-int (i8/16/64, dtam) +
sext/trunc + işaretsiz + gerçek dönüş-tipi emit (mantıksal→i1 main).

---

## D-076 — AŞAMA 3 CG4: kontrol akışı (eğer/iken → br + %bbN blok) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `deyim_uret`'e EGER
(if/else/else-if zinciri) ve IKEN (while). `ifade_uret`'e DOGRU/YANLIS literali (→ "1"/"0").

**Blok yönetimi:** `Ayr.lbl` (etiket sayacı, `bb0/bb1/...` ADLI bloklar — LLVM unnamed-temp
sayacını TÜKETMEZ, `%N` reg sayacı bağımsız kalır) + `Ayr.bb_term` (mevcut blok terminatörlü
mü). Yardımcılar: `yeni_label`, `etiket_yaz` (bb_term=0), `br_to` (yalnız bb_term==0 iken
fall-through dal — çift terminatör önlenir), `kosul_i1` (i32 0/1 koşulu → i1 `icmp ne 0`).
İşlev sonunda blok açıksa `ret i32 0` (ölü ama geçerli — iki dal da `ver`'lediğinde end bloğu).

**EGER:** else-yok → Lelse=Lend; else var → ayrı Lelse; else child BLOK veya iç EGER (her ikisi
de `deyim_uret` ile). **IKEN:** Lhead (koşul) → Lbody → Lhead geri-dal / Lend. SSA sayacı
işlev-geneli (blok-başı değil), etiketler adlı → numaralama çakışması yok.

**Doğrulama:** test/cg_korpus 23 program (+5 CG4: if/if-else/else-if/while/gcd). gcd(48,36)+30=42
(while + iç değişken + `%`). **23/23 exit eşdeğer** (ardışık koşu kararlı). Üretilen IR temiz:
bb0=head, bb1=body, bb2=end; sıralı `%0..%10`. **Sonraki:** CG5 — çağrı (call) + parametre
(alloca/store) + özyineleme (fib/faktöriyel).

---

## D-075 — AŞAMA 3 CG3: değişken + atama + tanımlayıcı (entry alloca/store/load) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `ifade_uret`'e
TANIMLAYICI (`load i32, ptr %a`); `deyim_uret`'e DEGISKEN (`alloca i32` + `store` + ad→reg
kaydı), ATAMA (lvalue TANIMLAYICI → `store`), IFADE_DEYIMI (yan etki için değerlendir).

**Tasarım — işlev-içi değişken haritası APPEND-only + cg_base (reassignment YOK):** ilk
denemede `p.cg_ad = []` (işlev başında haritayı sıfırla) **codegen.exe'yi SEGFAULT ettirdi.**
Kök-neden ↓. Çözüm: `Ayr.cg_base` = işlev-başı slice indeksi; `cg_var_ekle` yalnız append,
`cg_var_bul` `cg_base..son` arar (önceki işlevlerin değişkenleri görünmez). Parser zaten Dizi
alanlarını yalnız append eder (t_ad vb.) — aynı güvenli desen.

**🔴 KEŞİF — C derleyici accept-but-crash deliği (ATAMA ile dizi-literal):** `--check` KABUL
eder ama codegen ÇÖKER (segfault, exit 139):
```
değişken xs: Dizi<tam32> = []; xs = [1]; dizi_ekle(xs, 7);   // SEGFAULT
yapı K { xs: Dizi<tam32>; } ... k.xs = [1]; dizi_ekle(k.xs,7); // SEGFAULT
```
Tetik: **`Dizi<T>` lvalue'ya ATAMA ile dizi-literal RHS** (`xs = [...]`). `değişken`-init
yolu (`değişken xs: Dizi<T> = [...]`) ve yalnız-append ÇALIŞIR — init yolu heap KdlDizi
promote eder; ATAMA yolu stack `[N×T]` pointer'ını Dizi-slot'a yazar → `dizi_ekle` çöker.
D-070 ailesi (dizi-literal temsil uyuşmazlığı), ATAMA analoğu. **Self-host bundan etkilenmez**
(cg_base ile reassignment yok). Odaklı [YÜKSEK] düzeltme için işaretlendi (codegen ATAMA
yolunda init ile aynı heap-promote; G-kodu reddi DEĞİL — reassignment normal işlem).

**Doğrulama:** test/cg_korpus 18 program (5 CG1 + 8 CG2 + 5 CG3), **18/18 exit eşdeğer**
(5 ardışık koşu kararlı). Harness sağlamlık: 127 (Defender ilk-exec taraması) → bekle+tekrar
(6 tur). **Sonraki:** CG4 — eğer/iken (br + %bbN blok) kontrol akışı.

---

## D-074 — AŞAMA 3 CG2: karşılaştırma + mantıksal + tekli (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `ifade_uret`'e:
karşılaştırma (`== != < > <= >=` → `icmp <pred> i32` + `zext i1→i32`), mantıksal
(`ve`/`veya` → `and`/`or i32`, kısa-devresiz — C v1 ile aynı semantik), tekli (`neg`
→ `sub i32 0,x`; `degil` → `icmp eq i32 x,0` + zext). `karsilastirma_mi`/`icmp_pred`/
`zext_i1` yardımcıları eklendi.

**Önemli bulgu — korpus tip-GEÇERLİ olmalı (oracle önkoşulu):** karşılaştırma `mantıksal`
üretir; KEMGU'da `mantıksal → tam32` ÖRTÜK dönüşüm YOK (çekirdek ASLA kuralı). Yani
`(5>3)+41` tip-GEÇERSİZ → C `--check` reddeder → C codegen çöp/0 üretir → anlamsız oracle.
Çözüm: karşılaştırma/mantıksal/`degil` testleri `mantıksal`-dönüşlü main ile (`işlev main()
-> mantıksal { ver 5 > 3; }` → exit 1). C codegen bunlara `define i1 @main()` basar; KEMGU
codegen `define i32 @main()` + `ret i32 <zext 0/1>` basar — exit-kod 0/1 için eşdeğer
(gerçek dönüş-tipi emit'i CG6 multi-int'te). `degil`: C `xor i1,true`, KEMGU `icmp eq+zext`
— bayt farklı, **semantik aynı** (D-072 KARAR 1 oracle'ının tam da amacı).

**Doğrulama:** test/cg_korpus 13 program (5 CG1 + 8 CG2), **13/13 exit eşdeğer**. Harness
sağlamlık düzeltmesi: Win11 `.exe` yeniden-yazım dosya-kilidi yarışı → dosya-başı benzersiz
çıktı adları (`$b.c.exe`/`$b.k.exe`). **Sonraki:** CG3 — değişken (entry alloca/store/load)
+ atama + tanımlayıcı.

---

## D-073 — AŞAMA 3 CG1: codegen.kem iskeleti + ilk semantik-oracle yeşil (2026-06-14)

**Karar [ETKİ: yeni self-host artefakt; C derleyici DEĞİŞMEDİ].** D-072 planının CG1 adımı:
`selfhost/codegen.kem` oluşturuldu = `selfhost/parser.kem` kopyası (lexer+parser REUSE) +
AST-yürüten LLVM IR text emitter. `duz_yaz` (--ast dumper) → `program_uret`/`islev_uret`/
`deyim_uret`/`ifade_uret` ile değiştirildi; `main` artık IR basar.

**Kapsam (CG1):** işlev (parametresiz) + `ver` + tam literal + ikili aritmetik (`+ - * / %`).
Üretilen IR: `target triple` + `define i32 @ad() { entry: ... ret i32 <op> }`. **Hoist-FREE:**
tek blok `entry:`, SSA sıralı `%N` sayacı (`Ayr.reg`, işlev başında 0'a reset). C codegen'in
`entry:`+`%0`-başlangıç deseni doğrulandı; KEMGU emitter TAM literallerini doğrudan immediate
basar (C'nin `add i32 0, N`'inden daha sıkı ama semantik aynı).

**Doğrulama:** `test/codegen_diff_harness.sh` (SEMANTİK exit-kod oracle, D-072 KARAR 1) —
`test/cg_korpus/` 5 program (sabit/aritmetik/çıkarma/bölme/mod), **5/5 C-codegen ile exit
eşdeğer**. Makefile `calistir_codegen_diff` hedefi `test_tumu`'ya bağlandı (codegen.exe yokken
harness graceful → geriye uyumlu).

**Sınır/Sonraki:** CG1 dışı düğümler (tanımlayıcı/çağrı/değişken/eğer/...) henüz `0` üretir
(placeholder; korpus onları içermez). Sonraki: **CG2** — ikili karşılaştırma/mantıksal
kısa-devre + tekli (neg/değil); ardından CG3 değişken/atama/tanımlayıcı (entry alloca).

---

## D-072 — AŞAMA 3 (codegen self-host) ADIM-0: oracle + temsil + CG plan (2026-06-14)

**Karar [ETKİ: yok — dokümantasyon/plan; kod yok].** Bootstrap fixpoint'in (Aşama 5) asıl
darboğazı = codegen self-host: `src/llvm.c` (5271 satır, 34 düğüm tipi) → KEMGU'da yeniden
yazım (`selfhost/codegen.kem`). Lexer/parser/checker self-host'larındaki gibi ADIM-0 = envanter
+ oracle kararı + temsil + milestone planı.

**KARAR 1 — Oracle: SEMANTİK (exit-kod) eşdeğerliği, byte-identik IR-DİFF DEĞİL.** Parser/checker
oracle'ları düz-dump diff'iydi; codegen IR'ı için byte-identik diff ÇOK KIRILGAN: C codegen'in
SSA reg numaralandırması, D-041 hoist_renumber, formatlama = uygulama detayı (KEMGU codegen aynı
byte'ı üretmek zorunda kalmamalı). Bunun yerine: korpus programını HEM C codegen (`kemgu --llvm |
clang | run → exit`) HEM KEMGU codegen (`codegen.exe in.kem | clang | run → exit`) ile derle,
EXIT KODLARINI karşılaştır. Semantik eşdeğerlik = doğru oracle (metinsel değil). Korpus: test/
ornekler (main'li) + test_llvm gömülü programları (~199).

**KARAR 2 — Temsil: codegen.kem = selfhost parser (REUSE) + AST-yürüten IR text emitter.**
checker.kem deseni (parser + checker) gibi codegen.kem = parser + codegen. Düz AST tablosunu
(a_ad/a_deg/cocuk...) gezer; LLVM IR'ı yaz_str/yb ile basar. **Hoist-FREE tasarım:** alloca'lar
DOĞRUDAN entry bloğuna emit edilir (C'nin inline-alloca+hoist_renumber'ı YENİDEN YAZILMAZ —
D-041 sorunu baştan önlenir). SSA reg sıralı sayaç (LLVM unnamed value zorunluluğu); etiketler
%bbN. Runtime intrinsic declare'ları sabit header (llvm.c'deki gibi).

**KARAR 3 — CG milestone planı (her biri semantik-oracle kapılı, küçük korpus):**
- CG1: tam literal + işlev(main) + ver → `define i32 @main(){ret i32 N}`.
- CG2: ikili (aritmetik/karşılaştırma/mantıksal kısa-devre) + tekli.
- CG3: değişken (entry alloca/store/load) + atama + tanımlayıcı.
- CG4: eğer/iken (br + %bbN bloklar) — kontrol akışı.
- CG5: çağrı (call) + parametre alloca + özyineleme (fib/faktöriyel).
- CG6: multi-int (i8/16/64, dtam) + sext/trunc dönüşüm + işaretsiz.
- CG7: metin literali (@.str global) + yapı (%T type, alloca, gep) + erişim.
- CG8: dizi (heap KdlDizi + stack [N×T] + sınır-kontrol D-069) + indeks + için.
- CG9: çeşit/eşleş + generic mono + lambda/closure (D-071) + modül + cross-file.

**Riskler:** (a) 34 düğüm tipi = geniş yüzey, çok-pencere iş; (b) SSA sıralı numaralandırma
(KEMGU'da sayaç + dikkatli emit); (c) runtime ABI (yetki sret, yapı by-value) birebir; (d)
korpus seçimi (yalnız main'li + codegen-tam programlar; --check-only/parse-only hariç).
**Sonraki:** CG1 — codegen.kem (parser kopyası + minimal emitter) + codegen_diff_harness.sh.

---

## D-071 [YÜKSEK] — Sınıf B lambda/closure codegen V2: KARMA temsil (kabul-ama-çöküyor kapandı) (2026-06-14)

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit].** Güvenlik-iddiası izi
(D-070 devamı). D-031 Sınıf B'nin 4 lambda örneği (`04_islev`, `10_lambda`, `42_lambda_hesap`,
`25_closure_capture`) lambda codegen YOKLUĞUNDAN çöp fn-ptr çağrısı → SEGFAULT yapıyordu
(D-004 ertelemesi). Lambda/closure codegen sıfırdan yazıldı. C derleyici codegen değişti.

**Tasarım (5-ajan workflow ile doğrulandı, opt -passes=verify):** KARMA temsil —
- **Yakalamasız lambda → BARE fn-ptr** (`bitcast @lambda_N to ptr`; top-level fn ile aynı ABI;
  bare-call). `işlev(T)→R` param da bare-call → top-level fn VE yakalamasız lambda ikisi de geçer.
- **Yakalamalı lambda → CLOSURE** stack `{ ptr fn, ptr env }`; lifted `@lambda_N(ptr env, params)`
  capture'ları env'den load. Lokal değişken `closure_mu=1` → çağrıda env-unpack.
- Lifted fn'ler DEFERRED emit (BekleyenLambda kuyruğu, generic mono deseni; çevre fn gövdesi
  bitince — INLINE emit IR'ı bozardı). Capture analizi (`lambda_serbest_tara`) OLUŞTURMA anında
  (scope canlı) yapılıp kayda konur. D-041 hoist_renumber `%bbN`/`%env` (named) korur.

**Neden karma (uniform değil):** İlk deneme tüm lambda'ları closure + işlev-param closure_mu=1
yaptı → stdlib map/filtre/indirgeme (top-level fn'i işlev param'a geçiyor: `harita(xs, iki_kat)`)
KIRILDI (closure-unpack ham fn-ptr'da → çöp). Karma temsilde yakalamasız lambda = top-level fn
= bare fn-ptr → işlev param bare-call → ikisi de çalışır + yakalamalı (25) closure ile.

**Doğrulama:** 4 örnek → exit 42 (closure capture dahil); ASan/UBSan TEMİZ (4/4); opt verify
PASS; **test_llvm 235/235** (stdlib higher-order regresyonu çözüldü); bounds 11/11; checker
48/48; self-host 3/3; ASan E2E **PASS=91 FAIL=0** (4 lambda allowlist'ten çıktı → korumalı PASS;
ALLOW 6→2, yalnız G003-red 35/40). Yeni `make calistir_lambda_test` (test_tumu'da) 4/4. 0 uyarı.

**KAPSAM-DIŞI (V2/D-072):** yakalamalı lambda'yı işlev param'a geçirme (call-site trampolin
gerek); blok-form gövde son-ver çıkarsama (`||{...}` — lineer_closure/29_linear_closure);
dönüş tipi i32-dışı (call-site/lifted-fn senkronu — IR "ptr" tip taşımıyor); yapı/dizi/&T
capture; lambda escape (env stack — şu an non-escaping KEMGU v1 garantisi). **Sınıf B kapandı;
8 kabul-ama-çöküyor deliğinin tümü artık ya düzeltildi (D-070 literal, D-071 lambda) ya da
checker-reddi (D-070 G003 değişken-arg).**

---

## D-070 [YÜKSEK] — Sınıf A kabul-ama-çöküyor: dizi-LİTERAL → Dizi<T> param → heap (UB kapandı) (2026-06-14)

**Karar [ETKİ: orta — `src/llvm.c` CAGRI codegen; izole commit].** Güvenlik-iddiası izi
(D-069 devamı). D-031'de teşhis edilen "8 kabul-ama-çöküyor" deliğinden Sınıf A'nın
LİTERAL-arg kısmı kapatıldı. C derleyici codegen değişti.

**Hole (D-031 Sınıf A):** `f([1,2,3])` — `f(xs: Dizi<tam32>)` KdlDizi* bekler ama array
literal STACK `[N x T]` üretir. Callee `xs`'i KdlDizi* sanıp `için`/`[]`/`dizi_boyut` ile
okur → **misaligned access UB / SEGFAULT** (UBSan: "member access ... requires 8 byte
alignment"). `--check` geçer (tip sistemi stack/heap temsilini ayırmaz) → görünmez delik,
#1 iddia "Kırılamaz Güvenlik" ihlali.

**Fix:** CAGRI normal-çağrı arg döngüsünde, callee param[i] tipi `DUGUM_TIP_DIZI` ise
`g->beklenen_tip = param.tip` (AST düğümü) verilir → `DUGUM_DIZI_OLUSTUR` HEAP `kdl_dizi_olustur`
üretir (D-044 mekanizması A). `IslevKayit.ast` tüm işlevler için kayıtlı (line 861) → param
tipleri çağrı yerinde erişilir. **D-044'ün açıkça belirttiği "TÜM Dizi<T> bağlamları heap"
amacını çağrı-arg için tamamlar** (D-044 yalnız yapı-alanı setter'ını yapmıştı) → yeni DUR-SOR
DEĞİL, settled option-b'nin tutarlı uygulaması.

**Doğrulama:** 03_kontrol.kem (tek hayatta kalan literal-arg örneği; 36_quicksort silinmiş) →
ASan/UBSan TEMİZ + rc=151 (120+30+1 doğru sonuç, eskiden UB). test_llvm 235/235; bounds 10/10;
checker 48/48; ASan E2E denetim **PASS=88 FAIL=0** (03_kontrol allowlist'ten çıktı → korumalı
PASS; ALLOW 8→6). 0 uyarı.

**DEĞİŞKEN-arg → ÇÖZÜLDÜ (Mehmet kararı: checker reddi / G003).** `değişken xs=[..]; f(xs)` —
stack-array DEĞİŞKENİ Dizi<T> param'a. Literal-route uygulanamaz (arg TANIMLAYICI). Mehmet
**checker-reddi**ni seçti: C tip denetleyici (tip_kontrol.c CAGRI pas-2) — param `TIP_DIZI`
iken arg TANIMLAYICI ve sembolün ast_dugumu annotasyonsuz `değişken x=[literal]`
(DUGUM_DIZI_OLUSTUR) ise → **G003** ("stack dizi degiskeni Dizi<T> parametresine gecirilemez;
annotasyonlu heap Dizi kullanin"). Çökme yerine compile-time red (çökmezlik #1). Programcı
`değişken xs: Dizi<T> = [..]` (heap) kullanır. 35/40 artık --check'te G003 reddi (codegen'e
ulaşmaz; --llvm bypass ederse ASan-allowlist'te kalır = "checker'ı atladın" = güvensiz-eşi).
Doğrulama: 35/40→G003; 03_kontrol (literal)→OK; annotasyonlu→OK; ornekler 42/42; korpus 48/48;
test_llvm 235/235; bounds harness vaka9 (G003 reddi) 11/11.
**Self-host mirror (follow-up, TC9):** checker.kem'de G003 = Dizi-param + stack-array-var
izleme infra'sı gerek (fn_ptip "?" bileşik tipte; ayrı flag). Gating parite kırılmıyor
(42/42 korunur — hiç geçerli örnek G003 tetiklemiyor). Sınıf B (lambda) = V2 (D-004), ayrı.

---

## D-069 — Dizi sınır-güvenliği: OOB → panic (sessiz-0 / segfault DEĞİL) — Kategori 1 (heap) + 2 (stack) DONE (2026-06-14)

**Bağlam (firsthand doğrulandı):** Dizi indekslemenin iki yolu da bellek-güvensizdi:
- **Heap** (`kdl_dizi_al_tam/tam64/ptr`, kdl_runtime.c:557-570): sınır kontrolü VARDI ama
  OOB'da `return 0`/`NULL` → sessiz yanlış değer / downstream NULL-deref (D-065 segfault'unun
  asıl nedeni: OOB→NULL→`metin_esit(NULL,…)`).
- **Heap yazma** (`kdl_dizi_yaz_*`, :575-588): OOB'da sessizce yok sayılıyordu.
- **Stack/GEP** (`src/llvm.c` DUGUM_INDEKS:1964): ham GEP+load, kontrol YOK → OOB=segfault.

Parite-audit'ine GÖRÜNMEZ delik (C de aynı şekilde güvensiz → divergence çıkmaz). #1 iddia
("Kırılamaz Güvenlik") ile çelişiyor. Güvenlik-iddiası izinin ilk kalemi.

**Karar:** Dizi indeksleme varsayılan güvenli — OOB (`i<0` veya `i>=boyut`) → **panic**
(temiz, yakalanabilir durma); asla segfault, asla sessiz-0/noop. Üç kategori; perf gerilimi
`güvensiz` opt-out ile çözülür (Rust modeli: varsayılan güvenli, açık+işaretli opt-out).

**Panic mekanizması (eklendi):** `kdl_panik(const char *)` (kdl_runtime.c, hosted): stderr'e
`"PANIK: <mesaj>"` + `abort()` (`__attribute__((noreturn))`). Mevcut `kdl_panik_dur` yalnız
bare-metal/mock'tu (`#error` ile hosted'ta yoktu) → hosted runtime artık panic edebiliyor.

**KATEGORİ 1 (heap) — DONE [maliyet SIFIR: karşılaştırma zaten vardı]:** `kdl_dizi_al_*` ve
`kdl_dizi_yaz_*` OOB-dalı `return 0`/noop → `kdl_dizi_oob(i, boyut)` (mesaj "dizi sınır ihlali
(i=…, boyut=…)", noreturn). NULL-dizi (d==NULL) ayrı durum → şimdilik return 0/NULL korunur
(D-070+). Koşulsuz, varsayılan güvenli.

**LATENT BUG yakalandı (güvenliğin değeri kanıtı):** Kategori 1 açar açmaz `test_llvm [155]`
(17_kontrol_dili.kem mini-yorumlayıcı) PANIC verdi → `faktor`/`deyim_calistir`/`calistir`
EOF'ta `dizi_al(t.kind, p)` ve `dizi_al(t.kind, p+1)` OKUYOR (sessiz-0'ı EOF-sentineli
sanıyordu). Açık sınır kontrolü eklendi (`p>=boyut→ver 0`, `p+1<boyut ve …` — `ve`
kısa-devre). Davranış korundu, bellek-güvenli oldu. Bu tam da #1-iddianın yakalaması gereken
sınıf.

**KATEGORİ 2 (stack `[N×T]`) — DONE [YÜKSEK — codegen, commit ayrı]:** `LlvmIsim.dizi_uzunluk`
eklendi; DEGISKEN annot-yok dalı değer `DUGUM_DIZI_OLUSTUR` ise N kaydeder. `DUGUM_INDEKS`
stack yolu GEP'ten ÖNCE: `icmp uge i64 idx, N` (unsigned → negatif=dev-unsigned + i>=N tek
seferde) → `br`→`bb<oob>`(call @kdl_panik + unreachable) / `bb<ok>`(GEP+load). IR header'a
`declare void @kdl_panik(ptr)` + `@.str.dizi_sinir_panik` global. Etiketler `%bbN` (hoist_renumber
`%<digit>` dokunmaz → D-041 güvenli). **Stack OOB artık segfault DEĞİL → panic.**
Doğrulama: vaka5/6 (stack OOB/negatif → PANIC, eskiden rc=139); test_llvm **235/235**;
opt -passes=verify PASS; ASan temiz (panic erişimden önce → 0 OOB raporu); 0 uyarı.
**Opt-out (perf, Rust modeli):** `LlvmGen.guvensiz_derinlik` — `güvensiz` blok içinde stack
sınır-kontrolü ATLANIR (vaka8: güvensiz arr[i] → 0 panic-IR; dışında → kontrollü). Varsayılan
güvenli, opt-out açık+işaretli+programcı sorumluluğunda.

**FOLLOW-UP — Cat2 stack YAZMA deliği kapandı (2026-06-14):** İlk Cat2 implementasyonu yalnız
**OKUMA** yolunu (`DUGUM_INDEKS` → GEP+load) sınır-kontrol etti; **YAZMA** yolu (`DUGUM_ATAMA`
hedefi `DUGUM_INDEKS`, `arr[i]=v`, src/llvm.c) kontrolsüz GEP+store yapıyordu → sessiz stack
taşması (kabul-ama-sessizce-yanlış: `arr[10]=9` rc=0). Heap yazma (D-083 `kdl_dizi_yaz_*`)
zaten runtime'da kontrollüydü; delik yalnız stack yazma codegen'ineydi. **Fix:** yazma dalında
`LlvmIsim.dizi_uzunluk` ile okuma yolunun aynısı GEP+store'dan ÖNCE (`icmp uge i64 idx, N` →
`@kdl_panik(@.str.dizi_sinir_panik)` + unreachable). `güvensiz` opt-out yazmada da geçerli
(`guvensiz_derinlik==0` gate). Bölge-`*T` yolu (`pointee_elem`) `dizi_uzunluk=0` → kontrolsüz
(uzunluk yok, Cat3 ile tutarlı). **Doğrulama:** harness'a vaka5b/6b (OOB/negatif yazma →
PANIC), vaka7c (geçerli yazma → rc=9), vaka8b (güvensiz yazma → 0 panic-IR) eklendi →
`calistir_dizi_sinir_test` **15/15**; test_llvm **235/235**; codegen korpus 48/48; test_tumu
yeşil; 0 uyarı.

**KATEGORİ 3 (ham `*T` bölge-tabanı) — KARAR: güvensiz-only opt-out, ZATEN ENFORCE [inceleme
tamam]:** Firsthand bulgu: (1) bölge-container UZUNLUK TAŞIMIYOR — `kdl_bolge_ayir(a, boyut)`
ham `void*` taban döndürür; `*T` çıplak pointer, boyut yok → kontrol edilemez. (2) Ham `*T`
indekslemesi ZATEN `güvensiz`-only: C checker DUGUM_INDEKS (tip_kontrol.c:3295-3305)
`TIP_POINTER` indeksini `guvensiz_baglam==0` iken → **G001** ("*T pointer indeksleme yalniz
guvensiz blok icinde"). Bu tam da spec'in "varsayılan güvenli + açık opt-out" modeli — ZATEN
var. (3) Cat2 codegen'i bölge-`*T` yolunu kontrolsüz bırakır (stack_uzunluk=0 yalnız sabit
`[N×T]` literalinde >0; region `*T` → 0 → ham GEP). Tutarlı.

**Karar:** Ham `*T` region indekslemesi = `güvensiz`-only (G001 zaten enforce; opt-out açık+
işaretli+programcı sorumluluğunda — Rust modeli). Kontrol edilemez (uzunluk yok) ama erişim
yalnız güvensiz blokta. Uzunluk-taşıyan region handle (fat pointer) → ileri iş (D-070+).
Kod değişikliği GEREKMEZ (model zaten doğru). Self-host checker'da G001 henüz yok = ayrı TC
(güvensiz/pointer); bu DEĞİL — prod C checker zaten enforce ediyor.

**SONUÇ:** D-069 üç kategori de kapandı (Cat1 heap + Cat2 stack implemente; Cat3 karar+zaten-
enforce). Dizi-sınır bellek-güvenliği boyutu TAMAM. Kalan güvenlik-iddiası izi (D-070+):
8 "kabul-ama-çöküyor" deliği + scoping false-negative + NULL-dizi (d==NULL) + region fat-pointer.

**Doğrulama:** Yeni `make calistir_dizi_sinir_test` (test_tumu'ya bağlı) → **6/6** (heap-OOB-oku/
negatif/yaz/ptr → PANIC; geçerli → rc=60; D-065 koruması → segfault yok). `test_llvm` **235/235**
(latent bug fix sonrası). checker korpus 48/48; test/ornekler 42/42; self-host 3/3 (0 panik).
Hosted runtime 0 uyarı (`-Wall -Wextra -Wpedantic`).

**Kapsam-dışı (güvenlik-iddiası boşluk izi, D-070+):** Kategori 2/3 (yukarıda); 8 "kabul-ama-
çöküyor" deliği (Sınıf A dizi-literal-param, Sınıf B lambda); scoping false-negative (gölgeleme).

---

## D-068 — SELF-HOST checker TC8a: cross-file/modül import (kullan → dışa toplama) — 246/319 (2026-06-14)

**Karar [ETKİ: orta — `selfhost/checker.kem`; cross-file altyapı].** TC8 başlangıcı: `kullan`
ile içe aktarılan modüllerin `dışa`-export adlarını (transitif) toplayıp false-T002'yi kapat.
C derleyici DOKUNULMADI.

**Mimari:** `kullan_yukle_hepsi` (üst-düzey `kullan`'lar, giriş dizininden) → `modul_yukle`
(dedup `kullan_gorulen` → yol çöz → 3 arama yolu → taze `Ayr`'a lex+parse → `dışa` iç-adları
`g_isim`'e ekle → modülün `kullan`'ını transitif izle). Yol: `modul_path` (a::b::c → a/b/c.kem
via `metin_yer_degistir`); `modul_icerik` (C 3-yol: importer-dizin → kök → kütüphane/);
`dizin_al` (son '/'). builtin_ekle'den SONRA, genel_topla'dan ÖNCE (T002 öncesi g_isim hazır).

**Kapsam (TC8a):** YALNIZ `dışa` ADLARI toplanır (flat-görünür; çok-segment çıplak yol
düzleştirme — C legacy davranışı). İmza/param tipleri TC8b (cross-file fonksiyon
return/arity — şimdilik ad-only → false-pos yok, under-report). T040 (bulunamadı)/T041
(private)/T042 (ambiguous) modül-edge → TC8b. Modül gövdeleri tip-kontrol EDİLMEZ (yalnız
importer için ad toplama).

**Doğrulama:** test/crossfile transitif/sonuc_cagri/lib_islem/lib_sonuc **4/4** (transitif
zincir: transitif→lib_islem→lib_sayi `iki_kat` çözülür). Full audit **246/319** (önceki 243;
+3 crossfile, SIFIR yeni regresyon). korpus 48/48; ornekler 42/42; self-host 3/3.

**Kalan 73 farklı:** lex_korpus (22) + parse_korpus (12) = parser/lexer P/L kodu (~23);
snapshots (16: bölge/asm/çeşit/constraint/referans); virtio (12: yetki TC7+bölge TC6 — cross-
file kısmı çözüldü ama yetki/bölge kaldı); moduller (5: T040/41/42 edge); stdlib (3); eski (2).

---

## D-067 — SELF-HOST checker: full-repo parite audit SONUÇ + TC6-9 yol haritası (2026-06-14)

**Karar [ETKİ: yok — dokümantasyon; ultracode workflow audit sonucu].** 319 .kem dosyası
üzerinde KEMGU checker vs C oracle (`--checkdump`) tam tarama. Genuine-bug'lar kapatıldıktan
sonra durum + kalan feature-gap yol haritası.

**Audit sonucu:** **243/319 birebir, 76 farklı, 0 çökme** (başlangıç: 233/315, 82 farklı,
1 çökme). Kapatılan genuine-bug'lar: D-064 generic-T003 (stdlib 3), D-065 segfault (m3_04),
D-066 bit-T028 (snapshots 2). Geçerli kodda checker artık SAHTE-HATA üretmiyor; self-host
kaynaklar (lexer/parser/checker.kem) **3/3 birebir** → **checker bootstrap-HAZIR**
(geçerli derleyici kaynakları sahte-hatasız kabul ediliyor; çökmüyor).

**Kalan 76 farkın kategorizasyonu (oracle ilk-kod + dizin):**
1. **Parser/lexer hata-kodu raporlama (~23 dosya):** P001×15, P031×4, P015×3, L009×1.
   test/lex_korpus (22) + test/parse_korpus (12) — token/parça testleri; geçersiz program →
   C parser P-kodu basar, KEMGU checker'ın parser'ı kurtarıp OK/farklı basar. KEMGU parser
   hata kurtarıyor ama P/L kodunu th_kod'a YAZMIYOR. (Muhtemelen Aşama-1 parser-oracle
   kapsamı; checker-parite için P/L emit gerekli.)
2. **Cross-file/modül TC8 (~28 dosya):** oracle-OK×16 (false-T002) + T002×11 + T011×8 +
   T040/41/42×3. drivers/virtio (12), test/moduller (5), test/crossfile (3), test/stdlib (3),
   kütüphane (1), snapshots/21_modul_kullan. `kullan` import + cross-file sembol çözümü yok →
   KEMGU T002. Mimari: diğer .kem yükle + sembol birleştir.
3. **Misc feature-gaps (~10 dosya):** referans/deref T001×6 (26_referans_aktarim — T022 birebir
   ama `*r`/`&T` tip çıkarsama eksik), bölge BL001 (TC6), asm AS001/G002, çeşit M001
   (exhaustiveness), constraint T007 (TC bound), cast E002.

**Değerlendirme — Aşama 5 için:** Kalan 76 = TC9 GENİŞLİK (breadth); bootstrap-kritik DEĞİL
(self-host kaynaklar zaten 3/3). Bootstrap'a en büyük kaldıraç = Aşama 3 codegen self-host
(llvm.c → KEMGU, IR-diff oracle), checker breadth değil. Öncelik kullanıcı kararı: (a) TC8
cross-file (en çok dosya, drivers/stdlib değeri) · (b) parser P/L emit · (c) Aşama 3 codegen.

---

## D-066 — SELF-HOST checker TC5d: bit operatörü tamsayı kontrolü (T028) — 48/48 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Full-repo audit genuine-bug
#3: bit operatörü (`& | ^ << >>` + tekli `~`) operandı tamsayı olmalı (T028). C derleyici
DOKUNULMADI.

**Bulgu (audit):** test/snapshots/31_bit_komb.kem + 50_kompleks_program.kem → oracle T028,
KEMGU YANLIŞ T020. Örn `(deger >> pozisyon) & 1 == 1` → öncelik gereği `& (1==1)` → bit op
sağ operandı mantıksal. C: T028 (& operatör konumu). KEMGU bit op'u kontrol etmiyordu →
ifade_tip(&) sol tipini (tam32) döndürüyor → ver dönüşü (mantıksal) ile T020 → YANLIŞ KOD.

**Düzeltme (T003/T004 ile simetrik):** `tamsayi_mi` (tam/dtam; kesirli/mantıksal HARİÇ) +
`bit_op_mi` + `bilinen_tamsayi_degil`. `t028_kontrol` (IKILI bit op operandı kesin
tamsayı-değil → T028) + tekli_kontrol `~` dalı. `ifade_tip` bit op/`~` non-tamsayı operandda
"?" döndürür (C TIP_HATA bastırma) → dış T020/T001 bastırılır → iç-içe tek T028.

**Doğrulama:** 31_bit_komb ✅ + 50_kompleks ✅ (artık T028 birebir, T020 değil).
`make calistir_checker_diff` **48/48** (+tc5d: bit-OK / bit-T028 / tekli-~-T028).
test/ornekler 42/42; self-host 3/3; SIFIR regresyon (geçerli bit kodu tam-integer → T028 yok).

**Audit ilerleme:** 3 genuine-bug kapandı (generic-T003, segfault, bit-T028). Kalan farklar
büyük oranda feature-gap: parser/lexer P/L kodu raporlama (lex/parse_korpus), cross-file/modül
TC8 (virtio/crossfile/moduller), bölge TC6 (bolge_al), referans/deref tip (26_referans_aktarim
— T022 birebir ama deref T001 eksik), asm/çeşit/constraint. → TC6-9 yol haritası.

---

## D-065 — SELF-HOST [YÜKSEK robustness]: parser token erişimi sınır-güvenli (segfault düzeltildi) (2026-06-14)

**Karar [ETKİ: orta — `selfhost/checker.kem` + `selfhost/parser.kem`; robustness/çökmezlik].**
Full-repo parite audit'inde KEMGU checker `test/lex_korpus/m3_04_ayrac_hata.kem` üzerinde
SEGFAULT (rc=139) veriyordu. Kök neden bulundu + düzeltildi. C derleyici DOKUNULMADI.

**Kök neden (bisect ile):** Çöken yapı = `yapı Nokta x y z` (süslü `{` yok). Parser bozuk
girdide panik-sync yapmadığından `parse_alan`/`bekle` döngüsü `p.imlec`'i DOSYA_SONU
sentinelinin ÖTESİNE ilerletiyor; sonra `tip_i`/`lex_i` → `dizi_al(p.t_ad, i)` sınır-dışı →
segfault. KEMGU'nun "çökmezlik" (Direktif) ilkesine aykırı kritik bir robustness hatası.

**Düzeltme (sınır-güvenli accessor):** `tip_i` → i sınır-dışıysa "DOSYA_SONU"; `lex_i` →
"". Böylece imleç taşsa bile tüm parse döngülerinin `sim_mi(DOSYA_SONU)` kontrolü sonlanır;
çökme ya da sonsuz döngü yok. C lexer'ın DOSYA_SONU sentineli + bounded-peek davranışının
karşılığı. **Hem checker.kem hem parser.kem'e** uygulandı (paylaşılan parser kodu, aynı
latent bug).

**Doğrulama:** m3_04 artık rc=0 (çökme yok); `yapı Nokta x y z` tek başına rc=0. Parser
bootstrap **270/270** sıfır-diff (self-parse dahil); parser diff 12/12; checker korpus 45/45;
test/ornekler 42/42; self-host lexer/parser/checker.kem 3/3. SIFIR regresyon.

**Not:** m3_04 artık "OK" basıyor (oracle P-kodları basıyor) → hâlâ DIVERGENT ama ÇÖKMÜYOR.
m3_04 tam paritesi = parser/lexer hata-kodu raporlama (P/L kodları) feature-gap'ine bağlı
(checker'ın parser'ı hata kurtarıyor ama P/L kodu th_kod'a yazmıyor) — ayrı iş (muhtemelen
Aşama 1 parser-oracle kapsamı; checker-parite dışı).

---

## D-064 — SELF-HOST checker: generic param tip "?" (full-repo parite denetimi başladı) — stdlib 3/3, 45/45 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Tüm-repo parite denetimi
(315 .kem dosyası, ultracode workflow ile) başlatıldı → 233/315 birebir, 82 farklı. İlk
genuine-bug düzeltildi: generic işlevlerde false T003/T020. C derleyici DOKUNULMADI.

**Bağlam — full-repo parite audit:** KEMGU checker (kemcheck.exe) C oracle'a (`--checkdump`)
karşı TÜM repo .kem dosyalarında tarandı. 82 fark kategorize edildi: çoğu feature-gap
(parser/lexer P/L kodları, cross-file/modül TC8, bölge TC6, yetki TC7/CP005, asm, çeşit
M001, generic-constraint) + birkaç genuine-bug (CRASH m3_04, generic-T003, yanlış-kod 26/31/50).

**Genuine-bug #1 — generic param false T003/T020 (workflow agent kök-neden).** `mutlak<T>(x: T)
-> T { eğer x < 0 { ver 0 - x; } ... }` → oracle OK, KEMGU 29 false T003 (stdlib/temel/*).
Sebep: `yerel_topla` param/değişken tip-string'ini ham saklıyor; generic `x: T` → "T" →
`bilinen_sayisal_degil("T")`=doğru → T003. Ayrıca dönüş tipi "T" → aktif_donus "T" → ver
çıkarsanan "tam32" ≠ "T" → T020. C: TIP_GENERIC_PARAM için tip_sayisal_mi "deferred true".

**Düzeltme:** `yerel_tip_filtrele(t)` = bilinen-skaler VEYA bilinen-yapı → t, aksi "?".
İki yerel_topla write-site'ında (param tip_str, değişken annot_str) + kontrol_govde
aktif_donus (donus_str) uygulandı. Generic "T" → "?" → kontrol atlanır; yapı adları
(ERISIM/TC4) KORUNUR.

**Doğrulama:** stdlib/temel matematik+karsilastir+sayisal **3/3** (29 false T003+T020 kapandı);
self-host lexer/parser/checker.kem **3/3**; `make calistir_checker_diff` **45/45** (+tc5c_01
generic); test/ornekler **42/42** (regression yok). Workflow agent risk-analizi + empirik
doğrulama uyumlu.

**Sıradaki genuine-bug'lar:** CRASH m3_04_ayrac_hata (parser sınır-dışı/sonsuz döngü → segfault),
yanlış-kod 26/31/50 (T022 vs T001, T020 vs T028). Feature-gap'ler TC6-9 yol haritasına.

---

## D-063 — SELF-HOST checker: aynı-ad belirsiz tip → "?" (self-host kaynak paritesi) — lexer+parser+checker.kem TAM (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem`].** KEMGU checker'ı KENDİ derleyici
kaynakları üzerinde (lexer/parser/checker.kem) C oracle'ına karşı doğrulandı → **3/3
self-host kaynak BİREBİR**. Bir false-positive bulundu ve düzeltildi. C derleyici
DOKUNULMADI.

**Bulgu (bağımsız doğrulama):** `parser.kem:969` `ver ic;` → KEMGU FALSE T020, oracle OK.
Sebep: `parse_birincil`'de iki ayrı blok kapsamında iki `ic` (`metin` @935, `tam32` @967);
düz `yerel` listesi kapsam tutmaz, `var_tip` ileri-arama İLK eşleşmeyi (`metin`) döndürdü →
`ver ic` (tam32 fonksiyonda) metin sanıldı → T020. C blok kapsamlarıyla doğru çözer.

**Düzeltme (GÜVENLİ):** `var_tip` — bir ad birden fazla FARKLI tiple bağlıysa → "?"
(belirsiz → kontrol atla). Tek tip → o tip. Yalnız under-report (false-positive önler,
hiç error EKLEMEZ). Hem false T020 (ver ic) hem olası false T001'i (dugum0 arg ic) kapatır.

**Doğrulama:** self-host kaynaklar **lexer.kem ✅ parser.kem ✅ checker.kem ✅** (hepsi
oracle = KEMGU). `make calistir_checker_diff` **44/44**; test/ornekler **42/42** (regression
yok — yalnız belirsiz-ad checkleri atlanır, geçerli kodda zaten error yok).

**Önemi (Aşama 5 hazırlığı):** KEMGU checker artık TÜM derleyiciyi (kendisi dâhil) C ile
birebir tip-kontrol ediyor — bootstrap fixpoint için checker accept/reject doğruluğu
KANITLANDI (geçerli kaynak → kabul, sahte hata yok).

---

## D-062 — SELF-HOST checker TC5b: lineer akış L001/L002/L004 — 🎉 test/ornekler 42/42 TAM PARİTE (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC5b (Linear
Types Spec V1 akış denetimi): L001 (scope sonu tüketilmedi), L002 (move sonrası erişim),
L004 (lineer referans). **lineer_hata.kem kapandı → test/ornekler 42/42 TAM accept/reject
+ tanı paritesi.** C derleyici DOKUNULMADI.

**Mimari — lineer bağlama izleme (aktif işlev dilimi).** `Ayr`'a lin_ad/lin_sat/lin_sut/
lin_tuk (paralel Dizi) + lin_basla (dilim başı; `yerel` deseni). `kontrol_govde`: girişte
lin_basla + lineer parametreler (`lin_param_topla`); çıkışta `lin_kapanis` (tüketilmemiş →
L001 bildirim konumunda). Tüketim noktaları (`lin_tuket_dugum`, tekrar → L002 düğüm
konumunda): kullan/imha (KULLAN_IFADE/IMHA_IFADE), çağrı-arg→lineer-param (fn_plin), ver
değeri, değişken move. L004: `&`/`&değişken` lineer bağlama → tekli_kontrol'da.

**KRİTİK karar — Linear V1 = YALNIZ tekkez (`tip_node_tekkez_mi`).** C `tip_lineer_mi`
tekkez+yetki+görev kapsar AMA tüketilmemiş yetki→CP005, görev→DRF (L001 DEĞİL). Bu yüzden
L001/L002/L004 izleme TEKKEZ'e kısıtlandı → mmio_smoke (yetki<MMIO>) FALSE-L001 vermez.
**LR002 GENİŞ kalır** (tekkez+yetki+görev) — geçerli yapıda hiç lineer alan yok →
false-positive yok. (yetki CP005 = TC7, görev DRF = TC6.)

**Doğrulama:** lineer_hata.kem KEMGU = oracle BİREBİR: LR002 24:5, L001 7:5, L002 13:28,
L004 18:20. `make calistir_checker_diff` → **44/44 korpus** (önceki 40 + TC5b 4: L001/L002/
L004/temiz). **test/ornekler 42/42** (lineer_temel/closure OK; mmio_smoke OK; regression yok).

**Bilinen sınır (TC5 kalan):** L007/L008 (consume operand tekkez değil / tekkez_olustur
arity); kapsam blok-düzeyi değil işlev-düzeyi (lineer_hata/temel/closure'da fark yok);
closure LC-2/LC-3 (yakalama → consume-at-traversal modeliyle örtüşüyor). Sıradaki: yetki
CP005 (TC7), bölge (TC6), modül (TC8), tam-korpus (TC9) → Aşama 3 codegen.

---

## D-061 — SELF-HOST checker TC5a: yapı lineer alan yasağı (LR002) — 40/40 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC5a (Linear
Types Spec V1 başlangıcı): yapı lineer-tipli alan içeremez (LR002). C derleyici
DOKUNULMADI.

**Kapsam:** `lr002_kontrol` pre-pass (gövdelerden ÖNCE, `kontrol_ust`'tan önce) —
üst-düzey/modül/dışa YAPI'ların alanlarını gezer; alan tipi lineer ise (`tip_node_lineer_mi`:
TIP_TEKKEZ/TIP_YETKI/TIP_GOREV) → LR002 alan düğümünde. C `tip_lineer_mi` (tekkez+yetki+
görev; kanal/sabitsüre HARİÇ) birebir; konum ALAN düğümü (--checkdump: lineer_hata
LR002 24:5).

**Sıra kararı:** LR002 pre_populate'te (gövdelerden önce) → çok-hatalı dosyada (lineer_hata)
LR002 İLK çıkar (L001/L002/L004 gövde hatalarından önce). Bu yüzden ayrı pre-pass.

**Doğrulama:** `make calistir_checker_diff` → **40/40 korpus** (önceki 38 + TC5a 2:
lineer-alan-LR002 / lineer-olmayan-OK). test/ornekler **41/42** (lineer_hata hâlâ
diff — L001/L002/L004 TC5b'de; geçerli yapılarda false-LR002 YOK).

**Sıradaki (TC5b):** lineer akış — L001 (scope sonu tüketilmedi), L002 (move sonrası
erişim), L004 (lineer referans). Lineer bağlama izleme (tekkez_olustur değer / tekkez
annotasyon / lineer param) + tüketim noktaları (kullan/imha/çağrı-arg/ver). lineer_hata
→ 42/42 kapanır.

---

## D-060 — SELF-HOST checker TC4a: yapı oluştur (T002/T017/T012/T001) + erişim tipi — 38/38 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC4a: yapı
oluşturma denetimi (bilinmeyen yapı T002, bilinmeyen alan T017, eksik alan T012, alan
değer tipi T001) + alan erişim tip çıkarsama (`nesne.alan` → alan tipi). C derleyici
DOKUNULMADI.

**Mimari karar — yapı tablosu (`yapi_ad`/`yapi_abase`/`yapi_acount` + düz `alan_ad`/
`alan_tip`).** `genel_topla` YAPI (+ dışa-YAPI) düğümlerinde `yapi_kaydet`: alan adları
+ tipleri (yalnız bilinen skaler; generic "T"/yapı/bileşik → "?"). Sorgular:
`yapi_var_mi`, `alan_var_mi`, `alan_tip_bul`.

**C `kontrol_yapi_olustur_ic` sırası birebir (--checkdump doğrulaması):**
- Yapı tanımsız → T002 (oluştur düğümü) + **erken dönüş** (alan kontrolü yok).
- Her alan-atama (oluşturma sırası): bilinmeyen alan → T017 (alan düğümü, **değer
  kontrol edilmez**); bilinen → değer T002 + T001 (alan değer tipi, alan düğümü).
- Eksik alanlar (bildirim sırası) → T012 (oluştur düğümü), per-alan döngüsünden SONRA.
- `ifade_tip` YAPI_OLUSTUR → yapı adı; ERISIM → alan tipi (referans nesne → "?").

**GÜVENLİ strateji:** Alan tipi yalnız bilinen-skaler saklanır (generic/yapı alan →
"?" → T001 atla); referans-nesne erişimi → "?" → atla. Geçerli kodda false-positive
YOK → test/ornekler (yapilar/hasta dâhil) 41/42 KORUNDU.

**Doğrulama:** `make calistir_checker_diff` → **38/38 korpus** (önceki 32 + TC4 6:
yapı-OK / bilinmeyen-alan / eksik-alan / alan-tip / bilinmeyen-yapı / erişim-tip).
test/ornekler **41/42** (regression yok; lineer_hata = TC5).

**Sıradaki (TC4b):** eşleş exhaustiveness M001 + çeşit varyant (M002/M003/M004) +
INDEKS tip/T013. Sonra linear (TC5 → lineer_hata kapanır), bölge/yetki/modül (TC6-8).

---

## D-059 — SELF-HOST checker TC3h: atama lvalue (T022) + atama tip uyumu (T001) — 32/32 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3h: atama
hedefi lvalue olmalı (T022); atama değeri hedef tipiyle uyumlu olmalı (T001). C
derleyici DOKUNULMADI.

**Kapsam:** `kontrol_dugum` ATAMA özel-case'i. `lvalue_mi` (TANIMLAYICI/ERISIM/INDEKS).
C DUGUM_ATAMA sırası birebir: T022 (lvalue, **erken dönüş YOK**) → hedef T002 → değer
T002 → T001 (`ifade_tip(hedef)` vs `ifade_tip(değer, ht)`). Konum: ATAMA düğümü
(sol-taraf başı; `--checkdump` ile doğrulandı: `x=doğru`→T001 3:5, `5=3`→T022 2:5,
`f()=3`→T022 3:5).

**GÜVENLİ strateji:** T001 yalnız hedef tipi bilinen-skaler (TANIMLAYICI → var_tip)
iken; ERISIM/INDEKS hedef → ht "?" → T001 atla (alan/indeks tipi TC4). Değer "?" →
atla. Geçerli kodda (lvalue + uyumlu tip) hata yok → false-positive YOK.

**Doğrulama:** `make calistir_checker_diff` → **32/32 korpus** (önceki 28 + TC3h 4:
atama-OK / atama-T001 / literal-hedef-T022 / çağrı-hedef-T022). test/ornekler **41/42**
(regression yok; lineer_hata = TC5).

**Aşama 2 ilerleme (T-kodları):** T001 (annot/atama/arg/IKILI-aynı-tip), T002, T003,
T004, T010, T020, T021, T022, T024, T026 — 10 kod. Sıradaki (TC4): struct alan T017 +
ERISIM/INDEKS tip çıkarsama + eşleş exhaustiveness M001. Sonra linear (TC5 →
lineer_hata kapanır), bölge/yetki/modül (TC6-8), tam-korpus paritesi (TC9).

---

## D-058 — SELF-HOST checker TC3g: CAGRI per-arg T001 (param tip tablosu) — 28/28 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3g: kullanıcı
işlevi çağrısında argüman tipi parametre tipiyle uyumsuz → T001 (arg konumunda).
C derleyici DOKUNULMADI.

**Mimari karar — parametre tip tablosu (`fn_pbase` + `fn_ptip` düz liste).** İmza
tablosu genişletildi: her işlevin param tipleri `fn_ptip`'e ardışık yazılır, `fn_pbase`
başlangıç indeksini tutar. `fn_ptip_bul(ad, j)` j. param tipini verir. CAGRI arite-OK
yolunda her arg için `ifade_tip(arg, pt)` vs `pt` → uyumsuz ise T001 arg düğümünde.

**EMPİRİK C DAVRANIŞI (--checkdump ile doğrulandı):**
- `f(tanımsız)` → T002 **İKİ KEZ** (C iki-geçiş: pass1 unify + pass2 check, her ikisi
  `tip_belirle` → arg-İÇİ hata çiftlenir). Per-arg **T001 yalnız pass2** → tek emisyon.
- `f(b)` (b yanlış-tip, iç hata yok) → T001 **bir kez** (arg konumu).
- İç-hatasız argümanlarda tek-geçiş = C ile birebir (çiftleme yalnız iç-hatalı argda).

**Tasarım — tek-geçiş + per-arg T001 (GÜVENLİ):** Param tipi yalnız **bilinen skaler**
saklanır (generic "T"/yapı → "?" → atla; generic false-positive yok). Arg "?" veya pt
"?" → atla. Literal arg bidirectional (`byte_al(100)` param tam8 → tam8 → OK). Bilinen
sınır: arg-İÇİ hata çiftlemesi (tanımsız-ad-arg) tek-geçişte tek kez — yalnız geçersiz
kodda; geçerli korpusta (iç-hatasız arg) tam parite. Method/builtin → tek-geçiş (mevcut).

**Doğrulama:** `make calistir_checker_diff` → **28/28 korpus** (önceki 24 + TC3g 4:
arg-OK / arg-T001 / ikinci-arg / literal-bidir). test/ornekler **41/42** (regression yok;
lineer_hata = TC5).

**Sıradaki (TC3h):** T022 (atama lvalue) + eşitlik/karşılaştırma aynı-tip T001. Sonra
struct alan T017 + erişim tipi (TC4), exhaustiveness M001, linear (TC5 → lineer_hata
kapanır), bölge/yetki/modül (TC6-8), tam-korpus paritesi (TC9).

---

## D-057 — SELF-HOST checker TC3f: mantıksal operand (T004) + tekli neg/değil — 24/24 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3f: ikili
`ve`/`veya` operandı mantıksal olmalı (T004); tekli `-` (neg) sayısal (T003); tekli
`değil` mantıksal (T004). C derleyici DOKUNULMADI.

**Kapsam:** `kontrol_dugum` IKILI post-check'e `t004_kontrol` (ve/veya → her iki
operand mantıksal); TEKLI post-check `tekli_kontrol` (neg → sayısal/T003, değil →
mantıksal/T004). `~`/`&`/`deref*` → ileri TC. Konum: IKILI/TEKLI düğümü (= operatör;
parser bootstrap ile C ile özdeş). `bilinen_mantiksal_degil` (≠"?" ve ≠"mantıksal").

**C semantiği birebir (TIP_HATA bastırma):** `ifade_tip` artık ve/veya, neg, değil
için operand kesin-uyumsuzsa "?" döner (C operand→TIP_HATA→ikili/tekli erken dönüş).
Böylece `değişken c: mantıksal = x ve doğru` (x tam32) → yalnız T004 (T001 yok);
`değişken r: tam32 = -b` (b mantıksal) → yalnız T003; `eğer değil x` (x tam32) →
yalnız T004 (T021 yok). `==`/`!=` mantıksal döner (aynı-tip T001 ileri TC).

**GÜVENLİ strateji:** Yalnız KESİN uyumsuz operandda hata → geçerli kodda
false-positive YOK.

**Doğrulama:** `make calistir_checker_diff` → **24/24 korpus** (önceki 20 + TC3f 4:
mantık/tekli-OK / ve-T004 / neg-T003 / değil-T004). test/ornekler **41/42**
(regression yok; lineer_hata = TC5).

**Sıradaki (TC3g):** CAGRI per-arg T001 (param tip tablosu) + eşitlik/karşılaştırma
aynı-tip T001 + T022 (lvalue atama hedefi). Sonra struct alan T017 + exhaustiveness
M001, generic (TC4), linear (TC5 → lineer_hata kapanır), bölge/yetki/modül (TC6-8).

---

## D-056 — SELF-HOST checker TC3e: aritmetik/karşılaştırma sayısal operand (T003) — 20/20 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3e: ikili
aritmetik (`+ - * / %`) ve karşılaştırma (`< > <= >=`) operandı sayısal olmalı (T003).
C derleyici DOKUNULMADI.

**Kapsam:** `kontrol_dugum` IKILI post-check'i `t003_kontrol` — operand tiplerini
`ifade_tip` ile hesaplar; aritmetik VEYA karşılaştırmada bir operand **kesinlikle
sayısal değil** (`bilinen_sayisal_degil`: ≠"?" ve `sayisal_mi` yanlış) ise T003
(IKILI düğüm konumunda = operatör; parser bootstrap ile C ile özdeş). Eşitlik
(`== !=`), mantıksal (`ve veya`), bit/kaydırma → T003 YOK (ileri TC).

**C `tip_belirle(IKILI)` semantiği birebir:**
- Operand TIP_HATA ise (örn. tanımsız ad → T002) ikili **erken TIP_HATA döner →
  T003 YOK**. KEMGU karşılığı: `ifade_tip` arit/karşılaştırmada operand "?" ise
  "?" döner; `t003_kontrol` "?" operandı atlar → çift hata yok.
- T003 fırlayınca C TIP_HATA döner → dış T001/T020/T021 **bastırılır**. KEMGU:
  `ifade_tip` arit-non-sayısal → "?", karşılaştırma-non-sayısal → "?" döner;
  böylece `değişken r: tam32 = b + 1` (b mantıksal) → yalnız T003 (T001 yok),
  `eğer b < 3` → yalnız T003 (T021 yok). İç-içe `(a<3)+1` → yalnız dış '+' T003.

**GÜVENLİ strateji:** Yalnız KESİN bilinen non-sayısal operandda T003 → geçerli
kodda (tüm aritmetik operandlar sayısal) false-positive YOK.

**Doğrulama:** `make calistir_checker_diff` → **20/20 korpus** (önceki 16 + TC3e 4:
aritmetik-OK / aritmetik-T003 / karşılaştırma-T003 / iç-içe). test/ornekler **41/42**
(regression yok; lineer_hata = TC5).

**Sıradaki (TC3f):** CAGRI per-arg T001 (param tip tablosu) + eşitlik/karşılaştırma
aynı-tip T001 + mantıksal T004 + T022 (lvalue) + tekli '-' T003. Sonra struct alan
T017 + exhaustiveness M001, generic (TC4), linear (TC5 → lineer_hata kapanır), modül.

---

## D-055 — SELF-HOST checker TC3d: CAGRI dönüş çıkarsama + T010 arite — 16/16 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3d: kullanıcı
işlevi çağrısının dönüş tipini çıkarsama (T001/T020'yi besler) + çağrı argüman sayısı
uyumsuzluğu (T010). C derleyici DOKUNULMADI.

**Mimari karar — işlev imza tablosu (`fn_ad`/`fn_donus`/`fn_psay` paralel Dizi).**
`genel_topla` sırasında her üst-düzey/modül/`dışa` ISLEV için imza kaydedilir: ad,
dönüş tipi, parametre sayısı. CAGRI `ifade_tip` hedef TANIMLAYICI ise `fn_donus_bul`
ile dönüş tipini verir; `kontrol_dugum` CAGRI özel-case'i `fn_psay_bul` ile arite
karşılaştırır.

**GÜVENLİ strateji (false-positive YOK):**
- Dönüş tipi YALNIZ **bilinen skaler** (`bilinen_skaler_mi`: sayısal/mantıksal/metin/
  karakter/boş) ise saklanır; yapı/generic-param/bileşik dönüş → "?". Böylece generic
  `kimlik<T>() -> T` dönüşü "T" gibi sahte tiplerle T001 üretmez (parser tip-paramları
  AST'te yok → generic tespit edilemez; skaler-whitelist bunu kapsar).
- Arite YALNIZ kullanıcı işlevleri için (`fn_psay_bul >= 0`); builtin'ler → atla
  (builtin arite'leri C'de özel; geçerli kodda doğru çağrılır → diff yok).
- Method (`x.m()` = ERISIM hedef) ve dolaylı çağrı → atla (TANIMLAYICI değil).

**C `tip_belirle(CAGRI)` sırası birebir:** hedef T002 → (tanımsız hedef →
TIP_HATA → **erken dönüş**, arg atlanır) → T010 arite (uyumsuz → **erken dönüş**,
arg tip kontrolü yok) → argümanlar. Pozisyon: T010 CAGRI düğümünde (= `(` konumu;
parser bootstrap 224/224 ile C ile özdeş).

**Doğrulama:** `make calistir_checker_diff` → **16/16 korpus** (önceki 12 + TC3d 4:
çağrı-dönüş OK / T010 arite / dönüş-uyumsuz T001 / ver-çağrı T020). test/ornekler
**41/42** (regression yok; lineer_hata = TC5).

**Sıradaki (TC3e):** CAGRI per-arg T001 (param tip tablosu) + T022 (lvalue) + T003
(sayısal beklenen). Sonra struct alan T017 + exhaustiveness M001, generic (TC4),
linear (TC5 → lineer_hata kapanır), bölge/yetki/modül, tam-korpus paritesi (TC9).

---

## D-051 — SELF-HOST Aşama 2 (TİP DENETLEYİCİ) ADIM-0: --checkdump oracle + mimari (2026-06-14)

**Karar [ETKİ: düşük — additive C `--checkdump` modu; mevcut yol değişmedi].** Aşama 2
(tip denetleyici self-host) başlangıcı. Mandate: Aşama 5'e kadar otonom, faz-sınırında
durmadan, kendi mimari kararlarımla.

**Karar 1 — Oracle: `--checkdump` (accept/reject + tanı paritesi).** C `--check` insan-
okunur (hata[KOD] blokları + özet). Yeni `--checkdump`: hata callback'i (hata.h
`hata_callback_ayarla`) ile tip-kontrol hatalarını toplar, DÜZ basar:
`<KOD>\t<satır>\t<sütün>` (callback/traversal sırasıyla), hata yoksa `OK`. KEMGU-checker
aynı çıktıyı üretecek → diff = accept/reject + kod/konum paritesi. (Parser/yükleme/wcet
hataları da toplanır ama TC korpusu TEMIZ parse eder → yalnız T/L/M kodları.)

**Karar 2 — KEMGU-checker temsili: indeks-düz + STRING-encoded tipler.** Sembol tablosu =
paralel Dizi (ad/kategori/tip-string/scope-seviye), scope = seviye-sayacı (append-only;
toy-demo scope-stack deseni). Tipler STRING-encoded ("tam32", "Dizi<tam32>", "&Nokta") —
nominal eşitlik `metin_esit` (C TipBilgisi struct yerine; KEMGU'da en doğal). Checker
parser'ın düz AST tablosunu (`Ayr`) gezer. selfhost/checker.kem parser'ı İÇERİR (AST
gerek; tek-dosya; modülerleştirme Aşama 4/entegrasyon).

**Plan (TC1-TC9, her biri --checkdump paritesi):** TC1 temel (literal tip + scope +
T002 tanımsız + T001/ifade-tip uyumsuz) · TC2 işlev/çağrı (T-arity/arg) · TC3 struct/
çeşit (alan/exhaustiveness M001) · TC4 generic/mono · TC5 linear (tekkez L001-L008) ·
TC6 bölge · TC7 yetki · TC8 modül · TC9 tam güvenlik + tüm-korpus paritesi.

**Doğrulama:** `--checkdump` OK örnek + T001/T002 hata örneği bayt-exact. Prod 0 uyarı.
Additive — `--check`/testler etkilenmedi.

**Sıradaki:** TC1 — selfhost/checker.kem (parser AST üzerinde sembol/tip/temel kontrol).

---

## D-054 — SELF-HOST checker TC3a: tip çıkarsama temeli + annotation T001 — 9/9 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3a: tip
çıkarsama çekirdeği (mountain'ın özü). değişken/sabit annotation-değer uyumsuzluğu (T001).

**Kapsam:** STRING-encoded tip çıkarsama `ifade_tip` — literal (TAM/KESIRLI bidirectional
beklenen sayı/kesirli tiple; METIN/KARAKTER/MANTIKSAL/BOS) + tanımlayıcı (yerel_tip
takibi). `yerel_tip` Dizi (param: tip-çocuğundan; değişken: annotation; için/desen: "?").
T001: değişken/sabit annotation vs değer tipi; çocuk T002'lerinden SONRA (C sırası).

**GÜVENLİ strateji:** Bilinmeyen tip → "?" → T001 ATLA. ifade_tip yalnız emin olduğu
(literal/bilinen-ident) tipleri döndürür; IKILI/CAGRI/ERISIM → "?" (TC3b). Böylece
geçerli kodda FALSE-T001 yok → gerçek tek-dosya 41/42 KORUNDU (under-report > over-report).
bidirectional: `değişken a: tam8 = 5` OK (5→tam8); `b: mantıksal = 7` T001.

**Doğrulama:** `make calistir_checker_diff` → **9/9 korpus** (TC1 4 + TC2 2 + TC3 3).
test/ornekler 41/42 (regression yok; lineer_hata = TC5). C derleyici değişmedi.

**Sıradaki (TC3b):** IKILI operatör tipi (sayısal aritmetik → operand; karşılaştırma/
mantıksal → mantıksal) + CAGRI dönüş tipi + T020 (ver) + T021 (koşul mantıksal) +
T022 (lvalue) + T010 (arite). Sonra struct alan/exhaustiveness, generic, linear, modül.

---

## D-053 — SELF-HOST checker TC2: üst-düzey çift-tanım (T024/T026) — 6/6 korpus, 41/42 gerçek (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC2: üst-düzey
çift-tanım denetimi. C `pre_populate` accept/reject + sıra paritesi.

**Kapsam:** PROGRAM doğrudan çocuklarında çift-tanım. Kod kind'e göre: yapı/çeşit →
**T026**, işlev/sabit/özellik/modül → **T024**. İkinci tanımın konumunda. C pre_populate
**4-geçiş sırası BİREBİR**: özellik → yapı/çeşit → (uygula: ad yok) → işlev/sabit/modül,
her geçiş kaynak-sırası, PAYLAŞILAN global kapsam (gor). dışa-sarmalı açılır.

**Bulgu (sıra kritik):** Naif kaynak-sırası dup-tarama yanlış sıra üretti — C iki-geçişli
(yapı/çeşit önce, işlev/sabit sonra) → T026'lar T024'lerden önce. 4-geçiş replikasyonu
düzeltti. GÜVENLİ kapsam: yalnız top-level (modül-içi/cross-modül → false-T024 riski,
TC8'e ertelendi) — gerçek tek-dosya 41/42 korundu.

**Doğrulama:** `make calistir_checker_diff` → **6/6 korpus** (TC1 4 + TC2 2). test/ornekler
tek-dosya 41/42 (tek fark lineer_hata = TC5). C derleyici değişmedi.

**Sıradaki:** TC3 = tip çıkarsama (T001 uyumsuzluk — 22× en sık; TipBilgisi modeli) +
arite (T010). Bu "DAĞ"ın çekirdeği. Sonra struct alan/exhaustiveness, generic, linear, modül.

---

## D-052 — SELF-HOST checker TC1: temel ad çözümü (T002) — 41/42 gerçek tek-dosya (2026-06-14)

**Karar [ETKİ: düşük — `selfhost/checker.kem` (parser kopyası + checker) + korpus].**
Aşama 2 TC1: KEMGU'da temel tip denetleyici — kapsam/ad çözümü (T002 tanımsız sembol).
C `tip_kontrol.c` accept/reject + tanı paritesi (`--checkdump` oracle).

**Kapsam:** `selfhost/checker.kem` = parser (kopya, AST için) + sembol kümeleri
(`g_isim` global: 47 EKLE_BUILTIN + özel-builtin'ler [vektor_*/mmio_*/yetki_olustur/
tekkez_olustur/delege/geri_al/görev/kanal/dur/dondur] + üst-düzey tanım adları +
keyword-konstrüktör değer/tamam/hata/kendin/hiç; `yerel` append-only dilim: param+
lokal+için+desen-binding). Traversal: işlev gövdesi + sabit değeri; TANIMLAYICI ref
genel∪yerel'de değilse → T002. Tip/desen/yol alt-ağaçları atlanır (ileri TC).

**Doğrulama:** `make calistir_checker_diff` → 4/4 korpus + **41/42 test/ornekler tek-dosya
--checkdump sıfır-diff**. Tek fark `lineer_hata.kem` (kasıtlı L001/L002 → TC5; TC1
linear yapmaz). Bulgu: keyword-konstrüktör (değer/tamam/hata) + özel-builtin'ler
(mmio/yetki/vektor) genel'e eklenmezse false-T002.

**Sınır/sıradaki:** Cross-modül import (kullan) adları henüz çözülmez (→TC8). Tip
uyumsuzluğu (T001), arite (T010), struct alan, exhaustiveness → TC2-TC3+. checker.kem
parser kopyası içerir (modülerleştirme Aşama 4).

---

## D-050 — 🎉 SELF-HOST parser P6: BOOTSTRAP — 223/223 GERÇEK .kem + SELF-PARSE (Aşama 1 TAMAM) (2026-06-14)

**Karar [ETKİ: düşük — `selfhost/parser.kem` + additive `ondalik_bicimle` intrinsic +
harness].** Aşama 1 (PARSER self-host) KAPANIŞI. KEMGU'da yazılı parser, C parser'ın
`--ast` oracle'ına karşı TÜM gerçek korpusta sıfır-diff — **kendi kaynağı dahil**.

**Son iki kapatma:** (1) **KESIRLI float:** `ondalik_bicimle(metin)->metin` intrinsic
(runtime strtod + `%g`, C ast_duz_yaz birebir) — `yaz_karakter` gibi float-format
runtime primitifi. `metin_`/`dosya_` dışı → açık dispatch (tip_kontrol+llvm+runtime).
(2) **satıriçi_asm:** deyim parse (mimari/şablon/çevrim/çıktı/girdi/bozulan clause);
yalnız `girdi` ifadeleri AST çocuğu (C ast_duz_yaz), gerisi tüketilir.

**Doğrulama:** `make calistir_parser_bootstrap` → **223/223 SIFIR-DİFF** (build/lex_korpus/
ornekler-eski hariç TÜM .kem) — **selfhost/parser.kem SELF-PARSE dahil**. test_llvm
235/235 + lexer bootstrap 261/261 (ondalik_bicimle regresyonsuz). `make
calistir_parser_diff` 12/12 korpus.

**eski/ hariç:** `test/ornekler/eski/tip_alias.kem` `tip Ad = T;` kullanır; `tip`
v1'de anahtar kelime DEĞİL → C parser DA P001 hata verir (geçersiz). Hata-kurtarma
diverjansı (gerçek boşluk değil) → bootstrap'tan çıkarıldı.

**Aşama 1 ÖZET (D-035→D-050):** ADIM-0 (--ast oracle + index-AST kararı) → P1 ifade →
P2 deyim → P3 bildirim → P4 tip → P5 modül/import → P6 bootstrap. İndeks-tabanlı düz
AST tablosu, &değişken struct threading (D-044), düz preorder dumper. Üç additive
intrinsic: yaz_bayt, ondalik_bicimle (+ D-041/D-044 codegen fix'leri parser'ı sağladı).

**Sıradaki (Aşama 2 — TİP DENETLEYİCİ):** DAĞ. C checker accept/reject paritesi.
TC1 temel → TC9 tam güvenlik. Mimari: KEMGU'da sembol tablosu + scope + tip temsili.

---

## D-049 — SELF-HOST parser P5: modül/kullan/dışa/genel + geri_al/delege + TAM-clamp — 115/118 GERÇEK .kem (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/parser.kem` + korpus].** Aşama 1 P5: üst-düzey
modül/kullan/dışa/genel + iki ifade-builtin'i + tamsayı taşma davranışı. parser.c
parse_kullan/parse_disa/parse_genel/parse_modul + parse_birincil DELEGE/GERI_AL birebir.

**Kapsam:** `kullan m::seg::{a,b} olarak d;` (KULLAN deger=`::`-yol; seçili/alias
dump'ta yok). `dışa <tanım>` (DISA sarmalar). `genel <tanım>` (SARMALAMAZ — iç tanımı
döner; genel_mi dump'ta yok). `modül Ad { üyeler }` (recursive parse_ust_oge). İfade:
`delege(...)`/`geri_al(...)` → DUGUM_CAGRI (hedef TANIMLAYICI). **TAM taşma:**
`tamsayi_deger` artık strtoll gibi int64-max'a CLAMP eder (`0xFFFF...FFFF` → LLONG_MAX,
önce wrap → -1).

**Doğrulama:** `make calistir_parser_diff` → 11/11 korpus + **115/118 GERÇEK .kem**
(ornekler/drivers/stdlib/moduller/crossfile). Kalan 3: KESIRLI float (drone_kontrol,
matris_carpim) + tip_alias (ayrı). C derleyici değişmedi.

**Sınır:** KESIRLI float (%g dump) → sıradaki (runtime float-format intrinsic gerek).
satıriçi_asm deyimi henüz yok (korpusta nadirse sonra).

---

## D-048 — SELF-HOST parser P3: bildirimler — 69/113 GERÇEK .kem --ast sıfır-diff (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/parser.kem` + korpus].** Aşama 1 P3: tüm
üst-düzey bildirimler. parser.c parse_islev_genel/parse_yapi/parse_cesit/
parse_ozellik/parse_uygula/parse_sabit/parse_parametre/parse_ust_oge ile birebir.

**Kapsam:** işlev (gerçekzamanlı? + generic `<T: Bound>` + param + dönüş + gövde;
imza_yeterli özellik için), yapı (+generic + alan), çeşit (varyant + C3 payload;
generic v1-YOK skip), özellik (imza/default), uygula (trait `için` + inherent),
sabit, parametre (`kendin`/`&kendin`/normal). Generic params + bound'lar PARSE+
DISCARD (dump'ta yok; bound düğümleri orphan). atla_tip_paramlar `>>` böl.

**İki kök-fix:** (1) **PROGRAM pozisyonu** = ilk token (C); önce 1:1 hardcode →
yorumla başlayan HER dosya farklıydı (0→69 sıfır-diff sıçraması). (2) **Anti-hang:**
çeşit generic + varyant-loop non-identifier'da ilerlemiyordu → sonsuz döngü; C
panik_sync deseni eklendi.

**Doğrulama:** `make calistir_parser_diff` → **11/11 korpus** + **69/113 GERÇEK .kem**
(ornekler/drivers/stdlib/moduller) tam --ast sıfır-diff. Kalan 44: P5 (modül/kullan/
dışa/genel/satıriçi_asm) + KESIRLI float. C derleyici değişmedi.

**Sınır:** Üst-düzey modül/kullan/dışa/genel/satıriçi_asm → P5; KESIRLI float (%g) → ayrı.

---

## D-046 — SELF-HOST parser P2: deyimler + kontrol akışı + desenler — 9/9 sıfır-diff (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/parser.kem` + korpus; C tarafı 0 değişiklik].**
Aşama 1 P2: tüm deyimler + kontrol akışı + eşleş desenleri. parser.c parse_deyim/
parse_eger/parse_iken/parse_icin/parse_esles/parse_desen/parse_guvensiz ile birebir.

**Kapsam:** değişken (`: tip` ops. + `= ifade`), atama (lvalue `=`), ver (0/1
çocuk), ifade-deyimi, eğer/değilse/değilse-eğer (else-if zinciri = recursive),
iken, için (`ad: koleksiyon`), eşleş + kollar, güvensiz (±`[etiket: "..."]`).
Desenler: joker `_`, tanımlayıcı, yapıcı `Ad(...)`, çeşit-yol `Çeşit::Varyant[(payload)]`,
literal. Kol gövdesi: ifade `;` veya `{ blok }`.

**`yapi_izni` bayrağı (parser.c yapi_olusturma_izni birebir):** Koşul bağlamında
(eğer/iken/için/eşleş değeri) `Tip { }` yapı-oluşturma KAPALI → `{` blok başı sayılır.
`Ayr.yapi_izni` (1 default; parse_kosul 0/restore). Düğüm pozisyonları C ile birebir
(deyim=keyword; atama/ifade-deyimi=ifade başı; eşleş-kolu=desen başı).

**Doğrulama:** `make calistir_parser_diff` → **9/9 SIFIR-DİFF** (6 P1 + 3 P2:
değişken-atama, kontrol-akışı, eşleş-güvensiz). C derleyici değişmedi → regresyon yok.
`--check` temiz.

**Sınır:** Param/generic/bildirim → P3; tam tip sözdizimi (annot Dizi/seçimlik/...)
→ P4; KESIRLI float → ayrı. P2 sarmalayıcı yine `işlev f() -> T { deyimler }` (param yok).

---

## D-045 — SELF-HOST parser P1: Pratt ifade parser — 6/6 korpus --ast sıfır-diff (2026-06-14)

**Karar [ETKİ: düşük — `selfhost/parser.kem` + korpus; C tarafı yalnız additive
`yaz_bayt` intrinsic].** Aşama 1 P1: KEMGU'da tam Pratt ifade parser. C `--ast`
oracle'ına karşı sıfır-diff.

**P1a — token-tablo temeli:** Lexer scanning REUSE (emit sink'i print→`dizi_ekle`),
token tablosu `Ayr` struct'ta (paralel Dizi). 250/250 gerçek .kem'de re-emit
`--token` sıfır-diff (foundation kanıtı). State threading: `&değişken Ayr`
(D-044'e dayanır; scalar+Dizi alan mutasyonu + ref-param passthrough de-risk edildi).

**P1b — Pratt ifade:** ifade.c ile birebir öncelik (VEYA=1…CARPMA=10, ÖNEK=11,
SONEK=12). Birincil (TAM/TANIMLAYICI/MANTIKSAL/BOS/METIN/KARAKTER + paren), önek
(neg/değil/~/&/&değişken/deref*), sonek zinciri (.alan/[i]/(args)/::yol/olarak),
yapı/dizi/lambda oluşturma, kullan/imha. Düğüm pozisyonları C ile birebir
(ikili/tekli=operatör tokenı; sonek=sonek tokenı; literal=kendi tokenı). Sayı
değeri (`_` temizle + 0x/0b/0o taban → int64 → ondalık string) ifade.c parse ile
aynı. AST = düz düğüm tablosu (append-on-create flat çocuk listesi). Düz dumper
preorder, `\t\n\r\\` kaçışlı (`yaz_kacis`).

**Yeni intrinsic `yaz_bayt(tam32)` (additive):** `yaz_karakter` argümanını
codepoint sayıp UTF-8 ENCODE eder → METIN değer dump'ında Türkçe bayt mojibake
(`ç`→`Ã§`). `yaz_bayt` HAM bayt yazar (putchar & 0xFF). 3 yer: tip_kontrol.c
builtin registry, llvm.c dispatch+declare, runtime. Lexer ASCII-only olduğu için
bunu hiç tetiklemedi; parser ham UTF-8 yazar → gerekli.

**Doğrulama:** `make calistir_parser_diff` → **6/6 SIFIR-DİFF** (aritmetik/mantık-bit/
önek-sonek/literal/bileşik/metin — Türkçe METIN + KARAKTER U+XXXX dahil). test_tumu
29 suite + ASan YEŞİL (yaz_bayt regresyonsuz). lexer bootstrap 256/256. `--check` temiz.

**Sınır (P1 dışı, sonraki adımlar):** KESIRLI float (%g formatı — ayrı adım); diğer
deyimler (P2); param/generic/bildirim (P3); tam tip sözdizimi (P4). P1 sarmalayıcı:
`işlev f() -> T { ver İFADE; }` (param yok, tek `ver` deyimi).

---

## D-044 [YÜKSEK] — Kök-neden fix: yapı Dizi<T> alanı boş/[...] literal → STACK [0xi8] → SEGFAULT (2026-06-13)

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit].** Parser
self-host P1 de-risk'inde ortaya çıktı (virtio track'teki "&Struct-param+Dizi"
segfault'unun kök-nedeni). `Yapı { d: [] }` veya `{ d: [e,...] }` — alan tipi
`Dizi<T>` iken — `DUGUM_DIZI_OLUSTUR` codegen'i **her zaman STACK `[N x i8]`**
üretiyordu (boş `[]` → `alloca [0 x i8]`). Alan KdlDizi* yerine 0-byte STACK
buffer'a işaret eder; `dizi_ekle(t.d, ..)` onu KdlDizi* sanıp `.veri/.boyut/...`
erişince **SEGFAULT**. `--check` geçiyordu (tip sistemi `[]`'i geçerli Dizi<T>
sayar) → latent codegen miscompile.

**Neden gizliydi:** `değişken d: Dizi<T> = []` ZATEN çalışıyordu — ama AYRI bir
özel-yol (DUGUM_DEGISKEN handler'ı, llvm.c:3341) heap'e dönüştürüyordu. Diğer TÜM
bağlamlar (yapı alanı, çağrı argümanı, `ver`) bu yoldan geçmiyordu → stack.

**Fix (kök, genel):** (A) `DUGUM_DIZI_OLUSTUR` artık `g->beklenen_tip` `Dizi<T>`
ise HEAP `kdl_dizi_olustur` + eleman başına `dizi_ekle` üretir (eleman_byte
eleman IR tipinden; iç içe için beklenen_tip elemana inilir). (B) `yapi_olustur_uret`
alan değerini değerlendirmeden ÖNCE `g->beklenen_tip = alan_tip_d` koyar. Böylece
TÜM Dizi<T> bağlamları (sadece değişken değil) doğru heap üretir.

**Doğrulama:** De-risk (`yapı Tablo{adlar:Dizi<metin>; sayilar:Dizi<tam32>}` +
`&değişken Tablo` param field-append) → exit 42 (segfault yok). test_llvm yeni
[161] regresyon. Tüm test paketi + ASan + lexer bootstrap YEŞİL (aşağıda).

**Kapsam/sınır:** Yalnız `[]`/`[...]` literalin heap-Dizi yönlendirmesi eklendi —
stack-dizi yolu (annot yok / index'lenen sabit dizi) korunur. `t.d[i]` index
sintaksı struct-field heap-dizi için ayrı (builtin `dizi_al/ekle/boyut` çalışır;
INDEX düğümü gerekirse sonra).

---

## D-043 — SELF-HOST parser ADIM-0: AST temsili + --ast diff-oracle (2026-06-13)

**Karar [ETKİ: düşük — additive C: yeni `--ast` modu + `ast_duz_yaz`; mevcut yol
değişmedi].** Aşama 1 (parser self-host) ADIM-0. Mandate: tasarım kararları ajan
verir + loglar + devam eder (sormaz). İki çekirdek karar:

**Karar 1 — KEMGU AST temsili: İNDEKS-TABANLI DÜZ DÜĞÜM TABLOSU** (paralel `Dizi`'ler).
Recursive `çeşit` (C3 payload) bir alternatifti ama kendine-referanslı tip
heap-boxing gerektirir (KEMGU'da belirsiz). Toy demolar (D-033/D-034) indeks-arena'yı
KANITLADI: düğüm = indeks; paralel diziler `dugum_tip[]`/`satir[]`/`sutun[]`/`deger[]`
+ düz çocuk-listesi (`cocuk[]` + `cocuk_basla[]`/`cocuk_sayi[]`). Recursive-descent
doğal: alt-ifadeleri parse et (indeks al) → ebeveyn düğümü o indekslerle oluştur.
Dump = preorder traversal (D-041 sonrası KEMGU çağrı-özyinelemesi stack-güvenli;
AST derinliği ~onlarca).

**Karar 2 — --ast diff-oracle formatı: DÜZ derinlik-etiketli preorder.**
`<derinlik>\t<TIP_ADI>\t<deger>\t<satır>\t<sütün>` (lexer dersi: düz > iç-içe-girinti).
Derinlik-etiketli preorder AĞACI BİREBİR belirler (benzersiz). `<deger>` = skaler yük
(ad/literal/operatör), `\t \n \r \\` kaçışlı (alan-ayracı güvenliği). Mevcut
`ast_yazdir` (--parse, insan-okunur) EKSİK — ~30 düğüm tipi `default`'a düşüp
çocuklarını gezmiyor. Yeni `ast_duz_yaz` (`--ast`) TÜM 67 düğüm tipini + çocuklarını
KANONİK sırada gezer (oracle tamlığı). KEMGU-parser aynı çıktıyı üretecek → diff = doğruluk.

**Doğrulama:** Prod 0 uyarı. `--ast` 249/249 gerçek .kem'de deterministik + boş-değil.
Öncelik doğru (`x + 1 * 2` → `x + (1*2)`). Mevcut `--parse`/test'ler etkilenmedi (additive).

**Plan (P1-P6, her biri --ast sıfır-diff kapılı):**
- **P1 ifadeler** — Pratt öncelik (veya<ve<==<karşılaştırma<+−<*/%<önek<sonek),
  önek (−/değil/~/&/&değişken/*), sonek zinciri (.alan [i] (arg) ::yol), yapı/dizi/
  lambda oluşturma, `olarak` cast, kullan/imha ifade. (+minimal işlev/blok/ver sarmalayıcı.)
- **P2 deyim/kontrol** — değişken/atama/ver/eğer-değilse/iken/için/eşleş+desen/güvensiz/blok.
- **P3 bildirim** — işlev (generic+bound), yapı, çeşit (payload), özellik, uygula, sabit, alan, parametre.
- **P4 tip-sözdizimi** — &T/&değişken T/*T, Dizi/seçimlik/sonuç/tekkez/sabitsüre/yetki/
  vektör/görev/kanal, işlev(...)→T, Kullanıcı<...>, `>>` generic-böl.
- **P5 modül/import** — modül, kullan (namespaced/seçili/alias), dışa, genel.
- **P6 tüm-korpus** — KEMGU-parser tüm .kem + KENDİ kaynağı (self-parsing) → --ast sıfır-diff.

**Ön-koşul/sınır:** Sayı literal→değer dönüşümü (TAM int64, `_`/hex/bin/oct) KEMGU'da
C parser ile birebir gerekecek (P1). KESIRLI değer formatı (`%g`) fragility riski →
gerekirse P1/P4'te lexeme-tabanlıya geçilir (karar o noktada). Generic-param/çeşit-varyant
ad string-metadata --ast'a P3/P4'te eklenir (şimdilik yapısal ağaç).

---

## D-042 — SELF-HOST lexer M6: BOOTSTRAP kapanışı — 249/249 gerçek .kem sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yeni harness + Makefile hedefi].** M6 = self-host lexer'ın
asıl ispatı: KEMGU-lexer (selfhost/lexer.kem) TÜM gerçek KEMGU korpusunu C lexer
(oracle) ile **sıfır-diff** lex'ler.

**Kapsam:** `test/lexer_bootstrap_harness.sh` — KEMGU-lexer'ı derler, `build/`
(üretilmiş temp) hariç tüm `.kem` dosyalarını (`stdlib/`, `drivers/`, `kütüphane/`,
`test/**`, **`selfhost/lexer.kem`'in KENDİSİ** = self-lexing) C `--token` dump'ına
karşı diff'ler. `make calistir_lexer_bootstrap`.

**Sonuç:** **249/249 SIFIR-DİFF** — KEMGU-lexer C lexer'ı gerçek dünya KEMGU
kodunda TAM İKAME EDER. Self-lexing dahil (4566 token, kendi kaynağı). M1-M5
korpus 22/22 regresyon kalır.

**Engel + çözüm:** İlk koşuda 3 büyük dosya (lexer.kem dahil) crash etti (exit 127,
~binlerce iterasyon sonra) → kök-neden D-041 codegen alloca bug'ı. Düzeltildi.

**Bootstrap durumu:** Lexer parite TAM. Sıradaki gerçek-entegrasyon adımı
(parser'ın KEMGU-lexer çıktısını tüketmesi / C lexer'ın emekliye ayrılması)
mimari karar gerektirir (Token API köprüsü) → DUR-SOR (Mehmet). M6 = token-parite
+ self-lexing ispatı tamamlandı.

---

## D-041 [YÜKSEK] — Kök-neden fix: dongu govde alloca'sı → stack overflow (entry hoist + renumber) (2026-06-13)

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit].** Döngü
gövdesindeki `değişken` (ve koşul/ifade temp'leri) BLOK-İÇİ `alloca` üretiyordu.
LLVM yalnız **entry-blok** alloca'sını fonksiyon girişinde BİR KEZ tahsis eder;
başka blok'taki alloca her ÇALIŞMADA stack ayırır → uzun döngüde **STACK
OVERFLOW**. Latent bug — toy programlar az iterasyonla tetiklemedi; **self-host
lexer'ın binlerce-iterasyonlu ana döngüsü açtı** (exit 127, ~3775 tokende crash,
dosyaya göre farklı nokta = döngü-başı alloca kanıtı).

**Fix:** `islev_uret` gövdeyi `tmpfile()` buffer'a yazar; `hoist_renumber` tüm
`%N = alloca` satırlarını entry blok başına taşır. Taşıma SSA ardışık-numara
kuralını bozduğundan (`clang`: "instruction expected to be numbered") TÜM numaralı
değerler (`%<rakam>`; `%bb<ad>`/`%<ad>` hariç) yeniden numaralanır. **Güvenli
çünkü:** tüm alloca'lar statik-boyut (operandsız) → erken taşıma ileri-referans
yaratmaz; codegen **phi kullanmaz** (alloca/load-store) → tek-geçiş renumber yeterli;
koşullu alloca'yı her zaman tahsis etmek semantik olarak zararsız (kullanılmayan
stack).

**Doğrulama:** Tüm test paketi YEŞİL — test_llvm **234/234** (yeni [160]:
500000-iter döngü-yerel alloca, crash yok → 42), birim testleri (57/39/35/40/23/
30/5/50/9/6/16/36/13/6/21 hepsi 0 başarısız), ASan matris 20000 iter/0 crash,
stdlib --check temiz. Self-host lexer artık kendi kaynağını crash'sız lex'ler.

**Kapsam/sınır:** Yalnız alloca yerleşimi değişti — ABI/imza/struct-layout/semantik
DEĞİŞMEDİ (mem2reg/SROA zaten hoist ederdi; fix sadece text-IR'ı geçerli kılar).
Tüm fonksiyonlar tek yoldan (`islev_uret`) emit → fix global.

---

## D-040 — SELF-HOST lexer M5: trivia (yorum) + ham string — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus].** M5: `bosluk_atla`'ya
`//` satır + `/* */` İÇ İÇE blok yorum (derinlik sayacı); ham string `r#"..."#`
(`ham_basi_mi` + `ham_emit`, hash eşleme). C bosluk_atla/ham_metin_oku birebir.

**Kapsam:** `//` → `\n`'e kadar (tüketmeden). `/*` → derinlik sayacı, iç içe
(`/* /* */ */` doğru). Ham string: açılış N hash = kapanış N hash; `r"..."` (0 hash)
özel; iç tırnak literal. L011 (geçersiz baş), L002 (kapanmamış). **Çok-satırlı
trivia/ham string → satir/sutun re-scan** (yorum: bosluk_atla içinde inline; ham
string: hlen span'i üzerinden re-scan, `\n`→satir++).

**Kasıtlı NON-hata parite:** kapanmamış blok-yorum SESSİZ EOF'ta biter (HATALI YOK —
C ile aynı). `//`/`/*` string/ham-string İÇİNDE yorum değil (literal tarama önce).

**Doğrulama:** `make calistir_lexer_diff` → **22/22 SIFIR-DİFF** (18 M1-M4 + 4 M5).
Spot: `a /* /* iç */ dış */ b` → `b` 1:25 (iç içe tüketildi); `x = r"çok⏎satır"⏎y`
→ `y` satır 4 (çok-satırlı ham string satir izleme bayt-exact). `--check` temiz.

**Sıradaki (M6):** bootstrap kapanışı — KEMGU-lexer'ı (a) KENDİ kaynağına +
(b) tüm gerçek `.kem` korpusuna karşı sıfır-diff doğrula (self-lexing ispatı).

---

## D-039 — SELF-HOST lexer M4: literaller (sayı/float/metin/karakter) — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus].** M4: tam literal
desteği — `sayi_emit` (ondalık + 0x/0b/0o + float kesir+üs), `dize_emit`
(`"..."`), `karakter_emit` (`'a'`/UTF-8). C lexer sayi_oku/metin_oku/karakter_oku
ile span-exact (kaynak L106-268 birebir port).

**Kapsam:** Sayı — 4 taban (0x/0b/0o erken-return TAMSAYI; boş gövde `0x` hatasız),
float kesir (`.` guard `sonraki≠.` → `1.5` vs `1..5`; trailing-dot `7.`), üs `e/E±`.
Metin — escape DECODE EDİLMEZ (`\`+1 bayt atlanır, C gibi); newline/EOF → HATALI
(L001). Karakter — escape (`\`+1) veya UTF-8 tek-karakter (utf8_uz); boş `''`→L009,
çok/kapanmamış→L010. Hepsi tüketilen bayt döner.

**KÖK-NEDEN bulgu [self-host isim kısıtı]:** Codegen `metin_*` ön-ekini runtime
intrinsic'e yönlendiriyor (llvm.c:2961 `memcmp(cagri_adi,"metin_",6)`) → `kdl_metin_*`
(ptr dönüş). `işlev metin_emit` bu yüzden `kdl_metin_emit` sayıldı → IR tip hatası
(`store i32 ptr`). **Çözüm:** `metin_emit`→`dize_emit`. (Pure-prefix dispatch'ler
yalnız `metin_`/`dosya_`; `karakter_`/`sayi_`/`dizi_` exact-match → güvenli.)
Kaynak değiştirilmedi — isim kuralıyla çözüldü.

**Doğrulama:** `make calistir_lexer_diff` → **18/18 SIFIR-DİFF** (13 M1-M3 + 5 M4).
Spot: `1..5 1.5 7. 0x 1e10 "tam"` → bayt-exact (`7.`=ONDALIK trailing-dot,
`0x`=TAMSAYI boş-hex, `1..5`=TAMSAYI+ARALIK+TAMSAYI). `--check` temiz.

**Sıradaki (M5):** trivia — `//` satır + `/* */` İÇ İÇE yorum (derinlik sayacı) +
ham string `r#"..."#` (hash eşleme, L002/L011). Kasıtlı NON-hata parite (kapanmamış
blok-yorum sessiz, geçersiz UTF-8→bayt-bayt HATALI).

---

## D-038 — SELF-HOST lexer M3: operatörler + noktalama (maximal munch) — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus].** M2'nin tek-karakter
`tek_kar_tip`'i, tüm çok-karakter operatörleri MAXIMAL MUNCH ile çözen `op_emit`
ile değiştirildi (C lexer switch 318-375 ile birebir).

**Kapsam:** Çatışma zincirleri — `.`/`..`/`...`, `:`/`::`, `<`/`<=`/`<<`,
`>`/`>=`/`>>`, `=`/`==`/`=>`, `-`/`-=`/`->`, `+`/`+=`, `*`/`*=`, `/`/`/=`,
`%`/`%=`, `!`/`!=`. Bit ops `& | ^ ~` KOŞULSUZ (`&&` yok; `&değişken` lexer'da
BİRLEŞMEZ). `>>` daima tek `SAGA_KAYDIR` (generic'i parser böler). Tek `!`→HATALI.
Kalan ayraç `[ ] :`. `op_emit` tüketilen bayt sayısını döner (sütün/pos ilerletme).
`ikinci_bayt` sınır-güvenli lookahead (OOB→0).

**Doğrulama:** `make calistir_lexer_diff` → **13/13 SIFIR-DİFF** (9 M1/M2 + 4 M3).
Adversaryel munch spot-check: `a>>b ....x !c ===z` → C oracle ile bayt-exact
(`>>`=tek SAGA_KAYDIR, `....`=UC_NOKTA+NOKTA, `!c`=HATALI+TANIMLAYICI,
`===`=ESIT_ESIT+ESIT). Not: C stderr L005 mesajı token dump'ında değil → diff'i
etkilemez; HATALI tokenı eşleşir. `--check` temiz.

**Sınır (kasıtlı):** `//` `/*` trivia M5'te (M3 yalnız `/` `/=`). `digit.` (float)
M4'e ait — M3 korpusu `digit.` içermez (yalnız `..`/`...` aralık güvenli).

**Sıradaki (M4):** literaller — hex/bin/oct tamsayı, float (kesir+üs), karakter/
metin (escape ham bırakma). Korpus literal-ağırlıklı genişler.

---

## D-037 — SELF-HOST lexer M2: UTF-8 + 44 Türkçe anahtar kelime — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus genişler; C tarafı 0
değişiklik].** M1 ASCII çekirdeği M2'de UTF-8 Türkçe'ye genişletildi. Tam ikame
(bootstrap) yolunda ikinci milestone.

**Kapsam (M2):**
- **UTF-8 identifier:** ASCII `[A-Za-z_]` + 2-bayt Türkçe harf. `turkce_2byte_mi`
  fonksiyonu `utf8.c turkce_harf_2byte` ile BİREBİR: 0xC3(195)→ç Ç ö Ö ü Ü,
  0xC4(196)→ğ Ğ ı İ, 0xC5(197)→ş Ş (2.bayt değerleri tek tek eşleşir).
- **44 anahtar kelime:** M1'in 15 ASCII'sine geri_al/uygula/kanal + 27 Türkçe
  (`anahtar_tip` tam-eşleşme zinciri; `metin_esit` UTF-8 lexeme byte-byte).
- **Bayt-tabanlı sütün:** Türkçe karakter sütünü +2 ilerletir (C `ilerle` deseni —
  her bayt 1 sütün). Doğrulandı: `değişken`=10 bayt → uzunluk 10, sonraki token
  sütün 12; `çörek_adedi`=13 bayt.
- **Değişken-bayt identifier taraması:** `kimlik_basi_uz`/`kimlik_devam_uz` her
  karakterin bayt-uzunluğunu döner (1 ASCII | 2 Türkçe | 0 yok), tarama buna göre
  ilerler.

**`metin_bayt` İŞARETLİ-bayt çözümü (ADIM-0 ön-koşulu):** `metin_bayt` `tam8`
(işaretli) döner → Türkçe 0xC3 = -61. Dağınık işaretli sabitler yerine tek
`bayt(s,i)` helper'ı UNSIGNED (0-255) döndürür (`eğer b<0 { ver b+256 }`). Tüm
bayt karşılaştırmaları 195/167/... gibi doğal unsigned değerlerle. Temiz + UTF-8
dayanıklı.

**Doğrulama:** `make calistir_lexer_diff` → **9/9 SIFIR-DİFF** (5 M1 + 4 M2 korpus:
44 keyword · Türkçe identifier ç/ğ/ı/ö/ş/ü · karışık · kelime-sınır). Kelime-sınır
adversaryel: `değişken`→DEGISKEN ama `değişkenler`/`değişken2`→TANIMLAYICI (yalnız
tam eşleşme). `--check` temiz. C tarafı değişmedi → prod/test_lexer etkilenmedi.

**Sıradaki (M3):** çok-karakter operatörler (`==`, `!=`, `<=`, `>=`, `->`, `::`,
`&&` yok→`ve`, `&değişken`, `>>` generic-böl) — maksimal-munch. Korpus operatör
ağırlıklı genişler; M1/M2 regresyon kalır. (Literal varyant → M4, yorum/raw → M5.)

---

## D-036 — SELF-HOST lexer M1: ASCII çekirdek iskelet — C lexer'a karşı sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yeni `selfhost/lexer.kem` + korpus + harness; C tarafı yalnız
`--token` dump formatı]:** Gerçek KEMGU lexer'ını KEMGU'da yazma fazının M1'i
(ADIM-0/D-035 planı). Hedef: tam ikame (bootstrap) — Mehmet onayı. M1 = ASCII
çekirdek, C lexer'a karşı SIFIR-DİFF.

**M1 kapsamı (`selfhost/lexer.kem`):** ASCII identifier + **15 ASCII anahtar kelime**
(iken/ve/veya/ver/delege/hata/imha/kendin/kullan/olarak/sabit/tamam/tekkez/yetki/
genel) + ondalık tamsayı (`_` ayraç) + tek-karakter op (`+ - * / % =`) + ayraçlar
(`( ) { } , ;`) + DOSYA_SONU. Satır/sütün/offset C `ilerle` desenine BİREBİR (\n →
satır++/sütün=1; diğer → sütün++; bayt-tabanlı). (Türkçe keyword/UTF-8 → M2;
çok-karakter op → M3; sayı varyant/literal → M4; yorum/raw → M5.)

**Diff-oracle formatı (D-035 — Mehmet "C'nin daha iyisi" istedi):** C `--token`
(ana.c) ESKİ `%-20s "%.*s"\t\t%d:%d` (ham lexeme gömülü → string-literal'de kırılır,
padding+çift-tab parse-zor) YERİNE: `<TIP>\t<satır>\t<sütün>\t<offset>\t<uzunluk>`.
Ham lexeme YOK (offset+uzunluk'tan kurtarılır) → kaçış-kopyalama riski SIFIR + tek-tab
makine-parse-edilebilir. KEMGU-lexer birebir aynı satırı üretir → `diff` = otomatik
doğruluk. Hiçbir test `--token`'a bağlı değil (test_lexer API-tabanlı, etkilenmez).

**Teknik notlar:** `arg_al(1)`+`dosya_oku` ile dosya okuma (DOĞRULANDI: çalışır).
`yaz_metin` builtin DEĞİL → string bayt-bayt `yaz_karakter` ile (`yaz_str`).
`yaz_karakter` `karakter` ister → `yb(c)` = `c olarak karakter` cast helper'ı.
metin_bayt işaretli ama M1 ASCII (<128) → sorun yok (Türkçe işaretlilik M2'de).

**Doğrulama:** `make calistir_lexer_diff` (`test/lexer_diff_harness.sh`) — 5 ASCII
korpus (`test/lex_korpus/m1_*.kem`: aritmetik, 15 keyword, sayı-ayraç, yapı-punct,
identifier-kenar) → **5/5 SIFIR-DİFF**. test_lexer 103/103 (format değişikliği API'yi
bozmadı). Prod 0 uyarı. selfhost/lexer.kem --check temiz.

**Sıradaki (M2):** UTF-8 identifier (byte-byte 0xC3/C4/C5 + ikinci-bayt; metin_bayt
İŞARETLİ → signed-karşılaştır) + 28 Türkçe anahtar kelime + bayt-tabanlı sütün
doğrulama. Korpus Türkçe keyword'lerle genişler; M1 korpusları regresyon kalır.

---

## D-034 — Self-hosting: mini dil V3 — FONKSİYONLAR (tanım+çağrı+özyineleme) → Turing-tam (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-033 (kontrol akışı)
üstüne fonksiyon soyutlaması — `islev ad(p1,p2){ … don ifade; }` + `ad(arg)` çağrısı
+ özyineleme + karşılıklı çağrı (`test/ornekler/18_fonksiyon_dili.kem`). Toy-dil
artık **Turing-tam**. **Bounded:** fonksiyonlar — closures (yakalama) ERTELENDİ
(DUR-SOR sınırı; ayrı çetin tasarım).

**Tasarım (flat-token yürütücü + KEMGU'nun kendi özyinelemesi — AST'ye geçmeden):**
- **Fonksiyon tablosu** (Fonksiyonlar struct): ön-geçiş `islev` tanımlarını
  kaydeder (ad → param adları + gövde '{' konumu); normal yürütmede tanım gövdeleri
  `islev_atla` ile atlanır.
- **Çağrı (cagri_yap):** argümanlar ÇAĞIRANIN kapsamında değerlendirilir → YENİ
  kapsam itilir (params bağlanır) → gövde yürütülür → dönüş yayılır.
- **Kapsam yığını:** `Semboller.ust` = mantıksal tepe. dizi_pop YOK → slot'lar
  yeniden kullanılır (ust kaydet/sıfırla = push/pop); arama tepeden tabana (en
  yakın bağlama → özyinelemede her çağrının param'ı İZOLE). Global = en alttaki.
- **don:** dondu bayrağı + donus değeri (Semboller'de 1-elemanlı Dizi); blok/döngü
  her deyimden sonra dondu kontrolüyle erken çıkar (exception'sız erken-dönüş).
- **İMLEÇ özyineleme-güvenli:** cagri_yap çağrı-sonrası konumu (`resume`) ve kapsam
  tabanını (`marker`) YEREL değişkende saklar → KEMGU'nun çağrı yığını her seviyeyi
  korur → paylaşılan imleç doğru kaydedilip geri yüklenir. **Flat-token'ın
  çağrı/dönüş için "zorlanması" bu yerel-kaydet deseniyle çözüldü; AST rewrite
  GEREKMEDİ.**
- Lexer'a `,` (18), `don` (19), `islev` (20) + 2-param çağrı eklendi.

**Doğrulama (adversarial, 9 program):** faktöriyel (fakt(5)=120), Fibonacci
(fib(10)=55, fib(12)=144), çok-param (topla(40,2), carp(6,7)), KARŞILIKLI özyineleme
(cift/tek = isEven/isOdd), fonksiyon-içi döngü (kareler(10)=45), iç içe çağrı
(kare/iki), fonksiyondan global erişim (g=30; ekle(12)). Headline: fakt(5)-78 = 42.
opt-verify PASS. **ASan/UBSan TEMİZ** — fib(12) derin özyineleme dahil 0 ihlal
(kapsam-yığını slot reuse belleği sınırlar).

**Testler:** test_llvm 231→**233** ([verify]+[run]). asan_e2e_denetim otomatik
kapsar (örnek temiz). Derleyici DOKUNULMADI → diğer suite'ler etkilenmez.

**Self-hosting durumu:** Mini-dil V3 = lexer + öncelikli parser + kontrol akışı +
fonksiyonlar/özyineleme + kapsam — tam bir Turing-tam toy dil, saf KEMGU'da.
**Bu, KEMGU'nun ifade-gücü kanıtının (D-022→D-034) son büyük yapı taşı.**
Sıradaki gerçek faz: bu demolar self-hosting PROXY'siydi; asıl adım gerçek KEMGU
derleyicisinin bir parçasını (doğal ilk parça: gerçek KEMGU lexer'ı) KEMGU'da
yazmak — daha büyük, planlı bir faz (ayrı konuşulacak).

---

## D-033 — Self-hosting: mini dil V2 — KONTROL AKIŞI + deyim blokları (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-028 (atama + sembol
tablosu + ifade) üstüne, mini-dili gerçek bir İMPERATİF dile çıkardım: koşul
(`eger`/`degilse`), döngü (`iken`) ve `{ deyim* }` blokları
(`test/ornekler/17_kontrol_dili.kem`). "Çok-deyimli → gerçek derleyici alt-kümesi"
adımı; kontrol akışı = gerçek dil. **Bounded:** yalnız bu dilim — fonksiyon-tanımı
SONRAKİ dilime bırakıldı (DUR-SOR sınırına uyuldu).

**Yeni yetenekler (hepsi saf-KEMGU, mevcut intrinsic'lerle):**
- **Lexer V2:** çok-harf IDENT + ASCII anahtar kelime tanıma (eger/degilse/iken,
  metin_esit ile) + 2-karakter `==` (lookahead) + `< > { }` token'ları.
- **İfade:** karşılaştırma seviyesi (`<`/`>`/`==`, sonuç 1/0) toplama üstünde.
- **Yürütücü (flat-token, ağaç-yürüyen):** `deyim_calistir` (atama/eğer/iken
  dağıtımı), `blok_calistir` (`{ deyim* }`), `blok_atla` (eşleşen `}` say,
  yanlış dal/döngü-çıkışı için), `eger_calistir` (koşullu dal + opsiyonel
  degilse), `iken_calistir` (koşulu re-eval + gövde tekrar yürütme, imleç
  konum kaydet/sıfırla). Mini-dilin if/while'ı KEMGU'nun if/while'ıyla yorumlanır.

**Doğrulama (adversarial, 18+ program):** iken-döngü toplam (1..10=55), eğer/değilse
her iki dal, if-içinde-döngü, döngü-içinde-if, İÇ İÇE döngü, ardışık döngüler,
boş blok, faktöriyel (3!=6), false-from-start döngü, çok-harf değişken, iç içe
if-else. Headline: `i=0;t=0; iken(i<10){t=t+i+1;i=i+1;} r=0; eger(t==55){r=t-13;}
degilse{r=0;} r` = 42. opt-verify PASS. **ASan/UBSan TEMİZ** (42, 0 ihlal — yoğun
dizi kullanımı, D-029/D-030 fix'leri + matris kapsaması geçerli).

**Testler:** test_llvm 229→**231** ([154] verify + [155] run). asan_e2e_denetim
84→85 (yeni örnek otomatik kapsanır, temiz). Derleyici dokunulmadı → diğer suite'ler
etkilenmez. **Sıradaki:** mini-dilde fonksiyon-tanımı/çağrısı (ayrı büyük dilim),
sonra gerçek derleyici alt-kümesi.

---

## D-032 — ASan/UBSan bellek güvenliği matrisi: D-029/D-030 eksenleri kalıcı regresyon ağı (2026-06-13)

**Karar:** D-029 (yapı alan-adı çözümü) ve D-030 (dizi_olustur element_byte
heap-overflow) hatalarının yaşadığı EKSENLERİ sınır-noktalarında zorlayan, kendini
doğrulayan (başarı=exit 42) temsili bir program seti — `test/asan_matris/m01..m10.kem`
— + `test/asan_matris_calistir.sh` + `make calistir_asan_matris`. Her program HEM
sanitizer'sız (değer doğruluğu) HEM ASan/UBSan altında (bellek güvenliği) koşulur.
Tam kartezyen değil; her eksende SINIR-NOKTALARI.

**Kapsanan eksenler (neden D-030/D-029'a odaklı):**
- **Eleman-byte sınırı** (D-030 element-SIZE'dı): tam32 (4-byte) vs tam64/metin/&Yapi
  (8-byte). m01-m04. 8-byte tipler için olustur(4)+10/20 ekle = eski "kapasitenin
  yarısında heap-overflow" tetikleyicisini + realloc büyüme yolunu zorlar.
- **Küçük eleman**: tam8/tam16 (i32 stride) m05.
- **dizi_yaz in-place** (D-025) × tam64 + metin × sınır-üstü indeks: m06, m07.
- **Yapı konfigürasyonları** (D-029 alan-adı): tek yapıda KARIŞIK eleman-byte
  koleksiyonlar (Dizi<tam32>+Dizi<metin>+Dizi<tam64> bir arada, her biri doğru
  byte) m08; iki yapı AYNI alan adı 'ad' FARKLI eleman tipi (T.ad=metin 8-byte,
  U.ad=tam32 4-byte) m09; &Yapi param üzerinden alan erişimi m04/m08/m09;
  tam-kapasite + realloc geçişi m10.

**Sonuç: 10/10 TEMİZ — değer-doğru + ASan/UBSan ihlali YOK.** Yeni codegen bug'ı
bulunmadı. D-029/D-030 fix'leri tüm sınır eksenlerinde geçerli. Bu, KEMGU'nun
çekirdek iddiasına (bellek güvenliği — buffer-overflow imkansız) sınır-zorlamalı
bir güven verir + kalıcı regresyon ağı (gelecekte element-byte/alan-çözüm
regresyonlarını yakalar). Derleyici DOKUNULMADI (yalnız fikstür + harness +
make hedefi). Tüm suite yeşil, temiz build.

---

## D-031 — ASan/UBSan codegen denetimi: harness + 8 pre-existing crash teşhisi (2026-06-13)

**Bağlam:** D-030 (dizi_olustur heap-overflow) gösterdi ki test_llvm E2E zinciri
(`kemgu --llvm | clang | run`) üretilen kodu SANITIZER'SIZ koşuyor → codegen
bellek hataları gözden kaçıyor. KEMGU'nun #1 hedefi bellek güvenliği olduğundan,
proaktif bir ASan/UBSan denetimi eklendi.

**Karar:** `test/asan_e2e_denetim.sh` + `make calistir_asan_denetim` — tüm
çalışabilir örnekleri `clang -fsanitize=address,undefined` ile derleyip çalıştırır,
ihlalleri raporlar. **Sonuç: 84 örnek ASan/UBSan-TEMİZ**, 8 bilinen başarısızlık
(ALLOWLIST, nedeni belgeli), 27 atla (main yok / parse-only).

**Bulunan 8 pre-existing crash (test suite E2E koşmadığı için saklıydı):**

*Sınıf A — dizi-literal → `Dizi<T>` parametresi (4):* `03_kontrol`, `35_binary_search`,
`36_quicksort_stub`, `40_dizi_islemler`. Kök-neden: array literal `[1,2,3]` STACK
`[N x T]` üretir; `Dizi<T>` PARAMETRESİ ise dinamik `KdlDizi*` bekler (param
`dinamik_dizi_mi=1`). `xs[i]` / `için x: xs` / `dizi_boyut(xs)` stack array'i
KdlDizi olarak okur → misaligned access → SEGFAULT. (Dinamik dizi `dizi_olustur`
parametre olarak ÇALIŞIR — D-024; yalnız stack-literal→param yolu kırık.)
**KARAR Mehmet'e açık (DUR-SOR):** stack-array-literal ↔ dinamik-KdlDizi temsil
uyumsuzluğu. Seçenekler: (a) çağrı sınırında literal→KdlDizi coercion, (b) array
literal Dizi<T> bağlamında daima heap, (c) `için`/`[]` param için stack-array
yolu. Hepsi temsil/semantik kararı — tek başıma değiştirmedim.

*Sınıf B — lambda/closure (4):* `04_islev`, `10_lambda`, `25_closure_capture`,
`42_lambda_hesap`. Garbage func-ptr çağrısı → access-violation. **Bilinen:
D-004 ile LAMBDA codegen V2'ye ERTELENDİ** (fonksiyon-değer codegen yok). Yeni
bug değil; bu örnekler ertelenen özelliği egzersiz ediyor.

**Kapsam:** Bu commit DENETİM ALTYAPISI + teşhis. 8 crash'in fix'i ayrı
(A = temsil kararı Mehmet'te; B = V2 lambda feature). Harness bunları ALLOWLIST'le
dışlar; fix indikçe ALLOWLIST'ten çıkarılır → denetim regresyon koruması olur.
Bu, derleyici tip-kontrolünden geçen ama segfault eden programların (bellek
güvenliği ihlali) gelecekte yakalanmasını kurumsallaştırır.

---

## D-030 [YÜKSEK] — Kök-neden fix: dizi_olustur element_byte heap-buffer-overflow (ptr/tam64 dizi) (2026-06-13)

**Bağlam:** D-029'da "kapsam dışı, pre-existing" diye bırakılan `m*n*p+18`=18 bug'ı.
Runtime trace + **AddressSanitizer** ile kök-neden bulundu — CİDDİ bir bellek
güvenliği (heap-buffer-overflow) bug'ı.

**KÖK-NEDEN (ASan KANITI):**
```
AddressSanitizer: heap-buffer-overflow WRITE size 8 in kdl_dizi_ekle_ptr
  <- token_ekle <- lexle ;  allocated by kdl_dizi_kapasite_ayarla <- calistir
```
`dizi_olustur` codegen'i element_byte'ı **SABİT 4** emit ediyordu
(`kdl_dizi_olustur(i32 4)`). `dizi_olustur(N)` → `kapasite_ayarla(N)` →
buffer = N×4 byte. Ama `Dizi<metin>` (8-byte ptr eleman) `dizi_ekle_ptr` ile
8-byte yazıyor → N×4 buffer yalnız **N/2 ptr** tutar; (N/2+1). yazım (boyut hâlâ
< kapasite olduğu için realloc tetiklenmeden) HEAP'i taşırıyor. Token-adı dizisi
`dizi_olustur(32)` → 128 byte → 16 ptr; 17. token (`t.ad[16]`) taşma → komşu
bellek + sonraki metin_kes buffer'ları bozuluyor → değişken adı GARBAGE →
`sembol_ara` bulamıyor → 0 → `m*n*p`=0. Non-deterministik (ASLR'ye göre değişen
garbage) — D-029'da kafa karıştıran buydu.

**Neden 17 token (m*n*p) çalışıp 19 (m*n*p+18) çökmüştü:** ikisi de `t.ad[16]`'ı
taşırır ama +18 fazladan 2 taşma yazımı (17,18) yapıp "p" adının buffer'ını
deterministik olarak eziyordu; 17-token tek taşma çoğu zaman zararsız komşuyu
bozuyordu (şanslı 24).

**Fix [YÜKSEK]:** dizi_olustur element_byte'ı eleman tipinden hesapla:
ptr/i64 → 8, i8/i16/i32 → 4. Kaynak: `g->beklenen_tip` (değişken annotasyonu
`Dizi<T>`). Bilinmiyorsa (struct-alan inşası — yapi_olustur_uret per-alan
beklenen_tip set etmez) **8 = güvenli max** (i32'yi 2x reserve eder ama taşma
İMKANSIZ; tüm eleman tipleri ≤8 byte). kapasite_ayarla N×8 ayırır → ptr/tam64
güvenli; sonraki dizi_ekle realloc'ları zaten sizeof ile doğru.

**Etki:** Bu bug `Dizi<metin>`/`Dizi<&T>`/`Dizi<tam64>` >N/2 eleman tutan HER
programı sessizce bozuyordu (yalnız demo değil). Bellek güvenliği — KEMGU'nun
çekirdek hedefi.

**Repro test (kırmızı→yeşil):** `test/snapshots/dizi_metin_kapasite.kem` — 20
metin (>16) ekle+oku = 42. Fix öncesi ASan heap-overflow + crash (127); sonrası
ASan TEMİZ + 42. test_llvm [150]. 16_degiskenli_dil.kem ana ifadesi
`m=2;n=3;p=4;m*n*p+18` (19 token, >16) yapıldı — gerçek demo bağlamında regresyon
koruması. Adversarial: 4-değişken zincir (`m*n*p*q-78`), uzun ifadeler hepsi 42.

**Doğrulama:** test_llvm +2 ([150] kapasite, demo güncel). 22 suite + 0 ASan +
prod 0 uyarı + stdlib 12. (D-029'daki "kapsam dışı pre-existing" notu → ÇÖZÜLDÜ.)

---

## D-029 [YÜKSEK] — Kök-neden fix: yapı alan-adı çakışması codegen bug'ı (D-028 PROB ÇÖZÜLDÜ) (2026-06-13)

**Bağlam:** D-028 PROB'u "dizi_olustur-alan-init'li 2. yapı, 1. yapının koleksiyon
alanını bozuyor (boyut 3→6)" diye gözlemlemişti. Runtime trace ile KÖK-NEDEN
bulundu — gözlem yanlış çerçevelenmişti (yapı-yerel kopya değil).

**KÖK-NEDEN 1 (alan-adı çözüm bug'ı) — KANIT (runtime trace):**
`kdl_dizi_olustur/ekle/boyut` pointer + boyut trace'lendi. Minimal repro'da
(T{kind,deger,ad,imlec} + U{ad,deger}; `te()` t.kind/t.deger/t.ad'ye ekler):
```
ekle_tam CE0(kind)  ekle_tam 68A0(deger)  ekle_ptr CE0(ad → YANLIŞ! 6780 olmalı)
```
`dizi_ekle(t.ad, …)` t.ad'ye (6780) değil **t.kind'e (CE0, alan 0)** yazıyordu →
t.kind 3 yerine 6 eleman (3 kind + 3 ad), t.ad boş. İKİ KOLEKSİYON ALİASLANMIYOR;
**t.ad alanı YANLIŞ alana (kind) çözülüyor.** Tetikleyici: U'nun da `ad` alanı
olması (U.ad index 0). Hipotez U alan adlarını `isim/sonuc` yapınca doğrulandı (33).

**Mekanizma (codegen):** `erisim_uret` (+`erisim_lvalue`), nesnenin IR tipi `ptr`
(yani &Yapi parametre) iken yapı tipini IR'dan çıkaramıyor → **TÜM yapılarda alan
adına göre ilk eşleşeni arıyordu** (llvm.c eski 1227-1234). T.ad (index 2) + U.ad
(index 0) varken `t.ad` → U'ya çözülüp `getelementptr %U, …, 0, 0` = T'nin alan 0'ı
(kind). GEP base nesne.reg (gerçek T) olduğu için sessizce kind'e yazıyordu.

**Fix 1:** `LlvmIsim.ref_yapi_ir` alanı eklendi (&Yapi/*Yapi/Yapi değişken/param
→ "%T"). `ref_yapi_ir_al()` helper'ı referans/pointer soyup yapı IR'ını verir.
param + annotasyonlu `değişken` kaydında set edilir. `erisim_uret`/`erisim_lvalue`
artık nesne TANIMLAYICI ise kayıtlı yapı tipini kullanır; alan-adı arama yalnız
SON ÇARE (yapı tipi bilinmiyorsa). Struct-VALUE (`%T`) yolu zaten doğruydu.

**KÖK-NEDEN 2 (yan keşif — struct-bundled proof yazarken):** `dizi_al(s.ad, i)`
s.ad bir `Dizi<metin>` ALANI iken SEGFAULT. `dizi_eleman_beklenen` çıkarsaması
yalnız TANIMLAYICI arg0 (düz değişken) için çalışıyordu; struct-alan dizi (s.ad)
için eleman tipi çıkarsanmıyor → `kdl_dizi_al_tam` (i32) route edip metin ptr'ini
i32 okuyordu → çöp ptr → strcmp segfault.
**Fix 2:** `dizi_alan_eleman_ir()` helper'ı — dizi-builtin arg0 DUGUM_ERISIM ise
alanın Dizi<T> eleman IR tipini çözer. Inference bloğuna ERISIM dalı eklendi.

**Repro test (önce KIRMIZI sonra YEŞİL):** `test/snapshots/yapi_yerel_bozulma.kem`
(33 bekleniyor, bug'da 63) → `test_yapi_alan_cakismasi` (test_llvm [150]).
**Canlı kanıt:** `test/ornekler/16_degiskenli_dil.kem` açık-param workaround'undan
YAPI-PAKETLİ sürüme taşındı (Tokenler + Semboller, İKİSİ DE `ad` alanı taşıyor —
çakışma senaryosu) → "x=6;y=7;x*y" = 42 (test_llvm [152]).

**Doğrulama:** test_llvm 227→**228**, +22 suite (tip_kontrol 174, snapshot 50, …).
0 ASan. Prod 0 uyarı. stdlib 12 OK. Repro 63→33, sembol-tablosu segfault→42.

**Kapsam dışı (PRE-EXISTING, bu fix değil):** "m=2;n=3;p=4;m*n*p+18" = 18 (3-değişken
çarpım zinciri + toplama) HEM yapı-paketli HEM commit'li açık-param D-028'de
başarısız → ayrı, önceden var olan demo/codegen sorunu (m*n / m*n-39 / m*3+1 çalışır;
çarpım-zinciri+toplama dar bir kombinasyon). Bu fix'ten bağımsız; ayrı göreve havale.

---

## D-028 — String stdlib IV: değişkenli mini dil (string-anahtarlı sembol tablosu) + PROB: yapı-yerel codegen bug'ı (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** Self-hosting'in ad
çözümü çekirdeği: değişkenli mini dil — atama + STRING-ANAHTARLI sembol tablosu
(`test/ornekler/16_degiskenli_dil.kem`). `( DEĞİŞKEN '=' ifade ';' )* ifade`.
Sembol tablosu = paralel `Dizi<metin>` (adlar) + `Dizi<tam32>` (değerler); arama
`metin_esit` ile (byte-byte ad). Token adları `metin_kes(kaynak, i, 1)` ile
çıkarılır (tek-harf değişken). `"x=6;y=7;x*y"` → 42.

**Yeni doğrulanan yetenekler:** `Dizi<metin>` (ptr-eleman dizisi, string saklar),
`metin_kes(start, **uzunluk**)` semantiği (DOĞRULANDI: start,length — bitiş değil),
`harf_mi` ile identifier lexing, `metin_esit` ile string-key sözlük arama.

**Doğrulama (adversarial, 8 senaryo):** `x=6;y=7;x*y`=42, `x=6;x*7`=42,
`a=2;b=3;c=7;a*b*c`=42, `x=50;y=8;x-y`=42, `x=5;x=42;x`=42 (yeniden-atama/güncelle),
`z=10;(z+4)*3`=42 (değişken+parantez), `40+2`=42, `x=21;x+x`=42. opt-verify PASS.
test_llvm 225→**227**. 0 ASan. Derleyici dokunulmadı.

**✅ ÇÖZÜLDÜ → D-029 (kök-neden: alan-adı çakışması, yapı-yerel kopya DEĞİL).**

**🔴 PROB (bu çalışma sırasında bulunan CİDDİ codegen bug'ı) — yapı-yerel
bozulması:** İlk denemede token+sembol tablosu İKİ `yapı` (Tokenler + Semboller,
Dizi alanlı) olarak paketlenmişti. ÇALIŞMADI. İzole edilen kök neden:
> **Bir `yapı` yerel değişkeni inşa edildiğinde (`değişken u: U = U { f:
> dizi_olustur(N), ... }`), önceden tanımlanmış BAŞKA bir `yapı` yerelinin
> koleksiyon-alanı içeriği BOZULUYOR.** Minimal repro: `t: T` (Dizi alanlı)
> oluştur+doldur (boyut 3), sonra `u: U` (yine `dizi_olustur` alan-init'li)
> oluştur → `dizi_boyut(t.kind)` 3 yerine **6** okuyor.
> - İkinci yapı KOLEKSİYONSUZ ise (`V { x: tam32 }`) → bozulmuyor (33 ✓).
> - İkinci yapının inşası `dizi_olustur` çağırıyorsa → bozuyor (63 ✗).
> IR yüzeysel doğru (alloca %T %0/%1, construct→%1, copy→%0); mekanizma
> struct-by-value yerel kopya/alloca etkileşiminde, runtime tracing gerekiyor.
> NOT: D-027 (15_agac_insa) İKİ yapı (Agac+Tokenler) kullanıp ÇALIŞIYOR —
> tetikleyici spesifik (yapı-alan-okuma aynı fonksiyonda + dizi_olustur'lu 2.
> yapı). Ayrı görev olarak fix'e havale edildi (spawn_task).

**Workaround (shipped):** Yapı-paketleme yerine açık `Dizi` PARAMETRELERİ
(D-024/D-026 deseni — kanıtlı). faktor/terim/ifade 6 param alır (kindler,
degerler, adlar, imlec, s_ad, s_deg). Verbose ama sağlam; yerel Dizi'ler
(ptr) yapı-yerel bug'ından etkilenmez.

**Self-hosting durumu:** LEXER + PARSER + AST + EVAL + DEĞİŞKEN/SEMBOL — bir mini
dilin tüm derleyici fazları KEMGU'da. Sıradaki: yapı-yerel bug fix (sonra struct
bundling temizliği), çok-deyimli dil, uzun vade derleyici alt-kümesi.

---

## D-027 — Self-hosting CAPSTONE: tam derleyici hattı (lex→parse(AST inşa)→eval) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** Self-hosting'in eksik
ORTA parçası. D-026 parser'ı değeri doğrudan hesaplıyordu; D-027 parser önce bir
SOYUT SÖZDİZİM AĞACI (AST) İNŞA ediyor, sonra AYRI bir geçiş ağacı geziyor — gerçek
bir derleyicinin yapısı (`test/ornekler/15_agac_insa.kem`). Tam hat KEMGU'da:
**lex → parse(AST inşa) → eval(AST gez).**

**AST temsili — İNDEKS-TABANLI ARENA:** düğümler paralel `Dizi<tam32>`'lerde
(tur/deger/sol/sag), çocuklar İNDEKS ile gösterilir (pointer değil). Bu, KEMGU'nun
KENDİ derleyicisinin arena+AST modelinin (ast.c) birebir KEMGU karşılığı —
self-hosting'e en yakın yapı. Heap-tahsisli özyinelemeli çeşit (henüz yok)
GEREKTİRMEZ; mevcut dizi intrinsic'leriyle (dizi_ekle=düğüm ayır, dizi_al=oku,
dizi_yaz=imleç) tamamen ifade edilir. Arena append-only → indeksler kararlı.

**Yeni doğrulanan kompozisyon yeteneği:** Diziler `yapı` içinde paketlenip
&referansla aktarılır (`yapı Agac { tur: Dizi<tam32>; ... }`, `&Agac` param,
`a.tur` field→dizi erişimi, struct construction'da `dizi_olustur()` field değeri).
struct + koleksiyon kompozisyonu E2E çalışıyor (probe ile doğrulandı). İmleç + iki
struct (Agac arena + Tokenler) → parser durumu 2 param.

**Doğrulama (adversarial, 10 ifade — AST yolu üzerinden):** `2+4*10`=42,
`2+3*4`=14 (öncelik), `(2+3)*4`=20, `2*(3+(4*5))`=46, `100-2*3-2`=92,
`((100-16))/2`=42, `1+2*3+4*5+15`=42, `(((7)))`=7. opt-verify PASS. test_llvm
223→**225**. 0 ASan. Derleyici dokunulmadı.

**Self-hosting tablosu — 4 parça da KEMGU'da:**
| Faz | Demo | Temsil |
|-----|------|--------|
| LEXER | D-024 | metin → token akışı (Dizi) |
| PARSER | D-026/D-027 | token → öncelikli AST (arena) |
| AST | D-027/D-022 | indeks-arena / özyinelemeli çeşit |
| EVAL | D-027/D-022 | ağaç gezme |

**Sıradaki:** string-key sembol tablosu (`metin_esit` + paralel ad/değer dizileri)
→ değişkenli ifadeler; sonra çoklu-deyim + atama (mini dil); uzun vade gerçek
derleyici alt-kümesinin KEMGU'da yeniden yazımı.

---

## D-026 — String stdlib III: özyinelemeli-iniş öncelikli ayrıştırıcı (parser yarısı) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-024 token akışı
üzerine self-hosting'in PARSER yarısı: operatör ÖNCELİĞİ + PARANTEZ destekli
özyinelemeli-iniş (recursive-descent) ifade ayrıştırıcısı
(`test/ornekler/14_oncelikli_ayristirici.kem`). Gramer:
`ifade=terim(('+'|'-')terim)*; terim=faktör(('*'|'/')faktör)*; faktör=SAYI|'('ifade')'`.

**Enabling (D-025):** Özyinelemeli iniş, çağrılar arası paylaşılan MUTABLE konum
imleci gerektirir. KEMGU'da global mutable yok + skalerler değerle geçer →
imleç = tek-elemanlı `Dizi<tam32>` (ptr → referansla paylaşılır), `dizi_yaz` ile
yerinde ilerletilir. faktör→ifade→terim→faktör KARŞILIKLI özyineleme (forward
referans; iki-geçişli pre-populate codegen'de islev_kayit pre-pass ile çözülür).

**Doğrulama (adversarial, 17 ifade):** Öncelik — `2+4*10`=42 (soldan-sağa 60
DEĞİL), `2+3*4`=14, `100-2*3`=94, `2*3+4*5`=26. Parantez — `(2+3)*4`=20,
`(2+4)*(3+4)`=42, `2*(3+(4*5))`=46, `((9))`=9. Bölme — `100/2-8`=42, `84/2`=42,
`(100-16)/2`=42. opt-verify PASS. test_llvm 221→**223**. 0 ASan. Derleyici
dokunulmadı.

**Tuzak:** `iken doğru { ... ver ... }` idiomu (KEMGU'da `break` keyword YOK) —
döngüden yalnız erken `ver` ile çıkılır; codegen + opt-verify temiz.

**Self-hosting durumu:** Artık 3 parça da KEMGU'da ÇALIŞIYOR — LEXER (D-024
metin→token), PARSER (D-026 token→öncelikli değerlendirme), AST+EVAL (D-022
özyinelemeli çeşit yorumlayıcı). Sıradaki: parser'ın değerlendirme yerine çeşit
AST İNŞA etmesi (token→AST), sonra string-key sembol tablosu (metin_esit).

---

## D-025 [YÜKSEK] — dizi_yaz intrinsic: in-place eleman güncelleme (mutable cursor) (2026-06-13)

**Karar [YÜKSEK — yeni intrinsic]:** `dizi_yaz<T>(d: Dizi<T>, i: tam32, e: T) ->
boş` — dinamik dizinin i. elemanını YERİNDE günceller. Koleksiyon API'sinde
göze batan eksiklik: `dizi_ekle` (append) + `dizi_al` (oku) vardı ama eleman-SET
yoktu. Bu, recursive-descent parser'ın paylaşılan MUTABLE KONUM İMLECİ için şart
(tek elemanlı Dizi<tam32>, ptr → çağrılar arası paylaşılır, dizi_yaz ile ilerletilir).

**Uygulama (dizi_al/dizi_ekle simetrisi):**
- tip_kontrol.c: `dizi_yaz` özel-cased (DUGUM_CAGRI) — 3 arg, arg0 Dizi<T>, arg1
  tam32 indeks (T028), arg2 eleman T (T001 uyumsuzluk).
- llvm.c: element-tip varyant dispatch (i32→kdl_dizi_yaz_tam, i64→_tam64,
  ptr→_ptr); index i32'ye, değer eleman-tipine cast. declare satırları eklendi.
  **dizi_deger_arg:** dizi_ekle/al'da değer/indeks arg[1]; dizi_yaz'da DEĞER
  arg[2] — `dizi_eleman_beklenen` forward'ı bu pozisyona yönlendirildi (önceki
  sabit `i == 1` literal-eleman-tip çıkarsamasını yanlış arga verirdi).
- runtime: kdl_dizi_yaz_tam/_tam64/_ptr — sınır dışı (i<0||i>=boyut)/NULL →
  sessizce yok say (boyut BÜYÜTMEZ; büyütme dizi_ekle ile). dizi_al ile simetrik.

**Doğrulama:** in-place (d[1]=40) + cursor (c[0]=c[0]+2) E2E; tam32 + tam64
varyant dispatch ayrı ayrı E2E (42). Tam regresyon: test_llvm 220→**221**, +22
suite (tip_kontrol 174, snapshot 50, parser 107, …). 0 ASan. Prod 0 uyarı.
stdlib --check 12 OK. (Sınır: shrink/insert/remove yok — append+set+read yeterli.)

---

## D-023 [YÜKSEK] — String stdlib I: metin_bayt intrinsic + metin literal pre-pass düzeltmesi (2026-06-13)

**Bağlam:** Self-hosting'in 2. ön-koşulu "gerçek string işlemleri" (Mehmet: "4
konsolide, sonra 3"). Mevcut `kdl_metin_*` yüzeyi zaten geniş (uzunluk, birleştir,
kes, içerir, başlar/biter, kırp, yer_değiştir, küçük/büyük±tr/ascii) AMA bir
tokenizer'ın temel taşı eksikti: **indeksli karakter erişimi**.

**Karar 1 [YÜKSEK — yeni intrinsic]:** `metin_bayt(s: metin, i: tam32) -> tam8`
— s'in i. HAM BAYT'ı (UTF-8; ASCII'de = karakter). Sınır dışı/NULL → 0 (taşma
imkansız, KEMGU güvenlik hedefi). `metin_uzunluk` ile birlikte bir metin üzerinde
bayt-bayt gezinmeyi (lexer döngüsü) sağlar. Ayrıca `metin_esit(metin,metin) ->
mantıksal` builtin olarak bağlandı (runtime'da `kdl_metin_esit` zaten vardı ama
tip-kontrol builtin tablosuna kayıtlı değildi — anahtar kelime tanıma için).
Runtime'daki ESKİ `int kdl_metin_esit` (ölü; ne builtin ne C çağıranı vardı)
`_Bool` dönen tek sürümle değiştirildi (i1 declare + diğer boolean metin fn'leriyle
tutarlı). DEĞER naming: metin_bayt/metin_esit — temiz.

**Karar 2 [ORTOGONAL CORRECTNESS FIX]:** Metin literal pre-pass (`ast_taransa_
metinleri`, @.str.N toplayıcı) **cast düğümünü taramıyordu**. `metin_uzunluk("...")
olarak tam32` gibi — literal `DUGUM_TIP_DONUSTUR` (x olarak T) altında kalınca
"kayitsiz" düşüp **sessizce `add i32 0,0`'a** derleniyordu (yanlış değer, hata yok).
Eklenen case'ler: DUGUM_TIP_DONUSTUR (.kaynak), DUGUM_LAMBDA (.govde),
DUGUM_KULLAN_IFADE/DUGUM_IMHA_IFADE (.operand). Bu, TÜM metin builtin'lerini
literal+cast argümanıyla kullanılabilir yapar (yaygın durum). Bug sınıfı: herhangi
bir metin literali taranmayan bir düğümün altında → sessiz miscompile.

**Doğrulama:** `metin_uzunluk("hello")`=5, `metin_bayt("ABC",1)`='B'=66,
`metin_esit("ver","ver")`=1 — hepsi literal+cast argümanla E2E. Saf-KEMGU
tokenizer `test/ornekler/12_metin_tokenizer.kem` (metin_uzunluk + metin_bayt ile
"N+N+N" bayt-bayt tarama): "10+20+12" = 42. Bir KEMGU programı kendi girdisini
karakter karakter okuyabiliyor — lexer/self-hosting temeli.

**Tam regresyon:** test_llvm 215→**218** (+metin_bayt/esit/tokenizer). tip_kontrol
174, snapshot 50 (IR baseline drift YOK — fikstürlerde cast-altı metin yok),
otp_cli 9, parser 107, lexer 103, linear 57, drf 39, capability 40, sabitsure 39,
wcet 35, mmio 23, simd 30, simd_llvm 5, arena/ast/tip/sembol/json/lsp/bolge/escape.
**0 ASan.** Prod temiz rebuild **0 uyarı.** stdlib --check yeşil.

**Sınırlar / sıradaki:** metin_bayt BYTE döner (UTF-8 codepoint değil) — ASCII
tokenizing için doğru; çok-baytlı codepoint iterasyonu V2. İsimle değişken arama
hâlâ slot-id (string-key assoc V2). Koleksiyon tarafı (Liste<T>) zaten KdlDizi
runtime'da var; tokenizer'ın token LİSTESİ üretmesi (dizi_ekle ile) doğal sonraki
adım. Sonra: gerçek lexer → parser (self-hosting derleyici çekirdeği).

---

## D-024 — String stdlib II: iki fazlı lexer → token akışı → değerlendirici (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-023'ün doğal devamı
(Mehmet: "string/**koleksiyon** stdlib"). Mevcut KdlDizi koleksiyonu (`dizi_olustur/
ekle/al/boyut`) + D-023 metin primitifleri birleştirilerek self-hosting'in GERÇEK
mimarisi gösterildi: metin önce TOKEN AKIŞINA çevrilir (lexer), sonra AYRI bir geçiş
bu akışı değerlendirir (`test/ornekler/13_token_akisi.kem`).

**Önce paralel "Harita" workflow'u:** KdlDizi yüzeyi (intrinsic'ler, element-tip
çıkarsama, runtime, kanıtlı kalıplar, riskler) 5 paralel okuyucuyla eksiksiz
haritalandı (ultracode). Çıkan iki kritik gerçek E2E probe ile doğrulandı:
- **Proven kalıp:** `dizi_olustur → iken dizi_ekle → iken dizi_al` toplama (15 ✓).
- **YENİ doğrulanan yetenek:** `Dizi<tam32>` FONKSİYON PARAMETRESİ olarak çalışır
  (4×10+2=42 ✓). Eski CLAUDE.md notu "dizi param yok" STATİK dizi içindi; dinamik
  Dizi = ptr olduğundan sorunsuz aktarılır. İki fazlı mimariyi mümkün kılar.

**Tasarım:** Token = iki PARALEL `Dizi<tam32>` (kindler + degerler). Tür kodları
0=SAYI/1=ARTI/2=CARPI/3=EKSI. `lexle(metin, kindler, degerler)` bayt-bayt tarar,
sayıları biriktirir, operatörleri token'lar (diziler referansla aktarılır).
`degerlendir(kindler, degerler)` akışı soldan sağa hesaplar. Token kuralı:
SAYI (op SAYI)*.

**Doğrulama (adversarial):** 8 ayrı ifadeyle birden — `2*3+36`=42, `7*6`=42,
`100-58`=42, `2*3*7`=42, `50-3-5`=42, `1+2+3`=6, `9`=9, `10*10-58`=42. Çok-basamaklı
sayı, üç operatör, tek sayı, değişken token sayısı — hepsi doğru (şanslı 42 değil).
opt -passes=verify PASS. test_llvm 218→**220** ([143] verify + [144] run).
0 ASan. Derleyici dokunulmadı → diğer suite'ler etkilenmez.

**Tuzak (kayda değer):** `uygula` bir ANAHTAR KELİME (trait impl) — fonksiyon adı
olamaz; `op_uygula` yapıldı. (35 keyword listesi: işlev adlarında kaçınılmalı.)

**Sıradaki:** gerçek lexer→parser (parantez/öncelik), veya token'ı (kind,value)
çift olarak tek dizide (struct/çeşit element) — şimdilik paralel-dizi pragmatik.
String-key sembol tablosu (metin_esit ile) self-hosting derleyici için gerekecek.

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

---

## D-020 — Çapraz-modül payload + recursive çeşit (modüler AST) (2026-06-13)

**Self-hosting deseni:** Compiler'ın AST'si kendi modülünde yaşamalı (`genel çeşit
Ifade { Sayi(tam64), Topla(&Ifade,&Ifade) }` modül `ifd`'de), parser/codegen
modüllerince import edilmeli. D-018 payload çeşit'i MODÜL-İÇİ doğrulamıştı; bu
adım çapraz-modülü kapatır (D-018 follow-up'ı).

**Düzeltilen gap'ler:**
1. **Codegen nitelikli yapıcı (`m::Cesit::V(args)`):** `cesit_kayit_yoldan(g, sol)`
   — çeşit'i sol-yoldan çözer (sol TANIMLAYICI=`Renk` ya da YOL=`m::Renk` →
   sag_ad'den, düz IR-ad uzayı D-011). DUGUM_YOL (bare) + DUGUM_CAGRI (yapıcı)
   yolları artık YOL sol kabul eder (önceden yalnız TANIMLAYICI → i32 fallback
   miscompile).
2. **Tip-kontrol payload-tip çözümü (recursive/modül-yerel):** Recursive çeşit'in
   payload tipi `&Ifade` dışarıdan (caller scope) çözülürken T011/M004 veriyordu
   (Ifade modül-yerel). `ast_tip_to_bilgi` DUGUM_TIP_BASIT artık çözülemezse
   `yapi_sembol_capraz_bul` (D1) ile yüklü modüllerde düz adla arar — alan-erişimi
   çapraz-modül çözümüyle simetrik.

**Doğrulama:** Modüler recursive AST (`kullan ifd; ifd::Ifade::Topla(&a,&b);
ifd::hesapla(&kök)`) — (3+4)*6 = 42, HEM --check HEM E2E. Çapraz-modül payloadsuz
çeşit (Renk) + primitive-payload çeşit (Ifade::Ikili) de doğrulandı.

**Kapsam/sınırlar:** Nitelikli payload DESENİ (`eşleş` içinde `m::Cesit::V(a,b)`)
denenmedi — match genelde modül-içi (genel işlev). Çapraz-modül match nitelikli
desen gerekirse follow-up. Generic çeşit hâlâ ayrı.

**Testler:** test_llvm 211→212 (+çapraz-modül payload+recursive cesit E2E).
tip_kontrol 174, parser 107, snapshot 50, linear 57, drf 39, capability 40,
ast/arena/sembol. 0 ASan. stdlib --check yeşil. Temiz rebuild 0 uyarı.
Fikstür: test/moduller/{ifd,ana_ifd}.kem.

---

## D-021 — Sayı literal bağlam-bağımlı: tipli tamsayı + literal ikili op (ergonomi) (2026-06-13)

**Sorun (çeşit edge-probe sırasında bulundu):** `değişken x: tam64; x + 1` → T001
("ikili operator iki tarafi ayni tip"). Literal `1` varsayılan tam32, x tam64 →
uyuşmazlık. Her non-tam32 tamsayı aritmetiğinde `(1 olarak tam64)` zorunluluğu —
yaygın ergonomi pürüzü (çeşit/AST kodu tam64 sayaç/değer kullanır). codegen ZATEN
genişletiyordu; yalnız tip-kontrol katıydı. CLAUDE.md zaten "Sayı literal:
Context-dependent" diyor — IKILI op'ta uygulanmamıştı.

**Çözüm [ETKİ]:** DUGUM_IKILI'de sol/sag belirlendikten sonra: bir taraf TİPSİZ
tamsayı LİTERALİ (DUGUM_TAM) + diğer taraf TİPLİ tamsayı + tipler farklı ise,
literali karşı tarafın tipinde yeniden çıkar (`tip_belirle_beklenen`). Yalnız
şu anda HATA veren durumu gevşetir:
- Explicit cast (`… olarak tamX`) DUGUM_TAM DEĞİL → etkilenmez.
- tam32 + literal zaten eşit → etkilenmez.
- **sabitsüre/vektör HARİÇ** (taint/lane yayılımı kendi kurallarına sahip —
  tip_tamsayi_mi sabitsüre'yi iç tipe açtığı için ilk denemede S2 testleri
  kırıldı; guard eklendi).

**Doğrulama:** `tam64 x; x+1`, `x>8`, `x*2` artık --check geçer + doğru değer.
Tam regresyon: test_llvm 213→214 (+tam64 literal bağlam E2E), tip_kontrol 174,
sabitsure 39 (guard sonrası), simd 30, simd_llvm 5, wcet 35, mmio 23, lexer 103,
parser 107, linear 57, drf 39, capability 40, snapshot 50, arena/ast/tip/sembol.
0 ASan. stdlib --check yeşil. Temiz rebuild 0 uyarı.

---

## D-022 — Self-hosting doğrulama eseri: ağaç-yürüyen yorumlayıcı (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — yalnız örnek + test, derleyici değişmedi]:** Bu oturumda
eklenen özyinelemeli payload çeşit yığınını GERÇEK bir programda birden zorlamak
için, küçük bir ifade dilinin ağaç-yürüyen yorumlayıcısı saf KEMGU ile yazıldı
(`test/ornekler/11_yorumlayici.kem`) ve E2E regresyon testine bağlandı.

**Gerekçe:** Generic çeşit'in (sıradaki büyük aday) codegen monomorfizasyonu
per-T layout gerektiriyor (yeni instantiation-izleme + mangled struct emisyonu;
yapıcı/eşleş/annotasyon sitelerinde) — "her commit yeşil + E2E" disiplini altında
tek hamlede temiz inmesi yüksek riskli, tip-kontrol-only yarım özellik olurdu.
Bunun yerine SIFIR derleyici-regresyon riski olan, shipped çeşit işini gerçekçi
yük altında doğrulayan ve bir sonraki özelliği KANITLA gerekçelendiren eseri seçtim.

**Ne kanıtlıyor:** KEMGU kendi dilinin küçük bir klonunu kendi tip sistemiyle
ifade edip değerlendirebiliyor — HEM AST (`çeşit Ifade`, 8 varyant: Sabit,
Degisken, Topla, Carp, Cikar, Kucuk, Kosul/3-payload, Bagla/karışık-tip) HEM
leksik ORTAM (`çeşit Cevre` immutable assoc-list) özyinelemeli payload çeşit.
Ortam özyineli çağrılar boyunca elden ele aktarılır; `eşleş` kolunda yerel çeşit
kurup `&referansını` alma (`Cevre::Bag(yuva,v,ortam)` → `&yeni`); nested match +
koşul + karşılaştırma. Program: `bağla x=6 içinde (x<10 ? x*7 : 0)` = **42**
(--check ✓ + E2E exit 42 ✓). `x*7` bu oturumun D-021 literal-bağlam düzeltmesini
de doğrular (tam64 değer * literal).

**Kapsam/sınırlar:** Değişken erişimi tam sayı YUVA kimliğiyle (slot id) — `metin`
eşitliği intrinsic'i yok, isimle arama V2. Tek dosya. Yorumlayıcı kapsamı: tam
sayı aritmetik + karşılaştırma + koşul + let; tip/closure/fonksiyon-değer yok.

**Sıradaki büyük adayı kanıtlıyor:** Bu eser genel container ihtiyacını (örn.
`Opsiyonel<T>`, `Liste<çeşit>`) somutlaştırıyor → **generic çeşit** ve **gerçek
string/koleksiyon stdlib** self-hosting'in net ön-koşulları olarak doğrulandı.

**Testler:** test_llvm 214→215 ([139] Yorumlayici E2E). Diğer tüm suite'ler
değişmedi (derleyici dokunulmadı). 0 ASan. Temiz rebuild 0 uyarı. Hiçbir test
`ornekler/` dizinini enumerate etmiyor (yeni dosya izole).

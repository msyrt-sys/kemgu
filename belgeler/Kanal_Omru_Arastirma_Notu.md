# Kanal Ömrü — Araştırma Notu

> **[D-532]** Bu bir **tasarım notudur, uygulama değildir.** D-515'te dar bir
> kanal-serbestı ölçülüp **bilerek yazılmadı**; bu not o kararın *neden* doğru
> olduğunu ve gerçek çözümün *ne gerektirdiğini* kayda geçirir. Kod
> değişikliği YOK.

## Bugünkü durum (ölçüldü, tahmin değil)

```
runtime API      : kdl_kanal_olustur · _gonder · _al · _bos_mu · _serbest
kdl_kanal_serbest: runtime/kdl_runtime.c'de VAR ve DOGRU (kilit yok et,
                   veri free, kanal free)
cagiran           : src/llvm.c        -> 0
                    selfhost/codegen.kem -> 0
```

Yani **yaşam döngüsünün yok-etme ucu yazılmış ama hiçbir derleyici çağırmıyor.**
D-462'nin *"kod var, hiçbir ölçüm ateşlemiyor"* sınıfı.

## Neden dar çözüm YAZILMADI (D-515'in ölçümü)

İlk sezgi *"kanalı yaratan işlevin sonunda, hapsedilmişse serbest bırak"*tı.
İki ölçüm bunu çürüttü:

1. **Altküme boş değil** — ilk varsayımım yanlıştı. `kanal_oluştur` kullanan
   dosyaların bir kısmında `görev_başlat` **hiç yok** (tamponlu kanal tek
   thread'de geçerli bir desendir).
   ⚠ Sayı zamanla değişir — D-515'te **13 dosyanın 5'i**, bu not yazılırken
   **16 dosyanın 8'i** ölçüldü (korpus büyüdü). *Belgeye gömülü sayı yazıldığı
   gün doğrudur; kararı sayıya değil ORANA dayandır.*
2. **Ama o dosyalar SIZDIRMIYOR.** ASan+LSan ile koşuldu: **sıfır sızıntı
   raporu**. Sebep: kanal işaretçisi `main`'in canlı yuvasında duruyor →
   LeakSanitizer *ulaşılabilir* tahsisi sızıntı saymaz.

Ölçülen tek kanal sızıntısı **`kanal_mesaj`**dır ve o **görevlere yakalanan**
kanaldır — yani dar kuralın **kapsamadığı** dosya. Dar çözüm, ölçülebilir
kazanç üretmeden bir bellek-serbest yolu eklerdi: **çift-serbest/UAF riski
gerçek, kazanç görünmez.**

## Gerçek soru: "kanalı tutan tüm görevler birleştirildi mi?"

Kanalı güvenle serbest bırakmak için şu **üçünün birden** doğru olması gerekir:

1. Kanalı **yaratan** işlev dönüyor.
2. Kanalı **yakalayan** her görev **birleştirilmiş** (`görev_birleştir`).
3. Kanal, yaratanın çerçevesinden **kaçmamış** (dönüş/yapı alanı/küresel).

(3) zaten var: `ky_confined` bu soruyu yanıtlıyor ve 18-UAF avından geçmiş bir
makine. (1) trivial. **Eksik olan (2)'dir.**

### (2) neden zor

`ky_confined`in LAMBDA dalı *"gövde bu adı yakalıyorsa DENY"* der — yani
görevlere yakalanan kanal **zaten** hapsedilmemiş sayılır. Bu **sağlamdır**
ama **çok kaba**: yakalayan görevlerin *birleştirilmiş olup olmadığını*
sormaz. Doğru yüklem şudur:

> Kanalı yakalayan **her** `görev<T>` tanıtıcısı, yaratan işlevin **her çıkış
> yolunda** `görev_birleştir` ile tüketilmiş mi?

Bu, **akış-duyarlı** bir sorudur ve depoda benzeri **zaten var**: lineer
tüketim takibi (L001/L002/L005) tam olarak *"her dalda tüketildi mi"* sorusunu
yanıtlıyor ve D-311/D-312'de dal-duyarlı hale getirildi (`eğer` · `eşleş` ·
döngü). `görev<T>` **zaten lineerdir** — birleştirilmezse L001 verir.

**Yani mekanizma var; eksik olan bağlantı:** kanal ↔ onu yakalayan görevler
eşlemesi. Bugün hiçbir yan-kanal bunu tutmuyor.

## Üç seçenek (uygulanmadı — ölçülüp karşılaştırıldı)

| | Yaklaşım | Kazanç | Bedel |
|---|---|---|---|
| **A** | Yaratan işlevde: kanalı yakalayan görev listesini topla; **hepsi** L-tüketilmişse `ret` öncesi `kdl_kanal_serbest` | `kanal_mesaj`ın 8+168+16 baytı kapanır; mekanizma mevcut lineer makineden türer | Yeni yan-kanal (kanal→görev eşlemesi) + üç uygulamaya port (D-517'nin dersi) |
| **B** | `kanal<T>`yi **lineer** yap (`tekkez` gibi) → kapatmayı unutmak L001 | Yapısal; yeni analiz gerekmez | **Kanalın varlık sebebine aykırı**: D-505'te kanal, görevler arası paylaşım için taşımadan *bilerek muaf tutuldu*. Lineer kanal ikinci `kanal_gönder`i imkânsız kılar |
| **C** | Bırak sızsın (bugünkü hâl) | Risk sıfır | Süreç ömrü boyunca kanal başına ~192 bayt |

**A** tek gerçekçi yol; **B** dil semantiğini bozar; **C** bugünkü bilinçli hâl.

## A'nın ön koşulları (yapılmadan başlanmamalı)

1. **Ölçüm önce:** depoda kaç dosyada *"kanal yakalayan tüm görevler aynı
   işlevde birleştiriliyor"* şekli var? **Tavan sıfıra yakınsa A da D-515 gibi
   ölçülemez bir değişiklik olur** (D-430).
   **KABA ÖLÇÜM YAPILDI (yeterli DEĞİL):** kanal + `görev_başlat` içeren
   **8 dosya** var ve birçoğunda `başlat` sayısı `birleştir` sayısından
   **fazla** görünüyor (ör. `kanal_mesaj` 2/1, `codegen.kem` 28/13).
   ⚠ **BU SAYILAR GÜVENİLİR DEĞİL** — `grep` yorumları da sayıyor ve bu
   dosyaların başlıkları bu adları bolca anıyor. Gerçek ölçüm **AST üzerinden**
   yapılmalı (çağrı düğümü say, metin değil). *Bu notun kendi kuralı: sayıyı
   ölçüm aracına değil, doğru araca dayandır.*
2. **Default-DENY**: yakalayan görevlerden **biri bile** izlenemiyorsa (dolaylı
   çağrı, ada bağlı kapanış, çapraz-dosya) serbest **yayılmamalı**. D-494'ün
   *"sızıntı bir hata, UAF bir felaket"* ilkesi.
3. **Üç uygulama**: `src/tip_kontrol.c` (ya da `escape.c`) + `selfhost/checker.kem`
   + `selfhost/codegen.kem`. D-517 ve D-519'da bu tuzağa **üç kez** düşüldü.
4. **Ölçülebilir kapı**: `asan_denetim`in `SIZINTI_MUAF` listesinden
   `kanal_mesaj` **çıkarılabilmeli**. Çıkarılamıyorsa değişiklik ölçülmemiştir.

## Karar

**Şimdilik C (bugünkü hâl) korunuyor.** `kdl_kanal_serbest` **bilerek ölü
kalıyor** — silinmemeli: onu canlandıracak şey A'dır ve o gelene kadar ölü kod
burada **sessiz-başarısız bir yol açmıyor** (hiç çağrılmıyor). D-459'un
*"ölü kodu bırakma"* kuralının tersi bir durum: orada ölü kod bir tuzaktı,
burada bir yer tutucu.

**A'ya girişmek dil yüzeyi kararı değildir** (yeni sözdizimi/tanı kodu
gerektirmez, mevcut L001/L002 makinesini kullanır) — ama **ölçüm önce**
maddesi (yukarıda 1) yerine getirilmeden başlanmamalıdır.

---

## [D-540] ÖN KOŞUL 1 ÖLÇÜLDÜ — tavan SIFIR DEĞİL

Ölçüm `kemgu --ast` üzerinden yapıldı (`test/kanal_omru_olcum.py`), **grep ile
değil** — bu notun kendi uyarısı gereği.

```
kanal yaratan islev: 23   |  sekil TUTUYOR: 23  |  TUTMUYOR: 0
bunlardan yakalayan >= 1 (asil zor vaka): 4
  cg_gorev_kanal.main      yakalayan=1 join=1
  cg_rho_sahip_kacis.main  yakalayan=1 join=1
  drf_gorunurluk.main      yakalayan=1 join=1
  kanal_mesaj.main         yakalayan=1 join=1   <- D-515'te SIZDIGI olculen dosya
```

**SONUÇ: A ölçülebilir bir değişikliktir.** 23 işlevin tamamı şekli sağlıyor;
dördü birleştirme akıl yürütmesi gerektiren gerçek vaka. Daha önemlisi,
**D-515'te LeakSanitizer ile sızdığı ölçülen tek dosya (`kanal_mesaj`) bu
kümenin İÇİNDE** — yani A'nın kapatacağı gerçek bir sızıntı var.

Bu, D-515'in dar kuralından farklıdır: o kural görevlere **yakalanan** kanalı
bilerek dışarıda bırakıyordu ve tam da bu yüzden `kanal_mesaj`ı kapsamıyordu.
A birleştirme-duyarlı olduğu için kapsıyor.

**⚠ ÖLÇÜM ARACI ÖNCE YANLIŞTI:** ilk sürüm `gönderen(k)`/`alan(k)`
projeksiyonlarını izlemiyordu → `kanal_mesaj` için `yakalayan=0`. D-511'in
bulgusuyla çelişince araç şüpheli kılındı (D-500) ve takma ad izleme eklendi.
İki bağımsız ölçüm ancak ondan sonra uyuştu.

**BU ADIM KOD YAZMADI.** Madde yalnız ön koşulu ölçmeyi istiyordu; A'nın
kendisi bellek serbest bırakan yeni kod demektir (D-515'in çift-serbest/UAF
uyarısı geçerli) ve ayrı bir iştir.


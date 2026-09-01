# LOOP
## Kurallar
- Bana hiçbir şey sorma. "Onaylıyor musun", "şunu öneriyorum", "devam edeyim mi", "sıradaki işe geçeyim mi" yazma. Öneri üretip bekleme; seç ve uygula.
- Belirsizlikte en muhafazakar secenegi kendin sec, gerekcesini Gunluk'e yaz, devam et.
- Her iterasyonda SADECE bir madde bitir.
- Her madde icin ne yapildi, neden oyle yapildi, hangi kapi neyi olctu ve yanlis giden denemeler (ornegin gecersiz sabotaj) Gunluk'e yazilir. Uzunluk sinirli degil; kayit denetim izidir.

## Iterasyon
1. Bu dosyayi oku, Sirada listesinin en ustundeki maddeyi al.
2. Uygula.
3. Testleri calistir. Gecerse commit et. Kirilirsa duzelt, ayni iterasyonda tekrar dene.
4. Bu dosyayi guncelle: maddeyi Sirada'dan cikar, Gunluk'e tek satir ekle (tarih + ne yapildi + sonuc). Yeni is ciktiysa Sirada'nin sonuna ekle.

## Sirada

- [ ] D-531 BULGU 2: self-host legacy duzlestirmeyi HIC uygulamiyor (C: OK, SELF: T002). D-520'nin kacis kapisi C'de VAR, self'te YOK. Once HANGI YONDE hizalanacagini olc (C'yi sikilastir mi, self'i gevset mi) — ikisi de DIL YUZEYI karari; kod yazmadan once secenekleri ve maliyetlerini belgeye yaz.

## Gunluk
- 2026-08-31 D-523: `Dizi<T>` iceren kullanici yapisi goreve yakalanirsa L002 (C + checker.kem + codegen.kem). Skaler alanli yapi MUAF. checker_diff 169/169, ct_bariyer 14/14, codegen_diff 162/162, drf_test 54/54.
- 2026-08-31 D-524: aritmetik tasma sabitlendi — yeni kapi `calistir_tasma` (12 olcum, C+SELF): -O0/-O2 ayni VE IR'da nsw/nuw yok. Sabotaj S99 (nsw enjekte) -> 3 dosya kirmizi, rc=2. Dil degisikligi YOK.
- 2026-08-31 D-525: D-510'un dali ULASILABILIR cikti (literal argüman/cesit payload/ic ice literal; korpusta 0 iz). Fikstur cg_bilinmeyen_eleman eklendi, 6/6 GLOBAL. bolge_operand 165/165, codegen_diff 163/163. Ilk enstrumantasyon hic uygulanmamisti — sahte 'ulasilamaz' sonucu yakalandi.
- 2026-08-31 D-526: K1 kapandi — `-> mantiksal` self-host'ta da i1 (yalniz donus konumu; ll_tip degismedi). Kok: tip_genislik'te i1 YOKTU -> sext i32->i1 = gecersiz IR. yapi_diff muafiyet 27->18, codegen_diff 163/163, llvm_test 286/286. Sabotaj S100 -> 154/163.
- 2026-08-31 D-527: kanal ABI'si bare-metal'de olculuyor (runtime/kem_kanal_abi.kem, ciplak+ARM64). Depoda kanal kullanan runtime satiri YOKTU. baremetal_diff 4/4 -> 5/5. S101 gecersizdi (kaynak degisikligi iki derleyiciyi birden etkiler); S102 (tek tarafi boz) -> rc=2.
- 2026-08-31 Altyapi: dongu artik dis kabuk betigiyle surulyor (`loop.sh`, en fazla 200 tur, tur basi 45 dk zaman asimi). Oturum-ici cron isleri (576a86e0 30dk, ea3657f5 5dk) SILINDI — ikisi ayni anda etkindi ve ayni LOOP.md maddesini es zamanli isleyebilirdi; bu, D-297'de kayitli "ayni testin iki es zamanli kosumu birbirini ezer" sinifinin ta kendisi. Betik seri kosar: bir tur bitmeden digeri baslamaz. Durma kosullari: sifir-disi cikis kodu, ciktida "DURDU:", ya da Sirada'nin bosalmasi. `loop.sh` ve `loop.log` .gitignore'da — birincisi yerel surucu, ikincisi kosum ciktisi; ikisi de depo icerigi degil.
- 2026-08-31 Durum: test/perf maddesine BASLANMADI (yarim is birakmamak icin bilerek). Sirada'da 5 madde duruyor. Son yesil olcumler: codegen_diff 163/163, yapi_diff 147/147 (18 muaf), checker_diff 169/169 (0 muaf), baremetal_diff 5/5, llvm_test 286/286, tasma 12/12, ct_bariyer 14/14.
- 2026-09-01 D-528: test/perf tabani kapiya cevrildi — yeni hedef `calistir_perf_bellek` (bench1/bench2, zirve RSS, C+SELF, 4 olcum, esik 4096 KB).
  NEDEN BU SEKILDE: D-506'nin 17x kazanci bir YONLENDIRME kararina bagli ve davranissal kapilar bu sinifa KOR (D-417): yonlendirme bozulsa program yine exit 42 verir, codegen_diff yesil kalir, ASan susar. bolge_operand IR'daki rho SINIFINI olcer; bu kapi GERCEK TUKETIMI olcer.
  ESIK OLCUMLE SECILDI: bugun iki derleyicide de 1152 KB (D-506 ile birebir); rho_caller'a donus 19968 KB. 4096 KB ~3.5x baslik birakir, regresyonu kesin yakalar. Dar esik ortam gurultusunden araliklli kirmizi verirdi. Cikis kodu da (42) denetlenir: hic calismayan program da dusuk RSS verir.
  HANGI KAPI NEYI OLCTU: perf_bellek 4/4 . codegen_diff 163/163 . yapi_diff 147/147 (18 muaf). Sabotaj S103 (D-506 yonlendirmesini rho_ref'e dondur) -> "C bench2: zirve RSS 19968 KB > esik 4096 KB", rc=2.
  YANLIS GIDEN DENEMELER (ikisi de kayitli derslerin tekrari): (1) perf_bellek hedefi $(BUILD)/codegen'e BAGIMLI oldugu icin make onu S103 ETKINKEN yeniden kurdu; sabotaji geri alirken yalniz kemgu'yu kurmustum -> sabote edilen derleyiciyle URETILEN her artefakt da kirlenir. (2) WSL agacindaki selfhost/codegen.kem hala S100 tasiyordu (D-526'nin sabotaji Windows'ta geri alinmis, WSL'e kopyalanmamis); belirti "sext i32 1 to i1" ve K1'in 9 dosyasi LINK-RED idi, bir an D-526'yi bozdum sandim — git'teki kaynak TEMIZDI. Teshis sirasi: git temiz mi -> iki agac senkron mu -> ikili o kaynaktan mi kurulmus. (3) Kendi `rm -f build/*.o` komutum kdl_runtime.o'yu da sildi -> codegen LINK-RED; sessiz `&&` zinciri yerine her adimin rc'sini basinca gorundu.
  ORTAM NOTU: /usr/bin/time yoksa kapi BILDIREREK atlar (D-453'un QEMU deseni); D-486'nin yasakladigi sey SESSIZ atlamadir.
- 2026-09-01 D-529: Lean ispatlari ILK KEZ gercekten derleniyor. Yeni opt-in hedef `calistir_lean_tam` (test_tumu'ya BAGLANMADI).
  OLCUM: lean/lake KURULU ama WINDOWS'ta (~/.elan/bin), WSL'de degil. `lake build` mathlib4'u klonlamaya calisip "git exited with code 128" ile 11 DAKIKA sonra dusuyordu.
  KOK: 32 .lean dosyasinin HICBIRI Mathlib'i import etmiyor (tum importlar ic: Kemgu.*) -> `require mathlib` BILDIRIM ARTIGIYDI. Kaldirilinca proje CEVRIMDISI 45 saniyede, 33/33 is, sifir hata ile derlendi. Yani ispatlar bugune kadar hic tip-denetlenmemis; engel bir ispat sorunu degil kullanilmayan bir bagimlilikti.
  NEDEN test_tumu'ya BAGLANMADI: takim WSL'de kosuyor, lake Windows'ta. Opt-in hedef; lake yoksa BILDIREREK atlar (D-453 QEMU deseni, D-486'nin yasakladigi SESSIZ atlama degil).
  POZITIF KANIT: artimli derlemede lake hicbir sey basmaz -> "0 is" onbellekten mi gectigini soylemez. Kapi ayrica .olean SAYAR (31); sifirsa "bosa kostu" diye kirmizi olur.
  HANGI KAPI NEYI OLCTU: lean_tam OK (1 is, 31 .olean) . lean_sorry 32 dosya 0 sorry (bayat "lake build KOSULMADI" uyarisi guncellendi). Sabotaj S104 (theorem s104_bozuk : 1 = 2 := rfl) -> "error: 1 = 2", Kemgu.Drf.Drf basarisiz, rc=2.
  YANLIS GIDEN DENEMELER: (1) `set -u` altinda $USER Git Bash'te TANIMSIZ -> harness kendi hatasiyla dustu; ${USER:-${USERNAME:-}} yapildi. (2) Atlama dalini PATH'i bosaltarak sinadim ama harness KENDISI ~/.elan/bin'i PATH'e ekliyor -> atlama hic tetiklenmedi, probe gecersizdi; HOME de gizlenince dogru olculdu. (3) Ilk kosumda `mingw32-make` PATH'te yoktu (CLAUDE.md'de kayitli clang64/ucrt64 oneki eksikti).
  DURUSTCE: lake-manifest.json hala mathlib girdisini tasiyor; `lake update` aga cikacagi icin DOKUNULMADI. Iddia "derleniyor", "manifest tutarli" DEGIL.
- 2026-09-01 D-530: ARM64 fiziksel donanim kontrol listesi yazildi (belgeler/ARM64_Fiziksel_Donanim_Kontrol_Listesi.md). Kod degisikligi YOK (bir yorum guncellemesi disinda).
  NEDEN: D-490 "gercek dogrulama yalniz fiziksel ARM64'te yapilabilir" demis ama NASIL yapilacagi hicbir yerde yazili degildi; kaynak dosyada uyari vardi, calistirilabilir adim yoktu. Borc ertelenmis degil UNUTULMAYA ACIK haldeydi.
  ICERIK: on kosullar (>=2 cekirdek, clang aarch64, ld.lld) . taban kosumu (SMP QUEUE OK, toplam=20540) . sabotaj (onbellek bariyerlerini nop yap) . sonucun nasil OKUNACAGI . kaydin nasil guncellenecegi . ayni turda kosulacak diger hedefler (qemu_cekirdek, baremetal_diff, arm64_test).
  EN ONEMLI MADDE: YESIL SONUC "BARIYER GEREKSIZ" DEMEK DEGILDIR — tam olarak QEMU'da yasanan budur. Yesilse prosedur testi GUCLENDIRMEYI soyler; araliklli bir kirmizi bile kesin kanittir (zayif bellek hatalari belirlenimci degil).
  YANLIS GIDEN DENEMELER: (1) Belgedeki iki iddia depoya karsi dogrulaninca YANLIS cikti: `grep -c "dc civac"` 9 doner (yorumlari da sayar), kodda 2 var; dogrusu grep -cE '"dc (civac|ivac)'. (2) `dsb sy` 4 kez geciyor ve IKISI SPINLOCK yolunda — sabotaj yalniz onbellek bakimindakileri hedeflemeli; bu ayrim ne belgede ne kaynakta yaziliydi. (3) OZ-GONDERGESEL TUZAK: sayim desenini kaynaga yorum olarak ekleyince DESEN KENDINI SAYDI (2->3, 4->5); talimat ogrettigi olcumu bozuyordu. Desenler kaynaktan cikarildi, kaynak kontrol listesine ISARET ediyor.
  DOGRULAMA: kod satiri sayimlari geri dondu (dc 2, dsb 4), dosya aarch64 hedefiyle derleniyor (rc=0).
- 2026-09-01 D-531: D-520'yi fiksturle kilitleme denemesi. FIKSTUR GERI CEKILDI, iki yeni gercek olculdu. Kod degisikligi YOK.
  BULGU 1 — LEGACY MODUL COZUMU CWD-GORELIDIR, ice aktarana gore DEGIL: ayni dosya test/moduller dizininden OK, depo kokunden T040. Yeni-bicim yukleyici ice aktaranin dizinini arar (D-427); legacy CWD'ye bakar. drivers/virtio de kok-goreli yaziyor. Bu ayrim hicbir yerde YAZILI DEGILDI.
  BULGU 2 — SELF-HOST LEGACY DUZLESTIRMEYI HIC UYGULAMIYOR: C "OK" der, self "T002 26:9, T002 26:25". Yani D-520'nin kacis kapisi C'de VAR, self-host'ta YOK. test/moduller'de ciplak cok-segmentli tek bir import bile yoktu, bu yuzden checker_diff bu ayrismayi hic gormemis.
  NEDEN FIKSTUR GERI CEKILDI: checker_diff'in muafiyet listesi BILEREK BOS ("modul yuzeyi D-361/362/363'te tamamen kapandi"). Fikstur oraya konsa ya kapi kalici kirmizi olurdu ya da o listeye ILK muafiyet girerdi — ikisi de belgelenmis bir kazanimi bozar. Madde ayrica "gocu BASLATMA" diyordu; yeni bir parite cephesi acmak o siniri asar. Bulgu kaybolmadi: Sirada'ya is olarak girdi.
  SONUC D-520'YI DEGISTIRIYOR: "gercek kodun cogunda gizlilik kapali" demistim; dogrusu ORACLE'DA kapali, self-host'ta zaten kapali degil.
  DOGRULAMA: fikstur cikarildiktan sonra checker_diff 169/169 (0 muaf), modul_codegen 22/22, calisma agacinda kalinti yok.
- 2026-09-01 D-532: Kanal omru arastirma notu yazildi (belgeler/Kanal_Omru_Arastirma_Notu.md). KOD DEGISIKLIGI YOK — madde zaten "kod yazma" diyordu.
  OLCULEN DURUM: kdl_kanal_serbest runtime'da VAR ve DOGRU; cagiran src/llvm.c 0, selfhost/codegen.kem 0. D-462'nin "kod var, hicbir olcum atesliyor degil" sinifi.
  GERCEK SORU UC PARCAYA AYRILDI: (1) yaratan islev donuyor [trivial], (2) kanali yakalayan HER gorev birlestirilmis mi [EKSIK OLAN], (3) kanal cerceveden kacmamis [ky_confined zaten yapiyor, 18-UAF avindan gecmis].
  ONEMLI BAGLANTI: (2) akis-duyarli bir sorudur ve depoda BENZERI ZATEN VAR — lineer tuketim takibi (L001/L002/L005, D-311/D-312'de dal-duyarli). `gorev<T>` ZATEN lineerdir. Yani yeni analiz gerekmiyor; eksik olan kanal -> onu yakalayan gorevler ESLEMESI (hicbir yan-kanal tutmuyor).
  UC SECENEK KARSILASTIRILDI: A (yakalayan gorevlerin hepsi L-tuketilmisse ret oncesi serbest) tek gercekci yol . B (kanali lineer yap) DIL SEMANTIGINI BOZAR — D-505'te kanal paylasim icin taşımadan bilerek muaf tutulmustu . C (biraksin sizsin) bugunku bilincli hal.
  KARAR: C korunuyor. kdl_kanal_serbest BILEREK olu kaliyor, SILINMEMELI — D-459'un "olu kodu birakma" kuralinin TERSI durum: orada olu kod bir tuzakti (sessiz-basarisiz yol acabilirdi), burada bir yer tutucu (hic cagrilmiyor).
  YANLIS GIDEN DENEMELER: (1) Nota D-515'in "13 dosyanin 5'i" sayisini kopyaladim; bugun olcunce 16/8 cikti (korpus buyudu) -> "belgeye gomulu sayi yazildigi gun dogrudur" uyarisiyla duzeltildi. (2) A'nin 1. on kosulunu grep ile olcmeye calistim; grep YORUMLARI da sayiyor (kanal_mesaj 2/1, codegen.kem 28/13 gorunuyor ama basliklar bu adlari boluca aniyor) -> sayilar GUVENILIR DEGIL, gercek olcum AST uzerinden yapilmali. Bu, notun kendi kuralinin ihlaliydi ve nota kaydedildi.

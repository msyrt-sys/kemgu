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
- [~] KEMGU-OS ESZAMANLILIK KATMANI (kullanici istegi, 2026-09-27). OLCUM: gorev/kanal
      HOST'ta tam (D-291..D-543) ama D-592'den beri kem_os yalniz boot .S + SAF-.kem
      bagliyor -> C kdl_kanal.c/kdl_gorev.c LINKLENMIYOR; dilin kanal/gorev ilkelleri
      cekirdekte HIC CALISMIYORDU (D-527 ABI'yi yalniz DERLIYORDU).
      - [x] kanal SAF-.kem (kem_heap.kem) + faz [41], tek gorevli (D-623)
      - [x] gorev_başlat/birleştir SAF-.kem + faz [42] BLOKLAYAN kanal (D-624)
- [x] A1 -> KAPANDI. CI IKI PLATFORMDA TAM YESIL (ef3eb7d, 2026-10-03).
      ADIM DUZEYINDE dogrulandi ("yesil bir iddiadir", D-486): Windows
      `test_tumu` + onceden ATLANAN tum alt adimlar (Stdlib --check, Snapshot,
      Fuzzer, Runtime link) `success`. Yol: D-655 (yutulan stderr acildi) ->
      D-658 (kok: uretilen konak modulu mojibake oluyordu).
- [~] C0a [M] Kalan `bekle` sitelerini C kodlariyla esle. ESLENEN: 22 kod
      (D-643'te 3, D-647'de 4, D-648 P017, D-649 P261, D-650 P015/P016/P012/P013/
      P081/P082/P010/P101, D-651 P001, D-652 P350/P150/P122/P060/P040).
      Eslenmemis `bekle` cagrisi: 91 -> 85 -> 79. ESLENEN: 28 kod.
      YONTEM (D-647'de kuruldu, her turda ise yariyor): hata SEKLINI yaz, C'ye
      sor, KONUM uyusuyorsa siteyi esle; uyusmuyorsa davranis farki demektir.
      ⚠ D-647'DE OLCULDU: token -> kod eslemesi MEKANIK DEGIL. Tek basina
      `TANIMLAYICI` beklentisi baglama gore P200/P350/P269/P240/P080/P060/P045...
      oluyor; C'de 76 farkli kod var. Toplu cevirme "makul ama dogrulanmamis" kod
      uretir -> her site FIKSTURLE kanitlanmali. Yontem: once hata SEKLINI yaz,
      C'ye sor, konum uyusuyorsa site esle.
- [x] G14 -> D-660. `checker_diff` artik per-dosya `timeout` + `ulimit -v`
      tasiyor ve asilan dosyayi ADIYLA kirmizi yapiyor (S160 ile kanitlandi).
      Yol ustunde KENDI onarimimda yanlis-yesil bulundu ve kapatildi (asagida).
- [x] G15 -> D-662 + D-663'te TAMAMLANDI. OLCULEREK kapatildi: self-host
      ikilisini korpus uzerinde kosturan HER kapi sarildi. baremetal_diff .
      surucu_diff . yapi_diff (D-663) + codegen_diff . selfhost_driver (D-662).
      `parser_diff`/`lexer_diff` DISARIDA: olculdu, yalniz C oracle'i (`$KEMGU`)
      kullaniyorlar, self ikilisine hic dokunmuyorlar -> asilma sinifina kapali.
- [x] C1a -> D-664'te OLCULDU, KAPANMIS. Annotasyonsuz CAGRI basaticisi dort
      sekilde parite: 2^33 donus (exe 42/42), zincir g(f()), metin, kesirli64.
      D-640 dar birakmisti; sonraki isler kapatmis, madde bayatmis.
- [x] C1b -> D-664'te OLCULDU, KAPANMIS. `değişken x = 20+22` ve `değişken b = a`
      ikisi de chk + exe paritede.
- [ ] C1c [M] ACIK — D-664'te KOK NETLESTI. `değişken y: tam64 = x + 8589934592`
      (x: tam32): C IKI tani (T043 literal + T001 deyim), self YALNIZ T001.
      Eksik olan literal TASMASI DEGIL (2^33 < 2^64, `tasti=0`): C'nin D-021
      YENIDEN-TIPLEME yolu `x`(tam32) ile toplami tam32 baglamina sokuyor,
      `8589934592` tam32'ye sigmiyor -> T043. Self'te T043 makinesi VAR ama
      yalniz 2^64+ doymasi icin (D-632); D-021 yeniden-tipleme dalinda T043
      kontrolu YOK. Onarim: `tip_belirle_beklenen` aritmetik dalinda, literal
      hedef tam genisligine sigmiyorsa T043 (C `tip_kontrol.c` ~2963 aynasi).
      ⚠ DAVRANIS degil TANI farki: exe iki tarafta da 42 (deger dogru), eksik
      olan yalniz tanilama -> dikkatli, cunku sahte-pozitif riski yuksek
      (D-632'nin kendi yorumu: baglamsiz yolda raporlamak sahte T043 uretir).
- [ ] C3a [S] C3.2 karar: asm cikti hedefi AST'de AD yerine IFADE dugumu.
- [ ] C3b [M] parser.c — `&` sonrasi SINIRLI lvalue (tanimlayici | tanimlayici.alan).
- [ ] C3c [M] tip_kontrol.c:6575 — AS002 lvalue tipi uzerinden (primitif olmali).
- [ ] C3d [M] llvm.c:7496 — TANIMLAYICI'da `isim_bul`, ERISIM'de `erisim_lvalue`.
- [ ] C3e [M] self-host codegen.kem aynasi.
- [ ] C3f [S] cg_korpus fiksturu (gercek kosum: alana yaz -> 42) + sabotaj.

# ===== D. DIL OZELLIKLERI =====
- [ ] D1a [M] turbofish: parser (`::<>` ayrimi + belirsizlik kurali).
- [ ] D1b [M] turbofish: checker — acik argumanin cikarsamayi EZMESI.
- [ ] D1c [M] turbofish: codegen mono yolu.
- [ ] D1d [M] turbofish: self-host parite (iki dosya).
- [ ] D1e [S] turbofish: korpus fikstur + sabotaj.
- [ ] D2a [M] Kabiliyet odunc alma: omur kurali (kime, ne kadar).
- [ ] D2b [M] Kabiliyet odunc: checker zorlamasi (yeni hata kodu).
- [ ] D2c [M] Kabiliyet odunc: codegen.
- [ ] D2d [M] Kabiliyet odunc: self-host parite.
- [ ] D2e [S] Kabiliyet odunc: `yetki<R>` korpus fiksturu.
- [x] D3a -> D-669. Bariyer "herkes gelmeden kimse gecmiyor" + YENIDEN KULLANIM
      (5 tur) eszamanlilik kapisinda. Cok-tur YUK TASIYOR (olculdu, asagida).
- [x] D3b -> D-667'de yapildi (README semafor/bariyer maddesi duzeltildi).
- [x] D3c -> D-668. Semafor n>1: "en fazla n" (guvenlik) VE "n kadar gercekten"
      (anlam) AYRI AYRI olculuyor; eszamanlilik kapisinda.
- [ ] D4a [M] LSP v3: artimli didChange.
- [ ] D4b [M] LSP v3: workspace/symbol.
- [ ] D4c [M] LSP v3: semanticTokens.
- [ ] D4d [M] LSP v3: references/rename.
- [ ] D4e [S] LSP v3: her biri icin JSON-RPC kapi testi.
- [ ] D5a [S] `metin`/`dosya` EKSIK ISLEV ENVANTERI cikar (metin 36, dosya 17 islev
      VAR — "tamamlama" olculmeden is degil; D-639).
- [ ] D5b [M] BLAKE3 (`kripto/karma.kem`, bugun 3 islev).
- [ ] D5c [M] HMAC (`kripto/karma.kem`).
- [ ] D5d [M] OS RNG (`kripto/rastgele.kem`, bugun 4 islev; OS entropi kaynagi YOK).
- [ ] D5e [S] Yeni islevleri `calistir_kripto_kosum` kapsamina al.
- [ ] D6a [M] Gorev govdesi icin escape ozeti (D9a'ya bagli).
- [ ] D6b [M] Hapsedilme kurali + yeni tani kodu.
- [ ] D6c [M] Kanit varsa bolgeyi serbest birak.
- [ ] D6d [M] ASan altinda UAF kapisi.
- [ ] D6e [S] Sabotaj: hapsedilme denetimini gevset -> ASan kirmizi OLMALI.
- [ ] D7a [M] Ayrik derleme: arayuz dosyalari (imza-only cikti).
- [ ] D7b [M] Ayrik derleme: glob import.
- [ ] D7c [M] Ayrik derleme: opak tipler.
- [ ] D7d [M] Ayrik derleme: re-export.
- [ ] D7e [L] Artimli yeniden derleme (degisen birim + bagimlilari).
- [ ] D7f [S] Her biri icin cross-file korpus fiksturu.
- [ ] D8a [M] `özellik` bildirimi sozdizimi.
- [ ] D8b [M] `uygula` blogu + method cozumu.
- [ ] D8c [M] Generic'te bound (`T: Ozellik`).
- [ ] D8d [M] STATIK dispatch.
- [ ] D8e [L] DINAMIK dispatch (vtable) — ayri bir KARAR, once statik.
- [ ] D8f [M] Trait: self-host parite.
- [ ] D8g [S] Trait: korpus + sabotaj.
- [ ] D9a [M] Islev basina escape ozeti (cagri grafi).
- [ ] D9b [M] Ozetin cagri yerinde kullanilmasi (`escape.c:308` konservatif dal).
- [ ] D9c [M] Ozyineleme/dongu icin sabit-nokta.
- [ ] D9d [M] Bolge/escape'i surucu hattina baglama.
- [ ] D9e [L] Otomatik-serbestleyen arena.
- [ ] D10a [S] checker.kem (6801 satir) ile codegen.kem gomulu checker'inin
      FARKINI olc (diff tabanli envanter).
- [ ] D10b [S] Yon karari: driver tek kaynak mi, checker.kem mi?
- [ ] D10c [L] Goc.
- [ ] D10d [M] `checker_diff` kapisini yeniden tanimla (tek kaynak olunca neyi
      olcecek? — SILME, anlamini degistir).
- [ ] D10e [S] Sabotaj.

# ===== E. KEMGU-OS =====
- [ ] E1a [S] Spark'in UART taban adresini DT/ACPI'den OKU (bugun bilinmiyor).
- [ ] E1b [M] Imaji o adres + yukleme tabaniyla kur.
- [ ] E1c [-] Seri yakalama duzenegi — IKINCI MAKINE (Mehmet'in donanim isi).
- [ ] E1d [M] Fiziksel taban kosum (SMP QUEUE OK, toplam=20540).
- [ ] E1e [M] Bariyer sabotaji + sonucu D-490'a yaz.
- [ ] E2a [M] Ikincil cekirdek bring-up (PSCI CPU_ON deseni smp_queue_arm.c'de VAR).
- [ ] E2b [M] Cekirdek-basi zamanlayici.
- [ ] E2c [M] Kilit/bariyer cekirdekte.
- [ ] E2d [M] `-smp 2` ile kem_os kapisi.
- [ ] E2e [S] SMP sabotaji.
- [ ] E3a [-] KARAR: "v1 userland neye denir?" (Mehmet) — bitis tanimi YOK.
- [ ] E3b [L] Surec modeli (birden cok EL0 sureci).
- [ ] E3c [L] Surec olusturma (fork/exec analogu).
- [ ] E3d [M] Dosya tanimlayici tablosu.
- [ ] E3e [L] Basit kabuk.
- [ ] E3f [M] Surecler arasi izolasyon kapisi.

# ===== F. FORMEL =====
- [ ] F1a [L] DRF V2: per-thread bolgeler (mevcut L0/L1 lemmalarina EN YAKIN olan;
      once bu onerilir).
- [ ] F1b [L] DRF V2: tam-dil kapsami.
- [ ] F1c [L] DRF V2: operasyonel/runtime tanik.
- [ ] F1d [L] DRF V2: weak-memory (C++11) fence emisyonu. ⚠ E1'e BAGLI: fiziksel
      olcum olmadan ispat "modele gore dogru" kalir.
- [ ] F1e [L] DRF V2: yan-kanal bileseni teoreme.
- [ ] F1f [L] DRF V2: WCET bileseni teoreme.

# ===== G. KAPI HIJYENI (bu oturumda OLCULEN bulgular) =====
- [ ] G1  [M] KAPI-YUZEY HARITASI. Bu oturumda UC kez ayni ders cikti: kapi yesil
      olmasi yuzeyin olculdugu anlamina gelmiyor. (C1: check_genis check_korpus'u
      taramiyor. C2: uc parite kapisi codegen_genis'in gordugu gerilemeyi gormedi.
      C0: check_korpus'ta parse-hatali dosya olmadigi icin butun bir hata sinifi
      kapsam disiydi.) Hangi kapinin hangi korpusu/yuzeyi gordugu YAZILI DEGIL.
- [ ] G2  [S] KAPI KAYIT DENETIMI. D-645'te kapiyi `test_tumu`ya ekledigimi
      sandim; `.PHONY` satiri ayni hedef dizisini icerdigi icin yama ORAYA gitti
      ve kapi KOSMADI. `rc=0` idi. Yeni kapi eklendiginde ozet satirinin koşumda
      GORUNDUGU denetlenmeli (D-446'nin onlemi).
- [ ] G3  [S] `test_tumu` hedef listesi ile `.PHONY` listesi AYNI diziyi iki yerde
      tutuyor -> D-407 ayrisma yuzeyi (G2'nin kok nedeni).
- [ ] G4  [S] `selfhost_driver_harness.sh:29` `kemgu_self2.exe` adini SABIT yaziyor
      (Linux'ta uzantisiz olmali; bugun calisiyor cunku adi harness kendi koyuyor,
      ama D-469 sinifinda bir artik).
- [ ] G13 [S] `SIZINTI-MUAF` METRIGI DALGALANIYOR. D-648'de olculdu: dort
      gozlemin birinde 2 yerine 1 cikti (kanal_mesaj/gorev_temel sizintisi her
      kosumda MANIFEST OLMUYOR). Bu, bu oturumda kullandigim birincil dogrulama
      yontemini (ozet satirlarini onceki yesille diff'lemek) yanlis-pozitife acik
      kiliyor. Metrik ya kararli hale getirilmeli ya da ozette dalgalandigi
      ISARETLENMELI. (Degisikligimle ilgisi OLMADIGI ayrica kanitlandi: ASan
      kapisi yalniz `$KEMGU`yu kullaniyor, self-host ikilisine hic dokunmuyor.)
- [ ] G6  [S] `P000` yer tutucu envanteri (iki dosyada 3'er yer) — C0a ilerledikce
      sifira inmeli; kalanlar listelenmeli.
- [ ] G7  [S] `tumu*.log` .gitignore'a (loop.log ile ayni sinif: kosum ciktisi).
- [ ] G8  [?] stdlib modulleri `genel` DEGIL -> secili import ile tuketilemiyorlar;
      bu yuzden testleri `cat` ile birlestiriliyor. Dil yuzeyi karari gerekebilir.
- [ ] G9  [?] stdlib/kilit,semafor,bariyer'in URETIM tuketicisi YOK (D-645 test
      ekledi ama depoda kullanan kod yok). Olu API mi, yoksa kullanilmasi gereken
      mi? Karar gerekiyor.
- [ ] G11 [S] `aout=` ve `out=1` — IZLENEN 0 BAYTLIK dosyalar, kok dizinde.
      Kabuk yonlendirme kazasi (`-o aout=` / `out=1` yazim hatasi) commit'lenmis;
      Makefile/harness'larda REFERANS YOK (olculdu). Silinmeli.
      (.gitignore D-631'de `NUL`/`nul` icin ayni sinifi zaten kapatmisti.)
- [ ] G12 [?] `dz.kem` (101 bayt) ve `probe_haz.kem` (184 bayt) kok dizinde izleniyor
      ama Makefile/harness'larda referansi YOK. Gecici probe artigi mi, yoksa
      belgelenmemis bir fikstur mu? ONCE OLC, sonra sil ya da yerine tasi.
- [ ] G10 [S] `runtime/kdl_runtime.c` gecerli UTF-8 DEGIL (CLAUDE.md'de kayitli
      tuzak; D-633'te 2328 satir kaybina yol acti). Kodlama borcu kapatilmali.

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
- 2026-09-01 D-533: D-531'in BULGU 2'si YANLISTI — eksik PORT cikti, dil karari DEGIL.
  OLCUM: C -> OK . codegen.kem -> OK (D-427'de almis, IR de dogru) . checker.kem -> T002 (legacy dali ve kullan_yeni_bicim_mi YOK). Okudugum kod bir uygulamadan, test ettigim ikili baskasindandi — uc-uygulama tuzaginin bu oturumdaki DORDUNCU tekrari ve bu kez beni "karar gerekiyor" diye YANLIS BIR SINIFA soktu.
  NE YAPILDI: kullan_yeni_bicim_mi + legacy dali checker.kem'e portlandi. Yeni karar YOK, yeni tani kodu YOK — kural zaten yazilmis ve calisiyordu.
  PORTTA SADELESTIRME (olculdu, varsayilmadi): codegen.kem'in yukleminde ALIAS (al_yol) dongusu var, checker.kem'de o dizi yok. Parser P046 "secili/alias import v1'de TEK modul adi gerektirir" diyor -> seg > 1 iken ikisi de IMKANSIZ, yani o dongu bu dalda ULASILAMAZ. si_yol yine de korundu (P046 ileride gevserse dogru kalsin).
  HANGI KAPI NEYI OLCTU: checker_diff 170/170 (0 muaf, fikstur sayildi) . modul_codegen 22/22 . check_genis 133/133. Sabotaj S105 (legacy bayragini yanlis sabitle) -> 169/170, rc=2.
  D-520 HALA ACIK VE DEGISMEDI: cok-segmentli kullan T041'i atliyor; kapatmak ~700 referansliK goc ister ve O bir dil yuzeyi karari. Bu artim yalniz IKI UYGULAMAYI HIZALADI — acigi kapatmadi, olculebilir kildi.
- 2026-09-01 TAM TAKIM (D-528 sonrasi ilk tam kosum): rc=0, 0 gercek basarisizlik, FIXPOINT OK (76.989 satir), 20 dk 19 sn, SIFIRDAN kurulum (build temizlendi).
  NEDEN SIFIRDAN: bu oturumda uc kez bayat artefakt yasandi (D-528'de perf_bellek hedefi codegen'i sabotaj etkinken kurdu; iki agac senkronu kacti). Temiz kurulum o siniftan gelen sahte yesil/kirmiziyi eler.
  rc=0 BIR IDDIADIR — AYRICA OLCULDU (D-486): test_tumu 71 hedef cagiriyor . "Basarisiz: [1-9]" ve "FAIL=[1-9]" satiri SIFIR . atlama izlerinin HEPSI adlandirilmis+sayilmis (cgmodul_mat/zincir oracle-yok . codegen_genis 9 . baremetal_diff 6 . surucu_diff 1 . bolge_operand 2 . ASan SKIP=31 . [24] RAM<=1GiB) — SESSIZ atlama YOK.
  YENI KAPILAR GERCEKTEN KOSTU: `aritmetik tasma` ve `perf zirve bellek` ozet satirlari logda VAR (eklendikleri halde cagrilmama sinifi D-446'da kayitli; ayrica dogrulandi).
  LOG kalici yola yazildi (/mnt/c/...): WSL /tmp cagrilar arasinda siliniyor ve onceki bir turda 12 atlama izi olculmeden kaybolmustu.
- 2026-09-01 D-534: check_kapisi'nin muafiyetleri tek tek olculdu — BIRI BAYAT cikti ve SILINDI.
  OLCUM: listede 8 girdi var ama kapi 7 muaf sayiyordu; fark ipucuydu. Sekizi tek tek --check'ten gecirildi: yedisi hala gercek, `cg_skaler_deref.kem` ise TEMIZ geciyor (gerekcesi "kasitli skaler deref cast, E002" artik gecerli degil). Listenin kendi kurali: "Muafiyet KALICI DEGIL: gerekce ortadan kalkarsa satir silinmeli."
  DIGER 7 DOGRULANDI: kem_os T002 (parca dosya) . kem_asm_kernel AS001+T001 . kem_kullanici AS001 + iddia edilen `--mimari arm64` yeniden denetimi harness'ta GERCEKTEN var (satir 83) . lineer_hata L001/L002/L004/LR002 (belgede yazili dordu birebir) . sifrele_dosya T002 . cg6_trunc E004 . cg_deref_pointer T001.
  KOR NOKTA KANITLANDI (ayirt edici deney): dosyaya kasitli tip hatasi enjekte edilip kapi IKI DURUMDA kosuldu -> (a) muafiyet silinmis: 1 RED, rc=2 (yakaliyor) . (b) muafiyet geri konmus: 8 muaf, 0 RED, rc=0 (sessizce yutuyor). Yani girdi dururken o dosyanin GERCEK bir kirilmasi gorunmez olurdu.
  NEDEN BU DENEY GEREKLIYDI: girdi olu oldugu icin silmek HICBIR SAYIYI DEGISTIRMEDI (267/274 -> 268/275, muaf 7 -> 7; artis fikstur eklemelerinden). Tek basina "kapi yesil" degisikligin degerini gostermiyordu; deger ancak "yanlisin gozlenebilir oldugu sekli" (D-425) kurulunca olculdu.
  KAPI: check_kapisi 268/275, 7 muaf, 0 RED, rc=0.
- 2026-09-01 D-535: K4 kokunu olcerken DAHA AGIR bir acik cikti — `Dizi<T>` parametresinden T CIKARSAMA YOKTU, SESSIZ YANLIS CEVAP.
  ONCE SINIR OLCULDU (madde bunu istiyordu): D-401'in "cikarsama yalniz ciplak T'den" siniri HALA GECERLI (D-441'de baska bir bilinen sinir bayat cikmisti, bu cikmadi). Uc probe: ciplak T -> `@kimlik$i64` OK . donus-tipi-guudumlu -> `@bos_yap$i32` YOK . `Dizi<T>` -> `@boyut$i32` YOK.
  ⚠ AYIRT EDICI TIP SART: ilk uc probe'umu `tam32` ile yazmistim ve ACIGI GOREMIYORDU — dogru cikarsama ile `i32` fallback'i tam32'de BIREBIR AYNI IR uretir. `tam64` + 2^33'e gecince acik gorundu.
  ACIK: `al_ilk<T>(xs: Dizi<T>, v: T) -> T` uzerinde `Dizi<tam64>` -> `call i32 @al_ilk$i32` -> deger KIRPILIR -> exit 1 (generic'siz ayni is: 42). Derleme temiz, link temiz, program KOSUYOR.
  D-411'IN SAGLAMLIK IDDIASI CURUDU: "fallback yanlissa annotasyonla uyusmaz -> LLVM REDDEDER, hata GURULTULU kalir" deniyordu. `Dizi<T>` konumunda annotasyon DA uyusur (dizi her genislikte `ptr` tasir) -> LLVM SUSAR. Iddia D-415'te bir kez daha yanlis cikmisti; bu ikinci kez ve bu kez SESSIZ. Yorum iki uygulamada da gercege gore duzeltildi.
  NE YAPILDI: C `src/llvm.c`'ye (c) dali (`TIP_DIZI` param + eleman `TIP_BASIT` == T -> argumanin `eleman_llvm_tip`i) . `selfhost/codegen.kem`'e `fn_param_dizi_idx` yuklemi + paralel `arge` kanali (`cg_var_elem_bul`). Eleman tipi `argt`ten OKUNAMAZ (orada yalniz "ptr" var) — ayri kanal sart.
  ⚠⚠ EN ONEMLI YANLIS DENEME — DALI EKLEMEK KAZANACAGI ANLAMINA GELMEZ: self-host portu ilk denemede HICBIR SEYI DEGISTIRMEDI (`@al_ilk$i32` aynen kaldi). C parametreleri SIRAYLA gezip ILK cikarsamayi alir; bu sekilde ciplak `v: T` index-1'de ve argumani literal `0` (i32). Self-host'un `fn_param_idx`i sira gozetmeden tarayip onu buluyordu, yeni dal `bulundu == yanlis` korumasi yuzunden hic atesLENMEDI. Onarim dal eklemek degil INDEKS ONCELIGI kurmakti.
  HANGI KAPI NEYI OLCTU: codegen_diff 164/164 (DAVRANIS: exit 42 == 42) . stdlib_check rc=0 (Dizi<T> stdlib'in tamaminin dayandigi sekil) . yapi_diff K4 muafiyetine eklendi — self BASE govdesini de yayiyor, bu D-401 V1'in KASITLI davranisi ve K4'un tanimi; muafiyet yeni bir sapma DEGIL, mevcut sinifin 11. ornegi.
  ⚠⚠ IKINCI BUYUK YANLIS DENEME — FIKSTUR YANLIS SEBEPLE YESILDI. Ilk fikstur `al_ilk<T>(xs: Dizi<T>, v: T)` idi. C'de (c) dalini kapatinca YINE 42 verdi: `degisken g: tam64 = al_ilk(a, 0)` beklenen tipi literal `0`'a yayiyor, ciplak `v: T` dali i64 uretiyor. Yani fikstur C tarafinda HICBIR SEY olcmuyordu. Ayirt edici sekil `ilk_tek<T>(xs: Dizi<T>) -> T` — T'yi tasiyan TEK parametre dizi olmali. Ikisi de fiksturde: biri C'yi ayirt eder, digeri self-host'ta indeks onceligini kilitler.
  ⚠⚠ DORT GECERSIZ SABOTAJ ust uste (D-402/D-500'un tekrari): (1) `make build/kemgu` hicbir seyi yeniden kurmadi, olcum BAYAT ikiliden geldi (D-457). (2,3,4) Sabotaj desenim TEK SATIRLIYDI, kod COK SATIRLI (`if (vi && ...) {` + ayri satirda atama) -> `python -c` sessizce eslesmedi ve ben "C'de baska bir yol da cikarsiyor" diye YANLIS BIR HIPOTEZ kurdum. Ustelik `/tmp` wsl.exe cagrilari ARASINDA siliniyor -> bir turda olcum dosyasi yoktu ve "exit=1" cikti, bunu bir an gercek sonuc sandim. Cozum: tek wsl cagrisi + build/ altinda calisma dizini + desen sayisini BASTIR + grep ile dogrula.
  SABOTAJ 2/2 (gecerli olanlar): S107 (self-host indeks onceligini kaldir) -> codegen_diff 163/164, `@al_ilk$i32` . S108e/f (C'de (c) dalini kapat) -> `@al_ilk$i64` -> `$i32`, fikstur exit 42 -> 1. Parite kapisi TEK BASINA yetmezdi: iki taraf birden yanlis olsa yesil kalirdi, bu yuzden her iki taraf AYRI AYRI sabote edildi.
  KALAN (degismedi): donus-tipi-guudumlu cikarsama HALA YOK — K4 muafiyetinin asil kokudur ve ayri bir istir.
- 2026-09-02 D-536 ON OLCUM: donus-tipi-guudumlu cikarsamanin TAVANI olculdu; NEGATIF sonuc BEKLIYORDUM, GERCEK BIR ACIK CIKTI. Kod degisikligi YOK (C yamasi yazildi, olculdu, GERI ALINDI).
  YONTEM: grep DEGIL (madde bunu yasakliyordu) — derleyici gecici olarak enstrumante edildi: cikarsama fallback'e dustugunde `(islev, tip_parami)` stderr'e basildi, sonra 655 .kem dosyasi derlendi.
  TAVAN: 14 olay / 5 benzersiz bolge / 655 dosya. Dagilim dosyasina kadar cikarildi: stdlib/sonuc.kem (k_hata T, k_tamam E) . cg_generic_sonuc_ptr + d1_generic_sonuc_ptr (hata_yap T) . dort test/moduller dosyasi (dizi.oluştur T) . 24_nested_generic (al T). selfhost/, drivers/, runtime/, kütüphane/ TUKETICILERINDE SIFIR.
  UCU ZARARSIZ CIKTI, VARSAYILMADI OLCULDU: (1) `dizi.oluştur<T>(böl) -> Liste<T>` YALNIZ 0 BAYT tahsis eder ve `Liste<T>`nin tum alanlari T'den bagimsizdir (`*T`=ptr + iki tam64) -> yerlesim ayni; asagi akistaki `ekle`/`al`/`boy` zaten `$i64` aliyor (IR'dan dogrulandi). (2,3) `hata_yap`/`k_hata`/`k_tamam` D-411'in KENDI belgeledigi sinif: fallback annotasyonla UYUSUYOR.
  🔴 DORDUNCUSU GERCEK ACIK: `yapı Kutu<T> { icerik: T; }` + `işlev al<T>(k: Kutu<T>) -> T` -> `@al$i32` (oysa `@olustur$i64` DOGRU). `Kutu<tam64>` + 2^33 -> exit 1, C ve SELF ikisinde de. Derleme temiz, link temiz, program kosuyor = D-535'in AYNI sinifi, bu kez DEGER konumundaki generic yapi parametresinde. Mevcut (b) dali YALNIZ `&Kullanici<T>`yi kapsiyor.
  NEDEN ONARILMADI (bilincli, D-531 deseni): C yarisi tek dal ve YAZILDI, kuruldu, `@al$i64` + exit 42 verdi. Ama self-host `olustur`u HIC specialize etmiyor (`define %Kutu$i32 @olustur`, `$i64` surumu YOK) -> `k` degiskeninin tipi bastan `%Kutu$i32`; suffix'ten T okumak da i32 verirdi. Yani self-host yarisi tam da olcmekte oldugum donus-tipi-guudumlu cikarsamayi ISTIYOR. C'yi TEK BASINA birakmak pariteyi bozar (fikstur eklenirse codegen_diff kalici kirmizi; eklenmezse kod GATE'SIZ kalir = D-430'un yasakladigi dogrulanmamis yuzey). Yama geri alindi, bulgu Sirada'ya IS olarak girdi.
  YANLIS GIDEN DENEMELER: (1) Ilk enstrumantasyon derlenmedi (`
` kabuk/python katmanlarinda bozuldu) ama `make rc=2`yi gormeden once "0 fallback" cikti ve bir an TAVAN SIFIR sanildi — sessiz sifir once ARACI supheli kilar (D-500, bu oturumda ucuncu kez). Cozum: kacissiz `fputc(10, stderr)` + `make rc` bastirma + ikili varligini dogrulama. (2) Ilk `oluştur` probe'um `yetki_olustur`a 3 argüman verdi (CP004). (3) Ikinci probe test/moduller dizininden kosuldu -> T040 (legacy modul cozumu CWD-goreli, D-531). (4) Ucuncu probe `dizi_kullan.kem`i 2^33 ile yeniden kosturdu ama dosyanin sonu `s olarak tam32` — ACIK bir daraltma sonucu maskeliyordu; ayirt edici probe ancak dorduncude kuruldu.
- 2026-09-02 D-536: DEGER konumundaki generic yapi parametresi (`al<T>(k: Kutu<T>) -> T`) T'yi cikarsamiyordu -> `@al$i32`, `Kutu<tam64>` + 2^33 -> exit 1, HER IKI derleyicide. Onarildi (C + self-host), fikstur + iki tarafli sabotaj.
  NEDEN BOYLE: mevcut dal yalniz `&Kullanici<T>` (referans) konumunu taniyordu. C'de kaynak `generic_arg_ir` yan-kanali; self-host'ta `argt` o konumda `%Kutu$i64` tasir — yapi IR'i type-erased ama MANGLE EDILMIS AD T'yi korur, bu yuzden yeni bir yan-kanal GEREKMEDI (`mono_ir_sonek` sonektten okur). Kanal yoksa dokunulmaz = default-DENY. Indeks onceligi D-535 ile ayni gerekcede.
  ⚠⚠ ONCEKI TURDA "SELF SPECIALIZE ETMIYOR" DIYE KAYDETMISTIM VE YANLISTI: `grep ... | head -12` ciktiyi KIRPMISTI; self `@olustur$i64`u zaten yayiyor, ben yalniz taban govdeyi (K4) gormustum. O yanlis okuma onarimi bir tur "donus-tipi-guudumlu cikarsama gerekiyor" diye ERTELETTI. Kirpilmis cikti, yanlis bir MIMARI sonuca goturur — bu oturumda `head`/`tail` kaynakli ucuncu hata.
  🔴 YOL USTUNDE AYRI KUSUR: self-host'ta `8589934592 olarak tam64` -> `sext i32 8589934592 to i64` (literal i32'de materyalize, aralik disi) = sessiz yanlis deger; C temiz. D-299 duz literal yolunu onarmis, CAST yolu acik kalmis. D-537 olarak Sirada'ya girdi. Fikstur bu sekli BILEREK kullanmaz — yoksa D-536 kapisi YANLIS SEBEPLE kirmizi olurdu (D-421).
  HANGI KAPI NEYI OLCTU: codegen_diff 165/165 (davranis) . yapi_diff 147/147, 20 muaf (fikstur K4'un 12. ornegi — self taban govdeyi de yayar, D-401 V1'in kasitli davranisi) . snapshot 50/50 . stdlib_check rc=0.
  SABOTAJ 2/2: S109 (C dalini kapat) -> exit 1 . S110 (self dalini kapat) -> exit 1. Parite kapisi tek basina yetmezdi (iki taraf birden yanlis olsa yesil kalirdi), bu yuzden her iki taraf AYRI sabote edildi.
- 2026-09-02 D-537: self-host'ta `<buyuk literal> olarak tam64` -> `sext i32 8589934592 to i64` = SESSIZ YANLIS DEGER (exit 1; C ayni programda 42). Onarildi, fikstur + sabotaj.
  KOK: cast operandi BAGLAMSIZ degerlendiriliyor -> tamsayi literali i32'de materyalize oluyor, sonra genisletiliyor; immediate'in KENDISI aralik disi. C referansi olculdu (icat edilmedi): `add i64 0, 8589934592` — literal HEDEF genislikte dogar. D-299 duz literal yolunu onarmis, CAST yolu acik kalmis.
  NEDEN DAR: yalniz GENISLETME. Daraltma (`300 olarak tam8`) `trunc` ile kalir cunku orada kirpma KASITLIDIR ve C de kirpar. Fikstur bunu POZITIF olcer (b != 44 -> ver 2); olmasaydi "her cast'i hedef genislikte dogur" sabotaji kapidan GECERDI (D-425). Ucuncu satir (`(0-1) olarak tam64`) isaretli genisletmenin bozulmadigini kilitler.
  C REFERANSI DORT SEKILDE OKUNDU: buyuk-genisletme / daraltma / isaretli / kesirli->tamsayi. Self ile fark yalniz ilkinde DEGER-DEGISTIRICI; `(0-1)` seklinde C tum ifadeyi i64'te hesaplarken self i32'de hesaplayip genisletiyor — DEGER AYNI, yapisal fark yapi_diff'in kapsaminda degil (yalniz define kumesi). Ifade geneline beklenen tip yaymak cok daha genis bir degisiklik olurdu; olculen kusur literal genisligiydi ve onarim ona sinirlandi.
  HANGI KAPI NEYI OLCTU: codegen_diff 166/166 . yapi_diff 148/148 (20 muaf) . snapshot 50/50 . stdlib_check rc=0. Sabotaj S111 (genisletme dalini kapat) -> fikstur exit 1.
- 2026-09-02 D-538 (NEGATIF SONUC): donus-tipi-guudumlu cikarsama YAZILMADI. Kod degisikligi YOK.
  NEDEN: fallback'in HER sinifi tek tek olculdu ve hicbiri sessiz-yanlis-cevap uretmiyor. `bos_yap<T>() -> Dizi<T>` -> `$i32` ama exit 42 (dizi her genislikte ptr; eleman tipi annotasyonlu bagLAMADAN gelir). `dizi.olustur` -> 0 bayt tahsis + T-bagimsiz yerlesim = gorunmez. `hata_yap`/`k_hata`/`k_tamam` -> D-411'in kendi sinifi (fallback annotasyonla uyusur). `bos_kutu<T>() -> Kutu<T>` -> LINK-RED (`'%4' defined with type '%"Kutu$i32"' but expected '%"Kutu$i64"'`). Ic ice cagri argumani -> LINK-RED, her iki derleyicide.
  YANI: kalan her fallback ya GORUNMEZ ya LOUD. Sessiz olan iki sinif PARAMETRE konumundaydi (D-535, D-536) ve ikisi de kapandi. Bu ozelligin olculebilir tek kazanci YAPISAL (yapi_diff K4 muafiyeti, 12 dosya) — D-430 tam bu gerekcede bir degisikligi geri almisti.
  YAN KAZANC — D-411'IN SINIRI KESINLESTI: iddia DONUS konumunda DOGRU, PARAMETRE konumunda yanlisti. Ayrim olculdu: yanlis tip TASIYICIYA (dizi ptr, skaler register) sigiyorsa SESSIZ; ADLANDIRILMIS TIPE (`%Kutu$i64`) yansiyorsa LLVM yakalar. Bu, gelecekte "fallback guvenli mi?" sorusunun mekanik cevabidir.
  ⚠ TAVAN D-536 SONRASI YENIDEN OLCULDU ve DEGISMEDI (14 olay / 5 bolge): D-536 depoda BULUNMAYAN bir sekli onarmisti. `al T` hala dusuyor cunku D-536'nin dali yalniz TANIMLAYICI argumani kapsiyor, `24_nested_generic` IC ICE CAGRI argumani kullaniyor — daralik bilincli (default-DENY) ve o dosyada zararsiz (tam32).
  YENI IS: ic ice generic-yapi cagrisi (`al(olustur(b))`) HER IKI derleyicide de derlenmiyor. Sirada'ya girdi.
- 2026-09-02 D-539 (C YARISI): ic ice generic-yapi cagrisi (`al(olustur(b))`) HER IKI derleyicide derlenmiyordu (`Cannot allocate unsized type`) — gecerli program reddediliyor, sessiz DEGIL. C tarafi onarildi; self yarisi FARKLI kok ve acik kaldi.
  C KOK 1 (cikarsama): D-536'nin dali yalniz TANIMLAYICI argumanini kapsiyordu; ic ice cagrida yan-kanal yok. Argumanin DEGERLENDIRILMIS IR tipi (`%Kutu$i64`) mangle edilmis adi tasir -> sonekten okunuyor. Bu tek basina yetmedi.
  C KOK 2 (siralama) — VE KAYNAKTAKI YORUM YANLISTI: `mono_tip_tanimlari_emit` yorumu "LLVM adli-tipleri modul-genelinde cozer, forward-ref guvenli" diyordu. Olculdu: `alloca` tipin BOYUTUNU ayristirma aninda ister, IMZA konumu istemez — bu yuzden `define i64 @al$i64(%Kutu$i64 %k)` satiri geciyor ama govdedeki `alloca %Kutu$i64` dusuyordu (tip 211. satirda, kullanim 186'da). Govdeler artik tmpfile'a yaziliyor; sonda ONCE tipler, SONRA tampon. Saf bayt kopyasi — register numaralandirma fonksiyon-yerel.
  SELF YARISI ACIK, FARKLI KOK: self'te `%Kutu$i64` HIC tanimlanmiyor (`yapi_tip_emit` govdelerden ONCE kosar). `yaz_str` dogrudan stdout'a yazar, tampon YOK -> hoist uygulanamaz. Iki secenek Sirada'ya yazildi.
  FIKSTUR EKLENMEDI (bilincli): self duzelene kadar codegen_diff'i kalici kirmizi yapardi (D-421). Yani C yarisi YENI YETENEK icin gate'siz; ancak GERILEME'ye karsi 166+148+50+22+286 olcumle korunuyor ve degisiklik yonu kesin (daha kesin cikarsama + daha erken tip tanimi).
  ⚠⚠ BU TURDA BES OLCUM-ARACI HATASI: (1) `python3 ... 2>/dev/null` -> AssertionError SESSIZ kaldi, "eklendi" satiri cikmadigi halde devam ettim ve BAYAT ikiliyi olctum. (2) Kok sebep: `src/llvm.c` CRLF (8472 satir CRLF, 40 LF) -> COK SATIRLI capalar `
` ile HIC eslesmiyor; tek satirlik capa sart. (3) Tek satirlik capayla eklerken blok mevcut bir YORUM BLOGUNUN ICINE dustu -> `"/*" within comment` + 4 derleme hatasi. (4) `grep -c fn_tmp` = 0 gorunce once "cp calismadi" sandim; gercekte yama hic yazilmamisti. (5) Onceki turdan devreden ders tekrar isledi: sabotaj/yama sonrasi MUTLAKA `grep` ile say.
- 2026-09-02 D-539 SELF-HOST YARISI KAPANDI — madde bitti, fikstur eklendi.
  KOK (C'den FARKLI): self'te `%Kutu$i64` HIC tanimlanmiyordu. `yapi_tip_emit` govdelerden ONCE kosar ve `yaz_str` dogrudan stdout'a yazar (TAMPON YOK) -> C'nin hoist cozumu UYGULANAMAZ.
  COZUM — ON-KAYIT: `mono_kesif` ZATEN vardi ("emit'ten ONCE") ama yalniz AST'deki TIP_ dugumlerini geziyor; probe'da hicbir yerde `Kutu<tam64>` YAZMIYOR, o yuzden kesfedilmiyordu. Yeni `mono_on_kayit`: tek tip paramli her generic yapi icin skaler genislik kumesini (i1/i8/i16/i32/i64/float/double/ptr) kaydeder. Yeni yardimci `mono_kayit_yapi_ir` (IR listesiyle dogrudan kayit — `tip_idx` yok cunku AST dugumu de yok).
  NEDEN UST-YAKLASIM (bilincli): kesin kume, cagri yerindeki T cikarsamasini on-geciste TEKRARLAMAYI ister — ayni soruyu iki yerde ayri yanitlamak D-407'nin ayrisma sinifidir. Kullanilmayan adlandirilmis tip LLVM'de YASAL; maliyet yapi basina 8 satir ve hicbir kapi tip KUMESINI karsilastirmiyor (yapi_diff `define` kumesine bakar).
  DAR TUTULDU: cok paramli yapi (`Cift<A,B>`) kapsanmadi — capraz carpim gerekir ve o sekil olculmus bir kusur uretmiyor.
  HANGI KAPI NEYI OLCTU: codegen_diff 167/167 (davranis; fikstur artik korpusta) . yapi_diff 148/148 (21 muaf — fikstur K4'un 13. ornegi, self taban govdeyi de yayar) . snapshot 50/50 . modul_codegen 22/22 . stdlib rc=0. Sabotaj S112 (on-kaydi kapat) -> fikstur exit 1.
  ⚠ SATIR SONU OLCULDU, VARSAYILMADI: `src/llvm.c` CRLF ama `selfhost/codegen.kem` SAF LF (0 CRLF / 13193 LF). Onceki turda CRLF yuzunden dort yama sessizce dusmustu; bu turda once olcup dogru ayraci sectim.
- 2026-09-02 D-540: kanal omru A'nin ON KOSUL 1'i olculdu. KOD DEGISIKLIGI YOK (madde zaten "kod yazma" diyordu).
  YONTEM: `kemgu --ast` uzerinden, GREP DEGIL — D-532'de tam bu hata yapilmisti. Dogrulama: `selfhost/checker.kem` ve `codegen.kem` grep'te ESLESIYOR ama AST'de hicbir kanal baglamasi YOK (adlar yalniz derleyicinin kendi dizgilerinde geciyor) -> arac grep'in saydigi iki sahte adayi dogru sekilde eledi.
  SONUC — TAVAN SIFIR DEGIL: 23 islev kanal yaratiyor, 23'u de sekli sagliyor (yakalayan==0 ya da join>=yakalayan), 0 ihlal. Bunlardan 4'u `yakalayan>=1` yani birlestirme akil yurutmesi gerektiren gercek vaka: cg_gorev_kanal . cg_rho_sahip_kacis . drf_gorunurluk . kanal_mesaj.
  EN ONEMLI BULGU: D-515'te LeakSanitizer ile SIZDIGI olculen TEK dosya (`kanal_mesaj`) bu kumenin ICINDE. D-515'in dar kurali onu bilerek disarida birakiyordu (goreve yakalanan kanal); A birlestirme-duyarli oldugu icin KAPSIYOR. Yani A'nin kapatacagi gercek, olculmus bir sizinti var -> D-430'un "olculemeyen degisiklik" gerekcesi burada GECERSIZ.
  ⚠⚠ OLCUM ARACI ONCE YANLISTI VE CELISKI ONU ELE VERDI: ilk surum `gönderen(k)`/`alan(k)` PROJEKSIYONLARINI izlemiyordu -> `kanal_mesaj` icin yakalayan=0 dedi. D-511 orada kanalin goreve YAKALANDIGINI olcmustu; iki olcum celisince araci supheli kildim (D-500) ve takma ad izleme ekledim -> yakalayan=1, celiski kalkti. Celiskiyi "eski not bayat" diye gecistirmek yanlis bir tavan sayisi kaydettirirdi.
  ⚠ BU TURDA IKI SESSIZ YAMA: `2>/dev/null` iki kez AssertionError'i/hata ciktisini yuttu ve ilk seferinde yama hic uygulanmadigi halde devam ettim (`grep -c` 0 gosterince yakalandi). Bu oturumda ALTINCI kez.
  KALICI ARTEFAKT: `test/kanal_omru_olcum.py` (basliginda yontem, grep yasagi, arac hatasi ve bilinen sinirlar yazili). Kapiya baglanmadi — bu bir OLCUM araci, bir invaryant degil; ayrica Windows'ta python3 yok (yalniz WSL).
- 2026-09-02 D-541: kanal omru A'nin KAZANCI prototiple olculdu + uygulama tasarimi cikarildi. KOD DEGISIKLIGI YOK.
  NEDEN ONCE PROTOTIP: D-515 acikca "serbest birakma yolu eklemek gercek bir cift-serbest/UAF riski tasir" diyor. Kanit makinesi yazmadan once kazancin GERCEK oldugunu ve UAF uretmedigini olcmek sarttir.
  OLCUM (kanal_mesaj, IR'a elle enjeksiyon): sizinti 192 bayt/3 tahsis -> 8 bayt/1 tahsis . ASan hatasi YOK . stdout birebir . ASan'siz kosumda taban ve prototip IKISI DE exit 15. Kalan 8 bayt KANAL DEGIL (main'de dogrudan malloc = kapanis env sinifi, D-483/D-507).
  ⚠ LSan CIKIS KODUNU MASKELER (sizinti varsa 1) -> davranis dogrulugu AYRICA ASan'siz olculdu; yalniz LSan kosumuna bakip "exit 1, bozuldu" demek yanlis olurdu.
  TEK EMISYON NOKTASI VAR: `rho_yerel_serbest_emit` her ret'ten once cagriliyor (kanal_mesaj.main'de 3 cikis, ucunde de). Kanal serbesti icin yeni mekanizma gerekmiyor.
  KANIT P1-P4 yazildi (yerel+dogrudan kanal_olustur . `imha` YOK . spawn/join UST DUZEY ve join>=spawn . kanal ve projeksiyonlari sizmaz). P3'un gerekcesi: `görev<T>` LINEERDIR, L001/L002/L005 her tutamagin her yolda tam bir kez tuketildigini zaten garanti eder; eksik olan yalniz "tuketen birlestirme mi imha mi" ayrimi ve P2 onu sagLAM bicimde kapatir.
  ⚠⚠ UYGULAMA BU TURDA YAPILMADI VE ISKELE GERI ALINDI — sebep olculdu: `src/ast.h`'de GENEL BIR COCUK YINELEYICI YOK, her gezgin elle yazilmis switch (`escape.c`'de ~30 ozyineleme noktasi). P4 icin llvm.c'ye ikinci bir gezgin yazmak D-407'nin ayrisma sinifidir. Dogru yol `ky_confined`i birlestirme-duyarli gevsetmeyle genisletmek; o dal bugun kosulsuz DENY veriyor (D-511'de kayitli) ve kanal_mesaj'i tam bu yuzden kapsamiyor. Yarim iskele birakmak D-459'un "olu kod" tuzagi olurdu -> `git checkout` ile geri alindi, yeniden kuruldu, kalinti 0 dogrulandi.
- 2026-09-02 D-542: kanal omru A UYGULANMAYA CALISILDI; kanal kaniti GECTI, takma ad kaniti gecmedi. KOD GERI ALINDI (olu kod birakmamak icin, D-459) ama engel TEK NOKTAYA indirildi.
  YAZILAN VE DERLENEN (sifir uyari): escape.c "kanal modu" — `ky_confined` ile AYNI yuruyus, ikinci gezgin YAZILMADI (D-407). Uc gevsetme: `görev_başlat` lambda yakalamasi · kanal yerlesikleri (`kanal_gönder`/`kanal_al`/`gönderen`/`alan`) argumani · sayaclar yuruyusun ICINDE.
  ASIL TASARIM KAZANCI: sayaclari yuruyuse tasimak. `ky_confined` 1 donerse agacin TAMAMI gezilmistir -> spawn/join/imha sayimlari ISLEV GENELIDIR. Boylece D-541 tasariminin "ust duzey deyim" kisiti (P3) TAMAMEN KALKTI ve ayri bir ust-duzey taramasi gerekmedi.
  🎯 OLCULEN: `kanal_mesaj.main` uzerinde kanalin kendisi P1-P4'un DORDUNU DE gecti (conf=1 spawn=1 join=1 imha=0). Emisyonu engelleyen tek sey `gönderen(k)`->g ve `alan(k)`->a takma adlari icin yapilan IKINCI hapsedilme cagrisinin 0 donmesi.
  TAKMA AD KANITI KALDIRILAMAZ: `gönderen`/`alan` PROJEKSIYONDUR, ayni handle'i geri verir. Yalniz kanali kanitlamak saglam degildir — `ver g` ile kanal cerceveyi asar ve serbest birakma UAF uretir.
  ⚠ OLCUM EKSIKTI VE BU BENIM HATAM: takma ad izini cagri ONCESINE koydum, DONUS DEGERINI basmadim -> hangisinin neden 0 dondugu gorulemedi. Bir sonraki turun ilk adimi bu tek satir.
  ⚠ BU TURDA IKI KEZ `
` KACISI GERCEK SATIR SONUNA DONUSTU (python -> C dizgisi, iki katman kacis) ve C dizgilerini bozdu ("missing terminating \" character"). Cozum: ters bolu'yu kaynakta duz yazmak yerine `chr(92)` ile kurmak. CRLF dosyada cok satirli capa da yine tutmadi (D-539'da kayitli).
  ⚠ Uc kez ust uste "serbest cagrisi: 0" gorup kodu commit etmemek DOGRU KARARDI: derlenen ama HIC ATESLENMEYEN kod, D-459'un tam olarak uyardigi tuzaktir (sonraki okuyucu onu "hazir mekanizma" sanir).
- 2026-09-02 D-543: KANAL OMRU ACILDI — `kdl_kanal_serbest` artik cagriliyor (D-511'den beri sifirdi).
  KANIT P1-P4 uygulandi. ASIL TASARIM KARARI: ikinci gezgin YAZILMADI (D-407) — `ky_confined`e "kanal modu" eklendi ve SAYACLAR DA AYNI YURUYUSTE toplandi. `ky_confined` 1 donerse agacin tamami gezilmistir, yani spawn/join/imha sayimlari ISLEV GENELIDIR; bu, D-541 tasarimindaki "ust duzey deyim" kisitini tamamen kaldirdi.
  🔴 ASan AVI GERCEK BIR UAF YAKALADI: `drf_gorunurluk` -> SEGV in __asan free (0x000100000004). Kok: LIFTED LAMBDA govdesindeki `ver` de ayni cikis noktasindan geciyor ve kanal listesi main'den SIFIRLANMAMIS kaliyordu -> lambda kapsaminda baska bir yuva bulunup COP ISARETCI serbest birakiliyordu. Fikstur yine exit 15 veriyordu; yalniz davranisa bakan kapi bunu GOREMEZDI. Bu, yeni kapinin ucuncu olcumunun (ASan sagligi) var olus gerekcesidir.
  ⚠⚠ UC TUR BOYUNCA KANIT HIC ATESLENMEDI VE SEBEP BAYT SAYIMIYDI: `kanal_al` 8 bayt (9 yazmistim), `kanal_gönder` 13 (14 yazmistim). `görev_başlat` 14 DOGRUYDU — bu yuzden spawn sayaci calisiyor, gevsetme calismiyordu ve "conf=0 spawn=1 join=0" gibi kafa karistirici bir tablo cikiyordu. Tanıyı kapatan sey argumanin GERCEK DUGUM TIPINI basmakti; tahminle ucuncu tura kadar yanlis yerlere baktim.
  YENI KAPI `calistir_kanal_omru` (test_tumu + .PHONY): yapi (>=1 serbest) + davranis (exit 15) + saglik (ASan 0 hata).
  OLCULEN KAZANC: cg_kanal_metin/_param/_tam64/_temel -> sizinti 0 bayt (oncesinde kanal siziyordu). kanal_mesaj ve cg_kanal_yon projeksiyon kullandigi icin BILEREK kapsam disi.
  V1 SINIRLARI (ikisi de Sirada'ya girdi): projeksiyonlu kanallar · self-host portu (C 2 serbest, self 0).
  KAPILAR: kanal_omru 3/3 . codegen_diff 167/167 . yapi_diff 148/148. Sabotaj 2/2 (S113b kanal modu, S114 emisyon) -> ikisi rc=2. ⚠ Ilk sabotaj S113 GECERSIZDI: `#define` derlemeyi kirdi, yani kapiyi degil YAPIMI olctu.
- 2026-09-02 D-544: KANAL OMRU V2 (a) — `gönderen`/`alan` PROJEKSIYONLU kanallar artik serbest ALIYOR. D-543'un V1 sinirini (projeksiyon varsa DENY) kaldirdi.
  NE YAPILDI: escape.c kanal modunda projeksiyonlar SEFFAF yapildi; llvm.c'deki toptan `d543_projeksiyon_var` DENY'i silinip yerine TAKMA AD KANITI kondu (her `değişken g = gönderen(k)` baglamasi icin `escape_kanal_hapsedilmis(g)` ayrica kanitlanir; biri dusen kanal serbest ALMAZ).
  OLCULEN KAZANC: kanal_mesaj sizinti 192 -> 8 bayt (D-541 prototipiyle birebir). cg_kanal_yon 1 serbest, sizinti 0.
  ⚠ KENDI ILK COZUMUM SAGLAMSIZDI VE NEGATIF FIKSTUR YAKALADI: projeksiyonu KOSULSUZ seffaf yapmak `ver gönderen(k)` seklini de hapsedilmis gosteriyordu -> kanal serbest birakiliyor, CAGIRAN olu uc tutuyor = UAF (olculdu: exit 139, ASan SEGV). Onarim: seffaflik `ver` ALT-AGACINDA KAPALI (`g_ver_altinda`). Projeksiyon disarida seffaf, `ver` icinde DEGIL.
  HANGI KAPI NEYI OLCTU: kanal_omru 3/3 -> 5/5 (yapisal + davranis + saglik + NEGATIF yapisal + NEGATIF davranis) . codegen_diff 169/169 . yapi_diff 149/149 (22 muaf) . sifir uyari 38/0.
  NEGATIF FIKSTUR IKI SEKIL TASIYOR VE IKISI DE GEREKLI: (1) `ver gönderen(k)` dogrudan kacis — escape.c korumasini (S116) gate'ler; (2) `değişken g = gönderen(k); ver g;` takma ad kacisi — `ver` icinde `k` HIC GECMEZ, escape yuruyusu tek basina `k`yi hapsedilmis sanir; llvm.c takma ad kanitini (S115) gate'leyen TEK sekil budur.
  ⚠ S115 ILK TURDA YESIL KALDI VE BU BIR KOR NOKTAYDI: sekil (2) korpusta YOKTU, yani takma ad kaniti ayirt edilemez = dogrulanmamis yuzeydi (D-430). ONCE dead-code mi diye olctum — 8 kanal dosyasinin sekizinde de S115'li ve temiz cikti BIREBIR ayniydi; silmeye karar vermeden once "hangi sekli koruyor" diye sorunca sekil (2) cikti. Silseydim bir UAF yolu acilacakti. "Ayirt edilemiyor" ONCE korpusu supheli kilar (D-356), kodu degil.
  YANLIS GIDEN DENEMELER: (1) S115'in ilk capasi 16 bosluk girintiliydi, kod 8 — python assert'i yakaladi ama betik `set -u` altinda DEVAM ETTI ve TEMIZ derleyiciyle kapi kosup "S115 rc=0" bastı; grep -c 0 dedigi icin gecersiz oldugu anlasildi (D-500). (2) 2 dk'da zaman asimina ugrayan foreground `make` OLMEDI, orphan olarak kosmaya devam etti; ikinci kosumu baslatinca ikisi `build/kanal_kapi/n` uzerinde carpisti ve temiz kaynakta "1 serbest" + asilma gosterdi — bir an onarimi bozdum sandim. CLAUDE.md'de yazili (D-414) ve yine yapildi. (3) Kapinin fikstur kosumlarinda ZAMAN ASIMI YOKTU; asilan kapi sessiz kapi kadar kotudur (D-466/D-468/D-471) — `timeout 20/60` eklendi ve daha ayni turda karsiligini verdi: S116 asilma yerine temiz `exit=124` verdi.
  KAPSAM DISI (bilincli): self-host portu — Sirada'da (b) olarak duruyor.
- 2026-09-02 D-544-b: KANAL OMRU V2 (b) — self-host portu. `selfhost/codegen.kem` artik kanal serbesti yayiyor; 8 kanal dosyasinin sekizinde de C ile BIREBIR (3/3, 1/1, 2/2, 0/0, 1/1, 1/1, 1/1, 2/2).
  NE YAPILDI: yeni analiz ICAT EDILMEDI — `ky_confined` aynasi self-host'ta zaten vardi; eklenen yalniz KANAL KIPI (Ayr'e k_modu/k_spawn/k_join/k_imha/k_ver + kanal_ad; ky_kanal_seffaf_mi; CAGRI dalinda sayaclar + iki gevsetme; YENI VER dali; kanal_kanit_kur; kanal_serbest_emit). Sayimlar ayni yuruyusten toplanir.
  KAPI GENISLETILDI: `calistir_kanal_omru` artik IKI derleyiciyi de olcuyor (3/3 -> 10/10) ve Makefile'da $(BUILD)/codegen$(EXE)'e bagimli. Port yalniz C'de kalsaydi codegen_diff cikis koduna baktigi icin ayrisma SESSIZ kalirdi (D-486).
  HANGI KAPI NEYI OLCTU: kanal_omru 10/10 (C+SELF) . codegen_diff 169/169 . yapi_diff 149/149 (22 muaf) . check_genis 133/133 . self_driver TUM MODLAR + FIXPOINT.
  SABOTAJ 2/2: S117 (self `ver` korumasi) ve S118 (self takma ad kaniti) -> ikisi de negatif fiksturde 1 serbest + SEGV, rc=2.
  YANLIS GIDEN DENEMELER — BIR TUR OLCUM GECERSIZ CIKTI: onceki turda oldurulen bir ORPHAN kosum WSL agacindaki src/escape.c'yi S116 UYGULANMIS halde birakti. Once IKILI eskiydi (S116'dan once kurulmus) -> parite tablosu ve kanal_omru 10/10 yesil verdi, yani iddia dogruydu ama KANIT gecersizdi. Sonra bir `make` kaynagi yakaladi ve SABOTE EDILMIS bir oracle kuruldu -> codegen_diff 168/169 kirmiziya dondu ve bir an "portum bozdu" sandim; git'teki kaynak TEMIZDI. Cozum: iki agaci `diff`leyen bir resync adimi. DERS: sabotaj izini `grep`lemek YETMEZ — izin KAYNAKTA olmasi ile IKILIDE olmasi ayri seylerdir; olcumden once agaclari diff'le.
  OLUMLU YAN: kapi tam da tasarlandigi gibi calisti — S116'li oracle negatif fiksturde serbest yayip SEGV verdi. D-544'un negatif fiksturu, baska bir sey icin yazilmisken gercek bir kontaminasyonu yakaladi.
- 2026-09-03 D-545: DEGISMEZ AVI — REALTIME/WCET ekseni. `gerçekzamanlı` KARSILIKLI OZYINELEMEYE ACIKTI; RT003 zincir-farkindali yapildi.
  AV SONUCU (6 sekil): `eşleş` kolunda dongu RT002 ✓ . `güvensiz` blokta dongu RT002 ✓ . `dizi_olustur(N)` RT005 ✓ . duz govde TEMIZ ✓ . karsilikli a->b->a **OK** 🔴 . dolayli a->ara->a **OK** 🔴
  KESIN OLCUM: `a(n){ver b(n);}` + `b(n){ver a(n);}` --check rc=0 TEMIZ, kosumda exit 124 (asildi). `gerçekzamanlı`nin TEK vaadi sinirli WCET'tir; bu sekilde vaat tamamen geciersizdi.
  KOK: RT003 yalniz DOGRUDAN self-call'i soruyordu. wcet.h'nin kendi notu sinirin ADINI koymus ama garantinin DELINDIGI hic olculmemis. D-503/D-517 sinifi: kural var, sorulmadigi yer var.
  ONARIM (yeni tani kodu YOK): cagri zinciri `cagri_yigin`da tutulur; cagrilan islev zaten acik zincirdeyse RT003. Kesif `walk`'un KENDISIYLE yapilir — ikinci gezgin YAZILMADI (D-407): ic yuruyus `sessiz`, tani yalniz dis cagri yerinde bir kez basilir, hata_sayisi ic yuruyuste artmaz. Sembol->ast_dugumu zaten vardi, yeni tesisat/ana.c/imza degisikligi GEREKMEDI.
  DERINLIK ASIMI SESSIZ ATLAMA DEGIL: zincir 32'yi asarsa RT005 (WCET hesaplanamaz) = default-DENY. Sessizce gecmek kapatilan kusurun ta kendisi olurdu.
  WCET SAYISI DEGISMEDI: ic yuruyusun maliyeti ATILIR, V1 sozlesmesi geregi cagri basina sabit 50 korunur -> ekleme sayi-notr.
  YANLIS-POZITIF: 600+ .kem tarandi, tek RT tanisi ve o da ONCEDEN VAR OLAN (RT005 callee bilinmiyor, p3_bildirimler) -> sifir yeni yanlis-pozitif.
  POZITIF OLCUM ZATEN VARDI: W18 (a->b, dongu yok -> 0 hata). Yalniz negatif sekiller olsaydi "her cagriyi RT003 yap" sabotaji GECERDI (D-425).
  HANGI KAPI NEYI OLCTU: wcet_test 35 -> 37/37 . sifir uyari 38/0 . check_kapisi 274/281 (0 RED) . checker_diff 170/170 (0 muaf) . check_genis 133/133 . codegen_diff 169/169. Sabotaj S119 (zincir kesfini kapat) -> [19] ve [20] kirmizi, 35/37, rc=2.
  YANLIS GIDEN DENEMELER: (1) ilk karsilikli-ozyineleme probe'um ileri bildirim (`işlev b(..) -> tam32;`) kullaniyordu — KEMGU'da YOK, P017; probe dili degil sozdizimini olcuyordu (iki islevi sirayla tanimlamak yeterli, pre_populate ileri referansi cozuyor). (2) `dizi_oluştur` degil `dizi_olustur`. (3) `rc=$?`'yi BORUDAN SONRA okudum -> head'in kodu; ilk turda alti probe de sahte "rc=0" gosterdi (D-444 tekrari). (4) src/wcet.{c,h} CRLF; cok satirli \n capalari tutmadi. (5) test_wcet.c'ye yamada python `\x` ve `\n` kaciislarini YORUMLADI -> mojibake + gercek satir sonu; chr(92) ile yeniden kuruldu.
- 2026-09-03 D-546: DEGISMEZ AVI — SIMD ekseni. `vektor_eleman` ARALIK DISI INDEKSTE UB'ydi; calisma zamani panigi eklendi (C + self-host).
  AV SONUCU (6 sekil): <4>*<8> V003 ✓ . <tam32,4>*<kesirli32,4> V003 ✓ . vektör<T,0> V002 ✓ . annot <8> = deger <4> T001 ✓ . vektor_eleman(v,99) **OK** 🔴 . vektor_eleman(v,-1) **OK** 🔴
  KESIN OLCUM: aralik disi indekste -O0 exit=224, -O2 exit=1 — AYNI PROGRAM, AYNI IR. `extractelement` LLVM'de poison uretir; bu sarma degil, cevabin optimizasyon seviyesine bagli olmasidir (deponun en agir saydigi sinif).
  ONARIM: yeni politika ICAT EDILMEDI. Dil bu sinif icin panik secmisti (dizi siniri D-069, sifira bolme D-502, kaydirma D-514) — ayni kalip: inline icmp uge + br + kdl_panik(noreturn) + unreachable. Yeni tani kodu YOK, tip degisikligi YOK. C ve selfhost/codegen.kem birebir (-O0 ve -O2'de 134).
  IKI BILINCLI DARALTMA, ikisi de fiksturde POZITIF olculuyor: (a) TEK karsilastirma yeter — `icmp uge` ISARETSIZ, negatif indeks ayni dala duser, ayri `icmp slt 0` gereksiz dal olurdu (negatif.kem); (b) sabit ve aralikta olan indekste kontrol yayilmaz, aralik disi SABITTE yayilir. degisken_disi.kem ise derleme-zamani reddin KACIRACAGI sekli olcer (D-514'un kendi gerekcesi).
  KAPI AYRI ACILMADI: calistir_sifir_bolme genisletildi (20 -> 28 olcum). Mekanizma ve degismez ayni; ucuncu kapi envanteri gereksiz bolerdi. normal.kem (21+21 -> 42) ZORUNLU — yalniz negatif sekiller olsaydi "her vektor_eleman'i panikletir" sabotaji GECERDI (D-425).
  HANGI KAPI NEYI OLCTU: sifir_bolme 28/28 (C+SELF) . simd_test 30/30 . simd_llvm_test 5/5 . llvm_test 286/286 . codegen_diff 169/169 . yapi_diff 149/149 (22 muaf) . sifir uyari 38/0. Sabotaj S120 (C) -> exit=0/0/7; S121 (self) -> exit=100/100/7; ikisi de rc=2.
  YANLIS GIDEN DENEMELER — ucu de KAYITLI derslerin tekrari: (1) heredoc "\n"i "\n"e indirgedi -> uretilen C kaynagina GERCEK satir sonu girdi, dizgi literali koptu (D-518 ile ayni; care chr(92)). (2) src/llvm.c SATIR SONLARI KARISIK — dosyanin cogu CRLF ama vektor_eleman bolgesi LF; \r\n ile kurulan cok satirli capa SESSIZCE eslesmedi. Care: satir sonunu kaynaktan OKU, capayi INDEKSLE bul. (3) BAYT SAYISINI SAYDIM, OLCMEDIM: "vektor lane indeksi gecersiz" 28+NUL=29, ben [27 x i8] yazdim -> tum IR LINK-RED (D-543'te uc tur kaybettiren hatanin aynisi); len(msg)+1 ile hesaplatildi.
- 2026-09-03 D-547: `codegen_genis`in 9 atlamasi olculdu ve kurate listeye baglandi. KAYITLI GEREKCE YANLIS CIKTI.
  KOR NOKTA: oracle-link dali TAMAMEN SESSIZDI (`atla=$((atla+1)); continue`, mesaj YOK) -> oracle tarafindaki her gerileme sessizce yutuluyordu. D-518'de codegen_diff icin olculen kor noktanin birebir aynisi.
  OLCUM (9 link hatasi TEK TEK okundu): 8'i GERCEKTEN bare-metal (kdl_mmio_oku32 / kdl_mmio_yaz32, host runtime'da yok); 9'uncusu `05_yapi` ve o bare-metal DEGIL — C oracle'in KENDISI gecersiz IR uretiyor ("base element of getelementptr must be sized", D-419'da zaten olculmus). Roadmap "hepsi mesru bare-metal" diyordu; yanlisti (D-406: muafiyet gerekcesi de bir iddiadir).
  TASARIM: liste AD-BAZLI DEGIL SEBEP-BAZLI. BM_MUAF'taki bir dosya BASKA bir sebeple linklenemezse kapi KIRMIZI olur (kdl_mmio_ araniyor). Duz ad listesi o gerilemeyi de yutardi — muafiyet kabul edilebilirligi GENISLETMEMELI (D-421).
  HANGI KAPI NEYI OLCTU: codegen_genis 70/70 (9 atlandi, her biri artik ADIYLA ve GEREKCESIYLE basiliyor) . codegen_diff 169/169 . sifir_bolme 28/28.
  SABOTAJ 2/2: S122 (mmio_smoke'u listeden cikar) -> "oracle link BASARISIZ ve kurate listede YOK", 70/71, rc=2. S123 (05_yapi'yi BM_MUAF'a tasi) -> "listede ama link hatasi MMIO DEGIL", 70/71, rc=2. Ikisi yeni mantigin IKI DALINI da olcuyor.
  UCUNCU SABOTAJ GECERSIZDI VE BIR SEY OGRETTI: S124 gercek bir oracle gerilemesi denemesiydi (D-546 panik bayti 29->27). Kapi rc=2 verdi ama KENDI mesajlarindan hicbirini basmadi — o gerileme once `build/codegen`in kurulmasini kiriyor, kapi hic kosmuyor (S113 sinifi: sabotaj kapiyi degil YAPIMI olctu). Bulgu: bu dala ulasacak kadar agir bir oracle gerilemesi cogunlukla bootstrap'i DAHA ONCE ve DAHA GURULTULU kirar; kurasyonun asil degeri TEK BIR DOSYAYI etkileyen dar gerilemelerdir, ki S122/S123 tam o dallari olcuyor.
  YANLIS GIDEN DENEME: ilk taramada `undefined symbol:` (lld sozdizimi) aradim; bu ortamda baglayici GNU ld ve "undefined reference to" yaziyor -> sekiz dosyanin da sembol listesi BOS gorundu, bir an "sebep bilinmiyor" diye kaydedecektim.
- 2026-09-03 D-548: `lean_tam`i otomatiklestirmenin UC YOLU DA olculdu — hicbiri bugun atesle(ne)miyor. Yerine D-529'un onarimi kapilandi.
  OLCUM (madde "kod yazmadan once hangisinin GERCEKTEN kosulacagini degerlendir" diyordu): (a) belge eskir — D-530'un ARM64 listesi mesruydu cunku O IS otomatiklestirilemez, bu edilebilir. (b) CI VAR (.github/workflows/ci.yml, claude/** push'unda test_tumu) AMA origin/main D-349'da, HEAD D-547 -> ~200 artim PUSH EDILMEMIS; CI bu islerin hicbirini gormedi. (c) WSL'de lean/lake/elan YOK (command -v -> hicbiri); Windows'taki lake.exe bayat bir elan kilidiyle bloke ("held by PID 30356") ve ag guncellemesi istiyor.
  SONUC: lean_tam OPT-IN KALDI. Sessizce atlayan ya da her kosumda "atlandi" basan bir kapi eklemek D-486/D-490'da kapatilan kapsam yanilsamasinin ta kendisi olurdu.
  BUNUN YERINE ATESLENEBILEN SEY KAPILANDI: `lake build`i aylarca kosulamaz kilan sey bir ispat sorunu degil, KULLANILMAYAN bir `require mathlib` idi. O satir geri gelirse ispatlar yine sessizce derlenmez olur ve hicbir sey fark etmez (lean_tam opt-in, lean_sorry lakefile'a bakmiyordu). Kural lean_sorry'ye eklendi — o kapi test_tumu'da HER KOSUMDA calisir (ayri kapi envanteri gereksiz bolerdi; D-514/D-546 deseni).
  KURAL "require olmasin" DEGIL: bir require ancak GERCEK bir kullanima dayaniyorsa mesrudur (D-529'un kendi cumlesi) -> `require X` varsa en az bir dosya `import X...` etmeli.
  HANGI KAPI NEYI OLCTU: lean_sorry 32 dosya, 0 sorry/admit + lakefile require artigi YOK. Sabotaj 2/2: S125 (kullanilmayan require geri koy) -> rc=2; S126 POZITIF (gercekten import edilen require) -> rc=0. Pozitif olmasaydi "her require'i reddet" sabotaji GECERDI (D-425).
  KAPI ILK KOSUMUNDA GERCEK BIR SEY BULDU: WSL agacindaki lakefile.lean HALA ESKI SURUMDU — D-529'un onarimi Windows'ta yapilmis, gate'lerin kostugu agaca hic kopyalanmamis. Yani o ortamda lake build bugun de eski gerekcesiyle duserdi. Iki-agac senkron kaybinin bu oturumdaki UCUNCU ornegi.
  YANLIS GIDEN DENEME: kendi kapimda mesaj dizgilerinde backtick kullandim; cift tirnak icinde backtick KOMUT IKAMESIDIR, yani kapi `require`i calistirmayi deneyecekti. `bash -n` bunu GECERLI sayar; yalnizca satiri okumak yakaladi (D-456'da check_genis muafiyet dizgisinde olculen sinifin aynisi).
- 2026-09-03 D-549: `lean_tam` ARTIK GERCEKTEN DERLIYOR (Windows worktree'de). D-548'in (c) gerekcesi YANLISTI, yerinde duzeltildi.
  KENDI OLCUM HATAM: D-548'de "Windows'taki lake.exe bayat bir elan kilidiyle bloke ve ag guncellemesi istiyor" yazmistim. Kilit GECICIYDI; ag denemesi ise lake.exe'yi PROJE DIZINI DISINDA calistirdigim icindi — orada elan projenin pinini goremez, VARSAYILAN toolchain'i cozup indirmeye kalkar. Proje leanprover/lean4:v4.29.0 pinliyor ve o surum KURULU. Dogru olcum: WSL->Windows interop calisiyor, proje dizininde `lake build` CEVRIMDISI 33 is / 58 sn / rc=0.
  ASIL KOK KESIFTI, ORTAM DEGIL: D-529 kapiyi "lean/lake Windows'ta, takim WSL'de" diye opt-in birakmisti; gercekte harness `lake` ararken ne /mnt/c/.../.elan/bin'e ne de `.exe` adina bakiyordu -> WSL'de HER ZAMAN atliyordu. Arama duzeltildi; kapi artik Windows worktree'sinden kosuldugunda ispatlari GERCEKTEN derliyor (31 .olean).
  AMA BIR ORTAM SINIRI GERCEK VE INCE: Windows lake, projesi WSL DOSYA SISTEMINDE olan bir agaci derleyemez — interop'ta cwd Windows'a UNC gorunur; `lake --version` calisir (dosya sistemi isi yok) ama `lake build` yalnizca "error: 1" der. /mnt/c worktree rc=0, ~/kemgu rc=1. Kapi bu durumu artik ADIYLA bildirip atliyor (sebep + cozum yazili), eski yaniltici "lake YOK" mesajiyla degil.
  test_tumu'ya BAGLANMADI: D-548'in karari GECERLI ama artik DOGRU gerekceyle — takim ~/kemgu'da kosuyor ve orada bu kapi YAPISAL olarak kosamaz.
  YOL USTUNDE D-529'DAN KALAN KUSUR: hata yolundaki echo satirinda backtick KOMUT IKAMESIDIR -> ag hatasi dalinda kapi `require`i calistirmayi deneyecekti. Ayrica S127 ilk turda kirmizi verdi ama ekranda yalnizca bir LINTER IPUCU vardi; artik once gercek `error:` satirlari basiliyor (dosya:satir:sutun ile).
  HANGI KAPI NEYI OLCTU: lean_tam Windows worktree'de rc=0 (31 .olean), WSL fs'te bildirilerek atliyor . lean_sorry 32 dosya, 0 sorry + require artigi yok. Sabotaj S127 (theorem s127_bozuk : 1 = 2 := rfl) -> rc=1, hata satiri ve konumu adiyla basiliyor; geri alinca rc=0.
  YANLIS GIDEN DENEME (BACKTICK TUZAGI UCUNCU KEZ, bu kez ZARARLI): belge yamasini `wsl bash -lc "...python3 - <<PY..."` icinde gecirdim; cift tirnakli kabuk dizgisindeki backtick'ler KOMUT IKAMESI oldu ve `test_tumu` ile `lake` GERCEKTEN CALISTI (elan yeniden indirmeye kalkti, komut 2 dk'da zaman asimina ugradi, CLAUDE.md hic yazilmadi). Care: python'u DOSYAYA yaz, kabuk dizgisine gomme.
- 2026-09-03 D-550: SELF-HOST'a RT denetimi eklendi (RT001/RT002/RT003) — D-545'te olculen kor nokta kapandi.
  ONCE HANGI KAPININ GORECEGI OLCULDU (maddenin kendi talimati): gecici fikstur korpusa konup olculdu -> checker_diff 170/171 rc=2 GORUYOR (selfhost/checker.kem); self_driver 137/138 rc=2 GORUYOR (selfhost/codegen.kem); codegen_genis / ct_bariyer GORMUYOR. Sonuncusunun sebebi sasirtici: C `--llvm` RT ihlaline RAGMEN IR uretiyor (rc=0, 193 satir) — WCET yalniz `--check` yolunu durduruyor. Yani bu bir CHECK-ZAMANI isidir.
  UC-UYGULAMA TUZAGI TAM BEKLENDIGI GIBI ISIRDI: once yalniz checker.kem'e portladim -> checker_diff 171/171 YESIL ama self_driver 137/138 KIRMIZI (o kapi codegen.kem'i okuyor). D-517'de kayitli, madde de uyariyordu, yine de tek uygulamayla baslamak yetmedi.
  ONARIM: parser GERCEKZAMANLI'yi YUTUYORDU (D-363'un `genel` deseni) -> `çıplak` yan-kanalinin birebir aynasi (rt_node). Tek yuruyus: RT001 (dizi literali/lambda), RT002 (iken/için), RT003 (DOGRUDAN self-call). Yeni tani kodu YOK.
  IKI INCE PARITE KURALI, ikisi de C'den OKUNDU: (a) C islev basina EN FAZLA BIR tani verir (walk ilk hatada -1 doner) -> self yuruyusu de ilk bulguda durur, yoksa dump ayrisirdi; (b) SIRA — C'de WCET tip kontrolunden SONRA ayri bir gecistir (ana.c), bu yuzden pas kontrol_ust'tan sonra kosar.
  KAPSAM (bilincli): RT004/RT005 (cagrilanin gerceklzamanli olup olmadigini + yerlesik kumesini bilmek ister), RT003'un ZINCIR dali (D-545), RT007 (parser `çevrim:` alanini atiyor) PORTLANMADI. Kismi port ayrismayi ARTIRMAZ: self bugun hic RT tanisi vermiyordu.
  YANLIS-POZITIF: 727 .kem tarandi, TEK sapma p3_bildirimler.kem'de C'nin RT005'i (portlanmayan kod). Self'in C'de olmayan RT bastigi tek dosya YOK.
  HANGI KAPI NEYI OLCTU: checker_diff 171/171 (0 muaf) . self_driver 138/138 + FIXPOINT . check_kapisi 274/281 (0 RED) . check_genis 133/133 . codegen_diff 169/169 . ct_bariyer 14/14 . wcet_test 37/37. Sabotaj 2/2: S128 (checker pasi) -> 170/171 rc=2; S129 (codegen pasi) -> 137/138 rc=2.
  FIKSTUR tc46_01_realtime.kem: uc negatif sekil AYRI islevlerde (tek-tani kurali yuzunden sart) + IKI pozitif (`temiz` gecerli gerceklzamanli islev, `normal` gerceklzamanli DEGIL -> dongusu serbest). Pozitifler olmasa "her gerceklzamanli islevi reddet" sabotaji GECERDI (D-425).
- 2026-09-03 D-551: CI ortam denetimi yapildi; yol ustunde `kanal_omru`da SESSIZ GECEN bir olcum bulundu ve onarildi.
  MADDENIN KARAR KISMI MEHMET'IN (push disa donuk bir islem); OLCUM kismi yapildi. Arac envanteri (72 kapi, kelime-sinirli tarama): setarch -> asan_e2e_denetim + asan_matris_calistir + kanal_omru; /usr/bin/time -> perf_bellek (KORUMALI); lake -> lean_* (KORUMALI); qemu -> Makefile (D-453, bildirerek atlar); ld.lld + objcopy -> Makefile bare-metal.
  CI ubuntu-latest'te yalniz clang gcc make file kuruyor: setarch (util-linux) ve objcopy (binutils) taban imajda gelir, ld.lld ve qemu GELMEZ -> bare-metal/QEMU kapilari atlanir. ⚠ BU SON CUMLE CIKARIMDIR, OLCUM DEGIL — CI kosulmadigi icin dogrulanamaz; gercek liste ilk yesil CI kosumunda olculmeli.
  ASIL BULGU — KENDI YAZDIGIM KAPIDA SESSIZ GECEN OLCUM: kanal_omru'in ASan saglik satiri setarch'i KORUMASIZ cagiriyor ve `|| true` ile bitiyordu. setarch basarisizsa program HIC KOSMUYOR, hata dosyasi BOS kaliyor, grep -c 0 donuyor -> SAGLIK olcumu SESSIZCE GECIYOR. Oysa o olcum bu kapinin VAR OLUS GEREKCESIYDI: D-543'te gercek bir UAF'i (SEGV in __asan free) yalnizca o yakalamisti.
  AYIRT EDICI DENEY (D-534 deseni): `setarch -R true` BASARAN ama gercek kosumda 127 donen sahte ikili ile -> fix VAR: "ASan ikilisi KOSMADI (rc=127)", rc=2; fix YOK: "10/10 olcum gecti", rc=0. Yani duzeltme olmadan kapi yesil kalip HICBIR SEY olcmuyordu.
  ONARIM IKI PARCALI: (a) ASAN_RUN YETENEGI OLCULEREK kuruluyor — `command -v setarch` YETMEZ, bazi cekirdeklerde setarch var ama -R basarisiz olur, o yuzden `setarch -R true` kosturulur (asan_matris_calistir.sh'in zaten kullandigi desen; kardes harness'lar korumaliydi, YALNIZ BENIMKI DEGILDI). (b) ASan kosumunun CIKIS KODU okunuyor: 127 (komut yok) ve 124 (zaman asimi) artik SERT HATA — bos hata dosyasi hem "temiz" hem "hic calismadi" demektir.
  HANGI KAPI NEYI OLCTU: kanal_omru 10/10 . asan_matris 12/12.
  YANLIS GIDEN DENEME: ilk sabotajim S130 GECERSIZDI — setarch'i tumuyle 127 yapinca yetenek probe'u DOGRU sekilde ASAN_RUN=""e dusuyor ve ikili setarch'siz GERCEKTEN kosuyor; yani kusuru degil ZARIF DUSUSU olcmustum. Dogru sabotaj probe'u gecirip gercek kosumu bozan sahte ikilidir.
- 2026-09-03 D-552: RT004 / RT005 / RT003-ZINCIR self-host'a portlandi (checker.kem + codegen.kem). Sabotaj YANLIS BIR KURAL buldu.
  ONCE ULASILABILIRLIK OLCULDU (maddenin talimati, D-360): besinin besi de ulasilabilir — RT004, RT005 (bilinmeyen callee), RT005 (dolayli cagri), RT003 (zincir), RT007 (asm cevrim yok) + pozitif `cevrim: 24` -> OK.
  PORTLANAN (dordu): RT004, RT005 x2, RT003-zincir. C D-545'in `sessiz` bayragi birebir aynalandi: ic kesif yuruyusu tani basmaz, yalniz zincir bulgusu bayrakla dis cagri yerine tasinir. Derinlik asimi -> RT005 (sessiz atlama DEGIL). Dil `dizi_cikar` sunmuyor -> yigin icin mantiksal uzunluk sayaci (rt_yigin_n).
  ASIL BULGU — SABOTAJIN SESSIZLIGI YANLIS BIR KURALI ACIGA CIKARDI: ilk surumum yerlesikleri ayiriyordu ("C'nin sembol tablosu yerlesikleri bulmaz -> RT005"). 727 dosyalik tarama SIFIR sapma dedi ve kural dogru gorunuyordu. Ama S133 (ayrimi kapat) YESIL kaldi -> mekanizma ayirt edilemiyordu. Ayirt edici sekli arayinca kural CURUDU: `metin_uzunluk` (KAYITLI yerlesik) C'de RT004 verir, benim surumum RT005 diyordu. Yani ayrim "yerlesik mi" degil "SEMBOL MU" ayrimidir ve fn_ad uyeligi tam olarak onu verir. fn_kul alani hem GEREKSIZ hem YANLISTI, kaldirildi (D-459).
  DERS: 727 dosyalik yesil bir tarama kuralin DOGRU oldugunu kanitlamaz — yalniz korpusun o sekli icermedigini gosterir (D-356'nin altinci tekrari). Kurali kanitlayan sey, sabotajin KIRMIZI olabildigi bir seklin VAR OLMASIDIR.
  PORTLANMADI — RT007: parser `cevrim:` alanini tuketip atiyor, iki uygulamada da yan-kanal gerekiyor. Ulasilabilir ve gate'lenebilir oldugu OLCULDU (gecerli probe'lar yazildi); ayri is olarak Sirada'da.
  HANGI KAPI NEYI OLCTU: checker_diff 172/172 (0 muaf) . self_driver 139/139 + FIXPOINT . check_kapisi 274/281 (0 RED) . check_genis 133/133 . codegen_diff 169/169 . wcet_test 37/37 . repo taramasi 728 dosya 0 RT sapmasi.
  SABOTAJ 3/3 gecerli: S132 (checker zincir dali) -> 171/172 rc=2; S134 (codegen RT004/RT005) -> 138/139 rc=2; S135 (yanlis yerlesik ayrimini geri getir) -> 171/172 rc=2. S133 GECERSIZ DEGIL, TESHISTI: yesil kalmasi kapinin zayifligini degil KURALIN YANLISLIGINI gosterdi.
  YANLIS GIDEN DENEME: codegen.kem'e yardimci blok kopyalarken aralik tasti ve `yerel_tip_filtrele` iki kez tanimlandi (T024). Blok cikarimini "iki capa arasi tek aralik" + `assert` ile saglamlastirdim; ilk assert de yanlisti (yorumdaki "işlev" kelimesini sayiyordu, satir basi aranmali).
- 2026-09-03 D-553: RT007 self-host'a portlandi — RT alt-sistemi TAMAMLANDI (7/7).
  D-550'de "parser `cevrim:` alanini atiyor -> portlanamaz" diye kaydedilen son koddu. Engel gercekti ama ASILABILIRDI: alan TUKETILIYOR, KAYDEDILMIYORDU.
  COZUM D-351'in DESENI: asm_node ile PARALEL bir bayrak (asm_cev). Dugume ALAN EKLENMEDI — bu SART, cunku --ast/--parse dump'i dugum alanlarini basar ve bir alan eklemek parser_diff + snapshot paritesini SESSIZCE bozardi. Olculdu: parser_diff 13/13, snapshot 50/50 bozulmadi.
  BILINMEYEN DUGUM -> "cevrim YOK" sayilir (default-DENY): yan-kanalda kaydi olmayan asm dugumu RT007 alir. C'nin kendi gerekcesi budur — opak asm maliyeti sessizce 0 sayilamaz.
  RT ALT-SISTEMI ARTIK TAM: RT001, RT002, RT003 (dogrudan VE zincir), RT004, RT005 (bilinmeyen callee VE dolayli cagri), RT007 — yedisi de C <-> checker.kem <-> codegen.kem uclusunde birebir. Repo taramasi 729 dosya, 0 RT sapmasi.
  HANGI KAPI NEYI OLCTU: checker_diff 173/173 (0 muaf) . self_driver 140/140 + FIXPOINT . parser_diff 13/13 . snapshot 50/50 . check_genis 133/133.
  SABOTAJ 3/3: S136 (checker RT007 dali) -> 172/173 rc=2; S137 (cevrim bayragini hep 0 yap) -> 172/173 rc=2 — POZITIF yolu olcen sabotaj budur, `iyi()` yanlislikla RT007 alinca kapi kirmizi oluyor; S138 (codegen dali) -> 139/140 rc=2.
  FIKSTUR tc46_03_realtime_asm.kem uc sekil tasir: kotu() (cevrim YOK -> RT007), iyi() (cevrim: 24 -> TEMIZ, POZITIF), normal() (gerceklzamanli DEGIL, anotasyonsuz -> TEMIZ). Pozitifler olmasa "her asm'i reddet" sabotaji GECERDI (D-425).
- 2026-09-04 D-554: CI 4 AYDIR HIC CALISMAMIS — tek tirnaksiz ':' yuzunden. Yama yapildi, CI ILK KEZ gercekten kostu.
  OLCUM (public API, kimlik dogrulamasiz): 13 Mayis 2026'dan bu yana ornekleme 399 kosum, 399 basarisiz, 0 basarili. 100/100 kosum created_at == updated_at ve 0 is -> STARTUP FAILURE, yani isler HIC BASLAMIYOR. "CI kirmizi" degil, "CI hic kosmamis".
  KOK: `- name: apt: clang + gcc + make` — tirnaksiz YAML skalerinde ": " ic ice esleme baslatir, dosya ayristirilamaz, GitHub workflow'u hic calistirmaz. Yerel dogrulama: yaml.safe_load -> "mapping values are not allowed here, line 74, column 18".
  BU D-548'IN OLCUMUNU DUZELTIR: orada "CI VAR ve claude/** push'unda test_tumu kosuyor" demistim; KOSMUYORDU. ci.yml'in var olmasi calistigi anlamina gelmiyordu (D-446'nin CI karsiligi: dosyada duran kapi, kosan kapi degildir).
  ILK GERCEK KOSUM (a0e431e): Linux -> checkout ok, apt ok, `make` DERLEME OK, `make test_tumu` BASARISIZ. Windows -> MSYS2 ok, "Derleyici kontrolu" BASARISIZ (derleme hic denenmedi). Linux'ta derleyicinin CI'da KURULMASI yeni ve olumlu bir bilgi.
  HIPOTEZLER (loglar 403, kimlik dogrulamasi gerekiyor): Windows'ta is `msystem: UCRT64` kabugunda kosuyor ama `clang` Clang64 paketinden geliyor ve o dizin PATH'te degil (CLAUDE.md tam bunu sart kosuyor); Linux'ta ld.lld ve qemu ubuntu taban imajinda yok (D-551) + D-486 bircok "atla"yi sert hataya cevirdi. IKISI DE HIPOTEZ, log okunmadan dogrulanmadi.
  SIRADAKI: `gh auth login` (ya da GH_TOKEN) olmadan is loglari okunamiyor. Loglar okununca iki hipotez dogrulanip onarilmali.
- 2026-09-04 D-555->D-559: CI kampanyasi — "hic kosmuyor"dan "Linux tam yesil"e. Her adimda hipotez kuruldu, LOGLA dogrulandi, sonra onarildi.
  D-555: 18 test `opt -passes=verify`de dustu; opt CI'da YOK. Test 2>/dev/null ile "command not found"u yutup "IR reddedildi" diyordu — EKSIK ARAC, DERLEYICI KUSURU gibi raporlaniyordu. llvm kuruldu + eksik arac artik ayri ORTAM HATASI banneriyle bildiriliyor.
  D-556: ld.lld yok -> calistir_uart_merhaba_bare_metal Error 127. lld kuruldu.
  D-557: QEMU yokken bes temsilcinin BESI de atlandi ama ozet "5/5 gecti" diyordu (D-486 kapsam yanilsamasi). Ozet artik atlamayi soyluyor; QEMU VARKEN ciktinin degismedigi yerel olarak olculdu.
  D-558: Windows `clang: command not found` — is UCRT64 kabugunda, clang Clang64'te. MSYS2_PATH_TYPE=inherit + PATH on-eki.
  D-559: Windows Error -1073741515 = 0xC0000135 STATUS_DLL_NOT_FOUND (ASan runtime DLL). PATH on-eki HER Windows adimina (kabuk her adimda taze).
  SONUC: Linux isi "Tum testler gecti!" — tam takim CI'da yesil (llvm_test 286/286, checker_diff 173/173, bare-metal hedefleri kosuyor).
  D-551'IN CIKARIMI KISMEN YANLISTI: "ld.lld ve qemu yok -> o kapilar atlanir" demistim; ld.lld ATLANMADI, SERT HATA verdi (D-486 atlamalari sert hataya cevirmisti) ve ilk engel hic tahmin etmedigim `opt`tu. Cikarim olcumun yerine gecmez.
  ACIK: Windows codegen_genis 3/70 — 67 programda cikis kodu AYNI, "STDOUT farkli". ILK HIPOTEZIM (CRLF) YANLIS OLURDU: harness farki basiyor (diff ... | head -6) ama logda TEK BIR FARK SATIRI YOK. diff -q eksik dosyada da sifir-disi doner, ayrintili diff'in "No such file" hatasi 2>/dev/null ile yutulur. Yani kanit "icerik farkli" degil "cikti dosyalarindan biri hic olusmamis". Satir sonlarini onarsaydim hicbir sey degismezdi (D-421).
  SIRADAKI: harness eksik dosyayi icerik farkindan AYIRMALI (bugun ikisi ayni mesaji veriyor), sonra Windows'ta .out'un neden yazilmadigi olculmeli.
- 2026-09-04 D-560->D-566: CI TAM YESIL — Linux VE Windows. D-554 CI'yi ilk kez calistirmisti; bu seri Windows'u da yesile cekti.
  D-560: 67 program "STDOUT farkli" diyordu ama diff TEK SATIR basmiyordu. Kok: diff -q EKSIK DOSYADA da sifir-disi doner, ayrintili diff'in hatasi 2>/dev/null ile yutulur. Uc durum ayri mesaja bolundu.
  D-561: `[ -f ]`=VAR ama /usr/bin/diff "No such file". Kok: recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari AYRI /tmp baglamalari cozuyor. Gecici dizin depo-goreli yapildi.
  D-562: bir sonraki kapi AYNI IMZAYLA dustu -> kok tek harness'ta degil PAYLASILAN KALIPTA. 27 harness'in hepsi cevrildi.
  D-563: /usr/bin/diff: /dev/fd/63 — SUREC IKAMESI `<(...)` MSYS2 araclariyla calismaz. Gercek dosyaya yazildi. Kapsam olculdu: `done < <(find ...)` bicimi SORUN DEGIL (okuyucu bash'in kendisi).
  D-565: exit=127 ama ham kosum "PANIK: sifira bolme" basiyor -> program CALISIYOR. Kok: 134 = 128+SIGABRT POSIX'e ozgu; Windows'ta abort() 127 doner (trivial `int main(){abort();}` ile gcc ve clang'da olculdu). ABORT_RC platforma bagli yapildi; ASIL IDDIA olan MESAJ her yerde denetleniyor.
  D-566: zirve RSS 5544 KB > esik 4096 -> esik LINUX olcumunden turetilmisti. Platforma bagli (Win 12288); ayirt edicilik korunur (gerileme ~19968 KB). Ayrica ayni dosyada BACKTICK komut ikamesi kusuru (dorduncu kez, D-456 sinifi).
  ORTAK SINIF: POSIX VARSAYIMLARI. Altisinin hicbiri derleyici kusuru degildi — hepsi KAPILARIN tasinabilirlik varsayimiydi (/tmp baglamasi, /dev/fd, sinyal-tabanli cikis kodu, RSS muhasebesi). D-477/D-481 ailesi.
  YANLIS GIDEN: D-564 GECERSIZ BIR HIPOTEZDI. Israrli 127'yi D-413'un Defender yarisina yorup 12 yeniden-deneme ekledim; COZMEDI. Gercek koku tani ciktisi gosterdi. Bir hipotezi "makul" diye uygulamak olcmenin yerine gecmiyor.
  IKI KEZ BAYAT ARTEFAKT: Windows yerelde codegen_diff 166/169 ve bolge_operand 14 sapma gorundu; ikisinde de build/codegen.exe 7 gun eskiydi. Taze kurulunca temiz — "Windows ayrismasi" diye kaydedilmedi.
  YEREL YESIL != CI YESIL: yerel Windows takimi "Tum testler gecti!" dedi ama perf_bellek orada ATLANMISTI (/usr/bin/time yok). Yerel yesil o kapi hakkinda hicbir sey soylemiyordu.
  SONUC: main uzerinde Linux success + Windows success (run 33866729367).
- 2026-09-04 D-567 (NEGATIF SONUC): `metin` UTF-8 ekseni tarandi. BELLEK GUVENLIGI TUTUYOR; D-458'in "gecerli-UTF-8 degismezi" iddiasi FAZLAYDI. KOD DEGISIKLIGI YOK.
  NEDEN BU EKSEN: Sirada bosalmisti; LOOP kurali geregi en muhafazakar ve olculebilir isi kendim sectim. `metin` dilin her yerinde ve D-458 onun icin acikca bir degismez ILAN etmisti — yani sinanabilir bir iddia vardi.
  BELLEK GUVENLIGI, 8/8 TUTTU (ASan her kosumda temiz): kes(s,100,5)->"" (baslangic kirpiliyor), kes(s,-5,2)->"ab" (negatif 0'a), kes(s,1,999)->"bc" (uzunluk kirpiliyor), metin_bayt(s,100)->0 (GERCEK koruma, kaynak okundu: `i<0` ve `i>=n`), kod_gecerli 0 ve 0x110000'i reddediyor, 'A'yi kabul. Tasma yok, sinir ihlali yok, cokme yok.
  AMA UTF-8 GECERLILIGI DEGISMEZ DEGIL: metin_uzunluk("çığ")=6 (BAYT), metin_kes("çığ",0,1)=1 bayt (0xC3 = ç'nin YARISI), yazdir_metin o yarimi HAM basiyor (exit 42, sikayet yok), metin_kucuk_tr yarimi cokmeden isliyor, iki yarim birlestirilince "ç" geri geliyor. Yani `metin` NUL-sonlandirmali BAYT dizisidir; UTF-8 bir SOZLESMEDIR, korunan bir degismez degil.
  BAYT INDEKSLEME BILINCLIDIR: D-458'in KENDISI metin_kes("\n",1,1) ile bayt diliminden yararlaniyor; D-461'in regex'i kod-noktasi duzeyini KENDI kuruyor.
  DUZELTILEN IDDIA: D-458 ham-bayt primitifini "gecersiz dizgi kurmaya izin verirdi" diye reddetmisti; gerekce eksikti cunku gecersiz dizgi metin_kes ile ZATEN kurulabiliyor. kod_metin'in 0'i reddetmesi UTF-8 icin degil NUL-SONLANDIRMA icindir — o kisim dogru.
  KOD DEGISIKLIGI YAPILMADI: bayt indeksleme kaldirilamaz (mevcut kullanimlar ona dayaniyor) ve dogrulama eklemek D-515 sinifina girerdi (olculebilir kusuru olmayan yere yeni kod). Degisen tek sey kaydin gercege uydurulmasi (D-406).
  YANLIS GIDEN: iki probe kendi hatamdi — `yazdir_satir("")` arguman almiyor (T010); ve metin_uzunluk'u karakter sanip "çığ" icin 3 beklemistim, olcum 6 dedi. Beklentiyi olcumden ONCE yazmak yaniltiyor (D-500).
- 2026-09-04 D-568 (ACIK — OLCULDU): `Kilit`in "yapisal olarak imkansiz" iddiasi TUTMUYOR. KOD DEGISIKLIGI YOK (secim dil yuzeyi karari).
  YONTEM: D-567'nin dersi uygulandi — once dilin ILAN ETTIGI iddia arandi. stdlib/kilit.kem:4 "KAPSAMLI (scoped) API — ham al/birak cifti BILEREK disa verilmez"; D-455 bunu "unutulmus-birak = deadlock, cift-birak = UB" sekillerini YAPISAL OLARAK IMKANSIZ kilmakla gerekcelendiriyor.
  OLCUM: kilit_al ve kilit_birak YERLESIKTIR (yerlesik_ekle listesinde), yani stdlib/kilit.kem hic kullanilmadan cagrilabilir. Koruma KUTUPHANE duzeyinde, DIL duzeyinde DEGIL.
  DORT SEKIL, DORDU DE --check'ten TEK TANI ALMADAN gecti: (k1) al var birak yok -> exit 42; (k2) cift birak -> exit 42; (k3) kilit_yok SONRASI al -> exit 42; (k4) ayni kilidi iki kez al -> exit 124 ASILDI = DEADLOCK. k4 tam olarak onlendigi soylenen sey.
  k3 AYRICA TUTARSIZLIK: iptal edilmis YETKI ile kullanim D-503'te CP005 aliyor; iptal edilmis KILIT ile kullanim hicbir sey almiyor. Ayni "olu handle" sinifi, iki farkli muamele. Kilit'in lineer OLMAMASI D-455'te bilincliydi ama kilit_yok sonrasini korumasiz birakiyor.
  AYNI YUZEY Semafor ve Bariyer icin de acik (semafor_al, semafor_birak, bariyer_bekle de yerlesik).
  UC SECENEK, secim MEHMET'IN: (a) yerlesikleri gizle — stdlib'in kendisi onlara dayaniyor, ayrica onceden gecerli programlari reddeder; (b) checker'a kilit disiplini ekle — MAKINE ZATEN VAR (dal-duyarli lineer tuketim, D-311/D-312; L005 tam bu sekli taniyor) ama bu bir LIVENESS kuralidir ve depo D-296'da "safety korunuyor liveness kayboluyor"u kabul edilemez saymisti; (c) iddiayi gercege uydur. En ucuzu (c), en degerlisi (b).
  YANLIS GIDEN: ilk turda k1/k2 exit=1 gosterdi, bir an gercek kusur sandim; ASan'siz kosunca 42 cikti. Sebep LeakSanitizer'di — fiksturlerimde kilit_yok yoktu, 48 baytlik kilit siziyordu. Kusuru fiksturun kendi eksiginden ayir.
- 2026-09-04 D-569: KILIT DISIPLINI checker'a eklendi (Mehmet karari (b)). D-568'in olctugu dort acik da derleme zamaninda yakalaniyor. YENI TANI KODU YOK.
  DURUM MAKINESI (baglama basina): SERBEST --al--> TUTULU --birak--> SERBEST; herhangi --yok--> OLU. Cikista TUTULU -> L001; SERBEST iken birak -> L002; TUTULU iken al -> L002 (self-deadlock); OLU iken kullanim -> CP005 (D-503 ile ayni sinif). Kodlarin anlamlari birebir ortustugu icin yeniden kullanildi (D-503/D-505 deseni).
  KAPSAM YALNIZ kilit_* VE BU OLCULDU: kdl_semafor_al bir SAYAC azaltir (counting semaphore), coklu `al` MESRUDUR, 1:1 eslesme kurali orada YANLIS olurdu. bariyer_bekle'nin esi yok. Uc aileye birden uygulamak kolay ve yanlis olurdu.
  RAPORLAMADA DEFAULT-DENY: baglama izlenemez hale gelirse (kilit_* disi cagriya arguman, kosullu/dongu/esles govdesi, yeniden atama) izleme BIRAKILIR ve o baglama icin HICBIR tani uretilmez. Fiksturdeki `kosullu` sekli bunu POZITIF olarak olcer.
  YANLIS-POZITIF TARAMASI: 700+ .kem; L001/L002/CP005 veren 29 dosyanin HICBIRI kilit_ icermiyor (hepsi onceden var olan lineer/yetki tanilari). Gercek kilit kullanicilari (stdlib/kilit.kem, selfhost/checker.kem, selfhost/codegen.kem) TEMIZ — dogru kullanim cezalandirilmiyor.
  UC UYGULAMAYA DA PORTLANDI (D-517 dersi): C + checker.kem + codegen.kem, tani kodu ve konum birebir.
  HANGI KAPI NEYI OLCTU: checker_diff 174/174 (0 muaf) . self_driver FIXPOINT . check_kapisi 274/281 (0 RED) . check_genis 133/133 . stdlib_check tum testler . sifir uyari 38/0. Sabotaj 2/2: S139 (C pasi) -> C "OK" dondu, 173/174 rc=2; S140 (self pasi) -> 173/174 rc=2.
  YANLIS GIDEN: pasi tek bir dev heredoc ile yazmaya calistim, kabuk "unexpected EOF" verdi ve dosya HIC OLUSMADI. KEMGU pasi iki metin dosyasina bolunup kucuk bir enjektorle yerlestirildi. Uzun heredoc bu oturumda besinci kez arac hatasi uretti.
- 2026-09-04 D-570: DOSYA HANDLE DISIPLINI — `tekkez` korumasi ham yerlesiklerle baypas ediliyordu. D-569'un makinesi genisletildi; YENI TANI KODU YOK.
  MADDENIN SORUSU: `tekkez` olmak gercekten koruma sagliyor mu? ONCE POZITIF TARAF OLCULDU — EVET: tc24_* fiksturleri hala tutuyor (L001 sizinti, L002 cift kapat, temiz pozitif). D-452/D-466'nin mekanizmasi gercek.
  AMA D-568'IN AYNISI: dosya_ac / dosya_kapat / dosya_oku / dosya_yaz YERLESIKTIR ve ham `metin` handle dondurur; `Dosya` sarmalayicisi kullanilmazsa lineer koruma HIC uygulanmaz. Olculdu: hic kapatmama, iki kez kapatma, kapattiktan sonra yazma -> UCU DE `OK`.
  ONARIM: D-569'un durum makinesine `aile` alani eklendi (0=kilit 1=dosya). Cikista ACIK -> L001; kapaliyi tekrar kapat -> L002; kapali handle kullanimi -> CP005.
  ILK SURUM SAHTE L001 URETTI ve onu KAPI DEGIL DEPO TARAMASI yakaladi: stdlib/dosya.kem kendi sarmalayicisinda `ver tamam(Dosya { h: h })` yapiyor; C'nin kd_kacti'si YALNIZ TANIMLAYICI ve CAGRI'ya iniyordu, YAPI_OLUSTUR'u hic gormuyordu -> baglama "hala acik" sanildi. Self-host surumum genel cocuk dongusu kullandigi icin IKISI AYRISIYORDU.
  ONARIM IKINCI GEZGIN YAZMAK DEGIL KURALI DARALTMAK OLDU (D-407): C'de genel cocuk yineleyici YOK ve yazmak ayni soruyu iki yerde yanitlamak olurdu. kd_yuru artik yalniz MODELLEDIGI dugumleri isler (BLOK, DEGISKEN, IFADE_DEYIMI, CAGRI, VER, zararsiz yapraklar, ciplak TANIMLAYICI) ve MODELLENMEYEN her dugumde TUM izlemeyi birakir. Sonuc: stdlib/dosya.kem TEMIZ, uc tehlike hala yakalaniyor.
  YANLIS-POZITIF: dosya_ac iceren 8 dosya; stdlib/dosya.kem, selfhost/*, test_dosya, 07_linear, dosya_io TEMIZ. Kalan tanilar yalniz tc22_01/tc22_02'de ve OZGUN olanlar (L001 0 0, L002 14 39); benim ekledigim sahteler (L001 7 1, L001 5 1) daraltmadan sonra kayboldu.
  HANGI KAPI NEYI OLCTU: checker_diff 175/175 (0 muaf) . self_driver FIXPOINT . check_kapisi 274/281 (0 RED) . check_genis 133/133 . stdlib_check . sifir uyari 38/0. Sabotaj S141 (C dosya ailesini kapat) -> 174/175, rc=2.
  KAPSAM DISI (durustce): soket_* ailesi portlanmadi — probe'um T010 verdi (soket_al arite farkli) ve dogru sekli olcmeden kural yazmak D-421 sinifidir. Ayri is olarak Sirada'da.
- 2026-09-06 D-571: soket handle disiplini (ucuncu aile) — ham `soket_baglan`/`soket_kapat` yerlesikleri `yapi tekkez Baglanti` korumasini baypas ediyordu; uc tehlike de tanisiz `OK` idi. Yeni tani kodu YOK (L001/L002/CP005). Uc uygulamaya da portlandi. IKI kusur cikti: (1) `kd_kacti` `YAPI_OLUSTUR`a inmiyor -> POZITIF fikstur tc24_03 sahte L001 aldi (kural daraltildi: modellenmeyen sekil = kacti); (2) self-host yedek dali `kd_hepsini_birak` ile TUMUNU birakiyordu, C ise yalniz adi geceni -> araya giren `geri_al(y)` soket izlemesini dusuruyordu ve self-host SESSIZ kaliyordu (`kd_kacti_birak` eklendi). Kapilar: checker_diff 176/176 (0 muaf), self_driver FIXPOINT, check_kapisi 274/281 (0 RED), check_genis 133/133, stdlib_check, sifir uyari 38/0. Sabotaj 3/3: S142 (C) 175/176 rc=2, S143 (checker.kem) 175/176 rc=2, S144 (codegen.kem) self_driver 142/143 rc=2. SUREC: python evrensel satir-sonu okumasi CRLF olan src/tip_kontrol.c'yi LF'e cevirip 14.180 satirlik sahte diff uretti; ikili modda yeniden uygulandi.
- 2026-09-06 D-572: cesit payload'inda Dizi — C yigin dizisini cesit yuvasina koyup HEAP erisimcisiyle okuyordu (dizi_boyut 1/39 yerine 2, dizi_al SEGV; hic guvensiz blok yok). Self-host heap-uniform oldugu icin ZATEN dogruydu (parite TERS yonde, D-442 sinifi). Dar onarim: cesit yapicisi payload'in BILDIRILEN tipini beklenen tip olarak gecirir (yalniz Dizi<T>). AYIRT EDICI: payload OKUNMALI — D-437'nin bagla-ama-kullanma probe'u bu sinifi goremiyordu. Sabotaj S145 ilk olcumde 3 kosumun 1'inde yakalandi: harness'in 'oracle 127 = daima ortamsal' premisi (D-339 bunu ADAY tarafinda curutmustu, ORACLE tarafi kalmis; D-565: Windows'ta abort 127) gercek SEGV'i yutuyordu. Kural simetriklestirildi (12 tekrar + taze kopya sonrasi 127 = sert hata) -> sabotaj 3/3, temiz kosum 2/2 yesil. Kapilar: codegen_diff 170/170, yapi_diff 150/150, codegen_genis 70/70, modul_codegen 22/22, ct_bariyer 14/14, bolge_operand 172/172, llvm_test 286/286, snapshot 50/50, self_driver FIXPOINT, sifir uyari 38/0.
- 2026-09-06 D-573: LR002'nin DIZI yarisi hic atesleniyordu degil — `Dizi<tekkez<T>>` dort konumda da (baglama/parametre/donus/yapi alani) sessizce kabul ediliyor ve AYNI lineer deger diziden gecirilerek IKI KEZ tuketilebiliyordu (--check temiz, exit 42). Tek site: ast_tip_to_bilgi'nin TIP_DIZI dali. YAYILIM (D-467) burada YANLIS olurdu (cok-elemanli kapsayicida sahiplik tek tuketimle temsil edilemez) -> YASAK. Uc uygulamaya da portlandi, birebir. Yol ustunde `esles` baglama omrunun UC sekli olculdu ve TUTTU (skrutini yeniden atama / baglama kacisi / gecici skrutini; ASan temiz). Lineer payload'li `cesit` acigi olculdu ama BILINCLI olarak kapatilmadi (dil yuzeyi karari, Siradaya yazildi). Kapilar: checker_diff 177/177, self_driver FIXPOINT 170/170, check_kapisi 275/282 (0 RED), check_genis 133/133, linear_test 89/89, capability_test 40/40, sifir uyari 38/0. Sabotaj 3/3: S146/S147 176/177 rc=2, S148 self_driver 143/144 rc=2.
- 2026-09-06 D-574: lineer payload'li `cesit` — YASAK degil YAYILIM secildi, ve secim OLCUMLE yapildi. Belirleyici olcum: `esles` lineer cesidi TUKETIYOR (pozitif sekil TEMIZ gecti); tuketmeseydi yayilim her dogru programi L001'e bogar ve yasak tek secenek kalirdi. Mekanizma D-313'un TIP_YAPI.lineer_mi bayragi (yeni makine YOK, yeni tani kodu YOK), tek site yapi_tipi_sembolden. D-573'un tersi yonde ama tutarsiz DEGIL: dizi cok-elemanli (sahiplik tek tuketimle temsil edilemez) -> yasak; cesit tek deger -> yayilim. Etki alani once/sonra karsilastirmasiyla olculdu: 700+ dosya, 35 tanili dosya, FARK YOK. Uc uygulamaya portlandi. Kapilar: checker_diff 178/178 (0 muaf), self_driver FIXPOINT (145/145 + 170/170), check_kapisi 275/282 (0 RED), check_genis 133/133, linear_test 89/89, capability_test 40/40, sifir uyari 38/0. Sabotaj 3/3: S149/S150 177/178 rc=2, S151 self_driver 144/145 rc=2. SUREC: ayni yamayi iki self-host dosyasina kopyaladim, codegen.kem'de degisken adi farkliydi -> T002; 'ayni kod iki dosyada' varsayimi yine yanlis.
- 2026-09-06 D-575: gecisli R-YAKALAMA-THREAD — yaris adlandirilmis bir kapanisin ARDINA saklaniyordu. D-505 yalniz DOGRUDAN lambda argumanini goruyordu; `deger f = || { dizi_yaz(d,..) }` + iki `gorev_baslat(|| { ver f(); })` seklinde --check TEMIZ ve guvensiz YOK iken 5 kosum 120/12/81/205/41 verdi (beklenen 160) = D-504'un yarisi. Yeni tani kodu YOK (L002), yeni analiz YOK: 'bu lambda tasima gerektiren bir sey yakaliyor mu' sorusu baglamada kalicilastirildi (Sembol.kapanis_tasima) ve kayit gecisli yapildi. Self-host'ta gtas_topla YENIDEN KULLANILDI (ikinci gezgin yazilmadi, D-407); `dizi_cikar` dilde olmadigi icin anlik goruntu KOPYAYLA geri yuklenir. Uc kenar zaten tutuyordu (ic ice yapi, Dizi<Dizi>, kuresel=P034 yapisal imkansiz). Iki POZITIF sekil fiksturde (saf kapanis + skaler yakalayan kapanis). Depo taramasi: sifir yeni yanlis-pozitif. Kapilar: checker_diff 179/179 (0 muaf), self_driver FIXPOINT (146/146 + 170/170), check_kapisi 275/282 (0 RED), check_genis 133/133, drf_test 54/54, linear_test 89/89, drf_gorunurluk 100/100, sifir uyari 38/0. Sabotaj 3/3: S152/S153 178/179 rc=2, S154 self_driver 145/146 rc=2.
- 2026-09-06 D-576: D-575'e yazdigim 'kalan: tek seviye' notu OLCULDU ve YANLIS cikti (iki/uc seviye zincir zaten yakalaniyor). Ama sinir aranirken GERCEK acik bulundu: kapanis bir YAPI ALANINDA saklaninca (`Kap { f: || { dizi_yaz(d,..) } }`) kural hic sorulmuyordu -> 5 kosum 29/242/138/160/70 (beklenen 160), --check TEMIZ, guvensiz YOK. Kural 'deger bir LAMBDA'dir'dan 'deger tasima-gerektiren bir kapanis ICERIR'e genellestirildi (C'de lambda sonucu birikiyor + DEGISKEN degeri boyunca izole; self'te alt-agac gezgini). Yol ustunde parite ayrismasi: `k.f()` C'de IKI L002 veriyordu (k ERISIM+CAGRI icin iki kez ziyaret ediliyor, self bir kez) -> AYNI satir/sutunun tekrari bastirildi; FARKLI konumdaki iki anma hala iki tani veriyor (r3 probe'u ile olculdu). Kapilar: checker_diff 179/179 (0 muaf), self_driver FIXPOINT (146/146 + 170/170), check_kapisi 275/282 (0 RED), check_genis 133/133, drf_test 54/54, linear_test 89/89, capability_test 40/40, sifir uyari 38/0; depo taramasi sifir yeni yanlis-pozitif. Sabotaj 4/4: S155/S156/S157 178/179 rc=2, S158 self_driver 145/146 rc=2.
- 2026-09-06 D-577 (NEGATIF SONUC): `dondur` (R-PAYLAS) atil — spec aksiyomunun IKI yarisi da uygulanmiyor (paylasim hakki VERILMIYOR: iki gorev dondurulmus diziyi okuyunca yine L002; yazma yasagi UYGULANMIYOR: calisma zamaninda 'donmus' dizi gercekten degisti, exit 99). Bugunku hal GUVENSIZ DEGIL cunku hicbir hak verilmiyor. KOD YAZILMADI: yasagi tek basina uygulamak olculebilir kazanc vermez ve gecerli programlari reddeder; hakki vermek yasagi GEREKTIRIR -> ikisi birlikte dil yuzeyi karari. Bagimlilik taramasi: `dondur` yalniz 2 fiksturde. Bunun yerine BAGLASIM kapiya baglandi (tc51_01): birisi paylasim hakkini yasak olmadan acarsa fikstur kirmiziya doner. Sabotaj S159 -> fikstur L002->OK, checker_diff 176/180 rc=2. Kapilar: checker_diff 180/180 (0 muaf), check_kapisi 275/282 (0 RED), drf_test 54/54.
- 2026-09-09 D-578: yeni eksen = tahsis boyutu aritmetigi. Bes sekil olculdu, dordu tuttu (negatif/sifir N zararsiz, yanlis kullanim LOUD panik, argumanin yan etkisi iki derleyicide de korunuyor, metin_kes kirpiyor). Besincisi gercek bulgu: `dizi_kapasite` ve `dizi_kapasite_ayarla` self-host'ta KULLANICI CAGRISI olarak dispatch koluna sahip degildi -> tanimsiz sembol, LINK-RED; yerlesikler biliniyor ve declare ediliyordu, eksik olan yalniz cagri yeri esdegeriydi. OLCUM ARACI IKI KEZ YANILDI: (1) kirpilmis grep yuzunden 'argüman yok sayiliyor' diyecektim — gercek sozlesme N=KAPASITE REZERVASYONU, boyut 0; (2) '2e9 rezervasyon basarisiz olur' premisi yanlisti, bu platformda ~8 GB LAZY olarak BASARIYOR, sabotaj sessiz kaldi ve premis curudu (o madde fiksturden cikarildi). Kod okumasiyla ayri bir acik bulundu ve Siradaya yazildi (kdl_dizi_buyut tahsis-basarisizligini denetlemiyor; ulasilabilirlik OLCULMEDI). Kapilar: codegen_diff 171/171, yapi_diff 151/151, self_driver FIXPOINT (147/147 + 171/171), sifir uyari 38/0. Sabotaj 2/2: S162/S163 -> 170/171 rc=2.
- 2026-09-09 D-579: D-578'in Siradaki maddesi — `kdl_dizi_buyut` tahsis basarisizligi. ULASILABILIRLIK OLCULDU ve ~1 GB veri GEREKMEDEN ulasilabilir cikti: KdlDizi'yi dogrudan kurup (boyut==kapasite==2^30) kdl_dizi_ekle_tam cagirmak ANINDA SEGV veriyor. Iki kusur bir arada: `yk = kapasite*2` int32'de tasiyor, ve tahsis basarisizliginda `d->veri=NULL` yazilip kapasite yine de guncelleniyor -> cagiran NULL'a yaziyor, eski veri kayboluyor. SESSIZ DUSUS dogru degildi (cagiran hemen yazar -> eski tamponun sonuna yazma = heap tasmasi); tek tutarli cevap TEMIZ PANIK, D-069/D-502/D-514/D-546 ile ayni politika. IKI KORUMA BIRBIRINI YEDEKLIYOR: S164 ve S165 tek tek SESSIZ kaldi, S166 (ikisi birden) SEGFAULT'u yakaladi rc=2 -> fikstur cifti olcuyor; tasma korumasi D-510 disiplini geregi korundu. Kapi calistir_dizi_sinir_test'e eklendi (34->39 olcum): vaka11 negatif + vaka12 POZITIF (1000 ekleme panik uretmemeli). Kapilar: dizi_sinir 39/39, panik_test 6/6, runtime_link 33/33, kdl_bolge 6/6, dizi_perf 6/6, gorev_rt 16/16, llvm_test 286/286, codegen_diff 171/171, sifir uyari 38/0. SUREC: heredoc 
'i gercek satir sonuna cevirip probe'u kirdi (ucuncu tekrar).
- 2026-09-12 D-580 (MEHMET KARARI): P046 kaldirildi + yukleyicide `::`->`/` -> D-520 ASAMALI GOCE ACILDI. Karar oncesi maliyet yeniden olculdu ve kayittakinden FARKLI cikti: legacy altinda NITELIKLI ad T016 aliyor, cok-segment alias/secili P046 aliyor -> 'once nitelendir sonra cevir' MUMKUN DEGILDI, D-520 bir flag-day olurdu. Gercek kilit P046'ydi. Olculen sonuc: cok-segment alias/secili + genel uye OK, + private uye T041 (kacak kapaniyor), legacy ciplak OK (bozulmadi). Self-host portu GEREKMEDI (modul_path zaten `::`->`/` yapiyor, P046 self-host'ta hic yoktu). Iki ONCEDEN VAR OLAN bosluk olculdu ve Siradaya yazildi: (1) self-host alias/secili yolunda T041 hic uygulanmiyor — tek segmentte de ayni, GOCTEN ONCE kapanmali; (2) alias/secili + sabit codegen'de yok. SUREC: var olan bir fiksturu (D-533) ezdim ve parite kapilari GORMEDI — iki tarafi birden bozan degisiklik sifir-diff kalir; git status'ta `??` yerine `M` gormek ele verdi. Kapilar: checker_diff 181/181 (0 muaf), modul_codegen 23/23 (0 atlandi 0 muaf), self_driver FIXPOINT (147/147 + 171/171), parser_diff 13/13, surucu_diff 13/13, check_kapisi 276/283 (0 RED), check_genis 133/133, codegen_diff 171/171, sifir uyari 38/0. Sabotaj 2/2: S167 (yol cevirisi) fikstur T040 rc=2, S168 (P046 geri) fikstur P046 rc=2.
- 2026-09-12 D-581 (D-580 ADIM 2): self-host alias/secili ithalatta T041 HIC uygulanmiyordu — gocun ON KOSULU, kapanmadan goc edilseydi goc eden her dosya self-host'ta gizlilik denetimini KAYBEDERDI. UC kok: (1) alias yeni-bicim sayilmiyor (kullan_yeni_bicim_mi'de alias dali yok; D-533 onu 'P046 yuzunden ulasilamaz' diye yazmamisti, D-580 ulasilabilir kildi); (2) alias gercek modul adina cozulmuyor (priv_mod son segmenti tutar, erisimde alias var) -> alias_modul_coz + al_ad/al_yol kanali; (3) secili import hic YOL dugumu uretmez -> denetim ITHALAT YERINDE, ayrica ozel ad artik global'e EKLENMEZ (C de eklemiyor; kullanimi T002 aliyor). Secim kendi `kullan`ina si_sat/si_sut ile baglandi — yol esitligi yetmiyordu (ayni modulu hem alias hem secili ithal eden dosyada alias satirinda da atesliyordu). IKI SABOTAJ SESSIZ KALDI ve ikisi de korpusu/kapiyi duzelttirdi: S169 (fikstur tek dosyada alias+secili tasidigi icin alias dali ayirt edilemiyordu -> fikstur IKIYE bolundu) ve S173 (modul_codegen yalniz `islev main()` iceren dosyalari olcuyor -> fiksturler sessizce atlaniyordu, main() eklendi; self_driver'in korpusu test/moduller'i kapsamiyor, codegen.kem'in bu kancasini goren TEK kapi modul_codegen). Depo taramasi: T041 ekseninde sifir sapma. Kapilar: checker_diff 183/183 (0 muaf), modul_codegen 25/25 (0 atlandi 0 muaf), self_driver FIXPOINT (147/147 + 171/171), check_kapisi 276/283 (0 RED), check_genis 133/133, surucu_diff 13/13, codegen_diff 171/171, parser_diff 13/13, sifir uyari 38/0. Sabotaj 5/5: S169/S170/S171/S172 checker_diff 182/183 rc=2, S173 modul_codegen 24/25 rc=2.
- 2026-09-12 D-582 (NEGATIF SONUC): D-580'in goc planindaki 3. adima baslandi, ilk dosya goc etti ve TUKETICISINI KIRDI -> plandaki 'teker teker, her commit yesil' cumlesi olcumle curudu. Kod YOK, kismi goc geri alindi. Once PROSEDUR olculdu ve UCUZ cikti: secili import adlari NITELIKSIZ getirir -> SIFIR referans duzenlemesi (status.kem: 6 sabite `genel` + isim listesi, referans dokunulmadi) => D-520'nin '~700 referansi nitelendir' maliyeti GECERSIZ. Ama 2x2 matris olculdu (minimal tekrar uretimle izole edildi): legacy->legacy OK, yeni->yeni OK, legacy->YENI T040, yeni->LEGACY T002 — IKI karisik hucre de KIRIK. Yani goc birimi DOSYA DEGIL BAGLANTILI KUME: drivers/virtio + tests/drivers/virtio = 10 dosya tek kume, test/crossfile = 3. 'Her adimda yesil' gecersiz degil, GRANULERLIGI degisti (ara durum yalniz calisma agacinda). Yol ustunde ikinci bulgu: self-host IKI kirik hucrede de ayrisiyor ve DAHA MUSAMAHAKAR (CHK=OK) -> yari goc etmis agac self-host'ta temiz gorunur; bu yuzden matris bugun kapiya BAGLANAMADI (fikstur checker_diff'i kirardi) ve hizalama kume gocunun ON KOSULU olarak Siradaya yazildi. Agac temiz birakildi (virtio_blk onceden var olan T011 ile).
- 2026-09-12 D-583: D-582 ADIM 4'e (kume gocu) baslandi; virtio kumesi (9 dosya goc + 1 olu ithalat) uygulandi, hepsi --checkdump OK ve once/sonra tabani SIFIR gerileme gosterdi (virtio_blk_init_test 11->6 tani IYILESTI). Ama surucu_diff kirmizi oldu ve IKI ayri engel olculdu. ENGEL 1 KAPANDI: C codegen secili importlu `sabit`i HIC gormuyordu (`kullan m::{SBT}` -> check OK, llvm 'tanimsiz tanimlayici'; islev yolu calisiyordu). Kok: modul_uyeleri_kayit yalniz ISLEV+MODUL kaydediyor, DUGUM_SABIT'i hic; legacy duzlestirme her seyi program uyesi yaptigi icin orada calisiyordu -> kusur YALNIZ yeni bicimde, yani tam da gocun hedef yolunda. Self-host ikisini de cozuyordu (parite TERS yonde, D-442). Onarim cagri yolunun AYNASI: sabit `<onek>.<ad>` ile kaydedilir, referans yerinde resolver'in yazdigi cozum_modul_onek ile once mangled ad aranir; duz ad fallback'i KORUNDU. Yeni tani kodu YOK, self-host portu gerekmedi. ENGEL 2 (ACIK, goc GERI ALINDI): self-host cok-segmentli yeni-bicimde SON SEGMENTLE mangle ediyor, C TAM YOLLA (minimal probe: C @"d583x::sbt.fn" vs SELF @sbt.fn) — onceden var, kapili korpusta o sekil olmadigi icin GORUNMUYORDU; goc onu korpusa sokunca surucu_diff YAPISAL kirmizi (8/16). Ucuncu olcum (Siradaya): alias ile NITELIKLI sabit erisimi self-host'ta SESSIZCE 0 okuyor (20+22 -> 22), C gurultulu reddediyor; fikstur o sekli BILEREK kullanmiyor (D-421). Fikstur ana_secili_sabit.kem + sbt_mod.kem, POZITIF yerel sabit iceriyor (yoksa 'her sabiti mangle et' sabotaji GECERDI). Kapilar: modul_codegen 26/26 (0 atlandi 0 muaf), checker_diff 185/185 (0 muaf), codegen_diff 171/171, surucu_diff 13/13, sifir uyari 38/0. Sabotaj 2/2: S174 (kayit dali) ve S175 (referans yerindeki mangled arama) -> ikisi de modul_codegen 25/26 rc=2.
- 2026-09-12 D-584: D-583'un ENGEL 2'si kapandi (kume gocunun ON KOSULU). C dosya-modulunun ADI TAM YOLDUR (`a::b`) ve DEGISTIRILEMEZ — tip_kontrol `kullan_isle` modul sembolunu tam yolla arar; ilk onarimim adi ana.c'de son segmente indirdi ve ANINDA T040 verdi (yanlis katman, ayni alani iki tuketici farkli amacla okuyor — D-439 sinifi). Dogru katman codegen: kirpma `modul_mangle`in ICINE kondu, boylece hem KAYIT hem HER ARAMA ayni yardimciyi cagirir ve tanim geregi hizali kalir (D-407); ic ice modul onekleri `.` ile kurulur ve `::` icermez -> dokunulmaz. Yeni tani kodu YOK. ASIL BULGU: S176 (kirpmayi kapat) modul_codegen'de SESSIZ kaldi (26/26 yesil) cunku iki derleyici de KENDI ICINDE tutarli — ayni exit, ayni stdout; davranissal kiyas MANGLE ADINA KORDUR (D-422/D-506 sinifi). Kapiya yapisal olcum eklendi (IR'lar zaten uretiliyordu, maliyet sifir). TAM AD KUMESI kullanilamadi: temiz agacta 9 dosyada kirmizi verdi ve sebep mangle degil D-401'in bilinen K4 siniriydi (self generic BASE govdeyi de yayar) -> onu buraya kopyalamak yapi_diff'in envanterini ikiye bolerdi; olculen sey MODUL ONEKI kumesi (ilk `.`ya kadar). Fikstur EKLENMEDI, gerekmedi: ana_ic_ice.kem (D-580) zaten cok-segmentli alias+secili tasiyor ve kapi sertlesince TAM DA O dosya kirmiziya dondu — eksik olan fikstur degil OLCUMDU. Kapilar: modul_codegen 26/26 (0 atlandi 0 muaf), checker_diff 185/185 (0 muaf), codegen_diff 171/171, yapi_diff 151/151 (22 muaf), surucu_diff 13/13, sifir uyari 38/0. Sabotaj S176: sertlestirmeden ONCE 26/26 rc=0 (sessiz), SONRA 25/26 rc=2.
- 2026-09-13 D-585 (D-582 ADIM 4): virtio kumesi TEK COMMIT'te goc etti (drivers/virtio + tests/drivers/virtio = 10 dosya, 34 ithalat, 1 olu ithalat dustu). Prosedur D-583'te olculmustu: secili import adlari NITELIKSIZ getirir -> SIFIR referans duzenlemesi, yalniz `genel` + ithalat listesi. D-520'nin '~700 referansi nitelendir' maliyeti nihai olarak GECERSIZ. OLCULEN KAZANC: private uye ithalati artik C=T041 SELF=T041 (goc oncesi OK idi), `genel` uye ikisinde de temiz. YOL USTUNDE SELF-HOST KUSURU: surucu_diff 9/16 verdi, sayilar esitken (77=77) ad kumeleri ayrisiyordu — self bazi modul uyelerini MANGLE ETMEDEN yayiyordu (virtqueue_bind: self 36 nitelikli ad, C 63). Kok: kullan_yeni_bicim_mi(p, yol) GIRIS dosyasinin si_yol/al_yol'una bakiyor; gecisli ithalatta o `kullan` dugumu MODULUN KENDI mp'sindedir -> yuklem yanlis 'legacy' diyor, modul sarmalanmiyor. Onarim: bicim kararini CAGIRAN verir (modul_src_bir'e yeni_bicim parametresi); her cagri yeri dugumu OKUYAN ayristiriciyla hesaplar. Kusur ONCEDEN VARDI ve gorunmuyordu (kapili hicbir dosyada gecisli cok-segmentli yeni-bicim ithalat yoktu — D-356 sinifi). MUAFIYET 5 -> 2 ve bunu KAPININ KENDISI bildirdi ('MUAF ama --check artik ESLESIYOR'): virtio_blk, virtio_blk_oku, virtio_blk_oku_test; legacy duzlestirme tum adlari gorunur kiliyordu ve iki uygulama farkli kabalikta muhafazakardi. Fikstur tc52_01_virtio_gizlilik.kem (negatif T041 + POZITIF genel uye). Depoda kalan ciplak cok-segment kullan: 4, dordu de KASITLI fikstur -> ADIM 5 artik yalniz o dort fiksturun gelecegi kararina bagli. Kapilar: surucu_diff 16/16 (2 muaf), checker_diff 186/186 (0 muaf), modul_codegen 26/26, codegen_diff 171/171, check_kapisi 276/283 (0 RED), check_genis 133/133, yapi_diff 151/151 (22 muaf), self_driver TUM MODLAR + FIXPOINT, sifir uyari 38/0.
- 2026-09-13 D-586: nitelikli/alias'li MODUL SABITI codegen'de cozulmuyordu — kayittakinden GENIS: yalniz alias (`sm::K`) degil DUZ NITELIKLI (`m::K`) de ayni. C gurultulu reddediyordu ('yol ifadesi desteklenmiyor'), self-host SESSIZCE 0 okuyordu (20+22 -> 22). Self koku bir COP KOLU: ifade_uret YOL dali yalniz cesit varyantini taniyor, geri kalani `ver "0"`a dusuyordu; sabit_topla da modul sabitlerini yalniz CIPLAK adla kaydediyordu. C koku: DUGUM_YOL D-583'un <onek>.<ad> sabit tablosunu hic sormuyordu. Onarim iki tarafta da NITELIKLI CAGRI YOLUNUN AYNASI: C cozum_modul_onek + yol_noktali_ad fallback; self sabit_topla nitelikli adi da kaydeder (ciplak korundu), YOL dali yol_noktali (alias cozer) ile arar, goreli yol once. Yeni tani kodu YOK. Fikstur ana_nitelikli_sabit.kem AYIRT EDICI (iki yol farkli deger 20/10, POZITIF yerel sabit 12). Cop kolunun kendisi HALA duruyor -> Siradaya (once ona dusen baska sekil olc). Siradaki bayat D-582 ADIM 3/4/5 tekrarlari temizlendi (D-585 onlari kapatmisti).
- 2026-09-13 D-587: self-host `ifade_uret` cop kollari (`ver "0"`) OLCULDU: gecici izle tum depo derlendi -> 6 isabet (5 `tamam(boş)` birim deger, 1 `g::Renk::Kirmizi` modul-nitelikli cesit varyanti DEGER konumunda). Varyant isabeti SESSIZ YANLIS CEVAPTI: ilk olmayan varyantla `g::Renk::Yesil` C=42 SELF=1; mevcut modul_cesit fiksturu TESADUFEN geciyordu (Kirmizi indeksi 0). Kok D-407'nin aynasi: D-407 cagri (yapici) yolunda sol TANIMLAYICI|YOL kabul ettirmisti, DEGER yolu yalniz TANIMLAYICI kabul ediyordu. `boş` kusur degil (birim deger) ama cop koluna tesadufen dusuyordu -> acik dal. Onarim sonrasi cop kollarina dusen sekil SIFIR. Kolu gurultulu yapmak self-host'ta olmayan bir hata kanalini ister -> Siradaya tasarim maddesi olarak yazildi. Fikstur cg_nitelikli_cesit_deger.kem (uc varyant, iki ilk-olmayan, farkli degerler).
- 2026-09-16 D-588: LOOP.md'nin "D-587 yan bulgu (TASARIM)" maddesi ele alindi ve IKI IDDIASI DA CURUDU. (1) "Self-host'ta hata bildirme mekanizmasi yok" YANLIS: yazdir_hata D-450'de eklenmis, codegen.kem onu kendi surucusunde (14070) ZATEN cagiriyor. (2) "Olculmus kusur yok" YANLIS: TANIMLAYICI olmayan bir ifadenin adresi (`&n.x` / `&xs[0]` / `&f()`) self-host'ta `store ptr 0` yayiyordu = GECERSIZ IR; C ucunu de 42 ile calistiriyor (parite TERS yonde, D-442 sinifi). Kok, sucladigim genel cop kolu DEGIL, `&` dalinin kendi dusus satiriydi. Onarim C'den okunan kalip: alloca <T> + store + adres. Yeni tani kodu YOK. Fikstur cg_adres_ifade.kem (uc farkli deger + pozitif &TANIMLAYICI). YANLIS GIDEN DENEMELER (alti arac hatasi, hepsi D-500 listesinde kayitli): (a) ilk probe'ta `clang && ./exe; rc=$?` -> && kisa devre yapinca clang'in rc'sini olctum, "C=1" sandim; (b) clang stderr'ini /dev/null'a yollayip tam da gereken kaniti bastirdim; (c) probe'lari kdl_runtime.o OLMADAN linkledim -> C tarafinda sahte "undefined symbol"; (d) hata satirini `cut -d: -f3` ile ayikladim, dogrusu -f2; (e) dogrulama komutunu `&&` ile zincirledim ve aradaki `grep -c` SIFIR eslesmede exit 1 dondugu icin ZINCIR KIRILDI -> yeniden kurulum hic kosmadi, ben de bayat .ll'den "rc=1" okuyup bir an kendi yamami sucladim; (f) sabotaj zincirindeki `grep -c D-588` kontrolu 0 bekliyordu ama S181 kodu silip YORUMU biraktigi icin 1 dondu -> o satir hicbir sey kanitlamiyor, sabotajin gercek kaniti fiksturun kirmiziya donmesi. Ilk yamam da YANLIS YERE gitti: `&` zaten daha ONCEKI bir dalda ele alindigi icin ekledigim kol ULASILAMAZ olu koddu; kaldirilip gercek dusus satirina tasindi.
- 2026-09-16 D-589: D-582 ADIM 5'in ON KOSULU tamamlandi; SILME YAPILMADI (dil yuzeyi karari -> Mehmet). Once olculdu: on kosulun gizlilik yarisi ZATEN VARDI (ana_ic_gizlilik + ana_ic_gizlilik_secili, D-581) -> madde bayatlamis; eksik olan yalniz crossfile yarisiydi. 5 yeni-bicim fikstur yazildi (lib_sayi_yeni/lib_islem_yeni/transitif_yeni + lib_sonuc_yeni/sonuc_cagri_yeni) ve test_llvm.c'ye 4 DAVRANISSAL olcum eklendi. KAPSAM HATAM: ilk taramam yalniz test/*.sh + Makefile'a bakip 'crossfile yalniz check_genis, yani sadece --check' dedi; .c dosyalari taranmamisti -> test_llvm.c legacy ikilisini exit 42 + opt verify ile ZATEN davranissal kapiliyormus. Yalniz check_genis'e fikstur eklemek DENK bir karsilik OLMAZDI (D-427'nin 'on kosul olcumunun kapsami da dogrulanmali' dersi). `genel` ZORUNLU oldugu olculdu (`dışa` -> T041), bu ayni zamanda fiksturlerin gercekten yeni-bicim yolundan gectiginin pozitif kaniti. YANLIS GIDEN DENEMELER: (a) parite probe'unda C'yi --checkdump, self'i BAYRAKSIZ cagirdim -> 5/5 'dump FARKLI' cikti ve bir an gerileme sandim; harness self'i --check ile cagiriyor (check_genis_harness.sh:110) - D-499'un tekrari, bes-uzerinden-bes sapmanin kendisi ipucuydu. (b) echo dizgilerinde backtick kullandim -> kabuk `disa`/`genel` komutlarini calistirmaya kalkti (D-548 tuzagi). (c) probe dosyalarini GATELI dizine (test/crossfile) yazdim; check_genis o dizini geziyor -> silindi, yoksa scratch dosyalar kapiya girecekti.
- 2026-09-17 D-590: legacy duzlestirme SILINDI (D-582 ADIM 5, Mehmet secenek a). C: ana.c kullan_yeni_bicim + llvm.c worklist dosya yukleyicisi + tip_kontrol.c DUGUM_KULLAN legacy govdesi ve YuklenmisModul/YuklenmisDosya kaldirildi; self: checker.kem + codegen.kem kullan_yeni_bicim_mi ve `legacy` dallari kaldirildi, modul_src_bir her modulu sarmalar. YOL USTUNDE OLCULDU: ciplak cok-segment ithalatta C modulu TAM YOLLA bagliyordu (`derin::f` -> T016, tam yol da T016) ve self `OK` diyordu; self-host (sarmal, priv_mod, D-584 mangle) zaten SON segmenti kullandigi icin C son segmente hizalandi. ana_legacy_gizlilik.kem artik `derin::derin_gizli()` -> T041 (C=checker=codegen 16:14) + POZITIF `derin::derin_acik()` kilitler. Legacy crossfile x5 + test_llvm 4 olcum silindi (D-589 karsiliklari yerinde). Kapilar SERI: checker_diff 187/187, modul_codegen 27/27, surucu_diff 16/16, check_genis 133/133 (-5 silinen), check_kapisi 278/285 0 RED, parser_diff 13/13, codegen_diff 173/173, yapi_diff 153/153, bolge_operand 175/175, stdlib_check, llvm_test 286/286 (-4 silinen), sifir_uyari 38/0, self_driver FIXPOINT. Sabotaj 3/3: S182 (C son-segment baglamasi) checker_diff 186/187 rc=2 . S183 (checker.kem priv kaydi) 182/187 rc=2 . S184 (codegen.kem priv kaydi) modul_codegen 23/27 rc=2 loud->silent. ARAC: `python` yok (py -3); arka plan komutunda stdin'siz `cat >` ASILDI; tip_kontrol.c HEAD blobu `-text` oldugundan autocrlf tum dosyayi degismis gosterdi -> commit core.autocrlf=false ile (gercek fark 20/90).
- 2026-09-17 D-591: kem_os C konsol yazicisini (bm_a64_yazdir.o) artik LINKLEMIYOR. OLCULDU: kem_os link girdileri icinde kdl_yazdir_* cagiran TEK kaynak kdl_zaman.c idi ve o yol kem_os'ta ULASILAMAZ (IRQ SAF-.kem: kem_zaman.kem kdl_irq_isle, kdl_kesme_isle/kdl_tik'i cagirmiyor; .kem karsiligi '[5] TIMER TIK OK' zaten var). da1b152 kem_yazdir prototipi ENTEGRE EDILMEDI (Mehmet karari). Tanilama -DKEMGU_KEM_MALLOC cesitinde derlenmez. Kapilar: kem_os_arm 24 faz rc=0 (link satirinda bm_a64_yazdir 0), qemu_cekirdek 5/5, timer_test_arm (C yaziciyi HALA kullanan bagimsiz test) 'TIMER OK tik=5' rc=0. SABOTAJ DURUSTCE: S185 (yazdirma korumasini kaldir) rc=2 ama DERLEME hatasiyla (bildirimler de korumali) -> link iddiasini olcmedi. S186 (iki korumayi birden kaldir) rc=0 SESSIZ: kdl_kesme_isle/kdl_tik hic cagrilmadigi icin --gc-sections onlari atiyor. Yani koruma bir KAPI DEGIL, hijyendir; asil kanit 24 fazli boot'un C yazici olmadan linklenip gecmesidir. SUREC HATASI: S185'i `git checkout runtime/kdl_zaman.c` ile geri aldim ve COMMIT'LENMEMIS degisikligimi de sildim -> yeniden uygulandi, boot yeniden olculdu (rc=0). Ilk S186 denemesi deseni tutmadi ('sabotaj 0') ve rc=0 GECERSIZDI.
- 2026-09-17 KAYITLI KARARLAR (Mehmet, kod degisikligi YOK): (2) aritmetik tasma SARMA olarak kalir — tanimli ve -O0/-O2 kararli, D-524 `calistir_tasma` kapisiyla sabit; panik/sonuc eklenmeyecek. (4) `dondur` (R-PAYLAS) OLDUGU GIBI kalir — D-577 baglasim kapisi (tc51_01) paylasim hakkinin yazma yasagi olmadan acilmasini engeller. (5) Send/Sync benzeri paylasilabilirlik isareti ERTELENDI — D-505'in `Dizi<T>` icerip yapi icinde kalan acigi kayitli sinir olarak durur. (6) ρ_sahip birlestirme-duyarli omur (gorev_temel `metin` donusu) YAPILMAYACAK — D-511'in 65.580 baytlik bilincli sizintisi kabul edilir (UAF riskine karsi).
- 2026-09-17 D-592: KEMGU-OS link haritasi cikarildi ve SONUC maddenin varsayimini CURUTTU. Madde 'kalan 9 C parcasini seri tasi' diyordu; olcum: --gc-sections sonrasi uart/bolge_kemregion/heap_kemmalloc/panik/zaman_kem/mmu_kem/gorev/virtio/virtio_net ve bm_a64_mmio_kem.o'nun HICBIR bolumu imajda kalmiyordu (map: yalniz start.o .text/.data/_start). Yalniz kem_os_routed.o + start.o ile baglanan cekirdek HAM IKILISI cmp ile BIREBIR AYNI (33.563.184 bayt). Yani tasinacak C YOKTU; nesneler olu link girdisiydi. KEM_OS_A64_OBJS = start.o. Kapilar: kem_os_arm 24 faz rc=0, qemu_cekirdek 5/5. S187: ayni link yapilandirmasinda tanimsiz sembol GURULTULU (ld.lld 'undefined symbol', rc=1) -> bir .kem kodu ileride C'ye dayanirsa sessiz gecmez. bm_a64_mmio_kem.o calistir_kem_os_arm on kosulu olarak KALDI (1394'teki 'C-tanimi yok' denetimi onu okuyor). YANLIS GIDEN OLCUMLER: (a) llvm-nm -u kesisimi 10/10 bos verdi ve dogru ama yetersizdi (C nesneleri birbirini cagirir) -> map ile dogrulandi; (b) ilk cmp `<(objcopy ... /dev/stdout)` ile IKI BOS cikti karsilastirdi ve 'AYNI' dedi (D-563 tuzagi) -> gercek dosyalarla tekrarlandi; (c) nesneler bir onceki temizlikte silindigi icin once imaj yeniden kuruldu. KALAN C: yalniz boot/start_aarch64.S (asm, C degil).
- 2026-09-17 D-593: TAM TAKIM (make -k test_tumu, 105 dk) YESIL, MAKE_RC=0. ONCE KIRMIZIYDI: test_parser.c [24] D-580'de kaldirilan P046'yi hala bekliyordu ve test_tumu ILK HATADA durdugu icin D-580'DEN BERI takimin calistir_parser_test SONRASI HIC KOSMAMISTI (hedefli kapilar yesildi, tam takim hic tamamlanmamisti). Test yeni davranisi (kabul + segment_sayi=2 + secili_sayi=1) olcecek bicimde duzeltildi; -k ile ayni kosumda BASKA kirmizi CIKMADI. Atlamalar hepsi bilinen/mesru (perf_bellek /usr/bin/time yok, bare-metal MMIO, ASan kurate SKIP). ARAC: arka plan bildirimi 'exit 0' dedi ama o tail'in koduydu; gercek MAKE_RC=2 logdan okundu (D-444). Gunluk satirini ilk yazarken cift tirnak icinde backtick kullandim -> kabuk tail'i calistirip stdin'de ASILDI (D-548 tuzagi), commit ve push hic olmadi. D-590 son-segment baglama anlami Mehmet tarafindan TEYIT edildi.
- 2026-09-17 D-594 (OLCUM, kod yok): (1) push: claude/jovial-euclid-a22e29 952e1d6..4edca0e (D-590..D-593). (2) Eski bare-metal C nesnelerini 131 calistir_* hedefi kullaniyor, hicbiri dogrudan test_tumu'da DEGIL ama qemu_cekirdek sha256/virtio/bignum_selfhost_arm uzerinden 3'unu DOLAYLI cagiriyor -> toptan silme o kapiyi kirar. (3) kdl_runtime.c cagri-zinciriyle dogrulanmis harita: 115 disa-verilen islev = OS 62 . ALLOC 2 . SAF 51. SAF listesinde BILINEN YANLIS-POZITIF: kdl_soket_* (5) Winsock'u fonksiyon isaretcisiyle cagirir (D-466) ve kdl_dosya_yeniden_adlandir regex disi rename kullanir. Gercek saf cekirdek: metin_* 19 . yetki_* 9 . kod_* 2 . min/maks/mutlak(64) 6 . prng 2. ILK tarama (zincirsiz) 61 SAF demisti; zincir 10'unu OS'a tasidi. ARAC: heredoc icindeki regex kacislari bozuldu (unterminated character set) -> betik dosyaya yazildi.
- 2026-09-17 D-595 (OLCUM, kod yok): runtime pilotu (kdl_min/maks/mutlak(64)) ONCE OLCULDU ve GECERSIZ cikti: alti da OLU KOD — src/selfhost'ta esleme yok, uretilen IR'da @kdl_min/maks/mutlak cagrisi 0 (codegen.kem ve cg_yetki), depoda baska referans 0. Olu kodu .kem'e tasimak anlamsiz. Genisletilmis tarama: 115 disa-verilen islevin 13'u olu aday (onek eslemesi canli sayildigi icin ALT SINIR). Kalan iki Sirada maddesi de dil/yapi karari istedigi icin loop DURDURULDU.
- 2026-09-17 D-596: runtime/kdl_runtime.c'den 9 OLU islev silindi (Mehmet: secenek a): kdl_mutlak/min/maks (+64), kdl_hata_yazdir, kdl_oku_tam, kdl_kanal_bos_mu. Tum depoda kelime-sinirli arama: referans 0 (yalniz tarihi log/CLAUDE.md anmalari). BILINCLI KORUNAN 4: kdl_bellek_hizali_al/serbest (KEMGU_SIMD_Spec_V1'de planli API) + kdl_prng_next64/seed (Capability Spec) -> spesifikasyonla celismemek icin silinmedi. Yan bulgu: kdl_mutlak(INT_MIN) tanimsiz davranisti. Kapilar: sifir_uyari 38/0 . runtime_link_test OK . gorev_rt 16/16 . kanal_omru 10/10 . dizi_sinir 39/39 . panik 6/6 . kdl_bolge 33/33 . llvm_test 286/286 . codegen_diff 173/173 . stdlib_check . self_driver FIXPOINT. ARAC: ilk kosumda iki hedef adini YANLIS yazdim (calistir_runtime_link, calistir_gorev_rt -> 'No rule') ve rc=2 aldim; gerileme degildi, dogru adlarla (_test) yeniden kosuldu. Sabotaj YOK: silme bir kural degil; olculecek davranis 'hic referans yok' ve o, linkin gecmesiyle olculdu.
- 2026-09-17 D-597 (OLCUM, kod yok): qemu_cekirdek'in dolayli kullandigi 3 temsilcinin link haritasi (--gc-sections + Map). D-592'den FARKLI SONUC: burada C GERCEKTEN kullaniliyor. Ortak tutulan bolumler: start.o vektorlerinin cektigi kdl_istisna_isle/kdl_syscall_isle/kdl_el0_izolasyon_isle (kesme), kdl_mmu_kur/kdl_ttbr_degis (mmu), kdl_preempt/kdl_surec_spawn (gorev), kdl_irq_isle (zaman), virtio_net_*; programlarin kendisi kdl_yazdir_* + kdl_bolge_* + malloc/free (heap) + sha256'da kdl_dizi_*; virtio'da ek olarak kdl_mmio_oku32 + kdl_yetki_*. Yani 131 eski hedefi silmek once bu uc temsilcinin kem_os'a tasinmasini gerektirir; Sirada 3 seri adima bolundu. Uc hedef bugun yesil (KEM SHA OK / KEM VIRTIO OK / KEM BIGNUM OK). ARAC: bu hedefler normalde --gc-sections KULLANMIYOR; olcum icin ayri gc'li link yapildi, imajlari degismedi.
- 2026-09-17 D-598: bignum_selfhost_arm testi kem_os'a faz [25] olarak TASINDI (kbn_ onekli, birlesik birimde ad cakismasi yok). Vektorler eski testle BIREBIR (V1/V2/V3). kem_os_arm kapi zincirine `[25] BIGNUM OK` eklendi; qemu_cekirdek eski bignum_selfhost_arm'i birakti (5 -> 4 temsilci, ozet metni guncellendi). Eski hedefin kendisi SILINMEDI (silme listesi onaya sunulacak). Kapilar: kem_os_arm rc=0 ([25] BIGNUM OK) . qemu_cekirdek 4/4 . baremetal_diff 5/5 . check_kapisi 278/285 0 RED. Sabotaj S188 (V1 beklenenini (1,1) yap) -> `[25] BIGNUM HATA`, kem_os_arm rc=2. SUREC HATASI: sabotaj yedegini build/kem_os.yedek adiyla aldim, ardindan `rm -f build/kem_os*` onu da sildi -> geri kopyalama BASARISIZ, sabotaj dosyada kaldi; grep ile yakalandi, Edit ile duzeltildi, kapilar yeniden kosuldu. Yedek adi silinen desenle AYNI oneki tasimamali. YAN BULGU (Sirada): kem_os_arm tarifindeki llvm-nm denetimleri D-592'de linkten dusen nesneleri (bm_a64_heap_kemmalloc.o vb.) sorguluyor ve 'eslesirse FAIL' bicimindeler -> dosya yoksa SESSIZCE gecer.
- 2026-09-17 D-599: kem_os_arm tarifindeki 5 BOS denetim kaldirildi ve yerine olcen tek denetim kondu. Eskileri `llvm-nm bm_a64_heap_kemmalloc.o | grep T malloc -> FAIL` bicimindeydi; D-592'den sonra o nesneler linke hic girmedigi icin dosya yoksa grep eslesmez ve denetim SESSIZCE gecerdi. Yeni: link `-Map=build/kem_os.map` uretir; (a) harita yoksa ya da kem_os_routed.o icermiyorsa FAIL (bos haritanin gecmesini engeller), (b) haritada start.o disinda bm_a64_*.o bolumu varsa FAIL. Pozitif 'kem_os.o T malloc/kdl_bolge_olustur/...' denetimleri KORUNDU (onlar gercekten olcuyor). Kapi: kem_os_arm rc=0, '(harita: start.o disinda C nesnesi YOK)'. Sabotaj S189 (-Map bayragini kaldir) -> 'FAIL: kem_os.map yok', rc=2. S190 (KEM_OS_A64_OBJS'e bm_a64_uart.o ekle) -> rc=0 ve bu DOGRU: nesne link listesinde ama hicbir bolumu imaja girmedi (--gc-sections atti); denetimin sorusu 'imaja C kodu girdi mi', 'listede C dosyasi var mi' degil. Imaja C kodu sokan bir sabotaj ancak .kem tarafinda bir C sembolune gercek referans ekleyerek kurulabilir; o durumda ld.lld once tanimsiz sembolde duser (S187) ya da nesne haritaya girer. SUREC: yedek adi bu kez silme deseninden ayrildi (build/yedek599_*); geri alma dogrulandi (git diff 13/21, KEM_OS_A64_OBJS = start.o).
- 2026-09-17 D-600: sha256_selfhost_arm testi kem_os'a faz [26] olarak TASINDI (ksha_ onekli; sabitler ve tur fonksiyonlari kaynak testle BIREBIR; sikistirma 8 kez yerine BIR KEZ hesaplanip 8 digest word karsilastirilir). YOL USTUNDE: ilk link `undefined symbol: kdl_dizi_yaz_tam` ile GURULTULU dustu (S187'nin ongordugu davranis) — kem_os'un saf-.kem heap'inde dizi YAZMA primitifi hic yoktu (yalniz olustur/ekle/al/boyut). runtime/kem_heap.kem'e C kdl_dizi.inc karsiligiyla birebir sinir-kontrollu `kdl_dizi_yaz_tam` eklendi. qemu_cekirdek eski sha256_selfhost_arm'i birakti (4 -> 3 temsilci; dry-run'da 0 referans). Kapilar: kem_os_arm rc=0 ('[26] SHA256 OK (abc, 8/8 word)', harita: start.o disinda C nesnesi YOK) . qemu_cekirdek 3/3 . baremetal_diff 5/5 . check_kapisi 278/285 0 RED. Sabotaj S191 (ksha_rotr'da 32-n -> 31-n) -> '[26] SHA256 HATA eslesen=0', rc=2. GECERSIZ ILK DENEMELER: (a) S191'in ilk surumu desenin IKI eslesmesi (biri YORUM satiri) yuzunden assert'e takildi ve UYGULANMADI, rc=0 anlamsizdi; (b) Makefile duzenlemesi tek satirlik `py -c` icinde ters-bolu kacisi bozuldugu icin uygulanmadi, kapilar eski Makefile ile kostu. Ikisi de betik dosyasiyla tekrarlandi.
- 2026-09-17 D-601: virtio_selfhost_arm'in kem_os'ta ZATEN karsiligi oldugu OLCULDU, kod yazilmadi. Eski test yalniz iki register okuyordu (yetki<MMIO> ile 0x0A000000 magic + version). kem_os: magic faz [3] mmio_magic_oku (ayni adres, ayni yetki mekanizmasi); version==2 runtime/kem_virtio_blk.kem:88 ve kem_virtio_net.kem:54 baslatma yolunda denetleniyor ve basarisizsa init -1 doner -> kapinin ZORUNLU tuttugu '[6] DISK RW OK' duser. qemu_cekirdek virtio_selfhost_arm'i birakti (3 -> 2: qemu_smoke + kem_os_arm). Sabotaj GEREKMEDI: yeni kural yok, kapsama iddiasi kaynak satirlarina dayandirildi.
- 2026-09-17 D-602 (OLCUM, kod yok): 131 eski BM_A64/BM_X86 hedefinin kem_os karsiligi envanteri. Once kem_os'un GERCEKTEN dogruladigi faz dizgileri Makefile kapisindan ve kem_os.kem'den cikarildi (33 dizgi), sonra her eski hedefin kendi basari dizgisi (grep -q) okundu. 131'in HICBIRI test_tumu ya da qemu_cekirdek icinde degil. Sinif (A) ~20 kem_os ayni davranisi dogruluyor; (B) ~15 isim eslesiyor ama eski test daha guclu (ornegin kalici_test iki boot arasi kalicilik, fs_journal gunluk, crashfs cokme sonrasi) — anahtar-kelime eslemesi bunlari 'karsiligi var' saymisti, ELLE DUZELTILDI; (C) ~96 karsiligi yok (smp/ag/userspace/selfhost algoritmalari/x86 ve sched/priority/rtc/kanal gibi cekirdek ozellikleri). SINIR: A/B ayrimi dogrulama dizgisi + hedef adi okunarak yapildi, her testin kaynagi satir satir karsilastirilmadi.
- 2026-09-17 D-603: Sinif A silindi (Mehmet onayi). Envanterdeki 28 adaydan IKISI olcumle CIKARILDI: uart_merhaba_bare_metal (CI ci.yml + $(BUILD)/kernel.elf kurali kullaniyor) ve kernel_dizi_bare_metal (calistir_os_kernels'te + D-250 diag sablonu). Silinen 26 hedef: timer/tick/preempt(+el0)/syscall(+arg,+ret)/istisna/uart_rx/shell/kabuk/spawn/multiproc/arp/net/icmp/virtio(+rw)_test_arm, bignum/sha256/virtio(+rw)/virtio_blk_config/virtio_net(+mac)_selfhost_arm, calis_test_arm. Makefile -755 satir (tarifler + yalniz o hedefe ait yorum bloklari; baska KALAN hedefi anan yorumlar korundu) ve calistir_os_kernels/.PHONY listelerinden cikarildi. Silinen C kaynak 17 (test/bare_metal/*_arm.c). KORUNANLAR: timer_test.c + syscall_test.c (x86 hedefleriyle paylasimli) ve .kem kaynaklari (sha256/bignum/virtio_*_selfhost.kem — codegen_genis/check_kapisi korpusunun parcasi). Referans taramasi: silinen dosyalarin adi yalniz YORUMLARDA geciyor (derleme girdisi 0). Dogrulama: make -n calistir_os_kernels/test_tumu/qemu_cekirdek rc=0 . calistir_sched_test_arm (kalan eski hedef, ayni BM_A64_OBJS) rc=0 . qemu_cekirdek 2/2. SUREC: ilk betik re.sub yerine-koyma dizgisindeki ters bolu yuzunden Makefile'a YAZMADAN dustu; ardindan gelen 'os_kernels dry rc=1' kopan && zincirinin kodu olup OLCUM DEGILDI — git diff bos oldugu dogrulanip betik duzeltildi.
- 2026-09-17 D-604: Sinif B'nin ilk kalemi. kem_pointer_arm (C derleyici) kem_os_arm tarafindan ZATEN kapsaniyor (kem_os.ll'de 101 inttoptr + 86 volatile; kapi volatile'i zorunlu tutuyor; faz [3] gercek MMIO magic). kem_pointer_self_arm ise SELF-HOST derleyicinin bare-metal volatile/inttoptr uretimini olcuyordu ve kem_os bunu KAPSAMIYORDU (kem_os yalniz C derleyiciyle kurulur). Bu degismez baremetal_diff'in BIRLESIK OS birimine TASINDI: asm sayisinin yanina volatile ve inttoptr SAYILARININ C ile esit VE C tarafinda >0 olmasi eklendi (esitlik tek basina yetmez: iki derleyici birlikte volatile'i birakirsa 0=0 gecerdi). Once olculdu: iki derleyici birebir (39 load + 47 store volatile, 101 inttoptr, 254 islev). Sabotaj S192 (codegen.kem ' = load volatile ' -> ' = load ') -> islev 254=254 ve asm 44=44 ESIT KALDI (ESKI KAPI YESIL KALIRDI), yeni sart volatile C=86 KEMGU=47 yakaladi, rc=2; geri alininca temiz rc=0. Iki hedef Makefile'dan silindi (-77 satir); kem_pointer.kem kaynagi derleyici korpusu oldugu icin KORUNDU; asan_e2e_denetim.sh'deki silinen hedefe isaret eden yorum guncellendi. make -n os_kernels/test_tumu rc=0. SUREC: kayit komutuna stdin bekleyen `cat > /dev/null` koydum, komut ASILDI ve durduruldu; kayit/commit bu betikle yeniden yapildi.
- 2026-09-17 D-605: Sinif B'den 4 kalem kem_os karsiligi OLCULEREK silindi (kod yazilmadi). Eslesme ad degil faz GOVDESI okunarak yapildi: d1 (ayni VA farkli PA, A degeri B yazdiktan sonra duruyor) = faz [16] kmmu_izolasyon_testi ile BIREBIR ayni degismez; d2 (EL0 ayricaligi + kernel sayfasi EL0'a kapali) = [5] EL0 SYSCALL + IZOLASYON; proc_test (kendi adres alani + EL0 + syscall arg) = [16]+[5]+[17]; yasam (spawn->exit->join) = [15] kem_spawn_testi (KG_OLU bekleme + program bayragi) + [13]. KALANLAR bilincli: metin (LS count=2 listeleme kem_os'ta yok) ve geri_al (4-slot havuzda 6 spawn = slot GERI ALMA; kem_os [15] tek spawn ve 2-slot sabit round-robin 'havuz TUKENMEZ' MVP) -> ikisi tasinarak kapatilacak. Makefile -94 satir, 4 C kaynak silindi (d1/d2/proc/yasam_arm.c); proc_arm.c'nin kalan anmalari yalniz x86 muadilinin YORUMLARINDA. make -n os_kernels/test_tumu rc=0.
- 2026-09-17 D-606: KEMGU-OS gorev tablosunda TASMA acigi bulunup onarildi. geri_al'i tasimak icin kem_gorev.kem okundu: kem_psayi'nin UST SINIRI YOKTU; zamanlayici olu[n]/psp[n]/ttbr[n]'yi n<kem_psayi boyunca okuyup yaziyor ve olu[] (0x45002c00)'nin hemen ARKASINDA KG_G1/KG_AKTIF (0x45003000/10) duruyor. sys(12) spawn EL0 syscall'i oldugundan kullanici alani donguyle spawn cagirip cekirdek durum degiskenlerini ezebilirdi (sys(14) zaten arg>=16 reddediyordu -> tasarlanan kapasite 16). Onarim: kem_gorev_olustur ve _el0 trap-frame yazmadan ONCE `kem_psayi >= KG_GOREV_AZAMI(16)` ise -1 doner; EL0 spawn cagiranlari zaten pid<0 denetliyor, cekirdek ici cagrilar faz basina <=3 gorev kurar. Yeni kem_os fazi [27] GOREV KAPASITE: zamanlayici ETKINLESTIRILMEDEN 20 olusturma -> 15 basari + 5 kez -1, kem_psayi==16, KG_AKTIF==0; sonda tablo sifirlanir. Kapilar: kem_os_arm rc=0 ([27] OK, onceki fazlar dahil) . baremetal_diff 5/5 . qemu_cekirdek 2/2 . check_kapisi 278/285 0 RED. Sabotaj S193 (kem_gorev_olustur kapasite satirini sil) -> '[27] GOREV KAPASITE HATA', rc=2; geri yukleme dogrulandi. SINIFLANDIRMA: geri_al (slot GERI ALMA) ve metin (LS/cok-dosya) kem_os'ta OLMAYAN ozellikleri olcuyor -> B'den C'ye tasindi (silinmedi). SUREC: ilk surumde kgk_testi normal `işlev`di ve kucresel kem_psayi okudugu icin E010 aldi (dil kurali dogru calisti) -> `çıplak işlev` yapildi. YAN BULGU: tip hatasina ragmen tarif durmadi — `kemgu | awk > kem_os.ll` boru hatti cikis kodunu maskeledi, hata bagla adiminda 'undefined symbol main' olarak goruldu (Sirada'ya yazildi).
- 2026-09-17 D-607 (OLCUM, kod yok): Sinif B'nin kalan 13 kalemi kem_os'ta OLMAYAN yetenekleri olcuyor -> hepsi SINIF C'ye tasindi, hicbiri silinmedi. kem_minifs.kem yalniz mfs_format/mfs_dosya_yaz/mfs_dosya_oku (TEK dosya, 4 bayt ad) sunuyor: kalici (iki boot arasi), crashfs + fs_journal (WAL), minifs_crud, sil, ls, dosya (cok dosya) karsiliksiz; recon_shell(2) ag-komutlu kabuk ve shell_script yorumlayici ozellik; guvenlik_kalici disk deserialize (kalicilik yok). GUVENLIK BULGUSU: guvenlik_oku/guvenlik_spawn'in olctugu kullanici-pointer ve spawn-giris dogrulamasi kem_os'ta YOK — kem_gorev.kem'in kendi yorumu syscall'larin 'kernel-guvenilir arg' aldigini yaziyor; sys(5) arg pointer'ini dogrudan okuyor. Ulasilabilirlik OLCULMEDI (yalniz kaynak okundu) -> Sirada'ya 'once olc' maddesi olarak yazildi.
- 2026-09-17 D-608 (OLCUM, BELIRSIZ): sys(5) EL0-pointer dogrulama yoklugunun ulasilabilirligi olculmeye calisildi. Kaynak KESIN: sys(5) `arg olarak *tam8`i dogrudan kdl_metin_uzunluk + kdl_metin_bayt ile okuyor, kullanici-aralik denetimi YOK; kem_gorev.kem kendi yorumu 'kdl_user_yaz_ptr_gecerli henuz .kem'e tasinmadi, kernel-guvenilir arg alir' diyor. GECICI PROBE: cekirdek-only KG_LEAK=0x45004000'e 'SIZINTI' seed + EL0 gorevi sys(5,KG_LEAK) + sys(13). SONUC BELIRSIZ: EL0 gorevi kostu ve exit etti ('[608] ... bitti' -> sys(5) sonra sys(13) yurudu) AMA UART'ta 'SIZINTI' yok, cokme/iptal de yok. Yani sys(5) bir sey basmadi -> muhtemelen seed yolum (kg_seed_kopyala -> kis_buf_yaz8) o ham cekirdek adresini KAPSAMIYOR (kis_buf .user pencerene yaziyor), yani probe KENDI TESISATINI olctu, vuln'u DEGIL (D-402: sessiz probe once PROBE'u supheli kilar). Ne 'sizinti var' ne 'yok' kanitlandi. Probe TAMAMEN geri alindi (git temiz). Gecerli olcum + fix ayri C-sinifi is (Mehmet). KALAN SIRADA'nin tamami C-sinifi -> loop DURDURULDU.
- 2026-09-17 D-609: kem_os_arm tarifindeki `kemgu --llvm | awk > kem_os.ll` boru hatti kemgu'nun tip-hatasi cikisini MASKELIYORDU (boru kodu awk'in). Once olculdu: tip hatali .kem'de kemgu rc=1 + bos IR uretiyor, ama boru rc=0 donuyor -> clang bos .ll'yi derleyip bag asamasinda 'undefined symbol main' veriyordu (D-606'nin gec/yaniltici belirtisi). Boru iki satira bolundu: `kemgu ... > kem_os_ham.ll` (rc!=0 make'i DOGRUDAN durdurur) sonra `awk ... kem_os_ham.ll > kem_os.ll`. set -o pipefail SECILMEDI: recipe kabugu Windows'ta sh; bolme tasinabilir ve acik. Temiz kem_os_arm rc=0 (faz [27] dahil). Sabotaj S194 (kem_os.kem'e tip hatali degisken enjekte) -> make rc=2 ve 'hata[T001]' ile durdu (ARTIK 'undefined symbol main' DEGIL); geri yukleme dogrulandi. NOT: bm_a64_kem_heap.o tarifinde (satir 1021) AYNI maske + 2>/dev/null var ama kem_heap.kem kararli/kanitli tek dosya; ayni sinif, ayrica bolunebilir — kayda gecti, bu artimin kapsami disi.
- 2026-09-18 D-610 (Mehmet onayi): kem_os syscall EL0-pointer aralik dogrulamasi. D-608
  probe'u belirsiz kalmisti; bu artim GECERLI bir probe kurdu ve acigi ISPATLADI. Kok
  KESIN: sys(5) `arg olarak *tam8`i kdl_metin_uzunluk+kdl_metin_bayt ile denetimsiz DEREF
  ediyor; sys 17/18/26 de user-ptr'yi denetimsiz okuyor/yaziyordu. PROVEN-C KARSILIGI VAR
  (kdl_kesme.c D-150 kdl_user_yaz_ptr_gecerli + D-151 kdl_user_oku_str_gecerli); .kem'e
  taspera: kem_os user penceresi TEK 2 MiB sayfa [0x42000000,0x42200000) (kem_mmu.kem L2[16],
  AP=01). Iki yardimci (kem_gorev.kem): kg_user_yaz_ptr_gecerli (pencere+len sinir+tasma) ·
  kg_user_oku_str_gecerli (pencere + 4 KiB tarama tavaninda null bulma; kis_buf_oku8 ile
  yalniz mapped-izinli byte'a dokunur). Guard'lar: sys(5) arg (oku) · sys(17) arg+arg2 (oku)
  · sys(18) arg (oku) + arg2/512 (yaz) · sys(26) arg/arg2 (yaz). Pencere disi -> 0-1, kernel
  bellegi ne okunur ne yazilir. GECERLI PROBE (faz [28] kptr_testi): kg_seed_kopyala ham
  kernel RAM 0x45004000'e "GIZLIVERI" seed'ler (kis_buf_yaz8 -> *(adr+i)=b, RAW yazar ->
  D-608'in "seed .user'a gitti" teshisi EKSIKTI, seed dogrudan kernel adresine iniyor) +
  kdl_syscall_isle(5, KG_LEAK, 0) EL1'de cagirir (gercek SVC ile ayni yol, deterministik,
  timing yok). GATE iki yonlu (D-425): kernel-ptr RED (r_kotu==0-1) VE user-ptr KABUL
  (r_iyi==0, "MERHABA" gorunur yankilanir). Yeni tani kodu YOK, dil yuzeyi degisikligi YOK.
  Sabotaj 2/2: S1 (sys(5) guard'i kaldir) -> "GIZLIVERI" UART'a SIZDI, faz [28] HATA, rc=2
  (acigin gercek+ulasilabilir oldugunu ve seed'in indigini AYNI ANDA kanitlar) · S2 (oku-guard
  daima 0) -> user-ptr reddedildi, faz [28] HATA, rc=2 (pozitif sekil yuk tasiyor). Kapilar:
  kem_os_arm 28 faz rc=0 · baremetal_diff 5/5 (BIRLESIK OS 258 islev/44 asm/86 volatile/101
  inttoptr uclu eslesti) · qemu_cekirdek 2/2. NOT: probe EL0-svc yerine EL1'den dogrudan
  cagri kullanir cunku SVC handler zaten EL1'de kosar -> ayni denetimsiz deref yolu; EL0
  crossing aciga sebep DEGIL, eksik aralik-denetimi acidir.
- 2026-09-18 D-611 (Mehmet delege etti): Sinif C KARARI + ilk goc (base64). KARAR: toplu
  silme YOK, kategoriye gore ayrik — GOCUR (self-host algoritmalari + benzersiz userspace →
  kem_os fazina tasi, eski izole demo sil; ANAYASA-uyumlu) · DONDUR (SMP D-490 fiziksel-ARM64,
  TCP/IP sonraki NET fazi, x86 ikincil port → belgelenmis referans, QEMU'ya kapilama, silme).
  ILK GOC: base64_selfhost_arm → kem_os faz [29]. Saf algoritma (OS bagimliligi yok):
  kb64_alfabe/index/encode/decode + kb64_testi kem_os.kem'e tasindi (normal islev + Dizi<karakter>/
  Dizi<tam32> literalleri — kem_os zaten ksha_*/heap_dizi ile bu yapilari kullaniyor). Faz [29]
  encode ("KEMGU"→"S0VNR1U=", bilinen vektore karsi pozitif dogrulama) + decode round-trip;
  ikisi de dogruysa 1. Eski test/ornekler/base64_selfhost.kem + calistir_base64_selfhost_arm
  hedefi + calistir_os_kernels satiri SILINDI (kapsam kem_os fazinda yasar → net etki
  non-destructive). Sabotaj S3 (encode `b0>>2`→`b0>>1`) → faz [29] HATA, rc=2 (gate ayirt
  ediyor; bilinen-vektor karsilastirmasi "her seyi kabul et" sabotajini da eler). Kapilar:
  kem_os_arm 29 faz rc=0 · baremetal_diff 5/5 (BIRLESIK OS 263 islev, uclu eslesti) ·
  qemu_cekirdek dolayli. KALAN GOC: crc32/sort/hashmap/rc4/hashcrack/utf8/turkce_case/
  turkce_sort/vm/json/asm-selfhost (11) — her biri ayri iterasyon.
- 2026-09-18 D-612 (Sinif C goc, iterasyon 2): crc32_selfhost_arm -> kem_os faz [30]. Saf
  algoritma (dizi yok, dtam32 bit-ops). CRC-32 yansitilmis (poli 0xEDB88320) "123456789" ->
  0xCBF43926. KRITIK dtam32: isaretsiz `>>` = lshr; tam32 olsa ashr isaret-uzatir, algoritma
  bozulur (kaynak yorumu korundu). kcrc32_byte + kcrc_testi kem_os.kem'e; eski
  crc32_selfhost.kem + hedef + os_kernels satiri silindi. Sabotaj S4 (`crc>>1`->`crc>>2`) ->
  faz [30] HATA, rc=2 (bilinen-vektor karsilastirmasi ayirt ediyor). Kapilar: kem_os_arm
  30 faz rc=0 · baremetal_diff 5/5. KALAN GOC (10): sort/hashmap/rc4/hashcrack/utf8/
  turkce_case/turkce_sort/vm/json/asm-selfhost.
- 2026-09-18 D-613 (Sinif C goc, iterasyon 3): sort_selfhost_arm -> kem_os faz [31]. Bubble
  sort in-place (Dizi<tam32> heap mutasyon: d[j]=x -> kdl_dizi_yaz_tam, sinir-kontrollu).
  [5,2,8,1,9,3,7,4,6,0] -> [0..9]. ksort_testi KESIN esitlik (d[i]==i) dogrular — no-op ve
  yanlis-permutasyon sabotajlarini birlikte eler. ksort_bubble + ksort_testi kem_os.kem'e;
  eski sort_selfhost.kem + hedef + os_kernels satiri silindi. Sabotaj S5 (`>`->`<` = azalan)
  -> faz [31] HATA, rc=2. Kapilar: kem_os_arm 31 faz rc=0 · baremetal_diff 5/5. KALAN GOC
  (9): hashmap/rc4/hashcrack/utf8/turkce_case/turkce_sort/vm/json/asm-selfhost.
- 2026-09-18 D-614 (Sinif C goc, iterasyon 4): hashmap_selfhost_arm -> kem_os faz [32].
  Knuth carpimsal hash (dtam32 mod-2^32 wrap, & 15) + linear probing, 3 paralel Dizi<tam32>
  (anahtar/deger/dolu, fn-param mutasyon). Cakisma senaryosu 5/21/37 -> ayni slot -> probing
  zinciri; khm_testi 50/210/370 + miss(99)=-1 dogrular (cakisma + miss birlikte). khm_hash/
  ekle/bul/testi kem_os.kem'e; eski hashmap_selfhost.kem + hedef + os_kernels satiri silindi.
  Sabotaj S6 (bul probing kapat, `adim<KHM_KAP`->`adim<1`) -> faz [32] HATA rc=2 (probing
  yolunu dogrudan olcer). Kapilar: kem_os_arm 32 faz · baremetal_diff 5/5. KALAN GOC (8):
  rc4/hashcrack/utf8/turkce_case/turkce_sort/vm/json/asm-selfhost.
- 2026-09-18 D-615 (Sinif C goc, iterasyon 5): rc4_selfhost_arm -> kem_os faz [33]. Stream
  cipher KSA+PRGA, 256-byte S-box in-place permutasyon (Dizi<dtam32>, dizi_olustur(256)+ekle
  ile boyut=256). dtam32 & 255 (kaydirma YOK). anahtar "Key" + "Plaintext" -> bilinen sifreli
  vektor [187,243,22,232,217,64,175,10,211] + simetrik round-trip (taze S ile sifreli->duz).
  krc4_s_olustur/ksa/prga/testi kem_os.kem'e; eski rc4_selfhost.kem + hedef + os_kernels
  satiri silindi. Sabotaj S7 (KSA `j=(j+si+ak)`->`j=(j+si)`) -> faz [33] HATA rc=2. NOT:
  round-trip TEK BASINA yetmez (simetrik oldugu icin yanlis keystream'de de gecer) — bilinen
  vektor karsilastirmasi kritik ayirt edicidir. Kapilar: kem_os_arm 33 faz · baremetal_diff
  5/5. KALAN GOC (7): hashcrack/utf8/turkce_case/turkce_sort/vm/json/asm-selfhost.
- 2026-09-18 D-616 (Sinif C goc, iterasyon 6): hashcrack_selfhost_arm -> kem_os faz [34].
  SHA-256 sozluk saldirisi (dictionary attack): "kemgu" (idx 2) SHA-256'si hedef, 8 aday
  taranir, eslesen = kirilan. KRITIK: SHA-256 primitifleri (rotr/sigma/ch/maj/K) faz [26]'nin
  ksha_* fonksiyonlariyla BIREBIR AYNI -> YENIDEN KULLANILDI (D-407: ayni soruyu iki yerde
  yanitlama). Yalniz PARAMETRELI schedule (kcrack_mesaj_cizelgesi bayt+uzunluk) + compress
  (kcrack_sikistir) + sozluk (kcrack_aday_*) + crack dongusu eklendi — faz [26]'nin ksha'si
  "abc" icin sabit-mesajdir, keyfi parola hash'leyemez. Bu goc ksha primitiflerinin keyfi
  mesaj icin de dogru oldugunu KANITLAR. Dogrulama: kirilan==2 VE atlanan==7 (hem hedefi
  dogru bulma hem digerlerini reddetme). Eski hashcrack_selfhost.kem + hedef + os_kernels
  satiri silindi. Sabotaj S8 (crack dongusu `idx<8`->`idx<2`, hedef taranmadan durur) ->
  faz [34] HATA rc=2. Kapilar: kem_os_arm 34 faz · baremetal_diff 5/5. KALAN GOC (6):
  utf8/turkce_case/turkce_sort/vm/json/asm-selfhost.
- 2026-09-18 D-617 (Sinif C goc, iterasyon 7): utf8_selfhost_arm -> kem_os faz [35] (TURKCE
  DNA). UTF-8 kod-cozucu: "cgisou" (12 byte, hepsi 2-byte) -> 6 Unicode kod-noktasi. dtam32
  mask/shift skaler uzerinde (D-173). kutf8_cozumle_iki_byte/byte_turu/devam_byte_mu/coz/testi
  kem_os.kem'e; eski utf8_selfhost.kem + hedef + os_kernels satiri silindi. Dogrulama: sayi==6
  (byte 12 DEGIL) + her kod-nokta beklenen Turkce deger + bozuk devam-byte (195,65) -> 0
  (savunmaci, sonsuz dongu yok). Sabotaj S9 (2-byte birlestirmede `<<6`->`<<5`) -> faz [35]
  HATA rc=2. Kapilar: kem_os_arm 35 faz · baremetal_diff 5/5. KALAN GOC (5): turkce_case/
  turkce_sort/vm/json/asm-selfhost.
- 2026-09-18 D-618 (Sinif C goc, iterasyon 8): turkce_case_selfhost_arm -> kem_os faz [36]
  (TURKCE DNA). Turkce-I problemi: i(105)->I(304, nokta-ustu buyuk, ASCII 'I'=73 DEGIL) ve
  i-noktasiz(305)->I-noktasiz(73). ktc_buyut/kucult/dizi_buyut/dizi_kucult/testi kem_os.kem'e.
  Dogrulama: "istanbul"->"ISTANBUL" (ist[0]==304 ASCII 73 DEGIL), "IRMAK"->"irmak"
  (irm[0]==305 ASCII 105 DEGIL) + 5 Turkce ozel harf round-trip. Eski turkce_case_selfhost.kem
  + hedef + os_kernels satiri silindi. Sabotaj S10 (i->I'da KTC_BI yerine KTC_BIc=ASCII 73)
  -> faz [36] HATA rc=2 (tam Turkce-I hatasini ayirt eder). Kapilar: kem_os_arm 36 faz ·
  baremetal_diff 5/5. KALAN GOC (4): turkce_sort/vm/json/asm-selfhost.
- 2026-09-18 D-619 (Sinif C goc, iterasyon 9): turkce_sort_selfhost_arm -> kem_os faz [37]
  (TURKCE DNA). Turkce collation: kod-nokta sirasi Turkce alfabeyle ORTUSMEZ (c<ç<d ama
  Unicode ç=231; ı<i ama Unicode ı=305). ktr_sira_indeksi (harf->0..28 Turkce sira) +
  ktr_karsilastir (havuz-tabanli substring cmp) + ktr_sirala (bubble, yalniz baslar/uzunluklar
  swap) + ktr_testi. Dogrulama: çam>can (ç>c), ıhlamur<irmak (ı<i); siralama [çam,can,ada,
  ıhlamur,irmak]->[ada,can,çam,ıhlamur,irmak] (baslar[2]==0 çam ortada + baslar[3]==9 ıhlamur
  once — Unicode tuzagi olsa ikisi de yanlis yerde). Eski turkce_sort_selfhost.kem + os_kernels
  satiri silindi. NOT: bu hedefin RECIPE'i YOKTU (yalniz os_kernels'te referansli, onceden var
  olan tutarsizlik) -> silme yalniz os_kernels satiri + kaynak. Sabotaj S11 (ç sira 3->29 =
  Unicode sonu) -> faz [37] HATA rc=2. Kapilar: kem_os_arm 37 faz · baremetal_diff 5/5.
  KALAN GOC (3): vm/json/asm-selfhost.
- 2026-09-18 D-620 (Sinif C goc, iterasyon 10): vm_selfhost_arm -> kem_os faz [38]. Stack-based
  bytecode yorumlayici (YIGIN + PC + opcode dispatch + kontrol akisi). Dizi<tam32> in-place
  PUSH/POP (kdl_dizi_yaz/al_tam, sinir-kontrollu). Opcode 0=HALT 1=PUSH 2=ADD 3=SUB 4=MUL
  5=DUP 6=PRINT. Program [PUSH 6,PUSH 7,MUL,PRINT,PUSH 100,PUSH 58,ADD,PRINT,HALT] -> 42,158.
  kvm_it/tepe/calistir/testi kem_os.kem'e (yazdir_tam kaldirildi, dogrulama bd karsilastirmasi
  ile ic). Eski vm_selfhost.kem + hedef + os_kernels satiri silindi. Sabotaj S12 (MUL `a*b`->
  `a+b`, 42->13) -> faz [38] HATA rc=2. Kapilar: kem_os_arm 38 faz · baremetal_diff 5/5.
  KALAN GOC (2): json/asm-selfhost. NOT: asm_selfhost (mini-assembler mnemonic->bytecode->VM)
  bu VM opcode'larina dayaniyor -> son iterasyonda kvm_* yeniden kullanilabilir.
- 2026-09-18 D-621 (Sinif C goc, iterasyon 11): json_selfhost_arm -> kem_os faz [39]. Byte-
  dizisi JSON ayristirici (durum makinesi): {"x": 42, "y": 100} -> [42, 100]. Dizi<tam32>
  okuma sinir-kontrollu, deger-dizisi in-place YAZMA. kjson_rakam_mi/string_atla/sayi_oku/
  ayristir/testi kem_os.kem'e (yazdir_tam kaldirildi, dogrulama ic). NOT: kem_os'ta zaten
  stdlib/json.kem (cesit ADT, host) var; bu bare-metal byte-tarayici AYRI yaklasim (cihazsiz).
  Dogrulama: cift_sayi==2 + degerler[0]==42 + degerler[1]==100. Eski json_selfhost.kem + hedef
  + os_kernels satiri silindi. Sabotaj S13 (sayi_oku `n*10`->`n*8`) -> faz [39] HATA rc=2.
  Kapilar: kem_os_arm 39 faz · baremetal_diff 5/5. KALAN GOC (1): asm-selfhost (son).
- 2026-09-27 D-622 (Sinif C goc, iterasyon 12 — SON): asm_selfhost_arm -> kem_os faz [40].
  Mini-assembler: (mnemonic, operand) ciftleri -> [38] VM bytecode'u (CEVIRI, kimlik degil:
  PRINT mnemonic 5 -> opcode 6, HALT 6 -> 0). kasm_assemble/testi kem_os.kem'e; VM icin ayri
  kopya YAZILMADI, [38]'in kvm_calistir'i yeniden kullanildi. Dogrulama GUCLU: (a) uretilen
  13 hucre [38]'in elle yazilmis programiyla HUCRE HUCRE ayni + VM 42,158; (b) SUB + tanimsiz
  mnemonic 99 (hucre URETMEZ, n2==7) -> 42. Eski asm_selfhost.kem + hedef + os_kernels
  satiri silindi. Sabotaj S14 (PRINT -> 5/DUP) -> faz [40] HATA rc=2. Kapilar: kem_os_arm
  40 faz (Linux, QEMU 8.2) · baremetal_diff 5/5. SINIF C GOCU TAMAMLANDI; SMP/TCP-IP/x86
  gruplari D-611 geregi DONDURULMUS (belgeli referans, kapisiz, silinmedi).
- 2026-09-27 (iterasyon 13, "sirada" tarama): Sinif C karar maddesi [x]'e cevrildi (12/12
  goc adimi + karar tamam; DONDUR gruplari NIHAI durum, iş degil). Sirada listesi bastan
  sona tarandi: BASKA acik/checkbox'siz madde YOK. Bu LOOP.md'nin kapsadigi Sinif A/B/C
  self-host goc calismasi (bu oturumun /loop gorevi) TAMAMLANDI. Yeni buyuk ozellik
  (concurrency/LSP/stdlib genisletme vb.) bu LOOP.md'nin kapsami DISINDA — CLAUDE.md'nin
  "Sıradaki büyük seçenekler" bölümü kullanıcı onayı ister (muhafazakar secim: kendi
  basima buyuk yeni is baslatmadim). Sirada BOS oldugu icin bu iterasyonda dosya disi
  degisiklik yapilmadi; dongu durduruldu.
- 2026-09-27 D-623 (Eszamanlilik katmani, adim 1): KANAL KEMGU-OS'ta SAF-.kem. Olcum: kem_os
  D-592'den beri C kdl_kanal.c'yi baglamiyor -> `kanal_*` cagiran her kem_os kodu tanimsiz
  sembolle duserdi. kem_heap.kem'e SUBSYSTEM/kanal: kdl_kanal_olustur/gonder/al (host ile
  AYNI ABI, i64 tasiyici) + kdl_kanal_serbest (D-543 kanal omru codegen'i bunu ISTEDI —
  ilk link hatasi) . Halka N=kapasite+1 (C surumu >3'u REDDEDIYORDU; heap var). `%` BILEREK
  yok: degisken bolenli `%` D-502 sifir-bolme kontrolu -> @kdl_panik ister, kem_os'ta YOK
  (ikinci link hatasi; onceden de 8 cagri vardi ama --gc-sections olu bolumlerde tanimsiz
  sembolu RAPORLAMIYOR). Faz [41]: FIFO + 2^33+12 (i64) + 10 tur halka sarmasi + kapasite
  0/3/16.
  🎯 SABOTAJ S16 (sarmayi kaldir) ILK TURDA SESSIZDI: indeksler sinirsiz buyur, yazimlar
  tamponun DISINA tasar (heap tasmasi) ama FIFO tutarli kalir. "Bitisik nobetci kanal"
  denendi, O DA SESSIZDI; adres OLCULDU: ardisik kanallar 64 KB aralikli — malloc serbest
  listede ilk-uyum + BOLMESIZ, her 80 baytlik kanal onceki fazlarin serbest biraktigi
  64 KB'lik rho blogunu butun aliyor (bellek israfi, ayri konu). Sonuc: tasma HICBIR
  davranissal olcumde gorunmez -> test hilesi yerine GERCEKLEMEDE sinir kontrolu
  (kem_kanal_hucre: 0<=i<N, degilse PL011'e "PANIK: kanal indeks sinir ihlali" + dur;
  D-069 dizi siniriyla ayni politika). S16 artik PANIK, rc=2. S15 (degeri i32'ye kirp)
  -> [41] HATA, rc=2. Kanal `olarak *tam8`e donusturulemiyor (E002) — iyi; adres olcumu
  runtime icinden gecici MMIO yazimiyla yapildi, geri alindi.
  Kapilar: kem_os_arm 41 faz · baremetal_diff 5/5 (BIRLESIK OS 314 islev) · check_kapisi
  266/273 (0 RED). Yol ustunde: iki vakum llvm-nm denetimi bulundu -> Sirada.
- 2026-09-27 D-624 (Eszamanlilik katmani, adim 2): GOREV KEMGU-OS'ta SAF-.kem + faz [42].
  kem_gorev.kem'e DIL GOREVI: kdl_gorev_basla_kapanis/birlestir (host ile ayni ABI). Mevcut
  preemptive zamanlayicinin (sentetik trap-frame + timer-IRQ SP-swap) USTUNE kuruldu: yeni
  gorev `kem_dil_trampolin`e eret eder, frame x0 yuvasina (@0) gorev kaydi yazilir; trampolin
  kapanisi `blr` ile cagirir (x0..x18+lr BOZULAN -> girdiler x19+'da, mov x0/x1 ezemez).
  Kayit: fn/env/rho_sahip/rho_serbest/sonuc/bitti/yigin. rho_sahip her gorevin KENDI bolgesi
  (R-GOREV S1/S2); join'de yalniz D-309 kaniti varsa serbest. SIRA + IRQ MASKESI: once olu=1
  sonra bitti=1 (ters sira = main yigini serbest birakirken olu gorevin frame yazmasi UAF;
  maskesiz = arada IRQ gorevi bitti yazilmadan kalici atlatir -> birlestir sonsuza bekler).
  Faz [42]: uretici gorev -> kap=2 kanal -> main 20 mesaj (IKI YONDE gercek bloklama),
  iki paralel gorev (5050 + 2^33+42), L005 geregi her eşleş kolunda birlestir.
  🎯 YOL USTUNDE UC KUSUR:
  (1) KEM_OS YANLIS ADRESE BAGLIYDI: ham imaj (.img, RPi kernel8 bicimi) QEMU'da 0x40080000'a
      yukleniyor, ELF 0x40000000'a bagli, yeniden-konumlama YOK. Kod PC-goreli (adrp) oldugu
      icin 41 faz bunu HIC gormedi; ilk kez VERIDE saklanan mutlak islev adresi (yakalamasiz
      kapanisin sabit-katlanmis {ptr @lambda, null}) kullanilinca olculdu: ESR EC=0 (tanimsiz
      komut), ELR = baglama-zamani adresi = sifir bellek. QEMU -d int + monitor `xp` ile
      KANITLANDI (0x40029f58 -> 0x00000000, gercek kod 0x400a9f58). Onarim: linker script'e
      istege bagli taban (`DEFINED(__kem_yukleme_tabani)`), yalniz kem_os `--defsym ...=0x40080000`
      gecer; diger ELF hedefleri degismedi. 42 fazin HEPSI (EL0/izolasyon/W^X/ag/disk) sagam.
  (2) C SESSIZ KIRPMA: gorev_başlat'a DOGRUDAN verilen annotasyonsuz BLOK-form kapanis `i32`
      yayiliyordu -> gorev<tam64> sonucu kirpiliyordu (host -O2 exit 1, ARM64 `mov w0,#42`;
      -O0 x86'da rax ust yarisi tesadufen korundugu icin DOGRU GORUNUYORDU). Onarim: bu
      baglamda donus = i64 (runtime ABI; self-host D-300 ile ayni), ptr/double tahmini korunur.
      BONUS: `|| { ver "abc"; }` gorevi ONCEDEN LINK-RED idi, artik calisiyor.
      ⚠ yapi_diff'in K3 muafiyeti ("lifted lambda i32 vs i64 = bilincli fark") BU KUSURU
      MASKELIYORDU; onarimla cg_gorev_lambda_blok eslesti -> listeden CIKARILDI (D-534).
      baremetal_diff'e K3 benzeri normallestirme EKLEMEK ayni kusuru orada da gizlerdi —
      bilerek yapilmadi; C'nin i64 yaymasi pariteyi normallestirmesiz sagladi.
  (3) SELF-HOST T020 + C'nin baglanmis-kapanis yolu -> Sirada'ya (ikisi de olculdu).
  SABOTAJ: S17 (birlestir beklemesin) -> [42] HATA rc=2 . S18 (kanal dolu-bekleme kaldir)
  -> [41] OK ama [42] HATA rc=2: [42] GERCEK bloklamayi olcuyor . S19 (C onarimini kapat)
  -> yapi_diff 153/154 rc=2. ⚠ llvm_test (-O0) S19'u GOREMEZ — olculdu, o yuzden kapi yapi_diff.
  Kapilar: kem_os_arm 42 faz . baremetal_diff 5/5 (BIRLESIK OS 324 islev) . codegen_diff
  173/173 . yapi_diff 154/154 (21 muaf) . llvm_test 286/286 . drf_test 54/54 . gorev_rt 16/16
  . drf_gorunurluk 100/100 . kanal_omru 10/10 . check_kapisi 266/273 . sifir uyari 38/0.
  Ortam: drf_test ilk kosumda ASan runtime eksikligiyle (libclang-rt-18-dev) bag hatasi verdi
  — kod degil; paket kuruldu, 54/54.
- 2026-09-27 D-625 (C kapanis tip kaybi — IKI YONLU sessiz kirpma). D-624'un Sirada'ya
  yazdigi madde ve yol ustunde bulunan kardesi.
  (1) DONUS: `lambda_donus_tahmin` blok-form'da ilk `ver`in degerini yalniz DIS kapsamda
      ariyordu -> govdenin yereli/lambda parametresi bulunamiyor -> NULL -> i32. Daha kotusu
      GOLGELEMEDE dis kapsamdaki ayni adli degiskeni bulup YANLIS tipi veriyordu. Onarim:
      TahminBaglam — ad cozumu sirasi = golgeleme sirasi: bloktaki ONCEKI `değişken`ler
      (sondan basa; annotasyon ya da degerin tahmini, yalniz daha oncekilere bakarak ->
      `x = x + 1` sonlanir) -> lambda parametreleri -> dis kapsam.
  (2) ARGUMAN (yol ustunde bulundu): kapanis cagri yeri argumanlari BEKLENEN TIP OLMADAN
      uretiyordu -> `f(8589934592)` literali i32 dogup `i64 %a` parametresine kirpilmis
      geciyordu — ANNOTASYONLU `işlev(tam64) -> tam64` kapanista BILE. Onarim: LlvmIsim.
      kapanis_imza (bildirilen `işlev(..)` tip dugumu ya da annotasyonsuz baglamada lambda
      dugumu); cagri yeri i. argumani o tiple uretir + tamsayida genisligine uyarlar
      (isaretsizse zext).
  Olcum matrisi (host clang -O2, eski/yeni): v1 1/42 . m2 LINK-RED/42 . c1 1/42 . c2 1/42
  . c3 1/42 . c5 1/42 . a3 1/42 . a4 1/42 . a5 1/42 . u3 1/42 . s1 1/42 . s3 1/42 . m1
  LINK-RED/42 . degismemesi gereken c4 (tam32) 42/42 . u2 (dtam8->dtam32 zext) 42/42.
  🎯 KAPI SECIMI OLCULDU: llvm_test -O0 derliyor ve x86'da rax ust yarisi cogu sekilde
  TESADUFEN korundugu icin kusur GORUNMUYOR. -O0'da da ayirt edici sekiller secildi:
  c2 (DIS kapsamda ayni adli tam32 — golgeleme), a3, a4, c3, u3 -> llvm_test [287]-[291].
  ⚠ Ilk yazimda c2'nin dis `x: tam32`ini dusurmustum -> S20 SESSIZ kaldi; golgeleme geri
  konunca yakalandi. cg_korpus'a KONAMADI: self-host ayni sekillerde yanlis (Sirada).
  SABOTAJ: S20 (blok-yereli aramasini kapat) -> [287] ✗ rc=2 . S21 (cagri yeri argüman
  tiplemesini kapat) -> [288]-[291] ✗ rc=2.
  Kapilar: llvm_test 291/291 . codegen_diff 173/173 . yapi_diff 154/154 . codegen_genis
  58/58 (70 - Sinif C gocunde silinen 12) . bolge_operand 175/175 . ct_bariyer 14/14 .
  modul_codegen 27/27 . snapshot 50/50 . drf_test 54/54 . gorev_rt 16/16 . kanal_omru
  10/10 . drf_gorunurluk 100/100 . stdlib_check . sifir uyari 38/0 . kem_os_arm 42 faz .
  baremetal_diff 5/5.
  YOL USTUNDE: dtam64 2^63 literal doyurmasi (ilgisiz, sessiz) + self-host ayni sinif -> Sirada.
- 2026-09-27 D-626 (self-host kapanis tip kaybi — D-625'in aynasi). Olcum: self-host AYNI
  iki sessiz kusuru tasiyordu (a4 `|a: tam64| a + 42; f(2^33)` -> `call i32 %8(ptr, i32
  8589934592)`, exit 1). Onarim C ile birebir: (1) `lam_ret_tahmin_b` — ad cozumu golgeleme
  sirasinda (bloktaki onceki `değişken`ler -> lambda parametreleri -> dis kapsam);
  `lam_ret_tahmin_l` lambda dugumunden, genel kapanis ve gorev_başlat yollari onu kullanir.
  (2) `cg_aimza` paralel dizisi (TIP_ISLEV ya da LAMBDA dugumu; degisken + parametre
  kaydinda yazilir) + `kapanis_param_tip`; cagri yeri argumani param tipine uydurur:
  immediate (literal metni zaten tam deger) -> yalniz tip, register -> int_uydur (dtamN
  zext). Olcum (self, -O2, eski/yeni): a4 1/42 . u3 1/42 . c2 1/42 . c5 1/42 . m2
  LINK-RED/42 . s2/c4/u2 degismedi (42/42). Checker'in T020 verdigi 7 sekil (a3 c3 c1 v1
  m1 a5 s1) bu adimin disinda — ayri, gurultulu kusur.
  🎯 D-625'in BEKLEYEN ISI KAPANDI: iki derleyici artik ayni sekillerde anlastigi icin
  kapanis tip kaybi cg_korpus'a girdi -> `cg_kapanis_tip_kaybi.kem` (her olcum AYRI cikis
  kodu; 2^32 ustu degerler — tam32 bu sinifi gosteremez; POZITIF tam32 sekli). Eski C ve
  eski self ikisi de exit 1 veriyordu. ⚠ Ilk surumde (e) isaretci-donen blok yereli vardi;
  self checker onu T020 ile REDDETTI (ayri kusur) -> kapi yanlis sebeple kirmizi olmasin
  diye cikarildi.
  SABOTAJ: S22 (self arguman uydurmasi kapali) -> codegen_diff 173/174, "C 42 ≠ KEMGU 2" .
  S23 (self blok-yereli aramasi kapali) -> 173/174, "C 42 ≠ KEMGU 1". Kodlar hangi yolun
  dustugunu soyluyor.
  Kapilar: codegen_diff 174/174 . yapi_diff 155/155 (fikstur MUAFIYETSIZ — lambda donus
  tipleri C ile birebir) . codegen_genis 58/58 . modul_codegen 27/27 . surucu_diff 16/16 .
  bolge_operand 176/176 . ct_bariyer 14/14 . kanal_omru 10/10 . checker_diff 187/187 .
  self_driver TUM MODLAR + SELF-HOST + FIXPOINT ✓.
- 2026-09-27 D-627 (self-host checker: blok-form kapanis `ver` baglami). Kok: checker gövdeyi
  cevreleyen islevin `aktif_donus`uyla yuruyordu; blok-form kapanistaki `ver`, KAPANISIN
  donusudur (C tip_kontrol.c D-304: `lambda_blok_cikarsama` iken aktif_donus_tipi'ye karsi
  denetlenmez). Sonuc: gecerli programlar sahte T020 aliyordu; GOLGELEMEDE ise distaki ayni
  adli degiskenin tipi tesadufen eslesip geciyordu. Onarim (checker.kem + codegen.kem check
  yolu, UC-UYGULAMA kurali — ikisi birden): genel cocuk dongusunde LAMBDA'nin govdesi BLOK
  ise alt agac `aktif_donus = "?"` ile yurunur, bitince GERI YUKLENIR (ic ice kapanis guvenli).
  Olcum (C / ref checker / surucu, kod+satir+sutun BIREBIR): 9 gecerli sekil (b1 b3 a3 c3 c1
  v1 m1 a5 s1) sahte T020 -> temiz; 3 negatif (normal islevde T020, kapanistan SONRAKI dis
  `ver`, ic ice kapanistan cikistaki dis `ver`) T020'yi AYNEN veriyor.
  Onarimla self-host codegen bu sekilleri ILK KEZ gordu: 8'i C ile ayni ve dogru cevap;
  v1 (`görev_başlat(f)`, baglanmis kapanis) self-host DERLEYICIYI COKERTTI -> ayri kusur,
  Sirada (gurultulu).
  Fikstur: tc53_01_kapanis_ver_baglami (checker_diff: 1 pozitif islev + 2 negatif) +
  cg_kapanis_tip_kaybi'ne D-626'da DISARIDA kalan (e) metin, (f) tam64 blok yereli, (g)
  annotasyonlu blok-form geri eklendi (C=SELF=42).
  SABOTAJ: S24 (checker.kem geri yukleme yok) -> checker_diff 187/188 (negatifler kayboldu)
  . S25 (codegen.kem "?" atamasi yok) -> self_driver --check iki modda tc53 kirmizi.
  Kapilar: checker_diff 188/188 . check_genis 133/133 (bayat muafiyet yok) . codegen_diff
  174/174 . yapi_diff 155/155 . surucu_diff 16/16 . modul_codegen 27/27 . codegen_genis 58/58
  . bolge_operand 176/176 . check_kapisi 267/274 . self_driver FIXPOINT ✓.
- 2026-09-27 D-628 (self-host `görev_başlat(f)` paniği). Kok: gorev_başlat dali argumani
  DAIMA lambda LITERALI sayip a_cb/a_cs okuyordu; TANIMLAYICI'da cocuk -1 -> "PANIK: dizi sinir
  ihlali" (rc=134). D-627 oncesi checker bu sekli T020 ile durdurdugu icin GORUNMUYORDU.
  Onarim (C llvm.c birebir): arguman literal degilse fat value ifade_uret + extractvalue
  fn/env; rho_serbest = 0 (kanit YOK = DENY, C gorev_rho_confined "fn degeri -> DENY");
  kuyruklama YOK (baglanmis lambda tanim yerinde genel kapanis kolunca kuyruklandi);
  T = bagllamanin kayitli kapanis donusu (cg_aic: annotasyonlu -> bildirilen, annotasyonsuz
  -> D-325 tahmini). Literal yolu AYNEN.
  Olcum (C / SELF): v1 (blok tam64) 42/42 . g1 (yakalamali tam32) 42/42 . g2 (metin) 42/42
  . g3 (annotasyonlu tam64) 42/42 . g4 (ayni kapanis iki gorevde) 42/42.
  Fikstur: cg_gorev_bagli_kapanis.kem (4 sekil, ayri cikis kodlari; eski self-host bu dosyayi
  hic derleyemiyordu).
  SABOTAJ: S26 (eski davranis) -> codegen_diff 174/175 . S27 (baglanmis kapanista T'yi dusur)
  -> 174/175 (i64 ptr yuvasina: LINK-RED — gurultulu). ⚠ rho_serbest'i baglanmis kapanista
  yanlislikla 1 yapmak (UAF riski) BU FIKSTURLE AYIRT EDILEMEZ — sonuclar ρ_sahip'e isaret
  etmiyor; C ile ayni DENY bilincli secildi, kanit analizi (rho_confined) yalniz literal
  govdede calisir.
  Kapilar: codegen_diff 175/175 . yapi_diff 156/156 (fikstur muafiyetsiz) . bolge_operand
  177/177 . kanal_omru 10/10 . codegen_genis 58/58 . ct_bariyer 14/14 . modul_codegen 27/27 .
  self_driver FIXPOINT ✓.
- 2026-09-27 D-629 (dtam64 ust yarisi literal). Kok: C `strtoll` 2^63 ve ustunu INT64_MAX'a
  DOYURUYORDU; uc self-host kopyasi (`parser/checker/codegen.kem tamsayi_deger`) bunu parite icin
  BILEREK taklit ediyordu -> `dtam64` 2^63..2^64-1 hic yazilamiyordu (`x >> 60` 8 yerine 7).
  ONARIM: C'de literal dogrudan kaynaktan, self-host `tamsayi_deger` ile BIREBIR ayni algoritmayla
  okunur (strtoull de degil — D-407); [0, 2^64) tam, deger iki'nin tumleyeni BIT DESENI (memcpy),
  2^64+ iki tarafta ayni sekilde 2^64-1'e doyar. Self-host dtam64 biriktirir.
  🎯 YOL USTUNDE UC ONCEDEN VAR OLAN KUSUR:
  (1) C `sayi_tokeni_temizle` girdiyi `_`'lari atmadan ONCE 63 karaktere KIRPIYORDU -> 64 basamakli
      ikilik literal 2^63 yerine 2^60 (fikstur p_buyuk_literal'in ilk kosumu yakaladi; self dogruydu).
  (2) Self-host `tam64_str`: `0 - INT64_MIN` tasar -> SONSUZ OZYINELEME -> SEGFAULT (onceden hicbir
      literal INT64_MIN uretmedigi icin gizliydi). Son basamak `%` ile ayrilir, uc kopyada.
  (3) 🔴 KRIPTO: `stdlib/kripto.kem sabit_süre_seç_u64` maskesi `18446744073709551615` literaliyle
      kuruluyordu -> 0x7FFF..FF -> maske "hepsi 1" iken secilmemesi gereken `f`'nin UST BITI
      sonuca SIZIYORDU (eski derleyici exit 9; sabit-sureli secimde sessiz yanlis cevap). Literal
      onarimi bunu da duzeltti; `stdlib/kripto/rastgele.kem`in 0x9E3779B97F4A7C15 sabiti de
      simdiye dek 0x7FFF.. oluyordu (placeholder PRNG, artik dogru sabit).
  Fikstur/kapi: parse_korpus/p_buyuk_literal (dump; INT64_MIN basimi) . cg_korpus/cg_dtam64_ust_yari
  (davranis) . llvm_test [292] (MUTLAK) . kripto_kosum'a 4. vektor (+8, gercek stdlib islevi;
  BEKLENEN 7 -> 15).
  🎯 KAPI DERSI: S29 (C'yi eski doyurmaya dondur) codegen_diff VE parser_diff'te SESSIZ kaldi —
  self-host'un kendi `tavan` literalini de C derledigi icin iki taraf ESIT bozuluyor (bootstrap
  baglasimi; D-580 "parite kapisi esit bozulmaya kordur" dersi). Mutlak beklenen degerli kapilar
  eklendi: llvm_test [292] ✗ ve kripto_kosum ❌ ile yakalandi. ⚠ Ilk S29 denemesi ikiliyi hic
  yeniden kurmamisti (ayni-saniye mtime, D-457) — sabotajin ikilide oldugu `--ast` ile dogrulandi.
  SABOTAJ: S28 (kripto_kosum eski C ile) -> exit 7 ❌ . S29 -> llvm_test 291/292 + kripto 7/15 .
  S30 (parser.kem eski tam64_str) -> parser_diff 13/14.
  Kapilar: parser_test 107 . lexer_test 103 . snapshot 50 . parser_diff 14/14 . checker_diff 188 .
  codegen_diff 176 . yapi_diff 157 . check_genis 133 . surucu_diff 16 . modul_codegen 27 .
  codegen_genis 58 . llvm_test 292 . tip_kontrol 202 . stdlib_check . kripto_kosum 4/4 .
  check_kapisi 269/276 . sifir uyari 38/0 . kem_os_arm 42 faz . baremetal_diff 5/5 . FIXPOINT ✓.
- 2026-09-27 D-630 (literal aralik tanisi T043). Literal baglamdaki somut tamsayi tipine
  SIGMIYORSA T043 (eskiden sessiz kirpma: `tam8 = 300` -> 44, `tam64 = 2^63` -> INT64_MIN).
  Tekli eksi baglaminda isaretli alt sinir (-128, INT64_MIN) gecerli. Baglamsiz literal degere
  gore yukseltilir: <=2^31-1 tam32, <=2^63-1 tam64, ustu dtam64. Rapor dugum basina TEK kez
  (C checker literali D-021 yuzunden birden cok kez tipler). D-021 iki-literal yeniden tiplemesi
  artik DAR tarafi GENISE cevirir (self-host kendi kaynaginda `0 - 9223372036854775807 - 1`
  icin sahte T043 aliyordu). C + checker.kem + codegen.kem check yolu.
  🔴 ILK PUSH SELF-HOST'TA SESSIZ YANLIS CEVAP URETTI (codegen_genis sha256_selfhost yakaladi):
  literal i64 dogunca (a) dizi literali elemani kendi tipiyle `_tam64` eklenip `Dizi<dtam32>`in
  4 baytlik gozelerine 8 bayt yaziyordu (HEAP TASMASI), (b) `d[i] != 3049323471` dtam32 elemani
  ISARETLI genisletiyordu. Onarim: eleman dizinin eleman tipine uydurulur; ciplak literal karsi
  operandin tipini alir (C D-021 aynasi). ⚠ Ilk onarimda "baglamsiz diziyi on-taramayla i64 yap"
  denedim — `ver [..]` donus baglami eleman bilgisi TASIMADIGI icin YANLISTI, geri alindi.
  ⚠ Gomulu kaynak: test_llvm.c [269] `0 - 128` tam8 -> T043 (DOGRU; 128 tam8 degil) — .kem
  taramam C dizgisine gomulu kaynagi yine GORMEDI (D-517'nin tekrari); `-128`e cevrildi.
  Fikstur: check_korpus/tc54_01_literal_aralik (7 T043 + pozitif, uc checker birebir) .
  cg_korpus/cg_literal_yukseltme (baglamsiz yukseltme + donus-baglamli karisik dizi; C=SELF=42,
  eski C 134).
  SABOTAJ: S132 (C rapor kapali) -> checker_diff 188/189 . S133 (checker.kem) -> 188/189 .
  S134 (codegen.kem check yolu) -> dogrudan olcum T043 7 -> 0 (self_driver --check paritesi) .
  S135 (C baglamsiz yukseltme kapali) -> codegen_diff 176/177 (C exit 134).
  Kapilar: test_tumu TAM rc=0 (Tum testler gecti, FIXPOINT ✓) . checker_diff 189 . codegen_diff
  177 . yapi_diff 158 . llvm_test 292 . check_kapisi 270/277 . kripto_kosum 4/4 . sifir uyari 38/0.
- 2026-09-27 D-631 (C escape analizi bag dongusu). `a = b` ve `b = a` (ayri dallarda)
  `bag_guncelle` ile a->b, b->a dongusu kurar; `ifadeyi_yukselt`in TANIMLAYICI dali zinciri
  izleyip SONSUZ OZYINELEMEYE giriyordu -> C derleyici SEGFAULT (`--check` dahil; gecerli
  program). D-630 onarimi sirasinda self-host kaynagindaki `gt = st; st = gt` bicimi tetikledi.
  Onceden vardi (D-629 ikilisi de cokuyor); self-host etkilenmiyor.
  ONARIM: bag basina `zincirde` bayragi — gezinti bu bagdan zaten geciyorsa dur. Terfi MONOTON
  (ayni `yeni` ile ikinci ziyaret hicbir kaydi degistirmez) -> sound. Bayrak gezinti sonunda
  SIFIRLANIR (kalici "ziyaret edildi" DEGIL). `ifadeyi_yukselt` bag EKLEMEZ -> realloc yok,
  indeks gecerli. Kullanilmayan `bag_cozumle` kaldirildi (sifir uyari).
  UAF OLCUMU: dongu/uzerine-yazma sekillerinde diziler ρ_caller'da (`%rho`) — `ky_confined`
  ayri kanit; yani kusur COKME idi, UAF degil (olculdu, varsayilmadi).
  Fikstur: cg_korpus/cg_escape_bag_dongusu (dizi + metin dongusu, C=SELF=42, eski C 139) .
  test_escape [17][18] (analiz biter + bayrak sifirlanir).
  SABOTAJ S131 (koruma kapali) -> test_escape ASan stack-overflow + fikstur --check 139.
  Kapilar (worktree): escape_test 24/24 . bolge_atama 15 . codegen_diff 178 . bolge_operand 180 .
  bolge_yonlendirme DOGRU . check_kapisi 271/278 . llvm_test 292 . yapi_diff 159 . sifir uyari 38/0.
- 2026-09-27 D-632 (2^64 ve ustu literal). Iki derleyicide de 2^64-1'e SESSIZCE doyuyordu
  (`dtam64 = 18446744073709551616` -> 2^64-1). Hicbir tamsayi tipine sigmaz -> baglamdan BAGIMSIZ
  T043 (yeni tani kodu YOK). C: ayristiricinin zaten hesaplayip ATTIGI `tasti` dugume tasinir
  (`tam.tasti`), checker baglamli + baglamsiz yolda raporlar (D-630 tekillestirmesi). Self-host:
  ayristirici DOYMUS degeri `a_deg`e yazar -> yan kanal `tasma_node` (checker.kem + codegen.kem;
  --ast dump'i degismedi, parser_diff 14/14). ⚠ Self checker annotasyonsuz baglamanin baslaticisini
  `ifade_tip` ile HIC tiplemiyor (bilinen bosluk) -> ilk surumde `değişken c = 999..9;` kaciyordu;
  gezinti TAM dugume vardiginda da raporlanir.
  Olcum: 13 sekil (onluk/onaltilik/ikilik/sekizlik/`_`, arguman, donus, tekli eksi, dizi, kesirli
  baglam + T003/T001 ile sira) uc checker'da kod+satir+sutun BIREBIR; 2^64-1 her baglamda temiz;
  repo genelinde T043 farki 0; test/*.c gomulu kaynaklarda 2^64+ literal yok.
  Fikstur: check_korpus/tc54_02_literal_2_64 (7 T043 + sinir-alti pozitif).
  SABOTAJ: S136 (C bayragi dugume yazilmaz) -> checker_diff 189/190 . S137 (checker.kem yan kanal
  kapali) -> 189/190 . S138 (codegen.kem gezinti raporu kapali) -> annotasyonsuz 2 T043 kaybolur
  (self_driver --check check_korpus'u ayni sekilde karsilastirir).
  Kapilar: test_tumu TAM rc=0 (Tum testler gecti, FIXPOINT ✓) . checker_diff 190/190 . parser_diff
  14/14 . check_kapisi 271/278.
- 2026-09-27 D-633 (kesirli literal 63 karakter kirpmasi). C `sayi_tokeni_temizle` (sabit 64
  bayt, `_`'lari atmadan ONCE kirpiyor) ve self-host runtime `kdl_ondalik_bicimle` (`j < 63`)
  kesirli literali 63 karakterde kesiyordu -> uzun literalin USSU dusuyordu: `1.000…0e10` (67
  karakter) -> 1.0. Iki derleyicide de SESSIZ YANLIS DEGER (olculdu: ikisi de exit 1). Tampon artik
  lexeme boyunda (C: parser arena'si; runtime: malloc/free). --ast dump'i C=self birebir.
  ⚠ SUREC: runtime dosyasi gecerli UTF-8 DEGIL; python'u `latin-1` ile acip Turkce karakter
  yazmaya calisinca `open('w')` dosyayi ONCE kesti, sonra encode hatasi verdi -> kdl_runtime.c
  2328 satir kaybetti (git'ten geri alindi, baska degisiklik yoktu). Bu dosyada DUZENLEME IKILI
  MODDA (bytes) ve ASCII yorumla yapilir.
  Fikstur: cg_korpus/cg_uzun_kesirli_literal (67/76/85 karakter + `_` + kisa literal; C=SELF=42,
  eski 1) . llvm_test [293] MUTLAK (C ayristiricisi ile runtime ayni kusuru tasiyordu -> iki taraf
  esit bozulursa parite kapilari kor, D-629 dersi).
  SABOTAJ: S139 (C tamponu 64'e sinirla) -> codegen_diff 178/179 (C exit 1) + llvm_test [293] ✗ .
  S140 (runtime `j > 63` kirp) -> codegen_diff 178/179 (KEMGU exit 1).
  Kapilar: test_tumu TAM rc=0 (Tum testler gecti, FIXPOINT ✓) . llvm_test 293 . checker_diff 190 .
  parser_diff 14/14 . codegen_diff 179.
- 2026-09-28 D-634: ILK YERLI ARM64 (DGX Spark) TAM KOSUMU — iki kok onarildi, test_tumu rc=0.
  NEDEN: gelistirme 2026-09-27'de Spark'a tasindi ama yerli tam kosum HIC yapilmamisti; CLAUDE.md
  "ilk kosumun sonuclari buraya olculerek yazilmali" diyordu. Ilk kosum rc=2 ile 70 kapinin
  7.'sinde DURDU; tam tablo ancak `make -k` ile cikti (D-486'nin "rc bir IDDIADIR" kuralinin
  canli ornegi — tek basina rc'ye bakan biri 63 kapiyi hic gormezdi).
  OLCUM: 9 kapi kirmizi, iki kokte toplandilar.
  KOK A — TESTLER KONAK MIMARISINI GOMUYORDU: test_tip_kontrol.c / test_wcet.c / test_llvm.c /
  check_kapisi.sh AS001 beklentisini sabit "x86_64"e dayandiriyordu. D-469 ile varsayilan hedef
  DERLEME PLATFORMUNDAN geldigi icin ARM64'te roller TERSINE dondu: gomulu x86_64 etiketi AS001
  yedi, "yabanci" sanilan arm64 etiketi temiz gecti. Derleyici DOGRUYDU, test yanlis olcuyordu
  (ayrica olculdu: mimari arm64 -> check rc=0, x86_64 -> AS001 rc=1).
  Onarim: etiket artik AS001'in okudugu AYNI tek kaynaktan gelir (llvm_hedef_mimari() ya da
  linklenmeyen test_llvm.c'de KEMGU_HEDEF_MIMARI makrosu; olculen ikili de ayni makroyla kurulur).
  KOK B — SELF-HOST'UN HEDEFI SABIT x86_64/WINDOWS UCLUSUNDEYDI. CLAUDE.md'de TAHMIN olarak
  kayitliydi ("dusmesi beklenir, kod okumasiyla bulundu, olculmedi"); artik olculdu: C `csdb`
  uretirken self-host `lfence` uretti.
  Onarim D-407 geregi AYNI KURAL: Makefile `uname`den selfhost/konak.kem URETIR (konak_mimari +
  konak_triple, .gitignore'da); codegen.kem ve checker.kem `kullan` ile alir. Mekanizma C'ninkinden
  FARKLI olmak ZORUNDAYDI: codegen.kem'in onislemcisi, `ortam_al`i ya da onceden tanimli bir hedef
  sabiti YOK (ucu de arandi) -> konak ancak URETILEN BIR MODULLE gorunur. Kural ayni: "varsayilan
  hedef = uzerinde kuruldugum makine". Uc bayrak durumunda da iki derleyici artik BIREBIR ayni
  ucluyu verir (bayraksiz aarch64-unknown-linux-gnu / arm64 none-elf / x86_64 windows-gnu).
  checker.kem'de IKINCI bir sabit daha vardi (as001_kontrol); oradaki yorum kendi kosulunu
  YAZMISTI ("surucuye --mimari eklenirse BURASI da guncellenmeli") — kosul gerceklesmisti.
  ⚠ B'YI TEK BASINA ONARMAK ct_bariyer'i YESILE DEGIL BOSA CIKARIRDI: o blok lfence sayar ama iki
  derleyiciyi de BAYRAKSIZ cagiriyordu ("bayraksiz = x86_64" varsayimi). Varsayilan konaga
  donunce iki taraf da csdb uretir, sayi 0=0 cikar, kapi OLCMEDEN gecerdi (D-425). x86 yarisi
  artik acikca --mimari x86_64 sabitliyor (asagidaki ARM64 yarisinin, D-468, eksik ikizi);
  ARM64 makinede x86 bariyer paritesi ILK KEZ gercekten kapsaniyor.
  MIMARI IKIZLER: etiket tasiyan .kem TANIM GEREGI mimariye ozgudur (gercek makine komutu kosar),
  tek dosya iki konagi kapsayamaz -> snapshots/asm_round_trip_arm64.kem + cg_korpus/
  cg_satirici_asm_arm64.kem eklendi. Harness konaga gore secer (KONAK_MIM=$(ARCH), `uname` IKINCI
  KEZ cagrilmaz — D-407), yabanci ikiz ACIKCA yazdirilarak atlanir. Muafiyet listelerine HICBIR
  satir eklenmedi.
  HANGI KAPI NEYI OLCTU (once -> sonra): tip_kontrol 197/202 -> 202/202 . llvm 289/293 -> 293/293 .
  wcet 35/37 -> 37/37 . check_kapisi 1 RED -> 0 RED . ct_bariyer 7/14 -> 14/14 . checker_diff
  186/190 -> 190/190 . check_genis 130/133 -> 134/134 . codegen_diff 177/179 -> 179/179 .
  self_driver --check 147/151 -> 151/151, FIXPOINT ✓ (stage1 IR == stage2 IR, 84212 satir).
  test_tumu TAM rc=0, 0 kirmizi, eksik aractan atlama YOK.
  Ozet satirlari ilk kosumla DIFFLENDI: degisen her satir ya onarim ya ikizlerin diger kapilarca
  kapsanmasi; muaf sayilari (6/13/21), ASan SKIP=31 ve "atlanan 3" AYNI -> hicbir kapi sessizce
  atlamaya DONUSMEDI.
  SABOTAJ S141 (self-host'un x86 lfence emisyonunu dusur) -> ct_bariyer 7/14, "x86 bariyer sayisi:
  C=35 != KEMGU=0", rc=2. Bu kanit onemli: yeni sabitlenen x86 yarisinin ARM64 konakta GERCEKTEN
  disi oldugunu gosterir — onarim oncesi o yari bos olacakti, yani iddia olculdu, varsayilmadi.
  Geri alma SAYILDI: lfence 2 (call + declare), kaynakta "SABOTAJ" izi yok, kapi yine 14/14.
  W33 YANLIS SEBEPLE YESILDI: iddiasi `h >= 1 && rt >= 1` idi ve AS001'in ekledigi fazladan hatayi
  YUTUYORDU; olctugu sey RT007. `h == 1 && rt == 1` diye SIKILASTIRILDI — bu onarim bir kapiyi
  yesilden yesile degil, GEVSEKTEN SIKIYA tasidi.
  YANLIS GIDEN DENEMELER: (1) codegen_diff'e ekledigim KONAK_MIM zorunlulugunu selfhost_driver
  delegasyonuna gecirmeyi UNUTTUM -> self_driver "kapi KOSMADI" ile dustu. Kontrolun gurultulu
  olmasi kendi eksigimi yakaladi (D-446 lehine dogrudan kanit). (2) Sabotaj sayim beklentisini
  "1 -> 0" yazdim; dogrusu "2 -> 1" cunku dizge `declare` satirinda da geciyor — D-530'un ayni
  sinifta kayitli sayim dersi. (3) UART tabanini parametrelestirmeyi IS olarak onerdim; ZATEN
  parametreydi (-DKDL_PL011_BASE / -DKDL_16550_BASE, PL011'de basliginda belgeli) -> oneri dustu.
  (4) Ilk probe'da python str.format kaynaktaki suslu parantezlerde patladi ve dosyayi ONCE
  kesti -> BOS .kem dosyasi `--check`ten "basarili" doner; olcum bir an yanlis okundu.
  ACIK KALDI (a): ARM64 FIZIKSEL dogrulama YAPILAMADI. Bare-metal imaj QEMU `virt`e CAKILI —
  giris 0x40000000 (bare-metal-aarch64.ld:33), UART 0x09000000 + kaynaktaki yorumu
  "/* QEMU virt UART0 */" (kdl_runtime_uart_pl011.c:39); imaj -ffreestanding -nostdlib oldugu icin
  Linux'ta "dogrudan calistirilamaz", BOOT edilmesi gerekir. Yani D-490'in gerekcesi QEMU'yu
  dislarken imajin kendisi QEMU'ya bagliymis — belgenin (D-530) fark etmedigi bagimlilik.
  Yazilim onkosullari HAZIR (yukleme tabani ve UART tabani ezilebilir, Spark konsolu 16550 sinifi
  ve o surucu yesil, kexec kurulu); engel FIZIKSEL: Spark'in UART tabani DT/ACPI'den okunmali,
  seri ciktiyi okumak icin ikinci makine gerekli, kexec Linux'u dusurur (geri donus power-cycle).
  Linux KULLANICI ALANINDA "ayni testi" kosma kisayolu REDDEDILDI ve gerekcesi kontrol listesine
  yazildi: bariyerler MMU-off/MMU-on CACHEABILITY UYUSMAZLIGI icindir (smp_queue_arm.c:28-33 —
  cekirdek 1 MMU kapali non-cacheable, cekirdek 0 MMU acik Normal-WB). Linux'ta iki is parcacigi
  da MMU acik ve ayni inner-shareable alanda -> donanim coherency'si devreye girer, `dc` komutlari
  GERCEKTEN gereksiz olur, sabotaj YANLIS-YESIL verir. Adim 3'un uyardigi tuzagin ta kendisi,
  ustelik bu sefer test zayif oldugu icin degil OLCULEN KOSUL HIC KURULAMADIGI icin.
  D-490 kapatilmadi ve daraltilmadi; "ertelenmis borc" olmaktan cikip "tek eksigi fiziksel
  kurulum" haline geldi.
  ACIK KALDI (b): kemgu_self hedefi build/kemgu.exe yolunu SABIT yaziyor -> Linux'ta kosamaz.
  ⚠ BU SATIRIN ILK HALI YANLISTI ve D-636'da duzeltildi: "rc denetlenmedigi icin yine de
  'uretildi' der (D-446 sinifi sessiz basarisizlik)" demistim. Olculdu: hedef SESSIZCE
  GECMIYOR, Error 127 ile DUSUYOR; `2>/dev/null` yalniz taniyi yutuyor, cikis kodunu degil.
  Kusur "sessiz" degil "teshis edilemez"di. [D-636'da kapandi]
- 2026-09-28 D-635: kem_os_arm VAKUM DENETIMLERI silindi (cevrilmedi) + iki yetim .o kurali.
  OLCUM (once): `build/bm_a64_mmu_kem.o` ve `build/bm_a64_zaman_kem.o` DOSYA OLARAK YOK.
  `llvm-nm <olmayan dosya>` hata verir, boru hattindaki `grep -q` BOS girdi alir, `if` yanlis
  olur -> denetim "0 C-tanim" mesajini basip SESSIZCE GECER. Elle tekrarlandi: "GECTI dali".
  KOK: D-592 kem_os linkini `boot .S + SAF-.kem`e indirdi ve KEM_OS_A64_OBJS'i yalniz
  `start.o` birakti. O iki nesneye HICBIR hedef bagimli degildi -> hic kurulmadilar. Tek
  "kullanicilari" bu iki denetimdi; denetimler de tam bu yuzden bos kosuyordu. Dairesel olu
  kod: kural denetim icin, denetim kuralin urettigi (uretmedigi) nesne icin.
  NEDEN CEVIRMEK DEGIL SILMEK: madde "harita tabanli denetime cevir YA DA sil" diyordu.
  Cevirmek yeni kapsam getirmezdi — silinen denetimin kapsami SIFIRDI (hicbir kosulda
  ateslenemezdi), dolayisiyla silmek sifir kaybettirir. Ayrica ayni soruyu ikinci bir yerde
  yanitlamak D-407 sinifidir: C-disligi ZATEN iki yerde olculuyor ve ikisi de kosuyor —
  (1) pozitif .ll denetimi `define @kdl_mmu_kur` kem_os.ll'de OLMAK ZORUNDA (asil koruma:
  biri MMU'yu C'ye geri alirsa .kem define kaybolur), (2) D-599 harita denetimi.
  Yetim kurallar da silindi: birakmak "bu nesne kuruluyor + denetleniyor" izlenimi verirdi,
  olculen gercek bunun tersiydi. Kaynaklar (kdl_mmu.c / kdl_zaman.c) DURUYOR — bm_a64_mmu.o,
  bm_a64_zaman.o ve bm_x86_zaman.o onlari kullaniyor (once olculdu, sonra silindi).
  ⚠ GECERSIZ SABOTAJ S142 — VE BU BIR BULGU: harita denetiminin disini kanitlamak icin
  KEM_OS_A64_OBJS'e `bm_a64_mmio_kem.o` ekledim. Kapi ATESLEMEDI (rc=0, "harita: start.o
  disinda C nesnesi YOK") ve bir an "demek harita denetimi de bos" sandim. Olculdu: link
  `--gc-sections` kullaniyor, nesnenin hicbir sembolu referans edilmiyor -> nesne haritaya
  HIC girmiyor. Yani DENETIM YANILMADI, SABOTAJ GECERSIZDI: nesne imaja girmedigi icin
  "imajda C nesnesi yok" iddiasi DOGRUYDU. Denetim "ld'ye ne verildigini" degil "imajda ne
  KALDIGINI" olcer — korunmak istenen sey de budur. (D-527'de kayitli gecersiz-sabotaj
  sinifinin tekrari; oradaki ders "sabotaj olculen kosulu GERCEKTEN kurmali"ydi.)
  Bu sinir Makefile yorumuna yazildi: ilk yazdigim gerekce harita denetimini "EVRENSEL"
  diye niteliyordu, olcum bunu YALANLADI ve gerekce duzeltildi.
  GECERLI SABOTAJIN NASIL OLACAGI: C nesnesinin imajda TUTULMASI gerekir, yani ayni sembolu
  .kem'in saglamayi BIRAKMASI sart (tek basina nesne eklemek ya gc'lenir ya da cift-tanimla
  link'i loud kirar). Iki parcali oldugu icin bu iterasyonda kurulmadi; pozitif .ll
  denetimleri o senaryoyu zaten D-277/D-279'da kapiyor.
  HANGI KAPI NEYI OLCTU: kem_os_arm rc=0 . qemu_cekirdek 2/2 . test_tumu TAM rc=0.
- 2026-09-28 D-636: kemgu_self Linux'ta onarildi + D-634'teki YANLIS TARIF duzeltildi.
  OLCUM (once): `make kemgu_self` -> "make: *** [Makefile:783: kemgu_self] Error 127", rc=2.
  Recete `build/kemgu.exe` ve `build/kemgu_self.exe` yollarini SABIT yaziyordu; Linux'ta
  ikili uzantisiz (`build/kemgu`) -> komut bulunamiyor. Geride 0 baytlik kemgu_self.ll kaliyor.
  ⚠ KENDI IDDIAMI YALANLADIM: D-634'te bu kalemi "rc denetlenmedigi icin yine de 'uretildi'
  der (D-446 sinifi SESSIZ basarisizlik)" diye kaydetmistim. YANLIS. Kabuk komutu bulamayinca
  127 doner ve make bunu YAYAR -> hedef GURULTULU duser. `2>/dev/null` yalniz TANIYI yutuyordu,
  cikis kodunu degil. Kusur "sessiz" degil "TESHIS EDILEMEZ"di: kullanici yalniz "Error 127"
  goruyor, nedenini gosteren satir kayboluyor. Yanlis tarif CLAUDE.md'ye ve bu dosyaya
  commit'lenmisti; ikisi de duzeltildi (D-407: yanlis olgu iki yerde yasiyordu).
  ONARIM: $(EXE) kullaniliyor (Makefile bunu ZATEN hesapliyordu, recete kullanmiyordu —
  D-469'un "derleyici tasinir, kapilar tasinmaz" sinifi), stderr BASTIRILMIYOR, komutlar
  gorunur (`@` kaldirildi; bu elle calistirilan bir kolaylik hedefi, sessizligin degeri yok).
  DUMAN DENETIMI EKLENDI: ikili yalniz URETILMIS olmakla kalmaz, kosturulup IR uretebildigi
  de dogrulanir. Gerekce: `calistir_self_driver` bu hedefe BAGLI DEGIL (harness kendi
  driver'ini SELF/SELF2 env ile kurar) -> hedef test_tumu'da yok ve bozuklugu hicbir kapi
  yakalamadi. Denetim o bosluGu hedefin KENDI icinde kapatir.
  DOGRULAMA: rc=0, `build/kemgu_self` (uzantisiz) uretildi, kosuyor ve
  `target triple = "aarch64-unknown-linux-gnu"` emit ediyor — D-634'un konak hedefi
  self-host IKILISINDE de yerinde demektir (ek kanit).
  SABOTAJ S143 KISMEN GECERSIZ — ve nedeni yapisal: ikiliyi bozup (exit 3 doner) duman
  denetimini sinadim; IDDIA TETIKLENDI ("FAIL dali"). Ama `make kemgu_self` phony oldugu
  icin her cagrida YENIDEN LINK ediyor ve sabote edilen ikiliyi UZERINE YAZIYOR -> make
  duzeyinde sabotaj tutunamiyor. Bu bir kusur DEGIL, dogru davranis. Gecerli bir make-duzeyi
  sabotaji, derlenen ama calismayan bir ikili uretecek KAYNAK bozulmasi ister; maliyeti
  yuksek, iddia-duzeyi kanit yeterli goruldu.
  HANGI KAPI NEYI OLCTU: make kemgu_self rc=0 + duman denetimi . test_tumu TAM rc=0.
- 2026-09-28 D-637: README'nin bayat KEMGU-OS iddialari duzeltildi + eksik bolum eklendi.
  OLCUM: README dort ayri yerde artik YANLIS olan sey soyluyordu — (1) "Uc Stratejik Hedef"
  altinda "Mevcut durum: ... Tam OS yok" (yalniz UART+VirtIO+ELF bring-up sayiyordu),
  (2) "Surucüler / Bare-Metal" girisi "tam bir isletim sistemi degildir (zamanlayici,
  MMU/sayfalama, syscall YOK)", (3) yol haritasi "kanal'in bare-metal tarafinda testi yok",
  (4) uzun vade kovasi "Saf-KEMGU isletim sistemi + surucüler (zamanlayici, MMU, syscall)"
  diye HEDEF sayiyordu. Dorduncusunu ilk taramada KACIRDIM; "bayat iddia" gre'bi tekrar
  kosunca cikti — tek pasaji duzeltip gecmek yetmiyor, AYNI iddianin butun kopyalari
  aranmali (D-407'nin belge tarafi).
  GERCEK: kem_os_arm kapisi 42 fazi falsifiye-kanitla olcuyor — MMU (sayfalama +
  non-identity ceviri + fault, D-276/277), gercek trap ESR_EL1 (D-278), CNTV timer IRQ
  (D-279), PREEMPTIVE zamanlayici + context-switch (D-280), syscall + EL0 izolasyonu +
  EL0-pointer dogrulamasi (D-281/D-610), VirtIO blok + minifs + VirtIO net, ve D-623/624
  ile kanal (faz 41) + bloklayan gorev (faz 42). Tamami .kem; haritada start.o disinda
  C nesnesi yok.
  EKLENDI: "KEMGU-OS — saf-KEMGU mikrocekirdek" bolumu (alan/olcum/karar tablosu).
  Gerekce: (2)'yi duzeltirken "asagida KEMGU-OS bolumu" diye atif yaptim ve o bolum
  YOKTU -> asili referans birakmis oldum; onarim yeni kusur uretmemeli.
  DURUSTLUK: "Tam OS yok" iddiasi SILINMEDI, DARALTILDI. Bugun gecerli sinirlar her
  pasajda acikca yaziyor: yalniz QEMU `virt` (imaj o bellek haritasina bagli, fiziksel
  donanimda HIC kosmadi), TEK CEKIRDEK (-smp yok -> eszamanlilik orada kanitlanamaz,
  D-490), gorev bolgesi serbest birakilmiyor, tam kullanici alani yok.
  HANGI KAPI NEYI OLCTU: belge_kapisi 9/9 . test_tumu TAM rc=0.
- 2026-09-28 D-638: PROGRESS.md ve TODO.md — bayat "yapilacaklar" belgeleri temizlendi.
  NEDEN BIR ISTI: ikisi de kok dizinde CANLI is listesi gibi okunuyordu ama ikisi de
  kapanmis kampanyalarin gunlugu. Yanlis yon gosteren belge, olmayan belgeden kotudur.
  OLCUM — PROGRESS.md'nin BES ertelenmis maddesi tek tek sinandi (kopyalanmadi):
    D-003 heap `d[i]=v`      -> KAPANMIS (cg_yapi_dizi.kem: Dizi<Nokta> indeks YAZMA)
    D-004 LAMBDA V2          -> KAPANMIS (`|| k + 2` ucdan uca exit 42; korpusta 10
                                cg_kapanis_*/cg_gorev_lambda dosyasi)
    D-007 struct-degerli dizi-> KAPANMIS (ayni dosya: ps[0].x + `için` gezinme)
    D-008 dondur/kanal/gorev -> KAPANMIS (cg_gorev_* korpusu + kem_os faz [41]/[42])
    D-009 asm ciktisi yapi alanina -> HALA ACIK, dogrulandi (P264 ile reddediliyor)
  Yani 5'te 4 bayat. Bu oran tek basina belgenin neden guvenilmez oldugunu gosteriyor.
  KARAR — IKISI DE SILINMEDI (LOOP kurali: belirsizlikte en muhafazakar secenek):
    PROGRESS.md yerinde kaldi, "TARIHSEL KAYIT" banneri + satir satir duzeltme aldi;
    D-009 canli kuyruktaki C3'e isaret ediyor.
    TODO.md `belgeler/D-086_Driver_Entegrasyon_Gunlugu.md`ye TASINDI + banner. Silmek
    bilgi kaybi olurdu: 12 maddenin hepsi [DONE] ve D-086 DECISIONS_LOG'da kayitli AMA
    M1-M12 goc adimlarinin ayrintisi yalniz o dosyada. Asil kusur icerik degil ADIYDI —
    kok dizinde "TODO.md" gorunmesi.
  Once referans taramasi yapildi: Makefile/harness/CI hicbiri TODO.md'ye bakmiyor
  (yalniz LOOP.md'nin kendi kuyruk maddesi) -> tasima kapilari kirmiyor.
  HANGI KAPI NEYI OLCTU: belge_kapisi 9/9 . test_tumu TAM rc=0.
- 2026-09-28 D-639: kuyruk L'den M'e BOLUNDU — 18 ust madde, 74 somut alt adim.
  NEDEN: onceki konsolide kuyrukta (D-634 sonrasi) belirsizligin neredeyse tamami L
  maddelerinden geliyordu — 34-74 iterasyonluk bandin 24-64'u. "Ne zaman biter"
  sorusunu daraltmanin tek yolu L'leri M'e bolmekti; bunu Mehmet de istedi.
  YONTEM: alt adimlar UYDURULMADI, her biri depoda olculdu (satir/islev sayilari,
  kod yeri, mevcut kapi). Olcum sirasinda IKI BAYAT IDDIA DAHA cikti:
    D3 "Semaforlar / bariyerler" — README yapilacak diye sayiyor. OLCULDU:
       stdlib/semafor.kem + bariyer.kem + kilit.kem VAR (uceri de 3 islev) ve
       calistir_stdlib_check onlari TIP-denetliyor. Eksik olan KOSUM kaniti; hicbir
       kapi bu ucunun CALISTIGINI olcmuyor. Madde "semafor yaz"dan "var olanin
       calistigini kanitla"ya donustu (D-425: tip-denetimi calismayi kanitlamaz).
    D5 "Metin/Dosya tamamlama" — ikisi de bos DEGIL: metin.kem 36 islev, dosya.kem
       17 islev. Gercekten ince olan kripto (karma.kem 3, rastgele.kem 4) ve OS RNG
       kaynagi YOK. Madde "once EKSIGI LISTELE, sonra yaz" diye daraltildi.
  Bu ikisi, B1/B2'deki desenin ucuncu ve dorduncu tekrari: README/PROGRESS bir isi
  "yapilacak" sayiyor, olcum "zaten var ya da baska bir sey eksik" diyor. Kuyrugu
  olcmeden konsolide etseydim bu oturumda toplam 10'a yakin hayalet is tasiyacaktim.
  BAGIMLILIKLAR ISARETLENDI (once yoktu): D6 -> D9 (bolge serbest birakma, escape
  ozetini bekler) . E2 E1'den BAGIMSIZ kosulabilir (QEMU'da SMP var) ama zayif-bellek
  kaniti icin yine E1 gerekir . F1.4 (weak-memory fence) <-> E1: fiziksel olcum
  olmadan ispat "modele gore dogru" kalir.
  ACIK UCLU OLANLAR ISARETLENDI: E3 (tam userland — "v1 userland neye denir" karari
  Mehmet'te) ve F1 (DRF V2 — alti bileseni de tek basina arastirma kalemi).
  SONUC: 1 S + 15 M + 2 L (acik uclu). Onceki 5S/7M/10L ile kiyaslanabilir degil
  cunku L'ler bolundu; bandin daralmasi bu yuzden.
  HANGI KAPI NEYI OLCTU: belge_kapisi 9/9 . test_tumu TAM rc=0 (D-638 kosumu, kuyruk
  degisikligi kod degil).
- 2026-09-28 D-640: annotasyonsuz baglamanin tipi izleniyor (DAR: yalniz literal).
  OLCUM (once): `değişken x = 8589934592; değişken y: tam32 = x;` C T001, self OK;
  `x = "a"` de oyle. Kok: `yerel_topla` tipi YALNIZ annotasyondan okuyordu
  (checker.kem:5145 / codegen.kem:12433); annotasyon yoksa "?" -> dosyanin kendi
  kurali geregi ("?" = bilinmiyor -> T001 atla) o baglamaya dair BUTUN denetimler
  dusuyordu.
  ORACLE ONCE OLCULDU (8 vakalik matris) — ve bir varsayimi yikti: KEMGU'da ORTUK
  GENISLETME YOK. `x = 1` (tam32) -> `y: tam64 = x` bile C'de T001. Bunu bilmeden
  yazsaydim "genisletme zararsiz" diye yanlis kural koyardim.
  `ifade_tip` CAGRILMADI: yan etkili (`t043_raporla`), `yerel_topla` ise ON-TOPLAMA
  gecisi -> oradan cagirmak tanilari CIFTLERDI. Literal dallarinin SAF alt kumesi
  yazildi; kural ayni (D-630 baglamsiz yukseltmesi dahil).
  BILEREK DAR: yalniz duz literaller; cagri/aritmetik/tanimlayici basaticilar "?"
  = eski davranis. Gerekce olculmus risk: `yerel_tip_filtrele` notlari generic'te
  sahte T003 uretildigini kaydediyor. D-377/D-378 gibi adim adim acilmali.
  SONUC: 8 vakanin 7'si paritede (p6 T043 yan etki olarak kapandi). p7
  (`y: tam64 = x + 8589934592`) HALA ayrisiyor ama artik SESSIZ DEGIL: C T043 3:31,
  self T001 3:5. Aritmetik yeniden-tipleme yolu, bu maddenin disinda.
  ⚠ SABOTAJ BIR KUSURUMU YAKALADI: once codegen.kem'i sabote edip checker_diff
  bekledim — ATESLEMEDI. Cunku checker_diff checker'i `checker.kem`den kurar.
  Iki uygulama FARKLI kapilarca sinaniyor. Ayri ayri olculdu:
    checker.kem  -> checker_diff  (S144b: 190/191, rc=2)
    codegen.kem  -> self_driver   (S144c: 151/152, rc=2, hem C-built hem self-host)
  Ikisi de korumali, ama ikisini de KANITLAMAK gerekti. Bu, D10'un (tek-kaynak
  konsolidasyon) maliyetinin somut olcumu: tek onarim iki dosya + iki sabotaj.
  check_genis bu yuzeyi HIC gormuyor (check_korpus'u taramiyor — snapshots/ornekler/
  stdlib tariyor); yanlis kapiya guvenmek sessiz bosluk olurdu.
  HANGI KAPI NEYI OLCTU: checker_diff 190 -> 191/191 . check_genis 134/134 .
  check_kapisi 274/280 0 RED . self_driver 152/152 . test_tumu TAM rc=0; ozet
  satirlarinda TEK fark checker_diff'in +1 fiksturu.
- 2026-09-28 D-641: bagalamsiz dizi literalinde T013 paritesi — ve tolerans altindaki
  GERCEK kusur.
  OLCUM (once): `[1, 2.5]` ve `[1, 8589934592]` C'de T013, self-host'ta OK. Kuyruk
  maddesi sebebi "bagsiz yolda `sayisal_mi` toleransi" diye kaydetmisti.
  C1'DEN SONRA OLCULDU (madde oyle diyordu): C1'in literal cikarsamasi bunu
  KAPATMAMIS. Ama olcum boslugu daraltti — `[1, "x"]` (sayisal olmayan karisim) ve
  annotasyonlu yol ZATEN paritedeydi; ayrisan yalniz BAGLAMSIZ SAYISAL karisim.
  ILK ONARIM (eksikti): tolerans kaldirildi. C kurali toleranssiz
  (tip_kontrol.c DUGUM_DIZI_OLUSTUR: `!tip_esit(ilk,e) && e->kategori != TIP_HATA`),
  ustelik self-host'un BAGLAMLI dali da toleranssizdi -> ayni dil kurali iki dalda
  FARKLI uygulaniyordu. 6/6 probe paritede, uc parite kapisi YESIL.
  ⚠ AMA TAM TAKIM KIRMIZI: codegen_genis 58/58 -> 57/58, `sha256_selfhost.kem`de
  yanlis-pozitif. UC PARITE KAPISI DA BUNU GORMEDI — yakalayan sey gercek programlari
  derleyip kosturan baska bir korpustu. "Kapilar yesil" tek basina onay degil;
  HANGI kapinin HANGI yuzeyi gordugu onemli (C1'de de ayni ders cikmisti).
  GERCEK KOK: dosya `ver [1116352408, ..., 3049323471, ...]` yaziyor, donus tipi
  `Dizi<dtam32>`. 3049323471 > 2^31 oldugu icin BAGLAMSIZ yolda "tam64" cikarsaniyor
  ve ilk elemanin "tam32"siyle karisim saniliyor. C ayni dosyaya OK der cunku
  beklenen tipi `ver`e TASIR. Self-host `dizi_bek` baglamini yalniz DEGISKEN/SABIT
  icin kuruyordu, `ver` icin KURMUYORDU.
  Yani `sayisal_mi` toleransi KEYFI DEGILDI: EKSIK CIFT-YONLU TIP AKISINI ortuyordu.
  Bu, "bir toleransi kaldirmadan once NEYI ortttugunu sor" dersi.
  IKINCI ONARIM (dogrusu): toleransi geri koymak yerine AKISI TAMAMLA — `ver`
  dugumunde `p.dizi_bek` `aktif_donus`tan kurulur (C ile ayni kural, D-407).
  Yanlis-pozitif kapandi, 6/6 probe paritede kaldi.
  Fikstur tc47_02: karisim vakalari + TEMIZ nobetciler (`[1,2]`, `[1.5,2.5]`,
  `["a","b"]`) — kural gevsetilirse de asiri sikilastirilirsa da kirmizi olur.
  SABOTAJ iki dosyada ayri (C1 dersi): S145 (checker.kem) -> checker_diff 191/192;
  S145b (codegen.kem) -> self_driver 152/153, make rc=2.
  ⚠ ONCEKI TURDA `rc` YANLIS RAPORLANDI: "SABOTAJ rc=0" yazmistim ama o grep'in
  cikis koduydu, make'in degil. Bu turda make rc'si ayrica olculdu.
  HANGI KAPI NEYI OLCTU: checker_diff 191 -> 192/192 . codegen_genis 58/58 (geri
  donduruldu) . check_genis 134/134 . check_kapisi 0 RED . test_tumu TAM rc=0;
  ozette TEK fark checker_diff'in +1 fiksturu.
- 2026-09-28 D-642: C3 olculdu -> KOD DEGISIKLIGI YOK; iki yeni gercek + kuyruk duzeltmesi.
  (D-531 deseni: olcum bir iterasyonun mesru ciktisidir.)
  BULGU 1 — C3 YANLIS BOYUTLANDIRILMIS (M -> L). satirici_asm cikti hedefi AST'de
  AD (string: `cikti_adlar`) olarak tutuluyor, lvalue/ifade olarak DEGIL. `&r.deger`
  desteklemek TEMSIL degisikligidir: C'de ast.h + parser.c + tip_kontrol.c:6575 +
  llvm.c:7496 + ast_yazdir.c, self-host'ta codegen.kem; ustelik `--ast` dokumu
  parser_diff'te BAYT-BIREBIR parite kapisi. Tek iterasyonluk is degil.
  BULGU 2 — ASIL KUSUR BASKA VE DAHA BUYUK: C3'u olcerken cikti su oldu:
    C    `çıktı("=r", &r.deger)` -> P264 (reddeder)
    SELF ayni dosya -> `OK` VE IR URETIR:
         `%7 = call %R asm sideeffect "mov $0, #42", "=r"()`
    yani register kisiti YAPININ TAMAMINA baglaniyor — SESSIZ YANLIS IR.
  Kok C3'te degil: SELF-HOST `--check` PARSER HATALARINI HIC RAPORLAMIYOR.
  Genel sinandi: `ver 1 }` (noktali virgul eksik) -> C `P090 1 32`, self `OK`.
  Tip hatalari (T020 1 26) IKI TARAFTA DA cikiyor; yalniz PARSE hatalari gorunmez.
  Surucunun `--check` dali `p.th_kod` (tip tablosu) bakiyor, `p.hata_say` (parser
  sayaci) HIC bakmiyor.
  NEDEN KAPILAR SUSUYOR: checker_diff ve self_driver `test/check_korpus`u
  karsilastirir; o korpusta PARSE HATALI TEK DOSYA YOK (hepsi checker'i olcmek icin
  yazilmis, dolayisiyla ayristirilabilir). 192/192 ve 152/152 bu yuzeyi HIC olcmuyor.
  Bu, D-424'te kayitli "loud -> silent" sinifinin tekrari ve bu oturumda ucuncu kez
  ayni ders: KAPI YESIL OLMASI YUZEYIN OLCULDUGU ANLAMINA GELMEZ (C1'de check_genis
  check_korpus'u taramiyordu; C2'de uc parite kapisi codegen_genis'in gordugu
  gerilemeyi gormemisti).
  BOYUT OLCULDU: self-host parser YALNIZ `hata_say` sayaci tutuyor, kod/konum
  KAYDETMIYOR (14 artirma noktasi). C bicimiyle parite icin parser tani tablosu
  gerekli -> L. Bu yuzden kuyruga C0 olarak ve C3'ten ONCE kondu: C3'un belirtisi
  C0'in bir ornegi, kok once kapanmali.
  KOD DEGISIKLIGI BILEREK YAPILMADI: dar bir "hata_say > 0 ise OK deme" yamasi
  loud'a cevirirdi ama C ile FORMAT paritesi olmadigi icin korpusa fikstur
  EKLENEMEZDI -> kapi yine kor kalirdi. Yarim onarim, olculmemis onarimdir.
  HANGI KAPI NEYI OLCTU: degisiklik yok; tum kapilar D-641 yesilinde.
- 2026-09-28 D-643: self-host parse hatalari artik GORUNUR — ve `--llvm` yanlis IR
  uretmiyor.
  OLCUM (once, D-642'den): `ver 1 }` -> C `P090 1 32`, self `OK`. Daha kotusu:
  `çıktı("=r", &r.deger)` -> C reddeder (rc=1, bos stdout), self rc=0 + 156 satir IR,
  icinde `call %R asm sideeffect ..., "=r"()` — register kisiti YAPININ TAMAMINA
  bagli, sessiz YANLIS IR. `--llvm | clang` boru hattinda bu bozuk IR'in sessizce
  derlemeye gitmesi demekti.
  KOK: parser YALNIZ `hata_say` sayaci tutuyordu (kod/konum YOK) ve surucu ona HIC
  bakmiyordu. Iki ayri eksik: tablo yok + kapi yok.
  ONARIM: (1) `ph_kod/ph_sat/ph_sut` tani tablosu, (2) `bekle_k(p,t,kod)` — kod
  CAGRI YERINDE verilir (C `parser_bekle` aynasi; ayni token beklemesi farkli
  baglamda farkli kod alir), (3) `--check` dalinda parse hatasi varsa tip kontrolu
  KOSMAZ ve yalniz parser tanilari basilir — C davranisi ONCE OLCULDU (eksik `;` +
  tip-hatali baglama iceren dosyada C yalniz P082 basar), (4) `--llvm` dalinda
  parse kapisi: bos stdout + stderr + cikis 1.
  ARTIMLI (C1 deseni): 104 `bekle` cagrisindan UCU eslendi; gerisi "P000". Bu
  BILEREK — hepsini tek seferde eslemek olculmemis toplu degisiklik olurdu.
  Korpusta parse-hatali dosya YOKTU (olculdu) -> "P000" hicbir kapiyi bozmuyor.
  KORPUSUN SEKLI KAPININ KOR NOKTASIYDI: check_korpus'un 153 dosyasinin hepsi
  checker'i sinamak icin yazilmis, dolayisiyla AYRISTIRILABILIR. Bu yuzden
  checker_diff 192/192 ve self_driver 152/152 yesil oldugu halde butun bir hata
  sinifi olculmuyordu. tc48_01 korpustaki ILK parse-hatali dosya.
  SONUC: eksik `;` -> iki tarafta da `P090 18 1` (BIREBIR). asm alan vakasi:
  `--check` self `P264 7 26` (onceden OK), `--llvm` rc=1 + 0 satir (onceden 156).
  ⚠ IKI GECERSIZ SABOTAJ (ikisi de kendi kusurumdu, kaydediliyor):
    (1) S146b ilk deneme: duzenlemem `{`i yorumun ICINE soktu -> DERLEME KIRILDI
        (rc=2) ve kapi sabotaj yuzunden degil SOZDIZIMI yuzunden kirmizi oldu.
        Kirmizi'yi kanit saymak yanlis olurdu.
    (2) Ikinci deneme: girinti varsayimim (8 bosluk) tutmadi, yama HIC UYGULANMADI
        ve kapi YESIL kaldi — bu da "sabotaj gecti" diye okunabilirdi.
    Ders: sabotajin INDIGINI ve DERLEMENIN GECTIGINI ayrica dogrula (D-402/D-490'in
    sayim dersinin ikizi: yalniz "uygulandi mi" degil, "gecerli mi" de sorulmali).
  GECERLI SABOTAJ: S146 (checker.kem) -> checker_diff 192/193, make rc=2.
                   S146b (codegen.kem, dogru sozdizimi) -> self_driver 153/154, rc=2.
  HANGI KAPI NEYI OLCTU: checker_diff 192 -> 193/193 . self_driver 152 -> 154/154 .
  test_tumu TAM rc=0; ozette TEK fark checker_diff'in +1 fiksturu.
- 2026-09-28 D-644: C3 yeniden olculdu -> BOYUT L'DEN M'E DUSTU. KOD DEGISIKLIGI YOK.
  D-642'de C3'u M'den L'ye CIKARMISTIM; o yukseltmenin iki dayanagi vardi ve
  ikisi de bugun olculup YANLIS bulundu:
    (1) "`--ast` bayt-paritesi riske girer." YANLIS. ast_yazdir.c'nin
        DUGUM_SATIRICI_ASM dali cikti hedeflerini alt agac olarak BASMIYOR — yalniz
        `cikti=%d girdi=%d bozulan=%d` sayilarini yaziyor ve cocuk olarak SADECE
        girdi ifadelerini dokuyor. Temsil ad->ifade degisse bile dump sekli
        korunabilir -> parser_diff paritesi etkilenmez.
        (Olcumu yaparken kendi dumpumda `TANIMLAYICI "x" 7:9` gorup bir an "cikti
        basiliyor" sandim; konumu kontrol edince o satirin `ver x` oldugu cikti.
        Konum dogrulamadan dump okumak yanlis sonuc verir.)
    (2) "codegen sifirdan yazilacak." YANLIS. `erisim_lvalue()` (llvm.c:3028) alan
        adresini + alan IR tipini donduruyor ve DUGUM_ATAMA yolunda zaten
        kullaniliyor (llvm.c:6881). Cikti hedefi icin aynisi kullanilabilir.
  KALAN IS dort yer + self-host aynasi olarak kuyruga yazildi (C3.2-C3.7).
  ⚠ SIRALAMA KARARI VE GEREKCESI: kuyrugun en ustundeki madde C3 ama bu iterasyonda
  KOD YAZILMADI. Sebep: C3 dort C dosyasi + self-host aynasi + gercek-kosum
  fiksturu demek; yarim birakmak agaci kirmizi birakirdi ve "Her iterasyonda SADECE
  bir madde bitir" kuralini bozardi. Bunun yerine C3'un TASARIM ADIMI (C3.1: "C0'dan
  sonra olc") tamamlandi ve boyut duzeltildi — bu da maddenin bir parcasi.
  Sonraki iterasyon D3'e gidiyor (ayni sebeple: kendi icinde tamamlanabilir bir
  kapi ekleme isi), C3 kod adimlari taze baglamla yapilacak.
  HANGI KAPI NEYI OLCTU: degisiklik yok; kapilar D-643 yesilinde.
- 2026-09-28 D-645: eszamanlilik ilkelleri ilk kez KOSTURULUYOR (kilit + semafor).
  OLCUM (once): `stdlib/kilit.kem`, `stdlib/semafor.kem`, `stdlib/bariyer.kem`
  YALNIZ tip-denetleniyordu (calistir_stdlib_check) ve depoda uculeri de kullanan
  TEK DOSYA YOKTU. Yani "calisiyor" iddiasinin arkasinda hicbir kosum yoktu
  (D-425: tip denetimi calismayi kanitlamaz). README ikisini hala "yapilacak" diye
  sayiyordu; D-639'da bayat oldugu olculmustu.
  Runtime tarafi dogrulandi: kdl_kilit_al/birak, kdl_bariyer_bekle vb. semboller
  build/kdl_runtime.o icinde VAR; derleyici yerlesikleri de taniyor.
  YENI KAPI: `calistir_eszamanli_kosum` — 3 kosucu (2 gorev + ana) x 500 artirma
  = 1500; kilit `kilitle`, semafor `semaforda(n=1)` ile. Birlestirme (`cat
  stdlib/X.kem test/...`) stdlib_check idiomu: modu(ller `genel` DEGIL, secili
  import ile tuketilemiyor.
  ⚠ COK TURLU OLMASI OLCUMLE GELDI, SUS DEGIL: S147 (kilit alinmiyor) ham ikilide
  20 turun 14'unde kirmizi verdi = tek turluk kapi bozuk kilidi ~%30 YESIL gecirirdi
  (flaky + yanlis-yesil). 20 turla yakalama olasiligi 1-0.3^20. Ayni gerekce
  drf_gorunurluk'ta (100 tur) uygulanmisti; desen oradan alindi.
  SABOTAJ S147b (kapinin kendisine): kilit 20/20 kirmizi, semafor yesil kaldi ->
  kapi modul bazinda ayristiriyor, toptan degil. make rc=2.
  ⚠⚠ KENDI HATAM — D-446 SINIFI, BU OTURUMDA SILDIGIM KUSURUN AYNISI: kapiyi
  `test_tumu`ya ekledigimi SANDIM. `.PHONY` satiri ayni hedef dizisini icerdigi
  icin `replace(...,1)` yamayi ORAYA koydu; `test_tumu` zinciri DEGISMEDI ve kapi
  KOSMADI — ustelik `rc=0` idi. Dogrulamam da yanlisti (grep yanlis satiri
  sayiyordu). YAKALAYAN SEY: ozet satirlarini onceki yesille diff'lemek — yeni
  kapi eklendigi halde YENI SATIR YOKTU. Bu celiski olmasa "rc=0, tamam" deyip
  gecerdim. Ders kuyruga G2 olarak girdi: yeni kapi eklendiginde ozet satirinin
  kosumda GORUNDUGU ayrica denetlenmeli. Kok neden G3: hedef dizisi `.PHONY` ve
  `test_tumu` diye IKI yerde tutuluyor (D-407 yuzeyi).
  DUZELTME SONRASI DOGRULAMA: satir 2877'de `=== eszamanlilik ilkelleri kosumu:
  2/2 modul (20 tur) ===`, ozet diff'inde TAM BIR eklenen satir.
  KALAN (kuyruga yazildi): D3a bariyer kapisi (sayac yarisiyla olculemez, bulusma
  sekli gerekir), D3c semafor n>1 semantigi — bu kapi n=1 ile yani KILIT GIBI test
  ediyor; semaforun asil iddiasi "es zamanli en fazla n" ve o HIC olculmedi.
  HANGI KAPI NEYI OLCTU: eszamanli_kosum 2/2 (20 tur) . test_tumu TAM rc=0;
  ozette TEK fark yeni kapinin satiri.

- 2026-09-28 D-646: kuyruk 17 ust maddeden 97 ayrintili maddeye acildi.
  NEDEN: alt adimlar ust maddelerin ICINE gomuluydu -> ne sayilabiliyor ne
  siralanabiliyor ne de bagimsiz alinabiliyordu.
  ⚠ 100 ISTENDI, 97 YAZILDI — DOLGU YAPILMADI. Bu oturumda belgeden KOPYALANAN
  ~10 maddenin bayat oldugu olculdu (D-638 PROGRESS.md 5'te 4; D-639 README'nin
  semafor ve stdlib maddeleri; D-642/D-644 kendi boyutlandirmalarim). Uc madde
  uydurup 100'e tamamlamak, tam da bu oturumda elestirdigim seyi yapmak olurdu.
  `[?]` isaretli BES madde bilerek oyle: henuz OLCULMEDILER ve bir kismi zaten
  kapanmis olabilir; ilk isleri dogrulanmak.
  YENI G KUMESI (12 madde) tamamen bu oturumda OLCULEN bulgular: G1 kapi-yuzey
  haritasi (ayni ders UC kez cikti), G2 kapi kayit denetimi (D-645'teki kendi
  hatam), G3 .PHONY/test_tumu ikizligi, G4 harness'ta sabit .exe, G5 checker.kem
  ile codegen.kem parser yuzeyi farki (`çıktı` 4'e 32), G6 P000 envanteri,
  G11 izlenen 0 baytlik `aout=`/`out=1` (kabuk yonlendirme kazasi, referanssiz),
  G12 kokte referanssiz `dz.kem`/`probe_haz.kem`.
  Dagilim: 24 S, 54 M, 13 L, 5 [?], 2 [-] (bende bitmez: A1 push, E1c donanim,
  E3a karar).
- 2026-09-29 D-647: parser tani kodlarindan DORDU eslendi + yontem olculdu.
  ⚠ ONEMLI OLCUM — C0a NASIL YAPILAMAZ: token -> kod eslemesi MEKANIK DEGIL.
  C'de 76 farkli P-kodu var ve tek basina `TOK_TANIMLAYICI` beklentisi baglama
  gore P200/P350/P269/P240/P214/P212/P080/P060/P045/P043/P041/P040/P035/P030/
  P024/P021/P014 olabiliyor. 104 siteyi toplu cevirmek "makul gorunen ama
  DOGRULANMAMIS" kodlar uretirdi — bu oturumda o sinifin bedeli defalarca olculdu.
  YONTEM (olculerek secildi): once hata SEKLINI yaz, C'ye sor, sonra esle.
  Alti sekil sinandi ve IKI SINIF cikti:
    (a) KONUM ZATEN UYUSUYOR, yalniz kod farkli -> mekanik site eslemesi yeter.
        e1 degisken adi (P080), e5 blok suslu (P070+P071), e6 yapi adi (P021).
        Dordu de eslendi ve fiksturlendi (tc48_02/03/04).
    (b) KONUM/SAYI DA FARKLI -> parser DAVRANIS farki, kod eslemesi cozmez.
        Kuyruga C0c olarak girdi; en kotusu `işlev main() -> tam32` govde
        suslusu eksikken self'in `OK` demesi (C uc tani basar) — D-643 tabloyu
        ekledi ama bu sekil tabloya HIC ULASMIYOR, yani HALA sessiz-kabul.
  ⚠⚠ UCUNCU KEZ AYNI GECERSIZ-SABOTAJ HATASI: S148'de `bekle_k(p,"TANIMLAYICI",
  "P080")` desenini degistirdim ama satir sonundaki `;` ve yorum disarida kaldi ->
  `;` YORUMUN ICINE dustu, DERLEME KIRILDI ve kapi sabotaj yuzunden degil
  SOZDIZIMI yuzunden kirmizi oldu ("KEMGU-checker --llvm uretemedi").
  Ayni sinif S146b'de ve D-645'te de olmustu. KURAL (artik yazili): sabotaj EN
  KUCUK BELIRTECI degistirmeli (burada yalniz `"P080"` -> `"P999"` dizgesi) ve
  DERLEMENIN GECTIGI ayrica dogrulanmali; "kirmizi" tek basina kanit degildir.
  S148b (dogru): checker_diff 195/196, make rc=2, derleme temiz.
  HANGI KAPI NEYI OLCTU: checker_diff 193 -> 196/196 . self_driver 154 -> 157/157 .
  test_tumu TAM rc=0; ozette TEK fark checker_diff'in +3 fiksturu.
- 2026-09-29 D-648: govde-suslusu sessiz-kabulu kapandi + EMNIYET AGI.
  KOK KENDI D-643 ONARIMIMIN BOSLUGUYDU: tani tablosunu ekleyip kapiyi TABLOYA
  baglamistim, ama `bekle` DISINDAKI 13 site `p.hata_say`i DOGRUDAN artirip
  tabloya HIC yazmiyordu. Sonuc: sayac > 0 iken tablo BOS kaliyor ve kapi
  ATESLEMIYOR. Yani D-643 "parse hatalari artik gorunur" derken aslinda YALNIZ
  `bekle` uzerinden gelenleri gorunur kilmisti.
  Olculdu: `işlev main() -> tam32` govde suslusu eksikken C uc tani basar
  (P017 2:5, P001 2:5, P001 3:1), self `OK` derdi. Satir 1952 sayaci artiriyordu,
  kayit yoktu.
  ONARIM 1: `parse_hata_kaydet(p, kod)` — sayaci artirir VE tabloya kod+konum
  yazar. P017 sitesi buna baglandi (C parser.c:367 aynasi). Sonuc: self
  `P017 2 5` — C'nin KONUMUYLA BIREBIR. (C'nin uc tanili cascade'i C0b'de.)
  ONARIM 2 — EMNIYET AGI (asil degerli kisim): kapi artik "sayac > 0 AMA tablo
  BOS" durumunu da yakalayip `P000 0 0` basiyor. Kalan 12 kayitsiz site artik
  SESSIZCE GECEMEZ. Amac dogru kodu vermek DEGIL, sessiz kalmamak.
  AGIN DISI OLCULDU: asm blogunda bilinmeyen clause ile kayitsiz site (satir
  1872) tetiklendi -> self onceden `OK`, simdi `P000 0 0` (C: uc P261).
  Ag hicbir kapiyi bozmuyor (checker_diff 196/196, self_driver 157/157) -> bedava
  sigorta. ⚠ Boyle bir dosya KORPUSA KONAMAZ: ag parite degil emniyet.
  ⚠ ACIKLANMAMIS METRIK DEGISIMI ARASTIRILDI: tam kosumda ASan ozeti
  `SIZINTI-MUAF=2` -> `1` dondu. Degisiklige YUKLEMEDIM, olctum: uc ardisik
  ASan kosumu 2 verdi, yani dort gozlemin birinde 1 = NADIR DALGALANMA
  (kanal_mesaj/gorev_temel sizintisi her kosumda manifest olmuyor).
  Ayrica ILGISIZ oldugu KANITLANDI: ASan kapisi yalniz `$KEMGU`yu kullaniyor,
  self-host ikilisine hic dokunmuyor (harness'taki uc "codegen" gecisi YORUM).
  Bu bulgu kuyruga G13 olarak girdi cunku BENIM DOGRULAMA YONTEMIMI etkiliyor:
  ozet satirlarini diff'lemek bu metrikte yanlis-pozitif verebilir.
  HANGI KAPI NEYI OLCTU: checker_diff 196/196 . self_driver 157/157 .
  test_tumu TAM rc=0; ozette TEK fark dalgalanan ASan sayisi.
- 2026-09-29 D-649: panik cascade paritesi — asm dali kapandi.
  OLCUM: C, `&r.deger` sonrasinda kalan `.`, `deger`, `)` belirteclerini BILINMEYEN
  CLAUSE sayip her biri icin P261 basiyor ve BIR belirtec ilerliyor (parser.c:1751)
  -> toplam DORT tani. Self-host'ta o site sayaci artiriyor, tabloya YAZMIYORDU.
  `parse_hata_kaydet(p,"P261")`e baglandi. Sonuc BIREBIR:
    C / codegen.kem: P264 17:26 . P261 17:26 . P261 17:27 . P261 17:32
  Bu, C3'un TANI tarafini tamamen kapatiyor (ozellik yok, o C3'te duruyor).
  ⚠ FIKSTUR GERCEK BIR AYRISMAYI ORTAYA CIKARDI: tc48_05 korpusa girince
  checker_diff KIRMIZI (197/198) ama self_driver YESIL (159/159). Olculdu:
    C            P264 17 26 ...
    checker.kem  P000 17 26 ...   <-- ayrisan
    codegen.kem  P264 17 26 ...
  Sebep: D-643'te asm cikti sitesini eslerken desenim `codegen.kem`e ozgu bir
  satir iceriyordu ve `checker.kem`de TUTMAMISTI — o zaman "ATLANDI (desen yok)"
  diye kaydetmistim. Kayit dogruydu, TAKIBI eksikti. checker.kem:1557 eslendi
  (TANIMLAYICI->P269, SAG_PAREN->P264) -> 198/198.
  BU G5'I DE COZDU VE IDDIAMI YALANLADI: G5'i "checker.kem asm yuzeyi FARKLI
  olabilir" diye `[?]` yazmistim. Yuzeyler AYNI; "4'e 32" sayimim YORUMLARI da
  sayiyordu. Gercek sorun eksik eslemeydi. Ders: `[?]` maddeler gercekten
  olculmeli — benim tahminim de bayat cikabiliyor.
  SABOTAJ S149b: bu kez BASTAN en kucuk belirtec degistirildi ("P264"->"P997") —
  D-647'de yazdigim kuralin ilk uygulamasi. Derleme KIRILMADI, kapi 197/198,
  make rc=2. (Uc gecersiz sabotajdan sonra kural ise yaradi.)
  ⚠ G13 BAGIMSIZ TEYIT: ASan `SIZINTI-MUAF` bu kosumda 1'den 2'ye DONDU. D-648'de
  "nadir dalgalanma" demistim; bu, gerilemeden bagimsiz dalgalandiginin ikinci
  gozlemi.
  HANGI KAPI NEYI OLCTU: checker_diff 196 -> 198/198 . self_driver 157 -> 159/159 .
  test_tumu TAM rc=0; ozette iki fark: +2 fikstur ve dalgalanan ASan sayisi.
- 2026-09-29 D-650: kurtarma davranisi paritesi — 6/7 sekil birebir. VE BIR KOR
  NOKTA KAPANDI (asil bulgu bu).
  ONCE: yedi hata seklinden UCU uyusuyordu. SONRA: ALTISI. Kalan tek sekil
  ust-duzey P001 cascade'i.
  IKI YAPISAL FARK bulundu (kod eslemesi DEGIL):
  (1) PARAMETRE KURTARMASI: C, parametre adi tanimlayici degilse P012 basip
      HEMEN hata dugumuyle DONER (parser.c:269). Self-host DEVAM ediyordu
      (`:` + `parse_tip`) ve C'nin basmadigi FAZLADAN tanilar uretiyordu
      (olculdu: `işlev main -> tam32` icin C uc tani, self BES; sonraki taninin
      sutunu da kayiyordu). C aynalandi -> birebir.
  (2) EKSIK SONSUZ-DONGU KORUMASI — dolayli bir tani kaymasi yaratiyordu:
      `parse_birincil`in son caresi C'de ILERLEMEZ ama self-host'ta ilerlemek
      ZORUNDAYDI, cunku C'nin `parse_blok`taki "token ilerlemediyse zorla ilerle"
      korumasi (parser.c:1164) self-host'ta HIC YOKTU. Sira onemliydi: ONCE
      koruma eklendi, SONRA ilerleme kaldirildi. Korlemesine yapilmadi — once
      C'de koruma olup olmadigi olculdu.
  ⚠⚠ ASIL BULGU — SABOTAJIM GECERSIZ CIKTI VE SEBEBI KAPININ KAPSAMIYDI:
  S150 (korumayi devre disi birak) kapiyi KIRMIZI YAPMADI (make rc=0, 200/200).
  "Demek koruma gereksiz" demek COK KOLAY olurdu. Olctum: yedi seklin hicbiri
  asilmiyor. Sonra DAHA ZORLAYICI girdiler denedim: `{ , }`, `{ : }`, `{ ) }`
  ucu de 5 sn timeout'a takildi -> KORUMA YUK TASIYOR, iddiam dogruydu.
  Gecersiz olan sabotaj degil KAPSAMDI: koruma eklendiginde onu SINAYAN hicbir
  korpus dosyasi yoktu. Yuk tasiyan bir mekanizma tamamen kapsam disiydi.
  Bu, bu oturumda DORDUNCU kez ayni ders (C1, C2, C0, simdi burasi):
  KAPI YESIL OLMASI YUZEYIN OLCULDUGU ANLAMINA GELMEZ.
  KOR NOKTA KAPATILDI: `{ , }` icin bir kod eksigi daha vardi (C P101, self
  P000); eslendi -> parite tam. `tc48_09` korpusa girdi ve dongu korumasini
  SINAYAN ILK dosya oldu. S150b (fikstur varken ayni sabotaj) -> `make rc=124`
  yani kapi ASILDI = gecerli kirmizi.
  ⚠ YAN BULGU: asilan kapi TEMIZ KIRMIZI vermiyor, timeout'a dusuyor. Kuyruga
  G14 olarak girdi (asilan kapi, dusen kapidan kotu teshis edilir).
  ESLENEN KOD SAYISI: D-647'de 4 -> simdi 16 (P080/P070/P071/P021/P261/P269/
  P264/P017/P015/P016/P012/P013/P081/P082/P010/P101).
  HANGI KAPI NEYI OLCTU: checker_diff 198 -> 201/201 . self_driver 159/159 .
  parser_diff 14/14 . test_tumu TAM rc=0; ozette TEK fark +3 fikstur.
- 2026-09-29 D-651: ust-duzey panik cascade'i — C0c TAMAMLANDI.
  Eksik IKI seydi, ikincisi YAPISAL:
  (1) Ust-duzey son care kod KAYDETMIYORDU -> `parse_hata_kaydet(p,"P001")`.
  (2) KURTARMA STRATEJISI FARKLIYDI: self-host TEK BELIRTEC ilerliyordu, C ise
      `parser_panik_sync` (parser.c:175) ile SENKRON BELIRTECE KADAR yutuyor ve
      sonra `;`/`}` ise ONU DA tuketiyor (anahtar kelimeler TUKETILMEZ — yeni
      tanim baslangicidir). C'nin 13 belirteclik senkron kumesi aynalandi.
  Sonuc: `işlev main() -> tam32` govdesiz dosyada
    C / SELF: P017 2:5 . P001 2:5 . P001 3:1  (BIREBIR)
  D-647'de yedi sekilden UCU uyusuyordu; simdi YEDISI + uc asilma girdisi (h1/h2/h4)
  de birebir. Eslenen kod: 16 -> 17 (P001).
  SABOTAJ S151: `panik_senkron` -> tek ilerleme (yani DAVRANISI hedefledi, kod
  dizgesini degil) -> checker_diff 201/202, make rc=2, derleme temiz.
  ⚠ ACIKLANMAMIS METRIK DEGISIMI ARASTIRILDI (D-648'deki refleksin tekrari):
  `check_genis` 134/134 (13 muaf) -> 135/135 (12 muaf). Harness'in KENDI uyarisi
  sebebi soyledi: "tip_alias — MUAF ama artik ESLESIYOR". Olculdu: dosya
  `tip Yas = tam32;` (desteklenmeyen tip takma adi) kullaniyor, C iki `P001`
  basiyor ve self-host artik BIREBIR ayni ciktiyi veriyor. Muafiyet BAYATLADI ve
  silindi (D-419: "muafiyet listesi bir KOR NOKTA ENVANTERIDIR").
  Yani bu onarim, kendisiyle ilgisiz gorunen bir muafiyeti de gecersiz kildi —
  metrik degisimini arastirmasaydim bayat muafiyet listede kalacakti.
  HANGI KAPI NEYI OLCTU: checker_diff 201 -> 202/202 . check_genis 134 -> 135/135
  (muaf 13 -> 12) . parser_diff 14/14 . self_driver 14/14 . test_tumu TAM rc=0.
- 2026-09-29 D-652: bes kod daha eslendi (P350/P150/P122/P060/P040).
  YONTEM UYGULANDI (D-647): sekiz yeni hata sekli yazildi, C'ye soruldu.
  UCU ZATEN PARITEDEYDI (`eşleş`, `için`, `iken` govdesi — `iken` D-650'de eklenen
  blok kodlarindan geciyor). BESI ayristti ve HEPSINDE KONUM UYUSUYORDU, yani
  hepsi (a) sinifi mekanik esleme:
    P350 cesit adi (parser.c:607) . P150 dizi literali ']' (ifade.c:191)
    P122 cagri ')' (ifade.c:549)  . P060 modul adi (parser.c:1069)
    P040 kullan yolu (parser.c:884)
  ⚠ BIR YANLIS ESLEME YAPTIM VE OLCUM YAKALADI: P122'yi `parse_birincil`deki
  UCUNCU `SAG_PAREN`e bagladim; test hala `P000` verdi. Cagri aslinda
  `parse_sonek`te ayristiriliyor (CAGRI dugumu orada uretiliyor). Dogru siteye
  tasindi. Bu, "ayni token beklentisi baglama gore FARKLI kod alir" ilkesinin
  somut ornegi ve toplu cevirmenin neden yanlis kodlar uretecegi'nin kaniti —
  desen ESLESMEK ZORUNDA DEGIL, DAVRANIS eslesmeli.
  ⚠ Ayrica `parse_modul_tanimi` diye aradim, self-host'ta adi `parse_modul`.
  Isim varsayimi da olculmeli.
  Bes fikstur eklendi (tc48_11..15), her biri TEK hata sekli — bir dosyanin
  yalniz ILK cascade'i olculebiliyor.
  SABOTAJ S152 (P150 -> P996, en kucuk belirtec): checker_diff 206/207, rc=2.
  HANGI KAPI NEYI OLCTU: checker_diff 202 -> 207/207 . check_genis 135/135 .
  parser_diff 14/14 . self_driver 14/14 . test_tumu TAM rc=0; ozette TEK fark
  +5 fikstur.
- 2026-09-29 D-653: yeni SESSIZ-KABUL kapandi + bes kod eslendi.
  ⚠ EN DEGERLI BULGU — `satıriçi_asm` ZORUNLU CLAUSE DENETIMI HIC YOKTU:
  `satıriçi_asm { şablon: ... }` (mimari YOK) self-host'ta `OK` geciyordu; C
  P266 ile reddediyor. C kapanis `}`dan SONRA iki zorunlu denetim yapiyor
  (parser.c:1761-1769: P266 mimari, P267 sablon); self-host'ta IKISI DE yoktu.
  Tani konumu `satıriçi_asm` ANAHTAR KELIMESI (kapanis `}` degil) oldugu icin
  `parse_hata_konum(p,kod,sat,sut)` yardimcisi eklendi — `parse_hata_kaydet`
  GECERLI token'in konumunu kullanir ve burada yanlis olurdu.
  DORT MEKANIK ESLEME: P200 (ozellik adi), P030 (sabit adi), P022 (yapi '{'),
  P221 (esles kolu ';'). Ayrica P260 (asm kapanis '}').
  ⚠⚠ IKI DOSYANIN AYNI OLMADIGINI UCUNCU KEZ OGRENDIM: ayni yamayi iki dosyaya
  uyguladim ve `checker.kem` DERLENMEDI — `sab` degiskeni orada YOK, cunku o
  dosya sablonu SAKLAMIYOR, ATLIYOR (`asm_kisit_atla`). `sab_var` bayragi
  eklenip denetim ona baglandi. Kirilmayi kapi yakaladi ("KEMGU-checker --llvm
  uretemedi") ama fiksturlerim `codegen.kem` ile GECMISTI, yani bir an her sey
  yolunda gorunmustu. checker_diff olmasa sessizce ilerleyecektim.
  Bu, D10 (tek-kaynak konsolidasyon) maliyetinin bu oturumdaki UCUNCU olcumu
  (D-634 konak mimarisi, D-649 asm kod eslemesi, simdi burasi).
  IKI YENI DAVRANIS FARKI kuyruga C0d olarak yazildi: `uygula { }` konum kaymasi,
  ve `yapı N { x }` — C UC tani, self YIRMI BIR (en buyuk cascade farki).
  SABOTAJ S153 (mimari zorunlulugunu devre disi birak): checker_diff 211/212,
  rc=2, derleme temiz.
  HANGI KAPI NEYI OLCTU: checker_diff 207 -> 212/212 . check_genis 135/135 .
  parser_diff 14/14 . test_tumu TAM rc=0; ozette TEK fark +5 fikstur.
- 2026-10-03 D-655: Windows CI tanisi — yutulan stderr acildi.
  CI ILK KEZ D-634 SONRASI HALI OLCTU: Linux YESIL, Windows KIRMIZI.
  Sucu commit'lere yuklemeden once OLCTUM: 27 Eylul'deki yesil kosum
  (a3382b9) Windows'ta `test_tumu`yu gercekten kosmus ve gecmis (adim
  duzeyinde dogrulandi) -> gerileme 38 commit'in icinde. "Yesil bir
  iddiadir" (D-486) kuralinin CI karsiligi.
  TEK KIRILMA: `Makefile:651 build/codegen.ll Error 1`, `lambda_v2`den hemen
  sonra (codegen_diff'in on kosulu). AMA SEBEP LOGDA YOKTU: kural
  `2>/dev/null` tasiyordu. Ayni sinif D-636'da `kemgu_self` icin kapatilmisti;
  BU IKI KURAL GOZDEN KACMISTI (codegen.ll + codegen$(EXE)).
  ⚠ ICERIK SEBEP DEGIL — olculdu: CI'nin konak.kem'i (x86_64 /
  x86_64-pc-windows-gnu) birebir uretilip yerel kosuldu -> `--llvm` rc=0,
  stderr BOS. Yani sorun uretilen MODULUN ICERIGI degil, Windows ortami.
  ONARIM: iki kuralda stderr ARTIK GORUNUR + `selfhost/konak.kem` varligi/
  bosluğu AYRICA ve GURULTULU denetlenir + basarisizlikta kismi `.ll` SILINIR
  (bayat artefakt tuzagi). Konak modulu bu kuralin ILK tuketicisidir (ondan
  once onu okuyan kapi YOK), yani uretim sessizce dusse ilk belirti tam
  burada cikar.
  ⚠⚠ ILK HIPOTEZIM YANLISTI VE ONU OLCUMLE CURUTTUM: "arm64 ikiz fiksturleri
  mimari-atlamasi olmayan kapilarda x86'da reddedilir" demistim. Kusur
  GERCEK (A3 olarak kuyruga girdi) ama BUGUNKU KIRMIZININ SEBEBI DEGIL:
  o dort kapi (yapi_diff 46 · check_genis 49 · bolge_operand 56 ·
  asan_denetim 71) test_tumu sirasinda codegen_diff'ten (42) SONRA geliyor,
  kosum onlara hic ulasmadi. Siralamayi olcmeden "guclu hipotez" demek
  yanlis kokdu.
  ⚠ SABOTAJ S155 GECERSIZDI: `konak.kem`i elle bosalttim, `FORCE` bagimlisi
  oldugu icin make onu YENIDEN URETTI -> sessiz kaldi. Sessizlik once
  SABOTAJI supheli kilar (D-402). S155b uretimin KENDISINI bos cikti verecek
  sekilde sabote etti -> `🔴 selfhost/konak.kem YOK ya da BOS (ARCH=arm64
  TRIPLE=aarch64-unknown-linux-gnu)`, make rc=2, kismi `.ll` silindi.
  HANGI KAPI NEYI OLCTU: temiz yol `build/codegen` rc=0 (ll 2.498.532 bayt,
  konak 5 satir); S155b rc=2 ve tani ADIYLA basiliyor.
- 2026-10-03 D-654 + D-656 + D-657: parser tanilari, hedef sifirlanmasi, yapi
  dongu korumasi. ⚠ TEK COMMIT, ve bu BILINCLI: uc is ayni iki dosyada birikti
  ve TAM TAKIM ucunu BIRLIKTE dogruladi. Ayirmak, hicbiri tek tek olculmemis
  iki ara durum commit'lemek olurdu — bu depoda olculmus tek commit, kozmetik
  olarak bolunmus olculmemis commit'lerden iyidir.

  D-654 (C0d): `uygula { }` konum kaymasi + `yapı N { x }` cascade'i kapandi.
  Kok TEK yerdeydi: `parse_tip`in son caresi kod KAYDETMIYOR ve BIR BELIRTEC
  ILERLIYORDU (C `ifade.c:955` ilerlemez). Ilerleme kalkinca self'in 21 tanisi
  3'e indi ve konumlar C ile hizalandi; kalan P018/P019/P020 mekanik esleme +
  C'nin erken-donus + panik-senkron deseni.

  D-656 (A3'un yerine): IKI gercek kusur.
  (1) MODUL BIRLESTIRMESI HEDEFI SIFIRLIYORDU. `ayr_olustur` varsayilani
      `hedef_mim: "x86_64"`; birlestirme `p`yi TAZE bir `Ayr` ile degistirip
      hedefi geri YUKLEMIYORDU. Olculdu (ARM64, `ana_alias`): self
      `x86_64-pc-windows-gnu`, C `aarch64-unknown-linux-gnu` = SESSIZCE YANLIS
      HEDEF. ⚠ D-634 bunu YARATMADI, GORUNUR KILDI (onceden varsayilan da
      sabit x86_64 oldugu icin sifirlama ayni degere donuyordu).
      ⚠⚠ KAPI NEDEN GORMEDI: `clang` modul uclusunu KONAGINKIYLE EZIYOR
      (`-Woverride-module`) ve harness stderr'i yutuyor -> link gecer, cikis
      kodlari esit, kapi yesil. DAVRANISSAL KIYAS UCLUYE KORDUR.
      `modul_codegen` artik ucluyu YAPISAL olarak karsilastiriyor.
  (2) AS001'in CODEGEN KATMANI YOKTU. C'de denetim iki yerde (tip kontrolu +
      codegen, ikincisi `--tip-atla`dan BAGIMSIZ ve olumcul). Self-host yalniz
      `th_kod`a yaziyordu -> `--tip-atla` ile YABANCI MIMARI asm'i hedef module
      basiyordu (olculdu: C rc=1/0 define, self rc=0/2 define). `checker.kem`de
      ayni sifirlama OLMADIGI ayrica olculdu.

  D-657 (yol ustunde, ayri kusur): YAPI GOVDESI SONSUZ DONGU KORUMASI YOKTU.
  C'de VAR (`parser.c:573` zorla-ilerle + `PARSER_MAX_HATA`), self-host'ta
  YOKTU. D-650 ayni korumayi `parse_blok`a eklemisti; YAPI gövdesi AYRI bir
  dongudur ve gozden kacmisti. Ayrica C'nin AYNI-KONUM-AYNI-KOD tekrar
  bastirmasi (`PARSER_MAX_AYNI_HATA = 3`, deger C'den OKUNDU) self-host'ta HIC
  YOKTU — C'nin kendi yorumu bu senaryoyu adiyla tarif ediyor.
  YUK TASIDIGI OLCULDU: koruma kaldirilinca `yapı N { , }` ve `yapı N { ) }`
  — ARDINDAN BIR TANIM GELDIGINDE — timeout'a takiliyor; tek basina yapi
  ASILMAZ. Korumali halde ikisi de C ile BIREBIR alti tani veriyor.
  Fikstur `tc48_23` eklendi (yapi gövdesi korumasini sinayan ILK korpus dosyasi).

  ⚠⚠⚠ ASIL SURECI DERSI — SABOTAJ ARTIGI BUTUN BIR TESHISI YANLIS KOKE
  GOTURDU. `selfhost/checker.kem` S154 sabotajini (D-654'u GERI ALAN satir)
  OTURUMLAR BOYUNCA tasidi: arka plan gorevi geri alma satirina gelmeden
  kesilmisti. Bir kez tespit edip kaldirdim, GERI GELDI ve fark etmedim.
  Sonuc: `kemcheck` 26.6 GiB'e cikip OOM-killer tarafindan oldurüldu (cekirdek
  gunlugunde kayitli, cgroup `app-com.anthropic.Claude-*`), TAM TAKIMI asagi
  cekti ve bunu "D-654 asilmaya yol aciyor" diye teshis ettim — YANLIS KOK.
  Temiz agacta D-654 sorunsuz. KURAL: her olcumden ONCE `grep -rn "SABOTAJ S"`
  ile TUM depoyu tara; "bir kez kaldirdim" yeterli degil.

  HANGI KAPI NEYI OLCTU: checker_diff 212 -> 215/215 (0 muaf) .
  modul_codegen 27/27 (0 atlandi, 0 muaf) . test_tumu TAM rc=0, 0 hata;
  ozette TEK fark +3 fikstur.
  SABOTAJ: S156 (hedef geri yuklemesini kaldir) -> modul_codegen rc=2, ucluyu
  uc dosyada adiyla bildirdi. D-657'nin yuk tasidigi ayrica olculdu (yukarida).
- 2026-10-03 D-658: Windows CI kirmizisinin KOKU — uretilen modul mojibake.
  D-655 stderr'i actigi icin tani CI logunda GORUNUR oldu:
    hata[P001]: ust duzey tanim bekleniyor
      --> selfhost/konak.kem:5:11
    5 | genel iÅŸlev konak_triple() -> metin { ver "x86_64-pc-windows-gnu"; }
  `iÅŸlev` = `işlev`in CIFT KODLANMIS hali (0xC5 0x9F baytlari Latin-1 sanilip
  yeniden UTF-8'e cevrilmis). Makefile kurali `printf` ile TURKCE KEMGU KAYNAGI
  uretiyordu ve Windows kabuk/printf katmani baytlari bozuyordu. Lexer anahtar
  kelimeyi tanimayinca P001 x4, IR uretilmiyor, `build/codegen.ll` dusuyor.
  ⚠ YEREL OLARAK UREMEYEN BIR KUSUR: ARM64/Linux'ta `printf` dogru bayt uretir,
  bu yuzden 38 commit boyunca gorunmedi. CI'in tek basina yakalayabildigi sinif.
  ONARIM — TURKCE BAYTLAR ARTIK MAKEFILE'DAN HIC GECMIYOR: `selfhost/konak.kem.in`
  sablonu depoya girdi (Turkce git'ten gelir), kural yalniz ASCII yer tutuculari
  (`@ARCH@`/`@TRIPLE@`) ikame eder. `.gitattributes`'a `*.kem.in text eol=lf`.
  IKI GURULTULU DENETIM eklendi (ikisi de KOSULSUZ, D-486):
    (1) yer tutucu ikame edilmemisse -> adiyla hata
    (2) uretilen modul `--check`ten GECMELI -> kodlama bozulmasi KAYNAGINDA
        yakalanir, `codegen.ll` dustugunde P001 gurultusu olarak DEGIL.
  ⚠ `kemgu` kurala ON KOSUL olarak eklendi: (2) onu kullanir ve kosulsuz olmali;
  dongu yok (kemgu yalniz src/*.c'ye bagli).
  ⚠ Recipe yorumlari `@` tasimadigi icin her kosumda ekrana basiliyordu; kural
  disina tasindi (uretim artik SESSIZ).
  SABOTAJ S157: sablon bilerek cift-kodlandi -> `🔴 konak.kem GECERSIZ KEMGU
  uretildi (kodlama bozulmasi?)` + `hata[P051]`, make rc=2.
  HANGI KAPI NEYI OLCTU: uretilen islev satirlari birebir ayni (davranis
  esdegerligi) . ct_bariyer 14/14 (0 atlandi) . `ana_alias` uclusu
  `aarch64-unknown-linux-gnu` (D-656 korundu).
  ⚠ GERCEK DOGRULAMA CI'DA: bu kusur yerelde uremez.
- 2026-10-03 D-659: KUYRUK BUTUNLUGU — bayat/cift kayitlar temizlendi.
  Kuyrugun kendisi bir KAYITTIR, yani bayatlayabilir (D-406). Tarandi ve UC
  kusur bulundu, hepsi KENDI duzenleme artigim:
  (1) A1 CIFT KAYITLI: biri "23 commit bekliyor; bu makinede GitHub yazma
      yetkisi yok" diyordu. OLCULDU: `ahead=0`, `gh auth` OK -> bayat girdi
      silindi, guncel `[~]` olan kaldi.
  (2) C0c CIFT KAYITLI (`[x]` + `[~] C0c-eski`) -> ikincisi silindi.
  (3) C0d hala `[ ]` idi; D-654'te KAPANMISTI. Olculdu ve kapatildi:
      `uygula { }` -> C/self `P011 2 8` . `yapı N { x }` -> C/self
      `P019 1 13 P011 1 13 P020 1 13`.
  AYRICA 25 KAPALI MADDE Sirada'da duruyordu (protokol: kapanan madde Gunluk'e
  tasinir). Silmeden ONCE guvenlik kosulu olculdu: her blogun D-referansi
  Gunluk'te VAR MI. 25'inin 25'i kapsanmis -> silindi; Gunluk BIREBIR AYNI
  kaldigi `diff` ile dogrulandi (132 girdi).
  SONUC: kuyruk 104 satirdan 99 maddeye indi ve sayilar artik DURUST —
  95 acik + 4 surmekte. Oncesinde "97 acik" deniyordu ama 25'i kapaliydi.
  ⚠ SABOTAJ YOK VE GEREKCESI: bu artim CALISTIRILABILIR davranis
  degistirmiyor, kayit duzeltiyor. Dogrulama buna gore secildi: cift kalmadigi,
  sayimin dogrulugu, Gunluk'un bayt-esitligi ve her silinen blogun Gunluk'te
  kapsandigi.
  ⚠⚠ OLCUM ARACIM BU TURDA UC KEZ YANILDI (D-500 listesinin tekrari):
  (a) `[ ]` icindeki BOSLUK awk alanlarini kaydirdi -> cift tespiti `]` dondu
      ve A1 ciftini GORMEDI; regex'e gecildi.
  (b) `D-654:` (iki nokta ile) arayinca Gunluk'te BULUNAMADI ve "indeks bozuk"
      sanildi; girdi `D-654 + D-656 + D-657:` biciminde, hepsi greplenebiliyor.
      OLMAYAN bir kusuru onarmaya BASLAMADAN once dogrulandi.
  (c) D-referansi denetimi yalniz ILK satira bakiyordu -> `D-631` devam
      satirinda oldugu icin "kapsanmamis" gorundu.
- 2026-10-03 D-660 (G14): `checker_diff` per-dosya zaman asimi + bellek tavani.
  NEDEN SIMDI: bu oturumda bir asilma TUM KAPIYI asti ve `make` yalniz `rc=124`
  dondu — hangi DOSYA astigi gorunmuyordu. Dahasi surec 26.6 GiB'e cikip
  OOM-killer tarafindan oldurüldu ve OTURUMU asagi cekti.
  EKLENEN: `kos()` sarmali (per-cagri `timeout` + `ulimit -v`), 124/137 icin
  ADIYLA kirmizi, ve ORACLE-CIKTI denetimi.
  🔴 YOL USTUNDE KENDI ONARIMIM YANLIS-YESIL URETTI VE SABOTAJ YAKALADI:
  tavan IKI TARAFI da dusurunce ikisi de BOS cikti veriyor, `diff` esit diyor
  ve kapi GECIYOR (S159: `KAP_KB=20000` -> 215/215 YESIL). Onarim: C derleyici
  her gecerli korpus dosyasi icin ya `OK` ya tani basar, yani BOS cikti
  "oracle kosmadi" demektir -> ayri ve gurultulu red (D-547'nin disiplini).
  ⚠ BELLEK TAVANI BIR TESPIT MEKANIZMASI DEGIL, HASAR SINIRLAMASI — olculdu:
  tavani asan surec `rc=1` verir (tahsis hatasi), 137 DEGIL. Ayrica bugunku
  korpus tavani ASMIYOR (C derleyici 20 MB altinda bile dogru cikti veriyor)
  -> tavan ATESLENMEYEN bir sigortadir ve kapi onu GATE'LEMIYOR. D-510'un
  disiplini geregi boyle kaydedildi; "gate'lendi" diye yazmak yanlis olurdu.
  ⚠⚠ IKI GECERSIZ SABOTAJ: S158 `$KEMGU`yu uyuyan bir shim yapti ama harness
  `kemcheck`i DE `$KEMGU` ile kuruyor (satir 29) -> yapim kirildi, dongu hic
  kosmadi (kapiyi degil YAPIMI olctu). S159 yukarida anlatildi ve kapinin
  gercek bir kusurunu acti. GECERLI: S160 (`KAP_SN=0.01`) -> dosyalar ADIYLA
  `ASILDI/OLDURULDU`, rc=1.
  HANGI KAPI NEYI OLCTU: checker_diff 215/215 (0 muaf) temiz yolda; S160'ta
  kirmizi. Kalan yedi harness G15 olarak kuyruga girdi.
- 2026-10-03 D-661: `timeout` yetenegi OLCULUR, varsayilmaz.
  D-660 POSIX sekilli bir sinir ekledi (`timeout`/`ulimit -v`) ve bu depo POSIX
  varsayimindan ALTI KEZ isirildi (D-560..D-566). `timeout` YOKSA `kos` 127
  doner ve HIC CIKTI YAZMAZ -> D-660'in "oracle cikti uretmedi" denetimi
  WINDOWS'TA YANLIS KIRMIZI verirdi. Yetenek bir kez olculur
  (`command -v` YETMEZ: `timeout 5 true` de kosulur, D-551'in dersi); yoksa
  sinirsiz kosulur ve bu ACIKCA bildirilir.
  ⚠ KENDI KURALIMI CIGNEDIM: uyari mesajinda cift tirnak icinde BACKTICK
  kullandim — o KOMUT IKAMESIDIR (D-548/D-549'da kayitli) ve `bash -n` onu
  GECERLI sayar. Yalniz satiri okumak yakaladi. Yorum satirlarindaki backtick
  zararsiz (calistirilmiyor); temizlenen yalniz `echo` satiri.
  ⚠⚠ SABOTAJ OLCUMUM IKI KEZ GECERSIZDI: (a) PATH'ten `timeout`u cikarmaya
  calistim ama `/usr/bin` PATH'te kaldi -> hic kaldirilmadi; (b) shim'i PATH'in
  basina koydum ama DIS sarmalayici `timeout 1800` DE shim'e dustu -> harness
  hic kosmadi (rc=1) ve bir an "geri-dusus bozuk" sandim. Mutlak yol
  (`/usr/bin/timeout`) ile dogru olculdu.
  HANGI KAPI NEYI OLCTU: S161 (probe basarisiz) -> uyari ADIYLA basildi,
  checker_diff 215/215, rc=0 (kapi OLCMEYE DEVAM ETTI). timeout VARKEN S160
  zaten kirmiziydi (D-660). Yani her iki yol da gate'li.
- 2026-10-04 D-662 (G15 kismi): asilma/OOM korumasi iki kritik kapiya yayildi.
  D-660 `checker_diff`i kapatmisti; ayni sinif self-host ikilisini korpus
  uzerinde kosturan HER kapida acikti. Ikisi kapatildi:
  - codegen_diff: iki `--llvm` cagrisi da (`$KEMGU` oracle + `$CODEGEN` aday)
    `ir_uret` ile sarildi (timeout + ulimit -v). ASIL risk self tarafinda.
    SABOTAJ S162 (self --llvm uyut, KAP_SN=2) -> dosyalar ADIYLA `ASILDI/
    OLDURULDU rc=124`, make rc=124. S163 (timeout probe basarisiz) -> uyari
    adiyla, 179/179, rc=0 (kapi OLCMEYE DEVAM ETTI).
  - selfhost_driver: LLVM yolu ZATEN codegen_diff'e delege ediyor -> D-662 orada
    OTOMATIK yayildi; kalan acik `byte_modlari`nin --check/--parse/--token
    dongusuydu (OOM'u yaratan tam da --check parser dongusu). Sarildi; asilma
    tespiti her-argumanda-uyuyan shim ile dogrulandi (fail=1, rc=124).
  D-661 dersi tasindi: `timeout` yetenegi OLCULUR, yoksa sinirsiz + uyari.
  Backtick tuzagina DUSMEDIM (iki harness da `echo "..."` icinde 0 backtick).
  ⚠⚠ IKI OLCUM HATASI, IKISI DE KENDI SHIM'IMDE (harness'ta DEGIL):
  (a) `KONAK_MIM` gecirmeyi unuttum -> harness erken cikti ("kapi KOSMADI"),
      sabotaji hic olcmedi; kapiyi degil ON KOSULU olctum.
  (b) ilk uyku-shim'i YALNIZ argumansiz uyuyordu (`sleep 30`); driver onu
      `--check "$x"` ile cagirinca hemen cikti -> S164 sessiz. Her-argumanda
      uyuyan shim ile tekrarlandi.
  Her iki durumda da sessizlik once SHIM'I/PREMISI supheli kildi (D-402/D-500).
  HANGI KAPI NEYI OLCTU: self_driver TUM MODLAR + FIXPOINT ✓, codegen_diff
  179/179, test_tumu TAM rc=0 (ozet onceki yesille BIREBIR — harness-ici
  degisiklik sayilari degistirmez).
- 2026-10-04 D-663 (G15 tamam): asilma korumasi kalan uc riskli kapiya yayildi.
  ⚠ "KALAN BES" OLCULDU VE UCE INDI: onceki tick'te besini de "kalan" diye
  yazmistim; bu kez her birinin self-host ikilisini KORPUS UZERINDE kosturup
  kosturmadigini olctum. UC acik (baremetal_diff/surucu_diff/yapi_diff —
  `$CODEGEN` ile), IKI kapali (parser_diff/lexer_diff — yalniz `$KEMGU` oracle).
  Korlemesine sarmak yerine olcmek, gereksiz iki sarmayi onledi (D-515).
  Ucu de `codegen_diff`le ayni `ir_uret` desenini kullaniyor; uyarlandi.
  SABOTAJ: S165 (yapi_diff self --llvm uyut) -> 149 dosya ADIYLA ASILDI,
  rc=124. S166 (baremetal) -> 4 birim, rc=1, 0/5. surucu_diff ayrica
  sabote EDILMEDI: `ir_uret` birebir ayni ve iki kapida kanitlandi.
  ⚠ ONCEKI IKI SHIM HATASINA DUSMEDIM: her-argumanda uyuyan shim + mutlak
  `/usr/bin/timeout` dis sarmalayici + backtick yok (D-661/D-662 dersleri).
  HANGI KAPI NEYI OLCTU: yapi_diff 160/160, surucu_diff 16/16, baremetal_diff
  5/5 temiz yolda; test_tumu TAM rc=0 (ozet birebir — harness-ici degisiklik).
- 2026-10-04 D-664 (olcum): C1a/C1b KAPANDI, C1c koku netlesti.
  Kuyrukta uc "annotasyonsuz basatici" maddesi vardi; ucu de olculdu.
  C1a (CAGRI basaticisi): dort sekilde parite (2^33 donus exe 42/42, zincir,
  metin, kesirli64) -> KAPANDI, D-640'tan beri bayatmis.
  C1b (aritmetik/tanimlayici): iki sekil de chk+exe paritede -> KAPANDI.
  C1c (p7): HALA ACIK ama kok yanlis tarif edilmisti. `y: tam64 = x+2^33`
  (x:tam32) icin C IKI tani basar (T043 1:70 literal + T001 1:44 deyim), self
  yalniz T001. Olculdu: literal TASMASI DEGIL (2^33 < 2^64, tasti=0); C'nin
  D-021 yeniden-tipleme yolu toplami tam32 baglamina sokunca `8589934592`
  tam32'ye sigmiyor -> T043. Self'in T043 makinesi VAR (D-632) ama yalniz
  2^64+ doymasi icin; D-021 dalinda kontrol YOK. DAVRANIS degil TANI farki
  (exe 42/42). Onarim ayri is olarak C1c'de guncellendi; sahte-pozitif riski
  yuksek (D-632 yorumu: baglamsiz yolda raporlamak sahte T043 uretir).
  SABOTAJ YOK: bu bir OLCUM iterasyonu, calistirilabilir davranis
  degismedi; dogrulama parite tablolari + tani konumlarinin kaynaga
  (tip_kontrol.c:2685/2963) baglanmasiyla yapildi.
- 2026-10-04 D-665/D-666/D-667: DIS INCELEME BULGULARI — birinci asama.
  Bir dis inceleme (`446ebb7` uzerinde) somut iddialar ortaya koydu; HEPSI kodla
  karsilastirildi ve kontrol edilebilir olanlarin TAMAMI dogru cikti (bolge sayac
  yarisi, rho_serbest, CI ozeti, README bayatligi, Lean cross-step HB, semafor/
  bariyer test boslugu). Cogunu bu oturumda KACIRMISTIM.

  D-665 BOLGE SAYACLARI ATOMIK (yalniz HOSTED). Duz `uint64_t ++` idi; gorevler
  gercek OS thread'leri oldugu icin artirmalar kayboluyordu. Bu makinede olculdu:
  duz sayacla olustur 63.841 / 200.000 (incelemenin x86 olcumunden cok daha agir;
  zayif bellek + cok cekirdek). `kdl_bolge_bakiye` = sizinti TANIGI -> gozlem
  mekanizmasinin KENDISI yanlis sonuc veriyordu.
  ⚠ BARE-METAL DUZ BIRAKILDI, bilincli: aarch64'te MMU kapaliyken LDXR/STXR
  tanimsiz olabilir (D-490); bare-metal tek cekirdek, yaris olusmaz.
  YENI KAPI `calistir_bolge_sayac_yarisi`: GERCEK runtime'a karsi duz sayim +
  ThreadSanitizer. DEPODAKI ILK TSan KAPISI. ASan bu sinifi GORMEZ — yarisin
  bugune dek fark edilmemesinin sebebi tam buydu.
  SABOTAJ S168 (header'da atomik dali kapat): 3/3 tur kayip + TSan rc=66,
  yaris `kdl_bolge.c:84` (sayac ++) satirinda. Ayrica S167 (-D__STDC_NO_ATOMICS__,
  dosya duzenlemeden) ayni sonucu verdi.

  D-666 GOREV OLUSTURUCULARI calloc. Eski `kdl_gorev_basla_i32` D-309'un ekledigi
  `rho_serbest`i baslatmiyordu; inceleme bunu STATIK buldu, ben MemorySanitizer
  ile CALISAN KANITA cevirdim: use-of-uninitialized-value @ kdl_runtime.c:1301.
  Bu API'nin SIFIR cagirani var (olu) ama kok bir SINIFTI: sonradan eklenen alan
  eski olusturucuda unutuldu. Tek satir yalniz bu ornegi kapatirdi; iki
  olusturucu da `calloc` -> gelecekteki alanlar da sifirla baslar.
  YENI KAPI `calistir_gorev_msan`: DEPODAKI ILK MSan KAPISI.
  SABOTAJ S169 (eski olusturucuyu malloc'a dondur) -> MSan rc=1, kapi rc=2.
  ⚠ kdl_runtime.c GECERLI UTF-8 DEGIL -> bayt modunda, saf ASCII ile duzenlendi.
  Ilk denememde bayt literaline Turkce karakter koydum; python AYRISTIRMADA
  dustu, dosyaya HICBIR SEY yazilmadi (yedekle cmp ile dogrulandi).

  D-667 CI OZETI GERCEK SONUCLARA BAGLANDI. Iki isin ozeti `if: always()` ile
  SABIT "OK"/"gecti" basiyordu — onceki adim dusse bile. Artik
  `steps.<id>.outcome`dan kuruluyor; atlanan adim ATLANDI gorunur. "rc=0 bir
  iddiadir" (D-486) dersinin CI karsiligi; kapilari bu kadar sertlestirirken
  insanin okudugu ozetin hicbir sey olcmedigini GORMEMISTIM. YAML `safe_load` +
  her outcome referansinin kendi isinde tanimli oldugu dogrulandi (D-554).

  README: dort bayat iddia duzeltildi — uc gorev-bolgesi pasaji (D-309 kosullu
  serbest birakma HEM host HEM KEMGU-OS'ta var; `kem_gorev.kem:1016` OLCULDU,
  varsayilmadi) + semafor/bariyer (D-456'da yapilmis, "gelecek is" yaziyordu).

  HANGI KAPI NEYI OLCTU: test_tumu TAM rc=0; ozette fark TAM OLARAK iki yeni kapi
  (ikisi de KOSTU ve GECTI — D-645'in "eklendi ama kosmadi" tuzagi bu diff ile
  dislandi). Kalan: CI ozetinin GitHub'da gercek sonucu gostermesi (push sonrasi).
- 2026-10-04 D-668 + D-669: dis incelemenin ikinci asamasi — semafor n>1 ve bariyer.
  D-668 SEMAFOR n>1. D-645 semaforu yalniz n=1 (kilit gibi) olcuyordu; asil iddia
  "ayni anda en fazla n" HIC olculmemisti. Test IKI iddiayi ayri olcer:
  tepe<=N (guvenlik, cikis 1) ve tepe>=2 (anlam, cikis 2). Ikincisi olmasa KILIT
  GIBI davranan bozuk semafor "en fazla 2" testini trivial gecerdi (D-425).
  ⚠⚠ KENDI TEST TASARIMIMDA KUSUR, SABOTAJ YAKALADI: ilk surum `icerde`/`tepe`
  sayaclarini IKINCI BIR n=1 SEMAFORLA koruyordu. S170 (semafor beklemesin)
  ayni kodu paylasan IKI semaforu birden bozdu -> koruma coktu, test "n asildi"
  (1) yerine "olcum guvenilmez" (3) dedi: KIRMIZIYDI AMA GUVENLIK DALINI
  KANITLAMADI. Koruma ayri bir runtime ilkeline (ham `kilit_*`, KdlKilit)
  tasindi. Sonra: S170b -> 5/5 cikis 1 (n asildi), S171b (daima 1 izin) ->
  5/5 cikis 2 (kilit gibi). DERS: OLCUM ARACI OLCULENDEN BAGIMSIZ OLMALI.
  D-669 BARIYER. "Herkes gelmeden kimse gecmiyor" + yeniden kullanim. Kademeli
  gecikme (id ile orantili) erken gecisi GOZLENEBILIR yapar. 5 TUR:
  S172 (bariyer beklemesin) -> 5/5 cikis 1. S173 (YALNIZ yeniden kullanim
  bozuk: `vardi` sifirlanmiyor) -> 5/5 cikis 1.
  ⚠ COK-TUR YUK TASIYOR, OLCULDU: ayni S173 TEK TURLUK varyantta 5/5 YESIL
  (yanlis yesil) geciyor — D-456'nin kaydettigi nesil-sayaci kusuru tek turla
  GORUNMEZ. Test yorumundaki iddia boylece olculerek dogrulandi.
  HANGI KAPI NEYI OLCTU: eszamanli_kosum 2/2 -> 4/4 modul (20 tur);
  test_tumu TAM rc=0, ozette TEK fark bu satir.
  CI (faz-1, 0250d07): Linux'ta TSan + MSan GERCEKTEN kostu (atlanmadi); ozet
  adimi gercek `outcome`lari aldi (S_TUMU: success ...).

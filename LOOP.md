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
- [ ] test/perf tabanini kapiya cevir: bench1/bench2 icin zirve bellek esigi olcup regresyon kapisi ekle (D-506'nin 17x kazanci sessizce kaybolmasin).
- [ ] `lean_sorry` kapisina "lake build KOSULMADI" uyarisini SERT hale getirmeden once: lean/lake kurulu mu diye olc, kuruluysa `calistir_lean_tam` hedefi ekle (test_tumu'ya BAGLAMA).
- [ ] ARM64 fiziksel donanim kontrol listesi: D-490'in `smp_queue_arm` sabotajini gercek donanimda tekrarlama adimlarini belgeye yaz (QEMU'da anlamsiz oldugu olculdu).
- [ ] D-520 (cok-segmentli `kullan` T041'i atliyor): en muhafazakar adim = mevcut davranisi FIKSTURLE kilitle + `::`->`/` cevirisinin gocun ilk adimi oldugunu belgeye yaz. Gocu BASLATMA.
- [ ] Kanal omru: `kdl_kanal_serbest` olu kalmaya devam (D-515 kapali). Bunun yerine "kanal tutan tum gorevler birlestirildi mi?" sorusunu olcen bir ARASTIRMA notu yaz, kod yazma.

## Gunluk
- 2026-08-31 D-523: `Dizi<T>` iceren kullanici yapisi goreve yakalanirsa L002 (C + checker.kem + codegen.kem). Skaler alanli yapi MUAF. checker_diff 169/169, ct_bariyer 14/14, codegen_diff 162/162, drf_test 54/54.
- 2026-08-31 D-524: aritmetik tasma sabitlendi — yeni kapi `calistir_tasma` (12 olcum, C+SELF): -O0/-O2 ayni VE IR'da nsw/nuw yok. Sabotaj S99 (nsw enjekte) -> 3 dosya kirmizi, rc=2. Dil degisikligi YOK.
- 2026-08-31 D-525: D-510'un dali ULASILABILIR cikti (literal argüman/cesit payload/ic ice literal; korpusta 0 iz). Fikstur cg_bilinmeyen_eleman eklendi, 6/6 GLOBAL. bolge_operand 165/165, codegen_diff 163/163. Ilk enstrumantasyon hic uygulanmamisti — sahte 'ulasilamaz' sonucu yakalandi.
- 2026-08-31 D-526: K1 kapandi — `-> mantiksal` self-host'ta da i1 (yalniz donus konumu; ll_tip degismedi). Kok: tip_genislik'te i1 YOKTU -> sext i32->i1 = gecersiz IR. yapi_diff muafiyet 27->18, codegen_diff 163/163, llvm_test 286/286. Sabotaj S100 -> 154/163.
- 2026-08-31 D-527: kanal ABI'si bare-metal'de olculuyor (runtime/kem_kanal_abi.kem, ciplak+ARM64). Depoda kanal kullanan runtime satiri YOKTU. baremetal_diff 4/4 -> 5/5. S101 gecersizdi (kaynak degisikligi iki derleyiciyi birden etkiler); S102 (tek tarafi boz) -> rc=2.
- 2026-08-31 Altyapi: dongu artik dis kabuk betigiyle surulyor (`loop.sh`, en fazla 200 tur, tur basi 45 dk zaman asimi). Oturum-ici cron isleri (576a86e0 30dk, ea3657f5 5dk) SILINDI — ikisi ayni anda etkindi ve ayni LOOP.md maddesini es zamanli isleyebilirdi; bu, D-297'de kayitli "ayni testin iki es zamanli kosumu birbirini ezer" sinifinin ta kendisi. Betik seri kosar: bir tur bitmeden digeri baslamaz. Durma kosullari: sifir-disi cikis kodu, ciktida "DURDU:", ya da Sirada'nin bosalmasi. `loop.sh` ve `loop.log` .gitignore'da — birincisi yerel surucu, ikincisi kosum ciktisi; ikisi de depo icerigi degil.
- 2026-08-31 Durum: test/perf maddesine BASLANMADI (yarim is birakmamak icin bilerek). Sirada'da 5 madde duruyor. Son yesil olcumler: codegen_diff 163/163, yapi_diff 147/147 (18 muaf), checker_diff 169/169 (0 muaf), baremetal_diff 5/5, llvm_test 286/286, tasma 12/12, ct_bariyer 14/14.

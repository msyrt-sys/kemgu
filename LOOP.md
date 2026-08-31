# LOOP
## Kurallar
- Bana hiçbir şey sorma. "Onaylıyor musun", "şunu öneriyorum", "devam edeyim mi", "sıradaki işe geçeyim mi" yazma. Öneri üretip bekleme; seç ve uygula.
- Belirsizlikte en muhafazakar secenegi kendin sec, gerekcesini Gunluk'e yaz, devam et.
- Her iterasyonda SADECE bir madde bitir. Cikti en fazla 5 satir, aciklama yok.

## Iterasyon
1. Bu dosyayi oku, Sirada listesinin en ustundeki maddeyi al.
2. Uygula.
3. Testleri calistir. Gecerse commit et. Kirilirsa duzelt, ayni iterasyonda tekrar dene.
4. Bu dosyayi guncelle: maddeyi Sirada'dan cikar, Gunluk'e tek satir ekle (tarih + ne yapildi + sonuc). Yeni is ciktiysa Sirada'nin sonuna ekle.

## Sirada
- [ ] D-510'un olculemeyen dali: `bolge_yerel_yonlendir`e bilinmeyen-eleman yolunu ULASILABILIR kilan bir sekil ara. Bulunursa fikstur ekle; bulunmazsa CLAUDE.md'ye "ulasilamaz, borc kapali" yaz.
- [ ] yapi_diff K1 (`mantiksal` -> C i1 / self i32, 9 dosya): once TEK bir dosyada dene, kapilar yesilse yay; kirilirsa geri al ve olcumu kaydet.
- [ ] `kanal` bare-metal ABI testi: `runtime/*.kem` yolunda `kanal_olustur/gonder/al` kullanan minimal bir program + `baremetal_diff` kapsamina al.
- [ ] test/perf tabanini kapiya cevir: bench1/bench2 icin zirve bellek esigi olcup regresyon kapisi ekle (D-506'nin 17x kazanci sessizce kaybolmasin).
- [ ] `lean_sorry` kapisina "lake build KOSULMADI" uyarisini SERT hale getirmeden once: lean/lake kurulu mu diye olc, kuruluysa `calistir_lean_tam` hedefi ekle (test_tumu'ya BAGLAMA).
- [ ] ARM64 fiziksel donanim kontrol listesi: D-490'in `smp_queue_arm` sabotajini gercek donanimda tekrarlama adimlarini belgeye yaz (QEMU'da anlamsiz oldugu olculdu).
- [ ] D-520 (cok-segmentli `kullan` T041'i atliyor): en muhafazakar adim = mevcut davranisi FIKSTURLE kilitle + `::`->`/` cevirisinin gocun ilk adimi oldugunu belgeye yaz. Gocu BASLATMA.
- [ ] Kanal omru: `kdl_kanal_serbest` olu kalmaya devam (D-515 kapali). Bunun yerine "kanal tutan tum gorevler birlestirildi mi?" sorusunu olcen bir ARASTIRMA notu yaz, kod yazma.

## Gunluk
- 2026-08-31 D-523: `Dizi<T>` iceren kullanici yapisi goreve yakalanirsa L002 (C + checker.kem + codegen.kem). Skaler alanli yapi MUAF. checker_diff 169/169, ct_bariyer 14/14, codegen_diff 162/162, drf_test 54/54.
- 2026-08-31 D-524: aritmetik tasma sabitlendi — yeni kapi `calistir_tasma` (12 olcum, C+SELF): -O0/-O2 ayni VE IR'da nsw/nuw yok. Sabotaj S99 (nsw enjekte) -> 3 dosya kirmizi, rc=2. Dil degisikligi YOK.

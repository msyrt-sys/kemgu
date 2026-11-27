# KEMGU test altyapısı

## Tam takımı çalıştırma

Depo kökünde aşağıdaki komut her kapının çıktısını bir arada gösterir ve ölçüm raporunu üretir.

```sh
make -j10 --output-sync=target test_tumu_olcum
```

`build/zaman.tsv` her tarif satırının hedef adını, başlangıcını, bitişini ve çıkış kodunu tutar. `build/zaman_raporu.txt` duvar saatini, kapı başına toplamı, en uzun 15 kapıyı, başarısız hedefleri ve disk büyümesini gösterir. Paralel kapıların süreleri örtüştüğünden toplamları duvar saatini aşabilir. Başarılı takım sonunda yeni geçici koşum dizini sayısı sıfır olmalıdır. Aynı çalışma ağacında ikinci bir ölçümlü takım, birincinin süre ve disk raporunu yayımlamasını bekler.

```sh
make -j10 --output-sync=target test_tumu
ZAMAN_BUTCE_SIKI=1 make -j10 --output-sync=target test_tumu_olcum
make calistir_kapi_butcesi calistir_disk_denetimi
```

Bütçeler `test/kapi_butce.tsv` içinde saniye cinsindendir. Taban için aynı makinede üç tam koşum alınır. Başlangıç bütçesi ölçülen en büyük sürenin 1,5 katının yukarı yuvarlanmış değeridir ve en az 60 saniyedir. Raporda bütçesiz veya bütçesini aşan kapı işaretlenir. Sıkı kipte raporlayıcı bütçe ihlalinde 3 döndürür. GNU make bir tarif başarısız olduğunda kendi hata kodunu döndürür.

## Değişikliğe göre seçim

```sh
make test_secili TABAN=origin/main
python3 tools/kapi/kapi_secici.py --taban origin/main
python3 tools/kapi/kapi_secici.py --dosya drivers/nvme/komut.kem
```

İlk komut seçilen kapıları on işçiyle çalıştırır. Python komutları her dosya için eşleşen kuralı ve seçimi yazdırır. Çalışma ağacındaki ve indeksteki değişiklikler ile izlenmeyen dosyalar dahildir. Yeniden adlandırmanın hem eski hem yeni yolu değerlendirilir. Temiz ağaçta kapı çalışmaz. Bilinmeyen bir yol, derleyici, runtime C kodu, Makefile, boot veya linker değişikliği tam takımı seçer.

Bildirim tablosu `test/kapi_haritasi.tsv` içindedir. Genel belgeler iki belge kapısını çalıştırır. ABI, gramer ve dil kartı için ilgili tüketici kapıları da seçilir. Sürücü alanları Make tarifleri, harness girdileri ve geçişli KEMGU içe aktarmalarından üretilir. Örneğin tek başına sim sürücüsü köprü kapısını, x86 kabuğuna giren bir ağ modülü ise kabuğu kullanan çok sayıda QEMU kapısını seçer. Sürücü tablosu güncel kaynaklarla uyuşmazsa seçici tam takıma döner.

```sh
python3 tools/kapi/kapi_haritasi.py --yaz
make calistir_kapi_secici
python3 tools/kapi/secici_gecmisi.py --adet 30
```

Son komut belirtilen commit'lerin kendi kaynaklarını ve Make hedeflerini kullanır. Birleştirme commit'lerinde fark ilk ebeveyne göredir. Ortak harness içindeki koşullu kaynaklar fazladan kapı seçebilir.

## Çekirdek önbelleği

UEFI ve sekiz x86 harness'ı `test/cekirdek_onbellek.sh` kullanır. Anahtar derleyici ikilisini, bütün yamalar uygulandıktan sonraki birleşik kaynağı, içe aktarılan modülleri, IR süzgecini, bağlayıcı betiğini, başlangıç nesnelerini, derleme komutunu ve araç sürümlerini kapsar. `ham.ll`, `k.ll`, `k.o` ve `k.elf` birlikte yayımlanır. Kopyalama sırasında bir kayıt budanmışsa harness yeniden derler. Önbellek en son kullanılan 64 kaydı tutar.

```sh
make calistir_cekirdek_onbellek
KEMGU_CEKIRDEK_ONBELLEK=0 make calistir_bm_x86_ekran
KEMGU_CEKIRDEK_ONBELLEK_KOK=build/deney_onbellek make calistir_bm_x86_ekran
```

Bayt eşitliği kapısı isabeti, önbelleksiz ürünü ve anahtarı değiştirmesi gereken girdileri dener. Sabotajdan sonraki temiz koşum için özel kök kullanmak deneyleri ayırır.

## Yeni harness için kontrol listesi

- Kabukta `test/calisma_dizini.sh`, Python'da `tests/qemu/calisma_dizini.py` ile `build/` altında koşuma özel dizin aç. Başarıda temizle. Hatada tanı yolunu bildir ve çıktıları koru.
- Seri günlük, QMP soketi, yazılabilir disk ve QEMU çıktıları aynı koşuma ait olsun. Ortak disk tabanını salt okunur veya snapshot kipinde aç. TCP dinleyicisinin sahip olduğu portu kullan.
- Ortak ürünün tek Make üreticisi olsun. Uzun süre paylaşılan sabit ürünler için aynı dosya kilidini bütün kullanıcılar alsın. `test/olcum/paylasim_izinleri.tsv` içindeki her kaydın korumasını açıklayan gerekçesi bulunur.
- QEMU'nun hazır olduğunu seri işaretinden, gerçek QMP bağlantısından veya bellek sayacından ölç. `tests/qemu/bekle.py` beklenen koşulu, zaman sınırını ve son gözlemi birlikte bildirir. Protokolün gerektirdiği süreyi ayrıca belirt.
- Yeni hedefi `test_tumu` ve bütçe tablosuna ekle. Bölünen bir hedefin eski adı parçaları önkoşul olarak tutar. `calistir_kapi_kapsam` sekiz şemsiyenin çağrı kapsamını denetler. `calistir_paylasim_denetimi` sabit kaynak ve dinleyici çakışmalarını arar.
- Çıkış kodunu kaydet. Bash'te bir komutu `tee` ile günlüğe alırken `PIPESTATUS` dizisini boru hattından hemen sonra kopyala. Yerel Windows Python'una çağrılan aracı `shutil.which` ile çöz ve Türkçe metni UTF-8 olarak aç.

## CI ve ölçümlerin kapsamı

Linux işi paralellik için `nproc`, MSYS2 işi `NUMBER_OF_PROCESSORS` kullanır. Her iki işte GNU make hedef çıktısı birlikte yazılır. Linux'ta ölçümlü takımın raporu Actions günlüğündedir. Windows'ta tam takımın derleme ve host denetimleri koşar. Bir QEMU kapısı gerekli araçlar bulunmadığı için atlandığında günlükteki atlama bildirimi, o mimarinin çalışma zamanı için kanıt sayılmaz. Gerçek QEMU denetimleri ayrıca geliştirme makinesinde koşulur.

| DGX Spark, ARM ana makine | Tam takım duvar saati | En uzun kapı |
|---|---:|---:|
| Bölüm 1, üç taban koşumu, 10 işçi | 37:31 / 35:00 / 34:18 | Kapı başına üç ölçüm bulgular belgesinde |
| Bölüm 4, uzun kapılar bölündükten sonra, 10 işçi | 25:08 | 8:30 |
| Bölüm 5, olay beklemeleri, 10 işçi | 23:03 / 23:13 / 23:01 | Son koşum 8:37 |
| Bölüm 6, seri | 4 sa 48 dk 11 sn | |
| Bölüm 6, 10 işçi | 23:24 | 8:41 |
| Bölüm 6, 20 işçi | 14:59 / 15:10 | |

İkinci taban koşumunda semafor gözlemcisi aralıklı hata verdi. Bölüm 6'da gözlemci düzeltildikten sonra tablodaki son dört koşum geçti. Bu makinede x86 konuklar TCG ile çalışır. Host yükü, kullanılabilen araçlar, önbellek ve koşulacak kapılar değiştiğinde süreler de değişir. Ham ölçümlerin, geçici dizinlerin ve hataların ayrıntıları [Dalga 18 bulgularında](../otomasyon/chatgpt/codex18_bulgular.md) yer alır.

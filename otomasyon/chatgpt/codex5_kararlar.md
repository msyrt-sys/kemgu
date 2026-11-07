# Dalga 5 kararlar

VARSAYIM: Görev 5’in açık derleyici kapsamı ve codex/d5-* dalları, eski genel AGENTS.md sürücü sınırının bu göreve özgü istisnasıdır.

## F005 ön envanter

D-740 tabanı: `son_segment` için 16 metinsel eşleşme: 15 çağrı + işlev tanımı. Görevdeki 13 sayısı yerine güncel kaynak sayıldı.

- `alias_coz`: kod üretiminde alias → modül öneki.
- `fn_coz`: seçili işlevin IR kayıt anahtarı.
- `deyim_uret` çeşit payload yolu: varyant adı; modül IR adıyla ilgisiz, korunmalı.
- `secili_imza_anahtari`: checker imza ve gizlilik anahtarı; seçili ad çözümlemesi.
- `alias_modul_coz`: checker alias/gizlilik anahtarı.
- `secili_uyeleri_tasi`: seçili çeşidin gizlilik kontrolü.
- `modul_yukle` (2): nitelikli imza ve özel üye kayıt anahtarları.
- `modul_src_bir`: kaynak sarmal adı, dolayısıyla IR tanım öneki.
- `kullan_bag_adi` (3): yerel modül bağı ve T024 kontrolü; son segment davranışı korunmalı.
- `kullan_yukle_hepsi` (3): nitelikli erişim için modül adı kaydı, seçili gizlilik kontrolü, izinli seçili adın global kaydı.

Tasarım incelemesi: `modül drivers { ... }` sarmalını her dosya için yeniden üretmek ortak önekleri çoğaltır. Kaynak sarmalını tek modül olarak tutup birleşik AST’de yalnız sentetik modülün IR önekini tam yola dönüştürmek, ad çözümleme yüzeyini değiştirmeden uygulanabilecek adaydır. Ölçüm yapılmadan uygulanmış karar sayılmaz.

## Kapı kapsamı ölçümü

`codegen_diff_harness.sh` ve `modul_codegen_harness.sh`, C IR bağlama hatasını atlıyor. Dolayısıyla F002/F004/F005 gibi C LINK-RED kusurlarında yalnız bu kapıların rc=0 sonucu kanıt sayılmaz. `codegen_genis_harness.sh`, kurate listede olmayan C bağlama hatasını kırmızı sayıyor. Harness değiştirmeden, yeni `test/ornekler/` giriş fikstürleriyle bu kapıda da kırmızı/yeşil ve sabotaj ölçüldü. F002/F004 girişleri asıl korpus fikstürünü çağırır; F005 geçişli yükleme farkı aşağıda ayrıca açıklanır.

### F005 uygulanan seçim ve ilk ölçüm

Sentetik kaynak sarmalı tek modül olarak korundu. Bağımlılıkların yayım sırasıyla tam IR adları bir listede taşınıp, birleşik AST'nin yalnız öne eklenen dosya modüllerine geri yüklendi. Böylece ortak `f005::sol` öneki için yinelenen `modül f005` üretilmedi. Ad çözümlemesi ayrı ilk checker geçişinde değişmeden kaldı; gizlilik, T024, seçili ithalat ve checker imza anahtarları değiştirilmedi. Kod üretimindeki alias ve seçili işlev kayıt anahtarları aynı tam IR adına yöneltildi.
İlk ölçüm: üç dosya (iki cekirdek ve aynı önekli ek), aynı özel/genel işlevler, seçili ithalat ve alias ile iki derleyicide link rc=0, exit=42. Tek başına ithal edilen sol/cekirdek ile çoklu ithalatın `f005.sol.cekirdek.*` kümeleri birebir; tek parçalı `f005_tek.deger` adı korundu. Son kapı sonuçları ilerleme tablosunda kaydedildi.

F005 ek ölçüm: `test/ornekler` girişinden `test::moduller::ana_f005_cakisma` ithal edilince self kaynak yükleyicisi, kökte bulduğu dosyanın geçişli göreli `f005::...` bağımlılıklarını yanlış birleşik dizinde arayıp atladı; IR'da alt modül tanımları yoktu. `modul_src_bir` içindeki `mdiz`, dosyanın fiilen bulunduğu kökten hesaplanmıyor. Bu ayrı yol çözümleme kusuruna dokunulmadı. Geniş kapı giriş fikstürü aynı üç test modülünü kökten doğrudan ithal ediyor; değerler ve exit=42 değişmedi.

## F002 dizi elemanı ölçümü ve kapsam

Yeni fikstür C/self checker tarafından kabul edildi. C alan/indeks `hiç` üretiminde rc=1; self IR üretti ama alan atamasında i32 / `{i8,i64}` uyuşmazlığıyla bağlanamadı. Üç büyük değer Python ile hesaplandı: 2^33+17=8589934609, 2^34+29=17179869213, 2^35+43=34359738411.
Dizi elemanı ölçümü C'nin `dizi_eleman_struct_mi` sınıflandırmasında ek bir eksikliği açtı: yalnız nominal `%` ve kapanış `{ptr,ptr}` by-value sayılıyor, seçimlik/sonuç inline agregatları skaler yola gidiyordu. Self aynı yordamında zaten bütün `{...}` agregatlarını by-value sayıyor (mevcut D-439). C sınıflandırması bu mevcut temsile uyarlandı; boyut elle hesaplanmadı, mevcut LLVM sizeof/GEP yolu kullanıldı. Yeni dil özelliği veya tanı eklenmedi. İlk düzeltme ölçümü: dört yapıcı, alan, iç içe alan ve dizi indeksi iki derleyicide exit=42.

F002 tam takım ilk koşumu rc=2 ile `bolge_operand` kapısında durdu: yeni fikstürde iki C tahsisi CALLER, self tahsisleri YEREL. Self `bolge_yerel_yonlendir` yalnız nominal `%Yapı` elemanını dışlıyordu; satır içi `{...}` agregatını skaler sayıyordu. Çağıran bölgesi kuralı, iki tarafta da ortak by-value sınıflandırmasına bağlandı. Beklenti/harness değiştirilmedi; ilk başarısız tam koşum başarılı sayılmadı. Ek bölge sabotajında derleyici kuruldu ve kapı yeniden 1/198 sapma, rc=2 verdi. Geri yükleme sonrası tüm kapılar ve tam takım baştan koşuldu: rc=0, bölge operandı 198/198, FIXPOINT 97415 satır.

## F004 bölge paritesi yan düzeltmesi

İlk tam takım yeni fikstürün sarmalsız çeşit dizisinde C CALLER / self YEREL farkıyla rc=2 verdi (1/198). C nominal `%Olay` elemanını iç işaretçi taşıyabilen agregat sayıyor; self aynı çeşidin `{...}` temsilini yanlışlıkla yerel tahsise uygun sayıyor. F002'de ölçülmüş aynı `bolge_yerel_yonlendir` koşulu bu bağımsız dalda da gerekli; F002 henüz main'de olmadığı için aynı küçük hunk burada da yer aldı. Fikstürün yerel dizi pozitif yolu değiştirilmedi. F002 ile aynı hunk kullanılması sonraki birleştirmede gereksiz yorum çakışmasını önler.

F004 son doğrulama: bölge hunk sabotajında compiler build rc=0, kapı rc=2; geri yükleme sonrası sekiz kapı ve tam takım rc=0, bölge 198/198, FIXPOINT 97357. İlk tam koşumun rc=2 sonucu ayrı saklandı.

## F001 — alan çözümleme fazı ve yerleşim

İki yeni modül fikstüründe sarmalsız Rsdp C T011+T001×3, seçimlik<Rsdp> C T011+T001×4 verdi; self checker OK. İlk checker kapısı 299/301 rc=2. CLAUDE.md:358 ileri başvuruları belgeler; C007 ve self yapi_topla yorumları önce ad kaydı/sonra tip çözümünü amaçlar. Bilinçli bir ithal alan yasağı bulunmadı.
C alan adlarını ve T024 kontrolünü ilk geçişte bırakır; tüm modül tip kayıtlarından sonra, işlev imzalarından önce aynı yapı scope'unda alan tipleri çözülür. İlk deneme alan geçişini kullan bağlarından sonraya koydu; iki eski negatif fikstürde tanı sırası değişti (299/301). Beklentiler değiştirilmedi; alan tanıları imza tanılarından önce kalacak faz seçildi. Mevcut düz-ad fallback seçimi aynıdır; yapı scope'unda seçilen sembolün lineer bayrağı korunur, LR002 kaybolmaz. Generic alan parametreleri kendi scope'unda kalır; yinelenen alan ilk sembolü ezmez.
Self checker kabul etse de çalışma zamanı fikstürü ayrı LINK-RED açtı: Kapsayici içindeki Rsdp i32, seçimlik<Rsdp> ise {i8,i32} yayılıyordu. yapi_topla kapsamın alanlarını modül adlarından önce çözüyordu. Adların ayrı recursive geçişi eklendi; eski root→modül ad sırası ve alan/base/count paralel dizilerinin sırası aynen korundu. C onarımından sonra self eskiyken modül kapısı 55/57 rc=2; iki onarım sonrası checker 301/301 ve modül 57/57, dört koşum exit=42.
Sabotaj: C eski kaynak yedeğiyle compiler build=0/checker 299/301 rc=2; self eski kaynak yedeğiyle compiler build=0/modül 55/57 rc=2. Alan fallback'ının lineer bayrak taşıması ayrıca ölçüldü: ithal `genel yapı tekkez Kaynak` sıradan K alanına konunca doğru C LR002/rc=1; yalnız bayrak koruması kaldırılınca build=0 ve hatalı OK/rc=0; geri yüklendi. Bu ek prob build/codex5/f001/ithal_lineer.kem + lineer_tip.kem içinde; ana korpusun iki negatif fikstürü T011 ve LR002 tanılarını korur.

### Kapsam dışı iki self checker bulgusu

- `yapı K { r: tam32; r: tam64; }`: C T024/rc=1, self --check OK/rc=0. C reddi korundu; self kuralı bu göreve eklenmedi.
- Ayrı dosyadan seçili ithal `genel yapı tekkez Kaynak { n: tam32; }`, sıradan yapı alanında: C LR002, self OK. Ek kök ölçümü: aynı hata ithalatsız `yapı tekkez Kaynak` alanında da var (C LR002, self OK). `lr002_yapi`, yalnız TIP_TEKKEZ/TIP_YETKI/TIP_GOREV AST sarmallarını sayan `tip_node_lineer_mi` kullanıyor; nominal lineer yapı alanını atlıyor. Bu nedenle yalnız ithal bayrağı kaybı diye sınıflandırmak eksik olurdu. F001'in pozitif alan çözümünden ayrı, daha geniş eski bir güvenlik/parite bulgusudur; mevcut self checker kuralı bu göreve genişletilmedi. C tarafında çözümleme fazı değişirken bu reddin kaybolmaması ayrıca sabotajla korundu.
Negatif --check ölçümünde self tanı yazsa da rc=0 dönebilir; tanı çıktısı karşılaştırıldı. C --checkdump ile self --check aynı ölçüm yüzeyidir; self'e --checkdump verilmesi geçerli karşılaştırma değildir.

## Doğrulama hesabı ve ölçüm sınırları

- Her kusur kendi başlangıç anındaki origin/main tabanında doğrulandı; beş düzeltmenin birleşik entegrasyon testi yapılmış sayılmaz. Main'e merge/push ve auto-merge uygulanmadı.
- Tam takım atlamaları tekil dosya sayısı değil, koşum olayıdır: doğrudan codegen 4 (2 yabancı mimari + 2 C oracle link hatası), self-driver iki LLVM turu 8, geniş codegen 9, yapısal 2, sürücü paritesi 1, eski VirtIO host-mock 5, bölge operandı 4, ASan 32 = 65. Her koşumun gerçek sayısı PR gövdesinde kontrol edildi. QEMU üst bellek senaryosu RAM ≤1 GiB nedeniyle ayrıca 1 atlamadır; HASHCRACK'in 7 arama adayı test atlaması değildir.
- Mevcut muafiyetler: yapısal 24 + sürücü 1 + geniş checker 18 + tip kontrol 9 = 52; ASan'ın 4 bilinen ve 2 sızıntı muafiyeti ayrıca belirtilir. Muafiyet listeleri değiştirilmedi.
- Geniş codegen kapısı iki bare-metal MMIO keşif programında C/self eşleşen 139 çıkışını parite sayar; günlükteki bu dört segfault satırı yeni fikstürlerin başarısı değildir. Yeni fikstürlerde ayrı IR/link rc=0 ve exit=42 ölçüldü.
- Lean kapısı 32 dosyada 0 sorry/admit metinsel denetimidir. lean/lake yok; gerçek Lean tip denetimi koşulmuş sayılmaz.
- F002/F004 ilk tam koşumları bölge sapmasıyla rc=2 oldu. Onarım ve bağımsız bölge sabotajından sonra bütün kapılar ve tam takımlar yeniden koşuldu; yalnız son rc=0 koşumları başarı olarak raporlandı.

Ek bulguların sınırı: yinelenen alan için bir, nominal lineer alan için yerel ve dosyadan ithal olmak üzere iki prob ölçüldü; depo çapında kullanım sayısı çıkarılmış sayılmaz. Sonraki seçenek, C'nin mevcut T024/LR002 kurallarını self'te aynı kapsamda uygulamak ve ayrı negatif/pozitif korpusla kanıtlamaktır. C'nin kurallarını daraltmak dil tasarımı kararı olur; böyle bir karar verilmedi.

F001 son ölçüm: sekiz kapı ve tam takım rc=0, FIXPOINT 97431 satır; checker 301/301, modül 57/57. Yeni tabandaki NVMe/AHCI gerçek QEMU qtest kapısı C + self-host dört koşumda da geçti.

## T1 kararı — düzeltmeler henüz origin/main içinde değil

- F001: drivers/acilis/multiboot2.kem yerel AcilisRsdp kaydı; drivers/nvme/cekirdek.kem ve drivers/ahci/cekirdek.kem başlıklarındaki ithal kayıt gerekçeleri.
- F002: drivers/acilis/multiboot2.kem kok/ekran/rsdp seçimlik ara değişkenleri.
- F003: drivers/nvme/cekirdek.kem, drivers/ahci/cekirdek.kem, tests/drivers/nvme/sahte_nvme.kem, tests/drivers/ahci/sahte_ahci.kem, tests/drivers/usb/sahte_depo.kem içindeki yapı alanı bayt kopyaları. Parametre b üzerinden oku_u8 kullanımları otomatik dolanma sayılmamalı.
- F004: tests/drivers/usb/hid_klavye_test.kem içindeki üç açık TusOlayi ara değişkeni.
- F005: drivers/ahci/cekirdek.kem özel ahci_bekle/ahci_tamamlandi/ahci_istek_kur adları. Genel ahci_* API adları aynı gerekçeyle değiştirilmez.
- Alias: tests/drivers/sim/depolama_matrisi_test.kem nv/ah alias'ları, aynı örtük cekirdek bağı için C001/T024 tasarımı nedeniyle gereklidir. F005 yalnız IR adı; kullan_bag_adi kuralları bilerek korunur. Bunlar F005 düzelince topluca kaldırılamaz.
Son ölçüm: 2026-11-07 20:13 UTC fetch, origin/main=98f39b7. f7a2f5a/ffeb68a/2fd2918/18419a7/378a54b commitlerinin hepsinde ancestor rc=1. Beş draft PR da açık ve merge edilmemiş. Dolanmalar güncel main üzerinde hâlâ gerekli; T1 kaynak değişikliği uygulanmadı. Main merge/cherry-pick edilmedi. PR'ların bağımsız güncel-main tabanı kuralı korunur.

T1 ek kanıt: `git diff 3225ce1..98f39b7 -- src selfhost` boş. Sonraki ana dal değişiklikleri görev belgesi, QEMU qtest ve AHCI sürücüsü tarafında; beş kusurun derleyici kaynakları başka commitlerle de onarılmış değil.

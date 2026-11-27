# E4k — Uzun Ömürlü Döngüde Geçici Belleğin Geri Alınması (Tasarım)

Durum: **KARAR VERİLDİ (Mehmet, 2026-11-27, D-780) — A kabul edildi (koşullu).** A0 fizibilite
ölçümü yapıldı ve A'yı destekliyor, bir düzeltmeyle (§8). Kesin onay dalga 18 (çok çekirdek)
birleşince verilecek (§9.6). Kararlar §7'de, güncel tasarım §9'da; §1–§6 karar öncesi kayıttır.
Tarih: 2026-11-27. İlgili: D-747, D-756, D-770, D-774/775, D-776, D-777, D-778, D-779, D-780.

## 1. Sorun

KEMGU-OS çekirdeği tek bir ömür boyu döngüde koşar (`kabuk(...)` içindeki `iken doğru`,
`test/ornekler/kem_os_x86_kabuk.kem`). Sürücü ve kabuk durumu `&değişken` ile bu döngüye
taşınır; her tahsis çağrı zinciri boyunca `main`in bölgesine, yani **ana bölgeye** düşer ve
**hiç geri verilmez**. Çekirdek heap'i 28 MB'tır; yeterince uzun bir oturum onu tüketir.

D-774/775 (dönüş-saf çağrı + tur bölgesi), D-777 (yerinde uzatma) ve D-778 (dar eleman)
sabit çarpanı düşürdü ama mekanizmayı **kapatmadı**.

## 2. Ölçümler (2026-11-27, güncel `main`, C çekirdeği)

Araçlar `scratchpad/e4k_*.py` (depoya girmedi): ana bölgenin blok listesinden gerçek
`kullanilan` toplamı; çekirdek IR'ında `kdl_dizi_olustur`a `llvm.returnaddress` yaması ile
her dizinin **oluşturucusu**; heap + veri/bss/yığın üzerinde referans taraması.

| Ölçüm | Sonuç |
|---|---|
| Boşta (komut yok, 40 sn) | **+752 bayt** (yalnız ilk okuma) — boşta yoklama SIZDIRMAZ |
| Dosya oturumu (10 komut) | ~0,75 MB (D-778 sonrası; D-777 öncesi 2,42 MB) |
| `mv` tek komut | ana bölge +328 KB, yeni dizi başlıkları 140 KB |
| Komut sonrası **referanssız** (kesin çöp) dizi başlığı | `cat` 98/113 · `mkdir` 381/433 · `yaz` 413/459 · `mv` **790/873** |
| Aynı, bayt olarak | `mv` 102 KB çöp / 38 KB referanslı (**≥ %73 çöp**) |

Referans taraması **geçişsizdir** → gerçek çöp oranının **alt sınırıdır** (çöp bir yapının
gösterdiği dizi "referanslı" sayılır). Büyüme artıkları ve dizi dışı tahsisler (yapı, metin)
tabloya girmez; onlar da büyük olasılıkla çöptür.

`mv` oluşturucu dağılımı (ilk 6): `bayt_sifir` 181× (LFN tamponu — `dizin_coz` aynı dizini
**181 kez** yeniden ayrıştırıyor), `onbellek.kopya` 39×, `onbellek.sona` 28×,
`utf8_bayt_coz` 113×, `fsy_dizin` 28×, `bm_bellek_oku` 151× (aygıt durum yoklaması, 16 B).

**Zamanlama bağımlılığı:** aynı kaynağın yalnız ölü kodla farklı iki derlemesi 2,42 MB /
1,62 MB verdi (D-777). Kök: G/Ç **tamamlanmasını beklerken** her yoklama turu yeni bir yanıt
dizisi üretir; tur sayısı aygıt gecikmesine bağlıdır. Gerçek donanımda (daha yavaş aygıt,
daha hızlı CPU) bu kalem **büyür**.

## 3. Çıkarım

Sızıntının baskını **geçici çalışma verisidir**; hayatta kalan durum küçük ve **adlıdır**:
döngüden önce bildirilen yereller (`h`, `tr`, `satir`, `fd`, `im`, `p`, `hid_tampon`, …) ve
`&değişken` parametreleri (`d: KabukDisk`, `x: XhciDurum`, `ke: KabukEkran`). Bunlar dışında
bir tur içinde oluşan her şey tur bitince ölüdür — **ancak** o tur içinde hayatta kalan
duruma yazılmadıysa.

Zor kısım tam bu "ancak": durum alanına yazılan yeni dizi (`s.kuyruk = …`, önbellek
kayıtları, yeni açılan dosyalar) hayatta kalmalı; aynı alanın **eski** değeri ise ölmeli.
Kaçış analizi bunu yapısal olarak çözemez (D-776 notu): eski değer erişilemez hâle gelir ama
bunu derleme zamanında bilmek için sahiplik/eşsizlik bilgisi gerekir.

## 4. Seçenekler

### A. Tur sonu durum taşıma (evacuation) — ÖNERİLEN
Döngü turu kendi bölgesinde (ρ_tur) koşar. Tur sonunda (güvenli nokta) **döngüye taşınan
durum** — yukarıdaki adlı yereller + `&değişken` parametrelerin gösterdiği değerler — tip
bilgisiyle **derin kopyalanarak** uzun ömürlü bölgeye taşınır, sonra ρ_tur topluca serbest
bırakılır. Kopyalanan değerden erişilemeyen her şey (eski alan değerleri, geçici sonuçlar)
bedavaya gider.

- **Doğruluk:** KEMGU değer grafiği statik tiplidir (yapı, çeşit, `Dizi<T>`, `metin`,
  `sonuç`, `seçimlik`); kopyalayıcı tipten **derleyici tarafından üretilir**. ⚠ ÖLÇÜLDÜ:
  güvenli kodda yapı alanı `&T` referansı da `*T` ham işaretçi de tutabilir
  (`yapı K { r: &tam32; }` ve `yapı K { p: *tam32; }` --check'ten geçiyor). Bu yüzden
  kopyalayıcı (i) paylaşımı korumak için **yönlendirme tablosu** (Cheney tarzı: aynı nesne
  bir kez kopyalanır, her işaretçi yeni adrese yazılır), (ii) her işaretçide **çalışma
  anında bölge üyeliği sınaması** (yalnız ρ_tur bloklarının içini gösteren işaretçi taşınır;
  yığın, ana bölge ve UEFI çerçevelerini gösterenler aynen kalır) yapmalıdır.
- **Dil yüzeyi:** yok (yeni sözdizimi/yerleşik gerekmez); derleyici `iken doğru` gibi ömür
  boyu döngüleri ve döngüye taşınan değişkenleri zaten görür.
- **Maliyet:** her taşıma canlı durumun boyutuyla orantılı bir duraklama (kopyalama).
  **Eşik tetikli** yapılırsa (ρ_tur > örn. 256 KB olduğunda) amortize edilir; boşta döngü
  hiç tetiklemez (ölçüm: boşta büyüme 0).
- **⚠ Felsefe sorusu:** bu, güvenli noktada çalışan, tip yönlendirmeli **kopyalayıcı bir
  toplayıcıdır**. "GC yok, sıfır duraklama" hedefiyle doğrudan çelişir; duraklama sınırlı
  (canlı durum ~ birkaç yüz KB) ve öngörülebilir noktada olsa da. D-756 bu sınıfı ("dönemsel
  durum kopyalama") reddetmişti; o karar **o günkü ölçümle** (çöp oranı bilinmiyordu)
  verilmişti.
- **Açık noktalar:**
  - `*T` bir dizinin ORTASINI gösterebilir; hedefin sınırı tipten bilinmez. Taşıma anında
    ρ_tur içini gösteren ham işaretçi bulunursa **temiz panik** (sessiz bozulma yerine); ya
    da derleme zamanında: döngüye taşınan durum tipinde `*T` varsa taşıma o döngü için KAPALI.
  - Adresi `tam64` olarak tutan kod (`kem_os` sürücülerinde yaygın: 454 `tam64` kullanımı,
    çoğu MMIO/DMA adresi) kopyalayıcıya GÖRÜNMEZ. Bu adresler bölge dışı (UEFI çerçeveleri)
    olmak zorundadır; ρ_tur'daki bir tahsisin adresini `tam64`e çeviren `güvensiz` kod
    taşıma sonrası sarkan adres üretir → derleyici bunu (dönüşüm kaynağı ρ_tur tahsisi ise)
    reddetmeli ya da o döngüde taşımayı kapatmalı. **Bu, A'nın en büyük sağlamlık riskidir.**
  - Kapanışlar (`{ptr, ptr}` env) ve `görev`/`kanal` tutamakları opak → V1'de döngüye
    taşınan durumda bunlar varsa taşıma kapalı (eski davranış).

### B. Sahiplikli alan yazımı (eski değeri serbest bırakma)
`s.alan = yeni` atamasında eski değer **eşsiz sahipliliyse** onun bölgesi serbest bırakılır.
- Gerektirdiği: dizilerde/yapılarda eşsizlik bilgisi (lineer `Dizi`, `tekkez` alanlar). Dil
  yüzeyi + sürücülerin büyük kısmının yeniden yazımı; takma adlar (`s.kimlik = s.sonuc`,
  D-777) bugün yaygın.
- Geçici çağrı sonuçlarını (durumda hiç saklanmayan) KAPATMAZ — onlar ayrıca tur bölgesi ister.

### C. Bölge-akış analizi (escape v3, D-756 kararı)
Her tahsisi "duruma ulaşır → sahibin bölgesi / ulaşmaz → ρ_tur" diye ayıran analiz.
- Sürücü kodu durumu her yerde `&değişken` ile taşıdığı için analiz pratikte çoğu tahsisi
  "ulaşabilir" sayar (D-747, D-774 ölçümleri); kesinlik düşük, sağlamlık riski yüksek.
- Ölçülen çöpün çoğu bu sınıfta (durum işlevlerinin içi) — C'nin kazancı sınırlı kalır.
- ⚠ D-780: D-756'nın escape v3 kararı **geri alındı** (§7.1).

### D. Algoritmik azaltma (A/B/C'den bağımsız, hemen yapılabilir)
- `dizin_coz`: dizin ayrıştırma sonucunu (FAT nesli + küme numarasıyla anahtarlı) önbelleğe
  al; `mv` başına 181 ayrıştırma → birkaç.
- G/Ç bekleme yoklaması: yanıt dizisini her turda üretmek yerine sabit tampona yaz (D-747
  seçenek (a)'nın dar biçimi — yalnız **durum yoklaması** yanıtları; veri yanıtları değil).
- Kazanç: sabit çarpan (tahminen 2–4×); mekanizmayı kapatmaz ama A'nın taşıma sıklığını ve
  gerçek donanımdaki zamanlama bağımlılığını düşürür.

## 5. Öneri

> ⚠ D-780: bu bölüm karar öncesi öneridir. Aşamaların güncel hâli §9.7'dedir. "A0" artık
> fizibilite ölçümünün adıdır (§8); burada A0 diye anılan ölçüm kapısı A2'ye taşındı.

**D hemen, A karar sonrası.**

1. **D (sürücü işi, Codex'e verilebilir):** dizin önbelleği + durum yoklaması sabit tamponu.
   Kapı: dosya oturumu kullanımı (deterministik değil → yalnız bilgi) ve `dizin_coz` çağrı
   sayısı (deterministik → kapı).
2. **A (derleyici, Claude):** aşamalı.
   - A0 — ölçüm kapısı: `kabuk` benzeri bir test çekirdeğinde (yoklama yok) N tur × geçici
     tahsis + küçük kalıcı durum; ana bölge büyümesi turlarla **sınırlı** olmalı. Bugün
     doğrusal büyür → kapı kırmızıdan başlar.
   - A1 — tip yönlendirmeli derin kopyalayıcı üretimi (her iki derleyici; yapı/çeşit/dizi/
     metin/sonuç/seçimlik). Kopyalayıcı birim testleri host'ta (ASan).
   - A2 — ömür boyu döngü tespiti + döngüye taşınan değişken kümesi + eşik tetikli taşıma
     (yalnız çekirdek hedefinde, bayrakla açılır).
   - A3 — `güvensiz` ham işaretçi kuralı (yeni tanı) + opak tutamaklarda taşımayı kapatma.
   - A4 — KEMGU-OS'ta açma; tam takım + uzun oturum (≥ 1000 komut) kapısı.
3. **B ve C ertelenir:** B dil yüzeyi + büyük yeniden yazım ister; C ölçülen çöpün çoğunu
   kapsamaz.

## 6. Karar soruları (Mehmet)

1. A'nın "güvenli noktada, eşik tetikli kopyalayıcı" niteliği KEMGU'nun "GC yok" ilkesiyle
   bağdaşır mı? (Duraklama: canlı durumla orantılı, öngörülebilir noktada; boşta sıfır.)
   Bağdaşmazsa geriye B (dil yüzeyi) kalır.
2. A yalnız çekirdek hedefinde mi (bayrakla), yoksa genel bir dil özelliği mi olmalı?
3. D'yi Codex'in bir sonraki dalgasına ekleyelim mi?

> ✓ Üçü de yanıtlandı — §7.

## 7. Kararlar (Mehmet, 2026-11-27 — Opus önerileri kabul edildi) [D-780]

1. **A ve ilke.** A kabul edildi ve ilke **dürüstçe yeniden yazılır**:
   > KEMGU'da arka planda çalışan bir çöp toplayıcı yoktur ve duraklama **öngörülemeyen bir
   > anda** gelmez. Ömür boyu döngülerde (KEMGU-OS çekirdek döngüsü gibi) derleyici, **kendi
   > seçtiği güvenli noktada**, tipten **üretilmiş** bir bölge tahliyesi yerleştirir. Duraklama
   > canlı durumla orantılıdır ve boşta hiç tetiklenmez.

   "Sıfır duraklama" ifadesi A devreye girince doğru olmaktan çıkar ve kaldırılır. README ve
   CLAUDE.md o gün güncellenir (A4). Bugün A uygulanmadığı için eski ifade hâlâ doğrudur.

   Kabul **koşulludur**:
   - (i) A0 fizibilite ölçümü (§8) — **yapıldı**.
   - (ii) Dalga 18'in çok çekirdekli yapısı bilinmeli. Çekirdekler arasında el değiştiren hiçbir
     nesne R'de olmamalı (§9.6). Kesin onay dalga 18 birleşince verilir.

   Koşul tutmazsa geriye B (dil yüzeyi) kalır. Bu karar **D-756'nın "escape v3" önerisini geri
   alır**: C ölçülen çöpün çoğunu kapsamıyor (§4.C) ve sağlamlık riski en yüksek seçenekti.
2. **Kapsam.** A genel bir derleyici mekanizmasıdır ve **bayrakla** açılır. Varsayılan kapalıdır:
   bayraksız program bugünkü IR'ı birebir üretir. İlk kullanıcı çekirdektir (`kem_os`). Mekanizma
   host'ta da derlenir ve çalışır; kopyalayıcı ile tahliye host'ta **ASan altında** sınanır
   (çekirdekte ASan yok).
3. **D.** Codex dalga 19'a yalnız **D(1) dizin önbelleği** gider.
   - Geçersizleme mevcut `kd_icerik_surumu` sayacıyla (`test/ornekler/kem_os_x86_kabuk.kem:536`).
     Sayaç bugün FAT yazma planında, ramfs yaz/taşı yollarında ve süreç dosya kapatmada
     artırılıyor. Her yazma ile bağlama/çıkarma yolunun onu artırdığını dalga 19 **ölçmeli**.
   - Kapı: `dizin_coz` çağrı sayısı (deterministik) + mevcut FAT/fsck kapıları.
   - **D(2) durum yoklaması sabit tamponu ertelendi.** A yoklama yanıtlarını da toplar. D(2)
     yalnız sabit çarpan kazandırır ve sürücü yüzeyine yayılır.

## 8. A0 — fizibilite ölçümü (2026-11-27, güncel `main`, C çekirdeği)

Üç soru soruldu:
1. Döngüye taşınan durumun tipleri, kopyalayıcının izleyebileceği tipler mi?
2. Bölge adresini kopyalayıcıdan gizleyen kod var mı?
3. Canlı durum ne kadar büyük?

Araçlar `scratchpad/a0_*.py` ve tip denetleyicisinin geçici yazdırmalı bir kopyası; hiçbiri
depoya girmedi.

### A0-1 Tip kapanışı
Kökler `kabuk`un `&değişken` parametreleri (`KabukDisk`, `XhciDurum`, `KabukEkran`) ve döngüden
önce bildirilen yerellerdir (`HidKlavye`, `KlavyeDurumu`, `SatirDuzenleyici`, `FareDurumu`,
`EkranFare`, `KsProgram`, `KsKabukSureci`, `Dizi<TusHamOlay>`).

Çekirdek ve sürücü kaynaklarının birleşiminde 162 yapı/çeşit var. Kapanış bunların **49**'unu
kapsıyor ve **hiçbirinde** `*T`, `&T`, `işlev`, `görev`, `kanal`, `yetki` ya da `tekkez` alanı
yok. Durum tamamen yapı, çeşit, `Dizi<T>`, `metin` ve skalerden oluşuyor; kopyalayıcı tipten
üretilebilir.

⚠ Tarama sözdizimseldir: generic tip argümanları ayrıca denetlenmedi. A1'deki üretici bunu tip
denetleyicisinin çözülmüş tipleriyle yapacak.

### A0-2 Gizli adres kanalları
Tip denetleyicisinin yazdırmalı kopyası, çekirdeğin birleşik biriminde (`comb.kem`, 573 KB, yüklenen
modüller dahil) her `olarak` dönüşümünü kaydetti. 120 dönüşümün tamamı çekirdek ve runtime
kaynağında; sürücülerde hiç yok.

| Dönüşüm | Adet |
|---|---|
| `*T → tam64` | 112 |
| `&T → *T` | 8 |
| `metin`, `Dizi` ya da yapı → tamsayı veya işaretçi (doğrudan) | **0** |

⚠ **Doğrudan dönüşümün yokluğu yetmez.** `(&x) olarak *U` ile `*p` (işaretçi yeniden yorumlama)
aynı işi dolaylı yapar. Bu yüzden 8 `&T → *T` sitesi tek tek okundu:

| Site (işlev) | Yön | Hedef | Bugün |
|---|---|---|---|
| `kis_ileti_metin_al`, `ksv_metin` | tamsayı → `metin` | IPC ya da kullanıcı tamponu | bölge dışı ✓ |
| `ag_dizi8`, `ag_dizi64` | tamsayı → `Dizi` | UEFI sayfasında elle kurulmuş başlık (`ag_dizi_baslik`, `bolge = 0`) | bölge dışı ✓ |
| `ks_adim` (`&b`, `p`), `kabuk` (`ke`) | durum yapısının adresi → küresel `tam64` (`ksv_baglam_adresi`, `ks_yonetim`, `ksv_ekran_adr`) | yığındaki durum yapıları; sistem çağrısı işleyicileri tur içinde bunlara ham işaretçiyle yazar | yığın taşınmaz ✓ |
| **`uefi_bellek_kur_ic`** — `(&sakla) olarak *tam64` | **`Dizi` bitleri → tamsayı** | `uefi_surec_ayirici` küreseli bir `Dizi<CerceveAyirici>`nin veri tamponunu gösterir | açılışta kurulur (ana bölge, R değil); döngüde (`uefi_surec_ayir`, `uefi_surec_iade`) yalnız okunur, kopya geri yazılmaz ✓ |

**Sonuç:** bugün hiçbir gizli kanal **R**'ye dokunmuyor. Ama bu **yapısal değil, tesadüfi**:
- Dil küresel durumu ham işaretçiye ya da `tam64`e itiyor (E011: küresel yalnız skaler veya ham
  işaretçi olabilir).
- `uefi_surec_ayirici` tam olarak "bölge adresi tamsayıda" örüntüsüdür.

Ayırıcıyı döngü içinde yeniden kurmak ya da ham işaretçiyle bölge referansı yazmak gibi bir
sonraki değişiklik bu kanalı R'ye bağlayabilir ve hiçbir kapı fark etmez. A3 bunu **zorlamalıdır**
(§9.4).

⚠ ÖLÇÜM DERSİ: yalnız doğrudan dönüşümleri sayan ilk tarama "risk yapısal olarak yok" sonucunu
veriyordu. Sonuç ancak 8 site tek tek okununca düzeldi. Sayı, sitelerin kendisine bakılmadan
sonuç değildir (D-500).

### A0-3 Canlı durum boyutu
Çekirdek QEMU'da açıldı ve konuk belleği QMP ile okundu. Ölçüm yöntemi:
- Kökler `.data` + `.bss` + **tüm yığın**dır (`__stack_top`'a kadar 512 KB, ölü çerçeveler dahil).
- İşaretleme `Dizi` başlıklarından geçişli ve **muhafazakârdır**: bir başlığın adresine eşit her
  8 baytlık sözcük işaretçi sayılır. Bu yüzden canlılık bir **üst sınırdır**.

| An | Ana bölge kullanımı | Tüm dizi baytı (başlık) | **Canlı dizi baytı (başlık)** | Bölge içi dizi-dışı işaretçi |
|---|---|---|---|---|
| Açılış | 598.312 | 408.920 (1712) | **208.200 (105)** | 69 |
| 9 dosya komutu sonrası | 1.328.960 | 951.304 (6868) | **163.536 (145)** | 117 |

Komutlar: `bagla --yazilabilir`, `cd`, `mkdir`, `cd`, `yaz`, `cp`, `mv`, `cat`, `sil`.

- Canlı dizi grafiği **~160–210 KB** ve 9 komutta artmadı. Açılıştaki fazlalık, açılış verisinin
  ölü yığın çerçevelerinden hâlâ gösterilmesidir. Aynı sürede toplam 2,2 kat büyüdü; 9 komuttan
  sonra dizi baytlarının **%83'ü** çöp (alt sınır).
- **Ölçülmeyen:** `metin` ve dizi dışı yapı tahsislerinin canlı baytı. 117 bölge içi işaretçi
  bunları gösteriyor; çoğunun kısa ad/yol metni olması beklenir ama bu ÖLÇÜLMEDİ. Taşıma maliyeti
  tahmini bu yüzden bir alt sınırdır.
- **Çıkarım:** tahliye başına kopyalanacak canlı veri birkaç yüz KB'tır; eşik tetikli tahliye
  ucuzdur. A **uygulanabilir**.

### A0-4 Yol üstü bulgu: metin runtime'ı bölgeyi yok sayıyor
`metin` üreten yerleşikler çağıranın bölgesini **almıyor**, doğrudan ana (küresel) bölgeye tahsis
ediyor:
- Çekirdekte 4 işlev: `kdl_metin_birlestir`, `kdl_kod_metin`, `kdl_tam_metin`
  (`runtime/kem_dizi_genis.kem`) ve `kdl_metin_kes` (`runtime/mimari/x86_64/kem_gc_yurut.kem`).
- Host'ta `kdl_sizan_al` üzerinden 14 işlev (`runtime/kdl_runtime.c`).

A bunlara dokunmadan uygulanırsa **metin çöpü sızmaya devam eder**. Bu, A2'nin ön koşuludur (§9.3).

## 9. A'nın güncel tasarımı (A0 sonrası)

### 9.1 İlke: "hepsini izle, yalnız R'yi kopyala"
- **Döngü bölgesi R.** Ömür boyu döngünün gövdesindeki çağrılara `%rho` olarak ana bölge yerine
  R geçirilir. Bugün ana bölgeye düşen her tahsis R'ye gider. D-775'in ρ_tur'u kalır: kanıtla
  tur-yerel olan tahsisler orada, tur sonunda ucuzca sıfırlanır.
- **Güvenli nokta: geri dönüş kenarı (tur sonu).** O anda döngü işlevinin altında etkin çağrı
  yoktur; çağrılan çerçevelerdeki geçici referanslar var olamaz. Kökler yalnız döngü işlevinin
  yerelleri ve `&değişken` parametrelerinin gösterdiği değerlerdir.
- **Tetik.** R'nin kullanımı eşiği aşınca tahliye yapılır. Öneri: `max(512 KB, 2 × son tahliyede
  hayatta kalan)`; bu, duraklama sıklığını ve bellek payını birlikte sınırlar. Boşta R büyümez,
  tahliye de olmaz.
- **İzleme tiplidir ve erişilebilir her nesneyi ziyaret eder.** Ana bölgedeki, yığındaki ve UEFI
  sayfalarındaki nesneler de ziyaret edilir. Örneğin elle kurulmuş bir ağ başlığının tamponu
  sonradan R'ye büyümüş olabilir.
- **Yalnız R bloklarının içindeki nesne kopyalanır.** Bölge üyeliği sınaması işaretçinin R'nin
  blok aralıklarından birinde olup olmadığına bakar. Kopyalanan nesneyi gösteren her alan
  **yerinde** güncellenir; ana bölgedeki yapı alanları da buna dahildir.
- **Yönlendirme tablosu (Cheney).** Aynı nesne bir kez kopyalanır; paylaşım ve döngüsel grafik
  korunur (`Dizi<Dugum>` kendi başlığını içerebilir). Ziyaret kümesi R dışı nesneleri de kapsar,
  böylece döngüsel bir R dışı grafikte izleme sonsuza dek yürümez.
- **Kopyalar yeni bir R' bölgesine gider.** R topluca serbest bırakılır ve R' yeni döngü bölgesi
  olur.
- **R dışındaki nesneler asla taşınmaz.** Ana bölgedeki açılış verisinin adresi bir tamsayıda
  gizli olabilir (§8 A0-2, `uefi_surec_ayirici`); onu taşımak sessizce bozardı.
- **Açık soru (A2'de ölçülecek).** Döngüden önce oluşturulmuş bir dizi, döngüde büyürse yeni
  tamponunu kendi bölgesinden (ana bölge, D-770) alır ve büyüme artığı orada kalır. Seçenek: böyle
  bir dizinin yeni tamponu R'den alınır. Başlık yerinde kalır, tampon taşınabilir olur; başlığın
  `bolge` alanı tahliyede R'den yeni bölge R' değerine güncellenir. Bu dizilerin payı ölçülmeden karar verilmeyecek.

### 9.2 Döngü çıkışı
Döngüden `ver` ile (ör. `ver hata(...)`) çıkılırsa R serbest bırakılmaz. Dönen değer R'yi
gösterebilir; bu yüzden R'nin blok listesi kopyalamadan üst bölgeye eklenir (O(1)). KEMGU'da
`kır`/`devam` yoktur.

### 9.3 metin runtime'ı R'ye tahsis etmeli (A2'nin ön koşulu)
§8 A0-4'teki yerleşikler çağıranın `%rho`sunu **parametre olarak** alır. Bu, bölge modeliyle
aynı yoldur.

İş parçacığı yerel bir "etkin bölge" değişkeni **reddedildi**: dinamik kapsamdır, çok çekirdekte
ve `görev`de ayrıca eşzamanlama ister.

ABI değişikliği iki derleyicinin çağrı yerini, çekirdekte 4 ve host'ta 14 işlevi kapsar.
Bayraksız derlemede `%rho` zaten ana bölgedir; davranış değişmez.

### 9.4 Gizli kanal kuralı (A3)
A0-2'deki kanal birlikte çalışan iki katmanla kapatılır:
1. **Derleme zamanı tanısı.** Bayrak açıkken, tahliye edilen döngünün çağrı ağacında bir `&T → *U`
   (U ≠ T) dönüşümü yeni bir tanı alır, eğer T bölge referansı taşıyorsa (`metin`, `Dizi<T>` ya da
   bunları içeren yapı/çeşit). Bu dönüşüm referansın **bitlerini** tamsayıya ya da başka bir
   işaretçi tipine çevirir. `uefi_bellek_kur_ic` açılışta, çağrı ağacının dışında çalışır ve
   etkilenmemeli; bu ölçülecek.
2. **Ham işaretçi yazımında yükseltme.** `güvensiz` blokta ham işaretçi üzerinden yazılan değer
   bölge referansı içeriyorsa önce **ana bölgeye derin kopyalanır**. Böylece ham işaretçinin
   arkasında R'yi gösteren hiçbir şey kalmaz ve izleyicinin göremediği bir kök oluşmaz. Maliyet
   yalnız `güvensiz` yazımlarda ödenir.

   Alternatif: izleme sırasında `tam64` ve `*T` alanları ile küresel sözcükler **muhafazakâr**
   taranır ve R'yi gösterenler **sabitlenir** ("çoğunlukla kopyalayan" tasarım). Daha az derleyici
   işi ister, ama ham işaretçinin gösterdiği R dışı nesnenin içi tipsiz kalır ve izlenemez.
   Tercih 2. katmandır.
3. Döngüye taşınan durumda kapanış (`{fnptr, env}`), `görev`, `kanal` ya da `yetki` varsa o
   döngüde tahliye **kapalıdır**. Bugün böyle bir alan yok (A0-1).

### 9.5 Kopyalayıcı üretimi (A1)
Derleyici her tip için bir taşıma işlevi üretir:
- yapı: alan alan;
- çeşit: etikete göre yük;
- `Dizi<T>`: başlık + `kapasite × eleman_byte` tampon, eleman tipine göre özyineleme;
- `metin`: NUL'a kadar;
- `sonuç` ve `seçimlik`: etiket + yük.

Üretim iki derleyicide birebir olmalı. Kapılar `yapi_diff`, `codegen_diff` ve ASan altında
yeni host birim testleridir.

⚠ Dar eleman (D-778): tampon boyu `eleman_byte`tan okunur; sabit 4 varsayılmaz (D-778'deki kâhin
hatasının dersi). Elle kurulmuş başlıklar (`bolge = 0`, UEFI sayfası) R dışındadır ve taşınmaz.

### 9.6 Çok çekirdek (dalga 18 birleşince kesinleşir)
Dalga 18 (bölüm 16) kullanıcı süreçlerini çekirdekler arasında zamanlıyor:
- Süreç kaydı kuyruk, kullanıcı ve BSP hizmeti arasında **tek sahiple** el değiştiriyor (sıra 10
  kilidi).
- Dosya ve aygıt çağrıları BSP'de sıraya alınıyor.

**Kural: başka bir çekirdeğin tutabileceği hiçbir nesne R'de olamaz.**

Ölçülecek: el değiştiren kayıtlar (süreç kaydı, kuyruk girdileri) hangi bölgeden tahsis ediliyor
ve R referansı taşıyabiliyorlar mı? İki olası sonuç:
- (a) Paylaşılanlar zaten ayrı, çerçeve/heap kilitli yapılarda. O zaman R çekirdek başınadır ve
  tahliye yerel kalır.
- (b) BSP'nin R'sinden tahsis edilip AP'ye geçiyorlar. O zaman el değiştirme noktasında §9.4-2'deki
  gibi bir yükseltme gerekir.

Çekirdekleri durduran bir el sıkışma (stop-the-world) **seçenek değildir**; ilkeyle çelişir.

### 9.7 Aşamalar (güncel)
- **A0 ✓** Fizibilite ölçümü (§8).
- **A1** Kopyalayıcı üretimi; iki derleyici, host. ASan birim testleri şunları ölçer: paylaşım,
  döngüsel grafik, dar eleman, iç içe yapı/çeşit/dizi, R dışı nesneye dokunmama.
- **A2** Döngü bölgesi R + geri dönüş kenarında eşik tetikli tahliye + çıkışta yükseltme + metin
  runtime'ında `%rho` (§9.3); bayrakla açılır. Kapı: kabuk benzeri bir test çekirdeğinde (yoklama
  yok) N tur × geçici tahsis + küçük kalıcı durum; ana bölge büyümesi turlarla **sınırlı**
  kalmalı. Bugün büyüme doğrusaldır, yani kapı kırmızıdan başlar. Aynı program host'ta ASan
  altında da koşar.
- **A3** Gizli kanal kuralları (§9.4) + opak tutamakta tahliyeyi kapatma. Her kural için olumsuz
  fikstür ve sabotaj.
- **A4** KEMGU-OS'ta açma; önce §9.6'daki çok çekirdek kuralı karşılanmalı. Kapılar: tam takım +
  uzun oturum (≥ 1000 komut). README ve CLAUDE.md ilke metni (§7.1) o gün güncellenir.

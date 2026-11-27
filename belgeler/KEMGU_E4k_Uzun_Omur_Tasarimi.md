# E4k — Uzun Ömürlü Döngüde Geçici Belleğin Geri Alınması (Tasarım)

Durum: **KARAR VERİLDİ (Mehmet, 2026-11-27, D-780) — A kabul edildi (koşullu).** A0 fizibilite
ölçümü yapıldı ve A'yı destekliyor, bir düzeltmeyle (§8). Kesin onay dalga 18 (çok çekirdek)
birleşince verilecek (§9.6). Kararlar §7'de, güncel tasarım §9'da, A1 öncesi ölçümler ve ayrıntılı
tasarım §10'da (D-781; D-779'daki bir sayı hatasının düzeltmesi dahil); §1–§6 karar öncesi kayıttır.
Tarih: 2026-11-27. İlgili: D-747, D-756, D-770, D-774/775, D-776, D-777, D-778, D-779, D-780, D-781.

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

`mv` oluşturucu dağılımı (ilk 6): `bayt_sifir` 181× (⚠ D-781 DÜZELTMESİ: bunların hepsi LFN
tamponu DEĞİL; boyuta göre ayrılınca yalnız **17**'si `dizin_coz`un 520 baytlık LFN tamponu, geri
kalanı USB G/Ç geçicileri — §10.1. Burada önceden "`dizin_coz` aynı dizini 181 kez ayrıştırıyor"
yazıyordu ve YANLIŞTI), `onbellek.kopya` 39×, `onbellek.sona` 28×,
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
  al; `mv` başına **17** ayrıştırma (D-781 ölçümü; D-779'da 181 yazılmıştı, yanlıştı) → ~3.
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
     **Ölçüldü (D-786, §10.6): koşul tutuyor.** Kesin onay Mehmet'in.

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
   - ⚠ D-781: bu karar "`mv` başına 181 ayrıştırma" sayısına dayanıyordu; gerçek sayı **17**
     (§10.1). D(1)'in kazancı sanılandan küçüktür; asıl çarpan her işlemin taze `FatSektor` ile
     aynı sektörleri diskten yeniden okumasıdır. Güncel öneri §10.1'de; dalga 19 kapsamı Mehmet'le
     yeniden konuşulacak.

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
- **Canlının kaynağı (ikinci koşum, `scratchpad/a0_buyume.py`):** 9 komuttan sonra canlı 147 başlık
  (163.584 B) var. Bunların **56'sı (131.520 B) açılıştan** geliyor ve A bunları taşımaz (ana bölge).
  **Döngüde oluşan canlı yalnız 91 başlık, 32.064 B.** Tahliye başına KOPYALANACAK dizi verisi bu
  kadardır (+ ölçülmemiş metin); açılış verisi yalnız izlenir.
- **Çıkarım:** tahliye başına kopyalanacak canlı veri onlarca KB, izlenecek canlı veri birkaç yüz
  KB'tır; eşik tetikli tahliye ucuzdur. A **uygulanabilir**.

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
- **Döngü öncesi dizinin büyümesi — ÖLÇÜLDÜ, bugün sorun değil.** Döngüden önce oluşturulmuş bir
  dizi döngüde büyürse yeni tamponunu kendi bölgesinden (ana bölge, D-770) alır ve büyüme artığı
  orada kalır; A bunu toplamaz. Ölçüm (`a0_buyume.py`): açılıştaki 1712 dizi başlığının **hiçbirinin**
  tamponu 9 dosya komutu boyunca yeniden tahsis edilmedi (aynı adresteki başlığın `veri`
  işaretçisi değişmedi; ana bölge boşaltılmadığı için adres yeniden kullanılmaz). Bu yüzden basit
  tasarım yeterli: döngü öncesi dizi kendi bölgesinden büyümeye devam eder. Bir gün gerekirse:
  böyle bir dizinin yeni tamponu R'den alınır, başlık yerinde kalır, `bolge` alanı tahliyede R'den
  yeni bölge R' değerine güncellenir. A2 kapısı bu ölçümü tekrarlamalı (sınıf sessizce büyümesin).

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
   etkilenmemeli; bu ölçülecek. Aynı tipe dönüşüm (`&T → *T`) serbest kalır: referanslar asla bölge
   belleğini göstermez (§10.2-4).
2. **Ham işaretçi yazımında yükseltme.** `güvensiz` blokta ham işaretçi üzerinden yazılan değer
   bölge referansı içeriyorsa ve **hedef yığının dışındaysa** önce **ana bölgeye derin kopyalanır**.
   Böylece ham işaretçinin arkasında R'yi gösteren hiçbir şey kalmaz ve izleyicinin göremediği bir
   kök oluşmaz. Maliyet yalnız `güvensiz` yazımlarda ödenir.
   - ⚠ D-781: hedef yığındaysa yükseltme YOK. Sistem çağrısı işleyicileri tur içinde yığındaki
     durum yapılarına (`ks_yonetim`, `ksv_ekran_adr`) ham işaretçiyle yazar; o hedef ya bir köktür
     (güvenli noktada izlenir) ya da ölü bir çağrılan çerçevesidir (güvenli noktada önemsiz). Her
     yazımı yükseltmek bu yolları ana bölge sızıntısına geri çevirirdi.

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

**Sonuç (D-786, dalga 18 birleştikten sonra ölçüldü): (a).** AP'de koşabilen kod (77 işlev) bölge
taşımıyor, tahsis yapmıyor ve BSP'nin ham işaretçilerine dokunmuyor. Ayrıntı §10.6.

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
- **A4** KEMGU-OS'ta açma; önce §9.6'daki çok çekirdek kuralı karşılanmalı (✓ ölçüldü, D-786/§10.6;
  A4'te `tools/smp_ap_erisim.py` kapıya bağlanır). Kapılar: tam takım +
  uzun oturum (≥ 1000 komut). README ve CLAUDE.md ilke metni (§7.1) o gün güncellenir.

## 10. A1 öncesi ölçümler ve ayrıntılı tasarım (D-781)

### 10.1 D(1) için düzeltilmiş ölçüm
D-779'da `mv` başına "181 `dizin_coz`" yazılmıştı. 181, `bayt_sifir` çağrılarının **toplamıydı**.
Atıf yamalı çekirdekte (`scratchpad/e4k_kaynak_boy.py`) boyuta göre ayrılınca `mv` tek komutta:

| Boy (bayt) | Adet | Anlamı |
|---|---|---|
| 520 | **17** | `dizin_coz` LFN tamponu (`drivers/fs/fat32_dizin.kem:42`) — gerçek ayrıştırma sayısı |
| 16 | 100 | xHCI TRB (USB yığın depolama) |
| 512 | 24 | sektör tamponu |
| 31 | 19 | USB CBW |
| 10 | 19 | SCSI READ/WRITE(10) CDB |
| 64 | 2 | — |

Diğer komutlar: `cp` 10, `yaz` 7, `mkdir` 3, `cd` 1–3 ayrıştırma.

Host ölçümü (`scratchpad/d1olc`; `fat32_sektor_yaz_test.kem`deki yeniden deneme öykünmesi, sayaçlı):
taze FAT32 imajında (70000 ve 131072 sektör) tek yazma planı 6–7 koşu ve koşu başına 1 `dizin_coz`
yapıyor. `mv` = okuma + `Yaz` planı + `DosyaSil` planı (`kd_kopyala`) → çekirdekteki 17 ile tutarlı.

**Sonuç:**
- Ayrıştırmayı `FatSektor` ömrüne bağlayan önbellek `mv`de 17'yi ~3'e indirir. Gerçek, ama D-779'un
  ima ettiğinden ~6 kat küçük bir kazanç.
- Daha büyük çarpan **G/Ç**: her işlem taze bir `FatSektor` ile başladığı için aynı sektörler her
  işlemde diskten yeniden okunur (`mv`de ~19 sektör G/Ç'si; her biri ~5 TRB + CBW + CDB + 512 B).
  İşlemler arası sektör önbelleği `kd_icerik_surumu` ile geçersizlenebilir (emsal:
  `ks_program_hazirla`).
- Bellek tarafını A zaten toplayacak; D'nin kalan değeri **süre** (G/Ç gidiş-dönüşü). Dalga 19
  kapsamı bu sayılarla yeniden değerlendirilecek.

### 10.2 Ölçülen olgular (A1'in dayanağı)
1. **Bölgede duran her `metin` ayrı bir tahsisin başıdır.** Metin üreten yerleşikler her çağrıda taze
   tahsis yapar. Güvenli kodda bir tahsisin ortasını gösteren `metin` yoktur. → Yönlendirme tablosu
   başlangıç adresiyle anahtarlanabilir.
2. **`metin` tipli opak tutamaklar bölge dışındadır:** `bellek_al` → `malloc`, `dosya_oku` → `malloc`
   tamponu, `dosya_ac` → `FILE*`, kilit/semafor/bariyer → runtime nesnesi, `arg_al` → argv. Üyelik
   sınaması kesin kaldıkça (yalnız R blokları) bunlar asla "metin olarak kopyalanmaz". ⚠ Sınama
   gevşetilirse (ör. "ana bölge veya R") bu tutamaklar NUL'a kadar kopyalanıp bozulur → A1 birim
   testi: opak tutamak taşınmaz.
3. **Gizli adres kanallarının hepsi `güvensiz` ister.** `(&x) olarak *U` güvenli kodda E002 verir;
   `güvensiz` içinde hem okuma (dizi başlığının bitlerini `tam64` olarak) hem yazma (dizi işaretçisini
   `tam64` alana) kabul edilir. → Güvenli alt küme tahliye için kendiliğinden sağlamdır; A3 kuralları
   yalnız `güvensiz` kodda çalışır.
4. **Referanslar asla bölge belleğini göstermez.**
   - `f(&değişken xs[0])` → T022 ("elemanı yerele al, değiştir, `dizi_yaz` ile geri yaz").
   - `&değişken d.k` (yerel yapının alanı) kabul edilir; referent yığındadır.
   - `oku(&xs[0])` (değişmez) kabul edilir, ama iki derleyici de elemanı **yığındaki geçici bir
     kopyaya** alıp onun adresini geçirir (`kdl_dizi_al_yapi` → `alloca`).

   → `&T → *T` dönüşümleri (`ks_adim`, `kabuk`) daima yığın adresi üretir ve tahliyeden etkilenmez.
   A3 için tehlikeli olan yalnız bir bölge referansının **bitlerini** başka tip olarak okuyan ya da
   yazan yeniden yorumlamadır (§9.4-1).
5. **C derleyicisinde yığın dizisi yalnız açıklamasız dizi literali yerelinde çıkar**
   (`değişken a = [1,2,3]` → `alloca [3 x i32]`). Açıklamalı `Dizi<T>` ve yapı alanı her zaman heap
   başlığıdır; self-host heap-tekdüzedir. → Döngüye taşınan böyle bir yerel by-value `[N x T]` kök
   olarak izlenir (elemanları referans içerebilir) ve taşınmaz.
6. **Elle kurulmuş başlık** (UEFI sayfası, `bolge = 0`) döngüde büyürse yeni tamponunu bugün
   çağıranın bölgesinden (= R) alır. Başlık izlenmeyen bellekte durduğu için bu, R'ye gizli bir
   işaretçi olur. Öneri (A3, önce ölçülecek): `bolge = 0` başlığın büyümesi ana bölgeden yapılır
   (ana bölge taşınmaz → güvenli; sızar ama UAF yok).

### 10.3 Taşıyıcının yapısı
- Derleyici her tip için bir **yürüyücü** üretir: `__tasi_<T>(ctx, yer)`. `yer`, bir T değerinin
  bulunduğu bellek adresidir (yığın yuvası, yapı alanı, dizi elemanı).
  - **Referans tipler** (`Dizi`, `metin`): `*yer` R'de mi? Evetse yönlendirme tablosuna bakılır;
    kayıt yoksa nesne R''ye kopyalanır ve kaydedilir; `*yer` yeni adresle değiştirilir. Ardından
    nesnenin (yeni ya da R dışı) içi iş listesine konur.
  - **Değer tipleri** (yapı, çeşit, `sonuç`, `seçimlik`, `[N x T]`): yerinde, alan alan yürünür;
    çeşit/`sonuç`/`seçimlik` için etikete göre yalnız etkin yük.
- **Özyineleme yalnız tip iç içeliği kadar derindir** (durağan). Dinamik derinlik (`Dizi<Dugum>`
  ağacı, döngüsel grafik) iş listesinden geçer. Girdi = (dizi içi yürüyücü, başlık adresi).
- **Ziyaret kümesi R dışı başlıkları da kapsar**; böylece R dışı döngüsel bir grafikte izleme sonsuza
  dek yürümez.
- **Dizi tamponunun boyu** `kapasite × eleman_byte`tır; `eleman_byte` başlıktan okunur (D-778).
  Başlık R dışında olup tamponu R'de olabilir (madde 6) → tampon ayrıca sınanır; başlık yerinde
  kalır, yalnız `veri` güncellenir.
- **Bağlam (ctx):** R'nin blok aralıkları (eşik/64 KB ≈ 8 blok → doğrusal tarama yeter), hedef R',
  yönlendirme tablosu + ziyaret kümesi + iş listesi. Son üçü geçici bir bölgededir ve tahliye
  sonunda topluca serbest bırakılır.
- **Üretim kümesi:** her ömür boyu döngünün köklerinden ulaşılan tiplerin kapanışı; generic
  örnekler monomorftur. Kapanışta kapanış/`görev`/`kanal`/`yetki`/`tekkez`/`*T` alanı varsa o
  döngüde tahliye kapalıdır. Bu sessiz olmaz: derleyici bir bilgi notu basar.

### 10.4 A1 test planı (host, ASan)
- **Runtime birim testleri** (C, `test_kdl_bolge.c` tarzı): üyelik sınaması, yönlendirme tablosu,
  metin ve dizi kopyası (eb 1/2/4/8 + yapı elemanı), R dışı nesneye dokunmama, opak tutamağın
  taşınmaması, paylaşım (aynı dizi iki alanda → tek kopya), döngüsel grafik, tahliye sonrası R'nin
  tamamen boş olması.
- **Derleyici paritesi:** üretilen yürüyücülerin yapısal paritesi (`yapi_diff`).
- **Davranış (A2 ile birlikte):** döngülü host programı bayrakla koşar. Çıktı bayraksızla birebir
  aynı olmalı, ana bölge sınırlı kalmalı, ASan temiz olmalı. Yanlış taşıma UAF olarak görünür ve
  ASan bu sınıfı GÖRÜR, çünkü R gerçekten serbest bırakılır.

### 10.5 Çok çekirdek — ön ölçüm (dalga 18 bölüm 16, birleşmeden salt okuma)
- Çekirdekler arası el değiştiren süreç kaydı **ham bellektir**: alanlar `kxm_o64`/`kxm_y64` ile
  sabit ofsetlerden okunup yazılır (durum, PID, CR3, dilim, vektör, hata, RIP, CR2, çıkış kodu).
  Zamanlayıcı yolunda bölge nesnesi el değiştirmiyor → §9.6 sonuç (a) olası: R çekirdek başına
  (kabuk döngüsü BSP'de), tahliye yerel.
- Dosya ve aygıt çağrıları BSP'de sıraya alınıp kabuk döngüsü içinde hizmet ediliyor.
- **Kalan soru (dalga 18 birleşince ölçülecek):** AP'de çalışan bir işleyici `ks_yonetim`,
  `ksv_ekran_adr` ya da `ksv_baglam_adresi` ham işaretçileri üzerinden BSP'nin durum yapılarına
  (ve onların R'deki dizilerine) erişiyor mu? Erişiyorsa o erişim BSP'nin tahliyesiyle yarışır.
  Ayrıca AP kodu kendi tahsislerinde hangi `%rho`yu kullanıyor? → **Yanıt §10.6: erişmiyor;
  AP kodu hiç `%rho` taşımıyor ve hiç tahsis yapmıyor.**

### 10.6 Çok çekirdek — kesin ölçüm (dalga 18 birleştikten sonra, D-786)
**Sonuç: §9.6 (a).** Çekirdekler arasında el değiştiren hiçbir nesne bölge nesnesi değildir. R
yalnız BSP'nin kabuk döngüsüne aittir ve tahliye yerel kalır. D-780'in (ii) koşulu (§7.1)
karşılanmıştır; çekirdekleri durduran bir el sıkışma gerekmez.

**Yöntem.** IR, `test/bm_x86_uefi_harness.sh`'ın birleştirmesiyle birebir üretildi:
1. harness'taki `RT` listesi,
2. kabuk ekleri (`kem_surec_hw.kem`, `kem_rtc_hw.kem`, `kem_os_x86_blok_ortak.kem`,
   `kem_os_x86_vfs_ortak.kem`),
3. `kem_os_x86_kabuk.kem`,
4. `test/x86_heap_bagla.py`,
5. `build/kemgu --llvm --mimari x86_64-none`.

Birim 12.618 satır kaynak, 230.271 satır IR ve 1.379 işlevdir. `tools/smp_ap_erisim.py` AP
köklerinden fazla yaklaşımlı erişilebilirlik hesaplar: her `@` başvurusu ve asm metnindeki
`call AD` / `AD(%rip)` bir kenar sayılır. Fazla yaklaşım yanlış-negatif üretmez.

AP kökleri (kaynakta doğrulandı):

| kök | neden AP'de koşar |
|---|---|
| `kem_ap_giris` | AP giriş noktası (`smp_trampolin_yaz`: `leaq kem_ap_giris(%rip)`) |
| `kxs_smp_isci` | AP zaman kancası (`kzs_baslat`: `leaq kxs_smp_isci(%rip)` → `smp_zaman_kanca`) |
| `smp_ap_zaman_irq` | vektör 49, AP yerel zamanlayıcısı |
| `kem_ipi_isle` | vektör 80–83 (IPI, TLB vurgulama) |
| `kdl_istisna_isle` | çekirdek kipindeki istisnalar (isr0..31) |
| `kdl_panik` | AP'den çağrılan panik yolu |

Aygıt kesmeleri AP'de koşmaz. IO-APIC hedefi BSP kurulumunda okunan yerel APIC kimliğidir
(`kem_apic_hw.kem`, `kap_yol_kur` çağrıları). MSI adresi de kuran işlemcinin, yani BSP'nin
kimliğini taşır (`kem_os_x86_kabuk.kem`, `0xFEE00000 | id<<12`). Yine de ölçüldü:
`kdl_disk_irq` + `kdl_xhci_irq` kümesi (3 işlev) de temiz.

| ölçüm | sonuç |
|---|---|
| AP-erişilebilir tanımlı işlev | 77 |
| `ptr %rho` alan (bölge taşıyan) | **0**: hepsi `çıplak`; E013 bunu durağan olarak zorlar |
| `ks_yonetim` / `ksv_ekran_adr` / `ksv_baglam_adresi` / `kxs_ek_vekil` başvurusu | **0** (kontrol: tüm IR'da 3/5/2/2 işlev başvuruyor) |
| tahsis girişi (bölge, dizi, metin üreticisi, `malloc`) | **0**; `declare` edilmiş dış çağrı da 0 |
| AP'nin okuduğu küreseller | yalnız sayıl/ham tutamaçlar: `kap_lapic`, `kap_tsc_ms`, `kdl_cekirdek_bsp`, `kdl_dilim_tablo`, `kdl_istisna_ileti`, `kgun_*`, `kis_*`, `smp_*`, `ekran_panik_taban` |

Ayırt edicilik (D-425):
1. **Sabotaj.** `kxs_smp_isci` gövdesine `eğer ks_yonetim == 12345 { ver 0; }` eklendi. Araç
   `BULGU kxs_smp_isci: ham=['ks_yonetim']` verdi, çıkış 1.
2. **Pozitif kontrol.** Kök `ks_adim` (BSP adımı) seçildi. 611 işlev çıktı; 375'i `%rho` alıyor;
   üç ham işaretçi ile `kdl_bolge_olustur` ve `kdl_metin_birlestir` bulundu.
3. **Yanlış kök.** Çıkış 2 ("ölçüm geçersiz").

**Mekanizma** (kod okuması, ölçümle tutarlı):
- AP yalnız atanan kullanıcı çerçevesini yürütür (`kxs_smp_isci` → `kxs_smp_gir`).
- Her tuzak ve sistem çağrısı `kdl_smp_dilim_yakala` (C) ile çekirdek başı GS alanına yazılır.
  AP vektörü, hata kodunu ve CR2'yi 64 baytlık zamanlayıcı kaydına koyar ve evreyi 3'e
  ("BSP hizmeti bekliyor") çeker. Bu kayıt sabit çerçeve belleğindedir (`ipi_oku`/`ipi_yaz`).
- Sistem çağrısını yalnız BSP yürütür, `kzs_hizmet` içinde (`kem_surec_hw.kem`).
  `ks_yonetim`, `ksv_baglam_adresi` ve `kxs_ek_vekil` bu çağrının hemen öncesinde kurulur, hemen
  sonrasında sıfırlanır (`kem_os_x86_kabuk.kem`, `ks_adim`).
- Süreç kaydı (durum, PID, CR3, dilim, vektör…) ham bellektir (`kxm_o64`/`kxm_y64`), bölge nesnesi
  değil.
- AP'nin panik ekranı `ekran_panik_taban`ı kullanır (D-755, çerçeve ayırıcısından);
  `ksv_ekran_adr`a dokunmaz.
- Çekirdek günlüğü (`kgun_adres`) `malloc` ile alınır, yani R'nin dışındadır.

**A3 (§9.4) için yol üstü girdiler** (çok çekirdekle ilgisiz, BSP içi):
- `ks_yonetim`, `ksv_baglam_adresi` ve `kxs_ek_vekil` **dinamik kapsamlıdır**: yalnız bir hizmet
  çağrısı süresince kuruludurlar. Sistem çağrısı dağıtıcısı `ksv_dagit`ten (fazla yaklaşımla 695
  işlev) `ks_adim` ya da `ks_bekle`ye yol yoktur; bu pencerede döngünün geri dönüş kenarına,
  yani tahliye noktasına ulaşılmaz. Hedefleri de yığındadır (`&b` yerel kopyası, çağıranın `p`
  yapısı) ve §9.4-2'deki yığın-hedef muafiyetine uyar. A3 bu gözlemi bir kural olarak
  sabitlemelidir: pencere içinde tahliye noktası olmamalı.
- `ksv_ekran_adr` **uzun ömürlüdür**: `kabuk(...)` girişinde bir kez kurulur ve döngü boyunca
  yaşar. Hedefi `ke: &değişken KabukEkran`, yani çağıranın yapısıdır. Hedefin yığında olduğu
  A3'te doğrulanmalıdır. Yığındaysa §9.4-2 muafiyeti geçerlidir; değilse yükseltme gerekir.

**A4 için.** Kural ancak tahliye çekirdekte açılınca yük taşır. A4, `tools/smp_ap_erisim.py`'yi
bu birleşik birimin IR'ında kapıya bağlamalıdır; araç bugün kapı **değildir** (D-446). Yeni bir
AP girişi eklendiğinde (yeni IPI vektörü, yeni kanca) kök listesi güncellenmelidir.

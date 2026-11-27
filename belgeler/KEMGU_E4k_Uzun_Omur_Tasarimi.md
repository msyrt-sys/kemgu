# E4k — Uzun Ömürlü Döngüde Geçici Belleğin Geri Alınması (Tasarım)

Durum: **TASLAK — karar bekliyor (Mehmet).** Tarih: 2026-11-27. İlgili: D-747, D-756,
D-770, D-774/775, D-776, D-777, D-778.

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

### D. Algoritmik azaltma (A/B/C'den bağımsız, hemen yapılabilir)
- `dizin_coz`: dizin ayrıştırma sonucunu (FAT nesli + küme numarasıyla anahtarlı) önbelleğe
  al; `mv` başına 181 ayrıştırma → birkaç.
- G/Ç bekleme yoklaması: yanıt dizisini her turda üretmek yerine sabit tampona yaz (D-747
  seçenek (a)'nın dar biçimi — yalnız **durum yoklaması** yanıtları; veri yanıtları değil).
- Kazanç: sabit çarpan (tahminen 2–4×); mekanizmayı kapatmaz ama A'nın taşıma sıklığını ve
  gerçek donanımdaki zamanlama bağımlılığını düşürür.

## 5. Öneri

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

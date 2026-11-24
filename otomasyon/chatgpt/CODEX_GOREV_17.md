# CODEX GÖREVİ 17 — ağ yığını: NIC sürücüleri, Ethernet/ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP, soket ABI'si, ağ programları

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## ⏱ Bu dalga ÇOK UZUNDUR — çalışma biçimi
- Dalga 16 on üç bölümü bitirdi. Bu dalga da **on üç bölümdür, HEPSİ zorunlu ve sırayla yapılır.**
  Ağ yığını bölümleri birbirinin üstüne kurulur (sürücü → L2/L3 → UDP → TCP → soket → programlar), sırayı
  bozma. 15+ saat normaldir. Bir bölümü bitirince **durma, rapor yazıp bekleme — sonraki bölüme geç.**
- Her bölümün sonunda: (1) o bölümün kapıları + sabotajları, (2) ayrı commit
  (`SURUCU(codex): Dalga 17 bölüm N — ...`), (3) `codex17_ilerleme.md`'ye kısa not (yapılan, ölçümler,
  sabotaj sonuçları, süre). Dalga yarıda kesilse bile biten bölümler kullanılabilir olmalı.
- Gerçekten takılırsan (60 dk ilerleme yok): `codex17_bulgular.md`'ye yaz, bölümün yapılabilen kısmını
  kapılı bırak ve **sonraki bölüme geç**.
- **Her sabotajı gerçekten uygula, çıktısını yaz.** Uygulamadığın sabotaj varsa bunu AÇIKÇA yaz; "benzer bir
  sabotaj" ile değiştirme. Son iki dalgada istenen bulanıklaştırma sabotajı uygulanmadı ve Claude
  uyguladığında birinde **kapının kör olduğu** ortaya çıktı (D-767 S223: korunma kaldırılınca bulanıklaştırıcı
  yeşil kaldı, çünkü korpus o şekli hiç üretmiyordu). Sabotaj sessiz kalırsa önce korpusu/kapıyı düzelt.
- **Draft PR'ı açtıktan sonra son commit'i değiştirip zorla gönderme** (dalga 16'da ef40ab7 böyle geldi);
  ek değişiklik gerekiyorsa yeni bir commit ekle.
- Draft PR'ı yalnız on üç bölüm bittikten (ya da her biri için takılma notu yazıldıktan) sonra aç.

---

## 0. Hedef

Donanım yol haritası büyük ölçüde kapandı (D-767: 111 maddenin 94'ü tamam; açık kalanlar ertelenmiş, fiziksel
ya da Claude'da). Kullanılabilir bir sistemin en büyük eksiği **ağ**. Bu dalga KEMGU-OS'a ağ yığınını ekler:
yaygın NIC'ler için sürücüler, Ethernet/ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP, kullanıcı programlarına soket ABI'si
ve ağ programları (`ping`, `ag`, `cozumle`, `indir`, `sunucu`, `nc`).

Her katman **sans-IO** (saf durum makinesi, `drivers/ag/`), host'ta iki derleyiciyle birim testli ve
bulanıklaştırılmış; QEMU'da **bağımsız kâhinle** (host'ta elle yazılmış Python Ethernet eşi + pcap kaydı)
doğrulanır.

Kapsam DIŞI: IPv6, TLS, yönlendirme (çoklu arayüz arası iletim), IP parçalarını birleştirme (parça gelirse
sayılıp atılır), multicast/IGMP, Wi-Fi, Realtek RTL8111/8168 (QEMU'da öykünmesi yok — kâhinsiz sürücü yazma).

---

## 1. Bugünkü durum (Claude ölçtü — varsayma, yine de kendin doğrula)

- Dalga 16 (D-767/D-768) çıktıları: klavye tamamlama, PCI ECAM + köprü + BAR, kesme kayıt arayüzü
  (`kesme_kaydet`/`kesme_ayir`/`kesme_birak`) + AHCI MSI + NVMe MSI-X, disk hata kurtarma (blkdebug), USB 2 hub,
  ramdisk + `/ram`, ACPI S5, PAT/WC, bulanıklaştırma korpusları, UART TX sınırı, MMIO volatile kapısı,
  `bilgi`/`duzenle`/`dosyalar` eklemeleri. Ölçümler `otomasyon/chatgpt/codex16_bulgular.md`'de.
- **ABI 72'ye kadar dolu** (71 `kopyala`, 72 `tasi`). Bu dalganın yeni çağrıları **73-99**.
- Var olan ağ kodu: `runtime/kem_virtio_net.kem` (ARM KEMGU-OS'un virtio-mmio yolu, ARP + ICMP ping içerir —
  ARM'de "PING CANLI" kanıtı). x86'da ağ yok. `stdlib/ag.kem` host programları içindir (Winsock/BSD soket
  sarmalı), çekirdekle ilgisi yok. Protokol mantığını ARM dosyasına değil **yeni `drivers/ag/`** altına,
  sans-IO olarak yaz; ARM yolu bu dalgada değişmez (isteğe bağlı: ARM'in ARP/ICMP'sini yeni modüle bağlamak —
  yaparsan `calistir_kem_os_arm` yeşil kalmalı).
- QEMU 8.2.2 bu makinede şunları sunuyor (Claude doğruladı, hepsi açılıyor):
  - NIC'ler: `e1000` (82540EM), `e1000e` (82574L), `igb` (82576), `rtl8139`, `virtio-net-pci`, `usb-net`.
  - **Ham Ethernet'i host'a taşıyan arka uç** (Python eşi için):
    `-netdev dgram,id=n0,local.type=inet,local.host=127.0.0.1,local.port=<A>,remote.type=inet,remote.host=127.0.0.1,remote.port=<B>`
    → QEMU her Ethernet çerçevesini UDP datagramı olarak `<B>`ye yollar, `<A>`dan gelenleri konuğa verir.
  - **pcap kaydı:** `-object filter-dump,id=f0,netdev=n0,file=<yol>.pcap` (dosya açılışta oluşuyor).
  - **slirp:** `-netdev user,id=u0,hostfwd=tcp:127.0.0.1:<P>-10.0.2.15:80` (konuk 10.0.2.15, ağ geçidi
    10.0.2.2, DNS 10.0.2.3, slirp'in kendi DHCP sunucusu — bu adresleri kendin de doğrula).
  - `scapy` KURULU DEĞİL. Python eşi çerçeveleri `struct` ile elle kurar/ayrıştırır — bu iyidir: konuktaki
    koddan bağımsız ikinci bir uygulama olur. Kurma, ekleme.
- **Heap (E4k — açık, Claude'da):** çekirdek heap'i geri verilmiyor. Ağ halkaları, paket tamponları ve bağlantı
  tabloları **fiziksel çerçeve ayırıcısından, sabit boyutlu ve önceden ayrılmış** olmalı; paket başına heap
  tahsisi YOK. Her ağ senaryosunda başlangıç/son heap ve boş çerçeve sayısını ölç ve yaz.
- Claude'un tam takımı (D-767) 4 sa 38 dk, seninki 52 dk sürdü; fark ölçülmedi. Uzun sürmesi normal.

---

## 2. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-769     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex17 -b codex/ag-dalga17 origin/main && cd ../kemgu-codex17
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_v1 calistir_bm_x86_uefi_pc calistir_bm_x86_saglamlik calistir_kem_os_x86   # taban
date > otomasyon/chatgpt/codex17_ilerleme.md
```
Tek dal, tek draft PR. ⚠ Claude bu dalga sırasında derleyicide (E4k) çalışabilir; **dalını dalga ortasında
main ile birleştirme** — Claude entegrasyonda birleştirir.

---

## 3. Okunacaklar
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md`, `KEMGU_OS_Mimari.md`, `KEMGU_OS_Syscall_ABI.md`,
  `KEMGU_OS_Program_Yazma.md`, `KEMGU_OS_Sabotaj_Envanteri.md`, `X86_Fiziksel_Donanim_Kontrol_Listesi.md`.
- `LOOP.md` D-763..D-768, `otomasyon/chatgpt/codex16_bulgular.md`.
- `runtime/kem_virtio_net.kem` (ARP/ICMP deseni), `runtime/mimari/x86_64/kem_pci_hw.kem`,
  `kem_gc_yurut.kem` (sans-IO yürütücü + PIT kalibreli bekleme), kesme kayıt arayüzü, `drivers/usb/*`
  (hub ve sınıf sürücüsü deseni), `drivers/blok` (aygıt arayüzü deseni).
- `test/ornekler/kem_os_x86_kabuk.kem`, `kem_os_x86_vfs_ortak.kem`, `kem_os_x86_blok_ortak.kem`,
  `runtime/kullanici/x86_64/*.kem`, `test/kullanici/*.kem`, `tests/qemu/*.py`, `test/bm_x86_*_harness.sh`.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
- `drivers/**` saf (sans-IO); **yeni `drivers/ag/`** serbest.
- `runtime/mimari/x86_64/*.kem`, `runtime/kullanici/x86_64/*.kem`, `linker/user-x86_64.ld`.
- `runtime/kem_gorev_cekirdek.kem` (sistem çağrısı tablosu; arm64 davranışı değişmez).
- `boot/start_x86_64.S`: YALNIZ yeni kesme vektörü koçanı; `runtime/kdl_kesme.c`: YALNIZ
  `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- `test/ornekler/kem_os_x86_*.kem`, `test/kullanici/*.kem`, `test/bm_x86_*_harness.sh`, `tests/qemu/*`,
  `tests/drivers/**`, `tools/codex/*`.
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86 CAT listeleri), `test/check_kapisi.sh`,
  `test/baremetal_diff_harness.sh` sınıflandırma satırları.
- `belgeler/KEMGU_OS_*.md` (yeni `KEMGU_OS_Ag.md` dahil), `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`,
  `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` (yeni H13 bölümü + işaretler).
- YASAK: `src/`, `selfhost/`, `boot/efi/`, diğer `linker/` dosyaları, runtime C dosyaları (bu dalgada C
  değişikliği gerekmez). Derleyici hatası → `codex17_bulgular.md`'ye C17-xxx + küçük tekrar üretimi (C ve
  self-host komutu, beklenen/gerçek sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri (önceki dalgalarınkiler geçerli, ek olarak)
1. **Katman başına sans-IO modül:** `drivers/ag/{eth,arp,ipv4,icmp,udp,dhcp,dns,tcp}.kem` saf; girdi = bayt
   dizisi + zaman, çıktı = gönderilecek çerçeveler + olaylar. NIC sürücüleri (`drivers/ag/e1000.kem` vb.)
   `GcIstek`/`GcYanit` desenini kullanır. Yürütücü (`runtime/mimari/x86_64/`) yalnız G/Ç ve kesme yapar.
2. **Ağ baytları büyük-endian'dır.** Ortak `drivers/ag/bayt.kem` (be16/be32 oku/yaz, internet sağlaması)
   bir kez yazılır, her katman onu kullanır (D-407). Sağlama hesabı RFC 1071; host kâhini aynı hesabı
   bağımsız olarak Python'da yapar.
3. **Sabit bellek:** halkalar, tamponlar, ARP önbelleği, soket ve TCP bağlantı tabloları sabit boyutlu, açılışta
   fiziksel çerçevelerden ayrılır; doluysa belgelenmiş hata kodu (sessiz atma yok; atılan paket sayaçta).
4. **Her bekleme sınırlı:** ARP çözümü, DHCP, DNS, TCP bağlantısı ve yeniden iletim zaman aşımlı; zaman
   kaynağı dalga 15'in PIT kalibreli TSC'si. Hiçbir ağ olayı kabuğu asmaz.
5. **Sayaçlar:** arayüz başına alınan/gönderilen çerçeve, bayt, sağlama hatası, kısa çerçeve, bilinmeyen
   protokol, halka dolu, parça (atıldı) sayaçları; `ag` komutu ve `donanim` raporu gösterir.
6. ABI geriye uyumlu büyür (5-72 değişmez; yeni çağrılar 73-99); `tests/qemu/surec_abi.py` tutarlılık kapısı
   her yeni çağrıyı kapsar.
7. **Bağımsız kâhin:** ağ kâhinleri konuğun kendi raporuna değil, host'taki Python eşinin gözlemine ve
   `filter-dump` pcap kaydının bağımsız ayrıştırmasına dayanır (sağlama toplamları, sıra numaraları, bayt
   içerikleri host'ta yeniden hesaplanır).

---

## 6. İş listesi — on üç bölüm, HEPSİ zorunlu, sırayla

### Bölüm 1 — ağ yol haritası, belge iskeleti, host eşi
- Yol haritasına **H13 Ağ** bölümü ekle (bu dalganın her bölümü için madde + kanıt kapısı adı; IPv6/TLS
  "kapsam dışı" maddesi). H12b'yi kapat: USB sıfır uzunluk sabotajı D-767'de Claude tarafından S223 olarak
  uygulandı (`belgeler/KEMGU_OS_Sabotaj_Envanteri.md`), korpusa sınır örneği eklendi — kanıtı ekle.
- `belgeler/KEMGU_OS_Ag.md` iskeleti: katmanlar, adresleme, zaman aşımları, sınırlar, hata kodları (bölümler
  ilerledikçe doldurulur).
- **Host Ethernet eşi** `tests/qemu/ag_es.py`: `-netdev dgram` üzerinden konuğun çerçevelerini alır/gönderir;
  ARP yanıtı, ICMP echo (iki yön), UDP yankısı, DHCP sunucusu, DNS sunucusu, TCP sunucu/istemci (Bölüm 7'de),
  deterministik **kayıp/sıra bozma/çiftleme** enjeksiyonu (sabit tohum). Elle `struct` ile; `scapy` yok.
  Kendi birim testleri (bilinen pcap baytlarına karşı) ile gelir.
- pcap ayrıştırıcı `tests/qemu/pcap_oku.py`: her çerçeve için Ethernet/IPv4/UDP/TCP/ICMP başlıkları + sağlama
  toplamlarının bağımsız doğrulaması.
- Sabotaj: eşin IPv4 sağlama hesabında bir baytı atla → eşin kendi birim testi KIRMIZI (eşin de kâhin
  olarak güvenilir olduğunu gösterir).

### Bölüm 2 — e1000 (82540EM) sürücüsü
- `drivers/ag/e1000.kem` (sans-IO): aygıt sıfırlama (CTRL.RST, zaman aşımlı), MAC okuma (RAL0/RAH0; EEPROM
  gerekirse EERD), RX/TX tanımlayıcı halkaları (fiziksel çerçevelerde, hizalı), RCTL/TCTL/TIPG, bağlantı
  durumu (STATUS.LU), kesme (ICR/IMS) — Bölüm 3'teki kayıt arayüzüyle MSI ya da INTx/IOAPIC; yoklama yedeği.
- Kabuk: açılışta NIC bulunursa `ag` komutu MAC, bağlantı ve sayaçları gösterir; `donanim` raporunda NIC satırı.
- QMP `set_link` ile bağlantı kopar/geri gelir → konuk bunu algılar ve raporlar; kabuk asılmaz.
- Kâhin `calistir_bm_x86_e1000`: QMP'den (`query-rx-filter` ya da HMP `info network`) okunan MAC = konuğun
  raporu; konuk ham bir test çerçevesi yollar → Python eşi ve pcap birebir aynı baytları görür; eş bir çerçeve
  yollar → konuk sayacı ve içerik raporu birebir.
- Sabotajlar: TX tanımlayıcısında EOP bitini kurma → çerçeve gitmez, KIRMIZI; RX halka kuyruğunu (RDT)
  ilerletme → ikinci çerçeveden sonra alım durur, KIRMIZI.

### Bölüm 3 — e1000e (82574L) + virtio-net-pci
- e1000e: e1000 ile ortak kısmı paylaş (D-407), farklı yazmaç/başlatma adımlarını ayrı tut (seçimini ve
  82574L'nin gerektirdiği farkları belgeye yaz). MSI-X varsa kullan.
- virtio-net-pci (x86 PCI aktarımı, modern ya da legacy — seçimini yaz): virtqueue'lar mevcut virtio kodundan
  yararlanabilir ama aktarım katmanı PCI'dır.
- Sürücü seçimi PCI satıcı:aygıt kimliğiyle; desteklenmeyen NIC `donanim` raporunda adıyla "desteklenmiyor"
  olarak listelenir (Realtek dahil).
- Kâhin `calistir_bm_x86_nic_matris`: {e1000, e1000e, virtio-net-pci} × {C, self-host} × ≥2 açılış;
  her biri Bölüm 2'nin çerçeve gidiş-dönüş testini geçer.
- Sabotaj: e1000e'de RX halka taban adresinin üst 32 bitini yazma → (4 GiB üstü çerçevelerle çalışan
  yapılandırmada) alım bozulur → KIRMIZI. Bunu tetikleyecek bir yapılandırma (yüksek RAM) kur.

### Bölüm 4 — Ethernet, ARP, IPv4, ICMP + `ping`
- ARP: istek/yanıt, önbellek (sabit boy, yaşlanma), çözüm bekleyen paket kuyruğu (sınırlı), yeniden deneme +
  zaman aşımı, gereksiz (gratuitous) ARP açılışta.
- IPv4: başlık doğrulama (sürüm, IHL, toplam boy, sağlama), TTL, seçenekler atlanır, parça gelirse sayılıp
  atılır; varsayılan ağ geçidi üzerinden yönlendirme kararı (yerel alt ağ mı değil mi).
- ICMP: echo yanıtı (konuğa gelen ping), echo isteği (konuktan giden ping), hedefe ulaşılamaz mesajlarını
  sayma.
- Statik yapılandırma komutu (`ag ayarla <ip>/<önek> <geçit>`) — DHCP Bölüm 5'te.
- Kullanıcı programı `ping` (sayı, aralık, zaman aşımı, RTT istatistiği) — Bölüm 8'deki soket ABI'si henüz
  yoksa geçici bir sistem çağrısıyla başla ve Bölüm 8'de soket ABI'sine taşı.
- Kâhin `calistir_bm_x86_ag_l3`: (a) Python eşi konuğa 100 ping atar, 100 yanıt alır, sağlamalar pcap'te
  doğru; (b) konuk `ping` eşe 20 ping atar, eş 20 isteği görür; (c) slirp ile `ping 10.0.2.2` (slirp kendi ağ
  geçidinde ICMP yanıtlar — doğrula); (d) ARP önbelleği yaşlanınca yeniden istek gider (pcap'te sayılır).
- Sabotajlar: IPv4 sağlamasında taşıma katlamasını (carry fold) atla → eş paketleri reddeder, KIRMIZI;
  ARP yanıtında gönderen MAC'i hedef alanına yaz → eşin ARP tablosu yanlış dolar, KIRMIZI.

### Bölüm 5 — UDP, DHCP istemcisi, DNS istemcisi
- UDP: bağlantı noktası tablosu (sabit), sağlama (sözde başlıkla), kapalı porta gelen paket için ICMP
  ulaşılamaz (sayaçla sınırlı).
- DHCP istemcisi: DISCOVER/OFFER/REQUEST/ACK, kira süresi, yenileme (T1) zamanlayıcısı, seçenekler (alt ağ,
  geçit, DNS, kira); NAK ve zaman aşımında yeniden deneme; açılışta otomatik, `ag dhcp` ile elle.
- DNS istemcisi: A sorgusu, yanıt ayrıştırma (sıkıştırma işaretçileri — **işaretçi döngüsüne karşı sınır**),
  CNAME zinciri (sınırlı), önbellek (TTL), zaman aşımı + yeniden deneme; `cozumle <ad>` komutu.
- Kâhin `calistir_bm_x86_ag_udp`: (a) Python eşi DHCP sunucusu: konuk adres alır, kira yenilemesini zamanında
  yapar (kısa kira ile), pcap'te seçenekler doğru; (b) slirp DHCP ile 10.0.2.15; (c) eşin DNS sunucusu
  sıkıştırmalı + CNAME'li yanıtlar verir, konuk doğru IP'yi bulur; (d) UDP yankı: 1000 datagram, içerikler
  birebir.
- Sabotajlar: DNS işaretçi döngüsü sınırını kaldır → bulanıklaştırıcı/eş döngülü yanıtla asılmayı yakalar
  (zaman aşımı), KIRMIZI — **gerçekten uygula**; DHCP yenilemesini T1 yerine kira sonunda yap → kâhin
  zamanlamayı yakalar, KIRMIZI.

### Bölüm 6 — TCP (temel)
- Durum makinesi (RFC 793 + 9293 sadeleştirilmiş): LISTEN, SYN_SENT, SYN_RECEIVED, ESTABLISHED, FIN_WAIT_1/2,
  CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT (sınırlı süre), RST işleme.
- İstemci (aktif açma) ve sunucu (pasif açma, bekleme kuyruğu sınırlı), MSS seçeneği, ISN (zaman + sayaç),
  sıra numarası aritmetiği **32 bit sarmalı** (karşılaştırmalar sarmaya dayanıklı).
- Kayan pencere: alıcı penceresi (sabit tampon), göndericide pencereye uyma, sıfır pencere yoklaması.
- Bağlantı tablosu sabit boyutlu; dolu → yeni SYN'e RST + sayaç.
- Kâhin `calistir_bm_x86_tcp_temel`: Python eşi sunucu ve istemci; el sıkışma, 64 KiB veri iki yönde (SHA256
  host'ta), düzgün kapanış (FIN iki yön), RST ile kapanış, kapalı porta SYN → RST.
- Sabotajlar: sıra numarası karşılaştırmasını sarmaya dayanıksız yap (düz `<`) ve ISN'yi 2^32'ye yakın seç →
  aktarım bozulur, KIRMIZI; FIN'in tükettiği sıra numarasını sayma → kapanış asılır, zaman aşımıyla KIRMIZI.

### Bölüm 7 — TCP (güvenilirlik)
- Yeniden iletim zamanlayıcısı (RTO: SRTT/RTTVAR, RFC 6298), üstel geri çekilme, azami deneme sonrası bağlantı
  hatası; hızlı yeniden iletim (3 yinelenen ACK); sıra dışı segmentleri sınırlı tamponda tutma (ya da düşürme —
  seçimini yaz); yinelenen segmentleri yok sayma.
- Tıkanıklık denetimi: en az yavaş başlangıç + tıkanıklık önleme (cwnd/ssthresh), kayıpta ssthresh yarıya.
- Kâhin `calistir_bm_x86_tcp_kayip`: Python eşi deterministik tohumla %5 kayıp, %5 sıra bozma, %2 çiftleme
  uygular; **1 MiB** veri iki yönde, SHA256 birebir; yeniden iletim sayısı > 0 ve pcap'te gözlenir; aktarım
  süresi ölçülür ve yazılır.
- Sabotajlar: RTO'yu sıfırla/çok büyük yap (ikisi ayrı) → biri aktarımı aşırı yavaşlatır (süre eşiği),
  diğeri ağı boğar (yeniden iletim sayısı eşiği), ikisi de KIRMIZI; yinelenen segmenti yok saymayıp veriye
  ekle → SHA256 KIRMIZI.

### Bölüm 8 — soket ABI'si (73+) ve çekirdek entegrasyonu
- Çağrılar (adları ve imzaları belgeye yaz; öneri): `soket(tur)`, `bagla(fd, ip, port)`, `baglan_dinle(fd,
  port, kuyruk)`, `kabul(fd)`, `gonder(fd, ptr, boy)`, `al(fd, ptr, boy, zaman_asimi_ms)`, `soket_kapat`
  (ya da mevcut `kapat` ile birleşik — seçimini yaz), `udp_gonder_hedef`, `udp_al_kaynak`, `dns_coz(ad, ptr)`,
  `ag_bilgi(ptr)`. Soket tanımlayıcıları süreç fd tablosunda (dalga 14/15'in fd ve boru altyapısı); bir
  soket boruya/yönlendirmeye bağlanabilir.
- Bloklama: okuyan veri yoksa uyur (dalga 14 süreç uykusu), zaman aşımı parametresiyle; süreç sonlanınca
  soketleri kapanır (RST ya da FIN — seçimini yaz); Ctrl+C ön plandaki ağ programını sonlandırır.
- `runtime/kullanici/x86_64/kem_sis.kem` sarmalayıcıları + `surec_abi.py` kapsamı.
- Kâhin `calistir_bm_x86_soket`: kullanıcı programı soketle eşe bağlanır, veri yollar/alır; aynı anda iki
  süreç iki ayrı bağlantı; bir süreç sonlandırılınca bağlantısı kapanır (eş FIN/RST görür); `ping` artık soket
  ABI'si (ham ICMP ya da özel çağrı — seçimini yaz) üzerinden.
- Sabotaj: süreç çıkışında soket kapatmayı atla → eş bağlantıyı açık görür, kâhin KIRMIZI.

### Bölüm 9 — ağ programları
- `ag`: arayüzler, MAC, IP/önek, geçit, DNS, kira süresi, sayaçlar, ARP tablosu (`ag arp`), bağlantılar
  (`ag baglantilar`).
- `cozumle <ad>`, `ping` (Bölüm 4'ten, soket ABI'sinde).
- `indir <ip|ad> <port> <yol> <hedef_dosya>`: HTTP/1.0 GET, yanıt kodu, `Content-Length` (yoksa bağlantı
  kapanışına kadar), FAT32'ye ya da `/ram`'e yazar; ilerleme gösterir; hata kodları.
- `sunucu <port> <dizin>`: tek iş parçacıklı küçük HTTP sunucusu, dizindeki dosyaları servis eder (yol
  temizleme: `..` reddedilir), dizin listesi, 404; Ctrl+C ile durur.
- `nc` (netcat benzeri): istemci ve dinleme kipi; standart girdi/çıktıyla borulara bağlanır
  (`kedi notlar.txt | nc 10.0.2.2 9000`, `nc -d 7000 > gelen.txt`).
- Kâhin `calistir_bm_x86_ag_program`: (a) host'ta Python HTTP sunucusu (slirp üzerinden 10.0.2.2) 2 MiB
  dosya → `indir` → FAT32 → host'ta imajdan okunur, SHA256 birebir; (b) konukta `sunucu`, host `hostfwd` ile
  `urllib` istemcisi dosya ister, SHA256 birebir; `..` içeren yol 403/404; (c) `nc` ile iki yönde boru
  aktarımı; (d) her oturumdan sonra `fsck.fat -n` temiz.
- Sabotajlar: `indir`'de son kısmi segmenti dosyaya yazma → SHA256 KIRMIZI; `sunucu`'da `..` denetimini
  kaldır → kâhin dizin dışı dosyanın sunulduğunu yakalar, KIRMIZI.

### Bölüm 10 — USB CDC-ECM (usb-net) — gerçek PC yedeği
- Desteklenmeyen dahili NIC'li makineler (Realtek yaygın) için USB Ethernet adaptörü yedeği: CDC-ECM sınıf
  sürücüsü (`drivers/usb/cdc_ecm.kem`, sans-IO): yapılandırma seçimi (QEMU `usb-net` birden fazla yapılandırma
  sunar — hangisini neden seçtiğini yaz), MAC (iMACAddress dizgi tanımlayıcısı), toplu IN/OUT ile çerçeveler,
  bağlantı bildirimi (kesme ucu). Hub arkasında da çalışır (dalga 16).
- Ağ yığını NIC'den bağımsız (aynı arayüz soyutlaması — D-407).
- Kâhin `calistir_bm_x86_usb_ag`: `-device usb-net` (kök portta ve hub arkasında) + slirp; DHCP + `ping
  10.0.2.2` + `indir` küçük dosya SHA256.
- Sabotaj: iMACAddress dizgisini bir hane kaydırarak çöz → MAC yanlış, slirp/eş yanıtlamaz, KIRMIZI.

### Bölüm 11 — bulanıklaştırma ve sağlamlık
- Host'ta (C ve self-host ikisiyle) sabit tohumlu ≥1000'er örnek: Ethernet, ARP, IPv4, ICMP, UDP, DHCP
  (seçenek uzunlukları, son işareti eksik), DNS (işaretçi döngüsü, aşırı uzun ad, kesik yanıt), TCP
  (seçenek uzunluğu 0/1, veri ofseti başlıktan büyük, pencere ölçeği). Mevcut `calistir_kem_os_ayristirici`
  takımına katılır. **Her korpus, korunması gereken şekli gerçekten üretmeli**: her korunma için bir sınır
  örneği ekle (D-767 S223 dersi).
- QEMU'da: Python eşi bozuk çerçeve akışı (1000 rastgele + mutasyon) gönderir → konuk çökmez, asılmaz, sayaçlar
  artar, ardından normal ping/TCP hâlâ çalışır.
- Bağlantı kopması (QMP `set_link off/on`) aktarım sırasında → TCP yeniden iletir ve tamamlar ya da zaman
  aşımıyla açık hata; DHCP kirası bitince yeniden alınır.
- Heap/çerçeve ölçümü: 10.000 ping + 200 TCP bağlantısı + 50 MiB aktarım sonrası çekirdek heap artışı ve
  boş çerçeve farkı (taze açılışlara bölünebilir — E4k); sonuçları tabloya yaz.
- Sabotajlar: TCP seçenek uzunluğu 0 korunmasını kaldır → bulanıklaştırıcı asılmayı yakalar, KIRMIZI —
  **gerçekten uygula**; IPv4 toplam boy > çerçeve boyu denetimini kaldır → çalışma zamanı sınır paniği,
  KIRMIZI.

### Bölüm 12 — iki KEMGU arası ağ + PC matrisi + fiziksel liste
- İki QEMU konuğu birbirine bağla (`-netdev dgram` karşılıklı portlarla ya da `-netdev stream` — seçimini yaz,
  sözdizimini doğrula): biri `sunucu`, diğeri `indir`; biri `nc -d`, diğeri `nc`. İki taraf da KEMGU.
  (Bu, Python eşinden bağımsız ikinci bir uygulamayla değil, KEMGU'nun kendisiyle birlikte çalışabilirliktir;
  Python eşi kâhin olmaya devam eder — bu senaryo ek kanıttır.)
- PC matrisine NIC ekseni: en az 6 yapılandırma {e1000, e1000e, virtio-net, usb-net} × açılış aygıtı;
  her birinde DHCP + ping + küçük `indir`.
- Fiziksel kontrol listesi: ağ adımları (dahili NIC satıcı:aygıt kimliği `donanim.txt`'de; destekleniyorsa
  DHCP + ping + `indir`; Realtek ise USB Ethernet adaptörüyle aynı adımlar — hangi adaptör sınıfının (CDC-ECM)
  çalışacağını yaz), sonucu `donanim.txt` + `ag` çıktısıyla getirme.
- Sabotaj: iki-konuk senaryosunda bir tarafın ARP yanıtını kapat → diğeri zaman aşımı mesajı verir ve
  asılmaz (pozitif sağlamlık kanıtı); kâhin beklenen hatayı görmezse KIRMIZI.

### Bölüm 13 — belgeler, yol haritası, sabotaj envanteri
- `belgeler/KEMGU_OS_Ag.md` tamamlanır: katman şeması, adresleme, zaman aşımı ve sınır tablosu, sayaçlar, hata
  kodları, desteklenen NIC listesi (satıcı:aygıt), kapsam dışı maddeler.
- `KEMGU_OS_Syscall_ABI.md` (73+), `KEMGU_OS_Program_Yazma.md` (soketli bir örnek; örnek kapıda derlenip
  çalışır — `calistir_bm_x86_belge15` deseni), `KEMGU_OS_Mimari.md` (ağ katmanı), kabukta `yardim` yeni
  komutlar.
- Yol haritası H13 maddeleri kanıt kapılarıyla işaretlenir; sabotaj envanterine bu dalganın satırları
  (uygulanmayan varsa AÇIKÇA).

---

## 7. Kapılar (her bölümden sonra)
O bölümün yeni kapıları + `calistir_bm_x86_v1`, `calistir_bm_x86_uefi_pc`, `calistir_bm_x86_saglamlik`,
`calistir_bm_x86_cok_surec`, `calistir_bm_x86_boru`, `calistir_bm_x86_program15`, `calistir_kem_os_x86`;
`kem_gorev_cekirdek.kem` değişirse `calistir_kem_os_arm` + `calistir_qemu_cekirdek`; sans-IO değişikliklerinde
`calistir_surucu_diff`, `calistir_surucu_kosum`, `calistir_chatgpt_kopru`, `calistir_kem_os_ayristirici`.
Bölüm 3, 10 ve 12'den sonra `calistir_kem_os_pc_matris`. On üçüncü bölümden sonra `make test_tumu` (rc
PIPESTATUS ile, atlamalar sayılmış ve sınıflandırılmış), `calistir_check_kapisi`, `calistir_baremetal_diff`.

**Windows CI notları (D-768'de üç kez ölçüldü):** CI'da QEMU yok, x86 kapıları "ATLANDI" der — ama
**derleme-yalnız** yolları (`UEFI_YALNIZ_DERLE=1`) Windows'ta da koşar. Oralarda: MSYS `/` önekli komut satırı
bayraklarını yol sanır (`lld-link -nodefaultlib` biçimini kullan); Python yerel Windows Python'udur, MSYS
yollarını (`/usr/bin/...`) açamaz — yeteneği Python'un kendisinden ölç; metin fikstürleri CRLF'e dönebilir
(`.gitattributes` + satır sonu toleransı). Yeni bir host aracı gerektiren kapı, araç yoksa **bildirerek** atlar.

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi ve kullanıcı ELF'lerini her koşumdan önce `rm -f`;
disk imajı her koşumda yeniden üretilir; `echo "$(cmd) $?"` yazma; `--checkdump` tanı bassa da 0 döner;
QEMU üçlü hatada sessizce yeniden başlar (`-no-reboot` + tek açılış işareti); kâhin anında gönderirse bekleme
kusurları görünmez; Türkçe kaynak okuyan her Python betiği `encoding="utf-8"` kullanır; sabotaj geri
alındıktan sonra ilgili `.o`/ikiliyi `rm -f` et; `make | tail` çıkış kodunu gizler (`${PIPESTATUS[0]}`);
test koşumu izlenen belgeleri DEĞİŞTİRMEMELİ (D-767: ölçüm kapısı `codex16_bulgular.md`'yi her koşumda
yeniden yazıyordu — raporları `build/` altına yaz); ağ kâhinlerinde UDP port çakışmasını önlemek için
portları koşum başına benzersiz seç (paralel koşumlar).

---

## 8. Bitirince
Draft PR `codex/ag-dalga17`, gövde `codex17_ilerleme.md` (on üç bölümün her biri için: yapılanlar, kapılar,
sabotaj sonuçları — uygulanmayan sabotaj varsa AÇIKÇA, süre). `codex17_bulgular.md`: ölçülen davranışlar ve
VARSAYIM'lar, NIC başına ölçümler, kayıplı TCP aktarım süreleri ve yeniden iletim sayıları, heap/çerçeve
tablosu, bulanıklaştırma istatistikleri, derleyici bulguları (C17-xxx), kalanlar. Auto-merge AÇMA, merge
ETME; PR açıldıktan sonra commit'leri yeniden yazma.

## 9. Bu dalganın DIŞINDA
- E4k (heap geri kazanımı), E4q (derleme süresi), H1b (ayrık derleme), H9j/E3f (süreç yalıtımı testleri): Claude.
- IPv6, TLS, Wi-Fi, Realtek dahili NIC, yönlendirme/NAT, multicast, pencere yöneticisi, ses, SMP, KEMFS,
  BIOS/GRUB, Secure Boot, fiziksel makine koşumları (Mehmet).

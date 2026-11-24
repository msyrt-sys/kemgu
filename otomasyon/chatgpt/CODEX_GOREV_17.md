# CODEX GÖREVİ 17 — ağ yığını (x86 + ARM): NIC sürücüleri, Ethernet/ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP, soket ABI'si, HTTP/NTP/TFTP, paket kurucu, uzak teşhis

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## ⏱ Bu dalga ŞİMDİYE KADARKİ EN UZUN dalgadır — çalışma biçimi
- Dalga 16 on üç bölümü bitirdi. Bu dalga **yirmi bölümdür, HEPSİ zorunlu ve sırayla yapılır.** Beklenen süre
  dalga 16'nın en az iki katıdır; **30+ saat normaldir.** Ağ bölümleri birbirinin üstüne kurulur (sürücü →
  L2/L3 → UDP → TCP → soket → uygulamalar → ARM), sırayı bozma.
- Bir bölümü bitirince **durma, rapor yazıp bekleme, özet sunup onay isteme — sonraki bölüme geç.** "Bu kadar
  yeterli" deme; yirmi bölümün hepsi bitene ya da her biri için takılma notu yazılana kadar çalış.
- Her bölümün sonunda: (1) o bölümün kapıları + sabotajları, (2) ayrı commit
  (`SURUCU(codex): Dalga 17 bölüm N — ...`), (3) `codex17_ilerleme.md`'ye kısa not (yapılan, ölçümler,
  sabotaj sonuçları, süre). Dalga yarıda kesilse bile biten bölümler kullanılabilir olmalı.
- Gerçekten takılırsan (60 dk ilerleme yok): `codex17_bulgular.md`'ye yaz, bölümün yapılabilen kısmını
  kapılı bırak ve **sonraki bölüme geç**.
- Her 5 bölümde bir (5, 10, 15) ara regresyon: §7'deki temel kapılar + o ana kadarki tüm ağ kapıları; kırmızı
  varsa bir sonraki bölüme geçmeden önce kökünü bul.
- **Her sabotajı gerçekten uygula, çıktısını yaz.** Uygulamadığın sabotaj varsa bunu AÇIKÇA yaz; "benzer bir
  sabotaj" ile değiştirme. Son iki dalgada istenen bulanıklaştırma sabotajı uygulanmadı ve Claude
  uyguladığında birinde **kapının kör olduğu** ortaya çıktı (D-767 S223: korunma kaldırılınca bulanıklaştırıcı
  yeşil kaldı, çünkü korpus o şekli hiç üretmiyordu). Sabotaj sessiz kalırsa önce korpusu/kapıyı düzelt.
- **Draft PR'ı açtıktan sonra son commit'i değiştirip zorla gönderme** (dalga 16'da ef40ab7 böyle geldi);
  ek değişiklik gerekiyorsa yeni bir commit ekle.
- Draft PR'ı yalnız yirmi bölüm bittikten (ya da her biri için takılma notu yazıldıktan) sonra aç.

---

## 0. Hedef

Donanım yol haritası büyük ölçüde kapandı (D-767: 111 maddenin 94'ü tamam; açık kalanlar ertelenmiş, fiziksel
ya da Claude'da). Kullanılabilir bir sistemin en büyük eksiği **ağ**. Bu dalga KEMGU-OS'a **iki mimaride**
(x86_64 ve AArch64) ortak bir ağ yığını ekler: yaygın NIC'ler için sürücüler, Ethernet/ARP/IPv4/ICMP/UDP/
DHCP/DNS/TCP, loopback, kullanıcı programlarına soket ABI'si ve çoklu bekleme, ağ programları (`ping`, `ag`,
`cozumle`, `nc`), HTTP istemcisi ve çoklu istemcili sunucu, NTP ile saat eşitleme, TFTP, ağ üzerinden paket
kurucu ve fiziksel PC denemeleri için **ağ üzerinden teşhis gönderimi**.

Her katman **sans-IO** (saf durum makinesi, `drivers/ag/`), host'ta iki derleyiciyle birim testli ve
bulanıklaştırılmış; QEMU'da **bağımsız kâhinle** (host'ta elle yazılmış Python Ethernet eşi + pcap kaydı)
doğrulanır. Aynı sans-IO modüller x86 ve ARM'de kullanılır (D-407).

Kapsam DIŞI: IPv6, TLS, yönlendirme (çoklu arayüz arası iletim), multicast/IGMP, Wi-Fi, Realtek RTL8111/8168
(QEMU'da öykünmesi yok — kâhinsiz sürücü yazma), CDC-NCM/RNDIS (QEMU'da yalnız ECM sınanabilir — doğrula).

---

## 1. Bugünkü durum (Claude ölçtü — varsayma, yine de kendin doğrula)

- Dalga 16 (D-767/D-768) çıktıları: klavye tamamlama, PCI ECAM + köprü + BAR, kesme kayıt arayüzü
  (`kesme_kaydet`/`kesme_ayir`/`kesme_birak`) + AHCI MSI + NVMe MSI-X, disk hata kurtarma (blkdebug), USB 2 hub,
  ramdisk + `/ram`, ACPI S5, PAT/WC, bulanıklaştırma korpusları, UART TX sınırı, MMIO volatile kapısı,
  `bilgi`/`duzenle`/`dosyalar` eklemeleri. Ölçümler `otomasyon/chatgpt/codex16_bulgular.md`'de.
- **ABI 72'ye kadar dolu** (71 `kopyala`, 72 `tasi`). Bu dalganın yeni çağrıları **73-119**.
- **Var olan ağ kodu — üç ayrı şey, karıştırma:**
  1. `runtime/kem_virtio_net.kem`: ARM KEMGU-OS'un (`.kem` çekirdek) virtio-mmio yolu; ARP + ICMP ping içerir
     (`test/ornekler/kem_os.kem`'de "[10] PING CANLI"). Bölüm 17'de yeni yığına bağlanacak.
  2. `test/bare_metal/*_arm.c` (tcp, udp, dns, dhcp, http_get, tftp, userspace_* …): **C ile yazılmış eski ARM
     prototipleri**, C virtio-net sürücüsüyle (`bm_a64_virtio_net.o`). `test_tumu`'ya BAĞLI DEĞİLLER
     (Claude ölçtü). Protokol ayrıntıları için **başvuru** olarak okunabilir; KEMGU yığını bunları çağırmaz,
     taşımaz, değiştirmez.
  3. `stdlib/ag.kem`: host programları içindir (BSD/Winsock soket sarmalı), çekirdekle ilgisi yok.
  x86 KEMGU-OS'ta ağ yok.
- SHA-256: `stdlib/kripto/karma.kem` (KEMGU) — paket kurucu (Bölüm 15) bütünlük denetiminde kullanılabilir;
  kullanıcı programına bağlanabildiğini doğrula (bağlanamıyorsa küçük bir kullanıcı-alanı kopyası yaz ve
  host'ta `hashlib` ile 1000 rastgele girdide birebir olduğunu kanıtla).
- RTC: `runtime/mimari/x86_64/kem_rtc_hw.kem` (okuma). Bölüm 13 yazma ekler.
- QEMU 8.2.2 bu makinede şunları sunuyor (Claude doğruladı, hepsi açılıyor):
  - NIC'ler: `e1000` (82540EM), `e1000e` (82574L), `igb` (82576), `rtl8139`, `virtio-net-pci`, `usb-net`;
    ARM `virt` makinesinde `virtio-net-device` (mmio).
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
make calistir_bm_x86_v1 calistir_bm_x86_uefi_pc calistir_bm_x86_saglamlik calistir_kem_os_x86 calistir_kem_os_arm   # taban
date > otomasyon/chatgpt/codex17_ilerleme.md
```
Tek dal, tek draft PR. ⚠ Claude bu dalga sırasında derleyicide (E4k) çalışabilir; **dalını dalga ortasında
main ile birleştirme** — Claude entegrasyonda birleştirir.

---

## 3. Okunacaklar
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md`, `KEMGU_OS_Mimari.md`, `KEMGU_OS_Syscall_ABI.md`,
  `KEMGU_OS_Program_Yazma.md`, `KEMGU_OS_Sabotaj_Envanteri.md`, `X86_Fiziksel_Donanim_Kontrol_Listesi.md`.
- `LOOP.md` D-763..D-769, `otomasyon/chatgpt/codex16_bulgular.md`.
- `runtime/kem_virtio_net.kem` (ARP/ICMP deseni), `runtime/mimari/x86_64/kem_pci_hw.kem`,
  `kem_gc_yurut.kem` (sans-IO yürütücü + PIT kalibreli bekleme), kesme kayıt arayüzü, `drivers/usb/*`
  (hub ve sınıf sürücüsü deseni), `drivers/blok` (aygıt arayüzü deseni), `stdlib/kripto/karma.kem`.
- `test/ornekler/kem_os.kem` (ARM çekirdeği), `test/ornekler/kem_os_x86_kabuk.kem`, `kem_os_x86_vfs_ortak.kem`,
  `kem_os_x86_blok_ortak.kem`, `runtime/kullanici/x86_64/*.kem`, `test/kullanici/*.kem`, `tests/qemu/*.py`,
  `test/bm_x86_*_harness.sh`.
- Başvuru (kopyalama, çağırma): `test/bare_metal/{tcp,tcp_close,dns,dhcp,http_get,userspace_tftp}_arm.c`.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
- `drivers/**` saf (sans-IO); **yeni `drivers/ag/`** serbest.
- `runtime/mimari/x86_64/*.kem`, `runtime/kullanici/x86_64/*.kem`, `linker/user-x86_64.ld`.
- **ARM (Bölüm 17):** `runtime/kem_virtio_net.kem`, `runtime/kem_*.kem` içinde ağla ilgili olanlar,
  `test/ornekler/kem_os.kem` — mevcut ARM fazları birebir yeşil kalır.
- `runtime/kem_gorev_cekirdek.kem` (sistem çağrısı tablosu; arm64 davranışı Bölüm 17 dışında değişmez).
- `boot/start_x86_64.S`: YALNIZ yeni kesme vektörü koçanı; `runtime/kdl_kesme.c`: YALNIZ
  `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- `test/ornekler/kem_os_x86_*.kem`, `test/kullanici/*.kem`, `test/bm_x86_*_harness.sh`, `tests/qemu/*`,
  `tests/drivers/**`, `tools/codex/*`.
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86/ARM CAT listeleri), `test/check_kapisi.sh`,
  `test/baremetal_diff_harness.sh` sınıflandırma satırları.
- `belgeler/KEMGU_OS_*.md` (yeni `KEMGU_OS_Ag.md` dahil), `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`,
  `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` (yeni H13 bölümü + işaretler).
- YASAK: `src/`, `selfhost/`, `boot/efi/`, diğer `linker/` dosyaları, runtime C dosyaları,
  `test/bare_metal/*.c`. Derleyici hatası → `codex17_bulgular.md`'ye C17-xxx + küçük tekrar üretimi (C ve
  self-host komutu, beklenen/gerçek sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri (önceki dalgalarınkiler geçerli, ek olarak)
1. **Katman başına sans-IO modül:** `drivers/ag/{eth,arp,ipv4,parca,icmp,udp,dhcp,dns,tcp,http,ntp,tftp}.kem`
   saf; girdi = bayt dizisi + zaman, çıktı = gönderilecek çerçeveler + olaylar. NIC sürücüleri
   (`drivers/ag/e1000.kem` vb.) `GcIstek`/`GcYanit` desenini kullanır. Yürütücü (`runtime/mimari/<mimari>/`)
   yalnız G/Ç ve kesme yapar. **Aynı modüller iki mimaride** (Bölüm 17).
2. **Ağ baytları büyük-endian'dır.** Ortak `drivers/ag/bayt.kem` (be16/be32 oku/yaz, internet sağlaması)
   bir kez yazılır, her katman onu kullanır (D-407). Sağlama hesabı RFC 1071; host kâhini aynı hesabı
   bağımsız olarak Python'da yapar.
3. **Arayüz soyutlaması:** her NIC (e1000, e1000e, igb, virtio-net, CDC-ECM, virtio-mmio, loopback) aynı
   `AgArayuz` yüzünü sunar (çerçeve gönder, çerçeve al, MAC, bağlantı, sayaçlar); yığın NIC'i bilmez.
4. **Sabit bellek:** halkalar, tamponlar, ARP önbelleği, parça birleştirme tamponları, soket ve TCP bağlantı
   tabloları sabit boyutlu, açılışta fiziksel çerçevelerden ayrılır; doluysa belgelenmiş hata kodu (sessiz
   atma yok; atılan paket sayaçta).
5. **Her bekleme sınırlı:** ARP çözümü, DHCP, DNS, NTP, TFTP, TCP bağlantısı ve yeniden iletim zaman aşımlı;
   zaman kaynağı dalga 15'in PIT kalibreli TSC'si (ARM'de genel zamanlayıcı). Hiçbir ağ olayı kabuğu asmaz.
6. **Sayaçlar:** arayüz başına alınan/gönderilen çerçeve, bayt, sağlama hatası, kısa çerçeve, bilinmeyen
   protokol, halka dolu, parça (birleştirilen/atılan), yeniden iletim sayaçları; `ag` komutu ve `donanim`
   raporu gösterir.
7. ABI geriye uyumlu büyür (5-72 değişmez; yeni çağrılar 73-119); `tests/qemu/surec_abi.py` tutarlılık kapısı
   her yeni çağrıyı kapsar.
8. **Bağımsız kâhin:** ağ kâhinleri konuğun kendi raporuna değil, host'taki Python eşinin gözlemine ve
   `filter-dump` pcap kaydının bağımsız ayrıştırmasına dayanır (sağlama toplamları, sıra numaraları, bayt
   içerikleri host'ta yeniden hesaplanır).

---

## 6. İş listesi — yirmi bölüm, HEPSİ zorunlu, sırayla

### Bölüm 1 — ağ yol haritası, belge iskeleti, host eşi
- Yol haritasına **H13 Ağ** bölümü ekle (bu dalganın her bölümü için madde + kanıt kapısı adı; IPv6/TLS
  "kapsam dışı" maddesi). H12b'yi kapat: USB sıfır uzunluk sabotajı D-767'de Claude tarafından S223 olarak
  uygulandı (`belgeler/KEMGU_OS_Sabotaj_Envanteri.md`), korpusa sınır örneği eklendi — kanıtı ekle.
- `belgeler/KEMGU_OS_Ag.md` iskeleti: katmanlar, adresleme, zaman aşımları, sınırlar, hata kodları (bölümler
  ilerledikçe doldurulur).
- **Host Ethernet eşi** `tests/qemu/ag_es.py`: `-netdev dgram` üzerinden konuğun çerçevelerini alır/gönderir;
  ARP yanıtı, ICMP echo (iki yön), UDP yankısı, DHCP sunucusu, DNS sunucusu, NTP sunucusu, TFTP sunucusu,
  TCP sunucu/istemci (bölümler ilerledikçe), deterministik **kayıp/sıra bozma/çiftleme/parçalama** enjeksiyonu
  (sabit tohum). Elle `struct` ile; `scapy` yok. Kendi birim testleri (bilinen pcap baytlarına karşı) ile gelir.
- pcap ayrıştırıcı `tests/qemu/pcap_oku.py`: her çerçeve için Ethernet/IPv4/UDP/TCP/ICMP başlıkları + sağlama
  toplamlarının bağımsız doğrulaması.
- Sabotaj: eşin IPv4 sağlama hesabında bir baytı atla → eşin kendi birim testi KIRMIZI (eşin de kâhin
  olarak güvenilir olduğunu gösterir).

### Bölüm 2 — e1000 (82540EM) sürücüsü
- `drivers/ag/e1000.kem` (sans-IO): aygıt sıfırlama (CTRL.RST, zaman aşımlı), MAC okuma (RAL0/RAH0; EEPROM
  gerekirse EERD), RX/TX tanımlayıcı halkaları (fiziksel çerçevelerde, hizalı), RCTL/TCTL/TIPG, bağlantı
  durumu (STATUS.LU), kesme (ICR/IMS) — dalga 16'daki kayıt arayüzüyle MSI ya da INTx/IOAPIC; yoklama yedeği.
- Kabuk: açılışta NIC bulunursa `ag` komutu MAC, bağlantı ve sayaçları gösterir; `donanim` raporunda NIC satırı.
- QMP `set_link` ile bağlantı kopar/geri gelir → konuk bunu algılar ve raporlar; kabuk asılmaz.
- Kâhin `calistir_bm_x86_e1000`: QMP'den (`query-rx-filter` ya da HMP `info network`) okunan MAC = konuğun
  raporu; konuk ham bir test çerçevesi yollar → Python eşi ve pcap birebir aynı baytları görür; eş bir çerçeve
  yollar → konuk sayacı ve içerik raporu birebir.
- Sabotajlar: TX tanımlayıcısında EOP bitini kurma → çerçeve gitmez, KIRMIZI; RX halka kuyruğunu (RDT)
  ilerletme → ikinci çerçeveden sonra alım durur, KIRMIZI.

### Bölüm 3 — e1000e (82574L), igb (82576), virtio-net-pci + NIC matrisi
- e1000e ve igb: e1000 ile ortak kısmı paylaş (D-407), farklı yazmaç/başlatma adımlarını ayrı tut (82574L ve
  82576'nın farklarını — gelişmiş tanımlayıcılar, kuyruk etkinleştirme bitleri, MSI-X — belgeye yaz).
- virtio-net-pci (x86 PCI aktarımı, modern ya da legacy — seçimini yaz): virtqueue'lar mevcut virtio kodundan
  yararlanabilir ama aktarım katmanı PCI'dır.
- Sürücü seçimi PCI satıcı:aygıt kimliğiyle; desteklenmeyen NIC `donanim` raporunda adıyla "desteklenmiyor"
  olarak listelenir (Realtek ve `rtl8139` dahil — `rtl8139`'u bilinçli desteklememe kararını belgeye yaz).
- Kâhin `calistir_bm_x86_nic_matris`: {e1000, e1000e, igb, virtio-net-pci} × {C, self-host} × ≥2 açılış;
  her biri Bölüm 2'nin çerçeve gidiş-dönüş testini geçer.
- Sabotajlar: e1000e'de RX halka taban adresinin üst 32 bitini yazma → (4 GiB üstü çerçevelerle çalışan
  yapılandırmada) alım bozulur → KIRMIZI; bunu tetikleyecek bir yapılandırma (yüksek RAM) kur. igb'de kuyruk
  etkinleştirme bitini kurma → alım yok, KIRMIZI.

### Bölüm 4 — Ethernet, ARP, IPv4, ICMP, loopback + `ping`
- ARP: istek/yanıt, önbellek (sabit boy, yaşlanma), çözüm bekleyen paket kuyruğu (sınırlı), yeniden deneme +
  zaman aşımı, gereksiz (gratuitous) ARP açılışta, aynı IP için çakışma algılama (sayaç + uyarı).
- IPv4: başlık doğrulama (sürüm, IHL, toplam boy, sağlama), TTL, seçenekler atlanır; varsayılan ağ geçidi
  üzerinden yönlendirme kararı (yerel alt ağ mı değil mi). Parçalar Bölüm 6'da.
- ICMP: echo yanıtı (konuğa gelen ping), echo isteği (konuktan giden ping), gelen hata mesajlarını
  (ulaşılamaz, TTL aşımı) ilgili sokete/sayaca bağlama.
- **Loopback** (`lo`, 127.0.0.1/8): ayrı `AgArayuz`; tek konuk içinde `nc`/`sunucu`/`indir` sınamaları için.
- Statik yapılandırma komutu (`ag ayarla <ip>/<önek> <geçit>`) — DHCP Bölüm 5'te.
- Kullanıcı programı `ping` (sayı, aralık, zaman aşımı, RTT istatistiği) — soket ABI'si (Bölüm 10) gelene kadar
  geçici bir sistem çağrısıyla; Bölüm 10'da soket ABI'sine taşınır.
- Kâhin `calistir_bm_x86_ag_l3`: (a) Python eşi konuğa 100 ping atar, 100 yanıt alır, sağlamalar pcap'te
  doğru; (b) konuk `ping` eşe 20 ping atar, eş 20 isteği görür; (c) slirp ile `ping 10.0.2.2` (slirp kendi ağ
  geçidinde ICMP yanıtlar — doğrula); (d) ARP önbelleği yaşlanınca yeniden istek gider (pcap'te sayılır);
  (e) `ping 127.0.0.1` dış arayüzden hiçbir çerçeve çıkarmadan yanıtlanır (pcap boş).
- Sabotajlar: IPv4 sağlamasında taşıma katlamasını (carry fold) atla → eş paketleri reddeder, KIRMIZI;
  ARP yanıtında gönderen MAC'i hedef alanına yaz → eşin ARP tablosu yanlış dolar, KIRMIZI; loopback paketini
  dış arayüze yolla → (e) KIRMIZI.

### Bölüm 5 — UDP, DHCP istemcisi, DNS istemcisi
- UDP: bağlantı noktası tablosu (sabit), sağlama (sözde başlıkla), kapalı porta gelen paket için ICMP
  ulaşılamaz (oran sınırlı + sayaç).
- DHCP istemcisi: DISCOVER/OFFER/REQUEST/ACK, kira süresi, yenileme (T1) ve yeniden bağlama (T2)
  zamanlayıcıları, seçenekler (alt ağ, geçit, DNS, kira, NTP sunucusu — seçenek 42), NAK ve zaman aşımında
  yeniden deneme; açılışta otomatik, `ag dhcp` ile elle; kira bitince adres bırakılır.
- DNS istemcisi: A sorgusu, yanıt ayrıştırma (sıkıştırma işaretçileri — **işaretçi döngüsüne karşı sınır**),
  CNAME zinciri (sınırlı), önbellek (TTL), birden çok DNS sunucusu (ilki yanıtlamazsa ikincisi), zaman aşımı +
  yeniden deneme; `cozumle <ad>` komutu.
- Kâhin `calistir_bm_x86_ag_udp`: (a) Python eşi DHCP sunucusu: konuk adres alır, T1'de yenilemeyi, T2'de
  yeniden bağlamayı zamanında yapar (kısa kira ile), pcap'te seçenekler doğru; (b) slirp DHCP ile 10.0.2.15;
  (c) eşin DNS sunucusu sıkıştırmalı + CNAME'li yanıtlar verir, konuk doğru IP'yi bulur; ilk DNS sunucusu
  sessizken ikincisine geçer; (d) UDP yankı: 1000 datagram, içerikler birebir.
- Sabotajlar: DNS işaretçi döngüsü sınırını kaldır → bulanıklaştırıcı/eş döngülü yanıtla asılmayı yakalar
  (zaman aşımı), KIRMIZI — **gerçekten uygula**; DHCP yenilemesini T1 yerine kira sonunda yap → kâhin
  zamanlamayı yakalar, KIRMIZI.

### Bölüm 6 — IPv4 parça birleştirme + giden parçalama + ICMP hata üretimi
- Birleştirme: sabit sayıda birleştirme yuvası (kaynak, hedef, kimlik, protokol), toplam boy sınırı, örtüşen
  parçalar (ilk geleni koru — seçimini yaz), zaman aşımı (yuva boşalır + sayaç), eksik parça.
- Giden parçalama: MTU'yu aşan UDP datagramları için (DF bitiyle gelen TCP'ye uygulanmaz; TCP MSS kullanır).
- ICMP hata üretimi: kapalı UDP portu (ulaşılamaz), TTL'si biten (yönlendirme yoksa yalnız gelen TTL=0 için
  sayaç — seçimini yaz); oran sınırlaması.
- Kâhin `calistir_bm_x86_ag_parca`: Python eşi 8 KiB UDP datagramını sıra bozulmuş, çiftlenmiş ve bir
  parçası eksik varyantlarla yollar → konuk tamamını birebir alır, eksik olanı zaman aşımında atar (sayaç);
  konuk 8 KiB UDP yollar → eş parçaları birleştirip SHA256 doğrular; pcap'te parça ofsetleri doğru.
- Sabotajlar: parça ofsetini 8 ile çarpmayı unut → birleştirme bozulur, KIRMIZI; birleştirme yuvası zaman
  aşımını kaldır → eksik parçalarla yuvalar tükenir, sonraki datagramlar kaybolur, KIRMIZI.

### Bölüm 7 — TCP (temel)
- Durum makinesi (RFC 793 + 9293 sadeleştirilmiş): LISTEN, SYN_SENT, SYN_RECEIVED, ESTABLISHED, FIN_WAIT_1/2,
  CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT (sınırlı süre), RST işleme.
- İstemci (aktif açma) ve sunucu (pasif açma, bekleme kuyruğu sınırlı), MSS seçeneği, ISN (zaman + sayaç),
  sıra numarası aritmetiği **32 bit sarmalı** (karşılaştırmalar sarmaya dayanıklı).
- Kayan pencere: alıcı penceresi (sabit tampon), göndericide pencereye uyma, sıfır pencere yoklaması.
- Bağlantı tablosu sabit boyutlu; dolu → yeni SYN'e RST + sayaç.
- Kâhin `calistir_bm_x86_tcp_temel`: Python eşi sunucu ve istemci; el sıkışma, 64 KiB veri iki yönde (SHA256
  host'ta), düzgün kapanış (FIN iki yön), RST ile kapanış, kapalı porta SYN → RST; loopback üzerinden konuk içi
  bağlantı.
- Sabotajlar: sıra numarası karşılaştırmasını sarmaya dayanıksız yap (düz `<`) ve ISN'yi 2^32'ye yakın seç →
  aktarım bozulur, KIRMIZI; FIN'in tükettiği sıra numarasını sayma → kapanış asılır, zaman aşımıyla KIRMIZI.

### Bölüm 8 — TCP (güvenilirlik)
- Yeniden iletim zamanlayıcısı (RTO: SRTT/RTTVAR, RFC 6298), üstel geri çekilme, azami deneme sonrası bağlantı
  hatası; hızlı yeniden iletim (3 yinelenen ACK); sıra dışı segmentleri sınırlı tamponda tutma (ya da düşürme —
  seçimini yaz); yinelenen segmentleri yok sayma.
- Tıkanıklık denetimi: yavaş başlangıç + tıkanıklık önleme (cwnd/ssthresh), kayıpta ssthresh yarıya, hızlı
  kurtarma.
- Kâhin `calistir_bm_x86_tcp_kayip`: Python eşi deterministik tohumla %5 kayıp, %5 sıra bozma, %2 çiftleme
  uygular; **4 MiB** veri iki yönde, SHA256 birebir; yeniden iletim sayısı > 0 ve pcap'te gözlenir; aktarım
  süresi ölçülür ve yazılır.
- Sabotajlar: RTO'yu sıfırla/çok büyük yap (ikisi ayrı) → biri aktarımı aşırı yavaşlatır (süre eşiği),
  diğeri ağı boğar (yeniden iletim sayısı eşiği), ikisi de KIRMIZI; yinelenen segmenti yok saymayıp veriye
  ekle → SHA256 KIRMIZI.

### Bölüm 9 — TCP uzantıları ve verim
- Pencere ölçeği (RFC 7323 §2) ve zaman damgası seçeneği (RTT ölçümü + PAWS — PAWS'ı yapmazsan yaz);
  gecikmeli ACK (sınırlı süre), Nagle algoritması (soket seçeneğiyle kapatılabilir — Bölüm 10), keepalive
  (soket seçeneği; karşı taraf kaybolunca bağlantı hatası).
- Verim ölçümü: Python eşiyle kayıpsız 64 MiB tek yön aktarım; MB/s ve CPU zamanı; pencere ölçeği açık ve
  kapalı karşılaştırması; eşik kapısı (ölçülen değerin altına düşmeyi yakalayan, gerekçeli eşik — paralel
  koşum yükünü hesaba kat).
- Kâhin `calistir_bm_x86_tcp_verim` + `calistir_bm_x86_tcp_secenek` (seçenek baytları pcap'te doğru; keepalive
  karşı taraf susunca bağlantıyı belirli sürede kapatır).
- Sabotajlar: pencere ölçeği kaydırmasını göndericide uygula ama alıcıda uygulama → aktarım durur/çok yavaşlar,
  KIRMIZI; gecikmeli ACK zamanlayıcısını kaldır (yalnız ikinci segmentte ACK) → tek segmentlik istek/yanıt
  asılır, KIRMIZI.

### Bölüm 10 — soket ABI'si (73+), bloksuz soketler, çoklu bekleme
- Çağrılar (adları ve imzaları belgeye yaz; öneri): `soket(tur)`, `bagla(fd, ip, port)`, `baglan_dinle(fd,
  port, kuyruk)`, `kabul(fd)`, `gonder(fd, ptr, boy)`, `al(fd, ptr, boy, zaman_asimi_ms)`, `soket_kapat`
  (ya da mevcut `kapat` ile birleşik — seçimini yaz), `udp_gonder_hedef`, `udp_al_kaynak`, `dns_coz(ad, ptr)`,
  `ag_bilgi(ptr)`, `soket_secenek(fd, ad, deger)` (bloksuz, Nagle, keepalive, alım zaman aşımı).
- **Çoklu bekleme** `bekle_coklu(fd_listesi_ptr, adet, zaman_asimi_ms)` (poll benzeri): soket, boru, klavye
  (standart girdi) fd'lerini birlikte bekler; okunabilir/yazılabilir/hata bitleri.
- Soket tanımlayıcıları süreç fd tablosunda (dalga 14/15'in fd ve boru altyapısı); bir soket boruya/
  yönlendirmeye bağlanabilir. Bloklama: okuyan veri yoksa uyur (dalga 14 süreç uykusu); bloksuz kipte hemen
  belgelenmiş "bekle" kodu; süreç sonlanınca soketleri kapanır (RST ya da FIN — seçimini yaz); Ctrl+C ön
  plandaki ağ programını sonlandırır.
- `runtime/kullanici/x86_64/kem_sis.kem` sarmalayıcıları + `surec_abi.py` kapsamı.
- Kâhin `calistir_bm_x86_soket`: kullanıcı programı soketle eşe bağlanır, veri yollar/alır; aynı anda üç süreç
  üç ayrı bağlantı; bir süreç sonlandırılınca bağlantısı kapanır (eş FIN/RST görür); `bekle_coklu` ile tek süreç
  hem klavyeyi hem iki soketi izler (QMP tuş + eş verisi karışık sırayla); `ping` artık soket ABI'si (ham ICMP
  ya da özel çağrı — seçimini yaz) üzerinden.
- Sabotajlar: süreç çıkışında soket kapatmayı atla → eş bağlantıyı açık görür, KIRMIZI; `bekle_coklu`'da
  okunabilir bitini boru fd'leri için kurma → karışık senaryo asılır, KIRMIZI.

### Bölüm 11 — kullanıcı ağ kütüphanesi + `ag`, `cozumle`, `nc`
- `runtime/kullanici/x86_64/kem_ag.kem`: üst düzey sarmalayıcılar (`ad_ile_bagla(ad, port)`, satır okuma
  tamponu, tamamını yaz, zaman aşımlı okuma, IP biçimleme/ayrıştırma) — programlar ham çağrıları değil bunu
  kullanır.
- `ag`: arayüzler, MAC, IP/önek, geçit, DNS, NTP, kira süresi, sayaçlar, ARP tablosu (`ag arp`), bağlantılar
  (`ag baglantilar`: yerel/uzak uç, durum, kuyruk boyları).
- `cozumle <ad>`, `ping` (soket ABI'sinde).
- `nc` (netcat benzeri): istemci ve dinleme kipi (`-d`), UDP kipi (`-u`), zaman aşımı (`-z`); standart
  girdi/çıktıyla borulara bağlanır (`kedi notlar.txt | nc 10.0.2.2 9000`, `nc -d 7000 > gelen.txt`); iki yönlü
  etkileşim `bekle_coklu` ile.
- Kâhin `calistir_bm_x86_ag_arac`: `ag` çıktısının alanları QMP/eş gözlemiyle birebir; `nc` iki yönde boru
  aktarımı (1 MiB, SHA256); `nc -u` yankı; loopback üzerinde `nc -d` + `nc` aynı konukta.
- Sabotaj: `nc`'de standart girdi dosya sonunda yazma yarısını kapatma (FIN) gönderme → karşı taraf veri sonunu
  göremez, aktarım asılır, KIRMIZI.

### Bölüm 12 — HTTP istemcisi (`indir`) ve çoklu istemcili sunucu (`sunucu`)
- `drivers/ag/http.kem` (sans-IO ayrıştırıcı/kurucu): istek satırı, başlıklar (sınırlı boy/sayı), durum
  kodları, `Content-Length`, **chunked** aktarım kodlaması, `Connection: keep-alive`/`close`.
- `indir <url> <hedef_dosya>`: `http://ad[:port]/yol` ayrıştırma, DNS, HTTP/1.1 GET, chunked, yönlendirme
  (301/302/307, en çok 5), FAT32'ye ya da `/ram`'e yazar; ilerleme gösterir; hata kodları; aynı sunucuya
  keep-alive ile ardışık iki indirme.
- `sunucu <port> <dizin>`: **çoklu istemci** (`bekle_coklu` ile en az 8 eşzamanlı bağlantı), dizindeki
  dosyaları servis eder (yol temizleme: `..`, mutlak yol ve yüzde kodlu `..` reddedilir), dizin listesi, 404,
  keep-alive, erişim günlüğü (`log`'a); Ctrl+C ile durur.
- Kâhin `calistir_bm_x86_http`: (a) host'ta Python HTTP sunucusu (slirp üzerinden 10.0.2.2) 8 MiB dosya,
  chunked yanıt ve yönlendirme zinciri → `indir` → FAT32 → host'ta imajdan okunur, SHA256 birebir; (b) konukta
  `sunucu`, host `hostfwd` ile 8 eşzamanlı `urllib` istemcisi farklı dosyalar ister, hepsi SHA256 birebir;
  `..`/`%2e%2e` içeren yollar 403/404; (c) her oturumdan sonra `fsck.fat -n` temiz.
- Sabotajlar: chunked ayrıştırıcıda son (0 uzunluklu) parçayı tanıma → `indir` asılır, KIRMIZI; `sunucu`'da
  yüzde kodlu `..` denetimini kaldır → kâhin dizin dışı dosyanın sunulduğunu yakalar, KIRMIZI; yönlendirme
  sınırını kaldır → döngülü yönlendirmede asılır, KIRMIZI.

### Bölüm 13 — NTP ile saat eşitleme + RTC yazma
- `drivers/ag/ntp.kem`: SNTP istemcisi (RFC 4330), sunucu DHCP seçenek 42'den ya da `ag ntp <ip>` ile; ofset
  ve gecikme hesabı; RTC'ye (CMOS, `kem_rtc_hw.kem`'e yazma ekle — BCD/ikili ve 12/24 saat bitlerine uy)
  yazma; açılışta otomatik eşitleme seçeneği; `saat` programı eşitlenmiş saati gösterir.
- Kâhin `calistir_bm_x86_ntp`: Python eşi bilinen bir zaman (ör. 2031-07-15 12:34:56 UTC) döner → konuk RTC'ye
  yazar → QEMU'nun RTC'si (QMP `qom-get` path `/machine` property `rtc-time` — Claude doğruladı, `tm_year`
  1900 tabanlı döner) beklenen
  zamanı ±2 s gösterir; ofset hesabı (eş yapay gecikme eklediğinde) beklenen aralıkta.
- Sabotajlar: NTP zaman damgasının 1900→1970 dönüşümünü (2208988800 s) atla → RTC yanlış yıl, KIRMIZI;
  RTC'ye BCD kipinde ikili yaz → saat yanlış, KIRMIZI.

### Bölüm 14 — TFTP + uzak teşhis gönderimi
- `drivers/ag/tftp.kem` (RFC 1350; isteğe bağlı RFC 2347/2348 blksize): istemci `tftp al <ip> <uzak> <yerel>`,
  `tftp gonder <ip> <yerel> <uzak>`; zaman aşımı + yeniden iletim, yinelenen blok yok sayma, hata paketleri.
- **Uzak teşhis** (fiziksel PC denemeleri için): `donanim gonder <ip> <port>` ve `log gonder <ip> <port>` —
  `donanim.txt` ve çekirdek günlüğünü TCP (ya da TFTP) ile host'a yollar; host tarafı için
  `tools/codex/teshis_al.py` (dinler, dosyaya yazar, SHA256 basar). Ağ yoksa açık hata.
- Kâhin `calistir_bm_x86_tftp_teshis`: Python eşi TFTP sunucusu 2 MiB dosya (blokların %5'i kayıp) → konuk
  FAT32'ye yazar, SHA256 birebir; konuk `tftp gonder` → eş SHA256 birebir; `donanim gonder` → `teshis_al.py`
  dosyası konuğun FAT32'deki `donanim.txt`'si ile birebir.
- Sabotajlar: TFTP'de yinelenen bloğu dosyaya ikinci kez yaz → SHA256 KIRMIZI; son bloğu (512'den kısa)
  aktarım sonu sayma → asılır, KIRMIZI.

### Bölüm 15 — ağ üzerinden paket kurucu (`paket`)
- Depo biçimi (belgeye yaz): bir HTTP dizininde `paketler.txt` bildirimi (ad, sürüm, dosya, boy, SHA256) +
  ELF dosyaları. Depo, `make kem_os_paket_deposu` ile mevcut kullanıcı programlarından üretilir.
- `paket guncelle <url>` (bildirimi indirir), `paket liste`, `paket kur <ad>` (indirir, **SHA256 doğrular**,
  `/diskler/<veri>/bin`'e kurar; doğrulama başarısızsa dosyayı silip açık hata), `paket kaldir <ad>`,
  `paket bilgi <ad>`. Kurulu paketler `/diskler/<veri>/paket/kurulu.txt`'de; kabukta program araması kurulan
  programı bulur.
- SHA256: `stdlib/kripto/karma.kem` kullanıcı programına bağlanabiliyorsa onu kullan; değilse kullanıcı-alanı
  kopyası + `hashlib` ile 1000 rastgele girdide birebir kanıtı (§1).
- Kâhin `calistir_bm_x86_paket`: host Python HTTP sunucusu depoyu servis eder → konuk `paket guncelle` +
  `paket kur hesapla` → kurulan program çalışır ve doğru çıktı verir; host imajda dosya SHA256 = bildirim;
  bozuk bir paket (bir baytı değiştirilmiş) kurulmaz, diskte kalmaz; `paket kaldir` sonrası program yok;
  `fsck.fat -n` temiz.
- Sabotajlar: SHA256 karşılaştırmasını atla → bozuk paket kurulur, KIRMIZI; kurulum başarısızken geçici dosyayı
  silme → diskte artık dosya kalır, KIRMIZI.

### Bölüm 16 — USB CDC-ECM (usb-net) — gerçek PC yedeği
- Desteklenmeyen dahili NIC'li makineler (Realtek yaygın) için USB Ethernet adaptörü yedeği: CDC-ECM sınıf
  sürücüsü (`drivers/usb/cdc_ecm.kem`, sans-IO): yapılandırma seçimi (QEMU `usb-net` birden fazla yapılandırma
  sunar — hangisini neden seçtiğini yaz), MAC (iMACAddress dizgi tanımlayıcısı), toplu IN/OUT ile çerçeveler,
  bağlantı bildirimi (kesme ucu). Hub arkasında da çalışır (dalga 16). Aynı `AgArayuz` yüzü.
- Kâhin `calistir_bm_x86_usb_ag`: `-device usb-net` (kök portta ve hub arkasında) + slirp ve + Python eşi;
  DHCP + `ping` + `indir` 2 MiB SHA256 + `paket kur`; sıcak çıkarma/takma (QMP `device_del`/`device_add`)
  sonrası arayüz düşer ve geri gelir, kabuk asılmaz.
- Sabotaj: iMACAddress dizgisini bir hane kaydırarak çöz → MAC yanlış, slirp/eş yanıtlamaz, KIRMIZI.

### Bölüm 17 — ARM KEMGU-OS'ta aynı yığın
- `runtime/kem_virtio_net.kem`'in ad hoc ARP/ICMP'sini yeni sans-IO yığına bağla: virtio-mmio NIC'i
  `AgArayuz` olarak sunan ince bir köprü; ARM zaman kaynağıyla zamanlayıcılar; **aynı `drivers/ag/*`
  modülleri** (kopya YOK — D-407).
- ARM KEMGU-OS'a (`test/ornekler/kem_os.kem`) yeni fazlar: DHCP (slirp), `ping 10.0.2.2`, TCP ile Python eşine
  1 MiB SHA256, HTTP `indir` (sistem çağrısı katmanı ARM'de farklıysa çekirdek içi bir sınama fazı olarak —
  seçimini yaz). Mevcut 24 faz birebir yeşil kalır; "[10] PING CANLI" yeni yığından geçer.
- Kâhin `calistir_kem_os_arm_ag` (ayrı hedef; `calistir_kem_os_arm` ve `calistir_qemu_cekirdek` de yeşil).
- Sabotaj: ARM köprüsünde virtio RX tamponunu yeniden kuyruğa koymayı atla → birkaç çerçeveden sonra alım durur,
  TCP fazı KIRMIZI.

### Bölüm 18 — bulanıklaştırma ve sağlamlık
- Host'ta (C ve self-host ikisiyle) sabit tohumlu ≥1000'er örnek: Ethernet, ARP, IPv4, parça birleştirme
  (örtüşme, aşırı boy, sıfır boy parça), ICMP, UDP, DHCP (seçenek uzunlukları, son işareti eksik), DNS (işaretçi
  döngüsü, aşırı uzun ad, kesik yanıt), TCP (seçenek uzunluğu 0/1, veri ofseti başlıktan büyük, pencere ölçeği
  > 14), HTTP (başlık boyu, chunked boy taşması, eksik CRLF), NTP, TFTP, paket bildirimi. Mevcut
  `calistir_kem_os_ayristirici` takımına katılır. **Her korpus, korunması gereken şekli gerçekten üretmeli**:
  her korunma için bir sınır örneği ekle (D-767 S223 dersi).
- QEMU'da: Python eşi bozuk çerçeve akışı (10.000 rastgele + mutasyon, tüm katmanlar) gönderir → konuk çökmez,
  asılmaz, sayaçlar artar, ardından normal ping/TCP/HTTP hâlâ çalışır.
- Bağlantı kopması (QMP `set_link off/on`) aktarım sırasında → TCP yeniden iletir ve tamamlar ya da zaman
  aşımıyla açık hata; DHCP kirası bitince yeniden alınır; NTP/DNS sunucusu kaybolunca zaman aşımı mesajı.
- Kaynak tükenmesi: soket tablosu, TCP bağlantı tablosu, ARP önbelleği, parça yuvaları dolunca belgelenmiş
  hata; boşalınca yeniden kullanılır (100 tur doldur/boşalt).
- Heap/çerçeve ölçümü: 10.000 ping + 500 TCP bağlantısı + 200 MiB aktarım + 100 HTTP isteği sonrası çekirdek
  heap artışı ve boş çerçeve farkı (taze açılışlara bölünebilir — E4k); sonuçları tabloya yaz.
- Sabotajlar: TCP seçenek uzunluğu 0 korunmasını kaldır → bulanıklaştırıcı asılmayı yakalar, KIRMIZI —
  **gerçekten uygula**; IPv4 toplam boy > çerçeve boyu denetimini kaldır → çalışma zamanı sınır paniği,
  KIRMIZI — **gerçekten uygula**; HTTP chunked boy taşması denetimini kaldır → KIRMIZI — **gerçekten uygula**.

### Bölüm 19 — iki KEMGU arası ağ, mimariler arası ağ, PC matrisi, fiziksel liste
- İki x86 konuğu birbirine bağla (`-netdev dgram` karşılıklı portlarla ya da `-netdev stream` — seçimini yaz,
  sözdizimini doğrula): biri `sunucu`, diğeri `indir`; biri `nc -d`, diğeri `nc`; biri TFTP gönderir, diğeri
  `paket` deposu olur (konuk `sunucu` depo servis eder).
- **x86 ↔ ARM:** bir x86 ve bir ARM KEMGU konuğu aynı sanal ağda: TCP ile 1 MiB iki yönde, SHA256 birebir
  (iki mimarinin aynı sans-IO yığınının birlikte çalışabilirliği).
- PC matrisine NIC ekseni: en az 8 yapılandırma {e1000, e1000e, igb, virtio-net, usb-net (kök), usb-net (hub
  arkası)} × açılış aygıtı × RAM; her birinde DHCP + ping + küçük `indir` + `paket kur`.
- Fiziksel kontrol listesi: ağ adımları (dahili NIC satıcı:aygıt kimliği `donanim.txt`'de; destekleniyorsa
  DHCP + ping + `indir`; Realtek ise USB Ethernet adaptörüyle aynı adımlar — CDC-ECM destekli adaptör sınıfını
  yaz), **`donanim gonder` ile sonucu ağdan getirme** (USB belleği sökmeden), NTP ile saatin eşitlendiğini
  görme.
- Sabotaj: iki-konuk senaryosunda bir tarafın ARP yanıtını kapat → diğeri zaman aşımı mesajı verir ve
  asılmaz (pozitif sağlamlık kanıtı); kâhin beklenen hatayı görmezse KIRMIZI.

### Bölüm 20 — belgeler, yol haritası, sabotaj envanteri
- `belgeler/KEMGU_OS_Ag.md` tamamlanır: katman şeması, `AgArayuz`, adresleme, zaman aşımı ve sınır tablosu,
  sayaçlar, hata kodları, desteklenen NIC listesi (satıcı:aygıt), paket deposu biçimi, NTP/TFTP/HTTP
  ayrıntıları, kapsam dışı maddeler.
- `KEMGU_OS_Syscall_ABI.md` (73+), `KEMGU_OS_Program_Yazma.md` (soketli ve `bekle_coklu`'lu iki örnek; örnekler
  kapıda derlenip çalışır — `calistir_bm_x86_belge15` deseni), `KEMGU_OS_Mimari.md` (ağ katmanı, iki mimari),
  kabukta `yardim` yeni komutlar.
- Yol haritası H13 maddeleri kanıt kapılarıyla işaretlenir; sabotaj envanterine bu dalganın satırları
  (uygulanmayan varsa AÇIKÇA).

---

## 7. Kapılar (her bölümden sonra)
O bölümün yeni kapıları + `calistir_bm_x86_v1`, `calistir_bm_x86_uefi_pc`, `calistir_bm_x86_saglamlik`,
`calistir_bm_x86_cok_surec`, `calistir_bm_x86_boru`, `calistir_bm_x86_program15`, `calistir_kem_os_x86`;
`kem_gorev_cekirdek.kem` ya da ARM dosyaları değişirse `calistir_kem_os_arm` + `calistir_qemu_cekirdek`;
sans-IO değişikliklerinde `calistir_surucu_diff`, `calistir_surucu_kosum`, `calistir_chatgpt_kopru`,
`calistir_kem_os_ayristirici`. Bölüm 3, 16 ve 19'dan sonra `calistir_kem_os_pc_matris`. Yirminci bölümden sonra
`make test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış ve sınıflandırılmış), `calistir_check_kapisi`,
`calistir_baremetal_diff`.

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
yeniden yazıyordu — raporları `build/` altına yaz); ağ kâhinlerinde UDP/TCP port çakışmasını önlemek için
portları koşum başına benzersiz seç (paralel koşumlar); verim ve zaman eşiklerini paralel QEMU yükünde de
geçecek, ama gerilemeyi yakalayacak biçimde ölçerek seç.

---

## 8. Bitirince
Draft PR `codex/ag-dalga17`, gövde `codex17_ilerleme.md` (yirmi bölümün her biri için: yapılanlar, kapılar,
sabotaj sonuçları — uygulanmayan sabotaj varsa AÇIKÇA, süre). `codex17_bulgular.md`: ölçülen davranışlar ve
VARSAYIM'lar, NIC başına ölçümler, kayıplı TCP aktarım süreleri ve yeniden iletim sayıları, verim tablosu,
heap/çerçeve tablosu, bulanıklaştırma istatistikleri, ARM ölçümleri, derleyici bulguları (C17-xxx), kalanlar.
Auto-merge AÇMA, merge ETME; PR açıldıktan sonra commit'leri yeniden yazma.

## 9. Bu dalganın DIŞINDA
- E4k (heap geri kazanımı), E4q (derleme süresi), H1b (ayrık derleme), H9j/E3f (süreç yalıtımı testleri): Claude.
- IPv6, TLS, Wi-Fi, Realtek dahili NIC, CDC-NCM/RNDIS, yönlendirme/NAT, multicast, pencere yöneticisi, ses, SMP,
  KEMFS, BIOS/GRUB, Secure Boot, fiziksel makine koşumları (Mehmet).

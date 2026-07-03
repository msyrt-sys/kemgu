# DECISIONS_LOG — Codegen Kampanyası Karar Kaydı

Format: D-NNN | tarih | karar | gerekçe | kapsam/sınırlar. [YÜKSEK] = merge-review'da
özellikle bakılması istenen, izole commit'li kararlar.

---

## D-194 — OS: SELF-HOST 128-bit bignum toplama — KEMGU carry propagation (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-193).

**Karar [ETKİ: yeni `test/ornekler/bignum_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Çok-word aritmetiği (carry propagation) saf KEMGU'da: 128-bit = 2×dtam64 (yuksek,dusuk).
`dusuk_top = a_dusuk+b_dusuk` (mod-2^64 wrap), **carry = (dusuk_top < a_dusuk)?1:0** (unsigned overflow), `yuksek
= a_yuksek+b_yuksek+carry`. **Dil-doğrulaması:** dtam64 `<` → **`icmp ult`** (unsigned, slt DEĞİL) + `add i64`
wrap. 3 vektör (V1 carry, V2 çift-wrap, V3) bilinen sonuçla eşleşti → "KEM BIGNUM OK". **LEXER BULGU (spawn_task
task_6184e549 ile flag'lendi):** integer literal SIGNED (strtoll) parse → yüksek-bitli 64-bit hex sabitler
(0xFFFFFFFFFFFFFFFF) INT64_MAX'a KIRPILIR → aritmetikle (INT64_MAX+INT64_MAX+1) kuruldu. `yazdir_isaretsiz_tam64`
(i64). **Not:** Paralel mini-agent üretti; lexer-bulgu spawn_task ile flag'lendi; cherry-pick ile entegre.

## D-193 — OS: userspace TFTP GET — EL0 syscall ile dosya transferi (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-192).

**Karar [ETKİ: yeni `test/bare_metal/userspace_tftp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
EL0 süreç ağdan DOSYA çeker — D-176 net-syscall (24/25) üstünde TFTP. SLIRP dahili TFTP sunucusu (`-netdev
user,tftp=DIR`) 10.0.2.2:69. EL0: RRQ (opcode 1, "dosya.txt\0octet\0") sys2(24) → DATA (opcode 3) sys2(25) →
içerik çıkar. **Kanıt:** "KEMGU-TFTP-DATA" (15 byte) çekildi → "USERTFTP OK" (gerçek RX). DETERMİNİSTİK. **BULGU:**
SLIRP, DATA'yı bize yollamadan ÖNCE ARP ile MAC'imizi sorar (çift-yönlü) → EL0 poll'a ARP-reply eklendi (SLIRP'e
MAC öğret). ACK (opcode 4) da gönderilir. EL0 .rodata-deref-etmez (D-177). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-192 — OS: SMP ticket-lock — adil FIFO kilit (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-191).

**Karar [ETKİ: yeni `test/bare_metal/smp_ticket_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-170 spinlock ADİL DEĞİL (açlık mümkün); ticket-lock FIFO adalet: `bilet_al` (LDXR/STXR atomik fetch-add) +
`kilitle` (simdi_hizmet==bilet bekle) + `ac` (simdi_hizmet++). İki çekirdek N=5000 kez ortak sayacı (kritik
bölgede DÜZ artırım) artırır. **Kanıt:** sayac=10000 (mutual-exclusion, lost-update yok) + **her çekirdek TAM
5000** (FIFO adalet, açlık yok — spinlock'un vermediği garanti), 6/6 det → "SMP TICKET OK". Naked trampoline
(D-174), dc ivac/civac+dsb. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-191 — OS: SMP 4-çekirdek bring-up — PSCI çoklu-AP (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-190).

**Karar [ETKİ: yeni `test/bare_metal/smp4_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]** Çok-
çekirdek 2→4 ölçekleme. QEMU -smp 4, BSP 3 AP'yi PSCI CPU_ON ile başlatır (3 çağrı, target MPIDR affinity=1/2/3).
ORTAK naked-trampoline giriş: her AP `mrs mpidr_el1 & 0xFF` ile hangi çekirdek olduğunu bulur → MPIDR-indeksli
KENDİ 8KB stack'ini kurar (ap_yiginlar[no+1]*8192) → cekirdek_durum[no].canli set (64-byte hizalı, false-sharing
yok). **Kanıt:** 3×"CPU_ON ret=0" + 3×MPIDR-Aff0=1/2/3 → "SMP4 OK 4 cekirdek", 5/5 det. QEMU virt MPIDR-Aff0=
çekirdek-no. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-190 — OS: x86 userspace ring3 + syscall — privilege ayrımı (D2-x86) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-189).

**Karar [ETKİ: yeni `test/bare_metal/ring3_x86.c`; `Makefile`. Yalnız test — runtime/boot/linker değişmedi.]**
aarch64 D2/D3 (EL0 privilege ayrımı)'nın x86 muadili — **çift-mimari userspace paritesi**. GDT'ye ring3
segmentleri (DPL=3: user-kod 0x1b, user-veri 0x23) + TSS (RSP0 ring0 stack) + IDT int-0x80 gate (DPL=3). iretq
ile ring3'e geç → ring3 kod CPL=3'te koşar. **Kanıt:** CS.RPL=3 + int 0x80 (rax=1)→ring0 handler + `cli`@ring3→
**#GP yakalandı** → "RING3 X86 OK", 5/5 det. **2 bug çözüldü:** (1) boot page-table supervisor-only → ring3
sayfalarına runtime U/S-bit (smp_x86 harita deseni); (2) monitor-stdio seri karışması. **Dürüst sınır:** CPL+
privileged-instruction-#GP ayrımı kanıtlar; tam sayfa-tabanlı user/kernel izolasyonu (D-124 x86 muadili) ayrı
milestone. **Not:** Paralel mini-agent üretti (dürüst debug); cherry-pick ile entegre.

## D-189 — OS: ağ-recon kabuğu — canlı ping/dns komutları (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-188).

**Karar [ETKİ: yeni `test/bare_metal/recon_shell_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-
OS kabuk kapstonu: D-188 (interaktif kabuk) + D-177/178 (EL0 net) BİRLEŞİMİ. EL1 kabuk UART RX'ten CANLI komut
okur → ağ recon: `ping <oktet>` (ARP+ICMP echo, net syscall) → "PING: CANLI"/yanit-yok; `dns` (DNS A çöz) →
"DNS: <IP>". **Kanıt:** `printf 'ping 2\ndns\n' | qemu -serial stdio` → "PING: CANLI" (SLIRP gateway det) +
"DNS: <ip>" → "RECON SHELL OK", 3/3. Ağ EL1'e taşındı (SVC EL1'den çalışır), tampon user-VA (D-150). Giriş
karakter-karakter pace (D-188 PL011 1-byte-holding dersi). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-188 — OS: interaktif UART kabuk — canlı komut oku + çalıştır (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-187).

**Karar [ETKİ: yeni `test/bare_metal/shell_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-181 (UART RX
gerçek stdio-giriş) + D-135 (komut parse) BİRLEŞİMİ = GERÇEK interaktif kabuk. EL1 kabuk döngüsü: "KABUK> "
prompt → UART RX'ten CANLI satır oku (RXFE poll + DR, byte-echo, '\n'e kadar) → tokenize → FS syscall çalıştır
(yaz num=17/oku num=18/ls num=19-20). SABİT script DEĞİL. **Kanıt:** `printf 'yaz gunluk MERHABA\noku
gunluk\nls\n' | qemu -serial stdio` → prompt+echo + "MERHABA" (oku) + "gunluk" (ls) → "SHELL OK". **KRİTİK
bulgu:** QEMU virt PL011 reset RX = **1-byte holding register** (FIFO değil) → burst-pipe overrun (kararsız);
fix = Makefile girişi KARAKTER-KARAKTER ~30ms pacing → 8/8 deterministik. Tamponlar user-VA (D-150 guard).
**Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-187 — OS: x86 SMP AP başlatma — Local APIC INIT-SIPI (D-169 x86 paritesi) (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-186).

**Karar [ETKİ: yeni `test/bare_metal/smp_x86.c`; `Makefile`. Yalnız test — runtime/boot/linker değişmedi.]**
D-169 (aarch64 PSCI CPU_ON)'un x86 muadili — çok-çekirdek universal-OS paritesi. BSP, Local APIC (0xFEE00000)
ICR (0x300/0x310) ile AP'ye INIT IPI + SIPI×2 (vektör=trampolin>>12) gönderir. **AP GERÇEKTEN KOŞTU (fallback
DEĞİL):** real-mode (SIPI 0x8000) → protected → **long-mode** (CS64, EFER.LMA, PG+PAE, kendi stack) → C fn →
kendi APIC ID (0x1, BSP'nin 0'ından FARKLI) okudu + paylaşımlı bayrak set etti. **2 zor bug çözüldü (ham QEMU
log):** (1) #PF @0xFEE00020 — LAPIC boot page-table'da harita-dışı → test-içinde runtime 2MB uncacheable
huge-page map (PDPT[3]); (2) triple-fault — trampolin 64-bit kodu GDT verisiyle çakışıyordu → veri 0x100'e
kaydırıldı + kod-sığdı invaryantı. Trampolin elle-derlenmiş makine-kodu blob (clang 16-bit üretemez, linker
kısıt-dışı). 5/5 det. **Not:** Paralel mini-agent üretti (dürüst debug); cherry-pick ile entegre.

## D-186 — OS: SMP çekirdekler-arası üretici-tüketici — lock-free SPSC ring (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-185).

**Karar [ETKİ: yeni `test/bare_metal/smp_prodcons_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-174/180/179 SMP üstünde: çekirdek 0 ÜRETİR, çekirdek 1 TÜKETİR — **lock-free SPSC** halka tampon (üretici
yalnız bas, tüketici yalnız son yazar → lock GEREKMEZ, bariyerler yeter). **Kanıt:** 1000 öğe FIFO sırada
(sira_bozuldu=0), toplam=499500 (Σ0..999), 5/5 det → "SMP PRODCONS OK". **2 gerçek SMP bug çözüldü:** (1)
ring-overrun — bas/son serbest-akan ama dolu-testi maskeli-karışık → serbest-akan konvansiyon (dolu=(bas-son)
==KAP); (2) false-sharing — bitişik slotlar aynı cache-satırında → dc civac/ivac komşuyu bozuyordu → her slot
64-byte padded RingSlot. Naked trampoline (D-174), dc civac/ivac+dsb bariyerler. **Not:** Paralel mini-agent
üretti (dürüst debug); cherry-pick ile entegre.

## D-185 — OS: userspace DHCP — EL0 syscall ile ağ oto-konfig (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-184).

**Karar [ETKİ: yeni `test/bare_metal/userspace_dhcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 net-syscall (24/25) üstünde EL0 süreç KENDİ IP'sini DHCP ile öğrenir — DISCOVER inşa (Eth-broadcast+
IPv4 0.0.0.0→255.255.255.255+UDP 68→67+BOOTP+magic+opt53=1) sys2(24) → OFFER sys2(25) → 7 alan doğrula
(op=2,xid,yiaddr,magic,opt53=2,portlar,ethertype). **Kanıt:** yiaddr=10.0.2.15 → "USERDHCP OK". DETERMİNİSTİK
(SLIRP DHCP, internetsiz). EL0 .rodata-deref-etmez (D-177). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-184 — OS: userspace HTTP GET — EL0 syscall ile uygulama katmanı (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-183).

**Karar [ETKİ: yeni `test/bare_metal/userspace_http_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 net-syscall üstünde EL0 süreç bir web sayfası çeker — DNS(example.com)→TCP handshake(sys2 24/25)→HTTP
GET(PSH+ACK)→yanıt "HTTP/1." ara. **Kanıt:** 104.20.23.154:80 → **HTTP/1.1 200 OK** → "USERHTTP OK" (gerçek
RX). HTTP request byte'ları EL0 tamponuna elle yazıldı (.rodata-deref-etmez, D-177). host-internet+fallback.
**Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-183 — OS: userspace TCP handshake — EL0 syscall ile soket (2026-07-03) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-182).

**Karar [ETKİ: yeni `test/bare_metal/userspace_tcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 net-syscall (24/25) üstünde EL0 süreç TAM TCP üç-yönlü handshake yapar (çekirdekte TCP durum-makinesi
YOK): ARP→DNS(example.com)→SYN(pseudo-header checksum)→**SYN-ACK al**(flags=0x12,ack=seq+1 doğrula)→ACK→
ESTABLISHED. **Kanıt:** 172.66.147.243:80 → "USERTCP OK" (gerçek RX, 3/3 stabil). **Userspace soket katmanı
tam (D-183 TCP + D-184 HTTP + D-185 DHCP): EL0 süreç raw-frame syscall'larıyla L2-L7 protokol yığını çalıştırır.**
host-internet+fallback. EL0 .rodata-deref-etmez (D-177). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-182 — OS: x86_64 CMOS RTC okuma — donanım saati (D-172 x86 paritesi) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-181).

**Karar [ETKİ: yeni `test/bare_metal/rtc_x86.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** D-172 (aarch64
PL031 RTC)'nin x86 paritesi (universal-OS piları). PC uyumlu MC146818 CMOS RTC: port 0x70 (index)/0x71 (data),
inline asm `outb/inb`. BCD register'lar (0=sn,2=dk,4=saat,7=gün,8=ay,9=yıl); UIP (Status-A bit7) beklenip
tutarlı okuma. **Kanıt:** 2026-07-02 20:13:05 (host wall-clock, `-rtc base=utc`) → "RTC X86 OK". Deterministik
makul-pencere (yıl 24-99, ay 1-12, gün 1-31). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-181 — OS: PL011 UART RX giriş yolu — donanım okuma (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-180).

**Karar [ETKİ: yeni `test/bare_metal/uart_rx_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** İlk kez
konsol RX (giriş) yolu — interaktif kabuğun ön-koşulu. PL011 FR (0x09000000+0x18) RXFE(bit4)+DR(0x00). **GERÇEK
giriş enjeksiyonu ÇÖZÜLDÜ (Windows gate zorluğu):** `-chardev file,input-path=` Windows'ta "not supported",
AMA **`-serial stdio` + stdin'e byte pipe** (`printf 'K' | qemu ... -serial stdio`) çalışır → guest RXFE=0
görür, DR'den 0x4b='K' okur, echo → "UART RX OK". Fallback: byte gelmezse bounded-spin → RXFE=1 (boş) doğru →
"UART RX PATH OK" (deadlock yok). **Kanıt:** giriş-enjeksiyon 3/3 "UART RX OK". **Ders:** QEMU-Windows seri-giriş
= `-serial stdio` + pipe. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-180 — OS: SMP atomik sayaç çekişmesi — LDXR/STXR lost-update yok (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-179).

**Karar [ETKİ: yeni `test/bare_metal/smp_atomic_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
İki çekirdek AYNI paylaşımlı sayacı N=10000 kez ATOMİK artırır — aarch64 exclusive-monitor RMW: `dmb ish;
ldxr; add; stxr; cbnz-retry`. Rakip STXR fail → taze değerle retry → hiçbir artırım düşmez. **Kanıt:** sayac=
**20000** (=2N), 9/9 deterministik → atomik doğruluk (atomik olmasa <20000 lost-update). Cache-coherency (dc
ivac/civac + dsb, RMW-öncesi/sonrası), rendezvous ile eşzamanlı çekişme, naked trampoline (D-174). **Not:**
Paralel mini-agent üretti; cherry-pick ile entegre.

## D-179 — OS: SMP bariyer senkronizasyonu — iki çekirdek lockstep (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-178).

**Karar [ETKİ: yeni `test/bare_metal/smp_barrier_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-170/174 SMP üstünde LOCKSTEP senkron: iki çekirdek K=5 tur, her turda **sense-reversing bariyer**'de buluşur
(spinlock'lu varan-sayacı + nesil/generation; son gelen sayacı sıfırlar + nesli artırır; erken gelenler nesil
değişene kadar bounded poll). Nesil izleme ABA-problemini önler. **Kanıt:** cekirdek0_tur=5, cekirdek1_tur=5,
nesil=5, 3/3 deterministik → "SMP BARRIER OK". Cache-coherency (dc ivac/civac+dsb, 64-byte hizalı), naked
trampoline SP (D-174 dersi). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-178 — OS: userspace ICMP ping — EL0 syscall ile L3 protokol (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-177).

**Karar [ETKİ: yeni `test/bare_metal/userspace_ping_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176 raw-frame syscall'larının (net_gonder=24/net_al=25) MEYVESİ: EL0 (yetkisiz) süreç TAM protokol yığınını
kendi çalıştırır (çekirdekte değil). ARP-çöz + IPv4+ICMP Echo (payload "KEMGU") inşa → sys2(24) yolla →
sys2(25) poll ile echo reply doğrula. **Kanıt:** SLIRP gateway echo → "USERPING OK" (gerçek RX, pcap KEMGU
TX+RX), DETERMİNİSTİK. Protokol EL0'da, yalnız 2 syscall aracılık. **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-177 — OS: userspace DNS — EL0 syscall ile tam protokol çözümleme (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-176).

**Karar [ETKİ: yeni `test/bare_metal/userspace_dns_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-176'nın MEYVESİ: EL0 süreç TAM L2-L7 DNS yığınını userspace'te çalıştırır — ARP-çöz → Eth+IPv4+UDP+DNS
("example.com" A) inşa → sys2(24) yolla → sys2(25) poll ile yanıt al → ANSWER parse (isim-sıkıştırma 0xC0) →
IPv4 çıkar. **Kanıt:** example.com → 172.66.147.243 → "USERDNS OK" (gerçek RX, internet+fallback). **Bellek
güvenliği:** EL0 `.rodata` (AP=00) dereference EDEMEZ → hex-yazımı aritmetik (nibble_hex), disassembly ile
doğrulandı; tüm tampon EL0 user-yığınında (D-150 guard geçer). **Not:** Paralel mini-agent üretti; cherry-pick ile entegre.

## D-176 — OS: USERSPACE NETWORKING — EL0 süreç syscall ile ağ (süreç+ağ birleşimi) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-175).

**Karar [ETKİ: `runtime/kdl_kesme.c` (num=24/25 net syscall + externs); `Makefile` (BM_A64_OBJS'e
bm_a64_virtio_net.o + 14 net-test link satırından redundant explicit kaldırıldı); yeni
`test/bare_metal/userspace_net_arm.c`, Makefile hedefi.]** İKİ BÜYÜK ALT-SİSTEMİ BİRLEŞTİRİR: süreç/syscall
modeli (D-124..140) + ağ yığını (D-144..167). Şimdiye kadar ağ hep KERNEL (EL1) kodundan yapılıyordu; artık
bir EL0 (yetkisiz) süreç virtio-net'e DOĞRUDAN erişmeden, yalnız SYSCALL ile ham ethernet çerçevesi
gönderir/alır: **num=24 net_gonder(cerceve, uzun)** (kernel frame'i OKUR + virtio-net'e yollar; driver frame'i
kendi TX DMA buffer'ına kopyalar) + **num=25 net_al(buf, maxlen)** (kernel gelen frame'i user buffer'a YAZAR).

**GÜVENLİK (D-150/151 disiplini):** net_gonder frame'i user VA'da + mantıklı ethernet boyu (≤1514) olmalı
(kdl_user_yaz_ptr_gecerli okuma-length-bound); net_al hedef user VA'da olmalı (write-guard). Kötü pointer →
-1 (kernel belleği korunur). net_al kısa per-çağrı timeout (2M tik) → EL0 kendi poll döngüsünde tekrar
çağırır (D-158 yük-duyarlılık dersi). Net syscall'ları `#if __aarch64__` (x86'da yok).

**Link:** kdl_kesme.c artık kdl_virtio_net_* referans eder → bm_a64_virtio_net.o BM_A64_OBJS'e eklendi (D-143
blk deseni; tüm aarch64 kernel linkler, kullanılmasa dead-code, net+blk sürücü aynı anda link — clash yok,
net testleri zaten ikisini de linkliyordu). Net-test link satırlarından redundant explicit ref kaldırıldı.

**Kanıt (aarch64 QEMU + -netdev user):** userspace_net_arm.c — EL1 main net sürücüsünü kurar; EL0 launcher
ARP isteği (gateway 10.0.2.2) inşa eder → **sys2(24, frame, 42)** ile yollar → **sys2(25, rx, 128)** poll
ile SLIRP'in ARP yanıtını alır+doğrular → ayrıca kötü-pointer net_al(0x40000000)→-1 (guard) → "USERNET OK".
**Bir userspace program çekirdek-aracılı ağ syscall'larıyla ARP round-trip yaptı.**

## D-175 — OS: SELF-HOST Base64 kodlama/çözme — KEMGU payload codec (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-174).

**Karar [ETKİ: yeni `test/ornekler/base64_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Pentest OS payload-kodlama yardımcısı, saf KEMGU. RFC 4648 Base64 encode+decode round-trip:
`Dizi<karakter>` 64-karakter alfabe tablosu, hesaplanan 6-bit index ile erişim, bit ops (`>> << & |`), ham
karakter çıktısı (`yaz_karakter`, newline'sız). **Kanıt:** "KEMGU"→"S0VNR1U="→"KEMGU" → "KEM B64 OK" +
"KEM B64 DECODE OK". **Dil gözlemi (kısıt değil):** `karakter` tipi sayısal DEĞİL — `s[0]-'A'` → T003
(KEMGU no-implicit-conversion felsefesiyle tutarlı); decode'da karakter-aritmetiği yerine alfabede lineer
arama (64 karşılaştırma). Cihazsız deterministik gate.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-174 — OS: SMP iş-kuyruğu — iki çekirdek dinamik work-stealing (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-173).

**Karar [ETKİ: yeni `test/bare_metal/smp_queue_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi.]**
D-170 (statik yarı-yarıya bölme) → DİNAMİK work-stealing: 40 iş öğesi, paylaşımlı `sonraki_is` indeksi
SPINLOCK korumalı; iki çekirdek de kilit-al→indeks-çek→işle döngüsü koşar (i*i topla). **Kanıt:** toplam=
20540 (Σi², i=0..39) — **5/5 deterministik** (her öğe tam bir kez → spinlock doğru serialize); per-çekirdek
işlenen sayıları timing'e göre DEĞİŞİR (22/18, 17/23, 23/17…) → gerçek yarış = gerçek work-stealing → "SMP
QUEUE OK". **KRİTİK bare-metal bulgu (dürüst):** çekirdek 1 ilk versiyonlarda çöküyordu — C prologue
`stp x29,x30,[sp,#-0x20]!` PSCI CPU_ON'dan gelen **undefined SP** ile garbage adrese yazıyordu (D-170'te iş
basit→spill yok→gizli kalmış). **Çözüm:** `naked` trampoline giriş — asm ilk iş SP kur, sonra C'ye dallan.
**Kural: PSCI CPU_ON ile başlayan ikincil çekirdek, spill üretebilecek HERHANGİ bir C kodundan ÖNCE SP kurmalı.**

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı (dürüst debug notlarıyla); cherry-pick ile entegre.

## D-173 — OS: SELF-HOST SHA-256 — KEMGU kripto hash (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-172).

**Karar [ETKİ: yeni `test/ornekler/sha256_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA + güvenlik piları — CRC/checksum ÖTESİNDE gerçek KRİPTO hash: NIST FIPS 180-4 SHA-256
saf KEMGU'da. K[64]+H[8] diziler, W[64] mesaj çizelgesi, 64 tur (rotr, ch, maj, sigma), mod-2^32 toplama.
**Kanıt:** SHA-256("abc") = `ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad` (NIST
vektörü, TAM eşit) → "KEM SHA OK". **Dil-doğrulaması:** dtam32 mod-2^32 wrap (`0xFFFFFFFF+1==0`) + rotate-
right (dtam32 `>>`→lshr) ÇALIŞIR. **CODEGEN BULGU (spawn_task ile ayrı fix'e flag'lendi):** dtam32 DİZİ-ELEMANI
doğrudan `>>` operandı olunca codegen `ashr` (İŞARETLİ) üretir → unsignedness kaybolur. **Workaround (dil-
seviyesi, codegen değişmeden):** bit-karıştırmayı skaler-dtam32-parametreli yardımcı işlevlere taşı → argüman
geçince kaydırma skaler üstünde → `lshr` (doğru). IR: 0 ashr, 3 lshr. Cihazsız deterministik gate.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; codegen-bulgu spawn_task ile flag'lendi; cherry-pick ile entegre.

## D-172 — OS: PL031 RTC okuma — donanım gerçek-zaman saati (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-171).

**Karar [ETKİ: yeni `test/bare_metal/rtc_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** NTP (D-167)
zamanı AĞDAN aldı; bu DONANIMDAN alır (deterministik, ağsız). QEMU virt PL031 RTC 0x09010000'de (ilk 1GB
Device-map, MMU-on erişilebilir). DR register (offset 0x00) = Unix epoch saniyesi (u32). `*(volatile
uint32_t*)0x09010000` ile oku → makul-kontrol (>1.6G, <2.0G). **Kanıt:** DR=0x6a46a824=**1783015460 =
2026-07-02 18:04 UTC** (bugünle uyumlu) → "RTC OK". Host wall-clock yansıması (her koşuda 1-2s değişir ama
makul-pencere hep geçer → deterministik-pass). Donanım-zaman = NTP'nin ağsız ikizi.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-171 — OS: SELF-HOST sıralama — KEMGU dizi in-place mutasyon (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-170).

**Karar [ETKİ: yeni `test/ornekler/sort_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA — D-168 (CRC32 saf-compute) ötesi: KEMGU DİZİ + in-place mutasyon + fonksiyon-geçişi
olgunluğu. Bubble sort ([5,2,8,1,9,3,7,4,6,0]→[0..9]), iç içe `iken` + `>` + geçici-değişken swap. **KRİTİK
dil-doğrulaması:** (1) **in-place dizi mutasyonu `d[i]=x` codegen'de ÇALIŞIR** (→ `kdl_dizi_yaz_tam`,
runtime bounds-checked — heap-uniform, self-host invaryantı). (2) **`Dizi<tam32>` FONKSİYON PARAMETRESİ
çalışır** (referansla geçer, mutasyon çağırana yansır — eski "LLVM v3 dizi param yok" notu GÜNCEL DEĞİL,
codegen artık destekliyor). Cihazsız gate. **Kanıt:** sıralama + `d[i]<=d[i+1]` doğrulama → "KEM SORT OK".

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-170 — OS: SMP paralel hesaplama + spinlock — iki çekirdek gerçek iş (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-169).

**Karar [ETKİ: yeni `test/bare_metal/smp_compute_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi,
tüm SMP mantığı inline asm.]** D-169 (ikincil çekirdek bir bayrak set etti) → GERÇEK PARALEL HESAPLAMA:
çekirdek 0 dizinin ilk yarısını (Σ0..99=4950), çekirdek 1 ikinci yarısını (Σ100..199=14950) topladı →
toplam 19900 (yalnız İKİSİ de payını doğru hesaplarsa çıkar). **İKİ birleştirme yolu:** (A) 64-byte-hizalı
ayrı-slot (yarışsız), (B) **SPINLOCK** — aarch64 `LDAXR`/`STXR` atomik test-and-set + `STLR` release, ortak
akümülatöre iki çekirdek de güvenli ekledi (yarış-koşulu serialize). Cache-coherency (MMU-off çekirdek1 /
MMU-on çekirdek0): `dc civac`/`dc ivac`+`dsb sy` bariyerleri, kilit satırı her denemede tazelenir. DETERMİNİSTİK
(bounded bekleme 40M tik + 64-yield backoff; 5/5). **Kanıt:** "SMP COMPUTE OK toplam=19900". **Dürüst sınır:**
MMU-off/on coherency manuel bariyerlere dayanır (D-169 sınıf riski); test coherency bozulursa "SMP COMPUTE
KISMI/FAIL" basar (sessiz-gizlemez). Çok-çekirdek pilarının gerçek-iş adımı.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı (dürüst teknik notlarla); cherry-pick ile entegre.

## D-169 — OS: SMP 2. çekirdek bring-up — PSCI CPU_ON (çok-çekirdek) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-168).

**Karar [ETKİ: yeni `test/bare_metal/smp_arm.c`; `Makefile`. Yalnız test — runtime/boot değişmedi, tüm SMP
mantığı smp_arm.c inline asm.]** YENİ PILAR: çok-çekirdek (performans/ölçek). Şimdiye kadar tek-çekirdek.
QEMU virt `-smp 2` ile ikincil çekirdek PSCI CPU_ON (fn_id=0xC4000003) ile başlatılır. **GERÇEK CPU_ON**
(fallback PSCI_VERSION değil): conduit=**HVC** (QEMU virt EL2-firmware'siz → HVC), ret=0x0 (SUCCESS), hedef
CPU MPIDR affinity=0x1, entry=cekirdek1_giris fiziksel adresi (identity-map). **Çekirdek 1 GERÇEKTEN koştu:**
paylaşılan `cekirdek1_canli` bayrağını YALNIZ çekirdek 1 giriş fn'si yazar; çekirdek 0 onu görünce "SMP OK".
**Cache coherency ele alındı:** çekirdek 1 MMU-OFF (non-cacheable) → RAM'e yaz + `dsb sy`; çekirdek 0 MMU-ON
(WB-cacheable) → poll'da `dc ivac`+`dsb sy` (invalidate-to-PoC, taze oku); bayrak 64-byte hizalı. Çekirdek 1
kendi 8KB stack'ini kurar (PSCI SP kurmaz). **Kanıt:** "SMP OK 2 cekirdek (HVC, ret=0x0)".

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı (dürüst teknik notlarla); cherry-pick ile entegre.

## D-168 — OS: SELF-HOST CRC32 — KEMGU saf-hesaplama algoritması (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-167).

**Karar [ETKİ: yeni `test/ornekler/crc32_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA — mmio ÖTESİNDE saf compute: KEMGU dilinin gerçek algoritma kaldırdığını kanıtlar.
Standart IEEE 802.3/zlib CRC-32 (polinom 0xEDB88320, tablosuz bit-bit), cihazsız. **Kanıt:** CRC-32("123456789")
= **0xCBF43926** (standart test vektörü, birebir). **KRİTİK dil-doğrulaması:** tüm bitwise op'lar çalışıyor
(`^`→xor, `&`→and, `|`→or, `<<`→shl, `>>`→ashr/lshr). **`>>` işlenen-tipine göre kod üretir:** tam32(işaretli)→
`ashr`, dtam32(işaretsiz)→`lshr` → CRC crc değişkeni `dtam32` OLMALI (MSB sık 1; ashr algoritmayı bozar).
Doğru işaretlilik semantiği. `yazdir_isaretsiz_tam(dtam32)` işaretsiz gösterim. Cihazsız gate (net/drive yok).

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-167 — OS: NTP istemcisi — internetten zaman senkronizasyonu (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-166).

**Karar [ETKİ: yeni `test/bare_metal/ntp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** OS açılışta
internetten doğru saati öğrenir. DNS-çöz(time.google.com→216.239.35.8) → Ethernet+IPv4(UDP)+UDP(123→123)+
SNTP(48 byte, LI/VN/Mode=0x1B client) gönder → response RX → Transmit Timestamp (offset 40, 1900'den beri
saniye) çıkar → Unix = ntp_sn - 2208988800. **Kanıt:** ntp_sn=3992001928 → Unix=**1783013128 = 2026-07-02
17:25:28 UTC** (bugünün tarihiyle uyumlu). **GERÇEK RX** (fallback değil). **Sınır:** host-internet-bağımlı
(offline → TX-pcap "NTP SENT OK", pcap'te UDP 123↔123). SLIRP dış-UDP proxy.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-166 — OS: reverse DNS (PTR) — IP → isim çözümleme (recon) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-165).

**Karar [ETKİ: yeni `test/bare_metal/dns_ptr_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest
recon: bir IP'nin hangi isme ait olduğunu bul (hedef tanıma). D-157 DNS A-çözümlemesini PTR'ye uyarlar:
IP oktetlerini TERS sırada + ".in-addr.arpa" QNAME, QTYPE=12 (PTR). Yanıtın ANSWER RDATA'sındaki domain-name'i
`isim_oku` ile PARSE eder (isim_atla'nın tersi — label biriktir + 0xC0 compression pointer takip, ≤32-atlama
sonsuz-döngü koruması). **Kanıt:** 8.8.8.8 → **dns.google** → "PTR OK". ANCOUNT=1, gerçek internet (SLIRP→host
DNS). **Sınır:** host-DNS-bağımlı (PTR yoksa "KISMI" kısmi kanıt; "PTR OK" yalnız gerçek isimde). L2-L4+DNS-A/PTR.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-165 — OS: SELF-HOST virtio-blk kapasite okuma — KEMGU disk config-space (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-164).

**Karar [ETKİ: yeni `test/ornekler/virtio_blk_config_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/
codegen değişmedi.]** Özgün DNA — D-163 (virtio-net MAC) desenini virtio-blk'a taşır: `.kem` sürücüsü disk
KAPASİTESİNİ config-space'ten okur. Slot tara → DeviceID=2 (virtio-blk) → config offset 0x100+0x104 (mmio_oku32
×2) → u64 capacity (sektör sayısı). **Kanıt:** `dd bs=512 count=64` disk → capacity=**64** → "KEM BLK OK".
mmio_oku8 yok → 32-bit×2 word. `değilse eğer`/`ve`/shift/mask codegen'de sorunsuz. DETERMİNİSTİK (disk boyutu
bilinir). KEMGU dili disk-cihaz config erişimi de kaldırıyor (net+blk self-host okuma tam).

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-164 — OS: TCP SYN port-tarayıcı — pentest recon (nmap-lite) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-163).

**Karar [ETKİ: yeni `test/bare_metal/port_scan_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** İkonik
pentest aracı — bir host'un açık portlarını bul. D-159 SYN-inşasını (`tcp_syn_kur`, src-port parametrik) çok-port
taramaya genişletir: DNS-çöz(example.com) → port listesi {80,443,22,8080,65000} için SYN gönder → yanıt sınıflandır:
SYN-ACK(0x12)=AÇIK, RST(0x04/0x14)=KAPALI, timeout=FİLTRELİ. Yarım-açık bağlantılar RST ile kapatılır; src-port
per-port (gecikmiş yanıt eşleme). **Kanıt:** example.com → 80/443/8080 AÇIK, 22/65000 FİLTRELİ → "PORT SCAN OK"
(gerçek SYN-ACK RX). **Sınır:** host-internet-bağımlı (offline → TX-pcap fallback "PORT SCAN SENT OK", pcap'te
5 farklı dst-port SYN). nmap-lite recon primitifi.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-163 — OS: SELF-HOST virtio-net MAC okuma — KEMGU config-space erişimi (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-162).

**Karar [ETKİ: yeni `test/ornekler/virtio_net_mac_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/
codegen değişmedi.]** Özgün DNA — D-160'ı (virtio-net TANIMA) bir adım ileri taşır: `.kem` sürücüsü cihazın
MAC adresini CONFIG-SPACE'ten okur. virtio-mmio cihaza-özel config offset 0x100'de; virtio-net için ilk 6
byte MAC. `.kem`: slot tara → DeviceID=1 bul → `mmio_oku32(y, taban+0x100)` + `+0x104` (2 word) → MAC
byte'larını little-endian çıkar (`(w >> (8*k)) & 0xFF` — `ashr`+mask codegen'de tam destekli). **Kanıt:**
QEMU virtio-net varsayılan MAC **52:54:00:12:34:56** okundu → "KEM MAC OK". mmio_oku8 dilde YOK (oku16/32/64
var) → 32-bit oku + shift/mask. Codegen kısıtı yok. **KEMGU dili gerçek cihaz config-space erişimi kaldırıyor.**

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-162 — OS: DHCP DISCOVER/OFFER — ağ oto-konfigürasyon (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-161).

**Karar [ETKİ: yeni `test/bare_metal/dhcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Bir bare-metal
OS'un ilk açılış adımı: DHCP ile ağ config al. Ethernet(broadcast)+IPv4(0.0.0.0→255.255.255.255,UDP)+UDP(68→67)
+BOOTP/DHCP(op=1, xid, chaddr, magic 0x63825363, option 53=1 DISCOVER) gönder → SLIRP OFFER'ı RX ile al.
**7 alan doğrulandı:** ethertype/proto, UDP portları (67→68), op=2 (BOOTREPLY), xid eşleşme, yiaddr non-zero,
magic cookie, option 53=2 (OFFER, TLV yürüyüşü). **Kanıt:** yiaddr=**10.0.2.15** → "DHCP OK". **DETERMİNİSTİK**
— SLIRP dahili DHCP sunucusu (internet gerekmez). OS artık kendi IP'sini otomatik öğreniyor.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-161 — OS: HTTP GET over TCP — uygulama katmanı (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-160).

**Karar [ETKİ: yeni `test/bare_metal/http_get_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** İLK UYGULAMA
KATMANI — OS bir web sayfası çekiyor. D-159 TCP handshake'i üstüne TCP DATA exchange: ARP→DNS(example.com)→
SYN/SYN-ACK/ACK ESTABLISHED → HTTP GET isteği (`GET / HTTP/1.1\r\nHost:...\r\nConnection: close\r\n\r\n`) PSH+ACK
(flags=0x18) DATA segmenti olarak gönder (seq/ack takibi, pseudo-header checksum payload dâhil) → HTTP yanıtını
RX ile al → durum satırında "HTTP/1." ara. **Kanıt:** hedef 104.20.23.154:80 → **HTTP/1.1 200 OK** → "HTTP GET
OK". **GERÇEK RX** (fallback değil). L2+L3+L4+DNS+HTTP tam ağ yığını.

**Kapsam/sınır (GATE-BELİRSİZLİĞİ):** HOST İNTERNET'ine bağlı (D-159 gibi). Offline → TX-pcap fallback ("GET /"
pcap'te → "HTTP GET SENT OK"). Timeout 20s. Tek-segment yanıt (multi-segment reassembly yok — kanıta yeter).

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-160 — OS: SELF-HOST virtio-net tanıma — KEMGU dilinde ağ-cihaz sürücüsü (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-159).

**Karar [ETKİ: yeni `test/ornekler/virtio_net_selfhost.kem`; `Makefile`. Yalnız test/örnek — runtime/codegen
değişmedi.]** Özgün DNA — OS kendi dilinde (KEMGU) yazılıyor. D-148/149 (virtio-blk, DeviceID=2) desenini
virtio-NET'e (DeviceID=1) taşır: `.kem` sürücüsü virtio-mmio slot aralığını (`iken` döngüsü) tarar, her
slotta `mmio_oku32(y, adres)` ile MAGIC (0x74726976 "virt") + DEVICE_ID okur, DeviceID=1'i bulunca tanır.
`yetki<MMIO>` object-capability (derleme-zamanı ispat, sıfır runtime) her okumada ödünç alınır. **Kanıt:**
`kemgu --llvm` → clang aarch64 → QEMU (`-device virtio-net-device`) → magic=1953655158 (0x74726976) + id=1
→ "KEM NET OK". **Codegen kısıtına takılmadı** (yetki/mmio_oku32/iken/eğer+ve/tam64 hepsi mevcut). KEMGU
dili gerçek ağ-cihaz tanıma sürücüsü kaldırıyor.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-159 — OS: TCP gerçek üç-yönlü handshake — SYN-ACK alımı (Faz H) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-158).

**Karar [ETKİ: yeni `test/bare_metal/tcp_connect_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-155 (yalnız SYN emisyonu) TAM handshake'e tamamlanır — SLIRP'in dış-TCP proxy'si üzerinden GERÇEK bir
internet host'una. Adımlar: virtio-net kur → ARP gateway MAC → **DNS ile "example.com" A-kaydı çöz** (D-157
mantığı) → çözülen IP:80'e TCP SYN (pseudo-header checksum, D-155 inşası) → **SYN-ACK al** (RX; flags=0x12
doğrula + ack_num=seq+1) → ACK gönder → ESTABLISHED → nazik RST/ACK kapanış. **GERÇEK SYN-ACK RX** (fallback
DEĞİL). **Kanıt:** hedef 104.20.23.154:80 → SYN-ACK → "TCP CONNECT OK". Ağ yığını artık L2(ARP)+L3(IP)+
L4(TCP-established)+DNS tam zincir.

**Kapsam/sınır (GATE-BELİRSİZLİĞİ):** HOST İNTERNET'ine bağlı (SLIRP dış-TCP'yi host'a proxy'ler). Offline
ortamda SYN-ACK gelmez → test TX-pcap fallback'ine ("TCP CONNECT SENT OK", pcap'te SYN) düşer; Makefile ikisini
de kabul eder. Timeout 20s (DNS+TCP iki round-trip). Geliştirme makinesi internetli → gerçek handshake geçer.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-158 — OS: ARP host-keşfi — subnet taraması (pentest recon) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-157).

**Karar [ETKİ: yeni `test/bare_metal/arp_scan_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-OS
temel keşif primitifi (L2 canlı-host bulma). D-145 tek-hedef ARP round-trip'ini SUBNET TARAMASINA genişletir:
10.0.2.1–10.0.2.15 aralığına ARP request broadcast → RX ile reply'leri topla (60 poll) → her reply'den spa
(sender IP) + sha (sender MAC) çıkar, dedup. **Kanıt:** SLIRP gateway (10.0.2.2) + DNS (10.0.2.3) → **2 canlı
host** deterministik keşfedildi → "ARP SCAN OK". Gateway ARP'a her zaman yanıt verir → ≥1 host garanti.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-157 — OS: DNS A-kaydı çözümleme — isim → IPv4 (Faz G ağ derinleşme) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-156).

**Karar [ETKİ: yeni `test/bare_metal/dns_resolver_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]**
D-147 DNS round-trip'i (yanıt ALINIR ama parse EDİLMEZ) tam çözümleyiciye genişletir: DNS yanıtının
ANSWER bölümünü parse edip çözümlenen IPv4 A-kaydını çıkarır. **İsim sıkıştırma (0xC0 pointer) ele
alınır** (`isim_atla` helper — hem compression-pointer hem düz-label; Question + answer NAME atlama),
sınır kontrolleri (paket taşması, RDLENGTH). **Kanıt:** "example.com" A sorgusu → SLIRP host resolver'a
forward → yanıt RX → ANCOUNT=2, ilk A-kaydı çıkarıldı → 172.66.147.243 → "RESOLVE OK". Reprodüsibl (2
koşu birebir).

**Kapsam/sınır (GATE-BELİRSİZLİĞİ):** Bu test HOST İNTERNET'ine bağlı (SLIRP sorguyu host DNS'e forward
eder; gerçek A-kaydı gerekir). Offline ortamda ANCOUNT=0 → "A-KAYDI YOK" → gate FAIL olabilir. Parser
DETERMİNİSTİK; yalnız gerçek-çözümleme internet-bağımlı. (D-147 aksine yalnız "yanıt geldi" kontrol eder,
internet gerektirmez.) Geliştirme makinesi internetli → gate geçer.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-156 — OS: ICMP echo (ping) round-trip — ağ katmanı (Faz G) (2026-07-02) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-155).

**Karar [ETKİ: yeni `test/bare_metal/icmp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-OS
keşif primitifi (ping-sweep temeli). ARP ile gateway (SLIRP 10.0.2.2) MAC çöz → Ethernet+IPv4(proto=1)+
ICMP Echo Request (type=8, id=0xBEEF, seq=1, ICMP checksum RFC1071, payload "KEMGU") gönder → **echo
reply'i RX ile al** (SLIRP gateway ping'lerini host-ayrıcalığı gerektirmeden DAHİLİ yanıtlar) → doğrula
(type=0/code=0, id/seq eşleşir, payload geri döner) → "PING OK". **GERÇEK RX round-trip** (TX-pcap fallback
değil; fallback Makefile'da mevcut ama tetiklenmedi). virtio-net TX+RX + ARP + IP üstüne kurulu.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-155 — OS: TCP SYN paket emisyonu — ağ katmanı (Faz H) (2026-07-02)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-154).

**Karar [ETKİ: yeni `test/bare_metal/tcp_arm.c`; `Makefile`. Yalnız test — kaynak değişmedi.]** Pentest-OS
keşif primitifi (port-tarama temeli). ARP ile gateway MAC çöz → Ethernet+IPv4(proto=6)+TCP SYN segmenti
inşa (src 40000, dst 9999, SYN=0x02, **TCP checksum PSEUDO-HEADER dahil** = src/dst IP + proto + TCP-len)
→ gönder. **Kanıt: TX-pcap** (D-144/146 deseni) — pcap'te SYN segmenti "KEMG" seq marker'ı ile doğrulandı;
TCP checksum 0x1bf6 + full-segment-verify 0x0000 (RFC1071) + IP checksum 0x22c0 bağımsız Python ile teyit.

**Kapsam/sınır (DÜRÜST):** TAM handshake DEĞİL — yalnız SYN inşa+checksum+emisyon. SLIRP kapalı gateway
portuna (10.0.2.2:9999) SYN'i SESSİZCE DÜŞÜRÜR (user-mode TCP yığını RST dönmez) → RX round-trip bu
ortamda olmadı. Emisyon (pseudo-header checksum dahil) gerçek yapı taşı; tam handshake gerçek TCP peer
(internet-out veya listener) gerektirir → gelecek iş. Makefile hem RX ("TCP HANDSHAKE OK") hem TX-pcap
("KEMG") kontrol eder — listener'lı ortamda RX yolu otomatik geçer.

**Not:** Paralel mini-agent (worktree-izole) üretti + doğruladı; cherry-pick ile entegre.

## D-154 — OS: düşman-userspace bombardıman regresyon testi — syscall-ptr güvenlik yüzeyi (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-153).

**Karar [ETKİ: yeni `test/bare_metal/guvenlik_bombardiman_arm.c`; `Makefile`. Yalnız test — kaynak
değişmedi.]** D-150+D-151 sertleştirilmiş syscall-pointer yüzeyinin KALICI regresyon bekçisi. Bir EL0
launcher, 8 kötü-niyetli syscall'lık bir BATARYA ateşler (unmapped/kernel-adres/MMIO okuma+yazma
hedefleri: num=5 yaz×2, 16 dosya_oku, 15 dosya_yaz, 17 dosya_yaz_metin, 18 dosya_oku_metin write-hedef,
20 dosya_ad write-hedef, 21 dosya_sil); her biri -1 dönmeli VE kernel HALT ETMEMELİ. Bataryadan sonra
geçerli iş akışı (dosya oluştur+oku) kernel'in tam canlılığını kanıtlar → "HOSTILE SURVIVED OK". Vaka #6'nın
"not-found değil gerçek D-150 write-guard reddi" olduğu, dosyanın önceden kurulup sonra geçerli tampona
okunabilmesiyle ayrıştırılır. Bir syscall halt ettirirse test FAIL → o guard eksik demektir (bisect talimatı).

**Not:** Paralel mini-agent (worktree-izole) tarafından üretildi + doğrulandı; cherry-pick ile entegre.

## D-153 — OS: kalıcı FS deserialize sertleştirme — poisoned boyut clamp (savunma-derinliği) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-152).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_dosya_yukle + num=18); yeni `test/bare_metal/guvenlik_kalici_arm.c`,
`Makefile`.]** Audit defense-in-depth bulgusu (EL0-erişilebilir DEĞİL — kötü niyetli disk gerekir):
kdl_dosya_yukle diskteki kdl_dosyalar[] tablosunu VERBATIM yükler. Kötü niyetli disk aşırı büyük `boyut`
içerirse → num=18 kdl_dosyalar[i].boyut byte kopyalar → 64-byte icerik[] tamponunu aşan OOB okuma →
kernel belleği user'a sızar. **İki katman:** (A) kdl_dosya_yukle deserialize sonrası sanitize — kullanildi
0/1, ad/icerik null-term, boyut `[0,64)` dışıysa 0. (B) num=18'de ham boyut yerine clamp'li `lim` (hem
`kdl_user_yaz_ptr_gecerli(arg2, lim+1)` hem kopya sınırı `n<lim`). **Kanıt:** guvenlik_kalici_arm.c — EL1
main elle "KEMG" magic + boyut=9999 ZEHİR disk image üretir, kdl_dosya_yukle, EL0 num=18 → dönen uzunluk
≤63 (9999 değil) + kernel sağ → "KALICI GUARD OK". **Negatif kanıt:** fix stash'lenince test doğru FAIL
eder ("KALICI GUARD HATA uz=9999") → zafiyet gerçek + test false-positive değil. Kalıcı regresyon (777) geçer.

**Not:** Paralel mini-agent (worktree-izole) tarafından üretildi + negatif-kanıtla doğrulandı; cherry-pick ile entegre.

## D-152 — OS: spawn-entry doğrulama — num=12 DoS koruması (güvenlik sertleştirme) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-151).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_surec_spawn); yeni `test/bare_metal/guvenlik_spawn_arm.c`,
`Makefile`.]** Audit confirmed bulgusu (medium DoS): num=12 spawn'da EL0, yeni sürecin GİRİŞ adresini
(arg=entry) tam kontrol eder. kdl_surec_spawn(entry) bu entry'yi yeni EL0 sürecinin ELR_EL1'ine koyar;
entry kernel/unmapped/hizasız ise EL0 komut-fetch'i fault → lower-EL sync exception → kdl_istisna_isle
sonsuz halt (**tek SVC ile tüm kernel ölür**). **Fix:** kdl_surec_spawn EN BAŞINA guard — entry paylaşılan
EL0 .user kod sayfası `[0x42000000, 0x42200000)` içinde VE 4-byte hizalı olmalı; değilse -1 (süreç
yaratılmaz, slot tüketilmez). Başka fonksiyona dokunulmadı. **Kanıt:** guvenlik_spawn_arm.c — EL0 launcher
sys(12, 0x40080000)[kernel] + sys(12, 0)[null] → ikisi de -1, kernel SAĞ; sys(12, &worker)[geçerli .user]
→ ≥0, worker koştu → "SPAWN GUARD OK" + "WORKER OK". spawn/yasam/calis/geri_al regresyonları (worker'lar
.user'da = geçerli) bozulmadan geçer.

**Not:** Paralel mini-agent (worktree-izole) tarafından üretildi + doğrulandı; cherry-pick ile entegre.

## D-151 — OS: syscall OKUMA-pointer doğrulama — kernel DoS + info-leak koruması (güvenlik sertleştirme) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-150).

**Karar [ETKİ: `runtime/kdl_kesme.c` (read-guard + num=5/15/16/17/18/21); `linker/bare-metal-aarch64.ld`
+ `bare-metal-x86_64.ld` (__rodata_start/end); yeni `test/bare_metal/guvenlik_oku_arm.c`, `Makefile`
hedefi.]** D-150'nin YAZMA-tarafı korumasının OKUMA-tarafı ikizi. Çekirdek, EL0-kontrollü bir string
pointer'ını **deref ederek OKURKEN** de doğrulamalı; aksi halde kötü/hatalı bir EL0 süreç:
- **DoS:** unmapped adres geçirir → kernel EL1'de data-abort → `kdl_istisna_isle` sonsuz halt (**tek
  SVC ile tüm kernel ölür**);
- **info-leak:** kernel adresi geçirir → kernel belleği UART'a yazılır (num=5) veya bir dosyaya
  kopyalanıp num=18 ile geri okunur (num=17→18 exfiltrasyon zinciri).

**Çok-ajanlı adversarial audit (23 ajan, 2.07M token) bu sınıfı üretti** — 14 confirmed EL0-reachable
bulgu, hepsi read-ptr; num=18 ad-okuması D-150 sonrası hâlâ açıktı (D-150 yalnız arg2 write-hedefini
koruyordu). Refuted: spawn-havuz int-bounds (zaten korumalı).

**Read-guard:** `kdl_user_oku_str_gecerli(p)` — izinli okuma bölgeleri `[user VA 0x42000000,0x42400000)
∪ kernel .rodata [__rodata_start,__rodata_end)`. `.data/.bss` (dosya tablosu burada!) / stack / heap /
Device MMIO / unmapped → RED. **Null-sonlandırıcı İZİNLİ bölge içinde bulunmalı** (yalnız mapped-izinli
byte taranır → tarama fault üretemez; straddle-over-read imkânsız; 4KB tarama tavanı). num=5 (yaz-string),
15/16/18/21 (dosya adı), 17 (ad + içerik arg2) → hepsi guard'lı, geçersiz→RED (-1).

**.rodata neden izinli:** mevcut testler çıktı/ad string LİTERALLERİNİ (.rodata, kernel adresi) syscall'a
geçirir (sys(5,"GUVENLIK OK"), dosya adı "mesaj"/"f"). Bunlar const, sır değil; izin vermek tüm testleri
korur. Sızıntı-hedefleri (.data/.bss/stack/heap) reddedilir.

**Kanıt (aarch64 QEMU):** `guvenlik_oku_arm.c` EL0 launcher: (2) num=5'e UNMAPPED 0x80000000 → RED,
kernel HALT ETMEZ; (3) num=16'ya kernel-RAM 0x40100000 → RED; (4) buraya ulaşmak = kernel sağ →
"GUVENLIK OKU OK". Fix öncesi (2) kerneli sonsuza halt ederdi. FS regresyonları (dosya/metin/ls/sil/
kabuk/kalıcı — .rodata ad + user-VA token) + D-150 bozulmadan geçer. x86_64 de __rodata sembolleriyle
linklenir (arch paritesi).

**Kapsam/sınır:** Read-guard string-deref eden syscall'ları kapsar. Kalan audit bulguları (ayrı D):
num=12 spawn-entry DoS (medium), persistence deserialize boyut-clamp (defense-in-depth), num=14 durum
ownership (low). **KURAL: kernel→user OKUYAN/YAZAN her yeni syscall ilgili guard'ı (oku_str / yaz_ptr)
kullanmalı.**

## D-150 — OS: syscall kullanıcı-pointer doğrulama — kernel bellek-yazma koruması (güvenlik sertleştirme) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-149).

**Karar [ETKİ: `runtime/kdl_kesme.c` (guard + num=18/20); yeni `test/bare_metal/guvenlik_arm.c`,
`Makefile` hedefi.]** KEMGU-OS'un çekirdek güvenlik invaryantını (bellek-güvenli OS) syscall
sınırında zorunlu kıl: kernel (EL1), kullanıcı-kontrollü bir pointer'a **user VA aralığı dışında**
YAZMAMALI. Aksi halde kötü/hatalı bir EL0 süreç, kernel'in yazdığı bir syscall'a kernel adresi
geçirip çekirdek belleğini bozabilir (privilege escalation vektörü).

**Guard:** `kdl_user_yaz_ptr_gecerli(p, len)` — yalnız `[0x42000000, 0x42400000)` (EL0 user VA)
içindeki, `len<=4MB` ve toplama-taşması olmayan yazma-hedeflerini kabul eder. Kernel'in
kullanıcı-tampona YAZDIĞI iki syscall'a eklendi: num=18 (dosya_oku_metin → buf'a içerik) ve
num=20 (dosya_ad → buf'a ad). Geçersizse RED (-1), yazma yapılmaz. Okuma-syscall'ları (.rodata
kernel çıktı stringleri) muaf — yalnız user-tampona YAZAN yollar denetlenir.

**Kanıt (aarch64 QEMU):** `guvenlik_arm.c` bir EL0 launcher olarak: dosya oluşturur, sonra
num=18'e (a) kernel-adresi 0x40000000 → **RED (-1)**, (b) geçerli user-tampon 0x42210000 →
**OK (>=0)** verir. İkisi de beklendiği gibiyse EL0 `yaz` syscall'ı ile "GUVENLIK OK" basar. Seri
çıktı: `GUVENLIK BASLA` → `GUVENLIK OK`. FS regresyonları (metin/ls/sil/kabuk — hepsi geçerli
user-tampon 0x42210000 kullanır) guard'la bozulmadan geçer.

**Kapsam/sınır:** Guard yalnız num=18/20 (mevcut write-to-user yollar). İleride kernel→user yazan
her yeni syscall aynı guard'ı kullanmalı (kural). Read-güvenliği (user'ın kernel .rodata OKUması)
zaten MMU AP=00 ile engelli — bu guard yazma-tarafı savunma-derinliği. Test-only bug (EL0'ın
kernel fonksiyonunu doğrudan çağırması) fix'te `yaz` syscall'ına çevrildi; guard mantığı değişmedi.

## D-135 — OS: basit userspace kabuk (shell) — komut ayrıştırma + FS dağıtım (Faz E/F DORUĞU) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-134).

**Karar [ETKİ: yeni `test/bare_metal/kabuk_arm.c`; `Makefile`. Yalnız test — mevcut syscall/kernel
kullanılır, kod değişmedi.]** Tüm userspace + FS + süreç yığınını tanınabilir bir OS artefaktına
bağlayan doruk: bir userspace program komut SCRIPT'ini ayrıştırıp (tokenize) FS syscall'larına
dağıtır — gerçek kabuk/komut yorumlayıcısı.

**Kabuk:** .user_data'daki script (yaz/oku/ls komutları) EL0'da in-place tokenize edilir (str_esit
+ tokenize helper'ları .user section'da, EL0-exec). yaz→dosya_yaz_metin, oku→dosya_oku_metin+bas,
ls→listele. Kernel çağırmaz; yalnız syscall.

**Öğrenilen (bellek koruması KANITI):** İlk deneme ISTISNA tip=0x24 DFSC=0x0E (permission fault,
FAR=0x40003fd3) — komut adı literalleri ("yaz"/"oku"/"ls") .rodata'da (AP=00); EL0 str_esit OKUYUNCA
fault. Bu, D3 bellek-korumasının GERÇEKTEN çalıştığının kanıtı (EL0 kernel belleğini okuyamaz).
DÜZELTME: komut adları .user_data'ya (AP=01, EL0-okunur). NOT: sys(5,literal) çıktı stringleri
.rodata'da KALIR (kernel EL1 okur, sorun yok) — yalnız EL0'ın DOĞRUDAN okuduğu stringler .user_data.

**Doğrulama (QEMU 11.0.1):** kabuk_arm — "SHELL> yaz gunluk KEMGU-OS" / "SHELL> oku gunluk" /
"  KEMGU-OS" / "SHELL> ls" / "  gunluk". Full gate GATE=0 (33 hedef). sıfır-uyarı. **KEMGU-OS artık
komut yorumlayan bir userspace kabuk çalıştırıyor — gösterici kernelden çalışan-OS'a.**

**Sıradaki:** UART RX (klavye → interaktif kabuk; gate zor); kaynak geri-alma; D2-x86; C5 virtio-blk.

---

## D-141 — OS: VirtIO-Blk gerçek disk okuma (C5 — kalıcı depolama, Faz E) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-140).

**Karar [ETKİ: yeni `runtime/kdl_virtio.c` (bare-metal virtio-mmio v2 blk sürücüsü); yeni
`test/bare_metal/virtio_arm.c`; `Makefile` (bm_a64_virtio.o + disk-imaj + QEMU virtio-blk). C/host
runtime — .kem driver'lardan (drivers/virtio/*.kem) yalnız register-offset bilgisi alındı, kod C.]**
İlk GERÇEK DONANIM depolama: QEMU virtio-blk diskinden blok okuma (RAM-FS'i kalıcı yapmanın temeli).

**Sürücü (kdl_virtio.c, aarch64):** virtio-mmio slot tara (0x0a000000+i*0x200, DeviceID=2) →
kdl_virtio_blk_bul. Init (kdl_virtio_blk_kur): reset→ACK→DRIVER→feature(VERSION_1 bit32)→FEATURES_OK
→virtqueue 0 (split: desc[8]+avail+used ayrı hizalı DMA tamponları, QueueDesc/Driver/Device Lo/Hi)
→DRIVER_OK. Oku (kdl_virtio_blk_oku): 3-desc zinciri (başlık RO + veri WR + durum WR) → avail.idx++
→ QueueNotify → used.idx poll → status==0 → 512 bayt kopya. DMA tamponları RAM identity-map (VA=PA);
QEMU coherent DMA (dsb ordering yeter, cache-flush yok). Register offsetleri constants.kem ile aynı.

**Doğrulama (QEMU 11.0.1):** virtio_arm — disk.img (blok 0'da "KEMGU-DISK-BLOK0") + `-device
virtio-blk-device` → kernel blok 0'ı okur, "KEMGU" doğrular → "DISK OK KEMGU". **İLK DENEMEDE geçti**
(virtqueue doğru). Full gate GATE=0 (37 hedef; diğer kernel'ler disk'siz — virtio target kendi
disk'ini kurar). sıfır-uyarı.

**Sıradaki:** virtio-blk YAZMA (D-142); dosya sistemini disk-backed yap; UART RX; D2-x86.

---

## D-149 — OS: SELF-HOST virtio init — KEMGU'da tarama + MMIO yazma (status handshake) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-148).

**Karar [ETKİ: yeni `test/ornekler/virtio_selfhost_rw.kem` (KEMGU!); `Makefile` (self-host-rw target).
Mevcut mmio/yetki runtime (D-148) kullanıldı.]** D-148 OKUMA'yı gerçek DRIVER INIT'e taşır: KEMGU
dilinde cihaz TARAMA + status durum-makinesi YAZMA.

**Mekanizma:** virtio_selfhost_rw.kem — (1) TARA: `iken i<32` döngüsünde her slot'un DeviceID'sini
mmio_oku32 ile oku (yetki ÖDÜNÇ → döngüde thread YOK), DeviceID!=0 ilk slotu bul. (2) HANDSHAKE:
status register'a mmio_yaz32 ile reset→ACK→ACK|DRIVER yaz — yetki LİNEAR olduğundan her yazmada
THREAD edilir (y→y1→y2→y3). (3) status geri oku = 3. **KEMGU dil özellikleri gerçek driver kodunu
kaldırıyor:** tam64 adres aritmetiği (i*512), döngü, linear-capability (borrow-in-loop + thread-in-chain).

**Doğrulama (QEMU 11.0.1):** virtio_selfhost_rw + virtio-blk device → KEMGU sürücüsü cihazı buldu,
handshake yaptı, status=3 okudu → "KEM VIRTIO RW OK". Full gate GATE=0 (45 hedef). libc-temiz.
sıfır-uyarı. **KEMGU tam bir virtio init sekansını (tarama+oku+yaz, capability-güvenli) kendi
dilinde çalıştırıyor — self-host OS sürücüsü.**

**Sıradaki:** .kem userspace program (EL0); TCP; UART RX; sürücüyü virtqueue'ya kadar genişlet.

---

## D-148 — OS: SELF-HOST virtio sürücüsü — KEMGU dilinde bare-metal OS sürücüsü (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-147).

**Karar [ETKİ: yeni `test/ornekler/virtio_selfhost.kem` (KEMGU!); yeni `runtime/kdl_yetki_bare.c`
(freestanding capability runtime); `Makefile` (bm_a64_mmio.o + bm_a64_yetki.o + self-host target).
Mevcut mmio codegen (src/llvm.c) + kdl_runtime_mmio.c bare-metal modu kullanıldı.]** Projenin
ÖZGÜN DNA'sı OS düzeyinde: bir OS sürücüsü KEMGU DİLİNDE yazıldı, KEMGU derleyicisiyle bare-metal
derlendi, gerçek donanım register'ı okudu.

**Mekanizma:** virtio_selfhost.kem — `yetki<MMIO>` (object-capability, DERLEME-ZAMANI donanım-erişim
ispatı, sıfır runtime yük) + `mmio_oku32(y, adres)` intrinsic'i ile virtio-mmio magic (0x0A000000)
+ version register'larını okur. kemgu --llvm → clang aarch64 (-x ir) → ld.lld → QEMU virt. mmio_oku32
codegen'de `kdl_mmio_oku32(adres)` volatile load'a iner (yetki runtime'a geçmez → WCET sıfır ek).
kdl_yetki_bare.c: KdlYetki (16B, codegen %kdl_yetki ile birebir) + olustur/geri_al (PRNG yerine sayaç,
libc yok). sret ABI aarch64'te x8 kullanır ama yetki MMIO'da kullanılmadığından benign.

**Doğrulama (QEMU 11.0.1):** virtio_selfhost — QEMU virt boş slot 0 virtio-mmio transport'u magic
(0x74726976) her zaman sunar → KEMGU sürücüsü okur+doğrular → "KEM VIRTIO OK" + version(1). **LİBC-TEMİZ**
(malloc/printf yok — capability-güvenli donanım erişimi). Full gate GATE=0 (44 hedef). sıfır-uyarı.
**KEMGU (memory-safe dil) kendi OS sürücüsünü kendi dilinde yazıyor — capability ile donanım erişimi
compile-time güvenli. Projenin özgün değer önerisi OS düzeyinde kanıtlandı.**

**Sıradaki:** self-host sürücüyü genişlet (yaz + tam virtio init .kem'de); .kem userspace program;
TCP; UART RX.

---

## D-147 — OS: DNS round-trip — UDP request-response (OS internet'le konuşuyor) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-146).

**Karar [ETKİ: yeni `test/bare_metal/dns_arm.c`; `Makefile`. Yalnız test — net driver + IP/UDP
(D-144/145/146) kullanılır.]** TÜM AĞ YIĞINI bir arada: gerçek istek-yanıt döngüsü (OS internet
servisiyle konuşuyor).

**Mekanizma:** (1) ARP ile DNS sunucusunun (SLIRP 10.0.2.3) MAC'ini çöz (sha çıkar). (2) DNS sorgusu
inşa et: eth(dst=dns_mac)+IPv4(dst=10.0.2.3)+UDP(dst=53)+DNS(header id/RD/qdcount=1 + qname "a.com" +
qtype=A + qclass=IN), IP checksum. (3) Gönder. (4) Yanıtı RX ile al + doğrula (IPv4+UDP, src=10.0.2.3,
src-port=53).

**Doğrulama (QEMU 11.0.1):** dns_arm — "DNS BASLA" + "DNS REPLY OK" (DNS sunucusundan UDP yanıtı
alındı). **İLK DENEMEDE.** Full gate GATE=0 (43 hedef). sıfır-uyarı. **OS gerçek bir internet
servisiyle (DNS) request-response yapıyor = internet-katmanı round-trip. Ağ yığını: ARP+IP+UDP+DNS.**

**Sıradaki:** ICMP ping; TCP handshake; DNS yanıtından IP çıkar (tam resolver); UART RX; D2-x86.

---

## D-146 — OS: IP/UDP paket gönderme — internet katmanı (Faz G derinleşme) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-145).

**Karar [ETKİ: yeni `test/bare_metal/udp_arm.c`; `Makefile`. Yalnız test — net driver (D-144/145)
kullanılır.]** ARP (L2) üstüne İNTERNET KATMANI: geçerli IPv4 + UDP paketi (IP header checksum
dâhil) inşa + gönder.

**Mekanizma:** ip_checksum (RFC 1071, 16-bit tümleyen toplamı). Frame: eth(IPv4) + IPv4(20:
v4/IHL5, total_len, TTL, proto=17, checksum, src=10.0.2.15, dst=10.0.2.3) + UDP(8: src=5000,
dst=53, len, checksum=0) + payload "KEMGU-UDP-DATA". virtio-net TX ile gönder.

**Doğrulama (QEMU 11.0.1):** udp_arm — paket gönder → filter-dump pcap → "UDP GONDERILDI" +
`grep -a "KEMGU-UDP-DATA" udp.pcap`. Full gate GATE=0 (42 hedef). sıfır-uyarı. **OS geçerli IPv4/UDP
paketi oluşturuyor (checksum'lı) — gerçek internet-protokol yığını temeli.**

**Sıradaki:** DNS/UDP round-trip (10.0.2.3:53'e sor, yanıt al); ICMP; TCP handshake; UART RX; D2-x86.

---

## D-145 — OS: ARP round-trip — 2-yönlü ağ (virtio-net RX + ARP protokolü) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-144).

**Karar [ETKİ: `runtime/kdl_virtio_net.c` (+RX queue (0) kurulumu + kdl_virtio_net_al); yeni
`test/bare_metal/arp_arm.c`; `Makefile`. Sadece ekleme — net TX (D-144) regresyonsuz.]** D-144
gönderme'yi ALMA ile tamamlar → gerçek 2-yönlü ağ + ilk protokol (ARP).

**Mekanizma:** kdl_virtio_net_kur artık RX queue 0'ı da kurar (NVQ_N tampon avail'e AÇIK verilir,
cihaz gelen paketleri yazar, QueueNotify 0 ile bildirilir). kdl_virtio_net_al: rx_used poll → gelen
çerçeveyi (net-başlığı 12 bayt atlanmış) kopyala + uzunluk döner. ARP protokol mantığı testte
(request/reply parse).

**Doğrulama (QEMU 11.0.1):** arp_arm — kernel gateway (SLIRP 10.0.2.2) için ARP isteği yollar; SLIRP
ARP yanıtı verir; kernel RX ile alır + doğrular (ethertype 0x0806 + oper=2 + spa=10.0.2.2) →
"ARP REPLY OK". **İLK DENEMEDE.** Full gate GATE=0 (41 hedef). sıfır-uyarı. **OS 2-yönlü ağ: paket
gönder + al + ARP round-trip. Faz G derinleşti.**

**Sıradaki:** IP/UDP paketi (ping/DNS); ARP tablosu; UART RX; D2-x86. TÜM ROADMAP FAZLARI (C-G)
temel formda çalışıyor.

---

## D-144 — OS: VirtIO-Net paket gönderme — ağ TX (Faz G başlangıcı) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-143).

**Karar [ETKİ: yeni `runtime/kdl_virtio_net.c` (bare-metal virtio-net TX sürücüsü); yeni
`test/bare_metal/net_arm.c`; `Makefile` (bm_a64_virtio_net.o + QEMU netdev + filter-dump pcap gate).
Sadece ekleme — net driver yalnız net testine linklenir (BM_A64_OBJS'e DEĞİL, kimse referans etmiyor).]**
İlk AĞ yeteneği: gerçek Ethernet çerçevesi gönderme. Faz G açılışı.

**Sürücü:** virtio-blk (D-141) virtqueue makinesi yeniden kullanıldı; fark: DeviceID=1 (net), transmit
queue=1, buffer=virtio-net başlığı(12,sıfır)+çerçeve, tek desc (cihaz OKUR/TX). kdl_virtio_net_bul/
kur/gonder.

**Doğrulama (QEMU 11.0.1):** net_arm — broadcast Ethernet çerçevesi (ethertype 0x88b5, payload
"KEMGUNET-PAKET") gönder. QEMU `-netdev user -device virtio-net-device -object filter-dump,file=pcap`
→ paket pcap'e yakalandı. Gate: seri "NET GONDERILDI" + `grep -a "KEMGUNET-PAKET" net.pcap`. **İLK
DENEMEDE** (virtqueue makinesi taşındı). Full gate GATE=0 (40 hedef). sıfır-uyarı. **OS gerçek ağ
paketi gönderebiliyor — pcap ile kanıtlandı.**

**Sıradaki:** virtio-net RX (paket AL); ARP/IP/UDP stack (Faz G derinleşme); UART RX; D2-x86.

---

## D-143 — OS: KALICI dosya sistemi — disk-backed persistence (boot'lar arası) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-142).

**Karar [ETKİ: `runtime/kdl_kesme.c` (+kdl_dosya_kaydet/yukle disk serialize/deserialize +
kdl_dosya_olustur_deger/deger kernel helper'ları); yeni `test/bare_metal/kalici_arm.c`; `Makefile`
(iki-boot gate). virtio (aarch64) guard'lı. Sadece ekleme.]** RAM dosya sistemini (D-131) virtio-blk
diske (D-141/142) bağlar → dosyalar BOOT'LAR ARASI yaşar (gerçek kalıcılık).

**Mekanizma:** kdl_dosya_kaydet(base) — kdl_dosyalar tablosunu blok 0-1'e serialize (magic "KEMG" +
bytes). kdl_dosya_yukle(base) — blok 0-1 oku, magic varsa tabloyu geri yükle (-1 diskte FS yok).
Byte-kopya (aliasing yok). 2 blok (768 bayt tablo + 16 header < 1024).

**Doğrulama (QEMU 11.0.1):** kalici_arm — AYNI kernel AYNI diskle İKİ KEZ boot. Boot 1: FS yok →
"kalici"=777 oluştur+kaydet → "FIRST BOOT saved". Boot 2: magic var → yükle → "SECOND BOOT
kalici=777". **Dosya kernel yeniden başlayınca diskten geri geldi = GERÇEK KALICILIK.** Full gate
GATE=0 (39 hedef). sıfır-uyarı.

**Sıradaki:** FS'i syscall'la kaydet/yükle (userspace tetikler); UART RX (interaktif kabuk); D2-x86;
networking (Faz G).

---

## D-142 — OS: VirtIO-Blk yaz+oku round-trip — gerçek kalıcı depolama (C5 tamam) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-141).

**Karar [ETKİ: `runtime/kdl_virtio.c` (+kdl_virtio_blk_yaz); yeni `test/bare_metal/virtio_rw_arm.c`;
`Makefile`. Sadece ekleme.]** D-141 okumasını YAZMA ile tamamlar → çift-yönlü disk I/O = gerçek
kalıcılık.

**Mekanizma:** kdl_virtio_blk_yaz(base, sektor, kaynak) — okumadan farkı: type=1 (VIRTIO_BLK_T_OUT);
veri descriptor'ı cihaz-OKUR (WRITE flag YOK, cihaz veriyi diske yazar). Aynı virtqueue makinesi.

**Doğrulama (QEMU 11.0.1):** virtio_rw_arm — blok 7'ye "KEMGU-YAZDI-42" yaz → geri oku → eşleşme →
"DISK RW OK". Full gate GATE=0 (38 hedef). sıfır-uyarı. **C5 TAMAM: OS gerçek diske veri yazıp
okuyabiliyor (kalıcı depolama). virtio-blk sürücüsü: bul/kur/oku/yaz.**

**Sıradaki:** dosya sistemini disk-backed yap (RAM-FS'i blok'lara serialize); UART RX (interaktif
kabuk); D2-x86; networking (Faz G).

---

## D-140 — OS: userspace mesaj kanalı (IPC) — süreçler-arası mesajlaşma (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-139).

**Karar [ETKİ: `runtime/kdl_kesme.c` (global kdl_msg[] ring buffer + num 22 kanal_gonder, 23 kanal_al);
yeni `test/bare_metal/kanal_ipc_arm.c`; `Makefile`. Sadece ekleme.]** İki userspace süreç çekirdek-
aracılı mesaj kanalıyla DOĞRUDAN haberleşir (dosya-IPC ötesinde; KEMGU `kanal` ilkelinin userspace
düzeyi).

**Mekanizma:** global int ring buffer (16). num=22 kanal_gonder(deger) enqueue (dolu=-1); num=23
kanal_al() dequeue (boş=-1). Bloklamasız → alıcı EL0'da yoklar (deadlock yok).

**Doğrulama (QEMU 11.0.1):** kanal_ipc_arm — launcher(alıcı) spawn(sender); sender kanal_gonder
(100/200/300)+exit; launcher kanal_al ile 3 değer alıp toplar → "KANAL SUM=600". Full gate GATE=0
(36 hedef). sıfır-uyarı. **Userspace IPC iki yolla: dosya (paylaşılan depo) + kanal (mesaj geçişi).**

**Sıradaki:** UART RX (interaktif kabuk); D2-x86 (ring3); C5 virtio-blk (kalıcı disk).

---

## D-139 — OS: kabuğa aritmetik — topla komutu (shell hesap makinesi) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-138).

**Karar [ETKİ: `test/bare_metal/kabuk_arm.c` (+CMD_TOPLA + str_sayi (atoi) + branch + script satırı);
`Makefile` (gate "= 42"). Yalnız test.]** Kabuk artık sayı ayrıştırıp aritmetik yapıyor — FS komut
yorumlayıcısı + hesap makinesi.

**Mekanizma:** str_sayi (EL0 atoi, .user) + "topla A B" komutu → str_sayi(tok[1])+str_sayi(tok[2])
→ "= toplam". Kabuk metin→sayı ayrıştırma + hesap (userspace'de).

**Doğrulama (QEMU 11.0.1):** kabuk_arm — "SHELL> topla 12 30" / "= 42". Full gate GATE=0 (35 hedef).
sıfır-uyarı. **Kabuk komut seti: yaz/oku/ls/say/sil (FS CRUD) + topla (aritmetik).**

**Sıradaki:** UART RX (interaktif kabuk); D2-x86; C5 virtio-blk.

---

## D-138 — OS: kaynak geri-alma (slot reuse) — sınırsız spawn (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-137).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_preempt_gorev_olustur_el0 ölü görev-slotu reuse;
kdl_surec_spawn ölü havuz-slotu reuse + kdl_spawn_kullanildi/task[]); yeni
`test/bare_metal/geri_al_arm.c`; `Makefile`. Sadece ekleme/iyileştirme.]** Süreç bitince (exit) hem
scheduler görev-slotu hem spawn-havuz-slotu geri alınır → OS programları SINIRSIZ çalıştırabilir
(eski: monoton sayaç, 4 spawn'da tükeniyordu).

**Mekanizma:** kdl_preempt_gorev_olustur_el0 önce ÖLÜ (kdl_olu) görev slotu arar, yoksa yeni
(kdl_psayi++). kdl_surec_spawn boş VEYA görevi ölmüş havuz slotunu yeniden kullanır
(kdl_spawn_kullanildi[]+kdl_spawn_task[]). Güvenli: ölü görev scheduler'da atlanır + spawn eden
canlı görevden çağrılır (kdl_paktif != geri-alınan slot).

**Doğrulama (QEMU 11.0.1):** geri_al_arm — launcher 6× spawn+join (havuz=4'ten fazla) → "SPAWNS=6"
(hepsi başarılı; geri-alma olmasaydı 5.'te -1 → SPAWNS=4). Full gate GATE=0 (35 hedef). spawn/yasam/
multiproc/calis regresyon yeşil. sıfır-uyarı. **OS artık sınırsız süreç yaratıp bitirebilir.**

**Sıradaki:** UART RX (interaktif kabuk); D2-x86 (ring3 parite); C5 virtio-blk (kalıcı disk).

---

## D-137 — OS: program çalıştırma iş akışı — spawn→hesap→dosya→join→oku (uçtan-uca) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-136).

**Karar [ETKİ: yeni `test/bare_metal/calis_arm.c`; `Makefile`. Yalnız test — mevcut syscall'lar.]**
Süreç + FS + IPC yığınının uçtan-uca entegrasyonu: bir program başka bir programı çalıştırır, o
hesap yapıp sonucu dosyaya yazar, başlatan program sonucu geri okur ("bir programı çalıştır,
çıktısını al" — gerçek OS iş akışı).

**Akış:** launcher spawn(worker)→join; worker 1..10 topla(=55)→dosya_yaz("sonuc",55)→exit; launcher
dosya_oku("sonuc")→bas. Global FS worker çıktısını launcher'a taşır (süreçler-arası).

**Doğrulama (QEMU 11.0.1):** calis_arm — "CALIS BASLA" + "RESULT=55" (worker hesabı dosya üzerinden
launcher'a ulaştı). Full gate GATE=0 (34 hedef). sıfır-uyarı. **KEMGU-OS: kernel + izole userspace
süreçler + preemptive multitask + syscall ABI + RAM-FS + kabuk + program-çalıştır-çıktı-al akışı —
çalışan çok-programlı bir OS.**

**Sıradaki:** UART RX (interaktif kabuk); kaynak geri-alma (slot reuse); D2-x86; C5 virtio-blk.

---

## D-136 — OS: kabuk komut genişletme — say + sil (shell tam CRUD komut seti) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-135).

**Karar [ETKİ: `test/bare_metal/kabuk_arm.c` (+CMD_SAY/CMD_SIL + branch + script satırları);
`Makefile` (gate say/sil kontrolü). Yalnız test.]** D-135 kabuğunu tam CRUD komut setine genişletir.

**Kabuk komutları:** yaz/oku/ls (D-135) + say (dosya sayısı) + sil (dosya_sil). Script:
yaz gunluk → oku → ls → say(COUNT=1) → sil gunluk → say(COUNT=0). Silme öncesi/sonrası sayaç
(1→0) sil'in çalıştığını kanıtlar.

**Doğrulama (QEMU 11.0.1):** kabuk_arm — "SHELL> say"/"COUNT=1"/"SHELL> sil gunluk"/"SHELL> say"/
"COUNT=0". Full gate GATE=0 (33 hedef). sıfır-uyarı. **Userspace kabuk artık tam FS CRUD komut
seti yorumluyor (yaz/oku/ls/say/sil).**

**Sıradaki:** UART RX (interaktif kabuk); kaynak geri-alma; D2-x86; C5 virtio-blk (kalıcı disk).

---

## D-134 — OS: dosya sil — FS CRUD tamamlandı (oluştur/oku/güncelle/listele/sil) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-133).

**Karar [ETKİ: `runtime/kdl_kesme.c` (+num 21 dosya_sil; num 20 dosya_ad → kullanılan-index);
yeni `test/bare_metal/sil_arm.c`; `Makefile`. Sadece ekleme + dosya_ad iyileştirme.]** RAM dosya
sistemi artık tam CRUD.

**Mekanizma:** num=21 dosya_sil(ad=arg) → slot serbest (kullanildi=0). num=20 dosya_ad artık
KULLANILAN-index (raw değil) → silinmiş slotlar sıralamayı bozmaz (boşluk atlanır). D-133 ls
regresyonsuz (silme yoksa kullanılan==raw).

**Doğrulama (QEMU 11.0.1):** sil_arm — alfa+beta+gama oluştur → dosya_sil("beta") → listele →
"AFTER count=2" + alfa + gama (beta YOK). Full gate GATE=0 (32 hedef). D-133 ls regresyon yeşil.
sıfır-uyarı. **RAM-FS tam CRUD: oluştur(15)/oku(16)/metin(17,18)/listele(19,20)/sil(21).**

**Sıradaki:** scripted userspace kabuk (komut dizisi → FS işlemleri); UART RX (klavye, gate zor);
kaynak geri-alma; D2-x86; C5 virtio-blk (kalıcı disk).

---

## D-133 — OS: dosya listeleme (ls) — userspace dosya enumerasyonu (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-132).

**Karar [ETKİ: `runtime/kdl_kesme.c` (+num 19 dosya_sayisi, 20 dosya_ad); yeni
`test/bare_metal/ls_arm.c`; `Makefile`. Sadece ekleme.]** D-131/132 dosya sistemi üstünde ilk
"shell primitifi": userspace program dosya deposunu enumere eder (ls).

**Mekanizma:** num=19 dosya_sayisi() → kullanımdaki dosya sayısı. num=20 dosya_ad(idx=arg, buf=arg2)
→ idx'inci dosyanın adını user tamponuna kopyala. Userspace program dosya_sayisi() kez döngüyle
her adı okuyup basar (ls).

**Doğrulama (QEMU 11.0.1):** ls_arm — launcher dosya_yaz("alfa",1)+dosya_yaz("beta",2) → listele →
"LS count=2" + "  alfa" + "  beta". Full gate GATE=0 (31 hedef). sıfır-uyarı. **Userspace ABI 20+
syscall: process (spawn/exit/durum/getpid) + zaman (gettick) + I/O (yaz*) + dosya (yaz/oku/metin/
sayisi/ad). Basit bir kabuk (shell) yazmaya yetecek temel.**

**Sıradaki:** basit userspace kabuk (komut → dosya işlemi); dosya sil; kaynak geri-alma; D2-x86; C5.

---

## D-132 — OS: metin içerikli dosya — bulk read/write (kernel↔user bellek kopyası) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-131).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_dosyalar +icerik[64]+boyut; +num 17 dosya_yaz_metin,
18 dosya_oku_metin); yeni `test/bare_metal/metin_arm.c`; `Makefile`. Sadece ekleme.]** D-131 tek-değer
dosyasını GERÇEK byte-içeriğe genişletir — kernel↔userspace çift-yönlü bellek kopyası (gerçek
read/write syscall ailesinin temeli).

**Mekanizma:** num=17 dosya_yaz_metin(ad=arg, str=arg2) — kernel kullanıcı belleğinden (arg2) string'i
dosya içeriğine kopyalar (yazılan byte döner). num=18 dosya_oku_metin(ad=arg, buf=arg2) — dosya
içeriğini kullanıcı tamponuna (arg2) kopyalar (okunan byte döner). Kernel EL1'den AP=01 user
sayfasını okur/yazar (buf worker'ın veri sayfasında). 2-arg syscall (D-131).

**Doğrulama (QEMU 11.0.1):** metin_arm — launcher dosya_yaz_metin("mesaj","MERHABA DOSYA")+spawn;
worker dosya_oku_metin ile metni kendi tamponuna okur+basar → "FILE TEXT: MERHABA DOSYA" (dosya
metin içeriği süreçler-arası aktarıldı). Full gate GATE=0 (30 hedef). D-131 regresyon yeşil. sıfır-uyarı.

**Sıradaki:** dosya offset'li read/write (kısmi); dizin/listeleme; kaynak geri-alma; D2-x86; C5
virtio-blk (RAM-FS'i kalıcı disk'e).

---

## D-131 — OS: RAM dosya sistemi + 2-argümanlı syscall (Faz E ilk adım) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-130).

**Karar [ETKİ: `boot/start_aarch64.S` (SVC path arg2=saklanan-x1 geçirir); `runtime/kdl_kesme.c`
(kdl_syscall_isle 3-param (num,arg,arg2) + RAM dosya deposu + num 15 dosya_yaz, 16 dosya_oku); yeni
`test/bare_metal/dosya_arm.c`; `Makefile`. x86 stub değişmedi (nums 1/2/3 arg2 kullanmaz — gate'te
doğrulandı). linker/host/codegen dokunulmadı.]** İki yenilik: 2-argümanlı syscall ABI + çekirdek-
aracılı isimli depolama (Faz E dosya sisteminin ilk adımı, virtio-blk GEREKTİRMEZ).

**2-arg syscall:** D-126 x1-koruma (register-şeffaflık) bunu mümkün kıldı; şimdi SVC path saklanan-x1'i
3. C param (arg2) olarak geçirir. `ldr x2,[sp,#8]` eklendi. dosya_yaz(ad, değer) gibi 2-arg syscall'lar.

**RAM dosya deposu:** kdl_dosyalar[8] (ad[16]+deger+kullanildi); kdl_dosya_ac (bul/oluştur) + kdl_dosya_bul
+ kdl_ad_esit (freestanding strcmp). num=15 dosya_yaz(ad=arg, deger=arg2); num=16 dosya_oku(ad=arg)→değer.
Süreçler-arası paylaşılır (çekirdek durumu).

**Doğrulama (QEMU 11.0.1):** dosya_arm — launcher dosya_yaz("sayac",1234)+spawn(worker); worker
dosya_oku("sayac")=1234 → "FILE OK deger=1234" (BAŞKA süreç, launcher'ın yazdığını okudu). Full gate
GATE=0 (29 hedef). x86 syscall + tüm SVC regresyon (syscall_arg/ret/d2/userspace) yeşil. sıfır-uyarı.
**Userspace ABI: yaz/yaz_sayi/satir/cik/artir/gettick/getpid/spawn/exit/durum/dosya_yaz/dosya_oku.**

**Sıradaki:** dosya read/write byte-buffer (tek-değer değil); kaynak geri-alma; D2-x86; C5 virtio-blk
(gerçek disk → RAM-FS'i kalıcı yap).

---

## D-130 — OS: süreç yaşam döngüsü — exit + join (spawn→çalış→exit→join tam döngü) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-129).

**Karar [ETKİ: `runtime/kdl_gorev.c` (+kdl_olu[] state + kdl_gorev_bitir/kdl_gorev_durum +
kdl_preempt'te ölü-görev atla); `runtime/kdl_kesme.c` (+num 13 exit, 14 durum); yeni
`test/bare_metal/yasam_arm.c`; `Makefile`. x86/host/codegen dokunulmadı (exit/durum arch-generic).]**
D-129 spawn'ı tam yaşam döngüsüne tamamlar: süreç bitişi + ebeveyn join.

**Mekanizma:** kdl_olu[görev] (1=bitmiş). num=13 exit → kdl_gorev_bitir() (kdl_olu[kdl_paktif]=1);
kdl_preempt ölü görevi ATLAR (bloklu gibi) → süreç bir daha koşmaz. num=14 durum(pid) →
kdl_gorev_durum(pid) (bitti mi?). **Bloklamalı join YERİNE EL0-yoklama:** ebeveyn preemptive
olduğundan `while(!durum(pid))` yoklarken çocuk koşar→exit eder→durum=1 (deadlock yok; blocking-in-
syscall / IRQ-masked sorununu bypass eder). Kaynak geri-alma v1'de yok (havuz slotu serbest değil).

**Doğrulama (QEMU 11.0.1):** yasam_arm — launcher spawn(worker)→worker iş+exit→launcher join
(durum yokla)→"WORKER done"+"JOINED worker exited". Full gate GATE=0 (28 hedef). Scheduler
regresyon (kdl_olu skip additive) yeşil. sıfır-uyarı. **Tam süreç yaşam döngüsü: yarat→koş→bitir→
bekle. Userspace ABI: yaz/yaz_sayi/satir/cik/artir/gettick/getpid/spawn/exit/durum.**

**Sıradaki:** read/write dosya syscall'ları (C5 storage sonrası); kaynak geri-alma (exit'te havuz
free); D2-x86; C5 virtio-blk → Faz E fs.

---

## D-129 — OS: dinamik süreç oluşturma — spawn syscall'ı (fork/spawn yeteneği) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-128).

**Karar [ETKİ: `runtime/kdl_gorev.c` (+kdl_surec_spawn — havuz-tabanlı runtime süreç); `runtime/
kdl_kesme.c` (kdl_syscall_isle +num 12 spawn, #if __aarch64__); yeni `test/bare_metal/spawn_arm.c`;
`Makefile`. x86/host/codegen/linker dokunulmadı.]** D-127 (statik çok-süreç) → runtime dinamik
süreç: bir userspace süreç RUNTIME'da yeni izole süreç yaratır (gerçek OS fork/spawn).

**Mekanizma:** kdl_surec_spawn(entry) — havuzdan (KDL_SPAWN_MAX=4) L1/L2 tabloları + kernel yığını
+ sürece-özel veri PA'sı (0x46000000+i*2MB, RAM içi) alır → kdl_surec_kur_el0_veri (paylaşılan kod
+ özel veri) → kdl_preempt_gorev_olustur_el0 (preemptive EL0 görev, entry'de) → kdl_task_l1[t]=yeni
tablo. num=12 spawn(entry_va) syscall bunu çağırır, yeni pid döner. Syscall IRQ-masked → kdl_psayi++
scheduler ile yarışmaz (güvenli); yeni görev eret sonrası ilk timer-IRQ'da schedulable.

**Doğrulama (QEMU 11.0.1):** spawn_arm — launcher (EL0 süreç, kendi TTBR) spawn(worker) çağırır →
"LAUNCHER spawned pid=2"; worker DİNAMİK yaratılan izole adres-uzayında EL0'da koşar → "WORKER OK".
Full gate GATE=0 (27 hedef). sıfır-uyarı. **Programlar artık yeni süreç başlatabiliyor —
gerçek çok-görevli OS'un temel yeteneği.**

**Sıradaki:** süreç bitişi/join (worker exit → launcher öğrenir); read/write dosya syscall'ları;
D2-x86; C5 virtio-blk → Faz E fs.

---

## D-128 — OS: userspace introspection syscall'ları — gettick + getpid (userspace ABI genişleme) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-127).

**Karar [ETKİ: `runtime/kdl_zaman.c` (+kdl_tik_al getter); `runtime/kdl_gorev.c` (+kdl_aktif_gorev
getter); `runtime/kdl_kesme.c` (kdl_syscall_isle +num 10 gettick, 11 getpid); yeni
`test/bare_metal/tick_arm.c`; `Makefile`. x86/host/codegen dokunulmadı (getter'lar iki arch'ta;
x86 syscall kernel gate'te doğrulandı).]** D-126 dönüş-değeri ABI'si üstünde ilk gerçek "kernel
durumu okuyan" userspace syscall'ları.

**Yeni syscall'lar (dönüş-değerli):** num=10 gettick → kdl_tik_al() (timer tik sayısı, userspace
zamanı okur); num=11 getpid → kdl_aktif_gorev() (o an koşan preemptive görev id'si). Getter'lar:
kdl_tik_al (kdl_zaman.c, static kdl_tik_sayisi'ni açar), kdl_aktif_gorev (kdl_gorev.c, kdl_paktif).

**Doğrulama (QEMU 11.0.1):** tick_arm — preemptive EL0 görev gettick(t1)→zaman-geçir→gettick(t2)→
getpid; t2>t1 (timer preemptive görevde IRQ-açık → tikler) + pid=1 → "TICK OK pid=1". Full gate
GATE=0 (26 hedef). sıfır-uyarı. **Userspace artık çekirdek durumunu (zaman/kimlik) syscall ile
okuyabiliyor — gerçek programların temel ihtiyacı.**

**Sıradaki:** read/write dosya syscall'ları (C5 storage sonrası); dinamik süreç spawn; D2-x86; C5
virtio-blk → Faz E fs.

---

## D-127 — OS: çoklu EL0 süreç — izole userspace multitasking (gerçek multi-process OS DORUĞU) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-126).

**Karar [ETKİ: `runtime/kdl_mmu.c` (+kdl_surec_kur_el0_veri — paylaşılan kod + özel veri sayfası);
`runtime/kdl_gorev.c` (+kdl_task_l1[] + kdl_preempt_gorev_ttbr + kdl_preempt'te guard'lı TTBR-swap);
yeni `test/bare_metal/multiproc_arm.c`; `Makefile`. x86/host/codegen/linker dokunulmadı.]** D3
(per-process TTBR) ⊕ D-125 (preemptive EL0) birleşimi: BİRDEN ÇOK userspace süreç, her biri KENDİ
izole adres-uzayında, preemptively multitask.

**Mekanizma:**
- **kdl_surec_kur_el0_veri(l1,l2,kod_pa,veri_pa):** L2[16] (0x42000000) → kod_pa (PAYLAŞILAN .user
  kod, tüm süreçlerde aynı, AP=01); L2[17] (0x42200000) → veri_pa (SÜRECE-ÖZEL, AP=01); kernel
  identity her tabloda (swap güvenli). Paylaşılan-kod/özel-veri deseni (klasik OS).
- **Scheduler TTBR-swap:** kdl_task_l1[görev] (kdl_preempt_gorev_ttbr ile set). kdl_preempt seçilen
  göreve geçerken `if (kdl_task_l1[en_iyi]) kdl_ttbr_degis(...)` → o sürecin adres-uzayına çevir.
  **GUARD'LI:** set edilmemişse (mevcut EL1 testleri) swap YOK → regresyon YOK (6 scheduler testi
  doğrulandı). `#if defined(__aarch64__)` (x86 cooperative-only).

**İZOLASYON KANITI (multiproc_arm):** A markörü 0xAA'yı bir kez yazar, sonra 40000-iter döngüde
sürekli 0xAA doğrular; B simetrik 0xBB. İkisi AYNI VA'yı (0x42200000) kullanır ama FARKLI PA
(A→0x44000000, B→0x46000000). Timer-IRQ defalarca aralarında geçer; izole olduğundan A hep 0xAA
(B'nin 0xBB'si A'nın PA'sına DOKUNMAZ) → "A OK" + "B OK". Paylaşsalardı çapraz-bozulma → "CORRUPT".

**Doğrulama (QEMU 11.0.1):** "MULTIPROC BASLA" + "A OK" + "B OK". Full gate GATE=0 (25 hedef).
sıfır-uyarı. **Process modeli TAM DORUK: kernel + N izole userspace süreç, her biri kendi
adres-uzayında, preemptively multitask + syscall (arg+dönüş+çok-arg) + bellek-koruması.**

**Sıradaki:** userspace ABI genişletme (gettick/getpid/read); dinamik süreç oluşturma (fork-benzeri);
D2-x86 (ring3+TSS); C5 (virtio-blk → Faz E dosya sistemi).

---

## D-126 — OS: syscall dönüş-değeri ABI + kdl_exc_ortak register-şeffaflık onarımı (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-125).

**Karar [ETKİ: `boot/start_aarch64.S` (kdl_exc_ortak+kdl_svc_ortak → tek "frame-önce-kaydet"
işleyici; str x0 dönüş); `runtime/kdl_kesme.c` (kdl_syscall_isle → uint64_t; +num 9 artir);
yeni `test/bare_metal/syscall_ret_arm.c`; `Makefile`. x86/host/codegen dokunulmadı.]** Userspace
ABI'nin eksik yarısı (syscall DEĞER döndürür) + bunu yaparken keşfedilen gerçek register-şeffaflık
bug'ının onarımı.

**Dönüş-değeri ABI:** kdl_svc_ortak `bl kdl_syscall_isle` sonrası `str x0, [saved-x0]` → restore
ile EL0 çağıran x0'da sonucu alır. kdl_syscall_isle artık uint64_t döner (num=9 'artir': arg+1).

**KEŞFEDİLEN + ONARILAN BUG (register-şeffaflık):** Eski kdl_exc_ortak, frame kaydetmeden ÖNCE
`lsr x9, x1, #26` (EC) + `mrs x1/x2/x3` ile çağıranın x1/x2/x3/x9'unu klobber ediyordu; SVC için
EC=0x15 → x9=0x15. syscall_ret testi (dönüş-değerine bağlı dallı string-ptr'yi x9'da tutan)
BUNU tetikledi: OK-string ptr'si (0x40003570) x9'da → syscall sonrası x9=0x15 → sys(yaz, 0x15) →
çöp → hiçbir şey basılmadı. Empirik teşhis: QEMU `-d in_asm,cpu` (x9: 0x40003570 → 0x15 svc'de).
**ONARIM:** işleyici FRAME'İ ÖNCE kaydeder, SONRA ESR okur/dispatch eder → tüm çağıran register'ları
korunur (yalnız x0=dönüş değişir). num/arg saklanandan okunur. **Çok-argümanlı syscall'ları da
mümkün kıldı (x1+ artık korunuyor — eski bug x1'i eziyordu).**

**Doğrulama (QEMU 11.0.1):** syscall_ret "SYSCALL RET OK" (41→42 EL0'a döndü). Tüm SVC/fault
regresyon yeşil: syscall/syscall_arg/istisna(fault)/d2(EL0+SVC)/proc(D3)/userspace. Full gate GATE=0
(24 hedef). sıfır-uyarı. **Öğrenilen: `-MMD -MP` header-dep var ama .c değişince .o rebuild
timing — rm+make ile force gerekebildi (staleness teşhisi).**

**Sıradaki:** çoklu EL0 süreç (scheduler TTBR-swap); gettick/getpid/read syscall'ları (artık dönüş +
çok-arg hazır); D2-x86; C5 (virtio-blk).

---

## D-125 — OS: preemptive EL0 (userspace) görev — userspace multitasking (process modeli tamam) (2026-07-01) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-124).

**Karar [ETKİ: `boot/start_aarch64.S` (kdl_irq_ortak SP_EL0 save/restore @264 — Stage 1);
`runtime/kdl_gorev.c` (+kdl_preempt_gorev_olustur_el0 — Stage 2); `linker/bare-metal-aarch64.ld`
(.user output'a .user_data eklendi); yeni `test/bare_metal/preempt_el0_arm.c`; `Makefile`.
x86/host/codegen dokunulmadı.]** Process modelinin son parçası: userspace (EL0) görevler
PREEMPTIVELY multitask edilir.

**Stage 1 (non-regressing):** kdl_irq_ortak trap-frame'e SP_EL0'ı @264 ekler (mrs/msr sp_el0).
EL1 görevlerde SP_EL0 kullanılmaz → zararsız; 6 EL1 preemptive testi (preempt/sleep/priority/kanal/
sched/timer) hâlâ yeşil (doğrulandı).

**Stage 2:** kdl_preempt_gorev_olustur_el0(giris, kernel_yigin, user_yigin) — sentetik trap-frame
SPSR=0x0 (EL0t, IRQ-açık) + SP_EL0=user yığını. İlk switch eret ile EL0'a atlar; timer-IRQ EL0'dan
EL1'e alır, kdl_irq_ortak SP_EL0 dâhil tüm bağlamı kaydeder → EL0 görev preempt edilip sürdürülür.
İKİ yığın: kernel (trap-frame/SP_EL1, AP=00) + user (SP_EL0, .user AP=01).

**Linker:** .user output section artık .user_data (EL0-yazılabilir veri) de toplar — kod (.user, X)
ve veri (.user_data, W) ayrı input-section → derleyici section-tip çakışması yok, ikisi de aynı
0x42000000 AP=01 sayfasında. (İleride process code/data ayrımı temeli.)

**Doğrulama (QEMU 11.0.1):** "PREEMPT EL0 BASLA" + "PREEMPT EL0 OK" (el0_sayac>0 = EL0 userspace
görev timer-IRQ ile preempt edilerek koştu, main EL1 de koştu). Full gate GATE=0. sıfır-uyarı.

**Önem:** Process modeli ARTIK TAM — kernel(EL1) + userspace(EL0) görevler timer-IRQ ile
preemptively dönüşümlü koşar, banked SP_EL0 korunur. Gerçek OS multitasking'inin çekirdeği.

**Sıradaki:** çoklu EL0 süreç + per-process TTBR swap scheduler'da (D3+D-125 birleşik); userspace
ABI (exit-kod/oku); D2-x86 (ring3+TSS); C5 (virtio-blk → Faz E fs).

---

## D-124 — OS: ilk userspace programı — EL0 hesap + syscall ABI ile I/O (Faz F temeli) (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-123).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_syscall_isle +num 5/6/7 = yaz/yaz_sayi/satir; +kdl_yaz_metin/
kdl_yaz_tam decl); yeni `test/bare_metal/userspace_arm.c`; `Makefile`. x86/host/codegen dokunulmadı
(syscall_isle paylaşılan — x86 yaz_* referansı gate'te doğrulandı).]** D3'ün (korumalı süreç) üstüne
userspace ABI'nin ilk gerçek kullanımı: bir userspace programı EL0'da HESAP yapar + kernel
hizmetlerini SYSCALL ile kullanır (Faz F userspace temeli).

**Userspace syscall ABI (v0):** num=5 yaz(ptr) — string yaz (kernel kullanıcı belleğinden ptr OKUR —
pointer/veri geçişi ABI'si); num=6 yaz_sayi(n); num=7 satir; num=3 cik (bitir/dur). Sarmalayıcı
`always_inline` → SVC .user section'a gömülü (ayrı fonksiyon .text/AP=00'da kalır, EL0
çalıştıramaz). String literalleri .rodata'da (kernel EL1 okur; EL0 yalnız adres geçer, dereference
etmez → AP=00 sorunu yok).

**Doğrulama (QEMU 11.0.1):** "MERHABA userspace" + "USERSPACE OK toplam=55" — EL0 program 1..10
topladı (userspace hesap) + syscall I/O ile yazdı. Full gate GATE=0 (23 hedef). sıfır-uyarı.

**Önem:** userspace program artık HESAP + I/O yapabiliyor (syscall ABI ile) — gerçek program
çalıştırmanın (Faz F) çekirdek yapıtaşı. Kernel kullanıcı pointer'ından veri okuyor (read/write
syscall ailesinin temeli). NOT: ptr doğrulaması yok (gerçek OS'te user-adres-uzayı kontrolü gerek).

**Sıradaki:** preemptive EL0 süreç (per-task kernel stack); userspace ABI genişletme (oku/exit-kod);
D2-x86 (ring3+TSS); C5 (virtio-blk → Faz E fs).

---

## D-123 — OS D3: korumalı EL0 user-process (D1⊕D2⊕D-122 birleşik) — gerçek OS sürecinin dört özelliği bir arada (2026-07-01)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-122).

**Karar [ETKİ: `runtime/kdl_mmu.c` (+kdl_surec_kur_el0 — user sayfası AP=01); yeni
`test/bare_metal/proc_arm.c`; `Makefile`. Mevcut kod DEĞİŞMEDİ (yalnız ekleme). x86/host/codegen
dokunulmadı.]** D-121/D-122 ön-koşulları (preemption-x0 + syscall-arg) hazır olunca, gerçek bir
işletim sistemi sürecinin dört tanımlayıcı özelliğini BİR ARADA gösteren keystone.

**Birleşen özellikler (tek süreçte):**
1. **Kendi adres-uzayı** — süreç kendi L1/L2 tablolarına sahip (kernel global tablolarından ayrı),
   `kdl_ttbr_degis` ile TTBR0 swap (D1 makinesi).
2. **Kullanıcı ayrıcalığı** — kod EL0'da, kendi TTBR'ı altında (`kdl_el0_calistir`, D2).
3. **Syscall arayüzü** — EL0 kod SVC ile argüman geçer (num=4 arg=42) → "SYSCALL ARG OK" (D-122).
4. **Bellek koruması / HAPİS** — süreç kernel-only sayfaya (0x40000000, AP=00) erişince EL0
   **permission-fault** → kernel yakalar. Kendi adres-uzayına hapsedilmiş.

**Yeni API:** `kdl_surec_kur_el0(l1,l2,user_pa)` — kdl_surec_kur (D1, AP=00) gibi ama user sayfası
`| (1<<6)` (AP=01, EL0+EL1 RW; UXN=0 → EL0-exec). user_pa=0x42000000 (identity — .user section
fiziksel yeri). el0_kod `.user` section'da, self-contained pure-SVC.

**Doğrulama (QEMU 11.0.1):** "PROC BASLA (EL1)" + "SYSCALL ARG OK" + "ISTISNA tip=0x24
a=0x9200000e b=0x42000010 adr=0x40000000". ESR decode: EC=0x24 (data abort, lower-EL/EL0),
**DFSC=0x0E = PERMISSION fault** (sayfa VAR ama EL0 reddedildi → gerçek koruma, translation değil),
ELR=0x42000010 (fault eden EL0 komutu), FAR=0x40000000 (erişilmeye çalışılan kernel adresi). Full
gate GATE=0 (22 hedef). sıfır-uyarı, libc-temiz.

**Sınır (bilinçli):** süreç henüz PREEMPTIVE değil (SPSR=EL0t DAIF-masked → timer maskeli). Preemptive
EL0 süreç = per-task KERNEL stack (trap-frame SP_EL1 ≠ run SP_EL0) + SP_EL0 trap-frame'de kaydet →
ayrı milestone. İzolasyon (private DATA) = ayrı .user_code/.user_data sayfaları (shared-code+
private-data) → follow-up.

**Sıradaki:** preemptive EL0 süreç (per-task kernel stack); D2-x86 (ring3+TSS); C5 (virtio-blk).

---

## D-122 — OS: SVC arg0 (x0) vektör-stub tarafından eziliyordu — syscall argüman geçişi onarımı (2026-06-30) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-121).

**Karar [ETKİ: `boot/start_aarch64.S` (VEKTOR_EXC macro — `mov x0,#\tip` kaldırıldı; kdl_exc_ortak
fault-yolu `mov x0,x9` ile EC'yi tip yapar); `runtime/kdl_kesme.c` (kdl_syscall_isle num=4 arg
kontrolü); yeni `test/bare_metal/syscall_arg_arm.c`; `Makefile`. x86/host/codegen DEĞİŞMEDİ.]**
D-121'de keşfedilen ikincil latent bug'ın onarımı — IRQ ile aynı sınıf, EXC yolunda.

**KÖK-NEDEN:** `VEKTOR_EXC \tip` → `mov x0,#\tip ; b kdl_exc_ortak`. Same-EL SVC slot 4 →
`mov x0,#4`, kdl_svc_ortak x0'ı (syscall arg0) kaydetmeden ÖNCE 4 ile eziyordu → her syscall'ın
arg0'ı sessizce vektör-indeksine (4) dönüşüyordu. Latentti (mevcut syscall testleri arg0
kontrol etmiyordu) ama userspace syscall'ları arg geçince bozulurdu.

**ONARIM:** VEKTOR_EXC artık doğrudan `b kdl_exc_ortak` (x0 dokunulmaz). SVC dalında x0=arg0
korunur → kdl_svc_ortak doğru kaydeder/geçer. Fault dalında (b.eq kdl_svc_ortak alınmazsa)
`mov x0, x9` ile tip = ESR.EC (teşhis; x0 artık arg0 değil, fault noreturn). istisna gösterimi
"tip=0x<vektör>" yerine "tip=0x<EC>" (daha bilgilendirici; test "ISTISNA" arar, etkilenmez).

**Doğrulama (QEMU 11.0.1):** syscall_arg — SVC num=4 arg=42 → kernel arg==42 görür →
"SYSCALL ARG OK" (+ "SYSCALL ARG SONRA" = eret kurtarma çalışıyor). Onarımdan önce arg=4 →
"HATA" olurdu. Regresyon yeşil: syscall (AFTER SYSCALL) + istisna (EC display) + d2 (EL0 SVC
kaynak-EL). sıfır-uyarı. **Vektör-stub register-bozma bug sınıfı (IRQ D-121 + EXC D-122) artık
tamamen kapalı** — preemption x0 + syscall arg0 ikisi de güvenli.

**Sıradaki:** D1+D2+C7 birleşik gerçek EL0 user-process (artık syscall-arg + x0-koruma hazır);
D2-x86; C5 (virtio-blk).

---

## D-121 — OS: IRQ vektör stub'u preempt edilen görevin x0'ını bozuyordu (KÖK-NEDEN onarımı) + C7d cap=4 IPC restore (2026-06-30) [YÜKSEK]

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-120).

**Karar [ETKİ: `boot/start_aarch64.S` (VEKTOR_IRQ macro — `mov x0,#\tip` kaldırıldı);
`runtime/kdl_kanal.{c,h}` + `test/bare_metal/kanal_arm.c` (KAP 16→4 restore + yorum). x86/host/
codegen DEĞİŞMEDİ.]** D-119'da bayraklı cap=4 "deterministik bozulma" — çok-ajanlı adversarial
workflow (5 bağımsız lens + sentez, gdb HW-watchpoint + QEMU `-d in_asm,cpu` trace ile) KÖK-NEDENİ
EMPİRİK buldu.

**KÖK-NEDEN (gerçek + ciddi, latent scheduler bug):** aarch64 IRQ vektör stub'u
`VEKTOR_IRQ \tip` → `mov x0,#\tip ; b kdl_irq_ortak`. Timer her zaman slot 5 (Cur-EL-SPx IRQ) →
`mov x0,#0x5`. Bu, **trap-frame KAYDEDİLMEDEN ÖNCE** preempt edilen görevin CANLI x0'ını 5 ile
eziyor. kdl_irq_ortak x0'ı [sp,#0]'a kaydedince bozuk x0=5 frame'e yazılıyor; eret'te görev x0=5
ile sürüyor. kdl_irq_ortak tip'i HİÇ kullanmaz (`mov x0,sp` ile ezer) → `mov x0,#\tip` saf
tahripti. "5 = 5. değer" RASTLANTI (5 = vektör indeksi). Global kanal ASLA bozulmadı (yanlış
teşhisti). cap=4 spesifik: yalnız o, ÜRETİCİYİ `gonder` spin'inde (pointer x0'da canlı, reload
yok) preempt eder; tüketici `al` pointer'ı x8'de tutar → bağışık. **preempt/sleep/priority sadece
şanstan geçmişti** (preempt sonrası x0-deref-reloadsız desen yoktu).

**ONARIM:** VEKTOR_IRQ artık doğrudan `b kdl_irq_ortak` (x0 dokunulmaz → görev x0'ı bozulmadan
kaydedilir/geri yüklenir). Cerrahi, EXC yolu değişmedi. cap=4 IPC restore edildi (D-119 cap=16
work-around kaldırıldı) → çift-yönlü back-pressure ping-pong artık sağlam.

**Doğrulama (QEMU 11.0.1):** kanal cap=4 → "KANAL OK toplam=55" (eski: Data Abort). TÜM aarch64
regresyon yeşil: preempt/sleep/priority/sched + istisna(EXC) + timer(IRQ) + syscall(SVC) + d2(EL0)
+ d1. sıfır-uyarı.

**KEŞFEDİLEN İKİNCİL LATENT BUG (ayrı iş):** Aynı sınıf EXC yolunda — `VEKTOR_EXC 4` → `mov x0,#4`
SVC arg0'ı (x0) kdl_svc_ortak kaydetmeden önce yok ediyor. Şu an latent (syscall testleri x0-arg
kontrol etmiyor) ama userspace syscall'lar arg geçince ısıracak. Takip: EXC vektörlerini de
register-bozmaz yap + kdl_exc_ortak tip'i ESR EC'den türetsin (istisna gösterimi + kdl_istisna_isle
imza dokunuşu → ayrı commit + arg-geçen syscall testi).

**Sıradaki:** SVC arg0 onarımı (yukarıda); D1+D2+C7 birleşik gerçek EL0 user-process; D2-x86.

---

## D-120 — OS C7e: öncelikli (priority) scheduling — strict priority + round-robin koruma (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-119).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_pri[] + kdl_preempt_oncelik + kdl_preempt seçim
mantığı); yeni `test/bare_metal/priority_arm.c`; `Makefile`. x86/host/codegen + trap-frame
asm DEĞİŞMEDİ.]** FAZ C: öncelikli zamanlama — scheduler en yüksek öncelikli READY görevi
seçer (eşit öncelikte round-robin korunur). Gerçek-zaman/QoS temeli.

**Mekanizma:** kdl_pri[] (büyük=yüksek, varsayılan 0). kdl_preempt round-robin sırada
(kdl_paktif sonrası) tarar, en yüksek öncelikli READY'yi seçer; eşit öncelikte ilk-bulunan
(round-robin döner) kazanır. Tümü-eşit (pri=0) → eski round-robin ile BİREBİR aynı (regresyon
yok). kdl_preempt_oncelik(gorev, pri) ile atanır.

**Doğrulama (QEMU 11.0.1):** priority_arm — main yüksek (1), B düşük (0). Faz1: main meşgul-
döner, timer tikler ama main tekelde → B aç kalır (b_sayac=0). Faz2: main kdl_uyu(10) bloklanır
→ tek READY=B → B koşar (b_sayac>0). b1==0 && b2>0 → "PRIORITY OK ac-faz1=0". **Tüm scheduler
regresyon yeşil:** sched(coop) + preempt + sleep + kanal bozulmadı (eşit-öncelik round-robin
korundu). sıfır-uyarı; libc-temiz; test_tumu host-nötr.

**Sıradaki:** D1+D2+C7 birleşik gerçek user-process; D2-x86 (ring3+TSS); C5 (virtio-blk);
cap=4 IPC corner-case GDB-teşhisi (D-119).

---

## D-119 — OS C7d: kanal (SPSC IPC) — preemptive scheduler üstünde görevler-arası mesaj (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-118).

**Karar [ETKİ: yeni `runtime/kdl_kanal.h` + `runtime/kdl_kanal.c`; yeni
`test/bare_metal/kanal_arm.c`; `Makefile` (bm_a64_kanal.o kuralı + calistir_kanal_test_arm +
os_kernels gate). x86/host/codegen + scheduler runtime DEĞİŞMEDİ.]** FAZ C: KEMGU `kanal`
ilkelinin (DRF V1 — R-KANAL aksiyomu, `görev`/`kanal` keyword'leri) çekirdek-düzeyi karşılığı.
SPSC halka tampon + preemptive scheduler + bloklamalı-alım birlikte çalışır → görevler-arası
mesaj geçişi (IPC) kanıtı.

**Mekanizma:** `KdlKanal` opak SPSC halka tampon (volatile buf/bas/son, tek slot rezerve →
KAP-1 öğe). `kdl_kanal_gonder/al` busy-wait bloklar (dolu/boş iken döngü); preemptive scheduler
(C7b) timer-IRQ'da karşı göreve geçirir → ilerleme garanti (tek çekirdek, kilitlenme yok).
Tek-çekirdek → bellek-bariyeri gerekmez (SMP'de DMB eklenir).

**Doğrulama (QEMU 11.0.1):** kanal_arm — üretici görev 1..10 yollar, tüketici (main) boş-kanalda
bloklanıp uyanarak 10 değeri FIFO sırayla alır+toplar → "KANAL OK toplam=55" (libc-temiz).
Scheduler regresyon: preempt (PREEMPT OK) + sleep (B WOKE) bozulmadı. test_tumu host-nötr
(kdl_kanal yalnız bare-metal'de derlenir). sıfır-uyarı.

**KISIT (dürüst kayıt):** Üretici-tarafı dolu-bloklama (back-pressure) çok küçük kapasite (KAP=4)
ile hızlı ping-pong preemption altında DETERMINISTIK bir durum bozulmasına yol açtı (kanal global
0x40004000 → 5; FAR=0x19; üretici `gonder` içinde dolu-bloklarken). Yığın-bitişikliği DEĞİL
(16KB ayrık yığınla aynı semptom); IRQ trap-frame dengeli (sub/add #272), yığın taşması yok →
kök-neden açık, GDB-düzeyi ayrı oturuma ertelendi. Şimdiki demo KAP=16 (üretici tek planlama-
diliminde tüm öğeleri yollar, dolu-bloklamaz); tüketici boş-bloklama bu kısıttan etkilenmez →
milestone sağlam + doğrulanmış. Takip: cap=4 ping-pong corner-case'i GDB ile incele.

**Sıradaki:** öncelikli scheduling; D1+D2+C7 birleşik gerçek user-process; D2-x86; C5 (virtio-blk);
cap=4 IPC corner-case GDB-teşhisi.

---

## D-118 — OS C7c: blocking scheduler (sleep/wake) — preemptive üstüne (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-117).

**Karar [ETKİ: `runtime/kdl_gorev.c` (kdl_block[] + kdl_uyu + kdl_preempt blocking dalı); yeni
`test/bare_metal/sleep_arm.c`; `Makefile`. x86/host/codegen DEĞİŞMEDİ.]** FAZ C: blocking
(sleep/wake) — görev N tick uyur, scheduler atlar, uyanınca kaldığı yerden sürer. Gerçek
zaman/I-O bekleme temeli (sleep(), bloklu I/O).

**Mekanizma:** her görevin tick geri-sayımı (kdl_block[]). kdl_uyu(N) → block=N + spin (scheduler
bloklu süresince ATLAR, görev koşmaz). kdl_preempt her tick tüm block'ları azaltır + yalnız READY
(block==0) göreve geçer; hepsi bloklu → idle (mevcutta kal).

**Doğrulama (QEMU 11.0.1):** sleep_arm — görev B kdl_uyu(8) ile bloklanır, A (main) o sırada koşar
(a_sayac artar), 8 tick sonra B READY → uyanır → "B WOKE a_kostu=VAR" (A uyku sırasında koştu).
Diğer scheduler testleri (sched/preempt) regresyonsuz. sıfır-uyarı.

**Sıradaki:** öncelikli scheduling; D1+D2+C7 birleşik gerçek user-process; D2-x86; C5 (virtio-blk).

---

## D-117 — OS C7b: preemptive scheduling (timer-IRQ → zorunlu bağlam-değiştirme) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-116).

**Karar [ETKİ: `boot/start_aarch64.S` (kdl_irq_ortak → FULL trap-frame + kdl_irq_isle);
`runtime/kdl_zaman.c` (kdl_irq_isle); `runtime/kdl_gorev.c` (kdl_preempt + preemptive scheduler);
yeni `test/bare_metal/preempt_arm.c`; `Makefile`. x86/host/codegen DEĞİŞMEDİ.]** FAZ C:
preemptive multitasking — timer-IRQ görevi ZORLA switch eder (görev yield etmez).

**Mimari (full trap-frame IRQ):** kdl_irq_ortak artık FULL bağlam (x0-x30 + ELR_EL1 + SPSR_EL1 =
272 bayt, x30@240/ELR@248/SPSR@256) kaydeder → kdl_irq_isle(sp) [GICC_IAR + tik/re-arm +
EOI(switch-ÖNCESİ) + kdl_preempt] devam SP'sini döner → SP swap (preempt'te sonraki görevin
trap-frame'i) → restore → eret. Preempt kapalıysa SP aynı → eski davranış (timer/sched testleri
NÖTR, regresyonsuz).

**Preemptive scheduler (kdl_gorev.c):** kdl_preempt(sp) round-robin görev trap-frame SP swap.
kdl_preempt_gorev_olustur sentetik trap-frame kurar (ELR=giriş, SPSR=EL1h+IRQ-açık) → ilk switch
eret ile göreve atlar. **EOI switch-ÖNCESİ KRİTİK:** GIC serbest kalır → sonraki timer-IRQ
sonraki görevi preempt eder; aksi halde IRQ27 active kalır → deadlock.

**Doğrulama (QEMU 11.0.1):** preempt_arm — 2 görev (A=main, B) busy-loop, ASLA yield ETMEZ.
B **1071 kez** koştu (yalnız timer preemption ile!), A 4 kez → "PREEMPT OK". Timer/sched/capstone
regresyonsuz (Stage-1 doğrulandı: full-trap-frame, preempt-off = eski davranış). sıfır-uyarı.

**Kapsam/sınır:** round-robin (öncelik/quantum-ayarı yok = C7c TCB-durum); tek adres-uzayı
(görevler bellek paylaşır; D1 ile birleşik per-process preemptive sonra); aarch64 (x86 IRQ0-stub
trap-frame rework = sonra).

**Sıradaki:** D1+D2+C7b birleşik gerçek user-process; C7c (öncelik/durum); D2-x86; C5 (virtio-blk).

---

## D-116 — OS D1: per-process adres-uzayı izolasyonu (TTBR0 swap) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-115).

**Karar [ETKİ: `runtime/kdl_mmu.c` (kdl_surec_kur + kdl_ttbr_degis — yeni fonksiyonlar); yeni
`test/bare_metal/d1_arm.c`; `Makefile` (calistir_d1_test_arm + os_kernels gate). Diğer kernel'ler
için dormant. x86/host/codegen DEĞİŞMEDİ.]** FAZ D: per-process adres-uzayı — her sürece ayrı
sayfa tablosu, geçişte TTBR0 swap → process bellek izolasyonu (D2 privilege ayrımının tamamlayıcısı).

**Mekanizma:** `kdl_surec_kur(L1, L2, user_pa)` — kernel identity (paylaşılan, AP=00) + user VA
(0x42000000) → sürece-özel `user_pa`. `kdl_ttbr_degis(L1)` — TTBR0_EL1 swap + dsb/tlbi/isb (TLB
flush) → adres-uzayı geçişi.

**Doğrulama (QEMU 11.0.1):** d1_arm — 2 süreç (A: user→PA 0x44000000, B: user→PA 0x46000000),
AYNI sanal adres 0x42000000'a A 0xAA / B 0xBB yazar; TTBR geçişlerinden sonra A hâlâ 0xAA, B hâlâ
0xBB → "SUREC A uva=0xaa" + "SUREC B uva=0xbb" = birbirini ETKİLEMEZ = **izolasyon kanıtı**. Diğer
kernel'ler regresyonsuz (yeni fonksiyonlar dormant). sıfır-uyarı.

**Kapsam/sınır:** demo EL1'de (VA→PA izolasyonunu izole gösterir; tam EL0-user-process = D1+D2
birleşimi); scheduler-entegrasyonu (context switch'te TTBR swap) sonra; kernel-identity aliasing
(PA_A/B aynı zamanda identity-VA'da görünür) — demo user-VA'dan eriştiği için zararsız.

**Sıradaki:** D1+D2 birleşik EL0 user-process (ayrı adres-uzayı + EL0); C7b preemptive; D3
process oluşturma (fork/exec-eşdeğeri).

---

## D-115 — OS D2: aarch64 finer paging (L2 2MB) → user/kernel privilege ayrımı (EL0) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-114).

**Karar [ETKİ: `runtime/kdl_mmu.c` (L1[1] RAM → L2 2MB tablo); `linker/bare-metal-aarch64.ld`
(.user section @0x42000000); `boot/start_aarch64.S` (kdl_el0_calistir); `runtime/kdl_kesme.c`
(syscall num2/3); yeni `test/bare_metal/d2_arm.c`; `Makefile` (calistir_d2_test_arm + os_kernels
gate). x86/host/codegen DEĞİŞMEDİ.]** FAZ D opener: gerçek EL0/EL1 (user/kernel) privilege ayrımı.

**Finer paging (D2 ön-koşulu):** C8a'nın L1[1] 1GB Normal bloğu → L2 tablo (512 × 2MB identity
sayfa). Per-region izin artık mümkün: kernel sayfaları AP=00 (EL1-only); user 2MB sayfası
(0x42000000) AP=01 (EL0+EL1 RW). Bu, D-114'teki D2-wall'u (EL0-writable RAM → EL1-non-executable)
ayrı user-page ile çözer (kernel kodu hâlâ AP=00 EL1-exec).

**D2 mekanizması:** `kdl_el0_calistir` (boot asm): SP_EL0 + ELR_EL1 + SPSR_EL1(EL0t) kur, eret →
EL0. EL0 kodu (d2_arm.c `el0_kod`, `.user` section @0x42000000, SELF-CONTAINED pure-SVC) kernel/
device sayfalarına (AP=00) DOĞRUDAN erişemez → yalnız SVC ile EL1 kernel'e geçer. Handler
SPSR_EL1.M[3:2] okur → kaynak-EL.

**Doğrulama (QEMU 11.0.1):** d2_arm → "D2 BASLA (EL1)" + "EL0 SYSCALL kaynak-EL=" + "0x0"
(=EL0, privilege ayrımı KANITI) + "D2 OK". 6 aarch64 kernel (dizi/sched/timer/syscall/istisna/
capstone) regresyonsuz (L2 finer granülarite + boş .user zararsız). sıfır-uyarı. test_tumu YEŞİL.

**Kapsam/sınır:** tek user-page (kod+stack aynı AP=01 sayfada → EL0 kendi kodunu yazabilir; tam
izolasyon = kod AP=11-RO + data AP=01 ayrı sayfa + D1 per-process sayfa tablosu). x86 ring3 (TSS
gerekir) = D2-x86 ayrı. IRQ EL0'da maskeli (D2 testi basitliği).

**Sıradaki:** D1 per-process adres-uzayı (TTBR0 swap, per-process L1/L2); C7b preemptive; C5
(virtio-blk, import+codegen C-track).

---

## D-114 — OS C8c: fault-adresi teşhisi (FAR_EL1 / CR2) + D2 ertelendi (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-113).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_istisna_isle). Sadece teşhis; davranış-nötr.]** İstisna
(fault) işleyicisi artık fault ADRESİNİ de basar: aarch64 FAR_EL1, x86 CR2 (#PF lineer adresi).
Data/instruction abort'ta hangi adrese erişildiği görünür → teşhis. Abort-dışı için stale ama
zararsız.

**Doğrulama:** istisna testleri (aarch64 data-abort + x86 ud2) hala geçer + "adr=0x<FAR>" basar.
Sıfır-uyarı (iki arch). Diğer kernel'ler regresyonsuz (yalnız fault yolunda çalışır).

**NOT — D2 (EL0/ring3 privilege ayrımı) ERTELENDİ:** EL0 user/kernel ayrımı denendi. ARMv8
mimari kuralı: **EL0-writable RAM → EL1'de non-executable** (Prefetch Abort EC=0x21 ile
kanıtlandı). Tek-region identity-map (L1 1GB blok) kernel-kodu + EL0-user-region'ı aynı sayfa
permission'a zorluyor → çakışma. Minimal D2 AYRI user-region (finer L2/L3 sayfa tablosu +
linker section yerleşimi) gerektirir = D1 per-process adres-uzayı ile birlikte yapılacak D-fazı
paging işi. Tasarım (kdl_el0_calistir eret→EL0 + syscall SPSR_EL1 kaynak-EL raporu) hazır,
revert edildi.

**Sıradaki seçenekler:** D-fazı (finer page tables → per-process adres uzayı + privilege ayrımı);
C5 (virtio-blk, import+codegen C-track fix gerek); C7b (preemptive — IRQ-handler full-context
trap-frame rework).

---

## D-113 — OS C7a: cooperative scheduling — bağlam-değiştirme (görev kuyruğu) (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-112).

**Karar [ETKİ: yeni `runtime/kdl_gorev.c`; `boot/start_aarch64.S` + `boot/start_x86_64.S`
(kdl_baglam_degis asm); yeni `test/bare_metal/sched_test.c`; `Makefile` (bm_*_gorev.o + 2 sched
hedefi + os_kernels gate). x86 IR/host/codegen DEĞİŞMEDİ.]** FAZ C: işbirlikçi çok-görevlilik.
Görevler kdl_gorev_ver() (yield) ile gönüllü CPU bırakır → round-robin bağlam-değiştir.
MMU/preemption gerektirmez (C7b preemptive = timer-IRQ quantum, sonra).

**Tasarım:** TCB = callee-saved register'lar + SP. kdl_baglam_degis (asm): mevcut görevin
callee-saved'ını TCB'ye kaydet, sonrakinin kinden yükle, `ret` → sonraki görev kaldığı yerden
sürer (caller-saved yield çağrı-noktasında C ABI'siyle korunur, kaydetmeye gerek yok). Görev 0 =
main bağlamı (init gerektirmez; ilk yield'de TCB'ye kaydedilir). Yeni görev: TCB dönüş-adresi =
giriş, TCB.sp = yığın-tepe.
- aarch64: x19-x28+x29+x30+sp (TCB[0..12], ×8 bayt; x30=giriş, ret oraya).
- x86: rbx/rbp/r12-r15+rsp (TCB[0..6]); giriş yığına push (switch ret'i pop eder).

**Doğrulama (QEMU 11.0.1):** sched_test.c (AYNI kernel iki mimaride): main+gorev1 round-robin →
"SCHED BASLA / [main] / [gorev1] / [main] / [gorev1] / [main] / [gorev1] / SCHED OK" — interleave
= bağlam-değiştirme çalışıyor. aarch64 + x86 ikisi de geçti. kdl_gorev.c sıfır-uyarı. Diğer
kernel'ler regresyonsuz (kdl_baglam_degis dormant). os_kernels gate artık **14** (7 yetenek × 2 arch).

**KEMGU bağı:** region-ownership + `görev` (D-008 concurrency) ileride gerçek thread'le buluşur —
statik tip-kontrollü `görev`/`kanal`'ın runtime temeli.

**Kapsam/sınır:** cooperative (preemption yok = C7b timer-IRQ quantum); tek adres-uzayı (TCB ortak
bellek; per-process MMU-izolasyon = D1); SIMD-context yok (-mgeneral-regs-only, C8b).

**Sıradaki:** C7b preemptive (timer-IRQ → zorunlu yield) / C5 (import+codegen) / D1 (per-process).

---

## D-112 — OS C8a: aarch64 MMU-on (identity map) — RAM Normal-WB, sanal-bellek temeli (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-111).

**Karar [ETKİ: yeni `runtime/kdl_mmu.c`; `boot/start_aarch64.S` (`bl kdl_mmu_kur` main'den önce);
`Makefile` (bm_a64_mmu.o). x86/host/codegen DEĞİŞMEDİ.]** FAZ C keystone: aarch64 MMU'yu
identity-map ile aç → RAM Device-nGnRnE'den **Normal-WB cacheable**'a → cache + SIMD-uyumlu
bellek + sanal-bellek/process-izolasyon (D fazı) temeli.

**Identity harita (4KB granül, 39-bit VA, L1 1GB blok):**
- L1[0] 0-1GB → Device (GICv2 0x08000000, UART 0x09000000).
- L1[1] 1-2GB → Normal-WB (kernel + 16MB heap @ 0x40000000).
- MAIR (attr0=Device, attr1=Normal-WB), TCR (T0SZ=25, 4KB, WB, inner-sh, EPD1, IPS=36bit),
  TTBR0=L1, SCTLR.M|C|I. dsb/tlbi/isb sıralı.

**x86_64:** long mode ZATEN paging gerektirir → `boot/start_x86_64.S` identity sayfa
tablolarıyla zaten MMU-on (PVH→long mode). Ek MMU kurulumu gerekmez; kdl_mmu.c yalnız aarch64.

**Doğrulama (QEMU 11.0.1):** 4 aarch64 kernel MMU-on boot eder — hello, region+heap-dizi (memcpy
Normal-cached bellekte), timer (IRQ MMU üstünden), syscall. x86 regresyonsuz. test_tumu YEŞİL
(host değişmedi). kdl_mmu.c sıfır-uyarı.

**Kapsam/sınır:** identity-only (VA==PA), tek adres-uzayı (per-process = D1). **-mgeneral-regs-only
KORUNUYOR (C8b ertelendi):** MMU Normal-memory'yi açtı ama kernel-geneli SIMD, IRQ/exception
handler'larında SIMD-bağlam kaydı gerektirir (handler'lar şu an yalnız GPR kaydeder, q0-q31
değil) → kesme anında SIMD-state bozulur. Linux-benzeri sound kernel-FP-yasağı politikası sürüyor;
SIMD'i global açmak ayrı refinement (handler SIMD-save). C8c sayfa-hata: data abort zaten
kdl_exc_ortak'ta yakalanıyor (C3a) → ESR+ELR teşhis+halt; demand-paging D-fazı.

**Sıradaki:** C7a cooperative scheduling (görev kuyruğu + context switch, MMU-bağımsız).

---

## D-111 — OS Capstone: tam yığın tek boot'ta kompoze (region+timer+IRQ+syscall) — entegrasyon kanıtı (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-110).

**Karar [ETKİ: yeni `test/bare_metal/capstone.c`; `Makefile` (2 capstone hedef + os_kernels 12 teste).
Runtime/codegen/host/boot DEĞİŞMEDİ.]** Minimal gösterici parçalarını (her biri ayrı kanıtlı) TEK
boot'ta birlikte koşturur → izole birim testlerin ötesinde **entegrasyon/kompozisyon kanıtı**.

**Capstone kernel (iki arch ortak C kernel):** (1) region dizi 1..10=55 (frame allocator);
(2) timer+IRQ aç (kdl_kesme_kur + kdl_timer_baslat); (3) region tahsis IRQ AÇIKKEN → "POST-IRQ=99"
(**allocator IRQ-safe** — kesme handler'ları allocation-free olduğundan heap bozulmuyor; D-108/109
KISIT'ı pratikte doğrulanır); (4) syscall → "CAPSTONE OK"; (5) idle (timer arka planda → "TIMER OK
tik=5").

**Doğrulama (QEMU 11.0.1) — `calistir_os_kernels` 12/12:** capstone (aarch64 virt + x86_64 PVH) →
"55" + "99" + "CAPSTONE OK" + "TIMER OK" hepsi. 10 birim test + 2 capstone.

**🎉 MİNİMAL OS GÖSTERİCİ + ENTEGRASYON TAM (her iki mimaride):** os/c1-region-backing branch —
C1a/b/x86 (region) + C3a (exception) + C3b/C4 (IRQ+timer) + C6 (syscall) + Capstone (7 commit).
`make calistir_os_kernels` = **12 QEMU boot kanıtı**. Beyond-minimal (flag'li): C5 virtio codegen
(task_09d48d31 — &Struct+sonuç deep multi-subsystem), C7 scheduling, MMU, self-host bare-metal.

---

## D-110 — OS C6: bare-metal sistem çağrısı (aarch64 SVC + x86 int 0x80) — dispatch+dönüş; MİNİMAL GÖSTERİCİ TAM (2026-06-30)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-109).

**Karar [ETKİ: `runtime/kdl_kesme.c` (kdl_syscall_isle + IDT[0x80] gate); `boot/start_aarch64.S`
(kdl_exc_ortak ESR.EC kontrolü → kdl_svc_ortak); `boot/start_x86_64.S` (kdl_syscall_stub); yeni
`test/bare_metal/syscall_test.c` (portable); `Makefile` (2 syscall hedef + os_kernels 10 teste).
Codegen/host DEĞİŞMEDİ.]** C6: minimal sistem çağrısı → **MİNİMAL OS GÖSTERİCİ TAMAMLANDI**.

**aarch64 (SVC):** sync exception handler (kdl_exc_ortak) artık ESR.EC ayrımı yapar: 0x15 (SVC) →
kdl_svc_ortak (bağlam kaydet, num=x8/arg=x0, kdl_syscall_isle, **ERET**); diğer EC → kdl_istisna_isle
(fault, halt). İstisna ve syscall AYNI sync vektörde (0x200), EC ile ayrılır.

**x86_64 (int 0x80):** IDT[0x80] → kdl_syscall_stub (caller-saved kaydet, num=rax/arg=rdi,
kdl_syscall_isle, **IRETQ**).

**Doğrulama (QEMU 11.0.1) — `calistir_os_kernels` 10/10:** syscall_test (iki arch ortak): "BEFORE
SYSCALL" → "SYSCALL OK num=1" (kernel dispatch) → "AFTER SYSCALL" (eret/iretq dönüş kanıtı).
test_tumu YEŞİL (host değişmedi).

**🎉 MİNİMAL OS GÖSTERİCİ TAM (her iki mimaride QEMU-kanıtlı):**
boot + region-bellek + sürücü(UART) + exception + IRQ + timer + syscall — aarch64 (QEMU virt) +
x86_64 (QEMU PVH). AYNI KEMGU region-confinement runtime (F4.2b/F4.3 backing) iki platformda.
`make calistir_os_kernels` = 4 boot + 2 istisna + 2 timer + 2 syscall (D-105..D-110).

**Kapsam-dışı (Mehmet kararı: minimal-gösterici):** scheduling/multitasking (C7); virtio-blk
codegen fix (C5 — &Struct param+sonuç<> segfault; UART sürücü zaten "bir sürücü" gereğini karşılar,
virtio beyond-minimal); MMU/sayfalama; self-host bare-metal.

---

## D-109 — OS C3b/C4: bare-metal IRQ + periyodik timer (GICv2/CNTV + PIC/PIT) — tick kanıtı (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-108).

**Karar [ETKİ: yeni `runtime/kdl_zaman.c` (IRQ dispatch + timer, iki arch ortak API);
`boot/start_aarch64.S` (IRQ vektör routing + kdl_irq_ortak bağlam-kaydet/eret);
`boot/start_x86_64.S` (kdl_irq0_stub + EOI/iretq); `runtime/kdl_kesme.c` (IDT[32]→IRQ0); yeni
`test/bare_metal/timer_test.c` (portable, iki arch); `Makefile` (bm_*_zaman.o + 2 timer hedef +
os_kernels 8 teste genişledi). Codegen/host DEĞİŞMEDİ.]** C3b IRQ altyapısı + C4 timer birleşik:
periyodik donanım kesmesi → handler → tick (C4 timer'ı C3b IRQ altyapısı olmadan kanıtlanamaz).

**Ortak API (arch-bağımsız kernel):** `kdl_kesme_kur()` (GIC/PIC init) + `kdl_timer_baslat()`
(CNTV/PIT + IRQ aç) + `kdl_kesme_isle(irq)` (dispatch, tik say) + `kdl_idle()` (wfi/hlt). AYNI
timer_test.c iki mimaride (kernel arch-bağımsız, runtime arch-spesifik).

**aarch64 (GICv2 + sanal generic timer):** GICD@0x08000000 + GICC@0x08010000 enable + ISENABLER0
bit27 (timer PPI 27). CNTV ~10ms (CNTFRQ/100). IRQ vektör (0x280, entry5 → kdl_irq_ortak):
bağlamı kaydet (x0-x18,x30; x19-x29 C korur) → GICC_IAR oku → kdl_kesme_isle → re-arm (CNTV_TVAL,
ISTATUS temizler) → GICC_EOIR ack → **ERET** (kesilen wfi-döngüsüne dön). DAIF.I temizle.

**x86_64 (PIC 8259 + PIT 8254):** PIC remap IRQ0-15→vektör32-47 (ICW1-4), mask=yalnız IRQ0. PIT
ch0 mode3 ~100Hz (bölen 11932). IDT[32]→kdl_irq0_stub: caller-saved kaydet → kdl_kesme_isle →
PIC EOI (port 0x20) → **IRETQ**. sti.

**KISIT:** kesme bağlamı bölge/frame allocator KULLANMAZ (tek-thread, IRQ-safe değil) → yalnız UART
yazımı + register.

**Doğrulama (QEMU 11.0.1) — `calistir_os_kernels` 8/8:**
- `calistir_timer_test_arm`: GICv2+CNTV → "TIMER BASLA" + 5 tik → "TIMER OK tik=5".
- `calistir_timer_test_x86`: PIC+PIT → "TIMER OK tik=5".
- 4 boot + 2 istisna regresyonsuz (IRQ vektör routing sync exception'ları etkilemedi). Sıfır
  uyarı (iki arch). test_tumu YEŞİL (host değişmedi).

**Sıradaki:** C5 virtio sürücü codegen fix (&Struct param+sonuç<> segfault — virtio_blk_init/bind),
C6 minimal syscall (SVC/int).

---

## D-108 — OS C3a: bare-metal exception vektörleri (aarch64 VBAR + x86 IDT) — fault→teşhis+halt (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-107).

**Karar [ETKİ: yeni `runtime/kdl_kesme.c` (istisna işleyici + x86 IDT kur); `boot/start_aarch64.S`
(VBAR + 16-giriş vektör tablosu + EL-duyarlı ortak işleyici); `boot/start_x86_64.S` (32 ISR stub +
isr_ortak + kdl_idt_kur çağrısı); yeni `test/bare_metal/istisna_{arm,x86}.c`; `Makefile`
(bm_*_kesme.o + 2 istisna test hedefi). Runtime/codegen/host DEĞİŞMEDİ.]** C3 ilk adım: CPU
istisnaları artık vektör tablosuyla yakalanır → sessiz çöküş yerine "ISTISNA tip/synd/PC" + halt.

**aarch64 (VBAR):** boot 16-giriş (×0x80, 2KB-hizalı) vektör tablosu kurar, VBAR_ELx=tablo
(EL-duyarlı). Ortak işleyici ESR/ELR okur — **KRİTİK EL-duyarlı:** QEMU virt **EL1**'de koşar
(`-d int` "from EL1 to EL1"); EL1'de `esr_el2` okumak UNDEF → işleyici kendini sonsuz fault'lar
(58683× gözlendi). Düzeltme: CurrentEL kontrolü → esr_el1/elr_el1.

**x86_64 (IDT):** 32 ISR stub (hata-kodlu 8/10/11/12/13/14/17/21 ISR_ERR, diğerleri ISR_NOERR +
dummy 0). Long mode same-privilege exception SS:RSP:RFLAGS:CS:RIP push eder → ortak yığın
[vektör][hata][RIP] → isr_ortak → kdl_istisna_isle(rdi,rsi,rdx). boot long_entry'de `kdl_idt_kur()`
(C; gate-tablosu 256×16B + lidt) main'den ÖNCE. ud2 → vektör 6.

**KISIT:** işleyiciler bölge/frame allocator KULLANMAZ (tek-thread, IRQ-safe değil) → yalnız UART
yazımı + register oku.

**Doğrulama (QEMU 11.0.1):**
- `calistir_istisna_test_arm`: eşlenmemiş erişim → ESR=0x96000000 (EC 0x25 Data Abort) →
  "ISTISNA tip=0x4" + halt; "GORUNMEMELI" yok.
- `calistir_istisna_test_x86`: ud2 → "ISTISNA tip=0x6" (invalid opcode) + halt; "GORUNMEMELI" yok.
- 4 normal kernel (aarch64+x86 × hello+dizi) regresyonsuz. test_tumu YEŞİL (host değişmedi).

**DÜZELTME (D-105/107 notları):** QEMU virt -cpu cortex-a72 **EL1**'de boot eder (EL2 DEĞİL).
FP-enable kodu EL-duyarlı olduğu için EL1'de CPACR_EL1 yolu çalışıyordu (CPTR_EL2 dalı ölü, zararsız).
D-105/107'deki "EL2" ifadeleri bu yönde okunmalı.

**Sıradaki:** C3b IRQ altyapısı (GICv2 aarch64 / PIC 8259 x86) — C4 timer için.

---

## D-107 — OS C1-x86: x86_64 bare-metal parite — PVH→long-mode boot, AYNI region kernel (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-106).

**Karar [ETKİ: yeni `boot/start_x86_64.S` (PVH→long mode); yeni `linker/bare-metal-x86_64.ld`;
`Makefile` (BM_X86_OBJS + 2 x86 kernel hedefi + `calistir_os_kernels` toplu gate). Runtime/
codegen/host DEĞİŞMEDİ.]** Mehmet kararı (aarch64+x86_64 paralel): AYNI region-backed kernel'i
x86_64'te de boot et.

**PVH boot (multiboot1 yerine):** QEMU `-kernel` multiboot1 için 32-bit ELF ister, 64-bit'i
reddeder ("Cannot load x86-64 image, give a 32bit one"). Çözüm = PVH: `.note.Xen` içinde
XEN_ELFNOTE_PHYS32_ENTRY (tip 18) → QEMU 64-bit ELF'i 32-bit `pvh_entry`'den boot eder
(ELFCLASS64 korunur). Minimal PVH stub'la (COM1'e 'OK') önce doğrulandı.

**32-bit → long mode trampoline (boot/start_x86_64.S):**
1. (32-bit) BSS sıfırla (sayfa tabloları .bss'te → temizlenir, sonra kurulur).
2. Identity sayfa tabloları: PD 512×2MB huge page = 0..1GB (kernel+stack+16MB heap < 1GB);
   PDPT[0]→PD; PML4[0]→PDPT.
3. CR3=PML4 → CR4.PAE → EFER.LME (MSR 0xC0000080) → CR0.PG|PE → long mode.
4. GDT (null + 64-bit code L-bit + data); `ljmp $0x08,$long_entry` → 64-bit.
5. (64-bit) segment'ler + RSP=__stack_top + `call main`; main dönerse hlt döngüsü.

**Region backing ARCH-BAĞIMSIZ (C0 mimarisinin öngörüsü doğrulandı):** kdl_bolge.c +
kdl_bare_heap.c (frame allocator) + kdl_dizi.inc DEĞİŞMEDEN x86_64'te derlenir/çalışır. Tek
arch-spesifik fark: UART=16550 (COM1 0x3F8 port I/O, KDL_UART_PUTC=kdl_uart_16550_putc), boot
stub, linker. `-mgeneral-regs-only` (x86: SSE emit etme → boot'ta CR4.OSFXSR enable gerekmez;
aarch64'teki Device-memory q-register sorununun simetrik çözümü).

**Doğrulama (qemu-system-x86_64 11.0.1) — `calistir_os_kernels` toplu gate 4/4:**
- `calistir_kernel_dizi_x86_bare_metal`: AYNI kernel_dizi.kem → BOOT + "KERNEL DIZI OK" + "55"
  (libc-yok temiz). aarch64 ile bit-bit aynı .kem + aynı region runtime.
- `calistir_uart_merhaba_x86_bare_metal`: "Merhaba KEMGU" + "42".
- aarch64 hedefleri (qemu_smoke + dizi) regresyonsuz. `test_tumu` YEŞİL (host değişmedi).

**Sonuç — C1 keystone TAM (her iki mimaride):** aynı KEMGU kaynağı (kernel_dizi.kem) + aynı
region-confinement runtime (F4.2b/F4.3 backing) hem aarch64 (QEMU virt) hem x86_64 (QEMU PVH)
üzerinde boot eder + doğru hesaplar. Region modeli OS'in bellek temelini iki platforma da bedavaya verir.

**Kapsam/sınır:** PVH 32-bit entry → long mode minimal (identity 1GB, tek-çekirdek, IDT/exception
yok — aarch64 ile simetrik). 16550 init V1-no-op (QEMU yeterli; ham donanım board-init sonra).
Sıradaki: C3 exception/interrupt (DESIGN-STOP), C4 timer.

---

## D-106 — OS C1b: bare-metal dizi runtime — heap Dizi<tam32> kernel boot (kdl_dizi.inc tek-kaynak) (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D'sine göre kesinleştir (taban: D-105).

**Karar [ETKİ: yeni `runtime/kdl_dizi.inc` (paylaşımlı dizi impl); `runtime/kdl_runtime.c`
(dizi bloğu → `#include`); `runtime/kdl_bare_heap.c` (kdl_panik seam + `#include`); yeni
`test/ornekler/kernel_dizi.kem`; `Makefile` (aarch64 bare-metal shared-objects refactor +
`calistir_kernel_dizi_bare_metal`). Codegen DEĞİŞMEDİ; host davranışı DEĞİŞMEDİ.]** C1a
region-backing'i diziye genişlet: heap `Dizi<tam32>` (kdl_dizi_* runtime) bare-metal'de boot.

**Tek-kaynak carve (duplikasyon YASAK — D-069 sınır-kontrolü iki yerde olamaz):**
- `kdl_dizi.inc`: KdlDizi + kdl_dizi_buyut/olustur/ekle/al/yaz/yapi/boyut/kapasite/serbest +
  kdl_dizi_oob. Host (kdl_runtime.c) VE bare-metal (kdl_bare_heap.c) `#include` eder → her TU
  KENDİ kopyasını derler, ASLA birlikte linklenmez (duplicate-symbol yok) = tek kaynak.
- Panik seam `kdl_panik`: host kdl_runtime.c (stderr+abort); bare-metal kdl_bare_heap.c
  (→ kdl_panik_dur, UART+halt). kdl_dizi_oob FREESTANDING biçimlenir (snprintf YOK; "(i,boyut)"
  detayı korunur) → host çıktısı bayt-özdeş, bare-metal'de libc'siz çalışır. kdl_panik codegen
  inline-OOB (src/llvm.c) tarafından da çağrılır → evrensel panik girişi.

**Makefile shared-objects refactor (zorunluydu):** aarch64 kernel'leri artık `BM_A64_OBJS`
paylaşır (start+uart+yazdir+bolge+heap+panik). kdl_bare_heap.o artık `.inc` yüzünden
kdl_panik→kdl_panik_dur referansı verdiğinden HER kernel kdl_runtime_panik.o linklemeli (merhaba
dahil) → inline-compile yerine ortak obje kuralları.

**Doğrulama:**
- `calistir_kernel_dizi_bare_metal` (QEMU, qemu-system-aarch64 11.0.1): kernel BOOT →
  "KERNEL DIZI OK" + "55" (1..10 toplam; dizi_olustur→ekle [kapasite 0→4→8→16: kdl_dizi_buyut +
  bölge realloc + memcpy] → al [D-069 sınır-kontrollü]). libc-yok kapısı temiz. IR'de
  kdl_dizi_olustur/ekle_tam/al_tam/boyut/kapasite_ayarla = heap path (stack değil).
- `calistir_qemu_smoke` (merhaba shared-objects refactor sonrası): boot + "Merhaba KEMGU"+"42" ✓
  (regresyon yok).
- Host: kdl_runtime.c (`.inc` ile) sıfır-uyarı; `calistir_llvm_test` (30 array programı) yeşil;
  `test_tumu` YEŞİL (self-host fixpoint 32157 satır + tüm array/OOB testleri — `.inc` host
  davranışını değiştirmedi). Yeni bare-metal objeleri sıfır-uyarı.

**Kapsam/sınır:** (1) dizi-OOB → kdl_panik (D-069) bare-metal'de geçerli (kernel UART "PANIK:"+halt).
(2) Büyüyen-dizi grow-leak (F4.3 flag, task_dc5b969f) bare-metal'de de var (sabit-kapasiteli
kernel-loop için yeterli). (3) x86_64 dizi paritesi C1-x86 (x86_64 long-mode boot bring-up gerekir).

---

## D-105 — OS C1a: bare-metal bölge-backing — ilk boot-eden region-alloc KEMGU kernel (aarch64/QEMU) (2026-06-29)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: D-104 sonrası; merge'de origin/main ilerlemişse güncelle).

**Karar [ETKİ: yeni `runtime/kdl_bare_heap.c`; `runtime/kdl_bolge.c` (#ifdef köprü);
`boot/start_aarch64.S`; `linker/bare-metal-aarch64.ld`; `Makefile` (uart_merhaba + qemu_smoke).
C derleyici/codegen DEĞİŞMEDİ; host yolu DEĞİŞMEDİ.]** OS gösterici-kernel FAZ C keystone'u:
bölge runtime'ını bare-metal'e bağla — **codegen'e dokunmadan**, yalnız RUNTIME backing.

**Kök bulgu (regresyon — readiness-envanteri "reçete-tam" dediği yer KIRIKTI):** Region codegen
(F4.x) main dâhil HER fonksiyona koşulsuz `@kdl_global_bolge_al`/`@kdl_bolge_olustur`/
`@kdl_bolge_serbest` emit eder (declare'da attribute yok → -O2 DCE EDEMEZ). Bare-metal link bu
sembolleri sağlamıyordu → `calistir_uart_merhaba_bare_metal` HEAD'de `ld.lld: undefined symbol`
ile KIRIK (hello-world dâhil TÜM kernel'ler). Ampirik kanıt: `kemgu --llvm uart_merhaba.kem`
main'inde 3 canlı region çağrısı + mevcut link denemesi 3 undefined-symbol.

**Çözüm (4 parça, codegen-nötr):**
1. `kdl_bare_heap.c` (freestanding, yalnız <stdint/stddef>): linker heap bölgesinden
   (`__heap_start..__heap_end`) bump + serbest-liste `malloc`/`free` + `memcpy`/`memset` +
   `kdl_global_bolge_al`. Region 64KB blokları serbest-listede geri kazanılır → F4.3 per-iter
   region-free → kernel-loop sınırlı-bellek temeli.
2. `kdl_bolge.c`: `#ifdef KEMGU_BARE_METAL` → <stdlib.h> yerine malloc/free prototip (<stdlib.h>
   aarch64-unknown-none'da YOK). Host (#else) AYNEN — `calistir_kdl_bolge_test` 33/33 ASan-temiz.
3. `boot/start_aarch64.S`: FP/SIMD trap kapat — EL-duyarlı (QEMU virt EL2 → CPTR_EL2.TFP[10]=0;
   ham EL1 → CPACR_EL1.FPEN=0b11).
4. `linker`: stack üstüne 16 MB heap (`__heap_start/__heap_end`, NOBITS).
+ `Makefile`: bare-metal compile'lara `-mgeneral-regs-only` + region runtime'ı link'e ekle +
   `calistir_qemu_smoke` `-serial file:` (Windows stdio-redirect bypass).

**İki kritik bare-metal gotcha (QEMU `-d in_asm` trace ile teşhis edildi):**
- **FP/SIMD trap:** clang -O2 16-baytlık struct move'u `ldr/stur q0` ile yapar → EL2 reset'te
  trap → vektör 0x200 (vektör tablosu yok) → çöküş. (FP-enable + GPR-only.)
- **Device-memory alignment:** MMU kapalıyken RAM = Device-nGnRnE → 16-baytlık q-erişimi
  8-hizalı adreste **alignment-fault**. `-mgeneral-regs-only` q-register'ı tümden eler
  (≤8-bayt hizalı erişim = Device-memory'de güvenli; Linux çekirdek deseni).

**Doğrulama:** `calistir_qemu_smoke` (gerçek qemu-system-aarch64 11.0.1) → kernel BOOT +
"Merhaba KEMGU - Bare Metal" + "42" ✓. kernel.elf q-register sayısı=0, libc-yok kapısı temiz,
sıfır derleme uyarısı. `test_tumu` YEŞİL (self-host fixpoint 32157 satır kararlı; 72/72 codegen;
4-mod driver; ASan-temiz — regresyon yok).

**Kapsam/sınır:** (1) MMU kapalı → `-mgeneral-regs-only` kernel-geneli FP-yasağı (gösterici için
yeterli; FP-li kernel = MMU+Normal-memory sonraki milestone). (2) Dizi (`kdl_dizi_*`) bare-metal'de
HENÜZ yok → C1b (kdl_dizi.c carve + allocate-eden dizi kernel). (3) x86_64 boot stub/linker yok →
C1-x86 (Mehmet kararı: aarch64+x86_64 paralel). (4) Üretici C-bootstrap kemgu; self-host bare-metal
kapsam-dışı v1.

---

## D-104 — düzelt(self-host codegen): blok-leksik-kapsam shadowing → değişken tablosu push/pop (2026-06-27)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: D-103 sonrası; merge'de origin/main ilerlemişse güncelle).

**Karar [ETKİ: `selfhost/codegen.kem` (KEMGU kaynağı; C derleyici DEĞİŞMEDİ) + 2 yeni
`test/cg_korpus` regresyon testi].** Self-host codegen'in işlev-içi değişken tablosu
(`cg_ad`/`cg_areg`/… paralel diziler) **append-only** idi (`cg_base..son` sondan-ara =
shadow). İç blokta DIŞ değişkeni gölgeleyen `değişken v` blok bittikten sonra **pop
EDİLMİYORDU** → blok-sonrası `v` lookup'ı İÇ (kapsam-dışı) slot'a çözülürdü = **leksik
kapsam MISCOMPILE** (bellek-güvenliği değil; dizi fonksiyon-dönüşüne kadar canlı, UAF yok).

**Repro (`cg_kapsam_shadow.kem`):** `değişken v=[100,200,300]; eğer …>0 { değişken v=[7,8,9]; … }
ver dizi_al(v,2)` → DIŞ v[2]=300 olmalı; hata İÇ v slot'una `load ptr %2` emit edip **9**
döndürürdü (koşul yanlışsa İÇ slot hiç store edilmez → D-069 sınır-kontrolü PANIC; yine de
yanlış-kapsam çözümü). C `src/llvm.c` AYRI kod yolu, `scope_gir`/`scope_cik` (bağlı-liste
başı kaydet/geri-yükle) ile DOĞRU idi (300 döner).

**Çözüm — C disiplinini yansıt (`cg_kapsam_kapat`):** Dizi-shrink built-in'i olmadığından
truncate yerine **ad-blank**: blok/döngü/eşleş-kolu girişinde `kapsam_bas = dizi_boyut(cg_ad)`;
çıkışta `[kapsam_bas, son)` aralığındaki bağlamaların `cg_ad` adını `""` yap → `cg_var_bul`
(non-empty `ad` arar) boş slot'u atlar, dış bağlamaya geri döner. Slot canlı kalır, **emit
edilmiş IR DEĞİŞMEZ** (yalnız derleme-zamanı isim çözümü). Kapsamlanan yollar: `BLOK` (eğer/
iken/güvensiz gövdeleri de BLOK → transitif), `ICIN` döngü değişkeni, `ESLES` kol payload
bağlamaları. ESLES skaler kol (literal/joker) bağlama yapmaz → dokunulmadı.

**Soundness (fixpoint güvenliği):** `--check` zaten iç-kapsam değişkenine blok-sonrası erişimi
reddeder; gölgeleme yoksa üretilen IR bayt-aynı kalır. Bu yüzden lexer/parser/checker.kem
bootstrap'ı etkilenmez (zaten gölge-sızıntıya bel bağlamıyorlardı).

**Doğrulama:** cg_korpus **72/72** semantik (C-oracle ≡ self-host; +2 yeni: shadow=44≡300%256,
döngü-shadow=120). Bootstrap **FIXPOINT** ✓ (lexer/parser/checker 55/55 bayt-aynı + codegen
stage1==stage2, 25290 satır). Driver 4-mod **TÜM GEÇTİ** (token 22 + parse 12 + check 48,
C-built & self-host). Manuel kenar: iç-içe üçlü-shadow=173, kardeş-blok ad-yeniden-kullanım=45,
koşul-yanlış→DIŞ-oku=300 — hepsi C ile eşleşir. **Sınır:** C kapsam yolu zaten doğruydu (bu
commit yalnız self-host'u hizalar); lokal-değişken-tablosu hâlâ append (truncate yok), ad-blank
yeterli.

---

## D-103 — [YÜKSEK] F4.2b: ρ_yerel GERÇEK-serbest + escape kaçış-yolu sağlamlaştırma (SOUND) (2026-06-22)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: D-102 KONSOLİDASYON sonrası; merge'de origin/main ilerlemişse güncelle).

**Karar [ETKİ: YÜKSEK — `src/escape.c` + `src/escape.h` + `src/llvm.c` + testler; bölge-serbest
semantiği ilk kez GERÇEKTEN ETKİN; izole commit].** F4.2b'nin başlık işi: **ilk gerçek bölge-serbest.**
Kaçmayan yerel diziler `ρ_yerel`'e (fonksiyon-yerel KdlBolge) tahsis edilir ve fonksiyon çıkışında
`kdl_bolge_serbest` ile serbest bırakılır. **Kaçan TÜM tahsisler `ρ_caller`'da kalır (serbest EDİLMEZ)
→ UAF imkânsız.**

**Yük taşıyan invaryant:** *kaçan → ρ_caller, kaçmayan → ρ_yerel, ρ_yerel ret'te serbest. Bunu kır →
sessiz UAF. Şüphede konservatif (ρ_caller).*

**KÖKLÜ KARAR — POZİTİF KANIT, escape DFA'ya GÜVENME (default-deny):** İlk iki routing girişimi escape
DFA'sının yargısına güvendi (önce `bolge_belirle` default-YEREL; sonra `escape_kayitli_mi`+`BOLGE_YEREL`).
**Çok-ajanlı adversarial hunt (8 aile × repro + bağımsız ASan-doğrulama, `sanitize_address` enjekte ederek
IR-fonksiyon load/store'ları da denetlendi) bu yaklaşımda 18 DOĞRULANMIŞ UAF buldu:** iç-içe agregat
(`Dizi<Dizi<T>>`, yapı-içi-dizi, ara-değişken), closure yakalama, alias/yeniden-atama, loop-carried.
**Kök sebep:** escape DFA bir **MAY-yaklaşımı** — kaçış yollarını KAÇIRIR; "DFA escape bulamadı" free için
GÜVENİLMEZ (her kaçırılan yol = UAF; D-102'nin "sınırsız 'tüm rotaları yakaladım mı?' riski" tam da bu).
Kaçırılan-yolu-tek-tek-yamamak (deep `ifadeyi_yukselt`, lambda-guard...) sonsuz whack-a-mole.

**ÇÖZÜM (principle 1+3 — "lokallik KANITI" + "DAR, inşa-gereği sound"):** Routing artık escape DFA'ya
DEĞİL, **POZİTİF + default-deny CONFINEMENT kanıtına** dayanır. Bir dizi-değişkeni `ρ_yerel`'e SADECE şu
İKİ koşulda yönlenir:
1. **SKALER-ELEMAN** (`elem_ir` ne `ptr` ne `%struct`): skaler eleman → `dizi_al` KOPYA döndürür, iç-ptr
   kaçışı YOK. `Dizi<Dizi>/Dizi<metin>/Dizi<yapı>` → ptr/struct eleman → iç heap-ref kaçabilir → ρ_caller.
2. **`escape_kesin_yerel` (confined kanıtı, `escape.c`):** bağlı değişkenin govdedeki **HER** kullanımı
   şunlardan biri olmalı: `var[i]` okuma, `var[i]=...` yerinde-yazma, retain-ETMEYEN dizi-builtin'in
   İLK argümanı (`dizi_al/boyut/yaz/ekle/kapasite`). **Başka HER konum** (ver, `b=var` alias, `[..,var,..]`
   /yapı alanı, çağrı argümanı, lambda yakalama, `&var`, `var` yeniden-atama) → **DENY → ρ_caller.**
   Default-deny = inşa-gereği sound (bir tek yamadan değil, kapalı-form pozitif kanıttan). Tüm AST düğüm
   tipleri kapsanır; bilinmeyen düğüm → konservatif deny.

**Yan-sağlamlaştırma (`ifadeyi_yukselt` DERİN yapıldı):** Escape DFA'nın DİĞER tüketicileri (G005 closure,
bolge_atama) için transitif terfi sığdı → agregat kaçınca gömülü dizi/alan da `ESC_CAGIRAN`. Routing artık
bu DFA'ya bağlı OLMASA da DFA'nın kendi soundness'ı için tutuldu. Agregat-store (`dış[i]=arr`) ve A1 DELIK
testleri CAGIRAN'a güncellendi (ASLA ITERASYON invaryantı korunur).

**Yakalanan bug (init):** `EscapeKayit.kesin_yerel` `kayit_ekle`'de sıfırlanmıyordu → garbage truthy →
HER dizi yönlendi (batch 43 UAF). `k->kesin_yerel = 0` ile düzeldi. (Adversarial gate + uninit-init
disiplini yakaladı.)

**C ve self-host SOUND (R1 gevşedi):** C-tarafı confined dizileri serbest eder; self-host `codegen.kem`
ρ_yerel ÜRETMEZ (her şey ρ_caller = konservatif-sound). İkisi de hiçbir kaçanı serbest etmez.

**DOĞRULAMA:** `parser.kem` bootstrap FIXPOINT ✓ + `codegen_diff` 70/70 ✓ + **48/48 routable repro
ASan-temiz (18 orijinal UAF'ın TAMAMI kapandı)** + confined `dizi_al` döngüsü GERÇEK free + ASan temiz
(exit 42) + `test_tumu` YEŞİL + DRF 39/39 + escape 22/22 + bolge_atama 15/15. Kalan 2 repro UAF
(`probe_bare`, `probe_p1_ekle_inline`) F4.2b'den BAĞIMSIZ PRE-EXISTING bug (bare-literal→`dizi_*`
stack/heap descriptor uyuşmazlığı; machinery commit `f9ad67a`'da da çöküyor) → ayrı task'a flaglendi.

**Sınırlar (v1):** Gerçek-serbest yalnız C-tarafı + skaler-eleman + confined-değişken. Geniş ama sound
desenler (kaçan dizi, ptr-eleman, alias, capture) ρ_caller'da kalır (free kaçırılır ama UAF imkânsız).
Genişletme (ptr-eleman transitif free, daha keskin alias, self-host port) sonraki iş.

---

## D-102 — [YÜKSEK] Loop-carried soundness: escape/bölge ESC_ITERASYON ÜRETMEZ (güvenli geri-çekilme) (2026-06-20)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `1994a88` → D-101 ayrılmıştı; main D-101'i F4.2a'ya verdi → KONSOLİDASYON: D-102'ye yeniden numaralandı).

**Karar [ETKİ: ORTA — yalnız `src/escape.c` + `src/bolge_atama.c` + testler; codegen/checker/IR
DEĞİŞMEZ; izole commit; PR, merge edilmedi].** Escape analizi ARTIK **hiçbir** tahsisi `ESC_ITERASYON`
(= bölge sisteminde `BOLGE_ITERASYON`, **EN KISA** ömürlü bölge: F4.3'te iterasyon-başına serbest
bırakılacak) işaretlemez; döngü içindekiler dahil **tüm tahsisler `ESC_YEREL`** (daha uzun ömürlü =
güvenli). F4.2a/codegen'den tamamen bağımsız.

**Delik (loop-carried UAF):** Eski kod, bir döngü gövdesinde oluşan HER tahsisi **koşulsuz**
`ESC_ITERASYON` işaretliyordu (escape.c join'inde + bolge_atama.c syntax-fallback'inde). Tahsis
iterasyonu AŞIYORSA (döngü-dışı bir yere bağlanıp sonra kullanılıyorsa) ama `ver`'lemiyorsa →
`ITERASYON` kalıyordu. F4.3'te ρ_iterasyon iterasyon-başına serbest bırakılınca **canlıyken serbest =
UAF**. Şu an BOXED (F4.3 yok) ama F4.3'ten ÖNCE kapatılmalıydı.

**Neden DETECTION değil GERİ-ÇEKİLME (önemli — orijinal plan değişti):** İlk yaklaşım "iyimser default'u
tersine çevir + kanıtlanmış iterasyon-yerelleri ITERASYON'a indir" (iterasyon-kaçtı bayrağı + post-pass)
idi. Bu yaklaşımın sağlamlığı, kaçış rotalarını KAPATAN **kapılara KOŞULLUYDU**: D-007 (diziler
skaler-eleman → dış agregaya referans saklanamaz) ve R-GÖMME (kaçan agregaya gömülü heap-ref yok).
**Çok-ajanlı adversarial review (4 bağımsız lens + adjudikasyon, uçtan uca `--check`/`--llvm`/ASan ile
doğrulandı) bu kapıların ENFORCE EDİLMEDİĞİNİ kanıtladı:**
- `Dizi<Dizi<T>>`, `Dizi<metin>` ve `Dizi` alanlı yapı tipleri tip-kontrolden GEÇER ve codegen'de
  **by-ref `KdlDizi*`/`ptr` eleman** olarak lower edilir (D-007 yalnız STRUCT-VALUED dizi-elemanı
  codegen ertelemesi; skaler/ptr eleman çalışır).
- Tip sistemi `nesne.alan = x` (`DUGUM_ERISIM`) ve `dizi[i] = x` (`DUGUM_INDEKS`) lvalue'lerini kabul
  eder (`tip_kontrol.c:4545-4549`). Bir döngü-tahsisini `dış[i] = tahsis` / `nesne.alan = tahsis` ile
  dış (iterasyonu aşan) bir agregaya **by-ref** koymak gerçek bir kaçış rotasıdır; sentaktik tespit
  bunu (`DUGUM_TANIMLAYICI` dışı lvalue) kaçırınca **under-approximation = gizli UAF** olur.
Bu rotaları (ve gömme/closure varyantlarını) sağlamca kapsamak, her kaçırılan rotanın UAF olduğu bir
analizde sınırsız "tüm rotaları yakaladım mı?" riski taşır. Üstelik per-iterasyon optimizasyonunun
ŞU AN HİÇ tüketicisi yok (F4.3 yok; analiz codegen'e unwired). Bu yüzden direktifin AÇIKÇA izin verdiği
**güvenli geri-çekilme** seçildi: ITERASYON'u hiç üretme. Bu, herhangi bir kapıya bakılmaksızın
**trivially sağlam** (ITERASYON hiç üretilmezse iterasyon-başına serbest hiç olmaz → loop-carried UAF
**imkânsız**). Per-iterasyon optimizasyonu F4.3'e (gerçek bölge-serbest semantiği + kapılar enforce
edilince ya da tüm rotalar kapsanınca) ertelenir.

**Değişiklik:**
- `escape.c`: alloca-literal visit'inde **koşulsuz loop→ITERASYON marking'i KALDIRILDI**; yalnız kayıt
  oluşturulur (default `ESC_YEREL`). Analiz `ESC_ITERASYON` ÜRETMEZ. `ver`→`ESC_CAGIRAN` yolu DEĞİŞMEDİ
  → `ESC_CAGIRAN` seti **byte-identik** (G005 closure tespiti `== ESC_CAGIRAN`'a bağlı, etkilenmez).
- `bolge_atama.c`: syntax-fallback'teki koşulsuz `dongu_derinligi>0 → aktif_iterasyon` KALDIRILDI →
  döngü tahsisi `BOLGE_YEREL`. `escape_to_bolge`'daki `ESC_ITERASYON → aktif_iterasyon` eşlemesi KORUNUR
  ama **şu an ULAŞILMAZ** (escape ITERASYON üretmez); F4.3 yeniden etkinleştirirse doğru kalır.
- `escape.h`/`bolge.h`: `ESC_ITERASYON`/`BOLGE_ITERASYON` enum'ları API/gelecek için KORUNUR.

**Sağlamlık modeli:** Şüphede DAİMA uzun ömürlü. Under-approximation (yanlış ITERASYON) = gizli UAF =
KABUL EDİLEMEZ. Over-approximation (tüm döngü tahsisleri YEREL = kaçırılan per-iterasyon optimizasyonu)
= SORUN DEĞİL. Soundness argümanı artık TEK CÜMLE: *escape analizi ESC_ITERASYON üretmez.*

**🔗 F4.3 İÇİN NOT (per-iterasyon optimizasyonu geri açılırken):** ITERASYON'u tekrar üretmeden ÖNCE,
bir döngü-tahsisinin iterasyonu aşma rotalarının TAMAMI kapsanmalı: (a) `ver`→CAGIRAN; (b) daha-sığ
değişkene bağlanma; (b2) **agrega-lvalue store** (`dış[i]=x` / `nesne.alan=x`); (b3) **agregaya gömme**
sonra agrega kaçışı (`dış = Yapı{f: x}`, `ver Yapı{f: x}`); (c) çağrı argümanı/sonucu; (d) closure
yakalama (G005 kaçan closure'ı reddeder — enforce EDİLİR). VEYA kapıları (D-007 referans-eleman reddi,
R-GÖMME) önce enforce et. **loop-carried per-iterasyon optimizasyonu, D-007 + R-GÖMME enforcement'ına
bağımlıdır.**

**Doğrulama:** `test_escape` 17→22 (T8 ITERASYON→YEREL; +5 geri-çekilme testi: dış-skaler-store,
**dış-dizi-eleman-store [b2 DELİK]**, **dış-yapı-alan-store [b2 DELİK]**, döngü-ver→CAGIRAN, umbrella
"hiçbir tahsis ITERASYON değil" invariant'ı). `test_bolge_atama` 13→15 (döngü→BOLGE_YEREL, b2
dış-dizi-eleman→BOLGE_YEREL [DELİK], syntax-fallback→YEREL). **Teeth kanıtlandı:** eski koşulsuz
loop→ITERASYON geri enjekte edilince tam 5 geri-çekilme assertion'ı (b2 delik testleri dahil)
BAŞARISIZ. `tip_kontrol` 184/184 (G005 değişmedi), `llvm` 235/235, **test_tumu yeşil ("Tum testler
gecti!")** + self-host FIXPOINT korundu. 0 ASan, 0 uyarı (clang+gcc strict). Geri-çekilme kararı,
çok-ajanlı adversarial review'in (uçtan uca `--check`/`--llvm`/ASan ile) gerçek b2/b3 UAF rotalarını
doğrulaması üzerine alındı — direktifin "güvenli geri-çekilme: ITERASYON'u hiç üretme" yolu.

---



## D-101 — [YÜKSEK] V2 F4 FAZ 2a: region-passing ABI (ρ) — kullanıcı-fn + dizi helper'ları (re-scoped) (2026-06-17)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `1994a88` → en yüksek D-100 → D-101 ayrıldı).

**Karar [ETKİ: YÜKSEK — `src/llvm.c` + `selfhost/codegen.kem` (İKİ-DERLEYİCİ MİRROR) + `kdl_runtime.c`;
izole commit].** Region-passing ABI'nin İLK adımı: her KULLANICI fonksiyonu ilk param `ptr %rho`
(bölge) alır, her kullanıcı-fn çağrısı ρ geçirir, DİZİ allokasyon helper'ları ρ alır, tüm dizi
tahsisi → geçirilen ρ_caller. AMAÇ: uniform ρ ABI iskeleti + iki-derleyici mirror + YENİ
self-host FIXPOINT'i kararlılaştırmak. Serbest + gerçek YEREL/CAGIRAN ayrımı = F4.2b/F4.4.

**RE-SCOPE (orchestrator kararı):** İlk tasarım (her şey ρ: metin + closure-env-malloc + bölge_al +
tüm helper'lar) ~50 byte-kritik edit + çok-iterasyonlu konverjans gerektiriyordu. Mirror yüzeyini
küçültmek için **kullanıcı-fn + DİZİ helper'ları + lambda imzaları + fat-value dispatch** ρ-threaded
edildi; **metin (kdl_metin_*), closure-env-malloc, bölge_al inline-malloc global'de KALDI** (F4.1
davranışı — `kdl_global_bolge_al` fallback). **Lambda/dispatch ρ yalnız `src/llvm.c`'de** —
`codegen.kem`'de fat-value/lambda YOK (`parse_lambda` salt parser-fn), dolayısıyla mirror edilmedi
ve fixpoint etkilenmedi.

**Tasarım:**
- ρ = adlı param `%rho`, LİTERAL geçirilir (alloca YOK) → gövde reg numaraları DEĞİŞMEZ.
- main HARİÇ her kullanıcı-fn: `define <ret> @f(ptr %rho, ...)`. Kullanıcı-fn çağrısı `f(%rho, ...)`.
- **main:** ρ param almaz (libc çağırır); gövde başında `%r = call ptr @kdl_global_bolge_al()` seed +
  `rho_ref` ile çağrılara geçer.
- **lambda (fat-value hedefi):** ρ İLK param: yakalamasız `@l(ptr %rho, args)`, yakalamalı
  `@l(ptr %rho, ptr %env, args)`. Gövdesi geçirilen ρ'yu kullanır. Böylece üst-düzey-fn (ρ-ABI) ile
  lambda fat-value dispatch'te ABI-uniform.
- **fat-value indirect dispatch:** her iki dal ρ geçirir — bare `fn(ρ, args)` (üst-düzey-fn-değer
  ya da yakalamasız-lambda), closure `fn(ρ, env, args)`. ρ = çağıranın `rho_ref`'i. Bu, stdlib
  yüksek-mertebe fn'lerini (harita/filtre/indirgeme — fn'i DEĞER geçirip indirect çağırır) ρ-doğru
  kılar; aksi halde üst-düzey-fn ρ-ABI iken dispatch ρ'suz → ABI uyumsuz (test_llvm 59/60/61 ✗).
- Dizi helper'ları (`kdl_dizi_olustur/ekle_{tam,tam64,ptr,yapi}/kapasite_ayarla`) ρ ilk param +
  `kdl_bolge_ayir(ρ,...)`. Non-alloc (al/yaz/boyut) + metin + yazdir ρ ALMAZ.
- Sınıflandırma: çağrı yerinde "kullanıcı-fn mı?" (`ik!=NULL` / codegen.kem `kdl==""`) → ρ; built-in → ρ yok.

**FIXPOINT'in DOĞASI (kritik anlayış):** bootstrap "fixpoint" = **stage1 == stage2** ve İKİSİ DE
SELF-HOST çıktısı (codegen.exe vs codegen2.exe) → self-host İDEMPOTANSI. llvm.c↔codegen.kem
BYTE-eşitliği DEĞİL. codegen_diff ise SEMANTİK (exit-kod) eşdeğerlik. Dolayısıyla codegen.kem'in
ρ-emit'i llvm.c ile byte-eşleşmek zorunda DEĞİL — yalnız DOĞRU + deterministik olmalı (stage1==stage2
otomatik). Bu, mirror'ı çok daha tractable yaptı (reg-numara eşleştirme kaygısı moot).

**KAPSAM / RESIDUAL:**
- Bölge HİÇ serbest bırakılmaz (status-quo leak; deterministik serbest = F4.4).
- ρ_yerel YOK — her tahsis ρ_caller (global'den seed). Gerçek YEREL/CAGIRAN escape ayrımı = F4.2b.
- metin (kdl_metin_*), closure-env-malloc, bölge_al inline-malloc ρ ALMAZ (global; F4.1) — bunlar
  TAHSİS noktaları, ρ-threading'den ayrı; F4.2b/F4.4 ele alır.
- Fat-value indirect ABI uyumsuzluğu (top-level-fn-değer ρ-ABI iken dispatch ρ'suz) ÇÖZÜLDÜ:
  lambda imzası + dispatch'in iki dalı da ρ alır (yalnız llvm.c; codegen.kem'de fat-value yok).

**Doğrulama (hepsi YEŞİL):** `test_tumu` → **"Tum testler gecti!"** (rc=0). test_llvm **235/235**
(stdlib harita/filtre/indirgeme dahil — fat-value ρ-dispatch düzeltmesiyle). Bootstrap:
LEXER/PARSER/CHECKER **51/51/51 birebir, 0 fark**; **CODEGEN FIXPOINT stage1==stage2 (21967 satır)
BİREBİR ✓** (ρ-threaded). self-host --parse 12/12, --check 48/48; codegen semantik eşdeğerlik
**58/58** (kemgu_self + kemgu_self2). ASan E2E: **PASS=97 FAIL=0** (serbest yok → UAF yok; leak F4.1
ile aynı). 0 derleyici uyarısı.

**Build-env notu (reviewer):** bootstrap/codegen harness'ları `mktemp -d` kullanır. MSYS2 `mktemp`
`/tmp`'i `C:\msys64\tmp`'e, git-bash ise `AppData\Local\Temp`'e çözer; PATH karışırsa harness
"stage1.ll No such file" ile YANLIŞ-NEGATİF verir (kod sorunu DEĞİL). Çözüm: testleri tutarlı bir
MSYS2 kabuğunda çalıştır veya `TMPDIR`'i `/c/`-köklü bir yola sabitle (her iki kabuk aynı çözer).

## D-100 — [YÜKSEK] V2 F4 FAZ 1: sızan array/metin tahsisini global bölgeye yönlendir + sembol-çakışması temizliği (2026-06-17)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `75912a2` → en yüksek D-099 → D-100 ayrıldı).

**Karar [ETKİ: ORTA — yalnız `runtime/kdl_runtime.c`; codegen/checker/IR DEĞİŞMEZ; izole commit].**
Sızan (çağırana dönen + hiç free edilmeyen) array ve yeni-metin tahsislerini F4.0 global bölgesine
(`kdl_bolge`) yönlendirir. Codegen helper imzaları DEĞİŞMEDİĞİ için IR aynı → **FIXPOINT byte-identik**;
mirror yok (saf C runtime). Bölge HİÇ serbest bırakılmaz (status-quo leak; deterministik toplu serbest = F4.4).

**Sembol-çakışması temizliği (ön-blokör — orchestrator onayıyla çözüldü):** `kdl_runtime.c` ZATEN
`kdl_bolge_olustur/ayir/serbest(+toplam_byte)` tanımlıyordu = `bölge_al` için ESKİ `KdlArena`
(chunk-bump, int32). AMA `bölge_al` codegen'i inline `@malloc` kullanıyor (llvm.c:2726) → KdlArena
**TAMAMEN ÖLÜ** (sıfır çağıran, derlemeyle doğrulandı). F4.0 aynı isimleri farklı imzayla almıştı →
gizli çakışma (`#include "kdl_bolge.h"` → `conflicting types` derleme hatası). **Çözüm:** ölü KdlArena
kümesi (KdlArena/KdlArenaChunk + kdl_bolge_olustur/ayir/serbest/toplam_byte + kdl_bolge_metin_birlestir,
hepsi sıfır-çağıran) SİLİNDİ; F4.0'ın `kdl_bolge.c`'si dosya sonuna `#include "kdl_bolge.c"` ile GÖMÜLDÜ
→ `kdl_runtime.o` allokatörü kendi içinde taşır, **harness link satırları DEĞİŞMEZ**. Standalone
`kdl_bolge.o` yalnız F4.0 birim testinde linklenir; hiçbir hedef ikisini birden linklemez → çift-sembol yok.

**Yönlendirilen sızan tahsisler:**
- `kdl_dizi_olustur` descriptor → bölge.
- `kdl_dizi_ekle_{tam,tam64,ptr,yapi}` büyüme: `realloc` → `kdl_dizi_buyut()` (bölgeden yeni tampon +
  CANLI `boyut*eb` memcpy; eski tampon bölgede sızar). **`kdl_dizi_kapasite_ayarla` DE** dönüştürüldü
  (zorunlu invaryant: d->veri bölge-sahipli → realloc'a geçmek UB/çökme olurdu).
- Yeni-metin döndüren `kdl_metin_*` + `kdl_ondalik_bicimle` + `kdl_tam_to_metin` (12 nokta) → bölge.
- `kdl_dizi_serbest` NÖTR (no-op) — d artık bölge-sahibi, `free()` çökme olurdu; codegen zaten emit
  etmiyordu (ölü, dizi hep sızıyordu).

**DOKUNULMAYAN:** dosya/kripto geçici tamponları (malloc…free çiftli — bölgeye alınsa sızıntı YARATIRDI),
eşzamanlılık (kdl_gorev/kanal — D-008, çağrılmıyor), `kdl_bellek_hizali_*`, derleyicinin kendi
`src/arena.c`'si (ayrı, compile-time). bölge_al / closure-env / intrinsic inline `@malloc`'ları = F4.2.

**Doğrulama:** ASan/UBSan smoke (1000-eleman geometrik büyüme değerleri doğru → büyüme-memcpy doğru;
kapasite_ayarla reserve; metin birleştirme; bakiye=1). `mingw32-make test_tumu` → "Tum testler gecti!"
+ **FIXPOINT byte-identik** (self-host kendini bölge-tahsisiyle derleyip aynı IR üretiyor = allokatör +
büyüme deseni gerçek yük altında doğru). ASan E2E ~97/0 (büyüme memcpy + d->veri güncelleme bellek-temiz;
eski tampon bölgede serbest değil → UAF yok). `kdl_bolge_bakiye()` çıkışta 1 (global bölge, kasıtlı
hiç-serbest — beklenen). 0 uyarı.

## D-099 — V2 F4 FAZ 0: bölge (region) arena allokatörü runtime (`kdl_bolge`) (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `7166880` → en yüksek D-098 → D-099 ayrıldı).

**Karar [ETKİ: DÜŞÜK — saf runtime; codegen/checker/IR DEĞİŞMEZ; izole commit].** Bölge tabanlı
bellek modelinin (`belgeler/KEMGU_Bellek_Modeli.md`, Katman 1) runtime tabanı. Bir BÖLGE = bir
ARENA: malloc'lu blok tek-yönlü listesi + blok-içi bump pointer. Tahsis O(1) bump; bölge
kapanışında TÜM bloklar tek seferde free (O(blok)). GC yok — deterministik serbest. **Bu fonksiyonları
henüz kimse çağırmaz** (F4.1'de lambda env + dizi/metin tahsisi buraya bağlanır; F4.2'de
region-passing ABI — bölge `ptr` parametresi). Soundness + FIXPOINT'ten tamamen bağımsız.

**API (`runtime/kdl_bolge.h` + `.c`):**
- `KdlBolge *kdl_bolge_olustur(void)` — handle + ilk blok (64 KB) malloc'la; OPAK ptr döner
  (F4.2 region-passing'de `ptr` param).
- `void *kdl_bolge_ayir(KdlBolge *b, uint64_t n)` — 16-bayt hizalı bump; aktif blokta yer yoksa
  yeni blok (boyut = max(64KB, n+16) → oversized'a adanmış blok).
- `void kdl_bolge_serbest(KdlBolge *b)` — tüm bloklar + handle free (O(blok)).
- Sızıntı-tanığı (Windows'ta LSan yok): `kdl_bolge_olustur_sayisi`/`kdl_bolge_serbest_sayisi`
  global sayaçları + `int kdl_bolge_bakiye(void)` (oluştur−serbest; 0 = sızıntı yok).

**Tasarım inceliği — hizalama:** esnek dizi (FAM `veri[]`) ofseti platforma göre 16-hizalı
OLMAYABİLİR (64-bit'te header 24 bayt → veri 8-hizalı). Bu yüzden hizalama derleme-zamanı ofsetine
değil ÇALIŞMA-ZAMANI ADRESİNE (uintptr_t) göre yapılır → her platform/header düzeninde 16-hizalı.
Yeni blok kapasitesi `hn + 16` ile en kötü hizalama payını garanti eder. Tüm aritmetik taşma-korumalı
(hiza_yukari wrap guard, `kap < hn` guard, `SIZE_MAX - sizeof(header)` malloc guard).

**KAPSAM / SINIR:**
- Tahsisler TEK TEK serbest EDİLMEZ — yalnız bölge topluca (dizi/metin "leak OK" status-quo ile
  aynı sınıf; bölge modeli deterministik serbest'i ZATEN sağlıyor: bölge kapanınca hepsi gider).
- Bare-metal: host malloc/free üstüne; `KEMGU_BARE_METAL` altında kdl page-allocator'a bağlanması
  TODO (bloklamaz — bare-metal bu dosyayı henüz derlemiyor).
- Tek-thread host; sayaçlar atomik değil (concurrency Katman 2).
- Makefile: `kdl_bolge.o` (plain, F4.1 link'i için) + `test_kdl_bolge` (ASan/UBSan birim test);
  codegen/test emit hedeflerine DOKUNULMADI.

**Adversarial inceleme:** çok-ajanlı (alignment · overflow · memsafety · caplogic · UB/port ·
test-gaps; her bulgu ayrıca doğrulandı) → **0 doğrulanmış correctness/safety açığı**. Doğrulayıcılar
tüm korumaları (runtime-adres hizalaması, `hn<n`/`kap<hn`/`SIZE_MAX` taşma guard'ları, free-all
zincir-yürüyüşü, NULL guard'ları) izleyip teyit etti. İnceleme önerileriyle SERTLEŞTİRME:
`kdl_bolge_blok_sayisi()` teşhis erişimcisi eklendi (büyümeyi blok-sayısıyla KESİN doğrula; eski
write-only alan artık kullanılıyor) + NULL-handle ve taşma-reddi (UINT64_MAX → NULL) testleri.

**Doğrulama:** birim test **33/33**, ASan/UBSan TEMİZ (blok-içi · büyüme [blok sayısı 1→2] ·
oversized 128KB [adanmış blok] · free-all · bakiye=0 · hizalama 1..33 bayt + uint64/16-bayt tür ·
1000 yoğun tahsis örtüşmez · NULL-handle no-op · UINT64_MAX taşma reddi). `mingw32-make test_tumu`
→ "Tum testler gecti!" (codegen değişmedi → IR/FIXPOINT trivial korunur; yeni fonksiyonlar
referanssız ölü kod). ASan E2E 97/0 (yeni runtime'ı kullanan yok). 0 uyarı.

## D-098 — [YÜKSEK] V2 FAZ 2: yakalayan closure env'i stack→HEAP (@malloc) — kaçışta yaşar (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `099cd5e` → en yüksek D-097 → D-098 ayrıldı).

**Karar [ETKİ: ORTA — `src/llvm.c` lambda env allokasyonu; izole commit; C-codegen-only].**
V2 yol haritası 2. fazı (F1 üzerine). Yakalayan closure'ın **env ALLOKASYONUNU** stack `alloca`'dan
**heap `@malloc`**'a çevirir → env, oluşturan frame'i aşsa bile yaşar (kaçış UAF'inin KÖKÜ kapanır).
F1'in fat-value temsili ve env-null dispatch'i DEĞİŞMEZ.

**Değişiklik (tek nokta — lambda materyalizasyonu, DUGUM_LAMBDA):**
- Yakalayan lambda env'i: `%env = alloca %envtip` → `%env = call ptr @malloc(i64 ptrtoint
  (ptr getelementptr (%envtip, ptr null, i32 1) to i64))`. Boyut = LLVM constexpr sizeof
  (D-087 GEP-null idiomu; padding/alignment LLVM layout'uyla birebir).
- Capture store'ları (GEP+store) ve `{@lambda_N, %env}` insertvalue'su DEĞİŞMEDİ — %env artık heap
  ptr; GEP/store/insertvalue ptr üzerinde stack/heap-agnostik.
- **Allokatör seçimi:** `@malloc` (dizi/metin runtime'ının nihai allokatörü). Tutarlı + F4
  region-dealloc tek noktadan (env+dizi+metin) bağlanabilir.

**DOKUNULMAYAN:** çağrı dispatch (env-null; stack/heap fark etmez), lifted @lambda_N (env'i ptr okur),
yakalamayan closure ({@lambda_N, null}), top-level fn ({@f, null}).

**KAPSAM / SINIR:**
- **SERBEST BIRAKMA YOK** — env malloc'u hiç free edilmez (LEAK). Bilinçli: dizi/metin
  (`runtime/kdl_runtime.c` "leak OK") ile AYNI sınıf status-quo; deterministik region-dealloc = F4.
- **UNCONDITIONAL heap:** non-escaping dahil tüm capturing closure env'i heap. Escape-driven
  stack/region optimizasyonu (kaçmayan → yerel bölge) F4/region işi.
- **KAÇIŞ HÂLÂ G005 ile REDDEDİLİR** (F5'e dek). F2 env'i güvenli kılar ama özelliği AÇMAZ;
  davranış-eşdeğer (non-escaping closure'lar AYNI sonuç). UAF-fix LATENT (F5'te G005 kalkınca aktif).
- Yan not: capturing lambda DÖNGÜ içindeyse her iterasyon artık taze env malloc'lar (önceki
  hoist'lu stack alloca iterasyonlar arası PAYLAŞILIYORDU → closure-per-iteration için daha doğru).
  Korpusta döngü-içi capturing lambda yok → korpus davranışı birebir.

**FIXPOINT güvenliği:** self-host `.kem` fn-değer/lambda kullanmıyor → codegen.kem IR'ı etkilenmez →
bootstrap byte-identik korunur (F1 ön-kontrolüyle aynı).

**Doğrulama:** lambda E2E 5/5 (10_lambda, 04_islev, 42_lambda_hesap, 25_closure_capture,
43_closure_param) → exit 42 (artık HEAP env ile); IR teyidi: env = `@malloc(... sizeof envtip ...)`.
`mingw32-make test_tumu` → "Tum testler gecti!" (FIXPOINT korunur; --check/G005 değişmedi).
ASan E2E PASS=97 FAIL=0 (env leak'i dizi/metin leak'iyle aynı sınıf; leak-detection kapalı). 0 uyarı.

## D-097 — [YÜKSEK] V2 FAZ 1: fat-value closure ABI iskeleti — fn değeri {ptr fn, ptr env} + runtime env-null dispatch (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı origin/main `f938741` → en yüksek D-096 → D-097 ayrıldı).

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit; DAVRANIŞ-EŞDEĞER;
C-codegen-only].** V2 (escaping-closure desteği) yol haritasının 1. fazı. Fonksiyon-değeri
temsilini tek-tipleştirir; D-071'in deneyip-bozduğu uniform-ABI tuzağını **yapısal olarak** eler.
KAÇIŞ HENÜZ GÜVENLİ DEĞİL (env hâlâ STACK → heap F2); G005 reddi KALIR (F5'te kalkar).

**Tasarım (B-i, v2_tasarim_plani.md onaylı):**
- `işlev(...)→R` IR lowering: `ptr` → **`{ ptr, ptr }`** (2-word first-class fat value).
  `ast_tip_to_ir` TIP_ISLEV → `{ ptr, ptr }` → değişken/param/dönüş/alan/dizi-eleman hepsi jenerik
  olarak fat value taşır.
- **Materyalizasyon:** top-level fn değeri → `{@f, null}` (insertvalue); yakalamayan lambda →
  `{@lambda_N, null}`; yakalayan lambda → `{@lambda_N, %env}` (env F1'de HÂLÂ STACK alloca).
- **Çağrı dispatch:** fat değerden `extractvalue` fn+env → `icmp eq ptr %env, null` → dallan:
  bare `call R %fn(args)` / closure `call R %fn(ptr %env, args)` → slot-deseniyle birleştir
  (phi yerine mevcut bellek-slot idiomu). **Compile-time `closure_mu`/`son_closure` tag'leri
  KALDIRILDI** — "closure mu?" artık DEĞERİN PARÇASI (env-null), kaçışta kaybolmaz.

**D-071 tuzağı neden artık imkânsız:** D-071'in uniform denemesi çağrı yerini "daima closure-unpack"
yapıp bare fn-ptr'ı `{fn,env}` sanıyordu (→ `harita(xs, iki_kat)` çöp). Burada bare ve closure AYNI
fat-value şeklini paylaşır; çağrı yeri env==null ile runtime'da ayrışır → bare fn doğal imzayla
çağrılır, sarma/thunk gerekmez. Temsil-uyumsuzluğu yapısal olarak ortadan kalkar.

**FIXPOINT güvenliği (ÖN-KONTROL):** self-host `.kem` kaynakları (lexer/parser/checker/codegen)
**fn-DEĞER kullanmıyor** (fn-tipli param/dönüş/alan YOK; lambda literal YOK — yalnız yorumlarda).
→ C compiler bu kaynakları derlerken TIP_ISLEV yolu hiç tetiklenmez → kemgu_self IR'ı DEĞİŞMEZ →
self-host bootstrap byte-identik (stage1==stage2 korunur). Salt C-codegen değişikliği.

**KAPSAM (F1 = yalnız iskelet):**
- env HÂLÂ STACK alloca (heap promosyonu = F2); kaçan yakalayan closure HÂLÂ UAF olur → G005
  reddi bu yüzden KALIR (F5'e dek kaldırılmaz; savunma derinliği).
- Davranış-eşdeğer: korpus AYNI çıktı/exit. Yeni özellik (kaçış) AÇILMAZ.
- Yakalamalı lambda'yı işlev-param'a geçirme (D-071 KAPSAM-DIŞI) artık ÇALIŞIR (fat value + runtime
  dispatch) — F1'in yan kazanımı; ama escape G005 ile sınırlı.

**Doğrulama:** lambda E2E **5/5** → exit 42, ASan temiz: 10_lambda, 04_islev,
42_lambda_hesap (D-071-kritik: top-level/yerel-lambda → işlev-param, env==null yolu),
25_closure_capture (yakalayan, env!=null yerel), **43_closure_param (YENİ: yakalayan closure →
işlev-param, env!=null param yolu — D-071 KAPSAM-DIŞI item)**. SELF-HOST bootstrap: lexer 51/51,
parser 51/51, **CODEGEN FIXPOINT stage1==stage2 byte-identik**. `mingw32-make test_tumu` →
"Tum testler gecti!"; ASan E2E PASS=96 FAIL=0; --check/--checkdump 48/48. 0 uyarı.
> Not: test makinesinde `mktemp`/`/tmp` MSYS mount'u ara sıra bozuk → bootstrap harness'ı flaky
> (kod-dışı). TMPDIR yazılabilir dizine ayarlanınca geçer; fixpoint byte-identik ayrıca elle kanıtlandı.

## D-096 — [YÜKSEK] V1 kaçan-closure UAF reddi (G005): YAKALAYAN ∧ KAÇAN closure compile-time reddedilir (2026-06-16)

> **D-no:** merge anında güncel main'in en yüksek D-numarasına göre kesinleştir
> (branch tabanı: origin/main 68f1fb0 → en yüksek D-095, dolayısıyla D-096 ayrıldı).

**Karar [ETKİ: `src/tip_kontrol.c` checker — yeni redd kodu G005; izole commit; C derleyici
codegen DEĞİŞMEDİ].** Güvenlik-iddiası izi (D-071 devamı). Kaçan yakalayan closure açığı
(`kacan_closure_kapsam.md` kapsam analizi) **"tehlikeli kodu derleyemezsin"** diyerek kapatıldı.

**Açık (D-071'de belgeli ama ZORLANMAYAN boşluk):** Yakalayan lambda → closure `{ptr fn, ptr env}`;
hem `env` (alloca `src/llvm.c:3758`) hem `{fn,env}` çifti (alloca `src/llvm.c:3805`) **STACK**'te.
Closure frame'i aşarsa (`ver` ile dönüş / frame-aşırı saklama) → env dangling = **UAF**; ayrıca
`closure_mu` tek yerde set edildiğinden (`src/llvm.c:4008`) kaçışta kaybolup çağrı yerinde
mis-dispatch. D-071 KAPSAM-DIŞI listesi bunu *"lambda escape (env stack — şu an non-escaping KEMGU
v1 garantisi)"* diye işaretlemişti — yani checker'la **zorlanmayan** bir yorum-garantisi. #1
"Kırılamaz Güvenlik" ihlali (analog dizi-deliği D-070'te koşulsuz heap-promote ile düzeltilmişti).

**Mekanizma — escape.c yeniden kullanımı (hedefli kontrol yerine):**
- `src/escape.c` (forward DFA fixed-point escape analizi) ZATEN `DUGUM_LAMBDA`'yı alloc-site izliyor
  ve `ver` (+ transitif atama zinciri + koşullu dal) ile dönen lambda'yı **`ESC_CAGIRAN`**
  işaretliyor. `escape.o` ZATEN ana ikiliye linkli (Makefile SRCS) ama `ana.c`'de çağrılmıyordu →
  **ÖLÜ ALTYAPI**. Bu commit onu ilk kez tüketir.
- **Bağlama:** `tip_kontrol_tanim` `DUGUM_ISLEV` kolu, gövde kontrolünden önce `escape_analiz_islev`
  çalıştırır (per-işlev; `escape_baslat`/`escape_serbest` dengeli → ASan temiz), sonucu
  `tk->aktif_escape`'e koyar.
- **Yakalama bilgisi** checker'da hesaplanır: `genel_yakalama_kontrol` (codegen'in
  `lambda_serbest_tara`'sı ile birebir — yalnız ÇEVRE lokal/param yakalama sayılır; global
  işlev/sabit/tip ve lambda-içi/gölgeleme sayılmaz). Lineer (LC-2) + lineer-olmayan yakalamayı kapsar.
- **Redd (G005):** lambda case'te `(yakaladi_genel ∧ escape_kategori(d)==ESC_CAGIRAN)` → `tip_hata`.
  Sadece (yakalıyor ∧ kaçıyor) reddedilir.

**Over-reject guard (testli — reddedilMEZ):** yakalamayan lambda return (bare fn-ptr, env yok);
yakalayan closure fonksiyon-içi çağrı (`ver arttir(10)` = `25_closure_capture.kem` deseni);
yakalayan closure çağrılır + sonuç saklanır, kaçmaz. **Pozitif:** `ver ||cap` / transitif
`f=||cap; ver f` / lineer `ver c` → G005.

**KAPSAM:** C-checker only — self-host lambda yapmıyor → port **moot** (G004 ile aynı; iki self-host
checker lambda görmez, fixpoint tetiklenmez). Korpus etkisi: **0 program G005 tetiklemez** (firsthand
doğrulandı: 25/10/42 lambda örnekleri yakalayan-closure KAÇIRMAZ; `p1_05` lambdaları yakalamasız +
parse-only; `lineer_closure.kem` fonksiyon-içi) → `--check`/`--checkdump` divergens YOK.

**RESIDUAL (V1 kapsamı dışı — bilinen kalan, follow-up):**
1. **Agregat gömme:** yakalayan closure'ı DÖNEN dizi/yapı içine gömme (`ver [|| b]`) — escape.c
   `ifadeyi_yukselt` agregat ELEMAN/ALAN alt-escape'ini izlemiyor (escape.h v1 sınırı) → **GERÇEK
   UAF, yakalanmıyor.** Yapı-alanı function-tip ayrıca parse etmiyor (P020) → o yol bugün erişilemez;
   dizi yolu erişilebilir + açık. Follow-up: escape.c v2 agregat alt-escape recursion'u VEYA V2 heap-env.
2. **Param-geçişi:** `al(|| b)` — escape.c interprocedural değil → yakalanmıyor. Ancak senkron
   çağrıda çevre frame canlı kaldığından **temiz UAF DEĞİL**; asıl risk callee'nin saklaması (interproc)
   + D-071 mis-dispatch (closure_mu kaybı). D-071'de zaten V2'ye ertelenmiş.

Kalıcı/tam çözüm: **V2** (heap/uniform env + bölge runtime + uniform self-describing closure temsili) —
ayrı kampanya (bkz. `belgeler/KEMGU_Bellek_Modeli.md` R-YAKALAMA-ESCAPE/THREAD).

**Doğrulama:** `test_tip_kontrol` 184/184 (6 yeni G005: 3 pozitif + 3 guard), ASan/UBSan TEMİZ.
`mingw32-make test_tumu` → "Tum testler gecti!" (fixpoint stage1==stage2 korunur; `--check`/`--checkdump`
korpus divergens yok; 4 lambda E2E korunur). 0 uyarı.

## D-095 — [YÜKSEK] Self-host codegen `güvensiz { }` bloğu — sessiz düşme (accept-but-miscompile) kapatma (2026-06-16)

**Karar [ETKİ: self-host codegen doğruluk; izole commit].** `selfhost/codegen.kem`
lexer (`güvensiz`→`GUVENSIZ`, ~satır 205) ve parser (`parse_guvensiz`, ~satır 1203 →
`dugum1("GUVENSIZ", ..., parse_blok)` = TEK çocuk, o da BLOK) `güvensiz` bloğunu
tanıyordu; ancak codegen `deyim_uret` (~satır 2367) içinde **GUVENSIZ emisyon dalı
YOKTU** → düğüm hiçbir kola düşmüyor, fonksiyon sonundaki `ver 0` fall-through ile
**gövde TAMAMEN düşürülüyordu** (latent miscompile). Checker bloğu kabul ediyor,
codegen sessizce atıyor → accept-but-miscompile, **[YÜKSEK]**. C derleyici
(`kemgu.exe`, oracle; `src/llvm.c:4639` `DUGUM_GUVENSIZ`) AYNI programı doğru
derliyordu — saf self-host mirror-gap (D-093 ile aynı sınıf).

**Kök-neden (mirror-gap; reprodüksiyon + IR-teyit):** Test programı
```
işlev yardimci(x: tam32) -> tam32 { ver x + 1; }
işlev main() -> tam32 {
    değişken toplam: tam32 = 0;
    güvensiz { değişken a: tam32 = 40; a = a + 1; toplam = yardimci(a); }
    ver toplam;
}
```
C-codegen `main` gövdesi alloca+store+`call i32 @yardimci` emit ediyor → **exit 42**.
Self-host gövdesi GUVENSIZ kolu yokken bu deyimleri HİÇ emit etmiyordu → `toplam` 0
kalır → **exit 0**. Bug birebir teyit edildi.

**Çözüm (`selfhost/codegen.kem`, iki nokta — DESCENT denetimi):**
1. `deyim_uret` — BLOK kolunun hemen yanına **GUVENSIZ kolu** eklendi: tek çocuğu
   (`cocuk[a_cb+0]` = BLOK gövde) `deyim_uret` ile aynen emit, `ver 0`. C llvm.c
   `DUGUM_GUVENSIZ` ile aynı: güvensiz = codegen açısından DÜZ BLOK, gövde aynen
   üretilir. C'de ayrıca `guvensiz_derinlik` ile inline stack sınır-kontrolü atlanır;
   self-host **HEAP-uniform** (her dizi heap `KdlDizi*`, stack `[N×T]` yok → inline
   stack kontrolü zaten yok) → o makine GEREKMEZ; heap erişimleri runtime-kontrollü
   kalır = güvenli.
2. `alloca_hoist_pass` (~satır 2538) konteyner listesine **GUVENSIZ** eklendi
   (`BLOK/EGER/IKEN` yanına). GUVENSIZ'in tek çocuğu BLOK → mevcut döngü gövdeyi
   içeri indirir → güvensiz içindeki annotasyonlu `değişken` alloca'ları girişe
   hoist edilir (döngü-içi güvensiz'de yığın taşması önlenir; `pa_reg_bul` idx-eşli,
   sıra-bağımsız → güvenli).

   **Checker:** gömülü `kontrol_dugum` (~satır 3478) zaten GENEL çocuk-rekürsiyonu
   yapıyor (özel kol gerektirmeyen düğümler için tüm çocuklara iner) → GUVENSIZ'in
   çocuğu (BLOK) ZATEN denetleniyor; ek kol GEREKMEDİ (teyit: güvensiz içindeki
   tanımsız ad T002 veriyor; tanımlı `değişken` ad-çözümleniyor). Fonksiyon-seviyesi
   döngüler (emisyon ~2619, hoist çağrısı ~2612, `kontrol_govde` ~3510) GUVENSIZ'i
   doğrudan çocuk olarak GÖRMEZ (güvensiz gövde-içi, ISLEV'in BLOK çocuğunun altında)
   → dokunulmadı.

**Doğrulama:** Repro fix sonrası **exit 42** (C oracle ile eşit). Self-host IR
`main`: iki alloca girişe HOIST (`toplam` + güvensiz-içi `a`), `store 40` /
`a=a+1` / `call i32 @yardimci` gövdede EMIT (önceden hiçbiri yoktu). Yeni korpus
`test/cg_korpus/cg_guvensiz.kem` (gövde düşerse exit 0, doğruysa 42 → C oracle ile
auto-diff yakalar). `codegen_diff_harness`: **58/58** semantik eşdeğer. `make test_tumu`
→ **"Tum testler gecti!"**. `selfhost_driver_harness`: 4 mod (token 22/22, parse 12/12,
**check 48/48** = `--check`↔C `--checkdump` byte-diff TEMİZ) hem C-derlenmiş hem
self-host; **CODEGEN FIXPOINT stage1 IR == stage2 IR BİREBİR (21835 satır)**;
bootstrap lexer/parser/checker 51 birebir 0 fark. `asan_e2e_denetim.sh` →
**PASS=96 FAIL=0** (SKIP/ALLOW belgeli-ortamsal). Sıfır derleyici uyarısı
(`kemgu.exe` `-Wall -Wextra -Wpedantic` değişmedi; değişiklik `.kem`).

**Sınır/Not:** Kapsam YALNIZ self-host codegen — **POINTER-SİZ** güvensiz blokları
(düz blok gövdesi). Güvensiz içindeki pointer işlemleri (deref `*`, adres-al `&`;
`codegen.kem:2244` "CG sonrası") self-host'ta HENÜZ codegen edilmiyor → bu kararın
DIŞINDA (ayrı açık iş); bu fix pointer-siz güvensizi çalıştırır + silent-drop'u kaldırır.
`codegen.kem`'in KENDİSİ (ve lexer/parser/checker.kem) `güvensiz` bloğu KULLANMIYOR
(yalnız anahtar-kelime tanıma + `parse_guvensiz`) → yeni dal self-derlemede
tetiklenmiyor; **FIXPOINT yapısal olarak güvenli**. İzole commit; D-NNN merge-anı
güncel main'den tahsis (branch'te D-095 sabitlendi; main D-094 = G004 / PR #68).

---

## D-094 — [YÜKSEK] C checker G004 — işlev/lambda-tipli değişken YENİDEN-ATANAMAZ (accept-but-crash kapatma; öksüz fix backport) (2026-06-15)

**Karar [ETKİ: C checker doğruluk/bellek-güvenliği; izole commit].** `src/tip_kontrol.c`
`DUGUM_ATAMA` handler'ına (T022 lvalue kontrolünden hemen sonra, ~satır 4491) G004
reddi eklendi: hedef **TANIMLAYICI** ve tipi **TIP_ISLEV** (işlev/lambda) ise atama
**reddedilir**. Bu, `feature/self-host-checker` dalında ZATEN var olan ama `main`'in C
checker'ında BULUNMAYAN bir **öksüz fix**'in birebir backport'udur (kaynak:
`origin/feature/self-host-checker:src/tip_kontrol.c` ~satır 4490-4496).

**Kök-neden (KARMA closure temsili — değere bağlı):** Bir işlev/lambda değerinin runtime
temsili **yakalama-durumuna** bağlı: yakalamasız lambda / top-level fn → bare fn-ptr;
yakalamalı lambda → closure `{fn, env}`. Çağrı yeri statik `closure_mu` bayrağına göre
dispatch eder (bağlama anında sabitlenir). İşlev-tipli bir değişken **farklı**
yakalama-durumlu bir değerle YENİDEN atanırsa → temsil uyumsuzluğu → çağrıda bare-ptr'ı
closure sanıp deref → **access-violation / SEGFAULT** (accept-but-crash, **[YÜKSEK]**).
Mehmet'in seçtiği V1 ucuz-güvenli çözüm: compile-time reddet (çökmezlik #1); programcı
yeni bir `değişken` ile bağlasın.

**Çözüm (`src/tip_kontrol.c`, DUGUM_ATAMA — T022'den sonra, T001/bidirectional'dan önce):**
`TipBilgisi *ht = tip_belirle(tk, hedef);` ardından `hedef->tip == DUGUM_TANIMLAYICI &&
ht->kategori == TIP_ISLEV` ise `tip_hata(tk, d, "G004", ...)` + `break`. Hata mesajı
ASCII-güvenli (Türkçe `\x` hex-escape kuralı gereği string literal'de Türkçe karakter
yok). ERISIM/INDEKS hedefler (`o.alan = v`, `arr[i] = v`) ETKİLENMEZ — yalnız çıplak
TANIMLAYICI yeniden-bağlaması reddedilir.

**Doğrulama:** `test/test_tip_kontrol.c`'ye 4 yeni vaka (175-178): (1) lambda lokali
yeniden atama → hata, (2) yakalama-durumu divergent yeniden atama → hata, (3) tek-atama
lambda bildirimi → 0 hata (yanlış-pozitif yok), (4) lambda-OLMAYAN yeniden atama
(`x = 7`) → 0 hata (over-reject yok). `tip_kontrol` harness 178/178 yeşil. E2E
(`kemgu.exe --check`): lambda yeniden-atama programı → `hata[G004]` (exit 1); `tam32`
yeniden-atama programı → `OK` (exit 0). `make test_tumu` → "Tum testler gecti!" (tam
yeşil). Sıfır derleyici uyarısı (`-Wall -Wextra -Wpedantic`).

**Sınır/Not:** Kapsam **YALNIZ C checker** (`src/tip_kontrol.c`). Self-host
`selfhost/checker.kem` lambda kullanmıyor → self-host port ŞİMDİLİK GEREKMEZ (mirror-gap
yok). V1 ucuz-güvenli reddetme; tam değer-akışı / yakalama-durumu izlemeyle koşullu izin
V2'ye ertelendi (D-072 ailesi). Öksüz fix backport — main'de eşi yoktu. İzole commit;
base `main`, **MERGE EDİLMEDİ** (orchestrator denetler, Mehmet merge eder).

---

## D-093 — [YÜKSEK] Self-host codegen INDEKS-atama (`arr[i] = v`) — sessiz düşme (accept-but-miscompile) kapatma (2026-06-15)

**Karar [ETKİ: self-host codegen doğruluk; izole commit].** `selfhost/codegen.kem`
ATAMA handler'ı (`deyim_uret`, ~satır 2448) yalnız **TANIMLAYICI** (`x = v`) ve
**ERISIM** (`o.alan = v`) dallarına sahipti; **INDEKS** hedef (`arr[i] = v`) dalı
YOKTU. Checker (`selfhost/checker.kem` lvalue T022, ~satır 2806) INDEKS'i geçerli
lvalue olarak KABUL ediyor, codegen ise sessizce DÜŞÜRÜYORDU → yazma kayboluyor
(`ver 0` fall-through). Accept-but-miscompile, **[YÜKSEK]**. C derleyici
(`kemgu.exe`, oracle) AYNI programı doğru derliyordu — yani saf self-host mirror-gap.

**Kök-neden (mirror-gap; reprodüksiyon + IR-teyit):** Test programı
```
işlev main() -> tam32 { değişken xs: Dizi<tam32> = []; dizi_ekle(xs,5); xs[0]=42; ver dizi_al(xs,0); }
```
C-codegen `main` gövdesi `call void @kdl_dizi_yaz_tam(ptr %5, i32 0, i32 42)` emit
ediyor → **exit 42**. Self-host gövdesi bu çağrıyı HİÇ emit etmiyordu (ATAMA INDEKS
dalı yok) → **exit 5** (`dizi_ekle` değeri kalıyor). Bug birebir teyit edildi.

**Çözüm (`selfhost/codegen.kem`, ATAMA handler — ERISIM dalından sonra):** INDEKS
hedef dalı eklendi. HEAP-uniform model (her dizi heap `KdlDizi*`; stack `[N×T]` yok)
→ inline sınır kontrolü GEREKMEZ; yalnız `kdl_dizi_yaz_*` route yeterli (runtime
`i<0` + `i>=boyut` denetler, `runtime/kdl_runtime.c`). Emisyon `dizi_yaz` built-in'iyle
(~satır 2121) BİREBİR aynı: taban dizi → `ptr`, indeks → `i32`, değer → `vty`
(`p.son_tip`); `call void @kdl_dizi_yaz_<dizi_ekle_sonek(vty)>(ptr taban, i32 idx,
<dizi_arg_tip(vty)> v)`. Sonek seçimi değer-tipinden: `i64`→`tam64`, `ptr`→`ptr`,
diğer→`tam` — `dizi_yaz` ile AYNI seçici/cast (tutarlılık + iyi-tipli IR garantisi).

**Doğrulama:** Repro fix sonrası **exit 42** (C oracle ile eşit). OOB (`xs[10]=9`,
boyut 1) → runtime **PANİK** (`dizi sınır ihlali (i=10, boyut=1)`, hem C hem self-host
özdeş). İç-içe `m[i][j]=v` → **99**, yapı-alanı `k.xs[i]=v` → **55** (her ikisi de C
oracle ile eşit; eleman-tip propagasyonu nested + struct-field için ÇALIŞIYOR — analizin
"kısmi" şüphesi bu vakalarda gerçekleşmedi). `selfhost_driver_harness.sh`: 4 mod
(token 22/22, parse 12/12, check 48/48) byte-diff TEMİZ; LLVM eşdeğerlik self 56/56 +
self2 57/57; **FIXPOINT KORUNDU** (kemgu_self2 codegen.kem IR kararlı, stage1==stage2
birebir, 21807 satır). `make test_tumu` → "Tum testler gecti!" (tam yeşil).
`asan_e2e_denetim.sh` **PASS=96 FAIL=0**. Yeni korpus: `test/cg_korpus/cg8_indeks_yaz.kem`
(yazma düşerse 11, doğruysa 42 → C oracle ile auto-diff yakalar). Sıfır derleyici
uyarısı (`-Wall -Wextra -Wpedantic`).

**Sınır/Not:** Kapsam YALNIZ self-host codegen. C `src/llvm.c`'nin `xs[i]=v`'si ZATEN
main'de (D-088 ailesi) — bu kararın DIŞINDA. `codegen.kem`'in KENDİSİ `arr[i]=v`
sözdizimi kullanmıyor (yalnız `dizi_yaz` built-in) → yeni dal self-derlemede
tetiklenmiyor; FIXPOINT yapısal olarak güvenli. İndeks `i32` varsayılır (`dizi_yaz` +
INDEKS-okuma ile AYNI sözleşme); `i64` indeks bu kararın dışında. İzole commit;
şu an paralel dal yok.

---

## D-092 — [YÜKSEK] `Dizi<T>` ATAMA dizi-literal heap-promote — accept-but-crash kapatma (2026-06-15)

**Karar [ETKİ: codegen bellek-güvenliği; izole commit].** `D-075`'in 🔴 KEŞİF
notunda işaretlenen accept-but-crash deliği kapatıldı: `--check` KABUL eden ama
üretilen kodu ÇÖKERTEN (segfault, exit 139) iki tetik düzeltildi:
```
değişken xs: Dizi<tam32> = []; xs = [1]; dizi_ekle(xs, 7);      // eskiden SEGFAULT
yapı K { xs: Dizi<tam32>; } ... k.xs = [1]; dizi_ekle(k.xs, 7); // eskiden SEGFAULT
```

**Kök-neden (D-075 KEŞİF + IR doğrulaması):** `değişken xs: Dizi<T> = [..]` (init)
yolu dizi-literalini HEAP `KdlDizi*`'a promote ederken, **ATAMA** yolu (`xs = [..]`
/ `k.xs = [..]`) stack `[N×T]` pointer'ını `Dizi<T>` (heap `KdlDizi*`) slot'una
store ediyordu. Üretilen IR'da görüldü: `%1 = alloca [1 x i32]; ... store ptr %1,
ptr %0` (`%0` = KdlDizi* slot). Sonraki `dizi_ekle`/`dizi_boyut` `KdlDizi*`
beklerken stack-array görünce çöküyordu. D-070 ailesinin (dizi-literal temsil
uyuşmazlığı) ATAMA analoğu.

**Çözüm (`src/llvm.c`) — main'in `beklenen_tip` kanalı (ayrı helper DEĞİL):**
`DUGUM_DIZI_OLUSTUR` codegen'i zaten `g->beklenen_tip` `Dizi<T>` ise heap
`KdlDizi*` üretir (D-044/D-088 yolu, ~satır 2132). ATAMA hedefi `Dizi<T>` heap +
RHS dizi-literal iken, `ifade_uret(RHS)`'den ÖNCE `g->beklenen_tip`'i hedefin
`Dizi<T>` AST tipine SET edip (sonra restore) bu heap-path'i devreye sokuyoruz —
init ile **AYNI** mekanizma. Reddetme DEĞİL; reassignment normal işlem.
- **Yerel değişken (`xs = [..]`):** `i->dinamik_dizi_mi` + RHS literal → küçük
  `dizi_tip_sar(g, i->eleman_tip_ast)` yardımcısı eleman AST'sini sentetik
  `DUGUM_TIP_DIZI`'ye sarar (heap-path yalnız `tip` + `eleman_tip` okur),
  `beklenen_tip`'e konur, heap `KdlDizi*` slot'a store edilir.
- **Yapı alanı (`k.xs = [..]`):** `dizi_alan_eleman_ast` alanın `Dizi<T>` eleman
  AST'sini verir (NULL → normal skaler alan); aynı `dizi_tip_sar` + heap store.

**Doğrulama:** 2 repro fix öncesi exit 139 → fix sonrası exit 2 (boyut doğru),
IR'da stack `[N×T]` yok (yalnız `kdl_dizi_olustur`+`kdl_dizi_ekle`). `make
test_tumu` tam yeşil — **self-host FIXPOINT korundu** (stage1 IR == stage2 IR,
21728 satır birebir). `asan_e2e_denetim.sh` PASS=96 FAIL=0 (yeni
`test/ornekler/dizi_atama.kem` auto-discovery). `dizi_sinir_harness.sh` 37/37
(yeni vaka30/31/32: ATAMA dizi-literal → çalışır). Sıfır derleyici uyarısı
(`-Wall -Wextra -Wpedantic`).

**Sınır/Not:** Numara D-092 (orchestrator) — main self-host serisi D-082..D-091'i
aldığı için PR #60'ın eski D-082 numarası kullanılmadı. PR #60 D-083 (heap
`xs[i]=v`) ARTIK main'de (PR #63 / D-088 ailesi) → bu kararın kapsamı DIŞINDA,
düşürüldü. Türetilmiş olmayan basit (nesne TANIMLAYICI olmayan, örn. `a.b.xs`)
alan zincirleri `dizi_alan_eleman_ast` kapsamı dışı (D-088 ile aynı sınır).

---

## D-091 — [YÜKSEK] İç-içe `Dizi<Dizi<T>>` — iç dizi literali heap + nested `m[i][j]` heap-route (2026-06-14)
*(eski D-088; main self-host serisi D-082..D-087 ile çakışan dizi-indeks ailesi yeniden numaralandığından kaydırıldı)*

**Karar [ETKİ: codegen doğruluk; izole commit].** İç-içe dizi literali
`[[1,2],[3,4]]`'in İÇ dizileri (`[1,2]`, `[3,4]`) heap `KdlDizi*` değil, STACK
`[N×T]` olarak depolanıyordu (dış heap dizi onlara düz ptr tutuyor). Sonuç:
- `m[1][1]` ÇALIŞIYORDU (iç stack `[2×i32]` üzerinde düz GEP doğru) — bu yüzden
  hata gizliydi.
- İç diziyi değişkene çıkarınca uzunluk metadata BOZUK: `inner = m[0];
  dizi_boyut(inner)` → 1 (gerçek 2); `dizi_al(inner, i)` → PANİK (`boyut=1`).
  Çünkü `m[0]` stack `[2×i32]` ptr (KdlDizi* descriptor değil); `kdl_dizi_boyut`
  descriptor'ın ilk alanı sanıp `inner[0]` = 1 okuyor.

**Kök-neden:** DEGISKEN dedicated heap path (`değişken d: Dizi<T> = [..]`) iç
elemanları üretirken `g->beklenen_tip`'i AST eleman tipine SET ETMİYORDU (yalnız
IR string `eleman_tip` geçiyor) → iç `[1,2]` `DUGUM_DIZI_OLUSTUR`'da beklenen_tip
`DUGUM_TIP_DIZI` görmeyip stack dalına düşüyordu. D-085/D-087 dizi-indeks
serisinin BİLEREK ERTELENMİŞ son parçası (D-085 ve D-087 "Sınır" notları).

**Çözüm (llvm.c) — `m[i][j]`'yi BOZMADAN:**
1. **İç literal heap:** DEGISKEN heap literal path'te eleman döngüsünü
   `g->beklenen_tip = <iç dizi AST tipi>` ile sarmala → iç `[1,2]`
   `DUGUM_DIZI_OLUSTUR` heap yolunu seçer (heap `KdlDizi*`). Dış dizi artık iç
   descriptor'ları (`ptr`) tutar.
2. **AST eleman tipi izleme:** `LlvmIsim`'e `const Dugum *eleman_tip_ast`
   (`eleman_llvm_tip="ptr"` iç diziyi gizlerken gerçek AST'yi saklar). DEGISKEN
   (literal + annot heap) ve param (`Dizi` + `&Dizi`) sitelerinde set.
3. **Nested INDEKS heap-route:** `turetilmis_heap_dizi_eleman` (IR döndüren,
   ERISIM/CAGRI) → `heap_dizi_eleman_ast` (AST döndüren ortak çözümleyici:
   TANIMLAYICI + nested INDEKS + ERISIM + CAGRI). `m[i][j]`: `m[i]` artık heap
   `KdlDizi*` → `[j]` recursive olarak `kdl_dizi_al`'a route edilir (eski
   stack-GEP iç descriptor'ı i32 okuyup BOZARDI → regresyon olurdu). `[]`
   okuma+yazma her ikisi.
4. **Struct iç eleman:** `Dizi<Dizi<Yapı>>` by-value yolu (D-087
   `dizi_struct_al_emit` / `kdl_dizi_*_yapi`) `heap_dizi_eleman_ast` üzerinden
   kapsanır (`et[0]=='%'`).

**Doğrulama:** `dizi_sinir_harness.sh` +5 vaka (iç-içe oku KORUNUR `m[1][1]=4`,
inner boyut=2, inner dizi_al=2, nested yazma `m[0][1]=99`, dış-indeks OOB PANIC)
→ 33/33. `test/ornekler/icice_dizi.kem` (matris satır çıkarma + döngü, exit 42,
ASan auto-discovery). Tüm suite yeşil; `asan_e2e` PASS=95 FAIL=0; 0 uyarı
(-Wall -Wextra -Wpedantic). Üçlü iç-içe (`Dizi<Dizi<Dizi<T>>>`), `tam64` iç
eleman, `Dizi<Dizi<Yapı>>` by-value elle doğrulandı. ERISIM/CAGRI tek-indeks
(D-085) regresyonsuz (`heap_dizi_eleman_ast` aynı IR'i üretir).

**Sınır:** `dizi_olustur(N)` explicit-builtin iç-içe için kapsam-dışı (literal
`[...]` yolu doğru; D-087'deki gibi explicit dizi_olustur nadir). Test edilen
heap tabanlar: düz değişken/param `Dizi<Dizi<...>>` (üçlü derinliğe kadar) +
tek-indeks ERISIM/CAGRI. İç-içe dizinin yapı ALANI olduğu zincirler (`k.m[i][j]`)
`heap_dizi_eleman_ast` ile çözülür ancak iç literal heap'liği yapı-oluştur yoluna
bağlı olduğundan ayrıca test edilmedi (gelecek).

## D-090 — [YÜKSEK] `Dizi<Yapı>` by-value struct eleman (skaler-i32 varsayımı kaldırıldı) (2026-06-14)
*(eski D-087; main self-host D-087 ile çakıştığından yeniden numaralandı)*

**Karar [ETKİ: runtime + codegen; izole commit].** `Dizi<Yapı>` (struct elemanlı
dizi) skaler `kdl_dizi_ekle_tam`/`kdl_dizi_al_tam` (i32) + `eleman_byte=4` ile
derleniyordu → 8+ baytlık yapı **truncation** (alanlar sessizce kaybolur,
`ps[0].x+ps[0].y` yanlış) + `değişken p: Yapı = dizi_al(ps,i)` GEÇERSİZ IR
(`call %Yapi @kdl_dizi_al_tam` — i32 dönüş ile uyumsuz) → **link-fail**.

**Çözüm:**
- **runtime (kdl_runtime.c):** üç by-value memcpy fonksiyonu —
  `kdl_dizi_ekle_yapi(d, src)`, `kdl_dizi_al_yapi(d, i, dst)`,
  `kdl_dizi_yaz_yapi(d, i, src)`. Eleman boyutu `d->eleman_byte` (descriptor'dan).
  al/yaz OOB → PANIC (D-069 sınıfı).
- **codegen (llvm.c):** `dizi_eleman_struct_mi(et)` (`et[0]=='%'`) tüm dizi
  sitelerinde (literal ekle, değişken-annot heap, dizi_ekle/al/yaz built-in,
  `[]` okuma TANIMLAYICI+türetilmiş, `[]` yazma) struct dalı:
  ekle = store-to-temp + ekle_yapi; al = alloca dst + al_yapi + load; yaz =
  store-to-temp + yaz_yapi. **eleman_byte** struct için `kdl_eleman_byte_yaz`
  ile LLVM `sizeof` const-expr (`ptrtoint (getelementptr (%Yapi, null, 1))`) →
  padding/alignment LLVM layout'uyla BİREBİR (C tarafı elle hesap miscompile riski).
- Skaler/ptr yolları DEĞİŞMEDİ (`et[0]=='%'` guard'ı yalnız struct'ta devreye girer).

**Doğrulama:** `dizi_sinir_harness.sh` +5 vaka (struct [] oku/yaz, dizi_al,
{tam8,tam64} padding, struct OOB PANIC) → 28/28.
`test/ornekler/dizi_yapi_eleman.kem` (exit 42, ASan auto-discovery; padding +
by-value yazma + dizi_al-struct). Tüm suite yeşil (28 paket). `asan_e2e` PASS=94
FAIL=0 (memcpy-tabanlı, heap-overflow yok). 0 uyarı. **Sınır:** `dizi_olustur(N)`
explicit-builtin'i struct için eleman_byte hâlâ skaler-varsayım (literal `[...]`
yolu doğru; explicit dizi_olustur+struct nadir — gelecekte). İç-içe
`Dizi<Dizi<Yapı>>` türetilmiş indeks D-085 nested sınırına tabi.

## D-089 — [YÜKSEK] `&Dizi<T>` referans param: codegen deref + built-in tip-kontrol tutarlılığı (2026-06-14)
*(eski D-086; main self-host D-086 ile çakıştığından yeniden numaralandı)*

**Karar [ETKİ: codegen doğruluk + tip-kontrol tutarlılık; izole commit].** İki
yüzlü `&Dizi<T>` hatası:
1. **Codegen (çöp/PANIK):** `&a` çağrı argümanı, heap dizi değişkeninin SLOT
   adresini (`KdlDizi**` — çift pointer) geçiyor; callee bunu doğrudan `KdlDizi*`
   sanıp `dizi_al`/`[]` ile indeksliyordu → descriptor'ın kendisini veri okuyordu.
2. **Tip-kontrol (tutarsız):** `dizi_al(&Dizi)` SESSİZCE kabul (t_hata, rapor yok),
   `dizi_boyut(&Dizi)` T001 reddi — aynı argüman biçimi iki built-in'de farklı.

**Çözüm:**
- **llvm.c (param girişi):** `&Dizi<T>` / `&değişken Dizi<T>` param girişte BİR KEZ
  deref edilir (`load ptr` → `KdlDizi*`) ve alloca'ya o yazılır → sonrası NORMAL
  heap dizi (dinamik_dizi_mi=1, eleman tipi referans hedefinden). `dizi_al`/
  `dizi_yaz`/`dizi_boyut`/`[]` ek deref gerektirmez. Mutasyon (`dizi_yaz`) paylaşılan
  descriptor üzerinden çağırana yansır (referans semantiği korunur).
- **tip_kontrol.c:** ortak `dizi_arg_coz(t)` — `Dizi<T>` ya da `&Dizi<T>` →
  altındaki Dizi tipini (referansı soyarak) döner. Tüm dizi built-in'leri
  (ekle/al/yaz/boyut/kapasite/kapasite_ayarla) bunu kullanır → `&Dizi` tutarlı kabul.

**Doğrulama:** `dizi_sinir_harness.sh` +4 vaka (ref dizi_al / dizi_boyut / [] /
mutasyon-çağırana-yansır) → 23/23. `test/ornekler/dizi_referans_param.kem`
(exit 42, ASan auto-discovery). Tüm suite yeşil (tip_kontrol dahil). `asan_e2e`
PASS=93 FAIL=0. 0 uyarı. **Sınır:** `&Dizi` param girişte deref edildiği için
`xs = başka_dizi` (referansı yeniden bağlama) callee-yerel kalır (çağıranın slot'u
değişmez) — KEMGU referans semantiğinde nadir; içerik mutasyonu (asıl sözleşme)
çalışır. Lokal `değişken r: &Dizi = &a` (param olmayan) bu commit'te kapsam dışı.

## D-088 — [YÜKSEK] `[]` türetilmiş heap dizi tabanı (yapı-alanı / çağrı-dönüşü) — okuma+yazma heap-route (2026-06-14)
*(eski D-085; main self-host D-085 ile çakıştığından yeniden numaralandı)*

**Karar [ETKİ: codegen doğruluk; izole commit].** `[]` indeks operatörü yalnız
düz `TANIMLAYICI + dinamik_dizi_mi` tabanlarda heap-route (kdl_dizi_al/yaz)
ediyordu; TÜRETİLMİŞ heap `Dizi<T>` tabanları (yapı-alanı `k.xs`, `Dizi<T>` dönen
işlev `yap()`) KdlDizi* DESKRİPTÖRÜNÜ düz veri sanıp GEP yapıyordu →
**accept-but-silently-wrong** okuma (çöp değer) + **accept-but-crash** yazma
(segfault). Karşıtlık: `dizi_al`/`dizi_yaz`/`dizi_boyut` built-in'leri aynı
tabanlarda DOĞRU — taban reg'i (KdlDizi*) doğru üretiliyor, yalnız `[]`
lowering'i onu raw buffer sanıyordu (D-083 "Sınır" notunda kapsam-dışı işaretliydi).

**Çözüm (src/llvm.c):**
- Ortak `turetilmis_heap_dizi_eleman(g, nesne)` çözümleyici: ERISIM (yapı alanı
  `Dizi<T>` → `dizi_alan_eleman_ir`) ve CAGRI (`Dizi<T>` dönen işlev →
  `islev_bul` + dönüş tipi AST'sinden eleman IR). Değilse NULL → stack GEP.
- Okuma (DUGUM_INDEKS): TANIMLAYICI fast-path'in ARDINDAN türetilmiş taban
  heap ise `kdl_dizi_al_<tip>` route (`ifade_uret(taban)` zaten KdlDizi* verir).
- Yazma (DUGUM_ATAMA→DUGUM_INDEKS): TANIMLAYICI heap yazma artık DÜŞMÜYOR
  (eski "kdl_dizi_yaz_eleman yok" yorumu geçersiz — runtime'da `kdl_dizi_yaz_*`
  VAR); türetilmiş heap yazma da `kdl_dizi_yaz_<tip>` route.
- Ortak fn-seçici `kdl_al_fn`/`kdl_yaz_fn`/`kdl_al_donus_ir` (built-in ile
  paylaşılan eleman-IR→intrinsic eşlemesi).
- `değişken xs: Dizi<T> = yap()` (literal-DIŞI değer, çağrı/başka-dizi) artık
  `dinamik_dizi_mi=1` işaretlenir → `xs[i]` TANIMLAYICI fast-path'ten heap-route.
  (Tip kontrolü Dizi<T> annotasyonunu heap garanti eder; stack → G003 reddi.)

**Doğrulama:** `dizi_sinir_harness.sh` +5 vaka (erisim oku/yaz, çağrı oku, direct
heap yaz, türetilmiş OOB PANIC) → 19/19. `test/ornekler/dizi_turetilmis_taban.kem`
(exit 42, ASan auto-discovery). Tüm suite yeşil (29 paket, llvm 235/235).
`asan_e2e_denetim.sh` PASS=92 FAIL=0. 0 uyarı.

**Kapsam/sınır:** Skaler + ptr eleman. **Struct eleman (`Dizi<Yapı>`, `et[0]=='%'`)
şimdilik stack/yorum yoluna düşer — D-087'de by-value yapı.** İç-içe
`Dizi<Dizi<T>>` türetilmiş indeks (nested INDEKS tabanı) çözümleyici kapsamında
DEĞİL → mevcut çalışan stack-GEP yolu korunur (m[i][j] regresyonsuz); iç diziyi
değişkene çıkarınca uzunluk metadata hâlâ bozuk (nested-literal stack temsili,
ayrı sorun — D-082 inner-heap'e bağlı, deferred). &Dizi referansı D-086.

> **Not (merge):** PR #63'ün eski D-084'ü (stack `[N×T]` YAZMA OOB sınır-kontrolü)
> bu main-merge'inde DÜŞÜRÜLDÜ — birebir aynı düzeltme main'de `a6d690d` / D-069
> (Kategori 2) olarak zaten mevcut (`stack_uz` sınır-kontrolü, `src/llvm.c`). Kod
> kaybı yok; yalnız çift kayıt önlendi. Düzeltmenin kendisi PR #63'ün
> `src/llvm.c`'sinde KORUNUYOR.

---

## D-087 — Bootstrap CHECKER kanıtı: 4 bileşenin TAMAMI self-host codegen ile doğru derlenir (2026-06-14)

**Karar [ETKİ: yalnız test/codegen_bootstrap_harness.sh; kaynak DEĞİŞMEDİ].** D-086'da codegen.kem
DRIVER oldu (checker dâhil) ve self-host-codegen ile FIXPOINT'e derlendi — ama fixpoint yalnız
DETERMİNİZM (stage1==stage2) kanıtlar, self-host-codegen-derlenmiş checker'ın DOĞRULUĞUNU değil.
Bootstrap harness'a CHECKER bileşeni eklendi (lexer/parser ile aynı desen): self-host-codegen ile
derlenen checker.kem'in `--checkdump` çıktısı, C-codegen ile derlenenle korpus üzerinde diff'lenir.

**Sonuç:** `make calistir_codegen_bootstrap` artık LEXER 46 + PARSER 46 + **CHECKER 46** +
CODEGEN FIXPOINT (stage1==stage2, 21728 satır) — **4 self-host bileşeninin TAMAMI** (lexer, parser,
checker, codegen) self-host codegen tarafından DOĞRU derlenir (korpus: selfhost/*.kem + ornekler;
3815-satır driver ve checker.kem'in kendisi dâhil). Bu, D-086 driver fixpoint'inin korelatif
doğruluk kanıtı (yalnız determinizm değil). `make test_tumu` YEŞİL, 0 regresyon, 0 uyarı.

**Not:** Bu, origin/feature/self-host-checker'daki 62dd7e8 (öksüz; main'e hiç merge olmadı) ile aynı
amacı main hattında bağımsız gerçekler — D-086 driver state'i üzerine (checker artık driver'da).

---

## D-086 — 🎉 AŞAMA 4 driver: tek self-host kemgu binary (checker + 4-mod dispatch) + driver FIXPOINT (2026-06-14)

**Karar [ETKİ: self-host birleştirme; C derleyici DEĞİŞMEDİ].** Aşama 1-3'te lexer/parser/checker/
codegen AYRI self-host binary'lerdi; D-085 codegen self-compile fixpoint'i kanıtladı. Aşama 4 =
`selfhost/codegen.kem`'i TEK birleşik KEMGU derleyici driver'ına dönüştürmek: checker mantığı +
`--token/--parse/--check/--llvm` dispatch eklendi → `build/kemgu_self.exe`. **Sonuç D-085'i aşar:**
birleşik driver da KENDİNİ fixpoint olarak üretir (self-host derleyici, checker dâhil).

**Süreç notu (şeffaflık):** Bu iş ilk olarak bayat D-081 tabanı üzerinde [D-082] etiketiyle yapıldı
(commit 20b5408, tag `asama4-d082-backup`) — ama gerçek D-082 = CG8 dizi (origin/main). Branch
origin/main'e (9f66dc9; D-082..D-085 dahil) sıfırlandı ve driver **yeni** codegen.kem (2659 satır;
CG8 dizi + CG7d + CG9a alloca-hoist + fixpoint) üzerine **yeniden** uygulandı, doğru D-086 ile.

**KARAR 1 — Yer: codegen.kem YERİNDE.** `checker.kem`/`lexer.kem`/`parser.kem` DOKUNULMADI (Aşama 1-2
referans; harness'ları yeşil). codegen.kem 2659→3820 satır. no-flag→--llvm varsayılan → mevcut
`calistir_codegen_diff` VE `calistir_codegen_bootstrap` (`<file>`→IR çağrıları) bozulmaz.

**KARAR 2 — Merge: front-end birebir, back-end union.** İki `Ayr` struct'ı lexer+parser+AST
tablosunda ÖZDEŞ. Ortak-isimli alanlar (`yapi_ad/alan_tip/fn_ad`) PAYLAŞILIR (—check KEMGU tipiyle,
—llvm LLVM tipiyle; ayrı invocation). Çatışma = tam 4: `ayr_olustur` (union init), `main` (dispatch),
`karsilastirma_mi` (checker yalnız sıralı `<>` → **`sirali_kars_mi`** rename, codegen'in `==/!=`
dahil olanıyla çakışmasın), `yapi_var_mi` (checker dup DROP — codegen `yapi_idx` ≡ checker
`yapi_idx_bul`). 64 checker fonksiyonu (`duz_yaz` + `g_ekle`..`kontrol_program`) programatik port;
sıfır duplicate (C checker T024 doğrular). CG8/CG9a'nın +12 fonksiyonu da çakışmaz.

**KARAR 3 — `--token` lexer.kem'den PORT EDİLMEDİ.** lexer.kem `lex_dosya`/`emit` streaming;
helper'ları (`sayi_emit`/`op_emit`) codegen'in tablo-tabanlı eşadlılarından divergent. Yeni
`token_dump` codegen'in mevcut `lex_et` tablolarını (t_ad/t_sat/t_sut/t_off/t_uz) C `--token`
formatında döker (lex_et isimleri C `token_tipi_adi` ile ampirik birebir).

**Doğrulama (`make calistir_self_driver`):** HEM C-derlenmiş HEM **self-host-derlenmiş** driver
(driver kendini derler → kemgu_self2) 4 modda da C oracle ile eşleşir: TOKEN 22/22, PARSE 12/12,
CHECK 48/48, LLVM 56/56 (her iki driver). **FIXPOINT:** kemgu_self2'nin codegen.kem IR'ı kararlı
(21728 satır). `make calistir_codegen_bootstrap` driver-ify codegen.kem ile: lexer 46 + parser 46 +
codegen stage1==stage2 (21728 satır) ✓ — **birleşik derleyici self-hosting fixpoint.** `make
test_tumu` YEŞİL (sıfır regresyon). 0 uyarı.

**Sınırlamalar / sonraki:** (a) checker mantığı artık iki yerde (checker.kem Aşama 2 referansı +
codegen.kem driver) — kasıtlı; tek-kaynağa indirgeme ileride. (b) `--check` checkdump formatı
(test edilebilirlik); insan-okunur "OK/HATA" ayrımı ileride. (c) origin/feature/self-host-checker'ın
D-071 lambda G004 + D-085 checker-bootstrap-proof commit'leri ayrı; bu iş onlardan bağımsız.

---

## D-085 — 🎉 AŞAMA 5 BOOTSTRAP FIXPOINT — codegen self-host KENDİNİ ÜRETİYOR (2026-06-14)

**Karar [ETKİ: milestone — kod değişmedi, doğrulama].** KEMGU-yazılı codegen (selfhost/codegen.kem)
gerçek bir self-host derleyici: ÜÇ bağımsız bootstrap kanıtı yeşil.

**1) LEXER bootstrap (46/46):** codegen.exe ile derlenen lexer, TÜM self-host + ornekler korpusunda
C-codegen lexer ile byte-identik token çıktısı (codegen.kem'in kendisi dahil — 26122 token).

**2) PARSER bootstrap (46/46):** codegen.exe ile derlenen parser, aynı korpusta C-codegen parser ile
byte-identik --ast (parser self-parse 9672, checker 16214, codegen 16397 AST satırı).

**3) CODEGEN self-compile FIXPOINT:** codegen.exe (C-build, stage0) codegen.kem'i derler → stage1 IR
(15114 satır) → codegen2.exe. codegen2.exe codegen.kem'i derler → stage2 IR. **stage1 == stage2,
BYTE-IDENTİK.** = derleyici kendini sabit-nokta olarak yeniden üretiyor (self-hosting'in tanımlayıcı
özelliği). Transitif: codegen2.exe lexer.kem IR'ı da codegen.exe ile birebir.

**Bu fixpoint'i mümkün kılan son düzeltmeler:** D-072..D-084 (CG1-9: literal→ifade→deyim→kontrol→
çağrı→multi-int→metin→yapı→dizi→hoist), önek-builtin (D-083), bool-lit MANTIKSAL fix (D-083,
bootstrap'in yakaladığı), alloca-hoist (D-084, döngü yığın taşması). Semantik oracle (exit-kod)
+ byte-diff bootstrap oracle birlikte.

**Doğrulama:** `test/codegen_bootstrap_harness.sh` (3 kanıt) Makefile `calistir_codegen_bootstrap`
→ test_tumu. **Sınır/Sonraki:** (a) CHECKER (checker.kem) self-host ayrı iş (tip-kontrol, codegen
değil) — checker_diff zaten 48/48 C-paritesinde; KEMGU-codegen-built checker bootstrap'i sıradaki;
(b) codegen.kem CG9-üstü özellik kullanmıyor (çeşit/eşleş/lambda/modül yok) → o yollar korpus-test'li
ama self-host'ta egzersiz edilmiyor; (c) AŞAMA 4 driver (tek `kemgu` binary'de lex+parse+check+codegen
zinciri) ayrı paketleme işi.

---

## D-084 — AŞAMA 5 CG9a: alloca-hoist ön-pass → LEXER BOOTSTRAP TAM (46/46 birebir) (2026-06-14)

**Karar [ETKİ: self-host codegen; C derleyici DEĞİŞMEDİ].** D-083'te teşhis edilen alloca-in-loop
yığın taşması düzeltildi. **alloca-hoist ön-pass:** `alloca_hoist_pass` işlev gövdesini gezip TÜM
annotasyonlu DEGISKEN alloca'larını entry bloğuna çıkarır (döngü-içi `değişken` artık bir kez
alloca → yığın sabit). `pa_node`/`pa_reg`/`pa_base` (düğüm→entry-reg eşlemi; shadow-güvenli, düğüm
anahtarlı). DEGISKEN handler: annotasyonlu → hoist-edilmiş reg'i kullan (alloca yok), yalnız store;
annotasyonsuz → inline (eski yol; self-host hepsi annotasyonlu, korpus nadir). C codegen D-041
hoist_renumber ile AYNI amaç, farklı mekanizma (ön-pass vs tmpfile-buffer-renumber).

**🎉🎉 LEXER BOOTSTRAP TAM — 46/46 BİREBİR (büyükler dahil):** codegen.exe ile derlenen lexer,
parser.kem (15558 token), checker.kem (26398), **codegen.kem (26122 — kendini lex'ler)** dahil
TÜM self-host kaynaklarında C-codegen-built lexer ile byte-identik. İlk TAM self-host fixpoint
bileşeni: KEMGU-yazılı codegen'in ürettiği makine kodu, C derleyiciyle aynı davranan lexer veriyor.

**Doğrulama:** oracle 56/56 (regresyon yok); `test/codegen_bootstrap_harness.sh` (KEMGU-codegen
lexer vs C-codegen lexer diff) Makefile `calistir_codegen_bootstrap` → `test_tumu`. **Sonraki:**
parser.kem bootstrap (--ast paritesi), sonra checker.kem (--checkdump), sonra codegen.kem
self-compile (Aşama 5 tam fixpoint: codegen.exe codegen.kem'i derler → codegen2.exe → idempotent).

---

## D-083 — AŞAMA 3/5 CG7d + LEXER BOOTSTRAP: önek-builtin + bool-lit fix + alloca-hoist teşhisi (2026-06-14)

**Karar [ETKİ: self-host codegen; C derleyici DEĞİŞMEDİ].** İlk gerçek bootstrap denemesi:
codegen.exe (KEMGU-yazılı codegen) ile self-host kaynakları derle.

**Builtin önek-eşleme (CG7d):** `builtin_kdl_ad` artık önek-tabanlı (`metin_`/`dosya_`/`yaz_`/
`yazdir_`/`arg_`/`oku_karakter`/`ondalik_bicimle` → `kdl_*`), C codegen ile aynı. **Kritik gate:**
önce `fn_var_mi` (kullanıcı işlevi mi) bakılır — `yaz_str`/`yaz_kacis` gibi `yaz_` önekli USER
fonksiyonlarını builtin sanmamak için. `dizi_yaz` özel-case eklendi. Declare bloğu tam küme.

**🔴 Bool-literal fix (bootstrap'in yakaladığı GERÇEK bug):** parser `doğru`/`yanlış` için
`MANTIKSAL` düğümü (a_deg="1"/"0") üretir, `DOGRU`/`YANLIS` DEĞİL. CG2'deki varsayımım yanlıştı;
hiçbir korpus testi çıplak bool literal kullanmadığından gizli kaldı. `iken doğru` → koşul "0"
(fallthrough default) → döngü hiç girilmiyordu. ifade_uret `MANTIKSAL` → a_deg döndürür. 55/55.

**🎉 LEXER BOOTSTRAP — 45/48 birebir:** codegen.exe lexer.kem'i derler → çalışan exe → C-codegen-
built lexer ile **BYTE-IDENTİK çıktı** (test/ornekler + küçük korpus 45 dosya). İlk self-host
fixpoint kanıtı.

**🔴 TEŞHIS — alloca-in-loop yığın taşması (kalan bootstrap engeli):** 3 BÜYÜK dosya (parser/
checker/codegen.kem) ~30KB+ girdide erken-temiz-çıkış (rc=0, çıktı capped). Kök-neden: "hoist-free"
tasarımım `alloca`'yı DEGISKEN'in olduğu yere basar → DÖNGÜ İÇİ `değişken` her iterasyonda alloca
→ yığın sınırsız büyür → ~binlerce iterasyonda taşma. C codegen D-041 hoist_renumber ile tam da
bunu önler. Korpus döngüleri az iterasyon (gizli kaldı); lexer binlerce. **Düzeltme (sonraki):**
DEGISKEN alloca'larını entry bloğuna hoist eden ön-pass (annotasyon→ll_tip; annotasyonsuz→tip_cikar).

---

## D-082 — AŞAMA 3 CG8: dizi (heap KdlDizi + []-literal + element-tip polimorfik builtin) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** codegen.kem'in son büyük
bağımlılığı (dizi_al 95× / dizi_ekle 89× / dizi_boyut 20× + `[]` init). Element-tip izleme:
`cg_aelem` (Dizi değişkeni eleman tipi) + `alan_elem` (Dizi alanı) + `beklenen_elem` (`[]` bağlamı)
+ `son_elem` (sonuç). Yardımcılar: `ll_eleman_tip` (TIP_DIZI→eleman), `dizi_eleman_byte`
(ptr/i64→8, diğer→4), `dizi_ekle_sonek`/`dizi_al_sonek`/`dizi_arg_tip`/`dizi_al_rettip`.

**`[]` (DIZI_OLUSTUR):** `call ptr @kdl_dizi_olustur(i32 <byte>)` (boş = sadece oluştur; runtime
boyut/kapasite=0, ekle'de büyür) + non-empty için her eleman `ekle`. Eleman byte = `beklenen_elem`
(annotasyon/alan bağlamından). **dizi_ekle:** DEĞER tipine göre route (ptr→ekle_ptr, i64→ekle_tam64,
else→ekle_tam). **dizi_al:** DİZİNİN eleman tipine göre route (`son_elem` arg[0]'dan); ptr→al_ptr/ptr,
i64→al_tam64/i64, else→al_tam/i32. **dizi_boyut→i32.** INDEKS (`xs[i]`) = dizi_al eşi. `metin`/`Dizi`
alanları `ptr` (8-byte slot ptr-eleman ile tutarlı).

**Doğrulama:** test/cg_korpus 54 program (+6 CG8: temel/boyut/literal/indeks/**Dizi&lt;metin&gt;**/
**struct-ref-dizi_ekle**). 54/54 exit eşdeğer. struct-ref IR doğru (load ptr→GEP→load Dizi→ekle —
self-host tok_ekle deseni); Dizi&lt;metin&gt; → `olustur(i32 8)`+`ekle_ptr`+`al_ptr`. **Sonraki:**
self-compile denemesi (codegen.exe ile lexer/parser.kem derle) + CG9 (kullanılan kalan: çeşit/eşleş?).

---

## D-081 — AŞAMA 3 CG7c: yapı by-reference (&Yapi param + alan mutasyonu) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** Self-host'un kalbi:
codegen.kem'in `Ayr`'ı her yerde `&değişken Ayr` (238 alan erişimi). Ref-izleme: `Ayr.cg_aref`
(değişken ptr ise işaret ettiği yapı) + `Ayr.son_ref` (son ifade ref'i) + `cg_var_ref_bul` +
`param_ref_yapi` (`&Yapi` param → yapı adı).

**Adres-al (`&`/`&değişken` TANIMLAYICI):** LOAD YOK — alloca zaten adres → `cg_var_bul`'u döndür;
son_ref = (değer-yapı `%X`→X, ya da ptr ise onun ref'i). **ERISIM ptr-yolu:** son_ref boş değilse
`getelementptr %Ref, ptr nesne, i32 0, i32 fidx` + `load`. **ATAMA ERISIM lvalue (`p.alan = v`):**
nesne struct-değer → alloca=adres; nesne ptr → `load ptr` ile taban; sonra GEP + `store`.
**Çağrı:** main `f(&değişken p)` (değer-var adresi) ve nested `f(p)` (ptr param yükle) — ikisi de
`ptr` arg üretir (mevcut & + TANIMLAYICI-load yolları).

**Doğrulama:** test/cg_korpus 48 program (+4 CG7c: ref-oku/**mutasyon**/nested-bare-ptr/çoklu-
mutasyon). 48/48 exit eşdeğer. Mutasyon IR doğru (load ptr→GEP→load/store alan). **Sınır:** tek-
seviye ERISIM lvalue (a.b.c= nadiren; self-host p.alan= kullanır). **Sonraki:** CG8 — dizi
(heap KdlDizi + []-literal + dizi_* element-tip polimorfik builtin + indeks + için).

---

## D-080 — AŞAMA 3 CG7b: yapı (struct) by-value — tip-def + oluştur + erişim (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** Yapı tablosu
(`yapi_ad/yapi_abase/yapi_acount/alan_ad/alan_tip` — checker.kem deseni) + ön-pass `yapi_topla`
(iki sub-pass: önce adlar, sonra alanlar → iç içe yapı ref'i çözülür) + `yapi_tip_emit`
(`%Ad = type { t0, ... }`).

**Bulgu:** sade yapı tipi (`Nokta`) parser'da **TIP_BASIT** (a_deg="Nokta"), TIP_KULLANICI DEĞİL
(o yalnız generic/qualified). ll_tip: TIP_BASIT + `yapi_var_mi` → `%Ad`; TIP_REFERANS/POINTER/DIZI
→ `ptr`. **YAPI_OLUSTUR (by-value):** `alloca %T` + her ALAN_ATAMA için `getelementptr+store`
(alan indeksi ADLA bulunur → alan-sırası bağımsız) + `load %T` (by-value akış). **ERISIM (value):**
nesne tipi `%...` ile başlıyorsa `extractvalue` (ptr/referans erişimi → CG7c). Yapı değişkeni/
dönüşü generic CG3/CG6 makinesiyle çalışır (vtip=%Ad, cur_ret=%Ad).

**Doğrulama:** test/cg_korpus 44 program (+5 CG7b: nokta/3-alan/by-value-dönüş/karışık-tip-tam64/
alan-sıra-bağımsız). 44/44 exit eşdeğer. **Sınır:** by-REFERANS (`&Yapi` param + alan mutasyonu)
→ CG7c (self-host'un Ayr'ı her yerde `&değişken Ayr` — kritik). **Sonraki:** CG7c — &T param +
adres-al (&var) + ptr erişim/atama (GEP+load/store).

---

## D-079 — AŞAMA 3 CG7a: metin literali + runtime builtin + declare header (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** D-072 CG7 planının metin
+ runtime yarısı (yapı = CG7b). Ön-pass workflow'u (4 ajan) C-codegen ABI'sını birebir çıkardı.

**Metin literali:** `Ayr.str_deg` (benzersiz literal havuzu, dedup). Ön-pass düz düğüm tablosunu
tarar (`str_pre_pass`), tüm METIN → havuz. Global: `@.str.N = private unnamed_addr constant
[K x i8] c"...\00"` (K=byte+1). **Escape (C ile birebir):** `\` `"` `<0x20` `>=0x7F` → `\HH`
(BÜYÜK hex) — Türkçe UTF-8 yüksek-byte'lar tam escape (`"çay"` → `c"\C3\A7ay\00"`, uzunluk 4).
Referans: `getelementptr [K x i8], ptr @.str.N, i32 0, i32 0`. `metin` tipi → `ptr` (ll_tip).

**Runtime builtin:** `builtin_kdl_ad` (KEMGU adı → `@kdl_*`; metin_*/yaz_*/dosya_*/arg_*) +
`builtin_ret` (dönüş tipi). CAGRI built-in tespit → `@kdl_*` çağrısı; void çağrı `%r` atamaz.
**Kritik — i1 normalizasyonu:** runtime `metin_esit` vb. GERÇEK i1 döner ama mantıksal=i32
invaryantı → call sonrası `zext i1→i32` (CG2/CG4 ile tutarlı, exit eşdeğer). `runtime_header_yaz`
declare bloğu (codegen.kem alt kümesi + libc).

**Doğrulama:** test/cg_korpus 39 program (+6 CG7a: uzunluk/esit/birlestir/metin-dönüş/param-bayt/
**türkçe**). 39/39 exit eşdeğer. IR temiz (dedup, UTF-8 escape, zext). **Sınır:** dizi_* builtin
henüz yok (CG8 — element-tip polimorfizmi). **Sonraki:** CG7b — yapı (%T type, alloca/GEP,
erişim/oluştur, by-ref param + by-value dönüş).

---

## D-078 — AŞAMA 3 CG6: multi-int (i8/16/32/64) + tip-izleme + sext/trunc (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** Uniform-i32'den
gerçek-tip codegen'e: `Ayr.son_tip` (her ifade_uret sonucunun LLVM tipi — recursive "dönüş-tip
register'ı"), `Ayr.cur_ret` (mevcut işlev dönüş tipi), `Ayr.cg_atip` (değişken→LLVM tip),
`Ayr.fn_ad/fn_ret` (ön-pass imza tablosu). Yardımcılar: `ll_tip` (TIP_BASIT → i8/i16/i32/i64),
`tip_birlestir` (operand birleştir), `tip_genislik`, `fn_ret_bul`, `islev_donus_tip`, `param_tip`.

**Anahtar basitleştirme — KEMGU örtük-dönüşüm YOK → operand birleştirme tek-yönlü:** `a + b`'de
operandlar zaten aynı tip (checker garantisi); tek istisna bağlamsız literal (i32-default).
`tip_birlestir` = biri i32 ise diğeri. Literaller metin-agnostik ("5"), tip yalnız komut
annotasyonunda → literali yeniden-emit gerekmez. **mantıksal = i32 tutuldu** (CG2/CG4 bool
mantığı bozulmadı; `define i1` yerine `define i32` — exit-kod eşdeğer). **Casts (`olarak`,
TIP_DONUSTUR):** hedef>kaynak → `sext`, hedef<kaynak → `trunc`, eşit → no-op.

**Doğrulama:** test/cg_korpus 33 program (+5 CG6: tam64/tam16/tam8-sext/trunc/i64-param).
i64 alloca/add/mul, `trunc i64→i8`, `sext i8→i32`, i64-param+call hepsi temiz IR. **33/33 exit
eşdeğer.** **Sınırlar (v1):** (a) signed div/rem (sdiv/srem) — dtam (unsigned) için udiv/urem
henüz yok (codegen.kem signed tam kullanır → self-host etkilenmez); (b) bağlamsız literal →
geniş paramda i32 default (geçici: tipli yerel kullan); (c) kesirli (float) yok (CG sonrası).
**Sonraki:** CG7 — metin literali (@.str global) + yapı (%T, alloca/gep) + runtime declare header.

---

## D-077 — AŞAMA 3 CG5: çağrı + parametre + özyineleme (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `islev_uret` artık
parametreleri emit eder; `ifade_uret`'e CAGRI (call) eklendi.

**Parametre:** İmzada ADLI param (`%a0, %a1...` — adlı olduğundan `%N` reg sayacını tüketmez,
entry alloca'ları %0'dan başlar). Entry'de her param için `alloca i32` + `store %aN` + ad→reg
kaydı (CG3 yerel deseni; param mutable). **CAGRI:** hedef = çocuk[0] TANIMLAYICI adı; argümanlar
SIRAYLA `ifade_uret` ile değerlendirilip operandlar yerel `Dizi<metin>`'e biriktirilir (init+append
— güvenli desen, ATAMA-reassignment DEĞİL), sonra tek `call i32 @ad(i32 a0, ...)`. Aynı-modül
ileri-referans (özyineleme + forward-call) LLVM'de declare gerektirmez.

**Doğrulama:** test/cg_korpus 28 program (+5 CG5: topla/kare/fib/fakt/gcd-işlev). fib(10)=55,
fakt(5)=120, gcd(48,36)+30=42 — **28/28 exit eşdeğer**. **Sınır:** runtime/builtin çağrıları
(yaz_tam, dizi_*) CG7+ (declare header gerek). **Sonraki:** CG6 — multi-int (i8/16/64, dtam) +
sext/trunc + işaretsiz + gerçek dönüş-tipi emit (mantıksal→i1 main).

---

## D-076 — AŞAMA 3 CG4: kontrol akışı (eğer/iken → br + %bbN blok) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `deyim_uret`'e EGER
(if/else/else-if zinciri) ve IKEN (while). `ifade_uret`'e DOGRU/YANLIS literali (→ "1"/"0").

**Blok yönetimi:** `Ayr.lbl` (etiket sayacı, `bb0/bb1/...` ADLI bloklar — LLVM unnamed-temp
sayacını TÜKETMEZ, `%N` reg sayacı bağımsız kalır) + `Ayr.bb_term` (mevcut blok terminatörlü
mü). Yardımcılar: `yeni_label`, `etiket_yaz` (bb_term=0), `br_to` (yalnız bb_term==0 iken
fall-through dal — çift terminatör önlenir), `kosul_i1` (i32 0/1 koşulu → i1 `icmp ne 0`).
İşlev sonunda blok açıksa `ret i32 0` (ölü ama geçerli — iki dal da `ver`'lediğinde end bloğu).

**EGER:** else-yok → Lelse=Lend; else var → ayrı Lelse; else child BLOK veya iç EGER (her ikisi
de `deyim_uret` ile). **IKEN:** Lhead (koşul) → Lbody → Lhead geri-dal / Lend. SSA sayacı
işlev-geneli (blok-başı değil), etiketler adlı → numaralama çakışması yok.

**Doğrulama:** test/cg_korpus 23 program (+5 CG4: if/if-else/else-if/while/gcd). gcd(48,36)+30=42
(while + iç değişken + `%`). **23/23 exit eşdeğer** (ardışık koşu kararlı). Üretilen IR temiz:
bb0=head, bb1=body, bb2=end; sıralı `%0..%10`. **Sonraki:** CG5 — çağrı (call) + parametre
(alloca/store) + özyineleme (fib/faktöriyel).

---

## D-075 — AŞAMA 3 CG3: değişken + atama + tanımlayıcı (entry alloca/store/load) (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `ifade_uret`'e
TANIMLAYICI (`load i32, ptr %a`); `deyim_uret`'e DEGISKEN (`alloca i32` + `store` + ad→reg
kaydı), ATAMA (lvalue TANIMLAYICI → `store`), IFADE_DEYIMI (yan etki için değerlendir).

**Tasarım — işlev-içi değişken haritası APPEND-only + cg_base (reassignment YOK):** ilk
denemede `p.cg_ad = []` (işlev başında haritayı sıfırla) **codegen.exe'yi SEGFAULT ettirdi.**
Kök-neden ↓. Çözüm: `Ayr.cg_base` = işlev-başı slice indeksi; `cg_var_ekle` yalnız append,
`cg_var_bul` `cg_base..son` arar (önceki işlevlerin değişkenleri görünmez). Parser zaten Dizi
alanlarını yalnız append eder (t_ad vb.) — aynı güvenli desen.

**🔴 KEŞİF — C derleyici accept-but-crash deliği (ATAMA ile dizi-literal):** `--check` KABUL
eder ama codegen ÇÖKER (segfault, exit 139):
```
değişken xs: Dizi<tam32> = []; xs = [1]; dizi_ekle(xs, 7);   // SEGFAULT
yapı K { xs: Dizi<tam32>; } ... k.xs = [1]; dizi_ekle(k.xs,7); // SEGFAULT
```
Tetik: **`Dizi<T>` lvalue'ya ATAMA ile dizi-literal RHS** (`xs = [...]`). `değişken`-init
yolu (`değişken xs: Dizi<T> = [...]`) ve yalnız-append ÇALIŞIR — init yolu heap KdlDizi
promote eder; ATAMA yolu stack `[N×T]` pointer'ını Dizi-slot'a yazar → `dizi_ekle` çöker.
D-070 ailesi (dizi-literal temsil uyuşmazlığı), ATAMA analoğu. **Self-host bundan etkilenmez**
(cg_base ile reassignment yok). Odaklı [YÜKSEK] düzeltme için işaretlendi (codegen ATAMA
yolunda init ile aynı heap-promote; G-kodu reddi DEĞİL — reassignment normal işlem).

**Doğrulama:** test/cg_korpus 18 program (5 CG1 + 8 CG2 + 5 CG3), **18/18 exit eşdeğer**
(5 ardışık koşu kararlı). Harness sağlamlık: 127 (Defender ilk-exec taraması) → bekle+tekrar
(6 tur). **Sonraki:** CG4 — eğer/iken (br + %bbN blok) kontrol akışı.

---

## D-074 — AŞAMA 3 CG2: karşılaştırma + mantıksal + tekli (2026-06-14)

**Karar [ETKİ: self-host codegen genişletme; C derleyici DEĞİŞMEDİ].** `ifade_uret`'e:
karşılaştırma (`== != < > <= >=` → `icmp <pred> i32` + `zext i1→i32`), mantıksal
(`ve`/`veya` → `and`/`or i32`, kısa-devresiz — C v1 ile aynı semantik), tekli (`neg`
→ `sub i32 0,x`; `degil` → `icmp eq i32 x,0` + zext). `karsilastirma_mi`/`icmp_pred`/
`zext_i1` yardımcıları eklendi.

**Önemli bulgu — korpus tip-GEÇERLİ olmalı (oracle önkoşulu):** karşılaştırma `mantıksal`
üretir; KEMGU'da `mantıksal → tam32` ÖRTÜK dönüşüm YOK (çekirdek ASLA kuralı). Yani
`(5>3)+41` tip-GEÇERSİZ → C `--check` reddeder → C codegen çöp/0 üretir → anlamsız oracle.
Çözüm: karşılaştırma/mantıksal/`degil` testleri `mantıksal`-dönüşlü main ile (`işlev main()
-> mantıksal { ver 5 > 3; }` → exit 1). C codegen bunlara `define i1 @main()` basar; KEMGU
codegen `define i32 @main()` + `ret i32 <zext 0/1>` basar — exit-kod 0/1 için eşdeğer
(gerçek dönüş-tipi emit'i CG6 multi-int'te). `degil`: C `xor i1,true`, KEMGU `icmp eq+zext`
— bayt farklı, **semantik aynı** (D-072 KARAR 1 oracle'ının tam da amacı).

**Doğrulama:** test/cg_korpus 13 program (5 CG1 + 8 CG2), **13/13 exit eşdeğer**. Harness
sağlamlık düzeltmesi: Win11 `.exe` yeniden-yazım dosya-kilidi yarışı → dosya-başı benzersiz
çıktı adları (`$b.c.exe`/`$b.k.exe`). **Sonraki:** CG3 — değişken (entry alloca/store/load)
+ atama + tanımlayıcı.

---

## D-073 — AŞAMA 3 CG1: codegen.kem iskeleti + ilk semantik-oracle yeşil (2026-06-14)

**Karar [ETKİ: yeni self-host artefakt; C derleyici DEĞİŞMEDİ].** D-072 planının CG1 adımı:
`selfhost/codegen.kem` oluşturuldu = `selfhost/parser.kem` kopyası (lexer+parser REUSE) +
AST-yürüten LLVM IR text emitter. `duz_yaz` (--ast dumper) → `program_uret`/`islev_uret`/
`deyim_uret`/`ifade_uret` ile değiştirildi; `main` artık IR basar.

**Kapsam (CG1):** işlev (parametresiz) + `ver` + tam literal + ikili aritmetik (`+ - * / %`).
Üretilen IR: `target triple` + `define i32 @ad() { entry: ... ret i32 <op> }`. **Hoist-FREE:**
tek blok `entry:`, SSA sıralı `%N` sayacı (`Ayr.reg`, işlev başında 0'a reset). C codegen'in
`entry:`+`%0`-başlangıç deseni doğrulandı; KEMGU emitter TAM literallerini doğrudan immediate
basar (C'nin `add i32 0, N`'inden daha sıkı ama semantik aynı).

**Doğrulama:** `test/codegen_diff_harness.sh` (SEMANTİK exit-kod oracle, D-072 KARAR 1) —
`test/cg_korpus/` 5 program (sabit/aritmetik/çıkarma/bölme/mod), **5/5 C-codegen ile exit
eşdeğer**. Makefile `calistir_codegen_diff` hedefi `test_tumu`'ya bağlandı (codegen.exe yokken
harness graceful → geriye uyumlu).

**Sınır/Sonraki:** CG1 dışı düğümler (tanımlayıcı/çağrı/değişken/eğer/...) henüz `0` üretir
(placeholder; korpus onları içermez). Sonraki: **CG2** — ikili karşılaştırma/mantıksal
kısa-devre + tekli (neg/değil); ardından CG3 değişken/atama/tanımlayıcı (entry alloca).

---

## D-072 — AŞAMA 3 (codegen self-host) ADIM-0: oracle + temsil + CG plan (2026-06-14)

**Karar [ETKİ: yok — dokümantasyon/plan; kod yok].** Bootstrap fixpoint'in (Aşama 5) asıl
darboğazı = codegen self-host: `src/llvm.c` (5271 satır, 34 düğüm tipi) → KEMGU'da yeniden
yazım (`selfhost/codegen.kem`). Lexer/parser/checker self-host'larındaki gibi ADIM-0 = envanter
+ oracle kararı + temsil + milestone planı.

**KARAR 1 — Oracle: SEMANTİK (exit-kod) eşdeğerliği, byte-identik IR-DİFF DEĞİL.** Parser/checker
oracle'ları düz-dump diff'iydi; codegen IR'ı için byte-identik diff ÇOK KIRILGAN: C codegen'in
SSA reg numaralandırması, D-041 hoist_renumber, formatlama = uygulama detayı (KEMGU codegen aynı
byte'ı üretmek zorunda kalmamalı). Bunun yerine: korpus programını HEM C codegen (`kemgu --llvm |
clang | run → exit`) HEM KEMGU codegen (`codegen.exe in.kem | clang | run → exit`) ile derle,
EXIT KODLARINI karşılaştır. Semantik eşdeğerlik = doğru oracle (metinsel değil). Korpus: test/
ornekler (main'li) + test_llvm gömülü programları (~199).

**KARAR 2 — Temsil: codegen.kem = selfhost parser (REUSE) + AST-yürüten IR text emitter.**
checker.kem deseni (parser + checker) gibi codegen.kem = parser + codegen. Düz AST tablosunu
(a_ad/a_deg/cocuk...) gezer; LLVM IR'ı yaz_str/yb ile basar. **Hoist-FREE tasarım:** alloca'lar
DOĞRUDAN entry bloğuna emit edilir (C'nin inline-alloca+hoist_renumber'ı YENİDEN YAZILMAZ —
D-041 sorunu baştan önlenir). SSA reg sıralı sayaç (LLVM unnamed value zorunluluğu); etiketler
%bbN. Runtime intrinsic declare'ları sabit header (llvm.c'deki gibi).

**KARAR 3 — CG milestone planı (her biri semantik-oracle kapılı, küçük korpus):**
- CG1: tam literal + işlev(main) + ver → `define i32 @main(){ret i32 N}`.
- CG2: ikili (aritmetik/karşılaştırma/mantıksal kısa-devre) + tekli.
- CG3: değişken (entry alloca/store/load) + atama + tanımlayıcı.
- CG4: eğer/iken (br + %bbN bloklar) — kontrol akışı.
- CG5: çağrı (call) + parametre alloca + özyineleme (fib/faktöriyel).
- CG6: multi-int (i8/16/64, dtam) + sext/trunc dönüşüm + işaretsiz.
- CG7: metin literali (@.str global) + yapı (%T type, alloca, gep) + erişim.
- CG8: dizi (heap KdlDizi + stack [N×T] + sınır-kontrol D-069) + indeks + için.
- CG9: çeşit/eşleş + generic mono + lambda/closure (D-071) + modül + cross-file.

**Riskler:** (a) 34 düğüm tipi = geniş yüzey, çok-pencere iş; (b) SSA sıralı numaralandırma
(KEMGU'da sayaç + dikkatli emit); (c) runtime ABI (yetki sret, yapı by-value) birebir; (d)
korpus seçimi (yalnız main'li + codegen-tam programlar; --check-only/parse-only hariç).
**Sonraki:** CG1 — codegen.kem (parser kopyası + minimal emitter) + codegen_diff_harness.sh.

---

## D-071 [YÜKSEK] — Sınıf B lambda/closure codegen V2: KARMA temsil (kabul-ama-çöküyor kapandı) (2026-06-14)

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit].** Güvenlik-iddiası izi
(D-070 devamı). D-031 Sınıf B'nin 4 lambda örneği (`04_islev`, `10_lambda`, `42_lambda_hesap`,
`25_closure_capture`) lambda codegen YOKLUĞUNDAN çöp fn-ptr çağrısı → SEGFAULT yapıyordu
(D-004 ertelemesi). Lambda/closure codegen sıfırdan yazıldı. C derleyici codegen değişti.

**Tasarım (5-ajan workflow ile doğrulandı, opt -passes=verify):** KARMA temsil —
- **Yakalamasız lambda → BARE fn-ptr** (`bitcast @lambda_N to ptr`; top-level fn ile aynı ABI;
  bare-call). `işlev(T)→R` param da bare-call → top-level fn VE yakalamasız lambda ikisi de geçer.
- **Yakalamalı lambda → CLOSURE** stack `{ ptr fn, ptr env }`; lifted `@lambda_N(ptr env, params)`
  capture'ları env'den load. Lokal değişken `closure_mu=1` → çağrıda env-unpack.
- Lifted fn'ler DEFERRED emit (BekleyenLambda kuyruğu, generic mono deseni; çevre fn gövdesi
  bitince — INLINE emit IR'ı bozardı). Capture analizi (`lambda_serbest_tara`) OLUŞTURMA anında
  (scope canlı) yapılıp kayda konur. D-041 hoist_renumber `%bbN`/`%env` (named) korur.

**Neden karma (uniform değil):** İlk deneme tüm lambda'ları closure + işlev-param closure_mu=1
yaptı → stdlib map/filtre/indirgeme (top-level fn'i işlev param'a geçiyor: `harita(xs, iki_kat)`)
KIRILDI (closure-unpack ham fn-ptr'da → çöp). Karma temsilde yakalamasız lambda = top-level fn
= bare fn-ptr → işlev param bare-call → ikisi de çalışır + yakalamalı (25) closure ile.

**Doğrulama:** 4 örnek → exit 42 (closure capture dahil); ASan/UBSan TEMİZ (4/4); opt verify
PASS; **test_llvm 235/235** (stdlib higher-order regresyonu çözüldü); bounds 11/11; checker
48/48; self-host 3/3; ASan E2E **PASS=91 FAIL=0** (4 lambda allowlist'ten çıktı → korumalı PASS;
ALLOW 6→2, yalnız G003-red 35/40). Yeni `make calistir_lambda_test` (test_tumu'da) 4/4. 0 uyarı.

**KAPSAM-DIŞI (V2/D-072):** yakalamalı lambda'yı işlev param'a geçirme (call-site trampolin
gerek); blok-form gövde son-ver çıkarsama (`||{...}` — lineer_closure/29_linear_closure);
dönüş tipi i32-dışı (call-site/lifted-fn senkronu — IR "ptr" tip taşımıyor); yapı/dizi/&T
capture; lambda escape (env stack — şu an non-escaping KEMGU v1 garantisi). **Sınıf B kapandı;
8 kabul-ama-çöküyor deliğinin tümü artık ya düzeltildi (D-070 literal, D-071 lambda) ya da
checker-reddi (D-070 G003 değişken-arg).**

---

## D-070 [YÜKSEK] — Sınıf A kabul-ama-çöküyor: dizi-LİTERAL → Dizi<T> param → heap (UB kapandı) (2026-06-14)

**Karar [ETKİ: orta — `src/llvm.c` CAGRI codegen; izole commit].** Güvenlik-iddiası izi
(D-069 devamı). D-031'de teşhis edilen "8 kabul-ama-çöküyor" deliğinden Sınıf A'nın
LİTERAL-arg kısmı kapatıldı. C derleyici codegen değişti.

**Hole (D-031 Sınıf A):** `f([1,2,3])` — `f(xs: Dizi<tam32>)` KdlDizi* bekler ama array
literal STACK `[N x T]` üretir. Callee `xs`'i KdlDizi* sanıp `için`/`[]`/`dizi_boyut` ile
okur → **misaligned access UB / SEGFAULT** (UBSan: "member access ... requires 8 byte
alignment"). `--check` geçer (tip sistemi stack/heap temsilini ayırmaz) → görünmez delik,
#1 iddia "Kırılamaz Güvenlik" ihlali.

**Fix:** CAGRI normal-çağrı arg döngüsünde, callee param[i] tipi `DUGUM_TIP_DIZI` ise
`g->beklenen_tip = param.tip` (AST düğümü) verilir → `DUGUM_DIZI_OLUSTUR` HEAP `kdl_dizi_olustur`
üretir (D-044 mekanizması A). `IslevKayit.ast` tüm işlevler için kayıtlı (line 861) → param
tipleri çağrı yerinde erişilir. **D-044'ün açıkça belirttiği "TÜM Dizi<T> bağlamları heap"
amacını çağrı-arg için tamamlar** (D-044 yalnız yapı-alanı setter'ını yapmıştı) → yeni DUR-SOR
DEĞİL, settled option-b'nin tutarlı uygulaması.

**Doğrulama:** 03_kontrol.kem (tek hayatta kalan literal-arg örneği; 36_quicksort silinmiş) →
ASan/UBSan TEMİZ + rc=151 (120+30+1 doğru sonuç, eskiden UB). test_llvm 235/235; bounds 10/10;
checker 48/48; ASan E2E denetim **PASS=88 FAIL=0** (03_kontrol allowlist'ten çıktı → korumalı
PASS; ALLOW 8→6). 0 uyarı.

**DEĞİŞKEN-arg → ÇÖZÜLDÜ (Mehmet kararı: checker reddi / G003).** `değişken xs=[..]; f(xs)` —
stack-array DEĞİŞKENİ Dizi<T> param'a. Literal-route uygulanamaz (arg TANIMLAYICI). Mehmet
**checker-reddi**ni seçti: C tip denetleyici (tip_kontrol.c CAGRI pas-2) — param `TIP_DIZI`
iken arg TANIMLAYICI ve sembolün ast_dugumu annotasyonsuz `değişken x=[literal]`
(DUGUM_DIZI_OLUSTUR) ise → **G003** ("stack dizi degiskeni Dizi<T> parametresine gecirilemez;
annotasyonlu heap Dizi kullanin"). Çökme yerine compile-time red (çökmezlik #1). Programcı
`değişken xs: Dizi<T> = [..]` (heap) kullanır. 35/40 artık --check'te G003 reddi (codegen'e
ulaşmaz; --llvm bypass ederse ASan-allowlist'te kalır = "checker'ı atladın" = güvensiz-eşi).
Doğrulama: 35/40→G003; 03_kontrol (literal)→OK; annotasyonlu→OK; ornekler 42/42; korpus 48/48;
test_llvm 235/235; bounds harness vaka9 (G003 reddi) 11/11.
**Self-host mirror (follow-up, TC9):** checker.kem'de G003 = Dizi-param + stack-array-var
izleme infra'sı gerek (fn_ptip "?" bileşik tipte; ayrı flag). Gating parite kırılmıyor
(42/42 korunur — hiç geçerli örnek G003 tetiklemiyor). Sınıf B (lambda) = V2 (D-004), ayrı.

---

## D-069 — Dizi sınır-güvenliği: OOB → panic (sessiz-0 / segfault DEĞİL) — Kategori 1 (heap) + 2 (stack) DONE (2026-06-14)

**Bağlam (firsthand doğrulandı):** Dizi indekslemenin iki yolu da bellek-güvensizdi:
- **Heap** (`kdl_dizi_al_tam/tam64/ptr`, kdl_runtime.c:557-570): sınır kontrolü VARDI ama
  OOB'da `return 0`/`NULL` → sessiz yanlış değer / downstream NULL-deref (D-065 segfault'unun
  asıl nedeni: OOB→NULL→`metin_esit(NULL,…)`).
- **Heap yazma** (`kdl_dizi_yaz_*`, :575-588): OOB'da sessizce yok sayılıyordu.
- **Stack/GEP** (`src/llvm.c` DUGUM_INDEKS:1964): ham GEP+load, kontrol YOK → OOB=segfault.

Parite-audit'ine GÖRÜNMEZ delik (C de aynı şekilde güvensiz → divergence çıkmaz). #1 iddia
("Kırılamaz Güvenlik") ile çelişiyor. Güvenlik-iddiası izinin ilk kalemi.

**Karar:** Dizi indeksleme varsayılan güvenli — OOB (`i<0` veya `i>=boyut`) → **panic**
(temiz, yakalanabilir durma); asla segfault, asla sessiz-0/noop. Üç kategori; perf gerilimi
`güvensiz` opt-out ile çözülür (Rust modeli: varsayılan güvenli, açık+işaretli opt-out).

**Panic mekanizması (eklendi):** `kdl_panik(const char *)` (kdl_runtime.c, hosted): stderr'e
`"PANIK: <mesaj>"` + `abort()` (`__attribute__((noreturn))`). Mevcut `kdl_panik_dur` yalnız
bare-metal/mock'tu (`#error` ile hosted'ta yoktu) → hosted runtime artık panic edebiliyor.

**KATEGORİ 1 (heap) — DONE [maliyet SIFIR: karşılaştırma zaten vardı]:** `kdl_dizi_al_*` ve
`kdl_dizi_yaz_*` OOB-dalı `return 0`/noop → `kdl_dizi_oob(i, boyut)` (mesaj "dizi sınır ihlali
(i=…, boyut=…)", noreturn). NULL-dizi (d==NULL) ayrı durum → şimdilik return 0/NULL korunur
(D-070+). Koşulsuz, varsayılan güvenli.

**LATENT BUG yakalandı (güvenliğin değeri kanıtı):** Kategori 1 açar açmaz `test_llvm [155]`
(17_kontrol_dili.kem mini-yorumlayıcı) PANIC verdi → `faktor`/`deyim_calistir`/`calistir`
EOF'ta `dizi_al(t.kind, p)` ve `dizi_al(t.kind, p+1)` OKUYOR (sessiz-0'ı EOF-sentineli
sanıyordu). Açık sınır kontrolü eklendi (`p>=boyut→ver 0`, `p+1<boyut ve …` — `ve`
kısa-devre). Davranış korundu, bellek-güvenli oldu. Bu tam da #1-iddianın yakalaması gereken
sınıf.

**KATEGORİ 2 (stack `[N×T]`) — DONE [YÜKSEK — codegen, commit ayrı]:** `LlvmIsim.dizi_uzunluk`
eklendi; DEGISKEN annot-yok dalı değer `DUGUM_DIZI_OLUSTUR` ise N kaydeder. `DUGUM_INDEKS`
stack yolu GEP'ten ÖNCE: `icmp uge i64 idx, N` (unsigned → negatif=dev-unsigned + i>=N tek
seferde) → `br`→`bb<oob>`(call @kdl_panik + unreachable) / `bb<ok>`(GEP+load). IR header'a
`declare void @kdl_panik(ptr)` + `@.str.dizi_sinir_panik` global. Etiketler `%bbN` (hoist_renumber
`%<digit>` dokunmaz → D-041 güvenli). **Stack OOB artık segfault DEĞİL → panic.**
Doğrulama: vaka5/6 (stack OOB/negatif → PANIC, eskiden rc=139); test_llvm **235/235**;
opt -passes=verify PASS; ASan temiz (panic erişimden önce → 0 OOB raporu); 0 uyarı.
**Opt-out (perf, Rust modeli):** `LlvmGen.guvensiz_derinlik` — `güvensiz` blok içinde stack
sınır-kontrolü ATLANIR (vaka8: güvensiz arr[i] → 0 panic-IR; dışında → kontrollü). Varsayılan
güvenli, opt-out açık+işaretli+programcı sorumluluğunda.

**FOLLOW-UP — Cat2 stack YAZMA deliği kapandı (2026-06-14):** İlk Cat2 implementasyonu yalnız
**OKUMA** yolunu (`DUGUM_INDEKS` → GEP+load) sınır-kontrol etti; **YAZMA** yolu (`DUGUM_ATAMA`
hedefi `DUGUM_INDEKS`, `arr[i]=v`, src/llvm.c) kontrolsüz GEP+store yapıyordu → sessiz stack
taşması (kabul-ama-sessizce-yanlış: `arr[10]=9` rc=0). Heap yazma (D-083 `kdl_dizi_yaz_*`)
zaten runtime'da kontrollüydü; delik yalnız stack yazma codegen'ineydi. **Fix:** yazma dalında
`LlvmIsim.dizi_uzunluk` ile okuma yolunun aynısı GEP+store'dan ÖNCE (`icmp uge i64 idx, N` →
`@kdl_panik(@.str.dizi_sinir_panik)` + unreachable). `güvensiz` opt-out yazmada da geçerli
(`guvensiz_derinlik==0` gate). Bölge-`*T` yolu (`pointee_elem`) `dizi_uzunluk=0` → kontrolsüz
(uzunluk yok, Cat3 ile tutarlı). **Doğrulama:** harness'a vaka5b/6b (OOB/negatif yazma →
PANIC), vaka7c (geçerli yazma → rc=9), vaka8b (güvensiz yazma → 0 panic-IR) eklendi →
`calistir_dizi_sinir_test` **15/15**; test_llvm **235/235**; codegen korpus 48/48; test_tumu
yeşil; 0 uyarı.

**KATEGORİ 3 (ham `*T` bölge-tabanı) — KARAR: güvensiz-only opt-out, ZATEN ENFORCE [inceleme
tamam]:** Firsthand bulgu: (1) bölge-container UZUNLUK TAŞIMIYOR — `kdl_bolge_ayir(a, boyut)`
ham `void*` taban döndürür; `*T` çıplak pointer, boyut yok → kontrol edilemez. (2) Ham `*T`
indekslemesi ZATEN `güvensiz`-only: C checker DUGUM_INDEKS (tip_kontrol.c:3295-3305)
`TIP_POINTER` indeksini `guvensiz_baglam==0` iken → **G001** ("*T pointer indeksleme yalniz
guvensiz blok icinde"). Bu tam da spec'in "varsayılan güvenli + açık opt-out" modeli — ZATEN
var. (3) Cat2 codegen'i bölge-`*T` yolunu kontrolsüz bırakır (stack_uzunluk=0 yalnız sabit
`[N×T]` literalinde >0; region `*T` → 0 → ham GEP). Tutarlı.

**Karar:** Ham `*T` region indekslemesi = `güvensiz`-only (G001 zaten enforce; opt-out açık+
işaretli+programcı sorumluluğunda — Rust modeli). Kontrol edilemez (uzunluk yok) ama erişim
yalnız güvensiz blokta. Uzunluk-taşıyan region handle (fat pointer) → ileri iş (D-070+).
Kod değişikliği GEREKMEZ (model zaten doğru). Self-host checker'da G001 henüz yok = ayrı TC
(güvensiz/pointer); bu DEĞİL — prod C checker zaten enforce ediyor.

**SONUÇ:** D-069 üç kategori de kapandı (Cat1 heap + Cat2 stack implemente; Cat3 karar+zaten-
enforce). Dizi-sınır bellek-güvenliği boyutu TAMAM. Kalan güvenlik-iddiası izi (D-070+):
8 "kabul-ama-çöküyor" deliği + scoping false-negative + NULL-dizi (d==NULL) + region fat-pointer.

**Doğrulama:** Yeni `make calistir_dizi_sinir_test` (test_tumu'ya bağlı) → **6/6** (heap-OOB-oku/
negatif/yaz/ptr → PANIC; geçerli → rc=60; D-065 koruması → segfault yok). `test_llvm` **235/235**
(latent bug fix sonrası). checker korpus 48/48; test/ornekler 42/42; self-host 3/3 (0 panik).
Hosted runtime 0 uyarı (`-Wall -Wextra -Wpedantic`).

**Kapsam-dışı (güvenlik-iddiası boşluk izi, D-070+):** Kategori 2/3 (yukarıda); 8 "kabul-ama-
çöküyor" deliği (Sınıf A dizi-literal-param, Sınıf B lambda); scoping false-negative (gölgeleme).

---

## D-068 — SELF-HOST checker TC8a: cross-file/modül import (kullan → dışa toplama) — 246/319 (2026-06-14)

**Karar [ETKİ: orta — `selfhost/checker.kem`; cross-file altyapı].** TC8 başlangıcı: `kullan`
ile içe aktarılan modüllerin `dışa`-export adlarını (transitif) toplayıp false-T002'yi kapat.
C derleyici DOKUNULMADI.

**Mimari:** `kullan_yukle_hepsi` (üst-düzey `kullan`'lar, giriş dizininden) → `modul_yukle`
(dedup `kullan_gorulen` → yol çöz → 3 arama yolu → taze `Ayr`'a lex+parse → `dışa` iç-adları
`g_isim`'e ekle → modülün `kullan`'ını transitif izle). Yol: `modul_path` (a::b::c → a/b/c.kem
via `metin_yer_degistir`); `modul_icerik` (C 3-yol: importer-dizin → kök → kütüphane/);
`dizin_al` (son '/'). builtin_ekle'den SONRA, genel_topla'dan ÖNCE (T002 öncesi g_isim hazır).

**Kapsam (TC8a):** YALNIZ `dışa` ADLARI toplanır (flat-görünür; çok-segment çıplak yol
düzleştirme — C legacy davranışı). İmza/param tipleri TC8b (cross-file fonksiyon
return/arity — şimdilik ad-only → false-pos yok, under-report). T040 (bulunamadı)/T041
(private)/T042 (ambiguous) modül-edge → TC8b. Modül gövdeleri tip-kontrol EDİLMEZ (yalnız
importer için ad toplama).

**Doğrulama:** test/crossfile transitif/sonuc_cagri/lib_islem/lib_sonuc **4/4** (transitif
zincir: transitif→lib_islem→lib_sayi `iki_kat` çözülür). Full audit **246/319** (önceki 243;
+3 crossfile, SIFIR yeni regresyon). korpus 48/48; ornekler 42/42; self-host 3/3.

**Kalan 73 farklı:** lex_korpus (22) + parse_korpus (12) = parser/lexer P/L kodu (~23);
snapshots (16: bölge/asm/çeşit/constraint/referans); virtio (12: yetki TC7+bölge TC6 — cross-
file kısmı çözüldü ama yetki/bölge kaldı); moduller (5: T040/41/42 edge); stdlib (3); eski (2).

---

## D-067 — SELF-HOST checker: full-repo parite audit SONUÇ + TC6-9 yol haritası (2026-06-14)

**Karar [ETKİ: yok — dokümantasyon; ultracode workflow audit sonucu].** 319 .kem dosyası
üzerinde KEMGU checker vs C oracle (`--checkdump`) tam tarama. Genuine-bug'lar kapatıldıktan
sonra durum + kalan feature-gap yol haritası.

**Audit sonucu:** **243/319 birebir, 76 farklı, 0 çökme** (başlangıç: 233/315, 82 farklı,
1 çökme). Kapatılan genuine-bug'lar: D-064 generic-T003 (stdlib 3), D-065 segfault (m3_04),
D-066 bit-T028 (snapshots 2). Geçerli kodda checker artık SAHTE-HATA üretmiyor; self-host
kaynaklar (lexer/parser/checker.kem) **3/3 birebir** → **checker bootstrap-HAZIR**
(geçerli derleyici kaynakları sahte-hatasız kabul ediliyor; çökmüyor).

**Kalan 76 farkın kategorizasyonu (oracle ilk-kod + dizin):**
1. **Parser/lexer hata-kodu raporlama (~23 dosya):** P001×15, P031×4, P015×3, L009×1.
   test/lex_korpus (22) + test/parse_korpus (12) — token/parça testleri; geçersiz program →
   C parser P-kodu basar, KEMGU checker'ın parser'ı kurtarıp OK/farklı basar. KEMGU parser
   hata kurtarıyor ama P/L kodunu th_kod'a YAZMIYOR. (Muhtemelen Aşama-1 parser-oracle
   kapsamı; checker-parite için P/L emit gerekli.)
2. **Cross-file/modül TC8 (~28 dosya):** oracle-OK×16 (false-T002) + T002×11 + T011×8 +
   T040/41/42×3. drivers/virtio (12), test/moduller (5), test/crossfile (3), test/stdlib (3),
   kütüphane (1), snapshots/21_modul_kullan. `kullan` import + cross-file sembol çözümü yok →
   KEMGU T002. Mimari: diğer .kem yükle + sembol birleştir.
3. **Misc feature-gaps (~10 dosya):** referans/deref T001×6 (26_referans_aktarim — T022 birebir
   ama `*r`/`&T` tip çıkarsama eksik), bölge BL001 (TC6), asm AS001/G002, çeşit M001
   (exhaustiveness), constraint T007 (TC bound), cast E002.

**Değerlendirme — Aşama 5 için:** Kalan 76 = TC9 GENİŞLİK (breadth); bootstrap-kritik DEĞİL
(self-host kaynaklar zaten 3/3). Bootstrap'a en büyük kaldıraç = Aşama 3 codegen self-host
(llvm.c → KEMGU, IR-diff oracle), checker breadth değil. Öncelik kullanıcı kararı: (a) TC8
cross-file (en çok dosya, drivers/stdlib değeri) · (b) parser P/L emit · (c) Aşama 3 codegen.

---

## D-066 — SELF-HOST checker TC5d: bit operatörü tamsayı kontrolü (T028) — 48/48 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Full-repo audit genuine-bug
#3: bit operatörü (`& | ^ << >>` + tekli `~`) operandı tamsayı olmalı (T028). C derleyici
DOKUNULMADI.

**Bulgu (audit):** test/snapshots/31_bit_komb.kem + 50_kompleks_program.kem → oracle T028,
KEMGU YANLIŞ T020. Örn `(deger >> pozisyon) & 1 == 1` → öncelik gereği `& (1==1)` → bit op
sağ operandı mantıksal. C: T028 (& operatör konumu). KEMGU bit op'u kontrol etmiyordu →
ifade_tip(&) sol tipini (tam32) döndürüyor → ver dönüşü (mantıksal) ile T020 → YANLIŞ KOD.

**Düzeltme (T003/T004 ile simetrik):** `tamsayi_mi` (tam/dtam; kesirli/mantıksal HARİÇ) +
`bit_op_mi` + `bilinen_tamsayi_degil`. `t028_kontrol` (IKILI bit op operandı kesin
tamsayı-değil → T028) + tekli_kontrol `~` dalı. `ifade_tip` bit op/`~` non-tamsayı operandda
"?" döndürür (C TIP_HATA bastırma) → dış T020/T001 bastırılır → iç-içe tek T028.

**Doğrulama:** 31_bit_komb ✅ + 50_kompleks ✅ (artık T028 birebir, T020 değil).
`make calistir_checker_diff` **48/48** (+tc5d: bit-OK / bit-T028 / tekli-~-T028).
test/ornekler 42/42; self-host 3/3; SIFIR regresyon (geçerli bit kodu tam-integer → T028 yok).

**Audit ilerleme:** 3 genuine-bug kapandı (generic-T003, segfault, bit-T028). Kalan farklar
büyük oranda feature-gap: parser/lexer P/L kodu raporlama (lex/parse_korpus), cross-file/modül
TC8 (virtio/crossfile/moduller), bölge TC6 (bolge_al), referans/deref tip (26_referans_aktarim
— T022 birebir ama deref T001 eksik), asm/çeşit/constraint. → TC6-9 yol haritası.

---

## D-065 — SELF-HOST [YÜKSEK robustness]: parser token erişimi sınır-güvenli (segfault düzeltildi) (2026-06-14)

**Karar [ETKİ: orta — `selfhost/checker.kem` + `selfhost/parser.kem`; robustness/çökmezlik].**
Full-repo parite audit'inde KEMGU checker `test/lex_korpus/m3_04_ayrac_hata.kem` üzerinde
SEGFAULT (rc=139) veriyordu. Kök neden bulundu + düzeltildi. C derleyici DOKUNULMADI.

**Kök neden (bisect ile):** Çöken yapı = `yapı Nokta x y z` (süslü `{` yok). Parser bozuk
girdide panik-sync yapmadığından `parse_alan`/`bekle` döngüsü `p.imlec`'i DOSYA_SONU
sentinelinin ÖTESİNE ilerletiyor; sonra `tip_i`/`lex_i` → `dizi_al(p.t_ad, i)` sınır-dışı →
segfault. KEMGU'nun "çökmezlik" (Direktif) ilkesine aykırı kritik bir robustness hatası.

**Düzeltme (sınır-güvenli accessor):** `tip_i` → i sınır-dışıysa "DOSYA_SONU"; `lex_i` →
"". Böylece imleç taşsa bile tüm parse döngülerinin `sim_mi(DOSYA_SONU)` kontrolü sonlanır;
çökme ya da sonsuz döngü yok. C lexer'ın DOSYA_SONU sentineli + bounded-peek davranışının
karşılığı. **Hem checker.kem hem parser.kem'e** uygulandı (paylaşılan parser kodu, aynı
latent bug).

**Doğrulama:** m3_04 artık rc=0 (çökme yok); `yapı Nokta x y z` tek başına rc=0. Parser
bootstrap **270/270** sıfır-diff (self-parse dahil); parser diff 12/12; checker korpus 45/45;
test/ornekler 42/42; self-host lexer/parser/checker.kem 3/3. SIFIR regresyon.

**Not:** m3_04 artık "OK" basıyor (oracle P-kodları basıyor) → hâlâ DIVERGENT ama ÇÖKMÜYOR.
m3_04 tam paritesi = parser/lexer hata-kodu raporlama (P/L kodları) feature-gap'ine bağlı
(checker'ın parser'ı hata kurtarıyor ama P/L kodu th_kod'a yazmıyor) — ayrı iş (muhtemelen
Aşama 1 parser-oracle kapsamı; checker-parite dışı).

---

## D-064 — SELF-HOST checker: generic param tip "?" (full-repo parite denetimi başladı) — stdlib 3/3, 45/45 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Tüm-repo parite denetimi
(315 .kem dosyası, ultracode workflow ile) başlatıldı → 233/315 birebir, 82 farklı. İlk
genuine-bug düzeltildi: generic işlevlerde false T003/T020. C derleyici DOKUNULMADI.

**Bağlam — full-repo parite audit:** KEMGU checker (kemcheck.exe) C oracle'a (`--checkdump`)
karşı TÜM repo .kem dosyalarında tarandı. 82 fark kategorize edildi: çoğu feature-gap
(parser/lexer P/L kodları, cross-file/modül TC8, bölge TC6, yetki TC7/CP005, asm, çeşit
M001, generic-constraint) + birkaç genuine-bug (CRASH m3_04, generic-T003, yanlış-kod 26/31/50).

**Genuine-bug #1 — generic param false T003/T020 (workflow agent kök-neden).** `mutlak<T>(x: T)
-> T { eğer x < 0 { ver 0 - x; } ... }` → oracle OK, KEMGU 29 false T003 (stdlib/temel/*).
Sebep: `yerel_topla` param/değişken tip-string'ini ham saklıyor; generic `x: T` → "T" →
`bilinen_sayisal_degil("T")`=doğru → T003. Ayrıca dönüş tipi "T" → aktif_donus "T" → ver
çıkarsanan "tam32" ≠ "T" → T020. C: TIP_GENERIC_PARAM için tip_sayisal_mi "deferred true".

**Düzeltme:** `yerel_tip_filtrele(t)` = bilinen-skaler VEYA bilinen-yapı → t, aksi "?".
İki yerel_topla write-site'ında (param tip_str, değişken annot_str) + kontrol_govde
aktif_donus (donus_str) uygulandı. Generic "T" → "?" → kontrol atlanır; yapı adları
(ERISIM/TC4) KORUNUR.

**Doğrulama:** stdlib/temel matematik+karsilastir+sayisal **3/3** (29 false T003+T020 kapandı);
self-host lexer/parser/checker.kem **3/3**; `make calistir_checker_diff` **45/45** (+tc5c_01
generic); test/ornekler **42/42** (regression yok). Workflow agent risk-analizi + empirik
doğrulama uyumlu.

**Sıradaki genuine-bug'lar:** CRASH m3_04_ayrac_hata (parser sınır-dışı/sonsuz döngü → segfault),
yanlış-kod 26/31/50 (T022 vs T001, T020 vs T028). Feature-gap'ler TC6-9 yol haritasına.

---

## D-063 — SELF-HOST checker: aynı-ad belirsiz tip → "?" (self-host kaynak paritesi) — lexer+parser+checker.kem TAM (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem`].** KEMGU checker'ı KENDİ derleyici
kaynakları üzerinde (lexer/parser/checker.kem) C oracle'ına karşı doğrulandı → **3/3
self-host kaynak BİREBİR**. Bir false-positive bulundu ve düzeltildi. C derleyici
DOKUNULMADI.

**Bulgu (bağımsız doğrulama):** `parser.kem:969` `ver ic;` → KEMGU FALSE T020, oracle OK.
Sebep: `parse_birincil`'de iki ayrı blok kapsamında iki `ic` (`metin` @935, `tam32` @967);
düz `yerel` listesi kapsam tutmaz, `var_tip` ileri-arama İLK eşleşmeyi (`metin`) döndürdü →
`ver ic` (tam32 fonksiyonda) metin sanıldı → T020. C blok kapsamlarıyla doğru çözer.

**Düzeltme (GÜVENLİ):** `var_tip` — bir ad birden fazla FARKLI tiple bağlıysa → "?"
(belirsiz → kontrol atla). Tek tip → o tip. Yalnız under-report (false-positive önler,
hiç error EKLEMEZ). Hem false T020 (ver ic) hem olası false T001'i (dugum0 arg ic) kapatır.

**Doğrulama:** self-host kaynaklar **lexer.kem ✅ parser.kem ✅ checker.kem ✅** (hepsi
oracle = KEMGU). `make calistir_checker_diff` **44/44**; test/ornekler **42/42** (regression
yok — yalnız belirsiz-ad checkleri atlanır, geçerli kodda zaten error yok).

**Önemi (Aşama 5 hazırlığı):** KEMGU checker artık TÜM derleyiciyi (kendisi dâhil) C ile
birebir tip-kontrol ediyor — bootstrap fixpoint için checker accept/reject doğruluğu
KANITLANDI (geçerli kaynak → kabul, sahte hata yok).

---

## D-062 — SELF-HOST checker TC5b: lineer akış L001/L002/L004 — 🎉 test/ornekler 42/42 TAM PARİTE (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC5b (Linear
Types Spec V1 akış denetimi): L001 (scope sonu tüketilmedi), L002 (move sonrası erişim),
L004 (lineer referans). **lineer_hata.kem kapandı → test/ornekler 42/42 TAM accept/reject
+ tanı paritesi.** C derleyici DOKUNULMADI.

**Mimari — lineer bağlama izleme (aktif işlev dilimi).** `Ayr`'a lin_ad/lin_sat/lin_sut/
lin_tuk (paralel Dizi) + lin_basla (dilim başı; `yerel` deseni). `kontrol_govde`: girişte
lin_basla + lineer parametreler (`lin_param_topla`); çıkışta `lin_kapanis` (tüketilmemiş →
L001 bildirim konumunda). Tüketim noktaları (`lin_tuket_dugum`, tekrar → L002 düğüm
konumunda): kullan/imha (KULLAN_IFADE/IMHA_IFADE), çağrı-arg→lineer-param (fn_plin), ver
değeri, değişken move. L004: `&`/`&değişken` lineer bağlama → tekli_kontrol'da.

**KRİTİK karar — Linear V1 = YALNIZ tekkez (`tip_node_tekkez_mi`).** C `tip_lineer_mi`
tekkez+yetki+görev kapsar AMA tüketilmemiş yetki→CP005, görev→DRF (L001 DEĞİL). Bu yüzden
L001/L002/L004 izleme TEKKEZ'e kısıtlandı → mmio_smoke (yetki<MMIO>) FALSE-L001 vermez.
**LR002 GENİŞ kalır** (tekkez+yetki+görev) — geçerli yapıda hiç lineer alan yok →
false-positive yok. (yetki CP005 = TC7, görev DRF = TC6.)

**Doğrulama:** lineer_hata.kem KEMGU = oracle BİREBİR: LR002 24:5, L001 7:5, L002 13:28,
L004 18:20. `make calistir_checker_diff` → **44/44 korpus** (önceki 40 + TC5b 4: L001/L002/
L004/temiz). **test/ornekler 42/42** (lineer_temel/closure OK; mmio_smoke OK; regression yok).

**Bilinen sınır (TC5 kalan):** L007/L008 (consume operand tekkez değil / tekkez_olustur
arity); kapsam blok-düzeyi değil işlev-düzeyi (lineer_hata/temel/closure'da fark yok);
closure LC-2/LC-3 (yakalama → consume-at-traversal modeliyle örtüşüyor). Sıradaki: yetki
CP005 (TC7), bölge (TC6), modül (TC8), tam-korpus (TC9) → Aşama 3 codegen.

---

## D-061 — SELF-HOST checker TC5a: yapı lineer alan yasağı (LR002) — 40/40 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC5a (Linear
Types Spec V1 başlangıcı): yapı lineer-tipli alan içeremez (LR002). C derleyici
DOKUNULMADI.

**Kapsam:** `lr002_kontrol` pre-pass (gövdelerden ÖNCE, `kontrol_ust`'tan önce) —
üst-düzey/modül/dışa YAPI'ların alanlarını gezer; alan tipi lineer ise (`tip_node_lineer_mi`:
TIP_TEKKEZ/TIP_YETKI/TIP_GOREV) → LR002 alan düğümünde. C `tip_lineer_mi` (tekkez+yetki+
görev; kanal/sabitsüre HARİÇ) birebir; konum ALAN düğümü (--checkdump: lineer_hata
LR002 24:5).

**Sıra kararı:** LR002 pre_populate'te (gövdelerden önce) → çok-hatalı dosyada (lineer_hata)
LR002 İLK çıkar (L001/L002/L004 gövde hatalarından önce). Bu yüzden ayrı pre-pass.

**Doğrulama:** `make calistir_checker_diff` → **40/40 korpus** (önceki 38 + TC5a 2:
lineer-alan-LR002 / lineer-olmayan-OK). test/ornekler **41/42** (lineer_hata hâlâ
diff — L001/L002/L004 TC5b'de; geçerli yapılarda false-LR002 YOK).

**Sıradaki (TC5b):** lineer akış — L001 (scope sonu tüketilmedi), L002 (move sonrası
erişim), L004 (lineer referans). Lineer bağlama izleme (tekkez_olustur değer / tekkez
annotasyon / lineer param) + tüketim noktaları (kullan/imha/çağrı-arg/ver). lineer_hata
→ 42/42 kapanır.

---

## D-060 — SELF-HOST checker TC4a: yapı oluştur (T002/T017/T012/T001) + erişim tipi — 38/38 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC4a: yapı
oluşturma denetimi (bilinmeyen yapı T002, bilinmeyen alan T017, eksik alan T012, alan
değer tipi T001) + alan erişim tip çıkarsama (`nesne.alan` → alan tipi). C derleyici
DOKUNULMADI.

**Mimari karar — yapı tablosu (`yapi_ad`/`yapi_abase`/`yapi_acount` + düz `alan_ad`/
`alan_tip`).** `genel_topla` YAPI (+ dışa-YAPI) düğümlerinde `yapi_kaydet`: alan adları
+ tipleri (yalnız bilinen skaler; generic "T"/yapı/bileşik → "?"). Sorgular:
`yapi_var_mi`, `alan_var_mi`, `alan_tip_bul`.

**C `kontrol_yapi_olustur_ic` sırası birebir (--checkdump doğrulaması):**
- Yapı tanımsız → T002 (oluştur düğümü) + **erken dönüş** (alan kontrolü yok).
- Her alan-atama (oluşturma sırası): bilinmeyen alan → T017 (alan düğümü, **değer
  kontrol edilmez**); bilinen → değer T002 + T001 (alan değer tipi, alan düğümü).
- Eksik alanlar (bildirim sırası) → T012 (oluştur düğümü), per-alan döngüsünden SONRA.
- `ifade_tip` YAPI_OLUSTUR → yapı adı; ERISIM → alan tipi (referans nesne → "?").

**GÜVENLİ strateji:** Alan tipi yalnız bilinen-skaler saklanır (generic/yapı alan →
"?" → T001 atla); referans-nesne erişimi → "?" → atla. Geçerli kodda false-positive
YOK → test/ornekler (yapilar/hasta dâhil) 41/42 KORUNDU.

**Doğrulama:** `make calistir_checker_diff` → **38/38 korpus** (önceki 32 + TC4 6:
yapı-OK / bilinmeyen-alan / eksik-alan / alan-tip / bilinmeyen-yapı / erişim-tip).
test/ornekler **41/42** (regression yok; lineer_hata = TC5).

**Sıradaki (TC4b):** eşleş exhaustiveness M001 + çeşit varyant (M002/M003/M004) +
INDEKS tip/T013. Sonra linear (TC5 → lineer_hata kapanır), bölge/yetki/modül (TC6-8).

---

## D-059 — SELF-HOST checker TC3h: atama lvalue (T022) + atama tip uyumu (T001) — 32/32 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3h: atama
hedefi lvalue olmalı (T022); atama değeri hedef tipiyle uyumlu olmalı (T001). C
derleyici DOKUNULMADI.

**Kapsam:** `kontrol_dugum` ATAMA özel-case'i. `lvalue_mi` (TANIMLAYICI/ERISIM/INDEKS).
C DUGUM_ATAMA sırası birebir: T022 (lvalue, **erken dönüş YOK**) → hedef T002 → değer
T002 → T001 (`ifade_tip(hedef)` vs `ifade_tip(değer, ht)`). Konum: ATAMA düğümü
(sol-taraf başı; `--checkdump` ile doğrulandı: `x=doğru`→T001 3:5, `5=3`→T022 2:5,
`f()=3`→T022 3:5).

**GÜVENLİ strateji:** T001 yalnız hedef tipi bilinen-skaler (TANIMLAYICI → var_tip)
iken; ERISIM/INDEKS hedef → ht "?" → T001 atla (alan/indeks tipi TC4). Değer "?" →
atla. Geçerli kodda (lvalue + uyumlu tip) hata yok → false-positive YOK.

**Doğrulama:** `make calistir_checker_diff` → **32/32 korpus** (önceki 28 + TC3h 4:
atama-OK / atama-T001 / literal-hedef-T022 / çağrı-hedef-T022). test/ornekler **41/42**
(regression yok; lineer_hata = TC5).

**Aşama 2 ilerleme (T-kodları):** T001 (annot/atama/arg/IKILI-aynı-tip), T002, T003,
T004, T010, T020, T021, T022, T024, T026 — 10 kod. Sıradaki (TC4): struct alan T017 +
ERISIM/INDEKS tip çıkarsama + eşleş exhaustiveness M001. Sonra linear (TC5 →
lineer_hata kapanır), bölge/yetki/modül (TC6-8), tam-korpus paritesi (TC9).

---

## D-058 — SELF-HOST checker TC3g: CAGRI per-arg T001 (param tip tablosu) — 28/28 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3g: kullanıcı
işlevi çağrısında argüman tipi parametre tipiyle uyumsuz → T001 (arg konumunda).
C derleyici DOKUNULMADI.

**Mimari karar — parametre tip tablosu (`fn_pbase` + `fn_ptip` düz liste).** İmza
tablosu genişletildi: her işlevin param tipleri `fn_ptip`'e ardışık yazılır, `fn_pbase`
başlangıç indeksini tutar. `fn_ptip_bul(ad, j)` j. param tipini verir. CAGRI arite-OK
yolunda her arg için `ifade_tip(arg, pt)` vs `pt` → uyumsuz ise T001 arg düğümünde.

**EMPİRİK C DAVRANIŞI (--checkdump ile doğrulandı):**
- `f(tanımsız)` → T002 **İKİ KEZ** (C iki-geçiş: pass1 unify + pass2 check, her ikisi
  `tip_belirle` → arg-İÇİ hata çiftlenir). Per-arg **T001 yalnız pass2** → tek emisyon.
- `f(b)` (b yanlış-tip, iç hata yok) → T001 **bir kez** (arg konumu).
- İç-hatasız argümanlarda tek-geçiş = C ile birebir (çiftleme yalnız iç-hatalı argda).

**Tasarım — tek-geçiş + per-arg T001 (GÜVENLİ):** Param tipi yalnız **bilinen skaler**
saklanır (generic "T"/yapı → "?" → atla; generic false-positive yok). Arg "?" veya pt
"?" → atla. Literal arg bidirectional (`byte_al(100)` param tam8 → tam8 → OK). Bilinen
sınır: arg-İÇİ hata çiftlemesi (tanımsız-ad-arg) tek-geçişte tek kez — yalnız geçersiz
kodda; geçerli korpusta (iç-hatasız arg) tam parite. Method/builtin → tek-geçiş (mevcut).

**Doğrulama:** `make calistir_checker_diff` → **28/28 korpus** (önceki 24 + TC3g 4:
arg-OK / arg-T001 / ikinci-arg / literal-bidir). test/ornekler **41/42** (regression yok;
lineer_hata = TC5).

**Sıradaki (TC3h):** T022 (atama lvalue) + eşitlik/karşılaştırma aynı-tip T001. Sonra
struct alan T017 + erişim tipi (TC4), exhaustiveness M001, linear (TC5 → lineer_hata
kapanır), bölge/yetki/modül (TC6-8), tam-korpus paritesi (TC9).

---

## D-057 — SELF-HOST checker TC3f: mantıksal operand (T004) + tekli neg/değil — 24/24 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3f: ikili
`ve`/`veya` operandı mantıksal olmalı (T004); tekli `-` (neg) sayısal (T003); tekli
`değil` mantıksal (T004). C derleyici DOKUNULMADI.

**Kapsam:** `kontrol_dugum` IKILI post-check'e `t004_kontrol` (ve/veya → her iki
operand mantıksal); TEKLI post-check `tekli_kontrol` (neg → sayısal/T003, değil →
mantıksal/T004). `~`/`&`/`deref*` → ileri TC. Konum: IKILI/TEKLI düğümü (= operatör;
parser bootstrap ile C ile özdeş). `bilinen_mantiksal_degil` (≠"?" ve ≠"mantıksal").

**C semantiği birebir (TIP_HATA bastırma):** `ifade_tip` artık ve/veya, neg, değil
için operand kesin-uyumsuzsa "?" döner (C operand→TIP_HATA→ikili/tekli erken dönüş).
Böylece `değişken c: mantıksal = x ve doğru` (x tam32) → yalnız T004 (T001 yok);
`değişken r: tam32 = -b` (b mantıksal) → yalnız T003; `eğer değil x` (x tam32) →
yalnız T004 (T021 yok). `==`/`!=` mantıksal döner (aynı-tip T001 ileri TC).

**GÜVENLİ strateji:** Yalnız KESİN uyumsuz operandda hata → geçerli kodda
false-positive YOK.

**Doğrulama:** `make calistir_checker_diff` → **24/24 korpus** (önceki 20 + TC3f 4:
mantık/tekli-OK / ve-T004 / neg-T003 / değil-T004). test/ornekler **41/42**
(regression yok; lineer_hata = TC5).

**Sıradaki (TC3g):** CAGRI per-arg T001 (param tip tablosu) + eşitlik/karşılaştırma
aynı-tip T001 + T022 (lvalue atama hedefi). Sonra struct alan T017 + exhaustiveness
M001, generic (TC4), linear (TC5 → lineer_hata kapanır), bölge/yetki/modül (TC6-8).

---

## D-056 — SELF-HOST checker TC3e: aritmetik/karşılaştırma sayısal operand (T003) — 20/20 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3e: ikili
aritmetik (`+ - * / %`) ve karşılaştırma (`< > <= >=`) operandı sayısal olmalı (T003).
C derleyici DOKUNULMADI.

**Kapsam:** `kontrol_dugum` IKILI post-check'i `t003_kontrol` — operand tiplerini
`ifade_tip` ile hesaplar; aritmetik VEYA karşılaştırmada bir operand **kesinlikle
sayısal değil** (`bilinen_sayisal_degil`: ≠"?" ve `sayisal_mi` yanlış) ise T003
(IKILI düğüm konumunda = operatör; parser bootstrap ile C ile özdeş). Eşitlik
(`== !=`), mantıksal (`ve veya`), bit/kaydırma → T003 YOK (ileri TC).

**C `tip_belirle(IKILI)` semantiği birebir:**
- Operand TIP_HATA ise (örn. tanımsız ad → T002) ikili **erken TIP_HATA döner →
  T003 YOK**. KEMGU karşılığı: `ifade_tip` arit/karşılaştırmada operand "?" ise
  "?" döner; `t003_kontrol` "?" operandı atlar → çift hata yok.
- T003 fırlayınca C TIP_HATA döner → dış T001/T020/T021 **bastırılır**. KEMGU:
  `ifade_tip` arit-non-sayısal → "?", karşılaştırma-non-sayısal → "?" döner;
  böylece `değişken r: tam32 = b + 1` (b mantıksal) → yalnız T003 (T001 yok),
  `eğer b < 3` → yalnız T003 (T021 yok). İç-içe `(a<3)+1` → yalnız dış '+' T003.

**GÜVENLİ strateji:** Yalnız KESİN bilinen non-sayısal operandda T003 → geçerli
kodda (tüm aritmetik operandlar sayısal) false-positive YOK.

**Doğrulama:** `make calistir_checker_diff` → **20/20 korpus** (önceki 16 + TC3e 4:
aritmetik-OK / aritmetik-T003 / karşılaştırma-T003 / iç-içe). test/ornekler **41/42**
(regression yok; lineer_hata = TC5).

**Sıradaki (TC3f):** CAGRI per-arg T001 (param tip tablosu) + eşitlik/karşılaştırma
aynı-tip T001 + mantıksal T004 + T022 (lvalue) + tekli '-' T003. Sonra struct alan
T017 + exhaustiveness M001, generic (TC4), linear (TC5 → lineer_hata kapanır), modül.

---

## D-055 — SELF-HOST checker TC3d: CAGRI dönüş çıkarsama + T010 arite — 16/16 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3d: kullanıcı
işlevi çağrısının dönüş tipini çıkarsama (T001/T020'yi besler) + çağrı argüman sayısı
uyumsuzluğu (T010). C derleyici DOKUNULMADI.

**Mimari karar — işlev imza tablosu (`fn_ad`/`fn_donus`/`fn_psay` paralel Dizi).**
`genel_topla` sırasında her üst-düzey/modül/`dışa` ISLEV için imza kaydedilir: ad,
dönüş tipi, parametre sayısı. CAGRI `ifade_tip` hedef TANIMLAYICI ise `fn_donus_bul`
ile dönüş tipini verir; `kontrol_dugum` CAGRI özel-case'i `fn_psay_bul` ile arite
karşılaştırır.

**GÜVENLİ strateji (false-positive YOK):**
- Dönüş tipi YALNIZ **bilinen skaler** (`bilinen_skaler_mi`: sayısal/mantıksal/metin/
  karakter/boş) ise saklanır; yapı/generic-param/bileşik dönüş → "?". Böylece generic
  `kimlik<T>() -> T` dönüşü "T" gibi sahte tiplerle T001 üretmez (parser tip-paramları
  AST'te yok → generic tespit edilemez; skaler-whitelist bunu kapsar).
- Arite YALNIZ kullanıcı işlevleri için (`fn_psay_bul >= 0`); builtin'ler → atla
  (builtin arite'leri C'de özel; geçerli kodda doğru çağrılır → diff yok).
- Method (`x.m()` = ERISIM hedef) ve dolaylı çağrı → atla (TANIMLAYICI değil).

**C `tip_belirle(CAGRI)` sırası birebir:** hedef T002 → (tanımsız hedef →
TIP_HATA → **erken dönüş**, arg atlanır) → T010 arite (uyumsuz → **erken dönüş**,
arg tip kontrolü yok) → argümanlar. Pozisyon: T010 CAGRI düğümünde (= `(` konumu;
parser bootstrap 224/224 ile C ile özdeş).

**Doğrulama:** `make calistir_checker_diff` → **16/16 korpus** (önceki 12 + TC3d 4:
çağrı-dönüş OK / T010 arite / dönüş-uyumsuz T001 / ver-çağrı T020). test/ornekler
**41/42** (regression yok; lineer_hata = TC5).

**Sıradaki (TC3e):** CAGRI per-arg T001 (param tip tablosu) + T022 (lvalue) + T003
(sayısal beklenen). Sonra struct alan T017 + exhaustiveness M001, generic (TC4),
linear (TC5 → lineer_hata kapanır), bölge/yetki/modül, tam-korpus paritesi (TC9).

---

## D-051 — SELF-HOST Aşama 2 (TİP DENETLEYİCİ) ADIM-0: --checkdump oracle + mimari (2026-06-14)

**Karar [ETKİ: düşük — additive C `--checkdump` modu; mevcut yol değişmedi].** Aşama 2
(tip denetleyici self-host) başlangıcı. Mandate: Aşama 5'e kadar otonom, faz-sınırında
durmadan, kendi mimari kararlarımla.

**Karar 1 — Oracle: `--checkdump` (accept/reject + tanı paritesi).** C `--check` insan-
okunur (hata[KOD] blokları + özet). Yeni `--checkdump`: hata callback'i (hata.h
`hata_callback_ayarla`) ile tip-kontrol hatalarını toplar, DÜZ basar:
`<KOD>\t<satır>\t<sütün>` (callback/traversal sırasıyla), hata yoksa `OK`. KEMGU-checker
aynı çıktıyı üretecek → diff = accept/reject + kod/konum paritesi. (Parser/yükleme/wcet
hataları da toplanır ama TC korpusu TEMIZ parse eder → yalnız T/L/M kodları.)

**Karar 2 — KEMGU-checker temsili: indeks-düz + STRING-encoded tipler.** Sembol tablosu =
paralel Dizi (ad/kategori/tip-string/scope-seviye), scope = seviye-sayacı (append-only;
toy-demo scope-stack deseni). Tipler STRING-encoded ("tam32", "Dizi<tam32>", "&Nokta") —
nominal eşitlik `metin_esit` (C TipBilgisi struct yerine; KEMGU'da en doğal). Checker
parser'ın düz AST tablosunu (`Ayr`) gezer. selfhost/checker.kem parser'ı İÇERİR (AST
gerek; tek-dosya; modülerleştirme Aşama 4/entegrasyon).

**Plan (TC1-TC9, her biri --checkdump paritesi):** TC1 temel (literal tip + scope +
T002 tanımsız + T001/ifade-tip uyumsuz) · TC2 işlev/çağrı (T-arity/arg) · TC3 struct/
çeşit (alan/exhaustiveness M001) · TC4 generic/mono · TC5 linear (tekkez L001-L008) ·
TC6 bölge · TC7 yetki · TC8 modül · TC9 tam güvenlik + tüm-korpus paritesi.

**Doğrulama:** `--checkdump` OK örnek + T001/T002 hata örneği bayt-exact. Prod 0 uyarı.
Additive — `--check`/testler etkilenmedi.

**Sıradaki:** TC1 — selfhost/checker.kem (parser AST üzerinde sembol/tip/temel kontrol).

---

## D-054 — SELF-HOST checker TC3a: tip çıkarsama temeli + annotation T001 — 9/9 korpus (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC3a: tip
çıkarsama çekirdeği (mountain'ın özü). değişken/sabit annotation-değer uyumsuzluğu (T001).

**Kapsam:** STRING-encoded tip çıkarsama `ifade_tip` — literal (TAM/KESIRLI bidirectional
beklenen sayı/kesirli tiple; METIN/KARAKTER/MANTIKSAL/BOS) + tanımlayıcı (yerel_tip
takibi). `yerel_tip` Dizi (param: tip-çocuğundan; değişken: annotation; için/desen: "?").
T001: değişken/sabit annotation vs değer tipi; çocuk T002'lerinden SONRA (C sırası).

**GÜVENLİ strateji:** Bilinmeyen tip → "?" → T001 ATLA. ifade_tip yalnız emin olduğu
(literal/bilinen-ident) tipleri döndürür; IKILI/CAGRI/ERISIM → "?" (TC3b). Böylece
geçerli kodda FALSE-T001 yok → gerçek tek-dosya 41/42 KORUNDU (under-report > over-report).
bidirectional: `değişken a: tam8 = 5` OK (5→tam8); `b: mantıksal = 7` T001.

**Doğrulama:** `make calistir_checker_diff` → **9/9 korpus** (TC1 4 + TC2 2 + TC3 3).
test/ornekler 41/42 (regression yok; lineer_hata = TC5). C derleyici değişmedi.

**Sıradaki (TC3b):** IKILI operatör tipi (sayısal aritmetik → operand; karşılaştırma/
mantıksal → mantıksal) + CAGRI dönüş tipi + T020 (ver) + T021 (koşul mantıksal) +
T022 (lvalue) + T010 (arite). Sonra struct alan/exhaustiveness, generic, linear, modül.

---

## D-053 — SELF-HOST checker TC2: üst-düzey çift-tanım (T024/T026) — 6/6 korpus, 41/42 gerçek (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/checker.kem` + korpus].** Aşama 2 TC2: üst-düzey
çift-tanım denetimi. C `pre_populate` accept/reject + sıra paritesi.

**Kapsam:** PROGRAM doğrudan çocuklarında çift-tanım. Kod kind'e göre: yapı/çeşit →
**T026**, işlev/sabit/özellik/modül → **T024**. İkinci tanımın konumunda. C pre_populate
**4-geçiş sırası BİREBİR**: özellik → yapı/çeşit → (uygula: ad yok) → işlev/sabit/modül,
her geçiş kaynak-sırası, PAYLAŞILAN global kapsam (gor). dışa-sarmalı açılır.

**Bulgu (sıra kritik):** Naif kaynak-sırası dup-tarama yanlış sıra üretti — C iki-geçişli
(yapı/çeşit önce, işlev/sabit sonra) → T026'lar T024'lerden önce. 4-geçiş replikasyonu
düzeltti. GÜVENLİ kapsam: yalnız top-level (modül-içi/cross-modül → false-T024 riski,
TC8'e ertelendi) — gerçek tek-dosya 41/42 korundu.

**Doğrulama:** `make calistir_checker_diff` → **6/6 korpus** (TC1 4 + TC2 2). test/ornekler
tek-dosya 41/42 (tek fark lineer_hata = TC5). C derleyici değişmedi.

**Sıradaki:** TC3 = tip çıkarsama (T001 uyumsuzluk — 22× en sık; TipBilgisi modeli) +
arite (T010). Bu "DAĞ"ın çekirdeği. Sonra struct alan/exhaustiveness, generic, linear, modül.

---

## D-052 — SELF-HOST checker TC1: temel ad çözümü (T002) — 41/42 gerçek tek-dosya (2026-06-14)

**Karar [ETKİ: düşük — `selfhost/checker.kem` (parser kopyası + checker) + korpus].**
Aşama 2 TC1: KEMGU'da temel tip denetleyici — kapsam/ad çözümü (T002 tanımsız sembol).
C `tip_kontrol.c` accept/reject + tanı paritesi (`--checkdump` oracle).

**Kapsam:** `selfhost/checker.kem` = parser (kopya, AST için) + sembol kümeleri
(`g_isim` global: 47 EKLE_BUILTIN + özel-builtin'ler [vektor_*/mmio_*/yetki_olustur/
tekkez_olustur/delege/geri_al/görev/kanal/dur/dondur] + üst-düzey tanım adları +
keyword-konstrüktör değer/tamam/hata/kendin/hiç; `yerel` append-only dilim: param+
lokal+için+desen-binding). Traversal: işlev gövdesi + sabit değeri; TANIMLAYICI ref
genel∪yerel'de değilse → T002. Tip/desen/yol alt-ağaçları atlanır (ileri TC).

**Doğrulama:** `make calistir_checker_diff` → 4/4 korpus + **41/42 test/ornekler tek-dosya
--checkdump sıfır-diff**. Tek fark `lineer_hata.kem` (kasıtlı L001/L002 → TC5; TC1
linear yapmaz). Bulgu: keyword-konstrüktör (değer/tamam/hata) + özel-builtin'ler
(mmio/yetki/vektor) genel'e eklenmezse false-T002.

**Sınır/sıradaki:** Cross-modül import (kullan) adları henüz çözülmez (→TC8). Tip
uyumsuzluğu (T001), arite (T010), struct alan, exhaustiveness → TC2-TC3+. checker.kem
parser kopyası içerir (modülerleştirme Aşama 4).

---

## D-050 — 🎉 SELF-HOST parser P6: BOOTSTRAP — 223/223 GERÇEK .kem + SELF-PARSE (Aşama 1 TAMAM) (2026-06-14)

**Karar [ETKİ: düşük — `selfhost/parser.kem` + additive `ondalik_bicimle` intrinsic +
harness].** Aşama 1 (PARSER self-host) KAPANIŞI. KEMGU'da yazılı parser, C parser'ın
`--ast` oracle'ına karşı TÜM gerçek korpusta sıfır-diff — **kendi kaynağı dahil**.

**Son iki kapatma:** (1) **KESIRLI float:** `ondalik_bicimle(metin)->metin` intrinsic
(runtime strtod + `%g`, C ast_duz_yaz birebir) — `yaz_karakter` gibi float-format
runtime primitifi. `metin_`/`dosya_` dışı → açık dispatch (tip_kontrol+llvm+runtime).
(2) **satıriçi_asm:** deyim parse (mimari/şablon/çevrim/çıktı/girdi/bozulan clause);
yalnız `girdi` ifadeleri AST çocuğu (C ast_duz_yaz), gerisi tüketilir.

**Doğrulama:** `make calistir_parser_bootstrap` → **223/223 SIFIR-DİFF** (build/lex_korpus/
ornekler-eski hariç TÜM .kem) — **selfhost/parser.kem SELF-PARSE dahil**. test_llvm
235/235 + lexer bootstrap 261/261 (ondalik_bicimle regresyonsuz). `make
calistir_parser_diff` 12/12 korpus.

**eski/ hariç:** `test/ornekler/eski/tip_alias.kem` `tip Ad = T;` kullanır; `tip`
v1'de anahtar kelime DEĞİL → C parser DA P001 hata verir (geçersiz). Hata-kurtarma
diverjansı (gerçek boşluk değil) → bootstrap'tan çıkarıldı.

**Aşama 1 ÖZET (D-035→D-050):** ADIM-0 (--ast oracle + index-AST kararı) → P1 ifade →
P2 deyim → P3 bildirim → P4 tip → P5 modül/import → P6 bootstrap. İndeks-tabanlı düz
AST tablosu, &değişken struct threading (D-044), düz preorder dumper. Üç additive
intrinsic: yaz_bayt, ondalik_bicimle (+ D-041/D-044 codegen fix'leri parser'ı sağladı).

**Sıradaki (Aşama 2 — TİP DENETLEYİCİ):** DAĞ. C checker accept/reject paritesi.
TC1 temel → TC9 tam güvenlik. Mimari: KEMGU'da sembol tablosu + scope + tip temsili.

---

## D-049 — SELF-HOST parser P5: modül/kullan/dışa/genel + geri_al/delege + TAM-clamp — 115/118 GERÇEK .kem (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/parser.kem` + korpus].** Aşama 1 P5: üst-düzey
modül/kullan/dışa/genel + iki ifade-builtin'i + tamsayı taşma davranışı. parser.c
parse_kullan/parse_disa/parse_genel/parse_modul + parse_birincil DELEGE/GERI_AL birebir.

**Kapsam:** `kullan m::seg::{a,b} olarak d;` (KULLAN deger=`::`-yol; seçili/alias
dump'ta yok). `dışa <tanım>` (DISA sarmalar). `genel <tanım>` (SARMALAMAZ — iç tanımı
döner; genel_mi dump'ta yok). `modül Ad { üyeler }` (recursive parse_ust_oge). İfade:
`delege(...)`/`geri_al(...)` → DUGUM_CAGRI (hedef TANIMLAYICI). **TAM taşma:**
`tamsayi_deger` artık strtoll gibi int64-max'a CLAMP eder (`0xFFFF...FFFF` → LLONG_MAX,
önce wrap → -1).

**Doğrulama:** `make calistir_parser_diff` → 11/11 korpus + **115/118 GERÇEK .kem**
(ornekler/drivers/stdlib/moduller/crossfile). Kalan 3: KESIRLI float (drone_kontrol,
matris_carpim) + tip_alias (ayrı). C derleyici değişmedi.

**Sınır:** KESIRLI float (%g dump) → sıradaki (runtime float-format intrinsic gerek).
satıriçi_asm deyimi henüz yok (korpusta nadirse sonra).

---

## D-048 — SELF-HOST parser P3: bildirimler — 69/113 GERÇEK .kem --ast sıfır-diff (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/parser.kem` + korpus].** Aşama 1 P3: tüm
üst-düzey bildirimler. parser.c parse_islev_genel/parse_yapi/parse_cesit/
parse_ozellik/parse_uygula/parse_sabit/parse_parametre/parse_ust_oge ile birebir.

**Kapsam:** işlev (gerçekzamanlı? + generic `<T: Bound>` + param + dönüş + gövde;
imza_yeterli özellik için), yapı (+generic + alan), çeşit (varyant + C3 payload;
generic v1-YOK skip), özellik (imza/default), uygula (trait `için` + inherent),
sabit, parametre (`kendin`/`&kendin`/normal). Generic params + bound'lar PARSE+
DISCARD (dump'ta yok; bound düğümleri orphan). atla_tip_paramlar `>>` böl.

**İki kök-fix:** (1) **PROGRAM pozisyonu** = ilk token (C); önce 1:1 hardcode →
yorumla başlayan HER dosya farklıydı (0→69 sıfır-diff sıçraması). (2) **Anti-hang:**
çeşit generic + varyant-loop non-identifier'da ilerlemiyordu → sonsuz döngü; C
panik_sync deseni eklendi.

**Doğrulama:** `make calistir_parser_diff` → **11/11 korpus** + **69/113 GERÇEK .kem**
(ornekler/drivers/stdlib/moduller) tam --ast sıfır-diff. Kalan 44: P5 (modül/kullan/
dışa/genel/satıriçi_asm) + KESIRLI float. C derleyici değişmedi.

**Sınır:** Üst-düzey modül/kullan/dışa/genel/satıriçi_asm → P5; KESIRLI float (%g) → ayrı.

---

## D-046 — SELF-HOST parser P2: deyimler + kontrol akışı + desenler — 9/9 sıfır-diff (2026-06-14)

**Karar [ETKİ: düşük — yalnız `selfhost/parser.kem` + korpus; C tarafı 0 değişiklik].**
Aşama 1 P2: tüm deyimler + kontrol akışı + eşleş desenleri. parser.c parse_deyim/
parse_eger/parse_iken/parse_icin/parse_esles/parse_desen/parse_guvensiz ile birebir.

**Kapsam:** değişken (`: tip` ops. + `= ifade`), atama (lvalue `=`), ver (0/1
çocuk), ifade-deyimi, eğer/değilse/değilse-eğer (else-if zinciri = recursive),
iken, için (`ad: koleksiyon`), eşleş + kollar, güvensiz (±`[etiket: "..."]`).
Desenler: joker `_`, tanımlayıcı, yapıcı `Ad(...)`, çeşit-yol `Çeşit::Varyant[(payload)]`,
literal. Kol gövdesi: ifade `;` veya `{ blok }`.

**`yapi_izni` bayrağı (parser.c yapi_olusturma_izni birebir):** Koşul bağlamında
(eğer/iken/için/eşleş değeri) `Tip { }` yapı-oluşturma KAPALI → `{` blok başı sayılır.
`Ayr.yapi_izni` (1 default; parse_kosul 0/restore). Düğüm pozisyonları C ile birebir
(deyim=keyword; atama/ifade-deyimi=ifade başı; eşleş-kolu=desen başı).

**Doğrulama:** `make calistir_parser_diff` → **9/9 SIFIR-DİFF** (6 P1 + 3 P2:
değişken-atama, kontrol-akışı, eşleş-güvensiz). C derleyici değişmedi → regresyon yok.
`--check` temiz.

**Sınır:** Param/generic/bildirim → P3; tam tip sözdizimi (annot Dizi/seçimlik/...)
→ P4; KESIRLI float → ayrı. P2 sarmalayıcı yine `işlev f() -> T { deyimler }` (param yok).

---

## D-045 — SELF-HOST parser P1: Pratt ifade parser — 6/6 korpus --ast sıfır-diff (2026-06-14)

**Karar [ETKİ: düşük — `selfhost/parser.kem` + korpus; C tarafı yalnız additive
`yaz_bayt` intrinsic].** Aşama 1 P1: KEMGU'da tam Pratt ifade parser. C `--ast`
oracle'ına karşı sıfır-diff.

**P1a — token-tablo temeli:** Lexer scanning REUSE (emit sink'i print→`dizi_ekle`),
token tablosu `Ayr` struct'ta (paralel Dizi). 250/250 gerçek .kem'de re-emit
`--token` sıfır-diff (foundation kanıtı). State threading: `&değişken Ayr`
(D-044'e dayanır; scalar+Dizi alan mutasyonu + ref-param passthrough de-risk edildi).

**P1b — Pratt ifade:** ifade.c ile birebir öncelik (VEYA=1…CARPMA=10, ÖNEK=11,
SONEK=12). Birincil (TAM/TANIMLAYICI/MANTIKSAL/BOS/METIN/KARAKTER + paren), önek
(neg/değil/~/&/&değişken/deref*), sonek zinciri (.alan/[i]/(args)/::yol/olarak),
yapı/dizi/lambda oluşturma, kullan/imha. Düğüm pozisyonları C ile birebir
(ikili/tekli=operatör tokenı; sonek=sonek tokenı; literal=kendi tokenı). Sayı
değeri (`_` temizle + 0x/0b/0o taban → int64 → ondalık string) ifade.c parse ile
aynı. AST = düz düğüm tablosu (append-on-create flat çocuk listesi). Düz dumper
preorder, `\t\n\r\\` kaçışlı (`yaz_kacis`).

**Yeni intrinsic `yaz_bayt(tam32)` (additive):** `yaz_karakter` argümanını
codepoint sayıp UTF-8 ENCODE eder → METIN değer dump'ında Türkçe bayt mojibake
(`ç`→`Ã§`). `yaz_bayt` HAM bayt yazar (putchar & 0xFF). 3 yer: tip_kontrol.c
builtin registry, llvm.c dispatch+declare, runtime. Lexer ASCII-only olduğu için
bunu hiç tetiklemedi; parser ham UTF-8 yazar → gerekli.

**Doğrulama:** `make calistir_parser_diff` → **6/6 SIFIR-DİFF** (aritmetik/mantık-bit/
önek-sonek/literal/bileşik/metin — Türkçe METIN + KARAKTER U+XXXX dahil). test_tumu
29 suite + ASan YEŞİL (yaz_bayt regresyonsuz). lexer bootstrap 256/256. `--check` temiz.

**Sınır (P1 dışı, sonraki adımlar):** KESIRLI float (%g formatı — ayrı adım); diğer
deyimler (P2); param/generic/bildirim (P3); tam tip sözdizimi (P4). P1 sarmalayıcı:
`işlev f() -> T { ver İFADE; }` (param yok, tek `ver` deyimi).

---

## D-044 [YÜKSEK] — Kök-neden fix: yapı Dizi<T> alanı boş/[...] literal → STACK [0xi8] → SEGFAULT (2026-06-13)

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit].** Parser
self-host P1 de-risk'inde ortaya çıktı (virtio track'teki "&Struct-param+Dizi"
segfault'unun kök-nedeni). `Yapı { d: [] }` veya `{ d: [e,...] }` — alan tipi
`Dizi<T>` iken — `DUGUM_DIZI_OLUSTUR` codegen'i **her zaman STACK `[N x i8]`**
üretiyordu (boş `[]` → `alloca [0 x i8]`). Alan KdlDizi* yerine 0-byte STACK
buffer'a işaret eder; `dizi_ekle(t.d, ..)` onu KdlDizi* sanıp `.veri/.boyut/...`
erişince **SEGFAULT**. `--check` geçiyordu (tip sistemi `[]`'i geçerli Dizi<T>
sayar) → latent codegen miscompile.

**Neden gizliydi:** `değişken d: Dizi<T> = []` ZATEN çalışıyordu — ama AYRI bir
özel-yol (DUGUM_DEGISKEN handler'ı, llvm.c:3341) heap'e dönüştürüyordu. Diğer TÜM
bağlamlar (yapı alanı, çağrı argümanı, `ver`) bu yoldan geçmiyordu → stack.

**Fix (kök, genel):** (A) `DUGUM_DIZI_OLUSTUR` artık `g->beklenen_tip` `Dizi<T>`
ise HEAP `kdl_dizi_olustur` + eleman başına `dizi_ekle` üretir (eleman_byte
eleman IR tipinden; iç içe için beklenen_tip elemana inilir). (B) `yapi_olustur_uret`
alan değerini değerlendirmeden ÖNCE `g->beklenen_tip = alan_tip_d` koyar. Böylece
TÜM Dizi<T> bağlamları (sadece değişken değil) doğru heap üretir.

**Doğrulama:** De-risk (`yapı Tablo{adlar:Dizi<metin>; sayilar:Dizi<tam32>}` +
`&değişken Tablo` param field-append) → exit 42 (segfault yok). test_llvm yeni
[161] regresyon. Tüm test paketi + ASan + lexer bootstrap YEŞİL (aşağıda).

**Kapsam/sınır:** Yalnız `[]`/`[...]` literalin heap-Dizi yönlendirmesi eklendi —
stack-dizi yolu (annot yok / index'lenen sabit dizi) korunur. `t.d[i]` index
sintaksı struct-field heap-dizi için ayrı (builtin `dizi_al/ekle/boyut` çalışır;
INDEX düğümü gerekirse sonra).

---

## D-043 — SELF-HOST parser ADIM-0: AST temsili + --ast diff-oracle (2026-06-13)

**Karar [ETKİ: düşük — additive C: yeni `--ast` modu + `ast_duz_yaz`; mevcut yol
değişmedi].** Aşama 1 (parser self-host) ADIM-0. Mandate: tasarım kararları ajan
verir + loglar + devam eder (sormaz). İki çekirdek karar:

**Karar 1 — KEMGU AST temsili: İNDEKS-TABANLI DÜZ DÜĞÜM TABLOSU** (paralel `Dizi`'ler).
Recursive `çeşit` (C3 payload) bir alternatifti ama kendine-referanslı tip
heap-boxing gerektirir (KEMGU'da belirsiz). Toy demolar (D-033/D-034) indeks-arena'yı
KANITLADI: düğüm = indeks; paralel diziler `dugum_tip[]`/`satir[]`/`sutun[]`/`deger[]`
+ düz çocuk-listesi (`cocuk[]` + `cocuk_basla[]`/`cocuk_sayi[]`). Recursive-descent
doğal: alt-ifadeleri parse et (indeks al) → ebeveyn düğümü o indekslerle oluştur.
Dump = preorder traversal (D-041 sonrası KEMGU çağrı-özyinelemesi stack-güvenli;
AST derinliği ~onlarca).

**Karar 2 — --ast diff-oracle formatı: DÜZ derinlik-etiketli preorder.**
`<derinlik>\t<TIP_ADI>\t<deger>\t<satır>\t<sütün>` (lexer dersi: düz > iç-içe-girinti).
Derinlik-etiketli preorder AĞACI BİREBİR belirler (benzersiz). `<deger>` = skaler yük
(ad/literal/operatör), `\t \n \r \\` kaçışlı (alan-ayracı güvenliği). Mevcut
`ast_yazdir` (--parse, insan-okunur) EKSİK — ~30 düğüm tipi `default`'a düşüp
çocuklarını gezmiyor. Yeni `ast_duz_yaz` (`--ast`) TÜM 67 düğüm tipini + çocuklarını
KANONİK sırada gezer (oracle tamlığı). KEMGU-parser aynı çıktıyı üretecek → diff = doğruluk.

**Doğrulama:** Prod 0 uyarı. `--ast` 249/249 gerçek .kem'de deterministik + boş-değil.
Öncelik doğru (`x + 1 * 2` → `x + (1*2)`). Mevcut `--parse`/test'ler etkilenmedi (additive).

**Plan (P1-P6, her biri --ast sıfır-diff kapılı):**
- **P1 ifadeler** — Pratt öncelik (veya<ve<==<karşılaştırma<+−<*/%<önek<sonek),
  önek (−/değil/~/&/&değişken/*), sonek zinciri (.alan [i] (arg) ::yol), yapı/dizi/
  lambda oluşturma, `olarak` cast, kullan/imha ifade. (+minimal işlev/blok/ver sarmalayıcı.)
- **P2 deyim/kontrol** — değişken/atama/ver/eğer-değilse/iken/için/eşleş+desen/güvensiz/blok.
- **P3 bildirim** — işlev (generic+bound), yapı, çeşit (payload), özellik, uygula, sabit, alan, parametre.
- **P4 tip-sözdizimi** — &T/&değişken T/*T, Dizi/seçimlik/sonuç/tekkez/sabitsüre/yetki/
  vektör/görev/kanal, işlev(...)→T, Kullanıcı<...>, `>>` generic-böl.
- **P5 modül/import** — modül, kullan (namespaced/seçili/alias), dışa, genel.
- **P6 tüm-korpus** — KEMGU-parser tüm .kem + KENDİ kaynağı (self-parsing) → --ast sıfır-diff.

**Ön-koşul/sınır:** Sayı literal→değer dönüşümü (TAM int64, `_`/hex/bin/oct) KEMGU'da
C parser ile birebir gerekecek (P1). KESIRLI değer formatı (`%g`) fragility riski →
gerekirse P1/P4'te lexeme-tabanlıya geçilir (karar o noktada). Generic-param/çeşit-varyant
ad string-metadata --ast'a P3/P4'te eklenir (şimdilik yapısal ağaç).

---

## D-042 — SELF-HOST lexer M6: BOOTSTRAP kapanışı — 249/249 gerçek .kem sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yeni harness + Makefile hedefi].** M6 = self-host lexer'ın
asıl ispatı: KEMGU-lexer (selfhost/lexer.kem) TÜM gerçek KEMGU korpusunu C lexer
(oracle) ile **sıfır-diff** lex'ler.

**Kapsam:** `test/lexer_bootstrap_harness.sh` — KEMGU-lexer'ı derler, `build/`
(üretilmiş temp) hariç tüm `.kem` dosyalarını (`stdlib/`, `drivers/`, `kütüphane/`,
`test/**`, **`selfhost/lexer.kem`'in KENDİSİ** = self-lexing) C `--token` dump'ına
karşı diff'ler. `make calistir_lexer_bootstrap`.

**Sonuç:** **249/249 SIFIR-DİFF** — KEMGU-lexer C lexer'ı gerçek dünya KEMGU
kodunda TAM İKAME EDER. Self-lexing dahil (4566 token, kendi kaynağı). M1-M5
korpus 22/22 regresyon kalır.

**Engel + çözüm:** İlk koşuda 3 büyük dosya (lexer.kem dahil) crash etti (exit 127,
~binlerce iterasyon sonra) → kök-neden D-041 codegen alloca bug'ı. Düzeltildi.

**Bootstrap durumu:** Lexer parite TAM. Sıradaki gerçek-entegrasyon adımı
(parser'ın KEMGU-lexer çıktısını tüketmesi / C lexer'ın emekliye ayrılması)
mimari karar gerektirir (Token API köprüsü) → DUR-SOR (Mehmet). M6 = token-parite
+ self-lexing ispatı tamamlandı.

---

## D-041 [YÜKSEK] — Kök-neden fix: dongu govde alloca'sı → stack overflow (entry hoist + renumber) (2026-06-13)

**Karar [ETKİ: YÜKSEK — `src/llvm.c` çekirdek codegen; izole commit].** Döngü
gövdesindeki `değişken` (ve koşul/ifade temp'leri) BLOK-İÇİ `alloca` üretiyordu.
LLVM yalnız **entry-blok** alloca'sını fonksiyon girişinde BİR KEZ tahsis eder;
başka blok'taki alloca her ÇALIŞMADA stack ayırır → uzun döngüde **STACK
OVERFLOW**. Latent bug — toy programlar az iterasyonla tetiklemedi; **self-host
lexer'ın binlerce-iterasyonlu ana döngüsü açtı** (exit 127, ~3775 tokende crash,
dosyaya göre farklı nokta = döngü-başı alloca kanıtı).

**Fix:** `islev_uret` gövdeyi `tmpfile()` buffer'a yazar; `hoist_renumber` tüm
`%N = alloca` satırlarını entry blok başına taşır. Taşıma SSA ardışık-numara
kuralını bozduğundan (`clang`: "instruction expected to be numbered") TÜM numaralı
değerler (`%<rakam>`; `%bb<ad>`/`%<ad>` hariç) yeniden numaralanır. **Güvenli
çünkü:** tüm alloca'lar statik-boyut (operandsız) → erken taşıma ileri-referans
yaratmaz; codegen **phi kullanmaz** (alloca/load-store) → tek-geçiş renumber yeterli;
koşullu alloca'yı her zaman tahsis etmek semantik olarak zararsız (kullanılmayan
stack).

**Doğrulama:** Tüm test paketi YEŞİL — test_llvm **234/234** (yeni [160]:
500000-iter döngü-yerel alloca, crash yok → 42), birim testleri (57/39/35/40/23/
30/5/50/9/6/16/36/13/6/21 hepsi 0 başarısız), ASan matris 20000 iter/0 crash,
stdlib --check temiz. Self-host lexer artık kendi kaynağını crash'sız lex'ler.

**Kapsam/sınır:** Yalnız alloca yerleşimi değişti — ABI/imza/struct-layout/semantik
DEĞİŞMEDİ (mem2reg/SROA zaten hoist ederdi; fix sadece text-IR'ı geçerli kılar).
Tüm fonksiyonlar tek yoldan (`islev_uret`) emit → fix global.

---

## D-040 — SELF-HOST lexer M5: trivia (yorum) + ham string — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus].** M5: `bosluk_atla`'ya
`//` satır + `/* */` İÇ İÇE blok yorum (derinlik sayacı); ham string `r#"..."#`
(`ham_basi_mi` + `ham_emit`, hash eşleme). C bosluk_atla/ham_metin_oku birebir.

**Kapsam:** `//` → `\n`'e kadar (tüketmeden). `/*` → derinlik sayacı, iç içe
(`/* /* */ */` doğru). Ham string: açılış N hash = kapanış N hash; `r"..."` (0 hash)
özel; iç tırnak literal. L011 (geçersiz baş), L002 (kapanmamış). **Çok-satırlı
trivia/ham string → satir/sutun re-scan** (yorum: bosluk_atla içinde inline; ham
string: hlen span'i üzerinden re-scan, `\n`→satir++).

**Kasıtlı NON-hata parite:** kapanmamış blok-yorum SESSİZ EOF'ta biter (HATALI YOK —
C ile aynı). `//`/`/*` string/ham-string İÇİNDE yorum değil (literal tarama önce).

**Doğrulama:** `make calistir_lexer_diff` → **22/22 SIFIR-DİFF** (18 M1-M4 + 4 M5).
Spot: `a /* /* iç */ dış */ b` → `b` 1:25 (iç içe tüketildi); `x = r"çok⏎satır"⏎y`
→ `y` satır 4 (çok-satırlı ham string satir izleme bayt-exact). `--check` temiz.

**Sıradaki (M6):** bootstrap kapanışı — KEMGU-lexer'ı (a) KENDİ kaynağına +
(b) tüm gerçek `.kem` korpusuna karşı sıfır-diff doğrula (self-lexing ispatı).

---

## D-039 — SELF-HOST lexer M4: literaller (sayı/float/metin/karakter) — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus].** M4: tam literal
desteği — `sayi_emit` (ondalık + 0x/0b/0o + float kesir+üs), `dize_emit`
(`"..."`), `karakter_emit` (`'a'`/UTF-8). C lexer sayi_oku/metin_oku/karakter_oku
ile span-exact (kaynak L106-268 birebir port).

**Kapsam:** Sayı — 4 taban (0x/0b/0o erken-return TAMSAYI; boş gövde `0x` hatasız),
float kesir (`.` guard `sonraki≠.` → `1.5` vs `1..5`; trailing-dot `7.`), üs `e/E±`.
Metin — escape DECODE EDİLMEZ (`\`+1 bayt atlanır, C gibi); newline/EOF → HATALI
(L001). Karakter — escape (`\`+1) veya UTF-8 tek-karakter (utf8_uz); boş `''`→L009,
çok/kapanmamış→L010. Hepsi tüketilen bayt döner.

**KÖK-NEDEN bulgu [self-host isim kısıtı]:** Codegen `metin_*` ön-ekini runtime
intrinsic'e yönlendiriyor (llvm.c:2961 `memcmp(cagri_adi,"metin_",6)`) → `kdl_metin_*`
(ptr dönüş). `işlev metin_emit` bu yüzden `kdl_metin_emit` sayıldı → IR tip hatası
(`store i32 ptr`). **Çözüm:** `metin_emit`→`dize_emit`. (Pure-prefix dispatch'ler
yalnız `metin_`/`dosya_`; `karakter_`/`sayi_`/`dizi_` exact-match → güvenli.)
Kaynak değiştirilmedi — isim kuralıyla çözüldü.

**Doğrulama:** `make calistir_lexer_diff` → **18/18 SIFIR-DİFF** (13 M1-M3 + 5 M4).
Spot: `1..5 1.5 7. 0x 1e10 "tam"` → bayt-exact (`7.`=ONDALIK trailing-dot,
`0x`=TAMSAYI boş-hex, `1..5`=TAMSAYI+ARALIK+TAMSAYI). `--check` temiz.

**Sıradaki (M5):** trivia — `//` satır + `/* */` İÇ İÇE yorum (derinlik sayacı) +
ham string `r#"..."#` (hash eşleme, L002/L011). Kasıtlı NON-hata parite (kapanmamış
blok-yorum sessiz, geçersiz UTF-8→bayt-bayt HATALI).

---

## D-038 — SELF-HOST lexer M3: operatörler + noktalama (maximal munch) — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus].** M2'nin tek-karakter
`tek_kar_tip`'i, tüm çok-karakter operatörleri MAXIMAL MUNCH ile çözen `op_emit`
ile değiştirildi (C lexer switch 318-375 ile birebir).

**Kapsam:** Çatışma zincirleri — `.`/`..`/`...`, `:`/`::`, `<`/`<=`/`<<`,
`>`/`>=`/`>>`, `=`/`==`/`=>`, `-`/`-=`/`->`, `+`/`+=`, `*`/`*=`, `/`/`/=`,
`%`/`%=`, `!`/`!=`. Bit ops `& | ^ ~` KOŞULSUZ (`&&` yok; `&değişken` lexer'da
BİRLEŞMEZ). `>>` daima tek `SAGA_KAYDIR` (generic'i parser böler). Tek `!`→HATALI.
Kalan ayraç `[ ] :`. `op_emit` tüketilen bayt sayısını döner (sütün/pos ilerletme).
`ikinci_bayt` sınır-güvenli lookahead (OOB→0).

**Doğrulama:** `make calistir_lexer_diff` → **13/13 SIFIR-DİFF** (9 M1/M2 + 4 M3).
Adversaryel munch spot-check: `a>>b ....x !c ===z` → C oracle ile bayt-exact
(`>>`=tek SAGA_KAYDIR, `....`=UC_NOKTA+NOKTA, `!c`=HATALI+TANIMLAYICI,
`===`=ESIT_ESIT+ESIT). Not: C stderr L005 mesajı token dump'ında değil → diff'i
etkilemez; HATALI tokenı eşleşir. `--check` temiz.

**Sınır (kasıtlı):** `//` `/*` trivia M5'te (M3 yalnız `/` `/=`). `digit.` (float)
M4'e ait — M3 korpusu `digit.` içermez (yalnız `..`/`...` aralık güvenli).

**Sıradaki (M4):** literaller — hex/bin/oct tamsayı, float (kesir+üs), karakter/
metin (escape ham bırakma). Korpus literal-ağırlıklı genişler.

---

## D-037 — SELF-HOST lexer M2: UTF-8 + 44 Türkçe anahtar kelime — sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yalnız `selfhost/lexer.kem` + korpus genişler; C tarafı 0
değişiklik].** M1 ASCII çekirdeği M2'de UTF-8 Türkçe'ye genişletildi. Tam ikame
(bootstrap) yolunda ikinci milestone.

**Kapsam (M2):**
- **UTF-8 identifier:** ASCII `[A-Za-z_]` + 2-bayt Türkçe harf. `turkce_2byte_mi`
  fonksiyonu `utf8.c turkce_harf_2byte` ile BİREBİR: 0xC3(195)→ç Ç ö Ö ü Ü,
  0xC4(196)→ğ Ğ ı İ, 0xC5(197)→ş Ş (2.bayt değerleri tek tek eşleşir).
- **44 anahtar kelime:** M1'in 15 ASCII'sine geri_al/uygula/kanal + 27 Türkçe
  (`anahtar_tip` tam-eşleşme zinciri; `metin_esit` UTF-8 lexeme byte-byte).
- **Bayt-tabanlı sütün:** Türkçe karakter sütünü +2 ilerletir (C `ilerle` deseni —
  her bayt 1 sütün). Doğrulandı: `değişken`=10 bayt → uzunluk 10, sonraki token
  sütün 12; `çörek_adedi`=13 bayt.
- **Değişken-bayt identifier taraması:** `kimlik_basi_uz`/`kimlik_devam_uz` her
  karakterin bayt-uzunluğunu döner (1 ASCII | 2 Türkçe | 0 yok), tarama buna göre
  ilerler.

**`metin_bayt` İŞARETLİ-bayt çözümü (ADIM-0 ön-koşulu):** `metin_bayt` `tam8`
(işaretli) döner → Türkçe 0xC3 = -61. Dağınık işaretli sabitler yerine tek
`bayt(s,i)` helper'ı UNSIGNED (0-255) döndürür (`eğer b<0 { ver b+256 }`). Tüm
bayt karşılaştırmaları 195/167/... gibi doğal unsigned değerlerle. Temiz + UTF-8
dayanıklı.

**Doğrulama:** `make calistir_lexer_diff` → **9/9 SIFIR-DİFF** (5 M1 + 4 M2 korpus:
44 keyword · Türkçe identifier ç/ğ/ı/ö/ş/ü · karışık · kelime-sınır). Kelime-sınır
adversaryel: `değişken`→DEGISKEN ama `değişkenler`/`değişken2`→TANIMLAYICI (yalnız
tam eşleşme). `--check` temiz. C tarafı değişmedi → prod/test_lexer etkilenmedi.

**Sıradaki (M3):** çok-karakter operatörler (`==`, `!=`, `<=`, `>=`, `->`, `::`,
`&&` yok→`ve`, `&değişken`, `>>` generic-böl) — maksimal-munch. Korpus operatör
ağırlıklı genişler; M1/M2 regresyon kalır. (Literal varyant → M4, yorum/raw → M5.)

---

## D-036 — SELF-HOST lexer M1: ASCII çekirdek iskelet — C lexer'a karşı sıfır-diff (2026-06-13)

**Karar [ETKİ: düşük — yeni `selfhost/lexer.kem` + korpus + harness; C tarafı yalnız
`--token` dump formatı]:** Gerçek KEMGU lexer'ını KEMGU'da yazma fazının M1'i
(ADIM-0/D-035 planı). Hedef: tam ikame (bootstrap) — Mehmet onayı. M1 = ASCII
çekirdek, C lexer'a karşı SIFIR-DİFF.

**M1 kapsamı (`selfhost/lexer.kem`):** ASCII identifier + **15 ASCII anahtar kelime**
(iken/ve/veya/ver/delege/hata/imha/kendin/kullan/olarak/sabit/tamam/tekkez/yetki/
genel) + ondalık tamsayı (`_` ayraç) + tek-karakter op (`+ - * / % =`) + ayraçlar
(`( ) { } , ;`) + DOSYA_SONU. Satır/sütün/offset C `ilerle` desenine BİREBİR (\n →
satır++/sütün=1; diğer → sütün++; bayt-tabanlı). (Türkçe keyword/UTF-8 → M2;
çok-karakter op → M3; sayı varyant/literal → M4; yorum/raw → M5.)

**Diff-oracle formatı (D-035 — Mehmet "C'nin daha iyisi" istedi):** C `--token`
(ana.c) ESKİ `%-20s "%.*s"\t\t%d:%d` (ham lexeme gömülü → string-literal'de kırılır,
padding+çift-tab parse-zor) YERİNE: `<TIP>\t<satır>\t<sütün>\t<offset>\t<uzunluk>`.
Ham lexeme YOK (offset+uzunluk'tan kurtarılır) → kaçış-kopyalama riski SIFIR + tek-tab
makine-parse-edilebilir. KEMGU-lexer birebir aynı satırı üretir → `diff` = otomatik
doğruluk. Hiçbir test `--token`'a bağlı değil (test_lexer API-tabanlı, etkilenmez).

**Teknik notlar:** `arg_al(1)`+`dosya_oku` ile dosya okuma (DOĞRULANDI: çalışır).
`yaz_metin` builtin DEĞİL → string bayt-bayt `yaz_karakter` ile (`yaz_str`).
`yaz_karakter` `karakter` ister → `yb(c)` = `c olarak karakter` cast helper'ı.
metin_bayt işaretli ama M1 ASCII (<128) → sorun yok (Türkçe işaretlilik M2'de).

**Doğrulama:** `make calistir_lexer_diff` (`test/lexer_diff_harness.sh`) — 5 ASCII
korpus (`test/lex_korpus/m1_*.kem`: aritmetik, 15 keyword, sayı-ayraç, yapı-punct,
identifier-kenar) → **5/5 SIFIR-DİFF**. test_lexer 103/103 (format değişikliği API'yi
bozmadı). Prod 0 uyarı. selfhost/lexer.kem --check temiz.

**Sıradaki (M2):** UTF-8 identifier (byte-byte 0xC3/C4/C5 + ikinci-bayt; metin_bayt
İŞARETLİ → signed-karşılaştır) + 28 Türkçe anahtar kelime + bayt-tabanlı sütün
doğrulama. Korpus Türkçe keyword'lerle genişler; M1 korpusları regresyon kalır.

---

## D-034 — Self-hosting: mini dil V3 — FONKSİYONLAR (tanım+çağrı+özyineleme) → Turing-tam (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-033 (kontrol akışı)
üstüne fonksiyon soyutlaması — `islev ad(p1,p2){ … don ifade; }` + `ad(arg)` çağrısı
+ özyineleme + karşılıklı çağrı (`test/ornekler/18_fonksiyon_dili.kem`). Toy-dil
artık **Turing-tam**. **Bounded:** fonksiyonlar — closures (yakalama) ERTELENDİ
(DUR-SOR sınırı; ayrı çetin tasarım).

**Tasarım (flat-token yürütücü + KEMGU'nun kendi özyinelemesi — AST'ye geçmeden):**
- **Fonksiyon tablosu** (Fonksiyonlar struct): ön-geçiş `islev` tanımlarını
  kaydeder (ad → param adları + gövde '{' konumu); normal yürütmede tanım gövdeleri
  `islev_atla` ile atlanır.
- **Çağrı (cagri_yap):** argümanlar ÇAĞIRANIN kapsamında değerlendirilir → YENİ
  kapsam itilir (params bağlanır) → gövde yürütülür → dönüş yayılır.
- **Kapsam yığını:** `Semboller.ust` = mantıksal tepe. dizi_pop YOK → slot'lar
  yeniden kullanılır (ust kaydet/sıfırla = push/pop); arama tepeden tabana (en
  yakın bağlama → özyinelemede her çağrının param'ı İZOLE). Global = en alttaki.
- **don:** dondu bayrağı + donus değeri (Semboller'de 1-elemanlı Dizi); blok/döngü
  her deyimden sonra dondu kontrolüyle erken çıkar (exception'sız erken-dönüş).
- **İMLEÇ özyineleme-güvenli:** cagri_yap çağrı-sonrası konumu (`resume`) ve kapsam
  tabanını (`marker`) YEREL değişkende saklar → KEMGU'nun çağrı yığını her seviyeyi
  korur → paylaşılan imleç doğru kaydedilip geri yüklenir. **Flat-token'ın
  çağrı/dönüş için "zorlanması" bu yerel-kaydet deseniyle çözüldü; AST rewrite
  GEREKMEDİ.**
- Lexer'a `,` (18), `don` (19), `islev` (20) + 2-param çağrı eklendi.

**Doğrulama (adversarial, 9 program):** faktöriyel (fakt(5)=120), Fibonacci
(fib(10)=55, fib(12)=144), çok-param (topla(40,2), carp(6,7)), KARŞILIKLI özyineleme
(cift/tek = isEven/isOdd), fonksiyon-içi döngü (kareler(10)=45), iç içe çağrı
(kare/iki), fonksiyondan global erişim (g=30; ekle(12)). Headline: fakt(5)-78 = 42.
opt-verify PASS. **ASan/UBSan TEMİZ** — fib(12) derin özyineleme dahil 0 ihlal
(kapsam-yığını slot reuse belleği sınırlar).

**Testler:** test_llvm 231→**233** ([verify]+[run]). asan_e2e_denetim otomatik
kapsar (örnek temiz). Derleyici DOKUNULMADI → diğer suite'ler etkilenmez.

**Self-hosting durumu:** Mini-dil V3 = lexer + öncelikli parser + kontrol akışı +
fonksiyonlar/özyineleme + kapsam — tam bir Turing-tam toy dil, saf KEMGU'da.
**Bu, KEMGU'nun ifade-gücü kanıtının (D-022→D-034) son büyük yapı taşı.**
Sıradaki gerçek faz: bu demolar self-hosting PROXY'siydi; asıl adım gerçek KEMGU
derleyicisinin bir parçasını (doğal ilk parça: gerçek KEMGU lexer'ı) KEMGU'da
yazmak — daha büyük, planlı bir faz (ayrı konuşulacak).

---

## D-033 — Self-hosting: mini dil V2 — KONTROL AKIŞI + deyim blokları (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-028 (atama + sembol
tablosu + ifade) üstüne, mini-dili gerçek bir İMPERATİF dile çıkardım: koşul
(`eger`/`degilse`), döngü (`iken`) ve `{ deyim* }` blokları
(`test/ornekler/17_kontrol_dili.kem`). "Çok-deyimli → gerçek derleyici alt-kümesi"
adımı; kontrol akışı = gerçek dil. **Bounded:** yalnız bu dilim — fonksiyon-tanımı
SONRAKİ dilime bırakıldı (DUR-SOR sınırına uyuldu).

**Yeni yetenekler (hepsi saf-KEMGU, mevcut intrinsic'lerle):**
- **Lexer V2:** çok-harf IDENT + ASCII anahtar kelime tanıma (eger/degilse/iken,
  metin_esit ile) + 2-karakter `==` (lookahead) + `< > { }` token'ları.
- **İfade:** karşılaştırma seviyesi (`<`/`>`/`==`, sonuç 1/0) toplama üstünde.
- **Yürütücü (flat-token, ağaç-yürüyen):** `deyim_calistir` (atama/eğer/iken
  dağıtımı), `blok_calistir` (`{ deyim* }`), `blok_atla` (eşleşen `}` say,
  yanlış dal/döngü-çıkışı için), `eger_calistir` (koşullu dal + opsiyonel
  degilse), `iken_calistir` (koşulu re-eval + gövde tekrar yürütme, imleç
  konum kaydet/sıfırla). Mini-dilin if/while'ı KEMGU'nun if/while'ıyla yorumlanır.

**Doğrulama (adversarial, 18+ program):** iken-döngü toplam (1..10=55), eğer/değilse
her iki dal, if-içinde-döngü, döngü-içinde-if, İÇ İÇE döngü, ardışık döngüler,
boş blok, faktöriyel (3!=6), false-from-start döngü, çok-harf değişken, iç içe
if-else. Headline: `i=0;t=0; iken(i<10){t=t+i+1;i=i+1;} r=0; eger(t==55){r=t-13;}
degilse{r=0;} r` = 42. opt-verify PASS. **ASan/UBSan TEMİZ** (42, 0 ihlal — yoğun
dizi kullanımı, D-029/D-030 fix'leri + matris kapsaması geçerli).

**Testler:** test_llvm 229→**231** ([154] verify + [155] run). asan_e2e_denetim
84→85 (yeni örnek otomatik kapsanır, temiz). Derleyici dokunulmadı → diğer suite'ler
etkilenmez. **Sıradaki:** mini-dilde fonksiyon-tanımı/çağrısı (ayrı büyük dilim),
sonra gerçek derleyici alt-kümesi.

---

## D-032 — ASan/UBSan bellek güvenliği matrisi: D-029/D-030 eksenleri kalıcı regresyon ağı (2026-06-13)

**Karar:** D-029 (yapı alan-adı çözümü) ve D-030 (dizi_olustur element_byte
heap-overflow) hatalarının yaşadığı EKSENLERİ sınır-noktalarında zorlayan, kendini
doğrulayan (başarı=exit 42) temsili bir program seti — `test/asan_matris/m01..m10.kem`
— + `test/asan_matris_calistir.sh` + `make calistir_asan_matris`. Her program HEM
sanitizer'sız (değer doğruluğu) HEM ASan/UBSan altında (bellek güvenliği) koşulur.
Tam kartezyen değil; her eksende SINIR-NOKTALARI.

**Kapsanan eksenler (neden D-030/D-029'a odaklı):**
- **Eleman-byte sınırı** (D-030 element-SIZE'dı): tam32 (4-byte) vs tam64/metin/&Yapi
  (8-byte). m01-m04. 8-byte tipler için olustur(4)+10/20 ekle = eski "kapasitenin
  yarısında heap-overflow" tetikleyicisini + realloc büyüme yolunu zorlar.
- **Küçük eleman**: tam8/tam16 (i32 stride) m05.
- **dizi_yaz in-place** (D-025) × tam64 + metin × sınır-üstü indeks: m06, m07.
- **Yapı konfigürasyonları** (D-029 alan-adı): tek yapıda KARIŞIK eleman-byte
  koleksiyonlar (Dizi<tam32>+Dizi<metin>+Dizi<tam64> bir arada, her biri doğru
  byte) m08; iki yapı AYNI alan adı 'ad' FARKLI eleman tipi (T.ad=metin 8-byte,
  U.ad=tam32 4-byte) m09; &Yapi param üzerinden alan erişimi m04/m08/m09;
  tam-kapasite + realloc geçişi m10.

**Sonuç: 10/10 TEMİZ — değer-doğru + ASan/UBSan ihlali YOK.** Yeni codegen bug'ı
bulunmadı. D-029/D-030 fix'leri tüm sınır eksenlerinde geçerli. Bu, KEMGU'nun
çekirdek iddiasına (bellek güvenliği — buffer-overflow imkansız) sınır-zorlamalı
bir güven verir + kalıcı regresyon ağı (gelecekte element-byte/alan-çözüm
regresyonlarını yakalar). Derleyici DOKUNULMADI (yalnız fikstür + harness +
make hedefi). Tüm suite yeşil, temiz build.

---

## D-031 — ASan/UBSan codegen denetimi: harness + 8 pre-existing crash teşhisi (2026-06-13)

**Bağlam:** D-030 (dizi_olustur heap-overflow) gösterdi ki test_llvm E2E zinciri
(`kemgu --llvm | clang | run`) üretilen kodu SANITIZER'SIZ koşuyor → codegen
bellek hataları gözden kaçıyor. KEMGU'nun #1 hedefi bellek güvenliği olduğundan,
proaktif bir ASan/UBSan denetimi eklendi.

**Karar:** `test/asan_e2e_denetim.sh` + `make calistir_asan_denetim` — tüm
çalışabilir örnekleri `clang -fsanitize=address,undefined` ile derleyip çalıştırır,
ihlalleri raporlar. **Sonuç: 84 örnek ASan/UBSan-TEMİZ**, 8 bilinen başarısızlık
(ALLOWLIST, nedeni belgeli), 27 atla (main yok / parse-only).

**Bulunan 8 pre-existing crash (test suite E2E koşmadığı için saklıydı):**

*Sınıf A — dizi-literal → `Dizi<T>` parametresi (4):* `03_kontrol`, `35_binary_search`,
`36_quicksort_stub`, `40_dizi_islemler`. Kök-neden: array literal `[1,2,3]` STACK
`[N x T]` üretir; `Dizi<T>` PARAMETRESİ ise dinamik `KdlDizi*` bekler (param
`dinamik_dizi_mi=1`). `xs[i]` / `için x: xs` / `dizi_boyut(xs)` stack array'i
KdlDizi olarak okur → misaligned access → SEGFAULT. (Dinamik dizi `dizi_olustur`
parametre olarak ÇALIŞIR — D-024; yalnız stack-literal→param yolu kırık.)
**KARAR Mehmet'e açık (DUR-SOR):** stack-array-literal ↔ dinamik-KdlDizi temsil
uyumsuzluğu. Seçenekler: (a) çağrı sınırında literal→KdlDizi coercion, (b) array
literal Dizi<T> bağlamında daima heap, (c) `için`/`[]` param için stack-array
yolu. Hepsi temsil/semantik kararı — tek başıma değiştirmedim.

*Sınıf B — lambda/closure (4):* `04_islev`, `10_lambda`, `25_closure_capture`,
`42_lambda_hesap`. Garbage func-ptr çağrısı → access-violation. **Bilinen:
D-004 ile LAMBDA codegen V2'ye ERTELENDİ** (fonksiyon-değer codegen yok). Yeni
bug değil; bu örnekler ertelenen özelliği egzersiz ediyor.

**Kapsam:** Bu commit DENETİM ALTYAPISI + teşhis. 8 crash'in fix'i ayrı
(A = temsil kararı Mehmet'te; B = V2 lambda feature). Harness bunları ALLOWLIST'le
dışlar; fix indikçe ALLOWLIST'ten çıkarılır → denetim regresyon koruması olur.
Bu, derleyici tip-kontrolünden geçen ama segfault eden programların (bellek
güvenliği ihlali) gelecekte yakalanmasını kurumsallaştırır.

---

## D-030 [YÜKSEK] — Kök-neden fix: dizi_olustur element_byte heap-buffer-overflow (ptr/tam64 dizi) (2026-06-13)

**Bağlam:** D-029'da "kapsam dışı, pre-existing" diye bırakılan `m*n*p+18`=18 bug'ı.
Runtime trace + **AddressSanitizer** ile kök-neden bulundu — CİDDİ bir bellek
güvenliği (heap-buffer-overflow) bug'ı.

**KÖK-NEDEN (ASan KANITI):**
```
AddressSanitizer: heap-buffer-overflow WRITE size 8 in kdl_dizi_ekle_ptr
  <- token_ekle <- lexle ;  allocated by kdl_dizi_kapasite_ayarla <- calistir
```
`dizi_olustur` codegen'i element_byte'ı **SABİT 4** emit ediyordu
(`kdl_dizi_olustur(i32 4)`). `dizi_olustur(N)` → `kapasite_ayarla(N)` →
buffer = N×4 byte. Ama `Dizi<metin>` (8-byte ptr eleman) `dizi_ekle_ptr` ile
8-byte yazıyor → N×4 buffer yalnız **N/2 ptr** tutar; (N/2+1). yazım (boyut hâlâ
< kapasite olduğu için realloc tetiklenmeden) HEAP'i taşırıyor. Token-adı dizisi
`dizi_olustur(32)` → 128 byte → 16 ptr; 17. token (`t.ad[16]`) taşma → komşu
bellek + sonraki metin_kes buffer'ları bozuluyor → değişken adı GARBAGE →
`sembol_ara` bulamıyor → 0 → `m*n*p`=0. Non-deterministik (ASLR'ye göre değişen
garbage) — D-029'da kafa karıştıran buydu.

**Neden 17 token (m*n*p) çalışıp 19 (m*n*p+18) çökmüştü:** ikisi de `t.ad[16]`'ı
taşırır ama +18 fazladan 2 taşma yazımı (17,18) yapıp "p" adının buffer'ını
deterministik olarak eziyordu; 17-token tek taşma çoğu zaman zararsız komşuyu
bozuyordu (şanslı 24).

**Fix [YÜKSEK]:** dizi_olustur element_byte'ı eleman tipinden hesapla:
ptr/i64 → 8, i8/i16/i32 → 4. Kaynak: `g->beklenen_tip` (değişken annotasyonu
`Dizi<T>`). Bilinmiyorsa (struct-alan inşası — yapi_olustur_uret per-alan
beklenen_tip set etmez) **8 = güvenli max** (i32'yi 2x reserve eder ama taşma
İMKANSIZ; tüm eleman tipleri ≤8 byte). kapasite_ayarla N×8 ayırır → ptr/tam64
güvenli; sonraki dizi_ekle realloc'ları zaten sizeof ile doğru.

**Etki:** Bu bug `Dizi<metin>`/`Dizi<&T>`/`Dizi<tam64>` >N/2 eleman tutan HER
programı sessizce bozuyordu (yalnız demo değil). Bellek güvenliği — KEMGU'nun
çekirdek hedefi.

**Repro test (kırmızı→yeşil):** `test/snapshots/dizi_metin_kapasite.kem` — 20
metin (>16) ekle+oku = 42. Fix öncesi ASan heap-overflow + crash (127); sonrası
ASan TEMİZ + 42. test_llvm [150]. 16_degiskenli_dil.kem ana ifadesi
`m=2;n=3;p=4;m*n*p+18` (19 token, >16) yapıldı — gerçek demo bağlamında regresyon
koruması. Adversarial: 4-değişken zincir (`m*n*p*q-78`), uzun ifadeler hepsi 42.

**Doğrulama:** test_llvm +2 ([150] kapasite, demo güncel). 22 suite + 0 ASan +
prod 0 uyarı + stdlib 12. (D-029'daki "kapsam dışı pre-existing" notu → ÇÖZÜLDÜ.)

---

## D-029 [YÜKSEK] — Kök-neden fix: yapı alan-adı çakışması codegen bug'ı (D-028 PROB ÇÖZÜLDÜ) (2026-06-13)

**Bağlam:** D-028 PROB'u "dizi_olustur-alan-init'li 2. yapı, 1. yapının koleksiyon
alanını bozuyor (boyut 3→6)" diye gözlemlemişti. Runtime trace ile KÖK-NEDEN
bulundu — gözlem yanlış çerçevelenmişti (yapı-yerel kopya değil).

**KÖK-NEDEN 1 (alan-adı çözüm bug'ı) — KANIT (runtime trace):**
`kdl_dizi_olustur/ekle/boyut` pointer + boyut trace'lendi. Minimal repro'da
(T{kind,deger,ad,imlec} + U{ad,deger}; `te()` t.kind/t.deger/t.ad'ye ekler):
```
ekle_tam CE0(kind)  ekle_tam 68A0(deger)  ekle_ptr CE0(ad → YANLIŞ! 6780 olmalı)
```
`dizi_ekle(t.ad, …)` t.ad'ye (6780) değil **t.kind'e (CE0, alan 0)** yazıyordu →
t.kind 3 yerine 6 eleman (3 kind + 3 ad), t.ad boş. İKİ KOLEKSİYON ALİASLANMIYOR;
**t.ad alanı YANLIŞ alana (kind) çözülüyor.** Tetikleyici: U'nun da `ad` alanı
olması (U.ad index 0). Hipotez U alan adlarını `isim/sonuc` yapınca doğrulandı (33).

**Mekanizma (codegen):** `erisim_uret` (+`erisim_lvalue`), nesnenin IR tipi `ptr`
(yani &Yapi parametre) iken yapı tipini IR'dan çıkaramıyor → **TÜM yapılarda alan
adına göre ilk eşleşeni arıyordu** (llvm.c eski 1227-1234). T.ad (index 2) + U.ad
(index 0) varken `t.ad` → U'ya çözülüp `getelementptr %U, …, 0, 0` = T'nin alan 0'ı
(kind). GEP base nesne.reg (gerçek T) olduğu için sessizce kind'e yazıyordu.

**Fix 1:** `LlvmIsim.ref_yapi_ir` alanı eklendi (&Yapi/*Yapi/Yapi değişken/param
→ "%T"). `ref_yapi_ir_al()` helper'ı referans/pointer soyup yapı IR'ını verir.
param + annotasyonlu `değişken` kaydında set edilir. `erisim_uret`/`erisim_lvalue`
artık nesne TANIMLAYICI ise kayıtlı yapı tipini kullanır; alan-adı arama yalnız
SON ÇARE (yapı tipi bilinmiyorsa). Struct-VALUE (`%T`) yolu zaten doğruydu.

**KÖK-NEDEN 2 (yan keşif — struct-bundled proof yazarken):** `dizi_al(s.ad, i)`
s.ad bir `Dizi<metin>` ALANI iken SEGFAULT. `dizi_eleman_beklenen` çıkarsaması
yalnız TANIMLAYICI arg0 (düz değişken) için çalışıyordu; struct-alan dizi (s.ad)
için eleman tipi çıkarsanmıyor → `kdl_dizi_al_tam` (i32) route edip metin ptr'ini
i32 okuyordu → çöp ptr → strcmp segfault.
**Fix 2:** `dizi_alan_eleman_ir()` helper'ı — dizi-builtin arg0 DUGUM_ERISIM ise
alanın Dizi<T> eleman IR tipini çözer. Inference bloğuna ERISIM dalı eklendi.

**Repro test (önce KIRMIZI sonra YEŞİL):** `test/snapshots/yapi_yerel_bozulma.kem`
(33 bekleniyor, bug'da 63) → `test_yapi_alan_cakismasi` (test_llvm [150]).
**Canlı kanıt:** `test/ornekler/16_degiskenli_dil.kem` açık-param workaround'undan
YAPI-PAKETLİ sürüme taşındı (Tokenler + Semboller, İKİSİ DE `ad` alanı taşıyor —
çakışma senaryosu) → "x=6;y=7;x*y" = 42 (test_llvm [152]).

**Doğrulama:** test_llvm 227→**228**, +22 suite (tip_kontrol 174, snapshot 50, …).
0 ASan. Prod 0 uyarı. stdlib 12 OK. Repro 63→33, sembol-tablosu segfault→42.

**Kapsam dışı (PRE-EXISTING, bu fix değil):** "m=2;n=3;p=4;m*n*p+18" = 18 (3-değişken
çarpım zinciri + toplama) HEM yapı-paketli HEM commit'li açık-param D-028'de
başarısız → ayrı, önceden var olan demo/codegen sorunu (m*n / m*n-39 / m*3+1 çalışır;
çarpım-zinciri+toplama dar bir kombinasyon). Bu fix'ten bağımsız; ayrı göreve havale.

---

## D-028 — String stdlib IV: değişkenli mini dil (string-anahtarlı sembol tablosu) + PROB: yapı-yerel codegen bug'ı (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** Self-hosting'in ad
çözümü çekirdeği: değişkenli mini dil — atama + STRING-ANAHTARLI sembol tablosu
(`test/ornekler/16_degiskenli_dil.kem`). `( DEĞİŞKEN '=' ifade ';' )* ifade`.
Sembol tablosu = paralel `Dizi<metin>` (adlar) + `Dizi<tam32>` (değerler); arama
`metin_esit` ile (byte-byte ad). Token adları `metin_kes(kaynak, i, 1)` ile
çıkarılır (tek-harf değişken). `"x=6;y=7;x*y"` → 42.

**Yeni doğrulanan yetenekler:** `Dizi<metin>` (ptr-eleman dizisi, string saklar),
`metin_kes(start, **uzunluk**)` semantiği (DOĞRULANDI: start,length — bitiş değil),
`harf_mi` ile identifier lexing, `metin_esit` ile string-key sözlük arama.

**Doğrulama (adversarial, 8 senaryo):** `x=6;y=7;x*y`=42, `x=6;x*7`=42,
`a=2;b=3;c=7;a*b*c`=42, `x=50;y=8;x-y`=42, `x=5;x=42;x`=42 (yeniden-atama/güncelle),
`z=10;(z+4)*3`=42 (değişken+parantez), `40+2`=42, `x=21;x+x`=42. opt-verify PASS.
test_llvm 225→**227**. 0 ASan. Derleyici dokunulmadı.

**✅ ÇÖZÜLDÜ → D-029 (kök-neden: alan-adı çakışması, yapı-yerel kopya DEĞİL).**

**🔴 PROB (bu çalışma sırasında bulunan CİDDİ codegen bug'ı) — yapı-yerel
bozulması:** İlk denemede token+sembol tablosu İKİ `yapı` (Tokenler + Semboller,
Dizi alanlı) olarak paketlenmişti. ÇALIŞMADI. İzole edilen kök neden:
> **Bir `yapı` yerel değişkeni inşa edildiğinde (`değişken u: U = U { f:
> dizi_olustur(N), ... }`), önceden tanımlanmış BAŞKA bir `yapı` yerelinin
> koleksiyon-alanı içeriği BOZULUYOR.** Minimal repro: `t: T` (Dizi alanlı)
> oluştur+doldur (boyut 3), sonra `u: U` (yine `dizi_olustur` alan-init'li)
> oluştur → `dizi_boyut(t.kind)` 3 yerine **6** okuyor.
> - İkinci yapı KOLEKSİYONSUZ ise (`V { x: tam32 }`) → bozulmuyor (33 ✓).
> - İkinci yapının inşası `dizi_olustur` çağırıyorsa → bozuyor (63 ✗).
> IR yüzeysel doğru (alloca %T %0/%1, construct→%1, copy→%0); mekanizma
> struct-by-value yerel kopya/alloca etkileşiminde, runtime tracing gerekiyor.
> NOT: D-027 (15_agac_insa) İKİ yapı (Agac+Tokenler) kullanıp ÇALIŞIYOR —
> tetikleyici spesifik (yapı-alan-okuma aynı fonksiyonda + dizi_olustur'lu 2.
> yapı). Ayrı görev olarak fix'e havale edildi (spawn_task).

**Workaround (shipped):** Yapı-paketleme yerine açık `Dizi` PARAMETRELERİ
(D-024/D-026 deseni — kanıtlı). faktor/terim/ifade 6 param alır (kindler,
degerler, adlar, imlec, s_ad, s_deg). Verbose ama sağlam; yerel Dizi'ler
(ptr) yapı-yerel bug'ından etkilenmez.

**Self-hosting durumu:** LEXER + PARSER + AST + EVAL + DEĞİŞKEN/SEMBOL — bir mini
dilin tüm derleyici fazları KEMGU'da. Sıradaki: yapı-yerel bug fix (sonra struct
bundling temizliği), çok-deyimli dil, uzun vade derleyici alt-kümesi.

---

## D-027 — Self-hosting CAPSTONE: tam derleyici hattı (lex→parse(AST inşa)→eval) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** Self-hosting'in eksik
ORTA parçası. D-026 parser'ı değeri doğrudan hesaplıyordu; D-027 parser önce bir
SOYUT SÖZDİZİM AĞACI (AST) İNŞA ediyor, sonra AYRI bir geçiş ağacı geziyor — gerçek
bir derleyicinin yapısı (`test/ornekler/15_agac_insa.kem`). Tam hat KEMGU'da:
**lex → parse(AST inşa) → eval(AST gez).**

**AST temsili — İNDEKS-TABANLI ARENA:** düğümler paralel `Dizi<tam32>`'lerde
(tur/deger/sol/sag), çocuklar İNDEKS ile gösterilir (pointer değil). Bu, KEMGU'nun
KENDİ derleyicisinin arena+AST modelinin (ast.c) birebir KEMGU karşılığı —
self-hosting'e en yakın yapı. Heap-tahsisli özyinelemeli çeşit (henüz yok)
GEREKTİRMEZ; mevcut dizi intrinsic'leriyle (dizi_ekle=düğüm ayır, dizi_al=oku,
dizi_yaz=imleç) tamamen ifade edilir. Arena append-only → indeksler kararlı.

**Yeni doğrulanan kompozisyon yeteneği:** Diziler `yapı` içinde paketlenip
&referansla aktarılır (`yapı Agac { tur: Dizi<tam32>; ... }`, `&Agac` param,
`a.tur` field→dizi erişimi, struct construction'da `dizi_olustur()` field değeri).
struct + koleksiyon kompozisyonu E2E çalışıyor (probe ile doğrulandı). İmleç + iki
struct (Agac arena + Tokenler) → parser durumu 2 param.

**Doğrulama (adversarial, 10 ifade — AST yolu üzerinden):** `2+4*10`=42,
`2+3*4`=14 (öncelik), `(2+3)*4`=20, `2*(3+(4*5))`=46, `100-2*3-2`=92,
`((100-16))/2`=42, `1+2*3+4*5+15`=42, `(((7)))`=7. opt-verify PASS. test_llvm
223→**225**. 0 ASan. Derleyici dokunulmadı.

**Self-hosting tablosu — 4 parça da KEMGU'da:**
| Faz | Demo | Temsil |
|-----|------|--------|
| LEXER | D-024 | metin → token akışı (Dizi) |
| PARSER | D-026/D-027 | token → öncelikli AST (arena) |
| AST | D-027/D-022 | indeks-arena / özyinelemeli çeşit |
| EVAL | D-027/D-022 | ağaç gezme |

**Sıradaki:** string-key sembol tablosu (`metin_esit` + paralel ad/değer dizileri)
→ değişkenli ifadeler; sonra çoklu-deyim + atama (mini dil); uzun vade gerçek
derleyici alt-kümesinin KEMGU'da yeniden yazımı.

---

## D-026 — String stdlib III: özyinelemeli-iniş öncelikli ayrıştırıcı (parser yarısı) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-024 token akışı
üzerine self-hosting'in PARSER yarısı: operatör ÖNCELİĞİ + PARANTEZ destekli
özyinelemeli-iniş (recursive-descent) ifade ayrıştırıcısı
(`test/ornekler/14_oncelikli_ayristirici.kem`). Gramer:
`ifade=terim(('+'|'-')terim)*; terim=faktör(('*'|'/')faktör)*; faktör=SAYI|'('ifade')'`.

**Enabling (D-025):** Özyinelemeli iniş, çağrılar arası paylaşılan MUTABLE konum
imleci gerektirir. KEMGU'da global mutable yok + skalerler değerle geçer →
imleç = tek-elemanlı `Dizi<tam32>` (ptr → referansla paylaşılır), `dizi_yaz` ile
yerinde ilerletilir. faktör→ifade→terim→faktör KARŞILIKLI özyineleme (forward
referans; iki-geçişli pre-populate codegen'de islev_kayit pre-pass ile çözülür).

**Doğrulama (adversarial, 17 ifade):** Öncelik — `2+4*10`=42 (soldan-sağa 60
DEĞİL), `2+3*4`=14, `100-2*3`=94, `2*3+4*5`=26. Parantez — `(2+3)*4`=20,
`(2+4)*(3+4)`=42, `2*(3+(4*5))`=46, `((9))`=9. Bölme — `100/2-8`=42, `84/2`=42,
`(100-16)/2`=42. opt-verify PASS. test_llvm 221→**223**. 0 ASan. Derleyici
dokunulmadı.

**Tuzak:** `iken doğru { ... ver ... }` idiomu (KEMGU'da `break` keyword YOK) —
döngüden yalnız erken `ver` ile çıkılır; codegen + opt-verify temiz.

**Self-hosting durumu:** Artık 3 parça da KEMGU'da ÇALIŞIYOR — LEXER (D-024
metin→token), PARSER (D-026 token→öncelikli değerlendirme), AST+EVAL (D-022
özyinelemeli çeşit yorumlayıcı). Sıradaki: parser'ın değerlendirme yerine çeşit
AST İNŞA etmesi (token→AST), sonra string-key sembol tablosu (metin_esit).

---

## D-025 [YÜKSEK] — dizi_yaz intrinsic: in-place eleman güncelleme (mutable cursor) (2026-06-13)

**Karar [YÜKSEK — yeni intrinsic]:** `dizi_yaz<T>(d: Dizi<T>, i: tam32, e: T) ->
boş` — dinamik dizinin i. elemanını YERİNDE günceller. Koleksiyon API'sinde
göze batan eksiklik: `dizi_ekle` (append) + `dizi_al` (oku) vardı ama eleman-SET
yoktu. Bu, recursive-descent parser'ın paylaşılan MUTABLE KONUM İMLECİ için şart
(tek elemanlı Dizi<tam32>, ptr → çağrılar arası paylaşılır, dizi_yaz ile ilerletilir).

**Uygulama (dizi_al/dizi_ekle simetrisi):**
- tip_kontrol.c: `dizi_yaz` özel-cased (DUGUM_CAGRI) — 3 arg, arg0 Dizi<T>, arg1
  tam32 indeks (T028), arg2 eleman T (T001 uyumsuzluk).
- llvm.c: element-tip varyant dispatch (i32→kdl_dizi_yaz_tam, i64→_tam64,
  ptr→_ptr); index i32'ye, değer eleman-tipine cast. declare satırları eklendi.
  **dizi_deger_arg:** dizi_ekle/al'da değer/indeks arg[1]; dizi_yaz'da DEĞER
  arg[2] — `dizi_eleman_beklenen` forward'ı bu pozisyona yönlendirildi (önceki
  sabit `i == 1` literal-eleman-tip çıkarsamasını yanlış arga verirdi).
- runtime: kdl_dizi_yaz_tam/_tam64/_ptr — sınır dışı (i<0||i>=boyut)/NULL →
  sessizce yok say (boyut BÜYÜTMEZ; büyütme dizi_ekle ile). dizi_al ile simetrik.

**Doğrulama:** in-place (d[1]=40) + cursor (c[0]=c[0]+2) E2E; tam32 + tam64
varyant dispatch ayrı ayrı E2E (42). Tam regresyon: test_llvm 220→**221**, +22
suite (tip_kontrol 174, snapshot 50, parser 107, …). 0 ASan. Prod 0 uyarı.
stdlib --check 12 OK. (Sınır: shrink/insert/remove yok — append+set+read yeterli.)

---

## D-023 [YÜKSEK] — String stdlib I: metin_bayt intrinsic + metin literal pre-pass düzeltmesi (2026-06-13)

**Bağlam:** Self-hosting'in 2. ön-koşulu "gerçek string işlemleri" (Mehmet: "4
konsolide, sonra 3"). Mevcut `kdl_metin_*` yüzeyi zaten geniş (uzunluk, birleştir,
kes, içerir, başlar/biter, kırp, yer_değiştir, küçük/büyük±tr/ascii) AMA bir
tokenizer'ın temel taşı eksikti: **indeksli karakter erişimi**.

**Karar 1 [YÜKSEK — yeni intrinsic]:** `metin_bayt(s: metin, i: tam32) -> tam8`
— s'in i. HAM BAYT'ı (UTF-8; ASCII'de = karakter). Sınır dışı/NULL → 0 (taşma
imkansız, KEMGU güvenlik hedefi). `metin_uzunluk` ile birlikte bir metin üzerinde
bayt-bayt gezinmeyi (lexer döngüsü) sağlar. Ayrıca `metin_esit(metin,metin) ->
mantıksal` builtin olarak bağlandı (runtime'da `kdl_metin_esit` zaten vardı ama
tip-kontrol builtin tablosuna kayıtlı değildi — anahtar kelime tanıma için).
Runtime'daki ESKİ `int kdl_metin_esit` (ölü; ne builtin ne C çağıranı vardı)
`_Bool` dönen tek sürümle değiştirildi (i1 declare + diğer boolean metin fn'leriyle
tutarlı). DEĞER naming: metin_bayt/metin_esit — temiz.

**Karar 2 [ORTOGONAL CORRECTNESS FIX]:** Metin literal pre-pass (`ast_taransa_
metinleri`, @.str.N toplayıcı) **cast düğümünü taramıyordu**. `metin_uzunluk("...")
olarak tam32` gibi — literal `DUGUM_TIP_DONUSTUR` (x olarak T) altında kalınca
"kayitsiz" düşüp **sessizce `add i32 0,0`'a** derleniyordu (yanlış değer, hata yok).
Eklenen case'ler: DUGUM_TIP_DONUSTUR (.kaynak), DUGUM_LAMBDA (.govde),
DUGUM_KULLAN_IFADE/DUGUM_IMHA_IFADE (.operand). Bu, TÜM metin builtin'lerini
literal+cast argümanıyla kullanılabilir yapar (yaygın durum). Bug sınıfı: herhangi
bir metin literali taranmayan bir düğümün altında → sessiz miscompile.

**Doğrulama:** `metin_uzunluk("hello")`=5, `metin_bayt("ABC",1)`='B'=66,
`metin_esit("ver","ver")`=1 — hepsi literal+cast argümanla E2E. Saf-KEMGU
tokenizer `test/ornekler/12_metin_tokenizer.kem` (metin_uzunluk + metin_bayt ile
"N+N+N" bayt-bayt tarama): "10+20+12" = 42. Bir KEMGU programı kendi girdisini
karakter karakter okuyabiliyor — lexer/self-hosting temeli.

**Tam regresyon:** test_llvm 215→**218** (+metin_bayt/esit/tokenizer). tip_kontrol
174, snapshot 50 (IR baseline drift YOK — fikstürlerde cast-altı metin yok),
otp_cli 9, parser 107, lexer 103, linear 57, drf 39, capability 40, sabitsure 39,
wcet 35, mmio 23, simd 30, simd_llvm 5, arena/ast/tip/sembol/json/lsp/bolge/escape.
**0 ASan.** Prod temiz rebuild **0 uyarı.** stdlib --check yeşil.

**Sınırlar / sıradaki:** metin_bayt BYTE döner (UTF-8 codepoint değil) — ASCII
tokenizing için doğru; çok-baytlı codepoint iterasyonu V2. İsimle değişken arama
hâlâ slot-id (string-key assoc V2). Koleksiyon tarafı (Liste<T>) zaten KdlDizi
runtime'da var; tokenizer'ın token LİSTESİ üretmesi (dizi_ekle ile) doğal sonraki
adım. Sonra: gerçek lexer → parser (self-hosting derleyici çekirdeği).

---

## D-024 — String stdlib II: iki fazlı lexer → token akışı → değerlendirici (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — örnek + test, derleyici değişmedi]:** D-023'ün doğal devamı
(Mehmet: "string/**koleksiyon** stdlib"). Mevcut KdlDizi koleksiyonu (`dizi_olustur/
ekle/al/boyut`) + D-023 metin primitifleri birleştirilerek self-hosting'in GERÇEK
mimarisi gösterildi: metin önce TOKEN AKIŞINA çevrilir (lexer), sonra AYRI bir geçiş
bu akışı değerlendirir (`test/ornekler/13_token_akisi.kem`).

**Önce paralel "Harita" workflow'u:** KdlDizi yüzeyi (intrinsic'ler, element-tip
çıkarsama, runtime, kanıtlı kalıplar, riskler) 5 paralel okuyucuyla eksiksiz
haritalandı (ultracode). Çıkan iki kritik gerçek E2E probe ile doğrulandı:
- **Proven kalıp:** `dizi_olustur → iken dizi_ekle → iken dizi_al` toplama (15 ✓).
- **YENİ doğrulanan yetenek:** `Dizi<tam32>` FONKSİYON PARAMETRESİ olarak çalışır
  (4×10+2=42 ✓). Eski CLAUDE.md notu "dizi param yok" STATİK dizi içindi; dinamik
  Dizi = ptr olduğundan sorunsuz aktarılır. İki fazlı mimariyi mümkün kılar.

**Tasarım:** Token = iki PARALEL `Dizi<tam32>` (kindler + degerler). Tür kodları
0=SAYI/1=ARTI/2=CARPI/3=EKSI. `lexle(metin, kindler, degerler)` bayt-bayt tarar,
sayıları biriktirir, operatörleri token'lar (diziler referansla aktarılır).
`degerlendir(kindler, degerler)` akışı soldan sağa hesaplar. Token kuralı:
SAYI (op SAYI)*.

**Doğrulama (adversarial):** 8 ayrı ifadeyle birden — `2*3+36`=42, `7*6`=42,
`100-58`=42, `2*3*7`=42, `50-3-5`=42, `1+2+3`=6, `9`=9, `10*10-58`=42. Çok-basamaklı
sayı, üç operatör, tek sayı, değişken token sayısı — hepsi doğru (şanslı 42 değil).
opt -passes=verify PASS. test_llvm 218→**220** ([143] verify + [144] run).
0 ASan. Derleyici dokunulmadı → diğer suite'ler etkilenmez.

**Tuzak (kayda değer):** `uygula` bir ANAHTAR KELİME (trait impl) — fonksiyon adı
olamaz; `op_uygula` yapıldı. (35 keyword listesi: işlev adlarında kaçınılmalı.)

**Sıradaki:** gerçek lexer→parser (parantez/öncelik), veya token'ı (kind,value)
çift olarak tek dizide (struct/çeşit element) — şimdilik paralel-dizi pragmatik.
String-key sembol tablosu (metin_esit ile) self-hosting derleyici için gerekecek.

---

## D-001 [YÜKSEK] — Modül ad-mangling şeması: `@<modul>.<ad>` (2026-06-11)

**Karar:** Modül üyesi işlevler IR'da `@modul.ad` olarak emit edilir; iç içe modül
`@m1.m2.ad` (nokta ayraçlı zincir). `mat::kare(x)` çağrısı yol zincirinden noktalı
ada knit edilip (`mat.kare`) kayıt tablosundan çözülür.

**Gerekçe:** LLVM `@` adları nokta içerebilir (örn. `@llvm.x86.*`); KEMGU
tanımlayıcılarında `.` olamayacağı için düz-ad uzayıyla çakışma imkânsız. `$`
zaten generic monomorphization'da kullanılıyor (`kimlik$i32`) — modül için ayrı
ayraç, iki mekanizmanın okunabilir kalmasını sağlar.

**Önceki durum (audit DUR-SOR #2):** Modül üyeleri HİÇ emit edilmiyordu;
`mat::f()` çağrısı `; HATA: cagri hedefi tanimlayici degil` yorumuyla sessiz 0
dönüyordu.

**Kapsam/sınırlar:**
- Modül gövdesi içinden kardeş işleve çıplak-ad çağrı: `aktif_modul_onek`
  fallback'iyle çözülür (önce düz ad, bulunamazsa `<önek>.<ad>`).
- Modül içi `sabit`/`yapı`/`çeşit` üyeleri v1'de mangle edilmiyor (yalnız işlev —
  audit'in bulduğu gap). Gerekirse ayrı iş.
- **AÇIK:** tip_kontrol modül-üye çağrısını zaten çözemiyor (`T016: modul
  bulunamadi`, tek seviyede bile; iç içe yol "yol cozumlemesi karmasik") —
  ÖNCEDEN VAR OLAN sınır, bu kampanyanın scope-lock'u (src/llvm.c + test/)
  dışında. Codegen artık hazır; tip_kontrol çözümü ayrı C-track işi.

## D-002 [YÜKSEK] — ve/veya kısa-devre (short-circuit) semantiği (2026-06-11)

**Karar:** `a ve b` / `a veya b` standart kısa-devre: sol taraf yeterliyse sağ
taraf DEĞERLENDİRİLMEZ. Lowering: alloca + koşullu dal + her dalda store + load
(phi yerine mevcut codegen'in bellek-slot deseni). Sonuç tipi `i1`.

**Gerekçe:** Önceki `and/or i32` lowering'i her iki tarafı da değerlendiriyordu —
yan etkili sağ taraf (çağrı) için SESSİZ-YANLIŞ semantik. Kısa-devre, KEMGU'nun
örtük-dönüşümsüz/çökmez felsefesiyle uyumlu tek doğru davranış (Rust/C aynı).

**Sınır:** WCET maliyeti (wcet.c, scope dışı) her iki dalı toplamaya devam ediyor —
kısa-devre sonrası bu ÜST SINIR olarak güvenli tarafta kalır (değişiklik gerekmez).

## D-003 — Heap `d[i] = v` eleman ataması: kampanya KAPSAMI DIŞI (2026-06-11)

Heap dizi (KdlDizi) eleman ataması `runtime/`'a setter gerektiriyor
(`kdl_dizi_yaz_eleman` yok); kampanya scope'u `src/llvm.c + test/`. Şu an
SESSİZ DEĞİL: codegen görünür `; atama: heap dizi eleman atamasi runtime
setter bekliyor` yorumu emit ediyor. Ayrı küçük görevde kapatılacak
(runtime'a ~5 satır setter + llvm.c'de ~10 satır çağrı).

## D-006 — `&p.x` / `&d[i]` parser önceliği: ✅ ÇÖZÜLDÜ (ifade.c) (2026-06-11)

**Bulgu (matris C):** `&p.x` AST'de `(&p).x`, `&d[i]` ise `(&d)[i]` olarak parse
ediliyordu — dökümante önceliğe AYKIRI (postfix `.`/`[]` prefix `&`'den sıkı bağlamalı
→ `&(p.x)` / `&(d[i])` olmalı). Yanlış ağaç codegen'de `(&p)`'yi ptr'e çevirip `.x`'i
ptr-path GEP+load ile değer olarak okuyor; `artir(&p.x)` çağrısında i32 değer ptr-param'a
geçip **segfault**.

**Kök neden:** `parse_onek` (ifade.c) prefix `-`/`~`/`&`/`&değişken`/`*` operandını
`parse_onek` ile alıyordu — bu yalnız bir sonraki öneki işleyip postfix YUTMUYORDU.
`değil` (Madde H) zaten doğru deseni (`parse_oncelik(p, ONC_ONEK)`) kullanıyordu.

**Fix (ayrı görev, scope `ifade.c + test/`):** Beş prefix operatörün operandı da
`parse_oncelik(p, ONC_ONEK)` ile alınır. ONC_SONEK(12) > ONC_ONEK(11) → postfix
(`. [] () :: olarak`) operanda bağlanır; ikili operatörler (≤10) bağlanmaz → `&x+y`
hâlâ `(&x)+y`. İç içe önek (`*&x`, `--x`) korunur (parse_oncelik önce parse_onek çağırıp
sonraki öneki recursive işler). **Öncelik tablosu değişmedi** (lokalize fix, değer-bazlı
ripple yok). Binary `*`/`&` (çarpma/bit) infix konumda, parse_onek'e girmez → etkilenmez.

**Doğrulandı (runtime round-trip):** `&p.x`, `&d[i]`, `&a.b.c` deref-oku → 42 (segfault
yok); regresyon `*(&x)`, `-p.x`=`-(p.x)`, `&x+y`, `&v`, `*p` → hepsi yeşil; parser/
snapshot/fuzzer (20000 iter) + tüm test_tumu yeşil, 0 ASan.

**Not (D-006 dışı, ayrı feature):** `&p.x` üzerinden YAZMA `*p = v` deref-assignment
gerektirir — bu dilde **T022-red** (deref-hedef lvalue değil, tasarım kararı). Scaler
alan referansına yazma ifade edilemez; struct ref'e yazma `ref.alan = v` ile zaten
çalışıyor (&Struct task). `&arr[i].alan` parse artık doğru ama codegen D-007 (struct-
değerli dizi) ile bloklu.

## D-007 — Struct-değerli diziler (`arr[i].alan`, `a.b[i].c`, `d[i][j]`): feature, ertelendi (2026-06-11)

**Bulgu (matris B):** Eleman tipi struct olan diziler (`[P{..}, P{..}]`) — hem stack
(`d[i].alan` okuma exit-yanlış) hem heap (`Dizi<P>` + `dizi_ekle(.., p)` → struct
değerini `kdl_dizi_ekle_tam`'a i32 olarak geçirip clang-fail). Stack tarafı eleman-tipi
takibi (INDEKS `beklenen`'e düşüyor, struct çıkaramıyor); heap tarafı KdlDizi yalnız
i32/i64/ptr saklıyor (struct-by-value runtime gerektirir — D-003 sınıfı, `runtime/`
scope dışı).

**Karar:** Mekanik değil (DIZI_OLUSTUR struct eleman tipi + INDEKS eleman-tip
propagasyonu + lvalue zinciri + runtime aggregate). Kampanyaya dahil EDİLMEDİ; ayrı
"struct-değerli dizi" feature görevi. Skalerli diziler (`d[i]` oku+yaz) ÇALIŞIYOR
(audit gap #2). Çok-boyut `d[i][j]` de aynı feature'a bağlı (ertelendi).

## D-008 — Concurrency (`dondur`/`kanal`/`görev`) codegen: YOK, işaretlendi (2026-06-11)

**Bulgu (matris F):** `dondur(&değişken x)` → `call ptr @dondur(...)` tanımsız sembol
(link-fail); `kanal_oluştur()` → T002 tanımsız. Concurrency / DRF V1 yalnız statik
tip-kontrol katmanında (görev/kanal keyword + DRF001-005); runtime thread/channel +
codegen YOK (CLAUDE.md: "Plan Karar F V2 — runtime thread/channel implementasyonu").

**Karar:** Kampanya dışı — codegen değil, koca bir runtime+codegen alt-sistemi (V2).
İŞARETLENDİ. "Lineer değer kanaldan geçiyor" hücresi (F çapraz) buna bağlı, ertelendi.

## D-009 — `satıriçi_asm` çıktısı struct alanına (`çıktı("=r", &r.deger)`): parser, ertelendi (2026-06-11)

**Bulgu (stretch):** asm `çıktı` clause grammar yalnız düz `&değişken_adi` kabul ediyor
(parser.c P269 TANIMLAYICI bekler); `&r.deger` alan-erişimi P264 ile parse-fail.

**Karar:** C5 v1 tasarımı asm çıktısını düz değişkene bağlar (deyim-form). Alan hedefi
istenirse: asm→temp değişken sonra `r.deger = temp`. Grammar genişletmesi parser.c'de
(scope dışı) + dil kararı. Ertelendi. Çekirdek asm (düz &var çıktı) ÇALIŞIYOR (C5).

## D-005 [YÜKSEK] — İşaretsiz (dtamN) + i1 genişletme: signedness yan-kanalı (2026-06-11)

**Karar:** `dtamN` (işaretsiz tamsayı) değerler IR'da işaret bilgisini `IfadeSonuc.isaretsiz`
/ `LlvmIsim.isaretsiz` / `IslevKayit.donus_isaretsiz` yan-kanalında taşır. İşaretsiz
operand → `udiv`/`urem`/`lshr` + işaretsiz karşılaştırma yüklemi (`ult/ugt/ule/uge`);
genişletme `zext`. **i1 (mantıksal) genişletme HER ZAMAN `zext`.**

**Gerekçe / önceki SESSİZ-YANLIŞ durum:** Tüm tamsayılar işaretli lower ediliyordu:
- `dtam8 200 > 100` → signed `icmp sgt` ile `-56 > 100` = YANLIŞ (probe exit 1, beklenen 42).
- `dtam8 / dtam8` → `sdiv`, `dtam >> 1` → `ashr` (işaret bitini kopyalar).
- **`doğru olarak tam32`** → `sext i1` = `-1`; `41 + (-1)` = 40 (beklenen 42). Bu en
  yaygın gap — her bool→int dönüşümü yanlıştı.

**Kapsam/sınırlar:** Yan-kanal değişken/parametre/dönüş/alan/cast üzerinden akıyor.
İkili işlemde operandlardan biri işaretsizse işlem işaretsiz. Karışık işaretli/işaretsiz
aritmetik tip kontrolde zaten engelli (örtük dönüşüm yok). `{reg,tip}` eski başlatıcılar
C11 gereği `isaretsiz=0` (işaretli) → güvenli varsayılan, regresyon yok.

## D-004 — LAMBDA codegen: V2'ye ERTELENDİ (2026-06-11)

Closure codegen (ortam yakalama, env struct, fonksiyon-pointer ABI'si) mekanik
değil — ayrı feature tasarımı. Kampanyada yok. Mevcut durum: lambda ifadesi
`; ifade tipi desteklenmiyor` + 0 döner; stdlib yüksek-mertebe işlevleri
adlandırılmış işlevlerle çalışıyor (test_stdlib_* yeşil).

## D-010 [YÜKSEK] — Tek-geçiş ad çözümü: resolver binding'i AST'te, codegen tüketir (2026-06-12)

**Karar (çekirdek spec — Mehmet belirledi):** Her ad-referansı düğümüne (`DUGUM_TANIMLAYICI`,
`DUGUM_YOL`) "çözülmüş binding" eklendi (`ast.h`: `cozum_sembol` + `cozum_kategori`
{YEREL, MODUL_UYESI, GLOBAL} + `cozum_modul_onek`). Resolver (`tip_kontrol.c`) binding'i
module-first/lexical kuralla doldurur ve düğüme YAZAR (tek doğruluk kaynağı); codegen
(`llvm.c`) string'le yeniden ÇÖZMEZ, kaydı okuyup tam o sembolü emit eder (`@modul.ad`
mangling'i ile). Sonuç: tip kontrol ile codegen inşa gereği aynı sembole anlaşır.

**Önceki SAPMA (ADIM-0'da ampirik üretildi):** global `f` + modül-kardeşi `f`, modül
içinden çıplak `f()`: tip kontrol modül-kardeşine (lexical, imza-ayrık varyantla kanıtlı),
codegen `islev_bul` global-first olduğundan global'e bağlanıyordu → exit 2 yerine 1;
imza-ayrık ikizde geçersiz IR (`call i32 @f()` vs `define i32 @f(i32)`). Göreli YOL
(`m` içinden `ic::g()`) codegen'de hiç çözülemiyordu (string-knit "ic.g" kayıt "m.ic.g").
Regression guard: `test/snapshots/ad_cozum_sapma.kem` (exit 42) + `ad_cozum_govde.kem`.

**[ETKİ] Taktik seçimler (otomatik uygulandı):**
1. **Binding alanları union DIŞINDA ortak başlıkta** — `dugum_olustur` `arena_ayir_sifir`
   kullandığından varsayılan `COZUM_YOK`/NULL; mevcut düğüm üretim yolları (ifade.c dahil)
   değişmedi, ifade.c'ye dokunulmadı. Maliyet ~24B/düğüm (derleyici için ihmal edilebilir).
2. **`sembol.h`/`tip_kontrol.h` API'sine dokunulmadı** — modül öneki, bulunan SCOPE_MODUL
   scope'undan türetilir (`modul_onek_turet`: parent scope'ta `modul_scope==s` olan modül
   sembolünün adını biriktirir, iç içe "m1.m2"); `ast.h`'de yalnız `struct Sembol` forward
   declaration (sembol.h→ast.h yönlü include, döngü yok).
3. **ÖN-KOŞUL TAMİRİ (`tip_kontrol.c` DUGUM_ISLEV):** İşlev sembolü artık tanımlandığı
   scope'ta aranır (`sembol_bul_yerel(tk->scope)`, eskisi `tk->global_scope`) ve gövde
   scope'unun parent'ı `tk->scope` (eskisi global). Önceki durum: modül işlev gövdeleri
   HİÇ tip-kontrol edilmiyordu (sembol modül scope'unda → sessiz erken dönüş; `ver doğru;`
   → tam32 --check'ten GEÇİYORDU) ya da aynı adlı global ikizin imzasına karşı denetleniyor,
   arity farkında parametre dizisi OOB okumasıyla ÇÖKÜYORDU (RC=139 repro'landı). İkiz/
   builtin-gölgeleme koruması: `ast_dugumu != d` veya param sayısı uyuşmazsa gövde atlanır
   (T024 zaten raporlu).
4. **`ana.c mode_llvm`'e resolver geçişi eklendi (KAPSAM dışı dosya — gerekçe):** codegen'in
   tüketeceği binding'i ancak resolver yazabilir; mode_llvm bugüne dek tip_kontrol'ü HİÇ
   çalıştırmıyordu (CLAUDE.md'deki pipeline tarifi aspirasyoneldi). mode_check kalıbı
   kopyalandı; hatalar `hata_callback_ayarla(sessiz_cb)` ile susturulur → tip hatalı
   programlar --llvm'de ESKİSİ GİBİ emit edilir (CLI çıktısı bayt-bayt korunur), binding'i
   eksik düğümler codegen'de string fallback'ine düşer. Tek arena → Sembol* ömrü
   `llvm_ir_uret` boyunca geçerli (SembolLink linked-list, relocation yok).
5. **Graceful degradation tasarım gereği KALICI:** `COZUM_YOK` → eski global-first +
   aktif-önek-fallback yolu aynen korunur. Sebep: built-in'ler (yazdir/dizi_ekle/
   tekkez_olustur/...) ya sembol tablosunda değil ya da IslevKayit'ta kayıtsız; ayrıca
   resolver koşmadan doğrudan `llvm_ir_uret` çağıran tüketiciler kırılmamalı.
6. **`COZUM_YEREL` callee → indirect-call yolu:** lokal function-pointer, aynı adlı global
   işlevi artık GÖLGELEYEBİLİR (tip kontrolle tutarlı; eski codegen global'i seçerdi).

**Kapsam/sınırlar:** Çoklu-dosya/modül yükleme (A), generic specialization çekirdeği (C),
nitelikli TİP annotation (D) DOKUNULMADI. `DUGUM_ERISIM` (method dispatch) binding'i v2.
Identifier-yük (lvalue/load) codegen'i lokal isim tablosuyla devam ediyor (sapma çağrı
sitelerindeydi); modül-üyesi `sabit` referansı v1'de zaten desteklenmiyor.

## D-011 [YÜKSEK] — Çok-dosya modül temeli: whole-program namespaced yükleme (2026-06-13)

**Karar (yüzey Mehmet kilidi):** `kullan dizi;` nitelikli bağ (düzleştirme YOK),
`kullan dizi::{Liste,ekle};` seçili niteliksiz, `kullan dizi olarak d;` alias; GLOB yok;
modül=dosya (dizi.kem ⇒ `dizi`); arama yolu [içe-aktaran dizini → proje kökü → kütüphane/],
İLK eşleşme kazanır; private-by-default + `genel` export işareti; iki-faz yükleme
(döngüsel import v1'de hata değil); seçili-import çakışması KULLANIMDA T042
(nitelikli erişim geçerli kalır).

**Mimari:** Loader (ana.c `modulleri_yukle`) entry'nin kullan grafiğini BFS gezer, her
dosyayı bir kez parse edip sentetik `DUGUM_MODUL(dosya_modulu=1)` olarak program AST'sinin
başına ekler → `tip_kontrol_program` faz-1 (pre_populate: kanonik kayıtlar) + faz-2
(`kullan_baglari_kur`: görünür bağlar) → B'nin resolver'ı çapraz-dosya adları
`COZUM_MODUL_UYESI` binding'iyle çözer → TEK codegen tüm modülleri `@modul.ad` emit eder.
**B-entegrasyon doğrulaması:** binding düşseydi `@topla` tanımsız kalırdı (link hatası) —
exit-42 E2E testleri bunu yapısal olarak kanıtlıyor.

**[ETKİ-YÜKSEK] Legacy düzleştirme korundu:** Çok-segment çıplak `kullan a::b::c;`
ESKİ düzleştirme yolunda kaldı (tip_kontrol DUGUM_KULLAN + llvm.c kullan pre-pass).
Sebep: drivers/virtio/*.kem (çapraz-dosya struct kullanıyor — D bölgesine bağımlı) ve
test/crossfile fikstürleri bu semantiğe test-pinli; görevin kendi kısıtları
(drivers 2/2 + test_llvm taban düşmeyecek + D'ye dokunma) başka çözüm bırakmıyor.
Yeni biçimler (tek-segment / seçili / alias) HER ZAMAN namespaced yükleyicide.
Çıkış stratejisi: D (nitelikli tip) inince sürücüler yeni biçime taşınıp legacy kaldırılır.

**[ETKİ] Diğer taktik kararlar:**
1. **builtin_scope ayrımı:** built-in'ler global'in PARENT'ına taşındı; dosya-modül
   scope'ları da oraya bağlanır → giriş dosyasının özel adları modüllere sızmaz
   (private-by-default iki yönlü). Yan etki: built-in adı gölgeleme artık T024 değil
   (gölge kazanır) — suite'te pinli test yoktu.
2. **Kanonik modül sembolü gizli (`Sembol.gizli`):** dosya-modül kaydı builtin_scope'ta
   ama normal çözümde görünmez — `dizi::f` import'suz ÇÖZÜLMEZ (T016). Önek türetme
   (`scope_modul_sembolu`) SembolLink'i doğrudan taradığı için mangling etkilenmez.
3. **Seçili alias `ithal_onek` taşır:** `cozum_bagla` alias'ı görünce binding'i
   MODUL_UYESI + asıl modül öneki olarak yazar — codegen değişikliği GEREKMEDİ.
4. **Ad-bazlı modül dedup:** aynı ada ikinci yükleme yok (ilk çözüm kazanır) —
   döngü/elmas importlar doğal sonlanır; farklı dizinlerden aynı ad = tek modül (v1).
5. **mode_llvm yükleme hatasında IR üretmez** (eksik modül zaten link edilemez);
   mode_check yükleme hatasını ayrı sayaçla raporlar ("yukleme N").
6. **Windows UTF-8:** loader `dosya_ac_utf8` (MultiByteToWideChar→_wfopen) —
   kütüphane/ ayağı E2E testle (UTF-8 dizin) doğrulandı.
7. **Modül-içi tip emisyonu gap fix (llvm.c):** yapı/çeşit pre-pass'i artık DUGUM_MODUL
   içine iner (düz adla, ilk-kazanır); metin literal taraması DUGUM_MODUL + DUGUM_ESLES
   düğümlerine de iner (önceden modüldeki stringler @.str'e toplanmıyordu).
8. **v1 sınırları:** modül-içi `sabit` codegen'de kayıtsız (önceden de öyleydi);
   in-file modüllerde görünürlük denetimi YOK (geriye uyum — `dışa` eski anlamında);
   LSP loader koşmaz; aynı adlı struct'lar modüller arası düz IR-ad uzayını paylaşır
   (D'de nitelikli tip ile ayrışacak); seçili/alias çok-segment yol v1'de P046.

**Testler:** test_llvm 182→194 (+12 A testi: çapraz-modül fonk/--check, seçili, alias,
T041 negatif, transitif, gölgeleme, kütüphane-UTF8, modül-içi yapı x2, T042 negatif,
nitelikli-çakışma); parser 102→107 (+5 gramer). Fikstürler: test/moduller/*.kem;
kütüphane fikstürleri runtime'da yazılıp silinir.

---

## D-012 [YÜKSEK] — Çapraz-modül generic monomorphization: FONKSİYON routing (faz C-1) (2026-06-13)

**Bağlam:** A (whole-program namespaced yükleme) + B (tek-geçiş resolver) main'e
merge edildi. Generic gövdeler ZATEN bellekte + ZATEN çözülü. C = çapraz-modül
generic instantiation'ın YÖNLENDİRİLMESİ + EMİSYONU (gövde-görünürlüğü ya da
yeniden-çözüm DEĞİL).

**GAP (kodla teyit edildi):** Çapraz-modül qualified çağrı `m::f(...)` codegen'de
İKİ ayrı yol kullanıyor: TANIMLAYICI yolu (modül-içi/global, `ifade_uret`
DUGUM_CAGRI ~2660) generic'i specialize ediyordu; ama YOL yolu (`m::f` qualified,
DUGUM_YOL hedef, ~1726) jenerik kontrolü YAPMADAN `mik->donus_tip @modul.f`
**plain** emit ediyordu. Sonuç: `call i32 @sayi.azami(...)` → clang `use of
undefined value '@sayi.azami'` (define yalnız `@sayi.azami$i32` adında üretilir).

**Karar (mekanizma):**
1. **Tek-kaynak helper'lar (DRY):** `generic_islev_cagri_uret()` (inference +
   mangle + bekleyen-enqueue + substituted-return emit) ve `generic_param_beklenen()`
   (somut param → IR beklenen; generic-param içeren param → NULL = arg doğal
   tipinden T inference). Her İKİ yol (TANIMLAYICI + YOL) bu helper'ları çağırır;
   TANIMLAYICI'nın eski inline bloğu (~175 satır) helper çağrısıyla değiştirildi.
2. **Mangling şeması [ETKİ]:** Mevcut `$`-specialization mangling AYNEN korunur.
   Modül-nitelik mangled ada zaten gömülü çünkü `gislev->veri.islev.ad` modül
   kaydında `@modul.ad`'a yeniden yazılı (`modul_uyeleri_kayit`); `mangle_et`
   bundan `dizi.ekle$i64` üretir. Modül için AYRI mekanizma EKLENMEDİ — `.` (modül)
   + `$` (specialization) iki ayraç doğal kompoze olur.
3. **Dedup anahtarı [ETKİ]:** Modül-nitelikli mangled ad (`sayi.azami$i32`) =
   `mono_emitlendi` + bekleyenler tarama anahtarı. Aynı specialization birden çok
   use-site'tan referanslansa BİR kez emit. Doğrulama: ikinci $i32 çağrısı + i64
   çağrı, IR'da her define BİR kez (yapısal: duplicate-symbol link hatası yok).
4. **Binding-koruma [ETKİ-YÜKSEK]:** Specialize edilen gövdenin iç kardeş çağrıları
   owning-modülün üyelerine işaret eder — use-site bağlamında YENİDEN ÇÖZÜLMEZ.
   Mekanizma: `specialize_emit` mangled addaki SON `.`'tan öneki türetir
   (`sayi.azami$i32` → önek `sayi`) ve `aktif_modul_onek` olarak kurar; gövdedeki
   çıplak-ad kardeş çağrılar `islev_bul` fallback'iyle `sayi.azami`'ye çözülür.
   Transitif (`azami3$i32 → azami$i32`, hepsi `sayi` bağlamında) bu yolla çalışır.
5. **Linkage seçimi [ETKİ]:** A her şeyi TEK LLVM modülüne splice ettiği için
   mevcut specialization linkage'ı (define, external default) çapraz-modül için
   yeterli — TEYİT edildi (E2E link + çalıştırma). `linkonce_odr`+COMDAT ayrık-
   derleme (v2) işi → **ERTELENDİ** (tek modül, katlanacak ayrı obje yok).

**Kapsam/sınırlar:**
- Bu faz yalnız generic FONKSİYON (`@sayi.azami$i64`). Generic STRUCT (Liste<T>)
  faz C-2 (sonraki commit).
- Doğrulama INFERENCE ile (param tiplerinden T). Explicit yazılı tip-arg
  `f<i64>(...)` parser'da DESTEKLENMİYOR (`yap<tam32>(42)` = karşılaştırma zinciri
  olarak parse ediliyor) → açıkça D-bitişik parser fork; bu görevde EKLENMEDİ,
  witness-param inference ile köşe dönüldü.

**Testler:** test_llvm 194→197 (+3 C testi: çapraz-modül generic fonk --check,
azami$i32+$i64+dedup E2E, transitif azami3→azami E2E). Fikstürler:
test/moduller/{sayi,ana_sayi,ana_sayi_transitif}.kem. parser 107, tip_kontrol 174
düşmedi. In-file generic canary'ler (kimlik<T>, Liste<T>) yeşil.

---

## D-013 [YÜKSEK] — Çapraz-modül generic STRUCT (Liste<T>): routing-only (faz C-2) (2026-06-13)

**HEADLINE bulgusu (kritik):** Çapraz-modül generic STRUCT, faz C-1'in (D-012)
FONKSİYON routing düzeltmesi DIŞINDA **HİÇBİR ek codegen değişikliği gerektirmedi**.
Bu, görevin temel içgörüsünü doğrular: C = instantiation'ın YÖNLENDİRİLMESİ, struct
layout yeniden-mimarisi DEĞİL.

**Neden routing yetti (struct-mono yaklaşımı [ETKİ]):**
- Liste<T> **type-erased**: `%Liste = type { ptr, i64, i64 }` — `*T`→`ptr`, T IR
  layout'ta yok (D-011 #8: modüller arası düz IR-ad uzayı). Tek `%Liste` tüm T'ler
  için geçerli → struct için PER-T layout specialization GEREKMEZ (fonksiyon-mono'dan
  *temelde farklı bir yaklaşım* değil — aynı `$`-makinesi).
- T yalnız (a) specialized fonksiyon gövdesinde subst (T→i64 push edilir → `*T`
  pointee, `bölge_al` sizeof doğru), (b) inference yan-kanalları (`generic_arg_ir`,
  `pointee_llvm_tip`) ile taşınır.
- `oluştur`/`ekle`/`al`/`büyü` çapraz-modül çağrıları D-012 YOL routing'iyle
  `@kap.oluştur$i64` vb. olarak specialize+emit edilir; struct değer (`%Liste`
  by-value dönüş) + `&Liste<T>` by-pointer param mevcut v2/v3 makinesi.

**Doğrulama (saf INFERENCE — yazılı nitelikli tip YOK):**
- HEADLINE: `kullan kap; değişken l = kap::oluştur(sifir); kap::ekle(&l,10)…;
  kap::al(&l,0)+kap::al(&l,4)` → 42. Transitif büyü<T> (kapasite 0→4→8, eleman-
  kopyalı grow) `@"kap.büyü$i64"` olarak owning-modül bağlamında specialize edilir
  (5. eleman idx4'e düşer — grow olmasa heap-overflow; deterministik 42 = yapısal kanıt).
- Çoklu-tip: aynı Liste<T> i64+i32 → ayrık specialization'lar (`@kap.ekle$i64` /
  `@kap.ekle$i32`), paylaşılan `%Liste`. 40+2=42.
- Dedup: `ekle$i64` 2 çağrı → 1 define (yapısal: link hatası yok).

**Witness-param inference [ETKİ — DUR-SOR yerine köşe dönüşü]:** Üretimdeki
Liste<T> `oluştur`'ı T'yi DÖNÜŞ-bağlamı annotasyonundan (`değişken l: Liste<tam64>`)
çıkarır — ama nitelikli annotasyon (`kap::Liste<tam64>`) D işi + headline bunu
YASAKLIYOR. Explicit call-site tip-arg (`oluştur<i64>()`) parser'da YOK (yeni
semantik fork → kapsam dışı). Çözüm: minimal kapsayıcıda her generic fonk bir
**tip-tanık** value-param taşır (`oluştur<T>(taban: T)`, `büyü/al` zaten T-param'lı)
→ T arg'dan çıkarsanır, annotasyon/explicit-tip-arg GEREKMEZ. Üretim Liste<T>'nin
TAM taşınması (yetki-disiplinli oluştur'ın return-context inference'ı) follow-up;
ilgisiz altsistem (capability-borrow) genişletilmedi.

**Kapsam/sınırlar:**
- Fikstür modülü `kap` (test/moduller/kap.kem) — `kütüphane/dizi.kem` in-file
  canary'siyle (kendi main'i var) çakışmamak için ayrı ad.
- Liste<T> uzunluk/sınır built-in dönüş tipi taşımaz; minimal oluştur/ekle/al/büyü.
- Yazılı nitelikli generic-tip annotasyonu (`kap::Liste<i64>`) → D (dokunulmadı).

**Testler:** test_llvm 197→200 (+3 C-2: struct --check, headline oluştur/ekle/al+büyü
E2E, çoklu-tip i64+i32 E2E). Fikstürler: test/moduller/{kap,ana_kap,ana_kap_coklu}.kem.
parser 107, tip_kontrol 174 düşmedi. In-file Liste<T> (kütüphane/dizi.kem) canary yeşil.

---

## D-014 — Üretim Liste<T> gerçek çapraz-dosya modüle taşındı (relocation + PROB) (2026-06-13)

**Bağlam:** D-013 minimal `kap` container'ıyla çapraz-modül struct'ı kanıtlamıştı.
Bu adım ÜRETİM `kütüphane/dizi.kem` Liste<T>'sini gerçek importable modül yapar —
v1'de ERTELENEN relocation (B+A+C ile mümkün): Liste o zaman top-level'a zorlanmıştı
çünkü (a) modül-içi struct emisyonu (A) + (b) cross-module generic routing (C) yoktu.

**Yapı kararı (no-src-change, stdlib + test):**
- `kütüphane/dizi.kem` artık **mat.kem düzeni**: top-level `genel yapı Liste<T>` +
  7 op (`genel işlev`), AÇIK `modül dizi { }` SARMALAYICISI YOK. Loader dosyayı
  zaten `modül dizi`'ye sarar; açık sarmalayıcı `dizi.dizi` İÇ-İÇE olurdu (loader
  `fprog.uyeler`'i sentetik DUGUM_MODUL üyesi yapar — ana.c:315). → Liste<T> artık
  modül dizi üyesi (A modül-içi struct emisyonu → type-erased `%Liste`).
- In-file `main` + test_* (v1 self-contained) **kaldırıldı** — import edilince entry
  main'iyle çakışır. Doğrulama ayrı çapraz-dosya entry'lerine taşındı
  (test/moduller/dizi_{kullan,coklu,yapi}.kem; `kullan dizi;`, kütüphane/ arama
  yolundan bulunur). "In-file canary" → "çapraz-dosya canary" (eşdeğer kapsam yeşil).

**PROB RAPORU (görevin asıl çıktısı):**
1. **`oluştur` INFERENCE-FRIENDLY DEĞİL → witness-param gerekti.** Üretim imzası
   `oluştur(böl: yetki<Bellek>) -> Liste<T>` — paramlarında T YOK → T yalnız
   dönüş-bağlamı annotasyonundan (`değişken l: Liste<tam32> = ...`) çıkarsanırdı.
   Çapraz-dosya'da: yazılı nitelikli annotasyon (`dizi::Liste<tam32>`) = D (yasak);
   explicit call-site tip-arg (`oluştur<i64>()`) parser'da YOK (karşılaştırma zinciri
   olarak parse). **DEMO adaptasyonu:** `oluştur<T>(taban: T)` tip-tanık param (değer
   kullanılmaz) + yetki içeride üretilir → T arg'dan çıkarsanır. **Üretim API kararı
   DEĞİL** — gerçek çözüm explicit-type-arg parsing (ayrı görev, syntax fork) ya da
   return-type-driven inference. `ekle`/`al` zaten T-değer param'lı → çıkarsama sorunsuz.
2. **Capability-borrow workaround çapraz-dosya SORUNSUZ.** `yetki<Bellek>` lineer-MOVE
   disiplini (her op taze `yetki_olustur(3,3)`, son `geri_al`; ekle→büyü MOVE) çapraz-
   dosya'da AYNEN çalışır — yetki built-in'leri modül çözümünden bağımsız, specialized
   gövdede owning-bağlamda emit edilir. Runtime-link (kdl_yetki_*) sorunsuz.
3. **Küçük inference WART (düzeltilmedi, raporlandı):** `değişken l = dizi::oluştur(...)`
   (annotasyonsuz) sonucu `l` element-tip yan-kanalı (`generic_arg_ir`) TAŞIMAZ —
   yalnız değişken-annotasyonundan set ediliyor. Sonuç: T'nin TEK kaynağı `&Liste<T>`
   param olan doğrudan çağrı (örn. `dizi::boy(&l)`) i32'ye default'lar → `@dizi.boy$i32`
   emit edilir. **ZARARSIZ** burada: `boy` dönüşü somut `tam64`, gövde T'den bağımsız —
   $i32/$i64 gövdesi ÖZDEŞ. Ama T-DÖNÜŞLÜ böyle bir op olsaydı yanlış tip verirdi.
   Transitif çağrı (ekle→büyü, l param subst'lı) DOĞRU ($i64). Gerçek çözüm: generic
   call sonucunu değişkene atarken instantiated-T'yi yan-kanala propagate et (küçük
   codegen işi, kapsam dışı — mono değil, ergonomi). DUR-SOR yerine raporlandı.

**Doğrulama (saf inference, hepsi exit 42):** çapraz-dosya grow headline (oluştur/ekle×5/
al + transitif büyü, kapasite 0→4→8), çoklu-tip (i64+i32 ayrık spec, paylaşılan %Liste),
struct-eleman (Liste<Nokta> karışık genişlik). IR: `@dizi.{oluştur,ekle,al,boy}$i64` +
`@"dizi.büyü$i64"` owning-bağlamda; çoklu-tip $i64/$i32 ikiz set.

**Kapsam/sınırlar:** src/ kodu DEĞİŞMEDİ (stdlib + test). Yazılı nitelikli tip (D),
legacy flatten, proofs/, bölge/escape/wcet/lsp dokunulmadı. struct-eleman Liste<Nokta>
çalışıyor (D-013'te denenmemişti — burada doğrulandı).

**Testler:** test_llvm 201→203 (stdlib_liste_e2e in-file→çapraz-dosya güncellendi;
+çoklu-tip +struct-eleman; --check modül-alone). Fikstürler: kütüphane/dizi.kem (v2
yeniden yazıldı), test/moduller/dizi_{kullan,coklu,yapi}.kem. parser 107, tip_kontrol
174, drivers (uart_vtable 21) düşmedi. 0 ASan. stdlib --check yeşil.

---

## D-015 [YÜKSEK] — Nitelikli tip annotation (`modül::Tip<args>`): D dilim-1 (2026-06-13)

**Bağlam:** D-014 relocate PROB #1: üretim `oluştur(böl: yetki<Bellek>) -> Liste<T>`
paramlarında T YOK → çapraz-dosya T inference için ya nitelikli annotation ya
explicit-type-arg gerekir. Bu dilim nitelikli TİP annotation'ı getirir: in-file
return-context inference'ı çapraz-dosyaya açar; relocate'in DEMO witness-param'ını
kaldırır (üretim imzası geri).

**Karar (mekanizma):**
1. **Parser `::`-in-type-position [ETKİ — ifade::ile karışmaz]:** `ifade.c parse_tip`
   tanımlayıcı dalında `::` zinciri → DUGUM_YOL → `DUGUM_TIP_KULLANICI{yol:YOL, tip_arg}`.
   `tip_kullanici.yol` ZATEN "DUGUM_TANIMLAYICI veya DUGUM_YOL" kabul ediyordu (ast.h).
   `dizi::Liste<i64>` (args) ve `dizi::Nokta` (0-arg) → ikisi de TIP_KULLANICI.
   AMBIGUITY YOK: `::` yalnız tip pozisyonunda (annot/param/dönüş); ifade-pozisyonu
   `f<i64>()` (explicit-type-arg) AYRI sorun, DOKUNULMADI. "Dizi" özel-case yalnız
   NİTELİKSİZ (`dizi::Dizi` değil).
2. **Resolver TİP-namespace [ETKİ-YÜKSEK]:** `ast_tip_to_bilgi` DUGUM_TIP_KULLANICI
   artık YOL yol'u çözer: `yol_modul_scope_coz(sol)` → hedef modül scope, `sembol_bul_yerel`
   → SEMBOL_YAPI. Value-path (`dizi::ekle`) çözümünün YANINA; tip & value AYNI scope,
   SEMBOL kategorisiyle ayrışır (SEMBOL_YAPI=tip). Çözülen TipBilgisi DÜZ adlı
   (`Liste`) → mono key C ile AYNI (type-erased `%Liste`).
3. **Gizli-aware fallback [ETKİ-YÜKSEK — faz ordering]:** Param/dönüş tipleri
   `pre_populate` (faz-1, imza) içinde çözülür — `kullan_baglari_kur` (faz-2,
   görünür alias) ÖNCE çalışmaz → görünür `dizi` yok, yalnız GİZLİ kanonik
   builtin_scope'ta. Çözüm: nitelikli tip çözümünde görünür-alias bulunamazsa
   builtin_scope kanonik dosya-modülüne düş (tek-segment). Gerekçe: tip pozisyonu
   modülü açıkça adlandırır + modül zaten yüklü (loader bir `kullan` ister). Değişken
   annotasyonu (faz-3 gövde) görünür yolu kullanır; ikisi aynı modül_scope'a varır.
4. **Çapraz-modül yapı ALAN erişimi [ETKİ-YÜKSEK]:** `n.x` (n: sekil::Nokta)
   tip kontrolde yapının ALAN listesini ister; `sembol_bul(scope, "Nokta")` niteliksiz
   görünmez (Nokta sekil'de). `yapi_sembol_capraz_bul`: önce görünür scope (gölgeleme
   korunur), bulunamazsa YÜKLÜ tüm modül scope'larında DÜZ adla ara. Codegen'in düz
   IR-ad uzayıyla tutarlı (D-011; per-modül ayrım D ileri dilim). Yalnız DUGUM_ERISIM
   alan-çözümünde kullanılır (struct construction değil).
5. **Yan-kanal annotasyondan (PROB #3 by-pass) [ETKİ]:** `generic_arg_ir_al` zaten
   `tip_kullanici.tip_arg[0]`'ı yol-tipi gözetmeksizin okur → nitelikli annotation
   element-tip yan-kanalını besler. `değişken l: dizi::Liste<tam64>` → l.generic_arg_ir=i64
   → `dizi::boy(&l)` artık `@dizi.boy$i64` (i32 DEĞİL). codegen değişikliği yalnız
   `ast_tip_to_ir` YOL→`%sag_ad` (düz yapı adı).
6. **RENAME fold:** kütüphane/dizi.kem `oluştur`→`oluştur` (yetki_olustur ile tutarlı);
   DEMO witness-param `taban: T` KALDIRILDI — üretim imzası `oluştur(böl) -> Liste<T>`.
   T artık nitelikli annotation'dan (return-context).

**Doğrulama (hepsi exit 42, ÜRETİM imzası, nitelikli annotation):**
- HEADLINE (PROB #1 çözüldü): `değişken l: dizi::Liste<tam64> = dizi::oluştur(böl);
  ekle×5 + transitif büyü (0→4→8 grow); al(0)+al(4)`. `@dizi.boy$i64` (PROB #3 by-pass).
- Çoklu-tip (i64+i32 nitelikli annot, ayrık spec). struct-eleman `dizi::Liste<Nokta>`.
- Param nitelikli tip `&dizi::Liste<tam64>` (imza fazı, gizli-aware fallback).
- Çapraz-modül struct USE: `sekil::Nokta` (değişken+param) + factory kur + `n.x` alan.

**Kapsam/sınırlar [DUR-SOR yerine raporlandı]:**
- **Explicit call-site tip-arg (`oluştur<i64>()`) parser'da YOK** — ifade-pozisyonu,
  ayrı görev + syntax fork. Bu dilim DEĞİL (tip-pozisyonu `::` ile karışmaz).
- **Nitelikli yapı KURMA ifadesi (`sekil::Nokta { ... }`) parser'da YOK** (P082) —
  ifade-pozisyonu, D ileri dilim. struct USE testi factory (`sekil::yap`) ile kurar.
- İç-içe nitelikli tip (`a::b::Tip`) gizli-aware fallback yalnız tek-segment; çok-segment
  görünür-alias (faz-3) yolundadır. Legacy flatten/D2, drivers, proofs/, bölge/escape/
  wcet/lsp DOKUNULMADI.

**Testler:** test_llvm 203→205 (+2 D: nitelikli param E2E, çapraz-modül struct USE E2E).
Fikstürler: test/moduller/{dizi_nitelikli_param,sekil,sekil_kullan}.kem + dizi_{kullan,
coklu,yapi}.kem nitelikli annotation'a güncellendi. kütüphane/dizi.kem oluştur+üretim imza.
parser 107, tip_kontrol 174, drivers (uart_vtable 21) düşmedi. 0 ASan. stdlib --check yeşil.

---

## D-016 — D2 (legacy flatten kaldırma): ADIM-0 araştırma → DUR-SOR (kod DEĞİŞMEDİ) (2026-06-13)

**Görev:** Çok-segment `kullan a::b::c;` legacy flatten'i kaldır; drivers/virtio +
test/crossfile bağımlılarını D1 (nitelikli tip) yoluna taşı. **SONUÇ: yapısal +
D1-aşan blocker → DUR-SOR. Hiçbir kod/test/fikstür değişmedi (baseline 205 korundu).**

**Legacy flatten ne yapıyor (file:line):**
- Parser: `src/parser.c:816-926` — `kullan a::b::c;` → `segment_sayi>1` (seçili/alias yok).
- Loader: `src/ana.c:181-206` — legacy formları ATLAR (`kullan_yeni_bicim` filtresi).
- Tip kontrol: `src/tip_kontrol.c:4807-4885` (DUGUM_KULLAN legacy) — `a::b::c`→`a/b/c.kem`
  dosyasını yükler, `tip_kontrol_program`'ı RECURSIVE çağırır → yüklenen dosyanın
  top-level bildirimlerini İÇE-AKTARAN scope'a NİTELİKSİZ kaydeder (= flatten).
- Codegen: `src/llvm.c:4138-4199` — dosyayı yükler, top-level üyeleri programa DÜZ
  (plain-ad) splice eder. **Constants top-level olduğu için codegen INLINE eder.**

**Bağımlı listesi (tam):**
- GATED (test_llvm suite): `test/crossfile/{transitif,lib_islem,sonuc_cagri,lib_sayi,
  lib_sonuc}.kem` — yalnız FONKSİYON (`dışa işlev uc_kat/iki_kat/bol`), struct/sabit yok.
- GATESİZ (suite'te değil): `drivers/virtio/*.kem` (6) + `tests/drivers/virtio/*.kem` (9)
  — `sabit` CONSTANTS (constants.kem ~her dosyada) + `işlev` + struct. Çoğu private
  (`işlev`/`sabit`, `genel` değil); flatten görünürlüğü yok sayar.

**PROBE sonuçları (go/no-go):**
- ✅ Struct selective import: `kullan mod::{Nokta}; Nokta{x,y}; n.x` → exit 42. Niteliksiz
  construct + alan erişimi ÇALIŞIR.
- ❌ **Constant cross-file codegen GAP (KRİTİK blocker):** `kullan konst::{DEGER}` VE
  `konst::DEGER` — `--check` GEÇER ama E2E **exit 0** (değer değil). Cross-file `sabit`
  codegen'de KAYITSIZ (D-011 belgeli: "modül-içi sabit codegen'de kayıtsız"). Legacy
  flatten çalışıyor ÇÜNKÜ constants'ı top-level splice ediyor (codegen inline).
  Yeni modül sistemi modül-içi sayar → emit etmez → 0.
- ❌ `dışa işlev` + selective import → T041 ("'genel' değil"). `dışa` ≠ `genel`; migrasyon
  `dışa`→`genel` görünürlük değişikliği ister (yüzey-sözdizimi değil).
- virtio entry testleri ZATEN KIRIK: `virtio_blk_init_test` (11 hata), `virtio_blk_oku_test`
  (14 hata) bugün `--check` GEÇMİYOR (aktif/eksik virtio track). Korunacak yeşil yok.

**DUR-SOR gerekçesi (brief koşulları karşılandı):**
1. **D1'i AŞAR:** virtio constants.kem'i her yerde kullanır; cross-file `sabit` codegen
   yok → migrasyon constants'ı bozar (0). Yeni codegen özelliği gerekir (yüzey-sözdizimi
   DEĞİL, kapsam dışı).
2. **YAPISAL iş:** `genel` görünürlük değişikliği onlarca `sabit`/`işlev`'de; virtio
   zaten kırık (baseline yok). Minimal selective-import dokunuşu değil.
3. Flatten kaldırma TÜM bağımlıların migrasyonunu ister; virtio migrate edilemiyor →
   kaldırma BLOKE. Kısmi (yalnız crossfile) migrasyon kaldırmayı sağlamaz + gated
   fikstürleri risksiz değiştirmez → yapılmadı.

**D2 için ön-koşul (sonraki adım):** (a) cross-file `sabit` codegen (modül sabitlerini
kaydet/inline) — ayrı dilim; (b) virtio + crossfile `dışa`→`genel` + selective-import
migrasyonu; SONRA flatten kaldırılabilir. Alternatif: virtio track'i ayrı ele al.

**Doğrulama:** Kod/test DEĞİŞMEDİ. test_llvm 205, parser 107, tip_kontrol 174, drivers
(uart_vtable 21/uart_16550 13) korundu. ELLEME (proofs/, bölge/escape/wcet/lsp, D1
faz-reorder, per-modül namespacing) DOKUNULMADI.

---

## D-017 [YÜKSEK] — İsimlendirme borcu: yasaklı üretici sözcüğü → `olustur` (depo-geneli) (2026-06-13)

**Direktif (DEĞER — istisnasız):** Türkçe keyword/fonksiyon/intrinsic/örnek/yorumlarda
yasaklı üretici sözcüğü KULLANILMAZ. Standart karşılık: → `olustur` (üreticiler;
`_*` son-eki → `_olustur`), `yetki_olustur` ile tutarlı ASCII. (`çevrim`/`cevrim` =
CPU cycle/WCET — DOĞRU, dokunulmadı; 33 örnek korundu.)

**Kapsam:** Depo-geneli ~670 örnek (4 izole commit, her biri build+test yeşil).

**[YÜKSEK] — Üretici intrinsic adı değişikliği (derleyici tanıma):**
- `tekkez` üretici intrinsic → `tekkez_olustur`: tip_kontrol.c + llvm.c eşleştiricileri
  (string + bayt-uzunluk 12→14).
- `sabitsüre` üretici intrinsic → `sabitsüre_olustur`: ÜÇ eşleştirici (tip_kontrol.c ×2
  [biri beklenen-bağlam çıkarsama, 3523 — atlanırsa op testleri kırılır], llvm.c ×1),
  bayt-uzunluk 16→18 (`sabits`+ü[2 byte]+`re_olustur`). `_is_*` C değişkeni de yeniden
  adlandırıldı. Snapshot .ast baseline'ları (18_tekkez, 29/30_linear) regen.
  **Tuzak [ETKİ]:** ikinci/üçüncü eşleştiricinin bayt-uzunluğu kolayca atlanır — string
  güncellenip uzunluk eski kalırsa eşleşme sessizce DÜŞER (test_sabitsure 4 hata yakaladı).

**Üretici fonksiyonlar:** `anahtar_olustur` (kripto: stdlib/kripto/anahtar.kem def +
test_kripto* çağrı + test_k5_anahtar_olustur), `kap` fikstürü `olustur<T>`, test_tip_kontrol
gömülü Hasta üreticisi. Tümü ASCII `olustur`.

**Yorum/doc/Lean-yorum:** src yorumları + test etiketleri (.c/.h ASCII, .kem/.md Türkçe
morfoloji: `…ılır`→`oluşturulur`, `…an`→`oluşturan`, bağlanma ünlüsü). belgeler/*.md,
README, CLAUDE.md, KILAVUZ, spec'ler. proofs/*.lean YALNIZ YORUM (gerçek Lean kodu
yasaklı sözcük İÇERMİYOR — V2-hipotetik adlar yorumda; Lean derlemesi etkilenmez).

**Doğrulama:** Depo-geneli grep (büyük/küçük harf duyarsız, .git hariç) = **0 örnek**.
`çevrim`/`cevrim` = 33 (korundu). Tam test: test_llvm 205, tip_kontrol 174, linear 57,
sabitsure 39, arena 19, ast 31, tip 26, sembol 18, capability 40, snapshot 50, drf 39.
kripto + stdlib --check geçti. 0 ASan. Temiz rebuild 0 uyarı. BUNDAN SONRA yeni örnek
GİRİLMEZ.

---

## D-018 [YÜKSEK] — Payload-taşıyan çeşit (sum type with data) + recursive AST (C3) (2026-06-13)

**Stratejik hedef:** Evrensel OS / self-hosting. Direktif ön-koşulu: "payload-taşıyan
çeşit [AST için şart]". Bu adım payloadsuz çeşit'i (C2.7) payload-taşıyan + ÖZYİNELEMELİ
sum type'a genişletir → KEMGU kendi AST'sini kendi çeşit'leriyle temsil edebilir.

**Sözdizim/semantik [YÜKSEK]:**
- Tanım: `çeşit Ifade { Tam(tam64), Ikili(tam64,tam64), Yok }` — varyantlar tipli
  alanlar taşır (payloadsuz varyant aynı çeşitte serbest).
- İnşa: `Ifade::Ikili(30, 12)` (CAGRI(YOL,args) — ayrı sözdizim eklenmedi, mevcut
  parse'a oturdu).
- Eşleş destructuring: `Ifade::Tam(v) =>` / `Ifade::Ikili(a, b) =>` (DESEN_YOL
  alt-desenleri → payload tiplerine bind).

**Temsil (hibrit) [ETKİ]:** Payloadsuz çeşit → bare iN disc (C2.7 DEĞİŞMEDİ — geriye
uyum). Payload çeşit → `%Ad = type { iDISC, [tüm varyant payload alanları peş peşe] }`
(sonuç `{tag,T,H}` deseni; union DEĞİL — basitlik + ABI by-value). Varyant vi'nin alan
ofseti = `1 + sum(payload_sayilari[0..vi-1])`. AST: cesit struct'a paralel diziler
(`varyant_payload_tipleri` Dugum***, `varyant_payload_sayilari` int*); desen_yol'a
`alt_desenler`.

**Recursive çeşit [ETKİ-YÜKSEK — self-hosting HEADLINE]:** `çeşit Agac { Yaprak(tam64),
Dal(&Agac, &Agac) }` — özyineleme `&Agac` REFERANSI ile (ptr, sonlu boyut
`%Agac={i8,i64,ptr,ptr}`). Eşleş `&Cesit` scrutinee'sinde OTOMATIK DEREFERENCE eklendi
(tip_kontrol: TIP_REFERANS→hedef; llvm: ptr→`load %Cesit`, desenlerden çeşit çözülür).
Özyinelemeli gezinme E2E çalışır.

**Tip kontrol:** `cesit_yapici_tip_kontrol` (M002 yok varyant, M003 arity, M004 payload
tip) iki CAGRI yolunda. DESEN_YOL payload binding (alt-desen → SEMBOL_DEGISKEN, varyant
tipinde). Exhaustiveness mevcut (varyant adı bazlı — payload drilling gerekmiyor v1).

**Doğrulama (E2E exit 42):** Ifade{Tam/Ikili/Yok}; Olay{Tus(tam8),Konum(Nokta),
Cift(tam8,tam64),Bos} (karışık genişlik + STRUCT payload); Agac recursive AST; mini
aritmetik AST değerlendirici örneği `(3+4)*6` (test/ornekler/10_cesit_ast.kem).

**Kapsam/sınırlar:**
- Generic çeşit (`çeşit Kutu<T>`) HÂLÂ yok (parser P353 reddi — ayrı dilim).
- Türkçe (non-ASCII) çeşit/yapı TİP ADI codegen'de quote edilmiyor (`%İfd` geçersiz
  LLVM ad) — YAPI emisyonuyla ORTAK pre-existing sınır; örnekler ASCII tip adı kullanır
  (Ifade/Agac/Olay). Quote'lama ayrı robustness işi.
- Exhaustiveness payload-desen-derinliği denetlemez (varyant kapsaması yeterli).
- Nitelikli payload çeşit yapıcısı (`m::Cesit::V(args)`) codegen'de sol=TANIMLAYICI
  varsayar (modül-içi/düz ad). Çapraz-modül payload çeşit follow-up.

**Testler:** test_llvm 205→210 (+5 C3: payload verify/run, struct+karışık, recursive
verify/run). parser 107 (payload+desen sözdizimi), tip_kontrol 174, snapshot 50, linear
57, drf 39, lexer 103, ast 31, arena 19. 0 ASan. Temiz rebuild 0 uyarı. Fikstürler:
test/snapshots/cesit_{payload,payload_yapi,agac}.kem + test/ornekler/10_cesit_ast.kem.
4 izole commit (parser→tip→codegen→recursive→test).

---

## D-019 — Türkçe (non-ASCII) yapı/çeşit TİP ADLARI IR'da quote'lanır (2026-06-13)

**Değer (Türkçe kimlik — istisnasız):** KEMGU AST düğümleri doğal Türkçe adlarla
temsil edilebilmeli (`çeşit Düğüm`, `yapı Köşe`, `İfade`). Önceki durum: non-ASCII
tip adı `%Düğüm` GEÇERSİZ LLVM identifier → `clang` "expected top-level entity" /
"Cannot allocate unsized type". D-018 örneği bu yüzden ASCII (`Ifade/Agac/Olay`)
kullanmak zorundaydı. (Pre-existing: YAPI codegen'i de quote etmiyordu.)

**Çözüm [ETKİ]:** `yapi_ad_ir(g, ad, uz)` — yapı/çeşit IR tip adını üretir;
ASCII-güvenli değilse `%"Ad"` quote'lar (yerel_ad_yaz ile aynı kural). Simetrik
okuma `yapi_bul_ir(g, ir)` — `%"Ad"` stringinden quote'u soyup YapiKayit bulur.
Düzeltilen emisyon noktaları (TUTARLI olmalı, yoksa LLVM ad-uyuşmazlığı):
- ast_tip_to_ir (DUGUM_TIP_BASIT + KULLANICI struct/çeşit) → yapi_ad_ir.
- cesit_struct_ir → yapi_ad_ir. yapi_olustur_uret alloca tipi → yapi_ad_ir.
- yapi_tip_tanimlari_emit (yapı + çeşit tanımı `%Ad = type`) → yerel_ad_yaz.
- erisim GEP (struct alan adresi ×2) → yerel_ad_yaz.
Okuma noktaları → yapi_bul_ir: erisim_uret (alan erişimi), erisim_lvalue (×2).
İşlev adları (`@"köşe_topla"`) ZATEN yerel_ad_yaz ile quote'luydu.

**Doğrulama:** `yapı Köşe + çeşit Düğüm (recursive)` — Türkçe adlı yapı alan
erişimi (k.x+k.y) + Türkçe adlı recursive çeşit ağacı → exit 42. ASCII tip adları
DEĞİŞMEDİ (quote yok). test_llvm 210→211 (+Türkçe tip-adı E2E). Tam regresyon:
lexer 103, parser 107, tip_kontrol 174, snapshot 50, linear 57, drf 39, capability
40, ast/arena/sembol/tip/sabitsure. 0 ASan. Temiz rebuild 0 uyarı.
Fikstür: test/snapshots/turkce_tip_adi.kem.

---

## D-020 — Çapraz-modül payload + recursive çeşit (modüler AST) (2026-06-13)

**Self-hosting deseni:** Compiler'ın AST'si kendi modülünde yaşamalı (`genel çeşit
Ifade { Sayi(tam64), Topla(&Ifade,&Ifade) }` modül `ifd`'de), parser/codegen
modüllerince import edilmeli. D-018 payload çeşit'i MODÜL-İÇİ doğrulamıştı; bu
adım çapraz-modülü kapatır (D-018 follow-up'ı).

**Düzeltilen gap'ler:**
1. **Codegen nitelikli yapıcı (`m::Cesit::V(args)`):** `cesit_kayit_yoldan(g, sol)`
   — çeşit'i sol-yoldan çözer (sol TANIMLAYICI=`Renk` ya da YOL=`m::Renk` →
   sag_ad'den, düz IR-ad uzayı D-011). DUGUM_YOL (bare) + DUGUM_CAGRI (yapıcı)
   yolları artık YOL sol kabul eder (önceden yalnız TANIMLAYICI → i32 fallback
   miscompile).
2. **Tip-kontrol payload-tip çözümü (recursive/modül-yerel):** Recursive çeşit'in
   payload tipi `&Ifade` dışarıdan (caller scope) çözülürken T011/M004 veriyordu
   (Ifade modül-yerel). `ast_tip_to_bilgi` DUGUM_TIP_BASIT artık çözülemezse
   `yapi_sembol_capraz_bul` (D1) ile yüklü modüllerde düz adla arar — alan-erişimi
   çapraz-modül çözümüyle simetrik.

**Doğrulama:** Modüler recursive AST (`kullan ifd; ifd::Ifade::Topla(&a,&b);
ifd::hesapla(&kök)`) — (3+4)*6 = 42, HEM --check HEM E2E. Çapraz-modül payloadsuz
çeşit (Renk) + primitive-payload çeşit (Ifade::Ikili) de doğrulandı.

**Kapsam/sınırlar:** Nitelikli payload DESENİ (`eşleş` içinde `m::Cesit::V(a,b)`)
denenmedi — match genelde modül-içi (genel işlev). Çapraz-modül match nitelikli
desen gerekirse follow-up. Generic çeşit hâlâ ayrı.

**Testler:** test_llvm 211→212 (+çapraz-modül payload+recursive cesit E2E).
tip_kontrol 174, parser 107, snapshot 50, linear 57, drf 39, capability 40,
ast/arena/sembol. 0 ASan. stdlib --check yeşil. Temiz rebuild 0 uyarı.
Fikstür: test/moduller/{ifd,ana_ifd}.kem.

---

## D-021 — Sayı literal bağlam-bağımlı: tipli tamsayı + literal ikili op (ergonomi) (2026-06-13)

**Sorun (çeşit edge-probe sırasında bulundu):** `değişken x: tam64; x + 1` → T001
("ikili operator iki tarafi ayni tip"). Literal `1` varsayılan tam32, x tam64 →
uyuşmazlık. Her non-tam32 tamsayı aritmetiğinde `(1 olarak tam64)` zorunluluğu —
yaygın ergonomi pürüzü (çeşit/AST kodu tam64 sayaç/değer kullanır). codegen ZATEN
genişletiyordu; yalnız tip-kontrol katıydı. CLAUDE.md zaten "Sayı literal:
Context-dependent" diyor — IKILI op'ta uygulanmamıştı.

**Çözüm [ETKİ]:** DUGUM_IKILI'de sol/sag belirlendikten sonra: bir taraf TİPSİZ
tamsayı LİTERALİ (DUGUM_TAM) + diğer taraf TİPLİ tamsayı + tipler farklı ise,
literali karşı tarafın tipinde yeniden çıkar (`tip_belirle_beklenen`). Yalnız
şu anda HATA veren durumu gevşetir:
- Explicit cast (`… olarak tamX`) DUGUM_TAM DEĞİL → etkilenmez.
- tam32 + literal zaten eşit → etkilenmez.
- **sabitsüre/vektör HARİÇ** (taint/lane yayılımı kendi kurallarına sahip —
  tip_tamsayi_mi sabitsüre'yi iç tipe açtığı için ilk denemede S2 testleri
  kırıldı; guard eklendi).

**Doğrulama:** `tam64 x; x+1`, `x>8`, `x*2` artık --check geçer + doğru değer.
Tam regresyon: test_llvm 213→214 (+tam64 literal bağlam E2E), tip_kontrol 174,
sabitsure 39 (guard sonrası), simd 30, simd_llvm 5, wcet 35, mmio 23, lexer 103,
parser 107, linear 57, drf 39, capability 40, snapshot 50, arena/ast/tip/sembol.
0 ASan. stdlib --check yeşil. Temiz rebuild 0 uyarı.

---

## D-022 — Self-hosting doğrulama eseri: ağaç-yürüyen yorumlayıcı (saf KEMGU) (2026-06-13)

**Karar [ETKİ: düşük — yalnız örnek + test, derleyici değişmedi]:** Bu oturumda
eklenen özyinelemeli payload çeşit yığınını GERÇEK bir programda birden zorlamak
için, küçük bir ifade dilinin ağaç-yürüyen yorumlayıcısı saf KEMGU ile yazıldı
(`test/ornekler/11_yorumlayici.kem`) ve E2E regresyon testine bağlandı.

**Gerekçe:** Generic çeşit'in (sıradaki büyük aday) codegen monomorfizasyonu
per-T layout gerektiriyor (yeni instantiation-izleme + mangled struct emisyonu;
yapıcı/eşleş/annotasyon sitelerinde) — "her commit yeşil + E2E" disiplini altında
tek hamlede temiz inmesi yüksek riskli, tip-kontrol-only yarım özellik olurdu.
Bunun yerine SIFIR derleyici-regresyon riski olan, shipped çeşit işini gerçekçi
yük altında doğrulayan ve bir sonraki özelliği KANITLA gerekçelendiren eseri seçtim.

**Ne kanıtlıyor:** KEMGU kendi dilinin küçük bir klonunu kendi tip sistemiyle
ifade edip değerlendirebiliyor — HEM AST (`çeşit Ifade`, 8 varyant: Sabit,
Degisken, Topla, Carp, Cikar, Kucuk, Kosul/3-payload, Bagla/karışık-tip) HEM
leksik ORTAM (`çeşit Cevre` immutable assoc-list) özyinelemeli payload çeşit.
Ortam özyineli çağrılar boyunca elden ele aktarılır; `eşleş` kolunda yerel çeşit
kurup `&referansını` alma (`Cevre::Bag(yuva,v,ortam)` → `&yeni`); nested match +
koşul + karşılaştırma. Program: `bağla x=6 içinde (x<10 ? x*7 : 0)` = **42**
(--check ✓ + E2E exit 42 ✓). `x*7` bu oturumun D-021 literal-bağlam düzeltmesini
de doğrular (tam64 değer * literal).

**Kapsam/sınırlar:** Değişken erişimi tam sayı YUVA kimliğiyle (slot id) — `metin`
eşitliği intrinsic'i yok, isimle arama V2. Tek dosya. Yorumlayıcı kapsamı: tam
sayı aritmetik + karşılaştırma + koşul + let; tip/closure/fonksiyon-değer yok.

**Sıradaki büyük adayı kanıtlıyor:** Bu eser genel container ihtiyacını (örn.
`Opsiyonel<T>`, `Liste<çeşit>`) somutlaştırıyor → **generic çeşit** ve **gerçek
string/koleksiyon stdlib** self-hosting'in net ön-koşulları olarak doğrulandı.

**Testler:** test_llvm 214→215 ([139] Yorumlayici E2E). Diğer tüm suite'ler
değişmedi (derleyici dokunulmadı). 0 ASan. Temiz rebuild 0 uyarı. Hiçbir test
`ornekler/` dizinini enumerate etmiyor (yeni dosya izole).

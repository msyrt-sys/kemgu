# Bölüm 4 — EVREN incelemesi

- commit: `c957897d661f76ad1e81b858d5bc6ca2a43cce5d`
- model: qwen3.8-flash-next · bitiş: stop · süre: 838 sn · usage: {'prompt_tokens': 19799, 'total_tokens': 33086, 'completion_tokens': 13287, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 17600}, 'completion_tokens_details': {'reasoning_tokens': 12157}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'qwen/qwen3.8-flash-next'}}



## Bulgular

- [ŞÜPHE] `drivers/ahci/cekirdek.kem:~60` — Yorum "CR ve FR salt okunurdur, yazma değerine taşınmaz" diyor ama maske `4294918142` (= `~0xC001` = `0xFFFF3FFE`) yalnız bit 0, 14, 15'i temizler; CR (bit 1) ve FR (bit 2) yazma değerine taşınır. Donanım salt-okunur bitleri yok saydığından işlevsel etki yok, ancak yorum ile maske uyumsuz. Alıntı: `s.cmd=v&4294918142; s.adim=62; ver GcIstek::Yaz32(p(s,24),s.cmd);`

- [ŞÜPHE] `drivers/nvme/cekirdek.kem:~142` — `emir_baslat(s)` çağrısı diff'te görünmüyor (değişmemiş kısımda). Yorum "yeni gönderim yeni CID kullanır" diyor; doğrulama kahin ölçümleriyle (1/4 başarısız) dolaylı olarak yapılır, ancak `emir_baslat`'ın gerçekten CID ilerlettiği ve PRP'leri yeniden kurduğu diff'ten kanıtlanamıyor. Alıntı: `ver emir_baslat(s);`

## Karşılanan gereksinimler

- **AHCI: hata → port durdur → temizle → COMRESET gerekirse → yeniden başlat → ≤3 yeniden dene → açık hata (LBA + TFD + PxIS) → kabuk devam eder → sonraki geçerli okuma çalışır** → `drivers/ahci/cekirdek.kem` `port_kurtar` (adım 60–73) + `disk_hata_kahin.py` (kalıcı: "KABUK HATA ahci LBA … TFD … PxIS …" doğrulanır, TFD ERR biti ve PxIS.TFES assertion; sonraki dosya okunur).

- **NVMe: SC/SCT → sınırlı yeniden deneme + açık hata** → `drivers/nvme/cekirdek.kem` adım 32 + `disk_hata_kahin.py` ("KABUK HATA nvme LBA … SC … SCT …" regex match, SC/SCT non-zero assertion).

- **USB: CSW → REQUEST SENSE → sınırlı yeniden deneme + açık hata** → `drivers/usb/depo.kem` retry path + `disk_hata_kahin.py` `scsi_req_parsed` trace sayacı ile REQUEST SENSE = başarısız okuma sayısı doğrulanır.

- **QEMU blkdebug: `once="on"` tek seferlik (okuma başarılı, içerik host imajıyla birebir) + `once="off"` kalıcı (açık hata, kabuk devam, başka dosya okunur)** → `disk_hata_kahin.py` `once = "on"/"off"`, tek seferlik: `assert b == fat.get('/merhaba.txt')+b'\n'`, kalıcı: regex error + `yanitlar[1] == beklenen`.

- **Kâhin `calistir_bm_x86_disk_hata` (AHCI + NVMe + USB depo; her biri tek + kalıcı)** → Makefile hedefi + `disk_hata_kahin.py` döngüsü 3 taşıyıcı × 2 senaryo.

- **Kâhin bağımsızlığı** → `disk_hata_kahin.py`: `query-blockstats` (QMP, host), `Fat` imaj okuyucu, `fsck.fat`, `scsi_req_parsed` trace; konuk hata mesajı yalnız doğrulama hedefi olarak kullanılır.

- **Sabotaj 1: yeniden deneme = 0 → tek seferlik KIRMIZI** → `codex16_bulgular.md` "Sıfır yeniden denemede… rc 1 verdi". `disk_yeniden_dene(0)` → `0 < 0` → `yanlış` → anında hata → tek seferlik dosya okunamaz → kahin `assert b == fat.get(…)` başarısız.

- **Sabotaj 2: port yeniden başlatma atlanır → sonraki geçerli okuma da başarısız → KIRMIZI** → `codex16_bulgular.md` "Port yeniden başlatma yazımını atlayan kopyada… sektör (4102) okunamadı… rc 1". Kahin `yanitlar[1] == beklenen` assertion'ı başarısız olur.

- **Yasak dosya ihlali yok** → Değişen tüm dosyalar `drivers/**`, `test/**`, `tests/**`, `Makefile`, `otomasyon/chatgpt/` içinde.

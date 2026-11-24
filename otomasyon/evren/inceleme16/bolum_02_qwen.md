# Bölüm 2 — EVREN incelemesi

- commit: `ff31a105f0d30d9d8ed52eb7f9d4604c59c448e3`
- model: qwen3.8-flash-next · bitiş: stop · süre: 1346 sn · usage: {'prompt_tokens': 24928, 'total_tokens': 47563, 'completion_tokens': 22635, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 22400}, 'completion_tokens_details': {'reasoning_tokens': 21687}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'qwen/qwen3.8-flash-next'}}



## Bulgular

- [ŞÜPHE] `runtime/mimari/x86_64/kem_bellek_harita.kem:2` — `bar_boyu` ve `bar_boyu64` işlevleri `drivers/pci/yapilandirma` modülünden içe aktarılıyor, ancak bu modüle bu commit'te hiçbir değişiklik yok. Eğer bu iki işlev önceki bir commit'te eklenmediyse derleme başarısız olur; ilerleme notu derlemenin geçtiğini iddia ediyor, ancak diff'te tanımları görülemiyor. Alıntı: `kullan drivers::pci::yapilandirma::{ bar_boyu, bar_boyu64 };`

## Karşılanan gereksinimler

- **H1f (port oku/yaz 16/32)** → `runtime/mimari/x86_64/kem_uart.kem` yeni `kem_port_oku16/32`, `kem_port_yaz16/32` tanımlar; `kem_pci_hw.kem` eski yerel `kem_port_yaz32/oku32` tanımlarını silip ortak çifti kullanır. Host birim testi: `tests/drivers/pci/port16_test.kem`. QEMU doğrulaması: `kd_pci_tani` → `PORT16` satırı, oracle'da `assert int(re.search(r'^PORT16 (\d+)',s,re.M)[1]) == pci['00:00.0'][0]`.

- **H7a (MCFG + ECAM + CF8/CFC yedek)** → `drivers/acpi/mcfg.kem` (sans-IO, pencere ayırıştırma ve adres hesaplama). ECAM penceresi `uefi_ecam_kur` ile UC eşlenir; `kem_pci_oku16/32/yaz16/32` ECAM adresi sıfır ise CF8/CFC'ye düşer. İki yol karşılaştırması: `kd_pci_tani` → `CFG` satırları; oracle'da `assert all(int(o)==i*4 and v==e ...)` ve `assert c.mem(taban,256) == ilk` (QMP fiziksel bellek okumasıyla bağımsız). 256 üstü ECAM: `kd_pci_genis` zincir yürür, oracle'da `c.mem(taban + (bdf << 12), 4096)` ile bağımsız doğrulanır.

- **H7b (köprü arkası numaralandırma)** → `kem_pci_yol_tara` özyinelemeli tarama, ikincil/alt yazmaçlarını yazar (`kem_pci_yaz32(kopru,24,...)`). QEMU topolojileri: `pci_topolojisi='kok'` (iki pcie-root-port) ve `'kopru'` (pcie-pci-bridge + pci-bridge) `dalga12_kahin.py`'de tanımlı. Kâhin: `dugumler(q.call('query-pci')[0]['devices'])` ile QMP ağacı düzleştirilir; `assert pci == ilk_pci == pci_kumesi(q)` konuk envanteriyle birebir. Köprü numaraları: `assert kopruler[bdf] == bus['number'] | bus['secondary']<<8 | bus['subordinate']<<16`. Arkadaki disk okuması: `c.run(f'cat /diskler/{aygit}0p1/merhaba.txt', Fat(part.read()).get('/merhaba.txt') + b'\n')`.

- **H7c (BAR boyutlandırma)** → `pci_bar_olc`: tümüne-1-yaz (`4294967295`), geri-oku (`kem_pci_oku32`), geri-yükle (`kem_pci_yaz32(bdf,ofs,alt)`). 64 bit BAR: `(alt&7)==4` koşulu, üst yarısı okunur ve `adr|(ust<<32)` ile birleştirilir; boy `bar_boyu64(((yuk<<32)|m) ...

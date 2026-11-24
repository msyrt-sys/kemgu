# Bölüm 10 — EVREN incelemesi

- commit: `02760a0ee33b9bd865204737f3a4a9dc9530f845`
- model: deepseek-v4.1-flash · bitiş: stop · süre: 719 sn · usage: {'prompt_tokens': 11873, 'total_tokens': 24122, 'completion_tokens': 12249, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 9600}, 'completion_tokens_details': {'reasoning_tokens': 11398}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'qwen/qwen3.8-flash-next'}}

(birincil model hata verdi: HTTPError; yedek model)



## Bulgular

- [EKSİK] `otomasyon/chatgpt/codex16_ilerleme.md:58` — MMIO sabotajı (sıradan bellek döngüsü eklenince kapı kırmızı) yalnız ilerleme notunda iddia edilmiş; diff'te veya `codex16_bulgular.md`'de sonucu gösteren hiçbir artefakt (çıktı satırı, ayrı test dosyası, bulgu satırı) yok. Alıntı: `"Sıradan bayt toplamı döngüsü listeye eklenince volatile kapısı 1 ile döndü"` — ancak `codex16_bulgular.md`'de sabotaj sonucu kaydı bulunmuyor.

- [ŞÜPHE] `tools/codex/mmio_volatile.py:117` — `asm_kanit` içindeki bellek erişimi regex'i negatif lookahead ile `%rip`/`%rsp`'yi hariç tutuyor (`%(?!r(?:sp|ip))`). Konumdan bağımsız kod üretimi (PIC/PIE) durumunda MMIO adresine RIP-göreli erişim `movl (%rip)` biçiminde olabilir ve regex'i geçmez. Hedef `-ffreestanding -nostdlib` olduğundan büyük olasılıkla mutlak adres kullanılır, ancak `k.elf` diff'te görünmediği için teyit edilemiyor. Alıntı: `rf'^(?:mov{ek}|...)\\s+[^,\\n]*\\(%(?!r(?:sp|ip))'`

## Karşılanan gereksinimler

- H10g TX sınırı (16550) → `runtime/kdl_runtime_uart_16550.c`: `kalan = 100000U` döngüsü, `--kalan == 0U` ile bayt düşürme, `kdl_uart_16550_tx_kayip` artışı.
- H10g TX sınırı (PL011) → `runtime/kdl_runtime_uart_pl011.c`: aynı yapı, `kdl_uart_pl011_tx_kayip`.
- H10g RX davranışı değişmez → diff yalnız `putc` fonksiyonlarını değiştiriyor; `get`/RX yolu yok.
- H10g host birim testi "THRE hiç gelmiyor" → `tests/drivers/uart/tx_sinir.c`: `BEKLE = UINT32_MAX` ile sonsuz meşgul; `Makefile` hedeflerinde `timeout=3` ile zaman aşımı yakalanıyor.
- H10g envanter tablosu güncelleme → `codex16_bulgular.md`: sınırlandırılmış TX, açık uçlu RX (gerekçeli), MSI, disk, hub, S5 satırları.
- H1h kapı iki derleyici → `test/bm_x86_mmio_volatile_harness.sh`: `for derleyici in build/kemgu build/codegen` döngüsü.
- H1h IR + objdump denetimi → `tools/codex/mmio_volatile.py`: `ir_kanit` (ham + O2) ve `asm_kanit` (objdump) üç aşamalı.
- H1h beklenen-kırmızı değil bulgu yaklaşımı → `mmio_volatile.py:135` `raise SystemExit` ile kapı kırmızı bırakıyor, işlev listeden çıkarılmıyor.
- `calistir_mmio_volatile` → Makefile'da tanımlı ve `test_tumu`'na bağlı.
- Sabotaj mekanizması UART → limit kaldırılırsa `UINT32_MAX` mock sonsuz döngü → `timeout=3` → Python exception → make fail. Tasarım doğru.

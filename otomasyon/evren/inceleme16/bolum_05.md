# Bölüm 5 — EVREN incelemesi

- commit: `8d3211adfbb2a00ca5c328b0df4e7be848c07760`
- model: deepseek-v4.1-flash · bitiş: stop · süre: 904 sn · usage: {'prompt_tokens': 35622, 'total_tokens': 50233, 'completion_tokens': 14611, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 33600}, 'completion_tokens_details': {'reasoning_tokens': 13918}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'qwen/qwen3.8-flash-next'}}

(birincil model hata verdi: HTTPError; yedek model)



## Bulgular

- [ŞÜPHE] `drivers/usb/hub.kem:hub_hiz` — Bit 11:10 pozisyonu xHCI PORTSC hız kodlamasına karşılık gelir; USB 2.0 sınıf GetPortStatus yanıtında Port Speed bitleri farklı konumdadır. QEMU hub'ı yalnızca full-speed bildirdiği için testlerde görünmez, ancak gerçek donanımda yüksek hız yanlış sınıflandırılabilir. Alıntı: `eğer (durum&1536) == 1024 { ver 3; }` / `eğer (durum&1536) == 512 { ver 2; }`.

## Karşılanan gereksinimler

- Hub sınıfı sürücüsü (sans-IO, tanımlayıcı, port gücü/durum/değişim/sıfırlama, hız) → `drivers/usb/hub.kem` (yeni dosya: `hub_tanim`, `hub_port_iste`, `hub_hiz`, `hub_tanim_iste`, `hub_rota`)
- xHCI'de route string + kök port + hub yuvası + Slot Context (Route String, Parent Hub Slot ID, Parent Port Number, TT) → `drivers/usb/xhci_cekirdek.kem` (`aygit_baglami`, `xhci_alt_baslat`, `xhci_alt_birak`)
- QEMU tek/katmanlı hub arkasında kbd/mouse/storage + iki seviyeli hub → `tests/qemu/dalga12_kahin.py` (`hub_derinligi` parametresi), `tests/qemu/usb_hub_kahin.py` (`derinlik` 1 ve 2)
- QMP `device_add`/`device_del` ile hub arkasında takma/çıkarma, kabuk asılmaz → `tests/qemu/usb_hub_kahin.py` (`q.call('device_add',…)`, `q.call('device_del',…)`, `wait_for` ile sınırlı bekleme)
- Kâhin `calistir_bm_x86_usb_hub`, HMP `info usb` + xHCI çıkış bağlamı (QMP bellek okuma) bağımsız doğrulama → `tests/qemu/usb_hub_kahin.py` (`hmp_agaci`, `envanter`, DCBAA + slot context okuma), `Makefile` (yeni hedef)
- Sabotaj route string → Kırmızı → `codex16_bulgular.md`, `codex16_ilerleme.md` (xHCI tamamlanma kodu 5)
- Sabotaj port değişim biti temizlenmemesi → Kırmızı → `codex16_bulgular.md`, `codex16_ilerleme.md` (`KABUK HATA USB hub port degisimi onaylanmadi`)
- Dosya okuma/yazma hub arkasındaki diskte → `usb_hub_kahin.py` (`cat`, `yaz`, `sync`, Fat kontrolü `fsck`)

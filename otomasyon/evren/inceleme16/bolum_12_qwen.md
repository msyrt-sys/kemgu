# Bölüm 12 — EVREN incelemesi

- commit: `15624d77a661ffd50f8182c6cb5793d08c2116af`
- model: qwen3.8-flash-next · bitiş: stop · süre: 980 sn · usage: {'prompt_tokens': 29308, 'total_tokens': 46075, 'completion_tokens': 16767, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 27200}, 'completion_tokens_details': {'reasoning_tokens': 15722}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'qwen/qwen3.8-flash-next'}}



## Bulgular

Yok. Diff'te gereksinim eksikliği, sabotaj sapması, kâhin zayıflığı, sınırsız bekleme, izin dışı dosya veya mantık hatası tespit edilmedi.

## Karşılanan gereksinimler

- **`bilgi` komutu tüm alanları** → `test/ornekler/kem_os_x86_kabuk.kem` `kd_bilgi`: `CALISMA ms`, `CERCEVE bos toplam`, `HEAP bayt`, `SUREC etkin`, `GIRDI <klavye> <fare> <düzen>`, `KESME …` satırları, `BOS_BAYT <disk> <bayt>`.
- **`duzenle` arama/arama-değiştirme/geri-al/yinele/satıra-git** → `drivers/duzenle/islem.kem`: `du_bul` (ileri/geri, `katla` ile Türkçe harf katlama via `bul_harf`), `du_degistir` (tek/tümü, `kul_onay` ile onay), `du_geri_al` (64 adımlık halka), `du_goto`.
- **Geri-al sınırı belgelenmiş hata** → `test/kullanici/duzenle.kem`: `ileti="  Geçmiş sınırı"`.
- **`dosyalar` kopyala/taşı/sil/dizin, çapraz bağlama, ESP hata** → `test/kullanici/dosyalar.kem` `ge_islem`; `sis_kopyala`(71)/`sis_tasi`(72) `ksv_aktar` ile `yerel=yanlış` (çapraz bağlama açık), `KX_SIS_YENIDEN_ADLANDIR` `yerel=doğru` (sınırlı). `eğer r == -30 { g.ileti="Salt okunur"; }`.
- **Kabuk `kopyala`/`tasi` aynı VFS (D-407)** → `test/ornekler/kem_os_x86_kabuk.kem`: `bm_esit(cmd,"kopyala")→"cp"`, `bm_esit(cmd,"tasi")→"mv"` → mevcut `kd_kopyala`; `dosyalar` aynı çağrıları syscall 71/72 üzerinden kullanır.
- **Host birim testi (saf mantık)** → `tools/codex/arac16_test.kem`: `du_duzelt`/`du_geri_al`/`du_bul`/`du_degistir`/`du_goto`/`gezgin_hedef`/`cerceve_bos` testleri.
- **QEMU kâhini (seri + ekran + imaj/fsck)** → `tests/qemu/arac16_kahin.py`: `cerceveler()` QEMU fiziksel bellek + CR3 yürüyüşüyle bağımsız çerçeve hesaplar; `bos_bayt()` host FAT'tan bağımsız sayar; `Fat(part.read()).get(...)` + `fsck()` imaj doğrulaması; `du_ekran`/`piksel` GOP karşılaştırması.
- **Sabotaj (a) geri-al bir adım atla → KIRMIZI** → bulgular: "önceki adımı da atlayan mutasyon, GOP karşılaştırmasında 329 pikseli farklı kıldı"; kâhin `du_ekran` beklentisi farklılaşır.
- **Sabotaj (b) çapraz bağlama son kısmi kümeyi yazma → KIRMIZI** → bulgular: "1.264 yerine 1.024 bayt"; kâhin `assert yazilan == VERI` başarısız olur.
- **Sabotaj (c) `bilgi` boş çerçeve bir kaydır → KIRMIZI** → bulgular: "47.834 karşısında 47.835"; kâhin `assert gercek == (bos,toplam)` başarısız olur; `bos` QMP + CR3 yürüyüşüyle bağımsız hesaplanır.
- **Yeni syscall 71/72 ABI belgesi** → `belgeler/KEMGU_OS_Syscall_ABI.md` tablosuna eklenmiş; `tests/qemu/surec_abi.py` kapısı Makefile hedefinde çağrılır.
- **İzin dışı dosya yok** → değişiklik yalnızca izinli dosyalarda.

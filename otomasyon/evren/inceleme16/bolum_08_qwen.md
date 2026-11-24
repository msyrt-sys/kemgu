# Bölüm 8 — EVREN incelemesi

- commit: `9f06eaed278b428ad708e6f0db5a90dc66d5214c`
- model: qwen3.8-flash-next · bitiş: stop · süre: 1074 sn · usage: {'prompt_tokens': 12144, 'total_tokens': 29814, 'completion_tokens': 17670, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 9600}, 'completion_tokens_details': {'reasoning_tokens': 16471}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'qwen/qwen3.8-flash-next'}}

(birincil model hata verdi: HTTPError; yedek model)



## Bulgular

- [KUSUR] `tests/qemu/pat_kahin.py:30` — Büyük sayfa (2 MiB PDE) için PAT biti yanlış konumdan okunuyor. `bit = 7 if kayma == 12 else 12` ifadesi büyük sayfa için bit 12'yi okuyor; x86-64'te PAT bit 7 hem küçük hem büyük sayfalarda geçerlidir, bit 12 büyük sayfa girişinde ayrılmış (reserved, sıfır) alandadır. Alıntı: `bit = 7 if kayma == 12 else 12`
- [KUSUR] `tests/qemu/pat_kahin.py:31` — Önceki bulgu nedeniyle büyük sayfa PAT indeksi `PAT*4+PCD*2+PWT` yerine `0*4+PCD*2+PWT` hesaplanır; LAPIC ve PCI BAR sayfalarında (PCD=1, PWT=1, PAT=1, yani gerçek indeks 7→WC) oracle indeks 3'e çözer ve UC bulur. Alıntı: `indeks = (g >> bit & 1) * 4 + (g >> 4 & 1) * 2 + (g >> 3 & 1)`
- [KUSUR] `runtime/mimari/x86_64/kem_bellek_harita.kem:164–170` (diff satırları `pat_gop_kur`) — PAT girişi 7'ye WC yazılıyor, ancak mevcut GOP-dışı MMIO büyük sayfaları (PCD=1, PWT=1, PAT=1 → aynı indeks 7) yeniden eşlenmeden bırakılıyor. Eski `bm_uefi.py`'deki `assert g&0x9b==0x9b` denetimi bu sayfaların bit 7'sinin kurulu olduğunu kanıtlıyor; MSR değişikliğinden sonra bu sayfalar WC'ye düşüyor. Kâhin 2. bulgudaki bit-ofset hatası sayesinde bunu yakalayamıyor. Alıntı: `değişken yeni: tam64 = (eski&72057594037927935)|72057594037927936;`
- [ŞÜPHE] `runtime/mimari/x86_64/kem_bellek_harita.kem:152` (diff, `pat_gop_tablo`) — `((buyuk&4096)>>5)` ifadesi büyük sayfa PDE'sinin bit 12'sini (ayrılmış, daima 0) bit 7'ye taşıyor; sonuç hep 0. PAT biti korunmak istenmişse `(buyuk&128)` olmalıydı. Fonksiyonel etki yok (sonraki döngü `|152` ile PAT'ı kuruyor), ancak niyet belirsiz. Alıntı: `değişken bayrak: tam64 = (buyuk&3967)|((buyuk&4096)>>5)|((buyuk>>63)<<63);`

## Karşılanan gereksinimler

- PAT MSR programlama (girdi 7 WC, diğerleri değişmez) → `pat_gop_kur` satır `(eski&72057594037927935)|72057594037927936` yalnız bit 56'yı kurar; belge değeri `otomasyon/chatgpt/codex16_bulgular.md`
- PTE PAT/PCD/PWT kurulumu → `kem_mmio_yaz64(yuva,g|152)` (152 = 0x98 → PCD|PWT|PAT)
- TLB boşaltma → `kxm_cr3_yaz(kxm_cr3_oku())`
- Konuk raporu (PAT_MSR, PAT_ILK, PAT_SON, PAT_RAM, PAT_MMIO) → `pat_gop_kur` sonunda `kap_dok` çağrıları
- Host bağımsız sayfa tablosu yürüyüşü → `pat_kahin.py` `yaprak()` QMP `dump` ile CR3'ten yürür (ancak bit-ofset hatası nedeniyle büyük sayfalarda yanıltıcı; bkz. bulgu 1)
- Ölçüm ve fiziksel PC notu → `PAT_TSC` satırı + `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md` GOP yazma süresi bölümü
- Kâhin `calistir_bm_x86_pat` → Makefile yeni hedef + `test/bm_x86_pat_harness.sh`
- Sabotaj (PAT biti var, MSR WC yok → UC çözülür → KIRMIZI) → `pat_kahin.py` `bellek_tipi` beklenen=1, tip=0 → assert hatası; ilerleme notunda rc 1 doğrulanmış
- İzin dışı dosya: yok
- Yasak dizin (`src/`, `selfhost/`, `boot/efi/`, diğer `linker/`): dokunulmamış

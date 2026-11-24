# Bölüm 11 — EVREN incelemesi

- commit: `f206ac11ff7bb4ae3244f3843dfea4112c540425`
- model: deepseek-v4.1-flash · bitiş: stop · süre: 413 sn · usage: {'prompt_tokens': 11866, 'total_tokens': 18086, 'completion_tokens': 6220, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 11840}, 'completion_tokens_details': {'reasoning_tokens': 5674}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'deepseek/deepseek-v4.1-flash'}}

## Bulgular
- [EKSİK] `test/olcum/birim_uret.py:25` — Görevde istenen sabotaj (üreticiye yapay katlanan yapı, ör. aynı işlevin 10 katı) diff’te uygulanmamış; üretici yalnız normal `islev` üretiyor ve sabotaşı açan parametre/bayrak yok. Alıntı: `def islev(no):` … `satirlar.append(f'    r=r^aygit_{no}_isle(&değişken d,{emir});')`
- [KUSUR] `test/olcum/derleme.py:94-98` — Eşik aşılınca `rapor_yaz` çağrılmadan `SystemExit` yükseltiliyor; bu nedenle KIRMIZI durumda ölçüm değerleri `otomasyon/chatgpt/codex16_bulgular.md` dosyasına yazılmıyor. Alıntı: `if hatalar:` `raise SystemExit('DERLEME RED\n'+'\n'.join(hatalar))` ardından `rapor_yaz(a.rapor,metin)`

## Karşılanan gereksinimler
- Sentetik KEMGU birimi üretimi → `test/olcum/birim_uret.py`; sürücü benzeri `Aygit`, işlevler, `eşleş`, `Dizi` kullanılıyor.
- İki derleyiciyle `--llvm` ölçümü, süre, IR satırı, en büyük işlev IR boyu ve `/usr/bin/time -v` tepe RSS → `test/olcum/derleme.py:olc`.
- Birleşik kabuk birimi ölçümü → `test/olcum/derleme_harness.sh` içinde `comb.kem` kullanımı.
- Makinece okunur dosya → `test/olcum/derleme.py` içinde `olcum.json` yazımı.
- Eşik dosyası ve ~3 kat gerekçesi → `test/olcum/esikler.json` ve `codex16_bulgular.md` metni.

# TODO — Driver'ı origin/main'e entegre + Aşama 5 fixpoint (D-086)

**Bağlam:** İlk Aşama 4 driver (commit 20b5408, [D-082]) **bayat** D-081 codegen.kem üzerine
kurulmuştu + numara çakışması (gerçek D-082 = CG8 dizi, origin/main'de). Branch origin/main'e
(9f66dc9; D-085 fixpoint dahil) sıfırlandı (backup tag: `asama4-d082-backup`). Driver **yeni**
codegen.kem (2659 satır; CG8 dizi + CG7d + CG9a alloca-hoist + self-compile fixpoint) üzerine
**yeniden** uygulanıyor, **D-086** numarasıyla.

**Doğrulanan gerçekler:** checker.kem origin/main==D-081 (DEĞİŞMEMİŞ → port reçetesi geçerli);
çakışma = sadece `karsilastirma_mi` + `yapi_var_mi`; `duz_yaz` yeni codegen'de yok (port);
baseline yeşil (codegen_diff 56/56, bootstrap FIXPOINT lexer46+parser46+codegen stage1==stage2).

**KRİTİK YENİ HEDEF:** driver-ify edilince `calistir_codegen_bootstrap` artık **driver'ı**
derler → geçerse = **Aşama 5 driver fixpoint** (self-host codegen, checker bloğu dahil
codegen.kem'i kendi kendine üretir). Bu, ilk turda olmayan ekstra kanıt.

## Maddeler

- [DONE] **M1** — Git arkeoloji + baseline + analiz. origin/main kanonik; reset+rebuild; checker özdeş; çakışma 2; baseline codegen_diff 56/56 + bootstrap FIXPOINT ✓. ✓
- [DONE] **M2** — Ayr union struct (CG8/CG9 + checker alanları) + ayr_olustur union. `--check` temiz; `--llvm` 17494 satır. ✓
- [DONE] **M3** — checker bloğu port (3775 satır, sıfır duplicate); yapi_var_mi drop, karsilastirma_mi→sirali_kars_mi. `--check` temiz. ✓
- [DONE] **M4** — token_dump + dispatch main. `--check` temiz. ✓
- [DONE] **M5** — `build/kemgu_self.exe` (270KB; smoke 42). ✓
- [DONE] **M6** — Driver 4-mod (C-derlenmiş): TOKEN 22/22, PARSE 12/12, CHECK 48/48, LLVM 56/56. ✓
- [DONE] **M7** — **FIXPOINT:** bootstrap driver-ify codegen.kem ile stage1==stage2 (21728 satır) + lexer46/parser46. EK: self-host-derlenmiş driver (kemgu_self2) 4 modda da C-oracle eşleşir. ✓
- [DONE] **M8** — Harness güçlendirildi (C-built + self-host driver + fixpoint); Makefile kemgu_self/calistir_self_driver + test_tumu/.PHONY. `make calistir_self_driver` ✓. ✓
- [DONE] **M9** — `make test_tumu` exit 0, "Tum testler gecti!", sıfır regresyon (bootstrap fixpoint + driver harness dahil). ✓
- [DONE] **M10** — Docs: DECISIONS_LOG D-086 + CLAUDE.md Aşama durum + hafıza güncellendi. ✓
- [DONE] **M11** — Türkçe git commit [D-086]. ✓
- [DONE] **M12** — Final: tüm maddeler DONE + build/test (fixpoint dahil) geçti onayı. ✓

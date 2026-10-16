## İş 12 — D1: `--mimari x86_64-none` → `x86_64-unknown-none-elf` (C + self-host parite)

Sorun: `--mimari x86_64` `x86_64-pc-windows-gnu` yayar. x86 bare-metal hedefleri
bugün clang `-target x86_64-unknown-none -Wno-override-module` ile ezerek
çalışıyor; modül üçlüsüne saygı duyan araç (llc, LTO) Win64 çağrı sözleşmesi
üretir, `start_x86_64.S` SysV varsayar → sessiz yanlış yazmaç.
Karar: `x86_64` DEĞİŞMEZ (Windows CI ona bağlı). YENİ değer `x86_64-none`:
mimari ETİKETİ `"x86_64"` (AS001 asm etiketleri aynen eşleşsin), üçlü
`"x86_64-unknown-none-elf"`. C ve self-host BİREBİR (D-407).

### Değişiklikler (6 madde, hepsi birebir çapalı)
1. `src/ana.c` — çapa:
   `            } else if (strcmp(m, "x86_64") == 0) {` + bir sonraki satır
   `                llvm_hedef_ayarla("x86_64", "x86_64-pc-windows-gnu");`
   YENI: bu iki satır AYNEN + ardına
   `            } else if (strcmp(m, "x86_64-none") == 0) {` /
   `                llvm_hedef_ayarla("x86_64", "x86_64-unknown-none-elf");`
   (D1 yorum satırıyla: bare-metal x86, SysV, `start_x86_64.S` ile uyumlu).
2. `src/ana.c` — üç dizgide `(arm64|x86_64)` → `(arm64|x86_64|x86_64-none)`:
   `"--mimari: mimari argumani gerekli (arm64|x86_64)\n"`,
   `"--mimari: bilinmeyen mimari '%s' (arm64|x86_64)\n"`, ve kullanım satırı
   `"  --mimari M   Hedef mimari: arm64|x86_64 (satirici_asm arch-gate + triple; varsayilan x86_64)\n"`
   → `arm64|x86_64|x86_64-none` (ve "varsayilan x86_64" → "varsayilan konak").
   (Her dizgi için ayrı blok; her biri TEK kez geçer.)
3. `selfhost/codegen.kem` — çapa (3 satır):
   `                eğer metin_esit(mv, "x86_64") {` /
   `                    mim = "x86_64"; trp = "x86_64-pc-windows-gnu";` /
   `                }`
   YENI: aynı 3 satır + ardına
   `                değilse { eğer metin_esit(mv, "x86_64-none") {` …
   DİKKAT: mevcut yapı `eğer x86_64 { } değilse { hata; ver 2; }`. Yeni dal
   hata dalından ÖNCE gelmeli. En güvenli biçim: çapayı şu 8 satırlık bloğun
   TAMAMI yap ve yeniden yaz:
```
                eğer metin_esit(mv, "x86_64") {
                    mim = "x86_64"; trp = "x86_64-pc-windows-gnu";
                }
                değilse {
                    yaz_str("--mimari: bilinmeyen mimari (arm64|x86_64)"); yb(10);
                    ver 2;
                }
```
   (bu 7 satır; YENI'de x86_64-none dalı `değilse` içinde, hata en içte;
   mesajı `(arm64|x86_64|x86_64-none)` yap). Girinti 16/20/24 boşluk.
   Ayrıca yorum tablosuna satır ekle — çapa:
   `    //   --mimari x86_64 → x86_64-pc-windows-gnu` → altına
   `    //   --mimari x86_64-none → x86_64-unknown-none-elf  (D1, bare-metal SysV)`.
   Ve `yaz_str("--mimari: mimari argumani gerekli (arm64|x86_64)"); yb(10);` dizgisi →
   `(arm64|x86_64|x86_64-none)`.
4. `Makefile` — çapa: `	./$(BUILD)/kemgu$(EXE) --llvm --mimari x86_64 $(BUILD)/kem_os_x86_comb.kem > $(BUILD)/kem_os_x86_ham.ll`
   → `--mimari x86_64-none`; HEMEN ARDINA yeni satır (Kanıt 0):
   `	@grep -q 'target triple = "x86_64-unknown-none-elf"' $(BUILD)/kem_os_x86_ham.ll || { echo "FAIL: x86 IR uclusu bare-metal degil"; exit 1; }`
   ve `	@grep -m1 'target triple' $(BUILD)/kem_os_x86_ham.ll`.
5. `test/check_kapisi.sh` — 144. ve 149. satırlardaki `--check --mimari x86_64 build/kem_os_x86_kapi.kem`
   (iki geçiş) → `--mimari x86_64-none`; mesajdaki `--mimari x86_64)` → `--mimari x86_64-none)`.
   Her satırı ayrı blokta, satırın TAMAMINI çapa al (bağlamda var).
6. `test/baremetal_diff_harness.sh` — 247/248. satırlardaki `--mimari x86_64` → `--mimari x86_64-none`
   (satırın tamamı çapa); bölüm (5) yorumundaki "üçlü bugün x86_64-pc-windows-gnu (D1 açık)"
   ifadesini "D1 (D-681): x86_64-none → x86_64-unknown-none-elf" olarak güncelle (o yorum
   satırını bağlamdan birebir çapa al).

Blok yolları: `src/ana.c`, `selfhost/codegen.kem`, `Makefile`, `test/check_kapisi.sh`,
`test/baremetal_diff_harness.sh`. Başka dosya YOK.

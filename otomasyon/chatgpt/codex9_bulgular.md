# Dalga 9 ölçümleri

- Güncel main D-751 ile heap 28 MB; değiştirilmedi. Görevdeki 16 MB test bütçesi korunacak.

## A — font biçimi ölçümü

Özgün kaynak `/usr/share/consolefonts/Uni2-Fixed16.psf.gz`, console-setup copyright kamu malı hükmü; PSF1 512 glif, 8×16, 10.804 bayt. SHA256 `d9025175dcf18f8b7442009a1870837f6ae974fa9561e9d8dae5eb145566fee7`.

| Biçim | Kaynak baytı | C LLVM üretimi | clang -O2 | Gerçek QEMU EKRAN_HAZIR (5 açılış) |
|---|---:|---:|---:|---|
| Sıfır olmayan her bayt için yaz_u8 | 186714 | 0,26 s | 39,29 s | 0,35–0,40 s |
| Tek hex metin | 22470 | 0,02 s | 0,62 s | 4,76–4,77 s |
| Uzunluğu dışarı alınmış tek hex | 22499 | 0,04 s | 0,54 s | 3,21–3,22 s |
| **64 baytlık hex parçaları** | **26406** | **0,04 s** | **0,62 s** | **0,15–0,20 s** |

Seçim: parçalı hex. `metin_bayt` bare-metal runtime'ında her çağrıda metnin uzunluğunu tarıyor; tek uzun metin karesel iş yapıyordu. Runtime değiştirilmedi. Python kâhini parçaları KEMGU çalıştırmadan okur, sabit SHA ve yerel kaynak varsa gzip açılımıyla birebir eşitlik arar. Test tüm baytların ağırlıklı toplamını, Türkçe/kutu/U+FFFD eşlemelerini ve kısa/geçersiz girdileri denetler.

Sabotaj: Python ile `4 + 311*16 + 2 = 4982` ofsetindeki `ğ` glif baytı 66 → 194. KEMGU fonksiyonunun dönüşünden önce bozma eklendi; kâhinin özgün baytları aynı kaldı. Derleme başarılı; gerçek QEMU **1/1 açılış kırmızı**, iki piksel farklı; C testi exit=2. Geri alındı, dogrula yeşil.

Loglar: `build/codex9/a-{yaz,hex,hex-sabit,parca}.log`, `a-sabotaj-qemu.log`, `a-sabotaj-test.log` (yerel).

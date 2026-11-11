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

## B — canlı girdi, kaydırma, süre ve heap

- VARSAYIM: girdi düzenlemesi tek satırlık yatay pencere, imleç ters renk. Enter sonrası tüm komut mevcut konsola yazılır, normal sarılır. Seri akışa canlı yankı eklenmez.
- İlk kâhin eski davranışta rc=2: daha boş istemde 137 piksel fark. Gerçek QEMU'da canlı metin, sağ/sol imleç, fazladan `x` ve geri silme Enter ÖNCESİ ölçülür; seri bu arada değişmemeli.
- Bağımsız FAT32 yazıcısının isteğe bağlı `/kaydir.txt` dosyası: Python ile 70 satır, 910 UTF-8 bayt. Bağımsız okuyucu birebir; `fsck.fat -n` rc=0 (9 kullanılan küme). Önceki imaj biçimi varsayılan olarak korunur.
- Tam ekran çizimi: 78×29 hücre, her hücre `ğ`, 20 kez bütün satırlar kirli. Gerçek QEMU seri başlangıç/son işaretleri, 1 ms host yoklama: toplam 0,652158 s, ortalama **32,608 ms/çizim**. Kaydırma sonrası screendump+Python karşılaştırma ayrıca yaklaşık 43–80 ms; bu ikinci sayı çizim süresi diye sunulmuyor.
- Sabotaj: `hiç` dalında canlı çizimi kaldır → gerçek NVMe **1/1 kırmızı**, 268 piksel fark. Yukarı kaydırmada kaynak satırı kendisi yap → **1/1 kırmızı**, 7761 piksel fark. İkisi de derleniyor; kaynaklar geri alındı.
- Heap ölçümü tahmin değil: QMP `pmemsave` ile ELF'teki `kem_bump` okunup `__heap_start` çıkarılır. Bump geriye gitmez; koşumun tahsis üst sınırıdır. NVMe 3.214.112, AHCI 3.017.360, USB 3.279.728 bayt. Kâhin **16 MiB altında olmayı zorunlu tutar**; linker/heap değiştirilmedi.
- Yerel loglar `build/codex9/b-*.log`, `b-profil.txt`.

## C — panik ekranı

- Panik açıkken 640×480 kırmızı (`0xAA0000`), `PANIK` başlığı ve Türkçe mesaj; Python font çizimiyle 307.200 piksel birebir. Kapı her açık/ekransız/kapalı-eski-kayıt durumunu 5 kez açar. Seri satır tam karşılaştırılır; QMP `info registers` ile **HLT=1** aranır (yoğun döngü başarı sayılmaz).
- Eski `kdl_panik` yeni fikstürde rc=2 / HLT=0. Yeni gövde seri davranışını korur, mevcut `kdl_idle` ile durur; yeni asm yok. `kem_mmu_hw.kem` içinde yalnız bu gövde değişti.
- VARSAYIM: mevcut 200 MiB ekran aynasının hemen sonraki 32 KiB alanı panik kayıt/font kopyasına ayrıldı. Başlatma sırasında doğrulanmış 8×16 PSF'nin glifleri ve Unicode eşlemeleri kopyalanır; hazır bayrağı en son yazılır. Panik yolu heap ayırmaz; ekran modülünün sembollerine bağlanmaz, böylece ekransız birleşik birimler korunur. Çizime başlamadan bayrak sıfırlanır (yeniden girişte yalnız seri).
- Sabotaj: bayrak koşulu `doğru` yapılınca **1/1 gerçek QEMU açılışı kırmızı**. Ekran hiç açılmadan, bayrağı sıfır bırakılmış eski durum kaydı 1×1 RAM kanaryasına işaret eder; `0x12345678` kırmızı renkle ezildi. Sıfır başlıklı normal ekransız açılışın tek başına bu hatayı göstermeyeceği için kanaryalı negatif şekil kapıda kalıcıdır.
- Fikstür `kdl_panik` yolunu kasıtlı `PANIK: kasitli ekran deneyi ğ` ile tetikler; başka C runtime panik yollarını değiştirmez. Loglar `build/codex9/c-*.log`.

- C/self ek kanıt: panik çekirdeği self-host ile de derlenip 15 ayrı QEMU açılışında aynı pikselleri/seriyi/HLT durumunu verdi (`c-self.log`).

# KEMGU — Codex Çalışma Talimatı

> Bu dosya her Codex oturumunun başında yüklenir. Burada yazan kurallar mutlaktır.

## Proje

KEMGU, Türkçe sentaks tabanlı bir sistem programlama dili. C11 ile yazılmış bootstrap derleyicisi, 900+ geçen test, ARM64 + x86_64 CI, parser fuzzer (20k iter clean), snapshot test framework'ü, bare-metal UART driver'ları (PL011 ARM64 + 16550A x86_64) hâlihazırda mevcut. Bir sonraki büyük hat self-hosting derleyici; **bu çalışma hattı bu repo'da Claude Code tarafından yürütülür ve Codex bu hatta dokunmaz.**

Repo: https://github.com/msyrt-sys/kemgu.git

## KAPSAM — Kesin Sınırlar

### İzin verilen alanlar
- `drivers/` — hardware driver implementasyonları (.kem dosyaları)
- `kernel/drivers/` — kernel tarafı driver glue kodu
- `tests/drivers/` — driver testleri ve QEMU entegrasyon harness'ı
- `examples/drivers/` — driver kullanım örnekleri
- `docs/drivers/` — driver dokümantasyonu

### KESİNLİKLE YASAK alanlar
Aşağıdaki dizinlere dosya ekleme, var olanları değiştirme, silme yasaktır:
- `compiler/`, `parser/`, `typechecker/`, `codegen/` — derleyici çekirdeği
- `stdlib/core/` — temel kütüphane
- `proofs/` — DRF teorem çalışması
- `bootstrap/` — C bootstrap derleyicisi
- Repo kökündeki `Makefile`, `CMakeLists.txt`, `.github/workflows/*`

PR'da yukarıdaki dizinlerden herhangi birinde değişiklik varsa **PR otomatik reddedilir.** Senin oluşturduğun PR `git diff --name-only origin/main` çıktısı sadece izinli dizinleri içermek zorunda.

### Davranış kuralları
1. **Yeni dil özelliği önerme.** Yeni keyword, sentaks değişikliği, tip sistemi eklentisi yasak.
2. Tip sistemi mevcut özelliklerle yetmiyorsa: PR açma, görev üzerine `BLOCKED-NEEDS-LANG-FEATURE` etiketi ile yorum bırak ve dur.
3. Yeni stdlib modülü gerekiyorsa sadece `stdlib/drivers/` altına eklenir. `stdlib/core/` dokunulmaz.
4. Bootstrap C derleyicisini hiçbir koşulda değiştirme.
5. CI workflow dosyalarını değiştirme. Driver testleri mevcut `make test` zincirine eklenir.

## Kullanabileceğin Dil Özellikleri

- `tekkez<T>` — linear types. Her MMIO buffer ve DMA bölgesi bu tiple sarılır. Çift `imha` ve sızıntı compile-time yakalanır.
- `kullan` / `imha` — linear değer tüketimi ve serbest bırakma.
- `yetki<R>` — capability tokens. MMIO bölgesi, port range, interrupt line erişimi yetki olarak modellenir.
- `gerçekzamanlı` — WCET qualifier. **Tüm interrupt handler'larda mecburi.** Bu qualifier'sız ISR derleyici tarafından reddedilir.
- `sabitsüre<T>` — constant-time. Driver'larda nadir; sadece crypto-relevant key path varsa.
- `vektör<T,N>` — SIMD intrinsics. Bulk descriptor kopyalama için kullanılabilir.
- `sonuç<T,E>` — Result tipi. Driver init dönüş tipi standartı.
- `seçimlik<T>` — Option tipi.
- `eşleş` — pattern matching (exhaustiveness şu an derleyici tarafından zorlanmıyor; kapalı tip üzerinde tamlık garantisi yok).

Yeni sentaks "şu olsaydı daha temiz olurdu" diye düşünüyorsan: **görev sana yanlış scope ile verilmiş demektir.** Geri bildir, BLOCKED işaretle.

## Build & Test

### Sandbox setup (her yeni Codex oturumunda)
```bash
sudo apt-get update
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    qemu-system-x86 qemu-system-arm \
    clang lld make
make bootstrap
```

### Test ve doğrulama komutları
| Komut | Ne yapar |
|-------|----------|
| `make test` | Tüm test suite |
| `make test-drivers` | Sadece driver testleri |
| `make qemu-x86_64-drivers` | x86_64 QEMU entegrasyon |
| `make qemu-aarch64-drivers` | ARM64 QEMU entegrasyon |
| `make lint` | Linter |

> **Not:** Yukarıdaki make hedeflerinden bir kısmı henüz mevcut olmayabilir. Eksikse `Makefile`'ı değiştirmek yerine `tests/drivers/Makefile.drivers` ayrı bir alt-makefile olarak ekle ve `make -f tests/drivers/Makefile.drivers <hedef>` ile çağır. Kök Makefile'a dokunma.

### Acceptance gate
PR açmadan önce yerelde geçmek zorunda:
- `make test-drivers` yeşil
- Hedef mimari için QEMU entegrasyon yeşil
- `make lint` uyarısız
- `git diff --name-only origin/main` çıktısında **sadece** izinli dizin yolları
- Eklenen her `tekkez<T>` ve `yetki<R>` kullanımı, hemen üstünde tek satırlık yorum ile gerekçelendirilmiş

## Branch & PR

### Branch isimlendirme
`drivers/<kategori>-<spesifik-isim>` formatında:
- `drivers/virtio-blk-init`
- `drivers/virtio-net-rx-queue`
- `drivers/ahci-port-detect`
- `drivers/pci-enumeration`

`codex/*`, `feat/*`, `feature/*`, `claude/*` ön ekleri **yasak.**

### PR başlığı
`drivers: <kısa İngilizce açıklama>` formatında. Örnek:
`drivers: VirtIO block device detection and virtqueue setup`

### PR açıklaması (zorunlu bölümler, Türkçe)

```markdown
## Kapsam
- Yapılan: ...
- Yapılmayan (kapsam dışı): ...

## Test
- Komutlar: ...
- Mimari: x86_64 / aarch64 / her ikisi
- Yeni test sayısı: N

## Dil özellikleri
- `tekkez` kullanımı: nerede, neden
- `yetki` kullanımı: hangi capability, neden
- `gerçekzamanlı` kullanımı: hangi ISR

## Dosya değişiklikleri (whitelist kontrol)
git diff --name-only çıktısı buraya yapıştırılır.

## Sonraki adım
Bu PR neyi unblock eder, sonraki doğal görev nedir.
```

## Görev granülaritesi

Bir Codex görevi **1-2 saatlik** ölçekte tasarlanmıştır.

- "Tüm AHCI driver'ı" → değil, ÇOK BÜYÜK
- "AHCI HBA detection ve abar mapping" → evet, doğru ölçek
- "AHCI port enumeration" → evet
- "AHCI IDENTIFY command implementation" → evet

Görev sana büyük geldiyse, kendi başına alt görevlere böl, sadece ilkini bitir, kalanlarını `TODO(codex-next-pr)` yorumları olarak işaretle.

## Dil tercihi

- Kod yorumları: **Türkçe**
- Commit mesajı: **İngilizce** (conventional commits)
- PR açıklaması: **Türkçe**
- Identifier'lar: KEMGU Türkçe kuralı — `oku`, `yaz`, `bekle`, `başlat`, `durdur`
- KEMGU stdlib API isimleri: mevcut konvansiyona uy (kaynak: `stdlib/drivers/uart/pl011.kem` ve `stdlib/drivers/uart/ns16550a.kem` referans alınır)

## Hata durumunda

- **Bootstrap derleme hatası** → AGENTS.md'yi yanlış okumuşsundur; setup komutlarını yeniden çalıştır
- **Bir testin amaç fonksiyonu belirsiz** → testi yazma, görev üzerine "test acceptance criterion missing" yorumu bırak
- **Hedef mimari QEMU'da boot etmiyor** → host bootloader değişikliği gerekiyor olabilir; bu izinli alan dışı, dur

## Tek cümlelik özet

Sen sadece `drivers/`, `kernel/drivers/`, `tests/drivers/`, `examples/drivers/`, `docs/drivers/` altında çalışırsın; derleyici çekirdeğini ve dili genişletmezsin; her PR mevcut Türkçe dil özellikleriyle yazılmış, QEMU'da doğrulanmış, 1-2 saatlik bir iş paketidir.

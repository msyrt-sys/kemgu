# Dalga 10 — UEFI

Başlangıç: 2026-11-12T19:13:50.744781+00:00

- D-752 main: 98f8e13; yalnız kemgu-codex10/codex/uefi-dalga10.
- Taban derleme rc=0; ekran 5/5, kabuk 15/15 rc=0.
- VARSAYIM: PVH bilgi adres yuvası UEFI yolunda da kullanılır; magic ile ayrılır. PVH giriş talimatları korunur.

- [x] A · EFI ELF yükleyici, yeni 64-bit giriş, 24 alanlık ABI · C ve self OVMF ayrı 5/5 açılış ve dörder negatif ELF · eski EBS anahtarıyla ikinci deneme başarılı · ABI +8 sabotajı QEMU 1/1 kırmızı, magic sabotajı saf test kırmızı · seri kapılar rc=0 · 2026-11-12T19:28:50.544573+00:00

- [x] B · GOP açıcı + BGA ile ortak çizim · 11 saf olumlu/olumsuz şekil, C/self yeşil · QEMU C 5/5, self 1/1, 1.024.000 piksel birebir · adım+4: 1/1 kırmızı (2324 piksel); biçim koruması: exit=2 · tüm seri kapılar rc=0, baremetal_diff 23/23, 0 atlama · 2026-11-12T19:38:36.717948+00:00

- [x] C · UEFI harita/çerçeve ayırıcısı, E4o ayna/panik/DMA tahsisi, 64-bit PCI MMIO sayfa tabloları · C UEFI 15/15 (256M/512M/2G), self 3/3; QMP fiziksel aralık/UC doğrulaması · ayrılmış sayfa sabotajı 1/1 kırmızı, runtime niteliği exit=1; 800 bölgeli yığın sabotajı self exit=139 · C10-001/C10-002 kaydedildi, yığın dolanması · son seri kapılar rc=0, köprü 134 OK, baremetal_diff 23/23 ve 0 atlama · 2026-11-12T20:25:58.468452+00:00

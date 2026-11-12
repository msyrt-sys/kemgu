# Dalga 10 — UEFI

Başlangıç: 2026-11-12T19:13:50.744781+00:00

- D-752 main: 98f8e13; yalnız kemgu-codex10/codex/uefi-dalga10.
- Taban derleme rc=0; ekran 5/5, kabuk 15/15 rc=0.
- VARSAYIM: PVH bilgi adres yuvası UEFI yolunda da kullanılır; magic ile ayrılır. PVH giriş talimatları korunur.

- [x] A · EFI ELF yükleyici, yeni 64-bit giriş, 24 alanlık ABI · C ve self OVMF ayrı 5/5 açılış ve dörder negatif ELF · eski EBS anahtarıyla ikinci deneme başarılı · ABI +8 sabotajı QEMU 1/1 kırmızı, magic sabotajı saf test kırmızı · seri kapılar rc=0 · 2026-11-12T19:28:50.544573+00:00

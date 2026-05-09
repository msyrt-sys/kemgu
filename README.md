# KEMGU — Türkçe Sistem Programlama Dili

Türkçe syntax'lı bir sistem programlama dili ve bu dille yazılacak bir işletim sistemi projesi.

## Hedefler

- **Güvenlik:** Buffer overflow, null pointer, use-after-free dil tasarımı seviyesinde imkansız
- **Çökmezlik:** Exception yok (sonuç<T,H>), null yok (seçimlik<T>)
- **Hız:** GC yok, bölge tabanlı bellek modeli (zero-pause)
- **Taşınabilirlik:** x86_64 ve ARM64 desteği

## Derleme

```bash
make          # ana program (build/kemgu)
make test     # test paketi çalıştır
make clean    # temizle
```

Gereksinim: GCC veya Clang (C11 desteği), GNU Make.

## Kullanım

```bash
# Dosyadan tokenize et
./build/kemgu test/ornekler/hasta.kem

# stdin'den oku
echo 'değişken x = 42;' | ./build/kemgu
```

## Proje Yapısı

```
kemgu/
├── CLAUDE.md                       # Claude Code proje bağlamı
├── Makefile
├── belgeler/
│   ├── KEMGU_Grammar_EBNF.md       # EBNF grammar tanımı
│   └── KEMGU_Bellek_Modeli.md      # Bellek modeli formalizasyonu
├── src/
│   ├── lexer.h / lexer.c           # Tokenizer
│   ├── utf8.h / utf8.c             # Türkçe UTF-8 karakter tanıma
│   ├── anahtar_kelime.c            # 31 anahtar kelime tablosu
│   ├── hata.h / hata.c             # Hata raporlama
│   └── ana.c                       # Ana giriş noktası
├── test/
│   ├── test_lexer.c                # Birim testleri
│   └── ornekler/
│       └── hasta.kem               # Örnek KEMGU programı
```

## Mevcut Durum

- ✅ Lexer (tokenizer) tamamlandı
- 📋 Parser tasarımı hazır (EBNF grammar + AST yapısı)
- 📋 Bellek modeli formalize edildi (3 katman)
- ⏳ Parser implementasyonu sırada

## Lisans

Tüm hakları saklıdır.

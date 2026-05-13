# KEMGU Language Support (VS Code)

KEMGU dili için sözdizimi vurgulama eklentisi.

## Kurulum (geliştirme modu)

```bash
# Bu klasörü VS Code uzantı dizinine kopyala/linkle:
# Windows:  %USERPROFILE%\.vscode\extensions\kemgu-language\
# Linux:    ~/.vscode/extensions/kemgu-language/
# macOS:    ~/.vscode/extensions/kemgu-language/

# Veya sembolik link:
ln -s "$(pwd)/editor/vscode-kemgu" ~/.vscode/extensions/kemgu-language
```

VS Code'u yeniden başlatın. `.kem` dosyaları otomatik tanınır.

## Özellikler

- Sözdizimi vurgulama: anahtar kelimeler, tipler, literaller, yorumlar
- Türkçe karakter desteği (ı, ğ, ü, ş, ö, ç)
- Otomatik kapatma: `{}`, `[]`, `()`, `""`, `''`
- Yorum şortcut'ları (`//` ve `/* */`)
- Otomatik girinti

## Gelecek

- LSP server (göstergeleri, tamamlama, "definition'a git")
- Linter entegrasyonu (`--check` çıktısını VS Code'da göster)
- Debug adapter (LLVM IR debug bilgisiyle)

# Türkçe Q klavye düzeni

PS/2 denetleyicisi klavyeyi set 2 moduna alır ve i8042’de set 1 çevirisini açar. Tabloda basma baytları onaltılıdır. Bırakmada bit 7 birdir. E0 öneki ayrı bir bayt olarak gelir. USB HID kullanım kodları aynı Türkçe düzene çevrilir. [PS/2 çözümleyicisi](../drivers/ps2/klavye_tr.kem) · [HID karşılıkları](../drivers/usb/hid_tus_tablosu.kem)

| Set 1 | Normal | Shift | AltGr |
|---|---|---|---|
| `02` | 1 | ! |  |
| `03` | 2 | &#x27; |  |
| `04` | 3 | ^ |  |
| `05` | 4 | + |  |
| `06` | 5 | % |  |
| `07` | 6 | &amp; |  |
| `08` | 7 | / |  |
| `09` | 8 | ( |  |
| `0A` | 9 | ) |  |
| `0B` | 0 | = |  |
| `0C` | * | ? |  |
| `0D` | - | _ |  |
| `10` | q | Q |  |
| `11` | w | W |  |
| `12` | e | E |  |
| `13` | r | R |  |
| `14` | t | T |  |
| `15` | y | Y |  |
| `16` | u | U |  |
| `17` | ı | I |  |
| `18` | o | O |  |
| `19` | p | P |  |
| `1A` | ğ | Ğ |  |
| `1B` | ü | Ü |  |
| `1E` | a | A |  |
| `1F` | s | S |  |
| `20` | d | D |  |
| `21` | f | F |  |
| `22` | g | G |  |
| `23` | h | H |  |
| `24` | j | J |  |
| `25` | k | K |  |
| `26` | l | L |  |
| `27` | ş | Ş |  |
| `28` | i | İ |  |
| `29` | &quot; | é |  |
| `2B` | , | ; |  |
| `2C` | z | Z |  |
| `2D` | x | X |  |
| `2E` | c | C |  |
| `2F` | v | V |  |
| `30` | b | B |  |
| `31` | n | N |  |
| `32` | m | M |  |
| `33` | ö | Ö |  |
| `34` | ç | Ç |  |
| `35` | . | : |  |
| `39` | Boşluk | Boşluk |  |
| `56` | &lt; | &gt; | U+007C (boru) |

Sol ve sağ Shift (2A, 36) ayrı izlenir. Caps Lock (3A) harfleri değiştirir. Shift ile Caps birlikteyken küçük harf çıkar. Türkçe çiftler i/İ ve ı/I olarak eşlenir. Ctrl (1D), Alt (38) ve AltGr (E0 38) olay kaydında değiştirici bitlerini taşır. Uygulama ABI’sinde Shift, Ctrl, Alt, AltGr ve Caps sırasıyla 1, 2, 4, 8 ve 16 değerindedir.

| Set 1 | İşlev |
|---|---|
| 01, 0E, 0F, 1C | Escape, Geri Sil, Tab, Enter |
| E0 48, E0 50, E0 4B, E0 4D | Yukarı, aşağı, sol, sağ |
| E0 47, E0 4F, E0 49, E0 51 | Home, End, Page Up, Page Down |
| E0 53 | Delete |

Kabukta yukarı ve aşağı geçmişi gezer. Tab yol tamamlar. Düzenleyicide Ctrl+S kaydeder, Ctrl+Q çıkar. Basılı karakter ve gezinme tuşlarında ilk yazılım tekrarı 500 ms, sonraki aralık 50 ms’dir. PS/2 IntelliMouse ve USB farede tekerleğin bir çentiği düzenleyici ve gezginde üç satır ilerler.

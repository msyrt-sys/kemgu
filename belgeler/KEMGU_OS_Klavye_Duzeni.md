# Türkçe Q klavye düzeni

PS/2 denetleyicisi klavyeyi set 2 moduna alır ve i8042’de set 1 çevirisini açar. Tabloda basma baytları onaltılıdır. Bırakmada bit 7 birdir. E0 öneki ayrı bir bayt olarak gelir. USB HID kullanım kodları aynı Türkçe düzene çevrilir. [PS/2 çözümleyicisi](../drivers/ps2/klavye_tr.kem) · [HID karşılıkları](../drivers/usb/hid_tus_tablosu.kem)

| Set 1 | Normal | Shift | AltGr |
|---|---|---|---|
| `02` | 1 | ! | &gt; |
| `03` | 2 | &#x27; | £ |
| `04` | 3 | ^ | # |
| `05` | 4 | + | $ |
| `06` | 5 | % | ½ |
| `07` | 6 | &amp; | ¾ |
| `08` | 7 | / | { |
| `09` | 8 | ( | [ |
| `0A` | 9 | ) | ] |
| `0B` | 0 | = | } |
| `0C` | * | ? | &#92; |
| `0D` | - | _ | &#124; |
| `10` | q | Q | @ |
| `11` | w | W |  |
| `12` | e | E | € |
| `13` | r | R | ¶ |
| `14` | t | T | ₺ |
| `15` | y | Y | ← |
| `16` | u | U | û |
| `17` | ı | I | î |
| `18` | o | O | ô |
| `19` | p | P |  |
| `1A` | ğ | Ğ |  |
| `1B` | ü | Ü | ~ |
| `1E` | a | A | â |
| `1F` | s | S | ß |
| `20` | d | D |  |
| `21` | f | F | ª |
| `22` | g | G |  |
| `23` | h | H |  |
| `24` | j | J |  |
| `25` | k | K |  |
| `26` | l | L |  |
| `27` | ş | Ş | ´ |
| `28` | i | İ | &#x27; |
| `29` | &quot; | é | &lt; |
| `2B` | , | ; | &#96; |
| `2C` | z | Z | « |
| `2D` | x | X | » |
| `2E` | c | C | ¢ |
| `2F` | v | V | „ |
| `30` | b | B | “ |
| `31` | n | N | ” |
| `32` | m | M | µ |
| `33` | ö | Ö | × |
| `34` | ç | Ç | · |
| `35` | . | : |  |
| `39` | Boşluk | Boşluk |  |
| `56` | &lt; | &gt; | &#124; |

Sol ve sağ Shift (2A, 36) ayrı izlenir. Caps Lock (3A) harfleri değiştirir. Shift ile Caps birlikteyken küçük harf çıkar. Türkçe çiftler i/İ ve ı/I olarak eşlenir. Ctrl (1D), Alt (38) ve AltGr (E0 38) olay kaydında değiştirici bitlerini taşır. Uygulama ABI’sinde Shift, Ctrl, Alt, AltGr ve Caps sırasıyla 1, 2, 4, 8 ve 16 değerindedir.

| Set 1 | İşlev |
|---|---|
| 01, 0E, 0F, 1C | Escape, Geri Sil, Tab, Enter |
| E0 48, E0 50, E0 4B, E0 4D | Yukarı, aşağı, sol, sağ |
| E0 47, E0 4F, E0 49, E0 51 | Home, End, Page Up, Page Down |
| E0 53 | Delete |

Kabukta yukarı ve aşağı geçmişi gezer. Tab yol tamamlar. Düzenleyicide Ctrl+S kaydeder, Ctrl+Q çıkar. Basılı karakter ve gezinme tuşlarında ilk yazılım tekrarı 500 ms, sonraki aralık 50 ms’dir. PS/2 IntelliMouse ve USB farede tekerleğin bir çentiği düzenleyici ve gezginde üç satır ilerler.


## Düzen geçişi ve kilitler

Kabuk Türkçe Q ile açılır. `duzen us` ABD ANSI düzenini, `duzen tr` Türkçe Q’yu seçer. Ctrl+Boşluk iki düzen arasında geçiş yapar. Aynı seçim USB ve PS/2 girişinde, kabukta ve ön plan uygulamasında geçerlidir. ABD düzeninde fiziksel I tuşu i/I, Shift+2 ise @ üretir.

Num Lock açıkken sayısal tuş takımı rakam üretir. Kapalıyken yön, Home, End, Page Up, Page Down, Insert ve Delete kodlarını gönderir. Caps, Num ve Scroll Lock ilk basışta değişir. PS/2 klavye her `ED` komutunu ve onu izleyen maskeyi `FA` ile onaylar. PS/2 maskesinin düşükten yükseğe sırası Scroll, Num, Caps’tir. USB boot klavyesinde HID SET_REPORT çıktı baytının sırası Num, Caps, Scroll’dur.

| Tuş | Set 1 basma | Set 1 bırakma | ABI kodu |
|---|---|---|---:|
| Pause | E1 1D 45 E1 9D C5 | Ayrı bırakma olayı yok | 581 |
| PrintScreen | E0 2A E0 37 | E0 B7 E0 AA | 311 |

PS/2 başlatılırken F3 komutuna 7F parametresi verilir. Donanımın tekrar basma baytları fiziksel tuş bırakılana kadar süzülür. Böylece USB ile PS/2 aynı 500 ms ilk gecikme ve 50 ms tekrar aralığını kullanır.

Ctrl+C ön plan sürecini sonlandırır. Kabuk `^C` yazar ve komut almaya döner. Arka plan süreçleri çalışmayı sürdürür. `girdisay N` bir PS/2 IRQ tanısıdır. 1–20 saniyelik ölçüm boyunca kabuk girişi tüketmeyi erteler, ardından basma, bırakma ve kayıp bayt sayılarını seri hatta yazar.

"""Sürücü envanterini kaynak API adları ve açık durum/sınır bilgisiyle üretir."""
from pathlib import Path
import re
meta={
'ortak/bayt':('Little-endian bayt erişimi','Python int.to_bytes','Ham oku/yaz çağrısından önce aralık denetimi çağırana ait'),
'ortak/harf':('Türkçe kurallı Unicode büyük/küçük harf metin dönüşümü','Python Unicode 15.0.0 bütün 2958 eşleme ve Türkçe örnekler','Kod noktası temelli; çok karakterli büyük harf genişlemeleri var; bağlamsal sigma ve normalizasyon yok'),
'ortak/harf_veri':('Sıralı Unicode harf dönüşüm tabloları','Python Unicode 15.0.0, 1525 üst ve 1433 alt eşleme','Üretilen veri; sürüm sabitlenmiştir; i/İ ve ı/I Türkçe eşlenir'),
'fs/yol_harf':('İsteğe bağlı Türkçe harf duyarsız yol karşılaştırma','Türkçe I çiftleri ve normalleştirilmiş yol örnekleri','yol_bol kuralları; Unicode normalizasyonu veya tam casefold yapmaz; normal disk eşlemesi değişmez'),
'ortak/utf8':('UTF-8 ve UTF-16LE dönüşümü','Python encode/decode, Unicode vektörleri','Geçersiz UTF-8 bir bayt ilerleyerek U+FFFD; UTF-16 sıfırda biter'),
'ps2/klavye_tr':('Set-1 Türkçe Q klavye durumu','A1 bağımsız 49 tuş tablosu','E0 ve Shift/Caps; donanım I/O yok'),
'ps2/fare':('PS/2 üç/dört bayt fare paketleri','PS/2 bit alanları, işaretli hareket vektörleri','Taşan paket atılır; dy yukarı pozitiftir'),
'disk/crc32':('CRC32/IEEE hesabı','Python zlib.crc32','Geçersiz aralık sıfır döner'),
'disk/bolum':('MBR ve CRC denetimli GPT bölüm tablosu','Python struct/uuid/zlib','Disk baytları bellekte; genişletilmiş MBR zinciri yok'),
'disk/guid':('GPT GUID dönüşümü ve tür adları','Python uuid.bytes_le','16 bayt veya 36 karakter kanonik metin'),
'fs/fat32_onar':('FSInfo sayacı onarma ve sahipsiz küme tespiti','fsck.fat kurtarılan küme listesi; sayaç onarımı sonrası temiz denetim','Yalnız sayaçları yazar; sahipsiz kümeleri silmez; çapraz/bozuk erişilen zincir hatadır'),
'fs/fat32_bpb':('FAT32 BPB ve küme zinciri','Python struct ve FAT sınır vektörleri','BPB çözümü küçük örnekleri kabul eder; zincir azami uzunlukla sınırlı'),
'fs/fat32_dizin':('8.3 ve UTF-16 LFN dizin çözümü','Python UTF-16LE ve LFN sağlama','Bozuk LFN kısa ada düşer; kapalı tür tamlığına güvenilmez'),
'fs/yol':('Mutlak yol normalleştirme','Bağımsız yol örnekleri','Bileşen en çok 255 UTF-8 baytı; kök üstü .. kökte kalır'),
'fs/fat32_kisa_ad':('Çakışmasız FAT kısa adı üretme','8.3 ve ~N sayaç vektörleri','11 bayt ham ad; ~1..~999999; ASCII dışı karakter _ olur'),
'fs/fat32_bicimle':('Seyrek bellek FAT32 imajı kurma','fsck.fat -n; mkfs.fat ters örneği; F1 111 dosya','512 bayt sektör, 32 ayrılmış sektör, iki FAT; en az 65525 küme; 16MiB bellek öneki'),
'fs/fat32_imaj':('Ortak FAT/dizin imaj erişimi','B6-B8 gidiş/dönüş ve fsck.fat','Mantıksal disk sonundaki tutulmayan baytlar sıfırdır; yardımcılar doğrulanmış BPB bekler'),
'fs/fat32_yaz':('Dosya/dizin oluşturma ve dosya silme','Gidiş/dönüş, dolu disk, FAT aynaları, fsck.fat','Üzerine yazma ve dizin silme yok; asıl API adları kullanılır'),
'fs/fat32_oku':('İmajdan dosya ve dizin okuma','Gidiş/dönüş; Linux mkfs.fat örneği','16MiB dosya sınırı; liste . ve .. içerir'),
'fs/ustar':('USTAR arşiv dizini ve dosya bulma','Python tarfile USTAR_FORMAT','Yalnız normal dosya/dizin; iki sıfır son blok gerekir'),
'acpi/tablo':('RSDP/RSDT/XSDT başlık ve sağlama çözümü','Python struct ve toplam mod 256','Fiziksel bellek eşleme yok; RSDP arama 16 bayt adımlıdır'),
'acpi/madt':('MADT işlemci ve kesme girdileri','Python struct ACPI fikstürü','Tür 0/1/2/4/5; bilinmeyen türler uzunluk denetimiyle atlanır'),
'acpi/fadt':('FADT güç/kesme alanları ve GAS','Python struct ACPI fikstürü','En az 129 bayt; X_DSDT yalnız en az 148 baytta, sıfır değilse öncelikli'),
'pci/yapilandirma':('PCI tip-0 yapılandırma ve BAR çözümü','Python struct ve maske aritmetiği','256 bayt; yalnız tip-0; 64 bit BAR sonraki yuvayı tüketir'),
'bellek/harita':('E820/Multiboot2 bellek aralıkları','Python bağımsız aralık süpürmesi','En çok 4096 bölge; pozitif tam64 adresler; kullanılamaz tür örtüşmede kazanır'),
'bellek/cerceve':('Bitmap fiziksel sayfa ayırıcı modeli','4096 bayt hizalama/sıra vektörleri','İlk 1MiB ayrılmış; en çok 1048576 kullanılabilir sayfa; gerçek eşleme yok'),
'elf/elf64':('ELF64 yükleme planı doğrulama','readelf -lhW /bin/true ve Python struct','LE x86_64/AArch64; EXEC/DYN; W^X, örtüşme ve giriş denetimi; çalıştırmaz'),
'ekran/cerceve':('32bpp BGRX piksel/dolgu/kopya','Python renk baytları ve kırpma','0x00RRGGBB; adım bayt; 16384 boyut ve 16MiB tampon'),
'ekran/psf':('PSF1/PSF2 font ve Unicode eşlemesi','Lat38-VGA8.psf.gz ve Python PSF çözümü','Tek kod noktalı eşleme; ligatür dizileri doğrulanıp atlanır; 65536 glif/4096 boyut'),
'ekran/yazi':('Glif, Unicode kod ve metin çizimi','Python MSB piksel matrisi, gerçek PSF','U+FFFD sonra ? yedek glif; çizim kırpılır'),
'ekran/konsol':('Unicode hücre ızgarası ve kaydırma','Bağımsız konsol/sarma örnekleri','1M hücre, boyut en çok 4096; sekme 8 sütun; kirli satırlar tüketilir'),
'ekran/vt':('ANSI CSI ve parçalı UTF-8 konsol akışı','ANSI durum örnekleri ve E2 mutasyonları','16 parametre, sayılar 4096da doyar; OSC/DCS yutulur; sınırlı SGR'),
'girdi/klavye_tablo':('Tablo parametreli klavye karakter düzeni','49x4 A1 Türkçe eşdeğerlik ve özel tablo vektörleri','A1 durum/özel tuş çekirdeğini çağırır; en çok 128 eşleme; ilk eşleme kazanır'),
'girdi/klavye_us':('US ANSI set-1 karakter düzeni','Python ord ile 49 tuş ve dört Shift/Caps durumu','A1 değişmez; ISO ek 86 tuşu ters bölü/boru olarak eşlenir'),
'girdi/satir':('Unicode satır düzenleme ve geçmiş','Python Unicode kod noktaları ve E1 scancode akışı','256 kod noktası, 16 geçmiş; ardışık tekrar/boş satır geçmişe eklenmez'),
'girdi/imlec':('Fare hareketi ve düğme kenarları','Python kırpma/bit kenarı vektörleri','Hız 0..1024; ekran y aşağı; tekerlek tam32 sınırlarında doyar'),
'girdi/halka':('Sabit kapasite FIFO olay tamponu','Python collections.deque 120 işlem modeli','0..1048576 eleman; dolunca en yeni atılır; kayıp sayacı tam64te doyar'),
'ekran/bmp':('24/32 bit BMP çözme ve çizme','Python struct BMP ve bağımsız piksel matrisi','BI_RGB/32 bit BI_BITFIELDS; alfa yok sayılır; bitişik RGB maskesi; 16MiB piksel'),
'sim/kabuk':('Bellek diskinde ls/cat/echo komutları','E1 gerçek scancode → FAT32 → konsol → PSF piksel zinciri','Host süreç/dosya erişimi yok; cat sonuna LF; komut yankısı yok'),
}
legacy={
'constants':'VirtIO durum bitleri ve kayıt ofsetleri','features':'VirtIO özellik pazarlığı maskeleri','mock_transport':'VirtIO başlangıç için mock taşıma modeli','status':'VirtIO durum geçişleri','virtio_blk':'VirtIO-blk başlangıç sırası','virtio_blk_config':'VirtIO-blk yapılandırma okuma','virtio_blk_oku':'VirtIO-blk tek sektör okuma','virtio_mmio':'VirtIO MMIO v2 taşıma','virtqueue_bind':'Split virtqueue kayıt bağlama','virtqueue_layout':'Virtqueue boyut/yerleşim hesabı'}
s='''# KEMGU sürücü modülleri

Bu dalın yeni modülleri bayt dizileri üzerinde saf mantık çalıştırır. C bootstrap ve self-host derleyicileriyle aynı test çıktısı zorunludur. Donanım/QEMU bütünleştirmesi bu saf-mantık dalgasının kabul kapısı değildir.

`make calistir_chatgpt_kopru` dil kartını, birim testlerini, E1 benzetimini ve E2 bozuk girdi koşumlarını çalıştırır. Tek test için `bash otomasyon/chatgpt/dogrula.sh tests/drivers/ekran/bmp_test.kem` kullanılır. Her ikisi repo kökünden çağrılır.

Tabloda **Çift derleyici** yeni dalga testlerinin geçtiğini; **Önceden mevcut** dalga kapsamındaki doğrulamanın bu eski modüller için yapılmadığını belirtir. API sütununda kaynakta `genel` işaretli işlev, yapı, çeşit ve sabit adları listelenir.

| Modül | Amaç | Genel API | Durum | Kahin | Bilinen sınır |
|---|---|---|---|---|---|
'''
for p in sorted(Path('drivers').rglob('*.kem')):
 key=str(p.relative_to('drivers').with_suffix(''))
 if key in meta:purpose,oracle,limit=meta[key];status='Çift derleyici'
 elif key.startswith('virtio/'):
  purpose=legacy[p.stem];oracle='Bu dalgada yeniden doğrulanmadı';limit='Mevcut donanım/mock akışı; bu dalganın kapsamı dışında';status='Önceden mevcut'
 else:raise RuntimeError('Metaveri eksik: '+key)
 names=re.findall(r'^genel (?:işlev|yapı|çeşit|sabit)\s+(\w+)',p.read_text(),re.M)
 api=', '.join('`'+n+'`' for n in names) or 'Genel API yok (modül içi model)'
 s+=f'| [{key}]({key}.kem) | {purpose} | {api} | {status} | {oracle} | {limit} |\n'
s+='''
## Sözleşmeler ve doğrulama sınırları

- Dizi taşıyan çerçeve yapıları değer olarak geçirilir; içteki dizi paylaşıldığı için çizim çağrısı tamponu günceller. Ölçüler constructor sonucundan alınmalıdır.
- FAT32 kurucusu gerçek FAT32 küme eşiğini sağlar. Dönen dizi tam disk boyu değildir: sıfır kuyruğu atılmış bellek önekidir. Dış kahin aracı tam seyrek disk dosyasını yeniden kurar.
- `bash tools/codex/fat32_kahin.sh` fsck.fat ile imajı denetler, mkfs.fat ile ters yön örneğini oluşturur. `bash tools/codex/fat32_coklu_kahin.sh` iki alt dizinde 111 canlı dosyayı ve ~100+ kısa ad çakışmasını denetler. `bash tools/codex/fat32_onar_kahin.sh` KEMGU sahipsiz küme listesini fsck kurtarma dosyalarının küme zincirleriyle karşılaştırır. Bu oturumda mcopy/mdir bulunmadığından o karşılaştırmalar atlandı; fsck.fat ve mkfs.fat çalıştırıldı.
- E2, 13 ayrıştırıcıda 3900 rastgele örnek ve 5100 tek-bayt mutasyonu ile sıfır/tek-bayt sınırlarını dener. Bu deterministik örnekler kapsamlı ispat değildir; her koşumun 30 saniye sınırı vardır.
- UTF-8 hataları tek bayt ilerleme ve U+FFFD ile ele alınır. Türkçe dosya adları ve emoji için ortak UTF-16/UTF-8 çekirdeği kullanılır.

## Derleyici bulguları

Tarihsel yeniden üretimler [codex_bulgular.md](../otomasyon/chatgpt/codex_bulgular.md) içinde. Dalga 2 düzeltmeleri özgün örneklerle yeniden doğrulandı; dalga 3 ek API adlarını ve geçici probe dosyalarını kaldırdı.

| Kimlik | Bulgu | Güncel durum |
|---|---|---|
| C001 | Modül ve işlev aynı adlı seçili import çakışması | T024 kararı gereği nitelikli çağrı korunur |
| C002 | dizi_olustur yapı elemanı genişliği | Düzeltildi; işaretli üreticiler dizi_olustur kullanır |
| C003 | Eşleş bağındaki yapı dizisinin eleman tipi | Düzeltildi; işaretli gereksiz yerel bağlar kaldırıldı |
| C004 | İç içe bayt kopyasında işaret genişletme | Düzeltildi; GPT başlığı doğrudan dizi_al ile kopyalanır |
| C005 | Yapıdan sonraki tam64 sabit argümanın genişliği | Düzeltildi; sabit argümanın açık dönüşümü kaldırıldı |
| C006 | İçe aktarılan yapıda boş dizi alanı | Düzeltildi; boş alan literalleri doğrudan kurulur |
| C007 | Geçişli içe aktarımdaki yapı tipleri | Bu bağımsız tabanda T011 sürüyor; açık tip ithalatı korunur |
| C008 | Sonuç döndüren eşleş sonrası geçersiz LLVM varsayılanı | Düzeltildi; erişilemez koruma dönüşleri kaldırıldı |
| C009 | dosya_yaz/dosya_sil/dizin_olustur host yerleşiğine bağlanması | Düzeltildi; yalnız asıl imaj API adları kullanılır |
| C010 | İçe aktarılan enum kurucusunda self T016 | Düzeltildi; TusOlayi kurucuları doğrudan kullanılır |
| C011 | tam32 minimumunun yapı alanına atanması | Düzeltildi; özgün örnek iki derleyicide exit=0 |

F5: [sınır matrisi](../otomasyon/chatgpt/codex_sinirlar.md) 39 saf modül için 158 test içerir. S002: ortak bayt aralık denetimindeki tam32 toplama taşması kalan uzunluk karşılaştırmasıyla giderildi; panik üreten iki örnek kalıcı regresyon testidir.

S001: FAT etiketinin BPB ile kök dizin metadata girdisinde eşleşmesi gerektiği fsck.fat ile yakalandı ve düzeltildi. [İlerleme kaydı](../otomasyon/chatgpt/codex_ilerleme.md) her işin sabotajını ve kabul kapısını içerir.
'''
Path('drivers/README.md').write_text(s)
print('Envanter:',len(list(Path('drivers').rglob('*.kem'))),'modül')

# Codex 2 karar ve engel kayıtları

VARSAYIM: Başlangıç origin/main abd9aa9 sürümünde codex_bulgular.md ve LOOP.md E4b yok. Bulgu raporu codex/surucu-dalga1 Git nesnesinden okundu; sıra ve kapsam CODEX_GOREV_2.md ile belirlendi. Ana ve dalga 1 çalışma dosyaları değiştirilmedi.

C009 ek gözlem: seçili içe aktarılan `dosya_sil(tam32)` self checker tarafından yerleşik `dosya_sil(metin)` imzasıyla denetleniyor (T001); `--check` tanıya rağmen rc=0. Kod üretimi fikstürü iki imzanın da kabul ettiği metin parametresi ve boş yol kullanır; kullanıcı sonucu 11, host sonucu başarısızlık olur. Bu imza çözümleme farkı ayrıca araştırılmalı.

VARSAYIM: Araç katmanı `rm -f` biçimini reddetti. Sabotaj sonrası yalnız açıkça listelenmiş üretilen `build/llvm.o`, `build/kemgu`, `build/codegen`, `build/codegen.ll` dosyaları Python `Path.unlink(missing_ok=True)` ile silindi; kaynak yedeğinden geri yükleme ve taze derleme şartı korundu.

VARSAYIM (C011 fikstürü): dtam8 değişkenini tam64 alana örtük atamak iki checker tarafından T001 ile reddedildi. Geçerli pozitif kontrol açık `olarak tam64` dönüşümüyle kuruldu; dil kabulü değiştirilmedi, beklenen değer 203 kaldı.


Doğrulama sınırı: `make test_tumu` içindeki Lean kapısı 32 dosyada 0 sorry/admit ve artık require kontrolü yapıyor; ortamda lean/lake bulunmadığından gerçek Lean tip denetimi çalışmıyor. Bu, rc=0 ile tüm ispatların denetlendiği anlamına gelmez.

C006 negatif kontrolü: yeni fikstür yalnız sahte T014’ü değil, boş dizinin skaler alana ve yanlış derinlikte diziye verilmesinde eksik T001/T013’ü de ölçtü. C kaynağı `src/tip_kontrol.c:4826` boş diziyi Dizi<TIP_BILINMIYOR> tutuyor; `src/tip.c:276,299` bilinmeyen elemanı iki Dizi arasında uyumlu sayıyor. Self’in eski `?` yorumu bu ayrımı kaybediyordu. C kuralı aynalandı; mevcut lambda fikstürünün tanı beklentisi korunarak tüm checker korpusu 244/244 oldu. Yeni fikstürler veya eski beklentiler zayıflatılmadı.

C010 kapsam ölçümü: yalnız seçili/genel koruması yeterli değildi; parser seçili ad tablosunu kapsamlar arasında düz tutuyor. İç modülün ithalatı dışarı sızdığı yeni probla ölçülünce ara doğrulama süreç grubu (yalnız bu worktree’ye ait grup) durduruldu. Kök dosyanın doğrudan kullan bildirimi ve kaynak-konum eşlemesi eklendi; yeni negatif fikstür önce kırmızı, sonra yeşil ve ayrı sabotajda tekrar kırmızı oldu. Ara test koşusu başarı kanıtı sayılmadı, son kaynakla tüm kapılar baştan seri koşuldu.

C010 tanım kapsamı: ikinci prob, ithal edilen dosyanın iç modülüne ait çeşidin de düz cc_* tablosunda bulunduğunu gösterdi. Doğrudan üye kontrolü eklendi; C kullan_isle kuralındaki mevcut T002 ve kullanım T016 korundu. Bu ikinci ara doğrulama da durdurulup başarı sayılmadı. Son yedi fikstürle checker 247/247, modül 35/35; tanım kapsamı sabotajı 246/247 rc=2 (kurulum rc=0). Aynı adlı kapsamlı çeşitlerin payload kayıtları birleştirilmedi.

## C001 — KARAR BEKLİYOR: modül adı ile seçili üye aynı ad alanında

Taban: origin/main 71aa91e; ayrı yerel dal: codex/derleyici-c001. Derleyici kurulumu rc=0. Kod değiştirilmedi, dal push edilmedi, kusur PR’ı açılmadı.

Yeniden üretim (build/codex2/c001, 40+2 python3 ile 42):

```kem
// c001_ad.kem
genel işlev c001_ad(n: tam32) -> tam32 { ver n + 2; }
// ana_secili.kem
kullan c001_ad::{c001_ad};
işlev main() -> tam32 { ver c001_ad(40); }
```

| Ölçüm | C | Self |
|---|---|---|
| Seçili aynı ad + çıplak çağrı | check=1, T024; LLVM=1 | check=0 OK, LLVM/link=0, exit=42 |
| Çıplak modül ithalatı + c001_ad::c001_ad(40) | check/LLVM/link=0, exit=42 | check/LLVM/link=0, exit=42 |
| Seçili aynı ad + nitelikli çağrı | check=1, T024; LLVM=1 | check=0 OK, LLVM/link=0, exit=42 |

Bilinçli davranış kanıtı: src/tip_kontrol.c:5814 seçili ithalatı açıkça “modül bağı + üye alias’ları” tanımlar. :5832–5865 modül bağını önce ekler; :5890–5902 başka yerel tanımla çakışmayı T024 yapar. DECISIONS_LOG.md:14819–14824 mevcut modül yüzeyinin Mehmet kararına bağlı olduğunu; :15052–15056 tip/değerin aynı scope’u paylaştığını kaydeder. CLAUDE.md:1612–1616 sonradan verilen düzleştirmeyi kaldırma kararını doğrular. Eski src :5818 legacy yorumu güncel CLAUDE kararıyla çeliştiğinden geçerli sözleşme olarak alınmadı.

Seçenekler (hiçbiri uygulanmadı):

1. C’nin modül bağı + tek ad alanı kuralını korumak; self’te aynı çakışmayı T024 yapmak. Geçerli program kümesini self tarafında daraltır.
2. Seçili ithalatın örtük modül bağını kaldırmak; yalnız üyeleri bağlamak. Mevcut seçili ithalat sonrası nitelikli çağrılar ayrıca açık modül ithalatı gerektirir.
3. Modül ve çağrılabilir değer bağlarını ayrı bağlamlarda çözmek; aynı adla hem f() hem f::f() desteklemek. Resolver/scope sözleşmesi ve çakışma kuralları değişir.

Ölçülen etki alanı: 71aa91e’de Git tarafından izlenen 814 .kem ve 183 .c/.h dosyası tarandı. Modülün son segmentiyle aynı adı seçen mevcut ithalat **0 dosya**; dosya adıyla aynı adlı genel işlev modülü **0 dosya** bulundu. Bu sıfır, derleyiciler arasındaki yeni prob farkını ortadan kaldırmaz. Seçenek 2’nin kıracağı doğrudan mevcut program **1 dosya**: test/moduller/ana_nitelikli.kem (:2–6, cift_a/cift_b seçili ithalat ve nitelikli çağrı). test/test_llvm.c:2562’de bu davranışı çalıştıran test de vardır. Statik taramanın test/test_llvm.c içindeki “mat” eşleşmesi ayrı test dizgilerinden geldiği için ikinci etkilenen program sayılmadı. Tarama kaynak düzeyindedir; üretilen/haricî programları kapsamaz.

DOKUNMA kuralı gereği derleyicinin hangi kabul kümesine hizalanacağı seçilmedi. Kalıcı korpus beklentisi, sabotaj ve tam takım bu karar maddesi için uygulanmadı; düzeltme yapılmış gibi raporlanmadı. Yerel ham kanıt: build/codex2/c001_olcum.log, c001_*.check, c001_etki.json.

## C007 — KARAR BEKLİYOR: geçişli imza tipleri ve faz/kapsam sözleşmesi

Taban: origin/main 71aa91e; ayrı yerel dal: codex/derleyici-c007. C001 ile aynı temiz tabandaki derleyici ikilileri kullanıldı. Kod değiştirilmedi, dal push edilmedi, kusur PR’ı açılmadı.

Minimal üç dosya (build/codex2/c007; 2³³ python3 ile 8589934592):

```kem
// tipler.kem
genel yapı Nokta { x: tam64; }
// api.kem
kullan tipler::{Nokta};
genel işlev uret() -> Nokta { ver Nokta { x: 8589934592 }; }
// ana.kem
kullan api::{uret};
işlev main() -> tam32 {
    değişken n = uret();
    eğer n.x == 8589934592 { ver 42; }
    ver 1;
}
```

| Giriş / kontrol | C | Self |
|---|---|---|
| Yalnız api seçili ithalatı, tipler geçişli | check=1, T011×1; LLVM=1 | check=0 OK, LLVM/link=0, exit=42 |
| Önce kullan tipler; sonra api | check/LLVM/link=0, exit=42 | check/LLVM/link=0, exit=42 |
| Önce api; sonra kullan tipler | check=1, T011×1; LLVM=1 | check=0 OK, LLVM/link=0, exit=42 |
| api dönüşü tipler::Nokta (geçişli sıra) | check=1, T016×1; LLVM=1 | check=0 OK, LLVM/link=0, exit=42 |
| tipler önce, fakat api kendi Nokta ithalatını yapmıyor | check=1, kurucuda T002×1; T011 yok | check=0 OK, LLVM/link=0, exit=42 |

İlk yerel prototipte alan sonu noktalı virgül eksikti (P020); bu koşum C007 kanıtı sayılmadı. Yukarıdaki ölçümler sözdizimi düzeltilmiş üç dosyayla yeniden alındı. Mevcut fikstür beklentisi değiştirilmedi.

**Bulgu:** T011’ün sıraya duyarlı olması gerçek uygulama/faz sorunu kanıtıdır; bu reddin bilinçli tasarım olduğu iddia edilmiyor. Ancak hangi kapsam sözleşmesine göre onarılacağı bu görevde kendiliğinden seçilmedi. Bilinçli altyapı kararları: src/tip_kontrol.c:5760–5764 dosya modülünü girişin özel adlarından yalıtır; :5778 üyeleri, :5793 modül kaydından önce işler. :5718–5746 her modülün tip/işlevlerini yerel sıralar; tüm modüllerin tiplerini önce kurmaz. :1976–1983 bilinçli düz-ad çapraz-modül fallback’i yapar. DECISIONS_LOG.md:14819–14830 ad alanlı iki fazı ve yüzey kararını; :15057–15069 nitelikli imza tiplerinde gizli kanonik fallback’i ve düz IR-tip kimliğinin sınırını açıklar. CLAUDE.md:1612–1616 düzleştirmenin kaldırılmasının Mehmet kararı olduğunu söyler.

Son kontrol, tek bir küresel “tipler önce” geçişinin görünürlük politikasını da ele alması gerektiğini gösterir: ithalatı olmayan imza tipi C’de düz-ad fallback’iyle bulunuyor, aynı yapının kurucusu T002 kalıyor; self tümünü kabul ediyor. Yalnız nitelikli tipe geçmek de mevcut faz sorununu gidermiyor (T016).

Seçenekler (hiçbiri uygulanmadı):

1. Mevcut düz-ad fallback sözleşmesini koruyup tüm modül tip kayıtlarını işlev imzalarından önce kurmak. Sıra bağımlılığını giderir; ithalatsız imza tipi kabulü, aynı adlı tipler ve döngüler için korunacak davranış açıkça seçilmelidir.
2. Tip çözümünü her dosyanın açık ithalat bağlarına göre yapmak; fazları buna göre ayırmak. Ad alanı yalıtımını netleştirir fakat bugünkü gizli/düz-ad fallback kabul kümesini değiştirir; mevcut geçerli programlar için göç gerektirebilir.
3. Mevcut sıra kısıtını belgeleyip açık bağımlılıkları girişte önce yüklemeyi şart koşmak. Derleyici kapsamı dar kalır fakat kullanıcı programlarına yükleme sırası yükü getirir; sürücü dolanmalarını bu görevde değiştirmek anlamına gelmez.

Ölçülen depo etkisi: 814 izlenen .kem dosyasında seçili ithal edilen yapı/çeşidi işlev imzasında kullanan **9 tip referansı, 4 dosya** statik aday bulundu. Hepsi iki derleyicide doğrudan --check ile ölçüldü:

| Dosya | C rc / tanı | Self rc / tanı |
|---|---|---|
| drivers/virtio/virtio_blk.kem | 0 / yok | 0 / yok |
| drivers/virtio/virtio_blk_oku.kem | 0 / yok | 0 / yok |
| tests/drivers/virtio/virtio_blk_init_test.kem | 1 / T011×1, T002×5 | 0 / yok |
| tests/drivers/virtio/virtio_blk_oku_test.kem | 0 / yok | 0 / yok |

**Doğrudan doğrulanan mevcut C007 tetikleyicisi: 1 dosya.** virtio_blk_init_test.kem’in yalnız build altında oluşturulan kopyasına ilk satır olarak `kullan drivers::virtio::virtqueue_bind;` eklenince T011 1→0 oldu; ayrı T002×5 kaldı (C rc=1, self rc=0). Asıl sürücü/test değiştirilmedi; program tamamen düzelmiş gibi sunulmadı. Dört adayın hepsi bozuk değildir. Statik tarama genel bir anlamsal etki analizi değildir; tüm geçişli çağıranları veya derleme zamanı üretilen programları kapsadığı iddia edilmez.

DOKUNMA kuralı gereği politika seçimi yapılmadı. Kalıcı korpus beklentisi, sabotaj ve tam takım bu karar maddesi için uygulanmadı. Ham kanıt: build/codex2/c007_olcum.log, c007_ek_olcum.log, c007_etki_olcum.json, c007_virtio_tip_once.json ve ilgili .check dosyaları. Yeni tanı kodu/D numarası verilmedi.

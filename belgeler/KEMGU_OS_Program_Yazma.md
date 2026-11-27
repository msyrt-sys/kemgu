# KEMGU OS için kullanıcı programı

Kullanıcı programı ayrı bir x86_64 ELF dosyasıdır. Çekirdek dosyayı FAT32 üzerinden açar, yüklenebilir segmentleri sürecin özel sayfalarına kopyalar ve kullanıcı kipinde başlatır. Kaynakta giriş işlevi `kullanici_main(argc: tam64,argv: tam64) -> tam64` olur. Döndüğü sayı kabukta süreç çıkış kodudur.

## Derleme ve çalıştırma

İlk örneği `build/okuornek.kem` olarak kaydedin. Repo kökünde aşağıdaki komutları çalıştırın.

```bash
make -j8 build/kemgu build/codegen build/kdl_runtime.o
python3 tests/qemu/surec_elf.py build/okuornek.elf --kaynak build/okuornek.kem --derleyici build/kemgu
```

Aynı komutta `--derleyici build/codegen` self-host derleyicisini seçer. Araç kullanıcı heap'ini, dizi işlemlerini, sistem çağrısı sarmalayıcılarını ve başlangıç kodunu kaynakla birleştirir. Clang x86_64 freestanding nesnesi üretir. LLD, `linker/user-x86_64.ld` ile bağlar. Araç makine türünü, giriş adresini ve segment izinlerini denetler. Yazılabilir veri bölümündeki `/bin` dizinine ELF'i yerleştirin. Kabukta `baslat okuornek /diskler/usb0p2/notlar/günlük.txt` komutu programı ön planda çalıştırır. Dağıtım görüntüsünde `test/kullanici/*.kem` kaynakları aynı araçla otomatik derlenir.

## Bir dosyayı stdout'a kopyalama

`argv[0]` yürütülen programın yoludur. Örnek, dosya yolunu ikinci argümandan alır. Kısa okumayı işler, sıfır baytı EOF sayar ve açtığı tanımlayıcıyı kapatır.

<!-- program: okuornek -->
```kem
işlev kullanici_main(argc: tam64,argv: tam64) -> tam64 { güvensiz {
    eğer argc != 2 { ver 2; }
    değişken fd: tam64 = sis_ac(sis_argv(argv,1),0);
    eğer fd < 0 { ver 3; }
    değişken p: tam64 = malloc(512) olarak tam64;
    eğer p == 0 { sis_kapat(fd); ver 4; }
    değişken n: tam64 = sis_oku(fd,p,512);
    iken n > 0 {
        eğer sis_yaz(1,p,n) != n { sis_kapat(fd); ver 5; }
        n=sis_oku(fd,p,512);
    }
    sis_kapat(fd); eğer n < 0 { ver 6; } ver 0;
} }
```

## Ekran ve olay

Bu örnek yeşil yazıyla bir tuş ister. Birleşik olayın ilk alanı tür, tuş olayının üçüncü veri alanı basılma durumudur. İşaretli zaman aşımı milisaniye cinsindendir. Ön plan süreci beklerken işlemci başka bir hazır sürece geçebilir.

<!-- program: olayornek -->
```kem
işlev kullanici_main(argc: tam64,argv: tam64) -> tam64 { güvensiz {
    değişken p: tam64 = malloc(56) olarak tam64;
    eğer p == 0 veya sis_ekran_temizle(1056832) < 0 { ver 2; }
    eğer sis_hucre(2,1,"Tuşa bas",65280,1056832) < 0 { ver 3; }
    iken doğru {
        eğer sis_olay_bekle(p,1,-1) < 0 { ver 4; }
        eğer sis_alan(p,0) == 1 ve sis_alan(p,3) != 0 { ver 0; }
    } ver 0;
} }
```

## Bağlama noktaları arasında dosya aktarma

Bu örnek bir FAT32 dosyasını geçici dosya sistemine kopyalar ve orada yeniden adlandırır. İki çağrı da mutlak veya çalışma dizinine göre çözülen iki yol alır. Hedef aygıttaki yazma başarısız olursa kaynak yerinde kalır. Kaynak silinemediğinde taşıma hata döndürür ve yazılmış hedef elde kalır.

<!-- program: aktarornek -->
```kem
işlev kullanici_main(argc: tam64,argv: tam64) -> tam64 {
    eğer argc != 2 { ver 2; }
    eğer sis_kopyala(sis_argv(argv,1),"/ram/ornek.tmp") < 0 { ver 3; }
    eğer sis_tasi("/ram/ornek.tmp","/ram/ornek.txt") < 0 { ver 4; }
    ver 0;
}
```

## Sınırlar

Kod ve verinin başlangıcı `0x54200000`, üst sınırı `0x54600000` olur. Kullanıcı heap'i 8 MiB'dir. ELF yükleyicisi örtüşen segmentleri, yazılabilir ve çalıştırılabilir bir sayfayı ve görüntü alanı dışındaki hedefi reddeder. En fazla sekiz süreç yuvası ve süreç başına 16 fd bulunur. Bir çağrıda en çok 16 KiB okunur veya 4 KiB yazılır. FAT32 yazma sınırı dosya başına 64 KiB'dir. Komut en çok 32 argüman alır. Hata numaraları, kayıtlardaki alanların sırası ve boru uçlarının sahipliği ABI tablosundadır. [Sistem çağrıları](KEMGU_OS_Syscall_ABI.md)

`make calistir_bm_x86_belge15` bu sayfadaki altı kaynak bloğunu çıkarır, iki derleyiciyle yeniden üretir ve OVMF altında çalıştırır.

## IPv4 adresine ICMP echo gönderme

`baslat yankiornek 127.0.0.1` bir ICMP soketi açar. Adresi bir DNS adıyla da verebilirsiniz. İstek kaydında sıra numarası ve milisaniye cinsinden zaman aşımı vardır. Başarılı çağrı gidiş dönüş süresini mikrosaniye cinsinden verir.

<!-- program: yankiornek -->
```kem
işlev kullanici_main(argc: tam64,argv: tam64) -> tam64 {
    eğer argc!=2 { ver 2; }
    değişken ip: tam64=kag_coz(sis_argv(argv,1));
    değişken istek: tam64=kag_ayir(16);
    eğer ip<=0 veya istek==0 { ver 1; }
    değişken fd: tam64=sis_soket(3);
    eğer fd<0 { ver 1; }
    kag_koy(istek,0,1);
    kag_koy(istek,1,1000);
    değişken sure: tam64=sis_icmp_yanki(fd,ip,istek);
    sis_kapat(fd);
    eğer sure<0 { kag_satir(tam_metin(sure)); ver 1; }
    kag_satir("Yanki alindi");
    ver 0;
}
```

## İki UDP portundan ilk hazır olanı okuma

`baslat bekleornek` 9118 ve 9119 portlarını dinler. Başka bir makineden konuk IPv4 adresindeki bu portlardan birine kısa bir metin datagramı gönderin. Her bekleme kaydı 24 bayttır. Kayıt fd, istenen olaylar ve gerçekleşen olaylar için üç i64 alan içerir. Okuma biti 1, yazma biti 2, hata biti 4 değerindedir. Örnek beş saniye içinde gelen ilk datagramdan en çok 128 baytı standart çıktıya yazar.

<!-- program: bekleornek -->
```kem
işlev ko_datagram(a: tam64,b: tam64,p: tam64) -> tam64 {
    kag_bekleme(p,a,1);
    kag_bekleme(p+24,b,1);
    kag_satir("UDP 9118 9119 hazir");
    değişken say: tam64=sis_bekle_coklu(p,2,5000);
    eğer say==0 { kag_satir("Zaman asimi"); ver 1; }
    eğer say<0 { ver 1; }
    değişken fd: tam64=a;
    eğer (sis_alan(p,2)&1)==0 {
        eğer (sis_alan(p+24,2)&1)==0 { ver 1; }
        fd=b;
    }
    değişken n: tam64=sis_al(fd,p+48,128);
    eğer n<0 veya kag_hepsini_yaz(1,p+48,n)!=n { ver 1; }
    ver 0;
}
işlev kullanici_main(argc: tam64,argv: tam64) -> tam64 {
    değişken p: tam64=kag_ayir(176);
    eğer p==0 { ver 1; }
    değişken a: tam64=kag_dinle(9118,2,0);
    eğer a<0 { ver 1; }
    değişken b: tam64=kag_dinle(9119,2,0);
    eğer b<0 { sis_kapat(a); ver 1; }
    değişken kod: tam64=ko_datagram(a,b,p);
    sis_kapat(a);
    sis_kapat(b);
    ver kod;
}
```

## Süreci bir işlemciye bağlama

`baslat yakornek` son çevrimiçi işlemciyi seçer. Yakınlık çağrısı başarılı olduğunda süreç sonraki kullanıcı dilimini o işlemcide alır. Örnek gerçek numarayı yazdırır ve süreç için bütün işlemcileri yeniden kullanılabilir yapar. Bir işlemcili açılışta çıktı `CPU 0`, dört işlemcili açılışta `CPU 3` olur.

<!-- program: yakornek -->
```kem
işlev kullanici_main(argc: tam64,argv: tam64) -> tam64 {
    değişken adet: tam64=sis_islemci_sayisi();
    eğer adet<1 veya sis_yakinlik_ayarla(adet-1)!=0 { ver 1; }
    değişken no: tam64=sis_islemci_no();
    eğer sis_yakinlik_ayarla(-1)!=0 veya no!=adet-1 { ver 2; }
    kag_satir(metin_birlestir("CPU ",tam_metin(no)));
    ver 0;
}
```

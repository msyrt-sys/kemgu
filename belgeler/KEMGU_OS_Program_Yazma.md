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

## Sınırlar

Kod ve verinin başlangıcı `0x54200000`, üst sınırı `0x54600000` olur. Kullanıcı heap'i 8 MiB'dir. ELF yükleyicisi örtüşen segmentleri, yazılabilir ve çalıştırılabilir bir sayfayı ve görüntü alanı dışındaki hedefi reddeder. En fazla sekiz süreç yuvası ve süreç başına 16 fd bulunur. Bir çağrıda en çok 16 KiB okunur veya 4 KiB yazılır. FAT32 yazma sınırı dosya başına 64 KiB'dir. Komut en çok 32 argüman alır. Hata numaraları, kayıtlardaki alanların sırası ve boru uçlarının sahipliği ABI tablosundadır. [Sistem çağrıları](KEMGU_OS_Syscall_ABI.md)

`make calistir_bm_x86_belge15` bu sayfadaki iki kaynak bloğunu çıkarır, iki derleyiciyle yeniden üretir ve OVMF altında çalıştırır.

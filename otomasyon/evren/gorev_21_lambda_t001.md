# Tur 21 — self-host lambda annotasyonu T001 denetimi (eski PR #106 / D-331 yeniden portu)

İKİ dosyaya AYNI eklemeyi yap: `selfhost/checker.kem` VE `selfhost/codegen.kem`. Her dosya için
İKİ ```duzenle bloğu (toplam 4 blok), başka dosyaya DOKUNMA:

1. ESKİ: tam olarak şu satır (her iki dosyada da tek kez geçer):
   `// T001: annotation vs değer tipi (DEGISKEN/SABIT). Güvenli — yalnız emin olunca.`
   YENİ: aşağıdaki "ÖZGÜN KOD"daki yardımcı işlevlerin TAMAMI (`lam_param_idx`, `lam_ilk_ver`,
   `lam_donus_str`, `tip_kesin_farkli`, `t001_lambda_kontrol`, başlık yorumuyla), ardından o
   ESKİ satırın kendisi. Başlık yorumundaki "D-326" ibaresini "D-698 (eski PR #106 / D-331)"
   yap; kod gövdesine dokunma.
2. ESKİ: tam olarak şu iki satır
   `işlev t001_kontrol(p: &değişken Ayr, idx: tam32) -> tam32 {`
   `    değişken annot: metin = annot_str(p, idx);`
   YENİ: aynı iki satır, ama aralarına
   `    eğer t001_lambda_kontrol(p, idx) == 1 { ver 0; }   // D-698`
   satırı girer (yani ilk satırdan hemen sonra).

Kullanılan mevcut işlevler iki dosyada da VAR (yeniden tanımlama YOK): `son_cocuk`, `param_say`,
`param_tip_str`, `ifade_tip`, `tip_str`, `dizi_al`, `dizi_ekle`, `metin_esit`. Eklenen 5 işlev
iki dosyada da HENÜZ YOK.

## Bugünkü C davranışı (ölçüldü — port bununla eşleşmeli)
`değişken X: işlev(...) -> T = <lambda>;` için C T001 verir (DEGISKEN konumu): dar dönüş
(`işlev() -> tam8 = || 42`), blok-form dar (`işlev() -> tam64 = || { ver 5; }`), arite farkı (iki
yön), parametre tipi farkı, dönüş tipi ailesi farkı (`-> metin = || 42`, `-> mantıksal = || { ver 5; }`).
C KABUL eder: uyumlu tipler, `-> boş = || { }`, ve `işlev() -> tam64 = || 8589934592` (2^33
doğal olarak tam64 — `ifade_tip` bunu tam64 vermiyorsa bu satır ayrışır; kodu DEĞİŞTİRME, olduğu
gibi port et; ölçümü ben yapacağım).

## ÖZGÜN KOD (eski PR'daki ekleme, birebir — `+` önekleri kaldırılmış)
```
// === D-326: işlev-tipli değişken annotasyonu vs LAMBDA (checker parite açığı) ===
// annot_str bileşik tipte "?" döner → `değişken f: işlev()->tam64 = || 42;` HİÇ
// denetlenmiyordu: self, C'nin T001 ile reddettiğini KABUL ediyordu (self DAHA
// GEVŞEK = "self-host C'nin oracle'ı" invaryantının kırılması).
// C aynası: lambda tipi bağımsız çıkarsanır (beklenen YAYILMAZ) → tip_esit(annot,
// lambda_tipi); arite ∨ bilinen-param ∨ bilinen-dönüş farkı → T001 (DEGISKEN konumu).
// "?" (çözülemeyen) taraf ATLANIR — false-positive yok.

// LAMBDA'nın k'ıncı PARAMETRE çocuğunun düğüm indeksi (yoksa -1).
işlev lam_param_idx(p: &değişken Ayr, lam: tam32, k: tam32) -> tam32 {
    değişken cb: tam32 = dizi_al(p.a_cb, lam);
    değişken cs: tam32 = dizi_al(p.a_cs, lam);
    değişken i: tam32 = 0;  değişken n: tam32 = 0;
    iken i < cs {
        değişken c: tam32 = dizi_al(p.cocuk, cb + i);
        eğer metin_esit(dizi_al(p.a_ad, c), "PARAMETRE") {
            eğer n == k { ver c; }
            n = n + 1;
        }
        i = i + 1;
    }
    ver 0 - 1;
}
// Blok-form gövdedeki İLK `ver` (C lambda_blok_donus sırası). İç LAMBDA'ya GİRMEZ.
işlev lam_ilk_ver(p: &değişken Ayr, n: tam32) -> tam32 {
    değişken ad: metin = dizi_al(p.a_ad, n);
    eğer metin_esit(ad, "LAMBDA") { ver 0 - 1; }
    eğer metin_esit(ad, "VER") { ver n; }
    değişken cb: tam32 = dizi_al(p.a_cb, n);
    değişken cs: tam32 = dizi_al(p.a_cs, n);
    değişken i: tam32 = 0;
    iken i < cs {
        değişken r: tam32 = lam_ilk_ver(p, dizi_al(p.cocuk, cb + i));
        eğer r >= 0 { ver r; }
        i = i + 1;
    }
    ver 0 - 1;
}
// LAMBDA'nın çıkarsanan dönüş tipi. İfade-form: gövdenin DOĞAL tipi (beklenen "?"
// — C tip_belirle). Blok-form (D-304): ilk `ver`in değer tipi; `ver` yoksa "boş".
işlev lam_donus_str(p: &değişken Ayr, lam: tam32) -> metin {
    değişken g: tam32 = son_cocuk(p, lam);
    eğer g < 0 { ver "?"; }
    eğer metin_esit(dizi_al(p.a_ad, g), "BLOK") == yanlış { ver ifade_tip(p, g, "?"); }
    değişken v: tam32 = lam_ilk_ver(p, g);
    eğer v < 0 { ver "boş"; }
    eğer dizi_al(p.a_cs, v) == 0 { ver "boş"; }
    ver ifade_tip(p, dizi_al(p.cocuk, dizi_al(p.a_cb, v)), "?");
}
// İki tip-string'i "ikisi de BİLİNİYOR ve FARKLI" mı? ("?" → emin değiliz, atla.)
işlev tip_kesin_farkli(a: metin, b: metin) -> mantıksal {
    eğer metin_esit(a, "?") veya metin_esit(b, "?") { ver yanlış; }
    ver metin_esit(a, b) == yanlış;
}
// TIP_ISLEV annotasyonu + LAMBDA değeri: uyuşmazlık → T001 (1 = hata verildi).
işlev t001_lambda_kontrol(p: &değişken Ayr, idx: tam32) -> tam32 {
    eğer dizi_al(p.a_cs, idx) < 2 { ver 0; }
    değişken ti: tam32 = dizi_al(p.cocuk, dizi_al(p.a_cb, idx));
    eğer metin_esit(dizi_al(p.a_ad, ti), "TIP_ISLEV") == yanlış { ver 0; }
    değişken lam: tam32 = son_cocuk(p, idx);
    eğer lam < 0 { ver 0; }
    eğer metin_esit(dizi_al(p.a_ad, lam), "LAMBDA") == yanlış { ver 0; }
    değişken tcs: tam32 = dizi_al(p.a_cs, ti);
    eğer tcs == 0 { ver 0; }              // dönüş tipi çocuğu yok → emin değiliz
    değişken tpn: tam32 = tcs - 1;        // annot parametre sayısı (son çocuk = dönüş)
    değişken kotu: mantıksal = yanlış;
    eğer tpn == param_say(p, lam) {
        değişken i: tam32 = 0;
        iken i < tpn {
            değişken lp: tam32 = lam_param_idx(p, lam, i);
            eğer lp >= 0 {
                eğer tip_kesin_farkli(tip_str(p, dizi_al(p.cocuk, dizi_al(p.a_cb, ti) + i)),
                                      param_tip_str(p, lp)) { kotu = doğru; }
            }
            i = i + 1;
        }
        eğer tip_kesin_farkli(tip_str(p, dizi_al(p.cocuk, dizi_al(p.a_cb, ti) + tpn)),
                              lam_donus_str(p, lam)) { kotu = doğru; }
    } değilse {
        kotu = doğru;                     // arite farkı — C tip_esit de reddeder
    }
    eğer kotu {
        dizi_ekle(p.th_kod, "T001");
        dizi_ekle(p.th_sat, dizi_al(p.a_sat, idx));
        dizi_ekle(p.th_sut, dizi_al(p.a_sut, idx));
        ver 1;
    }
    ver 0;
}
    eğer t001_lambda_kontrol(p, idx) == 1 { ver 0; }   // D-326
```
